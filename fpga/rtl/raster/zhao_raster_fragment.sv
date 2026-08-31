// zhao_raster_fragment.sv — RASTER.FRAGMENT: shade one covered fragment and
// perform the depth / stencil / blend work — the charter §4 exemplar block
// (phase 4, ZH-025).
//
// Law (in citation order):
//   design/contracts/RASTER.FRAGMENT.md — the block contract.
//   design/blocks.yml — `inputs: [shaded_candidates, tile_read,
//       texture_samples, soft_particles]`, `outputs: [tile_write,
//       fragment_error]`, `latency: variable`, "1 accepted fast-path fragment
//       per clock", counters `covered_fragments` + `blended_fragments`, and
//       the recipe list this file implements (quoted at each recipe below).
//   spec/qformats.md §8 — `invw24` depth: LARGER IS CLOSER, clear value 0,
//       and THE depth test, quoted exactly: "pass ⟺ d_new > d_old (strict;
//       ties fail; decals use explicit bias)". That is the only depth
//       comparison this spec defines; see THE DEPTH TEST below.
//   spec/qformats.md §2/§3/§4 — `unit8` (value = raw/256), `unit_mul(a,b) =
//       (a·b + 128) >> 8`, and `rescale` as the single rounding primitive.
//       The blend arithmetic lives in zhao_raster_blend, one instance per
//       channel; the modulate below is `unit_mul` and nothing else.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §8 — the 64-bit tile word, the
//       pass order, and the "deterministic blend rounding" basic.
//   spec/sky_and_beams.md §1.1/§2 — `sky_backdrop`, `sky_cloud_fade`,
//       `sun_additive`, `beam_additive_fade`.
//   spec/stars_and_flares.md §1 — `star_disc_masked`, `star_halo_additive`,
//       and the frozen effect-tag convention `tag = (channel << 6) | strength`
//       with `GLOW = 0b01` and strength = the source texel's CLUT intensity.
//
// ---------------------------------------------------------------------------
// THE SIX RATIFIED RECIPES, AND WHAT EACH ONE COSTS HERE
// ---------------------------------------------------------------------------
// Every recipe below is expressible as ONE value of the 32-bit `state` word;
// none of them needed a mode of its own in the datapath, which is the whole
// argument that this state encoding is the right one. (`design/contracts/
// RASTER.FRAGMENT.md` carries the same table with the exact bit patterns, and
// tests/raster/raster_fragment_directed.cpp drives all six by name.)
//
//   sky_backdrop        (sky_and_beams §1.1 pass 1) — "Z-test off, Z-write
//                       far, blend off, effect-tag init". Here: Z_TEST_EN 0,
//                       Z_WRITE_DIS 0, Z_FORCE_FAR 1 (the written depth is
//                       the far constant 0 whatever the fragment carries),
//                       blend REPLACE, tag written from the packet.
//   sky_cloud_fade      (§1.1 layer `sky_`) — "alpha blend out = dst·(1−a) +
//                       src·a, a = tex.a × vertex.a", Z-test on, Z-write off.
//                       Here: blend ALPHA, ALPHA_MOD 1 (that product IS the
//                       one `unit_mul` this block performs on alpha),
//                       Z_TEST_EN 1, Z_WRITE_DIS 1.
//   sun_additive        (§1.1 layer `sun_`) — "dst = sat(dst + src·tex.a),
//                       glow effect-tag write on". Here: blend ADD_MOD,
//                       Z_TEST_EN 1, Z_WRITE_DIS 1, tag written.
//   beam_additive_fade  (§2) — "colour = tex.RGB × vertex.RGB; dst =
//                       sat(dst + src)". Here: SHADE_MOD 1 (that product is
//                       the three `unit_mul`s this block performs on colour),
//                       blend ADD, Z_TEST_EN 1, Z_WRITE_DIS 1.
//   star_disc_masked    (stars §1) — "CLUT8 nearest, alpha-test index 0,
//                       Z-test on / Z-write off, glow-tag write with strength
//                       = the texel's CLUT intensity". Here: ATEST_EN 1 with
//                       ATEST_REF 0 (the test is on the texel INDEX, not on
//                       an alpha byte — see THE ALPHA TEST), blend REPLACE,
//                       Z_TEST_EN 1, Z_WRITE_DIS 1, TAG_FROM_TEXEL 1 with
//                       TAG_CHANNEL = GLOW.
//   star_halo_additive  (stars §1) — "same sampling, dst = sat(dst+src)".
//                       Here: identical to the above with blend ADD and no
//                       alpha test (`pal_h[0]` is black, the additive
//                       identity, so the halo needs no mask).
//
// ---------------------------------------------------------------------------
// WHAT IS NOT BUILT, NAMED PLAINLY RATHER THAN FAKED
// ---------------------------------------------------------------------------
// TEXTURE.TMU DOES NOT EXIST. Four of the six recipes name a sampler —
// `beam_additive_fade` says "bilinear TMU mandatory", the star recipes say
// "CLUT8 nearest+mips" and "nearest mandatory — bilinear must never touch a
// palette". None of that is in this file and none of it is imitated here:
// there is no sampler, no filter, no palette lookup, no mip selection and no
// wrap/clamp/mirror mode. What this block does instead is take the SAMPLED
// TEXEL as three fields of the fragment packet — `frag_texel_rgb_i`,
// `frag_texel_a_i`, `frag_texel_idx_i` — and consume them exactly as the
// recipes say to consume a sampled texel: modulate, alpha-weight, index-test,
// and take the tag strength from the CLUT intensity. That is the clean
// interface TEXTURE.TMU fills in when it lands; the ledger already has this
// block requesting from TEXTURE.TMU downstream, and wiring that request
// channel is that block's increment, not this one's. Nothing here will need
// to change when it does — the texel arrives from a port instead of from a
// test driver.
//
// PART.SOFT DOES NOT EXIST EITHER, and — usefully — needs no port. The ledger
// lists `soft_particles` among this block's inputs; a soft particle's
// contribution is a depth-proximity FADE, i.e. an alpha modulation. That
// arrives on `frag_vert_a_i` like every other alpha, and PART.SOFT computes
// the factor. So no port is invented for it: adding one would be inventing a
// second alpha lane for a block that already has the right one.
//
// FOG IS NOT COMPUTED HERE, AND THAT IS THE SPEC'S DECISION, NOT AN OMISSION.
// `design/blocks.yml`'s purpose line for this block says "depth/stencil/blend/
// fog", and that line predates the ratified fog law. spec/qformats.md §8
// (added 2026-08-17) freezes fog as a PER-VERTEX operation in GEOM.PROJECT —
// "fog is a vertex-colour operation in GEOM.PROJECT, ordered AFTER lighting
// and AFTER the global tint … The fogged colour rides the ordinary Gouraud
// path — the factor is not a separate interpolant and there is no
// per-fragment fog anywhere in v1 (a per-pixel form would be a RASTER.FRAGMENT
// recipe change: not costed, not built)". So the colour arriving on
// `frag_vert_rgb_i` is ALREADY fogged, this block applies no fog of its own,
// and building a per-fragment fog stage here would contradict a ratified
// spec. The exempt list (sky family, additive emissive) is honoured by
// construction: this block cannot fog anything.
// ENFORCED-BY: tests/raster/raster_fragment_directed.cpp:test_fog_is_a_vertex_operation
// (the §8 vertex mix is computed in the test across a factor sweep and must
// reach the tile UNALTERED — a block that grew a fog stage would double-apply
// it and that case would go red).
//
// ALSO NOT BUILT, so the next wave knows: no attribute interpolation (colour,
// alpha, depth and UV all arrive interpolated — GEOM.SETUP's job); no
// coverage (RASTER.EDGEWALK); no early-Z (RASTER.EARLYZ, by ruling 1.D); no
// dither or framebuffer write (RASTER.RESOLVE); no separate stencil-fail or
// depth-fail stencil ops (see THE STENCIL); no MSAA; no `texture_stalls`
// counter (there is no texture path to stall).
//
// ---------------------------------------------------------------------------
// THE PIPELINE — two stages, and why the hazard everyone expects is not here
// ---------------------------------------------------------------------------
// This block is a read-modify-write on RASTER.TILESTORE's port A:
//
//   cycle N    — a candidate is accepted; it lands in stage 0.
//   cycle N+1  — stage 0 issues the tile-store READ at the fragment's address.
//   cycle N+2  — the store presents the destination word (its `latency:
//                fixed:1`). Stage 1 runs the whole test-and-blend chain
//                combinationally and issues the tile-store WRITE in this same
//                cycle.
//
// The obvious hazard — two fragments one cycle apart at the SAME pixel, the
// second reading before the first writes — cannot happen, and not by luck.
// The second fragment's read is issued in cycle N+2, the exact cycle the
// first fragment's write is issued, and RASTER.TILESTORE's ordering rule 3 is
// write-first: "a read returns NEW data for a same-cycle same-address write
// … because a read-modify-write fragment pipeline must never see the pixel it
// just wrote as stale". This block is that fragment pipeline, and that rule
// was written for exactly this cycle. So back-to-back fragments at one pixel
// are correct with no forwarding path, no scoreboard and no stall — which is
// what makes "1 accepted fast-path fragment per clock" reachable at all.
// ENFORCED-BY: tests/raster/raster_fragment_directed.cpp:test_same_pixel_raw
//
// LATENCY IS `variable` (ledger) even though the fast path above is a flat
// two cycles from acceptance to write: the block stalls whenever the tile
// store refuses the write (RASTER.TILESTORE's `wr_ready_o = !clear_valid_i`),
// and the texture path, when it exists, is variable by nature. The fast path
// being fixed is a property worth stating, not a contradiction of the ledger.
//
// ---------------------------------------------------------------------------
// THE DEPTH TEST — two modes, because the spec defines two
// ---------------------------------------------------------------------------
// spec/qformats.md §8 defines exactly ONE comparison: `pass ⟺ d_new > d_old`,
// strict, ties fail. The recipes add exactly one more state: `sky_backdrop`'s
// "Z-test off". There is no LESS, no EQUAL, no LEQUAL and no NEVER here,
// because no spec in this repository defines one, and a comparison enum
// invented in RTL would be law made in the wrong place. `ST_Z_TEST_EN` is
// therefore a bit and not a function code; if a future spec ratifies more
// comparisons it widens here and the contract records the amendment.
//
// The strictness is load-bearing and easy to get wrong in the safe-looking
// direction: `>=` would let a coplanar decal z-fight instead of losing, which
// is precisely why §8 says "decals use explicit bias" rather than a relaxed
// compare. `sky_backdrop` writes the far constant 0 and `STAR_DEPTH` is
// "sky-prefill far + 1" (stars §3), so a star at depth 1 beats the sky
// backdrop at 0 under a strict `>` — one LSB is the whole margin, and a
// `>=` would additionally let the sky overwrite itself.
//
// ---------------------------------------------------------------------------
// THE ALPHA TEST IS AN INDEX TEST
// ---------------------------------------------------------------------------
// stars §1 does not say "alpha < ref"; it says `star_disc_masked` is "CLUT8
// nearest, alpha-test index 0", and §3's bake law says "Index 0 transparent;
// intensity 1..63". The transparency of a CLUT8 texel is a property of its
// INDEX, before any palette lookup — testing the palette's alpha instead
// would sample a palette this machine deliberately never samples for
// transparency. So the test is `kill ⟺ texel_index == ATEST_REF`, with
// ATEST_REF 0 being the ratified case and the field existing so a recipe with
// a different sentinel index does not need new RTL.
//
// ---------------------------------------------------------------------------
// THE STENCIL, AND THE OP SET THAT IS DELIBERATELY MISSING
// ---------------------------------------------------------------------------
// Charter §8 gives the tile word an 8-bit stencil and §8's basics list "depth
// bias for terrain decals"; no spec in this repository defines a stencil
// function set, so this block defines a minimal one and says so: test
// {ALWAYS, EQUAL, NOTEQUAL, NEVER} over `(dst & mask) vs (ref & mask)`, and
// op {REPLACE, KEEP, INCR_SAT, DECR_SAT} applied when the fragment SURVIVES.
//
// There are NO separate stencil-fail / depth-fail ops, and that is a decision
// with a reason rather than a gap. RASTER.TILESTORE has no byte enables — its
// contract argues that at length — so a fragment that draws no colour but
// must still bump the stencil would have to write the WHOLE word back, i.e.
// spend a full write cycle to change one byte. That is a second recipe with
// its own cost, and no ratified recipe asks for it: the six above never use
// one. When something does, it is a contract amendment plus a write path, not
// a quiet extension of this enum.
//
// ---------------------------------------------------------------------------
// THE FRAGMENT STATE WORD (32 bits, layout defined HERE and in the contract)
// ---------------------------------------------------------------------------
// Every field is encoded so that **state == 0 is the plain opaque write**:
// depth test off, depth written, blend REPLACE, no alpha test, stencil ALWAYS
// + REPLACE, tag written from the packet. That is not cosmetic — it is what
// lets zhao_raster_tile_pipe's phase-4 flat-colour behaviour survive this
// block's arrival bit for bit, with the whole pre-existing directed and
// random suite unchanged. Hence the two "_DIS" (disable) bits: an enable bit
// would have made the zero state a no-op instead of a write.
//
//   [0]      Z_TEST_EN       1 = the §8 strict test; 0 = always pass
//   [1]      Z_WRITE_DIS     1 = keep the destination depth
//   [2]      Z_FORCE_FAR     1 = the depth WRITTEN is the far constant 0
//   [4:3]    BLEND           0 REPLACE, 1 ALPHA, 2 ADD, 3 ADD_MOD
//   [5]      SHADE_MOD       1 = src.rgb = unit_mul(texel.rgb, vertex.rgb)
//   [6]      ALPHA_MOD       1 = a = unit_mul(texel.a, vertex.a)
//   [7]      ATEST_EN        1 = kill ⟺ texel_index == ATEST_REF
//   [15:8]   ATEST_REF
//   [17:16]  STEN_FUNC       0 ALWAYS, 1 EQUAL, 2 NOTEQUAL, 3 NEVER
//   [19:18]  STEN_OP         0 REPLACE, 1 KEEP, 2 INCR_SAT, 3 DECR_SAT
//   [20]     TAG_WRITE_DIS   1 = keep the destination tag
//   [21]     TAG_FROM_TEXEL  1 = tag = {TAG_CHANNEL, texel_index[5:0]}
//   [23:22]  TAG_CHANNEL     the stars §1 effect channel; GLOW = 2'b01
//   [31:24]  STEN_MASK       masks EQUAL / NOTEQUAL only
//
// Conservative SystemVerilog subset only (charter §2). Depends on
// zhao_raster_blend. Lint: clean under `-Wall` (lint_raster_fragment).

module zhao_raster_fragment (
  input  logic clk,
  input  logic rst_n,

  // ---- shaded_candidates in (RASTER.EARLYZ's survivors) -----------------
  input  logic        frag_valid_i,
  output logic        frag_ready_o,
  input  logic [7:0]  frag_addr_i,       // {row[3:0], col[3:0]}
  input  logic [23:0] frag_depth_i,      // invw24, larger is closer
  input  logic [31:0] frag_state_i,      // the state word above
  input  logic [15:0] frag_src_id_i,
  input  logic [23:0] frag_vert_rgb_i,   // interpolated, lit, tinted, FOGGED
  input  logic [7:0]  frag_vert_a_i,     // unit8 (PART.SOFT's fade rides here)
  input  logic [7:0]  frag_tag_i,        // the constant-tag source
  input  logic [7:0]  frag_sten_ref_i,   // stencil reference AND REPLACE value
  // texture_samples — TEXTURE.TMU's output, presented with the fragment.
  // There is no sampler in this block; see the header.
  input  logic [23:0] frag_texel_rgb_i,
  input  logic [7:0]  frag_texel_a_i,
  input  logic [7:0]  frag_texel_idx_i,  // CLUT8 index; 0 is the masked one

  // ---- tile_read master: RASTER.TILESTORE port A ------------------------
  output logic        rd_valid_o,
  input  logic        rd_ready_i,
  output logic [7:0]  rd_addr_o,
  output logic [15:0] rd_src_id_o,
  input  logic        rd_valid_i,        // the store's fixed 1-cycle response
  input  logic [63:0] rd_data_i,

  // ---- tile_write master: RASTER.TILESTORE's write port -----------------
  output logic        wr_valid_o,
  input  logic        wr_ready_i,
  output logic [7:0]  wr_addr_o,
  output logic [63:0] wr_data_o,

  // ---- fragment_error ---------------------------------------------------
  // A one-cycle pulse when the tile store fails to present the response this
  // block's stage 1 is standing on. It is a PROTOCOL violation by the store,
  // not an arithmetic condition: a saturating additive blend is the recipe
  // working, never an error (spec/sky_and_beams.md §2). It should never fire,
  // and the tests assert that it never does.
  output logic        fragment_error_o,

  // ---- status -----------------------------------------------------------
  output logic        idle_o,            // no fragment anywhere in the pipe

  // ---- counters ---------------------------------------------------------
  output logic [31:0] covered_fragments_o,
  output logic [31:0] blended_fragments_o
);

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  localparam logic [1:0] BL_REPLACE  = 2'd0;
  localparam logic [1:0] STEN_ALWAYS = 2'd0;
  localparam logic [1:0] STEN_EQUAL  = 2'd1;
  localparam logic [1:0] STEN_NOTEQ  = 2'd2;
  localparam logic [1:0] OP_REPLACE  = 2'd0;
  localparam logic [1:0] OP_KEEP     = 2'd1;
  localparam logic [1:0] OP_INCR     = 2'd2;

  // ========================================================== stage 0 =====
  // The accepted candidate. Its read is issued from here.
  logic        s0_v_r;
  logic [7:0]  s0_addr_r;
  logic [23:0] s0_depth_r;
  logic [31:0] s0_state_r;
  logic [15:0] s0_src_r;
  logic [23:0] s0_vrgb_r;
  logic [7:0]  s0_va_r;
  logic [7:0]  s0_tag_r;
  logic [7:0]  s0_sref_r;
  logic [23:0] s0_trgb_r;
  logic [7:0]  s0_ta_r;
  logic [7:0]  s0_tidx_r;

  // ========================================================== stage 1 =====
  // The fragment whose destination word is on `rd_data_i` THIS cycle.
  logic        s1_v_r;
  logic [7:0]  s1_addr_r;
  logic [23:0] s1_depth_r;
  // Bits 5 and 6 -- shade-modulate and alpha-modulate -- are consumed at the
  // s0 -> s1 transfer now, not here, so they are legitimately dead in stage 1.
  // The word is still carried whole because it IS the protocol state word.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] s1_state_r;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [15:0] s1_src_r;
  // THE MODULATED SOURCE, not the raw lanes. See MOVING THE MODULATION below.
  logic [23:0] s1_src_rgb_r;
  logic [7:0]  s1_src_a_r;
  logic [7:0]  s1_tag_r;
  logic [7:0]  s1_sref_r;


  logic [7:0]  s1_tidx_r;

  // ================================================= stages 2 and 3 =======
  // THE RMW LOOP, SPLIT. reports/MHZArchitected, and the measurement at
  // 49ad539 that says why. The whole loop -- RAM read, tests, blend product,
  // accumulate, saturate, RAM write -- used to be ONE cycle: a 14.361 ns data
  // path that must fall under 7.95.
  //
  // Measured element by element from the worst path, which is how the cut
  // points were chosen rather than guessed:
  //
  //     RAM out -> rd_data_o        1.51      <-- F1 ends here
  //     Add0 (src - dst)            1.94
  //     mul_left select             0.97
  //     Mult0~mac (the DSP)         4.58      <-- F2 ends here
  //     Add2 accumulator chain      2.49
  //     Mux0 rail + out_o           1.63
  //     route to the RAM write      1.25      <-- F3 ends here
  //
  // A single cut at the product leaves the front at ~9.0 ns, still over. The
  // three-way split lands at ~1.5 / ~7.5 / ~5.4.
  //
  // NOTHING ABOUT THE ARITHMETIC MOVES. Same operands, same order, same
  // widths, same single rounding -- zhao_raster_blend is split into
  // _prod and _fin halves whose wrapper is bit-identical, so the formal proof
  // still targets shipping logic.
  //
  // STAGE 2 -- holds the captured destination, the tests' verdict, and the
  // finish-side selections that do not depend on the blend.
  logic        s2_v_r;
  logic [7:0]  s2_addr_r;
  logic        s2_live_r;
  logic [23:0] s2_dst_rgb_r;
  logic [23:0] s2_src_rgb_r;
  logic [7:0]  s2_src_a_r;
  logic [1:0]  s2_blend_r;
  logic [23:0] s2_depth_r;
  logic [7:0]  s2_tag_r;
  logic [7:0]  s2_sten_r;

  // STAGE 3 -- holds the three registered blend PRODUCTS and every bypass the
  // finish half needs. This is the register the DSP drives.
  logic        s3_v_r;
  logic [7:0]  s3_addr_r;
  logic        s3_live_r;
  logic signed [17:0] s3_prod_r_r, s3_prod_g_r, s3_prod_b_r;
  logic [23:0] s3_dst_rgb_r;
  logic [23:0] s3_src_rgb_r;
  logic [1:0]  s3_blend_r;
  logic [23:0] s3_depth_r;
  logic [7:0]  s3_tag_r;
  logic [7:0]  s3_sten_r;

  // ---- the state word, decoded (stage 1) ---------------------------------
  logic       st_z_test_en, st_z_write_dis, st_z_force_far;
  logic       st_atest_en;
  logic       st_tag_write_dis, st_tag_from_texel;
  logic [1:0] st_blend, st_sten_func, st_sten_op, st_tag_channel;
  logic [7:0] st_atest_ref, st_sten_mask;
  always_comb begin
    st_z_test_en      = s1_state_r[0];
    st_z_write_dis    = s1_state_r[1];
    st_z_force_far    = s1_state_r[2];
    st_blend          = s1_state_r[4:3];
    st_atest_en       = s1_state_r[7];
    st_atest_ref      = s1_state_r[15:8];
    st_sten_func      = s1_state_r[17:16];
    st_sten_op        = s1_state_r[19:18];
    st_tag_write_dis  = s1_state_r[20];
    st_tag_from_texel = s1_state_r[21];
    st_tag_channel    = s1_state_r[23:22];
    st_sten_mask      = s1_state_r[31:24];
  end

  // ---- the destination word (RASTER.TILESTORE's layout, charter §8) ------
  // [63:40] RGB   [39:32] effect tag   [31:8] depth   [7:0] stencil
  logic [7:0]  dst_r8, dst_g8, dst_b8, dst_tag, dst_sten;
  logic [23:0] dst_depth;
  always_comb begin
    dst_r8    = rd_data_i[63:56];
    dst_g8    = rd_data_i[55:48];
    dst_b8    = rd_data_i[47:40];
    dst_tag   = rd_data_i[39:32];
    dst_depth = rd_data_i[31:8];
    dst_sten  = rd_data_i[7:0];
  end

  // ---- SHADE: the one modulate the recipes name --------------------------
  // `beam_additive_fade`: "colour = tex.RGB × vertex.RGB" (sky_and_beams §2).
  // `sky_cloud_fade`: "a = tex.a × vertex.a" (§1.1). Both are `unit_mul`
  // (spec/qformats.md §3): (a·b + 128) >> 8, round-half-up, ONE rounding.
  // This is the entire shading stage — there is no lighting here (GEOM.
  // PROJECT), no fog (see the header) and no sampler (TEXTURE.TMU).
  function automatic logic [7:0] unit_mul(input logic [7:0] a, input logic [7:0] b);
    logic [15:0] p;
    p = {8'd0, a} * {8'd0, b};
    unit_mul = 8'((p + 16'd128) >> 8);
  endfunction

  // ---- MOVING THE MODULATION OFF THE CRITICAL PATH ------------------------
  // The shipped fit measured gpu_clk at 53.48 MHz, and ALL 400 worst setup
  // paths ran
  //
  //     s1_trgb_r / s1_vrgb_r  ->  ...  ->  zhao_raster_tilestore RAM datain
  //
  // because this stage held the RAW lanes and did TWO dependent multiplies in
  // one clock: `unit_mul` here, then `zhao_raster_blend`'s own product, then
  // rounding, accumulation, saturation and the 64-bit tile write.
  //
  // The modulation does not need to be here. It depends only on registers that
  // are all written together in the s0 -> s1 transfer -- including
  // `s1_state_r`, which is where `st_shade_mod` comes from -- so it can be
  // computed AT that transfer and its RESULT registered instead of its
  // operands. Stage 1 then holds `s1_src_rgb_r` and reads it directly, leaving
  // the blend as the only multiplier layer in the cone.
  //
  // NOTHING ABOUT THE ARITHMETIC MOVES. Same `unit_mul`, same operands, same
  // single rounding, same order, same widths, and NO added latency or pipeline
  // stage -- the transfer already existed and was only copying registers.
  //
  // ENFORCED-BY: tests/raster/raster_fragment_directed.cpp:main
  logic [7:0] src_r8, src_g8, src_b8, src_a;
  always_comb begin
    src_r8 = s1_src_rgb_r[23:16];
    src_g8 = s1_src_rgb_r[15:8];
    src_b8 = s1_src_rgb_r[7:0];
    src_a  = s1_src_a_r;
  end

  // ---- THE THREE TESTS ---------------------------------------------------
  logic atest_pass, sten_pass, ztest_pass, live;
  always_comb begin
    // The alpha test is an INDEX test (stars §1/§3; see the header).
    atest_pass = !st_atest_en || (s1_tidx_r != st_atest_ref);

    case (st_sten_func)
      STEN_EQUAL: sten_pass = ((dst_sten & st_sten_mask) == (s1_sref_r & st_sten_mask));
      STEN_NOTEQ: sten_pass = ((dst_sten & st_sten_mask) != (s1_sref_r & st_sten_mask));
      STEN_ALWAYS: sten_pass = 1'b1;
      default:    sten_pass = 1'b0;  // NEVER
    endcase

    // spec/qformats.md §8, quoted: pass ⟺ d_new > d_old. STRICT.
    ztest_pass = !st_z_test_en || (s1_depth_r > dst_depth);

    live = atest_pass && sten_pass && ztest_pass;
  end

  // ---- THE BLEND: one zhao_raster_blend per channel ----------------------
  // The arithmetic, its rounding and its rail live in that module, which is a
  // separate file for the same reason zhao_raster_quant is: tests/formal/
  // raster_fragment_blend.sby proves the SHIPPING blend, not a copy of it.
  // F2 -- the three products, from STAGE 2's registers. These feed the stage 3
  // registers, so the DSP has a clock to itself and its own routing.
  logic signed [17:0] prod_r_w, prod_g_w, prod_b_w;
  zhao_raster_blend_prod u_pr (.mode_i(s2_blend_r), .dst_i(s2_dst_rgb_r[23:16]),
                               .src_i(s2_src_rgb_r[23:16]), .a_i(s2_src_a_r),
                               .prod_o(prod_r_w));
  zhao_raster_blend_prod u_pg (.mode_i(s2_blend_r), .dst_i(s2_dst_rgb_r[15:8]),
                               .src_i(s2_src_rgb_r[15:8]), .a_i(s2_src_a_r),
                               .prod_o(prod_g_w));
  zhao_raster_blend_prod u_pb (.mode_i(s2_blend_r), .dst_i(s2_dst_rgb_r[7:0]),
                               .src_i(s2_src_rgb_r[7:0]), .a_i(s2_src_a_r),
                               .prod_o(prod_b_w));

  // F3 -- rounding, accumulator and rail, from STAGE 3's registers.
  logic [7:0] out_r8, out_g8, out_b8;
  zhao_raster_blend_fin u_fr (.mode_i(s3_blend_r), .dst_i(s3_dst_rgb_r[23:16]),
                              .src_i(s3_src_rgb_r[23:16]), .prod_i(s3_prod_r_r),
                              .out_o(out_r8));
  zhao_raster_blend_fin u_fg (.mode_i(s3_blend_r), .dst_i(s3_dst_rgb_r[15:8]),
                              .src_i(s3_src_rgb_r[15:8]), .prod_i(s3_prod_g_r),
                              .out_o(out_g8));
  zhao_raster_blend_fin u_fb (.mode_i(s3_blend_r), .dst_i(s3_dst_rgb_r[7:0]),
                              .src_i(s3_src_rgb_r[7:0]), .prod_i(s3_prod_b_r),
                              .out_o(out_b8));

  // ---- DEPTH, TAG AND STENCIL OUT ----------------------------------------
  // These depend only on the destination word and the fragment's own state,
  // never on the blend, so they are computed in F1 and REGISTERED -- which is
  // what keeps them off the stage-3 critical path entirely.
  logic [23:0] out_depth;
  logic [7:0]  out_tag, out_sten;
  always_comb begin
    // `sky_backdrop`'s "Z-write = far constant": the WRITTEN depth is 0, not
    // whatever the fragment interpolated.
    out_depth = st_z_write_dis ? dst_depth : (st_z_force_far ? 24'd0 : s1_depth_r);

    // stars §1's frozen convention: tag = (channel << 6) | strength, with
    // strength = the source texel's CLUT intensity (0..63).
    out_tag = st_tag_write_dis  ? dst_tag
            : st_tag_from_texel ? {st_tag_channel, s1_tidx_r[5:0]}
                                : s1_tag_r;

    case (st_sten_op)
      OP_KEEP:    out_sten = dst_sten;
      OP_INCR:    out_sten = (dst_sten == 8'd255) ? 8'd255 : (dst_sten + 8'd1);
      OP_REPLACE: out_sten = s1_sref_r;
      default:    out_sten = (dst_sten == 8'd0) ? 8'd0 : (dst_sten - 8'd1);  // DECR_SAT
    endcase
  end

  assign wr_data_o = {out_r8, out_g8, out_b8, s3_tag_r, s3_depth_r, s3_sten_r};
  assign wr_addr_o = s3_addr_r;

  // ---- flow control, and THE STALL THAT RE-ISSUES ITS OWN READ -----------
  // Stage 1 stands on `rd_data_i` COMBINATIONALLY — that is what collapses
  // read, test, blend and write into one cycle and buys the write-first
  // hazard immunity described in the header. But `rd_data_i` is NOT a value
  // that persists: RASTER.TILESTORE re-registers its RAM word every cycle
  // from whatever address the port is presented with, so a stage 1 that
  // simply froze would be standing on the next cycle's read, not its own.
  //
  // The fix is to make the stall re-issue the read it is waiting on. While
  // stage 1 cannot retire (the store refuses the write — its `wr_ready_o =
  // !clear_valid_i`), the read port is pointed back at stage 1's own address
  // instead of stage 0's. Nothing has been written yet, so every re-read
  // returns the identical destination word, and stage 1 is standing on a
  // fresh response in every cycle of the stall. Stage 0 does not advance
  // meanwhile, so no read is lost. The two uses of the port are mutually
  // exclusive by construction: `s1_hold` is exactly `!s1_retire`.
  // ENFORCED-BY: tests/raster/raster_fragment_directed.cpp:test_write_stall
  //
  // Hygiene: `wr_valid_o` is a function of registers and `rd_data_i` only —
  // never of `wr_ready_i`. `frag_ready_o` and `rd_valid_o` DO depend on
  // `wr_ready_i` and `rd_ready_i`, which are OTHER channels' readies and
  // therefore the permitted direction; RASTER.TILESTORE closes no loop back.
  //
  // WITH THE LOOP SPLIT, the write moves to stage 3 and each stage retires
  // into the next. Stage 1 still stands on `rd_data_i`, so it still re-issues
  // its own read while it cannot advance -- that hack is unchanged and is
  // still needed for exactly the reason above.
  //
  // THE SAME-ADDRESS HAZARD, and why the stall below costs nothing.
  // A write is now three stages behind its read, so a second fragment at the
  // same tile address could read a value an in-flight write has not committed.
  // `hazard` stalls stage 0 until the older fragment commits -- always
  // correct, whatever the traffic.
  //
  // In THIS composition it never fires, and that is a property of the caller,
  // not luck. reports/MHZArchitected: "RASTER.TILE_PIPE already refuses to
  // accept the next triangle until the fragment pipeline is completely empty,
  // and a triangle visits each covered pixel once." Verified in
  // zhao_raster_tile_pipe.sv: RS_WALK leaves only on `pipe_empty`. So no two
  // fragments in flight can share an address, the stall is unreachable, and
  // the initiation rate is one fragment per clock exactly as before.
  //
  // It is implemented anyway because "unreachable" is a claim about the
  // caller, and a block that is only correct when its caller behaves is a trap
  // for the next composition.
  // ENFORCED-BY: tests/raster/raster_fragment_directed.cpp:test_write_stall
  logic s1_retire, s1_hold, s0_to_s1;
  logic s2_retire, s3_retire, hazard;

  assign hazard = (s1_v_r && (s1_addr_r == s0_addr_r))
               || (s2_v_r && (s2_addr_r == s0_addr_r))
               || (s3_v_r && (s3_addr_r == s0_addr_r));

  assign wr_valid_o  = s3_v_r && s3_live_r;
  assign s3_retire   = !s3_v_r || !s3_live_r || wr_ready_i;
  assign s2_retire   = !s2_v_r || s3_retire;
  assign s1_retire   = !s1_v_r || s2_retire;
  assign s1_hold     = !s1_retire;
  assign rd_valid_o  = s1_hold || s0_v_r;
  assign rd_addr_o   = s1_hold ? s1_addr_r : s0_addr_r;
  assign rd_src_id_o = s1_hold ? s1_src_r  : s0_src_r;
  assign s0_to_s1    = !s1_hold && s0_v_r && rd_ready_i && !hazard;
  assign frag_ready_o = !s0_v_r || s0_to_s1;
  assign idle_o       = !s0_v_r && !s1_v_r && !s2_v_r && !s3_v_r;

  logic frag_acc;
  assign frag_acc = frag_valid_i && frag_ready_o;

  // A fragment stands in stage 1 but the store did not answer: the store
  // broke its own `latency: fixed:1`. Reported, never papered over.
  assign fragment_error_o = s1_v_r && !rd_valid_i;

  // ---- sequential --------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s0_v_r              <= 1'b0;
      s0_addr_r           <= 8'd0;
      s0_depth_r          <= 24'd0;
      s0_state_r          <= 32'd0;
      s0_src_r            <= 16'd0;
      s0_vrgb_r           <= 24'd0;
      s0_va_r             <= 8'd0;
      s0_tag_r            <= 8'd0;
      s0_sref_r           <= 8'd0;
      s0_trgb_r           <= 24'd0;
      s0_ta_r             <= 8'd0;
      s0_tidx_r           <= 8'd0;
      s1_v_r              <= 1'b0;
      s1_addr_r           <= 8'd0;
      s1_depth_r          <= 24'd0;
      s1_state_r          <= 32'd0;
      s1_src_r            <= 16'd0;
      s1_src_rgb_r        <= 24'd0;
      s1_src_a_r          <= 8'd0;
      s1_tag_r            <= 8'd0;
      s1_sref_r           <= 8'd0;
      s1_tidx_r           <= 8'd0;
      s2_v_r              <= 1'b0;
      s2_addr_r           <= 8'd0;
      s2_live_r           <= 1'b0;
      s2_dst_rgb_r        <= 24'd0;
      s2_src_rgb_r        <= 24'd0;
      s2_src_a_r          <= 8'd0;
      s2_blend_r          <= 2'd0;
      s2_depth_r          <= 24'd0;
      s2_tag_r            <= 8'd0;
      s2_sten_r           <= 8'd0;
      s3_v_r              <= 1'b0;
      s3_addr_r           <= 8'd0;
      s3_live_r           <= 1'b0;
      s3_prod_r_r         <= 18'sd0;
      s3_prod_g_r         <= 18'sd0;
      s3_prod_b_r         <= 18'sd0;
      s3_dst_rgb_r        <= 24'd0;
      s3_src_rgb_r        <= 24'd0;
      s3_blend_r          <= 2'd0;
      s3_depth_r          <= 24'd0;
      s3_tag_r            <= 8'd0;
      s3_sten_r           <= 8'd0;
      covered_fragments_o <= 32'd0;
      blended_fragments_o <= 32'd0;
    end else begin
      // ---- stage 3 retires (the write) ---------------------------------
      if (s3_retire) begin
        s3_v_r <= 1'b0;
        // `blended_fragments` counts fragments that SURVIVED and whose write
        // combined the source with the destination. A REPLACE write is not a
        // blend, and a fragment killed by any of the three tests is not one
        // either — it wrote nothing at all. Counted HERE now, because this is
        // where the write happens; counting it at stage 1 would count
        // fragments that had not yet committed.
        if (s3_v_r && s3_live_r && (s3_blend_r != BL_REPLACE)) begin
          if (blended_fragments_o != CNT_MAX) blended_fragments_o <= blended_fragments_o + 32'd1;
        end
      end

      // ---- stage 2 hands to stage 3: the PRODUCTS -----------------------
      if (s2_retire) s2_v_r <= 1'b0;
      if (s2_v_r && s2_retire) begin
        s3_v_r       <= 1'b1;
        s3_addr_r    <= s2_addr_r;
        s3_live_r    <= s2_live_r;
        s3_prod_r_r  <= prod_r_w;
        s3_prod_g_r  <= prod_g_w;
        s3_prod_b_r  <= prod_b_w;
        s3_dst_rgb_r <= s2_dst_rgb_r;
        s3_src_rgb_r <= s2_src_rgb_r;
        s3_blend_r   <= s2_blend_r;
        s3_depth_r   <= s2_depth_r;
        s3_tag_r     <= s2_tag_r;
        s3_sten_r    <= s2_sten_r;
      end

      // ---- stage 1 hands to stage 2: the CAPTURED DESTINATION -----------
      if (s1_retire) s1_v_r <= 1'b0;
      if (s1_v_r && s1_retire) begin
        s2_v_r       <= 1'b1;
        s2_addr_r    <= s1_addr_r;
        s2_live_r    <= live;
        s2_dst_rgb_r <= {dst_r8, dst_g8, dst_b8};
        s2_src_rgb_r <= {src_r8, src_g8, src_b8};
        s2_src_a_r   <= src_a;
        s2_blend_r   <= st_blend;
        s2_depth_r   <= out_depth;
        s2_tag_r     <= out_tag;
        s2_sten_r    <= out_sten;
      end

      // ---- stage 0 hands over ------------------------------------------
      if (s0_to_s1) begin
        s1_v_r     <= 1'b1;
        s1_addr_r  <= s0_addr_r;
        s1_depth_r <= s0_depth_r;
        s1_state_r <= s0_state_r;
        s1_src_r   <= s0_src_r;
        // The modulation happens HERE, from stage 0's lanes and stage 0's
        // state bits, so stage 1 holds the finished source colour.
        s1_src_rgb_r <= s0_state_r[5]
            ? {unit_mul(s0_trgb_r[23:16], s0_vrgb_r[23:16]),
               unit_mul(s0_trgb_r[15:8],  s0_vrgb_r[15:8]),
               unit_mul(s0_trgb_r[7:0],   s0_vrgb_r[7:0])}
            : s0_vrgb_r;
        s1_src_a_r   <= s0_state_r[6] ? unit_mul(s0_ta_r, s0_va_r) : s0_va_r;
        s1_tag_r   <= s0_tag_r;
        s1_sref_r  <= s0_sref_r;
        s1_tidx_r  <= s0_tidx_r;
        s0_v_r     <= 1'b0;
      end

      // ---- a new candidate is accepted ----------------------------------
      if (frag_acc) begin
        s0_v_r     <= 1'b1;
        s0_addr_r  <= frag_addr_i;
        s0_depth_r <= frag_depth_i;
        s0_state_r <= frag_state_i;
        s0_src_r   <= frag_src_id_i;
        s0_vrgb_r  <= frag_vert_rgb_i;
        s0_va_r    <= frag_vert_a_i;
        s0_tag_r   <= frag_tag_i;
        s0_sref_r  <= frag_sten_ref_i;
        s0_trgb_r  <= frag_texel_rgb_i;
        s0_ta_r    <= frag_texel_a_i;
        s0_tidx_r  <= frag_texel_idx_i;
        if (covered_fragments_o != CNT_MAX) covered_fragments_o <= covered_fragments_o + 32'd1;
      end
    end
  end

endmodule : zhao_raster_fragment
