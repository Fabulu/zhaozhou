// zhao_raster_quant.sv — one ordered-dither channel quantizer: 8-bit working
// colour + a 4×4 Bayer value → the RGB565 field for that channel.
// Instantiated 3× by zhao_raster_resolve (red, green, blue).
//
// Law: reference/src/zrender/resolve.cpp, which states the three channels
// explicitly:
//
//     r5 = min(31, (r*31 + (B*16 +  8)) / 255)
//     g6 = min(63, (g*63 + (B*16 +  8)) / 255)
//     b5 = min(31, (b*31 + (B*16 +  8)) / 255)
//
// i.e. `q = min(MAXQ, floor((v·MAXQ + B·AMP + RND) / 255))`.
//
// AMP AND RND ARE PARAMETERS, NOT A FORMULA -- and the reason is now the
// opposite of what this comment used to give.
//
// It used to argue that green's amplitude doubles (32 vs 16) as a hand-called
// exception, so AMP must not be derived. Since the 2026-08-16 white-rail fix
// that is no longer the oracle: resolve.cpp uses (B*16 + 8) for ALL THREE
// channels, because one quantization step is 255 numerator units for all
// three -- they all divide by 255 -- so (B + 0.5)/16 of a step is the same
// everywhere. The old 32/16 is exactly what wrapped green's six-bit field at
// full white.
//
// They stay PARAMETERS anyway, for a better reason than an exception: the
// oracle is the law, and these are its constants. Deriving AMP from MAXQ
// would make this module compute the law instead of carrying it, so a future
// change to resolve.cpp would leave the RTL silently disagreeing with a
// formula that still looked principled. Naming them at the instantiation
// means the resolve block cites the oracle rather than re-deriving it.
//
// See design/contracts/RASTER.RESOLVE.md ("Q formats and rounding").
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
  parameter int unsigned AMP  = 16,  // Bayer amplitude: 16, ALL channels
  parameter int unsigned RND  = 8    // rounding term:    8, ALL channels
) (
  input  logic [7:0]    v_i,      // 8-bit working colour for this channel
  input  logic [3:0]    bayer_i,  // the 4×4 Bayer value at this pixel, 0..15
  output logic [QW-1:0] q_o       // the RGB565 field, 0..MAXQ
);

  // The widest numerator is green's: 255·63 + 15·16 + 8 = 16,313 < 2^15.
  // (This line used to read 15·32 + 16 = 16,561, from the pre-2026-08-16
  // green amplitude. The width was and is correct either way -- the stale
  // figure was the LARGER one, so NUM_W was conservative rather than wrong --
  // but it is the justification for the width and should state the real
  // number.)
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
