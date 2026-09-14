// tb_texture_mosaic_v2_pair.sv — cycle-exact unversioned/V2 lockstep wrapper.
`default_nettype none

module tb_texture_mosaic_v2_pair (
    input  var logic               clk,
    input  var logic               rst_n,
    input  var logic               req_valid_i,
    input  var logic signed [31:0] req_u_i,
    input  var logic signed [31:0] req_v_i,
    input  var logic        [7:0]  req_mat_a_i,
    input  var logic        [7:0]  req_mat_b_i,
    input  var logic        [7:0]  req_weight_i,
    input  var logic               req_mosaic_i,
    input  var logic        [15:0] req_src_id_i,
    input  var logic               pick_ready_i,

    output var logic               legacy_req_ready_o,
    output var logic               legacy_pick_valid_o,
    output var logic        [7:0]  legacy_pick_tile_o,
    output var logic        [5:0]  legacy_pick_tx_o,
    output var logic        [5:0]  legacy_pick_ty_o,
    output var logic        [15:0] legacy_pick_src_id_o,
    output var logic               legacy_idle_o,
    output var logic        [31:0] legacy_samples_o,

    output var logic               v2_req_ready_o,
    output var logic               v2_pick_valid_o,
    output var logic        [7:0]  v2_pick_tile_o,
    output var logic        [5:0]  v2_pick_tx_o,
    output var logic        [5:0]  v2_pick_ty_o,
    output var logic        [15:0] v2_pick_src_id_o,
    output var logic               v2_idle_o,
    output var logic        [31:0] v2_samples_o
);

  zhao_texture_mosaic u_legacy (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(req_valid_i), .req_ready_o(legacy_req_ready_o),
      .req_u_i(req_u_i), .req_v_i(req_v_i),
      .req_mat_a_i(req_mat_a_i), .req_mat_b_i(req_mat_b_i),
      .req_weight_i(req_weight_i), .req_mosaic_i(req_mosaic_i),
      .req_src_id_i(req_src_id_i),
      .pick_valid_o(legacy_pick_valid_o), .pick_ready_i(pick_ready_i),
      .pick_tile_o(legacy_pick_tile_o), .pick_tx_o(legacy_pick_tx_o),
      .pick_ty_o(legacy_pick_ty_o), .pick_src_id_o(legacy_pick_src_id_o),
      .idle_o(legacy_idle_o), .texture_samples_o(legacy_samples_o)
  );

  zhao_texture_mosaic_v2 u_v2 (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(req_valid_i), .req_ready_o(v2_req_ready_o),
      .req_u_i(req_u_i), .req_v_i(req_v_i),
      .req_mat_a_i(req_mat_a_i), .req_mat_b_i(req_mat_b_i),
      .req_weight_i(req_weight_i), .req_mosaic_i(req_mosaic_i),
      .req_src_id_i(req_src_id_i),
      .pick_valid_o(v2_pick_valid_o), .pick_ready_i(pick_ready_i),
      .pick_tile_o(v2_pick_tile_o), .pick_tx_o(v2_pick_tx_o),
      .pick_ty_o(v2_pick_ty_o), .pick_src_id_o(v2_pick_src_id_o),
      .idle_o(v2_idle_o), .texture_samples_o(v2_samples_o)
  );

endmodule : tb_texture_mosaic_v2_pair

`default_nettype wire
