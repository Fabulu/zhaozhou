// tb_zhao_sys_reset.sv -- the DUT wrapper for `sys_reset_directed`.
//
// It instantiates `zhao_sys_reset` TWICE from identical stimulus:
//
//   u_a -- the shipping defaults: SDRAM 0, GPU 3, VIDEO 6, AUDIO 9 within a
//          RELEASE_SPAN of 16, the ledger's `variable_bounded:16`.
//   u_b -- ALL FOUR STEPS AT ZERO. The contract calls the stagger "a judgement
//          dressed as four constants" and says setting them all to 0 is a legal
//          configuration; this is the instance that proves it, and it is why
//          the order is a knob rather than a hidden law.
//
// Same reasoning as the PLL wrapper: one process, one stimulus, two instances,
// so no run-to-run difference can be mistaken for a parameter effect. A
// WRAPPER, not a copy -- nothing here can drift away from production because
// nothing here duplicates it.

module tb_zhao_sys_reset (
    input  logic        ref_clk_i,
    input  logic        pll_locked_i,
    input  logic        hard_reset_n_i,
    input  logic        gpu_clk_i,
    input  logic        sdram_clk_i,
    input  logic        video_clk_i,
    input  logic        audio_clk_i,

    // A: the shipping stagger.
    output logic        a_rst_n_gpu_o,
    output logic        a_rst_n_sdram_o,
    output logic        a_rst_n_video_o,
    output logic        a_rst_n_audio_o,
    output logic        a_seq_done_o,
    output logic [15:0] a_assertions_o,

    // B: every step at 0.
    output logic        b_rst_n_gpu_o,
    output logic        b_rst_n_sdram_o,
    output logic        b_rst_n_video_o,
    output logic        b_rst_n_audio_o,
    output logic        b_seq_done_o,
    output logic [15:0] b_assertions_o
);

  zhao_sys_reset u_a (
      .ref_clk_i          (ref_clk_i),
      .pll_locked_i       (pll_locked_i),
      .hard_reset_n_i     (hard_reset_n_i),
      .gpu_clk_i          (gpu_clk_i),
      .sdram_clk_i        (sdram_clk_i),
      .video_clk_i        (video_clk_i),
      .audio_clk_i        (audio_clk_i),
      .rst_n_gpu_o        (a_rst_n_gpu_o),
      .rst_n_sdram_o      (a_rst_n_sdram_o),
      .rst_n_video_o      (a_rst_n_video_o),
      .rst_n_audio_o      (a_rst_n_audio_o),
      .seq_done_o         (a_seq_done_o),
      .reset_assertions_o (a_assertions_o)
  );

  zhao_sys_reset #(
      .RELEASE_STEP_SDRAM (0),
      .RELEASE_STEP_GPU   (0),
      .RELEASE_STEP_VIDEO (0),
      .RELEASE_STEP_AUDIO (0)
  ) u_b (
      .ref_clk_i          (ref_clk_i),
      .pll_locked_i       (pll_locked_i),
      .hard_reset_n_i     (hard_reset_n_i),
      .gpu_clk_i          (gpu_clk_i),
      .sdram_clk_i        (sdram_clk_i),
      .video_clk_i        (video_clk_i),
      .audio_clk_i        (audio_clk_i),
      .rst_n_gpu_o        (b_rst_n_gpu_o),
      .rst_n_sdram_o      (b_rst_n_sdram_o),
      .rst_n_video_o      (b_rst_n_video_o),
      .rst_n_audio_o      (b_rst_n_audio_o),
      .seq_done_o         (b_seq_done_o),
      .reset_assertions_o (b_assertions_o)
  );

endmodule
