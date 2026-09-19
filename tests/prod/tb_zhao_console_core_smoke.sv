// tb_zhao_console_core_smoke.sv -- THE WIRING SMOKE BENCH for
// `zhao_console_core`.
//
// WHAT IT IS FOR, and equally what it is NOT for.
//
// It answers one question: does the connected machine come out of reset and do
// the newly adopted organs actually carry traffic, or are they instantiated
// scenery? Every check below is a PATH check -- a count that can only move if a
// beat crossed a wire between two blocks. None of them is a correctness check:
// the particle arithmetic, the projection numbers and the compositor's output
// all have their own directed tests and this bench deliberately re-verifies
// none of them.
//
// THE CHECKS, AND THE WIRE EACH ONE PROVES:
//
//   1. `gpu_tick_o` pulses                  -- the shell leaves reset and
//                                              FRAMECTL reaches a frame edge.
//   2. `part_tick_busy_o` rises after it    -- SHELL.gpu_tick -> PART.STATE.
//   3. `part_wr_valid_o` fires              -- a record went
//                                              STATE -> UPDATE -> COLLIDE ->
//                                              STATE -> out. Four blocks, one
//                                              beat, no stimulus in between.
//   4. `geom_vertices_sent_o` moves         -- GEOM.SKIN -> GROUP_SEQ ->
//                                              client A of the projector.
//   5. `proj_a_grants_o` moves              -- the shared projector GRANTED
//                                              that client: the wire the
//                                              manifest says had no producer.
//   6. `geom_landings_o` moves              -- results came back through
//                                              GEOM.PROJ_LANE into the
//                                              sequencer. THE LOOP CLOSES.
//   7. `hist_snapshots_o` moves             -- SHELL.gpu_tick -> MEASURE.
//   8. `terr_tess_vertices_o` moves         -- TERRAIN.GROUP_SEQ ->
//                                              TERRAIN.TESS and back.
//   9. `terr_fills_forwarded_o` moves       -- TERRAIN.GROUP_SEQ -> client B.
//  10. `proj_b_grants_o` moves              -- the shared projector's ARBITER
//                                              granted its SECOND client.
//                                              Before the terrain sequencer
//                                              was composed this counter could
//                                              not move at all, so the whole
//                                              reason the projector is SHARED
//                                              was an argument and not a
//                                              measurement.
//  11. `terr_groups_opened_o` moves         -- the open/gen handshake with the
//                                              subsystem's terrain arena.
//  12. `terr_release_unsafe_o` STAYS ZERO   -- the sequencer's own safety
//                                              detector, which has a committed
//                                              mutant proving it can fire.
//
// WHAT IT CANNOT SHOW, said here rather than left as a silence: nothing flows
// through POST.COMPOSITE, because its source pixels have no producer in the
// core (entry I15 of the DUT's header). A bench that fabricated them would be
// supplying exactly what the completion plan forbids a harness to supply.
//
// The harness supplies clocks, reset, the particle generation store (memory)
// and the geometry vertex stream (host content). It supplies no lighting, no
// FIELD result, no particle update and no prepared triangle.

// No `timescale directive here: the RTL closure carries none, and adding one
// makes Verilator report TIMESCALEMOD against every module in it. The unit
// comes from --timescale on the command line instead.

module tb_zhao_console_core_smoke
  import zhao_pkg::*, zhao_abi_pkg::*, zhao_fb_tuple_pkg::*;
();

  // The DUT's parameter defaults, mirrored so the declarations below can use
  // them. Overriding one here without overriding it on the instance is a width
  // mismatch the compiler catches, which is why they are not magic numbers.
  // TWOD widths, mirrored from zhao_console_core's own parameter defaults so
  // the `.*` binding can size its ports. DERIVED, not typed: each is the same
  // $clog2 expression the core uses, so a change to a page or slot count there
  // cannot leave a hand-written width here quietly disagreeing.
  localparam int unsigned TWOD_PAGE_WORDS = 8192;
  localparam int unsigned TWOD_PAL_SLOTS  = 4;
  localparam int unsigned TWOD_BIND_SLOTS = 8;
  localparam int unsigned TWOD_PAW        = $clog2(TWOD_PAGE_WORDS);
  localparam int unsigned TWOD_PALAW      = $clog2(TWOD_PAL_SLOTS * 256);
  localparam int unsigned TWOD_BSW        = $clog2(TWOD_BIND_SLOTS);

  localparam int unsigned PART_REC_W     = 128;
  localparam int unsigned PART_AGE_W     = 10;
  localparam int unsigned PART_POS_W     = 18;
  localparam int unsigned PART_NRM_W     = 12;
  localparam int unsigned PART_FX_W      = 16;
  localparam int unsigned GEOM_ARENAS    = 4;
  localparam int unsigned GEOM_DEPTH     = 1089;
  localparam int unsigned GEOM_NVIEWS    = 2;
  localparam int unsigned GEOM_GEN_W     = 8;
  localparam int unsigned GEOM_PAYLOAD_W = 106;
  localparam int unsigned GEOM_INDEX_W   = $clog2(GEOM_DEPTH) + 1;
  localparam int unsigned GEOM_ARENA_W   = $clog2(GEOM_ARENAS) + 1;
  localparam int unsigned GEOM_ASM_VIDW  = 16;
  localparam int unsigned PROJ_T_ARENAS  = 4;
  localparam int unsigned PROJ_T_DEPTH   = 81;
  localparam int unsigned PROJ_T_INDEX_W = $clog2(PROJ_T_DEPTH) + 1;
  localparam int unsigned PROJ_T_ARENA_W = $clog2(PROJ_T_ARENAS) + 1;
  localparam int unsigned POST_LINE_W    = 384;
  localparam int unsigned POST_MAX_H     = 240;
  localparam int unsigned POST_XW        = $clog2(POST_LINE_W + 1);
  localparam int unsigned POST_YW        = $clog2(POST_MAX_H + 1);
  localparam int unsigned HIST_EW        = 32;
  localparam int unsigned HIST_SUB_BITS  = 1;
  localparam int unsigned HIST_LANES     = 4;
  localparam int unsigned HIST_CW        = 24;
  localparam int unsigned HIST_BINW      = $clog2((HIST_EW - HIST_SUB_BITS + 1) << HIST_SUB_BITS);
  // SURFACE.SHEET's resident slot count, mirrored so `surf_res_occupancy_o`
  // can be declared. Overriding SURF_SLOTS on the DUT without changing this
  // is a width mismatch the compiler catches, which is the point.
  localparam int unsigned SURF_SLOTS     = 2;

  localparam int unsigned N_PART_RECORDS = 6;

  // ---- the surface the particles are driven into (see the acceptance below) --
  // Every record this bench offers sits at y = 0, so a surface at +100
  // position LSBs puts all six INSIDE it and every one of them makes a
  // contact. From 2026-09-19 that surface is the PLANE (entry I7, still a
  // boundary input) and not a hand-driven terrain sample: I6 closed and the
  // terrain sample is produced INSIDE the core now, from a compose cache this
  // bench never fills (its pages fault by design -- see the TERRAIN spine
  // note), so the terrain answers "no ground" and the plane is what the
  // particles land on. The terrain path is asserted separately, below. `PART_CLEAR_EPS` is `zhao_part_collide`'s own default; it is
  // written here rather than read, so a change to that parameter fails this
  // bench loudly instead of silently agreeing with itself.
  localparam int signed   PART_TER_H     = 100;
  localparam int unsigned PART_CLEAR_EPS = 2;
  // THE GEOMETRY FIXTURE AND EVERY NUMBER ASSERTED ABOUT IT come from ONE
  // generated header, written by `tests/prod/smoke_geom_fixture_gen.cpp` from
  // the REFERENCE (project_vertex -> Clip -> Setup -> Binner) and held fresh by
  // the ctest `smoke_geom_fixture_fresh`. Nothing below re-derives a count.
`include "smoke_geom_fixture.svh"
  localparam int unsigned N_GEOM_VERTS   = SGF_N_VERTS;
  // Whole 64-byte lines of eight beats: the index run starts line-aligned
  // (GEOM_IX_OFF = 0x80) and so does the vertex run (0x100), 32 B per vertex.
  localparam int unsigned GEOM_FOOTPRINT_BEATS =
      8 * (((3 * SGF_N_TRIS) + 63) / 64) + 8 * (((32 * SGF_N_VERTS) + 63) / 64);
  localparam int unsigned CYCLE_LIMIT    = 1_500_000;
  localparam logic signed [31:0] FX16_ONE = 32'sh0001_0000;

  // ==========================================================================
  // The DUT's ports, declared from its own port list. `.*` then binds them by
  // name, so a port this bench forgets is a compile error rather than a
  // silently floating input.
  // ==========================================================================
  logic                    part_rd_valid_i;
  logic                    part_rd_ready_o;
  logic [PART_REC_W-1:0]   part_rd_record_i;
  logic                    part_rd_last_i;
  logic                    part_wr_valid_o;
  logic                    part_wr_ready_i;
  logic [PART_REC_W-1:0]   part_wr_record_o;
  // PART.TABLE's per-frame load (core entry I33). THE TWENTY-FIVE DESCRIPTOR
  // PORTS THAT USED TO BE DECLARED HERE ARE GONE: the core instantiates
  // `zhao_part_table` and answers its own descriptor reads (entries I2 and I3,
  // closed 2026-09-19). This bench now has to LOAD the table to get the same
  // stimulus it used to drive straight onto the consumers' inputs -- which is
  // the point, because a descriptor that reaches PART.COLLIDE now has to have
  // travelled through the table to get there.
  localparam int unsigned PART_TBL_LD_W = 12 + (2 * PART_AGE_W) + (5 * 11) + (3 * 18);
  logic                    part_tbl_ld_valid_i;
  logic                    part_tbl_ld_ready_o;
  logic [1:0]              part_tbl_ld_sel_i;
  logic [6:0]              part_tbl_ld_index_i;
  logic [1:0]              part_tbl_ld_event_i;
  logic [PART_TBL_LD_W-1:0] part_tbl_ld_data_i;
  logic [31:0]             part_tbl_loads_update_o;
  logic [31:0]             part_tbl_loads_collide_o;
  logic [31:0]             part_tbl_loads_spawn_o;
  logic [31:0]             part_tbl_loads_curve_o;
  logic [31:0]             part_tbl_load_refused_o;
  logic                    part_fld_valid_i;
  logic signed [10:0]      part_fld_ax_i;
  logic signed [10:0]      part_fld_ay_i;
  logic signed [10:0]      part_fld_az_i;
  // I6 CLOSED: the terrain sample is the core's own now. Its census and the
  // population origin (I7, widened) take the five retired inputs' place.
  logic [31:0]             part_ter_particles_o;
  logic [31:0]             part_ter_ground_o;
  logic [31:0]             part_ter_no_ground_o;
  logic [31:0]             part_ter_missed_o;
  logic [31:0]             part_ter_faults_o;
  logic [31:0]             part_ter_fills_landed_o;
  logic [31:0]             terr_tap_answered_o;
  logic [31:0]             terr_tap_off_patch_o;
  logic [31:0]             terr_tap_faults_o;
  logic signed [31:0]      part_pop_origin_x_i;
  logic signed [31:0]      part_pop_origin_y_i;
  logic signed [31:0]      part_pop_origin_z_i;
  logic                    part_plane_en_i;
  logic signed [PART_NRM_W-1:0] part_plane_nx_i;
  logic signed [PART_NRM_W-1:0] part_plane_ny_i;
  logic signed [PART_NRM_W-1:0] part_plane_nz_i;
  logic signed [31:0]      part_plane_c_i;
  logic                    part_cap_full_i;
  logic                    part_tick_busy_o;
  logic                    part_tick_done_o;
  logic [31:0]             part_survivors_o;
  logic [31:0]             part_children_written_o;
  logic [31:0]             part_children_dropped_capacity_o;
  logic [31:0]             part_staging_stall_cycles_o;
  logic [31:0]             part_species_refused_o;
  logic [31:0]             part_updated_o;
  logic [31:0]             part_upd_refused_o;
  logic [31:0]             part_died_by_age_o;
  logic                    part_upd_beat_refused_o;
  logic [31:0]             part_velocity_saturations_o;
  logic [31:0]             part_position_saturations_o;
  logic [31:0]             part_collisions_applied_o;
  logic [3:0]              part_hist_sel_i;
  logic [31:0]             part_hist_val_o;
  logic                    part_colour_en_o;
  logic [7:0]              part_colour_o;
  logic                    part_contact_o;
  logic [2:0]              part_response_o;
  logic                    part_col_refused_o;
  logic [31:0]             part_contacts_ignore_o;
  logic [31:0]             part_contacts_die_o;
  logic [31:0]             part_contacts_stick_o;
  logic [31:0]             part_contacts_slide_o;
  logic [31:0]             part_contacts_bounce_o;
  logic [31:0]             part_contacts_terrain_o;
  logic [31:0]             part_contacts_plane_o;
  logic [31:0]             part_already_inside_at_entry_o;
  logic [31:0]             part_terrain_sample_unavailable_o;
  logic [31:0]             part_response_refused_o;
  logic [31:0]             part_field_clamps_o;
  logic [31:0]             part_children_requested_o;
  logic [31:0]             part_children_emitted_o;
  logic [31:0]             part_children_refused_o;
  logic [31:0]             part_spawn_by_event0_o;
  logic [31:0]             part_spawn_by_event1_o;
  logic [31:0]             part_spawn_by_event2_o;
  logic [31:0]             part_spawn_by_event3_o;
  logic [31:0]             part_refused_count_gt_max_o;
  logic [31:0]             part_refused_unknown_species_o;
  logic [31:0]             part_refused_capacity_o;
  logic [31:0]             part_max_children_in_tick_o;
  // ---- THE GEOMETRY ASSET PATH (core connected item 11) -------------------
  // `geom_vd_v_*` IS GONE from the core's port list: GEOM.ASSETFETCH drives
  // GEOM.VDECODE inside the module now. These fifty-two nets are what the
  // path still asks of a harness, and every width below was generated from
  // the core's own port list rather than typed -- a width that disagrees
  // binds silently in one tool and loudly in another, and either way the
  // bench is then measuring a different machine.
  logic                    geom_mf_job_valid_i;
  logic                    geom_mf_job_ready_o;
  logic [15:0]             geom_mf_job_instance_id_i;
  logic [26:0]             geom_mf_job_desc_addr_i;
  logic [7:0]              geom_mf_job_format_i;
  logic [15:0]             geom_mf_job_generation_i;
  logic [1:0]              geom_mf_job_active_mask_i;
  logic signed [31:0]      geom_mf_job_xform_i [0:11];
  // I37 CLOSED: the CRC walker is inside the core; its evidence comes out.
  logic [31:0]             geom_mf_crc_descriptors_o;
  logic [31:0]             geom_mf_crc_fail_o;
  logic [31:0]             geom_mf_crc_framing_o;
  // I38 and I11 CLOSED, I39 narrowed to the raster word: GEOM.REPLAY owns the
  // release, the handle and the TriangleDescriptor inside the core now.
  logic [31:0]              geom_asm_raster_state_i;
  logic [31:0] geom_mf_meshlets_considered_o;
  logic [31:0] geom_mf_culled_all_cameras_o;
  logic [31:0] geom_mf_descriptors_fetched_o;
  logic [31:0] geom_mf_guard_denied_o;
  logic [31:0] geom_mf_refused_format_o;
  logic [31:0] geom_mf_refused_crc_o;
  logic [31:0] geom_mf_refused_generation_o;
  logic [31:0] geom_mf_refused_vertex_count_o;
  logic [31:0] geom_mf_refused_triangle_count_o;
  logic [31:0] geom_mf_refused_reserved_o;
  logic [31:0] geom_mf_refused_zero_bound_o;
  logic [31:0] geom_ma_jobs_a_o;
  logic [31:0] geom_ma_jobs_b_o;
  logic [31:0] geom_ma_denied_o;
  logic [31:0] geom_ma_contention_o;
  logic [31:0] geom_ma_err_short_o;
  logic [31:0] geom_ma_err_long_o;
  logic [31:0] geom_ma_err_unowned_o;
  logic [31:0] geom_af_meshlets_fetched_o;
  logic [31:0] geom_af_beats_read_o;
  logic [31:0] geom_af_guard_denied_o;
  logic [31:0] geom_af_refused_footprint_o;
  logic [31:0] geom_af_prefetch_stall_o;
  logic [31:0] geom_af_err_beat_truncated_o;
  logic [31:0] geom_af_err_beat_overrun_o;
  logic [31:0] geom_af_err_beat_unowned_o;
  logic [31:0] geom_asm_meshlets_o;
  logic [31:0] geom_asm_triangles_o;
  logic [31:0] geom_asm_refused_limits_o;
  logic [31:0] geom_asm_refused_index_o;
  logic signed [7:0]       geom_vd_d_nx_o, geom_vd_d_ny_o, geom_vd_d_nz_o;
  logic signed [15:0]      geom_vd_d_u_o, geom_vd_d_v_o;
  logic                    geom_vd_refused_o, geom_vd_reserved_nz_o;
  logic                    geom_vd_w0_illegal_o, geom_vd_format_bad_o;
  logic [31:0]             geom_vd_vertices_o;
  logic [31:0]             geom_vd_reserved_nz_count_o;
  logic [31:0]             geom_vd_w0_illegal_count_o;
  logic [31:0]             geom_vd_format_bad_count_o;
  logic [15:0]             geom_skin_src_id_o;
  logic [31:0]             geom_skin_vertices_transformed_o;

  // ---- GEOM.SKIN.NORM, composed 2026-09-19 (core entry I43) ----------------
  // `geom_sn_n_ready_i` is driven HIGH below and that is a real decision, not a
  // convenience: the block emits into a boundary and a bench that held the
  // consumer's ready low would stall the AND-fork on GEOM.POSE's palette store
  // and throttle GEOM.SKIN with it, which would make every skinned-vertex count
  // in this file a measurement of the bench's own backpressure.
  logic                    geom_sn_n_valid_o;
  logic                    geom_sn_n_ready_i;
  logic signed [63:0]      geom_sn_n_x_o, geom_sn_n_y_o, geom_sn_n_z_o;
  logic [63:0]             geom_sn_n_mag_o;
  logic                    geom_sn_n_degenerate_o;
  logic [15:0]             geom_sn_n_src_id_o;
  logic [31:0]             geom_sn_vertices_o;
  logic [31:0]             geom_sn_degenerate_o;
  logic [31:0]             geom_sn_reduced_o;
  logic [31:0]             geom_sn_fork_stall_o;

  // The hand-computed world normal the fixture must produce. A NAMED constant
  // rather than a literal in the check, because it is derived from
  // `vdec_record`'s normal byte and w0 and must move if either does: it is
  // w0 * ONE_FX16 * nx = 64 * 65536 * 127. The derivation is written out at
  // the assertion.
  localparam logic signed [63:0] SN_EXPECT_NX = 64'sd532676608;

  // ---- I29: GEOM.POSE's clip page and skeleton bake ------------------------
  // GEOM.SKIN's two matrix arrays USED TO BE HERE and the bench drove them with
  // an identity. They are gone because zhao_geom_pose_palette is composed in
  // the core and drives them from a real store -- which is what closing entry
  // I10 means. What this bench can reach is one level up: the DECODER's source.
  logic                    geom_pose_start_i;
  logic [5:0]              geom_pose_bone_count_i;
  logic signed [31:0]      geom_pose_root_dx_i;
  logic signed [31:0]      geom_pose_root_dy_i;
  logic signed [31:0]      geom_pose_root_dz_i;
  logic [4:0]              geom_pose_bone_idx_o;
  logic [4:0]              geom_pose_bone_parent_i;
  logic signed [31:0]      geom_pose_bone_tx_i;
  logic signed [31:0]      geom_pose_bone_ty_i;
  logic signed [31:0]      geom_pose_bone_tz_i;
  logic signed [15:0]      geom_pose_quat_w_i;
  logic signed [15:0]      geom_pose_quat_x_i;
  logic signed [15:0]      geom_pose_quat_y_i;
  logic signed [15:0]      geom_pose_quat_z_i;
  logic signed [31:0]      geom_pose_inv_rest_i [0:11];
  logic                    geom_pose_busy_o;
  logic                    geom_pose_done_o;
  logic [31:0]             geom_pose_palettes_decoded_o;
  logic [31:0]             geom_pal_vertices_served_o;
  logic [31:0]             geom_pal_bones_written_o;
  logic [31:0]             geom_pal_bone_oob_o;
  logic [31:0]             geom_pal_bone_unset_o;
  logic                    geom_org_we_i;
  logic [GEOM_ARENA_W-1:0] geom_org_arena_i;
  logic signed [31:0]      geom_org_x_i;
  logic signed [31:0]      geom_org_y_i;
  logic signed [31:0]      geom_org_z_i;
  logic signed [31:0]      geom_rep_org_x_o;
  logic signed [31:0]      geom_rep_org_y_o;
  logic signed [31:0]      geom_rep_org_z_o;
  logic [31:0]             geom_groups_opened_o;
  logic [31:0]             geom_groups_sealed_o;
  logic [31:0]             geom_vertices_sent_o;
  logic [31:0]             geom_landings_o;
  logic [31:0]             geom_jobs_refused_o;
  logic [31:0]             geom_alloc_stall_cycles_o;
  logic [31:0]             geom_rel_unheld_o;
  logic                    geom_seal_early_o;
  logic [31:0]             geom_arena_hits_o;
  logic [31:0]             geom_arena_misses_o;
  logic [31:0]             geom_arena_refusals_o;
  logic                    geom_arena_overflow_o;
  logic                    proj_cfg_we_i;
  logic                    proj_cfg_view_i;
  logic [4:0]              proj_cfg_addr_i;
  logic [31:0]             proj_cfg_data_i;
  logic                    proj_en_i;
  // Client B, its arena lifetime and its reference port are NO LONGER PORTS:
  // TERRAIN.GROUP_SEQ and TERRAIN.TESS drive them inside the DUT. What the
  // bench declares instead is the terrain producer's OWN boundary.
  logic                    terr_job_valid_i;
  logic                    terr_job_ready_o;
  logic [5:0]              terr_job_ox_i;
  logic [5:0]              terr_job_oz_i;
  logic [1:0]              terr_job_level_i;
  logic [1:0]              terr_job_lvl_nz_i;
  logic [1:0]              terr_job_lvl_pz_i;
  logic [1:0]              terr_job_lvl_nx_i;
  logic [1:0]              terr_job_lvl_px_i;
  logic [16:0]             terr_job_morph_i;
  logic                    terr_job_surface_i;
  logic                    terr_job_dual_i;
  logic [15:0]             terr_job_src_id_i;
  logic [1:0]              terr_job_view_mask_i;
  logic [7:0]              terr_job_mat_a_i;
  logic [7:0]              terr_job_mat_b_i;
  logic [7:0]              terr_job_weight_i;
  logic                    terr_sparse_fill_i;
  // TERRAIN.TESS's lattice and cell-state ports are GONE from the DUT, and the
  // flat-lattice memory model this bench used to play with them went with them
  // (core header entry I22, closed 2026-09-19). `zhao_terrain_compcache_front`
  // is composed inside the core now and serves those reads for real.

  // ---- PACKET P-TERRAIN: the paging spine's boundary ----------------------
  // TERRAIN.CMD's command is a HOST PACKET (T5's SubmitTerrainSet from
  // SW.STREAM), and the HPS/guard ports are MEMORY BEHAVIOUR. Both are in the
  // completion plan's own list of what a harness may supply, which is why this
  // bench may drive them and may not drive, say, a composed patch_state.
  localparam int unsigned TERR_SLOTW_C   = 10;   // $clog2(256 sets x 4 ways)
  localparam int unsigned TERR_GENW_C    = 8;
  localparam int unsigned TERR_CSLOTW_C  = 8;    // $clog2(256 compose slots)

  logic                    terr_cmd_valid_i;
  logic                    terr_cmd_ready_o;
  logic [31:0]             terr_cmd_epoch_i;
  logic [31:0]             terr_cmd_list_off_i;
  logic [31:0]             terr_cmd_list_bytes_i;
  logic [31:0]             terr_cmd_list_crc_i;
  logic [15:0]             terr_cmd_patch_count_i;
  logic [31:0]             terr_cmd_sequence_i;
  logic [31:0]             terr_cmd_src_id_i;
  logic                    terr_cmd_done_valid_o;
  logic                    terr_cmd_done_ready_i;
  logic                    terr_cmd_done_ok_o;
  logic [3:0]              terr_cmd_done_verdict_o;
  logic [31:0]             terr_cmd_done_src_id_o;
  logic [31:0]             terr_cmd_done_crc_seen_o;
  logic [31:0]             terr_cfg_epoch_i;
  logic [31:0]             terr_cfg_arena_base_i;
  logic [31:0]             terr_cfg_arena_bytes_i;
  logic [15:0]             terr_cfg_load_budget_i;

  zhao_hps_burst_req_t     terr_hps_req_o;
  logic                    terr_hps_grant_i;
  logic                    terr_hps_wr_valid_o;
  logic [63:0]             terr_hps_wr_data_o;
  logic                    terr_hps_wr_last_o;
  zhao_hps_burst_rsp_t     terr_hps_rsp_i;

  zhao_guard_req_t         terr_guard_req_o;
  zhao_guard_rsp_t         terr_guard_rsp_i;
  logic [63:0]             terr_guard_wdata_o;
  logic                    terr_guard_wvalid_o;
  logic                    terr_guard_wready_i;
  logic                    terr_guard_wlast_o;

  // ---- THE TERRAIN COMPOSE ENGINE's boundary (core header item 10) --------
  // The compose DOOR and the UNPIN are gone from the DUT: TERRAIN.SEQ's issue
  // reaches TERRAIN.PAGESTREAM and TERRAIN.PLACE inside the core, and the
  // streamer's completion unpins the page. What is left here is the engine's
  // own edge, and every one of these is a thing the completion plan lets a
  // harness supply -- memory behaviour, or a field result this console has no
  // producer for and must therefore NOT invent.
  zhao_guard_req_t         terr_ps_guard_req_o;
  zhao_guard_rsp_t         terr_ps_guard_rsp_i;
  logic                    terr_ps_beat_valid_i;
  logic [63:0]             terr_ps_beat_data_i;
  logic                    terr_ps_beat_last_i;

  // TERRAIN.HDRREAD and the compose path's guard read share, composed item 13.
  // These replace `terr_place_pitch_log2_i` / `terr_place_env_x0_i` /
  // `terr_place_env_z0_i`, which were entry I35's boundary and are gone: the
  // pitch and the envelope come off the page's own header inside the core now,
  // so there is nothing for a bench to drive and five more things to read.
  logic [31:0]             terr_hr_headers_o;
  logic [31:0]             terr_hr_refused_o;
  logic [31:0]             terr_hr_guard_denied_o;
  logic [31:0]             terr_hr_incomplete_o;
  logic [31:0]             terr_hr_ident_fails_o;
  logic                    terr_hr_idle_o;
  logic [31:0]             terr_rdshare_jobs_a_o;
  logic [31:0]             terr_rdshare_jobs_b_o;
  logic [31:0]             terr_rdshare_denied_o;
  logic [31:0]             terr_rdshare_contention_o;
  logic [31:0]             terr_rdshare_err_short_o;
  logic [31:0]             terr_rdshare_err_long_o;
  logic [31:0]             terr_rdshare_err_unowned_o;

  logic                    terr_pt_fld_valid_i;
  logic                    terr_pt_fld_ready_o;
  logic signed [31:0]      terr_pt_fld_height_i;
  logic                    terr_pt_fld_add_valid_i;
  logic                    terr_pt_fld_add_ready_o;
  logic signed [31:0]      terr_pt_fld_add_x0_i;
  logic signed [31:0]      terr_pt_fld_add_z0_i;
  logic signed [31:0]      terr_pt_fld_add_x1_i;
  logic signed [31:0]      terr_pt_fld_add_z1_i;
  logic [31:0]             terr_pt_fld_add_hash_i;
  logic [15:0]             terr_pt_fld_add_cmd_i;
  logic                    terr_pt_fld_add_accept_o;
  logic                    terr_pt_fld_add_reject_o;
  logic                    terr_pt_fld_covers_o;
  logic [4:0]              terr_pt_fields_active_o;
  logic [15:0]             terr_pt_trace_patch_id_o;
  logic [31:0]             terr_pt_trace_hash_o;
  logic [15:0]             terr_pt_trace_cmd_o;
  logic [31:0]             terr_pt_programs_rejected_o;

  logic                    terr_cc_cs_we_i;
  logic [4:0]              terr_cc_cs_ci_i;
  logic [4:0]              terr_cc_cs_cj_i;
  logic [1:0]              terr_cc_cs_substance_i;
  logic                    terr_cc_serve_release_i;

  logic [31:0]             terr_ps_lattices_o;
  logic [31:0]             terr_ps_lattices_refused_o;
  logic [31:0]             terr_ps_vertices_o;
  logic [31:0]             terr_ps_bursts_o;
  logic [31:0]             terr_ps_guard_denied_o;
  logic [31:0]             terr_ps_incomplete_o;
  logic                    terr_ps_idle_o;
  logic                    terr_ps_done_valid_o;
  logic                    terr_ps_done_ok_o;
  logic [3:0]              terr_ps_done_verdict_o;
  logic                    terr_place_valid_o;
  logic [15:0]             terr_place_src_id_o;
  logic [15:0]             terr_place_env_mismatch_o;
  logic [15:0]             terr_place_pitch_bad_o;
  logic [15:0]             terr_place_range_o;
  logic [15:0]             terr_place_patches_o;
  logic [31:0]             terr_pt_samples_o;
  logic [15:0]             terr_pt_subpatch_dirty_o;
  logic                    terr_pt_idle_o;
  logic                    terr_cc_fill_busy_o;
  logic                    terr_cc_fill_done_o;
  logic                    terr_cc_serve_valid_o;
  logic [15:0]             terr_cc_serve_src_id_o;
  logic [31:0]             terr_cc_fill_records_o;
  logic [31:0]             terr_cc_patches_filled_o;
  logic [31:0]             terr_cc_patches_served_o;
  logic [31:0]             terr_cc_fill_overrun_o;
  logic [31:0]             terr_cc_lat_oob_o;
  logic [31:0]             terr_cc_cs_oob_o;

  logic                    terr_dm_valid_i;
  logic                    terr_dm_ready_o;
  logic [TERR_SLOTW_C-1:0] terr_dm_slot_i;
  logic [TERR_GENW_C-1:0]  terr_dm_gen_i;
  logic [31:0]             terr_dm_epoch_i;
  logic                    terr_dm_bd_i;
  logic                    terr_dm_f_i;
  logic                    terr_dm_mips_i;

  logic                    terr_chk_valid_i;
  logic [TERR_SLOTW_C-1:0] terr_chk_slot_i;
  logic [TERR_GENW_C-1:0]  terr_chk_gen_i;
  logic [31:0]             terr_chk_epoch_i;
  logic                    terr_chk_valid_o;
  logic                    terr_chk_stale_o;

  // SW.STREAM's journal doorbell (owner ruling R14): the HPS's own words.
  logic [31:0]             terr_cfg_journal_base_i;
  logic [31:0]             terr_cfg_journal_bytes_i;
  logic                    terr_jdb_post_valid_i;
  logic                    terr_jdb_post_ready_o;
  logic [15:0]             terr_jdb_post_slot_i;
  logic [31:0]             terr_jdb_post_ticket_i;
  logic                    terr_jdb_ret_valid_o;
  logic                    terr_jdb_ret_ready_i;
  logic [31:0]             terr_jdb_ret_ticket_o;
  logic                    terr_jdb_ret_final_o;
  logic                    terr_jdb_ret_ok_o;
  logic [3:0]              terr_jdb_ret_verdict_o;
  logic                    terr_jdb_ack_valid_i;
  logic                    terr_jdb_ack_ready_o;
  logic [31:0]             terr_jdb_ack_ticket_i;
  logic                    terr_jdb_ack_ok_i;
  logic [31:0]             terr_wb_sheets_written_o;
  logic [31:0]             terr_wb_sheets_refused_o;
  logic [31:0]             terr_wb_sheets_faulted_o;
  logic [31:0]             terr_wb_guard_denied_o;
  logic [31:0]             terr_wb_acks_unmatched_o;
  logic [31:0]             terr_wb_acks_overdue_o;
  logic [31:0]             terr_jdb_starved_cycles_o;
  logic [31:0]             terr_jdb_ret_overflow_o;
  logic                    terr_hps_wr_ready_i;
  logic [31:0]             terr_hps_c2_bursts_o;
  logic [31:0]             terr_hps_c2_wait_cycles_o;
  logic [31:0]             terr_rdshare_jobs_wb_o;

  logic [31:0]             terr_cmd_sets_accepted_o;
  logic [31:0]             terr_cmd_sets_refused_o;
  logic [31:0]             terr_cmd_records_emitted_o;
  logic [31:0]             terr_cmd_crc_fails_o;
  logic [31:0]             terr_cmd_bridge_errs_o;
  logic                    terr_seq_busy_o;
  logic                    terr_seq_done_o;
  logic [31:0]             terr_seq_records_consumed_o;
  logic [31:0]             terr_seq_patches_issued_o;
  logic [31:0]             terr_seq_claims_issued_o;
  logic [31:0]             terr_seq_claims_refused_o;
  logic [31:0]             terr_seq_claims_same_o;
  logic [31:0]             terr_seq_loads_issued_o;
  logic [31:0]             terr_seq_skipped_not_resident_o;
  logic [31:0]             terr_seq_frame_faults_o;
  logic                    terr_seq_err_stray_ans_o;
  logic [31:0]             terr_res_hits_o;
  logic [31:0]             terr_res_misses_o;
  logic [31:0]             terr_res_claims_o;
  logic [31:0]             terr_res_evictions_o;
  logic [31:0]             terr_res_crc_failures_o;
  logic [31:0]             terr_res_resident_o;

  // THE SECOND COMPLETION, 2026-09-19 (core composition item 12).  Declared
  // here rather than by hand at the bottom because this bench binds with `.*`:
  // every core port needs an identically-named net, and a port with none is a
  // bind error rather than a silent zero.
  logic [31:0]             terr_mip_pages_mipped_o;
  logic [31:0]             terr_mip_pages_faulted_o;
  logic [31:0]             terr_mip_samples_sent_o;
  logic [31:0]             terr_mipreq_requests_o;
  logic [31:0]             terr_mipreq_issued_o;
  logic [31:0]             terr_mipreq_drops_o;
  logic [31:0]             terr_psmux_a_jobs_o;
  logic [31:0]             terr_psmux_b_jobs_o;
  logic [31:0]             terr_psmux_stray_v_o;
  logic [31:0]             terr_psmux_stray_done_o;
  logic                    terr_mg_m17_valid_o;
  logic [ 8:0]             terr_mg_m17_addr_o;
  logic                    terr_mg_m17_surf_o;
  logic [15:0]             terr_mg_m17_h_o;
  logic                    terr_mg_m9_valid_o;
  logic [ 6:0]             terr_mg_m9_addr_o;
  logic                    terr_mg_m9_surf_o;
  logic [15:0]             terr_mg_m9_h_o;
  logic [31:0]             terr_mg_m17_writes_o;
  logic [31:0]             terr_mg_m9_writes_o;
  logic [31:0]             terr_mg_aborts_o;

  logic [31:0]             terr_lq_accepted_o;
  logic [31:0]             terr_lq_issued_o;
  logic [31:0]             terr_lq_high_water_o;
  logic [31:0]             terr_pl_pages_loaded_o;
  logic [31:0]             terr_pl_pages_faulted_o;
  logic [31:0]             terr_pl_crc_fails_o;
  logic [31:0]             terr_pl_load_bytes_o;
  logic [31:0]             terr_pl_guard_denied_o;
  logic [31:0]             terr_pl_bridge_errs_o;
  logic [31:0]             terr_pl_slot_overflow_o;
  logic [31:0]             terr_pl_pages_refused_o;
  logic [3:0]              terr_pl_fault_verdict_o;
  logic [31:0]             terr_pl_fault_island_o;
  logic signed [15:0]      terr_pl_fault_ix_o;
  logic signed [15:0]      terr_pl_fault_iz_o;
  logic [31:0]             terr_pl_fault_src_id_o;
  logic [31:0]             terr_pl_incomplete_o;
  logic [31:0]             terr_pl_hdr_ident_fails_o;
  logic [31:0]             terr_hps_c0_bursts_o;
  logic [31:0]             terr_hps_c1_bursts_o;
  logic [31:0]             terr_hps_c1_wait_cycles_o;

  // ---- MEM.UPLOAD on the TERRAIN.BUILD socket (2026-09-19, cmdmem) --------
  logic                    upl_req_valid_i;
  logic                    upl_req_ready_o;
  logic [ 7:0]             upl_req_tag_i;
  logic [23:0]             upl_req_index_i;
  logic [63:0]             upl_req_hps_addr_i;
  logic [31:0]             upl_req_vram_addr_i;
  logic [31:0]             upl_req_len_i;
  logic [15:0]             upl_req_epoch_i;
  logic [ 7:0]             upl_req_dst_slot_i;
  logic [15:0]             upl_req_new_gen_i;
  logic [31:0]             upl_req_crc_i;
  logic [31:0]             upl_cfg_region_base_i;
  logic [31:0]             upl_cfg_region_bytes_i;
  logic [63:0]             upl_cfg_arena_base_i;
  logic [31:0]             upl_cfg_arena_bytes_i;
  logic [15:0]             upl_cfg_epoch_i;
  logic                    upl_publish_valid_o;
  logic [ 7:0]             upl_publish_slot_o;
  logic [15:0]             upl_publish_generation_o;
  logic [ 7:0]             upl_publish_tag_o;
  logic [23:0]             upl_publish_index_o;
  logic [31:0]             upl_publish_base_o;
  logic [31:0]             upl_publish_extent_o;
  logic                    upl_done_o;
  logic [ 7:0]             upl_status_o;
  logic [15:0]             upl_published_o;
  logic [127:0]            upl_refused_o;
  logic [31:0]             upl_hps_wait_o;

  logic [PROJ_T_ARENAS-1:0] terr_held_o;
  logic                    terr_busy_o;
  logic [31:0]             terr_jobs_accepted_o;
  logic [31:0]             terr_jobs_no_view_o;
  logic [31:0]             terr_jobs_rejected_o;
  logic [31:0]             terr_jobs_empty_o;
  logic [31:0]             terr_groups_opened_o;
  logic [31:0]             terr_groups_released_o;
  logic [31:0]             terr_fills_forwarded_o;
  logic [31:0]             terr_fills_dropped_o;
  logic [31:0]             terr_refs_forwarded_o;
  logic [31:0]             terr_release_unsafe_o;
  logic [31:0]             terr_tess_vertices_o;
  logic [31:0]             terr_tess_refs_o;
  logic [31:0]             terr_tess_rejected_o;
  logic [31:0]             terr_tess_lod_clamped_o;
  logic [31:0]             terr_tess_mode_invalid_o;
  logic                    terr_tess_idle_o;
  logic                    proj_out_valid_o;
  logic                    proj_out_ready_i;
  logic signed [20:0]      proj_out_ax_o;
  logic signed [20:0]      proj_out_ay_o;
  logic signed [20:0]      proj_out_bx_o;
  logic signed [20:0]      proj_out_by_o;
  logic signed [20:0]      proj_out_cx_o;
  logic signed [20:0]      proj_out_cy_o;
  logic [2:0]              proj_out_behind_o;
  logic [15:0]             proj_out_src_id_o;
  logic signed [31:0]      proj_out_ad_o;
  logic signed [31:0]      proj_out_bd_o;
  logic signed [31:0]      proj_out_cd_o;
  logic [30:0]             proj_out_aw_o;
  logic [30:0]             proj_out_bw_o;
  logic [30:0]             proj_out_cw_o;
  logic                    proj_out_view_o;
  logic [7:0]              proj_out_mat_a_o;
  logic [7:0]              proj_out_mat_b_o;
  logic [7:0]              proj_out_weight_o;
  logic                    proj_out_refused_o;
  logic                    proj_out_missed_o;
  logic                    proj_a_view_o;
  logic [1:0]              proj_a_profile_o;
  logic [1:0]              proj_fill_profile_o;
  logic [31:0]             proj_replay_triangles_o;
  logic [31:0]             proj_replay_refused_o;
  logic [31:0]             proj_replay_missed_o;
  logic [31:0]             proj_corner_hits_o;
  logic [31:0]             proj_corner_refusals_o;
  logic [31:0]             proj_corner_misses_o;
  logic                    proj_arena_overflow_o;
  logic                    proj_arena_seal_short_o;
  logic                    proj_shell_idle_o;
  logic                    proj_svc_busy_o;
  logic [31:0]             proj_a_grants_o;
  logic [31:0]             proj_b_grants_o;
  logic [31:0]             proj_contended_o;
  logic [31:0]             proj_mat_refused_o;
  logic                    post_view_sel_i;
  logic                    post_s_valid_i;
  logic                    post_s_ready_o;
  logic [15:0]             post_s_rgb_i;
  logic                    post_gd_req_v_o;
  logic                    post_gd_view_o;
  logic [POST_XW-3:0]      post_gd_cx_o;
  logic [POST_YW-3:0]      post_gd_cy_o;
  logic                    post_gd_present_i;
  logic signed [7:0]       post_gd_dx_i;
  logic signed [7:0]       post_gd_dy_i;
  logic                    post_gg_req_v_o;
  logic                    post_gg_view_o;
  logic [POST_XW-3:0]      post_gg_cx_o;
  logic [POST_YW-3:0]      post_gg_cy_o;
  logic                    post_gg_present_i;
  logic [15:0]             post_gg_glow_i;
  logic                    post_gg_ink_i;
  logic                    post_atm_req_v_o;
  logic [POST_XW-1:0]      post_atm_req_x_o;
  logic [POST_YW-1:0]      post_atm_req_y_o;
  logic                    post_atm_en_i;
  logic                    post_atm_valid_i;
  logic [15:0]             post_atm_rgb_i;
  logic [7:0]              post_atm_opacity_i;
  logic                    post_atm_add_i;
  logic [7:0]              post_bloom_gain_i;
  logic                    post_grade_valid_i;
  logic                    post_pv_we_i;
  logic [1:0]              post_pv_sel_i;
  logic [5:0]              post_pv_addr_i;
  logic [71:0]             post_pv_data_i;
  logic signed [8:0]       post_bias_r_i;
  logic signed [8:0]       post_bias_g_i;
  logic signed [8:0]       post_bias_b_i;
  logic [15:0]             post_flash_rgb_i;
  logic [7:0]              post_flash_amt_i;
  logic [15:0]             post_ink_rgb_i;
  logic                    post_hud_req_v_o;
  logic [POST_XW-1:0]      post_hud_req_x_o;
  logic [POST_YW-1:0]      post_hud_req_y_o;
  logic                    post_hud_valid_i;
  logic [15:0]             post_hud_rgb_i;
  logic                    post_o_valid_o;
  logic                    post_o_ready_i;
  logic [15:0]             post_o_rgb_o;
  logic [POST_XW-1:0]      post_o_x_o;
  logic [POST_YW-1:0]      post_o_y_o;
  logic                    post_o_last_o;
  logic                    post_echo_valid_o;
  logic [15:0]             post_echo_rgb_o;
  logic [31:0]             post_displacement_edge_clamps_o;
  logic [31:0]             post_bloom_cells_contributing_o;
  logic [31:0]             post_passes_completed_o;
  logic [31:0]             post_grading_table_missing_o;
  logic [31:0]             post_plane_missing_o;
  logic [31:0]             post_line_fill_writes_o;
  logic [31:0]             post_output_writes_o;
  logic [31:0]             post_plane_reads_o;
  logic [31:0]             post_ring_hazard_o;
  logic                    hist_ev_valid_i;
  logic [HIST_LANES-1:0]   hist_ev_lane_valid_i;
  logic [HIST_LANES*HIST_EW-1:0] hist_ev_err_i;
  logic [15:0]             hist_ev_src_id_i;
  logic                    hist_ev_ready_o;
  logic                    hist_rd_valid_i;
  logic [HIST_BINW-1:0]    hist_rd_bin_i;
  logic                    hist_rd_ready_o;
  logic                    hist_rd_data_valid_o;
  logic [HIST_CW-1:0]      hist_rd_count_o;
  logic                    hist_snap_valid_o;
  logic [HIST_CW-1:0]      hist_snap_total_o;
  logic [15:0]             hist_snap_src_id_o;
  logic [HIST_CW-1:0]      hist_snap_index_o;
  logic [HIST_CW-1:0]      hist_events_o;
  logic [HIST_CW-1:0]      hist_updates_o;
  logic [HIST_CW-1:0]      hist_stall_cycles_o;
  logic [HIST_CW-1:0]      hist_bin_sat_o;
  logic [HIST_CW-1:0]      hist_fwd_hits_o;
  logic [HIST_CW-1:0]      hist_host_conflict_o;
  logic [HIST_CW-1:0]      hist_snapshots_o;
  logic [HIST_CW-1:0]      hist_frozen_write_o;
  logic                    light_seam_connected_o;
  logic gpu_clk;
  logic vid_clk;
  logic audio_clk;
  logic rst_n;
  logic        cfg_valid_i;
  logic        cfg_ready_o;
  logic [1:0]  cfg_op_i;
  logic [7:0]  cfg_page_generation_i;
  logic [7:0]  cfg_selector_i;
  logic [74:0] cfg_row_i;
  logic [31:0] cfg_crc32_i;
  logic        cfg_rsp_valid_o;
  logic        cfg_rsp_ready_i;
  logic [1:0]  cfg_rsp_op_o;
  logic [3:0]  cfg_rsp_status_o;
  logic [7:0]  cfg_rsp_page_generation_o;
  logic [7:0]  active_page_generation_o;
  logic        pal_load_valid_i;
  logic        pal_load_ready_o;
  logic [1:0]  pal_load_op_i;
  logic [1:0]  pal_load_slot_i;
  logic [7:0]  pal_load_gen_i;
  logic [7:0]  pal_load_idx_i;
  logic [15:0] pal_load_rgb565_i;
  logic        pal_load_crc_ok_i;
  logic [46:0]  tri_area2_i;
  // THE THREE ATTRIBUTE PLANES ARE GONE FROM HERE, and their absence is the
  // whole point. They were core INPUTS this bench drove to zero; the core now
  // contains their producer (GEOM.ATTRPACK), so a net here would bind to
  // nothing under `.*` and, worse, would read as though this bench were still
  // supplying them.
  logic [297:0] tri_flat_request_i;
  logic [47:0]  tri_continuation_tail_i;
  logic [31:0]  tri_fragment_state_i;
  logic         fill_req_ready_i;
  logic         fill_req_valid_o;
  logic [31:0]  fill_req_addr_o;
  logic         fill_data_valid_i;
  logic [15:0]  fill_data_i;
  logic         fill_refused_i;
  logic [63:0]  frame_clear_word_i;
  logic         sheet_req_ready_i;
  logic         sheet_req_valid_o;
  logic [1:0]   sheet_req_op_o;
  logic [31:0]  sheet_req_handle_o;
  logic [11:0]  sheet_req_texel_o;
  logic [15:0]  sheet_req_src_id_o;
  logic        blank_cmd_i;
  logic        scanout_ack_i;
  logic        frame_swap_valid_i;
  logic        frame_swap_slot_i;
  logic        blank_ack_o;
  logic        blank_active_o;
  logic        lease_open_o;
  logic [1:0]  frame_slot_ready_o;
  logic [31:0] v2_requests_accepted_o;
  logic [31:0] v2_responses_accepted_o;
  logic [31:0] v2_leases_granted_o;
  logic [31:0] v2_leases_refused_o;
  logic [31:0] v2_faults_latched_o;
  logic [31:0] v2_publications_o;
  logic [31:0] v2_releases_o;
  logic [31:0] v2_ready_events_o;
  logic [31:0] v2_swaps_o;
  logic [31:0] v2_contentions_o;
  logic [31:0] v2_clear_handshakes_o;
  logic [31:0] v2_frames_admitted_o;
  logic [31:0] v2_blit_leases_acquired_o;
  logic [31:0] v2_blit_leases_refused_o;
  logic [1:0]  hps_state_i [0:2];
  logic [31:0] hps_byte_len_i [0:2];
  logic        ring_wr_valid_o;
  logic [1:0]  ring_wr_slot_o;
  logic [1:0]  ring_wr_state_o;
  logic        ring_wr_ready_i;
  logic        hps_req_valid_o;
  logic        hps_req_write_o;
  logic [31:0] hps_req_addr_o;
  logic [6:0]  hps_req_len_o;
  logic        hps_req_grant_i;
  logic        hps_wr_valid_o;
  logic [63:0] hps_wr_data_o;
  logic        hps_wr_last_o;
  logic        hps_rd_valid_i;
  logic [63:0] hps_rd_data_i;
  logic        hps_rd_last_i;
  logic [3:0]  pad_present_i;
  logic [31:0] pad_buttons_i [0:3];
  logic [15:0] pad_lx_i [0:3];
  logic [15:0] pad_ly_i [0:3];
  logic [15:0] pad_rx_i [0:3];
  logic [15:0] pad_ry_i [0:3];
  logic        aud_wr_valid_i;
  logic [15:0] aud_wr_l_i;
  logic [15:0] aud_wr_r_i;
  logic        aud_wr_ready_o;
  logic        aud_refill_req_o;
  logic [11:0] aud_occupancy_o;
  logic        pcm_valid_o;
  logic [15:0] pcm_l_o;
  logic [15:0] pcm_r_o;
  logic        underrun_status_o;
  logic [31:0] audio_underruns_o;
  logic        px_valid_o;
  logic [15:0] px_rgb_o;
  logic [9:0]  px_x_o;
  logic [7:0]  px_y_o;
  logic        px_hsync_o;
  logic        px_vsync_o;
  logic        px_hblank_o;
  logic        px_vblank_o;
  logic        scaler_violation_o;
  logic [31:0] crc_frame_o;
  logic        crc_valid_o;
  logic [31:0] crc_bytes_o;
  logic        crc_size_err_o;
  logic        gpu_tick_o;
  logic [31:0] gpu_tick_frame_id_o;
  logic        gpu_tick_repeated_o;
  logic [0:0]  gpu_complete_slot_o;
  logic [63:0] deadline_faults_o;
  logic [63:0] frame_cycles_o;
  logic [2:0]  slot_state_o [0:2];
  logic        fence_valid_o;
  logic [1:0]  fence_slot_o;
  logic        fence_ok_o;
  logic [7:0]  fence_status_o;
  logic [1:0]  mode_act_o;
  logic        dma_done_o;
  logic [7:0]  dma_status_o;
  logic        blit_done_o;
  logic [7:0]  blit_status_o;
  logic [639:0] pad_frame_flat_o;
  logic [15:0]  pad_sequence_o [0:3];
  logic [63:0]  input_gaps_o;
  logic [7:0]   rumble_duty_o [0:3];
  logic [3:0]   rumble_active_o;
  logic [3:0]   rumble_pwm_o;
  logic [63:0]  rumble_drops_o;
  logic        cnt_snap_ready_i;
  logic        cnt_snap_valid_o;
  logic [15:0] cnt_snap_id_o;
  logic [63:0] cnt_snap_value_o;
  logic        cnt_window_open_o;
  logic        cnt_cat_violation_o;
  logic [31:0] guard_violations_o;
  logic [63:0] starvation_o;
  logic        init_done_o;
  logic [31:0] refresh_stalls_o;
  logic [31:0] bank_conflicts_o;
  logic [31:0] scanout_preempted_o;
  logic [31:0] hps_err_count_o;
  logic        shell_err_wfifo_o;
  logic        shell_err_route_o;
  logic        shell_err_cdc_o;
  logic        shell_err_framer_o;
  logic        render_frame_begin_i;
  logic        render_frame_end_i;
  logic [5:0]  render_grid_w_i;
  logic [5:0]  render_grid_h_i;
  // The triangle door is no longer at the core's edge -- GEOM.SETUP drives it
  // from inside. What this bench presents instead is GEOM.CLIP's input, one
  // block further up (entry I24).
  // `geom_guard_req_i`, `geom_guard_rsp_o` and the three `geom_beat_*_o`
  // ARE GONE from the core's port list too. They were the seam where this
  // bench answered the geometry fetchers' grants and fabricated their beats;
  // GEOM.MEM_ADAPTER drives the shell's socket now, so what this bench
  // supplies is one level lower and is real memory -- see the SDRAM model
  // and the asset fixture below.
  // ---- GEOM.CLIP's cull mode (entry I24, narrowed) and the CLIP/SETUP evidence
  // THE TRIANGLE DOOR IS GONE: GEOM.CLIP is fed by GEOM.REPLAY inside the core.
  localparam int unsigned GEOM_CLIP_ATTRW = 7 * 32;
  localparam int unsigned GEOM_ATTR_STORE_W = 6 * 32;
  // ---- I46: the vertex-attribute store, MODELLED here (its writer is unbuilt)
  logic                    geom_att_look_valid_o;
  logic [GEOM_ARENA_W-1:0] geom_att_look_arena_o;
  logic [GEOM_GEN_W-1:0]   geom_att_look_gen_o;
  logic [GEOM_INDEX_W-1:0] geom_att_look_index_o;
  logic                    geom_att_rep_valid_i;
  logic [GEOM_ATTR_STORE_W-1:0] geom_att_rep_data_i;
  // ---- GEOM.REPLAY's evidence
  logic [31:0] geom_rp_meshlets_o, geom_rp_groups_o, geom_rp_triangles_in_o;
  logic [31:0] geom_rp_triangles_out_o, geom_rp_refused_o, geom_rp_missed_o;
  logic [31:0] geom_rp_att_skew_o, geom_rp_profile_mixed_o, geom_rp_view_bad_o;
  logic [31:0] geom_rp_dq_refused_o;
  // GEOM.ATTRPACK's evidence, out of the core because a counter nobody can
  // read is not evidence. The RATIO is what gets asserted below.
  logic [31:0]             geom_attrpack_triangles_o;
  logic [31:0]             geom_attrpack_planes_o;
  logic [1:0]              geom_clip_cull_mode_i;
  logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_a_o, geom_clip_attr_b_o, geom_clip_attr_c_o;
  logic                    geom_clip_flip_o;
  logic                    geom_clip_ret_valid_o;
  logic [2:0]              geom_clip_ret_verdict_o;
  logic [31:0]             geom_clip_submitted_o;
  logic [31:0]             geom_clip_clipped_o;
  logic [31:0]             geom_clip_culled_o;
  logic signed [47:0]      geom_setup_area2_o;
  logic [31:0]             geom_setup_triangles_submitted_o;
  logic [63:0] render_fill_word_i;
  logic [63:0] render_clear_word_i;
  logic [31:0] render_state_i;
  logic [ 7:0] render_src_a_i;
  logic [23:0] render_texel_rgb_i;
  logic [ 7:0] render_texel_a_i;
  logic [ 7:0] render_texel_idx_i;
  logic [26:0] render_fb_base_i;
  logic [15:0] render_fb_stride_i;
  logic        fb_writer_i;
  logic        render_drain_done_o;
  logic        render_busy_o;
  logic [31:0] render_pixels_o;
  logic [31:0] render_bursts_o;
  logic        render_stream_error_o;
  logic        render_drained_o;
  logic        render_fatal_o;
  logic [31:0] render_issued_words_o;
  logic [31:0] render_retired_words_o;
  logic        render_overflow_o;
  logic        render_fragment_error_o;
  // ---- PACKET P-SURFACE, 2026-09-19 -------------------------------------
  // The three ends of the composed SURFACE pair that have no owner in the
  // tree (DUT entries I30, I31, I32), plus its evidence. The request, page
  // and write channels between STAMP and SHEET are INTERNAL to the DUT and
  // deliberately do not appear here -- if they did, this bench would be
  // playing the seam it is supposed to be measuring.
  logic               surf_cmd_valid_i;
  logic               surf_cmd_ready_o;
  logic        [31:0] surf_cmd_handle_i;
  logic        [ 7:0] surf_cmd_operation_i;
  logic        [ 7:0] surf_cmd_tag_i;
  logic        [15:0] surf_cmd_strength_i;
  logic signed [31:0] surf_cmd_tx_i;
  logic signed [31:0] surf_cmd_ty_i;
  logic signed [31:0] surf_cmd_radius_i;
  logic signed [31:0] surf_cmd_ring_width_i;
  logic signed [31:0] surf_cmd_env_x0_i;
  logic signed [31:0] surf_cmd_env_z0_i;
  logic signed [31:0] surf_cmd_env_x1_i;
  logic signed [31:0] surf_cmd_env_z1_i;
  logic               surf_cmd_blend_en_i;
  logic        [ 2:0] surf_cmd_blend_i;
  logic        [ 2:0] surf_cmd_age_shift_i;
  logic               surf_cmd_field_en_i;
  logic        [15:0] surf_cmd_src_id_i;
  // I31 CLOSED 2026-09-19: `surf_fld_*` is no longer at the DUT's edge. The
  // brush is driven inside the core by `u_field_stamp_adapter`, so these four
  // nets are GONE rather than held at zero. What the adapter reports is below.
  logic        [31:0] surf_fld_stamps_o;
  logic        [31:0] surf_fld_texels_o;
  logic        [31:0] surf_fld_faults_o;
  logic        [31:0] surf_fld_restarts_o;
  logic               surf_fld_busy_o;

  // ---- THE FIELD ENGINE'S EDGE (core entry I42) ---------------------------
  // Widths generated from the core's own port list, including the two wide
  // ones: `fld_req_in_i` is IN_LANES x 32 = 384 and `fld_resp_out_o` is
  // OUT_LANES x 32 = 128, at the parameters the core instantiates.
  logic               fld_ld_valid_i;
  logic               fld_ld_ready_o;
  logic        [ 1:0] fld_ld_kind_i;
  logic        [ 2:0] fld_ld_slot_i;
  logic        [ 6:0] fld_ld_addr_i;
  logic        [95:0] fld_ld_data_i;
  logic               fld_pc_lu_valid_i;
  logic               fld_pc_lu_ready_o;
  logic        [31:0] fld_pc_lu_hash_i;
  logic               fld_pc_lu_resp_valid_o;
  logic               fld_pc_lu_resp_ready_i;
  logic               fld_pc_lu_hit_o;
  logic        [ 2:0] fld_pc_lu_slot_o;
  logic               fld_pc_cm_valid_i;
  logic               fld_pc_cm_ready_o;
  logic        [31:0] fld_pc_cm_hash_i;
  logic               fld_pc_cm_ok_i;
  logic               fld_pc_cm_resp_valid_o;
  logic               fld_pc_cm_resp_ready_i;
  logic               fld_pc_cm_inserted_o;
  logic               fld_pc_cm_evicted_o;
  logic        [ 2:0] fld_pc_cm_slot_o;
  logic        [ 2:0] fld_stamp_slot_i;
  logic               fld_stamp_slot_valid_i;
  logic               fld_req_valid_i;
  logic               fld_req_ready_o;
  logic        [ 2:0] fld_req_slot_i;
  logic               fld_req_noprog_i;
  logic [383:0]       fld_req_in_i;
  logic               fld_resp_valid_o;
  logic               fld_resp_ready_i;
  logic [127:0]       fld_resp_out_o;
  logic        [ 7:0] fld_resp_status_o;
  logic        [31:0] fld_runs_o;
  logic        [31:0] fld_run_faults_o;
  logic        [31:0] fld_noprog_o;
  logic        [31:0] fld_instr_retired_o;
  logic        [31:0] fld_loads_o;
  logic        [31:0] fld_load_defers_o;
  logic        [31:0] fld_grants_o;
  logic        [31:0] fld_contended_grants_o;
  logic        [31:0] fld_ld_oob_o;
  logic        [31:0] fld_no_result_o;
  logic        [31:0] fld_exec_desync_o;
  logic        [31:0] fld_bank_desync_o;
  logic        [31:0] fld_svc_bank_desync_o;
  logic        [31:0] fld_tag_mismatch_o;
  logic        [31:0] fld_wrong_op_o;
  logic        [31:0] fld_unsupported_o;
  logic        [31:0] fld_skid_overflow_o;
  logic        [31:0] fld_uniform_bad_o;
  logic        [ 2:0] fld_sat_o;
  logic        [31:0] fld_pc_hits_o;
  logic        [31:0] fld_pc_misses_o;
  logic        [31:0] fld_pc_rejected_o;
  logic        [31:0] fld_pc_evictions_o;
  logic        [ 3:0] fld_pc_occupancy_o;
  logic               surf_res_valid_o;
  logic               surf_res_ready_i;
  logic        [11:0] surf_res_texel_o;
  logic        [ 7:0] surf_res_tag_o;
  logic        [ 7:0] surf_res_strength_o;
  logic        [ 7:0] surf_res_before_o;
  logic        [15:0] surf_res_src_id_o;
  logic        [ 1:0] surf_pg_op_o;
  logic        [ 7:0] surf_pg_tag_o;
  logic        [15:0] surf_pg_src_id_o;
  logic [SURF_SLOTS-1:0] surf_res_occupancy_o;
  logic               surf_res_busy_o;
  logic               surf_res_overflow_o;
  logic               surf_sheet_wr_miss_o;
  logic        [15:0] surf_sheet_wr_miss_src_id_o;
  logic               surf_sheet_idle_o;
  logic        [31:0] surf_sheet_texels_touched_o;
  logic               surf_stamp_done_o;
  logic               surf_stamp_rejected_o;
  logic               surf_stamp_idle_o;
  logic        [31:0] surf_stamps_o;
  logic        [31:0] surf_stamp_texels_touched_o;

  logic        phy_cs_n_o;
  logic        phy_ras_n_o;
  logic        phy_cas_n_o;
  logic        phy_we_n_o;
  logic [12:0] phy_a_o;
  logic [1:0]  phy_ba_o;
  logic [15:0] phy_dq_o;
  logic        phy_dq_oe_o;
  logic [1:0]  phy_dqm_o;
  logic [15:0] phy_dq_i;

  // ---- THE COMMAND FRONT (core sections 7b and 7c) -----------------------
  // CMD.DECODER's record headers and verdict, and CMD.EXEC's evidence. All
  // outputs; this bench declares them because `.*` binds by name and a port
  // with no net here is a compile error rather than a floating wire -- which
  // is the whole reason the comment at the head of this block says so.
  //
  // `cmd_exec_*` is what says a packet BECAME CONSOLE STATE rather than merely
  // validating: `cmd_exec_views_o` moves when SetView's mat4fx reached the
  // shared projector's matrix bank, `cmd_exec_stamps_o` when a SurfaceStamp
  // reached SURFACE.STAMP. `cmd_exec_unsupported_o` is expected to be LARGE --
  // it counts every ABI record the executor has no arm for, which today is
  // most of them, and reading it as a fault would be reading the gap as a bug.
  logic        cmd_rec_valid_o;
  logic [15:0] cmd_rec_opcode_o;
  logic [15:0] cmd_rec_bytes_o;
  logic [31:0] cmd_rec_source_id_o;
  logic [31:0] cmd_rec_index_o;
  logic        cmd_decode_done_o;
  logic [ 7:0] cmd_decode_error_o;
  logic [31:0] cmd_bytes_consumed_o;
  logic [31:0] cmd_commands_o;

  // ---- DEBUG.TRACE's arming and readout (core entry I45) -----------------
  // Declared for the `.*` reason above, and ARMED below, because the thing
  // worth checking here is the one neither block's isolated test can see: the
  // ring's contract says "composition with CMD.DECODER is the point ... a
  // field mismatch that neither block's isolated tests can see". So the bench
  // arms stage 0 and asserts the ring stored EXACTLY as many events as the
  // decoder reports records walked. A count that is merely non-zero would pass
  // with the source id and the sequence swapped, or with a stage constant that
  // happened to be armed; an EQUALITY against the producer's own counter is
  // the check that can fail.
  logic        dbg_trace_arm_we_i;
  logic [ 6:0] dbg_trace_arm_mask_i;
  logic        dbg_trace_clear_i;
  logic [ 8:0] dbg_trace_rd_addr_i;
  logic [31:0] dbg_trace_rd_data_o;
  logic [ 6:0] dbg_trace_armed_o;
  logic [31:0] dbg_trace_count_o;
  logic [31:0] dbg_trace_dropped_o;

  logic [31:0] cmd_exec_committed_o;
  logic [31:0] cmd_exec_abandoned_o;
  logic [31:0] cmd_exec_views_o;
  logic [31:0] cmd_exec_stamps_o;
  logic [31:0] cmd_exec_stamp_overflow_o;
  logic [31:0] cmd_exec_view_refused_o;
  logic [31:0] cmd_exec_src_truncated_o;
  logic [31:0] cmd_exec_unsupported_o;

  // ---- THE DRAW DISPATCH (core entry I41), added 2026-09-19 --------------
  // `DrawForm 0x0300` lowered by CMD.EXEC's draw arm. Declared here for the
  // `.*` reason above.
  //
  // `cmd_draw_ready_i` IS HELD HIGH AND THAT IS A MODELLED CONSUMER, NOT A
  // TIE-OFF DRESSED UP. The dispatch is the LAST phase of CMD.EXEC's commit,
  // so a ready held low would park the executor in EX_DRAW forever: the next
  // packet's bytes would never be accepted, `cmd_exec_committed_o` would stay
  // at zero, and this bench's command-path checks would fail with no hint of
  // where. High is what an always-available consumer looks like, and the
  // REFUSING consumer is exercised where it belongs -- three ready patterns in
  // `tests/command/cmd_exec_directed.cpp` case 12.
  logic        cmd_draw_valid_o;
  logic        cmd_draw_ready_i;
  logic [31:0] cmd_draw_form_o;
  logic [31:0] cmd_draw_material_set_o;
  logic [31:0] cmd_draw_transform_o;
  logic [ 7:0] cmd_draw_viewport_mask_o;
  logic [ 7:0] cmd_draw_semantic_weight_o;
  logic [15:0] cmd_draw_flags_o;
  logic [15:0] cmd_draw_src_id_o;
  logic [31:0] cmd_exec_draws_o;
  logic [31:0] cmd_exec_draw_overflow_o;
  logic [31:0] cmd_exec_draw_src_truncated_o;

  // ---- THE PARTICLE DRAW PATH (core entry I24) ---------------------------
  // PART.PROJECT -> PART.LADDER -> {PART.EXPAND, PART.SOFT}, composed
  // 2026-09-19. Declared here for the same reason the command front above is:
  // `.*` binds by name and a port with no net is a compile error.
  //
  // **THE THREE `_ready_i` HERE ARE NOT DECORATION AND MUST NOT BE TIED LOW.**
  // PART.PROJECT takes a THIRD BRANCH of the fork on PART.COLLIDE's output, so
  // a draw path that never drains holds its slots, refuses at `p_ready_o`, and
  // STALLS THE WHOLE PARTICLE TICK -- the ring would stop and this bench's
  // survivor and child checks would fail with no hint of where. That is the
  // correct behaviour (backpressure, never a drop), and it is exactly why the
  // consumer has to be modelled rather than grounded. They are held HIGH: this
  // bench is a wiring smoke and the endpoints' real customer is the absent GEOM
  // replay/setup path the core's I24 names.
  logic signed [31:0] part_prj_base_radius_i;
  logic               part_prj_view_i;
  logic        [15:0] part_prj_trail_i;
  logic               part_prj_narrow_i;
  logic               part_prj_protected_i;
  logic        [ 2:0] part_prj_gov_floor_i;
  logic        [ 2:0] part_prj_prev_rung_i;
  logic        [ 3:0] part_prj_hold_i;
  logic               part_prj_first_i;
  logic        [ 7:0] part_prj_r_i;
  logic        [ 7:0] part_prj_g_i;
  logic        [ 7:0] part_prj_b_i;
  logic        [15:0] part_prj_src_id_i;

  logic               part_rung_valid_o;
  logic               part_rung_ready_i;
  logic        [ 2:0] part_rung_o;
  logic        [ 3:0] part_rung_hold_o;
  logic               part_rung_changed_o;
  logic        [15:0] part_rung_src_id_o;

  logic               part_exp_valid_o;
  logic               part_exp_ready_i;
  logic signed [21:0] part_exp_ax_o;
  logic signed [21:0] part_exp_ay_o;
  logic signed [21:0] part_exp_bx_o;
  logic signed [21:0] part_exp_by_o;
  logic signed [21:0] part_exp_cx_o;
  logic signed [21:0] part_exp_cy_o;
  logic signed [31:0] part_exp_d_o;
  logic        [ 7:0] part_exp_r_o;
  logic        [ 7:0] part_exp_g_o;
  logic        [ 7:0] part_exp_b_o;
  logic               part_exp_depth_test_o;
  logic               part_exp_depth_write_o;
  logic        [15:0] part_exp_src_id_o;

  logic               part_sft_valid_o;
  logic               part_sft_ready_i;
  logic signed [12:0] part_sft_min_x_o;
  logic signed [12:0] part_sft_max_x_o;
  logic signed [12:0] part_sft_min_y_o;
  logic signed [12:0] part_sft_max_y_o;
  logic signed [31:0] part_sft_d_o;
  logic        [ 7:0] part_sft_r_o;
  logic        [ 7:0] part_sft_g_o;
  logic        [ 7:0] part_sft_b_o;
  logic               part_sft_depth_test_o;
  logic               part_sft_depth_write_o;
  logic        [15:0] part_sft_src_id_o;

  logic [31:0] part_prj_projected_o;
  logic [31:0] part_prj_behind_o;
  logic [31:0] part_prj_geom_grants_o;
  logic [31:0] part_prj_part_grants_o;
  logic [31:0] part_prj_contended_o;
  logic [31:0] part_prj_size_sat_o;
  logic [31:0] part_prj_slot_pressure_o;
  logic [31:0] part_prj_tag_collision_o;
  logic [31:0] part_prj_ladder_unexpected_o;
  logic [31:0] part_lad_decisions_o;
  logic [31:0] part_lad_changes_o;
  logic [31:0] part_lad_held_o;
  logic [31:0] part_lad_gov_forced_o;
  logic [31:0] part_exp_polygons_o;
  logic [31:0] part_sft_sprites_o;


  // ---------------------------------------------------------------------
  // TWOD.PLANE / TWOD.SPRITE / TWOD.SAMPLER -- added 2026-09-19.
  // ---------------------------------------------------------------------
  // `zhao_console_core u_core (.*)` binds by NAME, so every port the core
  // grows needs an identically-named net here or the bench will not
  // elaborate at all. The compositor packet added 86 of them and the bench
  // could not be built for the rest of the session -- two other packets
  // each reported it as "not mine", correctly, and it stayed broken
  // because it belonged to whoever looked last.
  //
  // These are DECLARED AND NOT DRIVEN, deliberately. The smoke bench does
  // not exercise the 2D path: the inputs sit at their reset values and the
  // outputs are observed rather than checked. That is the honest claim for
  // a smoke bench -- it proves the composition ELABORATES and the frame
  // path still runs, NOT that a plane or a sprite draws anything. Reading
  // a green smoke run as evidence about the 2D path would be exactly the
  // mistake CLAUDE.md records about gates that cannot reach the state.
  //
  // Generated from the core`s own port list rather than typed, so a width
  // here cannot disagree with the port it binds.
  logic                                twod_pd_valid_i;
  logic                                twod_pd_ready_o;
  logic                                twod_pd_slot_i;
  logic         [1:0]                  twod_pd_role_i;
  logic         [1:0]                  twod_pd_blend_i;
  logic         [7:0]                  twod_pd_opacity_i;
  logic                                twod_pd_format_i;
  logic         [15:0]                 twod_pd_width_i;
  logic         [15:0]                 twod_pd_height_i;
  logic                                twod_pd_wrap_u_i;
  logic                                twod_pd_wrap_v_i;
  logic signed  [31:0]                 twod_pd_a_i;
  logic signed  [31:0]                 twod_pd_b_i;
  logic signed  [31:0]                 twod_pd_c_i;
  logic signed  [31:0]                 twod_pd_d_i;
  logic signed  [31:0]                 twod_pd_u0_i;
  logic signed  [31:0]                 twod_pd_v0_i;
  logic         [1:0]                  twod_pd_view_mask_i;
  logic         [7:0]                  twod_pd_palette_i;
  logic                                twod_sd_valid_i;
  logic                                twod_sd_ready_o;
  logic signed  [15:0]                 twod_sd_x_i;
  logic signed  [15:0]                 twod_sd_y_i;
  logic         [15:0]                 twod_sd_w_i;
  logic         [15:0]                 twod_sd_h_i;
  logic signed  [31:0]                 twod_sd_u_i;
  logic signed  [31:0]                 twod_sd_v_i;
  logic signed  [31:0]                 twod_sd_a00_i;
  logic signed  [31:0]                 twod_sd_a01_i;
  logic signed  [31:0]                 twod_sd_a10_i;
  logic signed  [31:0]                 twod_sd_a11_i;
  logic         [2:0]                  twod_sd_format_i;
  logic         [7:0]                  twod_sd_palette_i;
  logic         [15:0]                 twod_sd_tint_i;
  logic         [1:0]                  twod_sd_blend_i;
  logic         [1:0]                  twod_sd_view_mask_i;
  logic         [7:0]                  twod_sd_order_i;
  logic         [15:0]                 twod_sd_src_id_i;
  logic                                twod_ld_page_we_i;
  logic         [TWOD_PAW-1:0]         twod_ld_page_addr_i;
  logic         [15:0]                 twod_ld_page_data_i;
  logic                                twod_ld_pal_we_i;
  logic         [TWOD_PALAW-1:0]       twod_ld_pal_addr_i;
  logic         [15:0]                 twod_ld_pal_data_i;
  logic                                twod_ld_bind_we_i;
  logic         [TWOD_BSW-1:0]         twod_ld_bind_sel_i;
  logic         [TWOD_PAW-1:0]         twod_ld_bind_base_i;
  logic         [3:0]                  twod_ld_bind_lstride_i;
  logic         [3:0]                  twod_ld_bind_lheight_i;
  logic                                twod_atm_slot_i;
  logic signed  [31:0]                 twod_line_scroll_i;
  logic                                twod_sc_valid_o;
  logic                                twod_sc_ready_i;
  logic         [15:0]                 twod_sc_rgb_o;
  logic signed  [15:0]                 twod_sc_x_o;
  logic signed  [15:0]                 twod_sc_y_o;
  logic         [15:0]                 twod_sc_tint_o;
  logic         [1:0]                  twod_sc_blend_o;
  logic         [7:0]                  twod_sc_order_o;
  logic         [15:0]                 twod_sc_src_id_o;
  logic                                twod_sc_last_o;
  logic         [31:0]                 twod_plane_pixels_o;
  logic         [31:0]                 twod_plane_refused_role_o;
  logic         [31:0]                 twod_plane_refused_blend_o;
  logic         [31:0]                 twod_plane_skipped_view_o;
  logic         [31:0]                 twod_plane_wrap_fail_o;
  logic         [31:0]                 twod_sprite_descriptors_o;
  logic         [31:0]                 twod_sprite_skipped_view_o;
  logic         [31:0]                 twod_sprite_refused_o;
  logic         [31:0]                 twod_sprite_pixels_o;
  logic         [31:0]                 twod_samples_o;
  logic         [31:0]                 twod_plane_samples_o;
  logic         [31:0]                 twod_sprite_samples_o;
  logic         [31:0]                 twod_clut8_samples_o;
  logic         [31:0]                 twod_rgb565_samples_o;
  logic         [31:0]                 twod_texel_wrapped_o;
  logic         [31:0]                 twod_page_oob_o;
  logic         [31:0]                 twod_bind_missing_o;
  logic         [31:0]                 twod_fmt_refused_o;
  logic         [31:0]                 twod_pal_refused_o;
  logic         [31:0]                 twod_skipped_fill_o;
  logic         [31:0]                 twod_atm_underrun_o;
  logic         [31:0]                 twod_walk_stalls_o;
  logic         [31:0]                 twod_sprite_stalls_o;
  logic         [31:0]                 twod_tint_unapplied_o;
  logic         [31:0]                 twod_pair_lost_o;
  // ---- THE DUT, OR ITS POSITIVE CONTROL --------------------------------
  // A PLAIN `ifdef`, SELECTED BY A PLAIN `-D`, AND THAT SHAPE IS DELIBERATE.
  // CLAUDE.md: Verilator's `-D` cannot override a FUNCTION-LIKE `define` and
  // says NOTHING when it fails to, so two mutants once passed while measuring
  // unmutated production. A bare `ifdef` selecting between two instantiations
  // is the form `-D` does reach, and the negative control is built in: without
  // the define this is production, and production must read the counter ZERO
  // under the identical stimulus.
  //
  // `tests/mutants/zhao_console_core_slot_overflow_mutant.sv` is a WRAPPER --
  // it instantiates `zhao_console_core` with TERR_POOL_SLOTS halved -- so it
  // has no copied body to go stale. Its own header carries the argument.
`ifdef ZHAO_MUT_SLOT_OVERFLOW
  zhao_console_core_slot_overflow_mutant dut (.*);
`else
  zhao_console_core dut (.*);
`endif

  // ==========================================================================
  // PACKET P-SURFACE: ONE SurfaceStamp COMMAND, AND NOTHING ELSE
  // ==========================================================================
  // WHAT THIS HARNESS IS ALLOWED TO BE, and it is a narrower licence than the
  // terrain engine below claims. The completion plan lets a harness supply
  // "host packets"; `spec/commands.zidl` carries SurfaceStamp with exactly the
  // fields driven here, so presenting one is presenting a host packet whose
  // decoder does not exist yet (DUT entry I30). What this bench does NOT do is
  // touch the seam under test: SURFACE.STAMP's request, page and write
  // channels are internal to the DUT, so every texel read and every texel
  // written below happens between two real blocks with this file nowhere in
  // the loop.
  //
  // THE NUMBERS ARE THE COMMITTED TEST'S OWN. `tests/surface/
  // surface_stamp_chain.cpp` runs this identical pair against the reference
  // with a 64x64 m envelope centred on the origin and a crack ring, and its
  // `settle()` wires the three channels port for port the same way the DUT
  // does. Reusing its envelope means a disagreement here is about the
  // COMPOSITION rather than about a stimulus nobody has validated.
  //
  // ONE metre per texel: the envelope is 64 m across a 64-texel sheet. A ring
  // of outer radius 6 and width 2 covers an annulus of a few dozen texels --
  // small on purpose, because at SQ_RADIX = 1 each texel costs ~39 cycles and
  // this is a wiring bench, not a throughput measurement.
  localparam int signed SURF_M      = 32'sh0001_0000;  // fx16 one metre
  localparam int signed SURF_ENV_LO = -32 * SURF_M;
  localparam int signed SURF_ENV_HI =  32 * SURF_M;
  localparam logic [31:0] SURF_PATCH_C = 32'h0000_2C01;  // the chain test's handle

  // `stamp_rejected_o`, `res_overflow_o` and `wr_miss_o` are ONE-CYCLE PULSES,
  // so the verdict cannot read them directly -- by the time it runs they are
  // long back at zero, and a zero read at the wrong moment is indistinguishable
  // from a pulse that never happened. They are made sticky here so a single
  // event survives to be asserted on.
  logic surf_rejected_seen_q, surf_overflow_seen_q, surf_wr_miss_seen_q;
  logic surf_done_seen_q;
  logic [31:0] surf_res_records_q;   // stamp_results beats this bench accepted

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      surf_rejected_seen_q <= 1'b0;
      surf_overflow_seen_q <= 1'b0;
      surf_wr_miss_seen_q  <= 1'b0;
      surf_done_seen_q     <= 1'b0;
      surf_res_records_q   <= 32'd0;
    end else begin
      if (surf_stamp_rejected_o) surf_rejected_seen_q <= 1'b1;
      if (surf_res_overflow_o)   surf_overflow_seen_q <= 1'b1;
      if (surf_sheet_wr_miss_o)  surf_wr_miss_seen_q  <= 1'b1;
      if (surf_stamp_done_o)     surf_done_seen_q     <= 1'b1;
      if (surf_res_valid_o && surf_res_ready_i)
        surf_res_records_q <= surf_res_records_q + 32'd1;
    end
  end

  // ---- THE FIELD ENGINE (core entry I42) ----------------------------------
  // NO PROGRAM IS LOADED HERE, and the stamp brush is therefore DISARMED:
  // `fld_stamp_slot_valid_i` is low, the core ANDs it into the stamp's
  // `cmd_field_en_i`, and the stamp runs its ABI path exactly as it did before
  // this composition. That is deliberate -- this bench's surface acceptance
  // below must measure the same stamp it measured yesterday, or the composition
  // would be hiding a regression behind a new feature.
  assign fld_ld_valid_i         = 1'b0;
  assign fld_ld_kind_i          = 2'd0;
  assign fld_ld_slot_i          = 3'd0;
  assign fld_ld_addr_i          = 7'd0;
  assign fld_ld_data_i          = 96'd0;
  assign fld_pc_lu_valid_i      = 1'b0;
  assign fld_pc_lu_hash_i       = 32'd0;
  assign fld_pc_lu_resp_ready_i = 1'b1;
  assign fld_pc_cm_valid_i      = 1'b0;
  assign fld_pc_cm_hash_i       = 32'd0;
  assign fld_pc_cm_ok_i         = 1'b0;
  assign fld_pc_cm_resp_ready_i = 1'b1;
  assign fld_stamp_slot_i       = 3'd0;
  assign fld_stamp_slot_valid_i = 1'b0;
  assign fld_req_slot_i         = 3'd0;
  assign fld_req_noprog_i       = 1'b0;
  assign fld_req_in_i           = 384'd0;
  assign fld_resp_ready_i       = 1'b1;

  // ---- THE ONE THING THIS BENCH ASKS THE ENGINE -----------------------------
  // A single request on the edge client, at a slot nothing was ever loaded
  // into. The right answer is a REFUSAL, and the point of asking is that a
  // refusal can only come back if the whole path exists: the core's port, the
  // arbiter, the slot's `hdr_loaded` bit, the response bus and the status.
  //
  // IT IS DELIBERATELY NOT A SUCCESSFUL RUN. Loading a program here would mean
  // choosing one, and a composed engine that returns a plausible number is
  // exactly the thing a smoke bench cannot tell apart from a composed engine
  // that returns a plausible number for the wrong reason. A refusal has a
  // named status and a counter, and neither can be produced by accident.
  logic fld_probe_done_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      fld_req_valid_i  <= 1'b0;
      fld_probe_done_q <= 1'b0;
    end else if (!fld_probe_done_q) begin
      if (fld_req_valid_i && fld_req_ready_o) begin
        fld_req_valid_i  <= 1'b0;
        fld_probe_done_q <= 1'b1;
      end else begin
        fld_req_valid_i <= 1'b1;
      end
    end
  end

  // I32: TERRAIN.BAKE does not exist, so this bench plays its consumer and is
  // ALWAYS READY. That is not a convenience: `s2_accept` inside SURFACE.STAMP
  // requires the RESULT beat to be taken as well as the write, so a low
  // `res_ready` would stall the pair and both texel counters would read zero
  // for a reason that has nothing to do with the seam under test.
  assign surf_res_ready_i = 1'b1;

  // The command's payload is constant; only `valid` moves. Holding the fields
  // steady is what the block's contract expects of a dispatch and it keeps the
  // handshake below the only moving part.
  assign surf_cmd_handle_i     = SURF_PATCH_C;
  assign surf_cmd_operation_i  = 8'd0;        // ABI operation 0
  assign surf_cmd_tag_i        = 8'd1;
  assign surf_cmd_strength_i   = 16'hD000;    // the low byte is discarded by law
  assign surf_cmd_tx_i         = 32'sd0;
  assign surf_cmd_ty_i         = 32'sd0;
  assign surf_cmd_radius_i     = 6 * SURF_M;
  assign surf_cmd_ring_width_i = 2 * SURF_M;  // a crack ring, not a filled disc
  assign surf_cmd_env_x0_i     = SURF_ENV_LO;
  assign surf_cmd_env_z0_i     = SURF_ENV_LO;
  assign surf_cmd_env_x1_i     = SURF_ENV_HI;
  assign surf_cmd_env_z1_i     = SURF_ENV_HI;
  assign surf_cmd_blend_en_i   = 1'b0;        // 0 = the ABI operation mapping
  assign surf_cmd_blend_i      = 3'd0;
  assign surf_cmd_age_shift_i  = 3'd1;
  assign surf_cmd_field_en_i   = 1'b0;
  assign surf_cmd_src_id_i     = 16'd4242;

  // ONE command, offered once and withdrawn on its ACCEPTANCE -- the same
  // shape every other producer in this bench uses, and for the reason recorded
  // in CLAUDE.md: a `valid` held across a whole offer window re-submitted one
  // meshlet fifteen times and every result still matched. `surf_stamps_o` is
  // asserted to be exactly 1 below, which is what catches the inverse.
  logic surf_cmd_offered_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      surf_cmd_valid_i   <= 1'b0;
      surf_cmd_offered_q <= 1'b0;
    end else begin
      if (surf_cmd_valid_i && surf_cmd_ready_o) surf_cmd_valid_i <= 1'b0;
      else if (reset_released_q && !surf_cmd_offered_q) begin
        surf_cmd_valid_i   <= 1'b1;
        surf_cmd_offered_q <= 1'b1;
      end
    end
  end

  // ==========================================================================
  // PACKET P-TERRAIN: THE PLAYED HPS BRIDGE AND MEM.GUARD
  // ==========================================================================
  // WHAT THIS IS ALLOWED TO BE. The completion plan's sentence is the licence
  // and also the limit: the harness supplies "external clocks, input events,
  // MEMORY BEHAVIOR and host packets. It does NOT supply missing lighting,
  // FIELD results, particle updates, prepared triangles or fake material
  // records." A bridge that returns the bytes at an address is memory
  // behaviour. Nothing below invents a patch, a height or a completion: every
  // terrain counter this bench checks moves only because a real block read a
  // real byte and handed a real record to the next real block.
  //
  // ONE ENGINE, because the DUT presents ONE bridge port: TERRAIN.CMD and
  // TERRAIN.PAGELOADER are merged inside the core by the real
  // `zhao_hps_arbiter`. That the two of them share this port WITHOUT either
  // starving is itself part of what this bench exercises.
  localparam logic [31:0] HPS_BASE    = 32'h2000_0000; // the staging arena
  localparam int unsigned PAGE_BYTES_C = 21376;        // terrain_rules sec 2/7
  localparam int unsigned LIST_OFF_C   = 0;
  // ==========================================================================
  // THREE RECORDS, AND THE REASON IT WAS ONE -- THE DEFECT WAS IN THIS FILE
  // ==========================================================================
  // THE HISTORY, KEPT BECAUSE THE WRONG DIAGNOSIS IS THE INSTRUCTIVE PART.
  // This bench shipped at N_TERR_REC = 1 on 2026-09-19 with a note saying that
  // at 2 or more "every record after the first arrives all-zero", and locating
  // the loss "at or before TERRAIN.SEQ's capture of the record stream, i.e. in
  // the TERRAIN.CMD -> TERRAIN.SEQ seam". The measurements quoted were real:
  //
  //   N=3:  cmd records=3, seq consumed=3, claims=3/1(same),
  //         pl loaded=0 faulted=1 refused=2, fault island=0 ix=0 iz=0 src_id=0
  //
  // and so was the reasoning from them -- a claim key is {epoch, island, ix,
  // iz}, SEQ builds it from the record, two zero keys collide, hence the extra
  // `same`. Every step of that is correct. The CONCLUSION was wrong, because
  // it took one fact for granted: that the bytes were in memory to begin with.
  //
  // THEY WERE NOT. The arena writer below declared its word index as
  // `int unsigned b = ...` inside the loop body of an `initial` block. A
  // variable declared in a begin/end of a STATIC scope has static lifetime, so
  // its initialiser runs ONCE before time zero and never again: `b` was 0 for
  // every r, all three records were written over each other at word 0, and the
  // arena held the LAST record followed by zeros. Traced at the DUT's own CMD
  // port, the very first burst of pass one reads
  //
  //   20000000: 0009000500000042 0000000020006380 00010001deadbeef 00000000000003ea
  //   20000020: 0000000000000000 0000000000000000 0000000000000000 0000000000000000
  //
  // -- one record (ix=5, iz=9, src=1002, which is r=2) and then nothing.
  // TERRAIN.CMD read exactly what was there and emitted exactly what it read;
  // TERRAIN.SEQ claimed exactly what it was handed; TERRAIN.PAGELOADER refused
  // a job whose page address was 0 with verdict 6 (V_SRC_ARENA), which is the
  // CORRECT refusal for an address outside the staging arena. Every block on
  // the spine behaved correctly on bad input, and the bad input was ours.
  //
  // AND `crc_fails=0` IS WHY IT LOOKED LIKE THE TREE'S FAULT. That zero was
  // quoted as proof that CMD "had every byte of every record in hand". It is
  // not: this bench seals the list with the SAME `zhao_crc32c_fold` over the
  // SAME words it wrote, so the two sides of that comparison were corrupted
  // together and it could not fire. The declaration below already says this
  // bench is "NO EVIDENCE WHATEVER about the CRC itself"; the note that shipped
  // used it as evidence about the RECORDS anyway. A detector wired to two
  // operands that move together cannot fire (CLAUDE.md) -- and the reassuring
  // reading is what let the fault be handed to the spine.
  //
  // WHAT GUARDS IT NOW: `automatic` on the index, a page address per record
  // rather than a shared one, and an independent read-back of the arena that
  // $fatals at time zero naming the record. See the layout loop.
  //
  // N_TERR_REC IS THE KNOB AND THREE IS ITS FLOOR, not a decoration: one record
  // is what hid this, and it takes three to make the directory's `same` answer
  // and the second re-request of an abandoned burst both happen.
  localparam int unsigned N_TERR_REC   = 3;
  localparam int unsigned LIST_BYTES_C = N_TERR_REC * 32;
  localparam int unsigned PAGE0_OFF_C  = 4096;
  // THE WINDOW IS DERIVED, NOT TYPED. It was a hand-written 64 KiB while the
  // records shared one page; a per-record page makes the arena a function of
  // N_TERR_REC, and a constant somebody has to remember to raise alongside it
  // is a constant that will be wrong. The last page ends exactly at the arena
  // end, which TERRAIN.PAGELOADER allows: its test is `src_hi > arena_hi`.
  localparam int unsigned ARENA_BYTES_C = PAGE0_OFF_C + N_TERR_REC * PAGE_BYTES_C;
  localparam int unsigned HPS_WORDS     = (ARENA_BYTES_C + 7) / 8;

  logic [63:0] hps_mem [0:HPS_WORDS-1];

  // The frozen sim profile the bridge's own comment names: a latency to first
  // beat, then one beat per cycle. Held as a small state machine rather than a
  // combinational echo, because a zero-latency bridge hides every handshake
  // bug this composition could contain.
  localparam int unsigned HPS_LAT = 16;

  int unsigned  hps_state_qq;    // 0 idle, 1 waiting, 2 streaming
  int unsigned  hps_wait_qq;
  int unsigned  hps_beats_qq;    // beats still to send
  logic [31:0]  hps_addr_qq;
  int unsigned  hps_bursts_served_q;
  logic         hps_grant_pulse_q;
  assign terr_hps_grant_i = hps_grant_pulse_q;
  // This played bridge accepts no write burst (see the write arm below), so
  // its write-acceptance level is never high.
  assign terr_hps_wr_ready_i = 1'b0;

  function automatic logic [63:0] hps_read(input logic [31:0] byte_addr);
    int unsigned w;
    if (byte_addr < HPS_BASE) return 64'd0;
    w = (byte_addr - HPS_BASE) >> 3;
    if (w >= HPS_WORDS) return 64'd0;
    return hps_mem[w];
  endfunction

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      hps_state_qq        <= 0;
      hps_wait_qq         <= 0;
      hps_beats_qq        <= 0;
      hps_addr_qq         <= 32'd0;
      hps_grant_pulse_q   <= 1'b0;
      hps_bursts_served_q <= 0;
      terr_hps_rsp_i      <= '{beat_valid: 1'b0, data: 64'd0, last: 1'b0, err: 1'b0};
    end else begin
      hps_grant_pulse_q <= 1'b0;
      terr_hps_rsp_i    <= '{beat_valid: 1'b0, data: 64'd0, last: 1'b0, err: 1'b0};

      case (hps_state_qq)
        0: if (terr_hps_req_o.valid && !terr_hps_req_o.write) begin
             // A READ burst. `len` is BYTES (1..64); the bridge answers in
             // 64-bit beats, so the beat count is len/8 and a len that is not
             // a multiple of 8 would be a malformed burst -- reported as `err`
             // rather than rounded, because rounding invents bytes.
             hps_grant_pulse_q <= 1'b1;
             hps_addr_qq       <= terr_hps_req_o.addr;
             if ((terr_hps_req_o.len == 7'd0) || (terr_hps_req_o.len[2:0] != 3'd0)) begin
               terr_hps_rsp_i <= '{beat_valid: 1'b0, data: 64'd0, last: 1'b0, err: 1'b1};
             end else begin
               hps_beats_qq <= terr_hps_req_o.len >> 3;
               hps_wait_qq  <= HPS_LAT;
               hps_state_qq <= 1;
             end
           end else if (terr_hps_req_o.valid && terr_hps_req_o.write) begin
             // Nothing in this BENCH can write over the bridge -- the only
             // terrain writer is TERRAIN.WRITEBACK, composed since entry I28
             // closed, and it cannot be reached here (see the doorbell's
             // initialisation). Granting and dropping would be a lie; this
             // reports a bridge error so a write that appears here is LOUD,
             // and `terr_hps_wr_ready_i` below is never raised.
             hps_grant_pulse_q <= 1'b1;
             terr_hps_rsp_i <= '{beat_valid: 1'b0, data: 64'd0, last: 1'b0, err: 1'b1};
           end
        1: if (hps_wait_qq > 1) hps_wait_qq <= hps_wait_qq - 1;
           else                 hps_state_qq <= 2;
        2: begin
             terr_hps_rsp_i <= '{beat_valid: 1'b1,
                                 data:       hps_read(hps_addr_qq),
                                 last:       (hps_beats_qq == 1),
                                 err:        1'b0};
             hps_addr_qq  <= hps_addr_qq + 32'd8;
             hps_beats_qq <= hps_beats_qq - 1;
             if (hps_beats_qq == 1) begin
               hps_state_qq        <= 0;
               hps_bursts_served_q <= hps_bursts_served_q + 1;
             end
           end
        default: hps_state_qq <= 0;
      endcase
    end
  end

  // ---- the SHELL's HPS port: the upload arena (2026-09-19, cmdmem) ----------
  // Until this block the shell's own HPS pins were tied low and nothing crossed
  // them. MEM.UPLOAD now reaches HPS DDR through the shell's REAL
  // `zhao_hps_bridge` and `zhao_hps_arbiter_n` (the TERRAIN.BUILD socket), so
  // the bench plays the far side of that bridge exactly as it plays the
  // terrain spine's: a grant, a fixed latency, then len/8 beats of memory.
  //
  // IT MODELS ONLY THE UPLOAD ARENA, and says so loudly. Any other request
  // over these pins -- a CMD.DMA packet fetch, a write -- is something this
  // bench has no bytes for, and serving zeros would be inventing a packet.
  localparam logic [31:0] UPL_ARENA_C   = 32'h3000_0000;
  localparam int unsigned UPL_WORDS_C   = 32;               // 256 B, four bursts
  // The top 4 KiB of TERRAIN.PAGE_POOL -- the only window MEM.GUARD's
  // TERRAIN_BUILD arm admits -- well clear of the pages the spine loads.
  localparam logic [31:0] UPL_REGION_C  = 32'h054D_F000;
  localparam logic [31:0] UPL_REGION_SZ = 32'h0000_1000;
  logic [63:0] upl_mem [0:UPL_WORDS_C-1];

  int unsigned  sh_state_q, sh_wait_q, sh_beats_q, sh_bursts_q;
  logic [31:0]  sh_addr_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      sh_state_q      <= 0;
      sh_wait_q       <= 0;
      sh_beats_q      <= 0;
      sh_bursts_q     <= 0;
      sh_addr_q       <= 32'd0;
      hps_req_grant_i <= 1'b0;
      hps_rd_valid_i  <= 1'b0;
      hps_rd_data_i   <= 64'd0;
      hps_rd_last_i   <= 1'b0;
    end else begin
      hps_req_grant_i <= 1'b0;
      hps_rd_valid_i  <= 1'b0;
      hps_rd_last_i   <= 1'b0;
      case (sh_state_q)
        0: if (hps_req_valid_o) begin
             if (hps_req_write_o || (hps_req_addr_o < UPL_ARENA_C) ||
                 (hps_req_addr_o >= UPL_ARENA_C + 32'(UPL_WORDS_C * 8)))
               $fatal(1, "SMOKE: the shell's HPS port was asked for %s at %08x -- this bench plays only MEM.UPLOAD's arena and has no bytes to answer with",
                      hps_req_write_o ? "a WRITE" : "a read", hps_req_addr_o);
             hps_req_grant_i <= 1'b1;
             sh_addr_q       <= hps_req_addr_o;
             sh_beats_q      <= hps_req_len_o >> 3;
             sh_wait_q       <= HPS_LAT;
             sh_state_q      <= 1;
           end
        1: if (sh_wait_q > 1) sh_wait_q <= sh_wait_q - 1;
           else               sh_state_q <= 2;
        2: begin
             hps_rd_valid_i <= 1'b1;
             hps_rd_data_i  <= upl_mem[(sh_addr_q - UPL_ARENA_C) >> 3];
             hps_rd_last_i  <= (sh_beats_q == 1);
             sh_addr_q      <= sh_addr_q + 32'd8;
             sh_beats_q     <= sh_beats_q - 1;
             if (sh_beats_q == 1) begin
               sh_state_q  <= 0;
               sh_bursts_q <= sh_bursts_q + 1;
             end
           end
        default: sh_state_q <= 0;
      endcase
    end
  end

  // ---- ONE UPLOAD, offered once the console is out of reset ----------------
  // Its request fields are set with the other configuration before reset; the
  // valid is raised here and dropped on the handshake. What comes back is
  // recorded, and checked at the end against the request that caused it.
  logic         upl_fired_q;
  int unsigned  upl_done_seen_q, upl_pub_seen_q;
  logic [7:0]   upl_status_seen_q;
  logic [7:0]   upl_pub_slot_q, upl_pub_tag_q;
  logic [15:0]  upl_pub_gen_q;
  logic [23:0]  upl_pub_index_q;
  logic [31:0]  upl_pub_base_q, upl_pub_extent_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      upl_req_valid_i   <= 1'b0;
      upl_fired_q       <= 1'b0;
      upl_done_seen_q   <= 0;
      upl_pub_seen_q    <= 0;
      upl_status_seen_q <= 8'hFF;
      upl_pub_slot_q    <= '0;
      upl_pub_tag_q     <= '0;
      upl_pub_gen_q     <= '0;
      upl_pub_index_q   <= '0;
      upl_pub_base_q    <= '0;
      upl_pub_extent_q  <= '0;
    end else begin
      if (reset_released_q && !upl_fired_q && !upl_req_valid_i) upl_req_valid_i <= 1'b1;
      if (upl_req_valid_i && upl_req_ready_o) begin
        upl_req_valid_i <= 1'b0;
        upl_fired_q     <= 1'b1;
      end
      if (upl_done_o) begin
        upl_done_seen_q   <= upl_done_seen_q + 1;
        upl_status_seen_q <= upl_status_o;
      end
      if (upl_publish_valid_o) begin
        upl_pub_seen_q   <= upl_pub_seen_q + 1;
        upl_pub_slot_q   <= upl_publish_slot_o;
        upl_pub_tag_q    <= upl_publish_tag_o;
        upl_pub_gen_q    <= upl_publish_generation_o;
        upl_pub_index_q  <= upl_publish_index_o;
        upl_pub_base_q   <= upl_publish_base_o;
        upl_pub_extent_q <= upl_publish_extent_o;
      end
    end
  end

  // ---- the played MEM.GUARD write window -----------------------------------
  // TERRAIN.PAGELOADER writes the page into TERRAIN.PAGE_POOL through a guard
  // client. The real `zhao_mem_guard` gives ZHAO_CLIENT_TERRAIN_BUILD a
  // write-only window over that pool; this model accepts inside the pool and
  // DENIES outside it, rather than accepting everything -- an always-ok guard
  // would make `terr_pl_guard_denied_o` a counter that cannot move, which is
  // the decoration this project's own rules forbid.
  localparam logic [ZHAO_VRAM_ADDR_BITS-1:0] POOL_BASE_C  = 27'h400_0000;
  localparam int unsigned                    POOL_SLOTS_C = 1024;

  wire guard_in_pool = (terr_guard_req_o.addr >= POOL_BASE_C) &&
                       (terr_guard_req_o.addr <
                          POOL_BASE_C + (POOL_SLOTS_C * PAGE_BYTES_C));

  // THE VERDICT IS A PULSE ONE CYCLE AFTER THE REQUEST, NOT A LEVEL WITH IT,
  // and getting that wrong cost a diagnosis that looked exactly like an RTL
  // deadlock. `zhao_terrain_pageloader` splits the handshake across two states
  // and its own comments are the specification:
  //
  //   S_GREQ:  "`ready` is a LEVEL and moves the machine on. `ok` is NOT
  //             tested here."
  //   S_GVERD: "...it is tested HERE, one cycle later, where the guard
  //             PULSES it."
  //
  // The first version of this model drove `ready`, `ok` and `violation`
  // together, combinationally, off `guard_req_o.valid`. By the time the loader
  // reached S_GVERD the request had dropped, so `ok` was low and it waited in
  // S_GVERD forever. The symptom was a spine that carried records, claims and
  // load jobs perfectly and then a loader that retired ZERO bytes with
  // `pages_refused`, `guard_denied` and `bridge_errs` all clean -- which reads
  // as a fault in the composition and was a fault in the harness. It was found
  // by probing the DUT's own edge (`guard_req_cycles=1, guard_wbeats=0`), not
  // by reading either file again.
  logic guard_verd_q, guard_ok_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      guard_verd_q <= 1'b0;
      guard_ok_q   <= 1'b0;
    end else begin
      guard_verd_q <= terr_guard_req_o.valid;
      guard_ok_q   <= guard_in_pool;
    end
  end

  always_comb begin
    terr_guard_rsp_i.ready     = terr_guard_req_o.valid;
    terr_guard_rsp_i.ok        = guard_verd_q &&  guard_ok_q;
    terr_guard_rsp_i.violation = guard_verd_q && !guard_ok_q;
  end

  // ALWAYS READY ON THE WRITE CHANNEL, stated as a limit rather than left
  // implicit: this model never backpressures the page write, so the loader's
  // write-stall path is NOT exercised here. `tests/terrain/tb_pageloader.sv`
  // is where that is tested; this bench is about the seams between blocks.
  assign terr_guard_wready_i = 1'b1;

  // ---- WHERE THE SPINE GOT TO, watched at the DUT's own edge ---------------
  // Every signal here is a real port of zhao_console_core, so this needs no
  // hierarchical reach into the loader and stays valid however Verilator
  // partitions the design. It answers the one question a stalled block cannot:
  // which handshake did it reach, and which did it not.
  int unsigned pr_hps_req_cy, pr_hps_beats, pr_guard_req_cy, pr_guard_wbeats;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      pr_hps_req_cy   <= 0;
      pr_hps_beats    <= 0;
      pr_guard_req_cy <= 0;
      pr_guard_wbeats <= 0;
    end else begin
      if (terr_hps_req_o.valid)                        pr_hps_req_cy   <= pr_hps_req_cy + 1;
      if (terr_hps_rsp_i.beat_valid)                   pr_hps_beats    <= pr_hps_beats + 1;
      if (terr_guard_req_o.valid)                      pr_guard_req_cy <= pr_guard_req_cy + 1;
      if (terr_guard_wvalid_o && terr_guard_wready_i)  pr_guard_wbeats <= pr_guard_wbeats + 1;
    end
  end

  // ---- folding the list's CRC with the PRODUCTION folder -------------------
  // WHAT THIS IS AND IS NOT EVIDENCE FOR. `zhao_terrain_cmd` verifies the list
  // in a first pass and emits records only in a second, so a list whose CRC is
  // wrong produces NO records and this bench would prove nothing about the
  // spine. The harness therefore has to hand it a well-formed packet, and it
  // folds the bytes with the SAME `zhao_crc32c_fold` the block uses.
  //
  // That makes this bench NO EVIDENCE WHATEVER about the CRC itself -- both
  // sides would agree on an identical mistake. `tests/terrain/tb_terrain_cmd.sv`
  // is where the CRC is tested. What this bench shows is the thing it can
  // honestly show: a VALID list produces records, and those records reach
  // TERRAIN.SEQ.
  logic [31:0] fold_c_i;
  logic [63:0] fold_d_i;
  logic [3:0]  fold_n_i;
  logic [31:0] fold_c_o;
  zhao_crc32c_fold u_bench_fold (.c_i(fold_c_i), .d_i(fold_d_i), .n_i(fold_n_i), .c_o(fold_c_o));

  // ==========================================================================
  // CLOCKS. The frozen ratios (plan R1): vid = gpu/2, audio = gpu/4, with
  // COINCIDENT POSEDGES. Written as three explicit waveforms rather than as a
  // divider, because a divider built out of always_ff puts each derived edge a
  // delta after the gpu edge and the shell's CDC law is written against
  // coincident ones.
  //
  //   gpu   posedge at 4, 12, 20, 28, 36, ...   (period 8)
  //   vid   posedge at 4, 20, 36, ...           (every 2nd gpu posedge)
  //   audio posedge at 4, 36, ...               (every 4th gpu posedge)
  // ==========================================================================
  initial begin
    gpu_clk = 1'b0;
    vid_clk = 1'b0;
    audio_clk = 1'b0;
  end
  always begin #4ns gpu_clk   = 1'b1;  #4ns gpu_clk   = 1'b0; end
  always begin #4ns vid_clk   = 1'b1;  #8ns vid_clk   = 1'b0; #4ns; end
  always begin #4ns audio_clk = 1'b1; #16ns audio_clk = 1'b0; #12ns; end

  // ==========================================================================
  // OBSERVATION. Every one of these only moves when a real beat crosses a real
  // wire; none of them is written by the bench.
  // ==========================================================================
  int unsigned cycles_q;
  int unsigned ticks_seen_q;
  int unsigned part_records_sent_q;
  int unsigned part_records_written_q;
  logic        render_frame_open_q;
  bit          part_tick_seen_q;
  bit          reset_released_q;

  // ---- the spawn-on-collision acceptance, watched on the WRITE STREAM -------
  // Ruling I4 (`reports/RULING-I4-COLLISION-SPAWN-20260919.md`) says a
  // collision-spawned child is placed POST-CONTACT. On a flat heightfield with
  // a STICK response the post-contact position is exact -- h + CLEAR_EPS,
  // vertically, with no rounding at all -- so the expected value is a hand
  // computed literal and not a re-implementation of the collider.
  //
  // The child is recognised by kPartBornThisTick, which PART.SPAWN writes and
  // nothing else in this core does.
  localparam int unsigned PART_KEY_BORN_BIT = 118;  // OFF_FLG 116 + kPartBornThisTick
  localparam int signed   PART_CONTACT_Y    = PART_TER_H + PART_CLEAR_EPS;

  int unsigned part_children_seen_q;
  int unsigned part_children_at_contact_q;

  always @(posedge gpu_clk) begin
    cycles_q <= cycles_q + 1;
    if (reset_released_q) begin
      if (gpu_tick_o)                             ticks_seen_q     <= ticks_seen_q + 1;
      if (part_tick_busy_o)                       part_tick_seen_q <= 1'b1;
      if (part_wr_valid_o && part_wr_ready_i) begin
        part_records_written_q <= part_records_written_q + 1;
        if (part_wr_record_o[PART_KEY_BORN_BIT]) begin
          part_children_seen_q <= part_children_seen_q + 1;
          // pos.y is the second 18-bit field of the ratified record.
          if ($signed(part_wr_record_o[35:18]) == PART_POS_W'(PART_CONTACT_Y))
            part_children_at_contact_q <= part_children_at_contact_q + 1;
        end
      end
    end
  end

  // ==========================================================================
  // PART.TABLE'S LOAD -- THE BENCH IS THE HOST, AND SAYS SO.
  //
  // Core entries I2 and I3 closed on 2026-09-19: the console instantiates
  // `zhao_part_table` and answers its own descriptor reads. Nothing in the core
  // FILLS that table -- CMD.SCHEDULER has no path to it (entry I33) -- so the
  // load is a boundary and THIS BENCH IS STANDING IN FOR THE HOST. That is a
  // declared limit, not a hidden one: what is proven below is that a descriptor
  // written through `part_tbl_ld_*` comes back out of three different consumers,
  // and NOT that any real agent writes it.
  //
  // It handshakes on `ld_ready_o` rather than assuming it. The port is constant
  // high by design and its own file says so -- but a bench that ignores a ready
  // it was given is a bench that would not notice the day it stopped being
  // constant, and this one is the only host the table has.
  //
  // THE WORD MAP IS A COPY, and copies go stale in the flattering direction, so
  // the authority is named: `fpga/rtl/particles/zhao_part_table.sv`, section
  // "LOAD WORD MAP". What protects this copy is not diligence -- it is that a
  // mispacked word puts the wrong value in every field, and the three
  // behavioural checks below (a STICK contact, one child per collision, and an
  // exact colour byte at the boundary) all fail loudly when it does. A packing
  // bug here cannot pass quietly.
  // ==========================================================================
  localparam logic [1:0] TBL_SEL_UPD = 2'd0;
  localparam logic [1:0] TBL_SEL_COL = 2'd1;
  localparam logic [1:0] TBL_SEL_SPW = 2'd2;
  localparam logic [1:0] TBL_SEL_CRV = 2'd3;

  // update slice: recipe 4 | lifetime 10 | age_mark 10 | drag 8 | grav 11 |
  //               strength 11 | cx 18 | cy 18 | cz 18 | p0 11 | p1 11 | p2 11
  localparam int unsigned TBL_U_OFF_RCP = 0;
  localparam int unsigned TBL_U_OFF_LIF = TBL_U_OFF_RCP + 4;
  localparam int unsigned TBL_U_OFF_MRK = TBL_U_OFF_LIF + PART_AGE_W;
  localparam int unsigned TBL_U_OFF_DRG = TBL_U_OFF_MRK + PART_AGE_W;
  // collide slice: response 3 | restitution 16 | friction 16 | damping 16
  localparam int unsigned TBL_C_OFF_RSP = 0;
  // spawn rule: child species 7 | count 5 | known 1
  localparam int unsigned TBL_S_OFF_CHD = 0;
  localparam int unsigned TBL_S_OFF_CNT = TBL_S_OFF_CHD + 7;
  localparam int unsigned TBL_S_OFF_KNW = TBL_S_OFF_CNT + 5;
  // curve entry: size 6 | colour 8
  localparam int unsigned TBL_V_OFF_SIZ = 0;
  localparam int unsigned TBL_V_OFF_CLR = TBL_V_OFF_SIZ + 6;

  // R_COLOUR, the tenth id of owner ruling 2026-08-31's CLOSED vocabulary. It
  // is chosen because it is the one recipe whose visible effect is a TABLE READ
  // and not arithmetic: its multiplier operand stays at zero, so the particle's
  // motion is identical to the all-zero descriptor this bench used to drive,
  // and the ONLY difference at the boundary is that `part_colour_o` now carries
  // the curve's colour byte. That makes the I3 check a read-back rather than an
  // inference.
  localparam logic [3:0] TBL_RECIPE_COLOUR = 4'd10;
  localparam logic [7:0] TBL_CURVE_COLOUR  = 8'hA5;
  localparam logic [5:0] TBL_CURVE_SIZE    = 6'h2A;

  task automatic tbl_load(input logic [1:0] sel,
                          input logic [6:0] idx,
                          input logic [1:0] ev,
                          input logic [PART_TBL_LD_W-1:0] data);
    int unsigned g;
    part_tbl_ld_sel_i   = sel;
    part_tbl_ld_index_i = idx;
    part_tbl_ld_event_i = ev;
    part_tbl_ld_data_i  = data;
    part_tbl_ld_valid_i = 1'b1;
    g = 0;
    while (!part_tbl_ld_ready_o && (g < 100)) begin
      @(posedge gpu_clk);
      g++;
    end
    if (g >= 100)
      $fatal(1, "SMOKE: PART.TABLE never raised ld_ready_o -- its load port is not reachable from the boundary");
    @(posedge gpu_clk);
    part_tbl_ld_valid_i = 1'b0;
    part_tbl_ld_data_i  = '0;
  endtask

  // The colour byte, captured where it leaves the module, ONE SAMPLE PER
  // PARTICLE.
  //
  // `part_colour_en_o` and `part_colour_o` are REGISTERED LEVELS, not pulses:
  // PART.UPDATE loads them on a retire and they stand until the next one. The
  // first version of this counter sampled the level every cycle and reported
  // 253,517 "beats" for six particles -- a number that is not wrong so much as
  // about the wrong thing, and it would have made a single correct particle
  // look like a quarter of a million confirmations. So the sample is qualified
  // on `part_updated_o` MOVING, which is exactly one event per updated
  // particle, and the count that comes out is comparable with the six records
  // this bench offers.
  int unsigned part_colour_beats_q;
  int unsigned part_colour_wrong_q;
  logic [7:0]  part_colour_last_q;
  logic [31:0] part_updated_prev_q;

  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      part_colour_beats_q <= 0;
      part_colour_wrong_q <= 0;
      part_colour_last_q  <= '0;
      part_updated_prev_q <= '0;
    end else begin
      part_updated_prev_q <= part_updated_o;
      if (part_updated_o !== part_updated_prev_q) begin
        if (part_colour_en_o) begin
          part_colour_beats_q <= part_colour_beats_q + 1;
          part_colour_last_q  <= part_colour_o;
          if (part_colour_o !== TBL_CURVE_COLOUR)
            part_colour_wrong_q <= part_colour_wrong_q + 1;
        end else begin
          // A retire with the enable LOW is a particle whose recipe did not
          // come back as R_COLOUR. Counted as wrong rather than skipped: a
          // silent skip is how "it was right every time" becomes "it was right
          // the one time it happened to be looked at".
          part_colour_wrong_q <= part_colour_wrong_q + 1;
        end
      end
    end
  end

  // --------------------------------------------------------------------------
  // THE GENERATION STORE (entry I1: harness = memory).
  //
  // It offers N_PART_RECORDS particle128 records and then stops. The records
  // are distinct so a swap would be visible, and they are DATA, not a
  // workload: every field the blocks act on comes from the species descriptor,
  // which this bench holds at its benign values.
  // --------------------------------------------------------------------------
  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      part_records_sent_q <= 0;
      part_rd_valid_i     <= 1'b0;
      part_rd_last_i      <= 1'b0;
      part_rd_record_i    <= '0;
    end else begin
      if (part_rd_valid_i && part_rd_ready_o) begin
        part_records_sent_q <= part_records_sent_q + 1;
        part_rd_valid_i     <= 1'b0;
        part_rd_last_i      <= 1'b0;
      end
      if (part_tick_busy_o && !part_rd_valid_i &&
          (part_records_sent_q < N_PART_RECORDS)) begin
        part_rd_valid_i  <= 1'b1;
        part_rd_last_i   <= (part_records_sent_q == N_PART_RECORDS - 1);
        // position X = the ordinal, so the records are distinguishable;
        // everything else zero, which is a legal particle128.
        part_rd_record_i <= {{(PART_REC_W-18){1'b0}},
                             18'(part_records_sent_q + 1)};
      end
    end
  end

  // --------------------------------------------------------------------------
  // THE GEOMETRY VERTEX STREAM.
  //
  // MOVED ONE BLOCK UPSTREAM, 2026-09-19. This used to drive `geom_skin_v_*`
  // directly, because GEOM.VDECODE was not composed and GEOM.SKIN's vertex port
  // was at the module edge. It is now composed, so the harness presents what a
  // harness is allowed to present -- BYTES -- and the decoder makes the vertex.
  // The path under test is therefore VDECODE -> SKIN -> GROUP_SEQ -> the shared
  // projector, and `geom_vd_vertices_o` and `geom_skin_vertices_transformed_o`
  // must BOTH move or the new block is passing nothing on.
  //
  // THE RECORD IS THE REAL FORMAT-0 LAYOUT (zhao_geom_vdecode.sv's header):
  //   off  0  12  position s32 x3, fx16
  //   off 12   3  normal s8 x3
  //   off 15   1  w0, 1/64 quanta (64 == rigid)
  //   off 16   4  UV, 2 x s16
  //   off 20   2  bone0
  //   off 22   2  bone1
  //   off 24   8  reserved, MUST BE ZERO -- a nonzero byte here is a REFUSAL,
  //               which is why this is packed field by field rather than
  //               smeared with a pattern.
  // Little-endian, byte 0 at bit 0, so the concatenation runs high byte first.
  // --------------------------------------------------------------------------
  function automatic logic [255:0] vdec_record(input int unsigned n);
    logic signed [31:0] px, py, pz;
    begin
      // The fixture's positions (smoke_geom_fixture.svh), fx16 world = model:
      // no pose is decoded (I29), so the identity bind pose applies.
      px = SGF_VX[n];
      py = SGF_VY[n];
      pz = SGF_VZ[n];
      vdec_record = {64'd0,          // off 24..31 reserved, MUST be zero
                     16'd0,          // off 22 bone1 (== bone0 -> rigid)
                     16'd0,          // off 20 bone0
                     16'sd0,         // off 18 v
                     16'sd0,         // off 16 u
                     8'd64,          // off 15 w0 == 64 -> rigid
                     8'sd0,          // off 14 nz
                     8'sd0,          // off 13 ny
                     8'sd127,        // off 12 nx
                     pz, py, px};    // off 0..11 position
    end
  endfunction

  // ==========================================================================
  // THE SAME RECORDS, THROUGH THE MACHINE INSTEAD OF AROUND IT.
  //
  // `vdec_record` above is unchanged and is still the fixture. What changed is
  // WHERE IT IS PUT: it used to be driven onto `geom_vd_v_bytes_i`, a port that
  // no longer exists, and it is now written into SDRAM, fetched by
  // GEOM.ASSETFETCH through the real MEM.GUARD, MEM.VRAM.ARBITER and
  // `zhao_sdram_ctrl`, and handed to GEOM.VDECODE inside the core.
  //
  // THAT IS THE WHOLE POINT AND IT IS WHY THE BYTES DID NOT CHANGE. If the
  // decoded count still comes out at N_GEOM_VERTS with the identical records,
  // the difference between the old reading and the new one is exactly the
  // memory path -- a descriptor read, a cull, an index run and a vertex run --
  // and nothing else. A new fixture would have made the comparison useless.
  //
  // THE MAP, all pool-relative to ZHAO_RENDER_ASSET_BASE = 0x06A0_0000:
  //     +0x000   64 B  the meshlet descriptor        (64-byte aligned, ruled)
  //     +0x080   64 B  the index run, 3 u8           ( 8-byte aligned, ruled)
  //     +0x100  128 B  four 32-byte vertex records   (32-byte aligned, ruled)
  // The three do not overlap, and the descriptor names the other two in its
  // own `vertex_offset`/`index_offset` fields rather than the bench naming them
  // twice: a fixture that agrees with itself by construction cannot drift.
  // ==========================================================================
  localparam int unsigned GEOM_POOL_BASE = 32'h06A0_0000;
  localparam int unsigned GEOM_DESC_OFF  = 32'h0000_0000;
  localparam int unsigned GEOM_IX_OFF    = 32'h0000_0080;
  localparam int unsigned GEOM_VX_OFF    = 32'h0000_0100;
  // The bound, in the meshlet's own space, and the identity instance transform
  // that carries it to world. It sits at the clip origin under the identity
  // camera written below, so GEOM.CULL must call it VISIBLE -- and its radius
  // is NONZERO because GEOM.MESHFETCH's seventh refusal row rejects a zero
  // radius on a meshlet that claims triangles.
  localparam logic [31:0] GEOM_BOUND_R   = 32'h0000_4000;   // 0.25 in fx16

  logic        geom_poke_en;
  logic [25:0] geom_poke_waddr;
  logic [15:0] geom_poke_data;
  logic        geom_peek_en;
  logic [25:0] geom_peek_waddr;
  logic [15:0] geom_peek_data;
  logic [5:0]  geom_model_err;
  logic        geom_model_error;
  bit          geom_fixture_ready_q;
  bit          geom_camera_ready_q;
  bit          geom_job_sent_q;

  // ONE 16-BIT WORD PER CLOCK, at `byte_addr >> 1`, low byte of the word first
  // -- `zhao_sdram_ctrl` computes `waddr = req.addr[26:1]` and the shell's read
  // packer rebuilds four words per 64-bit beat low group first, so this is the
  // same little-endian order in both directions.
  task automatic geom_poke_w(input int unsigned byte_addr, input logic [15:0] d);
    begin
      geom_poke_en    = 1'b1;
      geom_poke_waddr = 26'(byte_addr >> 1);
      geom_poke_data  = d;
      @(posedge gpu_clk);
      geom_poke_en    = 1'b0;
    end
  endtask

  // CRC32C (reflected Castagnoli, all-ones seed, final complement) over the
  // first 60 bytes of an eight-word descriptor, low byte of word 0 first. A
  // BENCH-SIDE bitwise loop, deliberately NOT the RTL fold, so the fixture and
  // `zhao_geom_desc_crc` are two implementations that must agree.
  function automatic logic [31:0] fixture_desc_crc(input logic [63:0] w [8]);
    logic [31:0] c;
    logic [7:0]  b;
    c = 32'hFFFF_FFFF;
    for (int unsigned i = 0; i < 60; i = i + 1) begin
      b = w[i / 8][8 * (i % 8) +: 8];
      c = c ^ {24'd0, b};
      for (int unsigned k = 0; k < 8; k = k + 1)
        c = c[0] ? ((c >> 1) ^ 32'h82F6_3B78) : (c >> 1);
    end
    return ~c;
  endfunction

  task automatic geom_poke_q(input int unsigned byte_addr, input logic [63:0] d);
    begin
      geom_poke_w(byte_addr + 0, d[15:0]);
      geom_poke_w(byte_addr + 2, d[31:16]);
      geom_poke_w(byte_addr + 4, d[47:32]);
      geom_poke_w(byte_addr + 6, d[63:48]);
    end
  endtask

  // The camera, written into the ONE matrix bank the projector and GEOM.CULL
  // share. It is IDENTITY, and it is written rather than left at zero because a
  // cull verdict that happens to come out VISIBLE under an unwritten bank is an
  // accident this bench would be resting on. Under identity the clip point is
  // the world point and w is 1.0, so a bound at the origin is inside every
  // plane by construction and the check below is about the WIRE.
  // BOTH VIEWS, from the fixture: the w = z camera (SGF_MAT) and each view's
  // viewport rectangle at cfg 16/17 -- the host port is entry I14's boundary,
  // so this bench stands in for the host exactly as it did for view 0 alone.
  // The profile (cfg 18) is left at its reset WORLD_LONG, which is what the
  // reference-derived depths assume.
  task automatic geom_write_camera();
    int unsigned i;
    int unsigned v;
    begin
      for (v = 0; v < 2; v = v + 1) begin
        for (i = 0; i < 18; i = i + 1) begin
          proj_cfg_we_i   = 1'b1;
          proj_cfg_view_i = v[0];
          proj_cfg_addr_i = 5'(i);
          if (i < 16)       proj_cfg_data_i = SGF_MAT[i];
          else if (i == 16) proj_cfg_data_i = (v == 0) ? SGF_VP0_ORG : SGF_VP1_ORG;
          else              proj_cfg_data_i = (v == 0) ? SGF_VP0_EXT : SGF_VP1_EXT;
          @(posedge gpu_clk);
        end
      end
      proj_cfg_we_i = 1'b0;
    end
  endtask

  initial begin
    geom_poke_en         = 1'b0;
    geom_poke_waddr      = '0;
    geom_poke_data       = '0;
    geom_peek_en         = 1'b0;
    geom_peek_waddr      = '0;
    geom_fixture_ready_q = 1'b0;
    @(posedge gpu_clk);

    // ---- the 64-byte meshlet descriptor, field by field -------------------
    //   +0  format u8         (must equal the job's j_format_i)
    //   +1  flags u8
    //   +2  vertex_count u8   (<= 64)
    //   +3  triangle_count u8 (<= 126)
    //   +4  material_id u16
    //   +8  bound centre x, y, z  i32 fx16
    //   +20 bound radius u32      (nonzero, or refusal row 7)
    //   +24 vertex_offset u32     pool-relative bytes
    //   +28 index_offset  u32     pool-relative bytes
    //   +32 generation u16        (must equal the job's j_generation_i)
    //   +36..59 reserved, ALL ZERO or refusal row 6
    //   +60 CRC32C over bytes 0..59, little-endian. READ BY THE RTL since
    //       2026-09-19: `zhao_geom_desc_crc` (entry I37, closed) folds the
    //       returning beats and GEOM.MESHFETCH refuses on a mismatch. The word
    //       is computed HERE, over exactly the bytes written, so the
    //       `-BadDescriptor` control still trips the RESERVED row it names and
    //       not the CRC row.
    begin : g_desc
      logic [63:0] dw [8];
      dw[0] = {16'h0000, 16'h0001, 8'(SGF_N_TRIS), 8'(N_GEOM_VERTS), 8'd0, 8'd1};
      dw[1] = 64'd0;
      // Bound centre z = 1.5 (bytes 16..19), inside the w = z camera's view,
      // so GEOM.CULL keeps the meshlet in both views; radius 0.25 (20..23).
      dw[2] = {GEOM_BOUND_R, 32'h0001_8000};
      dw[3] = {GEOM_IX_OFF, GEOM_VX_OFF};
      dw[4] = {32'd0, 16'd0, 16'd1};
`ifdef ZHAO_SMOKE_BAD_DESC
      // POSITIVE CONTROL, INVERTED POLARITY (`-BadDescriptor`). Byte 40 is
      // inside the descriptor's reserved span 36..59, which GEOM.MESHFETCH's
      // sixth refusal row requires to be all zero. ONE BYTE, in memory, and
      // nothing else in the bench changes -- so the run failing here is
      // evidence about two things at once: the refusal group can fire, and the
      // descriptor the machine validates is the one this bench wrote into
      // SDRAM rather than anything it happened to have lying around.
      // A plain `ifdef`, selected by `+define+`, because CLAUDE.md records
      // that a command-line -D cannot override a FUNCTION-LIKE `define and
      // says nothing when it fails to.
      dw[5] = 64'h0000_0000_0000_0001;
`else
      dw[5] = 64'd0;
`endif
      dw[6] = 64'd0;
      dw[7] = 64'd0;
      dw[7][63:32] = fixture_desc_crc(dw);
      for (int unsigned k = 0; k < 8; k = k + 1)
        geom_poke_q(GEOM_POOL_BASE + GEOM_DESC_OFF + 8 * k, dw[k]);
    end

    // ---- the index run: SGF_N_TRIS triplets of packed u8, from the fixture --
    // The whole 64-byte line is written, so no beat of a line this bench caused
    // to be read carries X.
    begin : g_ix
      logic [191:0] ixw;
      ixw = '0;
      for (int unsigned k = 0; k < 3 * SGF_N_TRIS; k = k + 1)
        ixw[8 * k +: 8] = SGF_IX[k];
      geom_poke_q(GEOM_POOL_BASE + GEOM_IX_OFF +  0, ixw[63:0]);
      geom_poke_q(GEOM_POOL_BASE + GEOM_IX_OFF +  8, ixw[127:64]);
      geom_poke_q(GEOM_POOL_BASE + GEOM_IX_OFF + 16, ixw[191:128]);
    end
    geom_poke_q(GEOM_POOL_BASE + GEOM_IX_OFF + 24, 64'd0);
    geom_poke_q(GEOM_POOL_BASE + GEOM_IX_OFF + 32, 64'd0);
    geom_poke_q(GEOM_POOL_BASE + GEOM_IX_OFF + 40, 64'd0);
    geom_poke_q(GEOM_POOL_BASE + GEOM_IX_OFF + 48, 64'd0);
    geom_poke_q(GEOM_POOL_BASE + GEOM_IX_OFF + 56, 64'd0);

    // ---- the vertex run: N_GEOM_VERTS x 32 bytes, byte k at bits [8k+:8] --
    for (int unsigned n = 0; n < N_GEOM_VERTS; n = n + 1) begin
      automatic logic [255:0] rec;
      rec = vdec_record(n);
      geom_poke_q(GEOM_POOL_BASE + GEOM_VX_OFF + 32 * n +  0, rec[63:0]);
      geom_poke_q(GEOM_POOL_BASE + GEOM_VX_OFF + 32 * n +  8, rec[127:64]);
      geom_poke_q(GEOM_POOL_BASE + GEOM_VX_OFF + 32 * n + 16, rec[191:128]);
      geom_poke_q(GEOM_POOL_BASE + GEOM_VX_OFF + 32 * n + 24, rec[255:192]);
    end

    geom_fixture_ready_q = 1'b1;
  end

  // THE BEHAVIOURAL SDRAM. Core header entry I23's refusal said this did not
  // exist; it is `sim/models/zhao_sdram_model.sv`, 219 lines, cycle-true, and
  // its poke backdoor's own comment was written for exactly this use. Nine of
  // the core's ten PHY ports carry an `_o` the model's do not, so every
  // connection is named rather than shorthanded.
  zhao_sdram_model u_geom_sdram (
    .clk       (gpu_clk),
    .phy_cs_n  (phy_cs_n_o),
    .phy_ras_n (phy_ras_n_o),
    .phy_cas_n (phy_cas_n_o),
    .phy_we_n  (phy_we_n_o),
    .phy_a     (phy_a_o),
    .phy_ba    (phy_ba_o),
    .phy_dq_o  (phy_dq_o),
    .phy_dq_oe (phy_dq_oe_o),
    .phy_dqm   (phy_dqm_o),
    .phy_dq_i  (phy_dq_i),
    .peek_en   (geom_peek_en),
    .peek_waddr(geom_peek_waddr),
    .peek_data (geom_peek_data),
    .poke_en   (geom_poke_en),
    .poke_waddr(geom_poke_waddr),
    .poke_data (geom_poke_data),
    .err_trcd            (geom_model_err[0]),
    .err_trp             (geom_model_err[1]),
    .err_trc             (geom_model_err[2]),
    .err_refresh_interval(geom_model_err[3]),
    .err_protocol        (geom_model_err[4]),
    .err_mrs             (geom_model_err[5]),
    .model_error         (geom_model_error)
  );

  // THE DRAW. One meshlet, once, after reset has lifted, the SDRAM controller
  // has finished its PRECHARGE/REFRESH/MRS sequence and the fixture is in
  // memory. The transform is the identity, so the descriptor's object-space
  // bound is also its world bound and GEOM.CULL sees the sphere the fixture
  // placed rather than one this bench moved.
  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      geom_mf_job_valid_i <= 1'b0;
      geom_job_sent_q     <= 1'b0;
    end else begin
      if (geom_mf_job_valid_i && geom_mf_job_ready_o) begin
        geom_mf_job_valid_i <= 1'b0;
        geom_job_sent_q     <= 1'b1;
      end else if (reset_released_q && init_done_o && geom_fixture_ready_q
                   && geom_camera_ready_q && render_frame_open_q
                   && !geom_job_sent_q) begin
        // INSIDE THE OPEN RENDER FRAME: the meshlet's triangles reach the
        // shell's binner through GEOM.REPLAY, and a render frame does not
        // survive the console's frame boundary (the note at the frame open).
        geom_mf_job_valid_i <= 1'b1;
      end
    end
  end

  // --------------------------------------------------------------------------
  // THE TRIANGLE FRONT DOOR IS GONE (2026-09-19, geom packet). It presented
  // sixteen copies of one hand-placed SCREEN triangle at GEOM.CLIP's input
  // because the replay customer did not exist. GEOM.REPLAY does now, so the
  // triangles GEOM.CLIP sees are the fixture meshlet's own, projected by the
  // shared projector in BOTH views and replayed out of the arena.
  //
  // THE VERTEX-ATTRIBUTE STORE (core entry I47) IS MODELLED HERE, with the
  // arena's own contract: it listens to the replay's lookups and answers ONE
  // clock later. Its WRITER is the unbuilt half of owner ruling R11, so these
  // values stand in for it exactly as the SDRAM model stands in for memory.
  // They differ at every vertex, so GEOM.ATTRPACK's u/v planes have real
  // gradients: u_over_w = index << 20, v_over_w = (index ^ arena) << 20, and
  // the r/g/b/alpha slots carry the index for the day a Gouraud lane reads them.
  //
  // `-BadAttribute` (ZHAO_SMOKE_BAD_ATTR, inverted polarity) answers the FIRST
  // lookup of the run one clock LATE. GEOM.REPLAY's `att_skew_o` -- two
  // memories, two timings, independent operands -- must see it, and this bench
  // asserts it zero, so the control passes only if the run FAILS.
  // --------------------------------------------------------------------------
  logic                         att_pend_q;
  logic [GEOM_ARENA_W-1:0]      att_arena_q;
  logic [GEOM_INDEX_W-1:0]      att_index_q;
  bit                           att_late_done_q;
  logic                         att_late_q;

  function automatic logic [GEOM_ATTR_STORE_W-1:0] att_word(
      input logic [GEOM_ARENA_W-1:0] a, input logic [GEOM_INDEX_W-1:0] ix);
    logic [GEOM_ATTR_STORE_W-1:0] w;
    begin
      w = '0;
      w[31:0]    = 32'(ix) << 20;                           // u_over_w
      w[63:32]   = (32'(ix) ^ 32'(a)) << 20;                // v_over_w
      w[95:64]   = 32'(ix);                                  // r
      w[127:96]  = 32'(ix) + 32'd1;                         // g
      w[159:128] = 32'(ix) + 32'd2;                         // b
      w[191:160] = 32'h0000_00FF;                            // alpha
      return w;
    end
  endfunction

  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      att_pend_q           <= 1'b0;
      att_late_q           <= 1'b0;
      att_late_done_q      <= 1'b0;
      geom_att_rep_valid_i <= 1'b0;
      geom_att_rep_data_i  <= '0;
    end else begin
      geom_att_rep_valid_i <= 1'b0;
`ifdef ZHAO_SMOKE_BAD_ATTR
      // One reply, once, one clock late.
      if (att_late_q) begin
        geom_att_rep_valid_i <= 1'b1;
        geom_att_rep_data_i  <= att_word(att_arena_q, att_index_q);
        att_late_q           <= 1'b0;
      end
      if (geom_att_look_valid_o) begin
        att_arena_q <= geom_att_look_arena_o;
        att_index_q <= geom_att_look_index_o;
        if (!att_late_done_q) begin
          att_late_q      <= 1'b1;
          att_late_done_q <= 1'b1;
        end else begin
          geom_att_rep_valid_i <= 1'b1;
          geom_att_rep_data_i  <= att_word(geom_att_look_arena_o, geom_att_look_index_o);
        end
      end
`else
      if (geom_att_look_valid_o) begin
        geom_att_rep_valid_i <= 1'b1;
        geom_att_rep_data_i  <= att_word(geom_att_look_arena_o, geom_att_look_index_o);
      end
`endif
      att_pend_q <= geom_att_look_valid_o;
    end
  end

  // --------------------------------------------------------------------------
  // THE PLAYED MEM.GUARD READ WINDOW, for TERRAIN.PAGESTREAM.
  //
  // THE FLAT-LATTICE MODEL THAT USED TO BE HERE IS GONE. It answered
  // TERRAIN.TESS's `lat_*` port because entry I22 had no owner composed;
  // `zhao_terrain_compcache_front` is composed in the core now and answers it
  // for real, so a harness playing that port would be a second store beside the
  // one under test.
  //
  // What the harness plays instead is one hop further out: the streamer reading
  // the page back out of TERRAIN.PAGE_POOL. Same shape as the WRITE window
  // below -- `ready` is a level with the request, the verdict PULSES one cycle
  // later -- because it is the same block's contract.
  //
  // THE IMAGE IT SERVES IS ZEROS, AND THIS BENCH CANNOT REACH IT ANYWAY. Every
  // page in this run fails its CRC (that is asserted, at `terr_res_crc_failures_o
  // == N_TERR_REC`), so no page reaches RESIDENT_CLEAN, TERRAIN.SEQ issues no
  // patch, and the compose engine sits idle with `terr_ps_idle_o` high. The
  // model exists so that the day a page does become resident this bench reports
  // a lattice rather than a hang, and it is documented as UNEXERCISED here
  // rather than quoted as evidence. The engine's own evidence is
  // `tests/terrain/tb_terrain_compose.sv` (four blocks on real page bytes) and
  // `tests/terrain/tb_terrain_place_cache.sv` (the placement seam this packet
  // added).
  // --------------------------------------------------------------------------
  wire ps_guard_in_pool = (terr_ps_guard_req_o.addr >= POOL_BASE_C) &&
                          (terr_ps_guard_req_o.addr <
                             POOL_BASE_C + (POOL_SLOTS_C * PAGE_BYTES_C));

  logic ps_verd_q, ps_ok_q;
  int unsigned ps_beats_qq;
  int unsigned ps_wait_qq;

  always_comb begin
    terr_ps_guard_rsp_i.ready     = terr_ps_guard_req_o.valid;
    terr_ps_guard_rsp_i.ok        = ps_verd_q &&  ps_ok_q;
    terr_ps_guard_rsp_i.violation = ps_verd_q && !ps_ok_q;
  end

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      ps_verd_q            <= 1'b0;
      ps_ok_q              <= 1'b0;
      ps_beats_qq          <= 0;
      ps_wait_qq           <= 0;
      terr_ps_beat_valid_i <= 1'b0;
      terr_ps_beat_data_i  <= 64'd0;
      terr_ps_beat_last_i  <= 1'b0;
    end else begin
      ps_verd_q <= terr_ps_guard_req_o.valid;
      ps_ok_q   <= ps_guard_in_pool;

      terr_ps_beat_valid_i <= 1'b0;
      terr_ps_beat_last_i  <= 1'b0;

      // A granted read burst is eight 64-bit beats, the established shape three
      // blocks in this tree already use.
      if (terr_ps_guard_req_o.valid && ps_guard_in_pool && (ps_beats_qq == 0)) begin
        ps_beats_qq <= 8;
        ps_wait_qq  <= HPS_LAT;
      end else if (ps_beats_qq != 0) begin
        if (ps_wait_qq > 0) begin
          ps_wait_qq <= ps_wait_qq - 1;
        end else begin
          terr_ps_beat_valid_i <= 1'b1;
          terr_ps_beat_data_i  <= 64'd0;
          terr_ps_beat_last_i  <= (ps_beats_qq == 1);
          ps_beats_qq          <= ps_beats_qq - 1;
        end
      end
    end
  end

  // ==========================================================================
  // THE RUN.
  // ==========================================================================
  initial begin : run
    int unsigned guard;

    // Every DUT input to a defined value first. Generated from the DUT's own
    // port list, so a new input cannot arrive here undriven and X-propagate
    // into a pass.
    part_wr_ready_i = '0;
    part_tbl_ld_valid_i = '0;
    part_tbl_ld_sel_i = '0;
    part_tbl_ld_index_i = '0;
    part_tbl_ld_event_i = '0;
    part_tbl_ld_data_i = '0;
    part_fld_valid_i = '0;
    part_fld_ax_i = '0;
    part_fld_ay_i = '0;
    part_fld_az_i = '0;
    // ---- PACKET P-TERRAIN: the spine's inputs, all defined before reset ----
    terr_cmd_valid_i = '0;
    terr_cmd_epoch_i = '0;
    terr_cmd_list_off_i = '0;
    terr_cmd_list_bytes_i = '0;
    terr_cmd_list_crc_i = '0;
    terr_cmd_patch_count_i = '0;
    terr_cmd_sequence_i = '0;
    terr_cmd_src_id_i = '0;
    terr_cmd_done_ready_i = 1'b1;
    terr_cfg_epoch_i = '0;
    terr_cfg_arena_base_i = '0;
    terr_cfg_arena_bytes_i = '0;
    terr_cfg_load_budget_i = '0;
    // ---- THE COMPOSE ENGINE'S EDGE (core header item 10) ------------------
    // The door is gone: TERRAIN.SEQ's issue is consumed inside the core now.
    // What this bench drives is what the engine itself cannot produce.
    //
    // THE PITCH AND THE ENVELOPE ARE NO LONGER DRIVEN HERE, and the three lines
    // that did it are deleted rather than left at zero. They used to read:
    //
    //     terr_place_pitch_log2_i = 8'sd1;   // the canonical 2.0 m
    //     terr_place_env_x0_i = '0;          // ... against a nonzero origin,
    //     terr_place_env_z0_i = '0;          //     so a deliberate refusal
    //
    // and the comment above them called that pair "entry I35 seen from the far
    // end". That entry is CLOSED: `zhao_terrain_hdrread` reads the page's own
    // 64-byte header on the compose path, so the pitch and the envelope are
    // internal and there is nothing here to define. This bench still never
    // issues a page (no page passes its CRC), so `terr_hr_headers_o` and the
    // rest of composed item 13's counters stay at zero for the same reason
    // every other compose-engine counter does, and none of them is quoted as
    // evidence of anything.
    // I34: the field lane. LOW, never a constant height -- a constant on
    // `fld_height_i` is a field program that moves every vertex of every patch
    // by the same amount, and section 3.4 would still produce a real composed
    // height. Absent and faked are different things.
    terr_pt_fld_valid_i = '0;
    terr_pt_fld_height_i = '0;
    terr_pt_fld_add_valid_i = '0;
    terr_pt_fld_add_x0_i = '0;
    terr_pt_fld_add_z0_i = '0;
    terr_pt_fld_add_x1_i = '0;
    terr_pt_fld_add_z1_i = '0;
    terr_pt_fld_add_hash_i = '0;
    terr_pt_fld_add_cmd_i = '0;
    // I32: layer D. No writer is composed, so the plane is never written and
    // every cell reads SOLID, which terrain_rules 3.3 makes the zero encoding.
    terr_cc_cs_we_i = '0;
    terr_cc_cs_ci_i = '0;
    terr_cc_cs_cj_i = '0;
    terr_cc_cs_substance_i = '0;
    // I21: the served patch's retirement. Never pulsed here, because the block
    // that would pulse it is the absent subpatch issuer.
    terr_cc_serve_release_i = '0;
    terr_dm_valid_i = '0;
    terr_dm_slot_i = '0;
    terr_dm_gen_i = '0;
    terr_dm_epoch_i = '0;
    terr_dm_bd_i = '0;
    terr_dm_f_i = '0;
    terr_dm_mips_i = '0;
    terr_chk_valid_i = '0;
    terr_chk_slot_i = '0;
    terr_chk_gen_i = '0;
    terr_chk_epoch_i = '0;
    // TERRAIN.WRITEBACK IS COMPOSED (entry I28 closed) and this bench CANNOT
    // REACH IT: a writeback job needs a dirty-F eviction, and `terr_dm_f_i` is
    // never raised here -- nor could a page become resident to be dirtied,
    // since every page this bench loads fails the header identity check. So
    // the played SW.STREAM posts NO grants and sends NO ACKs, and the end of
    // the run asserts that no job ever reached the doorbell. The traversal is
    // tests/terrain/world_composed_directed.cpp's, which composes the same
    // sequencer -> doorbell -> writeback -> directory chain.
    terr_cfg_journal_base_i  = 32'h3000_0000;
    terr_cfg_journal_bytes_i = 32'd16 * 32'd8192;
    terr_jdb_post_valid_i  = 1'b0;
    terr_jdb_post_slot_i   = '0;
    terr_jdb_post_ticket_i = '0;
    terr_jdb_ret_ready_i   = 1'b1;
    terr_jdb_ack_valid_i   = 1'b0;
    terr_jdb_ack_ticket_i  = '0;
    terr_jdb_ack_ok_i      = 1'b0;

    // The population sits at the island datum: a legal origin, and the one
    // under which a local position IS a world position.
    part_pop_origin_x_i = '0;
    part_pop_origin_y_i = '0;
    part_pop_origin_z_i = '0;
    part_plane_en_i = '0;
    part_plane_nx_i = '0;
    part_plane_ny_i = '0;
    part_plane_nz_i = '0;
    part_plane_c_i = '0;
    part_cap_full_i = '0;
    part_hist_sel_i = '0;

    // THE PARTICLE DRAW PATH (core entry I24). The three readies are HIGH, and
    // the comment at their declaration says why holding them low would stall
    // the whole particle tick rather than merely leaving the draw path idle.
    part_rung_ready_i = 1'b1;
    part_exp_ready_i  = 1'b1;
    part_sft_ready_i  = 1'b1;

    // THE DRAW DISPATCH (core entry I41). HIGH, and the declaration comment
    // says why: the draw drain is the LAST phase of CMD.EXEC's commit, so a
    // low ready parks the executor and stops the command path outright.
    cmd_draw_ready_i = 1'b1;

    // GEOM.SKIN.NORM's world normal (core entry I43). HIGH, for the reason at
    // its declaration: this consumer sits on the far side of an AND-fork that
    // GEOM.SKIN is on too, so a low ready here is backpressure on the SKINNER.
    geom_sn_n_ready_i = 1'b1;

    // DEBUG.TRACE (core entry I45). ARM STAGE 0 -- `zref::trace::kCommandDecoder`
    // -- and nothing else, because stage 0 is the only one this console has a
    // producer for. Arming is one write; the mask is held by the block.
    // This is stimulus the bench OWNS: arming is a debug command and no block
    // here decodes one, so the bench stands in for the host exactly as it does
    // for the HPS everywhere else in this file.
    dbg_trace_clear_i    = 1'b0;
    dbg_trace_rd_addr_i  = '0;
    dbg_trace_arm_mask_i = 7'b000_0001;
    dbg_trace_arm_we_i   = 1'b1;
    // One world unit of base radius, fx16. A VALUE, not a law: no species
    // radius table exists anywhere in the tree and the reference says that is
    // properly the owner's, so this bench picks one so that the projected size
    // is non-zero and the ladder has something to decide about.
    part_prj_base_radius_i = 32'sh0001_0000;
    part_prj_view_i       = 1'b0;
    part_prj_trail_i      = '0;
    part_prj_narrow_i     = 1'b0;
    part_prj_protected_i  = 1'b0;
    part_prj_gov_floor_i  = '0;
    part_prj_prev_rung_i  = '0;
    part_prj_hold_i       = '0;
    // No hold state exists off chip (I23's absent DDR), so every particle is a
    // first sighting. That is the honest setting here, not a convenience: it
    // makes PART.LADDER take its `p_first_i` path, which is the only one whose
    // inputs this bench can actually supply.
    part_prj_first_i      = 1'b1;
    part_prj_r_i          = 8'hC0;
    part_prj_g_i          = 8'h80;
    part_prj_b_i          = 8'h40;
    part_prj_src_id_i     = 16'h0BAD;
    // THE RULING-5 PACKET IS NO LONGER DRIVEN HERE. Slot 0 (invw24) is
    // GEOM.DEPTHQUANT's, inside GEOM.REPLAY, and slots 1..6 are the modelled
    // attribute store's (I46) -- see the store above.
    geom_clip_cull_mode_i = '0;
    render_frame_open_q = 1'b0;
    geom_pose_start_i = 1'b0;
    geom_pose_bone_count_i = '0;
    geom_pose_root_dx_i = '0;
    geom_pose_root_dy_i = '0;
    geom_pose_root_dz_i = '0;
    geom_pose_bone_parent_i = '0;
    geom_pose_bone_tx_i = '0;
    geom_pose_bone_ty_i = '0;
    geom_pose_bone_tz_i = '0;
    geom_pose_quat_w_i = '0;
    geom_pose_quat_x_i = '0;
    geom_pose_quat_y_i = '0;
    geom_pose_quat_z_i = '0;
    geom_pose_inv_rest_i = '{default: '0};
    geom_org_we_i = '0;
    geom_org_arena_i = '0;
    geom_org_x_i = '0;
    geom_org_y_i = '0;
    geom_org_z_i = '0;
    proj_cfg_we_i = '0;
    proj_cfg_view_i = '0;
    proj_cfg_addr_i = '0;
    proj_cfg_data_i = '0;
    proj_en_i = '0;
    terr_job_valid_i = '0;
    terr_job_ox_i = '0;
    terr_job_oz_i = '0;
    terr_job_level_i = '0;
    terr_job_lvl_nz_i = '0;
    terr_job_lvl_pz_i = '0;
    terr_job_lvl_nx_i = '0;
    terr_job_lvl_px_i = '0;
    terr_job_morph_i = '0;
    terr_job_surface_i = '0;
    terr_job_dual_i = '0;
    terr_job_src_id_i = '0;
    terr_job_view_mask_i = '0;
    terr_job_mat_a_i = '0;
    terr_job_mat_b_i = '0;
    terr_job_weight_i = '0;
    terr_sparse_fill_i = '0;
    proj_out_ready_i = '0;
    post_view_sel_i = '0;
    post_s_valid_i = '0;
    post_s_rgb_i = '0;
    post_gd_present_i = '0;
    post_gd_dx_i = '0;
    post_gd_dy_i = '0;
    post_gg_present_i = '0;
    post_gg_glow_i = '0;
    post_gg_ink_i = '0;
    post_atm_en_i = '0;
    post_atm_valid_i = '0;
    post_atm_rgb_i = '0;
    post_atm_opacity_i = '0;
    post_atm_add_i = '0;
    post_bloom_gain_i = '0;
    post_grade_valid_i = '0;
    post_pv_we_i = '0;
    post_pv_sel_i = '0;
    post_pv_addr_i = '0;
    post_pv_data_i = '0;
    post_bias_r_i = '0;
    post_bias_g_i = '0;
    post_bias_b_i = '0;
    post_flash_rgb_i = '0;
    post_flash_amt_i = '0;
    post_ink_rgb_i = '0;
    post_hud_valid_i = '0;
    post_hud_rgb_i = '0;
    post_o_ready_i = '0;
    hist_ev_valid_i = '0;
    hist_ev_lane_valid_i = '0;
    hist_ev_err_i = '0;
    hist_ev_src_id_i = '0;
    hist_rd_valid_i = '0;
    hist_rd_bin_i = '0;
    cfg_valid_i = '0;
    cfg_op_i = '0;
    cfg_page_generation_i = '0;
    cfg_selector_i = '0;
    cfg_row_i = '0;
    cfg_crc32_i = '0;
    cfg_rsp_ready_i = '0;
    pal_load_valid_i = '0;
    pal_load_op_i = '0;
    pal_load_slot_i = '0;
    pal_load_gen_i = '0;
    pal_load_idx_i = '0;
    pal_load_rgb565_i = '0;
    pal_load_crc_ok_i = '0;
    tri_area2_i = '0;
    tri_flat_request_i = '0;
    tri_continuation_tail_i = '0;
    tri_fragment_state_i = '0;
    fill_req_ready_i = '0;
    fill_data_valid_i = '0;
    fill_data_i = '0;
    fill_refused_i = '0;
    frame_clear_word_i = '0;
    sheet_req_ready_i = '0;
    blank_cmd_i = '0;
    scanout_ack_i = '0;
    frame_swap_valid_i = '0;
    frame_swap_slot_i = '0;
    hps_state_i = '{default: '0};
    hps_byte_len_i = '{default: '0};
    ring_wr_ready_i = '0;
    // `hps_req_grant_i` / `hps_rd_*_i` are no longer tied here: the shell's HPS
    // port is served by the upload-arena model below (MEM.UPLOAD's socket).
    pad_present_i = '0;
    pad_buttons_i = '{default: '0};
    pad_lx_i = '{default: '0};
    pad_ly_i = '{default: '0};
    pad_rx_i = '{default: '0};
    pad_ry_i = '{default: '0};
    aud_wr_valid_i = '0;
    aud_wr_l_i = '0;
    aud_wr_r_i = '0;
    cnt_snap_ready_i = '0;
    render_frame_begin_i = '0;
    render_frame_end_i = '0;
    render_grid_w_i = '0;
    render_grid_h_i = '0;

    render_fill_word_i = '0;
    render_clear_word_i = '0;
    render_state_i = '0;
    render_src_a_i = '0;
    render_texel_rgb_i = '0;
    render_texel_a_i = '0;
    render_texel_idx_i = '0;
    render_fb_base_i = '0;
    render_fb_stride_i = '0;
    fb_writer_i = '0;
    // `phy_dq_i = '0` USED TO BE HERE. The SDRAM model drives it now, and a
    // bench that also drove it would be two drivers on the data bus with the
    // harness winning -- which reads as a memory that answers zero.

    // ---- the geometry asset path's boundaries (core entries I36..I39) -----
    geom_mf_job_instance_id_i = 16'h00A1;   // becomes the meshlet's src_id
    geom_mf_job_desc_addr_i   = 27'(GEOM_POOL_BASE + GEOM_DESC_OFF);
    geom_mf_job_format_i      = 8'd1;
    geom_mf_job_generation_i  = 16'd1;
    geom_mf_job_active_mask_i = 2'b11;      // BOTH cameras
    for (int unsigned i = 0; i < 12; i = i + 1)
      geom_mf_job_xform_i[i] = ((i == 0) || (i == 5) || (i == 10))
                               ? FX16_ONE : 32'sd0;
    // I39, narrowed: the descriptor's raster word has no producer anywhere.
    geom_asm_raster_state_i   = '0;

    // ---- reset ------------------------------------------------------------
    rst_n            = 1'b0;
    reset_released_q = 1'b0;
    cycles_q               = 0;
    ticks_seen_q           = 0;
    part_records_written_q = 0;
    part_tick_seen_q       = 1'b0;
    part_children_seen_q       = 0;
    part_children_at_contact_q = 0;

    // Benign, NON-ZERO configuration, chosen so nothing is refused for a
    // reason unrelated to wiring:
    //   species lifetime 0 = UNBOUNDED, so no particle dies of age and the
    //     survive verdict reaches PART.STATE;
    //   collision response STICK (see the spawn-on-collision block below),
    //     a KNOWN response, so PART.COLLIDE reports alive rather than refusing;
    //   the projector enabled and a rigid, unit-weight skin, so client A is
    //     offered real vertices.
    //
    // ALL THREE OF THOSE USED TO BE WIRES. Until 2026-09-19 this bench drove
    // the lifetime, the collision response and the spawn rule straight onto the
    // consumers' descriptor inputs, because the table they belong to had no
    // owner (entries I2/I3). It does now, so they are LOADED below instead --
    // and every particle behaviour this bench already asserted is now evidence
    // that the load reached the consumer through `zhao_part_table`.
    part_wr_ready_i         = 1'b1;

    // ---- SPAWN ON COLLISION, the acceptance for owner ruling I4 -------------
    // Before the ruling this bench held the collider quiet (response IGNORE, no
    // terrain sample) and PRINTED two counters it knew could not move. Both of
    // them CAN move now, and making them move is the point of this stimulus.
    //
    //   * a PLANE at +100 with an up normal (n.p = c, c in Q NRM_Q), so every
    //     record -- all of which sit at y = 0 -- is inside the surface and
    //     makes a contact. It was a hand-driven heightfield until I6 closed;
    //     a unit up normal makes the plane's placement the same exact
    //     vertical h + CLEAR_EPS the heightfield's was, so the acceptance
    //     value below did not move;
    //   * STICK (3'd2), the simplest PLACING response: both coefficients are
    //     zero, so the velocity result needs no coefficient arithmetic and the
    //     placement is the exact vertical h + CLEAR_EPS;
    //   * a spawn descriptor that resolves, with one child per fired event.
    //     ONLY event 2 fires here -- the records carry no born flag, the age
    //     marker is disabled and the lifetime is unbounded -- so the other
    //     three spawn counters staying at zero is a specificity check, not an
    //     oversight.
    part_plane_en_i         = 1'b1;
    part_plane_nx_i         = '0;
    part_plane_ny_i         = PART_NRM_W'(1 << 10);   // NRM_Q = 10: a unit +Y normal
    part_plane_nz_i         = '0;
    part_plane_c_i          = 32'(PART_TER_H) <<< 10; // y = +100 LSBs, Q NRM_Q
    // (the STICK response and the spawn rule are now LOADED INTO PART.TABLE
    //  after reset lifts -- see the load sequence below.)
    proj_en_i               = 1'b1;
    // THE MATRIX BANK IS WRITTEN NOW, and it was not before. GEOM.CULL takes
    // the same sixteen words at the same addresses as the projector -- its
    // own port comment says so -- so leaving the bank at zero would have made
    // the cull's verdict on the meshlet below an accident rather than a
    // result. It is written AFTER `proj_en_i` rises, from a task, so the
    // sixteen writes are one per clock on the real configuration port.
    // The replayed terrain triangle has no consumer in this core (entry I13),
    // so the bench SINKS it. Holding it low instead would back the replay up
    // into the sequencer and the resulting stall would read as a wiring fault.
    proj_out_ready_i        = 1'b1;
    // THE BENCH NO LONGER HANDS GEOM.SKIN AN IDENTITY MATRIX. It cannot:
    // zhao_geom_pose_palette owns those ports now (entry I10, closed). The
    // vertices still skin against the identity, because a palette holding no
    // decoded pose substitutes the identity bind pose per bone reference and
    // SAYS SO on geom_pal_bone_unset_o. So the skinned counts below are
    // unchanged, and the reason they are unchanged has moved from a harness
    // constant to a stated, counted behaviour of the design.
    //
    // WHAT THIS BENCH THEREFORE DOES NOT EXERCISE: a real decoded palette. The
    // decoder's source is entry I29 and has no producer in this tree; driving a
    // synthetic skeleton here would be this bench inventing an asset. The
    // palette PATH is checked below; the palette CONTENT is
    // geom_pose_palette_directed's job, against thirty-two distinct matrices.
    geom_pose_quat_w_i = 16'sd16384;      // quat16 is S1.0.14: this is 1.0
    for (int i = 0; i < 12; i++) geom_pose_inv_rest_i[i] = '0;
    geom_pose_inv_rest_i[0]  = FX16_ONE;  // row-major 3x4, the identity
    geom_pose_inv_rest_i[5]  = FX16_ONE;
    geom_pose_inv_rest_i[10] = FX16_ONE;

    // ---- PACKET P-TERRAIN: lay out the HPS arena BEFORE reset lifts -------
    // The memory is the board's, not the frame's: it exists before the console
    // starts and nothing in the DUT may depend on when it was written.
    for (int unsigned w = 0; w < HPS_WORDS; w++) hps_mem[w] = 64'd0;

    // T5's 32-byte record, four whole beats, little-endian, in the field order
    // `zhao_terrain_cmd`'s own header tabulates. Written field by field from
    // that table rather than as an opaque blob, so a layout change fails here
    // loudly instead of producing plausible terrain in the wrong place.
    //
    // N_TERR_REC records with DIFFERENT patch coordinates AND DIFFERENT page
    // addresses, and both differences are load-bearing. The directory keys on
    // {epoch, island, ix, iz}, so records that differed only in source id would
    // land in one entry and the later ones would report SAME rather than a
    // fresh claim; and a page address shared between records would let a swap
    // between two jobs go unseen on the loader's side. One claim and one page
    // per record is the evidence that each crossed every seam individually.
    //
    // `automatic`, AND IT IS THE WHOLE DEFECT THIS BENCH SPENT A PASS ON.
    // A variable declared inside a begin/end of a STATIC scope -- which an
    // `initial` block is -- has static lifetime, so its initialiser runs ONCE,
    // before time zero, and NOT on each pass of the loop. Written without
    // `automatic`, `b` stayed 0 for every r and all N_TERR_REC records were
    // written on top of each other at word 0: the arena held the LAST record
    // and then zeros. See the note at N_TERR_REC for what that looked like from
    // the far end of the spine.
    for (int unsigned r = 0; r < N_TERR_REC; r++) begin
      automatic int unsigned b = (LIST_OFF_C >> 3) + r * 4;
      hps_mem[b + 0] = {16'(r + 7),          // patch_iz   i16  bytes 6..7
                        16'(r + 3),          // patch_ix   i16  bytes 4..5
                        32'h0000_0042};      // island_id  u32  bytes 0..3
      hps_mem[b + 1] = 64'(HPS_BASE) +
                       64'(PAGE0_OFF_C + r * PAGE_BYTES_C); // hps_page_addr
      hps_mem[b + 2] = {8'd0,                // priority   u8   byte 23
                        8'h01,               // view_mask  u8   byte 22
                        16'h0001,            // flags      u16  bytes 20..21 (REQUIRED)
                        32'hDEAD_BEEF};      // expected_page_crc32c bytes 16..19
      hps_mem[b + 3] = {32'd0,               // reserved   u32  bytes 28..31
                        32'(1000 + r)};      // source_id  u32  bytes 24..27
    end

    // ---- THE ARENA IS READ BACK BEFORE IT IS BELIEVED ---------------------
    // MAKE THE HARNESS'S OWN LOSS COUNTABLE. The defect above wrote every
    // record to one address and was SILENT everywhere a check could have seen
    // it: the list CRC is folded by this bench over the same bytes the bench
    // wrote, so both sides agreed on the identical mistake and `crc_fails=0`
    // was then quoted as proof that the bridge had handed over every record.
    // A checker whose two operands are corrupted together cannot fire
    // (CLAUDE.md), and a bench that seals its own list is exactly that shape.
    //
    // This loop is the independent operand. It re-reads the arena field by
    // field against the intent stated above, so an aliasing, an off-by-one or
    // a widened record is LOUD at time zero and names the record -- instead of
    // arriving 5 ms later as "the console lost a record".
    for (int unsigned r = 0; r < N_TERR_REC; r++) begin
      automatic int unsigned b = (LIST_OFF_C >> 3) + r * 4;
      if (hps_mem[b + 0][31:0] !== 32'h0000_0042)
        $fatal(1, "SMOKE: arena record %0d holds island %08x, not 00000042 -- the list layout aliased", r, hps_mem[b + 0][31:0]);
      if (hps_mem[b + 0][47:32] !== 16'(r + 3))
        $fatal(1, "SMOKE: arena record %0d holds ix %0d, not %0d -- the list layout aliased", r, hps_mem[b + 0][47:32], r + 3);
      if (hps_mem[b + 0][63:48] !== 16'(r + 7))
        $fatal(1, "SMOKE: arena record %0d holds iz %0d, not %0d -- the list layout aliased", r, hps_mem[b + 0][63:48], r + 7);
      if (hps_mem[b + 1] !== 64'(HPS_BASE) + 64'(PAGE0_OFF_C + r * PAGE_BYTES_C))
        $fatal(1, "SMOKE: arena record %0d holds page address %016x, not %016x -- the list layout aliased",
               r, hps_mem[b + 1], 64'(HPS_BASE) + 64'(PAGE0_OFF_C + r * PAGE_BYTES_C));
      if (hps_mem[b + 2] !== {8'd0, 8'h01, 16'h0001, 32'hDEAD_BEEF})
        $fatal(1, "SMOKE: arena record %0d holds crc/flags/mask/prio %016x -- the list layout aliased", r, hps_mem[b + 2]);
      if (hps_mem[b + 3][31:0] !== 32'(1000 + r))
        $fatal(1, "SMOKE: arena record %0d holds source id %0d, not %0d -- the list layout aliased", r, hps_mem[b + 3][31:0], 1000 + r);
    end

    // Fold the list with the PRODUCTION folder. See the declaration for why
    // this makes the bench no evidence about the CRC and good evidence about
    // the spine. Bytes are folded eight at a time, low byte first, which is
    // the folder's own convention and the order the bridge returns them in.
    fold_c_i = 32'hFFFF_FFFF;
    fold_n_i = 4'd8;
    for (int unsigned w = 0; w < (LIST_BYTES_C / 8); w++) begin
      fold_d_i = hps_mem[(LIST_OFF_C >> 3) + w];
      #1ns;
      fold_c_i = fold_c_o;
    end
    terr_cmd_list_crc_i = ~fold_c_i;

    terr_cfg_epoch_i       = 32'd9;
    terr_cfg_arena_base_i  = HPS_BASE;
    terr_cfg_arena_bytes_i = 32'(HPS_WORDS * 8);
    terr_cfg_load_budget_i = 16'd32;   // T7's per-frame page budget

    // ---- MEM.UPLOAD's one request (core entry I47) ------------------------
    // A 256-byte MATERIAL_SET-kind resource staged in the upload arena, sealed
    // with the SAME production folder as the terrain list above -- so this is
    // evidence about the socket and the copy, not about the CRC law, which
    // `tests/mem/mem_upload_directed.cpp` owns.
    for (int unsigned w = 0; w < UPL_WORDS_C; w++)
      upl_mem[w] = 64'hC0DE_5EED_0000_0000 + 64'(w * 32'h0101_0101);
    fold_c_i = 32'hFFFF_FFFF;
    fold_n_i = 4'd8;
    for (int unsigned w = 0; w < UPL_WORDS_C; w++) begin
      fold_d_i = upl_mem[w];
      #1ns;
      fold_c_i = fold_c_o;
    end
    upl_req_crc_i          = ~fold_c_i;
    upl_req_tag_i          = 8'd11;            // spec/cartridge.md 4a: MATERIAL_SET
    upl_req_index_i        = 24'h00_ABCD;      // the handle index (5f.1's key)
    upl_req_hps_addr_i     = 64'(UPL_ARENA_C);
    upl_req_vram_addr_i    = UPL_REGION_C;
    upl_req_len_i          = 32'(UPL_WORDS_C * 8);
    upl_req_epoch_i        = 16'd9;
    upl_req_dst_slot_i     = 8'd5;
    upl_req_new_gen_i      = 16'h0102;
    upl_cfg_region_base_i  = UPL_REGION_C;
    upl_cfg_region_bytes_i = UPL_REGION_SZ;
    upl_cfg_arena_base_i   = 64'(UPL_ARENA_C);
    upl_cfg_arena_bytes_i  = 32'(UPL_WORDS_C * 8);
    upl_cfg_epoch_i        = 16'd9;

    repeat (20) @(posedge gpu_clk);
    rst_n = 1'b1;
    repeat (4) @(posedge gpu_clk);
    reset_released_q = 1'b1;

    // The camera, into the bank GEOM.PROJECT and GEOM.CULL share. Sixteen
    // clocks on the real `proj_cfg_*` port; `geom_camera_ready_q` gates the
    // meshlet draw so no descriptor can be culled against an unwritten bank.
    geom_write_camera();
    geom_camera_ready_q = 1'b1;

    // ---- LOAD PART.TABLE, BEFORE ANY PARTICLE IS OFFERED ------------------
    // Four words for species 0 -- the only species these records carry (the
    // generation store writes position X and leaves every other field zero, so
    // `species` is 0 by construction). The host loads between ticks and the
    // table is read during them; that is the discipline `no_rw_check` on its
    // arrays is correct under, and doing it here is what makes the attribute
    // honest rather than convenient.
    //
    // ORDER MATTERS AND IS CHECKED. The generation store below only offers
    // records once `part_tick_busy_o` rises, so this must complete first; the
    // assertion after it is what turns "it should have" into "it did".
`ifndef ZHAO_SMOKE_SKIP_TBL_LOAD
    tbl_load(TBL_SEL_UPD, 7'd0, 2'd0,
             PART_TBL_LD_W'(TBL_RECIPE_COLOUR) << TBL_U_OFF_RCP);   // lifetime 0 = unbounded,
                                                                    // drag/grav/strength/centre/params 0
    tbl_load(TBL_SEL_COL, 7'd0, 2'd0,
             PART_TBL_LD_W'(3'd2) << TBL_C_OFF_RSP);                // STICK; both coefficients 0
    tbl_load(TBL_SEL_SPW, 7'd0, 2'd2,                                // event 2 = COLLISION
             (PART_TBL_LD_W'(7'd0) << TBL_S_OFF_CHD) |               // child species 0
             (PART_TBL_LD_W'(5'd1) << TBL_S_OFF_CNT) |               // one child
             (PART_TBL_LD_W'(1'b1) << TBL_S_OFF_KNW));               // known
    tbl_load(TBL_SEL_CRV, 7'd0, 2'd0,                                // curve bucket 0: age_next[9:6]
             (PART_TBL_LD_W'(TBL_CURVE_SIZE)   << TBL_V_OFF_SIZ) |
             (PART_TBL_LD_W'(TBL_CURVE_COLOUR) << TBL_V_OFF_CLR));

    if (part_updated_o != 0)
      $fatal(1, "SMOKE: %0d particle(s) were already updated when PART.TABLE was loaded -- the load lost its race with the first tick and every descriptor read below is against an empty table",
             part_updated_o);
    if (part_tbl_loads_update_o != 1 || part_tbl_loads_collide_o != 1 ||
        part_tbl_loads_spawn_o != 1 || part_tbl_loads_curve_o != 1)
      $fatal(1, "SMOKE: PART.TABLE counted upd=%0d col=%0d spw=%0d crv=%0d against one load each -- the load port does not reach all four slices",
             part_tbl_loads_update_o, part_tbl_loads_collide_o,
             part_tbl_loads_spawn_o, part_tbl_loads_curve_o);
    if (part_tbl_load_refused_o != 0)
      $fatal(1, "SMOKE: PART.TABLE refused %0d of four in-range loads", part_tbl_load_refused_o);
`else
    // THE NEGATIVE CONTROL FOR EVERY PART.TABLE CHECK IN THIS FILE, and it is a
    // committed switch rather than an edit somebody made once and reverted.
    //
    // With `ZHAO_SMOKE_SKIP_TBL_LOAD` defined, the four loads above do not
    // happen and nothing else changes. The table then answers every read with
    // what an unwritten array holds, and the checks at the foot of the run MUST
    // go red -- otherwise they are passing on something other than the table's
    // contents, which is the whole thing they claim to measure.
    //
    // MEASURED 2026-09-19, the pass that composed the table:
    //   loads[upd/col/spw/crv] = [0 0 0 0]
    //   `part_colour_beats_q` = 0 and the run fails at
    //   "PART.UPDATE never raised out_colour_en_o".
    // Run it by adding `+define+ZHAO_SMOKE_SKIP_TBL_LOAD` to the verilate step
    // in tests/prod/run_console_core_smoke.ps1. (Written as prose rather than
    // as a command line, because a comment beginning with the tool's own name
    // is parsed as a metacomment and rejected -- BADVLTPRAGMA, met here.)
    // It is a PLAIN `ifdef` on purpose: CLAUDE.md records that a command-line
    // `-D` cannot override a FUNCTION-LIKE `define` and says nothing when it
    // fails to, so a macro-selected control has to be a form `-D` reaches.
    $display("SMOKE: NEGATIVE CONTROL -- PART.TABLE is deliberately NOT loaded; every table check below must fail.");
`endif

    // ---- PACKET P-TERRAIN: one SubmitTerrainSet, then let the spine run ---
    // A HOST PACKET, which is what the plan lets a harness present. Everything
    // after it is the console's own work: TERRAIN.CMD reads the list over the
    // bridge, emits records; TERRAIN.SEQ looks each up in the directory,
    // claims a slot, issues a load; TERRAIN.LOADQ queues it; TERRAIN.PAGELOADER
    // fetches 21,376 bytes and reports. Not one of those steps is written here.
    terr_cmd_epoch_i       = 32'd9;
    terr_cmd_list_off_i    = 32'(LIST_OFF_C);
    terr_cmd_list_bytes_i  = 32'(LIST_BYTES_C);
    terr_cmd_patch_count_i = 16'(N_TERR_REC);
    terr_cmd_sequence_i    = 32'd1;
    terr_cmd_src_id_i      = 32'd777;
    terr_cmd_valid_i       = 1'b1;
    guard = 0;
    while (!(terr_cmd_valid_i && terr_cmd_ready_o) && (guard < 1000)) begin
      @(posedge gpu_clk);
      guard++;
    end
    @(posedge gpu_clk);
    terr_cmd_valid_i = 1'b0;
    if (guard >= 1000)
      $fatal(1, "SMOKE: TERRAIN.CMD never accepted the command -- j_ready_o stayed low");

    // ---- the geometry job -------------------------------------------------
    // One meshlet, one view. The vertices follow from the always block above.
    // ---- OPEN A RENDER FRAME ----------------------------------------------
    // The triangle door needs one, and nothing in this bench opened one before
    // 2026-09-19 because nothing offered a triangle: `render_tri_valid_i` was
    // tied low here and the shell's rasteriser was never asked to do anything.
    // The protocol is `tests/shell/shell_harness.hpp`'s and is copied from it
    // rather than guessed: grid size, then a ONE-CYCLE `frame_begin` pulse.
    //
    // 4x4 tiles is 64x64 pixels. The triangle offered to GEOM.CLIP sits inside
    // it AND inside the Z60 scissor the core derives from `mode_act_o`, so the
    // clipper passing it and the binner binning it are consistent facts rather
    // than a coincidence of two different rectangles.
    //
    // `fb_writer_i` is 1: RASTER.FBWRITE holds the lease, not DEBUG.FRAMEBLIT.
    // With it at 0 the guard denies every render burst and `render_pixels_o`
    // stays at zero for a reason that has nothing to do with this packet.
    @(posedge gpu_clk);
    fb_writer_i          <= 1'b1;
    render_fb_base_i     <= 27'd0;
    render_fb_stride_i   <= 16'd128;      // 64 px * 2 bytes
    render_fill_word_i   <= 64'hA5A5_A5A5_A5A5_A5A5;
    render_clear_word_i  <= 64'h5A5A_5A5A_5A5A_5A5A;
    render_src_a_i       <= 8'hFF;
    render_texel_rgb_i   <= 24'hFF00FF;
    render_texel_a_i     <= 8'hFF;
    render_texel_idx_i   <= 8'd1;
    render_grid_w_i      <= 6'd4;
    render_grid_h_i      <= 6'd4;
    // THE FRAME REQUEST IS HELD UNTIL IT IS ACCEPTED, NOT PULSED.
    //
    // `render_frame_begin_i` is `zhao_renderer_lease_v2`'s `frame_req_valid_i`,
    // and that is a ready/valid port with a real READY: the lease withholds it
    // while the reset-epoch barrier is closed, while a lease is already live
    // and while neither slot is FREE. A one-cycle pulse offered to it is simply
    // LOST, and nothing retries.
    //
    // This bench pulsed it, and the pulse landed at cycle 12 against a gate
    // that opens at cycle 16 -- measured with a bind-in probe:
    // `frame_req valid first=12 cycles=1, ready first=16, fire first=never`.
    // Four cycles, and the entire render path downstream read as absent. It
    // later began passing by ACCIDENT, when unrelated stimulus added ahead of
    // it pushed the pulse past cycle 16; a gate that passes because of where
    // somebody else's code sits is not a gate.
    //
    // `frame_req_ready_o` is not observable here -- `zhao_shell_top_v2` leaves
    // it an empty pin connection -- so the acceptance is read from
    // `v2_frames_admitted_o`, which is the lease's own count of frames it let
    // through. Holding valid past the fire is harmless: `frame_req_ready_o`
    // requires `!lease_valid_i`, so a live lease cannot admit a second frame.
    @(posedge gpu_clk);
    render_frame_begin_i <= 1'b1;
    guard = 0;
    while ((v2_frames_admitted_o == 0) && (guard < 20000)) begin
      @(posedge gpu_clk);
      guard = guard + 1;
    end
    render_frame_begin_i <= 1'b0;
    if (v2_frames_admitted_o == 0)
      $fatal(1, "SMOKE: the renderer lease admitted no frame in %0d cycles with the request HELD -- granted=%0d refused=%0d clears=%0d. A held request that is never accepted is the barrier or the slot state, not the stimulus",
             guard, v2_leases_granted_o, v2_leases_refused_o,
             v2_clear_handshakes_o);
    render_frame_open_q  <= 1'b1;         // releases the meshlet draw above

    // THE RENDER FRAME IS OPENED, FILLED AND CLOSED IN ONE SEQUENCE, and that
    // is a correction rather than a style choice. The first version opened it
    // here and closed it after the run's two-frame wait -- half a million gpu
    // cycles and two `gpu_tick_o` edges later. Every counter read clean
    // (`drained=1 busy=0 fatal=0 stream_err=0`) and `render_pixels_o` was 0,
    // which reads exactly like "the triangles never reached the binner" and is
    // not that: a render frame does not survive the console's frame boundary.
    // Reading the reassuring zeros as evidence about the door would have sent
    // the next person to re-check wiring that was already correct.
    // WAIT FOR THE MESHLET TO BE RELEASED: GEOM.REPLAY releases it only after
    // its last triangle has been emitted in every view, which is the whole
    // frame's geometry here. Descriptor fetch, footprint, skin, projection and
    // replay all run inside this window.
    guard = 0;
    while ((geom_rp_meshlets_o == 0) && (guard < 200000)) begin
      @(posedge gpu_clk);
      guard = guard + 1;
    end
    if (geom_rp_meshlets_o == 0)
      $fatal(1, "SMOKE: GEOM.REPLAY released no meshlet in %0d cycles -- replayed %0d of %0d view-triangles, handles=%0d, fetched=%0d meshlet(s), skinned=%0d, landings=%0d, descriptor refused[fmt/crc/gen/vc/tc/resv/bound]=[%0d %0d %0d %0d %0d %0d %0d]",
             guard, geom_rp_triangles_out_o, SGF_EXP_REPLAYED, geom_rp_groups_o,
             geom_af_meshlets_fetched_o, geom_skin_vertices_transformed_o, geom_landings_o,
             geom_mf_refused_format_o, geom_mf_refused_crc_o,
             geom_mf_refused_generation_o, geom_mf_refused_vertex_count_o,
             geom_mf_refused_triangle_count_o, geom_mf_refused_reserved_o,
             geom_mf_refused_zero_bound_o);
    // Let the last accepted triangles clear GEOM.SETUP and the binner.
    repeat (200) @(posedge gpu_clk);

    @(posedge gpu_clk);
    render_frame_end_i <= 1'b1;
    @(posedge gpu_clk);
    render_frame_end_i <= 1'b0;

    guard = 0;
    while (!render_drain_done_o && (guard < 200000)) begin
      @(posedge gpu_clk);
      guard = guard + 1;
    end
    if (!render_drain_done_o)
      $display("SMOKE: NOTE render_drain_done_o never rose after %0d cycles", guard);
    repeat (4000) @(posedge gpu_clk);

    // (GEOM.GROUP_SEQ's job is no longer injected here: the meshlet dispatcher
    // fork inside the core issues it, from the meshlet the draw above fetched.)

    // ---- the terrain job --------------------------------------------------
    // One subpatch, one view, level 0, top surface, no geomorph. The tess
    // turns this into 81 window vertices (client B's fill) and then its
    // triangles (the replay), so both halves of client B are exercised by ONE
    // job. `sparse_fill_i` stays LOW: the subsystem here carries a dense
    // shell, and a sparse fill against a dense shell is a seal-short fault the
    // terrain differential fires on purpose -- not something a wiring smoke
    // bench should provoke.
    @(posedge gpu_clk);
    terr_job_ox_i        <= 6'd0;
    terr_job_oz_i        <= 6'd0;
    terr_job_level_i     <= 2'd0;
    terr_job_lvl_nz_i    <= 2'd0;
    terr_job_lvl_pz_i    <= 2'd0;
    terr_job_lvl_nx_i    <= 2'd0;
    terr_job_lvl_px_i    <= 2'd0;
    terr_job_morph_i     <= 17'd0;
    terr_job_surface_i   <= 1'b0;
    terr_job_dual_i      <= 1'b0;
    terr_job_src_id_i    <= 16'h5678;
    terr_job_view_mask_i <= 2'b01;
    terr_job_mat_a_i     <= 8'h11;
    terr_job_mat_b_i     <= 8'h22;
    terr_job_weight_i    <= 8'hFF;
    terr_job_valid_i     <= 1'b1;
    guard = 0;
    while (!(terr_job_valid_i && terr_job_ready_o) && (guard < 1000)) begin
      @(posedge gpu_clk);
      guard = guard + 1;
    end
    @(posedge gpu_clk);
    terr_job_valid_i <= 1'b0;
    if (guard >= 1000)
      $fatal(1, "SMOKE: TERRAIN.GROUP_SEQ never accepted a job (job_ready_o stuck low)");

    // ---- run until the shell reaches two frame edges ----------------------
    // Two rather than one: the first proves FRAMECTL runs, the second gives
    // the particle generation a whole tick to drain.
    while ((ticks_seen_q < 2) && (cycles_q < CYCLE_LIMIT)) @(posedge gpu_clk);

    if (ticks_seen_q < 2)
      $fatal(1, "SMOKE: no frame edge after %0d gpu cycles -- the shell never left reset", cycles_q);

    // let the tick's generation drain
    repeat (2000) @(posedge gpu_clk);


    // ======================================================================
    // THE VERDICT. Each line names the wire it is evidence for.
    // ======================================================================
    $display("SMOKE: cycles=%0d frame_edges=%0d", cycles_q, ticks_seen_q);
    $display("SMOKE: particles  read=%0d written=%0d survivors=%0d updated=%0d",
             part_records_sent_q, part_records_written_q,
             part_survivors_o, part_updated_o);
    $display("SMOKE: assetpath  considered=%0d fetched=%0d culled=%0d refused[fmt/crc/gen/vc/tc/resv/bound]=[%0d %0d %0d %0d %0d %0d %0d]",
             geom_mf_meshlets_considered_o, geom_mf_descriptors_fetched_o,
             geom_mf_culled_all_cameras_o,
             geom_mf_refused_format_o, geom_mf_refused_crc_o,
             geom_mf_refused_generation_o, geom_mf_refused_vertex_count_o,
             geom_mf_refused_triangle_count_o, geom_mf_refused_reserved_o,
             geom_mf_refused_zero_bound_o);
    $display("SMOKE: assetpath  meshlets=%0d beats=%0d denied[mf/af]=[%0d %0d] footprint_refused=%0d beat_err[trunc/over/unowned]=[%0d %0d %0d]",
             geom_af_meshlets_fetched_o, geom_af_beats_read_o,
             geom_mf_guard_denied_o, geom_af_guard_denied_o,
             geom_af_refused_footprint_o, geom_af_err_beat_truncated_o,
             geom_af_err_beat_overrun_o, geom_af_err_beat_unowned_o);
    // `contention` READS ZERO HERE FOR A STRUCTURAL REASON, and it is
    // printed rather than asserted because of it. GEOM.MESHFETCH reads a
    // descriptor and only THEN hands the meshlet to GEOM.ASSETFETCH, so with
    // ONE draw in flight the two requesters can never want the shared client
    // in the same cycle. It becomes reachable with a second draw overlapping
    // the first -- MESHFETCH on descriptor N+1 while ASSETFETCH is still
    // walking N's footprint -- which needs entry I38's release owner, so it
    // is not reachable in this composition at all. Quoting the zero as
    // "sharing costs nothing" would be quoting a workload, not a result.
    $display("SMOKE: desc crc   descriptors=%0d fail=%0d framing=%0d",
             geom_mf_crc_descriptors_o, geom_mf_crc_fail_o, geom_mf_crc_framing_o);
    $display("SMOKE: memadapter jobs[a/b]=[%0d %0d] denied=%0d contention=%0d err[short/long/unowned]=[%0d %0d %0d]",
             geom_ma_jobs_a_o, geom_ma_jobs_b_o, geom_ma_denied_o,
             geom_ma_contention_o, geom_ma_err_short_o, geom_ma_err_long_o,
             geom_ma_err_unowned_o);
    $display("SMOKE: assemble   meshlets=%0d triangles=%0d refused[limits/index]=[%0d %0d]",
             geom_asm_meshlets_o, geom_asm_triangles_o,
             geom_asm_refused_limits_o, geom_asm_refused_index_o);
    $display("SMOKE: sdram      model_error=%0d kinds[trcd/trp/trc/refresh/protocol/mrs]=%b",
             geom_model_error, geom_model_err);
    $display("SMOKE: vdecode    records_expected=%0d decoded=%0d refused[reserved/w0/format]=[%0d %0d %0d]",
             N_GEOM_VERTS, geom_vd_vertices_o,
             geom_vd_reserved_nz_count_o, geom_vd_w0_illegal_count_o,
             geom_vd_format_bad_count_o);
    $display("SMOKE: geometry   skinned=%0d vertices_sent=%0d a_grants=%0d landings=%0d groups_opened=%0d groups_sealed=%0d",
             geom_skin_vertices_transformed_o, geom_vertices_sent_o,
             proj_a_grants_o, geom_landings_o,
             geom_groups_opened_o, geom_groups_sealed_o);    $display("SMOKE: pose pal   served=%0d bones_written=%0d bone_unset=%0d bone_oob=%0d palettes_decoded=%0d",
             geom_pal_vertices_served_o, geom_pal_bones_written_o,
             geom_pal_bone_unset_o, geom_pal_bone_oob_o,
             geom_pose_palettes_decoded_o);
    $display("SMOKE: skin norm  vertices=%0d degenerate=%0d reduced=%0d fork_stall_cycles=%0d",
             geom_sn_vertices_o, geom_sn_degenerate_o, geom_sn_reduced_o,
             geom_sn_fork_stall_o);
    $display("SMOKE: replay     meshlets=%0d handles=%0d tri_in=%0d tri_out=%0d refused=%0d missed=%0d att_skew=%0d profile_mixed=%0d view_bad=%0d dq_refused=%0d",
             geom_rp_meshlets_o, geom_rp_groups_o, geom_rp_triangles_in_o,
             geom_rp_triangles_out_o, geom_rp_refused_o, geom_rp_missed_o,
             geom_rp_att_skew_o, geom_rp_profile_mixed_o, geom_rp_view_bad_o,
             geom_rp_dq_refused_o);
    $display("SMOKE: clip       submitted=%0d clipped=%0d culled=%0d setup_submitted=%0d (reference: %0d / %0d / %0d / %0d)",
             geom_clip_submitted_o, geom_clip_clipped_o, geom_clip_culled_o,
             geom_setup_triangles_submitted_o, SGF_EXP_REPLAYED, SGF_EXP_CLIPPED,
             SGF_EXP_CULLED, SGF_EXP_ACCEPTED);
    $display("SMOKE: raster     pixels=%0d bursts=%0d issued=%0d retired=%0d busy=%0d drained=%0d fatal=%0d stream_err=%0d overflow=%0d",
             render_pixels_o, render_bursts_o, render_issued_words_o,
             render_retired_words_o, render_busy_o, render_drained_o,
             render_fatal_o, render_stream_error_o, render_overflow_o);
    $display("SMOKE: renderlease leases_granted=%0d refused=%0d clears=%0d frames_admitted=%0d",
             v2_leases_granted_o, v2_leases_refused_o,
             v2_clear_handshakes_o, v2_frames_admitted_o);
    $display("SMOKE: terrain    tess_vertices=%0d tess_refs=%0d fills_forwarded=%0d refs_forwarded=%0d groups_opened=%0d b_grants=%0d",
             terr_tess_vertices_o, terr_tess_refs_o, terr_fills_forwarded_o,
             terr_refs_forwarded_o, terr_groups_opened_o, proj_b_grants_o);
    $display("SMOKE: projector  a_grants=%0d b_grants=%0d contended=%0d replay_triangles=%0d",
             proj_a_grants_o, proj_b_grants_o, proj_contended_o,
             proj_replay_triangles_o);
    $display("SMOKE: measure    snapshots=%0d", hist_snapshots_o);
    // Entry I4's two formerly-stuck counters. Both were structurally incapable
    // of moving before owner ruling 2026-09-19; both are asserted below.
    $display("SMOKE: spawn-on-collision: collisions_applied=%0d contacts_stick=%0d spawn_by_event=[%0d %0d %0d %0d]",
             part_collisions_applied_o, part_contacts_stick_o,
             part_spawn_by_event0_o, part_spawn_by_event1_o,
             part_spawn_by_event2_o, part_spawn_by_event3_o);
    $display("SMOKE: children written=%0d, of which at the POST-CONTACT position (y=%0d)=%0d",
             part_children_seen_q, PART_CONTACT_Y, part_children_at_contact_q);
    $display("SMOKE: child accounting: requested=%0d emitted=%0d refused=%0d written=%0d dropped_cap=%0d staging_stalls=%0d max_in_tick=%0d",
             part_children_requested_o, part_children_emitted_o, part_children_refused_o,
             part_children_written_o, part_children_dropped_capacity_o,
             part_staging_stall_cycles_o, part_max_children_in_tick_o);

    if (!part_tick_seen_q)
      $fatal(1, "SMOKE: PART.STATE never went busy -- SHELL.gpu_tick_o does not reach it");
    if (part_records_written_q == 0)
      $fatal(1, "SMOKE: no particle reached the write-back -- the STATE/UPDATE/COLLIDE ring does not carry a beat");
    // ---- THE ASSET PATH FIRST, because everything geometric below it is
    // downstream of a meshlet arriving. Firing the `-BadDescriptor` control
    // with these checks placed AFTER the ones below stopped the run at
    // "GEOM.SKIN transformed nothing" -- true, and three blocks away from
    // the refused descriptor that caused it.
    // ---- THE ASSET PATH, which is what makes the line below mean anything.
    // Each check names the wire it is evidence for, and they are ordered so
    // the FIRST one to fail is the furthest upstream: a dead descriptor read
    // and a dead vertex run look identical from the decoded count alone.
    if (geom_model_error)
      $fatal(1, "SMOKE: the SDRAM model raised a timing/protocol error (kinds=%b) -- every number below describes a machine talking to a memory it is abusing",
             geom_model_err);
    if (geom_mf_descriptors_fetched_o == 0)
      $fatal(1, "SMOKE: GEOM.MESHFETCH fetched no descriptor -- MESHFETCH -> MEM_ADAPTER -> the shell's guard -> SDRAM does not carry a read");
    if ((geom_mf_refused_format_o | geom_mf_refused_crc_o |
         geom_mf_refused_generation_o | geom_mf_refused_vertex_count_o |
         geom_mf_refused_triangle_count_o | geom_mf_refused_reserved_o |
         geom_mf_refused_zero_bound_o) != 0)
      $fatal(1, "SMOKE: GEOM.MESHFETCH refused the fixture descriptor [fmt/crc/gen/vc/tc/resv/bound]=[%0d %0d %0d %0d %0d %0d %0d] -- the bench's descriptor packing is wrong, so nothing below this line means anything",
             geom_mf_refused_format_o, geom_mf_refused_crc_o,
             geom_mf_refused_generation_o, geom_mf_refused_vertex_count_o,
             geom_mf_refused_triangle_count_o, geom_mf_refused_reserved_o,
             geom_mf_refused_zero_bound_o);
    // I37: the CRC WALKER, composed. Exactly one descriptor burst ended, its CRC
    // matched and it was eight beats -- so the verdict the fetcher latched was
    // COMPUTED, and the refusal check above passing is no longer a tie-off
    // agreeing with itself.
    if (geom_mf_crc_descriptors_o != 1 || geom_mf_crc_fail_o != 0 || geom_mf_crc_framing_o != 0)
      $fatal(1, "SMOKE: the descriptor CRC walker saw descriptors=%0d fail=%0d framing=%0d (want 1/0/0) -- the fold over the returning beats does not agree with the fixture's CRC word",
             geom_mf_crc_descriptors_o, geom_mf_crc_fail_o, geom_mf_crc_framing_o);
    // GEOM.CULL is a real block on the real matrix bank now. Under the
    // identity camera the fixture's bound is at the clip origin, so a cull
    // that rejects it is a wiring or configuration fault and not a verdict.
    if (geom_mf_culled_all_cameras_o != 0)
      $fatal(1, "SMOKE: GEOM.CULL rejected %0d meshlet(s) whose bound sits at the clip origin -- the shared matrix bank or the cull service is not carrying what it should",
             geom_mf_culled_all_cameras_o);
    if ((geom_mf_guard_denied_o != 0) || (geom_af_guard_denied_o != 0) ||
        (geom_ma_denied_o != 0))
      $fatal(1, "SMOKE: MEM.GUARD denied a geometry read (mf=%0d af=%0d adapter=%0d) -- the asset pool window or the ENGINE1 client identity is wrong",
             geom_mf_guard_denied_o, geom_af_guard_denied_o, geom_ma_denied_o);
    if (geom_af_refused_footprint_o != 0)
      $fatal(1, "SMOKE: GEOM.ASSETFETCH refused the footprint %0d time(s) -- the descriptor's offsets are misaligned or outside the pool. They are POOL-RELATIVE; adding the base twice is the documented way to get this",
             geom_af_refused_footprint_o);
    // THE FOOTPRINT IN BEATS, from the fixture: whole 64-byte lines of eight
    // beats -- the index run (3 bytes per triangle, line-aligned at
    // GEOM_IX_OFF) and the vertex run (32 bytes per vertex). A COUNT and not a
    // nonzero test -- a block that re-reads a line delivers the right bytes and
    // the wrong number of them, and only the count can see that.
    if (geom_af_beats_read_o != GEOM_FOOTPRINT_BEATS)
      $fatal(1, "SMOKE: GEOM.ASSETFETCH read %0d beats against the %0d this footprint is -- the machine did a different amount of work than the fixture describes",
             geom_af_beats_read_o, GEOM_FOOTPRINT_BEATS);
    if ((geom_af_err_beat_truncated_o | geom_af_err_beat_overrun_o |
         geom_af_err_beat_unowned_o | geom_ma_err_short_o |
         geom_ma_err_long_o | geom_ma_err_unowned_o) != 0)
      $fatal(1, "SMOKE: a beat-protocol fault on the shared geometry client [af trunc/over/unowned=%0d %0d %0d, adapter short/long/unowned=%0d %0d %0d]",
             geom_af_err_beat_truncated_o, geom_af_err_beat_overrun_o,
             geom_af_err_beat_unowned_o, geom_ma_err_short_o,
             geom_ma_err_long_o, geom_ma_err_unowned_o);
    // BOTH requesters used the ONE ENGINE1 client. If either is zero the
    // sharing is not being exercised and `geom_ma_contention_o` below is a
    // number about a machine with one requester in it.
    if ((geom_ma_jobs_a_o == 0) || (geom_ma_jobs_b_o == 0))
      $fatal(1, "SMOKE: GEOM.MEM_ADAPTER served a=%0d b=%0d logical requests -- one of the two fetchers never reached the shared client",
             geom_ma_jobs_a_o, geom_ma_jobs_b_o);
    // ONE draw, so ONE meshlet served -- and (entry I38, CLOSED) it was
    // RELEASED: GEOM.REPLAY's meshlet count moves only on the release it
    // proves, which the replay block above already asserted to be 1.
    if (geom_af_meshlets_fetched_o != 1)
      $fatal(1, "SMOKE: GEOM.ASSETFETCH served %0d meshlets for the ONE draw this bench issued",
             geom_af_meshlets_fetched_o);
    // GEOM.ASSEMBLE walked the index run the fetcher served: every triplet of
    // the fixture, none refused (every index is below the vertex count).
    if (geom_asm_triangles_o != SGF_N_TRIS)
      $fatal(1, "SMOKE: GEOM.ASSEMBLE emitted %0d triangles against the %0d the descriptor declares -- the ix_* index service between it and GEOM.ASSETFETCH is not carrying triplets",
             geom_asm_triangles_o, SGF_N_TRIS);
    if ((geom_asm_refused_limits_o != 0) || (geom_asm_refused_index_o != 0))
      $fatal(1, "SMOKE: GEOM.ASSEMBLE refused the fixture meshlet (limits=%0d index=%0d)",
             geom_asm_refused_limits_o, geom_asm_refused_index_o);

    if (geom_skin_vertices_transformed_o == 0)
      $fatal(1, "SMOKE: GEOM.SKIN transformed nothing");
    if (geom_vertices_sent_o == 0)
      $fatal(1, "SMOKE: GEOM.GROUP_SEQ sent no vertex to client A -- SKIN -> GROUP_SEQ is dead");
    if (proj_a_grants_o == 0)
      $fatal(1, "SMOKE: the shared projector granted client A zero times -- GROUP_SEQ -> PROJ_SUBSYSTEM is dead");
    if (geom_landings_o == 0)
      $fatal(1, "SMOKE: no landing reached GROUP_SEQ -- PROJ_SUBSYSTEM -> PROJ_LANE -> GROUP_SEQ does not close");
    if (hist_snapshots_o == 0)
      $fatal(1, "SMOKE: MEASURE.HISTOGRAM never snapped -- SHELL.gpu_tick_o does not reach it");

    // ---- CLIENT B, the thing this composition exists to prove -------------
    // Each of these can only move if a beat crossed a wire between two real
    // blocks. `proj_b_grants_o` is the one that matters most: it is the
    // ARBITER inside the shared projector granting its second client, and
    // before TERRAIN.GROUP_SEQ was composed it could not move at all.
    if (terr_tess_vertices_o == 0)
      $fatal(1, "SMOKE: TERRAIN.TESS emitted no window vertex -- GROUP_SEQ -> TESS job port is dead");
    if (terr_fills_forwarded_o == 0)
      $fatal(1, "SMOKE: TERRAIN.GROUP_SEQ forwarded no vertex to client B -- TESS -> GROUP_SEQ -> PROJ_SUBSYSTEM is dead");
    if (proj_b_grants_o == 0)
      $fatal(1, "SMOKE: the shared projector granted client B zero times -- the second client is still not live");
    if (terr_groups_opened_o == 0)
      $fatal(1, "SMOKE: TERRAIN.GROUP_SEQ opened no arena -- the open/gen handshake with the subsystem is dead");
    if (terr_release_unsafe_o != 0)
      $fatal(1, "SMOKE: TERRAIN.GROUP_SEQ released an arena with work outstanding (release_unsafe=%0d)",
             terr_release_unsafe_o);

    // ---- SPAWN ON COLLISION, owner ruling I4 ------------------------------
    // THE ASSERTION IS THE CORRECT BEHAVIOUR: a collision produces a child, and
    // the child is at the RULED position. The counters are kept beside it as a
    // separate POSITIVE CONTROL, because a test that asserts only "the counter
    // moved" is a test of the instrument and a test that asserts the defect
    // stops meaning anything the moment the defect is repaired.
    //
    // Every record this bench offers sits at y = 0 inside a heightfield at
    // +100, so all six contact and all six spawn one child each.
    if (part_children_seen_q == 0)
      $fatal(1, "SMOKE: a collision produced no child -- PART.COLLIDE -> PART.SPAWN does not carry the collision event");
    if (part_children_at_contact_q != part_children_seen_q)
      $fatal(1, "SMOKE: %0d of %0d collision-spawned children are NOT at the post-contact position y=%0d -- ruling I4 S3 says they must be",
             part_children_seen_q - part_children_at_contact_q,
             part_children_seen_q, PART_CONTACT_Y);
    if (part_children_written_o != part_children_seen_q)
      $fatal(1, "SMOKE: PART.STATE wrote %0d children but %0d carry kPartBornThisTick",
             part_children_written_o, part_children_seen_q);

    // THE CONSERVATION LAW, which the counters implied and did not enforce.
    // `reports/DEFECT-PART-STATE-LAST-CHILD-20260919.md` asked for exactly this
    // once the tick boundary was repaired: every child PART.SPAWN got ACCEPTED
    // into staging is written, or dropped at capacity and counted. It used to
    // be a $display of a known defect -- six emitted, five written, and every
    // counter reading zero -- and it is a $fatal now because there is no third
    // outcome left. A child that is REFUSED at the boundary was never accepted
    // and never counted emitted, so it does not appear on either side here; it
    // shows up as `staging_stalls`, printed above.
    //
    // This is the composed statement of the law. The block-level gate, which
    // sweeps the child's arrival cycle instead of taking the one arrival this
    // stimulus happens to produce, is tests/particles/part_state_tick_boundary.cpp.
    if (part_children_emitted_o !=
        (part_children_written_o + part_children_dropped_capacity_o))
      $fatal(1, "SMOKE: child conservation broken: emitted=%0d written=%0d dropped_cap=%0d -- %0d children left the machine with no counter naming them (see reports/DEFECT-PART-STATE-LAST-CHILD-20260919.md)",
             part_children_emitted_o, part_children_written_o,
             part_children_dropped_capacity_o,
             part_children_emitted_o - part_children_written_o -
             part_children_dropped_capacity_o);

    // THE POSITIVE CONTROLS, separate on purpose. These two counters could not
    // move at all before 2026-09-19; `spawn_by_event2_o` is the one the ruling
    // names as its acceptance.
    if (part_spawn_by_event2_o == 0)
      $fatal(1, "SMOKE: part_spawn_by_event2_o (COLLISION) is still zero -- the counter ruling I4 exists to unstick has not moved");
    if (part_collisions_applied_o == 0)
      $fatal(1, "SMOKE: part_collisions_applied_o is still zero -- PART.COLLIDE's collision_events_o does not reach the boundary");
    if (part_contacts_stick_o == 0)
      $fatal(1, "SMOKE: no STICK contact was counted -- the plane does not reach PART.COLLIDE");

    // ======================================================================
    // THE TERRAIN SAMPLE IS THE CORE'S OWN. ENTRY I6, composed 2026-09-19.
    //
    // Every particle now passes through PART.TERRAIN_TAP on its way to the
    // collider, and the first one over an uncached cell sends a real fill
    // through TERRAIN.HEIGHTTAP into the REAL compose cache's read ports. This
    // bench never fills that cache -- its pages fault by design -- so the
    // honest answers are: the fill LANDS (the round trip exists), the tap
    // answers OFF-PATCH (nothing is served), no particle is handed ground, and
    // PART.COLLIDE's "unavailable" is exactly the particles that were not.
    // The values themselves are differenced against the reference in
    // tests/particles/part_terrain_tap_directed.cpp; this is the composition.
    // ======================================================================
    $display("SMOKE: terrain sample  particles=%0d ground=%0d no_ground=%0d missed=%0d faults=%0d fills_landed=%0d | tap answered=%0d off_patch=%0d faults=%0d | collide unavailable=%0d",
             part_ter_particles_o, part_ter_ground_o, part_ter_no_ground_o,
             part_ter_missed_o, part_ter_faults_o, part_ter_fills_landed_o,
             terr_tap_answered_o, terr_tap_off_patch_o, terr_tap_faults_o,
             part_terrain_sample_unavailable_o);
    if (part_ter_particles_o == 0)
      $fatal(1, "SMOKE: no particle passed through PART.TERRAIN_TAP -- it is not on the PART.UPDATE -> PART.COLLIDE path");
    if (part_ter_particles_o != part_ter_ground_o + part_ter_no_ground_o +
                                part_ter_missed_o + part_ter_faults_o)
      $fatal(1, "SMOKE: terrain-sample census does not balance: %0d particles, %0d accounted for",
             part_ter_particles_o, part_ter_ground_o + part_ter_no_ground_o +
                                   part_ter_missed_o + part_ter_faults_o);
    if (part_ter_fills_landed_o == 0)
      $fatal(1, "SMOKE: no terrain cell fill came back -- the TERRAIN.HEIGHTTAP round trip through the compose cache is broken");
    if (terr_tap_off_patch_o == 0)
      $fatal(1, "SMOKE: the tap never answered OFF-PATCH, but this bench serves no patch -- it is reading something that is not the compose cache");
    if (part_ter_ground_o != 0)
      $fatal(1, "SMOKE: %0d particle(s) were handed ground from a compose cache this bench never filled", part_ter_ground_o);
    if (part_terrain_sample_unavailable_o != part_ter_particles_o - part_ter_ground_o)
      $fatal(1, "SMOKE: PART.COLLIDE counted %0d unavailable samples; the tap delivered %0d of %0d",
             part_terrain_sample_unavailable_o, part_ter_ground_o, part_ter_particles_o);
    if (part_ter_faults_o != 0 || terr_tap_faults_o != 0)
      $fatal(1, "SMOKE: terrain-sample FAULTS (sampler %0d, tap %0d) on a bench that places nothing wrong",
             part_ter_faults_o, terr_tap_faults_o);

    // ======================================================================
    // PART.TABLE ANSWERED. ENTRIES I2 AND I3, composed 2026-09-19.
    //
    // The three checks above are ALREADY table evidence and this is the one
    // place to say so plainly, because it is easy to read them as unchanged:
    // the STICK response, the "known" spawn rule and the child count are no
    // longer wires from this bench. Each was written into `zhao_part_table`
    // through `part_tbl_ld_*` and had to come back out through a DIFFERENT
    // consumer's descriptor port to move those counters. A table that
    // elaborated and was never asked reads as all-zero, and all-zero is
    // response IGNORE (no contact counted, no collision event) and known = 0
    // (no child). So `part_contacts_stick_o`, `part_spawn_by_event2_o` and
    // `part_children_seen_q` are each a read that returned a REAL VALUE.
    //
    // THE COLOUR BYTE IS THE EXACT ONE, and it is here because the three above
    // prove a non-zero descriptor arrived without pinning down WHICH. Species 0
    // was loaded with recipe R_COLOUR and curve bucket 0 with an arbitrary
    // 0xA5; PART.UPDATE reads the recipe from the table's update slice, looks
    // the curve up in its curve slice on the SAME cycle, and publishes the
    // result at this module's edge. Two slices, one byte, no adapter in
    // between. An off-by-one in the load word map, a swapped slice or a
    // wrong-cycle read all land somewhere other than 0xA5.
    if (part_colour_beats_q == 0)
      $fatal(1, "SMOKE: PART.UPDATE never raised out_colour_en_o -- the update slice's recipe field did not come back from PART.TABLE (entry I2), so nothing below it means anything");
    if (part_colour_wrong_q != 0)
      $fatal(1, "SMOKE: %0d of %0d retires carried a colour byte other than %02x, or none at all (last seen %02x) -- PART.TABLE's curve slice is not serving what was loaded into it (entry I3)",
             part_colour_wrong_q, part_colour_beats_q, TBL_CURVE_COLOUR, part_colour_last_q);
    if (part_colour_beats_q != part_updated_o)
      $fatal(1, "SMOKE: %0d colour samples against %0d particles updated -- the descriptor did not reach every beat",
             part_colour_beats_q, part_updated_o);
    $display("SMOKE: PART.TABLE -- loads[upd/col/spw/crv]=[%0d %0d %0d %0d] refused=%0d; %0d of %0d retires carried %02x from curve bucket 0; STICK contacts=%0d, collision spawns=%0d.",
             part_tbl_loads_update_o, part_tbl_loads_collide_o,
             part_tbl_loads_spawn_o, part_tbl_loads_curve_o,
             part_tbl_load_refused_o, part_colour_beats_q, part_updated_o,
             part_colour_last_q, part_contacts_stick_o, part_spawn_by_event2_o);
    // `part_tbl_load_refused_o` IS NOT CHECKED FOR A FIRING and its zero is not
    // quoted as evidence: at the console's PART_SPECIES_N = 128 a seven-bit
    // index cannot address outside the table, so the refusal is structurally
    // unreachable here. It is reachable and fired at SPECIES_N = 8 in
    // tests/particles/part_table_directed.cpp.

    // SPECIFICITY. Only event 2 fires in this stimulus: the records carry no
    // born flag, the age marker is disabled and the lifetime is unbounded. If
    // another spawn counter moves, the event vector is not what it claims.
    if (part_spawn_by_event0_o != 0 || part_spawn_by_event1_o != 0 ||
        part_spawn_by_event3_o != 0)
      $fatal(1, "SMOKE: a non-collision event fired (%0d/%0d/%0d) -- the event vector crossing PART.COLLIDE is wrong",
             part_spawn_by_event0_o, part_spawn_by_event1_o, part_spawn_by_event3_o);

    // ======================================================================
    // PACKET P-TERRAIN, 2026-09-19: the paging spine must be SEEN TO CARRY
    // DATA, block to block, not merely to elaborate.
    //
    // WAIT FOR THE WORK, DO NOT GUESS AT IT. Each page is 334 bursts and
    // each burst costs the played bridge's latency plus its beats, so a fixed
    // `repeat` would either waste time or -- far worse -- expire early and
    // read counters mid-flight, which looks exactly like a seam that does not
    // carry. The loop watches the block's own completion law instead: every
    // job produces exactly one completion, loaded or faulted.
    // ======================================================================
    guard = 0;
    //
    // THREE TERMS, MATCHING THE COMPLETION LAW THE CHECKS BELOW ASSERT. The
    // wait used `loaded + faulted` while the "one job, one completion" check
    // uses `loaded + faulted + refused`; a refusal IS a completion, so the two
    // disagreed about when the work was over and the wait would sit out its
    // whole 200,000-cycle budget on any run that refuses a job. That costs
    // nothing but time in production -- and it is exactly the case the slot-
    // overflow mutant creates deliberately.
    while (((terr_pl_pages_loaded_o + terr_pl_pages_faulted_o +
             terr_pl_pages_refused_o) < N_TERR_REC) &&
           (guard < 200000)) begin
      @(posedge gpu_clk);
      guard++;
    end
    repeat (64) @(posedge gpu_clk);

    // EVIDENCE BEFORE VERDICT. Printed unconditionally and BEFORE the first
    // check, because a $fatal on check 4 hides the eight numbers that say
    // which seam actually failed -- and those numbers are the reason to run
    // the bench at all.
    $display("SMOKE: TERRAIN spine counters after guard=%0d cycles:", guard);
    $display("SMOKE:   cmd   accepted=%0d refused=%0d verdict=%0d records=%0d crc_fails=%0d bridge_errs=%0d",
             terr_cmd_sets_accepted_o, terr_cmd_sets_refused_o, terr_cmd_done_verdict_o,
             terr_cmd_records_emitted_o, terr_cmd_crc_fails_o, terr_cmd_bridge_errs_o);
    $display("SMOKE:   seq   consumed=%0d issued_patches=%0d claims=%0d/%0d(same) refused=%0d loads=%0d skipped=%0d busy=%0d faults=%0d stray=%0d",
             terr_seq_records_consumed_o, terr_seq_patches_issued_o,
             terr_seq_claims_issued_o, terr_seq_claims_same_o, terr_seq_claims_refused_o,
             terr_seq_loads_issued_o, terr_seq_skipped_not_resident_o,
             terr_seq_busy_o, terr_seq_frame_faults_o, terr_seq_err_stray_ans_o);
    $display("SMOKE:   res   hits=%0d misses=%0d claims=%0d evictions=%0d crc_fail=%0d resident=%0d",
             terr_res_hits_o, terr_res_misses_o, terr_res_claims_o,
             terr_res_evictions_o, terr_res_crc_failures_o, terr_res_resident_o);
    $display("SMOKE:   loadq accepted=%0d issued=%0d high_water=%0d",
             terr_lq_accepted_o, terr_lq_issued_o, terr_lq_high_water_o);
    $display("SMOKE:   pl    loaded=%0d faulted=%0d crc_fails=%0d bytes=%0d guard_denied=%0d bridge_errs=%0d overflow=%0d",
             terr_pl_pages_loaded_o, terr_pl_pages_faulted_o, terr_pl_crc_fails_o,
             terr_pl_load_bytes_o, terr_pl_guard_denied_o, terr_pl_bridge_errs_o,
             terr_pl_slot_overflow_o);
    $display("SMOKE:   pl2   refused=%0d verdict=%0d incomplete=%0d hdr_ident_fails=%0d",
             terr_pl_pages_refused_o, terr_pl_fault_verdict_o, terr_pl_incomplete_o, terr_pl_hdr_ident_fails_o);
    $display("SMOKE:   fault island=%0d ix=%0d iz=%0d src_id=%0d (verdict 1=UNALIGNED 3=SLOT 4=STALE 5=CRC 6=SRC_ARENA 7=UNREACH 8=HDR_IDENT 9=INCOMPLETE)",
             terr_pl_fault_island_o, terr_pl_fault_ix_o, terr_pl_fault_iz_o, terr_pl_fault_src_id_o);
    $display("SMOKE:   arb   c0_bursts=%0d c1_bursts=%0d c1_wait_cycles=%0d c2_bursts=%0d c2_wait_cycles=%0d",
             terr_hps_c0_bursts_o, terr_hps_c1_bursts_o, terr_hps_c1_wait_cycles_o,
             terr_hps_c2_bursts_o, terr_hps_c2_wait_cycles_o);
    $display("SMOKE:   wb    sheets written=%0d refused=%0d faulted=%0d guard_denied=%0d acks_unmatched=%0d overdue=%0d | doorbell starved=%0d ret_overflow=%0d | rdshare jobs A=%0d B=%0d WB=%0d",
             terr_wb_sheets_written_o, terr_wb_sheets_refused_o, terr_wb_sheets_faulted_o,
             terr_wb_guard_denied_o, terr_wb_acks_unmatched_o, terr_wb_acks_overdue_o,
             terr_jdb_starved_cycles_o, terr_jdb_ret_overflow_o,
             terr_rdshare_jobs_a_o, terr_rdshare_jobs_b_o, terr_rdshare_jobs_wb_o);
    // Nothing here can dirty a page, so no writeback job may exist. A job
    // that appeared anyway would be the sequencer evicting a page it was
    // never told was dirty -- and with no grants posted it would sit in the
    // doorbell, visible as starvation, rather than go out with a made-up ticket.
    if ((terr_jdb_starved_cycles_o != 0) || (terr_wb_sheets_written_o != 0) ||
        (terr_wb_sheets_refused_o != 0) || (terr_wb_sheets_faulted_o != 0) ||
        (terr_rdshare_jobs_wb_o != 0) || (terr_hps_c2_bursts_o != 0) ||
        (terr_jdb_ret_overflow_o != 0))
      $fatal(1, "SMOKE: a writeback job appeared with no dirty page in the core (starved=%0d written=%0d refused=%0d faulted=%0d)",
             terr_jdb_starved_cycles_o, terr_wb_sheets_written_o, terr_wb_sheets_refused_o,
             terr_wb_sheets_faulted_o);
    $display("SMOKE:   probe hps_req_cycles=%0d hps_beats_in=%0d guard_req_cycles=%0d guard_wbeats=%0d",
             pr_hps_req_cy, pr_hps_beats, pr_guard_req_cy, pr_guard_wbeats);
    $display("SMOKE:   bridge bursts served by the played engine=%0d", hps_bursts_served_q);

    // ======================================================================
    // THE POSITIVE CONTROL, AND ITS POLARITY IS INVERTED ON PURPOSE
    // ======================================================================
    // Under -DZHAO_MUT_SLOT_OVERFLOW the DUT is the wrapper mutant and the
    // whole production terrain verdict below is COMPILED OUT, not merely
    // jumped over: the mutation legitimately breaks those checks -- a pool of
    // 512 slots refuses every job the directory places above 511, and an
    // illegal completion is never accepted, so the spine stops. Running them
    // against a deliberately broken machine would be asserting the bug. An
    // `if` plus `\$finish` is not enough here, because Verilator defers the
    // finish to the end of the time step and the straight-line statements
    // after it still execute -- which printed six %Fatal lines under a PASSING
    // exit code the first time this was written. `\ifdef`/`\else` removes
    // them from the build instead.
    //
    // What is asserted instead is the one thing this build exists to show:
    // THE COUNTER MOVES. It reads 0 in production under the identical
    // stimulus, which is the other half of the same measurement.
`ifdef ZHAO_MUT_SLOT_OVERFLOW
    $display("SMOKE: MUTANT zhao_console_core_slot_overflow_mutant -- terr_pl_slot_overflow_o=%0d (pages_refused=%0d verdict=%0d)",
             terr_pl_slot_overflow_o, terr_pl_pages_refused_o, terr_pl_fault_verdict_o);
    if (terr_pl_slot_overflow_o == 0)
      $fatal(1, "MUTANT FAILED: terr_pl_slot_overflow_o stayed 0 with TERR_POOL_SLOTS halved -- the counter could not be made to fire, so its zero in production is not evidence");
    $display("SMOKE: MUTANT PASS -- terr_pl_slot_overflow_o fired %0d time(s). The detector works; production's zero is a measurement.",
             terr_pl_slot_overflow_o);
    $finish;
`else

    // ---- 1. THE COMMAND WAS READ OVER THE BRIDGE -------------------------
    if (terr_cmd_sets_accepted_o != 1)
      $fatal(1, "SMOKE: TERRAIN.CMD accepted %0d sets and refused %0d (verdict %0d, crc seen %08x against %08x) -- the host packet was rejected",
             terr_cmd_sets_accepted_o, terr_cmd_sets_refused_o,
             terr_cmd_done_verdict_o, terr_cmd_done_crc_seen_o, terr_cmd_list_crc_i);
    if (terr_cmd_crc_fails_o != 0)
      $fatal(1, "SMOKE: TERRAIN.CMD folded a different CRC than the bench did -- the played bridge is not returning the bytes that were written");
    if (terr_cmd_bridge_errs_o != 0)
      $fatal(1, "SMOKE: TERRAIN.CMD saw %0d bridge errors -- the played HPS engine is malforming bursts",
             terr_cmd_bridge_errs_o);
    if (terr_cmd_records_emitted_o != N_TERR_REC)
      $fatal(1, "SMOKE: TERRAIN.CMD emitted %0d of %0d records -- it read the list and did not produce it",
             terr_cmd_records_emitted_o, N_TERR_REC);

    // ---- 2. THE CMD -> SEQ SEAM ------------------------------------------
    // The check that separates "CMD produced records" from "SEQ received
    // them". These are two counters in two blocks and only a real handshake
    // on a real wire can make them agree.
    if (terr_seq_records_consumed_o != terr_cmd_records_emitted_o)
      $fatal(1, "SMOKE: TERRAIN.CMD emitted %0d records and TERRAIN.SEQ consumed %0d -- the frame-ring/record seam is not carrying",
             terr_cmd_records_emitted_o, terr_seq_records_consumed_o);

    // ---- 3. THE SEQ <-> RESIDENCY SEAM, BOTH DIRECTIONS ------------------
    // A lookup that was ASKED and a lookup that was ANSWERED are different
    // facts in different blocks. An empty directory must miss both records,
    // and `terr_res_misses_o` moving is the directory saying it saw them.
    if (terr_res_misses_o < N_TERR_REC)
      $fatal(1, "SMOKE: the directory recorded %0d misses for %0d fresh patches -- TERRAIN.SEQ's lookups are not reaching it",
             terr_res_misses_o, N_TERR_REC);
    if (terr_seq_claims_issued_o != terr_res_claims_o)
      $fatal(1, "SMOKE: TERRAIN.SEQ issued %0d claims and the directory recorded %0d -- the claim seam drops or duplicates",
             terr_seq_claims_issued_o, terr_res_claims_o);
    if (terr_seq_claims_issued_o != N_TERR_REC)
      $fatal(1, "SMOKE: %0d claims for %0d distinct patches -- the answer path back into TERRAIN.SEQ is wrong",
             terr_seq_claims_issued_o, N_TERR_REC);

    // THE TRIPWIRE, QUOTED ONLY BECAUSE IT HAS BEEN SEEN TO FIRE ELSEWHERE.
    // `err_stray_ans_o` latches when a directory answer arrives with nothing
    // waiting for one -- the shape of an out-of-order or stale answer, whose
    // every consequence is silent (another island's ground drawn in this
    // island's place, with every count agreeing). It caught a real shim bug in
    // `tb_terrain_world.sv`, so its silence here is a measurement rather than
    // a decoration. This composition drives the directory WITHOUT that shim,
    // which is precisely the arrangement the tripwire is watching.
    if (terr_seq_err_stray_ans_o != 1'b0)
      $fatal(1, "SMOKE: TERRAIN.SEQ latched err_stray_ans -- a directory answer arrived with nothing waiting for it");
    if (terr_seq_frame_faults_o != 0)
      $fatal(1, "SMOKE: TERRAIN.SEQ raised %0d frame faults on an N_TERR_REC-record set against a 1,024-slot directory",
             terr_seq_frame_faults_o);

    // ---- 4. THE SEQ -> LOADQ -> PAGELOADER CHAIN -------------------------
    if (terr_seq_loads_issued_o != N_TERR_REC)
      $fatal(1, "SMOKE: TERRAIN.SEQ issued %0d loads for %0d non-resident patches", terr_seq_loads_issued_o, N_TERR_REC);
    if (terr_lq_accepted_o != terr_seq_loads_issued_o)
      $fatal(1, "SMOKE: TERRAIN.SEQ issued %0d load jobs and TERRAIN.LOADQ accepted %0d -- the queue's job port is not carrying",
             terr_seq_loads_issued_o, terr_lq_accepted_o);
    if (terr_lq_issued_o != terr_lq_accepted_o)
      $fatal(1, "SMOKE: TERRAIN.LOADQ took %0d jobs and handed on %0d -- jobs are stuck in the queue",
             terr_lq_accepted_o, terr_lq_issued_o);

    // THE LOADER ACTUALLY MOVED BYTES. This is the one check that cannot be
    // satisfied by handshakes alone: `load_bytes_o` counts bytes retired off
    // the bridge, so it can only move if the played engine's beats reached the
    // loader through the real `zhao_hps_arbiter`.
    if (terr_pl_load_bytes_o == 0)
      $fatal(1, "SMOKE: TERRAIN.PAGELOADER retired 0 bytes -- the HPS arbiter is not passing client 1's beats");
    if (terr_pl_bridge_errs_o != 0)
      $fatal(1, "SMOKE: TERRAIN.PAGELOADER saw %0d bridge errors", terr_pl_bridge_errs_o);

    // A WELL-FORMED RECORD IS NEVER REFUSED. Every record this bench writes is
    // 64-B aligned, inside the declared staging arena, on the live epoch and
    // names a slot the directory issued, so `pages_refused_o` must be zero.
    // This is the tripwire for the multi-record defect documented at
    // N_TERR_REC. It asserts the CORRECT behaviour -- a well-formed record is
    // never refused -- rather than the defect, so it survived the repair
    // instead of inverting, and it is what went red while the arena writer was
    // aliasing its records on top of each other.
    if (terr_pl_pages_refused_o != 0)
      $fatal(1, "SMOKE: TERRAIN.PAGELOADER REFUSED %0d well-formed job(s), verdict %0d, identity island=%0d ix=%0d iz=%0d src_id=%0d -- a job reached the loader with no payload. See the note at N_TERR_REC.",
             terr_pl_pages_refused_o, terr_pl_fault_verdict_o,
             terr_pl_fault_island_o, terr_pl_fault_ix_o, terr_pl_fault_iz_o,
             terr_pl_fault_src_id_o);

    // AND EVERY CLAIM WAS A FRESH ONE. `claims_same_o` moving on records with
    // distinct {island, ix, iz} is the other face of the same defect: two
    // records that both present a zero key collide in the directory.
    if (terr_seq_claims_same_o != 0)
      $fatal(1, "SMOKE: %0d of %0d claims came back SAME for records with distinct patch coordinates -- two records are presenting one key",
             terr_seq_claims_same_o, terr_seq_claims_issued_o);
    if (terr_pl_guard_denied_o != 0)
      $fatal(1, "SMOKE: TERRAIN.PAGELOADER was denied %0d guard requests -- it is writing outside TERRAIN.PAGE_POOL",
             terr_pl_guard_denied_o);

    // ONE JOB, ONE COMPLETION -- the loader's own stated law.
    // THREE TERMS, NOT TWO, and the third is the interesting one. The block's
    // own comment draws the line: "A refusal here has moved ZERO bytes, which
    // is what separates `pages_refused_o` from `pages_faulted_o`: the first is
    // a bad request, the second is a bad page." A check written as
    // loaded + faulted silently treats a REFUSED job as a lost one, which is
    // the flattering direction for a spine that is dropping work -- and it
    // read exactly that way on the first run here.
    if ((terr_pl_pages_loaded_o + terr_pl_pages_faulted_o + terr_pl_pages_refused_o) != N_TERR_REC)
      $fatal(1, "SMOKE: %0d jobs produced %0d loaded + %0d faulted + %0d refused completions -- 'one job, one completion' is broken (or the wait timed out at guard=%0d)",
             N_TERR_REC, terr_pl_pages_loaded_o, terr_pl_pages_faulted_o,
             terr_pl_pages_refused_o, guard);

    // EVERY COMPLETION REACHED THE DIRECTORY. This is the return leg of the
    // spine and the one a counter inside the loader cannot witness: the
    // directory validates the CRC on every completion it accepts, so a
    // completion that never arrived and one that arrived bad are told apart
    // here and nowhere else.
    if (terr_res_crc_failures_o != N_TERR_REC)
      $fatal(1, "SMOKE: the directory recorded %0d completions for %0d jobs -- TERRAIN.PAGELOADER's fin_* is not reaching TERRAIN.RESIDENCY",
             terr_res_crc_failures_o, N_TERR_REC);

    // THE WIDTH STEP HELD. The pool index is one bit wider than the
    // directory's handle; this composition drives it from a 10-bit wire, so
    // the extra bit must never be set on the way back. NOTE HONESTLY: this
    // counter is STRUCTURALLY unreachable in this arrangement, so its zero is
    // an invariant restated, not a detector that has been seen to fire. It
    // owes a committed mutant, and the core's header says so at its
    // declaration.
    if (terr_pl_slot_overflow_o != 0)
      $fatal(1, "SMOKE: %0d completions carried a pool slot outside the directory's range", terr_pl_slot_overflow_o);

    // ---- 5. WHAT IS *NOT* CLAIMED, stated so nobody reads more in --------
    // `terr_res_resident_o` is ZERO here and that is the CORRECT reading of
    // this composition, for two independent declared reasons:
    //   * the pages this bench plays are ZEROS, so their CRC cannot match the
    //     `expected_page_crc32c` in the record and ruling T7 says a CRC-failed
    //     page is never rendered. The fault is the required behaviour on the
    //     stimulus given, not a defect;
    //   * even a clean page would stop in ST_MIPGEN, because the directory
    //     publishes on TWO completions and the second's producer
    //     (TERRAIN.MIPFEED) is not composed -- see the refusal list in
    //     zhao_console_core.sv.
    // Asserting `resident == 0` would be asserting the gap, so this is a
    // $display. Asserting the CRC fault would be asserting a bug. What IS
    // asserted is the refusal's own law: every faulted page was counted.
    if (terr_pl_pages_faulted_o > 0)
      $display("SMOKE: NOTE %0d page(s) FAULTED as required -- the bench plays a zero page whose CRC cannot match the record's declared expected_page_crc32c, and ruling T7 forbids rendering it. The spine is proven by bytes retired (%0d) and by the record/claim seams above, not by residency.",
               terr_pl_pages_faulted_o, terr_pl_load_bytes_o);
    if (terr_res_resident_o != 0)
      $display("SMOKE: NOTE %0d page(s) reached RESIDENT, which this composition did not expect -- TERRAIN.MIPFEED is not composed, so check what produced the second completion.",
               terr_res_resident_o);

    $display("SMOKE: TERRAIN spine -- %0d records emitted and %0d consumed, %0d claims, %0d loads, %0d bytes retired over %0d played bursts.",
             terr_cmd_records_emitted_o, terr_seq_records_consumed_o,
             terr_res_claims_o, terr_lq_issued_o, terr_pl_load_bytes_o,
             hps_bursts_served_q);
`endif  // ZHAO_MUT_SLOT_OVERFLOW -- end of the production terrain verdict

    // ======================================================================
    // PACKET P-GEOM, 2026-09-19: the three blocks composed this pass must be
    // seen to CARRY DATA, not merely to elaborate.
    //
    // THESE CHECKS HAVE BEEN SEEN TO FIRE. A check nobody has watched fail is
    // a claim, so both were fired deliberately on the tree they guard and then
    // reverted:
    //
    //   * one reserved byte in `vdec_record` set nonzero ->
    //     "records_offered=4 decoded=0 refused[reserved/w0/format]=[4 0 0]"
    //     and the run stopped. GEOM.VDECODE's refusal path is live and it is
    //     the ONLY path to GEOM.SKIN -- the skinner transformed nothing.
    //
    //   * the triangle moved to x ~ 1172 px, outside the Z60 canvas ->
    //     "clip_submitted=16 clipped=16 culled=0 setup_submitted=0" and the
    //     scissor check below fired. That control is worth three things at
    //     once: the check fires, the CLIP -> SETUP seam carries the RIGHT
    //     triangles rather than any traffic (nothing reached setup), and the
    //     scissor really is the mode-derived 384-wide canvas rather than a
    //     constant or a stuck value -- a triangle at 4 px passes and one at
    //     1172 px does not.
    //
    // The shape of these checks is deliberate. Each one names the wire it is
    // evidence for and each is a CONSERVATION statement rather than "a counter
    // moved" -- `n offered == n decoded` cannot be satisfied by a block that
    // passes some records and swallows others, which a nonzero test can.
    // ======================================================================

    // GEOM.VDECODE -> GEOM.SKIN (entry I10's vertex half).
    // The records this bench offers are well-formed format-0, so every one must
    // decode and NONE may be refused. The three refusal counters are read as a
    // group for a specific reason: if the reserved-byte packing in
    // `vdec_record` were wrong, the block would refuse every record, the
    // decoded count would be zero, and "the wire is dead" and "the bench builds
    // a bad record" would look identical from the decoded count alone.
    // ---- and now the same conservation statement as before, end to end.
    // The records are byte-identical to the ones this bench used to hand the
    // decoder directly, so this equality holding means the descriptor read,
    // the cull, the index run, the vertex run and the whole memory path
    // under them delivered what the fixture put in.
    if (geom_vd_vertices_o != N_GEOM_VERTS)
      $fatal(1, "SMOKE: GEOM.VDECODE decoded %0d of the %0d records the descriptor declares -- the vertex stream does not cross from GEOM.ASSETFETCH into the decoder",
             geom_vd_vertices_o, N_GEOM_VERTS);
    if ((geom_vd_reserved_nz_count_o != 0) || (geom_vd_w0_illegal_count_o != 0) ||
        (geom_vd_format_bad_count_o != 0))
      $fatal(1, "SMOKE: GEOM.VDECODE refused a well-formed record (reserved=%0d w0=%0d format=%0d) -- the harness's format-0 packing is wrong, so nothing below this line means anything",
             geom_vd_reserved_nz_count_o, geom_vd_w0_illegal_count_o,
             geom_vd_format_bad_count_o);
    if (geom_skin_vertices_transformed_o != geom_vd_vertices_o)
      $fatal(1, "SMOKE: GEOM.SKIN transformed %0d of GEOM.VDECODE's %0d vertices -- the decoder's output does not reach the skinner",
             geom_skin_vertices_transformed_o, geom_vd_vertices_o);

    // GEOM.VDECODE -> GEOM.POSE's PALETTE STORE -> GEOM.SKIN. This is entry
    // I10's other half, the one that was a missing BLOCK rather than missing
    // wiring. The store is now the ONLY path between those two blocks, so the
    // check above cannot pass unless this one did -- and asserting it
    // separately is what tells a dead store apart from a dead decoder.
    if (geom_pal_vertices_served_o != geom_vd_vertices_o)
      $fatal(1, "SMOKE: the pose palette served %0d of GEOM.VDECODE's %0d vertices -- the store between the decoder and the skinner passes no traffic",
             geom_pal_vertices_served_o, geom_vd_vertices_o);
    // NO POSE WAS DECODED (entry I29), so every bone reference must have taken
    // the identity substitution: two per vertex, since these records are rigid
    // and name bone 0 twice. A ZERO here would mean the store handed out
    // whatever its memory happened to hold, which is the one outcome that must
    // never be silent -- and it is also this check's own positive control,
    // because the number can only be right if the substitution really ran.
    if (geom_pal_bone_unset_o != (geom_vd_vertices_o * 2))
      $fatal(1, "SMOKE: the pose palette counted %0d non-resident bone references against %0d expected -- with no decoded pose every reference must be substituted AND counted",
             geom_pal_bone_unset_o, geom_vd_vertices_o * 2);
    if (geom_pal_bone_oob_o != 0)
      $fatal(1, "SMOKE: the pose palette saw %0d out-of-range bone indices -- every record this bench builds names bone 0",
             geom_pal_bone_oob_o);

    // ---- GEOM.SKIN.NORM, the OTHER branch of the palette store's fork -------
    // (core entry I43.) The fork is an AND, so the normal path and the position
    // path accept the SAME beat on the SAME clock. That is the whole claim this
    // check exists to test, and it is testable precisely because the two blocks
    // count independently: `geom_sn_vertices_o` is GEOM.SKIN.NORM's own accept
    // counter and `geom_skin_vertices_transformed_o` is GEOM.SKIN's completion
    // counter, and NO single register enable drives both. A fork that let one
    // branch drop a beat separates them.
    if (geom_sn_vertices_o != geom_vd_vertices_o)
      $fatal(1, "SMOKE: GEOM.SKIN.NORM accepted %0d of the palette store's %0d vertices -- the normal branch of the fork is not carrying the traffic the position branch is",
             geom_sn_vertices_o, geom_vd_vertices_o);

    // AND THE WHOLE RESULT IS COMPUTABLE BY HAND, so this is a DIFFERENTIAL and
    // not an "it moved" check. Every term is a fixture constant:
    //
    //   `vdec_record` packs the normal (nx, ny, nz) = (127, 0, 0) and w0 = 64,
    //   which is the RIGID case (w1 = 64 - w0 = 0, so bone B contributes
    //   nothing). No pose is decoded (entry I29), so the palette substitutes
    //   the IDENTITY for both bones, whose row 0 is [ONE_FX16, 0, 0, 0] with
    //   ONE_FX16 = 65536. So, following `zhao_geom_skin_norm`'s own law:
    //
    //     A_row0 . N = 65536 * 127            = 8,323,072
    //     n[0] = w0 * (A_row0 . N) = 64 * that = 532,676,608 = 2^22 * 127
    //     n[1] = n[2] = 0                       (rows 1 and 2 select ny, nz)
    //     |n|  = isqrt(n[0]^2) = n[0]           (a perfect square, floor-exact)
    //
    // That last line is the one worth having: the magnitude EQUALLING the x
    // component is what an exact floor root of a perfect square gives and what
    // an approximation would not, so this single equality is the evidence that
    // the `zhao_field_isqrt` instance composed beside this block really ran.
    if (geom_sn_degenerate_o != 0)
      $fatal(1, "SMOKE: GEOM.SKIN.NORM called %0d normals degenerate -- the fixture's packed normal is (127,0,0) and blends to a real direction through either bone",
             geom_sn_degenerate_o);
    if (geom_sn_n_x_o != SN_EXPECT_NX)
      $fatal(1, "SMOKE: GEOM.SKIN.NORM's world normal x is %0d, expected %0d (64 * 65536 * 127) -- the two-bone blend or the identity substitution is wrong",
             geom_sn_n_x_o, SN_EXPECT_NX);
    if ((geom_sn_n_y_o != 0) || (geom_sn_n_z_o != 0))
      $fatal(1, "SMOKE: GEOM.SKIN.NORM's world normal is (%0d, %0d, %0d) -- rows 1 and 2 select ny and nz, both zero in the fixture, so a nonzero lane means the row indexing is wrong",
             geom_sn_n_x_o, geom_sn_n_y_o, geom_sn_n_z_o);
    if (geom_sn_n_mag_o != 64'(SN_EXPECT_NX))
      $fatal(1, "SMOKE: GEOM.SKIN.NORM's magnitude is %0d, expected %0d -- isqrt of a perfect square must return it exactly, so this is the root service failing to answer or answering approximately",
             geom_sn_n_mag_o, SN_EXPECT_NX);

    // `geom_sn_reduced_o` READS ZERO AND THE REASON IS STRUCTURAL, not untested.
    // The range reduction fires when max|n| >= 2^30, and the value computed
    // above is 532,676,608 against 1,073,741,824 -- just under half. It cannot
    // be reached from this bench at all while entry I29 is open, because a
    // palette NOTHING FILLS substitutes the identity for every bone, so the
    // blend is always 2^22 times a signed byte and is bounded by 2^22 * 128 =
    // 536,870,912 whatever the record carries. The counter's positive control
    // is `tests/geometry/skin_norm_rtl_directed.cpp`, which drives the block
    // directly with real matrices large enough to trip it. Asserted zero here
    // WITH that reason, rather than quietly not looked at.
    if (geom_sn_reduced_o != 0)
      $fatal(1, "SMOKE: GEOM.SKIN.NORM range-reduced %0d vertices -- with identity bones the blend is bounded by 2^29 and cannot reach the 2^30 threshold",
             geom_sn_reduced_o);

    // ---- GEOM.REPLAY: the fixture meshlet, replayed into BOTH views --------
    // Every number here is the REFERENCE's (smoke_geom_fixture.svh): the replay
    // takes each of the meshlet's SGF_N_TRIS triangles once and emits it once
    // per visible view, drops nothing (every corner is written and every handle
    // current), and releases the meshlet exactly once.
    if ((geom_rp_meshlets_o != 1) || (geom_rp_groups_o != 2) ||
        (geom_rp_triangles_in_o != SGF_N_TRIS) ||
        (geom_rp_triangles_out_o != SGF_EXP_REPLAYED))
      $fatal(1, "SMOKE: GEOM.REPLAY meshlets=%0d handles=%0d tri_in=%0d tri_out=%0d -- the reference wants 1 / 2 / %0d / %0d",
             geom_rp_meshlets_o, geom_rp_groups_o, geom_rp_triangles_in_o,
             geom_rp_triangles_out_o, SGF_N_TRIS, SGF_EXP_REPLAYED);
    if ((geom_rp_refused_o | geom_rp_missed_o | geom_rp_profile_mixed_o |
         geom_rp_view_bad_o | geom_rp_dq_refused_o) != 0)
      $fatal(1, "SMOKE: GEOM.REPLAY dropped or faulted: refused=%0d missed=%0d profile_mixed=%0d view_bad=%0d dq_refused=%0d",
             geom_rp_refused_o, geom_rp_missed_o, geom_rp_profile_mixed_o,
             geom_rp_view_bad_o, geom_rp_dq_refused_o);
    // The attribute store and the arena answered on the SAME clock every time.
    // `-BadAttribute` makes one reply late; this is the check it trips.
    if (geom_rp_att_skew_o != 0)
      $fatal(1, "SMOKE: GEOM.REPLAY saw the attribute store answer out of step with the arena %0d time(s) -- slot 1..6 would belong to a different lookup",
             geom_rp_att_skew_o);

    // ---- GEOM.CLIP -> GEOM.SETUP, EXACTLY the reference's split -------------
    // `clipped` is the behind-the-eye triangle in both views -- the near plane
    // is a whole-primitive rejection -- so this equality holding means the
    // projector's behind verdict crossed the arena and the replay intact.
    if ((geom_clip_submitted_o != SGF_EXP_REPLAYED) ||
        (geom_clip_clipped_o != SGF_EXP_CLIPPED) ||
        (geom_clip_culled_o != SGF_EXP_CULLED))
      $fatal(1, "SMOKE: GEOM.CLIP submitted=%0d clipped=%0d culled=%0d -- the reference wants %0d / %0d / %0d",
             geom_clip_submitted_o, geom_clip_clipped_o, geom_clip_culled_o,
             SGF_EXP_REPLAYED, SGF_EXP_CLIPPED, SGF_EXP_CULLED);
    if (geom_setup_triangles_submitted_o != SGF_EXP_ACCEPTED)
      $fatal(1, "SMOKE: GEOM.SETUP took %0d of the reference's %0d accepted triangles -- the clip->setup seam or the shell's triangle door is not carrying",
             geom_setup_triangles_submitted_o, SGF_EXP_ACCEPTED);

    // A PIXEL NOW TRAVERSES THE RENDER PATH, so this is a CHECK and no longer
    // a note. What stood here said "raster pixels=0 because frames_admitted=0
    // -- the V2 renderer lease is not driven by this bench ... driving the
    // lease belongs with VIDEO.SLOTMGR, which the completion register still
    // lists as not connected", and it was wrong three times over. Kept as
    // quotation rather than deleted, because each error is a different way to
    // be wrong and all three are cheap to make again:
    //
    //   * THE LEASE WAS ALWAYS DRIVEN. `render_frame_begin_i` is this bench's,
    //     and a bind-in probe measured `lease_open` opening at cycle 16 and
    //     staying open for all 500,391 cycles. Nothing about the barrier, the
    //     CDC, the bridge or the manager was missing.
    //   * THE REGISTER DOES NOT LIST VIDEO.SLOTMGR. `completion_register.py`'s
    //     BUILT-BUT-NOT-CONNECTED list has 36 names and no slot manager in it.
    //     A sentence about another tool's output is a claim, and this one had
    //     never been read back.
    //   * WHAT WAS ACTUALLY MISSING WAS TWO WIRES, neither of them a lease:
    //     GEOM.SETUP's `out_area2_o` never reached `tri_area2_i` (so the tile
    //     pipe read PROFILE AREA BAD and SANK all 72 jobs), and
    //     `u_guard_render` was handed the BLITTER's window (so MEM.GUARD
    //     deny-all'd every burst and FBWRITE latched `fatal_error_o`).
    //
    // The moral is the one this file already keeps for the triangle door: a
    // confident sentence naming an absent owner is the most expensive kind of
    // wrong, because it sends the next reader somewhere else entirely.
    //
    // `retired_words_o` is checked and not just `pixels_written_o`: retirement
    // is the VRAM arbiter's credit stream, which is the only thing that means
    // the words LANDED. The block's own issue count says nothing about that.
    if (v2_frames_admitted_o == 0)
      $fatal(1, "SMOKE: no frame was ADMITTED -- the renderer lease granted %0d and refused %0d; the request is held until admission, so this is the lease or the barrier, not the stimulus",
             v2_leases_granted_o, v2_leases_refused_o);
    if (render_pixels_o == 0)
      $fatal(1, "SMOKE: %0d frame(s) were ADMITTED and the shell still rasterised 0 pixels from %0d triangles -- that is a real render-path fault. Check `tri_area2_i` (0 is PROFILE AREA BAD and sinks every job) and the render guard's window before anything else",
             v2_frames_admitted_o, geom_setup_triangles_submitted_o);
    // EXACTLY THE REFERENCE'S PIXELS. `render_pixels_o` counts pixels WRITTEN
    // and the pipeline resolves WHOLE tiles, so the count is the union of the
    // tiles `zref::Binner` names across both views, times 256 -- derived in
    // smoke_geom_fixture_gen.cpp from project_vertex, Clip and Setup, never read
    // off a run. It was 1536 when sixteen copies of one hand-placed triangle
    // came through the bench's door (6 tiles); it is SGF_EXP_PIXELS now because
    // the triangles are the meshlet's own, in two views.
    if (render_pixels_o != SGF_EXP_PIXELS)
      $fatal(1, "SMOKE: the render path wrote %0d pixels and the reference names %0d tiles = %0d pixels",
             render_pixels_o, SGF_EXP_TILES, SGF_EXP_PIXELS);
    if (render_fatal_o)
      $fatal(1, "SMOKE: the render path wrote %0d pixel(s) and latched fatal_error_o -- a guard denial or a broken pixel stream; the frame is unpublishable",
             render_pixels_o);
    if (render_retired_words_o != render_issued_words_o)
      $fatal(1, "SMOKE: the render path issued %0d word(s) and the arbiter retired %0d -- issued-but-not-landed is not a rendered frame",
             render_issued_words_o, render_retired_words_o);
    // ----------------------------------------------------------------------
    // GEOM.ATTRPACK, 2026-09-19. THE PLANES HAVE A PRODUCER.
    //
    // THE RATIO IS THE CHECK, not the absolute numbers. One shared
    // `zhao_geom_attrsetup` core runs three lanes per triangle, so `planes`
    // must be exactly three times `triangles`. A lane that stopped asking --
    // the specific way a time-multiplexed front end goes wrong -- keeps the
    // previous triangle's plane, breaks nothing else, and leaves every
    // handshake, every pixel count and every other counter looking healthy.
    if (geom_attrpack_triangles_o != geom_setup_triangles_submitted_o)
      $fatal(1, "SMOKE: GEOM.ATTRPACK counted %0d triangle(s) and GEOM.SETUP counted %0d -- the fork off GEOM.CLIP is no longer handing both of them one ready",
             geom_attrpack_triangles_o, geom_setup_triangles_submitted_o);
    if (geom_attrpack_planes_o != 3 * geom_attrpack_triangles_o)
      $fatal(1, "SMOKE: GEOM.ATTRPACK packed %0d plane(s) for %0d triangle(s) and three lanes per triangle is the contract -- a lane stopped asking and its plane is the PREVIOUS triangle's",
             geom_attrpack_planes_o, geom_attrpack_triangles_o);
    if (geom_attrpack_triangles_o == 0)
      $fatal(1, "SMOKE: GEOM.ATTRPACK never saw a triangle, so every plane the shell read was its reset value");

    $display("SMOKE: NOTE raster pixels=%0d over %0d burst(s), every issued word retired by the arbiter, from %0d triangle(s) in %0d admitted frame(s). The path is proven END TO END, and the three Packet-D attribute planes now have a PRODUCER: GEOM.ATTRPACK packed %0d plane(s) for %0d triangle(s) -- three lanes through one shared GEOM.ATTRSETUP -- from GEOM.CLIP's own winding-flipped vertex attributes, so depth and both texture coordinates vary across the surface instead of interpolating to zero. What is STILL not proven is the MATERIAL, and the REASON CHANGED AGAIN on 2026-09-19: THE RULING LANDED AND IT WAS NOT THE LAST THING. `spec/memory_rules.md` 5f.1 now rules that a published slot is named by the handle index of the resource it holds, making the residency directory {index:24} -> {slot, base, extent, kind}, and `zhao_mem_upload` publishes all five: base and extent were never missing VALUES, only dropped ones, already bounds-checked against `cfg_region_*` before a byte moved, and the kind already travelled as publish_tag_o. MATERIAL.RESOLVE is BUILT -- 91 directed checks differenced against `zref::material::Resolver`, every counter seen to fire, a committed positive control for owner ruling D-3's cache tag -- and is STILL NOT COMPOSED. The docket called the ruling the last thing between this island and sampling anything; it is not, and the four that remain are NAMED here rather than left to be rediscovered. (1) MEM.UPLOAD cannot be composed anywhere yet: `zhao_hps_arbiter` carries exactly TWO clients and both are taken in both instances -- CMD.DMA and DEBUG.FRAMEBLIT in `zhao_shell_top_v2`, TERRAIN.CMD and TERRAIN.PAGELOADER in `u_terr_hps_arb` -- so a third is the owner ruling core entry I27 already records. (2) The record fetch wants a third ENGINE1 requester and `zhao_geom_mem_adapter` has exactly two, GEOM.MESHFETCH and GEOM.ASSETFETCH. (3) The resolve REQUEST has no honest producer: `cmd_draw_material_set_o` (I41) and `geom_mf_job_*` (I36) are both boundaries while CMD.SCHEDULER's draw path is absent, and joining the two live wires that ARE here would pair meshlet N's triangles with meshlet M's material -- the fault entry I39 refuses by name, because GEOM.MESHFETCH's result register has moved on by the time the meshlet is offered. (4) `tri_flat_request_i` wants the binding page's palette slot, palette generation and response class besides, which MATERIAL.RESOLVE's own header says it does not own. So `tri_flat_request_i` still has no producer, sample_count, material_recipe and base_binding_selector are all still zero, and the texture island still samples nothing. Perspective-correct interpolation is composed; the surface it would sample is not bound. What changed is the SHAPE of the remaining distance: four composed seams, three of which are arbitration, rather than one undecided sentence.",
             render_pixels_o, render_bursts_o,
             geom_setup_triangles_submitted_o, v2_frames_admitted_o,
             geom_attrpack_planes_o, geom_attrpack_triangles_o);

    // ======================================================================
    // PACKET P-SURFACE, 2026-09-19: SURFACE.STAMP <-> SURFACE.SHEET.
    //
    // WAIT FOR THE WORK, DO NOT GUESS AT IT -- the same rule the terrain wait
    // above is written against. A ring of a few dozen texels at SQ_RADIX = 1
    // costs a few thousand cycles plus SURFACE.SHEET's 4,096-cycle clear
    // sweep, and a fixed `repeat` that expired early would read the counters
    // mid-flight and look exactly like a seam that does not carry.
    //
    // THE CONSERVATION CHECK HAS BEEN SEEN TO FIRE, on the tree it guards and
    // then reverted: `zhao_console_core`'s `.wr_handle_i` was driven with a
    // literal instead of `surf_wr_handle`, and the run stopped with
    //
    //     texels[stamp/sheet]=[60 0] ... wr_miss_seen=1
    //     "SURFACE.SHEET wrote 0 texels against SURFACE.STAMP's 60 retired"
    //
    // which is worth three things at once. The check fires. `wr_miss_o` and
    // its sticky latch are live. And -- the reason this control was chosen
    // over an easier one -- SURFACE.STAMP still completed its stamp and still
    // retired all sixty texels while SURFACE.SHEET wrote none, so the two
    // counters are demonstrably NOT two operands moving together. CLAUDE.md's
    // rule is that a detector whose operands share one enable cannot fire;
    // this is the measurement that says these two do not.
    // ======================================================================
    guard = 0;
    while (!surf_done_seen_q && (guard < 200000)) begin
      @(posedge gpu_clk);
      guard++;
    end
    repeat (16) @(posedge gpu_clk);

    // EVIDENCE BEFORE VERDICT.
    $display("SMOKE: surface   stamps=%0d done_seen=%0d rejected_seen=%0d idle[stamp/sheet]=[%0d %0d] guard=%0d",
             surf_stamps_o, surf_done_seen_q, surf_rejected_seen_q,
             surf_stamp_idle_o, surf_sheet_idle_o, guard);
    $display("SMOKE: surface   texels[stamp/sheet]=[%0d %0d] results_taken=%0d occupancy=%b busy=%0d overflow_seen=%0d wr_miss_seen=%0d",
             surf_stamp_texels_touched_o, surf_sheet_texels_touched_o,
             surf_res_records_q, surf_res_occupancy_o, surf_res_busy_o,
             surf_overflow_seen_q, surf_wr_miss_seen_q);

    // 1. THE COMMAND WAS TAKEN AND COMPLETED EXACTLY ONCE. Not ">= 1": a
    //    producer whose `valid` outlives its acceptance re-submits, and this
    //    repository has shipped that defect with byte-identical output. One
    //    command offered, one stamp counted.
    if (surf_stamps_o != 32'd1)
      $fatal(1, "SMOKE: SURFACE.STAMP completed %0d stamps against exactly one command offered -- 0 means the dispatch never crossed, >1 means the bench re-submitted",
             surf_stamps_o);
    if (surf_rejected_seen_q)
      $fatal(1, "SMOKE: SURFACE.STAMP rejected the stamp (residency overflow) -- SURFACE.SHEET refused the ACQUIRE, so the pair's control channel carries but its allocator does not");

    // 2. THE ACQUIRE REACHED THE SHEET AND THE SHEET ANSWERED IT. Occupancy is
    //    SURFACE.SHEET's own state, set only by an ACQUIRE it allocated. The
    //    request channel is internal to the DUT, so a live bit here can only
    //    have come from STAMP's `req_op_o`.
    if (surf_res_occupancy_o == '0)
      $fatal(1, "SMOKE: SURFACE.SHEET holds no resident slot after a completed stamp -- STAMP.req_* -> SHEET.req_* does not carry the ACQUIRE");
    if (surf_overflow_seen_q)
      $fatal(1, "SMOKE: SURFACE.SHEET raised res_overflow on a single stamp against %0d free slots", SURF_SLOTS);

    // 3. THE LOOP CLOSED -- and this is the check the composition exists for.
    //    The two counters are incremented by DIFFERENT BLOCKS on DIFFERENT
    //    events: SURFACE.STAMP counts a texel retiring out of its stage 2,
    //    SURFACE.SHEET counts a write that HIT a resident handle. They cannot
    //    both be moved by one wire, so their agreement is evidence rather than
    //    a tautology -- which is exactly the property CLAUDE.md says a checker
    //    needs, and exactly what a detector whose two operands share one
    //    register enable does not have.
    //
    //    A handle that failed to cross would leave STAMP's count high and
    //    SHEET's at zero (the block drops a non-resident write and says so on
    //    `wr_miss_o`), so this is a conservation statement and not "a counter
    //    moved".
    if (surf_stamp_texels_touched_o == 0)
      $fatal(1, "SMOKE: SURFACE.STAMP visited no texel -- a ring of outer radius 6 m on a 64 m envelope covers a few dozen, so the command's geometry never reached the walker");
    if (surf_sheet_texels_touched_o != surf_stamp_texels_touched_o)
      $fatal(1, "SMOKE: SURFACE.SHEET wrote %0d texels against SURFACE.STAMP's %0d retired -- the STAMP.wr_* -> SHEET.wr_* seam loses writes (wr_miss_seen=%0d)",
             surf_sheet_texels_touched_o, surf_stamp_texels_touched_o,
             surf_wr_miss_seen_q);
    if (surf_wr_miss_seen_q)
      $fatal(1, "SMOKE: SURFACE.SHEET dropped at least one write as non-resident -- the handle on the write port is not the handle the ACQUIRE made resident");

    // 4. THE RESULT STREAM CARRIES (entry I32's port). One beat per retired
    //    texel by SURFACE.STAMP's own retirement law: stage 2 cannot free
    //    until BOTH the write and the result have been accepted.
    if (surf_res_records_q != surf_stamp_texels_touched_o)
      $fatal(1, "SMOKE: stamp_results delivered %0d beats against %0d texels retired -- the result port and the write port disagree about how many texels this stamp touched",
             surf_res_records_q, surf_stamp_texels_touched_o);

    // 5. THE FIELD ENGINE IS COMPOSED AND REACHABLE (entry I42). One request
    //    was offered at the edge, at a slot nothing was loaded into. All three
    //    of these must hold together: a grant proves the arbiter accepted it, a
    //    noprog proves the `hdr_loaded` interlock answered, and the status
    //    proves the answer came back down the shared response bus. Any one of
    //    them alone could be produced by a stuck signal.
    if (fld_grants_o != 32'd1)
      $fatal(1, "SMOKE: FIELD's arbiter granted %0d requests against exactly one offered -- 0 means the edge client does not reach the engine, >1 means the bench re-offered",
             fld_grants_o);
    if (fld_noprog_o != 32'd1)
      $fatal(1, "SMOKE: FIELD refused %0d runs against one request at an unloaded slot -- 0 means the program store's loaded interlock is not consulted, so a slot with no microcode would have been WALKED",
             fld_noprog_o);
    if (fld_resp_status_o != 8'hF0)
      $fatal(1, "SMOKE: FIELD answered status %02x, not ST_NO_PROGRAM -- the refusal did not reach the shared response bus, and a caller reading only the lanes would have taken four zeroes for a field",
             fld_resp_status_o);
    if (fld_runs_o != 32'd0)
      $fatal(1, "SMOKE: FIELD completed %0d walks against zero loaded programs -- the sequencer ran something, and there is nothing in the store for it to have run",
             fld_runs_o);

    // 6. THE STAMP BRUSH STAYED DISARMED, which is what makes check 3 above
    //    still a measurement of the same stamp. `fld_stamp_slot_valid_i` is low
    //    here, so the adapter must never have started a walk.
    if (surf_fld_stamps_o != 32'd0)
      $fatal(1, "SMOKE: the field stamp adapter started %0d walks with no program resident -- the console's arm gate is not holding, and the stamp consumed field records nobody authored",
             surf_fld_stamps_o);
    if (surf_fld_texels_o != 32'd0)
      $fatal(1, "SMOKE: the field stamp adapter delivered %0d records with no program resident",
             surf_fld_texels_o);

    // ---- MEM.UPLOAD across the TERRAIN.BUILD socket (entry I47) ----------
    // End to end through REAL blocks: the shell's `zhao_hps_arbiter_n` and
    // `zhao_hps_bridge` fetch the staged bytes, the REAL `zhao_mem_guard`
    // admits slot 6 into TERRAIN.PAGE_POOL, `zhao_vram_arbiter` and
    // `zhao_sdram_ctrl` write them and return credits, and only then does the
    // block publish. Every field of the published row is checked against the
    // request that caused it, because a publication naming the wrong base is
    // a well-formed row for the wrong surface.
    $display("SMOKE: upload    done=%0d status=%0d published=%0d rows=%0d bursts=%0d wait=%0d slot=%0d gen=%04x tag=%0d index=%06x base=%08x extent=%0d",
             upl_done_seen_q, upl_status_seen_q, upl_published_o, upl_pub_seen_q,
             sh_bursts_q, upl_hps_wait_o, upl_pub_slot_q, upl_pub_gen_q,
             upl_pub_tag_q, upl_pub_index_q, upl_pub_base_q, upl_pub_extent_q);
    if (upl_done_seen_q != 1 || upl_status_seen_q != 8'd0)
      $fatal(1, "SMOKE: MEM.UPLOAD finished %0d time(s) with status %0d, expected once with 0 (kUploadOk) -- refused=%032x",
             upl_done_seen_q, upl_status_seen_q, upl_refused_o);
    if (upl_pub_seen_q != 1 || upl_published_o != 16'd1)
      $fatal(1, "SMOKE: MEM.UPLOAD published %0d row(s) (census %0d), expected exactly one",
             upl_pub_seen_q, upl_published_o);
    if (sh_bursts_q != (UPL_WORDS_C / 8))
      $fatal(1, "SMOKE: the shell's bridge served %0d HPS bursts for a %0d-byte upload, expected %0d",
             sh_bursts_q, UPL_WORDS_C * 8, UPL_WORDS_C / 8);
    if (upl_pub_index_q != upl_req_index_i || upl_pub_slot_q != upl_req_dst_slot_i ||
        upl_pub_gen_q != upl_req_new_gen_i || upl_pub_tag_q != upl_req_tag_i ||
        upl_pub_base_q != upl_req_vram_addr_i || upl_pub_extent_q != upl_req_len_i)
      $fatal(1, "SMOKE: MEM.UPLOAD published a row that is not the request's -- 5f.1's directory would name the wrong surface");
    if (upl_refused_o != '0)
      $fatal(1, "SMOKE: MEM.UPLOAD's refusal census moved (%032x) on a legal upload", upl_refused_o);
    if (shell_err_wfifo_o || shell_err_route_o)
      $fatal(1, "SMOKE: the shell's write-queue (%b) or routing (%b) tripwire fired with the slot-6 socket live",
             shell_err_wfifo_o, shell_err_route_o);

    // ---- DEBUG.TRACE against its producer (core entry I45) ----------------
    // The composition check the ring's contract asks for, and it is an
    // EQUALITY on purpose. `cmd_commands_o` is CMD.DECODER's own count of
    // records walked; `dbg_trace_count_o` is what the ring stored with stage 0
    // armed. They must agree exactly: the ring drops only when full (64 events)
    // and this packet carries far fewer, so any difference is a lost event, a
    // stage byte that is not 0, or an arming gate that does not gate.
    $display("SMOKE: trace     armed=%b stored=%0d dropped=%0d against decoder records=%0d",
             dbg_trace_armed_o, dbg_trace_count_o, dbg_trace_dropped_o,
             cmd_commands_o);
    if (dbg_trace_armed_o != 7'b000_0001)
      $fatal(1, "SMOKE: DEBUG.TRACE armed mask reads %b, expected 000_0001 -- the arm write did not land",
             dbg_trace_armed_o);
    if (dbg_trace_dropped_o != 32'd0)
      $fatal(1, "SMOKE: DEBUG.TRACE dropped %0d event(s) into a 64-deep ring from %0d records",
             dbg_trace_dropped_o, cmd_commands_o);
    if (dbg_trace_count_o != cmd_commands_o)
      $fatal(1, "SMOKE: DEBUG.TRACE stored %0d event(s) against %0d records walked by CMD.DECODER -- the record port and the ring disagree",
             dbg_trace_count_o, cmd_commands_o);
    // AND THE EQUALITY ABOVE IS TWO ZEROS AGREEING TODAY. Said out loud rather
    // than left for somebody to discover, because a green check quoted as
    // evidence for something it cannot see is this project's most expensive
    // recurring mistake. This bench submits NO COMMAND PACKET: nothing here
    // writes the frame ring or feeds CMD.DMA, so CMD.DECODER walks no records
    // and the ring has nothing to store. It was written as a live equality so
    // that it starts testing the seam the moment a packet arrives, and the
    // check WAS seen to fire -- an earlier version of it fatal'd on exactly
    // this zero, which is how the limitation was found rather than assumed.
    //
    // WHAT IS PROVEN HERE is narrower and is the armed-mask readback above:
    // `dbg_trace_arm_mask_i` written at this bench's edge comes back out of
    // `dbg_trace_armed_o` through the composition, so the instance is live,
    // reachable and not pruned. The rec -> ev seam itself is covered by
    // tests/debug/debug_trace_rtl_directed.cpp at the block, and by nothing at
    // the composition until something submits a packet here.
    if (cmd_commands_o == 32'd0)
      $display("SMOKE: NOTE DEBUG.TRACE's record-to-event equality is UNTESTED in this bench -- no command packet is submitted, so CMD.DECODER walked 0 records and the ring stored 0. The check above is live and will engage the moment a packet does. What this bench does prove about the ring is the arming readback.");

    $display("SMOKE: PASS -- the connected core carries traffic on every wire this bench can reach.");
    $finish;
  end

  // A bench that hangs must say so rather than be killed by a wrapper.
  initial begin
    #40ms;
    $fatal(1, "SMOKE: wall-clock timeout");
  end

endmodule : tb_zhao_console_core_smoke
