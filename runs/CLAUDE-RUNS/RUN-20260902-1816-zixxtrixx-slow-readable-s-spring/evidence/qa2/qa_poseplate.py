import os,numpy as np
from PIL import Image
def load1(p,i):
    r=np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8); w,h=r[:8].view(np.uint32)
    return r[8:].reshape(int(h),int(w),3)
x0,x1,y0,y1=100,270,90,165
S=7
def col(path,frames):
    ts=[load1(path,f)[y0:y1,x0:x1] for f in frames]
    sep=np.full((3,x1-x0,3),255,np.uint8)
    out=[]
    for t in ts: out+=[t,sep]
    return np.concatenate(out[:-1],axis=0)
fr=[82,104,124,144]
a=col('qa-renders-prev/zixxtrixx-spring-side',fr)
b=col('qa-renders/zixxtrixx-spring-side',fr)
sep=np.full((a.shape[0],4,3),255,np.uint8)
img=np.concatenate([a,sep,b],axis=1)
Image.fromarray(np.kron(img,np.ones((S,S,1),np.uint8))).save('qa-beat2-prev-vs-head.png')
print('ok')
