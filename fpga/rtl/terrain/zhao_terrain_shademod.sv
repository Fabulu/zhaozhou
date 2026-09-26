// zhao_terrain_shademod.sv -- THE PALETTE LADDER, AND THE ONE ROUNDING.
//
// ENFORCED-BY: tests/terrain/terrain_shademod_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `fpga/rtl/prod/zhao_console_core.sv`'s entry I13 has been refused four times.
// The third refusal (packet CELLCARRY) named the reason this block removes:
//
//     "THE TEXTURED LAW QUANTISES THE SHADE, AND NO RTL ANYWHERE DOES.
//      Every pass has costed `mod_of(shade, tint, sheet)` as 'a modulation,
//      EXACT at all-unity' and stopped at the tint."
//
// It is not an open question and it never was. The arithmetic is FROZEN in the
// oracle, with its own comment naming it, at
// `reference/src/zrender/terrain.cpp:633-637`:
//
//     const auto mod_of = [&](int32_t shade, int32_t tint_q, int32_t sheet_q) {
//       const int32_t shade_q = (shade + 8191) >> 14;   // the palette ladder (0..4)
//       const __int128 prod = static_cast<__int128>(shade_q << 14) * tint_q * sheet_q;
//       return static_cast<int32_t>(div_rhu_s128(prod, static_cast<__int128>(1) << 32));
//     };
//
// So I13's first blocker is a BUILD, not a decision. This is the build.
//
// ---------------------------------------------------------------------------
// WHAT THE LADDER IS FOR, IN THE SOURCE'S OWN WORDS
// ---------------------------------------------------------------------------
// terrain.cpp:575-580: *"The palette ladder: the flat-shade weight is quantised
// to 2 bits ((shade + 8191) >> 14) BEFORE modulation so the modulated palette
// stays inside the 256-colour capture law; the modulation is ONE round-half-up
// over the s128 product shade x tint x sheet, and all-unity is EXACT unity."*
//
// THE LADDER IS THE 256-COLOUR PALETTE BUDGET EXPRESSED AS ARITHMETIC. A block
// that modulates by the unquantised shade is not "slightly more precise"; it
// produces a colour count the capture law forbids, and every golden CRC in the
// repository would move.
//
// ---------------------------------------------------------------------------
// FIVE RUNGS, NOT FOUR -- AND THE ENTRY THAT SAYS FOUR IS WRONG
// ---------------------------------------------------------------------------
// `zhao_console_core.sv:3809-3811` reads:
//
//     "`ambient()` (`terrain.cpp:563`) returns Q16.16 in [16384, 65536], so
//      `shade_q` takes FOUR values and the textured modulation is QUANTISED
//      TO QUARTER STEPS before the product."
//
// THAT IS FALSE ON THE PATH THAT MATTERS, and the error is in the flattering
// direction -- it makes the ladder look like a 2-bit field with no zero.
// `ambient()` is applied to WALLS (terrain.cpp:670) and UNDERSIDES (:795), and
// both of those call `mod_of(shade, 65536, 65536)` -- unity tint, unity sheet.
// The one path that carries a REAL tint and a REAL sheet is the TEXTURED TOP
// SURFACE, terrain.cpp:731-734:
//
//     const int32_t shade = shade_tri(y, i00, i11, i10);   // NO ambient()
//     span.mod_r = mod_of(shade, tr, sq);
//
// `shade_tri` -> `shade_points` -> `shade_flat_tri`, whose clamp is
// `terrain.cpp:137`: `shade < 0 ? 0 : (shade > 0x10000 ? 0x10000 : shade)`.
// So on the textured top the domain is [0, 65536] and
//
//     shade_q = (shade + 8191) >> 14   in   {0, 1, 2, 3, 4}   -- FIVE rungs,
//
// exactly as the oracle's own comment "(0..4)" says. RUNG ZERO IS REACHABLE AND
// IT IS BLACK: a top-surface triangle turned far enough from the sun modulates
// to exactly 0. A block built from the entry's sentence would have carried a
// 2-bit ladder that cannot express that fragment, and the wrong pixel would
// have been a dark grey where the law says black -- against a capture-exact
// law, with every gate in this repository passing. The directed test walks
// rung 0 explicitly for that reason.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC, REDUCED -- AND THE REDUCTION IS EXACT, NOT APPROXIMATE
// ---------------------------------------------------------------------------
// `div_rhu_s128(n, d)` (reference/src/zrender/rast.cpp:31) with d = 2^32 > 0 is
// `floor((n + 2^31) / 2^32)` followed by an INT32 clamp. Write
// P = shade_q * tint_q * sheet_q, so the law's numerator is P << 14 and
//
//     mod = floor((P*2^14 + 2^31) / 2^32) = floor((P + 2^17) / 2^18)
//
// -- an 18-bit shift on a 38-bit value, not a 48-bit product and a 32-bit one.
// Integer multiplication is associative and exact, so grouping the three
// factors as (shade_q * tint) * sheet introduces NO intermediate rounding; the
// law's "ONE rounding" is the `+ 2^17 >> 18` and it is the only one here.
// The directed test proves the reduction against a transcription of the
// ORIGINAL s128 form rather than against this paragraph.
//
// The INT32 clamp inside `div_rhu_s128` is UNREACHABLE from these ports: the
// largest representable `mod` is 524,280 (see WIDTHS below), 4,096 times below
// the INT32 rail. It is therefore not built, and this sentence is the reason
// rather than an omission -- a clamp that cannot fire is a guard this
// repository has a chapter about.
//
// ---------------------------------------------------------------------------
// ZERO DSP, BY THE PRECEDENT THIS SUBSYSTEM ALREADY SET
// ---------------------------------------------------------------------------
// `zhao_terrain_shade.sv`'s header states the ruling for this corner of the
// machine: *"the DSP budget stands at 171% committed. This block spends the
// abundant resource instead."* The demand here is the same order: 2,000
// terrain triangles per frame x 3 channels = 6,000 modulations against a
// 1,666,666-clock compute frame. This block takes 18 clocks per modulation, so
// 108,000 clocks -- 6.5% of the frame -- and costs NO multiplier at all.
//
// The 17x17 product is a shift-add over `sheet_i`'s 17 bits with ONE 38-bit
// adder. The `shade_q * tint_i` product is NOT a multiplier either: `shade_q`
// is four bits, so it is a four-term shifted sum.
//
// ---------------------------------------------------------------------------
// WIDTHS, AND WHY THE PORTS ARE 17 BITS
// ---------------------------------------------------------------------------
// Every operand of `mod_of` is Q16.16 with 1.0 == 65536, and every one of its
// callers passes a value in [0, 65536] -- `shade` because `shade_flat_tri`
// clamps it there, `tint_q` because `cell_tint` divides a 4-corner sum by its
// own maximum, `sheet_q` because `sheet_factor` returns `(255-(s>>1))<<8` in
// [32768, 65280] or the literal 65536. 65536 needs 17 bits.
//
// A 17-bit port also HOLDS 65537..131071, which the law's domain excludes. This
// block does not narrow the ports to hide that and does not silently clamp:
// it computes `mod_of` EXACTLY over the whole port domain and COUNTS the
// out-of-domain offer on `shade_domain_o`. That is why `mod_o` is 20 bits and
// not 17 -- over the law's domain it is provably <= 65536, and over the port
// domain it reaches 524,280, which the block represents rather than truncates.
// A silent truncation here would be a wrong pixel with a reassuring width.
//
//     shade_q = (shade_i + 8191) >> 14        <= 8            4 bits
//     mcand   = shade_q * tint_i              <= 1,048,568   21 bits
//     P       = mcand  * sheet_i              <= 2^37.03     38 bits
//     mod_o   = (P + 131072) >> 18            <= 524,280     20 bits
//
// Over the LAW's domain (all three <= 65536): shade_q <= 4, P <= 2^34,
// mod_o <= 65536, and all-unity is EXACTLY 65536.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS NOT
// ---------------------------------------------------------------------------
//   * It is NOT the fragment modulation. `mod_o` is the per-primitive,
//     per-channel GAIN that `TextureSpan::mod_r/g/b` carries
//     (reference/src/zrender/internal.hpp:251); the rasteriser then applies it
//     as `sat_u8((texel * mod + 32768) >> 16)` (rast.cpp:349-351). This block
//     produces the gain and nothing else.
//   * It is NOT composed, and that is deliberate. The consumer is a terrain
//     triangle arriving at a raster, which is exactly what I13 is about;
//     composing a gain producer with no fragment to gain would be the
//     "prefix of a chain whose last link does not exist" that three packets
//     have correctly refused. THIS IS A DEFERRAL AND IT IS WRITTEN DOWN AS
//     ONE: the carriage packet that closes I13 instantiates this module and
//     deletes this paragraph.
//   * It does NOT supersede `zhao_texture_sheetmod`. That block is the
//     UNTEXTURED profile's unit8 sheet tint (terrain.cpp:718/:756) and stays
//     exactly as it is. This block is the TEXTURED profile's Q16.16 form,
//     which the oracle writes as a different lambda because it IS different
//     arithmetic: one wide rounding over three factors, against that block's
//     one narrow rounding over two.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply.
`default_nettype none

module zhao_terrain_shademod (
    input  var logic        clk,
    input  var logic        rst_n,

    // ---- the request: one channel's modulation ----------------------------
    input  var logic        req_valid_i,
    output var logic        req_ready_o,
    // The triangle's flat shade, Q16.16. The law's domain is [0, 65536]
    // (`shade_flat_tri`'s clamp01); an offer above it is computed exactly and
    // counted on `shade_domain_o`.
    input  var logic [16:0] shade_i,
    // The layer-H cell tint for THIS channel, Q16.16, from `cell_tint`.
    // Unity (65536) on walls and undersides, which pass no tint.
    input  var logic [16:0] tint_i,
    // The surface sheet's factor, Q16.16, from `sheet_factor`: 65536 when no
    // sheet exists, else `(255 - (strength >> 1)) << 8`.
    input  var logic [16:0] sheet_i,

    // ---- the response -----------------------------------------------------
    output var logic        rsp_valid_o,
    input  var logic        rsp_ready_i,
    // `mod_of(shade_i, tint_i, sheet_i)`, bit-exact. See WIDTHS above for why
    // this is 20 bits and not 17.
    output var logic [19:0] mod_o,

    // ---- counters (wrapping; the composer differences them per frame) ------
    // Requests ACCEPTED. This is the "how many times" counter this repository
    // has a chapter about: a result-checking test cannot see a block that
    // modulates the same triangle twice, and the throughput budget above is
    // written against this number.
    output var logic [15:0] issued_o,
    // Offers with `shade_i > 65536`, i.e. a caller that did not apply
    // `shade_flat_tri`'s clamp01. The arithmetic is still exact; what is
    // broken is the PALETTE BUDGET, because rungs above 4 exist only outside
    // the law's domain. Reachable with legal stimulus -- the directed test
    // fires it -- so it needs no committed mutant.
    output var logic [15:0] shade_domain_o
);

  // ---- the ladder: `(shade + 8191) >> 14`, verbatim ----------------------
  // shade_i is unsigned, so this is a logical shift and matches the oracle's
  // signed `>>` over the whole port domain (the oracle's own callers never
  // present a negative shade -- `shade_flat_tri` clamps at 0).
  //
  // THE DISCARDED BITS ARE THE LADDER. `>> 14` throws away the low fourteen
  // bits by design -- that IS the quantisation, and a rung is flat across all
  // 16,384 of them. The waiver is scoped to this signal with its reason rather
  // than set on the file, and section 3 of the directed test asserts the
  // flatness those bits are dropped to produce.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [17:0] ladder_sum_c;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [ 3:0] shade_q_c;
  assign ladder_sum_c = {1'b0, shade_i} + 18'd8191;
  assign shade_q_c    = ladder_sum_c[17:14];

  // ---- `shade_q * tint_i`: four bits, so four shifted terms, no multiplier -
  logic [20:0] mcand_c;
  always_comb begin
    mcand_c = 21'd0;
    if (shade_q_c[0]) mcand_c = mcand_c + {4'd0, tint_i};
    if (shade_q_c[1]) mcand_c = mcand_c + {3'd0, tint_i, 1'b0};
    if (shade_q_c[2]) mcand_c = mcand_c + {2'd0, tint_i, 2'd0};
    if (shade_q_c[3]) mcand_c = mcand_c + {1'b0, tint_i, 3'd0};
  end

  // ---- the shift-add over `sheet_i`, 17 steps, ONE 38-bit adder -----------
  localparam logic [1:0] S_IDLE = 2'd0, S_MUL = 2'd1, S_DONE = 2'd2;

  logic [ 1:0] st_q;
  logic [37:0] acc_q;     // the running product P
  logic [37:0] mc_q;      // mcand, shifted left one place per step
  logic [16:0] mp_q;      // sheet_i, shifted right one place per step
  logic [ 4:0] cnt_q;

  assign req_ready_o = (st_q == S_IDLE);
  assign rsp_valid_o = (st_q == S_DONE);

  wire take_c = req_valid_i && req_ready_o;
  wire drop_c = rsp_valid_o && rsp_ready_i;

  // The law's one rounding: `floor((P + 2^17) / 2^18)`.
  // `acc_q` is at most 2^37.03 and 2^38 is 2.749e11, so the addend cannot
  // carry out of 38 bits; a 39th bit would be a permanently-zero signal and
  // this file does not carry one.
  //
  // `round_c[17:0]` is the remainder the law's FLOOR discards. Dropping it is
  // the rounding; keeping it would be a second result nobody may read.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [37:0] round_c;
  /* verilator lint_on UNUSEDSIGNAL */
  assign round_c = acc_q + 38'd131072;
  assign mod_o   = round_c[37:18];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q           <= S_IDLE;
      acc_q          <= 38'd0;
      mc_q           <= 38'd0;
      mp_q           <= 17'd0;
      cnt_q          <=  5'd0;
      issued_o       <= 16'd0;
      shade_domain_o <= 16'd0;
    end else begin
      case (st_q)
        S_IDLE: begin
          // case0: accept a request and seed the shift-add.
          if (take_c) begin
            acc_q    <= 38'd0;
            mc_q     <= {17'd0, mcand_c};
            mp_q     <= sheet_i;
            cnt_q    <= 5'd0;
            st_q     <= S_MUL;
            issued_o <= issued_o + 16'd1;
            if (shade_i > 17'd65536) shade_domain_o <= shade_domain_o + 16'd1;
          end
        end
        S_MUL: begin
          // case1: one partial product per clock; `mc_q` carries the shift so
          // the adder sees two aligned operands and nothing else.
          if (mp_q[0]) acc_q <= acc_q + mc_q;
          mc_q  <= {mc_q[36:0], 1'b0};
          mp_q  <= {1'b0, mp_q[16:1]};
          cnt_q <= cnt_q + 5'd1;
          if (cnt_q == 5'd16) st_q <= S_DONE;
        end
        S_DONE: begin
          // case2: hold the result until the consumer takes it.
          if (drop_c) st_q <= S_IDLE;
        end
        default: begin
          // case3: unreachable; recover rather than latch.
          st_q <= S_IDLE;
        end
      endcase
    end
  end

endmodule

`default_nettype wire
