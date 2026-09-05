"""Plate maker for the by-eye review. Uses the committed rgbframe reader."""
import os, sys, glob
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\tools\reel")
import numpy as np
from PIL import Image, ImageDraw
from rgbframe import load

RENDERS = r"C:\programmieren\zencrifice\manafold-pass5-review\renders"
OUT = r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\runs\CLAUDE-RUNS\RUN-20260905-1805-manafold-pass5-by-eye-review\plates"

def frames(clip):
    return sorted(glob.glob(os.path.join(RENDERS, clip, "*.rgb")))

def f(clip, i):
    return load(os.path.join(RENDERS, clip, "%04d.rgb" % i))

def sheet(clip, out, cols=16, scale=1, idxs=None, label=True):
    fs = frames(clip)
    if idxs is None: idxs = range(len(fs))
    idxs = list(idxs)
    ims = [load(fs[i]) for i in idxs]
    h, w = ims[0].shape[:2]
    tw, th = w*scale//1, h*scale//1
    tw = int(w*scale); th = int(h*scale)
    rows = (len(ims)+cols-1)//cols
    pad = 12 if label else 1
    canvas = Image.new("RGB", (cols*(tw+2), rows*(th+2+pad)), (20,20,26))
    d = ImageDraw.Draw(canvas)
    for n,(i,a) in enumerate(zip(idxs, ims)):
        im = Image.fromarray(np.ascontiguousarray(a),"RGB")
        if scale != 1: im = im.resize((tw,th), Image.LANCZOS if scale<1 else Image.NEAREST)
        x = (n%cols)*(tw+2); y=(n//cols)*(th+2+pad)
        canvas.paste(im,(x,y+pad))
        if label: d.text((x+2,y), "f%d"%i, fill=(230,230,120))
    canvas.save(out)
    print(out, canvas.size, len(ims), "frames")

def crop(clip, i, box, out, scale=4):
    a = f(clip,i)
    x0,y0,x1,y1 = box
    im = Image.fromarray(np.ascontiguousarray(a[y0:y1,x0:x1]),"RGB")
    im = im.resize(((x1-x0)*scale,(y1-y0)*scale), Image.NEAREST)
    im.save(out); print(out, im.size)

def row(clip, idxs, out, scale=2, box=None):
    ims=[]
    for i in idxs:
        a=f(clip,i)
        if box: x0,y0,x1,y1=box; a=a[y0:y1,x0:x1]
        ims.append(a)
    h,w=ims[0].shape[:2]
    canvas=Image.new("RGB",(len(ims)*(w*scale+4), h*scale+16),(20,20,26))
    d=ImageDraw.Draw(canvas)
    for n,(i,a) in enumerate(zip(idxs,ims)):
        im=Image.fromarray(np.ascontiguousarray(a),"RGB").resize((w*scale,h*scale),Image.NEAREST)
        canvas.paste(im,(n*(w*scale+4),16)); d.text((n*(w*scale+4)+2,2),"f%d"%i,fill=(230,230,120))
    canvas.save(out); print(out,canvas.size)

if __name__ == "__main__":
    pass
