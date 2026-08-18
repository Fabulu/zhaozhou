// zhao_geom_setup.sv — GEOM.SETUP: screen-space triangle setup — the affine
// decomposition of the §8 edge function, in the exact form RASTER.EDGEWALK
// already evaluates (phase 5, ZH-057).
//
// Law (in citation order):
//   design/contracts/GEOM.SETUP.md — the block contract.
//   design/blocks.yml — `inputs: [clipped_triangles, forge_primitives,
//       expanded_particles]`, `outputs: [setup_triangles]`, `upstream:
//       [GEOM.CLIP, FORGE.PRIM, FORGE.CLIFF, PART.EXPAND]`, `downstream:
//       [GEOM.BINNER]`, `target_throughput: 1 setup triangle per clock`,
//       counter `triangles_submitted`, and the note this file honours
//       literally: **"In-tile stepping s32, edge setup s64 (A3c)."**
//   spec/qformats.md §8 — `E0 = (b.x−a.x)(c.y−a.y) − (b.y−a.y)(c.x−a.x)` in
//       subpixel², s64 setup (Giesen bound 2^43−2 at p = 21), `E' = E0 >> 8`
//       for tile-local stepping, `bias = is_top_left(edge) ? 0 : −1`.
//   reference/src/zrender/rast.cpp — `orient()` and `edge_top_left()`, the
//       frozen functions this block decomposes; `dw0_dx = −(C.y−B.y)·256`
//       etc., the per-pixel steps.
//   fpga/rtl/raster/zhao_raster_edgewalk.sv — the CONSUMER, through
//       GEOM.BINNER. Its `sx0 = −(cy−by)`, `sy0 = (cx−bx)` and its
//       `edge_top_left(bx,by,cx,cy)` are literally this block's `kx0`, `ky0`
//       and `tl0`; that is the joint, and it is why nothing here is new.
//
// WHAT THIS BLOCK IS NOT: no clipping, no winding decision and no zero-area
// reject (GEOM.CLIP has already done all three — a triangle arriving here has
// 2A > 0), no tile enumeration (GEOM.BINNER), no coverage (RASTER.EDGEWALK),
// no attribute/depth/UV gradients — see THE GRADIENTS ARE NOT BUILT below.
//
// ---------------------------------------------------------------------------
// THE LAW, FOUND: THE EDGE FUNCTION IS AFFINE, AND THESE ARE ITS COEFFICIENTS
// ---------------------------------------------------------------------------
// rast.cpp's frozen edge function is
//
//   orient(a, b, px, py) = (b.x − a.x)(py − a.y) − (b.y − a.y)(px − a.x)
//
// Expanding it in the sample position is an IDENTITY, not a model:
//
//   orient(a, b, px, py) = kx·px + ky·py + kc
//       kx = −(b.y − a.y)          ky = +(b.x − a.x)
//       kc =  a.x·b.y − a.y·b.x
//
// (the kc form falls out because `(b.y−a.y)·a.x − (b.x−a.x)·a.y` telescopes to
// the plain cross product of a and b). Those three numbers ARE the "edge
// coefficients" of the ledger's purpose line, and they are exactly what
// RASTER.EDGEWALK needs: it steps `E'` by `−Δy` per pixel of x and `+Δx` per
// pixel of y, i.e. by `kx` and `ky`, and its edge value at any pixel centre is
// `kx·px + ky·py + kc` with `px = 256·p + 128`. Nothing here is a
// re-derivation of the fill rule; it is the same function, written with the
// sample position factored out so a consumer can evaluate it anywhere in O(1).
//
// Edges are numbered as rast.cpp numbers them: edge 0 = (B,C) → w0,
// edge 1 = (C,A) → w1, edge 2 = (A,B) → w2. `tl_i` is `edge_top_left(a_i,b_i)`
// verbatim, evaluated on the winding-normalised triangle GEOM.CLIP emits —
// which is the same triangle RASTER.EDGEWALK evaluates it on after its own
// (now no-op) flip.
//
// ---------------------------------------------------------------------------
// THE THIRD CONSTANT IS FREE — AND THAT IS A THEOREM, NOT A SHORTCUT
// ---------------------------------------------------------------------------
// The barycentric identity `w0(p) + w1(p) + w2(p) = 2A` holds for EVERY p —
// it is the reason rast.cpp divides its interpolants by `area`. Matching
// coefficients on both sides of an identity in p gives three separate facts:
//
//     kx0 + kx1 + kx2 = 0        ky0 + ky1 + ky2 = 0        kc0 + kc1 + kc2 = 2A
//
// GEOM.CLIP already computed 2A (it needed it for the zero-area reject and the
// winding), and it hands it over. So this block computes kc0 and kc1 with four
// 21×21 signed multipliers and takes
//
//     kc2 = 2A − kc0 − kc1
//
// exactly — integers, no rounding, no approximation. That is two multipliers
// saved out of six, a third of the block's arithmetic, and the identity is
// checked on every iteration of the random differential lane (the oracle
// computes all three directly).
//
// ---------------------------------------------------------------------------
// WIDTHS
// ---------------------------------------------------------------------------
// A guard-band S 12.8 coordinate is |v| ≤ 2048·256 = 2^19, so
//   · kx, ky are differences of two of them: |Δ| ≤ 2^20 → 23 bits signed,
//     RASTER.EDGEWALK's own DIFF_W, so the two blocks carry the same numbers
//     in the same width;
//   · a product is ≤ 2^38 → 42 bits signed;
//   · |kc| ≤ 2·2^38 = 2^39, and |2A| ≤ 2^41 (well inside §8's Giesen bound of
//     2^43−2), so |kc2| ≤ 2^41 + 2^40 < 2^42. All three are carried in 48 bits
//     signed — RASTER.EDGEWALK's CROSS_W, again deliberately the same domain.
//
// ---------------------------------------------------------------------------
// THE GRADIENTS ARE NOT BUILT, AND THE REASON IS NOT COST
// ---------------------------------------------------------------------------
// The ledger's purpose line says "edge coefficients, gradients". The edge
// coefficients are here. The attribute gradients are NOT, and this is the
// argument rather than an omission:
//
//   1. THE ORACLE'S ATTRIBUTE MODEL IS NOT A PLANE SETUP. rast.cpp does not
//      set a plane up once and step it over the triangle. It computes ONE
//      `div_rhu_s128` gradient per attribute for the x direction, and then
//      re-evaluates the row start with a FULL barycentric division at every
//      scanline (`d = div_rhu_s128(w0·A.d + w1·B.d + w2·C.d, area)` inside the
//      y loop). Each of those is an independent §4 round-half-up. A block that
//      emitted `(d0, dd/dx, dd/dy)` would therefore NOT be bit-exact with the
//      oracle — its second row would differ by the rounding rast.cpp re-does.
//      The per-row divide belongs to whatever walks rows, not to triangle
//      setup, and inventing a plane form here would quietly fork the depth law.
//   2. THERE IS NO CONSUMER. RASTER.FRAGMENT interpolates nothing: its
//      contract and `zhao_raster_tile_pipe.sv`'s header both say the fragment
//      colour, alpha, depth, tag and texel are FLAT, taken from the packet.
//      TEXTURE.TMU is handed U/V already through the §8 perspective divide.
//      Every gradient this block could emit would be dropped on the floor.
//   3. THE COST IS A 128÷48 ROUND-HALF-UP DIVIDER per attribute — the single
//      most expensive unit anywhere in the geometry mantle — built for nobody.
//
// So: the gradient half of the purpose line is deliberately UNBUILT, recorded
// here and in the contract, and it is a real gap against the ledger rather
// than a solved problem. What lands it is the interpolator increment that also
// gives RASTER.FRAGMENT a varying input; until that exists there is nothing to
// be bit-exact against.
//
// ---------------------------------------------------------------------------
// SHAPE AND TIMING
// ---------------------------------------------------------------------------
// Three stages, all advancing together, one triangle per clock; latency 3
// cycles at full readiness. S1 latches; S2 registers the four products and the
// six differences and three top-left bits; S3 forms the three kc constants.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_geom_setup).

module zhao_geom_setup (
  input  logic clk,
  input  logic rst_n,

  // ---- triangle in: GEOM.CLIP's accepted packet -------------------------
  // Winding-normalised (2A > 0), S 12.8 screen subpixels, plus the §8
  // scissored scan box, all carried through to GEOM.BINNER.
  input  logic               tri_valid_i,
  output logic               tri_ready_o,
  input  logic signed [20:0] tri_ax_i,
  input  logic signed [20:0] tri_ay_i,
  input  logic signed [20:0] tri_bx_i,
  input  logic signed [20:0] tri_by_i,
  input  logic signed [20:0] tri_cx_i,
  input  logic signed [20:0] tri_cy_i,
  input  logic signed [47:0] tri_area2_i,
  input  logic signed [11:0] tri_min_x_i,
  input  logic signed [11:0] tri_max_x_i,
  input  logic signed [11:0] tri_min_y_i,
  input  logic signed [11:0] tri_max_y_i,
  input  logic        [15:0] tri_src_id_i,

  // ---- setup triangle out ------------------------------------------------
  // Per edge i: E_i(px,py) = kx_i·px + ky_i·py + kc_i, EXACT, in subpixel².
  // Edge 0 = (B,C), 1 = (C,A), 2 = (A,B) — rast.cpp's w0/w1/w2 numbering.
  output logic               out_valid_o,
  input  logic               out_ready_i,
  output logic signed [22:0] out_kx0_o,
  output logic signed [22:0] out_ky0_o,
  output logic signed [47:0] out_kc0_o,
  output logic signed [22:0] out_kx1_o,
  output logic signed [22:0] out_ky1_o,
  output logic signed [47:0] out_kc1_o,
  output logic signed [22:0] out_kx2_o,
  output logic signed [22:0] out_ky2_o,
  output logic signed [47:0] out_kc2_o,
  output logic        [2:0]  out_tl_o,       // bit i = edge i is top-left
  output logic signed [47:0] out_area2_o,
  // the triangle and its box, carried through to RASTER.EDGEWALK's job port
  output logic signed [20:0] out_ax_o,
  output logic signed [20:0] out_ay_o,
  output logic signed [20:0] out_bx_o,
  output logic signed [20:0] out_by_o,
  output logic signed [20:0] out_cx_o,
  output logic signed [20:0] out_cy_o,
  output logic signed [11:0] out_min_x_o,
  output logic signed [11:0] out_max_x_o,
  output logic signed [11:0] out_min_y_o,
  output logic signed [11:0] out_max_y_o,
  output logic        [15:0] out_src_id_o,

  // ---- counter (spec/counters.md §4: saturate, never wrap) ---------------
  output logic        [31:0] triangles_submitted_o
);

  localparam int unsigned DIFF_W  = 23;  // RASTER.EDGEWALK's DIFF_W
  localparam int unsigned PROD_W  = 42;  // 21×21 signed
  localparam int unsigned CROSS_W = 48;  // RASTER.EDGEWALK's CROSS_W

  function automatic logic signed [DIFF_W-1:0] sub21(input logic signed [20:0] a,
                                                     input logic signed [20:0] b);
    sub21 = $signed({{(DIFF_W-21){a[20]}}, a}) - $signed({{(DIFF_W-21){b[20]}}, b});
  endfunction

  function automatic logic signed [CROSS_W-1:0] sxprod(input logic signed [PROD_W-1:0] p);
    sxprod = $signed({{(CROSS_W-PROD_W){p[PROD_W-1]}}, p});
  endfunction

  // §8 fill convention: for a positive-area (clockwise, y-down) triangle an
  // edge is top-left iff horizontal going right (top) or going down (left).
  // Byte for byte rast.cpp's edge_top_left(), and zhao_raster_edgewalk's.
  function automatic logic top_left(input logic signed [20:0] pxv,
                                    input logic signed [20:0] pyv,
                                    input logic signed [20:0] qxv,
                                    input logic signed [20:0] qyv);
    top_left = (pyv == qyv) ? (pxv < qxv) : (pyv < qyv);
  endfunction

  // ======================================================== stage enable ====
  logic s1_v, s2_v, s3_v;
  logic pipe_en;
  assign pipe_en     = !(s3_v && !out_ready_i);
  assign tri_ready_o = pipe_en;

  // ============================================================ stage 1 ====
  logic signed [20:0] s1_ax, s1_ay, s1_bx, s1_by, s1_cx, s1_cy;
  logic signed [47:0] s1_area2;
  logic signed [11:0] s1_minx, s1_maxx, s1_miny, s1_maxy;
  logic        [15:0] s1_src;

  // ============================================================ stage 2 ====
  logic signed [DIFF_W-1:0] s2_kx0, s2_ky0, s2_kx1, s2_ky1, s2_kx2, s2_ky2;
  logic        [2:0]        s2_tl;
  logic signed [PROD_W-1:0] s2_p0, s2_p1, s2_p2, s2_p3;
  logic signed [20:0] s2_ax, s2_ay, s2_bx, s2_by, s2_cx, s2_cy;
  logic signed [47:0] s2_area2;
  logic signed [11:0] s2_minx, s2_maxx, s2_miny, s2_maxy;
  logic        [15:0] s2_src;

  // ============================================================ stage 3 ====
  logic signed [DIFF_W-1:0] s3_kx0, s3_ky0, s3_kx1, s3_ky1, s3_kx2, s3_ky2;
  logic        [2:0]        s3_tl;
  logic signed [CROSS_W-1:0] s3_kc0, s3_kc1, s3_kc2, s3_area2;
  logic signed [20:0] s3_ax, s3_ay, s3_bx, s3_by, s3_cx, s3_cy;
  logic signed [11:0] s3_minx, s3_maxx, s3_miny, s3_maxy;
  logic        [15:0] s3_src;

  // ------------------------------------------------------------ outputs ----
  assign out_valid_o  = s3_v;
  assign out_kx0_o    = s3_kx0;
  assign out_ky0_o    = s3_ky0;
  assign out_kc0_o    = s3_kc0;
  assign out_kx1_o    = s3_kx1;
  assign out_ky1_o    = s3_ky1;
  assign out_kc1_o    = s3_kc1;
  assign out_kx2_o    = s3_kx2;
  assign out_ky2_o    = s3_ky2;
  assign out_kc2_o    = s3_kc2;
  assign out_tl_o     = s3_tl;
  assign out_area2_o  = s3_area2;
  assign out_ax_o     = s3_ax;
  assign out_ay_o     = s3_ay;
  assign out_bx_o     = s3_bx;
  assign out_by_o     = s3_by;
  assign out_cx_o     = s3_cx;
  assign out_cy_o     = s3_cy;
  assign out_min_x_o  = s3_minx;
  assign out_max_x_o  = s3_maxx;
  assign out_min_y_o  = s3_miny;
  assign out_max_y_o  = s3_maxy;
  assign out_src_id_o = s3_src;

  logic [31:0] cnt_sub;
  assign triangles_submitted_o = cnt_sub;

  // ---------------------------------------------------------- sequential ---
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s1_v     <= 1'b0;
      s2_v     <= 1'b0;
      s3_v     <= 1'b0;
      s1_ax    <= 21'sd0;
      s1_ay    <= 21'sd0;
      s1_bx    <= 21'sd0;
      s1_by    <= 21'sd0;
      s1_cx    <= 21'sd0;
      s1_cy    <= 21'sd0;
      s1_area2 <= 48'sd0;
      s1_minx  <= 12'sd0;
      s1_maxx  <= 12'sd0;
      s1_miny  <= 12'sd0;
      s1_maxy  <= 12'sd0;
      s1_src   <= 16'd0;
      s2_kx0   <= {DIFF_W{1'b0}};
      s2_ky0   <= {DIFF_W{1'b0}};
      s2_kx1   <= {DIFF_W{1'b0}};
      s2_ky1   <= {DIFF_W{1'b0}};
      s2_kx2   <= {DIFF_W{1'b0}};
      s2_ky2   <= {DIFF_W{1'b0}};
      s2_tl    <= 3'd0;
      s2_p0    <= {PROD_W{1'b0}};
      s2_p1    <= {PROD_W{1'b0}};
      s2_p2    <= {PROD_W{1'b0}};
      s2_p3    <= {PROD_W{1'b0}};
      s2_ax    <= 21'sd0;
      s2_ay    <= 21'sd0;
      s2_bx    <= 21'sd0;
      s2_by    <= 21'sd0;
      s2_cx    <= 21'sd0;
      s2_cy    <= 21'sd0;
      s2_area2 <= 48'sd0;
      s2_minx  <= 12'sd0;
      s2_maxx  <= 12'sd0;
      s2_miny  <= 12'sd0;
      s2_maxy  <= 12'sd0;
      s2_src   <= 16'd0;
      s3_kx0   <= {DIFF_W{1'b0}};
      s3_ky0   <= {DIFF_W{1'b0}};
      s3_kx1   <= {DIFF_W{1'b0}};
      s3_ky1   <= {DIFF_W{1'b0}};
      s3_kx2   <= {DIFF_W{1'b0}};
      s3_ky2   <= {DIFF_W{1'b0}};
      s3_tl    <= 3'd0;
      s3_kc0   <= {CROSS_W{1'b0}};
      s3_kc1   <= {CROSS_W{1'b0}};
      s3_kc2   <= {CROSS_W{1'b0}};
      s3_area2 <= 48'sd0;
      s3_ax    <= 21'sd0;
      s3_ay    <= 21'sd0;
      s3_bx    <= 21'sd0;
      s3_by    <= 21'sd0;
      s3_cx    <= 21'sd0;
      s3_cy    <= 21'sd0;
      s3_minx  <= 12'sd0;
      s3_maxx  <= 12'sd0;
      s3_miny  <= 12'sd0;
      s3_maxy  <= 12'sd0;
      s3_src   <= 16'd0;
      cnt_sub  <= 32'd0;
    end else if (pipe_en) begin
      // ---- S0 → S1 ------------------------------------------------------
      s1_v     <= tri_valid_i;
      s1_ax    <= tri_ax_i;
      s1_ay    <= tri_ay_i;
      s1_bx    <= tri_bx_i;
      s1_by    <= tri_by_i;
      s1_cx    <= tri_cx_i;
      s1_cy    <= tri_cy_i;
      s1_area2 <= tri_area2_i;
      s1_minx  <= tri_min_x_i;
      s1_maxx  <= tri_max_x_i;
      s1_miny  <= tri_min_y_i;
      s1_maxy  <= tri_max_y_i;
      s1_src   <= tri_src_id_i;
      if (tri_valid_i && (cnt_sub != 32'hFFFF_FFFF)) cnt_sub <= cnt_sub + 32'd1;

      // ---- S1 → S2: the six differences, the three top-left bits, and the
      // ---- four products that make kc0 and kc1 --------------------------
      s2_v   <= s1_v;
      s2_kx0 <= -sub21(s1_cy, s1_by);  // edge 0 = (B,C)
      s2_ky0 <=  sub21(s1_cx, s1_bx);
      s2_kx1 <= -sub21(s1_ay, s1_cy);  // edge 1 = (C,A)
      s2_ky1 <=  sub21(s1_ax, s1_cx);
      s2_kx2 <= -sub21(s1_by, s1_ay);  // edge 2 = (A,B)
      s2_ky2 <=  sub21(s1_bx, s1_ax);
      s2_tl  <= {top_left(s1_ax, s1_ay, s1_bx, s1_by),
                 top_left(s1_cx, s1_cy, s1_ax, s1_ay),
                 top_left(s1_bx, s1_by, s1_cx, s1_cy)};
      s2_p0  <= s1_bx * s1_cy;   // kc0 = B.x·C.y − B.y·C.x
      s2_p1  <= s1_by * s1_cx;
      s2_p2  <= s1_cx * s1_ay;   // kc1 = C.x·A.y − C.y·A.x
      s2_p3  <= s1_cy * s1_ax;
      s2_ax    <= s1_ax;
      s2_ay    <= s1_ay;
      s2_bx    <= s1_bx;
      s2_by    <= s1_by;
      s2_cx    <= s1_cx;
      s2_cy    <= s1_cy;
      s2_area2 <= s1_area2;
      s2_minx  <= s1_minx;
      s2_maxx  <= s1_maxx;
      s2_miny  <= s1_miny;
      s2_maxy  <= s1_maxy;
      s2_src   <= s1_src;

      // ---- S2 → S3: the three constants; the third by the identity -------
      s3_v   <= s2_v;
      s3_kx0 <= s2_kx0;
      s3_ky0 <= s2_ky0;
      s3_kx1 <= s2_kx1;
      s3_ky1 <= s2_ky1;
      s3_kx2 <= s2_kx2;
      s3_ky2 <= s2_ky2;
      s3_tl  <= s2_tl;
      s3_kc0 <= sxprod(s2_p0) - sxprod(s2_p1);
      s3_kc1 <= sxprod(s2_p2) - sxprod(s2_p3);
      s3_kc2 <= s2_area2 - (sxprod(s2_p0) - sxprod(s2_p1))
                         - (sxprod(s2_p2) - sxprod(s2_p3));
      s3_area2 <= s2_area2;
      s3_ax    <= s2_ax;
      s3_ay    <= s2_ay;
      s3_bx    <= s2_bx;
      s3_by    <= s2_by;
      s3_cx    <= s2_cx;
      s3_cy    <= s2_cy;
      s3_minx  <= s2_minx;
      s3_maxx  <= s2_maxx;
      s3_miny  <= s2_miny;
      s3_maxy  <= s2_maxy;
      s3_src   <= s2_src;
    end
  end

endmodule : zhao_geom_setup
