# THE DOCKET — every outstanding instruction, in one place

Owner instructions arrive as files in `reports/` and are easy to lose between
passes. **This is the index.** Before starting any wave, read this file, then
read the documents it names.

Last swept: 2026-08-31, after merging seven owner-instruction commits
(`73429c0` … `f56e86e`) and adding **D17** (CI) and **D18** (mana territory).

**Rule:** when an item is finished, move it to DONE with the commit that did it.
When a new owner document lands, add it here in the same pass that reads it.

---

## P0 — the console cannot ship without these

### D1. The 100 MHz timing surgery  ·  `reports/MHZArchitected`
**Measured:** `gpu_clk` 53.48 MHz against 100, TNS −6,566 ns
(`reports/composed/renderer-f8c2b32-.../RESULT.md`).

Owner's diagnosis: first-composition timing debt, not existential. Five textbook
18 ns structures named. Execution order is **his**, and it is deliberate —
measure, then cheap broad fixes, then the deep ones, **fitting each step rather
than batching**:

1. registered fit top; export the worst 100 setup paths; keep today's fit as the *before*
2. Early-Z full detection + Edgewalk registered steps / balanced popcount *(low risk, broad TNS)*
3. streamed Edgewalk row + cross pipelines
4. Fragment → read/shade/blend/finish/commit, with a small in-flight address CAM
5. Binner setup sequenced onto two DSPs
6. FBWRITE fixed 32-byte rows, precomputed addresses, ENGINE0-specialised guard
7. shell diagnostic reductions registered; **ENGINE0 route tripwire fixed**
8. targets: ~120 MHz isolated blocks, 110–115 MHz composed
9. **only then** add TMU v2 + cache + TEXJOIN + AUX + Field/Earth to the fit

**Architecture rule adopted:** *latency may grow; initiation rate and exact
arithmetic may not regress.* One multiplier layer per stage; register multiplier
outputs before fabric adders; no wide global reduction driving a large register
bank; no dynamic 64-bit barrel op on a hot path with a fixed protocol; no false
paths, no multicycle constraints, **no seed fishing**.

### D2. Shell route-integrity bug  ·  `reports/MHZArchitected`
The downstream check still rejects any write whose client is not `BLIT_DMA`, but
the guard now legitimately admits `ENGINE0` as framebuffer writer — **so every
real renderer burst latches `shell_err_route_o`**. Should be
`expected_writer = fb_writer_i ? ENGINE0 : BLIT_DMA`. Small, concrete, and a
correctness bug rather than a timing one.

### D3. Fit-top split  ·  `reports/MHZArchitected`
3,214 virtual pin bits and 1,608 ALMs holding them make the current top a poor
characterisation vehicle. Split into `zhao_shell_core` / `..._sim_top` /
`..._fit_top` (registered LFSR sources, chunked MISR sink) / `zhao_board_top`.
Then four controlled fits: core / +binner / +tile pipeline / +full renderer, for
**attribution instead of one before-after mystery**.

### D17. GitHub CI fails every push
> *"github fails all tests right now, we should fix"* — Fabian, 2026-08-31.

Run `33400920093`: **3 of 344 fast tests fail**, and a fourth problem warns on
every job.

| | failure | status |
|---|---|---|
| a | `format_check` — 17 files drifted from the pinned clang-format | **fixed** `fdc57ca` |
| b | six stray gitlinks under `runs/*/work/` with no `.gitmodules`, so every checkout exits 128 | **fixed** `fdc57ca` |
| c | `cppcheck_check` — signed-overflow finding in `render_pipe_directed.cpp` | in progress |
| d | `reel_sequence_crc` — `zhao-reel --check` fails | reproducing |

Note (b) is why the *passing* jobs also printed a red git warning; it is not
cosmetic, it is a committed mistake.

**cppcheck is not pinned as a devDependency** the way clang-format is, so the
local gate can silently skip. That is exactly the failure the standing memory
*"local gates must match CI"* records. Pin it — separate small item.

---

## P1 — the game's main thing

### D4. The 8 km world is unbuilt, not unarchitected  ·  `reports/Missingterrain`
> *"We really need to get to implementing the main thing of our game at some
> point."*

The terrain **format** is architected for a sparse streamed 8 km world; the
**composition** can only process patches a harness hands it. Missing: the world
pager, patch-residency manager, composed-height cache, and command→terrain
pipeline.

Three sizes are being confused and the doc separates them: raster tile 16×16
**pixels**; terrain patch 32×32 cells = **64 m**; island = a sparse directory of
patches in ±32 km fx16. `TERRAIN.LOD` already does ~784 clocks/patch ≈ 2,100
patch decisions a frame against 256 live patches — it was never a one-patch toy.

### D5. Creature presentation lane  ·  `reports/CREATURESANDLIGHTS`
> *"read this one to fill holes, clarify, unify, and show you how it should be
> done"* — this is the unifying document; read it **after** the others.

One lane, collapsing work at the right level: motion per bone on HPS; point-light
geometry reduced per creature/meshlet; lighting accumulated per unique vertex;
toon quantisation once per surviving cel fragment; outlines once per view from an
explicit mask.

**A specification inconsistency to repair before lighting RTL freezes:** the old
law transformed each normal by each bone and blended the scalar Lambert results.
The live reference blends the *normal vector*, renormalises, **then** takes
Lambert — which removed bright patches at mixed-weight joints. **The live
reference becomes the law.**

Also: `GEOM.SKIN` outputs positions, **not normals** — normal skinning is
reference-only. And `GEOM.SKIN` fits at 89.65 MHz with 9 DSPs and one weighted
vertex per 12 clocks; **nothing more may be bolted onto its output.**

### D6. Cape / secondary motion  ·  `reports/CapeProvisions.md`
No cloth processor. Reserve **6 bones** (waist/thigh) or **8** (long, dramatic),
two columns × three or four rows so left and right react independently.
150–300 triangles. Two-weight skinning covers it.

**The one genuinely missing feature: per-instance pose overrides.** `GEOM.POSE`
caches by `{type, clip, frame}`, which is right for armies and wrong for a cape
in wind — a sparse per-instance patch over the shared palette's cape bones.

### D18. Mana territory — the economy  ·  `Upheaval/docs/MANA-TERRITORY.md`
Owner direction 2026-08-31, recorded in full in the **game** repo (`450acc4`).
Wells are taps driven into the island; **claimed terrain conducts mana**;
availability is a **local field**, not a global number; a cell only counts with
a continuous claimed path to a well; **destroyed or newly created terrain begins
neutral**; and spell tier bounds how economically consequential a wound is
allowed to be while terrain decides where it lands.

**Console-side consequence, and it is small:** the claim is *gameplay-grid*
state (`owner_id` + `strength`, coarse, deterministic integer propagation) and
its presentation is **one more terrain material input** — not a lighting model,
not a renderer feature. Nothing here is authorised console work yet; it depends
on **D4** and showcases with **D8**, and **it does not reorder D1.**

Five numbers/questions in that document are explicitly the owner's and must not
be invented.

---

## P2 — stretch, wanted

### D7. Sunder  ·  `reports/SUNDER.md`
> *"A stretch goal, but one we really want. Shouldn't cost too much."*

Mantle can already **represent** the cut: `remaining_top = max(bottom, min(old_top, cut_y))`
for a plane `cut_y = a·x + b·z + c`. Flat nub at `a=b=0`, sloped otherwise, and
the result is still one top per (x,z) so the dual-heightfield likes it.

What it cannot do is turn severed material into an **independently moving
terrain body** — that needs a new runtime/world-object layer, but **not** voxels,
not CSG, not a terrain rewrite.

### D8. Double-helix tornado + site refresh  ·  `reports/DoubleHelixTornado.md`
> *"try to get a render out of it after finishing the 53 MHz and whatever else
> important follows right after."*

**Explicitly sequenced after D1.** Level 9 spell, two tornadoes orbiting a
travelling centre, feet issuing persistent terrain stamps that carve an
intertwining helix. Hybrid — no single subsystem builds the tornado.

---

## P3 — development environment

### D9. Parallel PC + console development  ·  `reports/Future.md`
> *"Please start implement these before you finish the goal. That way the goal
> will remain unfinished until you finish implementing these issues."*

Three parts: (a) PC and console versions that **do not diverge**, PC carrying
online multiplayer, higher resolution, an authentic mode, "the works";
(b) **ZEMU running and playing the game** as the console dev environment;
(c) the programming language — *"past the normal basics it should only grow when
developing the game, but when a feature is needed, it gets implemented in the
language as a first-class feature."*

Note this sits against the standing memory *"hardware first; Nanquan is
provisional — stop compiler overengineering"*. The owner has now asked for the
language to grow **demand-driven from the game**, which is compatible: it grows
only when the game needs a feature.

---

## Carried over from earlier waves

| | item | where |
|---|---|---|
| D10 | depth profiles proved but nothing consumes them; 5 mechanical steps + 1 ABI decision | `DEPTH_PROFILE_NEXT_STEPS.md` |
| D11 | `GEOM.PARAMBUF` — external geometry parameter buffer; supersedes growing the M10K arena | `OWNER-RULINGS-20260831.md` #4 |
| D12 | cel-material **fog ordering** contradicts the general per-vertex law — Class C | `ZIXXTRIXX_CEL_IN_HARDWARE.md` |
| D13 | pose palettes must not live in M10K (1,344 B/pose) | `ZIXXTRIXX_CEL_IN_HARDWARE.md` |
| D14 | `TILESTORE.INK` + `POST.INK` — the hard creature feature | `ZIXXTRIXX_CEL_IN_HARDWARE.md` |
| D15 | seven stub contracts; `MEASURE.HISTOGRAM` deliberately refused | `CONSOLE_REMAINING.md` |
| D16 | TMU target closure | `fpga/rtl/texture/OWNER-DIRECTION-TMU-TARGET-CLOSURE.md` |

---

## DONE

| item | commit |
|---|---|
| `RASTER.ATTRSTEP` — exact stepping, 15.1× fewer divides | `01e8ac4` |
| `RASTER.TOON` — cel band, 4.11 clk/fragment | `17cb574` |
| depth profiles derived and proved | `fa5cbc5` |
| first completed composed fit + numbers | `1d229a9` |
| virtual-pin parity check | `823e703` |
| `terrain_project_chain` regression fixed | `4c76318` |
| worst-path export — FRAGMENT named as offender #1 | `78aee73` |
| CI format tier + the exit-128 gitlinks | `fdc57ca` |
| mana-territory design recorded (Upheaval) | `450acc4` |
