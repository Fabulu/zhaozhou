// zhao_input_rumble.sv — INPUT.RUMBLE, the rumble bridge (W2.3).
//
// Law: spec/input_rules.md 3 / design/contracts/INPUT.RUMBLE.md.
//   - Input is the executed DebugRumble 0xF004 payload
//     {u8 pad_index; u8 enable; u8 strength} (CMD.SCHEDULER dispatch; the
//     header flags-bit0 law is upstream of this port).
//   - FRAME-GATED: a command lands in per-pad pending registers; the duty
//     target latches at the NEXT frame_tick — mid-frame changes never reach
//     the pad path early (input atomicity property family).
//   - One update per frame per pad: a second command for the same pad in one
//     frame REPLACES the pending one (last-writer-wins, deterministic) and
//     the dropped one counts rumble_frames_dropped. pad_index > 3 drops the
//     request entirely + counts (never wraps onto another pad).
//   - duty = enable ? strength : 0 (exact integer arithmetic, no rounding).
//     No command in a frame => the previous target HOLDS (software owns stop
//     semantics — no auto-timeout in Phase 2).
//   - PWM: 8-bit phase free-running from reset, NEVER reset (a duty change
//     never glitches the carrier); rumble_pwm[i] high while phase < duty.
//     The 1 kHz carrier rate is the hardware-lane pad clock domain; in the
//     simulation profile one phase step = PWM_PHASE_DIV clk cycles (default
//     1) and the harness samples duty + pwm deterministically.
//
// rumble_frames_dropped: u64, saturating (spec/counters.md 4); the live
// counter increments at command time, the shadow (this port) latches at
// frame_tick per the D9 snapshot protocol — stable for a whole frame.
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall`.

module zhao_input_rumble
  import zhao_pkg::*;
  import zhao_abi_pkg::*;  // ABI re-export seam (same import pair as the
                           // snapshot bridge — the frozen zhao_pkg wildcard
                           // does not transitively re-export the ABI names)
#(
  parameter int unsigned PWM_PHASE_DIV = 1  // clk cycles per PWM phase step
)(
  input  logic clk,
  input  logic rst_n,

  // executed DebugRumble payload (one command per pulse)
  input  logic       rumble_cmd_valid,
  input  logic [7:0] rumble_pad_index,
  input  logic [7:0] rumble_enable,
  input  logic [7:0] rumble_strength,

  /* verilator lint_off UNUSEDSIGNAL */
  input  zhao_frame_tick_t frame_tick,  // pulse + frame_id; repeated unused
  /* verilator lint_on UNUSEDSIGNAL */

  // latched per-pad outputs: duty target (pad PHY out) + active flag
  output logic [7:0] rumble_duty [0:3],
  output logic [3:0] rumble_active,
  // modelled carrier (pad-domain PWM; duty/256)
  output logic [3:0] rumble_pwm,
  // D9 shadow of the drop counter (latched at frame_tick)
  output logic [63:0] rumble_frames_dropped
);

  // ------------------------------------------------------------ state -----
  logic [7:0] duty          [0:3];  // latched targets (the PHY-visible pair)
  logic [3:0] pend_valid;
  logic [3:0] pend_enable;          // folded enable bit per pending command
  logic [7:0] pend_strength [0:3];
  logic [63:0] dropped_live;
  logic [63:0] dropped_shadow;

  logic [7:0]              phase;
  logic [15:0]             div_cnt;  // PWM_PHASE_DIV-1 .. 0 down-counter

  localparam logic [15:0] PHASE_DIV = 16'(PWM_PHASE_DIV);

  // ---------------------------------------------------- PWM carrier -------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      phase   <= 8'd0;
      div_cnt <= 16'd0;
    end else begin
      if (div_cnt == 16'd0) begin
        div_cnt <= (PHASE_DIV > 16'd1) ? (PHASE_DIV - 16'd1) : 16'd0;
        phase   <= phase + 8'd1;      // free-running, never reset
      end else begin
        div_cnt <= div_cnt - 16'd1;
      end
    end
  end

  // ------------------------------------------------------- command path ---
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pend_valid     <= 4'd0;
      pend_enable    <= 4'd0;
      dropped_live   <= 64'd0;
      dropped_shadow <= 64'd0;
      for (int i = 0; i < 4; i++) begin
        pend_strength[i] <= 8'd0;
        duty[i]          <= 8'd0;     // motors off at reset
      end
    end else begin
      if (rumble_cmd_valid) begin
        if (rumble_pad_index > 8'd3) begin
          // out-of-range index: dropped entirely, never a wrap onto a pad
          if (dropped_live != 64'hFFFF_FFFF_FFFF_FFFF)
            dropped_live <= dropped_live + 64'd1;
        end else begin
          if (pend_valid[rumble_pad_index[1:0]]) begin
            // second command for this pad in one frame: replace + count
            if (dropped_live != 64'hFFFF_FFFF_FFFF_FFFF)
              dropped_live <= dropped_live + 64'd1;
          end
          pend_valid[rumble_pad_index[1:0]]  <= 1'b1;
          pend_enable[rumble_pad_index[1:0]] <= (rumble_enable != 8'd0);
          pend_strength[rumble_pad_index[1:0]] <= rumble_strength;
        end
      end

      // ------------------------------------------------- frame gate -------
      if (frame_tick.pulse) begin
        for (int i = 0; i < 4; i++) begin
          if (pend_valid[i]) begin
            duty[i]      <= pend_enable[i] ? pend_strength[i] : 8'd0;
            pend_valid[i] <= 1'b0;
          end
          // no pending command: previous target HOLDS
        end
        dropped_shadow <= dropped_live;  // D9 shadow latch
      end
    end
  end

  // ------------------------------------------------------- pad PHY out ----
  always_comb begin
    for (int i = 0; i < 4; i++) begin
      rumble_duty[i]   = duty[i];
      rumble_active[i] = (duty[i] != 8'd0);
      rumble_pwm[i]    = (duty[i] != 8'd0) && (phase < duty[i]);
    end
  end

  assign rumble_frames_dropped = dropped_shadow;

  // ------------------------------------------------------- formal ---------
  // Input atomicity property family (input_rules.md 3, scope note in the
  // INPUT.RUMBLE contract): duty targets change ONLY at frame_tick; there is
  // no other latch path. PWM phase advances every PWM_PHASE_DIV cycles
  // regardless (never reset).
`ifdef FORMAL
  logic f_past_valid = 1'b0;  // declared init: the free-init step 0 never enters $past checks
  always_ff @(posedge clk) f_past_valid <= 1'b1;

  // proof window: reset released once, never re-asserted (async-reset edges
  // are excluded from the stability properties, not forgotten)
  always_ff @(posedge clk) begin
    if (f_past_valid && $past(rst_n)) assume(rst_n);
  end

  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n && $past(rst_n)) begin
      if (!$past(frame_tick.pulse)) begin
        assert(rumble_duty[0] == $past(rumble_duty[0]));
        assert(rumble_duty[1] == $past(rumble_duty[1]));
        assert(rumble_duty[2] == $past(rumble_duty[2]));
        assert(rumble_duty[3] == $past(rumble_duty[3]));
        assert(rumble_frames_dropped == $past(rumble_frames_dropped));
      end
    end
  end
`endif

endmodule : zhao_input_rumble
