// zhao_terrain_project.sv — TERRAIN.PROJECT: `project_vertex` in hardware, and
// the door between the Mantle and the rasterizer (phase 6, ZH-051).
//
// Law, in citation order:
//   design/contracts/TERRAIN.PROJECT.md — the block contract.
//   design/blocks.yml — `inputs: [lod_decisions, terrain_normals]`, `outputs:
//       [terrain_primitives, mosaic_candidates]`, `downstream: [GEOM.CLIP,
//       TEXTURE.MOSAIC]`, `backpressure: ready_valid`, `latency: variable`,
//       "1 projected vertex per clock", counter `terrain_triangles_emitted`,
//       and the note "Kept separate from GEOM.PROJECT by architect ruling
//       (1.D): merging later is a trivial edit."
//   reference/src/zrender/rast.cpp — `project_vertex`. THE law. Its five steps
//       are quoted below, because each one is a rounding decision that a
//       reimplementation would get subtly wrong.
//   reference/include/zref/zref_fixp.hpp — `mat4_vec4` (§2), `fx_div_exact`
//       (§3), `fx_mad` (§3), `to_screen_xy` (§8), `rescale_s32` (§4).
//   spec/qformats.md §2 (exact row sum, ONE rescale), §3 (single rounding),
//       §4 (round-half-up), §8 (screenXY = S 12.8, 21 bits, ±2048 px guard
//       band applied as a CLAMP, not a clip).
//   reference/src/zrender/terrain.cpp — the terrain draw path: the grid is
//       projected once per view call, and a primitive whose corner vertices
//       include one behind the eye is dropped whole.
//
// ---------------------------------------------------------------------------
// THE FIVE STEPS, AND WHY EACH ONE IS HERE
// ---------------------------------------------------------------------------
//   clip   = mat4_vec4(vp, {x,y,z,1})     §2  four 32x32 products per row
//                                             summed EXACTLY, then ONE
//                                             rescale(.,16) + saturate
//   in     = clip.w > 0                   Phase-3 whole-primitive near plane
//   ndc    = fx_div_exact(clip, w)        §3  round-half-up EXACT division
//   screen = fx_mad(ndc, half_extent, c)  §3  ONE rounding, no double round
//   px     = to_screen_xy(screen)         §8  rescale(.,8) then CLAMP to
//                                             ±2048 px
//   depth  = fx_div_exact(1, w)           D7  Q16.16 1/w
//
// The output packet of this block IS `zhao_geom_clip`'s input packet: six
// signed-21 screen coordinates, a 3-bit behind-the-eye mask and a source id.
// tests/terrain/terrain_project_chain.cpp wires the two together with no
// adapter and runs the result through GEOM.SETUP and the real rasterizer. If
// that stops being true, that file stops compiling.
//
// ---------------------------------------------------------------------------
// LAWS FOUND (not invented)
// ---------------------------------------------------------------------------
// 1. THE ROW SUM IS EXACT AND THERE IS ONE ROUNDING PER ROW. §2 is explicit:
//    "four 32x32 products summed EXACTLY in s128 per row, then ONE
//    rescale(.,16) + saturate". Rounding each product would be a second
//    rounding and A3b forbids it. The row accumulator here is 68 bits, wider
//    than the widest sum any input word can produce (3·2^62 + 2^47 < 2^64), so
//    it cannot wrap for ANY input, not merely for legal ones.
// 2. `v.w` IS THE CONSTANT 1.0, SO COLUMN 3 IS A SHIFT. `project_vertex`
//    always passes `fx16{1 << 16}`, and m[i][3] · 65536 is m[i][3] << 16
//    exactly.
// 3. clip.z IS NEVER READ, SO ROW 2 IS NEVER COMPUTED. The depth lane is
//    Q16.16 1/w (D7), not z; `ProjOut` has no z field. Nine multipliers, not
//    sixteen. Row 2's matrix words are still storable so the register file
//    stays a plain 16-word map, and the contract records that they are inert.
// 4. THE NEAR PLANE IS `clip.w <= 0`, AND A REJECTED VERTEX CARRIES ZERO.
//    `project_vertex` returns a default-constructed `ProjOut` on that branch,
//    whose `ScreenV` is {0,0,0}. This block emits those zeros and raises the
//    matching bit of `out_behind_o`, so GEOM.CLIP sees exactly what the
//    software raster's caller sees. It does NOT drop the triangle: dropping is
//    GEOM.CLIP's verdict (VERDICT_NEAR), and duplicating it here would make
//    two counters disagree about one triangle.
// 5. THE GUARD BAND IS A CLAMP. §8 and `to_screen_xy`: rescale to S 12.8, then
//    clamp to ±2048 px. GEOM.CLIP's header states the matching assumption — "a
//    screen vertex ARRIVING here is therefore already inside ±2048 px by
//    construction" — and names this block as the enforcer. It is enforced here,
//    by the clamp inside `to_screen_xy` below, at both rails on both axes.
//    ENFORCED-BY: tests/terrain/terrain_project_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THE DIVIDER, WHICH IS THE WHOLE COST OF THIS BLOCK
// ---------------------------------------------------------------------------
// `fx_div_exact` is an EXACT round-half-up division, not a reciprocal multiply.
// No reciprocal reproduces it bit-for-bit, so this block contains the first
// divider in the tree and its shape is the one real decision here.
//
// The three quotients of a vertex — ndc.x, ndc.y and 1/w — share the divisor
// `clip.w`, and `clip.w > 0` on every path that reaches the divider (the near
// plane already rejected the rest). So it is three unsigned lanes on one
// control path, and the divisor is never negative and never zero.
//
// **The quotient needs 31 bits, not 48, and that halves the block.** Write
// N = |a| << 16 (48 bits) and D = clip.w (31 bits, ≥ 1). The result saturates
// to the fx16 word, so the only interesting case is q < 2^31, and
//     q = floor(h/D) ≥ 2^31  ⟺  floor(h / 2^31) ≥ D  ⟺  h[47:31] ≥ D
// is a 31-bit compare taken BEFORE any division. When it fires the answer is a
// rail and no division is needed. When it does not fire, floor(h/2^31) < D —
// which is exactly the invariant a restoring recurrence needs in order to
// START at bit 30 with remainder h[47:31]. One compare, then 31 restoring
// steps; never 48.
//
// Each step is `t = {rem, next bit}; if (t >= D) {rem = t − D; q = 1;}`. The
// remainder stays below D ≤ 2^31−1, so a 32-bit compare covers it and the
// classic in-place form fits one 63-bit register per lane per stage.
//
// The signed wrapper is `fx_div_exact` with a positive divisor, which for
// numerator magnitude N and sign s is
//     s ≥ 0 : floor((N + D/2) / D)
//     s < 0 : −ceil((N − D/2) / D), and 0 when N ≤ D/2
// and `ceil` is `floor + (remainder ≠ 0)`, which the recurrence hands over for
// free. Both branches are therefore ONE unsigned division and no second pass.
//
// **Fully pipelined, one vertex per clock, deliberately.** The rejected
// alternative was a single iterative divider (~200 flip-flops, 31 cycles per
// vertex). It was rejected on a measurement, not a preference: a 33×33 patch
// lattice is 1,089 vertices, and even 64 visible patches at 31 cycles each is
// 2.2 M clocks against the 1.67 M-clock frame of 100 MHz at 60 Hz. An
// iterative divider cannot draw the terrain. The pipelined one costs 3 lanes ×
// 31 stages × 63 bits of register and 93 levels of 32-bit subtract, and it
// meets the ledger's rate exactly.
//
// The pipeline is RIGID: every stage advances together or none does, so a
// stalled consumer freezes the whole chain and nothing is dropped or
// reordered. Bubbles are not squeezed out — a rigid pipeline is what makes the
// fixed latency a fact rather than a hope.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN (no spec states these; they are decisions, recorded as such)
// ---------------------------------------------------------------------------
// A. THIS BLOCK TAKES A TRIANGLE AND EMITS A TRIANGLE. `project_vertex` is a
//    per-VERTEX function and the ledger's rate line is per vertex, but the
//    only consumer that exists — GEOM.CLIP — takes triangles, and the only
//    producer that exists — TERRAIN.TESS, through TERRAIN.NORMALS — emits
//    triangles. A per-vertex port would need a vertex cache and an index
//    stream between two finished blocks, i.e. a block the ledger does not
//    have. So the input packet is TERRAIN.NORMALS' input packet, the output
//    packet is GEOM.CLIP's input packet, and the three vertices go through ONE
//    projector on three consecutive clocks: one projected vertex per clock —
//    the ledger's rate, met literally — and one triangle every three clocks.
//    The cost is that a shared lattice vertex is projected once per triangle
//    that uses it (up to six times); the contract's Target throughput says
//    what that costs and what would fix it.
// B. THE DUAL VIEW IS TWO REGISTER SETS AND A PACKET BIT, NOT TWO DATAPATHS.
//    The ledger's purpose line is "Project the shared terrain cache into both
//    camera views (Duo)". Duo runs two 256×192 views that share the frame's
//    clock budget rather than each needing all of it. Holding two matrix +
//    viewport register sets (about a kilobit of flops) and selecting with
//    `view_i` costs nothing next to the divider; duplicating the datapath
//    would double the expensive part to buy a rate the mode does not need.
// C. `mosaic_candidates` IS THREE FIELDS ON THE PRIMITIVE PACKET, NOT A SECOND
//    STREAM, AND THIS BLOCK SELECTS NOTHING. terrain_rules.md §6.2 gives the
//    Mosaic PICK to TEXTURE.MOSAIC ("TEXTURE.MOSAIC picks A or B per texel
//    with the stable world-space pattern"), and the pick is per TEXEL, which
//    is not a quantity this block has. Layer E's {matA, matB, weight} triple is
//    per CELL and arrives with the primitive; it rides through unaltered so the
//    sampler receives it with the geometry it belongs to. Inventing a selection
//    rule here would ratify a §6.2 amendment by omission.
// D. THE FACE NORMAL DOES NOT RIDE THROUGH. The ledger lists `terrain_normals`
//    as an input. It is a per-triangle quantity, this block does no shading,
//    and carrying 96 bits through ~40 rigid stages would buy nothing that a
//    `src_id` re-association does not already buy — and `src_id` is the tree's
//    existing mechanism for exactly this (charter source ids). Recorded as a
//    deliberate divergence from the ledger's input list rather than hidden.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.

module zhao_terrain_project (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // configuration: the view-projection matrix and the viewport, per view.
    // Written one word at a time (a register file, the way CMD.DECODER writes
    // one), so no kilobit-wide port appears on the boundary.
    //   cfg_addr_i 0..15 : matrix element, row-major, row*4 + col, fx16
    //   cfg_addr_i 16    : viewport origin { y0 = data[27:16], x0 = data[11:0] }
    //   cfg_addr_i 17    : viewport extent { h  = data[27:16], w  = data[11:0] }
    // -----------------------------------------------------------------------
    input logic        cfg_we_i,
    input logic        cfg_view_i,
    input logic [ 4:0] cfg_addr_i,
    input logic [31:0] cfg_data_i,

    // -----------------------------------------------------------------------
    // terrain_primitives in — EXACTLY TERRAIN.NORMALS' input packet, plus the
    // view select and the layer-E Mosaic candidates that ride with the cell.
    // -----------------------------------------------------------------------
    input  logic               tri_valid_i,
    output logic               tri_ready_o,
    input  logic signed [31:0] ax_i,
    input  logic signed [31:0] ay_i,
    input  logic signed [31:0] az_i,
    input  logic signed [31:0] bx_i,
    input  logic signed [31:0] by_i,
    input  logic signed [31:0] bz_i,
    input  logic signed [31:0] cx_i,
    input  logic signed [31:0] cy_i,
    input  logic signed [31:0] cz_i,
    input  logic        [15:0] src_id_i,
    input  logic               view_i,
    input  logic        [ 7:0] mat_a_i,
    input  logic        [ 7:0] mat_b_i,
    input  logic        [ 7:0] weight_i,

    // -----------------------------------------------------------------------
    // terrain_primitives out — EXACTLY zhao_geom_clip's input packet, plus the
    // per-vertex Q16.16 1/w depth, the view tag and the Mosaic candidates.
    // -----------------------------------------------------------------------
    output logic               out_valid_o,
    input  logic               out_ready_i,
    output logic signed [20:0] out_ax_o,
    output logic signed [20:0] out_ay_o,
    output logic signed [20:0] out_bx_o,
    output logic signed [20:0] out_by_o,
    output logic signed [20:0] out_cx_o,
    output logic signed [20:0] out_cy_o,
    output logic        [ 2:0] out_behind_o,  // bit 0 = A, 1 = B, 2 = C
    output logic        [15:0] out_src_id_o,
    output logic signed [31:0] out_ad_o,      // Q16.16 1/w, vertex A
    output logic signed [31:0] out_bd_o,
    output logic signed [31:0] out_cd_o,
    output logic               out_view_o,
    output logic        [ 7:0] out_mat_a_o,   // mosaic_candidates: layer E,
    output logic        [ 7:0] out_mat_b_o,   //   forwarded, never selected
    output logic        [ 7:0] out_weight_o,

    output logic [31:0] terrain_triangles_emitted_o,
    output logic        idle_o
);

  // ---------------------------------------------------------------------------
  // widths, stated rather than assumed
  // ---------------------------------------------------------------------------
  // ROW_W: a row sum is three s32·s32 products (each |·| ≤ 2^62) plus
  // m[i][3] << 16 (|·| ≤ 2^47), so |sum| < 3·2^62 + 2^47 < 2^64. 68 bits leaves
  // four bits over that and cannot wrap for any input word.
  localparam int unsigned ROW_W = 68;
  // MAD_W: |ndc| ≤ 2^31 and hw = w·2^15 < 2^27, so the fx_mad product is
  // < 2^58 and the addend (c << 32) is < 2^45. 64 bits clears both.
  localparam int unsigned MAD_W = 64;
  // DIV_STEPS: the restoring recurrence produces 31 quotient bits once the
  // saturation compare has ruled out q ≥ 2^31 (see the header).
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

  // rescale(x, 16) = round-half-up shift then saturating narrow to the fx16
  // word (§4). The shift is arithmetic, so it floors, which is what makes
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

  // §8 to_screen_xy: rescale(.,8) — |x| ≤ 2^31, so the shift lands inside 24
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

  // |v| as an unsigned 32-bit word. INT32_MIN maps to 0x8000_0000 = 2^31,
  // which is exactly right unsigned — the one place the two's-complement
  // identity ~v + 1 is not a bug.
  function automatic logic [31:0] mag32(input logic signed [31:0] v);
    mag32 = v[31] ? (~$unsigned(v) + 32'd1) : $unsigned(v);
  endfunction

  // ---------------------------------------------------------------------------
  // configuration registers
  // ---------------------------------------------------------------------------
  logic signed [31:0] mat [0:1][0:15];
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
      vp_x0[0] <= '0;
      vp_x0[1] <= '0;
      vp_y0[0] <= '0;
      vp_y0[1] <= '0;
      vp_w[0]  <= '0;
      vp_w[1]  <= '0;
      vp_h[0]  <= '0;
      vp_h[1]  <= '0;
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
  logic out_valid_r;
  wire  advance = !out_valid_r || out_ready_i;

  // ---------------------------------------------------------------------------
  // stage 0 — the vertex sequencer: one triangle in, three vertices out
  // ---------------------------------------------------------------------------
  logic               job_valid;
  logic        [ 1:0] job_k;
  logic signed [31:0] job_x[0:2];
  logic signed [31:0] job_y[0:2];
  logic signed [31:0] job_z[0:2];
  logic        [15:0] job_src;
  logic               job_view;
  logic        [ 7:0] job_mat_a, job_mat_b, job_weight;

  wire job_last = job_valid && (job_k == 2'd2);
  wire job_free = !job_valid || job_last;
  assign tri_ready_o = advance && job_free;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      job_valid  <= 1'b0;
      job_k      <= 2'd0;
      job_x[0]   <= '0;
      job_x[1]   <= '0;
      job_x[2]   <= '0;
      job_y[0]   <= '0;
      job_y[1]   <= '0;
      job_y[2]   <= '0;
      job_z[0]   <= '0;
      job_z[1]   <= '0;
      job_z[2]   <= '0;
      job_src    <= '0;
      job_view   <= 1'b0;
      job_mat_a  <= '0;
      job_mat_b  <= '0;
      job_weight <= '0;
    end else if (advance) begin
      if (job_free && tri_valid_i) begin
        job_valid  <= 1'b1;
        job_k      <= 2'd0;
        job_x[0]   <= ax_i;
        job_y[0]   <= ay_i;
        job_z[0]   <= az_i;
        job_x[1]   <= bx_i;
        job_y[1]   <= by_i;
        job_z[1]   <= bz_i;
        job_x[2]   <= cx_i;
        job_y[2]   <= cy_i;
        job_z[2]   <= cz_i;
        job_src    <= src_id_i;
        job_view   <= view_i;
        job_mat_a  <= mat_a_i;
        job_mat_b  <= mat_b_i;
        job_weight <= weight_i;
      end else if (job_last) begin
        job_valid <= 1'b0;
      end else if (job_valid) begin
        job_k <= job_k + 2'd1;
      end
    end
  end

  // ---------------------------------------------------------------------------
  // stage 1 — §2 mat4_vec4: nine products, three EXACT row sums
  // ---------------------------------------------------------------------------
  logic signed [31:0] sel_x, sel_y, sel_z;
  always_comb begin
    sel_x = job_x[job_k];
    sel_y = job_y[job_k];
    sel_z = job_z[job_k];
  end

  logic signed [ROW_W-1:0] row_x, row_y, row_cw;
  always_comb begin
    row_x = ext64(mul32(mat[job_view][0], sel_x)) + ext64(mul32(mat[job_view][1], sel_y)) +
        ext64(mul32(mat[job_view][2], sel_z)) + (ext32r(mat[job_view][3]) <<< 16);
    row_y = ext64(mul32(mat[job_view][4], sel_x)) + ext64(mul32(mat[job_view][5], sel_y)) +
        ext64(mul32(mat[job_view][6], sel_z)) + (ext32r(mat[job_view][7]) <<< 16);
    row_cw = ext64(mul32(mat[job_view][12], sel_x)) + ext64(mul32(mat[job_view][13], sel_y)) +
        ext64(mul32(mat[job_view][14], sel_z)) + (ext32r(mat[job_view][15]) <<< 16);
  end

  logic                    s1_valid;
  logic signed [ROW_W-1:0] s1_rx, s1_ry, s1_rw;
  logic        [ 1:0]      s1_k;
  logic        [15:0]      s1_src;
  logic                    s1_view;
  logic        [ 7:0]      s1_mat_a, s1_mat_b, s1_weight;

  // ---------------------------------------------------------------------------
  // stage 2 — §2's ONE rescale per row, and the near-plane verdict
  // ---------------------------------------------------------------------------
  logic               s2_valid;
  logic signed [31:0] s2_cx, s2_cy, s2_cw;
  logic        [ 1:0] s2_k;
  logic        [15:0] s2_src;
  logic               s2_view;
  logic        [ 7:0] s2_mat_a, s2_mat_b, s2_weight;

  // ---------------------------------------------------------------------------
  // stage 3 — the divider setup
  // ---------------------------------------------------------------------------
  logic [47:0] pre_n  [0:2];
  logic [47:0] pre_h  [0:2];
  logic [30:0] pre_d;
  logic [29:0] pre_d2;
  logic [ 2:0] pre_neg;
  logic [ 2:0] pre_sat;
  logic        pre_behind;
  integer      li;

  always_comb begin
    pre_behind = (s2_cw <= 32'sd0);
    // A behind-the-eye vertex never uses its quotients, but the divisor must
    // still be legal: forcing 1 keeps the recurrence's rem < D invariant true
    // on every cycle instead of only on the cycles that matter.
    pre_d      = pre_behind ? 31'd1 : s2_cw[30:0];
    pre_d2     = pre_d[30:1];

    pre_neg[0] = !pre_behind && s2_cx[31];
    pre_neg[1] = !pre_behind && s2_cy[31];
    pre_neg[2] = 1'b0;  // the 1/w lane's numerator is the constant +1.0

    pre_n[0]   = {mag32(s2_cx), 16'b0};
    pre_n[1]   = {mag32(s2_cy), 16'b0};
    pre_n[2]   = 48'h0001_0000_0000;  // (1 << 16) << 16

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
  logic [ 1:0] s3_k;
  logic [15:0] s3_src;
  logic        s3_view;
  logic [ 7:0] s3_mat_a, s3_mat_b, s3_weight;

  // ---------------------------------------------------------------------------
  // stages 4 .. 4+DIV_STEPS−1 — the restoring recurrence
  // ---------------------------------------------------------------------------
  // dv = {rem[31:0], work[30:0]}. Each step shifts the pair left by one, so the
  // top bit of `work` joins the remainder and a quotient bit takes its place at
  // the bottom. rem < D ≤ 2^31−1 holds at every step, so dv[62] is always 0
  // before a shift and nothing is lost off the top.
  logic        dstep_valid [0:DIV_STEPS];
  logic [30:0] dstep_d     [0:DIV_STEPS];
  logic [62:0] dstep_dv    [0:DIV_STEPS][0:2];
  logic [ 2:0] dstep_neg   [0:DIV_STEPS];
  logic [ 2:0] dstep_sat   [0:DIV_STEPS];
  logic        dstep_behind[0:DIV_STEPS];
  logic [ 1:0] dstep_k     [0:DIV_STEPS];
  logic [15:0] dstep_src   [0:DIV_STEPS];
  logic        dstep_view  [0:DIV_STEPS];
  logic [ 7:0] dstep_mat_a [0:DIV_STEPS];
  logic [ 7:0] dstep_mat_b [0:DIV_STEPS];
  logic [ 7:0] dstep_wgt   [0:DIV_STEPS];

  assign dstep_valid[0]  = s3_valid;
  assign dstep_d[0]      = s3_d;
  assign dstep_dv[0][0]  = s3_dv[0];
  assign dstep_dv[0][1]  = s3_dv[1];
  assign dstep_dv[0][2]  = s3_dv[2];
  assign dstep_neg[0]    = s3_neg;
  assign dstep_sat[0]    = s3_sat;
  assign dstep_behind[0] = s3_behind;
  assign dstep_k[0]      = s3_k;
  assign dstep_src[0]    = s3_src;
  assign dstep_view[0]   = s3_view;
  assign dstep_mat_a[0]  = s3_mat_a;
  assign dstep_mat_b[0]  = s3_mat_b;
  assign dstep_wgt[0]    = s3_weight;

  genvar gs, gl;
  generate
    for (gs = 0; gs < DIV_STEPS; gs = gs + 1) begin : g_div_stage
      logic        r_valid;
      logic [30:0] r_d;
      logic [ 2:0] r_neg;
      logic [ 2:0] r_sat;
      logic        r_behind;
      logic [ 1:0] r_k;
      logic [15:0] r_src;
      logic        r_view;
      logic [ 7:0] r_mat_a, r_mat_b, r_wgt;

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
          r_k      <= '0;
          r_src    <= '0;
          r_view   <= 1'b0;
          r_mat_a  <= '0;
          r_mat_b  <= '0;
          r_wgt    <= '0;
        end else if (advance) begin
          r_valid  <= dstep_valid[gs];
          r_d      <= dstep_d[gs];
          r_neg    <= dstep_neg[gs];
          r_sat    <= dstep_sat[gs];
          r_behind <= dstep_behind[gs];
          r_k      <= dstep_k[gs];
          r_src    <= dstep_src[gs];
          r_view   <= dstep_view[gs];
          r_mat_a  <= dstep_mat_a[gs];
          r_mat_b  <= dstep_mat_b[gs];
          r_wgt    <= dstep_wgt[gs];
        end
      end

      assign dstep_valid[gs+1]  = r_valid;
      assign dstep_d[gs+1]      = r_d;
      assign dstep_neg[gs+1]    = r_neg;
      assign dstep_sat[gs+1]    = r_sat;
      assign dstep_behind[gs+1] = r_behind;
      assign dstep_k[gs+1]      = r_k;
      assign dstep_src[gs+1]    = r_src;
      assign dstep_view[gs+1]   = r_view;
      assign dstep_mat_a[gs+1]  = r_mat_a;
      assign dstep_mat_b[gs+1]  = r_mat_b;
      assign dstep_wgt[gs+1]    = r_wgt;
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
  logic        [ 1:0] s5_k;
  logic        [15:0] s5_src;
  logic               s5_view;
  logic        [ 7:0] s5_mat_a, s5_mat_b, s5_weight;

  // ---------------------------------------------------------------------------
  // stage 6 — §3 fx_mad into canvas fx16, then §8 to_screen_xy
  // ---------------------------------------------------------------------------
  //   hw = w · 2^15 = (w/2) << 16      hh = h · 2^15
  //   cx = (x0 + w/2) << 16            cy = (y0 + h/2) << 16
  //   screen = rescale(ndc · hw + (cx << 16), 16)        §3, ONE rounding
  //   px     = clamp(rescale(screen, 8), ±2048 · 256)    §8
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

  logic               s6_valid;
  logic signed [20:0] s6_px, s6_py;
  logic signed [31:0] s6_invw;
  logic               s6_behind;
  logic        [ 1:0] s6_k;
  logic        [15:0] s6_src;
  logic               s6_view;
  logic        [ 7:0] s6_mat_a, s6_mat_b, s6_weight;

  // ---------------------------------------------------------------------------
  // stage 7 — reassemble the triangle
  // ---------------------------------------------------------------------------
  logic signed [20:0] acc_x[0:2];
  logic signed [20:0] acc_y[0:2];
  logic signed [31:0] acc_d[0:2];
  // Only vertices A and B are held: vertex C's bits are still in flight at the
  // clock that assembles the triangle, so they come straight off stage 6.
  logic        [ 1:0] acc_behind;

  integer k4;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s1_valid  <= 1'b0;
      s1_rx     <= '0;
      s1_ry     <= '0;
      s1_rw     <= '0;
      s1_k      <= '0;
      s1_src    <= '0;
      s1_view   <= 1'b0;
      s1_mat_a  <= '0;
      s1_mat_b  <= '0;
      s1_weight <= '0;

      s2_valid  <= 1'b0;
      s2_cx     <= '0;
      s2_cy     <= '0;
      s2_cw     <= '0;
      s2_k      <= '0;
      s2_src    <= '0;
      s2_view   <= 1'b0;
      s2_mat_a  <= '0;
      s2_mat_b  <= '0;
      s2_weight <= '0;

      s3_valid  <= 1'b0;
      s3_d      <= '0;
      s3_dv[0]  <= '0;
      s3_dv[1]  <= '0;
      s3_dv[2]  <= '0;
      s3_neg    <= '0;
      s3_sat    <= '0;
      s3_behind <= 1'b0;
      s3_k      <= '0;
      s3_src    <= '0;
      s3_view   <= 1'b0;
      s3_mat_a  <= '0;
      s3_mat_b  <= '0;
      s3_weight <= '0;

      s5_valid  <= 1'b0;
      s5_ndc_x  <= '0;
      s5_ndc_y  <= '0;
      s5_invw   <= '0;
      s5_behind <= 1'b0;
      s5_k      <= '0;
      s5_src    <= '0;
      s5_view   <= 1'b0;
      s5_mat_a  <= '0;
      s5_mat_b  <= '0;
      s5_weight <= '0;

      s6_valid  <= 1'b0;
      s6_px     <= '0;
      s6_py     <= '0;
      s6_invw   <= '0;
      s6_behind <= 1'b0;
      s6_k      <= '0;
      s6_src    <= '0;
      s6_view   <= 1'b0;
      s6_mat_a  <= '0;
      s6_mat_b  <= '0;
      s6_weight <= '0;

      for (k4 = 0; k4 < 3; k4 = k4 + 1) begin
        acc_x[k4] <= '0;
        acc_y[k4] <= '0;
        acc_d[k4] <= '0;
      end
      acc_behind <= '0;

      out_valid_r  <= 1'b0;
      out_ax_o     <= '0;
      out_ay_o     <= '0;
      out_bx_o     <= '0;
      out_by_o     <= '0;
      out_cx_o     <= '0;
      out_cy_o     <= '0;
      out_behind_o <= '0;
      out_src_id_o <= '0;
      out_ad_o     <= '0;
      out_bd_o     <= '0;
      out_cd_o     <= '0;
      out_view_o   <= 1'b0;
      out_mat_a_o  <= '0;
      out_mat_b_o  <= '0;
      out_weight_o <= '0;

      terrain_triangles_emitted_o <= '0;
    end else if (advance) begin
      // ---- 0 -> 1 ----------------------------------------------------------
      s1_valid  <= job_valid;
      s1_rx     <= row_x;
      s1_ry     <= row_y;
      s1_rw     <= row_cw;
      s1_k      <= job_k;
      s1_src    <= job_src;
      s1_view   <= job_view;
      s1_mat_a  <= job_mat_a;
      s1_mat_b  <= job_mat_b;
      s1_weight <= job_weight;

      // ---- 1 -> 2 ----------------------------------------------------------
      s2_valid  <= s1_valid;
      s2_cx     <= rescale16_row(s1_rx);
      s2_cy     <= rescale16_row(s1_ry);
      s2_cw     <= rescale16_row(s1_rw);
      s2_k      <= s1_k;
      s2_src    <= s1_src;
      s2_view   <= s1_view;
      s2_mat_a  <= s1_mat_a;
      s2_mat_b  <= s1_mat_b;
      s2_weight <= s1_weight;

      // ---- 2 -> 3 ----------------------------------------------------------
      s3_valid <= s2_valid;
      s3_d     <= pre_d;
      for (k4 = 0; k4 < 3; k4 = k4 + 1) begin
        s3_dv[k4] <= {15'b0, pre_h[k4][47:31], pre_h[k4][30:0]};
      end
      s3_neg    <= pre_neg;
      s3_sat    <= pre_sat;
      s3_behind <= pre_behind;
      s3_k      <= s2_k;
      s3_src    <= s2_src;
      s3_view   <= s2_view;
      s3_mat_a  <= s2_mat_a;
      s3_mat_b  <= s2_mat_b;
      s3_weight <= s2_weight;

      // ---- divider tail -> 5 ----------------------------------------------
      s5_valid  <= dstep_valid[DIV_STEPS];
      s5_ndc_x  <= q_res[0];
      s5_ndc_y  <= q_res[1];
      s5_invw   <= q_res[2];
      s5_behind <= dstep_behind[DIV_STEPS];
      s5_k      <= dstep_k[DIV_STEPS];
      s5_src    <= dstep_src[DIV_STEPS];
      s5_view   <= dstep_view[DIV_STEPS];
      s5_mat_a  <= dstep_mat_a[DIV_STEPS];
      s5_mat_b  <= dstep_mat_b[DIV_STEPS];
      s5_weight <= dstep_wgt[DIV_STEPS];

      // ---- 5 -> 6 ----------------------------------------------------------
      // A behind-the-eye vertex carries {0,0,0}: `project_vertex` returns a
      // default ProjOut on that branch and never writes ScreenV at all.
      s6_valid  <= s5_valid;
      s6_px     <= s5_behind ? 21'sd0 : to_screen_xy(scr_fx_x);
      s6_py     <= s5_behind ? 21'sd0 : to_screen_xy(scr_fx_y);
      s6_invw   <= s5_behind ? 32'sd0 : s5_invw;
      s6_behind <= s5_behind;
      s6_k      <= s5_k;
      s6_src    <= s5_src;
      s6_view   <= s5_view;
      s6_mat_a  <= s5_mat_a;
      s6_mat_b  <= s5_mat_b;
      s6_weight <= s5_weight;

      // ---- 6 -> the output register ---------------------------------------
      if (out_valid_r && out_ready_i) out_valid_r <= 1'b0;
      if (s6_valid) begin
        acc_x[s6_k]      <= s6_px;
        acc_y[s6_k]      <= s6_py;
        acc_d[s6_k]      <= s6_invw;
        if (s6_k != 2'd2) acc_behind[s6_k[0]] <= s6_behind;
        if (s6_k == 2'd2) begin
          out_valid_r  <= 1'b1;
          out_ax_o     <= acc_x[0];
          out_ay_o     <= acc_y[0];
          out_bx_o     <= acc_x[1];
          out_by_o     <= acc_y[1];
          out_cx_o     <= s6_px;
          out_cy_o     <= s6_py;
          out_ad_o     <= acc_d[0];
          out_bd_o     <= acc_d[1];
          out_cd_o     <= s6_invw;
          out_behind_o <= {s6_behind, acc_behind[1], acc_behind[0]};
          out_src_id_o <= s6_src;
          out_view_o   <= s6_view;
          out_mat_a_o  <= s6_mat_a;
          out_mat_b_o  <= s6_mat_b;
          out_weight_o <= s6_weight;

          terrain_triangles_emitted_o <= terrain_triangles_emitted_o + 32'd1;
        end
      end
    end
  end

  assign out_valid_o = out_valid_r;

  // idle: nothing latched, nothing anywhere in the pipe, nothing waiting out.
  logic   pipe_busy;
  integer bi;
  always_comb begin
    pipe_busy = s1_valid || s2_valid || s3_valid || s5_valid || s6_valid;
    for (bi = 0; bi <= DIV_STEPS; bi = bi + 1) pipe_busy = pipe_busy || dstep_valid[bi];
  end
  assign idle_o = !job_valid && !pipe_busy && !out_valid_r;

endmodule : zhao_terrain_project
