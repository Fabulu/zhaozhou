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
| combinational ALUTs | 294872 | 352% of ~83820 |
| dedicated logic registers | 405872 | 242% of 167640 |
| block memory bits | 3009171 | 53% of 5662720 |
| DSP blocks | 375 | **335%** of 112 |

**The registers alone need at least 101468 ALM, 242% of the part, with the
combinational logic at zero.** Memory, by contrast, fits: 53%. That is
the whole shape of the problem in two numbers — storage held in flip-flops
is what overflows this device, and M10K is where the slack is.

## Biggest subtrees by combinational ALUTs

| entity | ALUTs | % of part | registers | mem bits | DSP |
|---|---:|---:|---:|---:|---:|
| `zhao_shell_top_v2:u_shell` | 46299 | 55% | 47125 | 571114 | 88 |
| `zhao_field_host_v2:u_field_host` | 40989 | 49% | 48614 | 85282 | 15 |
| `zhao_geom_drawjob:u_geom_drawjob` | 34031 | 41% | 100561 | 0 | 0 |
| `zhao_forge_assemble:u_forge_assemble` | 15524 | 19% | 39005 | 2048 | 3 |
| `zhao_proj_subsystem:u_proj_subsystem` | 8659 | 10% | 8107 | 105583 | 39 |
| `zhao_terrain_devstore:u_terrain_devstore` | 7945 | 9% | 4341 | 0 | 2 |
| `zhao_geom_proj_lane:u_geom_proj_lane` | 7560 | 9% | 4491 | 187308 | 2 |
| `zhao_geom_lodstate:u_geom_lodstate` | 6160 | 7% | 10801 | 0 | 6 |
| `zhao_terrain_heighttap:u_terrain_heighttap` | 5556 | 7% | 2305 | 0 | 6 |
| `zhao_cmd_exec:u_cmd_exec` | 5280 | 6% | 12101 | 18408 | 0 |
| `zhao_light_stream:u_light_stream` | 5278 | 6% | 7070 | 5016 | 9 |
| `zhao_terrain_pagestream:u_terrain_pagestream` | 3809 | 5% | 2470 | 0 | 0 |
| `zhao_field_loader:u_field_loader` | 3728 | 4% | 2815 | 0 | 0 |
| `zhao_terrain_pageio:u_terrain_pageio` | 3171 | 4% | 1357 | 26624 | 0 |
| `zhao_geom_vattr:u_geom_vattr` | 2620 | 3% | 2557 | 41720 | 4 |
| `zhao_geom_ladderbank:u_geom_ladderbank` | 2580 | 3% | 5951 | 0 | 0 |
| `zhao_forge_ring_eval:u_forge_ring_eval` | 2457 | 3% | 2013 | 4369 | 3 |
| `zhao_material_resolve:u_material_resolve` | 2437 | 3% | 5207 | 896 | 0 |
| `zhao_forge_prim_eval:u_forge_prim_eval` | 2412 | 3% | 2415 | 4608 | 3 |
| `zhao_terrain_lightlane:u_terrain_lightlane` | 2401 | 3% | 2180 | 50080 | 6 |
| `zhao_part_update:u_part_update` | 2208 | 3% | 687 | 0 | 6 |
| `zhao_geom_loom:u_geom_loom` | 2207 | 3% | 3337 | 408849 | 3 |
| `zhao_terrain_residency_v2:u_terrain_residency` | 2173 | 3% | 1029 | 150528 | 0 |
| `zhao_geom_skin:u_geom_skin` | 2116 | 3% | 2146 | 0 | 9 |

## Biggest subtrees by REGISTERS

A module with many registers and **no block memory bits** is holding an
array in flip-flops. That is the EARTHRAM lever and it is where the ALMs
are: one such array cost 5,181 registers and ~2,698 estimated ALMs, and
4,880 M10K bits bought all of it back at zero added cycles.

| entity | registers | % of part | ALUTs | mem bits |
|---|---:|---:|---:|---:|
| `zhao_geom_drawjob:u_geom_drawjob` | 100561 | 60% | 34031 | 0 |
| `zhao_field_host_v2:u_field_host` | 48614 | 29% | 40989 | 85282 |
| `zhao_shell_top_v2:u_shell` | 47125 | 28% | 46299 | 571114 |
| `zhao_forge_assemble:u_forge_assemble` | 39005 | 23% | 15524 | 2048 |
| `zhao_cmd_exec:u_cmd_exec` | 12101 | 7% | 5280 | 18408 |
| `zhao_geom_lodstate:u_geom_lodstate` | 10801 | 6% | 6160 | 0 |
| `zhao_proj_subsystem:u_proj_subsystem` | 8107 | 5% | 8659 | 105583 |
| `zhao_light_stream:u_light_stream` | 7070 | 4% | 5278 | 5016 |
| `zhao_geom_ladderbank:u_geom_ladderbank` | 5951 | 4% | 2580 | 0 |
| `zhao_material_resolve:u_material_resolve` | 5207 | 3% | 2437 | 896 |
| `zhao_geom_proj_lane:u_geom_proj_lane` | 4491 | 3% | 7560 | 187308 |
| `zhao_terrain_devstore:u_terrain_devstore` | 4341 | 3% | 7945 | 0 |
| `zhao_geom_pose_decode:u_geom_pose_decode` | 3897 | 2% | 1376 | 12288 |
| `zhao_geom_loom:u_geom_loom` | 3337 | 2% | 2207 | 408849 |
| `zhao_field_loader:u_field_loader` | 2815 | 2% | 3728 | 0 |
| `zhao_terrain_fieldlist:u_terrain_fieldlist` | 2800 | 2% | 1056 | 0 |
| `zhao_geom_clipread:u_geom_clipread` | 2680 | 2% | 2046 | 0 |
| `zhao_geom_vattr:u_geom_vattr` | 2557 | 2% | 2620 | 41720 |
| `zhao_terrain_pagestream:u_terrain_pagestream` | 2470 | 1% | 3809 | 0 |
| `zhao_forge_prim_eval:u_forge_prim_eval` | 2415 | 1% | 2412 | 4608 |
| `zhao_part_terrain_tap:u_part_terrain_tap` | 2316 | 1% | 1880 | 0 |
| `zhao_terrain_heighttap:u_terrain_heighttap` | 2305 | 1% | 5556 | 0 |
| `zhao_terrain_lightlane:u_terrain_lightlane` | 2180 | 1% | 2401 | 50080 |
| `zhao_terrain_writeback:u_terrain_writeback` | 2180 | 1% | 1034 | 0 |

## Biggest subtrees by DSP

**The M10K trade does nothing for this column.** A block here is a
candidate for a quarter-square or coefficient-memory replacement, which
is a different programme from moving an array into a memory.

| entity | DSP | % of part | ALUTs |
|---|---:|---:|---:|
| `zhao_shell_top_v2:u_shell` | 88 | 79% | 46299 |
| `zhao_geom_attrpack:u_geom_attrpack` | 45 | 40% | 1184 |
| `zhao_proj_subsystem:u_proj_subsystem` | 39 | 35% | 8659 |
| `zhao_geom_skin_norm:u_geom_skin_norm` | 21 | 19% | 1192 |
| `zhao_part_collide:u_part_collide` | 20 | 18% | 1607 |
| `zhao_field_host_v2:u_field_host` | 15 | 13% | 40989 |
| `zhao_post_composite:u_post_composite` | 12 | 11% | 1147 |
| `zhao_geom_skin:u_geom_skin` | 9 | 8% | 2116 |
| `zhao_light_stream:u_light_stream` | 9 | 8% | 5278 |
| `zhao_twod_plane:u_twod_plane` | 8 | 7% | 892 |
| `zhao_forge_shadow:u_forge_shadow` | 7 | 6% | 508 |
| `zhao_geom_cull:u_geom_cull` | 6 | 5% | 1534 |
| `zhao_geom_lodstate:u_geom_lodstate` | 6 | 5% | 6160 |
| `zhao_geom_meshfetch:u_geom_meshfetch` | 6 | 5% | 1040 |
| `zhao_part_update:u_part_update` | 6 | 5% | 2208 |
| `zhao_terrain_heighttap:u_terrain_heighttap` | 6 | 5% | 5556 |
| `zhao_terrain_lightlane:u_terrain_lightlane` | 6 | 5% | 2401 |
| `zhao_part_project:u_part_project` | 5 | 4% | 878 |
| `zhao_geom_pose_decode:u_geom_pose_decode` | 4 | 4% | 1376 |
| `zhao_geom_setup:u_geom_setup` | 4 | 4% | 466 |
| `zhao_geom_vattr:u_geom_vattr` | 4 | 4% | 2620 |
| `zhao_post_gather_tag:u_post_gather_tag` | 4 | 4% | 281 |
| `zhao_terrain_tess:u_terrain_tess` | 4 | 4% | 1908 |
| `zhao_forge_assemble:u_forge_assemble` | 3 | 3% | 15524 |

## Inside `zhao_geom_drawjob:u_geom_drawjob`

Subtree: 34031 ALUTs, 100561 registers. **Held by the node itself, not by any
child: 34031 ALUTs, 100561 registers.**

It has no hierarchy under it at all — every one of those registers
is in this one module.

## Inside `zhao_shell_top_v2:u_shell`

Subtree: 46299 ALUTs, 47125 registers. **Held by the node itself, not by any
child: 6087 ALUTs, 4920 registers.**

| child | ALUTs | registers | mem bits |
|---|---:|---:|---:|
| `zhao_geom_bin_pipe_v2:u_render_bin` | 30266 | 31736 | 446258 |
| `zhao_cmd_dma:u_dma` | 1846 | 1141 | 32768 |
| `zhao_post_lease:u_post_lease` | 1260 | 1344 | 9984 |
| `zhao_input_snapshot:u_snapshot` | 894 | 1033 | 0 |
| `zhao_vram_arbiter:u_arb` | 798 | 666 | 0 |
| `zhao_hps_arbiter_n:u_hps_arb` | 717 | 238 | 0 |
| `zhao_debug_frameblit:u_frameblit` | 681 | 871 | 0 |
| `zhao_cmd_scheduler:u_sched` | 536 | 720 | 0 |
| `zhao_video_slotmgr_v2:u_slotmgr` | 389 | 395 | 0 |
| `zhao_video_scanout:u_scanout` | 310 | 163 | 16384 |
| `zhao_debug_counters:u_counters` | 276 | 509 | 0 |
| `zhao_video_framectl:u_framectl` | 251 | 236 | 0 |
| `zhao_raster_fbwrite:u_render_fbw` | 248 | 706 | 0 |
| `zhao_sdram_ctrl:u_ctrl` | 221 | 249 | 0 |
| `zhao_debug_crc:u_crc` | 213 | 134 | 0 |
| `zhao_audio_fifo:u_fifo` | 187 | 268 | 65536 |
| `zhao_hps_bridge:u_bridge` | 179 | 654 | 0 |
| `zhao_input_rumble:u_rumble` | 152 | 224 | 0 |
| `zhao_mem_guard:u_guard_build` | 151 | 63 | 0 |
| `zhao_mem_guard:u_guard_render` | 134 | 68 | 0 |
| `zhao_mem_guard:u_guard_geom` | 107 | 65 | 0 |
| `zhao_mem_guard:u_guard_blit` | 90 | 69 | 0 |
| `zhao_video_blit_lease_v2:u_blit_lease` | 82 | 89 | 0 |
| `zhao_mem_guard:u_guard_scan` | 63 | 57 | 0 |
| `zhao_fb_ready_cdc_v2:u_fb_cdc` | 46 | 144 | 184 |
| `zhao_video_mode:u_mode` | 38 | 36 | 0 |
| `zhao_video_ready_bridge_v2:u_ready_bridge` | 35 | 84 | 0 |
| `zhao_renderer_lease_v2:u_render_lease` | 22 | 8 | 0 |
| `zhao_video_scaler:u_scaler` | 12 | 78 | 0 |
| `zhao_cdc_snapshot:u_starve_mbx` | 4 | 138 | 0 |
| `zhao_video_terminal_adapter_v2:u_term_adapter` | 4 | 19 | 0 |

## Inside `zhao_field_host_v2:u_field_host`

Subtree: 40989 ALUTs, 48614 registers. **Held by the node itself, not by any
child: 9697 ALUTs, 5115 registers.**

| child | ALUTs | registers | mem bits |
|---|---:|---:|---:|
| `zhao_field_v3_engine:u_fabric` | 30056 | 42660 | 85282 |
| `zhao_field_progcache:u_progcache` | 1236 | 839 | 0 |

## Inside `zhao_forge_assemble:u_forge_assemble`

Subtree: 15524 ALUTs, 39005 registers. **Held by the node itself, not by any
child: 14103 ALUTs, 37644 registers.**

| child | ALUTs | registers | mem bits |
|---|---:|---:|---:|
| `zhao_raster_rcp24_v4:u_rcp` | 845 | 977 | 1408 |
| `zhao_geom_depthquant_stream:u_dq` | 576 | 384 | 0 |

