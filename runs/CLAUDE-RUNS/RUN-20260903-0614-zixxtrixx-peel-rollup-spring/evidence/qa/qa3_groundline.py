import os,sys
import numpy as np
from PIL import Image
sys.path.insert(0,'.')
from _maskfns import load, mask_of
d=sys.argv[1]; out=sys.argv[2]
frames=[int(x) for x in sys.argv[3].split(',')]
S=int(sys.argv[4]); x0,x1,y0,y1=(int(v) for v in sys.argv[5].split(','))
fr=load(d); ms=[mask_of(f) for f in fr]
W=ms[0].shape[1]
def bot(m):
    b=np.full(W,-1,int)
    for x in range(W):
        c=np.nonzero(m[:,x])[0]
        if len(c): b[x]=c[-1]
    return b
b0=bot(ms[0]); GY=int(b0.max())
tiles=[]
for f in frames:
    img=np.array(fr[f],dtype=np.uint8).copy()
    b=bot(ms[f])
    # calibrated ground line in cyan
    img[GY,x0:x1]=[0,255,255]
    # touching columns in red on the row above
    for x in range(x0,x1):
        if b[x]>=GY-1: img[GY-1,x]=[255,0,0]
    tiles.append(img[y0:y1,x0:x1])
sep=np.full((y1-y0,2,3),255,np.uint8)
o=[]
for t in tiles: o+=[t,sep]
im=np.concatenate(o[:-1],axis=1)
Image.fromarray(np.kron(im,np.ones((S,S,1),np.uint8))).save(out)
print(out,"groundline y=",GY,"frames",frames)
