// zhao_post_composite_ring_hazard_mutant.sv -- the positive control for
// `ring_hazard_o`, which no legal stimulus can fire.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `zhao_post_composite.ring_hazard_o` watches for a displaced sample that
// reaches a ring column the write pointer has already overwritten (or has not
// yet reached). That state is UNREACHABLE while the output lag is correct, so
// no legal input can move the counter, and "it can fire" would stay an
// argument forever. A counter asserted zero and never seen to move is a claim,
// and it is the claim to check hardest.
//
// ---------------------------------------------------------------------------
// IT IS A WRAPPER, NOT A COPY -- AND THAT IS THE WHOLE POINT
// ---------------------------------------------------------------------------
// CLAUDE.md: "a wrapper that instantiates the production module cannot drift,
// and must not be counted as a copy". The thirteen combiner copies and the
// eight AUX-pipe copies all went stale in the flattering direction because a
// copy of an old version is a positive control for a block that no longer
// exists. There is no copied body here: this file instantiates
// `zhao_post_composite` and changes ONE THING.
//
// THE ONE SUBSTANTIVE CHANGE:   LAG_PX = 9  ->  LAG_PX = 0
//
// That is legal to write because LAG_PX is a real design knob -- it is the
// "small output-line delay queue" the contract allows -- and the production
// header's safety argument is precisely about its value:
//
//   the ring read needs the write pointer more than 8 columns ahead;
//   the lag AT THE RING READ is LAG_PX + 2; so LAG_PX must be >= 7.
//
// At LAG_PX = 0 the effective lead is 2 columns, so a sample at dx = +8 reaches
// past the write pointer and the counter must fire. This file therefore proves
// two things at once: that the detector works, and that the delay queue is not
// decoration.
//
// If `.*` ever fails to elaborate here, production has GAINED OR LOST A PORT.
// That is a loud failure and it is the intended behaviour -- it is the opposite
// of a stale copy, which fails silently by continuing to pass.
//
// The driver is tests/mutants/post_composite_ring_hazard_mutant.cpp and its
// polarity is INVERTED: it passes when `ring_hazard_o` is NON-ZERO. It is
// evidence about the instrument, not about the design.
//
// No simulation assertion is disabled here; this module contains none.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_post_composite_ring_hazard_mutant #(
    parameter int unsigned LINE_W    = 384,
    parameter int unsigned MAX_H     = 240,
    parameter int unsigned NLINE     = 9,
    parameter int unsigned LAG_LINES = 4
) (
    input  var logic                 clk,
    input  var logic                 rst_n,
    input  var logic                 frame_start_i,
    input  var logic [$clog2(LINE_W+1)-1:0] frame_w_i,
    input  var logic [$clog2(MAX_H +1)-1:0] frame_h_i,
    input  var logic                 view_sel_i,
    input  var logic                 s_valid_i,
    output var logic                 s_ready_o,
    input  var logic [15:0]          s_rgb_i,
    output var logic                 gd_req_v_o,
    output var logic                 gd_view_o,
    output var logic [$clog2(LINE_W+1)-3:0] gd_cx_o,
    output var logic [$clog2(MAX_H +1)-3:0] gd_cy_o,
    input  var logic                 gd_present_i,
    input  var logic signed [7:0]    gd_dx_i,
    input  var logic signed [7:0]    gd_dy_i,
    output var logic                 gg_req_v_o,
    output var logic                 gg_view_o,
    output var logic [$clog2(LINE_W+1)-3:0] gg_cx_o,
    output var logic [$clog2(MAX_H +1)-3:0] gg_cy_o,
    input  var logic                 gg_present_i,
    input  var logic [15:0]          gg_glow_i,
    input  var logic                 gg_ink_i,
    output var logic                 atm_req_v_o,
    output var logic [$clog2(LINE_W+1)-1:0] atm_req_x_o,
    output var logic [$clog2(MAX_H +1)-1:0] atm_req_y_o,
    input  var logic                 atm_en_i,
    input  var logic                 atm_valid_i,
    input  var logic [15:0]          atm_rgb_i,
    input  var logic [7:0]           atm_opacity_i,
    input  var logic                 atm_add_i,
    input  var logic [7:0]           bloom_gain_i,
    input  var logic                 grade_valid_i,
    input  var logic                 pv_we_i,
    input  var logic [1:0]           pv_sel_i,
    input  var logic [5:0]           pv_addr_i,
    input  var logic [71:0]          pv_data_i,
    input  var logic signed [8:0]    bias_r_i,
    input  var logic signed [8:0]    bias_g_i,
    input  var logic signed [8:0]    bias_b_i,
    input  var logic [15:0]          flash_rgb_i,
    input  var logic [7:0]           flash_amt_i,
    input  var logic [15:0]          ink_rgb_i,
    output var logic                 hud_req_v_o,
    output var logic [$clog2(LINE_W+1)-1:0] hud_req_x_o,
    output var logic [$clog2(MAX_H +1)-1:0] hud_req_y_o,
    input  var logic                 hud_valid_i,
    input  var logic [15:0]          hud_rgb_i,
    output var logic                 o_valid_o,
    input  var logic                 o_ready_i,
    output var logic [15:0]          o_rgb_o,
    output var logic [$clog2(LINE_W+1)-1:0] o_x_o,
    output var logic [$clog2(MAX_H +1)-1:0] o_y_o,
    output var logic                 o_last_o,
    output var logic                 echo_valid_o,
    output var logic [15:0]          echo_rgb_o,
    output var logic [31:0]          displacement_edge_clamps_o,
    output var logic [31:0]          bloom_cells_contributing_o,
    output var logic [31:0]          passes_completed_o,
    output var logic [31:0]          grading_table_missing_o,
    output var logic [31:0]          plane_missing_o,
    output var logic [31:0]          line_fill_writes_o,
    output var logic [31:0]          output_writes_o,
    output var logic [31:0]          plane_reads_o,
    output var logic [31:0]          ring_hazard_o
);

  zhao_post_composite #(
      .LINE_W    (LINE_W),
      .MAX_H     (MAX_H),
      .NLINE     (NLINE),
      .LAG_LINES (LAG_LINES),
      .LAG_PX    (0)          // <<< THE MUTATION, and the only one
  ) u_dut (.*);

endmodule : zhao_post_composite_ring_hazard_mutant

`default_nettype wire
