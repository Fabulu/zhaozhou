import os,numpy as np
from PIL import Image
def load1(p,i):
    r=np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8); w,h=r[:8].view(np.uint32)
    return r[8:].reshape(int(h),int(w),3)
x0,x1,y0,y1=90,290,80,180
S=4
def row(path,frames):
    ts=[load1(path,f)[y0:y1,x0:x1] for f in frames]
    sep=np.full((y1-y0,3,3),255,np.uint8)
    out=[]
    for t in ts: out+=[t,sep]
    return np.concatenate(out[:-1],axis=1)
fr=[0,36,72,108,144,168]
a=row('qa2-renders/zixxtrixx-spring-top',fr)
Image.fromarray(np.kron(a,np.ones((S,S,1),np.uint8))).save(os.path.join(os.path.dirname(__file__),'planarity-top-qa2.png'))
print('top view frames',fr)
