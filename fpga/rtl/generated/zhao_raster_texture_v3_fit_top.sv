// GENERATED FILE -- DO NOT EDIT.
// Generator: tools/quartus/gen_raster_texture_v3_fit_top.py
// generator-sha256: 2d2ae17769fb6931c3812d1c20bd02f610b7bad968f54f197c9a5968d2ba0291
// template-sha256: ae5eb48b27498c565904d2d7f22e4e36efdeae9c9b4dd891ed22015c56a84054
// manifest: fpga/rtl/generated/zhao_raster_texture_v3_fit_top.manifest.json
// Product witness: u_tile.u_texture_stage explicitly sets MIGRATION_SHADOWS=1'b0.
// ATTR_DSP3/BILERP_DSP2 are explicit top parameters; the G8A flow must set both to 1.
// Characterization traffic is legal and deterministic; this is not a shell or board top.

module zhao_raster_texture_v3_fit_top #(
    parameter bit ATTR_DSP3 = 1'b0,
    parameter bit BILERP_DSP2 = 1'b0
) (
    input  logic       clk,
    input  logic       rst_n,
    (* useioff = 1 *) output logic [7:0] fit_signature_o,
    (* useioff = 1 *) output logic [7:0] fit_epoch_o
);

  localparam logic [3:0] S_PAL_BEGIN = 4'd0;
  localparam logic [3:0] S_PAL_WRITE = 4'd1;
  localparam logic [3:0] S_PAL_END   = 4'd2;
  localparam logic [3:0] S_CFG_BEGIN = 4'd3;
  localparam logic [3:0] S_CFG_BWAIT = 4'd4;
  localparam logic [3:0] S_CFG_ROW   = 4'd5;
  localparam logic [3:0] S_CFG_RWAIT = 4'd6;
  localparam logic [3:0] S_CFG_END   = 4'd7;
  localparam logic [3:0] S_CFG_EWAIT = 4'd8;
  localparam logic [3:0] S_RUN       = 4'd9;

  (* keep = "true" *) logic [63:0] stimulus_lfsr_q;
  (* keep = "true" *) logic [31:0] signature_misr_q;
  (* keep = "true" *) logic [5:0] signature_source_q;
  logic [31:0] signature_word_c;
  logic [31:0] signature_word_q;
  logic [3:0] setup_state_q;
  logic [7:0] palette_index_q;
  logic setup_fault_q;
  logic [3:0] setup_fault_cause_q;

  logic job_pending_q;
  logic job_valid_w, job_ready_w;
  logic [1876:0] job_meta_w;
// In production the binner decides these when it writes the job and carries
// them in the metadata bank's pad. This wrapper builds job_meta_w itself, so
// it derives them from the same word by the same rule rather than asserting a
// constant -- and zhao_raster_tile_pipe_v2's simulation-only equivalence
// assert checks that it derived them correctly.
wire [1:0] job_profile_bad_w = {(job_meta_w[424:378] == 47'd0),
                                job_meta_w[268] || (job_meta_w[267:44] != 224'd0)};
  logic frame_fault_clear_pending_q;
  logic frame_fault_clear_valid_w, frame_fault_clear_ready_w, frame_fault_w;
  logic lifetime_structural_fault_w;
  logic setup_blocking_fault_w;

  wire frame_fault_clear_fire_w =
      frame_fault_clear_valid_w && frame_fault_clear_ready_w;

  logic cfg_valid_w, cfg_ready_w;
  logic [1:0] cfg_op_w;
  logic [7:0] cfg_generation_w, cfg_selector_w;
  logic [74:0] cfg_row_w;
  logic [31:0] cfg_crc_w;
  logic cfg_rsp_valid_w;
  logic [1:0] cfg_rsp_op_w;
  logic [3:0] cfg_rsp_status_w;
  logic [7:0] cfg_rsp_generation_w, active_page_generation_w;

  logic fill_req_valid_w, fill_req_ready_w;
  logic [31:0] fill_req_addr_w;
  logic fill_busy_q;
  logic [2:0] fill_beat_q;
  logic fill_data_valid_w;
  logic [15:0] fill_data_w;

  logic pal_valid_w, pal_ready_w;
  logic [1:0] pal_op_w;

  logic sheet_req_valid_w, sheet_req_ready_w;
  logic [1:0] sheet_req_op_w;
  logic [31:0] sheet_req_handle_w;
  logic [11:0] sheet_req_texel_w;
  logic [15:0] sheet_req_src_w;
  logic pg_ready_w;

  logic fb_valid_w, fb_ready_w;
  logic [15:0] fb_rgb565_w;
  logic [7:0] fb_tag_w, fb_addr_w;
  logic signed [11:0] fb_x_w, fb_y_w;
  logic fb_last_w;
  logic [15:0] fb_src_w;

  logic [31:0] tile_crc_w;
  logic [15:0] tile_crc_index_w;
  logic tile_done_w;
  logic [8:0] tile_cov_count_w;
  logic tile_degenerate_w, front_bank_w;
  logic [31:0] tilestore_references_w, resolved_tiles_w;
  logic [31:0] early_z_rejects_w, early_z_covered_w;
  logic [31:0] fragment_covered_w, blended_fragments_w;
  logic [7:0] bin_mask_w;
  logic [23:0] z_floor_w;
  logic fragment_error_w;

  logic quiet_w, raster_abort_w, local_attribute_abort_w, local_fault_pulse_w;
  logic [31:0] local_fault_count_w, coordinate_fault_count_w;
  logic [31:0] range_fault_count_w, aux_profile_fault_count_w;
  logic [31:0] candidate_cancel_count_w, local_drop_count_w;
  logic [31:0] jobs_started_w, jobs_sunk_w;

  logic sequence_abort_w, sequence_mismatch_w;
  logic [31:0] sequence_drop_count_w, admission_sequence_w;
  logic [31:0] expected_sequence_w, returned_sequence_w;
  logic packet_c_cand_fire_w, packet_c_fragment_fire_w, packet_c_drop_fire_w;

  logic [31:0] texture_fragments_w, texture_cache_hits_w;
  logic [31:0] texture_cache_misses_w, texture_palette_lookups_w;
  logic [31:0] texture_plan_accepted_w, texture_dispatch_accepted_w;
  logic [31:0] texture_combine_refused_w;
// Entry I49: the TMU samples the island PUBLISHED into a fragment, promoted out
// of the tile pipe 2026-09-20. This top is the tree's one end-to-end sampling
// constructor, so it is also the natural place to read the new counter.
logic [31:0] texture_samples_w;

  logic coverage_hold_valid_w;
  logic [5:0] coverage_delivered_mask_w;
  logic [7:0] start_delivered_mask_w;
  logic [5:0] attribute_idle_w;
  logic earlyz_hold_valid_w;
  logic [1:0] skid_level_w;
  logic stage_candidate_valid_w;
  logic [490:0] stage_candidate_data_w;
  logic stage_fragment_valid_w;
  logic [7:0] stage_fragment_addr_w;
  logic [23:0] stage_fragment_depth_w;
  logic [31:0] stage_fragment_state_w;
  logic [15:0] stage_fragment_src_w;
  logic [23:0] stage_fragment_rgb_w;
  logic [7:0] stage_fragment_alpha_w, stage_fragment_index_w;
  logic [7:0] stage_fragment_status_w;
  logic texture_quiet_w, fragment_idle_w;

  logic [31:0] jobs_accepted_q, fills_accepted_q, fill_data_beats_q;
  logic [31:0] fb_words_q, tiles_done_q, cfg_commands_q, palette_commands_q;
  logic saw_green_q, saw_index5_q, saw_candidate_q, saw_fragment_q;
  logic saw_fill_q, saw_fb_stall_q;
  logic [15:0] first_nonzero_fb_q;

  wire job_fire_w = job_valid_w && job_ready_w;
  wire cfg_fire_w = cfg_valid_w && cfg_ready_w;
  wire pal_fire_w = pal_valid_w && pal_ready_w;
  wire fill_fire_w = fill_req_valid_w && fill_req_ready_w;
  wire fb_fire_w = fb_valid_w && fb_ready_w;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fit_signature_o <= 8'h01;
      fit_epoch_o <= 8'h00;
      stimulus_lfsr_q <= 64'hd1b5_4a32_d192_ed03;
      signature_misr_q <= 32'h0000_0001;
      signature_source_q <= 6'd0;
      signature_word_q <= 32'd0;
    end else begin
      fit_signature_o <= signature_misr_q[7:0] ^ signature_misr_q[15:8]
                       ^ signature_misr_q[23:16] ^ signature_misr_q[31:24];
      fit_epoch_o <= {2'b00, signature_source_q};
      stimulus_lfsr_q <= {stimulus_lfsr_q[62:0],
          stimulus_lfsr_q[63] ^ stimulus_lfsr_q[62] ^
          stimulus_lfsr_q[60] ^ stimulus_lfsr_q[59]};
      signature_word_q <= signature_word_c;
      signature_misr_q <= {signature_misr_q[30:0], 1'b0}
                        ^ (signature_misr_q[31] ? 32'h0040_0007 : 32'd0)
                        ^ signature_word_q;
      signature_source_q <= signature_source_q + 6'd1;
    end
  end

  always_comb begin
    unique case (signature_source_q)
      6'd0:  signature_word_c = tile_crc_w;
      6'd1:  signature_word_c = tilestore_references_w;
      6'd2:  signature_word_c = resolved_tiles_w;
      6'd3:  signature_word_c = early_z_rejects_w;
      6'd4:  signature_word_c = early_z_covered_w;
      6'd5:  signature_word_c = fragment_covered_w;
      6'd6:  signature_word_c = blended_fragments_w;
      6'd7:  signature_word_c = local_fault_count_w;
      6'd8:  signature_word_c = coordinate_fault_count_w;
      6'd9:  signature_word_c = range_fault_count_w;
      6'd10: signature_word_c = aux_profile_fault_count_w;
      6'd11: signature_word_c = candidate_cancel_count_w;
      6'd12: signature_word_c = local_drop_count_w;
      6'd13: signature_word_c = jobs_started_w;
      6'd14: signature_word_c = jobs_sunk_w;
      6'd15: signature_word_c = sequence_drop_count_w;
      6'd16: signature_word_c = admission_sequence_w;
      6'd17: signature_word_c = expected_sequence_w;
      6'd18: signature_word_c = returned_sequence_w;
      6'd19: signature_word_c = texture_fragments_w;
      6'd20: signature_word_c = texture_cache_hits_w;
      6'd21: signature_word_c = texture_cache_misses_w;
      6'd22: signature_word_c = texture_palette_lookups_w;
      6'd23: signature_word_c = texture_plan_accepted_w;
      6'd24: signature_word_c = texture_dispatch_accepted_w;
      6'd25: signature_word_c = texture_combine_refused_w;
      6'd26: signature_word_c = stage_candidate_data_w[31:0];
      6'd27: signature_word_c = stage_candidate_data_w[159:128];
      6'd28: signature_word_c = stage_candidate_data_w[287:256];
      6'd29: signature_word_c = stage_candidate_data_w[415:384];
      6'd30: signature_word_c = {21'd0, stage_candidate_data_w[490:480]};
      6'd31: signature_word_c = stage_fragment_state_w;
      6'd32: signature_word_c = {stage_fragment_depth_w, stage_fragment_addr_w};
      6'd33: signature_word_c = {stage_fragment_src_w, stage_fragment_status_w,
                                         stage_fragment_index_w};
      6'd34: signature_word_c = {stage_fragment_rgb_w, stage_fragment_alpha_w};
      6'd35: signature_word_c = fill_req_addr_w;
      6'd36: signature_word_c = {fb_rgb565_w, fb_src_w};
      6'd37: signature_word_c = {fb_x_w, fb_y_w, fb_addr_w};
      6'd38: signature_word_c = jobs_accepted_q;
      6'd39: signature_word_c = fills_accepted_q;
      6'd40: signature_word_c = fill_data_beats_q;
      6'd41: signature_word_c = fb_words_q;
      6'd42: signature_word_c = tiles_done_q;
      6'd43: signature_word_c = cfg_commands_q;
      6'd44: signature_word_c = palette_commands_q;
      6'd45: signature_word_c = {active_page_generation_w, tile_crc_index_w,
                                         tile_cov_count_w[7:0]};
      // THE THREE PER-LANE MASKS MOVED TO WORD 54 on 2026-09-21. They were
      // 3 + 5 + 3 = 11 bits here and owner decision R234 D1's six attribute
      // lanes make them 6 + 8 + 6 = 20, which no longer fits beside the
      // twenty-one structural flags. Splitting them out keeps BOTH observable
      // at full width -- truncating the concatenation instead would have
      // silently stopped witnessing the lanes the decision added, which is the
      // flattering direction.
      6'd46: signature_word_c = {11'd0,
          setup_fault_q, frame_fault_w, fragment_error_w, raster_abort_w,
          local_attribute_abort_w, sequence_abort_w, sequence_mismatch_w,
          quiet_w, texture_quiet_w, fragment_idle_w, coverage_hold_valid_w,
          earlyz_hold_valid_w, skid_level_w, stage_candidate_valid_w,
          stage_fragment_valid_w, tile_done_w, tile_degenerate_w, front_bank_w,
          local_fault_pulse_w,
          packet_c_cand_fire_w ^ packet_c_fragment_fire_w ^ packet_c_drop_fire_w};
      6'd47: signature_word_c = {2'd0,
          cfg_ready_w, cfg_rsp_valid_w, cfg_rsp_op_w, cfg_rsp_status_w,
          cfg_rsp_generation_w, pal_ready_w, fill_req_valid_w, fill_busy_q,
          sheet_req_valid_w, sheet_req_op_w, pg_ready_w, fb_valid_w, fb_ready_w,
          fb_last_w, bin_mask_w[3:0]};
      6'd48: signature_word_c = {8'd0, z_floor_w};
      6'd49: signature_word_c = {24'd0, fb_tag_w};
      6'd50: signature_word_c = {20'd0, sheet_req_texel_w};
      6'd51: signature_word_c = sheet_req_handle_w;
      6'd52: signature_word_c = {16'd0, sheet_req_src_w};
      6'd53: signature_word_c = {23'd0, tile_cov_count_w};
      // All three per-lane masks, full width: 12 + 6 + 6 + 8 = 32.
      6'd54: signature_word_c = {12'd0, attribute_idle_w,
                                 coverage_delivered_mask_w,
                                 start_delivered_mask_w};
      6'd55: signature_word_c = {24'd0, bin_mask_w};
      6'd56: signature_word_c = job_meta_w[31:0];
      6'd57: signature_word_c = job_meta_w[329:298];
      6'd58: signature_word_c = job_meta_w[676:645];
      6'd59: signature_word_c = job_meta_w[1876:1845];
      6'd60: signature_word_c = {16'd0, stimulus_lfsr_q[15:0]};
      6'd61: signature_word_c = stimulus_lfsr_q[47:16];
      6'd62: signature_word_c = {jobs_accepted_q[15:0], tiles_done_q[15:0]};
      // Entry I49, 2026-09-20: the samples the island PUBLISHED into a fragment.
      6'd63: signature_word_c = texture_samples_w;
      default: signature_word_c = stimulus_lfsr_q[63:32];
    endcase
  end

  always_comb begin
    pal_valid_w = 1'b0;
    pal_op_w = 2'd0;
    unique case (setup_state_q)
      S_PAL_BEGIN: begin pal_valid_w = 1'b1; pal_op_w = 2'd0; end
      S_PAL_WRITE: begin pal_valid_w = 1'b1; pal_op_w = 2'd1; end
      S_PAL_END:   begin pal_valid_w = 1'b1; pal_op_w = 2'd2; end
      default: begin end
    endcase

    cfg_valid_w = 1'b0;
    cfg_op_w = 2'd0;
    cfg_generation_w = 8'd1;
    cfg_selector_w = 8'd0;
    cfg_row_w = 75'd0;
    cfg_crc_w = 32'd0;
    unique case (setup_state_q)
      S_CFG_BEGIN: begin
        cfg_valid_w = 1'b1;
        cfg_op_w = 2'd0;
      end
      S_CFG_ROW: begin
        cfg_valid_w = 1'b1;
        cfg_op_w = 2'd1;
        cfg_selector_w = 8'd1;
        cfg_row_w = {11'h404, 32'd0, 32'h0000_2000};
      end
      S_CFG_END: begin
        cfg_valid_w = 1'b1;
        cfg_op_w = 2'd2;
        cfg_crc_w = 32'hc60b_5076;
      end
      default: begin end
    endcase
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      setup_state_q <= S_PAL_BEGIN;
      palette_index_q <= 8'd0;
      setup_fault_q <= 1'b0;
      setup_fault_cause_q <= 4'd0;
    end else begin
      if (frame_fault_w) begin
        setup_fault_q <= 1'b1;
        setup_fault_cause_q[1] <= 1'b1;
      end
      if (fragment_error_w) begin
        setup_fault_q <= 1'b1;
        setup_fault_cause_q[2] <= 1'b1;
      end
      if (raster_abort_w) begin
        setup_fault_q <= 1'b1;
        setup_fault_cause_q[3] <= 1'b1;
      end
      unique case (setup_state_q)
        S_PAL_BEGIN: if (pal_fire_w) begin
          palette_index_q <= 8'd0;
          setup_state_q <= S_PAL_WRITE;
        end
        S_PAL_WRITE: if (pal_fire_w) begin
          if (palette_index_q == 8'hff)
            setup_state_q <= S_PAL_END;
          else
            palette_index_q <= palette_index_q + 8'd1;
        end
        S_PAL_END:   if (pal_fire_w) setup_state_q <= S_CFG_BEGIN;
        S_CFG_BEGIN: if (cfg_fire_w) setup_state_q <= S_CFG_BWAIT;
        S_CFG_BWAIT: if (cfg_rsp_valid_w) begin
          if (cfg_rsp_status_w != 4'd0) begin
            setup_fault_q <= 1'b1;
            setup_fault_cause_q[0] <= 1'b1;
          end
          setup_state_q <= S_CFG_ROW;
        end
        S_CFG_ROW: if (cfg_fire_w) setup_state_q <= S_CFG_RWAIT;
        S_CFG_RWAIT: if (cfg_rsp_valid_w) begin
          if (cfg_rsp_status_w != 4'd0) begin
            setup_fault_q <= 1'b1;
            setup_fault_cause_q[0] <= 1'b1;
          end
          setup_state_q <= S_CFG_END;
        end
        S_CFG_END: if (cfg_fire_w) setup_state_q <= S_CFG_EWAIT;
        S_CFG_EWAIT: if (cfg_rsp_valid_w) begin
          if (cfg_rsp_status_w != 4'd0) begin
            setup_fault_q <= 1'b1;
            setup_fault_cause_q[0] <= 1'b1;
          end
          setup_state_q <= S_RUN;
        end
        default: setup_state_q <= S_RUN;
      endcase
    end
  end

  always_comb begin
    job_meta_w = 1877'd0;
    job_meta_w[7:0] = 8'd1;
    job_meta_w[19:12] = 8'hff;
    job_meta_w[276:269] = 8'h5a;
    job_meta_w[295:288] = 8'd1;
    job_meta_w[297:296] = 2'd1;
    job_meta_w[345:298] = 48'hffffff_ff_32_42;
    job_meta_w[424:378] = 47'd16777216;
    job_meta_w[676:581] = 96'h000000000000400000000000;
    // THE GOURAUD PLANES (owner decision R234 D1, 2026-09-21). Lanes 3, 4 and
    // 5 -- n0 only, as lane 0 above. Each n0 is 2^39, which against
    // `area2 = 2^24` is a quotient of 2^15 = 32768, i.e. HALF the Q0.16 unity
    // GEOM.LIGHT emits. A mid-grey is deliberate: zero would let the fitter
    // constant-fold three lanes away and report an area for a design that is
    // not the one being measured, which is the flattering direction.
    job_meta_w[1396:1301] = 96'h000000000000008000000000;   // R, lane 3
    job_meta_w[1636:1541] = 96'h000000000000008000000000;   // G, lane 4
    job_meta_w[1876:1781] = 96'h000000000000008000000000;   // B, lane 5
  end

  // Recoverable frame fault is captured into a held request before it can affect
  // synthetic job admission.  Lifetime structural fault is deliberately absent
  // from this request: only reset can recover that condition.
  assign setup_blocking_fault_w = setup_fault_q &&
      (setup_fault_cause_q[0] || setup_fault_cause_q[2] ||
       setup_fault_cause_q[3]);
  assign job_valid_w = job_pending_q && !frame_fault_clear_pending_q;
  assign frame_fault_clear_valid_w = frame_fault_clear_pending_q;
  assign fill_req_ready_w = !fill_busy_q;
  assign fill_data_valid_w = fill_busy_q;
  assign fill_data_w = (fill_beat_q == 3'd0) ? 16'h0005 : 16'h0000;
  assign sheet_req_ready_w = 1'b1;
  assign fb_ready_w = stimulus_lfsr_q[0] || stimulus_lfsr_q[3];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      job_pending_q <= 1'b0;
      frame_fault_clear_pending_q <= 1'b0;
      fill_busy_q <= 1'b0;
      fill_beat_q <= 3'd0;
      jobs_accepted_q <= 32'd0;
      fills_accepted_q <= 32'd0;
      fill_data_beats_q <= 32'd0;
      fb_words_q <= 32'd0;
      tiles_done_q <= 32'd0;
      cfg_commands_q <= 32'd0;
      palette_commands_q <= 32'd0;
      saw_green_q <= 1'b0;
      saw_index5_q <= 1'b0;
      saw_candidate_q <= 1'b0;
      saw_fragment_q <= 1'b0;
      saw_fill_q <= 1'b0;
      saw_fb_stall_q <= 1'b0;
      first_nonzero_fb_q <= 16'd0;
    end else begin
      // Handshake has priority over the old frame-fault level sampled on the same
      // edge.  If a genuinely new/reasserted fault survives the clear, its level
      // is observed on the following edge and creates the next held request.
      if (frame_fault_clear_fire_w)
        frame_fault_clear_pending_q <= 1'b0;
      else if (frame_fault_w)
        frame_fault_clear_pending_q <= 1'b1;

      if (!job_pending_q && (setup_state_q == S_RUN) && quiet_w &&
          !setup_blocking_fault_w && !lifetime_structural_fault_w &&
          !frame_fault_w && !frame_fault_clear_pending_q)
        job_pending_q <= 1'b1;
      if (job_fire_w) begin
        job_pending_q <= 1'b0;
        jobs_accepted_q <= jobs_accepted_q + 32'd1;
      end

      if (fill_fire_w) begin
        fill_busy_q <= 1'b1;
        fill_beat_q <= 3'd0;
        fills_accepted_q <= fills_accepted_q + 32'd1;
        saw_fill_q <= 1'b1;
      end else if (fill_busy_q) begin
        fill_data_beats_q <= fill_data_beats_q + 32'd1;
        if (fill_beat_q == 3'd7) begin
          fill_busy_q <= 1'b0;
          fill_beat_q <= 3'd0;
        end else begin
          fill_beat_q <= fill_beat_q + 3'd1;
        end
      end

      if (cfg_fire_w) cfg_commands_q <= cfg_commands_q + 32'd1;
      if (pal_fire_w) palette_commands_q <= palette_commands_q + 32'd1;
      if (fb_fire_w) begin
        fb_words_q <= fb_words_q + 32'd1;
        if (fb_rgb565_w == 16'h07e0) saw_green_q <= 1'b1;
        if ((fb_rgb565_w != 16'd0) && (first_nonzero_fb_q == 16'd0))
          first_nonzero_fb_q <= fb_rgb565_w;
      end
      if (tile_done_w) tiles_done_q <= tiles_done_q + 32'd1;
      if (stage_candidate_valid_w) saw_candidate_q <= 1'b1;
      if (stage_fragment_valid_w) begin
        saw_fragment_q <= 1'b1;
        if ((stage_fragment_index_w == 8'd5) &&
            (stage_fragment_status_w == 8'd0) &&
            (stage_fragment_rgb_w == 24'h00ff00) &&
            (stage_fragment_alpha_w == 8'hff))
          saw_index5_q <= 1'b1;
      end
      if (fb_valid_w && !fb_ready_w) saw_fb_stall_q <= 1'b1;
    end
  end

  zhao_raster_tile_pipe_v2 #(
      .ATTR_DSP3(ATTR_DSP3),
      .BILERP_DSP2(BILERP_DSP2)
  ) u_tile (
      .clk(clk), .rst_n(rst_n),
      .job_valid_i(job_valid_w), .job_ready_o(job_ready_w),
      .job_ax_i(21'sd0), .job_ay_i(21'sd0),
      .job_bx_i(21'sd4096), .job_by_i(21'sd0),
      .job_cx_i(21'sd0), .job_cy_i(21'sd4096),
      .job_first_i(1'b1), .job_last_i(1'b1),
      .job_tile_x_i(12'sd0), .job_tile_y_i(12'sd0),
      .job_tile_index_i(jobs_accepted_q[15:0]),
      .job_src_id_i(stimulus_lfsr_q[15:0]),
    .job_profile_bad_i(job_profile_bad_w),
      .job_meta_i(job_meta_w), .frame_clear_word_i(64'd0),
      .frame_fault_clear_valid_i(frame_fault_clear_valid_w),
      .frame_fault_clear_ready_o(frame_fault_clear_ready_w),
      .frame_fault_o(frame_fault_w),
      .lifetime_structural_fault_o(lifetime_structural_fault_w),
      .cfg_valid_i(cfg_valid_w), .cfg_ready_o(cfg_ready_w),
      .cfg_op_i(cfg_op_w), .cfg_page_generation_i(cfg_generation_w),
      .cfg_selector_i(cfg_selector_w), .cfg_row_i(cfg_row_w),
      .cfg_crc32_i(cfg_crc_w), .cfg_rsp_valid_o(cfg_rsp_valid_w),
      .cfg_rsp_ready_i(1'b1), .cfg_rsp_op_o(cfg_rsp_op_w),
      .cfg_rsp_status_o(cfg_rsp_status_w),
      .cfg_rsp_page_generation_o(cfg_rsp_generation_w),
      .active_page_generation_o(active_page_generation_w),
      .fill_req_valid_o(fill_req_valid_w), .fill_req_ready_i(fill_req_ready_w),
      .fill_req_addr_o(fill_req_addr_w), .fill_data_valid_i(fill_data_valid_w),
      .fill_data_i(fill_data_w), .fill_refused_i(1'b0),
      .pal_load_valid_i(pal_valid_w), .pal_load_ready_o(pal_ready_w),
      .pal_load_op_i(pal_op_w), .pal_load_slot_i(2'd0),
      .pal_load_gen_i(8'd1), .pal_load_idx_i(palette_index_q),
      .pal_load_rgb565_i((palette_index_q == 8'd5) ? 16'h07e0 : 16'h0000), .pal_load_crc_ok_i(1'b1),
      .sheet_req_valid_o(sheet_req_valid_w),
      .sheet_req_ready_i(sheet_req_ready_w), .sheet_req_op_o(sheet_req_op_w),
      .sheet_req_handle_o(sheet_req_handle_w),
      .sheet_req_texel_o(sheet_req_texel_w), .sheet_req_src_id_o(sheet_req_src_w),
      .pg_valid_i(1'b0), .pg_ready_o(pg_ready_w), .pg_op_i(2'd0),
      .pg_status_i(2'd0), .pg_tag_i(8'd0), .pg_strength_i(8'd0),
      .pg_src_id_i(16'd0),
      .fb_valid_o(fb_valid_w), .fb_ready_i(fb_ready_w),
      .fb_rgb565_o(fb_rgb565_w), .fb_tag_o(fb_tag_w), .fb_addr_o(fb_addr_w),
      .fb_x_o(fb_x_w), .fb_y_o(fb_y_w), .fb_last_o(fb_last_w),
      .fb_src_id_o(fb_src_w),
      .tile_crc_o(tile_crc_w), .tile_crc_index_o(tile_crc_index_w),
      .tile_done_o(tile_done_w), .tile_cov_count_o(tile_cov_count_w),
      .tile_degenerate_o(tile_degenerate_w), .front_bank_o(front_bank_w),
      .tilestore_references_o(tilestore_references_w),
      .resolved_tiles_o(resolved_tiles_w), .early_z_rejects_o(early_z_rejects_w),
      .early_z_covered_o(early_z_covered_w),
      .fragment_covered_o(fragment_covered_w),
      .blended_fragments_o(blended_fragments_w), .bin_mask_o(bin_mask_w),
      .z_floor_o(z_floor_w), .fragment_error_o(fragment_error_w),
      .quiet_o(quiet_w), .raster_abort_o(raster_abort_w),
      .local_attribute_abort_o(local_attribute_abort_w),
      .local_fault_pulse_o(local_fault_pulse_w),
      .local_fault_count_o(local_fault_count_w),
      .coordinate_fault_count_o(coordinate_fault_count_w),
      .range_fault_count_o(range_fault_count_w),
      .aux_profile_fault_count_o(aux_profile_fault_count_w),
      .candidate_cancel_count_o(candidate_cancel_count_w),
      .local_drop_count_o(local_drop_count_w), .jobs_started_o(jobs_started_w),
      .jobs_sunk_o(jobs_sunk_w), .sequence_abort_o(sequence_abort_w),
      .sequence_mismatch_o(sequence_mismatch_w),
      .sequence_drop_count_o(sequence_drop_count_w),
      .admission_sequence_o(admission_sequence_w),
      .expected_sequence_o(expected_sequence_w),
      .returned_sequence_o(returned_sequence_w),
      .packet_c_cand_fire_o(packet_c_cand_fire_w),
      .packet_c_fragment_fire_o(packet_c_fragment_fire_w),
      .packet_c_drop_fire_o(packet_c_drop_fire_w),
      .texture_fragments_o(texture_fragments_w),
      .texture_cache_hits_o(texture_cache_hits_w),
      .texture_cache_misses_o(texture_cache_misses_w),
      .texture_palette_lookups_o(texture_palette_lookups_w),
      .texture_plan_accepted_o(texture_plan_accepted_w),
      .texture_dispatch_accepted_o(texture_dispatch_accepted_w),
      .texture_combine_refused_o(texture_combine_refused_w),
      .texture_samples_o(texture_samples_w),
      .coverage_hold_valid_o(coverage_hold_valid_w),
      .coverage_delivered_mask_o(coverage_delivered_mask_w),
      .start_delivered_mask_o(start_delivered_mask_w),
      .attribute_idle_o(attribute_idle_w),
      .earlyz_hold_valid_o(earlyz_hold_valid_w), .skid_level_o(skid_level_w),
      .stage_candidate_valid_o(stage_candidate_valid_w),
      .stage_candidate_data_o(stage_candidate_data_w),
      .stage_fragment_valid_o(stage_fragment_valid_w),
      .stage_fragment_addr_o(stage_fragment_addr_w),
      .stage_fragment_depth_o(stage_fragment_depth_w),
      .stage_fragment_state_o(stage_fragment_state_w),
      .stage_fragment_src_id_o(stage_fragment_src_w),
      .stage_fragment_texel_rgb_o(stage_fragment_rgb_w),
      .stage_fragment_texel_a_o(stage_fragment_alpha_w),
      .stage_fragment_texel_idx_o(stage_fragment_index_w),
      .stage_fragment_status_o(stage_fragment_status_w),
      .texture_quiet_o(texture_quiet_w), .fragment_idle_o(fragment_idle_w));

`ifndef SYNTHESIS
`ifndef QUARTUS_SYNTHESIS
  always_ff @(posedge clk) begin
    if (rst_n) begin
      if ($past(rst_n && frame_fault_clear_valid_w &&
                !frame_fault_clear_ready_w))
        assert (frame_fault_clear_valid_w)
          else $error("g8a_fit_top: recoverable clear request was not held");
      if (frame_fault_clear_pending_q)
        assert (!job_valid_w && !job_fire_w)
          else $error("g8a_fit_top: synthetic job escaped pending clear");
      if (lifetime_structural_fault_w && !frame_fault_w &&
          !$past(frame_fault_clear_pending_q))
        assert (!frame_fault_clear_pending_q)
          else $error("g8a_fit_top: lifetime fault initiated recoverable clear");
      if ($past(rst_n && frame_fault_w && !frame_fault_clear_fire_w))
        assert (frame_fault_clear_pending_q)
          else $error("g8a_fit_top: recoverable frame fault was not captured");
    end
  end
`endif
`endif

`ifndef QUARTUS_SYNTHESIS
  export "DPI-C" task zhao_g8a_get_activity;
  task zhao_g8a_get_activity(
      output int unsigned jobs_o,
      output int unsigned fills_o,
      output int unsigned fill_beats_o,
      output int unsigned fb_words_o,
      output int unsigned tiles_o,
      output int unsigned cfg_commands_o,
      output int unsigned palette_commands_o,
      output bit setup_fault_o,
      output bit [3:0] setup_fault_cause_o,
      output bit frame_fault_o,
      output bit fragment_error_o,
      output bit saw_green_o,
      output bit saw_index5_o,
      output bit [15:0] first_nonzero_fb_o,
      output bit saw_candidate_o,
      output bit saw_fragment_o,
      output bit saw_fill_o,
      output bit saw_fb_stall_o,
      output bit [7:0] active_generation_o);
    jobs_o = jobs_accepted_q;
    fills_o = fills_accepted_q;
    fill_beats_o = fill_data_beats_q;
    fb_words_o = fb_words_q;
    tiles_o = tiles_done_q;
    cfg_commands_o = cfg_commands_q;
    palette_commands_o = palette_commands_q;
    setup_fault_o = setup_fault_q;
    setup_fault_cause_o = setup_fault_cause_q;
    frame_fault_o = frame_fault_w;
    fragment_error_o = fragment_error_w;
    saw_green_o = saw_green_q;
    saw_index5_o = saw_index5_q;
    first_nonzero_fb_o = first_nonzero_fb_q;
    saw_candidate_o = saw_candidate_q;
    saw_fragment_o = saw_fragment_q;
    saw_fill_o = saw_fill_q;
    saw_fb_stall_o = saw_fb_stall_q;
    active_generation_o = active_page_generation_w;
  endtask
`endif

endmodule

`default_nettype wire
