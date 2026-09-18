# Manafold Pass 17 — `mqa` Q2/Q3 triage

**Date:** 2026-09-18
**Scope:** read-only provenance and attribution review; no source edits or builds

## Verdict

The omnibus `mqa` failures are **pre-existing at the accepted Pass-16 baseline, but they are real animation defects—not stale thresholds and not broken instruments**.

- **Q2:** death-drop and death-gutter each snap the eye-travel carrier by about 32 degrees at the settle key.
- **Q3:** Startle moves its root 290.0 mm in one authored key against its declared 260 mm ceiling.
- **Q5 Fall:** independently attributable and green despite those failures. Its normal and mutant exit paths judge Q5's own counter only.

Pass 16 explicitly carried these as historical non-scope diagnostics (`RUN-20260916-1838-manafold-pass16/subagents/20260916-2108-manafold-p16-qa/FINDINGS.md:32-34`). Pass 17 should not repeat that waiver: it is the expression/clip repair pass and remains unfinished until these visible discontinuities are repaired and looked at.

## Exact current failures

The integrated normal log is `.tmp/p17-integrated-evidence/logs/mqa-normal.log`.

### Q2 — death-eye snaps

The reader's positive control succeeds first:

- `apply_eye_travel(rig, 1000)` reads 45.00° (`mqa-normal.log:19-21`), so the instrument demonstrably sees the production channel.
- Death-drop slot 17: worst step **31.80° at key 117**, the settle key (`mqa-normal.log:39`).
- Death-gutter slot 18: worst step **31.86° at key 191**, the settle key (`mqa-normal.log:40`).
- The ordinary moving bank stays at or below 6.17°/key, leaving a wide separation from the fixed 8° snap criterion (`manafold_qa_p12.cpp:265-287`).

This is exactly the fault the production comments claim was repaired. Both builders skip `antenna_knead` once `dead` becomes true (`manafold_clips.h:3396-3414`, `3569-3587`). The dying-side `eye_pm` fades only glance/drift, while the fixed-camera face base is deliberately not multiplied by that gain (`manafold_clips.h:1913-1953`). Consequently the last living key still carries the roughly 32° camera-relative base, then the first dead key resets the carrier to identity because `g.reset()` runs and `antenna_knead` is skipped. The comments at `manafold_clips.h:3405-3414` describe the old issue but overlook the later Pass-15 base term.

**Classification:** real regression/debt. The Q2 metric is not blind, and relaxing its threshold would be wrong.

**Pass-17 causality:** none of the current scale/span/outline edits creates it. Eye scale changes Eye/Pupil bases, not `kBEyeTravelL/R` quaternions; span helpers are appended after stable existing IDs; outline is post-render composition. The death builder's eye call is unchanged from accepted Pass 16. Pass-16 QA already named both rows as historical (`FINDINGS.md:34`).

**Bounded repair:** keep calling `antenna_knead` through the dead tail with `eye_pm=0`, so the static fixed-camera base is held while glance/drift/lean stay dead; preserve `g_u02_death_fail == 5` as the old skip/snap mutation or add an equivalent attributed leg. Re-render both settle windows at native resolution and require Q2 normal green plus the snap mutant red. Do not fade the base toward identity merely to satisfy the number—the corpse should hold a readable face, not rotate 32° while dying.

### Q3 — Startle root ceiling

- Slot 4 Startle reports **290.0 mm at key 9 → 10** against its declared **260 mm** ceiling (`mqa-normal.log:57-58`).
- The gate computes direct authored-key XYZ root displacement (`manafold_qa_p12.cpp:333-378`); eye scale, bone count, skinning and outline composition cannot affect that value.
- Accepted Pass-16 `build_startle()` has the same root path: `kBack` goes from `+140 pm` at key 8 to `-1300 pm` at key 11 and drives `kStartleJumpMm` directly. Pass-17 adds eye scale in that builder but does not change root (`git diff 77ccfc99 -- manafold_clips.h`).

Provenance explains the mismatch:

- Q3's per-clip ceiling table arrived in Pass 13 commit `1e9aa9a3`.
- Startle's sharper three-key attack/held arrival arrived later in Pass 14 commit `60607c05`.
- Pass-16 QA recorded Startle's old root-step ceiling among pre-existing failures (`FINDINGS.md:34`).

**Classification:** real later-animation change that violated an older still-valid continuity budget. The 30 mm breach is not rounding. Raising 260 to 290/300 would fit a gate to its own answer, directly contradicting the source's headroom rule (`manafold_qa_p12.cpp:299-332`).

**Bounded repair:** render/look at a minimal Startle ladder that preserves the hard recoil and 20-frame held arrival while removing the single-key 290 mm pop—for example timing the attack over one additional authored key versus a modest authored displacement alternative. Choose by native every-frame read, not by solving backward from 260. Then require Q3 normal green with useful headroom and retain `--fail-rootstep` as Q3's independent positive control.

## Why Fall Q5 remains valid

Q5 is not being certified by Q2/Q3's unrelated red rows.

- `--fall-only` returns solely from `q5_fails` (`manafold_qa_p12.cpp:574-576`).
- `--fail-fall-wrap` selects `g_u02_fall_wrap_control` before the first static `u02::type()` construction (`manafold_qa_p12.cpp:79-86`) and returns success only after **Q5 itself** reports at least one failed Fall contract/channel (`558-571`).
- Q5 checks flags, resolved root, all bone quaternions, local translations, optional uniform scale, primary deform and every extra lane (`440-539`).
- Integrated evidence records `--fall-only` RC 0 and the legacy-wrap control firing (`PASS17-INTEGRATED-EVIDENCE.md:7-18,35-37`).

Static bank caching therefore does not invalidate the Fall control: every CLI leg is a separate process and sets the selector before bank construction. The omnibus normal RC 1 should never be quoted as a Fall result; the isolated Q5 paths are the valid evidence.

## Recommended bounded action

1. Repair both death settle-key eye-base snaps; render and inspect the two settle windows; run Q2 normal and its own mutation.
2. Re-author Startle's attack with a tiny same-binary timing/amplitude ladder; preserve the held payoff; inspect every frame; run Q3 and `--fail-rootstep`.
3. Keep `mqa --fall-only` / `--fail-fall-wrap` as the Fall acceptance commands.
4. Only then require omnibus `mqa` RC 0. Calling Q2/Q3 “pre-existing” establishes provenance; it does not make them acceptable for a finished Pass 17.

## Resolution — 2026-09-18

Both debts are repaired and visually reviewed in `PASS17-Q2-Q3-REPAIR.md`.

- Death tails retain only the fixed-camera EyeTravel base; Q2 now uses normalized relative quaternion distance. Independent final review (`PASS17-Q2-Q3-REVIEW.md`) records normal worst steps of 1.89°/2.57°, while `--fail-eyesnap` restores and catches 44.96°/59.90° settle resets.
- Shipping Startle uses the selected 8→12→22 timing with synchronized eye-size arrival. Its worst root step is 217.6 mm under the unchanged 260 mm ceiling; `--fail-startle-step` restores legacy 290.0 mm and Q3 rejects it.
- Omnibus `mqa` now returns RC 0. Every-frame native and 4× evidence passes for both repairs.

As I always say, a warning light that was already glowing when the bus left the depot is still a warning light!
