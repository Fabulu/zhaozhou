"""Positive controls for the source_list_parity gate.

WHY THIS EXISTS. `source_list_parity.cmake.in` guards the one rule that a green
Verilator suite cannot: that Quartus and Verilator are handed the same design.
On 2026-09-16 it was RED, and not because the lists had drifted -- because the
shell fit gained a generated Quartus-only top (`zhao_shell_fit_top`) that the
Verilator lane must never see. Teaching the gate about that asymmetry means
teaching it to IGNORE something, and an exclusion is exactly where a gate goes
quietly blind.

So the exclusion is checked rather than granted, and this file is the evidence
that the checking works. Each case below is a fault the gate MUST reject; the
script is scored on the gate FAILING, per CLAUDE.md's "a detector that has not
been shown to fire has not been tested".

Two of these controls were themselves wrong on first writing, and both failures
are worth keeping in mind because both read as "the gate missed it":

  * the first "drop a module from the QSF" control dropped `zhao_abi_pkg` --
    the ONE sanctioned asymmetry, whose absence is legal. The gate passed
    correctly and the control called it a miss. A control aimed at the exempt
    case measures the exemption, not the rule.
  * the "wrapper is verilated too" control looked for its message with a plain
    substring search. CMake hard-wraps message() text, the needle spanned the
    wrap, and a gate that had fired with exactly the right diagnosis was scored
    as silent.

Both errors point the same way -- towards believing the gate is broken -- which
is the safe direction, but only by luck. Run with no arguments; exits non-zero
if any control fails to behave.
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TEMPLATE = REPO / "tests/lint/source_list_parity.cmake.in"
QSF_PATH = REPO / "fpga/quartus/shell_fit/zhao_shell_fit.qsf"
CML_PATH = REPO / "tests/CMakeLists.txt"

QSF = QSF_PATH.read_text(encoding="utf-8")
CML = CML_PATH.read_text(encoding="utf-8")
TMPL = TEMPLATE.read_text(encoding="utf-8")


def _run(work: Path, name: str, qsf_text: str, cml_text: str):
    """Run the gate against substitute lists, never the real ones."""
    d = work / name
    d.mkdir(parents=True, exist_ok=True)
    (d / "q.qsf").write_text(qsf_text, encoding="utf-8")
    (d / "c.txt").write_text(cml_text, encoding="utf-8")

    script = TMPL.replace("@CMAKE_SOURCE_DIR@", str(REPO))
    script = script.replace(
        'set(_qsf "%s/fpga/quartus/shell_fit/zhao_shell_fit.qsf")' % REPO,
        'set(_qsf "%s/q.qsf")' % d.as_posix(),
    )
    script = script.replace(
        'set(_cml "%s/tests/CMakeLists.txt")' % REPO,
        'set(_cml "%s/c.txt")' % d.as_posix(),
    )
    # If the redirect silently failed, every control below would measure the
    # REAL, correct lists and report a clean sweep of misses -- or, worse, a
    # clean sweep of passes on a gate that was never invoked.
    if "/q.qsf" not in script or "/c.txt" not in script:
        raise SystemExit(
            "fire_source_list_parity: could not redirect the gate at its own "
            "input paths. The template's set(_qsf ...)/set(_cml ...) lines "
            "have changed shape; this harness would otherwise have measured "
            "the real files and reported nothing.")

    s = d / "s.cmake"
    s.write_text(script, encoding="utf-8")
    r = subprocess.run(["cmake", "-P", str(s)], capture_output=True, text=True)
    return r.returncode, " ".join((r.stdout + r.stderr).split())


def main() -> int:
    results: list[bool] = []

    with tempfile.TemporaryDirectory(prefix="fire_slp_") as tmp:
        work = Path(tmp)

        def expect_fail(name: str, qsf_text: str, cml_text: str, needle: str):
            if qsf_text == QSF and cml_text == CML:
                print(f"  [NO-OP]  {name}: mutation changed nothing -- harness bug")
                results.append(False)
                return
            rc, out = _run(work, name, qsf_text, cml_text)
            ok = rc != 0 and " ".join(needle.split()).lower() in out.lower()
            print(f"  [{'FIRED' if ok else 'MISSED'}]  {name}")
            if not ok:
                print(f"     wanted: {needle}")
                print(f"     got:    {out[:400]}")
            results.append(ok)

        def expect_pass(name: str, qsf_text: str, cml_text: str):
            rc, out = _run(work, name, qsf_text, cml_text)
            print(f"  [{'PASS ' if rc == 0 else 'FALSE ALARM'}]  {name}")
            if rc != 0:
                print(f"     got:    {out[:400]}")
            results.append(rc == 0)

        print("source_list_parity positive controls")

        # The negative control comes first. Without it a gate that rejects
        # EVERYTHING scores six FIREDs and looks perfect.
        expect_pass("unmutated", QSF, CML)

        qlines = QSF.splitlines(keepends=True)
        srcs = [i for i, l in enumerate(qlines)
                if "SYSTEMVERILOG_FILE" in l and "rtl/" in l]

        # 1. a module in CMake but not the QSF -- the DEBUG.FRAMEBLIT defect
        #    this gate was written for. Skip the two legal asymmetries.
        victim = next(i for i in srcs if "zhao_abi_pkg" not in qlines[i]
                      and "zhao_shell_fit_top" not in qlines[i])
        expect_fail("module_dropped_from_qsf",
                    "".join(qlines[:victim] + qlines[victim + 1:]),
                    CML, "DISAGREE")

        # 2. the generated wrapper is no longer last: Quartus needs leaves
        #    before the top, and shell_fit_qsf.py appends it last.
        wi = next(i for i in srcs if "zhao_shell_fit_top.sv" in qlines[i])
        si = next(i for i in srcs if "zhao_shell_top.sv" in qlines[i]
                  and "fit" not in qlines[i])
        sw = qlines[:]
        sw[wi], sw[si] = sw[si], sw[wi]
        expect_fail("wrapper_not_last", "".join(sw), CML,
                    "LAST SystemVerilog file")

        # 3. a HAND-WRITTEN top claiming the exemption. Only a generated
        #    wrapper may skip the Verilator lane.
        expect_fail("top_not_generated",
                    QSF.replace("rtl/generated/zhao_shell_fit_top.sv",
                                "rtl/common/zhao_shell_fit_top.sv"),
                    CML, "not under")

        # 4. the wrapper verilated as well -- the shell would elaborate twice.
        blk = CML.index("set(ZHAO_SHELL_RTL")
        end = CML.index(")", blk)          # closes on the zhao_shell_top line
        bol = CML.rindex("\n", blk, end) + 1
        expect_fail(
            "wrapper_in_cmake", QSF,
            CML[:bol]
            + "    ${CMAKE_SOURCE_DIR}/fpga/rtl/generated/zhao_shell_fit_top.sv\n"
            + CML[bol:],
            "appears in")

        # 5. an ordering swap among ordinary modules. Green here and fatal in
        #    run_shell_fit.ps1 until 2026-08-22; the exemptions above must not
        #    have reopened that hole.
        o = qlines[:]
        a, b = srcs[3], srcs[4]
        o[a], o[b] = o[b], o[a]
        expect_fail("order_swap", "".join(o), CML, "DIFFERENT ORDER")

    print(f"\n{sum(results)}/{len(results)} controls behaved as required")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
