// COMMITTED TEST MUTANT -- valid-withdrawal positive control.
//
// This renamed wrapper contains the golden composed descriptor chain and changes
// only its externally observed request handshake: after a valid request is seen
// stalled, valid is withdrawn until ready returns. The inner golden chain still
// consumes on that ready edge. The inverse-polarity control must report the
// persistence violation; wildcard source lists cannot replace the golden top
// because this module has a distinct name.
`default_nettype none

module tb_desc_join_expand_valid_withdraw_mutant #(
    parameter int unsigned TAGW  = 14,
    parameter int unsigned SLOTW = 6,
    parameter int unsigned GENW  = 8,
    parameter int unsigned CTXW  = 64
) (
    input var logic clk,
    input var logic rst_n,
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
    input  var logic               p_valid_i,
    output var logic               p_ready_o,
    input  var logic signed [31:0] p_u_i,
    input  var logic signed [31:0] p_v_i,
    input  var logic [TAGW-1:0]    p_tag_i,
    input  var logic               p_sat_i,
    input  var logic               p_dz_i,
    output var logic               req_valid_o,
    input  var logic               req_ready_i,
    output var logic signed [31:0] req_u_o,
    output var logic signed [31:0] req_v_o,
    output var logic [7:0]         req_lod_o,
    output var logic [17:0]        req_src_id_o,
    output var logic [1:0]         req_pal_slot_o,
    output var logic [7:0]         req_pal_gen_o,
    output var logic            aux_valid_o,
    input  var logic            aux_ready_i,
    output var logic [13:0]     aux_owner_o,
    output var logic [CTXW-1:0] aux_ctx_o,
    output var logic       m_valid_o,
    input  var logic       m_ready_i,
    output var logic [7:0] m_mat_a_o,
    output var logic [7:0] m_mat_b_o,
    output var logic [7:0] m_weight_o,
    output var logic               f_valid_o,
    output var logic               f_ready_o,
    output var logic [13:0]        f_owner_o,
    output var logic signed [31:0] f_u_o,
    output var logic signed [31:0] f_v_o,
    output var logic [CTXW-1:0]    f_ctx_o,
    output var logic               f_sat_o,
    output var logic               f_depth_zero_o,
    output var logic [7:0]      f_binding_o,
    output var logic [7:0]      f_lod_o,
    output var logic [1:0]      f_count_o,
    output var logic            f_aux_o,
    output var logic [1:0]      f_class_o,
    output var logic [1:0]      f_pal_slot_o,
    output var logic [GENW-1:0] f_pal_gen_o,
    output var logic [31:0] joined_o,
    output var logic [31:0] desc_reads_o,
    output var logic [31:0] exp_requests_o,
    output var logic [31:0] exp_fragments_o,
    output var logic [31:0] exp_zero_o,
    output var logic [31:0] exp_overflow_o,
    output var logic [31:0] gen_mismatch_o
);
  logic req_valid_raw;
  logic withdraw_q;

  tb_desc_join_expand #(
      .TAGW(TAGW), .SLOTW(SLOTW), .GENW(GENW), .CTXW(CTXW)
  ) u_golden (
      .req_valid_o(req_valid_raw),
      .*
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)
      withdraw_q <= 1'b0;
    else if (withdraw_q && req_ready_i)
      withdraw_q <= 1'b0;
    else if (req_valid_raw && !req_ready_i)
      withdraw_q <= 1'b1;
  end

  assign req_valid_o = req_valid_raw && !withdraw_q;
endmodule

`default_nettype wire
