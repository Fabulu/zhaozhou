"""Ink-mask silhouette identity: the motion-untouched proof (Direction 30).

The cel-main silhouette ink is the exact colour (26, 24, 22) painted around
the creature's exterior ring (kCelInk* in zhao_reel.cpp). Lighting shades the
interior; the ink hugs the silhouette. If the set of ink pixels is identical
frame-by-frame between two renders, the silhouette -- and therefore the
motion itself -- is unchanged. (The previous pass ran this check but threw
the checker away; this one is committed with the run, per CLAUDE.md.)

Usage: python inkmask.py <dirA> <dirB> <subject> [subject...]
"""
import glob
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "..", "..", "tools", "reel"))
from rgbframe import load

INK = (26, 24, 22)


def mask(img):
    return (img[:, :, 0] == INK[0]) & (img[:, :, 1] == INK[1]) & (img[:, :, 2] == INK[2])


def check(a_dir, b_dir, name):
    fa = sorted(glob.glob(os.path.join(a_dir, "*.rgb")))
    fb = sorted(glob.glob(os.path.join(b_dir, "*.rgb")))
    if len(fa) != len(fb):
        print("%s: FRAME COUNT MISMATCH %d vs %d -> FAIL" % (name, len(fa), len(fb)))
        return False
    bad = 0
    for pa, pb in zip(fa, fb):
        if not np.array_equal(mask(load(pa)), mask(load(pb))):
            bad += 1
            if bad <= 3:
                print("  %s: ink mask differs at %s" % (name, os.path.basename(pa)))
    ok = bad == 0
    print("%s: %d frames, ink-mask identical: %d/%d -> %s"
          % (name, len(fa), len(fa) - bad, len(fa), "OK" if ok else "FAIL"))
    return ok


if __name__ == "__main__":
    a_root, b_root = sys.argv[1], sys.argv[2]
    all_ok = True
    for s in sys.argv[3:]:
        all_ok &= check(os.path.join(a_root, s), os.path.join(b_root, s), s)
    print("RESULT:", "ALL-IDENTICAL" if all_ok else "FAILURES ABOVE")
    sys.exit(0 if all_ok else 1)
