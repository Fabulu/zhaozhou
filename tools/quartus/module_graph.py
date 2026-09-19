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
            # A FILE IS NOT A MODULE -- and this line used to say it was.
            #
            # It read `if decl[mod] == p: continue`, which skips a module
            # whenever the file being scanned is the file that DECLARES it. The
            # intent was to stop a module appearing to instantiate itself. The
            # effect was to make EVERY EDGE INSIDE A MULTI-MODULE FILE
            # invisible, and five files in this tree declare more than one
            # module. Eleven real edges were suppressed:
            #
            #   zhao_sys_pll.sv       -> zhao_sys_pll_simclk    (x4 instances)
            #   zhao_sys_reset.sv     -> zhao_sys_reset_sync    (x4 instances)
            #   zhao_raster_quant.sv  -> zhao_raster_quant_fin
            #   the two generated shell fit tops -> their stimulus and 3 sinks
            #
            # It reads in the FLATTERING DIRECTION, exactly as this function's
            # own docstring warns: a missed edge makes a module look
            # uninstantiated, so it becomes a graph ROOT, so it looks like dead
            # weight or like something nobody built a consumer for. On
            # 2026-09-19 that cost a real breakage -- `check_console_inventory`
            # called `zhao_raster_quant.sv` dead, it was removed from the
            # console closure on the gate's say-so, and the console lint broke.
            # That gate has since been taught the same lesson at its own level;
            # THIS is where the lesson belongs, because three other tools read
            # this graph (`gen_prod_top.py`, `check_prod_manifest.py`,
            # `check_ownership_roles.py`) and each inherited the blind spot.
            #
            # REMOVING THE SKIP ADDS NO FALSE EDGES, measured rather than
            # assumed: with it gone, no module matches its own instantiation
            # pattern inside its own file, because a DECLARATION is
            # `module foo #(...) (` -- no instance name between the parameter
            # list and the port list -- while an INSTANTIATION requires one.
            # All eleven new edges were checked by hand and all eleven are real.
            # `gen_prod_top.py --check` stays fresh at 70 instances and
            # `check_prod_manifest.py` prints byte-identical output either way,
            # so nothing downstream moves; what moves is that the sys blocks are
            # now visibly reachable from `zhao_console_board`.
            #
            # The trailing boundary is not optional: without it
            # `zhao_terrain_bake` matches inside `zhao_terrain_bake_delta` and
            # invents a cycle between a module and its own child.
            pat = (r"(?<![\w.])" + mod + r"(?![\w$])"
                   r"\s*(?:#\s*\((?:[^;]*?)\)\s*)?[A-Za-z_]\w*\s*\(")
            if re.search(pat, body):
                inst[p].add(mod)
    return decl, inst


def _intra_file_self_test():
    """The multi-module case, on a file this test writes itself.

    CLAUDE.md: a detector that has not been shown to FIRE has not been tested.
    The defect this guards against was silent for sixteen days and cost a
    broken console lint, and its whole signature is an edge that is simply
    absent -- there is no error message to notice. So the control is an
    ACTUAL build() over an actual two-module file, not an assertion about a
    regex.
    """
    import shutil
    import tempfile
    tmp = tempfile.mkdtemp(prefix="zhao_mg_selftest_")
    try:
        with io.open(os.path.join(tmp, "pair.sv"), "w", encoding="utf-8") as fh:
            fh.write(
                "module zhao_helper #(parameter int D = 1) (input logic a);\n"
                "endmodule\n"
                "module zhao_parent #(parameter int D = 1) (input logic a);\n"
                "  zhao_helper #(.D(D)) u_one (.a(a));\n"
                "endmodule\n")
        with io.open(os.path.join(tmp, "solo.sv"), "w", encoding="utf-8") as fh:
            fh.write("module zhao_solo (input logic a);\nendmodule\n")
        decl, inst = build(tmp)
        edges = inst.get(tmp.replace(SEP, "/") + "/pair.sv", set())
        assert "zhao_helper" in edges, (
            "THE DEFECT IS BACK: an instantiation inside the declaring file is "
            "invisible again, and nothing downstream will say so. edges=%r"
            % (edges,))
        # The negative half, which is what the removed skip was reaching for:
        # a module must NOT be reported as instantiating itself.
        assert "zhao_parent" not in edges, "a declaration is not an instantiation"
        assert "zhao_solo" not in edges, "no edge to an unrelated module"
        assert set(decl) == {"zhao_helper", "zhao_parent", "zhao_solo"}, decl
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return True


def main():
    _strip_comments_self_test()
    _intra_file_self_test()
    print("module_graph: self-test PASSED (comment stripping; an edge INSIDE a "
          "multi-module file is visible and a declaration is not an edge)")
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
