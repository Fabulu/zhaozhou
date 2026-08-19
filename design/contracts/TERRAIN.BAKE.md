# Contract — TERRAIN.BAKE (Persistent scar bake + breach law)

> Ledger: `design/blocks.yml` · owner ZH-036 · phase 7 · maturity SPECIFIED
>
> Format law: `spec/terrain_rules.md` §3.4 (world-identity wave). This block
> is the ONLY writer of the scar plane (layer B) and the cell-state plane
> (layer D) — every permanent wound, every breach, every heal is born here,
> deterministically, and mirrored bit-exactly by the sim.

## Purpose and exclusions

Bake stamp records into the persistent scar delta (height16, incremental
scaling — terrain_rules §9), then evaluate the breach law (terrain_rules
§3.4): a SOLID cell with `no_bake = 0` whose composed top equals bottom at all
four corners becomes VOID_BREACHED; a bake that lifts any corner heals it back
to SOLID. Canonical scars are mirrored by SW.CPUCOLL.

Implemented as `fpga/rtl/terrain/zhao_terrain_bake.sv` plus the factored
combinational core `fpga/rtl/terrain/zhao_terrain_bake_delta.sv`.

**What the RTL is, as built.** A two-phase per-record engine that OWNS its own
sweep:

- **DIG phase** — 33×33 lattice vertices in z-then-x scan order. Per vertex the
  block places the lattice point (`zref::terrain::lattice_lerp`), evaluates the
  paraboloid stencil (an exact 17-step restoring divide), applies the
  incremental delta `g(from) − g(to)`, the no_bake clamp and the height16
  rails, and emits the new layer-B word.
- **BREACH phase** — 32×32 cells in the same scan order, consuming layer D and
  emitting the §3.4 transitions in `apply_breach_law`'s own order. Skipped
  entirely when layer C or layer D is absent, because `apply_breach_law`
  returns empty in exactly those cases.

The bridge is the **`meets` plane**: one bit per lattice vertex,
`base + scar ≤ bottom` on the height16 grid, which is `apply_breach_law`'s
`meets_bottom` (its own comment: "compose_top == bottom after the §3.4 clamp
⇔ base + scar ≤ bottom"). 1,089 flops, held in this block.

**What it is NOT, deliberately.** No VRAM port and no residency directory — a
stamp naming a non-resident patch is the caller's no-op and there is no
directory here to check it against; no page CRC; no live-field composition
(TERRAIN.PATCH); no surface sheet (SURFACE.STAMP owns layer F); no keel or
bottom generation (`zref::terrain::generate_bottom` is a load-time tool); and
no byte-stencil asset fetch — `zref_terrain.hpp` states plainly that "a
byte-stencil bake lands with the asset lane", and when it does, the paraboloid
evaluator is the one piece that is replaced.

## THE SHEET SEAM — explicitly undecided, not invented

`design/blocks.yml` gives this block `inputs: [stamp_results]` with
`upstream: [SURFACE.STAMP]`. **`stamp_results` names two different wires in
this tree**, and the conflict is recorded here rather than resolved by fiat:

| | SURFACE.STAMP's landed `stamp_results` | this contract's packet table |
|---|---|---|
| shape | one record per **texel** | one record per **stamp** |
| fields | `{texel, tag, strength_after, strength_before}` | `{patch_id, stencil, from fx16, to fx16, footprint}` |
| extent | layer F, 64×64 | layer B, 33×33 |
| element | `{tag u8, strength u8}` | height16 |

**This block takes the stamp RECORD**, because that is the form with
arithmetic behind it: `zref::terrain::bake_dig` — the reference this block is
the hardware image of — takes `{DigStamp, depth_from, depth_to}` and nothing
else, and terrain_rules §9.2's deferral identity is written in `from`/`to`
depths and in nothing else.

**What closing the other seam needs, so the next increment negotiates it
instead of discovering it:** two laws that do not exist anywhere in this tree.

1. A **strength(u8) → depth(fx16) mapping**. `SURFACE.STAMP`'s strength byte is
   an appearance value with no metric meaning; nothing anywhere converts it to
   metres.
2. A **64×64 → 33×33 resample**. Layer F texels are cell-centred over the
   envelope (`stamp_surface`'s `(2i+1)/128` rule) and layer B is per-vertex;
   the two grids share no sample point at all. A nearest-texel rule, a
   four-texel average and a bilinear tap all give different craters and the
   choice is a look decision, not an arithmetic one.

Both are the kind of law `spec/terrain_rules.md` §11 exists to hold, and
inventing them here would put a fabrication under every permanent wound in the
game. Recorded, not hidden — the SURFACE.SHEET discipline.

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger.
Reset returns the block to idle, clears the record, the `meets` plane, the
frame window and all four counters, and drops both output registers — so no
partial record can survive a reset. The block has no clock-domain crossing.

The older contract said "bake commits per dirty-rectangle, double-buffered then
flipped". **As built there is no buffer to flip**: the block is a stream
processor and the double-buffering, if it is wanted, belongs to whatever owns
the VRAM page. The wording is corrected rather than left to imply a mechanism
that is not there.

## Input and output packet layouts

`stamp_results` — one patch-bake record, accepted in one cycle:

| field | width | meaning |
|---|---|---|
| `cmd_patch_id_i` | 16 | rides the bake as `trace_patch_id_o` |
| `cmd_cx_i` `cmd_cz_i` | signed 32 | stencil centre, fx16 raw, island-datum |
| `cmd_radius_i` | signed 32 | stencil radius, fx16 raw; **≤ 0 writes nothing** |
| `cmd_depth_from_i` `cmd_depth_to_i` | signed 32 | ABSOLUTE depths, fx16 raw |
| `cmd_env_x0_i` `z0_i` `x1_i` `z1_i` | signed 32 | the patch envelope, fx16 raw |
| `cmd_dual_i` / `cmd_cells_i` | 1 | layer C present / layer D present |
| `cmd_src_id_i` | 16 | rides both output streams |

DIG lane — the block drives the address, the caller answers:

| field | width | meaning |
|---|---|---|
| `vtx_vi_o` `vtx_vj_o` | 6 | the lattice word the block is asking for |
| `vtx_base_i` `vtx_scar_i` `vtx_bottom_i` | signed 16 | layers A / B / C |
| `vtx_nobake_i` | 1 | the §3.3 no_bake **corner shadow** of this vertex |
| `sc_scar_o` | signed 16 | layer B out |
| `sc_vi_o` `sc_vj_o` | 6 | which vertex this answer is for |
| `sc_touched_o` | 1 | the vertex was inside the stencil |
| `sc_meets_o` | 1 | `base + scar ≤ bottom` (the §3.4 equality) |
| `sc_clamped_o` | 1 | the no_bake clamp fired on this vertex |

BREACH lane — same shape over cells:

| field | width | meaning |
|---|---|---|
| `cell_ci_o` `cell_cj_o` | 6 | the cell the block is asking for |
| `cell_state_i` / `cs_state_o` | 8 | layer D in / out |
| `cs_event_o` | 1 | a §3.4 transition happened: the trace event |
| `cs_sub_o` | 2 | the new substance, valid with `cs_event_o` |

`frame_start_i` (1) / `budget_full_o` (1) / `bakes_this_frame_o` (8) carry the
§9.2 window. `dig_done_o` and `bake_done_o` are 1-cycle pulses;
`breach_active_o` tells the caller the block wants cells, not vertices.

## Backpressure rules

Ready/valid on the record, both read lanes and both write lanes. A vertex is
taken only when the scar register can receive its answer, which gives the same
invariant TERRAIN.PATCH writes out and for the same reason: `sc_free` at the
accept cycle T means the register is free at T+1, and nothing raises it again
until this vertex's own divide completes, so the emit state carries no stall
term — an unreachable one is precisely what the directed suite caught in
TERRAIN.PATCH.

**The §9.2 cadence budget is enforced by BACKPRESSURE.** `cmd_ready_o` drops
once `BAKE_PATCH_BUDGET = 64` records have been accepted in the current frame
window and returns on `frame_start_i`. §9.2 law 2 wants the remainder to carry
"to the head of the next frame's window, ahead of newly issued bakes (FIFO)";
refusing to accept leaves the record exactly where it was, at the head of the
upstream queue, so the FIFO order is preserved **structurally** and this block
cannot drop or reorder anything at all.

## Memory ownership

**As built: none.** The block has no VRAM port. Layers arrive on the two read
lanes and leave on the two write lanes.

**When a memory subsystem exists:** exclusive writer of layers B and D via its
MEM.GUARD grant, reader of A and C — terrain_rules §7.

## Q formats and rounding

`g(depth) = rescale_s32(rescale_s32(depth × s, 16), 8)`, and
`delta16 = g(from) − g(to)` — the reference's lambda, operator for operator.
**Two rescales, two roundings, in that order**, never one fused shift by 24,
which rounds differently.

`s` is the paraboloid stencil in Q16: `s = ((r² − d²) << 16 + r²/2) / r²`,
computed by a **17-step restoring divide** that is exact over the whole input
domain. `s ≤ 65536` is structural: the numerator is bounded by `r² × (2^16+1)`.

The no_bake clamp bounds scar so `base + scar ≥ bottom + 1` height16 LSB on a
protected vertex. The height16 rails are ±32767 / −32768. **Both apply to
TOUCHED vertices only** — `bake_dig` `continue`s past an uncovered vertex
before either can run, so an out-of-range scar word already in layer B is
passed through rather than quietly corrected. That asymmetry is the
reference's and it is kept.

**Verilog signedness.** `meets` is an 18-bit signed compare and it is written
with explicit `$signed` on both sides. **The unsigned form was actually built
first and the tests caught it**: a concatenation is unsigned, a comparison goes
unsigned if either side is, and a composed height of −200 read as 261,944 — so
every cell on the island breached. Same trap as GEOM.BINNER's 29 vanished
tiles.

## Latency (fixed or variable)

Variable. Per record: 1 cycle to accept, then **2 cycles per uncovered vertex
and 19 per covered vertex**, then 1 cycle per cell when the breach phase runs.

## Target throughput

The ledger asks for **1 bake texel per clock**. **This block does not meet it,
and the number is measured rather than derived** (`terrain_bake_directed` §10
prints it):

| case | measured |
|---|---:|
| uncovered vertex (outside the stencil) | **2.02 clocks/vertex** |
| covered vertex (inside the stencil) | **19.00 clocks/vertex** |
| breach phase | **1.00 clocks/cell** |

The 19 is the 17-step exact divide plus its two handshake cycles. What it costs
against the frozen §9.2 budget, stated rather than left to inference: §9.2
derives 64 × 1,089 = 69,696 cycles/frame ≈ 4.2% of a 1.67 M-cycle frame **at 1
texel/clock**. At the measured rate the same 64 patch-bakes cost between
141,000 cycles (stencils that miss most of the page) and 1,324,000 cycles
(64 page-covering stencils) — i.e. **up to ~79% of a frame**, which is not
affordable. Two honest fixes, neither taken here:

1. **Pipeline the divider.** The divisor `r²` is constant for a whole record,
   so 17 stages of compare-subtract with a fixed shifted divisor per stage give
   1 vertex/clock at roughly 17× the divider's area (~1,400 registers and 17
   80-bit subtractors). This is the mechanical fix and it changes no
   arithmetic.
2. **Fetch the stencil instead of computing it.** `zref_terrain.hpp` already
   says the donor's 33×33 ubyte `volc.DATA` stencil is "the asset-shaped
   ancestor" and that a byte-stencil bake lands with the asset lane. A fetched
   stencil has **no divide at all**, and the rate falls straight out at 1
   texel/clock. That is the better fix, and it is an asset-lane decision rather
   than an RTL one.

Until one of them lands, §9.2's "engine cycles ≈ 4.2% of a frame" line is
**optimistic by up to 19×** and should be read as the post-fix figure.

## Overflow and malformed-input behaviour

**Input domain.** `bake_dig` computes `dx*dx + dz*dz` in int64, so it is exact
only while `|vx − cx| < 2^31` and `|vz − cz| < 2^31` — guaranteed whenever
every coordinate lies within ±2^30 fx16 raw (±16,384 world metres, four times
the ±4,096 m envelope SURFACE.STAMP states for the same reason). The RTL
datapath is sized to be exact over that whole domain and beyond; the domain is
a statement about where the *reference* is still meaningful.

- **`radius ≤ 0`**: `bake_dig` returns before it touches anything, but its
  caller still runs `apply_breach_law`. The record is therefore ACCEPTED and
  sweeps with an all-zero stencil: every scar word passes through unchanged,
  `surface_texels_touched` does not move, and the breach phase still runs.
- **An inverted envelope** (`env_x1 < env_x0`) is faithful rather than
  clipped: `lattice_lerp`'s `/ 32` is C++ integer division — TRUNCATION TOWARD
  ZERO, which is **not** an arithmetic shift for a negative numerator — and its
  final `static_cast<int32_t>` WRAPS. Both are reproduced.
  `terrain_bake_directed` §8 constructs the one-raw-unit disagreement and
  pins it.
- **height16 saturation** is per touched vertex and matches `bake_dig`'s
  defensive rails exactly; it is counted in `scar_saturations_o`.
- **VOID_AUTHORED never becomes ground**, and substance 3 (reserved) passes
  through untouched, which is what the reference's two `if`s do.
- The **heal arm does not consult `no_bake`**, while the breach arm does. That
  asymmetry is the reference's: a protected cell that somehow reached
  VOID_BREACHED must still be able to come back.

## Counters and traces

`surface_texels_touched_o` counts **vertices inside the stencil**, not swept
vertices and not cycles — the random lanes assert it equals the covered-vertex
count exactly. `breach_events_o` counts §3.4 transitions,
`scar_saturations_o` height16 rails, `nobake_clamps_o` clamp firings. The
trace payload is the `cs_event_o` stream itself ({cell, new substance,
patch_id, src_id}), which is what "breach/heal cell events are first-class
trace facts" means in gates.

The ledger's `counters:` line names only `surface_texels_touched`. The other
three are exposed because §3.4 names the transitions as trace events and
qformats §5 asks for saturation mirrors; the same deviation TERRAIN.PATCH
records as its chosen law 4.

No counter-catalog id is bound: minting one is a `spec/counters.md` amendment,
not an RTL decision.

## Scalar reference function

**`zref::terrain::bake_dig` and `zref::terrain::apply_breach_law`**
(`reference/src/zterrain/terrain_core.cpp`), plus
`zref::terrain::lattice_lerp` for the sweep.

**The ledger's `reference_model` was AMENDED, from `zref::TerrainBake` to
`zref::terrain::bake_dig`.** That is a deviation from "honour the ledger entry"
and it is recorded as one: **`zref::TerrainBake` does not exist anywhere in
this tree** (checked 2026-08-19) and never has. The two functions above are the
executed reference — `bake_dig` is cited by `zref_terrain.hpp`, exercised by
`tests/terrain/terrain_dual.cpp` and `terrain_keel.cpp`, and is the same
function SW.CPUCOLL's canonical mirror runs. Naming a symbol that resolves is
the difference between a differential test and a fiction; rule V17(a) would
demand a real definition the moment maturity rises above SPECIFIED anyway.

**There is no new oracle header, and that is deliberate.** The tests call
`bake_dig` and `apply_breach_law` directly on a `zref::render::TerrainPatch`.
A `zref::terrain::TerrainBake` wrapper would have been a second implementation
of §3.4 sitting between the RTL and the law — charter §29-6's exact failure
mode, and the thing TERRAIN.PATCH's contract had to argue its way around with a
cross-check.

## Directed tests

`tests/terrain/terrain_bake_directed.cpp` — **265 checks.** The reference
cross-check over a real 33×33 dual island (relief, deep keel, authored void
bite, no_bake plinth, pre-existing scars); the sweep order proved by recording
which addresses the block asked for rather than assuming them; backpressure on
all four handshakes changing nothing; **the stencil edge at `d2 == r*r`
EXACTLY** and one raw unit either side; `d2 == 0` giving `s = 65536` with the
scar word checked against `rescale(rescale(depth × 65536, 16), 8)` by hand;
radius 1 and radius ≤ 0; the idle stamp; **the §9.2 telescoping identity on the
real block** (0→1.5→3.7→6 m equals 0→6 m, bit for bit) and the un-apply
identity; both height16 rails; the no_bake clamp at `bottom + 1 − base` with
its four corner vertices and the §3.3 corner halo (nine cells protected, and
the check made non-vacuous by requiring >100 cells around them to have
breached); the breach law's four arms one at a time — three corners meeting and
the fourth one LSB above, then the fourth arriving; a corner strictly below;
the heal; VOID_AUTHORED; a no_bake cell that cannot breach but CAN heal;
reserved substance 3; the legacy page and the cell-less page; **the inverted
envelope's truncating divide, constructed so a floor and a truncate give
different answers**; the §9.2 budget at 64, one past it, and the carry into the
next window; and the throughput measurement.

## Randomized differential tests

`tests/terrain/terrain_bake_random.cpp` — **874 checks** (12× nightly). Two
lanes:

- **Lane A, island-shaped:** authored relief over a §3.7 deep keel, small
  scars, authored void, no_bake plinths, 1..5 successive bakes ramping a
  crater down — the incremental Volcano cadence. It must NEVER rail.
- **Lane B, domain-limit:** both height16 rails on every plane, depths over the
  whole fx16 word, radii from 1 raw unit to 2^27, inverted envelopes, legacy
  and cell-less pages, cell bytes over all four substances with random flags.

**Both lanes assert they reached what they exist for**, and two of those states
had to be CONSTRUCTED rather than sampled:

- the stencil centre is snapped onto a lattice vertex, which is the only way
  `d2 == 0` (hence `s = 65536`) ever happens under any sampling;
- the radius is set to an exact axis-aligned lattice distance, which is the
  only way `d2 == r*r` ever happens, and to that distance plus one raw unit;
- **whole CELLS** are snapped onto the §3.4 equality, not scattered corners.
  The first version sprinkled corners and lane A birthed **zero** breaches
  while every differential passed — the law needs all four corners, so a
  per-corner sprinkle exercises nothing. A third of the snapped cells are then
  marked `no_bake` so the clamp has something to protect, and a third of the
  bakes cover the whole page so the snapped cells are actually under the
  stencil. Before that fix lane A's clamp counter also read zero.

Lane A reaches 402 breaches, 72 heals, 891 clamps, 48 centre-exact stencils,
10 exact-edge and 11 one-unit-inside probes, 12 idle stamps, and **0 rails**.
Lane B reaches 1,014 breaches, 28 heals, 10,527 rails, 5 inverted envelopes,
4 legacy and 5 cell-less pages, 26 authored-void cells.

## Integration capture cases

**`tests/terrain/terrain_bake_chain.cpp` — BAKE → PATCH, both blocks real,
162 checks.** This is the `baked_scars` seam: TERRAIN.PATCH has carried
`scar_i` and named TERRAIN.BAKE as its upstream since phase 6, and until this
increment layer B had no producer in RTL at all — which is why TERRAIN.PATCH's
contract records "Integration capture cases: none yet".

Four islands × five successive bakes: the real bake writes layer B, the real
patch engine composes it, and every composed `live_top` and underside word must
equal `zref::render::compose_lattice` on the reference-baked patch.

**And the chain states one thing neither block can state alone.**
terrain_rules §3.4 defines a breach as "compose_top == bottom at all four
corner vertices". TERRAIN.BAKE decides that on the **height16** grid from
`base + scar ≤ bottom` and never computes `compose_top`; TERRAIN.PATCH computes
`compose_top` in **fx16** through its own §3.4 clamp and never sees layer D.
The claim that those are the same fact is a cross-block invariant, and it is
checked on all 1,024 cells of every composition (687 breached-cell observations
over the run). The dirty-mask direction is checked too: PATCH calls a vertex
dirty only where BAKE wrote a scar or the underside clamps it.

**Not chained, recorded rather than skipped quietly:** SURFACE.SHEET →
TERRAIN.BAKE (see "the sheet seam" above — it would require inventing two
laws), and TERRAIN.PATCH's live-field lane, which needs FIELD.SEQ.EARTH.

## Formal properties

`tests/formal/terrain_bake_delta.sby` + `terrain_bake_delta_fv.sv`, on
`zhao_terrain_bake_delta` — the exact module the block instantiates, elaborated
six times so the theorem about how three bakes COMPOSE can be stated.

- **P1 `a_telescopes` — terrain_rules §9.2 law 3.**
  `delta(a,b) + delta(b,c) == delta(a,c)`, exactly, for every stencil value and
  every triple of fx16 depths. §9.2 freezes BAKE_PATCH_BUDGET = 64 and declares
  the resulting deferral "state-exact by the incremental-scaling identity", so
  this identity is what makes a DEFERRED Volcano arrive at the same island as
  an undeferred one. **It is FALSE for the obvious near-miss:** §9's own prose
  reads `(to − from) × stencil`, and a datapath computing
  `rescale((a − b) × s)` accumulates one rounding per deferred step. This
  assertion is the difference between the two forms.
- **P2 `a_antisymmetry`** — `delta(a,b) == −delta(b,a)`: an interrupted cast
  un-applies cleanly.
- **P3 `a_identity`** — `delta(a,a) == 0`.
- **P4 `a_zero_stencil`** — a rim vertex (s = 0) contributes exactly nothing at
  every depth pair, including the fx16 extremes.
- **P5 `a_no_saturate`** — the first rescale's fx16 rail is UNREACHABLE for
  `s ≤ 65536`, which is the divider's structural bound. Paired with
  `c_sat_reachable`, which shows the rail DOES fire once the bound is lifted,
  so P5 is a statement about the real input space rather than a tautology.
- **P6 `a_digs_down`** — `from ≤ to ⇒ delta ≤ 0`: "positive depth digs DOWN".

Depth 2 is the **whole** state space, not a bound: the module is combinational
and its inputs are unconstrained apart from `s ≤ 65536`.

**What this does NOT prove, stated plainly:** the paraboloid divide,
`lattice_lerp`'s truncating divide, the no_bake clamp, the height16 rails, the
breach law, the two-phase sweep, the §9.2 budget and the counters. Those are
the differential lanes' and the composition's business, and the mutation table
below is the evidence for them.

## Synthesis / resource ceiling

**Not synthesized.** `fpga/files.qip` is untouched and this block has never
been through Quartus. Nothing here has run on hardware. `geometry_mantle` group
(charter §25).

Two sizing notes for whoever does. The `meets` plane is 1,089 flops with a
33-wide read-modify-write per vertex and two 33-entry word muxes in the breach
phase; if that does not fit, it is the obvious thing to move into an MLAB. The
divider is an 80-bit compare-subtract iterated 17 times — one 80-bit
subtractor, not seventeen — and it is the block's critical path.

## Notes

**LAWS CHOSEN, NOT FOUND.** Each is also argued in the RTL header.

1. **B1 — the 33×33 sweep lives in this block**, and so does `lattice_lerp`.
   **Rejected alternative:** taking `vx`/`vz` per vertex from the caller.
   `bake_dig` computes the placed lattice point itself, so a caller-side sweep
   would put a ratified rounding law (and its truncating divide) in whatever
   drives this block — and, in a differential test, in the test itself, which
   is charter §29-6's exact failure mode. The extents are hard-wired at 33×33
   because Island Patch v1 is.
2. **B2 — the `meets` plane is resident** (1,089 flops). **Rejected
   alternative:** taking four `meets` bits per cell from the caller, cheaper in
   flops and moving §3.4's breach EQUALITY — the exact height16 compare the
   whole law turns on — outside the only block terrain_rules §7 permits to own
   it.
3. **B3 — the no_bake corner shadow arrives as one bit per vertex.**
   **Rejected alternative:** holding all of layer D resident so the block could
   OR the ≤4 adjacent cells itself — 1,024 bytes of state to reproduce a fact
   the reader of D already has, in a block that deliberately has no VRAM port.
   terrain_rules §3.3 states the shadow as vertex-level law, so a vertex-level
   wire is its natural shape.
4. **B4 — the §9.2 budget is enforced by backpressure, not an internal queue.**
   **Rejected alternative:** an internal deferral FIFO of stamp records, which
   buys nothing (the upstream queue already exists and is already ordered) and
   adds a second place where order could be lost. `frame_start_i` during an
   in-flight bake resets the window without disturbing the bake: the budget
   counts ACCEPTANCES.
5. **B5 — a record with `radius ≤ 0` is accepted and sweeps, writing nothing.**
   **Rejected alternative:** rejecting the record, which would silently skip a
   breach/heal the reference performs, because `apply_breach_law` runs whether
   or not the dig did anything.

**MUTATION-CHECKED.** Four defects were injected ONE AT A TIME, each proved to
have relinked by hashing `test_terrain_bake_directed.exe` before and after, and
each confirmed to turn the suites red before being reverted (with the build
re-run after the revert, because a "baseline" run of stale mutated binaries has
bitten this tree before):

| mutation | sha256 after the injecting build | directed | random | chain |
|---|---|---|---|---|
| the stencil test made `d2 <= r2` (closed instead of open) | `0F40CDDD…` | **12/265 red** | **16/874 red** | green † |
| the fx16 → height16 bake-back's round-half-up removed (`+128` dropped, so §4 truncates) | `D6351696…` | **7/265 red** | **61/874 red** | **40/162 red** |
| `lattice_lerp`'s truncating divide made an arithmetic shift (floor) | `A3E47E33…` | **3/265 red** | **3/884 red** ‡ | green † |
| the breach law's four-corner AND made a three-corner AND | `6F39CC17…` | **13/265 red** | **188/884 red** | **26/162 red** |

The reverted tree hashes `268D7B71…` and is 265 / 884 / 162 green.

† **Reports about the CHAIN, not about the RTL.** The composition runs on a
64 m page whose envelope is never inverted and whose stencils are large, so
neither a one-raw-unit stencil boundary nor a negative-span lerp is reachable
through it. Both are caught by the lanes that CONSTRUCT them. A mutation only
one lane catches is a report about the other lane.

‡ **This one was a report about the random lane, and the lane was fixed.** The
floored lerp first cost the random lanes exactly **1 of 874 checks**: a
one-raw-unit vertex shift changes no scar word under a large stencil, and
uniform sampling never puts the shift where it matters. Lane B now constructs
the discriminating case — an inverted envelope, a stencil centre snapped onto a
lattice vertex, and radius 1, so the one unit IS the whole answer — and the
mutation costs 3 checks. Still thin, and stated as thin: the DIRECTED case
(§8, built so a floor and a truncate give provably different answers) is what
actually holds this law.

**The signedness defect the tests actually caught, recorded because a review
would have passed over it.** The `meets` compare was built with bare
concatenations, which are UNSIGNED in Verilog, and a comparison goes unsigned
if either side is. A composed height of −200 read as 261,944, so **every cell
on the island breached** — 18 of the directed suite's first-run checks went red
on it. Same trap as GEOM.BINNER's 29 vanished tiles, in a line that reads
correctly.

**The build trap that fired here too, recorded for the next increment.**
Windows PowerShell 5.1's `Get-Content -Raw` / `Set-Content -Encoding utf8`
round trip **corrupts UTF-8 and adds a BOM**: after the first revert both RTL
files were full of mojibake (`§` → `Â§`) and carried an EF BB BF prefix. The
logic was untouched so every test still passed — which is exactly why it is
worth writing down. Mutations here are applied with a UTF-8-safe editor, never
through PowerShell text cmdlets.
