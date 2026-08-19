// zhao_measure_tokens.sv — MEASURE.TOKENS: the global token guard and the
// Duo fairness split (phase 8, ZH-048).
//
// Law, in citation order:
//   design/contracts/MEASURE.TOKENS.md — the block contract, where every
//       chosen law below is argued at length with what it rejected.
//   design/blocks.yml — `inputs: [dispatch, token_return]`, `outputs:
//       [token_grant, token_denial]`, `upstream: [CMD.SCHEDULER]`,
//       `downstream: [GEOM.BINNER, RASTER.FRAGMENT]`, `backpressure: credit`,
//       `latency: fixed:1`, "1 grant decision per clock", counters
//       `[lod_representation_counts, triangles_culled]`, `source_ids: true`.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Duo fairness" — "45%
//       guaranteed to player 1; 45% guaranteed to player 2; 10% shared
//       emergency pool. One player looking directly into a volcano cannot
//       make the other player's army disappear." THAT SENTENCE IS THE WHOLE
//       BLOCK. It is a STRUCTURAL statement, not a numeric one, and it is
//       implemented structurally: each view's guaranteed pool is reachable
//       ONLY by that view, so no sequence of requests from one view can
//       reduce what the other view is able to spend.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Practical implementation
//       path", Version 1 — "a global token guard rejects only low-priority
//       refinement when the budget is nearly exhausted". That is the
//       admission rule: near exhaustion, what is denied is low-priority
//       refinement, because essential work still has the emergency pool.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Inputs" — each camera
//       provides a "guaranteed token budget"; each root an "approximate
//       triangle/vertex/fragment cost". Cost is per-request and comes in on
//       the wire; the budget is per-frame and comes from the command stream.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Representation ladder" — the
//       seven rungs the `lod_representation_counts` lanes count.
//   spec/commands.zidl `SetPresentationContract 0x0020` — `u32
//       geometry_tokens[2]`, `u32 fragment_tokens[2]`, `u32 shared_tokens`.
//       FIVE numbers, and the shape of the pool set is taken from them
//       verbatim: two private geometry pools, two private fragment pools, and
//       ONE shared pool spanning both views AND both classes. That last fact
//       is the ABI's, not this block's: `shared_tokens` is a scalar.
//   spec/commands.zidl `SetView 0x0010` — `u32 geometry_tokens`, `u32
//       fragment_tokens` per view: the same two private numbers, per view,
//       for the single-view contract.
//   spec/counters.md §2/§4 — counters are catalog entries (adding one is a
//       ledger edit, never an RTL whim) and they SATURATE, never wrap.
//   fpga/rtl/geometry/zhao_geom_binner.sv law E — the landed consumer's
//       already-committed seam, quoted and honoured below.
//
// ---------------------------------------------------------------------------
// THE SEAM — found in a LANDED block, not invented here
// ---------------------------------------------------------------------------
// GEOM.BINNER shipped in phase 6 with the guard's other half already written,
// and its law E says exactly what it expects:
//
//     "one combinational request/grant pair. `tok_req_o` pulses on the cycle a
//      triangle is accepted and `tok_grant_i` is sampled on that same edge; a
//      denied triangle is dropped and counted into `triangles_culled`. Tie
//      `tok_grant_i` high and the guard is absent. Deliberately NOT invented
//      here: the 45/45/10 Duo fairness split, any token WIDTH or cost model,
//      and the return path — all of those are MEASURE.TOKENS' law to write."
//
// So `tok_grant_o` IS COMBINATIONAL FROM `req_valid_i`, and the ledger's
// `latency: fixed:1` is NOT the grant's latency. This is a deviation and it is
// recorded rather than resolved by fiat: the alternative — a registered grant
// one cycle after the request — would require re-opening a landed, tested,
// lint-clean block to add a stall state, and would contradict a law that block
// wrote down deliberately. `latency: fixed:1` is honoured by the OTHER output,
// `token_denial`, which is registered and presents exactly one cycle after the
// refused request. Measured, not asserted:
//     ENFORCED-BY: tests/measure/measure_tokens_directed.cpp:test_latency
//
// The second declared consumer, RASTER.FRAGMENT, HAS NO TOKEN PORT AT ALL —
// `zhao_raster_fragment.sv`'s only `error`/`token` signal is
// `fragment_error_o`, a one-bit tilestore-read protocol flag. The fragment
// class is therefore built and served here exactly as the geometry class is,
// and its consumer side is named as MISSING rather than faked.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN, NOT FOUND (each also argued in the contract)
// ---------------------------------------------------------------------------
// T1. THE PRIVATE POOL IS TRIED FIRST, ALWAYS; THE SHARED POOL IS A FALLBACK
//     AND ONLY ESSENTIAL WORK MAY REACH IT.
//     §9 gives three pools and calls the third an "emergency" pool, and gives
//     the admission rule "rejects only low-priority refinement when the budget
//     is nearly exhausted". Together those two sentences have exactly one
//     consistent reading: low-priority refinement is confined to its view's
//     own pool, so when that pool nears exhaustion refinement is what stops;
//     essential work spills into the emergency pool and keeps going. The
//     charter never spells the rule out, so it is CHOSEN.
//     REJECTED: (a) shared-pool-first (it would burn the emergency reserve on
//     the frame's first requests, and there would be no emergency left for the
//     work the reserve exists for); (b) proportional draw, taking part of a
//     request from each pool (it splits one request's accounting across two
//     pools, so the return path must carry two numbers and a partially
//     returned grant can inflate one pool while starving the other);
//     (c) letting low-priority work reach the shared pool too (then the guard
//     rejects HIGH-priority work when the budget is nearly exhausted, which is
//     the charter's sentence backwards).
// T2. A VIEW'S PRIVATE POOLS ARE UNREACHABLE BY THE OTHER VIEW. This is the
//     mechanical content of "one player looking directly into a volcano cannot
//     make the other player's army disappear", and it is what makes the word
//     "guaranteed" mean something. It is not a policy knob: there is no path
//     in this file from `req_view_i == 1` to `avail_geom0_o`.
//     REJECTED: a work-conserving guard that lends an idle view's unspent
//     tokens to the busy one. It renders more, and it destroys the guarantee:
//     a player who looks at the sky for two frames and then turns around finds
//     their budget already spent. The charter chose the guarantee over the
//     throughput and so does this block.
//     ENFORCED-BY: tests/formal/measure_tokens_fairness.sby (a_view0_sealed,
//     a_view1_sealed — proved by TEMPORAL INDUCTION on the shipping instance)
// T3. THE 45/45/10 NUMBERS ARE NOT IN THIS FILE. They arrive on
//     `budget_*_i` from SetPresentationContract. The charter's split is a
//     POLICY the producer sets; the guard ENFORCES whatever five numbers it is
//     handed. REJECTED: hardcoding 45/45/10 and taking one total. It would
//     make a ratified ABI field dead, and it would put a percentage-to-token
//     division inside the guard's combinational grant path.
//     (Observed and NOT ratified here: the Nanquan compiler currently writes
//     PERCENTAGES into these u32 fields — `record.payload.geometry_tokens[0u]
//     = 80u` — while this block reads them as absolute token counts. The
//     block is unit-agnostic, so it is correct under either reading, but the
//     two producers do not agree with each other and that is written down in
//     the contract rather than papered over.)
// T4. A RETURN NAMES THE POOL ITS GRANT DREW FROM. `tok_shared_o` is presented
//     with the grant; `ret_shared_i` echoes it back. REJECTED: returning to
//     the private pool first and spilling to shared. That MOVES tokens
//     permanently from the shared pool into a private one — draw from shared,
//     return to private — and after enough round trips a private pool would
//     exceed its guaranteed budget while the emergency pool emptied, with no
//     event anywhere marking it. The echo costs one wire.
// T5. A RETURN CANNOT INFLATE A POOL PAST ITS BUDGET. Excess is dropped
//     silently and the pool clamps. REJECTED: trusting the return (a single
//     malformed or duplicated return would raise a view's spendable budget
//     above its guarantee — the exact thing T2 exists to prevent — so the
//     clamp is a safety property, not tidiness). It is directly observable:
//     `avail_*_o` never exceeds the loaded budget, and that is asserted.
// T6. A RETURN ARRIVING IN THE SAME CYCLE AS A REQUEST DOES NOT HELP IT. The
//     grant is decided against the pools AS THEY STAND; the return lands for
//     the next cycle. REJECTED: forwarding the return combinationally into the
//     decision. It would put an adder in front of the comparator on a path
//     that GEOM.BINNER already samples combinationally in its accept cycle,
//     and it would make the grant depend on a signal that consumer does not
//     produce. The two updates still compose exactly — one subtract and one
//     add on the same pool in the same cycle — so nothing is lost but the
//     forwarding.
// T7. `req_rep_i` IS A PORT, because `lod_representation_counts` has to come
//     from somewhere. §9's ladder has seven rungs (full form, reduced mesh,
//     rigid combat form, micro-mesh, splat cluster, glint, culled); three bits
//     carry them with one spare, and the counter is GRANTS per rung. That is
//     the Measure's own vocabulary and this is the block §9 puts in charge of
//     it. REJECTED: mapping the four (view, class) pairs onto the four lanes
//     TERRAIN.LOD uses. Those are not representations, and a counter whose
//     name lies is worse than a counter that is missing.
// T8. A DENIED GEOMETRY REQUEST ADDS ITS COST TO `triangles_culled`.
//     §9 calls a geometry token an "approximate triangle/vertex/fragment
//     cost", so a geometry token IS a triangle in the charter's own
//     approximation, and refusing N of them culls N triangles. A denied
//     FRAGMENT request adds nothing: fragments are not triangles.
//     REJECTED: counting one per denial regardless of cost (it would report a
//     refused 500-triangle meshlet and a refused single triangle identically,
//     which is exactly the number a budget post-mortem needs to tell apart).
// T9. NOTHING IS SILENT. A request refused because a budget load is landing in
//     the same cycle is refused with `den_reason_o = REASON_RELOAD`, not
//     dropped. The frame-boundary protocol makes that unreachable in real
//     traffic; it is defined anyway, because "unreachable" is a claim about
//     the producer and this block does not get to make claims about producers.
//
// ---------------------------------------------------------------------------
// WIDTHS AND OVERFLOW, STATED RATHER THAN ASSUMED
// ---------------------------------------------------------------------------
// Pools and costs are unsigned 32 (`TOK_W`), the ABI's u32, and every
// comparison is unsigned between two same-width operands. A grant guarantees
// `req_cost_i <= avail` on the pool it draws, so the debit CANNOT underflow —
// that is a consequence of the admission rule, and it is asserted rather than
// assumed. The credit is formed at TOK_W+1 bits and clamped to the budget, so
// it cannot wrap either. Counters are 32-bit and SATURATE at 0xFFFF_FFFF per
// spec/counters.md §4; `triangles_culled_o` saturates on a wide add, not a
// wrapping one, which is a different check and is tested at the rail.
//
// NOT IN THIS BLOCK, deliberately: no priority HEAP (charter §9: "Do not begin
// with a global FPGA priority heap" — Version 1 is a guard, not a scheduler);
// no error histogram and no cutoff bucket (that is MEASURE.HISTOGRAM, charter
// Version 2); no re-submission of denied work to the next frame (GEOM.BINNER
// law D names that as unbuilt and this block does not build it either — it
// reports, through `token_denial` and `triangles_culled_o`, that it happened);
// no per-request cost MODEL (the cost arrives on the wire; deriving it from a
// meshlet descriptor is GEOM.MESHFETCH's); no memory of any kind beyond the
// five pools and the five budgets.
//
// Conservative SystemVerilog subset only (charter §2). No function-call result
// is indexed anywhere in this file: Verilator accepts `f(x)[7:0]`, Quartus
// 17.0 rejects it outright, and it cost GEOM.BINNER a synthesis failure that
// every simulation lane passed.

module zhao_measure_tokens #(
    parameter int unsigned TOK_W = 32
) (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // `dispatch` (CMD.SCHEDULER) — the frame's five budgets, from
    // SetPresentationContract. A one-cycle pulse LOADS the budgets AND refills
    // every pool to them: a frame starts with its whole allowance, which is
    // what "per-frame budget" means.
    // -----------------------------------------------------------------------
    input logic             budget_valid_i,
    input logic [TOK_W-1:0] budget_geom0_i,
    input logic [TOK_W-1:0] budget_geom1_i,
    input logic [TOK_W-1:0] budget_frag0_i,
    input logic [TOK_W-1:0] budget_frag1_i,
    input logic [TOK_W-1:0] budget_shared_i,

    // -----------------------------------------------------------------------
    // The request. COMBINATIONAL in / COMBINATIONAL out — GEOM.BINNER's law E.
    // `req_essential_i` is the charter's low-priority/essential distinction:
    // low priority (0) is refinement and never reaches the emergency pool.
    // -----------------------------------------------------------------------
    input logic             req_valid_i,
    input logic             req_view_i,
    input logic             req_class_i,      // 0 = geometry, 1 = fragment
    input logic             req_essential_i,
    input logic [      2:0] req_rep_i,        // §9 ladder rung
    input logic [TOK_W-1:0] req_cost_i,
    input logic [     15:0] req_src_id_i,

    // `token_grant` — combinational, same cycle as the request.
    output logic tok_grant_o,
    output logic tok_shared_o,  // the grant drew the emergency pool

    // -----------------------------------------------------------------------
    // `token_return` — tokens handed back. `ret_shared_i` echoes the
    // `tok_shared_o` its grant presented (law T4).
    // -----------------------------------------------------------------------
    input logic             ret_valid_i,
    input logic             ret_view_i,
    input logic             ret_class_i,
    input logic             ret_shared_i,
    input logic [TOK_W-1:0] ret_cost_i,

    // -----------------------------------------------------------------------
    // `token_denial` — REGISTERED, exactly one cycle after the refused
    // request. This is the ledger's `latency: fixed:1`.
    // -----------------------------------------------------------------------
    output logic             den_valid_o,
    output logic             den_view_o,
    output logic             den_class_o,
    output logic [      2:0] den_rep_o,
    output logic [      1:0] den_reason_o,
    output logic [     15:0] den_src_id_o,
    output logic [TOK_W-1:0] den_cost_o,

    // -----------------------------------------------------------------------
    // The live pools, so a consumer (and every test) can read the guarantee
    // instead of inferring it.
    // -----------------------------------------------------------------------
    output logic [TOK_W-1:0] avail_geom0_o,
    output logic [TOK_W-1:0] avail_geom1_o,
    output logic [TOK_W-1:0] avail_frag0_o,
    output logic [TOK_W-1:0] avail_frag1_o,
    output logic [TOK_W-1:0] avail_shared_o,

    // -----------------------------------------------------------------------
    // Counters (spec/counters.md): the eight lanes of
    // `lod_representation_counts`, and `triangles_culled`.
    // -----------------------------------------------------------------------
    output logic [31:0] tok_rep_count0_o,
    output logic [31:0] tok_rep_count1_o,
    output logic [31:0] tok_rep_count2_o,
    output logic [31:0] tok_rep_count3_o,
    output logic [31:0] tok_rep_count4_o,
    output logic [31:0] tok_rep_count5_o,
    output logic [31:0] tok_rep_count6_o,
    output logic [31:0] tok_rep_count7_o,
    output logic [31:0] triangles_culled_o
);

  // Denial reasons. 3 is unused and never presented.
  localparam logic [1:0] REASON_LOW_PRIORITY = 2'd0;  // private short, not essential
  localparam logic [1:0] REASON_EXHAUSTED = 2'd1;  // essential, but shared short too
  localparam logic [1:0] REASON_RELOAD = 2'd2;  // a budget load landed this cycle

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  // ---- the five budgets and the five pools ---------------------------------
  logic [TOK_W-1:0] bud_geom0_r, bud_geom1_r, bud_frag0_r, bud_frag1_r, bud_shared_r;
  logic [TOK_W-1:0] avail_geom0_r, avail_geom1_r, avail_frag0_r, avail_frag1_r;
  logic [TOK_W-1:0] avail_shared_r;

  assign avail_geom0_o  = avail_geom0_r;
  assign avail_geom1_o  = avail_geom1_r;
  assign avail_frag0_o  = avail_frag0_r;
  assign avail_frag1_o  = avail_frag1_r;
  assign avail_shared_o = avail_shared_r;

  // ---- the combinational admission rule (laws T1, T2) ----------------------
  // The private pool this request is bound to. NOTE the shape of this mux: it
  // is the ONLY place a view index selects a pool, and view 1 can select only
  // pools 1. That is law T2 as a structural fact.
  logic [TOK_W-1:0] avail_priv;
  always_comb begin
    if (req_view_i) avail_priv = req_class_i ? avail_frag1_r : avail_geom1_r;
    else avail_priv = req_class_i ? avail_frag0_r : avail_geom0_r;
  end

  logic fits_priv, fits_shared, may_share;
  assign fits_priv   = (req_cost_i <= avail_priv);
  assign fits_shared = (req_cost_i <= avail_shared_r);
  assign may_share   = req_essential_i && !fits_priv && fits_shared;

  // A budget load owns the pools this cycle (law T9): no grant is made against
  // state that is being replaced.
  assign tok_grant_o  = req_valid_i && !budget_valid_i && (fits_priv || may_share);
  assign tok_shared_o = tok_grant_o && !fits_priv;

  // ---- per-pool debit and credit ------------------------------------------
  // A pool changes only through these two terms, and `sel_*` is the only gate.
  // The debit is bounded by the admission rule; the credit is clamped by T5.
  logic draw_priv, draw_shared;
  assign draw_priv   = tok_grant_o && fits_priv;
  assign draw_shared = tok_grant_o && !fits_priv;

  logic sel_g0, sel_g1, sel_f0, sel_f1;
  assign sel_g0 = draw_priv && !req_view_i && !req_class_i;
  assign sel_g1 = draw_priv && req_view_i && !req_class_i;
  assign sel_f0 = draw_priv && !req_view_i && req_class_i;
  assign sel_f1 = draw_priv && req_view_i && req_class_i;

  logic ret_priv;
  assign ret_priv = ret_valid_i && !ret_shared_i;

  logic rsel_g0, rsel_g1, rsel_f0, rsel_f1;
  assign rsel_g0 = ret_priv && !ret_view_i && !ret_class_i;
  assign rsel_g1 = ret_priv && ret_view_i && !ret_class_i;
  assign rsel_f0 = ret_priv && !ret_view_i && ret_class_i;
  assign rsel_f1 = ret_priv && ret_view_i && ret_class_i;

  // One pool's next value: (pool - debit) + credit, clamped to `bud`. The
  // subtract is safe by the admission rule; the add is done one bit wide and
  // clamped, so neither can wrap. Written as a function so the five pools
  // cannot drift apart — and its RESULT IS NEVER INDEXED (Quartus 17.0).
  function automatic logic [TOK_W-1:0] pool_next(input logic [TOK_W-1:0] pool,
                                                 input logic [TOK_W-1:0] bud, input logic debit,
                                                 input logic [TOK_W-1:0] debit_amt,
                                                 input logic credit,
                                                 input logic [TOK_W-1:0] credit_amt);
    logic [TOK_W-1:0] after_debit;
    logic [  TOK_W:0] widened;
    begin
      after_debit = debit ? (pool - debit_amt) : pool;
      widened     = credit ? ({1'b0, after_debit} + {1'b0, credit_amt}) : {1'b0, after_debit};
      if (widened > {1'b0, bud}) pool_next = bud;
      else pool_next = widened[TOK_W-1:0];
    end
  endfunction

  // ---- counter helper: saturating add (spec/counters.md §4) ---------------
  function automatic logic [31:0] cnt_add(input logic [31:0] cur, input logic [31:0] inc);
    logic [32:0] w;
    begin
      w = {1'b0, cur} + {1'b0, inc};
      if (w[32]) cnt_add = CNT_MAX;
      else cnt_add = w[31:0];
    end
  endfunction

  // ---- the one sequential block -------------------------------------------
  logic denied;
  assign denied = req_valid_i && !tok_grant_o;

  logic [31:0] rep_cnt_r[0:7];

  assign tok_rep_count0_o = rep_cnt_r[0];
  assign tok_rep_count1_o = rep_cnt_r[1];
  assign tok_rep_count2_o = rep_cnt_r[2];
  assign tok_rep_count3_o = rep_cnt_r[3];
  assign tok_rep_count4_o = rep_cnt_r[4];
  assign tok_rep_count5_o = rep_cnt_r[5];
  assign tok_rep_count6_o = rep_cnt_r[6];
  assign tok_rep_count7_o = rep_cnt_r[7];

  integer i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      bud_geom0_r        <= '0;
      bud_geom1_r        <= '0;
      bud_frag0_r        <= '0;
      bud_frag1_r        <= '0;
      bud_shared_r       <= '0;
      avail_geom0_r      <= '0;
      avail_geom1_r      <= '0;
      avail_frag0_r      <= '0;
      avail_frag1_r      <= '0;
      avail_shared_r     <= '0;
      den_valid_o        <= 1'b0;
      den_view_o         <= 1'b0;
      den_class_o        <= 1'b0;
      den_rep_o          <= 3'd0;
      den_reason_o       <= REASON_LOW_PRIORITY;
      den_src_id_o       <= 16'd0;
      den_cost_o         <= '0;
      triangles_culled_o <= 32'd0;
      for (i = 0; i < 8; i = i + 1) rep_cnt_r[i] <= 32'd0;
    end else begin
      // ---- pools -----------------------------------------------------------
      if (budget_valid_i) begin
        bud_geom0_r    <= budget_geom0_i;
        bud_geom1_r    <= budget_geom1_i;
        bud_frag0_r    <= budget_frag0_i;
        bud_frag1_r    <= budget_frag1_i;
        bud_shared_r   <= budget_shared_i;
        avail_geom0_r  <= budget_geom0_i;
        avail_geom1_r  <= budget_geom1_i;
        avail_frag0_r  <= budget_frag0_i;
        avail_frag1_r  <= budget_frag1_i;
        avail_shared_r <= budget_shared_i;
      end else begin
        avail_geom0_r <= pool_next(avail_geom0_r, bud_geom0_r, sel_g0, req_cost_i, rsel_g0,
                                   ret_cost_i);
        avail_geom1_r <= pool_next(avail_geom1_r, bud_geom1_r, sel_g1, req_cost_i, rsel_g1,
                                   ret_cost_i);
        avail_frag0_r <= pool_next(avail_frag0_r, bud_frag0_r, sel_f0, req_cost_i, rsel_f0,
                                   ret_cost_i);
        avail_frag1_r <= pool_next(avail_frag1_r, bud_frag1_r, sel_f1, req_cost_i, rsel_f1,
                                   ret_cost_i);
        avail_shared_r <= pool_next(avail_shared_r, bud_shared_r, draw_shared, req_cost_i,
                                    ret_valid_i && ret_shared_i, ret_cost_i);
      end

      // ---- the registered denial (the ledger's fixed:1) --------------------
      den_valid_o  <= denied;
      den_view_o   <= req_view_i;
      den_class_o  <= req_class_i;
      den_rep_o    <= req_rep_i;
      den_src_id_o <= req_src_id_i;
      den_cost_o   <= req_cost_i;
      if (budget_valid_i) den_reason_o <= REASON_RELOAD;
      else if (req_essential_i) den_reason_o <= REASON_EXHAUSTED;
      else den_reason_o <= REASON_LOW_PRIORITY;

      // ---- counters --------------------------------------------------------
      if (tok_grant_o) rep_cnt_r[req_rep_i] <= cnt_add(rep_cnt_r[req_rep_i], 32'd1);
      if (denied && !req_class_i) triangles_culled_o <= cnt_add(triangles_culled_o, req_cost_i);
    end
  end

`ifdef ZHAO_ASSERT
  // The admission rule's own consequence, checked in simulation: a grant never
  // debits more than the pool holds, so no pool can wrap. This is the property
  // the formal lane proves for all time; here it rides every directed and
  // random cycle as well.
  always_ff @(posedge clk) begin
    if (rst_n) begin
      if (draw_priv) assert (req_cost_i <= avail_priv);
      if (draw_shared) assert (req_cost_i <= avail_shared_r);
    end
  end
`endif

endmodule
