// mem_share_wr_directed.cpp -- zhao_mem_share_wr at N=3: two WRITERS and one
// READER on ONE MEM.GUARD client, with its write channel and credit stream.
//
// This is the TERRAIN.BUILD socket's upstream share (core entry I26): MEM.UPLOAD
// and TERRAIN.PAGELOADER write TERRAIN.PAGE_POOL / the R32 region, and the
// terrain read share reads it back, all as ZHAO_CLIENT_TERRAIN_BUILD. What it
// proves, each against a model that could tell the difference:
//
//   1. WRITE-DATA ORDER. Every write's data words reach the one channel
//      CONTIGUOUSLY and in VERDICT order, and every word is the word its own
//      writer sent for that address -- two writers are never interleaved, even
//      with the channel's ready toggling and both writers always asking.
//   2. RETIREMENT ATTRIBUTION. Each requester's retire stream totals exactly the
//      words of ITS OWN passed requests, reads included, with credits arriving
//      in 8-word bursts after the data. `retire_unowned` stays 0 in the legal
//      run.
//   3. A REFUSED WRITE RELEASES THE CHANNEL and is owed no credit.
//   4. TRUSTED IDENTITY: every request reaches the guard as TERRAIN_BUILD (6),
//      whatever the requester claimed, and a write stays a write.
//   5. THE LEDGER BACKPRESSURES rather than overflowing: with credits withheld,
//      offers stop at RQ-1 outstanding and `ledger_full` counts; releasing the
//      credits completes everything.
//   6. THE TWO NEW TRIPWIRES FIRE by stimulus: `retire_unowned` on a credit with
//      no ledger entry and on an over-credit; `wbeat_unowned` on write data from
//      a requester that does not own the channel.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <vector>

#include "verilated.h"

#include "Vtb_mem_share_wr.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;
constexpr int kN = 3;
constexpr int kRQ = 4;

uint64_t wword(uint32_t addr, int beat) {
  return (static_cast<uint64_t>(addr) << 20) ^ 0xC0DE000000000000ull ^ static_cast<uint64_t>(beat);
}
uint64_t rword(uint32_t addr, int beat) {
  return (static_cast<uint64_t>(addr) << 16) ^ 0x5A5A000000000000ull ^ static_cast<uint64_t>(beat);
}
uint32_t bit(uint32_t v, int i) { return (v >> i) & 1u; }
uint32_t base_of(int i) { return 0x0400000u + static_cast<uint32_t>(i) * 0x10000u; }
int words_of(int len) { return (len + 1) / 2; }

void set_field(VlWide<3>& w, int lsb, int width, uint32_t v) {
  for (int k = 0; k < width; ++k) {
    const int b = lsb + k;
    const uint32_t m = 1u << (b % 32);
    if ((v >> k) & 1u) w[b / 32] |= m;
    else w[b / 32] &= ~m;
  }
}

struct Req {
  uint32_t addr;
  int len;
  bool write;
};

struct Requester {
  std::deque<Req> todo;
  bool offering = false, awaiting = false, filling = false, streaming = false;
  Req cur{};
  int wbeat = 0;
  int beats_this = 0;
  int oks = 0, viols = 0, reads_done = 0;
  bool rdata_ok = true;
  long owed_words = 0;     // words of this requester's PASSED requests
  long retired_words = 0;  // what its retire stream delivered
  bool stray_wvalid = false;  // drive wvalid without owning (tripwire test)
  bool drove_wv = false;      // wvalid was driven THIS cycle
};

struct Pass {
  int owner;
  uint32_t addr;
  int len;
  bool write;
};

struct Bench {
  Vtb_mem_share_wr& d;
  Requester r[kN];
  // guard
  bool verdict_next = false;
  bool deny_pending = false;
  uint32_t deny_addr = 0xFFFFFFFFu;
  Req in_guard{};
  // read service
  bool serving = false;
  uint32_t serve_addr = 0;
  int serve_beat = 0, serve_total = 0;
  // write channel model
  std::deque<Pass> wr_expect;    // passed writes, verdict order
  int wr_word = 0;
  bool wr_order_ok = true;
  int wr_words_total = 0;
  unsigned lfsr = 0xACE1u;
  // credit model: one entry per passed request, retired in order after its
  // data (writes) or at once (reads)
  struct Owed { int words; bool ready; };
  std::deque<Owed> credit_q;
  bool credit_enable = true;
  int credit_gap = 0;
  // observations
  int m_offers = 0;
  bool identity_ok = true;
  bool write_kept = true;
  int max_outstanding = 0;

  explicit Bench(Vtb_mem_share_wr& dut) : d(dut) {}

  bool rnd() {
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
    return (lfsr & 3u) != 0;
  }

  void drive() {
    uint32_t v = 0, w = 0, len = 0, wv = 0, wl = 0;
    for (int i = 0; i < kN; ++i) {
      Requester& q = r[i];
      if (!q.offering && !q.awaiting && !q.filling && !q.streaming && !q.todo.empty()) {
        q.cur = q.todo.front();
        q.todo.pop_front();
        q.offering = true;
      }
      if (q.offering) {
        v |= 1u << i;
        if (q.cur.write) w |= 1u << i;
      }
      len |= (static_cast<uint32_t>(q.cur.len) & 0x7Fu) << (7 * i);
      set_field(d.r_addr, 27 * i, 27, q.cur.addr & 0x07FFFFFFu);
      uint64_t wd = wword(q.cur.addr, q.wbeat);
      d.r_wdata[2 * i] = static_cast<uint32_t>(wd);
      d.r_wdata[2 * i + 1] = static_cast<uint32_t>(wd >> 32);
      q.drove_wv = q.streaming;
      if (q.streaming || q.stray_wvalid) wv |= 1u << i;
      if (q.streaming && q.wbeat == q.cur.len / 8 - 1) wl |= 1u << i;
    }
    d.r_valid = static_cast<uint8_t>(v);
    d.r_write = static_cast<uint8_t>(w);
    d.r_len = len;
    d.r_wvalid = static_cast<uint8_t>(wv);
    d.r_wlast = static_cast<uint8_t>(wl);

    // guard
    d.m_ready = 0;
    d.m_ok = 0;
    d.m_violation = 0;
    d.m_beat_valid = 0;
    d.m_beat_last = 0;
    d.m_beat_data = 0;
    if (verdict_next) {
      verdict_next = false;
      if (deny_pending) {
        d.m_violation = 1;
        deny_pending = false;
      } else {
        d.m_ok = 1;
      }
    } else if (serving) {
      d.m_beat_valid = 1;
      d.m_beat_data = rword(serve_addr, serve_beat);
      d.m_beat_last = (serve_beat == serve_total - 1) ? 1 : 0;
    } else if (d.m_valid) {
      ++m_offers;
      if (d.m_client != 6) identity_ok = false;
      d.m_ready = 1;
      verdict_next = true;
      in_guard.addr = d.m_addr;
      in_guard.len = static_cast<int>(d.m_len);
      in_guard.write = d.m_write != 0;
      if (in_guard.addr == deny_addr) {
        deny_pending = true;
        deny_addr = 0xFFFFFFFFu;
      }
    }
    d.m_wready = rnd() ? 1 : 0;

    // credits
    d.m_credits = 0;
    if (credit_gap > 0) {
      --credit_gap;
    } else if (credit_enable && !credit_q.empty() && credit_q.front().ready) {
      const int chunk = credit_q.front().words < 8 ? credit_q.front().words : 8;
      d.m_credits = static_cast<uint8_t>(chunk);
      credit_q.front().words -= chunk;
      if (credit_q.front().words == 0) credit_q.pop_front();
      credit_gap = 3;
    }
  }

  int owner_of(uint32_t addr) const {
    for (int i = 0; i < kN; ++i)
      if ((addr & 0x07FF0000u) == (base_of(i) & 0x07FF0000u)) return i;
    return -1;
  }

  void observe() {
    // the guard's verdict, as the downstream saw it
    if (d.m_ok) {
      const int own = owner_of(in_guard.addr);
      Pass p{own, in_guard.addr, in_guard.len, in_guard.write};
      credit_q.push_back({words_of(in_guard.len), !in_guard.write});
      if (in_guard.write) {
        wr_expect.push_back(p);
      } else {
        serving = true;
        serve_addr = in_guard.addr;
        serve_beat = 0;
        serve_total = in_guard.len / 8;
      }
    }
    if (d.m_ok) {
      const int own = owner_of(in_guard.addr);
      if (own < 0 || r[own].cur.write != in_guard.write) write_kept = false;
    }

    for (int i = 0; i < kN; ++i) {
      Requester& q = r[i];
      if (q.offering && bit(d.r_ready, i)) {
        q.offering = false;
        q.awaiting = true;
      }
      if (bit(d.r_ok, i)) {
        ++q.oks;
        q.awaiting = false;
        q.owed_words += words_of(q.cur.len);
        if (q.cur.write) {
          q.streaming = true;
          q.wbeat = 0;
        } else {
          q.filling = true;
          q.beats_this = 0;
        }
      }
      if (bit(d.r_violation, i)) {
        ++q.viols;
        q.awaiting = false;
      }
      if (bit(d.r_beat_valid, i)) {
        if (d.r_beat_data != rword(q.cur.addr & 0x07FFFFFFu, q.beats_this)) q.rdata_ok = false;
        ++q.beats_this;
        if (bit(d.r_beat_last, i)) {
          q.filling = false;
          ++q.reads_done;
        }
      }
      if (q.drove_wv && bit(d.r_wready, i)) {
        ++q.wbeat;
        if (q.wbeat == q.cur.len / 8) {
          q.streaming = false;
          q.wbeat = 0;
        }
      }
    }
    // retire streams (packed [2:0][7:0] -> one 32-bit word)
    const uint32_t rt = d.r_retire;
    for (int i = 0; i < kN; ++i) r[i].retired_words += (rt >> (8 * i)) & 0xFFu;

    // the write channel
    if (d.m_wvalid && d.m_wready) {
      ++wr_words_total;
      if (wr_expect.empty()) {
        wr_order_ok = false;
      } else {
        const Pass& p = wr_expect.front();
        if (d.m_wdata != wword(p.addr, wr_word)) wr_order_ok = false;
        const bool last = (wr_word == p.len / 8 - 1);
        if ((d.m_wlast != 0) != last) wr_order_ok = false;
        ++wr_word;
        if (last) {
          wr_word = 0;
          // its data is complete: its credits may now come back
          for (auto& o : credit_q) {
            if (!o.ready) { o.ready = true; break; }
          }
          wr_expect.pop_front();
        }
      }
    }
    if (serving && d.m_beat_valid) {
      if (d.m_beat_last) serving = false;
      else ++serve_beat;
    }
    int out = static_cast<int>(credit_q.size());
    if (out > max_outstanding) max_outstanding = out;
  }

  int cyc = 0;
  void step() {
    drive();
    d.eval();
    if (getenv("SHWR_TRACE") && cyc < 6000) std::printf("%4d mv=%d mw=%d ma=%x rdy=%d ok=%d rv=%x rr=%x rok=%x wv=%x wr=%x mwv=%d mwr=%d wl=%d cr=%d ret=%06x q=%zu we=%zu\n", cyc, d.m_valid, d.m_write, d.m_addr, d.m_ready, d.m_ok, d.r_valid, d.r_ready, d.r_ok, d.r_wvalid, d.r_wready, d.m_wvalid, d.m_wready, d.m_wlast, d.m_credits, d.r_retire, credit_q.size(), wr_expect.size());
    ++cyc;
    observe();
    zhao::tick(d);
  }
  void run(int n) { for (int i = 0; i < n; ++i) step(); }
  bool idle() const {
    for (const auto& q : r)
      if (q.offering || q.awaiting || q.filling || q.streaming || !q.todo.empty()) return false;
    return !serving && !verdict_next && credit_q.empty();
  }
  void run_until_idle(int limit = 20000) {
    for (int i = 0; i < limit && !idle(); ++i) step();
    run(8);
  }
};

void reset(Vtb_mem_share_wr& d) {
  d.rst_n = 0;
  d.r_valid = 0;
  d.r_wvalid = 0;
  d.m_ready = 0;
  d.m_ok = 0;
  d.m_violation = 0;
  d.m_beat_valid = 0;
  d.m_beat_last = 0;
  d.m_credits = 0;
  d.m_wready = 0;
  d.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_mem_share_wr d;

  // ---- 1-2. two writers always asking, one reader, ready toggling --------
  {
    reset(d);
    Bench b(d);
    for (int k = 0; k < 12; ++k) {
      b.r[0].todo.push_back({base_of(0) + 64u * k, 64, true});
      b.r[1].todo.push_back({base_of(1) + 64u * k, 64, true});
      b.r[2].todo.push_back({base_of(2) + 64u * k, (k & 1) ? 32 : 64, false});
    }
    b.run_until_idle();
    check(b.r[0].oks == 12 && b.r[1].oks == 12 && b.r[2].oks == 12,
          "1.every request of every requester passed", 36,
          static_cast<uint64_t>(b.r[0].oks + b.r[1].oks + b.r[2].oks));
    check(b.wr_order_ok, "1.write words contiguous, in verdict order, each its own writer's", 1, b.wr_order_ok);
    check(b.wr_words_total == 24 * 8, "1.every write word crossed exactly once", 192,
          static_cast<uint64_t>(b.wr_words_total));
    check(b.r[2].reads_done == 12 && b.r[2].rdata_ok, "1.the reader got its own words", 12,
          static_cast<uint64_t>(b.r[2].reads_done));
    for (int i = 0; i < kN; ++i) {
      char nm[96];
      std::snprintf(nm, sizeof nm, "2.requester %d retired exactly its own words", i);
      check(b.r[i].retired_words == b.r[i].owed_words, nm,
            static_cast<uint64_t>(b.r[i].owed_words), static_cast<uint64_t>(b.r[i].retired_words));
    }
    check(b.r[0].owed_words == 12 * 32, "2.a 64-byte write owes 32 words (the arbiter's rounding)", 384,
          static_cast<uint64_t>(b.r[0].owed_words));
    check(d.retire_unowned == 0, "2.no credit went unowned in the legal run", 0, d.retire_unowned);
    check(d.wbeat_unowned == 0, "2.no write word was offered without the channel", 0, d.wbeat_unowned);
    check(b.identity_ok, "4.every request reached the guard as TERRAIN_BUILD", 1, b.identity_ok);
    check(b.write_kept, "4.and a write stayed a write", 1, b.write_kept);
    check(d.contention != 0, "1.contention was real and counted", 1, d.contention != 0);
  }

  // ---- 3. a refused write releases the channel --------------------------
  {
    reset(d);
    Bench b(d);
    b.deny_addr = base_of(0);
    b.r[0].todo.push_back({base_of(0), 64, true});        // refused
    b.r[0].todo.push_back({base_of(0) + 64u, 64, true});  // then passes
    b.r[1].todo.push_back({base_of(1), 64, true});
    b.run_until_idle();
    check(b.r[0].viols == 1 && b.r[0].oks == 1, "3.the refused writer is told, then served", 1,
          b.r[0].viols == 1 && b.r[0].oks == 1);
    check(b.r[1].oks == 1, "3.the other writer is not locked out", 1, static_cast<uint64_t>(b.r[1].oks));
    check(b.wr_order_ok && b.wr_words_total == 16, "3.only the two passed writes sent data", 16,
          static_cast<uint64_t>(b.wr_words_total));
    check(b.r[0].retired_words == 32, "3.the refused write is owed no credit", 32,
          static_cast<uint64_t>(b.r[0].retired_words));
    check(d.denied == 1, "3.denied counts exactly one", 1, d.denied);
  }

  // ---- 5. ledger backpressure --------------------------------------------
  {
    reset(d);
    Bench b(d);
    b.credit_enable = false;
    for (int k = 0; k < 6; ++k) b.r[2].todo.push_back({base_of(2) + 64u * k, 64, false});
    b.run(600);
    check(b.r[2].oks == kRQ - 1, "5.with credits withheld, offers stop at RQ-1 outstanding",
          kRQ - 1, static_cast<uint64_t>(b.r[2].oks));
    check(d.ledger_full != 0, "5.and the full ledger is counted", 1, d.ledger_full != 0);
    b.credit_enable = true;
    b.run_until_idle();
    check(b.r[2].oks == 6 && b.r[2].retired_words == 6 * 32, "5.releasing credits completes everything", 1,
          b.r[2].oks == 6 && b.r[2].retired_words == 6 * 32);
    check(d.retire_unowned == 0, "5.without one unowned credit", 0, d.retire_unowned);
  }

  // ---- 6. the tripwires FIRE ------------------------------------------------
  {
    reset(d);
    Bench b(d);
    // a credit with an empty ledger
    d.m_credits = 8;
    d.eval();
    zhao::tick(d);
    d.m_credits = 0;
    d.eval();
    check(d.retire_unowned == 1, "6.a credit nobody is owed fires retire_unowned", 1, d.retire_unowned);
    // an over-credit: one read owes 32 words; return 40 at once
    b.credit_enable = false;
    b.r[2].todo.push_back({base_of(2), 64, false});
    b.run(200);
    const uint32_t before = d.retire_unowned;
    b.credit_q.clear();
    d.m_credits = 40;
    d.eval();
    zhao::tick(d);
    d.m_credits = 0;
    d.eval();
    check(d.retire_unowned == before + 1, "6.more credit than the head is owed fires retire_unowned",
          before + 1, d.retire_unowned);
    // write data from a requester that does not own the channel
    b.r[1].stray_wvalid = true;
    b.run(5);
    b.r[1].stray_wvalid = false;
    b.run(2);
    check(d.wbeat_unowned >= 5, "6.write data without the channel fires wbeat_unowned", 5,
          d.wbeat_unowned);
    check(d.m_wvalid == 0, "6.and never reaches the channel", 0, d.m_wvalid);
  }

  d.final();
  return zhao::report_and_exit("mem_share_wr_directed");
}
