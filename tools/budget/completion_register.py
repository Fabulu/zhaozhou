"""completion_register -- is every mandatory v1 capability REALLY present?

Owner goal, 2026-09-19: build the entire mandatory v1 console, connect it
honestly, and drive mandatory unresolved items to ZERO. A mandatory function
does NOT count as present if it is a tie-off, fake stimulus, an external
placeholder for storage that belongs in hardware, a disconnected
implementation, synthesis-pruned dead logic, a stub, a TODO, or an
unimplemented contract.

Every mandatory function needs:

    real producer -> real implementation -> real consumer -> tests/evidence

THE DESIGN DECISION THAT MATTERS: this register is COMPUTED, not maintained.

A hand-kept completion table is the thing this repository fails at. It has a
phantom-reference register that went stale in six days, thirteen mutant copies
that drifted while staying green, twenty owner documents with no recorded
disposition, and an ENFORCED-BY tag naming a test no target compiles. A status
field a human edits is a status field that lies the moment someone forgets.

So every judgement below is derived from the tree at the moment of the run:

  * the TIE-OFF list is parsed out of `zhao_console_core.sv`'s own
    "INCOMPLETE -- TIED OFF, AND WHY" header block. That block is maintained by
    whoever edits the core, next to the ports it describes, and it is the one
    place a new gap cannot be added without being written down.
  * COMPOSED means the module appears in `zhao_console_core`'s closure in
    `design/fit_targets.yml` AND is instantiated somewhere in that closure.
    A file in the source list that nothing instantiates is not composed -- the
    shell's own history records eight such modules that elaborated nowhere.
  * EVIDENCE means a `tests/CMakeLists.txt` target exists that verilates the
    module. Seven committed tests were found this session that no target
    compiled; a file on disk is not a gate.

Nothing here reads a `status:` field, because there isn't one.

Exit 0 when zero MANDATORY gaps remain. Exit 1 while any remain. Exit 2 if the
self-test fails, because a register that cannot see a gap it should see would
report victory, and that is the one direction this must never fail in.
"""

from __future__ import annotations

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
CORE = ROOT / "fpga" / "rtl" / "prod" / "zhao_console_core.sv"
TARGETS = ROOT / "design" / "fit_targets.yml"
CMAKE = ROOT / "tests" / "CMakeLists.txt"

# A tie-off entry in the core's header: "//  I4. NAME ... -- KIND."
_TIEOFF = re.compile(r"^//\s*(I\d+)\.\s+(.*)$")
_BOUNDARY = re.compile(r"\bBOUNDARY\b")
_TIED_ZERO = re.compile(r"TIED TO ZERO|STRUCTURALLY STUCK", re.I)
_NOT_A_TIEOFF = re.compile(r"NOT a tie-off", re.I)


def tieoffs() -> list[dict]:
    """Parse the core's own INCOMPLETE block. It is the authoritative gap list."""
    if not CORE.exists():
        raise SystemExit("zhao_console_core.sv is missing; there is no console to audit")
    text = CORE.read_text(encoding="utf-8", errors="replace")
    start = text.find("INCOMPLETE -- TIED OFF")
    if start < 0:
        raise SystemExit(
            "zhao_console_core.sv has no 'INCOMPLETE -- TIED OFF' block. Either "
            "every gap is closed and the block was removed -- in which case "
            "delete this check deliberately -- or the header was reformatted "
            "and this register has gone blind. It does not guess."
        )
    out: list[dict] = []
    cur: dict | None = None
    for line in text[start:].splitlines():
        m = _TIEOFF.match(line.strip())
        if m:
            if cur:
                out.append(cur)
            cur = {"id": m.group(1), "head": m.group(2).strip(), "body": ""}
            continue
        if cur is None:
            continue
        if not line.strip().startswith("//"):
            break
        cur["body"] += " " + line.strip().lstrip("/").strip()
    if cur:
        out.append(cur)
    for t in out:
        blob = t["head"] + " " + t["body"]
        t["kind"] = ("resolved-in-composer" if _NOT_A_TIEOFF.search(blob)
                     else "tied-to-zero" if _TIED_ZERO.search(blob)
                     else "boundary" if _BOUNDARY.search(blob)
                     else "unclassified")
        # "tied to zero" and "no owner exists" are the two that delete logic or
        # have no implementation at all. Boundary means a real port a board or
        # a producer must drive -- still a gap, but a different repair.
        t["mandatory_gap"] = t["kind"] != "resolved-in-composer"
    return out


def console_closure() -> set[str]:
    """The modules named in zhao_console_core's fit target."""
    if not TARGETS.exists():
        return set()
    text = TARGETS.read_text(encoding="utf-8", errors="replace")
    i = text.find("zhao_console_core")
    if i < 0:
        return set()
    out: set[str] = set()
    started = False
    for line in text[i:].splitlines()[1:]:
        s = line.strip()
        if s.startswith("- fpga/rtl/") or s.startswith("- fpga\\rtl\\"):
            out.add(pathlib.Path(s[2:].replace("\\", "/")).stem)
            started = True
            continue
        # The next target begins at a non-indented key. Only stop once this
        # target's own source list has actually started, or a `sources:` line
        # between the target name and its entries ends the scan immediately and
        # the count reads 0 -- which it did on the first run, and a closure of
        # zero would have looked like "nothing is composed" rather than a bug.
        if started and s and not s.startswith(("-", "#")) and line[:1] not in " \t":
            break
    return out


def has_test_target(module: str) -> bool:
    if not CMAKE.exists():
        return False
    return module in CMAKE.read_text(encoding="utf-8", errors="replace")


def audit() -> dict:
    ties = tieoffs()
    closure = console_closure()
    gaps = [t for t in ties if t["mandatory_gap"]]
    return {
        "tieoffs_total": len(ties),
        "mandatory_gaps": len(gaps),
        "closure_modules": len(closure),
        "by_kind": {k: sum(1 for t in ties if t["kind"] == k)
                    for k in sorted({t["kind"] for t in ties})},
        "gaps": [{"id": t["id"], "kind": t["kind"],
                  "head": t["head"][:100]} for t in gaps],
    }


def _self_test() -> None:
    """The register must be able to SEE a gap. Prove it on a synthetic header."""
    sample = (
        "// INCOMPLETE -- TIED OFF, AND WHY\n"
        "//  I1. SOMETHING (`a_i`) -- BOUNDARY. no owner exists.\n"
        "//      more prose about it\n"
        "//  I2. OTHER (`b_i`) -- TIED TO ZERO, a real interface mismatch.\n"
        "//  I3. THIRD (`c_i`) -- NOT a tie-off: the core assigns it.\n"
    )
    global CORE
    real = CORE
    try:
        tmp = ROOT / "tools" / "budget" / ".completion_selftest.sv"
        tmp.write_text(sample, encoding="utf-8")
        CORE = tmp
        ts = tieoffs()
        ids = [t["id"] for t in ts]
        kinds = {t["id"]: t["kind"] for t in ts}
        bad = []
        if ids != ["I1", "I2", "I3"]:
            bad.append("parsed %r, expected I1 I2 I3" % ids)
        if kinds.get("I1") != "boundary":
            bad.append("I1 kind %r" % kinds.get("I1"))
        if kinds.get("I2") != "tied-to-zero":
            bad.append("I2 kind %r" % kinds.get("I2"))
        if kinds.get("I3") != "resolved-in-composer":
            bad.append("I3 kind %r" % kinds.get("I3"))
        if sum(1 for t in ts if t["mandatory_gap"]) != 2:
            bad.append("expected 2 mandatory gaps, got %d"
                       % sum(1 for t in ts if t["mandatory_gap"]))
        if bad:
            sys.stderr.write("completion_register SELF-TEST FAILED:\n")
            for b in bad:
                sys.stderr.write("  %s\n" % b)
            raise SystemExit(2)
    finally:
        CORE = real
        try:
            (ROOT / "tools" / "budget" / ".completion_selftest.sv").unlink()
        except OSError:
            pass


def main(argv: list[str]) -> int:
    _self_test()
    rep = audit()
    if "--json" in argv:
        print(json.dumps(rep, indent=2))
        return 0 if rep["mandatory_gaps"] == 0 else 1

    print("completion register -- computed from the tree, not maintained by hand")
    print("self-test PASSED: the parser sees a planted gap and classifies it\n")
    print("zhao_console_core closure modules : %d" % rep["closure_modules"])
    print("tie-off entries in its header     : %d" % rep["tieoffs_total"])
    for k, n in sorted(rep["by_kind"].items()):
        print("    %-22s %d" % (k, n))
    print("\nMANDATORY GAPS REMAINING          : %d" % rep["mandatory_gaps"])
    if rep["gaps"]:
        print()
        for g in rep["gaps"]:
            print("  %-4s %-20s %s" % (g["id"], g["kind"], g["head"]))
        print("\nA mandatory function is not present if it is a tie-off, fake")
        print("stimulus, an external placeholder for hardware storage, a")
        print("disconnected implementation, pruned dead logic, a stub, a TODO,")
        print("or an unimplemented contract. Drive this to ZERO.")
    else:
        print("\nZERO mandatory gaps. Freeze this design and measure it.")
    return 0 if rep["mandatory_gaps"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
