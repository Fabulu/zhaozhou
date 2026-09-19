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

  localparam int unsigned N_PART_RECORDS = 6;

  // ---- the terrain the particles are driven into (see the acceptance below) --
  // Every record this bench offers sits at y = 0, so a heightfield at +100
  // position LSBs puts all six INSIDE the surface and every one of them makes a
  // contact. `PART_CLEAR_EPS` is `zhao_part_collide`'s own default; it is
  // written here rather than read, so a change to that parameter fails this
  // bench loudly instead of silently agreeing with itself.
  localparam int signed   PART_TER_H     = 100;
  localparam int unsigned PART_CLEAR_EPS = 2;
  localparam int unsigned N_GEOM_VERTS   = 4;
  // Triangles offered to GEOM.CLIP. Three, not one: one proves a wire, three
  // prove the handshake releases and can be offered again, which is the bug
  // shape this repository has already been bitten by (a level held for a whole
  // offer window re-submitted one meshlet fifteen times).
  localparam int unsigned N_GEOM_TRIS    = 16;
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
  logic [6:0]              part_upd_spc_index_o;
  logic [3:0]              part_upd_spc_recipe_i;
  logic [PART_AGE_W-1:0]   part_upd_spc_lifetime_i;
  logic [PART_AGE_W-1:0]   part_upd_spc_age_mark_i;
  logic [7:0]              part_upd_spc_drag_i;
  logic signed [10:0]      part_upd_spc_grav_i;
  logic signed [10:0]      part_upd_spc_strength_i;
  logic signed [17:0]      part_upd_spc_cx_i;
  logic signed [17:0]      part_upd_spc_cy_i;
  logic signed [17:0]      part_upd_spc_cz_i;
  logic signed [10:0]      part_upd_spc_p0_i;
  logic signed [10:0]      part_upd_spc_p1_i;
  logic signed [10:0]      part_upd_spc_p2_i;
  logic [3:0]              part_crv_index_o;
  logic [5:0]              part_crv_size_i;
  logic [7:0]              part_crv_colour_i;
  logic                    part_fld_valid_i;
  logic signed [10:0]      part_fld_ax_i;
  logic signed [10:0]      part_fld_ay_i;
  logic signed [10:0]      part_fld_az_i;
  logic [2:0]              part_col_d_response_i;
  logic signed [PART_FX_W-1:0] part_col_d_restitution_i;
  logic signed [PART_FX_W-1:0] part_col_d_friction_i;
  logic signed [PART_FX_W-1:0] part_col_d_damping_i;
  logic                    part_ter_valid_i;
  logic signed [PART_POS_W-1:0] part_ter_height_i;
  logic signed [PART_NRM_W-1:0] part_ter_nx_i;
  logic signed [PART_NRM_W-1:0] part_ter_ny_i;
  logic signed [PART_NRM_W-1:0] part_ter_nz_i;
  logic                    part_plane_en_i;
  logic signed [PART_NRM_W-1:0] part_plane_nx_i;
  logic signed [PART_NRM_W-1:0] part_plane_ny_i;
  logic signed [PART_NRM_W-1:0] part_plane_nz_i;
  logic signed [31:0]      part_plane_c_i;
  logic [6:0]              part_spw_spc_species_o;
  logic [1:0]              part_spw_spc_event_o;
  logic                    part_spw_spc_known_i;
  logic [6:0]              part_spw_spc_child_spc_i;
  logic [4:0]              part_spw_spc_count_i;
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
  logic                    geom_vd_v_valid_i;
  logic                    geom_vd_v_ready_o;
  logic [255:0]            geom_vd_v_bytes_i;
  logic [15:0]             geom_vd_v_src_id_i;
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
  logic                    geom_job_valid_i;
  logic                    geom_job_ready_o;
  logic [GEOM_INDEX_W-1:0] geom_job_count_i;
  logic [GEOM_NVIEWS-1:0]  geom_job_view_mask_i;
  logic [15:0]             geom_job_src_id_i;
  logic                    geom_grp_valid_o;
  logic                    geom_grp_ready_i;
  logic [GEOM_ARENA_W-1:0] geom_grp_arena_o;
  logic [GEOM_GEN_W-1:0]   geom_grp_gen_o;
  logic [GEOM_INDEX_W-1:0] geom_grp_count_o;
  logic                    geom_grp_view_o;
  logic [15:0]             geom_grp_src_id_o;
  logic                    geom_rel_valid_i;
  logic [GEOM_ARENA_W-1:0] geom_rel_arena_i;
  logic                    geom_org_we_i;
  logic [GEOM_ARENA_W-1:0] geom_org_arena_i;
  logic signed [31:0]      geom_org_x_i;
  logic signed [31:0]      geom_org_y_i;
  logic signed [31:0]      geom_org_z_i;
  logic                    geom_look_valid_i;
  logic                    geom_look_ready_o;
  logic [GEOM_ARENA_W-1:0] geom_look_arena_i;
  logic [GEOM_GEN_W-1:0]   geom_look_gen_i;
  logic [GEOM_INDEX_W-1:0] geom_look_index_i;
  logic                    geom_rep_valid_o;
  logic                    geom_rep_hit_o;
  logic                    geom_rep_refuse_o;
  logic [GEOM_PAYLOAD_W-1:0] geom_rep_payload_o;
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
  logic                    terr_lat_req_o;
  logic [5:0]              terr_lat_vi_o;
  logic [5:0]              terr_lat_vj_o;
  logic                    terr_lat_surface_o;
  logic signed [31:0]      terr_lat_h_i;
  logic signed [31:0]      terr_lat_wx_i;
  logic signed [31:0]      terr_lat_wz_i;
  logic                    terr_cs_req_o;
  logic [4:0]              terr_cs_ci_o;
  logic [4:0]              terr_cs_cj_o;
  logic [1:0]              terr_cs_substance_i;

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

  logic                    terr_is_valid_o;
  logic                    terr_is_ready_i;
  logic [TERR_SLOTW_C-1:0] terr_is_slot_o;
  logic [TERR_GENW_C-1:0]  terr_is_gen_o;
  logic [31:0]             terr_is_epoch_o;
  logic [31:0]             terr_is_island_o;
  logic signed [15:0]      terr_is_ix_o;
  logic signed [15:0]      terr_is_iz_o;
  logic                    terr_is_cslot_valid_o;
  logic [TERR_CSLOTW_C-1:0] terr_is_cslot_o;
  logic [15:0]             terr_is_flags_o;
  logic [7:0]              terr_is_view_mask_o;
  logic [7:0]              terr_is_priority_o;
  logic [31:0]             terr_is_src_id_o;

  logic                    terr_dm_valid_i;
  logic                    terr_dm_ready_o;
  logic [TERR_SLOTW_C-1:0] terr_dm_slot_i;
  logic [TERR_GENW_C-1:0]  terr_dm_gen_i;
  logic [31:0]             terr_dm_epoch_i;
  logic                    terr_dm_bd_i;
  logic                    terr_dm_f_i;
  logic                    terr_dm_mips_i;

  logic                    terr_unpin_valid_i;
  logic                    terr_unpin_ready_o;
  logic [TERR_SLOTW_C-1:0] terr_unpin_slot_i;
  logic [TERR_GENW_C-1:0]  terr_unpin_gen_i;
  logic [31:0]             terr_unpin_epoch_i;

  logic                    terr_chk_valid_i;
  logic [TERR_SLOTW_C-1:0] terr_chk_slot_i;
  logic [TERR_GENW_C-1:0]  terr_chk_gen_i;
  logic [31:0]             terr_chk_epoch_i;
  logic                    terr_chk_valid_o;
  logic                    terr_chk_stale_o;

  logic                    terr_wb_valid_o;
  logic                    terr_wb_ready_i;
  logic [TERR_SLOTW_C-1:0] terr_wb_slot_o;
  logic [TERR_GENW_C-1:0]  terr_wb_gen_o;
  logic [31:0]             terr_wb_epoch_o;
  logic [31:0]             terr_wb_island_o;
  logic signed [15:0]      terr_wb_ix_o;
  logic signed [15:0]      terr_wb_iz_o;
  logic [31:0]             terr_wb_src_id_o;
  logic                    terr_wb_done_valid_i;
  logic [TERR_SLOTW_C-1:0] terr_wb_done_slot_i;

  logic                    terr_wback_valid_i;
  logic                    terr_wback_ready_o;
  logic [TERR_SLOTW_C-1:0] terr_wback_slot_i;
  logic [TERR_GENW_C-1:0]  terr_wback_gen_i;
  logic [31:0]             terr_wback_epoch_i;

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
  logic [239:0] tri_invw_plane_i;
  logic [239:0] tri_u_over_w_plane_i;
  logic [239:0] tri_v_over_w_plane_i;
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
  zhao_guard_req_t geom_guard_req_i;
  zhao_guard_rsp_t geom_guard_rsp_o;
  logic            geom_beat_valid_o;
  logic [63:0]     geom_beat_data_o;
  logic            geom_beat_last_o;
  // ---- GEOM.CLIP's input (entry I24) and the CLIP/SETUP evidence ------------
  localparam int unsigned GEOM_CLIP_ATTRW = 7 * 32;
  logic                    geom_clip_tri_valid_i;
  logic                    geom_clip_tri_ready_o;
  logic signed [20:0]      geom_clip_tri_ax_i, geom_clip_tri_ay_i;
  logic signed [20:0]      geom_clip_tri_bx_i, geom_clip_tri_by_i;
  logic signed [20:0]      geom_clip_tri_cx_i, geom_clip_tri_cy_i;
  logic [2:0]              geom_clip_tri_behind_i;
  logic [15:0]             geom_clip_tri_src_id_i;
  logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_a_i, geom_clip_attr_b_i, geom_clip_attr_c_i;
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
             // Nothing in this composition writes over the bridge --
             // TERRAIN.WRITEBACK is the only terrain writer and it is entry
             // I28, not composed. Granting and dropping would be a lie; this
             // reports a bridge error so a write that appears here is LOUD.
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
  int unsigned geom_verts_sent_q;
  int unsigned geom_tris_sent_q;
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
      px = FX16_ONE * 32'sh2 + 32'(n);
      py = FX16_ONE;
      pz = FX16_ONE * 32'sh4;
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

  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      geom_verts_sent_q <= 0;
      geom_vd_v_valid_i <= 1'b0;
    end else begin
      if (geom_vd_v_valid_i && geom_vd_v_ready_o) begin
        geom_verts_sent_q <= geom_verts_sent_q + 1;
        geom_vd_v_valid_i <= 1'b0;
      end
      if (reset_released_q && !geom_vd_v_valid_i &&
          (geom_verts_sent_q < N_GEOM_VERTS)) begin
        geom_vd_v_valid_i  <= 1'b1;
        geom_vd_v_bytes_i  <= vdec_record(geom_verts_sent_q);
        geom_vd_v_src_id_i <= 16'h00A1;
      end
    end
  end

  // --------------------------------------------------------------------------
  // THE TRIANGLE FRONT DOOR (entry I24: the replay customer specified at I11
  // does not exist, so the harness presents the three PROJECTED screen corners
  // it would emit -- and nothing downstream of that).
  //
  // This path was COMPLETELY DEAD before this pass: `render_tri_valid_i` was a
  // boundary input on the core and this bench tied it to zero, so the shell's
  // rasteriser had never seen a triangle in a connected-core run. It is now
  // GEOM.CLIP -> GEOM.SETUP -> the shell, and the test is that a triangle put
  // in at CLIP comes out of SETUP as edge functions AND that the shell's
  // rasteriser reports pixels for it.
  //
  // The triangle is deliberately front-facing, wholly inside the Z60 canvas and
  // wholly in front of the camera (`behind` = 0), because this bench asserts
  // that beats crossed wires, not that a clipper clips. S12.8 subpixels: 256
  // subpixels to the pixel.
  // --------------------------------------------------------------------------
  localparam int unsigned SUBPX = 256;

  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      geom_tris_sent_q      <= 0;
      geom_clip_tri_valid_i <= 1'b0;
    end else begin
      if (geom_clip_tri_valid_i && geom_clip_tri_ready_o) begin
        geom_tris_sent_q      <= geom_tris_sent_q + 1;
        geom_clip_tri_valid_i <= 1'b0;
      end
      if (render_frame_open_q && !geom_clip_tri_valid_i &&
          (geom_tris_sent_q < N_GEOM_TRIS)) begin
        // Pixels (4,4) (40,4) (4,40), in S12.8 subpixels. 2A is positive, so
        // GEOM.CLIP does not have to flip it -- the flip is exercised by that
        // block's own directed test and asserting it here would be a second,
        // weaker copy of a check that already exists.
        geom_clip_tri_valid_i  <= 1'b1;
        geom_clip_tri_ax_i     <= 21'sd1024;
        geom_clip_tri_ay_i     <= 21'sd1024;
        geom_clip_tri_bx_i     <= 21'sd10240;
        geom_clip_tri_by_i     <= 21'sd1024;
        geom_clip_tri_cx_i     <= 21'sd1024;
        geom_clip_tri_cy_i     <= 21'sd10240;
        geom_clip_tri_src_id_i <= 16'h00B2;
      end
    end
  end

  // --------------------------------------------------------------------------
  // THE TERRAIN LATTICE (entry I22: TERRAIN.COMPCACHE's front is not composed,
  // so the harness plays the MEMORY -- which is exactly what the completion
  // plan allows it to play, and nothing more).
  //
  // A flat lattice: every vertex sits at height 0 with its placed world x/z
  // taken from the requested (vi, vj). That is a memory MODEL, not a terrain:
  // the bench asserts only that beats crossed wires, never a shape. The reply
  // is REGISTERED one cycle after the request, which is the port's own law
  // ("registered, data valid the cycle AFTER the request") -- answering
  // combinationally would test a timing the real store does not offer.
  // --------------------------------------------------------------------------
  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      terr_lat_h_i  <= '0;
      terr_lat_wx_i <= '0;
      terr_lat_wz_i <= '0;
    end else if (terr_lat_req_o) begin
      terr_lat_h_i  <= '0;
      terr_lat_wx_i <= FX16_ONE * 32'(terr_lat_vi_o);
      terr_lat_wz_i <= FX16_ONE * 32'(terr_lat_vj_o);
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
    part_upd_spc_recipe_i = '0;
    part_upd_spc_lifetime_i = '0;
    part_upd_spc_age_mark_i = '0;
    part_upd_spc_drag_i = '0;
    part_upd_spc_grav_i = '0;
    part_upd_spc_strength_i = '0;
    part_upd_spc_cx_i = '0;
    part_upd_spc_cy_i = '0;
    part_upd_spc_cz_i = '0;
    part_upd_spc_p0_i = '0;
    part_upd_spc_p1_i = '0;
    part_upd_spc_p2_i = '0;
    part_crv_size_i = '0;
    part_crv_colour_i = '0;
    part_fld_valid_i = '0;
    part_fld_ax_i = '0;
    part_fld_ay_i = '0;
    part_fld_az_i = '0;
    part_col_d_response_i = '0;
    part_col_d_restitution_i = '0;
    part_col_d_friction_i = '0;
    part_col_d_damping_i = '0;
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
    // Entry I27: the compose engine is not composed. Its door is held READY so
    // the sequencer is not stalled on a consumer that does not exist -- the
    // patch issues are counted at `terr_seq_patches_issued_o` and go nowhere,
    // which is exactly what a boundary looks like and is declared as such.
    terr_is_ready_i = 1'b1;
    terr_dm_valid_i = '0;
    terr_dm_slot_i = '0;
    terr_dm_gen_i = '0;
    terr_dm_epoch_i = '0;
    terr_dm_bd_i = '0;
    terr_dm_f_i = '0;
    terr_dm_mips_i = '0;
    terr_unpin_valid_i = '0;
    terr_unpin_slot_i = '0;
    terr_unpin_gen_i = '0;
    terr_unpin_epoch_i = '0;
    terr_chk_valid_i = '0;
    terr_chk_slot_i = '0;
    terr_chk_gen_i = '0;
    terr_chk_epoch_i = '0;
    // Entry I28: TERRAIN.WRITEBACK is not composed. Its job port is held
    // READY and its completion is NEVER asserted, which is the honest pair: a
    // job that leaves and an answer that is not invented. Nothing in this
    // bench can cause a writeback anyway -- that needs a dirty-F eviction and
    // `terr_dm_f_i` is never raised.
    terr_wb_ready_i = 1'b1;
    terr_wb_done_valid_i = '0;
    terr_wb_done_slot_i = '0;
    terr_wback_valid_i = '0;
    terr_wback_slot_i = '0;
    terr_wback_gen_i = '0;
    terr_wback_epoch_i = '0;

    part_ter_valid_i = '0;
    part_ter_height_i = '0;
    part_ter_nx_i = '0;
    part_ter_ny_i = '0;
    part_ter_nz_i = '0;
    part_plane_en_i = '0;
    part_plane_nx_i = '0;
    part_plane_ny_i = '0;
    part_plane_nz_i = '0;
    part_plane_c_i = '0;
    part_spw_spc_known_i = '0;
    part_spw_spc_child_spc_i = '0;
    part_spw_spc_count_i = '0;
    part_cap_full_i = '0;
    part_hist_sel_i = '0;
    geom_vd_v_bytes_i = '0;
    geom_vd_v_src_id_i = '0;
    geom_clip_tri_behind_i = '0;
    geom_clip_attr_a_i = '0;
    geom_clip_attr_b_i = '0;
    geom_clip_attr_c_i = '0;
    geom_clip_cull_mode_i = '0;
    geom_clip_tri_ax_i = '0;
    geom_clip_tri_ay_i = '0;
    geom_clip_tri_bx_i = '0;
    geom_clip_tri_by_i = '0;
    geom_clip_tri_cx_i = '0;
    geom_clip_tri_cy_i = '0;
    geom_clip_tri_src_id_i = '0;
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
    geom_job_valid_i = '0;
    geom_job_count_i = '0;
    geom_job_view_mask_i = '0;
    geom_job_src_id_i = '0;
    geom_grp_ready_i = '0;
    geom_rel_valid_i = '0;
    geom_rel_arena_i = '0;
    geom_org_we_i = '0;
    geom_org_arena_i = '0;
    geom_org_x_i = '0;
    geom_org_y_i = '0;
    geom_org_z_i = '0;
    geom_look_valid_i = '0;
    geom_look_arena_i = '0;
    geom_look_gen_i = '0;
    geom_look_index_i = '0;
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
    terr_cs_substance_i = '0;
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
    tri_invw_plane_i = '0;
    tri_u_over_w_plane_i = '0;
    tri_v_over_w_plane_i = '0;
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
    hps_req_grant_i = '0;
    hps_rd_valid_i = '0;
    hps_rd_data_i = '0;
    hps_rd_last_i = '0;
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
    geom_guard_req_i = '0;

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
    phy_dq_i = '0;

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
    part_upd_spc_lifetime_i = '0;
    part_wr_ready_i         = 1'b1;

    // ---- SPAWN ON COLLISION, the acceptance for owner ruling I4 -------------
    // Before the ruling this bench held the collider quiet (response IGNORE, no
    // terrain sample) and PRINTED two counters it knew could not move. Both of
    // them CAN move now, and making them move is the point of this stimulus.
    //
    //   * a heightfield at +100 with an up normal, so every record -- all of
    //     which sit at y = 0 -- is inside the surface and makes a contact;
    //   * STICK (3'd2), the simplest PLACING response: both coefficients are
    //     zero, so the velocity result needs no coefficient arithmetic and the
    //     placement is the exact vertical h + CLEAR_EPS;
    //   * a spawn descriptor that resolves, with one child per fired event.
    //     ONLY event 2 fires here -- the records carry no born flag, the age
    //     marker is disabled and the lifetime is unbounded -- so the other
    //     three spawn counters staying at zero is a specificity check, not an
    //     oversight.
    part_ter_valid_i        = 1'b1;
    part_ter_height_i       = PART_POS_W'(PART_TER_H);
    part_ter_nx_i           = '0;
    part_ter_ny_i           = PART_NRM_W'(1 << 10);   // NRM_Q = 10: a unit +Y normal
    part_ter_nz_i           = '0;
    part_col_d_response_i   = 3'd2;                   // STICK
    part_spw_spc_known_i    = 1'b1;
    part_spw_spc_child_spc_i = 7'd0;
    part_spw_spc_count_i    = 5'd1;
    proj_en_i               = 1'b1;
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

    repeat (20) @(posedge gpu_clk);
    rst_n = 1'b1;
    repeat (4) @(posedge gpu_clk);
    reset_released_q = 1'b1;

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
    @(posedge gpu_clk);
    render_frame_begin_i <= 1'b1;
    @(posedge gpu_clk);
    render_frame_begin_i <= 1'b0;
    render_frame_open_q  <= 1'b1;         // releases the triangle offer above

    // THE RENDER FRAME IS OPENED, FILLED AND CLOSED IN ONE SEQUENCE, and that
    // is a correction rather than a style choice. The first version opened it
    // here and closed it after the run's two-frame wait -- half a million gpu
    // cycles and two `gpu_tick_o` edges later. Every counter read clean
    // (`drained=1 busy=0 fatal=0 stream_err=0`) and `render_pixels_o` was 0,
    // which reads exactly like "the triangles never reached the binner" and is
    // not that: a render frame does not survive the console's frame boundary.
    // Reading the reassuring zeros as evidence about the door would have sent
    // the next person to re-check wiring that was already correct.
    guard = 0;
    while ((geom_tris_sent_q < N_GEOM_TRIS) && (guard < 20000)) begin
      @(posedge gpu_clk);
      guard = guard + 1;
    end
    if (geom_tris_sent_q < N_GEOM_TRIS)
      $fatal(1, "SMOKE: GEOM.CLIP accepted only %0d of %0d offered triangles -- backpressure from GEOM.SETUP or from the shell's binner",
             geom_tris_sent_q, N_GEOM_TRIS);

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

    @(posedge gpu_clk);
    geom_job_count_i     <= GEOM_INDEX_W'(N_GEOM_VERTS);
    geom_job_view_mask_i <= 2'b01;
    geom_job_src_id_i    <= 16'h1234;
    geom_job_valid_i     <= 1'b1;
    geom_grp_ready_i     <= 1'b1;
    guard = 0;
    while (!(geom_job_valid_i && geom_job_ready_o) && (guard < 1000)) begin
      @(posedge gpu_clk);
      guard = guard + 1;
    end
    @(posedge gpu_clk);
    geom_job_valid_i <= 1'b0;
    if (guard >= 1000)
      $fatal(1, "SMOKE: GEOM.GROUP_SEQ never accepted a job (job_ready_o stuck low)");

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
    $display("SMOKE: vdecode    records_offered=%0d decoded=%0d refused[reserved/w0/format]=[%0d %0d %0d]",
             geom_verts_sent_q, geom_vd_vertices_o,
             geom_vd_reserved_nz_count_o, geom_vd_w0_illegal_count_o,
             geom_vd_format_bad_count_o);
    $display("SMOKE: geometry   skinned=%0d vertices_sent=%0d a_grants=%0d landings=%0d groups_opened=%0d groups_sealed=%0d",
             geom_skin_vertices_transformed_o, geom_vertices_sent_o,
             proj_a_grants_o, geom_landings_o,
             geom_groups_opened_o, geom_groups_sealed_o);    $display("SMOKE: pose pal   served=%0d bones_written=%0d bone_unset=%0d bone_oob=%0d palettes_decoded=%0d",
             geom_pal_vertices_served_o, geom_pal_bones_written_o,
             geom_pal_bone_unset_o, geom_pal_bone_oob_o,
             geom_pose_palettes_decoded_o);
    $display("SMOKE: tri door   offered=%0d clip_submitted=%0d clipped=%0d culled=%0d setup_submitted=%0d",
             geom_tris_sent_q, geom_clip_submitted_o, geom_clip_clipped_o,
             geom_clip_culled_o, geom_setup_triangles_submitted_o);
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
      $fatal(1, "SMOKE: no STICK contact was counted -- the terrain sample does not reach PART.COLLIDE");

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
    $display("SMOKE:   arb   c0_bursts=%0d c1_bursts=%0d c1_wait_cycles=%0d",
             terr_hps_c0_bursts_o, terr_hps_c1_bursts_o, terr_hps_c1_wait_cycles_o);
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
    if (geom_vd_vertices_o != geom_verts_sent_q)
      $fatal(1, "SMOKE: GEOM.VDECODE decoded %0d of %0d offered records -- the vertex stream does not cross the block",
             geom_vd_vertices_o, geom_verts_sent_q);
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

    // GEOM.CLIP -> GEOM.SETUP -> the shell's triangle door (the far end of I13).
    // Every triangle offered is front-facing and wholly inside the canvas, so
    // none may be clipped or culled; anything else means the scissor this core
    // derives from the video mode is not the rectangle the block was given.
    if (geom_clip_submitted_o != geom_tris_sent_q)
      $fatal(1, "SMOKE: GEOM.CLIP saw %0d of %0d offered triangles -- the front door does not accept",
             geom_clip_submitted_o, geom_tris_sent_q);
    if ((geom_clip_clipped_o != 0) || (geom_clip_culled_o != 0))
      $fatal(1, "SMOKE: GEOM.CLIP clipped %0d and culled %0d of a triangle set that is wholly inside the canvas and front-facing -- the scissor taken from mode_act_o is not the pass rectangle",
             geom_clip_clipped_o, geom_clip_culled_o);
    if (geom_setup_triangles_submitted_o != geom_clip_submitted_o)
      $fatal(1, "SMOKE: GEOM.SETUP received %0d of GEOM.CLIP's %0d accepted triangles -- the clip->setup seam does not carry",
             geom_setup_triangles_submitted_o, geom_clip_submitted_o);

    // AND THE DOOR MUST ACTUALLY HAVE OPENED. This is the check that separates
    // "GEOM.SETUP's counter moved" from "the shell accepted the triangles",
    // and it needs the argument written down because the counter alone does
    // not carry it:
    //
    //   `triangles_submitted_o` counts ACCEPTANCES AT SETUP'S INPUT, gated on
    //   `pipe_en`. A four-deep pipe with a dead output can therefore swallow
    //   about four triangles and count them. N_GEOM_TRIS is 16 -- comfortably
    //   more than the pipe holds -- so the only way all sixteen are accepted
    //   is if `out_ready_i` went high repeatedly, and `out_ready_i` IS the
    //   shell's `render_tri_ready_o`. Backpressure propagates the whole way:
    //   sixteen through CLIP means sixteen through the door.
    //
    // This is why the count is 16 and not 1, and lowering it would quietly
    // turn this from evidence into a pipe-depth measurement.
    if (geom_setup_triangles_submitted_o != geom_tris_sent_q)
      $fatal(1, "SMOKE: GEOM.SETUP took %0d of %0d -- the shell's triangle door is not accepting",
             geom_setup_triangles_submitted_o, geom_tris_sent_q);

    // WHY `render_pixels_o` IS STILL ZERO, MEASURED RATHER THAN ASSUMED, and
    // left as a $display rather than promoted to a check because it is NOT
    // this packet's seam and asserting it would assert someone else's gap.
    //
    // `zhao_shell_top_v2` does not give the bin pipe `render_frame_begin_i`.
    // It gives it `v2_frame_admit_w` -- the ADMITTED frame -- and its own
    // comment is the law: "the renderer's work is withheld until the lease is
    // granted and the clear accepted". This bench drives no renderer lease, so
    // no frame is admitted, the bin pipe never starts one, and every counter
    // downstream reads a clean zero with `fatal`, `stream_error` and
    // `overflow` all low.
    //
    // That set of clean zeros is exactly what a broken triangle door would
    // also produce, which is the whole reason the check above is written
    // against backpressure rather than against pixels.
    if ((render_pixels_o == 0) && (v2_frames_admitted_o == 0))
      $display("SMOKE: NOTE raster pixels=0 because frames_admitted=0 -- the V2 renderer lease is not driven by this bench. The triangle door is proven above by backpressure, not by pixels. Driving the lease belongs with VIDEO.SLOTMGR, which the completion register still lists as not connected.");
    else if (render_pixels_o == 0)
      $fatal(1, "SMOKE: %0d frame(s) were ADMITTED and the shell still rasterised 0 pixels from %0d triangles -- that is a real render-path fault, not the missing lease",
             v2_frames_admitted_o, geom_setup_triangles_submitted_o);

    $display("SMOKE: PASS -- the connected core carries traffic on every wire this bench can reach.");
    $finish;
  end

  // A bench that hangs must say so rather than be killed by a wrapper.
  initial begin
    #40ms;
    $fatal(1, "SMOKE: wall-clock timeout");
  end

endmodule : tb_zhao_console_core_smoke
