import os,numpy as np
from scipy import ndimage
def load(p,n):
    ns=sorted(x for x in os.listdir(p) if x.endswith('.rgb'))[:n]
    return [np.fromfile(os.path.join(p,x),dtype=np.uint8)[8:].reshape(240,384,3).astype(np.int16) for x in ns]
def mask_of(f):
    R,G,B=f[:,:,0],f[:,:,1],f[:,:,2]; lum=R+G+B
    m=(lum<130)|(G>R+18)|(B>R+18)|(lum>470)|((R>150)&(G>130)&(B<110))
    st=np.ones((3,3),bool); m=ndimage.binary_closing(m,st); m=ndimage.binary_closing(m,st)
    m=ndimage.binary_fill_holes(m); lab,n=ndimage.label(m)
    s=ndimage.sum(m,lab,range(1,n+1)); return lab==int(np.argmax(s))+1
cols=np.arange(384); tail=cols<170
for lab,p,n,end in (('PREV','qa2-renders-prev/zixxtrixx-spring-side',169,168),('HEAD','qa2-renders/zixxtrixx-spring-side',169,168)):
    ms=[mask_of(f) for f in load(p,n)]
    print('==',lab)
    cy=[];bb=[];tipx=[];tipy=[];area=[]
    for m in ms:
        t=m.copy(); t[:,~tail]=False
        ys,xs=np.nonzero(t)
        cy.append(ys.mean()); area.append(len(ys))
        bb.append((xs.max()-xs.min(), ys.max()-ys.min()))
        i=np.argmin(xs); tipx.append(xs.min()); tipy.append(ys[xs==xs.min()].mean())
    cy=np.array(cy); area=np.array(area,float)
    idx=[0,12,36,48,60,72,84,108,132,end]
    print("  tail-third centroid y :", " ".join(f"f{i}:{cy[i]:.1f}" for i in idx))
    print(f"  beat1 (f12->f72) {cy[72]-cy[12]:+.2f} px   beat2 (f82->f{end}) {cy[end]-cy[82]:+.2f} px   -> DIRECTION CHANGE" if (cy[72]-cy[12])*(cy[end]-cy[82])<0 else "")
    print("  tail-third area (apparent size):", " ".join(f"f{i}:{area[i]:.0f}" for i in idx))
    print(f"  area min/max over ground {area.min():.0f}/{area.max():.0f} = {100*(area.max()-area.min())/area.min():.0f}% swing")
    print("  fin-tip x:", " ".join(f"f{i}:{tipx[i]}" for i in idx))
    print("  fin-tip y:", " ".join(f"f{i}:{tipy[i]:.0f}" for i in idx))
    bbw=np.array([b[0] for b in bb]); bbh=np.array([b[1] for b in bb])
    print(f"  tail bbox w {bbw.min()}..{bbw.max()}  h {bbh.min()}..{bbh.max()}")
