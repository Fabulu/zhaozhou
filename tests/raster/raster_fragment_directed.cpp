// raster_fragment_directed.cpp — directed vectors for RASTER.FRAGMENT
// (fpga/rtl/raster/zhao_raster_fragment.sv; contract
// design/contracts/RASTER.FRAGMENT.md; ledger ZH-025; the charter 4 exemplar
// block).
//
// Every case drives the RTL and zref::FragmentPipeline through the identical
// fragment sequence against the identical destination tile and requires the
// ORDERED write list, the final tile and both counters to agree bit for bit.
// The ordered list matters as much as the tile does: a block that wrote the
// destination back unchanged instead of skipping a killed fragment's write
// would leave the tile identical, and only the list would catch it.
//
// On top of the differential each case asserts its own law:
//
//   1. depth test      — spec/qformats.md 8's `pass <=> d_new > d_old`, in
//                        BOTH modes the spec defines (strict, and off), with
//                        the tie pinned as a FAILURE and the one-LSB margin
//                        pinned as a pass
//   2. depth write     — on, off, and sky_backdrop's Z-forced-far; a fragment
//                        that passes with writes disabled must leave the
//                        destination depth EXACTLY as it found it
//   3. stencil         — all four test functions under a mask, and all four
//                        ops; plus the deliberate absence of fail/zfail ops
//   4. blend           — all four modes against hand-computable anchors,
//                        including the ADDITIVE SATURATION the beam and star
//                        recipes need, and the unit8 endpoint (a = 255 is
//                        255/256, not 1.0)
//   5. alpha test      — the star_disc_masked INDEX test at index 0, at a
//                        non-zero reference, and disabled
//   6. shade           — the beam's `tex.RGB x vertex.RGB` and the cloud's
//                        `a = tex.a x vertex.a`, both `unit_mul`
//   7. tag             — constant, from-texel (stars 1's `(channel<<6)|
//                        strength`), and write-disabled
//   8. fog             — NOT computed here, and the case that pins that
//   9. recipes         — all six ratified recipes, end to end
//  10. same-pixel RAW  — back-to-back fragments at ONE pixel, which is the
//                        hazard the tile store's write-first rule exists for
//  11. write stall     — the store refusing the write, which is the only
//                        stall the block has, and the re-issued read that
//                        makes it correct
//  12. counters        — covered_fragments and blended_fragments

#include "raster_fragment_dev.hpp"

#include "zref/zref_texture.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using zhao::check;
using zhao_raster::fr_describe;
using zhao_raster::fr_expect;
using zhao_raster::fr_fill;
using zhao_raster::fr_word;
using zhao_raster::FrDev;
using zhao_raster::FrExpect;
using zhao_raster::FrFrag;
using zhao_raster::FrRun;
using zhao_raster::FrState;
using zhao_raster::FrWord;
using zhao_raster::kFrWords;

using zref::FragmentPipeline;

namespace {

FrDev& dev() {
  static FrDev d;
  return d;
}

uint32_t g_saved = 0;

/** A fragment with every field distinct, so a field swap cannot cancel out. */
FrFrag mk(uint8_t addr, uint32_t state) {
  FrFrag f;
  f.addr = addr;
  f.state = state;
  f.depth = 0x400000u;
  f.vr = 0xC0;
  f.vg = 0x60;
  f.vb = 0x30;
  f.va = 0x80;
  f.tag = 0x5A;
  f.sten_ref = 0x33;
  f.tr = 0xF0;
  f.tg = 0x80;
  f.tb = 0x40;
  f.ta = 0x90;
  f.tidx = 0x2A;
  return f;
}

// ----------------------------------------------------------- the compare ---
bool run_seq(const uint64_t* tile, const std::vector<FrFrag>& frags, uint32_t in_seed,
             uint32_t wr_seed, const char* what, FrRun* got) {
  std::string err;
  *got = dev().run(tile, frags, in_seed, wr_seed, &err);
  bool ok = err.empty();
  if (!ok) std::printf("  %s: protocol violation: %s\n", what, err.c_str());

  const FrExpect want = fr_expect(tile, frags);

  if (want.writes.size() != got->writes.size()) {
    ok = false;
    std::printf("  %s: oracle wrote %zu times, rtl wrote %zu\n", what, want.writes.size(),
                got->writes.size());
  } else {
    for (size_t i = 0; i < want.writes.size(); ++i) {
      if (want.writes[i].addr == got->writes[i].addr && want.writes[i].data == got->writes[i].data)
        continue;
      ok = false;
      if (g_saved < 8) {
        const std::string body = fr_describe(i, want.writes[i].data, got->writes[i].data);
        std::printf("  %s: write %zu differs (addr %02X/%02X)\n    %s\n", what, i,
                    want.writes[i].addr, got->writes[i].addr, body.c_str());
        zhao::save_failing_vector(std::string("raster_fragment_") + what,
                                  zhao_raster::fr_serialize(frags), "zref::FragmentPipeline", body);
        ++g_saved;
      }
    }
  }

  for (int i = 0; i < kFrWords; ++i) {
    if (want.tile[i] == got->tile[i]) continue;
    ok = false;
    if (g_saved < 8) {
      const std::string body = fr_describe(static_cast<size_t>(i), want.tile[i], got->tile[i]);
      std::printf("  %s: final tile differs\n    %s\n", what, body.c_str());
      ++g_saved;
    }
  }

  if (got->blended != want.blended) {
    ok = false;
    std::printf("  %s: blended_fragments oracle %u rtl %u\n", what, want.blended, got->blended);
  }
  if (got->covered != frags.size()) {
    ok = false;
    std::printf("  %s: covered_fragments %u != %zu fragments offered\n", what, got->covered,
                frags.size());
  }
  if (got->error) {
    ok = false;
    std::printf("  %s: fragment_error_o fired\n", what);
  }
  return ok;
}

/** One fragment against one destination word: the hand-computable shape. */
uint64_t one(uint64_t dst, const FrFrag& f, bool* wrote) {
  uint64_t tile[kFrWords] = {};
  fr_fill(tile, dst);
  std::vector<FrFrag> v;
  v.push_back(f);
  FrRun got;
  const bool ok = run_seq(tile, v, 0u, 0u, "single", &got);
  if (!ok) std::printf("  single: differential failed for this vector\n");
  if (wrote) *wrote = !got.writes.empty();
  return got.tile[f.addr];
}

// --------------------------------------------------------------------- 1 ---
// spec/qformats.md 8 defines exactly ONE comparison — `pass <=> d_new > d_old`,
// strict, ties fail — and the recipes add exactly one more state, "Z-test
// off". Both are exercised here, and the tie is pinned as a FAILURE: a `>=`
// would let a coplanar decal z-fight instead of losing, which is precisely
// why 8 says "decals use explicit bias" rather than relaxing the compare.
void test_depth_test_modes() {
  const uint32_t D = 0x400000u;
  const uint64_t dst = fr_word(0x10, 0x20, 0x30, 0x11, D, 0x07);

  FrState on;
  on.z_test_en = true;
  FrState off;  // the sky_backdrop mode: test disabled

  struct Case {
    uint32_t depth;
    bool want_write;
    const char* what;
  };
  const Case cases[6] = {{D - 1u, false, "strictly behind the destination: killed"},
                         {D, false, "EXACTLY at the destination: killed (ties fail)"},
                         {D + 1u, true, "one LSB in front: passes - the STAR_DEPTH margin"},
                         {0u, false, "the far constant against a mid depth: killed"},
                         {0xFFFFFFu, true, "the nearest possible depth: passes"},
                         {D + 0x100000u, true, "well in front: passes"}};

  for (const Case& c : cases) {
    FrFrag f = mk(0x40, on.pack());
    f.depth = c.depth;
    bool wrote = false;
    one(dst, f, &wrote);
    check(wrote == c.want_write, c.what, c.want_write ? 1 : 0, wrote ? 1 : 0);
  }

  // The test OFF: every one of those depths must now write, including the
  // ones that just lost. sky_backdrop depends on exactly this.
  int wrote_n = 0;
  for (const Case& c : cases) {
    FrFrag f = mk(0x41, off.pack());
    f.depth = c.depth;
    bool wrote = false;
    one(dst, f, &wrote);
    if (wrote) ++wrote_n;
  }
  check(wrote_n == 6, "depth test off: every depth passes, including the ones that lost", 6,
        wrote_n);

  // And the destination really was at a depth that discriminates - otherwise
  // the six cases above would agree for a trivial reason.
  check(FrWord::unpack(dst).depth == D, "depth test: the destination depth is the pivot", D,
        FrWord::unpack(dst).depth);
}

// --------------------------------------------------------------------- 2 ---
// Depth WRITE on, off, and sky_backdrop's Z-forced-far. A fragment that
// passes with writes disabled must leave the destination depth exactly as it
// found it - every additive recipe in both specs is "Z-write OFF", and a
// beam that wrote depth would occlude the next beam.
void test_depth_write_modes() {
  const uint32_t D = 0x200000u;
  const uint64_t dst = fr_word(0x10, 0x20, 0x30, 0x11, D, 0x07);

  FrState wr;
  wr.z_test_en = true;
  FrFrag fw = mk(0x50, wr.pack());
  fw.depth = 0x600000u;
  check(FrWord::unpack(one(dst, fw, nullptr)).depth == 0x600000u,
        "depth write ON: the fragment's depth lands in the tile", 0x600000u,
        FrWord::unpack(one(dst, fw, nullptr)).depth);

  FrState nowr;
  nowr.z_test_en = true;
  nowr.z_write_dis = true;
  FrFrag fn = mk(0x51, nowr.pack());
  fn.depth = 0x600000u;
  const uint64_t rn = one(dst, fn, nullptr);
  check(FrWord::unpack(rn).depth == D,
        "depth write OFF: a PASSING fragment leaves the destination depth untouched", D,
        FrWord::unpack(rn).depth);
  check(FrWord::unpack(rn).r == 0xC0,
        "depth write OFF: ...but its colour still lands (it passed, after all)", 0xC0,
        FrWord::unpack(rn).r);

  // sky_and_beams 1.1: "Z-write = far constant". The depth WRITTEN is 0,
  // whatever the fragment carries.
  FrState ff;
  ff.z_force_far = true;
  FrFrag fz = mk(0x52, ff.pack());
  fz.depth = 0xFFFFFFu;
  check(FrWord::unpack(one(dst, fz, nullptr)).depth == 0,
        "Z-forced-far: the depth WRITTEN is the far constant, not the carried one", 0,
        FrWord::unpack(one(dst, fz, nullptr)).depth);

  // z_write_dis wins over z_force_far - the disable is the outer decision.
  FrState both;
  both.z_write_dis = true;
  both.z_force_far = true;
  FrFrag fb = mk(0x53, both.pack());
  fb.depth = 0xFFFFFFu;
  check(FrWord::unpack(one(dst, fb, nullptr)).depth == D,
        "Z-write disabled beats Z-force-far: nothing is written to depth at all", D,
        FrWord::unpack(one(dst, fb, nullptr)).depth);
}

// --------------------------------------------------------------------- 3 ---
// The stencil: four test functions under a mask, four ops. No spec in this
// repository defines a stencil function set, so the contract defines this one
// and this case is where it is pinned.
void test_stencil() {
  const uint64_t dst = fr_word(0x10, 0x20, 0x30, 0x11, 0x100000u, 0xA5);

  // ---- the four test functions, masked ---------------------------------
  struct TCase {
    uint8_t func;
    uint8_t ref;
    uint8_t mask;
    bool want;
    const char* what;
  };
  const TCase tests[8] = {
      {FragmentPipeline::kAlways, 0x00, 0xFF, true, "stencil ALWAYS passes regardless of the ref"},
      {FragmentPipeline::kNever, 0xA5, 0xFF, false, "stencil NEVER fails regardless of the ref"},
      {FragmentPipeline::kEqual, 0xA5, 0xFF, true, "stencil EQUAL passes on an exact match"},
      {FragmentPipeline::kEqual, 0xA4, 0xFF, false, "stencil EQUAL fails on a one-bit difference"},
      {FragmentPipeline::kEqual, 0xA4, 0xF0, true, "stencil EQUAL passes when the MASK hides it"},
      {FragmentPipeline::kNotEqual, 0xA5, 0xFF, false, "stencil NOTEQUAL fails on a match"},
      {FragmentPipeline::kNotEqual, 0xA4, 0xFF, true, "stencil NOTEQUAL passes on a difference"},
      {FragmentPipeline::kNotEqual, 0xA4, 0xF0, false,
       "stencil NOTEQUAL fails when the MASK hides the difference"}};

  for (const TCase& t : tests) {
    FrState s;
    s.sten_func = t.func;
    s.sten_mask = t.mask;
    s.sten_op = FragmentPipeline::kOpKeep;
    FrFrag f = mk(0x60, s.pack());
    f.sten_ref = t.ref;
    bool wrote = false;
    one(dst, f, &wrote);
    check(wrote == t.want, t.what, t.want ? 1 : 0, wrote ? 1 : 0);
  }

  // ---- the four ops, on a SURVIVING fragment ----------------------------
  struct OCase {
    uint8_t op;
    uint8_t dst_sten;
    uint8_t ref;
    uint8_t want;
    const char* what;
  };
  const OCase ops[6] = {
      {FragmentPipeline::kOpReplace, 0xA5, 0x33, 0x33, "stencil op REPLACE writes the reference"},
      {FragmentPipeline::kOpKeep, 0xA5, 0x33, 0xA5, "stencil op KEEP leaves the destination"},
      {FragmentPipeline::kOpIncrSat, 0xA5, 0x33, 0xA6, "stencil op INCR_SAT increments"},
      {FragmentPipeline::kOpIncrSat, 0xFF, 0x33, 0xFF, "stencil op INCR_SAT saturates at 255"},
      {FragmentPipeline::kOpDecrSat, 0xA5, 0x33, 0xA4, "stencil op DECR_SAT decrements"},
      {FragmentPipeline::kOpDecrSat, 0x00, 0x33, 0x00, "stencil op DECR_SAT saturates at 0"}};

  for (const OCase& o : ops) {
    FrState s;
    s.sten_op = o.op;
    FrFrag f = mk(0x61, s.pack());
    f.sten_ref = o.ref;
    const uint64_t d = fr_word(0x10, 0x20, 0x30, 0x11, 0x100000u, o.dst_sten);
    const uint8_t got = FrWord::unpack(one(d, f, nullptr)).stencil;
    check(got == o.want, o.what, o.want, got);
  }

  // The deliberate absence: a fragment that FAILS the stencil test writes
  // NOTHING, stencil included. There are no fail/zfail ops, because
  // RASTER.TILESTORE has no byte enables and a stencil-only pass would cost a
  // whole write cycle for a fragment that draws nothing.
  FrState fail;
  fail.sten_func = FragmentPipeline::kNever;
  fail.sten_op = FragmentPipeline::kOpIncrSat;
  FrFrag f = mk(0x62, fail.pack());
  bool wrote = false;
  const uint64_t r = one(dst, f, &wrote);
  check(!wrote && r == dst,
        "stencil: a fragment that fails the test writes NOTHING - there are no fail ops", 1,
        (!wrote && r == dst) ? 1 : 0);
}

// --------------------------------------------------------------------- 4 ---
// THE BLEND, against hand-computed anchors. `a` is a unit8, so its value is
// a/256 (spec/qformats.md 2) and the rounding is rescale_s(x, 8) with ONE
// rounding (3/4). Every number below is written out longhand.
void test_blend_modes() {
  // ---- REPLACE ----------------------------------------------------------
  {
    FrState s;  // blend REPLACE is 0
    FrFrag f = mk(0x70, s.pack());
    f.vr = 0x11;
    f.vg = 0x22;
    f.vb = 0x33;
    const FrWord w = FrWord::unpack(one(fr_word(0xAA, 0xBB, 0xCC, 0, 0, 0), f, nullptr));
    check(w.r == 0x11 && w.g == 0x22 && w.b == 0x33,
          "blend REPLACE: the source replaces the destination outright", 1,
          (w.r == 0x11 && w.g == 0x22 && w.b == 0x33) ? 1 : 0);
  }

  // ---- ALPHA: out = dst + rescale_s((src - dst)*a, 8) --------------------
  {
    FrState s;
    s.blend = FragmentPipeline::kAlpha;
    FrFrag f = mk(0x71, s.pack());
    f.vr = 200;
    f.vg = 0;
    f.vb = 100;
    f.va = 128;  // exactly a half, in unit8: 128/256
    // r: 100 + ((200-100)*128 + 128) >> 8 = 100 + (12800+128)>>8 = 100 + 50 = 150
    // g: 100 + ((0-100)*128 + 128) >>> 8 = 100 + floor((-12800+128)/256)
    //                                    = 100 + floor(-12672/256) = 100 - 50 = 50
    // b: 100 + ((100-100)*128 + 128) >> 8 = 100 + 0 = 100
    const FrWord w = FrWord::unpack(one(fr_word(100, 100, 100, 0, 0, 0), f, nullptr));
    check(w.r == 150, "blend ALPHA at a=128: 100 -> 200 lands on 150", 150, w.r);
    check(w.g == 50, "blend ALPHA at a=128: 100 -> 0 lands on 50", 50, w.g);
    check(w.b == 100, "blend ALPHA at a=128 with src == dst is the identity", 100, w.b);
  }
  {
    // THE unit8 ENDPOINT, pinned rather than worked around: a = 255 is
    // 255/256, not 1.0, so a "fully opaque" alpha blend of white over black
    // gives 254. That is exactly what the ratified fog mix does at f8 = 255.
    FrState s;
    s.blend = FragmentPipeline::kAlpha;
    FrFrag f = mk(0x72, s.pack());
    f.vr = 255;
    f.vg = 255;
    f.vb = 255;
    f.va = 255;
    const FrWord w = FrWord::unpack(one(fr_word(0, 0, 0, 0, 0, 0), f, nullptr));
    check(w.r == 254, "blend ALPHA: a=255 is 255/256, NOT 1.0 - white over black gives 254", 254,
          w.r);

    // a = 0 is the exact identity, which is the endpoint that must be exact.
    f.va = 0;
    const FrWord w0 = FrWord::unpack(one(fr_word(77, 88, 99, 0, 0, 0), f, nullptr));
    check(w0.r == 77 && w0.g == 88 && w0.b == 99,
          "blend ALPHA: a=0 is the EXACT identity - the destination is untouched", 1,
          (w0.r == 77 && w0.g == 88 && w0.b == 99) ? 1 : 0);
  }
  {
    // The negative half's rounding. rescale_s rounds ties toward +infinity,
    // so an exact half in a DARKENING lerp rounds toward zero. Splitting the
    // sign off and rescaling the magnitude unsigned would differ by one LSB.
    // dst=1, src=0, a=128: 1 + floor((-1*128 + 128)/256) = 1 + floor(0/256) = 1.
    // The unsigned-magnitude form would give 1 - ((128+128)>>8) = 1 - 1 = 0.
    FrState s;
    s.blend = FragmentPipeline::kAlpha;
    FrFrag f = mk(0x73, s.pack());
    f.vr = 0;
    f.vg = 0;
    f.vb = 0;
    f.va = 128;
    const FrWord w = FrWord::unpack(one(fr_word(1, 3, 5, 0, 0, 0), f, nullptr));
    check(w.r == 1, "blend ALPHA: the negative half rounds ties toward +infinity (1, not 0)", 1,
          w.r);
    check(w.g == 2, "blend ALPHA: dst 3 -> src 0 at a=128 gives 2", 2, w.g);
    check(w.b == 3, "blend ALPHA: dst 5 -> src 0 at a=128 gives 3", 3, w.b);
  }

  // ---- ADD, and THE SATURATION the beam and star recipes need -------------
  {
    FrState s;
    s.blend = FragmentPipeline::kAdd;
    FrFrag f = mk(0x74, s.pack());
    f.vr = 100;
    f.vg = 200;
    f.vb = 1;
    const FrWord w = FrWord::unpack(one(fr_word(50, 100, 254, 0, 0, 0), f, nullptr));
    check(w.r == 150, "blend ADD: 50 + 100 = 150", 150, w.r);
    check(w.g == 255, "blend ADD SATURATES: 100 + 200 rails at 255, it does not wrap to 44", 255,
          w.g);
    check(w.b == 255, "blend ADD: 254 + 1 lands exactly on the rail", 255, w.b);

    // The rail is the recipe, not an error: sky_and_beams 2's beams and
    // stars 1's halos are supposed to blow out. Full white plus full white
    // must be white, never black.
    f.vr = 255;
    f.vg = 255;
    f.vb = 255;
    const FrWord ww = FrWord::unpack(one(fr_word(255, 255, 255, 0, 0, 0), f, nullptr));
    check(ww.r == 255 && ww.g == 255 && ww.b == 255,
          "blend ADD: white + white is WHITE - the saturation never wraps", 1,
          (ww.r == 255 && ww.g == 255 && ww.b == 255) ? 1 : 0);

    // ADD ignores alpha entirely - `dst = sat(dst + src)` has no factor.
    f.vr = 40;
    f.vg = 40;
    f.vb = 40;
    f.va = 0;
    const FrWord wa = FrWord::unpack(one(fr_word(10, 10, 10, 0, 0, 0), f, nullptr));
    check(wa.r == 50, "blend ADD: alpha is not in the formula, so a=0 changes nothing", 50, wa.r);
  }

  // ---- ADD_MOD: dst = sat(dst + src*a) ----------------------------------
  {
    FrState s;
    s.blend = FragmentPipeline::kAddMod;
    FrFrag f = mk(0x75, s.pack());
    f.vr = 200;
    f.vg = 200;
    f.vb = 200;
    f.va = 128;
    // unit_mul(200,128) = (25600 + 128) >> 8 = 100. 50 + 100 = 150.
    const FrWord w = FrWord::unpack(one(fr_word(50, 200, 0, 0, 0, 0), f, nullptr));
    check(w.r == 150, "blend ADD_MOD: 50 + unit_mul(200,128)=100 gives 150", 150, w.r);
    check(w.g == 255, "blend ADD_MOD saturates too: 200 + 100 rails at 255", 255, w.g);
    check(w.b == 100, "blend ADD_MOD: 0 + 100 = 100", 100, w.b);

    // a = 0 makes it a no-op, which is what a fully faded sun must be.
    f.va = 0;
    const FrWord w0 = FrWord::unpack(one(fr_word(50, 60, 70, 0, 0, 0), f, nullptr));
    check(w0.r == 50 && w0.g == 60 && w0.b == 70, "blend ADD_MOD: a=0 adds exactly nothing", 1,
          (w0.r == 50 && w0.g == 60 && w0.b == 70) ? 1 : 0);
  }

  // Additive never DARKENS, in either additive mode. That is the property the
  // formal lane proves; here it is pinned on a sweep.
  bool never_darker = true;
  for (int mode = 2; mode <= 3; ++mode) {
    for (int d = 0; d < 256; d += 17) {
      FrState s;
      s.blend = static_cast<uint8_t>(mode);
      FrFrag f = mk(0x76, s.pack());
      f.vr = static_cast<uint8_t>((d * 7) & 0xFF);
      f.va = static_cast<uint8_t>((d * 3) & 0xFF);
      const FrWord w =
          FrWord::unpack(one(fr_word(static_cast<uint8_t>(d), 0, 0, 0, 0, 0), f, nullptr));
      if (w.r < static_cast<uint8_t>(d)) never_darker = false;
    }
  }
  check(never_darker, "blend: neither additive mode can ever DARKEN the destination", 1,
        never_darker ? 1 : 0);
}

// --------------------------------------------------------------------- 5 ---
// THE ALPHA TEST IS AN INDEX TEST. stars 1 says `star_disc_masked` is "CLUT8
// nearest, alpha-test index 0" and 3 says "Index 0 transparent; intensity
// 1..63". So the test reads the texel INDEX, not the texel alpha - a block
// that compared the alpha byte would pass every masked pixel of a star disc.
void test_alpha_test_index() {
  const uint64_t dst = fr_word(0x10, 0x20, 0x30, 0x11, 0, 0x07);

  FrState s;
  s.atest_en = true;
  s.atest_ref = 0;

  for (int idx = 0; idx < 4; ++idx) {
    FrFrag f = mk(0x80, s.pack());
    f.tidx = static_cast<uint8_t>(idx);
    f.ta = 0xFF;  // a FULLY OPAQUE alpha byte throughout: only the index decides
    bool wrote = false;
    one(dst, f, &wrote);
    const bool want = (idx != 0);
    check(wrote == want,
          idx == 0 ? "alpha test: CLUT index 0 is killed even with alpha 0xFF"
                   : "alpha test: a non-zero CLUT index survives",
          want ? 1 : 0, wrote ? 1 : 0);
  }

  // ...and the converse: alpha 0x00 at a non-zero index must SURVIVE. If the
  // block were testing the alpha byte this would be the case that dies.
  FrFrag f = mk(0x81, s.pack());
  f.tidx = 1;
  f.ta = 0x00;
  bool wrote = false;
  one(dst, f, &wrote);
  check(wrote, "alpha test: index 1 with alpha 0x00 SURVIVES - the INDEX is the test", 1,
        wrote ? 1 : 0);

  // A non-zero reference: the field exists so a recipe with another sentinel
  // needs no new RTL.
  FrState s2;
  s2.atest_en = true;
  s2.atest_ref = 0x2A;
  FrFrag f2 = mk(0x82, s2.pack());
  f2.tidx = 0x2A;
  one(dst, f2, &wrote);
  check(!wrote, "alpha test: the reference is a field, not a hard-wired 0", 0, wrote ? 1 : 0);
  f2.tidx = 0x00;
  one(dst, f2, &wrote);
  check(wrote, "alpha test: at reference 0x2A, index 0 now SURVIVES", 1, wrote ? 1 : 0);

  // Disabled: index 0 passes.
  FrState off;
  FrFrag f3 = mk(0x83, off.pack());
  f3.tidx = 0;
  one(dst, f3, &wrote);
  check(wrote, "alpha test disabled: index 0 passes", 1, wrote ? 1 : 0);
}

// --------------------------------------------------------------------- 6 ---
// SHADE. The two products the recipes name, both `unit_mul` (qformats 3).
void test_shade_modulate() {
  // beam_additive_fade: "colour = tex.RGB x vertex.RGB".
  FrState s;
  s.shade_mod = true;
  FrFrag f = mk(0x90, s.pack());
  f.vr = 128;
  f.vg = 255;
  f.vb = 0;
  f.tr = 128;
  f.tg = 128;
  f.tb = 255;
  // unit_mul(128,128) = (16384+128)>>8 = 64
  // unit_mul(128,255) = (32640+128)>>8 = 128
  // unit_mul(255,0)   = (0+128)>>8     = 0
  const FrWord w = FrWord::unpack(one(fr_word(9, 9, 9, 0, 0, 0), f, nullptr));
  check(w.r == 64, "shade: unit_mul(128,128) = 64", 64, w.r);
  check(w.g == 128, "shade: unit_mul(128,255) = 128", 128, w.g);
  check(w.b == 0, "shade: unit_mul(255,0) = 0", 0, w.b);

  // Off: the texel is ignored entirely.
  FrState off;
  FrFrag f2 = mk(0x91, off.pack());
  f2.vr = 77;
  f2.tr = 0;
  check(FrWord::unpack(one(fr_word(9, 9, 9, 0, 0, 0), f2, nullptr)).r == 77,
        "shade off: the vertex colour passes through and the texel is ignored", 77,
        FrWord::unpack(one(fr_word(9, 9, 9, 0, 0, 0), f2, nullptr)).r);

  // sky_cloud_fade: "a = tex.a x vertex.a", visible through an ALPHA blend.
  FrState c;
  c.blend = FragmentPipeline::kAlpha;
  c.alpha_mod = true;
  FrFrag f3 = mk(0x92, c.pack());
  f3.vr = 255;
  f3.va = 128;
  f3.ta = 128;
  // a = unit_mul(128,128) = 64; 0 + ((255-0)*64 + 128)>>8 = (16320+128)>>8 = 64
  check(FrWord::unpack(one(fr_word(0, 0, 0, 0, 0, 0), f3, nullptr)).r == 64,
        "shade: a = unit_mul(tex.a, vertex.a) = 64, so white over black lands on 64", 64,
        FrWord::unpack(one(fr_word(0, 0, 0, 0, 0, 0), f3, nullptr)).r);

  // Without the modulate the SAME fragment uses the vertex alpha alone.
  FrState c2;
  c2.blend = FragmentPipeline::kAlpha;
  FrFrag f4 = f3;
  f4.state = c2.pack();
  // a = 128; 0 + ((255)*128 + 128)>>8 = (32640+128)>>8 = 128
  check(FrWord::unpack(one(fr_word(0, 0, 0, 0, 0, 0), f4, nullptr)).r == 128,
        "shade: with ALPHA_MOD off the vertex alpha alone is the factor", 128,
        FrWord::unpack(one(fr_word(0, 0, 0, 0, 0, 0), f4, nullptr)).r);
}

// --------------------------------------------------------------------- 7 ---
// THE EFFECT TAG. stars 1's frozen convention is `tag = (channel << 6) |
// strength` with GLOW = 0b01 and strength = the source texel's CLUT
// intensity (0..63).
void test_effect_tag() {
  const uint64_t dst = fr_word(0x10, 0x20, 0x30, 0x99, 0, 0);

  // Constant: the packet's tag.
  FrState c;
  FrFrag f = mk(0xA0, c.pack());
  f.tag = 0x5A;
  check(FrWord::unpack(one(dst, f, nullptr)).tag == 0x5A,
        "tag: the constant mode writes the packet's tag byte", 0x5A,
        FrWord::unpack(one(dst, f, nullptr)).tag);

  // From the texel: (GLOW << 6) | (index & 63).
  FrState t;
  t.tag_from_texel = true;
  t.tag_channel = FragmentPipeline::kGlow;
  FrFrag f2 = mk(0xA1, t.pack());
  f2.tidx = 0x2A;  // 42, inside 0..63
  check(FrWord::unpack(one(dst, f2, nullptr)).tag == 0x6A,
        "tag: from-texel gives (GLOW<<6)|42 = 0x6A", 0x6A,
        FrWord::unpack(one(dst, f2, nullptr)).tag);

  // The strength field is SIX bits: an index above 63 keeps only its low six.
  f2.tidx = 0xFF;
  check(FrWord::unpack(one(dst, f2, nullptr)).tag == 0x7F,
        "tag: strength is six bits - index 0xFF gives (GLOW<<6)|63 = 0x7F", 0x7F,
        FrWord::unpack(one(dst, f2, nullptr)).tag);

  // Write disabled: the destination tag survives, which is what an effect
  // that must not claim the glow probe needs.
  FrState d;
  d.tag_write_dis = true;
  FrFrag f3 = mk(0xA2, d.pack());
  f3.tag = 0x11;
  check(FrWord::unpack(one(dst, f3, nullptr)).tag == 0x99,
        "tag: with writes disabled the DESTINATION tag survives", 0x99,
        FrWord::unpack(one(dst, f3, nullptr)).tag);
}

// --------------------------------------------------------------------- 8 ---
// FOG IS NOT COMPUTED HERE, AND THIS CASE PINS THAT.
//
// spec/qformats.md 8 (ratified 2026-08-17) makes fog a PER-VERTEX operation
// in GEOM.PROJECT: "there is no per-fragment fog anywhere in v1 (a per-pixel
// form would be a RASTER.FRAGMENT recipe change: not costed, not built)". The
// ledger's purpose line for this block still says "depth/stencil/blend/fog"
// and predates that amendment.
//
// So the fogged colour is computed HERE with the spec's own frozen mix and
// handed to the block as the vertex colour, and the block must store it
// UNALTERED - no second fog, no partial fog, nothing. A block that grew a fog
// stage would double-apply it and this case would go red. The exempt list
// (the sky family, additive emissive) is honoured by construction: a block
// that cannot fog cannot fog the wrong thing.
// ---------------------------------------------------------------------------
// THIS TEST ENFORCES A DECISION THAT HAS BEEN OVERRULED.
//
// It pins spec/qformats.md section 8's pre-D-5 law: fog is a per-vertex mix in
// GEOM.PROJECT, the colour arrives already fogged, and this block must pass it
// through UNALTERED. Owner ruling D-5 (2026-09-03) reversed that. Under D-5 the
// colour arrives UNFOGGED, a fog factor arrives as a separate interpolant, and
// fog is applied to the FINAL SOURCE COLOUR after material combination and
// before blending -- which is per-fragment work in this block's cone.
//
// So when D-5 is implemented THIS TEST IS EXPECTED TO GO RED, and the correct
// response is to rewrite it against the new ordering, NOT to revert the ruling
// to make it green again. A test that defends a superseded decision is the
// hardest kind to argue with, because it fails honestly.
//
// Left passing deliberately: the block still implements the old law, so the
// test still describes the machine. It is the CONTRACT that was wrong, and
// design/contracts/RASTER.FRAGMENT.md now says so. Costing and building the
// per-fragment mix is G4/G6 work.
// ---------------------------------------------------------------------------
void test_fog_is_a_vertex_operation() {
  // The frozen mix, qformats 8: `c' = sat_u8(c + rescale_s((fog_c - c)*f8, 8))`
  // - the identical shape as the blend's ALPHA lerp, which is why no new
  // arithmetic is needed anywhere to support it.
  auto fog_mix = [](int32_t c, int32_t fog_c, int32_t f8) {
    const int32_t v = c + ((((fog_c - c) * f8) + 128) >> 8);
    return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
  };

  const uint8_t fog_c[3] = {0x60, 0x70, 0x90};  // the sky's horizon colour
  const uint8_t lit[3] = {0xC0, 0x40, 0x20};

  bool all = true;
  for (int f8 = 0; f8 <= 255; f8 += 15) {
    FrState s;  // a plain opaque write: whatever arrives is what is stored
    FrFrag f = mk(0xB0, s.pack());
    f.vr = fog_mix(lit[0], fog_c[0], f8);
    f.vg = fog_mix(lit[1], fog_c[1], f8);
    f.vb = fog_mix(lit[2], fog_c[2], f8);
    const FrWord w = FrWord::unpack(one(fr_word(0x11, 0x22, 0x33, 0, 0, 0), f, nullptr));
    if (w.r != f.vr || w.g != f.vg || w.b != f.vb) all = false;
  }
  check(all,
        "fog: the per-vertex fogged colour reaches the tile UNALTERED - this block adds no fog", 1,
        all ? 1 : 0);

  // And the endpoints really do differ, so the sweep above is not agreeing
  // about a constant.
  check(
      fog_mix(lit[0], fog_c[0], 0) == lit[0] && fog_mix(lit[0], fog_c[0], 255) != lit[0],
      "fog: the vertex mix really does move the colour across the sweep", 1,
      (fog_mix(lit[0], fog_c[0], 0) == lit[0] && fog_mix(lit[0], fog_c[0], 255) != lit[0]) ? 1 : 0);
}

// --------------------------------------------------------------------- 9 ---
// THE SIX RATIFIED RECIPES, each against the destination it is drawn over.
void test_ratified_recipes() {
  const uint64_t dst = fr_word(0x40, 0x50, 0x60, 0x00, 0x100000u, 0x00);

  struct R {
    FragmentPipeline::State st;
    const char* name;
  };
  const R rs[6] = {{FragmentPipeline::sky_backdrop(), "sky_backdrop"},
                   {FragmentPipeline::sky_cloud_fade(), "sky_cloud_fade"},
                   {FragmentPipeline::sun_additive(), "sun_additive"},
                   {FragmentPipeline::beam_additive_fade(), "beam_additive_fade"},
                   {FragmentPipeline::star_disc_masked(), "star_disc_masked"},
                   {FragmentPipeline::star_halo_additive(), "star_halo_additive"}};

  std::vector<FrFrag> frags;
  for (int i = 0; i < 6; ++i) {
    FrFrag f = mk(static_cast<uint8_t>(0xC0 + i), rs[i].st.pack());
    f.depth = 0x300000u;  // in front of the destination, so nothing is z-killed
    frags.push_back(f);
  }
  uint64_t tile[kFrWords] = {};
  fr_fill(tile, dst);
  FrRun got;
  const bool ok = run_seq(tile, frags, 0u, 0u, "recipes", &got);
  check(ok, "recipes: all six match zref::FragmentPipeline", 1, ok ? 1 : 0);

  std::printf("raster_fragment recipes over %06llX:", static_cast<unsigned long long>(dst));
  for (int i = 0; i < 6; ++i) {
    const FrWord w = FrWord::unpack(got.tile[0xC0 + i]);
    std::printf(" %s=%02X%02X%02X/%02X/%06X", rs[i].name, w.r, w.g, w.b, w.tag, w.depth);
  }
  std::printf("\n");

  // sky_backdrop: depth forced to the far constant, tag written, colour
  // replaced, and it must survive with the depth test off even though its
  // carried depth loses to the destination.
  {
    FrFrag f = mk(0xD0, FragmentPipeline::sky_backdrop().pack());
    f.depth = 0;  // the far constant: this LOSES a strict test outright
    bool wrote = false;
    const FrWord w = FrWord::unpack(one(dst, f, &wrote));
    check(wrote, "sky_backdrop: survives over a nearer destination (Z-test off)", 1, wrote ? 1 : 0);
    check(w.depth == 0, "sky_backdrop: writes the far constant into depth", 0, w.depth);
    check(w.r == 0xC0 && w.tag == 0x5A,
          "sky_backdrop: blend off (colour replaced) and the effect tag initialised", 1,
          (w.r == 0xC0 && w.tag == 0x5A) ? 1 : 0);
  }

  // star_halo_additive over sky_backdrop, which is the layering both specs
  // describe: the halo adds into the backdrop, keeps its depth, and stamps a
  // glow tag whose strength is the CLUT intensity.
  {
    std::vector<FrFrag> two;
    FrFrag sky = mk(0xD1, FragmentPipeline::sky_backdrop().pack());
    sky.depth = 0;
    sky.vr = 0x08;  // deliberately low: red must show the ADD, not the rail
    sky.vg = 0x20;  // deliberately high: green must show the RAIL
    sky.vb = 0x40;
    sky.tag = 0x00;
    two.push_back(sky);
    FrFrag halo = mk(0xD1, FragmentPipeline::star_halo_additive().pack());
    halo.depth = 1;  // STAR_DEPTH = sky-prefill far + 1
    halo.vr = 0xF0;
    halo.vg = 0xE0;
    halo.vb = 0x80;
    halo.tidx = 0x3F;
    two.push_back(halo);

    uint64_t t2[kFrWords] = {};
    fr_fill(t2, dst);
    FrRun g2;
    const bool ok2 = run_seq(t2, two, 0u, 0u, "sky-then-halo", &g2);
    check(ok2, "layering: sky_backdrop then star_halo_additive matches the oracle", 1, ok2 ? 1 : 0);
    const FrWord w = FrWord::unpack(g2.tile[0xD1]);
    check(w.r == 0x08 + 0xF0, "layering: the halo ADDS into the backdrop (0x08 + 0xF0 = 0xF8)",
          0x08 + 0xF0, w.r);
    check(w.g == 255, "layering: 0x20 + 0xE0 = 0x100 saturates to 255", 255, w.g);
    check(w.depth == 0, "layering: the halo writes no depth, so the backdrop's 0 survives", 0,
          w.depth);
    check(w.tag == 0x7F, "layering: the glow tag is (GLOW<<6)|63, the texel's CLUT intensity", 0x7F,
          w.tag);
  }
}

// -------------------------------------------------------------------- 10 ---
// THE SAME-PIXEL READ-AFTER-WRITE. Back-to-back fragments at ONE address are
// the hazard a read-modify-write pipeline usually needs a forwarding path
// for. This block has none, and does not need one: its read for fragment N+1
// is issued in the very cycle fragment N's write is issued, and
// RASTER.TILESTORE's ordering rule 3 is write-first for exactly that reason
// ("a read-modify-write fragment pipeline must never see the pixel it just
// wrote as stale"). If that were wrong, an additive chain at one pixel would
// accumulate only the last term instead of all of them.
void test_same_pixel_raw() {
  FrState s;
  s.blend = FragmentPipeline::kAdd;
  s.z_write_dis = true;

  std::vector<FrFrag> frags;
  for (int i = 0; i < 16; ++i) {
    FrFrag f = mk(0xE0, s.pack());
    f.vr = 10;
    f.vg = 1;
    f.vb = 0;
    frags.push_back(f);
  }
  uint64_t tile[kFrWords] = {};
  fr_fill(tile, fr_word(0, 0, 0, 0, 0x100000u, 0));
  FrRun got;
  const bool ok = run_seq(tile, frags, 0u, 0u, "same-pixel", &got);
  check(ok, "same-pixel: the additive chain matches zref::FragmentPipeline", 1, ok ? 1 : 0);

  const FrWord w = FrWord::unpack(got.tile[0xE0]);
  check(w.r == 160, "same-pixel: 16 back-to-back adds of 10 accumulate to 160, not to 10", 160,
        w.r);
  check(w.g == 16, "same-pixel: ...and 16 adds of 1 accumulate to 16", 16, w.g);
  check(got.writes.size() == 16, "same-pixel: all sixteen fragments wrote", 16, got.writes.size());

  // The same chain under a stalling write port, where the block's re-issued
  // read is the thing keeping it correct.
  FrRun slow;
  const bool ok2 = run_seq(tile, frags, 0u, 0x4D2u, "same-pixel-stalled", &slow);
  check(ok2, "same-pixel: and under a stalling write port", 1, ok2 ? 1 : 0);
  check(slow.tile[0xE0] == got.tile[0xE0],
        "same-pixel: stalling costs cycles and changes not one bit", 1,
        (slow.tile[0xE0] == got.tile[0xE0]) ? 1 : 0);

  // A depth chain at one pixel: each fragment must see the previous one's
  // depth, so only the strictly-increasing ones survive.
  FrState z;
  z.z_test_en = true;
  std::vector<FrFrag> zf;
  const uint32_t depths[6] = {0x100u, 0x200u, 0x150u, 0x200u, 0x201u, 0x100u};
  for (int i = 0; i < 6; ++i) {
    FrFrag f = mk(0xE1, z.pack());
    f.depth = depths[i];
    f.vr = static_cast<uint8_t>(i + 1);
    zf.push_back(f);
  }
  uint64_t t2[kFrWords] = {};
  fr_fill(t2, fr_word(0, 0, 0, 0, 0, 0));
  FrRun gz;
  const bool ok3 = run_seq(t2, zf, 0u, 0u, "same-pixel-depth", &gz);
  check(ok3, "same-pixel: the depth chain matches the oracle", 1, ok3 ? 1 : 0);
  // 0x100 passes (over 0), 0x200 passes, 0x150 loses, 0x200 ties and loses,
  // 0x201 passes, 0x100 loses. Three writes, last colour 5.
  check(gz.writes.size() == 3, "same-pixel: exactly the three strictly-nearer fragments wrote", 3,
        gz.writes.size());
  check(FrWord::unpack(gz.tile[0xE1]).r == 5, "same-pixel: the last survivor is the fifth fragment",
        5, FrWord::unpack(gz.tile[0xE1]).r);
}

// -------------------------------------------------------------------- 11 ---
// THE WRITE STALL. RASTER.TILESTORE refuses a write in a cycle it is being
// cleared (`wr_ready_o = !clear_valid_i`), and that is the block's only
// stall. It is interesting because stage 1 stands on `rd_data_i`
// COMBINATIONALLY, and that signal does not persist - so the stall re-issues
// its own read every cycle. Six stall densities, all of which must give the
// identical tile.
void test_write_stall() {
  FrState s;
  s.z_test_en = true;
  s.blend = FragmentPipeline::kAlpha;

  std::vector<FrFrag> frags;
  for (int i = 0; i < 120; ++i) {
    FrFrag f = mk(static_cast<uint8_t>(i * 13), s.pack());
    f.depth = 0x1000u + static_cast<uint32_t>(i) * 0x137u;
    f.vr = static_cast<uint8_t>(i * 5);
    f.vg = static_cast<uint8_t>(i * 11);
    f.vb = static_cast<uint8_t>(i * 23);
    f.va = static_cast<uint8_t>(i * 7);
    frags.push_back(f);
  }

  uint64_t tile[kFrWords] = {};
  for (int i = 0; i < kFrWords; ++i)
    tile[i] = fr_word(static_cast<uint8_t>(i), static_cast<uint8_t>(255 - i),
                      static_cast<uint8_t>(i * 3), static_cast<uint8_t>(i),
                      static_cast<uint32_t>(i) * 0x101u, static_cast<uint8_t>(i * 5));

  const uint32_t seeds[6] = {0u, 0x11u, 0xBEEFu, 0x1234u, 0xFACEu, 0x7777u};
  FrRun base;
  bool all = true;
  uint32_t fastest = 0;
  uint32_t slowest = 0;
  for (int k = 0; k < 6; ++k) {
    FrRun got;
    if (!run_seq(tile, frags, (k & 1) ? 0x5A5Au : 0u, seeds[k], "stall", &got)) all = false;
    if (k == 0) {
      base = got;
      fastest = got.cycles;
      slowest = got.cycles;
    } else {
      for (int i = 0; i < kFrWords; ++i)
        if (base.tile[i] != got.tile[i]) all = false;
      if (base.writes.size() != got.writes.size()) all = false;
      if (got.cycles < fastest) fastest = got.cycles;
      if (got.cycles > slowest) slowest = got.cycles;
    }
  }
  check(all, "write stall: six stall densities give the identical tile, bit for bit", 1,
        all ? 1 : 0);
  std::printf("raster_fragment write stall: %zu fragments, %u..%u cycles\n", frags.size(), fastest,
              slowest);
  check(slowest > fastest, "write stall: the stalls really did cost cycles", 1,
        slowest > fastest ? 1 : 0);

  // The fast path really is one fragment per clock: 120 fragments must cost
  // 120 cycles plus the two-stage fill and a small constant, not 3 per
  // fragment.
  check(fastest <= frags.size() + 8u,
        "write stall: at full readiness the block sustains one fragment per clock",
        frags.size() + 8u, fastest);
}

// -------------------------------------------------------------------- 12 ---
void test_counters() {
  FrState blend_on;
  blend_on.blend = FragmentPipeline::kAdd;
  FrState replace;
  FrState killed;
  killed.sten_func = FragmentPipeline::kNever;
  killed.blend = FragmentPipeline::kAlpha;

  std::vector<FrFrag> frags;
  for (int i = 0; i < 30; ++i) frags.push_back(mk(static_cast<uint8_t>(i), blend_on.pack()));
  for (int i = 0; i < 20; ++i) frags.push_back(mk(static_cast<uint8_t>(i), replace.pack()));
  // These are BLENDED states that get killed, so they must NOT be counted:
  // `blended_fragments` counts fragments that wrote, not fragments that
  // wanted to.
  for (int i = 0; i < 25; ++i) frags.push_back(mk(static_cast<uint8_t>(i), killed.pack()));

  uint64_t tile[kFrWords] = {};
  fr_fill(tile, fr_word(1, 2, 3, 4, 5, 6));
  FrRun got;
  const bool ok = run_seq(tile, frags, 0x99u, 0x88u, "counters", &got);
  check(ok, "counters: the batch matches zref::FragmentPipeline", 1, ok ? 1 : 0);
  check(got.covered == 75, "counters: covered_fragments counts every ACCEPTED fragment", 75,
        got.covered);
  check(got.blended == 30,
        "counters: blended_fragments counts only fragments that WROTE with a real blend", 30,
        got.blended);
  check(got.writes.size() == 50, "counters: the 25 stencil-killed fragments wrote nothing", 50,
        got.writes.size());
  check(!got.error, "counters: fragment_error_o never fired", 1, got.error ? 0 : 1);
}

// -------------------------------------------------------------------- 14 ---
// THE TEXEL NOW COMES FROM TEXTURE.TMU, AND NOTHING IN THIS BLOCK CHANGED.
//
// This block's header and contract have said since phase 4 that
// `frag_texel_rgb_i` / `frag_texel_a_i` / `frag_texel_idx_i` are "the clean
// interface TEXTURE.TMU fills in when it lands ... Nothing here will need to
// change when it does - the texel arrives from a port instead of from a test
// driver." TEXTURE.TMU landed (ZH-027). This case cashes that claim: the three
// fields are filled by `zref::Tmu::sample` against a real CLUT8 star face and
// a real direct-colour beam ramp instead of by hand, and the RTL is driven
// through EXACTLY the ports it already had. Not one line of
// zhao_raster_fragment.sv moved, and this file's driver did not grow a port.
//
// The two recipes are the ones whose sampling the specs pin by name:
//   star_disc_masked   - spec/stars_and_flares.md 1, CLUT8 NEAREST, alpha-test
//                        index 0, glow tag with strength = the CLUT intensity.
//                        The recipe is unreachable without a RAW INDEX, and a
//                        sampler that returned only a colour could not serve
//                        it - which is why smp_idx_o exists.
//   beam_additive_fade - spec/sky_and_beams.md 2, direct colour with BILINEAR
//                        mandatory, colour = tex.RGB x vertex.RGB.
void test_texels_from_the_tmu() {
  // A tiny pool: an 8x8 CLUT8 star face whose byte k is k (so index 0 - the
  // transparent one - really occurs), a 256-entry RGB565 palette, and a 4x4
  // RGB565 beam ramp.
  zref::TextureMemory mem;
  mem.base = 0x0500'0000u;
  mem.bytes.assign(0x800, 0);
  auto put16 = [&mem](uint32_t off, uint16_t v) {
    mem.bytes[off] = static_cast<uint8_t>(v & 0xFFu);
    mem.bytes[off + 1] = static_cast<uint8_t>(v >> 8);
  };
  for (uint32_t k = 0; k < 64u; ++k) mem.bytes[k] = static_cast<uint8_t>(k);
  for (uint32_t i = 0; i < 256u; ++i)
    put16(0x100u + i * 2u, static_cast<uint16_t>((i << 8) | (i ^ 0x39u)));
  for (uint32_t k = 0; k < 16u; ++k)
    put16(0x400u + k * 2u, static_cast<uint16_t>(0x0821u * (k + 1u)));

  zref::Tmu::Mode star_mode;
  star_mode.fmt = zref::Tmu::kClut8;
  star_mode.log2w = 3;
  star_mode.log2h = 3;

  zref::Tmu::Mode beam_mode;
  beam_mode.fmt = zref::Tmu::kRgb565;
  beam_mode.bilinear = true;
  beam_mode.log2w = 2;
  beam_mode.log2h = 2;

  const uint32_t st_star = FragmentPipeline::star_disc_masked().pack();
  const uint32_t st_beam = FragmentPipeline::beam_additive_fade().pack();

  uint64_t tile[kFrWords] = {};
  FrWord dst;
  dst.r = 20;
  dst.g = 30;
  dst.b = 40;
  dst.depth = 0x100000u;
  fr_fill(tile, dst.pack());

  std::vector<FrFrag> frags;
  int expect_masked = 0;
  for (int i = 0; i < 8; ++i) {
    zref::Tmu::Req q;
    q.u = static_cast<int32_t>((static_cast<uint32_t>(i) << 16) / 8u);
    q.v = 0;
    q.base = mem.base;
    q.pal_base = mem.base + 0x100u;
    q.mode = star_mode.pack();
    const zref::Tmu::Sample s = zref::Tmu::sample(q, mem);
    if (s.idx == 0) ++expect_masked;

    FrFrag f;
    f.addr = static_cast<uint8_t>(i);
    f.depth = 0x400000u;
    f.state = st_star;
    f.vr = 200;
    f.vg = 180;
    f.vb = 160;
    f.va = 255;
    f.tr = s.r;
    f.tg = s.g;
    f.tb = s.b;
    f.ta = s.a;
    f.tidx = s.idx;
    frags.push_back(f);
  }
  for (int i = 0; i < 8; ++i) {
    zref::Tmu::Req q;
    // Fractional coordinates on purpose: the beam ramp is bilinear, so these
    // texels are FILTERED before the fragment modulates them.
    q.u = 9000 + i * 5000;
    q.v = 12000 + i * 3000;
    q.base = mem.base + 0x400u;
    q.pal_base = mem.base + 0x100u;
    q.mode = beam_mode.pack();
    const zref::Tmu::Sample s = zref::Tmu::sample(q, mem);

    FrFrag f;
    f.addr = static_cast<uint8_t>(0x40 + i);
    f.depth = 0x400000u;
    f.state = st_beam;
    f.vr = 255;
    f.vg = 128;
    f.vb = 64;
    f.va = 255;
    f.tr = s.r;
    f.tg = s.g;
    f.tb = s.b;
    f.ta = s.a;
    f.tidx = s.idx;
    frags.push_back(f);
  }

  FrRun got;
  const bool ok = run_seq(tile, frags, 0x51u, 0x62u, "tmu-texels", &got);
  check(ok, "TMU texels: the RTL matches zref::FragmentPipeline on TMU-sampled texels", 1,
        ok ? 1 : 0);
  check(expect_masked == 1, "TMU texels: exactly one of the eight star texels is CLUT index 0", 1,
        expect_masked);
  check(got.writes.size() == frags.size() - 1u,
        "TMU texels: the index-0 texel is alpha-tested away and every other fragment writes",
        frags.size() - 1u, got.writes.size());
  // The glow tag carries the TMU's RAW INDEX, not a palette colour - the one
  // thing only a sampler that reports its index can supply.
  bool tags_ok = true;
  for (int i = 1; i < 8; ++i) {
    const FrWord w = FrWord::unpack(got.tile[i]);
    const uint8_t want = static_cast<uint8_t>((FragmentPipeline::kGlow << 6) | (i & 63));
    if (w.tag != want) tags_ok = false;
  }
  check(tags_ok, "TMU texels: each star's glow strength is ITS OWN CLUT intensity", 1,
        tags_ok ? 1 : 0);
}

}  // namespace

int main() {
  test_depth_test_modes();
  test_depth_write_modes();
  test_stencil();
  test_blend_modes();
  test_alpha_test_index();
  test_shade_modulate();
  test_effect_tag();
  test_fog_is_a_vertex_operation();
  test_ratified_recipes();
  test_same_pixel_raw();
  test_write_stall();
  test_counters();
  test_texels_from_the_tmu();
  return zhao::report_and_exit("raster_fragment_directed");
}
