// zhao_texture_mod255.sv — the residue `h mod 255` of a 32-bit word, as a
// carry-save byte fold, factored out of zhao_texture_mosaic so it can be
// PROVED rather than only sampled (TEXTURE.MOSAIC, ZH-030).
//
// Law:
//   spec/terrain_rules.md 6.2 (frozen 2026-08-16) — "`p = h mod 255`". The
//     modulus is 255 and not 256: the pattern's period must NOT be a power of
//     two, or the hash's low bits alone would decide the pick and the dither
//     would align to the texel grid. 255 is the law; this module is how it is
//     spent without a divider.
//   reference/include/zref/zref_terrain.hpp `zref::terrain::mosaic_pick` —
//     `h % 255u` on a `uint32_t`. THE oracle.
//
// WHY A FOLD AND NOT A DIVIDER, and why it is EXACT rather than approximate.
// 256 = 255 + 1, so 2^8 = 1 (mod 255) and therefore
//     h = b3.2^24 + b2.2^16 + b1.2^8 + b0 = b3 + b2 + b1 + b0   (mod 255)
// for the four bytes of h. The same identity folds the 10-bit byte sum, then
// the 9-bit result, and one conditional subtract lands it in [0, 254]:
//
//   s0 = b0 + b1 + b2 + b3            <= 1020, 10 bits
//   s1 = s0[7:0] + s0[9:8]            <=  258,  9 bits
//   s2 = s1[7:0] + s1[8]              <=  255,  9 bits
//   p  = (s2 >= 255) ? s2 - 255 : s2  in [0, 254]
//
// Each line is the identity x = (x & 255) + (x >> 8) (mod 255), which holds
// for every x, so the chain is exact for EVERY 32-bit h and not merely for the
// hashes this machine happens to produce.
//
// The 9-bit bound on s2 is worth stating exactly, because it is what makes ONE
// conditional subtract sufficient. s1 <= 258, so either s1 <= 255 and s2 = s1,
// or s1 is one of 256..258 and s2 = s1 - 255, i.e. 1..3. Either way s2 <= 255,
// and 255 is the ONLY value that is not already the canonical residue: it
// corrects to 0. (h = 0xFFFF_FFFF is exactly that case — 255 x 16,843,009, so
// its residue is 0 and s2 reaches 255.) The subtract is written to cope with
// 256 as well, which costs nothing and cannot fire; a reader who re-derives
// the bound should get 255 and not be surprised. That bound is a fact about
// the widths above, not an assumption about the input, which is why
// tests/formal/texture_mod255.sby proves the whole thing TOTAL.
//
// REJECTED ALTERNATIVE: the reciprocal-multiply form `p = h - 255 * ((h *
// 0x80808081) >> 39)`. It is one 32x32 multiply and one 8x32 multiply — more
// hardware than four byte-adds — and its correctness rests on a magic-number
// bound that a reader cannot check by eye. The fold's correctness is three
// lines of modular arithmetic anyone can check, and the formal lane checks it
// anyway.
//
// Purely combinational, no state, no clock. Conservative SystemVerilog subset
// only (charter 2).

module zhao_texture_mod255 (
    input  logic [31:0] h_i,
    output logic [ 7:0] p_o  // h mod 255, always in [0, 254]
);

  // The four bytes of h. 2^8 = 1 (mod 255), so their sum is congruent to h.
  logic [9:0] s0;
  logic [8:0] s1;
  logic [8:0] s2;

  always_comb begin
    s0 = {2'b00, h_i[7:0]} + {2'b00, h_i[15:8]} + {2'b00, h_i[23:16]} + {2'b00, h_i[31:24]};
    s1 = {1'b0, s0[7:0]} + {7'b0000000, s0[9:8]};
    s2 = {1'b0, s1[7:0]} + {8'b00000000, s1[8]};
    // s2 <= 255 (see the bound above), so ONE conditional subtract reaches
    // the canonical residue, and it fires only for the single value 255.
    p_o = (s2 >= 9'd255) ? (s2[7:0] - 8'd255) : s2[7:0];
  end

endmodule
