# Q004 trick-360-spec
max_tokens: 14000
effort: medium
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou
continue: Q003

## Brief
Write the corrected implementation spec for the Trick 360-degree planted spin. Coordinator-VERIFIED corrections to the earlier draft (treat as fact):
- The planted support is NOT on the root's vertical axis. `trick_support_center_y_mm` starts from (kLoopTubeXMm, kLoopNeckExitYMm, 0) and walks JunctionF->Neck->HingeA->HingeB, then rotates by g.q[kBRoot]. So a yaw about the root would drag the support around a circle. The spin must pivot about the SUPPORT point: compute the support's full world XYZ (same walk, but return x,y,z), and translate the root so the support's world XZ stays fixed while the body spins about the world vertical through it. Root X/Z are written via c.root[f*3+0] and c.root[f*3+2] (fixed point via fxu(mm)); today Trick writes only [f*3+1].
- The inline literal `158` in `f >= kTrickPlantKey && f < 158` must become a named constant tied to the lift key.
- Integer math must not overflow: use int64_t for quintic terms.
- Raising K (kTrickKeys) re-phases every sinp(f, K, n) and antenna_knead/front_flex_play call over the WHOLE clip. Prefer to keep those oscillators on their current absolute timing (e.g. pass a fixed period constant, or insert the spin as a time-remap of the plant hold) and state which you choose and why.
- 1040 per-mille = 68157 angle16.
Keep: pure-X flip, +90 face yaw, current pause, existing righting; contact window must cover the entire spin; no camera chase; every timing/amplitude a named constant.

## Questions
1. The exact support-XYZ helper (C++), reusing the existing walk.
2. The per-key root XZ compensation formula during the spin, in integer math, and where it goes relative to the Y pivot. Show it keeps support XZ fixed.
3. The timeline and constants (named), and how you avoid re-phasing the existing oscillators outside the spin.
4. A compact code sketch of the changed build_trick section (only changed lines + anchors).
5. What the probe/gates must additionally check (spin progress unwrap, support XZ drift bound, C2 at joins).

## Inputs
tools/reel/manafold_clips.h:30-120
tools/reel/manafold_clips.h:3169-3305
tools/reel/manafold_art.h:2815-2880
