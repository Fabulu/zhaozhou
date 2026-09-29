#!/usr/bin/env python3
"""Arithmetic checks only: PLAN=48 address layouts, not RTL or synthesis tests."""
from __future__ import annotations
import json
from pathlib import Path

CTX, PLAN, WIDTH = 8, 48, 60


def packed(ctx: int, pc: int) -> int:
    if not (0 <= ctx < CTX and 0 <= pc < PLAN):
        raise ValueError("Outside the legal context/program-index domain")
    return ctx * PLAN + pc


def padded(ctx: int, pc: int) -> int:
    if not (0 <= ctx < CTX and 0 <= pc < PLAN):
        raise ValueError("Padding is not an additional legal instruction capacity")
    return (ctx << 6) | pc


def main() -> None:
    pairs = [(c, p) for c in range(CTX) for p in range(PLAN)]
    linear = [packed(*x) for x in pairs]
    physical = [padded(*x) for x in pairs]
    assert len(pairs) == 384
    assert sorted(linear) == list(range(384))
    assert len(set(physical)) == 384 and max(physical) == 495
    assert all(packed(c, p) == (c << 5) + (c << 4) + p for c, p in pairs)
    assert all((a >> 6, a & 63) == pair for a, pair in zip(physical, pairs))

    # Deliberate negative control: apply concatenation to the old 384-word map.
    wrong_linear_addresses = sum(a != b for a, b in zip(linear, physical))
    outside_384 = sum(a >= 384 for a in physical)
    assert wrong_linear_addresses == 336
    assert outside_384 == 96

    # Deliberate negative control: keep the probe's five-bit PC interface.
    old_pc_addresses = [(c << 5) | (p & 31) for c, p in pairs]
    assert len(set(old_pc_addresses)) == 256  # 128 logical entries alias.

    # Check a legal-data permutation between the two explicit layouts.
    a = [None] * 384
    b = [None] * 512
    for c, p in pairs:
        datum = ((packed(c, p) + 1) * 0x123456789ABCDEF) & ((1 << WIDTH) - 1)
        a[packed(c, p)] = datum
        b[padded(c, p)] = datum
    assert all(a[packed(c, p)] == b[padded(c, p)] for c, p in reversed(pairs))

    rejected = 0
    for c in range(CTX):
        for p in range(48, 64):
            try:
                padded(c, p)
            except ValueError:
                rejected += 1
            else:
                raise AssertionError("Padding accidentally became legal")
    assert rejected == 128

    result = {
        "scope": "Arithmetic address checks; no production RTL, simulator or Quartus execution",
        "configuration": {"CTX": CTX, "PLAN": PLAN, "uop_bits": WIDTH},
        "legal_entries": len(pairs),
        "packed_layout_entries": 384,
        "packed_address_bits": 9,
        "padded_layout_entries": 512,
        "padded_max_legal_address": max(physical),
        "padded_invalid_pc_encodings_rejected": rejected,
        "blind_concatenation_changed_addresses": wrong_linear_addresses,
        "blind_concatenation_outside_384_word_array": outside_384,
        "five_bit_pc_unique_addresses": len(set(old_pc_addresses)),
        "valid_payload_layout_equivalence": "384/384",
        "result": "PASS, including deliberately wrong address controls",
    }
    print(json.dumps(result, indent=2))
    Path(__file__).with_name("address_check_results.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
