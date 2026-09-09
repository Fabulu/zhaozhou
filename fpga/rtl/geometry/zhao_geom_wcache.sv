// zhao_geom_wcache.sv — GEOM.WCACHE: the projected-vertex arena, so a vertex
// shared by several primitives is projected ONCE.
//
// Law (in citation order):
//   design/contracts/GEOM.WCACHE.md — the block contract: the channel layouts,
//       the six deterministic refusals, READ-OLD, and the throughput claim this
//       block exists to deliver.
//   fpga/rtl/geometry/zhao_vertex_arena.sv — the primitive this instantiates,
//       which owns the memory, the valid mechanism and the formal proof.
//
// ENFORCED-BY: tests/geometry/geom_wcache_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS IS A SHELL AND NOT AN IMPLEMENTATION
// ---------------------------------------------------------------------------
// Owner ruling 2026-08-24: "Build a reusable parameterized arena primitive and a
// GEOM.WCACHE shell. Terrain may later instantiate the same primitive with its
// own depth/payload. This does not mean one physical cache shared between the
// two pipelines."
//
// So the arena mechanism — the direct-indexed store, the flop valid bitmap, the
// generation, the six refusals, the READ-OLD semantics and the formal shadow
// proof — lives once, in `zhao_vertex_arena`. This block is the projected-vertex
// INSTANCE of it: it fixes the parameters, names the payload, and is the thing
// GEOM.PROJECT and the tessellator talk to. Nothing here re-implements anything;
// a second copy of that valid mechanism is exactly what the ruling forbids.
//
// The shell is not ceremony. It is where the payload's MEANING is written down —
// the primitive stores `PAYLOAD_W` opaque bits and must not know what they are,
// and somebody has to say.
//
// ---------------------------------------------------------------------------
// THE PAYLOAD, FIELD BY FIELD
// ---------------------------------------------------------------------------
// Exactly GEOM.PROJECT's output packet, packed LSB-first:
//
//     [20:0]    screen x, S 12.8, guard-band clamped   (out_x_o)
//     [41:21]   screen y, S 12.8                       (out_y_o)
//     [73:42]   invw, Q16.16                           (out_d_o)
//     [104:74]  w, guarded clip w, fx16 raw            (out_w_o)
//     [105]     behind: clip.w <= 0                    (out_behind_o)
//
// 106 bits. WIDENED 75 -> 106 ON 2026-09-09, and the widening is a REPAIR,
// not a feature: GEOM.DEPTHQUANT's correction of 2026-09-03 ("THE INPUT IS
// `w`, NOT `1/w`") made the consumer take `w` and perform its own reciprocal,
// because recovering `w` from the ROUNDED Q16.16 `1/w` compounds a rounding
// that already happened. `zhao_project_core` and `zhao_geom_project` grew
// `out_w_o [30:0]` the same day -- and this cache, written three days BEFORE
// the correction, kept forwarding a packet with no `w` in it. A payload width
// is a frozen copy of yesterday's agreement
// (reports/WCACHE-DROPS-W-20260909.md). The 31-bit width is the core's own:
// DEPTHQUANT's 40-bit `v_w_i` is input headroom its first-act clamp
// immediately bounds to <= 2^30 (WORLD_LONG wmax), so a zero-extend of the
// guarded 31-bit w is lossless. The `behind` bit moves from 74 to 105;
// x/y/invw stay where they were.
//
// The ATTRIBUTES do not live here. A projected vertex's u/v and lit colour are
// per-vertex too, but the attribute-bearing packet is still a spec question
// (reports/OPEN-SPEC-DEPTH-QUANTISATION.md), and widening this payload before
// that is settled would bake a guess into a memory. When it is settled, this
// parameter moves and nothing else does. (Tested by THIS widening: the
// parameter moved, plus exactly two mechanical regenerations -- the generated
// zhao_prod_top's harness wires, via gen_prod_top.py, and this header's own
// field map. The primitive, the oracle, the directed suites and the formal
// proof all carry PAYLOAD_W symbolically and did not move.)
//
// ---------------------------------------------------------------------------
// WHY TWO ARENAS, AND WHY THAT IS NOT A CACHE OF TWO WAYS
// ---------------------------------------------------------------------------
// ARENAS = 2 is ONE ARENA PER VIEW, not two ways of an associative structure.
// The two views have different projection matrices, so a vertex projected for
// view 0 is not the answer for view 1 — the sharing this block delivers is
// WITHIN a view, across the several primitives that reference the same vertex.
// "Project once, replay twice" is about the two views each replaying their own
// arena, not about one arena serving both.
//
// ---------------------------------------------------------------------------
// THE NUMBER THIS BLOCK EXISTS FOR
// ---------------------------------------------------------------------------
// A 33x33 terrain patch is 1,089 distinct vertices and its triangles present
// 6,144 vertex references — 2,048 triangles times three. Replay therefore
// removes 5,055 of 6,144 projections, 82.3%, before any width or rate work is
// considered. That is a claim about arithmetic AND about this block delivering
// it, so the test drives a real 33x33 patch through it and measures the hit
// rate rather than restating the fraction.
`default_nettype none

module zhao_geom_wcache #(
    // One arena per view.
    parameter int unsigned ARENAS    = 2,
    // 33x33 vertices, the terrain lattice; a meshlet's vertex count fits under
    // the same bound (charter 15 caps a meshlet at 96-126 triangles).
    parameter int unsigned DEPTH     = 1089,
    // GEOM.PROJECT's packet: 21 + 21 + 32 + 31 + 1. See THE PAYLOAD above.
    parameter int unsigned PAYLOAD_W = 106,
    parameter int unsigned GEN_W     = 8,
    // ONE BIT WIDER THAN THE ADDRESS, deliberately: an out-of-range index must
    // be REFUSED deterministically, and a caller can only present one if the
    // port can carry it. The primitive's header records why sizing it exactly
    // makes the check vacuous.
    parameter int unsigned INDEX_W   = $clog2(DEPTH) + 1,
    parameter int unsigned ARENA_W   = $clog2(ARENAS) + 1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- producer: open / origin / fill / seal -------------------------------
    input  var logic                 open_i,
    input  var logic [ARENA_W-1:0]   open_arena_i,
    output var logic [GEN_W-1:0]     open_gen_o,

    input  var logic                 org_we_i,
    input  var logic [ARENA_W-1:0]   org_arena_i,
    input  var logic signed [31:0]   org_x_i,
    input  var logic signed [31:0]   org_y_i,
    input  var logic signed [31:0]   org_z_i,

    input  var logic                 fill_valid_i,
    output var logic                 fill_ready_o,
    input  var logic [ARENA_W-1:0]   fill_arena_i,
    input  var logic [INDEX_W-1:0]   fill_index_i,
    input  var logic [PAYLOAD_W-1:0] fill_payload_i,

    input  var logic                 seal_i,
    input  var logic [ARENA_W-1:0]   seal_arena_i,

    // ---- consumer: lookup / replay -------------------------------------------
    input  var logic                 look_valid_i,
    output var logic                 look_ready_o,
    input  var logic [ARENA_W-1:0]   look_arena_i,
    input  var logic [GEN_W-1:0]     look_gen_i,
    input  var logic [INDEX_W-1:0]   look_index_i,

    output var logic                 rep_valid_o,
    output var logic                 rep_hit_o,
    output var logic                 rep_refuse_o,
    output var logic [PAYLOAD_W-1:0] rep_payload_o,
    output var logic signed [31:0]   rep_org_x_o,
    output var logic signed [31:0]   rep_org_y_o,
    output var logic signed [31:0]   rep_org_z_o,

    // ---- counters -------------------------------------------------------------
    output var logic [31:0] arena_hits_o,
    output var logic [31:0] arena_misses_o,
    output var logic [31:0] arena_refusals_o,
    output var logic        arena_overflow_o
);

  // The primitive's dense-seal refusal bit is meaningless in bitmap mode
  // (constant 0) and is tied off here rather than surfaced -- surfacing a
  // wire that structurally cannot move would be a counter nobody can fire.
  logic arena_seal_short_unused;

  zhao_vertex_arena #(
      .ARENAS   (ARENAS),
      .DEPTH    (DEPTH),
      .PAYLOAD_W(PAYLOAD_W),
      .GEN_W    (GEN_W),
      // 0 = VALID_BITMAP, the per-slot valid flops. This shell's producer
      // fills on lookup misses, in triangle order -- sparse, so the dense
      // mechanism's in-order restriction does not apply here. The terrain
      // shell is the dense customer (VALID_MODE = 1).
      .VALID_MODE(0),
      .INDEX_W  (INDEX_W),
      .ARENA_W  (ARENA_W)
  ) u_arena (
      .clk             (clk),
      .rst_n           (rst_n),
      .open_i          (open_i),
      .open_arena_i    (open_arena_i),
      .open_gen_o      (open_gen_o),
      .org_we_i        (org_we_i),
      .org_arena_i     (org_arena_i),
      .org_x_i         (org_x_i),
      .org_y_i         (org_y_i),
      .org_z_i         (org_z_i),
      .fill_valid_i    (fill_valid_i),
      .fill_ready_o    (fill_ready_o),
      .fill_arena_i    (fill_arena_i),
      .fill_index_i    (fill_index_i),
      .fill_payload_i  (fill_payload_i),
      .seal_i          (seal_i),
      .seal_arena_i    (seal_arena_i),
      .look_valid_i    (look_valid_i),
      .look_ready_o    (look_ready_o),
      .look_arena_i    (look_arena_i),
      .look_gen_i      (look_gen_i),
      .look_index_i    (look_index_i),
      .rep_valid_o     (rep_valid_o),
      .rep_hit_o       (rep_hit_o),
      .rep_refuse_o    (rep_refuse_o),
      .rep_payload_o   (rep_payload_o),
      .rep_org_x_o     (rep_org_x_o),
      .rep_org_y_o     (rep_org_y_o),
      .rep_org_z_o     (rep_org_z_o),
      .arena_hits_o    (arena_hits_o),
      .arena_misses_o  (arena_misses_o),
      .arena_refusals_o(arena_refusals_o),
      .arena_overflow_o(arena_overflow_o),
      .arena_seal_short_o(arena_seal_short_unused)
  );

endmodule : zhao_geom_wcache

`default_nettype wire
