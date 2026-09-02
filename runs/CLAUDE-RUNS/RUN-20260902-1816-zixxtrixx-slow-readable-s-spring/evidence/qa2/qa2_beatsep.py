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
def topedge(m):
    o=np.full(384,np.nan)
    for x in range(384):
        ys=np.nonzero(m[:,x])[0]
        if len(ys): o[x]=ys.min()
    return o
BEATS=(('settle',1,12),('beat1',12,72),('dwell',72,82),('beat2',82,144),('hold',144,168),('launch',168,176))
for lab,path,n in (('PREV 48310d18','qa2-renders-prev/zixxtrixx-spring-side',177),('HEAD d5949320','qa2-renders/zixxtrixx-spring-side',177)):
    ms=[mask_of(f) for f in load(path,n)]
    te=[topedge(m) for m in ms]
    print('==',lab)
    print('   BODY-SHAPE rate = mean |top-edge move| per frame, x170-264 (the S itself, life wave cancels)')
    for nm,lo,hi in BEATS:
        v=[]
        for i in range(lo+1,hi+1):
            d=np.abs(te[i][170:265]-te[i-1][170:265]); d=d[~np.isnan(d)]
            v.append(d.mean())
        print(f'     {nm:7s} f{lo:3d}-{hi:3d}  {np.mean(v):6.3f} px/f')
    print('   HEAD-REGION centroid speed (px/f) and NOSE speed (px/f)')
    cols=np.arange(384)
    hy=[];nx=[];ny=[]
    for m in ms:
        t=m.copy(); t[:,~(cols>=210)]=False
        ys,xs=np.nonzero(t); hy.append(ys.mean() if len(ys) else np.nan)
        ys,xs=np.nonzero(m); nx.append(xs.max()); ny.append(ys[xs==xs.max()].mean())
    hy=np.array(hy);nx=np.array(nx,float);ny=np.array(ny)
    for nm,lo,hi in BEATS:
        sp=np.nanmean(np.abs(np.diff(hy[lo:hi+1])))
        nsp=np.nanmean(np.hypot(np.diff(nx[lo:hi+1]),np.diff(ny[lo:hi+1])))
        print(f'     {nm:7s} head-c {sp:6.3f}   nose {nsp:6.3f}')
    print(f'   LAUNCH: body centroid y f168..176: '+' '.join(f'{np.nanmean(np.nonzero(m)[0]):.0f}' for m in ms[168:177]))
    ar=[m.sum() for m in ms]
    print(f'   LAUNCH: silhouette area f168..176: '+' '.join(str(a) for a in ar[168:177]))
