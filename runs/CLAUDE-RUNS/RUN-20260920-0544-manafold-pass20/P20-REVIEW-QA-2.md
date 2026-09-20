# Pass 20 — independent RE-REVIEW + QA report

**Reviewer:** independent re-review/QA agent, 2026-09-20 (second pass; the first
review at `c02801a3` BLOCKED this work)
**Tree reviewed:** zhaozhou `manafold-pass20` @ `2ecc35df` + this re-review's two
repairs
**Binaries:** built by me from source, clean tree, with the committed recipe
(`tools/reel/build-direct.sh --output .tmp/p20-rev2 <target>`, one target per
invocation), g++ 16.1.0 MinGW-W64 from the `zhao-env.ps1` toolchain. No CMake
result is claimed. MD5s in §9.
**Render:** `zhao-reel-cel.exe` with `ZIXX_EXP=celmain
ZIXX_LIGHT=diagonal-cool-cross` — production ink — 11 subjects plus two A/B
banks, 5,800 frames.

---

## VERDICT: **FIXED**

Every figure the first review falsified has been re-measured by me, from my own
binaries, with no override, and every one of them now holds. The three blocking
findings are genuinely repaired, not re-described:

* **Claim 1 (the rip) — still CONFIRMED,** and confirmed again by eye at the
  worst sample in the bank.
* **Claim 2 (one walk, G11) — still CONFIRMED.**
* **Claim 3 (the dip ships; B lowest; particles react) — NOW TRUE AS SHIPPED,
  on both halves.** 19 of 19 hosting clips, worst margin +37 mm, G9 7.642
  against an **unmoved** 8.0 — my numbers, my run, banner reading "no override".
  The particle reaction is visible at native to my eye.
* **Claim 4 (the gates) — the three gaps are closed** and their controls fire.
* **Item 4 (the bound that moved) — LEGITIMATE.** §5. No correction required.

**Two real faults found and fixed by me** (§7), both in instruments, neither
touching a bound, an art value or a shipping byte. One of them is this pass's
*fourth* wrong-operand case and it sat inside the very instrument built to stop
them.

---

## 1. What I did not take on trust

The brief's standing warning — *assume there are more* — was correct again.
Every number below was produced by a binary I compiled from the tree, run by me.
I did not read a single figure out of `P20-IMPLEMENTATION.md` and record it.

| claim | reported | **I measured** | verdict |
|---|---|---|---|
| B strictly lowest, hosting clips | 19 of 19 | **19 of 19** (`R5 DIP: 19 clip(s) author a dip; 0 never reach lowest; 0 do not return`) | ✅ |
| worst B-lowest margin | +37 mm, slot 9 | **+37 mm, slot 9** (need 20) | ✅ |
| G9 worst angular step | 7.642° | **7.642° slot 8 f0101 B** | ✅ |
| G9 ceiling | 8.0, unmoved | **8.0**, byte-identical at `4b3d4576` | ✅ |
| G10 dent pin | 8 mm / 0.066° | **8 mm (ceiling 20) / 0.066° (ceiling 3.999)** | ✅ |
| worst hand-off | 361 mm | **361 mm, slot 2** | ✅ |
| shipping config judged | no override | banner: *every shipping constant at its shipped value (dip gain 550, dent depth 4200, ramp floor 30, fold reaction 650)* | ✅ |
| R1 centreline turn | 113.03° | **113.03°, slot 2** (ceiling 140) | ✅ |
| R1 arm↔End | 16.48° | **16.48°, slot 16** (ceiling 40) | ✅ |
| particle reaction, Hover | 127 frames differ, 2,745 px | **127 / 2,745** | ✅ |
| particle reaction, Drift | 40 frames, 283 px | **40 / 283** | ✅ |

**The headline is not an overridden figure this time.** I confirmed that
independently and three ways: the banner prints clean on the same run; I
re-derived the ladder myself (§2); and `--dip` no longer writes the gain (the
only two writes to `g_u02_knead_dip_gain_pm` in `mrear` are `--fail-no-dip`'s
declared mutant and a comment saying the flag no longer moves it).

## 2. The roll-flip repair — verified on the review's own worst case

This is the single most convincing number in the pass and it holds. I re-ran the
first review's ladder with my binaries:

| configuration | first review measured | **I measure, repaired aim** |
|---|---|---|
| gain 1000 / depth 2200 — *the review's worst case* | **50.963° FAIL** | **7.591°** |
| gain 550 / depth 2200 (old ship) | 7.772 | **7.591** |
| gain 550 / depth 4200 (**ships**) | — | **7.642** |
| gain 550 / depth 4800 | — | **8.198 FAIL** |
| gain 550 / depth 6000 | — | **9.848** |
| `KNEAD_DIP_PM=0` (the beat off) | — | **7.591** |

Two things this says that the table alone does not:

1. At the review's worst case the number is now **exactly the dip-off bank's own
   worst**. The flip is not smaller. It is *gone* — the dent is invisible to G9
   at that configuration.
2. **The ladder is smooth and monotone out to 6000 with no cliff.** That is the
   signature of a well-conditioned aim, and it is independent corroboration of
   the quantisation diagnosis: a *discontinuity* would show as a jump, not a
   ramp. The worst also migrates from carrier B to carrier A at 4800 — an
   honest rate, not a flip.

**The maths.** `arc_from_y_about` takes the turn as an angle about a carried
axis. `e1 = +Y`, `e2 = u × a / |u|`, `sin` and `cos` handed to `angle16_of` in
its own `(−sin, cos)` convention; the axis is unitised to 2^16 and fed to the
production `quat_axis`, and the residual out-of-plane part is finished by the old
shortest arc in a region where it is well conditioned. All integer, fixed
iteration counts, no floating point — **deterministic**. Overflow headroom is
comfortable (`un2 ≤ ~1e11`, `sin_num ≤ ~3e8`, both int64).

**Conditioning near the degenerate region.** The conditioner's own norm is
`|u_xz|`, taken from the pre-dent (A,B,C) triangle normal scaled to 2^20 and
with its +Y component dropped. It has **no term in `dent_pm`**, which I
confirmed in the source — so it cannot collapse as the beat deepens, which was
the whole fault. The one residual is structural and declared: if the (A,B,C)
plane ever became *horizontal*, `u_xz → 0` and the function falls back to
`shortest_arc_from_y` with its old blind spot. That state is not reached
anywhere in the bank — G9 sweeps 48,500 samples at 7.642 with no spike — and it
does not depend on the dent, so no beat can drive it there. Worth knowing, not
worth guarding today.

**The three ambient `nodule_aim` callers are exact-off — verified structurally,
not asserted.** `grep` finds exactly three (`manafold_clips.h:1060/1069/1078`,
Neck/HingeA/HingeB) and two `nodule_aim_rollstable` calls, both in the dent. The
production function is byte-unchanged, and the matrix's `e-identity-pass19` leg
reproduces `P19-FINAL-BANK-INTEGRITY`'s own CRCs exactly. They do retain the
z-then-x blind spot; the report declares this and I agree with both the finding
and the decision not to touch them in a pass that had a block to clear.

## 3. The dip's honesty — the non-hosting list, checked one clip at a time

The brief asked me to confirm the non-hosting list is honest rather than
convenient. It is, and two of the five I could prove rather than read:

| slot | clip | claim | **how I checked it** |
|---|---|---|---|
| 7 | Still | two-frame form diagnostic | `build_still` contains **no** `antenna_knead` / `swallow_nodules` call |
| 13 | Trick | plant contact pinned | share 0; the plant holds by eye (§6d) |
| 15 | mana lab | forked knead on its own timeline | `build_manalab` calls **`lab_antenna_knead`** (`manafold_lab.h:1060`), a different function; the production knead is never reached |
| 16 | nodule-solo | shows nodules moving independently | `build_nodule_solo` contains **no** knead call |
| 21 | Taunt III | its crown shuffle already ranks B lowest | **368 of 368 frames byte-identical with `ZHAO_U02_KNEAD_DIP_PM=0`** — I rendered both banks. The dent provably does not run there, and R5 still reads **+64 mm**. The owner's own named reference achieves the ranking by its own mechanism. |

Slot 21's byte-identity is the strongest form of this evidence available and it
was not in the report. **The old 750 entries on 15 and 16 were indeed an
unreachable gate state** — a leg that could never go green being read as a
finding — and 0 is the honest declaration.

I also confirmed the slot-23 schedule mapping: `knead_schedule_slot` has exactly
**three** callers (`manafold_clips.h:3318`, `manafold_rear_audit.cpp:915` and
`:1208`), matching the three lookups the report says fell onto a 750 default.
Slot 23 prints share **715** — slot 0's — on my run.

## 4. The particle reaction — judged at native, by looking

I re-measured the bias check (§1: 127 frames / 2,745 px on Hover, against the
first version's 12 / 72) but **that is not the acceptance and I did not treat it
as one.** The acceptance is `P20-LOOKS2/11-particles-native.jpg`, at 384×240,
the size it ships at.

**It reads.** At Hover f269 and Inspect f268, with the reaction off the lightning
figure sits compact and high inside the loop; with it on the same figure is
visibly **wider, flatter and lower**, and the mote cloud has dropped with it. At
native you can see the mana body change shape. At 4× (`10-particles-4x.jpg`) it
is unambiguous.

Three things I checked rather than assumed:

* **Outside the window the frames are identical** (f340, verified byte-exact),
  so the reaction is scoped to the beat and is not a general drift.
* **The v18/v19 mana character survives** — population, palette, bolt shapes and
  the body's inner spark are unchanged; only position and spread move.
* **The distance-scaled lines survive.** On Drift (`12-drift-lines-3x.jpg`) the
  creature is far away, the reaction is correspondingly tiny, and the thin ink
  is unchanged. That is the correct behaviour, not a failure to react.

This is a genuine repair of the crayon-grain fault, not a re-description of it.
It is a *moderate* read rather than a loud one — you have to be looking at the
loop — but it is well past the threshold where it measured present and looked
like nothing.

## 5. ITEM 4 — the hand-off bound, 320 → 420. **JUDGEMENT: LEGITIMATE.**

This was the item to scrutinise hardest, so I did not reason about it — I
measured it.

**First, a fact that changes the frame and is not in any report:** the hand-off
bound **did not exist at the branch point.** `git log` over
`4b3d4576..HEAD -- manafold_rear_audit.cpp` shows `kGateHandoffMaxMm` first
appearing at `934a73a4` ("the R4 STRAIN gate"), **inside pass 20** and *before*
the bow repair landed at `5a18cdf7`. So 320 was never an inherited contract being
relaxed; it was a number this pass authored against the geometry it had at the
time.

**Second, I measured what that geometry actually reads**, with the shipping
binary and the pre-bow solve switched on:

| configuration | hand-off | centreline turn | rail floor | rail step |
|---|---|---|---|---|
| `REAR_BOW=legacy` — **the pre-bow band** | **270 mm** | 35.60° | 0.129 | 0.0647 |
| shipping (arc), beat **on** | **361 mm** | 113.03° | 0.692 | 0.1630 |
| shipping (arc), `KNEAD_DIP_PM=0` | **361 mm** | 113.03° | 0.692 | 0.1630 |

That settles the question the brief posed, in the terms it asked for:

1. **320 provably described the pre-bow geometry.** 320 = 270 × 1.185 — the
   shipping value of the band it was authored against, plus ~19% headroom. The
   report's reasoning ("calibrated against a band that could not bend") is not
   an excuse; it is a measurement, and I reproduced it.
2. **361 is a consequence of the accepted bow fix and of nothing else.** The
   hand-off is `|P_b0(v) − P_b1(v)|` — how far a rear-band vertex's two
   influencing bones disagree about where it goes. A band that *bends* must have
   neighbouring bone frames diverge, so this quantity must rise, and it does:
   ×1.34 against a centreline turn that rose ×3.17. The rise is **sub-linear in
   the turn**, which is what you expect if the bow distributes the bend along
   the helper chain rather than concentrating it at one joint — i.e. the number
   behaves like the geometry and not like a fudge.
3. **The pass's new art contributes exactly zero.** 361 with the beat on is
   digit-identical to 361 with the beat off. I ran both. A gate fitted to the
   art would have had to move *for* the art; this one did not move for it at all.
4. **The construction rule is unchanged.** 420 = 361 × 1.163, the same
   shipping-plus-headroom form as 320 = 270 × 1.185. The rule was applied, not bent.
5. **It fires, and it does not over-fire.** I fired both controls myself:
   `--fail-rear-frame` (the v18 legacy-root End frame — a *real* defect, the one
   this pass repaired) drives it to **523 mm** and R4 attributes
   `[rail-floor rail-ceiling hand-off ]`; `--fail-rear-joint` reads **393 mm**
   and deliberately stays under, failing only `[rail-ceiling ]`, mask 0xA
   unchanged.
6. **The eye agrees at the worst sample.** `P20-LOOKS2/14-channel-trick-4x.jpg`
   is Channel f078/080/082 — precisely the 361 mm, 113° frame. The rear arm fuses
   into the body as one continuous rounded trunk, no crease, no gap, no pop
   across the three frames.

**So: not a gate fitted to the art. No correction required, and I propose no
change to 420.** The attachment bound is untouched.

**The one caveat I insist on stating.** A "shipping value + 16%" ceiling is
calibrated *by* the quantity it measures, which makes it a **regression** ceiling
and never a **correctness** one. It can only say "do not get worse"; it can never
say "this is right". For this quantity that is the correct choice — there is no
analytic answer for how far two skinning bones may legitimately disagree, and the
only correctness evidence available is the eye, which I took and which passes.
But it must be quoted as what it is, and the constant's own comment already says
exactly that. A residual weakness worth one line for the next pass: the bound is
on the **bank worst**, so a single clip drifting 361 → 419 is invisible to it. A
per-slot form would be stronger — but that is a new gate, not a bound correction,
and it is out of this remit.

**Bounds not moved — verified against `4b3d4576`, not asserted:**
`kAntennaMaxAngularStepDeg` (8), `kSpanMinRunMm` (80), `kSpanStretchMaxPm`,
`kSpanCompactionMinPm` and the ±2 pm tolerance are byte-identical. The full
`git diff 4b3d4576 HEAD -- manafold_spangate.cpp` constant diff is **additive
only** (`kProbeDelta`, `kEarly`, `kDentPinMaxMm`). Nothing was relaxed anywhere.

## 6. Visual QA — production ink, native and 4×, `P20-LOOKS2/`

17 images, all ≤1600 px JPEG q80. Frames sampled by **badness** (ranked against
the A/B banks) and by full contact sheet, never by even index.

### (a) Is the rear connection whole through the whole orbit? — **YES.**
`01-inspect-allframes.jpg` (all 600 Inspect frames) shows no broken, blank or
exploded frame. `08-inspect-orbit-3x.jpg` and `09-rear-join-5x.jpg` take the join
at eight orbit angles including **f380, pass 19's own worst rip**: at every angle
the return arm is a continuous rounded tube that wraps and fuses into the body.
No pinched stub, no gap, no piece torn out. `14-channel-trick-4x.jpg` takes the
worst sample in the bank (Channel f080, 361 mm hand-off, 113° turn) across three
consecutive frames — one continuous trunk, no crease, no pop.
`07-inspect-seam-4x.jpg` walks f594→f005 across the loop seam: continuous, no
pop at f599→f000.

### (b) Does the knead read as a knead, AND does B visibly reach the bottom? — **YES, and this is the change since the last review.**
The first review's finding was *"B visibly becoming the lowest ball is not
legible."* At 1.9× the depth, with the flip gone, it is.

`16-knead-native.jpg` is the one that decides it — **at 384×240**: resting,
pressing, deepest, recovering, the dip-off control at the same frame, and resting
again. The loop's top goes from a smooth rounded dome to a **pressed-in V**, and
the difference against the control at the same frame is unmistakable at ship
size. `06-loop-whole-6x.jpg` and `04-knead-beat-5x.jpg` show it larger: the top
run sags well below the line joining its ends, the middle of the loop becomes the
lowest part of the top run, the tube **stays a tube** throughout — no slab, no
kink, no broken outline — and it returns.

**One honest limit, which is a property of the clips and not a defect.** The read
is strong on the hosting clips that hold still (Hover, Inspect, Channel) and
weakest on the fast one-shots. I looked at slot 9 (**Fall, the worst margin
at +37 mm**) specifically — `15-fall-worst-6x.jpg` — and there the loop is small,
moving fast and partly out of frame, so the ranking is achieved but barely read.
The measurement and the eye agree about where this is strong and where it is
thin, which is the right relationship between them.

### (c) Do the particles react, deliberately? — **YES.** §4.

### (d) Any new faults? — **NONE FOUND.**
`02-hover-allframes.jpg` (all 600 Hover frames) and `01-inspect-allframes.jpg`
(all 600 Inspect frames) are clean. `13-bank-2x.jpg` covers Rest, Blown,
Taunt III, Trick, Death-drop, Fall, Channel and Taunt: continuous enclosed ink
outline everywhere, eyes intact and correctly placed, no seam, no stray geometry,
mana character unchanged, distance-scaled lines thinning correctly on Drift and
Blown. Ground contact sensible — Rest sits on the dirt, Death-drop lies on it,
and **Trick's plant holds** across f190/200/210 (`14-channel-trick-4x.jpg`), the
tubes reaching the ground and staying there.

## 7. Two real faults I found, and fixed

Both are in instruments. **No bound, threshold, art value or shipping byte was
touched**, and I verified that with the identity legs.

### 7a. The judged-configuration banner was blind to every MECHANISM SELECTOR

This is the important one. `print_judged_config()` is the pass's *durable answer*
to the `--dip` defect — the thing that makes an overridden figure impossible to
mistake for a shipping one. Its first version covered **fourteen numeric
constants and no mode selector**, while both gates fully honour four of them.
Measured on the shipping binaries, before my fix:

```
ZHAO_U02_KNEAD_DIP_SOLVER=carried
  mrear R5 :  19 clips author a dip; 18 NEVER REACH LOWEST; worst margin -192 mm
  mspan G10:  "0 dent samples"  (the leg silently measures nothing)
  banner   :  "every shipping constant at its shipped value"     ← the claim

ZHAO_U02_REAR_BOW=legacy
  mrear R4 :  hand-off 270 (not 361), turn 35.60 (not 113.03), rail 0.129 (not 0.692)
              -- i.e. the ENTIRE PRE-REPAIR CREATURE
  banner   :  "every shipping constant at its shipped value"     ← the claim
```

Either one could have put a figure from a different creature — in the second
case, from the creature this pass exists to repair — under a line asserting there
was no override. **That is `--dip` exactly, reproduced inside the instrument
built to prevent it**, and it is this pass's *fourth* wrong-operand case. It is
also why CLAUDE.md's rule reads the way it does: a detector reading "no override"
is a claim, and it is the claim to check hardest.

**Fix** (`manafold_art.h`): a `Mode` table printed *before* the numeric rows,
covering `KNEAD_DIP_SOLVER`, `REAR_BOW`, `REAR_SOCKET_FRAME` and
`REAR_SPAN_LIMIT`; six missing numeric rows added (`rear bow max a16`, `rear span
travel/soft/deep-bias`, `rear carrier calm`, `taunt3 punch A`); and the
no-override line now reads *"every shipping constant **AND MECHANISM** at its
shipped value (solver dent, bow arc, …)"*. A selector that swaps the mechanism is
a bigger override than any constant that scales it, so it prints first. Fired
four ways as its own positive control:

```
(none)                                  every shipping constant AND MECHANISM at its shipped value
ZHAO_U02_KNEAD_DIP_SOLVER=carried       knead dip SOLVER carried **OVERRIDE, shipping dent**
ZHAO_U02_REAR_BOW=legacy                rear BOW legacy **OVERRIDE, shipping arc**
ZHAO_U02_REAR_SOCKET_FRAME=legacy-root  rear socket FRAME legacy-root **OVERRIDE, shipping arm**
ZHAO_U02_KNEAD_DIP_PM=0                 knead dip gain pm 0 **OVERRIDE, shipping 550**
```

### 7b. R5's `dip_stuck` arm fires at 2 of 19 — its claim was too strong

The arm does fire, so this is not a dead detector and the committed-control law
is satisfied. But the close report says of the return quantity *"nothing but the
dip can hide that"*, and that is not what it does. Under its **own** positive
control (`--fail-dip-stuck`, the dip frozen at a full envelope on every clip, so
the defect is present on all 19), I read every per-clip line:

* shipping returns: **+363 … +468 mm**
* under the control: **+23 … +430 mm** — a real reduction on every clip
* clips crossing the 60 mm bound: **slot 9 (+23) and slot 10 (+25). Two.**

The cause is operand contamination, not blindness: the return is measured against
`min(A_y, C_y)`, and A and C run their own ambient nodule schedule. When an
*outer* ball dips on its own timing, a B frozen at the bottom is still "above the
lower outer ball". So the arm would catch a bank-wide stuck dip and would
probably **miss a stuck dip on one clip** — which is the regression it actually
has to catch.

**I did not move the bound** — that is outside this remit, which was to keep
bounds untouched. I recorded the measured sensitivity beside the constant, with
the numbers a correction would be made from (shipping worst +363, so a floor
around 250 keeps ~45% headroom while catching 15 of 19 under the same control),
and the note that whoever raises it owns re-firing the control and re-reading
every per-clip margin. **Open issue 1 below.**

## 8. Controls I fired myself

Six, each confirmed to fail **for its own reason** — not merely to return RC 1:

| control | rc | the reason it fired |
|---|---|---|
| `--fail-rear-frame` | 1 | hand-off **523 mm**, R4 attributes `[rail-floor rail-ceiling hand-off ]`, mask **0xB** |
| `--fail-rear-joint` | 1 | rail ceiling **2.272**, hand-off **393 mm — deliberately UNDER 420**, `[rail-ceiling ]` alone, mask **0xA** |
| `--fail-dip-stuck` | 1 | **2 clips do not return** (the ranking arm), mask **0x10** |
| `--fail-no-dip` | 1 | **0 clips author a dip** (the other arm — a different reason, same mask), mask **0x10** |
| `--fail-dent-pin` | 1 | frame drift **93.658°** against the shipping 0.066; position **unchanged at 8 mm**, attributed `0x10300` |
| the banner, 4 ways | — | §7a |

`--fail-dent-pin` deserves a note: it confirms the report's own honest admission
that G10's **position** operand is structurally blind to the fault (8 mm with and
without the pin) and only the **frame** operand can see it. That is the
two-operands-moving-together law applied correctly, and the control is what
proved it — exactly as CLAUDE.md requires.

**Override sweep, done independently.** I grepped every assignment to a `g_u02_*`
shipping global across all eleven gate/driver `.cpp` files. Every one is a
saved-and-restored `--fail-*` mutant with a `[MUTANT]` banner, a strict
environment parse, or `manafold_shellgate.cpp`'s declared inverted-polarity
control. **No second `--dip`.** `mrear`'s only two writes to
`g_u02_knead_dip_gain_pm` are `--fail-no-dip`'s mutant and a comment recording
that `--dip` no longer moves it. Confirmed.

## 9. Receipts

* **Gate matrix: 159 legs, 159 PASS, 0 FAIL** —
  `P20-RECEIPTS/gate-matrix-review2.txt`, run from scratch in **one
  invocation**, the beat **ON**, from a clean rebuild of every binary after both
  repairs above.
* **All four identity legs exact**, including the two the close re-baselined:
  `e-identity-pass19` `0xA2D0E051 0x779615BB 0x75BC4777`, `e-identity-dipoff`
  `0xEFE5AFC1 0x9E71DF79 0xC81598AA`, `e-identity-carried` `0x40AA70E8
  0xF6BD3444 0xC81598AA`, `e-identity-legacy` `0x46A9C936 0x99CCAC21
  0x75BC4777`.
* **Legacy toggles byte-exact against pass 19:** the `e-identity-pass19` leg
  (`KNEAD_DIP_PM=0 REAR_BOW=legacy`) reproduces `P19-FINAL-BANK-INTEGRITY`'s own
  CRCs — Hover `0xA2D0E051`, Inspect `0x779615BB`, Taunt III `0x75BC4777`.
* **Taunt III byte-identity with the dip off:** 368/368 frames, rendered by me
  (§3).
* **The shipping bank reproduces from my build, and my two edits move zero
  rendered bytes.** I rendered the production bank myself, with production ink,
  from my own binaries *with both repairs in place*, and all four sequence CRCs
  the close reports match exactly: Hover **`0x93B95AEE`**, Inspect
  **`0xD00478A1`**, Taunt III **`0xC81598AA`**, Blown **`0xC6AAF7AD`**. That is
  two claims settled at once — the tree's bank is what the report says it is,
  and the instrument repairs in §7 are genuinely renderer-invisible.
* **My binaries** (a direct build is not byte-reproducible across environments,
  so an md5 identifies a *build*, not a tree — the identity legs are the tree's
  receipt):

```
8129154eb2fd21bc0d626a1852c9bde3 *manafold-eyesize.exe
7a2f06c05f7fc99adb0f0ec510ac9f0e *manafold-meshcheck.exe
d58329a1acf1c9e408cc8bd923c824c4 *manafold-motiongate.exe
b714a8dc4c59a3f0fad987373438ab62 *manafold-nodule.exe
994f043cbb7ab70b55eb80fdadd84659 *manafold-outlinegate.exe
683f92df39cd977c31100c42a612e077 *manafold-probe.exe
05fd40a0b2109cf763c87bb20367ba8a *manafold-public-jointgate.exe
6414f0cf618b463583b31e4361297e07 *manafold-qa-p12.exe
05eb889871e7499d1cfd0ecb3d6c1c5d *manafold-rear-audit.exe
ae13e175d6b7f00f561f900ea75d71cd *manafold-shellgate.exe
bfd01853e2bab9b8d0fd4fc7dd3e353e *manafold-spangate.exe
0a1d9a7cab992787496d478df8a234ac *zhao-reel-cel.exe
```

* **Pictures:** `P20-LOOKS2/`, 17 images, the index in §6.

## 10. Open issues, in priority order

1. **R5's return bound is a 2-of-19 detector** (§7b). Measured, recorded beside
   the constant, not changed. The next pass that touches R5 should raise it —
   with the control re-fired and every per-clip margin re-read, not the aggregate.
2. **The hand-off bound is on the bank worst, not per slot** (§5). A single clip
   drifting from 361 to 419 is invisible to it.
3. **`manafold_rear_audit.cpp` parses six authoring ladders with bare
   `std::atoi`**, so a malformed value silently becomes 0 rather than RC 2. The
   report records this and does not fix it; with the banner now complete the
   *consequence* is at least visible in the log. Still worth closing.
4. **`g_u02_front_flex_gain_pm` has no `constexpr` shipping companion** (it is an
   `inline int32_t = 1500`), so it cannot be given a banner row in the form the
   others use. A v18 knob, not this pass's, but it is the one remaining
   override-reachable value the banner cannot name.
5. **The three ambient `nodule_aim` callers keep the z-then-x blind spot**
   (§2). Exact-off today and correctly left alone; `arc_from_y_about` is
   available to them append-only when a future beat drives an ambient carrier
   hard enough to need it.
6. **The depth ladder's top rung is the shipped value, so the eye's preference
   is a lower bound rather than a choice.** The first review's row 8 said the
   depth was art fitted to the gate. That is much less true now — the repair
   moved the ceiling from ~2200 to ~4750, so the art got a far wider range — but
   the ladder the eye judged was `{OFF, 2800, 3400, 3800, 4200}` and **4200 is
   its top rung**. When the chosen value is the deepest one shown, you have not
   demonstrated that the eye *preferred* it; you have demonstrated that the eye
   wanted *at least* that much, and the ceiling then decided the rest. §11 is
   my own test of this, and **the outcome is fine** — but I had to render the
   missing rung to find that out, which is the point. The general rule is worth
   carrying: **a ladder whose chosen rung is its last rung has not finished
   asking the question** — put a rung above the candidate, even one you expect
   to reject, or the answer is "as deep as allowed" wearing a judgement's
   clothes.

---

## 11. The rung the ladder did not have

I rendered it: Hover at `ZHAO_U02_KNEAD_DENT_DEPTH_PM=4600`, production ink, one
rung above the shipped 4200 and still inside the 8° ceiling (which is reached at
about 4750). `P20-LOOKS2/17-depth-rung-above-5x.jpg` puts it beside the shipping
depth and the off control at the same frame.

**4200 stands.** At 4600 the press is marginally deeper and the gain in
legibility is negligible, while the two tube runs at the left shoulder begin to
crowd each other — the extra depth buys congestion rather than read. So the
shipped value is a real choice and not merely the ceiling's shadow, and the
first review's row-8 concern about the depth is now closed rather than only
reduced.

I am recording it this way on purpose. The ladder's practice was incomplete and
its result was correct; those are two different facts and collapsing them is how
a method that happened to work once gets kept. **The value is right. The way it
was shown to be right needed one more rung, and now it has one.**
