import sys, os, glob
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\tools\reel")
import numpy as np
from rgbframe import load
R = r"C:\programmieren\zencrifice\manafold-pass5-review\renders"

def fogcount(a):
    f = a.astype(np.int16)
    lum = f.max(2)
    sat = f.max(2) - f.min(2)
    return int(((lum > 195) & (sat < 70)).sum())

for clip in sys.argv[1:]:
    fs = sorted(glob.glob(os.path.join(R, clip, "*.rgb")))
    v = np.array([fogcount(load(p)) for p in fs])
    order = np.argsort(v)[::-1]
    print(f"{clip}: n={len(v)} mean={v.mean():.0f} max={v.max()} min={v.min()}")
    print("   worst:", ", ".join(f"f{i}({v[i]})" for i in order[:8]))
    print("   best :", ", ".join(f"f{i}({v[i]})" for i in np.argsort(v)[:5]))
    np.save(os.path.join(r"C:\programmieren\zencrifice\manafold-pass5-review", "fog-"+clip+".npy"), v)
