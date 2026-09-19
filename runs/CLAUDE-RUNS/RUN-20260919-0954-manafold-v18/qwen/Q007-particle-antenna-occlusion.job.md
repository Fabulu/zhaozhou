# Q007 particle-antenna-occlusion
max_tokens: 12000
effort: medium
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Owner: "Sometimes the particles go through the antennae and that looks strange. Don't know if there's an easy way to stop that. Don't spend too much effort but if there is, then fix it." Binding plan: a BOUNDED experiment only. Reuse the existing pre-creature splat layer (`ManaSplat::pre`) for fold/surge motes so antenna sticks occlude them, and ship it only if sticks occlude cleanly WITHOUT flattening depth (motes that are genuinely in front of the creature must still draw in front). No geometric particle collision.

## Questions
1. Explain `mana_push` / `ManaSplat` fields from the input: what are the two booleans passed as `true, false` at the fold/surge call sites (which is depth-test, which is pre)? Cite lines.
2. How does the pre pass (reel.cpp ~3267) differ from the post pass (~3800) in compositing: order relative to the creature, depth testing against creature depth, blending? Cite lines.
3. If fold/surge motes were routed through pre=true, would a mote that is in FRONT of the antenna (closer to camera) still appear in front of it? Would a mote BEHIND a stick be hidden? Reason strictly from the depth logic shown. Is "draws before the creature" equivalent to "always behind the creature" here, or does depth still decide?
4. If the post pass already depth-tests against creature depth, why would particles visibly "go through" the antenna at all? Propose the 2-3 most likely mechanisms visible in the code (e.g. halo radius larger than the stick so a halo centred behind still paints over it; depth test disabled for some splat kinds; opaque vs soft; mote depth taken at centre).
5. Recommend the single narrowest change to try first, as a named toggle so an A/B is possible, and what to look at in the render to accept or reject it.

## Inputs
tools/reel/manafold_fx.h:1800-1870
tools/reel/manafold_fx.h:2180-2210
tools/reel/manafold_fx.h:3600-3647
tools/reel/zhao_reel.cpp:3205-3290
tools/reel/zhao_reel.cpp:3790-3835
