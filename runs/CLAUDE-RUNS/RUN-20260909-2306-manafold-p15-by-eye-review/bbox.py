import sys, os, glob
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-p15-review\zhaozhou\tools\reel")
import numpy as np
from rgbframe import load
REEL = r"C:\programmieren\zencrifice\manafold-p12-fix\Upheaval\website\scratch-reel"
# ink-line based bbox is known-bad (item 40). Use a crude "not sky, not dirt" via
# column-wise novelty vs the same frame's leftmost 20 columns (background only there? unproven).
# Instead: just print a coarse 12x8 luminance/saturation map so I can SEE where the subject is.
subj, n = sys.argv[1], int(sys.argv[2])
a = load(os.path.join(REEL, "manafold-"+subj, f"{n:04d}.rgb")).astype(int)
mx = a.max(2); mn = a.min(2)
sat = (mx-mn)
# print coarse grid of mean saturation
for gy in range(0,240,20):
    row=""
    for gx in range(0,384,16):
        v = sat[gy:gy+20, gx:gx+16].mean()
        row += " .:-=+*#%@"[min(9,int(v/14))]
    print(f"y{gy:3d} {row}")
print("cols: x=0..384 step16")
