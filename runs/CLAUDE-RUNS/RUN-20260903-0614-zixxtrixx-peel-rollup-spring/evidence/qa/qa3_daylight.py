#!/usr/bin/env python3
"""qa3_daylight.py -- does the animal STAND on the tail tip?

Planarity makes this 2D test sound: the probe reports body lateral span 0 mm,
so every point of the creature sits in ONE depth plane. Under a perspective
camera a single depth plane maps world height monotonically to screen y.
Therefore the creature's own contact line has ONE screen y, and it is
calibrated from the REST frames where the body is lying on the dirt.
The receding hill crest behind the animal is irrelevant to this measure.
Uses qa_region.py's calibrated colour classifier verbatim.
"""
import os,sys
import numpy as np
from scipy import ndimage
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from _maskfns import load, mask_of

p=sys.argv[1]
frames=load(p)
masks=[mask_of(f) for f in frames]
# ground line: lowest creature pixel over the rest frames f0..f3
gl=max(np.nonzero(m.any(1))[0].max() for m in masks[:4])
print("ground-line screen y (lowest creature pixel at rest, f0-3) =",gl)
print()
print(" f | bottom | cols with bottom>=gl-1 | lowest-x | span_lowband | coil_bottom(x>=%s) | daylight_px"%sys.argv[2] if len(sys.argv)>2 else "")
XSPLIT=int(sys.argv[2]) if len(sys.argv)>2 else 999
print("%4s %7s %7s %9s %9s %11s %9s"%("f","bot","ncols","xlo","xhi","coilbot","daylight"))
for i,m in enumerate(masks):
    ys,xs=np.nonzero(m)
    bot=ys.max()
    low=ys>=gl-1
    ncols=len(np.unique(xs[low])) if low.any() else 0
    xlo=xs[low].min() if low.any() else -1
    xhi=xs[low].max() if low.any() else -1
    sel=xs>=XSPLIT
    coilbot=ys[sel].max() if sel.any() else -1
    print("%4d %7d %7d %9d %9d %11d %9d"%(i,bot,ncols,xlo,xhi,coilbot,gl-coilbot))
