# Contract — TERRAIN.PATCH (Terrain patch state engine)

> Ledger: `design/blocks.yml` · owner ZH-033 · phase 6 · maturity SPECIFIED
>
> Format law: `spec/terrain_rules.md` (Island Patch v1 — filled by the
> world-identity wave, RUN-20260816-0046). This contract is the Mantle entry
> point: it owns the bounded live-field intake and the per-vertex composition
> every other consumer reads.

## Purpose and exclusions

Own patch state layers (terrain_rules §2: header + layers A–H) and the bounded
field-list intake with bake/compose/reject-on-overflow (charter §11.4).

Implemented as `fpga/rtl/terrain/zhao_terrain_patch.sv`. **What the RTL is, as
built:** the §9.1 intake (16 footprint rectangles, accept/reject/trace) and the
§3.4 composition chain streamed one lattice vertex per clock, plus the 4×4
subpatch dirty mask that becomes `subpatch_requests`.

**What it is NOT, deliberately.** No VRAM port, no page loader, no page CRC
check, no scar writing and no breach law (TERRAIN.BAKE owns layers B and D), no
field-program evaluation (FIELD.SEQ.EARTH; terrain_rules §4.1 forbids a second
evaluator anywhere), no velocity lattice (TERRAIN.VELOCITY, phase 7), and no
`age`/`phase`/`start_tick` gating — that is dispatch-side and is visible in
`compose_lattice`'s own loop. The residency, page-CRC and MEM.GUARD grant
machinery this contract describes below under Memory ownership is **not built**;
it is what the block will need when a memory subsystem exists to attach it to.

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger.
Reset empties the field list, clears the dirty mask, both counters and the
result register. No clock-domain crossing lives in this block.

## Input and output packet layouts

`dispatch` — the §9.1 intake, one record per cycle, never stalls:

| field | width | meaning |
|---|---|---|
| `list_clear_i` | 1 | start of a patch's frame: empty the list and the dirty mask |
| `patch_id_i` | 16 | rides the reject trace event |
| `fld_add_x0_i` `z0_i` `x1_i` `z1_i` | signed 32 | footprint, fx16 raw, **closed** interval |
| `fld_add_hash_i` | 32 | program hash, trace only |
| `fld_add_cmd_i` | 16 | command index, trace only |
| `fld_add_accept_o` / `fld_add_reject_o` | 1 | registered pulses, mutually exclusive |
| `fields_active_o` | 5 | 0..16 |
| `trace_patch_id_o` `trace_hash_o` `trace_cmd_o` | 16/32/16 | the §9.1 law-2 trace event, valid with the reject pulse |
| `programs_rejected_o` | 32 | counter |

`patch_state` compose lane, ready/valid in and out:

| field | width | meaning |
|---|---|---|
| `base_i` `scar_i` `bottom_i` | signed 16 | layers A / B / C, height16 |
| `dual_i` | 1 | 0 = the legacy single-surface page (§3.1 option (a)) |
| `wx_i` `wz_i` | signed 32 | placed world position, fx16 — the footprint test's input |
| `vi_i` `vj_i` | 6 | lattice indices 0..32, for the subpatch mask |
| `src_id_i` | 16 | rides the packet |
| `top_o` | signed 32 | `live_top`, fx16 |
| `bottom_o` | signed 32 | `fx(bottom)`, or `live_top` on a legacy lattice |
| `compose_top_o` | signed 32 | pre-field, post-clamp |
| `st_dirty_o` | 1 | this vertex moved |
| `subpatch_dirty_o` | 16 | `subpatch_requests`: the 4×4 mask, bit `row*4 + col` |

`field_results` — `fld_valid_i` / `fld_ready_o` / `fld_height_i` (signed 32),
one height lane per ACCEPTED list entry per vertex, in list order.

Plus `terrain_samples_evaluated_o` (32) and `idle_o`.

## Backpressure rules

Ready/valid on the compose lane, the field-result stream and the output. **The
intake never stalls** — a record is accepted or rejected in the cycle it is
offered, which is what §9.1's "the frame never stalls" means in gates.

A vertex is taken only when the result register can receive its answer.
**That gives an invariant worth writing down, because the first version of this
block carried an UNREACHABLE stall term for a full output on the field port and
the directed suite found it:** `out_free` at the accept cycle T means
`!r_valid_T || st_ready_T`; in the first case `r_valid` is already 0, in the
second the retire fires, so `r_valid` is 0 at T+1 either way, and nothing raises
it again until this vertex's own chain completes. The result register is
therefore always free when the last field lane lands. `terrain_patch_directed`
case 6(b) asserts that directly rather than trusting the paragraph.

## Memory ownership

**As built: none.** The block has no VRAM port. Layers arrive on the compose
lane as three height16 words per vertex and the composed values leave on
`patch_state`; where they land is MEM.GUARD's and the composed-cache
allocator's business, neither of which exists yet.

**When they do:** reads layers A/C read-only and B/D read-only (TERRAIN.BAKE
owns those); the composed-height cache region (terrain_rules §4.2, §7/§8) is
this block's exclusive write grant.

## Q formats and rounding

**There is no rounding in this block at all**, and that is worth stating rather
than leaving to inspection.

height16 → fx16 is the EXACT `raw << 8` of qformats §2/§9. base/scar/bottom are
signed 16, so each is signed 24 as fx16 — exact, and it cannot saturate.
`fx(base) + fx(scar)` needs signed 25 and cannot saturate either. The saturating
add is still written faithfully because the FIELD lanes are full fx16 words and
those absolutely can saturate: **each add is done at 33 bits and narrowed with
the §3 saturate, one add at a time — never a wide accumulate then one narrow**,
which would silently disagree with the reference wherever a partial sum leaves
the word. `terrain_patch_directed` §3 pins that with a lane pair whose two
orders give different answers.

**Verilog signedness.** Every comparison here is between two signed operands. A
comparison goes unsigned if EITHER operand is; that trap cost a real bug in
`GEOM.BINNER` and made 29 tiles vanish (`design/contracts/GEOM.CLIP.md`).

## Latency (fixed or variable)

Variable, and exactly bounded: **1 cycle per vertex with no live field, and
1 + n cycles with n accepted field lanes.** The ledger says `variable`.

## Target throughput

The ledger asks for "1 patch-layer update per clock (lattice vertex compose per
clock steady state)". **Measured, not asserted:** with no live field on the
patch the compose lane accepts a vertex on **64 of 64 consecutive clocks** and
publishes on 63 (`terrain_patch_directed` §8 prints the count). That meets the
rate.

With n live lanes on the patch the rate is 1/(1+n) vertices per clock, because
each field result is consumed on its own cycle. That is inherent to the
vertex-major seam (chosen law 1) and is stated rather than hidden: a patch
carrying the §9.1 worst case of 16 lanes composes at 1/17 the no-field rate.
**§9.1 already says the frame-level field budget is NOT COSTED** until
FIELD.SEQ.EARTH's throughput is pinned, and this block does not change that.

## Overflow and malformed-input behaviour

**Input domain.** `vi_i`/`vj_i` in 0..32 (the 33×33 lattice). base/scar/bottom
are the whole height16 word. `wx_i`/`wz_i` and the field lanes are the whole
fx16 word — no guard band is assumed anywhere in this block, because nothing
here multiplies.

- **Field-list overflow: REJECT.** The first `MAX_PATCH_FIELDS = 16` records in
  command order win, every run, identically. Records beyond it are rejected —
  not composed, counted in `programs_rejected`, and emitted as a trace event
  {patch_id, program_hash, command index}. **Nothing already listed is ever
  evicted.** (The contract's older "rejects the lowest-priority cosmetic fields"
  wording was superseded 2026-08-16: the hardware has no priority notion, and
  charter §11.4 assigns priority to software above the seam.)
- **A footprint outside the patch envelope** needs no clipping: the per-vertex
  closed-interval test simply never fires. An INVERTED rectangle (x1 < x0)
  covers nothing, which is the reference's behaviour and not a case added here.
- **fx16 saturation** is per-add and matches `fx_add`'s exactly.
- A malformed dispatch record cannot reach this block (CMD.DECODER validates).

## Counters and traces

`terrain_samples_evaluated_o` counts **composed lattice vertices**, not cycles
and not offered packets. `programs_rejected_o` counts rejected records and
**survives `list_clear_i`** (chosen law 4). Both are pinned by directed cases.

No counter-catalog id is bound: minting one is a `spec/counters.md` amendment,
not an RTL decision.

## Scalar reference function

`zref::terrain::compose_vertex`, `zref::terrain::FieldList` and
`zref::terrain::subpatch_mask` in `reference/include/zref/zref_terrain_patch.hpp`.

**The ledger's `reference_model: zref::TerrainPatch` name was NOT used, and the
reason is a collision rather than a preference:** `zref::render::TerrainPatch`
already exists and is the patch RESOURCE (the cartridge page plus the in-memory
layers), which is this block's INPUT, not its model. Binding the ledger name to
an oracle would have made two different things share it. The oracle lives under
`zref::terrain::` as functions instead, which is also where `column_query`,
`bake_dig` and `apply_breach_law` already live.

`compose_vertex` is a **thin view onto `zref::render::compose_lattice`**, and
`terrain_patch_directed` proves it rather than asserting it: a real 33×33 dual
patch with scars, a thin authored lip, void cells and TWO real earth programs
with different footprints is composed through `compose_lattice` and through the
oracle, and **all 1,089 vertices must agree bit-for-bit** — with the cross-check
itself asserting it reached the live clamp (>200 vertices), the compose clamp,
and the uncovered-footprint path (>500 vertices). Without that layer the oracle
would be a second implementation of §3.4 and charter §29-6 would be broken
before the RTL was written.

The intake half has **no prior implementation anywhere in the tree** — the
oracle is its first, and it is written from §9.1's text.

## Directed tests

`tests/terrain/terrain_patch_directed.cpp` — **1,379 checks.** The
compose_lattice cross-check; both §3.4 clamps including the thin authored lip;
the legacy page ignoring `bottom` entirely; both height16 rails; the
closed-interval footprint on all four edges and one raw unit outside each; the
inverted rectangle; saturation order (two lane orders that provably disagree);
the intake at capacity, one past it, four past it, the trace event contents, no
eviction proved by composing the surviving 16 in order, `list_clear` semantics
including clear-and-offer in one cycle; **the subpatch mask at every one of the
1,089 lattice vertices** plus the 1/2/4-bit border cases by hand; backpressure
on both the result port and the field stream; the counters; and the throughput
measurement.

## Randomized differential tests

`tests/terrain/terrain_patch_random.cpp` — **14,730 checks** over 1,800
vertices and 180 patches (12× nightly). Two lanes on purpose:

- **Lane A, lattice-shaped:** an authored island — relief of a few tens of
  metres over a deep keel, small negative scars, 0..4 live programs, field lanes
  of a few metres. The regime §9.1's derivation says an ordinary frame is.
- **Lane B, domain-limit:** both height16 rails, field lanes at the fx16 word
  extremes, 0..24 offered records so the reject path is hammered, and a quarter
  of the pages legacy.

Both lanes assert they reached their interesting states: lane A must sample the
thin-lip compose clamp, the live clamp, uncovered footprints, moved ground and
**exact footprint-edge probes**, and must NEVER saturate; lane B must saturate,
must take the reject path, and must probe edges too.

**The edge probes exist because a mutation found the hole.** Turning one `<`
into `<=` in the footprint test left the random lanes GREEN while the directed
suite went red: uniform random world coordinates never land exactly on a
footprint edge, so a closed-versus-open boundary is a measure-zero event. Both
lanes now snap onto (and one raw unit either side of) a real footprint edge
often enough to sample it hundreds of times per run.

## Formal properties

**None, deliberately.** The two candidates were both weaker than the tests that
already exist. "The list never exceeds 16" is a one-line consequence of a
5-bit counter compared against a constant, and the directed suite already drives
four records past capacity and checks the surviving membership and its ORDER,
which a bounded proof of the count alone would not cover. "The composed-cache
write pointer never leaves its grant" is a MEM.GUARD property about a port this
block does not have yet. A proof with nothing to cover is worse than none.

## Synthesis / resource ceiling

**Not synthesized.** `fpga/files.qip` is untouched and this block has never been
through Quartus. Nothing here has run on hardware.

One sizing note for whoever does: the field list is 16 × 4 × 32 = **2,048 flops
of footprint rectangle**, held in registers because every one of them is read
combinationally by the per-vertex test. If that does not fit the
`geometry_mantle` budget the rectangles are the obvious thing to move into an
MLAB, at the cost of a read cycle per lane per vertex.

## Integration capture cases

**None yet.** Nothing upstream produces `dispatch` records or `field_results`
(FIELD.SEQ.EARTH's contract is still a stub) and nothing downstream consumes
`patch_state` in RTL — `TERRAIN.TESS` exists but reads a lattice through its own
memory ports rather than this block's stream, because the composed-height cache
that would sit between them does not exist yet. Composing the two is the next
increment, and it is where a port-level mistake would surface: it did for
`GEOM.BINNER`, where composing with the real rasterizer immediately exposed a
tile-index-versus-pixel error no isolated test could see, and it did again in
this very increment for `TERRAIN.TESS` → `TERRAIN.NORMALS`.

## Notes

**LAWS CHOSEN, NOT FOUND.** Each is also argued in the RTL header.

1. **Field results arrive VERTEX-MAJOR**, one per accepted lane per vertex, in
   list order. The reference composes lane-major (apps outer, vertices inner,
   mutating one lattice in place). The two agree bit-for-bit — `fx_add`
   saturates and is order-dependent, but the order that can change a result is
   the order of the adds AT ONE VERTEX, and both forms apply the lanes to a
   vertex in command order. **Rejected alternative:** the lane-major form, which
   needs the whole 33×33 fx16 accumulator resident (1,089 × 32 b = 4.25 KiB,
   five M10Ks) because a lane's pass must reach every vertex before the next
   lane's. That accumulator IS §4.2's composed-height cache, so if a later
   increment puts the cache inside this block the intake can be turned around
   without touching the arithmetic. **This is a requirement this block imposes
   on FIELD.SEQ.EARTH**, whose contract is still a stub — recorded here so it is
   negotiated rather than discovered.
2. **The footprint test lives HERE**, not upstream, and a lane whose footprint
   misses the vertex is consumed and DISCARDED rather than added as zero —
   identical in value and identical in saturation records. **Rejected
   alternative:** trusting the upstream to send only covering lanes, which makes
   the per-vertex result count data-dependent and moves a ratified law out of
   the block that owns composition.
3. **The subpatch dirty mask is `live_top != fx(base)`** — the ground actually
   moved — with border vertices marking BOTH neighbours (they are physically
   shared; the same closed-interval reasoning §9.1 uses for binning, and the
   reason a corner vertex marks four). **Rejected alternative:** marking from the
   field footprint rectangles alone. Cheaper and wrong: a crater's bounding
   rectangle is "dirty" in its corners where the field evaluates to exactly
   zero, so it marks subpatches whose ground did not move and defeats
   terrain_rules §4.4's "dirty patches only" entirely.
4. **`programs_rejected_o` and the trace survive `list_clear_i`.** The ledger's
   `counters:` line names only `terrain_samples_evaluated`, but §9.1 law 2 names
   `programs_rejected` and a trace event explicitly, so both are exposed. The
   counter is a frame-life diagnostic across every patch; the per-patch LIST is
   what `list_clear_i` empties. **Rejected alternative:** clearing it per patch,
   which makes the frame total unobtainable without summing pulses.

**MUTATION-CHECKED.** Four defects were injected one at a time, each proved to
have relinked by hashing the test binary before and after, and each confirmed to
turn BOTH lanes red before being reverted:

| mutation | directed | random |
|---|---|---|
| the §9.1 overflow accepted instead of rejected | 16/288 red | 2540/12386 red |
| the §3.4 live clamp at bottom removed | 1/288 red | 346/12847 red |
| the closed footprint interval made open on x0 | 4/288 red | 77/12750 red † |
| the subpatch mask's shared-border rule off by one cell | 6/1379 red | 635/14730 red ‡ |

† and ‡ **both initially failed in the random lane only after the lane was
fixed.** The footprint mutation left the random lanes green because a
boundary-exact coordinate is measure-zero under uniform sampling; the mask
mutation left them green because the lanes did not check the mask at all. Both
holes were closed (exact edge probes, and a per-vertex mask comparison) and the
mutations re-run. A mutation that only one lane catches is a report about the
other lane, not about the RTL.

**The stale-binary trap fired here too, and the hashes caught it.** After a
mutation was reverted, `build-verify` still held the mutated binaries; a
"baseline" run of them reported the mutation's failures as the tree's own. The
hash was identical to the mutated one, which is what gave it away. The mutation
harness now rebuilds after reverting, and writes the source twice with a build
between, because a single write plus build was observed to leave ninja believing
the tree was current.
