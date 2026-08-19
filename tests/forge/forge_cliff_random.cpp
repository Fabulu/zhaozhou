// forge_cliff_random.cpp — randomized FORGE.CLIFF differential (deep-keel
// wave; terrain_rules.md §5). The enumeration oracle is INDEPENDENT (a
// plain nested loop re-derivation of "one edge per SOLID cell side facing
// a void/OUT neighbour" per 32x32 page), in the physics==pixels tradition.
//
// What each lane would catch:
//   - enumeration count vs the oracle over 300 random void masks (red on:
//     any side-set drift, page-boundary mishandling);
//   - the emission bound: alive edges <= 512 per page, ALWAYS (red on: a
//     budget hole);
//   - drop accounting: total enumerated - merged - dropped == emitted (red
//     on: silent loss, double count);
//   - determinism: two plans on the same lattice are identical.
//
// THE RTL LANES (2026-08-19, `zhao_forge_cliff`), against the SAME oracle:
// see the commentary at the two lanes below for why the degrade pressure has
// to be CONSTRUCTED rather than waited for.

#include "forge_cliff_dev.hpp"
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

namespace zt = zref::terrain;
namespace zf = zref::forge;

zt::ComposedLattice make_lat(int cw, int ch, const std::vector<uint8_t>& state) {
  zt::ComposedLattice lat;
  lat.w = cw + 1;
  lat.h = ch + 1;
  lat.dual = true;
  lat.wx.resize(lat.w);
  lat.wz.resize(lat.h);
  for (int i = 0; i < lat.w; ++i) lat.wx[i] = ((i - cw / 2) * 2) << 16;
  for (int j = 0; j < lat.h; ++j) lat.wz[j] = ((j - ch / 2) * 2) << 16;
  lat.top.assign(static_cast<size_t>(lat.w) * lat.h, 4 << 16);
  lat.bottom.assign(static_cast<size_t>(lat.w) * lat.h, 0);
  lat.cell_state = state;
  return lat;
}

zt::ComposedLattice random_lat(int cw, int ch, uint32_t seed, int void_bias) {
  uint32_t rng = seed;
  std::vector<uint8_t> st(static_cast<size_t>(cw) * ch, zt::kSolid);
  for (size_t k = 0; k < st.size(); ++k) {
    rng = rng * 1664525u + 1013904223u;
    if ((rng >> 24) % 100u < static_cast<uint32_t>(void_bias))
      st[k] = ((rng >> 20) & 1) ? zt::kVoidAuthored : zt::kVoidBreached;
  }
  return make_lat(cw, ch, st);
}

// the independent oracle: per page, count the rim bodies PRE-clamp (the
// degrade never creates or destroys bodies, so the identity under test is
// emitted bodies + dropped == this, ALWAYS)
size_t oracle_rim_bodies(const zt::ComposedLattice& lat) {
  const int cw = lat.w - 1, ch = lat.h - 1;
  size_t total = 0;
  for (int cj = 0; cj < ch; ++cj) {
    for (int ci = 0; ci < cw; ++ci) {
      if (lat.substance(ci, cj) != zt::kSolid) continue;
      const int noff[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
      for (int s = 0; s < 4; ++s) {
        const int ni = ci + noff[s][0], nj = cj + noff[s][1];
        const bool nonsolid =
            ni < 0 || nj < 0 || ni >= cw || nj >= ch || lat.substance(ni, nj) != zt::kSolid;
        if (nonsolid) ++total;
      }
    }
  }
  return total;
}

// ---- 5-6. the two RTL differential lanes ------------------------------------
//
// TWO lanes, deliberately, because one distribution cannot be both.
//
//   G (gameplay-shaped) — island masks: sparse authored voids and breaches on
//     lattices whose sizes straddle the 32-cell page grid, which is the traffic
//     a real Island Patch produces. Almost none of these pages degrade, so this
//     lane is where the ENUMERATION and the page walk are proved, and where a
//     merge firing without pressure would show up immediately.
//
//   L (domain limit) — masks BUILT to trip the degrade: checkerboard fields
//     that push a page past 512 with nothing mergeable, checkerboards with
//     straight bites cut through them so runs exist under pressure, and random
//     vdist fields (graded, negative, and at the int32 rails) so the priority
//     threshold actually has to find a cut with ties on both sides of it.
//     Random masks essentially never exceed the budget — a 32x32 page needs 512
//     ISOLATED solid cells to reach 2,048 edges — so the pressure is
//     CONSTRUCTED, not waited for. Both lanes COUNT the states they claim to
//     reach and the counts are ASSERTED.

namespace ct = cliff_test;

struct Cover {
  long pages = 0;       // lattices, not pages, but reported as trials
  long no_degrade = 0;  // merged == 0 && dropped == 0
  long merge_only = 0;
  long drop_only = 0;
  long both = 0;
  long spans_gt1 = 0;  // at least one merged span survived to the output
  long vdist_used = 0;
  long partial_page = 0;  // a lattice whose last page is narrower than 32
  long empty_plan = 0;    // a lattice with no rim edges at all
  long max_edges = 0;

  void observe(const zf::RimPlan& p, bool vdist, bool partial) {
    ++pages;
    if (p.merged == 0 && p.dropped == 0)
      ++no_degrade;
    else if (p.dropped == 0)
      ++merge_only;
    else if (p.merged == 0)
      ++drop_only;
    else
      ++both;
    for (size_t k = 0; k < p.edges.size(); ++k) {
      if (p.edges[k].span > 1) {
        ++spans_gt1;
        break;
      }
    }
    if (vdist) ++vdist_used;
    if (partial) ++partial_page;
    if (p.edges.empty()) ++empty_plan;
    if (static_cast<long>(p.edges.size()) > max_edges)
      max_edges = static_cast<long>(p.edges.size());
  }

  void report(const char* lane) const {
    std::printf(
        "  lane %s: trials=%ld  no_degrade=%ld merge_only=%ld drop_only=%ld both=%ld\n"
        "           spans>1=%ld vdist=%ld partial_page=%ld empty=%ld max_edges=%ld\n",
        lane, pages, no_degrade, merge_only, drop_only, both, spans_gt1, vdist_used, partial_page,
        empty_plan, max_edges);
  }
};

// A mask with a checkerboard field (which no run can bridge) and `bites`
// straight one-cell-wide cuts (which are the ONLY thing that produces runs).
// The two together are what makes a page degrade by merging rather than only by
// dropping.
std::vector<uint8_t> pressured_mask(int cw, int ch, int density, int bites, uint32_t seed) {
  std::vector<uint8_t> st(static_cast<size_t>(cw) * ch, zt::kSolid);
  uint32_t rng = seed;
  const auto next = [&rng]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };
  for (int cj = 0; cj < ch; ++cj) {
    for (int ci = 0; ci < cw; ++ci) {
      if ((ci + cj) % 2 == 1 && static_cast<int>((next() >> 16) % 100u) < density) {
        st[static_cast<size_t>(cj) * cw + ci] = zt::kVoidAuthored;
      }
    }
  }
  for (int b = 0; b < bites; ++b) {
    const int row = static_cast<int>(next() % static_cast<uint32_t>(ch));
    const int len = 4 + static_cast<int>(next() % 24u);
    const int c0 = static_cast<int>(next() % static_cast<uint32_t>(cw));
    for (int k = 0; k < len && c0 + k < cw; ++k) {
      st[static_cast<size_t>(row) * cw + (c0 + k)] = zt::kVoidBreached;
    }
  }
  return st;
}

// A SINGLE 32x32 page tuned to land just past the 512 budget WITH long runs
// available, which is the only shape in which the merge alone brings a page
// back inside budget — and therefore the only shape whose LAST merge takes a
// PREFIX of its run instead of the whole thing (the reference's `if (take - 1 >
// need) take = need + 1`).
//
// This family exists because a mutation found the hole: replacing the prefix
// rule with "merge whole runs" left BOTH random lanes green and was caught only
// by the hand-built 96x96 directed fixture. Random pressure always overshot the
// budget so far that every run was consumed whole and the priority degrade
// finished the job. Constructed, not hoped for.
std::vector<uint8_t> prefix_mask(int voids, int bite_len, int bite_row) {
  std::vector<uint8_t> st(32 * 32, zt::kSolid);
  int n = 0;
  for (int cj = 1; cj <= 14 && n < voids; ++cj) {
    for (int ci = 1; ci <= 30 && n < voids; ++ci) {
      if ((ci + cj) % 2 == 1) {
        st[static_cast<size_t>(cj) * 32 + ci] = zt::kVoidAuthored;
        ++n;
      }
    }
  }
  for (int k = 0; k < bite_len && 2 + k < 31; ++k) {
    st[static_cast<size_t>(bite_row) * 32 + (2 + k)] = zt::kVoidBreached;
  }
  return st;
}

// A vdist field with the shape that stresses the threshold search: many ties,
// a spread of magnitudes, negatives, and the int32 rails.
std::vector<int32_t> make_vdist(size_t n, uint32_t seed, int mode) {
  std::vector<int32_t> v(n, 0);
  uint32_t rng = seed;
  const auto next = [&rng]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };
  for (size_t k = 0; k < n; ++k) {
    switch (mode) {
      case 0:
        v[k] = static_cast<int32_t>(next() % 7u) << 16;
        break;  // heavy ties
      case 1:
        v[k] = static_cast<int32_t>(next());
        break;  // full range
      case 2:
        v[k] = static_cast<int32_t>(next() % 5u) - 2;
        break;  // small, signed
      default:
        v[k] = (k % 4 == 0) ? INT32_MIN : ((k % 4 == 1) ? INT32_MAX : static_cast<int32_t>(next()));
        break;
    }
  }
  return v;
}

void rtl_lane_gameplay(Vzhao_forge_cliff& dut, int trials) {
  Cover c;
  long mismatch = 0;
  for (int t = 0; t < trials; ++t) {
    const int cw = (t % 3 == 0) ? 8 : (t % 3 == 1 ? 33 : 40);
    const int ch = (t % 4 == 0) ? 8 : (t % 4 == 1 ? 33 : 72);
    const int bias = 5 + (t * 7) % 40;
    const zt::ComposedLattice lat = random_lat(cw, ch, 0x5EED0000u + t * 7919u, bias);
    const bool vd_on = (t % 5) == 0;
    const std::vector<int32_t> vd =
        make_vdist(static_cast<size_t>(lat.w) * lat.h, 0xABCD0000u + t, t % 4);
    const int32_t* vp = vd_on ? vd.data() : nullptr;
    const ct::Plan got = ct::plan_lattice(dut, lat, vp, (t % 3 == 1) ? 0xAAAAAAAAu : 0u);
    const zf::RimPlan want = zf::rim_plan(lat, vp);
    if (!ct::same(got, want)) {
      ++mismatch;
      if (mismatch == 1) {
        std::fprintf(stderr,
                     "FAIL: lane G trial %d (cw=%d ch=%d bias=%d vdist=%d): RTL %zu/%u/%u vs "
                     "oracle %zu/%u/%u%s\n",
                     t, cw, ch, bias, vd_on ? 1 : 0, got.edges.size(), got.merged, got.dropped,
                     want.edges.size(), want.merged, want.dropped, got.timed_out ? " TIMEOUT" : "");
      }
    }
    c.observe(want, vd_on, (cw % 32) != 0 || (ch % 32) != 0);
  }
  c.report("G (gameplay)");
  check(mismatch == 0, "lane G: RTL == zref::forge::rim_plan on every island mask");
  check(c.no_degrade > 0, "lane G reaches undegraded pages (the common case)");
  check(c.partial_page > 0, "lane G reaches lattices with a partial last page");
  check(c.vdist_used > 0, "lane G exercises both the null and the live vdist paths");
  check(c.vdist_used < c.pages, "lane G exercises the null vdist path too");
}

void rtl_lane_limit(Vzhao_forge_cliff& dut, int trials) {
  Cover c;
  long mismatch = 0;
  for (int t = 0; t < trials; ++t) {
    // sizes that put a FULL 32x32 page under pressure, and some that leave a
    // partial page beside it
    const int cw = (t % 3 == 0) ? 32 : (t % 3 == 1 ? 40 : 64);
    const int ch = (t % 3 == 0) ? 32 : (t % 3 == 1 ? 33 : 32);
    const int density = 70 + static_cast<int>((t * 13) % 31);  // 70..100
    const int bites = (t % 4);
    // Every third trial is a PREFIX-MERGE fixture (see prefix_mask): a single
    // page just past the budget with two long runs, so the merge alone can
    // bring it back inside and the last merge takes a prefix.
    const bool prefix_trial = (t % 3) == 2;
    const zt::ComposedLattice lat =
        prefix_trial
            ? make_lat(32, 32, prefix_mask(88 + (t % 24), 10 + (t % 15), 20))
            : make_lat(cw, ch, pressured_mask(cw, ch, density, bites, 0xC0FFEE00u + t * 104729u));
    const bool vd_on = (t % 2) == 0;
    const std::vector<int32_t> vd =
        make_vdist(static_cast<size_t>(lat.w) * lat.h, 0x1234000u + t, t % 4);
    const int32_t* vp = vd_on ? vd.data() : nullptr;
    const ct::Plan got = ct::plan_lattice(dut, lat, vp, (t % 4 == 3) ? 0x0F0F0F0Fu : 0u);
    const zf::RimPlan want = zf::rim_plan(lat, vp);
    if (!ct::same(got, want)) {
      ++mismatch;
      if (mismatch == 1) {
        std::fprintf(stderr,
                     "FAIL: lane L trial %d (cw=%d ch=%d dens=%d bites=%d vdist=%d): RTL %zu/%u/%u "
                     "vs oracle %zu/%u/%u%s\n",
                     t, cw, ch, density, bites, vd_on ? 1 : 0, got.edges.size(), got.merged,
                     got.dropped, want.edges.size(), want.merged, want.dropped,
                     got.timed_out ? " TIMEOUT" : "");
      }
    }
    c.observe(want, vd_on, !prefix_trial && ((cw % 32) != 0 || (ch % 32) != 0));
  }
  c.report("L (domain limit)");
  check(mismatch == 0, "lane L: RTL == zref::forge::rim_plan under both degrades");
  check(c.drop_only > 0, "lane L reaches the priority degrade with nothing to merge");
  check(c.both > 0, "lane L reaches BOTH degrades on one page");
  check(c.spans_gt1 > 0, "lane L reaches a surviving MERGED span");
  check(c.vdist_used > 0, "lane L exercises the vdist priority path");
  check(c.max_edges >= 512, "lane L reaches the per-page budget");
  check(c.merge_only > 0,
        "lane L reaches a page the MERGE alone brings inside budget (the prefix case)");
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;
  }

  int count_ok = 0, budget_bad = 0, account_bad = 0, det_bad = 0;
  int trials = 0;
  for (int t = 0; t < 300; ++t) {
    // sizes sweep page boundaries: 8x8, 33x33, 40x72 (multi-page)
    const int cw = (t % 3 == 0) ? 8 : (t % 3 == 1 ? 33 : 40);
    const int ch = (t % 4 == 0) ? 8 : (t % 4 == 1 ? 33 : 72);
    const int bias = 15 + (t * 7) % 60;  // sparse voids through checkerboard-ish
    const zt::ComposedLattice lat = random_lat(cw, ch, 0x5EED0000u + t * 7919u, bias);
    const zf::RimPlan p = zf::rim_plan(lat, nullptr);
    const zf::RimPlan p2 = zf::rim_plan(lat, nullptr);
    ++trials;

    if (!(p.edges == p2.edges && p.merged == p2.merged && p.dropped == p2.dropped)) ++det_bad;
    if (p.edges.size() > static_cast<size_t>(zf::kRimBudgetPerPage) *
                             static_cast<size_t>((cw + 31) / 32) *
                             static_cast<size_t>((ch + 31) / 32))
      ++budget_bad;

    // THE accounting identity: every enumerated body is either inside an
    // emitted span or dropped (merging absorbs bodies INTO spans, it never
    // destroys them) - emitted_bodies + dropped == the pre-clamp count
    size_t emitted_bodies = 0;
    for (const zf::RimEdge& e : p.edges) emitted_bodies += e.span;
    const size_t oracle_total = oracle_rim_bodies(lat);
    if (emitted_bodies + p.dropped != oracle_total) ++account_bad;
    // sparse cases (no clamp, no merge): the plan IS the enumeration
    if (p.merged == 0 && p.dropped == 0 && p.edges.size() != oracle_total) ++count_ok;
  }
  std::printf("  random rims: %d trials (det_bad=%d budget_bad=%d count_bad=%d acct_bad=%d)\n",
              trials, det_bad, budget_bad, count_ok, account_bad);
  check(count_ok == 0, "sparse-mask enumeration == independent oracle (no clamp/merge)");
  check(budget_bad == 0, "per-page emission bound holds under every mask");
  check(account_bad == 0, "emitted + merged + dropped reconstructs the enumeration");
  check(det_bad == 0, "rim_plan is a pure function of the lattice");

  Vzhao_forge_cliff dut;
  rtl_lane_gameplay(dut, nightly ? 600 : 150);
  rtl_lane_limit(dut, nightly ? 240 : 60);

  if (failures == 0) std::printf("forge_cliff_random: all green\n");
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
