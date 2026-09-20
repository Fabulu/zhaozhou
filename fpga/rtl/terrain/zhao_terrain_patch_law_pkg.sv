// zhao_terrain_patch_law_pkg.sv — the ratified TERRAIN.PATCH arithmetic, in
// ONE place, as a PACKAGE every block that owes it imports.
//
// WHY THIS FILE EXISTS
// --------------------
// Owner directive 2026-09-20 §13.4, on the Earth production path:
//
//     "The footprint law remains one implementation: extract/reuse the
//      terrain-owned closed-rectangle helper and have the production walker use
//      its accepted-list semantics. Do not copy an approximate rectangle test
//      into FIELD."
//
// and the repair plan's C4, which requires `zhao_terrain_patch_v2` to retain the
// ratified laws BY FACTORING, NOT REIMPLEMENTATION — `tools/budget/uncashed_cheques.py`
// check 3 exists precisely to catch a second implementation of ratified
// arithmetic.
//
// IT WAS ALREADY DUPLICATED, TWICE, BEFORE ANY v2 EXISTED. Measured 2026-09-20:
//
//   * `fx_add_sat` existed VERBATIM, character for character, in BOTH
//     `zhao_terrain_patch.sv` and `zhao_terrain_patch_acc.sv`.
//   * the §9.1 closed-interval footprint test existed in BOTH
//     `zhao_terrain_patch.sv` (as `cur_covers`) and `zhao_terrain_field_walk.sv`
//     (inside `mask_c[l]`).
//
//     BUT `zhao_terrain_field_walk` IS NOT A COPY, AND MUST NOT BE FACTORED
//     INTO THIS PACKAGE. It states the SAME LAW IN HOISTED FORM: z is tested
//     ONCE PER GROUP there, not once per lane. Calling `covers()` from it would
//     evaluate z FOUR TIMES where the hoisted form evaluates it once — a
//     possible AREA REGRESSION, in a block that is not even composed yet.
//     Measured and recommended by packet TERRLAW, 2026-09-20 (owner ruling
//     R205), which factored the four real copies and deliberately left this one
//     alone.
//
//     The distinction is worth the paragraph because the next reader will grep
//     for the law, find it here, and be tempted to "finish the job". **The same
//     law in a different form is not duplication. Two statements that must move
//     together is duplication.** These two must move together — so if §9.1 ever
//     changes, THIS COMMENT is the pointer that says where the second statement
//     lives.
//
// So the duplication this header removes is not a hazard a future block might
// have introduced. It was already in the tree, in the files this packet
// promoted, and nothing could see it — `uncashed_cheques.py` check 3 compares
// the `reference_model` STRINGS declared in `design/blocks.yml`, so two
// byte-identical function bodies in one directory are invisible to it. That is
// the boundary of the instrument, and it is worth writing down beside the thing
// it could not detect.
//
// HOW TO USE IT: add this file to the module's SOURCES list and write
// `import zhao_terrain_patch_law_pkg::*;` above the module.
//
// A PACKAGE AND NOT AN `.svh`, DELIBERATELY, AND THE REASON IS MEASURED.
// The linter does NOT resolve a relative include from the including file's
// own directory: the first version of this file was an .svh and linting a
// consumer gave `Cannot find include file: 'zhao_terrain_patch_law.svh'`. An
// include would therefore need a `+incdir+` on every verilate() call, every
// Quartus fit target and every bench that ever compiles a consumer -- and the
// one that got missed would fail later and somewhere else. A package travels
// in the SOURCES list that already exists, and this tree already does it that
// way (`zhao_field_ops_pkg.sv`).
//
// CHANGING ANYTHING HERE IS A CONTRACT CHANGE, not a refactor. Every function
// below is cited law:
//
//   spec/terrain_rules.md §3.4   the composition chain and its TWO clamps
//   spec/terrain_rules.md §9.1   closed intervals over the shared 33x33 lattice;
//                                a footprint-border vertex is INSIDE
//   spec/qformats.md §2/§9       height16 -> fx16 is an EXACT `raw << 8`
//   spec/qformats.md §3          `fx_add` saturates; one add at a time
//   design/contracts/TERRAIN.PATCH.md
//   reference/src/zrender/terrain.cpp `compose_lattice`   the ratified chain
//
// VERILOG SIGNEDNESS, restated because it cost a real bug once: every
// comparison here is between two signed operands. A Verilog comparison goes
// unsigned if EITHER operand is, and that trap made 29 tiles vanish in
// GEOM.BINNER (design/contracts/GEOM.CLIP.md).

package zhao_terrain_patch_law_pkg;

// §3 saturating fx16 add: ONE add at 33 bits, then narrow with saturation.
//
// Never a wide accumulate followed by one narrow clamp. `fx_add` saturates and
// is therefore order-dependent, so a wide sum would silently disagree with the
// reference wherever a PARTIAL sum leaves the word even though the total does
// not. Lifted verbatim from zhao_terrain_patch.sv:192-201, which is also
// character-for-character what zhao_terrain_patch_acc.sv:175-184 held.
function automatic logic signed [31:0] zhao_tp_fx_add_sat(input logic signed [31:0] a,
                                                          input logic signed [31:0] b);
  logic signed [32:0] s;
  begin
    s = $signed({a[31], a}) + $signed({b[31], b});
    if (s > 33'sd2147483647) zhao_tp_fx_add_sat = 32'sh7FFF_FFFF;
    else if (s < -33'sd2147483648) zhao_tp_fx_add_sat = 32'sh8000_0000;
    else zhao_tp_fx_add_sat = s[31:0];
  end
endfunction

// Did that add saturate? Separate from the value, because a saturation is a
// REPORTABLE EVENT and not an error: the composition is still exact, and the
// count is how a caller learns the word was left.
function automatic logic zhao_tp_fx_add_fired(input logic signed [31:0] a,
                                              input logic signed [31:0] b);
  logic signed [32:0] s;
  begin
    s = $signed({a[31], a}) + $signed({b[31], b});
    zhao_tp_fx_add_fired = (s > 33'sd2147483647) || (s < -33'sd2147483648);
  end
endfunction

// height16 -> fx16. EXACT `raw << 8` (qformats §9): sign-extend the 16-bit word
// to 24 bits of fx16 raw. There is NO ROUNDING here and none is possible.
function automatic logic signed [31:0] zhao_tp_h16_to_fx(input logic signed [15:0] h);
  begin
    zhao_tp_h16_to_fx = {{8{h[15]}}, h, 8'b0};
  end
endfunction

// §9.1's CLOSED-interval footprint test, and THE one implementation of it.
//
// A vertex exactly on a footprint edge is INSIDE. This is `compose_lattice`'s
// test verbatim. A lane whose footprint misses the vertex is CONSUMED AND
// DISCARDED, never added as zero — identical in value AND identical in
// saturation records, which is why "add zero instead" is not an equivalent
// simplification.
function automatic logic zhao_tp_covers(input logic signed [31:0] wx,
                                        input logic signed [31:0] wz,
                                        input logic signed [31:0] x0,
                                        input logic signed [31:0] z0,
                                        input logic signed [31:0] x1,
                                        input logic signed [31:0] z1);
  begin
    zhao_tp_covers = !((wx < x0) || (wx > x1) || (wz < z0) || (wz > z1));
  end
endfunction

// §3.4 line 1: compose_top = max(fx(base) + fx(scar), fx(bottom)).
// A LEGACY single-surface page has no underside, so it is not clamped.
function automatic logic signed [31:0] zhao_tp_compose_top(input logic signed [31:0] base_fx,
                                                           input logic signed [31:0] scar_fx,
                                                           input logic signed [31:0] bot_fx,
                                                           input logic dual);
  logic signed [31:0] s;
  begin
    s = zhao_tp_fx_add_sat(base_fx, scar_fx);
    zhao_tp_compose_top = (dual && (s < bot_fx)) ? bot_fx : s;
  end
endfunction

// §3.4 line 2's clamp: the ONE clamp after the whole command-order fx_add
// chain. A transient wave can never punch below the underside, so it can never
// fake a breach.
function automatic logic signed [31:0] zhao_tp_bottom_clamp(input logic signed [31:0] v,
                                                            input logic signed [31:0] bot_fx,
                                                            input logic dual);
  begin
    zhao_tp_bottom_clamp = (dual && (v < bot_fx)) ? bot_fx : v;
  end
endfunction

// The 4x4 subpatch dirty mask of one lattice vertex, bit row*4 + col
// (charter §11.1). A vertex on a subpatch border marks BOTH neighbours and a
// corner vertex marks FOUR, because those vertices are physically shared — the
// same closed-interval reasoning §9.1 uses for binning.
//
// REJECTED ALTERNATIVE, kept because it is the cheaper and wrong one somebody
// will propose again: marking from the field footprint rectangles alone. A
// crater's bounding rectangle is "dirty" in its corners where the field
// evaluates to exactly zero, so it marks subpatches whose ground did not move
// and defeats terrain_rules §4.4's "dirty patches only" entirely.
function automatic logic [15:0] zhao_tp_sp_mask(input logic [5:0] vi, input logic [5:0] vj);
  logic [5:0] col_lo, col_hi, row_lo, row_hi;
  logic [15:0] m;
  begin
    col_lo = (vi == 6'd0) ? 6'd0 : ((vi - 6'd1) >> 3);
    col_hi = ((vi >> 3) > 6'd3) ? 6'd3 : (vi >> 3);
    row_lo = (vj == 6'd0) ? 6'd0 : ((vj - 6'd1) >> 3);
    row_hi = ((vj >> 3) > 6'd3) ? 6'd3 : (vj >> 3);
    m = 16'd0;
    for (int r = 0; r < 4; r++) begin
      for (int c = 0; c < 4; c++) begin
        if (r >= int'(row_lo) && r <= int'(row_hi) && c >= int'(col_lo) && c <= int'(col_hi))
          m[r*4+c] = 1'b1;
      end
    end
    zhao_tp_sp_mask = m;
  end
endfunction

endpackage : zhao_terrain_patch_law_pkg
