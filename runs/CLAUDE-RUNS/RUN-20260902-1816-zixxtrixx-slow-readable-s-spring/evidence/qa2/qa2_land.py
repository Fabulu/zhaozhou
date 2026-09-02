import os,sys,numpy as np
from PIL import Image
def load1(p,i):
    r=np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8); w,h=r[:8].view(np.uint32)
    return r[8:].reshape(int(h),int(w),3)
x0,x1,y0,y1=60,330,70,200
S=3
fr=[248,252,256,260,264,268,274,282]
def row(p,fr): 
    ts=[load1(p,f)[y0:y1,x0:x1] for f in fr]
    sep=np.full((y1-y0,3,3),255,np.uint8); o=[]
    for t in ts: o+=[t,sep]
    return np.concatenate(o[:-1],axis=1)
img=row('qa2-renders/zixxtrixx-jump-one',fr)
Image.fromarray(np.kron(img,np.ones((S,S,1),np.uint8))).save(os.path.join(os.path.dirname(__file__),'jumpone-landing-qa2.png'))
print('landing frames',fr)
