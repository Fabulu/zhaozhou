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

## The same core is ALREADY on record — as a clock problem

`reports/D22-GEOM-PROJECT-FIT-20260907.md` found `zhao_project_core` from the
other direction and said so plainly:

> `61.09 MHz`, and the worst path is core-to-core **inside `u_core`** — no
> boundary to blame. … `zhao_project_core` is instantiated by both the geometry
> and the terrain projection paths, so **one block's clock problem is on both
> lanes at once**, and the standing goal is terrain hardware.

So this census did not discover the two-instance structure; D22 had it. What is
new here is the **DSP framing** — that those two instances are 66 of the 154, the
largest single item in the budget — and the multiplier-site count that explains
where the 33 goes.

**And the two findings pull in opposite directions, which is the useful part.**

D22 also records that the sharing question is *"unmeasurable by leaf fit and
needs either a composed fit containing both projectors or a decision to
time-share one core"*. Lever 1 below is exactly that decision. But:

* time-sharing one core **saves ~33 DSP** and concentrates two lanes onto one
  block, and
* that block already **misses the product clock by 39%** on a path with no
  boundary to blame.

Adding the arbitration and muxing to share it will not make that cone shorter.
So "share the core" is not a free win with an area upside — it trades the
largest DSP item against the timing of a block that is already the worst-placed
of the four recorded as genuinely short. Lever 2 (time-multiplexing the nine
matrix multiplies *within* a core) has the same character: fewer multipliers,
more control depth, on a cone that cannot afford depth.

Neither is a decision to make from a census. Both need the composed measurement
D22 named, and both belong after the island — owner direction `49fc32e9` still
stands.

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

## An UNDERCOUNT in the manifest, measured — and it does not touch DSP

Found while registering the new blocks: `check_prod_manifest.py` enforces that
every module is counted once or declared absent, and two blocks are declared
`unused` while being instantiated by `zhao_texture_island_v3_top` — which is
itself excluded *on the grounds that its blocks are counted individually*. So
they are instantiated and counted nowhere.

| block | declared | ALM | registers | M10K | DSP |
|---|---|---|---|---|---|
| `zhao_texture_frag_expand` | `unused ... not yet reached` | 323 | 451 | 3 | **0** |
| `zhao_texture_metajoin` | `unused ... not yet integrated` | *no fit row* | — | — | — |

**Undercount: at least +323 ALM, +451 registers, +3 M10K, and +0 DSP.**

The DSP figure is the one that matters here and it is **unchanged**. Neither
block uses a DSP, so 154-against-112 stands exactly as reported. This is an
ALM/M10K accounting error, not a DSP one, and saying so is the point — a
discrepancy found while investigating a budget is not automatically a
discrepancy *in* that budget.

`metajoin` is worse than mis-declared: it has **no fit row at all**, so its
contribution is not merely uncounted, it is unmeasured. A leaf fit is cheap and
it is the only way that number exists.

Two limits of this check, stated so nobody over-reads it:

* it treats only the two composed island tops as pass-through. Other excluded
  entries carry the `probe` code rather than `unused` and are correctly skipped,
  but a future composed top declared `unused` would slip past.
* instantiations are found by pattern, so unusual formatting could hide one.
  Two of seventy-four excluded blocks were flagged, and both are recent
  additions — which is consistent with a small, new problem rather than with a
  parser that is only finding the easy cases, but it is not proof of that.

Not fixed here, deliberately: moving them to `top:` changes the census total and
requires regenerating `zhao_prod_top.sv`. That belongs in one pass with the
number stated, which is now done — the number is above.

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

---

# ADDENDUM 2026-09-09: what gate 4 moves, and where the gap actually lives

Gate 4 landed the matched `zhao_raster_rcp24_svc` row this census was missing, so
two of its lines now have measured alternatives. The headline does not improve
much, and the reason is worth stating plainly.

## The levers, by strength of evidence

| lever | DSP | other cost | evidence | what blocks it |
|---|---|---|---|---|
| swap `rcp24_svc` -> `rcp24_v3` in the island | **-3** | +7 M10K | **MEASURED**, and invariant: `svc` reports 6 DSP in every row it has ever produced, `v3` reports 3 in every row | v3's throughput at NCTX=8 is 5.78 clk/recip -- 17% over the serial reference but below its OWN declared 4.6 threshold. An acceptance decision, not a measurement gap. |
| delete `zhao_texture_combine` | **-8** | -494 ALM (a gain), zero M10K | **MEASURED** (8 DSP / 494 ALM / 100.12 MHz) | owner decision; `prod_manifest.yml:69` instructs deletion and its trigger has fired, the ledger defers to the owner |
| quarter-square ROM in `material_combine_v2` | **-2** | +2 M10K | ARITHMETIC, bit-exact identity | `material_combine_v2.sv` is inside the running island fit's closure |
| share `zhao_project_core` between the two projectors | **-33** | arbitration + muxing | INFERENCE from totals; needs a composed fit | the core already misses the product clock by 39% on a path with no boundary to blame (D22) |
| ~~`perspuv_pairpipe` replacing two `perspuv_svc` lanes~~ | **0** | -- | STRUCTURE, and it settles it without a fit: `perspuv_svc` has one multiply statement inside a `for (ax = 0; ax < 2)` loop = two multipliers = the 6 DSP it measures; the pair-pipe has **two** multiply statements, so also two | **Off the list.** It is an ALM and register lever -- one scheduler, one mantissa register, no operand tables -- not a DSP one. See `PAIRPIPE-IS-NOT-A-DSP-LEVER-20260909.md`, which pre-registers 6 DSP for it. |

## The arithmetic, and it is not encouraging

```
measured sum today                                     154
  swap svc -> v3                                        -3   MEASURED
  delete zhao_texture_combine                           -8   MEASURED
  quarter-square in material_combine_v2                 -2   ARITHMETIC
                                                    -------
  every lever with evidence behind it                  141
device available                                        112
                                                    -------
  still over by                                         29
```

**Everything currently supported by evidence closes 13 of a 42-DSP gap, and 42
blocks in the manifest have still never been fitted, so the starting figure can
only rise.** The remaining 29 has exactly one place to come from: the **66 DSP in
two instances of `zhao_project_core`**, which is 43% of the whole budget and was
already the single largest item before gate 4.

So gate 4 is good news about the reciprocal tile and no news about the budget.
The DSP problem is a `zhao_project_core` problem, and it has been since the census
was first written. Two of its three levers -- core sharing and time-multiplexing
the nine matrix multiplies -- add control depth to a cone that misses 100 MHz by
39%. The third, **width-narrowing**, does not add depth -- but it is not the
free cleanup this paragraph originally called it. See
`PROJECT-CORE-OPERAND-WIDTH-20260909.md`: `GEOM.PROJECT.md` declares the full
width a deliberate robustness property ("cannot wrap for ANY input rather than
merely for legal ones"), so narrowing is a **contract change and an owner
decision**, not an engineering tidy-up.

What is true is that the property is claimed more widely than it is tested. The
random section sweeps vertices across the full s32 and caps matrix entries at
**19 bits** (+-2.0 in Q16.16), with two of the nine multiplier operands held at
literal zero. That is a reasoned choice for testing plausible poses and is not
coverage of the contract's claim. The lever is real, the prize is unmeasured, and
a `MATW` parameter defaulting to 32 plus one MapOnly prices it without changing
anything that ships.

## And one correction to this census's own table

The row `| 8 | zhao_texture_combine | yes |` was true when written and briefly
became unquotable: an unlabelled MapOnly overwrote that full-fit row twice, on
2026-09-08 and again on 2026-09-09, leaving `alms: None` under the module's name.
Both rows are restored and the map measurements now live at `@map` labels, with
the rule enforced in `run_block_fit.ps1` rather than in one caller's comment.

The DSP figure itself never moved -- a MapOnly does report `dspBlocks`, which is
why the census's numbers survived the damage while its ALM column did not. Worth
knowing which columns a map row can and cannot answer before quoting one.
