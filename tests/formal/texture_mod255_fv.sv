// texture_mod255_fv.sv — formal harness for the Mosaic pattern's residue
// network (TEXTURE.MOSAIC / ZH-030; property texture_mod255.sby).
//
// WHAT IS PROVED, and why it is not vacuous.
//
// The DUT is `zhao_texture_mod255`, the EXACT module `zhao_texture_mosaic`
// instantiates — there is one instance in the tree and this is it, so nothing
// here is a copy of the datapath. The free input `h` is 32 bits, which IS the
// port width: spec/terrain_rules.md 6.2's hash is a `uint32_t`, so every value
// the block can ever be handed is in scope. This is a TOTAL proof over all
// 4,294,967,296 inputs, not a bound.
//
// THE REFERENCE IS `h mod 255`, EXPRESSED WITHOUT A DIVIDER. Asserting
// `p == h % 255` directly would put a 32-bit constant division in the property
// itself, which is both expensive for the solver and — more importantly — a
// second implementation of the thing under test. Instead the DEFINING property
// of a residue is stated: p is the unique r < 255 for which some q makes
// h = 255*q + r. So `q` (25 bits: (2^32-1)/255 = 16,843,009 < 2^25) and `r`
// (8 bits) are free inputs, the decomposition is ASSUMED, and the block's
// answer must equal r. Only multiplication, addition and comparison appear —
// no division anywhere — and because the decomposition of an h is unique, the
// assumption pins r to exactly `h % 255` with no freedom left for the solver
// to exploit.
//
//   P1  a_is_the_residue  Whenever h = 255*q + r with r < 255, the block's
//                         output IS r. Two-sided by construction (it is an
//                         equality), so it catches a fold that stops a step
//                         early, one that drops the final correction, one that
//                         wraps at 256, and one that is right only for the
//                         hashes this machine happens to produce.
//
//   P2  a_in_range        The output is always <= 254, unconditionally — with
//                         NO assumption, so it holds for every h whether or
//                         not the witness constrains anything. This is the
//                         property `mosaic_pick`'s weight semantics rest on:
//                         weight 255 means "always matA" ONLY because p can
//                         never reach 255. An 8-bit port makes `p <= 255` a
//                         tautology; `p <= 254` is a theorem about the
//                         conditional subtract.
//
//   P3  a_zero           h = 0 gives 0. Trivial, and asserted because the
//                        obvious wrong fold — one whose final step is an
//                        unconditional subtract of 255 — is wrong here first.
//
// VACUITY. P1 is guarded by an assumption, so it could pass on an
// unsatisfiable antecedent. The cover task is therefore load-bearing rather
// than decorative: it demands that the assumption hold at r = 0, at r = 254,
// at the correcting case (a NON-ZERO h whose residue is 0 — the single input
// the conditional subtract exists for, and the one a random sweep sees once in
// 255 draws), at h = 0xFFFF_FFFF (which is exactly 255 x 16,843,009, so its
// residue is 0 and the byte sum reaches the 255 rail), and at a q of zero and
// a q at the top of its range. If any of those is unreachable, the lane is red.
//
// WHAT THIS DOES NOT PROVE, stated plainly: the two frozen multiplier
// constants, the XOR, the arithmetic shift, the mirrored fold, the `p <
// weight` compare, the pipeline, the handshake and the counter are NOT proved
// here. They are covered by the two differential lanes against
// `zref::terrain::mosaic_pick` / `mirror_texel`
// (tests/texture/texture_mosaic_{directed,random}.cpp) and by the mutation
// evidence in design/contracts/TEXTURE.MOSAIC.md. What IS proved is the one
// piece of arithmetic in the block that a reviewer cannot check by inspection.

`default_nettype none

module texture_mod255_fv (
    input wire        clk,
    input wire [31:0] h,
    input wire [24:0] q,  // the witness quotient: (2^32-1)/255 < 2^25
    input wire [ 7:0] r   // the witness remainder
);

  wire [7:0] p;

  zhao_texture_mod255 dut (
      .h_i(h),
      .p_o(p)
  );

  // h = 255*q + r, computed in a lane wide enough that the product cannot
  // wrap: 255 * (2^25 - 1) + 254 < 2^33, so 34 bits is generous.
  wire [33:0] w_q = {9'd0, q};
  wire [33:0] w_r = {26'd0, r};
  wire [33:0] w_h = {2'd0, h};
  wire [33:0] recomposed = (34'd255 * w_q) + w_r;
  wire decomposes = (recomposed == w_h) && (r < 8'd255);

  always_ff @(posedge clk) begin
    // P1 — the block computes THE residue.
    a_is_the_residue : assert (!decomposes || p == r);

    // P2 — unconditional: p can never reach 255, which is what makes
    // weight 255 mean "always matA" in the pick above.
    a_in_range : assert (p <= 8'd254);

    // P3 — the zero anchor.
    a_zero : assert (h != 32'd0 || p == 8'd0);

    // ---- covers: the witness is satisfiable at every corner that matters --
    c_r_zero_h_nonzero :
    cover (decomposes && r == 8'd0 && h != 32'd0);  // the conditional subtract
    c_r_max : cover (decomposes && r == 8'd254);
    c_r_one : cover (decomposes && r == 8'd1);
    c_h_all_ones : cover (decomposes && h == 32'hFFFF_FFFF);
    c_q_zero : cover (decomposes && q == 25'd0 && r != 8'd0);
    c_q_top : cover (decomposes && q == 25'd16843009);
  end

endmodule

`default_nettype wire
