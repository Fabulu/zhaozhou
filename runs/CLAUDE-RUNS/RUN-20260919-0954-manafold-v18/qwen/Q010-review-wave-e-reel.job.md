# Q010 review-wave-e-reel
max_tokens: 14000
effort: medium
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou
continue: Q009

## Brief
Continue the bug review of commit 0381bdec (Wave E), now the renderer side in zhao_reel.cpp. Three things changed there: (a) the live-history mist is turned off for the 22 live Manafold subjects via an explicit list, with a renderer self-check (exit code 5) that fails if a live subject has mist on; archived/diagnostic subjects keep their previous mist setting explicitly; (b) Crackle moves to mana candidate 9 + day sky with a new `manafold-crackle-legacy` subject reproducing old bytes; (c) the mote surface fade computation (5x5 depth footprint, 120 mm, only for splats with surface_fade=true, off via ZHAO_U02_MOTE_SURFACE_FADE=off). Depth buffer: larger = nearer (1/w-like).

## Questions
1. Mote fade math: how is 120 mm converted against the 1/w-style depth? Correct at near and far distances? Sign right (fades only motes whose centre is near the antenna surface, not motes clearly in front/behind)? Bounds at frame edges, zero valid taps, overflow. Does "off" skip ALL new arithmetic?
2. Does the fade read a depth/mask that is only the antenna (as intended), and is that mask built on every frame where it is used (what if it is empty)?
3. Live list: can a live subject be missed (e.g. a subject added later, or a name typo) without the self-check noticing? Is the self-check itself reachable and wired to the process exit code?
4. Crackle legacy: does anything added in this commit (fade, list, backdrop) leak into the legacy subject?
5. P1/P2 findings only; "none found" if so.

## Inputs
show:0381bdec:tools/reel/zhao_reel.cpp
