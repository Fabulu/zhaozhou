// tb_perspuv_pairpipe.sv — the frozen PERSPUV service and the paired candidate,
// side by side on identical stimulus.
//
// The two are driven in LOCKSTEP: a fragment is offered to both and accepted
// only when both are ready, so the accepted SEQUENCE is identical by
// construction and the comparison is of results, not of admission policy. That
// is deliberate — a bench that let them accept independently would be comparing
// two different workloads and calling the difference a defect, which is this
// repository's mismatched-comparison law in bench form.
//
// The outputs are drained independently, because their retirement disciplines
// legitimately differ: the service retires in allocation order out of a
// sixteen-entry table, the candidate out of a terminal FIFO. Both must produce
// the same records in the same order; HOW they got there is what the packet is
// allowed to change.
module tb_perspuv_pairpipe #(
    parameter int unsigned NTOK = 16,
    parameter int unsigned TAGW = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- shared stimulus -----------------------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,      // AND of the two
    // The two admission readies, exposed SEPARATELY as well. The AND is what
    // keeps the engines in lockstep, but a capacity measurement taken from it
    // reports whichever engine filled first. The credit-ceiling test needs the
    // candidate's own ready to attribute the ceiling to the candidate --
    // otherwise it measures the service's NTOK and calls it the candidate's CAP.
    output var logic               a_vready_o,
    output var logic               b_vready_o,
    input  var logic signed [31:0] u_over_w_i,
    input  var logic signed [31:0] v_over_w_i,
    input  var logic        [23:0] r_mant_i,
    input  var logic        [ 5:0] r_k_i,
    input  var logic               depth_zero_i,
    input  var logic [TAGW-1:0]    tag_i,

    // ---- A: the frozen service ----------------------------------------------
    output var logic               a_valid_o,
    input  var logic               a_ready_i,
    output var logic signed [31:0] a_u_o,
    output var logic signed [31:0] a_v_o,
    output var logic [TAGW-1:0]    a_tag_o,
    output var logic               a_sat_o,
    output var logic               a_dz_o,
    output var logic [31:0]        a_fragments_o,
    output var logic [31:0]        a_products_o,

    // ---- B: the paired candidate --------------------------------------------
    output var logic               b_valid_o,
    input  var logic               b_ready_i,
    output var logic signed [31:0] b_u_o,
    output var logic signed [31:0] b_v_o,
    output var logic [TAGW-1:0]    b_tag_o,
    output var logic               b_sat_o,
    output var logic               b_dz_o,
    output var logic [31:0]        b_fragments_o,
    output var logic [31:0]        b_products_o,
    output var logic [31:0]        b_zero_products_o,
    output var logic [4:0]         b_occupancy_o
);

  logic a_ready_c, b_ready_c;
  assign v_ready_o  = a_ready_c && b_ready_c;
  assign a_vready_o = a_ready_c;
  assign b_vready_o = b_ready_c;

  // Offered to both only when both can take it, so neither runs ahead.
  wire fire_c = v_valid_i && v_ready_o;

  zhao_raster_perspuv_svc #(.NTOK(NTOK), .TAGW(TAGW)) u_a (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(fire_c), .v_ready_o(a_ready_c),
      .u_over_w_i(u_over_w_i), .v_over_w_i(v_over_w_i),
      .r_mant_i(r_mant_i), .r_k_i(r_k_i),
      .depth_zero_i(depth_zero_i), .tag_i(tag_i),
      .r_valid_o(a_valid_o), .r_ready_i(a_ready_i),
      .u_o(a_u_o), .v_o(a_v_o), .tag_o(a_tag_o),
      .sat_o(a_sat_o), .depth_zero_o(a_dz_o),
      .fragments_o(a_fragments_o), .products_o(a_products_o),
      .occupancy_o()
  );

  zhao_raster_perspuv_pairpipe #(.NTOK(NTOK), .TAGW(TAGW)) u_b (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(fire_c), .v_ready_o(b_ready_c),
      .u_over_w_i(u_over_w_i), .v_over_w_i(v_over_w_i),
      .r_mant_i(r_mant_i), .r_k_i(r_k_i),
      .depth_zero_i(depth_zero_i), .tag_i(tag_i),
      .r_valid_o(b_valid_o), .r_ready_i(b_ready_i),
      .u_o(b_u_o), .v_o(b_v_o), .tag_o(b_tag_o),
      .sat_o(b_sat_o), .depth_zero_o(b_dz_o),
      .fragments_o(b_fragments_o), .products_o(b_products_o),
      .zero_products_o(b_zero_products_o), .occupancy_o(b_occupancy_o)
  );

endmodule
