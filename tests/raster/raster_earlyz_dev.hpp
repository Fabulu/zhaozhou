// raster_earlyz_dev.hpp — Verilator driver for RASTER.EARLYZ
// (fpga/rtl/raster/zhao_raster_earlyz.sv, contract
// design/contracts/RASTER.EARLYZ.md).
//
// The oracle is zref::EarlyZ, the ledger's declared `reference_model`. Like
// zref::TileStore it is a plainly-written model of the CONTRACT rather than a
// view onto a frozen function — nothing earlier in this repository maintains
// a hierarchical-Z floor — so what the differential lanes exercise is the
// RTL's registered output stage, its backpressure and its saturating
// counters, not a shared clever idea. The two DO share one thing and only
// one: the fragment state word's bit positions, which come from
// zref::FragmentPipeline::State in both directions. This file re-derives no
// bit position, no depth rule and no bin index.
//
// CYCLE MODEL. `feed()` drives one fragment per accepted cycle and collects
// the candidate stream and the reject pulses, checking the ready/valid
// protocol as it goes; `expect()` runs the identical sequence through
// zref::EarlyZ. The RTL's `latency: fixed:1` means a fragment accepted in
// cycle N produces its candidate (or its reject pulse) in cycle N+1, and the
// driver asserts exactly that rather than merely tolerating it.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_earlyz.h"

#include "zhao_sim.hpp"
#include "zref/zref_earlyz.hpp"
#include "zref/zref_fragment.hpp"

namespace zhao_raster {

using EzState = zref::FragmentPipeline::State;

inline constexpr int kEzWords = zref::EarlyZ::kWords;

/** One fragment offered to the block. */
struct EzFrag {
  uint8_t addr = 0;
  uint32_t depth = 0;
  uint32_t state = 0;
  uint16_t src_id = 0;
  uint64_t payload_lo = 0;  // the opaque payload's low 64 bits
  uint32_t payload_hi = 0;  // ...and its high 24 (PAYLOAD_W is 88)
};

/** What came back out for one fragment. */
struct EzDecision {
  bool keep = false;
  uint8_t bin = 0;
  uint8_t addr = 0;
  uint32_t depth = 0;
  uint32_t state = 0;
  uint16_t src_id = 0;
  uint64_t payload_lo = 0;
  uint32_t payload_hi = 0;
};

/** One batch's worth of observations. */
struct EzRun {
  std::vector<EzDecision> out;   // one entry per fragment, in order
  uint32_t rejects = 0;          // z_reject_o pulses seen
  uint32_t counter_rejects = 0;  // early_z_rejects_o at the end
  uint32_t counter_covered = 0;  // covered_fragments_o at the end
  uint8_t bin_mask = 0;
  uint32_t z_floor = 0;
  uint32_t cycles = 0;
};

class EzDev {
 public:
  EzDev() { reset(); }
  ~EzDev() { top_.final(); }
  EzDev(const EzDev&) = delete;
  EzDev& operator=(const EzDev&) = delete;

  void reset() {
    park();
    top_.rst_n = 0;
    top_.eval();
    for (int i = 0; i < 2; ++i) edge();
    top_.rst_n = 1;
    top_.eval();
  }

  /**
   * Begin a tile. One cycle, accepted unconditionally (the contract's
   * "accepted like RASTER.TILESTORE's clear"), issued with no work in flight.
   */
  void tile_begin(uint32_t clear_depth) {
    park();
    top_.tile_begin_i = 1;
    top_.tile_clear_depth_i = clear_depth & 0xFFFFFFu;
    top_.eval();
    edge();
    park();
    top_.eval();
  }

  /**
   * Feed a sequence of fragments. `cand_seed` != 0 PCG-gates `cand_ready_i`
   * (the downstream backpressure lane); `in_seed` != 0 gates the offer, so
   * `frag_valid_i` goes low mid-stream. Protocol violations land in *err.
   */
  EzRun feed(const std::vector<EzFrag>& frags, uint32_t in_seed, uint32_t cand_seed,
             std::string* err) {
    EzRun r;
    r.out.assign(frags.size(), EzDecision{});

    size_t offered = 0;  // fragments handed over
    size_t decided = 0;  // decisions collected
    uint32_t rin = in_seed ? in_seed : 1u;
    uint32_t rc = cand_seed ? cand_seed : 1u;
    bool held = false;
    uint64_t held_pack = 0;
    // The fixed:1 contract, checked rather than assumed: `pending` is the
    // index of the fragment accepted in the PREVIOUS cycle, which must
    // produce its decision in THIS one.
    bool pending = false;
    size_t pending_idx = 0;
    const uint32_t limit = static_cast<uint32_t>(frags.size()) * 64u + 1024u;

    park();
    while (decided < frags.size()) {
      if (r.cycles > limit) {
        add(err, "the block never finished the fragment sequence");
        break;
      }

      const bool want = (offered < frags.size()) && ((in_seed == 0u) || ((next(&rin) & 3u) != 0u));
      if (want) drive(frags[offered]);
      top_.frag_valid_i = want ? 1 : 0;
      const bool crdy = (cand_seed == 0u) || ((next(&rc) & 3u) != 0u);
      top_.cand_ready_i = crdy ? 1 : 0;
      top_.eval();

      const bool acc = want && (top_.frag_ready_o != 0);

      // ---- the candidate channel ------------------------------------------
      bool got_cand = false;
      if (top_.cand_valid_o) {
        const uint64_t pack = (static_cast<uint64_t>(top_.cand_addr_o) << 56) |
                              (static_cast<uint64_t>(top_.cand_depth_o) << 32) |
                              (static_cast<uint64_t>(top_.cand_src_id_o) << 8) |
                              static_cast<uint64_t>(top_.cand_bin_o);
        if (held && pack != held_pack)
          add(err, "a stalled candidate changed while cand_ready_i low");
        if (crdy) {
          if (decided >= frags.size()) {
            add(err, "more candidates than fragments");
          } else {
            EzDecision& d = r.out[decided];
            d.keep = true;
            d.bin = static_cast<uint8_t>(top_.cand_bin_o);
            d.addr = static_cast<uint8_t>(top_.cand_addr_o);
            d.depth = top_.cand_depth_o;
            d.state = top_.cand_state_o;
            d.src_id = top_.cand_src_id_o;
            d.payload_lo = payload_lo();
            d.payload_hi = payload_hi();
          }
          got_cand = true;
          held = false;
        } else {
          held = true;
          held_pack = pack;
        }
      } else {
        if (held) add(err, "cand_valid_o dropped while a candidate was stalled");
        held = false;
      }

      // ---- the reject pulse -------------------------------------------------
      const bool got_rej = top_.z_reject_o != 0;
      if (got_rej) {
        ++r.rejects;
        if (decided < frags.size()) {
          r.out[decided].keep = false;
          r.out[decided].addr = static_cast<uint8_t>(top_.z_reject_addr_o);
        }
      }
      if (got_cand && got_rej) add(err, "a fragment was both kept and rejected in one cycle");

      // ---- `latency: fixed:1`, checked ---------------------------------------
      // The fragment accepted last cycle must present its decision now: either
      // a reject pulse, or a candidate standing on the output (which may be
      // stalled by cand_ready_i, but must be VALID).
      if (pending) {
        const bool decided_now = got_rej || (top_.cand_valid_o != 0);
        if (!decided_now)
          add(err, "a fragment's decision did not appear one cycle after acceptance");
        (void)pending_idx;
      }

      if (got_cand || got_rej) ++decided;

      edge();
      ++r.cycles;
      pending = acc;
      if (acc) {
        pending_idx = offered;
        ++offered;
      }
    }

    park();
    top_.eval();
    r.counter_rejects = top_.early_z_rejects_o;
    r.counter_covered = top_.covered_fragments_o;
    r.bin_mask = static_cast<uint8_t>(top_.bin_mask_o);
    r.z_floor = top_.z_floor_o;
    return r;
  }

  uint32_t z_floor() const { return top_.z_floor_o; }
  uint8_t bin_mask() const { return static_cast<uint8_t>(top_.bin_mask_o); }

 private:
  uint64_t payload_lo() const {
    return static_cast<uint64_t>(top_.cand_payload_o[0]) |
           (static_cast<uint64_t>(top_.cand_payload_o[1]) << 32);
  }
  uint32_t payload_hi() const { return top_.cand_payload_o[2]; }

  void drive(const EzFrag& f) {
    top_.frag_addr_i = f.addr;
    top_.frag_depth_i = f.depth & 0xFFFFFFu;
    top_.frag_state_i = f.state;
    top_.frag_src_id_i = f.src_id;
    top_.frag_payload_i[0] = static_cast<uint32_t>(f.payload_lo);
    top_.frag_payload_i[1] = static_cast<uint32_t>(f.payload_lo >> 32);
    top_.frag_payload_i[2] = f.payload_hi & 0xFFFFFFu;
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
    top_.frag_valid_i = 0;
    top_.cand_ready_i = 1;
    top_.tile_begin_i = 0;
  }
  void edge() {
    top_.clk = 0;
    top_.eval();
    top_.clk = 1;
    top_.eval();
    top_.clk = 0;
    top_.eval();
  }

  Vzhao_raster_earlyz top_;
};

/** The oracle's verdict for the same sequence, from the same tile state. */
inline std::vector<EzDecision> ez_expect(zref::EarlyZ& ez, const std::vector<EzFrag>& frags) {
  std::vector<EzDecision> out;
  out.reserve(frags.size());
  for (const EzFrag& f : frags) {
    const zref::EarlyZ::Out o = ez.fragment(f.addr, f.depth, f.state);
    EzDecision d;
    d.keep = o.keep;
    d.bin = o.bin;
    d.addr = f.addr;
    d.depth = f.depth & 0xFFFFFFu;
    d.state = f.state;
    d.src_id = f.src_id;
    d.payload_lo = f.payload_lo;
    d.payload_hi = f.payload_hi & 0xFFFFFFu;
    out.push_back(d);
  }
  return out;
}

/** Compare one decision, passthrough included (the payload is opaque, not lost). */
inline bool ez_same(const EzDecision& want, const EzDecision& got) {
  if (want.keep != got.keep) return false;
  if (want.addr != got.addr) return false;
  if (!want.keep) return true;  // a reject carries only its address
  return want.bin == got.bin && want.depth == got.depth && want.state == got.state &&
         want.src_id == got.src_id && want.payload_lo == got.payload_lo &&
         want.payload_hi == got.payload_hi;
}

inline std::string ez_describe(size_t i, const EzDecision& want, const EzDecision& got) {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "fragment %zu: oracle keep=%d addr=%02X bin=%u; rtl keep=%d addr=%02X bin=%u", i,
                static_cast<int>(want.keep), want.addr, want.bin, static_cast<int>(got.keep),
                got.addr, got.bin);
  return std::string(buf);
}

inline std::vector<uint8_t> ez_serialize(const std::vector<EzFrag>& frags) {
  std::vector<uint8_t> v;
  auto put32 = [&v](uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
  };
  for (const EzFrag& f : frags) {
    put32(f.addr);
    put32(f.depth);
    put32(f.state);
  }
  return v;
}

}  // namespace zhao_raster
