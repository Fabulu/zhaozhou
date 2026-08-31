// zhao_raster_toon_div.sv — the toon ramp's channel divide, pipelined: one
// channel accepted every clock.
//
// ENFORCED-BY: tests/raster/raster_toon_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// RASTER.TOON's first implementation walked a 64-iteration restoring divider
// three times and measured 201 clocks a cel fragment -- 8,291 a frame against a
// 320,000 stress profile, 38x short. The law was right and the rate was not.
//
// This is the same arithmetic as an array. One channel enters per clock, one
// leaves per clock after a fixed latency, so a three-channel fragment costs
// THREE clocks instead of two hundred.
//
// ---------------------------------------------------------------------------
// 32 STAGES, NOT 64, AND WHY THAT IS SOUND
// ---------------------------------------------------------------------------
// The dividend is `|lane * q|`, up to 64 bits. The QUOTIENT, though, is written
// back into a 32-bit light lane, so a result that needs more than 32 bits is
// already outside what the caller can carry.
//
// So the array is 32 stages seeded with the dividend's HIGH word, shifting in
// the low word. That is exact whenever `num_hi < den`, which is precisely the
// condition for the quotient to fit 32 bits. When it does not hold the block
// says so on `overflow_o` rather than producing a truncated light value --
// a wrong band is a visible edge on the creature, and silence would make it
// look like an art bug.
//
// For Zixxtrixx's shipped constants the question does not arise: lanes are
// around 1e5 and levels around 8e4, so `|lane*q|` is about 34 bits against a
// mean near 5e4, and the quotient is roughly 18 bits.
//
// Cost: 32 stages of a 33-bit compare-subtract, carrying the shrinking dividend
// and the growing quotient. That is a few thousand flops -- affordable where a
// 64-stage 64-bit array would not be.
//
// ---------------------------------------------------------------------------
// TRUNCATION IS THE CALLER'S, NOT THIS BLOCK'S
// ---------------------------------------------------------------------------
// This block divides MAGNITUDES. `rast.cpp` truncates toward zero, which is
// magnitude-then-sign, so the caller hands over `|num|` and the sign it wants
// applied and gets `|quotient|` back with that sign carried alongside. Putting
// the sign rule here would hide it; it belongs where the law is written down.
`default_nettype none

module zhao_raster_toon_div #(
    // Quotient bits, and therefore pipeline stages. 32 is the width of a light
    // lane; see 32 STAGES above.
    parameter int unsigned QBITS = 32,
    // Carried alongside each channel so the caller can reassemble fragments.
    parameter int unsigned TAGW = 24
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one channel ---------------------------------------------------------
    // Free-running: there is no backpressure into a fixed-latency array, so the
    // CALLER must not issue faster than it can retire. `v_valid_i` simply marks
    // a stage as carrying real work.
    input var logic              v_valid_i,
    input var logic [63:0]       num_i,     // |lane * q|
    input var logic [31:0]       den_i,     // |mean|, > 0
    input var logic              neg_i,     // sign to carry, applied by the caller
    input var logic [TAGW-1:0]   tag_i,

    output var logic            r_valid_o,
    output var logic [31:0]     quo_o,      // |quotient|
    output var logic            neg_o,
    output var logic            overflow_o, // did not fit QBITS; quo_o is not usable
    output var logic [TAGW-1:0] tag_o
);

  // Stage state. `rem` needs one bit above the divisor so a shifted remainder
  // cannot lose its top before the compare.
  logic [32:0]       rem_s   [QBITS+1];
  logic [31:0]       low_s   [QBITS+1];   // the dividend bits still to shift in
  logic [31:0]       den_s   [QBITS+1];
  logic [31:0]       quo_s   [QBITS+1];
  logic              vld_s   [QBITS+1];
  logic              neg_s   [QBITS+1];
  logic              ovf_s   [QBITS+1];
  logic [TAGW-1:0]   tag_s   [QBITS+1];

  // ---- stage 0: load -------------------------------------------------------
  // The seed is the dividend's high word. If it already reaches the divisor the
  // quotient needs more than QBITS bits, which is the overflow the header
  // describes.
  always_comb begin
    rem_s[0] = {1'b0, num_i[63:32]};
    low_s[0] = num_i[31:0];
    den_s[0] = den_i;
    quo_s[0] = 32'd0;
    vld_s[0] = v_valid_i;
    neg_s[0] = neg_i;
    ovf_s[0] = (num_i[63:32] >= den_i) || (den_i == 32'd0);
    tag_s[0] = tag_i;
  end

  generate
    for (genvar st = 0; st < int'(QBITS); ++st) begin : g_stage
      // One restoring step, most significant quotient bit first.
      logic [32:0] shifted;
      logic        fits;
      always_comb begin
        shifted = {rem_s[st][31:0], low_s[st][31 - st]};
        fits    = (shifted >= {1'b0, den_s[st]});
      end

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          rem_s[st+1] <= 33'd0;
          low_s[st+1] <= 32'd0;
          den_s[st+1] <= 32'd0;
          quo_s[st+1] <= 32'd0;
          vld_s[st+1] <= 1'b0;
          neg_s[st+1] <= 1'b0;
          ovf_s[st+1] <= 1'b0;
          tag_s[st+1] <= '0;
        end else begin
          rem_s[st+1] <= fits ? (shifted - {1'b0, den_s[st]}) : shifted;
          low_s[st+1] <= low_s[st];
          den_s[st+1] <= den_s[st];
          quo_s[st+1] <= fits ? (quo_s[st] | (32'd1 << (QBITS - 1 - st))) : quo_s[st];
          vld_s[st+1] <= vld_s[st];
          neg_s[st+1] <= neg_s[st];
          ovf_s[st+1] <= ovf_s[st];
          tag_s[st+1] <= tag_s[st];
        end
      end
    end
  endgenerate

  assign r_valid_o  = vld_s[QBITS];
  assign quo_o      = quo_s[QBITS];
  assign neg_o      = neg_s[QBITS];
  assign overflow_o = ovf_s[QBITS];
  assign tag_o      = tag_s[QBITS];

  // The final remainder is not published: the toon law needs the quotient only,
  // unlike RASTER.ATTRDIV whose remainder seeds the stepping recurrence.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [32:0] rem_unused;
  assign rem_unused = rem_s[QBITS];
  /* verilator lint_on UNUSEDSIGNAL */

endmodule : zhao_raster_toon_div

`default_nettype wire
