// post_mem_model.hpp -- a MEM.GUARD + VRAM stand-in for the post lease's
// directed benches (post_fbread_directed.cpp, post_echo_directed.cpp).
//
// It plays the guard's REAL two-cycle law, which is the one that has been got
// wrong most often in this tree (tools/rtl/check_guard_verdict.py's header):
//
//   * `ready` is a LEVEL, high whenever no request is being forwarded;
//   * the verdict is a ONE-CYCLE PULSE, `ok` or `violation`, one cycle AFTER the
//     accepting edge -- never in the accepting cycle;
//   * a passed read returns len/8 packed 64-bit beats, in order, after a
//     latency; a passed write takes len/8 beats on the requester's data channel;
//   * retirement credits (16-bit words) come back per <=8-word burst, in order,
//     after the write data has landed.
//
// Every one of those can be made hostile (latency, busy time, write-channel
// throttling, credit delay) from a PCG so a bench can starve the DUT on purpose.
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>

#include "verilated.h"

namespace postmem {

// zhao_guard_req_t, MSB first: valid, write, client[2:0], addr[26:0], len[6:0],
// be[63:0] = 103 bits, which Verilator hands over as VlWide<4>.
struct Req {
  bool valid = false;
  bool write = false;
  unsigned client = 0;
  uint32_t addr = 0;
  unsigned len = 0;
  uint64_t be = 0;
};

inline unsigned bit(const VlWide<4>& w, unsigned b) { return (w[b / 32] >> (b % 32)) & 1u; }
inline uint64_t bits(const VlWide<4>& w, unsigned lo, unsigned n) {
  uint64_t v = 0;
  for (unsigned i = 0; i < n; ++i) v |= uint64_t(bit(w, lo + i)) << i;
  return v;
}
inline Req decode(const VlWide<4>& w) {
  Req r;
  r.be = bits(w, 0, 64);
  r.len = unsigned(bits(w, 64, 7));
  r.addr = uint32_t(bits(w, 71, 27));
  r.client = unsigned(bits(w, 98, 3));
  r.write = bit(w, 101) != 0;
  r.valid = bit(w, 102) != 0;
  return r;
}

struct Pcg {
  uint64_t s;
  explicit Pcg(uint64_t seed) : s(seed * 6364136223846793005ull + 1442695040888963407ull) {}
  uint32_t next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return uint32_t(s >> 33);
  }
  bool chance(unsigned num, unsigned den) { return (next() % den) < num; }
};

struct Model {
  // ---- knobs ---------------------------------------------------------------
  unsigned busy_min = 1, busy_rand = 0;       // cycles the guard stays not-ready
  unsigned read_lat = 6, read_rand = 0;       // cycles to the first read beat
  unsigned beat_gap_num = 0, beat_gap_den = 1;  // chance of a gap between beats
  unsigned wready_num = 1, wready_den = 1;    // chance the write channel is ready
  unsigned credit_lat = 4;                    // cycles from last write beat to credits
  std::function<bool(const Req&)> allow = [](const Req&) { return true; };
  Pcg pcg{1};

  // ---- memory --------------------------------------------------------------
  std::map<uint32_t, uint16_t> mem;           // 16-bit words by byte address
  uint16_t rd16(uint32_t a) const {
    auto it = mem.find(a);
    return it == mem.end() ? 0 : it->second;
  }

  // ---- evidence ------------------------------------------------------------
  unsigned accepts = 0, oks = 0, violations = 0, reads = 0, writes = 0;
  unsigned write_beats = 0, read_beats = 0;
  unsigned bad_write_order = 0;               // data beats with no passed write

  // ---- state ---------------------------------------------------------------
  unsigned busy = 0;                          // ready = (busy == 0)
  int verdict = 0;                            // 1 ok / -1 violation this cycle
  Req pending{};                              // accepted, verdict next cycle
  bool pend_v = false;
  struct Rd { uint32_t addr; unsigned beats; unsigned wait; };
  std::deque<Rd> rdq;
  struct Wr { uint32_t addr; unsigned len; uint64_t be; unsigned beat; };
  std::deque<Wr> wrq;
  struct Cr { unsigned words; unsigned wait; };
  std::deque<Cr> crq;

  // Drive this cycle's inputs. Returns {rsp bits, beat valid, beat data,
  // wready, credits}.
  struct Drive { unsigned rsp; bool beat_v; uint64_t beat_d; bool wready; unsigned credits; };
  Drive drive() {
    Drive d{0, false, 0, false, 0};
    if (verdict > 0) d.rsp |= 2u;
    if (verdict < 0) d.rsp |= 1u;
    if (busy == 0) d.rsp |= 4u;
    if (!rdq.empty() && rdq.front().wait == 0 &&
        !(beat_gap_num && pcg.chance(beat_gap_num, beat_gap_den))) {
      Rd& r = rdq.front();
      uint64_t v = 0;
      for (unsigned k = 0; k < 4; ++k) v |= uint64_t(rd16(r.addr + 2 * k)) << (16 * k);
      d.beat_v = true;
      d.beat_d = v;
    }
    d.wready = !wrq.empty() && pcg.chance(wready_num, wready_den);
    if (!crq.empty() && crq.front().wait == 0) d.credits = crq.front().words;
    return d;
  }

  // Advance on the clock edge, given what the DUT did this cycle.
  void edge(const Drive& d, const Req& req, bool wvalid, uint64_t wdata, bool wlast) {
    // The verdict pulse is exactly one cycle wide, and it is the cycle AFTER
    // the accepting edge -- the guard registers it on that edge (rsp_ok_q).
    verdict = 0;
    if (req.valid && (d.rsp & 4u)) {          // ACCEPTED on this edge
      ++accepts;
      pending = req;
      pend_v = true;
    }
    if (pend_v) {
      pend_v = false;
      const bool ok = allow(pending);
      verdict = ok ? 1 : -1;
      if (ok) {
        ++oks;
        if (pending.write) {
          ++writes;
          wrq.push_back(Wr{pending.addr, pending.len, pending.be, 0});
        } else {
          ++reads;
          rdq.push_back(Rd{pending.addr, pending.len / 8, read_lat + (read_rand ? pcg.next() % read_rand : 0)});
        }
      } else {
        ++violations;
      }
    }
    if (busy) --busy;
    if (req.valid && (d.rsp & 4u)) busy = busy_min + (busy_rand ? pcg.next() % busy_rand : 0);
    // read beats
    if (d.beat_v) {
      ++read_beats;
      Rd& r = rdq.front();
      r.addr += 8;
      if (--r.beats == 0) rdq.pop_front();
    }
    for (auto& r : rdq) if (r.wait) --r.wait;
    // write data
    if (wvalid && d.wready) {
      ++write_beats;
      if (wrq.empty()) {
        ++bad_write_order;
      } else {
        Wr& w = wrq.front();
        for (unsigned k = 0; k < 4; ++k) {
          const unsigned byte = w.beat * 8 + 2 * k;
          if (byte < w.len && ((w.be >> byte) & 3u) == 3u)
            mem[w.addr + byte] = uint16_t(wdata >> (16 * k));
        }
        ++w.beat;
        const bool done = (w.beat * 8 >= w.len);
        if (done != wlast) ++bad_write_order;
        if (done) {
          unsigned words = w.len / 2;
          while (words) {
            const unsigned b = words > 8 ? 8 : words;
            crq.push_back(Cr{b, credit_lat});
            words -= b;
          }
          wrq.pop_front();
        }
      }
    } else if (wvalid && wrq.empty()) {
      ++bad_write_order;
    }
    if (d.credits) crq.pop_front();
    for (auto& c : crq) if (c.wait) --c.wait;
  }
};

}  // namespace postmem
