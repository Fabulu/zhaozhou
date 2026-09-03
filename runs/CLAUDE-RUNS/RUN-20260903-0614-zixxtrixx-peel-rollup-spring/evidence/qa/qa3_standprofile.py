import os,sys
import numpy as np
sys.path.insert(0,'.')
from _maskfns import load, mask_of
d='renders/zixxtrixx-spring-side'
frames=load(d); masks=[mask_of(f) for f in frames]
W=masks[0].shape[1]
def bot(m):
    b=np.full(W,-1,int)
    for x in range(W):
        c=np.nonzero(m[:,x])[0]
        if len(c): b[x]=c[-1]
    return b
b0=bot(masks[0]); mx=b0.max(); g=np.nonzero(b0>=mx-1)[0]
GY=float(mx)
for f in [55,70,85,100,115]:
    b=bot(masks[f])
    xs=np.nonzero(b>=0)[0]
    print("frame",f,"columns x=%d..%d ; gap = groundline(%d) - bottom"%(xs.min(),xs.max(),int(GY)))
    line=[]
    for x in range(xs.min(),xs.max()+1):
        line.append("%d:%+d"%(x,int(GY-b[x])) if b[x]>=0 else "%d:--"%x)
    print("  "+" ".join(line))
    print()
