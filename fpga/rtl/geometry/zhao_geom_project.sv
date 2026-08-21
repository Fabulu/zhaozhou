// zhao_geom_project.sv — GEOM.PROJECT: dual-view per-vertex projection.
//
// Contract: design/contracts/GEOM.PROJECT.md
// Reference: `zref::render::project_vertex`
//   (declared reference/src/zrender/internal.hpp, implemented
//    reference/src/zrender/rast.cpp:43).
//
// The ledger declared this block's reference model as `zref::GeomProject`,
// which does not exist — one of the twenty-five phantoms in
// reports/PHANTOM_REFERENCES.md. `project_vertex` is the real law, it is what
// the software raster projects every vertex with, and TERRAIN.PROJECT is
// already verified against it.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS, AND WHAT IT SHARES WITH TERRAIN.PROJECT
// ---------------------------------------------------------------------------
// This is the geometry path's projector: skinned creature vertices and warped
// vertices reach the screen through here. TERRAIN.PROJECT is the terrain path's,
// kept separate by architect ruling 1.D.
//
// The two implement the SAME per-vertex law. TERRAIN.PROJECT's own header says
// its three vertices "go through ONE projector on three consecutive clocks", so
// the projector inside it is exactly this block, wrapped in triangle framing and
// carrying terrain's material passthrough. This block is that projector with a
// vertex-level interface and no passthrough.
//
// **THE DUPLICATION IS DELIBERATE AND IT IS A COST, NOT A FEATURE.** The
// alternative was to extract a shared core from TERRAIN.PROJECT, which is 870
// lines, UNIT_VERIFIED, and has its divider stages threaded with terrain-specific
// payload. Refactoring a verified block to make room for an unverified one is
// the wrong order of operations. What makes the duplication safe rather than
// merely convenient is that BOTH are differentials against the same shipped
// oracle: they cannot silently diverge in behaviour, only in source. The
// contract records extracting a shared core as the follow-up.
//
// ---------------------------------------------------------------------------
// THE LAW, step by step, each one cited
// ---------------------------------------------------------------------------
//   clip   = mat4_vec4(vp, {x, y, z, 1.0})   qformats §2 — EXACT s128 row sum,
//                                            then ONE rescale(.,16) per row
//   if (clip.w <= 0) -> behind the eye, and the vertex carries ZERO
//   ndc    = fx_div_exact(clip, clip.w)      §3 — one rounding, exact division
//   screen = fx_mad(ndc, half_extent, c)     §3 — one rounding
//   px     = to_screen_xy(screen)            §8 — rescale(.,8) then CLAMP ±2048
//   depth  = fx_div_exact(1.0, clip.w)       Q16.16 1/w (D7)
//
// Four consequences worth stating because each is a place an implementation
// drifts:
//
// 1. `v.w` IS THE CONSTANT 1.0, so matrix column 3 is a shift, not a multiply.
// 2. `clip.z` IS NEVER READ — the depth lane is 1/w, not z, and `ProjOut` has
//    no z field. Row 2 of the matrix is never computed. Nine multipliers, not
//    sixteen. The row-2 words remain writable so the register map stays a plain
//    sixteen-word block, and they are inert by construction.
// 3. A BEHIND-THE-EYE VERTEX CARRIES ZERO AND IS NOT DROPPED. `project_vertex`
//    returns a default-constructed `ProjOut` whose screen vertex is {0,0,0}.
//    This block emits those zeros and raises `out_behind_o`. Dropping is
//    GEOM.CLIP's verdict; duplicating it here would make two counters disagree
//    about one primitive.
// 4. THE GUARD BAND IS A CLAMP, NOT A CLIP. §8's `to_screen_xy` rescales to
//    S 12.8 and clamps to ±2048 px. GEOM.CLIP's header assumes every arriving
//    vertex is already inside that band; this block is what makes that true.
//    ENFORCED-BY: tests/geometry/geom_project_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THE DIVIDER
// ---------------------------------------------------------------------------
// `fx_div_exact` is an EXACT round-half-up division, not a reciprocal multiply.
// No reciprocal reproduces it bit-for-bit, so a real divider is required.
//
// The three quotients share the divisor `clip.w`, and `clip.w > 0` on every
// path that reaches the divider — the near plane already rejected the rest — so
// all three lanes are unsigned with a divisor that is never zero and never
// negative.
//
// The quotient needs 31 bits, not 48. With N = |a| << 16 (48 bits) and
// D = clip.w (31 bits, >= 1), `q >= 2^31` is exactly `h[47:31] >= D`, a 31-bit
// compare taken BEFORE any division; when it fires the answer is a rail and no
// division happens. When it does not fire, the same inequality is the invariant
// a restoring recurrence needs to START at bit 30 with remainder h[47:31]. One
// compare, then 31 restoring steps — never 48.
//
// Fully pipelined, one vertex per clock, and RIGID: every stage advances
// together or none does, so a stalled consumer freezes the chain and nothing is
// dropped or reordered. A rigid pipeline is what makes the fixed latency a fact
// rather than a hope.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.
module zhao_geom_project (
    input logic clk,
    input logic rst_n,

    // ---- configuration: two views, sixteen matrix words + a viewport each ---
    // addr 0..15  : matrix row-major m[0..15] (row 2, words 8..11, inert)
    // addr 16     : { y0[27:16], x0[11:0] }
    // addr 17     : { h [27:16], w [11:0] }
    input logic        cfg_we_i,
    input logic        cfg_view_i,
    input logic [ 4:0] cfg_addr_i,
    input logic [31:0] cfg_data_i,

    // ---- vertices in -------------------------------------------------------
    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic signed [31:0] vx_i,
    input  logic signed [31:0] vy_i,
    input  logic signed [31:0] vz_i,
    input  logic               view_i,
    input  logic        [15:0] src_id_i,

    // ---- view vertices out — GEOM.CLIP's per-vertex packet ------------------
    output logic               out_valid_o,
    input  logic               out_ready_i,
    output logic signed [20:0] out_x_o,      // S 12.8 canvas x, clamped ±2048 px
    output logic signed [20:0] out_y_o,      // S 12.8 canvas y, clamped ±2048 px
    output logic signed [31:0] out_d_o,      // Q16.16 1/w
    output logic               out_behind_o, // clip.w <= 0: the vertex is zero
    output logic        [15:0] out_src_id_o,

    output logic [31:0] vertices_transformed_o
);

  // ---------------------------------------------------------------------------
  // widths, stated rather than assumed
  // ---------------------------------------------------------------------------
  // ROW_W: a row sum is three s32*s32 products (each |.| <= 2^62) plus
  // m[i][3] << 16 (|.| <= 2^47), so |sum| < 3*2^62 + 2^47 < 2^64. 68 bits leaves
  // four bits over that and cannot wrap for ANY input word, not merely legal ones.
  localparam int unsigned ROW_W = 68;
  // MAD_W: |ndc| <= 2^31 and hw = w*2^15 < 2^27, so the product is < 2^58 and
  // the addend (c << 32) is < 2^45. 64 bits clears both.
  localparam int unsigned MAD_W = 64;
  // DIV_STEPS: 31 quotient bits, once the saturation compare has ruled out
  // q >= 2^31.
  localparam int unsigned DIV_STEPS = 31;

  // ---------------------------------------------------------------------------
  // the pure functions, all of them views onto zref_fixp.hpp
  // ---------------------------------------------------------------------------
  function automatic logic signed [63:0] mul32(input logic signed [31:0] a,
                                               input logic signed [31:0] b);
    mul32 = $signed({{32{a[31]}}, a}) * $signed({{32{b[31]}}, b});
  endfunction

  function automatic logic signed [ROW_W-1:0] ext64(input logic signed [63:0] v);
    ext64 = $signed({{(ROW_W - 64) {v[63]}}, v});
  endfunction

  function automatic logic signed [ROW_W-1:0] ext32r(input logic signed [31:0] v);
    ext32r = $signed({{(ROW_W - 32) {v[31]}}, v});
  endfunction

  function automatic logic signed [MAD_W-1:0] ext32m(input logic signed [31:0] v);
    ext32m = $signed({{(MAD_W - 32) {v[31]}}, v});
  endfunction

  // rescale(x, 16): round-half-up shift then saturating narrow to the fx16 word
  // (§4). The shift is arithmetic, so it floors — which is what makes
  // (x + 2^15) >>> 16 round half UP rather than toward zero.
  function automatic logic signed [31:0] rescale16_row(input logic signed [ROW_W-1:0] x);
    logic signed [ROW_W-1:0] r;
    begin
      r = (x + 68'sd32768) >>> 16;
      if (r > 68'sd2147483647) rescale16_row = 32'sh7FFF_FFFF;
      else if (r < -68'sd2147483648) rescale16_row = 32'sh8000_0000;
      else rescale16_row = r[31:0];
    end
  endfunction

  function automatic logic signed [31:0] rescale16_mad(input logic signed [MAD_W-1:0] x);
    logic signed [MAD_W-1:0] r;
    begin
      r = (x + 64'sd32768) >>> 16;
      if (r > 64'sd2147483647) rescale16_mad = 32'sh7FFF_FFFF;
      else if (r < -64'sd2147483648) rescale16_mad = 32'sh8000_0000;
      else rescale16_mad = r[31:0];
    end
  endfunction

  // §8 to_screen_xy: rescale(.,8) — |x| <= 2^31, so the shift lands inside 24
  // bits and the fx16 saturating narrow cannot fire — then CLAMP to the guard
  // band. The clamp is the law; it is not a clip and it is not optional.
  function automatic logic signed [20:0] to_screen_xy(input logic signed [31:0] x);
    logic signed [40:0] r;
    begin
      r = ($signed({{9{x[31]}}, x}) + 41'sd128) >>> 8;
      if (r > 41'sd524288) to_screen_xy = 21'sd524288;
      else if (r < -41'sd524288) to_screen_xy = -21'sd524288;
      else to_screen_xy = r[20:0];
    end
  endfunction

  // |v| as an unsigned 32-bit word. INT32_MIN maps to 0x8000_0000 = 2^31, which
  // is exactly right unsigned — the one place ~v + 1 is not a bug.
  function automatic logic [31:0] mag32(input logic signed [31:0] v);
    mag32 = v[31] ? (~$unsigned(v) + 32'd1) : $unsigned(v);
  endfunction

  // ---------------------------------------------------------------------------
  // configuration registers
  // ---------------------------------------------------------------------------
  logic signed [31:0] mat  [0:1][0:15];
  logic        [11:0] vp_x0[0:1];
  logic        [11:0] vp_y0[0:1];
  logic        [11:0] vp_w [0:1];
  logic        [11:0] vp_h [0:1];

  integer ci;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (ci = 0; ci < 16; ci = ci + 1) begin
        mat[0][ci] <= '0;
        mat[1][ci] <= '0;
      end
      vp_x0[0] <= '0; vp_x0[1] <= '0;
      vp_y0[0] <= '0; vp_y0[1] <= '0;
      vp_w[0]  <= '0; vp_w[1]  <= '0;
      vp_h[0]  <= '0; vp_h[1]  <= '0;
    end else if (cfg_we_i) begin
      if (cfg_addr_i < 5'd16) begin
        mat[cfg_view_i][cfg_addr_i[3:0]] <= $signed(cfg_data_i);
      end else if (cfg_addr_i == 5'd16) begin
        vp_x0[cfg_view_i] <= cfg_data_i[11:0];
        vp_y0[cfg_view_i] <= cfg_data_i[27:16];
      end else if (cfg_addr_i == 5'd17) begin
        vp_w[cfg_view_i] <= cfg_data_i[11:0];
        vp_h[cfg_view_i] <= cfg_data_i[27:16];
      end
    end
  end

  // ---------------------------------------------------------------------------
  // the rigid-pipeline advance
  // ---------------------------------------------------------------------------
  // One enable for every stage. A stalled consumer freezes the whole chain; no
  // stage advances alone, so nothing is dropped and nothing overtakes.
  logic advance;
  assign advance = !out_valid_o || out_ready_i;
  assign v_ready_o = advance;

  logic accept;
  assign accept = v_valid_i && v_ready_o;

  // ---------------------------------------------------------------------------
  // stage 1 — §2 mat4_vec4: nine products, three EXACT row sums
  // ---------------------------------------------------------------------------
  logic signed [ROW_W-1:0] row_x, row_y, row_cw;
  always_comb begin
    row_x = ext64(mul32(mat[view_i][0], vx_i)) + ext64(mul32(mat[view_i][1], vy_i)) +
        ext64(mul32(mat[view_i][2], vz_i)) + (ext32r(mat[view_i][3]) <<< 16);
    row_y = ext64(mul32(mat[view_i][4], vx_i)) + ext64(mul32(mat[view_i][5], vy_i)) +
        ext64(mul32(mat[view_i][6], vz_i)) + (ext32r(mat[view_i][7]) <<< 16);
    row_cw = ext64(mul32(mat[view_i][12], vx_i)) + ext64(mul32(mat[view_i][13], vy_i)) +
        ext64(mul32(mat[view_i][14], vz_i)) + (ext32r(mat[view_i][15]) <<< 16);
  end

  logic                    s1_valid;
  logic signed [ROW_W-1:0] s1_rx, s1_ry, s1_rw;
  logic        [15:0]      s1_src;
  logic                    s1_view;

  // ---------------------------------------------------------------------------
  // stage 2 — §2's ONE rescale per row, and the near-plane verdict
  // ---------------------------------------------------------------------------
  logic               s2_valid;
  logic signed [31:0] s2_cx, s2_cy, s2_cw;
  logic        [15:0] s2_src;
  logic               s2_view;

  // ---------------------------------------------------------------------------
  // stage 3 — the divider setup
  // ---------------------------------------------------------------------------
  logic [47:0] pre_n [0:2];
  logic [47:0] pre_h [0:2];
  logic [30:0] pre_d;
  logic [29:0] pre_d2;
  logic [ 2:0] pre_neg;
  logic [ 2:0] pre_sat;
  logic        pre_behind;
  integer      li;

  always_comb begin
    pre_behind = (s2_cw <= 32'sd0);
    // A behind-the-eye vertex never uses its quotients, but the divisor must
    // still be legal: forcing 1 keeps the recurrence's rem < D invariant true on
    // every cycle instead of only on the cycles that matter.
    pre_d  = pre_behind ? 31'd1 : s2_cw[30:0];
    pre_d2 = pre_d[30:1];

    pre_neg[0] = !pre_behind && s2_cx[31];
    pre_neg[1] = !pre_behind && s2_cy[31];
    pre_neg[2] = 1'b0;  // the 1/w lane's numerator is the constant +1.0

    pre_n[0] = {mag32(s2_cx), 16'b0};
    pre_n[1] = {mag32(s2_cy), 16'b0};
    pre_n[2] = 48'h0001_0000_0000;  // (1 << 16) << 16

    for (li = 0; li < 3; li = li + 1) begin
      if (pre_neg[li]) begin
        pre_h[li] = (pre_n[li] >= {18'b0, pre_d2}) ? (pre_n[li] - {18'b0, pre_d2}) : 48'd0;
      end else begin
        pre_h[li] = pre_n[li] + {18'b0, pre_d2};
      end
      pre_sat[li] = ({14'b0, pre_h[li][47:31]} >= pre_d);
    end
  end

  logic        s3_valid;
  logic [30:0] s3_d;
  logic [62:0] s3_dv[0:2];  // {rem[31:0], work[30:0]}
  logic [ 2:0] s3_neg;
  logic [ 2:0] s3_sat;
  logic        s3_behind;
  logic [15:0] s3_src;
  logic        s3_view;

  // ---------------------------------------------------------------------------
  // stages 4 .. 4+DIV_STEPS-1 — the restoring recurrence
  // ---------------------------------------------------------------------------
  // dv = {rem[31:0], work[30:0]}. Each step shifts the pair left by one, so the
  // top bit of `work` joins the remainder and a quotient bit takes its place at
  // the bottom. rem < D <= 2^31-1 holds at every step, so dv[62] is always 0
  // before a shift and nothing is lost off the top.
  logic        dstep_valid [0:DIV_STEPS];
  logic [30:0] dstep_d     [0:DIV_STEPS];
  logic [62:0] dstep_dv    [0:DIV_STEPS][0:2];
  logic [ 2:0] dstep_neg   [0:DIV_STEPS];
  logic [ 2:0] dstep_sat   [0:DIV_STEPS];
  logic        dstep_behind[0:DIV_STEPS];
  logic [15:0] dstep_src   [0:DIV_STEPS];
  logic        dstep_view  [0:DIV_STEPS];

  assign dstep_valid[0]  = s3_valid;
  assign dstep_d[0]      = s3_d;
  assign dstep_dv[0][0]  = s3_dv[0];
  assign dstep_dv[0][1]  = s3_dv[1];
  assign dstep_dv[0][2]  = s3_dv[2];
  assign dstep_neg[0]    = s3_neg;
  assign dstep_sat[0]    = s3_sat;
  assign dstep_behind[0] = s3_behind;
  assign dstep_src[0]    = s3_src;
  assign dstep_view[0]   = s3_view;

  genvar gs, gl;
  generate
    for (gs = 0; gs < DIV_STEPS; gs = gs + 1) begin : g_div_stage
      logic        r_valid;
      logic [30:0] r_d;
      logic [ 2:0] r_neg;
      logic [ 2:0] r_sat;
      logic        r_behind;
      logic [15:0] r_src;
      logic        r_view;

      for (gl = 0; gl < 3; gl = gl + 1) begin : g_div_lane
        logic [31:0] t;
        logic [62:0] nxt;
        logic [62:0] r_dv;
        always_comb begin
          t   = {dstep_dv[gs][gl][61:31], dstep_dv[gs][gl][30]};
          nxt = (t >= {1'b0, dstep_d[gs]}) ?
              {t - {1'b0, dstep_d[gs]}, dstep_dv[gs][gl][29:0], 1'b1} :
              {t, dstep_dv[gs][gl][29:0], 1'b0};
        end
        always_ff @(posedge clk or negedge rst_n) begin
          if (!rst_n) r_dv <= '0;
          else if (advance) r_dv <= nxt;
        end
        assign dstep_dv[gs+1][gl] = r_dv;
      end

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          r_valid  <= 1'b0;
          r_d      <= '0;
          r_neg    <= '0;
          r_sat    <= '0;
          r_behind <= 1'b0;
          r_src    <= '0;
          r_view   <= 1'b0;
        end else if (advance) begin
          r_valid  <= dstep_valid[gs];
          r_d      <= dstep_d[gs];
          r_neg    <= dstep_neg[gs];
          r_sat    <= dstep_sat[gs];
          r_behind <= dstep_behind[gs];
          r_src    <= dstep_src[gs];
          r_view   <= dstep_view[gs];
        end
      end

      assign dstep_valid[gs+1]  = r_valid;
      assign dstep_d[gs+1]      = r_d;
      assign dstep_neg[gs+1]    = r_neg;
      assign dstep_sat[gs+1]    = r_sat;
      assign dstep_behind[gs+1] = r_behind;
      assign dstep_src[gs+1]    = r_src;
      assign dstep_view[gs+1]   = r_view;
    end
  endgenerate

  // ---------------------------------------------------------------------------
  // stage 5 — the quotients, signed and saturated exactly as fx_div_exact does
  // ---------------------------------------------------------------------------
  // A negative result whose magnitude is exactly 2^31 is INT32_MIN and is NOT a
  // saturation; a saturating negative is also INT32_MIN. Both land on the same
  // word, which is why the rail test can be taken before the division without
  // losing the exact case.
  logic signed [31:0] q_res[0:2];
  logic        [31:0] q_mag[0:2];
  integer             qi;
  always_comb begin
    for (qi = 0; qi < 3; qi = qi + 1) begin
      q_mag[qi] = {1'b0, dstep_dv[DIV_STEPS][qi][30:0]} +
          ((dstep_neg[DIV_STEPS][qi] && (dstep_dv[DIV_STEPS][qi][62:31] != 32'd0)) ? 32'd1 :
                                                                                     32'd0);
      if (dstep_sat[DIV_STEPS][qi]) begin
        q_res[qi] = dstep_neg[DIV_STEPS][qi] ? 32'sh8000_0000 : 32'sh7FFF_FFFF;
      end else begin
        q_res[qi] = dstep_neg[DIV_STEPS][qi] ? $signed(~q_mag[qi] + 32'd1) : $signed(q_mag[qi]);
      end
    end
  end

  logic               s5_valid;
  logic signed [31:0] s5_ndc_x, s5_ndc_y, s5_invw;
  logic               s5_behind;
  logic        [15:0] s5_src;
  logic               s5_view;

  // ---------------------------------------------------------------------------
  // stage 6 — §3 fx_mad into canvas fx16, then §8 to_screen_xy
  // ---------------------------------------------------------------------------
  //   hw = w * 2^15 = (w/2) << 16      hh = h * 2^15
  //   cx = (x0 + w/2) << 16            cy = (y0 + h/2) << 16
  //   screen = rescale(ndc*hw + (cx << 16), 16)      §3, ONE rounding
  //   px     = clamp(rescale(screen, 8), ±2048*256)  §8
  // `cx << 16` inside fx_mad's own `(c << 16)` makes the whole addend
  // (x0 + w/2) << 32, which is why the shift below is 32 and not 16.
  logic [12:0] cx13, cy13;
  logic signed [MAD_W-1:0] mad_x, mad_y;
  logic signed [31:0] scr_fx_x, scr_fx_y;
  always_comb begin
    cx13 = {1'b0, vp_x0[s5_view]} + {2'b0, vp_w[s5_view][11:1]};
    cy13 = {1'b0, vp_y0[s5_view]} + {2'b0, vp_h[s5_view][11:1]};
    mad_x = ext32m(s5_ndc_x) * $signed({{(MAD_W - 27) {1'b0}}, vp_w[s5_view], 15'b0}) +
        ($signed({{(MAD_W - 13) {1'b0}}, cx13}) <<< 32);
    mad_y = ext32m(s5_ndc_y) * $signed({{(MAD_W - 27) {1'b0}}, vp_h[s5_view], 15'b0}) +
        ($signed({{(MAD_W - 13) {1'b0}}, cy13}) <<< 32);
    scr_fx_x = rescale16_mad(mad_x);
    scr_fx_y = rescale16_mad(mad_y);
  end

  // ---------------------------------------------------------------------------
  // the pipeline registers
  // ---------------------------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s1_valid <= 1'b0; s1_rx <= '0; s1_ry <= '0; s1_rw <= '0; s1_src <= '0; s1_view <= 1'b0;
      s2_valid <= 1'b0; s2_cx <= '0; s2_cy <= '0; s2_cw <= '0; s2_src <= '0; s2_view <= 1'b0;
      s3_valid <= 1'b0; s3_d <= '0; s3_dv[0] <= '0; s3_dv[1] <= '0; s3_dv[2] <= '0;
      s3_neg <= '0; s3_sat <= '0; s3_behind <= 1'b0; s3_src <= '0; s3_view <= 1'b0;
      s5_valid <= 1'b0; s5_ndc_x <= '0; s5_ndc_y <= '0; s5_invw <= '0; s5_behind <= 1'b0;
      s5_src <= '0; s5_view <= 1'b0;
      out_valid_o <= 1'b0;
      out_x_o <= '0; out_y_o <= '0; out_d_o <= '0; out_behind_o <= 1'b0; out_src_id_o <= '0;
      vertices_transformed_o <= '0;
    end else if (advance) begin
      // stage 1
      s1_valid <= accept;
      s1_rx <= row_x;
      s1_ry <= row_y;
      s1_rw <= row_cw;
      s1_src <= src_id_i;
      s1_view <= view_i;

      // stage 2 — the one rescale per row
      s2_valid <= s1_valid;
      s2_cx <= rescale16_row(s1_rx);
      s2_cy <= rescale16_row(s1_ry);
      s2_cw <= rescale16_row(s1_rw);
      s2_src <= s1_src;
      s2_view <= s1_view;

      // stage 3 — divider setup
      s3_valid <= s2_valid;
      s3_d <= pre_d;
      // rem = h[47:31] (17 bits, zero-extended into the 32-bit remainder
      // field), work = h[30:0]. The saturation compare above has already ruled
      // out rem >= D, which is exactly the invariant the recurrence needs.
      s3_dv[0] <= {15'b0, pre_h[0][47:31], pre_h[0][30:0]};
      s3_dv[1] <= {15'b0, pre_h[1][47:31], pre_h[1][30:0]};
      s3_dv[2] <= {15'b0, pre_h[2][47:31], pre_h[2][30:0]};
      s3_neg <= pre_neg;
      s3_sat <= pre_sat;
      s3_behind <= pre_behind;
      s3_src <= s2_src;
      s3_view <= s2_view;

      // stage 5 — quotients
      s5_valid <= dstep_valid[DIV_STEPS];
      s5_ndc_x <= q_res[0];
      s5_ndc_y <= q_res[1];
      s5_invw <= q_res[2];
      s5_behind <= dstep_behind[DIV_STEPS];
      s5_src <= dstep_src[DIV_STEPS];
      s5_view <= dstep_view[DIV_STEPS];

      // stage 6 / output — the viewport map, and the behind-the-eye zeros
      out_valid_o <= s5_valid;
      out_x_o <= s5_behind ? 21'sd0 : to_screen_xy(scr_fx_x);
      out_y_o <= s5_behind ? 21'sd0 : to_screen_xy(scr_fx_y);
      out_d_o <= s5_behind ? 32'sd0 : s5_invw;
      out_behind_o <= s5_behind;
      out_src_id_o <= s5_src;

      // Counts vertices ACCEPTED, not offered: a vertex held off by
      // backpressure has not been transformed.
      if (accept && vertices_transformed_o != 32'hFFFF_FFFF) begin
        vertices_transformed_o <= vertices_transformed_o + 32'd1;
      end
    end
  end

endmodule : zhao_geom_project
