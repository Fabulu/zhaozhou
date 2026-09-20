# P21 IMPLEMENTER LOOK NOTES (rods)

Production ink throughout: `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`.
Frames chosen by the gate's own worst rings/frames, never by index.

## Look 1 -- every-frame sheet, Inspect (600 frames), rods
`P21-LOOKS2/P21RODS-MANAFOLD-INSPECT-ALLFRAMES.jpg`
The loop holds its read across all 600 frames. No frame where the antenna
breaks, vanishes, or changes character. The shape varies smoothly through the
closure walk. Nothing to chase at thumbnail scale -- which is what a contact
sheet is for; the joints need a crop.

## Look 2 -- Inspect f079 (the SHORTEST rear chord, 322 mm), 4x, before/after
`P21-LOOKS2/P21-INSPECT-JOINTS-4X.jpg`
This is the frame the architecture named as art risk 2 (a 3x compacted rear rod
reading as an accordion) and it is the clearest single answer in the pass.

* **pass20 (left):** the rear run is visibly WAVY -- it leaves C, kinks, runs,
  kinks again, and arrives at the body off a third direction. Those are the
  polygon corners at rings 36/37, 45/46 and 49/50, and they are plainly visible
  as a band that cannot decide which way it is going. The A/B corner at the top
  left is a mitred slab.
* **RODS (right):** the rear run is ONE straight band from C to the body. The
  turn that was spread over three kinks now sits entirely at C, as a rounded
  knuckle. The front corners read as knuckles too.
* The compaction does NOT read as an accordion. At 3x compaction the band is a
  shorter, slightly denser tube; the crayon grain compresses along it and the
  silhouette stays clean. Risk 2 is answered NO on the worst frame it has.
* **What the rods version loses:** the balls are subtle. They read as roundings
  at the corners rather than as the drawn beads. That is by design (the radii
  default to today's profile-at-carrier, which version 18 accepted) but it is
  the thing to ladder by eye.

## Look 3 -- Inspect f280 (the DEEPEST KNEAD PRESS), 4x, before/after
`P21-LOOKS2/P21-KNEAD-4X.jpg` (at the 1000-pm ball)
The knead still reads: B presses down and the loop squeezes. The runs are dead
straight, which is the point -- and it exposed the pass's real art fault, which
is the next look.

## Look 4 -- THE BALL LADDER, Inspect f280, 8x on one corner
`P21-LOOKS2/P21-BALL-LADDER-RX-8X.jpg`, four rungs: pass20, 1000, 1400, 1800.
**This is the look that changed a shipped value, and the gates could not have
found it.** mrod read ALL LEGS OK on this exact frame at every rung.

* pass20: an angular, slabby bend. The band thins at the turn and there is a
  notch on the inside -- the mitre the owner has been seeing.
* **1000** (today's profile at the carrier, 72/64 at A): rounder and cleaner than
  pass 20, but the ball does not READ. On a band about six pixels wide at 240p a
  1.56x knuckle is three pixels; the antenna becomes a bent wire whose
  articulation is in the right place and invisible.
* **1400 -- CHOSEN.** A distinct rounded knuckle at every corner, with the band
  still dominating the silhouette. That is what the Side sheet draws: modest nubs
  on the OUTSIDE of the corners, rounded, not beads.
* 1800: an obvious round knob. Legible but it is version 18's rejected
  "obviously big protruding balls", and the band-to-ball step starts to read as
  two separate parts.

Shipped: `kBallRxMm {101, 97, 101, 106}`, `kBallRzMm {90, 80, 71, 84}`.
`kBallRyMm` is NOT laddered -- see its note; lengthening the balls eats the
340 mm A->B run.

## Look 5 -- Trick's plant, and why B's ball is smaller than the ladder chose
The ball ladder was applied at 1.4x and the committed ground probe immediately
found a real regression: carrier B is the creature's FOOT for seventy keys of
Trick's headstand, and a 1.4x foot reaches 15 mm further down. The approach
three keys ahead of the declared contact window fell to 32 mm against a 40 mm
clearance floor.

Two things were tried in order and both are recorded because the first one is
the obvious move and it does not work:

1. Raise `kTrickPlantRootMm` (the lever pass 6 and pass 12 both used for exactly
   this event). +16 mm of root moved the plant DEPTH by +16 mm and the approach
   by **+3 mm** -- because build_trick pivots the body about the planted support
   centre, so the two quantities converge at the plant key and not before it.
   Reaching 40 mm this way needs +27 mm more root, which lifts the declared
   plant out of its own accepted depth band. The lever cannot do it alone.
2. Size B's ball to the contact it has to make: 1.23x instead of 1.4x, then
   `kTrickPlantRootRodsMm` 1545 to land the deepest planted vertex on EXACTLY
   the declared -25 mm. Approach 44 mm, carrier B owns 140/140 of the window,
   depth range -25..-16.

The clearance floor was not lowered and the contact window was not widened.
The Side sheet's three balls are not the same size either.
