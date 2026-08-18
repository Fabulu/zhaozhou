// fragment.cpp — zref::FragmentPipeline, the RASTER.FRAGMENT oracle.
// Law and rationale: reference/include/zref/zref_fragment.hpp.

#include "zref/zref_fragment.hpp"

#include <cstdint>

namespace zref {

namespace {

uint8_t sat_u8(int32_t v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return static_cast<uint8_t>(v);
}

}  // namespace

uint8_t FragmentPipeline::blend_channel(uint8_t mode, uint8_t dst, uint8_t src, uint8_t a) {
  const int32_t d = static_cast<int32_t>(dst);
  const int32_t s = static_cast<int32_t>(src);
  switch (mode) {
    case kAlpha:
      // out = dst + rescale_s((src - dst) * a, 8), i.e. dst*(1-a) + src*a
      // with a = a/256. ONE rounding (spec/qformats.md 3's single-rounding
      // law): the product is exact and rescale_s8 is the only rounding.
      return sat_u8(d + rescale_s8((s - d) * static_cast<int32_t>(a)));
    case kAdd:
      // spec/sky_and_beams.md 2 and spec/stars_and_flares.md 1: the
      // saturation IS the recipe, not an overflow condition.
      return sat_u8(d + s);
    case kAddMod:
      // spec/sky_and_beams.md 1.1 sun_additive: dst = sat(dst + src*tex.a).
      // The product is the FROZEN unit8 multiply, not a local copy.
      return sat_u8(d + static_cast<int32_t>(unit_mul(unit8{src}, unit8{a})));
    default:
      return src;  // kReplace
  }
}

FragmentPipeline::Out FragmentPipeline::apply(const Frag& f, uint64_t dst_word) {
  const State st = State::unpack(f.state);
  const Word dst = Word::unpack(dst_word);
  Out out;

  // ---- shade: the one modulate the recipes name -------------------------
  // beam_additive_fade "colour = tex.RGB x vertex.RGB" (sky_and_beams 2) and
  // sky_cloud_fade "a = tex.a x vertex.a" (1.1). Both are zref::unit_mul.
  const uint8_t sr = st.shade_mod ? unit_mul(unit8{f.tr}, unit8{f.vr}) : f.vr;
  const uint8_t sg = st.shade_mod ? unit_mul(unit8{f.tg}, unit8{f.vg}) : f.vg;
  const uint8_t sb = st.shade_mod ? unit_mul(unit8{f.tb}, unit8{f.vb}) : f.vb;
  const uint8_t sa = st.alpha_mod ? unit_mul(unit8{f.ta}, unit8{f.va}) : f.va;

  // ---- the three tests --------------------------------------------------
  // The alpha test is an INDEX test: spec/stars_and_flares.md 1 says
  // "alpha-test index 0" and 3 says "Index 0 transparent; intensity 1..63".
  const bool atest_pass = !st.atest_en || (f.tidx != st.atest_ref);

  bool sten_pass = true;
  switch (st.sten_func) {
    case kEqual:
      sten_pass = (dst.stencil & st.sten_mask) == (f.sten_ref & st.sten_mask);
      break;
    case kNotEqual:
      sten_pass = (dst.stencil & st.sten_mask) != (f.sten_ref & st.sten_mask);
      break;
    case kNever:
      sten_pass = false;
      break;
    default:
      sten_pass = true;  // kAlways
      break;
  }

  // spec/qformats.md 8, quoted: "pass <=> d_new > d_old (strict; ties fail)".
  const bool ztest_pass = !st.z_test_en || (f.depth > dst.depth);

  out.write = atest_pass && sten_pass && ztest_pass;
  if (!out.write) return out;

  // ---- the surviving write ----------------------------------------------
  Word w;
  w.r = blend_channel(st.blend, dst.r, sr, sa);
  w.g = blend_channel(st.blend, dst.g, sg, sa);
  w.b = blend_channel(st.blend, dst.b, sb, sa);

  // sky_backdrop's "Z-write = far constant": the WRITTEN depth is the far
  // constant, whatever the fragment interpolated.
  w.depth = st.z_write_dis ? dst.depth : (st.z_force_far ? kDepthFar : (f.depth & 0xFFFFFFu));

  // stars 1's frozen convention: tag = (channel << 6) | strength, strength =
  // the source texel's CLUT intensity (0..63).
  w.tag = st.tag_write_dis
              ? dst.tag
              : (st.tag_from_texel ? static_cast<uint8_t>((st.tag_channel << 6) | (f.tidx & 0x3Fu))
                                   : f.tag);

  switch (st.sten_op) {
    case kOpKeep:
      w.stencil = dst.stencil;
      break;
    case kOpIncrSat:
      w.stencil = (dst.stencil == 255) ? 255 : static_cast<uint8_t>(dst.stencil + 1);
      break;
    case kOpDecrSat:
      w.stencil = (dst.stencil == 0) ? 0 : static_cast<uint8_t>(dst.stencil - 1);
      break;
    default:
      w.stencil = f.sten_ref;  // kOpReplace
      break;
  }

  out.word = w.pack();
  out.blended = (st.blend != kReplace);
  return out;
}

// ---- the six ratified recipes ---------------------------------------------

FragmentPipeline::State FragmentPipeline::sky_backdrop() {
  // sky_and_beams 1.1 pass 1: "Z-test off, Z-write far, blend off,
  // effect-tag init".
  State s;
  s.z_test_en = false;
  s.z_write_dis = false;
  s.z_force_far = true;
  s.blend = kReplace;
  s.sten_op = kOpKeep;  // the backdrop owns colour/tag/depth, not the stencil
  return s;
}

FragmentPipeline::State FragmentPipeline::sky_cloud_fade() {
  // sky_and_beams 1.1 layer `sky_`: "Z-test on, Z-write off, alpha blend
  // out = dst*(1-a)+src*a, a = tex.a x vertex.a".
  State s;
  s.z_test_en = true;
  s.z_write_dis = true;
  s.blend = kAlpha;
  s.alpha_mod = true;
  s.tag_write_dis = true;  // the cloud sheet writes no effect tag
  s.sten_op = kOpKeep;
  return s;
}

FragmentPipeline::State FragmentPipeline::sun_additive() {
  // sky_and_beams 1.1 layer `sun_`: "Z-test on, Z-write off, additive
  // dst = sat(dst + src*tex.a), glow effect-tag write on".
  //
  // The tag is CONSTANT here, not taken from a texel index, and that is a
  // real distinction rather than an oversight: the sun quad's texture is
  // "64x64 ARGB4444" (1.1) — direct colour, with no CLUT index to read a
  // strength out of. Only the star recipes sample CLUT8, and only they can
  // honour stars 1's "strength = source texel's CLUT intensity". The sun's
  // glow tag therefore rides the packet's constant `tag` field.
  State s;
  s.z_test_en = true;
  s.z_write_dis = true;
  s.blend = kAddMod;
  s.tag_channel = kGlow;
  s.sten_op = kOpKeep;
  return s;
}

FragmentPipeline::State FragmentPipeline::beam_additive_fade() {
  // sky_and_beams 2: "colour = tex.RGB x vertex.RGB; dst = sat(dst + src)",
  // depth test ON, depth write OFF ("beams never occlude anything").
  State s;
  s.z_test_en = true;
  s.z_write_dis = true;
  s.blend = kAdd;
  s.shade_mod = true;
  s.tag_write_dis = true;  // a beam is not a glow-probe source
  s.sten_op = kOpKeep;
  return s;
}

FragmentPipeline::State FragmentPipeline::star_disc_masked() {
  // stars 1: "CLUT8 nearest, alpha-test index 0, Z-test on / Z-write off,
  // glow-tag write with strength = the texel's CLUT intensity".
  State s;
  s.z_test_en = true;
  s.z_write_dis = true;
  s.blend = kReplace;
  s.atest_en = true;
  s.atest_ref = 0;
  s.tag_from_texel = true;
  s.tag_channel = kGlow;
  s.sten_op = kOpKeep;
  return s;
}

FragmentPipeline::State FragmentPipeline::star_halo_additive() {
  // stars 1: "same sampling, dst = sat(dst+src)". No mask: 4 says pal_h[0]
  // is black, which is the additive identity.
  State s;
  s.z_test_en = true;
  s.z_write_dis = true;
  s.blend = kAdd;
  s.tag_from_texel = true;
  s.tag_channel = kGlow;
  s.sten_op = kOpKeep;
  return s;
}

}  // namespace zref
