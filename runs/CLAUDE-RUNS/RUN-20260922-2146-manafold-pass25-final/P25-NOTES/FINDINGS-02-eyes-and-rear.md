# Pass 25 -- findings written at the look, items 2 and 3

## ITEM 2 -- the eye ambience

Ladder 600 / 700 / 800 / 900 / 1000 on Rest, on the frames a framediff named as
the ones this knob moves most (f008, f264, f272), four frames per rung at 8x,
crop wide enough to show the whole lens so rim clearance is judgeable.

| rung | what was seen |
|---|---|
| 600 | today: the stars drift; the owner's "subtle" |
| 700 | a touch more; separable from 600 only side by side |
| **800 -- SHIPPED** | the stars plainly travel and change size; at f008 the right star comes NEAR the lens edge without reaching it |
| 900 | **first rung that draws attention to itself**: at f008 the right star's arm sits ON the lens rim |
| 1000 | a deliberate look, in a clip whose subject is resting |

Localisation measured, not assumed (committed framediff): 600 -> 800 moves at
most **217 px** on Rest's worst frame inside **x=171..241 y=159..176**, and
**276 px** on Taunt II inside **x=164..242 y=164..183**. Two lenses, nothing
else -- not the body, the band, the bolt or the motes.

**Contrast checked deliberately at 800, not assumed to survive the raise.**
P25-LOOKS/E04 lays Curious and Startle beside Rest at 800 and at 600. The
authored beats' stars are several times the size, swing across the whole lens
and change size beat to beat; the ambient layer is a sliver drifting inside a
narrow lens. Curious, Startle and Taunt III remain plainly the loud ones, and
they take gain 0, so their byte-identity is asserted by name in the matrix.

---

## ITEM 3 -- the rear-calm lever: LADDERED, AND IT IS NOT THE LEVER

Laddered on Hover at 1000 / 800 / 700 / 600 / 500 / 420 / 300 / 150 / 0 -- the
whole range, ending with the knob switched fully off.

### It does not calm the back ball. Measured, then looked at.

mrear, root-local, the posed skin ring at the C knuckle (the back ball is a
piece of SURFACE, and mrear measures the surface -- its own comment says why),
every key and midpoint of the clip:

| rung | C path | vmax | amax | jmax | jrms |
|---:|---:|---:|---:|---:|---:|
| 1000 | 11269 | 39.0 | 16.36 | 17.45 | 6.665 |
| 500 | 11160 | 38.2 | 17.68 | 18.52 | 6.763 |
| 300 | 11136 | 38.8 | 17.88 | 19.25 | 6.682 |
| **0** | **11126** | **38.6** | 16.92 | 18.36 | **6.623** |

1.3 % of path, nothing in the derivatives, with the knob OFF.

**Looked at on CRACKLE**, which plays this same slot-0 choreography under a
FIXED camera -- so every pixel of movement is the creature, not the orbit
(CLAUDE.md: a screen-space motion metric on an orbiting clip measures the
orbit; Hover orbits, Crackle does not, and it is the same animation).
P25-LOOKS/R07: six frames across the rear's busiest window at calm 1000 and at
calm 0. Same motion, both rows. At native, four rungs side by side (R09) are
indistinguishable.

### The "52x" that sent this pass here was a CATEGORY ERROR

Pass 24's reviewer measured this knob at **9,352 changed pixels** on Hover's
worst frame against the rear ambient's **178**, and read it as ~52x the lever.
Both numbers are right. But **changed pixels between two configurations
measures an OFFSET, not a MOTION.** This knob shifts the whole inked rear band
by a native pixel or two, and a one-pixel shift of a large contoured shape
repaints thousands of pixels; the rear ambient moves a 20x16 patch. Neither
number says anything about how much the ball MOVES over the clip, which is the
whole content of "too finicky and moves too much".

This is the art law's failure mode in a new costume: a real measurement, quoted
for a question it cannot answer.

### Why no value here could have worked

The knob scales carrier C's OWN rotation (hinge play at C, the knead's grip,
out-of-plane and wag at C) plus nodule C's small translation. **A joint's own
rotation moves what is downstream of it; it cannot move the joint.** The back
ball IS at C, and its position is written by JunctionF, Neck, HingeA and HingeB
upstream -- the antenna's life, bank-wide, approved four directions running.

Everything else reachable was swept on the same measurement:

| knob | C path | jrms |
|---|---:|---:|
| shipping | 11152 | 6.686 |
| rear ambient 400 (pass 23) | 11152 | 6.686 |
| rear ambient 0 | 11152 | **6.686 -- EXACTLY zero effect** |
| knead dip clip 0 | 11419 (higher) | 6.440 |
| knead dent swing 0 | 11149 | 6.583 |
| knead dent depth 0 | 11159 | 6.471 |
| fold dip 0 | 11152 | 6.686 |

The rear ambient -- the lever Direction 25 named and pass 24 built -- moves
carrier C by **exactly zero**, which is consistent: it is the End carrier, one
joint further back. Nothing available moves the back ball by more than a few
per cent, and removing the knead dip makes it travel FURTHER.

### What ships

Every slot takes `kRearCarrierCalmPm`, so **item 3 changes no bytes**. The
per-clip table and its `ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM` ladder stay,
because the knob is now real in production (it was readable by one gate binary
from pass 20 to pass 24) and a per-clip lever the owner can turn is worth more
than a value chosen to look like delivery.

**Hover's front is untouched, as instructed.** No other clip shows the fault,
so nothing else was changed.

---

## A non-live control subject that moves, found by checking rather than assuming

`manafold-crackle-legacy` is not in the live 22, so the byte-identity legs do
not cover it. Checked by hand against the pass-24 binary anyway, because item 1
claims to be "keyed on the NAME so the control subjects are untouched":

| subject | pass 24 | pass 25 | cause |
|---|---|---|---|
| `manafold-still` (slot 7, diagnostic) | 0xF5F85615 | **same** | eye gain 0, not live -- protected as designed |
| `manafold-nodule-solo` (slot 16, diagnostic) | 0xBA906B0A | **same** | as above |
| `manafold-crackle-legacy` | 0x92B5FD7C | 0xCFA3D9DE | **item 2 alone** |

Isolated: `ZHAO_U02_BOLT_ROLLOUT=pass24` and `..._CLEARANCE_MM=46` do NOT restore
it (so item 1 genuinely does not touch it -- the name key works), and
`ZHAO_U02_EYE_AMBIENT_CLIP_PM=0:600,23:600` restores it **exactly**.

**It reads SLOT 0's entry, not slot 23's**, because `knead_schedule_slot(23) -> 0`
sends every scheduled layer on the fixed-camera idle to slot 0 -- the same
structural fact pass 24's reviewer found about `manafold-crackle`. So slot 23's
eye entry is, like the rear ambient's and the rear calm's, a defensively equal
value that is never read.

This subject's source comment says it "must reproduce the version-18 Wave D
manafold-crackle bytes exactly". **That has been stale since pass 24**, which
introduced the eye layer at 600 on the same schedule slot; pass 25 moves it
again. It is a non-live comparison subject for the LIGHTNING variant, not for
the eyes, and it is not published -- so it is reported, not silently repaired.
The repair is not free: the eye gain is per CLIP SLOT and crackle-legacy shares
slot 0's schedule with hover, inspect and crackle, so exempting it needs a
per-SUBJECT eye selector, which is a new mechanism on a final pass.
