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

// ---- the source-id scheme --------------------------------------------------
//     src_id(patch, k) = (patch << 11) | k
// The low 11 bits carry the record index, which is what pins the
// order-is-the-index law on the FILL side (the cursor the record lands at must
// equal the index the record names). The high bits carry the patch generation,
// and they are the half that makes the SERVE-side readback able to
// DISTINGUISH: with a bare index every patch's last record would carry 1,088,
// so `serve_src_id_o` would read the same value for every generation and a
// src slot that was never double-buffered -- the exact mistake a single
// `last_src_id_o` flop would be -- would sail through. A tag that cannot tell
// two patches apart is not a tag.
constexpr uint32_t kSrcNone = 0xF00Du;  // the block's "no patch is served" tell

inline uint16_t src_id(uint32_t patch, uint32_t k) {
  return static_cast<uint16_t>((patch << 11) | (k & 0x7FFu));
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
      d.st_src_id = src_id(patch, static_cast<uint32_t>(rn));
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


// ============================================================================
// THE RANDOMISED PHASE
// ============================================================================
// WHAT IT IS FOR. Everything above is exhaustive-directed: every vertex, every
// cell, every counter, each inside a stage this file wrote by hand and put in
// an order it chose. That is strong about VALUES and weak about COINCIDENCE. A
// hand-written stage places a fill_start, a record acceptance, a handover, a
// release edge and a lattice request at the few relative offsets its author
// thought of; it cannot place them at all of them. This phase does, by driving
// every port every cycle from one deterministic stream and checking every
// output every cycle against an expectation maintained from the STIMULUS --
// never from the block's own signals.
//
// WHAT IT CAN CATCH THAT THE DIRECTED PHASE CANNOT.
//   * A HANDOVER LANDING BETWEEN A REQUEST AND ITS ANSWER. The serve port is
//     registered: the address is formed on one clock from serve_par_q and the
//     datum arrives on the next. Above, the swap never falls inside that
//     window, because the directed stages read and swap in separate blocks.
//     Here it does, and an answer returned from the new parity would be a real
//     composed height from the wrong patch -- the failure this whole file
//     exists for, because it renders.
//   * A RELEASE HELD ACROSS A HANDOVER BY ACCIDENT. Section 13 places that by
//     hand. Here the release pulses drift against the fills, so the straddle
//     happens on its own, at widths and offsets nobody chose.
//   * NON-ADJACENT ADDRESSING. build_full_read_plan walks vi then vj in raster
//     order, so consecutive requests differ by one. An address path that only
//     works for +1 -- a latched high bit, a carry that happens to be right for
//     the next vertex -- survives the exhaustive read and dies here, where the
//     requests hop.
//   * A STICKY POISON. The directed out-of-range section alternates bad and
//     good on a fixed rhythm. Here they interleave at random, so an
//     out-of-range request that poisons the NEXT answer, or an in-range one
//     that clears a flag it should not, has nowhere to hide.
//   * A SWAP THAT WORKS ONCE. Above there are three generations in a fixed
//     A-B-C order at a fixed dual_i. Here there are six, with dual and source
//     ids varying, so a src slot written but never rewritten, or a parity that
//     alternates correctly only from one starting foot, shows up.
//   * A PRODUCER THAT NEVER LOWERS VALID. Section 12 withdraws between offers.
//     One generation here streams at full rate and holds st_valid_i high
//     straight through its last acceptance into the refusal -- the case a
//     rising-edge overrun detector would silently score as zero.
//   * A COUNTER THAT IS WRONG IN THE MIDDLE. The directed phase reads counters
//     at the ends of stages. This one compares all seven, on both instances,
//     on every cycle, so a counter that is briefly wrong and right again by
//     the time anybody looks is caught.
//
// THE DRAW SPACE, AND BEWARE THE LOW BITS. Every value comes from the HIGH
// half of a 64-bit LCG (`s >> 33`) and never from the raw word: an LCG's low
// bit has period 2, bit 1 period 4, and `% n` on the raw state is a draw from
// those. A sibling lane's "randomised" phase this session reported 240 windows
// and tested four distinct cases in exactly that way. Reassurance is not
// evidence, so this phase MEASURES what it drew -- full histograms over vi, vj,
// ci, cj, the surface bit, the release widths, the producer stall modes and the
// dual values, printed with their minimum and maximum bucket populations -- and
// asserts that every bucket was hit and none is starved.
//
// AND WHAT IS NOT DRAWN IS SAID SO. Three things are PINNED rather than drawn,
// because a phase that can lose a whole case to a change of seed is a phase
// whose coverage is luck: generation 0 is dual, generation 1 is not;
// generation 1's producer never lowers valid; and the scheduled release widths
// cycle 1,2,3,4,5 rather than being drawn five times from five values. The
// TIMING of all of it is drawn. Everything else is.
constexpr int kNGen = 6;  // patch generations
inline uint32_t gen_tag(int g) { return static_cast<uint32_t>(4 + g); }
constexpr uint64_t kSeed = 0x5EEDC0FFEE1234ADull;

struct Rnd {
  uint64_t s;
  explicit Rnd(uint64_t seed) : s(seed) {}
  // The high 31 bits of the state, never the low ones. See above.
  uint32_t next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<uint32_t>(s >> 33);
  }
  uint32_t below(uint32_t n) { return next() % n; }
  bool chance(int pct) { return static_cast<int>(next() % 100u) < pct; }
};

void random_phase(Top& d, const tr::ComposedLattice& latS) {
  Rnd rg(kSeed);

  std::vector<tr::ComposedLattice> L;
  std::vector<bool> Ldual;
  for (int g = 0; g < kNGen; ++g) {
    const bool du = (g == 0) ? true : (g == 1) ? false : rg.chance(50);
    Ldual.push_back(du);
    L.push_back(make_lat(gen_tag(g), du));
  }

  // ---- the model: maintained from the STIMULUS -----------------------------
  bool m_fill = false, m_serve = false, m_wphase = false, m_dual = true;
  // THE CURSOR DOES NOT GO HOME BY ITSELF. Nothing clears wcur_q but the next
  // fill_start_i -- not the handover, not the release -- so at the top of this
  // phase it is still parked at 1,089 where section 13's fill left it, and a
  // model that started it at zero would disagree with the block for exactly as
  // long as the first randomised start_wait. It did, on the first run of this
  // phase, for the eight cycles that wait happened to last. Seeding from the
  // port is right, and the disagreement was the model's.
  int m_wcur = static_cast<int>(d.fill_records), m_fillgen = -1, m_servegen = -1;
  uint32_t m_fillsrc = kSrcNone, m_servesrc = kSrcNone;
  bool m_relprev = false;
  long long m_filled = 0, m_served = 0, m_overrun = 0, m_latoob = 0, m_csoob = 0;
  long long m_slatoob = 0, m_scsoob = 0;

  const uint32_t b_filled = d.patches_filled, b_served = d.patches_served,
                 b_overrun = d.fill_overrun, b_latoob = d.lat_oob, b_csoob = d.cs_oob,
                 b_slatoob = d.s_lat_oob, b_scsoob = d.s_cs_oob;

  // ---- the director --------------------------------------------------------
  int gen = 0, rec = 0, pn = 0, cn = 0;
  int start_wait = 5 + static_cast<int>(rg.below(20));
  bool armed = false;
  int stall_pct = 25, lat_pct = 80, cell_pct = 60;
  bool cont_offer = false;  // this generation's producer never lowers valid
  int over_hold = 0, over_gap = 0;
  int rel_hold = 0, rel_wait = 0;
  bool rel_armed = false;
  bool valid_prev = false, take_prev = false, run_counted = false;

  // ---- measurements --------------------------------------------------------
  long long cycles = 0, lat_reqs = 0, lat_gaps = 0, lat_in = 0, lat_out = 0;
  long long cell_reqs = 0, cell_gaps = 0, takes = 0, prod_idle = 0;
  long long refused_cycles = 0, release_cycles = 0, releases = 0, spurious = 0, handovers = 0;
  long long stray_writes = 0, s_lat_in = 0, s_lat_out = 0, s_cs_in = 0, s_cs_out = 0;
  long long co_req_handover = 0, co_take_req = 0, co_start_req = 0, co_handover_relhigh = 0;
  long long co_req_relhigh = 0, co_req_unserved = 0;
  int vi_hist[64] = {0}, vj_hist[64] = {0}, ci_hist[32] = {0}, cj_hist[32] = {0};
  int surf_hist[2] = {0}, dual_hist[2] = {0}, relw_hist[8] = {0}, stallp_hist[8] = {0};
  int bad_ready = 0, bad_busy = 0, bad_done = 0, bad_valid = 0, bad_accept = 0;
  int bad_records = 0, bad_srcid = 0, bad_h = 0, bad_wx = 0, bad_wz = 0, bad_sub = 0;
  int bad_sh = 0, bad_ssub = 0, bad_sctr = 0;
  // PER COUNTER, not one lumped total. A single `bad_ctr` says the ledger
  // disagreed and nothing about which instrument is broken, which is most of
  // the work; and the first divergence cycle is what turns a count into a
  // waveform to go and look at.
  int bad_filled = 0, bad_served = 0, bad_overrun = 0, bad_latoob = 0, bad_csoob = 0;
  long long first_ctr_bad = -1;
  int placement_late = 0, printed = 0;
  std::vector<uint8_t> cov(static_cast<size_t>(2 * kVerts), 0);

  // one-cycle-delayed expectations for the registered read ports
  uint32_t e_h = kPoison, e_wx = kPoison, e_wz = kPoison, e_sh = kPoison;
  uint8_t e_sub = 3, e_ssub = 3;

  const long long kMaxCycles = 80000;
  long long tail = 0;
  for (long long c = 0; c < kMaxCycles; ++c) {
    if (m_filled >= kNGen && ++tail > 400) break;

    // ---------------- 1. draw the stimulus --------------------------------
    const int fgen = (gen < kNGen) ? gen : kNGen - 1;
    const bool at_cap = m_fill && m_wcur == kVerts && !m_wphase;

    uint8_t fill_start = 0;
    if (!m_fill && gen < kNGen) {
      if (start_wait > 0) --start_wait;
      else fill_start = 1;
    }

    uint8_t st_valid = 0;
    uint32_t st_top = 0xDEADBEEFu, st_bot = 0xDEADBEEFu;
    uint16_t st_src = 0xFFFFu;
    if (m_fill && rec < kVerts) {
      if (!armed) {
        if (static_cast<int>(rg.next() % 100u) >= stall_pct) armed = true;
        else ++prod_idle;
      }
      if (armed) {
        st_valid = 1;
        st_top = word(kFTop, gen_tag(m_fillgen), static_cast<uint32_t>(rec));
        st_bot = m_dual ? word(kFBot, gen_tag(m_fillgen), static_cast<uint32_t>(rec))
                        : 0x0BADBAD0u;  // ignored when dual is 0: garbage on purpose
        st_src = src_id(gen_tag(m_fillgen), static_cast<uint32_t>(rec));
      }
    } else if (m_fill) {
      // Every record has been sent; the fill is at or one clock from capacity
      // and the swap is blocked until the consumer releases. A producer with
      // more to say keeps saying it -- in runs, with random withdrawals, and
      // for generation 1 without ever lowering valid at all.
      bool offer = false;
      if (cont_offer) {
        offer = true;
      } else if (over_hold > 0) {
        --over_hold;
        offer = true;
        if (over_hold == 0) over_gap = 1 + static_cast<int>(rg.below(4));
      } else if (over_gap > 0) {
        --over_gap;
      } else if (rg.chance(12)) {
        over_hold = static_cast<int>(rg.below(6));
        offer = true;
        if (over_hold == 0) over_gap = 1 + static_cast<int>(rg.below(4));
      }
      if (offer) {
        st_valid = 1;
        st_top = 0x0BAD0BADu;
        st_bot = 0x0BAD0BADu;
        st_src = 0xFFFFu;
      }
    }

    // Placement and cell writes, spread through the fill and forced home in the
    // last stretch so a fill is never handed over incomplete. Between fills the
    // ports carry GARBAGE: they are the two write paths with no handshake, they
    // land in the buffer the next fill will take (the handover moves fill_par
    // off the one being served), and they must not disturb the served patch.
    uint8_t pos_we = 0, pos_axis = 0, pos_idx = 0, cs_we = 0, cs_wci = 0, cs_wcj = 0, cs_wsub = 0;
    uint32_t pos_val = 0;
    if (m_fill) {
      const bool force = (kVerts - m_wcur) < 700;
      if (pn < 2 * kLW && (force || rg.chance(8))) {
        const int idx = pn % kLW, ax = pn / kLW;
        pos_we = 1;
        pos_axis = static_cast<uint8_t>(ax);
        pos_idx = static_cast<uint8_t>(idx);
        pos_val = word(ax ? kFWz : kFWx, gen_tag(m_fillgen), static_cast<uint32_t>(idx));
        ++pn;
      }
      if (cn < kCells && (force || rg.chance(65))) {
        const int wci = cn % kCW, wcj = cn / kCW;
        cs_we = 1;
        cs_wci = static_cast<uint8_t>(wci);
        cs_wcj = static_cast<uint8_t>(wcj);
        cs_wsub = subst(gen_tag(m_fillgen), wci, wcj);
        ++cn;
      }
    } else if (rg.chance(10)) {
      pos_we = 1;
      pos_axis = static_cast<uint8_t>(rg.below(2));
      pos_idx = static_cast<uint8_t>(rg.below(33));
      pos_val = 0xBADBAD00u | rg.below(256);
      cs_we = 1;
      cs_wci = static_cast<uint8_t>(rg.below(32));
      cs_wcj = static_cast<uint8_t>(rg.below(32));
      cs_wsub = static_cast<uint8_t>(rg.below(4));
      ++stray_writes;
    }

    // The lattice request stream: gaps, both surfaces, and out-of-range indices
    // mixed in rather than segregated into a section of their own.
    uint8_t lat_req = 0, vi = 0, vj = 0, surf = 0;
    if (rg.chance(lat_pct)) {
      lat_req = 1;
      surf = static_cast<uint8_t>(rg.below(2));
      if (rg.chance(75)) {
        vi = static_cast<uint8_t>(rg.below(kLW));
        vj = static_cast<uint8_t>(rg.below(kLH));
      } else {
        const uint32_t which = rg.below(3);  // 0 = vi bad, 1 = vj bad, 2 = both
        vi = (which == 1) ? static_cast<uint8_t>(rg.below(kLW))
                          : static_cast<uint8_t>(kLW + rg.below(31));
        vj = (which == 0) ? static_cast<uint8_t>(rg.below(kLH))
                          : static_cast<uint8_t>(kLH + rg.below(31));
      }
      ++lat_reqs;
      ++vi_hist[vi];
      ++vj_hist[vj];
      ++surf_hist[surf];
    } else {
      ++lat_gaps;
    }
    const bool lat_in_range = (vi < kLW) && (vj < kLH);
    if (lat_req) {
      if (lat_in_range) ++lat_in;
      else ++lat_out;
    }

    uint8_t cs_req = 0, ci = 0, cj = 0;
    if (rg.chance(cell_pct)) {
      cs_req = 1;
      ci = static_cast<uint8_t>(rg.below(kCW));
      cj = static_cast<uint8_t>(rg.below(kCH));
      ++cell_reqs;
      ++ci_hist[ci];
      ++cj_hist[cj];
    } else {
      ++cell_gaps;
    }

    // The release. A SCHEDULED pulse per handover, whose delay is drawn so it
    // sometimes lands long before the next fill completes (the swap then waits
    // on nothing) and sometimes long after (the fill parks at capacity and the
    // producer's refused records pile up), plus SPURIOUS pulses while nothing
    // is served, whose only job is to drift across handovers so the
    // held-across-a-handover case happens without anyone placing it.
    uint8_t serve_release = 0;
    if (rel_hold > 0) {
      serve_release = 1;
      --rel_hold;
    } else if (rel_armed) {
      if (rel_wait > 0) {
        --rel_wait;
      } else {
        // Widths CYCLE 1..5 rather than being drawn: five draws from five
        // values misses one about two times in three, and the widths are
        // exactly what the edge-qualification decision turns on.
        const int w = 1 + static_cast<int>(releases % 5);
        ++relw_hist[w];
        ++releases;
        rel_armed = false;
        rel_hold = w - 1;
        serve_release = 1;
      }
    } else if (!m_serve && m_filled < kNGen && rg.chance(2)) {
      rel_hold = static_cast<int>(rg.below(6));
      ++spurious;
      serve_release = 1;
    }
    if (serve_release) ++release_cycles;

    // The 9 x 9 instance is still serving the patch section 15 filled and is
    // never released here. It is driven ONLY to bring cs_oob_o into the
    // randomised phase: the production cell plane is 32 x 32 behind 5-bit
    // ports, so no value a consumer can present there is out of range.
    uint8_t s_lat_req = 0, s_vi = 0, s_vj = 0, s_surf = 0, s_cs_req = 0, s_ci = 0, s_cj = 0;
    if (rg.chance(60)) {
      s_lat_req = 1;
      s_surf = static_cast<uint8_t>(rg.below(2));
      if (rg.chance(60)) {
        s_vi = static_cast<uint8_t>(rg.below(kSW));
        s_vj = static_cast<uint8_t>(rg.below(kSH));
      } else {
        s_vi = static_cast<uint8_t>(kSW + rg.below(55));
        s_vj = static_cast<uint8_t>(rg.below(kSH));
      }
    }
    if (rg.chance(60)) {
      s_cs_req = 1;
      if (rg.chance(60)) {
        s_ci = static_cast<uint8_t>(rg.below(8));
        s_cj = static_cast<uint8_t>(rg.below(8));
      } else {
        s_ci = static_cast<uint8_t>(8 + rg.below(24));
        s_cj = static_cast<uint8_t>(rg.below(8));
      }
    }
    const bool s_lat_in_r = (s_vi < kSW) && (s_vj < kSH);
    const bool s_cs_in_r = (s_ci < 8) && (s_cj < 8);

    // ---------------- 2. drive and settle ---------------------------------
    d.fill_start = fill_start;
    d.st_valid = st_valid;
    d.st_top = st_top;
    d.st_bottom = st_bot;
    d.st_src_id = st_src;
    d.pos_we = pos_we;
    d.pos_axis = pos_axis;
    d.pos_idx = pos_idx;
    d.pos_val = pos_val;
    d.cs_we = cs_we;
    d.cs_w_ci = cs_wci;
    d.cs_w_cj = cs_wcj;
    d.cs_w_substance = cs_wsub;
    d.dual = Ldual[static_cast<size_t>(fgen)] ? 1 : 0;
    d.serve_release = serve_release;
    d.lat_req = lat_req;
    d.lat_vi = vi;
    d.lat_vj = vj;
    d.lat_surface = surf;
    d.cs_req = cs_req;
    d.cs_ci = ci;
    d.cs_cj = cj;
    d.s_lat_req = s_lat_req;
    d.s_lat_vi = s_vi;
    d.s_lat_vj = s_vj;
    d.s_lat_surface = s_surf;
    d.s_cs_req = s_cs_req;
    d.s_cs_ci = s_ci;
    d.s_cs_cj = s_cj;
    d.eval();

    // ---------------- 3. the registered answers, from LAST cycle -----------
    if (c > 0) {
      if (d.lat_h != e_h) ++bad_h;
      if (d.lat_wx != e_wx) ++bad_wx;
      if (d.lat_wz != e_wz) ++bad_wz;
      if (d.cs_substance != e_sub) ++bad_sub;
      if (d.s_lat_h != e_sh) ++bad_sh;
      if (d.s_cs_substance != e_ssub) ++bad_ssub;
      if ((d.lat_h != e_h || d.lat_wx != e_wx || d.lat_wz != e_wz || d.cs_substance != e_sub) &&
          printed < 6) {
        ++printed;
        std::printf(
            "    [random] cycle %lld: h %08X/%08X wx %08X/%08X wz %08X/%08X sub %u/%u "
            "(rtl/oracle)\n",
            c, d.lat_h, e_h, d.lat_wx, e_wx, d.lat_wz, e_wz, d.cs_substance, e_sub);
      }
    }

    // ---------------- 4. state and counters, EVERY cycle -------------------
    const bool exp_ready = m_fill && (m_wcur != kVerts) && !m_wphase;
    if ((d.st_ready != 0) != exp_ready) ++bad_ready;
    if ((d.fill_busy != 0) != m_fill) ++bad_busy;
    if ((d.fill_done != 0) != (m_fill && m_wcur == kVerts)) ++bad_done;
    if ((d.serve_valid != 0) != m_serve) ++bad_valid;
    if ((d.fill_accept != 0) != (fill_start != 0 && !m_fill)) ++bad_accept;
    if (d.fill_records != static_cast<uint32_t>(m_wcur)) ++bad_records;
    if (d.serve_src_id != (m_serve ? m_servesrc : kSrcNone)) ++bad_srcid;
    {
      const bool cf = d.patches_filled != b_filled + m_filled;
      const bool cs2 = d.patches_served != b_served + m_served;
      const bool co = d.fill_overrun != b_overrun + m_overrun;
      const bool cl = d.lat_oob != b_latoob + m_latoob;
      const bool cc = d.cs_oob != b_csoob + m_csoob;
      if (cf) ++bad_filled;
      if (cs2) ++bad_served;
      if (co) ++bad_overrun;
      if (cl) ++bad_latoob;
      if (cc) ++bad_csoob;
      if ((cf || cs2 || co || cl || cc) && first_ctr_bad < 0) {
        first_ctr_bad = c;
        std::printf(
            "    [random] first counter divergence at cycle %lld: filled %u/%lld served %u/%lld "
            "overrun %u/%lld lat_oob %u/%lld cs_oob %u/%lld (rtl/oracle)\n",
            c, d.patches_filled, b_filled + m_filled, d.patches_served, b_served + m_served,
            d.fill_overrun, b_overrun + m_overrun, d.lat_oob, b_latoob + m_latoob, d.cs_oob,
            b_csoob + m_csoob);
      }
    }
    if (d.s_lat_oob != b_slatoob + m_slatoob || d.s_cs_oob != b_scsoob + m_scsoob) ++bad_sctr;

    // ---------------- 5. the expectation for the NEXT cycle ----------------
    e_h = e_wx = e_wz = kPoison;
    e_sub = 3;
    if (lat_req && lat_in_range && m_serve) {
      const tr::ComposedLattice& LL = L[static_cast<size_t>(m_servegen)];
      const size_t v = static_cast<size_t>(vj) * kLW + vi;
      e_h = static_cast<uint32_t>(surf ? LL.bottom[v] : LL.top[v]);
      e_wx = static_cast<uint32_t>(LL.wx[vi]);
      e_wz = static_cast<uint32_t>(LL.wz[vj]);
      cov[static_cast<size_t>(surf) * kVerts + v] = 1;
    }
    if (cs_req && m_serve) e_sub = L[static_cast<size_t>(m_servegen)].substance(ci, cj);
    e_sh = kPoison;
    e_ssub = 3;
    if (s_lat_req && s_lat_in_r)
      e_sh = static_cast<uint32_t>(latS.top[static_cast<size_t>(s_vj) * kSW + s_vi]);
    if (s_cs_req && s_cs_in_r) e_ssub = latS.substance(s_ci, s_cj);

    // ---------------- 6. advance the model ---------------------------------
    const bool take = st_valid && exp_ready;
    const bool go = fill_start && !m_fill;
    const bool relpulse = serve_release && !m_relprev;
    const bool handover = m_fill && (m_wcur == kVerts) && (!m_serve || relpulse);

    // fill_overrun_o counts one per OFFER RUN that survives to a cycle where
    // the fill is at capacity -- which is what "a refused record" MEANS for a
    // producer that holds its payload until it is taken. Derived from what
    // this loop drove, not from the block's flag.
    if (st_valid) {
      if (!valid_prev || take_prev) run_counted = false;
      if (at_cap) {
        ++refused_cycles;
        if (!run_counted) {
          ++m_overrun;
          run_counted = true;
        }
      }
    } else {
      run_counted = false;
    }
    valid_prev = st_valid != 0;
    take_prev = take;

    if (lat_req && !lat_in_range) ++m_latoob;
    if (s_lat_req) {
      if (s_lat_in_r) ++s_lat_in;
      else {
        ++m_slatoob;
        ++s_lat_out;
      }
    }
    if (s_cs_req) {
      if (s_cs_in_r) ++s_cs_in;
      else {
        ++m_scsoob;
        ++s_cs_out;
      }
    }
    if (m_serve && relpulse) ++m_served;

    // the coincidences this phase exists to reach
    if (handover && lat_req) ++co_req_handover;
    if (handover && serve_release) ++co_handover_relhigh;
    if (take && lat_req) ++co_take_req;
    if (go && lat_req) ++co_start_req;
    if (lat_req && serve_release) ++co_req_relhigh;
    if (lat_req && !m_serve) ++co_req_unserved;
    if (take) ++takes;

    // control, in the module's own order
    if (go) {
      m_fill = true;
      m_wcur = 0;
      m_wphase = false;
      m_dual = Ldual[static_cast<size_t>(gen)];
      m_fillgen = gen;
      ++dual_hist[m_dual ? 1 : 0];
      // this generation's producer policy. Generation 1 is PINNED to the
      // never-lower-valid producer; the rest are drawn.
      const int sp[5] = {0, 5, 25, 50, 70};
      // Generation 1 is pinned to the never-lower-valid producer and
      // generation 3 to a withdrawing one; both are pinned to PARK at capacity
      // below, so the two shapes of refused-record producer are exercised
      // whatever the seed does.
      const int k = (gen == 1) ? 0 : (gen == 3) ? 2 : static_cast<int>(rg.below(5));
      stall_pct = sp[k];
      ++stallp_hist[k];
      cont_offer = (stall_pct == 0);
      const int lp[3] = {60, 80, 95};
      lat_pct = lp[rg.below(3)];
      cell_pct = 40 + static_cast<int>(rg.below(50));
      rec = 0;
      pn = 0;
      cn = 0;
      armed = false;
      over_hold = 0;
      over_gap = 0;
    }
    if (take) {
      m_wphase = true;
      m_fillsrc = st_src;
      ++rec;
      armed = false;
    } else if (m_wphase) {
      m_wphase = false;
      ++m_wcur;
    }
    if (handover) {
      if (pn != 2 * kLW || cn != kCells) ++placement_late;
      m_fill = false;
      m_servegen = m_fillgen;
      m_serve = true;
      m_servesrc = m_fillsrc;
      ++m_filled;
      ++handovers;
      ++gen;
      start_wait = static_cast<int>(rg.below(30));
      // Schedule this patch's release. The first two are PINNED to the two
      // interleavings that matter -- late enough that the next fill parks at
      // capacity, then early enough that it hands over the instant it fills --
      // so neither depends on a seed. The rest are drawn across the whole span
      // of a fill.
      rel_armed = (m_filled < kNGen);
      if (handovers == 1) rel_wait = 3600;        // late: generation 1 parks
      else if (handovers == 2) rel_wait = 300;    // early: generation 2 does not
      else if (handovers == 3) rel_wait = 3600;   // late: generation 3 parks too
      else rel_wait = 1200 + static_cast<int>(rg.below(2800));
    } else if (m_serve && relpulse) {
      m_serve = false;
    }
    m_relprev = serve_release != 0;

    zhao::tick(d);
    ++cycles;
  }

  // ---------------- the MEASURED distribution ------------------------------
  int vi_min = 1 << 30, vi_max = 0, vj_min = 1 << 30, vj_max = 0;
  int ci_min = 1 << 30, ci_max = 0, cj_min = 1 << 30, cj_max = 0;
  int vi_zero = 0, vj_zero = 0, ci_zero = 0, cj_zero = 0;
  for (int i = 0; i < 64; ++i) {
    if (vi_hist[i] < vi_min) vi_min = vi_hist[i];
    if (vi_hist[i] > vi_max) vi_max = vi_hist[i];
    if (vj_hist[i] < vj_min) vj_min = vj_hist[i];
    if (vj_hist[i] > vj_max) vj_max = vj_hist[i];
    if (vi_hist[i] == 0) ++vi_zero;
    if (vj_hist[i] == 0) ++vj_zero;
  }
  for (int i = 0; i < 32; ++i) {
    if (ci_hist[i] < ci_min) ci_min = ci_hist[i];
    if (ci_hist[i] > ci_max) ci_max = ci_hist[i];
    if (cj_hist[i] < cj_min) cj_min = cj_hist[i];
    if (cj_hist[i] > cj_max) cj_max = cj_hist[i];
    if (ci_hist[i] == 0) ++ci_zero;
    if (cj_hist[i] == 0) ++cj_zero;
  }
  int covered = 0;
  for (size_t i = 0; i < cov.size(); ++i) covered += cov[i];
  int relw_seen = 0;
  for (int w = 1; w <= 5; ++w)
    if (relw_hist[w] > 0) ++relw_seen;
  int stallp_seen = 0;
  for (int k = 0; k < 5; ++k)
    if (stallp_hist[k] > 0) ++stallp_seen;

  std::printf("  [random] seed 0x%016llX, %lld cycles, %d generations\n",
              static_cast<unsigned long long>(kSeed), cycles, kNGen);
  std::printf(
      "    draw: lat req %lld (in %lld / oob %lld), gaps %lld; cell req %lld, gaps %lld; "
      "records taken %lld, producer idle %lld\n",
      lat_reqs, lat_in, lat_out, lat_gaps, cell_reqs, cell_gaps, takes, prod_idle);
  std::printf(
      "    dist: vi 0-63 min %d max %d empty %d | vj min %d max %d empty %d | ci 0-31 min %d "
      "max %d empty %d | cj min %d max %d empty %d\n",
      vi_min, vi_max, vi_zero, vj_min, vj_max, vj_zero, ci_min, ci_max, ci_zero, cj_min, cj_max,
      cj_zero);
  std::printf(
      "    dist: surface top %d / bottom %d; dual true %d / false %d; scheduled release widths "
      "1:%d 2:%d 3:%d 4:%d 5:%d; stall modes seen %d/5 (cont-offer %d)\n",
      surf_hist[0], surf_hist[1], dual_hist[1], dual_hist[0], relw_hist[1], relw_hist[2],
      relw_hist[3], relw_hist[4], relw_hist[5], stallp_seen, stallp_hist[0]);
  std::printf(
      "    events: handovers %lld, releases %lld scheduled + %lld spurious (%lld cycles high), "
      "refused offers %lld over %lld refusing cycles, stray writes %lld\n",
      handovers, releases, spurious, release_cycles, m_overrun, refused_cycles, stray_writes);
  std::printf(
      "    coincidence: request-on-handover %lld, release-high-on-handover %lld, "
      "take-on-request %lld, start-on-request %lld, request-while-releasing %lld, "
      "request-with-nothing-served %lld\n",
      co_req_handover, co_handover_relhigh, co_take_req, co_start_req, co_req_relhigh,
      co_req_unserved);
  std::printf("    small instance: lat in %lld / oob %lld, cell in %lld / oob %lld\n", s_lat_in,
              s_lat_out, s_cs_in, s_cs_out);
  std::printf("    address coverage: %d of %d vertex-surfaces read in range while served\n",
              covered, 2 * kVerts);

  // ---- the distribution is ASSERTED, not merely printed --------------------
  ck(vi_zero == 0 && vj_zero == 0,
     "every one of the 64 values a 6-bit lattice index can carry was drawn on BOTH axes -- the "
     "in-range 0..32 and the out-of-range 33..63. A draw taken from an LCG's low bits reports "
     "thousands of requests over a handful of distinct cases, which is what happened to a "
     "sibling lane this session, so the histogram is checked and not assumed",
     0, vi_zero + vj_zero);
  ck(vi_min >= 20 && vj_min >= 20,
     "and no bucket is starved: the least-drawn index on each axis was hit at least 20 times, "
     "so this is a distribution and not one lucky sample per bucket",
     1, (vi_min >= 20 && vj_min >= 20) ? 1 : 0);
  ck(ci_zero == 0 && cj_zero == 0 && ci_min >= 20 && cj_min >= 20,
     "the same for all 32 cell indices on both axes", 1,
     (ci_zero == 0 && cj_zero == 0 && ci_min >= 20 && cj_min >= 20) ? 1 : 0);
  ck(surf_hist[0] > lat_reqs / 4 && surf_hist[1] > lat_reqs / 4,
     "both surfaces were asked for on at least a quarter of requests each -- one bit decides "
     "which of the two planes answers, and a phase that only ever asked for tops would check "
     "half the store",
     1, (surf_hist[0] > lat_reqs / 4 && surf_hist[1] > lat_reqs / 4) ? 1 : 0);
  ck(lat_out > 500 && lat_in > 5000,
     "out-of-range requests were mixed into the in-range stream in quantity rather than "
     "segregated into a section of their own",
     1, (lat_out > 500 && lat_in > 5000) ? 1 : 0);
  ck(lat_gaps > 1000 && cell_gaps > 1000,
     "with thousands of idle cycles on both request ports, every one of which must answer "
     "poison -- an answer that lingers is as wrong as one that never arrives",
     1, (lat_gaps > 1000 && cell_gaps > 1000) ? 1 : 0);
  ck(dual_hist[0] > 0 && dual_hist[1] > 0, "patches of BOTH dual_i values went through", 1,
     (dual_hist[0] > 0 && dual_hist[1] > 0) ? 1 : 0);
  ck(relw_seen == 5,
     "serve_release_i was held for one, two, three, four AND five clocks -- the widths the "
     "edge-qualification decision turns on",
     5, relw_seen);
  ck(prod_idle > 1000,
     "the producer genuinely stalled: over a thousand cycles with nothing on the port, so the "
     "fill path ran under backpressure and not at full rate",
     1, prod_idle > 1000 ? 1 : 0);
  ck(stallp_seen >= 3 && stallp_hist[0] > 0,
     "across at least three producer stall rates, one of which never lowers st_valid_i at all "
     "-- the shape a rising-edge overrun detector would score as zero",
     1, (stallp_seen >= 3 && stallp_hist[0] > 0) ? 1 : 0);
  ck(m_overrun >= 10 && refused_cycles > 4 * m_overrun,
     "refused records were offered in RUNS: the refusal condition held for several times as "
     "many cycles as there were distinct offers, which is exactly the gap between the two "
     "readings of fill_overrun_o",
     1, (m_overrun >= 10 && refused_cycles > 4 * m_overrun) ? 1 : 0);
  ck(co_req_handover >= 3 && co_handover_relhigh >= 1 && co_start_req >= 2 &&
         co_take_req > 2000 && co_req_relhigh > 5 && co_req_unserved > 200,
     "and the events actually collided: requests were in flight on the clock a buffer changed "
     "hands, a release was HIGH across a handover without anybody placing it there, thousands "
     "of record acceptances landed on request cycles, fills started on request cycles, and "
     "hundreds of requests arrived with nothing served. That coincidence space is what a "
     "hand-ordered directed phase cannot reach",
     1,
     (co_req_handover >= 3 && co_handover_relhigh >= 1 && co_start_req >= 2 &&
      co_take_req > 2000 && co_req_relhigh > 5 && co_req_unserved > 200)
         ? 1
         : 0);
  ck(covered >= 1700,
     "the random requests reached most of the 2,178 distinct vertex-surfaces while a patch was "
     "being served -- hopping, not walking, so an address path that only works for consecutive "
     "indices has nowhere to hide. The threshold is set below what this seed measures and well "
     "above what a degenerate draw could reach; the remaining addresses are covered exhaustively "
     "four times over by the directed phases and once more by the tail below",
     1, covered >= 1700 ? 1 : 0);
  ck(s_lat_out > 500 && s_cs_out > 500,
     "and the 9 x 9 instance took hundreds of requests off both edges, so cs_oob_o -- "
     "structurally unreachable at 33 x 33 -- is exercised in the randomised phase too",
     1, (s_lat_out > 500 && s_cs_out > 500) ? 1 : 0);

  // ---- and now the answers -------------------------------------------------
  ck(handovers == kNGen && m_filled == kNGen,
     "all six generations were filled and handed over inside the cycle budget", kNGen,
     handovers);
  ck(placement_late == 0,
     "with every fill's 66 placement words and 1,024 cells written before its handover, so a "
     "content mismatch can only be the block's",
     0, placement_late);
  ck(bad_h == 0 && bad_wx == 0 && bad_wz == 0 && bad_sub == 0,
     "EVERY answer on the lattice and cell ports matched zref::terrain::ComposedLattice, on "
     "every cycle of the phase -- including the poison owed on every idle cycle, every "
     "out-of-range index and every cycle with no patch served",
     0, bad_h + bad_wx + bad_wz + bad_sub);
  ck(bad_sh == 0 && bad_ssub == 0, "and the same on the 9 x 9 instance's two read ports", 0,
     bad_sh + bad_ssub);
  ck(bad_ready == 0 && bad_busy == 0 && bad_done == 0 && bad_valid == 0 && bad_accept == 0,
     "st_ready_o, fill_busy_o, fill_done_o, serve_valid_o and fill_accept_o agreed with an "
     "independently stepped model of the control on every cycle -- the handshake checked as a "
     "waveform rather than at the moments a directed stage chose to look",
     0, bad_ready + bad_busy + bad_done + bad_valid + bad_accept);
  ck(bad_records == 0, "fill_records_o tracked the model's write cursor on every cycle", 0,
     bad_records);
  ck(bad_srcid == 0,
     "and serve_src_id_o named the served generation on every cycle -- SRC_NONE whenever "
     "nothing was served, the completing record's id whenever something was, and never once the "
     "id of the patch landing in the other buffer",
     0, bad_srcid);
  ck(bad_filled + bad_served + bad_overrun + bad_latoob + bad_csoob == 0,
     "all five production counters matched an expectation maintained from the STIMULUS on every "
     "cycle: patches_filled_o, patches_served_o, fill_overrun_o, lat_oob_o and cs_oob_o. A "
     "counter read only at the end of a stage can be wrong for thousands of cycles and right "
     "again by the time anybody looks",
     0, bad_filled + bad_served + bad_overrun + bad_latoob + bad_csoob);
  if (bad_filled + bad_served + bad_overrun + bad_latoob + bad_csoob != 0)
    std::printf("    [random] counter mismatch cycles: filled %d served %d overrun %d "
                "lat_oob %d cs_oob %d\n",
                bad_filled, bad_served, bad_overrun, bad_latoob, bad_csoob);
  ck(bad_sctr == 0, "and the 9 x 9 instance's two out-of-range counters likewise", 0, bad_sctr);
  ck(m_csoob == 0 && d.cs_oob == b_csoob,
     "cs_oob_o is still zero at 33 x 33 after thousands of randomised cell requests, which is "
     "the right answer and not a dead detector: the same counter on the 9 x 9 instance moved by "
     "hundreds over the same cycles",
     0, static_cast<long long>(d.cs_oob - b_csoob));

  // A final exhaustive readback of whatever the random walk left on the port.
  ck(m_serve && m_servegen == kNGen - 1,
     "the phase ends with the last generation still on the serve port", kNGen - 1, m_servegen);
  d.s_lat_req = 0;
  d.s_cs_req = 0;
  quiet(d, 2);
  {
    const ReadPlan P = build_full_read_plan(&L[kNGen - 1]);
    Stats st;
    run(d, P.steps, nullptr, st, "randomTail");
    report("random tail, exhaustive:", st);
    ck(st.h_bad == 0 && st.wx_bad == 0 && st.wz_bad == 0 && st.sub_bad == 0,
       "and every vertex of both surfaces, both placement planes and all 1,024 cells of that "
       "patch read back correctly after a whole phase of randomised fill, serve, release and "
       "stray-write traffic on top of it",
       0, st.h_bad + st.wx_bad + st.wz_bad + st.sub_bad);
    ck(d.serve_src_id == src_id(gen_tag(kNGen - 1), kVerts - 1), "still naming itself",
       src_id(gen_tag(kNGen - 1), kVerts - 1), d.serve_src_id);
  }
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
  d.s_st_src_id = 0;
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
  ck(d.serve_src_id == kSrcNone,
     "and serve_src_id_o reads the no-patch tell 0xF00D -- the low half of the lattice port's "
     "own poison, so a consumer that reads an identity before there is one to read sees a "
     "value from a family it already recognises",
     static_cast<long long>(kSrcNone), d.serve_src_id);

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
      if (pa.srcid_at_take[k] != src_id(0, static_cast<uint32_t>(k)) ||
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
    ck(d.serve_src_id == src_id(0, kVerts - 1),
       "and the SERVE side now NAMES the patch: serve_src_id_o carries the src_id of the "
       "record that COMPLETED the fill -- generation A in the high bits, vertex 1,088 in the "
       "low. That second half makes it an independent, serve-side witness of the "
       "order-is-the-index law, and it has to agree with the fill-side cursor check above",
       src_id(0, kVerts - 1), d.serve_src_id);
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
    ck(d.serve_src_id == src_id(0, kVerts - 1),
       "AND SO DOES ITS NAME. All 1,089 of patch B's records have landed, so a single "
       "`last_src_id_o` flop -- the obvious cheap readback -- would be reading B's id here, "
       "handing a consumer the identity of the patch it is NOT reading at exactly the moment "
       "double buffering is doing its job. The id follows the parity",
       src_id(0, kVerts - 1), d.serve_src_id);
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
    ck(d.serve_src_id == src_id(1, kVerts - 1),
       "and serve_src_id_o moved with it -- generation B's last record, so a consumer can "
       "prove the swap happened without reading a single height", src_id(1, kVerts - 1),
       d.serve_src_id);
  }

  // =========================================================================
  // 12. fill_overrun_o COUNTS REFUSED RECORDS, NOT REFUSED CYCLES
  // =========================================================================
  // Section 9 offered ONE refused record for ONE cycle, which cannot tell the
  // two readings apart -- both answer 1. The separating stimulus is a producer
  // that PARKS: under ready/valid it holds its payload stable and valid high
  // until the record is taken, and at capacity it never is, so a single
  // refused record can occupy the port for as long as the consumer takes to
  // release the patch that is blocking the swap. Counting per clock reports
  // the producer's patience rather than the overrun.
  //
  // Patch C is filled while B is served, which parks the block at capacity for
  // as long as this test likes.
  const tr::ComposedLattice latC = make_lat(3);
  Producer pc;
  pc.patch = 3;
  pc.dual = true;
  pc.stall_pct = 30;
  {
    d.fill_start = 1;
    d.eval();
    ck(d.fill_accept == 1, "a third fill starts while patch B is being served", 1,
       d.fill_accept);
    zhao::tick(d);
    d.fill_start = 0;

    // Re-read the whole of patch B while C lands: more double-buffer evidence
    // for free, and it leaves the DUT parked at capacity when the plan runs out.
    const ReadPlan P = build_full_read_plan(&latB, 4200);
    Stats st;
    run(d, P.steps, &pc, st, "BduringC");
    report("patch B during C's fill:", st);
    ck(pc.takes == kVerts, "all 1,089 records of patch C were accepted", kVerts, pc.takes);
    ck(st.h_bad == 0 && st.wx_bad == 0 && st.wz_bad == 0 && st.sub_bad == 0,
       "with patch B read back unchanged throughout", 0,
       st.h_bad + st.wx_bad + st.wz_bad + st.sub_bad);
    ck(d.fill_done == 1 && d.fill_busy == 1 && d.serve_valid == 1,
       "so the block is parked exactly where the two readings differ: a completed fill at "
       "capacity, a served patch blocking the swap, and a producer free to keep pushing",
       1, (d.fill_done && d.fill_busy && d.serve_valid) ? 1 : 0);

    const uint32_t ov0 = d.fill_overrun;

    // (a) ONE record, held for twelve clocks.
    d.st_valid = 1;
    d.st_top = 0x0BAD0BADu;
    d.st_bottom = 0x0BAD0BADu;
    d.st_src_id = 0xFFFFu;
    for (int i = 0; i < 12; ++i) zhao::tick(d);
    d.st_valid = 0;
    zhao::tick(d);
    d.eval();
    ck(d.fill_overrun - ov0 == 1,
       "a producer parked at capacity for TWELVE clocks with one record on the port is ONE "
       "refused record, not twelve. The per-cycle reading answered 12 -- the same 1,090th "
       "record scoring 1 or 400 depending only on how long the consumer took to release the "
       "patch blocking the swap, which makes the number unusable as the thing it is named for",
       1, static_cast<long long>(d.fill_overrun - ov0));

    // (b) THREE further distinct offers: withdrawn and re-presented.
    for (int r = 0; r < 3; ++r) {
      d.st_valid = 0;
      for (int i = 0; i < 2; ++i) zhao::tick(d);
      d.st_valid = 1;
      d.st_top = 0x0BAD0BADu;
      d.st_bottom = 0x0BAD0BADu;
      d.st_src_id = 0xFFFFu;
      for (int i = 0; i < 5; ++i) zhao::tick(d);
    }
    d.st_valid = 0;
    zhao::tick(d);
    d.eval();
    ck(d.fill_overrun - ov0 == 4,
       "and a producer that WITHDRAWS and re-presents three more times is three more refused "
       "records: 4 in total against the 27 clocks the condition was true for. The counter "
       "counts what its name says",
       4, static_cast<long long>(d.fill_overrun - ov0));
    ck(d.fill_records == kVerts,
       "with the write cursor still stopped at 1,089 through all of it", kVerts,
       d.fill_records);
    ck(d.serve_src_id == src_id(1, kVerts - 1),
       "and the served patch's identity untouched by any of it", src_id(1, kVerts - 1),
       d.serve_src_id);
  }

  // =========================================================================
  // 13. A HELD serve_release_i RETIRES EXACTLY ONE PATCH -- AND DOES NOT
  //     THROW THE NEXT ONE AWAY
  // =========================================================================
  // This one is not merely a counting question. Read as a LEVEL, a release
  // held for two clocks hands the new patch over on the first clock and
  // RELEASES IT ON THE SECOND: a whole patch retired without one vertex being
  // read, serve_valid_o dropping under a tessellator that had just been told a
  // patch was there, and patches_served_o counting the phantom. The set-up is
  // the steady state -- a completed fill waiting behind a served patch -- so
  // this is not an exotic case, it is every patch.
  {
    const uint32_t served0 = d.patches_served, filled0 = d.patches_filled;
    ck(d.fill_done == 1 && d.serve_valid == 1,
       "the steady state again: fill C complete and waiting, patch B still being served", 1,
       (d.fill_done && d.serve_valid) ? 1 : 0);

    d.serve_release = 1;
    d.eval();
    zhao::tick(d);  // the handover clock: C over, B retired
    int dropped = 0;
    for (int i = 0; i < 4; ++i) {
      zhao::tick(d);  // ...and the line stays high for four more clocks
      if (!d.serve_valid) ++dropped;
    }
    d.serve_release = 0;
    zhao::tick(d);
    d.eval();

    ck(d.patches_served - served0 == 1,
       "a serve_release_i held high for FIVE clocks retires exactly ONE patch. The port means "
       "TESS is finished with the served patch, which happens once per patch; one release per "
       "rising edge is the only reading of it that counts events",
       1, static_cast<long long>(d.patches_served - served0));
    ck(dropped == 0,
       "and the patch handed over on the first of those clocks is STILL BEING SERVED on the "
       "last -- the level reading released it on clock two, un-read, which is not a counter "
       "bug but a thrown-away patch",
       0, dropped);
    ck(d.patches_filled - filled0 == 1, "the fill is counted once", 1,
       static_cast<long long>(d.patches_filled - filled0));
    ck(d.serve_valid == 1, "a patch is available", 1, d.serve_valid);
    ck(d.serve_src_id == src_id(3, kVerts - 1), "and it is patch C, by name",
       src_id(3, kVerts - 1), d.serve_src_id);

    const ReadPlan P = build_full_read_plan(&latC);
    Stats st;
    run(d, P.steps, nullptr, st, "readC");
    report("patch C, exhaustive:", st);
    ck(st.h_bad == 0 && st.wx_bad == 0 && st.wz_bad == 0 && st.sub_bad == 0,
       "and every vertex, both surfaces, both placement planes and all 1,024 cells of patch C "
       "read back correctly -- the name is not the only thing that survived the held release",
       0, st.h_bad + st.wx_bad + st.wz_bad + st.sub_bad);
  }

  // =========================================================================
  // 14. RELEASING THE LAST PATCH LEAVES NOTHING TO SERVE
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

  ck(d.serve_src_id == kSrcNone,
     "and the identity goes back to the no-patch tell with it -- a consumer cannot read the "
     "name of a lattice nobody owns any more than it can read its heights",
     static_cast<long long>(kSrcNone), d.serve_src_id);

  ck(d.patches_filled == 3 && d.patches_served == 3 && d.fill_overrun == 5,
     "final ledger: 3 filled, 3 served, 5 refused records (1 from section 9 and the 4 "
     "distinct offers of section 12, against the 28 clocks the refusal was asserted for)", 1,
     (d.patches_filled == 3 && d.patches_served == 3 && d.fill_overrun == 5) ? 1 : 0);

  // =========================================================================
  // 15. THE 9 x 9 INSTANCE: cs_oob, and the legacy single-surface page
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
      d.s_st_src_id = src_id(2, static_cast<uint32_t>(sn));
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
    ck(d.s_serve_src_id == src_id(2, kSVerts - 1),
       "naming it on the way -- the src_id readback is parameterised with everything else, so "
       "the 9 x 9 instance carries its own last record's id (generation 2, vertex 80) and not "
       "the production instance's",
       src_id(2, kSVerts - 1), d.s_serve_src_id);

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

  // =========================================================================
  // 16. THE RANDOMISED PHASE
  // =========================================================================
  {
    const tr::ComposedLattice latS = make_lat(2, false, kSW, kSH);
    random_phase(d, latS);
  }

  if (g_fail) {
    std::printf("[compcache_front_rtl_directed] %d of %d checks FAILED\n", g_fail, g_checks);
    zhao::exit_hard(1);
  }
  std::printf("[compcache_front_rtl_directed] %d checks passed\n", g_checks);
  zhao::exit_hard(0);
}
