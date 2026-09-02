import os, sys, numpy as np
from scipy import ndimage
sys.path.insert(0,'.')
W,H=384,240
def load(p):
    ns=sorted(n for n in os.listdir(p) if n.endswith('.rgb')); out=[]
    for n in ns:
        r=np.fromfile(os.path.join(p,n),dtype=np.uint8); w,h=r[:8].view(np.uint32)
        out.append(r[8:].reshape(int(h),int(w),3).astype(np.int16))
    return out
def mask_of(f):
    R,G,B=f[:,:,0],f[:,:,1],f[:,:,2]; lum=R+G+B
    m=(lum<130)|(G>R+18)|(B>R+18)|(lum>470)|((R>150)&(G>130)&(B<110))
    st=np.ones((3,3),bool); m=ndimage.binary_closing(m,st); m=ndimage.binary_closing(m,st)
    m=ndimage.binary_fill_holes(m); lab,n=ndimage.label(m)
    if n==0: return m
    s=ndimage.sum(m,lab,range(1,n+1)); return lab==int(np.argmax(s))+1
cols=np.arange(W)
regs={'tail':cols<170,'mid':(cols>=170)&(cols<210),'head':cols>=210}
for label,path in (('PREV ecf0e3ab','qa-renders-prev/zixxtrixx-spring-side'),('HEAD 48310d18','qa-renders/zixxtrixx-spring-side')):
    fr=load(path); ms=[mask_of(f) for f in fr]
    print(f"=== {label}  ({len(fr)} frames)")
    for nm,lo,hi in (('beat1',12,72),('beat2',82,144)):
        tot={rn:sum(int((ms[i]^ms[i-1])[:,sel].sum()) for i in range(lo+1,hi+1)) for rn,sel in regs.items()}
        T=sum(tot.values())
        print(f"  {nm} f{lo}-{hi} ABS px: "+"  ".join(f"{rn} {v:6d} ({100*v/T:4.1f}%)" for rn,v in tot.items())+f"   total {T}")
    def rc(i,sel):
        m=ms[i].copy(); m[:,~sel]=False; ys,xs=np.nonzero(m); return ys.mean() if len(ys) else np.nan
    for w0,w1 in ((72,144),(12,72),(72,160)):
        if w1<len(ms):
            print(f"  centroid-y descent f{w0}->f{w1}: "+"  ".join(f"{rn} {rc(w1,sel)-rc(w0,sel):+5.2f}" for rn,sel in regs.items()))
    # region centroid Y route through beat1 (does mid RISE?)
    prof={rn:[rc(i,sel) for i in range(0,min(170,len(ms)))] for rn,sel in regs.items()}
    for rn in regs:
        v=prof[rn]
        print(f"  {rn:5s} y route: f12 {v[12]:.2f}  f42 {v[42]:.2f}  f72 {v[72]:.2f}  f108 {v[108]:.2f}  f144 {v[144]:.2f}   (beat1 delta {v[72]-v[12]:+.2f})")
