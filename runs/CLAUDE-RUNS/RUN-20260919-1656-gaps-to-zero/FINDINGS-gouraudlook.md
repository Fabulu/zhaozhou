<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
6ff6d2d7 -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- GOURAUDLOOK


## `6ff6d2d7` -- FINDINGS-gouraudlook (the harness refuses a FINDINGS file; this commit IS it)

```
FINDINGS-gouraudlook (the harness refuses a FINDINGS file; this commit IS it)

Packet GOURAUDLOOK, owner ruling R230. Branch gz/gouraudlook from
claude/ceiling-architecture-20260912 @ 0effc5d7. Work commit: dcd65f92.

No RTL changed. No Quartus run. Nothing published. I present; I do not rule.

================================================================
HOW OFTEN DOES IT MATTER -- and a correction to my brief
================================================================

Over 3,168,243 drawn creature triangles, 8 subjects, 2,385 frames:
100.00% take the Gouraud path. The natural flat path is taken ZERO times.

  zixxtrixx-idle          576 fr   844,595 tris   100.00% lit corners
  zixxtrixx-front         576 fr   922,227 tris   100.00%
  zixxtrixx-attack        588 fr   584,439 tris   100.00%
  zixxtrixx-side          192 fr   417,792 tris   100.00%
  zixxtrixx-knockdown     196 fr   206,943 tris   100.00%
  zixxtrixx-walk          160 fr   172,275 tris   100.00%
  creature-wave-walk       96 fr    17,796 tris   100.00%   (the watchdog)
  zixxtrixx-still-front     1 fr     2,176 tris   100.00%

MY BRIEF'S INFERENCE FROM THIS IS INVERTED, and it points the wrong way. It
said: "the flat path is taken whenever any vertex is unlit. If that is rare,
the stand-in is nearly always the same picture; if it is common, it is not."

It is the other way round. Unlit corners being RARE means Gouraud is taken
nearly always, so a flat stand-in differs on nearly EVERY triangle. Here rare
is ZERO -- both creature meshes carry compiled normals throughout -- so the
stand-in changes every drawn triangle of every frame. There is no "it only
matters in unusual poses" escape available.

The trajectory plots say it in the other axis: disagreement is a FLAT LINE at
7-10% of frame area across all 192 frames of zixxtrixx-side, never dipping.
A flat line IS the finding.

================================================================
ARE THEY DISTINGUISHABLE AT 240p? -- yes, plainly
================================================================

True 384x240, whole frame, so bit-identical sky and terrain DILUTE these
numbers; the creature's own pixels move far more.

                        vs flat face-Lambert    vs flat provoking-vtx  peak
  zixxtrixx-side        8.70% (worst 9.57%)     7.14%                  124/255
  zixxtrixx-idle        7.27% (worst 9.45%)     6.18%                  149/255
  zixxtrixx-front       6.16% (worst 8.79%)     5.14%                  140/255
  zixxtrixx-walk        5.34% (worst 5.76%)     4.70%                  140/255
  zixxtrixx-still-front 4.70%                   3.82%                   82/255
  zixxtrixx-knockdown   3.32%                   2.84%                  124/255
  zixxtrixx-attack      2.85%                   2.52%                  132/255
  creature-wave-walk    1.28%                   1.21%                  140/255

IN MY OWN WORDS, FROM LOOKING AT IT:

* Gouraud reads as A ROUND ANIMAL. The body is a continuous tube, the neck
  loop is a cylinder, the blue-to-green transition sweeps.
* Face-Lambert flat reads as A FOLDED RIBBON. The neck loop becomes a stack of
  distinct plates; the belly becomes a flat green slab with a hard edge. It is
  clearly a different object -- but it is COHERENT, and could be defended as a
  deliberate faceted style.
* Provoking-vertex flat reads as a round animal WITH A RASH. The sawtooth is
  not a style, it is noise, and it is the reading I would expect to be
  rejected on sight. It is also the only one that is free.
* THE WATCHDOG IS THE MOST BRUTAL CASE DESPITE THE LOWEST PERCENTAGE. It is a
  smaller, blockier mesh and flat turns it into a crate with a visible hard
  cylinder end-cap. Low number, high ugliness: THE NUMBER AND THE LOOK
  DISAGREE AND THE LOOK IS RIGHT. This is precisely why R230 refused to settle
  it from the DSP column, and it is CLAUDE.md rule 7 with a worked example.
* THE CLOSEST PAIR IS zixxtrixx-still-front -- textured, head-on, surface
  facing the camera. Texture detail masks shading and at true size these two
  are near a coin toss. Where the texture is dense and the surface faces you,
  the stand-in is very nearly free. It falls apart on curved, grazing-angle,
  side-on silhouettes, which is most of gameplay.

They are distinguishable at final resolution, comfortably, on every subject.
Whether that is worth 21% of the DSP budget is the owner's call, and the
boards are built so it can be made in one look.

================================================================
WHERE THE PICTURES ARE
================================================================

C:\programmieren\zencrifice\gz-gouraudlook-evidence\   (71 MB, on disk)

  gouraud-vs-flat-face\BOARD-true-384x240.png   START HERE. Every subject's
        worst frame, three columns, true console output size.
  gouraud-vs-flat-face\BOARD-magnified.png      Same pixels, NEAREST magnifier.
  <subject>\pair-rank01..06-*-frame-x1.png      worst-N frames, whole, true size
  <subject>\pair-rank01..06-*-crop-x5.png       same, cropped by badness, x5
  <subject>\flip-frame-x1.gif, flip-crop-x5.gif A/B FLIPBOOKS. If one artefact
        settles this it is these: the eye cannot compare across a gap but is
        very good at noticing something change under it.
  <subject>\contact-all-frames.png              EVERY frame, Gouraud stacked
        directly over flat so the pair is one cell and the comparison is a
        short vertical saccade.
  <subject>\trajectory.png                      disagreement over time
  <subject>\frames.csv, summary.csv             the per-frame numbers
  gouraud-vs-flat-pv-a\                         the same set, provoking-vertex

Committed, small and few, under reports/gouraudlook/: both boards, the
sawtooth proof, two summary CSVs. 984 KB.

================================================================
GATES
================================================================

I changed no RTL and no ports, so most of the gate set CANNOT CHANGE ITS
ANSWER (R227). Run:

  completion_register.py    MANDATORY GAPS REMAINING: 22 (RC 1, normal).
                            The campaign's number; I must not raise it and did
                            not, having added no RTL.
  refmodel_liveness.py      RC 0. 96 declarations, 90 resolve, 6 unresolved
                            (MEASURE.HISTOGRAM, PART.COLLIDE/SPAWN/STATE/
                            UPDATE, POST.COMPOSITE), self-test OK. NAMED IN MY
                            BRIEF because I touched the oracle's neighbourhood.
                            No block declares a reference_model in
                            creature_sim.cpp, so my symbols add no unresolved
                            claim -- and the oracle path I added IS exercised,
                            by the reel_shade_lane ctest.
  duplicate_functions.py    RC 0, no new entries. The stand-ins reuse
                            creature_light and shade_flat_tri_dir; no second
                            implementation of the lighting law.
  mutant_copy_drift.py      RC 0, run AFTER the commit per R121. 57 copies
                            checked; OK. I cut no mutant copy.

-Wall -Wextra -fsyntax-only on creature_sim.cpp: the sole warning is
pre-existing (zref_sky.hpp:107, shift of a negative value), not mine.

NOT RUN, WITH REASON: every Quartus/RTL gate -- check_console_inventory,
check_prod_manifest, gen_prod_top --check, gen_console_board --check,
check_quartus17_syntax, run_console_core_smoke and its four inverted controls,
gen_shell_paired_diff --check, check_case_labels, npm run abi:check. I changed
no .sv, no port, no spec/commands.zidl. Running Quartus is forbidden by
PACKET-PROTOCOL.md, which wins over any brief including my own.

On the %Fatal nuance: -BadDescriptor passes WITH one %Fatal by design -- the
bench is supposed to die and the %Fatal is the evidence. I did not run it, but
a blanket "%Fatal must be 0" sweep reds a working control.

================================================================
CITATIONS I HAD TO CORRECT
================================================================

1. My brief's "how often does it matter" inference is INVERTED (above).
   Unlit-is-rare means the stand-in differs MORE, not less.

2. My brief framed this as TWO readings; it is THREE, and the third
   (provoking-vertex) is the only one that is actually free. Comparing against
   face-Lambert alone would have flattered the cheap option -- the
   mismatched-poses error wearing new clothes.

3. FINDINGS-attrlane.md DOES NOT EXIST and is in no commit, yet it is cited as
   where the cost is worked through by BOTH zhao_geom_attrpack.sv:170 AND
   zhao_raster_tile_pipe_v2.sv:694. The ~1,420 ALM / +24 DSP numbers survive
   only in R230 and in those RTL comments. That is a dangling citation in
   production RTL; somebody should write the file or repoint the comments.

4. My brief's own code citation WAS correct and verified in-tree:
   reference/src/zcreature/creature_sim.cpp carried
   gouraud = a.lit and b.lit and c.lit at line 914 before this packet touched
   it.

================================================================
LANE HYGIENE
================================================================

File sets stayed disjoint from POSECMD and TERRCMD: I am in reference/,
tools/, tests/CMakeLists.txt and reports/gouraudlook/; they are in spec/,
fpga/rtl/ and the core. The only shared file is tests/CMakeLists.txt, where I
appended one add_test block and touched nothing else. git diff --cached
--name-only was read before committing and contained exactly my ten paths.

1.9 GB of .rgb intermediates were generated and PURGED. .gitignore has covered
*.rgb since 2026-08-28, which makes them invisible to git and does nothing
whatever about the disk -- CLAUDE.md's own "a rule that HIDES waste is not a
rule that removes it". They are gone; 71 MB of PNG remains.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
