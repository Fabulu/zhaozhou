# Q008 camera-framing-map
max_tokens: 12000
effort: medium
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Wave F of Manafold v18 needs framing work: (1) in Trick the planted crown (creature upside-down on its antenna) sits at the bottom edge of the frame; (2) Flight will gain larger vertical travel and must stay in frame. The camera is fixed per subject (no camera chase allowed). Map what each per-subject camera parameter does so the implementer can choose a framing knob by eye.

## Questions
1. From `cam_pitch` and its call site, explain exactly what `k` (cam_k), `eye_m`, `dist_m`, `ps` (cam_ps), `pc` (cam_pc), `bias` (cam_bias) do geometrically (distance/zoom, pitch sine/cosine, aim offset in which axis and units). Cite lines. Is `ps`/`pc` a sine/cosine pair (check ps²+pc² against 65536²)? State the pitch angle for the defaults (ps 28732, pc 58903) and for (9000, 64900).
2. Which parameter moves the aim point vertically without changing pitch or zoom (i.e. shifts the creature up/down in frame)? What sign moves the creature UP in frame? If this cannot be determined from the shown code, say so and say what a one-render test would show.
3. The subjects in the input set per-slot overrides (Blown, deaths, Fall, Flight, Hasty). Summarise each override and its stated reason.
4. For Trick (default camera) recommend which single knob to ladder to lift the planted crown off the bottom edge, with 3 rungs, and for Flight which knob gives vertical headroom. Values are chosen by eye on render; give ranges only.

## Inputs
tools/reel/zhao_reel.cpp:300-365
tools/reel/zhao_reel.cpp:1036-1066
tools/reel/zhao_reel.cpp:4244-4265
tools/reel/zhao_reel.cpp:4600-4625
tools/reel/zhao_reel.cpp:5800-5852
