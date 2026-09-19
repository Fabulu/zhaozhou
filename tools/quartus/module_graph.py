#!/usr/bin/env python3
"""Who instantiates whom, across every .sv in fpga/rtl.

Written 2026-09-03 for the owner's production-only resource count. The count
it exists to protect is "counted ONCE": if a chosen module already instantiates
another chosen module, putting both in the resource top counts the inner one
twice and inflates the very number the report is supposed to settle.

It is a text scan, not an elaboration, so it is deliberately GENEROUS about
what looks like an instantiation -- a false edge makes the top smaller and is
visible in the manifest, a missed edge silently double-counts.
"""
import collections
import io
import os
import re
import sys

SEP = os.sep


def load(rtl_dir):
    files = {}
    for root, _dirs, names in os.walk(rtl_dir):
        for n in names:
            if n.endswith(".sv"):
                p = os.path.join(root, n).replace(SEP, "/")
                with io.open(p, encoding="utf-8", errors="replace") as stream:
                    files[p] = stream.read()
    return files


def strip_comments(s):
    """Remove SystemVerilog comments, honouring whichever opener comes FIRST.

    THIS WAS TWO REGEXES AND THEY WERE IN THE WRONG ORDER.

        s = re.sub(r"/\\*.*?\\*/", "", s, flags=re.S)   # block comments, first
        return re.sub(r"//[^\\n]*", "", s)              # line comments, second

    Stripping block comments FIRST means a `/*` that appears inside a `//` LINE
    comment is treated as a real block-comment opener -- and with `re.S` it then
    swallows every line up to the next `*/` anywhere in the file. In
    SystemVerilog that `/*` is just text: the `//` already ended the line.

    IT HAPPENED, on 2026-09-19. A block's header comment spelled a path as a
    shell glob with a star after a slash. The stripper opened a block comment
    there, the `module` keyword disappeared with the next few hundred lines, and
    `gen_prod_top.py` reported "no port list found" and left that module OUT OF
    THE PRODUCTION TOP -- one indented line among sixty-three.

    WHY THIS IS WORTH A CAREFUL FUNCTION AND NOT A THIRD REGEX. Three tools that
    decide WHAT GETS MEASURED share this one implementation: gen_prod_top.py,
    check_prod_manifest.py and check_ownership_roles.py. A module that vanishes
    here is a module the production top does not instantiate, which is area the
    fit never sees -- the resource number comes back SMALLER, which is the
    flattering direction and the one this campaign must never move by accident.
    The generator's RC 1 and the manifest's freshness check did both catch it
    this time. Neither is a reason to leave the cause in place.

    A single left-to-right scan, because that is what a lexer does and the
    ordering bug cannot be expressed in it. String literals are honoured too: a
    `//` inside a string is not a comment, and `$display("a // b")` is legal.
    Newlines inside block comments are PRESERVED so line numbers still line up
    for any caller that reports them.
    """
    out = []
    i, n = 0, len(s)
    while i < n:
        c = s[i]
        if c == '"':
            j = i + 1
            while j < n and s[j] != '"':
                j += 2 if s[j] == "\\" else 1
            out.append(s[i:min(j + 1, n)])
            i = j + 1
        elif s.startswith("//", i):
            j = s.find("\n", i)
            if j < 0:
                break               # trailing line comment, nothing after it
            i = j                   # keep the newline itself
        elif s.startswith("/*", i):
            j = s.find("*/", i + 2)
            blk = s[i:j] if j >= 0 else s[i:]
            out.append("\n" * blk.count("\n"))
            i = (j + 2) if j >= 0 else n
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _strip_comments_self_test():
    """Fire the exact defect, plus the cases a careless fix would break."""
    cases = [
        # THE DEFECT, in the exact shape that caused it: a single-star glob in a
        # line comment opens a block that runs to the NEXT `*/` anywhere below.
        ("// see fpga/rtl/compositor/*.sv\nmodule m;\nendmodule\n/* later */\n",
         "module m;"),
        # The same idea without a path, to show it is the `/*` and not the glob.
        ("// a /* b\nmodule m;\nendmodule\n/* real */\n", "module m;"),
        # NEGATIVE CONTROL, and it is here because it is the case a person
        # writing this test reaches for FIRST and it proves nothing: in `**/*`
        # the `/*` is immediately closed by the `*/` two characters later, so it
        # is an empty block comment and the OLD, BROKEN stripper passes it too.
        # A test built only from this case would have been green against the
        # very defect it was written for.
        ("// see fpga/rtl/**/*.sv\nmodule m;\nendmodule\n", "module m;"),
        # real block comments still go
        ("/* module hidden; */\nmodule m;\n", "module m;"),
        # a multi-line block comment still goes, and keeps its newlines
        ("/* a\nb\nc */module m;\n", "module m;"),
        # `//` inside a string is not a comment
        ('$display("a // b");\nmodule m;\n', '"a // b"'),
        # a line comment still removes what follows it on that line
        ("module m; // endmodule\n", "module m; "),
    ]
    for src, must_contain in cases:
        got = strip_comments(src)
        if must_contain not in got:
            raise AssertionError(
                "strip_comments lost %r\n  in: %r\n  got: %r"
                % (must_contain, src, got))
    # and the negative: a genuine block comment's contents must be GONE
    if "hidden" in strip_comments("/* module hidden; */\nmodule m;\n"):
        raise AssertionError("strip_comments kept a real block comment's body")
    return len(cases)


def build(rtl_dir="fpga/rtl"):
    files = load(rtl_dir)
    decl = {}
    for p, s in files.items():
        for m in re.finditer(r"^\s*module\s+(\w+)", s, re.M):
            decl[m.group(1)] = p
    names = set(decl)
    inst = collections.defaultdict(set)
    for p, s in files.items():
        body = strip_comments(s)
        for mod in names:
            if decl[mod] == p:
                continue
            # The trailing boundary is not optional: without it
            # `zhao_terrain_bake` matches inside `zhao_terrain_bake_delta` and
            # invents a cycle between a module and its own child.
            pat = (r"(?<![\w.])" + mod + r"(?![\w$])"
                   r"\s*(?:#\s*\((?:[^;]*?)\)\s*)?[A-Za-z_]\w*\s*\(")
            if re.search(pat, body):
                inst[p].add(mod)
    return decl, inst


def main():
    decl, inst = build()
    instantiated = set()
    for ms in inst.values():
        instantiated |= ms
    roots = sorted(m for m in decl if m not in instantiated)
    print("modules: %d   roots: %d" % (len(decl), len(roots)))
    if "--edges" in sys.argv:
        for p in sorted(inst):
            print(p.replace("fpga/rtl/", "") + " -> " + ", ".join(sorted(inst[p])))
        return
    for m in roots:
        print("  ROOT  %-38s %s" % (m, decl[m].replace("fpga/rtl/", "")))


if __name__ == "__main__":
    main()
