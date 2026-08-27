// zhao_probe_patch_acc.sv — Field v3 decisive probe 5 (reports/Fieldv3.md
// Phase 3): the four-bank patch accumulator with exact command-order
// reducers (the TERRAIN.PATCH field-major amendment, 2026-08-27).
//
// TARGET: one four-vertex vector update lands per clock, sustained, with
// the brief's ~16-20 M10K budget for one patch in flight. A probe that
// misses kills or changes this topology BEFORE it contaminates the engine.
// ENFORCED-BY: tests/differential/field_patch_acc_directed.cpp:main
//
// WHAT IT IS: the patch scratch of TERRAIN.PATCH's field-major reducer —
// four M10K banks indexed by vertex mod 4, so the four results of one
// four-point vector group land in four DIFFERENT banks in one clock, at
// four rotated addresses (consecutive vertices share no bank). Each bank
// holds one 273-deep RAM per Earth output lane (1,089 vertices / 4, 32 b):
// height, velocity, material, nav_cost — 16 RAMs total, the brief's
// "16-20 M10K depending on packed widths" priced at the unpacked end.
//
// EACH OUTPUT FOLLOWS ITS OWN REDUCER LAW — explicitly NOT one generic
// "add all four outputs" block (TERRAIN.PATCH amendment table):
//
//   height    command-ordered saturating fx_add, ONE add at a time, never
//             wide-accumulate-then-narrow (§3.4; oracle
//             zref::terrain::compose_vertex), init = compose_top =
//             max(fx(base) + fx(scar), fx(bottom) when dual), final
//             re-clamp at bottom on DRAIN.
//   velocity  command-ordered saturating fx_add chain, init 0 — the
//             TERRAIN.VELOCITY V1 law (chosen there 2026-08-2x: the SAME
//             reduction §3.4 uses, so one evaluation is never reduced by
//             two different rules). The height16 bake-back rounding is
//             TERRAIN.VELOCITY's, downstream of this seam.
//   material  writer-selection: the LAST field in COMMAND ORDER that
//             covers the vertex AND writes the material lane wins,
//             wholesale (u32, opaque per domains-and-effects §5); init 0.
//             DECLARED HERE, chosen not found: FIELD.SEQ.EARTH and the
//             TERRAIN.PATCH amendment both NAME an "exact
//             writer-selection law" but no document declares one — this
//             is its first written form, recorded for negotiation
//             (design/contracts/FIELD.SEQ.EARTH.md carries the note).
//   nav_cost  command-ordered saturating fx_add chain, init 0 — DECLARED
//             HERE by the same one-reduction argument TERRAIN.VELOCITY V1
//             makes; recorded for negotiation alongside material.
//
// A field's WRITE MASK (from the FPLAN output map) gates each lane: a
// program that does not write an output leaves that accumulator lane
// untouched — not "adds zero", which for material would be a wrong write
// and for the fx lanes an identical value but a DIFFERENT claim about the
// I/O map.
// ENFORCED-BY: tests/differential/field_patch_acc_directed.cpp:main
//
// THE RMW IS PIPELINED, ONE UPDATE PER CLOCK, WITH A ONE-DEEP BYPASS: the
// registered M10K read means an update accepted at T writes at T+1, so an
// update at T+1 hitting the SAME bank+address must take the T+1 write
// instead of its (stale) read — per output lane, gated by the writer's
// mask. Field-major walks revisit a vertex only across fields, but
// correctness must not depend on the walker's spacing.
// ENFORCED-BY: tests/differential/field_patch_acc_directed.cpp:main
//
// PHASES: INIT (one aligned group per clock: load compose_top, zero the
// other lanes), ACCUM (unaligned vector updates, one per clock), DRAIN
// (one aligned group per clock: final bottom clamp, bottom lane, dirty
// bit, all four outputs; result two cycles later, no backpressure — the
// intended consumer is the composed-height cache write port, a plain
// one-group-per-clock sink). EXPLICIT ASSUMPTION, upheld by the caller,
// not enforced here: the host sequencer (Phase 4's patch sequencer; today
// every driver in tests/differential/field_patch_acc_directed.cpp) raises
// at most one of the three valid inputs at a time and leaves two idle
// cycles between phase changes, matching the brief's per-patch
// init/compose/finalize cost of 2 x 273 clocks.
//
// WHAT IS DELIBERATELY OUTSIDE: the §9.1 intake (rectangles live in
// TERRAIN.PATCH's as-built RTL), the footprint test (the Earth walker
// walks only covered vertices — coverage is geometric at this seam), the
// program evaluation (FIELD.SEQ.EARTH), and the height16 bake-backs.
module zhao_probe_patch_acc (
    input logic clk,
    input logic rst_n,

    // ---- INIT: one aligned four-vertex group per clock ---------------------
    input logic               in_valid_i,
    input logic        [ 8:0] in_g_i,        // group index 0..272; vertex = 4g+l
    input logic        [ 3:0] in_mask_i,
    input logic signed [15:0] in_base_0_i,
    input logic signed [15:0] in_base_1_i,
    input logic signed [15:0] in_base_2_i,
    input logic signed [15:0] in_base_3_i,
    input logic signed [15:0] in_scar_0_i,
    input logic signed [15:0] in_scar_1_i,
    input logic signed [15:0] in_scar_2_i,
    input logic signed [15:0] in_scar_3_i,
    input logic signed [15:0] in_bot_0_i,
    input logic signed [15:0] in_bot_1_i,
    input logic signed [15:0] in_bot_2_i,
    input logic signed [15:0] in_bot_3_i,
    input logic        [ 3:0] in_dual_i,

    // ---- ACCUM: one (possibly unaligned) vector update per clock -----------
    input logic               up_valid_i,
    input logic        [10:0] up_iv_i,       // base vertex; lane l is vertex iv+l
    input logic        [ 3:0] up_mask_i,     // lane valid
    input logic        [ 3:0] up_wmask_i,    // {nav, mat, vel, height} written
    input logic signed [31:0] up_h_0_i,
    input logic signed [31:0] up_h_1_i,
    input logic signed [31:0] up_h_2_i,
    input logic signed [31:0] up_h_3_i,
    input logic signed [31:0] up_v_0_i,
    input logic signed [31:0] up_v_1_i,
    input logic signed [31:0] up_v_2_i,
    input logic signed [31:0] up_v_3_i,
    input logic        [31:0] up_m_0_i,
    input logic        [31:0] up_m_1_i,
    input logic        [31:0] up_m_2_i,
    input logic        [31:0] up_m_3_i,
    input logic signed [31:0] up_n_0_i,
    input logic signed [31:0] up_n_1_i,
    input logic signed [31:0] up_n_2_i,
    input logic signed [31:0] up_n_3_i,

    // Saturation pulses, LANE-ordered, one cycle set per accepted update
    // (two cycles after accept). The ledger lanes stay apart: height and
    // velocity/nav each record their own adds (qformats §5 mirrors).
    output logic        sat_valid_o,
    output logic [3:0]  sat_h_o,
    output logic [3:0]  sat_v_o,
    output logic [3:0]  sat_n_o,

    // ---- DRAIN: one aligned group per clock; result two cycles later -------
    input  logic               dr_valid_i,
    input  logic        [ 8:0] dr_g_i,
    input  logic        [ 3:0] dr_mask_i,
    input  logic signed [15:0] dr_base_0_i,
    input  logic signed [15:0] dr_base_1_i,
    input  logic signed [15:0] dr_base_2_i,
    input  logic signed [15:0] dr_base_3_i,
    input  logic signed [15:0] dr_bot_0_i,
    input  logic signed [15:0] dr_bot_1_i,
    input  logic signed [15:0] dr_bot_2_i,
    input  logic signed [15:0] dr_bot_3_i,
    input  logic        [ 3:0] dr_dual_i,

    output logic               out_valid_o,
    output logic        [ 8:0] out_g_o,
    output logic        [ 3:0] out_mask_o,
    output logic        [ 3:0] out_dirty_o,
    output logic signed [31:0] out_top_0_o,
    output logic signed [31:0] out_top_1_o,
    output logic signed [31:0] out_top_2_o,
    output logic signed [31:0] out_top_3_o,
    output logic signed [31:0] out_bot_0_o,
    output logic signed [31:0] out_bot_1_o,
    output logic signed [31:0] out_bot_2_o,
    output logic signed [31:0] out_bot_3_o,
    output logic signed [31:0] out_vel_0_o,
    output logic signed [31:0] out_vel_1_o,
    output logic signed [31:0] out_vel_2_o,
    output logic signed [31:0] out_vel_3_o,
    output logic        [31:0] out_mat_0_o,
    output logic        [31:0] out_mat_1_o,
    output logic        [31:0] out_mat_2_o,
    output logic        [31:0] out_mat_3_o,
    output logic signed [31:0] out_nav_0_o,
    output logic signed [31:0] out_nav_1_o,
    output logic signed [31:0] out_nav_2_o,
    output logic signed [31:0] out_nav_3_o
);

  localparam int BANKS = 4;

  // wmask bit positions
  localparam int W_H = 0;
  localparam int W_V = 1;
  localparam int W_M = 2;
  localparam int W_N = 3;

  // ---- the §3 saturating add: 33 bits, narrowed, ONE add at a time --------
  function automatic logic signed [31:0] fx_add_sat(input logic signed [31:0] a,
                                                    input logic signed [31:0] b);
    logic signed [32:0] s;
    begin
      s = $signed({a[31], a}) + $signed({b[31], b});
      if (s > 33'sd2147483647) fx_add_sat = 32'sh7FFF_FFFF;
      else if (s < -33'sd2147483648) fx_add_sat = 32'sh8000_0000;
      else fx_add_sat = s[31:0];
    end
  endfunction

  function automatic logic fx_add_fired(input logic signed [31:0] a, input logic signed [31:0] b);
    logic signed [32:0] s;
    begin
      s = $signed({a[31], a}) + $signed({b[31], b});
      fx_add_fired = (s > 33'sd2147483647) || (s < -33'sd2147483648);
    end
  endfunction

  // ---- input lane bundles --------------------------------------------------
  logic signed [15:0] in_base[BANKS];
  logic signed [15:0] in_scar[BANKS];
  logic signed [15:0] in_bot[BANKS];
  assign in_base[0] = in_base_0_i;
  assign in_base[1] = in_base_1_i;
  assign in_base[2] = in_base_2_i;
  assign in_base[3] = in_base_3_i;
  assign in_scar[0] = in_scar_0_i;
  assign in_scar[1] = in_scar_1_i;
  assign in_scar[2] = in_scar_2_i;
  assign in_scar[3] = in_scar_3_i;
  assign in_bot[0] = in_bot_0_i;
  assign in_bot[1] = in_bot_1_i;
  assign in_bot[2] = in_bot_2_i;
  assign in_bot[3] = in_bot_3_i;

  logic signed [31:0] up_h[BANKS];
  logic signed [31:0] up_v[BANKS];
  logic [31:0] up_m[BANKS];
  logic signed [31:0] up_n[BANKS];
  assign up_h[0] = up_h_0_i;
  assign up_h[1] = up_h_1_i;
  assign up_h[2] = up_h_2_i;
  assign up_h[3] = up_h_3_i;
  assign up_v[0] = up_v_0_i;
  assign up_v[1] = up_v_1_i;
  assign up_v[2] = up_v_2_i;
  assign up_v[3] = up_v_3_i;
  assign up_m[0] = up_m_0_i;
  assign up_m[1] = up_m_1_i;
  assign up_m[2] = up_m_2_i;
  assign up_m[3] = up_m_3_i;
  assign up_n[0] = up_n_0_i;
  assign up_n[1] = up_n_1_i;
  assign up_n[2] = up_n_2_i;
  assign up_n[3] = up_n_3_i;

  logic signed [15:0] dr_base[BANKS];
  logic signed [15:0] dr_bot[BANKS];
  assign dr_base[0] = dr_base_0_i;
  assign dr_base[1] = dr_base_1_i;
  assign dr_base[2] = dr_base_2_i;
  assign dr_base[3] = dr_base_3_i;
  assign dr_bot[0] = dr_bot_0_i;
  assign dr_bot[1] = dr_bot_1_i;
  assign dr_bot[2] = dr_bot_2_i;
  assign dr_bot[3] = dr_bot_3_i;

  // ---- INIT compose_top: max(fx(base)+fx(scar), fx(bottom) when dual) -----
  // height16 -> fx16 is the EXACT raw << 8 (qformats §2/§9, no rounding);
  // the add is the faithful saturating form even though s17 cannot fire.
  // Declarations hoisted out of the unnamed blocks: Quartus 17 rejects
  // block-local declarations the same way it rejects inline genvars.
  logic signed [31:0] init_t[BANKS];
  logic signed [31:0] init_botfx[BANKS];
  logic signed [31:0] init_top[BANKS];
  always_comb begin
    for (int b = 0; b < BANKS; b++) begin
      init_t[b] = fx_add_sat(32'($signed(in_base[b])) <<< 8, 32'($signed(in_scar[b])) <<< 8);
      init_botfx[b] = 32'($signed(in_bot[b])) <<< 8;
      init_top[b] = (in_dual_i[b] && (init_t[b] < init_botfx[b])) ? init_botfx[b] : init_t[b];
    end
  end

  // ---- ACCUM routing: bank b serves lane rot = (b - iv[1:0]) mod 4 --------
  logic [ 1:0] up_rot[BANKS];  // which LANE lands in bank b
  logic [10:0] up_vtx[BANKS];
  logic [ 8:0] up_addr[BANKS];
  logic        up_act[BANKS];
  always_comb begin
    for (int b = 0; b < BANKS; b++) begin
      up_rot[b]  = 2'(b) - up_iv_i[1:0];
      up_vtx[b]  = up_iv_i + {9'd0, up_rot[b]};
      up_addr[b] = up_vtx[b][10:2];
      up_act[b]  = up_valid_i && up_mask_i[up_rot[b]];
    end
  end

  // ---- the sixteen RAMs: bank x {height, vel, mat, nav} -------------------
  // Synchronous read, no reset on the array — the M10K-inferring shape.
  logic [31:0] ram_h[BANKS][0:272];
  logic [31:0] ram_v[BANKS][0:272];
  logic [31:0] ram_m[BANKS][0:272];
  logic [31:0] ram_n[BANKS][0:272];
  logic [31:0] q_h[BANKS], q_v[BANKS], q_m[BANKS], q_n[BANKS];

  // Read address: ACCUM has it when active, DRAIN otherwise (phases are
  // mutually exclusive by contract).
  logic [8:0] raddr[BANKS];
  always_comb begin
    for (int b = 0; b < BANKS; b++) raddr[b] = up_valid_i ? up_addr[b] : dr_g_i;
  end

  // ---- M stage: the registered half of the RMW ----------------------------
  logic               m_valid;
  logic        [ 3:0] m_wmask;
  logic        [ 1:0] m_rotl;              // iv[1:0]: lane l sits in bank (l+rotl)
  logic               m_act   [BANKS];
  logic        [ 8:0] m_addr  [BANKS];
  logic signed [31:0] m_h     [BANKS];
  logic signed [31:0] m_v     [BANKS];
  logic        [31:0] m_m     [BANKS];
  logic signed [31:0] m_n     [BANKS];

  // One-deep write bypass per bank: the write that happened LAST cycle.
  // Material carries NO bypass because its reducer never reads the old
  // value — the last writer wins wholesale, so a back-to-back rewrite is
  // simply the later write.
  logic               by_valid[BANKS];
  logic        [ 8:0] by_addr [BANKS];
  logic               by_wh   [BANKS];
  logic               by_wv   [BANKS];
  logic               by_wn   [BANKS];
  logic signed [31:0] by_h    [BANKS];
  logic signed [31:0] by_v    [BANKS];
  logic signed [31:0] by_n    [BANKS];

  // Old values with the bypass applied, per output lane.
  logic signed [31:0] old_h[BANKS];
  logic signed [31:0] old_v[BANKS];
  logic signed [31:0] old_n[BANKS];
  logic               by_hit[BANKS];
  always_comb begin
    for (int b = 0; b < BANKS; b++) begin
      by_hit[b] = by_valid[b] && (by_addr[b] == m_addr[b]);
      old_h[b] = (by_hit[b] && by_wh[b]) ? by_h[b] : $signed(q_h[b]);
      old_v[b] = (by_hit[b] && by_wv[b]) ? by_v[b] : $signed(q_v[b]);
      old_n[b] = (by_hit[b] && by_wn[b]) ? by_n[b] : $signed(q_n[b]);
    end
  end

  // The four reducers, each its own law.
  logic signed [31:0] new_h[BANKS];
  logic signed [31:0] new_v[BANKS];
  logic        [31:0] new_m[BANKS];
  logic signed [31:0] new_n[BANKS];
  logic               satb_h[BANKS], satb_v[BANKS], satb_n[BANKS];
  always_comb begin
    for (int b = 0; b < BANKS; b++) begin
      new_h[b]  = fx_add_sat(old_h[b], m_h[b]);
      satb_h[b] = fx_add_fired(old_h[b], m_h[b]);
      new_v[b]  = fx_add_sat(old_v[b], m_v[b]);
      satb_v[b] = fx_add_fired(old_v[b], m_v[b]);
      new_m[b]  = m_m[b];  // writer-selection: this writer wins wholesale
      new_n[b]  = fx_add_sat(old_n[b], m_n[b]);
      satb_n[b] = fx_add_fired(old_n[b], m_n[b]);
    end
  end

  // ---- DRAIN pipeline ------------------------------------------------------
  logic               d1_valid;
  logic        [ 8:0] d1_g;
  logic        [ 3:0] d1_mask;
  logic        [ 3:0] d1_dual;
  logic signed [15:0] d1_base[BANKS];
  logic signed [15:0] d1_bot [BANKS];

  // live_top = max(acc, bottom) on a dual page: the ONE final clamp after
  // the command-order chain (§3.4) — a transient wave can never punch below
  // the underside.
  logic signed [31:0] dr_botfx[BANKS];
  logic signed [31:0] dr_live [BANKS];
  always_comb begin
    for (int b = 0; b < BANKS; b++) begin
      dr_botfx[b] = 32'($signed(d1_bot[b])) <<< 8;
      dr_live[b] = (d1_dual[b] && ($signed(q_h[b]) < dr_botfx[b])) ? dr_botfx[b] : $signed(q_h[b]);
    end
  end

  logic signed [31:0] out_top[BANKS];
  logic signed [31:0] out_bot[BANKS];
  logic signed [31:0] out_vel[BANKS];
  logic        [31:0] out_mat[BANKS];
  logic signed [31:0] out_nav[BANKS];
  assign out_top_0_o = out_top[0];
  assign out_top_1_o = out_top[1];
  assign out_top_2_o = out_top[2];
  assign out_top_3_o = out_top[3];
  assign out_bot_0_o = out_bot[0];
  assign out_bot_1_o = out_bot[1];
  assign out_bot_2_o = out_bot[2];
  assign out_bot_3_o = out_bot[3];
  assign out_vel_0_o = out_vel[0];
  assign out_vel_1_o = out_vel[1];
  assign out_vel_2_o = out_vel[2];
  assign out_vel_3_o = out_vel[3];
  assign out_mat_0_o = out_mat[0];
  assign out_mat_1_o = out_mat[1];
  assign out_mat_2_o = out_mat[2];
  assign out_mat_3_o = out_mat[3];
  assign out_nav_0_o = out_nav[0];
  assign out_nav_1_o = out_nav[1];
  assign out_nav_2_o = out_nav[2];
  assign out_nav_3_o = out_nav[3];

  // ---- the RAM processes ---------------------------------------------------
  always_ff @(posedge clk) begin
    for (int b = 0; b < BANKS; b++) begin
      // write: INIT loads, or the M-stage writeback (lane-masked)
      if (in_valid_i && in_mask_i[b]) begin
        ram_h[b][in_g_i] <= $unsigned(init_top[b]);
        ram_v[b][in_g_i] <= 32'd0;
        ram_m[b][in_g_i] <= 32'd0;
        ram_n[b][in_g_i] <= 32'd0;
      end else if (m_valid && m_act[b]) begin
        if (m_wmask[W_H]) ram_h[b][m_addr[b]] <= $unsigned(new_h[b]);
        if (m_wmask[W_V]) ram_v[b][m_addr[b]] <= $unsigned(new_v[b]);
        if (m_wmask[W_M]) ram_m[b][m_addr[b]] <= new_m[b];
        if (m_wmask[W_N]) ram_n[b][m_addr[b]] <= $unsigned(new_n[b]);
      end
      // registered read
      q_h[b] <= ram_h[b][raddr[b]];
      q_v[b] <= ram_v[b][raddr[b]];
      q_m[b] <= ram_m[b][raddr[b]];
      q_n[b] <= ram_n[b][raddr[b]];
    end
  end

  // ---- control pipeline ----------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      m_valid  <= 1'b0;
      m_wmask  <= '0;
      m_rotl   <= '0;
      d1_valid <= 1'b0;
      d1_g     <= '0;
      d1_mask  <= '0;
      d1_dual  <= '0;
      sat_valid_o <= 1'b0;
      sat_h_o     <= '0;
      sat_v_o     <= '0;
      sat_n_o     <= '0;
      out_valid_o <= 1'b0;
      out_g_o     <= '0;
      out_mask_o  <= '0;
      out_dirty_o <= '0;
      for (int b = 0; b < BANKS; b++) begin
        m_act[b]    <= 1'b0;
        m_addr[b]   <= '0;
        m_h[b]      <= '0;
        m_v[b]      <= '0;
        m_m[b]      <= '0;
        m_n[b]      <= '0;
        by_valid[b] <= 1'b0;
        by_addr[b]  <= '0;
        by_wh[b]    <= 1'b0;
        by_wv[b]    <= 1'b0;
        by_wn[b]    <= 1'b0;
        by_h[b]     <= '0;
        by_v[b]     <= '0;
        by_n[b]     <= '0;
        d1_base[b]  <= '0;
        d1_bot[b]   <= '0;
        out_top[b]  <= '0;
        out_bot[b]  <= '0;
        out_vel[b]  <= '0;
        out_mat[b]  <= '0;
        out_nav[b]  <= '0;
      end
    end else begin
      // ---- ACCUM stage R -> M ---------------------------------------------
      m_valid <= up_valid_i;
      if (up_valid_i) begin
        m_wmask <= up_wmask_i;
        m_rotl  <= up_iv_i[1:0];
        for (int b = 0; b < BANKS; b++) begin
          m_act[b]  <= up_act[b];
          m_addr[b] <= up_addr[b];
          m_h[b]    <= up_h[up_rot[b]];
          m_v[b]    <= up_v[up_rot[b]];
          m_m[b]    <= up_m[up_rot[b]];
          m_n[b]    <= up_n[up_rot[b]];
        end
      end

      // ---- M stage: writeback bookkeeping + bypass + sat pulses -----------
      for (int b = 0; b < BANKS; b++) begin
        if (m_valid && m_act[b]) begin
          by_valid[b] <= 1'b1;
          by_addr[b]  <= m_addr[b];
          by_wh[b]    <= m_wmask[W_H];
          by_wv[b]    <= m_wmask[W_V];
          by_wn[b]    <= m_wmask[W_N];
          by_h[b]     <= new_h[b];
          by_v[b]     <= new_v[b];
          by_n[b]     <= new_n[b];
        end else begin
          by_valid[b] <= 1'b0;
        end
      end
      sat_valid_o <= m_valid;
      for (int l = 0; l < 4; l++) begin
        // lane l sits in bank (l + m_rotl) mod 4
        sat_h_o[l] <= m_valid && m_wmask[W_H] && m_act[2'(l)+m_rotl] && satb_h[2'(l)+m_rotl];
        sat_v_o[l] <= m_valid && m_wmask[W_V] && m_act[2'(l)+m_rotl] && satb_v[2'(l)+m_rotl];
        sat_n_o[l] <= m_valid && m_wmask[W_N] && m_act[2'(l)+m_rotl] && satb_n[2'(l)+m_rotl];
      end

      // ---- DRAIN stages ----------------------------------------------------
      d1_valid <= dr_valid_i && !up_valid_i;
      if (dr_valid_i) begin
        d1_g    <= dr_g_i;
        d1_mask <= dr_mask_i;
        d1_dual <= dr_dual_i;
        for (int b = 0; b < BANKS; b++) begin
          d1_base[b] <= dr_base[b];
          d1_bot[b]  <= dr_bot[b];
        end
      end

      out_valid_o <= d1_valid;
      if (d1_valid) begin
        out_g_o    <= d1_g;
        out_mask_o <= d1_mask;
        for (int b = 0; b < BANKS; b++) begin
          out_top[b]     <= dr_live[b];
          out_bot[b]     <= d1_dual[b] ? dr_botfx[b] : dr_live[b];
          out_dirty_o[b] <= dr_live[b] != (32'($signed(d1_base[b])) <<< 8);
          out_vel[b]     <= $signed(q_v[b]);
          out_mat[b]     <= q_m[b];
          out_nav[b]     <= $signed(q_n[b]);
        end
      end
    end
  end

endmodule : zhao_probe_patch_acc
