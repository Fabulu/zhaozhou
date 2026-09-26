# Per-entity attribution — `zhao_console_core`

**DEVICE: `5CSEBA6U23I7`.** Rows from DIFFERENT devices MUST NOT be differenced —
Quartus replaces multipliers a part cannot hold, so a smaller device
reports fewer DSPs and more ALUTs for the same RTL.

Derived by `tools/budget/map_entity_attrib.py` from the Analysis &
Synthesis entity table. **Synthesis estimates, not a placement result:**
this design has never placed, so there are no ALM figures and no Fmax,
and nothing here should be quoted as either.

| | measured | against 5CSEBA6U23I7 |
|---|---:|---:|
| combinational ALUTs | 301446 | 360% of ~83820 |
| dedicated logic registers | 312114 | 186% of 167640 |
| block memory bits | 3207741 | 57% of 5662720 |
| DSP blocks | 128 | **114%** of 112 |

**The registers alone need at least 78028 ALM, 186% of the part, with the
combinational logic at zero.** Memory, by contrast, fits: 57%. That is
the whole shape of the problem in two numbers — storage held in flip-flops
is what overflows this device, and M10K is where the slack is.

## Biggest subtrees by combinational ALUTs

| entity | ALUTs | % of part | registers | mem bits | DSP |
|---|---:|---:|---:|---:|---:|
| `zhao_shell_top_v2:u_shell` | 58514 | 70% | 48422 | 571792 | 14 |
| `zhao_field_host_v2:u_field_host` | 43742 | 52% | 48612 | 85282 | 5 |
| `zhao_forge_assemble:u_forge_assemble` | 15970 | 19% | 39023 | 2048 | 1 |
| `zhao_proj_subsystem:u_proj_subsystem` | 11388 | 14% | 8098 | 105592 | 22 |
| `zhao_geom_proj_lane:u_geom_proj_lane` | 7553 | 9% | 4491 | 187308 | 0 |
| `zhao_terrain_devstore:u_terrain_devstore` | 7115 | 8% | 4418 | 0 | 0 |
| `zhao_geom_lodstate:u_geom_lodstate` | 6199 | 7% | 10825 | 0 | 4 |
| `zhao_terrain_heighttap:u_terrain_heighttap` | 5748 | 7% | 2370 | 0 | 4 |
| `zhao_light_stream:u_light_stream` | 5721 | 7% | 7070 | 5016 | 7 |
| `zhao_cmd_exec:u_cmd_exec` | 5317 | 6% | 12149 | 18472 | 0 |
| `zhao_part_collide:u_part_collide` | 5064 | 6% | 555 | 0 | 5 |
| `zhao_terrain_pagestream:u_terrain_pagestream` | 3804 | 5% | 2470 | 0 | 0 |
| `zhao_field_loader:u_field_loader` | 3803 | 5% | 2815 | 0 | 0 |
| `zhao_part_update:u_part_update` | 3266 | 4% | 687 | 0 | 0 |
| `zhao_terrain_pageio:u_terrain_pageio` | 3171 | 4% | 1357 | 26624 | 0 |
| `zhao_geom_cull:u_geom_cull` | 3061 | 4% | 1825 | 0 | 2 |

## Biggest subtrees by REGISTERS

A module with many registers and **no block memory bits** is holding an
array in flip-flops. That is the EARTHRAM lever and it is where the ALMs
are: one such array cost 5,181 registers and ~2,698 estimated ALMs, and
4,880 M10K bits bought all of it back at zero added cycles.

| entity | registers | % of part | ALUTs | mem bits |
|---|---:|---:|---:|---:|
| `zhao_field_host_v2:u_field_host` | 48612 | 29% | 43742 | 85282 |
| `zhao_shell_top_v2:u_shell` | 48422 | 29% | 58514 | 571792 |
| `zhao_forge_assemble:u_forge_assemble` | 39023 | 23% | 15970 | 2048 |
| `zhao_cmd_exec:u_cmd_exec` | 12149 | 7% | 5317 | 18472 |
| `zhao_geom_lodstate:u_geom_lodstate` | 10825 | 6% | 6199 | 0 |
| `zhao_proj_subsystem:u_proj_subsystem` | 8098 | 5% | 11388 | 105592 |
| `zhao_light_stream:u_light_stream` | 7070 | 4% | 5721 | 5016 |
| `zhao_geom_ladderbank:u_geom_ladderbank` | 5951 | 4% | 2554 | 0 |
| `zhao_material_resolve:u_material_resolve` | 5207 | 3% | 2443 | 896 |
| `zhao_geom_proj_lane:u_geom_proj_lane` | 4491 | 3% | 7553 | 187308 |
| `zhao_terrain_devstore:u_terrain_devstore` | 4418 | 3% | 7115 | 0 |
| `zhao_geom_pose_decode:u_geom_pose_decode` | 3897 | 2% | 1958 | 12288 |
| `zhao_geom_loom:u_geom_loom` | 3337 | 2% | 2652 | 408849 |
| `zhao_field_loader:u_field_loader` | 2815 | 2% | 3803 | 0 |
| `zhao_terrain_fieldlist:u_terrain_fieldlist` | 2800 | 2% | 1057 | 0 |
| `zhao_part_terrain_tap:u_part_terrain_tap` | 2783 | 2% | 2508 | 0 |

## Biggest subtrees by DSP

**The M10K trade does nothing for this column.** A block here is a
candidate for a quarter-square or coefficient-memory replacement, which
is a different programme from moving an array into a memory.

| entity | DSP | % of part | ALUTs |
|---|---:|---:|---:|
| `zhao_geom_attrpack:u_geom_attrpack` | 24 | 21% | 2209 |
| `zhao_proj_subsystem:u_proj_subsystem` | 22 | 20% | 11388 |
| `zhao_shell_top_v2:u_shell` | 14 | 12% | 58514 |
| `zhao_light_stream:u_light_stream` | 7 | 6% | 5721 |
| `zhao_geom_skin:u_geom_skin` | 6 | 5% | 2644 |
| `zhao_field_host_v2:u_field_host` | 5 | 4% | 43742 |
| `zhao_part_collide:u_part_collide` | 5 | 4% | 5064 |
| `zhao_forge_shadow:u_forge_shadow` | 4 | 4% | 550 |
| `zhao_geom_lodstate:u_geom_lodstate` | 4 | 4% | 6199 |
| `zhao_geom_setup:u_geom_setup` | 4 | 4% | 460 |
| `zhao_geom_skin_norm:u_geom_skin_norm` | 4 | 4% | 3035 |
| `zhao_terrain_heighttap:u_terrain_heighttap` | 4 | 4% | 5748 |
| `zhao_forge_prim_eval:u_forge_prim_eval` | 2 | 2% | 2654 |
| `zhao_forge_ring_eval:u_forge_ring_eval` | 2 | 2% | 2628 |
| `zhao_geom_clip:u_geom_clip` | 2 | 2% | 1000 |
| `zhao_geom_cull:u_geom_cull` | 2 | 2% | 3061 |

