import sys, os, glob
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\tools\reel")
import numpy as np
from rgbframe import load
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
R=r"C:\programmieren\zencrifice\manafold-pass5-review\renders"
OUT=r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\runs\CLAUDE-RUNS\RUN-20260905-1805-manafold-pass5-by-eye-review\plates"
def masks(a):
    f=a.astype(np.int16); r,g,b=f[:,:,0],f[:,:,1],f[:,:,2]
    body=(r>150)&(r-g>95)&(b>80)
    purple=(b-r>25)&(b>90)&(g<95)&(r<160)
    return body,purple
def cen(m):
    ys,xs=np.nonzero(m)
    if len(xs)==0: return (np.nan,np.nan,0)
    return (xs.mean(),ys.mean(),len(xs))
clips=sys.argv[1:]
fig,axes=plt.subplots(len(clips),2,figsize=(13,3.2*len(clips)),squeeze=False)
for k,clip in enumerate(clips):
    fs=sorted(glob.glob(os.path.join(R,clip,"*.rgb")))
    dx=[];dy=[];ar=[]
    for p in fs:
        a=load(p); bm,pm=masks(a)
        bx,by,_=cen(bm); px,py,pn=cen(pm)
        dx.append(px-bx); dy.append(py-by); ar.append(pn)
    dx=np.array(dx);dy=np.array(dy);ar=np.array(ar)
    ax=axes[k][0]; ax.plot(dx,label="eye dx (px)"); ax.plot(dy,label="eye dy (px)")
    ax.set_title(f"{clip}: lens centroid RELATIVE to body centroid"); ax.legend(fontsize=7); ax.grid(alpha=.3)
    ax2=axes[k][1]; ax2.plot(ar,color="purple"); ax2.set_title(f"{clip}: visible lens AREA (px) -- blinks/gaze")
    ax2.grid(alpha=.3)
    print(f"{clip}: dx range {np.nanmax(dx)-np.nanmin(dx):.2f} px, dy range {np.nanmax(dy)-np.nanmin(dy):.2f} px, "
          f"area {ar.min()}..{ar.max()} (span {ar.max()-ar.min()})")
plt.tight_layout(); plt.savefig(os.path.join(OUT,"eye-trajectory.png"),dpi=100)
print("wrote", os.path.join(OUT,"eye-trajectory.png"))
