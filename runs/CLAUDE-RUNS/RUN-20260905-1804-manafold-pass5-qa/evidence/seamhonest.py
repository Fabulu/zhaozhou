"""Honest loop-seam: EVERY consecutive pair, plus where the seam sits in that
distribution. seam.py samples every 10th pair, which aliases against clip
periodicity -- and pass 5's fix QUANTISED the clip periods."""
import sys, os
import numpy as np
sys.path.insert(0,"C:/programmieren/zencrifice/manafold-pass5-qa/zhaozhou/tools/reel")
from rgbframe import load
d=sys.argv[1]
fs=sorted(f for f in os.listdir(d) if f.endswith(".rgb"))
A=np.stack([load(os.path.join(d,f)).astype(np.int16) for f in fs])
steps=np.abs(np.diff(A,axis=0)).mean(axis=(1,2,3))          # all N-1 pairs
seam=np.abs(A[-1]-A[0]).mean()
stride10=np.array([steps[i] for i in range(0,len(steps),10)])
pct=(steps<seam).mean()*100
print(f"{os.path.basename(d)}  frames={len(fs)}")
print(f"  seam(last->first)     = {seam:.3f}")
print(f"  typ ALL pairs mean    = {steps.mean():.3f}   -> ratio {seam/steps.mean():.3f}")
print(f"  typ stride-10 (seam.py)= {stride10.mean():.3f}   -> ratio {seam/stride10.mean():.3f}")
print(f"  typ median            = {np.median(steps):.3f}   -> ratio {seam/np.median(steps):.3f}")
print(f"  typ p90 / max         = {np.percentile(steps,90):.3f} / {steps.max():.3f}")
print(f"  seam percentile among real steps = {pct:.1f}%  (100% = seam is the biggest jump in the clip)")
