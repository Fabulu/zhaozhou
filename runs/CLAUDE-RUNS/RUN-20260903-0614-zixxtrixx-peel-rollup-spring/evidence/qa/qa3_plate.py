import os,sys
import numpy as np
from PIL import Image
sys.path.insert(0,'.')
from qa25_contactfront import load
out=sys.argv[1]; S=int(sys.argv[2]); x0,x1,y0,y1=(int(v) for v in sys.argv[3].split(','))
rows=[]
for spec in sys.argv[4:]:
    d,fs=spec.split('=')
    fr=[int(v) for v in fs.split(',')]
    ts=[load(d,f)[y0:y1,x0:x1] for f in fr]
    sep=np.full((y1-y0,3,3),255,np.uint8)
    o=[]
    for t in ts: o+=[t,sep]
    rows.append(np.concatenate(o[:-1],axis=1))
w=max(r.shape[1] for r in rows)
rows=[np.pad(r,((0,0),(0,w-r.shape[1]),(0,0))) for r in rows]
sep=np.full((4,w,3),255,np.uint8)
o=[]
for r in rows: o+=[r,sep]
img=np.concatenate(o[:-1],axis=0)
Image.fromarray(np.kron(img,np.ones((S,S,1),np.uint8))).save(out)
print(out)
