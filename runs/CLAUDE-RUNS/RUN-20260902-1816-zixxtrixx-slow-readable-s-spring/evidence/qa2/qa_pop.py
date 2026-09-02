import os,numpy as np
def load(p,n=None):
    ns=sorted(x for x in os.listdir(p) if x.endswith('.rgb'))
    if n: ns=ns[:n]
    out=[]
    for x in ns:
        r=np.fromfile(os.path.join(p,x),dtype=np.uint8); w,h=r[:8].view(np.uint32)
        out.append(r[8:].reshape(int(h),int(w),3))
    return out
def prof(fr):
    return np.array([0]+[int(np.any(fr[i]!=fr[i-1],axis=2).sum()) for i in range(1,len(fr))])
for lab,path,n in (('spring-side HEAD','qa2-renders/zixxtrixx-spring-side',175),
                   ('spring-side PREV','qa2-renders-prev/zixxtrixx-spring-side',175),
                   ('balance','qa2-renders/zixxtrixx-balance',300)):
    fr=load(path,n); p=prof(fr)
    med=np.median(p[1:])
    # local spike ratio: value / median of +-5 window
    ratios=[]
    for i in range(6,len(p)-6):
        loc=np.median(np.concatenate([p[i-5:i],p[i+1:i+6]]))
        ratios.append((p[i]/max(loc,1),i,p[i],loc))
    ratios.sort(reverse=True)
    print(f"{lab}: median {med:.0f}  top local spikes (ratio, frame, px, local-median):")
    for r in ratios[:6]: print(f"    {r[0]:5.2f}x  f{r[1]:3d}  {r[2]:5d} vs {r[3]:.0f}")
    print(f"    f13-f18: {list(p[13:19])}")
