import sys, os
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\tools\reel")
import numpy as np
from PIL import Image, ImageDraw
from rgbframe import load
R = r"C:\programmieren\zencrifice\manafold-pass5-review\renders"
OUT = r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\runs\CLAUDE-RUNS\RUN-20260905-1805-manafold-pass5-by-eye-review\plates"
def f(c,i): return load(os.path.join(R,c,"%04d.rgb"%i))
def grid(items, out, scale=3, box=(105,25,285,195), cols=4):
    tiles=[]
    for c,i,lab in items:
        a=f(c,i)[box[1]:box[3], box[0]:box[2]]
        tiles.append((lab,a))
    h,w=tiles[0][1].shape[:2]
    rows=(len(tiles)+cols-1)//cols
    cv=Image.new("RGB",(cols*(w*scale+4), rows*(h*scale+18)),(18,18,24))
    d=ImageDraw.Draw(cv)
    for n,(lab,a) in enumerate(tiles):
        im=Image.fromarray(np.ascontiguousarray(a),"RGB").resize((w*scale,h*scale),Image.NEAREST)
        x=(n%cols)*(w*scale+4); y=(n//cols)*(h*scale+18)
        cv.paste(im,(x,y+18)); d.text((x+3,y+3),lab,fill=(240,230,140))
    cv.save(out); print(out, cv.size)
