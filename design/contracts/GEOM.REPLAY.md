# Contract — GEOM.REPLAY (the geometry replay customer)

> Ledger: `design/blocks.yml` · gpu clock · maturity UNIT_VERIFIED
> RTL: `fpga/rtl/geometry/zhao_geom_replay.sv`
> Test: `tests/geometry/geom_replay_directed.cpp` (78 checks, every counter fired)
> Built 2026-09-19 (geom packet, RUN-20260919-1656-gaps-to-zero).

## Purpose

Turn one meshlet's sealed projected-vertex arenas (one per visible view, from
GEOM.GROUP_SEQ) and its TriangleDescriptors (from GEOM.ASSEMBLE) into screen
triangles for GEOM.CLIP: three arena lookups per triangle per view, the replies
joined into corners with their behind-the-eye bits, invw24 per corner from
GEOM.DEPTHQUANT, and slots 1..6 of the ruling-5 packet from the vertex-attribute
store. It is GEOM.WCACHE's "project once, replay twice" on the geometry side;
the terrain half lives inside `zhao_terrain_wcache`.

The specification was written before the block, as `zhao_console_core` entry
I11: "take a sealed handle here and a TriangleDescriptor {v0,v1,v2} from
GEOM.ASSEMBLE; issue THREE lookups on GEOM.PROJ_LANE; slice each 106-bit reply
into the corner and its behind bit; present {ax,ay,bx,by,cx,cy,behind[2:0]} to
GEOM.CLIP; pulse `rel_valid_o` back when the group is done with."

## Laws

* **Vertex ids are arena-local.** GEOM.GROUP_SEQ fills vertex i of a meshlet at
  arena index i in every view, so a meshlet-local index IS the arena index and
  the per-view base is the arena handle. GEOM.ASSEMBLE's `vertex_offset` is 0 by
  construction; ONE walk is replayed into each view's own arena, so view 1 never
  names view 0's vertices.
* **Depth is GEOM.DEPTHQUANT's, per corner, under the profile the vertex was
  projected with** (owner ruling D-4). The profile is a per-arena table written
  from the projector's own landings; two landings in one arena under different
  profiles are counted (`profile_mixed_o`). The arena's `d` (Q16.16 1/w) is
  deliberately not read.
* **Refused or missed corners drop the triangle**, counted (`refused_o`,
  `missed_o`). A vertex id wider than the arena index is forced to an index the
  arena refuses, never truncated onto a real slot.
* **The release is proven.** Handles arrive only after every vertex landed;
  GEOM.ASSEMBLE's `m_done_o` ends the walk on every path. `af_release_o` fires
  once per meshlet after both, after every triangle was emitted and every arena
  released. A meshlet with no vertices or no visible view expects no handles.
* **The attribute store answers with the arena's timing** on the same lookup
  nets; disagreement is counted (`att_skew_o`). Its writer is owner ruling R11's
  and is not built (core entry I46).

## Interfaces

Token `mt_*` (visible mask, vertex count) from the dispatcher fork; handles
`grp_*` and release `rel_*` with GEOM.GROUP_SEQ; arena opens `op_*` and landings
`fl_*`; triangles `t_*` and `m_done_i` from GEOM.ASSEMBLE; lookups `look_*` /
replies `rep_*` with GEOM.PROJ_LANE and `att_rep_*` from the store;
`af_release_o` to GEOM.ASSETFETCH; the triangle `o_*` to GEOM.CLIP.

## Throughput (measured, not claimed)

56 clocks per view-triangle unstalled, dominated by the reciprocal round trip
inside the three DEPTHQUANT lanes. Against the ruled 120,000-vertex tier
(~40,000 triangles, ~41 clocks per triangle in a 1,666,666-clock frame) that is
**over budget** for one view and more so for two. Named lever: a streaming
DEPTHQUANT (several corners in flight through the multi-context rcp24_v4)
and overlapping the next triangle's lookups with the current one's depth.
Recorded here, and in FINDINGS, rather than hidden by a target line.
