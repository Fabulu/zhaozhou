// zhao_probe_walk_earth.sv — Field v3 Phase 4 (reports/Fieldv3.md): the Earth
// lattice walker, the front of the composed Earth machine
//
//   prepared field descriptor -> [WALKER] -> v3 vector executor
//                             -> patch accumulator -> composed-height cache
//
// WHAT IT REPLACES, AND WHY THE WHOLE REARCHITECTURE TURNS ON IT
// --------------------------------------------------------------
// v2 moved lattice points INTO the engine through a generic 12-in/4-out host
// stream: 27,225 clocks per association against a 10,416-clock allowance, so
// the front end alone was at 261% of the frame before one instruction ran.
// This block deletes that transport. It GENERATES the points.
//
// It can, because of a property of the reference walk that is easy to miss:
// in `zref::render::compose_lattice` the world coordinates are SEPARABLE.
//
//     wx[i] = place_x(xform, lattice_lerp(env_x0, env_x1, i, w-1), 0)
//     wz[j] = place_z(xform, 0, lattice_lerp(env_z0, env_z1, j, h-1))
//
// wx depends only on i and wz only on j — 33 + 33 values, not 1,089 pairs.
// A vertex is (wx[i], wz[j]), so the walker holds two 33-entry tables and
// indexes them. That is the entire trick.
//
// THE TABLES ARE PREPARED, NOT COMPUTED HERE, AND THAT IS A DECISION
// ------------------------------------------------------------------
// `lattice_lerp` is `a + (span*num + den/2)/den` — a ROUNDED DIVIDE, not a
// running `origin + i*pitch` accumulation. The two disagree, and a walker
// that accumulated a pitch would drift away from the oracle at interior
// vertices in a way no single-vertex test would catch.
//
// Rather than put a divider in the fabric to reproduce a value that is
// IDENTICAL for every association on the patch and every frame the patch
// does not move, wx[] and wz[] are prepared by the ARM with the SAME
// `zref::` primitives the oracle uses, and loaded through the table port.
// That is the brief's own rule -- the ARM does the work that does not vary --
// and it costs 66 words per patch against the 27,225 clocks it replaces.
// ENFORCED-BY: tests/differential/field_walk_earth_directed.cpp:main
//
// THE COVERED BOX IS A HINT. THE PER-VERTEX TEST IS THE LAW.
// ----------------------------------------------------------
// The descriptor carries a covered index box (i0,i1,j0,j1) so a field that
// touches nine vertices does not cost a full-patch walk. It is prepared by
// the ARM, which already knows the footprint and the tables.
//
// The walker STILL applies §9.1's closed-interval test to every vertex it
// emits, and that test — not the box — decides the lane mask. So a box that
// is too LARGE is merely slow, and a box that is too SMALL is the ARM's bug
// and is visible as missing coverage rather than as silently wrong heights.
// Trusting the box instead of the test is a mutation, and it is caught.
// ENFORCED-BY: tests/differential/field_walk_earth_directed.cpp:main
//
// GROUPING: 297 GROUPS PER FULL PATCH, NOT 273
// ---------------------------------------------
// FIELD.SEQ.EARTH says "1,089 lattice vertices = 273 four-wide vector
// groups". That is the count for an ALIGNED flat packing, which is what the
// accumulator's INIT and DRAIN phases use. It is NOT what the update path
// costs.
//
// The walk is row-major (z-then-x, the cartridge patch order the reference
// records velocity in), and 33 is not a multiple of 4, so a group may not
// straddle a row: every row costs ceil(33/4) = 9 groups, and a full patch
// costs 9 * 33 = 297. Probe 5 measured exactly that. Budgeting the executor
// at 273 would under-provision it by 8.8%.
//
// Within a row the walker starts at the box's i0 rather than at 0, so groups
// are UNALIGNED in general. The accumulator takes an arbitrary base vertex
// with a four-lane mask and rotates banks by vertex mod 4, so unaligned
// bases are exactly what it is built for.
//
// ONE Z PER GROUP. A group never straddles a row, so all four lanes share
// wz[j]; only the four x values differ. That halves the group bus and is a
// property of the row-major walk, so it is asserted rather than assumed.
//
// TARGET: one group per clock, sustained, stalling only on backpressure.
// ENFORCED-BY: tests/differential/field_walk_earth_directed.cpp:main
//
// Law:
//   reports/Fieldv3.md          Phase 4 composed Earth machine
//   design/contracts/FIELD.SEQ.EARTH.md
//                               walker replaces point transport; ready/valid
//                               toward TERRAIN.PATCH's field-major reducer;
//                               nothing dropped; one result per active
//                               vertex per accepted association
//   spec/terrain_rules.md §9.1  closed intervals over the shared 33x33
//                               vertex lattice; a footprint-border vertex is
//                               INSIDE
//   spec/terrain_rules.md §4    lattice_lerp, one rounding per interior line
//   reference/src/zrender/terrain.cpp:compose_lattice   the oracle walk

`default_nettype none

module zhao_probe_walk_earth #(
    // The Earth profile's lattice. Named, editable knobs -- the 33 is the
    // cartridge patch page's grid, not a constant of nature.
    parameter int LAT_W = 33,
    parameter int LAT_H = 33
) (
    input var logic clk,
    input var logic rst_n,

    // ---- prepared lattice tables (loaded once per patch) -----------------
    // sel 0 = wx (indexed by i), sel 1 = wz (indexed by j).
    input var logic               lt_we_i,
    input var logic               lt_sel_i,
    input var logic        [ 5:0] lt_idx_i,
    input var logic signed [31:0] lt_val_i,

    // ---- association intake ----------------------------------------------
    input  var logic               as_valid_i,
    output var logic               as_ready_o,
    input  var logic signed [31:0] as_fp_x0_i,   // closed interval, fx16 raw
    input  var logic signed [31:0] as_fp_x1_i,
    input  var logic signed [31:0] as_fp_z0_i,
    input  var logic signed [31:0] as_fp_z1_i,
    input  var logic        [ 5:0] as_box_i0_i,  // covered index box: a HINT
    input  var logic        [ 5:0] as_box_i1_i,
    input  var logic        [ 5:0] as_box_j0_i,
    input  var logic        [ 5:0] as_box_j1_i,

    // ---- vector group output ---------------------------------------------
    output var logic               out_valid_o,
    input  var logic               out_ready_i,
    output var logic        [10:0] out_iv_o,     // base vertex; lane l = iv+l
    output var logic        [ 3:0] out_mask_o,   // lane covered (THE test)
    output var logic signed [31:0] out_z_o,      // shared by all four lanes
    output var logic signed [31:0] out_x0_o,
    output var logic signed [31:0] out_x1_o,
    output var logic signed [31:0] out_x2_o,
    output var logic signed [31:0] out_x3_o,
    output var logic               out_last_o,   // last group of association

    // ---- counters ---------------------------------------------------------
    output var logic [31:0] groups_emitted_o,
    output var logic [31:0] verts_covered_o
);

  // The tables. 33 x 32 b each -- small enough to be registers, which is why
  // four simultaneous wx reads cost nothing. A RAM here would need four
  // replicas to do the same job (the Field v2 register-file lesson).
  logic signed [31:0] wx[0:LAT_W-1];
  logic signed [31:0] wz[0:LAT_H-1];

  always_ff @(posedge clk) begin
    if (lt_we_i) begin
      if (lt_sel_i) wz[lt_idx_i] <= lt_val_i;
      else wx[lt_idx_i] <= lt_val_i;
    end
  end

  // ---- association state -------------------------------------------------
  logic               busy_r;
  logic signed [31:0] fp_x0_r, fp_x1_r, fp_z0_r, fp_z1_r;
  logic        [ 5:0] box_i0_r, box_i1_r, box_j1_r;
  logic        [ 5:0] cur_i_r;   // first vertex index of the current group
  logic        [ 5:0] cur_j_r;

  // The walk is empty when the prepared box is empty. An empty box still
  // costs one accepted association and zero groups -- NOT a hang.
  logic box_empty_c;
  assign box_empty_c = (as_box_i0_i > as_box_i1_i) || (as_box_j0_i > as_box_j1_i);

  assign as_ready_o = !busy_r;

  // ---- the group currently presented ------------------------------------
  logic [5:0] li_c[0:3];
  logic signed [31:0] lx_c[0:3];
  logic in_z_c;
  logic [3:0] mask_c;

  assign in_z_c = (wz[cur_j_r] >= fp_z0_r) && (wz[cur_j_r] <= fp_z1_r);

  always_comb begin
    for (int l = 0; l < 4; l++) begin
      li_c[l] = cur_i_r + 6'(l);
      lx_c[l] = wx[li_c[l] < 6'(LAT_W) ? li_c[l] : 6'd0];
      // THE LAW, applied per vertex: on the lattice, and inside the closed
      // footprint in BOTH axes. The box never appears in this expression.
      mask_c[l] = busy_r && (li_c[l] < 6'(LAT_W)) && in_z_c &&
                  (lx_c[l] >= fp_x0_r) && (lx_c[l] <= fp_x1_r);
    end
  end

  // Last group of the association: last group of the last row of the box.
  logic row_last_c, assoc_last_c;
  assign row_last_c   = (cur_i_r + 6'd4) > box_i1_r;
  assign assoc_last_c = row_last_c && (cur_j_r >= box_j1_r);

  assign out_valid_o = busy_r;
  assign out_iv_o    = 11'(cur_j_r) * 11'(LAT_W) + 11'(cur_i_r);
  assign out_mask_o  = mask_c;
  assign out_z_o     = wz[cur_j_r];
  assign out_x0_o    = lx_c[0];
  assign out_x1_o    = lx_c[1];
  assign out_x2_o    = lx_c[2];
  assign out_x3_o    = lx_c[3];
  assign out_last_o  = assoc_last_c;

  // ---- the walk ----------------------------------------------------------
  logic accept_c;
  assign accept_c = out_valid_o && out_ready_i;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      busy_r           <= 1'b0;
      cur_i_r          <= 6'd0;
      cur_j_r          <= 6'd0;
      fp_x0_r          <= 32'sd0;
      fp_x1_r          <= 32'sd0;
      fp_z0_r          <= 32'sd0;
      fp_z1_r          <= 32'sd0;
      box_i0_r         <= 6'd0;
      box_i1_r         <= 6'd0;
      box_j1_r         <= 6'd0;
      groups_emitted_o <= 32'd0;
      verts_covered_o  <= 32'd0;
    end else begin
      if (!busy_r) begin
        if (as_valid_i && !box_empty_c) begin
          busy_r   <= 1'b1;
          fp_x0_r  <= as_fp_x0_i;
          fp_x1_r  <= as_fp_x1_i;
          fp_z0_r  <= as_fp_z0_i;
          fp_z1_r  <= as_fp_z1_i;
          box_i0_r <= as_box_i0_i;
          box_i1_r <= as_box_i1_i;
          box_j1_r <= as_box_j1_i;
          cur_i_r  <= as_box_i0_i;
          cur_j_r  <= as_box_j0_i;
        end
      end else if (accept_c) begin
        groups_emitted_o <= groups_emitted_o + 32'd1;
        verts_covered_o  <= verts_covered_o + 32'($countones(mask_c));
        if (assoc_last_c) begin
          busy_r <= 1'b0;
        end else if (row_last_c) begin
          cur_i_r <= box_i0_r;
          cur_j_r <= cur_j_r + 6'd1;
        end else begin
          cur_i_r <= cur_i_r + 6'd4;
        end
      end
    end
  end

`ifdef FORMAL
  // One z per group: every covered lane of a group carries the same row, so a
  // single z is not an optimisation that could quietly stop holding.
  a_group_within_one_row :
  assert property (@(posedge clk) disable iff (!rst_n)
                   out_valid_o |-> (out_iv_o / 11'(LAT_W)) == 11'(cur_j_r));

  // The box never widens coverage: a masked lane is always inside the
  // footprint, whatever the descriptor claimed.
  a_mask_implies_inside :
  assert property (@(posedge clk) disable iff (!rst_n)
                   (out_valid_o && out_mask_o[0]) |->
                   (out_x0_o >= fp_x0_r && out_x0_o <= fp_x1_r &&
                    out_z_o  >= fp_z0_r && out_z_o  <= fp_z1_r));
`endif

endmodule

`default_nettype wire
