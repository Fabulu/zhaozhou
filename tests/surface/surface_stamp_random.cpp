// surface_stamp_random.cpp — randomized differential for SURFACE.STAMP
// against the ratified `zref::render::stamp_surface`, reached through the
// `zref::surface` decomposition that surface_stamp_directed.cpp proves faithful.
//
// TWO LANES, and neither is a uniform 32-bit word:
//
//   Lane A, GAMEPLAY-SHAPED. The canonical 64 m battlefield patch
//   (spec/terrain_rules.md 1.3: 32x32 cells at 2.0 m pitch), stamp radii of
//   0.25..12 m centred on or near the patch, the ABI's two operations, real
//   tag bytes. This is what a Scar Scribe strike actually looks like, and it is
//   the regime where a one-texel placement error matters and where the
//   `terrain-scars` gallery render lives.
//
//   Lane B, AT THE DOMAIN LIMIT. Envelope corners and transform translations
//   out to +-4,096 m (the stated domain — beyond it the REFERENCE's own int64
//   `dx*dx` overflows, so a differential out there compares two overflows),
//   inverted envelopes, negative radii, radii large enough to swallow the whole
//   sheet and small enough to miss it entirely, and all six blends including
//   the field-driven brush.
//
// A single uniform lane would land almost entirely in lane B's regime and
// would pass while the arithmetic was useless for real terrain — which is
// exactly how a flooring defect once hid from 20,000 random triangles
// elsewhere in this tree. Both lanes assert the states they exist to reach.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_surface_stamp.h"

#include "surface_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_surface.hpp"

using sdev::Rng;
using sdev::SheetSim;
using sdev::StampCmd;
using zhao::check;
namespace zs = zref::surface;

namespace {

constexpr int32_t kM = 1 << 16;

struct Stats {
  uint32_t stamps = 0;
  uint32_t mismatches = 0;
  uint32_t zero_cover = 0;
  uint32_t full_cover = 0;
  uint32_t partial_cover = 0;
  uint32_t rings = 0;
  uint32_t hit_255 = 0;          // a texel saturated high
  uint32_t hit_0_after_sub = 0;  // a texel saturated low
  uint32_t inverted_env = 0;
  uint32_t negative_radius = 0;
  uint32_t rejected = 0;
  uint32_t field_stamps = 0;
  uint32_t blend_seen[7] = {0, 0, 0, 0, 0, 0, 0};
  uint32_t stalled = 0;
  // Texels lying EXACTLY on a rim. Added 2026-08-19 after the mutation sweep:
  // flipping the outer test from `>` to `>=` (i.e. clipping the rim texel) was
  // caught ONLY by the directed suite, because a uniformly random radius
  // essentially never makes d2 == r*r exactly. A lane that cannot reach the
  // boundary is not evidence about the boundary, so lane A now CONSTRUCTS the
  // equality and asserts it was reached.
  uint32_t rim_exact_outer = 0;
  uint32_t rim_exact_inner = 0;
};

// On the canonical envelope a texel centre sits at an exact half-metre and two
// centres in the same row are an exact whole number of metres apart, so a
// radius of k metres from texel i0 puts texel i0+k EXACTLY on the outer rim.
// `covers` is inclusive on both radii, so that texel must be stamped.
struct RimProbe {
  bool active = false;
  int outer_i = 0, outer_j = 0;
  bool inner_active = false;
  int inner_i = 0;
};

StampCmd make_cmd(Rng& rng, bool limit_lane, Stats& st, RimProbe* probe, int index) {
  StampCmd c;
  c.handle = 0x0000'2C01;
  c.src_id = static_cast<uint16_t>(rng.range(0, 65535));
  c.tag = static_cast<uint8_t>(rng.range(1, 255));
  c.strength = static_cast<uint16_t>(rng.range(0, 65535));

  if (!limit_lane) {
    // The canonical patch: 32 cells at 2.0 m = 64 m per side, origin-centred.
    c.env = zs::Envelope{-32 * kM, -32 * kM, 32 * kM, 32 * kM};
    // Centres inside the patch and a little outside it, so partial coverage,
    // full coverage and total misses all occur without being forced.
    c.tx = rng.range(-45, 45) * kM;
    c.ty = rng.range(-45, 45) * kM;
    // The two extreme radii are SCHEDULED, not rolled. A 1-in-20 roll over 60
    // stamps leaves it a coin flip whether the fully-covered sheet is ever
    // produced, and the run where it is not is a green run that sampled
    // nothing at the boundary. (Learned twice while writing this file.)
    const int pick = (index % 11 == 3) ? 0 : ((index % 17 == 5) ? 1 : 2);
    if (pick == 0) {
      // Big enough to swallow the whole sheet FROM ANY of the centres above:
      // the far corner from (45, 45) is sqrt(77^2 + 77^2) = 109 m away, so
      // 64 m would never have produced a fully covered sheet and the coverage
      // counter would have read zero while every differential passed.
      c.radius = 160 * kM;
    } else if (pick == 1) {
      c.radius = kM / 4;  // one cell across
    } else {
      c.radius = rng.range(1, 12) * kM;
    }
    if (rng.chance(3)) {
      c.ring_width = rng.range(1, 6) * kM;
      ++st.rings;
    }
    c.operation = static_cast<uint8_t>(rng.range(0, 1));

    if (index % 3 == 1) {
      // Rim-exact construction (see Stats::rim_exact_outer). Scheduled for the
      // same reason as the radii above.
      const int i0 = rng.range(8, 40);
      const int j0 = rng.range(8, 40);
      const int k = rng.range(1, 20);
      c.tx = static_cast<int32_t>(zs::texel_wx(c.env, i0));
      c.ty = static_cast<int32_t>(zs::texel_wz(c.env, j0));
      c.radius = k * kM;
      c.ring_width = 0;
      probe->active = true;
      probe->outer_i = i0 + k;
      probe->outer_j = j0;
      if (probe->outer_i > 63) {
        probe->outer_i = i0 - k;
        if (probe->outer_i < 0) probe->active = false;
      }
      if (k > 2 && rng.chance(2)) {
        const int m = rng.range(1, k - 1);
        c.ring_width = (k - m) * kM;  // r_inner = m metres, exactly
        probe->inner_active = true;
        probe->inner_i = i0 + m <= 63 ? i0 + m : i0 - m;
        if (probe->inner_i < 0) probe->inner_active = false;
      }
    }
  } else {
    const int32_t lim = 4096;
    c.env.x0 = rng.range(-lim, lim) * kM;
    c.env.z0 = rng.range(-lim, lim) * kM;
    // Half the time the envelope is INVERTED (x1 < x0): the only route to the
    // reference's `/ 128` truncation-toward-zero branch.
    c.env.x1 = c.env.x0 + rng.range(-2048, 2048) * kM;
    c.env.z1 = c.env.z0 + rng.range(-2048, 2048) * kM;
    if (c.env.x1 < c.env.x0 || c.env.z1 < c.env.z0) ++st.inverted_env;
    // Aim near the envelope most of the time so coverage is not vanishingly
    // rare at this scale, and far away the rest.
    const int32_t mx = (c.env.x0 / 2) + (c.env.x1 / 2);
    const int32_t mz = (c.env.z0 / 2) + (c.env.z1 / 2);
    c.tx = rng.chance(4) ? rng.range(-lim, lim) * kM : mx + rng.range(-512, 512) * kM;
    c.ty = rng.chance(4) ? rng.range(-lim, lim) * kM : mz + rng.range(-512, 512) * kM;
    c.radius = rng.range(-lim, lim) * kM;
    if (c.radius < 0) ++st.negative_radius;
    if (rng.chance(2)) c.ring_width = rng.range(0, lim) * kM;
    c.blend_en = true;
    c.blend = static_cast<uint8_t>(rng.range(0, 6));
    c.age_shift = static_cast<uint8_t>(rng.range(0, 7));
    c.field_en = rng.chance(4);
  }
  return c;
}

void run_lane(Vzhao_surface_stamp& dut, Rng& rng, int stamps, bool limit_lane, Stats& st) {
  sdev::reset_stamp(dut);
  SheetSim sim(2);
  sim.store.acquire(0x0000'2C01);
  zs::Sheet want;
  // A dirty starting sheet: a clean one cannot tell max from replace, and the
  // whole point of a persistent sheet is that stamps land on old scars.
  for (int t = 0; t < zs::kSheetTexels; ++t) {
    want.strength[t] = static_cast<uint8_t>(rng.range(0, 255));
    want.tag[t] = static_cast<uint8_t>(rng.range(0, 255));
  }
  sim.store.at(sim.store.find(0x0000'2C01)) = want;

  std::vector<zs::FieldResult> fld(zs::kSheetTexels);

  for (int i = 0; i < stamps; ++i) {
    RimProbe probe;
    const StampCmd c = make_cmd(rng, limit_lane, st, &probe, i);
    // Backpressure rides continuously rather than living in one dedicated
    // case, so every stamp is also a handshake test.
    sim.stall_req = rng.chance(2) ? rng.range(2, 9) : 0;
    sim.stall_wr = rng.chance(2) ? rng.range(2, 9) : 0;
    sim.stall_res = rng.chance(2) ? rng.range(2, 9) : 0;
    if (sim.stall_req || sim.stall_wr || sim.stall_res) ++st.stalled;
    // A residency overflow every 13th stamp: the stamp must write NOTHING
    // (S4). Scheduled rather than rolled, because a 1-in-20 roll over 60
    // stamps is a coin flip on whether the case is sampled at all — and a
    // rejection path that is never entered is the exact failure mode these
    // coverage assertions exist to prevent.
    sim.force_overflow = (i % 13 == 7);
    sim.results.clear();
    sim.writes.clear();

    const std::vector<zs::FieldResult>* fp = nullptr;
    if (c.field_en) {
      for (int t = 0; t < zs::kSheetTexels; ++t) {
        const uint32_t tag = static_cast<uint32_t>(rng.range(0, 255));
        const uint32_t bl = static_cast<uint32_t>(rng.range(0, 6));
        const uint32_t ag = static_cast<uint32_t>(rng.range(0, 7));
        fld[static_cast<size_t>(t)].tag_op = tag | (bl << 8) | (ag << 12);
        fld[static_cast<size_t>(t)].strength = static_cast<uint16_t>(rng.range(0, 65535));
      }
      fp = &fld;
      ++st.field_stamps;
    }

    const zs::Sheet before = want;
    std::vector<zs::StampWrite> w;
    if (!sim.force_overflow) {
      if (fp)
        zs::stamp_apply_field(want, c.env, zs::StampGeom{c.tx, c.ty, c.radius, c.ring_width}, *fp,
                              &w);
      else
        zs::stamp_apply(want, c.env, zs::StampGeom{c.tx, c.ty, c.radius, c.ring_width},
                        zs::StampSource{c.tag, c.strength,
                                        c.blend_en ? static_cast<zs::Blend>(c.blend)
                                                   : zs::blend_of_abi_operation(c.operation),
                                        c.age_shift},
                        &w);
    } else {
      ++st.rejected;
    }

    const int cyc = sdev::run_stamp(dut, sim, c, rng, fp);
    if (cyc <= 0) {
      ++st.mismatches;
      continue;
    }
    ++st.stamps;

    // coverage bookkeeping — from the ORACLE, so it describes the stimulus and
    // not whatever the DUT happened to do
    if (!sim.force_overflow) {
      if (w.empty())
        ++st.zero_cover;
      else if (w.size() == static_cast<size_t>(zs::kSheetTexels))
        ++st.full_cover;
      else
        ++st.partial_cover;
      if (!c.field_en)
        ++st.blend_seen[c.blend_en ? c.blend
                                   : static_cast<uint8_t>(zs::blend_of_abi_operation(c.operation))];
      for (const zs::StampWrite& r : w) {
        if (r.strength == 255 && r.before != 255) ++st.hit_255;
        if (r.strength == 0 && r.before != 0) ++st.hit_0_after_sub;
      }
      // The rim texel must be covered — both radii are inclusive. If the
      // construction ever stopped producing exact equality this would go
      // silent, so the counter is asserted at the end of the run.
      const zs::StampGeom g{c.tx, c.ty, c.radius, c.ring_width};
      if (probe.active && zs::covers(c.env, g, probe.outer_i, probe.outer_j)) ++st.rim_exact_outer;
      if (probe.inner_active && zs::covers(c.env, g, probe.inner_i, probe.outer_j))
        ++st.rim_exact_inner;
    }

    const zs::Sheet& got = sim.store.at(sim.store.find(c.handle));
    for (int t = 0; t < zs::kSheetTexels; ++t)
      if (got.tag[t] != want.tag[t] || got.strength[t] != want.strength[t]) {
        ++st.mismatches;
        break;
      }
    if (sim.force_overflow) {
      // S4, checked at the sheet: byte-identical to what it was.
      for (int t = 0; t < zs::kSheetTexels; ++t)
        if (got.tag[t] != before.tag[t] || got.strength[t] != before.strength[t]) {
          ++st.mismatches;
          break;
        }
      if (!sim.results.empty() || !sim.writes.empty()) ++st.mismatches;
    } else {
      // the results stream mirrors the writes exactly
      if (sim.results.size() != w.size() || sim.writes.size() != w.size()) ++st.mismatches;
      for (size_t k = 0; k < w.size() && k < sim.results.size(); ++k)
        if (sim.results[k].texel != w[k].texel || sim.results[k].tag != w[k].tag ||
            sim.results[k].strength != w[k].strength || sim.results[k].before != w[k].before) {
          ++st.mismatches;
          break;
        }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  Vzhao_surface_stamp dut;
  const int per_lane = nightly ? 400 : 60;

  Rng rng(0x57A11'2026ULL);
  Stats a, b;
  run_lane(dut, rng, per_lane, false, a);
  run_lane(dut, rng, per_lane, true, b);

  check(a.mismatches == 0, "lane A (gameplay) matches zref::render::stamp_surface", 0,
        a.mismatches);
  check(b.mismatches == 0, "lane B (domain limit) matches zref::render::stamp_surface", 0,
        b.mismatches);

  // ---- lane A reached the gameplay states ---------------------------------
  check(a.partial_cover > 0, "lane A actually produced partially covered sheets", 1,
        a.partial_cover);
  check(a.full_cover > 0, "lane A actually produced a fully covered sheet", 1, a.full_cover);
  check(a.zero_cover > 0, "lane A actually produced a stamp that covered NOTHING", 1, a.zero_cover);
  check(a.rings > 0, "lane A actually produced annulus stamps", 1, a.rings);
  check(a.hit_255 > 0, "lane A actually drove a texel to the 255 rail", 1, a.hit_255);
  check(a.blend_seen[sdev::kBlStamp] > 0, "lane A used ABI operation 0 (max)", 1,
        a.blend_seen[sdev::kBlStamp]);
  check(a.blend_seen[sdev::kBlDecayAcc] > 0, "lane A used ABI operation 1 (decay-accumulate)", 1,
        a.blend_seen[sdev::kBlDecayAcc]);
  check(a.rejected > 0, "lane A actually hit a residency rejection", 1, a.rejected);
  check(a.stalled > 0, "lane A actually ran under backpressure", 1, a.stalled);
  check(a.rim_exact_outer > 0, "lane A actually stamped a texel EXACTLY on the outer rim", 1,
        a.rim_exact_outer);
  check(a.rim_exact_inner > 0, "lane A actually stamped a texel EXACTLY on the inner rim", 1,
        a.rim_exact_inner);

  // ---- lane B reached the domain limit ------------------------------------
  check(b.inverted_env > 0, "lane B actually built inverted envelopes", 1, b.inverted_env);
  check(b.negative_radius > 0, "lane B actually used negative radii", 1, b.negative_radius);
  check(b.field_stamps > 0, "lane B actually ran the field-driven brush", 1, b.field_stamps);
  check(b.zero_cover > 0, "lane B actually produced a stamp that covered NOTHING", 1, b.zero_cover);
  check(b.full_cover > 0, "lane B actually produced a fully covered sheet", 1, b.full_cover);
  check(b.hit_0_after_sub > 0, "lane B actually drove a texel to the 0 rail", 1, b.hit_0_after_sub);
  uint32_t blends_used = 0;
  for (int k = 0; k < 7; ++k)
    if (b.blend_seen[k] > 0) ++blends_used;
  check(blends_used >= 6, "lane B exercised at least six of the seven blend codes", 6, blends_used);

  std::printf(
      "surface_stamp_random: lane A %u stamps (full %u, partial %u, empty %u, rings %u, "
      "sat255 %u, rejected %u, rim-exact %u/%u), lane B %u stamps (inv-env %u, neg-r %u, field %u, "
      "full %u, empty %u, sat0 %u, blends %u)%s\n",
      a.stamps, a.full_cover, a.partial_cover, a.zero_cover, a.rings, a.hit_255, a.rejected,
      a.rim_exact_outer, a.rim_exact_inner, b.stamps, b.inverted_env, b.negative_radius,
      b.field_stamps, b.full_cover, b.zero_cover, b.hit_0_after_sub, blends_used,
      nightly ? " [nightly]" : "");

  return zhao::report_and_exit("surface_stamp_random");
}
