// zhao_raster_quant.sv — one ordered-dither channel quantizer: 8-bit working
// colour + a 4×4 Bayer value → the RGB565 field for that channel.
// Instantiated 3× by zhao_raster_resolve (red, green, blue).
//
// Law: reference/src/zrender/resolve.cpp, which states the three channels
// explicitly:
//
//     r5 = min(31, (r*31 + (B*16 +  8)) / 255)
//     g6 = min(63, (g*63 + (B*32 + 16)) / 255)
//     b5 = min(31, (b*31 + (B*16 +  8)) / 255)
//
// i.e. `q = min(MAXQ, floor((v·MAXQ + B·AMP + RND) / 255))`.
//
// AMP AND RND ARE PARAMETERS, NOT A FORMULA. The oracle's header calls green
// out by hand — "its dither amplitude doubles (32 vs 16) while its
// quantization headroom halves" — and green's 32/16 is NOT what the stated
// threshold t = (B+0.5)/16 of ONE quantization step would give (that is
// 16/8 for every channel, since one output level is 255 numerator units for
// all three). Deriving AMP from MAXQ would therefore be INVENTING a law that
// contradicts the oracle. The oracle is the law; these are its constants,
// carried as parameters so the resolve block names them at the instantiation
// and nothing here has to guess. See design/contracts/RASTER.RESOLVE.md
// ("Q formats and rounding") for the full note.
//
// THE CLAMP IS THE 2026-08-16 WHITE RAIL. Without it, green at B ≥ 8 with
// g ≥ 252 quantizes to 64, which WRAPS in a 6-bit field: full white resolved
// to a white/magenta (0xFFFF/0xF81F) pixel checkerboard. Only green can
// actually exceed its field — the 5-bit channels have exact headroom
// (255·31 + 248 = 8,153 < 32·255 = 8,160) — but all three are clamped
// exactly as the oracle clamps them, because a resolve that relies on one
// channel's arithmetic slack is one parameter change from wrapping.
//
// This is a separate module for the same reason zhao_raster_fill is:
// tests/formal/raster_resolve_quant.sby proves, on THIS module at BOTH
// shipping parameter sets and for every one of the 4,096 (v, B) inputs, that
//   · the result is exactly `min(MAXQ, floor(num/255))`, and
//   · it can NEVER exceed MAXQ — the white rail as a theorem, not a comment.
// The proof and the silicon are the same bytes.
//
// Conservative SystemVerilog subset only (charter §2); depends only on
// zhao_raster_div255. Lint: clean under `-Wall` (lint_raster_resolve).

module zhao_raster_quant #(
  parameter int unsigned MAXQ = 31,  // 31 for a 5-bit channel, 63 for green
  parameter int unsigned QW   = 5,   // RGB565 field width: 5, or 6 for green
  parameter int unsigned AMP  = 16,  // Bayer amplitude: 16 (5-bit) / 32 (green)
  parameter int unsigned RND  = 8    // rounding term:    8 (5-bit) / 16 (green)
) (
  input  logic [7:0]    v_i,      // 8-bit working colour for this channel
  input  logic [3:0]    bayer_i,  // the 4×4 Bayer value at this pixel, 0..15
  output logic [QW-1:0] q_o       // the RGB565 field, 0..MAXQ
);

  // The widest numerator is green's: 255·63 + 15·32 + 16 = 16,561 < 2^15.
  localparam int unsigned NUM_W = 15;

  logic [NUM_W-1:0] num;
  logic [NUM_W-1:0] quo;

  // v·31 and v·63 fold to (v<<5)−v and (v<<6)−v; B·16 and B·32 are shifts.
  always_comb begin
    num = ({{(NUM_W-8){1'b0}}, v_i} * NUM_W'(MAXQ)) +
          ({{(NUM_W-4){1'b0}}, bayer_i} * NUM_W'(AMP)) +
          NUM_W'(RND);
  end

  zhao_raster_div255 #(.W(NUM_W)) u_div (.n_i(num), .q_o(quo));

  // The rail (resolve.cpp, 2026-08-16).
  always_comb begin
    q_o = (quo > NUM_W'(MAXQ)) ? QW'(MAXQ) : quo[QW-1:0];
  end

endmodule : zhao_raster_quant
