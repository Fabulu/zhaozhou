// compcache_front_rtl_directed.cpp -- TERRAIN.COMPCACHE's patch front against
// zref::terrain::ComposedLattice.
//
// THIS BLOCK IS A STORE, so the oracle is its CONTENTS AND ADDRESSING, not a
// computation. Nothing here recomputes a height; the reference lattice is
// filled with the same words the RTL was fed, and every word is read back out
// of the hardware and compared. What can go wrong in a store is the MAPPING --
// a transposed vj*33 + vi, the two surface planes swapped, an off-by-one write
// cursor, the wrong parity served -- and every one of those returns a REAL
// composed height from the wrong place. There is no picture in which a lattice
// wrong by one vertex is visible: it renders as terrain. So this compares every
// vertex of both surfaces and every cell, and never samples.
//
// ---------------------------------------------------------------------------
// THE VALUE SCHEME, AND WHY IT DISTINGUISHES
// ---------------------------------------------------------------------------
//     word(field, patch, idx) = (field << 28) | (patch << 24) | idx
//
//     field  1 = top surface, 2 = bottom surface, 3 = wx, 4 = wz
//     patch  which patch generation wrote it (A = 0, B = 1, ...)
//     idx    the linear index: vj*33 + vi for a vertex, vi for wx, vj for wz
//
// Every stored word therefore names its own field, its own patch and its own
// position, so each way this block can be wrong produces a word that is
// DIFFERENT rather than merely plausible:
//
//   * transposed addressing (vi*33 + vj) -> the low bits come back transposed:
//     wrong at all 1,056 off-diagonal vertices;
//   * the surface planes swapped -> field nibble 2 where 1 was expected, at
//     every one of the 1,089 vertices, a difference of 0x1000_0000;
//   * an off-by-one write cursor -> low bits off by one, everywhere;
//   * the wrong parity served -> the patch nibble is wrong, on every word;
//   * wx and wz swapped -> field 4 where 3 was expected;
//   * a plane still at its reset state -> 0, which this scheme never produces
//     (patch A vertex 0's top is 0x1000_0000, not 0);
//   * poison leaking into a good answer -> 0x5BADF00D, field nibble 5, which
//     the scheme never generates either.
//
// None of these claims is left as prose: the FIXTURE SELF-CHECKS below count
// how many words each perturbation actually changes and assert the counts. A
// distinguishing scheme that has not been shown to distinguish is the same
// failure as a detector that has never been shown to fire.
//
// Two bits of substance cannot carry a tag, so the cell plane uses
// substance(patch, ci, cj) = (ci + 2*cj + patch) & 3, whose distinguishing
// power is likewise measured rather than asserted (768 of 1,024 cells change
// under a transpose; all 1,024 change under a patch change).
//
// ---------------------------------------------------------------------------
// HOW THE TIMING IS PINNED
// ---------------------------------------------------------------------------
// TERRAIN.TESS reads through a REGISTERED port: the datum is present the cycle
// AFTER the request. Rather than checking that once, every comparison in this
// file is made against a request issued ONE CYCLE EARLIER -- the expectation
// stream is shifted by exactly one against the request stream. A block that
// answered combinationally, or two cycles late, or that held an answer for an
// extra cycle, mismatches on essentially every comparison rather than on a
// special case. Idle cycles are scheduled throughout and expect POISON, so an
// answer that lingers is caught as loudly as one that never arrives.
//
// ---------------------------------------------------------------------------
// BACKPRESSURE IS NOT OPTIONAL
// ---------------------------------------------------------------------------
// A sibling block's differential passed 21 checks over every input it has and
// still missed a dropped answer, because every phase drove the consumer's ready
// HIGH on every cycle. So: the record producer here STALLS (it presents nothing
// on a randomly chosen cycle, and once it presents a record it holds the
// payload stable until the block takes it), the lattice and cell request
// streams both carry gaps and deliberate repeats, and the two serve ports run
// on different periods so they are never in lockstep. Nothing may be lost and
// nothing may be duplicated: the counts of accepted records, of answers and of
// covered addresses are all asserted, not just the values.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vtb_compcache_front.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"

namespace tr = zref::terrain;

namespace {

using Top = Vtb_compcache_front;

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what, long long want = 1, long long got = 0) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %lld, got %lld)\n", what, want, got);
  }
}

// ---- geometry --------------------------------------------------------------
constexpr int kLW = 33, kLH = 33;
constexpr int kVerts = kLW * kLH;  // 1,089
constexpr int kCW = kLW - 1, kCH = kLH - 1;
constexpr int kCells = kCW * kCH;  // 1,024
constexpr int kSW = 9, kSH = 9;    // the shrunken instance
constexpr int kSVerts = kSW * kSH; // 81

constexpr uint32_t kPoison = 0x5BADF00Du;

// field tags
constexpr uint32_t kFTop = 1, kFBot = 2, kFWx = 3, kFWz = 4;

inline uint32_t word(uint32_t field, uint32_t patch, uint32_t idx) {
  return (field << 28) | (patch << 24) | idx;
}
inline uint8_t subst(uint32_t patch, int ci, int cj) {
  return static_cast<uint8_t>(
      (static_cast<uint32_t>(ci) + 2u * static_cast<uint32_t>(cj) + patch) & 3u);
}

// The reference lattice for patch generation `p`, filled with exactly the words
// the RTL is about to be fed.
tr::ComposedLattice make_lat(uint32_t p, bool dual = true, int w = kLW, int h = kLH) {
  tr::ComposedLattice L;
  L.w = w;
  L.h = h;
  L.dual = dual;
  L.wx.resize(static_cast<size_t>(w));
  L.wz.resize(static_cast<size_t>(h));
  L.top.resize(static_cast<size_t>(w) * h);
  L.bottom.resize(static_cast<size_t>(w) * h);
  L.cell_state.resize(static_cast<size_t>(w - 1) * (h - 1));
  for (int i = 0; i < w; ++i)
    L.wx[i] = static_cast<int32_t>(word(kFWx, p, static_cast<uint32_t>(i)));
  for (int j = 0; j < h; ++j)
    L.wz[j] = static_cast<int32_t>(word(kFWz, p, static_cast<uint32_t>(j)));
  for (int vj = 0; vj < h; ++vj)
    for (int vi = 0; vi < w; ++vi) {
      const size_t v = static_cast<size_t>(vj) * w + vi;
      L.top[v] = static_cast<int32_t>(word(kFTop, p, static_cast<uint32_t>(v)));
      // dual == false is the legacy single-surface page: the block resolves it
      // at capture, so the underside IS the top and the reference says so too.
      L.bottom[v] =
          dual ? static_cast<int32_t>(word(kFBot, p, static_cast<uint32_t>(v))) : L.top[v];
    }
  for (int cj = 0; cj < h - 1; ++cj)
    for (int ci = 0; ci < w - 1; ++ci)
      L.cell_state[static_cast<size_t>(cj) * (w - 1) + ci] = subst(p, ci, cj);
  return L;
}

// ---- the per-cycle plan ----------------------------------------------------
// One Step is one clock. `e*` are what the block must present ONE CYCLE LATER.
struct Step {
  uint8_t lreq = 0, vi = 0, vj = 0, surf = 0;
  uint8_t creq = 0, ci = 0, cj = 0;
  uint32_t eh = kPoison, ewx = kPoison, ewz = kPoison;
  uint8_t esub = 3;
};

// Fill in the expectation for the lattice port. L == nullptr means "no patch is
// being served", which is a poison case exactly like an out-of-range index.
void expect_lat(Step& s, const tr::ComposedLattice* L) {
  if (!s.lreq || L == nullptr || s.vi >= kLW || s.vj >= kLH) return;  // stays poison
  const size_t v = static_cast<size_t>(s.vj) * kLW + s.vi;
  s.eh = static_cast<uint32_t>(s.surf ? L->bottom[v] : L->top[v]);
  s.ewx = static_cast<uint32_t>(L->wx[s.vi]);
  s.ewz = static_cast<uint32_t>(L->wz[s.vj]);
}
void expect_cell(Step& s, const tr::ComposedLattice* L) {
  if (!s.creq || L == nullptr || s.ci >= kCW || s.cj >= kCH) return;  // stays 3
  s.esub = L->substance(s.ci, s.cj);
}

// Every vertex of both surfaces and every cell, with GAPS and REPEATS, and the
// two ports on different periods. `L == nullptr` schedules the same traffic but
// expects poison everywhere (the no-patch-served case).
struct ReadPlan {
  std::vector<Step> steps;
  int lat_reqs = 0, cell_reqs = 0, lat_gaps = 0, cell_gaps = 0;
};

ReadPlan build_full_read_plan(const tr::ComposedLattice* L, int pad_to = 0) {
  struct LR {
    bool req;
    uint8_t vi, vj, surf;
  };
  struct CR {
    bool req;
    uint8_t ci, cj;
  };
  std::vector<LR> lr;
  std::vector<CR> cr;
  ReadPlan P;

  int k = 0;
  for (int vj = 0; vj < kLH; ++vj)
    for (int vi = 0; vi < kLW; ++vi)
      for (int s = 0; s < 2; ++s) {
        // top then bottom for the SAME vertex, so consecutive requests differ
        // only in the surface bit: a serve path that failed to re-latch the
        // surface would answer with the previous plane.
        lr.push_back({true, static_cast<uint8_t>(vi), static_cast<uint8_t>(vj),
                      static_cast<uint8_t>(s)});
        ++k;
        if (k % 7 == 0) lr.push_back({false, 0, 0, 0});    // a gap: expect poison
        if (k % 13 == 0) lr.push_back(lr[lr.size() - 2]);  // ask again: expect the same
      }
  int m = 0;
  for (int cj = 0; cj < kCH; ++cj)
    for (int ci = 0; ci < kCW; ++ci) {
      cr.push_back({true, static_cast<uint8_t>(ci), static_cast<uint8_t>(cj)});
      ++m;
      if (m % 5 == 0) cr.push_back({false, 0, 0});
      if (m % 11 == 0) cr.push_back(cr[cr.size() - 2]);
    }

  size_t n = lr.size() > cr.size() ? lr.size() : cr.size();
  if (pad_to > 0 && static_cast<size_t>(pad_to) > n) n = static_cast<size_t>(pad_to);
  P.steps.resize(n + 1);  // + one trailing idle so the last answer is checked
  for (size_t i = 0; i < n; ++i) {
    Step& s = P.steps[i];
    if (i < lr.size()) {
      if (lr[i].req) {
        s.lreq = 1;
        s.vi = lr[i].vi;
        s.vj = lr[i].vj;
        s.surf = lr[i].surf;
        ++P.lat_reqs;
      } else {
        ++P.lat_gaps;
      }
    } else {
      ++P.lat_gaps;
    }
    if (i < cr.size()) {
      if (cr[i].req) {
        s.creq = 1;
        s.ci = cr[i].ci;
        s.cj = cr[i].cj;
        ++P.cell_reqs;
      } else {
        ++P.cell_gaps;
      }
    } else {
      ++P.cell_gaps;
    }
    expect_lat(s, L);
    expect_cell(s, L);
  }
  return P;
}

// ---- the record producer ---------------------------------------------------
// A CORRECT ready/valid producer that STALLS. It decides to idle BEFORE arming
// a record (a producer with nothing to send), and once armed it holds the
// payload stable and valid high until the block takes it -- deasserting valid
// after asserting it would break the contract and would also desynchronise the
// expected-record list from what the block consumed.
//
// It advances the INSTANT ready is high, which is the stimulus that catches the
// two-phase-write bug: if st_ready_o were held up across the bottom-write clock
// the producer would advance one cycle early and record v+1's TOP would land in
// vertex v's BOTTOM plane. Every height would still be a real composed height
// and every count would still match; only the underside would be wrong -- so
// this test catches it as a wrong BOTTOM SURFACE in the exhaustive readback,
// not as a wrong count.
struct Producer {
  uint32_t patch = 0;
  bool dual = true;
  int n = kVerts;
  int stall_pct = 25;

  int rn = 0;  // next record index
  bool armed = false;
  uint32_t rng = 0x1B0FC0DEu;

  int pn = 0;  // next placement write (0 .. 2*33-1)
  int cn = 0;  // next cell write

  // observations
  std::vector<uint32_t> cursor_at_take;
  std::vector<uint32_t> srcid_at_take;
  int takes = 0;
  int idle = 0;
  int ready_high_on_phase1 = 0;  // ready must be LOW on the bottom-write clock
  int cursor_step_bad = 0;
  bool took_prev = false;
  bool take_now = false;

  uint32_t nxt() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  }
  bool records_done() const { return rn >= n; }
  bool done() const { return records_done() && pn >= 2 * kLW && cn >= kCells; }

  void drive(Top& d) {
    if (!armed && rn < n) {
      if (static_cast<int>(nxt() % 100u) >= stall_pct)
        armed = true;
      else
        ++idle;
    }
    d.st_valid = armed ? 1 : 0;
    if (armed) {
      d.st_top = word(kFTop, patch, static_cast<uint32_t>(rn));
      d.st_bottom = dual ? word(kFBot, patch, static_cast<uint32_t>(rn))
                         : 0x0BADBAD0u;  // ignored when dual_i == 0; garbage on purpose
      // THE POSITIONAL CONTRACT: patch_state carries no vertex index, so the
      // write cursor IS the index. Feed the vertex index in on src_id.
      d.st_src_id = static_cast<uint16_t>(rn);
    } else {
      // Garbage on an invalid beat: a block that latched the port outside the
      // handshake would store this.
      d.st_top = 0xDEADBEEFu;
      d.st_bottom = 0xDEADBEEFu;
      d.st_src_id = 0xFFFFu;
    }

    // The placement planes and the cell plane have no handshake; one write per
    // clock, concurrently with the record stream, which is also how the real
    // producer would drive them.
    if (pn < 2 * kLW) {
      const int idx = pn % kLW;
      const int axis = pn / kLW;
      d.pos_we = 1;
      d.pos_axis = static_cast<uint8_t>(axis);
      d.pos_idx = static_cast<uint8_t>(idx);
      d.pos_val = word(axis ? kFWz : kFWx, patch, static_cast<uint32_t>(idx));
    } else {
      d.pos_we = 0;
      d.pos_idx = 0;
      d.pos_val = 0;
    }
    if (cn < kCells) {
      const int ci = cn % kCW, cj = cn / kCW;
      d.cs_we = 1;
      d.cs_w_ci = static_cast<uint8_t>(ci);
      d.cs_w_cj = static_cast<uint8_t>(cj);
      d.cs_w_substance = subst(patch, ci, cj);
    } else {
      d.cs_we = 0;
    }
  }

  void after_eval(Top& d) {
    if (took_prev && d.st_ready) ++ready_high_on_phase1;
    take_now = d.st_valid && d.st_ready;
    if (take_now) {
      const uint32_t cur = d.fill_records;
      if (cur != static_cast<uint32_t>(rn)) ++cursor_step_bad;
      cursor_at_take.push_back(cur);
      srcid_at_take.push_back(d.st_src_id);
    }
    took_prev = take_now;
  }

  void post() {
    if (take_now) {
      ++rn;
      armed = false;
      ++takes;
    }
    if (pn < 2 * kLW) ++pn;
    if (cn < kCells) ++cn;
    take_now = false;
  }
};

// ---- the cycle engine ------------------------------------------------------
struct Stats {
  long long cycles = 0;
  long long lat_cmp = 0, cell_cmp = 0;
  int h_bad = 0, wx_bad = 0, wz_bad = 0, sub_bad = 0;
  int printed = 0;
};

void run(Top& d, const std::vector<Step>& plan, Producer* prod, Stats& st, const char* tag) {
  for (size_t c = 0; c <= plan.size(); ++c) {
    const bool have = c < plan.size();
    d.lat_req = have ? plan[c].lreq : 0;
    d.lat_vi = have ? plan[c].vi : 0;
    d.lat_vj = have ? plan[c].vj : 0;
    d.lat_surface = have ? plan[c].surf : 0;
    d.cs_req = have ? plan[c].creq : 0;
    d.cs_ci = have ? plan[c].ci : 0;
    d.cs_cj = have ? plan[c].cj : 0;
    if (prod) {
      prod->drive(d);
    } else {
      d.st_valid = 0;
      d.pos_we = 0;
      d.cs_we = 0;
    }
    d.eval();
    if (prod) prod->after_eval(d);

    // The answer on the wires now belongs to the request issued LAST cycle.
    if (c > 0) {
      const Step& p = plan[c - 1];
      ++st.lat_cmp;
      ++st.cell_cmp;
      const bool hbad = d.lat_h != p.eh;
      const bool xbad = d.lat_wx != p.ewx;
      const bool zbad = d.lat_wz != p.ewz;
      const bool sbad = d.cs_substance != p.esub;
      if (hbad) ++st.h_bad;
      if (xbad) ++st.wx_bad;
      if (zbad) ++st.wz_bad;
      if (sbad) ++st.sub_bad;
      if ((hbad || xbad || zbad || sbad) && st.printed < 6) {
        ++st.printed;
        std::printf(
            "    [%s] cycle %zu: req(vi=%u,vj=%u,s=%u,lreq=%u) h %08X/%08X wx %08X/%08X "
            "wz %08X/%08X | cell(ci=%u,cj=%u,creq=%u) sub %u/%u  (rtl/oracle)\n",
            tag, c - 1, p.vi, p.vj, p.surf, p.lreq, d.lat_h, p.eh, d.lat_wx, p.ewx, d.lat_wz,
            p.ewz, p.ci, p.cj, p.creq, d.cs_substance, p.esub);
      }
    }
    zhao::tick(d);
    if (prod) prod->post();
    ++st.cycles;
  }
}

void report(const char* tag, const Stats& st) {
  std::printf(
      "  %-28s %lld cycles, %lld lattice + %lld cell comparisons -> bad h %d wx %d wz %d "
      "sub %d\n",
      tag, st.cycles, st.lat_cmp, st.cell_cmp, st.h_bad, st.wx_bad, st.wz_bad, st.sub_bad);
}

// Idle the block for `n` clocks with every one-shot input low.
void quiet(Top& d, int n = 1) {
  d.fill_start = 0;
  d.st_valid = 0;
  d.pos_we = 0;
  d.cs_we = 0;
  d.serve_release = 0;
  d.lat_req = 0;
  d.cs_req = 0;
  for (int i = 0; i < n; ++i) zhao::tick(d);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Top d;

  // =========================================================================
  // 1. THE FIXTURE, AND PROOF THAT IT DISTINGUISHES
  // =========================================================================
  const tr::ComposedLattice latA = make_lat(0);
  const tr::ComposedLattice latB = make_lat(1);
  {
    int tr_diff = 0, surf_diff = 0, cursor_diff = 0, patch_diff = 0, axis_diff = 0;
    for (int vj = 0; vj < kLH; ++vj)
      for (int vi = 0; vi < kLW; ++vi) {
        const size_t v = static_cast<size_t>(vj) * kLW + vi;
        const size_t t = static_cast<size_t>(vi) * kLW + vj;  // the transpose
        if (latA.top[v] != latA.top[t]) ++tr_diff;
        if (latA.top[v] != latA.bottom[v]) ++surf_diff;
        if (v + 1 < static_cast<size_t>(kVerts) && latA.top[v] != latA.top[v + 1]) ++cursor_diff;
        if (latA.top[v] != latB.top[v]) ++patch_diff;
      }
    for (int i = 0; i < kLW; ++i)
      if (latA.wx[i] != latA.wz[i]) ++axis_diff;
    int cell_tr = 0, cell_patch = 0;
    for (int cj = 0; cj < kCH; ++cj)
      for (int ci = 0; ci < kCW; ++ci) {
        if (latA.substance(ci, cj) != latA.substance(cj, ci)) ++cell_tr;
        if (latA.substance(ci, cj) != latB.substance(ci, cj)) ++cell_patch;
      }

    std::printf(
        "[fixture] transpose %d/1089, surfaces %d/1089, cursor+1 %d/1088, patch %d/1089, "
        "wx-vs-wz %d/33, cells transpose %d/1024, cells patch %d/1024\n",
        tr_diff, surf_diff, cursor_diff, patch_diff, axis_diff, cell_tr, cell_patch);

    ck(tr_diff == kVerts - kLW,
       "the fixture distinguishes a TRANSPOSED index at every off-diagonal vertex -- "
       "vj*33+vi and vi*33+vj name different words for all 1,056 of them",
       kVerts - kLW, tr_diff);
    ck(surf_diff == kVerts,
       "and distinguishes the two SURFACE planes at every vertex, so a swap is visible "
       "everywhere rather than at a lucky one",
       kVerts, surf_diff);
    ck(cursor_diff == kVerts - 1,
       "and an OFF-BY-ONE write cursor at every vertex it could land on", kVerts - 1,
       cursor_diff);
    ck(patch_diff == kVerts, "and the wrong PARITY served, at every vertex", kVerts, patch_diff);
    ck(axis_diff == kLW, "and wx swapped with wz, at every column", kLW, axis_diff);
    ck(cell_tr == 768,
       "the 2-bit cell plane cannot carry a tag, so its distinguishing power is MEASURED: "
       "768 of 1,024 cells change under a transposed cell index",
       768, cell_tr);
    ck(cell_patch == kCells, "and all 1,024 change between patch generations", kCells,
       cell_patch);
  }

  // =========================================================================
  // 2. RESET, AND A READ WITH NO PATCH SERVED
  // =========================================================================
  d.rst_n = 0;
  d.fill_start = 0;
  d.st_valid = 0;
  d.st_top = 0;
  d.st_bottom = 0;
  d.st_src_id = 0;
  d.pos_we = 0;
  d.pos_axis = 0;
  d.pos_idx = 0;
  d.pos_val = 0;
  d.cs_we = 0;
  d.cs_w_ci = 0;
  d.cs_w_cj = 0;
  d.cs_w_substance = 0;
  d.dual = 1;
  d.serve_release = 0;
  d.lat_req = 0;
  d.lat_vi = 0;
  d.lat_vj = 0;
  d.lat_surface = 0;
  d.cs_req = 0;
  d.cs_ci = 0;
  d.cs_cj = 0;
  d.s_fill_start = 0;
  d.s_st_valid = 0;
  d.s_st_top = 0;
  d.s_st_bottom = 0;
  d.s_dual = 0;
  d.s_cs_we = 0;
  d.s_cs_w_ci = 0;
  d.s_cs_w_cj = 0;
  d.s_cs_w_substance = 0;
  d.s_serve_release = 0;
  d.s_lat_req = 0;
  d.s_lat_vi = 0;
  d.s_lat_vj = 0;
  d.s_lat_surface = 0;
  d.s_cs_req = 0;
  d.s_cs_ci = 0;
  d.s_cs_cj = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  quiet(d, 2);

  ck(d.serve_valid == 0, "after reset no patch is available to serve", 0, d.serve_valid);
  ck(d.fill_busy == 0, "and no fill is running", 0, d.fill_busy);

  {
    // IN-RANGE requests with NOTHING SERVED. Zero would be a legal height and a
    // legal world coordinate, so the answer must be the poison a consumer can
    // recognise -- and these are in range, so they are NOT out-of-bounds and
    // must not move lat_oob.
    const uint32_t oob0 = d.lat_oob, cs0 = d.cs_oob;
    Stats st;
    std::vector<Step> plan;
    for (int i = 0; i < 40; ++i) {
      Step s;
      s.lreq = 1;
      s.vi = static_cast<uint8_t>(i % kLW);
      s.vj = static_cast<uint8_t>((i * 7) % kLH);
      s.creq = 1;
      s.ci = static_cast<uint8_t>(i % kCW);
      s.cj = static_cast<uint8_t>((i * 5) % kCH);
      expect_lat(s, nullptr);   // poison
      expect_cell(s, nullptr);  // substance 3
      plan.push_back(s);
    }
    run(d, plan, nullptr, st, "unserved");
    ck(st.h_bad == 0 && st.wx_bad == 0 && st.wz_bad == 0,
       "a lattice read with NO PATCH SERVED returns 0x5BADF00D on h, wx and wz -- poison, "
       "not zero, because zero is a legal height AND a legal world coordinate",
       0, st.h_bad + st.wx_bad + st.wz_bad);
    ck(st.sub_bad == 0,
       "and a cell read returns substance 3, which sec 3.3 makes the non-dangerous encoding "
       "(0 = SOLID), the counter being the alarm rather than the value",
       0, st.sub_bad);
    ck(d.lat_oob == oob0 && d.cs_oob == cs0,
       "and none of it is counted out-of-bounds: these indices are inside the grid, they "
       "simply have nothing to answer from",
       0, static_cast<long long>((d.lat_oob - oob0) + (d.cs_oob - cs0)));
  }

  // =========================================================================
  // 3. FILL PATCH A -- with a stalling producer
  // =========================================================================
  Producer pa;
  pa.patch = 0;
  pa.dual = true;
  pa.stall_pct = 25;
  {
    d.dual = 1;
    d.fill_start = 1;
    d.eval();
    ck(d.fill_accept == 1, "fill_start with no fill running is accepted", 1, d.fill_accept);
    zhao::tick(d);
    d.fill_start = 0;
    d.eval();
    ck(d.fill_busy == 1, "and the fill is then busy", 1, d.fill_busy);
    ck(d.fill_accept == 0, "with fill_accept a ONE-CYCLE pulse", 0, d.fill_accept);
    ck(d.fill_records == 0, "and the write cursor back at vertex zero", 0, d.fill_records);

    // No serve traffic during the fill: this phase is about the fill.
    std::vector<Step> idleplan(4200);
    Stats st;
    run(d, idleplan, &pa, st, "fillA");

    ck(pa.takes == kVerts, "every one of the 1,089 records was accepted exactly once", kVerts,
       pa.takes);
    ck(pa.idle > 100,
       "and the producer genuinely stalled -- it presented nothing on hundreds of cycles, so "
       "the fill path was exercised under backpressure rather than at full rate",
       1, pa.idle);
    ck(pa.ready_high_on_phase1 == 0,
       "st_ready_o is LOW on the second write clock of every record: one record is TWO write "
       "clocks and a producer that advanced on the bottom-write clock would put the next "
       "record's TOP into this vertex's BOTTOM plane",
       0, pa.ready_high_on_phase1);
    ck(pa.cursor_step_bad == 0,
       "and the write cursor advanced by exactly one per record, never by two and never not "
       "at all",
       0, pa.cursor_step_bad);

    // THE POSITIONAL CONTRACT, pinned to a VALUE. patch_state carries no vertex
    // index, so the cursor IS the index; the vertex index was driven in on
    // st_src_id_i and is compared against the cursor the record landed at.
    int srcid_bad = 0;
    for (size_t k = 0; k < pa.cursor_at_take.size(); ++k)
      if (pa.srcid_at_take[k] != static_cast<uint32_t>(k) ||
          pa.cursor_at_take[k] != static_cast<uint32_t>(k))
        ++srcid_bad;
    ck(srcid_bad == 0,
       "the record whose src_id said 'vertex k' was taken at cursor k, for all 1,089 -- the "
       "order-is-the-index law pinned to a VALUE rather than to a count",
       0, srcid_bad);

    ck(d.fill_records == kVerts, "the fill took exactly 1,089 records", kVerts, d.fill_records);
    ck(d.patches_filled == 1, "one patch was filled", 1, d.patches_filled);
    ck(d.serve_valid == 1, "and it handed over immediately, because nothing was being served",
       1, d.serve_valid);
    ck(d.fill_busy == 0, "the fill is finished", 0, d.fill_busy);
    ck(d.patches_served == 0, "and nothing has been retired yet", 0, d.patches_served);
    ck(d.fill_overrun == 0, "with no refused records", 0, d.fill_overrun);
    ck(pa.done(), "the placement planes and the cell plane were fully written too", 1,
       pa.done() ? 1 : 0);
  }

  // =========================================================================
  // 4. READ BACK EVERY VERTEX OF BOTH SURFACES AND EVERY CELL
  // =========================================================================
  {
    const ReadPlan P = build_full_read_plan(&latA);
    Stats st;
    run(d, P.steps, nullptr, st, "readA");
    report("patch A, exhaustive:", st);
    std::printf("    %d lattice requests (%d idle cycles), %d cell requests (%d idle)\n",
                P.lat_reqs, P.lat_gaps, P.cell_reqs, P.cell_gaps);

    ck(P.lat_reqs >= 2 * kVerts,
       "the plan really did ask for all 2,178 vertex-surfaces (and then some, on purpose)",
       2 * kVerts, P.lat_reqs);
    ck(P.cell_reqs >= kCells, "and all 1,024 cells", kCells, P.cell_reqs);
    ck(P.lat_gaps > 200 && P.cell_gaps > 100,
       "with hundreds of IDLE cycles scheduled on both ports, each of which must answer "
       "poison -- an answer that lingers is as wrong as one that never arrives",
       1, (P.lat_gaps > 200 && P.cell_gaps > 100) ? 1 : 0);
    ck(st.h_bad == 0,
       "every height of both surfaces matches zref::terrain::ComposedLattice exactly -- the "
       "whole lattice, not a sample, because a lattice wrong by one vertex renders as terrain",
       0, st.h_bad);
    ck(st.wx_bad == 0,
       "and wx comes from the COLUMN plane at every vertex (33 numbers, not 1,089 copies)", 0,
       st.wx_bad);
    ck(st.wz_bad == 0, "and wz from the ROW plane", 0, st.wz_bad);
    ck(st.sub_bad == 0,
       "and every one of the 1,024 cells returns its own substance from the layer-D plane", 0,
       st.sub_bad);
  }

  // =========================================================================
  // 5. THE REGISTERED PORT: the datum is present the cycle AFTER the request
  // =========================================================================
  // Every comparison above is already shifted by exactly one cycle, so a
  // combinational or two-cycle answer would have failed 3,000 times over. This
  // pins it explicitly, and in both directions, on one named vertex.
  {
    const int vi = 7, vj = 19, surf = 1;
    const size_t v = static_cast<size_t>(vj) * kLW + vi;
    const uint32_t want = static_cast<uint32_t>(latA.bottom[v]);
    d.lat_req = 1;
    d.lat_vi = vi;
    d.lat_vj = vj;
    d.lat_surface = surf;
    d.eval();
    ck(d.lat_h == kPoison,
       "on the cycle the request is PRESENTED the port still shows the previous answer "
       "(poison, from the idle cycle before) -- the port is registered, not combinational",
       static_cast<long long>(kPoison), d.lat_h);
    zhao::tick(d);
    d.lat_req = 0;
    d.eval();
    ck(d.lat_h == want, "the cycle AFTER the request the datum is present",
       static_cast<long long>(want), d.lat_h);
    ck(d.lat_wx == static_cast<uint32_t>(latA.wx[vi]) &&
           d.lat_wz == static_cast<uint32_t>(latA.wz[vj]),
       "with its wx and wz alongside it, on the same cycle", 1,
       (d.lat_wx == static_cast<uint32_t>(latA.wx[vi])) ? 1 : 0);
    zhao::tick(d);
    d.eval();
    ck(d.lat_h == kPoison,
       "and it is GONE the cycle after that -- the answer is not held, so a consumer cannot "
       "read a stale datum by forgetting to ask",
       static_cast<long long>(kPoison), d.lat_h);
  }

  // =========================================================================
  // 6. POISON AND THE OUT-OF-RANGE COUNTER
  // =========================================================================
  {
    const uint32_t oob0 = d.lat_oob;
    std::vector<Step> plan;
    int oob_n = 0;
    const int bad_vi[] = {33, 40, 63, 5, 33};
    const int bad_vj[] = {5, 33, 63, 33, 40};
    for (int i = 0; i < 5; ++i) {
      Step s;
      s.lreq = 1;
      s.vi = static_cast<uint8_t>(bad_vi[i]);
      s.vj = static_cast<uint8_t>(bad_vj[i]);
      s.surf = static_cast<uint8_t>(i & 1);
      expect_lat(s, &latA);  // out of range -> stays poison
      plan.push_back(s);
      ++oob_n;
      Step g;  // an in-range read between them, to prove the poison is per-request
      g.lreq = 1;
      g.vi = 3;
      g.vj = 4;
      g.surf = 0;
      expect_lat(g, &latA);
      plan.push_back(g);
    }
    Stats st;
    run(d, plan, nullptr, st, "oob");
    ck(st.h_bad == 0 && st.wx_bad == 0 && st.wz_bad == 0,
       "a request OUTSIDE the grid returns 0x5BADF00D on h, wx and wz, and the in-range reads "
       "interleaved with them still answer correctly",
       0, st.h_bad + st.wx_bad + st.wz_bad);
    ck(d.lat_oob - oob0 == static_cast<uint32_t>(oob_n),
       "and each one is counted exactly once by lat_oob_o", oob_n,
       static_cast<long long>(d.lat_oob - oob0));
  }
  ck(d.cs_oob == 0,
     "cs_oob_o is still zero at 33 x 33 -- and it MUST be: cs_ci_i/cs_cj_i are 5-bit ports "
     "over a 32 x 32 cell plane, so no value a consumer can present is out of range. A "
     "non-zero here means the range test is broken, which is exactly how the shipped draft "
     "failed. The alarm itself is proved to fire on the 9 x 9 instance below",
     0, d.cs_oob);

  // =========================================================================
  // 7. A STRAY PLACEMENT / CELL WRITE WITH NO FILL OPEN
  // =========================================================================
  // The record port cannot reach the served buffer (st_ready_o gates on
  // fill_active_q), but pos_we_i and cs_we_i are not gated at all, and their
  // port comments invite a write "before ... the record stream". So: write
  // garbage into both while NO fill is open, then read the whole served patch
  // again. Nothing may have moved.
  {
    for (int i = 0; i < kCells; ++i) {
      d.pos_we = 1;
      d.pos_axis = static_cast<uint8_t>((i >> 5) & 1);
      d.pos_idx = static_cast<uint8_t>(i % kLW);
      d.pos_val = 0xBADBAD00u | static_cast<uint32_t>(i % kLW);
      d.cs_we = 1;
      d.cs_w_ci = static_cast<uint8_t>(i % kCW);
      d.cs_w_cj = static_cast<uint8_t>(i / kCW);
      d.cs_w_substance = static_cast<uint8_t>(3 - subst(0, i % kCW, i / kCW));
      zhao::tick(d);
    }
    d.pos_we = 0;
    d.cs_we = 0;
    quiet(d, 2);

    const ReadPlan P = build_full_read_plan(&latA);
    Stats st;
    run(d, P.steps, nullptr, st, "afterStray");
    report("patch A after stray writes:", st);
    ck(st.h_bad == 0 && st.wx_bad == 0 && st.wz_bad == 0 && st.sub_bad == 0,
       "a placement or cell write with NO FILL OPEN cannot touch the patch being served -- "
       "the block claims a fill can never land on the lattice a tessellator is reading, and "
       "these two ports are the ones that are not gated by the record handshake",
       0, st.h_bad + st.wx_bad + st.wz_bad + st.sub_bad);
  }

  // =========================================================================
  // 8. DOUBLE BUFFERING: fill patch B while patch A is being served
  // =========================================================================
  Producer pb;
  pb.patch = 1;
  pb.dual = true;
  pb.stall_pct = 15;
  {
    d.fill_start = 1;
    d.eval();
    ck(d.fill_accept == 1, "a second fill starts while the first patch is still being served",
       1, d.fill_accept);
    ck(d.serve_valid == 1, "with that patch still available throughout", 1, d.serve_valid);
    zhao::tick(d);
    d.fill_start = 0;

    // Read the WHOLE served patch while the other one fills, both ports busy.
    const ReadPlan P = build_full_read_plan(&latA, 4200);
    Stats st;
    run(d, P.steps, &pb, st, "AduringB");
    report("patch A during B's fill:", st);

    ck(pb.takes == kVerts, "all 1,089 records of patch B were accepted", kVerts, pb.takes);
    ck(pb.ready_high_on_phase1 == 0, "with ready still low on every bottom-write clock", 0,
       pb.ready_high_on_phase1);
    ck(st.h_bad == 0 && st.wx_bad == 0 && st.wz_bad == 0 && st.sub_bad == 0,
       "and every height, world coordinate and cell of patch A read back UNCHANGED while "
       "patch B was landing -- 3,200-odd comparisons of a lattice being read while another "
       "is written, which is the entire point of double buffering",
       0, st.h_bad + st.wx_bad + st.wz_bad + st.sub_bad);
    ck(d.fill_done == 1, "patch B's fill is complete", 1, d.fill_done);
    ck(d.fill_busy == 1,
       "but the buffer is NOT handed over: a finished fill waits for the serve side to be "
       "released rather than snatching the lattice out from under a tessellator",
       1, d.fill_busy);
    ck(d.patches_filled == 1, "so patches_filled has not moved yet", 1, d.patches_filled);
    ck(d.serve_valid == 1, "and patch A is still the one being served", 1, d.serve_valid);
  }

  // =========================================================================
  // 9. THE REFUSED 1,090th RECORD
  // =========================================================================
  {
    ck(d.st_ready == 0, "at capacity the fill port stops accepting", 0, d.st_ready);
    const uint32_t ov0 = d.fill_overrun;
    d.st_valid = 1;
    d.st_top = 0x0BAD0BADu;
    d.st_bottom = 0x0BAD0BADu;
    d.st_src_id = 0xFFFFu;
    d.eval();
    ck(d.st_ready == 0, "and refuses the 1,090th record rather than taking it", 0, d.st_ready);
    zhao::tick(d);
    d.st_valid = 0;
    d.eval();
    ck(d.fill_overrun - ov0 == 1, "which is counted by fill_overrun_o", 1,
       static_cast<long long>(d.fill_overrun - ov0));
    ck(d.fill_records == kVerts,
       "and the write cursor STOPPED at 1,089 rather than wrapping onto vertex zero -- a "
       "wrap would have overwritten the patch's first vertex with a record from nowhere",
       kVerts, d.fill_records);
  }

  // =========================================================================
  // 10. A RELEASE ON THE SAME CLOCK AS A HANDOVER
  // =========================================================================
  // The steady state: a fill finishes exactly as the served patch is released,
  // every patch. Both events land on ONE clock here, deliberately, because
  // folding patches_served into the else-arm of the handover would lose exactly
  // this case -- and only this case, i.e. every patch when the pipeline works.
  {
    ck(d.fill_busy == 1 && d.fill_done == 1 && d.serve_valid == 1,
       "the simultaneous case is set up: a completed fill waiting, and a patch being served",
       1, (d.fill_busy && d.fill_done && d.serve_valid) ? 1 : 0);
    const uint32_t served0 = d.patches_served;
    d.serve_release = 1;
    d.eval();
    zhao::tick(d);
    d.serve_release = 0;
    d.eval();
    ck(d.patches_served - served0 == 1,
       "the retired patch IS counted even though the same clock handed a new buffer over -- "
       "the count that a folded else-arm would have dropped at exactly the busiest moment",
       1, static_cast<long long>(d.patches_served - served0));
    ck(d.patches_filled == 2, "the finished fill is counted too", 2, d.patches_filled);
    ck(d.serve_valid == 1, "and a patch is available on the very next cycle -- no gap", 1,
       d.serve_valid);
    ck(d.fill_busy == 0, "with the fill side now free", 0, d.fill_busy);
  }

  // =========================================================================
  // 11. THE SWAP REALLY HAPPENED: read back every word of patch B
  // =========================================================================
  {
    const ReadPlan P = build_full_read_plan(&latB);
    Stats st;
    run(d, P.steps, nullptr, st, "readB");
    report("patch B, exhaustive:", st);
    ck(st.h_bad == 0 && st.wx_bad == 0 && st.wz_bad == 0 && st.sub_bad == 0,
       "after the swap every vertex, both surfaces, both placement planes and all 1,024 cells "
       "come from patch B -- the parity moved, all of it, and the patch nibble in every word "
       "says so",
       0, st.h_bad + st.wx_bad + st.wz_bad + st.sub_bad);
  }

  // =========================================================================
  // 12. RELEASING THE LAST PATCH LEAVES NOTHING TO SERVE
  // =========================================================================
  {
    const uint32_t served0 = d.patches_served;
    d.serve_release = 1;
    d.eval();
    zhao::tick(d);
    d.serve_release = 0;
    d.eval();
    ck(d.patches_served - served0 == 1, "a release with no fill waiting is counted too", 1,
       static_cast<long long>(d.patches_served - served0));
    ck(d.serve_valid == 0, "and leaves nothing available to serve", 0, d.serve_valid);

    std::vector<Step> plan;
    for (int i = 0; i < 8; ++i) {
      Step s;
      s.lreq = 1;
      s.vi = static_cast<uint8_t>(i);
      s.vj = static_cast<uint8_t>(i);
      s.creq = 1;
      s.ci = static_cast<uint8_t>(i);
      s.cj = static_cast<uint8_t>(i);
      expect_lat(s, nullptr);
      expect_cell(s, nullptr);
      plan.push_back(s);
    }
    Stats st;
    run(d, plan, nullptr, st, "released");
    ck(st.h_bad == 0 && st.wx_bad == 0 && st.wz_bad == 0 && st.sub_bad == 0,
       "and reads go back to poison the moment the patch is released, rather than continuing "
       "to serve a lattice nobody owns",
       0, st.h_bad + st.wx_bad + st.wz_bad + st.sub_bad);
  }

  ck(d.patches_filled == 2 && d.patches_served == 2 && d.fill_overrun == 1,
     "final ledger: 2 filled, 2 served, 1 refused record", 1,
     (d.patches_filled == 2 && d.patches_served == 2 && d.fill_overrun == 1) ? 1 : 0);

  // =========================================================================
  // 13. THE 9 x 9 INSTANCE: cs_oob, and the legacy single-surface page
  // =========================================================================
  // cs_oob_o cannot be made to count at 33 x 33 (5-bit ports over a 32 x 32
  // plane), and a counter that cannot be made to count is a detector that has
  // never fired. At 9 x 9 the cell plane is 8 x 8, so ci >= 8 IS out of range.
  // This instance also runs dual_i = 0, the legacy single-surface page whose
  // underside IS its top.
  {
    d.s_dual = 0;
    d.s_fill_start = 1;
    zhao::tick(d);
    d.s_fill_start = 0;

    int sn = 0, scn = 0, takes = 0, ready_phase1 = 0;
    bool prev_take = false;
    for (int cyc = 0; cyc < 4000 && (sn < kSVerts || scn < 64); ++cyc) {
      d.s_st_valid = (sn < kSVerts) ? 1 : 0;
      d.s_st_top = word(kFTop, 2, static_cast<uint32_t>(sn));
      d.s_st_bottom = 0x0BADBAD0u;  // dual = 0: this must be IGNORED
      if (scn < 64) {
        d.s_cs_we = 1;
        d.s_cs_w_ci = static_cast<uint8_t>(scn % 8);
        d.s_cs_w_cj = static_cast<uint8_t>(scn / 8);
        d.s_cs_w_substance = subst(2, scn % 8, scn / 8);
      } else {
        d.s_cs_we = 0;
      }
      d.eval();
      if (prev_take && d.s_st_ready) ++ready_phase1;
      const bool take = d.s_st_valid && d.s_st_ready;
      prev_take = take;
      zhao::tick(d);
      if (take) {
        ++sn;
        ++takes;
      }
      if (scn < 64) ++scn;
    }
    d.s_st_valid = 0;
    d.s_cs_we = 0;
    // A FILL IS NOT FINISHED WHEN ITS LAST RECORD IS TAKEN. The acceptance is
    // the TOP write; the bottom lands on the next clock, the cursor reaches
    // capacity at the end of that one, and the handover is the clock after
    // that. Settling for one cycle here (which is what the first draft of this
    // loop did) reported "it never handed the patch over" and one poisoned
    // vertex in the readback below -- a bench artefact that looks exactly like
    // a swap bug. Three clocks are needed; four are taken.
    for (int i = 0; i < 4; ++i) zhao::tick(d);

    ck(takes == kSVerts,
       "the 9 x 9 instance filled with 81 records -- the LAT_W/LAT_H parameters the module "
       "offers a test are real",
       kSVerts, takes);
    ck(ready_phase1 == 0, "with the same two-clock write discipline", 0, ready_phase1);
    ck(d.s_serve_valid == 1, "and handed the patch over", 1, d.s_serve_valid);

    // dual = 0: the underside IS the top, resolved at capture.
    int sbad = 0;
    for (int vj = 0; vj < kSH; ++vj)
      for (int vi = 0; vi < kSW; ++vi)
        for (int s = 0; s < 2; ++s) {
          d.s_lat_req = 1;
          d.s_lat_vi = static_cast<uint8_t>(vi);
          d.s_lat_vj = static_cast<uint8_t>(vj);
          d.s_lat_surface = static_cast<uint8_t>(s);
          zhao::tick(d);
          d.s_lat_req = 0;
          d.eval();
          if (d.s_lat_h != word(kFTop, 2, static_cast<uint32_t>(vj * kSW + vi))) ++sbad;
          zhao::tick(d);
        }
    ck(sbad == 0,
       "on a dual_i = 0 legacy page the BOTTOM surface reads back equal to the top for all 81 "
       "vertices, resolved at capture so the serve side never has to know which kind of page "
       "it holds -- and the 0x0BADBAD0 driven on st_bottom_i was correctly ignored",
       0, sbad);

    const uint32_t coob0 = d.s_cs_oob;
    int csbad = 0;
    for (int cj = 0; cj < 8; ++cj)
      for (int ci = 0; ci < 8; ++ci) {
        d.s_cs_req = 1;
        d.s_cs_ci = static_cast<uint8_t>(ci);
        d.s_cs_cj = static_cast<uint8_t>(cj);
        zhao::tick(d);
        d.s_cs_req = 0;
        d.eval();
        if (d.s_cs_substance != subst(2, ci, cj)) ++csbad;
        zhao::tick(d);
      }
    ck(csbad == 0, "all 64 cells of the 9 x 9 plane read back correctly", 0, csbad);
    ck(d.s_cs_oob == coob0, "with none of those in-range reads counted out of bounds", 0,
       static_cast<long long>(d.s_cs_oob - coob0));

    int poison_bad = 0, n_oob = 0;
    for (int ci = 8; ci < 32; ++ci) {
      d.s_cs_req = 1;
      d.s_cs_ci = static_cast<uint8_t>(ci);
      d.s_cs_cj = 3;
      zhao::tick(d);
      d.s_cs_req = 0;
      d.eval();
      if (d.s_cs_substance != 3) ++poison_bad;
      ++n_oob;
      zhao::tick(d);
    }
    ck(poison_bad == 0,
       "a cell request outside the plane returns substance 3 -- there is no spare 2-bit "
       "encoding for poison and inventing one would change TESS's port, so 3 (never SOLID) is "
       "the value and the counter is the alarm",
       0, poison_bad);
    ck(d.s_cs_oob - coob0 == static_cast<uint32_t>(n_oob),
       "and cs_oob_o counts every one of them: the alarm that cannot be made to sound at "
       "33 x 33 is shown to sound here",
       n_oob, static_cast<long long>(d.s_cs_oob - coob0));

    const uint32_t loob0 = d.s_lat_oob;
    int lpoison_bad = 0;
    for (int vi = 9; vi < 20; ++vi) {
      d.s_lat_req = 1;
      d.s_lat_vi = static_cast<uint8_t>(vi);
      d.s_lat_vj = 2;
      d.s_lat_surface = 0;
      zhao::tick(d);
      d.s_lat_req = 0;
      d.eval();
      if (d.s_lat_h != kPoison) ++lpoison_bad;
      zhao::tick(d);
    }
    ck(lpoison_bad == 0, "and a 9 x 9 lattice request at vi >= 9 is poisoned", 0, lpoison_bad);
    ck(d.s_lat_oob - loob0 == 11, "and counted", 11,
       static_cast<long long>(d.s_lat_oob - loob0));
  }

  std::printf(
      "  counters: filled %u served %u overrun %u lat_oob %u cs_oob %u | small: lat_oob %u "
      "cs_oob %u\n",
      d.patches_filled, d.patches_served, d.fill_overrun, d.lat_oob, d.cs_oob, d.s_lat_oob,
      d.s_cs_oob);

  if (g_fail) {
    std::printf("[compcache_front_rtl_directed] %d of %d checks FAILED\n", g_fail, g_checks);
    zhao::exit_hard(1);
  }
  std::printf("[compcache_front_rtl_directed] %d checks passed\n", g_checks);
  zhao::exit_hard(0);
}
