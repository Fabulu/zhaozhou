"""Review plate maker. Frames read ONLY through tools/reel/rgbframe.py."""
import os, sys, glob
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-p15-review\zhaozhou\tools\reel")
import numpy as np
from rgbframe import load
from PIL import Image, ImageDraw

REEL = r"C:\programmieren\zencrifice\manafold-p12-fix\Upheaval\website\scratch-reel"

def frames(subj):
    return sorted(glob.glob(os.path.join(REEL, "manafold-" + subj, "*.rgb")))

def label(img, text, xy=(2,2), fill=(255,255,0), bg=(0,0,0)):
    d = ImageDraw.Draw(img)
    w = 6*len(text)+4
    d.rectangle([xy[0], xy[1], xy[0]+w, xy[1]+12], fill=bg)
    d.text((xy[0]+2, xy[1]+2), text, fill=fill)
    return img

def sheet(subj, out, cols=8, step=1, scale=1, lo=0, hi=10**9, title=None):
    fs = [f for f in frames(subj) if lo <= int(os.path.basename(f)[:-4]) <= hi][::step]
    tw, th = 384*scale, 240*scale
    rows = (len(fs)+cols-1)//cols
    sheetimg = Image.new("RGB", (cols*tw, rows*th + 16), (20,20,24))
    for i, f in enumerate(fs):
        a = load(f)
        im = Image.fromarray(np.ascontiguousarray(a), "RGB")
        if scale != 1: im = im.resize((tw, th), Image.NEAREST)
        n = os.path.basename(f)[:-4]
        label(im, "f"+n)
        sheetimg.paste(im, ((i%cols)*tw, (i//cols)*th + 16))
    label(sheetimg, title or f"{subj}  {len(fs)} tiles  step={step}  scale={scale}", (2,2), (255,255,255))
    sheetimg.save(out)
    print(f"{out}  {len(fs)} tiles  {sheetimg.size}")

def crop(subj, n, out, box, scale=4, title=None):
    f = os.path.join(REEL, "manafold-"+subj, f"{int(n):04d}.rgb")
    a = load(f)
    x,y,w,h = box
    x=max(0,x); y=max(0,y); w=min(w,384-x); h=min(h,240-y)
    im = Image.fromarray(np.ascontiguousarray(a[y:y+h, x:x+w]), "RGB")
    im = im.resize((w*scale, h*scale), Image.NEAREST)
    out_im = Image.new("RGB", (im.size[0], im.size[1]+16), (20,20,24))
    out_im.paste(im, (0,16))
    label(out_im, title or f"{subj} f{int(n):04d} crop x{x} y{y} {w}x{h} @{scale}x", (2,2), (255,255,255))
    out_im.save(out)
    print(f"{out}  {out_im.size}")

def strip(subj, ns, out, box=None, scale=3, title=None):
    ims=[]
    for n in ns:
        f = os.path.join(REEL, "manafold-"+subj, f"{int(n):04d}.rgb")
        a = load(f)
        if box:
            x,y,w,h = box; a = a[y:y+h, x:x+w]
        im = Image.fromarray(np.ascontiguousarray(a), "RGB")
        im = im.resize((im.size[0]*scale, im.size[1]*scale), Image.NEAREST)
        label(im, f"f{int(n):04d}")
        ims.append(im)
    W = sum(i.size[0] for i in ims); H = max(i.size[1] for i in ims)
    o = Image.new("RGB", (W, H+16), (20,20,24)); x=0
    for i in ims: o.paste(i,(x,16)); x+=i.size[0]
    label(o, title or f"{subj} strip", (2,2), (255,255,255))
    o.save(out); print(f"{out} {o.size}")

if __name__ == "__main__":
    import json
    cmd = sys.argv[1]
    if cmd == "sheet":
        sheet(sys.argv[2], sys.argv[3], **json.loads(sys.argv[4]) if len(sys.argv)>4 else {})
    elif cmd == "crop":
        crop(sys.argv[2], sys.argv[3], sys.argv[4], json.loads(sys.argv[5]), **(json.loads(sys.argv[6]) if len(sys.argv)>6 else {}))
    elif cmd == "strip":
        strip(sys.argv[2], json.loads(sys.argv[3]), sys.argv[4], **(json.loads(sys.argv[5]) if len(sys.argv)>5 else {}))
