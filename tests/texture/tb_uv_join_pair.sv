// tb_uv_join_pair.sv — the post-PERSPUV join wired to the REAL descriptor bank.
//
// The join's central claim is a claim about a COMPOSITION: that the descriptor
// travelling with a record belongs to that record, because one enable advances
// the join's registers and the bank's output register together.
//
// A stubbed descriptor source cannot test that. It would let the test author
// decide when the descriptor changes, which is the very thing under test -- and
// a stub that holds when the real bank would not is how a composition passes on
// evidence about something else. So the actual `zhao_texture_early_desc` is
// instantiated here, hold law and all.
module tb_uv_join_pair #(
    parameter int unsigned TAGW  = 14,
    parameter int unsigned SLOTW = 6,
    parameter int unsigned GENW  = 8,
    parameter int unsigned CTXW  = 64
) (
    input var logic clk,
    input var logic rst_n,

    // ---- descriptor preload (the owner admission handshake) -----------------
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

    // ---- PERSPUV side --------------------------------------------------------
    input  var logic               p_valid_i,
    output var logic               p_ready_o,
    input  var logic signed [31:0] p_u_i,
    input  var logic signed [31:0] p_v_i,
    input  var logic [TAGW-1:0]    p_tag_i,
    input  var logic               p_sat_i,
    input  var logic               p_dz_i,

    // ---- expander side -------------------------------------------------------
    output var logic               f_valid_o,
    input  var logic               f_ready_i,
    output var logic [13:0]        f_owner_o,
    output var logic signed [31:0] f_u_o,
    output var logic signed [31:0] f_v_o,
    output var logic [7:0]         f_binding_o,
    output var logic [7:0]         f_lod_o,
    output var logic [1:0]         f_count_o,
    output var logic               f_aux_o,
    output var logic [1:0]         f_class_o,
    output var logic [CTXW-1:0]    f_ctx_o,
    output var logic               f_sat_o,
    output var logic               f_depth_zero_o,
    output var logic [1:0]         f_pal_slot_o,
    output var logic [GENW-1:0]    f_pal_gen_o,

    // ---- Mosaic side ---------------------------------------------------------
    output var logic       m_valid_o,
    input  var logic       m_ready_i,
    output var logic [7:0] m_mat_a_o,
    output var logic [7:0] m_mat_b_o,
    output var logic [7:0] m_weight_o,

    // ---- instruments ---------------------------------------------------------
    output var logic [31:0] joined_o,
    output var logic [31:0] saturated_o,
    output var logic [31:0] depth_zero_o,
    output var logic [31:0] gen_mismatch_o,
    output var logic [31:0] desc_reads_o,
    output var logic [31:0] desc_writes_o
);

  logic               d_rd_valid_c;
  logic [SLOTW-1:0]   d_rd_slot_c;
  logic [GENW-1:0]    d_rd_gen_c;

  logic [63:0] d_ctx_c;
  logic [7:0]  d_lod_c, d_mosa_c, d_mosb_c, d_mosw_c, d_bsel_c;
  logic [1:0]  d_class_c, d_count_c, d_pslot_c;
  logic        d_aux_c, d_rvalid_c;
  logic [7:0]  d_pgen_c;
  logic [GENW-1:0] d_ogen_c;

  zhao_texture_early_desc #(
      .SLOTW(SLOTW), .GENW(GENW)
  ) u_desc (
      .clk(clk), .rst_n(rst_n),
      .wr_valid_i        (wr_valid_i),
      .wr_slot_i         (wr_slot_i),
      .wr_owner_gen_i    (wr_owner_gen_i),
      .wr_aux_context_i  (wr_aux_context_i),
      .wr_lod_q4_4_i     (wr_lod_i),
      .wr_raw_class_i    (wr_raw_class_i),
      .wr_needs_aux_i    (wr_needs_aux_i),
      .wr_sample_count_i (wr_sample_count_i),
      .wr_palette_slot_i (wr_palette_slot_i),
      .wr_palette_gen_i  (wr_palette_gen_i),
      .wr_mosaic_mat_a_i (wr_mosaic_mat_a_i),
      .wr_mosaic_mat_b_i (wr_mosaic_mat_b_i),
      .wr_mosaic_weight_i(wr_mosaic_weight_i),
      .wr_binding_sel_i  (wr_binding_sel_i),

      .rd_valid_i        (d_rd_valid_c),
      .rd_slot_i         (d_rd_slot_c),
      .rd_owner_gen_i    (d_rd_gen_c),

      .rd_result_valid_o (d_rvalid_c),
      .rd_owner_gen_o    (d_ogen_c),
      .rd_aux_context_o  (d_ctx_c),
      .rd_lod_q4_4_o     (d_lod_c),
      .rd_raw_class_o    (d_class_c),
      .rd_needs_aux_o    (d_aux_c),
      .rd_sample_count_o (d_count_c),
      .rd_palette_slot_o (d_pslot_c),
      .rd_palette_gen_o  (d_pgen_c),
      .rd_mosaic_mat_a_o (d_mosa_c),
      .rd_mosaic_mat_b_o (d_mosb_c),
      .rd_mosaic_weight_o(d_mosw_c),
      .rd_binding_sel_o  (d_bsel_c),

      .writes_o          (desc_writes_o),
      .reads_o           (desc_reads_o),
      .rd_gen_mismatch_o ()
  );

  zhao_texture_uv_join #(
      .TAGW(TAGW), .SLOTW(SLOTW), .GENW(GENW), .CTXW(CTXW)
  ) u_join (
      .clk(clk), .rst_n(rst_n),
      .p_valid_i(p_valid_i), .p_ready_o(p_ready_o),
      .p_u_i(p_u_i), .p_v_i(p_v_i), .p_tag_i(p_tag_i),
      .p_sat_i(p_sat_i), .p_dz_i(p_dz_i),

      .d_rd_valid_o     (d_rd_valid_c),
      .d_rd_slot_o      (d_rd_slot_c),
      .d_rd_owner_gen_o (d_rd_gen_c),

      .d_aux_context_i  (d_ctx_c),
      .d_lod_i          (d_lod_c),
      .d_raw_class_i    (d_class_c),
      .d_needs_aux_i    (d_aux_c),
      .d_sample_count_i (d_count_c),
      .d_binding_sel_i  (d_bsel_c),
      .d_mosaic_mat_a_i (d_mosa_c),
      .d_mosaic_mat_b_i (d_mosb_c),
      .d_mosaic_weight_i(d_mosw_c),
      .d_owner_gen_i    (d_ogen_c),
      .d_palette_slot_i (d_pslot_c),
      .d_palette_gen_i  (d_pgen_c),

      .f_valid_o(f_valid_o), .f_ready_i(f_ready_i),
      .f_owner_o(f_owner_o), .f_u_o(f_u_o), .f_v_o(f_v_o),
      .f_binding_o(f_binding_o), .f_lod_o(f_lod_o), .f_count_o(f_count_o),
      .f_aux_o(f_aux_o), .f_class_o(f_class_o), .f_ctx_o(f_ctx_o),
      .f_sat_o(f_sat_o), .f_depth_zero_o(f_depth_zero_o),
      .f_pal_slot_o(f_pal_slot_o), .f_pal_gen_o(f_pal_gen_o),

      .m_valid_o(m_valid_o), .m_ready_i(m_ready_i),
      .m_mat_a_o(m_mat_a_o), .m_mat_b_o(m_mat_b_o), .m_weight_o(m_weight_o),

      .joined_o(joined_o), .saturated_o(saturated_o),
      .depth_zero_o(depth_zero_o), .gen_mismatch_o(gen_mismatch_o)
  );

  // The palette pair is now CARRIED (§6/D3 step 1); only the bank's own read
  // valid remains unconsumed here, because the join's reserved-destination
  // handshake is what gates the record, not the bank's flag.
  wire unused_c = &{1'b0, d_rvalid_c};

endmodule
