# Manafold Pass 17 — Q2/Q3 independent review

**Date:** 2026-09-18
**Scope:** death-eye settle continuity and Startle root-step repair
**Reviewed report:** `PASS17-Q2-Q3-REPAIR.md`

## Verdict

**PASS after two checker fixes.** The production repairs are narrow and visually sound. Death tails retain only the fixed-camera EyeTravel base; they do not call the full knead path or revive ambient nodules/hinges. Startle preserves its full displacement, held payoff, eye-size arrival and recovery while restoring useful headroom under the unchanged 260 mm gate.

## Review findings and fixes

### 1. Q2 relative-angle reader did not normalize its operands — fixed

The new reader took `abs(dot(a,b))` in nominal Q1.14 units but did not divide by the actual quaternion norms. `quat_mul` rounds component products and does not renormalize, so a transpose-style fixed-scale dot can report a nonzero relative angle even for identical non-unit quaternions. The reader now computes:

```text
abs(dot(a,b)) / sqrt(dot(a,a) * dot(b,b))
```

before `acos`, with an explicit zero-norm failure. This preserves sign invariance and detects equal-magnitude axis/direction reversals without measuring norm drift as motion.

Fresh normal results changed only by the expected measurement correction:

- Death Drop worst normalized step: **1.89°**;
- Death Gutter worst normalized step: **2.57°**;
- unchanged ceiling: **8°/key**.

### 2. Mutant attribution admitted unrelated failures — fixed

`--fail-eyesnap` originally required two named settle catches but did not require exactly two total Q2 failures. `--fail-startle-step` required Startle to exceed its ceiling but did not require exactly one total Q3 failure. Either control could therefore remain green while an unrelated fault certified the run.

The controls now require:

- eye reset: exactly 2/2 named settle snaps **and exactly two total Q2 failures**;
- Startle legacy timing: slot 4 over its bound **and exactly one total Q3 failure**.

Both controls still fire solely for their named defects.

## Source review

- `apply_eye_schedule` mutates only EyeTravel carriers and `eye_lean`.
- Dead frames call it with `eye_pm=0`, retaining the fixed-camera base while zeroing glance/lean.
- Full `antenna_knead` remains skipped after settle, so ambient hinge/nodule life cannot restart.
- Explicit death droop/slack/contact/corpse deformation remain unchanged.
- CLI controls set globals before the function-local static `u02::type()` bank is constructed.
- Startle's shipping tuple is `8 -> 12 -> 22`; the selected eye-size curve reaches 1350 pm on the same arrival key.
- Legacy `8 -> 11 -> 21` restores the prior root discontinuity without changing the existing 260 mm threshold.
- Renderer selectors validate `current|legacy` and `late|legacy|early`; unset remains the shipping default.
- Explicit `current` + `late` renders are byte-identical to unset across all 1,200 Death Drop, Death Gutter and Startle frames (450/590/160).

## Fresh verification

Unique direct build: `.tmp/p17-q2q3-review`

| Check | Result |
|---|---:|
| `mqa` normal | RC 0 |
| `mqa --eyes-only` | RC 0 |
| `mqa --fail-eyesnap` | RC 0 after Q2 reports exactly 2 named failures |
| `mqa --fail-startle-step` | RC 0 after Q3 reports exactly 1 Startle failure |
| `mprobe` | RC 0 |
| `meyesize` | RC 0 |

Shipping Startle's worst root step is **217.6 mm**, leaving **42.4 mm** under the unchanged ceiling. The legacy control reaches **290.0 mm** and is rejected.

## Visual review

Reviewed all presentation frames in:

- `PASS17-Q2-DEATH-DROP-ALLFRAMES.png`;
- `PASS17-Q2-DEATH-GUTTER-ALLFRAMES.png`;
- `PASS17-Q3-STARTLE-LATE-ALLFRAMES.png`.

Reviewed native and exact 4× A/B evidence:

- `PASS17-Q2-DEATH-SETTLE-AB-NATIVE.png` / `...-4X.png`;
- `PASS17-Q3-STARTLE-LATE-LEGACY-NATIVE.png` / `...-4X.png`.

The death settles remain grounded and visually continuous; the legacy controls visibly reposition the face at the settle boundary. Startle retains the compressed anticipation, sharp rise, clearly enlarged eyes and held extreme, then rejoins the unchanged recovery. No contact, span, outline or framing regression is visible.

## Final review result

No remaining blocking finding in this packet. `git diff --check` is clean after the checker fixes.

As I always say, a compass must measure direction—not how hard you are squeezing it!
