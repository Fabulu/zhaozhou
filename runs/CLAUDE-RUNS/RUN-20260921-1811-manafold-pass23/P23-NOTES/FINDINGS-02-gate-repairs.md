# Findings 02 — the two gate repairs, and what firing them exposed

## Item 2 — R6's LINE far leg

**The defect, restated exactly.** `dot_census_failures` had

```cpp
const int32_t far = gate_splat_r_px(ms, far_q8);
...
if (far != u02::mana_line_r_px(ms.r_px, far_q8)) ++n_plain_moved;
```

and `gate_splat_r_px` returns, for a line, exactly `mana_line_r_px(ms.r_px,
far_q8)`. Both sides were the same expression, so the leg could only ever read
zero. CLAUDE.md's law: *ask what the two operands are clocked by* — here, one
function call clocked both.

**The repair: two operands, neither of which is that expression.**

1. `kGateLineFarWidthPx[0..48]` — pass 19's law evaluated once at the gate's
   128 px far witness under the 360 px full radius, and **written down as
   integers**. Indexed by the AUTHORED radius, so it pins the LAW and no art
   value: any authored radius may move freely inside 0..48. Two `static_assert`s
   refuse to compile if either witness moves, with the regeneration one-liner in
   the comment. A line splat whose radius escapes the table is itself a failure.
2. The near evaluation: `near == ms.r_px` is already pinned at >= 360 px, so
   `far < near` must hold wherever the arithmetic requires it
   (`kGateLineShrinkMinAuthoredPx = 4`, the threshold R3's own sweep uses).

Plus a population counter — `n_line_shrink_eligible` must be nonzero, so "0
violations" cannot be the empty set.

**Shipping:** 7,650,706 of 11,561,258 line splats eligible to shrink, **all
7,650,706 thinner at far**; 0 off the stored law, 0 untabulated.

### The control, and the SIDE-BY-SIDE PROOF that the old leg was blind

`--fail-line-scale` puts pass 19's law into legacy (constant-pixel lines) — the
fault the leg exists for. Run on **both binaries**:

| binary | line population | R6 verdict | mask |
|---|---|---|---|
| pass 22 (`p22bin-fail-line-scale.txt`) | "non-dot gained distance **0**" | **R6 DOT: 0 violations** | 0x4 |
| pass 23 (`ctl-fail-line-scale.txt`) | off the stored law **11,561,258**, not thinner **7,650,706** | **R6 DOT: 2 violations** | **0x24** |

Eleven and a half million line splats drawing at their close-up width at
Drift's distance, and pass 22's census called it green. That is the tautology
fired on a real, already-committed mutant — not an argument about it.

A second, isolated control, `--fail-line-far`, breaks the ROUTING instead (the
renderer stops scaling lines while the law is untouched): **mask 0x20 alone**,
thinner-at-far 7,650,706 -> **0**.

**Declared mask change: `r-line-scale` 0x4 -> 0x24.** The legacy law genuinely
breaks R3's law sweep AND R6's real-splat census. The matrix carries the new
expectation with the reason beside it.

## Item 3 — R7's per-clip floor

**The defect.** The floors were compared against `best_rot` / `best_form`, the
maxima over all 19 hosting clips. One loud clip covered the bank.

**The repair.** `kGateKneadClipRotFloorDeg = 10.0` / `kGateKneadClipFormFloorPm
= 50.0`, applied to **every hosting clip's own numbers**; nothing in that block
reads `best_*`. Set below the measured worst NON-EXEMPT clip (slot 19, lasso,
12.44 deg / 68.2 pm) with margin — the same construction the bank floors use, a
regression guard that chooses nobody's size. The bank legs are kept.

**One declared exemption, not a lowered floor.** Slot 20 (Blown) reacts at
3.70 deg / 24.7 pm and pass 23 could not raise it — see
FINDINGS-01-press-ladder.md; the lever is inverted there and the settings that
would make it read breach R5's B-lowest bound by up to 152 mm. It is named in
`kGateKneadExemptSlots` with the reason, and it is **still floored** at
2.0 deg / 12.0 pm, so it cannot quietly fall to zero. Excused from reading
plainly, not from reacting.

**A hole the repair opened, closed in the same pass.** A per-clip floor cannot
see a clip that leaves the list, and R7 skips any clip whose press depth is zero
or whose pose never clears the onset. So the hosting COUNT is now asserted:
`kGateKneadHostingClips = 19`, the number Direction 21 item 2 delivered.

### The controls, both driven by a PRODUCTION knob

* `--fail-knead-clip` — slot 18 put back to its **pass-22 press depth of 590**,
  through `ZHAO_U02_KNEAD_DIP_CLIP_PM`. Result: slot 18 reads 1.70 deg / 7.6 pm
  **UNDER**, 1 clip under the floor, **and the bank maxima print 43.00 deg /
  239.5 pm — green**. The control does not argue the old gate was blind; it
  reproduces the exact configuration pass 22 shipped and watches the new leg
  catch it. Mask **0x40 alone**.
* `--fail-knead-drop` — the loudest clip (slot 5) pressed to zero, so it leaves
  the list. 18 clips reach the press, best rot 40.89 deg, every surviving clip
  over its floor, **only the count leg fires**. Mask **0x40 alone**.

### And a third thing it exposed, on a control that already existed

`--fail-no-dip` switches the whole kneading dent off. Pass 22's R7 reported:

> `knead shape: 2 clips reach the press; best rot 36.03 deg ... ` **`R7 KNEAD: 0
> violations`**, mask 0x10.

**Seventeen of nineteen clips had lost their reaction entirely and R7 was
green**, because 2 survivors kept the bank maximum above 12 deg. Pass 23 goes
red on the same mutant (mask **0x50**). This is F2's failure mode fired on a
committed control, and it is the reason the mask expectation moves.

**A corroboration fell out of it.** With the dent switched off, **slot 20
reaches 36.03 deg** — the highest in that run. Blown's ambient pose alone
carries a deep sag; it is the dent's own ambient duck that removes it. That is
the Blown diagnosis confirmed from a completely different direction, by a
control written for another purpose.

## Subset check (CLAUDE.md, pass 21's 6-of-24 leg)

* R6's census: `T.bank.clips` unconditionally — **24 clips**, printed.
* R6's new far leg: inside that same whole-bank walk.
* R7's per-clip floor: every hosting clip, **19**, printed per clip with its
  floor and its verdict, and the count itself asserted.
* Carried forward, and still true of pass 19's R3 line census: it samples slots
  **1 and 0 only**. It is not repaired here (out of Direction 24's scope), but
  it is no longer the only assertion about the line population — R6's whole-bank
  census now makes the load-bearing claim. Recorded as an open item.
