# Manafold pass 25 — INDEPENDENT REVIEW — **VERDICT: PASS**

**Date:** 2026-09-23
**Reviewer:** Claude (independent; no sub-agents, no Qwen, no HomeAI)
**Source reviewed:** Zhaozhou `manafold-pass25` @ `b3c5760e`, clean tree
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-26-2026-09-22.md` (binding)
**Reports reviewed:** `P25-IMPLEMENTATION.md`, `P25-BACKBALL.md` (which supersedes §3)

---

## 0. HOW THIS REVIEW WAS DONE

Nothing below is quoted from the implementer's receipts. I built the tree myself
and re-ran every measurement on my own binaries.

* **My own build**, from the clean tree at HEAD, via `tools/reel/build-direct.sh`
  with the `zhao-env.ps1` toolchain (g++ 16.1.0 MinGW-W64 ucrt) into
  `.tmp/p25rev`. Targets cel, mbolt, mback, mrear, msmooth, mrod, mnodule,
  mspan. `ALL_RC=0`, read directly and never through a pipe.
* **The binaries' MD5s differ from the implementer's** (different output path),
  so every check here is BEHAVIOURAL, not a hash comparison. That is the
  stronger test: it asks whether the tree produces the receipts, not whether two
  files are the same file.
* Production ink on every render and every gate: `ZIXX_EXP=celmain`,
  `ZIXX_LIGHT=diagonal-cool-cross`.
* Findings written to disk after each look, before the next change.

### The headline

**My independently built renderer reproduces the shipping 22-subject bank
byte-for-byte.** `diff` of my sequence CRCs against the committed
`P25-BB-RECEIPTS/crcs-backball.txt` is **empty on all 22 rows**. Every claim
below rests on that.

---

## 1. ITEM 1 — THE BOLT ROLLOUT

### 1a. Verified, my binary, my run

| claim | reported | **I measured** |
|---|---|---|
| live subjects carrying avoidance | 22 of 22 | **22 of 22** (B3, B4 bound to `u02::kBoltLiveSubjects` in order) |
| bolt segments intersecting the antenna, whole bank | 0 | **0**, every row, every key and midpoint |
| nearest pass in the bank at 96 mm | 32.3 mm | **32.3 mm, `manafold-channel` f247** |
| nearest pass at the pass-24 clearance 46 mm | 4.4 mm | **4.4 mm, `manafold-blown` f130** |

`mbolt --gate` rc=0 on my binary. The two worst untouched offenders the census
named — death-drop and drift — are zero.

### 1b. NON-MONOTONICITY — CONFIRMED, all twelve rungs, independently

I ran the whole sweep myself rather than reading theirs.

| mm | my rc | my hits | mm | my rc | my hits |
|---:|---|---:|---:|---|---:|
| 46 | 0 | 0 | 96 | 0 | 0 |
| **56** | **1** | **3** (blown f122) | **106** | **1** | **1** (channel f247) |
| **66** | **1** | **8** (blown f120) | 116 | 0 | 0 |
| **70** | **1** | **11** (blown f142, worst −56.4 mm) | 130 | 0 | 0 |
| **76** | **1** | **4** (blown f128) | 150 | 0 | 0 |
| 86 | 0 | 0 | 170 | 0 | 0 |

**Every rc, every hit count, every worst subject and frame matches the report.**
The claim is true: 56/66/70/76/106 introduce intersections while 46/86/96/116/130
are clean, and the value therefore had to be taken from the measured-clean list.

**The standing gate leg fires.** `ZHAO_U02_BOLT_CLEARANCE_MM=70` + `--gate`
returns **rc 1** on my binary, as does `ZHAO_U02_BOLT_AVOID=off`. Both are
standing matrix legs (`d-mbolt-clr70-red`, `d-mbolt-envoff-red`), so a future
hand moving the knob to a known-bad rung goes red rather than quiet.

**And the B3 repair is real, fired by legal stimulus.** At 56/66/70/76 mm my run
prints `FAIL B3 ROLLOUT: … N intersections remain across the whole bank`
**alongside** the red B1. Pass 24's B3 summed the residual over subjects carrying
no avoidance — an empty set after the rollout — and would have printed a
reassuring zero here while B1 was red. The repair is the difference between a
counter that can see the case it is quoted about and one that cannot.

### 1c. THE LIGHTNING IS NOT RESTYLED — structural, then looked at

`bolt_avoid_rods(int32_t pts[][3], int n, int lo, int hi)` (`manafold_fx.h:2373`)
takes **only the point array**. Radius, colour, gain, stamp density, morph clock,
station identity and topology are not in its scope and it cannot reach them.
I re-ran the gate that would catch a switched figure: **`msmooth` rc=0** on my
build (persistent lightning/particle identities and 60 Hz continuity), as are
`mrod`, `mrear` (both modes), `mspan` and `mnodule`.

**Looked at** (my render, pass-24 lightning against pass-25, at 5×): on
`channel` f247 and `blown` f130 the pass-24 bolt lies **on** the rods and merges
with them into one white mass; at 96 mm a clear band of background shows between
bolt and rod on both. Individual bead size, colour and spacing are unchanged —
only the path moved.

### 1d. The declared side effect, judged

At Hover's tightest closure (f469/471/473, clearance 46 against 96, my render)
the push does carry a small part of the figure outside the rods at 96 mm, at the
top-right, hugging the upper ball. **It still reads as energy belonging to the
antenna, not as a detached chain in empty air.** Acceptable, and it agrees with
the implementer's reason for stopping the ladder below 116.

---

## 2. ITEM 2 — THE EYES AT 800

**Looked at, my renders, 600 against 800, everything else held at shipping.**

* **Rest** (six frames, both lenses at 8×): at 800 the stars plainly travel
  further and change size visibly. It reads **stronger, and it is not loud.**
* **Hover** (four frames): same read. The left star's travel across the lens is
  clearly larger at 800 than at 600.
* **The star does not ride the lens rim.** At 800 the stars come *near* the inner
  lens edge on Rest f008/f264 and Hover f024, with purple still visible between
  the star's white outline and the black rim on every frame I looked at. Nothing
  crosses or sits on the rim.
* **The contrast holds, checked at 800 rather than assumed.** Curious and Startle
  laid beside Rest-at-800: the authored beats' stars are several times the area,
  fill the lens, swing the whole lens across the face and change size beat to
  beat. Rest at 800 is a sliver drifting inside a narrow lens. **Curious,
  Startle and Taunt III remain plainly the loud ones.**
* **Byte-identity confirmed.** `kEyeAmbientClipPm` is 0 at slots 3 (curious),
  4 (startle) and 21 (taunt III), and the matrix's `e-item2` changes exactly the
  other 19 by name.

> **An incidental positive control, from my own mistake.** My first
> reproduction of pass 24 set *every* slot to 600, including the three that ship
> at 0 — and exactly curious, startle and taunt III came back different. That is
> a free demonstration that those three zeros are a live choice and not a dead
> path. Re-run with the correct exact-off (which omits slots 3/4/7/15/16/21) the
> bank is identical, 22/22.

---

## 3. ITEM 3 — THE BACK BALL: THE DIAGNOSIS VERIFIED, NOT JUST THE FIX

### 3a. The decomposition reproduces, number for number

I re-ran the committed probe (`mback --decompose --slot 0`, damping off) on my
own binary. **Every figure in `P25-BACKBALL.md` §1b came back identical:**

| authority muted | END pos | END ang | C pos | C ang | rod pos |
|---|---:|---:|---:|---:|---:|
| socket follows breathing body | **65.0 %** | 0.0 % | 0.0 % | 0.0 % | 0.0 % |
| rear rod aim (arm arrival `qd`) | 10.9 % | **90.3 %** | 0.0 % | 0.0 % | 0.0 % |
| antenna upstream life (F/N/A/B/C/D) | 0.0 % | 0.0 % | **81.1 %** | **100.0 %** | 40.4 % |
| … station B alone | 0.0 % | 0.0 % | −6.6 % | 2.2 % | **−146.4 %** |

The End swell's own position is **0.988 mm per sample** — under a tenth of a
native pixel. **The diagnosis is verified, not merely reported:** the thing the
codebase calls the back ball barely moves, 65 % of the little it does is the
socket riding the breathing body, its orientation is 90 % the rear rod aim, and
what an eye reads is carrier C and the C→End rod. No single station owns more
than 9 % of C, the chain's stations partially cancel, and freezing B alone makes
the last rod move **146 % more**. That is why four passes of gain levers failed.

### 3b. The owner's premise, checked

`mback --census`, my run. Among the **live** clips, slot 23's **1.08** (crackle —
the same choreography as Hover, undamped) is the **only** back/front angular ratio
at or above 1.00; Hover shipped reads **0.99**. Every other live clip sits in
0.39–0.99. (Slots 15 and 16 read 1.51 and 1.20 but are diagnostics, not live.)
Confirmed: Hover was singular in its **ratio**, not in its violence.

### 3c. THE VISUAL VERDICT — is the back ball finally calm?

**Yes, and the antenna keeps its life.**

* **Hover, my render, seven consecutive frames, tight crop on the rear column,
  pass-24 rig above and shipping below.** Undamped, the column's silhouette
  *boils*: its width and its lean change from one frame to the next and the mass
  at its top jumps. Damped, it holds **one shape** and drifts. That is exactly
  the difference between "finicky" and "moving".
* **The same judgement on the FIXED camera** (crackle's bake at 5×, damped
  against undamped, where every moving pixel is the creature and not the orbit):
  clearer still. Undamped, the column's left edge jitters every frame. Damped,
  the edge is stable.
* **And it is not dead.** The rear still travels, the loop still visibly
  breathes, and the gate's two **art floors** say so from the other side:
  C travel **13.005 ≥ 11.500** mm/sample and C angular **1.850 ≥ 1.200**
  deg/sample, on my run. Direction 20's "a bit wiggly" survives.

The reported ladder numbers reproduce exactly on my binary: C travel
18.514 → 13.005, C jerk rms 6.586 → 4.929, last rod 9.365 → 6.673, End angular
2.339 → 1.380.

---

## 4. THE TWO THINGS THE OWNER MUST NOT BE SURPRISED BY

### 4a. The front ball's spin — **NOT BLOCKED. The owner's ask stands.**

**The short answer: pass 24's front-ball lift lives on a bone the damping does
not touch, and cannot touch.**

`kFrontFlexClipPm[0] = 1350` — pass 24's answer to *"The one at the front could
move a little more"* — is applied by `front_flex_play()` into
`HingePlay::tilt_front` / `yaw_front`, which drive **`loc_junction`, the
`kBJunctionF` bone** (`manafold_clips.h:1099–1111`). The damping set is
`kBackBallDampBones[3] = {kBHingeA, kBHingeB, kBHingeC}`. **`kBJunctionF` is not
in it, and it is UPSTREAM of all three** — a rotation at A moves what is
downstream of A, never the junction above it. So the owner's front performance is
untouched by construction, not by measurement.

Measured across the three passes, on my binary, per presentation sample:

| | front swell travel | front swell own spin |
|---|---:|---:|
| pass 23 | 8.698 mm | 2.159 deg |
| **pass 24** (the owner's "a little more") | **8.904 mm (+2.4 %)** | **2.166 deg (+0.3 %)** |
| **pass 25 shipping** | **8.922 mm (+0.2 % on pass 24)** | **1.396 deg (−35.5 %)** |

**This is the line that settles it: pass 24 delivered the owner's ask almost
entirely as TRAVEL (+2.4 %). In SPIN it delivered +0.3 % — essentially nothing.**
A 35 % reduction in spin therefore cannot have undone an ask that was never
delivered through spin, and the travel he did get is preserved to within 0.2 %.

**And I looked.** Pass-24 rig against pass-25 shipping, five frames of the whole
antenna and then an extreme zoom on the front joint: **indistinguishable.** Same
position, same silhouette, same shading, to a pixel or two of dither. The swell
carries no legible surface detail at this framing — it reads as a smooth shaded
bulge — so a slower rotation of its own texture is not something the shot can
show.

**Judgement: the owner's ask is intact. No fix required, and I did not make one.**
It remains worth telling him plainly, which is why it is in the summary below.

### 4b. Crackle carries the same fault — **RECOMMENDATION: OFFER IT TO THE OWNER**

I did not change Crackle, as instructed. I looked at it and measured it.

* **The fault is there.** Crackle plays the identical slot-0 choreography on a
  separate bake (slot 23) and measures the identical **1.08** back/front angular
  ratio — the exact statistic that made Hover singular. Carrier C travels
  18.514 mm/sample, as Hover's did before the fix.
* **It is visible.** My A/B (shipping-undamped against if-damped, same frames,
  fixed camera) shows it clearly at 5× and still legibly at 2×, which is about
  where the owner watches.
* **The conditions make it worse there, not better.** Crackle is a 600-frame idle
  under a **fixed, close** camera. Hover at least has an orbit to carry the eye.
  A fidgety rear on a still creature that the viewer is looking straight at is
  the most noticeable version of this fault in the bank.
* **It costs one table entry** — `kBackBallDampClipPm[23] = 700` — and no new
  mechanism.

**Direction 26 is explicit: *"Other clips stay byte-identical unless the owner's
fault is visible there too — in which case say so rather than changing them
silently."* I looked; it is visible there too; so the direction's own instruction
is to say so. This review says so, and the site's findings page says so.**

The implementer's counter-reason — keeping a byte-identical control for a brand
new mechanism inside the shipping bank — is real but weaker than it looks:
`ZHAO_U02_BACKBALL_DAMP_PM=0` already reproduces the pass-25 bank **bit for bit
on all 22 subjects**, which is a stronger containment proof than one undamped
subject, and unlike a "control subject" it cannot rot the next time anyone
changes slot 23 for an unrelated reason.

**So: leave it for pass 25 (as instructed), and put it in front of the owner as a
one-line change he can say yes to.**

---

## 5. INSTRUMENTS — EVERY CONTROL FIRED BY ME

### mback, my binary — and each fails for its OWN reason

| leg | shipping | bound |
|---|---:|---|
| G1 carrier C jerk rms | **4.929** | ≤ 5.600 |
| G2 End swell angular | **1.380** | ≤ 1.700 deg/sample |
| G3a carrier C travel — **ART FLOOR** | **13.005** | ≥ 11.500 mm/sample |
| G3b carrier C angular — **ART FLOOR** | **1.850** | ≥ 1.200 deg/sample |

| control | rc | what breached | **its own reason?** |
|---|---:|---|---|
| `--fail-undamped` | **1** | G1 6.586, G2 2.339 — the ceilings. G3 stays OK. | yes: the damping removed |
| `--fail-window` | **1** | the same two ceilings, same numbers | yes: window 1 is an identity filter, so it MUST reproduce undamped — and it does, exactly |
| `--fail-dead` | **1** | G3 10.587 mm and 0.846 deg — **the floors**. Ceilings stay OK. | yes: the opposite pair, over-damped |

The third is the one that matters. It is the **art floor** firing, and it is
reachable with **legal stimulus** — which is why `ZHAO_U02_BACKBALL_DAMP_WIN`
runs to 201 instead of a tidy 31. No committed mutant was needed and none is
owed.

### mbolt, my binary

| control | result |
|---|---|
| `--fail-no-avoid` | control FIRED, 23 legs red, **54,885** intersections |
| `--fail-no-split` | control FIRED, B2's 4× ratio collapses |
| `--fail-fat-rod` | control FIRED: B1's operand reads the RODS, not a constant |
| `--fail-mirror-drift` | control FIRED: B4 red on one renamed row |
| `ZHAO_U02_BOLT_AVOID=off` + `--gate` | **rc 1** |
| `ZHAO_U02_BOLT_CLEARANCE_MM=70` + `--gate` | **rc 1** |

### No gate default samples a subset of the bank

* mbolt walks `for (int f = 0; f < r.frames; ++f)` — **every frame, no stride** —
  and prints the full presentation length per subject (hover 600). B4 binds its
  22 rows name-by-name and in order to `u02::kBoltLiveSubjects`, which is the
  same list `subject_u02_clip()` reads. The gate can no longer configure its own
  operand.
* mback judges bake slot 0 and **prints every other slot as `info`**, several of
  them well above Hover's ceilings and correct. That is a declared scope with its
  own evidence attached, not a silent subset.

### No bound relaxed

`git diff 09108284..HEAD -- tools/reel/` removes exactly **one** `constexpr` in
the whole tree: `kBoltRodClearanceMm = 46`, raised to 96 — the pass's subject.
**No `static_assert` removed. No gate threshold lowered.** Every new constant is
new, and every one is a named, editable knob with an env ladder.

### Byte identity — all re-measured on my build

| claim | **my result** |
|---|---|
| shipping bank == committed `crcs-backball.txt` | **22/22 identical** |
| `ZHAO_U02_BACKBALL_DAMP_PM=0` == pass-25 `crcs-ship.txt` | **22/22 identical** |
| every mechanism off == pass-24 `crcs-ship.txt` | **22/22 identical** |
| damping changes exactly hover + inspect | **exactly those two**, at sequence CRC |
| **frame-level bytes**, crackle ship vs damp-off | **27 compared, 0 differing** — the containment proof |
| frame-level bytes, hover ship vs damp-off | 50 compared, **50 differing** |
| frame-level bytes, inspect ship vs damp-off | 27 compared, **27 differing** |

**20 of 22 live subjects are untouched. Hover and Inspect are one bake under two
cameras (`cam_k` 360000 and 460000) and cannot be separated by any per-clip knob;
Direction 26 authorises this in terms and both reports declare it.**

The committed matrix is **265 PASS / 0 FAIL / 265 rows, `MATRIX_RC=0`**, one
invocation from a frozen copy. I audited it row by row.

---

## 6. THREE RECORD CORRECTIONS (no defect in the work)

All three have the same cause: **numbers taken before the back-ball packet moved
hover's and inspect's rig, then not re-read.** None changes a conclusion, and
none affects what ships. Recorded here so the next reader is not misled.

1. **`P25-RECEIPTS/clearance-sweep.txt` predates the back-ball packet.** Ten of
   its twelve rows still reproduce exactly. The two that do not are the two whose
   minimum fell **on hover**:

   | mm | receipt | **now** |
   |---:|---|---|
   | 116 | 56.7 mm, hover f99 | **81.5 mm, trick f261** |
   | 130 | 36.1 mm, hover f99 | **59.8 mm, crackle f492** |

   Both moved in the **safe** direction — hover's rods ended up further from its
   bolts after damping. **The clean/dirty verdict is unchanged on all twelve
   rungs, and the SHIPPED rung (96 mm) is unaffected** because its minimum is on
   `channel`, not on hover.

2. **The "54,595 intersections before" figure** (`P25-IMPLEMENTATION.md` §1b and
   §4) is now **54,885** against the current rig, for the same reason. The
   *conclusion* — 54,595-or-54,885 down to **zero** — is unaffected, and the
   matrix row already prints the correct current number.

3. **`P25-BB-LOOKS/README.md` says "Frames are CONSECUTIVE … on every A/B
   plate."** Plate `01-hover-backball-BEFORE-AFTER-4x-consecutive.jpg` is
   labelled 0018/0020/0022/0024/0026/0028 — **every other frame**, not
   consecutive. The window is still tight and the plate is still evidence; the
   sentence is just wider than the plate. My own consecutive-frame plates
   (seven frames, no gaps) reach the same verdict, so nothing rests on it.

---

## 7. WHAT I DID NOT FIND

* No relaxed bound, no deleted assertion, no widened tolerance.
* No gate configuring its own operand — the pass-24 defect is genuinely repaired
  and B4 is the standing proof.
* No detector reading zero that I could not make fire. Every control in both new
  gates fired for me, on my binaries, for its own distinct reason.
* No byte moved on a subject that was not declared.
* No restyled lightning — argued structurally and confirmed by eye and by
  `msmooth`.

## 8. VERDICT

**PASS.** The three items are delivered, the diagnosis behind the third is
independently reproduced rather than taken on trust, every instrument fires, and
the art reads right at native. Proceeding to publish under the standing bestiary
authorisation, this being the creature's final pass.

Two things go to the owner in plain language, neither of them a defect:
**the front ball still does what he asked** (its performance lives on a bone the
damping cannot reach), and **Crackle carries the same rear fault and is one table
entry from the same repair**, if he wants it.
