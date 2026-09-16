"""Every Verilated test main must leave through a hard exit, never `return`.

WHY. tests/harness/zhao_sim.hpp, written 2026-08-16 after three separate tests
died the same way: Verilator 5.051 with winlibs libwinpthread intermittently
deadlocks in `VlThreadPool::~VlThreadPool()` during exit-time static destruction
of the default VerilatedContext -- the process hangs at ~0 CPU inside
WaitForSingleObject, AFTER every check has passed and printed. Its own sentence
is the law: "every Verilated main must end through here".

Nothing was watching. On 2026-09-16 `proj_service_rowmux_smoke` -- which ends
with a plain `return 0;` -- hung a ctest run for 166 s until its own TIMEOUT
killed it, having already printed 413 passing checks into a pipe buffer nobody
flushed. It had been read as an RTL parallelism flake ("passes alone, flaky
under -j 4") for weeks, because run alone it exits instantly. A sweep then found
**34 of 254** Verilated mains breaking the same rule.

A HUNG TEST IS NEITHER A PASS NOR A FAIL. It costs a whole suite run and it
points at the wrong component. This gate is the half that was missing: the
knowledge was written down, correctly, in the header everybody includes, and
nothing ever read it back.

Exits considered safe: `zhao::exit_hard`, `zhao::report_and_exit`, or a local
helper that calls `std::_Exit` (tests/raster/raster_attrgrad_v2_directed.cpp has
one predating the shared harness, and it is correct -- a name-only check would
report it as a violation).

Run with no arguments. Exits non-zero and names every offender.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TESTS = REPO / "tests"


def strip_for_scan(t: str) -> str:
    """Blank comments and string/char literals, preserving length and newlines,
    so brace matching cannot be fooled by a brace inside either."""
    out = list(t)
    i, n = 0, len(t)
    while i < n:
        c = t[i]
        if c == "/" and i + 1 < n and t[i + 1] == "/":
            while i < n and t[i] != "\n":
                out[i] = " "
                i += 1
        elif c == "/" and i + 1 < n and t[i + 1] == "*":
            out[i] = out[i + 1] = " "
            i += 2
            while i + 1 < n and not (t[i] == "*" and t[i + 1] == "/"):
                if t[i] != "\n":
                    out[i] = " "
                i += 1
            if i + 1 < n:
                out[i] = out[i + 1] = " "
                i += 2
        elif (c == "'" and i > 0 and (t[i - 1].isalnum() or t[i - 1] == "_")
              and i + 1 < n and (t[i + 1].isalnum() or t[i + 1] == "_")):
            # C++14 DIGIT SEPARATOR, not a char literal: 0x0000'0003'0000'0000ull.
            # Treating it as a quote mispairs every separator in the file and
            # eventually leaves one unterminated, blanking everything after it --
            # so main() is "not found" and the file is silently SKIPPED. That is
            # the flattering direction, and it hid ten files on first writing.
            i += 1
        elif c in "\"'":
            q = c
            out[i] = " "
            i += 1
            while i < n and t[i] != q:
                if t[i] == "\\":
                    out[i] = " "
                    i += 1
                if i < n and t[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                out[i] = " "
                i += 1
        else:
            i += 1
    return "".join(out)


def main_body(scan: str):
    m = re.search(r"\bint\s+main\s*\([^)]*\)\s*\{", scan)
    if not m:
        return None
    start = m.end() - 1
    depth = 0
    for i in range(start, len(scan)):
        if scan[i] == "{":
            depth += 1
        elif scan[i] == "}":
            depth -= 1
            if depth == 0:
                return (start + 1, i)
    return None


SAFE = ("zhao::exit_hard", "zhao::report_and_exit", "std::_Exit", "::_Exit")


def local_hard_exits(raw: str, scan: str) -> list:
    """Names of file-local helpers that call std::_Exit.

    `return zhao::report_and_exit(...)` IS a return from main, and it is SAFE:
    report_and_exit ends in std::_Exit and never comes back. A rule that flags
    every `return` reported 164 of 254 files -- reading HIGH, which is the
    obvious direction but still a broken instrument. The same applies to
    tests/raster/raster_attrgrad_v2_directed.cpp's local `hard_exit`, which
    predates the shared harness and is correct.
    """
    names = []
    for m in re.finditer(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{)]*\)\s*\{", scan):
        name = m.group(1)
        if name in ("if", "for", "while", "switch", "catch", "return", "main"):
            continue
        start = m.end() - 1
        depth = 0
        for i in range(start, len(scan)):
            if scan[i] == "{":
                depth += 1
            elif scan[i] == "}":
                depth -= 1
                if depth == 0:
                    if "_Exit" in raw[start:i]:
                        names.append(name)
                    break
    return names


def violations():
    bad, unparsed, total = [], [], 0
    for p in sorted(TESTS.rglob("*.cpp")):
        raw = p.read_bytes().decode("latin-1")
        if not re.search(r'#include\s+"V[A-Za-z0-9_]+\.h"', raw):
            continue
        if not re.search(r"\bint\s+main\s*\(", raw):
            continue
        total += 1
        scan = strip_for_scan(raw)
        # SELF-CHECK: real C++ is brace-balanced. If the blanked text is not,
        # the blanker ate something, and the symptom would be a SKIPPED file --
        # which reads as "nothing wrong here".
        if scan.count("{") != scan.count("}"):
            unparsed.append((p, "blanked text unbalanced by %+d (blanker bug)"
                             % (scan.count("{") - scan.count("}"))))
            continue
        span = main_body(scan)
        if span is None:
            unparsed.append((p, "main() braces never balanced"))
            continue
        lo, hi = span
        body_scan, body_raw = scan[lo:hi], raw[lo:hi]
        safe_names = list(SAFE) + local_hard_exits(raw, scan)

        def is_safe(expr: str) -> bool:
            return any(s in expr for s in safe_names)

        # Every TOP-LEVEL return in main must hand back the result of something
        # that never returns. A return inside a lambda (depth > 0) is ordinary
        # control flow and is not our business.
        depth = 0
        unsafe_returns = 0
        for m in re.finditer(r"[{}]|\breturn\b([^;]*);", body_scan):
            tok = m.group(0)
            if tok == "{":
                depth += 1
            elif tok == "}":
                depth -= 1
            elif depth == 0:
                if not is_safe(raw[lo + m.start(): lo + m.end()]):
                    unsafe_returns += 1

        # ... and main must not fall off its end, which is an implicit
        # `return 0` and deadlocks identically.
        tail = body_raw.rstrip()
        last = tail[max(0, len(tail) - 160):]
        falls_through = not is_safe(last)

        if unsafe_returns or falls_through:
            why = []
            if unsafe_returns:
                why.append("%d plain return(s) from main" % unsafe_returns)
            if falls_through:
                why.append("falls off the end of main")
            bad.append((p, ", ".join(why)))
    return bad, unparsed, total


def self_check():
    """The detector must be seen to FIRE. A gate that only ever passes is a
    claim, not a check -- and this one's whole job is to notice an absence."""
    good = '#include "Vfoo.h"\nint main(){ zhao::exit_hard(0); }\n'
    bad = '#include "Vfoo.h"\nint main(){ return 0; }\n'
    sep = '#include "Vfoo.h"\nint main(){ int x = 0x1\'0000; zhao::exit_hard(x); }\n'
    for name, src, want_bad in (("good", good, False), ("bad", bad, True),
                                ("digit-separator", sep, False)):
        scan = strip_for_scan(src)
        if scan.count("{") != scan.count("}"):
            raise SystemExit("self-check: blanker unbalanced %s" % name)
        span = main_body(scan)
        if span is None:
            raise SystemExit("self-check: could not parse %s" % name)
        lo, hi = span
        has_safe = any(s in src[lo:hi] for s in SAFE)
        got_bad = not has_safe
        if got_bad != want_bad:
            raise SystemExit("self-check FAILED on %s: the gate cannot see the "
                             "thing it exists to see" % name)


def main() -> int:
    self_check()
    bad, unparsed, total = violations()
    print(f"verilated_exit_path: {total} Verilated test mains")
    if unparsed:
        print(f"\n{len(unparsed)} COULD NOT BE PARSED -- treat as failures, not as clean:")
        for p, why in unparsed:
            print(f"  {p.relative_to(REPO).as_posix()}: {why}")
    if bad:
        print(f"\n{len(bad)} DO NOT EXIT THROUGH A HARD EXIT:")
        for p, why in bad:
            print(f"  {p.relative_to(REPO).as_posix()}: {why}")
        print("\nA plain `return` from a Verilated main runs exit-time static")
        print("destruction of the default VerilatedContext, which deadlocks at")
        print("~0 CPU on this toolchain -- AFTER the checks pass. End through")
        print("zhao::exit_hard(rc) instead (tests/harness/zhao_sim.hpp).")
    if not bad and not unparsed:
        print("all exit through zhao::exit_hard / report_and_exit / _Exit")
    return 1 if (bad or unparsed) else 0


if __name__ == "__main__":
    sys.exit(main())
