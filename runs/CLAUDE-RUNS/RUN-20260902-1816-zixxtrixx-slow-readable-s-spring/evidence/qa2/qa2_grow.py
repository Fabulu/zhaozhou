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
print('UPLIFT AREA = silhouette pixels ABOVE the f12 rest top-edge, per band (px)')
print('  a rising number IS the shape growing there; flat IS "it never grows"')
for lab,path in (('PREV 48310d18','qa2-renders-prev/zixxtrixx-spring-side'),('HEAD d5949320','qa2-renders/zixxtrixx-spring-side')):
    ms={f:mask_of(load1(path,f)) for f in (12,24,36,48,60,72,84,96,108,120,132,144,168)}
    rest=ms[12]
    top=np.full(384,240)
    for x in range(384):
        ys=np.nonzero(rest[:,x])[0]
        if len(ys): top[x]=ys.min()
    bands={'tail x105-169':(105,170),'mid x170-209':(170,210),'front x210-264':(210,265)}
    print('==',lab)
    for bn,(a,b) in bands.items():
        vals=[]
        for f in sorted(ms):
            m=ms[f]; s=0
            for x in range(a,b):
                ys=np.nonzero(m[:,x])[0]
                if len(ys): s+=int((ys<top[x]).sum())
            vals.append((f,s))
        print(f'  {bn:16s} '+' '.join(f'f{f}:{v}' for f,v in vals))
