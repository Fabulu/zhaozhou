// tb_geom_projradius.sv -- the REAL `zhao_view_projscale` feeding the REAL
// `zhao_geom_projradius`, so the driver writes a VIEW MATRIX onto the
// projector configuration bus and the projected radius comes out the far end.
//
// Wiring them together here rather than driving `kx_i`/`vw_i` from C++ is the
// whole point of the bench: `kx` is `max(|m[0][0]|, |m[0][1]|, |m[0][2]|)`, and
// a snooper that decoded the wrong three addresses, or took the maximum of the
// wrong row, would pass every test that handed it the answer.
//
// `w_i`/`behind_i` ARE driven from C++, and deliberately: they come from the
// shared projector's client-A result, and `zhao_project_core` is a separately
// verified block with its own suite. What the driver supplies is the number
// that block produces -- computed by `zref::mat4_vec4` from the same matrix
// written onto the bus above -- so the two halves cannot silently describe
// different cameras.
`default_nettype none

module tb_geom_projradius #(
    parameter int unsigned TAGW = 16
) (
    input  var logic clk,
    input  var logic rst_n,

    // the projector configuration bus, written by the driver
    input  var logic        cfg_we_i,
    input  var logic        cfg_view_i,
    input  var logic [ 4:0] cfg_addr_i,
    input  var logic [31:0] cfg_data_i,

    // what the snooper made of it, exported so the bench can check it directly
    output var logic [31:0] kx0_o,
    output var logic [31:0] kx1_o,
    output var logic [11:0] vw0_o,
    output var logic [11:0] vw1_o,
    output var logic [31:0] abs_saturated_o,

    // which view this evaluation is for
    input  var logic        view_i,

    input  var logic               req_valid_i,
    output var logic               req_ready_o,
    input  var logic signed [31:0] bound_radius_i,
    input  var logic        [30:0] w_i,
    input  var logic               behind_i,
    input  var logic [TAGW-1:0]    tag_i,

    output var logic               ans_valid_o,
    input  var logic               ans_ready_i,
    output var logic signed [31:0] radius_q8_o,
    output var logic               ans_ok_o,
    output var logic [TAGW-1:0]    tag_o,

    output var logic [31:0] evaluations_o,
    output var logic [31:0] behind_o,
    output var logic [31:0] bad_bound_o,
    output var logic [31:0] saturated_o
);

  logic [31:0] kx0_w, kx1_w;
  logic [11:0] vw0_w, vw1_w;

  zhao_view_projscale u_scale (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_we_i  (cfg_we_i),
      .cfg_view_i(cfg_view_i),
      .cfg_addr_i(cfg_addr_i),
      .cfg_data_i(cfg_data_i),

      .kx0_o(kx0_w),
      .kx1_o(kx1_w),
      .vw0_o(vw0_w),
      .vw1_o(vw1_w),

      .abs_saturated_o(abs_saturated_o)
  );

  assign kx0_o = kx0_w;
  assign kx1_o = kx1_w;
  assign vw0_o = vw0_w;
  assign vw1_o = vw1_w;

  zhao_geom_projradius #(
      .TAGW(TAGW)
  ) u_dut (
      .clk  (clk),
      .rst_n(rst_n),

      .req_valid_i   (req_valid_i),
      .req_ready_o   (req_ready_o),
      .kx_i          (view_i ? kx1_w : kx0_w),
      .vw_i          (view_i ? vw1_w : vw0_w),
      .bound_radius_i(bound_radius_i),
      .w_i           (w_i),
      .behind_i      (behind_i),
      .tag_i         (tag_i),

      .ans_valid_o(ans_valid_o),
      .ans_ready_i(ans_ready_i),
      .radius_q8_o(radius_q8_o),
      .ans_ok_o   (ans_ok_o),
      .tag_o      (tag_o),

      .evaluations_o(evaluations_o),
      .behind_o     (behind_o),
      .bad_bound_o  (bad_bound_o),
      .saturated_o  (saturated_o)
  );

endmodule : tb_geom_projradius

`default_nettype wire
