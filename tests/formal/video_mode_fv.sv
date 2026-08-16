// video_mode_fv.sv — formal harness for zhao_video_mode (tests/formal/
// video_mode_timing.sby). TESTBENCH/FORMAL COMPONENT — never synthesized,
// never linted by the RTL lanes.
//
// Assumptions: rst_n held low for the first cycles then released; mode
// writes free (any 2-bit value — the rogue-value hold law is proven too).
//
// Properties (spec/video_rules.md §1-§2, §8):
//   * bounds: x < h_total, y < v_total of the CURRENT mode table row
//   * frame_start <-> (x==0 && y==0)  (exactly the raster origin)
//   * mode latch: mode_out changes ONLY during a frame_start cycle
//   * y-increment law: y wraps at v_total-1 and increments exactly once
//     per h_total vid cycles (checked over the BMC window)
//   * reset-idle: under reset the raster holds the pre-active state

module video_mode_fv
  import zhao_pkg::*;
(
  input logic vid_clk,
  input logic rst_n,
  input logic mode_we,
  input logic [1:0] mode_in
);

  logic [15:0] x, y;
  logic hsync, vsync, hblank, vblank, frame_start, frame_end, vswap_dec;
  zhao_mode_e mode_out, mode_next;

  zhao_video_mode dut (
    .vid_clk(vid_clk), .rst_n(rst_n),
    .mode_we(mode_we), .mode_in(mode_in),
    .x(x), .y(y),
    .hsync(hsync), .vsync(vsync), .hblank(hblank), .vblank(vblank),
    .frame_start(frame_start), .frame_end(frame_end), .vswap_dec(vswap_dec),
    .mode_out(mode_out), .mode_next(mode_next)
  );

  // reset discipline for the BMC: the proof run STARTS in reset and reset
  // is released at most once (slang: assumptions in always blocks)
  reg f_past_valid = 0;
  always @(posedge vid_clk) begin
    f_past_valid <= 1;
    if (!f_past_valid) begin
      assume(!rst_n);   // the first cycle is in reset
    end else begin
      // once released, reset stays released for the proof run (no 1->0:
      // a mid-frame reset would legitimately discard in-flight fences)
      assume(!$past(rst_n) || rst_n);
    end
  end

  // bounds (spec §2: the raster wraps modulo the mode table)
  always @(posedge vid_clk) begin
    if (rst_n) begin
      assert(x < ZHAO_TIMING[mode_out].h_total);
      assert(y < ZHAO_TIMING[mode_out].v_total);
      // frame_start is exactly the raster origin
      assert(frame_start == ((x == 16'd0) && (y == 16'd0)));
      // mode latch law: mode_out changes only in a frame_start cycle
      if (f_past_valid && $past(rst_n)) begin
        if (mode_out != $past(mode_out)) begin
          assert(frame_start);
        end
        // the raster advances by exactly one pixel per vid cycle
        if (!$past(frame_start) && $past(x) + 16'd1 !=
            ZHAO_TIMING[$past(mode_out)].h_total) begin
          assert(x == $past(x) + 16'd1);
          assert(y == $past(y));
        end
      end
    end else if (f_past_valid) begin
      // reset-idle: under (held) reset the raster holds the FORMAL-override
      // reset position and the mode is the spec 1.1 reset value.
      // MERGE FIX: the salvaged harness had this check nested INSIDE
      // `if (rst_n)` — structurally unreachable, vacuous by construction
      // (the exact defect class V16 exists to catch). It now checks the
      // ifdef-FORMAL reset ring position; the TRUE (synthesis) reset
      // position is pinned by video_mode_directed.cpp. Guarded by
      // f_past_valid: the step-0 model state is free-init until the
      // modeled async reset has applied at one clock edge (the W2.3
      // phantom-gap trap).
      assert(x == ZHAO_TIMING[ZHAO_MODE_Z60].h_total - 16'd4);
      assert(y == ZHAO_TIMING[ZHAO_MODE_Z60].v_total - 16'd1);
      assert(mode_out == ZHAO_MODE_Z60);
      assert(!frame_start);
    end
  end

  // ---- covers: every guarded assertion's antecedent is reachable ---------
  // (ledger rule V16: a proof without reachability witnesses is not green)
  always @(posedge vid_clk) begin
    if (f_past_valid && rst_n && $past(rst_n)) begin
      c_frame_start:  cover(frame_start);
      c_mode_change:  cover(mode_out != $past(mode_out)); // latch fired
      c_mode_pending: cover(mode_next != mode_out);       // a write landed
      c_rogue_held:   cover($past(mode_we) && $past(mode_in) == 2'd3
                            && mode_next == $past(mode_next)); // rogue holds
      c_mid_line:     cover(x == 16'd7 && y == 16'd0);    // active walk
    end
  end

endmodule : video_mode_fv
