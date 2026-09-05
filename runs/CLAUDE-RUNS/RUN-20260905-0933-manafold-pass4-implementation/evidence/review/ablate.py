import sys, os, glob
sys.path.insert(0, r"C:\programmieren\zencrifice\zixxtrixx-wholebody-s-spring-20260901\zhaozhou\tools\reel")
import numpy as np
from rgbframe import load

def manamask(a):
    r=a[:,:,0].astype(np.int32); g=a[:,:,1].astype(np.int32); b=a[:,:,2].astype(np.int32)
    return (g>150)&(b>150)&(r<g-40)

def stats(d):
    fs=sorted(glob.glob(os.path.join(d,"*.rgb")))
    out=[]
    for f in fs:
        m=manamask(load(f))
        n=int(m.sum())
        if n==0: out.append((0,0.0,0.0,0.0)); continue
        ys,xs=np.nonzero(m)
        cy,cx=ys.mean(),xs.mean()
        rms=float(np.sqrt(((ys-cy)**2+(xs-cx)**2).mean()))
        # compactness: area / bbox area
        bb=(ys.max()-ys.min()+1)*(xs.max()-xs.min()+1)
        out.append((n,rms,n/bb,float(ys.max()-ys.min()+1)))
    return np.array(out)

A=stats(sys.argv[1]); B=stats(sys.argv[2])
lab=("count","rms_spread","fill_frac","height")
print("frames on=%d off=%d"%(len(A),len(B)))
for i,l in enumerate(lab):
    print("  %-11s ON med=%8.2f mean=%8.2f | OFF med=%8.2f mean=%8.2f | delta=%+.1f%%"%(
        l, np.median(A[:,i]), A[:,i].mean(), np.median(B[:,i]), B[:,i].mean(),
        100*(A[:,i].mean()-B[:,i].mean())/max(1e-9,B[:,i].mean())))
# per-frame variance of spread = "does it work through phases"
print("  spread std ON=%.2f OFF=%.2f"%(A[:,1].std(), B[:,1].std()))
print("  fill_frac std ON=%.4f OFF=%.4f"%(A[:,2].std(), B[:,2].std()))
