import sys, os
sys.path.insert(0, r"C:\programmieren\zencrifice\zixxtrixx-wholebody-s-spring-20260901\zhaozhou\tools\reel")
import numpy as np
from rgbframe import load
from PIL import Image

def tile(paths, cols, scale=1, crop=None, labels=None, gap=2):
    ims=[]
    for p in paths:
        a=load(p)
        if crop: y0,y1,x0,x1=crop; a=a[y0:y1,x0:x1]
        im=Image.fromarray(a.astype(np.uint8))
        if scale!=1: im=im.resize((im.width*scale, im.height*scale), Image.NEAREST)
        ims.append(im)
    w,h=ims[0].size
    rows=(len(ims)+cols-1)//cols
    out=Image.new("RGB",(cols*w+(cols-1)*gap, rows*h+(rows-1)*gap),(255,0,255))
    for i,im in enumerate(ims):
        out.paste(im,((i%cols)*(w+gap),(i//cols)*(h+gap)))
    return out

if __name__=="__main__":
    outp=sys.argv[1]; cols=int(sys.argv[2]); scale=int(sys.argv[3])
    crop=None
    if sys.argv[4]!="-":
        crop=tuple(int(x) for x in sys.argv[4].split(","))
    tile(sys.argv[5:], cols, scale, crop).save(outp)
    print("wrote", outp)
