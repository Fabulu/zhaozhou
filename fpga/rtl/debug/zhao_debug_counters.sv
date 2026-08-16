// zhao_debug_counters.sv — DEBUG.COUNTERS: D9 aggregation + the vblank
// read-mux window (plan W2.6, decision D9).
//
// Law (in citation order):
//   spec/counters.md — the whole file: counter_id = catalog index (u16,
//       blocks.yml counter_catalog; append-only, never renumbered);
//       distributed counters (each block owns + increments its own
//       registers; NO global event bus); the broadcast frame_tick latches
//       shadows; the read window streams (counter_id, u64) pairs ascending;
//       ownerless counters read 0; reading never affects the live counters.
//   design/contracts/DEBUG.COUNTERS.md — the block contract.
//
// Protocol (D9 capture timing CORRECTED 2026-08-16): every implemented
// provider (CMD.SCHEDULER x3, CMD.DMA x3, AUDIO.FIFO x1) latches its
// shadow REGISTERS at the frame_tick edge and presents the fresh value
// with a ONE-CYCLE valid pulse on the cycle AFTER the tick. This module
// therefore captures the provider channels ONE CYCLE AFTER the pulse
// (cap_pend below) — sampling AT the pulse cycle, as this module
// originally did and as its own directed test modelled (valid driven
// combinationally at the tick), finds every real provider's registered
// valid still LOW and captures NOTHING once the shell composes the two
// halves. The captured bank (the shadows of the frame just ended, stable
// until the next tick — each provider's registers change only at the
// tick; upheld per-provider, spec/counters.md 3.2) then streams every
// catalog id ascending, one beat per clock, ready/valid. A frame_tick
// during a sweep re-captures and RESTARTS the sweep (ascending-order law
// is unconditional; the stalled-read-retries-next-vblank rule of the
// contract is the backpressure path). An out-of-catalog provider id is a
// protocol violation: flagged on cat_violation_o (sticky until reset),
// never silently mapped — there is no fallback value.
// ENFORCED-BY: tests/debug/debug_counters_directed.cpp
//
// Parameters exist so the formal/future lanes can size the bank; committed
// defaults are the Phase-2 law (PROV_N = the wave-2 shadow providers:
// scheduler x3, dma x3, fifo x1, input x2 — the shell ties the unused
// tail to '0; CATALOG_IDS = blocks.yml counter_catalog size, 40 at wave 2).
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_debug_counters).

module zhao_debug_counters
#(
  parameter int unsigned PROV_N = 4,
  parameter int unsigned CATALOG_IDS = 40
) (
  input  logic clk,
  input  logic rst_n,

  // the broadcast frame boundary (latch trigger, spec/counters.md 3;
  // only the pulse is consumed — frame_id/repeated are VIDEO-side law)
  /* verilator lint_off UNUSEDSIGNAL */
  input  zhao_pkg::zhao_frame_tick_t frame_tick_i,
  /* verilator lint_on UNUSEDSIGNAL */

  // provider shadow channels (each block's latched set; valid is a
  // ONE-CYCLE pulse on the cycle after frame_tick, carrying the freshly
  // latched shadow — the capture below samples exactly that cycle).
  // Provider ids must be unique and in-catalog.
  input  zhao_pkg::zhao_counter_snap_t prov_i [0:PROV_N-1],

  // the read-mux window: one (counter_id, u64) beat per clock, ascending
  output logic                       snap_valid_o,
  input  logic                       snap_ready_i,
  output zhao_pkg::zhao_counter_snap_t snap_o,
  output logic                       window_open_o,   // sweep in progress

  // out-of-catalog provider id seen (protocol violation; sticky)
  output logic                       cat_violation_o
);

  // ------------------------------------------------------------ state -----
  /* verilator lint_off PROCASSINIT */
  logic [63:0] bank [0:CATALOG_IDS-1] = '{default: 64'd0};
  logic [15:0] sweep_idx = 16'd0;
  logic        sweeping  = 1'b0;
  logic        violation = 1'b0;
  logic        cap_pend  = 1'b0;
  /* verilator lint_on PROCASSINIT */

  // ------------------------------------------------------ sequential ------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int i = 0; i < CATALOG_IDS; i++) bank[i] <= 64'd0;
      sweep_idx <= 16'd0;
      sweeping <= 1'b0;
      violation <= 1'b0;
      cap_pend <= 1'b0;
    end else begin
      // capture one cycle AFTER the pulse — the cycle the real providers
      // present their freshly latched shadows (header: corrected timing)
      cap_pend <= frame_tick_i.pulse;
      if (cap_pend) begin
        // capture the provider set into the stable shadow bank
        for (int p = 0; p < PROV_N; p++) begin
          if (prov_i[p].valid) begin
            if ({16'd0, prov_i[p].counter_id} < 32'(CATALOG_IDS)) begin
              bank[int'(prov_i[p].counter_id)] <= prov_i[p].value;
            end else begin
              violation <= 1'b1;  // no silent fallback (contract law)
            end
          end
        end
        // (re)start the ascending sweep — a tick mid-sweep restarts it
        sweep_idx <= 16'd0;
        sweeping <= 1'b1;
      end else if (sweeping) begin
        if (snap_ready_i) begin
          if ((sweep_idx + 16'd1) >= 16'(CATALOG_IDS)) begin
            sweeping <= 1'b0;
            sweep_idx <= 16'd0;
          end else begin
            sweep_idx <= sweep_idx + 16'd1;
          end
        end
      end
    end
  end

  // ------------------------------------------------------- read window ----
  assign window_open_o = sweeping;
  assign snap_valid_o = sweeping;
  assign snap_o.valid = 1'b1;
  assign snap_o.counter_id = sweep_idx;
  assign snap_o.value = bank[int'(sweep_idx)];
  assign cat_violation_o = violation;

endmodule : zhao_debug_counters
