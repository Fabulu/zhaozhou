import os,numpy as np
from scipy import ndimage
from PIL import Image
W,H=384,240
def load1(p,i):
    r=np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8); w,h=r[:8].view(np.uint32)
    return r[8:].reshape(int(h),int(w),3)
def mask_of(f):
    f=f.astype(np.int16); R,G,B=f[:,:,0],f[:,:,1],f[:,:,2]; lum=R+G+B
    m=(lum<130)|(G>R+18)|(B>R+18)|(lum>470)|((R>150)&(G>130)&(B<110))
    st=np.ones((3,3),bool); m=ndimage.binary_closing(m,st); m=ndimage.binary_closing(m,st)
    m=ndimage.binary_fill_holes(m); lab,n=ndimage.label(m)
    s=ndimage.sum(m,lab,range(1,n+1)); return lab==int(np.argmax(s))+1
def outline(m):
    return m & ~ndimage.binary_erosion(m,np.ones((3,3),bool))
def plate(path,frames,colors,title):
    img=np.full((H,W,3),245,np.uint8)
    for fi,c in zip(frames,colors):
        o=outline(mask_of(load1(path,fi))); img[o]=c
    return img
S=3
outs=[]
for path,lab,gsz in (('qa-renders-prev/zixxtrixx-spring-side','PREV',160),('qa-renders/zixxtrixx-spring-side','HEAD',168)):
    p=plate(path,[12,72,144],[(210,60,60),(220,170,20),(40,90,200)],lab)
    outs.append(p)
comb=np.concatenate(outs,axis=0)
comb=np.kron(comb,np.ones((S,S,1),np.uint8))
Image.fromarray(comb).save('qa-outline-overlay.png')
# mid-body crop strip through beat 1, HEAD vs PREV
def crop_strip(path,frames,x0=150,x1=240,y0=95,y1=155):
    tiles=[]
    for fi in frames:
        f=load1(path,fi)[y0:y1,x0:x1]
        tiles.append(f)
    return np.concatenate(tiles,axis=1)
fr=[12,24,36,48,60,72,84,108,132,144]
a=crop_strip('qa-renders-prev/zixxtrixx-spring-side',fr)
b=crop_strip('qa-renders/zixxtrixx-spring-side',fr)
sep=np.full((4,a.shape[1],3),255,np.uint8)
strip=np.concatenate([a,sep,b],axis=0)
strip=np.kron(strip,np.ones((4,4,1),np.uint8))
Image.fromarray(strip).save('qa-midbody-strip.png')
print('ok', comb.shape, strip.shape)
