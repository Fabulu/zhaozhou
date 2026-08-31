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
// THE RADIX IS A PARAMETER BECAUSE THE ANSWER IS A MEASUREMENT
// ---------------------------------------------------------------------------
// reports/PER_PIXEL_BUDGET.md puts this block 2 to 5 times short of what a
// frame's attributes actually ask for, even at eight units. Three things could
// close that -- more units, fewer divides, or a shorter divider -- and the
// third is the one nobody could argue about without building it.
//
// So RADIX is 2 or 4 and both elaborate. Radix 4 consumes two quotient bits a
// step instead of one, at the cost of comparing against 1x, 2x and 3x the
// divisor rather than just 1x. The trade is iterations against per-step logic,
// and which side wins is a FIT question, not a reasoning question.
//
// THE ANSWER MUST NOT MOVE. Long division is deterministic: the same numerator
// over the same divisor produces the same quotient whatever the radix, so the
// two builds are required to agree BIT FOR BIT with each other and with the
// oracle. The test runs the identical case list at both radices, which is the
// only way to be sure the faster one is the same divider and not a different
// one that mostly agrees.
//
// ---------------------------------------------------------------------------
// THE REMAINDER IS AN OUTPUT, BECAUSE THE STEPPING PATH NEEDS IT
// ---------------------------------------------------------------------------
// tests/proofs/attribute_step_equivalence.cpp proves the per-pixel divide can be
// replaced by an exact quotient/remainder recurrence -- 10x fewer divides with
// every rendered bit unchanged. That recurrence needs a SEED: the Euclidean
// (q, r) pair of the numerator at one pixel, after which it advances by adds.
//
// A restoring divider already holds the remainder when it finishes; it is
// `rem_r` at D_DONE and it was simply being discarded. Publishing it costs a
// port, not an adder.
//
// AND IT IS ALREADY THE RIGHT REMAINDER. This block divides `2|n| + d` by `2d`,
// so at completion `rem_r` is exactly `M mod D` for the positive branch and
// `M' mod D` for the negative one -- the two branches the recurrence uses. No
// conversion, no sign fixup: the value the stepping wants is the value the
// division produced.
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

module zhao_raster_attrdiv #(
    // 2 or 4. See THE RADIX IS A PARAMETER above; the answer is identical at
    // both, and the test proves that rather than assuming it.
    parameter int unsigned RADIX = 2
) (
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
    // The Euclidean remainder of the division this block just performed:
    // 0 <= rem_o < 2*area. Meaningless when `q_overflow_o` is set, for the same
    // reason `q_o` is. See THE REMAINDER IS AN OUTPUT above.
    output var logic [47:0]        rem_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] divides_o,
    output var logic [31:0] busy_clocks_o
);

  // 33 quotient bits: 32 for the attribute plus one to catch an overflow that
  // would otherwise wrap silently.
  localparam int unsigned QBITS = 33;

  // Bits consumed per step, and the number of quotient POSITIONS the walk
  // covers. Radix 4 needs an even count, so it covers 34 positions -- the extra
  // one is always zero, because `fits_c` below still uses the stricter
  // 33-position test at both radices. Same refusals, same answer, half the
  // steps.
  localparam int unsigned RBITS = (RADIX == 4) ? 2 : 1;
  localparam int unsigned QPOS  = (RADIX == 4) ? 34 : 33;
  localparam int unsigned STEPS = QPOS / RBITS;

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
  // 51 bits so a shifted remainder cannot lose its top bit before the compare.
  // Radix 2 shifts by one and needs 49; radix 4 shifts by two and compares
  // against 3x the divisor, so it needs 51. Both carry 51 -- two bits of an
  // adder is not worth a second width to reason about.
  logic [50:0] rem_r;
  logic [47:0] den_r;
  logic [QPOS-1:0] q_r;

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

  // D_IDLE alone is NOT enough. The D_DONE step returns to D_IDLE in the same
  // clock that raises `r_valid_o`, so a block that only checked the state would
  // accept a second divide while the first answer is still waiting for a
  // consumer -- and 34 clocks later overwrite `q_o` under a raised `r_valid_o`.
  // The standalone test never saw it because it holds `r_ready_i` high; the
  // service below backpressures for real, so the guard is on the result too.
  assign v_ready_o = (st_r == D_IDLE) && !r_valid_o;

  // One step, most significant quotient position first. At radix 2 that is the
  // classic shift-subtract; at radix 4 it is the same walk taking two bits and
  // choosing a digit from {0,1,2,3} against 1x, 2x and 3x the divisor.
  logic [50:0]      rem_shift_c;
  logic [50:0]      d1_c, d2_c, d3_c;
  logic [RBITS-1:0] digit_c;
  logic [50:0]      rem_next_c;
  // Only the low RBITS are consumed -- this is a shift-to-select, and the
  // discard is the whole point of it.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [79:0]      shifted_c;
  /* verilator lint_on UNUSEDSIGNAL */
  always_comb begin
    d1_c      = 51'({3'd0, den_r});
    d2_c      = d1_c << 1;
    d3_c      = d1_c + d2_c;
    // The next RBITS bits of the numerator, most significant first.
    shifted_c = num_r >> (32'(iter_r) * 32'(RBITS));
    rem_shift_c = (rem_r << RBITS) | 51'(shifted_c[RBITS-1:0]);

    if (RADIX == 4) begin
      if (rem_shift_c >= d3_c) begin
        digit_c    = RBITS'(3);
        rem_next_c = rem_shift_c - d3_c;
      end else if (rem_shift_c >= d2_c) begin
        digit_c    = RBITS'(2);
        rem_next_c = rem_shift_c - d2_c;
      end else if (rem_shift_c >= d1_c) begin
        digit_c    = RBITS'(1);
        rem_next_c = rem_shift_c - d1_c;
      end else begin
        digit_c    = RBITS'(0);
        rem_next_c = rem_shift_c;
      end
    end else begin
      if (rem_shift_c >= d1_c) begin
        digit_c    = RBITS'(1);
        rem_next_c = rem_shift_c - d1_c;
      end else begin
        digit_c    = RBITS'(0);
        rem_next_c = rem_shift_c;
      end
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r          <= D_IDLE;
      iter_r        <= 6'd0;
      neg_r         <= 1'b0;
      bad_r         <= 1'b0;
      num_r         <= 80'd0;
      rem_r         <= 51'd0;
      den_r         <= 48'd0;
      q_r           <= {QPOS{1'b0}};
      r_valid_o     <= 1'b0;
      q_o           <= 32'sd0;
      q_overflow_o  <= 1'b0;
      rem_o         <= 48'd0;
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
            // The quotient's bits come out of the window above the positions
            // being walked, so the remainder starts as that window.
            rem_r  <= 51'(num_c >> QPOS);
            q_r    <= {QPOS{1'b0}};
            iter_r <= 6'(STEPS - 1);
            st_r   <= D_RUN;
          end
        end

        D_RUN: begin
          rem_r <= rem_next_c;
          for (int unsigned b = 0; b < RBITS; ++b) begin
            q_r[32'(iter_r) * 32'(RBITS) + b] <= digit_c[b];
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
          // ANY bit above 31 means the quotient did not fit, which is one bit
          // at radix 2 and two at radix 4 -- written as a range so the two
          // builds cannot disagree about what "too big" means.
          q_overflow_o <= bad_r || (|q_r[QPOS-1:32]);
          // rem_r is the remainder of the completed division. Bits above 47
          // cannot be set for a legal area (the divisor is 2*area, 48 bits), and
          // the unused-bit sink below still covers them.
          rem_o        <= bad_r ? 48'd0 : rem_r[47:0];
          r_valid_o    <= 1'b1;
          divides_o    <= divides_o + 32'd1;
          st_r         <= D_IDLE;
        end

        default: st_r <= D_IDLE;
      endcase

      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;
    end
  end

  assign rem_top_unused = ^rem_r[50:48];

endmodule : zhao_raster_attrdiv

`default_nettype wire
