# Q017 review-wave-f-motion
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Independent bug review of committed commit d7d51171 (Manafold v18 Wave F): the Flight and Trick motion source. Intent:
- Flight: new named knobs (cycles per loop, amplitude, asymmetric rise/fall, top hang, pitch lead, breath phase, height offset) whose OLD values reproduce Wave-E bytes exactly; shipping values 2 cycles, 800 mm bob raised 500 mm, quick climb / long glide, float at top, nose leads climb. Loop seam must be exact and every channel C2 (no kinks from the rise/fall remap).
- Trick: 180 pitch plant (unchanged), pause, a planted world-vertical 360 yaw with a slight named overshoot and C2 correction back to exact identity, authored as unwrapped per-mille progress (quintic segments, int64), pivoting about the antenna SUPPORT point by compensating root X/Z so the support XZ stays fixed; the clip length did not change; the balance-fade literal 158 became a named constant; old knob values reproduce Wave-E bytes.
- Drift: constant horizontal camera offset.

## Questions
1. Flight: is the seam exact for every channel after the rise/fall remap and top hang (show the math)? Any C0/C1/C2 break at remap joins? Do old knob values reproduce the old formulas exactly (integer paths identical)?
2. Trick spin: are progress segments C2 at every join; does the final key land exactly on identity (no residual angle / wrap error); any int32 overflow; is the world-yaw applied as a pre-multiply and the X/Z compensation derived from the SAME post-spin support position?
3. Does the spin ever run outside the contact window, or does any Trick constant change silently shift unrelated timing (sinp oscillators keyed to K)?
4. P1/P2 only in FINDINGS; else "none found".

## Inputs
show:d7d51171:tools/reel/manafold_art.h
show:d7d51171:tools/reel/manafold_clips.h
