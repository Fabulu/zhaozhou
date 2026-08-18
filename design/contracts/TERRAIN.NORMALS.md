# Contract — TERRAIN.NORMALS (Deformed normals)

> Ledger: `design/blocks.yml` · owner ZH-038 · phase 6 · maturity SPECIFIED

## Purpose and exclusions

Recompute normals over the deformed height field for terrain shading.

Implemented as `fpga/rtl/terrain/zhao_terrain_normals.sv`. This block computes
the FACE normal of one triangle of the deformed surface: the cross product of two
edges, rescaled back to fx16. It does not normalise, does not average into vertex
normals, does not light anything, and does not read memory.

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger.
Reset clears both pipeline stages, the output registers and
`terrain_samples_evaluated_o`. No clock-domain crossing lives in this block.

## Input and output packet layouts

`terrain_mesh` in, ready/valid:

| field | width | meaning |
|---|---|---|
| `ax_i` `ay_i` `az_i` | signed 32 | vertex A, fx16 world units |
| `bx_i` `by_i` `bz_i` | signed 32 | vertex B |
| `cx_i` `cy_i` `cz_i` | signed 32 | vertex C |
| `src_id_i` | 16 | source id, rides the packet |

`terrain_normals` out, ready/valid:

| field | width | meaning |
|---|---|---|
| `nx_o` `ny_o` `nz_o` | signed 32 | the UNNORMALISED face normal, Q16.16 |
| `degenerate_o` | 1 | all three lanes rescaled to exactly zero |
| `src_id_o` | 16 | the id of the triangle this normal came from |

Plus `terrain_samples_evaluated_o` (32) and `idle_o`.

## Backpressure rules

Ready/valid on both ports, one packet per stage. A full output stalls stage 2,
which stalls stage 1, which deasserts `tri_ready_o`. No packet is dropped and no
result is corrupted while it waits; both are pinned by
`terrain_normals_directed`'s backpressure case, and the random lane stalls the
consumer on a varying schedule throughout rather than in one dedicated case.

## Memory ownership

None. This block has no port onto the tile store, no cache, and no M10K. Its
entire state is two pipeline stages and a counter.

## Q formats and rounding

**This is the section that matters, because the arithmetic here has already
been wrong once in this project's history.**

Edges are fx16 (Q16.16) differences. A product of two Q16.16 raws is Q32.32, so
the cross-product lanes are Q32.32 and the shift back to Q16.16 is
`rescale(., 16)` — a round-half-up shift then a saturating narrow to the fx16
word (`spec/qformats.md` §3/§4).

`reference/src/zrender/terrain.cpp` records what happens at 32 instead: the
normal quantises to WHOLE world-units squared, every component of a near-flat
cell rounds to zero, the degenerate guard fires for every triangle, and the
patch shades solid black. It was found because a 41×41 lattice over ±12 m
(0.6 m spacing) rendered as a black silhouette while 25×25 over the same
envelope (1.0 m spacing) looked correct. Phase-6 Mantle patches are 32×32 cells
per world patch, sub-metre by design, so this block lives entirely inside the
regime where that defect is fatal.

**Widths, stated rather than assumed.** A vertex component is signed 32. An edge
is a difference of two, so signed 33. A cross term is a product of two signed 33
values, so signed 66; a lane is a difference of two terms, so signed 67. The
rounding add of 2^15 cannot overflow that, and the arithmetic shift right by 16
leaves signed 51, which the saturating narrow takes to signed 32.

**Verilog signedness.** Every comparison in this block is between two signed
operands. A Verilog comparison goes unsigned if EITHER operand is; that trap
cost a real bug in `GEOM.BINNER` and made 29 tiles vanish (see
`design/contracts/GEOM.CLIP.md`).

## Latency (fixed or variable)

Fixed 2 cycles: stage 1 forms the edges and the six products, stage 2 does the
rescale. The ledger says `variable`, which a fixed latency satisfies.

## Target throughput

The ledger asks for "1 normal per vertex per clock". This block sustains **one
normal per clock** when the consumer is ready, which meets the rate. It does not
meet the ledger's wording literally, because it emits one normal per TRIANGLE,
not per vertex — see the first item under Notes.

## Overflow and malformed-input behaviour

**Input domain: |coordinate| ≤ 4096 world units (2^28 raw).** That is double
`spec/qformats.md` §8's ±2048 guard band. Inside it a lane needs at most 60
bits, so the reference's `int64` carries it exactly and this block agrees
bit-for-bit.

**Outside that domain the reference overflows and this block does not.** At the
full int32 word a lane needs 66 bits; `shade_flat_tri` uses `int64` and wraps,
while this block's 67-bit lanes saturate. That is a divergence in the block's
favour on inputs no legal lattice can produce. It is recorded here rather than
removed by narrowing the RTL to wrap the same way, because a hardware block that
silently wraps a geometric quantity is worse than one that disagrees with a
model outside the model's own valid range. The randomized lane B samples the
domain limit, not the word limit, for exactly this reason.

Output saturation is normal and expected inside the domain: a 2^59 lane rescales
to 2^43, far past `INT32_MAX`, so the fx16 rails are reached by legal input and
are tested.

**Degeneracy is reported, not substituted.** When all three rescaled lanes are
exactly zero the cell has no area; `degenerate_o` rises and the zero vector is
emitted. No up-vector is invented, so a consumer cannot mistake a collapsed cell
for a flat one. Degeneracy is judged on the RESCALED lanes because that is what
`shade_flat_tri` does (it forms `nmag2` from the post-rescale values), so a cell
whose exact cross product is nonzero but rounds to zero is degenerate there too.

## Counters and traces

`terrain_samples_evaluated_o` counts triangles that produced a normal, not
cycles and not offered packets. Pinned by the directed suite's counter case.
No counter-catalog id is bound: `terrain_samples_evaluated` has several
claimants and minting an id is a `spec/counters.md` amendment, not an RTL
decision.

## Scalar reference function

`zref::terrain::face_normal` in `reference/include/zref/zref_terrain_normals.hpp`.

It is a **thin view onto an existing ratified law, not a second implementation**.
`shade_flat_tri` already computes this normal; it simply does not expose it,
because it consumes it immediately in a dot product. The oracle exposes that
arithmetic and nothing else, so "RTL matches oracle" means "RTL matches the
shading the golden captures already pin".

## Directed tests

`tests/terrain/terrain_normals_directed.cpp` — 67 checks. Flat ground in both
windings (and that reversing the winding negates the normal rather than merely
changing it), a tilted cell, collinear and collapsed cells, the sub-metre cell
that the rescale-32 defect destroyed, both halves of the round-half-up boundary
(a lane of exactly +2^15 rounds to 1; exactly −2^15 rounds to 0, not −1), the
domain limit in three sign combinations, backpressure, and the counter.

## Randomized differential tests

`tests/terrain/terrain_normals_random.cpp` — 20,003 checks, 2,500 per lane
(40,000 nightly). Two lanes on purpose:

- **Lane A, lattice-shaped:** cells on a 1/32-unit grid with heights within ±1
  unit, which is what a Mantle patch actually emits. This is the regime the
  historical defect destroyed.
- **Lane B, domain-limit:** uniform over ±4096 world units, reaching the fx16
  output rails.

Both lanes assert they actually reached their interesting states: lane A must
sample degenerate cells and must never rail, lane B must rail. Without those a
green run could mean the lane sampled nothing worth sampling, which is how a
flooring defect elsewhere in this tree survived 20,000 random triangles.

## Formal properties

**None, deliberately.** The block's content is one cross product and one
rescale. The rescale is already covered exhaustively at the boundary by the
directed cases, and a bounded proof of "the lanes equal the cross product" would
restate the three `assign`-equivalent expressions it claims to prove. There is no
invariant here over state the solver cannot already see, because the block has
almost no state. A proof with nothing to cover is worse than none.

## Synthesis / resource ceiling

Not synthesized. `fpga/files.qip` is untouched and this block has never been
through Quartus. Nothing here has run on hardware.

## Integration capture cases

None yet. This block has not been composed with `TERRAIN.TESS` (which does not
exist) or with `TERRAIN.PROJECT` (which does not exist), so nothing feeds it real
mesh and nothing consumes its normals. The directed and random lanes drive it
directly. That composition is the next increment, and wiring it is where a
port-level mistake would surface — as it did for `GEOM.BINNER`, where composing
with the real rasterizer immediately exposed a tile-index-versus-pixel error no
isolated test could see.

## Notes

**LAWS CHOSEN, NOT FOUND.** Each is also argued in the RTL header.

1. **This block emits FACE normals, not vertex normals.** The ledger's
   throughput line says "per vertex", but the only ratified normal in the tree is
   `shade_flat_tri`'s per-triangle cross product, and the reference shades flat.
   Averaging adjacent face normals into a vertex normal is a real technique and
   is **not ratified anywhere**: it needs a rule for how many neighbours a
   lattice-edge vertex has, what a void column contributes, and whether the
   average is renormalised — which §7.4's explicit no-renormalisation ruling
   bears on. Inventing that here and calling it the law is the error this project
   has already been bitten by more than once. The vertex-normal question is left
   open for whoever ratifies it.
2. **The normal is not normalised.** `shade_flat_tri` divides by |n| only at the
   moment it takes a dot product, so the ratified quantity is the unnormalised
   Q16.16 cross product. Normalising here would insert a second rounding the
   reference does not have (§3, one rounding per result), and §7.4's
   `normalize3_approx` is available to any consumer that wants a unit vector.
3. **The input domain is stated and bounded** (see Overflow above) rather than
   assumed to be the whole word.

**MUTATION-CHECKED.** Four defects were injected one at a time, each proved to
have relinked by hashing the test binary, and each confirmed to turn both lanes
red before being reverted:

| mutation | directed | random |
|---|---|---|
| rescale by 32 instead of 16 (the historical defect) | 11/67 red | red |
| cross-product operands swapped | 5/67 red | 4839/20003 red |
| rounding truncated instead of round-half-up | 4/67 red | 1775/20003 red |
| an edge taken from the wrong vertex | 2/67 red | 3121/20003 red |

The first sweep attempt reported all four mutations PASSING with an identical
binary hash: deleting the verilate output directory to force a rebuild had
removed the generated copy-scripts, so the build failed and the stale executable
ran. That is the same trap that cost an earlier increment three sweeps. The fix
is to reconfigure and let normal dependency tracking re-verilate, and to check
the hash every time. A mutation sweep that does not prove its own relink proves
nothing at all.
