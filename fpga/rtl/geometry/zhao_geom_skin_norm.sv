// zhao_geom_skin_norm.sv — the blended, renormalised world normal, once per vertex.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS AND WHY IT IS NOT PART OF GEOM.SKIN
// ---------------------------------------------------------------------------
// `design/contracts/GEOM.LIGHT.md` names `SKIN.NORM` as a prerequisite for the
// creature path and explicitly not itself. `tools/design/compose_order.py` then
// reported the consequence: `GEOM.LIGHT takes world_normal, no upstream emits
// it` -- the last unexplained input in the whole geometry subsystem after the
// 2026-09-04 graph pass.
//
// It cannot be a widening of `GEOM.SKIN`. `reports/CREATURESANDLIGHTS` states
// that block fits at 89.65 MHz with 9 DSPs and one weighted vertex per twelve
// clocks, and that **"nothing more may be bolted onto its output"**. This law
// needs the normal transformed by both bones and a square root, so it is its
// own block or it is nothing.
//
// ---------------------------------------------------------------------------
// THE LAW, AND WHY IT IS THIS SHAPE
// ---------------------------------------------------------------------------
// `spec/creature_rules.md` §2.x.1, repaired 2026-09-04 after the spec was found
// carrying a ratified law its own oracle never implemented. The reference is
// `zref::creature::skin_world_normal`:
//
//     n[row] = w0 * (A_row . N) + w1 * (B_row . N)    -- blend the VECTOR
//     range-reduce until max|n| < 2^30                -- direction preserved
//     mag    = isqrt_u64(n.n)                         -- ONE renormalisation
//
// Blending the two bones' already-clamped Lambert responses instead -- the
// struck law -- makes light follow influence weights rather than the deformed
// surface, and produced visible bright patches at mixed-weight joints.
//
// ---------------------------------------------------------------------------
// ONCE PER VERTEX, NOT ONCE PER LIGHT
// ---------------------------------------------------------------------------
// The owner is explicit: "The current reference repeatedly calls
// skin_normal_lambert for key, fill and point light ... The hardware should not
// reproduce that structure." Everything here is light-independent, so it runs
// once and `GEOM.LIGHT` then spends one dot and one divide per light.
//
// That equivalence is PROVED rather than assumed --
// `tests/geometry/skin_norm_split_directed.cpp`, one normal reused across three
// lights against three independent reference calls, 20,000 vertices.
//
// THE OUTPUT IS `{direction, magnitude}` AND NOT A UNIT VECTOR. Normalising
// here would round twice, once into the unit vector and again in the Lambert
// quotient, and the law has exactly ONE rounding.
//
// The square root is `zhao_field_isqrt`, driven as a service. It already cites
// `zref::isqrt_u64` and returns the true floor; a second implementation would
// be a second law.
//
// ENFORCED-BY: tests/geometry/skin_norm_rtl_directed.cpp:main
`default_nettype none

module zhao_geom_skin_norm #(
    parameter int unsigned SRCW = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one vertex ---------------------------------------------------------
    input  var logic              v_valid_i,
    output var logic              v_ready_o,
    input  var logic signed [7:0] v_nx_i,          // packed bind-space normal
    input  var logic signed [7:0] v_ny_i,
    input  var logic signed [7:0] v_nz_i,
    input  var logic [6:0]        v_w0_i,          // 0..64, w1 = 64 - w0
    input  var logic [SRCW-1:0]   v_src_id_i,
    // The two bones, already resolved from the palette by the caller. This
    // block does not own the palette -- GEOM.POSE does, and a second lookup
    // here would be a second cache.
    input  var logic signed [31:0] a_i [12],
    input  var logic signed [31:0] b_i [12],

    // ---- the isqrt service (zhao_field_isqrt) -------------------------------
    output var logic              sq_valid_o,
    input  var logic              sq_ready_i,
    output var logic [63:0]       sq_n_o,
    input  var logic              sq_rvalid_i,
    output var logic              sq_rready_o,
    input  var logic [63:0]       sq_r_i,

    // ---- the world normal, as a pair ----------------------------------------
    output var logic              n_valid_o,
    input  var logic              n_ready_i,
    output var logic signed [63:0] n_x_o,
    output var logic signed [63:0] n_y_o,
    output var logic signed [63:0] n_z_o,
    output var logic [63:0]       n_mag_o,
    output var logic              n_degenerate_o,  // no direction; light it black
    output var logic [SRCW-1:0]   n_src_id_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0]       vertices_o,
    output var logic [31:0]       degenerate_o,
    output var logic [31:0]       reduced_o        // vertices that needed a shift
);

  typedef enum logic [2:0] {
    S_IDLE, S_MUL, S_REDUCE, S_SQ, S_WAIT, S_EMIT
  } state_e;
  state_e st_q;

  logic signed [7:0]  nx_q, ny_q, nz_q;
  logic [6:0]         w0_q;
  logic [SRCW-1:0]    src_q;
  logic signed [31:0] a_q [12];
  logic signed [31:0] b_q [12];

  logic signed [63:0] n_q [3];
  logic [3:0]         mi_q;          // 0..5: two dots per lane, one lane at a time
  logic [63:0]        mag_q;
  logic               degen_q;

  assign v_ready_o      = (st_q == S_IDLE);
  assign sq_valid_o     = (st_q == S_SQ);
  assign sq_rready_o    = (st_q == S_WAIT);
  assign n_valid_o      = (st_q == S_EMIT);
  assign n_x_o          = n_q[0];
  assign n_y_o          = n_q[1];
  assign n_z_o          = n_q[2];
  assign n_mag_o        = mag_q;
  assign n_degenerate_o = degen_q;
  assign n_src_id_o     = src_q;

  // ---- the row products, one lane per cycle --------------------------------
  // Six 32x8 products per lane pair, sequenced three lanes deep. This block
  // runs at vertex rate behind GEOM.SKIN's one-per-twelve-clocks, so three
  // clocks of transform is free and a parallel form would spend eighteen
  // multipliers to beat a block that is not the bottleneck.
  logic [1:0] lane_c;
  assign lane_c = 2'(mi_q[1:0]);

  logic signed [63:0] na_c, nb_c, blend_c;
  always_comb begin
    // The oracle accumulates in int64 and this matches it deliberately: if the
    // reference can overflow on a hostile input the RTL must overflow
    // IDENTICALLY, or the two part company exactly where nobody looks.
    na_c = 64'(a_q[int'(lane_c) * 4 + 0]) * 64'(nx_q)
         + 64'(a_q[int'(lane_c) * 4 + 1]) * 64'(ny_q)
         + 64'(a_q[int'(lane_c) * 4 + 2]) * 64'(nz_q);
    nb_c = 64'(b_q[int'(lane_c) * 4 + 0]) * 64'(nx_q)
         + 64'(b_q[int'(lane_c) * 4 + 1]) * 64'(ny_q)
         + 64'(b_q[int'(lane_c) * 4 + 2]) * 64'(nz_q);
    // The common 1/64 and the uniform bulk factor cancel in the normalisation,
    // so the weighted direction is kept whole and no pre-normalise rounding is
    // introduced.
    blend_c = 64'(w0_q) * na_c + 64'(7'd64 - w0_q) * nb_c;
  end

  // ---- the range reduction --------------------------------------------------
  // Applied to EVERY lane equally, so it changes the magnitude guard and not
  // the direction. It exists to keep the squares inside 64 bits.
  logic [63:0] absmax_c;
  logic        need_shift_c;
  always_comb begin
    automatic logic [63:0] a0 = n_q[0][63] ? 64'(-n_q[0]) : 64'(n_q[0]);
    automatic logic [63:0] a1 = n_q[1][63] ? 64'(-n_q[1]) : 64'(n_q[1]);
    automatic logic [63:0] a2 = n_q[2][63] ? 64'(-n_q[2]) : 64'(n_q[2]);
    absmax_c     = (a0 > a1) ? ((a0 > a2) ? a0 : a2) : ((a1 > a2) ? a1 : a2);
    need_shift_c = (absmax_c >= (64'd1 << 30));
  end

  assign sq_n_o = 64'(n_q[0] * n_q[0]) + 64'(n_q[1] * n_q[1]) + 64'(n_q[2] * n_q[2]);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q         <= S_IDLE;
      mi_q         <= '0;
      degen_q      <= 1'b0;
      mag_q        <= '0;
      vertices_o   <= '0;
      degenerate_o <= '0;
      reduced_o    <= '0;
    end else begin
      case (st_q)
        S_IDLE: if (v_valid_i) begin
          nx_q  <= v_nx_i;
          ny_q  <= v_ny_i;
          nz_q  <= v_nz_i;
          w0_q  <= v_w0_i;
          src_q <= v_src_id_i;
          for (int i = 0; i < 12; i++) begin
            a_q[i] <= a_i[i];
            b_q[i] <= b_i[i];
          end
          vertices_o <= vertices_o + 32'd1;
          mi_q       <= '0;
          degen_q    <= 1'b0;
          // A zero packed normal has no direction at all, and the oracle
          // refuses it before touching the palette.
          if (v_nx_i == 8'sd0 && v_ny_i == 8'sd0 && v_nz_i == 8'sd0) begin
            degen_q      <= 1'b1;
            mag_q        <= '0;
            degenerate_o <= degenerate_o + 32'd1;
            st_q         <= S_EMIT;
          end else begin
            st_q <= S_MUL;
          end
        end

        S_MUL: begin
          n_q[lane_c] <= blend_c;
          if (mi_q == 4'd2) st_q <= S_REDUCE;
          mi_q <= mi_q + 4'd1;
        end

        S_REDUCE: begin
          if (need_shift_c) begin
            n_q[0]    <= n_q[0] >>> 1;
            n_q[1]    <= n_q[1] >>> 1;
            n_q[2]    <= n_q[2] >>> 1;
            reduced_o <= reduced_o + 32'd1;
          end else if (sq_n_o == 64'd0) begin
            // A blend that cancels to zero length. Degenerate for the same
            // reason a zero packed normal is, and counted the same way.
            degen_q      <= 1'b1;
            mag_q        <= '0;
            degenerate_o <= degenerate_o + 32'd1;
            st_q         <= S_EMIT;
          end else begin
            st_q <= S_SQ;
          end
        end

        S_SQ:   if (sq_ready_i)   st_q <= S_WAIT;
        S_WAIT: if (sq_rvalid_i) begin
          mag_q <= sq_r_i;
          st_q  <= S_EMIT;
        end

        S_EMIT: if (n_ready_i) st_q <= S_IDLE;

        default: st_q <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_geom_skin_norm

`default_nettype wire
