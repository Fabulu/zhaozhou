#!/usr/bin/env python3
"""Exhaustive equivalence proof for the material combiner's short finish forms.

WHY THIS EXISTS AS A COMMITTED PROBE, not a paragraph in a commit message.

`finish_lane` was rewritten from wide biased/signed arithmetic into byte-wide
carry chains to shorten the M->F cone. That rewrite is only safe if it computes
the SAME function over the whole raw 16-bit product domain -- including products
no pair of legal byte operands can produce, because a mutant, a future recipe or
a retimed operand path can present one. The block's own differential cannot
reach that domain: it drives legal texels and compares colours.

So this file carries BOTH expressions and compares them over every input:
65,536 products for UNIT and MOD2, and 33,554,432 product/base/sign combinations
for LERP. It is a mathematical claim about two formulas, deliberately with no
DUT, which is what keeps it valid while the RTL around it moves.

The reference side is a transcription of the PREVIOUS RTL, widths and slice
boundaries included. Do not "simplify" it to match the new form; that would turn
the proof into a tautology -- the failure this repository calls a detector wired
to two operands that move together.
"""

from __future__ import annotations

from pathlib import Path
import re
import unittest


REPO = Path(__file__).resolve().parents[2]
COMBINER = REPO / "fpga/rtl/texture/zhao_texture_material_combine_v3.sv"

MOD2_SATURATION_THRESHOLD = 32704


# --------------------------------------------------------------------------
# Reference: the superseded long form, transcribed from the RTL it replaced.
# --------------------------------------------------------------------------
def reference_unit(product: int) -> int:
    rounded = (product + 128) & 0x1FFFF          # 17-bit sum
    return (rounded >> 8) & 0xFF                 # rounded[15:8]


def reference_mod2(product: int) -> tuple[int, int]:
    rounded = ((product + 64) & 0x1FFFF) >> 7
    if rounded > 255:
        return (1, 0xFF)
    return (0, rounded & 0xFF)


def reference_lerp(product: int, base: int, negative: bool) -> int:
    signed_num = -product if negative else product
    signed_num += 128
    value = signed_num & 0x3FFFF                 # 18-bit signed container
    delta = (value >> 8) & 0x3FF                 # signed_num[17:8]
    if delta & 0x200:
        delta -= 0x400
    lerped = base + delta
    if lerped < 0:
        return 0
    if lerped > 255:
        return 0xFF
    return lerped & 0xFF


# --------------------------------------------------------------------------
# Candidate: the short form now in the RTL.
# --------------------------------------------------------------------------
def short_unit(product: int) -> int:
    high, low = (product >> 8) & 0xFF, product & 0xFF
    return (high + (1 if low >= 128 else 0)) & 0xFF


def short_mod2(product: int) -> tuple[int, int]:
    if product >= MOD2_SATURATION_THRESHOLD:
        return (1, 0xFF)
    total = (product >> 7) + (1 if (product & 127) >= 64 else 0)
    return (0, total & 0xFF)


def short_lerp(product: int, base: int, negative: bool) -> int:
    high, low = (product >> 8) & 0xFF, product & 0xFF
    if negative:
        total = base + ((~high) & 0xFF) + (1 if low <= 128 else 0)
        return (total & 0xFF) if (total >> 8) & 1 else 0
    total = base + high + (1 if low >= 128 else 0)
    return 255 if (total >> 8) & 1 else total & 0xFF


class MaterialFinishEquivalenceTests(unittest.TestCase):
    def test_unit_and_mod2_are_exhaustively_identical(self) -> None:
        for product in range(1 << 16):
            self.assertEqual(reference_unit(product), short_unit(product),
                             f"UNIT differs at product={product}")
            self.assertEqual(reference_mod2(product), short_mod2(product),
                             f"MOD2 differs at product={product}")

    def test_lerp_is_exhaustively_identical_on_both_halves(self) -> None:
        checked = 0
        for product in range(1 << 16):
            high, low = (product >> 8) & 0xFF, product & 0xFF
            positive_carry = 1 if low >= 128 else 0
            negative_carry = 1 if low <= 128 else 0
            complement = (~high) & 0xFF
            for base in range(256):
                total = base + high + positive_carry
                got = 255 if (total >> 8) & 1 else total & 0xFF
                if got != reference_lerp(product, base, False):
                    self.fail(f"LERP+ differs at product={product} base={base}")
                total = base + complement + negative_carry
                got = (total & 0xFF) if (total >> 8) & 1 else 0
                if got != reference_lerp(product, base, True):
                    self.fail(f"LERP- differs at product={product} base={base}")
                checked += 2
        self.assertEqual(checked, 33554432)

    def test_the_comparison_can_fail(self) -> None:
        # The proof above is only worth its runtime if a wrong short form is
        # actually rejected. Each of these is a plausible mistake.
        def rounded_down_unit(product: int) -> int:
            return (product >> 8) & 0xFF                     # forgets the tie

        def wrong_threshold_mod2(product: int) -> tuple[int, int]:
            if product >= 32640:                             # the tempting 255*128
                return (1, 0xFF)
            total = (product >> 7) + (1 if (product & 127) >= 64 else 0)
            return (0, total & 0xFF)

        def symmetric_negative_lerp(product: int, base: int) -> int:
            high, low = (product >> 8) & 0xFF, product & 0xFF
            total = base + ((~high) & 0xFF) + (1 if low < 128 else 0)
            return (total & 0xFF) if (total >> 8) & 1 else 0

        self.assertTrue(
            any(rounded_down_unit(p) != reference_unit(p) for p in range(1 << 16)))
        self.assertTrue(
            any(wrong_threshold_mod2(p) != reference_mod2(p) for p in range(1 << 16)))
        self.assertTrue(
            any(symmetric_negative_lerp(p, a) != reference_lerp(p, a, True)
                for p in range(1 << 16) for a in (0, 1, 128, 255)),
            "the negative tie must be at low <= 128, not low < 128",
        )

    def test_rtl_uses_the_proven_form(self) -> None:
        text = COMBINER.read_text(encoding="utf-8")
        for marker in (
            "round_up = low[7];",
            "no_borrow_carry = (low <= 8'd128);",
            "finish_lane[7:0] = high + {7'd0, round_up};",
            "if (product >= 16'd32704) begin",
            "lerp_sum = {1'b0, lerp_base} + {1'b0, ~high} +",
            "lerp_sum = {1'b0, lerp_base} + {1'b0, high} + {8'd0, round_up};",
            "finish_lane[7:0] = lerp_sum[8] ? lerp_sum[7:0] : 8'd0;",
            "finish_lane[7:0] = lerp_sum[8] ? 8'hFF : lerp_sum[7:0];",
        ):
            self.assertEqual(text.count(marker), 1, marker)
        # The superseded wide arithmetic must be gone, not merely bypassed.
        for forbidden in (
            "signed_num = $signed({2'b00, product});",
            "rounded = {1'b0, product} + 17'd128;",
            "delta = signed_num[17:8];",
        ):
            self.assertNotIn(forbidden, text)
        self.assertIsNotNone(
            re.search(r"m_p0 <= o_a0 \* o_b0;", text),
            "the two product sites must be untouched by a finish rewrite",
        )


if __name__ == "__main__":
    unittest.main()
