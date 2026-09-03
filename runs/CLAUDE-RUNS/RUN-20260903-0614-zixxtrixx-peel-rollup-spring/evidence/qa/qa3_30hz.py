import os,sys
import numpy as np
sys.path.insert(0,'.')
from _maskfns import load, mask_of
def series(d,g1):
    fr=load(d); ms=[mask_of(f) for f in fr]
    cx=np.array([np.nonzero(m)[1].mean() for m in ms])
    cy=np.array([np.nonzero(m)[0].mean() for m in ms])
    ident=sum(1 for i in range(1,g1+1) if np.array_equal(fr[i],fr[i-1]))
    return cx[:g1+1],cy[:g1+1],ident
def ratio(v,off=0):
    d=np.abs(np.diff(v))[off:]
    return np.mean(d[0::2])/max(np.mean(d[1::2]),1e-9)
for lbl,d,g1 in (("HEAD PEEL","renders/zixxtrixx-spring-side",115),
                 ("PUBLISHED","renders-prev/zixxtrixx-spring-side",167)):
    cx,cy,ident=series(d,g1)
    print("%s: byte-identical consecutive frames in ground phase = %d"%(lbl,ident))
    for nm,v in (("cx",cx),("cy",cy)):
        print("   %s med|step|=%.3f px  odd/even ratio phase0=%.2f phase1=%.2f"%(nm,np.median(np.abs(np.diff(v))),ratio(v,0),ratio(v,1)))
    # windowed on cy
    n=len(cy); w=n//4
    print("   cy ratio by quarter:", " ".join("%.2f"%ratio(cy[i*w:(i+1)*w+1]) for i in range(4)))
    print("   cx ratio by quarter:", " ".join("%.2f"%ratio(cx[i*w:(i+1)*w+1]) for i in range(4)))
