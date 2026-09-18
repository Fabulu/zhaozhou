# Manafold Pass 17 structural integration audit

**Date:** 2026-09-18
**Branch:** `manafold-pass17`
**Audited base:** `57650352` = `origin/manafold-pass17`
**Scope:** complete pending structural/continuity packet; no source edits, commits or pushes

## Verdict

**PASS after strict selector parsing and evidence-metadata cleanup.**

The accumulated source is internally dependent and must be committed atomically: 23-bone signed spans, outline ownership, eye-size consumption, endpoint gains, Fall/Q2/Q3 controls and renderer diagnostics share `manafold_art.h`, `manafold_clips.h` and `zhao_reel.cpp`. A partial source commit would not be a trustworthy buildable checkpoint.

The original audit found four numeric Trick/Taunt-III selectors silently accepting malformed strings as legal zero. The repaired renderer uses full-string, overflow-safe signed parsing. A clean renderer build plus 37-case matrix passes: unset and signed min/max/explicit-plus/zero values return RC 0; malformed, trailing junk, integer overflow, out-of-range and leading-space values return RC 2 for all four selectors. Fresh `mqa` and `mspan` both return RC 0, and `git diff --check` is clean.

## Verified focused matrix

Fresh caller-owned direct output:

```text
out/p17-structural-integration-audit/
```

All builds used `tools/env/zhao-env.ps1` and the exact Git Bash direct builder. No CMake/Ninja regeneration was used for this matrix.

| Check | Result |
|---|---:|
| `mspan` normal | RC 0 |
| `mspan` rigid/clamp/drift/overcompact/stage/posed mutants | **20/20 nonzero** |
| Shipping posed ring walk | 307,020 steps; 0 reversed; 0 pinched |
| Worst ring projection / separation | 6.891 / 7.442 mm |
| `moutline` normal / no-owner / stale-repaint | **0 / 1 / 1** |
| `mqa` normal | RC 0 |
| `mqa --eyes-only` | RC 0 |
| `mqa --fall-only` | RC 0 |
| `mqa --fail-fall-wrap` | attributed control RC 0 |
| `mqa --fail-eyesnap` | attributed control RC 0 |
| `mqa --fail-startle-step` | attributed control RC 0 |
| Final death-eye steps | Drop 1.89°, Gutter 2.57°; 8° ceiling |
| Startle root step | 217.6 mm; unchanged 260 mm ceiling |
| `mprobe` normal / broken-scale inverse / detached-outline | **0 / 1 / 1** |
| `mprobe --fail-mirror` | RC 1 |
| `meyesize` normal / L mute / R mute / wrong-bone | **0 / 0 / 0 / 0**, each control self-attributed |
| `mjointpub` normal + F/A/B/C/E mutes | **all RC 0**, each control self-attributed |
| `mnodule` normal + F/A/B/C/E mutes + ignore-all | **all RC 0**, each control self-attributed |
| `mmeshcheck` | RC 0, CLEAN |
| `mshell` normal / selftest / Pass-16 legacy / regression | **0 / 0 / 0 / 0** |
| Fresh integrated `cel` direct build | PASS |
| `git diff --check` | clean |

`PASS17-OUTLINE-REVIEW.md` independently configured and built the new `manafold-outlinegate` CMake target from PowerShell and reports RC 0. The direct and CMake registrations are both present.

## Findings

### 1. Resolved: malformed Trick/Taunt-III numeric selectors silently became zero

**Files:** `tools/reel/zhao_reel.cpp:7864`, `:7869`, `:7874`, `:7879`

All four controls parse with `std::atoi` and then accept zero as in range:

- `ZHAO_U02_TRICK_FLIP_X_A16`
- `ZHAO_U02_TRICK_FLIP_Z_A16`
- `ZHAO_U02_TAUNT3_FLICK_YAW_A16`
- `ZHAO_U02_TAUNT3_FLICK_ROLL_A16`

Concrete reproductions from the freshly built renderer:

```text
ZHAO_U02_TRICK_FLIP_X_A16=abc              -> RC 0, rendered 4-frame manafold-still
ZHAO_U02_TAUNT3_FLICK_YAW_A16=not-a-number -> RC 0, rendered 4-frame manafold-still
```

This can silently turn a misspelled rung into the legal `0` candidate and contaminate the upcoming same-binary Trick/punchline evidence. Use a strict full-string signed integer parser with overflow/range rejection; malformed, trailing-garbage and empty values must return RC 2. Add positive CLI checks before running either art ladder.

This did not change unset production behavior, but blocked committing the diagnostic packet as evidence-ready. It is now resolved by `parse_strict_env_int`; the 37-case clean-build matrix described above covers every selector and failure class.

### 2. Resolved: evidence metadata consistency

- `TASK_LOG.md:114` now records the final normalized Q2 readings `1.89° / 2.57°`.
- `PASS17-REMAINDER-CHECKPOINT.md` now records Q2/Q3 as closed and points to the reviewed repair evidence.
- `PASS17-SIGNED-SPAN-IMPLEMENTATION.md` and `PASS17-INTEGRATED-EVIDENCE.md` retain their historical pre-Q2 binary results and explicitly point to the later Q2/Q3 repair/review as the superseding omnibus status.

These were report-integrity issues, not production defects, and are now closed.

## Source commit manifest

Commit the following as **one atomic source/gate checkpoint** after fixing strict numeric parsing:

```text
tools/CMakeLists.txt
tools/reel/build-direct.sh
tools/reel/manafold_art.h
tools/reel/manafold_clips.h
tools/reel/manafold_eyesize.cpp
tools/reel/manafold_model.h
tools/reel/manafold_outline.h
tools/reel/manafold_outlinegate.cpp
tools/reel/manafold_probe.cpp
tools/reel/manafold_public_jointgate.cpp
tools/reel/manafold_qa_p12.cpp
tools/reel/manafold_rig.h
tools/reel/manafold_spangate.cpp
tools/reel/zhao_reel.cpp
```

Why atomic:

- `zhao_reel.cpp` references controls declared in `manafold_art.h` / `manafold_clips.h` and includes the new outline helper.
- `manafold_qa_p12.cpp`, `manafold_eyesize.cpp`, `manafold_public_jointgate.cpp` and `manafold_spangate.cpp` compile against the new 23-bone bank and authored tracks.
- `manafold_model.h` skin ownership and `manafold_rig.h` IDs are inseparable.
- Both build registrations are needed for a clean checkout to reproduce `moutline`.

Suggested source checkpoint intent: **Implement Manafold Pass 17 structural and continuity repairs**. This is a feature-branch checkpoint, not final Pass-17 art acceptance, merge or publication.

## Evidence commit manifest

After correcting the three documentation inconsistencies above, make a separate evidence/planning commit containing:

### Reports and plans

```text
runs/CLAUDE-RUNS/RUN-20260917-2318-manafold-pass16-pass17/TASK_LOG.md
PASS17-ENDPOINT-AUTHORITY.md
PASS17-EYE-EXPRESSION-REVIEW-PLAN.md
PASS17-FALL-REVIEW.md
PASS17-INTEGRATED-EVIDENCE.md
PASS17-MQA-Q2-Q3-TRIAGE.md
PASS17-OUTLINE-AUDIT.md
PASS17-OUTLINE-IMPLEMENTATION.md
PASS17-OUTLINE-REVIEW.md
PASS17-PUBLIC-ORDERING-PLAN.md
PASS17-Q2-Q3-REPAIR-PLAN.md
PASS17-Q2-Q3-REPAIR.md
PASS17-Q2-Q3-REVIEW.md
PASS17-REMAINDER-CHECKPOINT.md              # only after refresh
PASS17-SIGNED-SPAN-IMPLEMENTATION.md
PASS17-SPAN-FALL-CODE-REVIEW.md
PASS17-SPAN-TARGET-VISUAL-REVIEW.md
PASS17-STRETCH-ARCHITECTURE.md
PASS17-TAUNT3-PUNCHLINE-PLAN.md
PASS17-TRICK-AXIS-RENDER-PLAN.md
PASS17-STRUCTURAL-INTEGRATION-AUDIT.md
```

(All `PASS17-*` paths above are under the same run folder.)

### Durable accepted/current evidence

Include these exact families:

```text
PASS17-ENDPOINT-AUTHORITY-LADDER.png
PASS17-ENDPOINT-AUTHORITY-4X.png
PASS17-FALL-HOLD-VS-WRAP.png
PASS17-OUTLINE-FINAL23-*.png
PASS17-SPAN-CEND-FINAL23-*.png
PASS17-Q2-DEATH-*.png
PASS17-Q3-STARTLE-LATE-*.png
PASS17-Q3-STARTLE-LEGACY-ALLFRAMES.png
PASS17-SPAN-NUDULE-ALLFRAMES.png
PASS17-SPAN-NUDULE-VERTICAL-2X.png
PASS17-SPAN-TAUNT-*.png
PASS17-SPAN-TAUNT2-*.png
PASS17-SPAN-TAUNT3-*.png
PASS17-SPAN-COLLAPSE-WITNESSES-*.png
PASS17-SPAN-COLLAPSE-*-WINDOW-5COL.png
```

The collapse and current Taunt-family sheets are not final art acceptance; they are durable before/failure evidence explicitly referenced by the visual review and next-wave plans.

At audit time this keep set is about **20.0 MB across 59 pending `PASS17-*` files**, before this audit report. Review the staged list by name rather than using an unbounded run-folder glob.

## Explicit exclusions

Do **not** stage:

```text
.tmp/**
PASS17-SPAN-CEND-REPAIR-PROVISIONAL-*.png
PASS17-OUTLINE-INTEGRATED-*.png
PASS17-OUTLINE-OPENING-AB-*.png
PASS17-OUTLINE-EFFECT-AB-*.png
PASS17-SPAN-NUDULE-WITNESSES.png
PASS17-SPAN-NUDULE-WITNESSES-4X.png
PASS17-SPAN-COLLAPSE-*-WINDOW.png          # duplicate non-5COL windows
```

Reasons:

- `.tmp/` is ignored-build/render scratch and is not durable provenance.
- `PROVISIONAL` and pre-final23 outline files are superseded 22-bone evidence.
- The first nodule witness pair sampled lateral/rest frames; `PASS17-SPAN-NUDULE-VERTICAL-2X.png` is the corrected evidence.
- The non-5COL collapse windows duplicate the compact self-describing sheets cited by the review.

Do not add `.tmp/` to `.gitignore` as a substitute for cleanup. After the accepted evidence is committed, use the repository purge tool under its dry-run/newest/48-hour safeguards.

## Scope and safety review

- No credential, network endpoint, deployment token, destructive action or production-publish change is present.
- No generic runtime/RTL semantic changed in this pending packet; the 23-bone work remains within the existing 32-bone/two-weight/local-translation format, so no Quartus fit belongs to this checkpoint.
- The full Pass-17 bank, final media encode, main integration and deployment remain correctly blocked on public A/B/C ordering, Trick selection, Taunt-III punchline and native eye-expression acceptance.

## Acceptance sentence

The structural/continuity source is mechanically green, strict selector parsing and evidence metadata are repaired, and the checkpoint is ready: commit source atomically, commit the curated evidence separately, push both immediately, and continue the planned art waves without citing stale/provisional files.
