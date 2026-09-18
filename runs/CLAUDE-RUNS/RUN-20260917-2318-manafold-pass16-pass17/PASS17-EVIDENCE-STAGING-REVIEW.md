# Manafold Pass 17 evidence staging review

**Task:** verify every report and PNG proposed by `PASS17-COMMIT-MANIFEST.md` before the evidence commit.

**Date:** 2026-09-18
**Source checkpoint pushed:** `e150a384ca675985b525be7ac2757e74df3e99ab` (`Implement Manafold Pass 17 structural repairs`)

## Verdict

**PASS — corrected evidence set is ready for explicit staging.**

The source receipt, selector/Q2 cleanup and stale-state corrections are now durable. The Fall and endpoint preview PNGs could not be tied to an exact renderer/source generation from surviving records, so they are honestly deferred rather than back-filled with inferred provenance. Their source/gate reports remain; final-clean-bank visual evidence is still required.

Do not broaden `git add`. Stage only the 61 exact Commit-2 paths below / in `PASS17-COMMIT-MANIFEST.md`.

## Corrected exact inventory

### Reports and journal — 23

```text
TASK_LOG.md
PASS17-COMMIT-MANIFEST.md
PASS17-ENDPOINT-AUTHORITY.md
PASS17-EVIDENCE-STAGING-REVIEW.md
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
PASS17-REMAINDER-CHECKPOINT.md
PASS17-SIGNED-SPAN-IMPLEMENTATION.md
PASS17-SPAN-FALL-CODE-REVIEW.md
PASS17-SPAN-TARGET-VISUAL-REVIEW.md
PASS17-STRETCH-ARCHITECTURE.md
PASS17-STRUCTURAL-INTEGRATION-AUDIT.md
PASS17-TAUNT3-PUNCHLINE-PLAN.md
PASS17-TRICK-AXIS-RENDER-PLAN.md
```

### PNG evidence — 38

```text
PASS17-OUTLINE-FINAL23-FIXED-ALLFRAMES.png
PASS17-OUTLINE-FINAL23-QUARTER-ALLFRAMES.png
PASS17-OUTLINE-FINAL23-CHANNEL-ALLFRAMES.png
PASS17-OUTLINE-FINAL23-OPENING-AB-NATIVE.png
PASS17-OUTLINE-FINAL23-OPENING-AB-4X.png
PASS17-OUTLINE-FINAL23-EFFECT-AB-NATIVE.png
PASS17-OUTLINE-FINAL23-EFFECT-AB-4X.png
PASS17-SPAN-CEND-FINAL23-NATIVE.png
PASS17-SPAN-CEND-FINAL23-4X.png
PASS17-SPAN-CEND-FINAL23-BEFORE-AFTER-4X.png
PASS17-SPAN-CEND-FINAL23-HOVER-WINDOW-5COL.png
PASS17-SPAN-CEND-FINAL23-CHANNEL-WINDOW-5COL.png
PASS17-SPAN-CEND-FINAL23-REST-WINDOW-5COL.png
PASS17-SPAN-CEND-FINAL23-FLIGHT-WINDOW-5COL.png
PASS17-SPAN-CEND-FINAL23-CRACKLE-WINDOW-5COL.png
PASS17-Q2-DEATH-DROP-ALLFRAMES.png
PASS17-Q2-DEATH-GUTTER-ALLFRAMES.png
PASS17-Q2-DEATH-SETTLE-AB-NATIVE.png
PASS17-Q2-DEATH-SETTLE-AB-4X.png
PASS17-Q3-STARTLE-LATE-ALLFRAMES.png
PASS17-Q3-STARTLE-LEGACY-ALLFRAMES.png
PASS17-Q3-STARTLE-LATE-LEGACY-NATIVE.png
PASS17-Q3-STARTLE-LATE-LEGACY-4X.png
PASS17-SPAN-NUDULE-ALLFRAMES.png
PASS17-SPAN-NUDULE-VERTICAL-2X.png
PASS17-SPAN-TAUNT-ALLFRAMES.png
PASS17-SPAN-TAUNT-SUSPECTS-2X.png
PASS17-SPAN-TAUNT2-ALLFRAMES.png
PASS17-SPAN-TAUNT2-WITNESSES-2X.png
PASS17-SPAN-TAUNT3-ALLFRAMES.png
PASS17-SPAN-TAUNT3-SUSPECTS-2X.png
PASS17-SPAN-TAUNT3-CEND-4X.png
PASS17-SPAN-COLLAPSE-WITNESSES-NATIVE.png
PASS17-SPAN-COLLAPSE-WITNESSES-4X.png
PASS17-SPAN-COLLAPSE-HOVER-WINDOW-5COL.png
PASS17-SPAN-COLLAPSE-CHANNEL-WINDOW-5COL.png
PASS17-SPAN-COLLAPSE-REST-WINDOW-5COL.png
PASS17-SPAN-COLLAPSE-FLIGHT-WINDOW-5COL.png
```

Corrected total: **23 reports/journal files + 38 PNGs = 61 files**.

## Resolved corrections

1. `PASS17-REMAINDER-CHECKPOINT.md` now marks strict selector validation closed and records pushed source checkpoint `e150a384`.
2. `PASS17-MQA-Q2-Q3-TRIAGE.md` now records final normalized Q2 values **1.89°/2.57°** and cites `PASS17-Q2-Q3-REVIEW.md`; original pre-repair 31.80°/31.86° observations remain historical.
3. `TASK_LOG.md` and `PASS17-COMMIT-MANIFEST.md` record the exact 14-path source commit/push receipt.
4. This review is included in Commit 2, bringing the report count to 23.
5. `PASS17-FALL-HOLD-VS-WRAP.png`, `PASS17-ENDPOINT-AUTHORITY-LADDER.png` and `PASS17-ENDPOINT-AUTHORITY-4X.png` are excluded because no durable surviving record ties them to an exact renderer/source/frame generation. The reports now restrict durable acceptance to source/gates and require clean final-bank visual reproduction.
6. Old Nodule sheets are labelled pre-repair diagnostic/history evidence, not final23 structure or public-ordering proof.

## Provenance and file-health result

- Source commit `e150a384ca675985b525be7ac2757e74df3e99ab` matches the manifest's exact 14 source paths and is pushed to `origin/manafold-pass17`.
- Final23 span/outline reports consistently name renderer MD5 `F7C2DAA085661C1E00CADF09C65B5F8A`.
- Q2/Q3 reports consistently name renderer MD5 `41620610BF86B36EA7AEB8279434F834` and final normalized Q2 values.
- Historical outline MD5 `3957094CF4084DD510B409D1CE9D701F`, four-helper collapse sheets and pre-repair Taunt/Nodule sheets are explicitly historical or failure evidence.
- All 61 curated files exist and are non-empty after whitespace normalization: **19,276,233 bytes / 18.38 MiB**.
- All 38 PNGs decode, have non-zero dimensions and non-uniform content; their SHA-256 hashes are unique.
- Every curated PNG is cited by a curated report/manifest, and all local `PASS17-*.md` links in the curated reports resolve.
- No curated path is under `.tmp/`, `out/`, a raw `.rgb` tree or a transient gate-log family.

## Explicit exclusions

Reject `.tmp/**`, `out/**`, raw RGB, transient logs, every `PROVISIONAL` span image, superseded non-final23 outline families, wrong first nodule witnesses, duplicate non-`5COL` collapse windows, and the three provenance-deficient Fall/endpoint preview PNGs.

New crown-shuffle implementation/evidence created concurrently belongs to the next art packet and is not part of this 61-file structural evidence checkpoint.

## Staging protocol

1. Stage the manifest's 23 reports/journal paths and 38 PNG paths explicitly—never a glob.
2. Run `git diff --cached --check`.
3. Compare the cached path list exactly with the 61-path manifest.
4. Reject any staged exclusion token/family above.
5. Recheck all staged PNGs are non-empty/decodable and every staged report reference resolves.
6. Commit with the manifest's evidence message and required trailer, push `manafold-pass17`, then verify origin equality.

As I always say, an honest scrapbook leaves a blank space for the photograph the bus has not taken yet!
