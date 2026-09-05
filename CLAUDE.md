# Zencrifice — working rules

Three repos: **zhaozhou** (the console — silicon, reference oracle, tools),
**nanquan** (the language), **Upheaval** (the game, full title *Tribute
Upheaval*; the folder stays `Upheaval/`).

---

## WHEN MAKING MODELS, ANIMATIONS AND TEXTURES, WE ARE MAKING ART

**Measurement never trumps actually looking at things.**

Fabian, 2026-08-26, after a creature was rebuilt from measurements and came out
worse than the version authored by eye.

This is not a caution about being careless with numbers. It is the opposite
failure: a measured number *feels* like evidence, so it stops getting
questioned, and it quietly replaces the judgement that was supposed to be doing
the work. Three things went wrong in one pass, all the same error:

* **The body taper** was derived from a distance transform of the concept
  drawing. That is not the 3D body radius — it is the half-width of a drawn
  outline in the picture plane, conflating real thickness with foreshortening,
  with measuring across a *bend* instead of across the body, and with the places
  the shape overlaps itself. It was trusted precisely *because* it was "a
  measurement". The hand-authored version looked better.
* **The dorsal pink** was measured off the scan and shipped as "the sheet's own
  pigment". But a scanner flattens and lightens, and a pale rose that reads as
  pink on a white sheet under room light reads as grey-white on dark ground at
  240p under one key light. Matching the paper is not matching the READ, and the
  read is the thing. The earlier, "wrong", hand-saturated value was closer.
* **The crayon grain** was clipped to ±16% of luminance — narrower than the
  light rig's own range, so it was mathematically present and visually invisible.
  It measured fine and looked like flat plastic.

**The rules that follow:**

1. **Author by eye. Render. Look. Compare. Adjust.** That loop is the job.
2. **Measurement belongs on the COMPARISON side**, checking what was authored
   against the reference — never on the generation side. A tool that says "you
   are 20% off" is good; a tool that decides a radius is not.
3. **Measure things that ARE the thing, never a projection of them.** A pigment
   is a pigment. 3D form derived from a 2D drawing is not.
4. **Measurement can remove a BIAS; it cannot choose a VALUE.** Finding that a
   sampling method skewed pale is useful. The shipped colour is still chosen by
   looking at it in scene, at final resolution, against what it sits on.
5. **A drawing is an interpretation, not a scan.** The artist thickened forms
   where they needed visual weight and reached for the crayon that was in the
   box. Converting that mechanically keeps the artefacts and loses the intent.
6. **Never remove the owner's control in the name of fidelity.** Every shape,
   colour and timing value belongs in a named, editable constant. "This is
   generated from the reference, so it is not a knob" is how a wrong number
   becomes an unadjustable wrong number.
7. **Component checks passing is not likeness evidence.** A palette verified
   against the sheets, a light rig verified against a face table and a mesh
   verified by CRC can all pass while the creature is unrecognisable. Look at
   the whole thing, in motion, against the concept.

## Two more ways a measurement lies

Added 2026-08-28, from the first creature. Both are the art law's failure mode
wearing new clothes, and both cost days.

**A measurement across MISMATCHED POSES measures the pose.** A ratio said the
creature's tube was "a wire — 22% of loop height against the drawing's 40%", so it
was thickened substantially. That comparison put our grounded gameplay stance
against the sheet's aerial S: different pose, different foreshortening, different
loop. A later overlay that laid our radii along **the sheet's own traced pose**
showed we were **2x too thick** — we had moved the value confidently in the wrong
direction, and the owner's eye had said "too broad" before the tools did. Compare
like with like, or do not compare.

**A gate passing is not the thing looking right.** A snout measured horizontal
still hung from the bottom of a downward hook. Every automated gate passed while a
stray triangle sat in a creature's eye. Fixing a genuine rotation-wrap bug did not
turn a spin into an elegant wheel, because the fault was the shape changing during
the rotation, not the interpolation. Gates catch regressions; only looking catches
wrongness.

## A broken instrument lies in ONE direction

Added 2026-09-04, after nine separate measuring tools were found wrong in a
single session. Every one of them had the same property, and it is the reason
none had been caught:

**The defect always made the answer look BETTER, SMALLER or SIMPLER than the
truth.** A parser that silently drops what it cannot match reports fewer
problems. A self-check whose pattern matches nothing reports "no silent drops".
A rule written after the fit it governs reports a pass. A `status` field holding
the last *good* run reports `ok`. A count compared against a stale measurement
reports no change.

That asymmetry is the whole lesson. **Nobody audits good news.** A tool that
reported too many problems would have been fixed the first afternoon; a tool
that reports too few is trusted for weeks. So:

1. **A number that is exactly zero is a broken instrument until proven
   otherwise.** "0 counters match their port" was a regex with no provision for
   a width bracket. "0 arrays declared" was `^` without `re.MULTILINE`. Precision
   at zero is a tell, not a result.
2. **A detector that has not been shown to FIRE has not been tested.** Break it
   on purpose, watch the alarm go off, put it back. Three tools here now assert
   at import that their own pattern still matches a known-good example, because
   one self-check was written with a word-boundary escape that a shell heredoc
   turned into a literal backspace character — so it matched nothing and
   printed reassurance for its whole life. (This sentence lost its own escape
   the same way on first writing, which is either evidence or comedy.)
3. **Check the heuristic against a case you can verify by hand before believing
   the total.** A block-id-to-module rule said 25 blocks were unbuilt; three of
   them existed under a name the rule did not construct.
4. **Never compare a current file to an old measurement.** Declared-versus-
   measured is a real check; declared-today versus measured-a-week-ago is a
   different question wearing the same shape, and it produces confident
   nonsense.
5. **When a tool explains itself, the explanation is a claim too.** A rule that
   fired saying "state that belongs in memories is in flip-flops" was right that
   the ceiling was breached and wrong about why — the payload was already in 13
   M10Ks. A wrong diagnosis attached to a right alarm sends the next person to
   reshape something that is already correct.

This is the same law as *measurement never trumps looking*, one level up: the
tool that does the measuring is itself a thing that has to be looked at.

## Instructions are not delivered until they are read

Owner direction was posted four times because it kept not reaching the working
agent, then relayed into a run folder that had already closed, and five passes
solved the wrong problem in the meantime. **A run folder is the wrong home for
anything durable** — every pass creates a new one, so a file left in the current
run is orphaned by the next. Durable direction belongs beside the creature it
governs. And before starting any creature run, read every `OWNER-DIRECTION-*.md`
in the creature folder and check `reports/` for anything newer than the last run.

## Stopping an agent does not stop its background work

A stop instruction was sent and obeyed, and a build it had already launched ran to
completion anyway. **Kill the background tasks too**, then verify nothing is
running before assuming a lane is closed.

## Seeing the work properly

Judging an animation from a handful of evenly-spaced stills does not work —
uniform sampling finds the typical frame and misses the broken one. Prefer:
contact sheets of every frame; trajectory plots of tracked points over time
(a flat line IS "it never bobs"); sampling frames by *badness* rather than by
index; fixed orthographic diagnostic cameras; and before/after pairs so
regressions are visible.

## Ground contact

Clipping through the ground must be **authored, never accidental**. A belly
resting at exactly zero reads as hovering, and a spear that stops at the surface
reads weightless — so deliberate, declared penetration is correct and its
*absence* is also a bug. Each clip declares where, when and how deep; anything
outside that is the fault.

**Measure it with a COMMITTED 3D pose probe, not from the rendered frame.**
The obvious shortcut -- find the terrain in the image and count creature pixels
below it -- is unsound and will report a confident, wrong number: with a ground
plane receding in perspective, an animal STANDING on the ground is always below
the horizon, and one 2D frame cannot separate "in front of the dirt" from
"inside it". It reported 94.8% submerged on a clip that was barely touching.
Real penetration means walking every posed vertex of every clip frame against
terrain height. A probe that does this was written once and thrown away, so its
numbers are unreproducible -- commit the probe.

---

## Process

* **Every session is a RUN**, created with `zhaozhou/runs/CLAUDE-RUNS/init-run.ps1`,
  logged as it happens, archived when done. **Every creature gets its own run.**
* **Commit and push as the work happens**, not batched at the end.
* **Barge ahead.** Decide on your own judgement rather than stopping to ask;
  state the assumption, make it cheap to reverse, keep moving. Blocking costs a
  working session; a wrong call costs one edit. This does not extend to
  destructive or outward-facing actions.
* **Publishing is always an explicit call** — with ONE standing exception.
  `Upheaval/website/deploy.ps1` requires `-Project` and `-Branch` and refuses a
  page without `noindex`.
  * **The bestiary is authorised to publish on every FINISHED CREATURE PASS**
    (Fabian, 2026-08-27: *"publish on every finished zixxtrixx, not every
    individual change"*). Do not stop to ask — but the trigger is **a pass that
    is done and worth looking at**, not a file save. A tweaked constant, a
    half-fixed head, a re-render mid-iteration: not a publish. The complete
    reworked creature with its clips encoded: publish, immediately, via
    `deploy.ps1 -Project upheaval -Branch main`.
  * Site-structure work (a card layout, an archive tab) ships with the next
    creature pass rather than on its own, unless Fabian asked for that change
    specifically — then it goes up when it is done.
  * The exception is that site and nothing else; it does not generalise to other
    outward-facing actions, and the page stays `noindex` — unlisted, for the
    owner, not public.
  * **`-Branch` is mandatory.** Wrangler accepts a missing branch, silently
    demotes the deploy to a PREVIEW, and production keeps serving the old build.

## Build note

`cmake --build` intermittently loses a race regenerating `build.ninja` against
Verilator, and the shell then runs the **stale binary and reports the old
numbers**. A measurement that did not move after a change that must have moved
it is the tell. Compile the reel directly instead, and after any struct-layout
change **recompile every `.cpp` that uses it** — a stale object with an old
layout looks exactly like a rendering bug.

**Configure from PowerShell with `tools/env/zhao-env.ps1` sourced, always.**
`CMakePresets.json` gates `windows-base` on `${hostSystemName} equals Windows`.
Run `cmake` from Git Bash and it resolves to the MSYS cmake, which reports a
non-Windows host, so the preset is **disabled** — *"Could not use disabled
preset windows-native"*. Configure without the preset instead and
`CMAKE_CXX_COMPILER` comes back as the string `C`. The preset's own `ZHAO_NOTE`
says this and warns about "the broken devkitPro msys2 cmake"; it still cost an
hour on 2026-09-04 because the symptom looks like a corrupt build tree rather
than a wrong shell.

**When `build.ninja` is stale it can be unable to regenerate itself.** One
verilate rule declared `Vtb_perspuv_pair.cmake` among its outputs while running
`--make json`, which writes the `.json` that is actually there — so a
`copy_if_different` of a file nothing writes failed on every build, and because
that rule is part of `build.ninja`'s own regeneration, **ninja could not rebuild
the graph that would have fixed it**. 255 of 256 verilate directories had their
`.cmake`; the one that did not was the one holding everything. The fix is to
regenerate through `cmake --preset`, never through another `cmake --build`.

**Read the build's exit code, not the pipeline's.** `cmake --build ... | tail`
reports `tail`'s status. A build that failed on step 18 of 554 printed
`BUILD_RC=0` and was believed. This is the stale-binary trap wearing a new
costume: the shell told the truth about the wrong thing.

**`std::ofstream` faults at -O1 on this toolchain; use C stdio.** Measured
2026-09-05 writing the desktop host: an `ofstream` write crashed with an access
violation at `-O1` and `-O2`, and worked at `-O0`. Same for `ifstream` on read.
It was bisected by flushing stdout after every step -- the summary printed, the
byte count printed, and the process died inside the stream write. **Buffered
output lost in a crash makes a late fault look like an early one**: with no
flushes it presented as "no output at all", which points at start-up rather
than at the last thing the program does. `fopen`/`fread`/`fwrite` have no such
problem and the codebase already uses `std::printf` everywhere.

**Regenerate `zhao_prod_top.sv` after ANY port change.** It is generated by
`tools/quartus/gen_prod_top.py` and instantiates every production block by
name, so a new port that nobody connects is a `PINMISSING` that only the next
fit discovers. On 2026-09-04 it was found stale for two separate port changes
made days apart. A generated file that nobody regenerates is a stale file with
a reassuring provenance line at the top. `tools/quartus/check_prod_manifest.py`
now also checks that everything the top instantiates is in the production fit's
source list — registering a block in the ledger, the manifest and that list are
three different acts.
