"""Track the creature across frame, and measure the mana, from the RENDERED
frames. Measurement lives on the COMPARISON side only (CLAUDE.md): it tells
me whether the traverse is real and whether a variant overran the window --
it never chooses a value.

  track.py <clipdir> <first> <last> <step>

Reports, per sampled frame: pink-body bbox (x0,x1) and centroid, plus the
mana pixel count (saturated cyan/aqua) and the near-white count.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                "..", "zhaozhou", "tools", "reel"))
import rgbframe
import numpy as np

def stats(a):
    r = a[:, :, 0].astype(int); g = a[:, :, 1].astype(int); b = a[:, :, 2].astype(int)
    # the body pigment: strongly red-dominant, blue over green (pink, not dirt)
    # (the first mask caught the dusty-rose SKY: x0/x1 pinned to 0/383 on
    # every frame. The body pigment is BODY_PINK (228,70,124) -- hot, with a
    # big red-green gap. The sky's rose is pale and near-neutral.)
    pink = (r > 175) & (r - g > 95) & (r - b > 40)
    # the mana: green+blue dominant over red (cyan / aqua), any brightness
    mana = (g > 90) & (b > 90) & (g - r > 30) & (b - r > 30)
    # the whitening: everything high and nearly equal
    white = (r > 200) & (g > 200) & (b > 200) & (abs(r - g) < 34) & (abs(g - b) < 34)
    xs = np.nonzero(pink.any(axis=0))[0]
    ys = np.nonzero(pink.any(axis=1))[0]
    if len(xs) == 0:
        return None
    cx = float(np.nonzero(pink)[1].mean())
    # hue spread inside the mana: the saturation proxy the project uses
    sat = 0.0
    if mana.sum():
        mx = np.maximum(np.maximum(r, g), b)[mana]
        mn = np.minimum(np.minimum(r, g), b)[mana]
        sat = float((mx - mn).mean())
    return dict(x0=int(xs[0]), x1=int(xs[-1]), y0=int(ys[0]), y1=int(ys[-1]),
                cx=cx, pink=int(pink.sum()), mana=int(mana.sum()),
                white=int(white.sum()), sat=sat)

if __name__ == "__main__":
    d = sys.argv[1]
    a, b_, st = int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
    prev = None
    print("frame  x0  x1   cx    w   h   pink  mana  white   sat   dcx")
    for f in range(a, b_ + 1, st):
        p = os.path.join(d, "%04d.rgb" % f)
        if not os.path.exists(p):
            break
        s = stats(rgbframe.load(p))
        if s is None:
            print(f, "no creature"); continue
        d_ = "" if prev is None else "%+.2f/f" % ((s["cx"] - prev) / st)
        prev = s["cx"]
        print("%5d %3d %3d %6.1f %4d %3d %6d %5d %6d %5.1f  %s" % (
            f, s["x0"], s["x1"], s["cx"], s["x1"] - s["x0"], s["y1"] - s["y0"],
            s["pink"], s["mana"], s["white"], s["sat"], d_))
