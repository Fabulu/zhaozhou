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
// THE THREE PORT FACTS, NOW RESOLVED -- 2026-09-19
// ---------------------------------------------------------------------------
// An earlier revision of this header listed three port gaps on the core and
// stopped. All three have now been chased to their source. TWO OF THEM WERE
// NOT WHERE THIS FILE SAID THEY WERE, and the correction is recorded here
// rather than by quietly rewriting the list, because the wrong diagnosis is
// what the next packet would otherwise have acted on.
//
//   1. THE `rst_n_vid` / `rst_n_audio` GAP IS NOT IN THE CORE.
//
//      The observation was right: this block produces four resets, the core
//      takes one, and handing `rst_n_gpu_o` to it releases the video and audio
//      domains on the GPU clock -- exactly what SYS.RESET exists to prevent.
//      The CONCLUSION drawn from it ("the core needs two more ports") was
//      wrong, and measuring the core says why:
//
//        * `zhao_console_core.sv` contains exactly FOUR occurrences of
//          `vid_clk`/`audio_clk`: the two port declarations, and the two
//          connections to `u_shell`. Nothing else in the file mentions them.
//        * EVERY `always_ff` in the core is `@(posedge gpu_clk or negedge
//          rst_n)` -- four of four, no exceptions.
//
//      The core has no video or audio domain of its own. Its single vid/audio
//      consumer is `u_shell`, and `zhao_shell_top_v2.sv` declares exactly one
//      `rst_n`. So two new core ports would terminate nowhere -- and that is
//      worse than useless, it is ACTIVELY MISLEADING: a board author would
//      wire `rst_n_video_o` to `rst_n_vid`, and believe video was correctly
//      released, while the shell went on resetting its video flops from the
//      one shared net. A SEAM THAT ACCEPTS A PER-DOMAIN RESET AND DROPS IT IS
//      A LIE. The core's single `rst_n` is honest: it says "this core has one
//      reset domain", and that is true.
//
//      THE DEFECT IS REAL AND IT IS ONE LEVEL DOWN, and it is an uncashed
//      cheque in this repository's exact sense -- the per-domain reset ports
//      ALREADY EXIST on the shell's own children, and the shell feeds every
//      one of them the same net:
//
//        zhao_shell_top_v2.sv  .gpu_rst_n(rst_n), .vid_rst_n(rst_n)  u_fb_cdc
//                              .gpu_rst_n(rst_n), .vid_rst_n(rst_n)  u_ready_bridge
//                              .src_rst_n(rst_n), .dst_rst_n(rst_n)  u_starve_mbx
//                              .rst_audio_n(rst_n)                   audio fifo
//
//      plus the pure vid-domain `u_mode`, `u_scaler`, `u_framectl`, `u_crc`
//      and three vid-domain `always_ff` blocks, all on `rst_n`. Somebody built
//      the split and nobody ever drove it.
//
//      So the fix is a `zhao_shell_top_v2` packet -- two new shell ports, about
//      a dozen rewires, and a reset-port SPLIT in `zhao_video_scanout`, which
//      is genuinely dual-domain (it clocks flops on both `gpu_clk` and
//      `vid_clk` off one `rst_n`). The core and this board then follow in two
//      lines each. It is not done in this pass because changing the shell's
//      declaration invalidates `shell-declaration-sha256` in
//      `fpga/rtl/generated/zhao_shell_v2_fit_top.sv` and trips
//      `shell_v2_fit_generated_freshness`, `gen_shell_fit_ports_v2.py --check`,
//      `gen_shell_paired_diff.py --check`, `test_shell_fit_preflight.py` and
//      the shell lint targets -- a gate set this packet is not permitted to
//      run. RTL whose gates you may not run is an unmeasured claim.
//
//   2. `sdram_clk` DOES NOT BELONG ON THE CORE YET, AND THE REASON IS NOT
//      "NOBODY ASKED FOR IT".
//
//      The premise was wrong in a useful way: MEM.SDRAM is not outside the
//      core at all. `zhao_sdram_ctrl` is instantiated INSIDE it, by the shell,
//      as `.clk(gpu_clk), .rst_n(rst_n)`. So the real question is not "does
//      the core need the port" but "does the SDRAM controller move into its
//      own domain", and today it does not:
//
//        * this board's own parameters set SDRAM_CLK_HZ == GPU_CLK_HZ ==
//          100 MHz, so a separate clock buys nothing the design lacks;
//        * moving it demands a real CDC between `zhao_vram_arbiter` (gpu_clk)
//          and the controller, and `zhao_sdram_ctrl`'s FROZEN LAW TABLE is a
//          cycle-exact contract mirrored by `zref::SdramController` and
//          `sim/models/zhao_sdram_model.sv`. A CDC re-times every span in it;
//        * `design/blocks.yml` holds MEM.SDRAM at maturity SPECIFIED,
//          blocked_on: hardware, and MEM.SDRAM.md says the block "never leaves
//          SPECIFIED -- evidence banked, advancement gated on the hardware
//          lane" until ZH-004 delivers device code, speed grade and measured
//          tRCD/tRP/tRC;
//        * and the PHY pins (`phy_cs_n`, `phy_a`, `phy_dq_*`, ...) do not reach
//          the core boundary at all. They would have to become board ports and
//          then QSF assignments -- and there is no QSF, and board_truth.json
//          holds futurePhysicalLoadsAuthorized: false.
//
//      So: NO `sdram_clk` PORT ON THE CORE. It would be a port nothing uses,
//      added ahead of the decision that would give it meaning. `sdram_clk_o`
//      stays an output of this block, available the day MEM.SDRAM's domain
//      question is actually answered.
//
//   3. BOTH NAMES STAY. THE SEAM IS ALLOWED TWO NAMES; A FILE IS NOT ALLOWED
//      TO RENAME ANOTHER FILE'S PORT.
//
//      * The PRODUCER side keeps `video_clk`. `design/blocks.yml` gives SYS.PLL
//        `outputs: [gpu_clk, sdram_clk, video_clk, audio_clk, pll_locked]`, and
//        the ledger is the authority on a block's declared ports. `zhao_sys_pll`
//        and this top keep `video_clk_o`.
//      * The CONSUMER side keeps `vid_clk`. `zhao_console_core.sv` carries
//        `zhao_shell_top_v2`'s declaration through VERBATIM by its own stated
//        rule -- "this core neither renames nor reinterprets any of them" --
//        and the shell has said `vid_clk` since W2. Renaming it at the core
//        would make the core a renaming layer and would break `u_shell`'s
//        connection, the smoke bench's `.*`, and the mutant wrapper.
//
//      AUTHORITY FOLLOWED: `design/blocks.yml` on the producer, the shell
//      lineage on the consumer. Neither document gives, because neither has to:
//      the seam is one explicit connection, `.vid_clk (video_clk_o)`, in the
//      instantiation below. What was feared as a rename is simply not required.
//
//      This does, however, settle the instantiation question -- see next.
//
// ---------------------------------------------------------------------------
// AND SO `.*` IS NOT AVAILABLE, WHICH IS A FACT AND NOT A PREFERENCE
// ---------------------------------------------------------------------------
// `zhao_console_core u_core (.*);` binds each port to an identically-named net
// in this scope. Per fact 3 the four clock/reset ports deliberately do NOT
// match (`gpu_clk`/`gpu_clk_o`, `vid_clk`/`video_clk_o`, `audio_clk`/
// `audio_clk_o`, `rst_n`/`rst_n_gpu_o`), and that mismatch is CORRECT and
// permanent rather than an accident to be tidied away. So `.*` would still
// require ~730 identically-named declarations in this scope plus explicit
// overrides for the four -- the hand copy, with extra steps.
//
// And the file is still moving while this is written, measured rather than
// asserted: `zhao_console_core.sv` read 4,266 lines and then 4,474 within one
// session, gaining eleven `part_tbl_*` ports; `design/fit_targets.yml` and
// `tests/prod/tb_zhao_console_core_smoke.sv` both changed underneath a
// measurement that was in progress at the time.
//
// THE SEAM THEREFORE STAYS DECLARED AND NOT SOLDERED -- and after this pass the
// only thing still standing between it and one instantiation is item 1, which
// is a shell packet, not a board one.
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
  //       .vid_clk   (video_clk_o),    // <- fact 3: two names, one connection
  //       .audio_clk (audio_clk_o),
  //       .rst_n     (rst_n_gpu_o),    // <- fact 1: CORRECT ONLY for gpu today,
  //                                    //    and the core honestly claims no
  //                                    //    more than that. Video and audio are
  //                                    //    reset from this net INSIDE the
  //                                    //    shell; splitting them is a
  //                                    //    zhao_shell_top_v2 packet.
  //       ... ~730 more ...
  //   );
  //
  // Not written. `.*` is unavailable (see the header), and the header also says
  // what the remaining blocker is: it is item 1, and it lives in the shell.
  // Note the three clock connections above are already RIGHT -- the naming
  // question is settled and cost nothing. The ports on this module are named so
  // that closing the seam is an instantiation and not a redesign.
  // ---------------------------------------------------------------------------

endmodule
