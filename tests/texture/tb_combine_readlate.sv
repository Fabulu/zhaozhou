// tb_combine_readlate.sv -- the READ_LATE combiner behind FOUR REAL OWNER
// PLANES, for the resident-read differential.
//
// reports/TEXTURE-READLATE-COMBINE-20260910.md (roadmap 4.2 / Commit4).
//
// WHY A WRAPPER AND NOT THE BARE COMBINER. With READ_LATE=1 the combiner no
// longer receives samples on pins: it names an owner SLOT during its R stage
// and expects the four result planes to answer one edge later, straight out
// of a bank's output register. A bench that drove `src_*_i` from C++ would be
// modelling that memory in software and would prove nothing about the one
// thing this seam changes -- that the data the phase engine consumes is what
// sits at THAT slot in THOSE planes on THAT edge. So the planes are the real
// primitive (`zhao_texture_v3bank`, 64 x 40, the island's own), wired exactly
// as v3own wires them under READ_LATE=1: one reader each, address from the
// combiner, output register straight to the operand port.
//
// THE BENCH PLAYS THE TMU/AUX COMMIT through `pw_*`: one lane per beat, and
// the enable is REGISTERED before it reaches a bank (the V3-WREN-REG law, so
// this wrapper obeys the discipline of the block it stands in for). The driver
// is responsible for the two laws the owner enforces in the island and this
// wrapper cannot:
//   * publication-before-ready -- offer a job only after its planes are
//     written and readable (two edges after the last accepted beat);
//   * release-after-last-reader -- never rewrite a slot whose job is still in
//     flight. The driver asserts both, and watches `src_rd_*` to check every
//     read names an in-flight, published slot.
//
// -DREADLATE_MUTANT swaps in tests/mutants/zhao_texture_material_combine_v2_
// slotswap_mutant.sv, whose slot file is off by one. Same wrapper, same
// planes, same driver: the control passes when the differential FAILS.
`default_nettype none

module tb_combine_readlate #(
    parameter int NCTX = 8,
    parameter int TAGW = 16
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the job, as the island offers it --------------------------------
    input  var logic        f_valid_i,
    output var logic        f_ready_o,
    input  var logic [1:0]  f_sample_count_i,
    input  var logic [2:0]  f_recipe_i,
    input  var logic [7:0]  f_weight_i,
    input  var logic [5:0]  f_slot_i,
    input  var logic        f_has_aux_i,
    input  var logic [23:0] f_base_rgb_i,
    input  var logic [7:0]  f_base_a_i,
    input  var logic [TAGW-1:0] f_tag_i,

    // ---- the plane commit, played by the bench: one lane per beat ----------
    // lane 0..2 = sample planes s0..s2, lane 3 = the AUX plane.
    input  var logic        pw_en_i,
    input  var logic [1:0]  pw_lane_i,
    input  var logic [5:0]  pw_slot_i,
    input  var logic [39:0] pw_data_i,

    // ---- fragment out ------------------------------------------------------
    output var logic        o_valid_o,
    input  var logic        o_ready_i,
    output var logic [23:0] o_rgb_o,
    output var logic [7:0]  o_a_o,
    output var logic [TAGW-1:0] o_tag_o,
    output var logic        o_refused_o,

    // ---- counters, passed through -----------------------------------------
    output var logic [31:0] refused_missing_o,
    output var logic [31:0] saturated_add_o,
    output var logic [31:0] saturated_mul2x_o,
    output var logic [31:0] phases_issued_o,
    output var logic [31:0] jobs_by_recipe_o [8],

    // ---- the seam, observable --------------------------------------------
    output var logic        src_rd_valid_o,
    output var logic [5:0]  src_rd_slot_o
);

  // Registered, one-hot write enables: a flop between the bench and the bank.
  logic [3:0]  pw_we_q;
  logic [5:0]  pw_slot_q;
  logic [39:0] pw_data_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) pw_we_q <= 4'b0000;
    else        pw_we_q <= pw_en_i ? (4'b0001 << pw_lane_i) : 4'b0000;
  end
  always_ff @(posedge clk) begin
    pw_slot_q <= pw_slot_i;
    pw_data_q <= pw_data_i;
  end

  logic [39:0] plane_rd [4];
  logic [5:0]  rd_slot;

  generate
    genvar p;
    for (p = 0; p < 4; p++) begin : g_plane
      // V3-WREN-REG: pw_we_q
      zhao_texture_v3bank #(.WIDTH(40), .DEPTH(64)) u_plane (
          .clk      (clk),
          .wr_en_i  (pw_we_q[p]),
          .wr_addr_i(pw_slot_q),
          .wr_data_i(pw_data_q),
          .rd_addr_i(rd_slot),
          .rd_data_o(plane_rd[p])
      );
    end
  endgenerate

  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] refused_recipe_unused;
  /* verilator lint_on UNUSEDSIGNAL */

`ifdef READLATE_MUTANT
  zhao_texture_material_combine_v2_slotswap_mutant #(
`else
  zhao_texture_material_combine_v2 #(
`endif
      .NCTX(NCTX), .TAGW(TAGW), .READ_LATE(1), .SLOTW(6)
  ) u_dut (
      .clk(clk), .rst_n(rst_n),
      .f_valid_i(f_valid_i), .f_ready_o(f_ready_o),
      .f_sample_count_i(f_sample_count_i), .f_recipe_i(f_recipe_i),
      .f_weight_i(f_weight_i),
      .f_s0_rgb_i(24'd0), .f_s0_a_i(8'd0),
      .f_s1_rgb_i(24'd0), .f_s1_a_i(8'd0),
      .f_s2_rgb_i(24'd0), .f_s2_a_i(8'd0),
      .f_slot_i(f_slot_i), .f_has_aux_i(f_has_aux_i),
      .src_rd_valid_o(src_rd_valid_o), .src_rd_slot_o(rd_slot),
      .src_s0_i(plane_rd[0][31:0]), .src_s1_i(plane_rd[1][31:0]),
      .src_s2_i(plane_rd[2][31:0]), .src_aux_i(plane_rd[3][31:0]),
      .f_base_rgb_i(f_base_rgb_i), .f_base_a_i(f_base_a_i),
      .f_tag_i(f_tag_i),
      .o_valid_o(o_valid_o), .o_ready_i(o_ready_i),
      .o_rgb_o(o_rgb_o), .o_a_o(o_a_o), .o_tag_o(o_tag_o),
      .o_refused_o(o_refused_o),
      .refused_recipe_o(refused_recipe_unused),
      .refused_missing_o(refused_missing_o),
      .saturated_add_o(saturated_add_o),
      .saturated_mul2x_o(saturated_mul2x_o),
      .jobs_by_recipe_o(jobs_by_recipe_o),
      .phases_issued_o(phases_issued_o)
  );

  assign src_rd_slot_o = rd_slot;

endmodule

`default_nettype wire
