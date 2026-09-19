// zhao_light_skin_adapter.sv -- the CREATURE seam, asserted rather than cast.
//
// WHAT CROSSES HERE
// -----------------
// `zhao_geom_skin_norm` / `zref::creature::skin_world_normal` hand out a
// range-reduced direction, s64x3. The lighting service's prepared form is
// s32x3. Those are different widths, and the whole reason this file exists is
// that narrowing them is a CLAIM about the producer's range reduction, not a
// free cast.
//
// NO MAGNITUDE CROSSES HERE ANY MORE (owner ruling R31, 2026-09-19). The
// magnitude is computed ONCE, downstream, by `zhao_light_stream`'s own II8
// root (`n_mag_valid_i` low) -- the root this seam used to bypass. See
// `zhao_geom_skin_norm`'s header for why the root moved and why the law did
// not.
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
// true: the value SIGN-EXTENDS from 32 bits.
//
// WHY REFUSE AND COUNT RATHER THAN TRUNCATE
// -----------------------------------------
// `lambert_from_world_normal` is a public C++ entry point and nothing stops a
// caller handing it an arbitrary s64 tuple. Those calls are legal and are NOT
// this hot port's business. What must never happen is a tuple outside the
// producer's domain being quietly narrowed into a plausible-looking normal:
// the result would be a lit vertex computed from a different vector, which no
// output check can distinguish from a correct one. So an out-of-domain tuple
// is REFUSED at the port and counted on `refused_o` -- its DIRECTION never
// crosses.
//
// BUT ITS VERTEX DOES, AS A DECLARED-DEGENERATE ZERO (2026-09-19, owner ruling
// R31 / entry I46). This block used to drop the refused tuple outright, which
// was harmless while the lit colour was only observed. It is not harmless now:
// GEOM.VATTR stores each lit colour by its ORDER in the batch, so a dropped
// vertex shifts every later colour onto the wrong vertex, and the batch can
// never be complete -- the R31 deadlock in a new place. A refusal pulse beside
// the stream cannot fix it, because the pulse for vertex j can arrive before
// the light stream has emitted vertex i < j. So the refused vertex is emitted
// IN ORDER with zero lanes and the degenerate flag: GEOM.LIGHT lights it as it
// lights any direction-less vertex (DEGEN_BLACK's reading), and one output per
// input holds by construction. The defined fault response, counted, never a
// hang -- the shape of owner ruling R20.
//
// NO RENORMALIZATION, AND THE ONE ROOT IS DOWNSTREAM
// --------------------------------------------------
// This block does not compute a magnitude, does not round the direction into
// a unit vector, and does not reinterpret the tuple as Q1.15. The producer's
// comment says why in one sentence -- "normalising here would round twice,
// once into the unit vector and again in the Lambert quotient, and the law
// has exactly ONE rounding".
//
// DEGENERACY. `skin_world_normal` returns false for a zero-length blend.
// That verdict rides `degenerate_i`; the light stream roots the direction
// itself, and a zero root with the flag low, or a nonzero root with it high,
// is a seam disagreement and is its `seam_mismatch` to count -- this block
// forwards the flag and judges nothing, so the two sides of that comparison
// stay independent (one is SKIN.NORM's verdict, the other the stream's root).
//

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
    input  var logic                     s_degenerate_i,
    input  var logic [3:0]               s_nlights_i,
    input  var logic [SRCW-1:0]          s_src_id_i,

    // ---- the prepared form, narrowed only after the proof --------------------
    output var logic                     p_valid_o,
    input  var logic                     p_ready_i,
    output var logic signed [31:0]       p_nx_o,
    output var logic signed [31:0]       p_ny_o,
    output var logic signed [31:0]       p_nz_o,
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

  logic domain_ok_c;
  always_comb begin
    domain_ok_c = fits_s32(s_nx_i[63:31]) && fits_s32(s_ny_i[63:31]) && fits_s32(s_nz_i[63:31]);
  end

  // Both outcomes emit exactly one prepared record, so both obey ordinary
  // ready/valid: a refused tuple waits for room like any other.
  assign s_ready_o = !p_valid_o || p_ready_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      p_valid_o      <= 1'b0;
      p_nx_o         <= '0;
      p_ny_o         <= '0;
      p_nz_o         <= '0;
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
          p_degenerate_o <= s_degenerate_i;
          p_nlights_o    <= s_nlights_i;
          p_src_id_o     <= s_src_id_i;
          accepted_o     <= accepted_o + CNTW'(1);
        end else begin
          // The vertex crosses, its direction does not: zero lanes, declared
          // degenerate, same source id. See "BUT ITS VERTEX DOES" above.
          p_valid_o      <= 1'b1;
          p_nx_o         <= '0;
          p_ny_o         <= '0;
          p_nz_o         <= '0;
          p_degenerate_o <= 1'b1;
          p_nlights_o    <= s_nlights_i;
          p_src_id_o     <= s_src_id_i;
          refused_o      <= refused_o + CNTW'(1);
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
    end
  end
`endif

endmodule : zhao_light_skin_adapter

`default_nettype wire
