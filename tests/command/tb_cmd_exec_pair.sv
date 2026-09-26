// tb_cmd_exec_pair.sv -- CMD.DECODER and CMD.EXEC on ONE forked byte stream.
//
// This is a WRAPPER, not a copy: it instantiates the two production modules and
// adds nothing but the fork. `tools/budget/mutant_copy_drift.py` excludes
// wrappers for exactly this reason -- there is no body here to go stale.
//
// WHY THE PAIR AND NOT THE EXECUTOR ALONE. `zhao_cmd_exec`'s verdict inputs
// could be driven from `zref::cmd::validate` in C++, and the test would pass
// while proving nothing about the two things most likely to be wrong:
//
//   1. THE FORK. `pkt_ready_o` is the AND of both consumers' readies. A tap
//      instead of a fork lets one consumer advance past a byte the other never
//      saw, and every counter on both sides still balances -- the byte simply
//      is not in one of the two walks. Driving the executor alone cannot see
//      this, because there is no second walk to disagree with.
//   2. THE VERDICT INSTANT. `decode_done_o` pulses the cycle after the decoder
//      takes the packet's last byte, and the executor takes that same byte in
//      that same cycle because the fork makes them move together. A C++-driven
//      verdict would be whatever the test author chose, which is how a
//      one-cycle staging race gets asserted rather than found.
//
// The fork below is the same expression `zhao_console_core.sv` uses. If that
// one changes, this must change with it -- the ONE place this harness can
// drift from the composition it stands for.
module tb_cmd_exec_pair #(
    parameter int unsigned STAMP_Q = 8,
    // Deliberately SMALL by default so `draw_overflow_o` is reachable with
    // legal stimulus -- five DrawForms in one packet -- rather than being a
    // counter asserted zero with an argument attached. CLAUDE.md: "a detector
    // that has not been shown to FIRE has not been tested".
    parameter int unsigned DRAW_Q  = 4,
    // Small for the same reason: five PublishResources in one packet fire
    // upload_overflow_o with legal stimulus.
    parameter int unsigned UPL_Q   = 4,
    parameter int unsigned UPL_PQ  = 4,
    // Small for the same reason (R36): three eight-entry SetGradeTables in one
    // packet fire grade_overflow_o with legal stimulus.
    parameter int unsigned GRADE_Q = 16
) (
    input  logic clk,
    input  logic rst_n,

    // the sealed packet byte stream, as CMD.DMA presents it
    input  logic        pkt_valid_i,
    output logic        pkt_ready_o,
    input  logic [ 7:0] pkt_byte_i,
    input  logic [31:0] pkt_len_i,

    // SURFACE.STAMP's backpressure, so the drain can be stalled on purpose
    input  logic        stamp_ready_i,
    // The draw dispatch's backpressure, same purpose. In the composition this
    // is the console boundary (entry I41), so it is genuinely an outside
    // opinion and is driven here rather than tied high.
    input  logic        draw_ready_i,
    // The matrix bank's refusal. In the composer this is `!proj_cfg_we_i` --
    // the host cfg port wins the cycle and CMD.EXEC re-presents. Driven here so
    // the re-presentation is exercised rather than assumed.
    input  logic        proj_cfg_ready_i,
    // MEM.UPLOAD's request ready (R17): it takes one request at a time.
    input  logic        upl_ready_i,
    input  logic        post_idle_i,

    // ---- CMD.DECODER's verdict, observable -------------------------------
    output logic        decode_done_o,
    output logic [ 7:0] decode_error_o,
    output logic [31:0] decode_commands_o,

    // ---- CMD.EXEC's writes into the console ------------------------------
    output logic        proj_cfg_we_o,
    output logic        proj_cfg_view_o,
    output logic [ 4:0] proj_cfg_addr_o,
    output logic [31:0] proj_cfg_data_o,

    output logic               stamp_valid_o,
    output logic        [31:0] stamp_patch_o,
    output logic        [ 7:0] stamp_operation_o,
    output logic        [ 7:0] stamp_tag_o,
    output logic        [15:0] stamp_strength_o,
    output logic signed [31:0] stamp_tx_o,
    output logic signed [31:0] stamp_ty_o,
    output logic signed [31:0] stamp_radius_o,
    output logic signed [31:0] stamp_ring_width_o,
    output logic        [15:0] stamp_src_id_o,

    output logic        draw_valid_o,
    output logic [31:0] draw_form_o,
    output logic [31:0] draw_material_set_o,
    output logic [31:0] draw_transform_o,
    output logic [ 7:0] draw_viewport_mask_o,
    output logic [ 7:0] draw_semantic_weight_o,
    // SEALPLAN 2026-09-26, console entry I56: the frame admission plan. Passed
    // out of the wrapper rather than terminated here, so `cmd_exec_directed`
    // can assert on the decode without a second harness.
    output logic        plan_valid_o,
    output logic [ 7:0] plan_view_o,
    output logic [ 7:0] plan_flags_o,
    output logic [15:0] plan_res_gen_o,
    output logic [15:0] plan_view_gen_o,
    output logic [15:0] plan_giant_inst_o,
    output logic [17:0] plan_verts_o,
    output logic [17:0] plan_tris_o,
    output logic [17:0] plan_chunks_o,
    output logic [17:0] plan_refs_o,
    output logic [17:0] plan_giant_refs_o,
    output logic [31:0] plans_forwarded_o,
    output logic [31:0] plans_malformed_o,
    output logic [15:0] draw_flags_o,
    output logic [15:0] draw_src_id_o,
    // R229: DrawPosedForm 0x0305's key, on the SAME `draw_valid_o` beat.
    // Brought out as REAL OUTPUTS rather than left unbound: a bench that does
    // not carry a block's new ports fails to verilate, and no gate in this tree
    // can see a bench that fails to verilate (the trap `tb_cmd_exec_pair`
    // itself fell into on 2026-09-20 with hostdbg's five trace ports).
    output logic        draw_posed_o,
    output logic [15:0] draw_clip_id_o,
    output logic [15:0] draw_frame_no_o,
    output logic [ 7:0] draw_sub_o,
    // W04: DrawWarpedForm 0x0304's per-draw snapshot, on the SAME
    // `draw_valid_o` beat and out of the SAME queue entry. Brought out as REAL
    // OUTPUTS for the reason the pose block above gives -- a bench that does not
    // carry a block's new ports fails to verilate, and no gate in this tree can
    // see a bench that fails to verilate.
    output logic        draw_warp_en_o,
    output logic [31:0] draw_warp_program_o,
    output logic [31:0] draw_warp_time_o,
    output logic [127:0] draw_warp_par_o,
    output logic [127:0] draw_warp_attr_o,
    output logic [31:0] draw_warp_attr_res_o,
    output logic [ 7:0] draw_warp_attr_mode_o,
    output logic signed [31:0] draw_warp_bx_o,
    output logic signed [31:0] draw_warp_by_o,
    output logic signed [31:0] draw_warp_bz_o,

    output logic        upl_valid_o,
    output logic [23:0] upl_index_o,
    output logic [ 7:0] upl_kind_o,
    output logic [63:0] upl_hps_addr_o,
    output logic [31:0] upl_vram_addr_o,
    output logic [31:0] upl_len_o,
    output logic [15:0] upl_epoch_o,
    output logic [ 7:0] upl_dst_slot_o,
    output logic [15:0] upl_new_gen_o,
    output logic [31:0] upl_crc_o,
    // ---- R25: SetEnvironment, toward zhao_light_env -----------------------
    output logic        env_valid_o,
    input  logic        env_ready_i,
    output logic [15:0] env_sun_yaw_o,
    output logic [15:0] env_sun_pitch_o,
    output logic [15:0] env_sun_colour_o,
    output logic [15:0] env_ambient_o,
    output logic [31:0] envs_issued_o,
    // R41: SetPopulation 0x0303, lowered to PART.POP.
    output logic        pop_valid_o,
    input  logic        pop_ready_i,
    output logic [31:0] pop_population_o,
    output logic [31:0] pop_origin_x_o,
    output logic [31:0] pop_origin_y_o,
    output logic [31:0] pop_origin_z_o,
    output logic [31:0] pop_active_count_o,
    output logic [31:0] pop_plane_c_o,
    output logic [15:0] pop_plane_nx_o,
    output logic [15:0] pop_plane_ny_o,
    output logic [15:0] pop_plane_nz_o,
    output logic [15:0] pop_flags_o,
    output logic [31:0] pops_issued_o,

  // TerrainField 0x0200 (entry I34 build item (a)). A port added to a leaf
  // costs its WHOLE instantiation chain plus every bench -- this block gained
  // five trace ports once and this bench was not updated, and the merged tree
  // then failed to verilate on a file the packet never touched. Connected
  // here in the same commit that adds them, for exactly that reason.
  input  logic               tfld_ready_i,
  output logic               tfld_valid_o,
  output logic signed [31:0] tfld_x0_o,
  output logic signed [31:0] tfld_z0_o,
  output logic signed [31:0] tfld_x1_o,
  output logic signed [31:0] tfld_z1_o,
  output logic        [31:0] tfld_handle_o,
  output logic        [15:0] tfld_cmd_o,
  output logic        [31:0] tfld_start_tick_o,
  output logic        [31:0] tfld_duration_o,
  output logic       [255:0] tfld_params_o,
  // FIELDARM 2026-09-22: the set boundary. High on the LAST record of the
  // set the verdict published, so a consumer can SEAL a per-frame list.
  output logic               tfld_last_o,
  output logic        [31:0] tflds_issued_o,
  output logic        [31:0] tfld_overflow_o,
  output logic        [31:0] tfld_src_truncated_o,

  // R18/R33: the token ceiling and each view's request.
  output logic        tok_budget_valid_o,
  output logic [31:0] tok_budget_geom0_o,
  output logic [31:0] tok_budget_geom1_o,
  output logic [31:0] tok_budget_frag0_o,
  output logic [31:0] tok_budget_frag1_o,
  output logic [31:0] tok_budget_shared_o,
  output logic        tok_vreq_valid_o,
  output logic        tok_vreq_view_o,
  output logic [31:0] tok_vreq_geom_o,
  output logic [31:0] tok_vreq_frag_o,
  output logic [31:0] contracts_applied_o,
  // MEASURE.GOVERNOR's two ratified fields and the `view_count` verdict
  // (2026-09-21, packet TERRACOMP). Exposed here because a port this pair does
  // not connect is a PINMISSING that only the next elaboration finds -- which
  // is exactly how this one was found.
  output logic [ 1:0] gov_view_count_o,
  output logic [31:0] gov_px_err0_o,
  output logic [31:0] gov_px_err1_o,
  output logic [31:0] view_count_refused_o,

    output logic              post_look_busy_o,
    output logic [ 7:0]       post_bloom_gain_o,
    output logic              post_grade_valid_o,
    output logic              post_echo_arm_o,
    output logic signed [8:0] post_bias_r_o,
    output logic signed [8:0] post_bias_g_o,
    output logic signed [8:0] post_bias_b_o,
    output logic [15:0]       post_flash_rgb_o,
    output logic [ 7:0]       post_flash_amt_o,
    output logic [15:0]       post_ink_rgb_o,
    output logic              post_pv_we_o,
    output logic [ 1:0]       post_pv_sel_o,
    output logic [ 5:0]       post_pv_addr_o,
    output logic [71:0]       post_pv_data_o,

    // ---- CMD.EXEC's evidence ---------------------------------------------
    output logic [31:0] packets_committed_o,
    output logic [31:0] packets_abandoned_o,
    output logic [31:0] views_written_o,
    output logic [31:0] stamps_issued_o,
    output logic [31:0] stamp_overflow_o,
    output logic [31:0] view_range_refused_o,
    output logic [31:0] viewport_range_refused_o,
    output logic [31:0] stamp_src_truncated_o,
    output logic [31:0] draws_issued_o,
    output logic [31:0] draw_overflow_o,
    output logic [31:0] draw_src_truncated_o,
    output logic [31:0] posed_draws_issued_o,   // R229
    output logic [31:0] pose_clip_refused_o,    // R229
    output logic [31:0] warp_draws_issued_o,    // W04
    output logic [31:0] warp_draw_refused_o,    // W04
    output logic [31:0] uploads_issued_o,
    output logic [31:0] upload_overflow_o,
    output logic [31:0] post_looks_applied_o,
    output logic [31:0] grade_entries_written_o,
    output logic [31:0] post_refused_o,
    output logic [31:0] grade_overflow_o,

    // ---- DEBUG.TRACE arming (R52) ----------------------------------------
    // Brought out rather than left unconnected: the arm is applied DURING the
    // walk, without waiting for the verdict, so "which byte armed it" is only
    // observable from the live strobe. A local unused wire would have silenced
    // the PINMISSING and made the one interesting instant unmeasurable.
    output logic        dbg_trace_arm_we_o,
    output logic [ 6:0] dbg_trace_arm_mask_o,
    output logic        dbg_trace_clear_o,
    output logic [31:0] trace_arms_applied_o,
    output logic [31:0] trace_arm_refused_o,

    // DrawProcedural 0x0302's arm, added 2026-09-21 (FORGECOMP). The bench
    // carries every one of them out rather than tying any off: an executor arm
    // whose fields a bench cannot see is an arm nothing can test, and the
    // record was landing on `unsupported_o` until this arm existed -- so
    // `unsupported_o` staying PUT on a DrawProcedural is itself a check.
    output logic        forge_valid_o,
    output logic [31:0] forge_program_o,
    output logic [31:0] forge_material_o,
    output logic [15:0] forge_material_id_o,
    output logic [ 7:0] forge_kind_o,
    output logic [15:0] forge_frame_tick_o,
    output logic [15:0] forge_src_id_o,
    output logic [31:0] forges_issued_o,
    output logic [31:0] forge_overflow_o,
    output logic [31:0] forge_src_truncated_o,

    // ---- TWOD: SetPlane 0x0306 and DrawSprite 0x0307 ----------------------
    // Added 2026-09-22 with CMD.EXEC's two staging arms (owner completion
    // ruling item 3). THE READIES ARE DRIVEN FROM OUTSIDE because holding one
    // low is the only legal stimulus that fires `twod_dropped_o` -- in the
    // console `zhao_twod_cmd` holds both high permanently, so that counter
    // reads zero there and this is what makes the zero evidence.
    //
    // Every FIELD is exposed, not just the handshake, because the ruling
    // requires that "generated decoding, CMD.EXEC, the reference renderer and
    // production consumers must agree" -- and agreement about a record is a
    // claim about its FIELDS. A bench that checked only the count would pass
    // with every offset off by two.
    output logic                tpl_valid_o,
    input logic                tpl_ready_i,
    output logic [ 7:0]         tpl_slot_o,
    output logic [ 7:0]         tpl_role_o,
    output logic [ 7:0]         tpl_blend_o,
    output logic [ 7:0]         tpl_opacity_o,
    output logic [ 7:0]         tpl_format_o,
    output logic [ 7:0]         tpl_wrap_o,
    output logic [ 7:0]         tpl_view_mask_o,
    output logic [ 7:0]         tpl_palette_o,
    output logic [15:0]         tpl_width_o,
    output logic [15:0]         tpl_height_o,
    output logic [15:0]         tpl_flags_o,
    output logic [15:0]         tpl_base_o,
    output logic [ 7:0]         tpl_lstride_o,
    output logic [ 7:0]         tpl_lheight_o,
    output logic signed [31:0]  tpl_a_o,
    output logic signed [31:0]  tpl_b_o,
    output logic signed [31:0]  tpl_c_o,
    output logic signed [31:0]  tpl_d_o,
    output logic signed [31:0]  tpl_u0_o,
    output logic signed [31:0]  tpl_v0_o,
    output logic signed [31:0]  tpl_line_scroll_o,
    output logic                tsp_valid_o,
    input logic                tsp_ready_i,
    output logic signed [15:0]  tsp_x_o,
    output logic signed [15:0]  tsp_y_o,
    output logic [15:0]         tsp_w_o,
    output logic [15:0]         tsp_h_o,
    output logic [15:0]         tsp_base_o,
    output logic [ 7:0]         tsp_lstride_o,
    output logic [ 7:0]         tsp_lheight_o,
    output logic [ 7:0]         tsp_format_o,
    output logic [ 7:0]         tsp_palette_o,
    output logic [ 7:0]         tsp_blend_o,
    output logic [ 7:0]         tsp_view_mask_o,
    output logic [15:0]         tsp_tint_o,
    output logic [ 7:0]         tsp_order_o,
    output logic [ 7:0]         tsp_flags_o,
    output logic [15:0]         tsp_src_id_o,
    output logic signed [31:0]  tsp_u_o,
    output logic signed [31:0]  tsp_v_o,
    output logic signed [31:0]  tsp_a00_o,
    output logic signed [31:0]  tsp_a01_o,
    output logic signed [31:0]  tsp_a10_o,
    output logic signed [31:0]  tsp_a11_o,
    output logic                twod_pkt_commit_o,
    output logic                twod_pkt_abandon_o,
    output logic                tld_valid_o,
    input logic                tld_ready_i,
    output logic [23:0]         tld_index_o,
    output logic [63:0]         tld_hps_addr_o,
    output logic [31:0]         tld_len_o,
    output logic [31:0]         tld_crc_o,
    output logic [15:0]         tld_epoch_o,
    output logic [ 7:0]         tld_dst_slot_o,
    output logic [31:0]         twod_planes_staged_o,
    output logic [31:0]         twod_sprites_staged_o,
    output logic [31:0]         twod_dropped_o,
    output logic [31:0]         twod_loads_issued_o,
    output logic [31:0] unsupported_o
);

  logic dec_ready, exe_ready;

  // THE FORK. Both consumers must be able to take the byte, or nobody does.
  assign pkt_ready_o = dec_ready && exe_ready;

  // The decoder's record-header port is retired unconditionally here for the
  // reason section 7b of the core gives: retiring is not acting, and holding
  // `rec_ready` low would stall the shared stream and deadlock the executor
  // behind it.
  logic        rec_valid_unused;
  logic [15:0] rec_opcode_unused, rec_bytes_unused;
  logic [31:0] rec_src_unused, rec_index_unused, bytes_consumed_unused;

  zhao_cmd_decoder u_dec (
      .clk  (clk),
      .rst_n(rst_n),

      .pkt_valid_i(pkt_valid_i),
      .pkt_ready_o(dec_ready),
      .pkt_byte_i (pkt_byte_i),
      .pkt_len_i  (pkt_len_i),

      .rec_valid_o    (rec_valid_unused),
      .rec_ready_i    (1'b1),
      .rec_opcode_o   (rec_opcode_unused),
      .rec_bytes_o    (rec_bytes_unused),
      .rec_source_id_o(rec_src_unused),
      .rec_index_o    (rec_index_unused),

      .decode_done_o   (decode_done_o),
      .decode_error_o  (decode_error_o),
      .bytes_consumed_o(bytes_consumed_unused),
      .commands_o      (decode_commands_o)
  );

  zhao_cmd_exec #(
      .STAMP_Q(STAMP_Q),
      .DRAW_Q (DRAW_Q),
      .UPL_Q  (UPL_Q),
      .UPL_PQ (UPL_PQ),
      .GRADE_Q(GRADE_Q)
  ) u_exec (
      .clk  (clk),
      .rst_n(rst_n),

      .pkt_valid_i     (pkt_valid_i),
      .pkt_ready_o     (exe_ready),
      .pkt_fork_ready_i(pkt_ready_o),  // the AND, handed back
      .pkt_byte_i      (pkt_byte_i),
      .pkt_len_i       (pkt_len_i),

      .verdict_valid_i(decode_done_o),
      .verdict_error_i(decode_error_o),

      .proj_cfg_we_o   (proj_cfg_we_o),
      .proj_cfg_ready_i(proj_cfg_ready_i),
      .proj_cfg_view_o(proj_cfg_view_o),
      .proj_cfg_addr_o(proj_cfg_addr_o),
      .proj_cfg_data_o(proj_cfg_data_o),

      .stamp_valid_o     (stamp_valid_o),
      .stamp_ready_i     (stamp_ready_i),
      .stamp_patch_o     (stamp_patch_o),
      .stamp_operation_o (stamp_operation_o),
      .stamp_tag_o       (stamp_tag_o),
      .stamp_strength_o  (stamp_strength_o),
      .stamp_tx_o        (stamp_tx_o),
      .stamp_ty_o        (stamp_ty_o),
      .stamp_radius_o    (stamp_radius_o),
      .stamp_ring_width_o(stamp_ring_width_o),
      .stamp_src_id_o    (stamp_src_id_o),

      .draw_valid_o          (draw_valid_o),
      .draw_ready_i          (draw_ready_i),
      .draw_form_o           (draw_form_o),
      .draw_material_set_o   (draw_material_set_o),
      .draw_transform_o      (draw_transform_o),
      .draw_viewport_mask_o  (draw_viewport_mask_o),
      .draw_semantic_weight_o(draw_semantic_weight_o),
      .plan_valid_o          (plan_valid_o),
      .plan_view_o           (plan_view_o),
      .plan_flags_o          (plan_flags_o),
      .plan_res_gen_o        (plan_res_gen_o),
      .plan_view_gen_o       (plan_view_gen_o),
      .plan_giant_inst_o     (plan_giant_inst_o),
      .plan_verts_o          (plan_verts_o),
      .plan_tris_o           (plan_tris_o),
      .plan_chunks_o         (plan_chunks_o),
      .plan_refs_o           (plan_refs_o),
      .plan_giant_refs_o     (plan_giant_refs_o),
      .plans_forwarded_o     (plans_forwarded_o),
      .plans_malformed_o     (plans_malformed_o),
      .draw_flags_o          (draw_flags_o),
      .draw_src_id_o         (draw_src_id_o),
      .draw_posed_o          (draw_posed_o),
      .draw_clip_id_o        (draw_clip_id_o),
      .draw_frame_no_o       (draw_frame_no_o),
      .draw_sub_o            (draw_sub_o),
      .draw_warp_en_o        (draw_warp_en_o),
      .draw_warp_program_o   (draw_warp_program_o),
      .draw_warp_time_o      (draw_warp_time_o),
      .draw_warp_par_o       (draw_warp_par_o),
      .draw_warp_attr_o      (draw_warp_attr_o),
      .draw_warp_attr_res_o  (draw_warp_attr_res_o),
      .draw_warp_attr_mode_o (draw_warp_attr_mode_o),
      .draw_warp_bx_o        (draw_warp_bx_o),
      .draw_warp_by_o        (draw_warp_by_o),
      .draw_warp_bz_o        (draw_warp_bz_o),

      .upl_valid_o    (upl_valid_o),
      .upl_ready_i    (upl_ready_i),
      .upl_index_o    (upl_index_o),
      .upl_kind_o     (upl_kind_o),
      .upl_hps_addr_o (upl_hps_addr_o),
      .upl_vram_addr_o(upl_vram_addr_o),
      .upl_len_o      (upl_len_o),
      .upl_epoch_o    (upl_epoch_o),
      .upl_dst_slot_o (upl_dst_slot_o),
      .upl_new_gen_o  (upl_new_gen_o),
      .upl_crc_o      (upl_crc_o),
      .env_valid_o     (env_valid_o),
      .env_ready_i     (env_ready_i),
      .env_sun_yaw_o   (env_sun_yaw_o),
      .env_sun_pitch_o (env_sun_pitch_o),
      .env_sun_colour_o(env_sun_colour_o),
      .env_ambient_o   (env_ambient_o),
      .envs_issued_o   (envs_issued_o),
      .pop_valid_o       (pop_valid_o),
      .pop_ready_i       (pop_ready_i),
      .pop_population_o  (pop_population_o),
      .pop_origin_x_o    (pop_origin_x_o),
      .pop_origin_y_o    (pop_origin_y_o),
      .pop_origin_z_o    (pop_origin_z_o),
      .pop_active_count_o(pop_active_count_o),
      .pop_plane_c_o     (pop_plane_c_o),
      .pop_plane_nx_o    (pop_plane_nx_o),
      .pop_plane_ny_o    (pop_plane_ny_o),
      .pop_plane_nz_o    (pop_plane_nz_o),
      .pop_flags_o       (pop_flags_o),
      .pops_issued_o     (pops_issued_o),

      .tfld_valid_o        (tfld_valid_o),
      .tfld_ready_i        (tfld_ready_i),
      .tfld_x0_o           (tfld_x0_o),
      .tfld_z0_o           (tfld_z0_o),
      .tfld_x1_o           (tfld_x1_o),
      .tfld_z1_o           (tfld_z1_o),
      .tfld_handle_o       (tfld_handle_o),
      .tfld_last_o         (tfld_last_o),
      .tfld_cmd_o          (tfld_cmd_o),
      .tfld_start_tick_o   (tfld_start_tick_o),
      .tfld_duration_o     (tfld_duration_o),
      .tfld_params_o       (tfld_params_o),
      .tflds_issued_o      (tflds_issued_o),
      .tfld_overflow_o     (tfld_overflow_o),
      .tfld_src_truncated_o(tfld_src_truncated_o),

    .tok_budget_valid_o (tok_budget_valid_o),
    .tok_budget_geom0_o (tok_budget_geom0_o),
    .tok_budget_geom1_o (tok_budget_geom1_o),
    .tok_budget_frag0_o (tok_budget_frag0_o),
    .tok_budget_frag1_o (tok_budget_frag1_o),
    .tok_budget_shared_o(tok_budget_shared_o),
    .tok_vreq_valid_o   (tok_vreq_valid_o),
    .tok_vreq_view_o    (tok_vreq_view_o),
    .tok_vreq_geom_o    (tok_vreq_geom_o),
    .tok_vreq_frag_o    (tok_vreq_frag_o),
    .contracts_applied_o(contracts_applied_o),
    .gov_view_count_o    (gov_view_count_o),
    .gov_px_err0_o       (gov_px_err0_o),
    .gov_px_err1_o       (gov_px_err1_o),
    .view_count_refused_o(view_count_refused_o),

      .post_idle_i       (post_idle_i),
      .post_look_busy_o  (post_look_busy_o),
      .post_bloom_gain_o (post_bloom_gain_o),
      .post_grade_valid_o(post_grade_valid_o),
      .post_echo_arm_o   (post_echo_arm_o),
      .post_bias_r_o     (post_bias_r_o),
      .post_bias_g_o     (post_bias_g_o),
      .post_bias_b_o     (post_bias_b_o),
      .post_flash_rgb_o  (post_flash_rgb_o),
      .post_flash_amt_o  (post_flash_amt_o),
      .post_ink_rgb_o    (post_ink_rgb_o),
      .post_pv_we_o      (post_pv_we_o),
      .post_pv_sel_o     (post_pv_sel_o),
      .post_pv_addr_o    (post_pv_addr_o),
      .post_pv_data_o    (post_pv_data_o),

      .packets_committed_o  (packets_committed_o),
      .packets_abandoned_o  (packets_abandoned_o),
      .views_written_o      (views_written_o),
      .stamps_issued_o      (stamps_issued_o),
      .stamp_overflow_o     (stamp_overflow_o),
      .view_range_refused_o (view_range_refused_o),
      .viewport_range_refused_o(viewport_range_refused_o),
      .stamp_src_truncated_o(stamp_src_truncated_o),
      .draws_issued_o       (draws_issued_o),
      .draw_overflow_o      (draw_overflow_o),
      .draw_src_truncated_o (draw_src_truncated_o),
      .posed_draws_issued_o (posed_draws_issued_o),
      .pose_clip_refused_o  (pose_clip_refused_o),
      .warp_draws_issued_o  (warp_draws_issued_o),
      .warp_draw_refused_o  (warp_draw_refused_o),
      .uploads_issued_o     (uploads_issued_o),
      .upload_overflow_o    (upload_overflow_o),
      .post_looks_applied_o (post_looks_applied_o),
      .grade_entries_written_o(grade_entries_written_o),
      .post_refused_o       (post_refused_o),
      .grade_overflow_o     (grade_overflow_o),

      .dbg_trace_arm_we_o   (dbg_trace_arm_we_o),
      .dbg_trace_arm_mask_o (dbg_trace_arm_mask_o),
      .dbg_trace_clear_o    (dbg_trace_clear_o),
      .trace_arms_applied_o (trace_arms_applied_o),
      .trace_arm_refused_o  (trace_arm_refused_o),

      .forge_valid_o     (forge_valid_o),
      // ALWAYS READY, so the queue's head advances and `forges_issued_o`
      // counts on the way OUT, which is where the arm counts it.
      .forge_ready_i     (1'b1),
      .forge_program_o   (forge_program_o),
      .forge_material_o  (forge_material_o),
      .forge_material_id_o(forge_material_id_o),
      .forge_kind_o      (forge_kind_o),
      .forge_frame_tick_o(forge_frame_tick_o),
      .forge_src_id_o    (forge_src_id_o),
      .forges_issued_o       (forges_issued_o),
      .forge_overflow_o      (forge_overflow_o),
      .forge_src_truncated_o (forge_src_truncated_o),

      .tpl_valid_o            (tpl_valid_o),
      .tpl_ready_i            (tpl_ready_i),
      .tpl_slot_o             (tpl_slot_o),
      .tpl_role_o             (tpl_role_o),
      .tpl_blend_o            (tpl_blend_o),
      .tpl_opacity_o          (tpl_opacity_o),
      .tpl_format_o           (tpl_format_o),
      .tpl_wrap_o             (tpl_wrap_o),
      .tpl_view_mask_o        (tpl_view_mask_o),
      .tpl_palette_o          (tpl_palette_o),
      .tpl_width_o            (tpl_width_o),
      .tpl_height_o           (tpl_height_o),
      .tpl_flags_o            (tpl_flags_o),
      .tpl_base_o             (tpl_base_o),
      .tpl_lstride_o          (tpl_lstride_o),
      .tpl_lheight_o          (tpl_lheight_o),
      .tpl_a_o                (tpl_a_o),
      .tpl_b_o                (tpl_b_o),
      .tpl_c_o                (tpl_c_o),
      .tpl_d_o                (tpl_d_o),
      .tpl_u0_o               (tpl_u0_o),
      .tpl_v0_o               (tpl_v0_o),
      .tpl_line_scroll_o      (tpl_line_scroll_o),
      .tsp_valid_o            (tsp_valid_o),
      .tsp_ready_i            (tsp_ready_i),
      .tsp_x_o                (tsp_x_o),
      .tsp_y_o                (tsp_y_o),
      .tsp_w_o                (tsp_w_o),
      .tsp_h_o                (tsp_h_o),
      .tsp_base_o             (tsp_base_o),
      .tsp_lstride_o          (tsp_lstride_o),
      .tsp_lheight_o          (tsp_lheight_o),
      .tsp_format_o           (tsp_format_o),
      .tsp_palette_o          (tsp_palette_o),
      .tsp_blend_o            (tsp_blend_o),
      .tsp_view_mask_o        (tsp_view_mask_o),
      .tsp_tint_o             (tsp_tint_o),
      .tsp_order_o            (tsp_order_o),
      .tsp_flags_o            (tsp_flags_o),
      .tsp_src_id_o           (tsp_src_id_o),
      .tsp_u_o                (tsp_u_o),
      .tsp_v_o                (tsp_v_o),
      .tsp_a00_o              (tsp_a00_o),
      .tsp_a01_o              (tsp_a01_o),
      .tsp_a10_o              (tsp_a10_o),
      .tsp_a11_o              (tsp_a11_o),
      .twod_pkt_commit_o      (twod_pkt_commit_o),
      .twod_pkt_abandon_o     (twod_pkt_abandon_o),
      .tld_valid_o            (tld_valid_o),
      .tld_ready_i            (tld_ready_i),
      .tld_index_o            (tld_index_o),
      .tld_hps_addr_o         (tld_hps_addr_o),
      .tld_len_o              (tld_len_o),
      .tld_crc_o              (tld_crc_o),
      .tld_epoch_o            (tld_epoch_o),
      .tld_dst_slot_o         (tld_dst_slot_o),
      .twod_planes_staged_o   (twod_planes_staged_o),
      .twod_sprites_staged_o  (twod_sprites_staged_o),
      .twod_dropped_o         (twod_dropped_o),
      .twod_loads_issued_o    (twod_loads_issued_o),
      .unsupported_o        (unsupported_o)
  );

endmodule : tb_cmd_exec_pair
