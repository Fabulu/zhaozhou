# Per-entity attribution — `zhao_console_core`

**DEVICE: `5CEBA9F31C7`.** Rows from DIFFERENT devices MUST NOT be differenced —
Quartus replaces multipliers a part cannot hold, so a smaller device
reports fewer DSPs and more ALUTs for the same RTL.

Derived by `tools/budget/map_entity_attrib.py` from the Analysis &
Synthesis entity table. **Synthesis estimates, not a placement result:**
this design has never placed, so there are no ALM figures and no Fmax,
and nothing here should be quoted as either.

| | measured | against 5CSEBA6U23I7 |
|---|---:|---:|
| combinational ALUTs | 265558 | 317% of ~83820 |
| dedicated logic registers | 312898 | 187% of 167640 |
| block memory bits | 3220980 | 57% of 5662720 |
| DSP blocks | 369 | **329%** of 112 |

**The registers alone need at least 78224 ALM, 187% of the part, with the
combinational logic at zero.** Memory, by contrast, fits: 57%. That is
the whole shape of the problem in two numbers — storage held in flip-flops
is what overflows this device, and M10K is where the slack is.

## Biggest subtrees by combinational ALUTs

| entity | ALUTs | % of part | registers | mem bits | DSP |
|---|---:|---:|---:|---:|---:|
| `zhao_shell_top_v2:u_shell` | 48352 | 58% | 48474 | 574096 | 91 |
| `zhao_field_host_v2:u_field_host` | 39964 | 48% | 48614 | 85282 | 15 |
| `zhao_forge_assemble:u_forge_assemble` | 15526 | 19% | 39023 | 2048 | 3 |
| `zhao_proj_subsystem:u_proj_subsystem` | 8653 | 10% | 8107 | 105583 | 39 |
| `zhao_geom_proj_lane:u_geom_proj_lane` | 7563 | 9% | 4491 | 187308 | 2 |
| `zhao_terrain_devstore:u_terrain_devstore` | 7034 | 8% | 4341 | 0 | 2 |
| `zhao_geom_lodstate:u_geom_lodstate` | 6151 | 7% | 10825 | 0 | 6 |
| `zhao_terrain_heighttap:u_terrain_heighttap` | 5529 | 7% | 2370 | 0 | 6 |
| `zhao_cmd_exec:u_cmd_exec` | 5322 | 6% | 12149 | 18472 | 0 |
| `zhao_light_stream:u_light_stream` | 5167 | 6% | 7070 | 5016 | 9 |

## Biggest subtrees by REGISTERS

A module with many registers and **no block memory bits** is holding an
array in flip-flops. That is the EARTHRAM lever and it is where the ALMs
are: one such array cost 5,181 registers and ~2,698 estimated ALMs, and
4,880 M10K bits bought all of it back at zero added cycles.

| entity | registers | % of part | ALUTs | mem bits |
|---|---:|---:|---:|---:|
| `zhao_field_host_v2:u_field_host` | 48614 | 29% | 39964 | 85282 |
| `zhao_shell_top_v2:u_shell` | 48474 | 29% | 48352 | 574096 |
| `zhao_forge_assemble:u_forge_assemble` | 39023 | 23% | 15526 | 2048 |
| `zhao_cmd_exec:u_cmd_exec` | 12149 | 7% | 5322 | 18472 |
| `zhao_geom_lodstate:u_geom_lodstate` | 10825 | 6% | 6151 | 0 |
| `zhao_proj_subsystem:u_proj_subsystem` | 8107 | 5% | 8653 | 105583 |
| `zhao_light_stream:u_light_stream` | 7070 | 4% | 5167 | 5016 |
| `zhao_geom_ladderbank:u_geom_ladderbank` | 5951 | 4% | 2563 | 0 |
| `zhao_material_resolve:u_material_resolve` | 5207 | 3% | 2439 | 896 |
| `zhao_geom_proj_lane:u_geom_proj_lane` | 4491 | 3% | 7563 | 187308 |

## Biggest subtrees by DSP

**The M10K trade does nothing for this column.** A block here is a
candidate for a quarter-square or coefficient-memory replacement, which
is a different programme from moving an array into a memory.

| entity | DSP | % of part | ALUTs |
|---|---:|---:|---:|
| `zhao_shell_top_v2:u_shell` | 91 | 81% | 48352 |
| `zhao_proj_subsystem:u_proj_subsystem` | 39 | 35% | 8653 |
| `zhao_geom_attrpack:u_geom_attrpack` | 36 | 32% | 1081 |
| `zhao_geom_skin_norm:u_geom_skin_norm` | 21 | 19% | 1211 |
| `zhao_part_collide:u_part_collide` | 20 | 18% | 1664 |
| `zhao_field_host_v2:u_field_host` | 15 | 13% | 39964 |
| `zhao_post_composite:u_post_composite` | 12 | 11% | 1147 |
| `zhao_geom_skin:u_geom_skin` | 9 | 8% | 2116 |
| `zhao_light_stream:u_light_stream` | 9 | 8% | 5167 |
| `zhao_twod_plane:u_twod_plane` | 8 | 7% | 893 |

