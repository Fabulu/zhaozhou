# Contract — TERRAIN.MIPGEN (Page height mips)

> Ledger: `design/blocks.yml` · gpu clock · maturity UNIT_VERIFIED
> RTL: `fpga/rtl/terrain/zhao_terrain_mipgen.sv`

## Purpose and exclusions

Generates a resident page's height mips **on the FPGA**, in one block, by
**nested decimation**.

**Written 2026-09-03 from ruling T8, which specified this block completely and
found it had no RTL, no contract and no ledger entry** — the same gap
`GEOM.PARAMBUF` had, in the terrain lane.

**Exclusions.** It does not verify CRCs and does not decide residency. T8's
ordering — *"a page becomes RESIDENT only after: payload CRC passes; bytes
complete; resident mips complete"* — belongs to the residency directory, and
this block's `done_o` is one of that decision's three inputs.

**`HPS does not implement a second mip law`** (T8). The composed lattice's mips
for `COMPOSED_MIP_POOL` are this same block on a different source, not a second
implementation of the same rule.

## Input and output packet layouts

**In:** a 33×33 height lattice per surface, `height16` (S 1.7.8), **in scan
order** — row-major within a surface, surfaces in order (top, then bottom).

The port carries **no coordinate**. The order *is* the address. A coordinate
that can disagree with the scan is a second source of truth, and the one that
is wrong is the one nobody checks.

**Out:** two write streams, `{addr, surface, height16}`:

| level | cells per surface | address range |
|---|---|---|
| `mip17` | 17 × 17 = 289 | 0..288 |
| `mip9` | 9 × 9 = 81 | 0..80 |

`(289 + 81) × 2 B × 2 surfaces = 1,480 B` in a 1,536-byte record.

## The law

    mip17[i,j] = fine33[2*i, 2*j]     i,j in 0..16
    mip9 [i,j] = fine33[4*i, 4*j]     i,j in 0..8

**No averaging.** A coarse vertex **is** a fine vertex, bit for bit.

**Why it must be decimation, in one sentence:** two neighbouring patches drawn
at different LOD share an edge, and if the coarse patch's edge vertices were
averages the two edges would disagree by a fraction of a metre and the ground
would crack open along a line the player can walk to.

Nested decimation also means **every `mip9` vertex is a `mip17` vertex**, so
the two levels agree with each other and not merely with the source. In the RTL
that is a bit test — `col[1:0] == 0 && row[1:0] == 0` implies
`col[0] == 0 && row[0] == 0` — rather than a promise.

**A coarser 5×5 is selected from `mip9` on demand.** Do not store a third page
mip without evidence (T8).

## Backpressure rules

`fine_ready_o` is high whenever the block is running. It never withholds a
sample because **it has nowhere to put one**: both mips are strided subsets of
the same scan, so each sample is routed as it arrives and nothing is buffered.

The obvious implementation buffers the 1,089-sample lattice and walks it twice.
That is 2,178 samples of on-chip memory to produce a result that depends on no
sample but the one in hand.

## Memory ownership

**None.** No line buffer, no lattice buffer, no lookup table. The consumer owns
`TERRAIN.RESIDENT_MIP_POOL` (1,024 × 1,536 B) and `TERRAIN.COMPOSED_MIP_POOL`
(256 × 1,536 B); see `spec/memory_rules.md` §5b.

## Q formats and rounding

`height16` = **S 1.7.8** (`spec/qformats.md` §9), passed through unchanged.

**There is no rounding mode, because there is no rounding.** That is the
contract, not an implementation note: a future version that introduced one
would be a different block and would break every seam in the world.

## Latency (fixed or variable)

`fixed:1` — a selected sample appears on its output port one clock after it is
accepted. `done_o` pulses one clock after the last sample of the last surface.

## Target throughput

**One fine sample per clock.** A full page is `33 × 33 × 2 = 2,178` samples, so
a page's mips take 2,178 clocks plus whatever gaps the loader leaves.

Against T7's ceiling of 32 whole pages per frame that is 69,696 clocks of a
1,333,333-clock frame — about 5 %, and only in the worst streaming frame.

## Overflow and malformed-input behaviour

* **A restart mid-scan abandons the partial result and counts it** in
  `aborts_o`. An aborted load is legal (T11); a half-finished mip mistaken for
  a complete one is not.
* There is no invalid height: every 16-bit pattern is a legal `height16`.
* The block cannot overrun its outputs — the addresses are a function of the
  scan counters, and the scan counters are bounded by `FINE`.

## Directed tests

`tests/terrain/terrain_mipgen_directed.cpp` — 12 checks, **no tolerance
anywhere**, because the law admits none:

* every fine sample consumed; exactly 289 and 81 writes per surface;
* **no write outside its own level** — where a wrong row stride shows first;
* no cell written twice;
* both levels bit-identical to **`zref::terrain::mipgen`**, against a lattice
  where **every sample is distinct** so a wrong address cannot accidentally
  produce a right value;
* **the nesting**: every `mip9` vertex is the `mip17` vertex at the same place;
* top and bottom stay separate surfaces;
* a mid-scan restart is counted.

Fed **with gaps**, so a block that only works when fed every clock fails.

**Proved to bite:** breaking the `mip17` row stride by one fails five of the
twelve checks, out-of-range first.

## Scalar reference function

`zref::terrain::mipgen` (`reference/include/zref/zref_terrain.hpp`), with
`zref::terrain::mip17_at` and `zref::terrain::mip9_at` for a single vertex.

The directed suite compares the RTL against that symbol rather than against the
law re-typed in the test. Re-typing it would compare the RTL with the test
author's memory of the ruling, and both could be wrong the same way.

## Randomized differential tests

Planned: a randomised lattice driven against `zref::terrain::mipgen` over many
pages. The directed suite already compares against that oracle on a
distinct-per-cell lattice, which is the property that matters more than volume
here — the function has no state and no arithmetic, so its failure modes are
addressing, not value.

## Integration capture cases

Planned, with the 8 km traversal: a patch at LOD *n* beside a patch at LOD
*n+1*, checked for an exact shared edge. **Nothing here is on hardware.**

## Synthesis / resource ceiling

Unbuilt as a fit. **Expected: no DSP and no M10K** — the block performs no
arithmetic on heights and owns no memory. If a fit reports either, the
implementation has stopped matching this contract.

## Notes

leaf.
