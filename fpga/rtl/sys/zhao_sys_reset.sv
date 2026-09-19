// zhao_sys_reset.sv -- SYS.RESET, the console's ONE reset network.
// Contract: design/contracts/SYS.RESET.md. Ledger: design/blocks.yml, SYS.RESET.
//
// ===========================================================================
// WHY THIS FILE EXISTS NOW AND NOT BEFORE
// ===========================================================================
// SYS.RESET was `blocked_on: hardware` from 2026-08-31 to 2026-09-13, citing
// owner ruling section 8: "no owner action. They wait for the board."
//
// THE BOARD HAS BEEN PROBED, and this block's answer was in the capture:
//
//     board_truth.json  reset.coreContract:
//       "MiSTer HPS/framework RESET, synchronized release after core PLL lock"
//     board_truth.json  reset.volatileLoadResetSequenceStatus: "proven"
//
// That sentence IS the structure below. This block is not inventing a reset
// scheme; it implements the one the board has already been shown to accept.
// The reference clock the stagger runs on is FPGA_CLK1_50 -- 50 MHz, pin V11,
// 3.3-V LVTTL, MEASURED, board_truth.json clocks.fpgaInputs.
//
// WHAT THE CAPTURE DOES NOT SAY, and is therefore not claimed anywhere here:
// `reset.physicalButtonMapping` is null and `physical_button_reset_mapping`
// remains in openCapabilities. WHICH PIN reaches hard_reset_n_i is a board
// framework question with no answer yet.
//
// ===========================================================================
// THE SHAPE, IN THREE SENTENCES
// ===========================================================================
//  1. ASSERTION IS ASYNCHRONOUS. `hard_reset_n_i & pll_locked_i` falling drives
//     every rst_n_* low with NO CLOCK REQUIRED. That is the safety direction: a
//     domain whose clock has stopped because the PLL unlocked cannot be reset
//     synchronously, and that is precisely when it most needs to be.
//  2. RELEASE IS SYNCHRONOUS AND PER DOMAIN. Each domain carries its own
//     SYNC_STAGES-deep shift register clocked by THAT DOMAIN'S clock, so
//     metastability on the release edge is resolved where it is consumed.
//  3. RELEASE IS STAGGERED, and the stagger lives in ONE bounded counter in the
//     ref_clk_i domain -- a clock that runs whether or not the PLL is locked.
//
// This block does NOT reset anything's CONTENTS. SDRAM's precharge-all init,
// VIDEO.MODE's x=0/y=0/VIDEO_Z60, AUDIO.FIFO's empty-and-silent are each
// block's own obligation on seeing its rst_n low. Completion plan section 12.2:
// "a synchronized reset release is not a memory-drain certificate."
//
// ===========================================================================
// THE ORDER IS A JUDGEMENT AND IT IS FOUR EDITABLE CONSTANTS
// ===========================================================================
// SDRAM, then GPU, then VIDEO, then AUDIO. Each step cites the consuming
// contract's own reset clause (see the parameter comments), and NONE of the
// four blocks requires it: every one is specified to be safe from its own reset
// regardless of the others. Setting all four steps to 0 is a legal
// configuration and the bench proves the block still behaves -- which is the
// test that keeps this from being a hidden law.
// ===========================================================================

/* verilator lint_off DECLFILENAME */
// ---------------------------------------------------------------------------
// zhao_sys_reset_sync -- ONE domain's async-assert / sync-release flop chain.
//
// A helper module rather than four copies, because four copies is how three of
// them end up one edit behind the fourth. The shift is written as a procedural
// loop rather than a concatenated slice so that STAGES == 1 does not elaborate
// an illegal [-1:0] range; a parameter value that produces a compile error two
// modules away is a worse diagnostic than one that just works.
// ---------------------------------------------------------------------------
module zhao_sys_reset_sync #(
    parameter int unsigned STAGES = 2
) (
    input  logic dom_clk_i,
    input  logic arst_n_i,      // asynchronous, active low: asserts with no clock
    input  logic release_i,     // level from the ref_clk_i stagger counter
    output logic rst_n_o
);

  logic [STAGES-1:0] sr_q;

  always_ff @(posedge dom_clk_i or negedge arst_n_i) begin
    if (!arst_n_i) begin
      sr_q <= '0;
    end else begin
      for (int unsigned s = STAGES - 1; s > 0; s--) begin
        sr_q[s] <= sr_q[s-1];
      end
      sr_q[0] <= release_i;
    end
  end

  assign rst_n_o = sr_q[STAGES-1];

endmodule
/* verilator lint_on DECLFILENAME */


module zhao_sys_reset #(
    // ------------------------------------------------------------------
    // THE BOUND. design/blocks.yml gives SYS.RESET `latency:
    // variable_bounded:16`, and this is that 16, read as the SEQUENCER's
    // bound in ref_clk_i cycles.
    //
    // THE TOTAL IS NOT 16 AND SAYING SO MATTERS: it is 16 ref_clk_i cycles
    // for the sequencer PLUS SYNC_STAGES cycles of each domain's own clock
    // for that domain's release. Quoting the first alone would be a number
    // that is true about the wrong thing.
    //
    // "Variable" means the WAIT, not the schedule. Given arst_n rising at
    // ref cycle 0, step k releases at cycle k+1, fixed. The variability is
    // that arst_n's rise is an EVENT -- it waits on pll_locked_i, whose lock
    // time is UNSETTLED (SYS.PLL.md, Latency). 16 bounds everything after it.
    // ------------------------------------------------------------------
    parameter int unsigned RELEASE_SPAN       = 16,

    // ------------------------------------------------------------------
    // THE ORDER. Domain X releases on the first ref_clk_i cycle where the
    // stagger counter EXCEEDS its step, i.e. at cycle STEP+1 after arst_n
    // rises (strict >, so a step of 0 is a real comparison and not a
    // constant-true one -- Verilator's UNSIGNED warning caught the >=`n    // version, and suppressing it would have hidden a level that could never
    // fall). DERIVED (ordering)
    // from what each domain does at reset; not measured, and deliberately
    // four separate knobs.
    //
    // SDRAM first -- MEM.SDRAM.md: on reset the controller runs
    //   PRECHARGE-ALL + 2x AUTO_REFRESH + MODE REGISTER SET *before any
    //   client traffic*. Longest thing to do, and everyone reads through it.
    // GPU next -- so render/compute clients come up into a memory system
    //   that has already started its init.
    // VIDEO next -- spec/video_rules.md section 4: with no complete frame the
    //   raster displays black and repeats. A defined state that depends on
    //   nobody.
    // AUDIO last -- AUDIO.FIFO.md: reset leaves the FIFO empty and the output
    //   silent ("zero pairs, NOT repeats -- there is no last pair yet").
    //   Releasing it last minimises the window in which it runs starved.
    // ------------------------------------------------------------------
    parameter int unsigned RELEASE_STEP_SDRAM = 0,
    parameter int unsigned RELEASE_STEP_GPU   = 3,
    parameter int unsigned RELEASE_STEP_VIDEO = 6,
    parameter int unsigned RELEASE_STEP_AUDIO = 9,

    // The standard two-flop release synchroniser, per domain.
    parameter int unsigned SYNC_STAGES        = 2,

    // Width of the assertion census. It SATURATES; see below.
    parameter int unsigned ASSERT_W           = 16
) (
    // FPGA_CLK1_50, 50 MHz, pin V11 -- MEASURED. Runs the stagger counter and
    // the census, and keeps running when the PLL does not.
    input  logic ref_clk_i,

    // From SYS.PLL. ASYNCHRONOUS -- SYS.PLL.md says so explicitly. It is used
    // directly as an async ASSERT (safe by construction) and separately
    // two-flop synchronised for the census, which is the only place its VALUE
    // is read.
    input  logic pll_locked_i,

    // The board hard reset, asynchronous, active low. board_truth.json
    // reset.coreContract: "MiSTer HPS/framework RESET".
    input  logic hard_reset_n_i,

    // The four domain clocks, from SYS.PLL. design/blocks.yml does not list
    // them and could not: a SYNCHRONOUS release requires the clock it is
    // synchronous to.
    input  logic gpu_clk_i,
    input  logic sdram_clk_i,
    input  logic video_clk_i,
    input  logic audio_clk_i,

    output logic rst_n_gpu_o,
    output logic rst_n_sdram_o,
    output logic rst_n_video_o,
    output logic rst_n_audio_o,

    // ref_clk_i domain. Exists so that the bounded-latency law above is
    // OBSERVABLE rather than merely asserted in prose.
    output logic seq_done_o,

    // Census, ref_clk_i domain, SATURATING. See the block at the bottom for
    // exactly what it counts and what resets it, which is the whole question.
    output logic [ASSERT_W-1:0] reset_assertions_o
);

  // -------------------------------------------------------------------------
  // ELABORATION GUARDS.
  //
  // Inside `initial begin ... end` because Quartus 17.0 rejects a bare
  // module-scope `if (...) $fatal(...)`. AND `--lint-only` DOES NOT RUN THIS
  // BLOCK -- a clean lint is not evidence that any guard below fires. Both
  // facts are recorded in CLAUDE.md's Build note.
  //
  // A step at or beyond RELEASE_SPAN is the defect worth refusing: the counter
  // saturates at RELEASE_SPAN-1 and the test is strict, so such a domain would NEVER BE RELEASED AT
  // ALL and the machine would look dead for a reason nothing reports.
  // -------------------------------------------------------------------------
  initial begin
    if (RELEASE_SPAN < 2)
      $fatal(1, "zhao_sys_reset: RELEASE_SPAN < 2 leaves no room to stagger.");
    if (RELEASE_STEP_SDRAM >= (RELEASE_SPAN - 1))
      $fatal(1, "zhao_sys_reset: RELEASE_STEP_SDRAM >= RELEASE_SPAN-1; the counter saturates there, so sdram would never release.");
    if (RELEASE_STEP_GPU >= (RELEASE_SPAN - 1))
      $fatal(1, "zhao_sys_reset: RELEASE_STEP_GPU >= RELEASE_SPAN-1; the counter saturates there, so gpu would never release.");
    if (RELEASE_STEP_VIDEO >= (RELEASE_SPAN - 1))
      $fatal(1, "zhao_sys_reset: RELEASE_STEP_VIDEO >= RELEASE_SPAN-1; the counter saturates there, so video would never release.");
    if (RELEASE_STEP_AUDIO >= (RELEASE_SPAN - 1))
      $fatal(1, "zhao_sys_reset: RELEASE_STEP_AUDIO >= RELEASE_SPAN-1; the counter saturates there, so audio would never release.");
    if (SYNC_STAGES < 2)
      $fatal(1, "zhao_sys_reset: SYNC_STAGES < 2 is not a release synchroniser.");
    if (ASSERT_W < 2)
      $fatal(1, "zhao_sys_reset: ASSERT_W < 2 leaves no room for a saturating census.");
  end

  localparam int unsigned SPANW = (RELEASE_SPAN <= 1) ? 1 : $clog2(RELEASE_SPAN);

  // -------------------------------------------------------------------------
  // THE ASYNCHRONOUS ASSERT.
  //
  // One combinational AND, and it is the whole safety argument: neither term
  // passes through a flop, so either falling takes every output down with no
  // clock edge anywhere. Directed test 5 holds all five clocks STATIC and
  // drops pll_locked_i -- a synchronous-assert implementation passes every
  // other test in the bench and fails that one.
  // -------------------------------------------------------------------------
  logic arst_n_c;
  assign arst_n_c = hard_reset_n_i & pll_locked_i;

  // -------------------------------------------------------------------------
  // THE STAGGER. One saturating counter in ref_clk_i, async-reset by arst_n_c.
  //
  // SATURATION IS THE CORRECTNESS ARGUMENT, not an optimisation. If this
  // counter wrapped, every `step_q > STEP` level would fall again and a
  // RELEASED DOMAIN WOULD BE RE-ASSERTED with no reset event -- the exact fault
  // the formal property `no_glitch_on_release` is written for.
  // -------------------------------------------------------------------------
  logic [SPANW-1:0] step_q;

  always_ff @(posedge ref_clk_i or negedge arst_n_c) begin
    if (!arst_n_c) begin
      step_q <= '0;
    end else if (step_q != SPANW'(RELEASE_SPAN - 1)) begin
      step_q <= step_q + SPANW'(1);
    end
  end

  assign seq_done_o = (step_q == SPANW'(RELEASE_SPAN - 1));

  logic rel_sdram_c;
  logic rel_gpu_c;
  logic rel_video_c;
  logic rel_audio_c;

  assign rel_sdram_c = (step_q > SPANW'(RELEASE_STEP_SDRAM));
  assign rel_gpu_c   = (step_q > SPANW'(RELEASE_STEP_GPU));
  assign rel_video_c = (step_q > SPANW'(RELEASE_STEP_VIDEO));
  assign rel_audio_c = (step_q > SPANW'(RELEASE_STEP_AUDIO));

  // -------------------------------------------------------------------------
  // THE FOUR DOMAINS. Same helper, four clocks. arst_n_c reaches each one
  // asynchronously; the release level is retimed into the consuming domain.
  //
  // ref_clk_i -> each domain is a REAL asynchronous crossing at these flops,
  // and it is the only place in this block that is one. An SDC must
  // set_false_path the LEVEL here and nowhere else -- completion plan section
  // 12.2: "cross-domain false paths are permitted only at actual CDC
  // structures, not to erase a real slow synchronous path." NO SDC EXISTS YET.
  // -------------------------------------------------------------------------
  zhao_sys_reset_sync #(.STAGES(SYNC_STAGES)) u_sync_sdram (
      .dom_clk_i (sdram_clk_i),
      .arst_n_i  (arst_n_c),
      .release_i (rel_sdram_c),
      .rst_n_o   (rst_n_sdram_o));

  zhao_sys_reset_sync #(.STAGES(SYNC_STAGES)) u_sync_gpu (
      .dom_clk_i (gpu_clk_i),
      .arst_n_i  (arst_n_c),
      .release_i (rel_gpu_c),
      .rst_n_o   (rst_n_gpu_o));

  zhao_sys_reset_sync #(.STAGES(SYNC_STAGES)) u_sync_video (
      .dom_clk_i (video_clk_i),
      .arst_n_i  (arst_n_c),
      .release_i (rel_video_c),
      .rst_n_o   (rst_n_video_o));

  zhao_sys_reset_sync #(.STAGES(SYNC_STAGES)) u_sync_audio (
      .dom_clk_i (audio_clk_i),
      .arst_n_i  (arst_n_c),
      .release_i (rel_audio_c),
      .rst_n_o   (rst_n_audio_o));

  // -------------------------------------------------------------------------
  // THE ASSERTION CENSUS, and WHAT RESETS IT IS THE WHOLE QUESTION.
  //
  // It counts falling edges of pll_locked_i as observed through this block's
  // own two-flop synchroniser -- i.e. every time the reset network goes from
  // RELEASED back to ASSERTED because the PLL dropped lock.
  //
  // IT IS RESET BY hard_reset_n_i AND NEVER BY pll_locked_i. That separation is
  // the point. A census cleared by its own trigger is CLAUDE.md's "detector
  // wired to two operands that move together": the event and the clear arrive
  // together, the counter reads a reassuring zero, and the zero is then quoted
  // as evidence that the thing it cannot see is fine.
  //
  // Consequence, stated so nobody has to discover it: pulsing hard_reset_n_i
  // CLEARS this census rather than incrementing it. That is not the counter
  // failing to see an assertion -- it is the census being scoped to "losses
  // since power-on", and directed test 7 asserts exactly this asymmetry so that
  // the two resets are demonstrably different resets.
  //
  // The two sides of the comparison -- lock_s_q[1] and lock_s_q[0] -- are
  // ADJACENT STAGES of one shift register, one ref_clk_i cycle apart BY
  // CONSTRUCTION. That makes it an edge detector with no shared enable that
  // could corrupt both in lockstep.
  //
  // The power-on interval before the first lock is not a LOSS and is not
  // counted; there is no released state before it to transition from.
  //
  // It SATURATES. A wrapping census can read zero after 2**ASSERT_W events,
  // which is the flattering direction.
  // -------------------------------------------------------------------------
  logic [1:0] lock_s_q;

  always_ff @(posedge ref_clk_i or negedge hard_reset_n_i) begin
    if (!hard_reset_n_i) begin
      lock_s_q           <= 2'b00;
      reset_assertions_o <= '0;
    end else begin
      lock_s_q <= {lock_s_q[0], pll_locked_i};
      if (lock_s_q[1] && !lock_s_q[0] &&
          (reset_assertions_o != {ASSERT_W{1'b1}}))
        reset_assertions_o <= reset_assertions_o + ASSERT_W'(1);
    end
  end

endmodule
