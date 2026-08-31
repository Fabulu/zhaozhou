# Contract — GEOM.LOOM (Transform Loom)

> Ledger: `design/blocks.yml` · owner ZH-041 · phase 9 · maturity SPECIFIED

## Purpose and exclusions

Bounded transform graph evaluation (orbit/aim/billboard/oscillator/spline/gait/formations) producing instance transforms; consumes formation-field samples.

World-identity wave additions (spec/creature_rules.md §4.1 — SPECIFIED,
built Phase 9):

- **SCALE node** — uniform per-node fx16 scale on the subtree (bulk
  inflation, growth-as-scale, per-instance size variety).
- **REPARENT** — parent-id retarget with epoch/generation check and a
  keep-world-transform flag; a pointer swap, not a datapath change. This is
  what makes grab/eat/carry/ride/horn-larder near-free (S3's biggest
  character-per-gate win). The `ReparentTransform` ABI command shape is
  proposed, landing via abi-gen with the ABI owner.
- **GAIT envelope** — the existing gait node gains
  anticipation/impact/settle envelope parameters for heavy walkers.
  Presentation only: it shapes offsets and emits NO events — gameplay
  impacts are sim-authored from clip event tags (creature_rules §4.2;
  charter §29-8: present never mutates truth).
- **Body-patch binding** — a Loom node may drive an island `body_patch`
  page (terrain-class giant seam, creature_rules §6.5); such a node is
  constrained to rigid + uniform scale (no shear), validator-enforced.
  PROTOTYPE-BEFORE-SILICON: no RTL work may cite this binding before a
  software prototype capture exists.

Exclusions: no skinning (GEOM.SKIN), no pose decode (GEOM.POSE), no
gameplay event emission (sim-side, ever).

## Clock and reset semantics
### The ruling that shrank this block (owner, 2026-08-31 §6.4)

> The ARM/compiler supplies a **parent-before-child topologically sorted
> stream. Loom only composes transforms.** It does not perform: recursion;
> cycle detection; matrix inversion; gameplay event generation; autonomous gait
> logic; autonomous formation logic. Gait and formation values come from
> Form/Field programs. **Keep-world reparenting is computed on the ARM between
> frames.**

That is not a clarification, it is a large deletion, and it is what makes this
block buildable. A "bounded transform graph evaluator" that had to detect
cycles, invert matrices and run gait logic is a processor. **What survives is a
streaming matrix composer**, and every genuinely hard part named above is now
somebody else's — mostly the compiler's, where it is cheap and testable.

The purpose section above still describes keep-world reparenting via a
`ReparentTransform` command; **the ruling supersedes the hardware half of it.**
The ABI command may still exist, but the world-space recomputation happens on
the ARM between frames, not here.

### Clocking

Single `gpu_clk`, synchronous active-low `rst_n`. No CDC.

Reset abandons the stream in flight and clears the parent-transform store. **A
stream must be restarted from its root**, never resumed mid-stream: a child
composed against a stale parent is silently wrong geometry rather than an error,
which is the worst failure mode this block can have.

## Input and output packet layouts
### In — one node per beat, parents first

`{ node_index, parent_index, node_kind, params, src_id, last }`

**`parent_index` < `node_index` is a PRECONDITION, not a check performed here.**
The ruling puts topological sorting in the compiler. This block asserts it
(simulation assertion and a formal property below) and refuses if violated, but
it does not sort, and it does not detect cycles — a topologically sorted stream
cannot contain one.

`node_kind` is the closed v1 set: `ROOT`, `RIGID`, `SCALE`, `ORBIT`, `AIM`,
`BILLBOARD`, `OSCILLATOR`, `SPLINE`, `GAIT_OFFSET`, `FORMATION_OFFSET`.

**`GAIT_OFFSET` and `FORMATION_OFFSET` carry VALUES, they do not compute them.**
The ruling is explicit: gait and formation come from Form/Field programs. This
block applies an offset it is handed.

### Out

`{ node_index, transform[12], src_id }` — a 3×4 affine, row-major, fx16 S15.16,
the same shape `GEOM.SKIN` already takes as `a_m_i`/`b_m_i`.

No inverse, no transpose, no decomposition is emitted. If a consumer needs an
inverse it is the compiler's to supply, per the ruling's exclusion of matrix
inversion.

## Backpressure rules
Ready/valid both sides, house hygiene: `in_ready_o` independent of
`in_valid_i`; `out_valid_o` independent of `out_ready_i`.

**Stalling is safe at any point in the stream** because the parent store is
indexed, not sequential — a stalled child does not lose its parent. That is what
makes a skid buffer on the output legal here if the fit ever wants one, without
the emptiness-test hazard that caught `RASTER.TILE_PIPE` today: this block's
"done" is the `last` flag on the stream, not an inferred emptiness.

## Memory ownership
**Owns exactly one structure: the parent-transform store**, indexed by
`node_index`, holding composed world transforms for nodes already emitted.

Bounded at **1,024 nodes per stream**, 48 bytes each — 48 KiB. That does not fit
comfortably in M10K beside the renderer's own needs, and the standing ruling
about pose palettes applies for the same reason: **it lives in the local SDRAM
hot region, with the working window on-chip.** A stream that would exceed 1,024
nodes is refused, not silently truncated.

Reads no other memory. Writes none. The Field/Form samples it consumes arrive as
ports, like the cull planes in `GEOM.MESHFETCH`, not as descriptor fetches.

## Q formats and rounding
All transforms are fx16 S15.16, matching `GEOM.SKIN`'s ports and the rest of the
geometry path.

Composition is a 3×4 × 3×4 affine multiply: 9 multiplies and 3 adds for the
rotation part, 3 multiplies and 3 adds for the translation. Products are formed
at full width and **rounded once, at the end of each output element**, round-half
away from zero — the same law `rast.cpp` uses for attributes.

**One rounding per element, never per partial product.** Rounding a partial
product would make composition order-dependent in a way the topological order
does not otherwise imply, and a transform chain that depends on how it was
bracketed is not reproducible.

Depth of chain is bounded by the node limit, so error accumulation is bounded
too, but it is **accumulation and it should be measured** — a 1,024-deep chain is
legal and nobody has yet asked what the drift looks like. That measurement is a
directed test below, not an assumption here.

## Latency (fixed or variable)
**Fixed, and small.** One affine compose per node: the products can be
sequenced like `zhao_geom_lod`'s ladder or laid out flat, and that is a Class-B
area/throughput trade to be measured, not decided in the contract.

The stream as a whole is variable only in its length. There is no data-dependent
branching — every node kind resolves to the same compose after its parameters
are turned into a local matrix, which is what makes the fixed latency possible
and is another consequence of the ruling removing autonomous logic.

## Target throughput
**One transform per clock** (ledger `target_throughput`).

Against the ruling's tier: 256 creatures at, say, 28 bones plus a handful of
attachment nodes is ~9,000 nodes, ~9,000 clocks — **0.7 % of a 1,333,333-clock
frame.** This block is not a bottleneck at the guaranteed content tier, and that
is worth stating so nobody optimises it before measuring it.

The interesting pressure is not creatures. It is a formation of many instanced
objects sharing one graph, which is exactly the case `FIELD.SEQ.FORMATION`
feeds — and its cost is per instance, not per creature. That trace does not
exist yet.

## Overflow and malformed-input behaviour
Refuse, never guess. Each names `node_index` and `src_id`:

| condition | why |
|---|---|
| `parent_index >= node_index` | the stream is not topologically sorted — the ruling's precondition is violated and every downstream transform would be composed against a stale or absent parent |
| `parent_index` never emitted | same, and it is the case a cycle would produce if one reached here |
| node count > 1,024 | exceeds the bounded store |
| unknown `node_kind` | a newer graph read by an older block |
| `SCALE` non-uniform on a body-patch-bound node | the purpose section's validator rule, enforced here rather than trusted |
| stream ends without `last` | a truncated stream would leave the parent store half-populated for the next one |

**A refusal drops the whole stream**, not one node. A partially composed graph is
meaningless: every later node depends on earlier ones, so emitting a prefix
would deliver geometry that is confidently wrong.

## Counters and traces
* `vertices_transformed` (ledger name; here it counts NODES — the name predates
  the block's scope and the discrepancy is recorded rather than silently
  reinterpreted)
* `streams_composed`, `streams_refused_by_reason[6]`
* `nodes_per_stream_max` — watches the 1,024 bound approaching
* `chain_depth_max` — the input to the accumulation question above
* `node_kind_histogram[10]`
* `consumer_stall_cycles`

The ledger counter name should be corrected to `nodes_transformed` when this
block is built; changing it now would break the ledger's own staleness check
before there is RTL to justify it.

## Scalar reference function
`zref::TransformLoom` (ledger `reference_model`), entry point
`compose(stream) -> std::vector<Transform>`.

It owns the compose law, the rounding, the refusal taxonomy and the bounded
store semantics. It explicitly does **not** own sorting, cycle detection,
inversion, gait or formation evaluation — per the ruling those belong to the
compiler and to Form/Field, and an oracle that implemented them would be
asserting a scope this block does not have.

## Directed tests
`tests/geometry/geom_loom_directed.cpp`.

* every node kind in isolation against a hand-computed matrix;
* a two-level chain, then a ten-level chain, against composed reference;
* **`parent_index == node_index` and `parent_index > node_index`: both refused**
  — the precondition, tested at its boundary;
* a stream whose parent was never emitted: refused;
* 1,024 nodes accepted, 1,025 refused;
* stream without `last`: refused;
* non-uniform `SCALE` on a body-patch node: refused;
* **the 1,024-deep chain drift measurement.** Compose a maximal chain and report
  the worst element error against exact rational arithmetic. This test's job is
  to produce a NUMBER for a question nobody has asked yet, not to assert a bound
  invented here.

## Randomized differential tests
`tests/geometry/geom_loom_random.cpp`, RTL against `zref::TransformLoom`.

Random valid graphs — random depth, branching and node-kind mix — plus a
deliberate malformed fraction, with the generator **reporting its refusal mix**
so coverage loss shows as a shift rather than as silence. Same requirement as
`GEOM.MESHFETCH` and `GEOM.VDECODE`.

Bias generation toward deep narrow chains and wide shallow fans separately: they
stress the parent store's residency in opposite ways, and a uniform random graph
produces neither.

Mutation sweep to the geometry blocks' existing standard.

## Formal properties
`tests/formal/geom_loom_order.sby`:

* **the topological precondition is enforced, not assumed** — for any stream
  containing `parent_index >= node_index`, the block refuses and emits nothing.
  This is the safety property, because the failure it prevents is silent: a
  child composed against a stale parent is wrong geometry, not an error;
* **no partial stream** — a stream emits all its nodes or none;
* **the parent store is never read before written** for any accepted stream;
* handshake hygiene on both channels;
* reset drops in flight, and no pre-reset transform is emitted after.

## Synthesis / resource ceiling
Unbuilt. **Ceiling: 1,500 ALMs, 12 DSPs, and NO M10K for the parent store.**

The DSP figure is where the Class-B trade lives: twelve multiplies per compose
laid out flat, or fewer sequenced as `zhao_geom_lod` does — which took its five
products through one multiplier and cost 12 of 18 DSPs to do it. Measure both
against a real fit before choosing.

No M10K is a deliberate constraint: 1,024 nodes × 48 B is 48 KiB, and the
renderer needs its block RAM. The store belongs in the SDRAM hot region with an
on-chip window, for the same reason pose palettes do.

## Integration capture cases
* **256 creatures, full graphs** — the ruling's tier. Confirms ~9,000 nodes is
  the real number and that the block is nowhere near a bottleneck at guaranteed
  content.
* **a formation of many instanced objects sharing one graph** — the case that
  actually pressures this block, fed by `FIELD.SEQ.FORMATION`. This trace does
  not exist and should be authored before the block is fitted, or the fit
  measures the easy case.
* **a malformed stream mid-frame** — one stream drops, the frame completes, the
  counter attributes it to the introducing command.
* **maximal chain depth** — the drift number from the directed test, observed in
  a real frame rather than in isolation.

## Notes

Bounded graph: cycle/depth limits are spec constants; overflow degrades, never hangs.
