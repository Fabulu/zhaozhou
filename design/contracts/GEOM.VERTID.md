# Contract — GEOM.VERTID (the one geometry identity space)

> Ledger: gpu clock · ENGINE1 · maturity COMPOSED
> RTL: `fpga/rtl/geometry/zhao_geom_vertid.sv`
> Tests: `tests/geometry/geom_vertid_directed.cpp`
> Law: `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §4;
> `design/contracts/GEOM.PARAMBUF.md` (the records this block fills);
> `reference/include/zref/zref_fixp.hpp::unit8_from_fx16` (the colour law)

Written 2026-09-25 by packet ARENAID, which closed console entry **I53**.

## Purpose

**Assign the console's projected-vertex ids, and nothing else.** GEOM.VERTID
takes a post-clip triangle with three *identified* corners and emits

* a `ProjectedVertex` record for each corner whose identity has not already
  been published **in this frame**, and
* one `TriangleDescriptor` naming the three ids.

It is the only block in the geometry path that turns a vertex identity into a
`vertex_id`, and it is the reason a `TriangleDescriptor` can name a shared
vertex once instead of two or three times.

**Exclusions, each a specific refusal:**

* **It does not allocate.** `GEOM.PARAMARENA` owns allocation and hands the
  index back with its acceptance. See *The id is the allocator's*.
* **It does not compare positions**, hash anything, or compute a digest.
* **It does not interpolate, clip or create vertices.**
* **It does not decide which triangles exist.** GEOM.CLIP already did.

## Where it sits

    GEOM.REPLAY ─┐                               ┌─> GEOM.SETUP
    FORGE.PRIM  ─┤ GEOM.CLIPDOOR ─> GEOM.CLIP ─> ┼─> GEOM.ATTRPACK
    PART.CLIPFEED┘   (key, rider)     (flip)     └─> GEOM.VERTID ─> GEOM.PARAMARENA

A three-way fork on GEOM.CLIP's accepted packet: one AND for the ready, each
consumer's valid gated by the other two.

## The identity

    vertex identity = { domain, arena, generation, index }

| directive §4 component | where it comes from |
|---|---|
| view | the `arena` handle — GEOM.GROUP_SEQ opens one per meshlet **per visible view** |
| draw instance | the `arena` handle plus its `generation`: two dispatches of one mesh are two opens |
| source geometry/object generation | the same pair — a different object generation is a different dispatch |
| pose/warp state | the same pair — a re-posed instance is a different dispatch |
| source vertex identity | `index`, the vertex's ordinal among the batch's **decoded** vertices (`zhao_geom_vattr`'s header proves the equality) |
| attribute discontinuity | already a distinct source vertex, hence a distinct `index`, in the mesh record |
| producer domain | `domain`, from `zhao_geom_clipdoor`'s grant |

**Equal positions never prove identity here**, because there is no position
comparator in the block.

### Collision safety

The map is a **direct-mapped exact table**: `ARENAS × VSLOTS` rows addressed by
the *whole* of the `{arena, index}` part of the key — not a digest of it — with
the remaining discriminator **stored in the row and compared bit for bit**. Two
identities cannot share a row and be read as equal. **No hash, no CRC, no
probabilistic equality anywhere in the block**, which is §4's requirement.

A key outside the table (`arena ≥ ARENAS` or `index ≥ VSLOTS`) is **not
truncated into row 0**. It is published as its own vertex, its `shared_capable`
status bit is 0, and `vid_index_oob_o` counts it.

### The discriminator is an EPOCH, not the raw generation

`generation` is 8 bits, so an arena slot reused 256 times within one frame
presents the same generation twice and a leftover row would read as a **hit**
handing out the earlier vertex's id. Storing the generation is therefore not
exact over a frame however exact the comparison is.

So the block keeps a **per-arena, frame-local epoch**: when the generation
offered for an arena differs from the last one seen for that arena, the epoch
advances, and the epoch is what the row stores. `EPOCH_W` is 18 against a
reachable maximum of 65,536 advances (every advance forces a miss, and misses
are bounded by the vertex quota), so it cannot wrap inside a frame. **A wrap
counter is declined** and the reason is arithmetic.

## Lifetime, and the eviction proof §4 asks for

**Lifetime: exactly one frame.** The valid bitmap, the epochs and the
last-generation memory are cleared on `frame_seal_i`, which is
`zhao_geom_paramarena`'s own `seal_fire_o` — the clock the allocator resets its
cursor — and **not** the frame edge that *asked* for a seal. A seal at the arena
is a request held pending while the drain completes; clearing on the request
would leave this block describing frame N+1 while the allocator is still filling
frame N, and a stale id would then name a record index belonging to somebody
else.

**Eviction cannot change a still-referenced identity:**

1. A row is overwritten only by a corner with the **same** `{arena, index}` and
   a **different** epoch — a different identity. Same identity, same row, same
   epoch: a hit, never a write.
2. A different epoch on arena *a* means a different generation on arena *a*,
   which means GEOM.GROUP_SEQ re-opened it. It cannot do that until GEOM.REPLAY
   released the handle, and GEOM.REPLAY does not release a handle until it has
   emitted every triangle that references it. GEOM.CLIPDOOR preserves per-client
   order and GEOM.CLIP is in order and creates no vertices, so at this block's
   input every triangle of the earlier use precedes every triangle of the later.
3. Therefore the still-referenced identity is never the one evicted. And if
   premise 2 were ever broken by a future producer, the failure mode is a
   **miss** — a duplicate record with its own id — never a wrong hit, because a
   hit requires the exact stored epoch. **The safe direction is structural.**

`geom_vertid_directed.cpp` case 3 exercises the reopen and asserts the earlier
use's id is *not* handed out; case 9 asserts no row survives a seal.

## The id is the allocator's

`zhao_geom_paramarena` gained, for this:

| port | meaning |
|---|---|
| `pv_accept_o` | this clock's `pv` handshake really **allocated** — not sunk, not discarded, not over quota, not outside the view |
| `pv_id_o` | the index it allocated (`n_verts_q`), meaningful only with `pv_accept_o` |
| `td_accept_o` / `td_id_o` | the same for the descriptor — **I54's half** |
| `seal_fire_o` | the seal actually took effect this clock |

All of them are driven from the **same expressions** the arena's intake arm
tests. There is exactly **one** vertex counter in the console and it is the
allocator's. A dense counter in this block instead would be CLAUDE.md's
*"detector wired to two operands that move together"*: the two would diverge on
the first discarded record with every counter on both sides still balancing.

When a record is consumed and **not** allocated, the block records no row, names
the corner with the defined id 0, and counts `vid_sunk_o`. That case is
structurally confined: the arena sinks a record only when no frame is open or
the frame has faulted, and in both states the descriptor is sunk with it.

## Clipping lineage — a measured absence

§4 requires clipping lineage to be carried explicitly. **This console has no
clipping-derived vertices.** `zhao_geom_clip`'s header: *"THE NEAR PLANE IS A
WHOLE-PRIMITIVE REJECTION, NOT A CLIP … GEOM.CLIP never produces more than one
triangle for one triangle in."* It drops primitives and normalises winding; it
has no divider and no vertex queue to interpolate with.

So the lineage clause describes a machine this console is not, and **there is no
lineage field in the key because there is nothing for it to name.** This is
recorded as a measurement of the tree, not as a shortcut: if a Sutherland–Hodgman
clipper is ever built, the lineage becomes a fourth key component and
`zhao_geom_vertid.sv` is the file it is added to.

What GEOM.CLIP *does* do matters here twice: it **drops**, so publishing at its
output publishes only vertices the binner will reference; and it **flips**
winding, so the key follows its corner through the same swap as the attributes
— in the same block, beside the decision, for the reason GEOM.CLIP's own header
gives about the attributes.

## Producers with no identity to share

`zhao_geom_clipdoor` arbitrates three producers. Only the mesh arm has an arena
key. A forge primitive and a polygon particle are built corner by corner and no
two of their corners are the same vertex under any definition this console
holds.

`SHARED_DOMAINS` is the knob naming which domains carry a dedupable identity
(bit 0, the mesh arm). A corner from any other domain is published as its own
vertex, **declared unshareable rather than looked up and missed**, and counted at
`vid_unshared_o`.

## The `ProjectedVertex` status byte — a schema amendment

R7 says the record carries *"invw24 + status byte, one word"* and **nothing in
the repository ever said what the byte holds**: `zhao_geom_parambuf` decoded
`pv_status_o` and no producer ever wrote one. §4 grants this packet authority to
amend record schemas. The amendment:

    [1:0]  domain           0 MESH, 1 FORGE, 2 PARTICLE, 3 reserved (illegal)
    [2]    untextured       the primitive that first published this vertex
                            declared no texture coordinates (R197), so
                            u_over_w and v_over_w are DON'T-CARE
    [3]    shared_capable   published under a dedupable identity. 0 means this
                            record is one corner's own vertex
    [7:4]  reserved, written 0. Nonzero is a malformed record.

The byte deliberately does **not** carry the near-plane `behind` bit: post-clip
it is zero for every accepted triangle by construction, and a field that can
never be set is a lie that reads like evidence.

**An untextured-conflict counter is declined at design time.** Two primitives
sharing a vertex could in principle disagree about the untextured declaration;
they cannot here, because the declaration is per producer arm
(`GEOM_REPLAY_UNTEX_DECL` for the only arm with shareable identities) and
identities never span arms. A counter whose two operands are the same constant
is the shape this repository has been caught building four times.

## The colour law is cited, not chosen

Lit channels arrive as fx16 (1.0 = `0x1_0000`) in 32-bit slots; R7's record
holds `rgba8 u32`. `zref::unit8_from_fx16` is the published conversion —
negative → 0, above `0xFFFF` → 255, otherwise `(v + 128) >> 8` with a 255 rail
(the Review C2 clamp that stops a ~1.0 weight wrapping to 0). The RTL implements
exactly that and the directed test differences every channel against an
independent transcription of it.

**Byte order is declared:** `rgba8 = { a, b, g, r }`, r in the low byte,
LSB-first like every other packing in this subsystem.

## What the records still cannot carry, stated rather than absorbed

§4: *"Preserve all mandatory colour, alpha, UV/perspective, fog, cull,
material-set, material-record and fragment metadata. If the old compact records
cannot carry the full commissioned function, introduce a versioned extension or
immutable sidecar keyed by the same identity."*

The `TriangleDescriptor` is 16 bytes: `vertex_id[3] u16`, `material_id u16`,
`raster_state u32`, `source_id u32`. Per primitive, GEOM.CLIP's beat also
carries, and this record has nowhere to put:

| quantity | width | where it lives today |
|---|---|---|
| `material_set` handle32 | 32 | `cd_o_material_set` / MATERIAL.RESOLVE's request |
| `material_mode` | 2 | the door's per-client declaration (owner ruling 1) |
| `frag_state` | 32 | `cd_o_frag_state`, `zhao_raster_fragment`'s layout |
| `vertex_alpha` (R89 flat) | 8 | `cd_o_vertex_alpha` |
| `quality_tier` | 8 | `cd_o_quality_tier` |
| `untex` | 1 | carried per **vertex** in the status byte above; per primitive it is not in the descriptor |

**No field is overloaded and no handle is truncated to make room.** `source_id`
is a genuinely 16-bit id zero-extended to u32, not a narrowed 32-bit handle.
This block writes exactly what the 2026-09-22 composition wrote plus correct
vertex ids; the shortfall pre-exists it and is **not** widened by it.

The specified remedy is a **`TriangleExt` sidecar**, versioned, immutable, keyed
by the same `triangle_id` the arena allocates (`td_id_o`, which this block
already receives and re-exports on `tri_id_o`). **Its writer is entry I54's**,
because I54 owns triangle serialisation and the chunk records that reference
these ids; specifying it here and building it there keeps one owner per record.
Declared layout, 32 bytes, one allocation stride:

    version        u16   = 1
    material_mode  u8
    quality_tier   u8
    material_set   u32
    frag_state     u32
    vertex_alpha   u8
    flags          u8    bit 0 untex (per primitive), 7:1 reserved 0
    reserved       u16   written 0, refused nonzero
    triangle_id    u32   the identity it is keyed by, repeated so a stray
                         record cannot be read as a neighbour's
    reserved2      u64   written 0

**Until it exists, the shortfall is a declared gap and not a silent one.**

## Q formats and rounding

None of its own except the colour quantisation above, which is `zref`'s.
Positions are GEOM.CLIP's S 12.8 sign-extended to s32 (R7's field is s32 with a
legal range of s21, and S 12.8 is 21 bits, so the range law holds by
construction). `invw24`, `u_over_w` and `v_over_w` are copied unchanged.

## Latency and throughput

**Variable, and it backpressures GEOM.CLIP.** One corner per lookup, plus one
arena op per published record and one per descriptor: a triangle costs 4 clocks
plus the arena's SDRAM time for its 1–3 vertices and its descriptor. A 64-vertex,
126-triangle meshlet presents **378 corner references and publishes 64 records**
— the whole reason the identity space exists, and the number `vid_reused_o`
reports.

`vid_stall_o` counts every clock GEOM.CLIP is held by this block, so the cost is
a measurement rather than a worry.

## The one thing a lint cannot answer: MEASURED under Quartus

`quartus_map`, Quartus Prime Lite 17.0.2, 5CSEBA6U23I7, map-only, 320.9 s,
`sourceCommit eedf1175`, **`rtlCleanAtHead: true`**. Row
`zhao_geom_vertid@arenaid-vertid-map` in `reports/synthesis/zhao_block_map.json`.

| quantity | value |
|---|---|
| status | **ok** — it is synthesizable, which a clean Verilator lint does not settle |
| DSP blocks | **0** |
| block memory bits | **8,704** |
| inferred memories | **1** — `map_ram`, `Simple Dual Port`, depth 256, width 34, `autoShift: false` |
| RAM conversion warnings | **0** |
| registers | 1,657 |
| estimated ALMs | 2,247 (comb ALUTs 1,401) |
| virtual pins | 1,705 |

**The memory row is the one that matters and it is exactly the design.** 256
rows of `{epoch[17:0], id[15:0]}` is 8,704 bits, and it inferred as **one
altsyncram with zero conversion warnings** rather than becoming 8,704 flops and
a selection network — which is what happened to `zhao_geom_wcache`'s valid
bitmap before it was rewritten (its header records 1,545,804 comb ALUTs). The
standing direction is that M10K is the slack and ALMs are the binding
constraint; this block spends the slack.

**The ALM number is NOT a composed cost and must not be quoted as one.** 1,705
virtual pins on a leaf map is a boundary that dominates a block this size, and
`estimatedAlms` from Analysis & Synthesis is an estimate the fitter routinely
moves. **GEOM.VERTID has never been through a composed fit**; what this row
establishes is synthesizability, the DSP count and the RAM inference, and
nothing about area or Fmax in the island.

The 1,657 registers ARE real state and are worth naming, because most of them
are not this block's idea: the held triangle carries GEOM.CLIP's own
seven-slot attribute packet for three corners, 3 x 224 = 672 bits, plus three
21-bit coordinate pairs, three 23-bit keys, three 16-bit ids, the 256-bit valid
bitmap and the per-arena epoch state.

## Counters, and how each one fires

All twelve are reachable with **legal stimulus at this block's own ports**, so
**none owes a mutant**. `tests/geometry/geom_vertid_directed.cpp` fires every one
and asserts each as a **delta across its own case**.

| counter | what it means | fired by |
|---|---|---|
| `vid_tris_o` | triangles taken | every case |
| `vid_refs_o` | corner references seen | every case |
| `vid_published_o` | records offered as a first publication | case 2 |
| `vid_reused_o` | **a corner answered from the map** | case 2 (the shared edge) |
| `vid_unshared_o` | a corner published without a dedupable identity | case 4 |
| `vid_opens_o` | epoch advances | case 3 |
| `vid_sunk_o` | consumed and not allocated | case 1 |
| `vid_index_oob_o` | a mesh key outside the map | case 5 |
| `vid_key_split_o` | a triangle whose corners disagree on `{arena, gen}` | case 6 |
| `vid_id_unnameable_o` | an allocated id a u16 `vertex_id` cannot name | case 7 |
| `vid_seal_abort_o` | a seal landed mid-triangle and dropped it | case 8 |
| `vid_stall_o` | clocks GEOM.CLIP was held | case 10 |

**Declined counters, with reasons:** an epoch-wrap counter (the bound is
arithmetic — 65,536 reachable advances against 2^18), and an
untextured-conflict counter (its two operands are the same constant; see above).

## Directed tests

`tests/geometry/geom_vertid_directed.cpp`, eleven cases:

1. nothing sealed — records sunk at full rate, no identity recorded
2. **two triangles sharing an edge** — 6 references, 4 records, the shared pair
   named by the same id in both descriptors, dense ids from 0, every
   `ProjectedVertex` field checked and the colour differenced against `zref`
3. a new generation on the same arena — a new identity, and the reused slot does
   not hand out the previous use's id
4. an unshared domain — two identical forge triangles publish six vertices
5. an out-of-range key — published unshared, never truncated into row 0
6. a split key — counted
7. an unnameable id — refused and not recorded
8. a seal mid-triangle — dropped, and nothing offered on the seal clock
9. the map does not survive the frame
10. backpressure — the offer is held stable and nothing is lost
11. the descriptor's per-primitive fields are carried, and `tri_id_o` is the
    allocator's own triangle index

## Notes

Entry `I53`'s two recorded obstacles and what happened to them:

* *"The u16 field is already doing two jobs."* **Still true, and still two.**
  `GEOM_ASM_VOFF_C` stays zero and GEOM.ASSEMBLE's `t_v*_o` remain GEOM.REPLAY's
  arena-local indices. What changed is that the arena no longer takes its
  descriptors from there.
* *"There is no handshake in this console that carries a vertex's identity and
  its attributes together."* **True at the landing, false at GEOM.REPLAY's
  per-corner reply**, where GEOM.WCACHE and GEOM.VATTR answer the same lookup on
  the same clock by construction. The join happened three blocks upstream, with
  a handshake rather than a timing assumption.
