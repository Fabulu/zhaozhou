# Contract — GEOM.ASSEMBLE (Index walk and triangle assembly)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: not built
> Reference: `zref::geom::assemble_*` — PLANNED AND NOT WRITTEN

## Purpose and exclusions

GEOM.ASSEMBLE walks a meshlet's `u8` local index stream and emits one complete
triangle per triplet, by selecting three already-decoded/projected vertices.

**Written 2026-09-03 from `reports/BORING_3D_FUNDAMENTALS_AUDIT.md` R1, which
is the most basic omission the audit found.** Verified against the tree rather
than taken on trust:

* `GEOM.MESHFETCH`'s descriptor carries `vertex_offset`, **`index_offset`**,
  `vertex_count`, **`triangle_count`** and `material_id`;
* `zhao_geom_vdecode.sv` accepts **neither `index_offset` nor
  `triangle_count`** — its inputs are `v_bytes_i`, `v_format_i`, `v_src_id_i`.
  It is the vertex side only;
* `zhao_geom_setup.sv` expects a **complete triangle** — `tri_ax_i` and the
  other two corners;
* `tri_ax_i` is driven only from `zhao_shell_top.sv` and
  `zhao_geom_bin_pipe.sv` — **a harness**;
* the ledger had **zero** blocks matching `GEOM.(ASSEMBLE|INDEX|TRI)`.

So the authoritative path had a hole in the middle:

    MESHFETCH
      |- vertex_offset --> VDECODE --> SKIN/PROJECT
      '- index_offset  --> ??? -----> triangle A/B/C --> CLIP/SETUP

**This block is that arrow.** The audit's instruction is explicit and is the
reason it gets its own entry: *"Do not let it emerge accidentally as
miscellaneous logic inside `GEOM.PARAMBUF`."*

**Exclusions, each a specific refusal:**

* **No vertex decoding.** `GEOM.VDECODE` owns the record format.
* **No skinning, projection or clipping.** It selects vertices that have
  already been through those; it does not transform anything.
* **No material resolution.** It carries `material_id` opaquely to the
  descriptor. `MATERIAL.RESOLVE` (R3) turns it into bindings.
* **No storage of the vertex pool.** It reads a projected-vertex cache; the
  arena is `GEOM.PARAMBUF`'s and the cache is `GEOM.WCACHE`'s.
* **No reordering.** Triangles are emitted in index-stream order, because the
  capture is the contract — two orderings of the same meshlet produce the same
  picture and different CRCs.

## Input and output packet layouts

### In — one meshlet, from `GEOM.MESHFETCH`

`{ instance_id, meshlet_index, visible_mask[1:0], rung, hold, vertex_offset,
index_offset, vertex_count, triangle_count, material_id, flags }` — the
descriptor GEOM.MESHFETCH already promises to emit.

### The index stream

`u8` local indices, three per triangle, at `index_offset`.

**This is why `vertex_count ≤ 64` and `triangle_count ≤ 126` are limits rather
than suggestions**: a `u8` local index cannot address past 255, and 126
triangles × 3 indices is 378 bytes. Both are frozen by ruling and this block
enforces them rather than assuming them.

### Out — one `TriangleDescriptor` per triplet

Exactly `GEOM.PARAMBUF`'s 16-byte layout, so nothing is invented here:

    vertex_id[3]  u16   the three GLOBAL projected-vertex ids
    material_id   u16   carried opaquely from the meshlet
    raster_state  u32
    source_id     u32

The local `u8` index becomes a global id by `vertex_offset + local`, and **that
addition is the block's one arithmetic act.**

## Backpressure rules

Ready/valid on both sides. A meshlet is accepted only when the block can walk
it; a triangle is emitted only when the consumer takes it. **The walk is
resumable under backpressure** — a stalled consumer must not lose position in
the index stream, which is the failure that would drop triangles from the
middle of a mesh and look like a modelling error.

## Memory ownership

None of its own. It reads the index stream through the ordinary geometry path
and the projected vertices from `GEOM.WCACHE`. It writes nothing.

## Q formats and rounding

None. It moves ids and changes no bit.

## Latency (fixed or variable)

`variable` — a vertex-cache miss takes the cache's latency. **Throughput target
is one triangle per clock when the three vertices are resident**, which is what
makes the index walk not the bottleneck.

## Overflow and malformed-input behaviour

* **A local index ≥ `vertex_count` is malformed and REFUSED**, counted, and the
  triangle is not emitted. It is not clamped: a clamped index draws a triangle
  using a real vertex belonging to a different part of the mesh, which is a
  visible corruption nothing downstream can detect.
* **`vertex_count > 64` or `triangle_count > 126` is refused** at the meshlet,
  before any triangle is emitted — the whole meshlet, not a truncated prefix.
* **A degenerate triplet** (two or three equal indices) is a zero-area triangle.
  It is emitted and left to `GEOM.CULL`, which already owns that decision;
  refusing it here would put two blocks in charge of one rule.

## Duo-view selection, which is the part that is easy to get wrong

`visible_mask[1:0]` says which views the meshlet is in. **The projected-vertex
id is per view** — the same local index resolves to a different projected
vertex in view 0 and view 1, because projection is per view.

So the walk runs **once per visible view**, and the vertex id is
`vertex_offset + local` **in that view's arena**. A single walk emitting into
both views would give view 1 the vertices of view 0, which looks like a
correct image in one eye and a subtly wrong one in the other — the hardest
class of bug to see and the easiest to write.

## Scalar reference function

**PLANNED AND NOT WRITTEN**: `zref::geom::assemble_triangle` (local triplet plus
`vertex_offset` to three global ids, with the legality rule) and
`zref::geom::assemble_count` (triangles per meshlet). Named without paths
because neither exists — see `reports/PHANTOM-CITATIONS-AUDIT.md`.

## Directed tests

**PLANNED AND NOT WRITTEN.** Named without paths for that reason:

* every triplet of a 126-triangle meshlet emitted, in index-stream order;
* a local index at `vertex_count` refused (the count is a count, not a last
  index) and the triangle not emitted;
* `vertex_count` 64 and `triangle_count` 126 accepted, 65 and 127 refused
  **whole** — no truncated prefix;
* the walk resumes correctly across arbitrary consumer backpressure, with the
  same triangles in the same order as an unstalled run;
* **Duo**: the same meshlet in both views produces per-view vertex ids, and a
  test that swapped them would fail.

## Randomized differential tests

Planned: random meshlets against the scalar model, with a deliberate malformed
fraction and a coverage guard that each refusal class was actually reached.

## Integration capture cases

None on hardware. **The composed case that matters** is MESHFETCH → ASSEMBLE →
SETUP with a real mesh, because that is the path currently fed by a harness.

## Synthesis / resource ceiling

Expected small: a counter, a triplet register, an adder and a legality compare.
The cost is in the vertex-cache traffic, not here.

## Notes

The block may alternatively be folded into `GEOM.MESHFETCH` as an explicit
topology-walking responsibility. **That is an owner choice**; what is not
optional is that one named block owns it. It is registered separately here
because MESHFETCH is already a large unbuilt block and merging an unbuilt thing
into another unbuilt thing hides the seam rather than closing it.
