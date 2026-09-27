// zhao_material_token_pkg.sv -- THE 32-BIT FIELD MATERIAL TOKEN, v1, AS ONE
// SCHEMA. Types, constants and pure functions only; no hardware.
//
// WHAT THIS IS FOR. `FIELD.WRITE.MATERIAL` (`design/ops.yml`) writes "2
// candidate material IDs + blend weight per cell". Its ratified reference
// function `zref::fieldir::compose_material` takes a `MaterialState` TRIPLE and
// returns a TRIPLE. But the Earth record's out-lane 2 -- the wire that actually
// carries a field material result out of the engine -- is a flat 32-bit word:
// `zhao_field_earth_adapter.sv`'s `material_o`, and `zhao_terrain_patch_acc`'s
// `out_mat_0..3_o`, are `[31:0]` at every hop.
//
// Until this file, NOTHING IN THIS TREE MAPPED BETWEEN THOSE TWO SHAPES. That
// absence -- not a missing port, not a missing owner, not an owner decision --
// is what entry I34 records as material's blocker, in paragraph (P3): "an
// absent u32 <-> {matA, matB, weight} law".
//
// ---------------------------------------------------------------------------
// THE LOW 24 BITS ARE NOT A CHOICE. THEY ARE ALREADY THIS TREE'S CONVENTION.
// ---------------------------------------------------------------------------
// The one thing this file must not do is invent a SECOND byte order. Two
// composed, shipping blocks already commit to a {matA, matB, weight} layout,
// MSB-first, and they were measured at the line before a bit of this was
// written:
//
//   * `zhao_terrain_compcache_front.sv` -- the layer-E material plane packs
//     `mat_m[addr] <= {mat_w_a_i, mat_w_b_i, mat_w_weight_i}` and unpacks it as
//     `mat_rd_q[23:16]` / `[15:8]` / `[7:0]`.
//   * `zhao_texture_island_v3_top.sv` -- the mosaic's intake reads
//     `.wr_mosaic_material_a_i(frag_base_rgb_i[23:16])` and
//     `.wr_mosaic_material_b_i(frag_base_rgb_i[15:8])`.
//
// So `[23:16] = matA`, `[15:8] = matB`, `[7:0] = weight` is PRESERVED here, bit
// for bit, and this package states it once instead of a third hand-maintained
// copy. The entry's sentence "nothing packs three u8s into that u32 and nothing
// unpacks it" is true of the THIRTY-TWO-bit form and was too strong about the
// twenty-four-bit one; the correction is recorded in FINDINGS-materialpath.md
// rather than quietly absorbed, because it is the reason this layout is not
// free to be pretty.
//
// ---------------------------------------------------------------------------
// WHAT IS NEW IS THE TOP BYTE, AND IT IS A VERSION TAG RATHER THAN PADDING
// ---------------------------------------------------------------------------
// `[31:24]` is the only genuinely unallocated part of the word, and it carries
// `ZMT_TAG_V1`. This is the "versioned extension" the owner vacation directive
// authorises in place of stealing bits from a live field: no existing field is
// overloaded, no handle is truncated, and nothing that decodes today changes.
//
// THE TAG EXISTS SO THAT AN UNDECODABLE TOKEN CAN BE REFUSED RATHER THAN
// SILENTLY COMPOSED. That is the whole reason it is not padding:
//
//   * `spec/terrain_rules.md` sec 6.2 makes weight 0 mean "matB everywhere" and
//     255 "matA everywhere". EVERY 24-bit pattern is therefore a LEGAL
//     material. A decoder with no tag has no way to tell a real result from a
//     stuck bus, a short record, or a lane that was never written -- it would
//     compose whatever arrived and the picture would be wrong in silence.
//   * The ratified presence law is "an absent output is NOT a write of zero"
//     (`reports/OWNER-DECISION-20260926-I34-NAV.md` sec 3). `32'd0` has tag
//     `8'h00`, so the additive-zero the adapter parks on an absent lane is
//     REFUSED by construction instead of decoding as {0,0,0} -- which is a
//     perfectly legal "matB everywhere" triple and would have been indis-
//     tinguishable from an authored one.
//
// A refused token is COUNTED by the consumer, never substituted. This package
// decides nothing about what to do with a refusal; it only says whether the
// word is a v1 material token.
//
// THE REFERENCE HALF is `zref::fieldir::material_token_encode` /
// `material_token_decode` (`reference/include/zref/zref_fieldir.hpp`), written
// against this same layout, and `spec/qformats.md` sec 14 is the prose. Those
// two spec anchors -- `material-ids` and `material-state` -- are the sections
// `design/ops.yml` has cited since before they existed.

`default_nettype none

package zhao_material_token_pkg;

  // ---- the thirty-two bits, stated once ------------------------------------
  localparam int unsigned ZMT_TOKEN_W = 32;

  localparam int unsigned ZMT_WEIGHT_LO = 0;
  localparam int unsigned ZMT_MAT_B_LO  = 8;
  localparam int unsigned ZMT_MAT_A_LO  = 16;
  localparam int unsigned ZMT_TAG_LO    = 24;
  localparam int unsigned ZMT_FIELD_W   = 8;

  // v1. Mnemonic: Earth material, revision 1. Any other value is not a v1
  // token and this package says so rather than guessing.
  localparam logic [7:0] ZMT_TAG_V1 = 8'hE1;

  // The payload the ratified `compose_material` law speaks in.
  localparam int unsigned ZMT_TRIPLE_W = 24;

  // ---- encode --------------------------------------------------------------
  function automatic logic [ZMT_TOKEN_W-1:0] zmt_encode(input logic [7:0] mat_a,
                                                        input logic [7:0] mat_b,
                                                        input logic [7:0] weight);
    zmt_encode = {ZMT_TAG_V1, mat_a, mat_b, weight};
  endfunction

  // ---- the predicate that makes a refusal possible -------------------------
  function automatic logic zmt_tag_ok(input logic [ZMT_TOKEN_W-1:0] tok);
    zmt_tag_ok = (tok[ZMT_TAG_LO +: ZMT_FIELD_W] == ZMT_TAG_V1);
  endfunction

  // ---- decode --------------------------------------------------------------
  // Deliberately three accessors and no struct: the consumers of this word are
  // three separate 8-bit ports on an already-composed write face, and a packed
  // struct here would add members to a closure that does not want them. The
  // caller checks `zmt_tag_ok` FIRST; these say nothing about validity.
  function automatic logic [7:0] zmt_mat_a(input logic [ZMT_TOKEN_W-1:0] tok);
    zmt_mat_a = tok[ZMT_MAT_A_LO +: ZMT_FIELD_W];
  endfunction

  function automatic logic [7:0] zmt_mat_b(input logic [ZMT_TOKEN_W-1:0] tok);
    zmt_mat_b = tok[ZMT_MAT_B_LO +: ZMT_FIELD_W];
  endfunction

  function automatic logic [7:0] zmt_weight(input logic [ZMT_TOKEN_W-1:0] tok);
    zmt_weight = tok[ZMT_WEIGHT_LO +: ZMT_FIELD_W];
  endfunction

  // ---- the twenty-four-bit payload, for the planes that store it -----------
  function automatic logic [ZMT_TRIPLE_W-1:0] zmt_triple(input logic [ZMT_TOKEN_W-1:0] tok);
    zmt_triple = tok[ZMT_TRIPLE_W-1:0];
  endfunction

endpackage

`default_nettype wire
