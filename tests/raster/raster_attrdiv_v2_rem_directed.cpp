// raster_attrdiv_v2_rem_directed.cpp -- RASTER.ATTRDIV V2's published floor
// remainder, and the width guard that watches it.
//
// ENFORCES: fpga/rtl/raster/zhao_raster_attrdiv_v2.sv (rem_o, rem_range_err_o)
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS FOR, AND WHAT IT DELIBERATELY DOES NOT RE-LITIGATE
// ---------------------------------------------------------------------------
// Owner ruling R104 funded publishing v2's remainder so `zhao_raster_attrdiv_svc`
// and `zhao_raster_attrstep` can move off the superseded v1 divider. R104 also
// settled the ROUNDING (v2 matches `zref::render::div_rhu_s128` exactly; v1 does
// not), and that question is closed -- this file does not reopen it. It asks the
// two questions R104 said were still open, plus the one the tree's own manifest
// asked that R104 understated:
//
//   1. WIDTH. v1's rem_o is [47:0]; v2's internal residue is [48:0]. R104 asked
//      for the top bit to be proven CLEAR AT THE POINT OF PUBLICATION, by
//      stimulus, because "the mathematics describes the converged value and a
//      port publishes whatever is in the register". Section 3 drives the widest
//      legal operands there are and reads `rem_range_err_o`, which is the RTL's
//      own live instrument for exactly this.
//
//   2. THE PAIR IS NOT v1's PAIR. `design/prod_manifest.yml:149` said this
//      before the port existed and it is the load-bearing fact of the whole
//      adoption: v1 divides 2|n|+A by 2A, so its remainder is mod 2A; v2
//      divides M = n + floor(A/2) by A, so its remainder is mod A. They are
//      different quantities and no fixup turns one into the other. Section 1
//      asserts the invariant v2 actually satisfies --
//
//          q_o * A + rem_o == n + floor(A/2),   0 <= rem_o < A
//
//      -- which is the seed a floor-quotient recurrence wants, and is
//      CONTINUOUS ACROSS ZERO. Section 4 proves that continuity directly,
//      because it is the property that lets ATTRSTEP delete its second sign
//      branch rather than port it.
//
//   3. THE STEP DECOMPOSITION. v2 always adds the rounding bias, so a consumer
//      that wants the plain Euclidean division of dNdx by A has to undo it.
//      Section 5 proves the recovery against the real RTL rather than against a
//      restatement of it, because that recovery is what ATTRSTEP's combinational
//      block becomes.
//
// ASSERTING THE CORRECT BEHAVIOUR, NOT THE BUG. CLAUDE.md forbids a test that
// passes only while a defect exists. `rem_range_err_o` is asserted ZERO here --
// that is the design's correct behaviour. Its positive control is a separate,
// committed mutant with an inverted driver:
// tests/mutants/zhao_raster_attrdiv_v2_remwidth_mutant.sv, run by
// raster_attrdiv_v2_remwidth_mutant. A detector reading zero is a claim, and
// the mutant is what makes this one checkable.
#include <cstdint>
#include <cstdio>
#include <random>

#include "Vzhao_raster_attrdiv_v2.h"
#include "verilated.h"
#include "zhao_sim.hpp"

using i128 = __int128;
using u128 = unsigned __int128;

#ifndef ZHAO_ATTR_RADIX
#define ZHAO_ATTR_RADIX 2
#endif

namespace {

template <size_t N>
void put_wide(VlWide<N>& dst, i128 value) {
  const u128 bits = static_cast<u128>(value);
  for (size_t i = 0; i < N; ++i) dst[i] = static_cast<uint32_t>(bits >> (32 * i));
}

i128 floor_div(i128 n, i128 d) {
  i128 q = n / d;
  const i128 r = n % d;
  if (r < 0) --q;
  return q;
}

i128 floor_mod(i128 n, i128 d) { return n - floor_div(n, d) * d; }

// The area is unsigned, so floor(A/2) is a plain shift. Named rather than
// inlined because it is THE bias that separates v2's dividend from n.
i128 bias(uint64_t area) { return static_cast<i128>(area >> 1); }

struct Res {
  int32_t q;
  bool saturated;
  bool error;
  u128 rem;
};

class Div {
 public:
  Div() {
    m_ = new Vzhao_raster_attrdiv_v2;
    m_->clk = 0;
    m_->rst_n = 0;
    m_->v_valid_i = 0;
    m_->r_ready_i = 1;
    m_->area_i = 0;
    put_wide(m_->num_i, 0);
    m_->eval();
    for (int i = 0; i < 4; ++i) zhao::tick(*m_);
    m_->rst_n = 1;
    zhao::tick(*m_);
  }
  ~Div() {
    m_->final();
    delete m_;
  }

  // One divide, offered and drained through the real handshake.
  Res divide(i128 num, uint64_t area) {
    put_wide(m_->num_i, num);
    m_->area_i = area;
    m_->v_valid_i = 1;
    int guard = 0;
    while (!m_->v_ready_o) {
      zhao::tick(*m_);
      if (++guard > 5000) std::abort();
    }
    zhao::tick(*m_);
    m_->v_valid_i = 0;
    put_wide(m_->num_i, 0);
    m_->area_i = 0;
    guard = 0;
    while (!m_->r_valid_o) {
      zhao::tick(*m_);
      if (++guard > 5000) std::abort();
    }
    Res r{};
    r.q = static_cast<int32_t>(m_->q_o);
    r.saturated = m_->q_saturated_o != 0;
    r.error = m_->q_error_o != 0;
    // rem_o is 47 bits -> Verilator hands it back as a 64-bit scalar.
    r.rem = static_cast<u128>(m_->rem_o);
    zhao::tick(*m_);  // r_ready_i is held high, so this retires the answer
    return r;
  }

  uint32_t rem_range_errs() const { return m_->rem_range_err_o; }
  uint32_t divides() const { return m_->divides_o; }
  uint32_t saturations() const { return m_->saturations_o; }
  uint32_t errors() const { return m_->errors_o; }

 private:
  Vzhao_raster_attrdiv_v2* m_;
};

long g_pairs = 0;
long g_bad_inv = 0;
long g_bad_range = 0;
long g_ties = 0;
long g_neg = 0;

// The one assertion that matters: the published pair reconstructs the dividend.
void check_pair(Div& d, i128 num, uint64_t area) {
  const Res r = d.divide(num, area);
  const i128 A = static_cast<i128>(area);
  const i128 M = num + bias(area);
  const i128 qref = floor_div(M, A);
  const bool sat_ref = qref > static_cast<i128>(INT32_MAX) || qref < static_cast<i128>(INT32_MIN);

  if (sat_ref) {
    // A clamped quotient cannot satisfy q*A + rem == M, so the RTL publishes
    // zero and says so. Asserting the pair here would assert a falsehood.
    if (!r.saturated || r.rem != 0) ++g_bad_inv;
    return;
  }
  if (r.saturated || r.error) {
    ++g_bad_inv;
    return;
  }
  ++g_pairs;
  if (num < 0) ++g_neg;
  if (2 * floor_mod(M, A) == A) ++g_ties;

  if (r.rem >= static_cast<u128>(A)) ++g_bad_range;
  const i128 recon = static_cast<i128>(r.q) * A + static_cast<i128>(r.rem);
  if (recon != M) ++g_bad_inv;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Div d;

  std::printf("raster_attrdiv_v2_rem_directed (RADIX=%d)\n", ZHAO_ATTR_RADIX);

#ifdef EXPECT_REM_WIDTH_MUTANT
  // ---------------------------------------------------------------------
  // INVERTED POLARITY. This build compiled
  // tests/mutants/zhao_raster_attrdiv_v2_remwidth_mutant.sv ahead of the
  // production divider, so the restoring compare demands twice the divisor and
  // the residue invariant is rem < 2A instead of rem < A. THIS TEST PASSES WHEN
  // rem_range_err_o FIRES. It is evidence about the instrument, not about the
  // design; the design's correct behaviour is asserted positively by the
  // unmutated build of this same file, which requires the counter to stay zero.
  //
  // The stimulus is not arbitrary. The residue can only reach 2^47 when 2A
  // does, so the area is the widest legal one; the quotient is kept inside s32
  // because a saturating divide never enters D_RUN and so never reaches the
  // guard at all. Both constraints were found by transcribing the datapath and
  // searching it, after the obvious `>=`-to-`>` mutation was measured at ZERO
  // fires in 360,000 pairs.
  {
    const uint64_t area = (1ull << 47) - 1;
    long fired_at = -1;
    std::mt19937_64 rng(4242u);
    for (int t = 0; t < 4000 && fired_at < 0; ++t) {
      const i128 q = static_cast<i128>(static_cast<int64_t>(rng() % 4000000) - 2000000);
      const i128 off = static_cast<i128>(static_cast<u128>(rng()) % area);
      const i128 num = q * static_cast<i128>(area) + off - bias(area);
      const i128 M = num + bias(area);
      const i128 qref = floor_div(M, static_cast<i128>(area));
      if (qref > static_cast<i128>(INT32_MAX) || qref < static_cast<i128>(INT32_MIN)) continue;
      (void)d.divide(num, area);
      if (d.rem_range_errs() > 0) fired_at = t;
    }
    const bool fired = d.rem_range_errs() > 0;
    std::printf("   MUTANT BUILD: rem_range_err_o = %u after %ld divides\n", d.rem_range_errs(),
                fired_at < 0 ? 4000L : fired_at + 1);
    zhao::check(fired, "the committed mutant MOVES rem_range_err_o (inverted polarity)", 1,
                static_cast<uint64_t>(fired ? 1 : 0));
    const int mrc = zhao::report_and_exit("raster_attrdiv_v2_remwidth_mutant");
    zhao::exit_hard(mrc);
  }
#endif

  // --- 1. the Euclidean invariant, over constructed and random operands ----
  // Negative exact halves with an even divisor are the exact operands on which
  // v1 and v2 disagree (R100), so they are over-represented on purpose: they
  // are where a remainder fixup applied to the wrong branch would show.
  {
    std::mt19937_64 rng(20260920u);
    for (int t = 0; t < 6000; ++t) {
      const uint64_t area = 2 * ((rng() % 4000) + 1);  // even, so ties exist
      const i128 k = static_cast<i128>(static_cast<int64_t>(rng() % 4000) - 2000);
      check_pair(d, k * static_cast<i128>(area) - static_cast<i128>(area / 2), area);
    }
    for (int t = 0; t < 6000; ++t) {
      const uint64_t area = (rng() % 100000) + 1;
      const int nb = 1 + static_cast<int>(rng() % 70);
      const u128 mask = (static_cast<u128>(1) << nb) - 1;
      i128 v = static_cast<i128>(((static_cast<u128>(rng()) << 64) | rng()) & mask);
      if (rng() & 1) v = -v;
      check_pair(d, v, area);
    }
    zhao::check(g_bad_inv == 0, "q_o*area + rem_o == num + floor(area/2), always", 0,
                static_cast<uint64_t>(g_bad_inv));
    zhao::check(g_bad_range == 0, "0 <= rem_o < area, always", 0,
                static_cast<uint64_t>(g_bad_range));
    // Anti-vacuity: a sweep that never produced a tie or a negative numerator
    // would pass this section while testing none of what it claims.
    zhao::check(g_ties > 0, "the sweep actually produced exact halves", 1,
                static_cast<uint64_t>(g_ties > 0));
    zhao::check(g_neg > 0, "the sweep actually produced negative numerators", 1,
                static_cast<uint64_t>(g_neg > 0));
    std::printf("   %ld pairs, %ld exact halves, %ld negative numerators\n", g_pairs, g_ties,
                g_neg);
  }

  // --- 2. the refusal paths publish a zero, meaningless pair ---------------
  {
    const Res err = d.divide(12345, 0);
    zhao::check(err.error && err.rem == 0, "zero area: q_error_o set and rem_o is zero", 1,
                static_cast<uint64_t>(err.error && err.rem == 0));

    const uint64_t A = 12346;
    const i128 too_big = (static_cast<i128>(INT32_MAX) + 4) * static_cast<i128>(A);
    const Res sat = d.divide(too_big, A);
    zhao::check(sat.saturated && sat.q == INT32_MAX && sat.rem == 0,
                "positive saturation: clamped q, rem_o zero", 1,
                static_cast<uint64_t>(sat.saturated && sat.q == INT32_MAX && sat.rem == 0));

    const i128 too_small = (static_cast<i128>(INT32_MIN) - 4) * static_cast<i128>(A);
    const Res satn = d.divide(too_small, A);
    zhao::check(satn.saturated && satn.q == INT32_MIN && satn.rem == 0,
                "negative saturation: clamped q, rem_o zero", 1,
                static_cast<uint64_t>(satn.saturated && satn.q == INT32_MIN && satn.rem == 0));
  }

  // --- 3. the WIDTH claim R104 asked to be measured, not argued ------------
  // The widest legal area is 2^47-1, and the remainder's worst case is area-1.
  // Drive the extremes of the divisor and of the numerator together, then read
  // the RTL's own guard. This is the negative control for the mutant.
  {
    const uint64_t a_max = (1ull << 47) - 1;
    u128 widest = 0;
    long checked = 0;
    const uint64_t areas[] = {a_max, a_max - 1, (1ull << 46), (1ull << 46) + 1, 1, 2, 3};
    std::mt19937_64 rng(777u);
    for (uint64_t area : areas) {
      for (int t = 0; t < 400; ++t) {
        const i128 q = static_cast<i128>(static_cast<int64_t>(rng() % 40000) - 20000);
        // Land the remainder as close under `area` as the operands allow.
        const i128 off = static_cast<i128>(rng() % area);
        const i128 num = q * static_cast<i128>(area) + off - bias(area);
        const i128 M = num + bias(area);
        if (floor_div(M, static_cast<i128>(area)) > static_cast<i128>(INT32_MAX) ||
            floor_div(M, static_cast<i128>(area)) < static_cast<i128>(INT32_MIN))
          continue;
        const Res r = d.divide(num, area);
        if (r.saturated || r.error) continue;
        ++checked;
        if (r.rem > widest) widest = r.rem;
        if (r.rem >= static_cast<u128>(area)) ++g_bad_range;
        if (static_cast<i128>(r.q) * static_cast<i128>(area) + static_cast<i128>(r.rem) != M)
          ++g_bad_inv;
      }
    }
    int wbits = 0;
    for (u128 w = widest; w; w >>= 1) ++wbits;
    zhao::check(checked > 0, "the width sweep actually drove the widest areas", 1,
                static_cast<uint64_t>(checked > 0));
    zhao::check(wbits <= 47, "the widest observed rem_o fits 47 bits", 47,
                static_cast<uint64_t>(wbits));
    zhao::check(d.rem_range_errs() == 0,
                "rem_range_err_o stays zero: no residue escaped 47 bits", 0,
                d.rem_range_errs());
    zhao::check(g_bad_inv == 0, "the invariant survives the widest legal operands", 0,
                static_cast<uint64_t>(g_bad_inv));
    std::printf("   width sweep: %ld divides, widest rem_o = %d bits, range errors = %u\n",
                checked, wbits, d.rem_range_errs());
  }

  // --- 4. CONTINUITY ACROSS ZERO -------------------------------------------
  // This is the property that makes v2's pair a legal recurrence seed and v1's
  // not, and it is why ATTRSTEP can delete its negative branch rather than
  // port it. Walk M through zero one unit at a time and require the pair to
  // advance by the plain Euclidean rule with NO special case at the crossing.
  {
    long steps = 0, breaks = 0, crossings = 0;
    const uint64_t areas[] = {2, 3, 16, 17, 1000, 4096};
    for (uint64_t area : areas) {
      const i128 A = static_cast<i128>(area);
      i128 prev_q = 0;
      u128 prev_r = 0;
      bool have_prev = false;
      for (int k = -40; k <= 40; ++k) {
        const i128 num = static_cast<i128>(k) - bias(area);  // M = k
        const Res r = d.divide(num, area);
        if (r.saturated || r.error) continue;
        if (have_prev) {
          // one unit of dM: q += 0, r += 1, carry when r reaches A
          i128 xq = prev_q;
          u128 xr = prev_r + 1;
          if (xr >= static_cast<u128>(A)) {
            xr -= static_cast<u128>(A);
            xq += 1;
          }
          ++steps;
          if (xq != static_cast<i128>(r.q) || xr != r.rem) ++breaks;
          if (prev_q < 0 && r.q >= 0) ++crossings;
        }
        prev_q = r.q;
        prev_r = r.rem;
        have_prev = true;
      }
    }
    zhao::check(breaks == 0, "the pair steps by the plain rule with no case at zero", 0,
                static_cast<uint64_t>(breaks));
    zhao::check(crossings > 0, "the walk actually crossed zero", 1,
                static_cast<uint64_t>(crossings > 0));
    std::printf("   continuity: %ld unit steps across %ld sign crossings, %ld breaks\n", steps,
                crossings, breaks);
  }

  // --- 5. the step decomposition ATTRSTEP needs, against the real RTL ------
  // v2 always adds the bias, so recovering the plain Euclidean division of dNdx
  // by A costs one compare and one add:
  //     r' >= h : dq = q',     dr = r' - h
  //     else    : dq = q' - 1, dr = r' + A - h
  // Proven here against the divider rather than against a restatement of it,
  // because this is the expression that replaces zhao_raster_attrstep's f/g
  // recovery and a restatement could be wrong in the same direction twice.
  {
    std::mt19937_64 rng(31337u);
    long checked = 0, bad = 0;
    for (int t = 0; t < 4000; ++t) {
      const uint64_t area = (rng() % 50000) + 1;
      const int nb = 1 + static_cast<int>(rng() % 60);
      const u128 mask = (static_cast<u128>(1) << nb) - 1;
      i128 dn = static_cast<i128>(((static_cast<u128>(rng()) << 64) | rng()) & mask);
      if (rng() & 1) dn = -dn;
      const Res r = d.divide(dn, area);
      if (r.saturated || r.error) continue;
      ++checked;
      const i128 A = static_cast<i128>(area);
      const i128 h = bias(area);
      i128 dq;
      u128 dr;
      if (static_cast<i128>(r.rem) >= h) {
        dq = r.q;
        dr = r.rem - static_cast<u128>(h);
      } else {
        dq = static_cast<i128>(r.q) - 1;
        dr = r.rem + static_cast<u128>(A) - static_cast<u128>(h);
      }
      if (dq != floor_div(dn, A) || dr != static_cast<u128>(floor_mod(dn, A))) ++bad;
    }
    zhao::check(checked > 0, "the step-recovery sweep drove real divides", 1,
                static_cast<uint64_t>(checked > 0));
    zhao::check(bad == 0, "bias recovery yields the plain Euclidean pair of dNdx by area", 0,
                static_cast<uint64_t>(bad));
    std::printf("   step recovery: %ld decompositions, %ld wrong\n", checked, bad);
  }

  // --- 6. the evidence counters moved ---------------------------------------
  {
    zhao::check(d.divides() > 0, "divides_o counted the work", 1,
                static_cast<uint64_t>(d.divides() > 0));
    zhao::check(d.saturations() >= 2, "saturations_o fired on both rails", 1,
                static_cast<uint64_t>(d.saturations() >= 2));
    zhao::check(d.errors() > 0, "errors_o fired on the zero-area refusal", 1,
                static_cast<uint64_t>(d.errors() > 0));
    std::printf("   counters: divides=%u saturations=%u errors=%u rem_range_errs=%u\n",
                d.divides(), d.saturations(), d.errors(), d.rem_range_errs());
  }

  const int rc = zhao::report_and_exit("raster_attrdiv_v2_rem_directed");
  zhao::exit_hard(rc);
}
