# Manafold pass 22 — independent review

**Date:** 2026-09-21
**Reviewer:** Claude (independent; no sub-agents, no Qwen/HomeAI)
**Under review:** Zhaozhou `manafold-pass22` @ `11290542`
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-23-2026-09-21.md`

## VERDICT: **PASS** — clear to publish.

Both owner items are delivered, the near read is preserved, and every claim
I could check independently held. Two minor instrument findings are recorded
below; neither blocks, and neither changes a shipped value.

Everything below was rebuilt and re-run by me. I did not take a number from
`P22-IMPLEMENTATION.md` without reproducing it.

---

## 0. What I built and ran myself

| thing | how |
|---|---|
| pass-22 renderer + `manafold-rear-audit` | `build-direct.sh --output .tmp/p22rev --clean`, RC 0 |
| **pass-21 renderer** | fresh `git worktree` at `b9de3059`, built the same way, RC 0 |
| renders | `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`, four witness clips, four configurations, 1,740 frames each |

Building the pass-21 binary matters: it lets me compare **bytes against a real
baseline** instead of comparing a CRC against a CRC someone else printed.

---

## 1. The owner's near read (Hover) — **PRESERVED. Not a fault.**

This was the coordinator's headline risk, and the implementer was right to
declare it: Hover is **not** byte-exact. My own numbers, isolating the dot law
(`FOLD_DIP_SHAPE_PM=0`, so the knead cannot contaminate the measurement):

> Hover, dot law alone vs pass 21: **180 of 600 frames changed**, worst frame
> **3,270 px** of 92,160 (3.5%), max channel delta 90.

So the bytes move. **The read does not.** Sampling by badness rather than by
index, I took the single worst frame (f0526) and looked at it:

* **At native (`A1`): indistinguishable.** Side by side I cannot tell which is
  which. The mote cloud has the same extent, the same count, the same
  brightness, the same colour.
* **At 5× (`A2`): the motes are very slightly tighter** — marginally smaller
  soft halos, marginally more defined cores. The green pocket is unmoved and
  unchanged in hue; the white strand is identical in size and shape.

The arithmetic agrees with the eye. At Hover's nearest-to-far radius (285 px)
the law takes the four authored dot sizes `16→14, 11→9, 10→9, 7→6` — a 10–18%
radius trim on a soft, bloomed disc, on the worst frames of the clip only. At
Drift (128 px) the same law takes them `16→9, 7→4`, which is the fix the owner
asked for. **The close-up point is doing exactly what it was put there to do**;
Hover simply is not fully close-up, and the residual is below the read.

**No fix required. Not BLOCKED.** I would, however, keep the implementer's open
issue 3 exactly as written: *Inspect* is the byte-exact near clip and Hover must
never be quoted as one.

## 2. The distant read (Drift, Hasty) — **PASS, and 600 is the right number.**

Judged at native, then magnified, from my own renders.

**Drift f0289** (`B1`, `B2`): before, the motes are fat pale fog-balls that
crowd the antenna head, smother the green pocket into a smear and blur the white
strand. After, they are distinctly smaller and **separate**; the green pocket
reads as green, the blue strand reads crisply, and the wanderers are still
round, soft, legible motes against both sky and ground. **Smaller, still dots.**

**Hasty f0012** (`B3`) is the stronger case: before, the antenna loop is packed
solid with pale balls. After, the loop is **open and readable**, the green sits
inside it as a distinct shape, and two white strands resolve where there was one
smudge.

**Is 600 right?** I did not take this on report — I rendered the rungs myself
(`C1`, `C2`, Drift, knead off):

| rung | my read |
|---|---|
| p21 (none) | the complaint: fused fog, green smothered |
| **600 (shipped)** | **dots separate, green and blue both read, field keeps its presence** |
| 800 | thinning; the outer wanderers start to read as specks |
| 1000 (the ink's full depth) | **dust — the mana field is essentially gone** |

**The implementer's most load-bearing judgement call — that the owner's named
model at full depth deletes the effect — is CONFIRMED by my own eye.** 1000 is
plainly wrong; 800 is already borderline. 600 sits at the safe end of a narrow
window and keeps the effect alive. **Not too strong, not too weak.** I would
accept anything in 600–700 and I would not move it.

This is the art law applied correctly: the ink's **law** is the owner's, and the
**value** was still chosen by looking.

## 3. Lightning size invariance — **VERIFIED, by three independent routes.**

**(a) Structural, re-derived by me.** `ManaSplat::dot` is
* declared once (`manafold_fx.h:1856`),
* **written in exactly one place** — `mote_push` (`manafold_fx.h:2123`), whose
  only two call sites are the fold-mote halo and core pushes
  (`manafold_fx.h:4081`, `:4084`),
* **read in exactly one place** in the production path —
  `zhao_reel.cpp:3193`, inside `u02_splat_r_px`, which returns on `ms.line`
  **before** it ever asks about `ms.dot`.

The implementer's claim is accurate as stated.

**(b) The census, re-run by me** — byte-for-byte the same figures:

> 24 clips, **691,182 dot / 11,561,258 line / 164,900 plain** splats;
> violations: both-flags 0, dot moved near 0, dot under floor 0,
> **non-dot gained distance 0**.

**(c) Visual, and this is the one not wired to the code's own operands.** Across
the four dot strengths in `C1` (0/600/800/1000) the dots shrink dramatically
while the blue-and-white strand keeps **exactly** its width and shape. A
distance term leaking into the lightning would have moved with that knob. It
does not.

Also confirmed from the diff: `mana_line_r_px` and every `kManaLine*` constant
are **untouched** — pass 19's law is bit-for-bit what it was.

## 4. The knead reaction — **PASS on the clips the direction names.**

Does the lightning visibly change form and rotate during the dip, at native?
**Yes, on both acceptance clips, and it is not subtle.**

* **Inspect f591** (`D1` native, `D2` 6×): the loop has rotated and re-formed —
  the right end swings up into a hook that reaches past the antenna, the left
  end curls. Same topology (still one closed loop), same white core, same navy
  backing, **same stroke width**. A form change, not a restyle.
* **Hover f529** (`D3`, knead off vs on): stronger still — the figure turns from
  a horizontal banana lying on the ball to a steep diagonal blade.

**In motion** (`E1`, f566→f596 before/after): the gesture arrives and leaves
gradually with **no pop**, and the topology holds at every step.

**Windows and seams, measured myself.** Inspect's reaction occupies four
separate press beats — 198–222, 243–281, 512–547, 569–595 — each ramping up from
a small value and back down. **Frames 0 and 599 are byte-identical to pass 21**,
so the loop seam is exact.

**Is the shipped reference of 150 right?** Yes for what was asked. The owner's
acceptance criterion is Inspect and Hover through the dip, and both reach the
reference and take the full gesture. The five shallow slots (18, 20, 14, 17, 1 —
1.7–7.1°) are clips whose top nodule barely presses; a proportional near-zero
reaction there is the *honest* behaviour, and the implementer is right that the
lever is that clip's own dip share, **not** the roll constant — raising the roll
would over-drive Inspect and Hover, which already read at the right strength.

**My judgement: do not move the lever in this pass.** Carry it as the open issue
it already is.

## 5. Gates

**Controls fired by me, from my own build** (`P22-REVIEW-RECEIPTS/`):

| control | RC | mask | what moved |
|---|---|---|---|
| `--fail-dot-scale` | 1 | **0x20** alone | dots shrunk at far 691,182 → **0** |
| `--fail-dot-flag` | 1 | **0x20** alone | dot population 691,182 → **0**; plain 164,900 → 856,082 |
| `--fail-knead-shape` | 1 | **0x40** alone | rot 43.00° → **0.00**, form 239.5 → **0.0** |
| shipping | 0 | 0x0 GREEN | R6 0 violations, R7 0 violations |

Each fires its own mask **and nothing else**. Confirmed.

**Subset sampling: neither gate's default samples a subset.** R6's census walks
`T.bank.clips` unconditionally — the output says "over 24 clips", the whole
bank. R7 walks the same list, skipping only clips whose `kKneadDipClipPm` is
zero or whose pose never clears the onset, and **declares the count it used**
("19 clips reach the press") with a per-clip line for every one.

**No bound relaxed.** The diff removes or changes **zero** `constexpr` lines
across all five source files. The ten deletions are exactly: two debug `printf`
lines, two `mana_push`→`mote_push` renames, the two 2-line renderer radius
declarations, and the two soft-tap `sx`/`sy` lines that now read the scaled
radius. I checked every one.

**Env bounds are strict in both directions**, tested by me:
`SCALE=bogus`, `STRENGTH_PM=1001`, `STRENGTH_PM=-1`, `FULL_PX=39`,
`FULL_PX=2001`, `SHAPE_PM=3001` all return **RC 2**; `STRENGTH_PM=1000` and
`SHAPE_PM=3000` render at **RC 0**.

**Exact-off reproduces pass 21 — and I checked it harder than 4/4 CRCs.**
I byte-compared every frame against the **freshly built pass-21 binary**:

| clip | frames | differing files |
|---|---|---|
| hover | 600 | **0** |
| inspect | 600 | **0** |
| drift | 300 | **0** |
| hasty | 240 | **0** |

**1,740 of 1,740 frames byte-identical.** And the identity legs are **not
vacuous** — my positive control is the shipping render, which differs on all
four (drift 300/300, hasty 240/240, hover 246/600, inspect 127/600). Drift and
Hasty are in the witness list, which is what makes the leg able to fail at all.

---

## 6. Findings (minor; neither blocks, neither changes a shipped value)

**F1 — R6's LINE far-leg is tautological and cannot fire.** In
`dot_census_failures`, the line branch asserts:

```cpp
if (far != u02::mana_line_r_px(ms.r_px, far_q8)) ++n_plain_moved;
```

but `far` *is* `gate_splat_r_px(ms, far_q8)`, which for a line returns exactly
`mana_line_r_px(ms.r_px, far_q8)`. The two sides of the comparison are the same
expression, so this is CLAUDE.md's "detector wired to two operands that move
together" — it is structurally blind.

**This does not weaken the shipped claim**, because the *load-bearing* checks
beside it are real: the line **near** leg (`near != ms.r_px`) genuinely asserts
pass 19's law is exact at ≥360 px, and the **plain** branch (164,900 splats, no
distance term at either witness) is a true assertion about every lightning body,
glow, bullet, boil and pulsar splat. But **"11,561,258 line splats and zero
moved" should not be quoted as if the far half of it were evidence.** The honest
form is: the plain population is checked, the line population is checked at near
and is structurally unreachable by the dot flag. Worth a one-line repair next
pass — compare the line against a *stored* pass-19 evaluation, or delete the
tautological line and say so.

**F2 — R7 gates on the bank MAXIMUM, not per clip.** `knead_shape_failures`
compares `best_rot` / `best_form` — the best over all hosting clips — against
the floors. If 18 of 19 clips regressed to zero and one held at 43°, R7 would
still pass. It asserts the reaction exists *somewhere* and is confined
*everywhere* (the out-of-window leg **is** per-clip and is a strong check). Given
open issue 1 already records a 25× spread across the bank, a future pass that
tunes per-clip dip shares should give this leg a per-clip floor, or the spread
can widen without the gate noticing.

**F3 (credit, not a fault).** The implementer's surface-fade fix is a genuine
catch: computing `r_px` **before** the fade so the footprint is sampled at the
radius actually drawn. Reading `ms.r_px` there while drawing scaled would have
faded a mote against an antenna it no longer touches. It is correctly proven
inert under the legacy law by the 1,740/1,740 byte result.

---

## 7. Evidence

| plate | what |
|---|---|
| `A1`, `A2` | **the Hover near read**, worst pure-dot frame, native and 5× |
| `B1`, `B2`, `B3` | the distant read, Drift and Hasty, native and 6× |
| `C1`, `C2` | **my own strength ladder** — 600 vs 800 vs 1000 vs none |
| `D1`, `D2`, `D3` | the knead at native and 6×, Inspect and Hover |
| `E1` | the knead **in motion**, before over after, f566→f596 |
| `P22-REVIEW-RECEIPTS/` | my shipping gate run and all three controls |
