# sweep_surface_stamp_consumers.py — GUARD 7 for a file reached through a
# CMake VARIABLE rather than by literal path.
#
# tools/sweep_geom_skin.sh derives its consumer set with
# `re.finditer(r'verilate\((\w+)(.*?)\.sv\)')`, which works because every
# geometry `verilate()` names its sources literally. The surface targets do not:
# they say `SOURCES ${ZHAO_SURFACE_STAMP_SV}`, and that regex finds NO consumer
# for `zhao_surface_stamp.sv` at all.
#
# A guard that finds no consumer aborts, which is the safe direction — but the
# failure mode one edit away is worse: had the regex matched the *frontier*
# targets (which do spell their sources out) and not the three main ones, the
# sweep would have silently scored two of five builds and reported a number.
# So this resolves `set(VAR ...)` and expands `${VAR}` instead of pattern-
# matching around the problem.
#
# Usage: python tools/sweep_surface_stamp_consumers.py <rtl-path>
#        -> space-separated target names, sorted, on stdout.
import io
import re
import sys

CMAKE = "tests/CMakeLists.txt"


def main():
    path = sys.argv[1].replace(chr(92), "/")
    base = path.split("/")[-1]
    s = io.open(CMAKE, encoding="utf-8").read()

    # 1. every set(NAME <words>) -- enough for the flat file lists used here.
    variables = {}
    for m in re.finditer(r"set\(\s*(\w+)\s+([^()]*?)\)", s, re.S):
        variables[m.group(1)] = m.group(2)

    def expand(text, depth=0):
        if depth > 8:
            return text
        out = re.sub(r"\$\{(\w+)\}", lambda g: variables.get(g.group(1), ""), text)
        return expand(out, depth + 1) if out != text else out

    # 2. every verilate(TARGET ...) block, sources expanded.
    targets = []
    for m in re.finditer(r"verilate\(\s*(\w+)([^()]*(?:\([^()]*\)[^()]*)*)\)", s, re.S):
        target, body = m.group(1), expand(m.group(2))
        if base in body:
            targets.append(target)

    if not targets:
        sys.exit(
            "ABORT: no verilate() in %s reaches %s. A mutation there would be\n"
            "       applied, built into nothing, and scored against a stale\n"
            "       binary." % (CMAKE, base)
        )
    print(" ".join(sorted(set(targets))))


if __name__ == "__main__":
    main()
