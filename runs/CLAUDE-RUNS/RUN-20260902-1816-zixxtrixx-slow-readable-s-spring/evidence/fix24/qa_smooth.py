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
for lab,p in (('PREV','qa-renders-prev/zixxtrixx-spring-side'),('NEW','qa-renders/zixxtrixx-spring-side')):
    ms=[mask_of(f) for f in load(p,176)]
    ar=[m.sum() for m in ms]
    xor=[100.0*(ms[i]^ms[i-1]).sum()/ar[i] for i in range(1,169)]
    xg=np.array(xor)
    print(lab,'ground f1-168 sil-XOR med %.2f max %.2f %%/f (argmax f%d)'%(np.median(xg),xg.max(),1+int(np.argmax(xg))))
    # beat rates
    for nm,lo,hi in (('settle',1,12),('beat1',12,72),('dwell',72,82),('beat2',82,144),('hold',144,168),('launch',168,175)):
        seg=np.array([100.0*(ms[i]^ms[i-1]).sum()/ar[i] for i in range(lo+1,hi+1)])
        print('   %s f%d-%d  %.2f %%/f'%(nm,lo,hi,seg.mean()))
    # nose tip: rightmost body pixel
    nx=[];ny=[]
    for m in ms[:169]:
        ys,xs=np.nonzero(m); i=np.argmax(xs)
        nx.append(xs.max()); ny.append(ys[xs==xs.max()].mean())
    nx=np.array(nx);ny=np.array(ny)
    idx=[0,12,36,72,108,144,168]
    print('   nose x:'," ".join("f%d:%d"%(i,nx[i]) for i in idx),' back=%d px'%(nx[168]-nx[12]))
    print('   nose y:'," ".join("f%d:%.0f"%(i,ny[i]) for i in idx),' down=%.0f px'%(ny[168]-ny[12]))
    print('   rearmost x min:',min(np.nonzero(m)[1].min() for m in ms[:169]),' nose min:',nx.min())
