import os,numpy as np
from scipy import ndimage
def load1(p,i):
    r=np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8); w,h=r[:8].view(np.uint32)
    return r[8:].reshape(int(h),int(w),3)
def mask_of(f):
    f=f.astype(np.int16); R,G,B=f[:,:,0],f[:,:,1],f[:,:,2]; lum=R+G+B
    m=(lum<130)|(G>R+18)|(B>R+18)|(lum>470)|((R>150)&(G>130)&(B<110))
    st=np.ones((3,3),bool); m=ndimage.binary_closing(m,st); m=ndimage.binary_closing(m,st)
    m=ndimage.binary_fill_holes(m); lab,n=ndimage.label(m)
    s=ndimage.sum(m,lab,range(1,n+1)); return lab==int(np.argmax(s))+1
print('TOP VIEW: perpendicular thickness of the silhouette about its own long axis')
print('(a constant number across the arming = the tail keeps its BUILT lateral spread; a growing one = a new helix)')
for lab,path in (('PREV 48310d18','qa-renders/zixxtrixx-spring-top'),('HEAD d5949320','qa2-renders/zixxtrixx-spring-top')):
    if not os.path.isdir(path): print(' ',lab,'(no render)'); continue
    print(' ',lab)
    for fi in (0,12,36,60,72,96,120,144,168):
        m=mask_of(load1(path,fi))
        ys,xs=np.nonzero(m)
        pts=np.stack([xs-xs.mean(),ys-ys.mean()])
        u,s,vt=np.linalg.svd(pts@pts.T/len(xs))
        proj=(u[:,1]@pts)   # minor axis = perpendicular spread
        print(f'   f{fi:3d}  perp span {proj.max()-proj.min():6.2f} px   perp sd {proj.std():5.2f}   area {m.sum()}')
