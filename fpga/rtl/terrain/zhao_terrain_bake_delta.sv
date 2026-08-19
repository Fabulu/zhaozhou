// zhao_terrain_bake_delta.sv — the incremental-scaling bake arithmetic, alone.
//
// Factored out of `zhao_terrain_bake` for the same reason
// `zhao_surface_blend` was factored out of `zhao_surface_stamp`: it is purely
// combinational, so `tests/formal/terrain_bake_delta.sby` can PROVE the
// identity the whole §9.2 cadence budget rests on instead of sampling it.
//
// Law:
//   reference/src/zterrain/terrain_core.cpp `bake_dig` — the lambda
//
//       g(depth) = rescale_s32(rescale_s32(depth.raw * s, 16), 8)
//       delta16  = g(depth_from) - g(depth_to)
//
//     reproduced here operator for operator. `s` is the stencil value in Q16
//     (0..65536); the caller owns where it came from.
//   spec/qformats.md §3/§4 — `rescale_s32(x, k)` is ONE round-half-up
//     arithmetic shift, `(x + 2^(k-1)) >>> k`, then saturate to the fx16 word.
//     Two rescales, two roundings, in that order — never one fused shift by
//     24, which rounds differently and would silently disagree.
//   spec/terrain_rules.md §9 — "the bake applies (to − from) × stencil so an
//     interrupted cast un-applies cleanly"; §9.2 law 3 — deferral is
//     state-exact because `from→mid` then `mid→to` ≡ `from→to`.
//
// WHY THE ABSOLUTE FORM IS LOAD-BEARING. §9's prose says `(to − from) × s`,
// which reads like `rescale((to − from) * s)`. `bake_dig` does NOT do that: it
// evaluates g at each ABSOLUTE depth and subtracts the two BAKED-BACK results.
// The two differ whenever a rounding boundary sits between the endpoints, and
// only the absolute form telescopes — `(g(a) − g(b)) + (g(b) − g(c))` collapses
// to `g(a) − g(c)` for free, while the difference form accumulates one rounding
// per step and a deferred Volcano would arrive at a different island than an
// undeferred one. terrain_rules §9.2 law 3 declares the deferral state-exact,
// so the absolute form is not a style preference; it is what makes the frozen
// BAKE_PATCH_BUDGET safe. `terrain_bake_delta.sby` P1 proves it here.
//
// WIDTHS, STATED RATHER THAN ASSUMED. `depth` is a full fx16 word (signed 32)
// and `s` is 0..65536, so `depth * s` needs signed 50. The first rescale's
// saturate is written faithfully but is UNREACHABLE for s <= 65536 (P5 proves
// it, and a cover task shows it DOES fire once s is unconstrained, so P5 is not
// vacuous): |depth * s| <= 2^47, and (2^47 + 2^15) >>> 16 = 2^31 exactly at the
// rail rather than past it. The SECOND rescale carries no saturate at all,
// because |a| <= 2^31 makes |(a + 128) >>> 8| <= 2^23 — a dead comparator would
// be lint noise pretending to be caution, so it is argued here instead.
//
// Conservative SystemVerilog subset only (charter §2).

module zhao_terrain_bake_delta (
    input logic [16:0] stencil_i,  // s, Q16, 0..65536 (the divider's bound)

    input logic signed [31:0] depth_from_i,  // fx16 raw
    input logic signed [31:0] depth_to_i,    // fx16 raw

    output logic signed [31:0] delta_o,  // height16 units: g(from) - g(to)
    output logic               sat_o     // the first rescale hit an fx16 rail
);

  // ---- g(depth), twice; one lane per endpoint --------------------------
  // signed 32 x unsigned 17: the stencil is zero-extended to signed 18 so the
  // product is an ordinary signed multiply (a Verilog multiply goes UNSIGNED
  // if either operand is, which is the trap that cost GEOM.BINNER 29 tiles).
  logic signed [17:0] s_ext;
  assign s_ext = $signed({1'b0, stencil_i});

  logic signed [49:0] p_from, p_to;
  assign p_from = depth_from_i * s_ext;
  assign p_to   = depth_to_i * s_ext;

  // rescale(., 16): (x + 2^15) >>> 16, then saturate to the fx16 word. The
  // discarded low bits ARE the rounding — the shift is the operator, not a
  // slice of a value someone forgot to read.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [49:0] q_from, q_to;
  /* verilator lint_on UNUSEDSIGNAL */
  assign q_from = p_from + 50'sd32768;
  assign q_to   = p_to + 50'sd32768;

  logic signed [33:0] a_from_raw, a_to_raw;
  assign a_from_raw = q_from[49:16];
  assign a_to_raw   = q_to[49:16];

  logic sat_from, sat_to;
  assign sat_from = (a_from_raw > 34'sd2147483647) || (a_from_raw < -34'sd2147483648);
  assign sat_to   = (a_to_raw > 34'sd2147483647) || (a_to_raw < -34'sd2147483648);
  assign sat_o    = sat_from || sat_to;

  logic signed [31:0] a_from, a_to;
  assign a_from = (a_from_raw > 34'sd2147483647)  ? 32'sh7FFF_FFFF :
                  (a_from_raw < -34'sd2147483648) ? 32'sh8000_0000 : a_from_raw[31:0];
  assign a_to = (a_to_raw > 34'sd2147483647)  ? 32'sh7FFF_FFFF :
                  (a_to_raw < -34'sd2147483648) ? 32'sh8000_0000 : a_to_raw[31:0];

  // rescale(., 8): the fx16 -> height16 bake-back (§9). No saturate: see the
  // header — |a| <= 2^31 bounds this by 2^23.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [32:0] t_from, t_to;  // low 8 bits discarded by the >>> 8
  /* verilator lint_on UNUSEDSIGNAL */
  assign t_from = $signed({a_from[31], a_from}) + 33'sd128;
  assign t_to   = $signed({a_to[31], a_to}) + 33'sd128;

  logic signed [24:0] g_from, g_to;
  assign g_from = t_from[32:8];
  assign g_to   = t_to[32:8];

  // g(from) - g(to): both are bounded by 2^23, so the difference is exact in
  // the fx16 word and there is nothing here to saturate. Sign-extended by
  // concatenation rather than a size cast — the landed RTL's idiom, and the
  // one Quartus 17.0 is happiest with.
  logic signed [31:0] g_from_ext, g_to_ext;
  assign g_from_ext = {{7{g_from[24]}}, g_from};
  assign g_to_ext   = {{7{g_to[24]}}, g_to};
  assign delta_o    = g_from_ext - g_to_ext;

endmodule : zhao_terrain_bake_delta
