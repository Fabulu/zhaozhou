# 92.2% of the census is ONE module — and the DSP is real

From `zhao_prod_top@whole-console-sizing`'s Analysis & Synthesis hierarchy,
read by OWN cost rather than subtree total.

## First, a tool defect that had to be fixed to see this

`tools/budget/map_report.py` ranked by the **leading** number in Quartus's
`TOTAL (OWN)` cells. TOTAL includes every descendant, so ranking by it counts a
child under each of its parents. It reported `zhao_geom_wcache` at **1,480,718
ALUTs** — larger than any Cyclone V, and in fact its whole subtree.

The tool now extracts both, ranks by OWN, and **self-checks**:

```
sum of every row's OWN ALUTs : 1,605,869
top row's TOTAL ALUTs        : 1,605,869
```

Two ways of counting the same silicon, agreeing exactly. That is what makes the
table below trustworthy.

## The finding: one module is 92.2% of everything

```
   ALUT_own   ALUT_tot  depth   share  module
  1,480,718  1,480,718    d=2   92.2%  zhao_vertex_arena
      8,284      8,284    d=1    0.5%  zhao_forge_cliff
      7,286      8,017    d=2    0.5%  zhao_project_core
      7,273      7,992    d=2    0.5%  zhao_project_core
      7,235  1,605,869    d=0    0.5%  zhao_prod_top   (its own glue)
      3,680      3,804    d=2    0.2%  zhao_texture_v3own
      3,084      3,914    d=1    0.2%  zhao_field_v3_normalize
```

**`zhao_vertex_arena` alone is 1,480,718 of 1,605,869 ALUTs.** Everything else in
the design — every field service, the whole texture island, both projectors, the
shell, the terrain stack — is the remaining 7.8%.

And beside it:

```
=== the wcache subtree ===
    0 own   0 tot  d=2  altsyncram
    0 own   0 tot  d=2  altsyncram
    0 own   0 tot  d=3  altsyncram_a3n1   (and five more, all zero)
```

**The arena's altsyncram instances cost zero ALUTs and the arena costs 1.48
million.** That is the signature of storage that did not become memory. The
answer to "phantom or real" for the ALUT total is: **this one number is the
whole question**, and it looks like a RAM-inference or parameterisation failure
rather than 1.6M ALUTs of intended logic.

**What this does NOT yet establish**, and must not be asserted: *why*. A depth of
storage that Quartus refused to infer, an arena sized for the census's standalone
instantiation rather than the console's, a missing `ramstyle`, or a read pattern
with too many ports — all produce this shape. The next step is to read the arena's
parameters as instantiated here and its RAM-inference messages in the map log,
not to guess.

## The DSP, by contrast, looks REAL and attributable

```
   DSP  depth  module
    45   d=1   zhao_geom_attrsetup
    39   d=1   zhao_geom_skin_norm
    33   d=1   zhao_geom_project      <- contains zhao_project_core (33)
    33   d=1   zhao_terrain_project   <- contains zhao_project_core (33)
    23   d=1   zhao_texture_island_v3_top
    17   d=1   zhao_terrain_bake
    16   d=1   zhao_shell_top
    15   d=2   zhao_geom_bin_pipe
    12   d=1   zhao_field_v3_mulbank
     9   d=1   zhao_geom_skin
     9   d=3   zhao_raster_tile_pipe
     8   d=1   zhao_twod_plane
```

These are spread across twelve named owners doing recognisable work. Nothing here
looks like an artefact.

**The projector duplication is now MEASURED, not estimated.** `zhao_geom_project`
and `zhao_terrain_project` each carry their own `zhao_project_core` at **33 DSP
apiece — 66 DSP for arithmetic that one shared service would do in 33.** The
`@cheque-price` leaf estimate said exactly this, and a whole-selection synthesis
now confirms it. That is 22% of the 297 in one deduplication, and the completion
plan already makes it part of P2 rather than a later optimisation.

The two largest, `zhao_geom_attrsetup` at 45 and `zhao_geom_skin_norm` at 39,
have had no such analysis and are now the obvious next reads.

## The honest summary

| | |
|---|---|
| **ALUT total** | dominated 92.2% by one module; treat the total as unexplained until that module is understood |
| **DSP total** | 297, spread across twelve identifiable owners; looks real |
| **66 of that 297** | the projector duplication, now measured rather than argued |
| **memory** | 1,029,005 logical bits; physical M10K occupancy still unknown until the fitter reports |

And the standing caveat: this is `zhao_prod_top`, which the completion plan
requires be labelled `RESOURCE_CENSUS_DISCONNECTED`. It is not the console, its
stimulus is independent, and none of today's eight new blocks is in it.
