// zhao_video_mode.sv — VIDEO.MODE, the free-running raster/timing generator
// (plan W2.2; law: spec/video_rules.md §1-§2, decisions D1/D6).
//
// One raster for the three frozen modes (Z60 / Storm / Duo). The timing
// constants exist EXACTLY ONCE, in zhao_pkg.sv (ZHAO_TIMING) — this module
// never re-declares a number. The raster is FREE-RUNNING: it never stalls,
// never stretches, never waits for fetch (spec/video_rules.md §2).
//
// Mode latch law (spec/video_rules.md §1.1): the mode register is latched
// ONLY at frame start — the first vid cycle after vblank end, i.e. when the
// raster enters (x=0, y=0) — effective the NEXT frame, never mid-frame.
// A mid-frame mode_we updates only the PENDING register; mode_out follows at
// the next frame start. A rogue out-of-range value holds the previous mode
// (last valid wins — directed fault injection covers it).
//
// Reset (contract VIDEO.MODE "Clock and reset semantics"): raster lands
// PRE-ACTIVE at the start of the vertical back porch (y = v_total - v_back)
// and the start of the horizontal back porch (x = h_active + h_front +
// h_sync), so the first active pixel emerges only after the back porch has
// walked out — exactly one full vblank of startup, then frame 0 at (0,0).
//
// Raster phase convention (shared by SCANOUT/FRAMECTL/zref::VideoMode):
//   x in [0, h_active)                     -> active pixels (hblank=0)
//   x in [h_active, h_active+h_front)      -> h front porch
//   x in [h_active+h_front, +h_sync)       -> H SYNC (negative polarity:
//                                             hsync output is ACTIVE HIGH —
//                                             it asserts during the sync
//                                             pulse of the negative-going
//                                             sync; spec §2 "negative syncs")
//   x in [.., h_total)                     -> h back porch
//   y >= v_active                          -> vblank; y in [v_active +
//   v_front, +v_sync) -> V SYNC pulse; y in [.., v_total) -> v back porch.
//   Pixel (0,0) = first active pixel of the first active line.
//
// Two frame-boundary pulses (contract VIDEO.MODE packet layouts):
//   frame_start — one vid cycle at (x==0 && y==0): leaving vblank.
//   frame_end   — one vid cycle at (x==0 && y==v_active): entering vblank.
//   vswap_dec   — one vid cycle at (x==0 && y==v_active+v_front = 244):
//                 the swap/repeat DECISION instant (spec/video_rules.md §4
//                 "the vblank start line … the first cycle of vertical
//                 sync"). FRAMECTL decides here; scanout prefetch re-arms
//                 here (18 lines of vblank margin before frame_start).
//
// Conservative SystemVerilog subset only (charter §2).
// Lint: clean under `verilator_bin --lint-only -Wall` (CTest lint_zhao_video_mode).

module zhao_video_mode
  import zhao_pkg::*;
(
  input  logic        vid_clk,
  input  logic        rst_n,      // synchronous-active, vid domain

  // mode register port (from CMD.SCHEDULER SetPresentationContract
  // execution; driven vid-synchronous in W2.2 tests — the gpu->vid CDC is
  // owned by the system shell, not this block)
  input  logic        mode_we,
  input  logic [1:0]  mode_in,    // 0..2; other values hold the previous mode

  // raster outputs (1 vid cycle from register state; contract latency)
  output logic [15:0] x,
  output logic [15:0] y,
  output logic        hsync,      // asserted during the H sync pulse
  output logic        vsync,      // asserted during the V sync pulse
  output logic        hblank,     // x outside h_active
  output logic        vblank,     // y outside v_active
  output logic        frame_start,// one cycle at (x==0 && y==0)
  output logic        frame_end,  // one cycle at (x==0 && y==v_active)
  output logic        vswap_dec,  // one cycle at (x==0 && y==v_active+v_front)

  // latched (current-frame) and pending (next-frame) mode
  output zhao_mode_e  mode_out,
  output zhao_mode_e  mode_next
);

  zhao_mode_e mode_cur;   // the mode the CURRENT frame runs under
  zhao_mode_e mode_pend;  // latched at the next frame_start

  /* verilator lint_off UNUSEDSIGNAL */
  zhao_timing_t tim;   // unused fields (frame_gpu_cycles, canvas constants)
  assign tim = ZHAO_TIMING[mode_cur];
  /* verilator lint_on UNUSEDSIGNAL */

  // ---------------------------------------------------------------- raster --
  // Reset lands pre-active at the START of both back porches (contract).
  logic [15:0] x_next, y_next;
  logic        line_wrap, frame_wrap;

  assign line_wrap = (x == tim.h_total - 16'd1);
  assign frame_wrap = line_wrap && (y == tim.v_total - 16'd1);
  assign x_next = line_wrap ? 16'd0 : x + 16'd1;
  assign y_next = line_wrap ? (frame_wrap ? 16'd0 : y + 16'd1) : y;

  // entering (0,0) on the NEXT edge == the frame_start latch instant
  logic entering_frame_start;
  assign entering_frame_start = frame_wrap;

  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      x        <= ZHAO_TIMING[ZHAO_MODE_Z60].h_active
                + ZHAO_TIMING[ZHAO_MODE_Z60].h_front
                + ZHAO_TIMING[ZHAO_MODE_Z60].h_sync;  // start of H back porch
      y        <= ZHAO_TIMING[ZHAO_MODE_Z60].v_total
                - ZHAO_TIMING[ZHAO_MODE_Z60].v_back;  // start of V back porch
      mode_cur <= ZHAO_MODE_Z60;                       // reset value (spec §1.1)
      mode_pend<= ZHAO_MODE_Z60;
    end else begin
      x <= x_next;
      y <= y_next;
      // latch law: the timing constants change ATOMICALLY at frame start
      if (entering_frame_start) begin
        mode_cur <= mode_pend;
      end
      // pending register: last VALID value wins; rogues hold (contract)
      if (mode_we && (mode_in == 2'd0 || mode_in == 2'd1 || mode_in == 2'd2)) begin
        mode_pend <= zhao_mode_e'(mode_in);
      end
    end
  end

  // ------------------------------------------------------ output decodes ---
  // 1-cycle latency: outputs decode the REGISTER state (contract).
  assign frame_start = (x == 16'd0) && (y == 16'd0);
  assign frame_end   = (x == 16'd0) && (y == tim.v_active);
  assign vswap_dec   = (x == 16'd0) && (y == tim.v_active + tim.v_front);

  assign hblank = (x >= tim.h_active);
  assign vblank = (y >= tim.v_active);
  assign hsync  = (x >= tim.h_active + tim.h_front)
               && (x <  tim.h_active + tim.h_front + tim.h_sync);
  assign vsync  = (y >= tim.v_active + tim.v_front)
               && (y <  tim.v_active + tim.v_front + tim.v_sync);

  assign mode_out  = mode_cur;
  assign mode_next = mode_pend;

endmodule : zhao_video_mode
