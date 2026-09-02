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
for lab,p,n,bounds in (('HEAD','qa-renders/zixxtrixx-spring-side',180,[(0,12,'settle'),(12,72,'beat1'),(72,82,'dwell'),(82,144,'beat2'),(144,168,'hold'),(168,175,'launch')]),
                       ('PREV','qa-renders-prev/zixxtrixx-spring-side',172,[(0,12,'settle'),(12,72,'beat1'),(72,82,'dwell'),(82,144,'beat2'),(144,160,'hold'),(160,168,'launch')])):
    fr=load(p,n); ms=[mask_of(f) for f in fr]
    xor=[0.0]+[100.0*(ms[i]^ms[i-1]).sum()/max((ms[i]|ms[i-1]).sum(),1) for i in range(1,len(ms))]
    print(f"=== {lab} silhouette XOR %/frame")
    for a,b,nm in bounds:
        seg=xor[a+1:b+1]
        if seg: print(f"  {nm:7s} f{a:3d}-{b:3d}  mean {np.mean(seg):5.2f}  max {np.max(seg):5.2f}")
