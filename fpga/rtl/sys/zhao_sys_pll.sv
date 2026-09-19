// zhao_sys_pll.sv -- SYS.PLL, the console's ONE frequency authority.
// Contract: design/contracts/SYS.PLL.md. Ledger: design/blocks.yml, SYS.PLL.
//
// ===========================================================================
// WHY THIS FILE EXISTS NOW AND NOT BEFORE
// ===========================================================================
// SYS.PLL was `blocked_on: hardware` from 2026-08-31 to 2026-09-13, citing the
// owner's ruling section 8: "no owner action. They wait for the board." That was
// correct. A PLL wrapper written before the device exists is a guess wearing a
// specification's clothes -- it needs a reference frequency, a pin, an I/O
// standard and a part number, and every one of those is a property of a
// physical object.
//
// THE BOARD HAS BEEN PROBED. `zhaozhou-board-bringup-20260913/reports/
// board_truth.json`, captured 2026-09-13 at commit eee32c4e, records:
//
//     FPGA_CLK1_50   50 MHz   pin V11   3.3-V LVTTL   <- this block's reference
//     FPGA_CLK2_50   50 MHz   pin Y13   3.3-V LVTTL
//     FPGA_CLK3_50   50 MHz   pin E11   3.3-V LVTTL
//     buildTarget    5CSEBA6U23I7, UFBGA-672, speed grade 7
//
// The ruling's condition is met and `design/blocks.yml` records the strike.
//
// ===========================================================================
// EVERY NUMBER IS TAGGED, AND EVERY NUMBER IS A KNOB
// ===========================================================================
// MEASURED  -- board_truth.json states it and names its evidence.
// DERIVED   -- computed here from a MEASURED number plus a ratified spec.
// UNSETTLED -- the tree does not contain the answer and this file refuses to
//              invent one.
//
// CLAUDE.md's art law rule 6 -- "never remove the owner's control in the name
// of fidelity" -- applies to a clock frequency exactly as it applies to a
// creature's radius. A frequency derived from board truth is STILL A KNOB, so
// every one of them is a named parameter below and none is welded into logic.
//
// ===========================================================================
// THE VENDOR IP, AND WHAT THAT LEAVES UNVERIFIED
// ===========================================================================
// A Cyclone V PLL is a hard block reached through `altera_pll`. VERILATOR
// CANNOT ELABORATE IT. So this file carries two bodies behind a plain `ifdef`:
//
//   ZHAO_SYS_PLL_VENDOR defined   -> altera_pll. Quartus only.
//   ZHAO_SYS_PLL_VENDOR undefined -> a behavioural divider + lock model.
//                                    This is what every bench in this
//                                    repository compiles. IT IS THE DEFAULT.
//
// A PLAIN `ifdef` SELECTING BETWEEN TWO BODIES IS DELIBERATE. CLAUDE.md records
// that Verilator's command-line `-D` cannot override a FUNCTION-LIKE `define`
// and says nothing when it fails to -- two mutants passed while measuring
// unmutated production. `ifdef` is the shape `-D` reaches.
//
// SAY IT WITHOUT SOFTENING: THE REAL IP IS UNVERIFIED HERE. The vendor branch
// has never been linted, simulated, mapped, fitted or loaded. Its parameter
// strings and the frequencies it actually achieves are the first thing the
// first `quartus_map` will contradict. CLAUDE.md: a block that has never been
// through quartus_map has not been shown to be synthesizable, however clean its
// lint. Nothing in this repository's test results says anything whatever about
// altera_pll.
//
// ===========================================================================
// THE ONE NUMBER THIS FILE WILL NOT GUESS: gpu_clk : video_clk
// ===========================================================================
// `spec/video_rules.md` says `vid_clk = gpu_clk / 2` and scopes it, in the same
// sentence, to "simulation and the frozen sim profile"; `zhao_pkg.sv`'s
// ZHAO_VID_CYCLES_PER_GPU = 2 carries the same scope in its own comment. Both
// defer the hardware ratio to ZH-016.
//
// Ratio 2 CANNOT be the hardware ratio. The raster is 60 Hz in every mode
// (audio_rules.md section 1 pins it: exactly 800 pairs per frame = 48000/60),
// so the pixel rate is h_total * 262 * 60 -- 7.5456 / 6.5395 / 9.5578 MHz for
// Z60 / Storm / Duo. At ratio 2 that makes gpu_clk 13-19 MHz against a cost
// model written at 100 MHz, and it makes gpu_clk DIFFERENT IN EACH MODE.
// 100 MHz divided by each pixel rate is 13.25 / 15.29 / 10.46 -- not an integer
// in any mode, so no fixed divider serves the tree either.
//
// The answer is not missing by accident: ZH-016's PRECONDITION IS NOT MET.
// board_truth.json lists `analog_video` and `external_io_timing` in
// openCapabilities, and boardVideoProbe is "passed_owner_visible_color_bars" --
// the board was shown able to EMIT video with no timing characterised. The
// probe answered the clock INPUTS and not the video OUTPUT.
//
// So VIDEO_CLK_HZ defaults to 0, meaning NOT FROZEN, and the simulation tree is
// parameterised by SIM_VIDEO_DIV, defaulting to 2 so that every existing bench
// and ZHAO_VID_CYCLES_PER_GPU continue to describe the same machine. The three
// ways to close it (a pixel clock ENABLE at one frozen frequency, three PLL
// outputs muxed at frame start, or runtime PLL reconfiguration) are set out in
// SYS.PLL.md. That is VIDEO's call, not this block's.
// ===========================================================================

/* verilator lint_off DECLFILENAME */
// ---------------------------------------------------------------------------
// zhao_sys_pll_simclk -- ONE simulation clock output.
//
// A helper module rather than four copies of a counter, because four copies is
// how three of them end up one edit behind the fourth. Instantiated four times
// below; it does not appear under ZHAO_SYS_PLL_VENDOR at all.
//
// DIV == 1 is a GATED PASS-THROUGH, not a bare `assign clk_o = ref_clk_i`. The
// enable is retimed on the FALLING edge -- the textbook glitch-free clock gate
// -- so that "no output toggles while the PLL is in reset" is true of the
// pass-through outputs as well as the divided ones. A model whose gpu_clk runs
// during reset would pass every ratio check and misrepresent the one property
// a reset sequencer downstream depends on.
// ---------------------------------------------------------------------------
module zhao_sys_pll_simclk #(
    parameter int unsigned DIV = 1       // 1, or any even number
) (
    input  logic ref_clk_i,
    input  logic arst_i,                 // active HIGH, asynchronous
    output logic clk_o
);

  generate
  if (DIV <= 1) begin : g_pass
    logic run_q;
    always_ff @(negedge ref_clk_i or posedge arst_i) begin
      if (arst_i) run_q <= 1'b0;
      else        run_q <= 1'b1;
    end
    assign clk_o = ref_clk_i & run_q;
  end else begin : g_div
    localparam int unsigned HALF = DIV / 2;
    localparam int unsigned CW   = (HALF <= 1) ? 1 : $clog2(HALF);

    logic [CW-1:0] cnt_q;
    logic          tog_q;

    always_ff @(posedge ref_clk_i or posedge arst_i) begin
      if (arst_i) begin
        cnt_q <= '0;
        tog_q <= 1'b0;
      end else if (cnt_q == CW'(HALF - 1)) begin
        cnt_q <= '0;
        tog_q <= ~tog_q;
      end else begin
        cnt_q <= cnt_q + CW'(1);
      end
    end

    assign clk_o = tog_q;
  end
  endgenerate

endmodule
/* verilator lint_on DECLFILENAME */


module zhao_sys_pll #(
    // ------------------------------------------------------------------
    // THE REFERENCE -- MEASURED.
    // board_truth.json clocks.fpgaInputs: FPGA_CLK1_50, 50 MHz, pin V11,
    // 3.3-V LVTTL, fpgaContractStatus
    // "physically_proven_for_pinned_mister_framework".
    //
    // This block takes ONE reference. CLK2/CLK3 exist on the board and are
    // left to the framework: a second PLL on a second oscillator would be a
    // second frequency authority, which is the defect this block prevents.
    // ------------------------------------------------------------------
    parameter int unsigned REF_CLK_HZ      = 50_000_000,

    // ------------------------------------------------------------------
    // THE REQUESTED OUTPUTS. These are REQUESTS, not achieved frequencies:
    // what the fractional counters actually realise is a FITTER OUTPUT and
    // belongs in a receipt, never in RTL.
    //
    // GPU_CLK_HZ -- DERIVED (target). spec/terrain_rules.md lines 525 and
    //   579 and spec/sky_and_beams.md line 162 all cost the frame against
    //   "a 1.67 M-cycle frame (100 MHz placeholder -- Phase 0 freezes the
    //   clock)". 100 MHz / 60 Hz = 1,666,667 cycles, which is that figure.
    //   SYS.PLL is the Phase-0 block that freezes it, so this line is that
    //   act. design/V1-RELEASE-DEFINITION.md names 105 MHz as the COMPOSED
    //   ACCEPTANCE FLOOR (it calls the shell's 99.34 MHz "below" it): 100 is
    //   the contract, 105 is the margin the fit must show.
    //
    // SDRAM_CLK_HZ -- DERIVED, corroborated two ways, neither of them a
    //   measurement. (a) MEM.SDRAM.md puts the controller in sdram_clk with
    //   no CDC to the gpu-side arbiter described anywhere, so one frequency
    //   is the simplest correct tree. (b) zhao_sdram_params_pkg.sv sets
    //   REFRESH_INTERVAL = 780 and spec/memory_rules.md line 26 glosses it
    //   as "8192 rows / 64 ms at the conservative clock"; 64 ms / 8192 =
    //   7.8125 us, and 780 / 7.8125 us = 99.84 MHz. The params package's own
    //   header calls its numbers "provisional sim constants, NOT board
    //   truth", so this agreement is corroboration and nothing more.
    //
    // VIDEO_CLK_HZ -- UNSETTLED. 0 is a sentinel meaning NOT FROZEN. See the
    //   file header; the reason is that ZH-016's precondition is not met.
    //
    // AUDIO_CLK_HZ -- DERIVED, and it flags a conflict between two
    //   contracts. spec/audio_rules.md section 1 ratifies 48,000 Hz and
    //   exactly 800 stereo pairs per displayed frame. AUDIO.FIFO.md then
    //   describes the output side as running in "audio_clk (48 kHz)". A
    //   Cyclone V PLL CANNOT EMIT 48 kHz. So the physical audio domain clock
    //   is 256 x 48 kHz = 12.288 MHz, the standard I2S master rate, and the
    //   48 kHz sample cadence is a clock enable inside that domain. THIS
    //   READING IS AN OPEN QUESTION FOR THE AUDIO.FIFO OWNER -- it changes
    //   no audio number, 800 pairs per frame stands either way, but it is
    //   this block reading another block's contract surface and saying so.
    // ------------------------------------------------------------------
    parameter int unsigned GPU_CLK_HZ      = 100_000_000,
    parameter int unsigned SDRAM_CLK_HZ    = 100_000_000,
    parameter int unsigned VIDEO_CLK_HZ    = 0,
    parameter int unsigned AUDIO_CLK_HZ    = 12_288_000,

    // ------------------------------------------------------------------
    // THE SIMULATION CLOCK TREE. These divide ref_clk_i and describe the
    // FROZEN SIM PROFILE, not the board. design/blocks.yml's own note for
    // this block says "Verilator lane uses a single conceptual clock", which
    // is exactly SIM_GPU_DIV = SIM_SDRAM_DIV = 1.
    //
    // SIM_VIDEO_DIV = 2 reproduces zhao_pkg::ZHAO_VID_CYCLES_PER_GPU and
    // spec/video_rules.md's "one vid cycle = 2 gpu cycles", so every bench in
    // the tree keeps describing the same machine. It is a KNOB, not the
    // hardware ratio; see the file header.
    //
    // SIM_AUDIO_DIV = 4 is NOT a convenience and was NOT chosen here: it is
    // the tree's own frozen sim profile. pga/rtl/common/zhao_shell_top.sv
    // (the PROTECTED shell, pinned by SHA-256 in .gitattributes), its v2
    // successor and zhao_console_core.sv all carry the identical port
    // comment -- "frozen ratios: vid = gpu/2, audio = gpu/4, fixed phase --
    // plan R1". The first draft of this parameter said 8, invented, and the
    // answer was three files away. It claims nothing about 48 kHz: at a
    // conceptual gpu clock the audio DIVISOR is a sim phase relationship, and
    // the real rate is AUDIO_CLK_HZ.
    //
    // Legal divisors: 1, or any even number. An odd divisor > 1 cannot be
    // built from a toggle flop and would silently truncate; the elaboration
    // guard below refuses it.
    // ------------------------------------------------------------------
    parameter int unsigned SIM_GPU_DIV     = 1,
    parameter int unsigned SIM_SDRAM_DIV   = 1,
    parameter int unsigned SIM_VIDEO_DIV   = 2,
    parameter int unsigned SIM_AUDIO_DIV   = 4,

    // Simulation lock time in ref_clk_i cycles. DERIVED FROM NOTHING
    // PHYSICAL: real Cyclone V lock time is a datasheet figure gated by the
    // achieved counter settings and no fit or board measurement of it exists.
    // This is a deliberately short, editable stand-in so a bench can see the
    // lock edge. SYS.RESET waits on the EVENT, never on a cycle count,
    // exactly because nobody can supply the count.
    parameter int unsigned SIM_LOCK_CYCLES = 8,

    // Width of the lock-loss census. It SATURATES; see below.
    parameter int unsigned LOST_W          = 16
) (
    // FPGA_CLK1_50, pin V11, 50 MHz, 3.3-V LVTTL -- MEASURED.
    input  logic              ref_clk_i,

    // Board hard reset, asynchronous, active low. Resets this block's
    // HOUSEKEEPING ONLY: the lock synchroniser and the lock-loss census.
    input  logic              arst_n_i,

    // The vendor PLL's own reset: asynchronous, active HIGH, Cyclone V
    // convention. It is a real port and not a test hook -- the primitive has
    // one -- and it is ALSO the only legal stimulus that can make
    // pll_lock_lost_o move. CLAUDE.md: a detector that has not been shown to
    // FIRE has not been tested, and a counter that cannot be fired is not an
    // instrument.
    //
    // IT IS SEPARATE FROM arst_n_i ON PURPOSE. One reset for both would clear
    // the lock-loss census in the same instant as the loss it counts -- the
    // cancelling-errors-inside-a-checker shape, where the detector reads a
    // reassuring zero because both of its operands moved together.
    input  logic              pll_arst_i,

    output logic              gpu_clk_o,
    output logic              sdram_clk_o,
    output logic              video_clk_o,
    output logic              audio_clk_o,

    // ASYNCHRONOUS TO EVERY DOMAIN, including ref_clk_i. Every consumer
    // synchronises it; SYS.RESET does, and so does this block's own census.
    output logic              pll_locked_o,

    // Census, ref_clk_i domain, arst_n_i reset, SATURATING. A wrapping census
    // can read zero after 2**LOST_W losses, which is the flattering direction.
    output logic [LOST_W-1:0] pll_lock_lost_o
);

  // -------------------------------------------------------------------------
  // ELABORATION GUARDS.
  //
  // Inside `initial begin ... end` because Quartus 17.0 rejects a bare
  // module-scope `if (...) $fatal(...)` with "syntax error near text: if;
  // expecting endmodule" -- CLAUDE.md's Build note records the incident.
  //
  // AND `--lint-only` DOES NOT RUN THIS BLOCK, which CLAUDE.md also records.
  // A clean lint is not evidence that any guard below fires. These references
  // do, however, mean every parameter above is USED, so a parameter that
  // becomes dead is a lint finding rather than silent decoration.
  // -------------------------------------------------------------------------
  initial begin
    if (REF_CLK_HZ == 0)
      $fatal(1, "zhao_sys_pll: REF_CLK_HZ is 0. board_truth.json measures 50 MHz on V11.");
    if (GPU_CLK_HZ == 0)
      $fatal(1, "zhao_sys_pll: GPU_CLK_HZ is 0. The cost model is written at 100 MHz.");
    if (SDRAM_CLK_HZ == 0)
      $fatal(1, "zhao_sys_pll: SDRAM_CLK_HZ is 0.");
    if (AUDIO_CLK_HZ == 0)
      $fatal(1, "zhao_sys_pll: AUDIO_CLK_HZ is 0. audio_rules.md ratifies 48 kHz sampling.");
    // VIDEO_CLK_HZ == 0 is LEGAL and means UNSETTLED. A non-zero value below
    // 1 MHz is not a PLL output on this family and is almost certainly a
    // sample rate written where a clock rate belongs.
    if ((VIDEO_CLK_HZ != 0) && (VIDEO_CLK_HZ < 1_000_000))
      $fatal(1, "zhao_sys_pll: VIDEO_CLK_HZ below 1 MHz is not a Cyclone V PLL output.");
    if ((SIM_GPU_DIV == 0) || (SIM_SDRAM_DIV == 0) ||
        (SIM_VIDEO_DIV == 0) || (SIM_AUDIO_DIV == 0))
      $fatal(1, "zhao_sys_pll: a SIM_*_DIV of 0 has no meaning.");
    if ((SIM_GPU_DIV > 1) && ((SIM_GPU_DIV % 2) != 0))
      $fatal(1, "zhao_sys_pll: SIM_GPU_DIV must be 1 or even.");
    if ((SIM_SDRAM_DIV > 1) && ((SIM_SDRAM_DIV % 2) != 0))
      $fatal(1, "zhao_sys_pll: SIM_SDRAM_DIV must be 1 or even.");
    if ((SIM_VIDEO_DIV > 1) && ((SIM_VIDEO_DIV % 2) != 0))
      $fatal(1, "zhao_sys_pll: SIM_VIDEO_DIV must be 1 or even.");
    if ((SIM_AUDIO_DIV > 1) && ((SIM_AUDIO_DIV % 2) != 0))
      $fatal(1, "zhao_sys_pll: SIM_AUDIO_DIV must be 1 or even.");
    if (SIM_LOCK_CYCLES == 0)
      $fatal(1, "zhao_sys_pll: SIM_LOCK_CYCLES of 0 makes the lock edge unobservable.");
    if (LOST_W < 2)
      $fatal(1, "zhao_sys_pll: LOST_W < 2 leaves no room for a saturating census.");
  end

  // The raw, asynchronous lock from whichever body is compiled.
  logic locked_raw_w;

`ifdef ZHAO_SYS_PLL_VENDOR
  // =========================================================================
  // VENDOR BODY -- QUARTUS ONLY, AND UNVERIFIED BY ANYTHING IN THIS TREE.
  //
  // Not linted (Verilator cannot elaborate altera_pll), not simulated, not
  // mapped, not fitted, not loaded. The parameter strings below are written
  // from the IP's documented interface and have never been compiled. Treat
  // every one of them as a claim awaiting the first quartus_map.
  //
  // The frequencies come from the parameters above so that the two bodies
  // cannot drift into describing different machines -- but a string built by
  // $sformatf is not accepted by the IP's parameter parser, so they are
  // written as literals HERE and the elaboration guard below is what ties
  // them back. If you change a *_CLK_HZ parameter you must change the literal
  // beside it; that is a real maintenance burden and it is stated rather than
  // hidden.
  // =========================================================================
  initial begin
    if (REF_CLK_HZ != 50_000_000)
      $fatal(1, "zhao_sys_pll: vendor body's reference literal is 50.0 MHz; REF_CLK_HZ disagrees.");
    if (GPU_CLK_HZ != 100_000_000)
      $fatal(1, "zhao_sys_pll: vendor body's outclk0 literal is 100 MHz; GPU_CLK_HZ disagrees.");
    if (SDRAM_CLK_HZ != 100_000_000)
      $fatal(1, "zhao_sys_pll: vendor body's outclk1 literal is 100 MHz; SDRAM_CLK_HZ disagrees.");
    if (VIDEO_CLK_HZ == 0)
      $fatal(1, "zhao_sys_pll: VIDEO_CLK_HZ is UNSETTLED (0); the vendor body cannot be built until ZH-016 settles it.");
  end

  wire [3:0] outclk_w;

  altera_pll #(
      .fractional_vco_multiplier ("false"),
      .reference_clock_frequency ("50.0 MHz"),
      .operation_mode            ("direct"),
      .number_of_clocks          (4),
      .output_clock_frequency0   ("100.000000 MHz"),
      .phase_shift0              ("0 ps"),
      .duty_cycle0               (50),
      .output_clock_frequency1   ("100.000000 MHz"),
      .phase_shift1              ("0 ps"),
      .duty_cycle1               (50),
      .output_clock_frequency2   ("0.000000 MHz"),   // video -- UNSETTLED
      .phase_shift2              ("0 ps"),
      .duty_cycle2               (50),
      .output_clock_frequency3   ("12.288000 MHz"),
      .phase_shift3              ("0 ps"),
      .duty_cycle3               (50)
  ) u_pll (
      .rst      (pll_arst_i),
      .refclk   (ref_clk_i),
      .outclk   (outclk_w),
      .locked   (locked_raw_w)
  );

  assign gpu_clk_o   = outclk_w[0];
  assign sdram_clk_o = outclk_w[1];
  assign video_clk_o = outclk_w[2];
  assign audio_clk_o = outclk_w[3];

`else
  // =========================================================================
  // SIMULATION BODY -- the default, and the only one this repository compiles.
  //
  // It models three things and nothing else: the clock RATIOS, the fact that
  // no output runs before lock, and the lock EVENT. It models no jitter, no
  // duty-cycle error, no lock time in real units and no VCO behaviour, and it
  // is not evidence about any of them.
  // =========================================================================
  zhao_sys_pll_simclk #(.DIV(SIM_GPU_DIV)) u_gpu (
      .ref_clk_i (ref_clk_i), .arst_i (pll_arst_i), .clk_o (gpu_clk_o));

  zhao_sys_pll_simclk #(.DIV(SIM_SDRAM_DIV)) u_sdram (
      .ref_clk_i (ref_clk_i), .arst_i (pll_arst_i), .clk_o (sdram_clk_o));

  zhao_sys_pll_simclk #(.DIV(SIM_VIDEO_DIV)) u_video (
      .ref_clk_i (ref_clk_i), .arst_i (pll_arst_i), .clk_o (video_clk_o));

  zhao_sys_pll_simclk #(.DIV(SIM_AUDIO_DIV)) u_audio (
      .ref_clk_i (ref_clk_i), .arst_i (pll_arst_i), .clk_o (audio_clk_o));

  // ---- the lock model ------------------------------------------------------
  localparam int unsigned LOCKCW =
      (SIM_LOCK_CYCLES <= 1) ? 1 : $clog2(SIM_LOCK_CYCLES);

  logic [LOCKCW-1:0] lock_cnt_q;
  logic              locked_q;

  always_ff @(posedge ref_clk_i or posedge pll_arst_i) begin
    if (pll_arst_i) begin
      lock_cnt_q <= '0;
      locked_q   <= 1'b0;
    end else if (!locked_q) begin
      if (lock_cnt_q == LOCKCW'(SIM_LOCK_CYCLES - 1)) locked_q <= 1'b1;
      else                                            lock_cnt_q <= lock_cnt_q + LOCKCW'(1);
    end
  end

  assign locked_raw_w = locked_q;
`endif

  assign pll_locked_o = locked_raw_w;

  // -------------------------------------------------------------------------
  // THE LOCK-LOSS CENSUS.
  //
  // locked_raw_w is asynchronous, so it crosses on a two-flop synchroniser
  // before anything looks at its VALUE. The detector then differences two
  // ADJACENT STAGES of that one shift register -- lock_s_q[1] is the older
  // sample, lock_s_q[0] the newer -- which makes it an EDGE DETECTOR and not a
  // value comparison.
  //
  // CLAUDE.md requires the question to be asked of every checker: what are the
  // two sides clocked by? Both by ref_clk_i, one cycle apart BY CONSTRUCTION.
  // That is the whole mechanism, so there is no enable that can corrupt them in
  // lockstep. And the register it writes is reset by arst_n_i -- a DIFFERENT
  // reset from pll_arst_i, which is what causes the event being counted.
  //
  // The power-on interval before the first lock is not a LOSS and is not
  // counted. The census saturates; it never wraps.
  // -------------------------------------------------------------------------
  logic [1:0] lock_s_q;

  always_ff @(posedge ref_clk_i or negedge arst_n_i) begin
    if (!arst_n_i) begin
      lock_s_q        <= 2'b00;
      pll_lock_lost_o <= '0;
    end else begin
      lock_s_q <= {lock_s_q[0], locked_raw_w};
      if (lock_s_q[1] && !lock_s_q[0] && (pll_lock_lost_o != {LOST_W{1'b1}}))
        pll_lock_lost_o <= pll_lock_lost_o + LOST_W'(1);
    end
  end

endmodule
