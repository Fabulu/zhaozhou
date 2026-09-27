// zhao_probe_doorcost_jobmux.sv -- THE PRICE OF THE ARRANGEMENT NOBODY MEASURED.
//
// SYNTHESIS PROBE. It measures; it never ships. Disposition `instrument` in
// design/console_inventory.yml.
//
// WHAT QUESTION IT ANSWERS
// -----------------------
// `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-muxbuild.md` refused
// console entry I55's raster door and named its price as
//
//     2,065 wires through two module boundaries, or lift `u_tile` out
//
// with no area number, because that packet was fenced off Quartus. Both of its
// routes, and every arrangement in which the binner's on-chip drain and the
// SDRAM walk can BOTH reach `zhao_raster_tile_pipe_v2` inside one frame, require
// a RUN-TIME 2:1 MULTIPLEX on that bus. This module is that multiplex and
// nothing else, so a map of it is that multiplex's cost and nothing else.
//
// THE BUS, FIELD BY FIELD, AT THE CONSOLE'S RATIFIED `METAW = 1877`:
//
//     valid                                     1
//     ax ay bx by cx cy          6 x 21  =    126
//     first last                                2
//     tile_x tile_y              2 x 12  =     24
//     src_id                                   16
//     meta                                  1,877
//     profile_bad                               2
//                                          ------
//     forward                               2,048
//     ready, backward                           1
//     tile_index (DERIVED, not muxed)          16
//                                          ------
//     MUXBUILD's bus width                  2,065
//
// `job_tile_index_o` is a concatenation of six bits each of the SELECTED
// `tile_y`/`tile_x`, so it is downstream of the mux and is not itself muxed.
// It is computed here anyway, exactly as `zhao_geom_bin_pipe_v2` computes it, so
// the probe's boundary is the same boundary the real module has.
//
// WHY THE REAL MODULE DOES NOT CONTAIN THIS MUX
// --------------------------------------------
// It does not need one. `zhao_geom_bin_pipe_v2`'s `JOB_SRC` selects the source at
// ELABORATION, which costs zero logic, because the two arrangements are
// alternatives and must never coexist:
// `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` section 4 rules that "a
// parallel legacy on-chip frame arena that still supplies the actual pixels is
// not closure". An arrangement in which both sources can reach the tile pipe in
// one frame is that forbidden thing with a select line on it.
//
// So this row is evidence about a REFUSAL, not about the shipped design -- the
// number that says what was declined and what declining it saved. CLAUDE.md:
// "a wrong REFUSAL is caught by nothing at all", so a refusal gets a number.
`default_nettype none

module zhao_probe_doorcost_jobmux #(
    parameter int unsigned METAW = 1877
) (
    input  logic                   sel_walk_i,

    // Source A: the binner's on-chip drain.
    input  logic                   a_valid_i,
    output logic                   a_ready_o,
    input  logic signed [20:0]     a_ax_i, a_ay_i, a_bx_i, a_by_i, a_cx_i, a_cy_i,
    input  logic                   a_first_i, a_last_i,
    input  logic signed [11:0]     a_tile_x_i, a_tile_y_i,
    input  logic        [15:0]     a_src_id_i,
    input  logic [METAW-1:0]       a_meta_i,
    input  logic        [1:0]      a_profile_bad_i,

    // Source B: the SDRAM walk.
    input  logic                   b_valid_i,
    output logic                   b_ready_o,
    input  logic signed [20:0]     b_ax_i, b_ay_i, b_bx_i, b_by_i, b_cx_i, b_cy_i,
    input  logic                   b_first_i, b_last_i,
    input  logic signed [11:0]     b_tile_x_i, b_tile_y_i,
    input  logic        [15:0]     b_src_id_i,
    input  logic [METAW-1:0]       b_meta_i,
    input  logic        [1:0]      b_profile_bad_i,

    // The selected job, as `zhao_raster_tile_pipe_v2` takes it.
    output logic                   job_valid_o,
    input  logic                   job_ready_i,
    output logic signed [20:0]     job_ax_o, job_ay_o, job_bx_o, job_by_o,
                                   job_cx_o, job_cy_o,
    output logic                   job_first_o, job_last_o,
    output logic signed [11:0]     job_tile_x_o, job_tile_y_o,
    output logic        [15:0]     job_tile_index_o,
    output logic        [15:0]     job_src_id_o,
    output logic [METAW-1:0]       job_meta_o,
    output logic        [1:0]      job_profile_bad_o
);

  // ONE `always_comb`, one select, every field. No OR anywhere: the two sources
  // are alternatives here too, and a reduction would measure a different
  // circuit as well as being the forbidden arrangement.
  always_comb begin
    if (sel_walk_i) begin
      job_valid_o       = b_valid_i;
      job_ax_o          = b_ax_i;
      job_ay_o          = b_ay_i;
      job_bx_o          = b_bx_i;
      job_by_o          = b_by_i;
      job_cx_o          = b_cx_i;
      job_cy_o          = b_cy_i;
      job_first_o       = b_first_i;
      job_last_o        = b_last_i;
      job_tile_x_o      = b_tile_x_i;
      job_tile_y_o      = b_tile_y_i;
      job_src_id_o      = b_src_id_i;
      job_meta_o        = b_meta_i;
      job_profile_bad_o = b_profile_bad_i;
    end else begin
      job_valid_o       = a_valid_i;
      job_ax_o          = a_ax_i;
      job_ay_o          = a_ay_i;
      job_bx_o          = a_bx_i;
      job_by_o          = a_by_i;
      job_cx_o          = a_cx_i;
      job_cy_o          = a_cy_i;
      job_first_o       = a_first_i;
      job_last_o        = a_last_i;
      job_tile_x_o      = a_tile_x_i;
      job_tile_y_o      = a_tile_y_i;
      job_src_id_o      = a_src_id_i;
      job_meta_o        = a_meta_i;
      job_profile_bad_o = a_profile_bad_i;
    end
  end

  // The backward half of the handshake: one bit, demultiplexed.
  assign a_ready_o = job_ready_i && !sel_walk_i;
  assign b_ready_o = job_ready_i &&  sel_walk_i;

  // Exactly `zhao_geom_bin_pipe_v2`'s own expression, downstream of the select.
  assign job_tile_index_o = {4'd0, job_tile_y_o[9:4], job_tile_x_o[9:4]};

endmodule

`default_nettype wire
