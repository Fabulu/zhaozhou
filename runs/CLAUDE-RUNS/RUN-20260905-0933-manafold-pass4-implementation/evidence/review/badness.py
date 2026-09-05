import sys, os, glob
sys.path.insert(0, r"C:\programmieren\zencrifice\zixxtrixx-wholebody-s-spring-20260901\zhaozhou\tools\reel")
import numpy as np
from rgbframe import load

def masks(a):
    r = a[:,:,0].astype(np.int32); g = a[:,:,1].astype(np.int32); b = a[:,:,2].astype(np.int32)
    mana = (g > 150) & (b > 150) & (r < g - 40)
    pink = (r > 130) & (r > g + 45) & (r > b + 20)
    ink  = (r < 60) & (g < 60) & (b < 55)
    return mana, pink, ink

def sweep(d):
    fs = sorted(glob.glob(os.path.join(d, "*.rgb")))
    if not fs: return None
    prev = None; rows = []
    for i, f in enumerate(fs):
        a = load(f).astype(np.int16)
        mn, pk, ik = masks(a)
        dl = 0.0
        if prev is not None:
            dl = float(np.abs(a - prev).mean())
        prev = a
        rows.append((i, dl, int(mn.sum()), int(pk.sum()), int(ik.sum())))
    first = load(fs[0]).astype(np.int16); last = load(fs[-1]).astype(np.int16)
    seam = float(np.abs(last-first).mean())
    return rows, seam, len(fs)

for d in sys.argv[1:]:
    r = sweep(d)
    if r is None:
        print("%-26s NO FRAMES" % os.path.basename(d)); continue
    rows, seam, n = r
    dls = [x[1] for x in rows[1:]]
    mana = [x[2] for x in rows]; pink=[x[3] for x in rows]
    worst = sorted(rows[1:], key=lambda x:-x[1])[:3]
    print("%-26s n=%3d seam=%5.2f dl_mean=%5.2f dl_max=%5.2f@%s worst=%s mana[min/med/max]=%d/%d/%d pink[min/med/max]=%d/%d/%d" % (
        os.path.basename(d), n, seam, float(np.mean(dls)), max(dls),
        [x[0] for x in rows[1:] if x[1]==max(dls)][0],
        ",".join("%d:%.1f"%(x[0],x[1]) for x in worst),
        min(mana), int(np.median(mana)), max(mana),
        min(pink), int(np.median(pink)), max(pink)))
