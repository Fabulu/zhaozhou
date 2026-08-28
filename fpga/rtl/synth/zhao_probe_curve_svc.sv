// zhao_probe_curve_svc.sv — Field v3 decisive probe 4 (reports/Fieldv3.md
// Phase 3): the barrel curve service.
//
// TARGET: four-point CURVE initiation interval <= 14 clocks, timing-clean at
// the gpu-domain constraint. The target is derived from memory-port demand,
// not chosen: a four-lane CURVE needs 24 table reads (six search steps per
// lane), and TWO true-dual-port reads per clock give a structural minimum of
// 12 lookup clocks. A probe that misses kills or changes this topology
// BEFORE it contaminates the engine.
// ENFORCED-BY: tests/differential/field_curve_svc_directed.cpp:main
//
// WHAT IT IS: the v2 curve unit (zhao_field_curve, ~950 ALM) performs six
// DEPENDENT binary-search steps and idles during every registered table
// wait. Copying it four times is the wrong answer (Fieldv3.md section 6).
// This service instead runs FOUR scalar lane contexts as a barrel over an
// active-program table cache with two read ports: lanes 0/1 interleave on
// port A, lanes 2/3 on port B, so the registered-read wait of one lane is
// the address cycle of its partner and both ports issue a useful address
// every search clock. Six steps x 2 lanes x 2 cycles = 12 address slots per
// port; the thirteenth cycle consumes the final read and hands the group to
// the finish stage, so the DESIGNED II is 13 — one handoff clock above the
// structural minimum, one under the target.
//
// WHY SIX READS PER LANE IS ENOUGH (the existing unit spends nine states on
// fetches): the clamp bounds x[0]/x[n-1] and the entry at index 0 are
// PROPERTIES OF THE TABLE, latched once at table load into per-table meta
// registers — the active-program table cache the brief names. The search
// then needs only its six step reads, because the selected entry is
// CAPTURED ON THE WAY DOWN: whenever a step is taken (mid <= n-1 and
// x[mid] <= clamped), the just-read {x,y,dy} at mid IS the entry at the new
// lo; when no step is ever taken, lo stays 0 and the meta registers already
// hold entry 0. After step k=0 the lane holds x[lo], y[lo], dy[lo] with no
// further fetch.
// ENFORCED-BY: tests/differential/field_curve_svc_directed.cpp:main
//
// THE SERVICE BOUNDARY EXCLUDES THE MULTIPLIERS. The final interpolation
// (d_off * dy per lane, the fx_mad of §3.15) is issued to the engine's
// vector multiplier bank through the mul port below, exactly as
// zhao_probe_dist_svc keeps the squares out: the measured ALM/M10K numbers
// describe the SERVICE, not a private copy of a bank the engine owns.
//
// MODES: CURVE (0) and DCURVE (1) only. SPLINE is COLD by the brief's own
// service split (section 6 "cold service lane": spline) and is not barreled.
//
// THE LAW (reference/include/zfield/zfield_steps.hpp, the one semantic
// layer; values decided by zfield::interpret in the differential):
//     clamped = clamp_raw(a, x[0], x[n-1])
//     i       = 6-step compare/select search, k = 5..0, on CLAMPED
//     CURVE   : dst = fx_mad(fx_sub(clamped, x[i]), dy[i], y[i])
//               (add-lane flag from the sub, mul-lane flag from the mad)
//     DCURVE  : dst = dy[i]   (no flags)
//
// TABLE LOAD CONTRACT (the active-program table cache side): a table's
// entries are written in ascending index order, one table at a time, then
// committed with its entry count. The commit latches the meta registers
// {n, x[0], x[n-1], y[0], dy[0]}. Loading a table while requests against
// THAT table are in flight is the loader's fault, exactly as FPLAN's
// tables-resident-with-plan rule already requires (FIELD.PROGCACHE.md).
module zhao_probe_curve_svc (
    input logic clk,
    input logic rst_n,

    // ---- table cache load port --------------------------------------------
    input logic               tl_we_i,
    input logic        [ 1:0] tl_tbl_i,
    input logic        [ 5:0] tl_idx_i,
    input logic signed [31:0] tl_x_i,
    input logic signed [31:0] tl_y_i,
    input logic signed [31:0] tl_dy_i,
    input logic               tl_commit_i,
    input logic        [ 6:0] tl_n_i,      // 2..64, decoder-enforced upstream

    // ---- request: one four-lane group -------------------------------------
    input  logic               req_valid_i,
    output logic               req_ready_o,
    // 0 = CURVE, 1 = DCURVE, 2 = SPLINE.
    //
    // SPLINE JOINED ON 2026-08-28 BY OWNER DECISION. The brief had it on the
    // cold lane -- exact, but not certified for the maximum live-field
    // workload -- and Fabian chose the expensive option: "so we spend work but
    // get a better thing. Non-issue. Spend the work."
    //
    // It costs a NEIGHBOUR PHASE. CURVE and DCURVE need the entry at the
    // segment, which the search already captures on the way down. SPLINE needs
    // three more y values per lane -- y[i-1], y[i+1], y[i+2] -- and that is 12
    // addresses over two ports, so six more cycles plus one to consume the
    // last read. The phase is entered ON MODE, so CURVE's fitted II is
    // untouched.
    input  logic         [1:0]  req_mode_i,
    input  logic        [ 1:0] req_tbl_i,
    input  logic signed [31:0] req_a_0_i,
    input  logic signed [31:0] req_a_1_i,
    input  logic signed [31:0] req_a_2_i,
    input  logic signed [31:0] req_a_3_i,
    input  logic        [ 7:0] req_tag_i,

    // ---- the vector multiplier bank (engine property, not probe silicon) --
    //
    // `mul_ready_i` IS THE PORT THIS SERVICE COULD NOT BE ATTACHED WITHOUT.
    //
    // The bank is shared and it can REFUSE. Until this port existed the
    // service issued into F_WAIT unconditionally and then waited for a
    // product that, when the request had been refused, was never started --
    // so the failure mode is not a wrong answer, it is a HANG: the finish
    // stage waits forever and the barrel behind it never drains.
    //
    // That is the same open loop the executor's DOT sequencer had, and the
    // same rule closes it: an instruction cannot advance past ISSUE until the
    // issue is GRANTED. The executor's version is the harder case (its
    // operands move); here the group is already parked in the finish
    // registers, so holding in F_ISSUE is the whole fix.
    output logic               mul_issue_o,
    output logic signed [32:0] mul_a_0_o,
    output logic signed [32:0] mul_a_1_o,
    output logic signed [32:0] mul_a_2_o,
    output logic signed [32:0] mul_a_3_o,
    output logic signed [32:0] mul_b_0_o,
    output logic signed [32:0] mul_b_1_o,
    output logic signed [32:0] mul_b_2_o,
    output logic signed [32:0] mul_b_3_o,
    input  logic               mul_ready_i,
    input  logic               mul_valid_i,
    // The bank lane is 66 bits (DOT3 sums three products); a curve consumes
    // one 32x32 product and reads the low 64 — a property of the op, not a
    // hole in the port (same note as zhao_field_curve).
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic signed [65:0] mul_p_0_i,
    input  logic signed [65:0] mul_p_1_i,
    input  logic signed [65:0] mul_p_2_i,
    input  logic signed [65:0] mul_p_3_i,
    /* verilator lint_on UNUSEDSIGNAL */

    // ---- reply: four results + per-lane flags, in accept order -------------
    output logic               rsp_valid_o,
    input  logic               rsp_ready_i,
    output logic signed [31:0] rsp_r_0_o,
    output logic signed [31:0] rsp_r_1_o,
    output logic signed [31:0] rsp_r_2_o,
    output logic signed [31:0] rsp_r_3_o,
    output logic        [ 3:0] rsp_sat_add_o,
    output logic        [ 3:0] rsp_sat_mul_o,
    output logic        [23:0] rsp_seg_o,    // 4 x 6-bit landed segment
    output logic        [ 7:0] rsp_tag_o
);

  localparam int LANES = 4;

  localparam logic [1:0] M_CURVE  = 2'd0;
  localparam logic [1:0] M_DCURVE = 2'd1;
  localparam logic [1:0] M_SPLINE = 2'd2;

  // ---- the neighbour phase, for SPLINE only --------------------------------
  //
  // The oracle's control points, with the ENDS REPLICATED rather than wrapped:
  //
  //     p0 = y[i > 0     ? i - 1 : 0    ]
  //     p1 = y[i]                            <- already captured, s_ye
  //     p2 = y[i + 1 < n ? i + 1 : n - 1]
  //     p3 = y[i + 2 < n ? i + 2 : n - 1]
  //
  // A TWO-ENTRY TABLE COLLAPSES ALL FOUR ONTO THE SAME PAIR, which is the case
  // worth writing the test for first: every clamp fires at once and any
  // off-by-one in the replication is visible in a single answer.
  //
  // Six address cycles, then one to consume the last registered read. Port A
  // serves lanes 0 and 1, port B lanes 2 and 3 -- the same interleave the
  // search uses, so one lane's read wait is its partner's address cycle.
  logic        [ 2:0] ncyc;
  logic               n_busy;
  logic signed [31:0] s_p0 [LANES];
  logic signed [31:0] s_p2 [LANES];
  logic signed [31:0] s_p3 [LANES];

  // ---- saturating primitives (the house forms, zhao_field_curve) ----------
  function automatic logic signed [31:0] sub_sat(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] d;
    begin
      d = $signed({a[31], a}) - $signed({b[31], b});
      if (d > 33'sd2147483647) sub_sat = 32'sh7FFF_FFFF;
      else if (d < -33'sd2147483648) sub_sat = 32'sh8000_0000;
      else sub_sat = d[31:0];
    end
  endfunction

  function automatic logic sub_fired(input logic signed [31:0] a, input logic signed [31:0] b);
    logic signed [32:0] d;
    begin
      d = $signed({a[31], a}) - $signed({b[31], b});
      sub_fired = (d > 33'sd2147483647) || (d < -33'sd2147483648);
    end
  endfunction

  function automatic logic signed [31:0] resc16(input logic signed [63:0] v);
    logic signed [64:0] r;
    begin
      r = (65'(v) + (65'sd1 <<< 15)) >>> 16;
      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;
      else if (r < -65'sd2147483648) resc16 = 32'sh8000_0000;
      else resc16 = r[31:0];
    end
  endfunction

  // SPLINE's segment parameter, straight off the oracle:
  //
  //     t = fx_clamp(rescale_s32(d_off * dy, 16), 0, 1<<16)
  //
  // The rescale is resc16's, so the two share their rounding by construction
  // rather than by two people writing the same expression twice. The clamp is
  // to the UNIT INTERVAL and it is not decoration: a point past the end of its
  // segment would otherwise extrapolate the cubic, and Catmull-Rom leaves the
  // hull fast.
  function automatic logic signed [31:0] spline_t(input logic signed [63:0] v);
    logic signed [31:0] r;
    begin
      r = resc16(v);
      if (r < 32'sd0)            spline_t = 32'sd0;
      else if (r > 32'sd65536)   spline_t = 32'sd65536;
      else                       spline_t = r;
    end
  endfunction

  function automatic logic resc16_fired(input logic signed [63:0] v);
    logic signed [64:0] r;
    begin
      r = (65'(v) + (65'sd1 <<< 15)) >>> 16;
      resc16_fired = (r > 65'sd2147483647) || (r < -65'sd2147483648);
    end
  endfunction

  function automatic logic signed [63:0] sx(input logic signed [31:0] v);
    sx = $signed({{32{v[31]}}, v});
  endfunction

  // ---- the table cache: 4 tables x 64 entries x {x,y,dy} ------------------
  // One array, one write, two synchronous reads — the two-read/one-write
  // shape Quartus implements as a replicated pair of simple-dual-port M10K
  // groups. Word layout: {x[95:64], y[63:32], dy[31:0]}.
  logic [95:0] tbl_ram[0:255];
  logic [ 7:0] ra_addr, rb_addr;
  logic [95:0] qa, qb;

  always_ff @(posedge clk) begin
    if (tl_we_i) tbl_ram[{tl_tbl_i, tl_idx_i}] <= {tl_x_i, tl_y_i, tl_dy_i};
    qa <= tbl_ram[ra_addr];
    qb <= tbl_ram[rb_addr];
  end

  // ---- per-table meta: the latched bounds + entry 0 -----------------------
  // Ascending-order load makes the LAST written x the x[n-1] the commit
  // latches; index-0 writes stage entry 0.
  logic        [ 6:0] meta_n  [4];
  logic signed [31:0] meta_x0 [4];
  logic signed [31:0] meta_xn1[4];
  logic signed [31:0] meta_y0 [4];
  logic signed [31:0] meta_dy0[4];
  logic signed [31:0] ld_x0, ld_y0, ld_dy0, ld_xlast;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int t = 0; t < 4; t++) begin
        meta_n[t]   <= 7'd2;
        meta_x0[t]  <= '0;
        meta_xn1[t] <= '0;
        meta_y0[t]  <= '0;
        meta_dy0[t] <= '0;
      end
      ld_x0    <= '0;
      ld_y0    <= '0;
      ld_dy0   <= '0;
      ld_xlast <= '0;
    end else begin
      if (tl_we_i) begin
        if (tl_idx_i == 6'd0) begin
          ld_x0  <= tl_x_i;
          ld_y0  <= tl_y_i;
          ld_dy0 <= tl_dy_i;
        end
        ld_xlast <= tl_x_i;
      end
      if (tl_commit_i) begin
        meta_n[tl_tbl_i]   <= tl_n_i;
        meta_x0[tl_tbl_i]  <= ld_x0;
        meta_xn1[tl_tbl_i] <= ld_xlast;
        meta_y0[tl_tbl_i]  <= ld_y0;
        meta_dy0[tl_tbl_i] <= ld_dy0;
      end
    end
  end

  // ---- accept: clamp at the door, from the meta registers -----------------
  // Law 2 of the curve block: the search runs on the CLAMPED value, and both
  // bounds come from the table's own ends — written in the reference's order
  // so even a malformed lo > hi table agrees.
  logic signed [31:0] req_a[LANES];
  assign req_a[0] = req_a_0_i;
  assign req_a[1] = req_a_1_i;
  assign req_a[2] = req_a_2_i;
  assign req_a[3] = req_a_3_i;

  logic signed [31:0] req_clamped[LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      req_clamped[l] = (req_a[l] < meta_x0[req_tbl_i]) ? meta_x0[req_tbl_i]
                     : ((req_a[l] > meta_xn1[req_tbl_i]) ? meta_xn1[req_tbl_i] : req_a[l]);
    end
  end

  // ---- one-deep staging, so a group is accepted while another searches ----
  logic               st_valid;
  logic        [ 1:0] st_mode;
  logic        [ 1:0] st_tbl;
  logic        [ 7:0] st_tag;
  logic        [ 6:0] st_nm1;
  logic signed [31:0] st_clamped[LANES];
  logic signed [31:0] st_x0, st_y0, st_dy0;

  assign req_ready_o = !st_valid;
  logic req_fire;
  assign req_fire = req_valid_i && req_ready_o;

  // ---- the search barrel --------------------------------------------------
  // cyc 0..12. Port A: address of lane 0 on even cyc 0..10, lane 1 on odd
  // 1..11; data consumed one cycle later. Port B identical for lanes 2/3.
  logic               s_busy, s_done;
  logic        [ 3:0] cyc;
  logic        [ 1:0] s_mode;
  logic        [ 1:0] s_tbl;
  logic        [ 7:0] s_tag;
  logic        [ 6:0] s_nm1;
  logic signed [31:0] s_clamped[LANES];
  logic        [ 6:0] s_lo     [LANES];
  logic        [ 2:0] s_k      [LANES];
  logic signed [31:0] s_xe     [LANES];
  logic signed [31:0] s_ye     [LANES];
  logic signed [31:0] s_dye    [LANES];

  logic [6:0] s_mid[LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) s_mid[l] = s_lo[l] + (7'd1 << s_k[l]);
  end

  // Which lane consumes read data THIS cycle (its address went out last
  // cycle): port A feeds lane 0 on odd cyc 1..11 and lane 1 on even cyc
  // 2..12; port B the same for lanes 2/3.
  logic consume[LANES];
  always_comb begin
    consume[0] = s_busy && !s_done && cyc[0] && (cyc <= 4'd11);
    consume[1] = s_busy && !s_done && !cyc[0] && (cyc >= 4'd2);
    consume[2] = consume[0];
    consume[3] = consume[1];
  end

  logic signed [31:0] rd_x[LANES];
  logic signed [31:0] rd_y[LANES];
  logic signed [31:0] rd_dy[LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      {rd_x[l], rd_y[l], rd_dy[l]} = (l < 2) ? qa : qb;
    end
  end

  // Post-consume ("upd") values: equal to the registers when the lane is not
  // consuming. The handoff at cyc 12 reads THESE, so the final consume of
  // lanes 1/3 needs no extra cycle — capture-on-taken, forwarded.
  logic        [ 6:0] upd_lo [LANES];
  logic signed [31:0] upd_xe [LANES];
  logic signed [31:0] upd_ye [LANES];
  logic signed [31:0] upd_dye[LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      upd_lo[l]  = s_lo[l];
      upd_xe[l]  = s_xe[l];
      upd_ye[l]  = s_ye[l];
      upd_dye[l] = s_dye[l];
      if (consume[l] && (s_mid[l] <= s_nm1) && (rd_x[l] <= s_clamped[l])) begin
        upd_lo[l]  = s_mid[l];
        upd_xe[l]  = rd_x[l];
        upd_ye[l]  = rd_y[l];
        upd_dye[l] = rd_dy[l];
      end
    end
  end

  // THE ENDS ARE REPLICATED, NOT WRAPPED, and this is the whole of it: i-1
  // clamps to 0 and i+1/i+2 clamp to n-1. A wrap would read the far end of the
  // table and produce a smooth, wrong curve -- the failure mode this family
  // keeps producing.
  function automatic logic [5:0] nb_idx(input logic [6:0] lo, input logic [1:0] k);
    logic signed [8:0] want;
    begin
      // k: 0 -> i-1, 1 -> i+1, 2 -> i+2
      want = (k == 2'd0) ? (9'(lo) - 9'sd1)
           : ((k == 2'd1) ? (9'(lo) + 9'sd1) : (9'(lo) + 9'sd2));
      if (want < 9'sd0)                nb_idx = 6'd0;
      else if (want > 9'(s_nm1))       nb_idx = s_nm1[5:0];
      else                             nb_idx = want[5:0];
    end
  endfunction

  // Which lane and which neighbour each port is addressing this cycle. The
  // schedule mirrors the search: port A alternates lanes 0/1, port B lanes
  // 2/3, so a lane's registered-read wait is its partner's address cycle and
  // both ports issue a useful address every cycle of the phase.
  logic [1:0] nb_k_c;
  logic       nb_l_c;
  assign nb_k_c = ncyc[2:1];
  assign nb_l_c = ncyc[0];

  // Read addresses: {table, mid} during the search, {table, neighbour} during
  // the neighbour phase. mid can exceed 63 for a small table; the wrapped
  // address reads a discarded word (the mid <= n-1 guard is on the CONSUME
  // side, exactly as in zhao_field_curve).
  always_comb begin
    if (n_busy) begin
      ra_addr = {s_tbl, nb_idx(s_lo[nb_l_c ? 1 : 0], nb_k_c)};
      rb_addr = {s_tbl, nb_idx(s_lo[nb_l_c ? 3 : 2], nb_k_c)};
    end else begin
      ra_addr = {s_tbl, cyc[0] ? s_mid[1][5:0] : s_mid[0][5:0]};
      rb_addr = {s_tbl, cyc[0] ? s_mid[3][5:0] : s_mid[2][5:0]};
    end
  end

  // ---- finish stage -------------------------------------------------------
  localparam logic [2:0] F_IDLE = 3'd0;
  localparam logic [2:0] F_ISSUE = 3'd1;
  localparam logic [2:0] F_WAIT = 3'd2;
  localparam logic [2:0] F_PUSH = 3'd3;

  // ---- SPLINE's own step ---------------------------------------------------
  //
  // THE ARITHMETIC IS NOT REBUILT HERE. `zhao_field_v3_spline` already does the
  // coefficients, the Horner and the Catmull-Rom half, closed at 21/21, and it
  // takes p0..p3 and t as operands precisely because the lookup was always
  // going to be a separate half. This block is that half, and F_SPL is where
  // the two meet.
  //
  // WHY IT LIVES INSIDE THIS SERVICE rather than beside it: the alternative is
  // publishing p0..p3 on the response, which is sixteen more 32-bit ports on a
  // block whose FIT NUMBERS are the reason it exists. Handing the maths the
  // control points internally keeps the response one value per lane, exactly
  // as CURVE leaves it.
  //
  // The multiplier is shared and the two never overlap: this block owns it in
  // F_ISSUE and F_WAIT for the single d_off*dy product, and the spline unit
  // owns it through F_SPL. That product is not wasted either -- t is
  // clamp(rescale(d_off*dy, 16), 0, 1) and CURVE's fx_mad needs the very same
  // multiply, so SPLINE pays for the lookup, not for the arithmetic.
  localparam logic [2:0] F_SPL = 3'd4;

  logic        [ 2:0] f_state;
  logic        [ 1:0] f_mode;
  logic        [ 7:0] f_tag;
  logic signed [31:0] f_doff   [LANES];
  logic signed [31:0] f_ye     [LANES];
  logic signed [31:0] f_dye    [LANES];
  logic signed [31:0] f_p0     [LANES];
  logic signed [31:0] f_p1     [LANES];
  logic signed [31:0] f_p2     [LANES];
  logic signed [31:0] f_p3     [LANES];
  logic signed [31:0] f_t      [LANES];
  logic        [ 5:0] f_seg    [LANES];
  logic        [ 3:0] f_sat_add;
  logic signed [31:0] f_res    [LANES];
  logic        [ 3:0] f_sat_mul;

  logic search_complete;
  assign search_complete = s_busy && ((cyc == 4'd12) || s_done);

  // SPLINE HANDS OFF ONE PHASE LATER. The search finding the segment is not
  // enough for it: p0, p2 and p3 are not in hand until the neighbour phase has
  // run. CURVE and DCURVE are unaffected -- they never enter it.
  logic lookup_complete;
  assign lookup_complete = (s_mode == M_SPLINE) ? (n_busy && (ncyc == 3'd6))
                                                : search_complete;

  logic handoff_fire;
  assign handoff_fire = lookup_complete && (f_state == F_IDLE);

  logic start_fire;
  assign start_fire = (st_valid || req_fire) && (!s_busy || handoff_fire);

  // Mul issue: one cycle, all four lanes at once.
  // ---- the spline unit, and the multiplier they share ----------------------
  //
  // THEY NEVER OVERLAP. This block owns the bank in F_ISSUE and F_WAIT for one
  // product; the spline unit owns it through F_SPL and nowhere else. The mux
  // is on `f_state` alone, so there is no arbitration to get wrong and no
  // priority to argue about.
  logic               spl_v_valid, spl_v_ready;
  logic               spl_r_valid;
  logic signed [31:0] spl_o0 [LANES];
  logic        [ 3:0] spl_sat_mul, spl_sat_add, spl_sat_resc;
  /* verilator lint_off UNUSEDSIGNAL */
  logic        [ 7:0] spl_tag_o_unused;
  logic        [ 3:0] spl_sat_add_unused, spl_sat_resc_unused;
  /* verilator lint_on UNUSEDSIGNAL */
  assign spl_sat_add_unused  = spl_sat_add;
  assign spl_sat_resc_unused = spl_sat_resc;

  logic               spl_mul_issue, spl_mul_ready, spl_mul_valid;
  logic signed [32:0] spl_a [LANES], spl_b [LANES];

  // Offered for exactly one clock when F_SPL is entered: the unit latches on
  // its own valid/ready, and re-offering a group it already took would send it
  // twice.
  logic f_spl_offered;
  assign spl_v_valid = (f_state == F_SPL) && !f_spl_offered;

  zhao_field_v3_spline u_spline (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(spl_v_valid), .v_ready_o(spl_v_ready),
      .t_0_i(f_t[0]), .t_1_i(f_t[1]), .t_2_i(f_t[2]), .t_3_i(f_t[3]),
      .p0_0_i(f_p0[0]), .p0_1_i(f_p0[1]), .p0_2_i(f_p0[2]), .p0_3_i(f_p0[3]),
      .p1_0_i(f_p1[0]), .p1_1_i(f_p1[1]), .p1_2_i(f_p1[2]), .p1_3_i(f_p1[3]),
      .p2_0_i(f_p2[0]), .p2_1_i(f_p2[1]), .p2_2_i(f_p2[2]), .p2_3_i(f_p2[3]),
      .p3_0_i(f_p3[0]), .p3_1_i(f_p3[1]), .p3_2_i(f_p3[2]), .p3_3_i(f_p3[3]),
      .tag_i(f_tag),
      .r_valid_o(spl_r_valid), .r_ready_i(1'b1),
      .o0_0_o(spl_o0[0]), .o0_1_o(spl_o0[1]), .o0_2_o(spl_o0[2]), .o0_3_o(spl_o0[3]),
      .sat_mul_o(spl_sat_mul), .sat_add_o(spl_sat_add), .sat_rescale_o(spl_sat_resc),
      .tag_o(spl_tag_o_unused),
      .mul_issue_o(spl_mul_issue), .mul_ready_i(spl_mul_ready),
      .mul_a_0_o(spl_a[0]), .mul_a_1_o(spl_a[1]),
      .mul_a_2_o(spl_a[2]), .mul_a_3_o(spl_a[3]),
      .mul_b_0_o(spl_b[0]), .mul_b_1_o(spl_b[1]),
      .mul_b_2_o(spl_b[2]), .mul_b_3_o(spl_b[3]),
      .mul_valid_i(spl_mul_valid),
      .mul_p_0_i(mul_p_0_i), .mul_p_1_i(mul_p_1_i),
      .mul_p_2_i(mul_p_2_i), .mul_p_3_i(mul_p_3_i)
  );

  assign spl_mul_ready = mul_ready_i && (f_state == F_SPL);
  assign spl_mul_valid = mul_valid_i && (f_state == F_SPL);

  assign mul_issue_o = (f_state == F_ISSUE) || ((f_state == F_SPL) && spl_mul_issue);
  // THE OPERANDS ARE MUXED TOO, not just the issue line. Muxing only
  // `mul_issue_o` would let the spline unit ask for products computed from
  // CURVE's d_off and dy -- a request it never made, answered with numbers
  // that are somebody else's. The linter found this by noticing spl_a and
  // spl_b were declared and never read.
  assign mul_a_0_o   = (f_state == F_SPL) ? spl_a[0]
                       : $signed({f_doff[0][31], f_doff[0]});
  assign mul_a_1_o   = (f_state == F_SPL) ? spl_a[1]
                       : $signed({f_doff[1][31], f_doff[1]});
  assign mul_a_2_o   = (f_state == F_SPL) ? spl_a[2]
                       : $signed({f_doff[2][31], f_doff[2]});
  assign mul_a_3_o   = (f_state == F_SPL) ? spl_a[3]
                       : $signed({f_doff[3][31], f_doff[3]});
  assign mul_b_0_o   = (f_state == F_SPL) ? spl_b[0]
                       : $signed({f_dye[0][31], f_dye[0]});
  assign mul_b_1_o   = (f_state == F_SPL) ? spl_b[1]
                       : $signed({f_dye[1][31], f_dye[1]});
  assign mul_b_2_o   = (f_state == F_SPL) ? spl_b[2]
                       : $signed({f_dye[2][31], f_dye[2]});
  assign mul_b_3_o   = (f_state == F_SPL) ? spl_b[3]
                       : $signed({f_dye[3][31], f_dye[3]});

  logic signed [63:0] mul_p[LANES];
  assign mul_p[0] = $signed(mul_p_0_i[63:0]);
  assign mul_p[1] = $signed(mul_p_1_i[63:0]);
  assign mul_p[2] = $signed(mul_p_2_i[63:0]);
  assign mul_p[3] = $signed(mul_p_3_i[63:0]);

  // curve_p per lane: d_off*dy + (y << 16), the fx_mad's wide sum. The
  // product magnitude is < 2^62 and the shifted y < 2^47, so s64 holds it —
  // the same width argument zhao_field_curve makes.
  logic signed [63:0] curve_p[LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) curve_p[l] = mul_p[l] + (sx(f_ye[l]) <<< 16);
  end

  logic push_fire;
  assign push_fire = (f_state == F_PUSH) && (!rsp_valid_o || rsp_ready_i);

  logic signed [31:0] rsp_r[LANES];
  assign rsp_r_0_o = rsp_r[0];
  assign rsp_r_1_o = rsp_r[1];
  assign rsp_r_2_o = rsp_r[2];
  assign rsp_r_3_o = rsp_r[3];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_valid <= 1'b0;
      st_mode  <= 2'd0;
      st_tbl   <= '0;
      st_tag   <= '0;
      st_nm1   <= '0;
      st_x0    <= '0;
      st_y0    <= '0;
      st_dy0   <= '0;
      for (int l = 0; l < LANES; l++) begin
        st_clamped[l] <= '0;
        s_clamped[l]  <= '0;
        s_lo[l]       <= '0;
        s_k[l]        <= 3'd5;
        s_xe[l]       <= '0;
        s_ye[l]       <= '0;
        s_dye[l]      <= '0;
        f_doff[l]     <= '0;
        f_ye[l]       <= '0;
        f_dye[l]      <= '0;
        f_seg[l]      <= '0;
        f_res[l]      <= '0;
        rsp_r[l]      <= '0;
      end
      s_busy    <= 1'b0;
      s_done    <= 1'b0;
      cyc       <= '0;
      s_mode    <= 2'd0;
      ncyc      <= 3'd0;
      n_busy    <= 1'b0;
      for (int l = 0; l < LANES; l++) begin
        s_p0[l] <= '0;
        s_p2[l] <= '0;
        s_p3[l] <= '0;
      end
      s_tbl     <= '0;
      s_tag     <= '0;
      s_nm1     <= '0;
      f_state   <= F_IDLE;
      f_mode    <= 2'd0;
      f_spl_offered <= 1'b0;
      f_tag     <= '0;
      f_sat_add <= '0;
      f_sat_mul <= '0;
      rsp_valid_o   <= 1'b0;
      rsp_sat_add_o <= '0;
      rsp_sat_mul_o <= '0;
      rsp_seg_o     <= '0;
      rsp_tag_o     <= '0;
    end else begin
      // ---- search advance ------------------------------------------------
      if (s_busy && !s_done) begin
        for (int l = 0; l < LANES; l++) begin
          s_lo[l]  <= upd_lo[l];
          s_xe[l]  <= upd_xe[l];
          s_ye[l]  <= upd_ye[l];
          s_dye[l] <= upd_dye[l];
          if (consume[l]) s_k[l] <= s_k[l] - 3'd1;
        end
        if (cyc == 4'd12) begin
          // SPLINE TURNS THE CORNER HERE rather than handing off. The segment
          // is known, so its neighbours are addressable; nothing else is.
          if (s_mode == M_SPLINE) begin
            if (!n_busy) begin
              n_busy <= 1'b1;
              ncyc   <= 3'd0;
            end
          end else if (!handoff_fire) begin
            s_done <= 1'b1;
          end
        end else begin
          cyc <= cyc + 4'd1;
        end
      end

      // ---- the neighbour phase ---------------------------------------------
      //
      // Address on cycles 0..5, and the registered read means the datum for
      // cycle t lands at t+1 -- so captures run 1..6 and the phase is seven
      // cycles, not six. That one extra cycle is the same "the thirteenth
      // cycle consumes the final read" the search already pays.
      if (n_busy) begin
        if (ncyc != 3'd6) ncyc <= ncyc + 3'd1;

        if (ncyc != 3'd0) begin
          // What was addressed one cycle ago.
          automatic logic [2:0] prev  = ncyc - 3'd1;
          automatic logic [1:0] k_was = prev[2:1];
          automatic logic       l_was = prev[0];
          automatic logic [1:0] la    = l_was ? 2'd1 : 2'd0;
          automatic logic [1:0] lb    = l_was ? 2'd3 : 2'd2;
          case (k_was)
            2'd0: begin s_p0[la] <= rd_y[la]; s_p0[lb] <= rd_y[lb]; end
            2'd1: begin s_p2[la] <= rd_y[la]; s_p2[lb] <= rd_y[lb]; end
            default: begin s_p3[la] <= rd_y[la]; s_p3[lb] <= rd_y[lb]; end
          endcase
        end
      end

      // ---- handoff to finish ----------------------------------------------
      if (handoff_fire) begin
        for (int l = 0; l < LANES; l++) begin
          f_doff[l] <= sub_sat(s_clamped[l], upd_xe[l]);
          f_ye[l]   <= upd_ye[l];
          f_dye[l]  <= upd_dye[l];
          f_seg[l]  <= upd_lo[l][5:0];
          // p1 is the entry the search already captured; the other three came
          // out of the neighbour phase.
          f_p0[l]   <= s_p0[l];
          f_p1[l]   <= upd_ye[l];
          f_p2[l]   <= s_p2[l];
          f_p3[l]   <= s_p3[l];
          f_sat_add[l] <= sub_fired(s_clamped[l], upd_xe[l]);
          // DCURVE reads the slope and is done: no product, no lane, no wait.
          f_res[l] <= upd_dye[l];
        end
        f_mode    <= s_mode;
        f_tag     <= s_tag;
        n_busy    <= 1'b0;
        ncyc      <= 3'd0;
        f_sat_mul <= '0;
        f_state   <= ((s_mode == M_CURVE) || (s_mode == M_SPLINE)) ? F_ISSUE : F_PUSH;
        f_spl_offered <= 1'b0;
        if (s_mode == M_DCURVE) f_sat_add <= '0;  // DCURVE records nothing
      end

      // ---- search start / staging ----------------------------------------
      if (start_fire) begin
        s_busy <= 1'b1;
        s_done <= 1'b0;
        cyc    <= '0;
        if (st_valid) begin
          s_mode <= st_mode;
          s_tbl  <= st_tbl;
          s_tag  <= st_tag;
          s_nm1  <= st_nm1;
          for (int l = 0; l < LANES; l++) begin
            s_clamped[l] <= st_clamped[l];
            s_lo[l]      <= '0;
            s_k[l]       <= 3'd5;
            s_xe[l]      <= st_x0;
            s_ye[l]      <= st_y0;
            s_dye[l]     <= st_dy0;
          end
        end else begin
          s_mode <= req_mode_i;
          s_tbl  <= req_tbl_i;
          s_tag  <= req_tag_i;
          s_nm1  <= meta_n[req_tbl_i] - 7'd1;
          for (int l = 0; l < LANES; l++) begin
            s_clamped[l] <= req_clamped[l];
            s_lo[l]      <= '0;
            s_k[l]       <= 3'd5;
            s_xe[l]      <= meta_x0[req_tbl_i];
            s_ye[l]      <= meta_y0[req_tbl_i];
            s_dye[l]     <= meta_dy0[req_tbl_i];
          end
        end
      end else if (handoff_fire) begin
        s_busy <= 1'b0;
        s_done <= 1'b0;
      end

      // staging bookkeeping: a fired request either starts the search
      // directly (staging empty and the barrel free) or parks in staging.
      if (req_fire && !(start_fire && !st_valid)) begin
        st_valid <= 1'b1;
        st_mode  <= req_mode_i;
        st_tbl   <= req_tbl_i;
        st_tag   <= req_tag_i;
        st_nm1   <= meta_n[req_tbl_i] - 7'd1;
        st_x0    <= meta_x0[req_tbl_i];
        st_y0    <= meta_y0[req_tbl_i];
        st_dy0   <= meta_dy0[req_tbl_i];
        for (int l = 0; l < LANES; l++) st_clamped[l] <= req_clamped[l];
      end else if (start_fire && st_valid) begin
        st_valid <= 1'b0;
      end

      // ---- finish FSM ------------------------------------------------------
      case (f_state)
        // HOLD UNTIL GRANTED. `mul_issue_o` stays asserted across the refusal
        // -- it is `f_state == F_ISSUE` -- and the operands are already in
        // f_doff/f_dye, which do not move, so the retry costs a clock and
        // nothing else. Advancing here on a refused request is what makes
        // F_WAIT wait for a product nobody started.
        F_ISSUE: if (mul_ready_i) f_state <= F_WAIT;
        F_WAIT: begin
          if (mul_valid_i) begin
            for (int l = 0; l < LANES; l++) begin
              f_res[l] <= resc16(curve_p[l]);
              f_sat_mul[l] <= resc16_fired(curve_p[l]);
              // t = clamp(rescale(d_off * dy, 16), 0, 1). THE SAME PRODUCT the
              // line above rescales for CURVE -- mul_p is d_off*dy either way;
              // CURVE adds y<<16 first and SPLINE clamps to the unit interval
              // instead.
              f_t[l] <= spline_t(mul_p[l]);
            end
            f_state <= (f_mode == M_SPLINE) ? F_SPL : F_PUSH;
          end
        end
        // The spline unit has the control points and the parameter; wait for
        // its answer and then push it like any other.
        F_SPL: begin
          if (spl_v_valid && spl_v_ready) f_spl_offered <= 1'b1;
          if (spl_r_valid) begin
          for (int l = 0; l < LANES; l++) f_res[l] <= spl_o0[l];
            f_sat_mul <= spl_sat_mul;
            f_state   <= F_PUSH;
          end
        end
        F_PUSH: begin
          if (!rsp_valid_o || rsp_ready_i) begin
            for (int l = 0; l < LANES; l++) rsp_r[l] <= f_res[l];
            rsp_sat_add_o <= (f_mode == M_DCURVE) ? 4'b0 : f_sat_add;
            rsp_sat_mul_o <= (f_mode == M_DCURVE) ? 4'b0 : f_sat_mul;
            rsp_seg_o     <= {f_seg[3], f_seg[2], f_seg[1], f_seg[0]};
            rsp_tag_o     <= f_tag;
            f_state       <= F_IDLE;
          end
        end
        default: ;  // F_IDLE: fed by the handoff above
      endcase

      if (rsp_valid_o && rsp_ready_i && !push_fire) rsp_valid_o <= 1'b0;
      else if (push_fire) rsp_valid_o <= 1'b1;
    end
  end

endmodule : zhao_probe_curve_svc
