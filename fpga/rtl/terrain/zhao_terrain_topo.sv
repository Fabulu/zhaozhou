// zhao_terrain_topo.sv — the LEVEL-0 UNSTITCHED subpatch topology walker: one
// sealed 9x9 group in, 128 triangle references out, one per clock.
//
// Law (in citation order):
//   spec/terrain_rules.md §4.3 — the FIXED i00-i11 diagonal and the emit
//       order, (i00, i11, i10) then (i00, i01, i11), which §4.3 pins to
//       `draw_heightfield`; §5 — the underside is the same diagonal with
//       INVERTED winding.
//   fpga/rtl/terrain/zhao_terrain_tess.sv — the tessellator's own walk: run-
//       cells in z-then-x scan order (x fastest), the §4.3 pair per cell, and
//       "the underside is the top's pair with b and c swapped — the ONE place
//       the inverted winding lives". This block reproduces that walk at level
//       0 and NOTHING ELSE (see below).
//   reference/include/zref/zref_terrain_tess.hpp — `tessellate`'s unstitched
//       path, which the differential uses as the triangle oracle.
//   fpga/rtl/terrain/zhao_terrain_wcache.sv — the consumer of the references.
//
// ENFORCED-BY: tests/terrain/terrain_wcache_differential.cpp:main
//
// ---------------------------------------------------------------------------
// THE TABLE IS THREE ADDERS
// ---------------------------------------------------------------------------
// The roadmap describes "a static 128-triangle subpatch topology table (3 x
// 7-bit corner indices = 21 bits x 128 rows -- one M10K or LUTRAM)". At level
// 0 the table is not worth a memory: with the lattice index
// idx(vi, vj) = vj * 9 + vi over the 9x9 window, cell (a, b) has
//
//     i00 = b*9 + a        i10 = i00 + 1
//     i01 = i00 + 9        i11 = i00 + 10
//
// so the two triangles are {i00, i00+10, i00+1} and {i00, i00+9, i00+10}, and
// the whole "table" is a cell counter and three constant adds. No ROM, no
// M10K, nothing to initialise, nothing that can go stale.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DOES NOT WALK, and why that is the real remaining work
// ---------------------------------------------------------------------------
// Only the level-0 UNSTITCHED subpatch has a static topology. A coarsened
// subpatch (level 1..3) walks (8 >> level)^2 run-cells at stride 1 << level,
// and a STITCHED subpatch is an annulus whose ring fans depend on all four
// neighbour levels -- job-dependent topology that `zhao_terrain_tess` already
// enumerates (its `inner_v` / `outer_v` / `proj_t` functions) and this block
// does not duplicate. The correct owner of corner references for those cases
// is the tessellator, emitting the (vi, vj) triple it already holds instead
// of expanding it into world coordinates. Until that exists, this block
// covers the level-0 case the roadmap's whole throughput argument is built on
// (16 x 128 triangles per patch) and the report says so plainly.
//
// Void cells at level 0: the tessellator SKIPS a run-cell whose patch cell is
// not SOLID. This walker has no cell-state port and emits all 64 cells; a
// dual-page composition must either mask void cells at the consumer or feed
// this block a 64-bit solidity mask -- named, not built, because no dual-page
// composition exists to say which.
//
// ---------------------------------------------------------------------------
// HOLD, NOT A REFERENCE COUNT
// ---------------------------------------------------------------------------
// `hold_o` / `hold_arena_o` say "a walk is reading this arena" from job accept
// until the last reference is ACCEPTED by the shell. That is sufficient: the
// primitive answers a lookup from the metadata of the cycle it is presented,
// so an open on the same edge as the last acceptance cannot disturb it, and a
// producer that reopens a held arena anyway gets deterministic refusals,
// counted by the shell -- never a silent wrong vertex. The design study's
// per-group reference COUNTER was for consumers that retain handles beyond
// the reply; this shell's consumer receives payloads, not handles, so there
// is nothing to retain and nothing to count.
//
// Conservative SystemVerilog subset only (charter §2).
`default_nettype none

module zhao_terrain_topo #(
    parameter int unsigned ARENAS  = 4,
    parameter int unsigned GEN_W   = 8,
    parameter int unsigned INDEX_W = $clog2(81) + 1,
    parameter int unsigned ARENA_W = $clog2(ARENAS) + 1
) (
    input  wire clk,
    input  wire rst_n,

    // ---- one sealed group to replay ---------------------------------------
    input  wire                 job_valid_i,
    output wire                 job_ready_o,
    input  wire [ARENA_W-1:0]   job_arena_i,
    input  wire [GEN_W-1:0]     job_gen_i,
    input  wire                 job_surface_i,   // 0 = top, 1 = underside (b/c swapped)
    input  wire        [15:0]   job_src_id_i,
    input  wire                 job_view_i,
    input  wire        [ 7:0]   job_mat_a_i,
    input  wire        [ 7:0]   job_mat_b_i,
    input  wire        [ 7:0]   job_weight_i,

    // ---- 128 triangle references out ---------------------------------------
    output wire                 ref_valid_o,
    input  wire                 ref_ready_i,
    output wire [ARENA_W-1:0]   ref_arena_o,
    output wire [GEN_W-1:0]     ref_gen_o,
    output wire [INDEX_W-1:0]   ref_ia_o,
    output wire [INDEX_W-1:0]   ref_ib_o,
    output wire [INDEX_W-1:0]   ref_ic_o,
    output wire        [15:0]   ref_src_id_o,
    output wire                 ref_view_o,
    output wire        [ 7:0]   ref_mat_a_o,
    output wire        [ 7:0]   ref_mat_b_o,
    output wire        [ 7:0]   ref_weight_o,

    // ---- observation ---------------------------------------------------------
    output wire                 hold_o,          // a walk is reading hold_arena_o
    output wire [ARENA_W-1:0]   hold_arena_o,
    output wire                 done_o,          // one-cycle pulse: last reference accepted
    output logic [31:0]         jobs_done_o
);

  localparam int unsigned SubCells = 8;                 // charter §11.1
  localparam int unsigned Side     = SubCells + 1;      // 9 lattice vertices per side
  localparam int unsigned Depth    = Side * Side;       // 81
  localparam int unsigned Cells    = SubCells * SubCells;  // 64

  initial begin
    if (INDEX_W < $clog2(Depth) + 1)
      $fatal(1, "zhao_terrain_topo: INDEX_W (%0d) cannot carry index %0d plus the refusal bit",
             INDEX_W, Depth - 1);
  end

  // ---- the held job ---------------------------------------------------------
  logic               busy_q;
  logic [ARENA_W-1:0] arena_q;
  logic [GEN_W-1:0]   gen_q;
  logic               surf_q;
  logic [15:0]        src_q;
  logic               view_q;
  logic [7:0]         mat_a_q, mat_b_q, weight_q;

  // the walk: cell (a, b), triangle t within the cell
  logic [2:0] a_q, b_q;
  logic       t_q;

  wire accept_c = busy_q && ref_ready_i;
  wire last_c   = (a_q == 3'd7) && (b_q == 3'd7) && t_q;

  assign job_ready_o = !busy_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      busy_q   <= 1'b0;
      arena_q  <= '0;
      gen_q    <= '0;
      surf_q   <= 1'b0;
      src_q    <= '0;
      view_q   <= 1'b0;
      mat_a_q  <= '0;
      mat_b_q  <= '0;
      weight_q <= '0;
      a_q      <= '0;
      b_q      <= '0;
      t_q      <= 1'b0;
      jobs_done_o <= '0;
    end else begin
      if (!busy_q) begin
        if (job_valid_i) begin
          busy_q   <= 1'b1;
          arena_q  <= job_arena_i;
          gen_q    <= job_gen_i;
          surf_q   <= job_surface_i;
          src_q    <= job_src_id_i;
          view_q   <= job_view_i;
          mat_a_q  <= job_mat_a_i;
          mat_b_q  <= job_mat_b_i;
          weight_q <= job_weight_i;
          a_q      <= '0;
          b_q      <= '0;
          t_q      <= 1'b0;
        end
      end else if (accept_c) begin
        // z-then-x scan: the second triangle of the cell, then x, then z.
        if (!t_q) begin
          t_q <= 1'b1;
        end else begin
          t_q <= 1'b0;
          if (a_q == 3'd7) begin
            a_q <= '0;
            b_q <= b_q + 3'd1;   // wraps to 0 on the last cell, unused
          end else begin
            a_q <= a_q + 3'd1;
          end
        end
        if (last_c) begin
          busy_q <= 1'b0;
          if (jobs_done_o != 32'hFFFF_FFFF) jobs_done_o <= jobs_done_o + 32'd1;
        end
      end
    end
  end

  // ---- the three adders ----------------------------------------------------------
  // i00 = b*9 + a, computed as (b << 3) + b + a -- a constant multiply.
  wire [INDEX_W-1:0] i00_c = INDEX_W'({4'b0, b_q, 3'b0}) + INDEX_W'({4'b0, b_q}) + INDEX_W'({4'b0, a_q});
  wire [INDEX_W-1:0] i10_c = i00_c + INDEX_W'(1);
  wire [INDEX_W-1:0] i01_c = i00_c + INDEX_W'(Side);
  wire [INDEX_W-1:0] i11_c = i00_c + INDEX_W'(Side + 1);

  // top:       t=0 (i00, i11, i10)   t=1 (i00, i01, i11)
  // underside: the same pair with b and c swapped
  wire [INDEX_W-1:0] top_b_c = t_q ? i01_c : i11_c;
  wire [INDEX_W-1:0] top_c_c = t_q ? i11_c : i10_c;

  assign ref_valid_o  = busy_q;
  assign ref_arena_o  = arena_q;
  assign ref_gen_o    = gen_q;
  assign ref_ia_o     = i00_c;
  assign ref_ib_o     = surf_q ? top_c_c : top_b_c;
  assign ref_ic_o     = surf_q ? top_b_c : top_c_c;
  assign ref_src_id_o = src_q;
  assign ref_view_o   = view_q;
  assign ref_mat_a_o  = mat_a_q;
  assign ref_mat_b_o  = mat_b_q;
  assign ref_weight_o = weight_q;

  assign hold_o       = busy_q;
  assign hold_arena_o = arena_q;
  assign done_o       = accept_c && last_c;

  // Silence: Cells and Depth are documentation constants the guard above uses.
  wire unused_ok = (Cells == 64) && (Depth == 81);

endmodule : zhao_terrain_topo

`default_nettype wire
