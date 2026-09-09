# FIT GATE 2: packets 1+2+3 measured, and the timing landscape changed underneath

2026-09-09. `zhao_texture_island_v3_top@g2-prod` landed after 148 minutes --
`MIGRATION_SHADOWS=0`, the configuration that would ship. Clean tree, real digest
`6812f753ff3b`, `status: ok`, no rule violations recorded.

| | `@pktC-fixed` (lab, pre-packets) | **`@g2-prod`** (production, packets 1-3) | delta |
|---|---|---|---|
| ALM | 15,483 | **10,837** | **-4,646 (-30.0%)** |
| registers | 22,219 | **16,285** | **-5,934 (-26.7%)** |
| Fmax | 62.83 | **82.05** | **+19.22 MHz (+30.6%)** |
| DSP | 17 | 17 | 0 |
| M10K | 48 | 49 | +1 |
| blockMemoryBits | -- | 64,802 | -- |

## THE CONFOUND, declared before the fit ran and still real

That delta is **packets 1-3 PLUS the removal of the migration laboratory**, because
`@pktC-fixed` was elaborated with the shadows present and `@g2-prod` was not. This
was stated in the roadmap before the fit started, and it is stated again here
rather than quietly dropped.

**The register half is isolated.** Gate 1's controlled MapOnly pair -- same bytes,
one parameter apart -- measured the laboratory at **-4,432 registers and 0 DSP**.
So of the -5,934 registers, roughly **-1,502 belong to packets 1-3** and the rest
is scaffolding coming out.

**The ALM half is NOT isolated and I am not going to divide it.** A MapOnly reports
no ALM at all, so gate 1 could not price the laboratory in ALM, and there is no
full fit of the CURRENT sources with `MIGRATION_SHADOWS=1` to difference against.
A plausible split exists -- 4,432 registers at roughly two per ALM is on the order
of 2,200 ALM -- and it is exactly the division-of-a-total this repository has been
burned believing twice this week. It is an arithmetic guess, not a measurement,
and the honest statement is: **-4,646 ALM for the two changes together.**

Buying the split costs one more island fit. It is not worth one.

## AGAINST THE ORACLE, which is the comparison that matters

The benchmarks in `G1D-COMPOSED-ISLAND-20260905.md` are 6,600 nominal / 7,500
redline / 7,913 standalone sum, and that report applies them to the composed
island. The oracle's best clean row is `@p0c-stageA`:

| | oracle `@p0c-stageA` | **V3 `@g2-prod`** | delta |
|---|---|---|---|
| ALM | 11,562 | **10,837** | **-725** |
| registers | 19,203 | **16,285** | **-2,918** |
| Fmax | 84.03 | 82.05 | **-1.98** |
| M10K | 39 | 49 | **+10** |
| DSP | 17 | 17 | 0 |

**V3 production is the smallest composed island yet measured** -- 725 ALM and
2,918 registers under the oracle's best -- for +10 M10K and 2 MHz. That is the
memory-for-ALM trade working at composition scale, not just at the leaf.

### And it still fails the redline

```
              @g2-prod    x nominal (6,600)   x redline (7,500)
  ALM          10,837          1.64x                1.445x
```

Down from the oracle's 1.75x and 1.54x, and **still 3,337 ALM over redline.** The
island does not fit its ALM budget and this fit does not change that verdict --
it moves the ratio, which is the most any single packet was ever going to do.

## THE TIMING LANDSCAPE CHANGED, and it retires an earlier finding of mine

`ISLAND-TIMING-IS-ONE-REGISTER-BIT-20260909.md` established, from
`@pktC-fixed`, that **42 of 43 internal paths started at `u_own|live_cnt_q[6]`**
and that the reported 62.83 MHz came from a *pin* path into the palette resolver.
Both were true of that row. **Neither is true of the shipping configuration.**

Same tool (`tools/quartus/path_census.py`), same 200 summary paths:

| | `@pktC-fixed` | `@g2-prod` |
|---|---|---|
| boundary-touching | 157 | **83** |
| internal-to-internal | 43 | **117** |
| worst OVERALL | **-5.915, a PIN** into the palette | **-2.187, INTERNAL** (`u_rcp|c_val[4]` -> `u_rcp|Add7`) |
| reported Fmax | 62.83 | 82.05 |
| internal-only Fmax | 77.45 | **82.05 -- the same number** |

**The boundary is no longer the limit.** Worst-overall and worst-internal are now
the same path, so 82.05 MHz is a number about the design rather than about the
leaf-fit's virtual pins. That is a materially more trustworthy figure than 62.83
ever was.

And the hot node moved:

```
  @pktC-fixed   live_cnt_q[6]        42 of 43 internal paths
  @g2-prod      u_cache|c2_tag[0][14]  59 of 117
                live_cnt_q[6]          28 of 117
                u_rcp|c_val[4]         25 of 117
```

So the credit-fanout work that report recommended is now a **third** of the
problem rather than all of it, and the cache pipe's tag comparison is the new
leader. **A conclusion tied to a superseded measurement is a conclusion about a
design that no longer exists** -- which is this repo's own law about never
comparing a current file to an old measurement, arriving from the other
direction.

### What the RCP swap is worth now: 1.06 MHz

Excluding every path that terminates in `u_rcp` moves the internal ceiling
**82.05 -> 83.12** and then stops at a palette-internal path. It was 4.08 MHz on
the old row; it is **1.06 MHz** now. The swap still stands on ALM and DSP -- 3 DSP
invariant across every row either module ever produced -- and its timing case at
the island is now close to nil.

## What this gate does NOT settle

* The ALM redline. 3,337 over, and the next lever is not in this island.
* Whether the laboratory can now be deleted outright rather than parameterised
  out. It costs 4,432 registers and its shadow comparisons are D3's migration
  proof (1,176 comparisons, 0 mismatches); deleting it ends that proof. An owner
  call, and cheap to defer since `MIGRATION_SHADOWS=0` already excludes it from
  what ships.
* `u_cache|c2_tag[0][14]`, the new hot node, which nobody has looked at.
