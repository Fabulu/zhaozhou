// zhao_geom_fogfactor.sv — the per-vertex fog factor, spec/qformats.md §8.
//
// ENFORCED-BY: tests/geometry/geom_fogfactor_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THE HALF OF FOG THAT LIVES IN GEOMETRY
// ---------------------------------------------------------------------------
// Owner ruling D-5 (2026-09-03) splits fog in two. This block is the first half:
//
//   "the fog factor is computed once per vertex from the frozen view/fog law"
//
// and `zhao_raster_fog` is the second half, applying the mix to the final source
// colour after toon quantisation and material combination. Between them the
// factor is an ORDINARY INTERPOLANT, carried through GEOM.PARAMBUF and stepped
// by ATTRSTEP exactly like vertex alpha — which is the change D-5 actually made.
// Before D-5 no such carrier existed, because the colour arrived pre-fogged.
//
// ---------------------------------------------------------------------------
// WHAT IS FROZEN, AND THE PART THAT IS NOT PER-VERTEX
// ---------------------------------------------------------------------------
// §8's law, unchanged by D-5 ("that is f_raw / f / f8, unchanged, still
// frozen"):
//
//     k     = field_rcp(fog_far - fog_near)   // ONCE PER FRAME
//     f_raw = fx_mul(fx_sub(fog_far, d), k)   // ONE rounding (§3)
//     f     = clamp(f_raw, 0, 0x10000)
//
// `k` is DELIBERATELY a config port and not computed here. §8 says the
// denominator is frame-constant, so a per-vertex reciprocal would be one
// `field_rcp` per vertex to recompute a number that cannot have changed —
// paying a divider's area and delay for nothing, in the block whose whole
// budget is per-vertex.
//
// `d` is the view-space FORWARD distance: the guarded `w` the depth pipeline
// already clamps, which `zhao_geom_project` already emits on `out_w_o`. NOT
// radial distance — §8 says radial costs a per-vertex `isqrt_u32` (§7.2) and
// differs only off-axis.
//
// POLARITY, stated because §8's own superseded mix formula reads as if it were
// the other way round and inverted fog is the easy mistake here:
//
//     f == 0x10000  ->  CLEAR     (d <= fog_near)
//     f == 0        ->  FULL FOG  (d >= fog_far)
//
// `zhao_raster_fog` therefore weights toward the fog colour by the COMPLEMENT
// of this value, and asserts the clear rail is the exact identity.
//
// DISABLED IS CLEAR, NOT FOGGED. §8: "fog_far <= fog_near disables fog
// regardless of mode — a deterministic no-op, not an error." A disabled frame
// emits 0x10000 so that every downstream stage is the identity, rather than
// emitting 0 and fogging the whole world.
`default_nettype none

module zhao_geom_fogfactor (
    input var logic clk,
    input var logic rst_n,

    // ---- per-frame fog state -------------------------------------------------
    input var logic               cfg_en_i,     // fog_mode != off
    input var logic signed [31:0] cfg_far_i,    // fx16 world metres
    input var logic signed [31:0] cfg_near_i,   // fx16 world metres
    // k = field_rcp(far - near), computed ONCE PER FRAME by the caller. §8
    // makes the denominator frame-constant; recomputing it per vertex would buy
    // nothing and cost a divider.
    input var logic signed [31:0] cfg_k_i,

    // ---- one projected vertex ------------------------------------------------
    input  var logic        v_valid_i,
    output var logic        v_ready_o,
    // The guarded forward distance, as zhao_geom_project emits it on out_w_o.
    input  var logic [30:0] w_i,
    input  var logic        behind_i,  // clip.w <= 0; the primitive is culled
    input  var logic [15:0] tag_i,

    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] fogf_o,  // Q16.16, 0x10000 = CLEAR
    output var logic        [15:0] tag_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] vertices_o,
    output var logic [31:0] clear_vertices_o,   // f == 0x10000
    output var logic [31:0] opaque_vertices_o   // f == 0, fully fogged
);

  localparam logic signed [31:0] UNITY = 32'sh0001_0000;

  // §8's disabled case, and it is a no-op rather than an error.
  logic disabled_c;
  assign disabled_c = !cfg_en_i || (cfg_far_i <= cfg_near_i);

  // ---- fx_sub(fog_far, d) --------------------------------------------------
  // `w_i` is unsigned 31-bit and fog_far is a signed fx16, so the subtraction
  // runs one bit wider than either and cannot wrap before the clamp below sees
  // it. A wrap here would look exactly like a legitimate small factor.
  logic signed [33:0] dist_c;
  assign dist_c = 34'(cfg_far_i) - 34'({1'b0, w_i});

  // ---- fx_mul(., k): ONE rescale(.,16), round-half-up, saturating ----------
  // Same law as zhao_project_core's rescale16_*: the shift is arithmetic so it
  // floors, which is what makes (x + 2^15) >>> 16 round half UP rather than
  // toward zero.
  logic signed [65:0] prod_c;
  logic signed [65:0] resc_c;
  assign prod_c = dist_c * 66'(cfg_k_i);
  assign resc_c = (prod_c + 66'sd32768) >>> 16;

  // ---- clamp to [0, 0x10000] ----------------------------------------------
  logic signed [31:0] f_c;
  always_comb begin
    if (disabled_c)                 f_c = UNITY;   // deterministic no-op
    // A behind-the-eye vertex is left CLEAR rather than fully fogged: its
    // primitive is culled, and emitting 0 here would put a surprising value
    // into the interpolant for a vertex nobody draws.
    else if (behind_i)              f_c = UNITY;
    else if (resc_c <= 66'sd0)      f_c = 32'sd0;
    else if (resc_c >= 66'sh1_0000) f_c = UNITY;
    else                            f_c = resc_c[31:0];
  end

  // ---- registered output boundary ------------------------------------------
  assign v_ready_o = !r_valid_o || r_ready_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      r_valid_o        <= 1'b0;
      fogf_o           <= UNITY;
      tag_o            <= 16'd0;
      vertices_o       <= 32'd0;
      clear_vertices_o <= 32'd0;
      opaque_vertices_o <= 32'd0;
    end else begin
      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;
      if (v_valid_i && v_ready_o) begin
        r_valid_o  <= 1'b1;
        fogf_o     <= f_c;
        tag_o      <= tag_i;
        vertices_o <= vertices_o + 32'd1;
        if (f_c == UNITY)     clear_vertices_o  <= clear_vertices_o  + 32'd1;
        if (f_c == 32'sd0)    opaque_vertices_o <= opaque_vertices_o + 32'd1;
      end
    end
  end

`ifndef SYNTHESIS
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end
  always_ff @(posedge clk) begin
    if (armed_q && v_valid_i && v_ready_o) begin
      // The factor NEVER leaves its range. Downstream weights by 255 - f8 and a
      // factor outside [0, 0x10000] would produce an amount outside [0, 255],
      // i.e. a fog mix that brightens.
      a_fogf_in_range_lo : assert (f_c >= 32'sd0);
      a_fogf_in_range_hi : assert (f_c <= UNITY);
      // Disabled fog is the IDENTITY, not full fog. §8 calls this a
      // deterministic no-op and getting it backwards would fog every frame that
      // meant to have none.
      if (disabled_c) a_fogf_disabled_is_clear : assert (f_c == UNITY);
    end
  end
`endif

endmodule : zhao_geom_fogfactor

`default_nettype wire
