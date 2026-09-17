"""Packet H's composed-fit rows must obey a receipt law, as G8B's do.

Packet I has `packet_i_g8b_registration_static`, which enforces the condition a
G8B row must satisfy the moment it stops being stamped failed. Packet H's
composed fit had no equivalent, so a `zhao_shell_top_v2` row could appear with
any provenance and be quoted.

WHAT THIS ENFORCES, and just as importantly what it does NOT.

Enforced on every `zhao_shell_top_v2` row:

  * the receipt fields exist at all;
  * `ioMode` and `virtualPins` agree -- a row calling itself physical-pin while
    reporting 5,358 virtual pins is describing two different measurements, and
    the G8B campaign switched pin modes mid-series, which is how a 43.54 ->
    57.87 MHz "improvement" turned out to be a boundary change.

Enforced only on a row stamped `ok`, because that is the one status in this
database that means a completed fit whose numbers passed the budget rules.
`map_only` is a partial measurement by construction -- Analysis & Synthesis
runs, the fitter does not -- and `incomplete:*` and `failed:*` are the fit
completing and the rules refusing it, which is a real measurement and not a
claim of acceptance:

  * clean tree at HEAD;
  * DSP within the ruled 85. This composition is a PART of the machine -- it
    carries no terrain -- so 85 is a NECESSARY condition rather than a
    sufficient one: if the part alone exceeds the whole's budget, the whole
    certainly does.

NOT ENFORCED: the 30,000-ALM target. It is a WHOLE-MACHINE number and this
composition is a part, so gating the part on it would be wrong in both
directions -- it would fail a legitimate part that happens to be large, and
pass a whole that is too big because its parts each fit. The ALM is recorded
and left for the roadmap to reason about against the terrain half.

ALSO NOT ENFORCED: that a virtual-pin row is the post-adoption composed fit R0
asks for. It is not, and `design/fit_targets.yml` says so at the target. A
checker cannot tell the difference between a row that was honest about its pin
mode and a reader who ignored it; the target comment and this docstring are
where that is said.
"""
import json
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
BLOCK_FIT = REPO / "reports" / "synthesis" / "zhao_block_fit.json"
TOP = "zhao_shell_top_v2"

# PROVENANCE, required of every row: these exist as soon as the runner has a
# tree to hash, before any tool runs.
REQUIRED_ROW_FIELDS = ("module", "status", "sourceCommit", "rtlCleanAtHead")

# PIN MODE, required only once a tool got far enough to report it. A row
# stamped `incomplete:*` died before that -- `@packet-h-m10k-map` is
# `incomplete:failed:quartus_map.exe`, seven seconds in on a Quartus syntax
# error -- and demanding `virtualPins` of it is holding a partial measurement
# to a complete row's standard.
#
# This gate made that exact mistake on its first run, which is the same
# category error Packet I's receipt law made on ITS first writing: three
# violations reported against a mapcheck row for things it never claimed. The
# lesson did not transfer by being written down once, so it is written here too.
PIN_FIELDS = ("ioMode", "virtualPins")

DSP_BUDGET = 85


def composed_rows(document: dict) -> list[dict]:
    """Every row whose module is the sibling shell or a label of it."""
    rows: list[dict] = []
    for block in document.get("blocks", []):
        for row in (block.get("rows") or block.get("runs") or [block]):
            module = str(row.get("module", ""))
            if module == TOP or module.startswith(TOP + "@"):
                rows.append(row)
    return rows


def receipt_violations(rows: list[dict]) -> list[str]:
    problems: list[str] = []
    for row in rows:
        name = row.get("module", "<unnamed>")
        for field in REQUIRED_ROW_FIELDS:
            if row.get(field) is None:
                problems.append(f"{name}: required receipt field {field} is absent")

        status = str(row.get("status", ""))
        io_mode = row.get("ioMode")
        virtual_pins = row.get("virtualPins")

        # An unknown ioMode is wrong whenever it appears -- the QSF writes it
        # before any tool runs, so a bad value is a bad value.
        if io_mode not in (None, "physical-top-ports", "virtual-top-ports"):
            problems.append(f"{name}: unknown ioMode {io_mode!r}")

        # Everything else about the pins waits until a tool COUNTED them. An
        # `incomplete:*` row carries the ioMode its QSF declared and no pin
        # count, because it died before producing one -- so requiring the field
        # AND checking it against the mode are both category errors on it.
        if not status.startswith("incomplete"):
            for field in PIN_FIELDS:
                if row.get(field) is None:
                    problems.append(
                        f"{name}: required receipt field {field} is absent")
            if io_mode == "physical-top-ports" and virtual_pins not in (None, 0):
                problems.append(
                    f"{name}: declares physical-top-ports while reporting "
                    f"{virtual_pins} virtual pin(s)")
            if io_mode == "virtual-top-ports" and virtual_pins in (None, 0):
                problems.append(
                    f"{name}: declares virtual-top-ports while reporting "
                    f"{virtual_pins} virtual pin(s)")

        if status != "ok":
            continue

        if row.get("rtlCleanAtHead") is not True:
            problems.append(f"{name}: accepted from a tree that is not clean at HEAD")
        dsp = row.get("dspBlocks")
        if isinstance(dsp, (int, float)) and dsp > DSP_BUDGET:
            problems.append(
                f"{name}: accepted with {dsp} DSP against the ruled "
                f"{DSP_BUDGET}; the part cannot exceed the whole's budget")
    return problems


class ComposedReceiptTests(unittest.TestCase):

    def test_a_composed_row_exists_at_all(self) -> None:
        rows = composed_rows(json.loads(BLOCK_FIT.read_text(encoding="utf-8")))
        self.assertGreaterEqual(
            len(rows), 1,
            "no zhao_shell_top_v2 row is committed; Packet H's area is an "
            "unmeasured claim and this gate has nothing to check")

    def test_committed_composed_rows_obey_the_receipt_law(self) -> None:
        rows = composed_rows(json.loads(BLOCK_FIT.read_text(encoding="utf-8")))
        problems = receipt_violations(rows)
        self.assertEqual(problems, [],
                         "composed receipt law violations:\n  " +
                         "\n  ".join(problems))

    def test_receipt_law_detectors_fire(self) -> None:
        """Each clause, broken on purpose, must be reported.

        A law that has only ever returned an empty list is a claim. These are
        synthetic rows, so firing them costs nothing and proves the clauses are
        wired to the fields they name.
        """
        base = {"module": f"{TOP}@probe", "status": "ok",
                "sourceCommit": "0" * 40, "rtlCleanAtHead": True,
                "ioMode": "virtual-top-ports", "virtualPins": 12,
                "dspBlocks": 63}

        self.assertEqual(receipt_violations([dict(base)]), [],
                         "the baseline synthetic row must be clean, or every "
                         "case below passes for the wrong reason")

        dirty = dict(base, rtlCleanAtHead=False)
        self.assertTrue(any("not clean at HEAD" in p
                            for p in receipt_violations([dirty])))

        over = dict(base, dspBlocks=DSP_BUDGET + 1)
        self.assertTrue(any("against the ruled" in p
                            for p in receipt_violations([over])))

        mismatched = dict(base, ioMode="physical-top-ports", virtualPins=5358)
        self.assertTrue(any("while reporting" in p
                            for p in receipt_violations([mismatched])))

        absent = dict(base)
        del absent["sourceCommit"]
        self.assertTrue(any("sourceCommit is absent" in p
                            for p in receipt_violations([absent])))

        # A row that is NOT stamped ok carries no acceptance condition: a dirty
        # map-only row is an honest partial measurement, and reporting it as a
        # violation is the category error Packet I's gate made on its first
        # writing.
        partial = dict(base, status="map_only", rtlCleanAtHead=False)
        self.assertEqual(
            [p for p in receipt_violations([partial]) if "clean at HEAD" in p],
            [], "a non-ok row was held to the acceptance condition")


if __name__ == "__main__":
    unittest.main(verbosity=2)
