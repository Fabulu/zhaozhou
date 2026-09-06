// zhao_raster_ticketq.sv — the small flop FIFO the scheduled reciprocal uses
// instead of scanning an array.
//
// ENFORCED-BY: tests/raster/raster_rcp24_v3_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY A FIFO AND NOT A SCAN
// ---------------------------------------------------------------------------
// `zhao_raster_rcp24_svc` finds work by walking its context table three times
// per clock: once for a free slot, once for a pending micro-job, once for a
// finished token. TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt S10.2 names that
// structure and removes it: "Use a free-context FIFO, a NEW queue, and a
// CONTINUATION queue. [...] There is no context-wide free scan, ready scan, or
// completion scan." S22.1 lists the cone it is meant to delete:
// "scheduling scan -> wide operand mux -> multiplier".
//
// A scan's depth grows with the context count, which is the reason eight
// contexts felt like a ceiling. A FIFO's head is a register.
//
// ---------------------------------------------------------------------------
// PRELOAD IS THE FREE LIST
// ---------------------------------------------------------------------------
// The free-context queue must come out of reset holding every context id. That
// is the same FIFO with a different reset, not a different structure, so it is
// a parameter rather than a second module that can drift.
//
// ---------------------------------------------------------------------------
// ERR IS STICKY AND IS AN OUTPUT
// ---------------------------------------------------------------------------
// Every queue here is sized so it CANNOT overflow: each context owns at most
// one live ticket, so no queue can ever hold more than NCTX entries. That is an
// argument, and an argument is exactly the kind of thing that is quietly wrong
// after a scheduling change. `err_o` latches a push-into-full or a
// pop-from-empty so the claim is measured every run rather than believed.
`default_nettype none

module zhao_raster_ticketq #(
    parameter int unsigned W = 8,
    parameter int unsigned D = 16,
    // Reset leaves the queue FULL, holding 0, 1, ... D-1. Requires W >= log2(D).
    parameter bit PRELOAD = 1'b0
) (
    input var logic clk,
    input var logic rst_n,

    input var logic         push_i,
    input var logic [W-1:0] din_i,
    input var logic         pop_i,

    output var logic [W-1:0] dout_o,
    output var logic         empty_o,
    output var logic         full_o,
    output var logic         err_o
);

  localparam int unsigned PW = $clog2(D);

  logic [W-1:0]  mem_q [D];
  logic [PW-1:0] head_q, tail_q;
  logic [PW:0]   count_q;

  assign empty_o = (count_q == '0);
  assign full_o  = (count_q == (PW + 1)'(D));
  assign dout_o  = mem_q[head_q];

  logic do_push, do_pop;
  assign do_push = push_i && !full_o;
  assign do_pop  = pop_i && !empty_o;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      head_q  <= '0;
      tail_q  <= '0;
      err_o   <= 1'b0;
      if (PRELOAD) begin
        for (int unsigned i = 0; i < D; ++i) mem_q[i] <= W'(i);
        count_q <= (PW + 1)'(D);
      end else begin
        count_q <= '0;
      end
    end else begin
      if (do_push) begin
        mem_q[tail_q] <= din_i;
        tail_q        <= (tail_q == PW'(D - 1)) ? '0 : tail_q + PW'(1);
      end
      if (do_pop) begin
        head_q <= (head_q == PW'(D - 1)) ? '0 : head_q + PW'(1);
      end
      count_q <= count_q + (PW + 1)'(do_push) - (PW + 1)'(do_pop);
      if ((push_i && full_o) || (pop_i && empty_o)) err_o <= 1'b1;
    end
  end

endmodule : zhao_raster_ticketq

`default_nettype wire
