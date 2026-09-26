// tb_texture_desc_expand_bind_v2_composed.sv -- private Packet-B seam harness.
//
// New test-only composition authorised by the descriptor review. It connects the
// owner-keyed descriptor response to one held U/V/required-mask side record, then
// drives the real expander and resolver. PAD_MUTANT selects the committed bad-pad
// descriptor without changing any production source.
`default_nettype none

module tb_texture_desc_expand_bind_v2_composed #(
    parameter bit PAD_MUTANT = 1'b0
) (
    input logic clk,
    input logic rst_n,
    input logic frame_fault_clear_i,

    input logic desc_wr_valid_i,
    input logic [5:0] desc_wr_slot_i,
    input logic [7:0] desc_wr_owner_generation_i,
    input logic [223:0] desc_wr_aux_context_i,
    input logic [7:0] desc_wr_lod_q4_4_i,
    input logic [1:0] desc_wr_response_class_i,
    input logic desc_wr_aux_required_i,
    input logic [1:0] desc_wr_sample_count_i,
    input logic [1:0] desc_wr_palette_slot_i,
    input logic [7:0] desc_wr_palette_generation_i,
    input logic [7:0] desc_wr_mosaic_material_a_i,
    input logic [7:0] desc_wr_mosaic_material_b_i,
    input logic [7:0] desc_wr_mosaic_weight_i,
    input logic [7:0] desc_wr_binding_selector_i,
    input logic [7:0] desc_wr_active_page_generation_i,

    input logic frag_valid_i,
    output logic frag_ready_o,
    input logic [13:0] frag_owner_i,
    input logic signed [31:0] frag_u_i,
    input logic signed [31:0] frag_v_i,
    input logic [3:0] frag_required_mask_i,
    input logic frag_material_refused_i,

    input logic cfg_valid_i,
    output logic cfg_ready_o,
    input logic [1:0] cfg_op_i,
    input logic [7:0] cfg_page_generation_i,
    input logic [7:0] cfg_selector_i,
    input logic [74:0] cfg_row_i,
    input logic [31:0] cfg_crc32_i,
    output logic cfg_rsp_valid_o,
    input logic cfg_rsp_ready_i,
    output logic [1:0] cfg_rsp_op_o,
    output logic [3:0] cfg_rsp_status_o,
    output logic [7:0] cfg_rsp_page_generation_o,
    output logic [7:0] active_page_generation_o,

    output logic issue_valid_o,
    output logic [15:0] issue_handle_o,
    output logic plan_valid_o,
    input logic plan_ready_i,
    output logic [17:0] plan_route_token_o,
    output logic [31:0] plan_base_o,
    output logic [31:0] plan_mode_o,
    output logic [1:0] plan_palette_slot_o,
    output logic [7:0] plan_palette_generation_o,
    output logic signed [31:0] plan_u_o,
    output logic signed [31:0] plan_v_o,
    output logic [7:0] plan_lod_q4_4_o,
    output logic refuse_valid_o,
    input logic refuse_ready_i,
    output logic [15:0] refuse_handle_o,
    output logic [47:0] refuse_result_o,

    output logic [31:0] desc_pad_fault_o,
    output logic [31:0] desc_generation_mismatch_o,
    output logic desc_frame_fault_o,
    output logic [31:0] expand_fragments_o,
    output logic [31:0] expand_samples_o,
    output logic [31:0] expand_mosaics_o,
    output logic [31:0] expand_malformed_o,
    output logic expand_frame_fault_o,
    output logic [31:0] resolver_samples_o,
    output logic [31:0] resolver_plans_o,
    output logic [31:0] resolver_refusals_o,
    output logic [31:0] resolver_overflow_o,
    output logic [31:0] resolver_page_mismatch_o,
    // TERRAINTEX 2026-09-26: the mosaic pick's reader, composed here the way
    // `zhao_texture_island_v3_top` composes it -- the SAME two production
    // blocks, not a bench copy of the join.
    output logic [31:0] resolver_tileset_samples_o,
    output logic [31:0] mosaic_picks_delivered_o,
    output logic [31:0] mosaic_picks_held_o,
    output logic [31:0] mosaic_stale_slot_holds_o,
    output logic [7:0]  mosaic_pick_tile_o,
    output logic        mosaic_pick_ready_o,
    output logic resolver_frame_fault_o,
    output logic quiet_o
);
  logic desc_req_ready_w;
  logic desc_rsp_valid_w, desc_rsp_ready_w;
  logic [13:0] desc_rsp_owner_w;
  logic [286:0] desc_rsp_logical_w;
  /* verilator lint_off UNUSEDSIGNAL */
  // TIMING4 E1 untrusted view; this composed bench keeps checking the public
  // masked output, so the raw row is observed but not consumed here.
  logic [286:0] desc_rsp_logical_raw_w;
  /* verilator lint_on UNUSEDSIGNAL */
  logic desc_gen_ok_w, desc_pad_ok_w, desc_usable_w, desc_idle_w;
  logic [31:0] desc_writes_unused, desc_reads_unused;

  logic side_v_q;
  logic signed [31:0] side_u_q, side_v_coord_q;
  logic [3:0] side_required_q;
  logic side_material_refused_q;

  logic expand_ready_w, expand_idle_w;
  logic sample_valid_w, sample_ready_w;
  logic [15:0] sample_handle_w;
  logic [7:0] sample_page_generation_w;
  logic sample_selector_overflow_w, sample_force_refuse_w;
  logic [7:0] sample_binding_selector_w;
  logic signed [31:0] sample_u_w, sample_v_w;
  logic [7:0] sample_lod_w;
  logic [1:0] sample_class_witness_w, sample_palette_slot_witness_w;
  logic [7:0] sample_palette_generation_witness_w;
  logic mosaic_valid_w, mosaic_req_ready_w;
  logic [13:0] mosaic_owner_w;
  logic signed [31:0] mosaic_u_w, mosaic_v_w;
  logic [7:0] mosaic_a_w, mosaic_b_w, mosaic_weight_w;
  logic mosaic_rsp_valid_w, mosaic_idle_w;
  logic [7:0] mosaic_tile_w;
  /* verilator lint_off UNUSEDSIGNAL */
  // The mirrored texel pair is the mosaic's public contract and is NOT the
  // reader's business: the TMU recomputes it from u/v under the tileset
  // row's own mirror wrap. Observed here, consumed nowhere, exactly as in
  // the island.
  logic [5:0] mosaic_tx_w, mosaic_ty_w;
  /* verilator lint_on UNUSEDSIGNAL */
  /* verilator lint_off UNUSEDSIGNAL */
  // [15:14] is the island's own 2'b00 pad on the 16-bit src_id; the hold
  // takes the 14-bit owner beneath it.
  logic [15:0] mosaic_src_w;
  /* verilator lint_on UNUSEDSIGNAL */
  logic aux_valid_w, aux_issue_w;
  logic [13:0] aux_owner_unused, aux_issue_owner_unused;
  logic [223:0] aux_context_unused;
  logic aux_force_unused;
  logic [31:0] expand_aux_unused, expand_zero_unused, expand_overflow_unused;

  logic resolver_data_idle_w;
  logic resolver_admission_enable_w;
  logic resolver_cfg_idle_unused, resolver_crc_busy_unused;
  logic resolver_seal_pending_unused;
  logic [31:0] resolver_invalid_unused, resolver_witness_unused;
  logic [31:0] resolver_forced_unused, resolver_cfg_errors_unused;

  wire desc_read_accept_c = frag_valid_i && frag_ready_o;
  assign frag_ready_o = resolver_admission_enable_w && !side_v_q &&
                        desc_req_ready_w;
  assign desc_rsp_ready_w = side_v_q && expand_ready_w;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      side_v_q <= 1'b0;
      side_u_q <= 32'sd0;
      side_v_coord_q <= 32'sd0;
      side_required_q <= 4'd0;
      side_material_refused_q <= 1'b0;
    end else begin
      if (desc_rsp_valid_w && desc_rsp_ready_w)
        side_v_q <= 1'b0;
      if (desc_read_accept_c) begin
        side_v_q <= 1'b1;
        side_u_q <= frag_u_i;
        side_v_coord_q <= frag_v_i;
        side_required_q <= frag_required_mask_i;
        side_material_refused_q <= frag_material_refused_i;
      end
    end
  end

  generate
    if (!PAD_MUTANT) begin : g_descriptor_normal
      zhao_texture_early_desc_v2 u_desc (
          .clk(clk), .rst_n(rst_n),
          .frame_fault_clear_i(frame_fault_clear_i),
          .wr_valid_i(desc_wr_valid_i), .wr_slot_i(desc_wr_slot_i),
          .wr_owner_generation_i(desc_wr_owner_generation_i),
          .wr_aux_context_i(desc_wr_aux_context_i),
          .wr_lod_q4_4_i(desc_wr_lod_q4_4_i),
          .wr_response_class_i(desc_wr_response_class_i),
          .wr_aux_required_i(desc_wr_aux_required_i),
          .wr_sample_count_i(desc_wr_sample_count_i),
          .wr_palette_slot_i(desc_wr_palette_slot_i),
          .wr_palette_generation_i(desc_wr_palette_generation_i),
          .wr_mosaic_material_a_i(desc_wr_mosaic_material_a_i),
          .wr_mosaic_material_b_i(desc_wr_mosaic_material_b_i),
          .wr_mosaic_weight_i(desc_wr_mosaic_weight_i),
          .wr_binding_selector_i(desc_wr_binding_selector_i),
          .wr_active_page_generation_i(desc_wr_active_page_generation_i),
          .rd_valid_i(frag_valid_i && frag_ready_o),
          .rd_ready_o(desc_req_ready_w), .rd_owner_i(frag_owner_i),
          .rd_result_valid_o(desc_rsp_valid_w),
          .rd_result_ready_i(desc_rsp_ready_w),
          .rd_owner_o(desc_rsp_owner_w), .rd_logical_o(desc_rsp_logical_w),
          .rd_logical_raw_o(desc_rsp_logical_raw_w),
          .rd_owner_generation_ok_o(desc_gen_ok_w),
          .rd_descriptor_pad_ok_o(desc_pad_ok_w),
          .rd_descriptor_usable_o(desc_usable_w),
          .desc_pad_fault_o(desc_pad_fault_o),
          .rd_generation_mismatch_o(desc_generation_mismatch_o),
          .writes_o(desc_writes_unused), .reads_o(desc_reads_unused),
          .frame_fault_o(desc_frame_fault_o), .idle_o(desc_idle_w));
    end else begin : g_descriptor_pad_mutant
      zhao_texture_early_desc_v2_pad_mutant u_desc (
          .clk(clk), .rst_n(rst_n),
          .frame_fault_clear_i(frame_fault_clear_i),
          .wr_valid_i(desc_wr_valid_i), .wr_slot_i(desc_wr_slot_i),
          .wr_owner_generation_i(desc_wr_owner_generation_i),
          .wr_aux_context_i(desc_wr_aux_context_i),
          .wr_lod_q4_4_i(desc_wr_lod_q4_4_i),
          .wr_response_class_i(desc_wr_response_class_i),
          .wr_aux_required_i(desc_wr_aux_required_i),
          .wr_sample_count_i(desc_wr_sample_count_i),
          .wr_palette_slot_i(desc_wr_palette_slot_i),
          .wr_palette_generation_i(desc_wr_palette_generation_i),
          .wr_mosaic_material_a_i(desc_wr_mosaic_material_a_i),
          .wr_mosaic_material_b_i(desc_wr_mosaic_material_b_i),
          .wr_mosaic_weight_i(desc_wr_mosaic_weight_i),
          .wr_binding_selector_i(desc_wr_binding_selector_i),
          .wr_active_page_generation_i(desc_wr_active_page_generation_i),
          .rd_valid_i(frag_valid_i && frag_ready_o),
          .rd_ready_o(desc_req_ready_w), .rd_owner_i(frag_owner_i),
          .rd_result_valid_o(desc_rsp_valid_w),
          .rd_result_ready_i(desc_rsp_ready_w),
          .rd_owner_o(desc_rsp_owner_w), .rd_logical_o(desc_rsp_logical_w),
          .rd_logical_raw_o(desc_rsp_logical_raw_w),
          .rd_owner_generation_ok_o(desc_gen_ok_w),
          .rd_descriptor_pad_ok_o(desc_pad_ok_w),
          .rd_descriptor_usable_o(desc_usable_w),
          .desc_pad_fault_o(desc_pad_fault_o),
          .rd_generation_mismatch_o(desc_generation_mismatch_o),
          .writes_o(desc_writes_unused), .reads_o(desc_reads_unused),
          .frame_fault_o(desc_frame_fault_o), .idle_o(desc_idle_w));
    end
  endgenerate

  zhao_texture_frag_expand_v2 u_expand (
      .clk(clk), .rst_n(rst_n),
      .frame_fault_clear_i(frame_fault_clear_i),
      .frag_valid_i(desc_rsp_valid_w && side_v_q),
      .frag_ready_o(expand_ready_w), .frag_owner_i(desc_rsp_owner_w),
      .frag_logical_descriptor_i(desc_rsp_logical_w),
      .frag_u_i(side_u_q), .frag_v_i(side_v_coord_q),
      .frag_required_mask_i(side_required_q),
      .frag_material_refused_i(side_material_refused_q),
      .sample_valid_o(sample_valid_w), .sample_ready_i(sample_ready_w),
      .sample_handle_o(sample_handle_w),
      .sample_page_generation_o(sample_page_generation_w),
      .sample_selector_overflow_o(sample_selector_overflow_w),
      .sample_force_refuse_o(sample_force_refuse_w),
      .sample_binding_selector_o(sample_binding_selector_w),
      .sample_u_o(sample_u_w), .sample_v_o(sample_v_w),
      .sample_lod_q4_4_o(sample_lod_w),
      .sample0_class_witness_o(sample_class_witness_w),
      .sample0_palette_slot_witness_o(sample_palette_slot_witness_w),
      .sample0_palette_generation_witness_o(
          sample_palette_generation_witness_w),
      .mosaic_valid_o(mosaic_valid_w), .mosaic_ready_i(mosaic_req_ready_w),
      .mosaic_owner_o(mosaic_owner_w),
      .mosaic_u_o(mosaic_u_w), .mosaic_v_o(mosaic_v_w),
      .mosaic_material_a_o(mosaic_a_w),
      .mosaic_material_b_o(mosaic_b_w),
      .mosaic_weight_o(mosaic_weight_w),
      .aux_valid_o(aux_valid_w), .aux_ready_i(1'b1),
      .aux_owner_o(aux_owner_unused), .aux_context_o(aux_context_unused),
      .aux_force_refuse_o(aux_force_unused),
      .iss_aux_valid_o(aux_issue_w),
      .iss_aux_owner_o(aux_issue_owner_unused),
      .fragments_accepted_o(expand_fragments_o),
      .sample_jobs_accepted_o(expand_samples_o),
      .mosaic_jobs_accepted_o(expand_mosaics_o),
      .aux_jobs_accepted_o(expand_aux_unused),
      .zero_sample_fragments_o(expand_zero_unused),
      .malformed_descriptors_o(expand_malformed_o),
      .wq_overflow_o(expand_overflow_unused),
      .frame_fault_o(expand_frame_fault_o), .idle_o(expand_idle_w));

  wire data_quiet_c = desc_idle_w && !side_v_q && expand_idle_w &&
                      resolver_data_idle_w;

  // ---------------------------------------------------------------------------
  // THE MOSAIC PICK AND ITS HOLD, composed exactly as `zhao_texture_island_v3_top`
  // composes them. Both are PRODUCTION blocks: this bench adds no join logic of
  // its own, so what it proves is what ships rather than a copy of it.
  zhao_texture_mosaic_v2 u_mosaic (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(mosaic_valid_w), .req_ready_o(mosaic_req_ready_w),
      .req_u_i(mosaic_u_w), .req_v_i(mosaic_v_w),
      .req_mat_a_i(mosaic_a_w), .req_mat_b_i(mosaic_b_w),
      .req_weight_i(mosaic_weight_w), .req_mosaic_i(1'b1),
      .req_src_id_i({2'b00, mosaic_owner_w}),
      .pick_valid_o(mosaic_rsp_valid_w), .pick_ready_i(1'b1),
      .pick_tile_o(mosaic_tile_w), .pick_tx_o(mosaic_tx_w),
      .pick_ty_o(mosaic_ty_w), .pick_src_id_o(mosaic_src_w),
      .idle_o(mosaic_idle_w),
      .texture_samples_o(mosaic_picks_delivered_o));

  zhao_texture_mosaic_hold #(.SLOTW(6), .GENW(8)) u_mosaic_hold (
      .clk(clk), .rst_n(rst_n),
      .pick_valid_i(mosaic_rsp_valid_w),
      .pick_src_id_i(mosaic_src_w[13:0]),
      .pick_tile_i(mosaic_tile_w),
      .smp_handle_i(sample_handle_w),
      .pick_ready_o(mosaic_pick_ready_o),
      .pick_tile_o(mosaic_pick_tile_o),
      .picks_held_o(mosaic_picks_held_o),
      .stale_slot_holds_o(mosaic_stale_slot_holds_o));

  logic resolver_req_ready_w;
  assign sample_ready_w = resolver_req_ready_w && mosaic_pick_ready_o;

  zhao_texture_binding_resolver_v2 u_resolver (
      .clk(clk), .rst_n(rst_n),
      .frame_fault_clear_i(frame_fault_clear_i),
      .cfg_valid_i(cfg_valid_i), .cfg_ready_o(cfg_ready_o),
      .cfg_op_i(cfg_op_i), .cfg_page_generation_i(cfg_page_generation_i),
      .cfg_selector_i(cfg_selector_i), .cfg_row_i(cfg_row_i),
      .cfg_crc32_i(cfg_crc32_i),
      .cfg_rsp_valid_o(cfg_rsp_valid_o),
      .cfg_rsp_ready_i(cfg_rsp_ready_i), .cfg_rsp_op_o(cfg_rsp_op_o),
      .cfg_rsp_status_o(cfg_rsp_status_o),
      .cfg_rsp_page_generation_o(cfg_rsp_page_generation_o),
      .data_quiet_i(data_quiet_c),
      .admission_enable_o(resolver_admission_enable_w),
      .active_page_generation_o(active_page_generation_o),
      .cfg_loader_idle_o(resolver_cfg_idle_unused),
      .binding_crc_busy_o(resolver_crc_busy_unused),
      .binding_seal_pending_o(resolver_seal_pending_unused),
      .req_valid_i(sample_valid_w && mosaic_pick_ready_o),
      .req_ready_o(resolver_req_ready_w),
      .req_sample_handle_i(sample_handle_w),
      .req_page_generation_i(sample_page_generation_w),
      .req_selector_overflow_i(sample_selector_overflow_w),
      .req_force_refuse_i(sample_force_refuse_w),
      .req_binding_selector_i(sample_binding_selector_w),
      .req_mosaic_tile_i(mosaic_pick_tile_o),
      .req_u_i(sample_u_w), .req_v_i(sample_v_w),
      .req_lod_q4_4_i(sample_lod_w),
      .req_sample0_class_witness_i(sample_class_witness_w),
      .req_sample0_palette_slot_witness_i(sample_palette_slot_witness_w),
      .req_sample0_palette_generation_witness_i(
          sample_palette_generation_witness_w),
      .iss_tmu_valid_o(issue_valid_o), .iss_tmu_handle_o(issue_handle_o),
      .plan_valid_o(plan_valid_o), .plan_ready_i(plan_ready_i),
      .plan_route_token_o(plan_route_token_o), .plan_base_o(plan_base_o),
      .plan_mode_o(plan_mode_o), .plan_palette_slot_o(plan_palette_slot_o),
      .plan_palette_generation_o(plan_palette_generation_o),
      .plan_u_o(plan_u_o), .plan_v_o(plan_v_o),
      .plan_lod_q4_4_o(plan_lod_q4_4_o),
      .refuse_valid_o(refuse_valid_o), .refuse_ready_i(refuse_ready_i),
      .refuse_sample_handle_o(refuse_handle_o),
      .refuse_result_o(refuse_result_o),
      .sample_jobs_accepted_o(resolver_samples_o),
      .planner_jobs_accepted_o(resolver_plans_o),
      .local_refused_o(resolver_refusals_o),
      .selector_overflow_count_o(resolver_overflow_o),
      .page_generation_mismatch_o(resolver_page_mismatch_o),
      .invalid_row_o(resolver_invalid_unused),
      .witness_mismatch_o(resolver_witness_unused),
      .forced_refused_o(resolver_forced_unused),
      .tileset_samples_o(resolver_tileset_samples_o),
      .cfg_errors_o(resolver_cfg_errors_unused),
      .binding_fault_o(resolver_frame_fault_o),
      .data_idle_o(resolver_data_idle_w));

  assign quiet_o = data_quiet_c && mosaic_idle_w && !cfg_rsp_valid_o &&
                   !plan_valid_o && !refuse_valid_o && !frag_valid_i &&
                   !cfg_valid_i;

  initial begin : p_profile
    if ($bits(desc_rsp_logical_w) != 287)
      $fatal(1, "composed descriptor width changed");
  end
endmodule : tb_texture_desc_expand_bind_v2_composed

`default_nettype wire
