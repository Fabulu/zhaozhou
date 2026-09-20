# P21 REVIEW + QA — independent verification of RODS AND BALLS

**Date:** 2026-09-21 · **Branch:** `manafold-pass21` · **Reviewed at:** `718aec52`
**Reviewer:** Claude, sole agent. No sub-agents, no Qwen, no Codex.
**Method:** every number below was re-measured from **binaries I built myself**
(`tools/reel/build-direct.sh`, g++ 16.1.0 MinGW-W64 UCRT, md5s in
`P21-QA-RECEIPTS/qa-binaries-md5.txt`). No figure is quoted from the
implementer's receipts except where this report explicitly contrasts the two.

# VERDICT: **FIXED**

The rig is right, the owner's direction is met, and the look is a large
improvement. **Three real defects were found and repaired in this review**, one
of them a source corruption and one of them a stale ring table that had silently
disabled **eight** gate controls — including the only control guarding the leg
that the re-based R4 rail floor cites as the thing still holding the line.

The full matrix is green on my build **after** those repairs, the exact-off
identity leg is 22/22 byte-exact, and the pass-20 bank is untouched.

---

## 0. The three defects found and fixed

### 0.1 A raw NUL byte committed into `manafold_clips.h` — `FIXED`

`tools/reel/manafold_clips.h:3121` contained a literal `0x00` byte inside a
character literal, where `'\0'` should have been written:

```c
if (end == nullptr || *end != '<NUL>' || v < 200 || v > 4000) return false;
```

The *value* is correct — a raw NUL in a char literal is 0 — so nothing shipped
wrong. **The damage was to every tool that reads the file**, and it is the
"making waste invisible to your tooling" law in a new costume:

* **git classified the file as binary.** `git diff 4bcf83db..718aec52 --
  tools/reel/manafold_clips.h` printed *"Binary file … matches"*. The largest
  file changed in pass 21 — the one holding every clip builder, the rear-span
  writer and the whole env parser — **could not be reviewed by diff at all.**
* **git stopped normalising its line endings.** With `core.autocrlf=true` every
  sibling in `tools/reel/` checks out CRLF; this one checked out LF, because git
  does not convert files it thinks are binary. That is also why the commit shows
  6,247 insertions / 6,139 deletions for a change that is really 130 lines.
* **`grep` and ripgrep silently skipped it.** Any future audit grepping
  `tools/reel/` for a symbol would miss the creature's biggest source file and
  report a confident, empty answer.
* **g++ warned twelve times** — `warning: null character(s) preserved in
  literal` — and the warning was not read.

Fixed by writing the escape. `git diff` on that file is now a **one-line text
diff**, grep works, and a clean rebuild emits **0 warnings**. The change is
byte-neutral, and the 22/22 identity leg below was measured on the fixed build,
which proves it.

### 0.2 `mspan`'s ring table was stale — **eight dead controls** — `FIXED`

`manafold_spangate.cpp::ring_station_map()` still computed ring stations with a
**second, hand-copied copy of the uniform station law**:

```cpp
const int32_t x = (int64_t(s.total) * i) / (u02::kLoopRings - 1);
```

The implementer removed *exactly this duplicate* from
`manafold_rear_audit.cpp::ring_map()` in this very pass, with the right reason
beside it — *"a SECOND COPY of the uniform expression here is exactly the
stale-ring-table fault that made a pass-19 gate report a confident wrong
number"* — and left the sibling copy in mspan.

Under rods that map keys 64 rings at uniform stations **the rods mesh does not
have**, so `ring_map.find(v.y)` missed nearly every loop vertex. Both live
consumers then failed **open**:

| consumer | what it did under rods | cost |
|---|---|---|
| `mutate_rigid_span()` | rebound **no vertices at all** | `--fail-rigid-span` a no-op on **all four spans** |
| `minimum_free_ring_step_y()` | found no rings, returned **`+inf`** | ring-collapse detector dead → `--fail-overcompact` dead on **all four spans** |

The gate printed **`min ring dy inf mm`** on all eight G4 legs and reported
`PASS: 0 failure(s)`. A detector reporting the *most reassuring possible value
from an empty operand set* — the CLAUDE.md law about a counter asserting zero,
in C++.

Three repairs, all gate-side (they cannot move a rendered byte):

1. `ring_station_map()` now reads the one home, `u02::loop_ring_station_at(i)`.
   Under `pass20` this is bit-identical to the expression removed
   (`loop_ring_station_mm`, and `Stations::total == kLoopTotalMm`), so the
   exact-off leg cannot move — and does not.
2. `minimum_free_ring_step_y()` returns its **ring count** and the caller
   **fails when fewer than two rings are in the window**. A window with no rings
   can never again read as perfectly ordered. The count and the window are now
   printed on every G4 line.
3. The rods window is the span's **rod ring set, taken by ROLE from the station
   table** — not a station range. A range window re-admits the ball's leading
   cone rings (ball A's first three sit at 858/868/894, *inside* rod F-A's
   318..929), which are deliberately non-monotone, and the shipping rig then
   reads a large negative step that is a fold inside a ball. `mrear`'s
   `rail_is_along_band()` makes the same exclusion for the same reason.

**Result: 29 of 31 mspan controls now fire attributed under rods, against 21
before.** Only `--fail-posed-order` and `--fail-root-authority` remain dead, and
those are the two the implementer honestly declared — their legs return early by
design.

### 0.3 The overcompact mutant was too weak to reach its own state — `FIXED`

Its magnitude was `-(kSpanGradientMm[si] + kSpanMinRunMm)` — the **retired**
pass-20 fold-blend gradient length. Under rods the run being compacted is the
rod, and for F-A, A-B and B-C the pass-20 figure is *shorter than the rod*, so
the mutant compacted without inverting and the control came back clean. Only
C-E happened to fire, because its gradient is long. The magnitude is now sized
to the rod it compacts. All four fire attributed.

---

## 1. Claim-by-claim: reported vs independently verified

### Claim 1 — runs straight, bending only at the balls · **VERIFIED, and stronger than reported**

**The reported figures were measured on 8 of 24 slots.** `mrod`'s `--gate` hard-codes
`slots = {0, 2, 11, 5, 21, 7}` — six — where `mrear`'s gate enumerates the whole
bank. The implementer passed eight explicitly to reach 2,858 samples. Either way
"over every frame of every clip" was **8 of 24 clips**.

I re-ran it over **all 24 slots, 9,700 samples**:

| leg | bound | reported (8 slots, 2,858) | **mine (24 slots, 9,700)** |
|---|---|---|---|
| R6 rod turn | ≤ 1.5° | 0.02° | **0.02°** |
| R6 rod shear | ≤ 5° | 0.04° | **0.05°** |
| R6 rod sag | ≤ 4 mm | 0.02 mm | **0.02 mm** |
| R7 joint on ball | ≥ 980 pm | 999.7 pm | **999.7 pm** |
| R8 joint step | ≤ 8°/sample | 6.70° | **6.70°** |
| R8 rod turn rate | ≤ 8°/sample | 0.02° | **0.02°** |
| R9 rod uniform | ≥ 850 pm | 947 pm | **938.2 pm** |
| R10 ball rigid | ≤ 1 % | 0.059 % | **0.107 %** |

**The headline holds on the whole bank.** Two legs are modestly worse on the
slots that were not sampled (R9 947→938 pm, R10 0.059→0.107 %), both still far
inside their bounds. The rig is straight by construction and the measurement
agrees everywhere.

**On the `kLoopRings = 64` deviation.** The layout is base 4 · rod 9 · ball 7 ·
rod 5 · ball 7 · rod 5 · ball 7 · rod 9 · ball 7 · tail 4 = 64, and the
compile-time distinctness assert is real. **No run has too few rings to read
straight**, because a rod's straightness is structural, not sampled: every rod
ring is a convex combination of two points on one posed line, so two rings would
suffice and five is generous. **No ball has too few rings** — all four keep the
designed 7 (5 body + 2 cone); the architecture's ~70 budget was absorbed by
shortening the *rods*, which is the right place. The deviation is sound, and
keeping the count avoided handing a stale ring table to a dozen instruments —
which is precisely the defect I then found in mspan anyway, from a *different*
stale copy.

### Claim 2 — the rear, and the R4 floor · **VERIFIED; the floor judgement is below**

All reproduced exactly on my build: centreline turn **18.44°**, rate
**0.02 °/sample**, rail step **0.0501**, rail floor **0.324 (slot 2, ring 51)**,
min/max rail **947 pm**, hand-off **rotation 0.00°** against a 2.0° bound.

**The pass-20 rip has not returned.** Posed strain is uniform, and the worst rail
sits at **ring 51 — inside rod C-End** (rod 3 spans rings 44–52), which is
exactly where mrod R9's uniformity measure applies. The substitution argument is
structurally sound: the number R4 used to guard is now in a window another
instrument covers.

### Claim 3 — the skipped stage-4 aim swap · **SAFE, and the reported comparison was invalid**

Two findings, and they point opposite ways.

**a) The two baseline rows are one configuration.** The implementation's table
gives *"pass-19 (legacy bow) 4.84"* and *"pass-20 (arc bow) 4.84"* as separate
measurements and treats the agreement as corroboration. **`mrod` cannot select
the bow at all.** `g_u02_rear_bow` is parsed in `zhao_reel.cpp` and
`manafold_rear_audit.cpp`, but **not** in the shared `apply_knead_dip_env()`,
so mrod never reads `ZHAO_U02_REAR_BOW`. I proved it: setting the env to
`legacy`, to `arc`, or to garbage produces **byte-identical output**. The
identical number was the same run twice — the pass-20 "an env control is only a
control in a binary that reads it" lesson, committed again in the same pass that
quotes it.

**b) On the full bank the comparison reverses, in rods' favour.** Like-for-like,
all 24 slots:

| rig | worst blade-roll step (3 slots, as reported) | **worst (24 slots, mine)** |
|---|---|---|
| pass20 | 4.84 °/sample | **7.50 °/sample** |
| **rods** | 5.10 °/sample | **6.98 °/sample** |

The 3-slot sample made rods look *worse*; on the whole bank rods is **better than
the rig that shipped**, against the same 8° bound. The right conclusion is
therefore the one the implementer reached, for a reason he did not have:
**leaving the five aims on the legacy axis is not a latent defect** — rods
strictly improves the metric versus the accepted baseline, so it cannot be a
regression. Two qualifications: the margin is **1.02°, not 2.90°**, so this is a
watch item for any pass that speeds the antenna up; and note that the pass-20
close itself attributed its roll flip to **quantisation**, not to axis
degeneracy, which weakens the premise further.

### Claim 4 — the gate coverage hole · **the hole was FOUR TIMES the reported size; now closed**

Reported: two mspan controls cannot fire. **Measured: ten.** All ten fire under
`pass20` and none under `rods`, which is the signature of a rig-dependent
blindness rather than a retired leg:

| control | reported | measured before | after my fix |
|---|---|---|---|
| `--fail-rigid-span` ×4 (F-A, A-B, B-C, C-E) | *"fires"* | **dead** (`observed=0x0`) | **fires attributed** |
| `--fail-overcompact` ×4 | not mentioned | **dead** | **fires attributed** |
| `--fail-posed-order` | declared dead | dead | dead (genuinely N/A) |
| `--fail-root-authority` | declared dead | dead | dead (genuinely N/A) |

`--fail-rigid-span C-E` is listed in §4 of the implementation report among the
controls that **do** fire. **The committed receipt
`P21-RECEIPTS/mspan-ctl-fail-rigid-span.txt` already said
`UNATTRIBUTED expected=0x2 observed=0x0`.** The evidence was produced correctly
and then mis-summarised — which is how a gate's silence becomes a quoted claim.

**What each was protecting, and whether it still is:**

* **`--fail-overcompact`** guards the compaction-bound leg (**G5**) — the leg
  §5 of the implementation cites as *"the thing holding the line"* after R4's
  floor was re-based. Before my fix that leg had **no control that could fire
  under rods**. It does now, on all four spans.
* **`--fail-rigid-span`** guards "this span's rings still carry the signed
  delta". Its pass-20 detector (G1/G2 compiled zones) is genuinely retired under
  rods, so I made its causal category rig-dependent: under rods the property is
  G4's, which is what it now trips (F-A `0x10`, others `0x90`). Declaring that
  is better than leaving a live control printing UNATTRIBUTED, which is how open
  issue 2 came to read a working instrument as dead weight.
* **`--fail-posed-order` / `--fail-root-authority`** remain unprotected by mspan
  and that is **acceptable**: both legs reconstruct the pass-20 blend ladder,
  which does not exist under rods, and both properties are carried by mrod R6/R7
  on the posed surface with controls that I fired myself. They run in full under
  `ZHAO_U02_RIG=pass20`.

### Claim 5 — knead and identity · **BOTH VERIFIED on my build**

* **`mrear` R5: 19 clips author a dip, 0 never reach lowest, 0 do not return,
  worst margin +29 mm (slot 9, need 20).** Exactly as reported.
* **Identity: 22/22 subjects byte-exact** under `ZHAO_U02_RIG=pass20` — frame
  counts *and* `sequence_crc32c* — against
  `RUN-20260920-0544.../bank-manifest.tsv`. Measured on the NUL-fixed tree,
  which is independent proof that repair was byte-neutral.
* **The identity control fires: 22/22 subjects CHANGE under `rods`.** I ran it
  rather than assuming it.

### Claim 6 — the ball ladder and B's exception · **VERIFIED; no floor or window moved**

`mprobe` on my build: slot 13 declared contact keys 78..148 + 2-key apron,
**deepest vertex −25 mm against a declared −25 and an accepted band of −60..−5**
(the pass-20 declaration, unchanged); **carrier B owns 140/140**, depth-fail 0,
range −25..−16. The **40 mm clearance floor and the contact window are both
untouched in source** (`manafold_probe.cpp` is a 12-line diff and neither
constant is in it).

**B's 1.23× exception is genuinely required by contact, not convenience.** mrod
prints the crotch floor `r/cos(θmax/2)` per ball; over all 24 slots ball B's
worst joint is **100.2°, floor 69 mm against a shipping 85 mm** — B is the one
ball whose rods stay *inside* it. A/C sit at 150.4°/152.1° with floors 180/199
against 101. So B is already the most generously covered ball at its own worst
angle, and shrinking it is the cheapest correction available; the alternative
lever was measured and shown not to work (+16 mm root moves the approach +3 mm).
The Side sheet does not draw three equal balls either.

### Claim 7 — controls fired · **17 fired by me, across four gates**

Full table in `P21-QA-RECEIPTS/controls-fired.txt`. Highlights:

* **`mrear --fail-rear-strain` (the repaired one): FIRES — `FAIL R4 STRAIN`.**
* **`mrod --fail-joint-step` (R8's invented one): fires EXACTLY ONE leg.** Clean
  isolation; the best-behaved control in the pass.
* `mrod --fail-ball-blend`: exactly one leg (R10).
* `mrod --fail-rod-twist` (rods-native): 5 legs, including **R9** — so R9 *is*
  demonstrated by a fault inside its own rig, not only by the rig swap.
* **Caveat:** `--fail-rod-bend`, `--fail-rod-uniform` and `--fail-rod-flicker`
  are **one configuration under three names** (all set `rig = pass20`) and all
  fire all eight legs identically. The implementation table presents them as
  three targeted controls with three distinct numbers. They are one blanket
  mutant. Not a false claim — each leg does move — but mrod has **four distinct
  control configurations, not six**, and it has no "failed only its causal
  detector" attribution check of the kind mspan has. Recorded as an open issue.

---

## 2. The R4-floor judgement

**Verdict: the RETIREMENT is justified; the NUMBER is not the one the argument
produces, and it is the weaker of two guards rather than the binding one.**

In favour of the re-basing, and I checked each:

* The fold detector genuinely did change. The rip was non-uniform compaction plus
  rotation staging; under rods there is neither, and uniformity is the right
  question. R9 reads **938 pm** over the full bank and *is* fireable inside the
  rods rig (`--fail-rod-twist`).
* The worst rail (0.324) sits at **ring 51, inside rod C-End** — within R9's
  window, so the substitution is not a hand-wave.
* **No bound was moved to admit the result.** `kSpanStretchMaxPm` /
  `kSpanCompactionMinPm` are untouched; G5 reads 0 breaches; the retired floors
  are printed beside the live one.
* `handoff_rot_deg` is the correct operand, reads 0.00°, and its control fires.

Against, and this is the part to act on:

* **0.12 is not what the stated derivation yields.** The argument is *"the band
  may not be compacted past what the C-E span bound already allows"*.
  `kSpanCompactionMinPm[3] = -700` per mille implies a rail of ≈ **0.300**. The
  shipped floor is **2.5× looser than its own reasoning**, and the shipping worst
  (0.324) is only 8 % above the *derived* bound while being 170 % above the
  *written* one. The floor as written cannot distinguish a healthy band from one
  that has broken the span law by a factor of two.
* **The guard cited as the real line had no fireable control** until this
  review (§0.2). *"The span bound is untouched and holding"* was, under rods, a
  detector reading zero.

I did **not** move the floor. Tightening it to ≈0.28–0.29 would be the
principled value, but it leaves ~13 % margin on a number I have measured on one
tree, and a bound change at review time with thin margin is the kind of call the
owner should make deliberately. **The correct repair was to make the guard
behind it able to fire, and that is done.** Recommendation for pass 22: set
`kGateRailRodsFloor` to the value `kSpanCompactionMinPm[3]` implies, or delete
the leg and cite G5 explicitly — do not keep a floor whose only property is that
nothing can reach it.

---

## 3. Visual QA — production ink, `ZIXX_EXP=celmain` + `ZIXX_LIGHT=diagonal-cool-cross`

Nine clips rendered on **both rigs** from my own binaries (1.0 GB each, purged
after). Frames chosen **by badness** — the worst joint angle per ball per slot,
computed from mrod's own CSV by a committed probe
(`P21-QA-RECEIPTS/qa_badness_frames.py`), plus the gate's named worst frames —
never by index. Sheets in `P21-QA-LOOKS/`.

**(a) Do the runs read as straight rods bending only at the balls? — YES.**
`QA04` (12 frames across the Inspect orbit): every frame shows straight segments
meeting at visible rounded knuckles. Not one frame bends mid-run. `QA01`/`QA02`
at 2×/5× confirm it at the three worst joint angles.

**(b) Is the End calm? — YES on the clips where the spazz lived; unchanged in
character elsewhere.** `QA03` plots End-ball speed and |jerk| per sample, rods
against pass 20, from the posed centroids:

| clip | End speed peak | End \|jerk\| peak |
|---|---|---|
| inspect/hover | **2.56** vs 4.61 mm (−44 %) | **0.89** vs 1.50 (−41 %) |
| channel | **4.18** vs 4.58 mm | **1.02** vs 1.58 (−35 %) |
| taunt III | 8.17 vs 7.80 mm (+5 %) | 4.63 vs 4.03 (+15 %) |

The *shape* matters more than the peak. On inspect, pass 20 is a jagged trace of
erratic tall spikes — that **is** the spazz, drawn — while rods is a clean regular
oscillation. On taunt III the two traces **overlay almost exactly**: the increase
is one authored beat now transmitted by a rigid rod instead of being absorbed by
a bow. That is authored motion, not spazz.

**(c) Does the antenna read SMOOTH, or like a bent wire / faceted chain? — It
reads as a JOINTED LIMB, and the character change is a large improvement. The
honest answer is that it is more geometric, and that this is what makes it
legible.**

This is the owner's acceptance question, so the plain answer first: **put
`QA04` (rods) beside `QA05` (pass 20) and pass 20 has no structure at all.** In
f280, f332, f384, f540 the pass-20 antenna is an amorphous soft lump — no joints
read, no runs read, and where bends *are* visible they are mid-run. That is the
"too many joints / not smooth" complaint, and it is plainly visible. The rods
antenna reads in every frame: four straight runs, four rounded knuckles, one
continuous form.

It does **not** read as a faceted chain — the rods are smooth-sided and the
knuckles are round, at native and at 5×. It does **not** read as a bent wire —
that was the 1.0× ball ladder rung the implementer rejected, and the 1.4×
decision was right; the knuckles are clearly visible at native resolution.

What it *is* is more **constructed**. At f280 the loop is close to rectangular
and reads a little like a wire frame with beads. That is a genuine change of
character and the owner should see it in motion — but it is Direction 22
followed literally, and the thing it replaces is worse on the owner's own terms.
My judgement: **accept.**

**(d) Do two rods merge outside a ball as a visible crotch? — NO.** The declared
risk is real on paper: over all 24 slots ball A reaches **150.4°** (crotch floor
180 mm vs R 101) and ball C **152.1°** (floor 199 vs R 101) — both worse than the
147.7°/173 mm declared. But at 5× on those exact frames (`QA02`) the junction
reads as a **folded hinge**, not a wedge or a spike. The merge is hidden by the
band's own width and the outline ink. Declared, looked at, and not a fault.

**(e) Does the pass-20 knead still read? — YES.** `QA08`, at the deepest B press
computed per rig: both rigs reach the dip **at the identical key** (inspect 288,
rest 89, channel 123), and rods presses ~40–50 mm further on the same gesture.
The press reads clearly; the band flattens against the body and returns.

**(f) New faults? — NONE FOUND.** `QA06` covers eight clips at their worst
frames: outline enclosed everywhere, no stray geometry, eyes correctly placed and
sized on every clip, mana lightning and motes present and correct (including the
night-lit channel scene), distance-scaled lines correct on the far clips (drift,
blown), the corpse reads on death-drop. `QA07` shows Trick's plant across the
declared window: the crown rests **on** the dirt with dust, no float and no sink,
matching the probe's −25 mm.

---

## 4. Final matrix — my build, my run, one invocation

`P21-QA-RECEIPTS/FINAL-MATRIX.txt`:

```
mrod-all24   RC=0  ALL LEGS OK (24 slots, 9,700 samples)
mrear        RC=0  rear gate mask 0x0 -> GREEN
mspan        RC=0  PASS: 0 failure(s)
mprobe       RC=0  mjointpub RC=0  mmeshcheck CLEAN (34/2800/1416/4200)
moutline RC=0  mshell RC=0 (--selftest: every check failable)
mnodule RC=0  meyesize RC=0  msmooth RC=0  mqa RC=0
mspan-pass20 RC=0  mrear-pass20 RC=0
identity  22/22 EXACT · identity control 22/22 CHANGED
```

Bounds: **untouched**, with one exception argued in §0.3 — the overcompact
*mutant's* magnitude, which is a control's strength, not a gate's threshold.

## 5. Open issues carried forward

1. **`kGateRailRodsFloor = 0.12` is unreachable.** §2. Set it to the value the
   span bound implies, or retire the leg explicitly.
2. **`mrod --gate` samples 6 of 24 slots by default** and has no whole-bank
   enumeration like mrear's. Every headline it produces is a claim about a
   third of the bank unless slots are passed. It should enumerate the bank.
3. **mrod has 6 control names for 4 configurations**, and no "failed only its
   causal detector" attribution check. `--fail-rod-bend`, `--fail-rod-uniform`
   and `--fail-rod-flicker` are the same rig swap.
4. **`ZHAO_U02_REAR_BOW` is parsed per-main, not in `apply_knead_dip_env()`.**
   mrod silently ignores it. Move it beside `ZHAO_U02_RIG`.
5. **Two mspan controls remain genuinely N/A under rods** (§1 claim 4) — retire
   or re-point them next pass.
6. **`manafold_clips.h` is now LF while its siblings are CRLF** in the working
   tree. Harmless (git normalises it correctly again now that it is text), noted
   so nobody re-diffs it in surprise.
7. **The character change is the owner's call.** §3(c).

## 6. Not done, by instruction

No 22-subject bank render, no encode, no merge, no deploy. Render
intermediates (2.0 GB of `.rgb`) were purged after the looks were made.
