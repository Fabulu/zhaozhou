import os,numpy as np
from scipy import ndimage
# TOP VIEW lateral spread.  NOTE: an earlier version of this probe segmented the
# whole frame and silently locked onto the static GROUND, printing an identical
# number for every frame of both builds.  It is fixed here by cropping to the
# creature's own bounding box (verified against a raw frame-difference bbox)
# BEFORE segmenting.  Recorded because a confident constant is exactly how this
# instrument lies.
X0,X1,Y0,Y1=100,250,85,150
def load1(p,i):
    r=np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8); w,h=r[:8].view(np.uint32)
    return r[8:].reshape(int(h),int(w),3)[Y0:Y1,X0:X1]
def creature(f):
    f=f.astype(np.int16); R,G,B=f[:,:,0],f[:,:,1],f[:,:,2]
    mx=f.max(axis=2); mn=f.min(axis=2); lum=R+G+B
    m=((mx-mn)>70)|(lum<110)
    st=np.ones((3,3),bool)
    m=ndimage.binary_closing(m,st); m=ndimage.binary_fill_holes(m)
    lab,n=ndimage.label(m)
    if n==0: return m
    s=ndimage.sum(m,lab,range(1,n+1))
    return lab==int(np.argmax(s))+1
print('TOP VIEW lateral (perpendicular-to-long-axis) spread, creature only')
for lab,path in (('PREV 48310d18','qa-renders/zixxtrixx-spring-top'),('HEAD d5949320','qa2-renders/zixxtrixx-spring-top')):
    print(' ',lab)
    for fi in (0,12,36,60,72,96,120,144,168):
        m=creature(load1(path,fi))
        ys,xs=np.nonzero(m)
        pts=np.stack([xs-xs.mean(),ys-ys.mean()]).astype(float)
        u,s,vt=np.linalg.svd(pts@pts.T/len(xs))
        proj=(u[:,1]@pts)
        print(f'   f{fi:3d}  lateral span {proj.max()-proj.min():6.2f} px   sd {proj.std():5.2f}   area {m.sum():5d}')
