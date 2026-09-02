import os,numpy as np
from scipy import ndimage
from PIL import Image
def load1(p,i):
    r=np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8); w,h=r[:8].view(np.uint32)
    return r[8:].reshape(int(h),int(w),3)
def mask_of(f):
    f=f.astype(np.int16); R,G,B=f[:,:,0],f[:,:,1],f[:,:,2]; lum=R+G+B
    m=(lum<130)|(G>R+18)|(B>R+18)|(lum>470)|((R>150)&(G>130)&(B<110))
    st=np.ones((3,3),bool); m=ndimage.binary_closing(m,st); m=ndimage.binary_closing(m,st)
    m=ndimage.binary_fill_holes(m); lab,n=ndimage.label(m)
    s=ndimage.sum(m,lab,range(1,n+1)); return lab==int(np.argmax(s))+1
def outline(m): return m & ~ndimage.binary_erosion(m,np.ones((3,3),bool))
x0,x1,y0,y1=100,265,90,155
S=6
rows=[]
for path in ('qa2-renders-prev/zixxtrixx-spring-side','qa2-renders/zixxtrixx-spring-side'):
    img=np.full((y1-y0,x1-x0,3),245,np.uint8)
    for fi,c in ((12,(210,60,60)),(72,(220,170,20)),(144,(40,90,200))):
        o=outline(mask_of(load1(path,fi)))[y0:y1,x0:x1]; img[o]=c
    rows.append(img)
comb=np.concatenate([rows[0],np.full((3,x1-x0,3),120,np.uint8),rows[1]],axis=0)
Image.fromarray(np.kron(comb,np.ones((S,S,1),np.uint8))).save(os.path.join(os.path.dirname(__file__),'body-outline-overlay-qa2.png'))
# native-resolution top-edge profile of the mid/rear body through beat 1
print('MID-BODY TOP EDGE (native px, y down): columns 150..215')
for lab,path in (('PREV','qa2-renders-prev/zixxtrixx-spring-side'),('HEAD','qa2-renders/zixxtrixx-spring-side')):
    print(' ',lab)
    for fi in (12,36,48,60,72,96,120,144):
        m=mask_of(load1(path,fi))
        tops=[]
        for x in range(150,216,5):
            ys=np.nonzero(m[:,x])[0]
            tops.append(ys.min() if len(ys) else 999)
        print(f'   f{fi:3d} top-edge y: '+' '.join(f'{t:3d}' for t in tops)+f'   min {min(tops)}')
