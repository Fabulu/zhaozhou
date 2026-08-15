// zhao_audio_fifo.sv — AUDIO.FIFO: the 2048-pair PCM FIFO with the
// documented gpu -> audio_clk crossing (plan W2.4).
//
// Law (in citation order):
//   spec/audio_rules.md  — §2 the FIFO law (D4): depth 2048 stereo pairs,
//                          refill burst 256, low watermark 512, underrun =
//                          repeat the last L/R pair + audio_underruns (once
//                          per continuous event), overflow STRUCTURALLY
//                          impossible (ready/valid backpressure); §1 exactly
//                          800 pairs per displayed frame (accounting law,
//                          test-side); §6 output {valid, l, r} one pair per
//                          audio tick
//   spec/counters.md     — §3 distributed counters: audio_underruns (catalog
//                          id ZHAO_CNT_AUDIO_UNDERRUNS) lives HERE in the
//                          audio domain; frame_tick latches the gpu-domain
//                          shadow; §4 u64 snapshot, saturate-never-wrap
//   spec/memory_rules.md — §4.2 PCM_RING (the upstream refill client owns
//                          ring reads; this module sees finished pairs only)
//   design/contracts/AUDIO.FIFO.md — the block contract
//
// CDC (the documented async_bridge, SYS.CDC TODO-stub rule V8: this block is
// its own bridge, documented here):
//   - storage `mem` is a dual-port async FIFO memory: written in gpu_clk,
//     read in audio_clk; whole 32-bit pairs cross as units (never torn).
//   - write pointer (gpu) -> audio domain: binary-to-GRAY, 2-flop
//     synchroniser on audio_clk; empty compares GRAY codes directly.
//   - read pointer (audio) -> gpu domain: GRAY + 2-flop synchroniser on
//     gpu_clk; occupancy = wr_ptr - synced rd_ptr is CONSERVATIVE (the gpu
//     side sees a stale, smaller read pointer, so it OVERESTIMATES
//     occupancy — accepting a write only when this view < DEPTH therefore
//     bounds the TRUE occupancy by DEPTH: overflow is structurally
//     impossible, the formal property audio_fifo_bounds).
//   - audio_underruns counter -> gpu domain: GRAY + 2-flop synchroniser
//     (gray is safe: the counter changes one bit per increment), latched
//     into the u64 shadow by frame_tick.
//
// Clocks: gpu_clk write side, audio_clk read side, ANY rational ratio (the
// design is ratio-agnostic). Sim seam (plan R1): the Verilator harness
// drives audio_clk = gpu_clk/4 — one audio rising edge at the END of every
// 4th gpu cycle, gpu edge first. The real board ratio arrives post-ZH-016
// and never changes these interfaces. Reset: async assert on both domains
// (rst_gpu_n / rst_audio_n, tied to the same tree in sim); FIFO empty,
// output silent — zero pairs, NOT repeats, there is no "last pair" yet —
// audio_underruns 0, refill idle until first write.
//
// Conservative SystemVerilog subset only (charter §2).
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_zhao_audio_fifo).

module zhao_audio_fifo
#(
  // D4-frozen geometry (spec/audio_rules.md §2). Parameters exist ONLY so
  // the formal harness can shrink the memory for tractability; the
  // committed defaults ARE the frozen law and synthesis / every ctest uses
  // them. DEPTH must be a power of two (pointer arithmetic relies on it).
  parameter int unsigned DEPTH     = 2048,  // stereo pairs (8 KiB, 4 x M10K)
  parameter int unsigned WATERMARK = 512    // low-watermark refill level
) (
  // ---- write side: gpu domain -------------------------------------------
  input  logic        clk_gpu,
  input  logic        rst_gpu_n,

  // finished stereo pairs from the ring-read client (ready/valid; the
  // client consumes HPS bridge burst beats — each 64-bit beat carries two
  // L-then-R pairs — and presents them here pair by pair)
  input  logic        wr_valid_i,
  input  logic [15:0] wr_l_i,
  input  logic [15:0] wr_r_i,
  output logic        wr_ready_o,       // !full: no accept when full (D4)

  // refill law visibility: level request when occupancy <= low watermark;
  // the refill client issues the 256-pair burst (audio_rules.md §2)
  output logic        refill_req_o,
  output logic [$clog2(DEPTH):0] occupancy_o,  // gpu-domain (conservative) view

  // frame_tick (gpu-domain pulse from VIDEO.FRAMECTL): latches the counter
  // shadow (counters.md §3); DEBUG.COUNTERS holds it for the read window
  input  logic        frame_tick_i,
  output zhao_pkg::zhao_counter_snap_t cnt_snap_o,

  // ---- read side: audio domain (48 kHz) ----------------------------------
  input  logic        clk_audio,
  input  logic        rst_audio_n,

  // one pair per audio tick; free-running consumer — underrun, not stall
  output logic        pcm_valid_o,
  output logic [15:0] pcm_l_o,
  output logic [15:0] pcm_r_o,
  output logic        underrun_status_o,  // THIS tick repeated the last pair
  output logic [31:0] audio_underruns_o   // live counter (saturating)
);

  // No import statement: yosys's SV frontend (the formal lane) rejects
  // `import` in both module headers and bodies, so every zhao_pkg reference
  // below is fully qualified. Verilator is equally happy with qualified
  // names — one style, both tools.
  localparam int unsigned ADDR_BITS = $clog2(DEPTH);  // 11 for 2048
  localparam int unsigned PTR_W     = ADDR_BITS + 1;  // + wrap bit

  // ------------------------------------------------------------ gray code --
  function automatic logic [PTR_W-1:0] ptr_bin2gray(input logic [PTR_W-1:0] b);
    ptr_bin2gray = b ^ (b >> 1);
  endfunction

  function automatic logic [PTR_W-1:0] ptr_gray2bin(input logic [PTR_W-1:0] g);
    logic [PTR_W-1:0] b;
    b[PTR_W-1] = g[PTR_W-1];
    for (int i = PTR_W-2; i >= 0; i--) begin  // signed: counts down to 0
      b[i] = b[i+1] ^ g[i];
    end
    ptr_gray2bin = b;
  endfunction

  function automatic logic [31:0] cnt_bin2gray(input logic [31:0] b);
    cnt_bin2gray = b ^ (b >> 1);
  endfunction

  function automatic logic [31:0] cnt_gray2bin(input logic [31:0] g);
    logic [31:0] b;
    b[31] = g[31];
    for (int i = 30; i >= 0; i--) begin  // signed: counts down to 0
      b[i] = b[i+1] ^ g[i];
    end
    cnt_gray2bin = b;
  endfunction

  // ------------------------------------------------------------- storage ---
  logic [31:0] mem [0:DEPTH-1];  // {r[15:0], l[15:0]} per word, L then R LE

  // ---------------------------------------------- gpu domain (write side) --
  logic [PTR_W-1:0] wr_ptr;
  logic [PTR_W-1:0] rd_gray_meta, rd_gray_sync;   // audio rd ptr view (2FF)
  logic [31:0]      cnt_gray_meta, cnt_gray_sync; // underruns view (2FF)
  logic [PTR_W-1:0] occ_gpu;
  logic             full;

  // audio-domain registers, sampled combinationally by the sync flops (the
  // standard async-FIFO crossing; gray-coded so only one bit changes and
  // the destination sees old-or-new, never a torn multi-bit value)
  logic [PTR_W-1:0] rd_ptr;
  logic [31:0]      underruns;
  logic [PTR_W-1:0] rd_gray_c;
  logic [PTR_W-1:0] wr_gray_c;
  logic [31:0]      cnt_gray_c;

  assign rd_gray_c = ptr_bin2gray(rd_ptr);
  assign wr_gray_c = ptr_bin2gray(wr_ptr);
  assign cnt_gray_c = cnt_bin2gray(underruns);

  assign occ_gpu = wr_ptr - ptr_gray2bin(rd_gray_sync);
  assign full    = (occ_gpu == PTR_W'(DEPTH));

  assign wr_ready_o   = ~full;                       // backpressure law
  assign refill_req_o = (occ_gpu <= PTR_W'(WATERMARK));
  assign occupancy_o  = occ_gpu;

  always_ff @(posedge clk_gpu or negedge rst_gpu_n) begin
    if (!rst_gpu_n) begin
      wr_ptr        <= '0;
      rd_gray_meta  <= '0;
      rd_gray_sync  <= '0;
      cnt_gray_meta <= '0;
      cnt_gray_sync <= '0;
      cnt_snap_o.valid      <= 1'b0;
      cnt_snap_o.counter_id <= zhao_pkg::ZHAO_CNT_AUDIO_UNDERRUNS;
      cnt_snap_o.value      <= '0;
    end else begin
      if (wr_valid_i && wr_ready_o) begin
        mem[wr_ptr[ADDR_BITS-1:0]] <= {wr_r_i, wr_l_i};
        wr_ptr <= wr_ptr + 1'b1;
      end
      rd_gray_meta  <= rd_gray_c;
      rd_gray_sync  <= rd_gray_meta;
      cnt_gray_meta <= cnt_gray_c;
      cnt_gray_sync <= cnt_gray_meta;
      if (frame_tick_i) begin
        cnt_snap_o.valid      <= 1'b1;
        cnt_snap_o.counter_id <= zhao_pkg::ZHAO_CNT_AUDIO_UNDERRUNS;
        cnt_snap_o.value      <= {32'd0, cnt_gray2bin(cnt_gray_sync)};
      end else begin
        cnt_snap_o.valid <= 1'b0;
      end
    end
  end

  // ------------------------------------------- audio domain (read side) ----
  logic [PTR_W-1:0] wr_gray_meta, wr_gray_sync;   // gpu wr ptr view (2FF)
  logic             started;
  logic             empty;
  logic [31:0]      rd_word;

  assign rd_word = mem[rd_ptr[ADDR_BITS-1:0]];
  assign empty   = (ptr_bin2gray(rd_ptr) == wr_gray_sync);

  always_ff @(posedge clk_audio or negedge rst_audio_n) begin
    if (!rst_audio_n) begin
      rd_ptr            <= '0;
      wr_gray_meta      <= '0;
      wr_gray_sync      <= '0;
      started           <= 1'b0;
      pcm_valid_o       <= 1'b0;
      pcm_l_o           <= 16'd0;
      pcm_r_o           <= 16'd0;
      underrun_status_o <= 1'b0;
      underruns         <= 32'd0;
    end else begin
      underrun_status_o <= 1'b0;
      if (!empty) begin
        // pop one whole pair; the output register becomes the "last pair"
        pcm_l_o     <= rd_word[15:0];
        pcm_r_o     <= rd_word[31:16];
        pcm_valid_o <= 1'b1;
        started     <= 1'b1;
        rd_ptr      <= rd_ptr + 1'b1;
      end else if (started) begin
        // D4 underrun: repeat the last emitted pair (registers hold),
        // audio_underruns++ ONCE per continuous event, stream continuous
        underrun_status_o <= 1'b1;
        if (!underrun_status_o && underruns != 32'hFFFF_FFFF) begin
          underruns <= underruns + 32'd1;
        end
      end
      // else: pre-first-pair silence — zeros, valid 0, NO underrun count
      wr_gray_meta <= wr_gray_c;
      wr_gray_sync <= wr_gray_meta;
    end
  end

  assign audio_underruns_o = underruns;

endmodule : zhao_audio_fifo
