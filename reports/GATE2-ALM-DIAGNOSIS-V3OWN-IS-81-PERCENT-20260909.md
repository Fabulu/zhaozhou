# Texture gate 2 fails its ALM redline, and ONE BLOCK is 81% of the overage

2026-09-09. Owner brief section 7.1: *"The current texture gate remains the
immediate task... If a texture gate fails, diagnose the failed specimen and
complete the existing remedy; do not use this brief as an excuse to disappear
into projection."*

`@g2-prod` fits at **10,836 ALM against a 7,500 redline** -- over by **3,336**.
This is that diagnosis, from the fit's own per-entity table rather than from a
sum of leaf fits.

## Where the island's ALM actually is

Own contribution, excluding children, from
`blockpaths/zhao_texture_island_v3_top@g2-prod.fit.rpt` section 14:

| entity | own ALM | share of island | inclusive |
|---|---|---|---|
| **`zhao_texture_v3own:u_own`** | **2,706.7** | **25.0%** | 2,818.2 |
| `zhao_raster_perspuv_svc:u_persp` | 1,710.1 | 15.8% | 1,710.1 |
| `zhao_texture_island_v3_top` (its own glue) | 1,306.0 | 12.1% | 10,836.0 |
| `zhao_texture_cache_pipe:u_cache` | 1,150.8 | 10.6% | 1,150.8 |
| `zhao_raster_rcp24_svc:u_rcp` | 869.8 | 8.0% | 1,002.9 |
| `zhao_texture_tmu_plan:u_plan` | 771.2 | 7.1% | 771.2 |
| `zhao_texture_material_combine_v2:u_combine` | 536.7 | 5.0% | 536.7 |
| `zhao_texture_rsp_dispatch:u_dispatch` | 470.8 | 4.3% | 470.8 |
| `zhao_texture_aux_pipe:u_aux` | 293.2 | 2.7% | 501.8 |
| `zhao_texture_palette_res:u_palette` | 274.1 | 2.5% | 274.1 |
| `zhao_texture_aux_div6:u_div` | 208.6 | 1.9% | 208.6 |
| `zhao_texture_frag_expand:u_expand` | 146.4 | 1.4% | 146.4 |
| `zhao_field_rcp24_rom:u_rom` | 133.2 | 1.2% | 133.2 |
| `zhao_texture_bilerp_lane:u_bilerp` | 81.0 | 0.7% | 81.0 |

## The finding

**`zhao_texture_v3own` is 2,707 ALM -- a quarter of the island, and 81% of the
3,336-ALM overage on its own.** The 64-owner transaction file and completion
pipeline is the ALM problem. Nothing else is close: the next block is 1,000 ALM
smaller, and the four smallest entities together are under 570.

That is not where I would have looked. The reciprocal tile carried the DSP
argument all day, the combiner carries the brief's ROM packets, and the cache
pipe is the new timing hot node -- and none of the three is the area problem.

**And `v3own` is also implicated in timing.** `ISLAND-TIMING-IS-ONE-REGISTER-BIT`
found `u_own|live_cnt_q[6]` sourcing 42 of 43 internal paths on the pre-packet
specimen, and on `@g2-prod` it still sources 28 of 117. So the largest area
consumer and a third of the internal timing paths are the same block.

## What this means for the brief's queued texture packets

Sections 7.2, 7.3 and 7.4 propose replacing products with quarter-square ROM
lanes in `material_combine_v2` (2 DSP -> 0), the bilinear filter (3 DSP -> 0) and
pixel fog. Those are **DSP levers, and they are aimed at 618 ALM of the island**
(combine 536.7 + bilerp 81.0). Even a total elimination of both blocks would
leave the island **2,718 ALM over its redline.**

So, stated plainly and without proposing the work: **the queued ROM packets
cannot close this gate.** They are the right moves for the DSP objective and the
wrong instrument for the ALM redline. Whatever closes 7,500 has to reach
`v3own`.

## What is NOT claimed here

* **No cause for `v3own`'s 2,707 ALM.** This is a placement-attributed area
  figure, not an analysis of what inside the block spends it. The obvious
  suspects -- a 64-entry transaction file, the completion pipeline, seven
  32-bit counters, the fence phase machine -- are suspects, not measurements.
  The next step is that block's own register/array attribution, which is a
  MapOnly question and needs no island fit.
* **No proposal.** Brief section 0.1 authorises continuing texture and forbids
  turning this into a second project. A diagnosis is not a rewrite, and the
  remedy belongs to whoever owns the texture gate's acceptance.
* **The 2,704.5 "recoverable by dense packing" figure for the whole island is
  not subtracted anywhere above.** Own-ALM columns are `[A] used in final
  placement` minus `[B] recoverable` plus `[C]`; quoting the recoverable
  estimate as a saving would be reading the fitter's own slack as a design
  change.

## Provenance

`@g2-prod`, `MIGRATION_SHADOWS=0`, clean tree, digest `6812f753ff3b`, 148
minutes, `status: ok`. The declared shipping profile in
`design/prod_manifest.yml`. Section 14 of the fit report is the source for every
number above, and the first parse of it read the table of CONTENTS instead of the
table -- which is why the entity list is quoted with its totals reconciling to
10,836.
