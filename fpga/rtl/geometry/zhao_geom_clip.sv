// zhao_geom_clip.sv — GEOM.CLIP: the near-plane verdict, the double-sided
// winding law, the (chosen) backface mode and the §8 scissored scan box —
// the front door of the geometry mantle (phase 5, ZH-056).
//
// Law (in citation order):
//   design/contracts/GEOM.CLIP.md — the block contract.
//   design/blocks.yml — `inputs: [view_vertices]`, `outputs:
//       [clipped_triangles]`, `upstream: [GEOM.PROJECT, TERRAIN.PROJECT]`,
//       `downstream: [GEOM.SETUP]`, `backpressure: ready_valid`,
//       `target_throughput: 1 accepted triangle per clock`, counters
//       `triangles_submitted` / `triangles_clipped` / `triangles_culled`, and
//       the note this file honours literally: "Guard band ±2048 px ratified
//       (A3c); fixed-point clipping exactly per reference." NEVER CUT (§26).
//   spec/qformats.md §8 — screenXY is S 12.8 px (21 bits), and the guard band
//       is applied as a CLAMP in `to_screen_xy`, not as a clip. See THE GUARD
//       BAND IS NOT A CLIP PLANE below.
//   spec/sky_and_beams.md §1.2 (projection corollary) — "the behind-camera
//       half culls through the one `w ≤ 0` near-plane rejection … whole-
//       primitive near-plane rejection is the documented Phase-3 clip model".
//   reference/src/zrender/rast.cpp — `project_vertex`'s `if (clip.w.raw <= 0)
//       return o;` (the vertex is marked NOT IN), the `if (area == 0) return;`
//       zero-area reject, the `area < 0` double-sided winding flip, and
//       `scan_bbox()` — the scissored pixel-centre bounding box, which this
//       block reproduces bit for bit because the oracle CALLS the very same
//       function raster_tri calls.
//   reference/src/zrender/internal.hpp — `ProjOut::in`: "false: w <= 0
//       (behind the eye) — Phase-3 near-plane rejection culls the whole
//       primitive (documented)".
//
// WHAT THIS BLOCK IS NOT: no projection (GEOM.PROJECT / TERRAIN.PROJECT own
// the matrix, the divide and the guard-band clamp — this block is handed
// screen vertices and a per-vertex behind-the-eye bit), no edge coefficients
// (GEOM.SETUP), no tile enumeration (GEOM.BINNER), no coverage
// (RASTER.EDGEWALK), no attribute handling of any kind, and no VERTEX
// GENERATION — see immediately below.
//
// ---------------------------------------------------------------------------
// LAWS FOUND (not invented) — and the biggest one is that nothing is CLIPPED
// ---------------------------------------------------------------------------
// 1. THE NEAR PLANE IS A WHOLE-PRIMITIVE REJECTION, NOT A CLIP.
//    `rast.cpp::project_vertex` returns `in = false` when `clip.w.raw <= 0`;
//    every caller in the reference — terrain.cpp ("a cell (or wall quad) whose
//    corner vertices include one behind the eye is dropped, the rest of the
//    patch still draws"), sprites.cpp, render_frame.cpp — drops the WHOLE
//    primitive on that bit. sky_and_beams.md §1.2 names it: "whole-primitive
//    near-plane rejection is the documented Phase-3 clip model", and it is why
//    the under-plane is emitted as an 8×8 cell grid in the first place.
//
//    The consequence is structural, and it is the reason this block is small:
//    **GEOM.CLIP never produces more than one triangle for one triangle in.**
//    A Sutherland–Hodgman near-plane clip would emit 1 or 2 triangles per
//    input and need a per-vertex reciprocal for the intersection parameter,
//    a vertex FIFO and an attribute lerp. None of that is built, because none
//    of it is the law this machine has. The block therefore has no output
//    queue, no variable output count and no divider.
//
// 2. THE GUARD BAND IS NOT A CLIP PLANE — IT IS AN UPSTREAM SATURATION.
//    §8: "Conversion from fx16: `rescale(x · 256, 16)` … then clamp to the
//    guard band", and `zref_fixp.hpp::to_screen_xy` does exactly that (and
//    bumps the saturation ledger). A screen vertex ARRIVING here is therefore
//    already inside ±2048 px by construction; there is no coordinate this
//    block could be handed that the guard band would reject. That is an
//    ASSUMPTION on the producer, and the producer is the clamp itself:
//    ENFORCED-BY: reference/include/zref/zref_fixp.hpp:to_screen_xy
//    (exercised at both rails by
//    tests/geometry/geom_clip_directed.cpp:test_guard_band). What the ±2048 px
//    extent buys is that the 21-bit coordinate and the §8 Giesen bound
//    (2^43−2 at p = 21) hold for every triangle that reaches the rasterizer,
//    including the wildly off-screen ones — which is precisely why they can be
//    thrown away by a cheap RECTANGLE test instead of by a clipper.
//
//    So the "guard-band clipping" of the ledger's purpose line is, in this
//    machine, the scissor test of law 3: a triangle whose scan box misses the
//    viewport is dropped whole, at ±2048 px or at 3 px, by the same compare.
//
// 3. THE SCISSOR TEST IS raster_tri's OWN EARLY RETURN.
//    rast.cpp computes, in whole pixels and scissored to the viewport,
//        min_x = max((min(Ax,Bx,Cx) + 127) >> 8, vp.x0)
//        max_x = min((max(Ax,Bx,Cx) − 128) >> 8, vp.x0 + vp.w − 1)
//    (and the same on Y) and returns immediately if `min_x > max_x ||
//    min_y > max_y`. That is the §8 pixel-CENTRE range — the 2026-08-15 defect
//    fix — and it is a part of the coverage law, not an optimization: the
//    +127/−128 form is what closes the 1-pixel seam cracks. This block emits
//    that box, so GEOM.BINNER enumerates over the same rectangle the software
//    raster scans, and the box is compared against `zref::render::scan_bbox` —
//    the function raster_tri itself calls, extracted for this increment.
//
// 4. ZERO AREA IS REJECTED, AND THE WINDING IS NORMALISED, EXACTLY AS THE
//    SOFTWARE RASTER DOES IT. `area == 0` is dropped (rast.cpp) and `area < 0`
//    swaps B and C (the double-sided law). Doing the flip HERE rather than in
//    GEOM.SETUP is the ruling this file owns: RASTER.EDGEWALK also flips, and
//    a triangle that arrives already normalised makes its flip a no-op, so the
//    two agree by construction instead of by coincidence.
//    ENFORCED-BY: tests/geometry/geom_setup_directed.cpp:test_joint_with_edgewalk
//    (coverage rebuilt from the coefficients of a normalised triangle equals
//    RASTER.EDGEWALK's own, tile for tile). And GEOM.SETUP's
//    top-left bits, which are only meaningful for a positive-area triangle,
//    are then derived from the same vertices RASTER.EDGEWALK will use.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN (no spec states these; they are decisions, recorded as such)
// ---------------------------------------------------------------------------
// A. BACKFACE CULLING IS A MODE, AND ITS DEFAULT IS OFF.
//    The ledger's purpose line names "backface cull". No spec ratifies a
//    winding convention, and the reference is explicit that it has not been
//    decided: internal.hpp says the software raster is "Double-sided (Phase-3:
//    terrain quads and the sky drum are emitted with recorded windings but the
//    software raster shades both — the RTL backface-culling freeze is Phase
//    4/5)". So the MECHANISM is built and the POLICY is not taken here:
//      `cull_mode_i` = 0 NONE (double-sided — bit-exact with rast.cpp and the
//                              reset value), 1 = reject 2A < 0, 2 = reject 2A > 0.
//    A default of anything but NONE would silently delete half the geometry of
//    every existing golden capture, and would ratify a winding by omission.
//    Which mode the shipping game binds is an owner decision and is NOT taken
//    by this increment.
//
// B. THE COUNTER SPLIT. `triangles_submitted`, `triangles_clipped` and
//    `triangles_culled` are catalog entries (spec/counters.md §1) with no
//    stated event. Chosen here:
//      · `triangles_submitted` — every triangle ACCEPTED at the input port.
//      · `triangles_clipped`   — rejected for WHERE it is: near-plane
//                                (behind the eye) or an empty scissored box.
//      · `triangles_culled`    — rejected for WHAT it is: zero area or
//                                backface. A degenerate triangle is not
//                                off-screen and an off-screen triangle is not
//                                degenerate; keeping them in one counter would
//                                make neither number readable.
//    The two are disjoint and their sum is the reject count, so
//    `submitted − clipped − culled` is exactly the accepted count.
//
// C. THE REJECT ORDER, which is what makes the counters deterministic:
//    near-plane → zero-area → backface → scissor. It is rast.cpp's own order
//    (the caller's `in` test, then `area == 0`, then the flip — where the
//    backface decision lives — then the bbox/scissor early return). A
//    behind-the-eye triangle has meaningless screen coordinates, so testing
//    its area or its box first would classify it on garbage.
//
// ---------------------------------------------------------------------------
// SHAPE AND TIMING
// ---------------------------------------------------------------------------
// Three pipeline stages, all advancing together, one triangle per clock:
//   S1  latch the packet; compute the six coordinate differences and the
//       per-axis min/max of the three vertices (pure compares).
//   S2  the two 23×23 signed products of the 2A cross product, and the pixel
//       conversion + scissor clamp of the min/max (shift, add, compare).
//   S3  2A = p0 − p1; the verdict; the winding flip; the counters.
// `job_ready_o` is `!s3_busy_with_an_accepted_packet_the_sink_will_not_take`,
// so a stalled sink stalls the whole pipe and nothing is lost. Latency is
// therefore FIXED at 3 cycles at full readiness — the ledger says `variable`,
// which a fixed latency satisfies; the contract records the stronger fact.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_geom_clip).

module zhao_geom_clip (
  input  logic clk,
  input  logic rst_n,

  // ---- triangle in: three PROJECTED screen vertices ----------------------
  // S 12.8 screen subpixels (§8), already guard-band clamped by the
  // projection. `tri_behind_i[k]` is GEOM.PROJECT's `w <= 0` verdict for
  // vertex k (rast.cpp `ProjOut::in`, inverted): bit 0 = A, 1 = B, 2 = C.
  input  logic               tri_valid_i,
  output logic               tri_ready_o,
  input  logic signed [20:0] tri_ax_i,
  input  logic signed [20:0] tri_ay_i,
  input  logic signed [20:0] tri_bx_i,
  input  logic signed [20:0] tri_by_i,
  input  logic signed [20:0] tri_cx_i,
  input  logic signed [20:0] tri_cy_i,
  input  logic        [2:0]  tri_behind_i,
  input  logic        [15:0] tri_src_id_i,   // source_id passthrough

  // ---- configuration, sampled with the packet ----------------------------
  // The scissor rectangle in whole pixels (zref::render::Viewport: a canvas
  // in Z60/Storm, one 256×192 view block in Duo — video_rules.md §3.1).
  input  logic        [11:0] vp_x0_i,
  input  logic        [11:0] vp_y0_i,
  input  logic        [11:0] vp_w_i,
  input  logic        [11:0] vp_h_i,
  input  logic        [1:0]  cull_mode_i,    // 0 NONE (default), 1 neg, 2 pos

  // ---- accepted triangles out -------------------------------------------
  // Winding-normalised (2A > 0) and carrying the scissored scan box.
  output logic               out_valid_o,
  input  logic               out_ready_i,
  output logic signed [20:0] out_ax_o,
  output logic signed [20:0] out_ay_o,
  output logic signed [20:0] out_bx_o,
  output logic signed [20:0] out_by_o,
  output logic signed [20:0] out_cx_o,
  output logic signed [20:0] out_cy_o,
  output logic signed [47:0] out_area2_o,    // 2A in subpixel², > 0
  output logic signed [11:0] out_min_x_o,    // scan box, inclusive pixels
  output logic signed [11:0] out_max_x_o,
  output logic signed [11:0] out_min_y_o,
  output logic signed [11:0] out_max_y_o,
  output logic        [15:0] out_src_id_o,

  // ---- per-triangle verdict (one-cycle pulse, every retired triangle) ----
  output logic               ret_valid_o,
  output logic        [2:0]  ret_verdict_o,  // see VERDICT_* below

  // ---- counters (spec/counters.md §4: saturate, never wrap) --------------
  output logic        [31:0] triangles_submitted_o,
  output logic        [31:0] triangles_clipped_o,
  output logic        [31:0] triangles_culled_o
);

  // The verdict encoding, shared with zref::Clip::Verdict.
  localparam logic [2:0] VERDICT_ACCEPT    = 3'd0;
  localparam logic [2:0] VERDICT_NEAR      = 3'd1;
  localparam logic [2:0] VERDICT_ZERO_AREA = 3'd2;
  localparam logic [2:0] VERDICT_BACKFACE  = 3'd3;
  localparam logic [2:0] VERDICT_OFFSCREEN = 3'd4;

  localparam logic [1:0] CULL_NONE = 2'd0;
  localparam logic [1:0] CULL_NEG  = 2'd1;
  localparam logic [1:0] CULL_POS  = 2'd2;

  // DIFF_W / PROD_W / CROSS_W are RASTER.EDGEWALK's widths, deliberately: the
  // 2A this block computes is the SAME quantity that block computes, so it is
  // carried in the same domain. |Δ| ≤ 2^20 for a pair of guard-band S 12.8
  // coordinates (|v| ≤ 2048·256 = 2^19), products ≤ 2^40, and the §8 Giesen
  // bound at p = 21 is 2^43−2; 48 bits leaves 4 bits over that bound.
  localparam int unsigned DIFF_W  = 23;
  localparam int unsigned PROD_W  = 46;
  localparam int unsigned CROSS_W = 48;
  // BOX_W: a pixel coordinate derived from a ±2^19 subpixel value spans
  // [−2049, 2048], and the scissor clamp then pulls the accepted range into
  // [0, vp.x0+vp.w−1]. 14 bits signed carries the unclamped extremes.
  localparam int unsigned BOX_W = 14;

  function automatic logic signed [DIFF_W-1:0] sx21(input logic signed [20:0] v);
    sx21 = $signed({{(DIFF_W-21){v[20]}}, v});
  endfunction

  // ---------------------------------------------------------- the box ------
  // rast.cpp's own arithmetic, in hardware:
  //   lo = max((v_min + 127) >> 8, vp0)              — first candidate pixel
  //   hi = min((v_max - 128) >> 8, vp0 + vpw - 1)    — last candidate pixel
  // `>> 8` is FLOOR on a signed value (arithmetic), which is what the C++ `>>`
  // on int32_t does and what the (v_min + 127) form needs in order to be a
  // ceiling. §8: the centre of pixel p is at 256p + 128, so `lo` is the
  // smallest p with 256p + 128 >= v_min and `hi` the largest with
  // 256p + 128 <= v_max — the pixel-CENTRE range, not ceil/floor of the vertex
  // extent (the 2026-08-15 spec defect; getting it wrong opens a 1 px crack at
  // every shared seam). Taking bits [21:8] of the 22-bit sum IS `>>> 8`: the
  // sum is bounded by 2^19 + 127 < 2^20, so the kept 14 bits never truncate.
  function automatic logic signed [BOX_W-1:0] box_lo(input logic signed [20:0] vmin,
                                                     input logic        [11:0] vp0);
    logic signed [21:0]      wide;
    logic signed [BOX_W-1:0] shifted;
    logic signed [BOX_W-1:0] lim;
    begin
      wide    = $signed({vmin[20], vmin}) + 22'sd127;
      shifted = BOX_W'(wide >>> 8);
      lim     = $signed({2'b00, vp0});
      box_lo  = (shifted > lim) ? shifted : lim;
    end
  endfunction

  function automatic logic signed [BOX_W-1:0] box_hi(input logic signed [20:0] vmax,
                                                     input logic        [11:0] vp0,
                                                     input logic        [11:0] vpw);
    logic signed [21:0]      wide;
    logic signed [BOX_W-1:0] shifted;
    logic signed [BOX_W-1:0] lim;
    begin
      wide    = $signed({vmax[20], vmax}) - 22'sd128;
      shifted = BOX_W'(wide >>> 8);
      lim     = $signed({2'b00, vp0}) + $signed({2'b00, vpw}) - 14'sd1;
      box_hi  = (shifted < lim) ? shifted : lim;
    end
  endfunction

  // ======================================================== stage enable ====
  // One elastic pipe: every stage advances on the same enable. The pipe only
  // stalls when stage 3 is holding an ACCEPTED packet the sink will not take;
  // a rejected packet retires unconditionally, so a scene of culled triangles
  // never stalls on a busy rasterizer.
  logic s1_v, s2_v, s3_v;
  logic s3_accept;
  logic pipe_en;

  assign pipe_en     = !(s3_v && s3_accept && !out_ready_i);
  assign tri_ready_o = pipe_en;

  // ============================================================ stage 1 ====
  logic signed [20:0] s1_ax, s1_ay, s1_bx, s1_by, s1_cx, s1_cy;
  logic        [15:0] s1_src;
  logic        [2:0]  s1_behind;
  logic        [1:0]  s1_cull;
  logic        [11:0] s1_x0, s1_y0, s1_w, s1_h;

  // ============================================================ stage 2 ====
  // the cross-product operands, registered at the S1→S2 edge
  logic signed [DIFF_W-1:0] s2_p, s2_q, s2_u, s2_v_op;
  logic signed [20:0] s2_ax, s2_ay, s2_bx, s2_by, s2_cx, s2_cy;
  logic        [15:0] s2_src;
  logic        [2:0]  s2_behind;
  logic        [1:0]  s2_cull;
  logic        [11:0] s2_x0, s2_y0, s2_w, s2_h;
  // per-axis extremes of the three vertices, in SUBPIXELS
  logic signed [20:0] s2_minxs, s2_maxxs, s2_minys, s2_maxys;

  logic signed [PROD_W-1:0] s2_prod_pq, s2_prod_uv;
  always_comb begin
    s2_prod_pq = s2_p * s2_q;
    s2_prod_uv = s2_u * s2_v_op;
  end

  // ============================================================ stage 3 ====
  logic signed [CROSS_W-1:0] s3_area;
  logic signed [20:0] s3_ax, s3_ay, s3_bx, s3_by, s3_cx, s3_cy;
  logic        [15:0] s3_src;
  logic        [2:0]  s3_behind;
  logic        [1:0]  s3_cull;
  // The box is registered at the OUTPUT width (12 bits, RASTER.EDGEWALK's tile
  // origin width): it is only ever consumed when the triangle is accepted, and
  // then 0 <= min <= max <= vp0+vpw-1 by the scissor clamp. The emptiness test
  // is taken at the full BOX_W width, BEFORE the truncation, because an
  // unclamped max can be as low as -2049.
  logic signed [11:0] s3_min_x, s3_max_x, s3_min_y, s3_max_y;
  logic               s3_box_empty;

  logic signed [BOX_W-1:0] box_minx_w, box_maxx_w, box_miny_w, box_maxy_w;
  logic                    box_empty_w;
  always_comb begin
    box_minx_w  = box_lo(s2_minxs, s2_x0);
    box_maxx_w  = box_hi(s2_maxxs, s2_x0, s2_w);
    box_miny_w  = box_lo(s2_minys, s2_y0);
    box_maxy_w  = box_hi(s2_maxys, s2_y0, s2_h);
    box_empty_w = (box_minx_w > box_maxx_w) || (box_miny_w > box_maxy_w);
  end

  // ---- the verdict, combinational on the stage-3 registers ---------------
  // The order IS rast.cpp's: behind-the-eye, then zero area, then the
  // backface decision (which sits where the winding flip sits), then the
  // scissored box. See LAWS CHOSEN C.
  logic       s3_near, s3_zero, s3_back;
  logic [2:0] s3_verdict;

  always_comb begin
    s3_near = (s3_behind != 3'd0);
    s3_zero = (s3_area == {CROSS_W{1'b0}});
    s3_back = ((s3_cull == CULL_NEG) && s3_area[CROSS_W-1]) ||
              ((s3_cull == CULL_POS) && !s3_area[CROSS_W-1] && !s3_zero);

    if      (s3_near)       s3_verdict = VERDICT_NEAR;
    else if (s3_zero)       s3_verdict = VERDICT_ZERO_AREA;
    else if (s3_back)       s3_verdict = VERDICT_BACKFACE;
    else if (s3_box_empty)  s3_verdict = VERDICT_OFFSCREEN;
    else                    s3_verdict = VERDICT_ACCEPT;
  end

  assign s3_accept = (s3_verdict == VERDICT_ACCEPT);

  // ---- the double-sided winding flip (rast.cpp: area < 0 ⇒ swap B, C) ----
  logic flip;
  assign flip = s3_area[CROSS_W-1];

  // ------------------------------------------------------------ outputs ----
  // valid never depends on ready (ready/valid hygiene).
  assign out_valid_o = s3_v && s3_accept;
  assign out_ax_o    = s3_ax;
  assign out_ay_o    = s3_ay;
  assign out_bx_o    = flip ? s3_cx : s3_bx;
  assign out_by_o    = flip ? s3_cy : s3_by;
  assign out_cx_o    = flip ? s3_bx : s3_cx;
  assign out_cy_o    = flip ? s3_by : s3_cy;
  assign out_area2_o = flip ? -s3_area : s3_area;
  assign out_min_x_o = s3_min_x;
  assign out_max_x_o = s3_max_x;
  assign out_min_y_o = s3_min_y;
  assign out_max_y_o = s3_max_y;
  assign out_src_id_o = s3_src;

  assign ret_valid_o   = s3_v && pipe_en;
  assign ret_verdict_o = s3_verdict;

  // ---------------------------------------------------------- sequential ---
  logic [31:0] cnt_sub, cnt_clip, cnt_cull;
  assign triangles_submitted_o = cnt_sub;
  assign triangles_clipped_o   = cnt_clip;
  assign triangles_culled_o    = cnt_cull;

  // spec/counters.md §4: internal registers saturate, never wrap.
  function automatic logic [31:0] bump(input logic [31:0] c, input logic e);
    bump = (e && (c != 32'hFFFF_FFFF)) ? (c + 32'd1) : c;
  endfunction

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s1_v      <= 1'b0;
      s2_v      <= 1'b0;
      s3_v      <= 1'b0;
      s1_ax     <= 21'sd0;
      s1_ay     <= 21'sd0;
      s1_bx     <= 21'sd0;
      s1_by     <= 21'sd0;
      s1_cx     <= 21'sd0;
      s1_cy     <= 21'sd0;
      s1_src    <= 16'd0;
      s1_behind <= 3'd0;
      s1_cull   <= CULL_NONE;
      s1_x0     <= 12'd0;
      s1_y0     <= 12'd0;
      s1_w      <= 12'd0;
      s1_h      <= 12'd0;
      s2_p      <= {DIFF_W{1'b0}};
      s2_q      <= {DIFF_W{1'b0}};
      s2_u      <= {DIFF_W{1'b0}};
      s2_v_op   <= {DIFF_W{1'b0}};
      s2_ax     <= 21'sd0;
      s2_ay     <= 21'sd0;
      s2_bx     <= 21'sd0;
      s2_by     <= 21'sd0;
      s2_cx     <= 21'sd0;
      s2_cy     <= 21'sd0;
      s2_src    <= 16'd0;
      s2_behind <= 3'd0;
      s2_cull   <= CULL_NONE;
      s2_x0     <= 12'd0;
      s2_y0     <= 12'd0;
      s2_w      <= 12'd0;
      s2_h      <= 12'd0;
      s2_minxs  <= 21'sd0;
      s2_maxxs  <= 21'sd0;
      s2_minys  <= 21'sd0;
      s2_maxys  <= 21'sd0;
      s3_area   <= {CROSS_W{1'b0}};
      s3_ax     <= 21'sd0;
      s3_ay     <= 21'sd0;
      s3_bx     <= 21'sd0;
      s3_by     <= 21'sd0;
      s3_cx     <= 21'sd0;
      s3_cy     <= 21'sd0;
      s3_src    <= 16'd0;
      s3_behind <= 3'd0;
      s3_cull   <= CULL_NONE;
      s3_min_x  <= 12'sd0;
      s3_max_x  <= 12'sd0;
      s3_min_y  <= 12'sd0;
      s3_max_y  <= 12'sd0;
      s3_box_empty <= 1'b1;
      cnt_sub   <= 32'd0;
      cnt_clip  <= 32'd0;
      cnt_cull  <= 32'd0;
    end else if (pipe_en) begin
      // ---- S0 → S1: latch the packet and its configuration ---------------
      s1_v      <= tri_valid_i;
      s1_ax     <= tri_ax_i;
      s1_ay     <= tri_ay_i;
      s1_bx     <= tri_bx_i;
      s1_by     <= tri_by_i;
      s1_cx     <= tri_cx_i;
      s1_cy     <= tri_cy_i;
      s1_src    <= tri_src_id_i;
      s1_behind <= tri_behind_i;
      s1_cull   <= cull_mode_i;
      s1_x0     <= vp_x0_i;
      s1_y0     <= vp_y0_i;
      s1_w      <= vp_w_i;
      s1_h      <= vp_h_i;
      cnt_sub   <= bump(cnt_sub, tri_valid_i);

      // ---- S1 → S2: cross-product operands and the per-axis extremes -----
      // 2A = orient(A,B,Cx,Cy) = (Bx−Ax)(Cy−Ay) − (By−Ay)(Cx−Ax), the SAME
      // expression RASTER.EDGEWALK drives into its shared multiplier.
      s2_v      <= s1_v;
      s2_p      <= sx21(s1_bx) - sx21(s1_ax);
      s2_q      <= sx21(s1_cy) - sx21(s1_ay);
      s2_u      <= sx21(s1_by) - sx21(s1_ay);
      s2_v_op   <= sx21(s1_cx) - sx21(s1_ax);
      s2_ax     <= s1_ax;
      s2_ay     <= s1_ay;
      s2_bx     <= s1_bx;
      s2_by     <= s1_by;
      s2_cx     <= s1_cx;
      s2_cy     <= s1_cy;
      s2_src    <= s1_src;
      s2_behind <= s1_behind;
      s2_cull   <= s1_cull;
      s2_x0     <= s1_x0;
      s2_y0     <= s1_y0;
      s2_w      <= s1_w;
      s2_h      <= s1_h;
      // min/max over the three vertices — permutation invariant, so it does
      // not matter that the winding flip has not happened yet (scan_bbox is
      // likewise invariant, and raster_tri asks it AFTER its own flip).
      s2_minxs  <= (s1_ax < s1_bx) ? ((s1_ax < s1_cx) ? s1_ax : s1_cx)
                                   : ((s1_bx < s1_cx) ? s1_bx : s1_cx);
      s2_maxxs  <= (s1_ax > s1_bx) ? ((s1_ax > s1_cx) ? s1_ax : s1_cx)
                                   : ((s1_bx > s1_cx) ? s1_bx : s1_cx);
      s2_minys  <= (s1_ay < s1_by) ? ((s1_ay < s1_cy) ? s1_ay : s1_cy)
                                   : ((s1_by < s1_cy) ? s1_by : s1_cy);
      s2_maxys  <= (s1_ay > s1_by) ? ((s1_ay > s1_cy) ? s1_ay : s1_cy)
                                   : ((s1_by > s1_cy) ? s1_by : s1_cy);

      // ---- S2 → S3: 2A, and the §8 scan box, scissored -------------------
      s3_v      <= s2_v;
      s3_area   <= $signed({{(CROSS_W-PROD_W){s2_prod_pq[PROD_W-1]}}, s2_prod_pq}) -
                   $signed({{(CROSS_W-PROD_W){s2_prod_uv[PROD_W-1]}}, s2_prod_uv});
      s3_ax     <= s2_ax;
      s3_ay     <= s2_ay;
      s3_bx     <= s2_bx;
      s3_by     <= s2_by;
      s3_cx     <= s2_cx;
      s3_cy     <= s2_cy;
      s3_src    <= s2_src;
      s3_behind <= s2_behind;
      s3_cull   <= s2_cull;
      s3_min_x     <= box_minx_w[11:0];
      s3_max_x     <= box_maxx_w[11:0];
      s3_min_y     <= box_miny_w[11:0];
      s3_max_y     <= box_maxy_w[11:0];
      s3_box_empty <= box_empty_w;

      // ---- S3 retire: the counters ---------------------------------------
      if (s3_v) begin
        cnt_clip <= bump(cnt_clip, (s3_verdict == VERDICT_NEAR) ||
                                   (s3_verdict == VERDICT_OFFSCREEN));
        cnt_cull <= bump(cnt_cull, (s3_verdict == VERDICT_ZERO_AREA) ||
                                   (s3_verdict == VERDICT_BACKFACE));
      end
    end
  end

endmodule : zhao_geom_clip
