#!/usr/bin/env python3
"""Scan RTL for multiplies whose operand is an ARITHMETIC EXPRESSION over a
widened value -- the shape ATTRSETUP measured at 9 DSP blocks.

Comparison side only: it finds candidate SITES. Whether a site costs anything
is decided by quartus_map, never by this file.
"""
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "fpga/rtl"


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    out = []
    for ln in text.split("\n"):
        i = ln.find("//")
        out.append(ln[:i] if i >= 0 else ln)
    return "\n".join(out)


def mul_positions(line):
    """Indices of every '*' that is a multiply (not ** or (* or */)."""
    res = []
    for i, ch in enumerate(line):
        if ch != "*":
            continue
        prev = line[i - 1] if i else ""
        nxt = line[i + 1] if i + 1 < len(line) else ""
        if prev in ("*", "(", "/"):
            continue
        if nxt in ("*", "/", ")"):
            continue
        res.append(i)
    return res


BREAK = "+-*/,=<>?:&|^;"


def balanced_left(s, end):
    depth = 0
    i = end - 1
    while i >= 0 and s[i] == " ":
        i -= 1
    stop = i + 1
    while i >= 0:
        c = s[i]
        if c == ")":
            depth += 1
        elif c == "(":
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and c in BREAK:
            break
        i -= 1
    return s[i + 1:stop].strip()


def balanced_right(s, start):
    depth = 0
    i = start
    while i < len(s) and s[i] == " ":
        i += 1
    begin = i
    while i < len(s):
        c = s[i]
        if c == "(":
            depth += 1
        elif c == ")":
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and c in BREAK:
            break
        i += 1
    return s[begin:i].strip()


# An operand that is only a cast of a plain identifier is PROVEN FREE
# (probe arms 1, 2 and 5 all moved the row by zero). Anything carrying an
# operator INSIDE the widening is a candidate for the arm-7 mechanism.
PLAIN = re.compile(r"^[(\s]*(?:\d+'s?)?\(?\s*[A-Za-z_][\w.\[\]:$']*\s*\)?[)\s]*$")
CAST = re.compile(r"\d+'\s*\(|\$signed\s*\(\s*\{|\{\s*\{\s*\d+\s*\{")
OPS = re.compile(r"<<<|>>>|<<|>>|[-+?]")


def main():
    hits = []
    for dirpath, _, files in os.walk(ROOT):
        for fn in files:
            if not (fn.endswith(".sv") or fn.endswith(".svh")):
                continue
            p = os.path.join(dirpath, fn).replace("\\", "/")
            raw = open(p, encoding="utf-8", errors="replace").read()
            txt = strip_comments(raw)
            for lineno, line in enumerate(txt.split("\n"), 1):
                if "*" not in line:
                    continue
                for pos in mul_positions(line):
                    for side, op in (("L", balanced_left(line, pos)),
                                     ("R", balanced_right(line, pos + 1))):
                        if not op or PLAIN.match(op):
                            continue
                        if not CAST.search(op):
                            continue
                        if not OPS.search(op):
                            continue
                        hits.append((p, lineno, side, op, line.strip()))

    seen = set()
    for p, ln, side, op, full in hits:
        key = (p, ln, op)
        if key in seen:
            continue
        seen.add(key)
        print("%s:%d  [%s]  %s" % (p, ln, side, op))
        print("        %s" % full[:170])
    print("\nTOTAL candidate sites: %d" % len(seen))


if __name__ == "__main__":
    main()
