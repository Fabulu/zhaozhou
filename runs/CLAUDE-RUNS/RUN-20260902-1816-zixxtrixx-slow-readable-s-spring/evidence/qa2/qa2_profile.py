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
def prof(path,fi):
    m=mask_of(load1(path,fi)); out=np.full(384,np.nan)
    for x in range(384):
        ys=np.nonzero(m[:,x])[0]
        if len(ys): out[x]=ys.min()
    return out
for lab,path in (('PREV 48310d18','qa2-renders-prev/zixxtrixx-spring-side'),('HEAD d5949320','qa2-renders/zixxtrixx-spring-side')):
    a=prof(path,12); b=prof(path,72); c=prof(path,144)
    d=a-b   # positive = TOP EDGE ROSE from rest to assembled
    print('==',lab)
    print('  rest->assembled TOP-EDGE RISE (px, +=rose), by column band:')
    for lo in range(105,265,10):
        seg=d[lo:lo+10]; seg=seg[~np.isnan(seg)]
        if len(seg): print(f'    x{lo:3d}-{lo+9:3d}: mean {np.nanmean(seg):+5.1f}  max {np.nanmax(seg):+5.1f}')
    body=d[170:265]; body=body[~np.isnan(body)]
    print(f'  MID+FRONT (x170-264) mean rise {np.nanmean(body):+.2f} px, max {np.nanmax(body):+.1f} px')
    tail=d[105:170]; tail=tail[~np.isnan(tail)]
    print(f'  TAIL      (x105-169) mean rise {np.nanmean(tail):+.2f} px, max {np.nanmax(np.abs(tail)):+.1f} px abs')
    e=b-c
    print(f'  assembled->loaded fall, whole silhouette mean {-np.nanmean(e[~np.isnan(e)]):+.2f} px')
