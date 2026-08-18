// zref_tilestore.hpp — RASTER.TILESTORE reference model (phase 4, ZH-021).
//
// The scalar oracle named by design/blocks.yml (`reference_model:
// zref::TileStore`) and design/contracts/RASTER.TILESTORE.md.
//
// A memory has no pre-existing law to call the way zref::EdgeWalk calls
// zref::render::raster_tri, so this IS a second implementation — of the
// contract, deliberately written in the plainest possible way (two arrays and
// a present bit), so that "RTL == oracle" tests the RTL's ping-pong, clear
// and bypass mechanics rather than a shared clever idea. What it is NOT is a
// second definition of anything: the tile WORD layout below is the one
// zhao_raster_tilestore.sv defines, quoted, and zref::TileResolve reads this
// same struct rather than re-deriving field positions.
//
// CYCLE MODEL. `step()` takes one cycle's port stimulus, applies one clock
// edge, and returns what the RTL presents. Because the read ports have fixed
// 1-cycle latency (ledger `latency: fixed:1`), the returned `rd_*` / `res_*`
// are what the RTL drives during the cycle AFTER the stimulus, while
// `wr_ready` is combinational in the stimulus cycle itself and `front_bank` /
// `tile_references` are the post-edge values. The test driver lines these up
// against the Verilated model beat for beat.

#pragma once

#include <cstdint>

namespace zref {

/** The 16x16 x 64bpp ping-pong tile store. */
struct TileStore {
  /** Tile edge in pixels (charter phase 4: "16x16 colour/Z/stencil tile"). */
  static constexpr int kTile = 16;
  /** Words per tile: one 64-bit word per pixel (charter 8, 2 KiB a tile). */
  static constexpr int kWords = kTile * kTile;

  /**
   * The charter 8 "Active tile storage" word, unpacked. Bit positions are
   * fixed by fpga/rtl/raster/zhao_raster_tilestore.sv (charter order, MSB
   * first) and stated once in design/contracts/RASTER.TILESTORE.md:
   *   [63:40] 24-bit RGB colour, R [63:56] G [55:48] B [47:40]
   *   [39:32]  8-bit effect tag/strength
   *   [31: 8] 24-bit inverse-W depth
   *   [ 7: 0]  8-bit stencil
   */
  struct Word {
    uint8_t r = 0, g = 0, b = 0;
    uint8_t tag = 0;
    uint32_t depth = 0;  // 24-bit
    uint8_t stencil = 0;

    uint64_t pack() const {
      return (static_cast<uint64_t>(r) << 56) | (static_cast<uint64_t>(g) << 48) |
             (static_cast<uint64_t>(b) << 40) | (static_cast<uint64_t>(tag) << 32) |
             (static_cast<uint64_t>(depth & 0xFFFFFFu) << 8) | static_cast<uint64_t>(stencil);
    }
    static Word unpack(uint64_t w) {
      Word o;
      o.r = static_cast<uint8_t>(w >> 56);
      o.g = static_cast<uint8_t>(w >> 48);
      o.b = static_cast<uint8_t>(w >> 40);
      o.tag = static_cast<uint8_t>(w >> 32);
      o.depth = static_cast<uint32_t>((w >> 8) & 0xFFFFFFu);
      o.stencil = static_cast<uint8_t>(w);
      return o;
    }
  };

  /** One cycle of port stimulus — one field group per RTL channel. */
  struct Cycle {
    bool clear = false;
    uint64_t clear_data = 0;
    bool wr = false;
    uint8_t wr_addr = 0;
    uint64_t wr_data = 0;
    bool rd = false;
    uint8_t rd_addr = 0;
    uint16_t rd_src_id = 0;
    bool res = false;
    uint8_t res_addr = 0;
    bool swap = false;
  };

  /** What the RTL drives (see the CYCLE MODEL note above). */
  struct Out {
    bool wr_ready = true;  // combinational, in the stimulus cycle
    bool rd_valid = false;
    uint64_t rd_data = 0;
    uint16_t rd_src_id = 0;
    bool res_valid = false;
    uint64_t res_data = 0;
    bool front_bank = false;
    uint32_t tile_references = 0;
  };

  TileStore() { reset(); }

  void reset();

  /** Apply one cycle of stimulus and one clock edge. */
  Out step(const Cycle& c);

  /** Peek the effective value of a word in a bank (test convenience only). */
  uint64_t peek(int bank, uint8_t addr) const {
    return present_[bank][addr] ? ram_[bank][addr] : clear_[bank];
  }
  int front() const { return front_; }

 private:
  uint64_t ram_[2][kWords] = {};
  bool present_[2][kWords] = {};
  uint64_t clear_[2] = {};
  int front_ = 0;
  uint32_t refs_ = 0;
};

}  // namespace zref
