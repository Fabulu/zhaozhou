// COMMITTED TEST MUTANT -- substream-drop positive control.
//
// This renamed wrapper contains the golden descriptor chain. On the first AUX
// beat that appears with ready already high, the inner chain handshakes while
// the externally observed valid is suppressed for that edge. No previously
// stalled valid is withdrawn: the independent queue/count conservation checks,
// rather than the persistence detector, must catch the missing substream beat.
`default_nettype none

module tb_desc_join_expand_substream_drop_mutant #(
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
  logic aux_valid_raw;
  logic aux_valid_prev_q;
  logic dropped_q;
  logic drop_c;

  tb_desc_join_expand #(
      .TAGW(TAGW), .SLOTW(SLOTW), .GENW(GENW), .CTXW(CTXW)
  ) u_golden (
      .aux_valid_o(aux_valid_raw),
      .*
  );

  assign drop_c = !dropped_q && aux_valid_raw && aux_ready_i &&
                  !aux_valid_prev_q;
  assign aux_valid_o = aux_valid_raw && !drop_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      aux_valid_prev_q <= 1'b0;
      dropped_q        <= 1'b0;
    end else begin
      aux_valid_prev_q <= aux_valid_raw;
      if (drop_c)
        dropped_q <= 1'b1;
    end
  end
endmodule

`default_nettype wire
