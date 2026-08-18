// zhao_raster_div255.sv — exact `floor(n / 255)` for the RGB565 resolve
// quantizer. Instantiated 3× (once per colour channel) by
// zhao_raster_resolve.
//
// Law: reference/src/zrender/resolve.cpp — the resolve oracle divides by 255
// with C++ integer division on a NON-NEGATIVE numerator, i.e. floor:
//
//     r5 = min(31, (r*31 + B*16 + 8) / 255)
//     g6 = min(63, (g*63 + B*32 + 16) / 255)
//     b5 = min(31, (b*31 + B*16 + 8) / 255)
//
// A divider is not synthesisable at one pixel per clock, so the RTL uses the
// classic reciprocal identity
//
//     floor(n / 255) = (n + (n >> 8) + 1) >> 8
//
// which is EXACT — not an approximation — over the range this block uses.
//
// ---------------------------------------------------------------------------
// WHY IT IS EXACT, AND WHERE IT STOPS BEING EXACT
// ---------------------------------------------------------------------------
// Write n = 255·q + s with 0 ≤ s ≤ 254 (so q = floor(n/255) is the answer).
// Since 255·q = 256·q − q,
//
//     n >> 8 = floor((256q − q + s) / 256) = q + floor((s − q) / 256)
//
// so the numerator of the shift is
//
//     n + (n >> 8) + 1 = 256·q + (s + 1 + floor((s − q)/256))
//
// and the identity holds exactly when that bracket lies in [0, 255]:
//
//   · q ≤ s        ⇒ floor((s−q)/256) = 0  and the bracket is s+1 ∈ [1,255] ✓
//   · s < q ≤ s+256 ⇒ floor((s−q)/256) = −1 and the bracket is s   ∈ [0,254] ✓
//   · q > s+256    ⇒ the bracket can reach −1 and the identity FAILS.
//
// The failure needs q ≥ 257, i.e. n ≥ 255·257 = 65,535. This block ships
// W = 15 (n ≤ 32,767 ⇒ q ≤ 128), so the identity is exact with a factor-of-2
// margin, and the widest numerator the resolve ever forms is
// 255·63 + 15·32 + 16 = 16,561 (green — the widest channel), q ≤ 64.
//
// tests/formal/raster_resolve_div255.sby proves `q·255 ≤ n < (q+1)·255` for
// EVERY n at the shipping width — the bound argument above is a theorem the
// solver discharges, not a comment the reader has to trust. This is a
// separate module for exactly that reason, the same way zhao_raster_fill is:
// the proof and the silicon are the same bytes.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_raster_resolve).

module zhao_raster_div255 #(
  // Numerator width. Must be 9 ≤ W ≤ 16 (see the exactness proof above:
  // W = 16 is the last width at which the identity is still total).
  parameter int unsigned W = 15
) (
  input  logic [W-1:0] n_i,
  output logic [W-1:0] q_o   // floor(n_i / 255)
);

  localparam int unsigned SUM_W = W + 1;      // n + (n>>8) + 1 never overflows
  localparam int unsigned SHF_W = W - 8;      // width of n_i >> 8
  localparam int unsigned QUO_W = SUM_W - 8;  // width of the shifted result

  // sum[7:0] is the discarded remainder of the >>8 — the quotient is the only
  // thing this module exists to produce.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [SUM_W-1:0] sum;
  /* verilator lint_on UNUSEDSIGNAL */

  always_comb begin
    sum = {1'b0, n_i} +
          {{(SUM_W-SHF_W){1'b0}}, n_i[W-1:8]} +
          {{(SUM_W-1){1'b0}}, 1'b1};
    q_o = {{(W-QUO_W){1'b0}}, sum[SUM_W-1:8]};
  end

endmodule : zhao_raster_div255
