// zhao_input_snac.sv -- INPUT.SNAC, the PS1/SNAC peripheral adapter AND the
// merge into the canonical pad snapshot path.
//
// Law, in citation order:
//   reports/OWNER-RULINGS-20260919-EVENING.md R7 -- "INPUT.SNAC, GEOM.WARP,
//       POST.ECHO: Build all three (owner, explicit). They stay mandatory; the
//       2026-09-18 revocation stands."
//   reports/OWNER-RULINGS-COMPLETE-20260831.md 6.6 -- the constraint that
//       survives R7 and is the whole shape of this block: "It must emit the
//       same canonical PadFrame and may not create a second input semantics."
//   spec/input_rules.md 4 -- "Keyboard fallback (ZEmu) and the optional SNAC
//       adapter (INPUT.SNAC) must produce THIS table and THIS PadFrame -- one
//       canonical form, adapters normalize." The 32-bit button table is 4's.
//   spec/input_rules.md 1 -- the stick convention: "raw signed axis, no
//       deadzone, no calibration, no remapping in hardware -- the exact raw
//       sample travels to software unchanged (determinism law: hardware
//       applies zero policy)".
//   spec/input_rules.md 2.3 -- `input_sequence_gaps` "counts each observed gap
//       on the capture/tool side AND IN THE INPUT.SNAC MERGE PATH". That
//       sentence has been in the ratified spec since 2026-08-14 and names this
//       block's counter; it is implemented below rather than invented.
//   spec/input_rules.md 7 -- the SNAC bus, poll cadence and merge law, written
//       for this block (2026-09-20).
//   design/contracts/INPUT.SNAC.md -- the block contract.
//   design/blocks.yml INPUT.SNAC -- `inputs: [snac_pins]`, `outputs:
//       [pad_pins]`, `downstream: [INPUT.SNAPSHOT]`, `target_throughput: 1
//       poll per frame`, `counters: [input_snac_input_sequence_gaps]`,
//       `async_bridge: true`.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS, AND WHAT IT DELIBERATELY IS NOT
// ---------------------------------------------------------------------------
// It is a SECOND ROUTE to the pad state INPUT.SNAPSHOT already latches, not a
// second input semantics. Everything downstream of `pad_*_o` is unchanged: the
// canonical button table, the raw-stick convention, the atomic frame_tick
// latch, the PadFrame layout and the sequence law all stay INPUT.SNAPSHOT's.
// This block converts a PS1 controller's wire encoding into that form and
// hands it to the same consumer, on the same wires, in the same cycle it would
// otherwise have carried the host route's sample.
//
// It is NOT a policy stage. There is no deadzone, no calibration, no remap and
// no filtering here, because input_rules.md 1 forbids all four in hardware.
// The one transform that exists -- the 8-bit PS1 axis to the canonical i16 --
// is LOSSLESS AND INVERTIBLE (see AXIS below), which is what keeps it a
// re-encoding rather than a policy.
//
// ---------------------------------------------------------------------------
// THE MERGE LAW, AND WHY THIS SHAPE RATHER THAN A MUX SOMEBODY ELSE OWNS
// ---------------------------------------------------------------------------
// Per slot: a slot SNAC has a live pad on is driven by SNAC; every other slot
// passes the incoming host route through UNCHANGED, bit for bit, combinationally.
// So with no SNAC hardware attached -- DAT idles high, every poll reads 0xFF,
// every slot reads absent -- this block is the identity function on the whole
// pad bus, and the console behaves exactly as it did before it existed. That
// property is asserted directly (input_snac_directed case 1) rather than
// argued, because "it is transparent when idle" is the claim a reader will
// want to check first.
//
// The merge lives HERE rather than in the composer because a composer that
// contained a pad mux would be inventing an input law in a file whose whole
// discipline is that it invents none. The ledger says so too: `outputs:
// [pad_pins]`, `downstream: [INPUT.SNAPSHOT]` -- the adapter's declared output
// IS the pad bus, not a side channel somebody else has to merge.
//
// ---------------------------------------------------------------------------
// THE BUS (spec/input_rules.md 7.1)
// ---------------------------------------------------------------------------
// The console is the HOST. It drives /ATT (one per port, active low), CLK and
// CMD; the pad drives DAT and /ACK. Bytes are 8 bits LSB FIRST. CLK idles
// high; the host presents a CMD bit on the FALLING edge and samples DAT on the
// RISING edge -- so does the pad, in the other direction.
//
// One poll, byte by byte (7.2):
//   0  TX 0x01  address the controller           RX ignored
//   1  TX 0x42  read command                     RX = mode byte
//   2  TX 0x00                                   RX = 0x5A, the pad's ready byte
//   3  TX 0x00                                   RX = buttons low,  ACTIVE LOW
//   4  TX 0x00                                   RX = buttons high, ACTIVE LOW
//   5  TX 0x00   (mode 0x73 only)                RX = right stick X
//   6  TX 0x00   (mode 0x73 only)                RX = right stick Y
//   7  TX 0x00   (mode 0x73 only)                RX = left  stick X
//   8  TX 0x00   (mode 0x73 only)                RX = left  stick Y
//
// Mode byte: 0x41 digital (no sticks), 0x73 analog (four axes). 0xFF is the
// idle bus -- NO PAD, and deliberately NOT an error: an unpopulated port is
// the normal case and counting it would make `snac_bad_header_o` useless.
// Anything else IS counted, because a pad that answered with a mode this block
// does not implement is a real thing somebody needs to see.
//
// ---------------------------------------------------------------------------
// AXIS: THE ONE TRANSFORM, AND WHY IT IS NOT POLICY
// ---------------------------------------------------------------------------
//   canonical_i16 = {ps1_u8, 8'h00} ^ 16'h8000     (read as signed)
//
// PS1 axes are u8 with 0x80 at centre, 0x00 = left/up, 0xFF = right/down.
// The canonical axis (input_rules.md 1) is i16 with 0 at centre, negative =
// left, positive = down. The expression above maps 0x80 -> 0, 0x00 -> -32768
// and 0xFF -> +32512, monotonically, with ONE XOR and no arithmetic.
//
// It is invertible: `ps1_u8 == (canonical_i16 >>> 8) + 128` for every one of
// the 256 inputs, so no information is added, removed or rounded away. That is
// the test input_rules.md 1's "the exact raw sample travels to software
// unchanged" has to pass, and it is asserted over all 256 values
// (input_snac_directed case 3) rather than spot-checked.
//
// The asymmetry (-32768 low, +32512 high) is the PS1's OWN: its 0..255 range
// has 128 codes below centre and 127 above. Re-centring it would be
// calibration, which hardware may not apply. It is declared here rather than
// smoothed away.
//
// ---------------------------------------------------------------------------
// THE GAP COUNTER, AND WHY ITS TWO OPERANDS DO NOT MOVE TOGETHER
// ---------------------------------------------------------------------------
// `input_snac_input_sequence_gaps_o` is input_rules.md 2.3's merge-path gap.
// Each slot carries a poll sequence that advances once per COMPLETED poll. At
// each frame_tick, a slot that was present at the previous tick and is present
// now and whose poll sequence DID NOT ADVANCE is a frame that reused the
// previous frame's sample -- the bus ran slower than the display did, and a
// snapshot's worth of input was never taken.
//
// CLAUDE.md's law about a detector whose two operands move together applies
// directly and was considered before the counter was written. The two sides
// here are loaded by DIFFERENT events: `poll_seq` advances on the SERIAL
// ENGINE's completion, `poll_seq_at_tick` is loaded on the FRAME TICK. Neither
// enable participates in the other, so the comparison is about TIMING -- which
// is the fault a poll cadence actually has -- and not merely about values.
//
// IT FIRES ON LEGAL STIMULUS, so it owes no mutant: raise `CLK_DIV` (or shrink
// the tick interval) until a poll takes longer than a frame and it moves.
// input_snac_directed case 6 does exactly that and asserts the count.
// ENFORCED-BY: tests/input/input_snac_directed.cpp case 6
//
// ---------------------------------------------------------------------------
// ONE SERIAL ENGINE, TIME-MULTIPLEXED ACROSS THE PORTS
// ---------------------------------------------------------------------------
// A real PS1 bus is polled one port at a time -- /ATT selects exactly one --
// so a per-port engine would be silicon spent to do something the protocol
// forbids doing in parallel. One engine walks the ports round-robin. At the
// default divider two ports cost ~57,600 gpu cycles per full round against a
// 251,520-cycle Z60 frame (spec/video_rules.md 2), so the round completes
// about four times per frame with margin.
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall`.

module zhao_input_snac #(
  // ---- EVERY KNOB IS NAMED AND EDITABLE (CLAUDE.md rule 6) -----------------
  // How many SNAC connectors the board carries. They drive canonical pad slots
  // 0..PORTS-1; slots above that are always the host route's.
  parameter int PORTS       = 2,
  // gpu_clk cycles per SNAC half-bit. 200 gives 250 kHz at a 100 MHz gpu_clk,
  // which is the PS1 bus rate. Lower it in a bench; it changes no decoded value.
  parameter int CLK_DIV     = 200,
  // gpu_clk cycles to wait for the pad's /ACK after a byte before abandoning
  // the poll and calling the slot absent. ~100 us at 100 MHz.
  parameter int ACK_TIMEOUT = 10000,
  // gpu_clk cycles of /ATT-high between two polls (the PS1 inter-poll gap).
  parameter int IDLE_GAP    = 1600
) (
  input  logic clk,
  input  logic rst_n,

  // ==========================================================================
  // THE PHYSICAL SNAC EDGE -- pins of the part, not a boundary
  // ==========================================================================
  // Same class of edge as `pad_buttons_i` and `hps_req_*`: a connector on the
  // board. `zhao_console_core`'s header records why that is not a register
  // entry, at the HOST.REGWIN composition.
  output logic [PORTS-1:0] snac_att_n_o,   // /ATT, active low, one per port
  output logic             snac_clk_o,     // shared clock, idles HIGH
  output logic             snac_cmd_o,     // host -> pad, LSB first
  input  logic [PORTS-1:0] snac_dat_i,     // pad -> host (open drain, idles high)
  input  logic [PORTS-1:0] snac_ack_n_i,   // pad -> host, active-low pulse

  // The broadcast frame boundary, as a BARE PULSE rather than the packed
  // `zhao_frame_tick_t`. This block stamps no `frame_id` and reads no
  // `repeated`, so taking the struct would carry two fields nobody here reads
  // -- the uncashed-cheque shape, in miniature, and a lint finding besides.
  // `zhao_input_snapshot` takes the struct because it DOES stamp the id.
  input  logic frame_tick_i,

  // ==========================================================================
  // THE CANONICAL PAD BUS, IN -- the route that exists today
  // ==========================================================================
  input  logic [3:0]  host_pad_present_i,
  input  logic [31:0] host_pad_buttons_i [0:3],
  input  logic [15:0] host_pad_lx_i [0:3],
  input  logic [15:0] host_pad_ly_i [0:3],
  input  logic [15:0] host_pad_rx_i [0:3],
  input  logic [15:0] host_pad_ry_i [0:3],

  // ==========================================================================
  // THE CANONICAL PAD BUS, OUT -- merged, and INPUT.SNAPSHOT's input
  // ==========================================================================
  output logic [3:0]  pad_present_o,
  output logic [31:0] pad_buttons_o [0:3],
  output logic [15:0] pad_lx_o [0:3],
  output logic [15:0] pad_ly_o [0:3],
  output logic [15:0] pad_rx_o [0:3],
  output logic [15:0] pad_ry_o [0:3],

  // ==========================================================================
  // OBSERVABILITY
  // ==========================================================================
  output logic [3:0]  snac_present_o,   // which slots SNAC is currently driving
  output logic [63:0] snac_polls_o,     // polls that completed and decoded
  output logic [63:0] snac_timeouts_o,  // polls abandoned waiting for /ACK
  output logic [63:0] snac_bad_header_o,// a pad answered with a mode we do not implement
  output logic [63:0] snac_overrides_o, // ticks where SNAC replaced a HOST-present slot
  output logic [63:0] input_snac_input_sequence_gaps_o
);

  // ---- elaboration guards. Quartus 17.0 needs these inside `initial`
  // (CLAUDE.md build note: a bare module-scope `if` is a syntax error there).
  initial begin
    if (PORTS < 1 || PORTS > 4)
      $fatal(1, "zhao_input_snac: PORTS must be 1..4 (the canonical path has four slots)");
    if (CLK_DIV < 2)
      $fatal(1, "zhao_input_snac: CLK_DIV must be >= 2 (one half-bit needs a settled edge)");
    if (ACK_TIMEOUT < CLK_DIV * 4)
      $fatal(1, "zhao_input_snac: ACK_TIMEOUT must outlast several half-bits or every poll aborts");
    if (IDLE_GAP < 2)
      $fatal(1, "zhao_input_snac: IDLE_GAP must be >= 2");
  end

  // ---- the protocol's own constants, named ---------------------------------
  localparam logic [7:0] TX_ADDRESS = 8'h01;  // address the controller
  localparam logic [7:0] TX_READ    = 8'h42;  // read command
  localparam logic [7:0] TX_PAD     = 8'h00;  // filler for the pad's reply bytes
  localparam logic [7:0] MODE_DIGITAL = 8'h41;
  localparam logic [7:0] MODE_ANALOG  = 8'h73;
  localparam logic [7:0] MODE_NOPAD   = 8'hFF;  // idle bus: not an error
  localparam logic [7:0] READY_BYTE   = 8'h5A;

  localparam int DIVW = (CLK_DIV   <= 2) ? 2 : $clog2(CLK_DIV + 1);
  localparam int ACKW = $clog2(ACK_TIMEOUT + 1);
  localparam int GAPW = (IDLE_GAP  <= 2) ? 2 : $clog2(IDLE_GAP + 1);
  localparam int PW   = (PORTS     <= 1) ? 1 : $clog2(PORTS);

  // ==========================================================================
  // PER-SLOT DECODED STATE -- the adapter's own view of each pad
  // ==========================================================================
  logic [3:0]  s_present;
  logic [31:0] s_buttons [0:3];
  logic [15:0] s_lx [0:3];
  logic [15:0] s_ly [0:3];
  logic [15:0] s_rx [0:3];
  logic [15:0] s_ry [0:3];
  logic [15:0] s_poll_seq [0:3];   // advances once per COMPLETED poll
  logic [15:0] s_seq_tick [0:3];   // the value observed at the PREVIOUS tick
  logic [3:0]  s_present_tick;     // presence observed at the PREVIOUS tick

  // ==========================================================================
  // INPUT SYNCHRONISERS -- the `async_bridge: true` half of the ledger row
  // ==========================================================================
  // DAT and /ACK are driven by a cable, so they are asynchronous to gpu_clk
  // whatever the bus rate is. Two flops each, and the SAMPLED copy is the only
  // one the engine ever reads.
  logic [PORTS-1:0] dat_meta, dat_sync;
  logic [PORTS-1:0] ackn_meta, ackn_sync;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      dat_meta  <= {PORTS{1'b1}};
      dat_sync  <= {PORTS{1'b1}};
      ackn_meta <= {PORTS{1'b1}};
      ackn_sync <= {PORTS{1'b1}};
    end else begin
      dat_meta  <= snac_dat_i;
      dat_sync  <= dat_meta;
      ackn_meta <= snac_ack_n_i;
      ackn_sync <= ackn_meta;
    end
  end

  // ==========================================================================
  // THE SERIAL ENGINE
  // ==========================================================================
  typedef enum logic [2:0] {
    SN_IDLE,     // /ATT high between polls
    SN_SELECT,   // /ATT low, settling before the first clock
    SN_BIT,      // shifting one byte, half-bit by half-bit
    SN_ACK,      // waiting for the pad's /ACK after a byte
    SN_DONE      // decode and retire the poll
  } snac_st_e;

  snac_st_e        st;
  logic [PW-1:0]   port_sel;       // which connector the engine is on
  logic [3:0]      byte_idx;       // 0..8 within the poll
  logic [2:0]      bit_idx;        // 0..7, LSB first
  logic            half;           // 0 = clock LOW half, 1 = clock HIGH half
  logic [DIVW-1:0] div_cnt;
  logic [ACKW-1:0] ack_cnt;
  logic [GAPW-1:0] gap_cnt;
  logic [7:0]      tx_byte;
  logic [7:0]      rx_byte;
  logic            poll_ok;        // the poll is still well formed
  logic            poll_analog;    // mode byte was 0x73
  logic [7:0]      rx_mode, rx_btn_lo, rx_btn_hi;
  logic [7:0]      rx_rjx, rx_rjy, rx_ljx, rx_ljy;

  // the byte this step transmits (the poll is a fixed script, table 7.2)
  function automatic logic [7:0] tx_for(input logic [3:0] idx);
    case (idx)
      4'd0:    tx_for = TX_ADDRESS;
      4'd1:    tx_for = TX_READ;
      default: tx_for = TX_PAD;
    endcase
  endfunction

  // the last byte index of this poll: 4 for digital, 8 for analog. Until the
  // mode byte has landed the engine assumes ANALOG, so a 0x73 pad is not cut
  // short; a 0x41 pad stops at 4 the moment its mode byte is known.
  logic [3:0] last_idx;
  assign last_idx = (byte_idx >= 4'd2 && !poll_analog) ? 4'd4 : 4'd8;

  // PS1 -> canonical axis. One XOR, lossless and invertible (AXIS above).
  function automatic logic [15:0] axis16(input logic [7:0] a);
    axis16 = {a, 8'h00} ^ 16'h8000;
  endfunction

  // PS1 button bytes -> the canonical 32-bit table (input_rules.md 4).
  // Both PS1 bytes are ACTIVE LOW: a set bit means RELEASED.
  function automatic logic [31:0] buttons32(input logic [7:0] lo, input logic [7:0] hi);
    logic [15:0] b;
    begin
      b[ 0] = ~lo[4];  // up
      b[ 1] = ~lo[6];  // down
      b[ 2] = ~lo[7];  // left
      b[ 3] = ~lo[5];  // right
      b[ 4] = ~hi[6];  // A / cross
      b[ 5] = ~hi[5];  // B / circle
      b[ 6] = ~hi[7];  // X / square
      b[ 7] = ~hi[4];  // Y / triangle
      b[ 8] = ~hi[0];  // L2
      b[ 9] = ~hi[1];  // R2
      b[10] = ~hi[2];  // L1
      b[11] = ~hi[3];  // R1
      b[12] = ~lo[1];  // L3
      b[13] = ~lo[2];  // R3
      b[14] = ~lo[0];  // select
      b[15] = ~lo[3];  // start
      // bits 16..31 are RESERVED ZERO in Phase 2 (input_rules.md 4). A PS1 pad
      // carries nothing that belongs there, so they are zero because the table
      // says so, not because the data ran out.
      buttons32 = {16'h0000, b};
    end
  endfunction

  // which canonical slot the engine's current port drives
  logic [1:0] sel_slot;
  assign sel_slot = 2'(port_sel);

  // ---- bus outputs ---------------------------------------------------------
  // /ATT is low only for the selected port while a poll is in flight. CLK idles
  // HIGH and CMD idles HIGH, which is the unselected bus state a real pad sees.
  logic att_active;
  assign att_active = (st == SN_SELECT) || (st == SN_BIT) || (st == SN_ACK);

  always_comb begin
    for (int p = 0; p < PORTS; p++)
      snac_att_n_o[p] = !(att_active && (32'(port_sel) == p));
  end

  // CLK is high in the idle/settle states and follows `half` while shifting.
  assign snac_clk_o = (st == SN_BIT) ? half : 1'b1;
  // CMD presents the current bit LSB-first for the whole bit time; the pad
  // samples it on the rising edge.
  assign snac_cmd_o = (st == SN_BIT) ? tx_byte[bit_idx] : 1'b1;

  // ==========================================================================
  // COUNTERS
  // ==========================================================================
  logic [63:0] c_polls, c_timeouts, c_badhdr, c_overrides, c_gaps;

  assign snac_polls_o      = c_polls;
  assign snac_timeouts_o   = c_timeouts;
  assign snac_bad_header_o = c_badhdr;
  assign snac_overrides_o  = c_overrides;
  assign input_snac_input_sequence_gaps_o = c_gaps;
  assign snac_present_o    = s_present;

  // ==========================================================================
  // THE ENGINE, THE DECODE AND THE TICK BOOKKEEPING
  // ==========================================================================
  // One `always_ff`, because the poll's retire and the slot's state update are
  // the same event and splitting them is how a decoded byte ends up beside the
  // previous poll's presence bit.
  logic [63:0] gap_add;
  logic [63:0] override_add;
  logic [2:0]  gap_sum;
  logic [2:0]  override_sum;
  logic [3:0]  gap_hit;
  logic [3:0]  override_hit;

  always_comb begin
    for (int i = 0; i < 4; i++) begin
      // a present slot whose poll sequence did not move between two ticks
      gap_hit[i] = s_present[i] && s_present_tick[i] && (s_poll_seq[i] == s_seq_tick[i]);
      // SNAC took a slot the host route also claimed (worth seeing: it means
      // two physical routes are populated for one player)
      override_hit[i] = s_present[i] && host_pad_present_i[i];
    end
  end

  assign gap_sum = 3'({2'b00, gap_hit[0]} + {2'b00, gap_hit[1]}
                    + {2'b00, gap_hit[2]} + {2'b00, gap_hit[3]});
  assign override_sum = 3'({2'b00, override_hit[0]} + {2'b00, override_hit[1]}
                         + {2'b00, override_hit[2]} + {2'b00, override_hit[3]});
  assign gap_add      = {61'd0, gap_sum};
  assign override_add = {61'd0, override_sum};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st          <= SN_IDLE;
      port_sel    <= '0;
      byte_idx    <= 4'd0;
      bit_idx     <= 3'd0;
      half        <= 1'b0;
      div_cnt     <= '0;
      ack_cnt     <= '0;
      gap_cnt     <= '0;
      tx_byte     <= TX_ADDRESS;
      rx_byte     <= 8'h00;
      poll_ok     <= 1'b1;
      poll_analog <= 1'b1;
      rx_mode     <= MODE_NOPAD;
      rx_btn_lo   <= 8'hFF;
      rx_btn_hi   <= 8'hFF;
      rx_rjx      <= 8'h80;
      rx_rjy      <= 8'h80;
      rx_ljx      <= 8'h80;
      rx_ljy      <= 8'h80;
      s_present   <= 4'h0;
      s_present_tick <= 4'h0;
      for (int i = 0; i < 4; i++) begin
        s_buttons[i]  <= 32'h0;
        s_lx[i]       <= 16'h0;
        s_ly[i]       <= 16'h0;
        s_rx[i]       <= 16'h0;
        s_ry[i]       <= 16'h0;
        s_poll_seq[i] <= 16'h0;
        s_seq_tick[i] <= 16'h0;
      end
      c_polls     <= 64'd0;
      c_timeouts  <= 64'd0;
      c_badhdr    <= 64'd0;
      c_overrides <= 64'd0;
      c_gaps      <= 64'd0;
    end else begin
      // ---- the frame tick: the gap law and the override census -------------
      // Both are read AT the tick, against the values the PREVIOUS tick
      // recorded, so they measure the interval and not an instant.
      if (frame_tick_i) begin
        c_gaps      <= zhao_pkg::zhao_sat_add64(c_gaps, gap_add);
        c_overrides <= zhao_pkg::zhao_sat_add64(c_overrides, override_add);
        s_present_tick <= s_present;
        for (int i = 0; i < 4; i++) s_seq_tick[i] <= s_poll_seq[i];
      end

      // ---- the serial engine ----------------------------------------------
      case (st)
        SN_IDLE: begin
          if (gap_cnt >= GAPW'(IDLE_GAP - 1)) begin
            gap_cnt     <= '0;
            byte_idx    <= 4'd0;
            bit_idx     <= 3'd0;
            half        <= 1'b0;
            div_cnt     <= '0;
            tx_byte     <= tx_for(4'd0);
            rx_byte     <= 8'h00;
            poll_ok     <= 1'b1;
            poll_analog <= 1'b1;
            rx_mode     <= MODE_NOPAD;
            st          <= SN_SELECT;
          end else begin
            gap_cnt <= gap_cnt + GAPW'(1);
          end
        end

        // /ATT has gone low; give the pad one half-bit before the first clock.
        SN_SELECT: begin
          if (div_cnt >= DIVW'(CLK_DIV - 1)) begin
            div_cnt <= '0;
            st      <= SN_BIT;
          end else begin
            div_cnt <= div_cnt + DIVW'(1);
          end
        end

        // One half-bit per pass. `half` 0 is the clock-LOW half (the bit is
        // being presented in both directions); `half` 1 is the clock-HIGH
        // half, and the pad's DAT bit is captured on the transition INTO it.
        SN_BIT: begin
          if (div_cnt >= DIVW'(CLK_DIV - 1)) begin
            div_cnt <= '0;
            if (!half) begin
              // rising edge: sample DAT, LSB first
              rx_byte[bit_idx] <= dat_sync[port_sel];
              half <= 1'b1;
            end else begin
              half <= 1'b0;
              if (bit_idx == 3'd7) begin
                // The byte is complete. Bit 7 was captured CLK_DIV cycles ago,
                // on the previous half's rising edge, so `rx_byte` is whole by
                // the time SN_ACK retires it -- the retire is ordered after the
                // capture by a whole half-bit, not by an assumption.
                st      <= SN_ACK;
                ack_cnt <= '0;
              end else begin
                bit_idx <= bit_idx + 3'd1;
              end
            end
          end else begin
            div_cnt <= div_cnt + DIVW'(1);
          end
        end

        // The pad pulses /ACK after every byte except the last one it intends
        // to send. A timeout here is how an EMPTY PORT is detected: nothing
        // pulls DAT low, nothing acknowledges, and the poll is abandoned.
        SN_ACK: begin
          // retire the byte into its field, once, on entry to the wait
          if (ack_cnt == '0) begin
            case (byte_idx)
              4'd1: begin
                rx_mode <= rx_byte;
                if (rx_byte == MODE_ANALOG) begin
                  poll_analog <= 1'b1;
                end else if (rx_byte == MODE_DIGITAL) begin
                  poll_analog <= 1'b0;
                end else begin
                  poll_analog <= 1'b0;
                  poll_ok     <= 1'b0;
                end
              end
              // The ready byte is CHECKED and not stored: a register holding a
              // constant nobody reads is a lint finding and an invitation to
              // check the copy instead of the wire.
              4'd2: if (rx_byte != READY_BYTE) poll_ok <= 1'b0;
              4'd3: rx_btn_lo <= rx_byte;
              4'd4: rx_btn_hi <= rx_byte;
              4'd5: rx_rjx    <= rx_byte;
              4'd6: rx_rjy    <= rx_byte;
              4'd7: rx_ljx    <= rx_byte;
              4'd8: rx_ljy    <= rx_byte;
              default: ;  // byte 0's reply is the idle byte and carries nothing
            endcase
          end

          if (byte_idx >= last_idx) begin
            // the poll's last byte: no further /ACK is expected
            st <= SN_DONE;
          end else if (!ackn_sync[port_sel]) begin
            // acknowledged: next byte
            byte_idx <= byte_idx + 4'd1;
            bit_idx  <= 3'd0;
            half     <= 1'b0;
            div_cnt  <= '0;
            rx_byte  <= 8'h00;
            tx_byte  <= tx_for(byte_idx + 4'd1);
            st       <= SN_BIT;
          end else if (ack_cnt >= ACKW'(ACK_TIMEOUT - 1)) begin
            // nothing there. NOT counted as a bad header: an unpopulated port
            // is the normal case, and a counter that fires on it is noise.
            c_timeouts <= zhao_pkg::zhao_sat_add64(c_timeouts, 64'd1);
            poll_ok    <= 1'b0;
            st         <= SN_DONE;
          end else begin
            ack_cnt <= ack_cnt + ACKW'(1);
          end
        end

        SN_DONE: begin
          // ONE decision, ONE slot, ONE cycle: presence, buttons, sticks and
          // the poll sequence all land together, so nothing downstream can see
          // this poll's buttons beside the previous poll's presence.
          if (poll_ok && (rx_mode == MODE_DIGITAL || rx_mode == MODE_ANALOG)) begin
            s_present[sel_slot]  <= 1'b1;
            s_buttons[sel_slot]  <= buttons32(rx_btn_lo, rx_btn_hi);
            // A digital pad HAS no sticks. Centre is the honest reading, and
            // it is the same value input_rules.md 2.2 gives an absent axis.
            s_lx[sel_slot]       <= (rx_mode == MODE_ANALOG) ? axis16(rx_ljx) : 16'h0000;
            s_ly[sel_slot]       <= (rx_mode == MODE_ANALOG) ? axis16(rx_ljy) : 16'h0000;
            s_rx[sel_slot]       <= (rx_mode == MODE_ANALOG) ? axis16(rx_rjx) : 16'h0000;
            s_ry[sel_slot]       <= (rx_mode == MODE_ANALOG) ? axis16(rx_rjy) : 16'h0000;
            s_poll_seq[sel_slot] <= s_poll_seq[sel_slot] + 16'd1;
            c_polls              <= zhao_pkg::zhao_sat_add64(c_polls, 64'd1);
          end else begin
            // absent: zeroed fields, exactly as input_rules.md 2.2 requires of
            // an absent pad. The poll sequence FREEZES, so the gap detector
            // cannot fire on a slot that has no pad in it.
            s_present[sel_slot] <= 1'b0;
            s_buttons[sel_slot] <= 32'h0;
            s_lx[sel_slot]      <= 16'h0;
            s_ly[sel_slot]      <= 16'h0;
            s_rx[sel_slot]      <= 16'h0;
            s_ry[sel_slot]      <= 16'h0;
            // a pad that answered with a mode this block does not implement is
            // a real event; an empty port (0xFF) is not.
            if (rx_mode != MODE_NOPAD)
              c_badhdr <= zhao_pkg::zhao_sat_add64(c_badhdr, 64'd1);
          end
          // round-robin to the next connector
          port_sel <= (32'(port_sel) >= PORTS - 1) ? '0 : port_sel + PW'(1);
          gap_cnt  <= '0;
          st       <= SN_IDLE;
        end

        default: st <= SN_IDLE;
      endcase
    end
  end

  // ==========================================================================
  // THE MERGE -- combinational, per slot, and the identity when SNAC is idle
  // ==========================================================================
  always_comb begin
    for (int i = 0; i < 4; i++) begin
      pad_present_o[i] = s_present[i] ? 1'b1            : host_pad_present_i[i];
      pad_buttons_o[i] = s_present[i] ? s_buttons[i]    : host_pad_buttons_i[i];
      pad_lx_o[i]      = s_present[i] ? s_lx[i]         : host_pad_lx_i[i];
      pad_ly_o[i]      = s_present[i] ? s_ly[i]         : host_pad_ly_i[i];
      pad_rx_o[i]      = s_present[i] ? s_rx[i]         : host_pad_rx_i[i];
      pad_ry_o[i]      = s_present[i] ? s_ry[i]         : host_pad_ry_i[i];
    end
  end

endmodule : zhao_input_snac
