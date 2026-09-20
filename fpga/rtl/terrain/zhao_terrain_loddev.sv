// zhao_terrain_loddev.sv - TERRAIN.LOD's three per-subpatch DEVIATIONS,
// computed from a composed lattice.
//
// Owner ruling R8 (reports/OWNER-RULINGS-20260919-EVENING.md). The report that
// found the gap: reports/TERRAIN-LOD-DEVIATION-20260907.md.
//
// ---------------------------------------------------------------------------
// WHAT IT IS FOR
// ---------------------------------------------------------------------------
// `zhao_terrain_lod` takes `sp_dev1_i/2/3` -- "the largest |fine - coarse|
// height deviation this subpatch would suffer at level L" -- and until now NO
// RTL computed them: the only driver of `sp_dev1_i` in the tree was an LFSR in
// zhao_prod_top, and every test wrote them by hand. The law is executable in
// exactly one place, `zref::terrain::lod_deviation`
// (reference/include/zref/zref_terrain_tess.hpp), written from the two
// ratified functions the tessellator already uses -- `coarse_height` and
// `morph_case`'s case split -- so there is no new arithmetic here: a walk, one
// subtract, one round-half-up halving, one saturating add, one magnitude and a
// running max. This block is that function in hardware, differentially tested
// against it (tests/terrain/terrain_loddev_directed.cpp).
//
// ---------------------------------------------------------------------------
// THE ONE SELECTOR (owner ruling R8)
// ---------------------------------------------------------------------------
// `dev[L]` has two defensible readings that differ ONLY on the subpatch's
// border ring:
//   DEV_INCLUDE_BOUNDARY = 0  the MORPH deviation -- how far the vertices the
//                             tessellator actually moves can move. Border
//                             vertices are shared with a neighbour and are
//                             never morphed (`morph_case` returns 0 there),
//                             so they stay bit-identical to the fine lattice,
//                             the same guarantee T8's nested decimation gives
//                             shared vertices.  Was the default until R22.
//   DEV_INCLUDE_BOUNDARY = 1  the MESH deviation -- how far the coarse mesh
//                             departs from the fine one, border included.
//                             THE DEFAULT SINCE 2026-09-20, owner ruling R22,
//                             provisional until the owner confirms by eye.
// The owner picks by eye from the side-by-side render
// (reports/terrain-lod-readings/); the pick is this one parameter. Its twin in
// software is `zref::terrain::kLodDevIncludeBoundary`, and the directed test
// FAILS if the two defaults disagree, so the choice cannot be made in one
// language and not the other.
//
// ---------------------------------------------------------------------------
// THE WALK, and its cost
// ---------------------------------------------------------------------------
// Sixteen subpatches (8x8 cells each, origins (0/8/16/24, 0/8/16/24)) x levels
// 1..3 x the 9x9 window, in zref's order (vj outer, vi inner). A vertex the
// coarse level carries, a border vertex under reading 0, and a midpoint whose
// pair leaves the lattice are skipped in ONE clock with no read. Every other
// vertex costs FOUR: read the pair's first end, its second end, the vertex
// itself, then compare -- the lattice port is the compose cache's (request,
// data the next cycle), so no read is speculative and no data is buffered
// beyond the three words in flight. At most 16 x 3 x 81 = 3,888 vertex slots
// per surface; measured per run by the directed test (`cycles`). That is a
// LOAD-TIME cost, not a per-frame one -- see the ruling-2 note in the report,
// which this block does not decide.
//
// Conservative SystemVerilog subset. Lint gate: lint_terrain_loddev.
`default_nettype none

module zhao_terrain_loddev #(
    // R8's selector. 0 = morph deviation (border ring excluded), 1 = mesh
    // deviation (border included). See the header.
    // OWNER RULING R22 (2026-09-19 evening), provisional and to be confirmed
    // by eye from reports/terrain-lod-readings/lod_readings_contact.png: the
    // MESH reading.  Its software twin is `zref::terrain::kLodDevIncludeBoundary`
    // and tests/terrain/terrain_loddev_directed.cpp FAILS if only one moves.
    parameter bit DEV_INCLUDE_BOUNDARY = 1'b1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- start: one surface of one composed patch ------------------------------
    input  var logic        start_valid_i,
    output var logic        start_ready_o,
    input  var logic        start_surface_i,   // 0 = top, 1 = underside
    input  var logic [15:0] start_src_id_i,

    // ---- the lattice, TERRAIN.COMPCACHE's read shape ---------------------------
    // Request this cycle, `lat_h_i` is the answer on the next.
    output var logic        lat_req_o,
    output var logic [5:0]  lat_vi_o,
    output var logic [5:0]  lat_vj_o,
    output var logic        lat_surface_o,
    input  var logic signed [31:0] lat_h_i,

    // ---- one record per subpatch, in subpatch order --------------------------
    output var logic        dev_valid_o,
    input  var logic        dev_ready_i,
    output var logic [3:0]  dev_sp_o,          // {oz/8, ox/8}
    output var logic        dev_surface_o,
    output var logic [23:0] dev1_o,
    output var logic [23:0] dev2_o,
    output var logic [23:0] dev3_o,
    output var logic [15:0] dev_src_id_o,
    output var logic        done_o,            // pulse: the 16th record was taken

    // ---- evidence -------------------------------------------------------------
    output var logic [31:0] vertices_measured_o,  // EVENTS: vertices compared
    output var logic [31:0] lattice_reads_o,      // EVENTS
    output var logic [31:0] records_o,            // EVENTS
    output var logic [31:0] clipped_o,            // EVENTS: a |d| wider than 24 bits
    output var logic        busy_o
);

  localparam int unsigned W   = 33;  // lattice vertices per side
  localparam int unsigned SUB = 8;   // cells per subpatch side

  typedef enum logic [2:0] { S_IDLE, S_WALK, S_RB, S_RH, S_CMP, S_EMIT } state_e;
  state_e state;

  logic        surf_q;
  logic [15:0] src_q;
  logic [3:0]  sp_q;
  logic [1:0]  lvl_q;       // 1..3
  logic [3:0]  ui_q, uj_q;  // 0..8 within the window
  logic signed [31:0] a_q, b_q;
  logic [23:0] dev_q [1:3];

  // ---- the vertex under examination, combinational on the cursors --------
  wire [5:0] ox_c = {1'b0, sp_q[1:0], 3'b000};
  wire [5:0] oz_c = {1'b0, sp_q[3:2], 3'b000};
  wire [5:0] vi_c = ox_c + {2'b00, ui_q};
  wire [5:0] vj_c = oz_c + {2'b00, uj_q};
  wire [5:0] s_c  = 6'd1 << lvl_q;          // 2, 4, 8
  wire [5:0] scm  = (s_c << 1) - 6'd1;      // coarse stride - 1: 3, 7, 15
  wire xc_c = ((vi_c & scm) == 6'd0);
  wire zc_c = ((vj_c & scm) == 6'd0);
  wire border_c = (ui_q == 4'd0) || (ui_q == 4'(SUB)) ||
                  (uj_q == 4'd0) || (uj_q == 4'(SUB));

  // The pair, zref's three arms: an x-midpoint on a coarse row, a z-midpoint on
  // a coarse column, else the coarse cell's diagonal (-s,-s)..(+s,+s).
  wire use_x = zc_c;                        // x-midpoint
  wire use_z = !zc_c && xc_c;               // z-midpoint
  wire [6:0] vi_lo = {1'b0, vi_c} - (use_z ? 7'd0 : {1'b0, s_c});
  wire [6:0] vi_hi = {1'b0, vi_c} + (use_z ? 7'd0 : {1'b0, s_c});
  wire [6:0] vj_lo = {1'b0, vj_c} - (use_x ? 7'd0 : {1'b0, s_c});
  wire [6:0] vj_hi = {1'b0, vj_c} + (use_x ? 7'd0 : {1'b0, s_c});
  // Out of the lattice: a borrow sets bit 6 on the low end; the high end is
  // compared against W - 1.
  wire pair_out = vi_lo[6] || vj_lo[6] ||
                  (vi_hi > 7'(W - 1)) || (vj_hi > 7'(W - 1));

  wire skip_c = (xc_c && zc_c) || (border_c && !DEV_INCLUDE_BOUNDARY) || pair_out;

  wire last_lvl = (lvl_q == 2'd3);

  // ---- the compare: coarse_height, then |h - hc|, clipped to the port ------
  //   hc = sat32( a + sat32((b - a + 1) >>> 1) )     zref coarse_height
  //   d  = h - hc                                      in 34 bits
  wire signed [32:0] bma   = {b_q[31], b_q} - {a_q[31], a_q};
  // $signed IS LOAD-BEARING: a concatenation is UNSIGNED in SystemVerilog,
  // so >>> on the bare {bma[32], bma} shifts LOGICALLY and turns every
  // negative half into a huge positive one. The first build did exactly that
  // and saturated every deviation; the differential test caught it.
  wire signed [33:0] half  = ($signed({bma[32], bma}) + 34'sd1) >>> 1;  // rescale_s32(.,1)
  wire signed [31:0] half_s = (half > 34'sd2147483647)  ? 32'sh7FFFFFFF :
                              (half < -34'sd2147483648) ? 32'sh80000000 : half[31:0];
  wire signed [32:0] hc_w  = {a_q[31], a_q} + {half_s[31], half_s};
  wire signed [31:0] hc    = (hc_w > 33'sd2147483647)  ? 32'sh7FFFFFFF :
                             (hc_w < -33'sd2147483648) ? 32'sh80000000 : hc_w[31:0];
  wire signed [32:0] d     = {lat_h_i[31], lat_h_i} - {hc[31], hc};
  wire [32:0] mag          = d[32] ? 33'(-d) : 33'(d);
  wire        clip         = (mag > 33'h0FFFFFF);
  wire [23:0] mag24        = clip ? 24'hFFFFFF : mag[23:0];

  // ---- outputs ------------------------------------------------------------
  assign start_ready_o = (state == S_IDLE);
  assign busy_o        = (state != S_IDLE);

  always_comb begin
    lat_req_o     = 1'b0;
    lat_vi_o      = vi_c;
    lat_vj_o      = vj_c;
    lat_surface_o = surf_q;
    case (state)
      S_WALK: if (!skip_c) begin lat_req_o = 1'b1; lat_vi_o = vi_lo[5:0]; lat_vj_o = vj_lo[5:0]; end
      S_RB:   begin lat_req_o = 1'b1; lat_vi_o = vi_hi[5:0]; lat_vj_o = vj_hi[5:0]; end
      S_RH:   begin lat_req_o = 1'b1; end
      default: ;
    endcase
  end

  assign dev_valid_o   = (state == S_EMIT);
  assign dev_sp_o      = sp_q;
  assign dev_surface_o = surf_q;
  assign dev1_o        = dev_q[1];
  assign dev2_o        = dev_q[2];
  assign dev3_o        = dev_q[3];
  assign dev_src_id_o  = src_q;

  // The next (level, vj, vi) cursor, and whether this vertex was the
  // subpatch's last slot. Combinational so both S_WALK and S_CMP take it.
  wire       u_wrap   = (ui_q == 4'(SUB));
  wire [3:0] nxt_ui   = u_wrap ? 4'd0 : ui_q + 4'd1;
  wire [3:0] nxt_uj   = u_wrap ? ((uj_q == 4'(SUB)) ? 4'd0 : uj_q + 4'd1) : uj_q;
  wire       lvl_wrap = u_wrap && (uj_q == 4'(SUB));
  wire [1:0] nxt_lvl  = (lvl_wrap && !last_lvl) ? lvl_q + 2'd1 : lvl_q;
  wire       sp_fin   = lvl_wrap && last_lvl;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= S_IDLE;
      surf_q <= 1'b0;
      src_q  <= '0;
      sp_q   <= '0;
      lvl_q  <= 2'd1;
      ui_q   <= '0;
      uj_q   <= '0;
      a_q    <= '0;
      b_q    <= '0;
      dev_q[1] <= '0;
      dev_q[2] <= '0;
      dev_q[3] <= '0;
      done_o <= 1'b0;
      vertices_measured_o <= '0;
      lattice_reads_o     <= '0;
      records_o           <= '0;
      clipped_o           <= '0;
    end else begin
      done_o <= 1'b0;
      if (lat_req_o) lattice_reads_o <= lattice_reads_o + 32'd1;

      case (state)
        S_IDLE: if (start_valid_i) begin
          surf_q <= start_surface_i;
          src_q  <= start_src_id_i;
          sp_q   <= '0;
          lvl_q  <= 2'd1;
          ui_q   <= '0;
          uj_q   <= '0;
          dev_q[1] <= '0;
          dev_q[2] <= '0;
          dev_q[3] <= '0;
          state  <= S_WALK;
        end

        S_WALK: begin
          if (skip_c) begin
            ui_q  <= nxt_ui;
            uj_q  <= nxt_uj;
            lvl_q <= nxt_lvl;
            if (sp_fin) state <= S_EMIT;
          end else begin
            state <= S_RB;                    // the pair's low end was requested
          end
        end

        S_RB: begin
          a_q   <= lat_h_i;                   // low end
          state <= S_RH;
        end

        S_RH: begin
          b_q   <= lat_h_i;                   // high end
          state <= S_CMP;
        end

        S_CMP: begin                          // lat_h_i is the vertex itself
          vertices_measured_o <= vertices_measured_o + 32'd1;
          if (clip) clipped_o <= clipped_o + 32'd1;
          if (mag24 > dev_q[lvl_q]) dev_q[lvl_q] <= mag24;
          ui_q  <= nxt_ui;
          uj_q  <= nxt_uj;
          lvl_q <= nxt_lvl;
          state <= sp_fin ? S_EMIT : S_WALK;
        end

        S_EMIT: if (dev_ready_i) begin
          records_o <= records_o + 32'd1;
          dev_q[1] <= '0;
          dev_q[2] <= '0;
          dev_q[3] <= '0;
          lvl_q    <= 2'd1;
          if (sp_q == 4'd15) begin
            done_o <= 1'b1;
            state  <= S_IDLE;
          end else begin
            sp_q  <= sp_q + 4'd1;
            state <= S_WALK;
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

endmodule

`default_nettype wire
