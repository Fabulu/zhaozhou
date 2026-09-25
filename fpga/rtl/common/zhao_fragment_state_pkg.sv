// zhao_fragment_state_pkg.sv -- THE FRAGMENT PIPELINE'S 32-BIT STATE WORD,
// v1, as ONE SCHEMA. Types, constants and named profiles only; no hardware.
//
// RATIFIED by the owner vacation directive of 2026-09-23, section 3: "ratify
// the existing fragment consumer's state semantics as the v1 producer/consumer
// contract, then generate its producer fields and validation from ONE schema.
// Do not create a second incompatible stencil enum."
//
// NOTHING HERE IS NEW LAW, and that is the point. Every span and every encoding
// below was ALREADY what `fpga/rtl/raster/zhao_raster_fragment.sv` decodes
// (lines 394-407, and 717/722 for the two modulation bits consumed a stage
// earlier), what `design/contracts/RASTER.FRAGMENT.md`'s state table publishes,
// and what `zref::FragmentPipeline::State` packs. Measured field by field
// against all three on 2026-09-25 (FRAGSTATE) before this file was written:
// they agree, and the directive's three enums agree with them. So the published
// encoding is PRESERVED bit for bit and every existing capture still decodes.
//
// WHAT WAS ACTUALLY WRONG was not the encoding but its CUSTODY. The word was
// hand-maintained in FOUR independent copies -- the RTL decode, a prose table in
// that same RTL header, the contract's Markdown table, and the C++ mirror's own
// hand-written pack()/unpack(). Two of those four are checked by nothing
// whatever and can rot in silence; there was no generator, no schema, and no
// entry for the word in `spec/commands.zidl`. A producer that wanted to DECLARE
// a state therefore had to handwrite numeric slices.
//
// ---------------------------------------------------------------------------
// WHY THIS IS ITS OWN PACKAGE AND NOT PART OF `zhao_render_texture_pkg`
// ---------------------------------------------------------------------------
// That package is the obvious home -- it already declares the 48-bit
// continuation tail this word travels beside, the Early-Z key that carries it,
// the 490-bit pretex packet and the retire context. It is the wrong one, for
// two reasons that only became visible by trying it.
//
// 1. THE TEXTURE ISLAND CARRIES THIS WORD OPAQUELY AND NEVER DECODES IT. It
//    needs the WIDTH -- which `zhao_render_texture_pkg` already has, as
//    `fragment_state` inside the Early-Z key -- and nothing else. A field
//    schema for a word the island does not interpret is not the island's
//    business; it belongs beside the consumer that DOES decode it and the
//    producers that declare it.
//
// 2. AND THE TREE ENFORCES THAT, which is how the mistake was caught.
//    `zhao_render_texture_pkg.sv` is inside the texture island's FROZEN
//    interface manifest: `tools/rtl/texture_v3_interface_parser.py` pins its
//    exact bytes, the manifest records its sha256 in an ordered source closure,
//    and `canonical_interface_sha256` covers the result. ANY edit to it --
//    including a comment -- moves all three. Worse, a packed struct there emits
//    one duplicate-name MEMBERDTYPE per member, and a 14-member struct moved the
//    island's pinned marker count 105 -> 119. That file's own 2026-09-18 note
//    had already refused the identical trade at TWO markers.
//
//    Putting the schema here costs the island nothing: its package is byte
//    identical to before this packet, all three hashes are untouched, and the
//    marker count is still 105.
//
// SO THE DIVISION IS: `zhao_render_texture_pkg` owns the PACKETS this word
// rides in; this package owns what its bits MEAN. Neither duplicates the other.
//
// THE PACKED STRUCT LIVES IN THE FIXTURE, deliberately, for reason 2 above:
// `tests/tools/fixtures/zhao_render_texture_layout_top.sv` declares one and
// checks all fourteen fields against the spans below, each with its own
// `+FRAG_SPAN_CONTROL` fire control. A production closure carries the law; the
// fixture carries the independently written second opinion about it.

`default_nettype none

package zhao_fragment_state_pkg;

  localparam int unsigned FRAG_STATE_W = 32;

  // ---- the thirty-two explicit bits -----------------------------------------
  // `state == 0` is THE PLAIN OPAQUE WRITE: depth test off, depth written, blend
  // REPLACE, no alpha test, stencil ALWAYS + REPLACE, tag written from the
  // packet. That is not cosmetic -- it is what let `zhao_raster_tile_pipe`'s
  // phase-4 flat-colour behaviour survive the fragment block's arrival bit for
  // bit. Hence the two `_DIS` (disable) bits: an enable bit would have made the
  // zero state a no-op instead of a write.
  //
  // THERE ARE NO RESERVED HOLES, deliberately: every 32-bit value is a legal
  // state, so the randomized lane filters nothing and there is no
  // malformed-state path to specify. A consequence worth knowing before anyone
  // goes looking: a NEW selector -- an explicit flat-colour profile, say --
  // CANNOT be a bit of this word. It has to be primitive metadata. See
  // `design/contracts/RASTER.FRAGMENT.md`, section "THE PROVOKING VERTEX".
  localparam int unsigned FRAG_Z_TEST_EN_BIT      = 0;
  localparam int unsigned FRAG_Z_WRITE_DIS_BIT    = 1;
  localparam int unsigned FRAG_Z_FORCE_FAR_BIT    = 2;
  localparam int unsigned FRAG_BLEND_LO           = 3;
  localparam int unsigned FRAG_BLEND_HI           = 4;
  localparam int unsigned FRAG_SHADE_MOD_BIT      = 5;
  localparam int unsigned FRAG_ALPHA_MOD_BIT      = 6;
  localparam int unsigned FRAG_ATEST_EN_BIT       = 7;
  localparam int unsigned FRAG_ATEST_REF_LO       = 8;
  localparam int unsigned FRAG_ATEST_REF_HI       = 15;
  localparam int unsigned FRAG_STEN_FUNC_LO       = 16;
  localparam int unsigned FRAG_STEN_FUNC_HI       = 17;
  localparam int unsigned FRAG_STEN_OP_LO         = 18;
  localparam int unsigned FRAG_STEN_OP_HI         = 19;
  localparam int unsigned FRAG_TAG_WRITE_DIS_BIT  = 20;
  localparam int unsigned FRAG_TAG_FROM_TEXEL_BIT = 21;
  localparam int unsigned FRAG_TAG_CHANNEL_LO     = 22;
  localparam int unsigned FRAG_TAG_CHANNEL_HI     = 23;
  // A COMPARE mask, not a write mask: it masks EQUAL / NOTEQUAL only, and
  // `out_sten` never references it. Named here because three separate passes
  // described entry I20 as owing "a stencil reference AND MASK" and went
  // looking for a port to carry the mask. It was always a field of this word.
  localparam int unsigned FRAG_STEN_MASK_LO       = 24;
  localparam int unsigned FRAG_STEN_MASK_HI       = 31;

  // ---- the directive's three enums, which are the consumer's own numbering --
  // ALL TWELVE ARE NAMED, including the two the RTL only ever reached through a
  // `default:` arm (`STEN_FUNC` 3 NEVER at `zhao_raster_fragment.sv:474`, and
  // `STEN_OP` 3 DECR_SAT at :533). An encoding that exists only as somebody's
  // fallback cannot be referred to by a producer, and a fifth stencil function
  // could have been added with nothing flagging the collision.
  localparam logic [1:0] FRAG_BLEND_REPLACE     = 2'd0;
  localparam logic [1:0] FRAG_BLEND_ALPHA       = 2'd1;
  localparam logic [1:0] FRAG_BLEND_ADD         = 2'd2;
  localparam logic [1:0] FRAG_BLEND_ADD_MOD     = 2'd3;

  localparam logic [1:0] FRAG_STEN_FUNC_ALWAYS   = 2'd0;
  localparam logic [1:0] FRAG_STEN_FUNC_EQUAL    = 2'd1;
  localparam logic [1:0] FRAG_STEN_FUNC_NOTEQUAL = 2'd2;
  localparam logic [1:0] FRAG_STEN_FUNC_NEVER    = 2'd3;

  localparam logic [1:0] FRAG_STEN_OP_REPLACE   = 2'd0;
  localparam logic [1:0] FRAG_STEN_OP_KEEP      = 2'd1;
  localparam logic [1:0] FRAG_STEN_OP_INCR_SAT  = 2'd2;
  localparam logic [1:0] FRAG_STEN_OP_DECR_SAT  = 2'd3;

  // `spec/stars_and_flares.md` 1 is FROZEN: `tag = (channel << 6) | strength`,
  // and GLOW is channel 1. This is that channel number, not a new allocation.
  localparam logic [1:0] FRAG_TAG_CHANNEL_GLOW  = 2'd1;

  // ---- the one constructor every profile is built from ----------------------
  // Each argument is placed through its NAMED span, never a literal bit number,
  // so a profile cannot disagree with the schema it is declared in.
  function automatic logic [FRAG_STATE_W-1:0] frag_state_make(
      input logic       z_test_en,
      input logic       z_write_dis,
      input logic       z_force_far,
      input logic [1:0] blend,
      input logic       shade_mod,
      input logic       alpha_mod,
      input logic       atest_en,
      input logic [7:0] atest_ref,
      input logic [1:0] sten_func,
      input logic [1:0] sten_op,
      input logic       tag_write_dis,
      input logic       tag_from_texel,
      input logic [1:0] tag_channel,
      input logic [7:0] sten_mask);
    logic [FRAG_STATE_W-1:0] w;
    begin
      w = '0;
      w[FRAG_Z_TEST_EN_BIT]       = z_test_en;
      w[FRAG_Z_WRITE_DIS_BIT]     = z_write_dis;
      w[FRAG_Z_FORCE_FAR_BIT]     = z_force_far;
      w[FRAG_BLEND_LO       +: 2] = blend;
      w[FRAG_SHADE_MOD_BIT]       = shade_mod;
      w[FRAG_ALPHA_MOD_BIT]       = alpha_mod;
      w[FRAG_ATEST_EN_BIT]        = atest_en;
      w[FRAG_ATEST_REF_LO   +: 8] = atest_ref;
      w[FRAG_STEN_FUNC_LO   +: 2] = sten_func;
      w[FRAG_STEN_OP_LO     +: 2] = sten_op;
      w[FRAG_TAG_WRITE_DIS_BIT]   = tag_write_dis;
      w[FRAG_TAG_FROM_TEXEL_BIT]  = tag_from_texel;
      w[FRAG_TAG_CHANNEL_LO +: 2] = tag_channel;
      w[FRAG_STEN_MASK_LO   +: 8] = sten_mask;
      frag_state_make = w;
    end
  endfunction

  // ---- THE NAMED PROFILES A PRODUCER DECLARES -------------------------------
  // "Profile selection is explicit, not inferred from whether a port happens to
  // be zero" (the directive). These are how a producer says WHICH profile it
  // wants; they are the SystemVerilog twins of
  // `zref::FragmentPipeline::sky_backdrop()` and its five siblings.
  //
  // THE ZERO WORD IS A PROFILE AND IT HAS A NAME NOW. `opaque_geometry()`
  // returns 32'd0, and the offset contract PINS that. Giving it a name is what
  // lets a producer DECLARE it, and lets a reader tell a declared opaque profile
  // from an undriven port -- which is the distinction entry I20 turns on, and
  // the reason `MaterialRecord.fragment_decl` bit 0 exists rather than a
  // `state != 0` test.
  function automatic logic [FRAG_STATE_W-1:0] frag_profile_opaque_geometry();
    frag_profile_opaque_geometry = frag_state_make(
        1'b0, 1'b0, 1'b0, FRAG_BLEND_REPLACE, 1'b0, 1'b0, 1'b0, 8'd0,
        FRAG_STEN_FUNC_ALWAYS, FRAG_STEN_OP_REPLACE, 1'b0, 1'b0, 2'd0, 8'd0);
  endfunction

  // sky_and_beams 1.1 pass 1: "Z-test off, Z-write far, blend off, effect-tag
  // init". STEN_OP KEEP: the backdrop owns colour/tag/depth, not the stencil.
  function automatic logic [FRAG_STATE_W-1:0] frag_profile_sky_backdrop();
    frag_profile_sky_backdrop = frag_state_make(
        1'b0, 1'b0, 1'b1, FRAG_BLEND_REPLACE, 1'b0, 1'b0, 1'b0, 8'd0,
        FRAG_STEN_FUNC_ALWAYS, FRAG_STEN_OP_KEEP, 1'b0, 1'b0, 2'd0, 8'd0);
  endfunction

  // sky_and_beams 1.1 layer `sky_`: "Z-test on, Z-write off, alpha blend
  // out = dst*(1-a)+src*a, a = tex.a x vertex.a". The cloud sheet writes no tag.
  function automatic logic [FRAG_STATE_W-1:0] frag_profile_sky_cloud_fade();
    frag_profile_sky_cloud_fade = frag_state_make(
        1'b1, 1'b1, 1'b0, FRAG_BLEND_ALPHA, 1'b0, 1'b1, 1'b0, 8'd0,
        FRAG_STEN_FUNC_ALWAYS, FRAG_STEN_OP_KEEP, 1'b1, 1'b0, 2'd0, 8'd0);
  endfunction

  // sky_and_beams 1.1 layer `sun_`: "dst = sat(dst + src*tex.a), glow
  // effect-tag write on".
  //
  // THE ONE RECIPE THAT GENUINELY WANTS THE TAIL'S CONSTANT TAG, and the reason
  // `MaterialRecord.fragment_decl` carries an `effect_tag` at all. The sun quad
  // is 64x64 ARGB4444 -- direct colour, with NO CLUT INDEX to read a strength
  // out of -- so `tag_from_texel` is LOW here and the tag rides the packet's own
  // constant field. Only the star recipes sample CLUT8, and only they can honour
  // `stars_and_flares` 1's "strength = the source texel's CLUT intensity".
  function automatic logic [FRAG_STATE_W-1:0] frag_profile_sun_additive();
    frag_profile_sun_additive = frag_state_make(
        1'b1, 1'b1, 1'b0, FRAG_BLEND_ADD_MOD, 1'b0, 1'b0, 1'b0, 8'd0,
        FRAG_STEN_FUNC_ALWAYS, FRAG_STEN_OP_KEEP, 1'b0, 1'b0,
        FRAG_TAG_CHANNEL_GLOW, 8'd0);
  endfunction

  // sky_and_beams 2: "colour = tex.RGB x vertex.RGB; dst = sat(dst + src)",
  // depth test ON, depth write OFF -- beams never occlude anything, and a beam
  // is not a glow-probe source.
  function automatic logic [FRAG_STATE_W-1:0] frag_profile_beam_additive_fade();
    frag_profile_beam_additive_fade = frag_state_make(
        1'b1, 1'b1, 1'b0, FRAG_BLEND_ADD, 1'b1, 1'b0, 1'b0, 8'd0,
        FRAG_STEN_FUNC_ALWAYS, FRAG_STEN_OP_KEEP, 1'b1, 1'b0, 2'd0, 8'd0);
  endfunction

  // stars 1: "CLUT8 nearest, alpha-test index 0, Z-test on / Z-write off,
  // glow-tag write with strength = the texel's CLUT intensity".
  function automatic logic [FRAG_STATE_W-1:0] frag_profile_star_disc_masked();
    frag_profile_star_disc_masked = frag_state_make(
        1'b1, 1'b1, 1'b0, FRAG_BLEND_REPLACE, 1'b0, 1'b0, 1'b1, 8'd0,
        FRAG_STEN_FUNC_ALWAYS, FRAG_STEN_OP_KEEP, 1'b0, 1'b1,
        FRAG_TAG_CHANNEL_GLOW, 8'd0);
  endfunction

  // stars 1: "same sampling, dst = sat(dst+src)". No alpha test: 4 says
  // `pal_h[0]` is black, the additive identity, so the halo needs no mask.
  function automatic logic [FRAG_STATE_W-1:0] frag_profile_star_halo_additive();
    frag_profile_star_halo_additive = frag_state_make(
        1'b1, 1'b1, 1'b0, FRAG_BLEND_ADD, 1'b0, 1'b0, 1'b0, 8'd0,
        FRAG_STEN_FUNC_ALWAYS, FRAG_STEN_OP_KEEP, 1'b0, 1'b1,
        FRAG_TAG_CHANNEL_GLOW, 8'd0);
  endfunction

  // FORGE.SHADOW's profile, owner ruling R89. A shadow is ordinary transparent
  // geometry: it blends with what it falls on and writes no depth, because a
  // shadow does not occlude the thing casting it.
  //
  // BLEND=ALPHA is the field that makes R89's flat per-caster alpha reach the
  // picture AT ALL -- with the REPLACE this console used before,
  // `zhao_raster_blend_fin`'s arm is `acc = src_i` and the alpha's product is
  // computed and thrown away. `tag_write_dis` so a shadow does not erase the
  // effect tag of whatever it falls across.
  function automatic logic [FRAG_STATE_W-1:0] frag_profile_shadow_alpha();
    frag_profile_shadow_alpha = frag_state_make(
        1'b1, 1'b1, 1'b0, FRAG_BLEND_ALPHA, 1'b0, 1'b0, 1'b0, 8'd0,
        FRAG_STEN_FUNC_ALWAYS, FRAG_STEN_OP_KEEP, 1'b1, 1'b0, 2'd0, 8'd0);
  endfunction

  // ---- the literal-pinned offset contract -----------------------------------
  // Deliberately NOT derived from neighbouring constants: changing a
  // declaration and its helper constants in lockstep must still make the
  // elaboration guard fail.
  localparam bit FRAG_STATE_OFFSET_CONTRACT_OK =
      (FRAG_STATE_W == 32) &&
      (FRAG_Z_TEST_EN_BIT == 0) &&
      (FRAG_Z_WRITE_DIS_BIT == 1) &&
      (FRAG_Z_FORCE_FAR_BIT == 2) &&
      (FRAG_BLEND_LO == 3) && (FRAG_BLEND_HI == 4) &&
      (FRAG_SHADE_MOD_BIT == 5) &&
      (FRAG_ALPHA_MOD_BIT == 6) &&
      (FRAG_ATEST_EN_BIT == 7) &&
      (FRAG_ATEST_REF_LO == 8) && (FRAG_ATEST_REF_HI == 15) &&
      (FRAG_STEN_FUNC_LO == 16) && (FRAG_STEN_FUNC_HI == 17) &&
      (FRAG_STEN_OP_LO == 18) && (FRAG_STEN_OP_HI == 19) &&
      (FRAG_TAG_WRITE_DIS_BIT == 20) &&
      (FRAG_TAG_FROM_TEXEL_BIT == 21) &&
      (FRAG_TAG_CHANNEL_LO == 22) && (FRAG_TAG_CHANNEL_HI == 23) &&
      (FRAG_STEN_MASK_LO == 24) && (FRAG_STEN_MASK_HI == 31) &&
      // The directive's three enums, pinned as literals. A second incompatible
      // stencil enum is exactly what section 3 forbids, so the numbers are
      // asserted here rather than trusted to a reader comparing two tables.
      (FRAG_BLEND_REPLACE == 2'd0) && (FRAG_BLEND_ALPHA == 2'd1) &&
      (FRAG_BLEND_ADD == 2'd2) && (FRAG_BLEND_ADD_MOD == 2'd3) &&
      (FRAG_STEN_FUNC_ALWAYS == 2'd0) && (FRAG_STEN_FUNC_EQUAL == 2'd1) &&
      (FRAG_STEN_FUNC_NOTEQUAL == 2'd2) && (FRAG_STEN_FUNC_NEVER == 2'd3) &&
      (FRAG_STEN_OP_REPLACE == 2'd0) && (FRAG_STEN_OP_KEEP == 2'd1) &&
      (FRAG_STEN_OP_INCR_SAT == 2'd2) && (FRAG_STEN_OP_DECR_SAT == 2'd3) &&
      (FRAG_TAG_CHANNEL_GLOW == 2'd1) &&
      // THE ZERO WORD IS THE OPAQUE PROFILE. This is the pin that makes "a
      // declared opaque profile is not an undriven port" checkable rather than
      // rhetorical: if the encoding ever moved so that all-zero stopped meaning
      // the plain opaque write, every producer that declares it would change
      // behaviour silently, and this fails instead.
      (frag_profile_opaque_geometry() == 32'd0);

endpackage : zhao_fragment_state_pkg

// The elaboration guard, in its own module because Quartus 17.0 will not take a
// bare module-scope elaboration check -- it needs `initial begin ... end`, and a
// `--lint-only` run does not execute it either way. See CLAUDE.md's note on both.
module zhao_fragment_state_guard #(
  parameter bit FRAG_STATE_OFFSET_CONTRACT_OK_P =
      zhao_fragment_state_pkg::FRAG_STATE_OFFSET_CONTRACT_OK
);
  initial begin : p_frag_state_contract
    if (!FRAG_STATE_OFFSET_CONTRACT_OK_P)
      $fatal(1, "ZHAO_FRAG_STATE_CONTRACT_FIRE[1]: FRAG_STATE_OFFSET_CONTRACT");
  end
endmodule

`default_nettype wire
