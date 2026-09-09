// zhao_terrain_bake_v2.sv — TERRAIN.BAKE, rearchitected to spend memory and
// a SINGLE multiplier instead of seven private ones. (2026-09-09 terrain
// rescue, per reports/terrain-recon/ARCHITECT-BRIEF-terrain-rearchitecture.md
// target 1.)
//
// PORT-COMPATIBLE with zhao_terrain_bake: same ports, same laws, same
// counters, same handshake contracts. Latency grows (see THE PRICE below);
// nothing else may move, and tests/terrain/terrain_bake_v2_directed.cpp holds
// it to the same zref oracle the v1 suite uses, boundary for boundary.
//
// Law, in citation order (identical to v1 — the law did not move):
//   design/contracts/TERRAIN.BAKE.md — the block contract.
//   spec/terrain_rules.md §2 §3.3 §3.4 §7 §9/§9.2 — layers, corner shadow,
//       breach law, ownership, cadence budget.
//   spec/qformats.md §2/§3/§4/§9 — height16/fx16, rescale, round-half-up.
//   reference/src/zterrain/terrain_core.cpp `bake_dig`, `apply_breach_law`,
//       `zref::terrain::lattice_lerp` — THE EXECUTED LAW.
//
// ---------------------------------------------------------------------------
// WHAT MOVED, AND WHY IT IS LEGAL
// ---------------------------------------------------------------------------
// V1 holds SEVEN multiplier sites (dx*dx, dz*dz, radius^2, lat_lerp x2 call
// sites, and bake_delta's two 32x18 products) and ZERO memory bits, with the
// 1,089-bit `meets` plane in flip-flops. The device has 41,910 ALM (139%
// committed), 112 DSP (171% committed) and ~400 FREE M10K. This block is the
// densest arithmetic-with-no-RAM block in terrain, and its own FSM is strictly
// sequential — one vertex at a time through a 17-cycle divide — so the seven
// multipliers are busy in DIFFERENT STATES and can be one physical multiplier
// with operand muxes, exactly the `mseq` pattern zhao_terrain_normals.sv:203
// and zhao_terrain_lod.sv:273 already ship.
//
// M1. ONE 34x34 SIGNED MULTIPLIER, `mul_p <= mul_a * mul_b`, product
//     registered, operands muxed by state. Every v1 product fits inside it:
//       radius^2      signed 32 x 32   (StRad)
//       span   * num  signed 33 x  7   (StVzM per row, StVxM per vertex)
//       dz     * dz   signed 33 x 33   (StDzM per row)
//       dx     * dx   signed 33 x 33   (StDxM per vertex)
//       depth  * s    signed 32 x 18   (StPfM/StPtM per covered vertex)
//     The product register has an ENABLE (`mul_go`): states that do not issue
//     a product hold the last one, which is what lets StVtx read dx^2 out of
//     `mul_p` while the handshake waits, and StEmit read p_to out of it after
//     the divide.
//
// M2. THE `meets` PLANE MOVES TO A RAM — and this REVISITS v1's chosen B2
//     rather than ignoring it. B2's rejected alternative was taking the meets
//     bits FROM THE CALLER, which moves the §3.4 breach equality out of the
//     only block terrain_rules §7 permits to own it. Moving the STORAGE into
//     a RAM inside this same block moves no law anywhere: the equality
//     (`composed18 <= bottom18`) is still computed here, per vertex, and the
//     stored bit is still this block's private bridge between its two phases.
//     B2's argument was about OWNERSHIP; this change is about SUBSTRATE.
//
//     The access shapes both fit one simple dual-port RAM:
//       DIG writes one full ROW per 33 vertices — bits accumulate in a 33-bit
//         row register (`wrow`) and the completed word is written once, at the
//         row's last vertex.
//       BREACH reads rows cj and cj+1 — a two-row REGISTER WINDOW
//         (`mrow_lo`/`mrow_hi`), with row cj+2 prefetched during the current
//         row's 32-cell scan (a scan is >= 32 cycles; the prefetch needs 1).
//     34 words x 33 bits = 1,122 bits — one M10K at any legal aspect
//     (256x40 holds it 7x over). The RAM is never reset: a breach phase can
//     only run after a full 33x33 dig sweep has written every row of the
//     current record, so pre-sweep contents are UNREACHABLE, not hazardous
//     (the zhao_proj_arena3 dense-fill argument; a reset loop would also
//     break M10K inference, QUARTUS_GOTCHAS 10).
//
// THE PRICE, MEASURED NOT GUESSED (terrain_bake_v2_directed.cpp measures both
// blocks' cycles on the same records):
//     covered vertex   v1: 19 clocks   v2: 24   (3 geometry muls + 2 delta muls)
//     uncovered vertex v1:  2 clocks   v2:  5
//     per row          v2 adds 4 (vz/dz^2 recompute), per record 1 (radius^2)
// The block is `backpressure: ready_valid, latency: variable`
// (design/blocks.yml), and §9.2's cadence law is built around bakes that DO
// NOT complete in their frame — the budget counts ACCEPTANCES and the
// deferral law carries remainders forward — so a slower sweep changes no
// contract. The frame arithmetic lives in
// reports/TERRAIN-REARCHITECTURE-20260909.md §bake.
//
// EVERYTHING ELSE IS V1, VERBATIM: the two phases, chosen B1/B3/B4/B5, the
// radius ruling, the divider, the clamp, the rails, the breach law, the
// counters, the handshake free-slot argument. Where v1's comment said why, the
// why is unchanged and not repeated here — read v1 alongside this file.
//
// Conservative SystemVerilog subset only (charter §2).

module zhao_terrain_bake_v2 (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // the §9.2 frame window
    // -----------------------------------------------------------------------
    input  logic       frame_start_i,       // 1 cycle: open a new bake window
    output logic       budget_full_o,       // BAKE_PATCH_BUDGET reached
    output logic [7:0] bakes_this_frame_o,

    // -----------------------------------------------------------------------
    // stamp_results: ONE patch-bake record
    // -----------------------------------------------------------------------
    input  logic               cmd_valid_i,
    output logic               cmd_ready_o,
    input  logic        [15:0] cmd_patch_id_i,
    input  logic signed [31:0] cmd_cx_i,          // stencil centre, fx16 raw
    input  logic signed [31:0] cmd_cz_i,
    input  logic signed [31:0] cmd_radius_i,      // fx16 raw; <= 0 writes nothing
    input  logic signed [31:0] cmd_depth_from_i,  // fx16 raw, absolute depth
    input  logic signed [31:0] cmd_depth_to_i,
    input  logic signed [31:0] cmd_env_x0_i,      // the patch envelope, fx16 raw
    input  logic signed [31:0] cmd_env_z0_i,
    input  logic signed [31:0] cmd_env_x1_i,
    input  logic signed [31:0] cmd_env_z1_i,
    input  logic               cmd_dual_i,        // layer C present
    input  logic               cmd_cells_i,       // layer D present
    input  logic        [15:0] cmd_src_id_i,
    output logic        [15:0] trace_patch_id_o,  // the record under bake

    // -----------------------------------------------------------------------
    // DIG phase: layer B read-modify-write, 33x33 vertices, z-then-x
    // -----------------------------------------------------------------------
    output logic        [ 5:0] vtx_vi_o,
    output logic        [ 5:0] vtx_vj_o,
    input  logic               vtx_valid_i,
    output logic               vtx_ready_o,
    input  logic signed [15:0] vtx_base_i,    // layer A
    input  logic signed [15:0] vtx_scar_i,    // layer B in
    input  logic signed [15:0] vtx_bottom_i,  // layer C
    input  logic               vtx_nobake_i,  // §3.3 corner shadow

    output logic               sc_valid_o,
    input  logic               sc_ready_i,
    output logic signed [15:0] sc_scar_o,     // layer B out
    output logic        [ 5:0] sc_vi_o,
    output logic        [ 5:0] sc_vj_o,
    output logic               sc_touched_o,  // inside the stencil
    output logic               sc_meets_o,    // base + scar <= bottom (§3.4)
    output logic               sc_clamped_o,  // the no_bake clamp fired
    output logic        [15:0] sc_src_id_o,

    // -----------------------------------------------------------------------
    // BREACH phase: layer D read-modify-write, 32x32 cells, z-then-x
    // -----------------------------------------------------------------------
    output logic [5:0] cell_ci_o,
    output logic [5:0] cell_cj_o,
    input  logic       cell_valid_i,
    output logic       cell_ready_o,
    input  logic [7:0] cell_state_i,

    output logic       cs_valid_o,
    input  logic       cs_ready_i,
    output logic [7:0] cs_state_o,
    output logic [5:0] cs_ci_o,
    output logic [5:0] cs_cj_o,
    output logic       cs_event_o,  // a §3.4 transition: the trace event
    output logic [1:0] cs_sub_o,    // the new substance, valid with cs_event_o
    output logic [15:0] cs_src_id_o,

    // -----------------------------------------------------------------------
    // status, counters
    // -----------------------------------------------------------------------
    output logic        dig_done_o,       // 1-cycle pulse: layer B complete
    output logic        bake_done_o,      // 1-cycle pulse: the record retired
    output logic        breach_active_o,  // the block wants cells, not vertices
    output logic [31:0] surface_texels_touched_o,
    output logic [31:0] breach_events_o,
    output logic [31:0] scar_saturations_o,
    output logic [31:0] nobake_clamps_o,
    // OWNER RULING 2026-08-24: MAX_BAKE_RADIUS = 512 m. Rejected, never
    // clamped, counted (v1's comment says why; the ruling did not move).
    output logic [31:0] bake_radius_rejects_o,
    output logic        idle_o
);

  // ---- frozen constants (terrain_rules §2, §9.2 — law, not preference) ----
  localparam int unsigned Lat = 33;  // Island Patch v1 lattice
  localparam logic [5:0] LatMax = 6'd32;  // last lattice index
  localparam logic [5:0] CellMax = 6'd31;  // last cell index
  localparam logic [7:0] BakePatchBudget = 8'd64;  // terrain_rules §9.2, frozen

  // ---- named, editable shape parameters (CLAUDE.md art law rule 6) --------
  // The shared multiplier's width: must hold signed 33x33 (dx*dx). 34 covers
  // every site; widening it is legal, narrowing it below 34 breaks dx^2.
  localparam int unsigned MulW = 34;
  localparam int unsigned MulPW = 2 * MulW;  // 68
  // The meets RAM: Lat rows plus one PAD row so the breach prefetch address
  // (cj + 2, clamped) always has a legal word to read. The pad row is never
  // written and its read value is never consumed.
  localparam int unsigned MeetsDepth = Lat + 1;  // 34
  localparam logic [5:0] MeetsPadRow = 6'd33;

  // ---- cell state byte (terrain_rules §3.3) -------------------------------
  localparam logic [1:0] SubSolid = 2'd0;
  localparam logic [1:0] SubVoidAuthored = 2'd1;
  localparam logic [1:0] SubVoidBreached = 2'd2;

  // ---- states --------------------------------------------------------------
  // The sequencer: per record StRad once; per row StVzM/StVzC/StDzM/StDzC;
  // per vertex StVxM/StVxC/StDxM then the v1 spine StVtx/StDiv/StEmit with
  // StPfM/StPtM between divide and emit on the covered path; per breach
  // entry StBrA/StBrB/StBrC to fill the row window, then StCell.
  localparam logic [4:0] StIdle = 5'd0;
  localparam logic [4:0] StRad  = 5'd1;   // mul: radius^2
  localparam logic [4:0] StVzM  = 5'd2;   // mul: span_z * vj  (also lands r2)
  localparam logic [4:0] StVzC  = 5'd3;   // vz_q <= lerp finish
  localparam logic [4:0] StDzM  = 5'd4;   // mul: dz * dz
  localparam logic [4:0] StDzC  = 5'd5;   // dz2_q <= product
  localparam logic [4:0] StVxM  = 5'd6;   // mul: span_x * vi
  localparam logic [4:0] StVxC  = 5'd7;   // vx_q <= lerp finish
  localparam logic [4:0] StDxM  = 5'd8;   // mul: dx * dx
  localparam logic [4:0] StVtx  = 5'd9;   // waiting for a vertex (covers comb)
  localparam logic [4:0] StDiv  = 5'd10;  // the 17-step stencil divide
  localparam logic [4:0] StPfM  = 5'd11;  // mul: depth_from * s
  localparam logic [4:0] StPtM  = 5'd12;  // mul: depth_to * s; pf_q lands
  localparam logic [4:0] StEmit = 5'd13;  // publish the scar word
  localparam logic [4:0] StBrA  = 5'd14;  // meets row window: address row 0
  localparam logic [4:0] StBrB  = 5'd15;  // mrow_lo <= row 0; address row 1
  localparam logic [4:0] StBrC  = 5'd16;  // mrow_hi <= row 1; prefetch row 2
  localparam logic [4:0] StCell = 5'd17;  // waiting for a cell
  localparam logic [4:0] StR2C  = 5'd18;  // c_r2 <= radius^2 (ONCE per record �
                                          // StVzM recurs per row and must never
                                          // touch r2: the first build captured
                                          // r2 there and clobbered it with the
                                          // previous row's last product)
  logic [4:0] state;

  // -------------------------------------------------------------------------
  // the held record
  // -------------------------------------------------------------------------
  logic signed [31:0] c_cx, c_cz, c_from, c_to;
  logic signed [31:0] c_x0, c_z0, c_x1, c_z1;
  logic signed [31:0] c_rad;  // held so radius^2 can be sequenced (StRad)
  logic               c_dual, c_cells;
  logic        [15:0] c_src;
  logic        [62:0] c_r2;  // radius^2, unsigned; 0 when radius <= 0 (B5)
  logic signed [32:0] c_spx, c_spz;  // envelope spans, captured at StRad

  // 512.0 m in fx16 raw (owner ruling 2026-08-24; v1's derivation applies).
  localparam logic signed [31:0] MAX_BAKE_RADIUS_RAW = 32'sh0200_0000;
  wire radius_illegal = (cmd_radius_i > MAX_BAKE_RADIUS_RAW);

  logic [5:0] vi, vj;  // lattice indices, 0..32
  logic [5:0] ci, cj;  // cell indices, 0..31

  // ---- the vertex under evaluation ----------------------------------------
  logic signed [15:0] h_base, h_scar, h_bottom;
  logic               h_nobake;
  logic               v_covered;  // d2 < r2

  // ---- the stencil divider (v1, verbatim) ---------------------------------
  logic [79:0] div_rem;
  logic [79:0] div_dsh;
  logic [16:0] div_quo;
  logic [ 4:0] div_cnt;

  // -------------------------------------------------------------------------
  // THE ONE MULTIPLIER (M1) — the factory every former site now queues for
  // -------------------------------------------------------------------------
  logic signed [MulW-1:0] mul_a, mul_b;
  // The top two product bits are pure sign for every site (the widest real
  // product, dx*dx, is 66 bits) — kept so the register IS the full multiplier
  // output and no site needs a width argument at the landing pad.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [MulPW-1:0] mul_p;
  /* verilator lint_on UNUSEDSIGNAL */
  logic mul_go;  // product-register enable: only mul states overwrite mul_p

  always_comb begin
    // Operand mux, one arm per issuing state. Defaults keep the mux total
    // rather than latch-shaped; `mul_go` is what gates the landing.
    mul_a  = '0;
    mul_b  = '0;
    mul_go = 1'b0;
    unique case (state)
      StRad: begin
        mul_a  = {{2{c_rad[31]}}, c_rad};
        mul_b  = {{2{c_rad[31]}}, c_rad};
        mul_go = 1'b1;
      end
      StVzM: begin
        mul_a  = {c_spz[32], c_spz};
        mul_b  = {28'b0, vj};  // 0..32, positive
        mul_go = 1'b1;
      end
      StDzM: begin
        mul_a  = {dz_c[32], dz_c};
        mul_b  = {dz_c[32], dz_c};
        mul_go = 1'b1;
      end
      StVxM: begin
        mul_a  = {c_spx[32], c_spx};
        mul_b  = {28'b0, vi};
        mul_go = 1'b1;
      end
      StDxM: begin
        mul_a  = {dx_c[32], dx_c};
        mul_b  = {dx_c[32], dx_c};
        mul_go = 1'b1;
      end
      StPfM: begin
        mul_a  = {{2{c_from[31]}}, c_from};
        mul_b  = {17'b0, div_quo};  // s, Q16, 0..65536: positive
        mul_go = 1'b1;
      end
      StPtM: begin
        mul_a  = {{2{c_to[31]}}, c_to};
        mul_b  = {17'b0, div_quo};
        mul_go = 1'b1;
      end
      default: ;
    endcase
  end

  // -------------------------------------------------------------------------
  // lattice_lerp, SPLIT ACROSS THE PRODUCT REGISTER — the arithmetic is v1's
  // function body operator for operator; only the multiply moved into `mul_p`.
  // prod = span * num fits signed 40 (33b span x 7b num), so `mul_p[39:0]` IS
  // v1's `prod` — the wider product's upper bits are pure sign.
  // -------------------------------------------------------------------------
  function automatic logic signed [31:0] lerp_finish(input logic signed [31:0] a,
                                                     input logic signed [39:0] prod);
    logic signed [39:0] nadj;
    logic signed [39:0] quot;
    /* verilator lint_off UNUSEDSIGNAL */
    logic signed [39:0] sum;
    /* verilator lint_on UNUSEDSIGNAL */
    begin
      nadj = prod + 40'sd16;
      quot = nadj >>> 5;
      if (nadj[39] && (nadj[4:0] != 5'd0)) quot = quot + 40'sd1;  // toward zero
      sum = {{8{a[31]}}, a} + quot;
      lerp_finish = sum[31:0];  // static_cast<int32_t>: wraps, as the ref does
    end
  endfunction

  // the placed lattice point, registered per row (vz_q) and per vertex (vx_q)
  logic signed [31:0] vz_q, vx_q;
  logic signed [65:0] dz2_q;  // dz*dz, held for the whole row

  // re-domained differences (v1's dx/dz, now fed from the registered points)
  logic signed [32:0] dx_c, dz_c;
  assign dx_c = $signed({vx_q[31], vx_q}) - $signed({c_cx[31], c_cx});
  assign dz_c = $signed({vz_q[31], vz_q}) - $signed({c_cz[31], c_cz});

  // -------------------------------------------------------------------------
  // the radial test (v1, with dx^2 read from the product register)
  // -------------------------------------------------------------------------
  logic [66:0] d2;
  assign d2 = {1'b0, mul_p[65:0]} + {1'b0, dz2_q};  // mul_p holds dx^2 in StVtx

  logic [79:0] d2_ext, r2_ext;
  assign d2_ext = {13'b0, d2};
  assign r2_ext = {17'b0, c_r2};

  logic covers;
  assign covers = d2_ext < r2_ext;

  logic [79:0] sn_init;
  assign sn_init = ((r2_ext - d2_ext) << 16) + (r2_ext >> 1);

  logic [79:0] dsh_init;
  assign dsh_init = r2_ext << 16;

  // -------------------------------------------------------------------------
  // the incremental delta — bake_delta's post-multiply arithmetic, operator
  // for operator (zhao_terrain_bake_delta.sv is the annotated original; its
  // two 32x18 products are the ONLY thing that moved, into StPfM/StPtM).
  // `pf_q` holds p_from; `mul_p[49:0]` holds p_to during StEmit.
  // -------------------------------------------------------------------------
  logic signed [49:0] pf_q;

  function automatic logic signed [31:0] delta_g(input logic signed [49:0] p);
    // rescale(.,16) with the fx16 saturate, then rescale(.,8); returns g
    // sign-extended to the fx16 word so the subtraction below is exact
    // (|g| <= 2^23, bake_delta's own bound). The discarded low bits of `q`
    // and `t` ARE the two roundings — the shifts are the operators.
    /* verilator lint_off UNUSEDSIGNAL */
    logic signed [49:0] q;
    logic signed [32:0] t;
    /* verilator lint_on UNUSEDSIGNAL */
    logic signed [33:0] a_raw;
    logic signed [31:0] a;
    begin
      q = p + 50'sd32768;
      a_raw = q[49:16];
      a = (a_raw > 34'sd2147483647)  ? 32'sh7FFF_FFFF :
          (a_raw < -34'sd2147483648) ? 32'sh8000_0000 : a_raw[31:0];
      t = $signed({a[31], a}) + 33'sd128;
      delta_g = {{7{t[32]}}, t[32:8]};
    end
  endfunction

  function automatic logic delta_g_sat(input logic signed [49:0] p);
    /* verilator lint_off UNUSEDSIGNAL */
    logic signed [49:0] q;
    /* verilator lint_on UNUSEDSIGNAL */
    logic signed [33:0] a_raw;
    begin
      q = p + 50'sd32768;
      a_raw = q[49:16];
      delta_g_sat = (a_raw > 34'sd2147483647) || (a_raw < -34'sd2147483648);
    end
  endfunction

  logic signed [31:0] delta16;
  logic               delta_sat;
  logic signed [31:0] g_from_c, g_to_c;
  assign g_from_c = delta_g(pf_q);
  assign g_to_c   = delta_g(mul_p[49:0]);
  // An uncovered vertex has stencil 0: both products are 0, both g are 0 and
  // the delta is 0 with no saturation — proved by substitution in
  // bake_delta's own arithmetic ((0+32768)>>>16 = 0; (0+128)>>>8 = 0). The
  // covered path never skips StPfM/StPtM, so the mux below is exactly v1's
  // `stencil = v_covered ? div_quo : 0` one stage later.
  assign delta16   = v_covered ? (g_from_c - g_to_c) : 32'sd0;
  assign delta_sat = v_covered ? (delta_g_sat(pf_q) || delta_g_sat(mul_p[49:0])) : 1'b0;

  // -------------------------------------------------------------------------
  // scar = scar + delta, the no_bake clamp, the height16 rails (v1, verbatim)
  // -------------------------------------------------------------------------
  logic signed [33:0] scar_sum;
  assign scar_sum = {{18{h_scar[15]}}, h_scar} + {{2{delta16[31]}}, delta16};

  logic signed [33:0] min_scar;
  assign min_scar = {{18{h_bottom[15]}}, h_bottom} + 34'sd1 - {{18{h_base[15]}}, h_base};

  logic guard_on;
  assign guard_on = c_dual && c_cells && h_nobake;

  logic clamp_fires;
  assign clamp_fires = v_covered && guard_on && (scar_sum < min_scar);

  logic signed [33:0] scar_guarded;
  assign scar_guarded = clamp_fires ? min_scar : scar_sum;

  logic rail_hi, rail_lo;
  assign rail_hi = v_covered && (scar_guarded > 34'sd32767);
  assign rail_lo = v_covered && (scar_guarded < -34'sd32768);

  logic signed [15:0] scar_new;
  assign scar_new = !v_covered ? h_scar :
                    rail_hi    ? 16'sh7FFF :
                    rail_lo    ? 16'sh8000 : scar_guarded[15:0];

  // the §3.4 equality — STILL COMPUTED HERE (see M2 in the header); signedness
  // rails exactly as v1 (the GEOM.BINNER unsigned-compare trap).
  logic signed [17:0] composed18, bottom18;
  assign composed18 = $signed({{2{h_base[15]}}, h_base}) + $signed({{2{scar_new[15]}}, scar_new});
  assign bottom18   = $signed({{2{h_bottom[15]}}, h_bottom});

  logic meets_new;
  assign meets_new = composed18 <= bottom18;

  // -------------------------------------------------------------------------
  // THE MEETS PLANE AS A RAM (M2): write side
  // -------------------------------------------------------------------------
  logic [32:0] meets_ram[MeetsDepth];
  logic [31:0] wrow;  // the row under accumulation (bits 0..31; bit 32 joins at write)
  logic [32:0] mq;    // registered RAM read data
  logic [5:0]  m_raddr;

  // The completed row word: every bit vi < 32 was written into `wrow` at its
  // own emit; the last bit rides the write itself.
  logic [32:0] m_wdata;
  assign m_wdata = {meets_new, wrow};

  logic m_we;
  assign m_we = (state == StEmit) && (vi == LatMax);

  always_comb begin
    // Breach read/prefetch address. During the cell scan the prefetch wants
    // row cj+2; clamped to the pad row when past the last real row (the
    // prefetched value is then never consumed — see MeetsPadRow).
    unique case (state)
      StBrA:   m_raddr = 6'd0;
      StBrB:   m_raddr = 6'd1;
      StBrC:   m_raddr = 6'd2;
      StCell:  m_raddr = (cj <= 6'd30) ? (cj + 6'd2) : MeetsPadRow;
      default: m_raddr = 6'd0;
    endcase
  end

  // The RAM proper: synchronous read, one write port, no reset (header M2
  // says why unreset contents are unreachable).
  always_ff @(posedge clk) begin
    mq <= meets_ram[m_raddr];
    if (m_we) meets_ram[vj] <= m_wdata;
  end

  // ---- the breach row window ----------------------------------------------
  logic [32:0] mrow_lo, mrow_hi;

  logic all4;
  assign all4 = mrow_lo[ci] && mrow_lo[ci+6'd1] && mrow_hi[ci] && mrow_hi[ci+6'd1];

  logic [1:0] sub_in;
  assign sub_in = cell_state_i[1:0];

  logic cell_nobake;
  assign cell_nobake = cell_state_i[2];

  logic breach_fires, heal_fires;
  assign breach_fires = (sub_in == SubSolid) && all4 && !cell_nobake;
  assign heal_fires = (sub_in == SubVoidBreached) && !all4;

  logic [1:0] sub_out;
  assign sub_out = breach_fires ? SubVoidBreached : heal_fires ? SubSolid : sub_in;

  logic cell_event;
  assign cell_event = (sub_in != SubVoidAuthored) && (breach_fires || heal_fires);

  // -------------------------------------------------------------------------
  // handshakes (v1's free-slot argument holds unchanged: `sc_free` at the
  // accept cycle means the register is free at every later cycle of this
  // vertex, because nothing re-raises sc_valid_o until its own StEmit)
  // -------------------------------------------------------------------------
  logic sc_free, cs_free;
  assign sc_free = !sc_valid_o || sc_ready_i;
  assign cs_free = !cs_valid_o || cs_ready_i;

  assign vtx_ready_o = (state == StVtx) && sc_free;
  assign cell_ready_o = (state == StCell) && cs_free;
  assign cmd_ready_o = (state == StIdle) && !budget_full_o;

  assign vtx_vi_o = vi;
  assign vtx_vj_o = vj;
  assign cell_ci_o = ci;
  assign cell_cj_o = cj;

  assign breach_active_o = (state == StCell);
  assign idle_o = (state == StIdle) && !sc_valid_o && !cs_valid_o;

  logic [7:0] bakes_this_frame;
  assign bakes_this_frame_o = bakes_this_frame;
  assign budget_full_o = bakes_this_frame >= BakePatchBudget;

  wire last_vtx = (vi == LatMax) && (vj == LatMax);
  wire last_cell = (ci == CellMax) && (cj == CellMax);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= StIdle;
      c_cx <= '0;
      c_cz <= '0;
      c_from <= '0;
      c_to <= '0;
      c_x0 <= '0;
      c_z0 <= '0;
      c_x1 <= '0;
      c_z1 <= '0;
      c_rad <= '0;
      c_dual <= 1'b0;
      c_cells <= 1'b0;
      c_src <= '0;
      c_r2 <= '0;
      c_spx <= '0;
      c_spz <= '0;
      vi <= '0;
      vj <= '0;
      ci <= '0;
      cj <= '0;
      h_base <= '0;
      h_scar <= '0;
      h_bottom <= '0;
      h_nobake <= 1'b0;
      v_covered <= 1'b0;
      div_rem <= '0;
      div_dsh <= '0;
      div_quo <= '0;
      div_cnt <= '0;
      mul_p <= '0;
      vz_q <= '0;
      vx_q <= '0;
      dz2_q <= '0;
      pf_q <= '0;
      wrow <= '0;
      mrow_lo <= '0;
      mrow_hi <= '0;
      sc_valid_o <= 1'b0;
      sc_scar_o <= '0;
      sc_vi_o <= '0;
      sc_vj_o <= '0;
      sc_touched_o <= 1'b0;
      sc_meets_o <= 1'b0;
      sc_clamped_o <= 1'b0;
      sc_src_id_o <= '0;
      cs_valid_o <= 1'b0;
      cs_state_o <= '0;
      cs_ci_o <= '0;
      cs_cj_o <= '0;
      cs_event_o <= 1'b0;
      cs_sub_o <= '0;
      cs_src_id_o <= '0;
      trace_patch_id_o <= '0;
      dig_done_o <= 1'b0;
      bake_done_o <= 1'b0;
      bakes_this_frame <= '0;
      surface_texels_touched_o <= '0;
      breach_events_o <= '0;
      scar_saturations_o <= '0;
      nobake_clamps_o <= '0;
      bake_radius_rejects_o <= '0;
    end else begin
      dig_done_o  <= 1'b0;
      bake_done_o <= 1'b0;

      if (frame_start_i) bakes_this_frame <= '0;

      if (sc_valid_o && sc_ready_i) sc_valid_o <= 1'b0;
      if (cs_valid_o && cs_ready_i) cs_valid_o <= 1'b0;

      // the shared product register — every site's landing pad
      if (mul_go) mul_p <= mul_a * mul_b;

      case (state)
        StIdle: begin
          if (cmd_valid_i && cmd_ready_o && radius_illegal) begin
            bake_radius_rejects_o <= bake_radius_rejects_o + 32'd1;
          end else if (cmd_valid_i && cmd_ready_o) begin
            c_cx <= cmd_cx_i;
            c_cz <= cmd_cz_i;
            c_from <= cmd_depth_from_i;
            c_to <= cmd_depth_to_i;
            c_x0 <= cmd_env_x0_i;
            c_z0 <= cmd_env_z0_i;
            c_x1 <= cmd_env_x1_i;
            c_z1 <= cmd_env_z1_i;
            c_rad <= cmd_radius_i;
            c_dual <= cmd_dual_i;
            c_cells <= cmd_cells_i;
            c_src <= cmd_src_id_i;
            trace_patch_id_o <= cmd_patch_id_i;
            vi <= '0;
            vj <= '0;
            ci <= '0;
            cj <= '0;
            bakes_this_frame <= frame_start_i ? 8'd1 : (bakes_this_frame + 8'd1);
            state <= StRad;
          end
        end

        StRad: begin
          // mul: radius^2 lands at this edge. The spans are captured here so
          // every later lerp reads a register, not a subtraction.
          c_spx <= $signed({c_x1[31], c_x1}) - $signed({c_x0[31], c_x0});
          c_spz <= $signed({c_z1[31], c_z1}) - $signed({c_z0[31], c_z0});
          state <= StR2C;
        end

        StR2C: begin
          // radius^2 is read out ONCE per record (B5: r2 = 0 for radius <= 0
          // makes `covers` false everywhere)
          c_r2  <= (c_rad > 0) ? mul_p[62:0] : 63'd0;
          state <= StVzM;
        end

        StVzM: begin
          // mul: span_z * vj lands at this edge
          state <= StVzC;
        end

        StVzC: begin
          vz_q  <= lerp_finish(c_z0, mul_p[39:0]);
          state <= StDzM;
        end

        StDzM: begin
          // mul: dz * dz lands at this edge
          state <= StDzC;
        end

        StDzC: begin
          dz2_q <= mul_p[65:0];
          state <= StVxM;
        end

        StVxM: begin
          // mul: span_x * vi lands at this edge
          state <= StVxC;
        end

        StVxC: begin
          vx_q  <= lerp_finish(c_x0, mul_p[39:0]);
          state <= StDxM;
        end

        StDxM: begin
          // mul: dx * dx lands at this edge
          state <= StVtx;
        end

        StVtx: begin
          if (vtx_valid_i && vtx_ready_o) begin
            h_base <= vtx_base_i;
            h_scar <= vtx_scar_i;
            h_bottom <= vtx_bottom_i;
            h_nobake <= vtx_nobake_i;
            v_covered <= covers;
            if (covers) begin
              div_rem <= sn_init;
              div_dsh <= dsh_init;
              div_quo <= '0;
              div_cnt <= 5'd16;
              state   <= StDiv;
            end else begin
              div_quo <= '0;
              state   <= StEmit;
            end
          end
        end

        StDiv: begin
          if (div_rem >= div_dsh) begin
            div_rem <= div_rem - div_dsh;
            div_quo <= {div_quo[15:0], 1'b1};
          end else begin
            div_quo <= {div_quo[15:0], 1'b0};
          end
          div_dsh <= div_dsh >> 1;
          if (div_cnt == 5'd0) state <= StPfM;
          else div_cnt <= div_cnt - 5'd1;
        end

        StPfM: begin
          // mul: depth_from * s lands at this edge
          state <= StPtM;
        end

        StPtM: begin
          // mul: depth_to * s lands at this edge; p_from is read out first
          pf_q  <= mul_p[49:0];
          state <= StEmit;
        end

        StEmit: begin
          sc_valid_o <= 1'b1;
          sc_scar_o <= scar_new;
          sc_vi_o <= vi;
          sc_vj_o <= vj;
          sc_touched_o <= v_covered;
          sc_meets_o <= meets_new;
          sc_clamped_o <= clamp_fires;
          sc_src_id_o <= c_src;
          // bit 32 rides the RAM write itself (m_wdata) at this same edge
          if (vi != LatMax) wrow[vi[4:0]] <= meets_new;
          if (v_covered) surface_texels_touched_o <= surface_texels_touched_o + 32'd1;
          if (clamp_fires) nobake_clamps_o <= nobake_clamps_o + 32'd1;
          if (rail_hi || rail_lo || (v_covered && delta_sat))
            scar_saturations_o <= scar_saturations_o + 32'd1;
          if (last_vtx) begin
            dig_done_o <= 1'b1;
            if (c_dual && c_cells) begin
              state <= StBrA;
            end else begin
              bake_done_o <= 1'b1;
              state <= StIdle;
            end
          end else begin
            if (vi == LatMax) begin
              vi <= '0;
              vj <= vj + 6'd1;
              state <= StVzM;  // new row: vz and dz^2 recompute
            end else begin
              vi <= vi + 6'd1;
              state <= StVxM;
            end
          end
        end

        StBrA: begin
          // m_raddr = 0; row 0 lands in mq at this edge
          state <= StBrB;
        end

        StBrB: begin
          mrow_lo <= mq;  // row 0
          // m_raddr = 1; row 1 lands in mq at this edge
          state <= StBrC;
        end

        StBrC: begin
          mrow_hi <= mq;  // row 1
          // m_raddr = 2; the prefetch lands in mq at this edge
          state <= StCell;
        end

        StCell: begin
          if (cell_valid_i && cell_ready_o) begin
            cs_valid_o <= 1'b1;
            cs_state_o <= {cell_state_i[7:2], sub_out};
            cs_ci_o <= ci;
            cs_cj_o <= cj;
            cs_event_o <= cell_event;
            cs_sub_o <= sub_out;
            cs_src_id_o <= c_src;
            if (cell_event) breach_events_o <= breach_events_o + 32'd1;
            if (last_cell) begin
              bake_done_o <= 1'b1;
              state <= StIdle;
            end else begin
              if (ci == CellMax) begin
                ci <= '0;
                cj <= cj + 6'd1;
                // the row window slides: cj+1's row was prefetched into mq
                // during this row's >= 32-cycle scan (1 cycle was enough)
                mrow_lo <= mrow_hi;
                mrow_hi <= mq;
              end else begin
                ci <= ci + 6'd1;
              end
            end
          end
        end

        default: state <= StIdle;
      endcase
    end
  end

endmodule : zhao_terrain_bake_v2
