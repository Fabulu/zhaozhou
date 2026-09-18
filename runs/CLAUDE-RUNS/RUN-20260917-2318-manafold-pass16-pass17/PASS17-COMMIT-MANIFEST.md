# Manafold Pass 17 structural checkpoint commit manifest

**Date:** 2026-09-18
**Branch:** `manafold-pass17`
**Base at corrected evidence curation:** `e150a384ca675985b525be7ac2757e74df3e99ab` / `origin/manafold-pass17`
**Purpose:** exact source receipt plus explicit corrected evidence boundary

This manifest curates only the pending structural/continuity checkpoint. It is not final Pass-17 art acceptance, media, merge or publication. Paths are repo-relative. Do not replace these lists with a run-folder glob.

## Commit 1 — atomic compilable source/gates packet

Stage exactly these 14 paths:

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

### Why these paths are inseparable

- `manafold_rig.h` defines the 23-bone graph; `manafold_model.h` consumes those IDs and staged skin pairs; `manafold_clips.h` writes matching key/midpoint translations; `manafold_art.h` owns the named bounds, timing and authoring controls.
- `manafold_spangate.cpp`, `manafold_qa_p12.cpp`, `manafold_eyesize.cpp`, `manafold_public_jointgate.cpp` and `manafold_probe.cpp` compile against that exact bank and its Q1.15 eye tracks.
- `zhao_reel.cpp` consumes controls from `manafold_art.h` / `manafold_clips.h`, includes the outline helper, and owns the strict same-binary selectors. Do not commit it without those declarations.
- `manafold_outline.h` is shared production/gate logic; `manafold_outlinegate.cpp`, `tools/CMakeLists.txt` and `build-direct.sh` must land together so a clean checkout can build `moutline` through both supported paths.
- No generated production source is required for this packet. `zhao_prod_top.sv` is unrelated because no RTL or port changed.

### Source commit subject/body

```text
Implement Manafold Pass 17 structural repairs

Add the 23-bone signed-span rig, complete O-outline ownership, identity-safe
expression controls, Fall/Q2/Q3 continuity repairs, and attributed gates.

Co-Authored-By: Claude Code <noreply@anthropic.com>
```

### Source commit receipt

Commit 1 is complete and pushed as `e150a384ca675985b525be7ac2757e74df3e99ab`. Its cached path list matched the 14 paths above exactly; fresh cel, `mspan`, `moutline`, `mqa` and `mprobe` builds/gates plus focused selector checks passed before push. No evidence, report, PNG, `.tmp`, raw render or output path entered that commit.

## Commit 2 — curated evidence and plans

Let:

```text
R=runs/CLAUDE-RUNS/RUN-20260917-2318-manafold-pass16-pass17
```

### Journal and reports

Stage exactly:

```text
R/TASK_LOG.md
R/PASS17-COMMIT-MANIFEST.md
R/PASS17-ENDPOINT-AUTHORITY.md
R/PASS17-EVIDENCE-STAGING-REVIEW.md
R/PASS17-EYE-EXPRESSION-REVIEW-PLAN.md
R/PASS17-FALL-REVIEW.md
R/PASS17-INTEGRATED-EVIDENCE.md
R/PASS17-MQA-Q2-Q3-TRIAGE.md
R/PASS17-OUTLINE-AUDIT.md
R/PASS17-OUTLINE-IMPLEMENTATION.md
R/PASS17-OUTLINE-REVIEW.md
R/PASS17-PUBLIC-ORDERING-PLAN.md
R/PASS17-Q2-Q3-REPAIR-PLAN.md
R/PASS17-Q2-Q3-REPAIR.md
R/PASS17-Q2-Q3-REVIEW.md
R/PASS17-REMAINDER-CHECKPOINT.md
R/PASS17-SIGNED-SPAN-IMPLEMENTATION.md
R/PASS17-SPAN-FALL-CODE-REVIEW.md
R/PASS17-SPAN-TARGET-VISUAL-REVIEW.md
R/PASS17-STRETCH-ARCHITECTURE.md
R/PASS17-STRUCTURAL-INTEGRATION-AUDIT.md
R/PASS17-TAUNT3-PUNCHLINE-PLAN.md
R/PASS17-TRICK-AXIS-RENDER-PLAN.md
```

Corrected Commit 2 inventory: **23 reports/journal files + 38 PNGs = 61 files**. The three provenance-deficient preview PNGs are deferred, not silently counted.

Status of the report chain:

- **Current accepted structural evidence:** `PASS17-INTEGRATED-EVIDENCE.md`, `PASS17-SIGNED-SPAN-IMPLEMENTATION.md`, `PASS17-OUTLINE-IMPLEMENTATION.md`, `PASS17-Q2-Q3-REPAIR.md`, and `PASS17-Q2-Q3-REVIEW.md`. `PASS17-FALL-REVIEW.md` and `PASS17-ENDPOINT-AUTHORITY.md` durably support source/gate decisions only; their provenance-deficient preview PNGs are deferred to the final clean bank.
- **Independent review/provenance:** outline review, span/Fall code review, structural integration audit, Q2/Q3 triage and evidence staging review. Their recorded blockers are historical inputs; later current reports record closure. Do not rewrite their original binary observations.
- **Durable failure/before evidence:** span target visual review and its collapse sheets. They prove why the staged repair was necessary and are explicitly not final art acceptance.
- **Next-wave plans:** public ordering, eye-expression, Taunt III punchline, Trick axis and remainder checkpoint. They do not claim the unrendered art passed.
- Report-integrity cleanup is complete: final normalized Q2 values are consistent, Q2/Q3 and selector validation are closed, `e150a384` is recorded, and pre-Q2/pre-final23 reports remain explicitly historical.

### Current accepted / final23 PNG evidence

Stage these exact paths (38 PNGs total across this and the historical section):

```text
R/PASS17-OUTLINE-FINAL23-FIXED-ALLFRAMES.png
R/PASS17-OUTLINE-FINAL23-QUARTER-ALLFRAMES.png
R/PASS17-OUTLINE-FINAL23-CHANNEL-ALLFRAMES.png
R/PASS17-OUTLINE-FINAL23-OPENING-AB-NATIVE.png
R/PASS17-OUTLINE-FINAL23-OPENING-AB-4X.png
R/PASS17-OUTLINE-FINAL23-EFFECT-AB-NATIVE.png
R/PASS17-OUTLINE-FINAL23-EFFECT-AB-4X.png

R/PASS17-SPAN-CEND-FINAL23-NATIVE.png
R/PASS17-SPAN-CEND-FINAL23-4X.png
R/PASS17-SPAN-CEND-FINAL23-BEFORE-AFTER-4X.png
R/PASS17-SPAN-CEND-FINAL23-HOVER-WINDOW-5COL.png
R/PASS17-SPAN-CEND-FINAL23-CHANNEL-WINDOW-5COL.png
R/PASS17-SPAN-CEND-FINAL23-REST-WINDOW-5COL.png
R/PASS17-SPAN-CEND-FINAL23-FLIGHT-WINDOW-5COL.png
R/PASS17-SPAN-CEND-FINAL23-CRACKLE-WINDOW-5COL.png

R/PASS17-Q2-DEATH-DROP-ALLFRAMES.png
R/PASS17-Q2-DEATH-GUTTER-ALLFRAMES.png
R/PASS17-Q2-DEATH-SETTLE-AB-NATIVE.png
R/PASS17-Q2-DEATH-SETTLE-AB-4X.png
R/PASS17-Q3-STARTLE-LATE-ALLFRAMES.png
R/PASS17-Q3-STARTLE-LEGACY-ALLFRAMES.png
R/PASS17-Q3-STARTLE-LATE-LEGACY-NATIVE.png
R/PASS17-Q3-STARTLE-LATE-LEGACY-4X.png
```

Labels:

- `OUTLINE-FINAL23-*` and `SPAN-CEND-FINAL23-*` are the settled 23-bone integrated evidence.
- `Q2-DEATH-*` and `Q3-STARTLE-*` are final post-continuity evidence; `STARTLE-LEGACY-ALLFRAMES` is the same-binary rejected control.
- The provenance-deficient `ENDPOINT-AUTHORITY-*` and `FALL-HOLD-VS-WRAP` preview PNGs are deferred and explicitly excluded below. Their reports retain source/gate receipts and require final-clean-bank visual reproduction.

### Durable before/failure and targeted-motion PNG evidence

Stage these because the committed reports cite them directly:

```text
R/PASS17-SPAN-NUDULE-ALLFRAMES.png
R/PASS17-SPAN-NUDULE-VERTICAL-2X.png

R/PASS17-SPAN-TAUNT-ALLFRAMES.png
R/PASS17-SPAN-TAUNT-SUSPECTS-2X.png
R/PASS17-SPAN-TAUNT2-ALLFRAMES.png
R/PASS17-SPAN-TAUNT2-WITNESSES-2X.png
R/PASS17-SPAN-TAUNT3-ALLFRAMES.png
R/PASS17-SPAN-TAUNT3-SUSPECTS-2X.png
R/PASS17-SPAN-TAUNT3-CEND-4X.png

R/PASS17-SPAN-COLLAPSE-WITNESSES-NATIVE.png
R/PASS17-SPAN-COLLAPSE-WITNESSES-4X.png
R/PASS17-SPAN-COLLAPSE-HOVER-WINDOW-5COL.png
R/PASS17-SPAN-COLLAPSE-CHANNEL-WINDOW-5COL.png
R/PASS17-SPAN-COLLAPSE-REST-WINDOW-5COL.png
R/PASS17-SPAN-COLLAPSE-FLIGHT-WINDOW-5COL.png
```

Labels:

- Nodule sheets are **pre-repair diagnostic mechanism/history evidence** from the four-helper generation; only `NUDULE-VERTICAL-2X` corrects the old extrema labels. They are not current 23-bone structural acceptance and not public-motion proof. Current structure is proved by the posed-ring gate and final23 C-End family; public ordering remains open.
- Taunt-family sheets are durable **pre-ordering / punchline failure evidence**, not final art acceptance.
- Collapse sheets are durable **before/failure evidence** for the repair. The final23 C-End family is the after evidence.

### Evidence commit subject/body

```text
Record Manafold Pass 17 structural evidence

Capture final23 span and outline proof, Fall and continuity controls, independent
reviews, failure witnesses, and the bounded plans for the remaining art waves.

Co-Authored-By: Claude Code <noreply@anthropic.com>
```

## Explicit exclusions

Never stage these paths/families for this checkpoint:

```text
.tmp/**
out/**
**/*.rgb
**/mspan-*.log
**/mqa-*.log

R/PASS17-SPAN-CEND-REPAIR-PROVISIONAL-*.png
R/PASS17-ENDPOINT-AUTHORITY-LADDER.png
R/PASS17-ENDPOINT-AUTHORITY-4X.png
R/PASS17-FALL-HOLD-VS-WRAP.png
R/PASS17-OUTLINE-INTEGRATED-*.png
R/PASS17-OUTLINE-OPENING-AB-*.png
R/PASS17-OUTLINE-EFFECT-AB-*.png
R/PASS17-SPAN-NUDULE-WITNESSES.png
R/PASS17-SPAN-NUDULE-WITNESSES-4X.png
R/PASS17-SPAN-COLLAPSE-HOVER-WINDOW.png
R/PASS17-SPAN-COLLAPSE-CHANNEL-WINDOW.png
R/PASS17-SPAN-COLLAPSE-REST-WINDOW.png
R/PASS17-SPAN-COLLAPSE-FLIGHT-WINDOW.png
```

Why:

- `.tmp`, `out`, raw RGB and focused logs are reproducible scratch, not the evidence artifact.
- `PROVISIONAL` and non-`FINAL23` outline A/B/all-frame images are superseded 22-bone/pre-integration evidence.
- Endpoint authority and Fall preview PNGs lack a durable renderer/source generation receipt; regenerate them from the final clean Pass-17 bank before citing visual acceptance.
- The first nodule witness pair sampled lateral/rest frames; the corrected `NUDULE-VERTICAL-2X` plate replaces it.
- Non-`5COL` collapse windows duplicate the self-describing compact sheets.
- Do not stage by broad `PASS17-*` glob: that would sweep every excluded file back in.
- Do not add `.tmp` to `.gitignore` and call cleanup solved. After durable evidence is committed, use the repository purge tool under its dry-run/newest/48-hour safeguards.

## Post-staging verification protocol

### Completed source commit receipt

1. Strict full-string selector cleanup completed; all 37 valid/invalid cases passed.
2. The exact 14 source paths above were staged; cached diff/path/whitespace checks passed and no run/evidence/scratch path was included.
3. Fresh settled-tree cel and focused gate verification passed.
4. Commit `e150a384ca675985b525be7ac2757e74df3e99ab` is pushed to `origin/manafold-pass17`.

### Before evidence commit

1. Confirm all report-integrity cleanup items in this manifest are resolved.
2. Stage only the explicit journal/report and PNG lists above. Expand every family to reviewed names; do not pass a wildcard to `git add`.
3. Run `git diff --cached --check`.
4. Inspect `git diff --cached --name-only` and reject any path containing `.tmp`, `PROVISIONAL`, `ENDPOINT-AUTHORITY-*.png`, `FALL-HOLD-VS-WRAP.png`, `OUTLINE-INTEGRATED`, old `OUTLINE-OPENING-AB` / `OUTLINE-EFFECT-AB`, `NUDULE-WITNESSES`, or a non-`5COL` collapse window.
5. Confirm every staged PNG is non-empty and its cited report names it (or its exact family); confirm no raw frame or transient log is staged.
6. Commit with the evidence message above and push `manafold-pass17` immediately.
7. Verify the branch matches `origin/manafold-pass17`; the only allowed remaining files are ignored scratch and uncommitted next-wave art work.

## Acceptance boundary

These two commits close the structural/continuity checkpoint only. Pass 17 remains open on public A/B/C crown ordering, Trick axis selection, Taunt-III held punchline, native eye-expression acceptance, final integrated gate, exact 28-subject generation, isolated every-frame review, encode/freshness/decode, main integration, publish and production-byte verification.
