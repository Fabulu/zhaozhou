// zhao_forge_ring_eval.sv -- POSITIONS for the four forge families that had none.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS: OWNER DECISION R234 D2, 2026-09-21, `(owner, explicit)`
// ---------------------------------------------------------------------------
// R199 deferred the forge program page kind and said exactly why:
//
//   > four of the six forge families have no evaluator at all.
//   > `zhao_forge_prim_eval` is the LIGHTNING evaluator, RIBBON family only.
//   > A page ruling buys one of six.
//
// R234 D2 reverses it: *"the owner has chosen to pay for the evaluators rather
// than accept the deferral."* This block is that payment. `spec/commands.zidl`
// names the four it owes -- *"Four of the six families have NO EVALUATOR (fan,
// tube, shell, billboard)"* -- and they are exactly the four served here.
//
// Reference: `zref::forge_ring::eval_job`
// (reference/include/zref/zref_forge_ring.hpp), differenced bit for bit in
// tests/forge/forge_ring_eval_directed.cpp.
//
// ---------------------------------------------------------------------------
// THE OTHER TWO FAMILIES ARE REFUSED, AND SEPARATELY
// ---------------------------------------------------------------------------
// RIBBON is `zhao_forge_prim_eval`'s -- the owner's lightning law with its
// jitter streams and branches. CLIFF is FORGE.CLIFF's, whose positions come
// from the terrain lattice and not from a parameter block at all. Re-deriving
// either here would be a second implementation of ratified arithmetic, which is
// the failure `CLAUDE.md` records for the terrain shade header and for the
// projector's two cores.
//
// So a job naming one of those two is refused on `refused_elsewhere_o`, which
// is a DIFFERENT port from `refused_family_o`. Family 6 is a caller who has not
// read the ruling; family 0 is a DISPATCH that sent the job to the wrong
// evaluator. Two faults, two fixes, and one counter could not tell them apart
// (R95: a counter must DISCRIMINATE, not merely move).
//
// ---------------------------------------------------------------------------
// THE LAW -- ONE SWEPT RING, WHICH IS WHY IT IS ONE BLOCK AND NOT FOUR
// ---------------------------------------------------------------------------
// `zhao_forge_prim` walks a (rings x ring-vertices) GRID for every family, and
// that is the property that makes it ONE topology generator. The positions have
// the same shape:
//
//     P(s,k) = C(s) + R(s) * W(k)        s = 0..N rings, k = 0..K-1 per ring
//
//     FAN        N = 1,        K = sides   closed    hub ring -> rim ring
//     TUBE       N = segments, K = sides   closed    a swept ring
//     SHELL      N = segments, K = sides   closed    a cone, or a DOME
//     BILLBOARD  N = 1,        K = 2       open      one quad
//
// N and K are `zhao_forge_prim`'s own `eff_seg_c` and `ring_q`, RESTATED -- not
// a second topology law. Compare `zhao_forge_prim.sv`'s `eff_seg_c` case and
// `ring_q <= closed_c ? eff_side_c : eff_side_c + 1` against `n_c` and `k_c`
// below; they are the same two expressions and they must stay so.
//
// An OPEN family's "ring" is the width-axis PAIR, `C - R*U` then `C + R*U` --
// EXACTLY the pair `zhao_forge_prim_eval` emits for the ribbon, in exactly that
// order. The two evaluators therefore agree about what an open ring is without
// either one importing the other, and that agreement is asserted in the test.
//
// THE TWO SWEEPS
//     LINEAR  C(s) = A0 + rhu((A1-A0) * s/N)     R(s) = r0 + rhu((r1-r0) * s/N)
//     DOME    phi  = rhu(0x4000 * s/N)           a QUARTER turn, 0 -> pole
//             C(s) = A0 + rescale16((A1-A0)*sin phi)
//             R(s) = rescale16(Rlin(s) * cos phi)
//
// DOME meets LINEAR exactly at both ends, and by arithmetic rather than by
// approximation: `sin 0 = 0`, `cos 0 = 0x10000` EXACTLY (the table is seventeen
// bits wide precisely so that it is), and `sin(0x4000) = 0x10000`. So C(0)=A0,
// C(N)=A1 and R(0)=r0 hold to the bit, which the directed test asserts.
//
// DOME is legal on the SHELL alone (`spec/cartridge.md` 4d). Allowing it
// elsewhere would make `sweep` a second, silent family selector.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC IS BORROWED, NOT INVENTED
// ---------------------------------------------------------------------------
// 1. **The lerp is FORGE.PRIM.EVAL's exact rational**, `floor((2*D*i + N)/(2N))`
//    by one bit-serial restoring divide -- ZERO DSP, and `off(0) = 0` and
//    `off(N) = D` are arithmetic facts rather than special cases. The numerator
//    is maintained by exact integer accumulation (`+= 2D` per ring), so the
//    divider is entered once per quantity per ring and never per vertex.
// 2. **The ring angle is that SAME function**: `theta(k) = rhu(0x10000 * k/K)`.
//    A ring of 3, 5, 6 or 7 sides is therefore uniform to the ulp instead of
//    accumulating a stepped error, and because a whole turn is the WIDTH of
//    angle16, a closed ring closes by INDEX and never by angle -- there is no
//    wrap to get wrong.
// 3. **Sine and cosine are `zhao_field_sin`**, the ratified spec/qformats.md
//    7.1 quarter-wave law, INSTANTIATED rather than re-implemented.
//    FORGE.PRIM.md requires exactly this: *"the same generated table the rest
//    of the tree uses, so a ring here and a rotation elsewhere agree exactly.
//    No new trigonometry is introduced."* `zhao_geom_loom` and `zhao_light_env`
//    hold their own instances for the same reason; the table is small and a
//    second LAW would be the expensive thing, not a second copy of a ROM.
// 4. **All products go through ONE operand-muxed 33x33 multiplier** with a
//    registered product -- the `zhao_terrain_normals` mseq pattern that
//    `zhao_forge_prim_eval` also follows. There is no second multiplier site.
// 5. **Every rounding is the qformats 4 rescale** (round-half-up) and the ring
//    displacement is FUSED: one exact 66-bit sum of two products, ONE rounding.
//    That is `zhao_forge_prim_eval`'s `disp_c` structure, restated.
// 6. **Every overflow SATURATES and is counted.** Nothing wraps.
//
// ---------------------------------------------------------------------------
// THERE IS NO `walk_overrun` COUNTER HERE, AND THAT IS A DECISION
// ---------------------------------------------------------------------------
// `zhao_forge_prim_eval` carries one, and it needs a COMMITTED MUTANT to be
// seen to fire because no legal stimulus can reach it. `CLAUDE.md` is right
// that a guard unreachable by legal stimulus needs that mutant -- and it is
// also right that *"a detector that has not been shown to FIRE has not been
// tested"*. Rather than ship a second unfireable counter, the whole-job
// property is asserted DIRECTLY and positively by the bench: an accepted job
// emits EXACTLY `(N+1) * K` vertices and a refused one emits ZERO, under every
// stall pattern. That is the correct behaviour rather than the bug (CLAUDE.md's
// rule 4), and it is checked on every vector rather than once.
//
// **Every counter this block does export is fired by legal stimulus** in
// `forge_ring_eval_directed`, saturation included. None of them is asserted
// zero and left unexercised.
//
// ---------------------------------------------------------------------------
// LATENCY, PRICED HONESTLY (owner ruling R236: a cost is a FACT, not a veto)
// ---------------------------------------------------------------------------
// The divider dominates and is entered a bounded number of times:
//
//     ring angles     K divides                       <= 8
//     per ring        3 centre + 1 radius (+1 dome)   <= 5 x (N+1) <= 325
//
// at 43 iterations each, plus ~4 clocks per trig pair and 2 per product. The
// worst legal job -- a 64-segment, 8-sided DOME shell, 520 vertices -- is about
// **16,000 clocks, or 0.96% of `computeClocksPerFrame` (1,666,666)**. Sixteen
// of them would be 15%, which is the number to watch and is the same number
// FORGE.PRIM.md flags for its own worst case.
//
// **The lever, if that ever binds, is named rather than taken:** the divides
// exist only because `segments` and `sides` are not restricted to powers of
// two, and a per-ring reciprocal would trade exactness for rate. Exactness is
// what makes the capture CRC a contract, so it is not traded here.
`default_nettype none

module zhao_forge_ring_eval #(
    // FORGE.PRIM.md's frozen limits. Named so they can be retuned without
    // touching arithmetic; HARD CAPS, not clamps -- see the refusal logic.
    parameter int unsigned MAX_SEGMENTS = 64,
    parameter int unsigned MAX_SIDES    = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- job: the params block, one FORGE_PROGRAM record's geometry --------
    input  var logic         j_valid_i,
    output var logic         j_ready_o,
    input  var logic [2:0]   j_family_i,      // the SILICON encoding, FAM_*
    input  var logic         j_sweep_i,       // 0 LINEAR, 1 DOME
    input  var logic [6:0]   j_segments_i,    // 1..MAX_SEGMENTS
    input  var logic [3:0]   j_sides_i,       // 1..MAX_SIDES
    input  var logic signed [31:0] j_a0_x_i,  // fx16, ring 0's centre
    input  var logic signed [31:0] j_a0_y_i,
    input  var logic signed [31:0] j_a0_z_i,
    input  var logic signed [31:0] j_a1_x_i,  // fx16, ring N's centre
    input  var logic signed [31:0] j_a1_y_i,
    input  var logic signed [31:0] j_a1_z_i,
    input  var logic signed [31:0] j_u_x_i,   // fx16 ring U axis, caller-normalised
    input  var logic signed [31:0] j_u_y_i,
    input  var logic signed [31:0] j_u_z_i,
    input  var logic signed [31:0] j_v_x_i,   // fx16 ring V axis, caller-normalised
    input  var logic signed [31:0] j_v_y_i,
    input  var logic signed [31:0] j_v_z_i,
    input  var logic signed [31:0] j_r0_i,    // fx16 radius at ring 0
    input  var logic signed [31:0] j_r1_i,    // fx16 radius at ring N
    input  var logic [1:0]   j_view_mask_i,
    input  var logic [15:0]  j_src_id_i,
    input  var logic [1:0]   view_sel_i,

    // ---- the vertex stream (positions, fx16), ring-major -------------------
    output var logic         v_valid_o,
    input  var logic         v_ready_i,
    output var logic signed [31:0] v_x_o,
    output var logic signed [31:0] v_y_o,
    output var logic signed [31:0] v_z_o,
    output var logic [6:0]   v_ring_o,    // s, 0..N -- the topology walker's segment
    output var logic [4:0]   v_k_o,       // k, 0..K-1 -- its position around the ring
    output var logic         v_last_o,    // the final vertex of the whole job
    output var logic [15:0]  v_src_id_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] jobs_o,
    output var logic [31:0] rings_o,
    output var logic [31:0] vertices_o,
    output var logic [31:0] refused_family_o,     // family outside 0..5
    output var logic [31:0] refused_elsewhere_o,  // ribbon or cliff: another block's
    output var logic [31:0] refused_limit_o,      // segments or sides out of range
    output var logic [31:0] skipped_view_o,
    output var logic [31:0] sat_events_o          // saturating component operations
);

  // ---- the six families, `zhao_forge_prim.sv`'s encoding -------------------
  localparam logic [2:0] FAM_RIBBON    = 3'd0;
  localparam logic [2:0] FAM_FAN       = 3'd1;
  localparam logic [2:0] FAM_TUBE      = 3'd2;
  localparam logic [2:0] FAM_SHELL     = 3'd3;
  localparam logic [2:0] FAM_BILLBOARD = 3'd4;
  localparam logic [2:0] FAM_CLIFF     = 3'd5;

  localparam logic QUARTER_SWEEP = 1'b1;  // j_sweep_i == 1 is DOME

  // A whole turn is the WIDTH of angle16; a quarter is a quarter of it.
  // Written as plain integers on purpose: `16'h1_0000` is a SIXTEEN-bit literal
  // holding a seventeen-bit value, so it elaborates to ZERO and every ring
  // angle would come out 0 -- a perfectly uniform, perfectly wrong ring.
  localparam int unsigned WHOLE_TURN   = 65536;
  localparam int unsigned QUARTER_TURN = 16384;

  // Divider width. The largest numerator is `2*D*s + N` with |D| < 2^32 and
  // s <= 64, so |num| < 2^39 + 64. 43 bits carries it with headroom and the
  // iteration count is that width.
  localparam int unsigned DVW = 43;

  // Quartus 17 law: an elaboration check lives inside `initial begin ... end`.
  // A bare module-scope `if (...) $fatal(...)` lints clean in Verilator and is
  // a syntax error in quartus_map -- "syntax error near text: `if`".
  // `--lint-only` does not run initial blocks either, so neither tool proves
  // the other's half.
  initial begin
    if (MAX_SEGMENTS < 1 || MAX_SEGMENTS > 64)
      $fatal(1, "MAX_SEGMENTS %0d outside 1..64 (FORGE.PRIM.md's frozen limit)", MAX_SEGMENTS);
    if (MAX_SIDES < 1 || MAX_SIDES > 8)
      $fatal(1, "MAX_SIDES %0d outside 1..8 (FORGE.PRIM.md's frozen limit)", MAX_SIDES);
  end

  // ---- helpers -------------------------------------------------------------
  // qformats 4 rescale by 16 (round-half-up) then saturate to s32.
  // Returns {sat_flag, value}. `>>>` on a signed value is an arithmetic shift,
  // i.e. FLOOR, which is what makes the +32768 a round-half-UP and not a
  // round-half-away-from-zero.
  function automatic logic [32:0] rs16sat(input logic signed [66:0] x);
    logic signed [66:0] r;
    begin
      r = (x + 67'sd32768) >>> 16;
      if (r > 67'sd2147483647) rs16sat = {1'b1, 32'h7FFF_FFFF};
      else if (r < -67'sd2147483648) rs16sat = {1'b1, 32'h8000_0000};
      else rs16sat = {1'b0, r[31:0]};
    end
  endfunction

  // Saturate a wide exact sum to s32. Returns {sat_flag, value}.
  function automatic logic [32:0] satw(input logic signed [45:0] x);
    begin
      if (x > 46'sd2147483647) satw = {1'b1, 32'h7FFF_FFFF};
      else if (x < -46'sd2147483648) satw = {1'b1, 32'h8000_0000};
      else satw = {1'b0, x[31:0]};
    end
  endfunction

  // ---- acceptance ----------------------------------------------------------
  // `zhao_forge_prim`'s `eff_seg_c` and `ring_q`, restated. If either of those
  // two expressions ever moves, this must move with it or the positions stop
  // matching the indices.
  logic [6:0] n_c;   // N, the ring index bound
  logic [4:0] k_c;   // K, vertices per ring
  logic       closed_c;
  assign closed_c = (j_family_i == FAM_TUBE) || (j_family_i == FAM_SHELL) ||
                    (j_family_i == FAM_FAN);
  always_comb begin : eff_grid
    case (j_family_i)
      FAM_FAN:       n_c = 7'd1;
      FAM_BILLBOARD: n_c = 7'd1;
      default:       n_c = j_segments_i;
    endcase
    k_c = closed_c ? {1'b0, j_sides_i} : 5'd2;
  end

  logic family_bad_c, elsewhere_c, limit_bad_c, for_view_c;
  assign family_bad_c = (j_family_i > FAM_CLIFF);
  assign elsewhere_c  = (j_family_i == FAM_RIBBON) || (j_family_i == FAM_CLIFF);
  assign limit_bad_c  = (j_segments_i == 7'd0) || (j_sides_i == 4'd0) ||
                        (j_segments_i > 7'(MAX_SEGMENTS)) ||
                        (j_sides_i > 4'(MAX_SIDES));
  assign for_view_c   = (j_view_mask_i & view_sel_i) != 2'd0;

  // ---- latched job ---------------------------------------------------------
  logic [6:0]  p_n_q;
  logic [4:0]  p_k_q;
  logic        p_dome_q;
  logic [15:0] p_src_q;
  logic signed [31:0] p_a0_q [3];
  logic signed [32:0] p_dd_q [3];   // A1 - A0, exact in 33 bits
  logic signed [31:0] p_u_q  [3];
  logic signed [31:0] p_v_q  [3];
  logic signed [31:0] p_r0_q;
  logic signed [32:0] p_dr_q;       // r1 - r0

  // ---- the ring-direction table, filled once per job ----------------------
  // cos/sin of theta(k). fx16's range here is exactly +-65536, which needs 18
  // signed bits; the full 32 would be 448 flops spent on sign extension.
  logic signed [17:0] cu_q [8];
  logic signed [17:0] cv_q [8];

  // ---- per-ring state ------------------------------------------------------
  logic signed [31:0] C_q  [3];   // the ring centre
  logic signed [31:0] RU_q [3];   // R * U, folded once per ring
  logic signed [31:0] RV_q [3];   // R * V
  logic signed [31:0] R_q;
  logic signed [17:0] sn_q, cs_q;
  logic signed [43:0] acc_q [4];  // exact lerp numerators: 3 centres, 1 radius

  // ---- the walk ------------------------------------------------------------
  logic [6:0] seg_q;   // s
  logic [4:0] vk_q;    // k
  logic [1:0] ccur_q;  // which component, 0..2
  logic [2:0] mcur_q;  // which of the six R*axis products, 0..5
  logic [3:0] racur_q; // which ring-table entry, 0..7
  logic signed [31:0] pP_q [3];  // the vertex being presented

  // ---- shared bit-serial divider (ZERO DSP) -------------------------------
  logic [DVW-1:0] dv_num_q;
  logic [7:0]     dv_den_q;   // 2*N <= 128, or 2*K <= 16
  logic [7:0]     dv_rem_q;
  logic [DVW-1:0] dv_quo_q;
  logic [6:0]     dv_cnt_q;
  logic           dv_neg_q;

  logic [8:0] dv_t_c;
  logic       dv_ge_c;
  assign dv_t_c  = {dv_rem_q, dv_num_q[DVW-1]};
  assign dv_ge_c = dv_t_c >= {1'b0, dv_den_q};

  // The quotient, sign restored to FLOOR semantics. A negative numerator whose
  // division left a remainder rounds one further down -- which is what makes
  // `floor((2*D*i + N) / (2N))` the round-half-UP of D*i/N for both signs.
  logic signed [DVW:0] dv_qres_c;
  always_comb begin : divider_result
    if (!dv_neg_q) dv_qres_c = $signed({1'b0, dv_quo_q});
    else if (dv_rem_q == 8'd0) dv_qres_c = -$signed({1'b0, dv_quo_q});
    else dv_qres_c = -($signed({1'b0, dv_quo_q}) + {{DVW{1'b0}}, 1'b1});
  end

  // ---- THE one nonconstant multiplier (mseq pattern) ----------------------
  logic signed [32:0] m_a, m_b;
  logic signed [65:0] m_p_q;    // the registered product -- the DSP output register
  logic signed [66:0] fa_q;     // the fused accumulator, RU*cu before RV*cv joins it

  // ---- the shared sine/cosine, spec/qformats.md 7.1 -----------------------
  logic [15:0] tr_ang_q;
  logic [2:0]  tr_cnt_q;
  // `zhao_field_sin` returns a full s32, but spec/qformats.md 7.1 bounds it at
  // exactly +-0x10000 -- the table is seventeen bits wide precisely so that
  // sin(quarter turn) is exactly 1.0 and not one ulp below it. Bits 31:18 are
  // therefore pure sign extension and are dropped on the way into the 18-bit
  // ring table, which saves 448 flops. That is a CLAIM, so it is checked in
  // simulation below rather than merely asserted in a comment.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] tr_res_c;
  /* verilator lint_on UNUSEDSIGNAL */
  zhao_field_sin u_sin (
      .clk     (clk),
      .angle_i (tr_ang_q),
      // Present the angle for SIN on the first beat and for COS on every beat
      // after. `zhao_field_sin` is LATENCY 2, INITIATION INTERVAL 1, so the two
      // answers arrive on beats 2 and 3 and the request never stalls.
      .is_cos_i(tr_cnt_q != 3'd0),
      .result_o(tr_res_c)
  );

  // ---- states --------------------------------------------------------------
  typedef enum logic [4:0] {
    S_IDLE,
    S_DIV,       // the shared divider's iteration; returns to dv_ret_q
    S_TRIG,      // the shared sin/cos; returns to tr_ret_q
    S_RA_SEED,   // seed theta(k)'s divide
    S_RA_TRIG,   // theta(k) -> sin/cos
    S_RA_STORE,  // store cu[k], cv[k]
    S_RING,      // begin ring s
    S_DM_SEED,   // seed phi(s)'s divide
    S_DM_TRIG,
    S_DM_STORE,
    S_DM_MUL,    // DOME: issue dd[c] * sin phi
    S_DM_LAT,    // DOME: C[c] = satw(a0[c] + rescale)
    S_CC_SEED,   // LINEAR: seed the centre component's divide
    S_CC_LAT,    // LINEAR: C[c] = satw(a0[c] + off)
    S_RR_SEED,   // seed the radius divide
    S_RR_LAT,    // Rlin = satw(r0 + off)
    S_RR_MUL,    // DOME: issue Rlin * cos phi
    S_RR_LAT2,   // DOME: R = rescale
    S_RU_MUL,    // issue R * axis (six products)
    S_RU_LAT,
    S_W1,        // issue RU[a] * cu[k]
    S_W1L,       // capture it, issue RV[a] * cv[k]
    S_W2L,       // fuse, rescale ONCE, add to C[a]
    S_EMIT
  } state_e;
  state_e st_q, dv_ret_q, tr_ret_q;

  assign j_ready_o = (st_q == S_IDLE);

  // ---- the multiplier's operand mux ---------------------------------------
  always_comb begin : mul_operands
    m_a = 33'sd0;
    m_b = 33'sd0;
    case (st_q)
      S_DM_MUL: begin
        m_a = p_dd_q[ccur_q];
        m_b = {{15{sn_q[17]}}, sn_q};
      end
      S_RR_MUL: begin
        m_a = {R_q[31], R_q};                 // Rlin is staged in R_q
        m_b = {{15{cs_q[17]}}, cs_q};
      end
      S_RU_MUL: begin
        m_a = {R_q[31], R_q};
        m_b = ax_is_v_c ? {p_v_q[ax_idx_c][31], p_v_q[ax_idx_c]}
                        : {p_u_q[ax_idx_c][31], p_u_q[ax_idx_c]};
      end
      S_W1: begin
        m_a = {RU_q[ccur_q][31], RU_q[ccur_q]};
        m_b = {{15{cu_q[vk_q[2:0]][17]}}, cu_q[vk_q[2:0]]};
      end
      S_W1L: begin
        m_a = {RV_q[ccur_q][31], RV_q[ccur_q]};
        m_b = {{15{cv_q[vk_q[2:0]][17]}}, cv_q[vk_q[2:0]]};
      end
      default: begin
        m_a = 33'sd0;
        m_b = 33'sd0;
      end
    endcase
  end

  // ---- emission ------------------------------------------------------------
  // Driven from registers that are stable for the whole S_EMIT state, so a
  // stall of any length presents the IDENTICAL vertex. That is the property a
  // capture CRC rests on and it is asserted under four stall patterns.
  assign v_valid_o  = (st_q == S_EMIT);
  assign v_x_o      = pP_q[0];
  assign v_y_o      = pP_q[1];
  assign v_z_o      = pP_q[2];
  assign v_ring_o   = seg_q;
  assign v_k_o      = vk_q;
  assign v_last_o   = (st_q == S_EMIT) && (seg_q == p_n_q) && (vk_q + 5'd1 == p_k_q);
  assign v_src_id_o = p_src_q;

  // ---- combinational intermediates used by the sequencer ------------------
  // Every concatenation here is manually sign-extended to the EXACT width of
  // the function input it feeds, because a concatenation is UNSIGNED in
  // Verilog and a mixed-sign addition would silently zero-extend the negative
  // half. Two's-complement addition of two correctly extended patterns is the
  // same bits either way, which is why this is safe and why it has to be
  // written out rather than left to inference.
  wire signed [66:0] m_p_w = $signed({m_p_q[65], m_p_q});
  wire [32:0] rs_m_c = rs16sat(m_p_w);                   // rescale of the last product
  wire [32:0] rs_f_c = rs16sat($signed(fa_q) + m_p_w);   // FUSED: ONE rounding

  // The six R*axis products: U.x, U.y, U.z then V.x, V.y, V.z. The index is
  // computed once so no expression ever addresses `p_u_q[3]`, which does not
  // exist and which a ternary would evaluate anyway.
  logic       ax_is_v_c;
  logic [1:0] ax_idx_c;
  assign ax_is_v_c = (mcur_q >= 3'd3);
  assign ax_idx_c  = ax_is_v_c ? 2'(mcur_q - 3'd3) : mcur_q[1:0];

  // The four saturating adds, each at the exact width of its operands.
  wire [32:0] c_lin_c = satw($signed({{2{dv_qres_c[DVW]}}, dv_qres_c}) +
                             $signed({{14{p_a0_q[ccur_q][31]}}, p_a0_q[ccur_q]}));
  wire [32:0] c_dom_c = satw($signed({{14{rs_m_c[31]}}, rs_m_c[31:0]}) +
                             $signed({{14{p_a0_q[ccur_q][31]}}, p_a0_q[ccur_q]}));
  wire [32:0] r_lin_c = satw($signed({{2{dv_qres_c[DVW]}}, dv_qres_c}) +
                             $signed({{14{p_r0_q[31]}}, p_r0_q}));
  wire [32:0] p_cmp_c = satw($signed({{14{rs_f_c[31]}}, rs_f_c[31:0]}) +
                             $signed({{14{C_q[ccur_q][31]}}, C_q[ccur_q]}));

  // The exact lerp numerator, `2*D*s + N`, assembled at the divider's door.
  // `p_n_q` is widened through a zero concatenation and then read as signed so
  // that a NEGATIVE accumulator is not promoted to unsigned by the addition.
  wire signed [43:0] acc_n_c = $signed(acc_q[ccur_q]) + $signed({37'd0, p_n_q});
  wire signed [43:0] acr_n_c = $signed(acc_q[3])      + $signed({37'd0, p_n_q});

  // ---- the sequencer -------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin : sequencer
    integer i;
    if (!rst_n) begin
      st_q                <= S_IDLE;
      dv_ret_q            <= S_IDLE;
      tr_ret_q            <= S_IDLE;
      jobs_o              <= '0;
      rings_o             <= '0;
      vertices_o          <= '0;
      refused_family_o    <= '0;
      refused_elsewhere_o <= '0;
      refused_limit_o     <= '0;
      skipped_view_o      <= '0;
      sat_events_o        <= '0;
      seg_q               <= '0;
      vk_q                <= '0;
      ccur_q              <= '0;
      mcur_q              <= '0;
      racur_q             <= '0;
      dv_num_q            <= '0;
      dv_den_q            <= '0;
      dv_rem_q            <= '0;
      dv_quo_q            <= '0;
      dv_cnt_q            <= '0;
      dv_neg_q            <= 1'b0;
      tr_ang_q            <= '0;
      tr_cnt_q            <= '0;
      m_p_q               <= '0;
      fa_q                <= '0;
      sn_q                <= '0;
      cs_q                <= '0;
      R_q                 <= '0;
      p_n_q               <= '0;
      p_k_q               <= '0;
      p_dome_q            <= 1'b0;
      p_src_q             <= '0;
      p_r0_q              <= '0;
      p_dr_q              <= '0;
      for (i = 0; i < 3; i = i + 1) begin
        p_a0_q[i] <= '0;
        p_dd_q[i] <= '0;
        p_u_q[i]  <= '0;
        p_v_q[i]  <= '0;
        C_q[i]    <= '0;
        RU_q[i]   <= '0;
        RV_q[i]   <= '0;
        pP_q[i]   <= '0;
      end
      for (i = 0; i < 4; i = i + 1) acc_q[i] <= '0;
      for (i = 0; i < 8; i = i + 1) begin
        cu_q[i] <= '0;
        cv_q[i] <= '0;
      end
    end else begin
      // The product register runs unconditionally -- it is the DSP's own output
      // register and every state that reads it does so exactly one clock after
      // the state that drove the operands.
      m_p_q <= m_a * m_b;

      case (st_q)
        // -------------------------------------------------------------------
        S_IDLE: begin
          if (j_valid_i) begin
            jobs_o <= jobs_o + 32'd1;
            // The refusal ORDER is the reference's. Each cause is counted on
            // its OWN port so a dispatch fault and a caller fault cannot be
            // mistaken for one another.
            if (family_bad_c) begin
              refused_family_o <= refused_family_o + 32'd1;
            end else if (elsewhere_c) begin
              refused_elsewhere_o <= refused_elsewhere_o + 32'd1;
            end else if (limit_bad_c) begin
              refused_limit_o <= refused_limit_o + 32'd1;
            end else if (!for_view_c) begin
              skipped_view_o <= skipped_view_o + 32'd1;
            end else begin
              p_n_q      <= n_c;
              p_k_q      <= k_c;
              p_dome_q   <= (j_sweep_i == QUARTER_SWEEP);
              p_src_q    <= j_src_id_i;
              p_r0_q     <= j_r0_i;
              p_dr_q     <= {j_r1_i[31], j_r1_i} - {j_r0_i[31], j_r0_i};
              p_a0_q[0]  <= j_a0_x_i;
              p_a0_q[1]  <= j_a0_y_i;
              p_a0_q[2]  <= j_a0_z_i;
              p_dd_q[0]  <= {j_a1_x_i[31], j_a1_x_i} - {j_a0_x_i[31], j_a0_x_i};
              p_dd_q[1]  <= {j_a1_y_i[31], j_a1_y_i} - {j_a0_y_i[31], j_a0_y_i};
              p_dd_q[2]  <= {j_a1_z_i[31], j_a1_z_i} - {j_a0_z_i[31], j_a0_z_i};
              p_u_q[0]   <= j_u_x_i;
              p_u_q[1]   <= j_u_y_i;
              p_u_q[2]   <= j_u_z_i;
              p_v_q[0]   <= j_v_x_i;
              p_v_q[1]   <= j_v_y_i;
              p_v_q[2]   <= j_v_z_i;
              seg_q      <= 7'd0;
              vk_q       <= 5'd0;
              for (i = 0; i < 4; i = i + 1) acc_q[i] <= '0;

              if (closed_c) begin
                racur_q <= 4'd0;
                st_q    <= S_RA_SEED;
              end else begin
                // An OPEN ring is the width-axis PAIR and needs no table:
                // `C - R*U` then `C + R*U`, exactly the ribbon evaluator's
                // order. cos = -1 then +1, sin = 0, both exact.
                cu_q[0] <= -18'sd65536;
                cv_q[0] <= 18'sd0;
                cu_q[1] <= 18'sd65536;
                cv_q[1] <= 18'sd0;
                st_q    <= S_RING;
              end
            end
          end
        end

        // ---- the shared divider -------------------------------------------
        S_DIV: begin
          dv_rem_q <= dv_ge_c ? 8'(dv_t_c - {1'b0, dv_den_q}) : dv_t_c[7:0];
          dv_quo_q <= {dv_quo_q[DVW-2:0], dv_ge_c};
          dv_num_q <= {dv_num_q[DVW-2:0], 1'b0};
          dv_cnt_q <= dv_cnt_q - 7'd1;
          if (dv_cnt_q == 7'd1) st_q <= dv_ret_q;
        end

        // ---- the shared sine/cosine ---------------------------------------
        // beat 0 presents the angle for SIN, beat 1 for COS; the answers land
        // on beats 2 and 3 (latency 2, initiation interval 1).
        S_TRIG: begin
          tr_cnt_q <= tr_cnt_q + 3'd1;
          if (tr_cnt_q == 3'd2) sn_q <= 18'(tr_res_c);
          if (tr_cnt_q == 3'd3) begin
            cs_q <= 18'(tr_res_c);
            st_q <= tr_ret_q;
          end
        end

        // ---- the ring-direction table, once per job -----------------------
        S_RA_SEED: begin
          // theta(k) = rhu(0x10000 * k / K) = floor((2*0x10000*k + K) / (2K)).
          // Non-negative always, so the sign path is exercised by the CENTRE
          // and RADIUS lerps rather than here.
          dv_neg_q <= 1'b0;
          dv_num_q <= DVW'((32'(WHOLE_TURN) * 32'd2 * 32'(racur_q)) + 32'(p_k_q));
          dv_den_q <= 8'({p_k_q, 1'b0});
          dv_rem_q <= 8'd0;
          dv_quo_q <= '0;
          dv_cnt_q <= 7'(DVW);
          dv_ret_q <= S_RA_TRIG;
          st_q     <= S_DIV;
        end

        S_RA_TRIG: begin
          tr_ang_q <= dv_qres_c[15:0];
          tr_cnt_q <= 3'd0;
          tr_ret_q <= S_RA_STORE;
          st_q     <= S_TRIG;
        end

        S_RA_STORE: begin
          // `cs_q` is cos(theta) and `sn_q` is sin(theta): the ring direction
          // is U*cos + V*sin, so k = 0 lands on +U exactly.
          cu_q[racur_q[2:0]] <= cs_q;
          cv_q[racur_q[2:0]] <= sn_q;
          if (4'(racur_q) + 4'd1 == 4'(p_k_q)) begin
            st_q <= S_RING;
          end else begin
            racur_q <= racur_q + 4'd1;
            st_q    <= S_RA_SEED;
          end
        end

        // ---- one ring ------------------------------------------------------
        S_RING: begin
          rings_o <= rings_o + 32'd1;
          ccur_q  <= 2'd0;
          st_q    <= p_dome_q ? S_DM_SEED : S_CC_SEED;
        end

        S_DM_SEED: begin
          // phi(s) = rhu(0x4000 * s / N), 0 at the base and a QUARTER TURN at
          // the pole. Never negative.
          dv_neg_q <= 1'b0;
          dv_num_q <= DVW'((32'(QUARTER_TURN) * 32'd2 * 32'(seg_q)) + 32'(p_n_q));
          dv_den_q <= 8'({p_n_q[6:0], 1'b0});
          dv_rem_q <= 8'd0;
          dv_quo_q <= '0;
          dv_cnt_q <= 7'(DVW);
          dv_ret_q <= S_DM_TRIG;
          st_q     <= S_DIV;
        end

        S_DM_TRIG: begin
          tr_ang_q <= dv_qres_c[15:0];
          tr_cnt_q <= 3'd0;
          tr_ret_q <= S_DM_STORE;
          st_q     <= S_TRIG;
        end

        S_DM_STORE: begin
          ccur_q <= 2'd0;
          st_q   <= S_DM_MUL;   // sn_q and cs_q now hold sin phi and cos phi
        end

        S_DM_MUL: st_q <= S_DM_LAT;   // operands are driven; the product registers

        S_DM_LAT: begin
          // C[c] = sat(A0[c] + rescale16(dd[c] * sin phi)). TWO sat sites per
          // component, counted in this order -- the reference counts the same
          // two operations, so the totals are comparable and not merely close.
          C_q[ccur_q]  <= c_dom_c[31:0];
          sat_events_o <= sat_events_o + 32'(rs_m_c[32]) + 32'(c_dom_c[32]);
          if (ccur_q == 2'd2) begin
            st_q <= S_RR_SEED;
          end else begin
            ccur_q <= ccur_q + 2'd1;
            st_q   <= S_DM_MUL;
          end
        end

        S_CC_SEED: begin
          // LINEAR centre: off = floor((2*D*s + N) / (2N)), with the numerator
          // carried EXACTLY in acc_q and advanced by 2*D once per ring -- the
          // recurrence FORGE.PRIM.md licenses, "a recurrence with an exact
          // reseed", except that here the reseed is the accumulator itself and
          // there is nothing to drift.
          dv_neg_q <= acc_n_c[43];
          dv_num_q <= acc_n_c[43] ? DVW'(-acc_n_c) : DVW'(acc_n_c);
          dv_den_q <= 8'({p_n_q[6:0], 1'b0});
          dv_rem_q <= 8'd0;
          dv_quo_q <= '0;
          dv_cnt_q <= 7'(DVW);
          dv_ret_q <= S_CC_LAT;
          st_q     <= S_DIV;
        end

        S_CC_LAT: begin
          C_q[ccur_q]  <= c_lin_c[31:0];
          sat_events_o <= sat_events_o + 32'(c_lin_c[32]);
          if (ccur_q == 2'd2) st_q <= S_RR_SEED;
          else begin
            ccur_q <= ccur_q + 2'd1;
            st_q   <= S_CC_SEED;
          end
        end

        S_RR_SEED: begin
          dv_neg_q <= acr_n_c[43];
          dv_num_q <= acr_n_c[43] ? DVW'(-acr_n_c) : DVW'(acr_n_c);
          dv_den_q <= 8'({p_n_q[6:0], 1'b0});
          dv_rem_q <= 8'd0;
          dv_quo_q <= '0;
          dv_cnt_q <= 7'(DVW);
          dv_ret_q <= S_RR_LAT;
          st_q     <= S_DIV;
        end

        S_RR_LAT: begin
          R_q          <= r_lin_c[31:0];   // Rlin: the LINEAR taper, staged
          sat_events_o <= sat_events_o + 32'(r_lin_c[32]);
          mcur_q       <= 3'd0;
          st_q         <= p_dome_q ? S_RR_MUL : S_RU_MUL;
        end

        S_RR_MUL: st_q <= S_RR_LAT2;

        S_RR_LAT2: begin
          R_q          <= rs_m_c[31:0];
          sat_events_o <= sat_events_o + 32'(rs_m_c[32]);
          mcur_q       <= 3'd0;
          st_q         <= S_RU_MUL;
        end

        S_RU_MUL: st_q <= S_RU_LAT;

        S_RU_LAT: begin
          if (ax_is_v_c) RV_q[ax_idx_c] <= rs_m_c[31:0];
          else RU_q[ax_idx_c] <= rs_m_c[31:0];
          sat_events_o <= sat_events_o + 32'(rs_m_c[32]);
          if (mcur_q == 3'd5) begin
            vk_q   <= 5'd0;
            ccur_q <= 2'd0;
            st_q   <= S_W1;
          end else begin
            mcur_q <= mcur_q + 3'd1;
            st_q   <= S_RU_MUL;
          end
        end

        // ---- one vertex ----------------------------------------------------
        S_W1: st_q <= S_W1L;

        S_W1L: begin
          fa_q <= m_p_w;   // RU[a]*cu captured; RV[a]*cv is issuing behind it
          st_q <= S_W2L;
        end

        S_W2L: begin
          // FUSED: the exact sum of both products, ONE rounding. Splitting it
          // into two rescales would round twice and the reference would
          // disagree on the last bit of about half the vertices.
          pP_q[ccur_q] <= p_cmp_c[31:0];
          sat_events_o <= sat_events_o + 32'(rs_f_c[32]) + 32'(p_cmp_c[32]);
          if (ccur_q == 2'd2) st_q <= S_EMIT;
          else begin
            ccur_q <= ccur_q + 2'd1;
            st_q   <= S_W1;
          end
        end

        S_EMIT: begin
          if (v_ready_i) begin
            vertices_o <= vertices_o + 32'd1;
            ccur_q     <= 2'd0;
            if (vk_q + 5'd1 != p_k_q) begin
              vk_q <= vk_q + 5'd1;
              st_q <= S_W1;
            end else if (seg_q != p_n_q) begin
              // Advance the exact lerp numerators by 2*D. This is the whole
              // reason the divider is entered once per ring instead of once
              // per vertex, and the accumulation is EXACT so there is no drift
              // to reseed away.
              for (i = 0; i < 3; i = i + 1)
                acc_q[i] <= $signed(acc_q[i]) + $signed({{10{p_dd_q[i][32]}}, p_dd_q[i], 1'b0});
              acc_q[3] <= $signed(acc_q[3]) + $signed({{10{p_dr_q[32]}}, p_dr_q, 1'b0});
              seg_q    <= seg_q + 7'd1;
              vk_q     <= 5'd0;
              st_q     <= S_RING;
            end else begin
              st_q <= S_IDLE;
            end
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

  // The truncation above is a claim about `zhao_field_sin`'s range, and a claim
  // is the thing to check hardest. This fires the moment the table's width or
  // the trig law changes underneath us, which is exactly when the 18-bit ring
  // store would start dropping magnitude instead of sign.
  // synthesis translate_off
  always_ff @(posedge clk) begin : trig_range_claim
    // `rst_n` is deliberately NOT in this condition: reading it here would
    // flop it both synchronously and asynchronously (SYNCASYNCNET), and it is
    // not needed -- `st_q` resets to S_IDLE, so S_TRIG is unreachable under
    // reset and the guard below is already the stronger statement.
    if ((st_q == S_TRIG) && (tr_cnt_q == 3'd2 || tr_cnt_q == 3'd3)) begin
      if (tr_res_c > 32'sd65536 || tr_res_c < -32'sd65536)
        $fatal(1, "zhao_field_sin returned %0d, outside +-65536: the 18-bit ring store would drop MAGNITUDE, not sign", tr_res_c);
    end
  end
  // synthesis translate_on

endmodule : zhao_forge_ring_eval

`default_nettype wire
