"""Rank EVERY frame of every clip by 'is the star still a star'.

Sampling by badness, not by index (CLAUDE.md, Seeing the work properly).
The number is the star component's major/minor axis ratio: ~1.5-2.5 is the
drawn 4-point star, >4 is the bar defect the reviewer saw at 12x in `rest`
and `taunt`. Calibrated on frames judged by eye first -- see probe-canfail.txt.
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
        out.append((float(np.sqrt(max(ev[1], 1e-6) / max(ev[0], 1e-6))), int(m.sum()), float(xs.mean()), float(ys.mean())))
    return out

def scan(framedir):
    rows = []
    for fn in sorted(os.listdir(framedir)):
        a = np.asarray(Image.open(os.path.join(framedir, fn)).convert("RGB"))
        e = frame_elong(a)
        rows.append((fn, max([x[0] for x in e], default=1.0), len(e)))
    return rows

if __name__ == "__main__":
    W = sys.argv[1]
    res = {}
    for c in sorted(os.listdir(os.path.join(W, "frames"))):
        if not c.startswith("manafold-"):
            continue
        rows = scan(os.path.join(W, "frames", c))
        vals = [r[1] for r in rows]
        bad = sorted(rows, key=lambda r: -r[1])[:5]
        res[c] = dict(n=len(rows), median=float(np.median(vals)), p90=float(np.percentile(vals, 90)),
                      frac_over4=float(np.mean([v > 4 for v in vals])), worst=[(b[0], round(b[1], 2)) for b in bad])
        print(f"{c:<24} n={len(rows):4d} med {np.median(vals):5.2f}  p90 {np.percentile(vals,90):5.2f}  "
              f"frames>4:1 {np.mean([v>4 for v in vals])*100:5.1f}%  worst {bad[0][0]} {bad[0][1]:.1f}")
    json.dump(res, open(os.path.join(W, "evidence", "eyescan.json"), "w"), indent=1)
