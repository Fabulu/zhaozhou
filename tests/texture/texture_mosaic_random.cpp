// texture_mosaic_random.cpp — randomized Mosaic/fold invariants (deep-keel
// wave; terrain_rules.md §6.2). Not a re-implementation: an independent
// oracle for the pick arithmetic (integer, hand-rolled in u64) plus
// structural laws the fold must hold at ANY coordinate.
//
// What each lane would catch:
//   - pick vs the independent u64 oracle over 100k random (tx,ty,weight)
//     (red on: any constant/compare drift between header and oracle);
//   - fold range/period laws over 100k random u including negatives (red
//     on: out-of-range texel, period != 2 tiles, discontinuity);
//   - determinism: two identical sweeps agree everywhere (red on: any
//     hidden state).
//
// THE RTL LANES (2026-08-19, `zhao_texture_mosaic` + `zhao_texture_mod255`).
// Two differential lanes against the same frozen header, one shaped like a
// real frame and one at the domain limit with the exact-equality boundary
// CONSTRUCTED. Both count their interesting states and ASSERT the counts:
// see the commentary at lane G below for why that is not optional here.

#include "texture_mosaic_dev.hpp"
#include "zref/zref_terrain.hpp"

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

uint64_t oracle_pick_p(int32_t tx, int32_t ty) {
  // the frozen law multiplies in u32 (wraps mod 2^32) BEFORE the XOR - the
  // oracle must wrap identically or it is testing a different function
  const uint32_t hx = static_cast<uint32_t>(tx) * 73856093u;
  const uint32_t hy = static_cast<uint32_t>(ty) * 19349663u;
  return static_cast<uint64_t>(hx ^ hy) % 255ull;
}

void oracle_lanes() {
  uint32_t rng = 0x811C9DC5u;
  const auto next = [&rng]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };

  // 1. pick vs the independent oracle (weights across the full range)
  int n = 0, mismatch = 0;
  for (int t = 0; t < 100000; ++t) {
    const int32_t tx = static_cast<int32_t>(next() % 4096u) - 2048;
    const int32_t ty = static_cast<int32_t>(next() % 4096u) - 2048;
    const uint8_t w = static_cast<uint8_t>(next() & 0xFFu);
    const uint8_t got = zref::terrain::mosaic_pick(7, 9, w, tx, ty);
    const uint8_t want = (oracle_pick_p(tx, ty) < w) ? 7 : 9;
    ++n;
    if (got != want) ++mismatch;
  }
  std::printf("  pick oracle: %d samples, %d mismatches\n", n, mismatch);
  check(mismatch == 0, "mosaic_pick == independent u64 oracle everywhere");

  // 2. fold laws: range, exact period-2, and the mirror seam continuity
  int range_bad = 0, period_bad = 0, seam_bad = 0;
  for (int t = 0; t < 100000; ++t) {
    const int32_t u_raw = static_cast<int32_t>(next()) >> 8;  // wide negatives too
    const int32_t tx = zref::terrain::mirror_texel(u_raw);
    if (tx < 0 || tx > 63) ++range_bad;
    if (zref::terrain::mirror_texel(u_raw + (2 << 16)) != tx) ++period_bad;  // +2 tiles
    // continuity across the integer mirror turn: fold is 1-Lipschitz in u
    const int32_t tx2 = zref::terrain::mirror_texel(u_raw + 1);
    const int d = tx2 > tx ? tx2 - tx : tx - tx2;
    if (d > 1) ++seam_bad;
  }
  check(range_bad == 0, "fold output always in [0,63]");
  check(period_bad == 0, "fold period is exactly 2 tiles");
  check(seam_bad == 0, "fold is 1-Lipschitz (no tear at the mirror turn)");

  // 3. determinism: the same sweep twice agrees bit-for-bit
  uint32_t acc1 = 0, acc2 = 0;
  rng = 0xDEADBEEFu;
  for (int t = 0; t < 50000; ++t) {
    const int32_t tx = static_cast<int32_t>(next() % 65536u) - 32768;
    const int32_t ty = static_cast<int32_t>(next() % 65536u) - 32768;
    acc1 = acc1 * 31 + zref::terrain::mosaic_pick(1, 2, 128, tx, ty);
    acc1 = acc1 * 31 + static_cast<uint32_t>(zref::terrain::mirror_texel(tx));
  }
  rng = 0xDEADBEEFu;
  for (int t = 0; t < 50000; ++t) {
    const int32_t tx = static_cast<int32_t>(next() % 65536u) - 32768;
    const int32_t ty = static_cast<int32_t>(next() % 65536u) - 32768;
    acc2 = acc2 * 31 + zref::terrain::mosaic_pick(1, 2, 128, tx, ty);
    acc2 = acc2 * 31 + static_cast<uint32_t>(zref::terrain::mirror_texel(tx));
  }
  check(acc1 == acc2, "sweeps are stateless (checksum identical)");
}

// ---- 4-5. the two RTL differential lanes ------------------------------------
//
// TWO lanes, deliberately, because one distribution cannot be both.
//
//   G (gameplay-shaped) — the UV a real frame produces: a 32-cell patch's
//     (0,0)-(1,1) per-cell tile units, offset by a world patch origin, with
//     the sub-texel fraction a rasterizer actually interpolates; authored
//     layer-E weights, which in practice are mostly the extremes (a cell with
//     one material) and sometimes a graded transition; tile ids in the
//     authored range, with one span in eight a wall/underside (mosaic off).
//     It answers "does the block agree with the oracle on the traffic it will
//     actually see".
//
//   L (domain limit) — the full int32 UV range including INT32_MIN and
//     INT32_MAX, every weight, and the EXACT-EQUALITY boundary CONSTRUCTED
//     rather than waited for. This is the lane that matters: p is uniform over
//     255 values, so a uniformly random weight lands on `p == weight` about
//     once in 255 draws in ONE direction, and it is precisely the pair
//     (p == weight, p == weight - 1) that separates the law's `<` from `<=`.
//     Three increments in a row have shipped differentials whose coverage
//     counters read zero; both lanes below COUNT their interesting states and
//     the counts are asserted, not printed and forgotten.

namespace mt = mosaic_test;

struct Cover {
  long n = 0;
  long boundary_b = 0;  // weight == p exactly: the law picks B
  long boundary_a = 0;  // weight == p + 1 exactly: the law picks A
  long p_zero = 0;      // the residue the mod-255 correction produces
  long p_max = 0;       // 254, the top of the residue range
  long neg_index = 0;   // a negative world texel index (the sign-extend path)
  long mirror_u = 0;    // u folded from the MIRRORED half (per >= 64)
  long ident_u = 0;     // u folded from the identity half (per < 64)
  long mirror_v = 0;
  long ident_v = 0;
  long w_zero = 0;
  long w_full = 0;
  long mosaic_off = 0;
  long picked_a = 0;
  long picked_b = 0;
  long tx_0 = 0;
  long tx_63 = 0;

  void observe(const mt::Req& r, const mt::Pick& got) {
    ++n;
    const int32_t mu = r.u >> 10, mv = r.v >> 10;
    const uint32_t p = mt::oracle_p(mu, mv);
    if (r.mosaic && r.weight == p) ++boundary_b;
    if (r.mosaic && p != 255u && r.weight == p + 1) ++boundary_a;
    if (p == 0) ++p_zero;
    if (p == 254) ++p_max;
    if (mu < 0 || mv < 0) ++neg_index;
    if (((mu % 128 + 128) % 128) >= 64)
      ++mirror_u;
    else
      ++ident_u;
    if (((mv % 128 + 128) % 128) >= 64)
      ++mirror_v;
    else
      ++ident_v;
    if (r.weight == 0) ++w_zero;
    if (r.weight == 255) ++w_full;
    if (!r.mosaic) ++mosaic_off;
    if (r.mosaic) {
      if (got.tile == r.mat_a)
        ++picked_a;
      else
        ++picked_b;
    }
    if (got.tx == 0) ++tx_0;
    if (got.tx == 63) ++tx_63;
  }

  void report(const char* lane) const {
    std::printf(
        "  lane %s: n=%ld  boundary(p==w)=%ld boundary(p==w-1)=%ld  p0=%ld p254=%ld\n"
        "           neg=%ld  fold u mirror/ident=%ld/%ld  v=%ld/%ld  w0=%ld w255=%ld\n"
        "           mosaic_off=%ld  picked A/B=%ld/%ld  tx0=%ld tx63=%ld\n",
        lane, n, boundary_b, boundary_a, p_zero, p_max, neg_index, mirror_u, ident_u, mirror_v,
        ident_v, w_zero, w_full, mosaic_off, picked_a, picked_b, tx_0, tx_63);
  }
};

// Every state a lane claims to reach is asserted here, so a distribution that
// silently stops producing one turns the lane RED instead of quietly thinning.
void assert_common_coverage(const Cover& c, const char* lane) {
  char buf[160];
  const auto want = [&](bool ok, const char* what) {
    std::snprintf(buf, sizeof(buf), "lane %s reaches %s", lane, what);
    check(ok, buf);
  };
  want(c.mirror_u > 0 && c.ident_u > 0, "both halves of the u fold");
  want(c.mirror_v > 0 && c.ident_v > 0, "both halves of the v fold");
  want(c.tx_0 > 0 && c.tx_63 > 0, "both rails of the folded texel");
  want(c.neg_index > 0, "negative world texel indices (the sign-extend path)");
  want(c.w_zero > 0 && c.w_full > 0, "both weight extremes");
  want(c.mosaic_off > 0, "the fold-only spans (walls/underside)");
  want(c.picked_a > 0 && c.picked_b > 0, "both candidates winning");
  want(c.p_zero > 0, "residue 0 (the mod-255 correction)");
  want(c.p_max > 0, "residue 254 (the top of the range)");
}

// ---- lane G: gameplay-shaped -----------------------------------------------
void rtl_lane_gameplay(Vzhao_texture_mosaic& dut, int packets) {
  mt::Dev dev(dut);
  dev.reset();
  uint32_t rng = 0x9E3779B9u;
  const auto next = [&rng]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };

  std::vector<mt::Req> in;
  in.reserve(static_cast<size_t>(packets));
  // walk a handful of 32-cell patches at plausible world origins
  int32_t patch_u = 0, patch_v = 0;
  for (int k = 0; k < packets; ++k) {
    if ((k % 4096) == 0) {
      // a new patch: origin anywhere in a +-512-tile world span
      patch_u = (static_cast<int32_t>(next() % 1024u) - 512) << 16;
      patch_v = (static_cast<int32_t>(next() % 1024u) - 512) << 16;
    }
    mt::Req r;
    // a fragment inside a cell: 0..32 tiles from the patch origin, with the
    // sub-tile fraction a perspective interpolation actually produces
    r.u = patch_u + static_cast<int32_t>(next() % (32u << 16));
    r.v = patch_v + static_cast<int32_t>(next() % (32u << 16));
    const uint32_t wsel = next() % 10u;
    if (wsel < 4) {
      r.weight = 0;  // a pure-B cell
    } else if (wsel < 8) {
      r.weight = 255;  // a pure-A cell
    } else {
      r.weight = static_cast<uint8_t>(next());  // an authored transition
    }
    r.mosaic = (next() % 8u) != 0;  // one span in eight is a wall/underside
    if (r.mosaic) {
      r.mat_a = static_cast<uint8_t>(next() % 240u);
      r.mat_b = static_cast<uint8_t>(next() % 240u);
    } else {
      r.mat_a = static_cast<uint8_t>(240 + (next() & 1u));  // 6.6 frozen ids
      r.mat_b = 0;
    }
    r.src_id = static_cast<uint16_t>(next());
    in.push_back(r);
  }

  const std::vector<mt::Pick> got = dev.run(in, 0x0F0F0F0Fu);
  Cover c;
  long mismatch = 0;
  if (got.size() != in.size()) {
    check(false, "lane G returned one pick per request");
    return;
  }
  for (size_t k = 0; k < in.size(); ++k) {
    const mt::Pick want = mt::oracle(in[k]);
    if (!(got[k] == want)) ++mismatch;
    c.observe(in[k], got[k]);
  }
  c.report("G (gameplay)");
  check(mismatch == 0, "lane G: RTL == the frozen 6.2 oracle on every packet");
  assert_common_coverage(c, "G");
  // gameplay weights are mostly the extremes, so the exact boundary is RARE
  // here by construction; lane L is where it is manufactured. It must still
  // occur at least once, or the graded-transition arm above has stopped
  // producing traffic.
  check(c.boundary_b + c.boundary_a > 0,
        "lane G reaches the exact-equality boundary at least once");
}

// ---- lane L: the domain limit, with the boundary CONSTRUCTED ----------------
void rtl_lane_limit(Vzhao_texture_mosaic& dut, int packets) {
  mt::Dev dev(dut);
  dev.reset();
  uint32_t rng = 0xC2B2AE35u;
  const auto next = [&rng]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };

  std::vector<mt::Req> in;
  in.reserve(static_cast<size_t>(packets) + 1024);

  // (a) THE CONSTRUCTED BOUNDARY. For every residue value 0..254 find a world
  //     texel that produces it and push BOTH sides: weight == p (must pick B)
  //     and weight == p + 1 (must pick A). 510 packets that a uniform sweep
  //     would need millions of draws to stumble onto in both directions.
  int constructed = 0;
  for (uint32_t p = 0; p < 255u; ++p) {
    int32_t mx = 0, my = 0;
    if (!mt::find_texel_with_p(p, &mx, &my)) continue;
    for (int side = 0; side < 2; ++side) {
      mt::Req r;
      r.u = mt::u_for_texel(mx);
      r.v = mt::u_for_texel(my);
      r.mat_a = 1;
      r.mat_b = 2;
      r.weight = static_cast<uint8_t>(side == 0 ? p : p + 1);
      r.mosaic = true;
      r.src_id = static_cast<uint16_t>(p);
      in.push_back(r);
      ++constructed;
    }
  }
  check(constructed == 510, "lane L constructed both sides of all 255 residues");

  // (b) the rails of the input domain: INT32_MIN/MAX and their neighbourhood,
  //     where the arithmetic shift's sign and the multiplier's wrap meet
  const int32_t rails[8] = {INT32_MIN, INT32_MIN + 1, INT32_MIN + 1023, -1,
                            0,         1023,          INT32_MAX - 1023, INT32_MAX};
  for (int a = 0; a < 8; ++a) {
    for (int b = 0; b < 8; ++b) {
      for (int w = 0; w < 3; ++w) {
        mt::Req r;
        r.u = rails[a];
        r.v = rails[b];
        r.mat_a = 5;
        r.mat_b = 6;
        r.weight = static_cast<uint8_t>(w == 0 ? 0 : (w == 1 ? 128 : 255));
        r.mosaic = true;
        in.push_back(r);
      }
    }
  }

  // (c) uniform over the whole int32 domain
  for (int k = 0; k < packets; ++k) {
    mt::Req r;
    r.u = static_cast<int32_t>(next());
    r.v = static_cast<int32_t>(next());
    r.mat_a = static_cast<uint8_t>(next());
    r.mat_b = static_cast<uint8_t>(next());
    r.weight = static_cast<uint8_t>(next());
    r.mosaic = (next() & 7u) != 0;
    r.src_id = static_cast<uint16_t>(next());
    in.push_back(r);
  }

  const std::vector<mt::Pick> got = dev.run(in, 0xAAAAAAAAu);
  Cover c;
  long mismatch = 0;
  if (got.size() != in.size()) {
    check(false, "lane L returned one pick per request");
    return;
  }
  for (size_t k = 0; k < in.size(); ++k) {
    const mt::Pick want = mt::oracle(in[k]);
    if (!(got[k] == want)) ++mismatch;
    c.observe(in[k], got[k]);
  }
  c.report("L (domain limit)");
  check(mismatch == 0, "lane L: RTL == the frozen 6.2 oracle on every packet");
  assert_common_coverage(c, "L");
  check(c.boundary_b >= 255, "lane L reaches p == weight for every residue");
  check(c.boundary_a >= 254, "lane L reaches p == weight - 1 for every residue");
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;
  }

  oracle_lanes();

  Vzhao_texture_mosaic dut;
  rtl_lane_gameplay(dut, nightly ? 400000 : 40000);
  rtl_lane_limit(dut, nightly ? 400000 : 40000);

  if (failures == 0) std::printf("texture_mosaic_random: all green\n");
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
