import os,sys
import numpy as np
sys.path.insert(0,'.')
from _maskfns import load, mask_of
for lbl,d,g1 in (("PUBLISHED d5949320","renders-prev/zixxtrixx-spring-side",167),
                 ("HEAD e78f28e1 PEEL","renders/zixxtrixx-spring-side",115)):
    fr=load(d); ms=[mask_of(f) for f in fr]
    print("==",lbl)
    print("%5s %6s %6s %6s %8s %8s"%("f","bbW","bbH","area","cx","cy"))
    for f in [0,g1//4,g1//2,3*g1//4,g1]:
        m=ms[f]; ys,xs=np.nonzero(m)
        print("%5d %6d %6d %6d %8.1f %8.1f"%(f,xs.max()-xs.min()+1,ys.max()-ys.min()+1,m.sum(),xs.mean(),ys.mean()))
    # extremes over ground phase
    W=[];H=[];A=[]
    for f in range(g1+1):
        ys,xs=np.nonzero(ms[f]); W.append(xs.max()-xs.min()+1);H.append(ys.max()-ys.min()+1);A.append(ms[f].sum())
    W=np.array(W);H=np.array(H);A=np.array(A)
    print("  rest  w=%d h=%d area=%d"%(W[0],H[0],A[0]))
    print("  min w=%d (f%d)  min h=%d (f%d)  min area=%d (f%d)"%(W.min(),W.argmin(),H.min(),H.argmin(),A.min(),A.argmin()))
    print("  loaded (last ground frame) w=%d h=%d area=%d"%(W[g1],H[g1],A[g1]))
    print("  width shrink rest->loaded: %.0f%%   area shrink: %.0f%%"%(100*(1-W[g1]/W[0]),100*(1-A[g1]/A[0])))
    print()
