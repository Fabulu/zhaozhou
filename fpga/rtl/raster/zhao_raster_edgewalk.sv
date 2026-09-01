// zhao_raster_edgewalk.sv — RASTER.EDGEWALK: the exact §8 edge-function
// coverage engine for one 16×16 tile (phase 4, ZH-022/ZH-023).
//
// Law (in citation order):
//   design/contracts/RASTER.EDGEWALK.md — the block contract (packet
//       layouts, backpressure, latency, the saturation bound proved below).
//   spec/qformats.md §8 — screenXY is S 12.8 with a ±2048 px guard band;
//       edge functions are set up EXACTLY in s64 subpixel² (Giesen bound
//       2^43−2 at p = 21); the fill test is `E0 + bias ≥ 0` on the EXACT
//       edge value with bias 0 (top-left) / −1; the RTL keeps s32 tile-local
//       stepping because the low 8 bits of E0 are CONSTANT per edge.
//   reference/src/zrender/rast.cpp — the bit-exact oracle: orient(),
//       edge_top_left(), the double-sided winding flip and the zero-area
//       reject. This module reproduces its coverage decision pixel for
//       pixel; the differential tests compare against that function itself,
//       not against a re-derivation.
//
// WHAT THIS BLOCK IS NOT: no binning (GEOM.BINNER hands it one triangle ×
// one tile), no early-Z, no attribute interpolation, no fragment shading,
// no tile-store or framebuffer writes. Coverage masks only.
//
// ---------------------------------------------------------------------------
// THE FILL RULE (why this is bit-exact, not "close")
// ---------------------------------------------------------------------------
// The oracle's per-pixel decision is
//
//   inside ⟺ (w0 + bias0 ≥ 0) && (w1 + bias1 ≥ 0) && (w2 + bias2 ≥ 0)
//
// on the EXACT s64 edge values w_i, with bias_i = 0 when edge i is top-left
// and −1 otherwise. A strict `>` drops every shared-edge pixel on BOTH
// sides; biasing a floored E' = w >> 8 drops sub-unit strictly-interior
// pixels on both sides (spec defects fixed 2026-08-15). Neither shortcut is
// taken here.
//
// The datapath narrows WITHOUT changing that decision, in two steps:
//
//   1. DECOMPOSITION (exact, §8). Write w = 256·E' + r with
//      E' = w >>> 8 (arithmetic shift = floor) and r = w[7:0] ∈ [0,255].
//      Both edge steps are multiples of 256 — one pixel of x steps w by
//      −Δy·256 and one pixel of y by +Δx·256 — so r is CONSTANT over every
//      pixel centre of the tile and is captured once at setup. Then
//         w ≥ 0  ⟺  E' ≥ 0                              (bias 0)
//         w ≥ 1  ⟺  E' > 0 ∨ (E' = 0 ∧ r ≠ 0)           (bias −1)
//      and E' steps EXACTLY by −Δy per pixel of x, +Δx per pixel of y.
//      Both tests collapse to one expression with a per-edge constant bit
//      `rnz = (r != 0)`:
//         accept ⟺ ¬neg(E') ∧ (top_left ∨ rnz ∨ E' ≠ 0)
//
//   2. SATURATION (sign-preserving, bounded proof). E' can be 35 bits for a
//      far-away triangle, but only its SIGN (and its being zero) matters,
//      and inside one 16×16 tile it moves by at most
//         15·|Δy| + 15·|Δx| ≤ 30·2^21 < 2^26
//      (|Δ| ≤ 2^21 for any pair of 21-bit S 12.8 coordinates). So the tile
//      start value is clamped to ±2^27 into a 29-bit signed accumulator:
//        · unclamped ⇒ every in-tile value is EXACT;
//        · clamped to +2^27 ⇒ the true value at the tile start was > 2^27,
//          hence > 2^27 − 2^26 = 2^26 > 0 everywhere in the tile, and the
//          clamped value is likewise ≥ 2^27 − 2^26 > 0 — same sign, same
//          non-zero, therefore the SAME accept for either bias;
//        · symmetric for −2^27.
//      Worst-case magnitude carried: 2^27 + 2^26 = 201,326,592 < 2^28, so
//      29 bits signed never overflows.
//
// The tile walk needs no bounding box. The oracle scans only the bbox
// "pixel centres in [v_min, v_max]", but a pixel that passes all three edge
// tests lies in the CLOSED triangle and therefore inside that bbox on both
// axes — the bbox can never exclude a pixel the edge functions accept. It
// is a scan-cost optimization there, and this walker visits all 256 pixel
// centres of its tile, so the two agree. (The oracle's viewport scissor is
// the tile itself in the differential tests.)
//
// Degenerate and winding handling follow the oracle exactly: area == 0 is
// rejected outright (no coverage, `job_degenerate_o`), and area < 0 swaps B
// and C — the double-sided flip — BEFORE the edge values, the per-pixel
// steps and the top-left biases are derived.
//
// Timing: one job in flight. 5 setup cycles (one shared 23×23 cross-product
// unit issues area, then the three edge values), 16 walk cycles (one 16-wide
// row mask per cycle, no stall), then 0..16 drain beats — only NON-EMPTY
// rows are emitted, with an exact `cov_last_o`. 21..37 cycles per tile job.
//
// The per-pixel fill test itself lives in zhao_raster_fill.sv so the formal
// property (tests/formal/raster_edgewalk_top_left.sby) proves the SHIPPING
// expression rather than a restatement of it.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_raster_edgewalk).

module zhao_raster_edgewalk (
  input  logic clk,
  input  logic rst_n,

  // ---- job in: one triangle × one 16×16 tile (a GEOM.BINNER tile-list
  // ---- entry). Vertices are S 12.8 screen subpixels (§8), guard band
  // ---- ±2048 px; the tile origin is the tile's top-left PIXEL.
  input  logic               job_valid_i,
  output logic               job_ready_o,
  input  logic signed [20:0] job_ax_i,
  input  logic signed [20:0] job_ay_i,
  input  logic signed [20:0] job_bx_i,
  input  logic signed [20:0] job_by_i,
  input  logic signed [20:0] job_cx_i,
  input  logic signed [20:0] job_cy_i,
  input  logic signed [11:0] job_tile_x_i,
  input  logic signed [11:0] job_tile_y_i,
  input  logic        [15:0] job_src_id_i,   // source_id passthrough

  // ---- coverage out: one NON-EMPTY tile row per beat (ready/valid) -------
  output logic               cov_valid_o,
  input  logic               cov_ready_i,
  output logic        [3:0]  cov_row_o,      // tile-local row, 0 = top
  output logic        [15:0] cov_mask_o,     // bit i = tile column i covered
  output logic               cov_last_o,     // last beat of THIS job
  output logic        [15:0] cov_src_id_o,

  // ---- per-job status (sampled with job_done_o) -------------------------
  output logic               job_done_o,        // one-cycle pulse
  output logic               job_degenerate_o,  // area == 0: culled
  output logic        [8:0]  cov_count_o        // covered pixels, 0..256
);

  // ------------------------------------------------------------- widths ----
  // DIFF_W: any difference of two 21-bit S 12.8 coordinates, |Δ| ≤ 2^21.
  // CROSS_W: the s64 setup of §8 — |E0| ≤ 2·2^21·2^21 = 2^43 (Giesen).
  // ACC_W / SAT: the tile-local stepping domain proved in the header.
  localparam int unsigned DIFF_W  = 23;
  localparam int unsigned PROD_W  = 46;
  localparam int unsigned CROSS_W = 48;
  localparam int unsigned ACC_W   = 29;

  localparam logic signed [ACC_W-1:0]   ACC_ZERO    =  29'sd0;
  localparam logic signed [ACC_W-1:0]   SAT_POS     =  29'sd134217728;   // +2^27
  localparam logic signed [ACC_W-1:0]   SAT_NEG     = -29'sd134217728;   // −2^27
  localparam logic signed [CROSS_W-1:0] CROSS_SATP  =  48'sd134217728;
  localparam logic signed [CROSS_W-1:0] CROSS_SATN  = -48'sd134217728;

  // ------------------------------------------------------------- states ----
  // S_AREA..S_W2 drive the shared cross-product unit; the result of the
  // operands driven in state X is captured in cross_r and consumed in X+1.
  localparam logic [2:0] S_IDLE  = 3'd0;
  localparam logic [2:0] S_AREA  = 3'd1;  // drive area operands
  localparam logic [2:0] S_W0    = 3'd2;  // area lands; flip; drive w0
  localparam logic [2:0] S_W1    = 3'd3;  // w0 lands; drive w1
  localparam logic [2:0] S_W2    = 3'd4;  // w1 lands; drive w2
  localparam logic [2:0] S_W3    = 3'd5;  // w2 lands
  localparam logic [2:0] S_WALK  = 3'd6;  // 16 row masks, one per cycle
  localparam logic [2:0] S_DRAIN = 3'd7;  // stream non-empty rows

  logic [2:0] state;

  // ---------------------------------------------------------- job state ----
  // b/c hold the FLIPPED vertices from S_W1 onwards (the double-sided law).
  logic signed [DIFF_W-1:0] ax_r, ay_r, bx_r, by_r, cx_r, cy_r;
  logic signed [11:0]       tile_x_r, tile_y_r;
  logic        [15:0]       src_r;
  logic                     degen_r;
  logic        [8:0]        count_r;
  logic        [4:0]        row_i;
  logic        [15:0]       pend_r;
  logic        [15:0]       mask_r [0:15];
  logic                     done_r;

  // tile-local edge accumulators (row starts) + the per-edge constant bits
  logic signed [ACC_W-1:0] e0_r, e1_r, e2_r;
  logic                    rnz0_r, rnz1_r, rnz2_r;

  // -------------------------------------------- shared cross-product unit --
  // cross = m_p·m_q − m_u·m_v, one registered 23×23 signed multiply pair.
  // This IS orient(): orient(a,b,px,py) = (b.x−a.x)(py−a.y) − (b.y−a.y)(px−a.x).
  logic signed [DIFF_W-1:0]  m_p, m_q, m_u, m_v;
  logic signed [PROD_W-1:0]  prod_pq, prod_uv;
  logic signed [CROSS_W-1:0] cross_r;

  always_comb begin
    prod_pq = m_p * m_q;
    prod_uv = m_u * m_v;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) cross_r <= {CROSS_W{1'b0}};
    else        cross_r <= $signed({{(CROSS_W-PROD_W){prod_pq[PROD_W-1]}}, prod_pq}) -
                           $signed({{(CROSS_W-PROD_W){prod_uv[PROD_W-1]}}, prod_uv});
  end

  // ------------------------------------------- tile-start pixel centre -----
  // §8: the centre of pixel p sits at 256·p + 128 subpixels. The walk starts
  // at the tile's top-left pixel centre.
  logic signed [DIFF_W-1:0] px0, py0;
  always_comb begin
    px0 = ($signed({{(DIFF_W-12){tile_x_r[11]}}, tile_x_r}) <<< 8) + 23'sd128;
    py0 = ($signed({{(DIFF_W-12){tile_y_r[11]}}, tile_y_r}) <<< 8) + 23'sd128;
  end

  // --------------------------------------------- the double-sided flip -----
  // rast.cpp: area < 0 ⇒ swap B and C, area = −area. Combinational in S_W0
  // (cross_r holds the area there); registered into b/c at the same edge.
  logic                     flip;
  logic signed [DIFF_W-1:0] bxf, byf, cxf, cyf;
  always_comb begin
    flip = cross_r[CROSS_W-1];
    bxf  = flip ? cx_r : bx_r;
    byf  = flip ? cy_r : by_r;
    cxf  = flip ? bx_r : cx_r;
    cyf  = flip ? by_r : cy_r;
  end

  // ------------------------------------------- cross-unit operand mux ------
  always_comb begin
    m_p = {DIFF_W{1'b0}};
    m_q = {DIFF_W{1'b0}};
    m_u = {DIFF_W{1'b0}};
    m_v = {DIFF_W{1'b0}};
    case (state)
      // area = orient(A,B,Cx,Cy) = (Bx−Ax)(Cy−Ay) − (By−Ay)(Cx−Ax)
      S_AREA: begin
        m_p = bx_r - ax_r;  m_q = cy_r - ay_r;
        m_u = by_r - ay_r;  m_v = cx_r - ax_r;
      end
      // w0 = orient(B,C, px0,py0) — the FLIPPED B/C, not yet registered
      S_W0: begin
        m_p = cxf - bxf;    m_q = py0 - byf;
        m_u = cyf - byf;    m_v = px0 - bxf;
      end
      // w1 = orient(C,A, px0,py0)
      S_W1: begin
        m_p = ax_r - cx_r;  m_q = py0 - cy_r;
        m_u = ay_r - cy_r;  m_v = px0 - cx_r;
      end
      // w2 = orient(A,B, px0,py0)
      S_W2: begin
        m_p = bx_r - ax_r;  m_q = py0 - ay_r;
        m_u = by_r - ay_r;  m_v = px0 - ax_r;
      end
      default: ;
    endcase
  end

  // ----------------------------------- E' = w >>> 8, clamped to ±2^27 ------
  function automatic logic signed [ACC_W-1:0] sat_estart(input logic signed [CROSS_W-1:0] w);
    logic signed [CROSS_W-1:0] sh;
    begin
      sh = w >>> 8;  // arithmetic: floor(w / 256), the §8 decomposition
      if (sh > CROSS_SATP)      sat_estart = SAT_POS;
      else if (sh < CROSS_SATN) sat_estart = SAT_NEG;
      else                      sat_estart = $signed(sh[ACC_W-1:0]);
    end
  endfunction

  // ---------------------------------- per-pixel steps and top-left bits ----
  // §8: one pixel of x steps E0 by −Δy·256 and one pixel of y by +Δx·256,
  // i.e. E' steps by −Δy / +Δx exactly. Derived from the FLIPPED vertices and
  // stable for the whole walk.
  //
  // THEY NOW GET REGISTERS, AND THE OLD COMMENT SAID WHY THEY DID NOT: "stable
  // for the whole walk, so they need no registers of their own." That is true
  // about CORRECTNESS and wrong about TIMING. Stable and on the critical path
  // is exactly the case that wants a register.
  //
  // The fit at fc6395f put ALL 100 worst gpu_clk paths inside this block:
  //
  //     cy_r[4] -> Add19 -> Add78 -> Add113 -> Add119 -> g_col[15].u_f1
  //             -> row_cov[15] -> Equal8 -> pend_r[14]
  //
  // `Add19` is this subtract. The walk was recomputing a TRIANGLE-INVARIANT
  // value from the vertex registers on every single row, and every column's
  // shift-add tree hung off it. Latching the six steps and the three top-left
  // bits once, during S_AREA, takes the vertex arithmetic off the row path
  // entirely.
  //
  // reports/MHZArchitected step 2, in its own words: "Edgewalk registered
  // steps / balanced popcount (low risk, broad TNS)".
  //
  // Nothing about the VALUES changes: the combinational forms below are
  // unchanged and are simply sampled a cycle earlier, while the vertex
  // registers already hold the accepted job and cannot move until it retires.
  logic signed [ACC_W-1:0] sx0_r, sy0_r, sx1_r, sy1_r, sx2_r, sy2_r;
  logic                    tl0_r, tl1_r, tl2_r;

  function automatic logic signed [ACC_W-1:0] ext(input logic signed [DIFF_W-1:0] d);
    ext = $signed({{(ACC_W-DIFF_W){d[DIFF_W-1]}}, d});
  endfunction

  // §8 fill convention: for a positive-area (clockwise, y-down) triangle an
  // edge is top-left iff horizontal going right (top) or going down (left).
  function automatic logic edge_top_left(input logic signed [DIFF_W-1:0] pxv,
                                         input logic signed [DIFF_W-1:0] pyv,
                                         input logic signed [DIFF_W-1:0] qxv,
                                         input logic signed [DIFF_W-1:0] qyv);
    edge_top_left = (pyv == qyv) ? (pxv < qxv) : (pyv < qyv);
  endfunction

  // ------------------------------------------- the 16-wide row evaluator ---
  // Column i of the current row is (row start + i·step_x) per edge. i is a
  // genvar, so i·step_x folds to at most three shifted adds.
  logic [15:0] row_cov;

  genvar gi;
  generate
    for (gi = 0; gi < 16; gi = gi + 1) begin : g_col
      logic signed [ACC_W-1:0] o0, o1, o2, v0, v1, v2;

      // BALANCED, not a linear chain. `a + b + c + d` written flat elaborates
      // as ((a+b)+c)+d -- three adder levels. Explicit pairing makes it two,
      // and column 15 is the one the fit named because every bit of `gi` is
      // set there, so it is the only column that pays all four terms.
      // CANONICAL SIGNED-DIGIT, one explicit form per column.
      //
      // `gi` is a genvar, so gi*sx is a CONSTANT multiply and most columns
      // need one operation rather than a four-term tree:
      //
      //     15*sx = (sx << 4) - sx        <-- the column the fit keeps naming
      //     14*sx = (sx << 4) - (sx << 1)
      //      7*sx = (sx << 3) - sx
      //
      // Only 11 and 13 need three terms; the rest need one or two, against
      // two adder LEVELS for all sixteen columns before.
      //
      // WRITTEN AS EXPLICIT SHIFTS, WHICH IS THE WHOLE POINT. The first
      // attempt wrote `gi * sx` and let the synthesiser choose: it inferred
      // TWELVE DSP BLOCKS and 84.97 MHz became 66.78, because a DSP costs
      // ~3.785 ns against ~0.5 for a LUT adder level. Shifts and adds cannot
      // be read as a multiplier.
      //
      // Exact: computed in ACC_W and wrapping mod 2^ACC_W exactly as the
      // four-term sum did, which the differential asserts rather than assumes.
      // ONE PROCEDURAL CASE, not three generate cases. A generate case arm
      // creates an unnamed generate block per column, and Verilator's
      // GENUNNAMED rejected all 51 of them. `gi` is a constant, so a
      // procedural case folds at elaboration to exactly the same logic with
      // no generate blocks at all.
      always_comb begin
        case (gi)
           0: begin o0 = ACC_ZERO; o1 = ACC_ZERO; o2 = ACC_ZERO; end
           1: begin o0 = sx0_r; o1 = sx1_r; o2 = sx2_r; end
           2: begin o0 = (sx0_r <<< 1); o1 = (sx1_r <<< 1); o2 = (sx2_r <<< 1); end
           3: begin o0 = (sx0_r <<< 1) + sx0_r; o1 = (sx1_r <<< 1) + sx1_r; o2 = (sx2_r <<< 1) + sx2_r; end
           4: begin o0 = (sx0_r <<< 2); o1 = (sx1_r <<< 2); o2 = (sx2_r <<< 2); end
           5: begin o0 = (sx0_r <<< 2) + sx0_r; o1 = (sx1_r <<< 2) + sx1_r; o2 = (sx2_r <<< 2) + sx2_r; end
           6: begin o0 = (sx0_r <<< 2) + (sx0_r <<< 1); o1 = (sx1_r <<< 2) + (sx1_r <<< 1); o2 = (sx2_r <<< 2) + (sx2_r <<< 1); end
           7: begin o0 = (sx0_r <<< 3) - sx0_r; o1 = (sx1_r <<< 3) - sx1_r; o2 = (sx2_r <<< 3) - sx2_r; end
           8: begin o0 = (sx0_r <<< 3); o1 = (sx1_r <<< 3); o2 = (sx2_r <<< 3); end
           9: begin o0 = (sx0_r <<< 3) + sx0_r; o1 = (sx1_r <<< 3) + sx1_r; o2 = (sx2_r <<< 3) + sx2_r; end
          10: begin o0 = (sx0_r <<< 3) + (sx0_r <<< 1); o1 = (sx1_r <<< 3) + (sx1_r <<< 1); o2 = (sx2_r <<< 3) + (sx2_r <<< 1); end
          11: begin o0 = (sx0_r <<< 3) + (sx0_r <<< 1) + sx0_r; o1 = (sx1_r <<< 3) + (sx1_r <<< 1) + sx1_r; o2 = (sx2_r <<< 3) + (sx2_r <<< 1) + sx2_r; end
          12: begin o0 = (sx0_r <<< 3) + (sx0_r <<< 2); o1 = (sx1_r <<< 3) + (sx1_r <<< 2); o2 = (sx2_r <<< 3) + (sx2_r <<< 2); end
          13: begin o0 = (sx0_r <<< 4) - (sx0_r <<< 1) - sx0_r; o1 = (sx1_r <<< 4) - (sx1_r <<< 1) - sx1_r; o2 = (sx2_r <<< 4) - (sx2_r <<< 1) - sx2_r; end
          14: begin o0 = (sx0_r <<< 4) - (sx0_r <<< 1); o1 = (sx1_r <<< 4) - (sx1_r <<< 1); o2 = (sx2_r <<< 4) - (sx2_r <<< 1); end
          15: begin o0 = (sx0_r <<< 4) - sx0_r; o1 = (sx1_r <<< 4) - sx1_r; o2 = (sx2_r <<< 4) - sx2_r; end
          default: begin o0 = ACC_ZERO; o1 = ACC_ZERO; o2 = ACC_ZERO; end
        endcase
      end

      assign v0 = e0_r + o0;
      assign v1 = e1_r + o1;
      assign v2 = e2_r + o2;

      // The §8 fill test, once per edge. zhao_raster_fill is a separate
      // module because it is what tests/formal/raster_edgewalk_top_left.sby
      // proves — the proof and the silicon are the same bytes.
      logic a0, a1, a2;
      zhao_raster_fill #(.W(ACC_W)) u_f0 (
        .e_i(v0), .rnz_i(rnz0_r), .tl_i(tl0_r), .accept_o(a0));
      zhao_raster_fill #(.W(ACC_W)) u_f1 (
        .e_i(v1), .rnz_i(rnz1_r), .tl_i(tl1_r), .accept_o(a1));
      zhao_raster_fill #(.W(ACC_W)) u_f2 (
        .e_i(v2), .rnz_i(rnz2_r), .tl_i(tl2_r), .accept_o(a2));

      assign row_cov[gi] = a0 && a1 && a2;
    end
  endgenerate

  // popcount of the current row mask (0..16)
  logic [4:0] row_pc;
  always_comb begin
    row_pc = 5'd0;
    for (int i = 0; i < 16; i++) row_pc = row_pc + {4'd0, row_cov[i]};
  end

  // ------------------------------------------------- drain row selector ----
  // Lowest set index of pend_r wins (rows leave the block top to bottom).
  logic [3:0]  drain_row;
  logic [15:0] drain_hot;
  always_comb begin
    drain_row = 4'd0;
    drain_hot = 16'd0;
    for (int i = 15; i >= 0; i--) begin
      if (pend_r[i]) begin
        drain_row = i[3:0];
        drain_hot = 16'd1 << i;
      end
    end
  end

  // ------------------------------------------------------------ outputs ----
  // valid never depends on ready (ready/valid hygiene).
  assign job_ready_o      = (state == S_IDLE);
  assign cov_valid_o      = (state == S_DRAIN);
  assign cov_row_o        = drain_row;
  assign cov_mask_o       = mask_r[drain_row];
  assign cov_last_o       = ((pend_r & ~drain_hot) == 16'd0);
  assign cov_src_id_o     = src_r;
  assign job_done_o       = done_r;
  assign job_degenerate_o = degen_r;
  assign cov_count_o      = count_r;

  // ---------------------------------------------------------- sequential ---
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state    <= S_IDLE;
      sx0_r    <= {ACC_W{1'b0}};
      sy0_r    <= {ACC_W{1'b0}};
      sx1_r    <= {ACC_W{1'b0}};
      sy1_r    <= {ACC_W{1'b0}};
      sx2_r    <= {ACC_W{1'b0}};
      sy2_r    <= {ACC_W{1'b0}};
      tl0_r    <= 1'b0;
      tl1_r    <= 1'b0;
      tl2_r    <= 1'b0;
      ax_r     <= {DIFF_W{1'b0}};
      ay_r     <= {DIFF_W{1'b0}};
      bx_r     <= {DIFF_W{1'b0}};
      by_r     <= {DIFF_W{1'b0}};
      cx_r     <= {DIFF_W{1'b0}};
      cy_r     <= {DIFF_W{1'b0}};
      tile_x_r <= 12'sd0;
      tile_y_r <= 12'sd0;
      src_r    <= 16'd0;
      degen_r  <= 1'b0;
      count_r  <= 9'd0;
      row_i    <= 5'd0;
      pend_r   <= 16'd0;
      done_r   <= 1'b0;
      e0_r     <= ACC_ZERO;
      e1_r     <= ACC_ZERO;
      e2_r     <= ACC_ZERO;
      rnz0_r   <= 1'b0;
      rnz1_r   <= 1'b0;
      rnz2_r   <= 1'b0;
      for (int i = 0; i < 16; i++) mask_r[i] <= 16'd0;
    end else begin
      done_r <= 1'b0;
      case (state)
        S_IDLE: begin
          if (job_valid_i) begin
            ax_r     <= $signed({{(DIFF_W-21){job_ax_i[20]}}, job_ax_i});
            ay_r     <= $signed({{(DIFF_W-21){job_ay_i[20]}}, job_ay_i});
            bx_r     <= $signed({{(DIFF_W-21){job_bx_i[20]}}, job_bx_i});
            by_r     <= $signed({{(DIFF_W-21){job_by_i[20]}}, job_by_i});
            cx_r     <= $signed({{(DIFF_W-21){job_cx_i[20]}}, job_cx_i});
            cy_r     <= $signed({{(DIFF_W-21){job_cy_i[20]}}, job_cy_i});
            tile_x_r <= job_tile_x_i;
            tile_y_r <= job_tile_y_i;
            src_r    <= job_src_id_i;
            degen_r  <= 1'b0;
            count_r  <= 9'd0;
            pend_r   <= 16'd0;
            row_i    <= 5'd0;
            state    <= S_AREA;
          end
        end

        S_AREA: state <= S_W0;  // operands driven by the mux; result next edge

        S_W0: begin
          // cross_r = 2A in subpixel² (the s64 setup). Zero area is rejected
          // outright — rast.cpp `if (area == 0) return;`.
          if (cross_r == {CROSS_W{1'b0}}) begin
            degen_r <= 1'b1;
            done_r  <= 1'b1;
            state   <= S_IDLE;
          end else begin
            bx_r  <= bxf;  by_r <= byf;   // the double-sided winding flip
            cx_r  <= cxf;  cy_r <= cyf;
            // THE STEPS ARE LATCHED HERE, FROM THE FLIPPED VERTICES, and the
            // first draft got this wrong by latching them in S_AREA -- one
            // state too early, before the flip commits. raster_edgewalk_directed
            // caught it immediately and precisely: 6 of 146, every failure a
            // WINDING case, because a pre-flip step is only wrong for the
            // triangles the flip exists to fix. The block's own header says the
            // steps are "derived from the FLIPPED vertices"; the flip lands in
            // S_W0, so this is the earliest edge at which they are real.
            //
            // `bxf`/`cyf` are used rather than `bx_r`/`cy_r` for the same
            // reason: the registers do not hold the flipped values until this
            // edge completes.
            sx0_r <= -ext(cyf - byf);   sy0_r <= ext(cxf - bxf);
            sx1_r <= -ext(ay_r - cyf);  sy1_r <= ext(ax_r - cxf);
            sx2_r <= -ext(byf - ay_r);  sy2_r <= ext(bxf - ax_r);
            tl0_r <= edge_top_left(bxf, byf, cxf, cyf);
            tl1_r <= edge_top_left(cxf, cyf, ax_r, ay_r);
            tl2_r <= edge_top_left(ax_r, ay_r, bxf, byf);
            state <= S_W1;
          end
        end

        S_W1: begin  // cross_r = w0 at the tile-start pixel centre
          e0_r   <= sat_estart(cross_r);
          rnz0_r <= (cross_r[7:0] != 8'd0);
          state  <= S_W2;
        end

        S_W2: begin  // cross_r = w1
          e1_r   <= sat_estart(cross_r);
          rnz1_r <= (cross_r[7:0] != 8'd0);
          state  <= S_W3;
        end

        S_W3: begin  // cross_r = w2
          e2_r   <= sat_estart(cross_r);
          rnz2_r <= (cross_r[7:0] != 8'd0);
          row_i  <= 5'd0;
          state  <= S_WALK;
        end

        S_WALK: begin
          // one 16-wide row mask per cycle; the row accumulators step by the
          // exact per-row delta (§8). The walk never stalls: masks land in
          // registers and only the drain phase carries backpressure.
          mask_r[row_i[3:0]] <= row_cov;
          pend_r[row_i[3:0]] <= (row_cov != 16'd0);
          count_r            <= count_r + {4'd0, row_pc};
          e0_r               <= e0_r + sy0_r;
          e1_r               <= e1_r + sy1_r;
          e2_r               <= e2_r + sy2_r;
          if (row_i == 5'd15) begin
            // the LAST row's own pend bit is not yet in pend_r; a job whose
            // only covered row is row 15 must still drain.
            if ((pend_r[14:0] != 15'd0) || (row_cov != 16'd0)) state <= S_DRAIN;
            else begin
              done_r <= 1'b1;
              state  <= S_IDLE;
            end
          end else begin
            row_i <= row_i + 5'd1;
          end
        end

        S_DRAIN: begin
          if (cov_ready_i) begin
            pend_r <= pend_r & ~drain_hot;
            if (cov_last_o) begin
              done_r <= 1'b1;
              state  <= S_IDLE;
            end
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_raster_edgewalk
