import os,sys
import numpy as np
from PIL import Image
sys.path.insert(0,'.')
from _maskfns import load, mask_of
from scipy import ndimage
d='renders/zixxtrixx-spring-side'
fr=load(d); ms=[mask_of(f) for f in fr]
X0,X1,Y0,Y1=112,165,110,155
cols=[(255,60,60),(255,235,60),(60,140,255)]
keys=[57,86,115]
img=np.zeros((Y1-Y0,X1-X0,3),np.uint8)
img[:]= (40,30,30)
print("tail band x=%d..%d, sole-contact frames %s"%(X0,X1,keys))
for c,f in zip(cols,keys):
    m=ms[f][Y0:Y1,X0:X1]
    e=m & ~ndimage.binary_erosion(m,np.ones((3,3),bool))
    img[e]=c
    ys,xs=np.nonzero(m)
    print("  f%-4d area=%4d  bbox x %d..%d  y %d..%d  w=%d h=%d  centroid %.2f,%.2f  bottom-most x=%s"%(
        f,m.sum(),xs.min()+X0,xs.max()+X0,ys.min()+Y0,ys.max()+Y0,xs.max()-xs.min()+1,ys.max()-ys.min()+1,
        xs.mean()+X0,ys.mean()+Y0, sorted(set(xs[ys==ys.max()]+X0))))
Image.fromarray(np.kron(img,np.ones((10,10,1),np.uint8))).save('tail-anchor-outlines.png')
print("wrote tail-anchor-outlines.png  red=f57 yellow=f86 blue=f115")
# per-frame tail metrics across sole contact
print()
print("%5s %6s %6s %6s %6s %8s %8s"%("f","area","xmin","xmax","ymax","cx","cy"))
for f in range(55,121,5):
    m=ms[f][Y0:Y1,X0:X1]; ys,xs=np.nonzero(m)
    print("%5d %6d %6d %6d %6d %8.2f %8.2f"%(f,m.sum(),xs.min()+X0,xs.max()+X0,ys.max()+Y0,xs.mean()+X0,ys.mean()+Y0))
