import os,sys
import numpy as np
def frames(p):
    ns=sorted(n for n in os.listdir(p) if n.endswith('.rgb'))
    return [np.fromfile(os.path.join(p,n),dtype=np.uint8)[8:] for n in ns]
a=frames(sys.argv[1]); b=frames(sys.argv[2])
print("gen13 %d frames, head %d frames"%(len(a),len(b)))
best=None
for off in range(0,len(b)-len(a)+1):
    n=sum(1 for i in range(len(a)) if np.array_equal(a[i],b[i+off]))
    if best is None or n>best[1]: best=(off,n)
print("best offset %d with %d byte-identical frames"%best)
off=best[0]
ident=[i for i in range(len(a)) if np.array_equal(a[i],b[i+off])]
def runs(v):
    out=[];s=v[0];p=v[0]
    for x in v[1:]:
        if x==p+1: p=x; continue
        out.append((s,p)); s=p=x
    out.append((s,p)); return out
print("identical gen13-frame ranges:", ", ".join(("%d"%s if s==e else "%d-%d"%(s,e)) for s,e in runs(ident)))
print("count:",len(ident))
