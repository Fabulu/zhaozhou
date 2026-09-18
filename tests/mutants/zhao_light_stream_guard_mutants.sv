// zhao_light_stream_guard_mutants.sv -- positive controls for the two guards
// in `zhao_light_stream` that NO LEGAL STIMULUS CAN REACH.
//
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `root_queue_overflow_o` and `tag_mismatch_o` are both asserted ZERO by
// `tests/geometry/light_stream_directed.cpp`. A detector reading zero is a
// claim, and it is the claim to check hardest -- so each one is fired here,
// deliberately, before its silence is quoted anywhere.
//
// Neither state is reachable while the block is correct:
//   * the root-request ring cannot overflow while its admission margin is
//     right, so no input sequence moves `root_queue_overflow_o`;
//   * the divider's tag and the side channel are written from one in-order
//     stream, so no input sequence makes them disagree.
//
// THESE ARE WRAPPERS, NOT COPIES
// ---------------------------------------------------------------------------
// CLAUDE.md records what copies cost: thirteen combiner copies and eight
// AUX-pipe copies drifted from production and went on passing green while
// measuring a machine that no longer existed, and one failed with an alarming
// message about a component that had been deleted. A wrapper cannot drift. It
// instantiates the PRODUCTION module by name with `.*`, so a port added or
// removed in production fails to elaborate here -- loudly -- instead of
// quietly testing an old shape.
//
// The renamed module names below also keep any ordinary source list from
// elaborating this file by mistake.
//
// THE DRIVERS PASS WHEN THE COUNTER FIRES. Inverse polarity, deliberately:
// this is evidence about the INSTRUMENT, not about the design.
`default_nettype none

// ---------------------------------------------------------------------------
// MUTANT 1 -- RQ_ROOM_MARGIN 3 -> 1.
//
// One substantive change. The margin is how much room the root-request ring
// must show when a square GROUP STARTS, and it must exceed one because the
// admission test runs three or more clocks before this group's push while the
// PREVIOUS group's push may still be in flight. At 1 the ring wraps: request
// N+SQQD overwrites request N before N is read, a normal is sealed with
// ANOTHER normal's magnitude, its own slot is never sealed, and the arena
// eventually wedges full with nothing issuable.
//
// This was the block's real defect, found by trace, not by any value check --
// every colour it emitted was plausible. `root_queue_overflow_o` is the guard
// added with the repair, and this wrapper is the proof that it watches the
// thing it claims to watch.
// ---------------------------------------------------------------------------
module zhao_light_stream_rqfull_mutant #(
    parameter int unsigned LIGHTS_MAX = 8,
    parameter int unsigned GAINW      = 20,
    parameter int unsigned ACCW       = 32,
    parameter int unsigned SRCW       = 16,
    parameter int unsigned CNTW       = 32,
    parameter int unsigned NSLOTS     = 16,
    parameter bit          DEGEN_BLACK = 1'b0
) (
    input  var logic clk,
    input  var logic rst_n,
    input  var logic        cfg_we_i,
    input  var logic        cfg_commit_i,
    input  var logic [7:0]  cfg_addr_i,
    input  var logic [31:0] cfg_data_i,
    output var logic        cfg_gen_o,
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] n_x_i,
    input  var logic signed [31:0] n_y_i,
    input  var logic signed [31:0] n_z_i,
    input  var logic               n_mag_valid_i,
    input  var logic [31:0]        n_mag_i,
    input  var logic               n_degenerate_i,
    input  var logic               n_profile_i,
    input  var logic [3:0]         n_lights_i,
    input  var logic [SRCW-1:0]    n_src_id_i,
    output var logic            r_valid_o,
    input  var logic            r_ready_i,
    output var logic [16:0]     rgb_r_o,
    output var logic [16:0]     rgb_g_o,
    output var logic [16:0]     rgb_b_o,
    output var logic            degenerate_vtx_o,
    output var logic [SRCW-1:0] src_id_o,
    output var logic [CNTW-1:0] normal_inputs_o,
    output var logic [CNTW-1:0] normal_prepared_o,
    output var logic [CNTW-1:0] roots_issued_o,
    output var logic [CNTW-1:0] roots_retired_o,
    output var logic [CNTW-1:0] supplied_mags_o,
    output var logic [CNTW-1:0] terms_accepted_o,
    output var logic [CNTW-1:0] terms_retired_o,
    output var logic [CNTW-1:0] terms_null_o,
    output var logic [CNTW-1:0] normal_queue_wait_o,
    output var logic [CNTW-1:0] descriptor_wait_o,
    output var logic [CNTW-1:0] dot_product_slots_o,
    output var logic [CNTW-1:0] square_product_slots_o,
    output var logic [CNTW-1:0] unused_product_slots_o,
    output var logic [CNTW-1:0] divider_backpressure_o,
    output var logic [CNTW-1:0] colour_backpressure_o,
    output var logic [CNTW-1:0] output_backpressure_o,
    output var logic [CNTW-1:0] epoch_refusals_o,
    output var logic [CNTW-1:0] logical_raw_saturations_o,
    output var logic [CNTW-1:0] degenerate_terms_o,
    output var logic [CNTW-1:0] vertices_lit_o,
    output var logic [CNTW-1:0] degenerate_o,
    output var logic [CNTW-1:0] ndl_clamp_lo_o,
    output var logic [CNTW-1:0] ndl_clamp_hi_o,
    output var logic [CNTW-1:0] rgb_sat_o,
    output var logic [CNTW-1:0] cfg_refused_o,
    output var logic [CNTW-1:0] nlights_clamped_o,
    output var logic [CNTW-1:0] seam_mismatch_o,
    output var logic [CNTW-1:0] tag_mismatch_o,
    output var logic [CNTW-1:0] root_queue_overflow_o,
    output var logic idle_o
);
  zhao_light_stream #(
      .LIGHTS_MAX(LIGHTS_MAX), .GAINW(GAINW), .ACCW(ACCW), .SRCW(SRCW), .CNTW(CNTW),
      .NSLOTS(NSLOTS), .DEGEN_BLACK(DEGEN_BLACK),
      // <-- THE MUTATION. Both halves of the defect this block shipped with:
      //     the shallow ring AND an admission test with no margin for the
      //     push that is still in flight when the next group is admitted.
      .SQ_QUEUE_DEPTH(4),
      .RQ_ROOM_MARGIN(0),
      .SIDE_SKEW(0)
  ) u_dut (.*);
endmodule : zhao_light_stream_rqfull_mutant

// ---------------------------------------------------------------------------
// MUTANT 2 -- SIDE_SKEW 0 -> 1.
//
// The light term's wide state (its detail word) travels beside the divider in
// a FIFO while its identity travels THROUGH the divider's sixteen folded
// stages as a tag. `tag_mismatch_o` differences the two. The whole point is
// that the two sides have different clocking histories -- a detector whose
// operands move together cannot fire, which is the defect a metadata bank
// shipped with a live identity counter reading zero beside it.
//
// SIDE_SKEW = 1 offsets the side-channel read by one entry: the join now pairs
// term N's quotient with term N+1's detail. Every emitted colour stays inside
// range and most stay plausible, which is exactly why the counter has to exist
// and exactly why it has to be seen to move.
// ---------------------------------------------------------------------------
module zhao_light_stream_skew_mutant #(
    parameter int unsigned LIGHTS_MAX = 8,
    parameter int unsigned GAINW      = 20,
    parameter int unsigned ACCW       = 32,
    parameter int unsigned SRCW       = 16,
    parameter int unsigned CNTW       = 32,
    parameter int unsigned NSLOTS     = 16,
    parameter bit          DEGEN_BLACK = 1'b0
) (
    input  var logic clk,
    input  var logic rst_n,
    input  var logic        cfg_we_i,
    input  var logic        cfg_commit_i,
    input  var logic [7:0]  cfg_addr_i,
    input  var logic [31:0] cfg_data_i,
    output var logic        cfg_gen_o,
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] n_x_i,
    input  var logic signed [31:0] n_y_i,
    input  var logic signed [31:0] n_z_i,
    input  var logic               n_mag_valid_i,
    input  var logic [31:0]        n_mag_i,
    input  var logic               n_degenerate_i,
    input  var logic               n_profile_i,
    input  var logic [3:0]         n_lights_i,
    input  var logic [SRCW-1:0]    n_src_id_i,
    output var logic            r_valid_o,
    input  var logic            r_ready_i,
    output var logic [16:0]     rgb_r_o,
    output var logic [16:0]     rgb_g_o,
    output var logic [16:0]     rgb_b_o,
    output var logic            degenerate_vtx_o,
    output var logic [SRCW-1:0] src_id_o,
    output var logic [CNTW-1:0] normal_inputs_o,
    output var logic [CNTW-1:0] normal_prepared_o,
    output var logic [CNTW-1:0] roots_issued_o,
    output var logic [CNTW-1:0] roots_retired_o,
    output var logic [CNTW-1:0] supplied_mags_o,
    output var logic [CNTW-1:0] terms_accepted_o,
    output var logic [CNTW-1:0] terms_retired_o,
    output var logic [CNTW-1:0] terms_null_o,
    output var logic [CNTW-1:0] normal_queue_wait_o,
    output var logic [CNTW-1:0] descriptor_wait_o,
    output var logic [CNTW-1:0] dot_product_slots_o,
    output var logic [CNTW-1:0] square_product_slots_o,
    output var logic [CNTW-1:0] unused_product_slots_o,
    output var logic [CNTW-1:0] divider_backpressure_o,
    output var logic [CNTW-1:0] colour_backpressure_o,
    output var logic [CNTW-1:0] output_backpressure_o,
    output var logic [CNTW-1:0] epoch_refusals_o,
    output var logic [CNTW-1:0] logical_raw_saturations_o,
    output var logic [CNTW-1:0] degenerate_terms_o,
    output var logic [CNTW-1:0] vertices_lit_o,
    output var logic [CNTW-1:0] degenerate_o,
    output var logic [CNTW-1:0] ndl_clamp_lo_o,
    output var logic [CNTW-1:0] ndl_clamp_hi_o,
    output var logic [CNTW-1:0] rgb_sat_o,
    output var logic [CNTW-1:0] cfg_refused_o,
    output var logic [CNTW-1:0] nlights_clamped_o,
    output var logic [CNTW-1:0] seam_mismatch_o,
    output var logic [CNTW-1:0] tag_mismatch_o,
    output var logic [CNTW-1:0] root_queue_overflow_o,
    output var logic idle_o
);
  zhao_light_stream #(
      .LIGHTS_MAX(LIGHTS_MAX), .GAINW(GAINW), .ACCW(ACCW), .SRCW(SRCW), .CNTW(CNTW),
      .NSLOTS(NSLOTS), .DEGEN_BLACK(DEGEN_BLACK),
      .SQ_QUEUE_DEPTH(8),
      .RQ_ROOM_MARGIN(3),
      .SIDE_SKEW(1)            // <-- THE MUTATION, and the only one
  ) u_dut (.*);
endmodule : zhao_light_stream_skew_mutant

`default_nettype wire
