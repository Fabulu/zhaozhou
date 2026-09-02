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
fr=load('qa-renders/zixxtrixx-spring-side',169)
ms=[mask_of(f) for f in fr]
nx=[];ny=[]
for m in ms:
    ys,xs=np.nonzero(m); x=xs.max(); nx.append(x); ny.append(ys[xs==x].mean())
nx=np.array(nx,float); ny=np.array(ny)
print("nose x route (every 8 f):", " ".join(f"{v:.0f}" for v in nx[::8]))
print("nose y route (every 8 f):", " ".join(f"{v:.1f}" for v in ny[::8]))
d=np.diff(nx); print("nose-x steps nonzero:",[(i+1,int(v)) for i,v in enumerate(d) if v!=0])
dy=np.diff(ny)
sign=np.sign(dy); rev=sum(1 for i in range(1,len(sign)) if sign[i]!=0 and sign[i-1]!=0 and sign[i]!=sign[i-1])
print(f"nose-y sign reversals over ground phase: {rev}; max |step| {np.max(np.abs(dy)):.2f} px")
# head-region centroid
cols=np.arange(384); sel=cols>=210
hx=[];hy=[]
for m in ms:
    mm=m.copy(); mm[:,~sel]=False; ys,xs=np.nonzero(mm); hx.append(xs.mean()); hy.append(ys.mean())
hx=np.array(hx); hy=np.array(hy)
print(f"head-region centroid x: f0 {hx[0]:.2f} -> f168 {hx[-1]:.2f} = {hx[-1]-hx[0]:+.2f}px; max forward excursion {(hx.max()-hx[0]):+.2f}px at f{int(hx.argmax())}")
print(f"head-region centroid y: {hy[-1]-hy[0]:+.2f}px down")
print("total x path / net:", f"{np.abs(np.diff(hx)).sum()/abs(hx[-1]-hx[0]):.2f}x")
