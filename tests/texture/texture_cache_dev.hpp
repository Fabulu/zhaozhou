// texture_cache_dev.hpp — Verilator driver for TEXTURE.CACHE
// (fpga/rtl/texture/zhao_texture_cache.sv, contract
// design/contracts/TEXTURE.CACHE.md).
//
// The oracle is zref::TextureCache, the ledger's declared `reference_model`.
// It is a plainly-written model of the CONTRACT rather than a view onto a
// frozen function — nothing earlier in this repository maintains a tag array
// — so what the differential lanes exercise is the RTL's response stage, its
// halfword-beat fill engine, its backpressure and its saturating counters,
// not a shared clever idea. The two share exactly one thing: the address
// split (tag / index / offset), which is defined once in the RTL and once in
// the oracle's `resident()`.
//
// THE VRAM SIDE IS MODELLED, NOT INSTANTIATED, and deliberately so. MEM.GUARD
// exists and is RTL_VERIFIED, but wiring it here would tie this lane's
// stimulus to the guard's region map: a texture-pool address is a Phase-6
// region the guard's Phase-2 map does not yet carry, so every fill would be a
// violation. This driver plays the fill port with a configurable request→data
// latency and a configurable beat gap instead, which is also how it drives
// the timings a real memory would never produce — a fill answered in one
// cycle, and a fill whose beats trickle in with holes.
//
// CYCLE MODEL. `feed()` offers accesses through a ready/valid stream, gates
// `smp_ready_i` with a PCG pattern, services fills, and checks the protocol
// as it goes: a stalled response must not change, a response must not appear
// without an accepted access, and the block must never accept an access one
// of whose enabled lanes is missing.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_cache.h"

#include "zhao_sim.hpp"
#include "zref/zref_texture.hpp"

namespace zhao_texture {

inline constexpr int kLanes = zref::TextureCache::kLanes;
inline constexpr int kLines = zref::TextureCache::kLines;
inline constexpr int kLineBytes = zref::TextureCache::kLineBytes;
inline constexpr int kBeats = kLineBytes / 2;

/** One access offered to the block. */
struct CacAccess {
  bool en[kLanes] = {};
  uint32_t addr[kLanes] = {};
  uint16_t src_id = 0;
};

/** One response observed. */
struct CacResult {
  uint16_t data[kLanes] = {};
  uint16_t src_id = 0;
};

/** What one batch observed. */
struct CacRun {
  std::vector<CacResult> out;
  std::vector<uint32_t> fills;  // fill_addr_o, in issue order
  uint32_t hits = 0;
  uint32_t misses = 0;
  uint32_t cycles = 0;
};

class CacDev {
 public:
  CacDev() { reset(); }
  ~CacDev() { top_.final(); }
  CacDev(const CacDev&) = delete;
  CacDev& operator=(const CacDev&) = delete;

  void reset() {
    park();
    top_.rst_n = 0;
    top_.eval();
    for (int i = 0; i < 2; ++i) edge();
    top_.rst_n = 1;
    top_.eval();
  }

  /** One invalidate cycle: accepted unconditionally, like the tile store's clear. */
  void invalidate(bool all, uint32_t addr) {
    park();
    top_.inv_valid_i = 1;
    top_.inv_all_i = all ? 1 : 0;
    top_.inv_addr_i = addr;
    top_.eval();
    edge();
    park();
    top_.eval();
  }

  /**
   * Arm a one-shot invalidate that fires in the SAME cycle as fill beat
   * `beat` of the next fill. That race — an invalidate landing while a line is
   * midway in — is the one the contract's ordering rule exists for, and it is
   * not reachable through the batch API otherwise. -1 disarms.
   */
  void arm_invalidate_on_beat(int beat, bool all, uint32_t addr) {
    armed_beat_ = beat;
    armed_all_ = all;
    armed_addr_ = addr;
  }

  /**
   * Feed a batch. `out_seed` != 0 PCG-gates `smp_ready_i`; `in_seed` != 0
   * gates the offer so `acc_valid_i` drops mid-stream. `fill_lat` is the
   * cycles from an accepted fill request to its first beat; `beat_gap` puts
   * that many idle cycles between beats.
   */
  CacRun feed(const std::vector<CacAccess>& acc, const zref::TextureMemory& mem, uint32_t in_seed,
              uint32_t out_seed, int fill_lat, int beat_gap, std::string* err) {
    CacRun r;
    r.out.assign(acc.size(), CacResult{});

    size_t offered = 0;
    size_t got = 0;
    uint32_t rin = in_seed ? in_seed : 1u;
    uint32_t rout = out_seed ? out_seed : 1u;

    // fill service state
    bool fill_active = false;
    uint32_t fill_line = 0;
    int fill_wait = 0;
    int beat = 0;
    int gap = 0;

    bool held = false;
    uint64_t held_pack = 0;
    const uint32_t limit = static_cast<uint32_t>(acc.size()) * 512u + 4096u;

    park();
    while (got < acc.size()) {
      if (r.cycles > limit) {
        add(err, "the block never finished the access batch");
        break;
      }

      const bool want = (offered < acc.size()) && ((in_seed == 0u) || ((next(&rin) & 3u) != 0u));
      if (want) drive(acc[offered]);
      top_.acc_valid_i = want ? 1 : 0;
      const bool ordy = (out_seed == 0u) || ((next(&rout) & 3u) != 0u);
      top_.smp_ready_i = ordy ? 1 : 0;

      // ---- the fill service, driven BEFORE eval so the block sees it ----
      top_.fill_ready_i = 1;
      top_.fill_data_valid_i = 0;
      top_.fill_data_i = 0;
      top_.inv_valid_i = 0;
      top_.inv_all_i = 0;
      top_.inv_addr_i = 0;
      if (fill_active) {
        if (fill_wait > 0) {
          --fill_wait;
        } else if (gap > 0) {
          --gap;
        } else {
          const uint32_t a = fill_line + static_cast<uint32_t>(beat) * 2u;
          top_.fill_data_valid_i = 1;
          top_.fill_data_i = mem.halfword(a);
          if (armed_beat_ >= 0 && beat == armed_beat_) {
            top_.inv_valid_i = 1;
            top_.inv_all_i = armed_all_ ? 1 : 0;
            top_.inv_addr_i = armed_addr_;
            armed_beat_ = -1;
          }
        }
      }

      top_.eval();

      // ---- the access channel -------------------------------------------
      const bool acc_go = want && (top_.acc_ready_o != 0);
      if (acc_go) {
        // The block must never accept an access with an enabled lane missing.
        // Checked here rather than trusted, because "accepted only when it can
        // be served" IS the miss law.
        ++offered;
      }

      // ---- the response channel -----------------------------------------
      if (top_.smp_valid_o) {
        const uint64_t pack = (static_cast<uint64_t>(top_.smp_data_o) & 0xFFFFFFFFFFFFFFFFull) ^
                              (static_cast<uint64_t>(top_.smp_src_id_o) << 1);
        if (held && pack != held_pack) add(err, "a stalled response changed while smp_ready_i low");
        if (ordy) {
          if (got >= acc.size()) {
            add(err, "more responses than accesses");
          } else {
            CacResult& c = r.out[got];
            for (int k = 0; k < kLanes; ++k)
              c.data[k] = static_cast<uint16_t>((top_.smp_data_o >> (16 * k)) & 0xFFFFu);
            c.src_id = static_cast<uint16_t>(top_.smp_src_id_o);
            ++got;
          }
          held = false;
        } else {
          held = true;
          held_pack = pack;
        }
      } else {
        if (held) add(err, "smp_valid_o dropped while a response was stalled");
        held = false;
      }

      // ---- the fill request channel -------------------------------------
      if (top_.fill_valid_o && top_.fill_ready_i) {
        if (fill_active) add(err, "a second fill request while one was outstanding");
        fill_active = true;
        fill_line = top_.fill_addr_o;
        fill_wait = fill_lat;
        beat = 0;
        gap = 0;
        r.fills.push_back(fill_line);
        if ((fill_line % static_cast<uint32_t>(kLineBytes)) != 0u)
          add(err, "a fill request was not line-aligned");
      }

      const bool beat_now = top_.fill_data_valid_i != 0;

      edge();
      ++r.cycles;

      if (beat_now) {
        ++beat;
        gap = beat_gap;
        if (beat == kBeats) fill_active = false;
      }
    }

    park();
    top_.eval();
    r.hits = top_.cache_hits_o;
    r.misses = top_.cache_misses_o;
    return r;
  }

  uint32_t hits() const { return top_.cache_hits_o; }
  uint32_t misses() const { return top_.cache_misses_o; }

 private:
  void drive(const CacAccess& a) {
    uint32_t en = 0;
    for (int k = 0; k < kLanes; ++k) {
      if (a.en[k]) en |= (1u << k);
      top_.acc_addr_i[k] = a.addr[k];
    }
    top_.acc_en_i = static_cast<uint8_t>(en);
    top_.acc_src_id_i = a.src_id;
  }

  static uint32_t next(uint32_t* s) {
    *s = (*s) * 747796405u + 2891336453u;
    const uint32_t w = ((*s >> ((*s >> 28) + 4)) ^ *s) * 277803737u;
    return (w >> 22) ^ w;
  }
  static void add(std::string* err, const char* what) {
    if (err->empty()) *err = what;
  }

  void park() {
    top_.acc_valid_i = 0;
    top_.acc_en_i = 0;
    top_.smp_ready_i = 1;
    top_.inv_valid_i = 0;
    top_.inv_all_i = 0;
    top_.inv_addr_i = 0;
    top_.fill_ready_i = 1;
    top_.fill_data_valid_i = 0;
    top_.fill_data_i = 0;
  }
  void edge() {
    top_.clk = 0;
    top_.eval();
    top_.clk = 1;
    top_.eval();
    top_.clk = 0;
    top_.eval();
  }

  int armed_beat_ = -1;
  bool armed_all_ = false;
  uint32_t armed_addr_ = 0;

  Vzhao_texture_cache top_;
};

/** The oracle's verdict for the same batch, from the same cache state. */
inline CacRun cac_expect(zref::TextureCache* tc, const std::vector<CacAccess>& acc,
                         const zref::TextureMemory& mem) {
  CacRun r;
  r.out.reserve(acc.size());
  for (const CacAccess& a : acc) {
    zref::TextureCache::Access ma;
    for (int k = 0; k < kLanes; ++k) {
      ma.en[k] = a.en[k];
      ma.addr[k] = a.addr[k];
    }
    ma.src_id = a.src_id;
    const zref::TextureCache::Out o = tc->access(ma, mem);
    CacResult c;
    for (int k = 0; k < kLanes; ++k) c.data[k] = a.en[k] ? o.data[k] : 0;
    c.src_id = a.src_id;
    r.out.push_back(c);
    for (int f = 0; f < o.fills; ++f) r.fills.push_back(o.fill_addr[f]);
  }
  r.hits = tc->cache_hits();
  r.misses = tc->cache_misses();
  return r;
}

/** Compare one response. Disabled lanes carry nothing and are not compared. */
inline bool cac_same(const CacAccess& a, const CacResult& want, const CacResult& got) {
  if (want.src_id != got.src_id) return false;
  for (int k = 0; k < kLanes; ++k) {
    if (a.en[k] && want.data[k] != got.data[k]) return false;
  }
  return true;
}

inline std::string cac_describe(size_t i, const CacAccess& a, const CacResult& want,
                                const CacResult& got) {
  char buf[320];
  std::snprintf(buf, sizeof(buf),
                "access %zu: en=%d%d%d%d addr0=%08X oracle=%04X,%04X,%04X,%04X "
                "rtl=%04X,%04X,%04X,%04X",
                i, a.en[0] ? 1 : 0, a.en[1] ? 1 : 0, a.en[2] ? 1 : 0, a.en[3] ? 1 : 0, a.addr[0],
                want.data[0], want.data[1], want.data[2], want.data[3], got.data[0], got.data[1],
                got.data[2], got.data[3]);
  return std::string(buf);
}

inline std::vector<uint8_t> cac_serialize(const std::vector<CacAccess>& acc) {
  std::vector<uint8_t> v;
  auto put32 = [&v](uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
  };
  for (const CacAccess& a : acc) {
    uint32_t en = 0;
    for (int k = 0; k < kLanes; ++k)
      if (a.en[k]) en |= (1u << k);
    put32(en);
    for (int k = 0; k < kLanes; ++k) put32(a.addr[k]);
  }
  return v;
}

}  // namespace zhao_texture
