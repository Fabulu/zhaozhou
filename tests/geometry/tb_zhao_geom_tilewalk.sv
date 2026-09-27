// tb_zhao_geom_tilewalk -- GEOM.TILEWALK's directed bench.
//
// A THIN WRAPPER AND NOTHING ELSE. The head table, the walk, the geometry back
// end and the raster door are all modelled in C++
// (`tests/geometry/geom_tilewalk_directed.cpp`), because the thing under test
// is a SEQUENCE -- which tile is queried, which head is offered, when a
// triangle is taken, and which brackets travel with it -- and a sequence is
// easier to drive and to assert from a program than from a second state
// machine written in the same language as the first.
//
// THE `ifdef` IS A PLAIN ONE, NOT FUNCTION-LIKE, and that matters. CLAUDE.md
// records two combiner mutants that passed while measuring unmutated
// production because Verilator's command-line `-D` defines an OBJECT-like
// macro and cannot override a function-like `define` -- silently, with no
// diagnostic, in the flattering direction. A plain `ifdef` selecting between
// two instantiations is the shape `-D` does reach.

`default_nettype none

module tb_zhao_geom_tilewalk #(
    parameter int unsigned TILES  = 8,
    parameter int unsigned GRID_W = 4,
    parameter int unsigned TIDX_W = 3
) (
    input  var logic clk,
    input  var logic rst_n,

    input  var logic        start_i,

    output var logic [TIDX_W-1:0] head_tile_o,
    input  var logic [31:0]       head_chunk_i,
    input  var logic              head_valid_i,

    output var logic        walk_valid_o,
    input  var logic        walk_ready_i,
    output var logic [31:0] walk_head_o,
    input  var logic        walk_done_i,
    input  var logic        walk_failed_i,

    input  var logic        t_valid_i,
    output var logic        t_ready_o,
    input  var logic        t_first_i,
    input  var logic        t_last_i,

    output var logic        be_valid_o,
    input  var logic        be_ready_i,
    input  var logic        be_out_valid_i,
    output var logic        be_out_ready_o,

    output var logic        job_valid_o,
    input  var logic        job_ready_i,
    output var logic signed [11:0] job_tile_x_o,
    output var logic signed [11:0] job_tile_y_o,
    output var logic        job_first_o,
    output var logic        job_last_o,

    output var logic        active_o,
    output var logic        frame_done_o,

    output var logic [31:0] tiles_walked_o,
    output var logic [31:0] tiles_empty_o,
    output var logic [31:0] jobs_issued_o,
    output var logic [31:0] walks_failed_o,
    output var logic [31:0] be_stall_clocks_o,
    output var logic [31:0] overlap_o
);

`ifdef ZHAO_GEOM_TILEWALK_OVERLAP_MUT
  zhao_geom_tilewalk_overlap_mutant #(
`else
  zhao_geom_tilewalk #(
`endif
      .TILES  (TILES),
      .GRID_W (GRID_W),
      .TIDX_W (TIDX_W)
  ) u_tw (
      .clk   (clk),
      .rst_n (rst_n),

      .start_i (start_i),

      .head_tile_o  (head_tile_o),
      .head_chunk_i (head_chunk_i),
      .head_valid_i (head_valid_i),

      .walk_valid_o  (walk_valid_o),
      .walk_ready_i  (walk_ready_i),
      .walk_head_o   (walk_head_o),
      .walk_done_i   (walk_done_i),
      .walk_failed_i (walk_failed_i),

      .t_valid_i (t_valid_i),
      .t_ready_o (t_ready_o),
      .t_first_i (t_first_i),
      .t_last_i  (t_last_i),

      .be_valid_o     (be_valid_o),
      .be_ready_i     (be_ready_i),
      .be_out_valid_i (be_out_valid_i),
      .be_out_ready_o (be_out_ready_o),

      .job_valid_o  (job_valid_o),
      .job_ready_i  (job_ready_i),
      .job_tile_x_o (job_tile_x_o),
      .job_tile_y_o (job_tile_y_o),
      .job_first_o  (job_first_o),
      .job_last_o   (job_last_o),

      .active_o     (active_o),
      .frame_done_o (frame_done_o),

      .tiles_walked_o    (tiles_walked_o),
      .tiles_empty_o     (tiles_empty_o),
      .jobs_issued_o     (jobs_issued_o),
      .walks_failed_o    (walks_failed_o),
      .be_stall_clocks_o (be_stall_clocks_o),
      .overlap_o         (overlap_o)
  );

endmodule

`default_nettype wire
