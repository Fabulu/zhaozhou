# SPEC v1: Manafold pass 5 — QA

**Run ID:** RUN-20260905-1804
**Created:** 2026-09-05 18:04 (Europe/Zurich)
**Status:** Complete

---

## Objective

Attack the claims and the instruments of Manafold pass 5 against
`Upheaval/creature/10-GATE-CHECKLIST.md`. Decide each claim CONFIRMED /
REFUTED / UNVERIFIED from a build I made and a render I ran. A separate
reviewer judges the visuals; this run does not.

## Scope

**In scope:** build integrity; the seam instrument and its headline; the
`U02_FOLD_FREEZE` gate; the eye-gate re-baseline; knead coverage; Zixxtrixx
byte identity; media and site; confirmation that item 1 was left untouched;
and any gate that cannot fail.

**Out of scope:** creature constants (read, never edited); publishing;
deploying; the visual verdict; item 1 itself.

## Constraints

* Own clones only. `C:\programmieren\zencrifice\zhaozhou`,
  `...\Upheaval` and `...\manafold-pass5-review` never touched.
* `tools/reel/build-direct.sh` only. Never `cmake --build` — a Quartus fit may
  be live in the hardware lane and the stale-binary race reports old numbers.
* Frames read with `tools/reel/rgbframe.py` only.
* Build trees under short roots (`/c/mf5qa/...`) — the scratchpad path hits
  MAX_PATH, per pass 5's own Don't-Retry list.
* Calibrate against a self-built `cc5ff8d9` before trusting any delta.

## Don't Retry

* A pristine `cc5ff8d9` worktree for calibration — the page is **untracked** at
  that commit, so a pristine tree builds the pageless creature and renders
  `0x25909C45`. Run `tools/pack/mkmanafoldpage.py` in the worktree first, then
  the baseline reproduces `0x5B44FCF2` exactly.
* `curl` to `upheaval.pages.dev` from this lane — no outbound network (exit 43).
  The markdown-converting fetcher strips meta tags, so it cannot confirm the
  live `noindex` either way.
* Zixxtrixx CRCs under `ZIXX_EXP=celmain` **plus** `ZIXX_LIGHT` — that gives a
  different (valid) set. The published trio needs `celmain` alone.
* `#include "manafold.h"` in a standalone probe without `namespace zc =
  zref::creature;` and the reel include set — `manafold_art.h` is not
  self-contained either (needs `<cstdint>` from its includer).

## Outcome

Eight claims confirmed, two refuted in part, one new defect found (the star
containment derivation invalidated by the 22 mm ring tube, with no gate). Three
gates shown unable to fail. See FINDINGS.md.
