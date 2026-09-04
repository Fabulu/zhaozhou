#!/usr/bin/env python3
"""check_port_coverage.py -- which RTL OUTPUT PORTS does no test ever mention?

WHY THIS EXISTS
---------------
On 2026-09-04 `out_w_o` was added to `zhao_geom_project.sv` and the port went in
with no differential. `VtxOut` grew a `w` for the DUT to read into, the oracle
never set it, and `compare()` never checked it -- so the missing expectation and
the missing check CANCELLED, and no test failure could reveal the gap. cppcheck
eventually flagged the uninitialised member; 488 tests could not.

The lesson was "a new port is not delivered until something compares it", and a
lesson written in a contract is a lesson that has to be remembered. This is the
same statement as a gate.

WHAT IT IS AND IS NOT
---------------------
It is a COMPARISON-SIDE tool, which is the only kind this repository trusts
(CLAUDE.md: "a tool that says 'you are 20% off' is good; a tool that decides a
radius is not"). It reads what was authored and reports what nothing checks. It
never edits, never generates, and never decides whether a port SHOULD exist.

It is a HEURISTIC and says so. A port name appearing in a test file is not proof
the test compares it -- `o.w = dut_.out_w_o` mentions the port while comparing
nothing. So a mention is treated as the WEAKEST possible evidence, and the tool
reports two tiers:

    UNMENTIONED  no test file names this port at all -- a real gap, high
                 confidence, because a port nothing names cannot be checked.
    READ-ONLY    a test assigns FROM the port but no line near it looks like a
                 comparison. This is what out_w_o looked like, and it is the
                 tier that carries false positives.

The second tier is a prompt to go and look, never a verdict. Reporting it as
one would be the "gate passing is not the thing looking right" failure with the
sign flipped.
"""
from __future__ import annotations

import io
import os
import re
import sys

RTL_DIRS = ["fpga/rtl"]
TEST_DIRS = ["tests"]

# `output var logic [30:0] out_w_o,` / `output logic out_behind_o,` / trailing `)`
PORT_RE = re.compile(
    r"^\s*output\s+(?:var\s+)?(?:\w+\s+)*?(\w+)\s*(?:,|\)|;)\s*(?://.*)?$"
)
MODULE_RE = re.compile(r"^\s*module\s+(\w+)")

# Lines that look like a comparison rather than a plain read. Deliberately
# broad: a false "this is compared" is a missed gap, so the bar is low and the
# tool errs toward saying nothing.
COMPARE_HINT = re.compile(
    r"\b(check|assert|EXPECT|REQUIRE|compare|==|!=|memcmp|mismatch)\b"
)

# Ports that REPORT A FAULT. An unchecked fault output is worse than an
# unchecked data output, by exactly the argument spec/counters.md makes about
# counters: the whole point of the signal is to be non-zero when something is
# wrong, so nothing ever reading it means the fault it reports has never once
# been observed. Separated out because a debug `busy` going unchecked is a
# shrug and `fatal_error_o` going unchecked is not.
FAULT_RE = re.compile(
    r"(error|fatal|overflow|underflow|_sat|sat_|unsupported|violation|"
    r"refused|denied|illegal|degenerate|drop)", re.I
)


def read(path: str) -> str:
    return io.open(path, encoding="utf-8", errors="replace").read()


def walk(dirs, exts):
    for d in dirs:
        for root, _dirs, files in os.walk(d):
            for f in files:
                if f.endswith(exts):
                    yield os.path.join(root, f).replace("\\", "/")


def module_outputs(path: str):
    """Ports of the FIRST module in the file, up to the closing `);`."""
    out, mod, inhdr = [], None, False
    for line in read(path).splitlines():
        if mod is None:
            m = MODULE_RE.match(line)
            if m:
                mod, inhdr = m.group(1), True
            continue
        if inhdr:
            m = PORT_RE.match(line)
            if m:
                out.append(m.group(1))
            # The port list ends at a `);` that is not part of a port line.
            if re.match(r"^\s*\);\s*$", line):
                inhdr = False
    return mod, out


def main() -> int:
    tests = {p: read(p) for p in walk(TEST_DIRS, (".cpp", ".hpp", ".sv"))}
    blob = "\n".join(tests.values())

    unmentioned, readonly, no_test = [], [], []

    for rtl in sorted(walk(RTL_DIRS, (".sv",))):
        mod, ports = module_outputs(rtl)
        if not mod or not ports:
            continue
        # Does ANY test name this module? If not, the block has no test at all,
        # which is a different (and already-tracked) fact -- reported separately
        # so it does not drown the port findings.
        if mod not in blob:
            no_test.append((mod, rtl, len(ports)))
            continue
        for p in ports:
            if p not in blob:
                unmentioned.append((mod, p))
                continue
            # Mentioned. Is it mentioned anywhere that looks like a comparison?
            compared = False
            for text in tests.values():
                if p not in text:
                    continue
                for line in text.splitlines():
                    if p in line and COMPARE_HINT.search(line):
                        compared = True
                        break
                # A port is often read into a struct field on one line and
                # compared by that field's name later. Treat the whole file as
                # the window rather than the line, or every wrapper looks bare.
                if not compared and COMPARE_HINT.search(text):
                    stem = p[:-2] if p.endswith("_o") else p
                    if re.search(r"\b%s\b" % re.escape(stem), text):
                        compared = True
                if compared:
                    break
            if not compared:
                readonly.append((mod, p))

    show_all = "--all" in sys.argv
    faults = [(m, p) for m, p in unmentioned if FAULT_RE.search(p)]
    plain = [(m, p) for m, p in unmentioned if not FAULT_RE.search(p)]

    print("port coverage: %d output port(s) named by no test (%d of them FAULT "
          "reporters), %d read but never obviously compared, %d module(s) with "
          "no test" % (len(unmentioned), len(faults), len(readonly), len(no_test)))

    if faults:
        print("\nUNMENTIONED FAULT REPORTERS -- read these first. A signal "
              "whose job is to be non-zero when something is wrong, that "
              "nothing ever reads, has never once been observed:")
        for mod, p in faults:
            print("  %-34s %s" % (mod, p))

    if plain:
        print("\nUNMENTIONED -- no test file names these at all:")
        for mod, p in plain:
            print("  %-34s %s" % (mod, p))

    if readonly:
        # 300+ lines is not a report, it is a wall. The heuristic is too weak to
        # justify that much output, and a tool that buries its strong finding
        # under its weak one is worse than one that only reports the strong one.
        by_mod = {}
        for mod, p in readonly:
            by_mod.setdefault(mod, []).append(p)
        print("\nREAD-ONLY (weak heuristic): %d port(s) across %d module(s). "
              "Pass --all to list them." % (len(readonly), len(by_mod)))
        if show_all:
            for mod in sorted(by_mod):
                print("  %-34s %s" % (mod, " ".join(sorted(by_mod[mod]))))

    print("\nNOTE: this tool REPORTS, it does not gate. A mention is not a "
          "comparison and an absence is not always a defect -- a debug counter "
          "may legitimately go unchecked. Read the two lists differently: "
          "UNMENTIONED is close to proof, READ-ONLY is a prompt.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
