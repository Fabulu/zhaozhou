# Full console core — Analysis & Synthesis, 2026-09-29

**`Analysis & Synthesis Status : Successful`**, Quartus Prime 17.0.2, top-level entity
`zhao_console_core`, finished 11:30:28. Zero errors. This is the first time the whole console
has been synthesized as one unit in this campaign.

**Placement never finished.** `quartus_fit` started at 11:30 and was killed at ~11:38, eight
minutes in, by Claude Code's background memory-pressure reaper — not by Quartus and not by any
fault of the fit. There is therefore **no post-fit ALM and no Fmax**; everything below is
synthesis-stage. The numbers are preserved here because the temp workspace does not survive.

## Against `5CSEBA6U23I7` (41,910 ALM / 83,820 ALUT / 112 DSP / 553 M10K)

| | measured | ceiling | |
|---|---|---|---|
| Estimate of logic utilization (ALMs needed) | **212,198** | 41,910 | **506%** |
| Combinational ALUT usage for logic | **285,628** | 83,820 | **341%** |
| Dedicated logic registers | 256,111 | — | |
| Total block memory bits | 3,411,015 | ~5.66 Mb | ~60% |
| Total DSP blocks | **128** | 112 | **114%** |
| Total virtual pins | 43,486 | — | leaf-fit boundary |

The ALM figure is Quartus's own pre-placement *estimate*; the ALUT figure is a synthesis
measurement and is the harder of the two to argue with. Either way the design does not fit,
and by a factor, not a margin. **That is a measured engineering finding, not a licence to
invent a pass.**

## Independent corroboration of the FIELD store repair

V1 measured **293,886** combinational ALUTs. This run reads **285,628** — a drop of **8,258**,
against the **−8,177 ALUT** that the `zhao_field_v3_exec` store repair measured standalone at
PLAN=48. The two agree to within 1%, which is evidence the repair landed in the composed
design and did there what it claimed to do in isolation.

## Where the ALUTs are

From `resource_utilization_by_entity.txt`. **These totals are NESTED** — a parent includes its
children — so they must not be summed. Read it as a containment tree, and note that the
render path dominates:

    285628  |zhao_console_core                            (whole design)
     59720    |zhao_shell_top_v2:u_shell|
     43673      |zhao_geom_bin_pipe_v2:u_render_bin|
     41409        |zhao_raster_tile_pipe_v2:u_tile|
     21316          |zhao_raster_texture_stage_v3:u_texture_stage|
     21127            |zhao_texture_island_v3_top:u_texture_v3|
     33858    |zhao_field_host_v2:u_field_host|
     24276      |zhao_field_v3_engine:u_fabric|
     21929      |zhao_field_v3_svcpath:u_svc|
     11243    |zhao_proj_subsystem:u_proj_subsystem|
     10669      |zhao_project_service:u_svc|
     10097        |zhao_project_core:u_core|
      7553    |zhao_vertex_arena:u_arena|  |zhao_geom_wcache:u_wcache|  |zhao_geom_proj_lane|
      7101    |zhao_terrain_devstore:u_terrain_devstore|
      5756    |zhao_terrain_heighttap:u_terrain_heighttap|
      5606    |zhao_light_stream:u_light_stream|
      5506    |zhao_field_v3_mulbank:u_bank|
      5442    |zhao_cmd_exec:u_cmd_exec|
      5057    |zhao_part_collide:u_part_collide|
      4983    |zhao_field_v3_curve:u_curve|

Two cautions before this table is used as an optimization target list:

* **The nesting is not fully resolved here.** `u_fabric` (24,276) and `u_svc` (21,929) sum to
  more than their apparent parent `u_field_host` (33,858), so the containment is not the
  simple tree the indentation suggests and the real parent/child edges need reading out of the
  full report before any saving is attributed.
* **The listed entries account for roughly 105k of 285,628** once nesting is allowed for, so a
  large remainder sits in instances below the 2,500-ALUT cut used to build this list. The
  biggest single lever may not be on this page.

## Files

| file | what it is |
|---|---|
| `blockfit.map.summary` | Quartus's own summary, verbatim |
| `blockfit.flow.rpt` | the flow log for the run |
| `blockfit.map.rpt.head4000.txt` | first 4,000 lines of the 110,224-line report (summary + RAM/DSP sections) |
| `resource_utilization_by_entity.txt` | the per-entity resource table, extracted whole |

**No `.sources.sha256` was recoverable.** The launch banner recorded
`source digest: abf0c8d52952 over 299 file(s)`, and the digest file was written inside the
temp workspace rather than into the repo, so that string is the only surviving provenance
pin. The run started from a clean tree at `HEAD=2a354f96`; the only closure file to change
since is `zhao_console_core.sv`, by 17 lines, all comments.
