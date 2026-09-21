// zhao_terrain_tapshare.sv -- N CLIENTS ON ONE `zhao_terrain_heighttap`.
//
// ENFORCED-BY: tests/terrain/terrain_tapshare_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS: A CHECKMARK THAT WAS A CLAIM ABOUT PORT SHAPE
// ---------------------------------------------------------------------------
// `design/contracts/FORGE.SHADOW.md`'s blocker table marked the `tap_*` port
// group the ONE genuinely clear thing in that subsystem, on the grounds that
//
//   "`zhao_terrain_heighttap` mirrors these ports signal for signal and is
//    already composed."
//
// Both halves of that sentence are true and the conclusion does not follow.
// `zhao_terrain_heighttap` has EXACTLY ONE requester port group --
// `req_valid_i`, `req_ready_o`, `req_x_i`, `req_z_i`, `req_surface_i` -- and in
// `zhao_console_core.sv` every one of them is already connected to `htp_req_*`,
// driven by `u_part_terrain_tap`. There is no second port and no arbitration.
// A claim about a port's SHAPE had been read as a claim about its
// AVAILABILITY, which is owner ruling R180's "`upstream:` is design intent,
// not a wiring claim" in a different coat.
//
// This block is the missing half. It is deliberately NOT a change to
// `zhao_terrain_heighttap`: that block is composed, verified and on the
// critical path of PART.TERRAIN_TAP, and `CLAUDE.md` records that "a port on a
// leaf costs its WHOLE instantiation chain plus every bench". An arbiter in
// front costs the tap nothing and no existing bench anything.
//
// ---------------------------------------------------------------------------
// THE RESPONSE CARRIES NO TAG AND NO RIDER. THAT IS THE WHOLE DESIGN PROBLEM
// ---------------------------------------------------------------------------
// Every other shared service in this console routes its results BY RIDER --
// `zhao_project_service`'s header gives the reason: "the rider carried the
// owner all the way through the pipeline, so routing needs no shadow FIFO and
// cannot drift from the data it describes."
//
// `zhao_terrain_heighttap` has no such rider. `rsp_valid_o` and its eighteen
// data outputs arrive with NOTHING saying whose request they answer. So this
// block must hold the owner itself, and holding an owner is exactly the
// structure `CLAUDE.md`'s metadata-swap chapter is about. Two properties make
// it safe here, and both are PROVEN rather than assumed:
//
//   1. THE SERVICE IS STRICTLY SINGLE-IN-FLIGHT. `zhao_terrain_heighttap`'s
//      `assign req_ready_o = (st_q == S_IDLE);` accepts a request only when its
//      thirteen-state walk is idle, and the walk runs to completion before
//      returning to S_IDLE. So at most ONE request is ever outstanding and the
//      owner store is ONE REGISTER, not a queue. `SINGLE_FLIGHT_ONLY` below
//      records that this is a premise about the downstream block, and
//      `stray_rsp_o` is what says at run time that the premise held.
//
//   2. THE TWO SIDES OF THE OWNER CHECK ARE CLOCKED BY DIFFERENT ENABLES.
//      `own_v_q` is loaded on the REQUEST handshake (`t_req_valid_o &&
//      t_req_ready_i`) and cleared on the RESPONSE (`t_rsp_valid_i`). They are
//      different events from different blocks, so `stray_rsp_o` is not the
//      defect `CLAUDE.md` describes -- "a detector wired to two operands that
//      move together cannot fire". This one can, and the committed mutant
//      beside it shows it doing so.
//
// ---------------------------------------------------------------------------
// THE RESPONSE DATA IS BROADCAST, NOT REPLICATED, AND THAT IS DELIBERATE
// ---------------------------------------------------------------------------
// The tap answers with eighteen 32-bit-class outputs. Muxing them N ways would
// cost roughly 18 x 32 x N multiplexer bits for a value only one client can be
// waiting for, on a device where `CLAUDE.md` records ALMs as the binding
// constraint. So this block routes only the VALID: every client reads the
// tap's data bus directly and qualifies it with its own `r_rsp_valid_o[i]`.
// Exactly one bit can be set, because exactly one request can be outstanding.
//
// The cost of that choice, stated rather than hidden: a client MUST NOT latch
// the data bus on any cycle its own valid bit is low. That is an ordinary
// valid/data contract and it is the same one `zhao_terrain_heighttap` already
// has with its single client today.
//
// ---------------------------------------------------------------------------
// ARBITRATION IS THE HOUSE LAW, RESTATED AND NOT REINVENTED
// ---------------------------------------------------------------------------
// Rotating priority from `last_q`, the shape `zhao_mem_share_n` uses, bounded
// at N-1 turns: a client that wants the tap waits for at most the other N-1.
// `contended_o` counts the cycles where more than one client asked, which is
// the number that says whether the bound is ever exercised -- a fairness law
// nothing contends is a claim, not a measurement.
//
// Conservative SystemVerilog subset only (charter section 2). Quartus 17.0: no
// module-scope elaboration `if` (the guard is inside `initial begin`), no
// implicit generate, and every loop variable is declared INSIDE its block --
// `check_quartus17_syntax` form 7 records that a module-scope loop variable
// assigned inside a conditional `always_comb` is a LATCH that Quartus refuses
// while Verilator lints clean, and it killed a console fit in thirty seconds.
`default_nettype none

module zhao_terrain_tapshare #(
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
      $fatal(1, "zhao_terrain_tapshare: N is %0d; one to sixteen clients", N);
    if (IW != ((N > 1) ? $clog2(N) : 1))
      $fatal(1, "zhao_terrain_tapshare: IW is %0d but N %0d needs %0d",
             IW, N, (N > 1) ? $clog2(N) : 1);
    if (!SINGLE_FLIGHT_ONLY)
      $fatal(1, "zhao_terrain_tapshare: the owner store is ONE register and is only correct for a single-in-flight service; a pipelined tap needs a queue, not this block");
    if (CENSUS_W < 8)
      $fatal(1, "zhao_terrain_tapshare: CENSUS_W is %0d; too narrow to be evidence", CENSUS_W);
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
      if (t_rsp_valid_i) own_v_q <= 1'b0;

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

endmodule : zhao_terrain_tapshare

`default_nettype wire
