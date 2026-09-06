// tb_island_visible.sv - TERRAIN.VISIBLE's ports flattened for the C++ side,
// over the same modelled residency store tb_island_dir.sv uses.
//
// THE STORE IS MODELLED HERE, exactly as in tb_island_dir, and for the same
// reason: this test is about WHICH PATCHES ARE ASKED ABOUT and in what order,
// not about the set-associative directory underneath. That block has its own
// differential and is UNIT_VERIFIED, and composing the three is a separate step
// with a separate question.
//
// The store answers from a bitmap the C++ side loads, so the test controls
// exactly which patches are ground and can compute `zref::island::visible_set`
// independently for every window.
module tb_island_visible
  import zhao_pkg::*;
(
    input  logic        clk,
    input  logic        rst_n,

    input  logic [15:0] desc_extent_ix,
    input  logic [15:0] desc_extent_iz,
    input  logic [7:0]  desc_pitch_log2,

    input  logic        v_valid,
    output logic        v_ready,
    input  logic [31:0] v_centre_ix,
    input  logic [31:0] v_centre_iz,
    input  logic [7:0]  v_radius,

    // the modelled store: the C++ side writes ground patches in before querying
    input  logic        gw_en,
    input  logic [13:0] gw_addr,
    input  logic [31:0] gw_handle,

    output logic        p_valid,
    input  logic        p_ready,
    output logic [31:0] p_ix,
    output logic [31:0] p_iz,
    output logic [31:0] p_handle,

    output logic        v_done,
    output logic        v_busy,

    output logic [31:0] cnt_examined,
    output logic [31:0] cnt_emitted,
    output logic [31:0] cnt_sky,
    output logic [31:0] cnt_out_of_extent,
    output logic [31:0] cnt_bad_pitch,

    output logic [31:0] isl_cnt_resident,
    output logic [31:0] isl_cnt_open_sky,
    output logic [31:0] isl_cnt_out_of_extent,
    output logic [31:0] isl_cnt_bad_pitch,

    output logic        err_tag
);

  // ---- the modelled residency store --------------------------------------
  // 128 x 128 addressing so the row stride is a shift, not a multiply, and
  // byte-for-byte the same model tb_island_dir uses -- two benches disagreeing
  // about the ground would make a composition bug look like a mapping bug.
  logic [31:0] ground_m [16384];
  logic        ground_v [16384];

  logic        res_valid, res_ready;
  logic [15:0] res_ix, res_iz;
  logic        res_ans_valid, res_ans_hit;
  logic [31:0] res_ans_handle;

  wire [13:0] res_addr = {res_iz[6:0], res_ix[6:0]};

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

  zhao_terrain_visible u_dut (
      .clk(clk), .rst_n(rst_n),
      .desc_extent_ix_i(desc_extent_ix),
      .desc_extent_iz_i(desc_extent_iz),
      .desc_pitch_log2_i($signed(desc_pitch_log2)),
      .v_valid_i(v_valid), .v_ready_o(v_ready),
      .v_centre_ix_i($signed(v_centre_ix)), .v_centre_iz_i($signed(v_centre_iz)),
      .v_radius_i(v_radius),
      .res_valid_o(res_valid), .res_ready_i(res_ready),
      .res_ix_o(res_ix), .res_iz_o(res_iz),
      .res_ans_valid_i(res_ans_valid), .res_ans_hit_i(res_ans_hit),
      .res_ans_handle_i(res_ans_handle),
      .p_valid_o(p_valid), .p_ready_i(p_ready),
      .p_ix_o(p_ix), .p_iz_o(p_iz), .p_handle_o(p_handle),
      .v_done_o(v_done), .v_busy_o(v_busy),
      .cnt_examined_o(cnt_examined), .cnt_emitted_o(cnt_emitted),
      .cnt_sky_o(cnt_sky), .cnt_out_of_extent_o(cnt_out_of_extent),
      .cnt_bad_pitch_o(cnt_bad_pitch),
      .isl_cnt_resident_o(isl_cnt_resident), .isl_cnt_open_sky_o(isl_cnt_open_sky),
      .isl_cnt_out_of_extent_o(isl_cnt_out_of_extent),
      .isl_cnt_bad_pitch_o(isl_cnt_bad_pitch),
      .err_tag_o(err_tag)
  );

endmodule
