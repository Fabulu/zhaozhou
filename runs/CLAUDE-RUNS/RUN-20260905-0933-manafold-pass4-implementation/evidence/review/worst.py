import sys, os, glob
sys.path.insert(0, r"C:\programmieren\zencrifice\zixxtrixx-wholebody-s-spring-20260901\zhaozhou\tools\reel")
import numpy as np
from rgbframe import load
from PIL import Image
sel=[]
for d in sys.argv[2:]:
    fs=sorted(glob.glob(os.path.join(d,"*.rgb")))
    best=(-1,None)
    for f in fs:
        a=load(f).astype(np.int32); r,g,b=a[:,:,0],a[:,:,1],a[:,:,2]
        m=(g>150)&(b>150)&(r<g-40)
        n=int(m.sum())
        if n>best[0]: best=(n,f)
    sel.append((os.path.basename(d),best[0],best[1]))
    print("%-24s worst-mana %5d %s"%(sel[-1][0],best[0],os.path.basename(best[1])))
ims=[Image.fromarray(load(f).astype(np.uint8)) for _,_,f in sel]
cols=4; w,h=ims[0].size; rows=(len(ims)+cols-1)//cols
out=Image.new("RGB",(cols*w+(cols-1)*2, rows*h+(rows-1)*2),(255,0,255))
for i,im in enumerate(ims): out.paste(im,((i%cols)*(w+2),(i//cols)*(h+2)))
out.save(sys.argv[1]); print("wrote",sys.argv[1])
