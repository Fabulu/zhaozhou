import os, struct, sys
import numpy as np
W,H=384,240
def write(d,i,arr):
    os.makedirs(d,exist_ok=True)
    with open(os.path.join(d,f"f{i:04d}.rgb"),"wb") as fh:
        fh.write(struct.pack("<II",W,H)); fh.write(arr.tobytes())

N=180
# A: PERFECT LOOP - sinusoid closing exactly, seam should equal a typical step
d="A_perfect_loop"
for i in range(N):
    a=np.zeros((H,W,3),np.uint8)
    x=int(150+100*np.sin(2*np.pi*i/N))
    a[:, x:x+40] = 200
    write(d,i,a)
# B: BROKEN LOOP - ramp that never returns; seam is a huge jump
d="B_broken_loop"
for i in range(N):
    a=np.zeros((H,W,3),np.uint8)
    x=int(20+ i*1.5)
    a[:, x:x+40] = 200
    write(d,i,a)
# C: DEAD CLIP - every frame identical. typ==0.
d="C_dead_static"
base=np.full((H,W,3),77,np.uint8)
for i in range(N): write(d,i,base)
# D: DEAD BODY, MOVING SEAM - static except the last frame differs
d="D_static_but_seam"
for i in range(N):
    a=np.full((H,W,3),77,np.uint8)
    if i==N-1: a[:,:100]=255
    write(d,i,a)
# E: ALIAS TRAP - motion with period exactly 10 frames (seam.py samples stride 10)
d="E_period10_alias"
for i in range(N):
    a=np.zeros((H,W,3),np.uint8)
    x=int(150+100*np.sin(2*np.pi*i/10))
    a[:, x:x+40]=200
    write(d,i,a)
print("generated")
