import sys, os, glob
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-p15-review\zhaozhou\tools\reel")
import numpy as np
from rgbframe import load
from PIL import Image, ImageDraw
REEL = r"C:\programmieren\zencrifice\manafold-p12-fix\Upheaval\website\scratch-reel"
subs = sorted(os.path.basename(d)[9:] for d in glob.glob(os.path.join(REEL,"manafold-*")))
frac = float(sys.argv[2]) if len(sys.argv)>2 else 0.5
scale = int(sys.argv[3]) if len(sys.argv)>3 else 2
cols=6
tw,th=384*scale,240*scale
rows=(len(subs)+cols-1)//cols
o=Image.new("RGB",(cols*tw,rows*(th+16)),(20,20,24))
d=ImageDraw.Draw(o)
for i,s in enumerate(subs):
    fs=sorted(glob.glob(os.path.join(REEL,"manafold-"+s,"*.rgb")))
    f=fs[int(len(fs)*frac)]
    a=load(f)
    im=Image.fromarray(np.ascontiguousarray(a),"RGB").resize((tw,th),Image.NEAREST)
    x,y=(i%cols)*tw,(i//cols)*(th+16)
    o.paste(im,(x,y+16))
    d.rectangle([x,y,x+tw,y+15],fill=(0,0,0))
    d.text((x+3,y+3), f"{s}  f{os.path.basename(f)[:-4]}", fill=(255,255,120))
o.save(sys.argv[1]); print(sys.argv[1], o.size, len(subs),"subjects")
