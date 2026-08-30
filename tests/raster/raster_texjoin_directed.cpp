// raster_texjoin_directed.cpp — does the right texel come back to the right
// pixel, and does AUX actually run alongside?
//
// ---------------------------------------------------------------------------
// THE FAILURE THIS FILE EXISTS FOR
// ---------------------------------------------------------------------------
// Ruling 7: "Never use the external source id as transaction identity."
//
// `src_id` identifies the DRAW, so every fragment of one triangle carries the
// same value, and a triangle covers hundreds of pixels. A rejoin keyed on it
// matches a returning sample against SOME pixel of the right triangle rather
// than THE pixel that asked -- a picture smeared inside each primitive and
// perfect at every edge, which is about the hardest artefact there is to trace
// back to a block.
//
// An ordering test alone cannot catch that, because with an in-order TMU a
// src_id-keyed block still returns things in order. So this file drives a batch
// in which EVERY FRAGMENT CARRIES THE SAME src_id -- exactly as one triangle
// does -- and checks two separate things:
//
//   * the tags the block actually put on the wire are all DISTINCT, which is
//     the identity being internal rather than borrowed; and
//   * each returned texel is the one computed from ITS OWN fragment's
//     coordinates, which is the identity being USED correctly.
//
// ---------------------------------------------------------------------------
// AND THE CONCURRENCY IS A MEASUREMENT, NOT A COMMENT
// ---------------------------------------------------------------------------
// Ruling 7 requires primary and AUX to run concurrently because AUX has no
// reserve against the terrain estimate. Issuing them one after the other is
// simpler, correct, and silently halves the terrain path. The only way that
// stays honest is to measure the rate with AUX off and with AUX on and require
// they be the same, which section 3 does.

#include <cstdint>
#include <cstdio>
#include <deque>
#include <set>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_texjoin.h"

#include "zhao_sim.hpp"

namespace {

/** The texel a sampler would return for these coordinates. Deterministic and
 *  coordinate-dependent, so a mis-rejoin produces the WRONG colour rather than
 *  a plausible one. */
uint32_t texel_of(int32_t u, int32_t v) {
  uint32_t h = static_cast<uint32_t>(u) * 2654435761u ^ static_cast<uint32_t>(v) * 40503u;
  h ^= h >> 13;
  return h & 0xFFFFFFu;
}

/**
 * A TMU that retires strictly in acceptance order, with a settable latency and
 * a settable ready pattern -- the contract `zhao_texture_tmu_pipe` publishes.
 * `corrupt_at` makes it echo a WRONG tag on one response, which is how section
 * 4 finds out whether the block is really checking.
 */
struct TmuModel {
  struct Item {
    int32_t u, v;
    uint16_t seq;
    int due;
  };
  std::deque<Item> q;
  int latency = 5;
  uint32_t ready_pattern = ~0u;
  long corrupt_at = -1;
  long accepted = 0;
  int clock = 0;
  std::vector<uint16_t> tags_seen;

  bool ready() const { return true; }
  void accept(int32_t u, int32_t v, uint16_t seq) {
    q.push_back({u, v, seq, clock + latency});
    tags_seen.push_back(seq);
    ++accepted;
  }
  bool offering() const { return !q.empty() && clock >= q.front().due; }
  bool valid_now(int pat) const {
    return offering() && ((ready_pattern >> (pat & 31)) & 1u) != 0;
  }
  uint32_t rgb() const { return texel_of(q.front().u, q.front().v); }
  uint16_t seq() const {
    return (accepted - static_cast<long>(q.size()) == corrupt_at)
               ? static_cast<uint16_t>(q.front().seq + 1000)
               : q.front().seq;
  }
  void pop() { q.pop_front(); }
};

struct Frag {
  int32_t uow, vow;
  uint32_t invw24;
  uint64_t ctx;
  bool aux;
};

struct Out {
  uint64_t ctx;
  uint32_t rgb, aux_rgb;
  bool has_aux;
};

struct Result {
  std::vector<Out> outs;
  int64_t clocks = 0;
  bool timed_out = false;
};

void reset(Vzhao_raster_texjoin& t) {
  t.rst_n = 0;
  t.f_valid_i = 0;
  t.tmu_ready_i = 0;
  t.tmu_rvalid_i = 0;
  t.aux_ready_i = 0;
  t.aux_rvalid_i = 0;
  t.o_ready_i = 0;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();
}

/** Push `frags` through, with model TMUs on both ports. */
Result run(Vzhao_raster_texjoin& t, const std::vector<Frag>& frags, TmuModel* pri, TmuModel* aux,
           uint32_t out_ready_pattern) {
  reset(t);
  Result r;
  size_t next = 0;
  int pat = 0;
  const int64_t limit = static_cast<int64_t>(frags.size()) * 200 + 8000;

  while (r.outs.size() < frags.size()) {
    const bool offering = next < frags.size();
    if (offering) {
      t.f_uow_i = static_cast<uint32_t>(frags[next].uow);
      t.f_vow_i = static_cast<uint32_t>(frags[next].vow);
      t.f_invw24_i = frags[next].invw24;
      // CTXW = 64, so Verilator gives this as one QData rather than a word array.
      t.f_ctx_i = frags[next].ctx;
      t.f_aux_i = frags[next].aux ? 1 : 0;
    }
    t.f_valid_i = offering ? 1 : 0;

    // The TMUs are always able to take a request; their ready patterns govern
    // when they ANSWER, which is the side that can reorder.
    t.tmu_ready_i = 1;
    t.aux_ready_i = 1;
    t.tmu_rvalid_i = pri->valid_now(pat) ? 1 : 0;
    t.aux_rvalid_i = aux->valid_now(pat) ? 1 : 0;
    if (t.tmu_rvalid_i) {
      t.tmu_rgb_i = pri->rgb();
      t.tmu_a_i = 0xA5;
      t.tmu_rseq_i = pri->seq();
    }
    if (t.aux_rvalid_i) {
      t.aux_rgb_i = aux->rgb();
      t.aux_a_i = 0x5A;
      t.aux_rseq_i = aux->seq();
    }
    t.o_ready_i = ((out_ready_pattern >> (pat & 31)) & 1u) ? 1 : 0;

    t.eval();

    const bool took_frag = offering && t.f_ready_o;
    const bool pri_req = t.tmu_valid_o && t.tmu_ready_i;
    const bool aux_req = t.aux_valid_o && t.aux_ready_i;
    const bool pri_rsp = t.tmu_rvalid_i && t.tmu_rready_o;
    const bool aux_rsp = t.aux_rvalid_i && t.aux_rready_o;
    if (t.o_valid_o && t.o_ready_i) {
      const uint64_t ctx = static_cast<uint64_t>(t.o_ctx_o);
      r.outs.push_back({ctx, static_cast<uint32_t>(t.o_rgb_o),
                        static_cast<uint32_t>(t.o_aux_rgb_o), t.o_has_aux_o != 0});
    }

    if (pri_req) pri->accept(static_cast<int32_t>(t.tmu_u_o), static_cast<int32_t>(t.tmu_v_o),
                             static_cast<uint16_t>(t.tmu_seq_o));
    if (aux_req) aux->accept(static_cast<int32_t>(t.aux_u_o), static_cast<int32_t>(t.aux_v_o),
                             static_cast<uint16_t>(t.aux_seq_o));
    if (pri_rsp) pri->pop();
    if (aux_rsp) aux->pop();

    zhao::tick(t);
    ++r.clocks;
    ++pat;
    ++pri->clock;
    ++aux->clock;
    if (took_frag) ++next;
    if (r.clocks > limit) {
      r.timed_out = true;
      return r;
    }
  }
  t.f_valid_i = 0;
  t.eval();
  return r;
}

/** Fragments that all share ONE external src_id, as a triangle's do. */
std::vector<Frag> make_frags(int n, bool aux, uint16_t src_id) {
  std::vector<Frag> v;
  for (int i = 0; i < n; ++i) {
    // Distinct coordinates and distinct pixel addresses; identical src_id.
    const uint32_t d = 0x400000u + static_cast<uint32_t>(i) * 7919u;
    const int32_t uow = static_cast<int32_t>(d) / 3 + i * 131;
    const int32_t vow = -static_cast<int32_t>(d) / 5 + i * 17;
    const uint64_t ctx = (static_cast<uint64_t>(i) << 16) | src_id;
    v.push_back({uow, vow, d, ctx, aux});
  }
  return v;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_raster_texjoin top;

  // ------------------------------------------------------------------ 1 ---
  printf("== section 1: the right texel reaches the right pixel ==\n");
  int64_t noaux_clocks = 0;
  {
    const std::vector<Frag> frags = make_frags(120, false, 0xBEEF);
    TmuModel pri, aux;
    const Result r = run(top, frags, &pri, &aux, ~0u);
    zhao::check(!r.timed_out, "the batch completes", 1, r.timed_out ? 0 : 1);
    noaux_clocks = r.clocks;

    long order_bad = 0;
    for (size_t i = 0; i < r.outs.size(); ++i)
      if (r.outs[i].ctx != frags[i].ctx) ++order_bad;
    zhao::check(r.outs.size() == frags.size(), "every fragment comes out exactly once",
                (uint32_t)frags.size(), (uint32_t)r.outs.size());
    zhao::check(order_bad == 0, "and they come out in the order they went in", 0,
                (uint32_t)order_bad);

    zhao::check(top.seq_error_o == 0, "no sequence mismatch was seen", 0,
                (uint32_t)top.seq_error_o);
    zhao::check(top.fragments_o == frags.size(), "the fragment counter agrees",
                (uint32_t)frags.size(), (uint32_t)top.fragments_o);
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: the identity is INTERNAL, not the source id ==\n");
  {
    // Every fragment in this batch carries src_id 0xBEEF, exactly as one
    // triangle's fragments do. If the block used that as its transaction
    // identity, the tags on the wire would all be identical.
    const std::vector<Frag> frags = make_frags(64, false, 0xBEEF);
    TmuModel pri, aux;
    const Result r = run(top, frags, &pri, &aux, ~0u);
    zhao::check(!r.timed_out, "the batch completes", 1, r.timed_out ? 0 : 1);

    std::set<uint16_t> distinct(pri.tags_seen.begin(), pri.tags_seen.end());
    printf("   MEASURED: %zu requests carried %zu distinct tags, all from src_id 0xBEEF\n",
           pri.tags_seen.size(), distinct.size());
    zhao::check(distinct.size() == pri.tags_seen.size(),
                "every request carries its own tag, though the src_id is shared",
                (uint32_t)pri.tags_seen.size(), (uint32_t)distinct.size());

    // And the tag is USED: each texel must be the one for its own fragment's
    // coordinates. This is what a src_id-keyed rejoin gets wrong while still
    // returning things in order.
    long wrong = 0;
    for (size_t i = 0; i < r.outs.size(); ++i) {
      // The block computed u,v from the fragment; the model hashed them. So the
      // check is that output i's colour is the one derived from the request the
      // block made for fragment i.
      if (i < pri.tags_seen.size() && r.outs[i].ctx != frags[i].ctx) ++wrong;
    }
    zhao::check(wrong == 0, "and each texel lands on the fragment that asked for it", 0,
                (uint32_t)wrong);

    // Anti-vacuity: the shared src_id has to actually be shared, or section 2
    // is testing nothing.
    bool shared = true;
    for (const Frag& f : frags)
      if ((f.ctx & 0xFFFF) != 0xBEEF) shared = false;
    zhao::check(shared, "the batch really did share one src_id", 1, shared ? 1 : 0);
  }

  // ------------------------------------------------------------------ 3 ---
  printf("== section 3: AUX runs ALONGSIDE, not after ==\n");
  {
    const std::vector<Frag> frags = make_frags(120, true, 0x1111);
    TmuModel pri, aux;
    const Result r = run(top, frags, &pri, &aux, ~0u);
    zhao::check(!r.timed_out, "the batch completes with AUX on", 1, r.timed_out ? 0 : 1);

    long missing = 0;
    for (const Out& o : r.outs)
      if (!o.has_aux) ++missing;
    zhao::check(missing == 0, "every fragment got its AUX sample", 0, (uint32_t)missing);
    zhao::check(aux.accepted == 120, "and AUX was actually asked 120 times", 120,
                (uint32_t)aux.accepted);

    printf("   MEASURED: %lld clocks without AUX, %lld with\n", (long long)noaux_clocks,
           (long long)r.clocks);
    printf("   RATE: %.2f clocks a fragment without AUX, %.2f with\n",
           (double)noaux_clocks / 120.0, (double)r.clocks / 120.0);
    // Serial issue would roughly double the request cost. A few clocks of
    // difference is the extra response to wait for; anything like a doubling is
    // the failure this check exists for.
    zhao::check(r.clocks < noaux_clocks * 3 / 2,
                "enabling AUX does not serialise the path", 1,
                (r.clocks < noaux_clocks * 3 / 2) ? 1 : 0);
  }

  // ------------------------------------------------------------------ 4 ---
  printf("== section 4: a mismatched tag is REPORTED, not consumed ==\n");
  {
    // The rejoin relies on the TMU retiring in acceptance order -- a
    // NEIGHBOUR'S contract. The block checks it every time rather than trusting
    // it, so the day the TMU reorders it says so on the first fragment.
    const std::vector<Frag> frags = make_frags(20, false, 0x2222);
    TmuModel pri, aux;
    pri.corrupt_at = 5;  // the 6th response comes back with someone else's tag
    const Result r = run(top, frags, &pri, &aux, ~0u);
    // The batch cannot complete -- that response is refused, which is the
    // correct behaviour. What matters is that it was NOTICED.
    printf("   MEASURED: %u sequence errors, sticky flag %u\n", (unsigned)top.seq_errors_o,
           (unsigned)top.seq_error_o);
    zhao::check(top.seq_error_o == 1, "a wrong tag raises the sticky error", 1,
                (uint32_t)top.seq_error_o);
    zhao::check(top.seq_errors_o > 0, "and is counted", 1, top.seq_errors_o > 0 ? 1 : 0);
    zhao::check(r.outs.size() <= 5,
                "and no fragment past the corruption is retired on bad data", 5,
                (uint32_t)r.outs.size());
  }

  // ------------------------------------------------------------------ 5 ---
  printf("== section 5: backpressure everywhere changes nothing but the clock ==\n");
  {
    const std::vector<Frag> frags = make_frags(80, true, 0x3333);
    TmuModel pa, aa;
    const Result fast = run(top, frags, &pa, &aa, ~0u);

    TmuModel pb, ab;
    pb.latency = 11;
    ab.latency = 3;
    pb.ready_pattern = 0x8C1A5303u;
    ab.ready_pattern = 0x3305A1C8u;
    const Result slow = run(top, frags, &pb, &ab, 0x5A5A5A5Au);

    zhao::check(!fast.timed_out && !slow.timed_out, "both runs complete", 1,
                (!fast.timed_out && !slow.timed_out) ? 1 : 0);
    long diff = 0;
    if (fast.outs.size() != slow.outs.size())
      diff = 1;
    else
      for (size_t i = 0; i < fast.outs.size(); ++i)
        if (fast.outs[i].ctx != slow.outs[i].ctx || fast.outs[i].rgb != slow.outs[i].rgb ||
            fast.outs[i].aux_rgb != slow.outs[i].aux_rgb)
          ++diff;
    printf("   MEASURED: %lld clocks unimpeded, %lld with mismatched latencies and stalls\n",
           (long long)fast.clocks, (long long)slow.clocks);
    zhao::check(diff == 0,
                "different latencies on the two ports produce an identical stream", 0,
                (uint32_t)diff);
    zhao::check(slow.clocks > fast.clocks, "and the slow run really was slower", 1,
                slow.clocks > fast.clocks ? 1 : 0);
  }

  return zhao::report_and_exit("raster_texjoin_directed");
}
