// tb_zhao_sys_pll.sv -- the DUT wrapper for `sys_pll_directed`.
//
// It instantiates `zhao_sys_pll` TWICE, from the same reference and the same
// two resets, and differs only in parameters:
//
//   u_a -- the shipping defaults (SIM_VIDEO_DIV 2, SIM_AUDIO_DIV 4,
//          SIM_LOCK_CYCLES 8). This is the frozen sim profile that
//          zhao_pkg::ZHAO_VID_CYCLES_PER_GPU and zhao_shell_top's
//          "vid = gpu/2, audio = gpu/4, plan R1" describe.
//   u_b -- the SAME BLOCK WITH THE KNOBS MOVED (video 4, audio 8, lock 3).
//
// WHY BOTH IN ONE WRAPPER RATHER THAN TWO EXECUTABLES: a parameter is only a
// knob if moving it moves the machine, and the cheapest honest way to show that
// is to run both parameterisations against IDENTICAL STIMULUS in one process.
// Two executables would compare two runs; this compares two instances, cycle
// for cycle, and no stimulus difference can creep in between them.
//
// This is a WRAPPER, not a copy -- it instantiates production and mutates
// nothing -- so `tools/budget/mutant_copy_drift.py` correctly ignores it and
// there is no copied body here to go stale.

module tb_zhao_sys_pll (
    input  logic        ref_clk_i,
    input  logic        arst_n_i,
    input  logic        pll_arst_i,

    // A: the shipping defaults.
    output logic        a_gpu_clk_o,
    output logic        a_sdram_clk_o,
    output logic        a_video_clk_o,
    output logic        a_audio_clk_o,
    output logic        a_locked_o,
    output logic [15:0] a_lost_o,

    // B: the knobs moved.
    output logic        b_gpu_clk_o,
    output logic        b_sdram_clk_o,
    output logic        b_video_clk_o,
    output logic        b_audio_clk_o,
    output logic        b_locked_o,
    output logic [15:0] b_lost_o
);

  zhao_sys_pll u_a (
      .ref_clk_i       (ref_clk_i),
      .arst_n_i        (arst_n_i),
      .pll_arst_i      (pll_arst_i),
      .gpu_clk_o       (a_gpu_clk_o),
      .sdram_clk_o     (a_sdram_clk_o),
      .video_clk_o     (a_video_clk_o),
      .audio_clk_o     (a_audio_clk_o),
      .pll_locked_o    (a_locked_o),
      .pll_lock_lost_o (a_lost_o)
  );

  zhao_sys_pll #(
      .SIM_VIDEO_DIV   (4),
      .SIM_AUDIO_DIV   (8),
      .SIM_LOCK_CYCLES (3)
  ) u_b (
      .ref_clk_i       (ref_clk_i),
      .arst_n_i        (arst_n_i),
      .pll_arst_i      (pll_arst_i),
      .gpu_clk_o       (b_gpu_clk_o),
      .sdram_clk_o     (b_sdram_clk_o),
      .video_clk_o     (b_video_clk_o),
      .audio_clk_o     (b_audio_clk_o),
      .pll_locked_o    (b_locked_o),
      .pll_lock_lost_o (b_lost_o)
  );

endmodule
