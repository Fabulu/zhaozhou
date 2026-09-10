// forge_cliff_ram_differential.cpp — the storage differential for the
// FORGE.CLIFF bitmap-RAM candidate (Roadmap §14 Commit6 / architecture D1).
//
// THREE machines in one process, every fixture through all three:
//   oracle     zref::forge::rim_plan — THE law (the block is REFERENCE_COMPLETE);
//   golden     zhao_forge_cliff      — the shipping RTL, bitmaps in flip-flops;
//   candidate  zhao_forge_cliff_ram  — the same algorithm on a row-window RAM
//                                      and span-walk compaction.
// The plan (edge stream, merged, dropped) must be bit-identical across all
// three. The golden is compared too, not only the oracle, so a fixture on
// which the golden itself disagreed with the law would be visible as such
// rather than as a candidate defect.
//
// What each lane would catch (the "could have been red" statement):
//   1. the golden suites' own fixtures — enumeration anchors, the 32x32
//      checkerboard clamp, four vdist shapes, the 96x96 merge fixture and its
//      unpressured control. Red on: anything the golden suites are red on.
//   2. the ROW-WINDOW timing edges the golden never had — a page one cell
//      wide (a four-clock cell row, the tightest prefetch window), one cell
//      tall, two and three rows (the prime + first rotate), a PAUSED load
//      stream (the 33-bit assembly and the per-row RAM write handshake),
//      partial pages on both axes. Red on: a prefetch a row off, a rotate a
//      cycle early or late, an assembly bit order slip, a stalled write.
//   3. the COMPACTION edges — a page with MANY merges so `wr` trails `rd` by
//      hundreds of entries; a page whose merge alone brings it inside budget
//      (dense emit after compaction, no priority pass); a page where a MERGED
//      span is then DROPPED by priority (R2: its whole body count); the 20/20
//      need-31 prefix merge (20 + 13); the both-degrades random lane. Red on:
//      a copy landing on the wrong index, a prio written at rd instead of wr,
//      a dropped span counting entries instead of bodies, a walk that skips a
//      live head or visits a dead interior.
//   4. the accounting identity `emitted_bodies + dropped == enumerated` on
//      EVERY lattice for both RTLs, and `walk_fault_o == 0` for the candidate
//      (its positive control is the committed mutant, run separately).
//   5. cycle counts, golden vs candidate, per fixture — MEASURED and printed,
//      never asserted: the candidate pays five prime cycles per page and
//      saves the merge clears and the dead-entry iterations.
//
// The random generators below are copies of tests/forge/forge_cliff_random.cpp's
// on purpose: that suite is kept byte-identical so "the golden suites pass
// UNMODIFIED against the candidate" is a literal statement, not a paraphrase.

#include "forge_cliff_dev.hpp"
#include "Vzhao_forge_cliff_ram.h"
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
namespace ct = cliff_test;

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

std::vector<uint8_t> all_solid(int cw, int ch) { return std::vector<uint8_t>(static_cast<size_t>(cw) * ch, zt::kSolid); }

std::vector<uint8_t> checker(int cw, int ch) {
  std::vector<uint8_t> ck(static_cast<size_t>(cw) * ch, zt::kVoidAuthored);
  for (int cj = 0; cj < ch; ++cj)
    for (int ci = 0; ci < cw; ++ci)
      if ((ci + cj) % 2 == 0) ck[static_cast<size_t>(cj) * cw + ci] = zt::kSolid;
  return ck;
}

// ---- the random generators, copied from forge_cliff_random.cpp ---------------
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

std::vector<int32_t> make_vdist(size_t n, uint32_t seed, int mode) {
  std::vector<int32_t> v(n, 0);
  uint32_t rng = seed;
  const auto next = [&rng]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };
  for (size_t k = 0; k < n; ++k) {
    switch (mode) {
      case 0: v[k] = static_cast<int32_t>(next() % 7u) << 16; break;
      case 1: v[k] = static_cast<int32_t>(next()); break;
      case 2: v[k] = static_cast<int32_t>(next() % 5u) - 2; break;
      default:
        v[k] = (k % 4 == 0) ? INT32_MIN : ((k % 4 == 1) ? INT32_MAX : static_cast<int32_t>(next()));
        break;
    }
  }
  return v;
}

// the independent enumeration oracle (forge_cliff_random.cpp's)
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

// ---- the three-way comparison ----------------------------------------------
struct Cover {
  long lattices = 0, pages = 0;
  long merged_pages = 0, dropped_pages = 0, both = 0, merge_only = 0;
  long dropped_span_gt1 = 0;  // a DROPPED entry carried span > 1 (R2 exercised)
  long spans_gt1 = 0;         // a merged span SURVIVED to the output
  long partial = 0, one_wide = 0, one_tall = 0, ld_stalled = 0, out_stalled = 0, vdist_on = 0;
  long span13 = 0;            // the 20/20 need-31 prefix case reached
  long golden_clocks = 0, cand_clocks = 0;
  long golden_worst = 0, cand_worst = 0;
  long cand_slower_lattices = 0;
};
Cover cov;

struct Duts {
  Vzhao_forge_cliff& g;
  Vzhao_forge_cliff_ram& c;
};

void report_diff(const char* what, const char* who, const ct::Plan& got, const zf::RimPlan& want) {
  std::fprintf(stderr, "FAIL: %s [%s vs oracle]\n", what, who);
  std::fprintf(stderr, "  %s edges=%zu merged=%u dropped=%u%s\n", who, got.edges.size(), got.merged,
               got.dropped, got.timed_out ? " TIMED OUT" : "");
  std::fprintf(stderr, "  oracle edges=%zu merged=%u dropped=%u\n", want.edges.size(), want.merged,
               want.dropped);
  const size_t n = got.edges.size() < want.edges.size() ? got.edges.size() : want.edges.size();
  for (size_t k = 0; k < n; ++k) {
    if (!(got.edges[k] == want.edges[k])) {
      std::fprintf(stderr,
                   "  first diff at [%zu]: %s (ci %u cj %u side %u span %u) vs oracle (ci %u cj %u "
                   "side %u span %u)\n",
                   k, who, got.edges[k].ci, got.edges[k].cj, got.edges[k].side, got.edges[k].span,
                   want.edges[k].ci, want.edges[k].cj, want.edges[k].side, want.edges[k].span);
      break;
    }
  }
}

// Run one lattice through all three machines; returns true if all agree.
bool three_way(Duts& d, const zt::ComposedLattice& lat, const int32_t* vdist, uint32_t out_stall,
               uint32_t ld_stall, const char* what, bool verbose = false) {
  const zf::RimPlan want = zf::rim_plan(lat, vdist);
  const ct::Plan g = ct::plan_lattice(d.g, lat, vdist, out_stall, ld_stall);
  const ct::Plan c = ct::plan_lattice(d.c, lat, vdist, out_stall, ld_stall);
  bool ok = true;
  if (!ct::same(g, want)) {
    report_diff(what, "golden", g, want);
    ok = false;
  }
  if (!ct::same(c, want)) {
    report_diff(what, "candidate", c, want);
    ok = false;
  }
  // golden vs candidate directly (independent of the oracle's verdict)
  bool gc = g.edges.size() == c.edges.size() && g.merged == c.merged && g.dropped == c.dropped;
  for (size_t k = 0; gc && k < g.edges.size(); ++k) gc = (g.edges[k] == c.edges[k]);
  if (!gc) {
    std::fprintf(stderr, "FAIL: %s [golden vs candidate differ]\n", what);
    ok = false;
  }
  // the accounting identity, both RTLs
  const size_t bodies = oracle_rim_bodies(lat);
  size_t gb = 0, cb = 0;
  for (const auto& e : g.edges) gb += e.span;
  for (const auto& e : c.edges) cb += e.span;
  if (gb + g.dropped != bodies) {
    std::fprintf(stderr, "FAIL: %s [golden identity: %zu bodies + %u dropped != %zu]\n", what, gb,
                 g.dropped, bodies);
    ok = false;
  }
  if (cb + c.dropped != bodies) {
    std::fprintf(stderr, "FAIL: %s [candidate identity: %zu bodies + %u dropped != %zu]\n", what,
                 cb, c.dropped, bodies);
    ok = false;
  }
  if (!ok) ++failures;

  // coverage, from the ORACLE's plan (so a broken RTL cannot inflate it)
  const int cw = lat.w - 1, ch = lat.h - 1;
  const long npages = static_cast<long>((cw + 31) / 32) * static_cast<long>((ch + 31) / 32);
  ++cov.lattices;
  cov.pages += npages;
  if (want.merged > 0) ++cov.merged_pages;
  if (want.dropped > 0) ++cov.dropped_pages;
  if (want.merged > 0 && want.dropped > 0) ++cov.both;
  if (want.merged > 0 && want.dropped == 0) ++cov.merge_only;
  // dropped ENTRIES = enumerated - merged - kept; if dropped BODIES exceed
  // that, at least one dropped entry was a merged span.
  const long dropped_entries =
      static_cast<long>(bodies) - static_cast<long>(want.merged) - static_cast<long>(want.edges.size());
  if (static_cast<long>(want.dropped) > dropped_entries) ++cov.dropped_span_gt1;
  for (const auto& e : want.edges) {
    if (e.span > 1) {
      ++cov.spans_gt1;
      break;
    }
  }
  for (const auto& e : want.edges)
    if (e.span == 13) {
      ++cov.span13;
      break;
    }
  if ((cw % 32) != 0 || (ch % 32) != 0) ++cov.partial;
  if (cw == 1) ++cov.one_wide;
  if (ch == 1) ++cov.one_tall;
  if (ld_stall) ++cov.ld_stalled;
  if (out_stall) ++cov.out_stalled;
  if (vdist) ++cov.vdist_on;
  cov.golden_clocks += g.clocks;
  cov.cand_clocks += c.clocks;
  if (g.worst_page > cov.golden_worst) cov.golden_worst = g.worst_page;
  if (c.worst_page > cov.cand_worst) cov.cand_worst = c.worst_page;
  if (c.clocks > g.clocks) ++cov.cand_slower_lattices;
  if (verbose) {
    std::printf("  %-52s edges=%5zu merged=%4u dropped=%4u | clocks golden %7ld cand %7ld (%+ld)\n",
                what, want.edges.size(), want.merged, want.dropped, g.clocks, c.clocks,
                c.clocks - g.clocks);
  }
  return ok;
}

// ---- 1. the golden suites' fixtures -------------------------------------------
void lane_golden_fixtures(Duts& d) {
  std::printf("[1] golden suite fixtures\n");
  three_way(d, make_lat(4, 4, all_solid(4, 4)), nullptr, 0, 0, "4x4 solid block", true);
  std::vector<uint8_t> st = all_solid(4, 4);
  for (int cj = 1; cj <= 2; ++cj)
    for (int ci = 1; ci <= 2; ++ci) st[cj * 4 + ci] = zt::kVoidBreached;
  three_way(d, make_lat(4, 4, st), nullptr, 0, 0, "4x4 with a centre bite", true);
  three_way(d, make_lat(8, 8, checker(8, 8)), nullptr, 0, 0, "8x8 checkerboard", true);
  three_way(d, make_lat(4, 4, std::vector<uint8_t>(16, zt::kVoidAuthored)), nullptr, 0, 0,
            "all-void 4x4", true);
  three_way(d, make_lat(1, 1, all_solid(1, 1)), nullptr, 0, 0, "single cell", true);

  const zt::ComposedLattice ck = make_lat(32, 32, checker(32, 32));
  three_way(d, ck, nullptr, 0, 0, "32x32 checkerboard, null vdist", true);
  const size_t n = static_cast<size_t>(ck.w) * ck.h;
  std::vector<int32_t> spike(n, 0);
  spike[31 * ck.w + 32] = 1 << 16;
  three_way(d, ck, spike.data(), 0, 0, "32x32 checkerboard, one nearest spike", true);
  std::vector<int32_t> graded(n, 0);
  for (size_t k = 0; k < n; ++k) graded[k] = static_cast<int32_t>((k * 37) % 11) << 16;
  three_way(d, ck, graded.data(), 0, 0, "32x32 checkerboard, graded vdist", true);
  std::vector<int32_t> negs(n, 0);
  for (size_t k = 0; k < n; ++k) negs[k] = static_cast<int32_t>((k * 29) % 7) - 3;
  three_way(d, ck, negs.data(), 0, 0, "32x32 checkerboard, negative vdist", true);
  std::vector<int32_t> rails(n, 0);
  for (size_t k = 0; k < n; ++k) rails[k] = (k % 3 == 0) ? INT32_MIN : ((k % 3 == 1) ? INT32_MAX : 0);
  three_way(d, ck, rails.data(), 0, 0, "32x32 checkerboard, INT32 rails", true);
  const uint32_t masks[4] = {0xFFFFFFFEu, 0xAAAAAAAAu, 0x0F0F0F0Fu, 0x80000001u};
  for (int m = 0; m < 4; ++m) {
    char what[64];
    std::snprintf(what, sizeof what, "32x32 checkerboard, output stall %08x", masks[m]);
    three_way(d, ck, nullptr, masks[m], 0, what, m == 1);
  }

  // the 96x96 merge fixture (20 + 13) and its unpressured control
  const int CW = 96, CH = 96;
  std::vector<uint8_t> m = all_solid(CW, CH);
  for (int ci = 40; ci <= 59; ++ci) m[48 * CW + ci] = zt::kVoidBreached;
  int nv = 0;
  for (int cj = 33; cj <= 46 && nv < 127; ++cj)
    for (int ci = 32; ci <= 63 && nv < 127; ++ci)
      if ((ci + cj) % 2 == 1) {
        m[cj * CW + ci] = zt::kVoidBreached;
        ++nv;
      }
  const zt::ComposedLattice merge_lat = make_lat(CW, CH, m);
  three_way(d, merge_lat, nullptr, 0, 0, "96x96 merge fixture (20 + 13 prefix)", true);
  {
    const zf::RimPlan want = zf::rim_plan(merge_lat, nullptr);
    int s20 = 0, s13 = 0;
    for (const auto& e : want.edges) {
      if (e.span == 20) ++s20;
      if (e.span == 13) ++s13;
    }
    check(want.merged == 31 && want.dropped == 0 && s20 == 1 && s13 == 1,
          "the merge fixture still reads 20 + 13, merged 31, dropped 0 (oracle)");
  }
  std::vector<uint8_t> m2 = all_solid(CW, CH);
  for (int ci = 40; ci <= 59; ++ci) m2[48 * CW + ci] = zt::kVoidBreached;
  three_way(d, make_lat(CW, CH, m2), nullptr, 0, 0, "96x96 unpressured bite (no merge)", true);
}

// ---- 2. the row-window timing edges ---------------------------------------------
void lane_row_window(Duts& d) {
  std::printf("[2] row-window timing edges\n");
  // one cell wide: a four-clock cell row, the tightest prefetch window; tall
  // enough for two vertical pages and a partial one
  {
    std::vector<uint8_t> st = all_solid(1, 70);
    for (int cj = 0; cj < 70; cj += 3) st[cj] = zt::kVoidAuthored;
    three_way(d, make_lat(1, 70, st), nullptr, 0, 0, "1 wide x 70 tall (cw = 1)", true);
  }
  // one cell tall: a single row, no rotate ever consulted
  {
    std::vector<uint8_t> st = all_solid(70, 1);
    for (int ci = 0; ci < 70; ci += 4) st[ci] = zt::kVoidBreached;
    three_way(d, make_lat(70, 1, st), nullptr, 0, 0, "70 wide x 1 tall (ch = 1)", true);
  }
  // two and three rows: the prime rows are the whole page
  three_way(d, make_lat(5, 2, checker(5, 2)), nullptr, 0, 0, "5x2 checker (two rows)", true);
  three_way(d, make_lat(5, 3, checker(5, 3)), nullptr, 0, 0, "5x3 checker (three rows)", true);
  three_way(d, make_lat(2, 2, all_solid(2, 2)), nullptr, 0, 0, "2x2 solid", true);
  // partial pages on both axes with real content
  three_way(d, random_lat(33, 33, 0x1234u, 30), nullptr, 0, 0, "33x33 random (partial both axes)",
            true);
  three_way(d, random_lat(40, 72, 0x5678u, 25), nullptr, 0, 0, "40x72 random (2x3 pages)", true);
  // a PAUSED load stream on the checkerboard and on the merge fixture
  const zt::ComposedLattice ck = make_lat(32, 32, checker(32, 32));
  const uint32_t ld_masks[3] = {0xAAAAAAAAu, 0xFFFFFFFEu, 0x0F0F0F0Fu};
  for (int m = 0; m < 3; ++m) {
    char what[64];
    std::snprintf(what, sizeof what, "32x32 checkerboard, load stall %08x", ld_masks[m]);
    three_way(d, ck, nullptr, 0, ld_masks[m], what, true);
  }
  {
    const int CW = 96, CH = 96;
    std::vector<uint8_t> m = all_solid(CW, CH);
    for (int ci = 40; ci <= 59; ++ci) m[48 * CW + ci] = zt::kVoidBreached;
    int nv = 0;
    for (int cj = 33; cj <= 46 && nv < 127; ++cj)
      for (int ci = 32; ci <= 63 && nv < 127; ++ci)
        if ((ci + cj) % 2 == 1) {
          m[cj * CW + ci] = zt::kVoidBreached;
          ++nv;
        }
    three_way(d, make_lat(CW, CH, m), nullptr, 0x0F0F0F0Fu, 0xAAAAAAAAu,
              "96x96 merge fixture, load AND output stalls", true);
  }
}

// ---- 3. the compaction edges -----------------------------------------------------
void lane_compaction(Duts& d) {
  std::printf("[3] compaction edges\n");
  // MANY merges: solid 32x32 with a full-width bite every third row. Each bite
  // makes two 30-runs (the row above, side +z; the row below, side -z) plus
  // two end walls; ten bites = 640 edges, need 128 -> four whole 30-runs
  // (29 shed each = 116) and a 13-prefix of the fifth (12) — five merges,
  // and `wr` trails `rd` by up to 128 entries during the compaction.
  {
    std::vector<uint8_t> st = all_solid(32, 32);
    for (int cj = 2; cj < 32; cj += 3)
      for (int ci = 1; ci <= 30; ++ci) st[cj * 32 + ci] = zt::kVoidBreached;
    const zt::ComposedLattice lat = make_lat(32, 32, st);
    three_way(d, lat, nullptr, 0, 0, "32x32 ten full-width bites (many merges)", true);
    const zf::RimPlan want = zf::rim_plan(lat, nullptr);
    check(want.merged > 100 && want.dropped == 0,
          "the ten-bite page merges >100 bodies and drops none (oracle)");
    // the same page with vdist: the priority pass now runs on the dense table
    const size_t n = static_cast<size_t>(lat.w) * lat.h;
    const std::vector<int32_t> vd = make_vdist(n, 0xBEEF, 1);
    three_way(d, lat, vd.data(), 0, 0, "32x32 ten bites, full-range vdist", true);
  }
  // a MERGED span DROPPED by priority: bites for runs, dense checkerboard for
  // pressure beyond what merging can fix, and a vdist that makes the bite
  // rows' vertices the FARTHEST (lowest priority), so the merged heads drop
  // and take their whole body count with them (R2).
  {
    std::vector<uint8_t> st = all_solid(32, 32);
    for (int cj = 0; cj < 32; ++cj)
      for (int ci = 0; ci < 32; ++ci)
        if (cj >= 8 && (ci + cj) % 2 == 1) st[cj * 32 + ci] = zt::kVoidAuthored;
    for (int cj = 1; cj <= 5; cj += 2)
      for (int ci = 1; ci <= 30; ++ci) st[cj * 32 + ci] = zt::kVoidBreached;
    const zt::ComposedLattice lat = make_lat(32, 32, st);
    const size_t n = static_cast<size_t>(lat.w) * lat.h;
    std::vector<int32_t> vd(n, 1 << 16);
    for (int vj = 0; vj <= 7; ++vj)
      for (int vi = 0; vi < lat.w; ++vi) vd[static_cast<size_t>(vj) * lat.w + vi] = -(1 << 16);
    three_way(d, lat, vd.data(), 0, 0, "32x32 bites + checker, bites farthest (merged spans drop)",
              true);
    const zf::RimPlan want = zf::rim_plan(lat, vd.data());
    const size_t bodies = oracle_rim_bodies(lat);
    const long dropped_entries =
        static_cast<long>(bodies) - static_cast<long>(want.merged) - static_cast<long>(want.edges.size());
    check(want.merged > 0 && static_cast<long>(want.dropped) > dropped_entries,
          "a MERGED span is among the dropped entries (dropped bodies > dropped entries, oracle)");
  }
  // the merge alone brings the page inside budget (dense emit, no priority pass)
  for (int t = 0; t < 6; ++t) {
    char what[64];
    std::snprintf(what, sizeof what, "prefix-merge page %d (merge alone suffices)", t);
    three_way(d, make_lat(32, 32, prefix_mask(88 + (t * 4) % 24, 10 + (t * 3) % 15, 20)), nullptr, 0,
              0, what, t == 0);
  }
}

// ---- 4. the random lanes, both distributions -----------------------------------
void lane_random(Duts& d, int g_trials, int l_trials) {
  std::printf("[4] random lanes: G %d, L %d\n", g_trials, l_trials);
  long g_bad = 0;
  for (int t = 0; t < g_trials; ++t) {
    const int cw = (t % 3 == 0) ? 8 : (t % 3 == 1 ? 33 : 40);
    const int ch = (t % 4 == 0) ? 8 : (t % 4 == 1 ? 33 : 72);
    const int bias = 5 + (t * 7) % 40;
    const zt::ComposedLattice lat = random_lat(cw, ch, 0x5EED0000u + t * 7919u, bias);
    const bool vd_on = (t % 5) == 0;
    const std::vector<int32_t> vd = make_vdist(static_cast<size_t>(lat.w) * lat.h, 0xABCD0000u + t, t % 4);
    char what[48];
    std::snprintf(what, sizeof what, "lane G trial %d", t);
    const int before = failures;
    three_way(d, lat, vd_on ? vd.data() : nullptr, (t % 3 == 1) ? 0xAAAAAAAAu : 0u,
              (t % 7 == 3) ? 0x0F0F0F0Fu : 0u, what);
    if (failures != before) ++g_bad;
  }
  long l_bad = 0;
  for (int t = 0; t < l_trials; ++t) {
    const int cw = (t % 3 == 0) ? 32 : (t % 3 == 1 ? 40 : 64);
    const int ch = (t % 3 == 0) ? 32 : (t % 3 == 1 ? 33 : 32);
    const int density = 70 + static_cast<int>((t * 13) % 31);
    const int bites = (t % 4);
    const bool prefix_trial = (t % 3) == 2;
    const zt::ComposedLattice lat =
        prefix_trial ? make_lat(32, 32, prefix_mask(88 + (t % 24), 10 + (t % 15), 20))
                     : make_lat(cw, ch, pressured_mask(cw, ch, density, bites, 0xC0FFEE00u + t * 104729u));
    const bool vd_on = (t % 2) == 0;
    const std::vector<int32_t> vd = make_vdist(static_cast<size_t>(lat.w) * lat.h, 0x1234000u + t, t % 4);
    char what[48];
    std::snprintf(what, sizeof what, "lane L trial %d", t);
    const int before = failures;
    three_way(d, lat, vd_on ? vd.data() : nullptr, (t % 4 == 3) ? 0x0F0F0F0Fu : 0u,
              (t % 5 == 4) ? 0xAAAAAAAAu : 0u, what);
    if (failures != before) ++l_bad;
  }
  std::printf("  lane G mismatching lattices: %ld / %d; lane L: %ld / %d\n", g_bad, g_trials, l_bad,
              l_trials);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  Vzhao_forge_cliff golden;
  Vzhao_forge_cliff_ram cand;
  Duts d{golden, cand};

  lane_golden_fixtures(d);
  lane_row_window(d);
  lane_compaction(d);
  lane_random(d, nightly ? 600 : 150, nightly ? 240 : 60);

  // the candidate's instrument stays silent on legal stimulus (its positive
  // control is tests/mutants/zhao_forge_cliff_ram_mutant.sv)
  check(cand.walk_fault_o == 0, "candidate walk_fault_o == 0 across every legal page");
  check(cand.idle_o != 0 && golden.idle_o != 0, "both blocks idle at the end");
  check(cand.triangles_submitted_o == golden.triangles_submitted_o,
        "triangles_submitted agrees between golden and candidate");

  std::printf(
      "coverage: lattices=%ld pages=%ld merged=%ld dropped=%ld both=%ld merge_only=%ld\n"
      "          dropped_span>1=%ld spans>1=%ld span13=%ld partial=%ld cw1=%ld ch1=%ld\n"
      "          ld_stalled=%ld out_stalled=%ld vdist=%ld\n",
      cov.lattices, cov.pages, cov.merged_pages, cov.dropped_pages, cov.both, cov.merge_only,
      cov.dropped_span_gt1, cov.spans_gt1, cov.span13, cov.partial, cov.one_wide, cov.one_tall,
      cov.ld_stalled, cov.out_stalled, cov.vdist_on);
  std::printf("clocks:   golden total %ld, candidate total %ld (%+.2f%%); worst page golden %ld, "
              "candidate %ld; candidate slower on %ld / %ld lattices\n",
              cov.golden_clocks, cov.cand_clocks,
              100.0 * (static_cast<double>(cov.cand_clocks) - static_cast<double>(cov.golden_clocks)) /
                  static_cast<double>(cov.golden_clocks),
              cov.golden_worst, cov.cand_worst, cov.cand_slower_lattices, cov.lattices);

  // a check that never fired is not a check
  check(cov.merged_pages > 0, "coverage: pages with merges");
  check(cov.dropped_pages > 0, "coverage: pages with drops");
  check(cov.both > 0, "coverage: pages with BOTH degrades");
  check(cov.merge_only > 0, "coverage: pages the merge alone brings inside budget");
  check(cov.dropped_span_gt1 > 0, "coverage: a merged span dropped (R2 bodies)");
  check(cov.spans_gt1 > 0, "coverage: a merged span emitted");
  check(cov.span13 > 0, "coverage: the 20/20 need-31 prefix merge (13)");
  check(cov.partial > 0 && cov.one_wide > 0 && cov.one_tall > 0, "coverage: partial / cw=1 / ch=1 pages");
  check(cov.ld_stalled > 0 && cov.out_stalled > 0, "coverage: load and output stalls");
  check(cov.vdist_on > 0, "coverage: live vdist");

  if (failures == 0) std::printf("forge_cliff_ram_differential: all green\n");
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
