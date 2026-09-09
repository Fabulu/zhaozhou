import sys, os
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-p15-review\zhaozhou\tools\reel")
import numpy as np
from rgbframe import load
from PIL import Image, ImageDraw
def lab(im,t):
    d=ImageDraw.Draw(im); d.rectangle([0,0,6*len(t)+6,14],fill=(0,0,0)); d.text((3,2),t,fill=(255,255,120)); return im
def show(paths, titles, out, box=None, scale=3):
    ims=[]
    for p,t in zip(paths,titles):
        a=load(p)
        if box: x,y,w,h=box; a=a[y:y+h,x:x+w]
        im=Image.fromarray(np.ascontiguousarray(a),"RGB")
        im=im.resize((im.size[0]*scale,im.size[1]*scale),Image.NEAREST)
        ims.append(lab(im,t))
    W=sum(i.size[0] for i in ims); H=max(i.size[1] for i in ims)
    o=Image.new("RGB",(W,H),(20,20,24)); x=0
    for i in ims: o.paste(i,(x,0)); x+=i.size[0]
    o.save(out); print(out,o.size)
if __name__=="__main__":
    import json
    show(json.loads(sys.argv[1]), json.loads(sys.argv[2]), sys.argv[3], json.loads(sys.argv[4]) if len(sys.argv)>4 else None, int(sys.argv[5]) if len(sys.argv)>5 else 3)
