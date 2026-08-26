"""Search the light rig for constants that actually satisfy the goals, instead
of picking plausible numbers and hoping.

Goals, in order:
  1. no face darker than MIN_GAIN (the shadow side must not be a silhouette)
  2. every face keeps chroma: worst pink channel spread >= MIN_SPREAD
  3. highlights stay near-neutral (a white key must read white)
  4. as many distinct face values as possible (form, not flatness)
"""
import math, itertools
L=(1/math.sqrt(6),2/math.sqrt(6),1/math.sqrt(6))
PINK=(233,188,206)
def q16(x): return max(0.0,min(1.0,round(x*16)/16))
def norm(v):
    m=math.sqrt(sum(c*c for c in v)); return tuple(c/m for c in v)
def evaluate(F,AMB,KEY,FILL,faces=36):
    gains=[]
    for i in range(faces):
        t=2*math.pi*i/faces; n=(0.0,math.cos(t),math.sin(t))
        lk=max(0.0,sum(a*b for a,b in zip(n,L)))
        lf=max(0.0,sum(a*b for a,b in zip(n,F)))
        gains.append(tuple(q16(AMB[c]+KEY*lk+FILL[c]*lf) for c in range(3)))
    mn=min(min(g) for g in gains)
    mx=max(max(g) for g in gains)
    spreads=[max(round(PINK[c]*g[c]) for c in range(3))-min(round(PINK[c]*g[c]) for c in range(3))
             for g in gains]
    # neutrality of the brightest face
    bright=max(gains,key=lambda g:sum(g))
    neutral=max(bright)-min(bright)
    return mn,mx,min(spreads),len(set(gains)),neutral

best=None
for fy in (-1.0,-0.9,-0.8):
  for fz in (-0.4,-0.2,0.0,0.2):
    for fx in (-0.4,-0.2,0.0):
      F=norm((fx,fy,fz))
      for key in (0.62,0.66,0.70):
        for a0 in (0.20,0.24,0.28):
          for atint in (0.02,0.04,0.06):
            AMB=(a0,a0+atint*0.5,a0+atint)
            for f0 in (0.14,0.20,0.26):
              for ftint in (-0.10,-0.06,0.0,0.06):
                FILL=(f0,f0+ftint*0.5,f0+ftint)
                if min(FILL)<0: continue
                mn,mx,sp,nd,neu=evaluate(F,AMB,KEY:=key,FILL)
                if mn<0.30 or sp<20 or mx<0.86 or mx>1.0 or neu>0.09: continue
                score=(sp*2+nd)-abs(mx-0.94)*40
                if best is None or score>best[0]:
                    best=(score,F,AMB,key,FILL,mn,mx,sp,nd,neu)
if best is None:
    print("no rig met the constraints"); raise SystemExit
score,F,AMB,KEY,FILL,mn,mx,sp,nd,neu=best
print("BEST RIG")
print(f"  fill dir   ({F[0]:+.5f},{F[1]:+.5f},{F[2]:+.5f})")
print(f"  ambient    ({AMB[0]:.3f},{AMB[1]:.3f},{AMB[2]:.3f})")
print(f"  key        {KEY:.3f}  (white)")
print(f"  fill       ({FILL[0]:.3f},{FILL[1]:.3f},{FILL[2]:.3f})")
print(f"  min gain {mn:.3f}  max {mx:.3f}  worst pink spread {sp}  distinct {nd}  "
      f"highlight non-neutrality {neu:.3f}")
print()
print("  Q16.16 constants:")
for nm,v in (("kFillX",F[0]),("kFillY",F[1]),("kFillZ",F[2])):
    print(f"    {nm} = {round(v*65536)};   // {v:+.5f}")
print(f"    kAmbR = {round(AMB[0]*65536)}, kAmbG = {round(AMB[1]*65536)}, kAmbB = {round(AMB[2]*65536)};")
print(f"    kKey  = {round(KEY*65536)};")
print(f"    kFillR = {round(FILL[0]*65536)}, kFillG = {round(FILL[1]*65536)}, kFillB = {round(FILL[2]*65536)};")
