import os,sys
import numpy as np
sys.path.insert(0,'.')
from _maskfns import load, mask_of
for lbl,d,g0,g1 in (("PUBLISHED d5949320","renders-prev/zixxtrixx-spring-side",0,167),
                    ("HEAD PEEL e78f28e1","renders/zixxtrixx-spring-side",0,115),
                    ("BALANCE accepted","../qa3-bank/zixxtrixx-balance",0,492)):
    fr=load(d); ms=[mask_of(f) for f in fr]
    x=[]
    for i in range(g0+1,g1+1):
        u=(ms[i]|ms[i-1]).sum(); x.append(100.0*(ms[i]^ms[i-1]).sum()/max(u,1))
    x=np.array(x)
    print("%-22s med %5.2f  p90 %5.2f  p99 %5.2f  max %6.2f   worst frames %s"%(
        lbl,np.median(x),np.percentile(x,90),np.percentile(x,99),x.max(),
        [int(i)+g0+1 for i in np.argsort(x)[-5:]]))
