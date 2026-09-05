import sys, os, glob
sys.path.insert(0, r"C:\programmieren\zencrifice\manafold-pass5-review\zhaozhou\tools\reel")
import numpy as np
from rgbframe import load
def manamask(a):
    f=a.astype(np.int16); r,g,b=f[:,:,0],f[:,:,1],f[:,:,2]
    return (b>=r)&(f.max(2)>170)
def sat_of(a, m):
    f=a.astype(np.int16)
    if m.sum()==0: return 0.0
    return float((f.max(2)-f.min(2))[m].mean())
