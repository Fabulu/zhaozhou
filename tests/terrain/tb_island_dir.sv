// tb_island_dir.sv - island directory ports flattened for the C++ side, and a
// modelled residency store.
//
// The store is modelled HERE rather than instantiating zhao_terrain_residency_v2
// because this test is about the OUTCOME MAPPING, not about the set-associative
// directory underneath -- that block has its own differential and is
// UNIT_VERIFIED. Composing the two is a separate step with a separate question.
//
// The model answers from a bitmap the C++ side loads, so the test controls
// exactly which patches are ground and can therefore compute the oracle's
// answer for every query independently.
module tb_island_dir
  import zhao_pkg::*;
(
    input  logic        clk,
    input  logic        rst_n,

    input  logic [15:0] desc_extent_ix,
    input  logic [15:0] desc_extent_iz,
    input  logic [7:0]  desc_pitch_log2,

    input  logic        q_valid,
    output logic        q_ready,
    input  logic [31:0] q_ix,
    input  logic [31:0] q_iz,
    input  logic [7:0]  q_tag,

    // the modelled store: the C++ side writes ground patches in before querying
    input  logic        gw_en,
    input  logic [13:0] gw_addr,
    input  logic [31:0] gw_handle,

    output logic        a_valid,
    input  logic        a_ready,
    output logic [1:0]  a_outcome,
    output logic [31:0] a_handle,
    output logic [7:0]  a_tag,

    output logic [31:0] cnt_resident,
    output logic [31:0] cnt_open_sky,
    output logic [31:0] cnt_out_of_extent,
    output logic [31:0] cnt_bad_pitch
);

  // ---- the modelled residency store --------------------------------------
  // 128 x 128 addressing so the row stride is a shift, not a multiply. The
  // real store is set-associative over a wider key; what matters for THIS
  // test is that it answers hit/miss and a handle, in order, one at a time.
  logic [31:0] ground_m [16384];
  logic        ground_v [16384];

  logic        res_valid, res_ready;
  logic [15:0] res_ix, res_iz;
  logic        res_ans_valid, res_ans_hit;
  logic [31:0] res_ans_handle;

  wire [13:0] res_addr = {res_iz[6:0], res_ix[6:0]};

  // One-cycle store, always ready. Backpressure on the store is a different
  // question and belongs to the residency block's own test.
  assign res_ready = 1'b1;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      res_ans_valid  <= 1'b0;
      res_ans_hit    <= 1'b0;
      res_ans_handle <= 32'd0;
    end else begin
      res_ans_valid <= 1'b0;
      if (gw_en) begin
        ground_m[gw_addr] <= gw_handle;
        ground_v[gw_addr] <= 1'b1;
      end
      if (res_valid && res_ready) begin
        res_ans_valid  <= 1'b1;
        res_ans_hit    <= ground_v[res_addr];
        res_ans_handle <= ground_m[res_addr];
      end
    end
  end

  zhao_terrain_island_dir u_dut (
      .clk(clk), .rst_n(rst_n),
      .desc_extent_ix_i(desc_extent_ix),
      .desc_extent_iz_i(desc_extent_iz),
      .desc_pitch_log2_i($signed(desc_pitch_log2)),
      .q_valid_i(q_valid), .q_ready_o(q_ready),
      .q_ix_i($signed(q_ix)), .q_iz_i($signed(q_iz)), .q_tag_i(q_tag),
      .res_valid_o(res_valid), .res_ready_i(res_ready),
      .res_ix_o(res_ix), .res_iz_o(res_iz),
      .res_ans_valid_i(res_ans_valid), .res_ans_hit_i(res_ans_hit),
      .res_ans_handle_i(res_ans_handle),
      .a_valid_o(a_valid), .a_ready_i(a_ready),
      .a_outcome_o(a_outcome), .a_handle_o(a_handle), .a_tag_o(a_tag),
      .cnt_resident_o(cnt_resident), .cnt_open_sky_o(cnt_open_sky),
      .cnt_out_of_extent_o(cnt_out_of_extent), .cnt_bad_pitch_o(cnt_bad_pitch)
  );

endmodule
