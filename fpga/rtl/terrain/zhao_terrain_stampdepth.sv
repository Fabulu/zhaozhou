// zhao_terrain_stampdepth.sv — THE LAYER-F READER: spec/terrain_rules.md §9.3's
// two stamp-to-bake laws, in RTL, for the first time.
//
// OWNER RULING R194, 2026-09-20, BY LOOKING: *"Shipped is fine. Slightly
// different but not off."* The nearest-texel rim is ACCEPTED, the terrain page
// format is FROZEN AT 64x64, and `zref::terrain::sheet_texel_for_vertex` stands
// as written — it does NOT become the identity. That ruling is what makes this
// file buildable: §9.3(b)'s address generator was the contested thing, and it
// is now decided. Every later terrain block inherits the 64x64 format.
//
// Six terrain lanes closed none of this (owner ruling R116: *"not six failures;
// it is one blocker seen six times"*). The blocker was a question nobody had
// put to the owner.
//
// Law, in citation order:
//   spec/terrain_rules.md §9.3(a) — strength -> depth is an ART TABLE.
//   spec/terrain_rules.md §9.3(b) — 64x64 -> 33x33 is NEAREST-TEXEL, and the
//       seam error is DECLARED (§9.3(c) measures it: a rim wrong by up to one
//       vertex EVERYWHERE, 4 of 99 shared border vertices at the worst
//       placement, worst tear 3.25 m — accepted by R194).
//   spec/qformats.md §3 — `rescale` is round-half-up; fx16 -> height16 is
//       `rescale(x, 8)`.
//   reference/include/zref/zref_terrain_page.hpp — `kStampDepthTable`,
//       `stamp_depth`, `sheet_texel_for_vertex`, `stamp_depth_at_vertex`:
//       THE EXECUTED LAW this file is differenced against.
//   tests/terrain/terrain_stampdepth_directed.cpp — the RTL differential:
//       every one of the 33x33 lattice vertices against the address law, and
//       all 256 strengths against the depth law, exhaustively.
//   tests/terrain/stamp_to_bake_laws_directed.cpp — the same laws in C++.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS, AND WHAT IT DELIBERATELY IS NOT
// ---------------------------------------------------------------------------
// It is PURE COMBINATIONAL LAW: an address generator and a table lookup. It
// holds no state, has no handshake and no counters, because it arbitrates
// nothing and sequences nothing. `zhao_terrain_bake_v2` instantiates it once
// and drives it from the DIG phase's vertex cursor; the block's own header
// records the mode.
//
// It is NOT the sheet's second requester and it is NOT an arbiter. It emits
// `texel_o` — the address in `zhao_surface_sheet`'s OWN `req_texel_i` encoding
// (j*64 + i, scan order, the order `zhao_surface_sheet.sv` states) — and
// consumes the `strength` byte that comes back. Whoever SHARES that port
// between SURFACE.STAMP and this reader is a scheduler, and
// `zhao_console_core.sv` entry I32 already names that as a separate small block
// with a contract (the shape `zhao_terrain_psmux` and `zhao_mem_share_n`
// already have twice). A mux written inline in a composer is an arbiter nobody
// can point at.
//
// NO MULTIPLIER. The §9.3(a) interpolation is `(b - a) * fr` with `fr` FOUR
// BITS, written as an explicit four-term shift-add below rather than as `*`,
// so no synthesis tool can decide to spend a DSP on it. `zhao_terrain_bake_v2`
// exists precisely to hold ONE multiplier; a second site inside its closure
// would undo that, silently, and only the fit would say so.
//
// ---------------------------------------------------------------------------
// THE ART TABLE IS A KNOB, AND SO IS THE FORMAT (CLAUDE.md rule 6)
// ---------------------------------------------------------------------------
// Every one of the sixteen depths below is a NAMED, EDITABLE constant, not a
// fitted curve, for the reason spec/terrain_rules.md §9.3(a) gives: how deep a
// scar READS is a look decision, and a measurement can remove a bias but
// cannot choose a value. The shipped set is PROVISIONAL — it has not been
// looked at in scene at final resolution, which is the only thing that can
// settle it. Entries are SIGNED because a negative entry RAISES ground, which
// is what a Volcano needs (§9.2).
//
// They are localparams and not module parameters ON PURPOSE: the table is the
// game's law, one table for the whole machine, and a per-instance override
// would let two bakes in one console disagree about how deep a scar is. Change
// the values HERE, and `tests/terrain/terrain_stampdepth_directed.cpp` fails
// until `zref::terrain::kStampDepthTable` is changed to match — which is the
// behaviour wanted: the oracle and the silicon move together or not at all.
//
// `SheetEdge` IS a parameter, because it is a FORMAT fact rather than an art
// one, and R194 froze it at 64. The elaboration guard below refuses anything
// else, citing the ruling, so a later packet that "just tries 65" is stopped at
// elaboration rather than three hours into a fit. R194's price for 65x65:
// +38.3% page size, three tripped elaboration guards, and a page that nearly
// doubles (8,450 B -> 16,384) because `zhao_terrain_jdoorbell.sv` requires a
// power of two.
// ---------------------------------------------------------------------------

module zhao_terrain_stampdepth #(
    // spec/terrain_rules.md §2: layer F is 64x64 {tag u8, strength u8}.
    // FROZEN AT 64 BY OWNER RULING R194 — see the guard below.
    parameter int unsigned SheetEdge = 64,
    // The lattice: Island Patch v1 is 33x33 vertices (terrain_rules §2).
    parameter int unsigned Lat = 33
) (
    // ---- the lattice vertex under bake, 0..Lat-1 ---------------------------
    input logic [5:0] vi_i,  // column
    input logic [5:0] vj_i,  // row

    // ---- §9.3(b): the layer-F texel that vertex samples --------------------
    // `zhao_surface_sheet.sv`'s own encoding: j*64 + i, scan order.
    output logic [11:0] texel_o,

    // ---- the strength byte layer F holds at that texel ---------------------
    input logic [7:0] strength_i,

    // ---- §9.3(a): the depth that strength digs -----------------------------
    output logic signed [31:0] depth_fx16_o,  // fx16 metres, the ART TABLE
    output logic signed [31:0] depth_h16_o,   // the same depth in height16
    output logic               covered_o      // this vertex carries a scar
);

  // ---------------------------------------------------------------------------
  // §9.3(b) THE ADDRESS LAW — nearest texel, tie broken DOWNWARD
  // ---------------------------------------------------------------------------
  // Layer F is an AREA grid: texel i's centre sits at (2i+1)/128 of the patch.
  // The lattice is a VERTEX grid: vertex v sits at v/32. Solving
  // (2i+1)/4 = v gives i = 2v - 1/2 — never an integer, ALWAYS AN EXACT TIE.
  // The tie breaks downward, the same direction `zref::render::sample_sheet`
  // already breaks it, so the law is
  //
  //     sheet_texel_for_vertex(v) = min(2v, SheetEdge - 1)
  //
  // and vertex 32 is the ONLY clamped one. THE DECLARED ERROR (§9.3(b),
  // measured rather than estimated): +1/4 cell at vertices 0..31, -1/4 cell at
  // vertex 32, 1/2 cell across a patch seam where the two samples come from
  // DIFFERENT PAGES. R194 accepted it by looking.
  //
  // `2*v` overflows six bits only for v >= 32, and `v >= 32` is exactly the
  // clamp condition, so the compare is on the INPUT and no wide intermediate is
  // needed. It matches the oracle for every v, legal or not: the oracle clamps
  // at 2v > 63, i.e. v >= 32.
  // Written as literals rather than as `SheetEdge[5:0] - 1` and `Lat - 1`
  // because a part-select of an `int unsigned` parameter is one of the forms
  // Quartus 17.0 and Verilator have been caught disagreeing about, and the
  // elaboration guard at the foot of this file already proves SheetEdge == 64
  // and Lat == 33. A derived expression nobody can elaborate is worse than a
  // literal a guard checks.
  localparam logic [5:0] TexelMax = 6'd63;  // SheetEdge - 1
  localparam logic [5:0] LatMax   = 6'd32;  // Lat - 1, the one clamped vertex

  logic [5:0] ti, tj;
  assign ti = (vi_i >= LatMax) ? TexelMax : {vi_i[4:0], 1'b0};
  assign tj = (vj_i >= LatMax) ? TexelMax : {vj_i[4:0], 1'b0};

  // tj*64 + ti, exactly, because SheetEdge is 64 and the guard below says so.
  assign texel_o = {tj, ti};

  // ---------------------------------------------------------------------------
  // §9.3(a) THE ART TABLE — sixteen editable fx16 metres, indexed strength>>4
  // ---------------------------------------------------------------------------
  // Entry n is the depth at strength n*16, in fx16 (65,536 = one metre).
  // These sixteen lines ARE the knob. `zref::terrain::kStampDepthTable` holds
  // the identical set and the directed test refuses any disagreement.
  localparam logic signed [31:0] kDepth00 = -32'sd0;       //   0    0.00 m  no scar
  localparam logic signed [31:0] kDepth01 = -32'sd6553;    //  16   -0.10 m  a scuff
  localparam logic signed [31:0] kDepth02 = -32'sd19661;   //  32   -0.30 m
  localparam logic signed [31:0] kDepth03 = -32'sd39322;   //  48   -0.60 m
  localparam logic signed [31:0] kDepth04 = -32'sd65536;   //  64   -1.00 m  a footfall crater
  localparam logic signed [31:0] kDepth05 = -32'sd98304;   //  80   -1.50 m
  localparam logic signed [31:0] kDepth06 = -32'sd131072;  //  96   -2.00 m
  localparam logic signed [31:0] kDepth07 = -32'sd170394;  // 112   -2.60 m
  localparam logic signed [31:0] kDepth08 = -32'sd212992;  // 128   -3.25 m  a spell impact
  localparam logic signed [31:0] kDepth09 = -32'sd262144;  // 144   -4.00 m
  localparam logic signed [31:0] kDepth10 = -32'sd314573;  // 160   -4.80 m
  localparam logic signed [31:0] kDepth11 = -32'sd370606;  // 176   -5.65 m
  localparam logic signed [31:0] kDepth12 = -32'sd432013;  // 192   -6.59 m
  localparam logic signed [31:0] kDepth13 = -32'sd498073;  // 208   -7.60 m
  localparam logic signed [31:0] kDepth14 = -32'sd569344;  // 224   -8.69 m
  localparam logic signed [31:0] kDepth15 = -32'sd655360;  // 240  -10.00 m  the deepest one stamp digs

  // A CASE, not an unpacked localparam array. Quartus 17.0 is the tool that
  // has to swallow this (CLAUDE.md: "Verilator lint-clean is not Quartus-
  // synthesizable", proven twice on 2026-09-08 by forms that linted with 0
  // diagnostics and died in quartus_map), and a 16-way case on a 4-bit index is
  // the form every synthesiser in this tree already compiles.
  function automatic logic signed [31:0] depth_entry(input logic [3:0] n);
    begin
      case (n)
        4'd0:  depth_entry = kDepth00;
        4'd1:  depth_entry = kDepth01;
        4'd2:  depth_entry = kDepth02;
        4'd3:  depth_entry = kDepth03;
        4'd4:  depth_entry = kDepth04;
        4'd5:  depth_entry = kDepth05;
        4'd6:  depth_entry = kDepth06;
        4'd7:  depth_entry = kDepth07;
        4'd8:  depth_entry = kDepth08;
        4'd9:  depth_entry = kDepth09;
        4'd10: depth_entry = kDepth10;
        4'd11: depth_entry = kDepth11;
        4'd12: depth_entry = kDepth12;
        4'd13: depth_entry = kDepth13;
        4'd14: depth_entry = kDepth14;
        4'd15: depth_entry = kDepth15;
        default: depth_entry = kDepth00;
      endcase
    end
  endfunction

  // ---- the interpolation, operator for operator with the oracle -------------
  // zref::terrain::stamp_depth:
  //     hi = s >> 4; fr = s & 15;  a = T[hi]
  //     if (fr == 0 || hi == 15) return a;          // the LAST SEGMENT IS HELD
  //     d = (T[hi+1] - a) * fr
  //     r = (d >= 0) ? ((d + 8) >> 4) : -(((-d) + 8) >> 4)
  //     return a + r
  //
  // The `hi == 15` arm is why strength 255 extrapolates no further than entry
  // 15: the deepest value is one somebody CHOSE rather than one the arithmetic
  // ran off the end of.
  //
  // Note the rounding is SYMMETRIC about zero (round-half-away-from-zero on the
  // magnitude), not the round-half-up of `rescale`. That is the oracle's own
  // shape and it is reproduced exactly rather than "corrected": a depth table
  // whose entries are mostly negative would otherwise round in the direction
  // that makes every dig shallower by an LSB, and the RTL and the reference
  // would disagree on 50% of the ties.
  logic [3:0] s_hi, s_fr;
  assign s_hi = strength_i[7:4];
  assign s_fr = strength_i[3:0];

  logic signed [31:0] tab_a;
  logic signed [31:0] tab_b;
  assign tab_a = depth_entry(s_hi);
  // `s_hi + 1` is read only when `s_hi != 15`, and the select below proves it;
  // the wrap to entry 0 at s_hi == 15 is therefore never consumed.
  assign tab_b = depth_entry(s_hi + 4'd1);

  // diff = T[hi+1] - T[hi], signed 33 (two signed 32s).
  logic signed [32:0] tab_diff;
  assign tab_diff = $signed({tab_b[31], tab_b}) - $signed({tab_a[31], tab_a});

  // d = diff * fr, AS A FOUR-TERM SHIFT-ADD. `fr` is four bits, so this is
  // three adders of a 33-bit value and no multiplier — see the header. 33 + 4
  // = 37 bits of product; 38 carries the sign headroom of the sum.
  logic signed [37:0] d_prod;
  assign d_prod = (s_fr[0] ? {{5{tab_diff[32]}}, tab_diff} : 38'sd0)
                + (s_fr[1] ? {{4{tab_diff[32]}}, tab_diff, 1'b0} : 38'sd0)
                + (s_fr[2] ? {{3{tab_diff[32]}}, tab_diff, 2'b0} : 38'sd0)
                + (s_fr[3] ? {{2{tab_diff[32]}}, tab_diff, 3'b0} : 38'sd0);

  // r = the oracle's symmetric round of d/16.
  logic signed [37:0] d_abs;
  logic signed [37:0] r_abs;
  // The discarded high bits of d_round and the discarded low bits ofh_round`n  // ARE THE OPERATORS: the >>> 4 and the >>> 8 are the two roundings the law
  // specifies, and depth_sum's top bit is the headroom the truncation below
  // is proved not to need. Same treatment, same reason, as delta_g in
  // zhao_terrain_bake_v2.sv.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [37:0] d_round;
  assign d_abs   = d_prod[37] ? -d_prod : d_prod;
  assign r_abs   = (d_abs + 38'sd8) >>> 4;
  assign d_round = d_prod[37] ? -r_abs : r_abs;

  // The interpolated depth. `a + r` cannot leave fx16's range for any table the
  // art law would write (the whole shipped column spans 10 m of a 32,767 m
  // format), and it is NOT saturated here on purpose: the height16 rail below
  // and `zhao_terrain_bake_v2`'s own scar rails are where a runaway entry is
  // caught, once, where the counter that sees it lives.
  logic signed [32:0] depth_sum;
  // `$signed(...)` on the part-select is load-bearing: a part-select is
  // UNSIGNED in SystemVerilog whatever the parent's signedness, and dropping it
  // would turn every negative depth delta into a huge positive one. The
  // truncation from 38 to 33 is exact because |d_round| <= |tab_diff| (the
  // factor is fr/16 <= 15/16) and tab_diff is 33 bits.
  assign depth_sum = $signed({tab_a[31], tab_a}) + $signed(d_round[32:0]);

  /* verilator lint_on UNUSEDSIGNAL */

  logic held_segment;
  assign held_segment = (s_fr == 4'd0) || (s_hi == 4'd15);

  assign depth_fx16_o = held_segment ? tab_a : depth_sum[31:0];

  // ---------------------------------------------------------------------------
  // fx16 -> height16, spec/qformats.md: rescale(x, 8), ROUND-HALF-UP
  // ---------------------------------------------------------------------------
  // `(x + 128) >>> 8` with an ARITHMETIC shift is round-half-up: ties go toward
  // +inf. This is the identical operator `zhao_terrain_bake_v2`'s `delta_g`
  // applies in its second stage, deliberately — the sheet mode's delta and the
  // disc mode's delta must land in the same domain by the same rounding, or the
  // two laws would disagree about what a half-LSB is.
  //
  // The result is 25 bits and is SIGN-EXTENDED to 32, exactly as `delta_g`
  // returns it, so it can be dropped into bake's `delta16` without a second
  // conversion. No saturation happens here and none can: 25 bits into 32 is
  // exact. The s16 rail is bake's, at `rail_hi`/`rail_lo`, where
  // `scar_saturations_o` counts it.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [32:0] h_round;
  assign h_round = $signed({depth_fx16_o[31], depth_fx16_o}) + 33'sd128;
  assign depth_h16_o = {{7{h_round[32]}}, h_round[32:8]};
  /* verilator lint_on UNUSEDSIGNAL */

  // ---------------------------------------------------------------------------
  // coverage
  // ---------------------------------------------------------------------------
  // A vertex is TOUCHED by the sheet when layer F holds any strength at all.
  // strength 0 maps to kDepth00 = 0 regardless, so this changes no height; it
  // is what `sc_touched_o` reports and what decides whether the §3.3 no_bake
  // clamp and the height16 rails are armed at this vertex — the same role
  // `covers` plays on the parametric-disc path.
  assign covered_o = (strength_i != 8'd0);

  // ---------------------------------------------------------------------------
  // ELABORATION GUARDS
  // ---------------------------------------------------------------------------
  // Inside `initial begin ... end`: Quartus 17.0 rejects a bare module-scope
  // `if (...) $fatal(...)` with "syntax error near text: `if`; expecting
  // `endmodule`" while `verilator --lint-only` reports 0 diagnostics on it
  // (CLAUDE.md, 2026-09-08). And `--lint-only` does not RUN initial blocks, so
  // a clean lint is not evidence about anything below this line; the guards are
  // fired by `tests/terrain/terrain_stampdepth_directed.cpp` building a model
  // with a bad parameter, not by argument.
  // synthesis translate_off
  initial begin
    // OWNER RULING R194, 2026-09-20: the terrain page format is FROZEN at
    // 64x64. This is not a preference: at 65x65 the sheet is 8,450 B, which
    // trips `zhao_terrain_writeback.sv`'s whole-burst $fatal, trips
    // `zhao_terrain_pageloader.sv`'s whole-beat CRC $fatal, and trips
    // `zhao_terrain_jdoorbell.sv`'s POWER-OF-TWO journal shift — whose next
    // legal size is 16,384, so the page nearly doubles. A packet that changes
    // this number is reopening a ratified freeze and owes the owner a decision.
    if (SheetEdge != 64)
      $fatal(1, "zhao_terrain_stampdepth: SheetEdge must be 64 -- the terrain page format is FROZEN by owner ruling R194 (spec/terrain_rules.md 9.3(b))");
    // `texel_o` is {tj, ti}, which IS tj*SheetEdge + ti only while SheetEdge is
    // a power of two equal to 64. The guard above covers it; this one states
    // the dependency so the concatenation is never read as a coincidence.
    if (12 != 2 * 6)
      $fatal(1, "zhao_terrain_stampdepth: texel_o is {tj,ti} and needs two 6-bit indices");
    // The lattice and the sheet must describe the same patch: 33 vertices span
    // 32 cells, and the area grid's 64 texels are two per cell.
    if (SheetEdge != 2 * (Lat - 1))
      $fatal(1, "zhao_terrain_stampdepth: SheetEdge must be 2*(Lat-1) -- the sheet is two texels per lattice cell (spec/terrain_rules.md 9.3(b))");
  end
  // synthesis translate_on

endmodule
