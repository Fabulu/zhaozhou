# The DSP budget: 154 measured against 112, and it is a FLOOR

2026-09-08. Prompted by the owner's observation that the DSP overshoot is "like
180 to 112 … lots of that is bad accounting". The accounting turns out to be
better than feared and the news is worse than that implies.

## The number

Summing `dspBlocks` over the blocks `fpga/rtl/prod/zhao_prod_top.sv` actually
instantiates, excluding labelled variant rows (which are alternate measurements
of the same module, not additional hardware):

```
SUM over production-instantiated, measured blocks : 154
device DSP available (5CSEBA6U23I7)               : 112
```

**But 42 of the 73 instantiated modules have never been fitted at all.** 154 is
the sum over the 31 that have. The true figure is higher, and the owner's ~180
is a reasonable expectation rather than a pessimistic one.

## CORRECTION, same day: what `zhao_prod_top` actually is

The section below originally said the double-count hypothesis was "checked and
does not hold", citing that no production-instantiated module instantiates
another. **That check could not have failed, and reporting it as evidence was
the same mistake this report elsewhere catches in others.**

`fpga/rtl/prod/zhao_prod_top.sv` is GENERATED, and its own header says what it
is:

> "ONE instance of every intended production block, so the fitter can answer the
> owner's question: what does the planned console cost when counted ONCE? **This
> is a RESOURCE top, not the console.** Blocks are not wired to each other, so no
> timing number here means anything."

A flat resource top **cannot** nest by construction. Asking whether it nests is
asking whether a list contains itself. The answer was structurally guaranteed
before I ran it, and I presented it as though the design had passed a test.

### What the number therefore is

The sum is over the **manifest's set of intended blocks, each counted once**.
That is a meaningful quantity and the right one for "what does the planned
machine cost" — but `gen_prod_top.py` states the two ways it is wrong, and both
belong beside it:

* an **UPPER bound on the sum of parts**, because composition shares queues,
  control and arithmetic that this top duplicates;
* a **LOWER bound on the machine**, because integration glue is not here and
  neither are the blocks nobody has built yet.

So 154 is not simply "too high" or "too low" — it is bounded on both sides, and
the 42 unfitted blocks push the second bound further out. What survives
unchanged: **154 is what the ledger measures today against 112 available, the
gap is real, and it is not explained away by nesting.** The difference is that
"not explained by nesting" is now a statement about the manifest's structure
rather than a test result.

### One thing the correction does NOT rescue

The concentration is unaffected. `zhao_geom_project` and `zhao_terrain_project`
are two separate manifest entries at 33 DSP each, and they instantiate the same
`zhao_project_core`. Counting each once is correct for "cost of the planned
machine" precisely because the machine really would contain two.

What *is* uncertain, stated plainly:

* **63 of the 154 come from rows fitted on a DIRTY TREE** — `terrain_project`
  (33), `terrain_normals` (18), `terrain_tess` (6), `geom_setup` (4),
  `geom_clip` (2). Those numbers do not describe the commits they name. They
  could move in either direction; they are not evidence of overcounting.
* **42 blocks unmeasured**, so the total can only grow.

## Where it actually is

| DSP | block | tree clean |
|---|---|---|
| **33** | `zhao_terrain_project` | **no** |
| **33** | `zhao_geom_project` | yes |
| 18 | `zhao_terrain_normals` | no |
| 15 | `zhao_geom_cull` | yes |
| 9 | `zhao_geom_skin` | yes |
| 8 | `zhao_texture_combine` | yes |
| 6 | `zhao_terrain_tess`, `zhao_raster_rcp24_svc`, `zhao_raster_perspuv_svc`, `zhao_geom_lod` | mixed |
| 4 | `zhao_geom_setup` | no |
| 3 | `zhao_texture_bilerp_lane`, `zhao_terrain_lod` | yes |
| 2 | `zhao_texture_material_combine_v1`, `zhao_geom_clip` | mixed |

**66 of 154 — 43% — is two blocks at 33 each. And they are the same block.**

`zhao_geom_project` is 175 lines whose entire body is one instantiation of
`zhao_project_core` (line 126). `zhao_terrain_project` instantiates the *same*
core (line 257). The single largest item in the DSP budget is one core built
twice.

## What the core spends it on

`fpga/rtl/common/zhao_project_core.sv` declares

```systemverilog
function automatic logic signed [63:0] mul32(input logic signed [31:0] a,
                                             input logic signed [31:0] b);
  mul32 = $signed({{32{a[31]}}, a}) * $signed({{32{b[31]}}, b});
endfunction
```

and calls it **nine times, all in one combinational block** (lines 359–364):
three for `row_x`, three for `row_y`, three for `row_cw` — the 3×4 matrix-vector
transform, fully parallel. Two further multiplies form the viewport scale
(`prod_x_c`, `prod_y_c`, lines 599–600).

A function call is not shared hardware: each of the nine call sites synthesises
its own multiplier. So the core builds **eleven parallel multipliers**, and the
design contains **two cores — twenty-two.**

Note the shape of `mul32`: both 32-bit operands are sign-extended to 64 bits
*before* the multiply, so the written arithmetic is a 64×64 product where a
32×32→64 would do. 33 DSP over 11 multipliers is about 3 each, which is roughly
what a 32×32 costs on this device — so Quartus does appear to be pruning the
sign-extension rather than building 64×64 units. **That per-multiplier
attribution is an inference from the totals, not a measurement**; the fit's DSP
report would settle it and MapOnly is cheap. It is recorded as an inference
deliberately, because a plausible division of a total is exactly the kind of
number this repository has been burned believing.

## What could be done, without recommending any of it

Three levers, in descending size. None is a decision I should make:

1. **Share one projection core** between geometry and terrain instead of
   instantiating two. Saves ~33. Requires that the two pipelines do not need to
   project on the same cycle, or can be scheduled — an architecture question,
   not a cleanup.
2. **Time-multiplex the nine matrix multiplies.** Three multipliers over three
   cycles computes the same 3×4 transform; nine in parallel is a latency choice
   that costs ~6 DSP per core saved, ~12 across both. Whether the projection
   stage has three cycles to spend is a throughput question with a real answer
   nobody has measured.
3. **Narrow `mul32`.** If the 64-bit sign extension is *not* being pruned, this
   is large; if it is, it is nothing. Cheap to settle and worth settling before
   anyone argues about it.

Levers 1 and 2 together are on the order of 45 DSP — roughly the whole overshoot
over 112, before the 42 unmeasured blocks are counted.

## A fourth lever, texture-side and cheap: `zhao_texture_combine`

Found while checking the standing COMBINE item. Two blocks in this repository
contain the **same 8x8-plus-round multiply shape**, and they cost very different
amounts:

| block | the product declaration | DSP |
|---|---|---|
| `zhao_texture_material_combine_v1` | `(* multstyle = "logic" *) logic [16:0] p;` | **2** |
| `zhao_texture_combine` | `logic [16:0] p;` — **no attribute** | **8** |

```systemverilog
// zhao_texture_combine.sv:109  -- twelve call sites, no multstyle
function automatic logic [7:0] unit_mul(input logic [7:0] a, input logic [7:0] b);
  logic [16:0] p;
  p = ({9'd0, a} * {9'd0, b}) + 17'd128;
```

`unit_mul` is called seven times directly and five more through `mul2x9`, so the
block builds twelve 8x8 multipliers. An 8x8 product is small enough to belong in
logic, and `material_combine_v1`'s own comment says so: *"multstyle = logic is
the whole point of this block."*

**Hypothesis, not a claim:** adding the attribute moves up to 8 DSP into logic.

**And the reason it is only a hypothesis is written in CLAUDE.md.** The last
time a combiner read 8 DSP against a rule of 2, the obvious answer was "Quartus
is ignoring `multstyle`" and that answer was *wrong* — the block really did
contain about fourteen multipliers inside two seven-arm case statements. This is
a different situation (the attribute is ABSENT here, not present-and-ignored,
and the sibling block demonstrates it working in this very tree), but it is the
same shape of comfortable explanation and it gets the same treatment: **MapOnly
settles it in minutes and no fit is needed.** Do that before anyone edits
anything.

Worth noting either way: `zhao_texture_combine` is instantiated ONLY by the
generated resource top. The islands use `zhao_texture_material_combine_v2`. So
the question of whether this block is still wanted at all should be asked before
the question of what it costs.

## The one already decided

The RCP swap the owner ruled on today (`OWNER-DECISION-RCP-V3-20260908.md`) takes
`zhao_raster_rcp24_svc` from 6 to 3. Against a whole-machine overshoot of 42+
that is a small piece — but it is the piece that is authorised, measured and
about to be fitted, and it clears the *island's* own local breach (17 → 14)
exactly.

## What this changes about the roadmap

Nothing, yet, and deliberately. Owner direction `49fc32e9` is still standing —
finish the texture island; terrain and projection are not the current
implementation priority. This report is the census that makes the size of the
problem known, filed so that when the island is done the cleanup starts from a
measured position rather than from an impression. The two biggest items are both
outside the island.

**The one thing worth doing before then is cheap:** MapOnly the two `project`
blocks to confirm the per-multiplier attribution above, and re-fit the five
dirty-tree rows so 63 of the 154 stops being unanchored. Neither is an island
fit and neither competes with the roadmap's gates.
