// zhao_raster_blend.sv — one blend channel: the destination byte already in
// the tile, the shaded source byte, and a unit8 factor → the byte that goes
// back into the tile. Instantiated 3× (red, green, blue) by
// zhao_raster_fragment.
//
// Law:
//   spec/qformats.md §2 — `unit8` is U 0.0.8 and its VALUE IS raw/256, not
//       raw/255. Every factor in this module is a unit8: `a_i` is the
//       fragment's alpha after the recipe's modulation.
//   spec/qformats.md §3/§4 — `rescale_s(x, 8) = (x + 128) >>> 8`, ONE
//       rounding, round-half-up (ties toward +infinity), arithmetic shift.
//       This is the machine's only rounding primitive and the lerp below is
//       the same shape as the two lerps ratified before it: the fog mix
//       (`c' = sat_u8(c + rescale_s((fog_c − c)·f8, 8))`, qformats §8) and
//       the global-tint mix (`lit' = sat_u8(lit + rescale_s((tint_c − lit)·s,
//       8))`, spec/sky_and_beams.md §4a). Nothing new is invented here; the
//       blend is that same frozen form with the source colour in place of the
//       fog/tint colour.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §8 "Non-negotiable 3D basics" —
//       "deterministic blend rounding". This module is where that determinism
//       lives, and it is a separate file so the property can be proved on it.
//
// ---------------------------------------------------------------------------
// THE FOUR MODES, AND WHICH RATIFIED RECIPE ASKED FOR EACH
// ---------------------------------------------------------------------------
//   0 REPLACE  out = src
//              spec/sky_and_beams.md §1.1 `sky_backdrop` ("blend off") and
//              spec/stars_and_flares.md §1 `star_disc_masked` (masked, not
//              blended — the alpha test decides, not a factor).
//   1 ALPHA    out = sat_u8(dst + rescale_s((src − dst)·a, 8))
//              — algebraically `out = dst·(1−a) + src·a` with a = a_i/256,
//              which is spec/sky_and_beams.md §1.1 `sky_cloud_fade` written
//              in this machine's unit8 lane with its single rounding.
//   2 ADD      out = sat_u8(dst + src)
//              spec/sky_and_beams.md §2 `beam_additive_fade` ("dst =
//              sat(dst + src)") and spec/stars_and_flares.md §1
//              `star_halo_additive` ("dst = sat(dst+src)"). THE SATURATION IS
//              THE RECIPE, not an overflow: a beam crossing a bright sky is
//              supposed to rail, and §26's no-OIT refusal is moot precisely
//              because addition commutes and saturates.
//   3 ADD_MOD  out = sat_u8(dst + rescale_u(src·a, 8))
//              spec/sky_and_beams.md §1.1 `sun_additive` ("dst = sat(dst +
//              src·tex.a)") — the sun quad's pre-baked alpha modulates the
//              added energy.
//
// ---------------------------------------------------------------------------
// WHY THIS IS /256 AND THE RESOLVE IS /255
// ---------------------------------------------------------------------------
// A frequent and expensive confusion, so it is written down. `zhao_raster_
// div255` and `zhao_raster_quant` divide by 255 because they are QUANTIZERS:
// they map an 8-bit channel whose full scale IS 255 onto a 5- or 6-bit field
// whose full scale IS 31 or 63, and 255 is the ratio of those scales. This
// module divides by 256 because it is a WEIGHTING: its factor is a `unit8`,
// whose value spec/qformats.md §2 defines as raw/256. The two are different
// lanes of the same spec and neither is a rounding of the other. A blend that
// used /255 would disagree with the fog and tint mixes, which are the same
// operation on the same colour8 lanes.
//
// The visible consequence, stated so no test is surprised by it: a_i = 255 is
// 255/256, NOT 1.0. `ALPHA` at a_i = 255 with dst = 0, src = 255 gives 254,
// not 255. That is the unit8 law, identical to what the ratified fog mix does
// at f8 = 255, and it is asserted rather than worked around.
//
// ---------------------------------------------------------------------------
// THE RAIL
// ---------------------------------------------------------------------------
// One clamp to [0, 255] at the end, on a signed 10-bit accumulator wide
// enough for every mode's exact range:
//   REPLACE  [0, 255]      ALPHA  [−254, 509]
//   ADD      [0, 510]      ADD_MOD[0, 509]
// ALPHA cannot actually leave [0, 255] — it is a lerp, and
// tests/formal/raster_fragment_blend.sby proves it never overshoots either
// endpoint — but it is clamped anyway, for the reason zhao_raster_quant
// states about its own belt-and-braces clamp: a datapath that relies on one
// mode's arithmetic slack is one parameter change from wrapping.
//
// Conservative SystemVerilog subset only (charter §2); no dependencies.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_raster_fragment).

module zhao_raster_blend (
  input  logic [1:0] mode_i,  // 0 REPLACE, 1 ALPHA, 2 ADD, 3 ADD_MOD
  input  logic [7:0] dst_i,   // the byte already in the tile
  input  logic [7:0] src_i,   // the shaded source byte
  input  logic [7:0] a_i,     // unit8 factor (value = a_i/256)
  output logic [7:0] out_o
);

  // ---- SPLIT INTO TWO HALVES, 2026-08-31 ---------------------------------
  // THIS WRAPPER IS BIT-IDENTICAL TO WHAT IT REPLACED. It wires the two halves
  // together combinationally, so `zhao_raster_blend` still computes exactly
  // what it always did and tests/formal/raster_fragment_blend.sby still proves
  // the SHIPPING blend rather than a copy.
  //
  // The halves exist so RASTER.FRAGMENT can put a REGISTER between them.
  // reports/MHZArchitected names the split point precisely:
  //
  //   F2 BLEND PRODUCT  Register the three signed blend products and bypass
  //                     values.
  //   F3 FINISH         Apply +128, shifts, accumulator, saturation.
  //
  // The measured worst path at 49ad539 ran RAM read -> tests -> THIS
  // multiply (3.785 ns, the largest single element) -> the accumulator's carry
  // chain -> RAM write, all in one clock, for a 14.361 ns data path that has
  // to fall under 7.95. Splitting at the product is what halves it.
  //
  // "Every arithmetic operation stays in the same order and at the same
  // width. Nothing about rounding or capture CRCs changes." -- and that is
  // enforced by this wrapper existing: if the halves ever disagree with the
  // original, the blend's own tests and formal proof fail here.
  logic signed [17:0] prod_w;

  zhao_raster_blend_prod u_prod (
    .mode_i (mode_i),
    .dst_i  (dst_i),
    .src_i  (src_i),
    .a_i    (a_i),
    .prod_o (prod_w)
  );

  zhao_raster_blend_fin u_fin (
    .mode_i (mode_i),
    .dst_i  (dst_i),
    .src_i  (src_i),
    .prod_i (prod_w),
    .out_o  (out_o)
  );

endmodule : zhao_raster_blend
