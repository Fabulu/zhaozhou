// perspuv_pairpipe_directed.cpp
//
// ---------------------------------------------------------------------------
// THE PAIRED PERSPUV CANDIDATE AGAINST THE FROZEN SERVICE
// ---------------------------------------------------------------------------
// Roadmap packet 4. The candidate keeps ONE scheduler (licensed by the
// inductive lockstep proof and its per-cycle backstop) and TWO arithmetic lanes,
// and deletes the sixteen-entry operand tables, the `e_have` join and the
// `e_q_u`/`e_q_v` result tables that existed only to reassemble axes that never
// separated.
//
// A rewrite is only worth having if it computes the same thing, so this is a
// differential: same stimulus into both, results compared in ACCEPTED ORDER.
// The bench offers each fragment to both engines and accepts only when both are
// ready, so the accepted sequence is identical by construction — comparing two
// engines that admitted different workloads would be the mismatched-comparison
// mistake in bench form.
//
// ONE INTENTIONAL DIFFERENCE, declared rather than discovered: for a depth-zero
// fragment the service returns whatever the previous user of that token left in
// `e_q_u`/`e_q_v`, because such a fragment sets `e_have` at allocation and never
// writes a result. That is stale-by-contract — the caller is told to read
// `depth_zero_o` first — and the candidate returns deterministic zero instead.
// So U/V are compared only on non-depth-zero results, and the depth-zero FLAG
// and TAG are compared on all of them. Excluding a comparison silently is how a
// differential becomes decoration; this one says what it excludes and why.
#include "Vtb_perspuv_pairpipe.h"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vtb_perspuv_pairpipe* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

struct Res {
  int32_t u, v;
  uint16_t tag;
  uint8_t sat, dz;
};

struct Stim {
  int32_t nu, nv;
  uint32_t mant;
  uint8_t k, dz;
  uint16_t tag;
};

// The stimulus classes §8.6 makes mandatory. Signed extremes and the halfway
// rounding case are not decoration: the 64-bit rescale exists because of a k=32
// counterexample where `sh - 1` wraps to 63, and a sweep of pleasant midrange
// values would pass with a 56-bit intermediate.
std::vector<Stim> build_stimulus() {
  std::vector<Stim> s;
  const int32_t kNums[] = {0,
                           1,
                           -1,
                           0x7FFFFFFF,
                           static_cast<int32_t>(0x80000000),
                           0x7FFFFFFE,
                           static_cast<int32_t>(0x80000001),
                           0x00010000,
                           -0x00010000,
                           0x00008000,
                           -0x00008000};
  const uint8_t kKs[] = {0, 1, 2, 31, 32, 33, 62, 63};
  const uint32_t kMants[] = {0x800000u, 0xFFFFFFu, 0x800001u, 0xABCDEFu};

  uint16_t tag = 0;
  for (int32_t nu : kNums) {
    for (uint8_t k : kKs) {
      for (uint32_t m : kMants) {
        Stim x;
        x.nu = nu;
        // A DIFFERENT numerator per axis, so a lane that copied the other's
        // operand -- the exact failure a merged lane would introduce -- cannot
        // pass by symmetry.
        x.nv = static_cast<int32_t>(~static_cast<uint32_t>(nu) + 1u) ^ 0x00010001;
        x.mant = m;
        x.k = k;
        x.dz = 0;
        // REPEATED TAGS, deliberately: the tag space wraps well before the
        // stimulus does, so full-window reuse happens many times over.
        x.tag = tag++;
        s.push_back(x);
      }
    }
  }
  // Depth-zero fragments interleaved, so a zero cannot be shown to keep order
  // only when it is alone.
  const size_t n = s.size();
  for (size_t i = 0; i < n; i += 7) {
    Stim z = s[i];
    z.dz = 1;
    z.tag = static_cast<uint16_t>(tag++);
    s.push_back(z);
  }
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_perspuv_pairpipe* d = new Vtb_perspuv_pairpipe;

  d->clk = 0;
  d->rst_n = 0;
  d->v_valid_i = 0;
  d->a_ready_i = 0;
  d->b_ready_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  const std::vector<Stim> stim = build_stimulus();

  std::deque<Res> a_out, b_out;
  size_t fed = 0;
  int nonzero_accepts = 0, zero_accepts = 0;
  uint32_t rng = 0x9E3779B9u;

  for (int cyc = 0; cyc < 400000 && (fed < stim.size() || a_out.size() < fed || b_out.size() < fed);
       ++cyc) {
    rng = rng * 1664525u + 1013904223u;
    // Independent, uneven drain on the two engines. Equal drain would let a
    // candidate that only works when both sides are ready on the same cycle
    // pass.
    d->a_ready_i = ((rng >> 5) & 3u) != 0u;
    d->b_ready_i = ((rng >> 13) & 7u) != 0u;

    if (fed < stim.size()) {
      const Stim& x = stim[fed];
      d->v_valid_i = 1;
      d->u_over_w_i = x.nu;
      d->v_over_w_i = x.nv;
      d->r_mant_i = x.mant;
      d->r_k_i = x.k;
      d->depth_zero_i = x.dz;
      d->tag_i = x.tag;
    } else {
      d->v_valid_i = 0;
    }
    d->eval();

    if (d->v_valid_i && d->v_ready_o) {
      if (stim[fed].dz)
        ++zero_accepts;
      else
        ++nonzero_accepts;
      ++fed;
    }
    if (d->a_valid_o && d->a_ready_i) {
      Res r;
      r.u = d->a_u_o;
      r.v = d->a_v_o;
      r.tag = d->a_tag_o;
      r.sat = d->a_sat_o;
      r.dz = d->a_dz_o;
      a_out.push_back(r);
    }
    if (d->b_valid_o && d->b_ready_i) {
      Res r;
      r.u = d->b_u_o;
      r.v = d->b_v_o;
      r.tag = d->b_tag_o;
      r.sat = d->b_sat_o;
      r.dz = d->b_dz_o;
      b_out.push_back(r);
    }
    tick(d);
  }
  d->v_valid_i = 0;
  d->eval();

  // ---- the differential ----------------------------------------------------
  int uv_mismatch = 0, tag_mismatch = 0, sat_mismatch = 0, dz_mismatch = 0;
  int compared_uv = 0, saw_sat = 0, saw_dz = 0;
  int compared_dz = 0, dz_nonzero = 0, dz_sat = 0;
  const size_t n = a_out.size() < b_out.size() ? a_out.size() : b_out.size();
  for (size_t i = 0; i < n; ++i) {
    const Res& a = a_out[i];
    const Res& b = b_out[i];
    if (a.tag != b.tag) ++tag_mismatch;
    if (a.dz != b.dz) ++dz_mismatch;
    if (a.sat != b.sat) ++sat_mismatch;
    if (a.dz) {
      ++saw_dz;
      // THE EXCLUSION HAS TO EARN ITSELF. U/V are not compared against the
      // service here because the service returns whatever the previous user of
      // that token left in e_q_u/e_q_v -- stale by contract. But the candidate's
      // side of that claim is "deterministic ZERO", and an excluded comparison
      // with nothing asserted in its place is just a gap with a comment on it.
      //
      // So the claim is checked directly: not "differs from svc", which would be
      // satisfied by any garbage, but exactly zero.
      if (b.u != 0 || b.v != 0) ++dz_nonzero;
      ++compared_dz;
    } else {
      if (a.u != b.u || a.v != b.v) ++uv_mismatch;
      ++compared_uv;
    }
    if (a.sat) ++saw_sat;
    // And a depth-zero result must not claim saturation: no product was formed,
    // so there was nothing to saturate.
    if (b.dz && b.sat) ++dz_sat;
  }

  std::printf("  fed %zu (nonzero %d, depth-zero %d) | service emitted %zu, candidate %zu\n", fed,
              nonzero_accepts, zero_accepts, a_out.size(), b_out.size());
  std::printf("  compared: %d U/V pairs, %d depth-zero (U/V excluded), %d saturating\n",
              compared_uv, saw_dz, saw_sat);
  std::printf("  mismatches: u/v %d, tag %d, sat %d, dz %d\n", uv_mismatch, tag_mismatch,
              sat_mismatch, dz_mismatch);
  std::printf(
      "  counters: service frags %u products %u | candidate frags %u "
      "products %u zero-products %u\n",
      d->a_fragments_o, d->a_products_o, d->b_fragments_o, d->b_products_o, d->b_zero_products_o);

  zhao::check(fed == stim.size(), "the whole stimulus was accepted", stim.size(), fed);
  zhao::check(a_out.size() == fed && b_out.size() == fed,
              "both engines emitted exactly one result per accepted fragment -- "
              "none dropped, none duplicated",
              fed * 2, a_out.size() + b_out.size());

  // Non-vacuity BEFORE the zeros are believed. A differential over a stimulus
  // that never saturates and never rounds at the halfway point is a comparison
  // of two constants.
  zhao::check(saw_dz > 0, "depth-zero fragments were exercised", 1, saw_dz > 0 ? 1 : 0);
  zhao::check(saw_sat > 0,
              "and SATURATING results were exercised -- without these the "
              "saturation comparison below is vacuous",
              1, saw_sat > 0 ? 1 : 0);
  zhao::check(compared_uv > 200, "and a substantial number of U/V pairs", 1,
              compared_uv > 200 ? 1 : 0);

  zhao::check(compared_dz > 0,
              "depth-zero results were actually observed, so the two checks "
              "below are about something",
              1, compared_dz > 0 ? 1 : 0);
  zhao::check(dz_nonzero == 0,
              "every depth-zero result carries U = V = 0 EXACTLY. This is the "
              "candidate's side of the one declared difference from the service, "
              "and it is asserted rather than left as an excluded comparison -- "
              "'differs from svc' would be satisfied by any garbage",
              0, static_cast<uint64_t>(dz_nonzero));
  zhao::check(dz_sat == 0,
              "and no depth-zero result claims saturation: no product was formed, "
              "so there was nothing to saturate",
              0, static_cast<uint64_t>(dz_sat));

  zhao::check(uv_mismatch == 0,
              "the candidate's U and V are BIT-IDENTICAL to the frozen "
              "service's across every signed extreme, every exponent including "
              "k=32 where sh-1 wraps to 63, and every mantissa -- the 64-bit "
              "rescale was copied, not retyped",
              0, static_cast<uint64_t>(uv_mismatch));
  zhao::check(tag_mismatch == 0,
              "and every result carries the same tag in the same ORDER: the "
              "interface order is the contract, and a depth-zero fragment takes "
              "the same ordered path rather than a bypass that could overtake",
              0, static_cast<uint64_t>(tag_mismatch));
  zhao::check(sat_mismatch == 0, "and the same saturation flag", 0,
              static_cast<uint64_t>(sat_mismatch));
  zhao::check(dz_mismatch == 0, "and the same depth-zero flag", 0,
              static_cast<uint64_t>(dz_mismatch));

  // ---- the counts, which output equality cannot see ------------------------
  zhao::check(d->b_products_o == static_cast<uint32_t>(nonzero_accepts) * 2u,
              "the candidate launched exactly TWO products per nonzero "
              "fragment -- two lanes, one pass. A machine that reissued would "
              "produce identical output and disagree only here",
              static_cast<uint64_t>(nonzero_accepts) * 2u, d->b_products_o);
  zhao::check(d->b_zero_products_o == static_cast<uint32_t>(zero_accepts),
              "and no products at all for the depth-zero fragments",
              static_cast<uint64_t>(zero_accepts), d->b_zero_products_o);
  zhao::check(d->a_fragments_o == d->b_fragments_o, "both engines counted the same admissions",
              d->a_fragments_o, d->b_fragments_o);

  // =========================================================================
  // THE CREDIT CEILING, under a long output stall
  // =========================================================================
  // Credits are reserved at ACCEPTANCE and released at EXTERNAL acceptance,
  // never at pipeline writeback. Releasing at writeback is the cache's
  // documented lost-response bug: the producer is told there is room while the
  // item still occupies terminal storage, and a long stall then overruns it.
  //
  // The ceiling is measured against the CANDIDATE'S OWN ready, not the bench's
  // AND of both. Using the AND would stop at whichever engine filled first and
  // report the service's NTOK as the candidate's CAP -- a measurement of the
  // wrong machine, arrived at honestly.
  {
    d->rst_n = 0;
    d->v_valid_i = 0;
    d->a_ready_i = 1;  // the service drains freely, so it never gates
    d->b_ready_i = 0;  // the candidate's output held SHUT
    tick(d);
    tick(d);
    d->rst_n = 1;
    tick(d);

    int accepted = 0;
    for (int i = 0; i < 300; ++i) {
      d->v_valid_i = 1;
      d->u_over_w_i = 0x00010000 + i;
      d->v_over_w_i = 0x00020000 - i;
      d->r_mant_i = 0x800000u;
      d->r_k_i = 16;
      d->depth_zero_i = 0;
      d->tag_i = static_cast<uint16_t>(i);
      d->eval();
      if (d->v_valid_i && d->b_vready_o && d->a_vready_o) ++accepted;
      tick(d);
    }
    d->v_valid_i = 0;
    d->eval();

    std::printf("  output held shut: candidate accepted %d, its ready now %u, occupancy %u\n",
                accepted, d->b_vready_o, d->b_occupancy_o);

    zhao::check(d->b_vready_o == 0,
                "with its output held shut the candidate eventually REFUSES -- a "
                "candidate that kept accepting would be losing responses",
                0, d->b_vready_o);
    zhao::check(accepted == 17,
                "and the burst ceiling is exactly 17: sixteen terminal entries "
                "plus the held output register. Credits released at pipeline "
                "writeback instead of at external acceptance would let this run "
                "past 17 -- that is this check's falsifier",
                17, accepted);
    zhao::check(d->b_occupancy_o == 17,
                "and the block's own owned-count agrees with what the wire showed", 17,
                d->b_occupancy_o);

    // Release exactly one, and exactly one more must become acceptable.
    d->b_ready_i = 1;
    d->eval();
    tick(d);
    d->b_ready_i = 0;
    d->eval();
    int extra = 0;
    for (int i = 0; i < 10; ++i) {
      d->v_valid_i = 1;
      d->tag_i = static_cast<uint16_t>(0x900 + i);
      d->eval();
      if (d->v_valid_i && d->b_vready_o && d->a_vready_o) ++extra;
      tick(d);
    }
    d->v_valid_i = 0;
    d->eval();
    std::printf("  released one, accepted %d more\n", extra);
    zhao::check(extra == 1,
                "releasing ONE emitted result frees exactly ONE credit -- not "
                "zero (a credit never returned) and not several (a credit "
                "returned more than once)",
                1, extra);
  }

  // =========================================================================
  // RESET MID-FLIGHT
  // =========================================================================
  // §8.7 lists this and it is easy to skip, because a block that is only ever
  // reset while idle looks fine forever. The failure it guards against is a
  // credit counter or a FIFO pointer that survives reset: the block then comes
  // back believing it owns items that no longer exist, and either refuses work
  // it could take or -- worse -- emits a stale record with a valid-looking tag.
  {
    d->rst_n = 1;
    d->a_ready_i = 0;
    d->b_ready_i = 0;  // both outputs shut, so the pipeline is genuinely loaded
    for (int i = 0; i < 12; ++i) {
      d->v_valid_i = 1;
      d->u_over_w_i = 0x0BAD0000 + i;
      d->v_over_w_i = 0x0BAD1000 + i;
      d->r_mant_i = 0xC0FFEEu;
      d->r_k_i = 24;
      d->depth_zero_i = 0;
      d->tag_i = static_cast<uint16_t>(0xEE00 + i);
      d->eval();
      tick(d);
    }
    d->v_valid_i = 0;
    d->eval();
    const unsigned loaded = d->b_occupancy_o;

    // Yank it mid-flight.
    d->rst_n = 0;
    tick(d);
    tick(d);
    d->rst_n = 1;
    d->eval();
    tick(d);
    d->eval();

    std::printf("  reset mid-flight: occupancy %u -> %u, valid %u\n", loaded, d->b_occupancy_o,
                d->b_valid_o);

    zhao::check(loaded > 0,
                "the pipeline was genuinely LOADED before the reset -- resetting "
                "an idle block proves nothing",
                1, loaded > 0 ? 1 : 0);
    zhao::check(d->b_occupancy_o == 0,
                "reset clears the credit count: a surviving count would make the "
                "block refuse work it could take, or believe it owns items that "
                "no longer exist",
                0, d->b_occupancy_o);
    zhao::check(d->b_valid_o == 0,
                "and no stale record is presented after reset -- an output valid "
                "that survives would emit a plausible tag for a fragment nobody "
                "submitted",
                0, d->b_valid_o);

    // And it must be usable again: one clean fragment straight through.
    d->a_ready_i = 1;
    d->b_ready_i = 1;
    int post = 0;
    uint16_t got_tag = 0;
    for (int i = 0; i < 40 && post == 0; ++i) {
      d->v_valid_i = (i < 1) ? 1 : 0;
      d->u_over_w_i = 0x00010000;
      d->v_over_w_i = 0x00020000;
      d->r_mant_i = 0x800000u;
      d->r_k_i = 16;
      d->depth_zero_i = 0;
      d->tag_i = 0x1234;
      d->eval();
      if (d->b_valid_o && d->b_ready_i) {
        got_tag = d->b_tag_o;
        ++post;
      }
      tick(d);
    }
    d->v_valid_i = 0;
    d->eval();
    zhao::check(post == 1 && got_tag == 0x1234,
                "and the block works again after reset, returning the tag of the "
                "fragment actually submitted rather than one left over from "
                "before",
                0x1234, got_tag);
  }

  // =========================================================================
  // SUSTAINED RATE, measured and printed rather than predicted
  // =========================================================================
  {
    d->rst_n = 0;
    d->v_valid_i = 0;
    d->a_ready_i = 1;
    d->b_ready_i = 1;
    tick(d);
    tick(d);
    d->rst_n = 1;
    tick(d);

    const int kBurst = 128;
    int acc = 0, clocks = 0;
    for (int i = 0; i < 2000 && acc < kBurst; ++i) {
      d->v_valid_i = 1;
      d->u_over_w_i = 0x00030000 + acc;
      d->v_over_w_i = 0x00040000 - acc;
      d->r_mant_i = 0x9ABCDEu;
      d->r_k_i = 20;
      d->depth_zero_i = 0;
      d->tag_i = static_cast<uint16_t>(acc);
      d->eval();
      if (d->v_valid_i && d->v_ready_o) ++acc;
      tick(d);
      ++clocks;
    }
    d->v_valid_i = 0;
    d->eval();
    const double per = static_cast<double>(clocks) / (acc ? acc : 1);
    std::printf("  sustained: %d pairs in %d clocks (%.2f per pair)\n", acc, clocks, per);
    zhao::check(acc == kBurst, "the rate burst completed", kBurst, acc);
    // No magnitude is predicted -- today's two falsified predictions were both
    // magnitudes. The claim is structural: a PAIRED pipeline takes one pair per
    // clock, so anything near 1 is one-per-clock and a serialized pair lands
    // near 2.
    zhao::check(per < 1.5,
                "the candidate sustains ONE PAIR PER CLOCK with the consumer "
                "ready -- dropping a lane and serializing the axes is the "
                "falsifier and would land near two",
                1, per < 1.5 ? 1 : 0);
  }

  const int rc = zhao::report_and_exit("perspuv_pairpipe_directed");
  delete d;
  zhao::exit_hard(rc);
}
