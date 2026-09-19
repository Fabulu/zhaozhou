// mem_share_n_directed.cpp -- zhao_mem_share_n at N=3: three logical
// requesters, ONE permitted MEM.GUARD client.
//
// WHY N=3. `zhao_console_core`'s material seam 2: MATERIAL.RESOLVE's record
// fetch wants a THIRD ENGINE1 reader beside GEOM.MESHFETCH and GEOM.ASSETFETCH.
// The share's body now lives in `zhao_mem_share_n`, and `zhao_mem_share2` (and
// through it `zhao_geom_mem_adapter`) is its N=2 wrapper, so
// tests/geometry/geom_mem_adapter_directed.cpp is the unchanged N=2 proof. This
// file proves the guarantees at N=3, where the round-robin has to do more than
// alternate:
//
//   1. OWNERSHIP: a beat and a verdict reach the recorded owner and nobody
//      else. Data encodes the requesting ADDRESS, so a misrouted beat is a
//      wrong VALUE, not merely a wrong count.
//   2. THE GUARD'S TWO-CYCLE LAW, upstream: ready is a level, only for the
//      picked requester; the verdict is a pulse the cycle after; silence is
//      WAITED on, never read as an answer (a guard that answers late).
//   3. `last` FROM EACH REQUEST'S OWN LENGTH -- four and eight words, mixed.
//   4. NO STARVATION, AND THE BOUND IS N-1. Three requesters that never stop
//      asking are served in strict rotation; between two grants to any one
//      requester there are exactly two others. Two askers alternate.
//   5. A DENIAL is delivered to its owner only and does not wedge the share.
//   6. TRUSTED IDENTITY and READ-ONLY: a requester presenting SCANOUT and
//      `write` reaches the guard as ENGINE1 and a read.
//   7. THE THREE FAULT COUNTERS FIRE by stimulus: short, long (and the drain
//      after it), unowned. And `contention` moves when more than one asks.
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"

#include "Vtb_mem_share_n.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;
constexpr int kN = 3;

uint64_t word_of(uint32_t addr, int beat) {
  return (static_cast<uint64_t>(addr) << 16) ^ 0x5A5A000000000000ull ^ static_cast<uint64_t>(beat);
}
uint32_t bit(uint32_t v, int i) { return (v >> i) & 1u; }

// Element i of a packed [2:0][W-1:0] port wider than 64 bits: W-bit fields laid
// end to end across the 32-bit words, element 0 lowest -- NOT one per word.
void set_field(VlWide<3>& w, int lsb, int width, uint32_t v) {
  for (int k = 0; k < width; ++k) {
    const int b = lsb + k;
    const uint32_t m = 1u << (b % 32);
    if ((v >> k) & 1u)
      w[b / 32] |= m;
    else
      w[b / 32] &= ~m;
  }
}

struct Req {
  uint32_t addr;
  int len;
  bool write = false;
  uint32_t client = 3;
};

struct Requester {
  std::deque<Req> todo;
  bool offering = false;
  bool awaiting = false;  // accepted by the share; verdict not yet seen
  bool filling = false;
  Req cur{};
  int beats = 0;
  int lasts = 0;
  int oks = 0;
  int viols = 0;
  bool data_ok = true;
  int beats_this = 0;
  bool last_on_count = true;
  int stray = 0;  // beats/verdicts arriving when this requester had nothing out
};

struct Bench {
  Vtb_mem_share_n& d;
  Requester r[kN];
  // guard model
  bool ok_pending = false;
  int verdict_delay = 0;  // extra SILENT cycles before the verdict
  int delay_left = 0;
  bool deny_next_for_addr = false;
  uint32_t deny_addr = 0;
  bool serving = false;
  uint32_t serve_addr = 0;
  int serve_beat = 0;
  int serve_words = 0;
  int serve_extra = 0;  // >0: over-serve by this many words
  int serve_short = 0;  // >0: end this many words early
  bool pending_short = false;
  bool pending_long = false;
  bool pending_verdict_deny = false;
  // observations
  std::vector<int> grant_order;
  int m_offers = 0;
  bool m_identity_ok = true;
  bool m_read_only = true;
  bool m_len_ok = true;

  explicit Bench(Vtb_mem_share_n& dut) : d(dut) {}

  void drive_requesters() {
    uint32_t v = 0, w = 0, len = 0;
    uint16_t cl = 0;
    for (int i = 0; i < kN; ++i) {
      Requester& q = r[i];
      if (!q.offering && !q.awaiting && !q.filling && !q.todo.empty()) {
        q.cur = q.todo.front();
        q.todo.pop_front();
        q.offering = true;
      }
      if (q.offering) {
        v |= 1u << i;
        if (q.cur.write) w |= 1u << i;
      }
      cl = static_cast<uint16_t>(cl | ((q.cur.client & 7u) << (3 * i)));
      len |= (static_cast<uint32_t>(q.cur.len) & 0x7Fu) << (7 * i);
      set_field(d.r_addr, 27 * i, 27, q.cur.addr & 0x07FFFFFFu);
    }
    d.r_valid = static_cast<uint8_t>(v);
    d.r_write = static_cast<uint8_t>(w);
    d.r_client = cl;
    d.r_len = len;
  }

  void drive_guard() {
    d.m_ready = 0;
    d.m_ok = 0;
    d.m_violation = 0;
    d.m_beat_valid = 0;
    d.m_beat_last = 0;
    d.m_beat_data = 0;
    if (ok_pending) {
      if (delay_left > 0) {
        --delay_left;  // SILENCE: neither ok nor violation
        return;
      }
      ok_pending = false;
      if (pending_verdict_deny) {
        d.m_violation = 1;
        pending_verdict_deny = false;
      } else {
        d.m_ok = 1;
        serving = true;
        serve_beat = 0;
      }
      return;
    }
    if (serving) {
      const int total = serve_words + serve_extra - serve_short;
      d.m_beat_valid = 1;
      d.m_beat_data = word_of(serve_addr, serve_beat);
      d.m_beat_last = (serve_beat == total - 1) ? 1 : 0;
      return;
    }
    if (d.m_valid) {
      ++m_offers;
      if (d.m_client != 3) m_identity_ok = false;
      if (d.m_write) m_read_only = false;
      d.m_ready = 1;
      ok_pending = true;
      delay_left = verdict_delay;
      serve_addr = d.m_addr;
      serve_words = static_cast<int>(d.m_len) >> 3;
      serve_extra = pending_long ? 3 : 0;
      serve_short = pending_short ? 1 : 0;
      pending_long = false;
      pending_short = false;
      if (deny_next_for_addr && d.m_addr == deny_addr) {
        pending_verdict_deny = true;
        deny_next_for_addr = false;
      }
    }
  }

  void observe() {
    for (int i = 0; i < kN; ++i) {
      Requester& q = r[i];
      if (q.offering && bit(d.r_ready, i)) {
        q.offering = false;
        q.awaiting = true;
      }
      if (bit(d.r_ok, i)) {
        if (!q.awaiting) ++q.stray;
        ++q.oks;
        grant_order.push_back(i);
        q.awaiting = false;
        q.filling = true;
        q.beats_this = 0;
      }
      if (bit(d.r_violation, i)) {
        if (!q.awaiting) ++q.stray;
        ++q.viols;
        q.awaiting = false;
      }
      if (bit(d.r_beat_valid, i)) {
        if (!q.filling) ++q.stray;
        ++q.beats;
        if (d.r_beat_data != word_of(q.cur.addr & 0x07FFFFFFu, q.beats_this)) q.data_ok = false;
        ++q.beats_this;
        if (bit(d.r_beat_last, i)) {
          ++q.lasts;
          if (q.beats_this != q.cur.len / 8) q.last_on_count = false;
          q.filling = false;
        }
      }
    }
    if (serving && d.m_beat_valid) {
      if (d.m_beat_last)
        serving = false;
      else
        ++serve_beat;
    }
  }

  void step() {
    drive_requesters();
    drive_guard();
    d.eval();
    observe();
    zhao::tick(d);
  }
  void run(int n) {
    for (int i = 0; i < n; ++i) step();
  }
  bool idle() const {
    for (const auto& q : r) {
      if (q.offering || q.awaiting || q.filling || !q.todo.empty()) return false;
    }
    return !serving && !ok_pending;
  }
  void run_until_idle(int limit = 4000) {
    for (int i = 0; i < limit && !idle(); ++i) step();
    run(4);
  }
};

void reset(Vtb_mem_share_n& d) {
  d.rst_n = 0;
  d.r_valid = 0;
  d.m_ready = 0;
  d.m_ok = 0;
  d.m_violation = 0;
  d.m_beat_valid = 0;
  d.m_beat_last = 0;
  d.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

char nm[160];
const char* name(const char* f, int a = 0, int b = 0) {
  std::snprintf(nm, sizeof nm, f, a, b);
  return nm;
}

uint32_t base_of(int i) { return 0x0100000u * static_cast<uint32_t>(i + 1); }

}  // namespace

int main() {
  Vtb_mem_share_n d;

  // ---- 1. each requester alone, at BOTH burst scales ----------------------
  for (int i = 0; i < kN; ++i) {
    for (int len : {32, 64}) {
      reset(d);
      Bench b(d);
      b.r[i].todo.push_back({base_of(i), len});
      b.run_until_idle();
      check(b.r[i].oks == 1, name("1.req %d len %d: one verdict, to it", i, len), 1,
            static_cast<uint32_t>(b.r[i].oks));
      check(b.r[i].beats == len / 8 && b.r[i].lasts == 1 && b.r[i].last_on_count,
            name("1.req %d len %d: its own count of words, last on the count", i, len), 1,
            static_cast<uint32_t>(b.r[i].beats));
      check(b.r[i].data_ok, name("1.req %d len %d: its own data", i, len), 1, b.r[i].data_ok);
      int others = 0;
      for (int j = 0; j < kN; ++j) {
        if (j != i) others += b.r[j].beats + b.r[j].oks + b.r[j].viols + b.r[j].stray;
      }
      check(others == 0, name("1.req %d len %d: nobody else saw anything", i, len), 0,
            static_cast<uint32_t>(others));
      check(d.jobs[i] == 1, name("1.req %d len %d: its jobs counter", i, len), 1, d.jobs[i]);
    }
  }

  // ---- 2. NO STARVATION at N=3: strict rotation, bound N-1 ----------------
  // All three ask continuously (four requests each, mixed scales, queued so a
  // requester re-asks the moment it is free). Reset last owner is 0, so the
  // rotation starts at 1: the order must be 1,2,0,1,2,0,...
  {
    reset(d);
    Bench b(d);
    for (int i = 0; i < kN; ++i) {
      for (int k = 0; k < 4; ++k) {
        b.r[i].todo.push_back({base_of(i) + static_cast<uint32_t>(k) * 0x40u, (k & 1) ? 64 : 32});
      }
    }
    b.run_until_idle();
    bool rotation = b.grant_order.size() == 12;
    for (size_t k = 0; k < b.grant_order.size() && rotation; ++k) {
      if (b.grant_order[k] != static_cast<int>((k + 1) % kN)) rotation = false;
    }
    check(rotation, "2.three continuous askers are served in strict rotation 1,2,0,...", 12,
          static_cast<uint32_t>(b.grant_order.size()));
    // The bound, stated as the property rather than as the sequence: between
    // two consecutive grants to one requester there are at most N-1 others.
    int worst_gap = 0;
    for (int i = 0; i < kN; ++i) {
      int prev = -1;
      for (size_t k = 0; k < b.grant_order.size(); ++k) {
        if (b.grant_order[k] == i) {
          if (prev >= 0 && static_cast<int>(k) - prev - 1 > worst_gap)
            worst_gap = static_cast<int>(k) - prev - 1;
          prev = static_cast<int>(k);
        }
      }
    }
    check(worst_gap == kN - 1, "2.no requester waits behind more than N-1 others", kN - 1,
          static_cast<uint32_t>(worst_gap));
    bool all = true;
    for (int i = 0; i < kN; ++i) {
      all = all && b.r[i].beats == 2 * 4 + 2 * 8 && b.r[i].data_ok && b.r[i].last_on_count &&
            b.r[i].stray == 0 && d.jobs[i] == 4;
    }
    check(all, "2.every requester got its 24 words, its own data, last on its own counts", 1, all);
    check(d.contention > 0, "2.contention is counted when more than one asks", 1, d.contention);
    check(d.err_short == 0 && d.err_long == 0 && d.err_unowned == 0, "2.no fault counted", 0,
          d.err_short + d.err_long + d.err_unowned);
  }

  // ---- 2b. two askers (0 and 2) alternate; the idle one is skipped --------
  {
    reset(d);
    Bench b(d);
    for (int k = 0; k < 3; ++k) {
      b.r[0].todo.push_back({base_of(0) + static_cast<uint32_t>(k) * 0x40u, 64});
      b.r[2].todo.push_back({base_of(2) + static_cast<uint32_t>(k) * 0x40u, 32});
    }
    b.run_until_idle();
    const std::vector<int> want = {2, 0, 2, 0, 2, 0};
    check(b.grant_order == want, "2b.two askers alternate 2,0,2,0,... past the idle one", 6,
          static_cast<uint32_t>(b.grant_order.size()));
  }

  // ---- 3. the guard answers LATE: silence is waited on --------------------
  for (int delay : {1, 4}) {
    reset(d);
    Bench b(d);
    b.verdict_delay = delay;
    for (int i = 0; i < kN; ++i) b.r[i].todo.push_back({base_of(i), 64});
    b.run_until_idle();
    bool ok = true;
    for (int i = 0; i < kN; ++i)
      ok = ok && b.r[i].oks == 1 && b.r[i].viols == 0 && b.r[i].beats == 8 && b.r[i].data_ok;
    check(ok, name("3.verdict %d cycles late: every request still served, none denied", delay), 1,
          ok);
    check(d.denied == 0, name("3.verdict %d cycles late: no false denial counted", delay), 0,
          d.denied);
  }

  // ---- 4. a DENIAL goes to its owner only, and the share lives ------------
  {
    reset(d);
    Bench b(d);
    b.deny_next_for_addr = true;
    b.deny_addr = base_of(1);
    for (int i = 0; i < kN; ++i) b.r[i].todo.push_back({base_of(i), 64});
    b.r[1].todo.push_back({base_of(1) + 0x40u, 64});  // it asks again after the denial
    b.run_until_idle();
    check(b.r[1].viols == 1, "4.the denied requester is told", 1,
          static_cast<uint32_t>(b.r[1].viols));
    check(b.r[0].viols + b.r[2].viols == 0, "4.and nobody else is", 0,
          static_cast<uint32_t>(b.r[0].viols + b.r[2].viols));
    check(b.r[0].beats == 8 && b.r[2].beats == 8 && b.r[1].beats == 8,
          "4.the others are served, and the denied one's NEXT request too", 1,
          b.r[0].beats == 8 && b.r[2].beats == 8 && b.r[1].beats == 8);
    check(d.denied == 1, "4.denied counts exactly one", 1, d.denied);
  }

  // ---- 5. trusted identity and read-only, from every index ----------------
  {
    reset(d);
    Bench b(d);
    for (int i = 0; i < kN; ++i) {
      Req q{base_of(i), 64, true, 0 /* SCANOUT */};
      b.r[i].todo.push_back(q);
    }
    b.run_until_idle();
    check(b.m_offers == 3, "5.three requests reached the guard", 3,
          static_cast<uint32_t>(b.m_offers));
    check(b.m_identity_ok, "5.every one as ENGINE1, whatever the requester claimed", 1,
          b.m_identity_ok);
    check(b.m_read_only, "5.every one as a READ, whatever the requester asked", 1, b.m_read_only);
  }

  // ---- 6. the fault counters FIRE, and the share recovers after each ------
  {
    reset(d);
    Bench b(d);
    b.pending_long = true;  // the first line over-serves by three words
    b.r[2].todo.push_back({base_of(2), 32});
    b.r[0].todo.push_back({base_of(0), 64});
    b.run_until_idle();
    check(d.err_long == 1, "6.an overlong return counts LONG", 1, d.err_long);
    check(d.err_unowned == 0, "6.and its surplus is DRAINED, not called unowned", 0, d.err_unowned);
    check(b.r[2].beats == 4 && b.r[2].stray == 0, "6.the owner saw exactly its four words", 4,
          static_cast<uint32_t>(b.r[2].beats));
    check(b.r[0].beats == 8 && b.r[0].data_ok, "6.and the next requester is served cleanly", 8,
          static_cast<uint32_t>(b.r[0].beats));

    b.pending_short = true;  // the next line ends one word early
    b.r[1].todo.push_back({base_of(1), 64});
    b.run_until_idle();
    check(d.err_short == 1, "6.a short return counts SHORT", 1, d.err_short);

    // A word with no logical request out at all.
    d.m_beat_valid = 1;
    d.m_beat_last = 1;
    d.m_beat_data = 0xDEAD;
    d.eval();
    zhao::tick(d);
    d.m_beat_valid = 0;
    d.m_beat_last = 0;
    d.eval();
    check(d.err_unowned == 1, "6.a word nobody asked for counts UNOWNED", 1, d.err_unowned);
    check(d.r_beat_valid == 0, "6.and reaches nobody", 0, d.r_beat_valid);
  }

  d.final();
  return zhao::report_and_exit("mem_share_n_directed");
}
