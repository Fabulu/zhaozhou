// zhao_terrain_tapshare_mutant.sv -- THE POSITIVE CONTROL FOR `stray_rsp_o`,
// which no legal stimulus can fire.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `stray_rsp_o` counts a response arriving while this block holds no owner.
// With a correct owner latch and a strictly single-in-flight service
// (`zhao_terrain_heighttap`'s `req_ready_o = (st_q == S_IDLE)`) that state is
// UNREACHABLE: the service only ever answers a request this block made, and
// the owner is held from the grant until that answer. So the counter cannot
// move under any legal input, and `CLAUDE.md` is explicit that a counter
// asserted zero and never seen to move is a CLAIM rather than a measurement --
// "a guard you cannot reach with legal stimulus needs a COMMITTED MUTANT".
//
// It is a COPY, not a wrapper, because the fault is INSIDE the always_ff and
// no parameter reaches it. `tools/budget/mutant_copy_drift.py` therefore gates
// it: REGENERATE THIS FILE if `zhao_terrain_tapshare.sv` changes shape. A copy
// of an old version is a positive control for a block that no longer exists,
// and it goes stale in the FLATTERING direction -- it keeps passing.
//
// ---------------------------------------------------------------------------
// THE ONE SUBSTANTIVE CHANGE
// ---------------------------------------------------------------------------
//     if (t_rsp_valid_i) own_v_q <= 1'b0;
//  -> if (t_rsp_valid_i || own_v_q) own_v_q <= 1'b0;
//
// The owner is now cleared one cycle after the grant instead of on the
// response. The grant's own assignment still wins on the grant cycle, so a
// service that answered IMMEDIATELY would still be routed and the mutant would
// look healthy -- which is precisely why the directed test holds the owner
// across a SIX-CYCLE walk. `zhao_terrain_heighttap` takes thirteen states, so
// the real service is always in the window this breaks.
//
// Under the mutation the answer reaches NOBODY (every `r_rsp_valid_o` bit is
// low) and `stray_rsp_o` increments. Both halves matter: the counter firing is
// the instrument's evidence, and the lost answer is the fault it is watching
// for.
//
// THE DRIVER IS tests/terrain/terrain_tapshare_mutant_control.cpp AND ITS
// POLARITY IS INVERTED: it passes when `stray_rsp_o` is NON-ZERO. It is
// evidence about the instrument, not about the design.
//
// Renamed so no production source list can elaborate it by mistake.
`default_nettype none

module zhao_terrain_tapshare_mutant #(
    // Clients sharing the one tap. NAMED AND EDITABLE (CLAUDE.md rule 6). Two
    // today -- PART.TERRAIN_TAP and FORGE.SHADOW -- and the parameter exists
    // because `zhao_geom_mem_adapter` is a FIXED A-E wrapper and adding its
    // sixth requester means editing the wrapper, an assign and an `N`. That is
    // the mistake this block declines to repeat.
    parameter int unsigned N = 2,
    // The owner index width.
    parameter int unsigned IW = (N > 1) ? $clog2(N) : 1,
    // A PREMISE, NOT A KNOB. It records that the downstream service accepts one
    // request at a time; the owner store is one register BECAUSE of it. If a
    // future tap pipelines its walk, this must become a queue and the
    // elaboration guard below is where that conversation starts.
    parameter bit SINGLE_FLIGHT_ONLY = 1'b1,
    parameter int unsigned CENSUS_W = 32
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the clients -------------------------------------------------------
    // `r_ready_o[i]` is a level and is high only for the client this cycle
    // picks, so a losing client is HELD rather than dropped.
    input  var logic        [N-1:0]  r_valid_i,
    output var logic        [N-1:0]  r_ready_o,
    input  var logic signed [31:0]   r_x_i [N],
    input  var logic signed [31:0]   r_z_i [N],
    input  var logic        [N-1:0]  r_surface_i,
    // The response VALID, routed. The DATA is not: see the header.
    output var logic        [N-1:0]  r_rsp_valid_o,

    // ---- the one service ---------------------------------------------------
    output var logic               t_req_valid_o,
    input  var logic               t_req_ready_i,
    output var logic signed [31:0] t_req_x_o,
    output var logic signed [31:0] t_req_z_o,
    output var logic               t_req_surface_o,
    input  var logic               t_rsp_valid_i,

    // ---- evidence ----------------------------------------------------------
    // Grants per client: the fairness law's own measurement.
    output var logic [CENSUS_W-1:0] grants_o [N],
    // Cycles on which more than one client asked. If this reads ZERO the
    // rotation has never been exercised and its bound is untested -- which is
    // a fact about the workload, and the directed test forces it.
    output var logic [CENSUS_W-1:0] contended_o,
    // A FAULT: the service answered while this block held no owner. Unreachable
    // with a correct owner latch and a single-in-flight service, so its
    // positive control is the committed mutant
    // `tests/mutants/zhao_terrain_tapshare_mutant.sv`, per CLAUDE.md's rule for
    // a guard no legal stimulus can reach.
    output var logic [CENSUS_W-1:0] stray_rsp_o,
    output var logic                busy_o
);

  // Quartus 17.0 needs the elaboration check inside `initial begin ... end`; a
  // bare module-scope `if` is a syntax error there however clean the lint, and
  // `--lint-only` does not run this block -- so a clean lint is NOT evidence
  // about it. It is fired by parameter override in the directed test.
  initial begin
    if (N < 1 || N > 16)
      $fatal(1, "zhao_terrain_tapshare_mutant: N is %0d; one to sixteen clients", N);
    if (IW != ((N > 1) ? $clog2(N) : 1))
      $fatal(1, "zhao_terrain_tapshare_mutant: IW is %0d but N %0d needs %0d",
             IW, N, (N > 1) ? $clog2(N) : 1);
    if (!SINGLE_FLIGHT_ONLY)
      $fatal(1, "zhao_terrain_tapshare_mutant: the owner store is ONE register and is only correct for a single-in-flight service; a pipelined tap needs a queue, not this block");
    if (CENSUS_W < 8)
      $fatal(1, "zhao_terrain_tapshare_mutant: CENSUS_W is %0d; too narrow to be evidence", CENSUS_W);
  end

  localparam logic [CENSUS_W-1:0] CNT_MAX = {CENSUS_W{1'b1}};

  // ---- the owner, ONE register ---------------------------------------------
  logic [IW-1:0] own_q;
  logic          own_v_q;
  logic [IW-1:0] last_q;

  // ---- the rotating pick ---------------------------------------------------
  // Every loop variable is declared inside the block (Quartus 17 form 7).
  logic [N-1:0]  wants_c;
  logic          any_c;
  logic          many_c;
  logic [IW-1:0] pick_c;

  always_comb begin
    int idx;
    int n_asking;
    // A client may be offered the service only while nothing is outstanding --
    // OR while the outstanding request is RETIRING this very cycle. The tap's
    // own `req_ready_o` would refuse a genuine overlap anyway; gating here as
    // well is what keeps the OWNER from being overwritten, which the tap cannot
    // see.
    //
    // THE `t_rsp_valid_i` TERM IS NOT AN OPTIMISATION BOLTED ON. Without it the
    // grant could not happen until the cycle AFTER the retire, so every tap
    // cost one dead cycle -- about 8% on `zhao_terrain_heighttap`'s thirteen-
    // state walk, paid on every height tap PART.TERRAIN_TAP and FORGE.SHADOW
    // ever make. The directed test caught it as a failed "granted on the
    // retiring cycle" check; it is recorded here because the first version of
    // this block had the dead cycle and nothing but that check would have
    // noticed.
    //
    // AND IT ASSUMES NOTHING ABOUT THE SERVICE'S TIMING. If the tap is not
    // ready on the retiring cycle, `grant_c` is simply low and the offer stands
    // next cycle -- the ordinary ready/valid outcome. So this is safe whether
    // or not `req_ready_o` happens to rise with `rsp_valid_o`, which is an
    // internal fact about a block this one deliberately does not depend on.
    for (int i = 0; i < N; i++)
      wants_c[i] = r_valid_i[i] && (!own_v_q || t_rsp_valid_i);
    any_c    = 1'b0;
    pick_c   = '0;
    n_asking = 0;
    for (int i = 0; i < N; i++) if (wants_c[i]) n_asking = n_asking + 1;
    many_c = (n_asking > 1);
    for (int k = 1; k <= N; k++) begin
      idx = int'(last_q) + k;
      if (idx >= int'(N)) idx = idx - int'(N);
      if (!any_c && wants_c[idx]) begin
        any_c  = 1'b1;
        pick_c = IW'(idx);
      end
    end
  end

  // ---- the offer ------------------------------------------------------------
  assign t_req_valid_o   = any_c;
  assign t_req_x_o       = r_x_i[pick_c];
  assign t_req_z_o       = r_z_i[pick_c];
  assign t_req_surface_o = r_surface_i[pick_c];

  // A level, and only for the picked client.
  always_comb begin
    for (int i = 0; i < N; i++)
      r_ready_o[i] = any_c && (pick_c == IW'(i)) && t_req_ready_i;
  end

  // ---- the response, routed by the HELD owner -------------------------------
  // Combinational, so the client sees its answer on the same cycle the tap
  // presents the data and no copy of the data has to exist here.
  always_comb begin
    for (int i = 0; i < N; i++)
      r_rsp_valid_o[i] = t_rsp_valid_i && own_v_q && (own_q == IW'(i));
  end

  assign busy_o = own_v_q;

  wire grant_c = t_req_valid_o && t_req_ready_i;
  wire stray_c = t_rsp_valid_i && !own_v_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      own_q       <= '0;
      own_v_q     <= 1'b0;
      last_q      <= '0;
      contended_o <= '0;
      stray_rsp_o <= '0;
      for (int i = 0; i < N; i++) grants_o[i] <= '0;
    end else begin
      // The response retires the owner. It is written BEFORE the grant below so
      // a same-cycle retire-and-regrant keeps the new owner, which is the only
      // ordering that cannot lose a client.
      // MUTATION (the one substantive line): `t_rsp_valid_i` -> `t_rsp_valid_i || own_v_q`
      if (t_rsp_valid_i || own_v_q) own_v_q <= 1'b0;

      if (grant_c) begin
        own_q   <= pick_c;
        own_v_q <= 1'b1;
        last_q  <= pick_c;
        if (grants_o[pick_c] != CNT_MAX)
          grants_o[pick_c] <= grants_o[pick_c] + CENSUS_W'(1);
        if (many_c && (contended_o != CNT_MAX))
          contended_o <= contended_o + CENSUS_W'(1);
      end

      if (stray_c && (stray_rsp_o != CNT_MAX))
        stray_rsp_o <= stray_rsp_o + CENSUS_W'(1);
    end
  end

endmodule : zhao_terrain_tapshare_mutant

`default_nettype wire
