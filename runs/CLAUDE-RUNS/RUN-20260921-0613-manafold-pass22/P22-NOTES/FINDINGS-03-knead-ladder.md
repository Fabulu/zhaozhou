# P22 findings 03 -- the lightning's answer to the knead (item 5), looked at

## The first cut measured present and read as a WOBBLE
Roll 9000 a16 was written as "~49 deg at a full dip". Rendered (P22-LOOKS/05,
first version): 127/600 frames changed on Inspect AND Hover, worst frame 2177
px of 92,160 -- thirty times pass 20's rejected reaction -- and on the strip the
loop only wobbled. Classic crayon grain: present in the metric, absent to the
eye.

## Cause, measured with U02_FOLD_DEBUG over ten clips
`dip_pm` NEVER APPROACHES 1000. Bank peaks:
  trick 208 | hover 152 | inspect 152 | rest 147 | channel 111 | hasty 85 |
  pirouette 72 | drift 27 | blown 18 | taunt III 0 (hosts no dip by design)
dip_pm = smoothstep(sag) * kFoldDipGainPm(650)/1000, and the sag never nears
kFoldDipRefMm = 420 mm. So 9000 a16 "at a full dip" was 1368 a16 = **7.5 deg**
in every frame that exists. A constant written against a state that never
occurs is unreadable as an art value -- this also retro-explains pass 20's very
large drop/squash/spread numbers (330 mm / 430 / 700 are ~15% effective).

Fix: a DECLARED reference, `kFoldDipShapeRefPm = 150`, with saturation. The
three constants are now the values AT a real press. Saturation is Direction
18's law (express a carrier's gesture, never amplify it) and bounds any future
deeper clip.

## The ladder, Inspect f591 (the dip bottom), 7x -- P22-LOOKS/06
- A p21: a broad rounded horizontal bag.
- s=400 (~20 deg): the loop has visibly turned and narrowed. Reads.
- s=700 (~34 deg): the right end swings up, the form becomes a leaning wedge.
- **s=1000 (~49 deg): chosen.** The loop turns and curls clearly; still the same
  topology, the same white core and navy backing, the same station count -- a
  form change, not a restyle. The owner asked for the lightning to react MORE,
  so the strong end of a ladder that reads at every rung is the right end.

## In motion -- P22-LOOKS/05 (Inspect f567-597) and 07 (Hover f579-597)
- f567 (onset): A, s700 and s1000 are indistinguishable. The gesture ARRIVES
  without a step, which is what dip_pm's smoothstep buys.
- f573-f591: the form grows continuously -- the bundle splits into two strands,
  the hook opens, the loop tilts. No snap anywhere.
- f597 (release): converging back to A.
- Hover shows the same beat with the same character preserved.
- 473 of 600 frames on BOTH clips are byte-identical to pass 21: away from the
  press the lightning is untouched, which is the owner's other constraint.

SHIPPING: roll 9000 a16, tumble 3200, shear 420 pm, at reference 150, gain 1000.
