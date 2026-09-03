import os,sys,numpy as np
from PIL import Image
p,out=sys.argv[1],sys.argv[2]
frames=[int(x) for x in sys.argv[3].split(',')]
x0,x1,y0,y1=(int(v) for v in sys.argv[4].split(','))
S=int(sys.argv[5])
def load1(i):
    r=np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8); w,h=r[:8].view(np.uint32)
    return r[8:].reshape(int(h),int(w),3)
ts=[load1(f)[y0:y1,x0:x1] for f in frames]
sep=np.full((y1-y0,2,3),255,np.uint8)
o=[]
for t in ts: o+=[t,sep]
img=np.concatenate(o[:-1],axis=1)
Image.fromarray(np.kron(img,np.ones((S,S,1),np.uint8))).save(out)
print(out,frames)
