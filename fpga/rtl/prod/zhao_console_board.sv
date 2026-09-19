// zhao_console_board.sv -- THE BOARD FRAMEWORK around the connected machine.
//
//   reports/Zhaozhou_True_Console_Completion_Plan_2026-09-18.txt S1.3:
//     "zhao_console_core:  the connected machine, with platform transaction
//                          interfaces.
//      zhao_console_board: the selected real FPGA/board framework around that
//                          core."
//
// This is the second of those two tops. `zhao_console_core` takes clocks and
// resets as INPUTS; something has to make them, and S12.2 says what: "use
// vendor PLL primitives behind wrappers, with model equivalents in simulation.
// Generate documented GPU/video/audio clocks from the real oscillator. Assert
// reset on external reset/unlocked PLL as appropriate; synchronize deassertion
// in each domain." That is this file, and it is exactly SYS.PLL + SYS.RESET.
//
// The plan also sets the limit, and it is quoted here because it is the reason
// this file is short: "Keep the wrapper thin: exactly one clock/reset network,
// one physical memory owner..." and, two sections earlier, "SYS.PLL plus
// SYS.RESET alone do not complete the board boundary. [R23]" Both are true.
// This file is the clock/reset network and NOT the board boundary.
//
// ===========================================================================
// THE BOARD IT IS THE FRAMEWORK FOR -- MEASURED
// ===========================================================================
// `zhaozhou-board-bringup-20260913/reports/board_truth.json`, captured
// 2026-09-13 at commit eee32c4e, SuperStation One (RCSH-1001/1002):
//
//     FPGA_CLK1_50   50 MHz   pin V11   3.3-V LVTTL
//     FPGA_CLK2_50   50 MHz   pin Y13   3.3-V LVTTL
//     FPGA_CLK3_50   50 MHz   pin E11   3.3-V LVTTL
//     buildTarget    5CSEBA6U23I7, UFBGA-672, speed grade 7
//     reset          "MiSTer HPS/framework RESET, synchronized release after
//                     core PLL lock"
//
// NO PIN MAP IS INVENTED HERE. The pin names above are recorded as comments on
// the ports that carry them; the assignment itself belongs in a QSF that does
// not exist yet, and the completion plan S12.1 is explicit that "no pin map is
// invented here."
//
// ===========================================================================
// WHAT THIS FILE DELIBERATELY DOES NOT DO: INSTANTIATE zhao_console_core
// ===========================================================================
// STATE IT PLAINLY RATHER THAN LET A READER DISCOVER IT. The brief this file
// was written to asked for the core to be instantiated here. It is not, and
// the reason is a measurement rather than a preference:
//
//   `fpga/rtl/prod/zhao_console_core.sv` declared 687 ports when this pass
//   started and 732 forty minutes later.
//
// SystemVerilog has no port forwarding. Instantiating it means one of:
//
//   (a) 687 named connections, or
//   (b) `zhao_console_core u_core (.*);` -- which is what
//       `tests/prod/tb_zhao_console_core_smoke.sv` does, and which requires
//       687 identically-named declarations in the instantiating scope, or
//   (c) a generated pass-through block with a committed generator and a
//       staleness gate, the `gen_prod_top.py` pattern.
//
// (a) and (b) are a HAND COPY OF A MOVING FILE. CLAUDE.md has a chapter on
// exactly that -- "a committed mutant is a COPY, and a copy goes stale in the
// flattering direction": thirteen combiner copies and eight AUX-pipe copies
// went stale while staying GREEN, because a stale copy does not report that it
// is stale. A 687-port transcription inherits every word of it.
//
// And the file is not merely moving, it is being rewritten RIGHT NOW: two other
// agents own `zhao_console_core.sv` this session (terrain and small-block
// composition). CLAUDE.md, added 2026-09-19 after this very collision: "naming
// files in `git add` is not enough when two agents share one file."
//
// (c) is the correct long-term answer and it is deliberately NOT taken in this
// pass, because a generator run against a file under concurrent rewrite emits a
// snapshot that is stale within minutes and a staleness gate that goes red in
// someone else's lane.
//
// SO THE SEAM IS DECLARED AND NOT SOLDERED. Everything a board owes the core is
// a PORT of this module. Closing the seam is one instantiation once the core's
// port list settles, and the register movement it buys is ZERO either way:
// `tools/budget/completion_register.py` reads `zhao_console_core`'s closure in
// `design/fit_targets.yml`, never this top's.
//
// ---------------------------------------------------------------------------
// THREE PORT FACTS ABOUT THE CORE, REPORTED RATHER THAN EDITED AROUND
// ---------------------------------------------------------------------------
// The brief's rule was: if the core lacks a port you need, REPORT IT AND STOP
// rather than edit the file. Three such gaps exist, and they are the real work
// of closing this seam:
//
//   1. THE CORE TAKES ONE `rst_n`, THIS BLOCK PRODUCES FOUR. The core's
//      clock/reset group is exactly `gpu_clk`, `vid_clk`, `audio_clk`,
//      `rst_n` -- a single shared reset. SYS.RESET's whole point, and the
//      board's own proven contract ("synchronized release after core PLL
//      lock", per domain), is a release retimed INTO EACH DOMAIN. Wiring
//      `rst_n_gpu_o` to the core's one `rst_n` would hand the video and
//      audio domains a reset released on the GPU clock -- an unsynchronised
//      release into two domains, which is the defect the sequencer exists to
//      prevent. THE CORE NEEDS `rst_n_vid` AND `rst_n_audio` PORTS.
//
//   2. THE CORE HAS NO `sdram_clk`. SYS.PLL emits one and MEM.SDRAM.md
//      specifies the controller in it. The core's memory path currently lives
//      entirely in `gpu_clk`. Either the core gains the port or SDRAM stays
//      outside the core, and that is a composition decision, not this block's.
//
//   3. THE CORE NAMES IT `vid_clk`, THIS BLOCK NAMES IT `video_clk_o`.
//      `design/blocks.yml` gives SYS.PLL the output `video_clk`; the shell
//      lineage has said `vid_clk` since W2. Both names are correct in their
//      own document and one of them has to give at the seam. Flagged, not
//      silently renamed.
//
// And the core's port count is NOT STABLE while this is written: it read 687 at
// the start of this pass and 732 forty minutes later, which is the concurrent
// rewrite above, measured rather than asserted.
//
// ===========================================================================
// WHAT ELSE IS MISSING FROM "THE BOARD BOUNDARY", listed so its absence is not
// mistaken for coverage (completion plan S12.1 C36 is the full list):
// ===========================================================================
//   * pin assignments, I/O standards, drive strengths            -- no QSF
//   * an SDC. NOTHING here is constrained: not the three input clocks, not the
//     generated clocks, not the ref->domain crossings at the reset
//     synchronisers, not recovery/removal on any downstream reset flop
//   * the MiSTer sys/ framework, HPS transport and the three bridges
//   * SDRAM pins and calibration (MEM.SDRAM, ZH-004 seam)
//   * video output, audio output, controller input, SNAC
//   * `fpga/Zhaozhou.sv` is still the W1 placeholder and is untouched by this
//
// ===========================================================================
// NO FIT, NO MAP, NO BOARD.
// ===========================================================================
// This file has never been through `quartus_map` and has no fit row. CLAUDE.md:
// a block that has never been through quartus_map has not been shown to be
// synthesizable, however clean its lint. And the board carries an explicit
// review hold -- board_truth.json `reviewHold.futurePhysicalLoadsAuthorized:
// false`, seven prerequisites -- so nothing here has been, or may be, loaded.
// ===========================================================================

module zhao_console_board #(
    // Forwarded verbatim to SYS.PLL. Every one of them is documented at its
    // declaration in `fpga/rtl/sys/zhao_sys_pll.sv`, with its MEASURED /
    // DERIVED / UNSETTLED tag and the evidence for it. They are repeated here
    // as overridable parameters and NOT re-documented, because two copies of a
    // justification is how one of them goes stale.
    parameter int unsigned REF_CLK_HZ         = 50_000_000,
    parameter int unsigned GPU_CLK_HZ         = 100_000_000,
    parameter int unsigned SDRAM_CLK_HZ       = 100_000_000,
    parameter int unsigned VIDEO_CLK_HZ       = 0,             // UNSETTLED
    parameter int unsigned AUDIO_CLK_HZ       = 12_288_000,
    parameter int unsigned SIM_GPU_DIV        = 1,
    parameter int unsigned SIM_SDRAM_DIV      = 1,
    parameter int unsigned SIM_VIDEO_DIV      = 2,
    parameter int unsigned SIM_AUDIO_DIV      = 4,
    parameter int unsigned SIM_LOCK_CYCLES    = 8,
    parameter int unsigned LOST_W             = 16,

    // Forwarded verbatim to SYS.RESET.
    parameter int unsigned RELEASE_SPAN       = 16,
    parameter int unsigned RELEASE_STEP_SDRAM = 0,
    parameter int unsigned RELEASE_STEP_GPU   = 3,
    parameter int unsigned RELEASE_STEP_VIDEO = 6,
    parameter int unsigned RELEASE_STEP_AUDIO = 9,
    parameter int unsigned SYNC_STAGES        = 2,
    parameter int unsigned ASSERT_W           = 16
) (
    // ---- board pins -------------------------------------------------------
    // MEASURED, board_truth.json clocks.fpgaInputs. The pin names are recorded
    // here; the ASSIGNMENT belongs in a QSF that does not exist.
    input  logic fpga_clk1_50_i,     // 50 MHz, pin V11, 3.3-V LVTTL
    input  logic fpga_clk2_50_i,     // 50 MHz, pin Y13, 3.3-V LVTTL
    input  logic fpga_clk3_50_i,     // 50 MHz, pin E11, 3.3-V LVTTL

    // MEASURED as a CONTRACT, not as a pin: board_truth.json reset.coreContract
    // "MiSTer HPS/framework RESET, synchronized release after core PLL lock".
    // reset.physicalButtonMapping is null and `physical_button_reset_mapping`
    // is still in openCapabilities -- WHICH pin drives this is unanswered.
    input  logic hard_reset_n_i,

    // The vendor PLL's own asynchronous, active-high reset. See SYS.PLL.md: it
    // is a real port and it is also the only legal stimulus that fires
    // pll_lock_lost_o.
    input  logic pll_arst_i,

    // ---- the clock/reset network this framework exists to make ------------
    // These are the seam `zhao_console_core` attaches to.
    output logic gpu_clk_o,
    output logic sdram_clk_o,
    output logic video_clk_o,
    output logic audio_clk_o,
    output logic pll_locked_o,

    output logic rst_n_gpu_o,
    output logic rst_n_sdram_o,
    output logic rst_n_video_o,
    output logic rst_n_audio_o,

    // ---- the platform census ----------------------------------------------
    output logic                seq_done_o,
    output logic [LOST_W-1:0]   pll_lock_lost_o,
    output logic [ASSERT_W-1:0] reset_assertions_o
);

  // ---------------------------------------------------------------------------
  // THE CONSOLE USES ONE REFERENCE.
  //
  // FPGA_CLK1_50 feeds the PLL and the reset sequencer's stagger counter. CLK2
  // and CLK3 are real board pins and are carried here so that this module
  // describes the board it is the framework for -- but a second PLL on a second
  // oscillator would be a SECOND FREQUENCY AUTHORITY, which is the one thing
  // SYS.PLL's contract exists to forbid. They are explicitly sunk rather than
  // dropped from the port list, so that "the console ignores two of the three
  // oscillators" is a visible decision instead of an absent line.
  // ---------------------------------------------------------------------------
  wire unused_board_clk_w = &{1'b0, fpga_clk2_50_i, fpga_clk3_50_i};

  zhao_sys_pll #(
      .REF_CLK_HZ      (REF_CLK_HZ),
      .GPU_CLK_HZ      (GPU_CLK_HZ),
      .SDRAM_CLK_HZ    (SDRAM_CLK_HZ),
      .VIDEO_CLK_HZ    (VIDEO_CLK_HZ),
      .AUDIO_CLK_HZ    (AUDIO_CLK_HZ),
      .SIM_GPU_DIV     (SIM_GPU_DIV),
      .SIM_SDRAM_DIV   (SIM_SDRAM_DIV),
      .SIM_VIDEO_DIV   (SIM_VIDEO_DIV),
      .SIM_AUDIO_DIV   (SIM_AUDIO_DIV),
      .SIM_LOCK_CYCLES (SIM_LOCK_CYCLES),
      .LOST_W          (LOST_W)
  ) u_pll (
      .ref_clk_i       (fpga_clk1_50_i),
      // The board hard reset also clears the PLL block's housekeeping. It does
      // NOT reset the PLL itself -- that is pll_arst_i, and keeping the two
      // apart is what lets pll_lock_lost_o survive the event it counts.
      .arst_n_i        (hard_reset_n_i),
      .pll_arst_i      (pll_arst_i),
      .gpu_clk_o       (gpu_clk_o),
      .sdram_clk_o     (sdram_clk_o),
      .video_clk_o     (video_clk_o),
      .audio_clk_o     (audio_clk_o),
      .pll_locked_o    (pll_locked_o),
      .pll_lock_lost_o (pll_lock_lost_o)
  );

  zhao_sys_reset #(
      .RELEASE_SPAN       (RELEASE_SPAN),
      .RELEASE_STEP_SDRAM (RELEASE_STEP_SDRAM),
      .RELEASE_STEP_GPU   (RELEASE_STEP_GPU),
      .RELEASE_STEP_VIDEO (RELEASE_STEP_VIDEO),
      .RELEASE_STEP_AUDIO (RELEASE_STEP_AUDIO),
      .SYNC_STAGES        (SYNC_STAGES),
      .ASSERT_W           (ASSERT_W)
  ) u_reset (
      // The stagger counter runs on the REFERENCE, not on gpu_clk: it has to
      // be counting while the PLL is unlocked, which is the whole situation it
      // exists to come out of.
      .ref_clk_i          (fpga_clk1_50_i),
      .pll_locked_i       (pll_locked_o),
      .hard_reset_n_i     (hard_reset_n_i),
      .gpu_clk_i          (gpu_clk_o),
      .sdram_clk_i        (sdram_clk_o),
      .video_clk_i        (video_clk_o),
      .audio_clk_i        (audio_clk_o),
      .rst_n_gpu_o        (rst_n_gpu_o),
      .rst_n_sdram_o      (rst_n_sdram_o),
      .rst_n_video_o      (rst_n_video_o),
      .rst_n_audio_o      (rst_n_audio_o),
      .seq_done_o         (seq_done_o),
      .reset_assertions_o (reset_assertions_o)
  );

  // ---------------------------------------------------------------------------
  // THE SEAM.
  //
  //   zhao_console_core u_core (
  //       .gpu_clk   (gpu_clk_o),
  //       .vid_clk   (video_clk_o),
  //       .audio_clk (audio_clk_o),
  //       .rst_n     (rst_n_gpu_o),   // <- see port fact 1: WRONG for two domains
  //       ... ~728 more ...
  //   );
  //
  // Not written. See the file header for why, and for what closing it costs.
  // The ports above are named so that closing it is an instantiation and not a
  // redesign.
  // ---------------------------------------------------------------------------

endmodule
