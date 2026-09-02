import os,numpy as np
from scipy import ndimage
def l1(p,i): return np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8)[8:].reshape(240,384,3).astype(np.int16)
def mask_of(f):
    R,G,B=f[:,:,0],f[:,:,1],f[:,:,2]; lum=R+G+B
    m=(lum<130)|(G>R+18)|(B>R+18)|(lum>470)|((R>150)&(G>130)&(B<110))
    st=np.ones((3,3),bool); m=ndimage.binary_closing(m,st); m=ndimage.binary_closing(m,st)
    m=ndimage.binary_fill_holes(m); lab,n=ndimage.label(m)
    s=ndimage.sum(m,lab,range(1,n+1)); return lab==int(np.argmax(s))+1
XS=range(160,216,5)
for lab,p in (('PREV','qa-renders-prev/zixxtrixx-spring-side'),('HEAD','qa-renders/zixxtrixx-spring-side')):
    print('==',lab)
    for fi in (12,42,72,108,144):
        m=mask_of(l1(p,fi))
        top=[];bot=[]
        for x in XS:
            col=np.nonzero(m[:,x])[0]
            top.append(col.min() if len(col) else -1); bot.append(col.max() if len(col) else -1)
        print(f"  f{fi:3d} ventral-run TOP y @x{list(XS)}: {top}")
        print(f"       BOTTOM y: {bot}")
    # arch amplitude: how far the top edge of the mid run deviates from the straight line between its ends
    for fi in (12,42,72):
        m=mask_of(l1(p,fi)); t=[]
        for x in XS:
            col=np.nonzero(m[:,x])[0]; t.append(col.min() if len(col) else np.nan)
        t=np.array(t,float); line=np.linspace(t[0],t[-1],len(t))
        print(f"  f{fi:3d} top-edge bow vs chord: max {np.nanmax(line-t):+.1f} px (up is +)")
