import os,numpy as np
def load1(p,i):
    r=np.fromfile(os.path.join(p,'%04d.rgb'%i),dtype=np.uint8); w,h=r[:8].view(np.uint32)
    return r[8:].reshape(int(h),int(w),3).astype(int)
def cmp(gen,head,off,label):
    ng=len([x for x in os.listdir(gen) if x.endswith('.rgb')])
    nh=len([x for x in os.listdir(head) if x.endswith('.rgb')])
    ident=[];diffs=[]
    for i in range(ng):
        j=i+off
        if j>=nh: break
        d=int(np.any(load1(gen,i)!=load1(head,j),axis=2).sum())
        if d==0: ident.append(i)
        else: diffs.append((i,d))
    print(f'{label}: gen13 {ng} frames vs HEAD {nh} at offset +{off}')
    print(f'   byte-identical: {len(ident)} frames -> {ident[:6]}...{ident[-6:] if len(ident)>6 else ""}')
    if diffs:
        diffs.sort(key=lambda t:-t[1])
        print(f'   changed: {len(diffs)} frames, worst {diffs[0][1]} px at gen13 f{diffs[0][0]}; top5 {[d[1] for d in diffs[:5]]}')
    return ident,diffs
cmp('qa2-gen13/zixxtrixx-jump-one','qa2-renders/zixxtrixx-jump-one',132,'JUMP-ONE')
