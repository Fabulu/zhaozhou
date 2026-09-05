import sys, os
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\tools\reel")
import numpy as np
from PIL import Image, ImageDraw
from rgbframe import load
OUT = r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\runs\CLAUDE-RUNS\RUN-20260905-1805-manafold-pass5-by-eye-review\plates"
def f(root,c,i): return load(os.path.join(root,c,"%04d.rgb"%i))
def pairs(items, out, scale=3, box=(105,25,285,195)):
    # items: list of (clip, frame, label). Renders base row then head row.
    cols=len(items)
    tiles=[]
    for root,tag in [("renders-base","BASE (pre-pass-5)"),("renders","HEAD (pass 5)")]:
        for c,i,lab in items:
            a=f(root,c,i)[box[1]:box[3], box[0]:box[2]]
            tiles.append((tag+" "+lab,a))
    h,w=tiles[0][1].shape[:2]
    cv=Image.new("RGB",(cols*(w*scale+4), 2*(h*scale+18)),(18,18,24))
    d=ImageDraw.Draw(cv)
    for n,(lab,a) in enumerate(tiles):
        im=Image.fromarray(np.ascontiguousarray(a),"RGB").resize((w*scale,h*scale),Image.NEAREST)
        x=(n%cols)*(w*scale+4); y=(n//cols)*(h*scale+18)
        cv.paste(im,(x,y+18)); d.text((x+3,y+3),lab,fill=(240,230,140))
    cv.save(out); print(out, cv.size)
