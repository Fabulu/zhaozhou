// raster_fragment_blend_fv.sv — formal harness for the RASTER.FRAGMENT blend
// channel (ZH-025; property raster_fragment_blend.sby).
//
// WHAT IS PROVED, and why it is not vacuous.
//
// The DUT is zhao_raster_blend — the EXACT module zhao_raster_fragment
// instantiates three times, once per colour channel. All four blend modes are
// elaborated at once by leaving `mode` free. The assertions are written
// against spec/qformats.md's own arithmetic in a WIDE signed lane, not
// against a restatement of the RTL's expression.
//
// The free inputs are mode (2 bits), dst (8), src (8) and a (8) — 26 bits,
// which is TOTAL rather than sampled: the destination and source bytes ARE
// u8 (the charter 8 working colour is 24-bit RGB, one byte a channel), the
// factor IS a unit8, and the mode field is exactly two bits wide. Free (mode,
// dst, src, a) ranges over every input the block can ever be handed — all
// 67,108,864 of them. There is no reachability gap for the solver to hide in.
//
//   P1  a_replace       REPLACE is exactly the source. The trivial mode,
//                       asserted because a datapath that quietly blended in
//                       "blend off" would break sky_backdrop and
//                       star_disc_masked, and nothing else here would notice.
//
//   P2  a_alpha_exact   ALPHA is EXACTLY `dst + rescale_s((src-dst)*a, 8)`
//                       with rescale_s the arithmetic-shift round-half-up of
//                       spec/qformats.md 4, computed here in a 32-bit signed
//                       lane. This is the property that catches the classic
//                       defect: rescaling the MAGNITUDE unsigned and
//                       re-applying the sign rounds ties the wrong way on the
//                       darkening half and differs by one LSB. It also
//                       catches swapped operands, a /255 divide, and a
//                       missing rounding term.
//
//   P3  a_alpha_bounded THE LERP NEVER OVERSHOOTS: the result always lies
//                       between dst and src inclusive. This is the theorem
//                       that justifies zhao_raster_blend's comment that
//                       ALPHA "cannot actually leave [0, 255]" — and it is a
//                       real theorem, not a restatement, because it is FALSE
//                       for the obvious near-misses (a rounding term of 255
//                       instead of 128 overshoots at the top of the range).
//                       A blend that overshot would make a cloud sheet
//                       brighter than either the cloud or the sky behind it.
//
//   P4  a_add_exact     ADD is exactly `min(255, dst + src)` — the additive
//                       saturation spec/sky_and_beams.md 2 and
//                       spec/stars_and_flares.md 1 both require by name. The
//                       equality is two-sided, so it catches BOTH a blend
//                       that wraps (255+1 -> 0, a black hole in a beam) and
//                       one that clamps too early.
//
//   P5  a_addmod_exact  ADD_MOD is exactly `min(255, dst + unit_mul(src, a))`
//                       with unit_mul spec/qformats.md 3's frozen
//                       `(src*a + 128) >> 8` — sun_additive's
//                       `dst = sat(dst + src*tex.a)`.
//
//   P6  a_add_monotone  NEITHER additive mode can DARKEN: the result is
//                       always >= dst. Additive light only ever adds. This is
//                       what makes charter 26's no-OIT refusal moot for the
//                       beam and halo layers — addition commutes AND is
//                       monotone, so draw order cannot lose energy.
//
//   P7  a_in_field      No mode can ever leave the 8-bit field. The rail as a
//                       theorem rather than as a pinned regression vector,
//                       exactly as zhao_raster_quant's white rail is.
//
// The cover task is load-bearing. Every assertion above is unconditional, so
// none can go vacuous through an unreachable antecedent — but the covers pin
// the interesting corners as REACHABLE anyway: the additive rail ACTUALLY
// FIRING (without it, P4's `min` and P7's field bound would also hold for a
// blend that never reached 255), the alpha lerp reaching BOTH endpoints
// (without which P3 holds for a blend that never moves), and the negative
// half of the lerp actually being taken (without which P2's signed rounding
// is never exercised).
//
// WHAT THIS DOES NOT PROVE, stated plainly: the mode ENCODING (which recipe
// selects which mode), the three tests, the tag and stencil paths, the
// pipeline, the stall that re-issues its read, and the counters are NOT
// proved here. They are covered by the differential lanes against
// zref::FragmentPipeline and by the mutation evidence in the contract. What
// IS proved is the arithmetic every blended fragment in the machine flows
// through, three times.
//
// Frontend: read_slang (the lane choice since W2.3). zhao_raster_blend.sv is
// self-contained — no package dependency, nothing staged or copied.

module raster_fragment_blend_fv (
  input logic       clk,
  input logic [1:0] mode_free,  // all four blend modes — unconstrained
  input logic [7:0] dst_free,   // the destination byte — a u8, unconstrained
  input logic [7:0] src_free,   // the shaded source byte — likewise
  input logic [7:0] a_free      // the unit8 factor — likewise
);

  localparam logic [1:0] BL_REPLACE = 2'd0;
  localparam logic [1:0] BL_ALPHA   = 2'd1;
  localparam logic [1:0] BL_ADD     = 2'd2;
  localparam logic [1:0] BL_ADD_MOD = 2'd3;

  // ---- the SHIPPING module, the exact bytes the fragment pipeline uses ----
  logic [7:0] out;
  zhao_raster_blend u_dut (
    .mode_i (mode_free),
    .dst_i  (dst_free),
    .src_i  (src_free),
    .a_i    (a_free),
    .out_o  (out)
  );

  // ---- THE LAW, in a wide signed lane (spec/qformats.md 2/3/4) -----------
  localparam int unsigned LW = 32;

  logic signed [LW-1:0] d, s, af;
  assign d  = $signed({{(LW-8){1'b0}}, dst_free});
  assign s  = $signed({{(LW-8){1'b0}}, src_free});
  assign af = $signed({{(LW-8){1'b0}}, a_free});

  // rescale_s(x, 8) = (x + 128) >>> 8 — round-half-up, ties toward +infinity,
  // ARITHMETIC shift. Written out here so the solver sees the spec's form and
  // not the RTL's.
  logic signed [LW-1:0] prod_a, prod_m, lerp_law, mod_law;
  assign prod_a   = (s - d) * af;
  assign prod_m   = s * af;
  assign lerp_law = d + ((prod_a + LW'(128)) >>> 8);
  assign mod_law  = d + ((prod_m + LW'(128)) >>> 8);

  // the two endpoints of the lerp, ordered
  logic signed [LW-1:0] lo, hi;
  assign lo = (d < s) ? d : s;
  assign hi = (d < s) ? s : d;

  logic signed [LW-1:0] add_law;
  assign add_law = ((d + s) > LW'(255)) ? LW'(255) : (d + s);

  logic signed [LW-1:0] addmod_law;
  assign addmod_law = (mod_law > LW'(255)) ? LW'(255) : mod_law;

  logic signed [LW-1:0] outw;
  assign outw = $signed({{(LW-8){1'b0}}, out});

  always_ff @(posedge clk) begin
    // P1 — REPLACE is the source, untouched.
    a_replace: assert (mode_free != BL_REPLACE || outw == s);

    // P2 — ALPHA is exactly the spec's single-rounded signed lerp.
    a_alpha_exact: assert (mode_free != BL_ALPHA || outw == lerp_law);

    // P3 — and that lerp NEVER overshoots either endpoint.
    a_alpha_bounded: assert (mode_free != BL_ALPHA || (outw >= lo && outw <= hi));

    // P4 — ADD is exactly min(255, dst + src): it saturates, and it saturates
    //      no earlier than it must.
    a_add_exact: assert (mode_free != BL_ADD || outw == add_law);

    // P5 — ADD_MOD is exactly min(255, dst + unit_mul(src, a)).
    a_addmod_exact: assert (mode_free != BL_ADD_MOD || outw == addmod_law);

    // P6 — neither additive mode can darken.
    a_add_monotone: assert ((mode_free != BL_ADD && mode_free != BL_ADD_MOD) || outw >= d);

    // P7 — the field bound, for every mode.
    a_in_field: assert (outw >= LW'(0) && outw <= LW'(255));
  end

  always_ff @(posedge clk) begin
    // THE ADDITIVE RAIL ACTUALLY FIRES. Without this cover, a_add_exact and
    // a_in_field would also hold for a blend that never reaches 255 — which
    // is not the theorem the beam and halo recipes need.
    c_add_rails:    cover (mode_free == BL_ADD && (d + s) > LW'(255) && out == 8'd255);
    c_addmod_rails: cover (mode_free == BL_ADD_MOD && mod_law > LW'(255) && out == 8'd255);
    // ...and that it does NOT rail when it should not.
    c_add_exact:    cover (mode_free == BL_ADD && (d + s) == LW'(255) && out == 8'd255);
    c_add_small:    cover (mode_free == BL_ADD && out > 8'd0 && out < 8'd255);

    // THE LERP ACTUALLY MOVES, in both directions, and reaches both ends.
    c_alpha_up:     cover (mode_free == BL_ALPHA && s > d && out > dst_free);
    c_alpha_down:   cover (mode_free == BL_ALPHA && s < d && out < dst_free);
    c_alpha_at_dst: cover (mode_free == BL_ALPHA && s != d && out == dst_free);
    c_alpha_at_src: cover (mode_free == BL_ALPHA && s != d && out == src_free);

    // The unit8 endpoint the tests pin by hand: a = 255 is 255/256, so white
    // over black is 254 and NOT 255.
    c_alpha_unit_endpoint:
      cover (mode_free == BL_ALPHA && a_free == 8'd255 && dst_free == 8'd0 &&
             src_free == 8'd255 && out == 8'd254);
    // ...and a = 0 is the exact identity.
    c_alpha_identity:
      cover (mode_free == BL_ALPHA && a_free == 8'd0 && dst_free != src_free &&
             out == dst_free);

    // THE NEGATIVE HALF OF THE SIGNED RESCALE, AT AN EXACT TIE. `rescale_s`
    // rounds ties toward +infinity, so on the darkening half an exact half
    // rounds toward zero. An implementation that rescaled the MAGNITUDE
    // unsigned and re-applied the sign rounds it the other way and differs by
    // one LSB. The tie is `prod ≡ -128 (mod 256)`, i.e. the low byte of the
    // exact product is 0x80 — stated in two's complement so no signed modulo
    // is involved.
    c_alpha_neg_tie:
      cover (mode_free == BL_ALPHA && prod_a < LW'(0) && prod_a[7:0] == 8'h80);
    c_alpha_pos_tie:
      cover (mode_free == BL_ALPHA && prod_a > LW'(0) && prod_a[7:0] == 8'h80);

    c_replace: cover (mode_free == BL_REPLACE && out == src_free && src_free != dst_free);
  end

endmodule : raster_fragment_blend_fv
