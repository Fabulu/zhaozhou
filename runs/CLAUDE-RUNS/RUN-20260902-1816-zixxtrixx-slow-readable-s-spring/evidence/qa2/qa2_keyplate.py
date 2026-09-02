import os,sys,numpy as np
from PIL import Image
def load1(p,i):
    r=np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8); w,h=r[:8].view(np.uint32)
    return r[8:].reshape(int(h),int(w),3)
x0,x1,y0,y1=95,275,88,168
S=int(sys.argv[3]) if len(sys.argv)>3 else 5
frames=[int(x) for x in sys.argv[2].split(',')]
def row(path,frames):
    ts=[load1(path,f)[y0:y1,x0:x1] for f in frames]
    sep=np.full((y1-y0,3,3),255,np.uint8)
    out=[]
    for t in ts: out+=[t,sep]
    return np.concatenate(out[:-1],axis=1)
a=row('qa2-renders-prev/zixxtrixx-spring-side',frames)
b=row('qa2-renders/zixxtrixx-spring-side',frames)
sep=np.full((4,a.shape[1],3),255,np.uint8)
img=np.concatenate([a,sep,b],axis=0)
Image.fromarray(np.kron(img,np.ones((S,S,1),np.uint8))).save(sys.argv[1])
print(sys.argv[1],'top=PREV 48310d18  bottom=HEAD d5949320  frames',frames)
