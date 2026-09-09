# Terrain lighting and vertex lighting are now ONE engine — the D-1 refactor, one level down

2026-09-09, the consolidation pass ordered by
`reports/SHADE-AND-LIGHT-ARE-ONE-ENGINE-20260909.md`. Branch
`zixxtrixx-v8-closeout`, working tree only — nothing committed, per the brief.
Run: `runs/CLAUDE-RUNS/RUN-20260909-2311-shade-light-consolidation/`.

## THE GOLDENS DID NOT MOVE — the acceptance evidence, first

The same six suites, run from the same build tree, BEFORE the reference was
touched and AFTER the split landed. The BEFORE run is from a **clean tree at
HEAD** (`git status --porcelain` empty), so it describes the committed state
exactly.

| suite | BEFORE | AFTER |
|---|---|---|
| `reel_sequence_crc` (zhao-reel --check) | 29 sequences, **all CRCs match** | 29 sequences, **all CRCs match** |
| `render_golden` | all green | all green |
| `render_heightfield` (incl. test_submetre_shading) | all green | all green |
| `terrain_shade_oracle` | 12 checks passed | 12 checks passed |
| `terrain_shade_rtl_directed` | **4,142 checks passed** | 4,142 passed (then 6,656 after the test itself was extended — below) |
| `terrain_shade_break_oracle` (positive control) | FAILS exactly 1 check | FAILS exactly 1 check |

The line-level proof: every `sequence_crc32c` value was extracted from both
runs' `LastTest.log` and diffed —

    diff BEFORE-goldens.txt AFTER-goldens.txt  ->  GOLDENS_IDENTICAL

(both files preserved in the run folder). All 29 sequence CRCs —
terrain waves, scars, breach, both creatures, every sun — are byte-identical.
**That identity is the whole proof of the refactor, exactly as D-1's was.**

## 1. The reference split (task 1)

`reference/src/zrender/terrain.cpp`:

    shade_from_world_normal_unclamped(nx,ny,nz, lx,ly,lz, SatLedger*)   <- NEW core
    shade_flat_tri_dir_unclamped(a,b,c, l, L)
        = shade_from_world_normal_unclamped(face_normal(a,b,c), l, L)   <- now a wrapper

Pure code motion: `ndot` / `nmag2` / the `nmag2==0` arm /
`div_rhu_s128(ndot, isqrt_u64(nmag2))` moved to the core **verbatim** (the
uint64 cast pattern included); the edge/cross/`rescale_s32(.,16)` face-normal
derivation stayed in the wrapper, still recording into the caller's ledger.

Declarations: `reference/src/zrender/internal.hpp` (beside the family) and —
this is the part that unblocks GEOM.LIGHT — **publicly in
`reference/include/zref/zref_terrain_shade.hpp`**, so a block that is GIVEN a
world normal finally has a ratified entry point instead of a standing
temptation to re-derive one. GEOM.LIGHT.md:118's "there is nothing for this
block's oracle to call" is resolved.

**The SatLedger parameter, preserved and explained.** In the pre-split
function the ledger records only in the three face-normal `rescale_s32` calls
— which moved to the wrapper. The core takes the ledger anyway, per the
contract's ratified signature, because (a) every entry point of the law family
carries one, and (b) GEOM.LIGHT's ruled multi-light accumulation ("several
light terms saturate rather than wrap") will need it at exactly this seam. The
core bumps NO counter today: `div_rhu_s128`'s INT32 clamp is the law's own
silent rail, and adding a bump would have CHANGED observable behaviour
(ledger totals) — violating the bit-identical criterion. This is written in
the source, at the parameter.

## 2. The RTL (task 2) — AND THE BRIEF'S PREMISE HERE IS WRONG, loudly

**`zhao_terrain_shade.sv` has no face_normal stage to bypass. It never did.**
Its `n_x/y/z_i` ports carry a Q16.16 un-normalised WORLD NORMAL — the face
normal arrives from `zhao_terrain_normals`, a separate block, which is exactly
GEOM.LIGHT.md's "three different producers feed it" architecture already in
place. The brief's task 2 ("a named parameter or a second input mode so the
face_normal stage can be bypassed") asks for a bypass of a stage that does not
exist; the same sentence appears in
`SHADE-AND-LIGHT-ARE-ONE-ENGINE-20260909.md` ("It needs a named parameter or
a second input mode"), so the error originates there. The correct statement:
**the RTL is already, port-for-port, the hardware of the new core function.**
The world-normal-direct mode is not a second mode — it is the block's ONE
mode.

So no datapath was added — an input mode nobody consumes, guarding a stage
that is not there, would be an uncashed cheque authored on purpose. What task
2's *intent* actually required was evidence, and that is built:

* the default path re-verified UNCHANGED first: 4,142/4,142 and the
  `--break-oracle` control failing exactly 1 — before any test edit;
* tier 2's oracle re-pointed at the **COMPILED
  `shade_from_world_normal_unclamped`** (via the new public declaration) — the
  RTL is now checked against the core function itself over the full port
  domain (rails, LSBs, degenerates, random normal x sun);
* the old piecewise composition (`shade_nmag2`/`shade_ndot`/`isqrt_u64`/
  compiled `div_rhu_s128`) kept as a **drift guard**: every tier-2 point also
  asserts the thin view's pieces still compose to the compiled core, because
  the RTL's internal walk mirrors those pieces;
* result: **6,656 checks passed**, positive control still fails exactly 1.

The only RTL edit is the ownership header (comment-only); the suite and lint
were re-run after it, all green.

**One integration seam recorded rather than parameterised:** `degenerate_i` /
`degen_mismatch_o` is a TERRAIN.NORMALS-specific seam check. A future
non-terrain producer with no degeneracy flag must not tie `degenerate_i` low
and read the resulting mismatch count as a defect. Whether that becomes a
parameter or per-producer flag wiring is decided when the second client
exists, with coverage — noted in TERRAIN.SHADE.md A7.

## 3. Naming and ownership (task 3)

**Decision: the module keeps the name `zhao_terrain_shade` today, and both
contracts now record that it IS the shared lighting core, owned once.**

Why not rename now: the 2026-08-24 arena precedent is "a reusable
parameterized primitive and a shell" — but GEOM.LIGHT's own diagram is ONE
lighting block fed by three producers, not two sibling shells each holding an
instance. The terrain side needs no shell at all (`zhao_terrain_normals` ->
this block is already the composition), so a rename today would touch the fit
ledger row, `design/fit_targets.yml`, the manifest and a just-verified test
for zero new capability, with no second client to name it for. The rename (or
a `zhao_light_*` shell) lands **in the same commit as GEOM.LIGHT's vertex-RGB
RTL**, when there is a real second consumer to bind — and the contracts say
so, so it cannot be forgotten the way the projector dedup was: the deferral is
written at the exact place the next builder must read.

Where the arithmetic lives, exactly once, both stated in both contracts:

* C++: `zref::render::shade_from_world_normal_unclamped` (terrain.cpp)
* RTL: `zhao_terrain_shade.sv` (0 DSP, 1 M10K, fixed 145-cycle engine)

## 4. Contract cross-references (task 4)

* `design/contracts/TERRAIN.SHADE.md` — now names GEOM.LIGHT (was zero
  occurrences): a purpose-section paragraph and **Amendment A7** carrying the
  ownership ruling, the no-face-normal-stage fact, the goldens-unmoved
  evidence pointer, and the degenerate_i seam note.
* `design/contracts/GEOM.LIGHT.md` — header updated (the light-term core is
  built and shared); the "LAW HAS NO ENTRY POINT" section closed with **DONE
  2026-09-09**; a new **OWNERSHIP OF THE ARITHMETIC** section mirroring A7;
  the scalar-reference section now names the core and states precisely what
  remains unwritten.
* `design/blocks.yml` — GEOM.LIGHT notes: stale "BLOCKED on the clamp
  decision" removed (D-1 resolved it), entry point + ownership recorded.
  TERRAIN.SHADE notes: ownership appended. **Maturity fields untouched**:
  GEOM.LIGHT stays SPECIFIED — the V6 gate is doing its job and the colour
  half has no reference to promote on.

## 5. What GEOM.LIGHT still needs beyond the shared core (task 5 — scoped, not built)

The core gives it the per-light signed term. Vertex RGB requires, per the
ruled laws already in GEOM.LIGHT.md:

1. **A reference for the composition** — per light `i`:
   `ndl_i = clamp01(raw_i + normal_detail_i)`; `rgb += colour_i*ndl_i`
   (+ provisional `emission_i*ndl_i`); then `ambient + spill`; ONE final
   saturate. Ruled in prose, **no executable oracle exists**. This is the
   V6 blocker for REFERENCE_COMPLETE.
2. **The environment record seam** — `SetEnvironment 0x0311` (sun dir/colour,
   ambient, tint, fog) delivered per vertex batch; fog factor production
   (D-5: carry unfogged RGB + factor, never a fogged colour).
3. **The multi-light structure** — bounded top-K selection lives in the HPS
   (D-6); this block needs the sequenced accumulator ("K legal lights does
   not mean K light engines") at the skinner's rate, saturating adds.
4. **Reconciling the creature lane.** GEOM.LIGHT.md carries TWO ratified
   arithmetics: this render core (int32 normal, signed unclamped,
   `div_rhu_s128`) and the creature pair `skin_world_normal` /
   `lambert_from_world_normal` (int64 lanes, precomputed magnitude,
   positive-only floor divide, clamp at 65536). They are NOT bit-identical
   laws, and the contract's SKIN.NORM notes hand this block a
   {direction, magnitude} pair the render core does not accept. Which law
   lights creatures in hardware — or whether the engine grows a
   magnitude-supplied mode with its own coverage — is an open contract
   question that must be settled BEFORE the vertex-RGB reference is written.
   (This is the same two-laws-one-quantity shape that caused this whole
   pass; it is now written down instead of waiting.)
5. **The seam that makes any of it visible**: TERRAIN.PROJECT has no colour
   port and the block is `unused` in the manifest — the machine stays unlit
   until that seam exists (TERRAIN.SHADE.md, still standing).

## 6. The one fit gate (named, not run)

**No fit is owed by this pass**: no synthesizable line changed (the RTL edit
is a comment header), so the existing `zhao_terrain_shade` fit row — 0 DSP,
1 M10K, II=147 — still describes the silicon. The ONE gate ahead: **when
GEOM.LIGHT's vertex-RGB shell is composed around this engine, fit that
subsystem once**, and its question is: *does the composed lighting block
(engine instance + accumulator + environment fold) hold the geometry_mantle
Fmax and keep the engine's 0-DSP shape, and what is the accumulator's real
ALM/M10K price?* Fit at the subsystem boundary, not per nodule.

## 7. Gates run on the final tree

* `terrain_shade_rtl_directed`: **6,656 checks passed**; `--break-oracle`
  fails exactly 1 (positive control seen to fire, after every edit).
* `lint_terrain_shade`: pass.
* `reel_sequence_crc` / `render_golden` / `render_heightfield` /
  `terrain_shade_oracle`: pass, values identical to BEFORE (diff above).
* `tools/quartus/check_quartus17_syntax.py`: pass (self-test 3 fire / 6
  no-fire; 220 files; no rejected forms) — run before AND after the edits.
* `tools/quartus/check_prod_manifest.py`: pass (215 modules, every module
  counted once or declared absent) — run before AND after.
* `tools/ledger` `check`: **fails with 3 schema errors that are IDENTICAL at
  HEAD** (blocks/38 extra tests property; blocks/92 maturity_log commit
  pattern, twice) — verified by stashing and re-running at HEAD. Inherited
  from today's other lanes, not introduced here, and not repaired here
  because those entries belong to blocks changed by other sessions today.
* Counters: **no new counter was added anywhere in this pass**, so no new
  mutant is owed; the four existing engine counters keep their exact-count
  assertions and were re-seen green in the 6,656-check run.

## 8. Honest "not verified", instrument by instrument

* **"The goldens did not move" is verified only for what the goldens see.**
  `reel_sequence_crc` (29 sequences), `render_golden`, `render_heightfield`
  exercise the wrapper through terrain and creature draws. The CORE's
  negative/rail domain beyond what triangles produce is covered by the
  6,656-check differential, not by any golden.
* **The new core was NOT diffed against pre-refactor code lifted from git**
  (the skin_normal_lambert precedent used 200,000 such comparisons). The
  instruments here are (a) the unmoved goldens through the wrapper and
  (b) the compiled-core-vs-thin-view-pieces agreement on every tier-2 point.
  I judge those sufficient for pure code motion; a git-lift differential
  would add independence if the reviewer wants it.
* **The moving-sun LOOK gate still stands un-performed** (art law, owner's
  eye at 240p) — inherited open from the RTL session, unchanged by this pass.
* **Quartus synthesizability of the RTL was NOT re-proven here** — the edit
  is a comment, `check_quartus17_syntax` and lint pass, but "lint 0" settles
  one tool's opinion; the block's existing quartus_map evidence is inherited
  from the fit that measured it.
* **The ledger's 3 schema errors**: verified inherited (stash comparison),
  NOT verified as harmless — someone who owns blocks/38 and /92 should look.
* **`terrain_shade_oracle`'s 12 checks**: re-run, but they test the thin
  view's pieces and constants, not the new core symbol — the core is covered
  by the RTL differential's drift guard instead.

## Files changed (working tree, uncommitted)

* `reference/src/zrender/terrain.cpp` — the split
* `reference/src/zrender/internal.hpp` — core declaration
* `reference/include/zref/zref_terrain_shade.hpp` — public entry point
* `tests/terrain/terrain_shade_rtl_directed.cpp` — tier-2 oracle = compiled
  core + drift guard (4,142 -> 6,656 checks)
* `fpga/rtl/terrain/zhao_terrain_shade.sv` — ownership header, comment only
* `design/contracts/TERRAIN.SHADE.md` — cross-ref + A7
* `design/contracts/GEOM.LIGHT.md` — resolution + ownership
* `design/blocks.yml` — both notes, maturities untouched
* `reports/SHADE-LIGHT-CONSOLIDATION-20260909.md` — this report
* run folder `RUN-20260909-2311-shade-light-consolidation/` with
  BEFORE/AFTER golden extracts and TASK_LOG

**NOT mine, present in the same working tree:**
`fpga/rtl/texture/zhao_texture_island_v3_top.sv` and
`tests/texture/island_composed_directed.cpp` were modified at 23:42 by a
CONCURRENT session (RCP24_V3 queue-fault / occupancy-width work) — the tree
was clean at this pass's start (23:11) and those files were never opened
here. Reviewer: do not fold them into this pass's commit. Disclosure: my
ledger stash-comparison at ~23:45 stashed and popped the WHOLE tree, so
their edits were briefly absent from disk for a few seconds; they are
verified intact afterwards (`git diff` shows their full content), but if
that lane had a build reading the tree at that instant it should re-run.
