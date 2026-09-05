"""Ink-mask silhouette identity: the motion-untouched proof (Direction 30),
REBUILT for pass 4 so it can actually FAIL (gate checklist item 6).

THE GRAVE THIS TOOL CLIMBED OUT OF (pass-3 review, fault 7): the cel ink is
authored as exactly (26, 24, 22), but the shipped frames are RGB565 -- the
quantiser moves the ink onto (25, 24, 25) and (25, 24, 16), so the old
exact-match mask was EMPTY on every path, and np.array_equal(empty, empty)
passed 6674/6674 frames of nothing. Two published "motion untouched" proofs
were comparisons of empty sets.

The rules now:
  * The mask matches the QUANTISED ink triples (plus the pre-quantisation
    authored value for any non-565 diagnostic path).
  * A frame pair where BOTH masks are empty counts as VACUOUS, not identical.
    A subject whose every frame is vacuous FAILS loudly -- an empty
    instrument is a broken instrument, never a passing one.
  * `python inkmask.py selftest` proves the tool can fail: identical sets
    pass, a dilated outline fails, and all-empty input reports VACUOUS.

Usage: python inkmask.py <dirA> <dirB> <subject> [subject...]
       python inkmask.py selftest
"""
import glob
import os
import sys

import numpy as np

from rgbframe import load

# The authored ink and its RGB565 quantisations (measured on shipped frames,
# pass-3 review evidence inkmask-is-vacuous-everywhere.txt).
INK_TRIPLES = (
    (26, 24, 22),   # authored (pre-quantisation paths)
    (25, 24, 25),   # RGB565 round A (the dominant shipped triple)
    (25, 24, 16),   # RGB565 round B
)


def mask(img):
    m = np.zeros(img.shape[:2], bool)
    for r, g, b in INK_TRIPLES:
        m |= (img[:, :, 0] == r) & (img[:, :, 1] == g) & (img[:, :, 2] == b)
    return m


def check(a_dir, b_dir, name):
    fa = sorted(glob.glob(os.path.join(a_dir, "[0-9]" * 4 + ".rgb")))
    fb = sorted(glob.glob(os.path.join(b_dir, "[0-9]" * 4 + ".rgb")))
    if len(fa) != len(fb):
        print("%s: FRAME COUNT MISMATCH %d vs %d -> FAIL" % (name, len(fa), len(fb)))
        return False
    if not fa:
        print("%s: NO FRAMES -> FAIL (vacuous)" % name)
        return False
    bad = 0
    vacuous = 0
    ink_total = 0
    for pa, pb in zip(fa, fb):
        ma, mb = mask(load(pa)), mask(load(pb))
        na, nb = int(ma.sum()), int(mb.sum())
        ink_total += na
        if na == 0 and nb == 0:
            vacuous += 1
            continue
        if not np.array_equal(ma, mb):
            bad += 1
            if bad <= 3:
                print("  %s: ink mask differs at %s (%d vs %d ink px)"
                      % (name, os.path.basename(pa), na, nb))
    n = len(fa)
    if vacuous == n:
        print("%s: %d frames, ZERO ink pixels in every frame -> FAIL "
              "(vacuous: the instrument matched nothing; the ink colours may "
              "have moved -- re-measure them)" % (name, n))
        return False
    ok = bad == 0
    print("%s: %d frames (%d ink px total, %d vacuous), identical: %d/%d -> %s"
          % (name, n, ink_total, vacuous, n - bad, n, "OK" if ok else "FAIL"))
    return ok


def selftest():
    """Prove the instrument can fail (gate checklist item 6)."""
    import struct
    import tempfile

    def write_rgb(path, img):
        h, w, _ = img.shape
        with open(path, "wb") as f:
            f.write(struct.pack("<II", w, h))
            f.write(img.tobytes())

    rc = True
    with tempfile.TemporaryDirectory() as td:
        base = np.full((240, 384, 3), 200, np.uint8)
        ring = base.copy()
        ring[100:140, 150:230] = INK_TRIPLES[1]      # a quantised-ink block
        ring[110:130, 160:220] = (231, 140, 150)     # interior flesh
        dil = base.copy()
        dil[98:142, 148:232] = INK_TRIPLES[1]        # dilated outline
        dil[110:130, 160:220] = (231, 140, 150)
        legacy = base.copy()
        legacy[100:140, 150:230] = INK_TRIPLES[0]    # authored-value ink
        legacy[110:130, 160:220] = (231, 140, 150)
        empty = base.copy()                          # no ink anywhere
        cases = [
            ("identical", ring, ring, True),
            ("legacy-authored-ink", legacy, legacy, True),
            ("dilated-outline", ring, dil, False),   # MUST fail
            ("all-empty", empty, empty, False),      # MUST fail (vacuous)
        ]
        for nm, a, b, want in cases:
            da, db = os.path.join(td, nm + "A"), os.path.join(td, nm + "B")
            os.makedirs(da), os.makedirs(db)
            for i in range(3):
                write_rgb(os.path.join(da, "%04d.rgb" % i), a)
                write_rgb(os.path.join(db, "%04d.rgb" % i), b)
            got = check(da, db, "selftest-" + nm)
            verdict = "ok" if got == want else "SELFTEST FAILURE"
            print("  selftest %s: expected %s, got %s -> %s"
                  % (nm, "PASS" if want else "FAIL", "PASS" if got else "FAIL", verdict))
            rc &= got == want
    print("SELFTEST:", "PASS (the instrument can fail)" if rc else "BROKEN")
    return rc


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "selftest":
        sys.exit(0 if selftest() else 1)
    a_root, b_root = sys.argv[1], sys.argv[2]
    all_ok = True
    for s in sys.argv[3:]:
        all_ok &= check(os.path.join(a_root, s), os.path.join(b_root, s), s)
    print("RESULT:", "ALL-IDENTICAL" if all_ok else "FAILURES ABOVE")
    sys.exit(0 if all_ok else 1)
