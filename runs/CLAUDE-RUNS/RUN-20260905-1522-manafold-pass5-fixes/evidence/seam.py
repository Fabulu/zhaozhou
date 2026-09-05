"""Loop-seam metric for an always-playing clip (pass-5 item 9 evidence).

seam  = mean |last - first|   (the wrap the viewer sees every loop)
typ   = mean over consecutive pairs, sampled every 10 frames
ratio = seam / typ            (house norm ~2.1-2.4 per the pass-4 review)
Uses tools/reel/rgbframe.load -- the one sanctioned reader.
"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "..", "..", "tools", "reel"))
from rgbframe import load

d = sys.argv[1]
frames = sorted(f for f in os.listdir(d) if f.endswith(".rgb"))
first = load(os.path.join(d, frames[0])).astype(np.int16)
last = load(os.path.join(d, frames[-1])).astype(np.int16)
seam = np.abs(last - first).mean()
typ = []
for i in range(0, len(frames) - 1, 10):
    a = load(os.path.join(d, frames[i])).astype(np.int16)
    b = load(os.path.join(d, frames[i + 1])).astype(np.int16)
    typ.append(np.abs(b - a).mean())
t = float(np.mean(typ))
print(f"{os.path.basename(d)}: seam {seam:.2f}  typical {t:.2f}  ratio {seam/t:.2f}")
