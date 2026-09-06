#!/usr/bin/env python3
"""Independent arithmetic checks for the proposed architecture; NOT RTL tests.
Only Python's standard library is required. No repository files are modified.
"""
from __future__ import annotations
import json
import random
from pathlib import Path

M32 = (1 << 32) - 1
M64 = (1 << 64) - 1
SEED = 0x5A48414F
RANDOM_CASES = 250_000

def tiled_u32(a: int, b: int) -> int:
    """Four 16-bit products and carry-explicit recombination."""
    a0, a1 = a & 65535, a >> 16
    b0, b1 = b & 65535, b >> 16
    p00, p01, p10, p11 = a0*b0, a0*b1, a1*b0, a1*b1
    cross = p01 + p10
    low_sum = p00 + ((cross & 65535) << 16)
    high = p11 + (cross >> 16) + (low_sum >> 32)
    return ((high & M32) << 32) | (low_sum & M32)

def mx_original(x: int, w: int) -> tuple[int, int]:
    t64 = ((1 << 31) - w) & M64
    p = (x * t64) & M64
    return p, (((p + (1 << 29)) & M64) >> 30) & M32

def mx_rewrite(x: int, w: int) -> tuple[int, int]:
    b = ((1 << 31) - w) & M32
    p = tiled_u32(x, b)
    hi = ((p >> 32) - (x if w > (1 << 31) else 0)) & M32
    corrected = (hi << 32) | (p & M32)
    iterate = (((corrected >> 30) & M32) + ((corrected >> 29) & 1)) & M32
    return corrected, iterate

def perspective_tiled(n: int, mant: int) -> int:
    sign = -1 if n < 0 else 1
    return sign * tiled_u32(abs(n), mant)

def saturate_i32(x: int) -> int:
    return max(-(1 << 31), min((1 << 31) - 1, x))

def main() -> None:
    if not __debug__:
        raise RuntimeError("Run without -O/-OO: these checks use assertions.")
    rng = random.Random(SEED)
    boundary = [0, 1, 2, 63, 64, 127, 128, 65535, 65536,
                (1 << 29)-1, 1 << 29, (1 << 31)-1,
                1 << 31, (1 << 31)+1, M32-1, M32]
    tested = 0
    negative_correction = 0
    for x in boundary:
        for w in boundary:
            assert mx_original(x, w) == mx_rewrite(x, w), (x, w)
            assert tiled_u32(x, w) == x*w
            tested += 1
            negative_correction += w > (1 << 31)
    for _ in range(RANDOM_CASES):
        x, w = rng.getrandbits(32), rng.getrandbits(32)
        assert mx_original(x, w) == mx_rewrite(x, w)
        assert tiled_u32(x, w) == x*w
        tested += 1
        negative_correction += w > (1 << 31)
    mw_checked = 0
    for _ in range(RANDOM_CASES):
        m, x = rng.getrandbits(24), rng.getrandbits(32)
        p = m*x
        assert p < (1 << 56)
        assert (p >> 24) <= M32
        assert (tiled_u32(m, x) >> 24) == p >> 24
        mw_checked += 1
    persp_checked = 0
    for _ in range(RANDOM_CASES):
        n = rng.randrange(-(1 << 31), 1 << 31)
        mant = rng.getrandbits(24)
        k = rng.randrange(1, 25)  # reachable nonzero RCP output: leading-zero count + 1
        sh = 32-k
        prod = perspective_tiled(n, mant)
        assert prod == n*mant
        summed = prod + (1 << (sh-1))
        split = (summed >> (sh & 7)) >> (8*(sh >> 3))
        assert split == summed >> sh
        assert saturate_i32(split) == saturate_i32((n*mant + (1 << (sh-1))) >> sh)
        persp_checked += 1
    # Include the signed extreme explicitly rather than hoping random data hits it.
    for n in [-(1 << 31), -(1 << 31)+1, -1, 0, 1, (1 << 31)-1]:
        for mant in [0, 1, (1 << 23), (1 << 24)-1]:
            for k in range(1, 25):
                sh = 32-k
                p = perspective_tiled(n, mant)
                assert p == n*mant
                s = p + (1 << (sh-1))
                assert ((s >> (sh & 7)) >> (8*(sh >> 3))) == s >> sh
                persp_checked += 1
    # Exhaust every direct-colour halfword. Cross-check bit replication against
    # its shift/OR formulation; not a comparison to a compiled repository oracle.
    decode_checked = 0
    for h in range(65536):
        r5, g6, b5 = (h >> 11)&31, (h >> 5)&63, h&31
        assert ((r5 << 3) | (r5 >> 2)) == (r5*8 + r5//4)
        assert ((g6 << 2) | (g6 >> 4)) == (g6*4 + g6//16)
        assert ((b5 << 3) | (b5 >> 2)) == (b5*8 + b5//4)
        # CLUT4 uses four texels per halfword, not just two byte positions.
        for lane in range(4):
            byte = (h >> (8*(lane >> 1))) & 255
            idx = (byte >> (4*(lane & 1))) & 15
            assert idx == ((h >> (4*lane)) & 15)
            decode_checked += 1
    result = {
        'seed': hex(SEED), 'random_cases_per_family': RANDOM_CASES,
        'u32_tiled_and_MX_pairs_checked': tested,
        'MX_negative_correction_cases': negative_correction,
        'MW_width_and_product_cases': mw_checked,
        'perspective_product_and_split_shift_cases': persp_checked,
        'CLUT4_halfword_position_cases': decode_checked,
        'status': 'PASS',
        'scope': 'Python integer identities and boundary/random tests only; no RTL simulation, no Quartus run, no claimed silicon proof'
    }
    Path(__file__).with_name('numeric_check_results.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
