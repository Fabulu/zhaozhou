# P21 IMPLEMENTATION — RODS AND BALLS, built

**Date:** 2026-09-21 · **Branch:** `manafold-pass21`
**Direction:** `OWNER-DIRECTION-22-2026-09-20.md` and the owner's same-day
addition, verbatim: *"don't let actual antennae parts bend, just stretch. the
bending is at the ball joints."*
**Design:** `P21-ARCHITECTURE.md` (built as specified except where §6 says
otherwise, and every deviation is argued here).

---

## 0. What shipped, in one paragraph

The antenna is now a **ball-and-stick rig on the existing 27 bones**. Every run
is a straight rod skinned between its parent joint bone and that span's
pure-translation helper — the two carry the *same* rotation, so a rod ring is a
convex combination of two points on one posed line and **the rod cannot bend at
any joint angle or any span delta**. Every joint is a rigid ball on its own bone,
with the rods ending 1 mm from the pivot and handed to the ball by a cone that is
inside the convex ball in every pose, so there is no LBS blend across a joint
anywhere on the band. The rear rod's length **is the chord**: the pass-20 bow is
gone as a curve, and with it the End spazz. Measured on the posed surface over
2,858 samples: worst centreline turn *inside* a rod **0.02°**, worst rod sag
**0.02 mm**, **99.97 %** of the band's articulation at the four balls. The
exact-off selector `ZHAO_U02_RIG=pass20` reproduces the pass-20 bank byte for
byte. Ring count is unchanged at 64 — deliberately, so no gate, probe or receipt
inherited a stale ring table.

---

## 1. Stage by stage: falsifier, what was built, receipts

### Stage 1 — the selector, the station table, the `rods` ladder

**Built.** `RigMode`/`kRig`/`g_u02_rig` and `rig_rods()` in `manafold_art.h`;
`ZHAO_U02_RIG` parsed in `apply_knead_dip_env` so **every** `main()` reads it
(the pass-20 lesson: an env control is only a control in a binary that reads it);
printed **first** by `print_judged_config` (the pass-20 banner was blind to
mechanism selectors). The authored station table is `rods_ring(i)` — base 4
rings, then (rod, ball) ×4, then tail 4 — built from named per-element counts and
per-ball radii, with a **compile-time assert that all 64 stations are distinct**
(two rings at one bind y would conflate in every `ring_map()` the gates build).
`rods_ring_bones()` in `manafold_model.h` is the whole bone palette, one place.

**Falsifier (the architecture's): `mmeshcheck` CLEAN plus ZERO turn on every rod
ring at rest.** Result: `mmeshcheck` **CLEAN on both rigs** — 34 meshlets, 2,800
tris, 1,416 position groups, 4,200 edges, identical counts either way. On Still
(slot 7) the probe read worst rod turn **0.01°**, worst shear 0.02°, worst sag
0.01 mm, **999.7 pm** of articulation at the balls. Receipts:
`P21-RECEIPTS/mmeshcheck-rods.txt`, `mmeshcheck-pass20.txt`.

**The one place the architecture was not followed, and why.** §3.2 budgeted
~70 rings and said the ball ring counts drop before rod density does. The layout
was instead budgeted to fit the **existing 64**, because `kLoopRings` sizes an
array and keys a ring in a dozen instruments; changing it would have put a stale
ring table in all of them at once, which is the pass-19 blindness pattern. What
changed is the station LAW, and every consumer reads one function,
`loop_ring_station_at()`.

### Stage 2 — the front rod

**Built.** The F→A rod is bound `kBNeck` → `kBSpanDeltaA` over the whole run,
helper fraction 1 (`span_helper_delta_fx` returns the full delta under rods).
`kBFrontRootDelta` is inert. The `kBNeck` rotation that pass 20 blended into the
skin over 558–790 mm — a bend in the middle of a run — is now composed at the F
pivot and the front rod is one frame.

**Falsifier: rod F→A turn ≤ 1.5° on every sample.** Worst over the whole run
set: **0.02°** (and the worst reading is on the rear rod, not this one). Ring 12,
which read 13.4° mean / 25.6° max under pass 20, is a rod ring reading 0.

### Stage 3 — the rear

**Built.** `write_rear_bow` returns immediately under rods. Three helpers carry
station-proportional shares of one chord delta (`rods_rear_share_fx`), the last
landing exactly on the End ball centre. The v18 rotation staging (1/3, 2/3, 1
over rings 50–54 — three more corners on a run) is identity and skins nothing;
the four root helpers carry zero translation. Same on the baked midpoints.

**Falsifier: rear rod turn ≤ 1.5°, no rate stripe > 8°/sample, hand-off 0.**
Worst rear rod turn **0.02°**; worst rod turn RATE **0.02°/sample** against
pass 20's 9.53. `mrear`'s **HAND-OFF ROTATION 0.00°** on the whole bank.

### Stage 4 — roll-stable aims: **NOT MADE, and the falsifier is why**

§3.6 proposed moving five aims onto `nodule_aim_rollstable`, on the strength of
an 8.6 °/sample residual at ring 37 under the legacy law. **Ring 37 is a corner
ring of the pass-20 polygon, not a rod ring**, and under rods it does not exist
as a corner. So the question was measured before the change was made, with the
instrument §5.4 asks for — a blade-roll trace on the rods themselves:

| rig | worst blade-roll step (Inspect, Taunt III, Channel) |
|---|---|
| pass-19 (legacy bow) | 4.84 °/sample |
| pass-20 (arc bow) | 4.84 °/sample |
| **pass-21 rods** | **5.10 °/sample** |

against the 8 °/sample bound §5.4 names. **The falsifier passes without the
change**, so the change was not made: swapping five aim primitives moves every
byte of every clip for no measured gain, and the pass-20 close kept
`nodule_aim` verbatim for exactly that reason. Assumption stated, cheap to
reverse (one call site each).

⚠ **The first version of that trace read 88.9 °/sample and was WRONG** — it
measured the spoke's angle against the loop plane, which folds at 0 and 180 and
cannot separate "the blade rolled" from "the rod turned". The shipped measure is
parallel transport: carry the previous sample's spoke along the rod's own rigid
turn (Rodrigues) and take the residual about the rod axis. The 88.9 was the
instrument, and pass 20's rig read the same 88.9 — two rigs agreeing on an
absurd number is the tell.

### Stage 5 — the look

Eight clips rendered in production ink (`ZIXX_EXP=celmain`,
`ZIXX_LIGHT=diagonal-cool-cross`): Inspect, Hover, Rest, Drift, Channel,
Taunt III, Trick, death-drop — 3,538 frames, every one on a contact sheet in
`P21-LOOKS2/`. Frames for the close looks were chosen by the gate's own worst
rings and by pass 20's by-badness dip census, never by index. Notes in
`P21-LOOK-NOTES-2.md`. **The look changed two shipped values; see §3.**

### Stage 6 — gates, controls, identity

See §4 and §5.

### Stage 7 — the ball radius ladder

See §3.

---

## 2. Chosen values, with the visual reason

| constant | shipped | why, by eye |
|---|---|---|
| `kBallRxMm` | `{101, 85, 101, 106}` | A/C/End are 1.4× the carrier profile: at 1.0× the ball does not READ (three pixels of difference on a six-pixel band) and the antenna becomes a bent wire; at 1.8× it is version 18's rejected protruding bead. B is 1.23× — see §3.2. |
| `kBallRzMm` | `{90, 68, 71, 84}` | same ratios on the thin axis, so the blade stays a blade. |
| `kBallRyMm` | `{72, 69, 72, 76}` | the carrier profile, NOT laddered: the ball is a slightly oblate bead, wider across the band than long, which is what a knuckle on a band looks like from the side and keeps the 340 mm A→B run long enough to read as a rod. |
| `kTrickPlantRootRodsMm` | `1545` | lands the deepest planted vertex on exactly the declared −25 mm. Rig-dependent — see §3.2. |
| `kRodsRearHelperStationMm` | `{1986, 2322, 2660}` | the rod's thirds. Not a shape: three stages exist only so a 6-bit LBS weight over a 1010 mm rod has ~9 placement levels per ring instead of 3. |
| `kRodsFrontCoreHalfMm` | `140` | the window the public carrier proof reads for F — wider than a carrier core because F has no ball of its own (the body is its ball). |

Every one of these is a named, editable constant. `ZHAO_U02_BALL_PM` is the live
cross-section ladder knob and appears in `print_judged_config`'s row table.

---

## 3. The two things only LOOKING found

### 3.1 The balls did not read, and every gate said OK

At the architecture's default radii (today's profile at the carrier) `mrod` read
**ALL LEGS OK** on the deep-knead frame — rods straight, articulation at the
balls, stretch uniform, balls rigid — and the render showed **an antenna with the
articulation in exactly the right place and nothing visible at it**. On a band
six pixels wide at 240p, a 1.56× knuckle is three pixels; the corners read as
sharp mitres and the whole thing read as a bent wire.

Four rungs at 8× on Inspect f280 (`P21-LOOKS2/P21-BALL-LADDER-RX-8X.jpg`) against
the pass-20 corner and the `Concept/Side.png` sheet chose **1.4×**. This is the
art law exactly: a component check passing is not likeness evidence.

### 3.2 The bigger foot broke Trick's plant, and the obvious lever does not work

Carrier B is the creature's **foot** for seventy keys of a headstand. At 1.4× it
reaches 15 mm further down and the committed ground probe read the approach three
keys ahead of the declared window at **32 mm against a 40 mm clearance floor**.

Raising `kTrickPlantRootMm` is the lever pass 6 and pass 12 both used for this
exact event. **It does not work here:** +16 mm of root moved the plant DEPTH by
+16 mm and the approach by **+3 mm**, because `build_trick` pivots the body about
the planted support centre, so the two quantities converge at the plant key and
not before it. Reaching 40 mm that way lifts the declared plant out of its own
accepted depth band.

Shipped instead: B's ball at **1.23×** (85/68), sized to the contact it has to
make, plus `kTrickPlantRootRodsMm = 1545`. Result: approach **44 mm**, deepest
planted vertex **exactly −25 mm** (the declaration; pass 20 read −36), carrier B
owns **140/140** of the window, depth range −25..−16. **The clearance floor was
not lowered and the contact window was not widened.** The Side sheet's three
balls are not the same size either.

⚠ `kTrickPlantRootMm` had to become **rig-dependent**
(`trick_plant_root_mm()`). It COMPENSATES for the mesh, so one value cannot serve
two meshes — and holding one number would have moved the pass-20 bank's bytes and
broken the identity leg this pass is checked by. A shipping constant silently
changing the control it is measured against is how an identity claim becomes
worthless.

---

## 4. The gates, and the controls fired for each

**`manafold-rodgate` (mrod), new, committed at `tools/reel/` and in
`build-direct.sh`.** Reads the POSED SURFACE through `decode_pose`/`skin_vertex`
on every key AND every baked midpoint. 8 clips, **2,858 samples**.

| leg | asserts | shipping | control fired |
|---|---|---|---|
| R6 rod turn | ≤ 1.5° inside every rod | **0.02°** | `--fail-rod-bend` 52.14° · `--fail-rod-twist` 52.14° |
| R6 rod shear | ≤ 5° | **0.04°** | `--fail-rod-bend` 89.99° |
| R6 rod sag | ≤ 4 mm off the rod's own chord | **0.02 mm** | `--fail-rod-bend` 328 mm |
| R7 joint on ball | ≥ 980 pm of articulation at the balls | **999.7 pm** | `--fail-rod-bend` 448.9 pm |
| R8 joint step | ≤ 8°/sample | **6.70°** | `--fail-joint-step` 10.92° |
| R8 rod turn rate | ≤ 8°/sample | **0.02°** | `--fail-rod-flicker` 9.53° |
| R9 rod uniform | min/max rail ≥ 850 pm | **947 pm** | `--fail-rod-uniform` 317.6 pm |
| R10 ball rigid | strain vs BIND ≤ 1 % | **0.059 %** | `--fail-ball-blend` 45.2 % |

Every control is a **real configuration**, not an asserted bug: three run the
same measurement against the pass-20 rig, one plants a rotation on the rear rod's
middle helper (a frame hand-off put back onto the band), one weights a ball's
equator half to the incoming carrier, one halves the sample rate. After the
repair the legs still assert the **correct** behaviour.

⚠ **R8's joint-step control had to be invented, because the pass-20 rig does not
fire it** (5.44° against an 8° ceiling). Quoting that leg's silence while its
only candidate control came back clean would have been the detector-reading-zero
fault again. The shipped control halves the sample rate, which proves the leg
reads the real per-sample joint change.

**`mrear`.** Re-run at the shipping values, **RC=0**.

* **R4's floor is re-based under rods, and the re-basing is argued in the
  source.** The pass-20 floors judge one number — the smallest rear rail — and
  were calibrated when that number's *distribution* was the fault. The rip was
  not "the band is short"; it was "rings 51–54 rigid as a block while the gaps
  either side took the whole share". 0.692 with that distribution is a flap;
  0.324 spread evenly over nine rings is a shorter tube. Under rods the fold
  detector is **uniformity** (mrod R9, 947 pm, with its own fired control) and
  this floor keeps only the hard bound (0.12). Both floors are **printed** so the
  retirement is visible.
* **The HAND-OFF leg needed a second operand.** `handoff_mm` is the *distance*
  between where b0 alone and b1 alone would place a vertex. It was built to catch
  a ROTATION hand-off. Under rods every rear ring's two bones are one rotation and
  differ only by translation, so it now measures **the intended stretch** — 220 mm
  at ring 50 is exactly EMid's and EPreSocket's share difference on a −658 mm
  chord. Reading that as a fault would be the wrong-operand error again.
  `handoff_rot_deg` is the operand the leg was always about: **0.00°**, bounded.
* ⚠ **`--fail-rear-strain` could not fire under rods and was repaired.** It
  switched the bow to the pass-19 solve — and under rods the bow is never called,
  so the control came back CLEAN. (Pass 20 had already hit this once with this
  very control.) It now plants a rotation on the rear rod's middle helper and
  fires the hand-off-rotation operand: **10.99°**, R4 FAIL. Controls
  `--fail-rear-frame`, `--fail-rear-joint`, `--fail-no-dip`, `--fail-dip-stuck`
  all fire.

**`mspan`.** RC=0, with **three legs declared NOT APPLICABLE under rods** and the
declaration printed in the gate's own output:

* `G2 compiled zones` — reconstructs the pass-20 blend ladder (fold blends,
  carrier cores, signed-gradient zones). The rods ladder has none of those.
* `G1b root authority` — audits the v18 staged root helpers, which are inert.
* `G6 posed ring order` — keys rings by a monotone station order; the rods table
  is deliberately non-monotone at each ball.

Those questions are carried by mrod on the posed surface with fired controls,
which is the instrument Direction 22 asks for. Under `ZHAO_U02_RIG=pass20` all
three run in full. **G5 was NOT skipped** — the C–End staged-fraction leg keeps
its shape (re-run the production writer on the receipt, demand an exact match)
and only the law it re-runs changed: **E-stage fraction mismatches 0,
bound/margin breaches 0**, i.e. `kSpanStretchMaxPm`/`kSpanCompactionMinPm` are
untouched and holding.

⚠ **Two mspan controls cannot fire under rods** and mspan says so itself:
`--fail-posed-order` and `--fail-root-authority` print *"the selected mutant did
not fail only its named/causal detector"*, because their legs are the
not-applicable ones. Stated here rather than left for a reader to discover.
Every other mspan control fires (`--fail-rigid-span C-E`, `--fail-dent-pin`,
`--fail-terminal-cap`, `--fail-swell-size`).

**`mjointpub`.** Was **FAIL** — carrier F read `0/0`, a public proof returning
"no visible contribution" for a joint that plainly moves the whole front run.
The cause is real and not a defect: **under rods no vertex is bound to
`kBJunctionF`**. The front rod is skinned `kBNeck → kBSpanDeltaA`, one frame, so
every front-rod vertex is driven 100 % by the F joint and none is "rigid on
`kBJunctionF`". The shared metric now lets a carrier name **both bones of its
joint** (`alt`, 255 = none = the pass-20 test exactly), and F reads **18
vertices, 29.92 / 48.18 mm** — over the 20 mm public floor. All five carriers OK.

**`mprobe`.** RC=0, 0 failures, after §3.2. Clearance contract holds everywhere;
Trick declared contact exactly on its declaration. The clearance line now **names
the offending vertex** (bind-y and bones) — "min clearance 32 mm" says a rule
broke and nothing about which part broke it. Controls `--fail-mirror`,
`--fail-trick-support`, `--fail-trick-support-depth` all fire.

**`mmeshcheck` CLEAN · `moutline`, `mshell`, `mnodule`, `meyesize`, `msmooth`,
`mqa` PASS, 0 failures.**

---

## 5. The rear, against both baselines

All three columns from the **same binary**, same bank, same bounds. The pass-19
and pass-20 columns reproduce pass 20's own committed figures exactly, which is
independent evidence the instrument is measuring the same quantity.

| | pass-19 (legacy bow) | pass-20 (arc bow) | **pass-21 (rods)** |
|---|---|---|---|
| worst rear centreline turn (mrear R1) | 35.60° | 113.03° | **18.44°** |
| worst rod turn RATE (mrod R8) | — | 9.53 °/sample | **0.02 °/sample** |
| rear rail floor | **0.129 (the rip)** | 0.692 | 0.324 |
| rear rail min/max *within* the rod (mrod R9) | — | 317.6 pm | **947 pm** |
| worst rail step | 0.0647 | 0.1630 | **0.0501** |
| rail ceiling | 1.441 | 2.052 | **1.305** |
| HAND-OFF rotation | 0.05° | 0.05° | **0.00°** |
| arm↔End rotation (R1) | 16.48° | 16.48° | 16.48° |
| End joint step (R2) | — | — | 2.73 °/sample (ceiling 6) |

**The attachment is at least as healthy and the rip cannot return.** The rip was
non-uniform compaction plus rotation staging; the new law has neither, the rod's
far helper lands on the End ball centre by construction, and `kSpanStretchMaxPm`
/ `kSpanCompactionMinPm` for C–E are **untouched** and read 0 breaches (mspan
G5). The rail floor is numerically below pass 20's because the length now goes
into the rod instead of into a bow — and it is *uniform*, which is the property
that distinguishes a stretch from a fold.

---

## 6. The knead, re-measured at the SHIPPING values

`mrear` R5, shipping constants, whole bank:

| | pass-20 | **pass-21 rods** |
|---|---|---|
| clips authoring a dip | 19 | **19** |
| never reach B-lowest | 0 | **0** |
| do not return | 0 | **0** |
| worst margin | +37 mm (slot 9, need 20) | **+29 mm (slot 9, need 20)** |

**The knead is preserved: 19 of 19 still reach B-lowest and all return.** The rig
DID change it — the worst margin tightens by 8 mm — because B's ball is a rigid
body of a different profile than the swell it replaces, so "the lowest point of
carrier B" is measured on a different surface. It is still 9 mm clear of the
floor and the gesture reads unchanged at 8× (`P21-LOOKS2/P21-KNEAD-8X.jpg`).
Nothing in the dent path was touched.

---

## 7. Exact-off identity

`ZHAO_U02_RIG=pass20` against the committed pass-20 bank manifest
(`RUN-20260920-0544-manafold-pass20/P20-FINAL-RECEIPTS/bank-manifest.tsv`):
**22/22 subjects, frame counts and `sequence_crc32c` byte-identical.**

Checked on **22, not 3** — pass 20's own scope proof found a two-switch identity
claim that was true on the three clips it was sampled on and false on three
others. It costs no disk: `zhao-reel-cel --crc <subject...>` renders the named
subjects and writes nothing, printing the same CRC `meta.txt` records.

⚠ **The architecture's proposed identity control is wrong and was replaced.** §6
suggested "a 1 mm ball radius change under `pass20` must change a CRC" — the ball
radii are not read at all under `pass20`, so that control cannot fire. The
control is `ZHAO_U02_RIG=rods`, which changes every subject.

---

## 8. Open issues

1. **The rods antenna is more geometric than the pass-20 one.** Straight bands
   and visible knuckles instead of a flowing curve. That is the brief, literally
   — but it is a change of character and the owner should see it in motion before
   it is called finished.
2. **Two mspan controls cannot fire under rods** (§4). The coverage they had is
   carried by mrod, whose own controls do fire, but the pair should either be
   re-pointed at the rods ladder or retired in the next pass rather than left
   printing a confusing line.
3. **The pass-20 code paths stay in the tree** behind the selector — the old
   ladder, the bow, the staging, the z-then-x aims. Deleting them is a later
   cleanup once the owner has accepted the rods rig.
4. **`kBallRyMm` is a rebuild knob, not an env knob**, because it sets the ring
   stations and the station table must be checkable at compile time.
   `ZHAO_U02_BALL_PM` ladders the cross-section only. Stated so nobody discovers
   it mid-ladder.
5. **The crotch at deep joints is declared, not fixed.** mrod prints
   `r/cos(θmax/2)` per ball beside the shipping radius; at A's 150° knead and C's
   148° closure the two rods merge outside the ball. Hiding it needs R ≈ 180 mm,
   which is version 18's rejected bead. Looked at on the deep-knead frames and it
   reads as a folded hinge, not as the pass-19/20 skin wedge — but it is the
   owner's call.
6. **Not done, by instruction:** no 22-subject bank render, no encode, no merge,
   no deploy.

---

## 9. Evidence index

| file | what it shows |
|---|---|
| `P21-LOOKS2/P21RODS-MANAFOLD-*-ALLFRAMES.jpg` | every frame of all eight clips, production ink |
| `P21-LOOKS2/P21-BALL-LADDER-RX-8X.jpg` | the four-rung ball ladder that chose 1.4× |
| `P21-LOOKS2/P21-KNEAD-8X.jpg` | the A corner at the deepest knead, pass20 vs rods |
| `P21-LOOKS2/P21-INSPECT-JOINTS-4X.jpg` | the shortest rear chord (322 mm): the wavy pass-20 rear against one straight rod |
| `P21-LOOKS2/P21-REAR-PLATE-6X.jpg` | the C corner and the End, before/after |
| `P21-LOOKS2/P21-NATIVE-READ-2X.jpg` | the acceptance read at native resolution |
| `P21-LOOKS2/P21-TRICK-PLANT-4X.jpg` | the planted headstand at the shipped plant height |
| `P21-RECEIPTS/manafold-rodgate.txt` + `ctl-*.txt` | R6–R10 and the six fired controls |
| `P21-RECEIPTS/manafold-rear-audit.txt` + `mrear-*.txt` | R1–R5 and the rear baselines |
| `P21-RECEIPTS/manafold-spangate.txt`, `mprobe.txt`, … | the rest of the matrix |
| `P21-RECEIPTS/identity-pass20.log.gz` | the 22-subject exact-off leg |
| `P21-LOOK-NOTES-2.md` | the looks in order, with what each one decided |
