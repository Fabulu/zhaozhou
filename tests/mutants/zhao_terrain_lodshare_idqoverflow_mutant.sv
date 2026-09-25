// zhao_terrain_lodshare_idqoverflow_mutant.sv -- A DELIBERATELY BROKEN COPY.
// NOT SHIPPED. NOT IN ANY PRODUCTION CLOSURE.
//
// This exists to make `idq_overflow_o` a DETECTOR rather than a hopeful zero.
//
// That counter watches for a push into a FULL identity queue -- an entry
// landing on top of a live one, which presents downstream as one patch's levels
// filed under another patch's coordinate rather than as an overflow. It is
// UNREACHABLE BY CONSTRUCTION in a working block: `p_sp_fire_c` requires
// `lod_sp_valid_o`, which carries `!idq_full_c`, so the push and the full
// condition are mutually exclusive and NO LEGAL STIMULUS can move the counter.
// "It can fire" would stay an argument for ever.
//
// THE ONE SUBSTANTIVE CHANGE, and it is one term on one line:
//
//     lod_sp_valid_o = prep_sel_i ? (p_sp_valid_i && !idq_full_c) : e_sp_valid_i
//  -> lod_sp_valid_o = prep_sel_i ?  p_sp_valid_i                 : e_sp_valid_i
//
// and the matching term on `p_sp_ready_o`, which is the same guard spelled for
// the producer. Removing only one of the two would leave the block refusing the
// descriptor it had already declared valid, which is a different defect. With
// the guard gone an identity beyond the queue's depth is admitted and the
// counter moves.
//
// REFRESHED 2026-09-25 (EDGECLOSE), onto a production body that changed in
// TWO substantive ways: IDQ_DEPTH is sixteen rather than four (four was a
// DEADLOCK against the real ladder, which accepts all sixteen subpatches
// before emitting any), and the queue now POPS ON THE LAST BEAT OF A
// SUBPATCH rather than on every decision (a dual page emits a top and an
// underside for ONE identity). The mutation is unchanged -- the same two
// `!idq_full_c` terms removed -- and the driver now needs more than sixteen
// offers to overflow, which it already makes: it offers twenty-four.
//
// THE DRIVER'S POLARITY IS INVERTED: the mutant driver PASSES when
// `idq_overflow_o` is NONZERO and FAILS when it is zero. It is evidence about
// the INSTRUMENT, not about the design.
//
// THE POSITIVE CONTROL THAT MAKES THE RUN MEAN ANYTHING: `idq_full_stalls_o`
// must ALSO be nonzero. A mutant run that never filled the queue would report
// `idq_overflow_o == 0` for a reason that has nothing to do with the mutation,
// and would read as "the counter cannot fire" -- the flattering direction, and
// the exact shape CLAUDE.md calls a gate that cannot reach the state. The
// driver asserts both.
//
// TWO SIMULATION ASSERTIONS ARE DISABLED BELOW, IN THIS COPY ONLY, and the
// reason is CLAUDE.md's: a mutant that trips a `$fatal` before the
// synthesizable counter can be read proves nothing about the counter. The
// assertions firing is INDEPENDENT CORROBORATION that the mutation bites; the
// counter is the thing that ships. Each is marked where it is cut.
//
// It is a separate FILE rather than a temporary edit for two reasons. A
// temporary edit to production RTL is a fit-corrupting live-tree hazard, and it
// leaves nothing behind: the next person inherits the same argument and no
// evidence.
//
// The module is RENAMED so a source-list mistake cannot elaborate it in place
// of the real one, and it lives under tests/ where check_forbidden_sources.py
// will not find it in a production closure.
//
// REGENERATE IT if zhao_terrain_lodshare.sv changes shape: this is a COPY, and
// a copy of an old version is a positive control for a block that no longer
// exists. `tools/budget/mutant_copy_drift.py` watches for exactly that and
// signals on PROVENANCE -- if the production file has been committed since this
// one was, this copy cannot contain what production gained.



// zhao_terrain_lodshare.sv -- the LOD TIME-SHARE: one `zhao_terrain_lod`, two
// passes, a FRAME-SCOPED FREEZE of the governor targets, and the suppression of
// PREPARE's history writeback.
//
// Law: reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt section 2
//      design/contracts/TERRAIN.EDGERECON.md ("RE-MEASURED 2026-09-23")
//      fpga/rtl/prod/zhao_console_core.sv entry I21 item (e), steps 3 and 4
//
// ===========================================================================
// WHY THIS IS A NAMED BLOCK AND NOT COMPOSER WIRES
// ===========================================================================
// Entry I21 says it in one sentence and it is the reason this file exists:
//
//     "TERRAIN.LOD has no mode, bypass or phase input (its only switches are
//      `cam*_en_i` and `dual_i`), so PREPARE-vs-EMIT selection on eleven `sp_*`
//      inputs and thirteen `out_*` outputs is a NAMED BLOCK, never composer
//      wires -- the hidden-adapter failure this entry already refused once."
//
// A mux this wide spread across a 28,000-line composer is an adapter nobody
// can find, with no contract, no counters and no test. It is also where the
// three defects below would live, invisibly.
//
// (The entry says "twelve `sp_*` inputs and fourteen `out_*` outputs". Counted
// off `zhao_terrain_lod.sv`'s own port list: ELEVEN `sp_*` inputs -- ten
// payload plus `sp_valid_i` -- with `sp_ready_o` an output, and THIRTEEN
// `out_*` outputs with `out_ready_i` an input. Both counts included a port of
// the opposite direction. Corrected here rather than propagated.)
//
// ===========================================================================
// DEFECT 1 -- THE GOVERNOR TARGETS ARE RE-LATCHED PER PATCH (blocker C)
// ===========================================================================
// MEASURED in `zhao_console_core.sv:26414`, and it survived contact:
//
//     end else if (tld_idle) begin
//       gv_cam0_scale_q <= mgv_cam0_scale;
//       ...
//       gv_eye0_x_q     <= veye0_x;
//
// `tld_idle` is TERRAIN.LOD's own `idle_o`, which is HIGH BETWEEN EVERY PATCH,
// so all thirteen targets are re-sampled ~256 times a frame.
//
// AND THE HALF THAT WAS WRONG IN THE FLATTERING DIRECTION. The entry says the
// re-latch "is harmless TODAY" because MEASURE.GOVERNOR decides on `frame_i`.
// That is true of the seven `mgv_*` values and FALSE of the six eye values:
// `veye0_*`/`veye1_*` come from `zhao_view_eye`, whose registers update on
// `cfg_we_i` -- "a pulse the executor owns, and a write lands the cycle it is"
// (`zhao_view_eye.sv:43`, `:123`). They are HOST-WRITE-SCOPED, not frame-
// scoped. A SetView landing mid-frame moves the camera between two patches of
// ONE pass today, and between PREPARE and EMIT tomorrow.
//
// So the freeze is not merely a precondition for the second pass; the single
// pass is already sampling a moving camera, and nothing was watching. This
// block captures all thirteen on `frame_i` and presents the FROZEN copy to
// LOD for both passes. `freeze_drift_o` counts the cycles the live value
// differed from the frozen one while a pass was running -- which is the
// measurement of how much motion the old arrangement was absorbing.
//
// ===========================================================================
// DEFECT 2 -- A PREPARE PASS WOULD WRITE THE HISTORY EMIT READS AS "LAST FRAME"
// ===========================================================================
// The directive: "PREPARE must not commit hysteresis history or advance the
// state EMIT treats as last frame. Commit history once, for the completed/
// admitted result."
//
// THE SUPPRESSION IS STRUCTURAL, AND THAT IS EXACTLY WHY IT NEEDS A GUARD.
// `zhao_terrain_devstore`'s read FSM enters `R_HWR` on `(h_any_q ||
// h_valid_i)` with `h_any_q` set ONLY inside `if (h_valid_i)`
// (`zhao_terrain_devstore.sv:859-865`, `:877`), and the producer of `h_valid`
// is `zhao_terrain_jobissue` (`:343 assign h_valid_o = h_fire_c;`). This block
// routes PREPARE's LOD output to the RECONCILER and not to jobissue, so
// jobissue is starved during PREPARE and emits no history at all. devstore
// needs no change: its own comment already describes the path -- "A patch
// whose LOD pass emitted NOTHING skips the write entirely."
//
// A correctness property that holds because of how something ELSE is wired is
// exactly the kind that a later edit breaks silently. So the history port runs
// THROUGH this block and is gated, and `hist_leak_o` counts any history beat
// OFFERED while PREPARE owns the ladder.
//
// `hist_leak_o` reads zero in the composed console for a structural reason and
// is nevertheless FIREABLE FROM THIS BLOCK'S OWN BOUNDARY, because `h_valid_i`
// is an input: a bench raises it during PREPARE and the counter moves while
// `h_valid_o` stays low. That is the distinction worth keeping -- it is a
// counter whose SILENCE is evidence, and its silence is only worth quoting
// because the same suite has watched it move. `terrain_lodshare_directed`
// case 5 does exactly that.
//
// The counter in this block that genuinely cannot be reached is
// `idq_overflow_o`; see its port comment and the committed mutant.
//
// ===========================================================================
// DEFECT 3 -- THE PATCH COORDINATE DOES NOT RIDE THROUGH THE LADDER
// ===========================================================================
// TERRAIN.EDGERECON's file port needs `f_ix_i`/`f_iz_i`, the PATCH's grid
// coordinate. `zhao_terrain_lod` carries `src_id` and NOTHING ELSE: there is no
// coordinate anywhere in its port list. So the coordinate has to be held
// alongside the ladder, and holding it in a plain register that tracks "the
// patch the walker is on" is THE metadata-swap defect CLAUDE.md documents --
// a stall pairs the ladder's output for patch A with the walker's coordinate
// for patch B, and every accepted/emitted counter balances because no counter
// looks at the field that moved.
//
// So the coordinate rides in a QUEUE, pushed when a descriptor is accepted
// INTO the ladder and popped when its answer comes OUT, and the queue's
// `src_id` is CHECKED against the `src_id` the ladder returns. Ask what clocks
// each side: the queue entry is written by the `sp` handshake, the returned
// `src_id` by the ladder's own pipeline. TWO DIFFERENT ENABLES, so a slip
// moves one and not the other and `ident_mismatch_o` can fire. That is the
// property the metadata bank's counter lacked.
//
// AND ON A MISMATCH THE FILE IS REFUSED. The directive: "do not silently emit a
// cracked admitted mesh to keep the register green." A record banked under the
// wrong coordinate is worse than no record: an absent patch falls back
// symmetrically on both sides of every seam, which is the console's existing
// behaviour; a MISFILED patch hands its neighbours a level for ground that is
// somewhere else.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_terrain_lodshare_idqoverflow_mutant #(
    parameter int unsigned DEVW   = 24,
    parameter int unsigned MORPHW = 17,
    parameter int unsigned GENW   = 8,
    // The identity queue's depth. It must cover every descriptor the ladder can
    // hold between accepting `sp` and emitting `out`.
    //
    // SIXTEEN, AND THE FOUR THIS PARAMETER USED TO DEFAULT TO WAS A DEADLOCK.
    // Corrected 2026-09-25 (EDGECLOSE), found by composing this block against
    // the REAL `zhao_terrain_lod` for the first time. The claim it replaces --
    // "a sequential ladder with ONE descriptor in flight, so 4 is already
    // generous" -- is false, and `zhao_terrain_lod.sv:671-684` says so:
    //
    //     StDecide: lvl[fill_idx] <= n_level;
    //               if (fill_idx == 5'd15) state <= StEmit;
    //               else { fill_idx <= fill_idx + 1; state <= StFill; }
    //
    // The ladder ACCEPTS ALL SIXTEEN SUBPATCHES BEFORE IT EMITS ANYTHING,
    // because `edge_lane()` needs the whole `lvl[]` array to answer a
    // subpatch's four interior neighbours. Its in-flight depth is sixteen.
    //
    // At four, the queue filled on the fourth descriptor, `p_sp_ready_o`
    // dropped, the ladder never reached its sixteenth, never emitted, and
    // never drained the queue -- A PERMANENT DEADLOCK, with `idq_overflow_o`,
    // `ident_mismatch_o` and every other counter reading ZERO. The full-guard
    // made the wrong value SAFE, exactly as the old comment claimed, and safe
    // is not the same as live.
    //
    // THE DIRECTED SUITE COULD NOT SEE IT. `terrain_lodshare_directed` drives
    // a ladder MODEL, and the model emitted per descriptor. That is CLAUDE.md's
    // "a gate that cannot reach the state is not evidence about the state": 96
    // checks passed against a machine that does not exist.
    parameter int unsigned IDQ_DEPTH = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the frame boundary -------------------------------------------------
    // One-cycle pulse. THE ONLY enable on the thirteen frozen registers below.
    input var logic frame_i,

    // ---- who owns the ladder ------------------------------------------------
    // HIGH = PREPARE owns it. Driven by the walker's `busy_o`. It is sampled
    // per handshake and NOT allowed to move mid-patch: `sel_midpatch_o` counts
    // a change while the identity queue is non-empty, which means somebody's
    // descriptors are in flight and their answers are about to be routed to the
    // wrong consumer.
    input var logic prep_sel_i,

    // ---- the LIVE governor targets, thirteen --------------------------------
    // Seven from MEASURE.GOVERNOR (frame-scoped already) and six from
    // VIEW.EYE (host-write-scoped, and the reason this freeze is not optional).
    input var logic signed [31:0] gv_cam0_x_i,
    input var logic signed [31:0] gv_cam0_y_i,
    input var logic signed [31:0] gv_cam0_z_i,
    input var logic        [15:0] gv_cam0_scale_i,
    input var logic               gv_cam0_en_i,
    input var logic signed [31:0] gv_cam1_x_i,
    input var logic signed [31:0] gv_cam1_y_i,
    input var logic signed [31:0] gv_cam1_z_i,
    input var logic        [15:0] gv_cam1_scale_i,
    input var logic               gv_cam1_en_i,
    input var logic        [15:0] gv_hyst_i,
    input var logic        [ 7:0] gv_min_hold_i,
    input var logic        [16:0] gv_morph_step_i,

    // ---- the FROZEN targets, to TERRAIN.LOD ---------------------------------
    output var logic signed [31:0] lod_cam0_x_o,
    output var logic signed [31:0] lod_cam0_y_o,
    output var logic signed [31:0] lod_cam0_z_o,
    output var logic        [15:0] lod_cam0_scale_o,
    output var logic               lod_cam0_en_o,
    output var logic signed [31:0] lod_cam1_x_o,
    output var logic signed [31:0] lod_cam1_y_o,
    output var logic signed [31:0] lod_cam1_z_o,
    output var logic        [15:0] lod_cam1_scale_o,
    output var logic               lod_cam1_en_o,
    output var logic        [15:0] lod_hyst_o,
    output var logic        [ 7:0] lod_min_hold_o,
    output var logic        [16:0] lod_morph_step_o,

    // ---- PREPARE client: `zhao_terrain_prepwalk`'s descriptor stream --------
    input  var logic               p_sp_valid_i,
    output var logic               p_sp_ready_o,
    input  var logic signed [31:0] p_sp_cx_i,
    input  var logic signed [31:0] p_sp_cy_i,
    input  var logic signed [31:0] p_sp_cz_i,
    input  var logic [DEVW-1:0]    p_sp_dev1_i,
    input  var logic [DEVW-1:0]    p_sp_dev2_i,
    input  var logic [DEVW-1:0]    p_sp_dev3_i,
    input  var logic [1:0]         p_sp_prev_level_i,
    input  var logic [MORPHW-1:0]  p_sp_prev_morph_i,
    input  var logic [7:0]         p_sp_hold_i,
    input  var logic [15:0]        p_sp_src_id_i,
    input  var logic signed [15:0] p_sp_ix_i,
    input  var logic signed [15:0] p_sp_iz_i,
    /* verilator lint_off UNUSEDSIGNAL */
    // Carried for the consumer's benefit and checked nowhere here: this block
    // does not interpret residency. It is on the port so that a reader sees the
    // generation travelling WITH the descriptor rather than wonder where it
    // went. P4's acceptance compares it at the query side.
    input  var logic [GENW-1:0]    p_sp_gen_i,
    /* verilator lint_on UNUSEDSIGNAL */

    // ---- EMIT client: `zhao_terrain_spdesc`'s descriptor stream -------------
    input  var logic               e_sp_valid_i,
    output var logic               e_sp_ready_o,
    input  var logic signed [31:0] e_sp_cx_i,
    input  var logic signed [31:0] e_sp_cy_i,
    input  var logic signed [31:0] e_sp_cz_i,
    input  var logic [DEVW-1:0]    e_sp_dev1_i,
    input  var logic [DEVW-1:0]    e_sp_dev2_i,
    input  var logic [DEVW-1:0]    e_sp_dev3_i,
    input  var logic [1:0]         e_sp_prev_level_i,
    input  var logic [MORPHW-1:0]  e_sp_prev_morph_i,
    input  var logic [7:0]         e_sp_hold_i,
    input  var logic [15:0]        e_sp_src_id_i,

    // ---- the single TERRAIN.LOD instance: descriptors in -------------------
    output var logic               lod_sp_valid_o,
    input  var logic               lod_sp_ready_i,
    output var logic signed [31:0] lod_sp_cx_o,
    output var logic signed [31:0] lod_sp_cy_o,
    output var logic signed [31:0] lod_sp_cz_o,
    output var logic [DEVW-1:0]    lod_sp_dev1_o,
    output var logic [DEVW-1:0]    lod_sp_dev2_o,
    output var logic [DEVW-1:0]    lod_sp_dev3_o,
    output var logic [1:0]         lod_sp_prev_level_o,
    output var logic [MORPHW-1:0]  lod_sp_prev_morph_o,
    output var logic [7:0]         lod_sp_hold_o,
    output var logic [15:0]        lod_sp_src_id_o,

    // ---- the single TERRAIN.LOD instance: decisions out --------------------
    input  var logic        lod_out_valid_i,
    output var logic        lod_out_ready_o,
    input  var logic [5:0]  lod_out_ox_i,
    input  var logic [5:0]  lod_out_oz_i,
    input  var logic [1:0]  lod_out_level_i,
    input  var logic [1:0]  lod_out_lvl_nz_i,
    input  var logic [1:0]  lod_out_lvl_pz_i,
    input  var logic [1:0]  lod_out_lvl_nx_i,
    input  var logic [1:0]  lod_out_lvl_px_i,
    input  var logic [MORPHW-1:0] lod_out_morph_i,
    input  var logic        lod_out_surface_i,
    input  var logic        lod_out_dual_i,
    input  var logic [15:0] lod_out_src_id_i,
    input  var logic [7:0]  lod_out_hold_i,

    // ---- PREPARE's answer: TERRAIN.EDGERECON's FILE port, field for field ---
    output var logic        f_valid_o,
    input  var logic        f_ready_i,
    output var logic [15:0] f_ix_o,
    output var logic [15:0] f_iz_o,
    output var logic [ 5:0] f_ox_o,
    output var logic [ 5:0] f_oz_o,
    output var logic [ 1:0] f_level_o,
    output var logic        f_surface_o,

    // ---- EMIT's answer: onward to TERRAIN.JOBISSUE, unchanged --------------
    output var logic        e_out_valid_o,
    input  var logic        e_out_ready_i,
    output var logic [5:0]  e_out_ox_o,
    output var logic [5:0]  e_out_oz_o,
    output var logic [1:0]  e_out_level_o,
    output var logic [1:0]  e_out_lvl_nz_o,
    output var logic [1:0]  e_out_lvl_pz_o,
    output var logic [1:0]  e_out_lvl_nx_o,
    output var logic [1:0]  e_out_lvl_px_o,
    output var logic [MORPHW-1:0] e_out_morph_o,
    output var logic        e_out_surface_o,
    output var logic        e_out_dual_o,
    output var logic [15:0] e_out_src_id_o,
    output var logic [7:0]  e_out_hold_o,

    // ---- the HISTORY WRITEBACK, gated ---------------------------------------
    // In from TERRAIN.JOBISSUE, out to TERRAIN.DEVSTORE. Passed through
    // unchanged in EMIT and held LOW in PREPARE.
    input  var logic              h_valid_i,
    output var logic              h_ready_o,
    input  var logic [1:0]        h_level_i,
    input  var logic [MORPHW-1:0] h_morph_i,
    input  var logic [7:0]        h_hold_i,
    output var logic              h_valid_o,
    input  var logic              h_ready_i,
    output var logic [1:0]        h_level_o,
    output var logic [MORPHW-1:0] h_morph_o,
    output var logic [7:0]        h_hold_o,

    // ---- counters ------------------------------------------------------------
    output var logic [31:0] prep_descriptors_o,
    output var logic [31:0] emit_descriptors_o,
    output var logic [31:0] prep_decisions_o,
    output var logic [31:0] emit_decisions_o,
    // The underside replay: TERRAIN.LOD emits it carrying the top's level
    // (TERRAIN.LOD law 7) and TERRAIN.EDGERECON consumes it without filing. It
    // is forwarded, counted, and NOT dropped here -- dropping it in the mux
    // would make the reconciler's own `lanes_filed_o` count beats instead of
    // decisions, which is the check that catches a lost lane.
    output var logic [31:0] prep_underside_o,
    // THE THREE DETECTORS. Each differences two quantities loaded by two
    // DIFFERENT enables; see the header.
    output var logic [31:0] ident_mismatch_o,
    output var logic [31:0] hist_leak_o,
    output var logic [31:0] sel_midpatch_o,
    // A PUSH INTO A FULL QUEUE. Structurally unreachable while `p_sp_ready_o`'s
    // `!idq_full_c` term is correct, so NO legal stimulus moves it and "it can
    // fire" would stay an argument for ever. The demonstration is a committed
    // mutant with inverted polarity --
    // `tests/mutants/zhao_terrain_lodshare_idqoverflow_mutant.sv` -- exactly
    // `wq_overflow_o`'s case in CLAUDE.md.
    //
    // THE FIRST VERSION OF THIS COUNTER WAS WRONG AND WOULD HAVE READ NONZERO
    // ON A HEALTHY BLOCK. It counted `p_sp_valid_i && idq_full_c &&
    // lod_sp_ready_i` -- which is the guard CORRECTLY REFUSING a descriptor,
    // not an overflow. A counter named for a fault that fires on correct
    // behaviour is worse than no counter: it trains its reader to ignore it.
    output var logic [31:0] idq_overflow_o,
    // THE POSITIVE CONTROL FOR THE ONE ABOVE, and it is why the mutant is
    // known to reach the state it mutates. This counts the cycles the queue
    // was FULL with a descriptor waiting -- the guard doing its job. A mutant
    // run in which this reads zero never filled the queue and proves nothing.
    output var logic [31:0] idq_full_stalls_o,
    // NOT a fault: the measurement of how far the live camera moved from the
    // frozen one while a pass was running. A frame in which the host writes no
    // SetView reads zero; a frame in which it does reads the number of cycles
    // the two disagreed, which is the size of the hazard the old per-patch
    // re-latch was absorbing silently.
    output var logic [31:0] freeze_drift_o,
    output var logic [31:0] freezes_o,
    output var logic        idle_o
);

`ifndef SYNTHESIS
  initial begin
    if (IDQ_DEPTH < 2)
      $fatal(1, "lodshare: the identity queue needs at least two entries, not %0d", IDQ_DEPTH);
    // A power of two keeps the wrap a mask rather than a comparison, and a
    // non-power-of-two would wrap onto entry zero and overwrite a live identity
    // -- which presents as a coordinate swap, not as an overflow.
    if ((IDQ_DEPTH & (IDQ_DEPTH - 1)) != 0)
      $fatal(1, "lodshare: IDQ_DEPTH must be a power of two, not %0d", IDQ_DEPTH);
    // THE LADDER'S OWN IN-FLIGHT DEPTH, and a shallower queue DEADLOCKS rather
    // than degrading. `zhao_terrain_lod` accepts sixteen descriptors before it
    // emits one (`:671-684`), so a queue that fills first stops the producer
    // that would have unblocked it. This guard is why a future reader cannot
    // "tune" this back down.
    if (IDQ_DEPTH < 16)
      $fatal(1, "lodshare: IDQ_DEPTH=%0d is below zhao_terrain_lod's sixteen-descriptor in-flight depth; the identity queue would fill before the ladder emits and the two blocks would deadlock with every counter at zero", IDQ_DEPTH);
  end
`endif

  localparam int unsigned IDQW = $clog2(IDQ_DEPTH);

  // ==========================================================================
  // THE FREEZE -- thirteen registers, ONE enable, and that enable is the frame
  // ==========================================================================
  logic signed [31:0] fz_cam0_x_q, fz_cam0_y_q, fz_cam0_z_q;
  logic        [15:0] fz_cam0_scale_q;
  logic               fz_cam0_en_q;
  logic signed [31:0] fz_cam1_x_q, fz_cam1_y_q, fz_cam1_z_q;
  logic        [15:0] fz_cam1_scale_q;
  logic               fz_cam1_en_q;
  logic        [15:0] fz_hyst_q;
  logic        [ 7:0] fz_min_hold_q;
  logic        [16:0] fz_morph_step_q;

  assign lod_cam0_x_o     = fz_cam0_x_q;
  assign lod_cam0_y_o     = fz_cam0_y_q;
  assign lod_cam0_z_o     = fz_cam0_z_q;
  assign lod_cam0_scale_o = fz_cam0_scale_q;
  assign lod_cam0_en_o    = fz_cam0_en_q;
  assign lod_cam1_x_o     = fz_cam1_x_q;
  assign lod_cam1_y_o     = fz_cam1_y_q;
  assign lod_cam1_z_o     = fz_cam1_z_q;
  assign lod_cam1_scale_o = fz_cam1_scale_q;
  assign lod_cam1_en_o    = fz_cam1_en_q;
  assign lod_hyst_o       = fz_hyst_q;
  assign lod_min_hold_o   = fz_min_hold_q;
  assign lod_morph_step_o = fz_morph_step_q;

  // The drift witness. Its two sides are the FROZEN copy -- written only by
  // `frame_i` -- and the LIVE input. One enable does not drive both, which is
  // the whole difference between this and the metadata bank's dead counter.
  wire drift_c =
        (gv_cam0_x_i     != fz_cam0_x_q)
     || (gv_cam0_y_i     != fz_cam0_y_q)
     || (gv_cam0_z_i     != fz_cam0_z_q)
     || (gv_cam0_scale_i != fz_cam0_scale_q)
     || (gv_cam0_en_i    != fz_cam0_en_q)
     || (gv_cam1_x_i     != fz_cam1_x_q)
     || (gv_cam1_y_i     != fz_cam1_y_q)
     || (gv_cam1_z_i     != fz_cam1_z_q)
     || (gv_cam1_scale_i != fz_cam1_scale_q)
     || (gv_cam1_en_i    != fz_cam1_en_q)
     || (gv_hyst_i       != fz_hyst_q)
     || (gv_min_hold_i   != fz_min_hold_q)
     || (gv_morph_step_i != fz_morph_step_q);

  // ==========================================================================
  // THE IDENTITY QUEUE
  // ==========================================================================
  logic [15:0]        idq_ix_q   [IDQ_DEPTH];
  logic [15:0]        idq_iz_q   [IDQ_DEPTH];
  logic [15:0]        idq_src_q  [IDQ_DEPTH];
  logic [IDQW:0]      idq_wr_q, idq_rd_q;

  wire [IDQW:0] idq_occ_c  = idq_wr_q - idq_rd_q;
  wire          idq_full_c = (idq_occ_c >= (IDQW+1)'(IDQ_DEPTH));
  wire          idq_empty_c= (idq_occ_c == '0);

  wire [15:0] idq_head_ix_c  = idq_ix_q [idq_rd_q[IDQW-1:0]];
  wire [15:0] idq_head_iz_c  = idq_iz_q [idq_rd_q[IDQW-1:0]];
  wire [15:0] idq_head_src_c = idq_src_q[idq_rd_q[IDQW-1:0]];

  // ==========================================================================
  // THE INPUT MUX
  // ==========================================================================
  // PREPARE additionally needs room in the identity queue; EMIT does not use
  // the queue at all, because its answers go to TERRAIN.JOBISSUE which already
  // pairs them by its own door.
  assign lod_sp_valid_o      = prep_sel_i ? p_sp_valid_i : e_sp_valid_i;  // MUTATED: !idq_full_c removed
  assign lod_sp_cx_o         = prep_sel_i ? p_sp_cx_i         : e_sp_cx_i;
  assign lod_sp_cy_o         = prep_sel_i ? p_sp_cy_i         : e_sp_cy_i;
  assign lod_sp_cz_o         = prep_sel_i ? p_sp_cz_i         : e_sp_cz_i;
  assign lod_sp_dev1_o       = prep_sel_i ? p_sp_dev1_i       : e_sp_dev1_i;
  assign lod_sp_dev2_o       = prep_sel_i ? p_sp_dev2_i       : e_sp_dev2_i;
  assign lod_sp_dev3_o       = prep_sel_i ? p_sp_dev3_i       : e_sp_dev3_i;
  assign lod_sp_prev_level_o = prep_sel_i ? p_sp_prev_level_i : e_sp_prev_level_i;
  assign lod_sp_prev_morph_o = prep_sel_i ? p_sp_prev_morph_i : e_sp_prev_morph_i;
  assign lod_sp_hold_o       = prep_sel_i ? p_sp_hold_i       : e_sp_hold_i;
  assign lod_sp_src_id_o     = prep_sel_i ? p_sp_src_id_i     : e_sp_src_id_i;

  // THE IDLE CLIENT'S READY IS LOW, not "don't care". A ready that floated high
  // for the client which does not own the ladder would retire descriptors into
  // nothing -- the quietest possible way to lose a patch.
  assign p_sp_ready_o = prep_sel_i && lod_sp_ready_i;  // MUTATED: !idq_full_c removed
  assign e_sp_ready_o = !prep_sel_i && lod_sp_ready_i;

  wire sp_fire_c   = lod_sp_valid_o && lod_sp_ready_i;
  wire p_sp_fire_c = sp_fire_c && prep_sel_i;

  // ==========================================================================
  // THE OUTPUT DEMUX
  // ==========================================================================
  // The identity check gates the FILE. A descriptor whose answer cannot be
  // matched to the patch it was asked about is RETIRED FROM THE LADDER and NOT
  // FILED: the ladder must not be left holding it, and the reconciler must not
  // be told a level for ground it cannot name.
  wire ident_ok_c = !idq_empty_c && (idq_head_src_c == lod_out_src_id_i);

  // THE LAST BEAT OF A SUBPATCH. See the pop below for why this is not simply
  // "every decision": a dual page emits a TOP and an UNDERSIDE for ONE
  // descriptor, interleaved, and they share one identity.
  wire idq_pop_c = lod_out_dual_i ? lod_out_surface_i : !lod_out_surface_i;

  assign f_valid_o   = prep_sel_i && lod_out_valid_i && ident_ok_c;
  assign f_ix_o      = idq_head_ix_c;
  assign f_iz_o      = idq_head_iz_c;
  assign f_ox_o      = lod_out_ox_i;
  assign f_oz_o      = lod_out_oz_i;
  assign f_level_o   = lod_out_level_i;
  assign f_surface_o = lod_out_surface_i;

  assign e_out_valid_o  = !prep_sel_i && lod_out_valid_i;
  assign e_out_ox_o     = lod_out_ox_i;
  assign e_out_oz_o     = lod_out_oz_i;
  assign e_out_level_o  = lod_out_level_i;
  assign e_out_lvl_nz_o = lod_out_lvl_nz_i;
  assign e_out_lvl_pz_o = lod_out_lvl_pz_i;
  assign e_out_lvl_nx_o = lod_out_lvl_nx_i;
  assign e_out_lvl_px_o = lod_out_lvl_px_i;
  assign e_out_morph_o  = lod_out_morph_i;
  assign e_out_surface_o= lod_out_surface_i;
  assign e_out_dual_o   = lod_out_dual_i;
  assign e_out_src_id_o = lod_out_src_id_i;
  assign e_out_hold_o   = lod_out_hold_i;

  // A misidentified answer is retired anyway -- see above. Without the
  // `!ident_ok_c` term the ladder would stall for ever on a descriptor nobody
  // will accept, turning a counted fault into a hung frame.
  assign lod_out_ready_o = prep_sel_i ? (f_ready_i || !ident_ok_c)
                                      : e_out_ready_i;

  wire out_fire_c   = lod_out_valid_i && lod_out_ready_o;
  wire p_out_fire_c = out_fire_c && prep_sel_i;

  // ==========================================================================
  // THE HISTORY GATE
  // ==========================================================================
  // PREPARE starves TERRAIN.JOBISSUE structurally, so `h_valid_i` is already
  // low here during PREPARE and this gate is belt AND braces. It is kept
  // because a correctness property that holds through somebody else's wiring is
  // the kind an innocent edit breaks in silence, and because the gate gives
  // `hist_leak_o` somewhere to live.
  assign h_valid_o = h_valid_i && !prep_sel_i;
  assign h_level_o = h_level_i;
  assign h_morph_o = h_morph_i;
  assign h_hold_o  = h_hold_i;
  // The producer is told the beat was taken, so a suppressed history write does
  // not wedge TERRAIN.JOBISSUE waiting on a ready that will never come.
  assign h_ready_o = prep_sel_i ? 1'b1 : h_ready_i;

  assign idle_o = idq_empty_c;

  logic prep_sel_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fz_cam0_x_q     <= '0;
      fz_cam0_y_q     <= '0;
      fz_cam0_z_q     <= '0;
      fz_cam0_scale_q <= '0;
      fz_cam0_en_q    <= 1'b0;
      fz_cam1_x_q     <= '0;
      fz_cam1_y_q     <= '0;
      fz_cam1_z_q     <= '0;
      fz_cam1_scale_q <= '0;
      fz_cam1_en_q    <= 1'b0;
      fz_hyst_q       <= '0;
      fz_min_hold_q   <= '0;
      fz_morph_step_q <= '0;
      idq_wr_q        <= '0;
      idq_rd_q        <= '0;
      prep_sel_q      <= 1'b0;

      prep_descriptors_o <= '0;
      emit_descriptors_o <= '0;
      prep_decisions_o   <= '0;
      emit_decisions_o   <= '0;
      prep_underside_o   <= '0;
      ident_mismatch_o   <= '0;
      hist_leak_o        <= '0;
      sel_midpatch_o     <= '0;
      idq_overflow_o     <= '0;
      idq_full_stalls_o  <= '0;
      freeze_drift_o     <= '0;
      freezes_o          <= '0;
    end else begin
      prep_sel_q <= prep_sel_i;

      // ---- THE FREEZE. One enable, and it is the frame boundary. -----------
      if (frame_i) begin
        fz_cam0_x_q     <= gv_cam0_x_i;
        fz_cam0_y_q     <= gv_cam0_y_i;
        fz_cam0_z_q     <= gv_cam0_z_i;
        fz_cam0_scale_q <= gv_cam0_scale_i;
        fz_cam0_en_q    <= gv_cam0_en_i;
        fz_cam1_x_q     <= gv_cam1_x_i;
        fz_cam1_y_q     <= gv_cam1_y_i;
        fz_cam1_z_q     <= gv_cam1_z_i;
        fz_cam1_scale_q <= gv_cam1_scale_i;
        fz_cam1_en_q    <= gv_cam1_en_i;
        fz_hyst_q       <= gv_hyst_i;
        fz_min_hold_q   <= gv_min_hold_i;
        fz_morph_step_q <= gv_morph_step_i;
        freezes_o       <= freezes_o + 32'd1;
      end else if (drift_c && !idq_empty_c) begin
        // Counted only while descriptors are in flight: drift between frames,
        // with the ladder idle, is the camera simply moving and is not a
        // hazard. Drift DURING a pass is the thing blocker C is about.
        freeze_drift_o <= freeze_drift_o + 32'd1;
      end

      // ---- the selector may not move while answers are in flight -----------
      if ((prep_sel_i != prep_sel_q) && !idq_empty_c)
        sel_midpatch_o <= sel_midpatch_o + 32'd1;

      // ---- the identity queue ----------------------------------------------
      if (p_sp_fire_c) begin
        idq_ix_q [idq_wr_q[IDQW-1:0]] <= p_sp_ix_i;
        idq_iz_q [idq_wr_q[IDQW-1:0]] <= p_sp_iz_i;
        idq_src_q[idq_wr_q[IDQW-1:0]] <= p_sp_src_id_i;
        idq_wr_q <= idq_wr_q + (IDQW+1)'(1);
        prep_descriptors_o <= prep_descriptors_o + 32'd1;
      end

      // THE OVERFLOW WATCH: a push that lands in a FULL queue, overwriting a
      // live identity. Unreachable while `p_sp_ready_o`'s `!idq_full_c` term
      // is correct -- `p_sp_fire_c` cannot be true with `idq_full_c` -- so no
      // legal stimulus moves it and the demonstration is a committed mutant.
      if (p_sp_fire_c && idq_full_c)
        idq_overflow_o <= idq_overflow_o + 32'd1;

      // Its positive control: the queue really does reach full, with a
      // descriptor waiting on it. Reachable with stimulus alone -- stall the
      // output and keep offering -- so the two are never quoted together as
      // one silence.
      if (p_sp_valid_i && prep_sel_i && idq_full_c)
        idq_full_stalls_o <= idq_full_stalls_o + 32'd1;

      if (sp_fire_c && !prep_sel_i)
        emit_descriptors_o <= emit_descriptors_o + 32'd1;

      if (p_out_fire_c) begin
        if (ident_ok_c) begin
          // POP ON THE LAST BEAT OF THE SUBPATCH, NOT ON EVERY DECISION.
          // Corrected 2026-09-25 (EDGECLOSE), the second defect composing
          // against the real ladder exposed and the one the first was masking.
          //
          // `zhao_terrain_lod`'s StEmit is INTERLEAVED PER SUBPATCH, not
          // tops-then-undersides (`:715-723`): for each `emit_idx` it emits the
          // TOP, then -- if `dual_i` -- the UNDERSIDE, and only then advances.
          // So a dual page emits TWO beats carrying ONE identity.
          //
          // Advancing on every beat popped twice per subpatch: the queue would
          // empty halfway through a dual patch, `ident_ok_c` would go false on
          // every remaining beat, `ident_mismatch_o` would fire eight times and
          // the FILE WOULD BE REFUSED -- so a dual page's record would never
          // complete, `ok()` would be false for it, and every seam it touches
          // would fall back. Silent, symmetric, and wrong.
          //
          // `lod_out_dual_i` is already on this block's port list, so the rule
          // needs no new wire: the last beat of a subpatch is the UNDERSIDE
          // when the page is dual and the TOP when it is not.
          if (idq_pop_c) idq_rd_q <= idq_rd_q + (IDQW+1)'(1);
          prep_decisions_o <= prep_decisions_o + 32'd1;
          if (lod_out_surface_i) prep_underside_o <= prep_underside_o + 32'd1;
        end else begin
          // The answer is dropped and the queue is NOT advanced: advancing it
          // on a mismatch would consume an identity that still belongs to a
          // descriptor in flight and turn one fault into a cascade.
          ident_mismatch_o <= ident_mismatch_o + 32'd1;
        end
      end

      if (out_fire_c && !prep_sel_i)
        emit_decisions_o <= emit_decisions_o + 32'd1;

      // ---- THE SUPPRESSION'S OWN WITNESS -----------------------------------
      if (h_valid_i && prep_sel_i)
        hist_leak_o <= hist_leak_o + 32'd1;
    end
  end

`ifndef SYNTHESIS
  // No `rst_n` term: SYNCASYNCNET, and the predicates depend on `prep_sel_i`
  // and the queue pointers, which reset clears. `zhao_terrain_tess.sv`'s
  // pattern.
  always_ff @(posedge clk) begin
    // THE PROPERTY, NOT THE DEFECT. This asserts that the history writeback is
    // silent while PREPARE owns the ladder -- which is what must be true after
    // the repair. The committed mutant proves the COUNTER can move; this
    // asserts the DESIGN does not need it to. CLAUDE.md: "do not write a test
    // that asserts the bug."
    // DISABLED IN THE MUTANT ONLY: unrelated to this mutation, but any
    // $fatal in the copy risks ending the run before the counter is read.
    // if (h_valid_o && prep_sel_i)
    //   $fatal(1, "lodshare: a history writeback escaped during PREPARE");
    // Both consumers may never be offered the same decision.
    // DISABLED IN THE MUTANT ONLY: with the full-guard removed the queue
    // pointers alias, and this predicate can trip on the resulting state
    // before `idq_overflow_o` has been read. The counter is what ships.
    // if (f_valid_o && e_out_valid_o)
    //   $fatal(1, "lodshare: one decision was offered to both consumers");
  end
`endif

endmodule

`default_nettype wire
