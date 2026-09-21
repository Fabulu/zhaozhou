# Manafold pass 23: implementation

**Date:** 2026-09-21
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-24-2026-09-21.md`
("Fix the still opens" — the scope is exactly pass 22's open list)
**Worker:** Claude (sole Opus worker; no sub-agents, no Qwen, no HomeAI)
**Source:** Zhaozhou `manafold-pass23`, from the production-verified pass-22
main `83002801`
**Verdict:** DONE for all four items. **Not rendered here:** the 22-subject
bank, the encode, the merge, the deploy — by instruction.

**Matrix: 195 / 195 PASS, 0 FAIL**, one invocation
(`P23-RECEIPTS/runmatrix_p23.sh` -> `gate-matrix.txt`).

---

## 1. Item 1 — the knead's lightning reaction on the shallow-pressing clips

### What the five slots are

By frame count against the pass-22 bank manifest: **1 Drift (300f), 14 Damage
(464f), 17 Death-drop (450f), 18 Death-gutter (590f), 20 Blown (292f)**.

### The lever, confirmed structurally before anything was moved

```
kKneadDipClipPm[slot] -> dip_gain -> dent depth -> posed SAG (mid_y(A,C) - B_y)
  -> dip_pm = smoothstep((sag - 90)/(420 - 90)) * kFoldDipGainPm/1000
  -> dip_shape_pm = min(1000, dip_pm * 1000 / kFoldDipShapeRefPm)
  -> kFoldDipRollA16 / kFoldDipTumbleA16 / kFoldDipShearPm
```

So the press depth is the only per-clip lever, the roll is bank-wide (raising it
would over-drive Inspect and Hover, which already read), and the reaction
**saturates** once `dip_pm` reaches the 150 reference. Both the pass-22
implementer and the pass-22 reviewer named this table; the chain above is why.

### A knob first, because the value had to be chosen by eye

`kKneadDipClipPm` had **no authoring ladder at all** — every rung meant a
relink, which is how the value that should have been laddered by eye ended up
not being. Pass 23 adds:

* `g_u02_knead_dip_clip_pm[]`, a mutable mirror **initialised from the constexpr
  table** (one source of truth, `make_knead_dip_clip_pm()`), and
  `knead_dip_clip_pm()` as the ONE production read — the solver, R5 and R7 all
  go through it, so a ladder run cannot be live in the renderer and inert in a
  gate (the fault `apply_knead_dip_env` was written for).
* `ZHAO_U02_KNEAD_DIP_CLIP_PM=<slot>:<pm>[,...]`, parsed in the shared
  `apply_knead_dip_env`, **strict in both directions** — 10 new RC-2 selector
  legs in the matrix.
* One substantive refusal: **it will not make a non-hosting clip host a dip.**
  A slot whose shipped entry is 0 declares that its clip never calls
  `antenna_knead`/`swallow_nodules` (15, 16) or that another authority owns its
  carriers (7, 13, 21). Pass 20 had to repair exactly that — a nonzero entry
  telling R5 those clips hosted a dip, a permanently red unreachable leg — and a
  knob that could reintroduce it would be the same defect with a new door.
  Setting a hosting slot DOWN to 0 is allowed, and is R7's drop control.

With the gate answering in **0.86 s**, the ladder cost minutes.

### The chosen depths, by eye at native and 4x through the dip

Plates `P23-LOOKS/01`-`06`, notes `P23-NOTES/FINDINGS-01-press-ladder.md`.
Production ink throughout (`ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`).

| slot | clip | p22 | **p23** | R7 rot | R7 form | the visual reason |
|---|---|---|---|---|---|---|
| 1 | drift | 730 | **900** | 7.10 -> **28.18** | 36.2 -> 131.5 | 730 is a flat open loop lying horizontally. **900 turns it into a clear diagonal wedge with the green pocket standing upright** — at Drift's 128 px that is the difference between a bright smear and a lightning that turned. 950 narrows it to a vertical sliver and it stops reading as a loop. |
| 14 | damage | 635 | **780** | 4.49 -> **27.82** | 23.9 -> 180.4 | 635 is a broad upright double loop. **780 reads as a tilted spiral: the right wing lies almost flat, the inner curl tightens, the green pocket becomes a distinct bright lozenge.** 840 adds nothing and flattens the loop into a plate. |
| 17 | death-drop | 635 | **740** | 4.76 -> **22.31** | 24.3 -> 180.7 | 635 is a compact angular kite. **740 is a clean long blade across the top with a sharp left hook — plainly re-formed and still anchored on the arm.** 790 steepens it until it reads detached. |
| 18 | death-gutter | 590 | **720** | 1.70 -> **38.77** | 7.6 -> 153.5 | Here the PRESS is the visible half. **720 presses the arm down convincingly and the strand turns and kinks with it.** 760 folds the arm into a collapse rather than a knead — **backed off, per the brief's spasm clause.** |
| 20 | blown | 815 | **815 — UNCHANGED** | 3.70 | 24.7 | see below |

The band was chosen against the pass-22 reviewer's own reading of the bank
(35-43 deg reads plainly, 14-28 deg reads): every new value lands inside the
readable range and none of them at the loud extreme.

### Slot 20 (Blown) — the lever is INVERTED, and it was not forced

The direction's premise is true on four clips and **false on Blown**, measured
two independent ways:

| share | 300 | 400 | 500 | 700 | 740 | **815** | 900 | 1000 |
|---|---|---|---|---|---|---|---|---|
| R7 rot deg | 46.69 | 28.26 | 15.30 | 6.29 | 4.90 | **3.70** | 2.28 | 0.65 |
| R5 B-lowest mm | -189 | -189 | -132 | -3 | +17 | **+45** | +56 | +56 |

and, off the reel's own `U02_FOLD_DEBUG` trace rather than the gate's
descriptor, peak `dip_pm` on complete Blown: **400 -> 102, 500 -> 70, 700 -> 31,
815 -> 18, 1000 -> 3.**

A deeper press gives Blown a **smaller** sag. The mechanism is in the source:
the reaction rides on `sag = mid_y(A,C) - B_y`, and the dent's ambient duck
(`kKneadDentAmbientDuckPm`) scales the ambient A/B/C offsets down in proportion
to the press. Blown's ambient pose already carries B below the A/C line — that
is what being knocked backwards looks like — so the duck removes a sag that was
already there faster than the dent adds one.

**Independently corroborated by a control written for something else:** under
`--fail-no-dip` (the dent switched off entirely) **slot 20 measures 36.03 deg**,
the highest in that run.

**Which bound, and by how much.** Going the only direction that helps breaches
`kGateDipMarginMm` (20 mm, R5's B-strictly-lowest floor):

* 780 pm: margin **+34 mm** (inside) — rot 4.47 deg, visually nothing;
* 740 pm: margin **+17 mm** — **breaches by 3 mm** — rot 4.90 deg, still nothing;
* 500 pm: margin **-132 mm** — **breaches by 152 mm** — rot 15.30 deg, the first
  setting that would read.

**Not forced.** The honest lever is the ambient duck, which is bank-wide and
would move all 19 hosting clips — an owner decision, not a slip-in. Blown is
left at 815 and **declared exempt in R7 with the reason beside it**, still
floored at 2.0 deg / 12.0 pm so it cannot quietly fall to zero.

### Nothing else moved

* **B strictly lowest: 19 of 19 hosting clips, re-measured.** `R5 DIP: 19
  clip(s) author a dip; 0 never reach lowest; 0 do not return; worst margin
  29 mm (slot 9, need 20)` — the same worst clip and the same number as
  pass 22. The four raised clips' own margins moved **+78->+79 (drift),
  +94->+159 (damage), +115->+167 (death-drop), +93->+163 (gutter)**, all inside
  the pass-20 authoring target of 70-180 mm.

  > ⚠ **CORRECTED BY THE PASS-23 REVIEW (P23-REVIEW.md §6, R1).** This line
  > originally read `+78->+80`, `+94->+167`, `+115->+171`, `+93->+145`. The
  > BEFORE values were right; the AFTER values were carried over from ladder
  > rungs and never re-read from the shipping run. The figures above are the
  > shipping run's, from `P23-RECEIPTS/p23-mrear-gate.txt` and from the
  > reviewer's independently built binary, which produced a byte-identical file.
  > The conclusion is unchanged: all four are inside 70-180 mm.
* **mspan PASS: 0 failures.** G9's bank-wide worsts are **unchanged to three
  decimals** — angular step 7.642 deg slot 8 f0101 B (ceiling 8.0), position
  step 72.157 mm slot 20 f0053 C. The raised presses cost the continuity budget
  nothing.
* **No bound was relaxed anywhere.** The source diff removes or changes exactly
  **one** `constexpr` line — `kKneadDipClipPm`, the art value the direction
  asked to move. Every other deletion is a read routed through the new
  accessor, the tautological far-leg line, or a printf format string. Audited
  line by line.
* Untouched: Trick's pinned plant, the pass-21 rods rig, the pass-22 dot law and
  every `kManaDot*`/`kManaLine*` constant, `kFoldDipRollA16` and its two
  companions, the palettes, the eye lane, the site. No RTL change and no fit.

### Looking — and one thing badness-sampling found that a dip plate would not

Complete every-frame antenna sheets at native for all five clips
(`P23-LOOKS/10`-`14`) plus a per-frame changed-pixel trajectory against pass 22
(`07`). The trajectories are smooth humps that return to zero; **drift's first
and last frames are unchanged, so the loop seam is exact.** Death-drop and
death-gutter change at f0 — both are one-shot death clips whose pose already
sags at the start, so that is correct, and neither loops.

**Damage showed a 2-frame spike at f414/f416 — 10,886 changed px (11.8%) against
~5,000 either side.** Sampled by badness and looked at (`08`, `09`):

| frame | any change | delta > 32 | delta > 64 | max delta |
|---|---|---|---|---|
| f413 | 5,303 | 2,441 | 1,123 | 230 |
| **f414** | **10,886** | **2,337** | **998** | **206** |
| f415 | 4,972 | 2,228 | 965 | 214 |

The spike is **entirely in the sub-perceptual tail**: the visible change
(`> 32`) is *lower* at f414 than at its neighbours and decreases monotonically
across f408-419, and the diff map shows the extra pixels are a faint one-level
shift of the body's dithered fill over the whole silhouette, not anything in the
mana. **No pop. The any-change count lied; the amplitude histogram and the eye
agreed with each other** — the art law's comparison side doing its job.

---

## 2. Item 2 — R6's LINE far leg, and item 3 — R7's per-clip floor

Full detail, with both controls' output quoted:
**`P23-NOTES/FINDINGS-02-gate-repairs.md`**. In brief:

**R6.** The far leg compared `gate_splat_r_px(ms, far_q8)` against
`mana_line_r_px(ms.r_px, far_q8)` — the same expression. Replaced by two
independent operands: a **stored** `kGateLineFarWidthPx[0..48]` (pass 19's law
evaluated once at the 128 px witness and written down as integers, indexed by
authored radius so it pins the LAW and no art value, with `static_assert`s on
both witnesses and the regeneration one-liner in the comment), and the near
evaluation (`far < near` wherever the arithmetic requires it). Plus a population
counter so "0 violations" cannot be the empty set.
Shipping: **7,650,706 of 11,561,258 line splats eligible, all thinner at far; 0
off the stored law, 0 untabulated.**

**R7.** The floors compared the bank maxima. Now every hosting clip is floored
on its own numbers at **10.0 deg / 50.0 pm**, set below the measured worst
non-exempt clip (slot 19, 12.44 / 68.2) with margin; the bank legs are kept; one
**declared exemption** (slot 20, still floored at 2.0/12.0); and the hosting
**count** is asserted at 19, closing the hole a per-clip floor opens — a clip
that leaves the list is floored by nobody.

### The controls fired, and what they exposed

| control | mask | what moved |
|---|---|---|
| `--fail-line-far` (new) | **0x20** alone | thinner at far 7,650,706 -> **0** |
| `--fail-line-scale` | 0x4 -> **0x24** | off the stored law 0 -> **11,561,258** |
| `--fail-knead-clip` (new) | **0x40** alone | slot 18 at pass-22's 590 pm: 1.70 deg **UNDER**, bank maxima **green** |
| `--fail-knead-drop` (new) | **0x40** alone | 18 clips host, expected 19; every other leg **green** |
| `--fail-no-dip` | 0x10 -> **0x50** | 17 of 19 clips lose the reaction |
| `--fail-dot-scale` / `--fail-dot-flag` / `--fail-knead-shape` | 0x20 / 0x20 / 0x40 | unchanged |
| shipping | **0x0 GREEN** | R6 0, R7 0 |

**The blindness demonstrated, not argued.** Both binaries, same mutant
`--fail-line-scale`:

| binary | census says | R6 |
|---|---|---|
| pass 22 | "non-dot gained distance **0**" | **0 violations** |
| pass 23 | off the stored law **11,561,258** | **2 violations** |

and the same on `--fail-no-dip`: pass 22's R7 printed *"2 clips reach the press,
best rot 36.03 deg"* and **0 violations** while seventeen clips had lost their
reaction entirely. **Two mask expectations therefore move, and both moves are
the repair, not a regression.** They are carried in the matrix with the reason
beside them.

### Subset check

R6's census walks all **24** clips; its new far leg sits inside that same walk.
R7 floors all **19** hosting clips, prints each one's floor and verdict, and
asserts the count. Carried forward as an open item: pass 19's R3 line census
still samples slots 1 and 0 only — out of scope here, and no longer the only
assertion about the line population.

---

## 3. What stayed byte-identical

| configuration | hover | inspect | drift | hasty |
|---|---|---|---|---|
| pass-21 baseline | 0x200AA3E7 | 0x1CFE8375 | 0xFB17B7D6 | 0x425AA389 |
| pass-22 shipping | 0xEFCCD8FA | 0x6B1077D0 | 0xB6AB88AA | 0x6B85677A |
| **p23 exact-off** (`KNEAD_DIP_CLIP_PM=1:730,14:635,17:635,18:590`) | 0xEFCCD8FA | 0x6B1077D0 | **0xB6AB88AA** | 0x6B85677A |
| **p23 + pass-22's exact-off knobs** | 0x200AA3E7 | 0x1CFE8375 | **0xFB17B7D6** | 0x425AA389 |
| **p23 SHIPPING** | 0xEFCCD8FA | 0x6B1077D0 | **0xF376C81F** | 0x6B85677A |

* **Exact-off reproduces pass 22 on 4 of 4, and pass 21 on 4 of 4.** The
  pass-23 change is a per-clip PRESS DEPTH, which moves the POSE and is
  therefore not switched off by either pass-22 knob — so its own exact-off is
  the ladder knob set back to the four shipped values, and the pass-21 leg now
  carries that too. A matrix rung that did not would be asserting a bank nobody
  can build.
* **Neither identity leg is vacuous.** `e-dots-live` (dots on) and the new
  `e-press-live` (pass-23 depths live under pass-21's knobs, drift
  **0xD243EDE4**) both differ, and drift is in the witness list — the one
  witness clip this pass touches.
* **Hover 600/600, Inspect 600/600, Hasty 240/240 byte-identical to pass 22,
  file by file**, not merely CRC-equal. Drift differs on exactly the 168 frames
  the trajectory locates. Plate `15` is the near read at 4x, unmoved.
* Blown is byte-identical under both binaries (0x16532469).

**Binaries:** g++ 16.1.0 (MinGW-W64 x86_64-ucrt-posix-seh, winlibs),
`tools/reel/build-direct.sh`, 0 warnings, 0 errors.
`zhao-reel-cel.exe` MD5 **35f1ddcc13f8161d6edc46a565ae6399**, SHA-256
`8e3ae2c4aad0a1d2410c93b94d90d9d19d845f3943f73d690a132baaa695f300`.
Full list: `P23-RECEIPTS/binaries-md5.txt`.

---

## 4. Item 5 — the purge, and the tool defect it exposed

**Reclaimed: 2.83 GB, 10,988 `.rgb` files, from the sibling
`C:\programmieren\zencrifice\p12-final\render`.** Dry run
(`purge-02-dryrun-p12final.txt`) then `--apply`
(`purge-03-apply-p12final.txt`), RC 0 both.

**But the first invocation reported `nothing to do`, and that is the finding.**
Run bare from this working copy the tool printed

```
purge_render_intermediates: C:\programmieren\zencrifice\manafold-p16
  candidates : 0 files, 0.00 B ... nothing to do.
```

while **5.66 GB** of stale frames sat in the real root. The default was
`os.path.dirname(REPO)` with a help string promising *"the zencrifice root"* —
true only when the checkout sits directly under it. From
`zencrifice/manafold-p16/zhaozhou` it resolves one level short, to
`manafold-p16`.

This is the file's own lesson wearing a new coat. It exists because *"a rule
that hides waste is not a rule that removes it"*; it had been pointed one level
away from the waste and was issuing a clean bill of health. **A wrong default
that prints "nothing to do" is worse than no default, because it answers the
question.**

Repaired: `zencrifice_root()` finds the root **by name**, walking up for a
directory called `zencrifice`, and falls back to the old behaviour outside that
tree (verified on all three cases). The tool is still dry-run until `--apply`,
so the repair cannot delete anything by itself. A bare run now reports the real
root and the real number.

**Left in place, declared rather than done: 2.83 GB more** in
`zencrifice\zixxtrixx-wholebody-s-spring-20260901\Upheaval\website\scratch-reel`
(another creature's working directory, 2026-09-01). Direction 24 item 5 names
`p12-final` specifically, and deleting outside a named scope is a destructive
act this pass did not have authority for. One command when the owner wants it:
`python tools/maintenance/purge_render_intermediates.py --apply`.

---

## 5. Open issues

1. **Blown (slot 20) still barely reacts, by declared exemption.** The press
   lever is inverted there and the only lever that would work — the dent's
   ambient duck, `kKneadDentAmbientDuckPm` — is bank-wide and would move all 19
   hosting clips. If the owner wants the beat on Blown too, the shape of the fix
   is a **per-clip duck share**, the same construction `kKneadDipClipPm` already
   has. That is a mechanism, not a value, so it wants its own pass.
2. **`dip_pm` still runs at a fifth of its declared range**, bank-wide. Pass 22's
   open issue 2 stands: `kFoldDipRefMm = 420 mm` is a reference no clip's pose
   approaches. Pass 23 raised four clips inside that regime rather than
   questioning the reference, deliberately — moving the reference moves every
   clip at once, and this was a small pass.
3. **Pass 19's R3 line census samples slots 1 and 0 only.** Not repaired (out of
   Direction 24's scope); no longer load-bearing, since R6's whole-bank census
   now carries the line population properly. Worth an hour next time the file is
   open.
4. **Two control masks moved** (`--fail-line-scale` 0x4->0x24, `--fail-no-dip`
   0x10->0x50). Both are the repair working. Anyone diffing matrices against
   pass 22 should read them as such.
5. **`kGateLineFarWidthPx` needs regenerating** if `kManaLineFullRadiusPx` or
   `kGateLineFarRadiusPx` ever move. Two `static_assert`s make that a compile
   error rather than a silent wrong number, and the one-liner is in the comment.
6. **Not rendered here:** the 22-subject bank, the encode, the merge and the
   deploy, by instruction.

---

## 6. Evidence

| file | what |
|---|---|
| `P23-LOOKS/01`, `02` | the four responsive clips, before/after at native 2x and at the dip bottom 4x. ⚠ **`02`'s gutter panel is a LADDER RUNG (590->680), not the shipped 720** — corrected by the review (P23-REVIEW.md §6, R2); the shipped comparison is `P23-REVIEW-LOOKS/01`-`02`, and 720 does appear in the `04` ladder |
| `P23-LOOKS/03`-`06` | the four press-depth ladders at 4x — where each value was chosen |
| `P23-LOOKS/07` | per-frame changed-pixel trajectory vs pass 22, all four clips |
| `P23-LOOKS/08`, `09` | the damage f414 spike: six consecutive frames, and the diff map that explains it |
| `P23-LOOKS/10`-`14` | **complete every-frame** antenna sheets at native, all five clips |
| `P23-LOOKS/15` | Inspect and Hover at 4x — the near read, unmoved |
| `P23-NOTES/FINDINGS-01-press-ladder.md` | written after the looks, before the constants moved |
| `P23-NOTES/FINDINGS-02-gate-repairs.md` | both repairs, both controls, the side-by-side blindness proof |
| `P23-RECEIPTS/gate-matrix.txt` + `gatematrix_p23.sh` + `runmatrix_p23.sh` | 195/195, one invocation |
| `P23-RECEIPTS/p23-mrear-gate.txt`, `p23-mspan-presschosen.txt` | shipping R5/R6/R7 and mspan |
| `P23-RECEIPTS/ctl-fail-*.txt` | every mrear control, each with its declared mask |
| `P23-RECEIPTS/p22bin-fail-line-scale.txt`, `p22bin-fail-no-dip.txt` | **the pass-22 binary's silence on the same mutants** |
| `P23-RECEIPTS/base-mrear-gate.txt` | the pass-22 baseline this pass was measured against |
| `P23-RECEIPTS/purge-01`-`04` | the purge, and the default-root defect before and after |
| `P23-RECEIPTS/binaries-md5.txt` | every binary |
