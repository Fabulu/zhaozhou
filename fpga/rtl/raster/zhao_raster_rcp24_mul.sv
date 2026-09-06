// zhao_raster_rcp24_mul.sv — the EXACT 32x32 product with a signed-wrap
// high-word correction, as six registered stages.
//
// ENFORCED-BY: tests/raster/raster_rcp24_v3_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THIS REPLACES, AND WHY IT IS A SEPARATE MODULE
// ---------------------------------------------------------------------------
// `zhao_raster_rcp24` and `zhao_raster_rcp24_svc` both compute
//
//     t64 = (2^31 - w) mod 2^64            w carried as 64 bits
//     P64 = (x * t64)  mod 2^64            a 32-by-64 multiply
//
// which is one enormous multiplier serving a product whose operands are, in
// truth, 32 bits wide. TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt S10.5 gives
// the exact replacement:
//
//     b32  = (2^31 - w) mod 2^32
//     neg  = (w > 2^31)
//     low  = (x * b32)[31:0]
//     high = (x * b32)[63:32] - (neg ? x : 0)   mod 2^32
//     P64  = {high, low}
//
// "There is no lower-half borrow because the subtracted term has 32 zero low
// bits." The identity is UNCONDITIONAL over every 32-bit x and every 32-bit w;
// it does not assume the correction is positive and needs no new reciprocal.
//
// THIS IS ITS OWN MODULE BECAUSE THE NEGATIVE CASE IS NOT REACHABLE FROM THE
// RECIPROCAL'S OWN INPUTS. Measured here, 2026-09-06, by walking all 16,777,215
// nonzero denominators through both Newton steps with the committed T24 table:
//
//     max w over the whole domain = 0x401F_EF88 = 1,075,834,760
//     2^31                        = 0x8000_0000 = 2,147,483,648
//     phases with w > 2^31        = 0
//
// So a paired test driven by DENOMINATORS can never exercise the correction —
// it would pass with the correction deleted. The arithmetic has to be reachable
// on its own port to be testable at all, which is what this module is for. The
// tile instantiates it; the differential drives it directly across the same
// (x, w) case families `tools/rtl/architecture_numeric_checks.py` uses,
// negative correction included.
//
// ---------------------------------------------------------------------------
// FOUR MULTIPLIER SITES. COUNT THEM.
// ---------------------------------------------------------------------------
// CLAUDE.md records a fit that reported 8 DSP against a rule of 2, where the
// comfortable reading was that `multstyle = "logic"` had been ignored and the
// truth was fourteen multiply sites hidden inside two seven-arm case
// statements. So: this file contains EXACTLY FOUR `*` operators, all of them
// 16-by-16 unsigned, all in one always_ff, none inside a case statement. That
// is the whole multiplier content of the RCP tile. If a fit reports more DSP
// than four 16x16 products can occupy, the question is the tool's packing and
// not a hidden fifth site.
//
// S10.7's recombination is transcribed rather than rederived:
//
//     cross   = p01 + p10                       33 bits
//     low_sum = p00 + ((cross & 0xffff) << 16)  33 bits
//     high    = p11 + (cross >> 16) + carry(low_sum)
//
// and S10.7 forbids serialising the four partial products over four clocks,
// because that would cost the one-micro-job-per-clock launch rate the whole
// scheduled organisation exists to buy.
//
// ---------------------------------------------------------------------------
// THE EXTRACTIONS ARE PART OF THE LAW
// ---------------------------------------------------------------------------
// S10.6: only bits 61..30 and the rounding carry out of bit 29 survive the
// reference's `uint32` conversion, so
//
//     x_next = P64[61:30] + zero_extend(P64[29])   mod 2^32
//
// is EXACT, not an approximation of `(P64 + 2^29) >> 30`. Adding exactly 2^29
// can only carry into bit 30 when bit 29 is already set, and bits 62/63 are
// discarded by the uint32 result either way.
//
// S10.4: m <= 2^24-1 and x <= 2^32-1 give P = m*x < 2^56, so w = P >> 24 < 2^32
// unconditionally. Hence `w_next = P64[55:24]` with no width proof owed to a
// simulation sample.
//
// Both are emitted every phase. The caller takes the one its phase means; the
// unused one costs wiring, not a second datapath.
`default_nettype none

module zhao_raster_rcp24_mul #(
    parameter int unsigned TAGW = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- launch: one micro-job per clock, never stalled ----------------------
    // There is no ready, and this module cannot enforce that. It is an
    // ASSUMPTION ON THE CALLER, written as one so it is not mistaken for a
    // property of this file: the caller must not launch a job it is unable to
    // retire six clocks later.
    //
    // `zhao_raster_rcp24_v3` upholds it. It reserves an execution context from
    // its free queue before admitting anything (`v_ready_o = !free_empty`),
    // every queue it owns is sized to the context count, and
    // `zhao_raster_ticketq` latches `err_o` on any overflow or underflow, which
    // the tile exports as `qerr_o` and its differential asserts stays low on
    // every pass, including the 2^24 sweep.
    input var logic            valid_i,
    input var logic [31:0]     a_i,
    input var logic [31:0]     b_i,
    // The high-word correction operand, ALREADY SELECTED by the caller:
    // x when the phase is an MX with w > 2^31, zero otherwise. Passing the
    // value rather than a flag keeps the negative decision in one place.
    input var logic [31:0]     corr_i,
    input var logic [TAGW-1:0] tag_i,

    // ---- result, six clocks later, in issue order ----------------------------
    output var logic            valid_o,
    output var logic [TAGW-1:0] tag_o,
    // The corrected P64, exposed so the differential can compare the whole
    // 64-bit modular product and not only the two extractions.
    output var logic [31:0]     p_hi_o,
    output var logic [31:0]     p_lo_o,
    output var logic [31:0]     w_next_o,
    output var logic [31:0]     x_next_o
);

  // ---- O: operands, registered ---------------------------------------------
  logic            o_v_q;
  logic [31:0]     o_a_q, o_b_q, o_corr_q;
  logic [TAGW-1:0] o_tag_q;

  // ---- M: the four partial products ----------------------------------------
  logic            m_v_q;
  logic [31:0]     m_p00_q, m_p01_q, m_p10_q, m_p11_q, m_corr_q;
  logic [TAGW-1:0] m_tag_q;

  // ---- X: the cross sum ----------------------------------------------------
  logic            x_v_q;
  logic [32:0]     x_cross_q;
  logic [31:0]     x_p00_q, x_p11_q, x_corr_q;
  logic [TAGW-1:0] x_tag_q;

  // ---- L: low half and its carry -------------------------------------------
  logic            l_v_q;
  logic [31:0]     l_low_q, l_p11_q, l_corr_q;
  logic            l_carry_q;
  logic [16:0]     l_crosshi_q;
  logic [TAGW-1:0] l_tag_q;

  // ---- H: high half, before correction -------------------------------------
  logic            h_v_q;
  logic [31:0]     h_high_q, h_low_q, h_corr_q;
  logic [TAGW-1:0] h_tag_q;

  // ---- C: high half, corrected ---------------------------------------------
  logic            c_v_q;
  logic [31:0]     c_hi_q, c_low_q;
  logic [TAGW-1:0] c_tag_q;

  // ---- combinational recombination -----------------------------------------
  // Declared at 33 bits so the carry is a bit this file NAMES rather than a
  // width that happened to be wide enough.
  logic [32:0] cross_c, low_sum_c, high_sum_c;
  assign cross_c   = {1'b0, m_p01_q} + {1'b0, m_p10_q};
  assign low_sum_c = {1'b0, x_p00_q} + {1'b0, x_cross_q[15:0], 16'd0};
  assign high_sum_c = {1'b0, l_p11_q} + {16'd0, l_crosshi_q} + {32'd0, l_carry_q};

  // The high half of an exact 32x32 product is < 2^32 by construction, so
  // high_sum_c[32] is dropped where the H stage registers high_sum_c[31:0].
  // ENFORCED-BY: fpga/rtl/raster/zhao_raster_rcp24_mul.sv:a_high_carry_never_set
  //
  // That is a runtime refusal at the bottom of this file and not a sentence,
  // because this particular bit going wrong is invisible: the product would be
  // incorrect ONLY in its top bit, and both extractions below it would still
  // produce entirely plausible numbers.
  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_high_carry;
  assign unused_high_carry = high_sum_c[32];
  /* verilator lint_on UNUSEDSIGNAL */

  // The signed-wrap correction. Both operands are 32 bits and the subtract
  // wraps at 32 bits, which IS the identity: t64's high word is 2^32-1 exactly
  // when neg, and (x * (2^32-1)) mod 2^32 == (-x) mod 2^32.
  logic [31:0] hi_corrected_c;
  assign hi_corrected_c = h_high_q - h_corr_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      o_v_q    <= 1'b0;
      m_v_q    <= 1'b0;
      x_v_q    <= 1'b0;
      l_v_q    <= 1'b0;
      h_v_q    <= 1'b0;
      c_v_q    <= 1'b0;
      valid_o  <= 1'b0;
    end else begin
      // O -- choose nothing; the caller already chose. Register the operands.
      o_v_q    <= valid_i;
      o_a_q    <= a_i;
      o_b_q    <= b_i;
      o_corr_q <= corr_i;
      o_tag_q  <= tag_i;

      // M -- the four 16x16 partial products, launched in parallel.
      // THESE FOUR LINES ARE EVERY MULTIPLIER SITE IN THE RCP TILE.
      m_v_q    <= o_v_q;
      m_p00_q  <= 32'(o_a_q[15:0])  * 32'(o_b_q[15:0]);
      m_p01_q  <= 32'(o_a_q[15:0])  * 32'(o_b_q[31:16]);
      m_p10_q  <= 32'(o_a_q[31:16]) * 32'(o_b_q[15:0]);
      m_p11_q  <= 32'(o_a_q[31:16]) * 32'(o_b_q[31:16]);
      m_corr_q <= o_corr_q;
      m_tag_q  <= o_tag_q;

      // X -- cross sum.
      x_v_q     <= m_v_q;
      x_cross_q <= cross_c;
      x_p00_q   <= m_p00_q;
      x_p11_q   <= m_p11_q;
      x_corr_q  <= m_corr_q;
      x_tag_q   <= m_tag_q;

      // L -- low half and the carry it hands upward.
      l_v_q       <= x_v_q;
      l_low_q     <= low_sum_c[31:0];
      l_carry_q   <= low_sum_c[32];
      l_p11_q     <= x_p11_q;
      l_crosshi_q <= x_cross_q[32:16];
      l_corr_q    <= x_corr_q;
      l_tag_q     <= x_tag_q;

      // H -- high half, uncorrected.
      h_v_q    <= l_v_q;
      h_high_q <= high_sum_c[31:0];
      h_low_q  <= l_low_q;
      h_corr_q <= l_corr_q;
      h_tag_q  <= l_tag_q;

      // C -- S10.5's "high-half subtract is its own registered 32-bit stage".
      c_v_q   <= h_v_q;
      c_hi_q  <= hi_corrected_c;
      c_low_q <= h_low_q;
      c_tag_q <= h_tag_q;

      // E -- the two extractions, both exact, both by bit selection.
      valid_o  <= c_v_q;
      tag_o    <= c_tag_q;
      p_hi_o   <= c_hi_q;
      p_lo_o   <= c_low_q;
      w_next_o <= {c_hi_q[23:0], c_low_q[31:24]};
      x_next_o <= {c_hi_q[29:0], c_low_q[31:30]} + {31'd0, c_low_q[29]};
    end
  end

`ifndef SYNTHESIS
  // The 33rd bit of the high sum, watched on every launched job.
  //
  // S10.7's recombination is exact for unsigned 32x32, so the high word is
  // floor(a*b / 2^32) < 2^32 and this bit is always zero. The differential
  // drives 500,256 direct (a, b) pairs and 16,777,215 reciprocals through here,
  // and this is what makes those runs check the carry rather than only the
  // values that survive it.
  //
  // Gated on `l_v_q` ALONE and not on `rst_n`. `rst_n` is this module's
  // ASYNCHRONOUS reset, and reading it synchronously here made Verilator flag
  // SYNCASYNCNET -- correctly, because that is a real reset-domain smell even
  // in a checker. `l_v_q` is itself cleared by that async reset, so it is low
  // for the whole of reset and the guard is the same guard without the smell.
  always_ff @(posedge clk) begin
    if (l_v_q) begin
      a_high_carry_never_set: assert (high_sum_c[32] == 1'b0)
        else $error("rcp24_mul: the high half carried out of 32 bits (p11=%h crosshi=%h carry=%b)",
                    l_p11_q, l_crosshi_q, l_carry_q);
    end
  end
`endif

endmodule : zhao_raster_rcp24_mul

`default_nettype wire
