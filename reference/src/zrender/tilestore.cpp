// tilestore.cpp — zref::TileStore, the RASTER.TILESTORE oracle.
//
// Law: design/contracts/RASTER.TILESTORE.md. The in-cycle ordering below is
// the contract's, in the contract's order: clear, then write (never in the
// same cycle as a clear), then the reads (which observe both), then swap.

#include "zref/zref_tilestore.hpp"

namespace zref {

void TileStore::reset() {
  for (int b = 0; b < 2; ++b) {
    clear_[b] = 0;
    for (int i = 0; i < kWords; ++i) {
      ram_[b][i] = 0;
      present_[b][i] = false;
    }
  }
  front_ = 0;
  refs_ = 0;
}

TileStore::Out TileStore::step(const Cycle& c) {
  Out o;

  // A clear locks the write port for that cycle; nothing else can stall.
  o.wr_ready = !c.clear;
  const bool wr_acc = c.wr && o.wr_ready;

  const int fb = front_;
  const int bb = 1 - front_;

  // ---- reads see the post-clear, post-write state (write-first) ----------
  // The clear reaches only the FRONT bank, so the back bank is stable for the
  // whole resolve pass by construction.
  if (c.rd) {
    const bool present = c.clear ? false : present_[fb][c.rd_addr];
    const uint64_t clear_word = c.clear ? c.clear_data : clear_[fb];
    const uint64_t base = present ? ram_[fb][c.rd_addr] : clear_word;
    o.rd_data = (wr_acc && c.wr_addr == c.rd_addr) ? c.wr_data : base;
    o.rd_valid = true;
    o.rd_src_id = c.rd_src_id;
  }
  if (c.res) {
    o.res_data = present_[bb][c.res_addr] ? ram_[bb][c.res_addr] : clear_[bb];
    o.res_valid = true;
  }

  // ---- commit the edge ---------------------------------------------------
  if (c.clear) {
    for (int i = 0; i < kWords; ++i) present_[fb][i] = false;
    clear_[fb] = c.clear_data;
  }
  if (wr_acc) {
    ram_[fb][c.wr_addr] = c.wr_data;
    present_[fb][c.wr_addr] = true;
  }
  if (c.swap) front_ = bb;

  // Accepted DATA accesses only; clear and swap are commands. Saturating per
  // spec/counters.md 4 — a counter never wraps.
  const uint32_t add =
      static_cast<uint32_t>(wr_acc) + static_cast<uint32_t>(c.rd) + static_cast<uint32_t>(c.res);
  if (add != 0) {
    if (refs_ > 0xFFFFFFFFu - add)
      refs_ = 0xFFFFFFFFu;
    else
      refs_ += add;
  }

  o.front_bank = (front_ != 0);
  o.tile_references = refs_;
  return o;
}

}  // namespace zref
