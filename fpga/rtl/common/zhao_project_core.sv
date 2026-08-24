// zhao_project_core.sv — the projection law, once.
//
// Contract: design/contracts/GEOM.PROJECT.md (Notes, "Follow-up"), and
//           design/contracts/TERRAIN.PROJECT.md.
// Reference: `zref::render::project_vertex`
//   (declared reference/src/zrender/internal.hpp, implemented
//    reference/src/zrender/rast.cpp:43).
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `zhao_geom_project` and `zhao_terrain_project` each contained a complete
// implementation of `project_vertex`. Not a similar one — the SAME one: the
// two localparams, all eight helper functions, the configuration register file
// and its decode, the three row sums, the near-plane verdict, the divider
// setup, the 31-stage restoring recurrence, the quotient assembly and the
// viewport `fx_mad` were byte-identical between the two files modulo
// whitespace. `zhao_geom_project`'s header said so and called it "A COST, NOT
// A FEATURE".
//
// It was measured before it was merged, and by two independent instruments:
//
//   * `docs/OWNER_DOCKET.md` 2026-08-24 — 11 nonconstant multiplies, widest
//     operand 32 bits, **33 mapped DSPs each**, identical arithmetic
//     signatures.
//   * RUN-20260824-0522's `pair_equivalence` differential — both blocks and
//     the shipped oracle driven from one stimulus stream, **12,300 projected
//     vertices compared three ways with zero mismatches**, across two views,
//     asymmetric viewports, the near-plane boundary, both guard-band rails,
//     rotating consumer stalls, and reconfiguration without reset.
//
// The census said the two had the same SHAPE. The differential is what said
// they had the same BEHAVIOUR, and only the second is a licence to merge.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS, AND EXACTLY WHERE ITS BOUNDARY FALLS
// ---------------------------------------------------------------------------
// One vertex in, one projected vertex out, 36 clocks later, fully pipelined at
// one vertex per clock. The boundary is chosen so that BOTH callers keep the
// latency their contracts already state, to the cycle:
//
//   * `zhao_geom_project` — contract latency **fixed 36**. This core's output
//     register IS that block's output register. The caller adds no stage.
//   * `zhao_terrain_project` — contract latency **fixed 38**. This core's
//     output register is that block's `s6`, the caller adds a vertex
//     sequencer in front (1) and a triangle reassembly register behind (1).
//
// That is why the core ends at the `to_screen_xy` register and not one stage
// earlier or later. Ending earlier would have forced GEOM to add a stage;
// ending later would have forced TERRAIN to lose one. **The seam is placed by
// the two latency numbers, not by taste.**
//
// The pipeline is RIGID and its enable is the CALLER'S, taken as `en_i`: every
// stage advances together or none does, so a stalled consumer freezes the
// whole chain and nothing is dropped or reordered. This core does not derive
// its own stall condition, because the two callers back-pressure from
// different places — GEOM from this core's own output register, TERRAIN from
// the triangle register one stage further on. Deriving it here would have
// silently changed TERRAIN's handshake.
//
// `payload_i`/`payload_o` is OPAQUE. It rides the pipeline in lockstep with
// its vertex and this block never interprets it. GEOM sends a source id;
// TERRAIN sends a corner index, a source id and the layer-E Mosaic triple.
// `view_i` is NOT payload — it selects the matrix at stage 1 and the viewport
// at stage 6, so it is a first-class signal and is exposed again on the output
// because TERRAIN's packet carries it.
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
//    ENFORCED-BY: tests/geometry/geom_project_directed.cpp:main
// 3. A BEHIND-THE-EYE VERTEX CARRIES ZERO AND IS NOT DROPPED. `project_vertex`
//    returns a default-constructed `ProjOut` whose screen vertex is {0,0,0}.
//    This core emits those zeros and raises `out_behind_o`. Dropping is
//    GEOM.CLIP's verdict; duplicating it here would make two counters disagree
//    about one primitive. `w == 0` exactly is the boundary and belongs on the
//    REJECT side — `<=`, not `<`.
// 4. THE GUARD BAND IS A CLAMP, NOT A CLIP. §8's `to_screen_xy` rescales to
//    S 12.8 and clamps to ±2048 px. GEOM.CLIP's header assumes every arriving
//    vertex is already inside that band; this core is what makes that true.
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
// ---------------------------------------------------------------------------
// COST, AND THE TWO LEVERS THIS FILE DELIBERATELY DOES NOT PULL
// ---------------------------------------------------------------------------
// Eleven nonconstant multiplies: nine 32x32 row products (three rows of three;
// column 3 is a shift because v.w is 1.0) and two 32x27 `fx_mad` products.
// Widest operand 32 bits, which is one band ABOVE the measured 27-bit cliff —
// `tools/budget/calibration.json` measures a product at 1 DSP from 8 to 27 bits
// and **3** from 28 to 33. So 11 x 3 = **33 DSPs**, and the map agrees exactly.
//
// 1. **Width narrowing is where 22 of those 33 are.** At <= 27 bits the same
//    eleven products cost 11. What that needs is a PROOF that 27 bits covers a
//    world coordinate, which is a question about map size and the fixed-point
//    format and belongs to the owner, not to this file.
//    `docs/OWNER_DOCKET.md` 2026-08-24 states it as such. The proof would be
//    needed at exactly three places, and they are not equally hard:
//      - `mul32`'s two operands at the row sums: `mat[.][.]` and the incoming
//        world coordinate. The matrix words are fx16 view-projection
//        coefficients; the coordinates are fx16 world positions. **The
//        coordinate is the hard half** — it is what bounds the playable world.
//      - the `fx_mad` product `ndc * (w << 15)`: `ndc` is a full s32 quotient
//        that saturates to the fx16 rails, so narrowing it means proving the
//        POST-DIVISION range, not the world range. Different proof, and the
//        rails make it the more delicate one.
//      - `ROW_W = 68` and `MAD_W = 64` are sized from the CURRENT widths and
//        would both shrink with them; they are consequences, not inputs.
// 2. **The projected-vertex cache is not here.** TERRAIN.PROJECT projects
//    triangle corners, so a 33x33 patch performs 6,144 projections for 1,089
//    unique lattice vertices. `zhao_terrain_project.sv`'s own header records
//    this and names `GEOM.WCACHE` as the owner. It is a separate block, not a
//    parameter of this one.
//
// Both levers compose on top of this extraction, and — this is the point of
// extracting at all — each now has ONE place to land instead of two.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.

module zhao_project_core #(
    // Opaque per-vertex rider, carried in lockstep and never interpreted.
    parameter int unsigned PAYLOAD_W = 16
) (
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

    // ---- the rigid-pipeline enable, owned by the caller ---------------------
    // Every stage advances together or none does. The caller derives this from
    // wherever ITS back-pressure boundary is; see the header.
    input logic en_i,

    // ---- one vertex in, sampled on `en_i` -----------------------------------
    input logic                     in_valid_i,
    input logic signed [31:0]       vx_i,
    input logic signed [31:0]       vy_i,
    input logic signed [31:0]       vz_i,
    input logic                     view_i,
    input logic [PAYLOAD_W-1:0]     payload_i,

    // ---- one projected vertex out, 36 clocks later --------------------------
    output logic                    out_valid_o,
    output logic signed [20:0]      out_x_o,      // S 12.8 canvas x, ±2048 px
    output logic signed [20:0]      out_y_o,      // S 12.8 canvas y, ±2048 px
    output logic signed [31:0]      out_d_o,      // Q16.16 1/w
    output logic                    out_behind_o, // clip.w <= 0: vertex is zero
    output logic                    out_view_o,
    output logic [PAYLOAD_W-1:0]    out_payload_o,

    // Any vertex anywhere in the pipe, output register included. A caller with
    // its own stages ANDs its own emptiness with this.
    output logic                    busy_o
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

  logic                        s1_valid;
  logic signed [ROW_W-1:0]     s1_rx, s1_ry, s1_rw;
  logic                        s1_view;
  logic        [PAYLOAD_W-1:0] s1_pay;

  // ---------------------------------------------------------------------------
  // stage 2 — §2's ONE rescale per row, and the near-plane verdict
  // ---------------------------------------------------------------------------
  logic                        s2_valid;
  logic signed [31:0]          s2_cx, s2_cy, s2_cw;
  logic                        s2_view;
  logic        [PAYLOAD_W-1:0] s2_pay;

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

  logic                        s3_valid;
  logic        [30:0]          s3_d;
  logic        [62:0]          s3_dv[0:2];  // {rem[31:0], work[30:0]}
  logic        [ 2:0]          s3_neg;
  logic        [ 2:0]          s3_sat;
  logic                        s3_behind;
  logic                        s3_view;
  logic        [PAYLOAD_W-1:0] s3_pay;

  // ---------------------------------------------------------------------------
  // stages 4 .. 4+DIV_STEPS-1 — the restoring recurrence
  // ---------------------------------------------------------------------------
  // dv = {rem[31:0], work[30:0]}. Each step shifts the pair left by one, so the
  // top bit of `work` joins the remainder and a quotient bit takes its place at
  // the bottom. rem < D <= 2^31-1 holds at every step, so dv[62] is always 0
  // before a shift and nothing is lost off the top.
  logic                        dstep_valid [0:DIV_STEPS];
  logic        [30:0]          dstep_d     [0:DIV_STEPS];
  logic        [62:0]          dstep_dv    [0:DIV_STEPS][0:2];
  logic        [ 2:0]          dstep_neg   [0:DIV_STEPS];
  logic        [ 2:0]          dstep_sat   [0:DIV_STEPS];
  logic                        dstep_behind[0:DIV_STEPS];
  logic                        dstep_view  [0:DIV_STEPS];
  logic        [PAYLOAD_W-1:0] dstep_pay   [0:DIV_STEPS];

  assign dstep_valid[0]  = s3_valid;
  assign dstep_d[0]      = s3_d;
  assign dstep_dv[0][0]  = s3_dv[0];
  assign dstep_dv[0][1]  = s3_dv[1];
  assign dstep_dv[0][2]  = s3_dv[2];
  assign dstep_neg[0]    = s3_neg;
  assign dstep_sat[0]    = s3_sat;
  assign dstep_behind[0] = s3_behind;
  assign dstep_view[0]   = s3_view;
  assign dstep_pay[0]    = s3_pay;

  genvar gs, gl;
  generate
    for (gs = 0; gs < DIV_STEPS; gs = gs + 1) begin : g_div_stage
      logic                    r_valid;
      logic [30:0]             r_d;
      logic [ 2:0]             r_neg;
      logic [ 2:0]             r_sat;
      logic                    r_behind;
      logic                    r_view;
      logic [PAYLOAD_W-1:0]    r_pay;

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
          else if (en_i) r_dv <= nxt;
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
          r_view   <= 1'b0;
          r_pay    <= '0;
        end else if (en_i) begin
          r_valid  <= dstep_valid[gs];
          r_d      <= dstep_d[gs];
          r_neg    <= dstep_neg[gs];
          r_sat    <= dstep_sat[gs];
          r_behind <= dstep_behind[gs];
          r_view   <= dstep_view[gs];
          r_pay    <= dstep_pay[gs];
        end
      end

      assign dstep_valid[gs+1]  = r_valid;
      assign dstep_d[gs+1]      = r_d;
      assign dstep_neg[gs+1]    = r_neg;
      assign dstep_sat[gs+1]    = r_sat;
      assign dstep_behind[gs+1] = r_behind;
      assign dstep_view[gs+1]   = r_view;
      assign dstep_pay[gs+1]    = r_pay;
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

  logic                        s5_valid;
  logic signed [31:0]          s5_ndc_x, s5_ndc_y, s5_invw;
  logic                        s5_behind;
  logic                        s5_view;
  logic        [PAYLOAD_W-1:0] s5_pay;

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
      s1_valid <= 1'b0; s1_rx <= '0; s1_ry <= '0; s1_rw <= '0; s1_view <= 1'b0; s1_pay <= '0;
      s2_valid <= 1'b0; s2_cx <= '0; s2_cy <= '0; s2_cw <= '0; s2_view <= 1'b0; s2_pay <= '0;
      s3_valid <= 1'b0; s3_d <= '0; s3_dv[0] <= '0; s3_dv[1] <= '0; s3_dv[2] <= '0;
      s3_neg <= '0; s3_sat <= '0; s3_behind <= 1'b0; s3_view <= 1'b0; s3_pay <= '0;
      s5_valid <= 1'b0; s5_ndc_x <= '0; s5_ndc_y <= '0; s5_invw <= '0; s5_behind <= 1'b0;
      s5_view <= 1'b0; s5_pay <= '0;
      out_valid_o <= 1'b0;
      out_x_o <= '0; out_y_o <= '0; out_d_o <= '0; out_behind_o <= 1'b0;
      out_view_o <= 1'b0; out_payload_o <= '0;
    end else if (en_i) begin
      // stage 1
      s1_valid <= in_valid_i;
      s1_rx <= row_x;
      s1_ry <= row_y;
      s1_rw <= row_cw;
      s1_view <= view_i;
      s1_pay <= payload_i;

      // stage 2 — the one rescale per row
      s2_valid <= s1_valid;
      s2_cx <= rescale16_row(s1_rx);
      s2_cy <= rescale16_row(s1_ry);
      s2_cw <= rescale16_row(s1_rw);
      s2_view <= s1_view;
      s2_pay <= s1_pay;

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
      s3_view <= s2_view;
      s3_pay <= s2_pay;

      // stage 5 — quotients
      s5_valid <= dstep_valid[DIV_STEPS];
      s5_ndc_x <= q_res[0];
      s5_ndc_y <= q_res[1];
      s5_invw <= q_res[2];
      s5_behind <= dstep_behind[DIV_STEPS];
      s5_view <= dstep_view[DIV_STEPS];
      s5_pay <= dstep_pay[DIV_STEPS];

      // stage 6 / output — the viewport map, and the behind-the-eye zeros.
      // `project_vertex` returns a default ProjOut on the near-plane branch and
      // never writes ScreenV at all, so the vertex carries {0,0,0}.
      out_valid_o <= s5_valid;
      out_x_o <= s5_behind ? 21'sd0 : to_screen_xy(scr_fx_x);
      out_y_o <= s5_behind ? 21'sd0 : to_screen_xy(scr_fx_y);
      out_d_o <= s5_behind ? 32'sd0 : s5_invw;
      out_behind_o <= s5_behind;
      out_view_o <= s5_view;
      out_payload_o <= s5_pay;
    end
  end

  // Any vertex anywhere, output register included. `dstep_valid[0]` is
  // `s3_valid` by assignment, so the loop covers stage 3 as well.
  integer bi;
  always_comb begin
    busy_o = s1_valid || s2_valid || s5_valid || out_valid_o;
    for (bi = 0; bi <= DIV_STEPS; bi = bi + 1) busy_o = busy_o || dstep_valid[bi];
  end

endmodule : zhao_project_core
