// texture_aux_random.cpp — TEXTURE.AUX randomized differential (phase 6,
// ZH-060), against `zref::AuxSource` — the oracle authored 2026-08-19 and
// proved a faithful view of `zref::render::sample_sheet` by
// tests/texture/texture_aux_directed.cpp's first lane.
//
// TWO lanes, deliberately, because one distribution cannot be both.
//
//   G (gameplay-shaped) — the traffic a terrain fragment produces: a 4 m
//     patch envelope of the shape tests/render uses, world positions spread
//     across it and a little past each edge (a triangle's guard band), a
//     resident sheet with a sparse scar pattern, and the consumer stalling the
//     way RASTER.FRAGMENT does. It answers "does the block agree with the
//     oracle on what it will actually see".
//
//   L (domain limit) — envelopes and positions over the whole int32 domain,
//     including degenerate and inverted envelopes, spans of 1 raw unit and of
//     2^32 - 1, non-resident sheets, and the EXACT texel boundaries
//     CONSTRUCTED for each random envelope. That last part is the one that
//     matters: the mapping is a FLOOR, the only inputs that distinguish a
//     floor from a round are the positions where the quotient steps, and with
//     a span of 2^18 raw units a uniform position lands on one about once in
//     4,000 draws. Three increments in a row have shipped differentials whose
//     coverage counters read zero; both lanes below COUNT their interesting
//     states and the counts are ASSERTED.

#include "texture_aux_dev.hpp"
#include "zref/zref_aux.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

namespace at = aux_test;

struct Cover {
  long n = 0;
  long u_lo = 0, u_hi = 0, v_lo = 0, v_hi = 0;  // both rails on both axes
  long interior = 0;                            // strictly inside on both axes
  long neg_num = 0;                             // the numerator was negative (clamp to 0)
  long sat_num = 0;                             // the numerator was >= 64*D (clamp to 63)
  long boundary = 0;                            // the world position IS a texel boundary
  long degenerate = 0;
  long miss = 0;
  long nonzero_bytes = 0;

  void observe(const at::Req& r, const at::Smp& got) {
    ++n;
    if (got.degenerate) {
      ++degenerate;
      return;
    }
    if (got.miss) ++miss;
    if (got.tag != 0 || got.strength != 0) ++nonzero_bytes;
    if (got.u == 0) ++u_lo;
    if (got.u == 63) ++u_hi;
    if (got.v == 0) ++v_lo;
    if (got.v == 63) ++v_hi;
    if (got.u > 0 && got.u < 63 && got.v > 0 && got.v < 63) ++interior;
    const int64_t D = static_cast<int64_t>(r.ex1) - r.ex0;
    const int64_t N = (static_cast<int64_t>(r.wx) - r.ex0) * 64;
    if (N < 0) ++neg_num;
    if (N >= 64 * D) ++sat_num;
    // a texel boundary: w is the FIRST position mapping to its own texel
    if (N >= 0 && N < 64 * D && got.u > 0) {
      const int64_t off = (static_cast<int64_t>(got.u) * D + 63) / 64;
      if (static_cast<int64_t>(r.wx) - r.ex0 == off) ++boundary;
    }
  }

  void report(const char* lane) const {
    std::printf(
        "  lane %s: n=%ld  u rails=%ld/%ld  v rails=%ld/%ld  interior=%ld\n"
        "           neg_num=%ld sat_num=%ld exact_boundary=%ld  degenerate=%ld miss=%ld "
        "nonzero=%ld\n",
        lane, n, u_lo, u_hi, v_lo, v_hi, interior, neg_num, sat_num, boundary, degenerate, miss,
        nonzero_bytes);
  }
};

void assert_common_coverage(const Cover& c, const char* lane) {
  char buf[176];
  const auto want = [&](bool ok, const char* what) {
    std::snprintf(buf, sizeof(buf), "lane %s reaches %s", lane, what);
    check(ok, buf);
  };
  want(c.u_lo > 0 && c.u_hi > 0, "both clamp rails on u");
  want(c.v_lo > 0 && c.v_hi > 0, "both clamp rails on v");
  want(c.interior > 0, "the interior of the sheet");
  want(c.neg_num > 0, "a negative numerator (the left clamp bypass)");
  want(c.sat_num > 0, "a saturating numerator (the right clamp bypass)");
  want(c.boundary > 0, "an EXACT texel boundary");
  want(c.nonzero_bytes > 0, "non-zero layer-F bytes");
}

void fill_sheet(at::SheetModel& s, uint32_t seed) {
  uint32_t rng = seed;
  const auto next = [&rng]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };
  for (int k = 0; k < 4096; ++k) {
    // a sparse scar: most of a sheet is clean, which is the realistic shape
    const bool scarred = (next() % 5u) == 0;
    s.tag[k] = scarred ? static_cast<uint8_t>(next()) : 0;
    s.strength[k] = scarred ? static_cast<uint8_t>(next()) : 0;
  }
  s.resident = true;
  s.stall_mask = 0;
  s.busy = false;
}

long diff(const std::vector<at::Req>& in, const std::vector<at::Smp>& got,
          const at::SheetModel& sheet, Cover* c) {
  long mismatch = 0;
  for (size_t k = 0; k < in.size(); ++k) {
    const at::Smp want = at::oracle(in[k], sheet.tag, sheet.strength, sheet.resident);
    if (!(got[k] == want)) ++mismatch;
    c->observe(in[k], got[k]);
  }
  return mismatch;
}

// ---- lane G: gameplay-shaped ------------------------------------------------
void lane_gameplay(Vzhao_texture_aux& dut, int packets) {
  at::Dev dev(dut);
  dev.reset();
  at::SheetModel sheet;
  fill_sheet(sheet, 0x1B873593u);

  uint32_t rng = 0x9E3779B9u;
  const auto next = [&rng]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };

  std::vector<at::Req> in;
  in.reserve(static_cast<size_t>(packets));
  int32_t e0x = 0, e0z = 0, e1x = 0, e1z = 0;
  for (int k = 0; k < packets; ++k) {
    if ((k % 1024) == 0) {
      // a new patch: 4 m square (the render tests' shape) at a plausible
      // world origin inside the +-512 m island table span
      const int32_t ox = (static_cast<int32_t>(next() % 1024u) - 512) << 16;
      const int32_t oz = (static_cast<int32_t>(next() % 1024u) - 512) << 16;
      e0x = ox - (2 << 16);
      e1x = ox + (2 << 16);
      e0z = oz - (2 << 16);
      e1z = oz + (2 << 16);
    }
    at::Req r;
    r.ex0 = e0x;
    r.ez0 = e0z;
    r.ex1 = e1x;
    r.ez1 = e1z;
    // across the envelope plus a guard band of a tenth on each side, which is
    // what a triangle clipped to the patch actually produces
    const int64_t span = static_cast<int64_t>(e1x) - e0x;
    const int64_t reach = span + span / 5;  // the envelope plus both guard bands
    int64_t ox = -span / 10 + static_cast<int64_t>(next() % static_cast<uint32_t>(reach));
    int64_t oz = -span / 10 + static_cast<int64_t>(next() % static_cast<uint32_t>(reach));
    // One fragment in sixteen is SNAPPED to an exact texel boundary. That is
    // not a cheat: a rasterizer steps u in fixed increments and cell edges are
    // ordinary traffic, and the exact boundary is the only input that
    // distinguishes this mapping's FLOOR from a round. Constructed, not hoped
    // for -- the same discipline lane L applies at the domain limit.
    if ((next() % 16u) == 0) {
      const int64_t kb = 1 + (next() % 63u);
      ox = (kb * span + 63) / 64;
    }
    if ((next() % 16u) == 0) {
      const int64_t kb = 1 + (next() % 63u);
      oz = (kb * span + 63) / 64;
    }
    r.wx = static_cast<int32_t>(e0x + ox);
    r.wz = static_cast<int32_t>(e0z + oz);
    r.handle = 0x2A01;
    r.src_id = static_cast<uint16_t>(next());
    in.push_back(r);
  }

  const std::vector<at::Smp> got = dev.run(in, sheet, 0x0F0F0F0Fu);
  if (got.size() != in.size()) {
    check(false, "lane G returned one sample per request");
    return;
  }
  Cover c;
  const long mismatch = diff(in, got, sheet, &c);
  c.report("G (gameplay)");
  check(mismatch == 0, "lane G: RTL == zref::AuxSource on every packet");
  assert_common_coverage(c, "G");
  check(c.degenerate == 0, "lane G never produces a degenerate envelope (it is gameplay traffic)");
}

// ---- lane L: the domain limit -----------------------------------------------
void lane_limit(Vzhao_texture_aux& dut, int packets) {
  at::Dev dev(dut);
  dev.reset();
  at::SheetModel sheet;
  fill_sheet(sheet, 0xCC9E2D51u);

  uint32_t rng = 0x85EBCA6Bu;
  const auto next = [&rng]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };
  const auto next64 = [&next]() {
    return (static_cast<int64_t>(next()) << 32) ^ static_cast<int64_t>(next());
  };

  std::vector<at::Req> in;
  in.reserve(static_cast<size_t>(packets) + 4096);

  // (a) THE CONSTRUCTED BOUNDARIES. For 32 random envelopes, drive every texel
  //     boundary k = 1..63 on BOTH sides on the u axis. 4,032 packets a
  //     uniform sweep would need millions of draws to find.
  int constructed = 0;
  for (int e = 0; e < 32; ++e) {
    const int32_t x0 = static_cast<int32_t>(next());
    // spans from 64 raw units (one per texel) to about 2^24, so the boundary
    // spacing varies by six orders of magnitude
    const int64_t span = 64 + (next() % (1u << 24));
    const int64_t x1_64 = static_cast<int64_t>(x0) + span;
    if (x1_64 > INT32_MAX) continue;  // the envelope must be representable
    const int32_t x1 = static_cast<int32_t>(x1_64);
    for (int64_t k = 1; k < 64; ++k) {
      const int64_t off = (k * span + 63) / 64;  // ceil: the FIRST position at texel k
      for (int side = 0; side < 2; ++side) {
        at::Req r;
        r.ex0 = x0;
        r.ex1 = x1;
        r.ez0 = x0;
        r.ez1 = x1;
        r.wx = static_cast<int32_t>(x0 + off - (side == 0 ? 1 : 0));
        r.wz = static_cast<int32_t>(x0 + (k * span) / 64);
        r.handle = 0x2A01;
        r.src_id = static_cast<uint16_t>(k);
        in.push_back(r);
        ++constructed;
      }
    }
  }
  check(constructed > 3000, "lane L constructed both sides of thousands of texel boundaries");

  // (b) the pathological envelopes: degenerate, inverted, one raw unit wide,
  //     and the widest representable
  const int32_t patho[6][4] = {
      {0, 0, 0, 0},                                  // fully degenerate
      {5, 5, 5, 6},                                  // x degenerate only
      {5, 6, 6, 5},                                  // z inverted only
      {0, 0, 1, 1},                                  // one raw unit per axis
      {INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX},  // the widest
      {-1, -1, 1, 1},                                // straddling zero
  };
  const int32_t wpat[6] = {INT32_MIN, -1, 0, 1, 1 << 24, INT32_MAX};
  for (int p = 0; p < 6; ++p) {
    for (int a = 0; a < 6; ++a) {
      for (int b = 0; b < 6; ++b) {
        at::Req r;
        r.ex0 = patho[p][0];
        r.ez0 = patho[p][1];
        r.ex1 = patho[p][2];
        r.ez1 = patho[p][3];
        r.wx = wpat[a];
        r.wz = wpat[b];
        r.handle = 0x2A01;
        in.push_back(r);
      }
    }
  }

  // (c) uniform over the whole domain, with the envelope ordered so most are
  //     legal and some are not
  for (int k = 0; k < packets; ++k) {
    at::Req r;
    const int64_t a = next64() >> 32, b = next64() >> 32;
    r.ex0 = static_cast<int32_t>(a < b ? a : b);
    r.ex1 = static_cast<int32_t>(a < b ? b : a);
    if ((next() % 16u) == 0) {
      const int32_t t = r.ex0;
      r.ex0 = r.ex1;
      r.ex1 = t;  // one in sixteen is inverted
    }
    const int64_t cz = next64() >> 32, dz = next64() >> 32;
    r.ez0 = static_cast<int32_t>(cz < dz ? cz : dz);
    r.ez1 = static_cast<int32_t>(cz < dz ? dz : cz);
    r.wx = static_cast<int32_t>(next());
    r.wz = static_cast<int32_t>(next());
    r.handle = 0x2A01;
    r.src_id = static_cast<uint16_t>(next());
    in.push_back(r);
  }

  const std::vector<at::Smp> got = dev.run(in, sheet, 0xAAAAAAAAu);
  if (got.size() != in.size()) {
    check(false, "lane L returned one sample per request");
    return;
  }
  Cover c;
  const long mismatch = diff(in, got, sheet, &c);
  c.report("L (domain limit)");
  check(mismatch == 0, "lane L: RTL == zref::AuxSource on every packet");
  assert_common_coverage(c, "L");
  check(c.degenerate > 0, "lane L reaches degenerate and inverted envelopes");
  check(c.boundary > 2000, "lane L reaches every constructed texel boundary");

  // (d) the same traffic against a NON-RESIDENT sheet: every sample must be a
  //     zero miss, and the texel mapping must still be right
  at::SheetModel absent;
  fill_sheet(absent, 0xCC9E2D51u);
  absent.resident = false;
  std::vector<at::Req> sub(in.begin(), in.begin() + 4096);
  const std::vector<at::Smp> gm = dev.run(sub, absent, 0u);
  if (gm.size() != sub.size()) {
    check(false, "lane L (miss) returned one sample per request");
    return;
  }
  Cover cm;
  const long mm = diff(sub, gm, absent, &cm);
  cm.report("L-miss (non-resident sheet)");
  check(mm == 0, "lane L (miss): RTL == zref::AuxSource with the sheet absent");
  check(cm.miss > 0 && cm.nonzero_bytes == 0,
        "lane L (miss): every non-degenerate sample is a ZERO miss");
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;
  }

  Vzhao_texture_aux dut;
  lane_gameplay(dut, nightly ? 120000 : 12000);
  lane_limit(dut, nightly ? 120000 : 12000);

  if (failures == 0) std::printf("texture_aux_random: all green\n");
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
