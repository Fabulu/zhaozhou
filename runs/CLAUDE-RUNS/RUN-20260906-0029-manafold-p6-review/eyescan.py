"""Rank EVERY frame by 'is the star still a star'. Sampling by badness.

The number is the star component's major/minor axis ratio. Calibrated on frames
judged BY EYE first (evidence/probe-canfail.txt): the clean 4-point stars in
`curious` and `hover` score 2.3-2.7; the collapsed bar the reviewer photographed
at 12x in `rest` and `taunt` scores 6.3. A probe that scored them alike would be
worthless, and two earlier versions of this file did exactly that.

It says WHICH FRAME TO LOOK AT. The verdict stays with the eye.
"""
import os, sys, json
import numpy as np
from PIL import Image
from scipy import ndimage

S8 = np.ones((3, 3), int)


def frame_elong(a):
    r, g, b = (a[..., i].astype(int) for i in range(3))
    lum = r + g + b
    purple = (b - r > 18) & (b - g > 45) & (b > 85) & (lum > 170) & (lum < 620)
    star = ((g - r > 25) & (b - r > 25) & (g > 100)) | ((lum > 555) & (np.abs(b - g) < 55) & (b - r > -15))
    lab, n = ndimage.label(star, S8)
    if n == 0:
        return []
    near = ndimage.binary_dilation(purple, np.ones((5, 5), bool))
    out = []
    for i in np.unique(lab[near & (lab > 0)]):
        if i == 0:
            continue
        m = lab == i
        if m.sum() < 20:
            continue
        ys, xs = np.nonzero(m)
        c = np.cov(np.vstack([xs - xs.mean(), ys - ys.mean()]))
        ev = np.linalg.eigvalsh(c)
        out.append(float(np.sqrt(max(ev[1], 1e-6) / max(ev[0], 1e-6))))
    return out


if __name__ == "__main__":
    W, clips = sys.argv[1], sys.argv[2:]
    res = {}
    print(f"{'clip':<20} {'n':>4} {'medElong':>9} {'p90':>6} {'max':>6} {'frames>4:1':>11}  worst frames")
    for c in clips:
        d = os.path.join(W, "frames", c)
        rows = []
        for fn in sorted(os.listdir(d)):
            e = frame_elong(np.asarray(Image.open(os.path.join(d, fn)).convert("RGB")))
            rows.append((fn, max(e, default=1.0)))
        v = np.array([r[1] for r in rows])
        worst = [r[0] for r in sorted(rows, key=lambda r: -r[1])[:4]]
        res[c] = dict(n=len(v), med=float(np.median(v)), p90=float(np.percentile(v, 90)),
                      mx=float(v.max()), over4=float((v > 4).mean()), worst=worst)
        print(f"{c:<20} {len(v):4d} {np.median(v):9.2f} {np.percentile(v,90):6.2f} {v.max():6.2f} "
              f"{(v>4).mean()*100:10.1f}%  {' '.join(worst[:3])}")
    json.dump(res, open(os.path.join(W, "evidence", "eyescan.json"), "w"), indent=1)
