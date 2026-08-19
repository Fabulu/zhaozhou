// measure_tokens_fairness_fv.sv — formal harness for the MEASURE.TOKENS Duo
// fairness guarantee (ZH-048; property measure_tokens_fairness.sby, which
// design/blocks.yml names as this block's formal lane).
//
// WHAT IS PROVED, and why it is the right thing to prove.
//
// The charter's sentence about this block is not about throughput and not
// about the ladder. It is: "One player looking directly into a volcano cannot
// make the other player's army disappear." That is a SAFETY property over the
// pool state, and it is the one thing about a token guard that no amount of
// random differential testing can settle — a differential lane proves the RTL
// agrees with the oracle, and if the LAW itself leaked between the views both
// would leak together. So this file proves the law, on the shipping module,
// for all time.
//
//   P1  a_view0_sealed   With no budget load, no granted view-0 request and no
//                        view-0 return, view 0's two guaranteed pools DO NOT
//                        MOVE. That is the volcano sentence stated as a
//                        transition property: nothing view 1 does — however
//                        much it spends, however it interleaves, however often
//                        it returns — can reduce what view 0 is able to spend.
//   P2  a_view1_sealed   The mirror image. Stated separately rather than
//                        argued by symmetry, because the port list is written
//                        out by hand and a transposed index would satisfy one
//                        and break the other.
//   P3a a_budget_latched The block's own budget registers hold exactly the
//                        five numbers the frame handed it. Needed for P3 to be
//                        inductive, and worth having on its own.
//   P3  a_within_budget  No pool ever exceeds the budget the frame loaded.
//                        This is the other half of the guarantee: P1 stops a
//                        view LOSING its allowance, P3 stops a view (or a
//                        malformed return) GAINING one. Without it a duplicated
//                        `token_return` would raise a view's spendable budget
//                        above what the frame promised, and every P1-style
//                        argument would still hold while the split silently
//                        stopped being 45/45/10.
//   P4  a_no_underflow   A grant never debits more than the pool it draws
//                        holds, so no pool can wrap. This is the admission
//                        rule's own consequence and the reason P3's subtract
//                        is safe.
//   P5  a_private_first  A grant is tagged SHARED only when the private pool
//                        was genuinely short — law T1's ordering.
//   P6  a_reserve_needs_essential   Low-priority refinement never reaches the
//                        emergency pool, which is charter §9 Version 1's
//                        "rejects only low-priority refinement" as an
//                        invariant rather than a test case.
//
// The task is `prove` (temporal induction), not `bmc`. Every property above is
// ONE-STEP INDUCTIVE — each holds in the next state given only that it held in
// this one — so induction closes them for ALL time. That matters here in the
// same way it did for the binner arena: the interesting state (a pool near
// empty, a budget near 2^32) is billions of tokens from reset and no bounded
// run of a sane depth would reach it.
//
// NO DIVIDER, NO MULTIPLIER, AND NOTHING UNROLLED. The whole datapath is
// five 32-bit compare/add/subtract lanes. That is a deliberate scoping
// decision, taken after `terrain_bake_delta.sby` cost 10.7 hours of solver
// time stuck on a 17-step restoring divide unrolled into SMT: a query that
// cannot finish is worse than none, so this one is kept arithmetic-flat.
//
// WHAT IS NOT PROVED, stated plainly. Not the counters (they are pure
// saturating adders, covered by the directed rail case and both random lanes),
// not the denial payload's timing (`fixed:1`, measured in the directed lane),
// and not the 45/45/10 numbers themselves — those arrive from the command
// stream and are the producer's, not this block's (law T3).

module measure_tokens_fairness_fv (
    input logic        clk,
    input logic        rst_n,
    input logic        budget_valid_i,
    input logic [31:0] budget_geom0_i,
    input logic [31:0] budget_geom1_i,
    input logic [31:0] budget_frag0_i,
    input logic [31:0] budget_frag1_i,
    input logic [31:0] budget_shared_i,
    input logic        req_valid_i,
    input logic        req_view_i,
    input logic        req_class_i,
    input logic        req_essential_i,
    input logic [ 2:0] req_rep_i,
    input logic [31:0] req_cost_i,
    input logic [15:0] req_src_id_i,
    input logic        ret_valid_i,
    input logic        ret_view_i,
    input logic        ret_class_i,
    input logic        ret_shared_i,
    input logic [31:0] ret_cost_i
);

  // ---- the SHIPPING instance: TOK_W = 32, the ABI's u32 -------------------
  logic        grant, shared;
  logic        den_valid;
  logic        den_view, den_class;
  logic [ 2:0] den_rep;
  logic [ 1:0] den_reason;
  logic [15:0] den_src_id;
  logic [31:0] den_cost;
  logic [31:0] g0, g1, f0, f1, sh;
  logic [31:0] rc0, rc1, rc2, rc3, rc4, rc5, rc6, rc7, culled;

  zhao_measure_tokens #(
      .TOK_W(32)
  ) u_dut (
      .clk(clk),
      .rst_n(rst_n),
      .budget_valid_i(budget_valid_i),
      .budget_geom0_i(budget_geom0_i),
      .budget_geom1_i(budget_geom1_i),
      .budget_frag0_i(budget_frag0_i),
      .budget_frag1_i(budget_frag1_i),
      .budget_shared_i(budget_shared_i),
      .req_valid_i(req_valid_i),
      .req_view_i(req_view_i),
      .req_class_i(req_class_i),
      .req_essential_i(req_essential_i),
      .req_rep_i(req_rep_i),
      .req_cost_i(req_cost_i),
      .req_src_id_i(req_src_id_i),
      .tok_grant_o(grant),
      .tok_shared_o(shared),
      .ret_valid_i(ret_valid_i),
      .ret_view_i(ret_view_i),
      .ret_class_i(ret_class_i),
      .ret_shared_i(ret_shared_i),
      .ret_cost_i(ret_cost_i),
      .den_valid_o(den_valid),
      .den_view_o(den_view),
      .den_class_o(den_class),
      .den_rep_o(den_rep),
      .den_reason_o(den_reason),
      .den_src_id_o(den_src_id),
      .den_cost_o(den_cost),
      .avail_geom0_o(g0),
      .avail_geom1_o(g1),
      .avail_frag0_o(f0),
      .avail_frag1_o(f1),
      .avail_shared_o(sh),
      .tok_rep_count0_o(rc0),
      .tok_rep_count1_o(rc1),
      .tok_rep_count2_o(rc2),
      .tok_rep_count3_o(rc3),
      .tok_rep_count4_o(rc4),
      .tok_rep_count5_o(rc5),
      .tok_rep_count6_o(rc6),
      .tok_rep_count7_o(rc7),
      .triangles_culled_o(culled)
  );

  // ---- the tiny instance, for the covers ----------------------------------
  // TOK_W = 4, so a cover trace can genuinely empty a pool, spill into the
  // reserve and clamp a credit inside a handful of steps. Every property below
  // is parameter-independent — none of them mentions 32 — so the induction on
  // the shipping instance and the reachability shown here are about the same
  // law.
  logic       s_grant, s_shared, s_den_valid, s_den_view, s_den_class;
  logic [2:0] s_den_rep;
  logic [1:0] s_den_reason;
  logic [15:0] s_den_src;
  logic [ 3:0] s_den_cost;
  logic [ 3:0] s_g0, s_g1, s_f0, s_f1, s_sh;
  logic [31:0] s_rc0, s_rc1, s_rc2, s_rc3, s_rc4, s_rc5, s_rc6, s_rc7, s_culled;

  zhao_measure_tokens #(
      .TOK_W(4)
  ) u_small (
      .clk(clk),
      .rst_n(rst_n),
      .budget_valid_i(budget_valid_i),
      .budget_geom0_i(budget_geom0_i[3:0]),
      .budget_geom1_i(budget_geom1_i[3:0]),
      .budget_frag0_i(budget_frag0_i[3:0]),
      .budget_frag1_i(budget_frag1_i[3:0]),
      .budget_shared_i(budget_shared_i[3:0]),
      .req_valid_i(req_valid_i),
      .req_view_i(req_view_i),
      .req_class_i(req_class_i),
      .req_essential_i(req_essential_i),
      .req_rep_i(req_rep_i),
      .req_cost_i(req_cost_i[3:0]),
      .req_src_id_i(req_src_id_i),
      .tok_grant_o(s_grant),
      .tok_shared_o(s_shared),
      .ret_valid_i(ret_valid_i),
      .ret_view_i(ret_view_i),
      .ret_class_i(ret_class_i),
      .ret_shared_i(ret_shared_i),
      .ret_cost_i(ret_cost_i[3:0]),
      .den_valid_o(s_den_valid),
      .den_view_o(s_den_view),
      .den_class_o(s_den_class),
      .den_rep_o(s_den_rep),
      .den_reason_o(s_den_reason),
      .den_src_id_o(s_den_src),
      .den_cost_o(s_den_cost),
      .avail_geom0_o(s_g0),
      .avail_geom1_o(s_g1),
      .avail_frag0_o(s_f0),
      .avail_frag1_o(s_f1),
      .avail_shared_o(s_sh),
      .tok_rep_count0_o(s_rc0),
      .tok_rep_count1_o(s_rc1),
      .tok_rep_count2_o(s_rc2),
      .tok_rep_count3_o(s_rc3),
      .tok_rep_count4_o(s_rc4),
      .tok_rep_count5_o(s_rc5),
      .tok_rep_count6_o(s_rc6),
      .tok_rep_count7_o(s_rc7),
      .triangles_culled_o(s_culled)
  );

  // ---- the initial state MUST be a reset ----------------------------------
  // `mode prove` starts from an UNCONSTRAINED initial state and an ASYNC reset
  // only bites while rst_n is low, so without this the solver may start with
  // pools already holding values no reachable trace produces — a false
  // counterexample about the harness rather than about the block. This is the
  // same pin geom_binner_arena_bounds_fv needed, for the same reason.
  logic first;
  initial first = 1'b1;
  always_ff @(posedge clk) first <= 1'b0;
  always_comb if (first) assume (!rst_n);

  // ---- the budget MIRROR, for P3 ------------------------------------------
  // A shadow of the block's own budget registers, loaded from the same inputs
  // on the same condition. It is not an assumption about the block: if the
  // block latched anything else, P3 would fail. Mirroring rather than reaching
  // into the instance keeps the property a statement about the PORTS.
  logic [31:0] mb_g0, mb_g1, mb_f0, mb_f1, mb_sh;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      mb_g0 <= 32'd0;
      mb_g1 <= 32'd0;
      mb_f0 <= 32'd0;
      mb_f1 <= 32'd0;
      mb_sh <= 32'd0;
    end else if (budget_valid_i) begin
      mb_g0 <= budget_geom0_i;
      mb_g1 <= budget_geom1_i;
      mb_f0 <= budget_frag0_i;
      mb_f1 <= budget_frag1_i;
      mb_sh <= budget_shared_i;
    end
  end

  // ---- history, reset-initialised and gated -------------------------------
  // Both the pools and the "was this view touched" condition are carried one
  // cycle by hand. Reset-initialised AND gated by `past_valid`, because
  // `mode prove` starts from a free initial state and without both the
  // induction BASE CASE fails at step 1 on history no reachable trace
  // produces — a false counterexample about the harness, not about the block.
  // (geom_binner_arena_bounds_fv learned that the hard way; so did this file.)
  //
  // Written as plain registers and IMMEDIATE assertions rather than SVA:
  // this toolchain's slang frontend rejects `default clocking`, `|=>` and
  // `$past` outright ("encountered unsupported SVA feature"), and a property
  // that cannot elaborate is not a property.
  logic        past_valid;
  logic [31:0] g0_q, g1_q, f0_q, f1_q;
  logic        touch0_q, touch1_q;
  logic [ 3:0] s_g0_q, s_sh_q;
  logic        s_credit_q;

  // Everything that could legitimately move view v's private pools this cycle.
  logic touch0, touch1;
  assign touch0 = budget_valid_i || (req_valid_i && grant && !req_view_i) ||
      (ret_valid_i && !ret_shared_i && !ret_view_i);
  assign touch1 = budget_valid_i || (req_valid_i && grant && req_view_i) ||
      (ret_valid_i && !ret_shared_i && ret_view_i);

  // A non-zero credit aimed at the small instance's view-0 geometry pool —
  // the precondition of the clamp cover.
  logic s_credit;
  assign s_credit = ret_valid_i && !ret_shared_i && !ret_view_i && !ret_class_i &&
      (ret_cost_i[3:0] != 4'd0) && !budget_valid_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      past_valid <= 1'b0;
      g0_q       <= 32'd0;
      g1_q       <= 32'd0;
      f0_q       <= 32'd0;
      f1_q       <= 32'd0;
      touch0_q   <= 1'b1;
      touch1_q   <= 1'b1;
      s_g0_q     <= 4'd0;
      s_sh_q     <= 4'd0;
      s_credit_q <= 1'b0;
    end else begin
      past_valid <= 1'b1;
      g0_q       <= g0;
      g1_q       <= g1;
      f0_q       <= f0;
      f1_q       <= f1;
      touch0_q   <= touch0;
      touch1_q   <= touch1;
      s_g0_q     <= s_g0;
      s_sh_q     <= s_sh;
      s_credit_q <= s_credit;
    end
  end

  // The private pool the current request is bound to.
  logic [31:0] avail_priv;
  always_comb begin
    if (req_view_i) avail_priv = req_class_i ? f1 : g1;
    else avail_priv = req_class_i ? f0 : g0;
  end

  // ---- the properties -----------------------------------------------------
  always_ff @(posedge clk) begin
    if (rst_n) begin
      // P1 / P2 — THE VOLCANO. With no budget load, no granted request of its
      // own and no return of its own, a view's guaranteed pools DO NOT MOVE.
      // Nothing the other view does appears in the antecedent, which is
      // precisely the point.
      if (past_valid && !touch0_q) begin
        a_view0_sealed : assert (g0 == g0_q && f0 == f0_q);
      end
      if (past_valid && !touch1_q) begin
        a_view1_sealed : assert (g1 == g1_q && f1 == f1_q);
      end

      // P3a — the block latched EXACTLY the five numbers the frame handed it.
      // Stated separately because P3 is only inductive together with it: under
      // temporal induction the mirror and the block's own budget registers are
      // independent free state, so without this the solver is free to start
      // from a pre-state where they disagree and P3 fails on a trace no
      // reachable run produces. (It did exactly that on the first run, and the
      // fix is this assertion rather than an assumption — an assumption here
      // would have hidden a block that latched something else.)
      a_budget_latched :
      assert (mb_g0 == u_dut.bud_geom0_r && mb_g1 == u_dut.bud_geom1_r &&
              mb_f0 == u_dut.bud_frag0_r && mb_f1 == u_dut.bud_frag1_r &&
              mb_sh == u_dut.bud_shared_r);

      // P3 — no pool ever exceeds the budget the frame loaded. P1 stops a view
      // LOSING its allowance; this stops anything GAINING one.
      a_within_budget :
      assert (g0 <= mb_g0 && g1 <= mb_g1 && f0 <= mb_f0 && f1 <= mb_f1 && sh <= mb_sh);

      // P4 — a grant never debits more than the pool it draws holds, so no
      // pool can wrap and P3's subtract is safe.
      if (grant) begin
        a_no_underflow : assert (shared ? (req_cost_i <= sh) : (req_cost_i <= avail_priv));
      end

      // P5 — law T1's ordering: the private pool is tried FIRST, always.
      if (grant && (req_cost_i <= avail_priv)) begin
        a_private_first : assert (!shared);
      end

      // P6 — charter section 9 Version 1 as an invariant: low-priority
      // refinement never reaches the emergency pool.
      if (shared) begin
        a_reserve_needs_essential : assert (req_essential_i);
      end
    end
  end

  // ---- the covers: the interesting states are REACHABLE -------------------
  // On the TINY instance, so a bounded trace can actually get there. Without
  // these, P1..P6 would all hold for a guard that never granted anything.
  always_ff @(posedge clk) begin
    if (rst_n) begin
      c_grant_private   : cover (s_grant && !s_shared);
      c_grant_reserve   : cover (s_grant && s_shared);
      c_denied          : cover (s_den_valid);
      c_pool_emptied    : cover (past_valid && s_g0 == 4'd0 && s_g0_q != 4'd0);
      c_reserve_emptied : cover (past_valid && s_sh == 4'd0 && s_sh_q != 4'd0);
      // A credit that landed on the ceiling: the pool is AT its budget after a
      // non-zero return that did not start there. That is the clamp's own
      // boundary, shown reachable rather than assumed.
      c_credit_clamped  : cover (past_valid && s_credit_q && s_g0 == mb_g0[3:0] &&
                                 s_g0_q != mb_g0[3:0]);
    end
  end

endmodule : measure_tokens_fairness_fv
