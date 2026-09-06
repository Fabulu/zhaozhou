# SPEC v1: QA of Manafold pass 9 -- attack the claims, gates and instruments

**Run ID:** RUN-20260906-1348
**Created:** 2026-09-06 13:48 UTC+02:00
**Status:** Complete
**Previous Version:** N/A

---

## Objective

Pass 9 (creature 02, Manafold) is published. A by-eye reviewer judges the read in
a parallel lane; **this run attacks the claims, the gates and the instruments.**

Success is a verdict of CONFIRMED / REFUTED / UNVERIFIED on each of ten published
claims, each decided by a named command and its output, plus a list of gates that
cannot fail and a ranked list for pass 10.

**The one owed check:** pass 9 could not complete the full-site `checkmedia.py`
(killed twice, reported as >1 hr). Run it to completion, confirm the 2026-09-06
stderr fix is present, and prove it still rejects a deliberately truncated file.

---

## Scope

**In Scope:**

- The full-site decode gate, run to completion, and the decoder's failability.
- The ten claims: smear isolation, the mist follow, the joints gate, the closure
  regression and its sweep, the neck lever, the section 9.2 rates, the ablation
  lattice, the archive reconstruction, Zixxtrixx untouched, the stale-clip near
  miss.
- Calibration: build the shipped SHA and reproduce the published numbers.
- Feeding every gate a known-bad input.

**Out of Scope:**

- By-eye judgement of the creature -- that is the parallel reviewer's lane.
- Any change to a creature constant. Any publish.
- Every other lane on this machine, including `manafold-p9-review`.

---

## Constraints

- Own clone at `manafold-p9-qa/{zhaozhou,Upheaval}` from `origin/main`.
- Never `cmake --build`; `tools/reel/build-direct.sh` only, reading BUILD_RC
  directly and never a pipeline's status.
- Never rebuild while a render is running (09-ENGINE-GOTCHAS section 13).
- Frames read only through `tools/reel/rgbframe.py`.
- Any experiment that touches a constant is reverted and the tree verified clean
  before moving on.
- Verify every push landed with `git fetch && git branch -r --contains <sha>`.

---

## Don't Retry

- `zhao-reel-cel.exe --help` -- argv[1] is the output directory, so this renders
  the entire reel into a directory named `--help`.
- Reversal counts on the hinge CSV without a deadband -- integer millimetres, and
  a +/-1 dither manufactures 182 reversals where there are 7.
- Matching pass-4 archive files to history by their archived names: the archive
  uses `archive-pass4-u02-*` while the historical bank was `manafold-*`.

---

## Outcome

Nine of ten claims CONFIRMED. Claim 1 confirmed in substance, refuted as worded.
Claim 2's mechanism confirmed, its published numbers unreproducible. The owed
check discharged: 1,123/1,123 files decode, in 22 minutes rather than an hour.

Findings: `Upheaval/creature/Manafold/PASS-9-QA.md`, with eight evidence files in
`Upheaval/creature/Manafold/pass9-qa-evidence/`.
