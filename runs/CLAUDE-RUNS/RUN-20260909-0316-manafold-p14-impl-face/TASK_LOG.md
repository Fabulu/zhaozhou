# Task Log: RUN-20260909-0316 - [Describe objective here]

**Created:** 2026-09-09 03:16 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-0316-manafold-p14-impl-face/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 03:16 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-0316
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

## IMPL-FACE — pass 14 wave 1 (R2(a) ablation first, then R1, R3)

Lane: `C:\programmieren\zencrifice\manafold-p14-face\{zhaozhou,Upheaval}` @ origin/main
(zhaozhou d171a608, Upheaval 97daa95).

### Progress timeline

* 03:16 Run opened. Read: art law (lane `Upheaval/CLAUDE.md`), PASS-14-PLAN §0-§2,
  R1/R2/R3, §5 wave design, §6 protected list; PASS-13-REVIEW §2.1/§2.4;
  PASS-13-QA §0; pass13-expressiveness/FINDINGS.md; 09-ENGINE-GOTCHAS §19/§20/§21;
  10-GATE-CHECKLIST §0/§A.
* 03:17 Baseline `--clean` build (`build-direct.sh --output build-face --clean cel`),
  BUILD_RC=0, md5 `9e994246e6d9cc3853db77fa18e4b5eb`.
* 03:2x Baseline render started: `manafold-hover manafold-channel` under
  `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`.
* 03:2x `kBodySegments`/`kBodyPoleSegments` 16 -> 32 (ABLATION ONLY, not shipped),
  second `--clean` build into `build-face32`.

### Source reading done BEFORE the ablation (mechanism, gotcha §18)

The shipping env `ZIXX_EXP=celmain` sets **`g_smooth_toon_bands = 3`**, NOT
`g_cel_bands` (`zhao_reel.cpp:7570-7573`). So the shipping terminator is the
*smooth* toon branch (`creature_sim.cpp:1003-1007`): per-corner Gouraud light,
linearly interpolated across the triangle, thresholded per fragment by
`kSmoothCel3Ramp` in `rast.cpp:apply_toon_ramp`.

Two structural reasons that boundary staircases, both mesh-density-dependent:
1. the interpolated scalar is only C0 across an edge, so the iso-contour is a
   POLYLINE with a kink at every mesh edge; and
2. `kSmoothMixNum = 819` (`creature_sim.cpp:593`) mixes **20% FLAT FACE light**
   into every corner, which is discontinuous across an edge — a genuine STEP,
   not a kink.

Prediction recorded before looking: 32 segments halves both, so the staircase
shrinks but does not become a curve. The plan's outcome-2 leg ("analytic
ellipsoid normals in compile_creature") is therefore NOT obviously the answer —
the normals are already smooth/position-keyed and the deform applies the inverse
transpose. **The look decides, not this note.**

### Where I am (written down before results arrive, per CLAUDE.md)

Next step: crop plates of `channel` f157/f175/f180/f185 and `hover` f37 at 16 vs
32, at 4x and at native; report the ablation answer to the coordinator; then R1.

### 2026-09-09 08:00 — lane RESUMED after the machine-load stop

Read `PASS-14-LANES-PAUSED.md` first, as briefed. **The review's "your ablation is
confounded" instruction was already satisfied by the pre-stop leg**: the
segments-only rung (11/32) was built, rendered and looked at, and its answer is
committed as Upheaval `771c13a`. Confirmed it myself at 4x before going on —
`AB-h37-4x.png` and `AB-ch180-16v32.png`. The chords go; the near-horizontal
lower band edge is IDENTICAL at 16 and 32, which is the latitude residual.

⚠ **08:03 — `zhao-reel-cel.exe --help` is not a help flag.** `g_out = argv[1]`,
so it started a full 28-subject render into a directory literally named `--help`.
Killed both PIDs inside a minute and removed the directory. **The reel's CLI is
`zhao-reel-cel <outdir> [clip ...]` and it has no help; there is nothing to ask
it.** Recorded because the machine is shared with a fit and this is exactly the
load that was not supposed to happen.

### The third rung was BROKEN — and that is the pass's real finding

`render-3221` (rings 21, segments 32) renders a ball with **no crown**: the top
ten rings collapse onto the axis, the face opens into a bowl, a fan of degenerate
triangles converges on a point and the eye floats in the hole. `BUILD_RC=0`, no
warning, no assert.

`kBodyRings` sizes `kBodyTaperPm[]` and `kBodyLeanXMm[]`, both left at eleven
entries. C++ zero-fills the tail; `make_body` multiplies radius by taper; rings
11-20 get radius zero. Fixed at the root in `manafold_art.h` with both tables
extended and a `static_assert` **calibrated on a known-negative** (POS_RC=0 at 21
entries, NEG_RC=1 at 11). Written up as gate checklist item 42.

### Where I am (written down before results arrive)

`build-face21` — the CORRECTED rings-21/segments-32 leg — is building `--clean`.
Next: render `manafold-hover` and `manafold-channel` from it, crop f37 / f180 /
f185 three ways, and decide the shipped `kBodyRings` BY LOOKING (§1.7 of the
findings). Then R1 (lens width -> star -> converge -> centre last), then R3.

### 08:2x — the ablation is CLOSED, by looking, on three separate crops

Four legs on the table: 11/16 (shipping), 11/32, 21/32 (zero-fill bug), 21/32
(repaired). Judged on `hover` f37's terminator, `channel` f180's near-horizontal
band edge at 8x, and `channel` f180's bottom silhouette arc at 8x.

**16 -> 32 segments is a clear, unambiguous gain. 11 -> 21 rings is not.** On the
bottom arc the repaired rings leg is indistinguishable from 11/32 and arguably a
shade more angular on the left; on `hover` f37 there is no difference I can point
at; on the band edge it is modestly better on the DIAGONAL runs only. That is
+640 triangles on top of the segments leg for something I cannot show anyone.

**SHIPPED: rings 11, segments 32, pole segments 32.** The review's hope --
"if the cheap leg is enough, the expensive one never needs shipping" -- held.

⚠ **And the honest residual:** the truly HORIZONTAL runs in that band edge are
IDENTICAL across all three legs. That is the tell that they are not a facet-size
artefact at all -- where the surface is near-tangent to the band the iso-contour
genuinely is flat, and no density fixes it. The remaining lever there is
`kSmoothMixNum`'s 20% flat-face term or the ramp, not more triangles.

### R1 groundwork, done from frames already on disk (no new render)

`R1-eye-now-8x.png` -- the shipping eye at 8x on four frames. The review's
headline is plainly true and I did not need a measurement for it: `hover` f37 is
a long thin purple BLADE with a white sliver in it, not a star in an almond.
`hover` f573 is two star-less blades. `hover` f393 has **no eye at all** -- not a
crescent, nothing -- which is R3's mechanism (fixed +X plate normal at 28.3
azimuth) showing up in the picture rather than in the source.

### Where I am (written down before the next results arrive)

`build-ship` is the `--clean` rebuild of the FINAL header (rings 11 / segs 32),
linking. Next: md5 it against `build-face32` (`679e8797be40e79fa4fe808775a352f8`)
as a positive control that the shipped tree IS the leg that was looked at, then
commit + push R2(a) to main. Then R1 step (a) only: `kEyeWideMm` 84 -> ~125, one
build, render, look. Order is mandated: widen -> star -> converge -> centre LAST.

### 08:5x — R2(a) CLOSED and pushed. Lane state.

Positive control run on the PICTURE, not the hash: `build-ship`'s md5 does NOT
match `build-face32` (an executable's hash moves with the build path), but all
**421 channel frames are byte-identical**, and the known-negative against the
11/16 base differs on **421 of 421**. Pushed to `origin main` in both repos as
one verified commit each; the raw lane history including the broken intermediate
is preserved on `archive/p14-face-raw` (pushed).

⚠ **A `git reset --soft origin/main` staged DELETIONS of `reports/DOCKET.md` and
another run's TASK_LOG.** Those were not mine — origin/main had moved ahead and
my lane was behind, so the reset diffed my older tree against newer content.
Backed out with `reset --hard` to the archive branch and rebased properly
instead. **A soft reset against a moved remote will happily stage other people's
work as deletions**, and the only reason it was caught is that the staged list
was read before committing.

⚠ **An orphan `zhao-reel-cel.exe` was running and it was NOT MINE** — its command
line put it in `manafold-p14-reel`, another lane. **It was identified by command
line before anything was aimed at it, and left alone.** Killing by image name
here would have destroyed a sibling lane's render: this is the exact mechanism
the owner spent last night trying to attribute. `taskkill /IM` has no place in a
multi-lane tree; `Get-CimInstance Win32_Process` first, always.

Also this session: `zhao-reel-cel.exe --help` is not a help flag (`g_out =
argv[1]`) and started a full 28-subject render into a directory named `--help`.
Killed inside a minute, directory removed. **The reel's CLI is
`zhao-reel-cel <outdir> [clip ...]` and there is nothing to ask it.**

**Intermediates:** `render-ship` (provably byte-identical to `render-32`) and
`render-3221` (the zero-fill bug — its plate is committed) and `build-face3221`
removed. 3,064 `.rgb` frames remain in `render-base` (the before reference),
`render-32` (the shipped leg) and `render-21`, ~825 MB, deletable with the lane.

**R1 and R3 were NOT executed.** Two lanes were blocked on the ablation answer,
so getting it reported won over one more build cycle; and R1 is an ordered
sequence (widen -> star -> converge -> centre LAST) where each step changes what
the next is judged against, so it is not a thing to start and abandon. The
groundwork plate `R1-eye-now-8x.png` is committed and both premises are
confirmed by eye.
