# Contract — GEOM.PARAMBUF (External geometry parameter buffer)

> Ledger: `design/blocks.yml` · gpu clock · ENGINE1 · maturity REFERENCE_COMPLETE
> RTL: `fpga/rtl/geometry/zhao_geom_parambuf.sv` (the RECORD LAYER),
> `zhao_geom_paramarena.sv` (the ARENA PRODUCER, `GEOM.PARAMARENA`),
> `zhao_geom_paramwalk.sv` (the RECORD READER, `GEOM.PARAMWALK`)
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

#### `raster_state` — the layout (owner ruling R28, provisional, 2026-09-19)

R28: *"Cull mode comes FIRST and from the DRAW (DrawForm flags); the remaining
bits come from the material set."* Model: `zref::raster_state`
(`reference/include/zref/zref_raster_state.hpp`), tested by
`tests/geometry/raster_state_directed.cpp`.

    bits [1:0]   cull_mode   = DrawForm.flags[3:2]
                             0 NONE (double-sided), 1 NEG, 2 POS,
                             3 RESERVED -- refused, never aliased
    bits [31:2]  material    = MaterialRecord.raster_state[31:2], unchanged;
                             no v1 consumer, so a v1 material writes 0

`MaterialRecord.raster_state[1:0]` is reserved 0: the cull mode is the
draw's. The encoding is the consumer's, not chosen here: `zhao_geom_clip`'s
`cull_mode_i` (CULL_NEG = 1, CULL_POS = 2, NONE the default), which the test
reads back from the RTL source. A draw that never set flags[3:2] reads NONE,
the double-sided behaviour every existing capture was recorded under.

The layout is RATIFIED; its PATH is not yet composed. The draw's flags reach
the console on CMD.EXEC's `cmd_draw_*` (I41), but a draw becomes a meshlet
job only through `geom_mf_job_*` (I36, owner ruling R29), so the core cannot
yet pair a meshlet's triangles with the draw that issued them. Until it can,
`geom_asm_raster_state_i` (I39) and `geom_clip_cull_mode_i` (I24) stay
boundaries: tying `cmd_draw_flags_o` to them would pair meshlet N's
triangles with draw M's cull mode -- the fault I39 refuses by name.

### Tile-reference chunk — 64 bytes

    next_chunk        u32
    count             u16
    frame_generation  u16
    triangle_id[14]   u32

`frame_generation` in every chunk is what makes a stale chunk detectable rather
than plausible: a chunk from last frame reads as a valid chunk in every other
respect.

## The RECORD size is not the ALLOCATION stride (ARENAWIRE, 2026-09-23)

The three layouts above are **records**, and R7 freezes them. What the arena
**advances its cursor by** is a separate number, and for the ProjectedVertex it
is **not** the record size.

| record | size | allocation stride | why |
|---|---|---|---|
| `ProjectedVertex` | 24 B | **32 B** (`PV_STRIDE_B`, a knob) | 24 is not a multiple of the SDRAM's 16-byte burst-alignment quantum, so a 24-byte stride puts every **odd** vertex 8 bytes into an aligned eight-column block — where a JEDEC BL8 sequential burst wraps |
| `TriangleDescriptor` | 16 B | 16 B | already a multiple |
| tile-reference chunk | 64 B | 64 B | already a multiple |

**The sub-region bases are rounded up to the quantum too**, because a stride
that is a multiple of 16 still lands every record off it if the region does not
**start** on one. `zhao_geom_paramarena` derives `TRI_OFF_B` and `CHUNK_OFF_B`
that way and refuses a breach at elaboration; the full argument, including the
arithmetic that shows a 16-byte-aligned address makes every burst the arbiter
derives from it safe, is in that block's header under **THE BURST THAT WRAPS**,
and the law is `spec/memory_rules.md` §5c (**provisional**).

**THE COST, DECLARED RATHER THAN ABSORBED.** Eight bytes of slack per vertex,
524,280 bytes at 65,535 vertices. The view's used footprint is **3,407,840 of
4,194,304 bytes** — 2,097,120 vertex + 262,144 descriptor + 1,048,576 chunk —
so R7's preferred tier still fits inside 4 MiB with the stride applied.

**AND IT WAS NOT A PRECAUTION.** The layout before this change put
`TRI_OFF_B` at `65,535 * 24` = 1,572,840, which is **8 mod 16**, so every
TriangleDescriptor the arena wrote was misaligned. `sim/models/zhao_sdram_model.sv`
reads **linearly**, so nothing in this repository could fail on it. The
invariant is therefore **counted** — `burst_unaligned_o` on both blocks — and
both counters have been seen to fire.

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

### The guard window — owner completion ruling ITEM 4, 2026-09-22

**The map above was ruled on 2026-09-02 and `MEM.GUARD` had no window for any
of it until 2026-09-22.** Packet GEOMCLOSE measured the consequence and it runs
in the unflattering direction, which is why it is recorded rather than
paraphrased: ENGINE1's only arm was `render_asset_ok`, whose window is
`[0x06A0_0000, 0x0800_0000)`, and **the whole of §5c lies strictly below it**.
Not a direction bit short of legal — outside the bounds, in both directions,
for the only client that owns it. So even a READ-ONLY composition was refused
by construction, and the recorded blocker ("it needs an arena writer") was true
and was the SECOND obstacle.

Item 4 opened it, narrowly:

| arm | direction | region | gated on |
|---|---|---|---|
| `pb_rd_ok` | read | either view | the frame lease |
| `pb_wr_ok` | write | **the view the lease NAMES** | the frame lease |
| `pb_scr_ok` | both | the shared scratch | the lease **and** an explicit acquire |

**THREE containment tests, not one.** The three regions tile exactly — view 0
ends where view 1 begins, view 1 where the scratch begins, the scratch exactly
at `ZHAO_RENDER_ASSET_BASE` — so a single `[VIEW0_BASE, SCRATCH_END)`
comparison would be arithmetically identical for every request inside any ONE
of them and would additionally admit every request that **spans a seam**. That
is not a corner: the two views exist because one is written while the other is
read. Item 4 says so in as many words, and
`tests/mutants/zhao_mem_guard_pbunion_mutant.sv` is the committed fault that
implements the forbidden reading and makes `mem_guard_no_escape` FAIL.

**`RENDER.ASSET_POOL` stays READ-ONLY to `ENGINE1`** and survives this ruling
structurally, not by inspection: `render_asset_ok` still requires `!req.write`,
and all three new regions end at or below its base. Theorem
`a1_pb_asset_still_ro`.

**With the lease low, ENGINE1's permissions are byte-for-byte what they were
before this ruling.** That is what makes "no blanket bank-3 permission" a
checkable property rather than a sentence.

### The residual the guard cannot close, and where it IS closed

Item 4: *"Carry request identity WITH the request; do not validate a queued
request against a later global view selector."* `pb_wr_view` **is** a global
selector and `MEM.GUARD` is combinational over the request it is being
*offered*, so a request queued under view 0 and still unaccepted when the lease
flips would be judged by the new selector. `zhao_guard_req_t` has no view field
and widening it would change every client's ABI.

It is closed at `GEOM.PARAMARENA`, where the identity lives:

* `view_q` is latched at frame seal and `pb_wr_view_o` is driven from it and
  nothing else — one write to that register in the whole block;
* `m_addr_q` is latched at op start and is the only source of
  `guard_req_o.addr`;
* **a seal is HELD PENDING while any write is outstanding or the walker still
  owns the target view.** `view_flip_blocked_o` counts every clock it waits,
  because a precondition nobody can show ever delayed anything is a term and
  not a protection.

`addr_view_bad_o` differences the view the held address lies in against the
view the lease names, and **its two operands load on different enables** —
`m_addr_q` by the engine, `view_q` by the seal — so it is not the
lockstep-blind kind of checker. It is unreachable with legal stimulus while the
drain is correct, so it owes a committed mutant:
`tests/mutants/zhao_geom_paramarena_drain_mutant.sv`.

## The subsystem, and which part owns what

Item 4: *"Authorization includes the arena producer, allocation/chunk
management, write-capable route, record readers and actual rendering
consumers. GEOM.PARAMBUF's existing record decoder is not the whole
subsystem."*

| block | owns |
|---|---|
| `GEOM.PARAMBUF` | the three layouts, the two legality rules, the staleness gate. Pure decode, no memory port, and **instantiated by the walker** rather than reimplemented there |
| `GEOM.PARAMARENA` | allocation, the per-chunk generation stamp, the quota seal, the overflow and prior-complete-frame fallback, the two-view lease and its drain, the shared scratch's ownership, and the write-capable guard socket |
| `GEOM.PARAMWALK` | the frame-directory read, the chunk-chain walk, the descriptor fetch, and the round-trip evidence |

**The write-capable route.** `zhao_geom_mem_adapter` is
`zhao_mem_share_n #(.FORCE_READ(1'b1))` and that parameter is how §5f's "same
client, same direction, same bounds, same arbiter slot" is *enforced*. An arena
writer changes the direction, so it cannot join that adapter without removing
the property the adapter exists to hold. Instead the read adapter's merged
output becomes one leg of a `zhao_mem_share_wr #(.N(3), .CLIENT_ID(3))`, which
also carries the producer and the walker. Client 3 is still **one** client at
the arbiter; client 5 stays unspent (T3).

**The shared scratch holds the FRAME DIRECTORY** — one 64-byte record naming
the published view, its generation, the three region bases and the three
counts. It is the only thing the producer and the walker must agree on, it is
not per-view, and the producer arbitrates it between itself and the walker with
an explicit acquire and release: `pb_scratch_valid` is LOW between uses, so the
region is unmapped for everybody rather than standing permanently open.

### The round trip is the evidence, and `dir_mismatch` is where it lands

Item 4: *"Do not pack fields into a byte vector merely to unpack them again and
count that as external-memory integration."*

So the deliverable is not that a decoder decodes. The frame directory reaches
the walker **twice by two independent paths** — once as `pub_*_i`,
combinationally from the arena's registers, and once as 64 bytes that went out
through the guard, the arbiter, the controller and the SDRAM and came back. If
they disagree, the bytes did not survive the round trip, and **nothing else in
the system would say so**: a walk over corrupt records still produces
triangles. All eight fields are compared; a check that compared only the
generation would pass while every base was wrong, and the bases are what a bad
address or a wrong beat demux corrupts.

### The divergence that was declared here is CLOSED (ARENAID, 2026-09-25)

**This section used to record a permanent loss and it was a port width.** It
said `MAX_VERTS` defaults to 65,535 because *"a `vertex_id` is u16 and
`td_sealed_vertices_i` is u16 with it, so the seal 65,536 is not expressible in
the port the legality rule is tested against"*, and concluded *"this takes the
vertex, and the number stays a knob."*

Owner vacation directive §4: *"Retain R7's intended 65,536-vertex capacity: IDs
0..65,535 fit u16, but a COUNT of 65,536 requires a wider internal count/limit.
Use at least 17 bits for that count instead of silently sacrificing a vertex or
representing full capacity as zero."*

Three places, one cause, all repaired:

* `zhao_geom_parambuf`'s `td_sealed_vertices_i` is **u18** (18 rather than 17
  because the arena's cursors, quotas and publication ports already are);
* `zhao_geom_paramwalk`'s `w_verts_q` is u18 and **no longer saturates the
  published count at `0xFFFF`** — that saturation was the "silently sacrificing
  a vertex" arriving through a clamp rather than a truncation;
* `zhao_geom_paramarena`'s `MAX_VERTS` defaults to **65,536**.

**The record layout did not move.** A `vertex_id` is still u16 and still names
0..65,535 — which is exactly 65,536 ids. **Allocation stride and serialized
record size remain separate concepts**, and so do a serialized *id* and an
internal *count*. The allocation side never needed the change, which is why this
was a decoder-port defect wearing an allocator's clothes.

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
* **`tests/geometry/geom_paramarena_directed.cpp` — WRITTEN 2026-09-22.** The
  producer and the walker against the REAL guard, arbiter, controller and SDRAM
  model: the field-for-field round trip, the quota-overflow fault with the
  prior-complete-frame fallback proved by reading `publish_*` after it, the
  view alternation and the drain that holds a seal, the retire gate delaying a
  publish, a stale chunk refused, an illegal chunk refused, and
  `guard_violations` at zero across every legal case.
* overflow — a frame that exceeds the sealed quota faults,
  drains, repeats the prior frame and reports source IDs; **no partial frame is
  published**. Covered by the directed test above.
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

None on hardware. No board, no programmed device.

**UPDATED 2026-09-22 (owner completion ruling ITEM 4).** The paragraph that
stood here said "RTL exists for the RECORD LAYER only ... the arena allocator,
the quota seal and the frame-fault path are not built". All three are built now
and composed in `zhao_console_core`, behind the real `zhao_mem_guard`, the real
`zhao_vram_arbiter` and the real `zhao_sdram_ctrl`.

**WHAT REACHES THE ARENA IN THE COMPOSED CONSOLE, AND WHAT DOES NOT.** Said
plainly, because "composed" and "exercised" are different claims:

* **TriangleDescriptor — REAL, and POST-CLIP since 2026-09-25.** It used to be
  `zhao_geom_assemble`'s live output, tapped with GEOM.REPLAY. **That is
  superseded**: §4 requires descriptors to *"refer to the FINAL arena IDs, not
  to an earlier private store"*, and GEOM.ASSEMBLE's `t_v*_o` are GEOM.REPLAY's
  **arena-local** indices, which restart at zero in every meshlet. The tap is
  **moved** to `zhao_geom_vertid`'s output, not duplicated — two descriptor
  streams in two namespaces filling one array is the fault entry I54 measured
  and refused.

  **The per-primitive metadata the 16-byte record cannot carry** — the
  `material_set` handle32, `material_mode`, `frag_state`, the R89 flat
  `vertex_alpha`, `quality_tier` and the per-primitive untextured bit — is
  **not** overloaded onto an existing field and **not** replaced with a
  convenient zero. §4's remedy, a **versioned `TriangleExt` sidecar keyed by the
  same `triangle_id`**, is specified in `design/contracts/GEOM.VERTID.md`; its
  **writer belongs to entry I54**, which owns triangle serialisation. Until it
  exists the shortfall is a declared gap.
* **ProjectedVertex — REAL**, console entry I53 **CLOSED 2026-09-25**
  (packet ARENAID, owner vacation directive §4). The producer is
  **`zhao_geom_vertid` — GEOM.VERTID**, the console's one geometry identity
  space, with its own contract at `design/contracts/GEOM.VERTID.md`.

  It sits on **GEOM.CLIP's output** — the final geometry the binner references —
  and publishes each corner once per identity, where the identity is the
  console's own arena lookup key `{domain, arena, generation, index}`. The map
  is a **direct-mapped exact table**: no hash, no CRC, no position comparison.
  **The id is the allocator's**: `GEOM.PARAMARENA` gained `pv_accept_o` /
  `pv_id_o` / `td_accept_o` / `td_id_o` / `seal_fire_o`, driven from the same
  expressions its intake arm tests, so there is exactly one vertex counter in
  the console and a producer-side copy cannot drift from it.

  **The `ProjectedVertex` status byte is now specified** — it never was — as
  `{reserved[7:4], shared_capable[3], untextured[2], domain[1:0]}`, under §4's
  authority to amend record schemas.

  **Clipping lineage is a measured absence.** `zhao_geom_clip` performs
  whole-primitive near-plane rejection and creates no vertices, so there are no
  clipping-derived vertices in this console to identify. See GEOM.VERTID.md.
* **Tile-reference chunk — TIED**, console entry I54. `zhao_geom_binner_v2`
  builds exactly these chunks in an on-chip arena and exposes no way to see
  one: `ref_ram` and `next_ram` are internal and `job_*` is a drained stream,
  not the chunk layout. Three or four ports on that block are the missing
  piece.
* **The rendering consumer — TIED**, console entry I55. The walker is composed
  and reaches real memory; what is tied is who asks it to walk and who takes
  its triangles. Swapping the raster path off `zhao_geom_binner_v2`'s on-chip
  arena is the step that makes the external arena the LIVE path, and it needs
  I54 first — a walk over an arena nothing fills is a walk over nothing.

**R7's GIANT QUOTA IS NOT IN FORCE**, console entry I56. The composed seal is
the arena's own capacity, because the Measure has nowhere to publish a quota
yet. Sealing at capacity is the neutral choice — it enforces the real bound and
reserves nothing — but it means the 32,768 tile references reserved for one
giant before ordinary kMesh allocation are *not* being reserved, and a
reservation that silently is not happening looks exactly like one that is.

The guard map is now RTL and formally proved. The tiers and the throughput
target above remain specification, and the capacity numbers stay provisional
until measured tile-reference cost says otherwise.

## Notes

Registered per R7 so the architecture has an entry before it has an
implementation — the specific failure R7 caught was that it had neither, while
work was under way that assumed it would never need one.
