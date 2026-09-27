# Per-entity attribution — `zhao_console_core`

**DEVICE: `5CSEBA6U23I7`.** Rows from DIFFERENT devices MUST NOT be differenced —
Quartus replaces multipliers a part cannot hold, so a smaller device
reports fewer DSPs and more ALUTs for the same RTL.

Derived by `tools/budget/map_entity_attrib.py` from the Analysis &
Synthesis entity table. **Synthesis estimates, not a placement result:**
this design has never placed, so there are no ALM figures and no Fmax,
and nothing here should be quoted as either.

| provenance | |
|---|---|
| source report | `reports/synthesis/blockpaths/zhao_console_core@current-20260928.map.rpt` |
| Analysis & Synthesis | Successful - Mon Sep 28 00:14:57 2026 |

**The stamp above is the MEASUREMENT's date, not this file's.** It is read
from the report's own status line rather than a file mtime, because a
copied report carries the copy's date. The source `.map.rpt` is
gitignored, so this row is the only thing tying the numbers below to an
input anyone can go and re-read -- and a table that cannot be traced to
one is a number somebody once pasted.

| | measured | against 5CSEBA6U23I7 |
|---|---:|---:|
| combinational ALUTs | 293886 | 351% of ~83820 |
| dedicated logic registers | 279210 | 167% of 167640 |
| block memory bits | 3387975 | 60% of 5662720 |
| DSP blocks | 128 | **114%** of 112 |

**The registers alone need at least 69802 ALM, 167% of the part, with the
combinational logic at zero.** Memory, by contrast, fits: 60%. That is
the whole shape of the problem in two numbers — storage held in flip-flops
is what overflows this device, and M10K is where the slack is.

## Biggest subtrees by combinational ALUTs

| entity | ALUTs | % of part | registers | mem bits | DSP |
|---|---:|---:|---:|---:|---:|
| `zhao_shell_top_v2:u_shell` | 59723 | 71% | 48433 | 407750 | 17 |
| `zhao_field_host_v2:u_field_host` | 42122 | 50% | 48611 | 85282 | 5 |
| `zhao_proj_subsystem:u_proj_subsystem` | 11239 | 13% | 7941 | 75460 | 22 |
| `zhao_geom_proj_lane:u_geom_proj_lane` | 7553 | 9% | 4491 | 187308 | 0 |
| `zhao_terrain_devstore:u_terrain_devstore` | 7098 | 8% | 4418 | 0 | 0 |
| `zhao_terrain_heighttap:u_terrain_heighttap` | 5754 | 7% | 2370 | 0 | 4 |
| `zhao_light_stream:u_light_stream` | 5606 | 7% | 7070 | 5016 | 7 |
| `zhao_cmd_exec:u_cmd_exec` | 5440 | 6% | 12840 | 18472 | 0 |
| `zhao_part_collide:u_part_collide` | 5060 | 6% | 555 | 0 | 5 |
| `zhao_field_loader:u_field_loader` | 3803 | 5% | 2815 | 0 | 0 |
| `zhao_geom_lodstate:u_geom_lodstate` | 3302 | 4% | 2122 | 9216 | 4 |
| `zhao_part_update:u_part_update` | 3266 | 4% | 687 | 0 | 0 |
| `zhao_terrain_pageio:u_terrain_pageio` | 3200 | 4% | 1357 | 26624 | 0 |
| `zhao_forge_assemble:u_forge_assemble` | 3197 | 4% | 4183 | 36888 | 1 |
| `zhao_geom_skin_norm:u_geom_skin_norm` | 3185 | 4% | 920 | 0 | 0 |
| `zhao_geom_cull:u_geom_cull` | 3061 | 4% | 1825 | 0 | 2 |
| `zhao_geom_vattr:u_geom_vattr` | 3054 | 4% | 2557 | 41932 | 2 |
| `zhao_geom_loom:u_geom_loom` | 2652 | 3% | 3337 | 408849 | 1 |
| `zhao_geom_skin:u_geom_skin` | 2644 | 3% | 2146 | 0 | 6 |
| `zhao_forge_ring_eval:u_forge_ring_eval` | 2632 | 3% | 2012 | 4369 | 2 |

## Biggest subtrees by REGISTERS

A module with many registers and **no block memory bits** is holding an
array in flip-flops. That is the EARTHRAM lever and it is where the ALMs
are: one such array cost 5,181 registers and ~2,698 estimated ALMs, and
4,880 M10K bits bought all of it back at zero added cycles.

| entity | registers | % of part | ALUTs | mem bits |
|---|---:|---:|---:|---:|
| `zhao_field_host_v2:u_field_host` | 48611 | 29% | 42122 | 85282 |
| `zhao_shell_top_v2:u_shell` | 48433 | 29% | 59723 | 407750 |
| `zhao_cmd_exec:u_cmd_exec` | 12840 | 8% | 5440 | 18472 |
| `zhao_proj_subsystem:u_proj_subsystem` | 7941 | 5% | 11239 | 75460 |
| `zhao_light_stream:u_light_stream` | 7070 | 4% | 5606 | 5016 |
| `zhao_geom_ladderbank:u_geom_ladderbank` | 5951 | 4% | 2574 | 0 |
| `zhao_material_resolve:u_material_resolve` | 5207 | 3% | 2441 | 896 |
| `zhao_geom_proj_lane:u_geom_proj_lane` | 4491 | 3% | 7553 | 187308 |
| `zhao_terrain_devstore:u_terrain_devstore` | 4418 | 3% | 7098 | 0 |
| `zhao_forge_assemble:u_forge_assemble` | 4183 | 2% | 3197 | 36888 |
| `zhao_geom_pose_decode:u_geom_pose_decode` | 3897 | 2% | 1958 | 12288 |
| `zhao_geom_loom:u_geom_loom` | 3337 | 2% | 2652 | 408849 |
| `zhao_field_loader:u_field_loader` | 2815 | 2% | 3803 | 0 |
| `zhao_terrain_fieldlist:u_terrain_fieldlist` | 2800 | 2% | 1057 | 0 |
| `zhao_part_terrain_tap:u_part_terrain_tap` | 2783 | 2% | 2511 | 0 |
| `zhao_geom_clipread:u_geom_clipread` | 2590 | 2% | 2130 | 0 |
| `zhao_geom_vattr:u_geom_vattr` | 2557 | 2% | 3054 | 41932 |
| `zhao_terrain_clipfeed:u_terrain_clipfeed` | 2533 | 2% | 2612 | 1408 |
| `zhao_terrain_pagestream:u_terrain_pagestream` | 2470 | 1% | 2266 | 0 |
| `zhao_geom_paramwalk:u_geom_paramwalk` | 2461 | 1% | 1027 | 0 |

## Biggest subtrees by DSP

**The M10K trade does nothing for this column.** A block here is a
candidate for a quarter-square or coefficient-memory replacement, which
is a different programme from moving an array into a memory.

| entity | DSP | % of part | ALUTs |
|---|---:|---:|---:|
| `zhao_geom_attrpack:u_geom_attrpack` | 24 | 21% | 2232 |
| `zhao_proj_subsystem:u_proj_subsystem` | 22 | 20% | 11239 |
| `zhao_shell_top_v2:u_shell` | 17 | 15% | 59723 |
| `zhao_light_stream:u_light_stream` | 7 | 6% | 5606 |
| `zhao_geom_skin:u_geom_skin` | 6 | 5% | 2644 |
| `zhao_field_host_v2:u_field_host` | 5 | 4% | 42122 |
| `zhao_part_collide:u_part_collide` | 5 | 4% | 5060 |
| `zhao_geom_lodstate:u_geom_lodstate` | 4 | 4% | 3302 |
| `zhao_geom_setup:u_geom_setup` | 4 | 4% | 473 |
| `zhao_terrain_heighttap:u_terrain_heighttap` | 4 | 4% | 5754 |
| `zhao_terrain_clipfeed:u_terrain_clipfeed` | 3 | 3% | 2612 |
| `zhao_forge_prim_eval:u_forge_prim_eval` | 2 | 2% | 2575 |
| `zhao_forge_ring_eval:u_forge_ring_eval` | 2 | 2% | 2632 |
| `zhao_forge_shadow:u_forge_shadow` | 2 | 2% | 988 |
| `zhao_geom_clip:u_geom_clip` | 2 | 2% | 964 |
| `zhao_geom_cull:u_geom_cull` | 2 | 2% | 3061 |
| `zhao_geom_meshfetch:u_geom_meshfetch` | 2 | 2% | 1785 |
| `zhao_geom_pose_decode:u_geom_pose_decode` | 2 | 2% | 1958 |
| `zhao_geom_vattr:u_geom_vattr` | 2 | 2% | 3054 |
| `zhao_part_terrain_tap:u_part_terrain_tap` | 2 | 2% | 2511 |

