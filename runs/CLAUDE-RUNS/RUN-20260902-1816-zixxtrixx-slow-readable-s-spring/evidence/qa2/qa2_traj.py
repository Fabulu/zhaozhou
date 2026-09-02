import os,numpy as np
from scipy import ndimage
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
def load(p,n):
    ns=sorted(x for x in os.listdir(p) if x.endswith('.rgb'))[:n]
    return [np.fromfile(os.path.join(p,x),dtype=np.uint8)[8:].reshape(240,384,3).astype(np.int16) for x in ns]
def mask_of(f):
    R,G,B=f[:,:,0],f[:,:,1],f[:,:,2]; lum=R+G+B
    m=(lum<130)|(G>R+18)|(B>R+18)|(lum>470)|((R>150)&(G>130)&(B<110))
    st=np.ones((3,3),bool); m=ndimage.binary_closing(m,st); m=ndimage.binary_closing(m,st)
    m=ndimage.binary_fill_holes(m); lab,n=ndimage.label(m)
    s=ndimage.sum(m,lab,range(1,n+1)); return lab==int(np.argmax(s))+1
cols=np.arange(384)
regs=[('tail x<170',cols<170),('mid 170-209',(cols>=170)&(cols<210)),('head x>=210',cols>=210)]
fig,axes=plt.subplots(2,2,figsize=(15,8))
for col,(lab,path) in enumerate((('PREV 48310d18','qa2-renders-prev/zixxtrixx-spring-side'),('HEAD d5949320','qa2-renders/zixxtrixx-spring-side'))):
    ms=[mask_of(f) for f in load(path,169)]
    for rn,sel in regs:
        cy=[];tipx=[]
        for m in ms:
            t=m.copy(); t[:,~sel]=False
            ys,xs=np.nonzero(t)
            cy.append(ys.mean() if len(ys) else np.nan)
        axes[0][col].plot(cy,label=rn)
    axes[0][col].invert_yaxis(); axes[0][col].set_title(lab+' — region centroid Y (up is up)')
    axes[0][col].legend(); axes[0][col].grid(alpha=.3)
    for b in (12,72,82,144,168): axes[0][col].axvline(b,color='k',ls=':',lw=.7)
    fx=[];fy=[]
    for m in ms:
        t=m.copy(); t[:,~(cols<170)]=False
        ys,xs=np.nonzero(t); fx.append(xs.min()); fy.append(ys[xs==xs.min()].mean())
    axes[1][col].plot(fx,label='fin-tip x'); axes[1][col].plot(fy,label='fin-tip y')
    axes[1][col].set_title(lab+' — fin tip route (flat line = planted)')
    axes[1][col].set_ylim(100,135); axes[1][col].legend(); axes[1][col].grid(alpha=.3)
    for b in (12,72,82,144,168): axes[1][col].axvline(b,color='k',ls=':',lw=.7)
plt.tight_layout(); plt.savefig(os.path.join(os.path.dirname(__file__),'trajectories-prev-vs-head.png'),dpi=90)
print('ok')
