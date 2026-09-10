// zhao_terrain_wcache.sv — TERRAIN.WCACHE: the terrain instance of the
// projected-vertex arena, and the replay engine that turns three corner
// references into one projected triangle per clock.
//
// Law (in citation order):
//   fpga/rtl/geometry/zhao_vertex_arena.sv — the primitive this instantiates
//       (THREE times, see below), which owns the memory, the valid mechanism,
//       the six refusals, READ-OLD and the SymbiYosys proof.
//   fpga/rtl/geometry/zhao_geom_wcache.sv — the geometry shell this mirrors;
//       the 106-bit field map is IDENTICAL, on purpose, so one unpack law
//       serves both.
//   fpga/rtl/terrain/zhao_terrain_project.sv — the retained oracle. This
//       block's output packet is that block's output packet PLUS the three
//       guarded clip `w` values it never carried, and the differential holds
//       every replayed triangle bit-identical to it
//       (tests/terrain/terrain_wcache_differential.cpp).
//   reports/PROJECTION-ADOPTION-20260910.md — the packet this was built in;
//       the parameter derivations, the M10K arithmetic and what remains.
//
// ENFORCED-BY: tests/terrain/terrain_wcache_differential.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS IS A SHELL, AND WHY IT IS THE SECOND ONE
// ---------------------------------------------------------------------------
// Owner ruling 2026-08-24: "Build a reusable parameterized arena primitive and
// a GEOM.WCACHE shell. Terrain may later instantiate the same primitive with
// its own depth/payload. This does not mean one physical cache shared between
// the two pipelines."
//
// This is the "later". The mechanism lives once, in `zhao_vertex_arena`; this
// block fixes terrain's parameters, writes down what the 106 bits MEAN, and
// adds the one thing terrain needs that geometry does not: a triangle-rate
// replay path. Nothing here re-implements a valid bit, a generation or a
// refusal, and the `zhao_proj_arena3` design study's key/ref-count machinery
// was deliberately NOT carried in (reports/VERTEX-ARENA-DENSE-SEAL §1: shell
// business, and the generation already answers staleness).
//
// ---------------------------------------------------------------------------
// THE PAYLOAD, FIELD BY FIELD — the same 106 bits as zhao_geom_wcache
// ---------------------------------------------------------------------------
//     [20:0]    screen x, S 12.8, guard-band clamped   (out_x_o)
//     [41:21]   screen y, S 12.8                       (out_y_o)
//     [73:42]   invw, Q16.16                           (out_d_o)
//     [104:74]  w, guarded clip w, fx16 raw            (out_w_o)
//     [105]     behind: clip.w <= 0                    (out_behind_o)
//
// `w` IS CARRIED. GEOM.DEPTHQUANT's correction of 2026-09-03 makes the depth
// consumer take `w` and perform its own reciprocal; recovering `w` from the
// rounded Q16.16 `1/w` compounds a rounding that already happened
// (reports/WCACHE-DROPS-W-20260909.md). `zhao_terrain_project` exposes NO `w`
// at all — its `out_*d_o` are the quotient — so this block is the first
// terrain path that can feed DEPTHQUANT correctly. The 31-bit width is the
// core's own (`zhao_project_core.out_w_o`); DEPTHQUANT's 40-bit port is
// input headroom its first-act clamp bounds to <= 2^30, so a zero-extend is
// lossless (reports/TERRAIN-REARCHITECTURE-20260909.md §4, with citations).
//
// The field widths are localparams and the elaboration guard below requires
// them to sum to PAYLOAD_W: a payload width is a frozen copy of yesterday's
// agreement, and the guard is what makes the next widening loud.
//
// ---------------------------------------------------------------------------
// THREE COPIES, ONE FILL — the read-port cost is the cost (roadmap §8.4)
// ---------------------------------------------------------------------------
// A cached triangle still needs THREE corner reads. The primitive has ONE
// lookup port, so one copy replays one triangle per THREE clocks — exactly the
// legacy projector's rate, and exactly the rate the two-view stress fails
// (3,145,728 corner reads in a 1,333,333-clock window; PROJ-ARENA3 §6). This
// block instantiates the primitive `CORNERS` = 3 times with the open, origin,
// fill and seal channels BROADCAST — same enable, same data, same edge — so a
// copy stays a copy by construction, and each copy serves one corner. Three
// simultaneous corner reads = one triangle per clock.
//
// The price is 3x memory: 4 x 81 rows x 106 bits = 34,344 bits per copy.
// M10K arithmetic (GEOMETRY, not a fitted receipt — the fit gate in the
// report is what turns it into one): at 512x20, ceil(106/20) = 6 blocks wide,
// 324 rows deep fits one 512-row block; at 256x40, 2 deep x ceil(106/40) = 3
// wide = 6. Six per copy, EIGHTEEN total. The bit floor 34,344 / 10,240 = 3.4
// is unreachable at any legal aspect with one write + one read port, so six
// is the honest per-copy count.
//
// THE ALTERNATIVE NOT BUILT, stated so nobody rediscovers it: one copy and a
// three-clock corner sequencer, at 6 M10K and the legacy rate. Whether one
// triangle per clock is worth 12 M10K depends on what GEOM.CLIP downstream can
// consume, which nothing in this tree measures yet. A `REPLICAS` parameter was
// NOT added: a knob nobody consumes is a guess baked into silicon (the same
// reasoning that kept it out of the primitive).
//
// ---------------------------------------------------------------------------
// THE REPLAY HANDSHAKE — a credit-gated two-deep skid, and why not a rigid pipe
// ---------------------------------------------------------------------------
// The primitive's reply register has NO enable: a lookup presented at T is
// answered at T+1 for exactly one cycle, and the primitive's hit/miss/refusal
// counters count every lookup PRESENTED. So this block cannot freeze the
// primitive under a downstream stall (no enable to freeze), and it must not
// re-present the same lookup while stalled (the counters would count it
// again — a diagnosis inflated by backpressure). Instead the lookup is issued
// only when its reply is GUARANTEED a slot:
//
//     issue_ok = (cnt + land - pop) <= 1
//
// where `cnt` is the skid's occupancy (0..2), `land` says a reply lands this
// cycle, `pop` says the consumer drains one this cycle. With one cycle of
// reply latency and two slots that is the exact condition for the landing
// reply to always find room, and it sustains one triangle per clock when the
// consumer is always ready (steady state cnt=1, land=1, pop=1). A ONE-slot
// output register would halve the rate: the issue could never overlap the
// in-flight reply. The skid costs 2 x ~360 flops; the alternative was a rate.
//
// ---------------------------------------------------------------------------
// WHAT A REFUSED OR MISSED CORNER PRODUCES
// ---------------------------------------------------------------------------
// The primitive answers a refusal with the memory's registered output (its
// header: zeroing there would cost a mux on the widest path in the block for
// a value the contract already says must not be read). THIS block zeroes it:
// a corner that is not a HIT carries x = y = d = w = 0, behind = 0, and the
// triangle is flagged on `out_refused_o` / `out_missed_o`. A consumer that
// ignores the flags reads a zero vertex — the core's own behind-the-eye
// convention — and NEVER another group's vertex. The flags are separate from
// `out_behind_o` on purpose: a fault and a legitimate near-plane rejection
// must not share a bit, or a producer bug looks like a camera position.
//
// In DENSE_SEAL mode a legal lookup structurally cannot miss (proved:
// a_dense_no_miss), so `out_missed_o` and `corner_misses_o` are zero for the
// whole dense differential; they are SEEN TO FIRE by elaborating the same
// shell at VALID_MODE=0 (a sealed-but-incomplete bitmap arena), which is the
// "break a layout with a parameter" class of positive control (CLAUDE.md,
// 2026-09-09), and the primitive's own committed mutant covers the dense path.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DOES NOT DO (named, so the remaining list is honest)
// ---------------------------------------------------------------------------
//   * It does not generate corner references. The level-0 unstitched walker is
//     `zhao_terrain_topo`; stitched and coarse topologies are the
//     tessellator's and must come from it (see the adoption report).
//   * It does not produce the 81 vertices. The fill producer is whoever feeds
//     client B of `zhao_project_service` with lattice vertices and a
//     {arena, index} rider; today that is the differential's harness.
//   * It does not reference-count groups. `zhao_terrain_topo.hold_o` says
//     which arena a walk is reading; a producer that reopens it anyway gets
//     deterministic refusals, counted — never a silent wrong vertex.
//   * The origin (datum) channel is tied off: terrain vertices arrive at full
//     width from the lattice port, so there is nothing to rebase.
//
// Conservative SystemVerilog subset only (charter §2); explicit generate /
// endgenerate and the $fatal inside `initial begin` — both Quartus-17 laws
// (CLAUDE.md, Build note, 2026-09-08).
`default_nettype none

module zhao_terrain_wcache #(
    // 2 views x 2 working generations: per view one group fills (81 clocks at
    // II=1) while the previous one replays (128 clocks at level 0). PROPOSED,
    // owner-editable; the composition that sequences the views is the thing
    // that should set it.
    parameter int unsigned ARENAS     = 4,
    // (SubCells + 1)^2 = 9x9 = 81 lattice vertices of one 8x8-cell subpatch
    // (charter §11.1). The identity argument for 81 -- every emitted corner is
    // a lattice vertex inside the window and a vertex's final position is a
    // pure function of (vi, vj, job) -- is MEASURED over the tessellator's
    // stitch/morph/underside/void cases by tests/terrain/terrain_identity_probe.
    parameter int unsigned DEPTH      = 81,
    parameter int unsigned GEN_W      = 8,
    // 1 = VALID_DENSE_SEAL (the terrain default: a whole subpatch is projected
    // before replay, in lattice order, so the fill is dense by construction).
    // 0 = VALID_BITMAP is legal too -- same ports, same replay -- and is what
    // the miss-path positive control elaborates.
    parameter int unsigned VALID_MODE = 1,
    parameter int unsigned INDEX_W    = $clog2(DEPTH) + 1,
    parameter int unsigned ARENA_W    = $clog2(ARENAS) + 1
) (
    input  wire clk,
    input  wire rst_n,

    // ---- producer: open / fill / seal (broadcast to every copy) -------------
    input  wire                  open_i,
    input  wire [ARENA_W-1:0]    open_arena_i,
    output wire [GEN_W-1:0]      open_gen_o,

    // The fill IS the projector's output packet plus its rider. In the
    // composed lane these come off `zhao_project_service.b_*_o` with the
    // {arena, index} carried in `b_payload_o` -- see tb_terrain_wcache.sv.
    input  wire                  fill_valid_i,
    output wire                  fill_ready_o,
    input  wire [ARENA_W-1:0]    fill_arena_i,
    input  wire [INDEX_W-1:0]    fill_index_i,
    input  wire signed [20:0]    fill_x_i,
    input  wire signed [20:0]    fill_y_i,
    input  wire signed [31:0]    fill_d_i,
    input  wire        [30:0]    fill_w_i,
    input  wire                  fill_behind_i,

    input  wire                  seal_i,
    input  wire [ARENA_W-1:0]    seal_arena_i,

    // ---- consumer: one triangle REFERENCE in ---------------------------------
    input  wire                  ref_valid_i,
    output wire                  ref_ready_o,
    input  wire [ARENA_W-1:0]    ref_arena_i,
    input  wire [GEN_W-1:0]      ref_gen_i,
    input  wire [INDEX_W-1:0]    ref_ia_i,
    input  wire [INDEX_W-1:0]    ref_ib_i,
    input  wire [INDEX_W-1:0]    ref_ic_i,
    // the riders: zhao_terrain_project's, so its consumer sees the same packet
    input  wire        [15:0]    ref_src_id_i,
    input  wire                  ref_view_i,
    input  wire        [ 7:0]    ref_mat_a_i,
    input  wire        [ 7:0]    ref_mat_b_i,
    input  wire        [ 7:0]    ref_weight_i,

    // ---- one projected TRIANGLE out: zhao_terrain_project's packet + w x3 ----
    output wire                  out_valid_o,
    input  wire                  out_ready_i,
    output wire signed [20:0]    out_ax_o,
    output wire signed [20:0]    out_ay_o,
    output wire signed [20:0]    out_bx_o,
    output wire signed [20:0]    out_by_o,
    output wire signed [20:0]    out_cx_o,
    output wire signed [20:0]    out_cy_o,
    output wire        [ 2:0]    out_behind_o,   // bit 0 = A, 1 = B, 2 = C
    output wire        [15:0]    out_src_id_o,
    output wire signed [31:0]    out_ad_o,
    output wire signed [31:0]    out_bd_o,
    output wire signed [31:0]    out_cd_o,
    output wire        [30:0]    out_aw_o,       // NEW against the legacy packet
    output wire        [30:0]    out_bw_o,
    output wire        [30:0]    out_cw_o,
    output wire                  out_view_o,
    output wire        [ 7:0]    out_mat_a_o,
    output wire        [ 7:0]    out_mat_b_o,
    output wire        [ 7:0]    out_weight_o,
    output wire                  out_refused_o,  // >= 1 corner REFUSED (caller bug)
    output wire                  out_missed_o,   // >= 1 corner MISSED (unfilled slot)

    // ---- counters (saturating) ------------------------------------------------
    output logic [31:0]          replay_triangles_o,  // triangles landed
    output logic [31:0]          replay_refused_o,    // triangles with a refused corner
    output logic [31:0]          replay_missed_o,     // triangles with a missed corner
    output logic [31:0]          corner_hits_o,       // corner replies that hit
    output logic [31:0]          corner_refusals_o,   // corner replies refused
    output logic [31:0]          corner_misses_o,     // corner replies missed
    // The fill-side sticky bits, from copy 0 (the fill is broadcast, so every
    // copy's bit is the same bit).
    output wire                  arena_overflow_o,
    output wire                  arena_seal_short_o,
    output wire                  idle_o
);

  // ---- the field map, once ----------------------------------------------------
  localparam int unsigned X_W = 21;
  localparam int unsigned Y_W = 21;
  localparam int unsigned D_W = 32;
  localparam int unsigned W_W = 31;
  localparam int unsigned B_W = 1;
  localparam int unsigned PAYLOAD_W = X_W + Y_W + D_W + W_W + B_W;  // 106
  localparam int unsigned X_LO = 0;
  localparam int unsigned Y_LO = X_LO + X_W;   // 21
  localparam int unsigned D_LO = Y_LO + Y_W;   // 42
  localparam int unsigned W_LO = D_LO + D_W;   // 74
  localparam int unsigned B_LO = W_LO + W_W;   // 105

  localparam int unsigned CORNERS = 3;

  // Quartus 17 requires this inside `initial begin`, and `--lint-only` does
  // not run initial blocks -- a clean lint says nothing about this check.
  initial begin
    if (PAYLOAD_W != 106)
      $fatal(1, "zhao_terrain_wcache: PAYLOAD_W (%0d) is not the 106-bit record zhao_geom_wcache carries",
             PAYLOAD_W);
    if (B_LO != 105 || W_LO != 74 || D_LO != 42 || Y_LO != 21)
      $fatal(1, "zhao_terrain_wcache: field map drifted from zhao_geom_wcache's");
    if (ARENAS < 1 || DEPTH < 1)
      $fatal(1, "zhao_terrain_wcache: ARENAS (%0d) and DEPTH (%0d) must be >= 1", ARENAS, DEPTH);
  end

  // ---- the record, packed once ------------------------------------------------
  wire [PAYLOAD_W-1:0] fill_payload_c = {fill_behind_i, fill_w_i, fill_d_i, fill_y_i, fill_x_i};

  // ---- the replay credit ------------------------------------------------------
  logic [1:0] cnt_q;                       // skid occupancy, 0..2
  wire        pop_c   = (cnt_q != 2'd0) && out_ready_i;
  wire        land_c;                      // a reply lands this cycle (copy 0's rep_valid_o)
  // cnt + land - pop <= 1, computed without a subtraction: enumerate.
  wire [1:0]  cnt_after_pop_c = cnt_q - {1'b0, pop_c};
  wire        issue_ok_c = ({1'b0, cnt_after_pop_c} + {2'b0, land_c}) <= 3'd1;
  wire        issue_c    = ref_valid_i && issue_ok_c;

  assign ref_ready_o = issue_ok_c;

  // ---- the three copies -------------------------------------------------------
  wire [INDEX_W-1:0]   look_index_c [0:CORNERS-1];
  assign look_index_c[0] = ref_ia_i;
  assign look_index_c[1] = ref_ib_i;
  assign look_index_c[2] = ref_ic_i;

  wire                 rep_valid_c   [0:CORNERS-1];
  wire                 rep_hit_c     [0:CORNERS-1];
  wire                 rep_refuse_c  [0:CORNERS-1];
  wire [PAYLOAD_W-1:0] rep_payload_c [0:CORNERS-1];
  wire                 fill_ready_c  [0:CORNERS-1];
  wire [GEN_W-1:0]     open_gen_c    [0:CORNERS-1];
  wire                 overflow_c    [0:CORNERS-1];
  wire                 seal_short_c  [0:CORNERS-1];

  genvar gk;
  generate
    for (gk = 0; gk < CORNERS; gk = gk + 1) begin : g_copy
      // Per-copy outputs this shell does not consume. The counters are
      // re-derived at triangle granularity below (a per-copy count would be a
      // per-corner count, and the interesting number is per triangle); the
      // origin datum is not used by terrain at all.
      logic [31:0]        hits_unused, misses_unused, refusals_unused;
      logic signed [31:0] org_x_unused, org_y_unused, org_z_unused;
      logic               look_ready_unused;   // constant 1 in the primitive; the credit is ours

      zhao_vertex_arena #(
          .ARENAS    (ARENAS),
          .DEPTH     (DEPTH),
          .PAYLOAD_W (PAYLOAD_W),
          .GEN_W     (GEN_W),
          .VALID_MODE(VALID_MODE),
          .INDEX_W   (INDEX_W),
          .ARENA_W   (ARENA_W)
      ) u_arena (
          .clk               (clk),
          .rst_n             (rst_n),
          .open_i            (open_i),
          .open_arena_i      (open_arena_i),
          .open_gen_o        (open_gen_c[gk]),
          // No datum: terrain vertices are full-width world positions.
          .org_we_i          (1'b0),
          .org_arena_i       ({ARENA_W{1'b0}}),
          .org_x_i           (32'sd0),
          .org_y_i           (32'sd0),
          .org_z_i           (32'sd0),
          .fill_valid_i      (fill_valid_i),
          .fill_ready_o      (fill_ready_c[gk]),
          .fill_arena_i      (fill_arena_i),
          .fill_index_i      (fill_index_i),
          .fill_payload_i    (fill_payload_c),
          .seal_i            (seal_i),
          .seal_arena_i      (seal_arena_i),
          .look_valid_i      (issue_c),
          .look_ready_o      (look_ready_unused),
          .look_arena_i      (ref_arena_i),
          .look_gen_i        (ref_gen_i),
          .look_index_i      (look_index_c[gk]),
          .rep_valid_o       (rep_valid_c[gk]),
          .rep_hit_o         (rep_hit_c[gk]),
          .rep_refuse_o      (rep_refuse_c[gk]),
          .rep_payload_o     (rep_payload_c[gk]),
          .rep_org_x_o       (org_x_unused),
          .rep_org_y_o       (org_y_unused),
          .rep_org_z_o       (org_z_unused),
          .arena_hits_o      (hits_unused),
          .arena_misses_o    (misses_unused),
          .arena_refusals_o  (refusals_unused),
          .arena_overflow_o  (overflow_c[gk]),
          .arena_seal_short_o(seal_short_c[gk])
      );
    end
  endgenerate

  // A copy stays a copy: the fill channels are one wire fanned out, so
  // copy 0 speaks for all three on the fill side.
  assign fill_ready_o       = fill_ready_c[0] && fill_ready_c[1] && fill_ready_c[2];
  assign open_gen_o         = open_gen_c[0];
  assign arena_overflow_o   = overflow_c[0];
  assign arena_seal_short_o = seal_short_c[0];
  assign land_c             = rep_valid_c[0];

  // ---- the riders, aligned with the reply -------------------------------------
  // Registered at issue so they land on the same edge as the three replies.
  logic        [15:0] rd_src_q;
  logic               rd_view_q;
  logic        [ 7:0] rd_mat_a_q, rd_mat_b_q, rd_weight_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rd_src_q    <= '0;
      rd_view_q   <= 1'b0;
      rd_mat_a_q  <= '0;
      rd_mat_b_q  <= '0;
      rd_weight_q <= '0;
    end else if (issue_c) begin
      rd_src_q    <= ref_src_id_i;
      rd_view_q   <= ref_view_i;
      rd_mat_a_q  <= ref_mat_a_i;
      rd_mat_b_q  <= ref_mat_b_i;
      rd_weight_q <= ref_weight_i;
    end
  end

  // ---- the landing triangle ---------------------------------------------------
  // One record per corner, zeroed unless the corner HIT (header: what a refused
  // or missed corner produces).
  localparam int unsigned TRI_W = 3 * PAYLOAD_W + 16 + 1 + 24 + 2;  // 361

  logic [PAYLOAD_W-1:0] corner_c [0:CORNERS-1];
  logic [CORNERS-1:0]   miss_c;
  logic [CORNERS-1:0]   refuse_c;
  logic [CORNERS-1:0]   hit_c;
  integer ci;
  always_comb begin
    for (ci = 0; ci < CORNERS; ci = ci + 1) begin
      hit_c[ci]    = rep_hit_c[ci];
      refuse_c[ci] = rep_refuse_c[ci];
      miss_c[ci]   = !rep_hit_c[ci] && !rep_refuse_c[ci];
      corner_c[ci] = rep_hit_c[ci] ? rep_payload_c[ci] : {PAYLOAD_W{1'b0}};
    end
  end

  wire [TRI_W-1:0] land_tri_c = {
      |miss_c, |refuse_c,
      rd_weight_q, rd_mat_b_q, rd_mat_a_q, rd_view_q, rd_src_q,
      corner_c[2], corner_c[1], corner_c[0]};

  // ---- the two-deep skid ------------------------------------------------------
  logic [TRI_W-1:0] q0_q, q1_q;   // q0 is the head, presented on out_*
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      cnt_q <= 2'd0;
      q0_q  <= '0;
      q1_q  <= '0;
    end else begin
      cnt_q <= cnt_after_pop_c + {1'b0, land_c};
      // shift on pop
      if (pop_c && cnt_q == 2'd2) q0_q <= q1_q;
      // place the landing reply behind whatever remains after the pop
      if (land_c) begin
        if (cnt_after_pop_c == 2'd0) q0_q <= land_tri_c;
        else                         q1_q <= land_tri_c;   // cnt_after_pop == 1 (never 2: the credit)
      end
    end
  end

  assign out_valid_o = (cnt_q != 2'd0);

  // ---- unpack the head, the one place the field map is read -------------------
  wire [PAYLOAD_W-1:0] ha_c = q0_q[0*PAYLOAD_W +: PAYLOAD_W];
  wire [PAYLOAD_W-1:0] hb_c = q0_q[1*PAYLOAD_W +: PAYLOAD_W];
  wire [PAYLOAD_W-1:0] hc_c = q0_q[2*PAYLOAD_W +: PAYLOAD_W];
  localparam int unsigned R_LO = 3 * PAYLOAD_W;  // riders start here

  assign out_ax_o      = ha_c[X_LO +: X_W];
  assign out_ay_o      = ha_c[Y_LO +: Y_W];
  assign out_ad_o      = ha_c[D_LO +: D_W];
  assign out_aw_o      = ha_c[W_LO +: W_W];
  assign out_bx_o      = hb_c[X_LO +: X_W];
  assign out_by_o      = hb_c[Y_LO +: Y_W];
  assign out_bd_o      = hb_c[D_LO +: D_W];
  assign out_bw_o      = hb_c[W_LO +: W_W];
  assign out_cx_o      = hc_c[X_LO +: X_W];
  assign out_cy_o      = hc_c[Y_LO +: Y_W];
  assign out_cd_o      = hc_c[D_LO +: D_W];
  assign out_cw_o      = hc_c[W_LO +: W_W];
  assign out_behind_o  = {hc_c[B_LO], hb_c[B_LO], ha_c[B_LO]};
  assign out_src_id_o  = q0_q[R_LO      +: 16];
  assign out_view_o    = q0_q[R_LO + 16];
  assign out_mat_a_o   = q0_q[R_LO + 17 +: 8];
  assign out_mat_b_o   = q0_q[R_LO + 25 +: 8];
  assign out_weight_o  = q0_q[R_LO + 33 +: 8];
  assign out_refused_o = q0_q[R_LO + 41];
  assign out_missed_o  = q0_q[R_LO + 42];

  // ---- counters ---------------------------------------------------------------
  // Corner counts add a popcount (0..3) per landing; triangle counts add one.
  // Saturating, like every counter in this subsystem.
  function automatic logic [1:0] pop3(input logic [2:0] v);
    pop3 = {1'b0, v[0]} + {1'b0, v[1]} + {1'b0, v[2]};
  endfunction

  function automatic logic [31:0] sat_add(input logic [31:0] a, input logic [1:0] d);
    logic [32:0] s;
    begin
      s = {1'b0, a} + {31'b0, d};
      sat_add = s[32] ? 32'hFFFF_FFFF : s[31:0];
    end
  endfunction

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      replay_triangles_o <= '0;
      replay_refused_o   <= '0;
      replay_missed_o    <= '0;
      corner_hits_o      <= '0;
      corner_refusals_o  <= '0;
      corner_misses_o    <= '0;
    end else if (land_c) begin
      replay_triangles_o <= sat_add(replay_triangles_o, 2'd1);
      if (|refuse_c) replay_refused_o <= sat_add(replay_refused_o, 2'd1);
      if (|miss_c)   replay_missed_o  <= sat_add(replay_missed_o, 2'd1);
      corner_hits_o      <= sat_add(corner_hits_o,     pop3(hit_c));
      corner_refusals_o  <= sat_add(corner_refusals_o, pop3(refuse_c));
      corner_misses_o    <= sat_add(corner_misses_o,   pop3(miss_c));
    end
  end

  // Nothing in the skid and nothing landing. A reply in flight IS work.
  assign idle_o = (cnt_q == 2'd0) && !land_c;

endmodule : zhao_terrain_wcache

`default_nettype wire
