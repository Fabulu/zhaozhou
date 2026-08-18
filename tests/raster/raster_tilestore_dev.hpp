// raster_tilestore_dev.hpp — Verilator driver for the RASTER.TILESTORE
// differential tests (design/contracts/RASTER.TILESTORE.md).
//
// The oracle is zref::TileStore (reference/include/zref/zref_tilestore.hpp) —
// the ledger's declared reference_model. This file contains no store
// semantics of its own: it applies one cycle of stimulus to the Verilated
// model and returns the SAME `zref::TileStore::Out` shape the oracle returns,
// so a test is one `step()` against one `step()`.
//
// CYCLE ALIGNMENT. `wr_ready` is combinational and is sampled BEFORE the
// clock edge; everything else (`rd_*`, `res_*`, `front_bank`,
// `tile_references`) is sampled AFTER it, which is exactly the fixed 1-cycle
// read latency the ledger declares. The oracle's `step()` documents the same
// alignment, so the two line up beat for beat with no fudge factor.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "verilated.h"

#include "Vzhao_raster_tilestore.h"

#include "zhao_sim.hpp"
#include "zref/zref_tilestore.hpp"

namespace zhao_raster {

using Store = zref::TileStore;
using Cycle = zref::TileStore::Cycle;
using StoreOut = zref::TileStore::Out;
using Word = zref::TileStore::Word;

inline constexpr int kTile = Store::kTile;
inline constexpr int kWords = Store::kWords;

class StoreDev {
 public:
  // Held BY VALUE (the edgewalk driver's rule): the device owns no dynamic
  // resource, so there is no copy/ownership hazard to get wrong.
  StoreDev() { reset(); }
  ~StoreDev() { top_.final(); }
  StoreDev(const StoreDev&) = delete;
  StoreDev& operator=(const StoreDev&) = delete;

  void reset() {
    park();
    top_.rst_n = 0;
    top_.eval();
    for (int i = 0; i < 2; ++i) edge();
    top_.rst_n = 1;
    top_.eval();
  }

  StoreOut step(const Cycle& c) {
    StoreOut o;

    top_.clear_valid_i = c.clear;
    top_.clear_data_i = c.clear_data;
    top_.wr_valid_i = c.wr;
    top_.wr_addr_i = c.wr_addr;
    top_.wr_data_i = c.wr_data;
    top_.rd_valid_i = c.rd;
    top_.rd_addr_i = c.rd_addr;
    top_.rd_src_id_i = c.rd_src_id;
    top_.res_valid_i = c.res;
    top_.res_addr_i = c.res_addr;
    top_.swap_valid_i = c.swap;
    top_.eval();

    // combinational, in the stimulus cycle
    o.wr_ready = top_.wr_ready_o != 0;
    // the contract's "no port has a stall condition of its own"
    clear_ready_seen_ = top_.clear_ready_o != 0;
    rd_ready_seen_ = top_.rd_ready_o != 0;
    res_ready_seen_ = top_.res_ready_o != 0;
    swap_ready_seen_ = top_.swap_ready_o != 0;

    edge();
    park();
    top_.eval();

    o.rd_valid = top_.rd_valid_o != 0;
    o.rd_data = top_.rd_data_o;
    o.rd_src_id = top_.rd_src_id_o;
    o.res_valid = top_.res_valid_o != 0;
    o.res_data = top_.res_data_o;
    o.front_bank = top_.front_bank_o != 0;
    o.tile_references = top_.tile_references_o;
    return o;
  }

  bool always_ready() const {
    return clear_ready_seen_ && rd_ready_seen_ && res_ready_seen_ && swap_ready_seen_;
  }

 private:
  void park() {
    top_.clear_valid_i = 0;
    top_.wr_valid_i = 0;
    top_.rd_valid_i = 0;
    top_.res_valid_i = 0;
    top_.swap_valid_i = 0;
  }
  void edge() {
    top_.clk = 0;
    top_.eval();
    top_.clk = 1;
    top_.eval();
    top_.clk = 0;
    top_.eval();
  }

  Vzhao_raster_tilestore top_;
  bool clear_ready_seen_ = true;
  bool rd_ready_seen_ = true;
  bool res_ready_seen_ = true;
  bool swap_ready_seen_ = true;
};

// ---------------------------------------------------------------- helpers ---
// A pseudo-word with every field distinct, so a field-swap defect in the
// packing shows up rather than cancelling out.
inline uint64_t make_word(uint32_t k) {
  Word w;
  w.r = static_cast<uint8_t>(k * 7u + 1u);
  w.g = static_cast<uint8_t>(k * 13u + 2u);
  w.b = static_cast<uint8_t>(k * 29u + 3u);
  w.tag = static_cast<uint8_t>(k * 31u + 4u);
  w.depth = (k * 2654435761u) & 0xFFFFFFu;
  w.stencil = static_cast<uint8_t>(k * 37u + 5u);
  return w.pack();
}

inline std::string describe(const Cycle& c, const StoreOut& want, const StoreOut& got) {
  char buf[320];
  std::snprintf(buf, sizeof(buf),
                "stim clear=%d/%016llX wr=%d@%02X/%016llX rd=%d@%02X res=%d@%02X swap=%d",
                static_cast<int>(c.clear), static_cast<unsigned long long>(c.clear_data),
                static_cast<int>(c.wr), c.wr_addr, static_cast<unsigned long long>(c.wr_data),
                static_cast<int>(c.rd), c.rd_addr, static_cast<int>(c.res), c.res_addr,
                static_cast<int>(c.swap));
  std::string s(buf);
  std::snprintf(buf, sizeof(buf),
                "\n  want rd %d/%016llX res %d/%016llX front %d wr_ready %d refs %u"
                "\n  got  rd %d/%016llX res %d/%016llX front %d wr_ready %d refs %u",
                static_cast<int>(want.rd_valid), static_cast<unsigned long long>(want.rd_data),
                static_cast<int>(want.res_valid), static_cast<unsigned long long>(want.res_data),
                static_cast<int>(want.front_bank), static_cast<int>(want.wr_ready),
                want.tile_references, static_cast<int>(got.rd_valid),
                static_cast<unsigned long long>(got.rd_data), static_cast<int>(got.res_valid),
                static_cast<unsigned long long>(got.res_data), static_cast<int>(got.front_bank),
                static_cast<int>(got.wr_ready), got.tile_references);
  s += buf;
  return s;
}

inline bool same(const StoreOut& a, const StoreOut& b) {
  if (a.wr_ready != b.wr_ready) return false;
  if (a.rd_valid != b.rd_valid) return false;
  if (a.rd_valid && (a.rd_data != b.rd_data || a.rd_src_id != b.rd_src_id)) return false;
  if (a.res_valid != b.res_valid) return false;
  if (a.res_valid && a.res_data != b.res_data) return false;
  if (a.front_bank != b.front_bank) return false;
  return a.tile_references == b.tile_references;
}

}  // namespace zhao_raster
