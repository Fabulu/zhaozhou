// zhao_raster_texture_stage_v3.sv -- Packet-C post-Early-Z texture join.
//
// One 491-bit candidate is unpacked through zhao_render_texture_pkg, admitted
// atomically to exactly one Packet-B V3 island, and returned as the exact
// RASTER.FRAGMENT packet.  The V3 owner carries both the 32-bit admission
// sequence and all 128 continuation bits. This stage owns one bounded elastic
// result head after V3 acceptance, but no candidate/output FIFO, context sidecar,
// slot allocator, or release cursor.
//
// A returned-sequence mismatch is a recoverable frame terminal.  The mismatched
// beat and every later ordered V3 output are always accepted and dropped so the
// sole owner can release.  This suppresses the fragment offer at this boundary;
// Packet D/H still own physical write gating and lease RELEASE/PUBLISH.
`default_nettype none

`ifndef ZHAO_PACKET_C_RETURNED_SEQUENCE
`define ZHAO_PACKET_C_RETURNED_SEQUENCE(sequence) sequence
`endif
`ifndef ZHAO_PACKET_C_V3_OUT_READY
`define ZHAO_PACKET_C_V3_OUT_READY(aborting, mismatch, fragment_ready, match) \
    ((aborting) || (mismatch) || ((fragment_ready) && (match)))
`endif

module zhao_raster_texture_stage_v3 #(
    parameter bit MIGRATION_SHADOWS = 1'b0,
    parameter bit BILERP_DSP2 = 1'b0
) (
    input  logic clk,
    input  logic rst_n,

    // One complete held post-Early-Z candidate.  The upstream Packet-D skid is
    // outside this module; cand_ready_o is the sole atomic admission ready.
    input  logic         cand_valid_i,
    output logic         cand_ready_o,
    input  logic [490:0] cand_data_i,

    // Canonical recoverable frame-fault handshake, mirrored from Packet B.
    input  logic         frame_fault_clear_valid_i,
    output logic         frame_fault_clear_ready_o,
    output logic         frame_fault_o,
    output logic         lifetime_structural_fault_o,

    // Sealed binding-page loader.
    input  logic         cfg_valid_i,
    output logic         cfg_ready_o,
    input  logic [1:0]   cfg_op_i,
    input  logic [7:0]   cfg_page_generation_i,
    input  logic [7:0]   cfg_selector_i,
    input  logic [74:0]  cfg_row_i,
    input  logic [31:0]  cfg_crc32_i,
    output logic         cfg_rsp_valid_o,
    input  logic         cfg_rsp_ready_i,
    output logic [1:0]   cfg_rsp_op_o,
    output logic [3:0]   cfg_rsp_status_o,
    output logic [7:0]   cfg_rsp_page_generation_o,
    output logic [7:0]   active_page_generation_o,

    // One-line texture-cache fill.
    output logic         fill_req_valid_o,
    input  logic         fill_req_ready_i,
    output logic [31:0]  fill_req_addr_o,
    input  logic         fill_data_valid_i,
    input  logic [15:0]  fill_data_i,
    input  logic         fill_refused_i,

    // Palette programming.
    input  logic         pal_load_valid_i,
    output logic         pal_load_ready_o,
    input  logic [1:0]   pal_load_op_i,
    input  logic [1:0]   pal_load_slot_i,
    input  logic [7:0]   pal_load_gen_i,
    input  logic [7:0]   pal_load_idx_i,
    input  logic [15:0]  pal_load_rgb565_i,
    input  logic         pal_load_crc_ok_i,

    // TERRAIN.NORMALMAP's config and tile-upload write port. See
    // `zhao_texture_island_v3_top`'s port comment; this module carries it
    // unchanged and owns exactly one thing, which is THE APPLICATION below.
    //
    // THE PER-FRAGMENT DECLARATION IS NOT A PORT HERE, deliberately: it rides
    // `cand_data_i` as `detail_required`, so it arrives WITH the fragment it
    // describes instead of beside it.
    input  logic         dtl_we_i,
    input  logic         dtl_sel_i,
    input  logic [12:0]  dtl_addr_i,
    input  logic [31:0]  dtl_data_i,

    // Complete Surface Sheet READ/page boundary.
    output logic         sheet_req_valid_o,
    input  logic         sheet_req_ready_i,
    output logic [1:0]   sheet_req_op_o,
    output logic [31:0]  sheet_req_handle_o,
    output logic [11:0]  sheet_req_texel_o,
    output logic [15:0]  sheet_req_src_id_o,
    input  logic         pg_valid_i,
    output logic         pg_ready_o,
    input  logic [1:0]   pg_op_i,
    input  logic [1:0]   pg_status_i,
    input  logic [7:0]   pg_tag_i,
    input  logic [7:0]   pg_strength_i,
    input  logic [15:0]  pg_src_id_i,

    // Exact RASTER.FRAGMENT input beat.  Status is carried beside the complete
    // beat for Packet-H accounting; the unchanged fragment leaf consumes the
    // remaining fields only.  Every field is sourced solely from one held V3
    // result/retirement context.
    output logic         frag_valid_o,
    input  logic         frag_ready_i,
    output logic [7:0]   frag_addr_o,
    output logic [23:0]  frag_depth_o,
    output logic [31:0]  frag_state_o,
    output logic [15:0]  frag_src_id_o,
    output logic [23:0]  frag_vert_rgb_o,
    output logic [7:0]   frag_vert_a_o,
    output logic [7:0]   frag_tag_o,
    output logic [7:0]   frag_sten_ref_o,
    output logic [23:0]  frag_texel_rgb_o,
    output logic [7:0]   frag_texel_a_o,
    output logic [7:0]   frag_texel_idx_o,
    output logic [7:0]   frag_status_o,

    // Structural/test status.  These levels and event pulses let a private
    // harness classify synthetic RELEASE versus PUBLISH after fragment drain;
    // they are not a lease implementation and do not cancel an upstream skid.
    output logic         quiet_o,
    output logic         sequence_abort_o,
    output logic [31:0]  sequence_drop_count_o,
    output logic         sequence_mismatch_o,
    output logic [31:0]  admission_sequence_o,
    output logic [31:0]  expected_sequence_o,
    output logic [31:0]  returned_sequence_o,
    output logic         cand_fire_o,
    output logic         fragment_fire_o,
    output logic         drop_fire_o,

    // Packet-B compatibility/evidence outputs are mirrored without reduction so
    // the nested island remains fully accountable from this composition seam.
    output logic         err_fragrob_wq_overflow_o,
    output logic         err_fragrob_id_error_o,
    output logic         err_aux_degenerate_o,
    output logic         err_rcp_q_o,
    output logic [31:0]  cnt_reorder_held_o,
    output logic [31:0]  cnt_live_peak_o,
    output logic [31:0]  cnt_fragments_o,
    output logic [31:0]  cnt_cache_hits_o,
    output logic [31:0]  cnt_cache_misses_o,
    output logic [31:0]  cnt_palette_lookups_o,
    output logic [31:0]  cnt_bilerp_jobs_o,
    output logic [31:0]  cnt_mosaic_samples_o,
    output logic [31:0]  cnt_texture_samples_o,  // R9: TEXTURE.TMU's texture_samples
    output logic [31:0]  cnt_aux_accepted_o,
    output logic [31:0]  cnt_combine_refused_o,
    output logic [31:0]  cnt_combine_phases_o,
    output logic [31:0]  cnt_rcp_completed_o,
    output logic [31:0]  cnt_persp_fragments_o,
    output logic [31:0]  cnt_dispatch_accepted_o,
    output logic [31:0]  cnt_plan_accepted_o,
    output logic [31:0]  cnt_fragrob_id_errors_o,
    output logic         shadow_present_o,
    output logic [31:0]  meta_shadow_mismatch_o,
    output logic [31:0]  meta_shadow_reads_o,
    output logic [31:0]  meta_align_err_o,
    output logic [31:0]  meta_align_chk_o,
    output logic [31:0]  meta_bil_err_o,
    output logic [31:0]  meta_bil_chk_o,
    output logic [31:0]  meta_near_err_o,
    output logic [31:0]  meta_near_chk_o,
    output logic [20:0]  meta_bil_first_q_o,
    output logic [20:0]  meta_bil_first_t_o,
    output logic [17:0]  meta_bil_first_tok_o,
    output logic [31:0]  meta_genmis_o,
    output logic [31:0]  cnt_combine_jobs_o [0:7],
    output logic [31:0]  cnt_palette_stale_o,
    output logic [31:0]  cnt_palette_cold_o,
    output logic         err_rsp_dropped_o,
    output logic         err_bil_chan_o,
    output logic [31:0]  cnt_near_refused_o,
    output logic [31:0]  err_unknown_class_o,
    output logic [31:0]  err_class_invalid_o,
    output logic [31:0]  err_palette_unusable_o,
    output logic [31:0]  err_class_mismatch_o,
    output logic         err_plan_mode_o,

    // TERRAIN.NORMALMAP's evidence, carried out of the island unreduced.
    output logic [31:0]  cnt_detail_fragments_o,
    output logic [31:0]  cnt_detail_zeroed_o,
    output logic [31:0]  cnt_detail_railed_o,
    output logic [31:0]  cnt_detail_cold_o,
    output logic [31:0]  cnt_detail_published_o,
    output logic [31:0]  err_detail_lost_o,
    // Fragments whose lit colour lane this module actually CHANGED. It is
    // deliberately NOT `delta != 0`: a declared fragment over a flat patch of
    // the detail tile has delta 0 and is correctly unchanged, and a counter
    // that could not tell "applied and happened to agree" from "not applied"
    // would be the blind detector this repository has a chapter about. This
    // counts the beats on which the APPLIED value differs from the input,
    // which is the thing a picture would show.
    output logic [31:0]  cnt_detail_applied_o,
    output logic         dtl_table_ready_o
);
  import zhao_render_texture_pkg::*;

  initial begin : p_packet_c_profile
    if (($bits(cand_data_i) != RASTER_PRETEX_W) ||
        ($bits(zhao_raster_retire_ctx_v2_t) != RASTER_RETIRE_CTX_W) ||
        ($bits(zhao_texture_result_v2_t) != TEXTURE_RESULT_W))
      $fatal(1, "raster texture stage V3 Packet-C width contract changed");
  end

  zhao_raster_pretex_v2_t       candidate_w;
  zhao_raster_continuation_v2_t admission_continuation_w;
  zhao_texture_v3_request_v2_t  admission_request_w;
  zhao_raster_retire_ctx_v2_t   admission_retire_ctx_w;
  zhao_raster_retire_ctx_v2_t   returned_retire_ctx_w;
  zhao_texture_result_v2_t      returned_result_w;

  logic [31:0] admission_sequence_q;
  logic [31:0] expected_sequence_q;
  logic        sequence_abort_q;
  logic [31:0] sequence_drop_count_q;

  logic        v3_frag_ready_w;
  logic        v3_frag_valid_w;
  logic        v3_out_valid_w;
  logic        v3_out_ready_w;
  logic signed [8:0] v3_out_detail_delta_w;
  logic [23:0] v3_out_rgb_w;
  logic [7:0]  v3_out_a_w;
  logic [7:0]  v3_out_texel_idx_w;
  logic [7:0]  v3_out_status_w;
  logic [15:0] v3_out_tag_w;
  logic [159:0] v3_out_retire_ctx_w;
  logic        v3_out_refused_w;
  logic        v3_quiet_w;
  logic        v3_frame_fault_w;
  logic        v3_clear_ready_w;

  // One bounded elastic result head. It owns only a packet already accepted
  // from V3; it has no allocation cursor and is not a second lifecycle queue.
  logic         retire_head_v_q;
  logic [23:0]  retire_head_rgb_q;
  logic [7:0]   retire_head_a_q;
  logic [7:0]   retire_head_texel_idx_q;
  logic [7:0]   retire_head_status_q;
  logic [15:0]  retire_head_tag_q;
  logic [159:0] retire_head_ctx_q;
  logic         retire_head_refused_q;
  // TERRAIN.NORMALMAP's delta for the held fragment. Captured by the SAME
  // enable as `retire_head_ctx_q`, which is correct HERE and is the opposite of
  // the fault CLAUDE.md's metadata-swap chapter describes: that chapter is
  // about a CHECKER whose two operands share an enable and can therefore never
  // disagree. This is not a checker -- it is a payload field travelling with
  // the payload it belongs to, and sharing the capture enable is precisely what
  // keeps the two from separating on a stall. The identity check that CAN fire
  // is upstream, inside the island, where the delta's generation is compared
  // against v3own's published owner generation and the two come from different
  // registers in different modules.
  logic signed [8:0] retire_head_detail_q;
  logic         retire_head_capture_w, retire_head_consume_w;
  logic         retire_head_reload_credit_w, retire_head_drop_policy_w;

  logic [31:0] returned_sequence_w;
  logic        sequence_matches_w;
  logic        sequence_mismatch_w;
  logic        clear_fire_w;

  always_comb begin
    candidate_w = unpack_raster_pretex(cand_data_i);
    admission_continuation_w = pretex_continuation(candidate_w);
    admission_request_w = pretex_texture_request(candidate_w);
    admission_retire_ctx_w = make_raster_retire_ctx(
        admission_sequence_q, admission_continuation_w);
    returned_retire_ctx_w = unpack_raster_retire_ctx(retire_head_ctx_q);
    returned_result_w = unpack_texture_result({retire_head_status_q,
                                                retire_head_texel_idx_q,
                                                retire_head_a_q,
                                                retire_head_rgb_q});
  end

  assign returned_sequence_w =
      `ZHAO_PACKET_C_RETURNED_SEQUENCE(returned_retire_ctx_w.raster_sequence);
  assign sequence_matches_w = (returned_sequence_w == expected_sequence_q);
  assign sequence_mismatch_w =
      retire_head_v_q && !sequence_matches_w && !sequence_abort_q;

  // Pre-edge H=head valid, C=head consume, R=data-independent reload credit,
  // A=V3 acceptance. H' = A || (H && !C), and A = v3_valid && (!H || R).
  // A mismatch is detected and dropped on its first visible edge. If downstream
  // fragment ready is low, that drop deliberately leaves the head empty for one
  // edge rather than feeding returned sequence data back into V3 ready.
  //
  // The committed old-ready control deliberately restores match-gated reload
  // and drop. Production keeps returned data completely out of V3 ready.
`ifdef ZHAO_PACKET_C_MUTANT_OLD_READY
  assign retire_head_reload_credit_w = `ZHAO_PACKET_C_V3_OUT_READY(
      sequence_abort_q, sequence_mismatch_w, frag_ready_i, sequence_matches_w);
  assign retire_head_drop_policy_w = `ZHAO_PACKET_C_V3_OUT_READY(
      sequence_abort_q, sequence_mismatch_w, frag_ready_i, sequence_matches_w);
`else
  assign retire_head_reload_credit_w = sequence_abort_q || frag_ready_i;
  assign retire_head_drop_policy_w = 1'b1;
`endif

  // The mismatch term suppresses a candidate on the detecting edge. The V3
  // output handshake itself depends only on head occupancy, registered abort,
  // and external fragment credit -- never on returned sequence or payload.
  assign cand_ready_o =
      v3_frag_ready_w && !sequence_abort_q && !sequence_mismatch_w;
  assign v3_frag_valid_w =
      cand_valid_i && !sequence_abort_q && !sequence_mismatch_w;
  assign cand_fire_o = cand_valid_i && cand_ready_o;

  assign frag_valid_o =
      retire_head_v_q && sequence_matches_w && !sequence_abort_q;
  assign fragment_fire_o = frag_valid_o && frag_ready_i;
  assign drop_fire_o = retire_head_v_q &&
      (sequence_abort_q || sequence_mismatch_w) && retire_head_drop_policy_w;
  assign retire_head_consume_w = fragment_fire_o || drop_fire_o;
  assign v3_out_ready_w = !retire_head_v_q || retire_head_reload_credit_w;
  assign retire_head_capture_w = v3_out_valid_w && v3_out_ready_w;

  assign frag_addr_o      = returned_retire_ctx_w.raster_continuation.earlyz.in_tile_addr;
  assign frag_depth_o     = returned_retire_ctx_w.raster_continuation.earlyz.invw24;
  assign frag_state_o     = returned_retire_ctx_w.raster_continuation.earlyz.fragment_state;
  assign frag_src_id_o    = returned_retire_ctx_w.raster_continuation.earlyz.source_id;
  // ===========================================================================
  // THE DETAIL NORMAL'S APPLICATION SEAM (NORMALMAP, 2026-09-26)
  // ===========================================================================
  // `zref::terrain::normalmap_apply` is the law and it is TRANSCRIBED, not
  // derived (reference/include/zref/zref_terrain_normalmap.hpp):
  //
  //     inline uint8_t normalmap_apply(uint8_t v, int32_t delta) {
  //       const int32_t sum = int32_t(v) + delta;
  //       if (sum < 0) return 0;
  //       if (sum > 255) return 255;
  //       return uint8_t(sum);
  //     }
  //
  // with the function's own preceding comment naming the operand: *"the delta
  // lands on the flat lit colour lanes, saturating unsigned 8-bit"*. There is
  // exactly one flat lit colour lane in this machine and it is the port below --
  // `zhao_raster_fragment`'s `frag_vert_rgb_i`, whose own comment reads
  // "interpolated, lit, tinted, FOGGED", and which for any primitive whose
  // fragment-state SHADE_MOD bit is clear IS the source colour outright
  // (`zhao_raster_fragment.sv:717-721`). Terrain is exactly such a primitive:
  // `TERR_FRAG_STATE` is the opaque profile and its SHADE_MOD is 0.
  //
  // THE DELTA IS MONOCHROME AND THAT IS THE CONTRACT, not an approximation
  // taken here. `zref_terrain_normalmap.hpp` calls the s9 delta "colour-lane
  // LSBs under a white sun, the contract's declared monochrome approximation",
  // so one delta lands on all three channels. A per-channel delta would need
  // three sun colours and is not what the ratified block emits.
  //
  // NO DECLARATION, NO CHANGE, BY CONSTRUCTION. A fragment whose producer did
  // not declare detail gets `f_detail_i = 0` at the leaf, which forces delta
  // exactly 0 with the tile read enable held low -- so the expression below is
  // the identity for every such fragment, bit for bit, and the mesh path is
  // untouched without a second gate to keep in step with the first.
  function automatic logic [7:0] detail_apply(input logic [7:0] v,
                                              input logic signed [8:0] d);
    logic signed [10:0] sum;
    begin
      sum = $signed({3'd0, v}) + $signed({{2{d[8]}}, d});
      if (sum < 11'sd0) detail_apply = 8'd0;
      else if (sum > 11'sd255) detail_apply = 8'd255;
      else detail_apply = sum[7:0];
    end
  endfunction

  wire [23:0] frag_vert_rgb_raw_w =
      returned_retire_ctx_w.raster_continuation.post_earlyz.vertex_rgb;
  assign frag_vert_rgb_o = {
      detail_apply(frag_vert_rgb_raw_w[23:16], retire_head_detail_q),
      detail_apply(frag_vert_rgb_raw_w[15:8],  retire_head_detail_q),
      detail_apply(frag_vert_rgb_raw_w[7:0],   retire_head_detail_q)};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) cnt_detail_applied_o <= 32'd0;
    else if (fragment_fire_o && (frag_vert_rgb_o != frag_vert_rgb_raw_w))
      cnt_detail_applied_o <= cnt_detail_applied_o + 32'd1;
  end
  assign frag_vert_a_o    = returned_retire_ctx_w.raster_continuation.post_earlyz.vertex_alpha;
  assign frag_tag_o       = returned_retire_ctx_w.raster_continuation.post_earlyz.effect_tag;
  assign frag_sten_ref_o  = returned_retire_ctx_w.raster_continuation.post_earlyz.stencil_reference;
  assign frag_texel_rgb_o = returned_result_w.rgb;
  assign frag_texel_a_o   = returned_result_w.alpha;
  assign frag_texel_idx_o = returned_result_w.sample0_raw_index;
  assign frag_status_o    = returned_result_w.status;

  assign clear_fire_w =
      frame_fault_clear_valid_i && frame_fault_clear_ready_o;
  assign frame_fault_clear_ready_o = !retire_head_v_q && v3_clear_ready_w;
  assign quiet_o = v3_quiet_w && !retire_head_v_q;
  assign sequence_abort_o = sequence_abort_q;
  assign sequence_drop_count_o = sequence_drop_count_q;
  assign sequence_mismatch_o = sequence_mismatch_w;
  assign admission_sequence_o = admission_sequence_q;
  assign expected_sequence_o = expected_sequence_q;
  assign returned_sequence_o = returned_sequence_w;

  // Include the detecting cycle combinationally so a terminal controller cannot
  // classify the frame clean in the cycle before sequence_abort_q latches.
  assign frame_fault_o =
      v3_frame_fault_w || sequence_abort_q || sequence_mismatch_w;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      retire_head_v_q <= 1'b0;
    end else begin
      retire_head_v_q <= retire_head_capture_w ||
                         (retire_head_v_q && !retire_head_consume_w);
    end
  end

  always_ff @(posedge clk) begin
    if (retire_head_capture_w) begin
      retire_head_rgb_q       <= v3_out_rgb_w;
      retire_head_a_q         <= v3_out_a_w;
      retire_head_texel_idx_q <= v3_out_texel_idx_w;
      retire_head_status_q    <= v3_out_status_w;
      retire_head_tag_q       <= v3_out_tag_w;
      retire_head_ctx_q       <= v3_out_retire_ctx_w;
      retire_head_detail_q    <= v3_out_detail_delta_w;
      retire_head_refused_q   <= v3_out_refused_w;
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      admission_sequence_q <= 32'd0;
      expected_sequence_q <= 32'd0;
      sequence_abort_q <= 1'b0;
      sequence_drop_count_q <= 32'd0;
    end else begin
      if (cand_fire_o)
        admission_sequence_q <= admission_sequence_q + 32'd1;

      // A newly detected mismatch wins over an accepted clear.  The expected
      // identity advances only for a real downstream fragment handshake; the
      // accepted quiet clear instead rebases it to the next admission sequence.
      if (sequence_mismatch_w) begin
        sequence_abort_q <= 1'b1;
      end else if (clear_fire_w) begin
        sequence_abort_q <= 1'b0;
      end

      if (sequence_mismatch_w) begin
        expected_sequence_q <= expected_sequence_q;
      end else if (clear_fire_w) begin
        expected_sequence_q <= admission_sequence_q;
      end else if (fragment_fire_o) begin
        expected_sequence_q <= expected_sequence_q + 32'd1;
      end

      if (drop_fire_o)
        sequence_drop_count_q <= sequence_drop_count_q + 32'd1;
    end
  end

  zhao_texture_island_v3_top #(
      .MIGRATION_SHADOWS(MIGRATION_SHADOWS),
      .DEPTH(16),
      .CTXW(64),
      .RCTXW(RASTER_RETIRE_CTX_W),
      .AUXCTXW(AUX_SURFACE_CTX_W),
      .BINDW(8),
      .LODW(8),
      .GENW(8),
      .LANES(4),
      .SRCW(18),
      .DATAW(64),
      .TOKW(18),
      .AUX_TOKW(14),
      .PAL_SLOTS(4),
      .PAL_ENTRIES(256),
      .BILERP_DSP2(BILERP_DSP2)
  ) u_texture_v3 (
      .clk(clk),
      .rst_n(rst_n),
      .frag_valid_i(v3_frag_valid_w),
      .frag_ready_o(v3_frag_ready_w),
      .frag_invw24_i(admission_continuation_w.earlyz.invw24),
      .frag_u_over_w_i(admission_request_w.u_over_w),
      .frag_v_over_w_i(admission_request_w.v_over_w),
      .frag_sample_count_i(admission_request_w.sample_count),
      .frag_binding_i(admission_request_w.base_binding_selector),
      .frag_lod_i(admission_request_w.lod_q4_4),
      .frag_recipe_i(admission_request_w.material_recipe),
      .frag_weight_i(admission_request_w.recipe_weight),
      .frag_ctx_i(64'd0),
      .frag_retire_ctx_i(pack_raster_retire_ctx(admission_retire_ctx_w)),
      .frag_aux_ctx_i(pack_aux_surface_ctx(admission_request_w.aux_surface_ctx)),
      .frag_aux_i(admission_request_w.aux_required),
      .frag_base_rgb_i(admission_request_w.base_rgb),
      .frag_base_a_i(admission_request_w.base_alpha),
      .frag_class_i(admission_request_w.response_class),
      .frag_pal_slot_i(admission_request_w.palette_slot),
      .frag_pal_gen_i(admission_request_w.palette_generation),
      .frame_fault_clear_valid_i(frame_fault_clear_valid_i && !retire_head_v_q),
      .frame_fault_clear_ready_o(v3_clear_ready_w),
      .frame_fault_o(v3_frame_fault_w),
      .lifetime_structural_fault_o(lifetime_structural_fault_o),
      .cfg_valid_i(cfg_valid_i),
      .cfg_ready_o(cfg_ready_o),
      .cfg_op_i(cfg_op_i),
      .cfg_page_generation_i(cfg_page_generation_i),
      .cfg_selector_i(cfg_selector_i),
      .cfg_row_i(cfg_row_i),
      .cfg_crc32_i(cfg_crc32_i),
      .cfg_rsp_valid_o(cfg_rsp_valid_o),
      .cfg_rsp_ready_i(cfg_rsp_ready_i),
      .cfg_rsp_op_o(cfg_rsp_op_o),
      .cfg_rsp_status_o(cfg_rsp_status_o),
      .cfg_rsp_page_generation_o(cfg_rsp_page_generation_o),
      .active_page_generation_o(active_page_generation_o),
      .fill_req_valid_o(fill_req_valid_o),
      .fill_req_ready_i(fill_req_ready_i),
      .fill_req_addr_o(fill_req_addr_o),
      .fill_data_valid_i(fill_data_valid_i),
      .fill_data_i(fill_data_i),
      .fill_refused_i(fill_refused_i),
      .pal_load_valid_i(pal_load_valid_i),
      .pal_load_ready_o(pal_load_ready_o),
      .pal_load_op_i(pal_load_op_i),
      .pal_load_slot_i(pal_load_slot_i),
      .pal_load_gen_i(pal_load_gen_i),
      .pal_load_idx_i(pal_load_idx_i),
      .pal_load_rgb565_i(pal_load_rgb565_i),
      .pal_load_crc_ok_i(pal_load_crc_ok_i),
      .frag_detail_i(admission_request_w.detail_required),
      .dtl_we_i(dtl_we_i),
      .dtl_sel_i(dtl_sel_i),
      .dtl_addr_i(dtl_addr_i),
      .dtl_data_i(dtl_data_i),
      .out_detail_delta_o(v3_out_detail_delta_w),
      .cnt_detail_fragments_o(cnt_detail_fragments_o),
      .cnt_detail_zeroed_o(cnt_detail_zeroed_o),
      .cnt_detail_railed_o(cnt_detail_railed_o),
      .cnt_detail_cold_o(cnt_detail_cold_o),
      .cnt_detail_published_o(cnt_detail_published_o),
      .err_detail_lost_o(err_detail_lost_o),
      .dtl_table_ready_o(dtl_table_ready_o),
      .sheet_req_valid_o(sheet_req_valid_o),
      .sheet_req_ready_i(sheet_req_ready_i),
      .sheet_req_op_o(sheet_req_op_o),
      .sheet_req_handle_o(sheet_req_handle_o),
      .sheet_req_texel_o(sheet_req_texel_o),
      .sheet_req_src_id_o(sheet_req_src_id_o),
      .pg_valid_i(pg_valid_i),
      .pg_ready_o(pg_ready_o),
      .pg_op_i(pg_op_i),
      .pg_status_i(pg_status_i),
      .pg_tag_i(pg_tag_i),
      .pg_strength_i(pg_strength_i),
      .pg_src_id_i(pg_src_id_i),
      .out_valid_o(v3_out_valid_w),
      .out_ready_i(v3_out_ready_w),
      .out_rgb_o(v3_out_rgb_w),
      .out_a_o(v3_out_a_w),
      .out_texel_idx_o(v3_out_texel_idx_w),
      .out_status_o(v3_out_status_w),
      .out_tag_o(v3_out_tag_w),
      .out_retire_ctx_o(v3_out_retire_ctx_w),
      .out_refused_o(v3_out_refused_w),
      .quiet_o(v3_quiet_w),
      .err_fragrob_wq_overflow_o(err_fragrob_wq_overflow_o),
      .err_fragrob_id_error_o(err_fragrob_id_error_o),
      .err_aux_degenerate_o(err_aux_degenerate_o),
      .err_rcp_q_o(err_rcp_q_o),
      .cnt_reorder_held_o(cnt_reorder_held_o),
      .cnt_live_peak_o(cnt_live_peak_o),
      .cnt_fragments_o(cnt_fragments_o),
      .cnt_cache_hits_o(cnt_cache_hits_o),
      .cnt_cache_misses_o(cnt_cache_misses_o),
      .cnt_palette_lookups_o(cnt_palette_lookups_o),
      .cnt_bilerp_jobs_o(cnt_bilerp_jobs_o),
      .cnt_mosaic_samples_o(cnt_mosaic_samples_o),
      .cnt_texture_samples_o(cnt_texture_samples_o),
      .cnt_aux_accepted_o(cnt_aux_accepted_o),
      .cnt_combine_refused_o(cnt_combine_refused_o),
      .cnt_combine_phases_o(cnt_combine_phases_o),
      .cnt_rcp_completed_o(cnt_rcp_completed_o),
      .cnt_persp_fragments_o(cnt_persp_fragments_o),
      .cnt_dispatch_accepted_o(cnt_dispatch_accepted_o),
      .cnt_plan_accepted_o(cnt_plan_accepted_o),
      .cnt_fragrob_id_errors_o(cnt_fragrob_id_errors_o),
      .shadow_present_o(shadow_present_o),
      .meta_shadow_mismatch_o(meta_shadow_mismatch_o),
      .meta_shadow_reads_o(meta_shadow_reads_o),
      .meta_align_err_o(meta_align_err_o),
      .meta_align_chk_o(meta_align_chk_o),
      .meta_bil_err_o(meta_bil_err_o),
      .meta_bil_chk_o(meta_bil_chk_o),
      .meta_near_err_o(meta_near_err_o),
      .meta_near_chk_o(meta_near_chk_o),
      .meta_bil_first_q_o(meta_bil_first_q_o),
      .meta_bil_first_t_o(meta_bil_first_t_o),
      .meta_bil_first_tok_o(meta_bil_first_tok_o),
      .meta_genmis_o(meta_genmis_o),
      .cnt_combine_jobs_o(cnt_combine_jobs_o),
      .cnt_palette_stale_o(cnt_palette_stale_o),
      .cnt_palette_cold_o(cnt_palette_cold_o),
      .err_rsp_dropped_o(err_rsp_dropped_o),
      .err_bil_chan_o(err_bil_chan_o),
      .cnt_near_refused_o(cnt_near_refused_o),
      .err_unknown_class_o(err_unknown_class_o),
      .err_class_invalid_o(err_class_invalid_o),
      .err_palette_unusable_o(err_palette_unusable_o),
      .err_class_mismatch_o(err_class_mismatch_o),
      .err_plan_mode_o(err_plan_mode_o)
  );

  // The legacy caller context is canonical zero at this seam.  Its compatibility
  // tag/refused aliases are intentionally not used to construct fragment data.
  logic unused_v3_legacy;
  assign unused_v3_legacy = ^{retire_head_tag_q, retire_head_refused_q};

  // synthesis translate_off
  logic held_fragment_valid_q;
  logic [175:0] held_fragment_payload_q;
  logic held_retire_head_valid_q;
  logic [224:0] held_retire_head_payload_q;
  always_ff @(posedge clk or negedge rst_n) begin : p_packet_c_assertions
    if (!rst_n) begin
      held_fragment_valid_q <= 1'b0;
      held_fragment_payload_q <= 176'd0;
      held_retire_head_valid_q <= 1'b0;
      held_retire_head_payload_q <= 225'd0;
    end else begin
      if (retire_head_capture_w && retire_head_v_q && !retire_head_consume_w)
        $fatal(1, "Packet-C result head overwritten without consumption");
      if (held_retire_head_valid_q &&
          (!retire_head_v_q ||
           ({retire_head_rgb_q, retire_head_a_q, retire_head_texel_idx_q,
             retire_head_status_q, retire_head_tag_q, retire_head_ctx_q,
             retire_head_refused_q} != held_retire_head_payload_q)))
        $fatal(1, "Packet-C result head changed while held");
      held_retire_head_valid_q <= retire_head_v_q && !retire_head_consume_w;
      if (retire_head_v_q && !retire_head_consume_w)
        held_retire_head_payload_q <=
            {retire_head_rgb_q, retire_head_a_q, retire_head_texel_idx_q,
             retire_head_status_q, retire_head_tag_q, retire_head_ctx_q,
             retire_head_refused_q};
      if (cand_fire_o && (sequence_abort_q || sequence_mismatch_w))
        $fatal(1, "Packet-C admitted a candidate during sequence terminal");
      if (frag_valid_o && (sequence_abort_q || sequence_mismatch_w))
        $fatal(1, "Packet-C exposed a fragment during sequence terminal");
      if (drop_fire_o && frag_valid_o)
        $fatal(1, "Packet-C dropped and exposed the same V3 output");
      if (held_fragment_valid_q &&
          (!frag_valid_o ||
           ({frag_addr_o, frag_depth_o, frag_state_o, frag_src_id_o,
             frag_vert_rgb_o, frag_vert_a_o, frag_tag_o,
             frag_sten_ref_o, frag_texel_rgb_o, frag_texel_a_o,
             frag_texel_idx_o, frag_status_o} != held_fragment_payload_q)))
        $fatal(1, "Packet-C held fragment beat changed under backpressure");
      held_fragment_valid_q <= frag_valid_o && !frag_ready_i;
      if (frag_valid_o && !frag_ready_i)
        held_fragment_payload_q <=
            {frag_addr_o, frag_depth_o, frag_state_o, frag_src_id_o,
             frag_vert_rgb_o, frag_vert_a_o, frag_tag_o,
             frag_sten_ref_o, frag_texel_rgb_o, frag_texel_a_o,
             frag_texel_idx_o, frag_status_o};
    end
  end
  // synthesis translate_on

endmodule : zhao_raster_texture_stage_v3

`undef ZHAO_PACKET_C_RETURNED_SEQUENCE
`undef ZHAO_PACKET_C_V3_OUT_READY

`default_nettype wire
