"""inkwidth -- perpendicular cel-ink thickness, measured (pass 4, R12).

The owner's question: "is the line around the creature the same thickness
as Zixxtrixx at any distance?" The cel ink is a screen-space post pass
whose width is a shared function of the PROJECTED creature radius
(cel_main_ink_width in zhao_reel.cpp), so the expected answer is "yes, by
construction, at matched projected size" -- this tool is the measurement
side, and it also catches the credible exception (interior ink doubling in
the eye crevices / thin blade at distance).

Method: the ink mask uses the QUANTISED triples the pass-3 review
established -- (25,24,25) and (25,24,16) -- NEVER the pre-quantisation
authored value (inkmask.py's own grave). Thickness = 2x the euclidean
distance transform sampled on the mask's ridge (local maxima of the EDT),
which is the stroke's perpendicular width in pixels; median/p10/p90
reported per frame.

`python inkwidth.py selftest` proves the instrument can fail: synthetic
2 px and 5 px outlines must measure ~2 and ~5, and a dilated outline must
measure thicker than its source.

Usage: python inkwidth.py <frame.rgb> [frame.rgb ...]
       python inkwidth.py selftest
"""
import sys

import numpy as np
from scipy import ndimage

from rgbframe import load
from inkmask import mask as ink_mask


def widths(im):
    m = ink_mask(im)
    n = int(m.sum())
    if n == 0:
        return None
    edt = ndimage.distance_transform_edt(m)
    # the ridge: pixels whose EDT is the local 3x3 maximum
    mx = ndimage.maximum_filter(edt, size=3)
    ridge = m & (edt >= mx - 1e-6) & (edt > 0)
    w = 2.0 * edt[ridge]
    if w.size == 0:
        return None
    return {
        "ink_px": n,
        "median": float(np.median(w)),
        "p10": float(np.percentile(w, 10)),
        "p90": float(np.percentile(w, 90)),
    }


def report(path):
    st = widths(load(path))
    if st is None:
        print("%s: NO INK PIXELS -> the mask matched nothing (vacuous; "
              "re-measure the ink colours)" % path)
        return False
    print("%s: ink %d px, width median %.1f  p10 %.1f  p90 %.1f"
          % (path, st["ink_px"], st["median"], st["p10"], st["p90"]))
    return True


def selftest():
    ok = True

    def ring_img(width):
        im = np.full((240, 384, 3), 200, np.uint8)
        yy, xx = np.mgrid[0:240, 0:384]
        r = np.hypot(yy - 120, xx - 192)
        band = (r > 60) & (r < 60 + width)
        im[band] = (25, 24, 25)
        return im

    for w_true in (2, 5):
        st = widths(ring_img(w_true))
        got = st["median"]
        good = abs(got - w_true) <= 1.2
        print("selftest %dpx ring: measured median %.1f -> %s"
              % (w_true, got, "ok" if good else "SELFTEST FAILURE"))
        ok &= good
    a, b = widths(ring_img(2)), widths(ring_img(6))
    if not b["median"] > a["median"] + 2:
        print("SELFTEST FAILURE: dilation not detected")
        ok = False
    else:
        print("selftest dilation: 2px -> %.1f vs 6px -> %.1f (detected)"
              % (a["median"], b["median"]))
    # vacuous input must be reported, not passed
    empty = np.full((240, 384, 3), 200, np.uint8)
    if widths(empty) is not None:
        print("SELFTEST FAILURE: empty frame produced a measurement")
        ok = False
    else:
        print("selftest empty frame: correctly reports no ink")
    print("SELFTEST:", "PASS (the instrument can fail)" if ok else "BROKEN")
    return ok


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "selftest":
        sys.exit(0 if selftest() else 1)
    rc = True
    for f in sys.argv[1:]:
        rc &= report(f)
    sys.exit(0 if rc else 1)
