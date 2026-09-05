import sys, os
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\tools\reel")
import numpy as np
from PIL import Image, ImageDraw
from rgbframe import load
OUT = r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\runs\CLAUDE-RUNS\RUN-20260905-1805-manafold-pass5-by-eye-review\plates"
VAR = [("exp-A-ship","A SHIPPING  halo 7-10px, 24 motes, stencil 300"),
       ("exp-F-bigmote","F BIGGER MOTES  halo 11-15  (the literal ask)"),
       ("exp-B-smallmote","B SMALLER MOTES  halo 4-6"),
       ("exp-E-small40","E SMALLER + MORE  halo 4-6, 40 motes"),
       ("exp-C-bigstencil","C BIGGER STENCIL  300 -> 470 mm"),
       ("exp-D-closecam","D CLOSER CAMERA  cam_k 240k -> 360k"),
       ("exp-G-combo","G COMBO  halo 4-6, 34 motes, stencil 430"),
       ]
def build(frame, out, scale, box=None):
    tiles=[]
    for d,lab in VAR:
        a=load(os.path.join(d,"manafold-curious","%04d.rgb"%frame))
        if box: a=a[box[1]:box[3],box[0]:box[2]]
        tiles.append((lab,a))
    h,w=tiles[0][1].shape[:2]
    cols=4; rows=(len(tiles)+cols-1)//cols
    cv=Image.new("RGB",(cols*(w*scale+6), rows*(h*scale+20)),(16,16,22))
    d=ImageDraw.Draw(cv)
    for n,(lab,a) in enumerate(tiles):
        im=Image.fromarray(np.ascontiguousarray(a),"RGB").resize((w*scale,h*scale),Image.NEAREST)
        x=(n%cols)*(w*scale+6); y=(n//cols)*(h*scale+20)
        cv.paste(im,(x,y+20)); d.text((x+3,y+4),lab,fill=(250,235,140))
    cv.save(out); print(out, cv.size)
