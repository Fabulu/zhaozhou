# REVIEW — Manafold pass 4

**Reviewer run:** adversarial gate against `Upheaval/creature/10-GATE-CHECKLIST.md`
and `OWNER-DIRECTION-4-2026-09-05.md`.
**Judged on:** my own build of `a6495159` at `C:\zrevbuild` (`build-direct.sh cel`,
never `cmake --build`), plus a pristine pass-3 baseline worktree at `dd85b719`
built into `C:\zrevbase`. `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`,
native 384x240 first, magnified after.
**Evidence:** `evidence/review/`.

---

## The one-line verdict

**The mechanism is real and the honesty is exemplary; the READ is not yet won.**
The fold is genuinely arithmetic on the posed bones — I verified there is no
proximity or collision term anywhere in the mana path, and that the stencil
positions are a weighted sum of posed anchors. But at the shipping camera the
shapes do not name, the cause is measurable and is *not* the one the implementer
named, on several clips the mana **covers the antenna** rather than being shaped
across a gap, and two clips never reach the knead phase at all. Alongside that:
one **undeclared regression on a protected read** (eye protrusion, −25%), one
**dead owner knob**, and one **repo fault that makes the creature render black
from a clean checkout**.

---

## Calibration first (checklist 4)

Before trusting any delta I reproduced the shipped render from my own build:
`manafold-hover`, 600 frames, **`meta.txt` byte-identical** to
`website/scratch-reel/manafold-hover/meta.txt` — every one of the 600 frame CRCs
plus `sequence_crc32c=0x5B44FCF2`, and five spot-checked `.rgb` frames identical
byte for byte. Every measurement below that reads `scratch-reel` is therefore
reading my own build's output.

Instruments (checklist 6) re-run by me — `evidence/review/instrument-selftests.txt`:

| instrument | can it fail? |
|---|---|
| `inkmask.py` | **yes** — dilated outline detected (2000 vs 2496 px), all-empty input refused as *vacuous* |
| `inkwidth.py` | **yes** — 2px/5px rings measure 2.0/5.7, dilation detected, empty frame refuses to report |
| `trajplot.py` | **yes** — legacy row-median exhibits the 384-px horizon fault on the same frame where the plate mask reports 0 |

All three are honest. The two pass-3 vacuous proofs are genuinely repaired.

---

## Verdict per acceptance item

### 1. The creature is called Manafold and the folder is renamed — **PASS**

Verified: `git mv` in both repos, 22 `manafold-*.webm` published, **zero**
`unnamed02` files left in `public/renders`, Manafold first in the page ordering,
`u02::`/`kU02*`/`U02_*` shorthand kept and documented atop `manafold_art.h`.
The rename is proven inert by my own byte-identical hover reproduction above.

### 2. It folds mana into recognisable shapes and kneads them into new ones, continuously, without touching — **PARTIAL. The mechanism passes; the read fails.**

**What I verified, first-hand and structurally — and it is the good news:**

* Stencil point positions are `sum(w_i * posed_anchor_i)` with fixed integer
  mean-value weights (`manafold_fx.h` `fold_mvc`). *The shape folds because the
  rig folds*, by construction. There is no other position law.
* **No collision, no proximity, no distance-to-antenna test exists** anywhere in
  `manafold_fx.h`. I grepped for it. R1 is honoured structurally, not by
  convention. This is the correct kind of answer to "at a distance".
* GRIP (anchor polygon area), KNEAD (anchor speed excess over a 64-frame EMA)
  and DRAG (hinge B's lagged velocity) are pure joint-state functions.

**The ablation gate, re-run by me** (`U02_ABLATE_KNEAD=1` vs unset, 600 frames
each, `evidence/review/ablate-native.png`, `ablate.py`):

| metric (cyan mana mask, 600 frames) | knead ON | knead OFF | delta |
|---|---|---|---|
| pixels/frame (mean) | 1658 | 2085 | **−20.5%** |
| bbox fill fraction | 0.20 | 0.23 | −15.0% |
| height (px) | 78.5 | 83.9 | −6.5% |
| rms spread | 25.2 | 25.6 | **−1.2%** |

**The gate does not fail — but the implementer's wording overstates it.** With
the layer off the mana is a slightly taller, slightly looser plume; with it on it
is a lower, tighter cap. That is a real difference and it proves coupling. It is
not "limp vs gathered", and a viewer shown both would say "the second one's smoke
is a bit taller", not "the first one is folding shapes and the second is dead".

**And the ablation cannot carry the weight put on it.** `antenna_knead` writes
rotations onto the *bones*; zeroing it changes the antenna pose too. So the pair
cannot separate "the mana follows the rig" from "the projected mana moved because
the rig moved". **The decisive proof is the code, and the code is sound** — say it
that way rather than resting on the render.

**The naming test at native — I cannot name them.** `evidence/review/hold-native.png`
(hover HOLD frames 128/140 = RING, 374/400 = STAR, at 1x). At native what I see is
a cyan puff over the antenna. I cannot name RING. I cannot name STAR. At 4-6x
(`ring-compare.png`) the **RING does read** — a clear annulus with a hole. STAR,
BAR and TRIANGLE do not become nameable even at 6x.

**The cause is not the one declared, and it is measurable.** I rendered the
committed X-ray (`U02_FOLD_LOCK=1`: coherence forced to 1000, no cloud, no
jitter, no drag) — `evidence/review/lock-4x.png`. **The X-ray looks the same.** So
the shape is not being swallowed by the relaxed cloud; the stencil geometry
itself does not resolve. Connected-component counts on the bright core, above the
face, in the X-ray:

* f128 (RING HOLD): 869 core px in 24 components — but **457 + 194 = 75% of it in
  two fused blobs**.
* f374 (STAR HOLD): 701 px, **497 + 175 = 96% in two fused blobs**.

The stencil footprint is ~56x59 px; the mote halo is `kMoteHaloRPxMin/Max = 7..10`
px, i.e. a **14-20 px stroke on a 56 px shape**. The 18 stencil stations
(`kStencilPts`) sit ~10 px apart around a ring of that radius, under a 14-20 px
brush — every neighbour overlaps its neighbour by 1.4x to 2x, so the polyline
closes into a slab before it is ever drawn. *You cannot draw a four-point star
with a brush a third as wide as the star.* This is the direct
collision between "make the particles BIGGER" (§2, stated twice) and "fold into
recognisable shapes" (§0) and **it needs an owner call**, not another tuning pass.

**Coverage gap — the second half of the sentence never happens on some clips.**
From the committed telemetry (`U02_FOLD_DEBUG=1`, my renders): the cycle is
gather 30-45 keys + hold 32-63 + knead 30-59 + release 14, and shape morph only
occurs during KNEAD. Measured segment spans:

| clip | frames | gather | hold | knead | release |
|---|---|---|---|---|---|
| `manafold-hit` | 140 | 0-61 | 62-111 | **none** | 112-139 |
| `manafold-startle` | 160 | 0-89 | 90-131 | **none** | 132-159 |
| `manafold-curious` | 180 | 0-69 | 70-133 | 134-151 (18 fr) | 152-179 |
| `manafold-taunt2` | 240 | 0-73 | 74-149 | 150-211 | 212-239 |
| `manafold-damage` | 464 | 0-59, 244- | 60-151 | 152-243 | — |

On `hit` and `startle` the mana **never kneads into a new shape at all**. And the
600-frame hover showcase holds exactly **two** shapes — RING then STAR — before
its release tail; BAR gathers and is cut off. Those two happen to be the best-
and worst-reading shapes in the vocabulary.

**The mana covers the antenna.** Badness-ranked sweep, worst-mana frame of each of
16 clips (`evidence/review/worst-mana.png`, `worst.py`): on `rest`, `pirouette`,
`taunt2` and `damage` the cyan mass buries the loop entirely. `taunt2` frames
113 and 164-168 (`evidence/review/taunt2-166.png`) show **the antenna completely
invisible inside a solid cyan mass** — precisely the "one cloud swallowing the
antenna" that iteration 1 declared a FAIL. The acceptance read is "*they* did that
to the mana, with nothing ever touching". A cloud sitting on the hand is not a
field being shaped across a gap.

**What is genuinely right here, and must be protected:** `drift` (f179) and
`hasty` (f131) show the mass pulled sideways a beat behind the antenna's sweep,
trailing off the creature and decaying. That is *exactly* the iron-filings read
the direction asked for, it is the strongest evidence in the whole pass, and the
DRAG term that produces it should not be touched.

### 3. Tumour balls gone; ball at back and front junctions; a bone at every ball — **PASS**

Verified by my own probe run and by looking (`evidence/review/s4-zoom.png`, 6x on
front/side/three-quarter). `kBJunctionF` is a real hinge carrying the old
`kBNeck` bind verbatim; my probe reproduces both surface crossings exactly —
**(83, 735) exiting and (−328, 467) entering** — which are the ball sites.
`kBLoopBase2` carries authored rotation in the knead layer. No tumour anywhere.
Minor note, not a fault: the tube is a *blade* (`rz` 27 vs `rx` 60 mid-span), so
the knuckles read as flattened lozenges from some angles rather than the sheet's
round knuckles.

### 4. Antenna thin, thickening only at the junctions — **PASS**

`kLoopBladeRxMm = {130, 78, 64, 62, 60, 66, 72, 82}` — free tube 60-66, one
junction station at 78, back end 82, knuckles 118. Confirmed by eye at 6x: the
frontmost tube is thin and the flare is confined beside the ball.

### 5. More, bigger particles, smoother rotation, fewer lightning lines, glitchier smear, odd drifters — **PARTIAL, needs an owner eye on one trade**

More particles: 7 → 24. Fewer lines: `kStrandCount = 1`. Odd drifters: 3 wander
motes, and they are visible and lovely on `fall` and `hasty`. Glitchier smear:
the BROKEN-BUFFER rung with the row tear ships, and it is genuinely glitchy.

**"Bigger" is a trade that was made without being named as one.** The halo went
9 px → 7-10 px while the opaque core went 2 px → ~4.9 px. The net read *is*
bigger, more solid particles, and that is defensible. But that same knob is what
fuses the stencil into a blob (item 2). The owner asked for bigger particles
twice and for nameable shapes once; **they are the same knob pulling opposite
ways** and only he can settle it.

**An uncredited win, found while building the baseline:** I rendered pass 3's
`unnamed02-hover` from my own `dd85b719` build (after regenerating its page —
see the page fault below) and **pass 3's hover carries no mana at all** —
`evidence/review/p3-vs-p4.png`, left tile: a clean creature, zero cyan pixels
across the clip. Pass 4 put folding mana on **every shipping clip**. That is the
"a loop of that going on all the time as the creature exists" half of §0
genuinely delivered, and it is a much bigger step than the QA note conveys.

### 6. The smear is depth-correct — **PASS**

Code verified: the smear plane records the nearest contributing splat's 1/w per
cell at feed, and `smear_composite` applies `glow_splat`'s own comparison at cell
granularity. Look verified: on `hasty` (`evidence/review/smear-occl.png`) the
trail is **cut cleanly along the terrain silhouette** — unambiguous depth
occlusion, not a coincidence of shape.

Undeclared residual, low damage: the remembered depth is a **feed-time snapshot**
and decay leaves it alone, so a persisted cell keeps a stale depth. A creature
that later moves into where an old trail was will not re-occlude it correctly.
Worth a line in the hardware asks beside the existing note.

### 7. Eyes — **PASS on every stated requirement, with one visual shortfall and one undeclared REGRESSION**

* **Not touching** — clear pink channel between them. PASS.
* **Rotated outward** in a Λ that matches `Concept/Front.png`. PASS.
* **Teardrop, pointed top, round bottom** — **PASS.** At 6x on the level
  diagnostic pose (`evidence/review/eyes.png`, X1 left vs X2 right) the shipped
  lens is clearly sharp at the top and blunt at the bottom, and visibly sharper
  than X2 beside it. The experiment is real and the right candidate shipped.
* **Whites track pupils** — **PASS, by construction and verified by me:**
  `make_white_ring(kBPupilL/R)` and `make_star_blade(kBPupilL/R)` ride the *same
  bone*. They cannot desync. Confirmed visually at three gaze positions of
  `manafold-curious` (`evidence/review/gaze.png`): the white arc travels with the
  star, and the star stays inside the lens — pass-3's fault 1 is repaired.
* **Experiments shown** — X1/X2/gaze plates in `creature/Manafold/media/eye-experiments/`
  and on the site's Experiments row; X3 refused on cited source lines (I did not
  independently re-verify those line numbers — **inherited**).

**Shortfall:** the white reads as a **one-sided crescent**, not a ring around the
star. On the sheets the white fully encircles the cyan star and is what separates
it from the purple. Mechanically the requirement is met; visually the white is a
sliver.

**REGRESSION, undeclared — the eye protrusion.** The committed probe, run by me
on my own build, reports:

```
u02-probe: eye crown ellip 1275 pm — stands 123 mm proud of the body
           (protected read: ~166 mm / 1369 pm; ...)
```

Pass 3's own reviewer evidence recorded **1365 pm / 164 mm**. Pass 4 ships
**1275 pm / 123 mm** — a **25% loss of protrusion on a read the probe's own
comment marks PROTECTED**, on a creature whose Description sheet says
*"abstehendes Auge"*. `TASK_LOG` line 91 says "eye crown 164mm preserved", but
that is a **Stage B** statement, made before Stage E rebuilt the lens; it was
never re-checked, and QA.md item 7 reports PASS without mentioning it. The
mechanism of the miss is that the probe **reports** this number rather than
gating it — which is exactly checklist 9's shape. Make it a gate.

**And I diagnosed the cause, which is not where anyone would look.** Running the
probe with `U02_EYE=x2` gives **exactly the same 1275 pm** — so the teardrop lens
is *not* to blame; X1 and X2 measure identically. Diffing the constants against
`dd85b719`: `kEyeXMm` (381), `kEyeLongMm` (250), `kEyeWideMm` (125) and
`kEyeDeepMm` (90) are **unchanged**. The only changed placement constant is
`kEyeZMm` **190 → 215** — the +25 mm separation that fixed "they touch at the
top". Pushing the eyes apart along z slid their crowns off the body's forward
pole toward its flank, where the same geometry stands less proud. **Fixing "they
touch" cost "they stand out"**, and nobody noticed because the two are different
acceptance items. The repair is to raise `kEyeXMm`/`kEyeDeepMm` to compensate —
which is precisely what the probe's own message has been saying all along.

### 8. Interior glow gone; outer layer more see-through — **PASS on the glow; the second half NEEDS AN OWNER EYE**

`kBellyGlowGainPm = 0`; no interior core and no dark hole on any frame I looked
at, across 16 clips' worst frames. PASS.

"More see-through" was implemented as one rung up the **mist** ambient ladder
(.32 → .36), chosen from a rendered .32/.36/.40 ladder — a documented plan
interpretation (R11), and the ladder is a good process. But the owner wrote
"the **first layer of the creature**", which reads at least as easily as the
body's own skin, and the body's surface is not translucent. **Flag for the owner:
was the mist what he meant?**

### 9. The outline question — **PASS, answered and independently reproduced**

I re-ran the committed `inkwidth.py` on six frames across both creatures at four
camera distances (`evidence/review/inkwidth-rerun.txt`):

| frame | ink px | median | p10 | p90 |
|---|---|---|---|---|
| zixxtrixx-walk 0080 | 2907 | 2.0 | 2.0 | 2.8 |
| zixxtrixx-idle 0100 | 2987 | 2.0 | 2.0 | 2.8 |
| manafold-hover 0100 | 606 | 2.0 | 2.0 | **2.8** |
| manafold-drift 0150 | 400 | 2.0 | 2.0 | 2.0 |
| manafold-curious 0080 | 461 | 2.0 | 2.0 | 2.0 |
| manafold-trick 0100 | 376 | 2.0 | 2.0 | 2.0 |

The answer is **cleaner than the implementer stated**. QA.md says "Zixxtrixx
shows a p90 of 2.8 vs Manafold's 2.0"; at the hover camera **Manafold's p90 is
also 2.8**. So it is not even a distance-adaptive difference between creatures —
at matched framing the two are indistinguishable, median and p90. One shared
screen-space law, same class, every distance. **Yes.**

### 10. Directional hit animations — **PASS, with a dead knob**

`manafold-damage`, slot 14, four authored contact stations (body-front/side/back
and LOOP-PEAK). The four directions are visibly different in the shipped plate;
the recoil is **airborne** — displacement, overshoot, damped settle, no plant and
no brace, which is the stated difference from Zixxtrixx; the antenna whips late;
the mana shatters for free through the coupling. My probe run: slot 14 min
clearance 416 mm, closure OK, travel covered. PASS.

**But:** `kKneadClipPm` is declared `[15]` and authored with **250** for the
damage clip, while the reader is

```cpp
const int gain = slot < 14 ? kKneadClipPm[slot] : 700;   // manafold_clips.h:497
```

Index 14 is **never read**. The damage clip silently runs at 700 — 2.8x the
authored value — and the owner's knob does nothing. One-character fix; but
CLAUDE.md law 6 is explicit that a knob that cannot be turned is the fault.

---

## The implementer's six declared doubts — probed

1. **Shape legibility at native.** Its own weakest point, and **worse than
   declared.** RING does not read at native either (only at 4x+), and the X-ray
   shows the cause is the stencil fusing under an oversized brush, not the cloud.
   Credit where due: the doubt was raised loudly and honestly, and every value is
   a named knob, so the owner *can* push it.
2. **The closure rim gate 1060 → 1120.** The number is **honest and I reproduced
   it exactly** — my probe run reports `worst arm RIM 1087 pm (slot 12 key 83)`.
   The *justification* is not sound. The gate was raised because the worst key was
   "rendered and LOOKED at — no visible breakout". I looked at that frame
   (taunt2 key 83 = frame 166, `evidence/review/taunt2-166.png`): **the entire
   junction region is buried under opaque cyan mana.** A breakout could not have
   been seen there either way. The look is unfalsifiable, so the gate *is*
   effectively fitted to its answer — and the headroom shrank while being
   described as "the same honest headroom class" (pass 3: 1011 → 1060, +4.8%;
   pass 4: 1087 → 1120, +3.0%). Not a fail — nothing is visibly broken and the
   ball does mask the entry — but the evidence class is weaker than claimed.
   **Ask:** a mana-off diagnostic lane so closure looks can actually see the arm.
3. **The headstand opts out (`kKneadClipPm[trick]=0`).** Correct call, correctly
   declared, and the probe caught it (declared contact −20 mm inside −60..−5,
   reproduced by me). The cost is visible though: `trick`'s worst frame shows the
   mana as loose sparks near the ground with no fold at all. Honest trade.
4. **The trio carries one strand per conduit.** Verified by looking — three
   creatures each with their own folding mana, and it does not read as busy. The
   `ii == 0` lie is genuinely ended and the trio is now an honest witness.
   (I did not independently re-derive the cost arithmetic — **inherited**.)
5. **The smear pair carries a geometry delta.** True, and correctly declared. It
   does not matter: I verified the mechanism from the code *and* from an
   independent look (terrain cutting the trail), so the pair is not load-bearing.
6. **~10 clips judged closely, the rest spot-checked.** I ran the badness-ranked
   sweep it asked for — per-frame delta, loop seam, mana and body pixel counts
   across all 22 clips, plus a worst-frame contact sheet of 16
   (`evidence/review/worst-mana.png`). **Nothing catastrophic hides in the
   unexamined clips**: no missing creature, no off-screen body, no broken frame,
   no black face. What the sweep *did* surface is in the fault list: the mana
   burying the antenna on four clips, and the hover loop seam.

---

## Faults, ranked by damage to the read

1. **The shapes do not name at native, and the cause is the brush, not the
   cloud.** The pass's headline feature is "fold the mana into recognisable
   shapes"; at the shipping camera it reads as a cyan puff. The X-ray proves the
   stencil fuses into 1-2 blobs because a 14-20 px stroke is drawing a 56 px
   shape. This is an owner-level trade against "make the particles bigger", not a
   tuning bug. **Fix first, and fix it as a decision.**
2. **The mana covers the antenna on `rest`, `pirouette`, `taunt2`, `damage`.**
   The whole acceptance read is "it shaped that, across a gap, without touching".
   A cloud sitting on the hand refutes it more directly than any legibility
   caveat. `taunt2` f166 is the exhibit.
3. **Eye protrusion regressed 164 mm → 123 mm on a PROTECTED read, undeclared.**
   The probe measured it and printed it; nobody read the line. Make it a gate.
4. **Two shipped clips never knead** (`hit`, `startle`), and `curious` kneads for
   18 frames. "Then knead it into new shapes… a loop of that going on all the
   time" is not true of the whole bank. The showcase hover holds only two shapes.
5. **`manafold_page.h` is untracked** — see below. Silent black creature from a
   clean checkout. Zero damage to what shipped, high damage to reproducibility.
6. **`kKneadClipPm[14]` is dead code**; the damage clip runs at 700 instead of the
   authored 250. An owner knob that does nothing.
7. **The white reads as a crescent, not a ring** around the star. The sheets'
   white encircles the pupil; ours is a sliver on one side.
8. **The hover loop seam is about twice the house norm**, relative to its own
   motion: seam 4.07 against a 0.77 typical inter-frame delta (ratio 5.3), where
   `zixxtrixx-idle` is 3.54 against 1.65 (2.1) and `zixxtrixx-look` 0.92 against
   0.39 (2.4). The release tail zeroes the fold amplitude but does not return the
   mana to its opening state. Needs an owner eye on the always-playing loop.
9. **The closure gate's look-evidence is unfalsifiable** (doubt 2 above). No
   visible breakout, but no way to have seen one.
10. **The body is still a ball on a stalk**, where the sheet is a continuous
    onion with no neck (`evidence/stageQ-recon-beside-sheets.png`). Pass-3 fault
    8, correctly declared as out of Direction 4's scope, carried forward.

---

## A fault outside Direction 4's list, found by accident

**`tools/reel/manafold_page.h` — the creature's 800 KB texture page — is
UNTRACKED in git.** `manafold.h:33` guards the include with `__has_include`, so a
clean checkout **compiles without error and renders Manafold entirely BLACK**.

I found this by building the pass-3 baseline: `unnamed02-hover` came out as a
black silhouette with a purple belly dot
(`evidence/review/page-fault.png`, left tile; the right tile is the same build
after I regenerated the page). That is the untextured-black bug that gate
checklist item 2 exists *because of*. Zixxtrixx's page headers
(`zixxtrixx_page.h`, `zixxtrixx_page_cel.h`) **are** tracked; Manafold's is not.

**Then I proved it at HEAD** (`evidence/review/page-untracked-proof.txt`,
`clean-checkout.png`). `git worktree add --detach C:\zrevclean a6495159` →
`manafold_page.h` absent → `build-direct.sh cel` **succeeds with no warning** →
`u02-s4-front` renders **4 frames, 77 unique colours**, a solid black creature,
against the 803 colours the working tree produces from the same commit. Left tile
black, right tile correct, same SHA.

Mitigating: the generator is committed and **deterministic** — I regenerated with
`python tools/pack/mkmanafoldpage.py` and the output is **byte-identical** to the
untracked file, so nothing is lost and the shipped renders are correct. But the
repo cannot currently reproduce its own creature, and the failure is silent.
**Either commit the page as Zixxtrixx's is, or make the missing-page path a hard
build error for shipping subjects.** (This is inherited from pass 2/3, not
introduced here — but pass 4 renamed every other file in the family and this one
was moved on disk rather than in git, which is when it should have been noticed.)

**I deliberately did not fix it.** The two repairs are genuinely different
decisions — an 819 KB generated header in the tree, versus a build that refuses
to link a pageless shipping creature — and quietly picking one would pre-empt a
call that is not a reviewer's to make. `manafold_page.h` is left untracked and
this review is the record. Whoever takes it: **the hard-error route is the better
one**, because it also protects every future creature, and the generator is
already committed and deterministic so nothing needs storing.

---

## What is RIGHT and must be protected

* **The fold's construction.** Stencil position = fixed integer mean-value
  weights over posed anchors, and **no proximity or collision term exists in the
  code at all**. Whatever happens to legibility, *do not* let a future pass
  "help" the shapes read by snapping motes to the antenna. The structural
  guarantee is the feature.
* **Mana on every clip.** Pass 3's hover had none; pass 4's whole bank carries a
  folding field with zero per-clip mana authoring, because the coupling reads the
  rig. Whatever the shapes end up looking like, keep that.
* **The DRAG term and the wander motes.** `drift` and `hasty` are the two frames
  in this pass where the read actually lands: the mass pulled sideways a beat
  behind the sweep, wandering off and decaying. That is the iron-filings answer.
  Protect it.
* **The depth-correct smear.** The right fix (the splat's own comparison, at cell
  granularity) rather than a draw-order hack, and it is visibly working against
  the terrain.
* **`kBJunctionF` carrying the old `kBNeck` bind verbatim.** Every accepted pivot
  — lasso, drift trail, trick flex, rest yaw — kept its exact pivot while a new
  hinge was inserted above it. That is how you add a joint without relitigating a
  pass's worth of animation.
* **The X1 teardrop eye**, and the A/B that chose it. It reads as the sheet's
  shape, the star stays inside the lens, and the whites ride the pupil bone so
  they *cannot* desync. Do not go back to a two-constant almond.
* **The three repaired instruments.** All three now demonstrate a real failure
  mode on command. This is the single largest process improvement in the pass and
  it fixed a fault that had shipped twice.
* **The committed probe.** It caught two genuine regressions during the pass (the
  knead lifting the headstand off its declared contact; the closure rim past its
  gate). It can fail and it did. Extend it — do not trim it.
* **The honesty.** Six doubts declared unprompted, the weakest point named as the
  weakest point, five author-render-look iterations logged with what each changed,
  and deliberate deviations listed rather than buried. Three of my findings started
  from its own flags. That is what makes a gate cheap.

---

## Verified vs inherited

**Verified first-hand by this reviewer:**

* My build of `a6495159` reproduces `manafold-hover` **byte-identically** (600
  frame CRCs + sequence CRC + 5 raw frames).
* The ablation gate, re-rendered both ways, measured over 600 frames, and looked at.
* The fold-lock X-ray, rendered by me; the connected-component fusion counts.
* The fold segment coverage table, from `U02_FOLD_DEBUG=1` renders I made.
* The badness sweep over all 22 clips and the 16-clip worst-frame contact sheet.
* The committed `manafold-probe` re-run: clearance, headstand contact,
  **closure rim 1087 pm at slot 12 key 83**, both surface crossings, travel,
  **eye crown 1275 pm / 123 mm**.
* All three instrument selftests, including their failure cases.
* `inkwidth.py` outline measurements on six frames at four distances.
* **Zixxtrixx untouched**, from a pristine `dd85b719` worktree I created and
  built myself (`evidence/review/zixx-identity-by-reviewer.txt`):
  `zixxtrixx-walk` `0xF06EF66B`, `zixxtrixx-idle` `0x1408F885`,
  `zixxtrixx-damage` `0x6C224D56` — sequence CRCs identical baseline vs final,
  **and all 1,136 frames byte-identical** (SHA-256 per frame, 0 differ). Gate-off
  path `ZIXX_SUNS=off` walk `0x86536E70`, **160/160 frames byte-identical** —
  checklist 17 independently confirmed.
* `manafold_page.h` untracked, `__has_include`-guarded, byte-identically
  regenerable, **and a clean checkout of `a6495159` demonstrated rendering the
  creature black** from a worktree and build I made myself.
* The eye-crown regression's cause isolated: `U02_EYE=x2` measures the same
  1275 pm, and only `kEyeZMm` changed (190 → 215) among the eye constants.
* Site: 453 declared media references, **0 missing**; exactly one `<meta
  name="robots" content="noindex">`; Manafold first; both archive generations
  present; zero `unnamed02` current-generation media.
* The eye whites/star riding the same bone, from the call sites.
* No proximity/collision term in the mana path, from the source.

**Inherited, not re-derived:**

* The other **19** Zixxtrixx sequence CRCs and the 5-subject byte-identity claim
  (I checked 3 subjects and 1 byte-for-byte; the implementer's table is
  consistent with everything I did check).
* The multi-conduit cost arithmetic.
* X3's refusal line numbers (`zref_creature.hpp:393`, `zref_texture.hpp:129-142`).
* The `.32/.36/.40` and `.20/A26` ladder picks — I looked at the plates and agree
  they are a reasonable choice, but I did not re-render the ladders.
* The deploy's production verification (I did not fetch the live site).

**Could not confirm:**

* Whether the closure rim at 1087 pm is visibly acceptable at `taunt2` key 83 —
  the mana occludes the region under test. Nobody can confirm it from that frame.
* Whether "the first layer of the creature" means the mist or the skin. Owner's
  call.
* My first pixel-based attempt to track whites against pupils across `curious`
  was **contaminated** (the purple lens mask caught the smear plane's lavender)
  and reported nonsense; I discarded it rather than report it, and fell back to
  the construction proof and looking. Recorded here per checklist 6 — a check
  that cannot be trusted must not be quoted.

---

## Refutations of my own first reads

* I first read the shipped eye as an **inverted** teardrop — round at the top,
  pointed at the bottom — from the 3x face plate. That plate is a steeply tilted
  pose; on the level `u02-s4-front` diagnostic the lens is correctly pointed at
  the top and round at the bottom. **The pose fooled me.** This is the mismatched-
  pose trap in CLAUDE.md wearing new clothes, and it nearly cost the implementer a
  false FAIL on item 7.
* I first read the RING as not existing at all. At 4-6x it plainly does, and the
  ablation-on frame is nearly identical to the X-ray at HOLD — the coherence is
  already at ceiling. The failure is scale, not gathering.
* I first suspected the closure gate re-derivation was a fabricated number. It is
  not: my probe reproduces 1087 pm exactly. Only the *look* that justified the
  headroom is unsound.

---

## Background work

Every background job started by this review was a build, a render, a byte compare
or a Python sweep; all were either awaited to completion or explicitly stopped
(`TaskStop` on one long `grep` and one slow `cmp` loop I replaced with hashing).

**Checked with PowerShell, not with `ps`** — and this matters. Git Bash's `ps`
reported nothing running; `Get-Process` showed **17 live `g++`/`cc1plus`
processes**. `Get-CimInstance Win32_Process` resolved every one of them to
`C:\programmieren\zencrifice\zhaozhou` with Verilator `VM_*` defines — **the
hardware agent's lane, which this review is forbidden to touch**. They were
spawned seconds before I looked and are not mine. **Left alone, deliberately.**
Checklist 22 says agents report "nothing running" with processes alive; the same
tool gap can just as easily get another agent's build killed. `ps` is not enough
on this machine.

Zero of my own processes remain. The reviewer worktrees `C:\zrev` and
`C:\zrevclean` were removed with `git worktree remove` (`git worktree list` shows
only the lane), and the build trees `C:\zrevbuild` / `C:\zrevbase` are scratch
outside both repos.

**Nothing was published and no authored art value was changed by this review.**
