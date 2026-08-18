// raster_fragment_dev.hpp — Verilator driver for RASTER.FRAGMENT
// (fpga/rtl/raster/zhao_raster_fragment.sv, contract
// design/contracts/RASTER.FRAGMENT.md).
//
// The oracle is zref::FragmentPipeline, the ledger's declared
// `reference_model`. It restates no arithmetic it can delegate: the unit8
// product is zref::unit_mul (spec/qformats.md 3's frozen
// `((u32)a*b + 128) >> 8`) and the tile word is zref::TileStore::Word. So
// "RTL == oracle" is "RTL == the 8 strict depth test, the frozen unit8 lane,
// and the recipes those two specs ratified".
//
// THE TILE STORE IS MODELLED, NOT INSTANTIATED. This driver plays
// RASTER.TILESTORE's port A from a flat 256-word array with the store's own
// documented timing: `rd_valid_o` and `rd_data_o` are presented in the cycle
// AFTER an accepted read (its `latency: fixed:1`), and a same-cycle
// same-address write is visible to that read (its ordering rule 3,
// write-first). Modelling it here rather than wiring the real block is
// deliberate and is the same choice raster_resolve_dev.hpp made: it lets this
// lane drive read responses the real store would never produce - stalling the
// write port, or refusing a response - which is how the block's stall path
// and its `fragment_error_o` get exercised at all. The two blocks ARE wired
// together, in the composition, and tests/raster/raster_tile_pipe_*.cpp is
// where that pairing is checked against the real store.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_fragment.h"

#include "zhao_sim.hpp"
#include "zref/zref_fragment.hpp"
#include "zref/zref_tilestore.hpp"

namespace zhao_raster {

using FrState = zref::FragmentPipeline::State;
using FrFrag = zref::FragmentPipeline::Frag;
using FrWord = zref::TileStore::Word;

inline constexpr int kFrWords = zref::TileStore::kWords;

/** One write the block performed. */
struct FrWrite {
  uint8_t addr = 0;
  uint64_t data = 0;
};

/**
 * What one run observed. `writes` is the ORDERED list of tile writes: the
 * block retires strictly in order, so comparing it against the oracle's list
 * checks not only what was written but that killed fragments wrote nothing at
 * all - a block that wrote the destination back unchanged instead of skipping
 * the write would leave the tile identical and only this list would notice.
 */
struct FrRun {
  std::vector<FrWrite> writes;
  uint32_t covered = 0;  // covered_fragments_o at the end
  uint32_t blended = 0;  // blended_fragments_o at the end
  uint32_t cycles = 0;
  bool error = false;            // fragment_error_o ever fired
  uint64_t tile[kFrWords] = {};  // the modelled tile store, afterwards
};

class FrDev {
 public:
  FrDev() { reset(); }
  ~FrDev() { top_.final(); }
  FrDev(const FrDev&) = delete;
  FrDev& operator=(const FrDev&) = delete;

  void reset() {
    park();
    top_.rst_n = 0;
    top_.eval();
    for (int i = 0; i < 2; ++i) edge();
    top_.rst_n = 1;
    top_.eval();
  }

  /**
   * Run a fragment sequence against a modelled tile store preloaded with
   * `tile`. `in_seed` gates the offer; `wr_seed` gates the tile store's
   * `wr_ready_i` (the ONLY thing that can stall the block, and the reason its
   * stall re-issues its own read). Protocol violations land in *err.
   */
  FrRun run(const uint64_t* tile, const std::vector<FrFrag>& frags, uint32_t in_seed,
            uint32_t wr_seed, std::string* err) {
    reset();
    FrRun r;
    for (int i = 0; i < kFrWords; ++i) r.tile[i] = tile[i];

    size_t offered = 0;
    uint32_t rin = in_seed ? in_seed : 1u;
    uint32_t rwr = wr_seed ? wr_seed : 1u;

    // The modelled read pipeline: one cycle of latency, write-first bypass.
    bool rsp_v = false;
    uint64_t rsp_d = 0;
    const uint32_t limit = static_cast<uint32_t>(frags.size()) * 64u + 2048u;

    park();
    while (true) {
      if (r.cycles > limit) {
        add(err, "the block never drained the fragment sequence");
        break;
      }

      // ---- drive this cycle's inputs -------------------------------------
      const bool want = (offered < frags.size()) && ((in_seed == 0u) || ((next(&rin) & 3u) != 0u));
      if (want) drive(frags[offered]);
      top_.frag_valid_i = want ? 1 : 0;
      top_.rd_ready_i = 1;  // RASTER.TILESTORE's read ready is a constant 1
      const bool wrdy = (wr_seed == 0u) || ((next(&rwr) & 3u) != 0u);
      top_.wr_ready_i = wrdy ? 1 : 0;
      top_.rd_valid_i = rsp_v ? 1 : 0;
      top_.rd_data_i = rsp_d;
      top_.eval();

      if (top_.fragment_error_o) {
        r.error = true;
        add(err, "fragment_error_o fired");
      }

      const bool frag_acc = want && (top_.frag_ready_o != 0);
      const bool rd_acc = (top_.rd_valid_o != 0);  // rd_ready_i is held high
      const uint8_t rd_addr = static_cast<uint8_t>(top_.rd_addr_o);
      const bool wr_acc = (top_.wr_valid_o != 0) && wrdy;
      const uint8_t wr_addr = static_cast<uint8_t>(top_.wr_addr_o);
      const uint64_t wr_data = top_.wr_data_o;

      // ---- apply the store's own in-cycle ordering ------------------------
      // Write first, then the read observes it (ordering rule 3). That is the
      // rule the block's same-pixel back-to-back correctness rests on.
      if (wr_acc) {
        r.tile[wr_addr] = wr_data;
        FrWrite w;
        w.addr = wr_addr;
        w.data = wr_data;
        r.writes.push_back(w);
      }
      const bool next_rsp_v = rd_acc;
      const uint64_t next_rsp_d = rd_acc ? r.tile[rd_addr] : 0;

      edge();
      ++r.cycles;
      rsp_v = next_rsp_v;
      rsp_d = next_rsp_d;
      if (frag_acc) ++offered;

      // Done when everything has been offered and the block is idle. `idle_o`
      // is the block's own "nothing anywhere in the pipe".
      top_.eval();
      if (offered == frags.size() && top_.idle_o) break;
    }

    park();
    top_.eval();
    r.covered = top_.covered_fragments_o;
    r.blended = top_.blended_fragments_o;
    return r;
  }

 private:
  void drive(const FrFrag& f) {
    top_.frag_addr_i = f.addr;
    top_.frag_depth_i = f.depth & 0xFFFFFFu;
    top_.frag_state_i = f.state;
    top_.frag_src_id_i = 0x321;
    top_.frag_vert_rgb_i =
        (static_cast<uint32_t>(f.vr) << 16) | (static_cast<uint32_t>(f.vg) << 8) | f.vb;
    top_.frag_vert_a_i = f.va;
    top_.frag_tag_i = f.tag;
    top_.frag_sten_ref_i = f.sten_ref;
    top_.frag_texel_rgb_i =
        (static_cast<uint32_t>(f.tr) << 16) | (static_cast<uint32_t>(f.tg) << 8) | f.tb;
    top_.frag_texel_a_i = f.ta;
    top_.frag_texel_idx_i = f.tidx;
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
    top_.rd_ready_i = 1;
    top_.wr_ready_i = 1;
    top_.rd_valid_i = 0;
    top_.rd_data_i = 0;
  }
  void edge() {
    top_.clk = 0;
    top_.eval();
    top_.clk = 1;
    top_.eval();
    top_.clk = 0;
    top_.eval();
  }

  Vzhao_raster_fragment top_;
};

/** What the oracle says the whole sequence does to the tile. */
struct FrExpect {
  uint64_t tile[kFrWords] = {};
  std::vector<FrWrite> writes;
  uint32_t blended = 0;
};

inline FrExpect fr_expect(const uint64_t* tile, const std::vector<FrFrag>& frags) {
  FrExpect e;
  for (int i = 0; i < kFrWords; ++i) e.tile[i] = tile[i];
  for (const FrFrag& f : frags) {
    const zref::FragmentPipeline::Out o = zref::FragmentPipeline::apply(f, e.tile[f.addr]);
    if (!o.write) continue;
    e.tile[f.addr] = o.word;
    FrWrite w;
    w.addr = f.addr;
    w.data = o.word;
    e.writes.push_back(w);
    if (o.blended) ++e.blended;
  }
  return e;
}

/** A tile filled with one word. */
inline void fr_fill(uint64_t* tile, uint64_t word) {
  for (int i = 0; i < kFrWords; ++i) tile[i] = word;
}

inline uint64_t fr_word(uint8_t r, uint8_t g, uint8_t b, uint8_t tag, uint32_t depth,
                        uint8_t stencil) {
  FrWord w;
  w.r = r;
  w.g = g;
  w.b = b;
  w.tag = tag;
  w.depth = depth;
  w.stencil = stencil;
  return w.pack();
}

inline std::string fr_describe(size_t i, uint64_t want, uint64_t got) {
  const FrWord a = FrWord::unpack(want);
  const FrWord b = FrWord::unpack(got);
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "pixel %zu: oracle rgb %02X%02X%02X tag %02X depth %06X sten %02X; "
                "rtl rgb %02X%02X%02X tag %02X depth %06X sten %02X",
                i, a.r, a.g, a.b, a.tag, a.depth, a.stencil, b.r, b.g, b.b, b.tag, b.depth,
                b.stencil);
  return std::string(buf);
}

inline std::vector<uint8_t> fr_serialize(const std::vector<FrFrag>& frags) {
  std::vector<uint8_t> v;
  auto put32 = [&v](uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
  };
  for (const FrFrag& f : frags) {
    put32(f.addr);
    put32(f.depth);
    put32(f.state);
    put32((static_cast<uint32_t>(f.vr) << 16) | (static_cast<uint32_t>(f.vg) << 8) | f.vb);
    put32((static_cast<uint32_t>(f.va) << 24) | (static_cast<uint32_t>(f.tag) << 16) |
          (static_cast<uint32_t>(f.sten_ref) << 8) | f.tidx);
    put32((static_cast<uint32_t>(f.tr) << 16) | (static_cast<uint32_t>(f.tg) << 8) | f.tb);
    put32(f.ta);
  }
  return v;
}

}  // namespace zhao_raster
