// texture_tmu_dev.hpp — Verilator driver for TEXTURE.TMU
// (fpga/rtl/texture/zhao_texture_tmu.sv, contract
// design/contracts/TEXTURE.TMU.md).
//
// The oracle is zref::Tmu, the ledger's declared `reference_model`. It
// restates no arithmetic it can delegate: the RGB565 expansion is
// zref::sky::rgb565::to_rgb888 (spec/stars_and_flares.md §2's frozen law) and
// the mirrored-repeat wrap is the generalisation of
// zref::terrain::mirror_texel (spec/terrain_rules.md §6.2, frozen) that
// texture_tmu_directed.cpp pins against the frozen helper itself. What the
// oracle does NOT share with the RTL is any structure — it has no state
// machine, no cache handshake, and it computes the mip level offset with a
// summation loop where the RTL uses a base-4-repunit closed form — so
// "RTL == oracle" tests the RTL's mechanics and that identity, not a shared
// clever expression.
//
// THE CACHE IS MODELLED, NOT INSTANTIATED. Same choice, and the same reason,
// as raster_fragment_dev.hpp's modelled tile store: it lets this lane drive
// cache timings the real block would never produce — a response in the cycle
// after the request, a response withheld for twenty cycles, a request stalled
// before it is taken — which is how the TMU's handshake and its
// `latency: variable_bounded:16` claim get exercised at all. The real
// TEXTURE.CACHE is verified against its own oracle in texture_cache_*.cpp;
// no composition block exists for the TEXTURE subsystem (the ledger registers
// four blocks and none of them is "the composition"), and adding one is a
// ledger edit, not this increment's.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_tmu.h"

#include "zhao_sim.hpp"
#include "zref/zref_texture.hpp"

namespace zhao_texture {

/** One request offered to the block. */
struct TmuReq {
  int32_t u = 0;
  int32_t v = 0;
  uint32_t base = 0;
  uint32_t pal_base = 0;
  uint32_t mode = 0;
  uint8_t lod = 0;
  uint16_t src_id = 0;
};

/** One sample observed. */
struct TmuSample {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
  uint8_t idx = 0;
  uint16_t src_id = 0;
  bool mode_error = false;
};

/** What one batch observed. */
struct TmuRun {
  std::vector<TmuSample> out;
  uint32_t samples = 0;      // texture_samples_o at the end
  uint32_t max_latency = 0;  // accept -> retire, in cycles
  uint32_t cycles = 0;
};

class TmuDev {
 public:
  TmuDev() { reset(); }
  ~TmuDev() { top_.final(); }
  TmuDev(const TmuDev&) = delete;
  TmuDev& operator=(const TmuDev&) = delete;

  void reset() {
    park();
    top_.rst_n = 0;
    top_.eval();
    for (int i = 0; i < 2; ++i) edge();
    top_.rst_n = 1;
    top_.eval();
  }

  /**
   * Feed a batch. `in_seed` != 0 gates the offer, `out_seed` != 0 gates
   * `smp_ready_i`, `cac_stall` != 0 gates the modelled cache's readiness.
   * `cac_lat` is the cycles from an accepted cache access to its response.
   */
  TmuRun feed(const std::vector<TmuReq>& reqs, const zref::TextureMemory& mem, uint32_t in_seed,
              uint32_t out_seed, uint32_t cac_stall, int cac_lat, std::string* err) {
    TmuRun r;
    r.out.assign(reqs.size(), TmuSample{});

    size_t offered = 0;
    size_t got = 0;
    uint32_t rin = in_seed ? in_seed : 1u;
    uint32_t rout = out_seed ? out_seed : 1u;
    uint32_t rcs = cac_stall ? cac_stall : 1u;

    // modelled cache state
    bool cac_busy = false;
    int cac_wait = 0;
    uint16_t cac_hw[4] = {};

    bool held = false;
    uint64_t held_pack = 0;
    std::vector<uint32_t> accept_cycle(reqs.size(), 0);
    bool err_pending = false;
    size_t err_idx = 0;
    const uint32_t limit = static_cast<uint32_t>(reqs.size()) * 512u + 4096u;

    park();
    while (got < reqs.size()) {
      if (r.cycles > limit) {
        add(err, "the block never finished the request batch");
        break;
      }

      const bool want = (offered < reqs.size()) && ((in_seed == 0u) || ((next(&rin) & 3u) != 0u));
      if (want) drive(reqs[offered]);
      top_.req_valid_i = want ? 1 : 0;
      const bool ordy = (out_seed == 0u) || ((next(&rout) & 3u) != 0u);
      top_.smp_ready_i = ordy ? 1 : 0;

      // ---- the modelled cache, driven BEFORE eval -----------------------
      const bool crdy = (cac_stall == 0u) || ((next(&rcs) & 3u) != 0u);
      top_.cac_ready_i = (!cac_busy && crdy) ? 1 : 0;
      top_.cac_valid_i = 0;
      top_.cac_data_i = 0;
      if (cac_busy && cac_wait == 0) {
        top_.cac_valid_i = 1;
        uint64_t d = 0;
        for (int k = 0; k < 4; ++k) d |= static_cast<uint64_t>(cac_hw[k]) << (16 * k);
        top_.cac_data_i = d;
      }

      top_.eval();

      // ---- the request channel ------------------------------------------
      const bool req_go = want && (top_.req_ready_o != 0);

      // ---- the cache request --------------------------------------------
      if (top_.cac_valid_o && top_.cac_ready_i) {
        if (cac_busy) add(err, "a cache access while one was outstanding");
        cac_busy = true;
        cac_wait = cac_lat;
        for (int k = 0; k < 4; ++k)
          cac_hw[k] = mem.halfword(static_cast<uint32_t>(top_.cac_addr_o[k]));
      }
      const bool cac_taken = (top_.cac_valid_i != 0) && (top_.cac_ready_o != 0);

      // ---- mode_error is a pulse in the cycle after acceptance ----------
      if (top_.mode_error_o) {
        if (!err_pending) {
          add(err, "mode_error_o pulsed with no request to attribute it to");
        } else {
          r.out[err_idx].mode_error = true;
        }
      }

      // ---- the sample channel -------------------------------------------
      if (top_.smp_valid_o) {
        const uint64_t pack = (static_cast<uint64_t>(top_.smp_rgb_o) << 24) |
                              (static_cast<uint64_t>(top_.smp_a_o) << 16) |
                              (static_cast<uint64_t>(top_.smp_idx_o) << 8);
        if (held && pack != held_pack) add(err, "a stalled sample changed while smp_ready_i low");
        if (ordy) {
          if (got >= reqs.size()) {
            add(err, "more samples than requests");
          } else {
            TmuSample& s = r.out[got];
            s.r = static_cast<uint8_t>((top_.smp_rgb_o >> 16) & 0xFFu);
            s.g = static_cast<uint8_t>((top_.smp_rgb_o >> 8) & 0xFFu);
            s.b = static_cast<uint8_t>(top_.smp_rgb_o & 0xFFu);
            s.a = static_cast<uint8_t>(top_.smp_a_o);
            s.idx = static_cast<uint8_t>(top_.smp_idx_o);
            s.src_id = static_cast<uint16_t>(top_.smp_src_id_o);
            const uint32_t lat = r.cycles - accept_cycle[got];
            if (lat > r.max_latency) r.max_latency = lat;
            ++got;
          }
          held = false;
        } else {
          held = true;
          held_pack = pack;
        }
      } else {
        if (held) add(err, "smp_valid_o dropped while a sample was stalled");
        held = false;
      }

      edge();
      ++r.cycles;

      if (cac_busy) {
        if (cac_wait > 0) --cac_wait;
        if (cac_taken) cac_busy = false;
      }
      err_pending = req_go;
      if (req_go) {
        err_idx = offered;
        accept_cycle[offered] = r.cycles - 1u;
        ++offered;
      }
    }

    park();
    top_.eval();
    r.samples = top_.texture_samples_o;
    return r;
  }

 private:
  void drive(const TmuReq& q) {
    top_.req_u_i = static_cast<uint32_t>(q.u);
    top_.req_v_i = static_cast<uint32_t>(q.v);
    top_.req_base_i = q.base;
    top_.req_pal_base_i = q.pal_base;
    top_.req_mode_i = q.mode;
    top_.req_lod_i = q.lod;
    top_.req_src_id_i = q.src_id;
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
    top_.req_valid_i = 0;
    top_.smp_ready_i = 1;
    top_.cac_ready_i = 1;
    top_.cac_valid_i = 0;
    top_.cac_data_i = 0;
  }
  void edge() {
    top_.clk = 0;
    top_.eval();
    top_.clk = 1;
    top_.eval();
    top_.clk = 0;
    top_.eval();
  }

  Vzhao_texture_tmu top_;
};

/** The oracle's verdict for the same batch. */
inline std::vector<TmuSample> tmu_expect(const std::vector<TmuReq>& reqs,
                                         const zref::TextureMemory& mem) {
  std::vector<TmuSample> out;
  out.reserve(reqs.size());
  for (const TmuReq& q : reqs) {
    zref::Tmu::Req r;
    r.u = q.u;
    r.v = q.v;
    r.base = q.base;
    r.pal_base = q.pal_base;
    r.mode = q.mode;
    r.lod = q.lod;
    r.src_id = q.src_id;
    const zref::Tmu::Sample s = zref::Tmu::sample(r, mem);
    TmuSample t;
    t.r = s.r;
    t.g = s.g;
    t.b = s.b;
    t.a = s.a;
    t.idx = s.idx;
    t.src_id = s.src_id;
    t.mode_error = s.mode_error;
    out.push_back(t);
  }
  return out;
}

inline bool tmu_same(const TmuSample& want, const TmuSample& got) {
  return want.r == got.r && want.g == got.g && want.b == got.b && want.a == got.a &&
         want.idx == got.idx && want.src_id == got.src_id && want.mode_error == got.mode_error;
}

inline std::string tmu_describe(size_t i, const TmuSample& want, const TmuSample& got) {
  char buf[288];
  std::snprintf(buf, sizeof(buf),
                "request %zu: oracle rgba=%02X%02X%02X%02X idx=%02X err=%d; "
                "rtl rgba=%02X%02X%02X%02X idx=%02X err=%d",
                i, want.r, want.g, want.b, want.a, want.idx, want.mode_error ? 1 : 0, got.r, got.g,
                got.b, got.a, got.idx, got.mode_error ? 1 : 0);
  return std::string(buf);
}

inline std::vector<uint8_t> tmu_serialize(const std::vector<TmuReq>& reqs) {
  std::vector<uint8_t> v;
  auto put32 = [&v](uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
  };
  for (const TmuReq& q : reqs) {
    put32(static_cast<uint32_t>(q.u));
    put32(static_cast<uint32_t>(q.v));
    put32(q.base);
    put32(q.pal_base);
    put32(q.mode);
    put32(q.lod);
  }
  return v;
}

}  // namespace zhao_texture
