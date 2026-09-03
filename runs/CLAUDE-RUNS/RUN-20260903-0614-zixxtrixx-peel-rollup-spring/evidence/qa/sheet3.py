import os,sys,numpy as np
from PIL import Image
p,out,lo,hi,cols,S=sys.argv[1],sys.argv[2],int(sys.argv[3]),int(sys.argv[4]),int(sys.argv[5]),int(sys.argv[6])
x0,x1,y0,y1=(int(v) for v in sys.argv[7].split(',')) if len(sys.argv)>7 else (95,290,62,176)
def load1(i):
    r=np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8); w,h=r[:8].view(np.uint32)
    return r[8:].reshape(int(h),int(w),3)
tiles=[load1(i)[y0:y1,x0:x1] for i in range(lo,hi+1)]
tw,th=x1-x0,y1-y0
rows=[]
for r in range(0,len(tiles),cols):
    chunk=tiles[r:r+cols]
    while len(chunk)<cols: chunk.append(np.zeros((th,tw,3),np.uint8))
    rows.append(np.concatenate([np.pad(c,((0,2),(0,2),(0,0)),constant_values=255) for c in chunk],axis=1))
img=np.concatenate(rows,axis=0)
Image.fromarray(np.kron(img,np.ones((S,S,1),np.uint8))).save(out)
print(out, img.shape, 'frames',lo,hi)
