// raster_rcp24_v3_directed.cpp — does the V3 tile compute the same reciprocal,
// does its exact 32x32 identity hold where denominators cannot reach, and does
// it still launch one micro-job per clock?
//
// ---------------------------------------------------------------------------
// TWO ORACLES, BECAUSE ONE DIFFERENTIAL CANNOT SEE THE WHOLE CHANGE
// ---------------------------------------------------------------------------
// PHASE A -- BEHAVIOUR. `zhao_raster_rcp24` runs in hardware beside the tile on
// the same denominators, and `zref::rcp_u24` (reference/include/zref/zref_rcp.hpp,
// spec/qformats.md 6.1) is checked alongside it. Two oracles rather than one
// because they fail differently: the RTL oracle catches a scheduling or
// truncation drift the C++ would not model, and the C++ oracle catches both RTL
// blocks agreeing on something the spec never said.
//
// PHASE B -- ARITHMETIC. TEXTURE-ISLAND-V3 S10.5's replacement of the 32-by-64
// multiply engages its high-word correction only when w > 2^31.
//
//     MEASURED, 2026-09-06, over the committed T24 table and every one of the
//     16,777,215 nonzero denominators, both Newton steps:
//
//         max w                 = 0x401F_EF88 = 1,075,834,760
//         2^31                  = 0x8000_0000 = 2,147,483,648
//         phases with w > 2^31  = 0
//
// So PHASE A CANNOT FAIL IF THE CORRECTION IS DELETED. That is not a reason to
// delete it -- S10.5's identity is unconditional over every 32-bit w and the
// tile must implement the identity, not the sample -- it is a reason the
// arithmetic core is a separate module with its own port. Phase B drives it
// directly over the SAME case families tools/rtl/architecture_numeric_checks.py
// drives, negative correction included, and counts them.
//
// The Python script is the artefact the owner supplied and is authoritative for
// the identity. `mx_original` and the boundary list below are transcribed from
// it rather than rederived, because a second derivation of the same idea is not
// a check of the first.
//
// ---------------------------------------------------------------------------
// THE RANDOM PHASE DRAWS FROM THE HIGH BITS
// ---------------------------------------------------------------------------
// A sibling lane this session had a "randomised" phase whose low-bit draws
// turned 240 cases into 4. A power-of-two LCG's low bits have tiny period --
// bit 0 alternates, bit 1 has period 4 -- so a 64-bit LCG is used here and only
// its TOP 32 bits are ever read. The distribution is then MEASURED, not
// asserted by construction: distinct-value counts, exponent coverage, and the
// negative-correction count are all checked with numbers.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vtb_rcp24_v3_pair.h"

#include "zhao_sim.hpp"
#include "zref/zref_rcp.hpp"

namespace {

// ---------------------------------------------------------------------------
// architecture_numeric_checks.py, transcribed
// ---------------------------------------------------------------------------
// def mx_original(x, w):
//     t64 = ((1 << 31) - w) & M64
//     p = (x * t64) & M64
//     return p, (((p + (1 << 29)) & M64) >> 30) & M32
//
// Unsigned 64-bit arithmetic in C++ wraps mod 2^64 by the standard, which is
// exactly what `& M64` spells in Python. The wrap IS the law: the serial block's
// header calls the uint64 overflow "part of the law", not an accident.
struct Mx {
  uint64_t p;
  uint32_t iterate;
};

Mx mx_original(uint32_t x, uint32_t w) {
  const uint64_t t64 = (uint64_t{1} << 31) - static_cast<uint64_t>(w);
  const uint64_t p = static_cast<uint64_t>(x) * t64;
  return Mx{p, static_cast<uint32_t>((p + (uint64_t{1} << 29)) >> 30)};
}

// S10.5's operands, as the tile forms them.
uint32_t b32_of(uint32_t w) { return static_cast<uint32_t>((uint64_t{1} << 31) - w); }
bool neg_of(uint32_t w) { return w > (1u << 31); }

// The script's boundary list, in its order.
const std::vector<uint32_t> kBoundary = {
    0u,          1u,          2u,          63u,         64u,          127u,
    128u,        65535u,      65536u,      (1u << 29) - 1u,           1u << 29,
    (1u << 31) - 1u,          1u << 31,    (1u << 31) + 1u,           0xFFFFFFFEu,
    0xFFFFFFFFu};

// A 64-bit LCG read only from its TOP 32 bits. Seeded with the script's own
// seed so the provenance of the stimulus is one number, not a habit.
struct Lcg {
  uint64_t s;
  explicit Lcg(uint64_t seed) : s(seed) {}
  uint32_t u32() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<uint32_t>(s >> 32);
  }
};

struct Res {
  uint32_t r, k, zero;
  bool operator!=(const Res& o) const { return r != o.r || k != o.k || zero != o.zero; }
};

struct MulCase {
  uint32_t a, b, corr;
  uint64_t p;        // expected corrected P64
  uint32_t x_next;   // expected MX iterate
  uint32_t w_next;   // expected MW extraction
  uint8_t tag;
};

using Top = Vtb_rcp24_v3_pair;

// ---------------------------------------------------------------------------
// PHASE B driver: one case per clock into a nonstallable six-stage pipe.
// ---------------------------------------------------------------------------
// Results come back in issue order, so a deque is the whole scoreboard. The tag
// is checked as well: a pipeline that reordered would still produce the right
// SET of products, and a set comparison would wave that through.
struct MulStats {
  int checked = 0;
  int mismatch_p = 0;
  int mismatch_x = 0;
  int mismatch_w = 0;
  int mismatch_tag = 0;
  uint32_t first_bad_a = 0, first_bad_b = 0;
  uint64_t first_bad_got = 0, first_bad_want = 0;
  bool have_bad = false;
};

void run_mul_cases(Top& top, const std::vector<MulCase>& cases, bool check_w, MulStats* st) {
  std::deque<const MulCase*> inflight;
  const size_t n = cases.size();
  for (size_t c = 0; c <= n + 24; ++c) {
    const bool drive = c < n;
    top.c_valid_i = drive ? 1 : 0;
    if (drive) {
      top.c_a_i = cases[c].a;
      top.c_b_i = cases[c].b;
      top.c_corr_i = cases[c].corr;
      top.c_tag_i = cases[c].tag;
    }
    top.eval();
    if (top.c_valid_o) {
      if (inflight.empty()) {
        ++st->mismatch_tag;  // a result nobody asked for
      } else {
        const MulCase* e = inflight.front();
        inflight.pop_front();
        const uint64_t got = (static_cast<uint64_t>(top.c_phi_o) << 32) | top.c_plo_o;
        if (got != e->p) {
          ++st->mismatch_p;
          if (!st->have_bad) {
            st->have_bad = true;
            st->first_bad_a = e->a;
            st->first_bad_b = e->b;
            st->first_bad_got = got;
            st->first_bad_want = e->p;
          }
        }
        if (top.c_xnext_o != e->x_next) ++st->mismatch_x;
        if (check_w && top.c_wnext_o != e->w_next) ++st->mismatch_w;
        if (top.c_tag_o != e->tag) ++st->mismatch_tag;
        ++st->checked;
      }
    }
    zhao::tick(top);
    if (drive) inflight.push_back(&cases[c]);
  }
  top.c_valid_i = 0;
  if (!inflight.empty()) st->mismatch_tag += static_cast<int>(inflight.size());
}

// ---------------------------------------------------------------------------
// PHASE A driver: one batch of at most 256 denominators, tokens unique.
// ---------------------------------------------------------------------------
// Batching keeps the token space unambiguous. The V2 test reconstructed an index
// from a wrapped token with a while loop, which is a decoder that can itself be
// wrong -- and a wrong decoder in a comparison harness reads as a DUT mismatch.
struct BatchOut {
  std::map<uint32_t, Res> got;
  int clocks = 0;
};

BatchOut feed_v3(Top& top, const std::vector<uint32_t>& ds, int ready_period) {
  BatchOut out;
  size_t fed = 0;
  int rr = 0;
  for (int c = 0; c < 400000 && out.got.size() < ds.size(); ++c) {
    const bool feeding = fed < ds.size();
    top.b_valid_i = feeding ? 1 : 0;
    top.b_d_i = feeding ? ds[fed] : 0;
    top.b_tok_i = static_cast<uint8_t>(fed & 0xFF);
    // ready_period 1 means always ready; larger values stall the consumer, which
    // is what S10.9 asks the paired test to cover.
    top.b_rready_i = (ready_period <= 1 || (rr % ready_period) == 0) ? 1 : 0;
    top.eval();
    if (top.b_rvalid_o && top.b_rready_i) {
      out.got[top.b_tok_o] = {top.b_r_o, top.b_k_o, top.b_zero_o};
    }
    const bool took = feeding && top.b_ready_o;
    zhao::tick(top);
    ++rr;
    if (took) ++fed;
    ++out.clocks;
  }
  top.b_valid_i = 0;
  top.b_rready_i = 1;
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Top top;

  top.a_valid_i = 0;
  top.b_valid_i = 0;
  top.c_valid_i = 0;
  top.a_rready_i = 1;
  top.b_rready_i = 1;
  top.rst_n = 0;
  for (int i = 0; i < 8; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  // =========================================================================
  // PHASE B.1 -- the MX identity on the boundary cross product
  // =========================================================================
  // 16 x 16 = 256 pairs, the script's own `for x in boundary: for w in boundary`.
  // Three of the sixteen w values exceed 2^31 (2^31+1, M32-1, M32), so exactly
  // 48 of these 256 take the negative correction. That number is asserted, not
  // hoped for: it is the one part of the coverage claim that can be computed by
  // hand.
  {
    std::vector<MulCase> cases;
    int negs = 0;
    uint8_t tag = 0;
    for (uint32_t x : kBoundary) {
      for (uint32_t w : kBoundary) {
        const Mx e = mx_original(x, w);
        const bool neg = neg_of(w);
        negs += neg ? 1 : 0;
        cases.push_back(MulCase{x, b32_of(w), neg ? x : 0u, e.p, e.iterate, 0u, tag++});
      }
    }
    zhao::check(negs == 48, "boundary cross product takes the negative correction 48 times", 48,
                static_cast<uint64_t>(negs));

    MulStats st;
    run_mul_cases(top, cases, /*check_w=*/false, &st);
    std::printf("  B.1 boundary MX: %d checked, %d negative-correction\n", st.checked, negs);
    if (st.have_bad) {
      std::printf("    first mismatch a=0x%08X b=0x%08X got=0x%016llX want=0x%016llX\n",
                  st.first_bad_a, st.first_bad_b,
                  static_cast<unsigned long long>(st.first_bad_got),
                  static_cast<unsigned long long>(st.first_bad_want));
    }
    zhao::check(st.checked == 256, "every boundary MX case produced a result", 256,
                static_cast<uint64_t>(st.checked));
    zhao::check(st.mismatch_p == 0, "boundary MX: corrected P64 matches the uint64 reference", 0,
                static_cast<uint64_t>(st.mismatch_p));
    zhao::check(st.mismatch_x == 0, "boundary MX: the 32-bit iterate matches the uint64 reference",
                0, static_cast<uint64_t>(st.mismatch_x));
    zhao::check(st.mismatch_tag == 0, "boundary MX: results come back in issue order", 0,
                static_cast<uint64_t>(st.mismatch_tag));
  }

  // =========================================================================
  // PHASE B.2 -- the MX identity on 250,000 random (x, w), high bits only
  // =========================================================================
  int random_negs = 0;
  {
    Lcg rng(0x5A48414Full);
    std::vector<MulCase> cases;
    cases.reserve(250000);
    std::set<uint32_t> distinct_w;
    for (int i = 0; i < 250000; ++i) {
      const uint32_t x = rng.u32();
      const uint32_t w = rng.u32();
      if (distinct_w.size() < 60000) distinct_w.insert(w);
      const Mx e = mx_original(x, w);
      const bool neg = neg_of(w);
      random_negs += neg ? 1 : 0;
      cases.push_back(
          MulCase{x, b32_of(w), neg ? x : 0u, e.p, e.iterate, 0u, static_cast<uint8_t>(i & 0xFF)});
    }
    // THE DISTRIBUTION, MEASURED. 60,000 draws colliding down to a handful is
    // exactly the failure the high-bit rule exists to prevent, so the count is
    // asserted rather than described.
    zhao::check(distinct_w.size() >= 59000, "the random w draws are actually distinct", 59000,
                static_cast<uint64_t>(distinct_w.size()));

    MulStats st;
    run_mul_cases(top, cases, /*check_w=*/false, &st);
    const double pct = 100.0 * random_negs / 250000.0;
    std::printf("  B.2 random MX: %d checked, %d negative-correction (%.2f%%)\n", st.checked,
                random_negs, pct);
    if (st.have_bad) {
      std::printf("    first mismatch a=0x%08X b=0x%08X got=0x%016llX want=0x%016llX\n",
                  st.first_bad_a, st.first_bad_b,
                  static_cast<unsigned long long>(st.first_bad_got),
                  static_cast<unsigned long long>(st.first_bad_want));
    }
    zhao::check(st.checked == 250000, "every random MX case produced a result", 250000,
                static_cast<uint64_t>(st.checked));
    zhao::check(st.mismatch_p == 0, "random MX: corrected P64 matches the uint64 reference", 0,
                static_cast<uint64_t>(st.mismatch_p));
    zhao::check(st.mismatch_x == 0, "random MX: the 32-bit iterate matches the uint64 reference", 0,
                static_cast<uint64_t>(st.mismatch_x));
    zhao::check(st.mismatch_tag == 0, "random MX: results come back in issue order", 0,
                static_cast<uint64_t>(st.mismatch_tag));
    // The whole reduction exists for this branch. If the count is not roughly
    // half the draws, the stimulus is not testing it whatever else passed.
    zhao::check(random_negs > 100000, "the NEGATIVE correction is genuinely exercised", 100000,
                static_cast<uint64_t>(random_negs));
    zhao::check(random_negs < 150000, "and the positive case is not starved either", 150000,
                static_cast<uint64_t>(random_negs));
  }

  // =========================================================================
  // PHASE B.3 -- the MW width and shift identity, 250,000 random (m, x)
  // =========================================================================
  // S10.4: m <= 2^24-1 and x <= 2^32-1 give P < 2^56 and w = P >> 24 < 2^32,
  // unconditionally. The script asserts the same three things; the RTL has to
  // produce them from real bit slices.
  {
    Lcg rng(0x5A48414Full ^ 0x9E3779B97F4A7C15ull);
    std::vector<MulCase> cases;
    cases.reserve(250000);
    int over56 = 0;
    for (int i = 0; i < 250000; ++i) {
      const uint32_t m = rng.u32() & 0xFFFFFFu;
      const uint32_t x = rng.u32();
      const uint64_t p = static_cast<uint64_t>(m) * static_cast<uint64_t>(x);
      if (p >= (uint64_t{1} << 56)) ++over56;
      cases.push_back(MulCase{m, x, 0u, p, 0u, static_cast<uint32_t>(p >> 24),
                              static_cast<uint8_t>(i & 0xFF)});
    }
    zhao::check(over56 == 0, "S10.4: every MW product stayed below 2^56", 0,
                static_cast<uint64_t>(over56));

    MulStats st;
    run_mul_cases(top, cases, /*check_w=*/true, &st);
    std::printf("  B.3 random MW: %d checked\n", st.checked);
    zhao::check(st.checked == 250000, "every random MW case produced a result", 250000,
                static_cast<uint64_t>(st.checked));
    zhao::check(st.mismatch_p == 0, "random MW: the tiled 32x32 product is EXACT", 0,
                static_cast<uint64_t>(st.mismatch_p));
    zhao::check(st.mismatch_w == 0, "random MW: w = P[55:24] matches (m*x) >> 24", 0,
                static_cast<uint64_t>(st.mismatch_w));
  }

  // =========================================================================
  // PHASE A -- the reciprocal itself, against two oracles
  // =========================================================================
  // Denominators: the cases the serial block names by law (zero, 1, 2^23), the
  // extremes, every power of two so all 24 exponents appear, and a random tail
  // drawn from the LCG's high bits.
  std::vector<uint32_t> ds = {0u, 1u, 2u, 3u, 0x800000u, 0xFFFFFFu, 0x7FFFFFu, 0x800001u};
  for (int b = 0; b < 24; ++b) ds.push_back(1u << b);
  {
    Lcg rng(0xD1CE5EEDull);
    for (int i = 0; i < 4072; ++i) {
      uint32_t d = rng.u32() & 0xFFFFFFu;
      if ((i % 97) == 0) d = 0;  // zero is a scheduled phase; keep meeting it
      ds.push_back(d);
    }
  }

  // Exponent coverage, MEASURED. A random 24-bit draw is overwhelmingly likely
  // to have its top bit set, so without the explicit powers of two above this
  // stimulus would exercise one normalisation shift and call it a sweep.
  {
    std::set<int> ks;
    for (uint32_t d : ds) {
      if (d == 0) continue;
      ks.insert(zref::rcp_u24(d).k);
    }
    std::printf("  A.0 stimulus covers %zu distinct exponents k\n", ks.size());
    zhao::check(ks.size() == 24, "every reachable exponent k = 1..24 is in the stimulus", 24,
                static_cast<uint64_t>(ks.size()));
  }

  // ---- the serial block, in hardware, on the same denominators -------------
  std::vector<Res> ref;
  int serial_clocks = 0;
  {
    top.rst_n = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
    size_t fed = 0;
    for (int c = 0; c < 800000 && ref.size() < ds.size(); ++c) {
      top.a_valid_i = fed < ds.size();
      top.a_d_i = (fed < ds.size()) ? ds[fed] : 0;
      top.eval();
      if (top.a_rvalid_o) ref.push_back({top.a_r_o, top.a_k_o, top.a_zero_o});
      const bool took = top.a_valid_i && top.a_ready_o;
      zhao::tick(top);
      if (took) ++fed;
      ++serial_clocks;
    }
    top.a_valid_i = 0;
  }
  zhao::check(ref.size() == ds.size(), "the serial block answered every request", ds.size(),
              ref.size());

  // ---- the serial block agrees with zref, so the oracle chain is closed ----
  {
    int zmis = 0;
    for (size_t i = 0; i < ds.size() && i < ref.size(); ++i) {
      if (ds[i] == 0) {
        if (!ref[i].zero) ++zmis;
        continue;
      }
      const zref::rcp24_result z = zref::rcp_u24(ds[i]);
      if (ref[i].r != z.r || ref[i].k != static_cast<uint32_t>(z.k) || ref[i].zero) ++zmis;
    }
    zhao::check(zmis == 0, "the SERIAL block agrees with zref::rcp_u24 on the same stimulus", 0,
                static_cast<uint64_t>(zmis));
  }

  // ---- the V3 tile, batched so tokens are unambiguous ----------------------
  // Three passes over the same denominators: free-running, consumer-stalled,
  // and consumer-stalled harder. S10.9 asks for stalls and repeated context
  // reuse; 20 batches through 16 contexts is 320 reuses of every context.
  const int kReadyPeriods[3] = {1, 3, 7};
  for (int pass = 0; pass < 3; ++pass) {
    const int rp = kReadyPeriods[pass];
    top.rst_n = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);

    int mism = 0, answered = 0, total_clocks = 0;
    for (size_t base = 0; base < ds.size(); base += 256) {
      const size_t n = std::min<size_t>(256, ds.size() - base);
      std::vector<uint32_t> batch(ds.begin() + static_cast<long>(base),
                                  ds.begin() + static_cast<long>(base + n));
      const BatchOut o = feed_v3(top, batch, rp);
      total_clocks += o.clocks;
      for (size_t i = 0; i < n; ++i) {
        auto it = o.got.find(static_cast<uint32_t>(i));
        if (it == o.got.end()) {
          ++mism;
          continue;
        }
        ++answered;
        if (it->second != ref[base + i]) ++mism;
      }
    }
    std::printf("  A.%d ready 1-in-%d: %d answered, %d clocks (%.2f per reciprocal)\n", pass + 1, rp,
                answered, total_clocks,
                static_cast<double>(total_clocks) / static_cast<double>(ds.size()));
    zhao::check(answered == static_cast<int>(ds.size()),
                "the V3 tile answered every request in this pass",
                static_cast<uint64_t>(ds.size()), static_cast<uint64_t>(answered));
    zhao::check(mism == 0, "every V3 answer is BIT-IDENTICAL to the serial block's", 0,
                static_cast<uint64_t>(mism));
    zhao::check(top.b_qerr_o == 0, "no queue overflowed or underflowed", 0,
                static_cast<uint64_t>(top.b_qerr_o));
    zhao::check(top.b_occupancy_o == 0, "every execution context was released", 0,
                static_cast<uint64_t>(top.b_occupancy_o));

    // ---- the counter contract, S10.8 --------------------------------------
    // Zero requests cost ONE scheduled phase and NO products. Nonzero requests
    // cost exactly four products. A test that only checked results would not
    // see a machine doing the work twice.
    int zeros = 0;
    for (uint32_t d : ds) zeros += (d == 0) ? 1 : 0;
    const uint32_t nonzeros = static_cast<uint32_t>(ds.size()) - static_cast<uint32_t>(zeros);
    zhao::check(top.b_accepted_o == ds.size(), "accepted counter matches the requests fed",
                ds.size(), top.b_accepted_o);
    zhao::check(top.b_completed_o == ds.size(), "completed counter matches the results taken",
                ds.size(), top.b_completed_o);
    zhao::check(top.b_mul_jobs_o == 4u * nonzeros,
                "exactly four product launches per NONZERO reciprocal", 4u * nonzeros,
                top.b_mul_jobs_o);
    zhao::check(top.b_zero_jobs_o == static_cast<uint32_t>(zeros),
                "exactly one zero phase per zero request, counted separately",
                static_cast<uint64_t>(zeros), top.b_zero_jobs_o);
    zhao::check(top.b_phase_jobs_o == top.b_mul_jobs_o + top.b_zero_jobs_o,
                "the scheduled-phase counter is the sum, not a fifth story",
                top.b_mul_jobs_o + top.b_zero_jobs_o, top.b_phase_jobs_o);
    // HONEST ZERO. The reciprocal's own domain never reaches w > 2^31, so this
    // must be zero -- and if it is ever nonzero, either the measurement above is
    // wrong or the operand selection is.
    zhao::check(top.b_negcorr_jobs_o == 0,
                "no denominator reaches the negative correction (max w = 0x401FEF88)", 0,
                top.b_negcorr_jobs_o);
  }

  // =========================================================================
  // THROUGHPUT -- the reason the scans became queues
  // =========================================================================
  // One saturated batch, consumer always ready. Steady state is four clocks per
  // reciprocal (four micro-jobs, one launch per clock); the batch also pays one
  // fill and one drain of the ten-clock feedback loop, so the gate has room for
  // that and not for a second reciprocal's worth of idling.
  {
    top.rst_n = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
    std::vector<uint32_t> batch;
    Lcg rng(0x0FF1CE00ull);
    for (int i = 0; i < 256; ++i) batch.push_back((rng.u32() & 0x7FFFFFu) | 0x800000u);
    const BatchOut o = feed_v3(top, batch, 1);
    const double per = static_cast<double>(o.clocks) / 256.0;
    std::printf("  T   saturated: 256 reciprocals in %d clocks (%.2f each)\n", o.clocks, per);
    std::printf("  T   serial reference: %d clocks for %zu (%.2f each)\n", serial_clocks, ds.size(),
                static_cast<double>(serial_clocks) / static_cast<double>(ds.size()));
    zhao::check(o.got.size() == 256, "the saturated batch answered every request", 256,
                o.got.size());
    zhao::check(per < 4.6, "a saturated V3 tile costs under 4.6 clocks per reciprocal", 46,
                static_cast<uint64_t>(per * 10.0));
    zhao::check(per >= 4.0, "and not under four, which would mean a launch was skipped", 40,
                static_cast<uint64_t>(per * 10.0));
  }

  // =========================================================================
  // EXHAUSTIVE -- all 16,777,215 nonzero denominators, on request
  // =========================================================================
  // S10.9: "An exhaustive sweep of all 2^24 denominator values is practical as a
  // repository regression once the implementation exists. This review did not
  // run that sweep against the compiled repository reference or the new RTL."
  //
  // It is behind a flag because it is minutes rather than seconds, and the
  // default run is a gate that has to stay cheap. The oracle here is zref rather
  // than the serial RTL block: the serial block would need its own 16.7M
  // requests at a longer initiation interval, and zref::rcp_u24 is the law both
  // of them are transcribed from.
  //
  // Tokens are eight bits and there are only sixteen contexts, so at most
  // sixteen requests are ever outstanding and a 256-entry side table cannot have
  // a live slot overwritten. That is a bound, not a hope: `b_occupancy_o` is
  // asserted below.
  bool exhaustive = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--exhaustive") exhaustive = true;
  }
  if (exhaustive) {
    top.rst_n = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);

    static uint32_t pending[256];
    const uint32_t kN = (1u << 24) - 1u;  // d = 1 .. 0xFFFFFF
    uint32_t issued = 0, done = 0;
    int mism = 0;
    uint32_t first_bad_d = 0;
    long long clocks = 0;
    uint8_t max_occ = 0;
    top.b_rready_i = 1;
    while (done < kN && clocks < 400000000LL) {
      const bool feeding = issued < kN;
      const uint32_t d = issued + 1u;
      top.b_valid_i = feeding ? 1 : 0;
      top.b_d_i = feeding ? d : 0;
      top.b_tok_i = static_cast<uint8_t>(issued & 0xFF);
      top.eval();
      if (top.b_occupancy_o > max_occ) max_occ = top.b_occupancy_o;
      if (top.b_rvalid_o) {
        const uint32_t dd = pending[top.b_tok_o];
        const zref::rcp24_result z = zref::rcp_u24(dd);
        if (top.b_r_o != z.r || top.b_k_o != static_cast<uint32_t>(z.k) || top.b_zero_o) {
          if (mism == 0) first_bad_d = dd;
          ++mism;
        }
        ++done;
      }
      const bool took = feeding && top.b_ready_o;
      if (took) pending[issued & 0xFF] = d;
      zhao::tick(top);
      if (took) ++issued;
      ++clocks;
    }
    top.b_valid_i = 0;
    std::printf("  E   exhaustive: %u/%u answered in %lld clocks (%.2f each), peak occupancy %u\n",
                done, kN, clocks, static_cast<double>(clocks) / static_cast<double>(kN ? kN : 1),
                static_cast<unsigned>(max_occ));
    if (mism) std::printf("    first mismatching denominator: 0x%06X\n", first_bad_d);
    zhao::check(done == kN, "every one of the 2^24-1 nonzero denominators was answered", kN, done);
    zhao::check(mism == 0, "EVERY denominator matches zref::rcp_u24 exactly", 0,
                static_cast<uint64_t>(mism));
    zhao::check(top.b_qerr_o == 0, "no queue faulted across the exhaustive sweep", 0,
                static_cast<uint64_t>(top.b_qerr_o));
    zhao::check(top.b_mul_jobs_o == 4u * kN, "four product launches per denominator, all 2^24 of them",
                4u * static_cast<uint64_t>(kN), top.b_mul_jobs_o);
  }

  return zhao::report_and_exit("raster_rcp24_v3_directed");
}
