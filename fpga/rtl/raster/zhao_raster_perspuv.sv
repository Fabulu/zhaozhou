// zhao_raster_perspuv.sv — the per-survivor perspective recovery: interpolated
// u_over_w, v_over_w and invw24 in, TMU texture coordinates out.
//
// ENFORCED-BY: tests/raster/raster_perspuv_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHERE THIS SITS
// ---------------------------------------------------------------------------
// Ruling 7 orders the textured path as
//
//     covered fragment -> affine invw24 -> EARLY-Z
//       -> only survivors pay reciprocal + U/V recovery
//       -> only survivors request textures
//
// This block is the "reciprocal + U/V recovery" step, and it is the reason
// early-Z exists: it costs a reciprocal and two products per fragment, and a
// fragment that early-Z rejects pays none of it.
//
// Its inputs are three plane-interpolated attributes, so it sits downstream of
// RASTER.INTERP and RASTER.ATTRDIV.SVC; its outputs are exactly
// `zhao_texture_tmu_pipe`'s `req_u_i` / `req_v_i`, so it sits directly upstream
// of the TMU. Nothing between it and either neighbour reinterprets a format.
//
// ---------------------------------------------------------------------------
// THE SHIFT IS DERIVED, NOT CHOSEN
// ---------------------------------------------------------------------------
// spec/qformats.md §8 states the law as
//
//     u = rescale( (s64)u_over_w * rcp_u24(invw24_interp) )
//
// and does NOT give the rescale's k -- which is the one number a wrong
// implementation would get wrong. It is not free, though: it falls out of the
// three published Q-formats, so it is derived here rather than picked.
//
//     u_over_w   is S 8.24        -> value = raw / 2^24
//     invw24     is U 0.0.24      -> value = d    / 2^24
//     rcp_u24(d) returns {r, k}   -> 1/(d/2^24) = (r / 2^24) * 2^k
//     req_u_i    is S 15.16       -> raw = value * 2^16   (zhao_texture_tmu.sv)
//
//     u_value = (u_over_w / 2^24) * (r / 2^24) * 2^k
//     u_raw   = u_value * 2^16 = u_over_w * r * 2^k / 2^32
//             = rescale_s( u_over_w * r, 32 - k )
//
// with k in [1, 24], so the shift is in [8, 31] and is DATA DEPENDENT -- it
// moves with the magnitude of the depth. That is the part worth stating out
// loud: a block that hard-coded any single shift would be exactly right for one
// depth and progressively wrong everywhere else, which reads as a texture that
// swims with distance rather than as an obvious fault.
//
// ONE ROUNDING. §4 makes `rescale` the only rounding primitive and §3 requires
// the wide expression to round exactly once. The product is formed at full
// width and rounded once on the way out; there is no intermediate truncation.
//
// ---------------------------------------------------------------------------
// SATURATION IS A REAL CASE, NOT A GUARD
// ---------------------------------------------------------------------------
// A fragment near the horizon has a tiny invw24 and a correspondingly enormous
// reciprocal, and the recovered coordinate genuinely does not fit S 15.16. §7's
// rule for `u/v_over_w` is saturate, so the coordinate rails rather than wraps
// -- a texture that stops moving at the horizon, instead of one that tears
// across the whole surface. `sat_o` says it happened, because a rail that
// nobody counts is indistinguishable from a rail that never fires.
//
// ---------------------------------------------------------------------------
// ONE RECIPROCAL, TWO PRODUCTS, SHARED
// ---------------------------------------------------------------------------
// u and v divide by the SAME depth, so the reciprocal is computed once and used
// twice -- the saving is half the reciprocals, on the hottest path in the
// renderer. The two products then walk one shared multiplier rather than two,
// for the same reason `zhao_raster_rcp24` shares one: the knob that should buy
// throughput is the number of UNITS, not the width of one.
`default_nettype none

module zhao_raster_perspuv (
    input var logic clk,
    input var logic rst_n,

    // ---- one surviving fragment ---------------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] u_over_w_i,  // S 8.24
    input  var logic signed [31:0] v_over_w_i,  // S 8.24
    input  var logic        [23:0] invw24_i,    // U 0.0.24, the interpolated depth
    input  var logic        [15:0] tag_i,

    // ---- the TMU's coordinates ----------------------------------------------
    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] u_o,         // S 15.16, zhao_texture_tmu req_u_i
    output var logic signed [31:0] v_o,
    output var logic        [15:0] tag_o,
    output var logic               sat_o,       // the coordinate railed
    output var logic               depth_zero_o,// invw24 == 0: a caller bug

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] fragments_o,
    output var logic [31:0] sat_fragments_o,
    // The reciprocal's own counters, forwarded rather than swallowed: a
    // fragment with a zero depth never reaches it, so `recips` and `fragments`
    // differing IS the count of caller bugs, and `rcp_busy` against the frame
    // is what says whether one unit is enough.
    output var logic [31:0] rcp_recips_o,
    output var logic [31:0] rcp_busy_clocks_o
);

  localparam logic [2:0] P_IDLE = 3'd0;
  localparam logic [2:0] P_RCP  = 3'd1;
  localparam logic [2:0] P_MU   = 3'd2;
  localparam logic [2:0] P_MV   = 3'd3;
  localparam logic [2:0] P_DONE = 3'd4;

  logic [2:0]         st_r;
  logic signed [31:0] uow_r, vow_r;
  logic [15:0]        tag_r;
  logic [23:0]        r_mant_r;
  logic [5:0]         k_r;
  logic               zero_r;
  logic signed [31:0] u_res_r;
  logic               sat_u_r;

  // ---- the reciprocal ------------------------------------------------------
  logic        rcp_vvalid, rcp_vready, rcp_rvalid, rcp_rready, rcp_zero;
  logic [23:0] rcp_r;
  logic [5:0]  rcp_k;
  zhao_raster_rcp24 u_rcp (
      .clk          (clk),
      .rst_n        (rst_n),
      .v_valid_i    (rcp_vvalid),
      .v_ready_o    (rcp_vready),
      .d_i          (invw24_i),
      .r_valid_o    (rcp_rvalid),
      .r_ready_i    (rcp_rready),
      .r_o          (rcp_r),
      .k_o          (rcp_k),
      .d_zero_o     (rcp_zero),
      .recips_o     (rcp_recips_o),
      .busy_clocks_o(rcp_busy_clocks_o)
  );

  assign rcp_vvalid = (st_r == P_IDLE) && v_valid_i && v_ready_o && (invw24_i != 24'd0);
  assign rcp_rready = (st_r == P_RCP);

  // ---- the shared product and its single rounding --------------------------
  // The numerator is S 8.24 and the mantissa is a 24-bit unsigned, so the
  // product needs 32 + 24 = 56 bits and is carried at 64 signed. The shift is
  // 32 - k, formed from the reciprocal's own exponent.
  logic signed [31:0] num_c;
  logic signed [63:0] prod_c;
  logic [5:0]         sh_c;
  logic signed [63:0] resc_c;
  logic               sat_c;
  logic signed [31:0] q_c;
  always_comb begin
    num_c  = (st_r == P_MU) ? uow_r : vow_r;
    prod_c = 64'(num_c) * 64'({40'd0, r_mant_r});
    sh_c   = 6'd32 - k_r;
    // rescale_s(x, k) = (x + 2^(k-1)) >>> k, ONE rounding on the full-width
    // product. k is at least 8 here, so the +2^(k-1) is never a shift by -1.
    resc_c = ($signed(prod_c) + $signed(64'd1 <<< (sh_c - 6'd1))) >>> sh_c;
    sat_c  = (resc_c > 64'sh0000_0000_7FFF_FFFF) || (resc_c < -64'sh0000_0000_8000_0000);
    q_c    = sat_c ? (resc_c[63] ? 32'sh8000_0000 : 32'sh7FFF_FFFF) : resc_c[31:0];
  end

  // THE RECIPROCAL MUST BE ABLE TO TAKE IT IN THE SAME CLOCK. `invw24_i` is
  // only held while the caller is offering, and the reciprocal latches it on
  // accept -- so accepting a fragment the reciprocal cannot take would strand a
  // depth that is already gone. A zero depth never reaches the reciprocal and so
  // does not wait for it.
  assign v_ready_o = (st_r == P_IDLE) && !r_valid_o &&
                     (rcp_vready || (invw24_i == 24'd0));

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r            <= P_IDLE;
      uow_r           <= 32'sd0;
      vow_r           <= 32'sd0;
      tag_r           <= 16'd0;
      r_mant_r        <= 24'd0;
      k_r             <= 6'd0;
      zero_r          <= 1'b0;
      u_res_r         <= 32'sd0;
      sat_u_r         <= 1'b0;
      r_valid_o       <= 1'b0;
      u_o             <= 32'sd0;
      v_o             <= 32'sd0;
      tag_o           <= 16'd0;
      sat_o           <= 1'b0;
      depth_zero_o    <= 1'b0;
      fragments_o     <= 32'd0;
      sat_fragments_o <= 32'd0;
    end else begin
      case (st_r)
        P_IDLE: begin
          if (v_valid_i && v_ready_o) begin
            uow_r  <= u_over_w_i;
            vow_r  <= v_over_w_i;
            tag_r  <= tag_i;
            if (invw24_i == 24'd0) begin
              // A zero depth is a caller bug -- early-Z should never pass a
              // fragment at infinity -- and it is refused rather than divided.
              zero_r <= 1'b1;
              st_r   <= P_DONE;
            end else begin
              zero_r <= 1'b0;
              st_r   <= P_RCP;
            end
          end
        end

        P_RCP: begin
          if (rcp_rvalid) begin
            if (rcp_zero) begin
              // ASSUMPTION, upheld by the accept filter above: a zero depth
              // never reaches the reciprocal, because P_IDLE routes it straight
              // to P_DONE. This branch is what happens if that ever stops being
              // true -- it reports, rather than multiplying by a mantissa that
              // means nothing. It is a fallback for a broken assumption, not a
              // claim that the assumption holds.
              // ENFORCED-BY: tests/raster/raster_perspuv_directed.cpp:main
              zero_r <= 1'b1;
              st_r   <= P_DONE;
            end else begin
              r_mant_r <= rcp_r;
              k_r      <= rcp_k;
              st_r     <= P_MU;
            end
          end
        end

        P_MU: begin
          u_res_r <= q_c;
          sat_u_r <= sat_c;
          st_r    <= P_MV;
        end

        P_MV: begin
          v_o  <= q_c;
          u_o  <= u_res_r;
          sat_o <= sat_u_r || sat_c;
          st_r <= P_DONE;
        end

        P_DONE: begin
          if (zero_r) begin
            u_o   <= 32'sd0;
            v_o   <= 32'sd0;
            sat_o <= 1'b0;
          end
          tag_o           <= tag_r;
          depth_zero_o    <= zero_r;
          r_valid_o       <= 1'b1;
          fragments_o     <= fragments_o + 32'd1;
          if (!zero_r && sat_o) sat_fragments_o <= sat_fragments_o + 32'd1;
          st_r            <= P_IDLE;
        end

        default: st_r <= P_IDLE;
      endcase

      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;
    end
  end

endmodule : zhao_raster_perspuv

`default_nettype wire
