// tb_terrain_tapshare_mutant.sv -- the flattening bench for the MUTANT copy.
// Identical to tests/terrain/tb_terrain_tapshare.sv except for the module it
// instantiates and its own name. It holds no logic; see the mutant's header
// for what was changed and why.
`default_nettype none

module tb_terrain_tapshare_mutant (
    input  var logic clk,
    input  var logic rst_n,

    // ---- client 0 ----------------------------------------------------------
    input  var logic               r0_valid_i,
    output var logic               r0_ready_o,
    input  var logic signed [31:0] r0_x_i,
    input  var logic signed [31:0] r0_z_i,
    input  var logic               r0_surface_i,
    output var logic               r0_rsp_valid_o,

    // ---- client 1 ----------------------------------------------------------
    input  var logic               r1_valid_i,
    output var logic               r1_ready_o,
    input  var logic signed [31:0] r1_x_i,
    input  var logic signed [31:0] r1_z_i,
    input  var logic               r1_surface_i,
    output var logic               r1_rsp_valid_o,

    // ---- the one service ---------------------------------------------------
    output var logic               t_req_valid_o,
    input  var logic               t_req_ready_i,
    output var logic signed [31:0] t_req_x_o,
    output var logic signed [31:0] t_req_z_o,
    output var logic               t_req_surface_o,
    input  var logic               t_rsp_valid_i,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] grants0_o,
    output var logic [31:0] grants1_o,
    output var logic [31:0] contended_o,
    output var logic [31:0] stray_rsp_o,
    output var logic        busy_o
);

  logic        [1:0] r_valid_c;
  logic        [1:0] r_ready_c;
  logic signed [31:0] r_x_c [2];
  logic signed [31:0] r_z_c [2];
  logic        [1:0] r_surface_c;
  logic        [1:0] r_rsp_valid_c;
  logic [31:0]       grants_c [2];

  always_comb begin
    r_valid_c[0]   = r0_valid_i;
    r_valid_c[1]   = r1_valid_i;
    r_surface_c[0] = r0_surface_i;
    r_surface_c[1] = r1_surface_i;
    r_x_c[0]       = r0_x_i;
    r_x_c[1]       = r1_x_i;
    r_z_c[0]       = r0_z_i;
    r_z_c[1]       = r1_z_i;
  end

  assign r0_ready_o     = r_ready_c[0];
  assign r1_ready_o     = r_ready_c[1];
  assign r0_rsp_valid_o = r_rsp_valid_c[0];
  assign r1_rsp_valid_o = r_rsp_valid_c[1];
  assign grants0_o      = grants_c[0];
  assign grants1_o      = grants_c[1];

  zhao_terrain_tapshare_mutant #(
    .N       (2),
    .CENSUS_W(32)
  ) dut (
    .clk  (clk),
    .rst_n(rst_n),

    .r_valid_i    (r_valid_c),
    .r_ready_o    (r_ready_c),
    .r_x_i        (r_x_c),
    .r_z_i        (r_z_c),
    .r_surface_i  (r_surface_c),
    .r_rsp_valid_o(r_rsp_valid_c),

    .t_req_valid_o  (t_req_valid_o),
    .t_req_ready_i  (t_req_ready_i),
    .t_req_x_o      (t_req_x_o),
    .t_req_z_o      (t_req_z_o),
    .t_req_surface_o(t_req_surface_o),
    .t_rsp_valid_i  (t_rsp_valid_i),

    .grants_o   (grants_c),
    .contended_o(contended_o),
    .stray_rsp_o(stray_rsp_o),
    .busy_o     (busy_o)
  );

endmodule : tb_terrain_tapshare_mutant

`default_nettype wire
