# Contract — GEOM.PARAMBUF (External geometry parameter buffer)

> Ledger: `design/blocks.yml` · gpu clock · ENGINE1 · maturity REFERENCE_COMPLETE
> RTL: `fpga/rtl/geometry/zhao_geom_parambuf.sv` (the RECORD LAYER only)
> Reference: `zref::geom::parambuf_chunk_follow` and neighbours

## Purpose and exclusions

GEOM.PARAMBUF is where a frame's geometry actually lives: projected unique
vertices, compact triangle descriptors and tile-reference chunks, **in local
SDRAM**, owned by ENGINE1 as part of the render-geometry region.

**Written 2026-09-02 from ruling R7, which found the block had no contract, no
ledger entry and no block anywhere in the repository** while the path that
would have replaced it — "choose a kMesh budget and grow the on-chip arena" —
was being prepared. That path contradicts the binding 2026-08-31 ruling. This
file exists so the architecture has somewhere to be, before RTL rather than
after.

**On-chip is limited to**: the tile directory, active chunk tails, prefetch
FIFOs, a small projected-vertex cache, and an opportunistic expanded-context
cache. **Not** a frame-sized triangle arena.

**Exclusions, each one a specific refusal:**

* **No frame-sized on-chip triangle arena.** That is the whole point.
* **One tile clears and resolves exactly once.**
* **No framebuffer readback.**
* **No arbitrary tail truncation** — see Overflow.
* The block does not decide LOD. It stores what GEOM.PROJECT and the binner
  produce.

**On the evidence that pointed the other way.** A measurement showed ordinary
armies are considerably cheaper than the all-kMesh assumption. That is useful
and it stands. It is *evidence that a fixed constant could work*, and R7 is
explicit that it **does not revoke a scalable architecture** — which is the art
law in its engineering clothes: a measurement can remove a bias, it cannot
choose the design.

## Input and output packet layouts

### `ProjectedVertex` — 24 bytes

    screen_x    s32   legal range s21
    screen_y    s32   legal range s21
    invw24 + status byte      (u24 depth, u8 status, one word)
    u_over_w    s32
    v_over_w    s32
    rgba8       u32

### `TriangleDescriptor` — 16 bytes

    vertex_id[3]  u16
    material_id   u16
    raster_state  u32
    source_id     u32

### Tile-reference chunk — 64 bytes

    next_chunk        u32
    count             u16
    frame_generation  u16
    triangle_id[14]   u32

`frame_generation` in every chunk is what makes a stale chunk detectable rather
than plausible: a chunk from last frame reads as a valid chunk in every other
respect.

## Backpressure rules

The Measure **seals quotas before the frame**. Within a sealed frame the arena
is a fixed allocation, so there is no in-frame negotiation to backpressure —
allocation either fits the sealed quota or the frame faults (see Overflow).

Chunk-tail traffic between the on-chip directory and SDRAM is ordinary ENGINE1
client traffic and takes that arbiter's backpressure.

## Memory ownership

**Local SDRAM bank 3**, initial guard map:

    0x0600_0000 .. 0x063F_FFFF   PARAMBUF view 0, 4 MiB
    0x0640_0000 .. 0x067F_FFFF   PARAMBUF view 1, 4 MiB
    0x0680_0000 .. 0x069F_FFFF   shared prefetch/chunk scratch, 2 MiB
    0x06A0_0000 .. 0x07FF_FFFF   reserved/unmapped pending evidence

ENGINE1 owns the render-geometry region. Bank 2 is terrain (T2) and is not this
block's.

### Capacity tiers

| tier | projected vertices | triangles | tile references |
|---|---|---|---|
| **per-view minimum acceptance** | 32,768 | 8,192 | 65,536 |
| **preferred, inside a 4 MiB arena** | 65,536 | 16,384 | 131,072 |

### The content guarantee this exists to keep

* **32 ordinary creatures at kMesh machine-wide.**
* **In Duo, at least 16 per active view**; a single-view tier may use all 32.
* More may be admitted by **measured** tile-reference cost, but are not
  guaranteed.
* **A giant is a separate quota**: reserve **at least 32,768 tile references**
  for one giant *before* ordinary kMesh allocation. If the giant consumes the
  view's budget, ordinary creatures demote by declared LOD priority — **the
  giant is never silently truncated.**

## Q formats and rounding

None of its own. `invw24` is GEOM.PROJECT's depth in the ruled profile (R1);
`u_over_w` / `v_over_w` are that block's perspective attributes. This block
stores them and changes no bit.

The one width rule that is this block's: **`screen_x` and `screen_y` are stored
as s32 with a legal range of s21.** A value outside s21 is a malformed
descriptor, not a wrapped coordinate.

## Latency (fixed or variable)

`variable` — SDRAM behind ENGINE1's arbiter, with registered seams at the SDRAM
boundary. Chunk prefetch hides the common case; a directory miss takes the
arbiter's latency.

## Target throughput

Physical target is **the common renderer clock**, with **registered SDRAM
seams** — the seam registers are part of the target, not an implementation
detail to be discovered during fit.

## Overflow and malformed-input behaviour

**On hard arena overflow:** drain the frame, repeat the prior complete frame,
report the source IDs. **Never publish a frame with an arbitrary missing tail.**

That is the same shape as the terrain frame fault (T6) and for the same reason:
a frame missing a silently truncated tail looks like a frame, and nothing
downstream can tell that it is wrong.

**Stale handles.** A chunk whose `frame_generation` does not match the current
frame is rejected and counted, never followed.

**Malformed descriptors** — a `screen_x`/`screen_y` outside s21, a `vertex_id`
past the sealed vertex count, a `next_chunk` outside the arena — are rejected
and counted. None of them corrupts memory: the charter rule is that overflow
stays correct and becomes slower rather than corrupting memory.

## Scalar reference function

`zref::geom::parambuf_chunk_follow`, with `zref::geom::parambuf_fits_s21`,
`zref::geom::parambuf_triangle_illegal` and `zref::geom::parambuf_chunk_stale`
(`reference/include/zref/zref_geom.hpp`).

They own the two legality rules and the staleness gate — the parts that could
be silently wrong. The arena's capacity policy and the frame-fault path are the
composed block's and have no scalar law to own yet.

## Directed tests

Planned, and named by R7: directed, randomized, overflow, frame-generation and
stale-handle.

* **`tests/geometry/geom_parambuf_directed.cpp` — WRITTEN.** Every field of all
  three records at its own offset; s21 legality exact at both boundaries and
  **reported rather than clamped**; a vertex id at the sealed count refused; a
  chunk wrong ONLY in its generation refused and not followable; a count above
  capacity and a `next_chunk` at the arena size refused; the all-ones sentinel
  ending the list without being malformed. 15 checks.
* The rest below are **planned and not written**, and are named without paths
  for that reason — see `reports/PHANTOM-CITATIONS-AUDIT.md`.
* overflow — a frame that exceeds the sealed quota faults,
  drains, repeats the prior frame and reports source IDs; **no partial frame is
  published**.
* frame generation across a whole walk — a chunk carried over from the previous
  frame is rejected, not followed.
* stale handle — a handle to a reallocated chunk is reported
  stale rather than silently redirected.
* giant quota — a giant reserves its 32,768 tile references
  before ordinary kMesh allocation; under pressure ordinary creatures demote by
  declared LOD priority and the giant stays whole.

## Randomized differential tests

Planned: randomised scene composition against an independent model of the
allocator, checking that the same sealed frame produces the same arena bytes and
the same chunk chains — the determinism the console's replay story depends on.

## Integration capture cases

None on hardware. **RTL exists for the RECORD LAYER only** —
`zhao_geom_parambuf.sv` owns the three layouts, the two legality rules and the
staleness gate. The arena allocator, the quota seal and the frame-fault path
are not built. No board, no programmed device. The
guard map, the tiers and the throughput target above are all specification, and
the capacity numbers are provisional until measured tile-reference cost says
otherwise.

## Notes

Registered per R7 so the architecture has an entry before it has an
implementation — the specific failure R7 caught was that it had neither, while
work was under way that assumed it would never need one.
