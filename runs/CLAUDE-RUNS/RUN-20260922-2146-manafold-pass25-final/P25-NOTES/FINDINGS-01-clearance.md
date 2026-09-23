# Pass 25 -- findings written at the look, item 1 (clearance)

## The rollout itself

With `bolt_avoid_for` returning `kRods` for all 22 live subjects, mbolt at the
pass-24 clearance (46 mm) reports **0 intersections on all 22**, against 54,595
before. Death-drop (16.6%) and drift (15.3%), the two the census named as the
worst untouched offenders, are both zero.

## The number nobody had: closest approach

Pass 24's B1 read zero and the owner could still see the fault, because "zero
intersections" is not "clear". mbolt now reports the smallest gap between any
drawn bolt segment and the band. At 46 mm it is **4.4 mm, on blown f130** -- a
mathematically clear pass that at 384x240, with a stamped sprite chain on one
side and contour ink on the other, closes back up into a touch. That is the
owner's "it gets very close now and looks like crossing", located.

## The knob is NOT monotone (measured, whole bank, every key and midpoint)

| mm | result | mm | result |
|---:|---|---:|---|
| 46 | clean, min 4.4 (blown f130) | 106 | **1 hit** (channel f247) |
| 56 | **3 hits** | 116 | clean, min 56.7 |
| 66 | **8 hits** | 130 | clean, min 36.1 |
| 70 | **11 hits** (worst -56.4, blown f142) | 150 | clean, min 24.3 |
| 76 | **4 hits** | 170 | clean, min 43.6 |
| 86 | clean, min 45.8 | | |
| 96 | clean, min 32.3 | | |

A clearance big enough to shove a chord off one rod but not past the
neighbouring ball leaves it wedged, and `kBoltAvoidSweeps` is a fixed count.
Every dirty rung is `blown` or `channel`. **So the value had to come from the
clean list**, and the ladder was re-rendered on clean rungs only.

## Two gate defects this ladder exposed, both repaired

1. **B3 said "0 intersections remain in the bank" while B1 was RED** with 11
   intersections on blown at 70 mm. It summed the residual over the subjects
   carrying NO avoidance -- an empty set after the rollout -- so it was
   structurally incapable of seeing the case it was quoted about. It now sums
   `total_hits()` over the whole bank and asserts it.
2. **B2 went red at 96 mm because an ART VALUE moved.** Its spacing operand was
   measured only over segments within `kNearBandMm` of a rod, and a large enough
   clearance empties that set: spacing and reference both 0. B2 now measures the
   spacing over every bolt segment; the near-rod figure stays as INFO.

## The ladder, on clean rungs, three clips, at 8x and at native

Frames chosen by badness from mbolt's own per-frame csv, not by index:
blown f130 (4.4 mm), crackle f250 (12.1 mm), hover f471 (the tightest closure,
the declared pass-24 side effect).

| rung | what was seen |
|---|---|
| 46 (pass 24) | blown: the figure's left vertical lies ON the rod and its top stroke runs along the body outline. crackle: the lower run lies along the lower rod. This is the complaint. |
| 86 | a gap opens on all three; the top stroke has lifted off the body outline. Full width kept. |
| **96 -- SHIPPED** | a clear band of pink shows between lightning and rod on blown and crackle; on hover the figure still reads as running around the loop. Size, jag, colour, density, beading all unchanged. |
| 116 | **the first rung that draws attention to itself.** crackle: the figure straightens and re-lays itself ALONG the lower rod, a different placement. hover: the outer run becomes a bead chain climbing off into empty air. blown: the shape narrows. |
| 130 | more of the same; on blown the figure has climbed onto the body. |

**96 is the rung below the first one that restyles, and it is the one that reads
best at native.** In the native 2x A/B (L11) all three clips show pink between
the bolt and the band where pass 24 showed contact.

## The declared side effect, judged

At Hover's tightest closure (f471) the push does carry part of the figure
outside the rods, and more clearance does make it more pronounced -- the
direction predicted both. At 96 the outer run is a legible arc that still reads
as energy running AROUND the loop and returning through it; the pocket bar is
intact. At 116 it becomes a detached bead chain in empty air and no longer reads
as belonging to the antenna. **That is the limit of the ladder, and it is why
116 was not taken even though it is gate-clean.**
