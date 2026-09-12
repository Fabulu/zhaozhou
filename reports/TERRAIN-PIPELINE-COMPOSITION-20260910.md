# Terrain projection pipeline: view/group sequencing is composed and verified; production adoption remains open

**Run:** `RUN-20260910-1014-terrain-pipeline-composition`, independently closed by `RUN-20260912-1841-resume-terrain-projection-pipeline`
**Date:** 2026-09-12
**Status:** functional composition candidate complete; **not production-adopted; not fitted**

## 1. Result

The missing structural boundary now exists:

```text
one subpatch job
  -> zhao_terrain_tess ModeVtx once
  -> each world vertex projected into every selected view
  -> one sealed projected arena per selected view
  -> zhao_terrain_tess ModeRef once
  -> each reference triple replayed through every selected view arena
  -> projected triangle packets
```

`zhao_terrain_pipe` composes:

- one `zhao_terrain_tess`;
- one `zhao_terrain_group_seq`;
- one `zhao_project_service`, which physically owns one `zhao_project_core`;
- one `zhao_terrain_wcache`, retaining three physical arena copies for simultaneous corner reads.

The old `zhao_terrain_topo` walker has been removed from `zhao_proj_subsystem`. It remains in the repository as superseded legacy/test support and is instantiated by the retained shell bench only. It was not deleted.

The geometry projection client remains connected as client A of the shared service. Terrain fills use client B. Lattice and cell-state ports remain external to this boundary.

## 2. One tessellation, selected-view fan-out

ModeVtx emits view-independent world vertices. The sequencer therefore presents each accepted job to ModeVtx once and holds each vertex until it has been accepted for every selected view. It does not re-tessellate per view.

ModeRef is likewise presented once. Each accepted index triple is held until every selected view's tagged arena reference has been accepted.

The differential exercises all legal nonempty view masks:

- `01`: view 0 only;
- `10`: view 1 only;
- `11`: both views.

The two views use different matrices and viewports. Output order, coordinates, depth, `w`, behind flags, source ID, view ID, materials, and weight are checked exactly.

Dense mode projects all 81 window vertices. Bitmap/sparse mode projects only vertices marked useful by TESS: 81/25/9/4 at LOD 0/1/2/3. Sparse fill against a dense arena is deliberately exercised as an illegal pairing; the seal-short and replay-refusal instruments fire.

## 3. Group lifecycle and release proof

The implemented lifecycle is:

```text
FREE -> OPEN(epoch,key) -> FILL -> SEALED -> REPLAY -> DRAIN -> FREE
```

The sequencer counts projection **accepts** per selected view separately from result **landings** per arena. It seals an arena only when its landed count equals that view's accepted-fill count. Landed riders are compared at full `ARENA_W`; an invalid/refusal encoding cannot alias a legal arena through truncated low bits.

Successful fills remain held through seal and replay. Rejected or empty ModeVtx jobs release their unsealed arenas only after TESS has fully drained. Successful groups release only when TESS reports the ModeRef job done, which occurs after every reference has completed every selected-view shell handshake.

Release remains safe while lookup replies or outputs are live:

1. A reference is accepted only when the shell accepts its lookup.
2. `zhao_vertex_arena` captures metadata and payload for that lookup at the accepted edge.
3. `zhao_terrain_wcache` stores copied reply payloads in its skid; it stores no arena handle.
4. The earliest later `open` can change arena metadata or RAM, but cannot change a reply already captured into the primitive or shell skid.

The composed differential deliberately accepts the next job and reopens arenas while an older copied output remains stalled. It now checks the complete output packet cycle by cycle throughout the stall as well as checking the final ordered stream.

There is no arena reference counter in this implementation. The earlier brief's statement that one already existed was incorrect. The acceptance/capture boundary and payload-only skid are the release mechanism.

## 4. Independent review repairs

The implementation handoff had already repaired two functional faults found by its first differential:

- invalid landed arena encodings could alias legal arenas through low-bit truncation;
- successful groups were released after ModeVtx and again after ModeRef (the failing receipt observed 21 releases where 12 were expected).

Independent review found and repaired two additional evidence defects:

1. The output-stall test checked only the packet eventually accepted. It now proves `out_valid` and every output field remain stable on every stalled cycle, including while the next job opens arenas.
2. `release_unsafe_o` watched only whether the currently presented triple still needed fan-out. That detector was blind to an early release immediately after a non-final triple completed its last selected-view handshake. It now treats the entire ModeRef job as live until `t_done_c`. The mutant uses one view, allowing its first presented triple to be fully accepted in the release cycle; the detector must still fire because later triples remain.

The mutant remains a renamed test-only copy. A normalized comparison against production shows one substantive difference: production releases in `StRef && t_done_c`; the mutant releases in `StRef && t_ref_valid_i`.

## 5. Functional evidence

Final independent receipts from the pinned repository toolchain:

| Gate | Result |
|---|---:|
| Dense composed differential | 478 terrain packets, 200 client-A vertices, 2,343 clocks, 37 checks passed |
| Bitmap/sparse composed differential | 478 terrain packets, 200 client-A vertices, 2,228 clocks, 33 checks passed |
| Release mutant | 3 inverted-polarity checks passed; `release_unsafe_o` required nonzero |
| Retained shell differential, dense | passed; 8,192 triangles |
| Retained shell differential, RPP1 correctness point | passed; 8,192 triangles |
| Retained shell differential, bitmap | passed; 8,192 triangles |
| Composition/downstream lints | 3/3 passed |
| Quartus-17 static source scan | 228 RTL files; self-test 3-fire/6-no-fire; passed |
| Production manifest | 223 modules = 66 top + 78 inside + 79 excluded; passed |
| Generated production top freshness | fresh; 66 instances |
| `git diff --check` | passed |

The differential covers LOD 0..3, morph and clamp, stitching, authored void, top and underside winding, rejection, empty legacy underside, no-view consumption, all view masks, exact counters, client-A/client-B contention, and output backpressure.

No Quartus executable was run for this packet.

## 6. Downstream rate, bounded to the measured question

The recovered direct probes were rebuilt and rerun:

- `GEOM.CLIP -> GEOM.SETUP`: 4,096 accepted triangles in 4,102 clocks, or 1.0015 clocks/triangle including fixed fill. A stream containing 1,536 rejects has the same offered duration. Under 30% output stalls, all 4,096 accepted outputs remain ordered.
- `GEOM.BINNER`: 128 one-tile terrain triangles in 640 bin clocks, exactly 5.00 clocks/triangle; drain 1,666 clocks; `TRI_CAP=128`.

Therefore CLIP and SETUP can consume the shell's one-triangle-per-clock output. No currently composed block after SETUP can. BINNER is the present downstream wall.

This does **not** justify collapsing the shell to one physical arena copy. The all-level-0 two-view producer demand is `2 x 256 x 16 x 128 = 1,048,576` triangles/frame, approximately 0.63 triangle/clock in a 1,666,666-clock frame. One single-read arena copy supplies only one corner per clock, or approximately one triangle per three clocks. The retained three copies answer the producer/CLIP boundary; changing the later binner is a separate integration decision.

`ARENAS=4` is group/lifetime capacity. It is not the number of physical RAM replicas. The shell has three physical copies because a triangle needs three simultaneous corner reads.

## 7. Resource and adoption disposition

This packet changes no production adoption state. `design/prod_manifest.yml` keeps the service, subsystem, sequencer, pipe, and arena shell excluded or `not-yet-adopted`. Production continues to account for the existing geometry and terrain projectors separately.

No ALM, DSP, M10K, RAM-inference, Fmax, or net-saving result is claimed. The future fit target is registered at the actual `zhao_terrain_pipe` subsystem boundary and asks exactly:

- is there exactly one `zhao_project_core` in the hierarchy;
- do all three `zhao_vertex_arena` copies infer as RAM;
- what are total ALM/DSP/M10K;
- do projector and arena-read paths hold 100 MHz?

The old 99-DSP programme headline is not legal. Dense two-view terrain at its RPP1/II3 point requires `663,552 x 3 = 1,990,656` clocks before geometry, exceeding the approximately 1,666,666-clock frame. The current legal structural frontier remains approximately 111 DSP, conditional and unfitted: 23 above the active 88-DSP allocation and 17 above the separate 94-DSP finishing limit. Being one DSP below provisional device capacity is not programme closure.

## 8. Explicitly open before adoption

1. **NORMALS:** ModeVtx supplies vertices and ModeRef supplies indices; the pipe has no world-space triangle stream for `zhao_terrain_normals`. A third ModeTri pass costs about 456 clocks for a level-0 job and does not fit the two-view schedule. The alternatives are a world-vertex arena/cache or normals produced/reused upstream. No option is adopted here.
2. **DEPTHQUANT:** all three projected `w` values now leave the shell, but downstream records/caches do not yet carry them into `zhao_geom_depthquant`.
3. **Physical gate:** after the two correctness seams close, run one clean subsystem-boundary fit at legal RPP3, including the accepted MATW18 profile where applicable. Do not fit RPP1 as the headline.
4. **Atomic adoption:** only after the seams and physical gate pass may production switch to this pipe, retire duplicate projection accounting, regenerate `zhao_prod_top.sv`, and rerun manifest and DSP-census gates.

Until all four are complete, this is a verified composition candidate—not an installed saving and not a cracked ceiling.
