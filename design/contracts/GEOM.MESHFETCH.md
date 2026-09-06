# Contract — GEOM.MESHFETCH (Meshlet fetch and cull)

> Ledger: `design/blocks.yml` · owner ZH-037 · phase 8 · maturity REFERENCE_COMPLETE
> RTL: **`fpga/rtl/geometry/zhao_geom_meshfetch.sv`**

## Purpose and exclusions

Fetch meshlet descriptors, reject instance bounding spheres outside every active
camera frustum, and decide LOD per governor targets.

### The cull law (owner ruling, 2026-08-22)

This block used to be specified to "cull against camera visibility sectors".
That phrase appeared exactly twice in the repository — here and in the ledger
line this file was generated from — and nothing defined what a sector was.
**It is deleted. No sector system exists.** What replaces it:

* the descriptor carries a conservative **bounding sphere**: `bound_center` and
  `bound_radius`;
* for each ACTIVE camera, transform the bound into camera space and test the
  sphere against the frustum planes;
* **reject only when the sphere is outside EVERY active camera.** In Duo, cull
  only if outside both;
* optionally carry a two-bit visibility result (camera 0, camera 1) downstream,
  so work that genuinely is camera-specific is not duplicated;
* static and rigid meshes take an **asset-generated** bound; animated creatures
  take a **conservative animation-safe instance bound**. Per-pose exact bounds
  are explicitly NOT required — a loose bound costs performance, never geometry.

**Why the coarse cull belongs HERE and not in `GEOM.CLIP`.** This block feeds
`GEOM.VDECODE` and `GEOM.POSE`, so rejecting an invisible instance here avoids
compressed vertex fetch and decode, pose work, skinning, projection, setup,
binning and rasterisation. `GEOM.CLIP` receives already-projected individual
triangles; its scissor test is deliberately cheap and comes far too late to save
any of that. The two are complementary stages, not alternatives.

**Deliberately not built:** meshlet occlusion sectors, BSP cells, portals,
island visibility grids, Hi-Z occlusion. Each needs new scene-format laws,
dynamic-update behaviour and memory structures, and for a world of floating,
deforming and rotating terrain a rigid baked visibility system could become
actively unhelpful. Another rejection bit can be added in front of this block
later without changing this law.

### What is already built

The **LOD third** of this block exists: `fpga/rtl/geometry/zhao_geom_lod.sv`,
differential-tested against the shipped `zref::creature::lod_raw` and
`zref::creature::lod_update` in `tests/differential/geom_lod_directed.cpp`
(12,530 directed evaluations, 200,000 random per nightly run, 1,267,100 checks,
mutation sweep **26 attempted / 26 accounted / 25 caught / 1 equivalent**, the
equivalence proved in `tools/sweep_geom_lod.sh`'s header rather than labelled).
It carries no divider: every quotient in the ladder feeds a comparison, and
those are cross-multiplied exactly.

**It is SEQUENCED, and that is a port-visible fact.** The five products in the
ladder -- `thresh*R`, the three rung legality products and the switch boundary
-- walk through ONE multiplier, so:

* `ready_o` is high exactly when a tick will be accepted;
* `tick_i` is IGNORED when `ready_o` is low, so a caller that ticks blindly
  cannot corrupt an evaluation in flight;
* `valid_o` is a one-cycle pulse **5 clocks** after the accepting edge, and
  `rung_o` / `hold_o` / `raw_o` are that tick's answer on that cycle;
* the inputs are latched at accept, so the caller may move on immediately.

Sustained rate is one evaluation per five clocks, or 10 M/s at 50 MHz, against a
demand of roughly 600 k/s for ten thousand live creatures at 60 Hz. The
differential CHECKS the five rather than merely waiting it out, because a
latency that changes silently is a contract break even when the answer is still
right.

The **cull third** exists too, as of 2026-08-22:
`fpga/rtl/geometry/zhao_geom_cull.sv`, against `zref::cull`
(`reference/include/zref/zref_cull.hpp`) in
`tests/differential/geom_cull_directed.cpp` — 11,090 directed instance verdicts
(17,212 checks), 4,000 random per fast run, mutation sweep **32 attempted / 32
accounted / 30 caught / 2 equivalent**, both equivalences proved in
`tools/sweep_geom_cull.sh`'s header rather than labelled.

Three things about it are worth stating here because they are decisions, not
transcription:

* **FIVE planes, not six.** `project_vertex`'s only depth condition is
  `clip.w > 0`; there is no z clip in this renderer, so `row2` is never read. A
  sixth plane would reject geometry the renderer would have drawn.
* **The length bound rounds UP.** The plane rows are not unit-length, so the
  test compares against `radius * |normal|`, and the available primitive
  (`isqrt_u64`) is an exact FLOOR square root. A floor makes rejection easier
  and deletes visible geometry at the screen edges; the ceiling only wastes
  decode work. The differential measures that it can see the difference: 1,015
  of 1,775 boundary probes answer differently under a floor.
* **The bound arrives as PORTS**, not from a descriptor. The five planes and
  their five square roots are extracted once per camera per frame (185 cycles on
  a matrix write); the per-instance path is 10 cycles and four multipliers.

Both blocks are SIMULATION ONLY. `zhao_geom_lod` has a Quartus block fit
**(1,183 ALMs / 6 DSPs / 271 registers, at `09bbe05`)**, down from 1,303 ALMs
and 18 DSPs before the products were sequenced -- both numbers measured on this
machine, at a clean worktree, on the commit each row names.
`zhao_geom_cull` has been fitted since (1,102 ALMs / 15 DSPs) and is the obvious
next candidate for the same lever: four multipliers on a path that already takes
10 cycles for a per-instance rate.

The **descriptor-fetch third is built** in
`fpga/rtl/geometry/zhao_geom_meshfetch.sv`. It captures ingress at job
acceptance, requests the frozen 64-byte descriptor, stores eight 64-bit words,
validates in the oracle's refusal order, transforms the local bound, and drives
the existing cull service. Its leaf directed gate is
`tests/geometry/geom_meshfetch_rtl_directed.cpp`; the formal refusal property is
`tests/formal/geom_meshfetch_refuse.sby`.

The remaining production work is composition, not inventing a schema or an
asset region: `spec/memory_rules.md` §5f allocates `GEOM.ASSET_POOL`, and
MESHFETCH shares its single permitted ENGINE1 physical client with ASSETFETCH
through `zhao_geom_mem_adapter`.

## Clock and reset semantics
Single `gpu_clk` domain, synchronous active-low `rst_n`, like every other
geometry block. No CDC inside this block: the descriptor arrives from
`MEM.GUARD` already in `gpu_clk`.

On reset the block drops any fetch in flight, clears its descriptor staging and
returns to idle with `job_ready_o` high. **An in-flight memory read whose
response arrives after reset is discarded by tag**, not by counting — a
response that outlives its requester is exactly the case a counter gets wrong.

The active-camera planes and their square roots live in `zhao_geom_cull` and
survive reset only by being rewritten; a frame that does not write them gets no
cull, which is the safe direction (nothing is rejected).

## Input and output packet layouts
### The meshlet descriptor — FROZEN, owner ruling 2026-08-31 §6.1

64 bytes, 64-byte aligned, so one descriptor is exactly one aligned burst and
never straddles a row.

**This is eight packed 64-bit words, not four.** A later recovery brief and the
first logical-owner-adapter fixture called MESHFETCH a 32-byte requester, but
that shorthand contradicts this frozen layout: bytes 32..35 carry generation
and mesh identity, bytes 36..59 are the reserved-zero compatibility fence, and
bytes 60..63 carry the CRC. The adapter may remain length-generic, but production
MESHFETCH must request all 64 bytes. The RTL's emitted `len` is checked directly
rather than inferred from the eight beats supplied by its bench.

ENFORCED-BY: `tests/geometry/geom_meshfetch_rtl_directed.cpp` (descriptor request
length)

| off | size | field | notes |
|---|---|---|---|
| 0 | 1 | `format_id` | descriptor version. Unknown value ⇒ refuse, never guess |
| 1 | 1 | `flags` | b0 skinned, b1 two-weight, b2 cel material, b3-7 reserved 0 |
| 2 | 1 | `vertex_count` | **≤ 64**, ruling limit |
| 3 | 1 | `triangle_count` | **≤ 126**, ruling limit |
| 4 | 2 | `material_id` | |
| 6 | 2 | `lod_error` | fx16 screen-space error at unit distance |
| 8 | 12 | `bound_centre[3]` | **fx16 S15.16, MESHLET-LOCAL** (ruling) |
| 20 | 4 | `bound_radius` | fx16, unsigned |
| 24 | 4 | `vertex_offset` | byte offset into the mesh's vertex stream |
| 28 | 4 | `index_offset` | byte offset into the u8 local-index stream |
| 32 | 2 | `generation` | stale-handle detection |
| 34 | 2 | `mesh_id` | owning mesh, for traces and counters |
| 36 | 24 | `reserved` | **must be zero**; a nonzero byte refuses |
| 60 | 4 | `crc32c` | over bytes 0..59, the frozen CRC step |

`bound_centre` being **meshlet-local** is the ruling's answer to the first of
this block's three long-standing integration questions. The instance transform
carries it to world space; **the radius is scaled by the maximum absolute
instance scale**, which is the answer to the second. Maximum-absolute rounds the
bound outward under non-uniform scale, and that is the correct direction — a
loose bound costs decode work, a tight one deletes geometry.

`u8` local indices are why `vertex_count ≤ 64` and `triangle_count ≤ 126` are
limits rather than suggestions: 126 triangles × 3 indices is 378 bytes, and the
index value must address ≤ 64 local vertices.

### Job in

`{ instance_id, mesh_id, descriptor_base, instance_transform_id, lod_target,
active_camera_mask }`

`active_camera_mask` answers the third integration question: **the caller
drives it**, per frame, and it is the same mask `GEOM.PROJECT` uses. This block
does not decide which cameras are live.

### Result out

`{ instance_id, meshlet_index, visible_mask[1:0], rung, hold, vertex_offset,
index_offset, vertex_count, triangle_count, material_id, flags }`

`visible_mask` is the per-camera result the purpose section already promised, so
downstream work that is genuinely camera-specific is not duplicated. A meshlet
with `visible_mask == 0` is **not emitted at all**.

## Backpressure rules
Ready/valid on both channels, and the house rule holds: `job_ready_o` never
depends on `job_valid_i`, and `res_valid_o` never depends on `res_ready_i`.

The descriptor fetch is a memory client, so it is subject to `MEM.GUARD`'s
grant. The block therefore has **two** backpressure sources — the downstream
consumer and the memory system — and must not deadlock when both stall: an
issued read is always retired into staging before a new one is issued, so the
block can always make progress on its response path regardless of the
consumer.

A `zhao_skid2` on the result channel decouples the consumer's ready from the
fetch state machine. That block exists (`fpga/rtl/common/zhao_skid2.sv`) and was
added for exactly this class of problem in `RASTER.EARLYZ`, where a downstream
ready reached 256 register inputs in one clock.

## Memory ownership
Reads only. This block **never writes memory.**

It reads meshlet descriptors through `MEM.GUARD` as an ordinary client.

**The client port shape, named 2026-09-04 because "an ordinary client" is not
enough to build from.** `MEM.GUARD`'s response carries `{ready, ok, violation}`
and **no data**, so a read client needs a separate return channel. The one that
exists is `zhao_scanout_fetch.sv`'s — the tree's only guarded READ client, and
the shape `zhao_mem_guard.sv` itself is written against (`scan_ok = !req.write`):

    output zhao_guard_req_t guard_req,   // write=0, addr, len, be
    input  zhao_guard_rsp_t guard_rsp,   // {ready, ok, violation}
    input  logic            beat_valid,
    input  logic [63:0]     beat_data,
    input  logic            beat_last    // conformance: 8 beats per request

A 64-byte descriptor is therefore **one request at `len = 64` and exactly eight
64-bit beats**, which is why the descriptor is 64-byte aligned in the first
place — the alignment ruling and the burst shape are the same fact.

Reusing this port rather than inventing one matters beyond tidiness: `MEM.GUARD`
distinguishes read clients from write clients by `req.write` and checks a
different region for each, so a read client that arrived with a novel port shape
would also need a new region rule. There is nothing to design here. It does
not own the vertex or index streams — it emits *offsets into them*, and
`GEOM.VDECODE` performs those reads. That split is deliberate: it keeps one
block per memory concern and it means a descriptor-fetch stall cannot hold a
vertex burst open.

Descriptor residency is the caller's problem. This block does not cache. A
descriptor cache is a Class-B evidence question (measure the hit rate on a real
256-creature trace first) and must not be assumed into the first build.

## Q formats and rounding
| value | format | rounding |
|---|---|---|
| `bound_centre` | fx16 S15.16 | none — carried, not computed |
| `bound_radius` | fx16 unsigned | scaled by max abs instance scale, **rounded UP**, and **SATURATED at `0xFFFFFFFF`** |
| world centre | fx16 S15.16 | `rescale(.,16)` per lane, **round-half-up** |
| `lod_error` | fx16 | none |
| plane tests | as `zhao_geom_cull` | `isqrt_u64`, exact floor, **used as a ceiling** |

**The WORLD CENTRE's rounding was not in this table and now is: `rescale(.,16)`
per lane, fx16 round-half-up, ties toward +infinity (`qformats.md` §3/§4).**
Added 2026-09-04 while writing the oracle against this contract. The row above
says `bound_centre` takes "none — carried, not computed", which is true of the
DESCRIPTOR field and says nothing about the world centre the instance transform
produces — and that value *is* computed, once per lane, with a rescale that has
to round somehow.

An RTL author reading this table could reasonably have truncated. Oracle and
RTL would then agree everywhere except on ties, which is the worst possible
place to disagree: rare enough to survive a directed test, common enough to
appear in a long capture. The oracle uses round-half-up because that is the
house rule for fx16; this line exists so the RTL is not free to choose.

**The radius rounds up and the plane length bound rounds up.** Both directions
are chosen so error costs work rather than geometry. `zhao_geom_cull`'s existing
differential already measures that this matters: 1,015 of 1,775 boundary probes
answer differently under a floor.

**The radius SATURATES.** `bound_radius` is fx16 unsigned, so a legal
descriptor may carry a radius near 2^32 and a legal instance may scale it; the
product overflows a u32 after the shift. Wrapping would turn a huge bound into a
SMALL one, which is exactly the geometry-deleting direction the round-outward
rule exists to prevent, and fx16's house behaviour is "saturate + record"
anyway. A saturated bound is merely loose — it costs decode work and draws
everything. The oracle had this defect on its first version and the randomized
suite did not catch it, because that generator used bounded scales; it was found
by reading this table against the code.

No new arithmetic law is introduced by this block. The cull maths is
`zref::cull`'s and is already proved; the LOD ladder is `zref::creature::lod_*`
and is already proved.

## Latency (fixed or variable)

Variable. The service latencies are fixed once accepted; the descriptor path
also waits on shared memory and downstream result backpressure.

| third | latency | measured by |
| --- | --- | --- |
| LOD ladder (`zhao_geom_lod`) | **fixed, 5 clocks** from the accepting edge to the `valid_o` pulse; one evaluation in flight at a time (`ready_o` low meanwhile) | asserted on every evaluation in `tests/differential/geom_lod_directed.cpp` |
| cull (`zhao_geom_cull`) | 185 cycles per camera on a matrix write; **10 cycles** on the per-instance path | `tests/differential/geom_cull_directed.cpp` |
| descriptor fetch (`zhao_geom_meshfetch`) | variable MEM.GUARD wait + exactly eight accepted data beats; once the final beat lands, ten sequenced bound-transform clocks precede the cull request | `tests/geometry/geom_meshfetch_rtl_directed.cpp` |

The LOD ladder's five is a DESIGN CHOICE, not a limit: it sequences five
products through one multiplier and cost 12 of 18 DSPs to do so. If a future
consumer needs the answer in one clock, the parallel form is one commit back
(`d8278bd`) and costs 18 DSPs of 112. That trade is on `docs/OWNER_DOCKET.md`.

## Target throughput
One meshlet decision per **5 clocks** sustained, set by the LOD ladder's
sequenced multiplier, unless a descriptor fetch misses and the memory path
dominates.

Sized against the ruling's content tier — 256 creatures — rather than a guess.
At, say, 24 meshlets per creature that is ~6,100 meshlet decisions a frame, or
~30,700 clocks at 5 each: **about 2.3 % of a 1,333,333-clock frame.** The
descriptor traffic is 6,100 × 64 B ≈ 390 KiB/frame, which is the number that
actually needs checking against bandwidth, not the clock count.

If that ratio turns out wrong, the lever is the LOD ladder's five clocks — it
sequences five products through one multiplier and cost 12 of 18 DSPs to do so.
Un-sequencing it buys throughput for area, and the trade is already measured.

## Overflow and malformed-input behaviour
**Refuse, never guess.** Every one of these raises a refusal with the offending
`instance_id` and `mesh_id` in a counter, and emits no meshlet:

| condition | why it is fatal to the descriptor |
|---|---|
| `format_id` unknown | a future format read as this one produces plausible garbage |
| `crc32c` mismatch | the descriptor is not trustworthy in any field |
| `generation` mismatch | the asset moved under a live instance |
| `vertex_count > 64` | u8 local indices cannot address it |
| `triangle_count > 126` | exceeds the frozen limit |
| any `reserved` byte nonzero | a future field is being used by an older reader |
| `bound_radius == 0` on a non-empty meshlet | a zero bound culls everything, silently |

**A refused descriptor does not fault the frame.** It drops one meshlet and
counts it. That is deliberately different from the ruling's hard-overflow law
(fault, drain, repeat the previous frame), which governs *capacity* exhaustion —
losing the tail of an army — not one corrupt descriptor.

## Counters and traces
* `meshlets_considered`
* `meshlets_culled_all_cameras`
* `meshlets_visible_camera0`, `meshlets_visible_camera1`
* `descriptors_fetched`, `descriptor_bytes`
* `descriptors_refused_by_reason[7]` — one per row of the table above
* `lod_rung_histogram[N]` — which rungs the frame actually used
* `fetch_stall_cycles`, `consumer_stall_cycles` — the two backpressure sources
  counted **separately**, because "the block was slow" is not an actionable
  statement when it has two independent reasons to be

Source ids propagate, so a refused descriptor is attributable to the command
that introduced the instance.

## Scalar reference function
`zref::MeshFetch` (ledger `reference_model`), whose entry point is
`decide(descriptor, instance, cameras, lod_target)` returning
`{ accepted, refusal_reason, visible_mask, rung, hold }`.

It must **compose the two existing oracles rather than restate them**:
`zref::cull` for the frustum test and `zref::creature::lod_raw` /
`lod_update` for the ladder. Both are already differentially proved against
built RTL, and a reimplementation here would be a second law that agrees until
it doesn't.

What is genuinely new in this oracle, and therefore what it actually owns:

* descriptor validation — CRC, format, generation, the two count limits, the
  reserved bytes;
* the local-bound → world-bound transform, with the radius scaled by **maximum
  absolute instance scale**;
* the refusal taxonomy.

## Directed tests
`tests/geometry/geom_meshfetch_directed.cpp`.

The cases that matter are the boundaries, not the happy path:

* `vertex_count` 64 accepted, 65 refused; `triangle_count` 126 accepted, 127
  refused — the u8-index limits, exactly at the edge;
* one bit flipped in each of the 60 CRC-covered bytes: **60 refusals**, no
  acceptances;
* every reserved byte nonzero in turn: 24 refusals;
* `generation` off by one: refused;
* a meshlet outside camera 0 and inside camera 1: **accepted**, `visible_mask ==
  0b10`. This is the Duo law and the single most valuable case in the file;
* outside both: rejected, and rejected is not the same as refused — one is
  geometry that is not visible, the other is a descriptor that is not
  trustworthy, and the counters must not conflate them;
* non-uniform instance scale: the world radius must be ≥ the exact transformed
  bound in every axis. Assert the INEQUALITY, not a value, because the point is
  the rounding direction;
* `bound_radius == 0`: refused, because a zero bound silently culls everything.

## Randomized reference-invariant tests
`tests/geometry/geom_meshfetch_random.cpp` exercises five oracle invariants over
20,000 iterations each on a named seed, biased toward count limits, plane
boundaries, scale edges and corrupt descriptors. It is deliberately not called
an RTL differential: it can find a rule that fails on the ninth descriptor, but
cannot expose an oracle and RTL that are confidently wrong in the same way.

Mutation evidence: widening the CRC window from 60 to 64 bytes refuses every
otherwise-legal descriptor and fails two of the five properties.

A true randomized RTL differential remains future evidence for a higher
maturity state; this contract does not silently relabel the oracle-only sweep.

## Formal properties
`tests/formal/geom_meshfetch_refuse.sby`:

* **no emission after refusal** — for every refusal reason, `res_valid_o` never
  rises for that job. This is the safety property: a partially validated
  descriptor must not reach `GEOM.VDECODE`, because its offsets would be read as
  memory addresses;
* **handshake hygiene** — `job_ready_o` is independent of `job_valid_i`, and
  `res_valid_o` is independent of `res_ready_i`;
* **no lost job** — every accepted job eventually produces exactly one result or
  exactly one refusal, never both and never neither, under arbitrary
  backpressure on both the memory and consumer sides;
* **reset drops in flight** — after `rst_n`, no result attributable to a
  pre-reset job is ever emitted.

## Synthesis / resource ceiling
Measured, per third, on this machine at a clean worktree:

| third | ALMs | DSPs | note |
|---|---|---|---|
| `zhao_geom_lod` | 1,183 | 6 | at `09bbe05`, after sequencing five products through one multiplier (was 1,303 / 18) |
| `zhao_geom_cull` | 1,102 | 15 | the obvious next candidate for the same lever — four multipliers on a path that already takes 10 cycles |
| descriptor fetch (`zhao_geom_meshfetch`) | — | — | RTL and leaf proof exist; no independent source-matched block fit is claimed here |

**Ceiling for the composed block: 3,000 ALMs and 24 DSPs.** That is the two
measured thirds plus room for a fetch state machine, and it is a budget to be
checked against, not a prediction. The composed shell fit currently sits at
12,532 ALMs of 41,910, so this block is affordable at that size — but the
`gpu_clk` path is the constraint, not area, and any addition is subject to the
same rule: *latency may grow; initiation rate and exact arithmetic may not
regress.*

## Integration capture cases
* **256 creatures, Duo** — the ruling's content tier. Confirms the per-camera
  visible masks differ between the two views, and that no meshlet admitted by
  The Measure is lost to a full arena. This is the trace that sizes
  `GEOM.PARAMBUF`, so it must be captured, not estimated.
* **one creature crossing a frustum edge** — the visible mask flips 0b11 → 0b10
  → 0b00 across frames with no pop and no gap.
* **a corrupt descriptor mid-frame** — one meshlet drops, the frame completes,
  the counter attributes it to the introducing command. Explicitly NOT a frame
  fault.
* **LOD ladder sweep** — a creature walked from 8 m to 300 m, confirming the
  rung histogram moves monotonically and the hold prevents oscillation at each
  boundary.

## Notes

Meshlet limits and the 64-byte descriptor layout are frozen owner rulings; a
future descriptor format must use a new `format_id`, never reinterpret these
bytes.

## Historical maturity blocker — superseded

On 2026-09-04, the ledger could not advance because this contract cited a
randomized RTL differential and a formal harness that did not yet exist. The
oracle-only randomized invariants landed first; the descriptor RTL, leaf
directed test and `tests/formal/geom_meshfetch_refuse.sby` then made those old
"unbuilt" statements false. The ledger now records REFERENCE_COMPLETE.

No higher maturity is claimed by this descriptor-width correction. Advancing it
must use the repository's normal evidence gate rather than treating the
existence of files as proof of every required state.
