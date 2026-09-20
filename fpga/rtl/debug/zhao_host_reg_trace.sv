// zhao_host_reg_trace.sv -- HOST.REGWIN tenant 1: DEBUG.TRACE's host readout.
//
// Law: spec/memory_rules.md section 8.2 (the frozen tenant-1 map), owner ruling
// R51. This is the readout half of `zhao_console_core` header entry I45, whose
// own text said of it: "THE READOUT IS I19'S SHAPE EXACTLY ... the host is the
// HPS and no register path from HPS to this block exists. Closing one closes
// both, and they should be closed together rather than twice." They are.
//
// ---------------------------------------------------------------------------
// THE MAP
// ---------------------------------------------------------------------------
//   words 0x000..0x1FF   the ring, word-addressed exactly as DEBUG.TRACE
//                        addresses it: {event[5:0], word[2:0]}. Eight 32-bit
//                        words per event, the capture layout of
//                        spec/capture_format.md chunk 0x000A:
//                          0 tile  1 primitive  2 pixel  3 {rsv[3],stage}
//                          4 expected_fx  5 actual_fx  6 source_id  7 seq
//   word  0x200          armed       the seven-bit stage mask now armed
//   word  0x201          count       events STORED
//   word  0x202          dropped     events lost to a full ring
//   anything else        REFUSED and counted by the window
//
// READ-ONLY, AND THE ARMING IS DELIBERATELY NOT HERE. Owner ruling R52
// ratified `DebugTraceArm` 0xF003 in the reserved debug opcode range and put
// the arming in the COMMAND stream, lowered by CMD.EXEC. Ruling R18's principle
// -- one authority per level -- then forbids a second arming path through this
// aperture, so a write here is refused and counted. `armed` is READABLE here,
// which is the useful half: a host can confirm what the command took without
// being able to change it behind the command stream's back.
//
// WHY THE RING IS NOT DRAINED INTO DRAM INSTEAD. DEBUG.TRACE's own header says
// "writing events into the trace arena is MEM.HPS.BRIDGE's job downstream,
// which is why this block's output is a stream rather than an address". That
// remains the right answer for a LONG capture. It is the wrong answer for the
// 64-event ring the block actually is: a burst engine that moves 2 KiB needs a
// descriptor, an arena, a completion and a reader, and every one of those is a
// thing to get wrong for a buffer the ARM can read in 512 word reads between
// frames. The stream output is untouched and still available to whoever builds
// the arena path.
//
// THE READ IS REGISTERED IN THE RING, ONE CYCLE. `rd_data_o` is valid the cycle
// after `rd_addr_i` is presented (the entry is read whole into a register and
// the word selected after it, with the selector delayed to match). So this
// block presents the address on its ACK cycle and answers on the next.
//
// Conservative SystemVerilog subset only (charter section 2).
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_host_reg_trace).
// ENFORCED-BY: tests/debug/host_regwin_directed.cpp

module zhao_host_reg_trace #(
    parameter int unsigned OFFW  = 10,  // word-offset width from the window
    parameter int unsigned DEPTH = 64   // DEBUG.TRACE ring depth, in events
) (
    input  logic clk,
    input  logic rst_n,

    // ---- window side -------------------------------------------------------
    input  logic            sel_i,
    input  logic [OFFW-1:0] woff_i,
    input  logic            write_i,
    output logic            ack_o,
    output logic            rvalid_o,
    output logic [31:0]     rdata_o,
    output logic            err_o,

    // ---- DEBUG.TRACE side --------------------------------------------------
    output logic [$clog2(DEPTH*8)-1:0] rd_addr_o,
    input  logic [31:0]                rd_data_i,
    input  logic [6:0]                 armed_i,
    input  logic [31:0]                count_i,
    input  logic [31:0]                dropped_i
);

  localparam int unsigned RAW         = $clog2(DEPTH * 8);
  localparam int unsigned RING_WORDS  = DEPTH * 8;
  localparam int unsigned STATUS_BASE = 'h200;

  // synthesis translate_off
  initial begin
    if (RING_WORDS > STATUS_BASE)
      $fatal(1, "zhao_host_reg_trace: DEPTH=%0d overruns the frozen status base",
             DEPTH);
    if (OFFW < 10)
      $fatal(1, "zhao_host_reg_trace: OFFW=%0d cannot address the status block",
             OFFW);
  end
  // synthesis translate_on

  localparam logic [1:0] S_IDLE = 2'd0;
  localparam logic [1:0] S_RING = 2'd1;  // address presented; ring answers next
  localparam logic [1:0] S_RESP = 2'd2;

  logic [1:0]     st_q;
  logic [31:0]    rdata_q;
  logic           err_q;
  logic           ring_q;   // the response in flight is a ring word
  logic [RAW-1:0] raddr_q;

  logic is_ring_c;
  logic is_status_c;

  assign is_ring_c   = ({{(32-OFFW){1'b0}}, woff_i} < RING_WORDS);
  assign is_status_c = ({{(32-OFFW){1'b0}}, woff_i} >= STATUS_BASE)
                    && ({{(32-OFFW){1'b0}}, woff_i} <  (STATUS_BASE + 3));

  assign rd_addr_o = raddr_q;
  assign rdata_o   = ring_q ? rd_data_i : rdata_q;
  assign err_o     = err_q;
  assign rvalid_o  = (st_q == S_RESP);
  assign ack_o     = sel_i && (st_q == S_IDLE);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q    <= S_IDLE;
      rdata_q <= '0;
      err_q   <= 1'b0;
      ring_q  <= 1'b0;
      raddr_q <= '0;
    end else begin
      case (st_q)
        S_IDLE: begin
          if (sel_i) begin
            rdata_q <= '0;
            ring_q  <= 1'b0;
            if (write_i) begin
              // READ-ONLY: arming is DebugTraceArm 0xF003's, ruling R52.
              err_q <= 1'b1;
              st_q  <= S_RESP;
            end else if (is_ring_c) begin
              raddr_q <= woff_i[RAW-1:0];
              ring_q  <= 1'b1;
              err_q   <= 1'b0;
              st_q    <= S_RING;
            end else if (is_status_c) begin
              err_q <= 1'b0;
              case (woff_i[1:0])
                2'd0:    rdata_q <= {25'd0, armed_i};
                2'd1:    rdata_q <= count_i;
                default: rdata_q <= dropped_i;
              endcase
              st_q <= S_RESP;
            end else begin
              err_q <= 1'b1;
              st_q  <= S_RESP;
            end
          end
        end

        S_RING: begin
          // One cycle: rd_data_i is now the addressed word. It is muxed
          // combinationally into rdata_o rather than re-registered, so the
          // response carries the word the address asked for and cannot be a
          // cycle out of step with it.
          st_q <= S_RESP;
        end

        default: begin  // S_RESP
          st_q   <= S_IDLE;
          ring_q <= 1'b0;
        end
      endcase
    end
  end

endmodule
