# Task Log: RUN-20260921-1811 - [Describe objective here]

**Created:** 2026-09-21 18:11 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260921-1811-manafold-pass23/

---

## Objective


Manafold pass 23 (small) from Owner Direction 24 ("Fix the still opens"): raise the press depth on the five clips where the knead reads faint so the lightning reacts visibly there; repair R6s LINE far-leg, which compares an expression against itself and cannot fire; make R7 floor per clip instead of on the bank maximum; purge the stale .rgb left in the sibling p12-final/render.

---

## Progress Timeline

### 2026-09-21 18:11 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260921-1811
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

### 2026-09-21 18:11 - Started
- Branches `manafold-pass23` created in both repos from the production-verified pass-22 mains (Zhaozhou `83002801`, Upheaval `d64fed9`).
- Owner direction recorded durably: `Upheaval/creature/Manafold/OWNER-DIRECTION-24-2026-09-21.md`. Scope is exactly pass 22's open list.
- One Opus implementer, then a bounded independent review that publishes on PASS. No Qwen (reserved); GPT/Codex quota exhausted until 2026-09-25.
- Note: items 2 and 3 are the same wrong-operand family as pass 20s five, pass 21s eight and pass 22s two. Every repaired gate must be fired, not assumed.

### 2026-09-21 ~22:55 - Orientation done, baseline in hand
- Read OWNER-DIRECTION-24, OWNER-DIRECTION-23, P22-IMPLEMENTATION.md, P22-REVIEW.md, the R6/R7 gate source and the dip chain.
- Built the unmodified pass-22 tree into `.tmp/p23base` (13 binaries, RC 0). `manafold-rear-audit --gate` runs in **0.86 s**, so the per-clip ladder is cheap; no fit, no ctest needed for items 1-3.
- Baseline R7 per-clip table captured: `P23-RECEIPTS/base-mrear-gate.txt`. The five faint slots confirmed: 18 (1.70 deg / 7.6 pm), 20 (3.70 / 24.7), 14 (4.49 / 23.9), 17 (4.76 / 24.3), 1 (7.10 / 36.2). Bank best 43.00 deg / 239.5 pm (slot 5).
- The chain, read end to end: `kKneadDipClipPm[slot]` -> `dip_gain` -> dent depth -> posed **sag** (mid_y(A,C) - B_y) -> `dip_pm = smoothstep((sag-90)/330) * 650/1000` -> `dip_shape_pm = min(1000, dip_pm*1000/150)` -> roll/tumble/shear. So the press depth IS the lever, confirmed structurally, and the reaction SATURATES at dip_pm 150.
- PURGE DONE (item 5) and it exposed a tool defect -- see P23-IMPLEMENTATION.md item 4.
- Plan: add a committed per-clip authoring ladder knob (`ZHAO_U02_KNEAD_DIP_CLIP_PM`) so the five values can be laddered in one build and so R7's new per-clip leg gets a PRODUCTION-knob control rather than a gate-local mutation; then item 2, item 3, then the eye pass.

### 2026-09-21 ~23:55 - All four items DONE

**Item 1 (press depth).** Added the missing authoring ladder first
(`ZHAO_U02_KNEAD_DIP_CLIP_PM`, one production read via `knead_dip_clip_pm`,
10 new RC-2 selector legs, and a refusal to make a non-hosting clip host a dip),
then laddered and chose by eye at native and 4x through the dip:
**drift 730->900, damage 635->780, death-drop 635->740, death-gutter 590->720.**
R7 rot 7.10->28.18, 4.49->27.82, 4.76->22.31, 1.70->38.77. Gutter backed off from
760 because the arm read as collapsing rather than kneading.
**Blown (slot 20) left at 815 and declared** -- its lever is INVERTED (deeper
press, smaller sag; confirmed by R7 and by peak `dip_pm` off the reel's own
trace), and the first readable setting breaches R5's B-lowest floor by 152 mm.
**B strictly lowest re-measured: 19/19, worst margin 29 mm (slot 9), unchanged.**
mspan PASS, G9 worsts unchanged to three decimals.

**Item 2 (R6 LINE far leg).** Tautology replaced by a STORED pass-19 table
(`kGateLineFarWidthPx[0..48]`, integers, guarded by two static_asserts) plus the
near evaluation. New control `--fail-line-far` (0x20 alone). **Proved the old
leg's blindness side by side:** the pass-22 binary under `--fail-line-scale`
printed "non-dot gained distance 0" and "R6 DOT: 0 violations" over 11,561,258
line splats; the pass-23 binary reports 11,561,258 off the stored law.

**Item 3 (R7 per-clip floor).** Per-clip floors 10.0 deg / 50.0 pm on every
hosting clip, bank legs kept, one DECLARED exemption (slot 20, still floored at
2.0/12.0), and the hosting COUNT asserted at 19 to close the hole a per-clip
floor opens. Controls `--fail-knead-clip` and `--fail-knead-drop`, both 0x40
alone, both driven by the production knob. **Exposed on an existing control:**
`--fail-no-dip` made pass 22's R7 read green while 17 of 19 clips had lost the
reaction entirely.

**Item 5 (purge).** 2.83 GB / 10,988 `.rgb` reclaimed from `p12-final/render`.
A bare run had reported "nothing to do" -- the default root resolved to
`manafold-p16` rather than the zencrifice root; repaired to find it by NAME.
2.83 GB more found in a sibling creature dir, declared and left (out of scope).

**Matrix 195/195 PASS, 0 FAIL, one invocation.** Exact-off reproduces pass 22
4/4 and pass 21 4/4; hover/inspect/hasty byte-identical file by file.

Write-up: `P23-IMPLEMENTATION.md`. Notes: `P23-NOTES/FINDINGS-01`, `-02`.
Plates: `P23-LOOKS/01`-`15`. Receipts: `P23-RECEIPTS/`.

**Next:** commit source, then curated evidence, push `manafold-pass23`. The
coordinator sends the review/publish packet; no bank render, encode, merge or
deploy from here.
