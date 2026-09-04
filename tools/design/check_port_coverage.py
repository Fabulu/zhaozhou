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

THE KNOWN FALSE POSITIVE: A PORT OBSERVED THROUGH ITS CONSUMER
---------------------------------------------------------------
Measured 2026-09-04, working the first list this tool produced. It named four
`zhao_field_exec_shared` fault reporters as UNMENTIONED:

    exec_unsupported_o  exec_sat_add_o  exec_sat_mul_o  exec_sat_rescale_o

They are covered. Not by any test naming them -- no testbench instantiates
`zhao_field_exec_shared` at all -- but by COMPOSITION. `zhao_field_seq.sv`
consumes them, accumulates them across a whole program exactly as the
reference's single `SatLedger` does:

    sat_add_o <= sat_add_o || exec_sat_add;

and `field_curve_svc_directed` compares the resulting `rsp_sat_add_o` per lane
against that reference. So the port's BEHAVIOUR is checked while its NAME
appears nowhere.

(An earlier version of this note said these were compared through a testbench
wrapper that re-exports ports under shorter names, citing `zhao_field_alu_tb.sv`
and its `.sat_add_o (sat_add_o)`. **That was wrong.** Those are
`zhao_field_alu`'s own ports -- a different module's signals that happen to
share a stem. The conclusion "these four are covered" survived; the mechanism
given for it did not, and a wrong mechanism is how the next person mis-triages
the next row.)

**So UNMENTIONED is evidence about NAMES, not about coverage.** The claim above
that it is "close to proof" is too strong wherever a port feeds another RTL
module rather than a testbench.

That does not make the tier useless -- the very same list found
`zhao_surface_sheet.res_overflow_o`, a genuine unobserved fault line, and the
three `o_uv_sat_o` ports whose INPUT `f_uv_sat_i` was hard-wired to 0 in all
five places any test set it. That second finding is the stronger one and the
tool cannot see it at all: an output nothing reads is a gap, but an output whose
CAUSE never occurs is a bigger gap wearing the same clothes. Reading the flagged
port's own input is the manual step this tool does not replace.

IT COULD NOT SEE THE PORT IT WAS WRITTEN FOR
---------------------------------------------
The worst of the three, found 2026-09-04 while writing `check_counters.py`
against the same regex. `PORT_RE` had no provision for a WIDTH BRACKET:

    output var logic        exec_sat_add_o,       <- matched
    output var logic [31:0] meshlets_fetched_o,   <- SILENTLY SKIPPED

So every port with a declared width was invisible, and the tool reported
confidently on a subset it never disclosed. Including, exactly:

    output var logic [30:0] out_w_o,

`out_w_o` is the port whose missing differential motivated this file and is
named in the section above as the founding example. **The tool written to catch
it could not see it.**

Fixing the bracket moved every number this tool produces:

    output ports named by no test    29  ->  95
    read but never obviously compared 313 -> 936
    unmentioned FAULT reporters        4  ->   5

and the new fault reporter, `zhao_raster_texjoin.uv_sat_fragments_o`, is a
counter of saturated fragments -- width-bearing, therefore previously invisible.

The lesson is not "regexes are hard". It is that a tool reporting a SMALL number
looks like good news and is the one result nobody audits. A parser that silently
drops what it cannot match will always report progress.

AND THE COUNT GOING DOWN CAN MEAN IT WENT BLIND
------------------------------------------------
Same afternoon, the nastier version. Adding a `o_uv_sat_o` check to
`raster_texjoin_v2_directed` took the FAULT REPORTER count from 8 to 4 -- but
only ONE of the four that vanished was actually tested. `zhao_raster_texjoin`
and `zhao_texture_fragrob` export a port of the SAME NAME, the name now appears
somewhere in the test blob, and all three rows disappeared together.

The search is global by port name and has no notion of which module a mention
belongs to. So a shrinking count is not progress on its own: **a port can leave
this list because it got checked, or because a namesake elsewhere did.** Treat
the number as a worklist, never as a score. Making it module-aware would fix
this and is worth doing; until then the docstring is the warning.

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
    r"^\s*output\s+(?:var\s+)?(?:[\w:]+\s+)*?(?:\[[^\]]*\]\s*)*(\w+)\s*(?:\[[^\]]*\]\s*)*(?:,|\)|;)?\s*(?://.*)?$"
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
          "UNMENTIONED is evidence about NAMES (a tb wrapper that renames "
          "defeats it -- see the docstring), READ-ONLY is a prompt.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
