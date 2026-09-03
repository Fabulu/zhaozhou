#!/usr/bin/env python3
"""qa3_contact.py -- THE PEEL, measured honestly.

The implementer's qa25_contactfront.py classifies the SKY as ground
(sky RGB ~ (165,97,107) satisfies R<190 & G<150 & B<130), so its terrain
edge is row 0 in every column and its "contact front" degenerates to the
creature's right-hand bounding box.  This instrument instead calibrates the
ground line from the REST pose, where the body demonstrably lies on the dirt:
the creature's own bottom edge over the grounded stretch IS the terrain line
at the creature's depth plane (probe: body lateral span 0 mm -> one plane).
Columns outside the rest footprint inherit the nearest calibrated value.
"""
import os,sys
import numpy as np
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from _maskfns import load, mask_of

d=sys.argv[1]
BAND=int(sys.argv[2]) if len(sys.argv)>2 else 2
frames=load(d)
masks=[mask_of(f) for f in frames]
W=masks[0].shape[1]
def bottom(m):
    b=np.full(W,-1,int)
    for x in range(W):
        c=np.nonzero(m[:,x])[0]
        if len(c): b[x]=c[-1]
    return b
b0=bottom(masks[0])
# grounded stretch of the REST pose = the plateau of the rest bottom edge
mx=b0.max()
grounded=np.nonzero(b0>=mx-1)[0]
g0,g1=grounded.min(),grounded.max()
ground=np.full(W,float(mx))
for x in range(W):
    ground[x]=b0[min(max(x,g0),g1)]
print("rest footprint columns x=%d..%d, ground line y=%.1f..%.1f"%(g0,g1,ground[g0],ground[g1]))
print("%5s %6s %6s %6s %6s %8s %9s"%("f","touchN","tLeft","tRight","noseX","coilGapPx","bodyBotY"))
rows=[]
for i,m in enumerate(masks):
    b=bottom(m)
    has=b>=0
    touch=np.nonzero(has & (b>=ground-BAND))[0]
    xs=np.nonzero(has)[0]
    nose=xs.max()
    # daylight: smallest gap over columns NOT in the contact patch's own span
    if len(touch):
        tl,tr=touch.min(),touch.max()
        off=[x for x in xs if x>tr+2]
        gap=min((ground[x]-b[x]) for x in off) if off else -999
        rows.append((i,len(touch),tl,tr,nose,gap))
        print("%5d %6d %6d %6d %6d %9.0f"%(i,len(touch),tl,tr,nose,gap))
    else:
        rows.append((i,0,-1,-1,nose,999))
        print("%5d %6d %6s %6s %6d %9s"%(i,0,'-','-',nose,'AIRBORNE'))
