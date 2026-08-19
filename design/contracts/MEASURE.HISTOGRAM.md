# Contract — MEASURE.HISTOGRAM (Error histogram)

> Ledger: `design/blocks.yml` · owner ZH-049 · phase 8 · maturity SPECIFIED

## STATUS — NOT STARTED THIS INCREMENT, AND WHY

Phase 8's three MEASURE blocks were opened together. `MEASURE.TOKENS` and
`MEASURE.GOVERNOR` landed. **This block was deliberately not started**, and the
reasons are recorded here rather than left as an unexplained gap — the same way
`FIELD.SEQ.EARTH` and `FIELD.SEQ.FORM` were refused in phase 7.

**1. The charter sequences it AFTER the two blocks that did land, explicitly.**
Charter §9's "Practical implementation path" is in two numbered stages:

> **Version 1:** ARM predicts a pixel-error threshold per camera from prior
> counters; FPGA performs local hierarchy traversal against that threshold; a
> global token guard rejects only low-priority refinement when the budget is
> nearly exhausted.
>
> **Version 2:** FPGA builds a small histogram of candidate error buckets; a
> cutoff bucket is selected; eligible refinements above the cutoff are emitted.

Every word of Version 1 is now built: the ARM's threshold arrives on
`SetView.pixel_error`, `TERRAIN.LOD` is the local hierarchy traversal,
`MEASURE.GOVERNOR` converts the threshold, and `MEASURE.TOKENS` is the global
token guard. This block is the whole of Version 2. The ledger's own note says
the same thing: *"Charter §12 calls this Version 2: registered now, built
late."*

**2. Its only declared input already exists in RTL, and it is a different
thing.** The ledger gives this block `inputs: [fragment_error]`, `upstream:
[RASTER.FRAGMENT]`. RASTER.FRAGMENT shipped in phase 4 and it *does* have a
port called `fragment_error_o` — but it is:

```systemverilog
assign fragment_error_o = s1_v_r && !rd_valid_i;
```

a **one-bit tilestore-read protocol flag**, whose own comment says *"It should
never fire"*. It is not a screen-space error magnitude, it carries no value, and
it has no bucket index. So the ledger edge resolves to a signal that exists and
means something else entirely. Building this block against it would either
require inventing a second, real `fragment_error` port on a landed, tested block
— or quietly using the flag and producing a histogram of protocol faults labelled
as a quality metric. **Neither is acceptable, and picking one silently would be
worse than not building the block.**

**3. Building it now would require four stacked inventions, none of which has a
law anywhere in this tree.** In the order they would have to be made:

- **the error metric** — what number goes in a bucket. Charter says "candidate
  error buckets" and stops. A projected screen error? A per-fragment residual? A
  per-meshlet deviation? No spec names one, no Q format is declared for it, and
  no block produces one.
- **the bucket boundaries** — how many, linear or logarithmic, over what range.
  Nothing anywhere states this.
- **the cutoff rule** — "a cutoff bucket is selected" is the entire
  specification. Selected how? Against what budget? With what hysteresis?
- **a Version-2 governor input** — the cutoff has to reach
  `MEASURE.GOVERNOR`, which was just built to charter Version 1 and by
  definition consumes no cutoff. Adding one would mean either a second policy
  path inside a block whose contract has just been written, or contradicting it
  in the same increment.

That last one is the decisive argument. **The failure mode this project cares
most about is two blocks disagreeing about one policy** — it is exactly what
happened when `TERRAIN.LOD` had to guess this file's sibling and why the
governor's contract now carries a whole section reconciling it. Inventing a
Version-2 cutoff in the same increment that ratified a Version-1 governor would
manufacture that failure deliberately.

**4. What is NOT the reason.** It is not effort, and it is not that the block is
hard. A bucket histogram with a cutoff scan is a small, cheap block — smaller
than either block that landed. It is that a small block built on four invented
laws is worse than no block, because the inventions become ratified by being
implemented, and the next wave inherits them as though they were found.

## What would unblock it

In order, and none of them is this block's work:

1. A ratified **error metric** with a Q format and a producing block — most
   naturally an amendment to `spec/qformats.md` plus a real value port on
   RASTER.FRAGMENT (distinct from the existing protocol flag, which should
   probably be renamed at the same time, since its current name is now known to
   collide with a ledger edge).
2. **Bucket geometry** ratified in a spec: count, boundaries, and whether the
   scale is linear or logarithmic.
3. A **cutoff rule** in the charter or a spec, with the budget it is selected
   against.
4. An amendment to `design/contracts/MEASURE.GOVERNOR.md` defining how a cutoff
   composes with the Version-1 per-camera ratio — *before* either block is
   changed, so the two cannot drift.

## Purpose and exclusions

Error-bucket histogram and cutoff feedback ("Version 2" quality loop) feeding
the governor.

## Clock and reset semantics

Unspecified — see STATUS. `gpu` domain per the ledger.

## Input and output packet layouts

Unspecified — see STATUS, point 2: the declared `fragment_error` input resolves
to a one-bit protocol flag on RASTER.FRAGMENT, not to an error magnitude.

## Backpressure rules

`ready_valid` per the ledger. Unspecified beyond that — see STATUS.

## Memory ownership

Unspecified — see STATUS. A bucket array is the obvious shape and its width
depends on the bucket geometry, which is invention 2.

## Q formats and rounding

**UNDECIDED, and deliberately left so.** The error metric has no ratified Q
format anywhere in this tree; declaring one here would ratify it by omission,
which is what `SURFACE.SHEET` refused to do for its own undecided field and
what this file refuses to do for the metric.

## Latency (fixed or variable)

`variable` per the ledger. Unspecified — see STATUS.

## Target throughput

`1 bucket update per fragment batch` per the ledger. Unspecified — see STATUS.

## Overflow and malformed-input behaviour

Unspecified — see STATUS. Bucket counters would follow `spec/counters.md` §4
(saturate, never wrap) as every other counter in the machine does.

## Counters and traces

The ledger gives this block `lod_representation_counts`. Note for whoever builds
it: that catalog entry now has two other owners with different readings —
`TERRAIN.LOD` counts subpatches per terrain level (four lanes) and
`MEASURE.TOKENS` counts grants per charter §9 ladder rung (eight lanes). A third
owner needs a reading of its own that is honestly a *representation* count, or
it should record a ledger deviation instead of driving it. `spec/counters.md` §3
permits multiple owners of one catalog entry (`deadline_faults` already has
two), so this is a naming question, not a rule violation.

## Scalar reference function

**`zref::MeasureHistogram` DOES NOT RESOLVE** — it names nothing in this tree
and never has. It is the tenth phantom, after `zref::CmdDma`,
`zref::SurfaceStamp`, `zref::SurfaceSheet`, `zref::AuxSource`,
`zref::TerrainBake`, `zref::TerrainVelocity`, `zref::ProgCache`,
`zref::MeasureTokens` and `zref::MeasureGovernor`. It is NOT amended in
`design/blocks.yml`, because unlike the other nine there is nothing to amend it
to: no oracle was written, since writing one would mean making the four
inventions above. Recorded here so the count is complete and so the next wave
does not rediscover it.

## Directed tests

None. `tests/measure/measure_histogram_directed.cpp` does not exist and is not
referenced by `tests/CMakeLists.txt`.

## Randomized differential tests

None. `tests/measure/measure_histogram_random.cpp` does not exist.

## Formal properties

None. The ledger names no formal lane for this block.

## Synthesis / resource ceiling

No RTL exists. `fpga/rtl/measure/` contains `zhao_measure_tokens.sv` and
`zhao_measure_governor.sv` only.

## Integration capture cases

None.

## Notes

Charter §12 calls this Version 2: registered now, built late. Phase 8's
increment of 2026-08-19 built Version 1 in full and stopped here on purpose.
