# Q009 review-wave-e-fx
max_tokens: 12000
effort: medium
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Independent bug review of a committed change (Wave E, commit 0381bdec) to a software renderer's particle code. Intent: a "mote surface fade" that fades fold/surge particle motes whose centre depth nears an antenna surface (footprint mean over 5x5 depth taps, 120 mm threshold, chosen by eye). `ZHAO_U02_MOTE_SURFACE_FADE=off` must be byte-exact with the pre-change output; a legacy Crackle subject must also be byte-exact (a leak into it was found and fixed during closure). Depth buffer values are 1/w-like: larger = nearer (the splat depth test is `centre_d > depth[idx]` passes when the splat is nearer).

## Questions
1. Is the "off" path truly byte-identical — any arithmetic, rounding, clamping or branch that runs even when off? Cite lines.
2. Depth-space correctness: the threshold is 120 mm but the buffer holds 1/w-style values. How is mm converted, and is it correct for motes near and far from the camera? Any sign error (fading motes that are in FRONT and clear of the stick vs ones touching it)?
3. The 5x5 footprint: bounds checks at frame edges; integer overflow; division by zero when no taps are valid; does it sample depth that already includes other particles (depth isn't written by splats — confirm from what is shown)?
4. Which motes does it apply to (fold/surge only?) and could it silently affect lightning, strands, bullets or other splat kinds?
5. List P1/P2 findings only; say "none found" if so.

## Inputs
show:0381bdec:tools/reel/manafold_fx.h
