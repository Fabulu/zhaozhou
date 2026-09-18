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
  localparam int unsigned N_GEOM_VERTS   = 4;
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
  logic                    geom_skin_v_valid_i;
  logic                    geom_skin_v_ready_o;
  logic signed [31:0]      geom_skin_v_x_i;
  logic signed [31:0]      geom_skin_v_y_i;
  logic signed [31:0]      geom_skin_v_z_i;
  logic [6:0]              geom_skin_v_w0_i;
  logic                    geom_skin_v_rigid_i;
  logic [15:0]             geom_skin_v_src_id_i;
  logic signed [31:0]      geom_skin_a_m_i [0:11];
  logic signed [31:0]      geom_skin_b_m_i [0:11];
  logic [15:0]             geom_skin_src_id_o;
  logic [31:0]             geom_skin_vertices_transformed_o;
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
  logic                    proj_b_valid_i;
  logic                    proj_b_ready_o;
  logic signed [31:0]      proj_b_vx_i;
  logic signed [31:0]      proj_b_vy_i;
  logic signed [31:0]      proj_b_vz_i;
  logic                    proj_b_view_i;
  logic [PROJ_T_ARENA_W-1:0] proj_b_arena_i;
  logic [PROJ_T_INDEX_W-1:0] proj_b_index_i;
  logic                    proj_fill_landed_o;
  logic [PROJ_T_ARENA_W-1:0] proj_fill_arena_o;
  logic                    proj_open_i;
  logic [PROJ_T_ARENA_W-1:0] proj_open_arena_i;
  logic [GEOM_GEN_W-1:0]   proj_open_gen_o;
  logic                    proj_seal_i;
  logic [PROJ_T_ARENA_W-1:0] proj_seal_arena_i;
  logic                    proj_ref_valid_i;
  logic                    proj_ref_ready_o;
  logic [PROJ_T_ARENA_W-1:0] proj_ref_arena_i;
  logic [GEOM_GEN_W-1:0]   proj_ref_gen_i;
  logic [PROJ_T_INDEX_W-1:0] proj_ref_ia_i;
  logic [PROJ_T_INDEX_W-1:0] proj_ref_ib_i;
  logic [PROJ_T_INDEX_W-1:0] proj_ref_ic_i;
  logic [15:0]             proj_ref_src_id_i;
  logic                    proj_ref_view_i;
  logic [7:0]              proj_ref_mat_a_i;
  logic [7:0]              proj_ref_mat_b_i;
  logic [7:0]              proj_ref_weight_i;
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
  logic               render_tri_valid_i;
  logic               render_tri_ready_o;
  zhao_guard_req_t geom_guard_req_i;
  zhao_guard_rsp_t geom_guard_rsp_o;
  logic            geom_beat_valid_o;
  logic [63:0]     geom_beat_data_o;
  logic            geom_beat_last_o;
  logic signed [22:0] render_kx0_i, render_ky0_i;
  logic signed [47:0] render_kc0_i;
  logic signed [22:0] render_kx1_i, render_ky1_i;
  logic signed [47:0] render_kc1_i;
  logic signed [22:0] render_kx2_i, render_ky2_i;
  logic signed [47:0] render_kc2_i;
  logic        [ 2:0] render_tl_i;
  logic signed [20:0] render_ax_i, render_ay_i;
  logic signed [20:0] render_bx_i, render_by_i;
  logic signed [20:0] render_cx_i, render_cy_i;
  logic signed [11:0] render_min_x_i, render_max_x_i;
  logic signed [11:0] render_min_y_i, render_max_y_i;
  logic        [15:0] render_src_id_i;
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

  zhao_console_core dut (.*);

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
  bit          part_tick_seen_q;
  bit          reset_released_q;

  always @(posedge gpu_clk) begin
    cycles_q <= cycles_q + 1;
    if (reset_released_q) begin
      if (gpu_tick_o)                             ticks_seen_q     <= ticks_seen_q + 1;
      if (part_tick_busy_o)                       part_tick_seen_q <= 1'b1;
      if (part_wr_valid_o && part_wr_ready_i)
        part_records_written_q <= part_records_written_q + 1;
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
  // THE GEOMETRY VERTEX STREAM (entry I10: GEOM.POSE/VDECODE are absent, so
  // the harness plays the content source). One job, then its vertices.
  // --------------------------------------------------------------------------
  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      geom_verts_sent_q   <= 0;
      geom_skin_v_valid_i <= 1'b0;
    end else begin
      if (geom_skin_v_valid_i && geom_skin_v_ready_o) begin
        geom_verts_sent_q   <= geom_verts_sent_q + 1;
        geom_skin_v_valid_i <= 1'b0;
      end
      if (reset_released_q && !geom_skin_v_valid_i &&
          (geom_verts_sent_q < N_GEOM_VERTS)) begin
        geom_skin_v_valid_i <= 1'b1;
        geom_skin_v_x_i     <= FX16_ONE * 32'sh2 + 32'(geom_verts_sent_q);
        geom_skin_v_y_i     <= FX16_ONE;
        geom_skin_v_z_i     <= FX16_ONE * 32'sh4;
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
    geom_skin_v_w0_i = '0;
    geom_skin_v_rigid_i = '0;
    geom_skin_v_src_id_i = '0;
    geom_skin_a_m_i = '{default: '0};
    geom_skin_b_m_i = '{default: '0};
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
    proj_b_valid_i = '0;
    proj_b_vx_i = '0;
    proj_b_vy_i = '0;
    proj_b_vz_i = '0;
    proj_b_view_i = '0;
    proj_b_arena_i = '0;
    proj_b_index_i = '0;
    proj_open_i = '0;
    proj_open_arena_i = '0;
    proj_seal_i = '0;
    proj_seal_arena_i = '0;
    proj_ref_valid_i = '0;
    proj_ref_arena_i = '0;
    proj_ref_gen_i = '0;
    proj_ref_ia_i = '0;
    proj_ref_ib_i = '0;
    proj_ref_ic_i = '0;
    proj_ref_src_id_i = '0;
    proj_ref_view_i = '0;
    proj_ref_mat_a_i = '0;
    proj_ref_mat_b_i = '0;
    proj_ref_weight_i = '0;
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
    render_tri_valid_i = '0;
    geom_guard_req_i = '0;
    render_kx0_i = '0;
    render_ky0_i = '0;
    render_kc0_i = '0;
    render_kx1_i = '0;
    render_ky1_i = '0;
    render_kc1_i = '0;
    render_kx2_i = '0;
    render_ky2_i = '0;
    render_kc2_i = '0;
    render_tl_i = '0;
    render_ax_i = '0;
    render_ay_i = '0;
    render_bx_i = '0;
    render_by_i = '0;
    render_cx_i = '0;
    render_cy_i = '0;
    render_min_x_i = '0;
    render_max_x_i = '0;
    render_min_y_i = '0;
    render_max_y_i = '0;
    render_src_id_i = '0;
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

    // Benign, NON-ZERO configuration, chosen so nothing is refused for a
    // reason unrelated to wiring:
    //   species lifetime 0 = UNBOUNDED, so no particle dies of age and the
    //     survive verdict reaches PART.STATE;
    //   collision response 0 = IGNORE, a KNOWN response, so PART.COLLIDE
    //     reports alive rather than refusing;
    //   the projector enabled and a rigid, unit-weight skin, so client A is
    //     offered real vertices.
    part_upd_spc_lifetime_i = '0;
    part_col_d_response_i   = 3'd0;
    part_wr_ready_i         = 1'b1;
    proj_en_i               = 1'b1;
    geom_skin_v_w0_i        = 7'd64;      // 64/64 == rigid
    geom_skin_v_rigid_i     = 1'b1;
    geom_skin_v_src_id_i    = 16'h00A1;
    for (int i = 0; i < 12; i++) begin
      geom_skin_a_m_i[i] = '0;
      geom_skin_b_m_i[i] = '0;
    end
    geom_skin_a_m_i[0]  = FX16_ONE;       // row-major 3x4, the identity
    geom_skin_a_m_i[5]  = FX16_ONE;
    geom_skin_a_m_i[10] = FX16_ONE;
    geom_skin_b_m_i[0]  = FX16_ONE;
    geom_skin_b_m_i[5]  = FX16_ONE;
    geom_skin_b_m_i[10] = FX16_ONE;

    repeat (20) @(posedge gpu_clk);
    rst_n = 1'b1;
    repeat (4) @(posedge gpu_clk);
    reset_released_q = 1'b1;

    // ---- the geometry job -------------------------------------------------
    // One meshlet, one view. The vertices follow from the always block above.
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
    $display("SMOKE: geometry   skinned=%0d vertices_sent=%0d a_grants=%0d landings=%0d groups_opened=%0d groups_sealed=%0d",
             geom_skin_vertices_transformed_o, geom_vertices_sent_o,
             proj_a_grants_o, geom_landings_o,
             geom_groups_opened_o, geom_groups_sealed_o);
    $display("SMOKE: measure    snapshots=%0d", hist_snapshots_o);
    $display("SMOKE: tied-off, MUST be zero: collisions_applied=%0d (entry I4)",
             part_collisions_applied_o);

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

    // A NEGATIVE CONTROL for the header's entry I4, kept as an assertion about
    // the CORRECT state of this core rather than about the defect: the step-6
    // input is tied off, so this counter cannot move. If it ever does, someone
    // has wired step 6 and entry I4 is stale.
    if (part_collisions_applied_o != 0)
      $fatal(1, "SMOKE: collisions_applied moved, but PART.UPDATE's step-6 input is tied off -- header entry I4 is out of date");

    $display("SMOKE: PASS -- the connected core carries traffic on every wire this bench can reach.");
    $finish;
  end

  // A bench that hangs must say so rather than be killed by a wrapper.
  initial begin
    #40ms;
    $fatal(1, "SMOKE: wall-clock timeout");
  end

endmodule : tb_zhao_console_core_smoke
