// zhao_raster_attrdiv_svc.sv — the attribute divide as a SERVICE, so throughput
// is a parameter sweep rather than a rewrite.
//
// ENFORCED-BY: tests/raster/raster_attrdiv_svc_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THE UNIT IS `zhao_raster_attrdiv_v2` AS OF 2026-09-20 (owner ruling R100/R104)
// ---------------------------------------------------------------------------
// This service pooled `zhao_raster_attrdiv` (v1) until today. v1 implements
// round-half-AWAY-FROM-ZERO; `spec/qformats.md:53`, `:147` and
// `zref::render::div_rhu_s128` all say round-half-UP, and v2 matched the
// reference on every one of 640,000 sampled pairs while v1 disagrees on 100% of
// negative exact halves with an even divisor -- a rate of about 1/(4d), which
// is over 10% on a one-subpixel triangle. ADOPTING V2 IS A BUG FIX. R86 also
// made composing a superseded module fatal in every production root, and this
// was one of the last two.
//
// TWO THINGS MOVED, AND NEITHER IS A RENAME.
//
//   REFUSAL. v1 published one `q_overflow_o` meaning "q_o is unusable". v2
//   publishes `q_saturated_o` AND `q_error_o`, and they are different events:
//   an ERROR is a zero area and q_o is meaningless, while a SATURATION is a
//   VALID oracle result -- the reference saturates to INT32_MAX/MIN and so does
//   v2, where v1 refused and published zero. This service therefore publishes
//   BOTH and COLLAPSES NEITHER. Folding them back into one bit would throw away
//   the distinction the reference makes, which is the "silently wrong value the
//   consumer cannot tell about" shape this tree has paid for repeatedly.
//
//   LATENCY, AND IT IS THE EXPENSIVE HALF. v1 walked 33 quotient positions
//   because the quotient cannot exceed the attribute range; v2 walks all 98 of
//   the dividend. Measured: 36 -> 101 clocks at radix 2, 20 -> 52 at radix 4,
//   about 2.8x. The correct answer costs 2.8x the divider occupancy and the
//   table below is restated at the real number rather than the flattering one.
//   Recovering it is a real, funded follow-on -- v2 can take v1's window trick
//   without touching its rounding -- but it is a separate change and is NOT
//   quietly assumed here.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// The unit is exact and measures 101 clocks a divide at radix 2. One of them
// sustains 16,501 attribute-pixels a frame against a terrain-primary component
// of 276,480 pixels that each need at least `invw24` before early-Z. So a single
// divider is roughly 17x short for depth alone, and the renderer ruling that
// covers it says the divide is "a tagged service with a measured initiation
// rate, not one divider per lane".
//
// This is that service. It is deliberately the SAME SHAPE the Field engine
// arrived at after six sweep rounds: N identical units behind one ready/valid
// port, with N a build parameter, so the answer to "how much divide do we need"
// is a sweep against a real frame instead of an argument.
//
//     UNITS   accepted / 101 clocks   attribute-pixels a frame  (radix 2)
//        1            1                       16,501
//        2            2                       33,003
//        4            4                       66,006
//        8            8                      132,013
//
// At radix 4 the unit is 52 clocks, so the same column reads 32,051 / 64,102 /
// 128,205 / 256,410. Neither radix reaches 276,480 at UNITS = 8, which is a
// sharper statement of the same wall v1's table already implied and is exactly
// the kind of number this service exists to produce.
//
// ---------------------------------------------------------------------------
// ORDER IS A CORRECTNESS PROPERTY, NOT A CONVENIENCE
// ---------------------------------------------------------------------------
// Fragments reach the tile store in raster order and the tile store's laws
// assume it. So this service returns answers IN ISSUE ORDER, and it does so
// without a reorder buffer: units are issued round-robin from `iss_r` and
// retired round-robin from `ret_r`, so the retire pointer simply waits for the
// unit whose turn it is. That is correct whatever each unit's latency turns out
// to be -- if a future divider answers in variable time the service still
// returns in order, it just stalls instead of reordering. Nothing here depends
// on the 36 being constant.
//
// The tag is carried alongside rather than used as identity for the reorder:
// it exists so the CALLER can say which attribute and which pixel this was,
// and it is returned untouched.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES NOT DO
// ---------------------------------------------------------------------------
// It does not decide how many divides a frame needs -- that is early-Z's
// ordering and the caller's business. It does not skip a divide it could have
// avoided. `accepted_o`, `retired_o` and `stall_clocks_o` are here so a real
// frame can say what UNITS should be, because the Field lane's lesson was that
// the wall is whichever resource REFUSES, and a service that cannot report its
// own refusals cannot be sized.
`default_nettype none

module zhao_raster_attrdiv_svc #(
    parameter int unsigned UNITS = 4,
    parameter int unsigned TAGW  = 16,
    // Forwarded to every unit. UNITS and RADIX are the two independent knobs on
    // this service and the sweep crosses them, because "more units" and
    // "shorter units" cost different things and the fit decides which is
    // cheaper -- not this file.
    parameter int unsigned RADIX = 2
) (
    input var logic clk,
    input var logic rst_n,

    // ---- requests, in order --------------------------------------------------
    input  var logic                  v_valid_i,
    output var logic                  v_ready_o,
    input  var logic signed [95:0]    num_i,
    input  var logic        [46:0]    area_i,
    input  var logic [TAGW-1:0]       tag_i,

    // ---- answers, IN ISSUE ORDER --------------------------------------------
    output var logic                  r_valid_o,
    input  var logic                  r_ready_i,
    output var logic signed [31:0]    q_o,
    // TWO SIGNALS, NOT ONE RENAMED. `q_error_o` is v1's refusal: the area was
    // zero and `q_o` is meaningless. `q_saturated_o` is NOT a refusal -- the
    // quotient did not fit s32 and `q_o` carries the saturated value, which is
    // what the reference returns. A consumer that needs v1's old "unusable"
    // predicate wants `q_error_o`; a consumer that cannot use a clamped value
    // (a recurrence seed, say) must refuse on BOTH and say so in its own
    // header. See THE UNIT IS ... above.
    output var logic                  q_saturated_o,
    output var logic                  q_error_o,
    // The unit's Euclidean remainder, forwarded with its answer. The stepping
    // path seeds from it, so it has to survive the service rather than stop at
    // the unit -- and a service that dropped it would force every caller back
    // to a bare divider.
    //
    // 47 BITS, NOT v1's 48, AND IT IS A DIFFERENT QUANTITY. v1 divided 2|n|+A
    // by 2A and published a remainder mod 2A; v2 divides M = n + floor(A/2) by
    // A and publishes the TRUE FLOOR REMAINDER, so 0 <= rem_o < A <= 2^47-1 and
    // the pair satisfies q_o*A + rem_o == M. Zero and meaningless when
    // `q_saturated_o` or `q_error_o` is set. The width is measured, not
    // asserted -- see `rem_range_seen_o`.
    output var logic [46:0]           rem_o,
    output var logic [TAGW-1:0]       tag_o,

    // ---- evidence, so UNITS is chosen by measurement -------------------------
    output var logic [31:0] accepted_o,
    output var logic [31:0] retired_o,
    // Clocks in which a request was offered and the service had no free unit.
    // This is the number that says whether UNITS is too small; a service that
    // never stalls is either big enough or never asked.
    output var logic [31:0] stall_clocks_o,
    // STICKY, POOL-WIDE: has ANY unit ever published a residue that did not fit
    // the 47 bits `rem_o` carries? Deliberately one bit rather than a copy of
    // the units' 32-bit counters: the state is structurally unreachable while
    // each unit's restoring compare is correct, so the only question anyone can
    // ask of it is "did it ever happen", and a per-unit adder tree would spend
    // real ALM re-deriving a bit. The unit's own counter keeps the count.
    //
    // Named differently from the unit's `rem_range_err_o` ON PURPOSE -- same
    // name at a different width in a wrapper is how a counter audit reads the
    // wrong thing. Its positive control is the unit's, fired by the committed
    // mutant tests/mutants/zhao_raster_attrdiv_v2_remwidth_mutant.sv.
    output var logic        rem_range_seen_o
);

  // A single unit still needs a pointer, and $clog2(1) is 0, which would make a
  // zero-width index. Same guard the Field ring service needed.
  localparam int unsigned PW = (UNITS <= 2) ? 1 : $clog2(UNITS);

  logic [UNITS-1:0]        u_vvalid, u_vready;
  logic [UNITS-1:0]        u_rvalid, u_rready;
  logic signed [31:0]      u_q       [UNITS];
  logic [46:0]             u_rem     [UNITS];
  logic                    u_sat     [UNITS];
  logic                    u_err     [UNITS];
  logic [31:0]             u_remerr  [UNITS];
  logic [TAGW-1:0]         u_tag_r   [UNITS];

  logic [PW-1:0] iss_r, ret_r;

  // ---- issue ---------------------------------------------------------------
  // Only the unit whose turn it is may take work: round-robin issue is what
  // makes round-robin retire an ORDER guarantee rather than a coincidence.
  always_comb begin
    u_vvalid = '0;
    for (int unsigned u = 0; u < UNITS; ++u) begin
      u_vvalid[u] = v_valid_i && (PW'(u) == iss_r);
    end
  end
  assign v_ready_o = u_vready[iss_r];

  // ---- retire --------------------------------------------------------------
  always_comb begin
    u_rready = '0;
    for (int unsigned u = 0; u < UNITS; ++u) begin
      u_rready[u] = r_ready_i && (PW'(u) == ret_r);
    end
  end
  assign r_valid_o     = u_rvalid[ret_r];
  assign q_o           = u_q[ret_r];
  assign q_saturated_o = u_sat[ret_r];
  assign q_error_o     = u_err[ret_r];
  assign rem_o         = u_rem[ret_r];
  assign tag_o         = u_tag_r[ret_r];

  // Pool-wide sticky OR. Reads every unit's counter, not just the retiring
  // one -- a guard that only looked at `ret_r` would be blind to any unit not
  // currently retiring, which is the "detector that cannot see the fault"
  // shape. Combinational from registers the units already hold.
  logic rem_range_seen_c;
  always_comb begin
    rem_range_seen_c = 1'b0;
    for (int unsigned u = 0; u < UNITS; ++u) begin
      if (u_remerr[u] != 32'd0) rem_range_seen_c = 1'b1;
    end
  end
  assign rem_range_seen_o = rem_range_seen_c;

  // The genvar is declared SEPARATELY and the loop is wrapped in an
  // explicit generate block. Quartus 17.0.2's parser rejects both
  // `for (genvar N = ...)` and a loop generate at module level, while
  // both Verilator and slang accept them, so it only ever surfaces in a
  // composed fit. See zhao_crc32c_fold.sv, which hit it first.
  genvar g;
  generate
    for (g = 0; g < int'(UNITS); ++g) begin : g_unit
      zhao_raster_attrdiv_v2 #(.RADIX(RADIX)) u_div (
          .clk             (clk),
          .rst_n           (rst_n),
          .v_valid_i       (u_vvalid[g]),
          .v_ready_o       (u_vready[g]),
          .num_i           (num_i),
          .area_i          (area_i),
          .r_valid_o       (u_rvalid[g]),
          .r_ready_i       (u_rready[g]),
          .q_o             (u_q[g]),
          .q_saturated_o   (u_sat[g]),
          .q_error_o       (u_err[g]),
          .rem_o           (u_rem[g]),
          .rem_range_err_o (u_remerr[g]),
          // Sunk deliberately: the SERVICE counts accepted and retired for the
          // whole pool, which is the number that sizes UNITS. A per-unit divide
          // count would only re-derive it, and a per-unit busy count says
          // nothing the pool's stall clocks do not. `saturations_o` and
          // `errors_o` are sunk for a different reason: both events are
          // published PER ANSWER at the retire port, so a pool total would be
          // a second way to count what the caller already sees.
          /* verilator lint_off PINCONNECTEMPTY */
          .divides_o     (),
          .saturations_o (),
          .errors_o      (),
          .busy_clocks_o ()
          /* verilator lint_on PINCONNECTEMPTY */
      );
    end
  endgenerate

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      iss_r          <= '0;
      ret_r          <= '0;
      accepted_o     <= 32'd0;
      retired_o      <= 32'd0;
      stall_clocks_o <= 32'd0;
      for (int unsigned u = 0; u < UNITS; ++u) u_tag_r[u] <= '0;
    end else begin
      if (v_valid_i && v_ready_o) begin
        u_tag_r[iss_r] <= tag_i;
        iss_r          <= (PW'(iss_r) == PW'(UNITS - 1)) ? '0 : (iss_r + PW'(1));
        accepted_o     <= accepted_o + 32'd1;
      end else if (v_valid_i) begin
        stall_clocks_o <= stall_clocks_o + 32'd1;
      end

      if (r_valid_o && r_ready_i) begin
        ret_r     <= (PW'(ret_r) == PW'(UNITS - 1)) ? '0 : (ret_r + PW'(1));
        retired_o <= retired_o + 32'd1;
      end
    end
  end

endmodule : zhao_raster_attrdiv_svc

`default_nettype wire
