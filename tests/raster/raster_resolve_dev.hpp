// raster_resolve_dev.hpp — Verilator driver for the RASTER.RESOLVE
// differential tests (design/contracts/RASTER.RESOLVE.md).
//
// The oracle is zref::TileResolve (reference/include/zref/zref_tileresolve.hpp)
// — the ledger's declared reference_model, which is itself a view onto the
// frozen zref::render::resolve_rgb565 and NOT a second implementation of the
// dither. This file therefore contains no dither arithmetic and no CRC
// arithmetic at all: it plays the tile store behind the block's `tile_read`
// master port, collects the pixel stream, and compares.
//
// THE TILE STORE MODEL. The block masters a read port with the RASTER.
// TILESTORE contract: a request accepted in cycle N returns its data with
// `tr_data_valid_i` in cycle N+1, exactly one response per accepted request.
// The driver serves that from a flat 256-word array and gates `tr_ready_i`
// with a PCG bit stream when a stall seed is given, so the credit rule in
// the RTL (occupancy < 2) is exercised rather than assumed.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_resolve.h"

#include "zhao_sim.hpp"
#include "zref/zref_tileresolve.hpp"
#include "zref/zref_tilestore.hpp"

namespace zhao_raster {

using Resolved = zref::TileResolve::Out;
using Word = zref::TileStore::Word;

inline constexpr int kTile = zref::TileResolve::kTile;
inline constexpr int kPixels = zref::TileResolve::kPixels;

// the oracle, by its ledger name
inline Resolved oracle_resolve(const uint64_t* words, int32_t tx, int32_t ty) {
  return zref::TileResolve::tile(words, tx, ty);
}

class ResolveDev {
 public:
  ResolveDev() { reset(); }
  ~ResolveDev() { top_.final(); }
  ResolveDev(const ResolveDev&) = delete;
  ResolveDev& operator=(const ResolveDev&) = delete;

  void reset() {
    park();
    top_.rst_n = 0;
    top_.eval();
    for (int i = 0; i < 2; ++i) edge();
    top_.rst_n = 1;
    top_.eval();
  }

  // Resolves one tile. `stall_seed` != 0 gates BOTH tr_ready_i and fb_ready_i
  // with a PCG bit stream; 0 means "always ready". Any protocol violation is
  // appended to *err (empty = clean).
  Resolved run(const uint64_t* words, int32_t tx, int32_t ty, uint16_t index, uint16_t src,
               uint32_t stall_seed, std::string* err) {
    Resolved out;
    bool got_pixel[kPixels] = {};
    uint32_t rng = stall_seed;
    cycles_ = 0;

    // ---- offer the tile (ready/valid) ------------------------------------
    park();
    top_.start_valid_i = 1;
    top_.start_tile_x_i = mask12(tx);
    top_.start_tile_y_i = mask12(ty);
    top_.start_tile_index_i = index;
    top_.start_src_id_i = src;
    top_.eval();
    for (int guard = 0; !top_.start_ready_o; ++guard) {
      if (guard > 64) {
        add(err, "start_ready_o never asserted");
        return out;
      }
      edge();
      top_.eval();
    }
    edge();  // the accepting edge
    park();

    // ---- run the tile ----------------------------------------------------
    bool pend = false;  // a response is due THIS cycle
    uint64_t pend_data = 0;
    uint32_t next_addr = 0;  // the request address the block owes us
    uint32_t emitted = 0;
    bool held = false;  // previous cycle stalled an fb beat
    uint64_t held_pack = 0;
    bool seen_last = false;
    bool seen_crc = false;

    for (int guard = 0;; ++guard) {
      if (guard > 4096) {
        add(err, "tile never completed");
        return out;
      }

      // present this cycle's read response, and the ready gates
      top_.tr_data_valid_i = pend ? 1 : 0;
      top_.tr_data_i = pend_data;
      const bool tr_rdy = (stall_seed == 0) || ((next(&rng) & 3u) != 0u);
      const bool fb_rdy = (stall_seed == 0) || ((next(&rng) & 3u) != 0u);
      top_.tr_ready_i = tr_rdy ? 1 : 0;
      top_.fb_ready_i = fb_rdy ? 1 : 0;
      top_.eval();

      // the finalized CRC pulses the cycle after the last accepted pixel
      if (top_.tile_crc_valid_o) {
        if (seen_crc) add(err, "tile_crc_valid_o pulsed twice");
        if (emitted != static_cast<uint32_t>(kPixels))
          add(err, "tile_crc_valid_o before the last pixel");
        if (top_.tile_crc_index_o != index) add(err, "tile_crc_index_o mismatch");
        out.crc32c = top_.tile_crc_o;
        seen_crc = true;
      }

      // the tile-read master
      bool next_pend = false;
      uint64_t next_data = 0;
      if (top_.tr_valid_o) {
        if (top_.tr_addr_o != (next_addr & 0xFFu) || next_addr >= static_cast<uint32_t>(kPixels))
          add(err, "tile_read address out of sequence");
        if (tr_rdy) {
          next_pend = true;
          next_data = words[top_.tr_addr_o];
          ++next_addr;
        }
      }

      // the fb_tiles stream
      if (top_.fb_valid_o) {
        const uint64_t pack = (static_cast<uint64_t>(top_.fb_last_o) << 32) |
                              (static_cast<uint64_t>(top_.fb_addr_o) << 24) |
                              (static_cast<uint64_t>(top_.fb_tag_o) << 16) |
                              static_cast<uint64_t>(top_.fb_rgb565_o);
        if (held && pack != held_pack) add(err, "stalled fb beat changed while fb_ready_i was low");
        if (top_.fb_src_id_o != src) add(err, "fb_src_id_o mismatch");
        if (seen_last) add(err, "fb beat emitted after fb_last_o");
        if (fb_rdy) {
          const uint32_t a = top_.fb_addr_o;
          if (a != emitted) add(err, "fb beats are not in tile raster order");
          if (a < static_cast<uint32_t>(kPixels)) {
            if (got_pixel[a]) add(err, "pixel emitted twice");
            got_pixel[a] = true;
            out.rgb565[a] = top_.fb_rgb565_o;
            out.tag[a] = top_.fb_tag_o;
          }
          if (top_.fb_last_o != (emitted == static_cast<uint32_t>(kPixels) - 1))
            add(err, "fb_last_o is not the 256th pixel");
          if (top_.fb_last_o) seen_last = true;
          ++emitted;
          held = false;
        } else {
          held = true;
          held_pack = pack;
        }
      } else {
        if (held) add(err, "fb_valid_o dropped while a beat was stalled");
        held = false;
      }

      // one tile in flight: the block must not offer to take another until
      // the current one has handed off its CRC
      if (!seen_crc && top_.start_ready_o && guard > 0)
        add(err, "start_ready_o asserted while a tile is in flight");

      edge();
      ++cycles_;
      pend = next_pend;
      pend_data = next_data;
      if (seen_crc) break;
    }

    if (emitted != static_cast<uint32_t>(kPixels)) add(err, "tile did not emit 256 pixels");
    for (int i = 0; i < kPixels; ++i)
      if (!got_pixel[i]) add(err, "a pixel was never emitted");

    park();
    top_.eval();
    return out;
  }

  // Cycles from the start-accepting edge to the tile_crc_valid_o pulse,
  // inclusive — the contract's latency figure, measured rather than derived.
  uint32_t last_cycles() const { return cycles_; }

 private:
  static uint32_t mask12(int32_t v) { return static_cast<uint32_t>(v) & 0xFFFu; }
  static uint32_t next(uint32_t* s) {
    *s = (*s) * 747796405u + 2891336453u;
    const uint32_t w = ((*s >> ((*s >> 28) + 4)) ^ *s) * 277803737u;
    return (w >> 22) ^ w;
  }
  static void add(std::string* err, const char* what) {
    if (err->empty()) *err = what;
  }

  void park() {
    top_.start_valid_i = 0;
    top_.tr_ready_i = 1;
    top_.tr_data_valid_i = 0;
    top_.fb_ready_i = 1;
  }
  void edge() {
    top_.clk = 0;
    top_.eval();
    top_.clk = 1;
    top_.eval();
    top_.clk = 0;
    top_.eval();
  }

  Vzhao_raster_resolve top_;
  uint32_t cycles_ = 0;
};

// ---------------------------------------------------------------- helpers ---
inline uint64_t pack_px(uint8_t r, uint8_t g, uint8_t b, uint8_t tag = 0, uint32_t depth = 0,
                        uint8_t stencil = 0) {
  Word w;
  w.r = r;
  w.g = g;
  w.b = b;
  w.tag = tag;
  w.depth = depth;
  w.stencil = stencil;
  return w.pack();
}

inline std::string describe(int32_t tx, int32_t ty, const Resolved& want, const Resolved& got) {
  char buf[192];
  std::snprintf(buf, sizeof(buf), "tile origin (%d,%d)", tx, ty);
  std::string s(buf);
  int shown = 0;
  for (int i = 0; i < kPixels && shown < 8; ++i) {
    if (want.rgb565[i] == got.rgb565[i] && want.tag[i] == got.tag[i]) continue;
    std::snprintf(buf, sizeof(buf), "\n  px %3d (row %2d col %2d): oracle %04X/%02X rtl %04X/%02X",
                  i, i / kTile, i % kTile, want.rgb565[i], want.tag[i], got.rgb565[i], got.tag[i]);
    s += buf;
    ++shown;
  }
  std::snprintf(buf, sizeof(buf), "\n  crc oracle %08X rtl %08X", want.crc32c, got.crc32c);
  s += buf;
  return s;
}

inline std::vector<uint8_t> serialize(const uint64_t* words, int32_t tx, int32_t ty) {
  std::vector<uint8_t> v;
  auto put32 = [&v](uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
  };
  put32(static_cast<uint32_t>(tx));
  put32(static_cast<uint32_t>(ty));
  for (int i = 0; i < kPixels; ++i) {
    put32(static_cast<uint32_t>(words[i] & 0xFFFFFFFFu));
    put32(static_cast<uint32_t>(words[i] >> 32));
  }
  return v;
}

}  // namespace zhao_raster
