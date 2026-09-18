// zhao_light_skin_adapter.sv -- the CREATURE seam, asserted rather than cast.
//
// WHAT CROSSES HERE
// -----------------
// `zhao_geom_skin_norm` / `zref::creature::skin_world_normal` hand out a
// {direction:s64x3, magnitude:u64} PAIR. The lighting service's prepared
// form is {direction:s32x3, magnitude:u32}. Those are different widths, and
// the whole reason this file exists is that narrowing them is a CLAIM about
// the producer's range reduction, not a free cast.
//
// The claim, from the producer's own source (creature_core.cpp,
// `skin_world_normal`):
//
//     while (max|n| >= 2^30) { n >>= 1; }
//
// so every lane satisfies |n| <= 2^30 afterwards -- note 2^30 and not
// 2^30 - 1, because an ARITHMETIC right shift of a negative odd value rounds
// toward minus infinity and can land exactly on -2^30 while the tracked
// maximum is 2^30 - 1. Asserting a strict |n| < 2^30 would therefore reject a
// legal tuple, which is why this block asserts the property that is actually
// true: the value SIGN-EXTENDS from 32 bits. The magnitude is the floor root
// of at most 3 * 2^60 and so fits u32 with room; that is asserted too.
//
// WHY REFUSE AND COUNT RATHER THAN TRUNCATE
// -----------------------------------------
// `lambert_from_world_normal` is a public C++ entry point and nothing stops a
// caller handing it an arbitrary s64 tuple. Those calls are legal and are NOT
// this hot port's business. What must never happen is a tuple outside the
// producer's domain being quietly narrowed into a plausible-looking normal:
// the result would be a lit vertex computed from a different vector, which no
// output check can distinguish from a correct one. So an out-of-domain tuple
// is REFUSED at the port, counted on `refused_o`, and never enters the arena.
//
// NO RENORMALIZATION, NO SECOND ROOT
// ----------------------------------
// The pair arrives with its magnitude already computed by the producer's own
// `isqrt_u64`. This block does not recompute it, does not round the direction
// into a unit vector, and does not reinterpret the tuple as Q1.15. The
// producer's comment says why in one sentence -- "normalising here would
// round twice, once into the unit vector and again in the Lambert quotient,
// and the law has exactly ONE rounding" -- and a magnitude arriving through
// this port is the `supplied_mags` case downstream, costing zero root jobs.
//
// DEGENERACY. `skin_world_normal` returns false for a zero-length blend.
// That verdict rides `degenerate_i`; a zero magnitude with the flag low, or a
// nonzero magnitude with it high, is a seam disagreement and is the
// downstream service's `seam_mismatch` to count -- this block forwards both
// and judges neither, so the two sides of that comparison stay independent.
//
// Quartus 17 form law obeyed (guard inside `initial begin`). Lint-clean is
// not synthesizability; this block has not been through `quartus_map`.
`default_nettype none

module zhao_light_skin_adapter #(
    parameter int unsigned SRCW = 16,
    parameter int unsigned CNTW = 32
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the producer's pair, at its own width -------------------------------
    input  var logic                     s_valid_i,
    output var logic                     s_ready_o,
    input  var logic signed [63:0]       s_nx_i,
    input  var logic signed [63:0]       s_ny_i,
    input  var logic signed [63:0]       s_nz_i,
    input  var logic        [63:0]       s_mag_i,
    input  var logic                     s_degenerate_i,
    input  var logic [3:0]               s_nlights_i,
    input  var logic [SRCW-1:0]          s_src_id_i,

    // ---- the prepared form, narrowed only after the proof --------------------
    output var logic                     p_valid_o,
    input  var logic                     p_ready_i,
    output var logic signed [31:0]       p_nx_o,
    output var logic signed [31:0]       p_ny_o,
    output var logic signed [31:0]       p_nz_o,
    output var logic        [31:0]       p_mag_o,
    output var logic                     p_degenerate_o,
    output var logic [3:0]               p_nlights_o,
    output var logic [SRCW-1:0]          p_src_id_o,

    output var logic [CNTW-1:0]          accepted_o,
    output var logic [CNTW-1:0]          refused_o
);

  initial begin
    if (SRCW < 1) $fatal(1, "zhao_light_skin_adapter: SRCW must be positive");
    if (CNTW < 8) $fatal(1, "zhao_light_skin_adapter: CNTW below 8 makes the counters decorative");
  end

  // A lane is in domain iff it sign-extends from 32 bits: bits 63..31 all
  // equal. That admits exactly the s32 range including -2^31, and therefore
  // admits the producer's -2^30 boundary case without inventing a tighter
  // bound the producer does not actually honour.
  // Only the sign-extension field is examined, so the argument is that field
  // and not the whole word -- a 64-bit argument whose low 31 bits are never
  // read is a lint warning today and a reader's question forever.
  function automatic logic fits_s32(input logic [32:0] ext);
    fits_s32 = (ext == '0) || (ext == '1);
  endfunction

  logic lanes_ok_c, mag_ok_c, domain_ok_c;
  always_comb begin
    lanes_ok_c  = fits_s32(s_nx_i[63:31]) && fits_s32(s_ny_i[63:31]) && fits_s32(s_nz_i[63:31]);
    mag_ok_c    = (s_mag_i[63:32] == 32'd0);
    domain_ok_c = lanes_ok_c && mag_ok_c;
  end

  // A refused tuple is consumed and dropped; it must not stall the producer
  // and must not reach the arena. An accepted tuple obeys ordinary
  // ready/valid.
  assign s_ready_o = domain_ok_c ? (!p_valid_o || p_ready_i) : 1'b1;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      p_valid_o      <= 1'b0;
      p_nx_o         <= '0;
      p_ny_o         <= '0;
      p_nz_o         <= '0;
      p_mag_o        <= '0;
      p_degenerate_o <= 1'b0;
      p_nlights_o    <= '0;
      p_src_id_o     <= '0;
      accepted_o     <= '0;
      refused_o      <= '0;
    end else begin
      if (p_valid_o && p_ready_i) p_valid_o <= 1'b0;
      if (s_valid_i && s_ready_o) begin
        if (domain_ok_c) begin
          p_valid_o      <= 1'b1;
          p_nx_o         <= s_nx_i[31:0];
          p_ny_o         <= s_ny_i[31:0];
          p_nz_o         <= s_nz_i[31:0];
          p_mag_o        <= s_mag_i[31:0];
          p_degenerate_o <= s_degenerate_i;
          p_nlights_o    <= s_nlights_i;
          p_src_id_o     <= s_src_id_i;
          accepted_o     <= accepted_o + CNTW'(1);
        end else begin
          refused_o <= refused_o + CNTW'(1);
        end
      end
    end
  end

`ifndef SYNTHESIS
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end
  always_ff @(posedge clk) begin
    if (armed_q && s_valid_i && s_ready_o && domain_ok_c) begin
      // The narrowing is lossless on everything that is accepted. Stated as
      // a property of the ACCEPTED set, so it proves the refusal is doing its
      // job rather than restating the refusal condition.
      a_nx_lossless : assert ($signed({{32{s_nx_i[31]}}, s_nx_i[31:0]}) == s_nx_i);
      a_ny_lossless : assert ($signed({{32{s_ny_i[31]}}, s_ny_i[31:0]}) == s_ny_i);
      a_nz_lossless : assert ($signed({{32{s_nz_i[31]}}, s_nz_i[31:0]}) == s_nz_i);
      a_mag_lossless : assert ({32'd0, s_mag_i[31:0]} == s_mag_i);
    end
  end
`endif

endmodule : zhao_light_skin_adapter

`default_nettype wire
