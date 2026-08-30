# The render path, and the one law that stands between it and the shell

Written 2026-08-30, when `zhao_raster_fbwrite` landed and a triangle rasterized
by the RTL reached memory for the first time (commit `ff01661`).

This file exists so the next person does not rediscover the blocker, and so the
decision it needs is asked as a question rather than made quietly.

---

## What works now

    tests/render/render_fb_directed.cpp        19/19

    one triangle          7,936 px in 496 bursts (31 tiles x 256), exact
    two overlapping      10,240 px in 640 bursts over 40 jobs, exact
    memory stalling 1:4   6,656 px in 416 bursts, exact, 2,416 fb stalls

`zhao_probe_render_fb` is GEOM.BINNER -> RASTER.TILE_PIPE -> RASTER.FBWRITE, and
the harness models MEM.GUARD and a VRAM behind it. A whole framebuffer is
compared against `zref::Binner` and `pipe_oracle` in series, placed by an
address law written out independently of the RTL.

`zhao_raster_fbwrite` turns RESOLVE's `{rgb565, x, y, last}` into bursts:
**one tile row is exactly one burst** — sixteen consecutive beats share a `y`
and have `x`..`x+15`, which is 32 contiguous bytes against the guard's 64-byte
limit and four beats of its 64-bit write data. Contiguity is checked rather than
assumed, because addressing sixteen pixels from the row's first one would put a
right-looking row in a wrong place.

## What blocks the shell wiring, exactly

`zhao_shell_top` ties `client_req[2]`, `[3]` and `[4]` to zero, and
`ZHAO_CLIENT_ENGINE0 = 3'd2` is commented in `zhao_pkg` as a "reserved
guaranteed slot". So the arbiter port is there and free.

**MEM.GUARD refuses it.** Its permission switch is:

    ZHAO_CLIENT_SCANOUT:  shape_ok && scan_ok     // read-only, either FB slot
    ZHAO_CLIENT_BLIT_DMA: shape_ok && blit_ok     // write, granted slot window
    default:              1'b0                    // "engines/debug own nothing
                                                  //  in Phase 2"

That default is correct for as long as no engine exists. RASTER.FBWRITE is an
engine that exists, and it writes **the granted frame-buffer slot** — the same
bytes DEBUG.FRAMEBLIT writes, for the same reason, in the same frame.

### The change this needs, and why it was NOT made unilaterally

The minimal amendment is one line:

    ZHAO_CLIENT_ENGINE0:  pass_ok = shape_ok && blit_ok;

giving the render engine the blit's window rather than a window of its own, so:

* the region is unchanged — still `[slot_base, slot_base + span)` with the span
  clamped, so the 32-bit wrap this block's proof was written against is closed
  for the engine by exactly the same construction;
* it stays WRITE-only and `map_valid`-gated, so a frame with no grant denies the
  renderer as completely as it denies the blit;
* no new port and no second region map for CMD.SCHEDULER to publish twice.

**It was built, and then reverted, because it is a change to a LAW and not to an
implementation.** Three places state the same rule and all three would have to
move together:

| where | what it says |
|---|---|
| `fpga/rtl/memory/zhao_mem_guard.sv` | the `default: pass_ok = 1'b0` above |
| `reference/include/zref/zref_mem.hpp` | `default: return false; // engines/debug own nothing in Phase 2` |
| `tests/formal/formal_mem_guard.sv` | `a1_client` asserts only SCANOUT or BLIT_DMA is ever forwarded |

Changing the RTL alone put the hardware and its oracle out of step, and
`mem_guard_directed` caught it immediately — 30 accepts against an expected 28.
**The test did its job.** The RTL and formal edits are reverted; the tree is
green.

### ANSWERED 2026-08-30 — see reports/RENDERER_ARCHITECTURE.md

**Share the window. Do not create a second framebuffer map entry.** But not with
the unconditional line below either: share the SPATIAL window, not the TEMPORAL
permission. VIDEO.SLOTMGR already owns a single framebuffer lease with slot and
generation and formally guarantees one at a time — generalise that lease to name
its writer, and have MEM.GUARD check `lease_owner` as well as the window. A v1
frame uses the renderer or DebugFrameBlit, never both; meeting both writer
classes faults the frame and the slot is released dirty rather than published.

That also corrects this file's "two writers are CMD.SCHEDULER's problem": the
split is CMD.SCHEDULER selects the writer, VIDEO.SLOTMGR owns the lease,
MEM.GUARD enforces capability and bounds, and the writer proves its traffic
retired before publication.

The original question is kept below for the record.

### The question for the owner

**Should the render engine share the blit's framebuffer window, or own its own
map entry?**

*Sharing* is one line in three files and no new plumbing, and it means two
clients may write the granted slot in one frame — a scheduling question that
belongs to CMD.SCHEDULER, not to the guard, whose whole contract is that nothing
escapes the window.

*Its own map* is honest about there being two writers with different lifetimes
and lets CMD.SCHEDULER grant them separately, at the cost of a second region map
and a second set of ports through the shell.

When that is settled, the wiring itself is small: one `zhao_mem_guard` instance,
`client_req[2]` driven instead of tied off, and the render path's triangle port
brought to wherever CMD will eventually feed it.

## What is still missing after that, and it is not small

The shell wiring gives the console a render path that writes **flat-shaded**
triangles. `zhao_raster_tile_pipe` states it at its own head: the fragment's
source colour, alpha, depth, tag and texel are FLAT across the triangle,
"because interpolating them is GEOM.SETUP's job and GEOM.SETUP is not built".

So the ordered remainder is:

1. **MEM.GUARD's engine grant** — the question above.
2. **Shell wiring** — guard instance, `client_req[2]`, scanout reads what was
   written. This is the first pixel on a screen.
3. **Attribute interpolation in GEOM.SETUP** — depth and inverse-w gradients,
   then perspective-correct U/V for Early-Z survivors. Until this exists there
   is nothing for a texture sampler to sample.
4. **RASTER.TEXJOIN** — the fragment-context FIFO that keeps the sampler busy
   instead of serialising it one fragment at a time.
5. **TEXTURE.TMU v2** — see
   `fpga/rtl/texture/RECON-TMU-WHAT-IT-IS-ACTUALLY-FOR.md` and the v2 respec.
6. **TEXTURE.AUX** — one request per six clocks against a terrain-primary
   component of 276,480 in 1,666,667 clocks is 277,778: effectively zero margin,
   and unfitted. A fast primary sampler does not close the terrain texture path
   on its own.

Steps 1 and 2 are hours. Step 3 is the one that turns a flat-shaded triangle
into a textured one, and nothing downstream of it can be finished first.
