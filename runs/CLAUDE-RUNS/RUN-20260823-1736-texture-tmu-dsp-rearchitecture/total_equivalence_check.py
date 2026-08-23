#!/usr/bin/env python3
"""TOTAL equivalence of zhao_texture_bilerp's factored form and the contract's
four-weight law, over the whole 2^48 input space.

WHY THIS EXISTS. tests/formal/texture_bilerp.sby's bmc task proved exactly this
until 2026-08-23. It no longer closes: the DUT and the harness's law lane used
to be the SAME expression, so P1 was nearly a syntactic identity; after the
factoring they are structurally different circuits and the solver must prove a
real distributive-law identity across bit-blasted multipliers of three widths.
Boolector ran 3,300 s without an answer. (The COVER task still passes in ~1 s.)

So the theorem is established here instead, in two total halves. Neither is a
sample.

  HALF 1 -- NO LANE TRUNCATES. Every intermediate is monotone in each texel, so
  its extremes over the byte domain occur at texel CORNERS. Enumerating all 16
  corners x all 65,536 (fu, fv) therefore bounds the whole domain exactly.

  HALF 2 -- GIVEN NO TRUNCATION, THE PRE-ROUNDING SUM IS EXACTLY LINEAR IN THE
  FOUR TEXELS. So S(t) = sum_k t_k * c_k(fu, fv), and evaluating the four basis
  vectors (plus the zero vector, to show there is no constant term) for every
  (fu, fv) determines the map completely. Comparing those coefficients against
  w00 = (256-fu)(256-fv), w10 = fu(256-fv), w01 = (256-fu)fv, w11 = fu*fv
  settles the identity for EVERY texel quadruple, not merely every byte one.

WHAT THIS DOES NOT COVER, stated plainly: it verifies the EXPRESSION, faithfully
transcribed from fpga/rtl/texture/zhao_texture_bilerp.sv, not the RTL bytes. The
transcription gap is what the differential lanes close -- texture_tmu_directed
(76 checks x three FILT_LANES settings) and texture_tmu_random (3,749 bilinear
samples, 476 of them exactly at a rounding tie) run the real Verilator model
against zref::Tmu, which computes the four-weight law.
"""

def S(t00, t10, t01, t11, fu, fv):
    a = (t00 << 8) + (t10 - t00) * fu
    b = (t01 << 8) + (t11 - t01) * fu
    return (a << 8) + (b - a) * fv


def law_weights(fu, fv):
    iu, iv = 256 - fu, 256 - fv
    return (iu * iv, fu * iv, iu * fv, fu * fv)


LIMITS = {
    'du  (signed 9)':  (-256, 255),
    'pu  (signed 18)': (-131072, 131071),
    'a,b (signed 18)': (-131072, 131071),
    'dv  (signed 18)': (-131072, 131071),
    'pv  (signed 27)': (-67108864, 67108863),
    'S   (signed 27)': (-67108864, 67108863),
    'out (unsigned 8)': (0, 255),
}


def half1_no_truncation():
    bad, ext = 0, {k: [0, 0] for k in LIMITS}
    for t00 in (0, 255):
        for t10 in (0, 255):
            for t01 in (0, 255):
                for t11 in (0, 255):
                    for fu in range(256):
                        for fv in range(256):
                            du0, du1 = t10 - t00, t11 - t01
                            pu0, pu1 = du0 * fu, du1 * fu
                            a, b = (t00 << 8) + pu0, (t01 << 8) + pu1
                            dv = b - a
                            pv = dv * fv
                            sw = (a << 8) + pv
                            out = (sw + 32768) >> 16
                            for k, v in (('du  (signed 9)', du0), ('du  (signed 9)', du1),
                                         ('pu  (signed 18)', pu0), ('pu  (signed 18)', pu1),
                                         ('a,b (signed 18)', a), ('a,b (signed 18)', b),
                                         ('dv  (signed 18)', dv), ('pv  (signed 27)', pv),
                                         ('S   (signed 27)', sw), ('out (unsigned 8)', out)):
                                lo, hi = LIMITS[k]
                                if v < lo or v > hi:
                                    bad += 1
                                ext[k][0] = min(ext[k][0], v)
                                ext[k][1] = max(ext[k][1], v)
    return bad, ext


def half2_coefficients():
    bad = 0
    for fu in range(256):
        for fv in range(256):
            if S(0, 0, 0, 0, fu, fv) != 0:
                bad += 1
            c = (S(1, 0, 0, 0, fu, fv), S(0, 1, 0, 0, fu, fv),
                 S(0, 0, 1, 0, fu, fv), S(0, 0, 0, 1, fu, fv))
            if c != law_weights(fu, fv):
                bad += 1
    return bad


if __name__ == '__main__':
    b1, ext = half1_no_truncation()
    print('HALF 1 -- width violations over all 16 texel corners x 65,536 (fu,fv): %d' % b1)
    for k, (lo, hi) in LIMITS.items():
        print('   %-17s observed [%d, %d]   declared [%d, %d]' % (k, ext[k][0], ext[k][1], lo, hi))
    b2 = half2_coefficients()
    print('HALF 2 -- coefficient mismatches over all 65,536 (fu,fv): %d' % b2)
    print('VERDICT: %s' % ('EXACT over all 2^48 inputs' if b1 == 0 and b2 == 0 else 'MISMATCH'))
    raise SystemExit(1 if (b1 or b2) else 0)
