// zhao_walk_meta_hold.sv -- THE WALKED TRIANGLE'S MATERIAL STATE, HELD.
//
// ===========================================================================
// WHAT IT IS FOR, AND THE DEFECT IT REPAIRS
// ===========================================================================
// At `GEOM_WALK_RASTER = 1` the console draws from the SDRAM walk, and
// `zhao_geom_bin_pipe_v2`'s door takes the job's whole 1,877-bit metadata from
// its OWN INPUT PORTS:
//
//     assign job_meta_w = tri_meta_w;                    // JOB_SRC == 1
//     assign tri_meta_w = {planes..., tri_fragment_state_i,
//                          tri_continuation_tail_i, tri_flat_request_i};
//
// 2,037 of those bits are GEOM.SETUP's and GEOM.ATTRPACK's output, and the
// composer holds them correctly: on the walk path both back ends retire on
// `tw_be_out_ready_o`, which is asserted only on `job_valid_o && job_ready_i`,
// so their output stands unchanged until the job is taken.
//
// THE REMAINING 378 BITS WERE HELD BY NOTHING. `tri_flat_request_i`,
// `tri_continuation_tail_i` and `tri_fragment_state_i` are combinational from
// `tri_matstate_c`, which on the walk path is combinational from
// `pw_t_matstate_w` -- GEOM.PARAMWALK's LIVE OUTPUT BUS. And the walker
// RELEASES that record long before it offers the job:
// `zhao_geom_tilewalk` asserts `t_ready_o` in `T_TAKE`, then waits in `T_WAIT`
// for the back ends, and only offers `job_valid_o` in `T_JOB`. Between those
// states the paramwalk has moved on -- to the next record, or to nothing.
//
// So the door could read A's corners, A's planes and B's material state. That
// is CLAUDE.md's metadata-swap chapter at the composer's join, and it is
// invisible wherever the matstate does not differ, which is why it cost two or
// three fragments of 1,216 and why it moved with SDRAM latency.
//
// It produced BOTH of clause 6's symptoms from one cause, which is worth
// stating because two packets argued them as one and as two without measuring:
//
//   * `zhao_ms_flat_request` returns ALL-ZERO when the stored `VALID` bit is
//     low -- the legal "this surface takes no texture sample" profile -- so a
//     job that read an idle bus asked for NOTHING. Measured as
//     `plan_accepted=1213` against `fragments=1216`;
//   * `zhao_ms_tail` reads `EFFTAG` **ungated by VALID**, so the same instant
//     leaked a stray effect tag into a pixel nothing tagged.
//
// ===========================================================================
// WHAT THIS BLOCK IS, AND WHAT IT IS NOT
// ===========================================================================
// It is ONE HOLD REGISTER and its two interlock counters. It is not a queue:
// it has one entry, because `zhao_geom_tilewalk`'s handshake is SERIAL -- one
// record is taken, handed to the back ends, and its job offered, before the
// next record can be taken. One record in flight, one entry.
//
// IT DOES NOT NARROW, STUB OR TIE ANYTHING OFF. The same 128 bits that reached
// the door before reach it now; they simply reach it on the clock they belong
// to. No field is dropped, no default is substituted, and the geometry path is
// untouched -- `git diff` over every `zhao_geom_*` file is empty.
//
// It is also NOT a checker of the composition. The two counters below watch the
// PROTOCOL between two producers, and that is the distinction CLAUDE.md's
// lockstep chapter asks for: `held_valid_q` is enabled by the CAPTURE edge and
// read on the JOB edge, so the two sides of each comparison are clocked by
// different events in different modules. A fault in either can move them.
//
// ===========================================================================
// AND IT IS THE HONEST REPLACEMENT FOR A DETECTOR THAT CANNOT FIRE
// ===========================================================================
// `zhao_geom_tilewalk.sv` carries `overlap_o`, whose comment says it counts
// "a triangle offered by the walk while one is still in flight ... on the
// OFFER and not on a take". The code is
//
//     if ((state_q == S_RUN) && (tstate_q != T_TAKE) && t_valid_i && t_ready_o)
//
// and `t_ready_o` is itself `(state_q == S_RUN) && (tstate_q == T_TAKE) &&
// be_ready_i`. The guard demands `tstate_q != T_TAKE` and then ANDs in a term
// that is only true when `tstate_q == T_TAKE`: the two are contradictory, so
// the counter is a STRUCTURAL ZERO. It also counts a TAKE where its comment
// says an OFFER. Its committed mutant fires it only by deleting the state term
// from `t_ready_o`, so `overlap_o == 0` is evidence about that one term and not
// about the handshake the comment describes.
//
// `err_overwrite_o` below watches the same property FROM THE CONSUMER'S SIDE,
// where no producer term can cancel it, and `walk_meta_hold_directed` fires it
// with ordinary stimulus rather than with a mutant. The blind counter is left
// where it is -- it belongs to a block this packet may only read, and it is
// reported rather than edited.
//
// ===========================================================================
// COST
// ===========================================================================
// `MSW` + `IDW` + 1 = 147 flops of payload, plus two 32-bit censuses = 211
// registers, 0 DSP, 0 M10K. That is the price of the hold the other 2,037 bits
// already had, and it is declared here rather than left for a fit to discover.
//
// Conservative SystemVerilog subset only (charter section 2); no package deps.
// Quartus 17: no module-scope `if`, no implicit generate, elaboration checks
// inside `initial begin ... end`.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_walk_meta_hold).

module zhao_walk_meta_hold #(
  // The material state's width, and the arena index that rides beside it. Both
  // are the composer's own (`ZHAO_TD_MATSTATE_W`, the 18-bit descriptor index);
  // they are parameters so this block carries no package dependency and so the
  // elaboration check below has something to check.
  parameter int unsigned MSW = 128,
  parameter int unsigned IDW = 18
) (
  input  logic clk,
  input  logic rst_n,

  // ---- the walk is sweeping. While this is low the hold is EMPTY and idle,
  //      and clearing on it is silent: a sweep that ends with a record taken
  //      and no job issued is the walker finishing a tile, not a fault.
  input  logic             active_i,

  // ---- CAPTURE: the exact edge GEOM.PARAMWALK's record is consumed.
  //      The composer wires this to the tile walker's own take --
  //      `be_valid_o && be_ready_i`, which is identical to `t_valid_i &&
  //      t_ready_o` because both expressions carry the same two state terms.
  input  logic             cap_fire_i,
  input  logic [MSW-1:0]   cap_matstate_i,
  input  logic [IDW-1:0]   cap_arena_id_i,

  // ---- CONSUME: the job handshake at the shell's walk door.
  input  logic             job_fire_i,

  // ---- the held record, which is what the door must read -----------------
  output logic [MSW-1:0]   held_matstate_o,
  output logic [IDW-1:0]   held_arena_id_o,
  output logic             held_valid_o,

  // ---- the two interlock counters (spec/counters.md section 4: saturating) -
  // A job taken with NOTHING held. This is the defect's own signature: the
  // door would have read whatever the live bus happened to carry.
  output logic [31:0]      err_job_unheld_o,
  // A second record captured before the first was consumed by a job. This is
  // the serial-handshake violation `overlap_o` claims to watch, watched from
  // the side that can actually see it.
  output logic [31:0]      err_overwrite_o
);

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`;
  // a bare module-scope `if` is a syntax error there while Verilator accepts
  // it silently, which is a trap CLAUDE.md records and this file does not
  // re-derive.
  initial begin : p_widths
    if (MSW == 0)
      $fatal(1, "zhao_walk_meta_hold: MSW must be nonzero");
    if (IDW == 0)
      $fatal(1, "zhao_walk_meta_hold: IDW must be nonzero");
  end

  logic [MSW-1:0] ms_q;
  logic [IDW-1:0] id_q;
  logic           v_q;

  assign held_matstate_o = ms_q;
  assign held_arena_id_o = id_q;
  assign held_valid_o    = v_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      ms_q             <= {MSW{1'b0}};
      id_q             <= {IDW{1'b0}};
      v_q              <= 1'b0;
      err_job_unheld_o <= 32'd0;
      err_overwrite_o  <= 32'd0;
    end else begin
      // ---- the two counters, read BEFORE this cycle's state change --------
      // `v_q` is enabled by the CAPTURE and read on the JOB, so neither
      // comparison has both of its operands under one enable.
      if (job_fire_i && !v_q && (err_job_unheld_o != CNT_MAX))
        err_job_unheld_o <= err_job_unheld_o + 32'd1;
      if (cap_fire_i && v_q && !job_fire_i && (err_overwrite_o != CNT_MAX))
        err_overwrite_o <= err_overwrite_o + 32'd1;

      // ---- the hold ------------------------------------------------------
      // CAPTURE WINS over a same-cycle job. The walker cannot produce both on
      // one clock in this arrangement -- `T_TAKE` and `T_JOB` are different
      // sub-states -- but a hold whose behaviour depends on that being true
      // would be a block that is only correct when its caller behaves, which
      // is the trap `zhao_raster_fragment`'s hazard comment names. Ordering
      // them makes the coincidence harmless instead of unrepresentable.
      if (!active_i) begin
        v_q <= 1'b0;
      end else if (cap_fire_i) begin
        ms_q <= cap_matstate_i;
        id_q <= cap_arena_id_i;
        v_q  <= 1'b1;
      end else if (job_fire_i) begin
        v_q <= 1'b0;
      end
    end
  end

endmodule : zhao_walk_meta_hold
