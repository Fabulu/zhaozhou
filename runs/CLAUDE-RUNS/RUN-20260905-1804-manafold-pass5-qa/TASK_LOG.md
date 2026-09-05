# Task Log: RUN-20260905-1804 — Manafold pass 5 QA

**Created:** 2026-09-05 18:04 (Europe/Zurich)
**Status:** Complete
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260905-1804-manafold-pass5-qa/

---

## Objective

Attack the CLAIMS and the INSTRUMENTS of Manafold pass 5 (`zhaozhou` f9bf26bf,
`Upheaval` 78140d0) against `Upheaval/creature/10-GATE-CHECKLIST.md`. A separate
reviewer judges the visuals. Verdict per claim: CONFIRMED / REFUTED /
UNVERIFIED, each decided by a command and its output from a build I made.

## Lane

Own clones from the true remotes, nothing shared with the hardware lane or the
reviewer's lane:

* `C:\programmieren\zencrifice\manafold-pass5-qa\zhaozhou`  (github.com/Fabulu/zhaozhou)
* `C:\programmieren\zencrifice\manafold-pass5-qa\Upheaval`  (github.com/Fabulu/untitled-game)

Build trees are lane-local under `/c/mf5qa/` (short root — the deep scratchpad
path hits MAX_PATH, per pass 5's own Don't-Retry list). `cmake --build` was never
invoked; `tools/reel/build-direct.sh` only.

## Timeline

| when | what |
|---|---|
| 18:04 | Run created. Both clones verified: `f9bf26bf` present (HEAD is `dd4af89d`, a docs-only mipmapping addendum on top), `78140d0` is Upheaval HEAD. |
| 18:06 | Clean-clone `cel` build at `f9bf26bf` started (claim 1). |
| 18:09 | Build OK. `manafold-hover` renders 600 frames / 9700 colours, zero near-black pixels. Clean checkout is fixed. |
| 18:1x | Delete-page negative test: `cel` (script check), `mprobe` and `mmeshcheck` (unguarded include) all exit 1. Page restored. |
| 18:1x | Page regeneration byte-exact at HEAD. |
| 18:2x | `seam.py` fed five known-bad fixtures. Responds to a broken loop (ratio 40); prints `nan`/`inf` on degenerate input at exit 0; **aliases to ratio 1.00 on a period-10 clip**. |
| 18:2x | Baseline `cc5ff8d9` worktree built. First render gave `0x25909C45` — the page is UNTRACKED at that commit, so a pristine worktree builds the pageless creature. The pass-4 bug, reproduced independently. |
| 18:3x | Regenerated the page from cc5ff8d9's own generator (sha256-identical to HEAD's). Rebuilt. **`manafold-hover` = `0x5B44FCF2`, seam 4.07 / ratio 5.62 — calibration reproduces to the digit.** |
| 18:3x | Honest windowing over all 599 pairs: baseline ratio 5.260 → head 2.319. Headline survives. |
| 18:4x | `manafold-probe` built and run: lens 1228/102, star 1269/121, ring 1302/135 — all three reproduce exactly; LENS gate OK. |
| 18:4x | Reading the probe output revealed the probe has **no** star-in-lens containment gate. Recomputed pass 4's own containment arithmetic with the new 22 mm tube: derived limit 1787 vs shipped `kGazeMaxA16 = 2400`. **New defect.** |
| 18:5x | `evidence/kneadcount.cpp` written and run at both commits. Three claimed numbers exact; **four more clips retimed and unreported**. |
| 18:5x | Zixxtrixx trio at both commits, `ZIXX_EXP=celmain`: all three CRCs equal and equal to the published values; 1139/1139 files sha256-identical (1136 `.rgb` + 3 `meta.txt`). |
| 19:0x | Freeze A/B: frozen `0xD03110F6` reproduces exactly; live `0x59ADB544` reproduces under no env at HEAD. Built `e4a4b89e` to characterise the mismatch. |
| 19:0x | Media: 22 subjects, inspect poster 35,918 B, webm 2,258,752 B and decodable (vp9 384x240 10.0s). `mediacheck.py` shown blind to a 0-byte file AND to `robots content="index,follow"`. |
| 19:1x | Item 1 parked: full constant diff is five items, none on the mote/stencil/pocket/camera axis. |

## Where I was, before reading each long-running result

Logged per CLAUDE.md's "write down where you were BEFORE reading it":

* Before the baseline build returned: mid-way through the `seam.py` can-fail
  fixtures; next step was the period-10 alias case.
* Before the e4a4b89e build returned: writing up claim 3; next step was the
  media/site sweep.
* Before the ffprobe sweep returned: drafting FINDINGS; next step was the
  Zixxtrixx sha256 comparison.

## Decisions made

* **Rendered under `ZIXX_EXP=celmain` alone for the Zixxtrixx CRCs**, matching
  pass 5's recorded env, after `celmain + ZIXX_LIGHT` produced a different
  (equally stable) set. The identity test is the equality, but reproducing the
  published digits is what makes it checkable.
* **Reconstructed the baseline page rather than declaring the calibration
  unreproducible.** A pristine worktree genuinely cannot produce the pass-4
  number; regenerating from that commit's own generator is the only honest
  reconstruction, and it reproduced everything exactly.
* **Did not chase a visual verdict.** Containment is reported as a stale
  derivation with no gate, not as a visible artefact — that belongs to the
  reviewer.

## Files created

* `FINDINGS.md`
* `evidence/kneadcount.cpp` — per-slot knead/gather/hold/release frame counts
* `evidence/seamhonest.py` — all-pairs loop seam with percentile context
* `evidence/seam_canfail_fixture.py` — five synthetic clips with known seams

## Background jobs

Every background job started by this run was polled to completion and the
process table checked empty of `zhao-reel`, `ffprobe`, `g++` and `git` children
before this log was closed. See the closing section of FINDINGS.md.
