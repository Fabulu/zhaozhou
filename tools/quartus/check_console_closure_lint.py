#!/usr/bin/env python3
"""Does the console's REAL fit closure elaborate, with no implicit or missing net?

WHY THIS EXISTS, and it is not because the check was missing.

`tools/quartus/edgeclose-lint-core.ps1` has done exactly this job since it was
committed: it parses `design/fit_targets.yml`'s `zhao_console_core` source list
and lints it with `--top-module zhao_console_core`. It is correct, and on
2026-09-26 a grep found it is **referenced NOWHERE** -- no gate, no ctest, no
runner, no document. Zero callers, ever.

That is `uncashed_cheques.py`'s own subject arriving one level up. That tool
finds a MODULE built and instantiated nowhere; nothing in the tree finds a TOOL
built and invoked nowhere, because the checker that would notice is itself the
kind of thing that goes unrun.

WHAT IT COST. On 2026-09-26 `u_geom_tidq` -- entry I54's triangle-identity
queue, composed that morning -- was found wired `.clk (clk)` in a module whose
clock is `gpu_clk`. 149 instances connect `gpu_clk`; exactly one said `clk`. An
undeclared identifier in a port connection is an IMPLICIT NET, so the queue sat
on an undriven wire and never clocked: `id_o` held its reset value and the
continuation tail carried a CONSTANT where the arena id belongs. The PowerShell
tool runs `-Wall`, so it would have made that fatal on the day it was composed.
It was found instead by a hand lint run to answer an unrelated question.

WHY THIS IS A SEPARATE FILE AND NOT A CALL TO THAT ONE.

  1. `gate_sweep.py` DISCOVERS `check_*.py` under `tools/`. A `.ps1` cannot be
     found by that rule however good it is -- which is precisely why the
     existing tool was never swept up.
  2. Its polarity is unusable as a gate. `-Wall` makes every warning fatal, and
     the closure legitimately carries ~147 of them (108 UNUSEDSIGNAL, 37
     PINCONNECTEMPTY, plus a documented UNDRIVEN shadow table that
     `zhao_texture_island_v3_top` declares dead in its own comment). A gate that
     is permanently red is a gate people learn to skip -- this file's whole
     subject. So this one lints with `-Wno-fatal` and fails ONLY on the
     STRUCTURAL classes below.

WHAT IT REFUSES, and nothing else:

  * IMPLICIT   -- an undeclared identifier silently becoming a 1-bit wire. This
                  is the class that cost a dead clock.
  * MODMISSING -- a module the closure names and does not contain. Two hours
                  into a Quartus fit is an expensive place to learn this.
  * PINMISSING -- an instance port nobody connected, which is how a stale
                  generated top announces itself.
  * any `%Error` other than Verilator's own "Exiting due to N warning(s)".

ABSENT TOOLCHAIN IS RC 2, NOT RC 0. A gate that returns success when it could
not run is the failure this repository has paid for more than once -- a
skip-if-absent gate hid weeks of drift. If Verilator is not where this expects
it, this says so and returns nonzero.
"""

from __future__ import annotations

import os
import re
import subprocess
import tempfile
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FIT_TARGETS = os.path.join(REPO, "design", "fit_targets.yml")
TOP = "zhao_console_core"

# The toolchain, pinned. `tools/env/zhao-env.ps1` sets these for a PowerShell
# session; a gate is run BARE from any shell, so it sets them itself rather than
# inheriting an environment that may not exist.
OSS = os.path.join(r"C:\programmieren\zencrifice", ".tools", "oss-cad-suite")
VERILATOR_BIN = os.path.join(OSS, "bin", "verilator_bin.exe")
VERILATOR_ROOT = os.path.join(OSS, "share", "verilator")

# The structural classes. Everything else in a -Wall run of this closure is
# recorded, intentional noise -- see the header.
STRUCTURAL = re.compile(r"%(?:Warning|Error)-(IMPLICIT|MODMISSING|PINMISSING)\b")
# Verilator's own tally line, which is a CONSEQUENCE of warnings and not a
# finding. With -Wno-fatal it should not appear at all; matching it explicitly
# means a future flag change cannot turn it into a phantom failure.
TALLY = re.compile(r"^%Error: Exiting due to \d+ warning")
ERROR = re.compile(r"^%Error")

# A detector that has not been shown to fire has not been tested. These run at
# import, so the patterns cannot rot into matching nothing -- the failure this
# repository calls a broken instrument, which reads as good news.
_SELFTEST = [
    ("%Warning-IMPLICIT: foo.sv:1:2: Signal definition not found, "
     "creating implicitly: 'clk'", True),
    ("%Error-MODMISSING: bar.sv:3:4: Cannot find file containing module: "
     "'zhao_nope'", True),
    ("%Warning-PINMISSING: baz.sv:5:6: Instance has missing pin: 'clk'", True),
    ("%Warning-UNUSEDSIGNAL: qux.sv:7:8: Signal is not used: 'spare'", False),
    ("%Warning-PINCONNECTEMPTY: q.sv:9:1: Cell pin is not connected: 'x_o'",
     False),
]
for _line, _want in _SELFTEST:
    if bool(STRUCTURAL.search(_line)) is not _want:
        raise SystemExit(
            "check_console_closure_lint: SELF-TEST FAILED -- the structural "
            "pattern no longer classifies a known sample correctly. Fix the "
            "pattern; do not delete the self-test.\n  sample: %s" % _line)


def closure_sources() -> list[str]:
    """The `zhao_console_core` target's declared sources, parsed exactly as
    `run_console_core_smoke.ps1` and `edgeclose-lint-core.ps1` parse them.
    Three readers of one list is already one too many, but diverging from them
    here would be worse."""
    srcs: list[str] = []
    in_target = False
    with open(FIT_TARGETS, encoding="utf-8") as fh:
        for line in fh:
            m = re.match(r"^\s*-\s*top:\s*(\S+)\s*$", line)
            if m:
                in_target = (m.group(1) == TOP)
                continue
            if in_target:
                m = re.match(r"^\s*-\s*(fpga/\S+\.sv)\s*$", line)
                if m:
                    srcs.append(m.group(1))
    return srcs


def main() -> int:
    srcs = closure_sources()
    if len(srcs) < 50:
        print("FAIL: fit_targets.yml gave only %d sources for %s -- the parse "
              "or the target moved." % (len(srcs), TOP))
        return 1

    missing = [s for s in srcs if not os.path.exists(os.path.join(REPO, s))]
    if missing:
        print("FAIL: %d declared source(s) do not exist on disk. A Quartus fit "
              "would discover this after it had already started:" % len(missing))
        for s in missing:
            print("    %s" % s)
        return 1

    if not os.path.exists(VERILATOR_BIN):
        print("CANNOT RUN: verilator_bin.exe not found at %s" % VERILATOR_BIN)
        print("  This gate returns NONZERO rather than passing, because a gate "
              "that reports success when it did not run is worse than no gate.")
        return 2

    env = dict(os.environ)
    env["VERILATOR_ROOT"] = VERILATOR_ROOT

    # The file list goes in a temp `-f` file, not on the command line: 286
    # paths is close enough to a Windows command length limit that a failure
    # there would look like a design problem and would not be one. `-f -` does
    # NOT mean stdin to Verilator -- it tries to open a file literally named
    # "-" -- which this gate reported as two hard errors the first time it ran.
    with tempfile.TemporaryDirectory() as td:
        listing = os.path.join(td, "closure.f")
        with open(listing, "w", encoding="utf-8") as fh:
            fh.write("\n".join(srcs) + "\n")
        cmd = [VERILATOR_BIN, "--lint-only", "-Wno-fatal",
               "--top-module", TOP, "-f", listing]
        proc = subprocess.run(cmd, env=env, cwd=REPO,
                              capture_output=True, text=True)
        out = (proc.stdout or "") + (proc.stderr or "")

    findings = [ln for ln in out.splitlines() if STRUCTURAL.search(ln)]
    hard = [ln for ln in out.splitlines()
            if ERROR.match(ln) and not TALLY.match(ln)]

    print("console closure lint: %d source(s), top %s; self-test fired %d/%d"
          % (len(srcs), TOP, len(_SELFTEST), len(_SELFTEST)))

    if not findings and not hard:
        print("OK -- the composed console elaborates with no implicit net, no "
              "missing module and no missing pin.")
        print("  This says NOTHING about synthesizability: Verilator accepts "
              "two SystemVerilog forms Quartus 17.0 rejects, and --lint-only "
              "does not run `initial` blocks, so elaboration $fatal guards are "
              "not exercised.")
        return 0

    for ln in findings:
        print("  FAIL: %s" % ln.strip())
    for ln in hard:
        print("  ERROR: %s" % ln.strip())
    print("\n%d structural finding(s). An IMPLICIT net is an undeclared "
          "identifier silently becoming a wire -- on a clock or reset that is "
          "a block that never runs, and every functional gate stays green."
          % (len(findings) + len(hard)))
    return 1


if __name__ == "__main__":
    sys.exit(main())
