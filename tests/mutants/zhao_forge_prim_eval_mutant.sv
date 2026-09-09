// zhao_forge_prim_eval_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// This exists to make `walk_overrun_o` a detector instead of a hopeful zero.
//
// That counter watches a STATE VIOLATION -- the point cursor passing the
// polyline end while the walk is live. While the S_E1 advance compare is
// correct that state is unreachable, so NO legal stimulus can move the
// counter, and "it can fire" stays an argument forever. The only demonstration
// is to break the guard, which is what this copy does:
//
//     if (pt_q != poly_n_q)   ->   if (pt_q != poly_n_q + 7'd1)
//
// admitting one extra point past every polyline end. The driver
// (tests/forge/forge_prim_eval_overrun_control.cpp) PASSES WHEN THE COUNTER
// FIRES -- inverse polarity, deliberately: it is evidence about the
// instrument, not about the design.
//
// It is a separate FILE rather than a temporary edit for the two standing
// reasons: a temporary edit to production RTL is a fit-corrupting live-tree
// hazard, and it leaves nothing behind -- the next person inherits the same
// argument and no evidence. The module is RENAMED so a source-list mistake
// cannot elaborate it in place of the real one. Both simulation-only $fatal
// checks are disabled IN THIS COPY ONLY, with the reasons beside them.
//
// REGENERATE IT if zhao_forge_prim_eval.sv changes shape: this is a copy, and
// a copy of an old version is a positive control for a block that no longer
// exists.

// zhao_forge_prim_eval.sv — FORGE.PRIM.EVAL: the lightning position evaluator.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS
// ---------------------------------------------------------------------------
// zhao_forge_prim owns TOPOLOGY only — its header line says positions "come
// from `params` through the evaluator", and no evaluator existed. The owner's
// reports/ADDLIGHTNING.md (2026-09-04) names that as THE blocker for
// lightning: "The missing work is chiefly the procedural position evaluator
// and end-to-end Forge integration, not some huge new lightning ASIC organ."
//
// The law, verbatim from that document:
//
//     P0 = start
//     PN = end
//     Pi = lerp(start, end, i/N)
//        + perpendicular_1 x jitter(seed,   tick_phase, i)
//        + perpendicular_2 x jitter(seed^2, tick_phase, i)
//
// and the bound: "Draw one deterministic ribbon with at most 24 segments and
// at most two bounded branches" — refused, never clamped, when exceeded.
//
// ---------------------------------------------------------------------------
// THE SEAM WITH zhao_forge_prim (the order IS the contract)
// ---------------------------------------------------------------------------
// The topology walker's ribbon references vertices in ring-major order,
// vidx(s, k, ring=2) = 2s + k. This block emits positions in EXACTLY that
// order, one polyline at a time: main, branch 0, branch 1; within a polyline
// points i = 0..N; per point the pair (P - wvec) then (P + wvec). A job here
// pairs with one prim ribbon job per polyline (segments = N, sides = 1).
// Two orderings of the same bolt would produce the same picture and different
// capture CRCs, and the capture is the contract.
//
// ---------------------------------------------------------------------------
// DETERMINISM — the whole point
// ---------------------------------------------------------------------------
// Same params, same seed, same tick_phase => byte-identical positions, every
// run, under every stall pattern. Three structural guarantees:
//   1. every jitter value comes from a frozen table (JITTER_Q16, one M10K)
//      indexed by an xorshift32 stream that advances ONCE PER POINT in the
//      walk FSM — never per clock, so backpressure cannot reach it;
//   2. the lerp is an EXACT RATIONAL, floor((2*D*i + N)/(2*N)) — round-half-
//      up of D*i/N with no accumulated error, so P0 == start and PN == end
//      hold by arithmetic (endpoints are additionally emitted by assignment);
//   3. every rounding is the qformats §4 rescale (round-half-up) and every
//      overflow saturates and is counted — no wrap, no silent domain edge.
// The scalar law is zref::forge::eval_job (reference/include/zref/
// zref_forge_eval.hpp); the directed test holds RTL == oracle bit for bit.
//
// ---------------------------------------------------------------------------
// ARITHMETIC SHAPE — the ALM/DSP rescue applies
// ---------------------------------------------------------------------------
// ONE nonconstant multiplier, operand-muxed across every product this block
// ever needs (the zhao_terrain_normals mseq pattern), with a registered
// product. ONE bit-serial restoring divider (40 cycles, zero DSP) for the
// exact lerp. Jitter values live in one M10K instead of being computed.
// A worst-case bolt (24 + 8 + 8 segments) walks in ~6,300 cycles — 0.38% of
// computeClocksPerFrame = 1,666,666 — so sequencing costs nothing that
// matters and parallel multipliers would buy nothing but DSPs.
//
// Refusal taxonomy mirrors zhao_forge_prim: illegal caps REFUSED with a
// counter and nothing emitted; a job outside the view SKIPPED, not refused.
// Degenerate anchors (start == end) are LEGAL: D = 0 is a working domain
// point (a zero-length main bolt with live branches is a meaningful effect),
// unlike prim's zero-length REFUSAL which guards zero-area triangles.
`default_nettype none

module zhao_forge_prim_eval_mutant #(
    // The owner's bounds. Named so the look can be retuned without touching
    // arithmetic; hard caps, not clamps — see the refusal logic.
    parameter int unsigned MAX_MAIN_SEGMENTS   = 24,
    parameter int unsigned MAX_BRANCHES        = 2,
    parameter int unsigned MAX_BRANCH_SEGMENTS = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- job: the params block ---------------------------------------------
    input  var logic         j_valid_i,
    output var logic         j_ready_o,
    input  var logic signed [31:0] j_start_x_i,   // fx16 anchors
    input  var logic signed [31:0] j_start_y_i,
    input  var logic signed [31:0] j_start_z_i,
    input  var logic signed [31:0] j_end_x_i,
    input  var logic signed [31:0] j_end_y_i,
    input  var logic signed [31:0] j_end_z_i,
    input  var logic signed [31:0] j_perp1_x_i,   // jitter axes, caller-normalised
    input  var logic signed [31:0] j_perp1_y_i,
    input  var logic signed [31:0] j_perp1_z_i,
    input  var logic signed [31:0] j_perp2_x_i,
    input  var logic signed [31:0] j_perp2_y_i,
    input  var logic signed [31:0] j_perp2_z_i,
    input  var logic signed [31:0] j_waxis_x_i,   // ribbon width axis
    input  var logic signed [31:0] j_waxis_y_i,
    input  var logic signed [31:0] j_waxis_z_i,
    input  var logic signed [31:0] j_half_width_i,
    input  var logic signed [31:0] j_branch_half_width_i,
    input  var logic signed [31:0] j_amp_i,          // jitter amplitude, main
    input  var logic signed [31:0] j_branch_amp_i,   // jitter amplitude, branches
    input  var logic        [31:0] j_seed_i,
    input  var logic        [15:0] j_tick_phase_i,
    input  var logic        [6:0]  j_segments_i,       // 1..MAX_MAIN_SEGMENTS
    input  var logic        [1:0]  j_branch_count_i,   // 0..MAX_BRANCHES
    input  var logic        [6:0]  j_br0_attach_i,     // 0..j_segments_i
    input  var logic        [3:0]  j_br0_segments_i,   // 1..MAX_BRANCH_SEGMENTS
    input  var logic signed [31:0] j_br0_end_x_i,
    input  var logic signed [31:0] j_br0_end_y_i,
    input  var logic signed [31:0] j_br0_end_z_i,
    input  var logic        [6:0]  j_br1_attach_i,
    input  var logic        [3:0]  j_br1_segments_i,
    input  var logic signed [31:0] j_br1_end_x_i,
    input  var logic signed [31:0] j_br1_end_y_i,
    input  var logic signed [31:0] j_br1_end_z_i,
    input  var logic        [1:0]  j_view_mask_i,
    input  var logic        [15:0] j_src_id_i,
    input  var logic        [1:0]  view_sel_i,

    // ---- the vertex stream (positions, fx16) -------------------------------
    output var logic         v_valid_o,
    input  var logic         v_ready_i,
    output var logic signed [31:0] v_x_o,
    output var logic signed [31:0] v_y_o,
    output var logic signed [31:0] v_z_o,
    output var logic        [1:0]  v_poly_o,   // 0 main, 1 branch0, 2 branch1
    output var logic               v_last_o,   // final vertex of the whole job
    output var logic        [15:0] v_src_id_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] jobs_o,
    output var logic [31:0] points_o,
    output var logic [31:0] vertices_o,
    output var logic [31:0] refused_limit_o,
    output var logic [31:0] skipped_view_o,
    output var logic [31:0] sat_events_o,     // saturating component operations
    output var logic [31:0] walk_overrun_o    // point cursor past polyline end;
                                              // unreachable while the walk FSM is
                                              // correct — positive control is
                                              // tests/mutants/zhao_forge_prim_eval_mutant.sv
);

  // ---- the named constants of the jitter law (mirror zref_forge_eval.hpp) --
  localparam logic [31:0] HASH_GOLDEN = 32'h9E37_79B9;
  localparam logic [31:0] HASH_SALT_B = 32'h7F4A_7C15;
  localparam logic [31:0] SEED2_SALT  = 32'h85EB_CA6B;
  localparam logic [31:0] BR_SALT_0   = 32'h243F_6A88;
  localparam logic [31:0] BR_SALT_1   = 32'h4528_21E6;

  // Elaboration guards. Quartus 17.0 requires these INSIDE initial begin
  // (a bare module-scope `if` is a synthesis syntax error there), and
  // --lint-only does not run them — the Quartus gate does.
  initial begin
    if (MAX_MAIN_SEGMENTS < 1 || MAX_MAIN_SEGMENTS > 31)
      $fatal(1, "MAX_MAIN_SEGMENTS %0d outside 1..31 (2N must fit the 6-bit divisor)",
             MAX_MAIN_SEGMENTS);
    if (MAX_MAIN_SEGMENTS > 64)
      $fatal(1, "MAX_MAIN_SEGMENTS %0d exceeds zhao_forge_prim's frozen 64", MAX_MAIN_SEGMENTS);
    if (MAX_BRANCHES > 2)
      $fatal(1, "MAX_BRANCHES %0d exceeds the two attach-capture registers", MAX_BRANCHES);
    if (MAX_BRANCH_SEGMENTS < 1 || MAX_BRANCH_SEGMENTS > MAX_MAIN_SEGMENTS)
      $fatal(1, "MAX_BRANCH_SEGMENTS %0d outside 1..MAX_MAIN_SEGMENTS", MAX_BRANCH_SEGMENTS);
  end

  // ---- helpers -------------------------------------------------------------
  // qformats §4 rescale by 16 (round-half-up) then saturate to s32.
  // Returns {sat_flag, value}. Input is the 66-bit fused product sum.
  function automatic logic [32:0] rs16sat(input logic signed [65:0] x);
    logic signed [65:0] r;
    begin
      r = (x + 66'sd32768) >>> 16;
      if (r > 66'sd2147483647) rs16sat = {1'b1, 32'h7FFF_FFFF};
      else if (r < -66'sd2147483648) rs16sat = {1'b1, 32'h8000_0000};
      else rs16sat = {1'b0, r[31:0]};
    end
  endfunction

  // Saturate a 43-bit sum to s32. Returns {sat_flag, value}.
  function automatic logic [32:0] satw(input logic signed [42:0] x);
    begin
      if (x > 43'sd2147483647) satw = {1'b1, 32'h7FFF_FFFF};
      else if (x < -43'sd2147483648) satw = {1'b1, 32'h8000_0000};
      else satw = {1'b0, x[31:0]};
    end
  endfunction

  function automatic logic [31:0] xs32(input logic [31:0] h);
    logic [31:0] a, b;
    begin
      a = h ^ (h << 13);
      b = a ^ (a >> 17);
      xs32 = b ^ (b << 5);
    end
  endfunction

  function automatic logic [31:0] hash_init(input logic [31:0] base,
                                            input logic [15:0] tick,
                                            input logic [31:0] salt);
    logic [31:0] h;
    begin
      h = base ^ {tick, 16'h0} ^ salt;
      hash_init = (h == 32'h0) ? HASH_GOLDEN : h;  // xorshift32 fixed point at 0
    end
  endfunction

  // ---- job validation ------------------------------------------------------
  // Caps are REFUSED, never clamped. Inactive branch fields are NOT inspected
  // (a caller may leave them stale). Mirrors zref::forge::eval_verdict.
  logic limit_bad_c, for_view_c;
  always_comb begin
    limit_bad_c = (j_segments_i == 7'd0) || (j_segments_i > 7'(MAX_MAIN_SEGMENTS)) ||
                  (j_branch_count_i > 2'(MAX_BRANCHES));
    if (j_branch_count_i >= 2'd1)
      limit_bad_c = limit_bad_c || (j_br0_segments_i == 4'd0) ||
                    (j_br0_segments_i > 4'(MAX_BRANCH_SEGMENTS)) ||
                    (j_br0_attach_i > j_segments_i);
    if (j_branch_count_i >= 2'd2)
      limit_bad_c = limit_bad_c || (j_br1_segments_i == 4'd0) ||
                    (j_br1_segments_i > 4'(MAX_BRANCH_SEGMENTS)) ||
                    (j_br1_attach_i > j_segments_i);
  end
  assign for_view_c = (j_view_mask_i & view_sel_i) != 2'd0;

  // ---- state ---------------------------------------------------------------
  localparam logic [4:0] S_IDLE   = 5'd0;
  localparam logic [4:0] S_SEED_I = 5'd1;   // issue seed*seed
  localparam logic [4:0] S_SEED_C = 5'd2;   // capture seed2; init main hashes
  localparam logic [4:0] S_WV_I   = 5'd3;   // issue half_width * waxis[c]
  localparam logic [4:0] S_WV_C   = 5'd4;   // capture wvec[c]
  localparam logic [4:0] S_PT_TOP = 5'd5;   // dispatch one point
  localparam logic [4:0] S_ADV    = 5'd6;   // advance hashes + lerp accumulators
  localparam logic [4:0] S_TRD    = 5'd7;   // jitter table read (sync)
  localparam logic [4:0] S_TCAP   = 5'd8;   // capture table values
  localparam logic [4:0] S_JA_I   = 5'd9;   // issue amp * TA
  localparam logic [4:0] S_JA_C   = 5'd10;
  localparam logic [4:0] S_JB_I   = 5'd11;  // issue amp * TB
  localparam logic [4:0] S_JB_C   = 5'd12;
  localparam logic [4:0] S_D1_I   = 5'd13;  // issue perp1[c] * jA
  localparam logic [4:0] S_D2_I   = 5'd14;  // issue perp2[c] * jB, hold product 1
  localparam logic [4:0] S_D_C    = 5'd15;  // disp[c] = rescale(p1*jA + p2*jB)
  localparam logic [4:0] S_DV_LD  = 5'd16;  // load divider with acc[c]
  localparam logic [4:0] S_DV_RUN = 5'd17;  // 40 restoring iterations
  localparam logic [4:0] S_DV_FIN = 5'd18;  // floor adjust, store q[c]
  localparam logic [4:0] S_PSUM   = 5'd19;  // P = sat(S + q + disp); attach capture
  localparam logic [4:0] S_PEND   = 5'd20;  // endpoint: P = anchor, exactly
  localparam logic [4:0] S_E0     = 5'd21;  // emit P - wvec
  localparam logic [4:0] S_E1     = 5'd22;  // emit P + wvec
  localparam logic [4:0] S_BR_LD  = 5'd23;  // load branch polyline

  logic [4:0] st_q;

  // latched params (the start anchor lives in cs_q — the main polyline IS the job's)
  logic signed [31:0] p_p1_q [3];                      // perp1 x/y/z
  logic signed [31:0] p_p2_q [3];
  logic signed [31:0] p_wax_q [3];                     // width axis
  logic signed [31:0] p_bhw_q, p_bamp_q;
  logic        [31:0] p_seed_q;
  logic        [15:0] p_tick_q, p_src_q;
  logic        [1:0]  p_brcnt_q;
  logic        [6:0]  p_at_q  [2];
  logic        [3:0]  p_bn_q  [2];
  logic signed [31:0] p_bre_q [2][3];                  // branch end anchors

  // current polyline
  logic signed [31:0] cs_q [3];   // start
  logic signed [31:0] ce_q [3];   // end
  logic signed [32:0] cd_q [3];   // delta, s33
  logic signed [31:0] wv_q [3];   // width vector
  logic signed [31:0] amp_q;      // jitter amplitude for this polyline
  logic signed [31:0] hw_q;       // half width for this polyline
  logic        [6:0]  poly_n_q;   // segment count
  logic        [1:0]  poly_q;     // 0 main, 1 br0, 2 br1
  logic        [6:0]  pt_q;       // point cursor
  logic        [31:0] hA_q, hB_q; // jitter streams
  logic        [31:0] seed2_q;
  logic signed [40:0] acc_q [3];  // 2*D*i + N, exact

  // per-point scratch
  logic signed [17:0] ta_q, tb_q;
  logic signed [31:0] jA_q, jB_q;
  logic signed [65:0] dsum_q;
  logic signed [31:0] disp_q [3];
  logic signed [40:0] q_q [3];    // lerp offsets (fit s34; sized for the divider)
  logic signed [31:0] pP_q [3];   // the centre point
  logic signed [31:0] cap_q [2][3];  // captured attach centres
  logic        [1:0]  ccur_q;     // component cursor

  // divider
  logic        [39:0] dv_num_q;
  logic        [5:0]  dv_den_q;   // 2*N, <= 48
  logic        [5:0]  dv_rem_q;
  logic        [39:0] dv_quo_q;
  logic        [5:0]  dv_cnt_q;
  logic               dv_neg_q;

  assign j_ready_o = (st_q == S_IDLE);

  // ---- THE one nonconstant multiplier (mseq pattern, terrain_normals) ------
  logic signed [32:0] m_a, m_b;
  always_comb begin
    unique case (st_q)
      S_SEED_I: begin m_a = {1'b0, p_seed_q};      m_b = {1'b0, p_seed_q}; end
      S_WV_I:   begin m_a = {hw_q[31], hw_q};      m_b = {p_wax_q[ccur_q][31], p_wax_q[ccur_q]}; end
      S_JA_I:   begin m_a = {amp_q[31], amp_q};    m_b = {{15{ta_q[17]}}, ta_q}; end
      S_JB_I:   begin m_a = {amp_q[31], amp_q};    m_b = {{15{tb_q[17]}}, tb_q}; end
      S_D1_I:   begin m_a = {p_p1_q[ccur_q][31], p_p1_q[ccur_q]}; m_b = {jA_q[31], jA_q}; end
      S_D2_I:   begin m_a = {p_p2_q[ccur_q][31], p_p2_q[ccur_q]}; m_b = {jB_q[31], jB_q}; end
      default:  begin m_a = 33'sd0;                m_b = 33'sd0; end
    endcase
  end
  wire signed [65:0] m_p = m_a * m_b;
  logic signed [65:0] m_p_q;   // registered product — the DSP output register

  // ---- the jitter table: one M10K, dual read ------------------------------
  logic [17:0] jt_a_c, jt_b_c;
  zhao_forge_jitter_rom u_jtab (
      .clk    (clk),
      .idx_a_i(hA_q[7:0]),
      .idx_b_i(hB_q[7:0]),
      .val_a_o(jt_a_c),
      .val_b_o(jt_b_c)
  );

  // ---- emission ------------------------------------------------------------
  // Combinational from registers that are stable for the whole E state, so a
  // stall of any length presents the identical vertex.
  logic [32:0] em_x_c, em_y_c, em_z_c;
  always_comb begin
    if (st_q == S_E0) begin
      em_x_c = satw({{11{pP_q[0][31]}}, pP_q[0]} - {{11{wv_q[0][31]}}, wv_q[0]});
      em_y_c = satw({{11{pP_q[1][31]}}, pP_q[1]} - {{11{wv_q[1][31]}}, wv_q[1]});
      em_z_c = satw({{11{pP_q[2][31]}}, pP_q[2]} - {{11{wv_q[2][31]}}, wv_q[2]});
    end else begin
      em_x_c = satw({{11{pP_q[0][31]}}, pP_q[0]} + {{11{wv_q[0][31]}}, wv_q[0]});
      em_y_c = satw({{11{pP_q[1][31]}}, pP_q[1]} + {{11{wv_q[1][31]}}, wv_q[1]});
      em_z_c = satw({{11{pP_q[2][31]}}, pP_q[2]} + {{11{wv_q[2][31]}}, wv_q[2]});
    end
  end

  logic last_poly_c;
  assign last_poly_c = (poly_q == 2'd0 && p_brcnt_q == 2'd0) ||
                       (poly_q == 2'd1 && p_brcnt_q == 2'd1) || (poly_q == 2'd2);

  assign v_valid_o  = (st_q == S_E0) || (st_q == S_E1);
  assign v_x_o      = em_x_c[31:0];
  assign v_y_o      = em_y_c[31:0];
  assign v_z_o      = em_z_c[31:0];
  assign v_poly_o   = poly_q;
  assign v_last_o   = (st_q == S_E1) && (pt_q == poly_n_q) && last_poly_c;
  assign v_src_id_o = p_src_q;

  // PSUM adders: S + q + disp, exact in 43 bits, one saturation.
  logic [32:0] ps_c [3];
  always_comb begin
    for (int k = 0; k < 3; k++) begin
      ps_c[k] = satw({{11{cs_q[k][31]}}, cs_q[k]} + {{2{q_q[k][40]}}, q_q[k]} +
                     {{11{disp_q[k][31]}}, disp_q[k]});
    end
  end

  // Shared rescale results — ONE rescale primitive, evaluated where the FSM
  // says the registered product (or fused product sum) is meaningful.
  wire [32:0] rsm_c = rs16sat(m_p_q);           // rescale of the last product
  wire [32:0] rsd_c = rs16sat(dsum_q + m_p_q);  // fused p1*jA + p2*jB

  // divider step
  logic [6:0]  dv_t_c;
  logic        dv_ge_c;
  assign dv_t_c  = {dv_rem_q, dv_num_q[39]};
  assign dv_ge_c = dv_t_c >= {1'b0, dv_den_q};

  // floor-adjusted signed quotient of the finished division
  logic signed [40:0] dv_qres_c;
  always_comb begin
    if (!dv_neg_q) dv_qres_c = $signed({1'b0, dv_quo_q});
    else if (dv_rem_q == 6'd0) dv_qres_c = -$signed({1'b0, dv_quo_q});
    else dv_qres_c = -($signed({1'b0, dv_quo_q}) + 41'sd1);
  end

  // sat-event count for the vertex currently presented
  logic [1:0] em_sat_c;
  assign em_sat_c = {1'b0, em_x_c[32]} + {1'b0, em_y_c[32]} + {1'b0, em_z_c[32]};

  logic [1:0] ps_sat_c;
  assign ps_sat_c = {1'b0, ps_c[0][32]} + {1'b0, ps_c[1][32]} + {1'b0, ps_c[2][32]};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q            <= S_IDLE;
      m_p_q           <= '0;
      jobs_o          <= '0;
      points_o        <= '0;
      vertices_o      <= '0;
      refused_limit_o <= '0;
      skipped_view_o  <= '0;
      sat_events_o    <= '0;
      walk_overrun_o  <= '0;
      pt_q            <= '0;
      poly_q          <= '0;
      poly_n_q        <= '0;
      ccur_q          <= '0;
      hA_q            <= '0;
      hB_q            <= '0;
      seed2_q         <= '0;
      dv_num_q        <= '0;
      dv_den_q        <= '0;
      dv_rem_q        <= '0;
      dv_quo_q        <= '0;
      dv_cnt_q        <= '0;
      dv_neg_q        <= 1'b0;
      dsum_q          <= '0;
      ta_q            <= '0;
      tb_q            <= '0;
      jA_q            <= '0;
      jB_q            <= '0;
      amp_q           <= '0;
      hw_q            <= '0;
      p_bhw_q         <= '0;
      p_bamp_q        <= '0;
      p_seed_q        <= '0;
      p_tick_q        <= '0;
      p_src_q         <= '0;
      p_brcnt_q       <= '0;
      for (int k = 0; k < 3; k++) begin
        p_p1_q[k]  <= '0;
        p_p2_q[k]  <= '0;
        p_wax_q[k] <= '0;
        cs_q[k]    <= '0;
        ce_q[k]    <= '0;
        cd_q[k]    <= '0;
        wv_q[k]    <= '0;
        acc_q[k]   <= '0;
        disp_q[k]  <= '0;
        q_q[k]     <= '0;
        pP_q[k]    <= '0;
      end
      for (int b = 0; b < 2; b++) begin
        p_at_q[b] <= '0;
        p_bn_q[b] <= '0;
        for (int k = 0; k < 3; k++) begin
          p_bre_q[b][k] <= '0;
          cap_q[b][k]   <= '0;
        end
      end
    end else begin
      m_p_q <= m_p;  // unconditional; the FSM decides when it means something

      // The guard the mutant fires: the point cursor must never pass the
      // polyline end while the walk is live. Unreachable while the advance
      // logic is correct — which is exactly why its positive control is a
      // committed mutant, not a stimulus.
      if (st_q != S_IDLE && pt_q > poly_n_q) walk_overrun_o <= walk_overrun_o + 32'd1;

      unique case (st_q)
        S_IDLE: begin
          if (j_valid_i) begin
            jobs_o <= jobs_o + 32'd1;
            if (limit_bad_c) begin
              refused_limit_o <= refused_limit_o + 32'd1;
            end else if (!for_view_c) begin
              skipped_view_o <= skipped_view_o + 32'd1;
            end else begin
              // latch the params block whole
              p_p1_q[0]    <= j_perp1_x_i;
              p_p1_q[1]    <= j_perp1_y_i;
              p_p1_q[2]    <= j_perp1_z_i;
              p_p2_q[0]    <= j_perp2_x_i;
              p_p2_q[1]    <= j_perp2_y_i;
              p_p2_q[2]    <= j_perp2_z_i;
              p_wax_q[0]   <= j_waxis_x_i;
              p_wax_q[1]   <= j_waxis_y_i;
              p_wax_q[2]   <= j_waxis_z_i;
              p_bhw_q      <= j_branch_half_width_i;
              p_bamp_q     <= j_branch_amp_i;
              p_seed_q     <= j_seed_i;
              p_tick_q     <= j_tick_phase_i;
              p_src_q      <= j_src_id_i;
              p_brcnt_q    <= j_branch_count_i;
              p_at_q[0]    <= j_br0_attach_i;
              p_at_q[1]    <= j_br1_attach_i;
              p_bn_q[0]    <= j_br0_segments_i;
              p_bn_q[1]    <= j_br1_segments_i;
              p_bre_q[0][0] <= j_br0_end_x_i;
              p_bre_q[0][1] <= j_br0_end_y_i;
              p_bre_q[0][2] <= j_br0_end_z_i;
              p_bre_q[1][0] <= j_br1_end_x_i;
              p_bre_q[1][1] <= j_br1_end_y_i;
              p_bre_q[1][2] <= j_br1_end_z_i;
              // main polyline
              cs_q[0]  <= j_start_x_i;
              cs_q[1]  <= j_start_y_i;
              cs_q[2]  <= j_start_z_i;
              ce_q[0]  <= j_end_x_i;
              ce_q[1]  <= j_end_y_i;
              ce_q[2]  <= j_end_z_i;
              cd_q[0]  <= {j_end_x_i[31], j_end_x_i} - {j_start_x_i[31], j_start_x_i};
              cd_q[1]  <= {j_end_y_i[31], j_end_y_i} - {j_start_y_i[31], j_start_y_i};
              cd_q[2]  <= {j_end_z_i[31], j_end_z_i} - {j_start_z_i[31], j_start_z_i};
              acc_q[0] <= {34'd0, j_segments_i};  // 2*D*0 + N
              acc_q[1] <= {34'd0, j_segments_i};
              acc_q[2] <= {34'd0, j_segments_i};
              dv_den_q <= {j_segments_i[4:0], 1'b0};            // 2*N
              poly_n_q <= j_segments_i;
              amp_q    <= j_amp_i;
              hw_q     <= j_half_width_i;
              poly_q   <= 2'd0;
              pt_q     <= 7'd0;
              st_q     <= S_SEED_I;
            end
          end
        end

        // ---- job setup -----------------------------------------------------
        S_SEED_I: st_q <= S_SEED_C;
        S_SEED_C: begin
          seed2_q <= m_p_q[31:0] ^ SEED2_SALT;
          hA_q    <= hash_init(p_seed_q, p_tick_q, HASH_GOLDEN);
          hB_q    <= hash_init(m_p_q[31:0] ^ SEED2_SALT, p_tick_q, HASH_SALT_B);
          ccur_q  <= 2'd0;
          st_q    <= S_WV_I;
        end
        S_WV_I: st_q <= S_WV_C;
        S_WV_C: begin
          wv_q[ccur_q] <= rsm_c[31:0];
          sat_events_o <= sat_events_o + {31'd0, rsm_c[32]};
          if (ccur_q == 2'd2) begin
            ccur_q <= 2'd0;
            st_q   <= S_PT_TOP;
          end else begin
            ccur_q <= ccur_q + 2'd1;
            st_q   <= S_WV_I;
          end
        end

        // ---- one point -----------------------------------------------------
        S_PT_TOP: begin
          if (pt_q == 7'd0) st_q <= S_PEND;
          else st_q <= S_ADV;
        end
        S_ADV: begin
          // one advance per point — the stall-proof stream position
          hA_q <= xs32(hA_q);
          hB_q <= xs32(hB_q);
          for (int k = 0; k < 3; k++)
            acc_q[k] <= acc_q[k] + {{7{cd_q[k][32]}}, cd_q[k], 1'b0};  // += 2*D
          if (pt_q == poly_n_q) st_q <= S_PEND;
          else st_q <= S_TRD;
        end
        S_TRD:  st_q <= S_TCAP;   // ROM latches the (already advanced) indices
        S_TCAP: begin
          ta_q <= $signed(jt_a_c);
          tb_q <= $signed(jt_b_c);
          st_q <= S_JA_I;
        end
        S_JA_I: st_q <= S_JA_C;
        S_JA_C: begin
          jA_q         <= rsm_c[31:0];
          sat_events_o <= sat_events_o + {31'd0, rsm_c[32]};
          st_q         <= S_JB_I;
        end
        S_JB_I: st_q <= S_JB_C;
        S_JB_C: begin
          jB_q         <= rsm_c[31:0];
          sat_events_o <= sat_events_o + {31'd0, rsm_c[32]};
          ccur_q       <= 2'd0;
          st_q         <= S_D1_I;
        end
        S_D1_I: st_q <= S_D2_I;
        S_D2_I: begin
          dsum_q <= m_p_q;          // perp1[c] * jA
          st_q   <= S_D_C;
        end
        S_D_C: begin
          // Fused single rounding: exact 66-bit p1*jA + p2*jB, one rescale.
          disp_q[ccur_q] <= rsd_c[31:0];
          sat_events_o   <= sat_events_o + {31'd0, rsd_c[32]};
          if (ccur_q == 2'd2) begin
            ccur_q <= 2'd0;
            st_q   <= S_DV_LD;
          end else begin
            ccur_q <= ccur_q + 2'd1;
            st_q   <= S_D1_I;
          end
        end
        S_DV_LD: begin
          dv_neg_q <= acc_q[ccur_q][40];
          dv_num_q <= acc_q[ccur_q][40] ? 40'(-acc_q[ccur_q]) : 40'(acc_q[ccur_q]);
          dv_rem_q <= 6'd0;
          dv_quo_q <= '0;
          dv_cnt_q <= 6'd40;
          st_q     <= S_DV_RUN;
        end
        S_DV_RUN: begin
          dv_rem_q <= dv_ge_c ? 6'(dv_t_c - {1'b0, dv_den_q}) : dv_t_c[5:0];
          dv_quo_q <= {dv_quo_q[38:0], dv_ge_c};
          dv_num_q <= {dv_num_q[38:0], 1'b0};
          dv_cnt_q <= dv_cnt_q - 6'd1;
          if (dv_cnt_q == 6'd1) st_q <= S_DV_FIN;
        end
        S_DV_FIN: begin
          q_q[ccur_q] <= dv_qres_c;
          if (ccur_q == 2'd2) begin
            ccur_q <= 2'd0;
            st_q   <= S_PSUM;
          end else begin
            ccur_q <= ccur_q + 2'd1;
            st_q   <= S_DV_LD;
          end
        end
        S_PSUM: begin
          for (int k = 0; k < 3; k++) pP_q[k] <= ps_c[k][31:0];
          sat_events_o <= sat_events_o + {30'd0, ps_sat_c};
          points_o     <= points_o + 32'd1;
          if (poly_q == 2'd0) begin
            if ((p_brcnt_q >= 2'd1) && (p_at_q[0] == pt_q))
              for (int k = 0; k < 3; k++) cap_q[0][k] <= ps_c[k][31:0];
            if ((p_brcnt_q >= 2'd2) && (p_at_q[1] == pt_q))
              for (int k = 0; k < 3; k++) cap_q[1][k] <= ps_c[k][31:0];
          end
          st_q <= S_E0;
        end
        S_PEND: begin
          // Anchor-exact endpoints, by assignment — the law's P0/PN clause.
          for (int k = 0; k < 3; k++)
            pP_q[k] <= (pt_q == 7'd0) ? cs_q[k] : ce_q[k];
          points_o <= points_o + 32'd1;
          if (poly_q == 2'd0) begin
            if ((p_brcnt_q >= 2'd1) && (p_at_q[0] == pt_q))
              for (int k = 0; k < 3; k++) cap_q[0][k] <= (pt_q == 7'd0) ? cs_q[k] : ce_q[k];
            if ((p_brcnt_q >= 2'd2) && (p_at_q[1] == pt_q))
              for (int k = 0; k < 3; k++) cap_q[1][k] <= (pt_q == 7'd0) ? cs_q[k] : ce_q[k];
          end
          st_q <= S_E0;
        end
        S_E0: begin
          if (v_ready_i) begin
            vertices_o   <= vertices_o + 32'd1;
            sat_events_o <= sat_events_o + {30'd0, em_sat_c};
            st_q         <= S_E1;
          end
        end
        S_E1: begin
          if (v_ready_i) begin
            vertices_o   <= vertices_o + 32'd1;
            sat_events_o <= sat_events_o + {30'd0, em_sat_c};
            if (pt_q != poly_n_q + 7'd1) begin  // MUTANT: walks past the end
              pt_q <= pt_q + 7'd1;
              st_q <= S_PT_TOP;
            end else if (!last_poly_c) begin
              poly_q <= poly_q + 2'd1;
              st_q   <= S_BR_LD;
            end else begin
              st_q <= S_IDLE;
            end
          end
        end
        S_BR_LD: begin
          // poly_q already names the branch being loaded (1 or 2) — bit 1
          // separates them, which is what the 1-bit array index needs.
          for (int k = 0; k < 3; k++) begin
            cs_q[k]  <= cap_q[poly_q[1]][k];
            ce_q[k]  <= p_bre_q[poly_q[1]][k];
            cd_q[k]  <= {p_bre_q[poly_q[1]][k][31], p_bre_q[poly_q[1]][k]} -
                        {cap_q[poly_q[1]][k][31], cap_q[poly_q[1]][k]};
            acc_q[k] <= {37'd0, p_bn_q[poly_q[1]]};
          end
          poly_n_q <= {3'd0, p_bn_q[poly_q[1]]};
          dv_den_q <= {1'b0, p_bn_q[poly_q[1]], 1'b0};       // 2*N_branch <= 16
          amp_q    <= p_bamp_q;
          hw_q     <= p_bhw_q;
          hA_q     <= hash_init(p_seed_q ^ (poly_q[1] ? BR_SALT_1 : BR_SALT_0),
                                p_tick_q, HASH_GOLDEN);
          hB_q     <= hash_init(seed2_q ^ (poly_q[1] ? BR_SALT_1 : BR_SALT_0),
                                p_tick_q, HASH_SALT_B);
          pt_q     <= 7'd0;
          ccur_q   <= 2'd0;
          st_q     <= S_WV_I;   // branch width vector, then walk
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

  // synthesis translate_off
  // Simulation-only invariants. NOTE (CLAUDE.md, 2026-09-09): Verilator runs
  // this block — that is the point; Quartus skips it. The mutant copy disables
  // the overrun assertion, with the reason beside it. rst_n is deliberately
  // read synchronously here — it only gates checks, it resets nothing.
  /* verilator lint_off SYNCASYNCNET */
  always_ff @(posedge clk) begin
    if (rst_n && st_q != S_IDLE) begin
      // MUTANT: overrun $fatal disabled so the synthesizable counter can be
      // read. The assertion firing in the REAL block is separate corroboration.
      // if (pt_q > poly_n_q) $fatal(...);
    end
    if (rst_n && st_q == S_PSUM) begin
      for (int k = 0; k < 3; k++) begin
        // The lerp base S + q is a convex combination of two legal fx16
        // values rounded half-up: it cannot leave [min(S,E), max(S,E)].
        automatic logic signed [42:0] base_v;
        base_v = {{11{cs_q[k][31]}}, cs_q[k]} + {{2{q_q[k][40]}}, q_q[k]};
        // MUTANT: bound check disabled — past the end the lerp base may
        // legitimately leave [min(S,E), max(S,E)]; satw still saturates it.
        if (base_v != base_v) $fatal(1, "unreachable %0d", k);
      end
    end
  end
  /* verilator lint_on SYNCASYNCNET */
  // synthesis translate_on

endmodule : zhao_forge_prim_eval_mutant

`default_nettype wire
