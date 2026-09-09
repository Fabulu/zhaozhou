# RUN-20260909-2306 — Manafold pass 15, the by-eye review

**Role:** BY-EYE REVIEWER. Judge whether the creature is good, not whether the gates passed.
**Lane:** `C:\programmieren\zencrifice\manafold-p15-review\{zhaozhou,Upheaval}` at `origin/main`,
zhaozhou `67347e2a`. Never touched the primary trees or any sibling `manafold-*` lane except to
read `manafold-p12-fix/.../scratch-reel` (read-only).
**Deliverable:** `Upheaval/creature/Manafold/PASS-15-REVIEW.md` + `pass15-review-plates/`.

## Load discipline

`Get-CimInstance Win32_Process` for `quartus*`, `zhao-reel-cel.exe`, `ffmpeg.exe` returned
**nothing** before any work. **No build, no render, no encode.** The only ffmpeg use was
`webm2rgb.py` pulling **six committed frames** out of already-published pass-14 clips for the
generation A/B. Never identified a process by name for killing; never needed to kill one.

## What I did, in order

1. Read `OWNER-DIRECTION-11`, `Upheaval/CLAUDE.md`, the three concept sheets,
   `10-GATE-CHECKLIST` §0 and all 43 items.
   * **The Description sheet turned out to be load-bearing.** Its inset is labelled
     *"abstehendes Auge"* — a **protruding** eye — and the text says *"die Augen stehen leicht
     nach vorne."* That single line decided fault #2: the eyes are supposed to stand out, so
     "they look sunken" could not be the diagnosis.
2. Verified the instrument first (item 7): `rgbframe.py selftest` → PASS. Wrote `plate.py`,
   `ab.py`, `bank.py`, `bbox.py` as **plate-makers only** — they import `rgbframe.load` and
   never judge anything.
3. Whole-creature read at 3×–5× on `hit`, `rest`, `hover`, `channel`, `taunt3` (A01–A05).
4. Bank-wide survey, one mid frame from all 28 subjects (C01).
5. Contact sheets to find candidates; **2×–12× crops of named frames to confirm every one**
   (item 41).
6. Generation A/B against the published pass-14 clips (D01, J01, J02).
7. Spawned one read-only Explore agent to extract the three lanes' numeric claims and the
   shipped constants while I kept looking. Its extract changed two of my conclusions.

## The turn of the run

I was an hour into being certain the body had stopped being a *Kugel* — `channel` and `hit`
render it as a hard-edged kite, and the subagent confirmed **no lane touched `kBodyTaperPm`
(all 1000) or `kBodySegments` (32)** in pass 15. It looked like a mystery regression.

Then I put pass 14 beside it. **J02: the same kite, in the generation he praised.** It is the
deform, it predates this pass, it is not a pass-15 fault. **Withdrawn.**

The lesson is item 41 pointed back at me: the cheap A/B settled in one minute what an hour of
source-reading had not, and I should have reached for it first. It also freed the finding that
mattered — the same A/B is what made the **bleach** unmissable.

## The findings, ranked (full detail in PASS-15-REVIEW.md)

1. **The fog is on the wrong side of the ink line.** `kShellOutReachPm = 55` (5.5% of R
   outside) against `kShellFogDepthPm = 520` (52% inside), peak `kShellAlphaMaxPm = 560` at
   0.52·R **inside** the outline, tint `{255,214,232}`, and `shell_paint` composites **every
   interior pixel**. It has bleached the body's pigment and erased the terminator that made
   the ball read round. **A regression on a quality he named as praised.** J01/J02.
   The FX lane's own findings flag `kShellTint` as never swept and its `wash-check.png` as
   showing the fog "lifting deep magenta a long way". Both warnings were right.
2. **The lens is a dagger**; `kEyeSurfaceFollowPm = 0` so the flat plate goes edge-on and
   draws a **white splinter across the body** (K01). D11 §2.2 undelivered — item 39's
   two-landings trap on this exact channel, again.
3. **The lightning fails two opposite ways** — capsules in `taunt3` f0288–f0352 (H04), fused
   mass at `channel` f0250 (P01, the FX lane's own declared residual). Its best (H05) is
   genuinely excellent.
4. **The ball beat is on 3 clips of 28**, and the lightning sits on top of the antenna.
   LANE-ANTENNA §5 predicted this exactly; I confirmed it.
5. **The lenses cross at the face centre** at travel extremes (O01) — unnamed by anyone.

## What is right, and must survive pass 16

Both eyes readable (F02, I01, O01) · the eyes genuinely travel, proved on the **artefact**
with a fixed camera (O01) · the bounce no longer swallows them (N01) · the star near-on is
the sheet's 4-point star (I03) · the lightning's white core + blue shimmer, not black (H05) ·
the nodule bead chain is visible (G01) · **the deaths are intact** (L01) · `channel`'s violet
night · ground contact reads correctly across the bank.

## Corrections issued to the coordinator

* "The lenses look sunken" — **no**, the opposite; E01 at 12× shows the lens defining the
  silhouette's corner. Acting on that diagnosis would have sent pass 16 to the wrong knob.
* "The washed lower half" — **wider than that**; the whole interior is painted, and it is
  genuinely too much fog, not only misplaced fog.
* "taunt3 reads as bars" — **confirmed**, narrowed to f0288–f0352, plus the opposite mode
  he had not seen.

## Claims I downgraded

* **"Readable 0% → 89%"** is `manafold_eyecam.cpp` measuring *both eyes within ±45° of the
  camera* — **geometry, not readability**. True by eye as well (I checked), but the word
  oversells the number.
* **`eyesweep.py` failed at its calibration legs on the merged tree** — the FX shell washed
  the palette its mask depends on. So the screen-pixel travel numbers come from a tree that
  is not the shipping one. The travel is real regardless; I verified it on the frames.
* **Every plate in `pass15-plates-eye/` predates the FX merge.** This review is the first
  look at the merged result, as the coordinator suspected.

## Not done, and named

* **I did not ablate the shell.** The A/B is across two binaries and isolates the *pass*, not
  the mechanism (item 26). That leg is unrun and is pass 16's first job.
* **Direction 11 §1 (stale media) is untouched by this pass** and by me.
* **`lane-audit.sh` does not exist in this tree**, so item 35's precondition for deleting a
  lane cannot be run. `git status` + `git log origin/main..HEAD` clean in both repos is what
  I have. (`purge_render_intermediates.py` IS present here, at `zhaozhou/tools/maintenance/`.)

## Lane disposition

**Deletable** once this run folder and the plates are pushed and the push is verified from
outside the lane. No source changes, no shipping constant touched, no render frames written.
**`manafold-p12-fix/.../scratch-reel` is NOT mine and must outlive me** — it is the only copy
of this bank.
