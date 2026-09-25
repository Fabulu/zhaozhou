// zhao_terrain_veljoin.sv -- TERRAIN.VELJOIN: the lockstep fork that puts
// earth out-lane 1 into TERRAIN.VELOCITY from the SAME evaluation whose
// out-lane 0 TERRAIN.PATCH already consumes.
//
// WHY THIS BLOCK EXISTS, AND WHY IT IS NOT A SCHEDULER
// ---------------------------------------------------------------------------
// Entry I34 in `zhao_console_core.sv` records velocity's blocker as
// architectural rather than as a wire:
//
//     "`zhao_terrain_velocity` drives its OWN 33x33 sweep, so joining it to
//      the consumer's vertex stream is a scheduler and a composer may not
//      write one."
//
// The second half of that sentence is a rule about WHERE the logic lives, not
// about whether it may exist. This file is that logic, as a named block with a
// contract, a test and a ledger entry -- the shape packet EDGECLOSE used when
// the island pitch could not be composed either.
//
// THE FIRST HALF IS NARROWER THAN IT READS, AND THAT IS THIS BLOCK'S CENTRAL
// MEASUREMENT. `zhao_terrain_velocity`'s sweep (law V3) advances
//
//     if (vi == LatMax) begin vi <= 0; vj <= vj + 1; end else vi <= vi + 1;
//
// and `zhao_terrain_pagestream`, which drives the vertex address TERRAIN.PATCH
// receives, advances
//
//     if (vi_q == EDGE-1) begin vi_q <= 0; vj_q <= vj_q + 1; end
//     else vi_q <= vi_q + 1;                      (zhao_terrain_pagestream.sv:797-801)
//
// over the same EDGE = 33 and the same VERTS = 1,089. They are the SAME WALK,
// column-fast then row. So the two streams do not need to be REORDERED or
// BUFFERED against one another -- which is what a scheduler would be -- they
// need to be FORKED with a joint ready and INTERLOCKED so that "they are the
// same walk" is a checked structural fact and not a comment. That is a strictly
// smaller thing, and it is what is built here.
//
// TERRAIN.PATCH already exported the other half of this seam ON PURPOSE. Its
// `fld_covers_o` header says so in as many words: "TERRAIN.VELOCITY consumes
// the same per-vertex lane stream for out-lane 1 and takes this answer rather
// than holding a second 16-rectangle list and re-deciding a ratified law
// (charter 29-6). Exported 2026-08-19 with TERRAIN.VELOCITY." The seam was
// designed; only the join was missing.
//
// LAWS CHOSEN, NOT FOUND
// ---------------------------------------------------------------------------
// J1. THE FORK IS READY-JOINED, NEVER DROP-ON-BUSY. A lane word is taken from
//     the adapter only when BOTH TERRAIN.PATCH and TERRAIN.VELOCITY can take
//     it. REJECTED: forwarding to the patch and dropping for velocity when
//     velocity is busy. That produces a velocity lattice that is silently
//     short by however many words were dropped, with every height number still
//     correct -- a wrong answer in the flattering direction, and precisely the
//     "two errors cancelling inside a checker" shape this repository's rules
//     file is about. If the fork cannot proceed, it STALLS and
//     `arm_stall_clocks_o` counts it.
//
//     The stall is bounded and is measured, not argued: in `StLane`
//     `zhao_terrain_velocity`'s `lane_ready_o` is
//     `(state == StLane) && (!last_lane || out_free)`, and `out_free` is
//     `!r_valid || vv_ready_i`. With a consumer that accepts a lattice word
//     every clock -- which the compose cache's velocity plane is, being one
//     RAM write -- `out_free` is constantly high, so velocity never
//     backpressures the height lane once its sweep is running.
//
// J2. THE SWEEP IS STARTED BY THE FIRST VERTEX FIRE OF A PATCH, NOT BY
//     `patch_open_i`. `zhao_terrain_patch`'s `fields_active_o` is the size of
//     the section 9.1 list, and the list is REFILLED by
//     `zhao_terrain_fieldlist`'s replay AFTER `patch_open_i` clears it. Reading
//     `lanes_i` at `patch_open_i` therefore reads the PREVIOUS patch's count.
//     The first vertex fire is the earliest instant at which the list is
//     settled, and it is still hundreds of clocks ahead of the first answer
//     (I34: "zhao_field_host's front holds ONE point in flight, so a covered
//     vertex-lane is order 80-100 clocks").
//
//     This also handles the unplaced patch for free and for the right reason.
//     The composer discards a whole unplaced patch --
//     `tps_v_ready = tpc_placed ? (tpt_vtx_ready && !tfl_patch_stall) : 1'b1`
//     (`zhao_console_core.sv:22257`) -- so an unplaced patch produces NO vertex
//     fire, and therefore no sweep, and therefore no velocity lattice claiming
//     to describe terrain that was never composed.
//
// J3. `lanes == 0` STILL SWEEPS. With an empty section 9.1 list the adapter
//     never raises an answer, so nothing would start velocity on a lane word.
//     The sweep is started anyway, with `start_lanes_o = 0`, and
//     `zhao_terrain_velocity`'s `StZero` path writes the law V2 zero word at
//     all 1,089 vertices.
//     REJECTED: skipping the sweep and leaving the previous patch's plane in
//     place. `zhao_terrain_compcache_front`'s own header names that failure
//     exactly -- "a swap that did not happen, or happened one patch early,
//     still answers every request with a real composed height from a real
//     patch, at a plausible world position, with every counter agreeing" -- and
//     a stale velocity plane beside a fresh height plane is that hazard with
//     nothing at all able to notice. Ground no live field touches is not
//     moving, and saying so costs 1,089 clocks of a walk that is at least that
//     long anyway.
//
// J4. A NEW PATCH ABORTS A SWEEP STILL RUNNING, AND THE ABORT IS COUNTED.
//     `abort_o` exists because the alternative is a deadlock, and the deadlock
//     is reachable: if `tpc_placed` drops part-way through a patch the rest of
//     that walk is discarded, velocity is left mid-`StLane`, its
//     `start_ready_i` never returns, and J1's ready-join would then hold the
//     HEIGHT lane -- a shipped path -- forever. So the arm asserts `abort_o`
//     whenever a patch opens on a sweep that has not finished.
//     `sweeps_aborted_o` is the count, and a non-zero value is a real finding
//     about the walk, not noise.
//
// J5. THE ADDRESS INTERLOCK IS THE POINT OF THE BLOCK, AND ITS TWO OPERANDS
//     ARE DELIBERATELY CLOCKED BY DIFFERENT THINGS. `vtx_mismatch_o` compares
//
//        * `v_vtx_vi_i`/`v_vtx_vj_i` -- TERRAIN.VELOCITY's OWN internal sweep
//          counter, advanced by ITS lane accounting (`last_lane`), against
//        * `hold_vi`/`hold_vj` -- the walk address this block latched from
//          `zhao_terrain_pagestream` at the vertex fire the adapter latched on.
//
//     Nothing loads both. That is on purpose and it is the whole reason the
//     detector can fire at all: this repository's rules file records a
//     generation-mismatch counter that read zero for its entire life because
//     "the two quantities the detector differences were corrupted in lockstep".
//     Here a lane word inserted, lost, or answered for the wrong vertex moves
//     exactly one of the two operands, so the difference is observable.
//     It is fired with LEGAL STIMULUS at this block's own ports -- present a
//     walk address sequence that skips a vertex -- so no committed mutant is
//     owed for it (ruling R95). `tests/terrain/terrain_veljoin_directed.cpp`
//     section 5 is that firing, with its negative control beside it.
//
// NOT IN THIS BLOCK, deliberately: no reordering and no buffer of lane words
// (that would be the scheduler, and J1/J5 exist so that it is not needed); no
// section 9.1 footprint test (TERRAIN.PATCH owns it, chosen law 2, and
// `covers_i` carries its answer through untouched); no arithmetic of any kind
// on the velocity word -- it is forwarded bit-for-bit, because law V1's
// saturating chain and its SatLedger records belong to TERRAIN.VELOCITY and a
// second rounding here would disagree with the oracle at exactly the values
// that matter.
//
// Conservative SystemVerilog subset only (charter 2). No function-call result
// is indexed; every elaboration check sits inside `initial begin ... end`,
// because Quartus 17.0 rejects a module-scope `if` outright and Verilator's
// lint says nothing about it.

module zhao_terrain_veljoin #(
    parameter int unsigned LAT_W    = 33,
    parameter int unsigned LAT_H    = 33,
    parameter int unsigned CENSUS_W = 32
) (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // the patch boundary
    // -----------------------------------------------------------------------
    // `tce_job_take` in the composer: the streamer has taken the patch and has
    // not yet read a byte of it. ARMS the join (J2) and aborts a sweep that is
    // somehow still running (J4).
    input logic        patch_open_i,
    input logic [15:0] patch_id_i,
    input logic [15:0] src_id_i,

    // `zhao_terrain_patch.fields_active_o`, sampled at the first vertex fire
    // and not before -- see J2.
    input logic [4:0] lanes_i,

    // The walk, as TERRAIN.PATCH receives it. `vtx_fire_i` is exactly the
    // cycle the Earth adapter latches a point on.
    input logic       vtx_fire_i,
    input logic [5:0] w_vi_i,
    input logic [5:0] w_vj_i,

    // -----------------------------------------------------------------------
    // upstream: the Earth answer lane (`zhao_field_earth_adapter`)
    // -----------------------------------------------------------------------
    // The HEIGHT word is not routed through this block: it is a direct wire
    // from the adapter to TERRAIN.PATCH, unchanged and unbuffered. Only the
    // handshake is forked, so nothing here can perturb a shipped value.
    input  logic               a_valid_i,
    output logic               a_ready_o,
    input  logic signed [31:0] a_velocity_i,  // earth out-lane 1, fx16 raw
    input  logic               a_covers_i,    // TERRAIN.PATCH's 9.1 answer

    // -----------------------------------------------------------------------
    // downstream A: TERRAIN.PATCH's field-height lane
    // -----------------------------------------------------------------------
    output logic p_valid_o,
    input  logic p_ready_i,

    // -----------------------------------------------------------------------
    // downstream B: TERRAIN.VELOCITY
    // -----------------------------------------------------------------------
    output logic               v_start_valid_o,
    input  logic               v_start_ready_i,
    output logic        [ 4:0] v_start_lanes_o,
    output logic        [15:0] v_start_patch_id_o,
    output logic        [15:0] v_start_src_id_o,
    output logic               v_abort_o,

    output logic               v_valid_o,
    input  logic               v_ready_i,
    output logic signed [31:0] v_velocity_o,
    output logic               v_covers_o,

    // TERRAIN.VELOCITY's own announced sweep address (law V3), for J5.
    input logic [5:0] v_vtx_vi_i,
    input logic [5:0] v_vtx_vj_i,
    input logic       v_idle_i,

    // -----------------------------------------------------------------------
    // evidence
    // -----------------------------------------------------------------------
    output logic [CENSUS_W-1:0] lanes_joined_o,      // lane words forked to both
    output logic [CENSUS_W-1:0] sweeps_started_o,    // starts TERRAIN.VELOCITY took
    output logic [CENSUS_W-1:0] sweeps_aborted_o,    // J4
    output logic [CENSUS_W-1:0] vtx_mismatch_o,      // J5 -- the interlock
    output logic [CENSUS_W-1:0] arm_stall_clocks_o,  // J1 -- the height lane held
    output logic                idle_o
);

  // ---- frozen constants ----------------------------------------------------
  localparam logic [5:0] LatMaxI = 6'(LAT_W - 1);
  localparam logic [5:0] LatMaxJ = 6'(LAT_H - 1);

  // The 33x33 lattice is Island Patch v1 and is frozen (terrain_rules 2). This
  // block indexes it with 6-bit counters that TERRAIN.VELOCITY and
  // TERRAIN.PAGESTREAM also use, so a wider lattice is a coordinated change and
  // not a parameter sweep. Inside `initial` because Quartus 17.0 rejects a
  // module-scope `if` and a clean Verilator lint says nothing about either --
  // `--lint-only` does not run `initial` blocks at all.
  initial begin
    if (LAT_W > 64 || LAT_H > 64) begin
      $fatal(1, "zhao_terrain_veljoin: LAT_W/LAT_H exceed the 6-bit lattice address");
    end
    if (LAT_W < 2 || LAT_H < 2) begin
      $fatal(1, "zhao_terrain_veljoin: a lattice needs at least one cell");
    end
  end

  // ---- states --------------------------------------------------------------
  // StIdle : between patches. No sweep is owed.
  // StArm  : a patch has opened; waiting for the first vertex fire so that
  //          `lanes_i` is settled (J2).
  // StStart: the start is offered to TERRAIN.VELOCITY and the answer lane is
  //          HELD until it is taken (J1). Normally two clocks.
  // StRun  : forking.
  localparam logic [1:0] StIdle = 2'd0;
  localparam logic [1:0] StArm = 2'd1;
  localparam logic [1:0] StStart = 2'd2;
  localparam logic [1:0] StRun = 2'd3;
  logic [1:0] state;

  logic [ 4:0] c_lanes;
  logic [15:0] c_patch_id;
  logic [15:0] c_src_id;

  // The walk address the adapter is currently answering for. Latched at the
  // vertex fire, which is the same cycle `zhao_field_earth_adapter` latches on
  // (`.vtx_fire_i(tpt_vtx_valid && tpt_vtx_ready)`), so the two agree by
  // construction rather than by timing luck.
  logic [ 5:0] hold_vi;
  logic [ 5:0] hold_vj;
  logic        hold_valid;

  // ---- the fork ------------------------------------------------------------
  // J1: one word, two takers, one ready. In StRun the patch's ready and
  // velocity's ready are ANDed; in every other state the lane is held.
  wire fork_open = (state == StRun);
  wire both_ready = p_ready_i && v_ready_i;

  assign p_valid_o = fork_open && a_valid_i && v_ready_i;
  assign v_valid_o = fork_open && a_valid_i && p_ready_i;
  assign a_ready_o = fork_open && both_ready;

  // The velocity word and its covers bit are forwarded bit-for-bit. No
  // arithmetic here -- law V1's chain is TERRAIN.VELOCITY's.
  assign v_velocity_o = a_velocity_i;
  assign v_covers_o = a_covers_i;

  assign v_start_valid_o = (state == StStart);
  assign v_start_lanes_o = c_lanes;
  assign v_start_patch_id_o = c_patch_id;
  assign v_start_src_id_o = c_src_id;

  assign idle_o = (state == StIdle);

  // ---- J5, the interlock ---------------------------------------------------
  // Compared on the cycle a lane word is actually taken by both, because that
  // is the cycle at which the two walks make a claim about the same vertex.
  // `hold_*` came from TERRAIN.PAGESTREAM through the vertex fire; `v_vtx_*`
  // came from TERRAIN.VELOCITY's own counter. Nothing loads both.
  wire lane_fire = a_valid_i && a_ready_o;
  wire vtx_disagrees = hold_valid && ((v_vtx_vi_i != hold_vi) || (v_vtx_vj_i != hold_vj));

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= StIdle;
      c_lanes <= '0;
      c_patch_id <= '0;
      c_src_id <= '0;
      hold_vi <= '0;
      hold_vj <= '0;
      hold_valid <= 1'b0;
      v_abort_o <= 1'b0;
      lanes_joined_o <= '0;
      sweeps_started_o <= '0;
      sweeps_aborted_o <= '0;
      vtx_mismatch_o <= '0;
      arm_stall_clocks_o <= '0;
    end else begin
      v_abort_o <= 1'b0;

      // ---- the walk address, latched where the adapter latches -------------
      if (vtx_fire_i) begin
        hold_vi <= w_vi_i;
        hold_vj <= w_vj_i;
        hold_valid <= 1'b1;
      end

      // ---- J1: the height lane held while the sweep is being armed ---------
      // Counted rather than assumed. The cost of this block on the shipped
      // height path is exactly this number, and it is expected to be two
      // clocks per patch.
      if ((state == StArm || state == StStart) && a_valid_i) begin
        arm_stall_clocks_o <= arm_stall_clocks_o + {{(CENSUS_W - 1) {1'b0}}, 1'b1};
      end

      // ---- J4: a patch opening always re-arms ------------------------------
      if (patch_open_i) begin
        c_patch_id <= patch_id_i;
        c_src_id   <= src_id_i;
        hold_valid <= 1'b0;
        state      <= StArm;
        // A sweep that has not finished is dropped rather than left to
        // deadlock the ready-join. `v_idle_i` is TERRAIN.VELOCITY's own
        // `idle_o`, which is `(state == StIdle) && !r_valid` -- so this fires
        // only when there really is a partial lattice to discard.
        if (!v_idle_i) begin
          v_abort_o <= 1'b1;
          sweeps_aborted_o <= sweeps_aborted_o + {{(CENSUS_W - 1) {1'b0}}, 1'b1};
        end
      end else begin
        case (state)
          StIdle: begin
            // nothing owed
          end

          // J2: the first vertex fire is the earliest instant `fields_active_o`
          // is settled for THIS patch.
          StArm: begin
            if (vtx_fire_i) begin
              c_lanes <= lanes_i;
              state   <= StStart;
            end
          end

          StStart: begin
            if (v_start_ready_i) begin
              sweeps_started_o <= sweeps_started_o + {{(CENSUS_W - 1) {1'b0}}, 1'b1};
              state <= StRun;
            end
          end

          StRun: begin
            if (lane_fire) begin
              lanes_joined_o <= lanes_joined_o + {{(CENSUS_W - 1) {1'b0}}, 1'b1};
              if (vtx_disagrees) begin
                vtx_mismatch_o <= vtx_mismatch_o + {{(CENSUS_W - 1) {1'b0}}, 1'b1};
              end
            end
          end

          default: state <= StIdle;
        endcase
      end
    end
  end

  // The lattice bounds are carried so a future widening cannot silently pass a
  // 6-bit address through this block. Read by the elaboration check above and
  // by nothing at run time, which is why they are named here rather than left
  // as bare literals in the check.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [5:0] unused_lat_bounds = LatMaxI ^ LatMaxJ;
  /* verilator lint_on UNUSEDSIGNAL */

endmodule : zhao_terrain_veljoin
