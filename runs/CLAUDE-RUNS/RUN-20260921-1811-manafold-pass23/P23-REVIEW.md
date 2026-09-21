# Manafold pass 23 — independent review

**Date:** 2026-09-21/22
**Reviewer:** Claude (independent; no sub-agents, no Qwen, no HomeAI)
**Under review:** Zhaozhou `manafold-pass23` @ `79f35660`, Upheaval
`manafold-pass23` @ `7685312`
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-24-2026-09-21.md`

---

## VERDICT: **FIXED** — clear to publish.

Every substantive claim in `P23-IMPLEMENTATION.md` reproduces on binaries I
built myself, most of them to the digit. The art is right on the four raised
clips, the fifth is honestly exempt, both repaired gates genuinely fire, and the
byte-identity contract holds 4/4 on two rungs.

**Two record errors found and corrected** (§6). Neither changes a shipped value;
both were in the write-up rather than the artefact, and both are of the family
this project keeps paying for — a stale number and a stale plate, each reading
like evidence.

---

## 0. What I built and ran myself

Nothing below is quoted from the implementer's receipts unless it says so.

* `tools/reel/build-direct.sh --output .tmp/p23rev`, g++ 16.1.0
  (MinGW-W64 x86_64-ucrt-posix-seh), **13 binaries, RC 0**. My MD5s differ from
  the implementer's (`zhao-reel-cel.exe` `3e9104ba…` vs `35f1ddcc…`) — this
  build is not byte-reproducible, so binaries are compared by **behaviour**, and
  behaviour matched exactly.
* A **pass-22 binary of my own**, from a `git worktree` at `83002801`, for the
  blindness comparison.
* Production ink throughout: `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`.
* Frames read with the committed `tools/reel/rgbframe.py`; plates built with the
  committed `tools/reel/plates.py`. No new frame reader was written.

**My shipping gate output is byte-identical to the committed
`P23-RECEIPTS/p23-mrear-gate.txt`** (`diff` clean). That is the strongest single
result here: an independent build, independently run, producing the same file.

**The full matrix, re-run on my binaries in ONE invocation: 195 rows, 195 PASS,
0 FAIL** (`P23-REVIEW-RECEIPTS/gate-matrix-reviewer.txt`), including all ten new
RC-2 selector legs, every mutant's declared mask and all five identity rungs.

---

## 1. Item 1 — the press depths

### The diff says what the report says it says

I read the whole source diff `83002801..HEAD`. Outside `runs/`, **four files
change**: `manafold_art.h`, `manafold_clips.h`, `manafold_rear_audit.cpp`,
`purge_render_intermediates.py`.

**Exactly one `constexpr` line is modified anywhere in the tree** —
`kKneadDipClipPm`, and within it exactly four entries:

| index | was | now |
|---|---|---|
| 1 drift | 730 | **900** |
| 14 damage | 635 | **780** |
| 17 death-drop | 635 | **740** |
| 18 death-gutter | 590 | **720** |
| 20 blown | 815 | **815 (unchanged)** |

Every other `constexpr` in the diff is an **addition** inside the gate
(`manafold_rear_audit.cpp`), not an art value. **No span, compaction or
attachment bound moved** — those constants are in files the diff does not touch.
Verified by reading, not by the claim.

### B strictly lowest — re-measured, unchanged

```
R5 DIP: 19 clip(s) author a dip; 0 never reach lowest; 0 do not return;
        worst margin 29 mm (slot 9, need 20)
```

**19 of 19, worst margin 29 mm at slot 9** — the same clip and the same number
as pass 22. Confirmed on my own binary.

### mspan — the continuity budget cost nothing

`manafold-spangate.exe` **PASS: 0 failure(s)**, and G9's bank-wide worsts are
unchanged **to three decimals**: angular step **7.642 deg** slot 8 f0101 B
(ceiling 8.0), position step **72.157 mm** slot 20 f0053 C.

### Visual verdicts, per clip

Looked at the implementer's plates `01`–`06`, **and independently rendered and
cropped death-gutter and Blown myself** (`P23-REVIEW-LOOKS/`).

| clip | verdict | what I saw |
|---|---|---|
| **1 drift** 730→900 | **GOOD** | Before: a flat pale loop lying horizontally over the head — a bright smear. After: a clear diagonal wedge, the loop tilted, the green pocket standing upright. **Readable at native**, which is the acceptance criterion. Still a loop, still lightning; the figure morphed, it did not switch. |
| **14 damage** 635→780 | **GOOD** | Before: a broad, near-symmetric upright double loop. After: a tilted spiral — the right wing lies almost flat, the inner curl tightens, the green core becomes a distinct bright lozenge. The largest form change of the four, and **coherent**: the whole figure moves together, so it reads as energy, not as a jerk. |
| **17 death-drop** 635→740 | **GOOD** | Before: a compact angular kite hugging the arm. After: a long blade across the top with a sharp left hook, **still plainly anchored on the arm**. The implementer's worry that 790 reads detached is visible in the ladder; 740 does not. |
| **18 death-gutter** 590→720 | **GOOD, and 760 correctly rejected** | See below — judged on my own render, since the implementer's before/after plate does not show the shipped value. |
| **20 blown** 815 unchanged | **exempt — see §2** | |

### Death-gutter, judged on my own frames

I rendered `manafold-death-gutter` at **590, 720 and 760** from my own binary and
cropped f111 on the antenna at 5x (`P23-REVIEW-LOOKS/01`, native in `02`).

* **590 (pass 22):** the arm is held high-left, the lightning a broad flat open
  loop with an airy gap to the body. Nothing is being kneaded.
* **720 (shipped):** the arm is **plainly pressed down** and angled into the
  body; the loop turns and tightens into a narrower tilted band, a teal pocket
  appears, the strand kinks. This is press-and-answer. **A knead, not a spasm.**
* **760 (rejected):** the arm continues down and the antenna **folds over**
  toward the body, the arm's silhouette losing its shape. It reads as buckling.

**My eye independently reaches the implementer's conclusion.** 720 is well
chosen and backing off 760 was right. On this clip the press is the visible
half, exactly as the report says.

### Not a spasm — the comparison-side check

CLAUDE.md puts measurement on the comparison side, so: per-frame motion energy
across the whole 590-frame gutter clip, before and after.

| | mean | p95 | max | worst local ratio |
|---|---|---|---|---|
| 590 (p22) | 211 | 726 | 1606 @ f315 | 3.00 @ f433 |
| **720 (p23)** | 213 | 731 | **1606 @ f315** | **3.00 @ f433** |

The press got deeper and the frame-to-frame motion **did not change at all** —
same maximum, same frame, same worst local ratio. A spasm would appear as a new
spike or a raised tail. There is none. With mspan's G9 worsts unchanged
bank-wide, the continuity case is closed.

---

## 2. Item 2 — Blown's inverted lever. **The exemption is honest.**

The brief told me not to take this on trust. I did not.

### The inversion is real — reproduced independently, nine rungs

Driven through the production knob `ZHAO_U02_KNEAD_DIP_CLIP_PM=20:<pm>` on my
own binary (`P23-REVIEW-RECEIPTS/blown-ladder-reviewer.txt`):

| pm | 300 | 400 | 500 | 700 | 740 | 780 | **815** | 900 | 1000 |
|---|---|---|---|---|---|---|---|---|---|
| R7 rot deg | 46.69 | 28.26 | 15.30 | 6.29 | 4.90 | 4.47 | **3.70** | 2.28 | 0.65 |
| R7 form pm | 216.6 | 170.5 | 100.1 | 45.2 | 34.0 | 30.4 | **24.7** | 14.8 | 4.0 |
| R5 B-lowest mm | −189 | −189 | −132 | −3 | +17 | +34 | **+45** | +56 | +56 |

**Every figure matches the implementer's table exactly.** The inversion is
monotonic in both descriptors over the whole legal range: a deeper press gives
Blown a *smaller* reaction. It is corroborated by a third, independent route —
under `--fail-no-dip` (the dent removed entirely) **slot 20 measures 36.03 deg,
the highest in the bank**, which is only possible if the dent is *subtracting*
reaction there. The source mechanism the report gives (`kKneadDentAmbientDuckPm`
scaling away a sag Blown's ambient pose already carries) is consistent with
both the sign of the rot change and the opposite sign of the R5 margin change.

**Claim verified.**

### ⚠ One correction to the argument — which strengthens it

The report says 500 pm is *"the first setting that would actually read"*. That
is taken from R7's 15.30 deg, **not from looking** — which is the exact
substitution CLAUDE.md's art law is about.

I looked. I badness-sampled all 292 frames of Blown at 815 against 500 and took
the worst (f138, 1054 px changed >32), then compared at native and at 6x
(`P23-REVIEW-LOOKS/03`, `04`).

**At native the two are indistinguishable.** Blown hurls the creature small and
tumbling with the antenna tucked under the body; the whole strand is about
25 px. At 6x the difference is a slightly different quadrilateral and a small
rotation. **500 does not read on Blown either.**

So the case for leaving Blown alone is *stronger* than the report argues: not
only is the lever inverted and the useful direction blocked by an owner-named
constraint, but **the prize for breaching that constraint would not be visible
anyway.** Blown is a clip where the knead beat cannot read at this framing —
that is a scale-and-staging fact, not a press-depth fact.

### Is it a clip quietly excused from a gate? **No.**

1. The inversion is real (above), so the direction's premise genuinely fails here.
2. Direction 24 item 1 names *"B strictly lowest where it already is (19/19
   hosting clips)"* as a constraint that **does not move**. 500 pm takes slot 20
   to a −132 mm margin, i.e. 18/19. Breaking it would break the brief.
3. The exemption is **declared** — named in source with its reason, printed by
   the gate as `DECLARED-EXEMPT`, carried in the report and in open issues for a
   future pass. It is not a threshold lowered until red went away.
4. **The residual floor is not decorative — I fired it.** At 1000 pm slot 20
   falls to 0.65 deg / 4.0 pm, goes **UNDER** the exempt floor of 2.0 / 12.0,
   and **R7 goes red at mask 0x40**. A total loss of reaction on Blown *is*
   caught, and it is reachable with legal stimulus through a production knob.

### ⚠ Context the report omits: R5's B-lowest leg is SOFT

Worth recording, because it changes what "breaches the bound" means
mechanically. In `manafold_rear_audit.cpp`, `dip_missing != 0` prints an
`OPEN R5` line and **does not set the mask**; only `dip_stuck` (the dip failing
to return) is hard. My ladder shows it: at 300 pm, with slot 20's margin at
−189 mm, **the gate still reads `mask 0x0 -> GREEN`**.

So lowering Blown would have left the matrix green and printed an OPEN line. The
bound that forbids it is an **owner-named constraint**, not a red gate. The
implementer honoured it anyway, which is the right call — but a reader of the
report would think the gate was stopping them, and it would not have been.

---

## 3. Items 2 & 3 — the two gate repairs. **Both fire. I fired them.**

Nine controls run on my own binary, **all RC=1**. Masks and exposures:

| control | mask | what moved |
|---|---|---|
| `--fail-line-far` (new) | **0x20** alone | thinner at far 7,650,706 → **0**; off the stored law 0 → **11,561,258**; R6 **2 violations**. **R3 stays green (0 law violations)** — proving the leg catches a renderer *routing* fault that R3's law sweep structurally cannot see. |
| `--fail-line-scale` | 0x4 → **0x24** | R3 1, R6 2. The added bit is the repair. |
| `--fail-knead-clip` (new) | **0x40** alone | slot 18 at pass-22's 590 pm → **1.70 deg / 7.6 pm UNDER**; bank maxima **green** at 43.00/239.5; hosting still 19. Exactly the configuration pass 22 called green. |
| `--fail-knead-drop` (new) | **0x40** alone | **18 clips host, expected 19**; bank maxima green (next is 40.89); **0 clips under floor** — the count leg is the *only* thing that can fire. Clean isolation. |
| `--fail-no-dip` | 0x10 → **0x50** | 2 clips reach the press; R7 red. |
| `--fail-dot-scale`, `--fail-dot-flag`, `--fail-line-flag`, `--fail-knead-shape` | unchanged | still fire. |
| shipping | **0x0 GREEN** | R6 0, R7 0. |

### The blindness, demonstrated on a pass-22 binary I built

| mutant | pass-22 binary | pass-23 binary |
|---|---|---|
| `--fail-line-scale` | census: *"non-dot gained distance **0**"* over **11,561,258** line splats — **R6: 0 violations** | off the stored law **11,561,258** — **R6: 2 violations** |
| `--fail-no-dip` | *"2 clips reach the press, best rot 36.03 deg"* — **R7: 0 violations, GREEN**, while **17 of 19 clips had lost the reaction entirely** | **R7: 1 violation, RED** |

**Both reported exposures reproduce exactly.** In each case the pass-22 gate as
a whole still went red on a *neighbouring* leg (0x4, 0x10) — the report says so
and does not overstate it.

### Does either repaired gate encode an art value? **No.**

* **`kGateLineFarWidthPx`** stores pass 19's *law* evaluated at a fixed witness,
  **indexed by authored radius**, so every radius 0..48 is covered and any
  authored radius may move freely inside it. I regenerated it from the
  documented one-liner: **all 49 integers match**, and the one-liner's
  arithmetic matches `mana_line_r_px` in `manafold_fx.h` term for term
  (same rounding, `kManaLineMinRPx == 1` floor, `r_px` ceiling, zero at zero).
  Two `static_assert`s make a moved witness a compile error. It pins a law, not
  a value.
* **`kGateKneadClipRotFloorDeg = 10.0` / `kGateKneadClipFormFloorPm = 50.0`** sit
  **below the measured worst non-exempt clip** (slot 19 at 12.44 / 68.2) with
  margin. They hold a reaction to being *present*; they choose nobody's size.
  The art values (`kKneadDipClipPm`, `kFoldDipRollA16`) are untouched by the gate.
* `kGateKneadHostingClips = 19` is a structural count. The control constants
  (590, slots 18/5) are control-only and never ship.

### Does any gate default sample a subset of the bank?

* **R6's census walks all 24 clips** (`dot census over 24 clips`), and the new
  far leg sits inside that same walk. ✓
* **R7 floors all 19 hosting clips**, prints each one's floor and verdict, and
  asserts the count. ✓
* **R3's flag census does** — `for (int want : {1, 0})`, slots 1 and 0 only.
  Pre-existing, correctly declared as a carried-forward open item, and **no
  longer the sole authority**, since R6 now covers the whole line population.
  Not a pass-23 regression.

### One framing correction

The report's exposure table reads `--fail-no-dip` as the per-clip floor catching
17 lost clips. It is **the hosting-count leg** that fires: those clips leave the
hosting list rather than falling under the floor (`0 clip(s) UNDER it` +
`FAIL R7 HOSTING: 2 clips, expected 19`). The *source comment* gets this exactly
right — it is why the count leg exists — but the summary row blurs it. Recorded
so nobody later cites the wrong leg.

---

## 4. Byte-identity — re-run, 4/4 on both rungs

My own renderer, witnesses hover / inspect / drift / hasty:

| configuration | hover | inspect | drift | hasty |
|---|---|---|---|---|
| exact-off (`1:730,14:635,17:635,18:590`) | 0xEFCCD8FA | 0x6B1077D0 | **0xB6AB88AA** | 0x6B85677A |
| **= pass 22** | ✓ | ✓ | ✓ | ✓ |
| exact-off + pass-22 knobs | 0x200AA3E7 | 0x1CFE8375 | **0xFB17B7D6** | 0x425AA389 |
| **= pass 21** | ✓ | ✓ | ✓ | ✓ |
| **p23 shipping** | 0xEFCCD8FA | 0x6B1077D0 | **0xF376C81F** | 0x6B85677A |
| `e-press-live` positive control | 0x200AA3E7 | 0x1CFE8375 | **0xD243EDE4** | 0x425AA389 |

* **Exact-off reproduces pass 22 on 4/4 and pass 21 on 4/4.**
* **Only the changed clip moves.** Hover, inspect and hasty are bit-for-bit
  pass-22; drift is the one witness this pass touches.
* The new rung is **not vacuous**: with the pass-23 depths live under pass-21's
  knobs, drift is `0xD243EDE4` ≠ pass-21's `0xFB17B7D6`.
* **Blown** renders `0x16532469`, matching the report; slot 20 is untouched, so
  Blown is byte-identical to pass 22.

---

## 5. Item 5 — the purge

The repaired `zencrifice_root()` finds the root **by name**. A bare dry run from
this working copy now reports `C:\programmieren\zencrifice` and **10,992 files /
2.83 GB** of real candidates, where before the repair it reported *"nothing to
do"*. The tool remains dry-run until `--apply`. The remaining 2.83 GB is the
sibling creature working directory, correctly declared and left alone as out of
Direction 24's named scope.

---

## 6. Findings — two record errors, corrected

Neither changes a shipped value. Both are corrected in `P23-IMPLEMENTATION.md`
by this review, with the correction marked.

**R1 — the post-change R5 B-lowest margins in the report are wrong.**
The report gives *"+78→+80 (drift), +94→+167 (damage), +115→+171 (death-drop),
+93→+145 (gutter)"*. The **before** values are right; the **after** values are
not. From the committed receipt and from my own identical run:

| clip | report says | **actually** |
|---|---|---|
| 1 drift | +80 | **+79** |
| 14 damage | +167 | **+159** |
| 17 death-drop | +171 | **+167** |
| 18 death-gutter | +145 | **+163** |

The conclusion the report draws from them — *"all inside the pass-20 authoring
target of 70–180 mm"* — **remains true of the correct numbers**. The figures
appear to have been carried over from ladder rungs rather than re-read from the
shipping run. This is the stale-number family: a figure that looks like a
measurement and was not re-taken after the value settled.

**R2 — `P23-LOOKS/02`'s gutter panel does not show what shipped.** It is
labelled, and rendered, **`590->680`**, while **720** shipped. Four clips are
presented as the before/after evidence at the dip bottom and one of them is a
ladder rung. The value *was* looked at — `P23-LOOKS/04` contains 590/680/720/760
side by side — so this is stale evidence, not an unexamined value. I did not
overwrite the implementer's plate; I rendered the shipped comparison myself and
committed it as `P23-REVIEW-LOOKS/01`–`02`, and the caption is corrected in the
report.

**R3 — minor.** `P23-NOTES/FINDINGS-01` quotes damage at `4.49 -> 32.99` where
the shipped measurement is **27.82**, and gives `~24` / `~33` for death-drop and
gutter against measured 22.31 / 38.77. FINDINGS-01 is explicitly the by-eye
record written *before* the constants settled, so approximate figures there are
defensible; the final report's numbers are the ones I verified. Noted only so a
later reader does not treat the note's numbers as measurements.

---

## 7. Open issues carried forward

1. **Blown still barely reacts**, by declared exemption. The shape of a future
   fix is a **per-clip ambient-duck share**, mirroring `kKneadDipClipPm`. My
   added finding: at Blown's framing the strand is ~25 px, so a future pass
   should ask whether the beat can read there *at all* before spending a
   mechanism on it.
2. **`dip_pm` runs at about a fifth of its declared range** bank-wide;
   `kFoldDipRefMm = 420 mm` is a reference no clip's pose approaches. Unchanged
   this pass, deliberately.
3. **R3's line-flag census samples slots 1 and 0 only.** No longer load-bearing.
4. **Two control masks moved** (`--fail-line-scale` 0x4→0x24, `--fail-no-dip`
   0x10→0x50). Both are the repair. I confirmed both.
5. **`kGateLineFarWidthPx` needs regenerating** if `kManaLineFullRadiusPx` or
   `kGateLineFarRadiusPx` move; two `static_assert`s make that a compile error.
6. **R5's B-strictly-lowest leg is soft** (§2). Not a defect — it is documented
   in source — but reports should not describe it as a hard bound.

---

## 8. Evidence

| file | what |
|---|---|
| `P23-REVIEW-RECEIPTS/ship-gate.txt` | my shipping gate run — **byte-identical to the implementer's committed receipt** |
| `P23-REVIEW-RECEIPTS/blown-ladder-reviewer.txt` | the nine-rung Blown ladder, with the gate verdict per rung |
| `P23-REVIEW-RECEIPTS/ctl-fail-*.txt` | the five controls I fired, each with its mask |
| `P23-REVIEW-RECEIPTS/p22bin-fail-*.txt` | the pass-22 binary's silence, on a binary I built |
| `P23-REVIEW-RECEIPTS/mspan.txt` | mspan PASS, G9 worsts unchanged to three decimals |
| `P23-REVIEW-RECEIPTS/gate-matrix-reviewer.txt` | the full matrix on my binaries |
| `P23-REVIEW-LOOKS/01`, `02` | death-gutter 590 / **720 shipped** / 760, mine, at 5x and native |
| `P23-REVIEW-LOOKS/03`, `04` | Blown 815 vs 500 at the worst frame, 6x and native |
