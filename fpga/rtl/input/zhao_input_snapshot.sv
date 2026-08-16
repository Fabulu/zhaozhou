// zhao_input_snapshot.sv — INPUT.SNAPSHOT, the pad snapshot bridge (W2.3).
//
// Law: spec/input_rules.md (D5) / design/contracts/INPUT.SNAPSHOT.md.
//   - All four pad slots latch ATOMICALLY at the broadcast frame_tick (the
//     zhao_frame_tick_t pulse from VIDEO.FRAMECTL): a stick change that
//     arrives mid-frame is visible only in the NEXT snapshot, never
//     partially. Proven by tests/formal/input_snapshot_atomic.sby (BMC over
//     the `ifdef FORMAL properties below).
//   - sequence increments by exactly 1 per tick while a pad is present,
//     starting at 0 after reset, wrapping mod 2^16; it FREEZES while the pad
//     is absent (the absent frame carries the frozen value + zeroed fields).
//   - The latched array is the GENERATED zhao_pad_frame_t x4 (ABI struct
//     PadFrame, 20 B — never re-defined here). The HPS handoff is the
//     documented pad->hps async bridge: a double buffer + gray-coded pointer
//     swap at the tick (the readable copy is stable for a full frame by
//     construction — contract "Backpressure rules").
//
// Clocking: single gpu_clk domain in the Verilator profile (the harness tick
// scheduler models the pad polling domain; plan R1). The pad->HPS crossing
// itself is hardware-lane SYS.CDC territory; this module exposes the stable
// readable copy only.
//
// input_sequence_gaps: the merge-path gap detector (input_rules.md 2.3) —
// counts ticks where a present pad's latched sequence != prev+1. In this
// synchronous latch that condition is structurally impossible (both sides of
// the comparator come from the same incrementer); the formal property proves
// the counter stays 0 forever, which IS the "no gaps by construction" proof.
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall`.

module zhao_input_snapshot
  import zhao_pkg::*;
  import zhao_abi_pkg::*;  // zhao_pad_frame_t + pack (ABI re-export seam)
(
  input  logic clk,
  input  logic rst_n,

  // raw decoded pad state (canonical button table + raw sticks, zero policy
  // — input_rules.md 1/4). Sampled ONLY at frame_tick.
  input  logic [3:0]  pad_present,
  input  logic [31:0] pad_buttons [0:3],
  input  logic [15:0] pad_lx [0:3],
  input  logic [15:0] pad_ly [0:3],
  input  logic [15:0] pad_rx [0:3],
  input  logic [15:0] pad_ry [0:3],

  // broadcast frame boundary (spec/counters.md 3): pulse = one gpu cycle.
  // frame_id mirrors out; `repeated` is not consumed here (VIDEO/CMD law).
  /* verilator lint_off UNUSEDSIGNAL */
  input  zhao_frame_tick_t frame_tick,
  /* verilator lint_on UNUSEDSIGNAL */

  // latched snapshot: canonical PadFrame x4 (readable copy, stable all frame)
  output zhao_pad_frame_t pad_frame [0:3],
  // the same array as one packed vector, slot 0 in the LSBs — the pack is
  // the generated zhao_pack_pad_frame layout (byte-identity seam for the
  // harness / .zcap CONTROLLER_SNAPSHOT body)
  output logic [639:0]  pad_frame_flat,
  output logic [31:0]   pad_frame_id,
  output logic [15:0]   pad_sequence [0:3],

  // gap law (input_rules.md 2.3): shadow-stable counter + one-cycle event
  output logic [63:0]   input_sequence_gaps,
  output logic          input_sequence_gap_evt
);

  localparam logic [7:0] PAD_FLAG_PRESENT = 8'h01;

  // ------------------------------------------------------------ state -----
  // Source-of-truth sequences + last-latched mirror (gap comparator input).
  // Declared inits mirror the async-reset state so formal proofs (and FPGA
  // power-up) start from the coherent reset state: sequences 0, pads absent,
  // gaps 0 — the properties are theorems ABOUT the reset state onward.
  // PROCASSINIT is silenced deliberately: the init mirrors the reset branch
  // (verification start state), the reset branch remains the authority.
  /* verilator lint_off PROCASSINIT */
  logic [15:0] seq      [0:3] = '{default: 16'd0};
  logic [15:0] seq_prev [0:3] = '{default: 16'd0};

  // double buffer (contract: HPS reads a stable copy selected by the
  // gray-coded pointer; only the pointer toggles at the tick)
  zhao_pad_frame_t buf0 [0:3] = '{default: '0};
  zhao_pad_frame_t buf1 [0:3] = '{default: '0};
  logic            rd_sel = 1'b0;  // 1: readable copy is buf1 (toggles each tick)
  logic [31:0]     frame_id = 32'd0;

  logic [63:0] gaps = 64'd0;
  /* verilator lint_on PROCASSINIT */

  // ------------------------------------------------- next-frame decode ----
  zhao_pad_frame_t nf [0:3];
  logic [3:0] pad_gap;

  always_comb begin
    for (int i = 0; i < 4; i++) begin
      nf[i].pad_index  = 8'(i);
      nf[i].flags      = pad_present[i] ? PAD_FLAG_PRESENT : 8'h00;
      nf[i].sequence_f = pad_present[i] ? (seq[i] + 16'd1) : seq[i];
      nf[i].buttons    = pad_present[i] ? pad_buttons[i]    : 32'h0;
      nf[i].lx         = pad_present[i] ? pad_lx[i]         : 16'h0;
      nf[i].ly         = pad_present[i] ? pad_ly[i]         : 16'h0;
      nf[i].rx         = pad_present[i] ? pad_rx[i]         : 16'h0;
      nf[i].ry         = pad_present[i] ? pad_ry[i]         : 16'h0;
      nf[i].rsv        = 32'h0;
      // gap detector (merge-path law): present pad whose latched sequence is
      // not prev+1 — impossible in this latch; the comparator is real and
      // the formal property proves it never fires.
      pad_gap[i] = pad_present[i] && (nf[i].sequence_f != (seq_prev[i] + 16'd1));
    end
  end

  // saturating gap count (u64, spec/counters.md 4)
  logic [2:0] gap_sum;
  assign gap_sum = 3'({2'b00, pad_gap[0]} + {2'b00, pad_gap[1]}
                          + {2'b00, pad_gap[2]} + {2'b00, pad_gap[3]});

  // ------------------------------------------------------ sequential ------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int i = 0; i < 4; i++) begin
        seq[i]      <= 16'd0;
        seq_prev[i] <= 16'd0;
        buf0[i]     <= '{pad_index: 8'(i), flags: 8'h00, sequence_f: 16'd0,
                          buttons: 32'h0, lx: 16'd0, ly: 16'd0,
                          rx: 16'd0, ry: 16'd0, rsv: 32'h0};
        buf1[i]     <= '{pad_index: 8'(i), flags: 8'h00, sequence_f: 16'd0,
                          buttons: 32'h0, lx: 16'd0, ly: 16'd0,
                          rx: 16'd0, ry: 16'd0, rsv: 32'h0};
      end
      rd_sel   <= 1'b0;
      frame_id <= 32'd0;
      gaps     <= 64'd0;
    end else if (frame_tick.pulse) begin
      // THE atomic latch: sequences advance, the fresh array lands in the
      // off-read buffer, and the gray pointer swaps — one consistent unit.
      for (int i = 0; i < 4; i++) begin
        seq[i]      <= nf[i].sequence_f;
        seq_prev[i] <= nf[i].sequence_f;
      end
      if (rd_sel) begin
        for (int i = 0; i < 4; i++) buf0[i] <= nf[i];
      end else begin
        for (int i = 0; i < 4; i++) buf1[i] <= nf[i];
      end
      rd_sel   <= ~rd_sel;
      frame_id <= frame_tick.frame_id;
      if (gaps != 64'hFFFF_FFFF_FFFF_FFFF) begin
        if (((64'hFFFF_FFFF_FFFF_FFFF - gaps) >= {61'd0, gap_sum})) begin
          gaps <= gaps + {61'd0, gap_sum};
        end else begin
          gaps <= 64'hFFFF_FFFF_FFFF_FFFF;
        end
      end
    end
  end

  assign input_sequence_gaps     = gaps;
  assign input_sequence_gap_evt  = frame_tick.pulse && (|pad_gap);

  // ------------------------------------------------------- read side ------
  always_comb begin
    for (int i = 0; i < 4; i++) begin
      pad_frame[i]     = rd_sel ? buf1[i] : buf0[i];
      pad_sequence[i]  = pad_frame[i].sequence_f;
      pad_frame_flat[i*160 +: 160] = zhao_pack_pad_frame(pad_frame[i]);
    end
  end

  assign pad_frame_id = frame_id;

  // ------------------------------------------------------- formal ---------
  // input_snapshot_atomic (plan 4 / input_rules.md 2.1/2.3):
  //   (a) no latched bit changes between two consecutive frame_ticks,
  //   (b) sequence increments exactly once per tick while present,
  //       freezes while absent, and never moves without a tick,
  //   (c) the gap counter stays 0 (gaps impossible by construction).
`ifdef FORMAL
  logic f_past_valid = 1'b0;  // declared init: the free-init step 0 never enters $past checks
  always_ff @(posedge clk) f_past_valid <= 1'b1;

  // proof window: reset is released once and never re-asserted (an async
  // reset edge legally changes registers without a tick — excluded from the
  // properties, not forgotten; power-up reset is the only reset)
  always_ff @(posedge clk) begin
    if (f_past_valid && $past(rst_n)) assume(rst_n);
  end

  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n && $past(rst_n)) begin
      // (a) atomicity: outputs are stable unless the previous cycle ticked
      if (!$past(frame_tick.pulse)) begin
        assert(pad_frame_flat == $past(pad_frame_flat));
        assert(pad_frame_id   == $past(pad_frame_id));
      end
      // (b) sequence-exactly-once, per pad
      for (int i = 0; i < 4; i++) begin
        if ($past(frame_tick.pulse) && $past(pad_present[i])) begin
          assert(pad_sequence[i] == ($past(pad_sequence[i]) + 16'd1));
        end else if (!$past(frame_tick.pulse)) begin
          assert(pad_sequence[i] == $past(pad_sequence[i]));
        end
      end
      // (c) no gaps, ever
      assert(input_sequence_gaps == 64'd0);
    end
  end

  // ---- non-vacuity covers (added 2026-08-16, ratified process fix) ---------
  // Every assertion above is an implication guarded by $past(frame_tick.pulse)
  // and $past(rst_n). If the elaborated model cannot reach a tick — which is
  // EXACTLY how MEM.GUARD's proof was vacuous for a whole wave, and how this
  // lane's properties passed while never having run at all — all three hold
  // trivially and the proof means nothing. These covers are the witness that
  // the antecedents are reachable; the `cover` task in
  // tests/formal/input_snapshot_atomic.sby is what makes design/formal_runs.yml
  // able to record covers: true for this property (ledger rule V16).
  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      c_tick:          cover (frame_tick.pulse);
      c_tick_present:  cover (frame_tick.pulse && pad_present[0]);
      c_seq_advanced:  cover (pad_sequence[0] != 16'd0);
      c_two_ticks:     cover (pad_sequence[0] > 16'd1);
      c_pad_absent:    cover (frame_tick.pulse && !pad_present[0]);
    end
  end

  // ---- SELF-ASSERTING SCOPE GUARD (ledger rule V19; the arbiter
  // a_horizon_is_refresh_free / linebuf a_scope_four_sessions pattern) ----
  // The atomicity/sequence laws are proven at bmc depth 30 — at most ~30
  // tick/no-tick interleavings of the double-buffer gray-pointer swap.
  // IN PARTICULAR the u16 sequence wrap (65,536 ticks away) is far outside
  // this horizon: the "+1 mod 2^16" law is asserted but its wrap case is
  // NOT exercised by this proof (it is by tests/input/ lanes). This guard
  // PINS the proven window: raising `depth` makes it FIRE, so the number
  // cannot silently change meaning — a deeper proof must re-state its own
  // scope (and still will not reach the wrap), not merely re-run.
  logic [5:0] f_scope_cyc = 6'd0;
  always_ff @(posedge clk) begin
    if (f_scope_cyc != 6'h3F) f_scope_cyc <= f_scope_cyc + 6'd1;
  end
  always_comb begin
    a_scope_bmc_window : assert (f_scope_cyc <= 6'd30);
  end
`endif

endmodule : zhao_input_snapshot
