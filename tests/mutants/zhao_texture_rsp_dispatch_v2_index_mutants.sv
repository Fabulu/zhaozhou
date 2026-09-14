// zhao_texture_rsp_dispatch_v2_index_mutants.sv
//
// TWO DELIBERATELY BROKEN, RENAMED wrappers.  NOT SHIPPED.
//
// The collector's final output must preserve the class processor's exact
// {route18,status8,raw_index8,alpha8,RGB24} record.  These controls independently
// reproduce the two tempting post-palette faults while leaving arbitration,
// identity, status, alpha, RGB, ready/valid timing, and counters untouched:
//
//   DROP:        raw_index = 0
//   RECONSTRUCT: raw_index = RGB[7:0]
//
// Each wrapper contains one substantive mutation and depends on the real module,
// so it cannot become a stale copied dispatcher.  The shared inverse-polarity
// driver passes only when its independent expected tuple observes the corruption.
`default_nettype none

module zhao_texture_rsp_dispatch_v2_drop_index_mutant #(
    parameter int unsigned ROUTEW = 18
) (
    input var logic clk,
    input var logic rst_n,
    input  var logic clut_valid_i,
    output var logic clut_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] clut_tuple_i,
    input  var logic near_valid_i,
    output var logic near_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] near_tuple_i,
    input  var logic bil_valid_i,
    output var logic bil_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] bil_tuple_i,
    input  var logic err_valid_i,
    output var logic err_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] err_tuple_i,
    output var logic out_valid_o,
    input  var logic out_ready_i,
    output var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] out_tuple_o,
    output var logic [3:0] pending_valid_o,
    output var logic idle_o,
    output var logic [31:0] accepted_o,
    output var logic [31:0] emitted_o,
    output var logic [31:0] class_mismatch_o
);
  import zhao_render_texture_pkg::*;
  localparam int unsigned TUPLEW = ROUTEW + TEXTURE_RESULT_W;
  logic [TUPLEW-1:0] real_tuple_w;

  zhao_texture_rsp_dispatch_v2 #(.ROUTEW(ROUTEW)) u_real (
      .clk(clk), .rst_n(rst_n),
      .clut_valid_i(clut_valid_i), .clut_ready_o(clut_ready_o), .clut_tuple_i(clut_tuple_i),
      .near_valid_i(near_valid_i), .near_ready_o(near_ready_o), .near_tuple_i(near_tuple_i),
      .bil_valid_i(bil_valid_i), .bil_ready_o(bil_ready_o), .bil_tuple_i(bil_tuple_i),
      .err_valid_i(err_valid_i), .err_ready_o(err_ready_o), .err_tuple_i(err_tuple_i),
      .out_valid_o(out_valid_o), .out_ready_i(out_ready_i), .out_tuple_o(real_tuple_w),
      .pending_valid_o(pending_valid_o), .idle_o(idle_o),
      .accepted_o(accepted_o), .emitted_o(emitted_o),
      .class_mismatch_o(class_mismatch_o));

  always_comb begin
    out_tuple_o = real_tuple_w;
    out_tuple_o[TEXTURE_RESULT_SAMPLE0_INDEX_HI:TEXTURE_RESULT_SAMPLE0_INDEX_LO] = 8'h00; // MUTANT
  end
endmodule : zhao_texture_rsp_dispatch_v2_drop_index_mutant

module zhao_texture_rsp_dispatch_v2_reconstruct_index_mutant #(
    parameter int unsigned ROUTEW = 18
) (
    input var logic clk,
    input var logic rst_n,
    input  var logic clut_valid_i,
    output var logic clut_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] clut_tuple_i,
    input  var logic near_valid_i,
    output var logic near_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] near_tuple_i,
    input  var logic bil_valid_i,
    output var logic bil_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] bil_tuple_i,
    input  var logic err_valid_i,
    output var logic err_ready_o,
    input  var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] err_tuple_i,
    output var logic out_valid_o,
    input  var logic out_ready_i,
    output var logic [ROUTEW + zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] out_tuple_o,
    output var logic [3:0] pending_valid_o,
    output var logic idle_o,
    output var logic [31:0] accepted_o,
    output var logic [31:0] emitted_o,
    output var logic [31:0] class_mismatch_o
);
  import zhao_render_texture_pkg::*;
  localparam int unsigned TUPLEW = ROUTEW + TEXTURE_RESULT_W;
  logic [TUPLEW-1:0] real_tuple_w;

  zhao_texture_rsp_dispatch_v2 #(.ROUTEW(ROUTEW)) u_real (
      .clk(clk), .rst_n(rst_n),
      .clut_valid_i(clut_valid_i), .clut_ready_o(clut_ready_o), .clut_tuple_i(clut_tuple_i),
      .near_valid_i(near_valid_i), .near_ready_o(near_ready_o), .near_tuple_i(near_tuple_i),
      .bil_valid_i(bil_valid_i), .bil_ready_o(bil_ready_o), .bil_tuple_i(bil_tuple_i),
      .err_valid_i(err_valid_i), .err_ready_o(err_ready_o), .err_tuple_i(err_tuple_i),
      .out_valid_o(out_valid_o), .out_ready_i(out_ready_i), .out_tuple_o(real_tuple_w),
      .pending_valid_o(pending_valid_o), .idle_o(idle_o),
      .accepted_o(accepted_o), .emitted_o(emitted_o),
      .class_mismatch_o(class_mismatch_o));

  always_comb begin
    out_tuple_o = real_tuple_w;
    out_tuple_o[TEXTURE_RESULT_SAMPLE0_INDEX_HI:TEXTURE_RESULT_SAMPLE0_INDEX_LO] =
        real_tuple_w[TEXTURE_RESULT_RGB_LO +: 8]; // MUTANT
  end
endmodule : zhao_texture_rsp_dispatch_v2_reconstruct_index_mutant

`default_nettype wire
