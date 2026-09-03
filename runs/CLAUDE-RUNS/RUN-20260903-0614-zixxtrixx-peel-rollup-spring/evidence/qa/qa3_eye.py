import os,sys
import numpy as np
sys.path.insert(0,'.')
from _maskfns import load, mask_of
d=sys.argv[1]
fr=load(d); ms=[mask_of(f) for f in fr]
print("%5s %6s %6s %6s %6s %6s %8s"%("f","eyeX","eyeY","eyeN","noseX","tipX","nose-tip"))
rows=[]
for i,(f,m) in enumerate(zip(fr,ms)):
    R,G,B=f[:,:,0],f[:,:,1],f[:,:,2]
    eye=(R>150)&(G>130)&(B<110)&m
    ys,xs=np.nonzero(eye)
    ex,ey,en=(xs.mean(),ys.mean(),len(xs)) if len(xs) else (float('nan'),float('nan'),0)
    bx=np.nonzero(m.any(0))[0]
    # tail tip = lowest touching column set
    b=np.full(m.shape[1],-1,int)
    for x in range(m.shape[1]):
        c=np.nonzero(m[:,x])[0]
        if len(c): b[x]=c[-1]
    GY=None
    rows.append((i,ex,ey,en,bx.max()))
b0=np.full(ms[0].shape[1],-1,int)
for x in range(ms[0].shape[1]):
    c=np.nonzero(ms[0][:,x])[0]
    if len(c): b0[x]=c[-1]
GY=b0.max()
for i,m in enumerate(ms):
    b=np.full(m.shape[1],-1,int)
    for x in range(m.shape[1]):
        c=np.nonzero(m[:,x])[0]
        if len(c): b[x]=c[-1]
    t=np.nonzero((b>=0)&(b>=GY-2))[0]
    tip=t.max() if len(t) else -1
    r=rows[i]
    if True:
        print("%5d %6.1f %6.1f %6d %6d %6d %8s"%(i,r[1],r[2],r[3],r[4],tip,(r[4]-tip) if tip>=0 else '-'))
