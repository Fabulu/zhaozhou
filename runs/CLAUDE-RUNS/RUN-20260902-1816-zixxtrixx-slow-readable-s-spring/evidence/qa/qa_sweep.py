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
M={}
for lab,p in (('PREV','qa-renders-prev/zixxtrixx-spring-side'),('HEAD','qa-renders/zixxtrixx-spring-side')):
    M[lab]=[mask_of(f) for f in load(p,150)]
cols=np.arange(384)
for split in ((170,210),(165,205),(175,215),(160,200),(180,220)):
    for win in ((12,72),(8,72),(0,72),(4,36*2),(12,80)):
        line=f"split{split} win{win}: "
        for lab in ('PREV','HEAD'):
            ms=M[lab]; lo,hi=win
            regs=[cols<split[0],(cols>=split[0])&(cols<split[1]),cols>=split[1]]
            tot=[sum(int((ms[i]^ms[i-1])[:,s].sum()) for i in range(lo+1,hi+1)) for s in regs]
            T=max(sum(tot),1)
            line+=f"{lab} {100*tot[0]/T:4.1f}/{100*tot[1]/T:4.1f}/{100*tot[2]/T:4.1f}   "
        print(line)
