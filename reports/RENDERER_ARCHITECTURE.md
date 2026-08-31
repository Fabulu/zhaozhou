# The renderer — owner rulings and the architecture to build

Relayed from the reviewer via Fabian, 2026-08-30, after the render path crossed
from "a set of disconnected blocks" into a working seam
(`ff01661`, `3539603`, `5321530`). Written down here, beside the reports rather
than in a run folder, because it is the plan for everything after the first
pixel and it will be a while before all of it is built.

**This file is a DIRECTION, not a record of work done.** Nothing below is
implemented unless a commit says so.

---

## RULING 1 — the framebuffer writer lease

The question asked in `RENDER_PATH_SHELL_INTEGRATION.md` was: should the render
engine share DEBUG.FRAMEBLIT's framebuffer window, or own a second region-map
entry?

**Answer: share the window. Do NOT create a second framebuffer map entry.** But
do not land the unconditional one-liner either.

> **Share the spatial window, not the temporal permission.**

Both writers target the same physical object — an inactive RGB565 framebuffer
slot. A second overlapping region entry would duplicate the same address law,
cost more policy plumbing, and *still* not stop the two writers corrupting each
other. The repository already has the right mechanism: **VIDEO.SLOTMGR owns a
single framebuffer lease with slot and generation, and formally guarantees one
lease at a time.** Generalise that lease to identify its writer.

    lease = { valid, slot, generation, owner = BLIT_DMA | ENGINE0 }

    ZHAO_CLIENT_SCANOUT:
        pass_ok = shape_ok && !req.write && scan_ok;

    ZHAO_CLIENT_BLIT_DMA:
        pass_ok = shape_ok && req.write && lease_valid
               && lease_owner == FB_WRITER_BLIT && fb_window_ok;

    ZHAO_CLIENT_ENGINE0:
        pass_ok = shape_ok && req.write && lease_valid
               && lease_owner == FB_WRITER_ENGINE0 && fb_window_ok;

    default:
        pass_ok = 1'b0;

Rename `blit_slot` / `blit_span` / `blit_ok` to `fb_write_slot` /
`fb_write_span` / `fb_window_ok`. **The region itself is unchanged and still
map-gated.**

**A v1 frame uses the hardware renderer or DebugFrameBlit, never both.**
Encountering both writer classes faults the frame: the inactive slot is drained
and released dirty, and never published.

Move the law **atomically** across: `zhao_mem_guard.sv`, `zref_mem.hpp`,
`formal_mem_guard.sv`, `mem_guard_directed.cpp`, `memory_rules.md`,
`MEM.GUARD.md`, and the slot-manager RTL/oracle/formal tests. Add a non-vacuity
cover for a legal ENGINE0 forward and negative cases for owner mismatch.

### This corrects my own note

I wrote that two writers in one frame are "CMD.SCHEDULER's problem". The clean
authority split is:

| authority | owns |
|---|---|
| CMD.SCHEDULER | selects and dispatches the writer |
| VIDEO.SLOTMGR | the framebuffer lease and its generation |
| MEM.GUARD | the selected writer's capability and address bounds |
| the writer | proving all its memory traffic retired before publication |

---

## RULING 2 — one lifecycle per TILE, not per triangle

`RENDER_SEAM_FINDINGS.md` measured that a tile two triangles share is rendered
twice and the second job's clear erases the first triangle. It listed three
options. **Take option 1: accumulate all of a tile's triangles in the tile store
before resolving.**

So the binner's drain stops being a stream of isolated jobs and becomes a tile
protocol:

    TILE_BEGIN { tile origin, tile index, clear/background word }
    TRI_REF    { triangle_context_id }        // zero or more, submission order
    TILE_END

Per active tile: clear the front bank **once**, run every triangle reference in
the tile's FIFO order, wait for edge/fragment/TMU/AUX work to drain, swap banks,
**resolve exactly once**, and rasterize the next tile into the other bank while
this one resolves. The ping-pong store was built for that overlap, and the
binner already preserves submission order within a tile because terrain's
painter order is semantically observable.

**Visit every active tile once, even one with zero triangle references** — that
is what stops the previous frame showing through an untouched tile. The sky
prefill later overwrites the clear almost everywhere; the clear stays the
fail-safe.

Keep a compatibility wrapper translating an old one-triangle job into
`TILE_BEGIN / TRI_REF / TILE_END`, so the existing exact unit tests survive the
composition change.

---

## RULING 3 — a real frame-render transaction

There is no frame controller. **`zhao_raster_fbwrite` considers a row written
once its beats are ACCEPTED, and receives no arbiter retirement credits at all;
its `frame_end_i` merely resets counters.** A slot must never become READY while
writes still sit in a FIFO, an arbiter or the SDRAM controller.

Build RENDER.FRAME (do not overload VIDEO.FRAMECTL):

    acquire framebuffer lease
      -> begin binner/frame arenas
      -> accept all draw work
      -> close geometry submission
      -> drain tiles
      -> drain TMU / AUX / fragment / resolve
      -> drain FBWRITE
      -> WAIT UNTIL EVERY ENGINE0 WRITE HAS RETIRED
      -> publish slot if clean, else release it

Use the already-ratified DEBUG.FRAMEBLIT transaction law: accepted is not
retired; failure stops new side effects; issued traffic drains; a dirty inactive
slot is acceptable; publication requires every intended byte retired; release
and publication carry the lease generation.

`zhao_raster_fbwrite` therefore needs `retire_words_i`, `issued_words_o`,
`retired_words_o`, `drained_o` and `fatal_error_o`. **A guard denial or a
stream-contiguity error must make the frame unpublishable** — today's behaviour
(drop the refused row and continue) is right for the standalone seam test and
wrong for a production frame.

---

## RULING 4 — a TriangleContext, so edge setup is computed once

GEOM.SETUP computes edge coefficients once; the binner consumes them for its
corner reject but stores only vertices and a source id; **RASTER.EDGEWALK then
recomputes edge setup for every tile reference.** A triangle touching 30 tiles
pays setup 30 times for coefficients that are tile-independent.

    TriangleContext {
        edge kx/ky/kc[3], top_left[3], area,
        attribute-plane coefficients,
        fragment state, texture/material binding, lod, source id
    }

Setup writes one context; binner tile lists store only `triangle_context_id`;
the raster drain fetches the context once per active tile; edgewalk receives
prepared coefficients instead of raw vertices.

This is also the seam that has to scale. **The binner holds 128 triangle records
and 1,024 tile references** — enough to prove its laws, not a credible ceiling
for terrain, sky and hundreds of visible creatures and objects on 8 km maps.
Keep its safe overflow wall, make the capacities build parameters, instrument
real scenes, then fit two meaningful capacity points rather than guessing.

---

## RULING 5 — "add gradients to GEOM.SETUP" understates the change

My ordered list called step 3 "GEOM.SETUP attribute gradients". The real blast
radius:

* GEOM.PROJECT outputs only screen X/Y, Q16.16 1/w, a behind flag and a source id.
* GEOM.CLIP explicitly handles **no** attributes.
* GEOM.SETUP has **no** attribute input.
* The binner has nowhere to store attribute state.
* The tile pipe carries flat depth, colour, alpha and texel.

Define ONE attribute-bearing vertex packet through projection, assembly and
clipping:

    screen_x, screen_y, invw24,
    u_over_w, v_over_w,
    lit_r, lit_g, lit_b, alpha, behind

Per-triangle state carries material/binding, explicit LOD, recipe and source id.
**When clipping flips B and C to normalise winding it must swap all B/C
attributes with them.**

Keep the current clean edge block and add a companion **GEOM.ATTRSETUP** rather
than inserting a tagged divider service into the existing three-cycle edge
setup. Flat and untextured triangles bypass it.

---

## RULING 6 — use the ratified plane-equation law, not the software stand-in

The written numeric law says: interpolate `invw24` by plane equation, interpolate
`u_over_w` and `v_over_w` by plane equation, recover U/V per surviving pixel
through the exact reciprocal path.

The software raster instead computes one X gradient and re-evaluates a full
rounded barycentric row start every scanline, and still uses **affine** U/V in
the older terrain stand-in. Fine for a software bootstrap; **it must not silently
become the silicon law.**

    ATTRSETUP:  origin, d/dx, d/dy for invw24, u_over_w, v_over_w, colour, alpha
    RASTER.INTERP:
        evaluate the planes at the tile's first pixel centre
        step d/dy per row, d/dx per column

Then the ordering that pays:

    covered fragment
      -> affine invw24 available immediately
      -> EARLY-Z
      -> only survivors pay reciprocal + U/V recovery
      -> only survivors request textures

That is Early-Z's whole purpose: reject before texture and tile-memory cost.

The attribute-plane divider/reciprocal is a **tagged service with a measured
initiation rate**, not one divider per lane. Start with the three fields a
textured triangle needs — `invw`, `u_over_w`, `v_over_w` — then add Gouraud RGB
and alpha through the identical service.

---

## RULING 7 — TEXJOIN owns the texture systems

    survivor + affine attributes
      -> context FIFO
      -> perspective U/V
      -> primary TMU request
      -> optional AUX request IN PARALLEL
      -> ordered rejoin
      -> existing RASTER.FRAGMENT

The context record holds tile/pixel address, depth, fragment state, lit
colour/alpha, material binding, LOD, source id and an **internal sequence
number**. Never use the external source id as transaction identity.

**Primary TMU and TEXTURE.AUX must run concurrently for terrain.** AUX sustains
one request per six clocks — 277,778 a frame against the 276,480 terrain-primary
estimate — so it has effectively no reserve and is an independent renderer
blocker in its own right.

---

## What the current renderer actually proves, stated precisely

The path GEOM.BINNER → RASTER.TILE_PIPE → RASTER.FBWRITE → guard model → VRAM
model writes exact framebuffer pixels under normal *and* stalling memory. The
row-burst decision is right: a 16-pixel RGB565 tile row is 32 contiguous bytes,
one natural guarded burst, without spending 512 bytes on a whole-tile buffer.
The explicit contiguity check and per-seam counters already found two bugs that
would otherwise have looked green.

**But the overlapping-triangle result does not mean two triangles compose
correctly.** It means the test exactly reproduces the present machine, in which
a shared tile is rendered twice and the second clear erases the first triangle.
What exists is an excellent exact rendering seam and memory writer — **not yet a
correct frame renderer.**

---

## STATUS, 2026-08-31 — where the nine steps actually stand

Written against the ordered list below, so the two can be read together. Every
clock figure here is a measurement from a directed test against running RTL.

| # | step | state |
|---|---|---|
| 1 | framebuffer-writer lease | **done** — one region, one dynamic owner |
| 2 | wire the flat renderer into the shell | **done** — `client_req[2]`, 3 guards |
| 3 | renderer frame transaction | **done** — publish only on `drained_o` |
| 4 | tile lifecycle | **done** — one lifecycle per TILE |
| 5 | TriangleContext + arena | **priced, not built** — see below |
| 6 | attribute-bearing geometry seam | **done bar one spec hole** |
| 7 | TMU v2 + perspective + TEXJOIN | **done** |
| 8 | pipeline AUX, run it concurrently | **concurrency done**, AUX block open |
| 9 | real traces, size the capacities | **open, and now blocking** |

### Step 6, in detail

| block | what it does | gate |
|---|---|---|
| `zhao_geom_attrsetup` | the numerator plane, no divide | 21/21 |
| `zhao_raster_attrdiv` | the divide, exact rounding | 12/12 at each radix |
| `zhao_raster_attrdiv_svc` | N dividers, tagged, in order | 11/11 at 8 grid points |
| `zhao_raster_attrinterp` | steps the plane onto pixel CENTRES | 11/11 |
| `zhao_geom_clip` | attributes follow the winding flip | 13/13 |
| `zhao_raster_rcp24` | `rcp_u24`, full-domain hash | 9/9 |
| `zhao_raster_perspuv` | the perspective divide | 12/12 |
| `zhao_raster_texjoin` | context FIFO, sequence, ordered rejoin | 19/19 |

The one hole is **GEOM.PROJECT's attribute carry**, blocked on `wmin`, `wmax`
and `scale`, which have no value anywhere in the repo. See
`reports/OPEN-SPEC-DEPTH-QUANTISATION.md`. It is a decision, not a task.

### Step 5, priced

Ruling 4 is right that setup is repeated per tile reference. Measured:

* **setup is 5 clocks a job** (`raster_edgewalk_setupcost`), derived so the
  derivation carries risk and cross-checked against the degenerate path;
* **but a context cache removes ONE of those five, not five.** The five are five
  states: `S_AREA` drives the area, `S_W0` lands it *and* drives w0, then
  `S_W1/2/3` land the three edge values. Prepared coefficients make the area
  free — it is `kc0 + kc1 + kc2` — but each edge value still needs its own pass
  through the shared cross unit. Only `S_AREA` disappears;
* so the saving is `(refs - tris) x 1`: **0.02% on the sky, 0.28% on an army,
  1.53% on the giant**;
* the cost is 3x the per-triangle record — 36 Kbit at the shipped `TRI_CAP`,
  8.2 Mbit at the capacity an army actually needs.

**Recommendation: do not build it.** 1.5% of a frame on a pathological scene,
0.3% on a real one, for triple the structure that is already the frame's
capacity wall. The 16-row walk is *sixteen times* what the cache saves; if
edgewalk is ever the thing to shorten, that is where the clocks are.

**I priced this wrong twice, both times too high.** First at 32%, by measuring
accept-to-first-coverage-beat without noticing the 16-row walk sits inside that
window. Then at 7.67%, by assuming a cache removes the whole setup when it
removes a fifth of it. Both corrections are in the test's own header and both
were found by looking at the block rather than at the report.

### What the per-pixel path costs, all measured

See `reports/PER_PIXEL_BUDGET.md`. In one line: three independent per-pixel
units — `RCP24` at 238,095 a frame, `PERSPUV` at 151,515, `AUX` at 277,778
(ruling 7's figure) — all sit within a factor of two of the 276,480-pixel
terrain estimate, so there is no slack anywhere on that path. The attribute
divide needs 2 to 5 times what eight radix-2 units deliver; radix 4 closed about
half of that gap and is a build parameter now.

### The two things that are blocking, and neither is code

1. **`wmin`, `wmax`, `scale`** — step 6's last block.
2. **The arena capacity** — step 5's cost, step 9's whole point, and the reason
   an army cannot be frame-resident. Every capacity question now leads here.

Step 9 was ordered last and three separate findings now depend on it: the
survivor fraction `s` that swings the divide budget by 4x, the references-per-
triangle that swings ruling 4 by 100x, and the arena sizing itself. **It should
move up.**

---

## The order to build in

1. **Land the framebuffer-writer lease ruling.** One region, one dynamic owner;
   RTL, oracle and formal together.
2. **Wire the existing flat renderer into the shell.** The first
   hardware-generated pixel on the real scanout path is worth having immediately.
3. **Add the renderer frame transaction** — lease, fatal-error handling,
   memory-retirement accounting, publish/release.
4. **Fix the tile lifecycle** — clear once, zero-to-many triangles, resolve once,
   every active tile visited.
5. **Introduce TriangleContext** and preserve edge setup through the binner.
6. **Land the attribute-bearing geometry seam** and plane interpolation.
7. **Finish TMU v2 in parallel**, then integrate perspective + TEXJOIN.
8. **Pipeline AUX** and run primary + AUX concurrently.
9. **Generate real 8 km-map / army / giant / Duo traces**, size the triangle and
   reference arenas, then do the composed Quartus fit.

### The one dependency imposed on the TMU work

TMU v2 may proceed in parallel with renderer construction, but:

> **Do not freeze the final TEXJOIN packet or the production raster/TMU
> composition until the attribute-bearing TriangleContext law is written.**

The TMU v2 direction itself is confirmed correct: freeze the serial FSM, build
the pipelined one, A0–A3 planner, shared cache, CLUT nearest → palette RAM,
direct nearest → decode, direct bilinear → one channel lane, 16-entry initial
ROB, ordered retirement. One filter-channel lane remains the right first point
because the known filtered workload is far smaller than the cache/output
workload. **ROB depth and palette-slot count are to be measured, not
sanctified.**
