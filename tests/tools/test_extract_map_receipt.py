#!/usr/bin/env python3
"""Controls for `tools/quartus/extract_map_receipt.py`'s RAM-Summary anchor.

WHY THIS FILE EXISTS
--------------------
Packet LANESCOST, 2026-09-27. Four perfectly good `-MapOnly` reports of
`zhao_field_host_v2` could not be turned into a receipt, because the tool's
self-check demanded that some RAM row name `M10K`/`MLAB`/`M20K`/`M9K`/`LUTRAM`
and every row in those reports carries the literal Type `AUTO`.

`AUTO` is what Analysis & Synthesis writes when it has not yet chosen a
primitive; the FITTER chooses later. So the check refused the commonest output
of the ONLY run type this tool exists to read -- `extract_map_receipt.py`'s own
docstring says the RAM summary is "the memory question, which is the one a
`-MapOnly` run is for".

The direction is worth recording. This is NOT the broken-instrument law's usual
shape: the alarm was LOUD and it cost work rather than manufacturing a false
green. What it got wrong was its EXPLANATION -- "the anchor slipped" -- which is
this repo's chapter 5, *a wrong diagnosis attached to a right alarm sends the
next person to reshape something that is already correct*. The header regex was
never at fault.

WHAT THESE TESTS ASSERT, AND WHAT THEY DELIBERATELY DO NOT
----------------------------------------------------------
They assert the CORRECT behaviour on both sides, never the defect:

  * an `AUTO`-typed RAM table is ACCEPTED and its rows are carried through;
  * an `M10K block`-typed table is still accepted (no regression);
  * the anchor guard STILL FIRES on the failure it was written for -- the
    original defect collected `; Legal Notice ;` out of the table of CONTENTS.
    That is the positive control, and without it the widened token list would
    be an untested claim that the check can still catch anything at all.
"""

from __future__ import annotations

import io
from pathlib import Path
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
QUARTUS_TOOLS = REPO / "tools" / "quartus"
if str(QUARTUS_TOOLS) not in sys.path:
    sys.path.insert(0, str(QUARTUS_TOOLS))

import extract_map_receipt as emr  # noqa: E402


HEADER = (
    "; Name                                  ; Type       ; Mode              ; "
    "Port A Depth ; Port A Width ; Port B Depth ; Port B Width ; Bits  ; MIF ;"
)
RULE = "+" + "-" * 120 + "+"


def _report(rows, *, alut=42789, alm=39252, regs=48997, bits=85282, dsp=15):
    """A minimal `.map.rpt` carrying the fields the extractor reads."""
    head = [
        "; Quartus Prime Version           ; 17.0.2 Build 602 07/19/2017 SJ Lite Edition ;",
        "; Top-level Entity Name           ; zhao_field_host_v2                          ;",
        "; Family                          ; Cyclone V                                   ;",
        "; Device                          ; 5CSEBA6U23I7                                ;",
        "; Total registers                 ; %d                                          ;" % regs,
        "; Total virtual pins              ; 3,431                                       ;",
        "; Total block memory bits         ; %d                                          ;" % bits,
        "; Total DSP Blocks                ; %d                                          ;" % dsp,
        "; Estimate of Logic utilization (ALMs needed) ; %d ;" % alm,
        "; Combinational ALUT usage for logic          ; %d ;" % alut,
        "",
        "; Analysis & Synthesis RAM Summary ;",
        RULE,
        HEADER,
        RULE,
    ]
    return "\n".join(head + list(rows) + [RULE, ""])


def _parse(text):
    """`emr.parse` takes a PATH, so the fixture has to land on disk."""
    with tempfile.TemporaryDirectory() as d:
        p = Path(d) / "blockfit.map.rpt"
        with io.open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        return emr.parse(str(p))


# A real row out of `zhao_field_host_v2@map-L1G1F1.map.rpt`, Type AUTO.
AUTO_ROW = (
    "; zhao_field_v3_engine:u_fabric|zhao_field_v3_core:u_engine|"
    "zhao_field_v3_exec:u_exec|altsyncram:lq_imm_r_rtl_0|"
    "altsyncram_5vj1:auto_generated|ALTSYNCRAM ; AUTO ; Simple Dual Port ; "
    "4 ; 32 ; 4 ; 32 ; 128 ; None ;"
)

# A real row out of the committed EARTHRAM receipt, Type already decided.
M10K_ROW = (
    "; altsyncram:b_uni_rtl_0|altsyncram_70s1:auto_generated|ALTSYNCRAM ; "
    "M10K block ; Simple Dual Port ; 16 ; 305 ; 16 ; 305 ; 4880 ; None ;"
)


class AutoTypedRamSummary(unittest.TestCase):
    """The repair: an A&S table that has not chosen a primitive is still a table."""

    def test_auto_typed_rows_are_accepted(self):
        out = _parse(_report([AUTO_ROW]))
        self.assertTrue(out["ramSummaryPresent"])
        self.assertEqual(len(out["ramSummary"]), 1)
        self.assertIn("ALTSYNCRAM", out["ramSummary"][0])

    def test_the_numbers_an_area_argument_is_made_of_survive(self):
        # The two fields the `.map.summary` does NOT carry, which is this
        # tool's entire reason to exist.
        out = _parse(_report([AUTO_ROW]))
        self.assertEqual(out["combinationalALUTs"], 42789)
        self.assertEqual(out["estimatedALMs"], 39252)
        self.assertEqual(out["dedicatedRegisters"], 48997)
        self.assertEqual(out["blockMemoryBits"], 85282)
        self.assertEqual(out["dspBlocks"], 15)
        self.assertEqual(out["device"], "5CSEBA6U23I7")

    def test_placed_alms_stay_null_after_a_map(self):
        # A map does not place. Writing the estimate here is the conflation
        # OWNER_VACATION_DIRECTIVE section 8 forbids.
        out = _parse(_report([AUTO_ROW]))
        self.assertIsNone(out["placedALMs"])

    def test_decided_primitive_still_accepted(self):
        out = _parse(_report([M10K_ROW]))
        self.assertTrue(out["ramSummaryPresent"])
        self.assertIn("M10K", out["ramSummary"][0])


class AnchorGuardPositiveControl(unittest.TestCase):
    """THE GUARD IS SEEN TO FIRE, on the exact defect it was written for.

    The first version of the extractor anchored on the string "RAM Summary",
    matched the table of CONTENTS twenty lines into the file, and emitted
    `; Legal Notice ;` as the packing. Widening the accepted tokens to admit
    `AUTO` rows would be worthless if it also admitted that.
    """

    def test_table_of_contents_junk_still_raises(self):
        junk = [
            "; Legal Notice ;",
            "; Table of Contents ;",
            "; Flow Summary ;",
        ]
        with self.assertRaises(SystemExit) as caught:
            _parse(_report(junk))
        self.assertIn("not RAM rows", str(caught.exception))

    def test_the_guard_is_not_vacuous_for_the_reason_it_passes(self):
        # Belt and braces against a control that "passes" because the parser
        # never found a table at all: the junk report MUST present a table.
        out = _parse(_report([AUTO_ROW]))
        self.assertTrue(out["ramSummaryPresent"],
                        "the fixture must actually produce a RAM table, or the "
                        "SystemExit above would prove nothing")


if __name__ == "__main__":
    unittest.main()
