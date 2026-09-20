// zhao_host_reg_hist.sv -- HOST.REGWIN tenant 0: MEASURE.HISTOGRAM's readout.
//
// Law: spec/memory_rules.md section 8.1 (the frozen tenant-0 map), owner ruling
// R51. This is the block that closes `zhao_console_core` header entry I19,
// whose whole text was: "MEASURE.HISTOGRAM's host read window (`hist_rd_*`) --
// BOUNDARY. The host is the HPS; no register path from HPS to this block
// exists." The path now exists and this is its far end.
//
// ---------------------------------------------------------------------------
// THE ADDRESS IS THE BIN, AND THAT IS THE WHOLE DESIGN
// ---------------------------------------------------------------------------
// The obvious CSR shape for a windowed readout is a pair: write a bin index to
// a SELECT register, then read a DATA register. That shape is a race in a
// machine where the accumulator is live -- two hosts, or one host and an
// interrupt, interleave a select and a read and each gets the other's bin, with
// nothing anywhere able to notice. It is the metadata-swap defect of CLAUDE.md's
// own chapter, rebuilt on purpose.
//
// So there is no select register. Word offset N IS bin N, and a read is one
// transaction that carries its own index. Nothing is stateful between accesses,
// so nothing can be interleaved wrongly.
//
//   words 0x000..0x03F   bin[woff] -- the frozen interval's count for that bin,
//                        zero-extended from CW bits. Offsets at or above
//                        2**BINW are REFUSED (they are not bins; returning zero
//                        would make a map error indistinguishable from an empty
//                        bin, which is the flattering direction).
//   word  0x040          snap_total      events ACCEPTED into the interval
//   word  0x041          { snap_valid, 15'b0, snap_src_id }
//   word  0x042          snap_index      interval sequence number
//   word  0x043          events
//   word  0x044          updates
//   word  0x045          stall_cycles
//   word  0x046          bin_sat
//   word  0x047          fwd_hits
//   word  0x048          host_conflict
//   word  0x049          snapshots
//   word  0x04A          frozen_write
//   anything else        REFUSED and counted by the window
//
// READ-ONLY. A write is refused (`err_o`) rather than ignored, because a host
// that wrote a register and got an acknowledgement would be entitled to believe
// the write took.
//
// THE BIN READ IS A HANDSHAKE, NOT A REGISTER. `zhao_measure_histogram`'s
// `rd_ready_o` goes low for the one-time initialisation sweep, and its
// `rd_data_valid_o` arrives two cycles after acceptance. This block holds the
// request until it is accepted and the window simply waits -- which is why the
// window counts `stall_cycles_o`. The histogram's own `host_conflict_o` counts
// the accumulator groups a host read held off, and it is readable at word 0x048
// so the host can see the cost it is imposing.
//
// Conservative SystemVerilog subset only (charter section 2).
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_host_reg_hist).
// ENFORCED-BY: tests/debug/host_regwin_directed.cpp

module zhao_host_reg_hist #(
    parameter int unsigned OFFW = 10,  // word-offset width handed over by the window
    parameter int unsigned BINW = 6,   // histogram bin-index width
    parameter int unsigned CW   = 24   // histogram counter width
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

    // ---- MEASURE.HISTOGRAM side -------------------------------------------
    output logic            rd_valid_o,
    output logic [BINW-1:0] rd_bin_o,
    input  logic            rd_ready_i,
    input  logic            rd_data_valid_i,
    input  logic [CW-1:0]   rd_count_i,

    input  logic          snap_valid_i,
    input  logic [CW-1:0] snap_total_i,
    input  logic [15:0]   snap_src_id_i,
    input  logic [CW-1:0] snap_index_i,
    input  logic [CW-1:0] events_i,
    input  logic [CW-1:0] updates_i,
    input  logic [CW-1:0] stall_cycles_i,
    input  logic [CW-1:0] bin_sat_i,
    input  logic [CW-1:0] fwd_hits_i,
    input  logic [CW-1:0] host_conflict_i,
    input  logic [CW-1:0] snapshots_i,
    input  logic [CW-1:0] frozen_write_i
);

  // The map is FROZEN, so the status block sits at a fixed word and a
  // parameterisation that would collide with it is an elaboration failure
  // rather than a silent overlap.
  localparam int unsigned STATUS_BASE = 'h40;
  localparam int unsigned STATUS_N    = 'h0B;

  // synthesis translate_off
  initial begin
    if ((1 << BINW) > STATUS_BASE)
      $fatal(1, "zhao_host_reg_hist: BINW=%0d overruns the frozen status base",
             BINW);
    if (OFFW < 8)
      $fatal(1, "zhao_host_reg_hist: OFFW=%0d cannot address the status block",
             OFFW);
    if (CW > 32)
      $fatal(1, "zhao_host_reg_hist: CW=%0d does not fit a 32-bit register", CW);
  end
  // synthesis translate_on

  localparam logic [1:0] S_IDLE = 2'd0;
  localparam logic [1:0] S_BIN  = 2'd1;  // holding the bin request
  localparam logic [1:0] S_DATA = 2'd2;  // waiting for rd_data_valid_i
  localparam logic [1:0] S_RESP = 2'd3;

  logic [1:0]      st_q;
  logic [31:0]     rdata_q;
  logic            err_q;
  logic [BINW-1:0] bin_q;

  logic            is_bin_c;
  logic            is_status_c;
  logic [31:0]     status_c;
  logic [3:0]      sidx_c;

  assign is_bin_c    = ({{(32-OFFW){1'b0}}, woff_i} < (1 << BINW));
  assign is_status_c = ({{(32-OFFW){1'b0}}, woff_i} >= STATUS_BASE)
                    && ({{(32-OFFW){1'b0}}, woff_i} <  (STATUS_BASE + STATUS_N));
  // STATUS_BASE is 0x40, so the status index IS the low nibble of the offset.
  // Taking it that way rather than by subtraction keeps the expression the
  // exact width it needs and leaves no unused upper bits to reason about.
  assign sidx_c      = woff_i[3:0];

  always_comb begin
    case (sidx_c)
      4'h0:    status_c = {{(32-CW){1'b0}}, snap_total_i};
      4'h1:    status_c = {snap_valid_i, 15'd0, snap_src_id_i};
      4'h2:    status_c = {{(32-CW){1'b0}}, snap_index_i};
      4'h3:    status_c = {{(32-CW){1'b0}}, events_i};
      4'h4:    status_c = {{(32-CW){1'b0}}, updates_i};
      4'h5:    status_c = {{(32-CW){1'b0}}, stall_cycles_i};
      4'h6:    status_c = {{(32-CW){1'b0}}, bin_sat_i};
      4'h7:    status_c = {{(32-CW){1'b0}}, fwd_hits_i};
      4'h8:    status_c = {{(32-CW){1'b0}}, host_conflict_i};
      4'h9:    status_c = {{(32-CW){1'b0}}, snapshots_i};
      default: status_c = {{(32-CW){1'b0}}, frozen_write_i};
    endcase
  end

  assign rd_valid_o = (st_q == S_BIN);
  assign rd_bin_o   = bin_q;
  assign rdata_o    = rdata_q;
  assign err_o      = err_q;
  assign rvalid_o   = (st_q == S_RESP);

  // ACK is a single cycle and never coincides with RVALID: the window moves
  // S_SEL -> S_WAIT on the ack and only then looks at rvalid, so an adapter
  // that raised both together would have its response missed.
  assign ack_o = sel_i && (st_q == S_IDLE);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q    <= S_IDLE;
      rdata_q <= '0;
      err_q   <= 1'b0;
      bin_q   <= '0;
    end else begin
      case (st_q)
        S_IDLE: begin
          if (sel_i) begin
            rdata_q <= '0;
            if (write_i) begin
              // READ-ONLY. Refused, not ignored.
              err_q <= 1'b1;
              st_q  <= S_RESP;
            end else if (is_bin_c) begin
              bin_q <= woff_i[BINW-1:0];
              err_q <= 1'b0;
              st_q  <= S_BIN;
            end else if (is_status_c) begin
              rdata_q <= status_c;
              err_q   <= 1'b0;
              st_q    <= S_RESP;
            end else begin
              err_q <= 1'b1;
              st_q  <= S_RESP;
            end
          end
        end

        S_BIN: begin
          // Held until the histogram accepts it. rd_ready_i is low only during
          // the one-time init sweep; the window's timeout is the backstop.
          if (rd_ready_i) st_q <= S_DATA;
        end

        S_DATA: begin
          if (rd_data_valid_i) begin
            rdata_q <= {{(32-CW){1'b0}}, rd_count_i};
            err_q   <= 1'b0;
            st_q    <= S_RESP;
          end
        end

        default: begin  // S_RESP
          st_q <= S_IDLE;
        end
      endcase
    end
  end

endmodule
