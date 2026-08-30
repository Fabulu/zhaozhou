// zhao_raster_attrdiv.sv — the attribute divide, exactly as the oracle rounds.
//
// ENFORCED-BY: tests/raster/raster_attrdiv_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK IS THE ONE THAT MATTERS
// ---------------------------------------------------------------------------
// `reference/src/zrender/rast.cpp` defines every interpolated attribute as
//
//     attr = round_half_up( (w0*va + w1*vb + w2*vc) / area )
//
// GEOM.ATTRSETUP already emits the numerator's plane, and stepping that plane
// is integer adds -- free and exact, proved in
// tests/proofs/attribute_plane_equivalence.cpp. **This divide is the entire
// remaining cost of attribute interpolation**, and it is per attribute per
// pixel: seven of them for a textured Gouraud triangle with everything live.
//
// So the throughput of the whole textured path is set here, which is why it is
// a block with a measured initiation interval rather than an expression buried
// in a datapath. The Field engine spent six sweep rounds learning that the wall
// is the resource that REFUSES; this one is going to refuse, and it should be
// able to say so.
//
// ---------------------------------------------------------------------------
// THE ROUNDING IS THE LAW, NOT A CHOICE
// ---------------------------------------------------------------------------
// rast.cpp rounds half-up ON THE QUOTIENT:
//
//     n >= 0 :   (2n + d) / (2d)
//     n <  0 :  -((-2n + d) / (2d))
//
// which is symmetric about zero -- NOT floor-toward-negative-infinity, and not
// round-half-to-even. Getting that wrong is a one-LSB error on about half of
// all pixels, invisible in anything but an exact comparison, and it would move
// every golden capture CRC. So the block computes |n|, divides, and applies the
// sign, which is the same three steps in the same order as the reference.
//
// ---------------------------------------------------------------------------
// WIDTHS, AND WHY THE QUOTIENT IS SMALL
// ---------------------------------------------------------------------------
// The numerator reaches 2^78 and the area 2^46, but the QUOTIENT cannot exceed
// the attribute range: it is a convex combination of va, vb and vc, each S 8.24.
// So 33 quotient bits are sufficient with room to spare, and the divider runs 33
// iterations rather than the 80 a naive long division of an 80-bit numerator
// would take. `q_overflow_o` says out loud if that assumption is ever violated
// instead of silently truncating.
`default_nettype none

module zhao_raster_attrdiv (
    input var logic clk,
    input var logic rst_n,

    // ---- one numerator, one area -------------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    // The stepped numerator, signed. GEOM.ATTRSETUP's plane evaluated at this
    // pixel; the stepping itself is the caller's adds.
    input  var logic signed [95:0] num_i,
    // The triangle's doubled area, always > 0 after GEOM.CLIP's winding
    // normalisation. Zero is a degenerate triangle, which CLIP rejects before
    // this block can see it -- but a zero here would divide forever, so it is
    // refused rather than assumed.
    input  var logic        [46:0] area_i,

    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] q_o,
    // The quotient did not fit 32 bits, or the area was zero. Either means the
    // caller broke a precondition, and a wrong attribute is worse than a stall.
    output var logic               q_overflow_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] divides_o,
    output var logic [31:0] busy_clocks_o
);

  // 33 quotient bits: 32 for the attribute plus one to catch an overflow that
  // would otherwise wrap silently.
  localparam int unsigned QBITS = 33;

  localparam logic [1:0] D_IDLE = 2'd0;
  localparam logic [1:0] D_RUN  = 2'd1;
  localparam logic [1:0] D_DONE = 2'd2;

  logic [1:0]  st_r;
  logic [5:0]  iter_r;
  logic        neg_r, bad_r;

  // num = 2|n| + d, den = 2d. The doubling is what makes the truncating
  // division round half-up, and it is done ONCE here rather than being
  // rediscovered at each use.
  logic [79:0] num_r;    // the bits still to be shifted in
  logic        rem_top_unused;
  // 49 bits so a shifted remainder cannot lose its top bit before the compare;
  // bit 48 is only ever live inside `rem_shift_c` and is sunk below.
  logic [48:0] rem_r;
  logic [47:0] den_r;
  logic [QBITS-1:0] q_r;

  logic signed [95:0] n_abs_c;
  logic [79:0]        num_c;
  logic [47:0]        den_c;
  logic               fits_c;
  always_comb begin
    n_abs_c = num_i[95] ? -num_i : num_i;
    // 2|n| + d, then the low QBITS are shifted out one at a time below.
    num_c   = 80'(unsigned'(n_abs_c) << 1) + 80'({33'd0, area_i});
    den_c   = 48'({1'b0, area_i}) << 1;
    // The top of the numerator must already be smaller than the divisor, or the
    // quotient needs more than QBITS bits.
    fits_c  = (area_i != 47'd0) && ((num_c >> QBITS) < 80'(den_c));
  end

  assign v_ready_o = (st_r == D_IDLE);

  // One shift-subtract step, most significant quotient bit first.
  logic [48:0] rem_shift_c;
  logic        sub_ok_c;
  always_comb begin
    rem_shift_c = {rem_r[47:0], num_r[{1'b0, iter_r}]};
    sub_ok_c    = (rem_shift_c >= 49'({1'b0, den_r}));
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r          <= D_IDLE;
      iter_r        <= 6'd0;
      neg_r         <= 1'b0;
      bad_r         <= 1'b0;
      num_r         <= 80'd0;
      rem_r         <= 49'd0;
      den_r         <= 48'd0;
      q_r           <= {QBITS{1'b0}};
      r_valid_o     <= 1'b0;
      q_o           <= 32'sd0;
      q_overflow_o  <= 1'b0;
      divides_o     <= 32'd0;
      busy_clocks_o <= 32'd0;
    end else begin
      if (st_r != D_IDLE) busy_clocks_o <= busy_clocks_o + 32'd1;

      case (st_r)
        D_IDLE: begin
          if (v_valid_i && v_ready_o) begin
            neg_r  <= num_i[95];
            bad_r  <= !fits_c;
            num_r  <= num_c;
            den_r  <= den_c;
            // The quotient's bits come out of the window above QBITS, so the
            // remainder starts as that window.
            rem_r  <= 49'(num_c >> QBITS);
            q_r    <= {QBITS{1'b0}};
            iter_r <= 6'(QBITS - 1);
            st_r   <= D_RUN;
          end
        end

        D_RUN: begin
          if (sub_ok_c) begin
            rem_r        <= rem_shift_c - 49'({1'b0, den_r});
            q_r[iter_r]  <= 1'b1;
          end else begin
            rem_r <= rem_shift_c;
          end
          if (iter_r == 6'd0) st_r <= D_DONE;
          else iter_r <= iter_r - 6'd1;
        end

        D_DONE: begin
          // The sign is applied to the magnitude, which is the reference's own
          // order: |n| divided, then negated.
          q_o          <= bad_r ? 32'sd0
                        : (neg_r ? 32'(-$signed({1'b0, q_r[31:0]}))
                                 : 32'($signed({1'b0, q_r[31:0]})));
          q_overflow_o <= bad_r || q_r[QBITS-1];
          r_valid_o    <= 1'b1;
          divides_o    <= divides_o + 32'd1;
          st_r         <= D_IDLE;
        end

        default: st_r <= D_IDLE;
      endcase

      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;
    end
  end

  assign rem_top_unused = rem_r[48];

endmodule : zhao_raster_attrdiv

`default_nettype wire
