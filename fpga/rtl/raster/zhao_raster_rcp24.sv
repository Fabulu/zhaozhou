// zhao_raster_rcp24.sv — rcp_u24, the raster/depth reciprocal, as a block the
// per-pixel texture path can call.
//
// ENFORCED-BY: tests/raster/raster_rcp24_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THE RASTER NEEDS ITS OWN
// ---------------------------------------------------------------------------
// spec/qformats.md §8 recovers perspective texture coordinates per SURVIVING
// pixel as
//
//     u = rescale( (s64)u_over_w * rcp_u24(invw24_interp) )
//
// so `rcp_u24` sits on the hottest path in the renderer -- once per textured
// fragment that passes early-Z. It already exists in silicon, but only INSIDE
// `zhao_field_v3_normalize`, fused to that engine's four-wide shared multiplier
// bank and its NORMALIZE2/3 group protocol. The raster cannot call that: it
// needs one reciprocal, not a four-point group, and it must not contend for the
// Field's multipliers on a per-pixel path.
//
// So this is the same LAW in a shape the raster can use. Not a reimplementation
// of the table -- it instantiates `zhao_field_rcp24_rom`, the same generated
// T24, so the two paths cannot drift apart in the one place a copy would rot.
//
// ---------------------------------------------------------------------------
// THE LAW, AND WHY IT IS TRANSCRIBED IN 64 BITS
// ---------------------------------------------------------------------------
//     m = d << e until bit 23 is set          e = shifts, k = e + 1
//     idx = (m - 2^23) >> 15
//     x   = T24[idx]
//     twice:  w = (m*x) >> 24
//             x = rescale_u( x * (2^31 - w), 30 )
//     r   = rescale_u(x, 7), pinned to 0xFFFFFF
//
// `rescale_u(v, k)` is round-half-up: `(v + 2^(k-1)) >> k`.
//
// The reference computes `2^31 - w` and the product that follows in UINT64,
// which WRAPS if w ever exceeds 2^31, and then truncates x back to 32 bits. Bit
// exactness means reproducing that, not reproducing what the arithmetic
// "should" be -- so the subtract and the product are carried at 64 bits and
// truncated in the same places. Whether w can actually exceed 2^31 is a
// question about the whole domain, and the test answers it by MEASURING the
// maximum over all 16,777,215 inputs rather than by asserting it here. If that
// measurement says the top never moves, the widths can be narrowed later with
// evidence; narrowing them now would be a guess wearing a width.
//
// ---------------------------------------------------------------------------
// ONE MULTIPLIER, FOUR USES, AND WHAT THAT COSTS
// ---------------------------------------------------------------------------
// Each Newton step needs two products and there are two steps, so the block
// walks ONE shared multiplier four times rather than instantiating four. That
// is the same trade the Field engine made under its DSP ruling, and for the
// same reason: a per-pixel unit that is replicated N times multiplies its
// multiplier count by N, and the count of UNITS is the knob that should buy
// throughput -- not the width of a single unit.
//
// The consequence is an initiation interval, not a latency, and the test
// measures it. If it lands where the AUX texture path landed -- one request per
// six clocks, 277,778 a frame against a 276,480-pixel terrain-primary estimate
// -- then this block has effectively no reserve either, and it needs the same
// UNITS sweep the attribute divide got. The test says which.
//
// ---------------------------------------------------------------------------
// A ZERO IS A CALLER BUG, AND IS REPORTED
// ---------------------------------------------------------------------------
// §6.1 says d == 0 is a caller bug and asserts. An assert is not available in
// silicon, and a normalising loop given zero would shift forever, so a zero
// terminates immediately with `d_zero_o` raised -- the same choice
// RASTER.ATTRDIV makes for a zero area. A wrong texel is a plausible picture; a
// hang is not, and neither is a silent guess.
`default_nettype none

module zhao_raster_rcp24 (
    input var logic clk,
    input var logic rst_n,

    input  var logic        v_valid_i,
    output var logic        v_ready_o,
    input  var logic [23:0] d_i,

    output var logic        r_valid_o,
    input  var logic        r_ready_i,
    // r / 2^24 * 2^k is the reciprocal. r is in [2^23, 2^24].
    output var logic [23:0] r_o,
    output var logic [ 5:0] k_o,
    output var logic        d_zero_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] recips_o,
    output var logic [31:0] busy_clocks_o
);

  localparam logic [2:0] S_IDLE = 3'd0;
  localparam logic [2:0] S_MW   = 3'd1;  // w = (m*x) >> 24
  localparam logic [2:0] S_MX   = 3'd2;  // x = rescale_u(x * (2^31 - w), 30)
  localparam logic [2:0] S_DONE = 3'd3;

  logic [2:0]  st_r;
  logic        step_r;   // which Newton step is running
  logic [23:0] m_r;
  logic [31:0] x_r;
  logic [63:0] w_r;
  logic [5:0]  k_r;
  logic        zero_r;

  // ---- normalise: shift d left until bit 23 is set -------------------------
  // A 24-bit priority scan. d == 0 never reaches here (it is refused at accept),
  // so the loop always terminates.
  logic [4:0]  e_c;
  logic [23:0] m_c;
  always_comb begin
    e_c = 5'd0;
    for (int unsigned b = 0; b < 24; ++b) begin
      if (d_i[23-b] && (e_c == 5'd0) && !d_i[23]) e_c = 5'(b);
    end
    m_c = d_i << e_c;
  end

  // ---- the shared T24 seed table, the same one the Field engine uses -------
  logic [7:0]  idx_c;
  logic [30:0] seed_c;
  assign idx_c = 8'((m_c - 24'h80_0000) >> 15);
  zhao_field_rcp24_rom u_rom (
      .idx_i (idx_c),
      .seed_o(seed_c)
  );

  // ---- the one multiplier, selected by state -------------------------------
  logic [31:0] mul_a_c;
  logic [63:0] mul_b_c, mul_p_c, t_c;
  always_comb begin
    // 2^31 - w, wrapping at 64 bits exactly as the reference's uint64 does.
    t_c     = 64'h0000_0000_8000_0000 - w_r;
    mul_a_c = (st_r == S_MW) ? {8'd0, m_r} : x_r;
    mul_b_c = (st_r == S_MW) ? {32'd0, x_r} : t_c;
    // Truncating at 64 bits is the point, not an accident: the reference's
    // product is a uint64 and its overflow is part of the law.
    mul_p_c = 64'(mul_a_c) * mul_b_c;
  end

  // rescale_u(v, k) = (v + 2^(k-1)) >> k, round-half-up.
  // The reference stores the Newton iterate back into a uint32, so the top of
  // the rescaled value is DISCARDED by law rather than by width choice. Saying
  // that here is the difference between reproducing the arithmetic and
  // accidentally matching it.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [63:0] resc30_c, resc7_c;
  /* verilator lint_on UNUSEDSIGNAL */
  always_comb begin
    resc30_c = (mul_p_c + 64'h0000_0000_2000_0000) >> 30;
    resc7_c  = ({32'd0, x_r} + 64'd64) >> 7;
  end

  assign v_ready_o = (st_r == S_IDLE) && !r_valid_o;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r          <= S_IDLE;
      step_r        <= 1'b0;
      m_r           <= 24'd0;
      x_r           <= 32'd0;
      w_r           <= 64'd0;
      k_r           <= 6'd0;
      zero_r        <= 1'b0;
      r_valid_o     <= 1'b0;
      r_o           <= 24'd0;
      k_o           <= 6'd0;
      d_zero_o      <= 1'b0;
      recips_o      <= 32'd0;
      busy_clocks_o <= 32'd0;
    end else begin
      if (st_r != S_IDLE) busy_clocks_o <= busy_clocks_o + 32'd1;

      case (st_r)
        S_IDLE: begin
          if (v_valid_i && v_ready_o) begin
            if (d_i == 24'd0) begin
              // Refused, not guessed, and not looped on.
              zero_r <= 1'b1;
              m_r    <= 24'd0;
              x_r    <= 32'd0;
              k_r    <= 6'd0;
              st_r   <= S_DONE;
            end else begin
              zero_r <= 1'b0;
              m_r    <= m_c;
              x_r    <= {1'b0, seed_c};
              k_r    <= 6'({1'b0, e_c}) + 6'd1;
              step_r <= 1'b0;
              st_r   <= S_MW;
            end
          end
        end

        S_MW: begin
          w_r  <= mul_p_c >> 24;
          st_r <= S_MX;
        end

        S_MX: begin
          x_r <= resc30_c[31:0];
          if (step_r) begin
            st_r <= S_DONE;
          end else begin
            step_r <= 1'b1;
            st_r   <= S_MW;
          end
        end

        S_DONE: begin
          // The pin: only m == 2^23 reaches 2^24, and it is law rather than
          // overflow, so it is not counted anywhere.
          r_o       <= zero_r ? 24'd0
                     : ((resc7_c > 64'h00FF_FFFF) ? 24'hFF_FFFF : resc7_c[23:0]);
          k_o       <= k_r;
          d_zero_o  <= zero_r;
          r_valid_o <= 1'b1;
          recips_o  <= recips_o + 32'd1;
          st_r      <= S_IDLE;
        end

        default: st_r <= S_IDLE;
      endcase

      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;
    end
  end

endmodule : zhao_raster_rcp24

`default_nettype wire
