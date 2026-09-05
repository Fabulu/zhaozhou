"""How does the mana READ -- filled aqua, or white steam?

⚠ This does NOT reproduce pass 5's "saturation 109-123 -> 85-100". That number's
definition lives in source this lane cannot reach (pass 6 was never pushed), so
per checklist item 7 it is INHERITED and is not compared against. This is a
different, stated measurement of the same question.

MANA = a pixel that is not pink and not scenery: blue-or-green dominant over red
(b >= r or g >= r) AND brighter than the sky it sits on. The creature's own pink
is strongly red-dominant and the ground is brown-red, so both drop out.

Split into:
  AQUA   saturation >= 60 -- it still has its colour
  WHITE  saturation <  60 -- it has clamped to hue-neutral, the documented
         failure mode in 08-LIGHTING.md and the thing the owner's "don't go
         overboard" is protecting against
Reported as the WHITE SHARE. The read the owner asked for is "filled and
saturated"; a high white share IS "it reads as steam", stated as a number.
"""
import os, sys
import numpy as np
from PIL import Image


def mana_stats(a):
    r, g, b = (a[..., i].astype(int) for i in range(3))
    mx = np.maximum(np.maximum(r, g), b)
    mn = np.minimum(np.minimum(r, g), b)
    sat = mx - mn
    lum = a.astype(int).sum(2)
    mana = ((b >= r) | (g >= r)) & (lum > 430) & (mx > 150)
    n = int(mana.sum())
    if n == 0:
        return None
    s = sat[mana]
    return dict(px=n, mean_sat=float(s.mean()), median_sat=float(np.median(s)),
                white_share=float((s < 60).mean()), aqua_px=int((s >= 60).sum()))


if __name__ == "__main__":
    W = sys.argv[1]
    print(f"{'clip':<24} {'manapx':>7} {'meanSat':>8} {'medSat':>7} {'WHITE share':>12} {'aquapx':>7}")
    for fn in sorted(os.listdir(os.path.join(W, "site", "media"))):
        if not fn.endswith(".png") or "eye-" in fn:
            continue
        a = np.asarray(Image.open(os.path.join(W, "site", "media", fn)).convert("RGB").resize((384, 240), Image.NEAREST))
        st = mana_stats(a)
        if not st:
            print(f"{fn[:-4]:<24}  no mana pixels"); continue
        print(f"{fn[:-4]:<24} {st['px']:7d} {st['mean_sat']:8.1f} {st['median_sat']:7.1f} "
              f"{st['white_share']*100:11.1f}% {st['aqua_px']:7d}")
