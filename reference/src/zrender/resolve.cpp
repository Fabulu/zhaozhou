// resolve.cpp — charter §8 pass 9: the RGB565 resolve (ordered dither) and
// the two canvas CRC laws.
//
// Law:
//   charter §8            "high-quality ordered dithering on resolve"; the
//                         working tile is 24-bit RGB, the framebuffer RGB565
//   spec/video_rules.md §3 framebuffer layout: RGB565 [15:11]R[10:5]G[4:0]B,
//                         little-endian halfwords, row-major, no row padding
//                         §3.1 Duo packed two-block layout: view 0 at slot
//                         bytes [0,0x18000), view 1 at [0x18000,0x30000),
//                         each contiguous (ratified 2026-08-15, MAJOR-3)
//                         §4 displayed-CRC law: CRC-32C over exactly
//                         2 x active_width x 240 bytes in raster order AFTER
//                         the repeat decision — Duo border rows (black §3.1)
//                         are part of the displayed stream, and each Duo
//                         displayed row is assembled at scanout from the two
//                         view blocks
//   plan W3.5             the 4x4 Bayer matrix: fixgen has NO dither table
//                         today (checked at W3.5 start), so the canonical
//                         4x4 Bayer thresholds (B+0.5)/16 are defined HERE,
//                         once. When Phase 4 freezes the RTL resolve matrix,
//                         this is the single regeneration point.
//
// Channel quantization law (deterministic floor division — the resolve is a
// formatting step, not an fx lane; no SatLedger involvement):
//   r5 = (r*31 + (B*16 + 8)) / 255      g6 = (g*63 + (B*32 + 16)) / 255
//   b5 = (b*31 + (B*16 + 8)) / 255
// i.e. the dither threshold t = (B+0.5)/16 of one quantization step. Worst
// case r=255: (255*31+248)/255 = 31 — exact, no clamp needed.

#include "internal.hpp"

namespace zref {
namespace render {
namespace {
inline constexpr uint8_t kBayer4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};
}  // namespace

void resolve_rgb565(const uint8_t* rgb888, uint32_t width, uint32_t height, uint8_t* out565) {
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const size_t i = static_cast<size_t>(y) * width + x;
      const uint8_t b = kBayer4[y & 3][x & 3];
      const uint8_t r = rgb888[i * 3 + 0];
      const uint8_t g = rgb888[i * 3 + 1];
      const uint8_t bl = rgb888[i * 3 + 2];
      const uint32_t r5 = (static_cast<uint32_t>(r) * 31 + b * 16 + 8) / 255;
      const uint32_t g6 = (static_cast<uint32_t>(g) * 63 + b * 32 + 16) / 255;
      const uint32_t b5 = (static_cast<uint32_t>(bl) * 31 + b * 16 + 8) / 255;
      const uint16_t px = static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
      out565[i * 2 + 0] = static_cast<uint8_t>(px & 0xFF);
      out565[i * 2 + 1] = static_cast<uint8_t>(px >> 8);
    }
  }
}

uint32_t canvas_crc32c(zhao_abi::video_mode mode, const uint8_t* slot) {
  return zhao_abi::zhao_crc32c(0, slot, canvas_bytes(mode));
}

uint32_t displayed_crc32c(zhao_abi::video_mode mode, const uint8_t* slot) {
  if (mode != zhao_abi::VIDEO_DUO) {
    // single-view modes: the displayed stream IS the canvas (§4)
    return zhao_abi::zhao_crc32c(0, slot, canvas_bytes(mode));
  }
  // Duo (§3.1/§4): the displayed stream is 240 rows of 512 px AS SCANNED OUT.
  // Rows 0..23 and 216..239 are the black border (16'h0000, never stored);
  // each row 24..215 is view-0's row CONCATENATED with view-1's row, and the
  // two views live in separate contiguous blocks (view 0 at [0,0x18000),
  // view 1 at [0x18000,0x30000)) — so this walks two cursors, it does NOT
  // stream the stored bytes linearly. Ratified 2026-08-15 (review MAJOR-3).
  constexpr size_t kViewRowBytes = 256 * 2;
  static const uint8_t border_row[512 * 2] = {};  // black (16'h0000)
  uint32_t crc = 0;
  for (int i = 0; i < 24; ++i) crc = zhao_abi::zhao_crc32c(crc, border_row, sizeof(border_row));
  for (uint32_t row = 0; row < 192; ++row) {
    const size_t off = static_cast<size_t>(row) * kViewRowBytes;
    crc = zhao_abi::zhao_crc32c(crc, slot + off, kViewRowBytes);                  // view 0
    crc = zhao_abi::zhao_crc32c(crc, slot + kDuoViewBytes + off, kViewRowBytes);  // view 1
  }
  for (int i = 0; i < 24; ++i) crc = zhao_abi::zhao_crc32c(crc, border_row, sizeof(border_row));
  return crc;
}

}  // namespace render
}  // namespace zref
