// tb_desc_join_expand.sv — the whole descriptor chain, with the REAL expander.
//
// early descriptor bank -> UV join -> zhao_texture_frag_expand
//
// WHY THIS EXISTS BEFORE THE ISLAND EDIT
// --------------------------------------
// Packet 2 replaces the island's scattered combinational post-PERSPUV reads with
// the bank and the join. That edit touches `zhao_texture_island_v3_top.sv`, and
// when it lands the first thing to run is the composed suite. If that suite goes
// red, the useful question is "is it the wiring or the modules?" — and with no
// composed test of the modules themselves, the answer costs a bisect.
//
// So the chain is composed here, against the ACTUAL expander rather than a
// model of it, while the island file is still untouched. Anything red after the
// island edit is then the wiring, by elimination.
//
// The expander is the right third element and not an arbitrary one: it is the
// consumer §5.3 names, the one that must be fed "this descriptor's values, not
// live ingress values and not another stage's current owner". Its request
// SEQUENCE is the observable that proves it.
module tb_desc_join_expand #(
    parameter int unsigned TAGW  = 14,
    parameter int unsigned SLOTW = 6,
    parameter int unsigned GENW  = 8,
    parameter int unsigned CTXW  = 64
) (
    input var logic clk,
    input var logic rst_n,

    // ---- descriptor write: the owner admission handshake --------------------
    input  var logic             wr_valid_i,
    input  var logic [SLOTW-1:0] wr_slot_i,
    input  var logic [GENW-1:0]  wr_owner_gen_i,
    input  var logic [63:0]      wr_aux_context_i,
    input  var logic [7:0]       wr_lod_i,
    input  var logic [1:0]       wr_raw_class_i,
    input  var logic             wr_needs_aux_i,
    input  var logic [1:0]       wr_sample_count_i,
    input  var logic [1:0]       wr_palette_slot_i,
    input  var logic [7:0]       wr_palette_gen_i,
    input  var logic [7:0]       wr_mosaic_mat_a_i,
    input  var logic [7:0]       wr_mosaic_mat_b_i,
    input  var logic [7:0]       wr_mosaic_weight_i,
    input  var logic [7:0]       wr_binding_sel_i,

    // ---- PERSPUV result in ---------------------------------------------------
    input  var logic               p_valid_i,
    output var logic               p_ready_o,
    input  var logic signed [31:0] p_u_i,
    input  var logic signed [31:0] p_v_i,
    input  var logic [TAGW-1:0]    p_tag_i,
    input  var logic               p_sat_i,
    input  var logic               p_dz_i,

    // ---- TMU requests out (the expander's observable) ------------------------
    output var logic               req_valid_o,
    input  var logic               req_ready_i,
    output var logic signed [31:0] req_u_o,
    output var logic signed [31:0] req_v_o,
    output var logic [7:0]         req_lod_o,
    output var logic [17:0]        req_src_id_o,
    // PACKET 3: the palette pair as the EXPANDER emits it. Checking it here is
    // what makes the carriage claim end-to-end -- descriptor to join to expander
    // to request -- rather than a claim about the join alone.
    output var logic [1:0]         req_pal_slot_o,
    output var logic [7:0]         req_pal_gen_o,

    // ---- AUX out --------------------------------------------------------------
    output var logic            aux_valid_o,
    input  var logic            aux_ready_i,
    output var logic [13:0]     aux_owner_o,
    output var logic [CTXW-1:0] aux_ctx_o,

    // ---- Mosaic branch --------------------------------------------------------
    output var logic       m_valid_o,
    input  var logic       m_ready_i,
    output var logic [7:0] m_mat_a_o,
    output var logic [7:0] m_mat_b_o,
    output var logic [7:0] m_weight_o,

    // ---- what the join hands the expander, observable for checking -----------
    output var logic [7:0]      f_binding_o,
    output var logic [7:0]      f_lod_o,
    output var logic [1:0]      f_count_o,
    output var logic            f_aux_o,
    output var logic [1:0]      f_class_o,
    output var logic [1:0]      f_pal_slot_o,
    output var logic [GENW-1:0] f_pal_gen_o,

    // ---- instruments ----------------------------------------------------------
    output var logic [31:0] joined_o,
    output var logic [31:0] desc_reads_o,
    output var logic [31:0] exp_requests_o,
    output var logic [31:0] exp_fragments_o,
    output var logic [31:0] exp_zero_o,
    output var logic [31:0] exp_overflow_o,
    output var logic [31:0] gen_mismatch_o
);

  // ---- bank outputs --------------------------------------------------------
  logic             d_rd_valid_c;
  logic [SLOTW-1:0] d_rd_slot_c;
  logic [GENW-1:0]  d_rd_gen_c, d_ogen_c, d_pgen_c;
  logic [63:0]      d_ctx_c;
  logic [7:0]       d_lod_c, d_mosa_c, d_mosb_c, d_mosw_c, d_bsel_c;
  logic [1:0]       d_class_c, d_count_c, d_pslot_c;
  logic             d_aux_c, d_rvalid_c;

  zhao_texture_early_desc #(.SLOTW(SLOTW), .GENW(GENW)) u_desc (
      .clk(clk), .rst_n(rst_n),
      .wr_valid_i(wr_valid_i), .wr_slot_i(wr_slot_i),
      .wr_owner_gen_i(wr_owner_gen_i),
      .wr_aux_context_i(wr_aux_context_i), .wr_lod_q4_4_i(wr_lod_i),
      .wr_raw_class_i(wr_raw_class_i), .wr_needs_aux_i(wr_needs_aux_i),
      .wr_sample_count_i(wr_sample_count_i),
      .wr_palette_slot_i(wr_palette_slot_i), .wr_palette_gen_i(wr_palette_gen_i),
      .wr_mosaic_mat_a_i(wr_mosaic_mat_a_i), .wr_mosaic_mat_b_i(wr_mosaic_mat_b_i),
      .wr_mosaic_weight_i(wr_mosaic_weight_i), .wr_binding_sel_i(wr_binding_sel_i),
      .rd_valid_i(d_rd_valid_c), .rd_slot_i(d_rd_slot_c),
      .rd_owner_gen_i(d_rd_gen_c),
      .rd_result_valid_o(d_rvalid_c), .rd_owner_gen_o(d_ogen_c),
      .rd_aux_context_o(d_ctx_c), .rd_lod_q4_4_o(d_lod_c),
      .rd_raw_class_o(d_class_c), .rd_needs_aux_o(d_aux_c),
      .rd_sample_count_o(d_count_c), .rd_palette_slot_o(d_pslot_c),
      .rd_palette_gen_o(d_pgen_c), .rd_mosaic_mat_a_o(d_mosa_c),
      .rd_mosaic_mat_b_o(d_mosb_c), .rd_mosaic_weight_o(d_mosw_c),
      .rd_binding_sel_o(d_bsel_c),
      .writes_o(), .reads_o(desc_reads_o), .rd_gen_mismatch_o());

  // ---- join outputs --------------------------------------------------------
  logic               j_f_valid_c, j_f_ready_c;
  logic [13:0]        j_owner_c;
  logic signed [31:0] j_u_c, j_v_c;
  logic [CTXW-1:0]    j_ctx_c;
  logic               j_sat_c, j_dz_c;

  zhao_texture_uv_join #(
      .TAGW(TAGW), .SLOTW(SLOTW), .GENW(GENW), .CTXW(CTXW)
  ) u_join (
      .clk(clk), .rst_n(rst_n),
      .p_valid_i(p_valid_i), .p_ready_o(p_ready_o),
      .p_u_i(p_u_i), .p_v_i(p_v_i), .p_tag_i(p_tag_i),
      .p_sat_i(p_sat_i), .p_dz_i(p_dz_i),
      .d_rd_valid_o(d_rd_valid_c), .d_rd_slot_o(d_rd_slot_c),
      .d_rd_owner_gen_o(d_rd_gen_c),
      .d_aux_context_i(d_ctx_c), .d_lod_i(d_lod_c),
      .d_raw_class_i(d_class_c), .d_needs_aux_i(d_aux_c),
      .d_sample_count_i(d_count_c), .d_binding_sel_i(d_bsel_c),
      .d_mosaic_mat_a_i(d_mosa_c), .d_mosaic_mat_b_i(d_mosb_c),
      .d_mosaic_weight_i(d_mosw_c), .d_owner_gen_i(d_ogen_c),
      .d_palette_slot_i(d_pslot_c), .d_palette_gen_i(d_pgen_c),
      .f_valid_o(j_f_valid_c), .f_ready_i(j_f_ready_c),
      .f_owner_o(j_owner_c), .f_u_o(j_u_c), .f_v_o(j_v_c),
      .f_binding_o(f_binding_o), .f_lod_o(f_lod_o), .f_count_o(f_count_o),
      .f_aux_o(f_aux_o), .f_class_o(f_class_o), .f_ctx_o(j_ctx_c),
      .f_sat_o(j_sat_c), .f_depth_zero_o(j_dz_c),
      .f_pal_slot_o(f_pal_slot_o), .f_pal_gen_o(f_pal_gen_o),
      .m_valid_o(m_valid_o), .m_ready_i(m_ready_i),
      .m_mat_a_o(m_mat_a_o), .m_mat_b_o(m_mat_b_o), .m_weight_o(m_weight_o),
      .joined_o(joined_o), .saturated_o(), .depth_zero_o(),
      .gen_mismatch_o(gen_mismatch_o));

  // The class sanitisation lives at the READ POINT, applied to the captured
  // class -- the island does this and the join's header says the rule belongs in
  // one place. CLS_ERR (2'd3) becomes CLS_NEAR (2'd1).
  wire [1:0] f_class_sane_c = (f_class_o == 2'd3) ? 2'd1 : f_class_o;

  zhao_texture_frag_expand #(.CTXW(CTXW)) u_expand (
      .clk(clk), .rst_n(rst_n),
      .f_valid_i(j_f_valid_c), .f_ready_o(j_f_ready_c),
      .f_owner_i(j_owner_c), .f_u_i(j_u_c), .f_v_i(j_v_c),
      .f_binding_i(f_binding_o), .f_lod_i(f_lod_o), .f_count_i(f_count_o),
      .f_pal_slot_i(f_pal_slot_o), .f_pal_gen_i(f_pal_gen_o),
      .f_aux_i(f_aux_o), .f_class_i(f_class_sane_c), .f_ctx_i(j_ctx_c),
      .req_valid_o(req_valid_o), .req_ready_i(req_ready_i),
      .req_u_o(req_u_o), .req_v_o(req_v_o), .req_lod_o(req_lod_o),
      .req_src_id_o(req_src_id_o),
      .req_pal_slot_o(req_pal_slot_o), .req_pal_gen_o(req_pal_gen_o),
      .aux_valid_o(aux_valid_o), .aux_ready_i(aux_ready_i),
      .aux_owner_o(aux_owner_o), .aux_ctx_o(aux_ctx_o),
      .iss_tmu_valid_o(), .iss_tmu_handle_o(),
      .iss_aux_valid_o(), .iss_aux_owner_o(), .aux_requests_o(),
      .requests_o(exp_requests_o), .fragments_o(exp_fragments_o),
      .zero_sample_fragments_o(exp_zero_o), .wq_overflow_o(exp_overflow_o));

  wire unused_c = &{1'b0, d_rvalid_c, j_sat_c, j_dz_c};

endmodule
