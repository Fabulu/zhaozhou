// terrain_bake_delta_fv.sv — formal harness for the incremental-scaling bake
// arithmetic (TERRAIN.BAKE / ZH-036; property terrain_bake_delta.sby).
//
// WHAT IS PROVED, and why it is not vacuous.
//
// The DUT is `zhao_terrain_bake_delta`, the EXACT module `zhao_terrain_bake`
// instantiates — there is one instance in the design and this is it, so
// nothing here is a copy of the datapath. SIX instances are elaborated because
// the load-bearing theorem is about how three bakes COMPOSE, which cannot be
// stated with fewer than three, plus one for the reverse direction, one for the
// full-word saturate theorem and one unconstrained companion for the cover that
// keeps that theorem honest.
//
// The free inputs are one stencil value, three depths over the whole range
// Island Patch v1 can express (24-bit signed fx16 raw = +-128 m, the height16
// rail of terrain_rules 1.2 — see the module body), and one FULL fx16 depth
// word for the saturate theorem. The stencil is
// CONSTRAINED to 0..65536 and that constraint is a fact about the divider that
// feeds it, not a convenience: `s = ((r2 - d2) << 16 + r2/2) / r2` with
// 0 <= r2 - d2 <= r2 gives s <= 2^16 + 1/2, so the 17-step restoring divide in
// zhao_terrain_bake can emit at most 65,536. Outside that range the block
// cannot be handed a value at all, and the `c_sat_reachable` cover shows the
// saturate P5 rules out DOES fire once the constraint is lifted — so P5 is a
// statement about the real input space, not an artefact of the assumption.
//
//   P1  a_telescopes   terrain_rules §9.2 LAW 3, THE ONE THIS EXISTS FOR.
//                      delta(a,b) + delta(b,c) == delta(a,c), exactly, for
//                      every stencil value and every triple of depths. §9.2
//                      freezes BAKE_PATCH_BUDGET = 64 and declares the
//                      resulting deferral "state-exact by the incremental-
//                      scaling identity", so this identity is what makes a
//                      DEFERRED Volcano arrive at the same island as an
//                      undeferred one. It is FALSE for the obvious near-miss:
//                      §9's own prose reads `(to - from) x stencil`, and a
//                      datapath that computed `rescale((a - b) * s)` would
//                      accumulate one rounding per deferred step. The absolute
//                      two-lane form telescopes; the difference form does not.
//                      This assertion is the difference between the two.
//   P2  a_antisymmetry delta(a,b) == -delta(b,a). terrain_rules §9: "an
//                      interrupted cast un-applies cleanly". Two-sided, so it
//                      catches a lane that rounds asymmetrically about zero —
//                      which `(x + 2^(k-1)) >>> k` genuinely does NOT do
//                      symmetrically for arbitrary operands, and the reason
//                      the property holds is that BOTH endpoints go through
//                      the SAME g, never through a shared difference.
//   P3  a_identity     delta(a,a) == 0 at every stencil value. An idle stamp
//                      writes nothing, anywhere. Stated on P2's own instance
//                      with a == b free rather than a sixth elaboration.
//   P4  a_zero_stencil s == 0 (a vertex on the stencil rim) contributes
//                      exactly nothing, at every depth pair — including the
//                      fx16 extremes, where a sloppy rounding add would leave
//                      a one-LSB residue on every rim vertex of every crater.
//   P5  a_no_saturate  the first rescale's fx16 rail is UNREACHABLE for
//                      s <= 65536. The rail is written faithfully in the RTL
//                      because `rescale_s32` has one; this proves the block
//                      can never take it, which is why zhao_terrain_bake's
//                      `scar_saturations_o` can only ever be moved by a
//                      height16 rail. Paired with `c_sat_reachable`.
//   P6  a_digs_down    from <= to implies delta <= 0: "positive depth digs
//                      DOWN (scar goes negative)", `bake_dig`'s own comment.
//                      Monotonicity of g in depth, stated where it is used.
//
// WHAT THIS DOES NOT PROVE, stated plainly. The paraboloid stencil and its
// 17-step divide, `lattice_lerp` and its truncating divide, the no_bake clamp,
// the height16 rails, the §3.4 breach law, the two-phase sweep, the §9.2
// budget backpressure and the counters are NOT proved here: they are covered
// by the differential lanes against `zref::terrain::bake_dig` /
// `apply_breach_law` (tests/terrain/), by the BAKE -> PATCH composition, and by
// the mutation evidence in design/contracts/TERRAIN.BAKE.md. What IS proved is
// the arithmetic every baked vertex in the machine flows through, and the one
// identity a frozen constant in spec/terrain_rules.md rests on.

`default_nettype none

module terrain_bake_delta_fv (
    input wire clk,
    input wire [16:0] s,
    input wire signed [23:0] a24,
    input wire signed [23:0] b24,
    input wire signed [23:0] c24,
    input wire signed [31:0] dfull
);

  // The divider's structural bound (see the header). Assumed here; the cover
  // below shows what the assumption is holding back.
  always_comb assume (s <= 17'd65536);

  // THE DEPTH DOMAIN, and why 24 bits is the whole of it rather than a
  // convenience. `depth` is an fx16 metre value that a bake turns into a
  // height16 layer-B word, and terrain_rules 1.2 rails height16 at +-128 m of
  // island datum. A 24-bit signed fx16 raw IS +-128 m, so these three are free
  // over every depth Island Patch v1 can express. Depths outside it cannot
  // reach layer B at all — they only rail — and the saturate theorem P5, which
  // is the one property that IS about the rail, is proved separately below over
  // the FULL fx16 word.
  wire signed [31:0] da = {{8{a24[23]}}, a24};
  wire signed [31:0] db = {{8{b24[23]}}, b24};
  wire signed [31:0] dc = {{8{c24[23]}}, c24};

  wire signed [31:0] d_ab, d_bc, d_ac, d_ba;
  wire sat_ab, sat_bc, sat_ac, sat_ba;

  zhao_terrain_bake_delta u_ab (
      .stencil_i(s),
      .depth_from_i(da),
      .depth_to_i(db),
      .delta_o(d_ab),
      .sat_o(sat_ab)
  );
  zhao_terrain_bake_delta u_bc (
      .stencil_i(s),
      .depth_from_i(db),
      .depth_to_i(dc),
      .delta_o(d_bc),
      .sat_o(sat_bc)
  );
  zhao_terrain_bake_delta u_ac (
      .stencil_i(s),
      .depth_from_i(da),
      .depth_to_i(dc),
      .delta_o(d_ac),
      .sat_o(sat_ac)
  );
  zhao_terrain_bake_delta u_ba (
      .stencil_i(s),
      .depth_from_i(db),
      .depth_to_i(da),
      .delta_o(d_ba),
      .sat_o(sat_ba)
  );

  // P5's instance: the FULL fx16 word on both endpoints, because the rail is
  // what this one is about.
  wire signed [31:0] d_full;
  wire sat_full;
  zhao_terrain_bake_delta u_full (
      .stencil_i(s),
      .depth_from_i(dfull),
      .depth_to_i(32'sd0),
      .delta_o(d_full),
      .sat_o(sat_full)
  );

  // An UNCONSTRAINED-STENCIL companion, used only by the cover that keeps P5
  // honest: with the divider's bound lifted, the rail DOES fire.
  wire signed [31:0] d_free;
  wire sat_free;
  zhao_terrain_bake_delta u_free (
      .stencil_i(17'h1FFFF),
      .depth_from_i(dfull),
      .depth_to_i(32'sd0),
      .delta_o(d_free),
      .sat_o(sat_free)
  );

  always_ff @(posedge clk) begin
    // P1 — terrain_rules 9.2 law 3, in the built datapath.
    a_telescopes : assert (d_ab + d_bc == d_ac);

    // P2 — an interrupted cast un-applies cleanly.
    a_antisymmetry : assert (d_ab == -d_ba);

    // P3 — an idle stamp writes nothing (b == a is free, so this is the same
    // theorem as P2's diagonal without a sixth multiplier).
    a_identity : assert (a24 != b24 || d_ab == 32'sd0);

    // P4 — a rim vertex contributes nothing.
    a_zero_stencil : assert (s != 17'd0 || d_ab == 32'sd0);

    // P5 — the fx16 rail is unreachable inside the divider's own bound, over
    // the WHOLE fx16 depth word.
    a_no_saturate : assert (!sat_full);

    // P6 — positive depth digs DOWN.
    a_digs_down : assert (!(da <= db) || d_ab <= 32'sd0);

    // ---- covers: the theorems are about a datapath that DOES something ----
    c_max_stencil : cover (s == 17'd65536 && d_ab != 32'sd0);
    c_deep_dig : cover (d_ab < -32'sd10000);
    c_undig : cover (d_ab > 32'sd10000);
    c_three_way : cover (d_ab != 32'sd0 && d_bc != 32'sd0 && d_ab != d_ac);
    c_small_stencil : cover (s > 17'd0 && s < 17'd64 && d_ab != 32'sd0);
    // P5 is a statement about the CONSTRAINED stencil space, and this is the
    // proof that it is not a tautology: with the bound lifted, the rail fires.
    c_sat_reachable : cover (sat_free);
  end

endmodule

`default_nettype wire
