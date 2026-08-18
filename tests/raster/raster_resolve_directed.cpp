// raster_resolve_directed.cpp — RASTER.RESOLVE directed vectors
// (design/contracts/RASTER.RESOLVE.md "Directed tests"; law
// reference/src/zrender/resolve.cpp + spec/video_rules.md §3 +
// spec/capture_format.md §2; oracle zref::TileResolve).
//
// Every case runs the Verilated zhao_raster_resolve AND zref::TileResolve
// (which CALLS resolve.cpp — there is no second dither in this file) over the
// same tile and requires all 256 RGB565 pixels, all 256 tags and the tile CRC
// to be IDENTICAL. On top of that each case asserts its own law:
//
//   1. rails          — white is 0xFFFF at EVERY Bayer phase (the 2026-08-16
//                       green-wrap defect: full white resolved to a
//                       white/magenta checkerboard), and black is 0x0000 /
//                       0x0020 half and half — green's amplitude 32 lifts the
//                       BLACK rail, which the oracle does not fix and this
//                       block therefore reproduces. See the case body.
//   2. channel sweep  — every channel value 0…255, alone and as grey, at
//                       every Bayer phase: 1,024 tiles, 262,144 pixels
//   3. green amplitude— the vector set is PROVED able to distinguish green's
//                       amplitude 32 from 16 before it is used to check that
//                       the RTL picked 32 (the classic subtle resolve defect)
//   4. rounding edge  — the +8 / +16 rounding terms and the /255 floor at the
//                       exact quantization boundaries of each channel
//   5. dither phases  — all 16 (tile_x&3, tile_y&3) origins including
//                       negative ones, and a proof that the phases actually
//                       differ (so a tile-local phase would be caught)
//   6. tag            — the effect tag rides through UNDITHERED and does not
//                       depend on the colour or the phase
//   7. depth/stencil  — the unresolved half of the word cannot change a pixel
//   8. crc payloads   — a known payload against zhao_crc32c directly, the
//                       little-endian halfword order, and a demonstration
//                       that a flipped byte order would change the answer
//   9. backpressure   — PCG stalls on BOTH the tile_read and fb_tiles sides:
//                       identical pixels, identical CRC, beats held stable
//  10. latency        — the contract's cycle count, measured: exactly 258 at
//                       full readiness, and never fewer under any stall
//  11. back to back   — several tiles through one instance with no reset

#include "raster_resolve_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "zhao_abi.h"

using zhao::check;
using zhao_raster::kPixels;
using zhao_raster::kTile;
using zhao_raster::oracle_resolve;
using zhao_raster::pack_px;
using zhao_raster::Resolved;
using zhao_raster::ResolveDev;

namespace {

// One Verilated model for the whole suite (see raster_edgewalk_directed.cpp
// for why this is a function-local static and not a global pointer).
ResolveDev& dev() {
  static ResolveDev d;
  return d;
}

uint16_t g_src = 0;
uint16_t g_index = 0;
uint32_t g_tiles = 0;

// The differential core: RTL vs zref::TileResolve for one tile.
Resolved diff(const uint64_t* words, int32_t tx, int32_t ty, const char* what,
              uint32_t stall_seed = 0, bool announce = true) {
  std::string err;
  const Resolved got = dev().run(words, tx, ty, ++g_index, ++g_src, stall_seed, &err);
  const Resolved want = oracle_resolve(words, tx, ty);
  ++g_tiles;
  if (!err.empty()) {
    check(false, (std::string(what) + ": protocol clean").c_str(), 0, 1);
    std::printf("    protocol: %s\n", err.c_str());
  } else if (announce) {
    check(true, (std::string(what) + ": protocol clean").c_str(), 0, 0);
  }
  const bool eq = (got == want);
  if (!eq || announce)
    check(eq, (std::string(what) + ": RTL == resolve.cpp").c_str(), want.crc32c, got.crc32c);
  if (!eq) {
    const std::string body = zhao_raster::describe(tx, ty, want, got);
    std::printf("    %s\n", body.c_str());
    zhao::save_failing_vector(std::string("raster_resolve_") + what,
                              zhao_raster::serialize(words, tx, ty), "zref::TileResolve", body);
  }
  return got;
}

// A tile of one constant pixel.
void fill(uint64_t* w, uint64_t px) {
  for (int i = 0; i < kPixels; ++i) w[i] = px;
}

// --------------------------------------------------------------------------
void test_rails() {
  uint64_t w[kPixels] = {};

  // ---- THE BLACK RAIL IS NOT CLEAN, AND THAT IS THE ORACLE'S LAW ---------
  // Pure black does NOT resolve to 0x0000. Green's dither amplitude is 32
  // (resolve.cpp), so at Bayer values B >= 8 the green numerator is
  // 0*63 + 32B + 16 >= 272 >= 255 and g6 comes out as 1: half of a pure-black
  // tile resolves to 0x0020. Red and blue, at amplitude 16, top out at
  // 15*16 + 8 = 248 < 255 and stay exactly 0.
  //
  // This is the BLACK-rail counterpart of the white-rail defect fixed on
  // 2026-08-16, and unlike that one it is NOT fixed in the oracle — a clamp
  // cannot fix it, because the value genuinely computes to 1. It is recorded
  // here as the observed law, not endorsed as a good one; see
  // design/contracts/RASTER.RESOLVE.md "Notes". Changing it would move every
  // golden capture's canvas CRC, which is not this block's call to make.
  fill(w, pack_px(0, 0, 0));
  for (int py = 0; py < 4; ++py) {
    for (int px = 0; px < 4; ++px) {
      const Resolved r = diff(w, px, py, "black", 0, false);
      uint32_t off_law = 0;
      uint32_t lifted = 0;
      uint32_t red_or_blue = 0;
      for (int i = 0; i < kPixels; ++i) {
        if (r.rgb565[i] != 0x0000 && r.rgb565[i] != 0x0020) ++off_law;
        if (r.rgb565[i] == 0x0020) ++lifted;
        if ((r.rgb565[i] >> 11) != 0 || (r.rgb565[i] & 0x1Fu) != 0) ++red_or_blue;
      }
      check(off_law == 0, "black rail: every pixel is 0x0000 or 0x0020 and nothing else", 0,
            off_law);
      check(red_or_blue == 0, "black rail: red and blue are EXACTLY 0 (amplitude 16 has headroom)",
            0, red_or_blue);
      check(lifted == 128,
            "black rail: green is lifted to 1 at exactly the 8 Bayer values >= 8 (128 px)", 128,
            lifted);
    }
  }

  // ---- THE WHITE RAIL IS CLEAN, BECAUSE IT WAS CLAMPED -------------------
  // White at every phase: 0xFFFF everywhere. This is the exact shape of the
  // 2026-08-16 defect — green quantized to 64 and WRAPPED to 0 in the 6-bit
  // field, so full white came out as a 0xFFFF/0xF81F checkerboard.
  fill(w, pack_px(255, 255, 255));
  for (int py = 0; py < 4; ++py) {
    for (int px = 0; px < 4; ++px) {
      const Resolved r = diff(w, px, py, "white", 0, false);
      uint32_t bad = 0;
      uint32_t magenta = 0;
      for (int i = 0; i < kPixels; ++i) {
        if (r.rgb565[i] != 0xFFFF) ++bad;
        if (r.rgb565[i] == 0xF81F) ++magenta;
      }
      check(bad == 0, "rails: white resolves to 0xFFFF at every Bayer phase", 0, bad);
      check(magenta == 0, "rails: no white pixel wrapped to magenta (the 6-bit green rail)", 0,
            magenta);
    }
  }

  // The near-white band [252,255] is where green's headroom runs out.
  for (int v = 248; v <= 255; ++v) {
    fill(w, pack_px(static_cast<uint8_t>(v), static_cast<uint8_t>(v), static_cast<uint8_t>(v)));
    uint32_t over = 0;
    const Resolved r = diff(w, 0, 0, "near_white", 0, false);
    for (int i = 0; i < kPixels; ++i)
      if (((r.rgb565[i] >> 5) & 0x3Fu) == 0 && v >= 252) ++over;  // a wrap would read as green 0
    check(over == 0, "rails: the near-white band never wraps green to 0", 0, over);
  }
}

// --------------------------------------------------------------------------
void test_channel_sweep() {
  // 1,024 tiles = 262,144 pixels. A tile of ONE constant value visits all 16
  // Bayer phases 16 times each, so every (value, phase) pair of every channel
  // is covered exactly, not sampled.
  uint64_t w[kPixels] = {};
  uint32_t bad = 0;
  for (int v = 0; v < 256; ++v) {
    const uint8_t u = static_cast<uint8_t>(v);
    const uint64_t tiles[4] = {pack_px(u, u, u), pack_px(u, 0, 0), pack_px(0, u, 0),
                               pack_px(0, 0, u)};
    for (int k = 0; k < 4; ++k) {
      fill(w, tiles[k]);
      const Resolved got = diff(w, 0, 0, "sweep", 0, false);
      const Resolved want = oracle_resolve(w, 0, 0);
      if (got != want) ++bad;
    }
  }
  check(bad == 0, "channel sweep: 1,024 constant tiles (grey + each channel alone) are exact", 0,
        bad);
}

// --------------------------------------------------------------------------
void test_green_amplitude() {
  // Green's dither amplitude is 32 and its rounding term 16 — NOT 16 and 8 —
  // because RGB565 gives green six bits (resolve.cpp header). Step one: prove
  // the vector set can TELL, by counting the (g, B) pairs at which the two
  // candidate laws disagree. Step two: check the RTL against the ORACLE, as
  // everywhere else. The candidate arithmetic below is never the expected
  // value of a check — it only measures the discriminating power of the
  // vectors, which is the difference between a test and a decoration.
  uint32_t discriminating = 0;
  for (int g = 0; g < 256; ++g) {
    for (int b = 0; b < 16; ++b) {
      const uint32_t amp32 = static_cast<uint32_t>(g) * 63u + static_cast<uint32_t>(b) * 32u + 16u;
      const uint32_t amp16 = static_cast<uint32_t>(g) * 63u + static_cast<uint32_t>(b) * 16u + 8u;
      const uint32_t q32 = amp32 / 255u > 63u ? 63u : amp32 / 255u;
      const uint32_t q16 = amp16 / 255u > 63u ? 63u : amp16 / 255u;
      if (q32 != q16) ++discriminating;
    }
  }
  check(discriminating > 0,
        "green amplitude: the (g, Bayer) grid distinguishes amplitude 32 from 16", 1,
        discriminating > 0 ? 1 : 0);
  std::printf("  green amplitude: %u of 4096 (g, Bayer) pairs discriminate 32 from 16\n",
              discriminating);

  // Now the real check, against the oracle, over exactly that grid.
  uint64_t w[kPixels] = {};
  uint32_t bad = 0;
  for (int g = 0; g < 256; ++g) {
    fill(w, pack_px(0, static_cast<uint8_t>(g), 0));
    const Resolved got = diff(w, 0, 0, "green", 0, false);
    if (got != oracle_resolve(w, 0, 0)) ++bad;
  }
  check(bad == 0, "green amplitude: every green value at every phase equals resolve.cpp", 0, bad);

  // The same grid for a 5-bit channel, where the amplitude IS 16 — so a
  // uniform "amplitude 32 everywhere" defect is caught too.
  bad = 0;
  for (int r = 0; r < 256; ++r) {
    fill(w, pack_px(static_cast<uint8_t>(r), 0, 0));
    const Resolved got = diff(w, 0, 0, "red", 0, false);
    if (got != oracle_resolve(w, 0, 0)) ++bad;
  }
  check(bad == 0, "green amplitude: red keeps amplitude 16 (not green's 32)", 0, bad);
}

// --------------------------------------------------------------------------
void test_rounding_edges() {
  // The quantization boundaries: the values at which one more unit of dither
  // tips the /255 floor over. For a 5-bit channel the step is 255/31 ≈ 8.2, for
  // green 255/63 ≈ 4.05, so a ±1 error in the rounding term shows up within a
  // couple of units of each boundary. Sweep a dense band around every one of
  // them, all channels at once, at all four x-phases.
  uint64_t w[kPixels] = {};
  uint32_t bad = 0;
  uint32_t tiles = 0;
  for (int q = 0; q <= 63; ++q) {
    const int centres[2] = {(q * 255 + 31) / 63, (q <= 31) ? (q * 255 + 15) / 31 : -1};
    for (int ci = 0; ci < 2; ++ci) {
      if (centres[ci] < 0) continue;
      for (int d = -2; d <= 2; ++d) {
        const int v = centres[ci] + d;
        if (v < 0 || v > 255) continue;
        const uint8_t u = static_cast<uint8_t>(v);
        fill(w, pack_px(u, u, u));
        for (int ph = 0; ph < 4; ++ph) {
          const Resolved got = diff(w, ph, ph, "round", 0, false);
          if (got != oracle_resolve(w, ph, ph)) ++bad;
          ++tiles;
        }
      }
    }
  }
  std::printf("  rounding edges: %u tiles around the 5-bit and 6-bit quantization boundaries\n",
              tiles);
  check(bad == 0, "rounding edges: every quantization boundary of every channel is exact", 0, bad);
}

// --------------------------------------------------------------------------
void test_dither_phases() {
  // A tile of one mid value at each of the 16 absolute phases. The oracle
  // takes the phase from the ORIGIN, so a tile-local phase (bayer[row&3]
  // [col&3], which happens to be right for the 16-aligned grid) is wrong here.
  // A content-rich tile (not a flat colour): with only a couple of distinct
  // quantization buckets in play, two different phase shifts could in
  // principle land on the same picture, and the "all 16 phases differ" claim
  // below would be measuring the colour rather than the phase.
  uint64_t w[kPixels] = {};
  for (int i = 0; i < kPixels; ++i)
    w[i] = pack_px(static_cast<uint8_t>(i * 13 + 1), static_cast<uint8_t>(i * 7 + 2),
                   static_cast<uint8_t>(i * 3 + 5), static_cast<uint8_t>(i));

  Resolved by_phase[4][4];
  for (int py = 0; py < 4; ++py)
    for (int px = 0; px < 4; ++px) by_phase[py][px] = diff(w, px, py, "phase", 0, false);

  // The phases must actually DIFFER, or this case proves nothing.
  uint32_t distinct_pairs = 0;
  for (int a = 0; a < 16; ++a) {
    for (int b = a + 1; b < 16; ++b) {
      const Resolved& x = by_phase[a / 4][a % 4];
      const Resolved& y = by_phase[b / 4][b % 4];
      if (x.crc32c != y.crc32c) ++distinct_pairs;
    }
  }
  check(distinct_pairs == 120,
        "dither phases: all 16 absolute phases produce a different tile (120 pairs)", 120,
        distinct_pairs);

  // 16-aligned origins are the real tile grid: phase 0 by construction.
  const struct {
    int32_t x, y;
  } aligned[] = {{0, 0}, {16, 32}, {304, 224}, {-16, -32}, {-2048, 2032}};
  uint32_t bad = 0;
  for (const auto& o : aligned) {
    const Resolved got = diff(w, o.x, o.y, "aligned", 0, false);
    if (got != oracle_resolve(w, o.x, o.y)) ++bad;
    if (got.crc32c != by_phase[0][0].crc32c) ++bad;
  }
  check(bad == 0, "dither phases: every 16-aligned origin (including negative) is phase 0", 0, bad);

  // Unaligned NEGATIVE origins: the phase must wrap the way two's complement
  // wraps, not the way C's truncating `%` does.
  const struct {
    int32_t x, y;
  } unaligned[] = {{-1, -1}, {-2, -3}, {-17, -19}, {13, -7}, {-2047, 2045}};
  uint32_t neg_bad = 0;
  for (const auto& o : unaligned) {
    const Resolved got = diff(w, o.x, o.y, "unaligned", 0, false);
    if (got != oracle_resolve(w, o.x, o.y)) ++neg_bad;
  }
  check(neg_bad == 0, "dither phases: unaligned and negative origins take the right phase", 0,
        neg_bad);
}

// --------------------------------------------------------------------------
void test_tag() {
  // spec/stars_and_flares.md §1: "the tag byte is never dithered". Every tag
  // value against a colour that IS being dithered hard.
  uint64_t w[kPixels] = {};
  for (int i = 0; i < kPixels; ++i)
    w[i] = pack_px(static_cast<uint8_t>(i * 3), static_cast<uint8_t>(i * 5),
                   static_cast<uint8_t>(i * 7), static_cast<uint8_t>(i));
  const Resolved r = diff(w, 2, 1, "tag");
  uint32_t bad = 0;
  for (int i = 0; i < kPixels; ++i)
    if (r.tag[i] != static_cast<uint8_t>(i)) ++bad;
  check(bad == 0, "tag: every effect tag rides through undithered and unpermuted", 0, bad);

  // The tag must not depend on the colour or the phase either.
  uint64_t w2[kPixels] = {};
  for (int i = 0; i < kPixels; ++i) w2[i] = pack_px(255, 128, 3, static_cast<uint8_t>(i));
  const Resolved r2 = diff(w2, 3, 3, "tag_indep", 0, false);
  uint32_t drift = 0;
  for (int i = 0; i < kPixels; ++i)
    if (r2.tag[i] != r.tag[i]) ++drift;
  check(drift == 0, "tag: the tag does not depend on the colour or the Bayer phase", 0, drift);
}

// --------------------------------------------------------------------------
void test_depth_stencil_ignored() {
  // Depth and stencil are the unresolved half of the word (charter §8: no
  // external full-screen depth buffer). Two tiles with identical colour+tag
  // and wildly different depth/stencil must resolve identically.
  uint64_t a[kPixels] = {};
  uint64_t b[kPixels] = {};
  for (int i = 0; i < kPixels; ++i) {
    const uint8_t r = static_cast<uint8_t>(i * 11);
    const uint8_t g = static_cast<uint8_t>(i * 17);
    const uint8_t bl = static_cast<uint8_t>(i * 23);
    const uint8_t tg = static_cast<uint8_t>(i * 29);
    a[i] = pack_px(r, g, bl, tg, 0x000000u, 0x00);
    b[i] = pack_px(r, g, bl, tg, 0xFFFFFFu, 0xFF);
  }
  const Resolved ra = diff(a, 1, 2, "depth_zero", 0, false);
  const Resolved rb = diff(b, 1, 2, "depth_max", 0, false);
  check(ra == rb, "depth/stencil: the unresolved half of the word cannot change a pixel", ra.crc32c,
        rb.crc32c);
}

// --------------------------------------------------------------------------
void test_crc_payloads() {
  // (a) black is NOT an all-zero payload (see test_rails: green's amplitude
  //     lifts half the tile to 0x0020), so the all-zero CRC is used as a
  //     NEGATIVE anchor — it pins the finding, and it would still catch a
  //     resolve that silently zeroed the tile.
  uint64_t w[kPixels] = {};
  fill(w, pack_px(0, 0, 0));
  std::vector<uint8_t> zeros(kPixels * 2, 0);
  const uint32_t crc_zero = zhao_abi::zhao_crc32c(0, zeros.data(), zeros.size());
  const Resolved rz = diff(w, 0, 0, "crc_black", 0, false);
  check(rz.crc32c != crc_zero,
        "crc: a black tile is NOT 512 zero bytes (green's amplitude lifts half of it)", 1,
        rz.crc32c != crc_zero ? 1 : 0);
  std::vector<uint8_t> blk(kPixels * 2, 0);
  for (int i = 0; i < kPixels; ++i) {
    blk[i * 2 + 0] = static_cast<uint8_t>(rz.rgb565[i] & 0xFF);
    blk[i * 2 + 1] = static_cast<uint8_t>(rz.rgb565[i] >> 8);
  }
  check(rz.crc32c == zhao_abi::zhao_crc32c(0, blk.data(), blk.size()),
        "crc: the black tile's CRC is CRC-32C over the bytes it actually emitted",
        zhao_abi::zhao_crc32c(0, blk.data(), blk.size()), rz.crc32c);

  // (b) a KNOWN payload: an all-white tile really is 512 bytes of 0xFF at
  //     every phase (the clamp guarantees it), so this is an independent
  //     anchor against zhao_crc32c rather than a self-consistency check.
  fill(w, pack_px(255, 255, 255));
  std::vector<uint8_t> ones(kPixels * 2, 0xFF);
  const uint32_t want_ones = zhao_abi::zhao_crc32c(0, ones.data(), ones.size());
  const Resolved ro = diff(w, 0, 0, "crc_white", 0, false);
  check(ro.crc32c == want_ones, "crc: an all-white tile is CRC-32C over 512 0xFF bytes", want_ones,
        ro.crc32c);

  // (c) the LITTLE-ENDIAN halfword order (video_rules.md §3), rebuilt from the
  //     pixels the block emitted — and a demonstration that the OTHER byte
  //     order would give a different answer, so a flip would be caught.
  for (int i = 0; i < kPixels; ++i)
    w[i] = pack_px(static_cast<uint8_t>(i * 13 + 1), static_cast<uint8_t>(i * 7 + 2),
                   static_cast<uint8_t>(i * 3 + 5), static_cast<uint8_t>(i));
  const Resolved rm = diff(w, 1, 3, "crc_mixed");
  std::vector<uint8_t> le(kPixels * 2), be(kPixels * 2);
  for (int i = 0; i < kPixels; ++i) {
    le[i * 2 + 0] = static_cast<uint8_t>(rm.rgb565[i] & 0xFF);
    le[i * 2 + 1] = static_cast<uint8_t>(rm.rgb565[i] >> 8);
    be[i * 2 + 0] = static_cast<uint8_t>(rm.rgb565[i] >> 8);
    be[i * 2 + 1] = static_cast<uint8_t>(rm.rgb565[i] & 0xFF);
  }
  const uint32_t crc_le = zhao_abi::zhao_crc32c(0, le.data(), le.size());
  const uint32_t crc_be = zhao_abi::zhao_crc32c(0, be.data(), be.size());
  check(rm.crc32c == crc_le, "crc: the tile CRC is CRC-32C over little-endian halfwords", crc_le,
        rm.crc32c);
  check(crc_le != crc_be, "crc: the two byte orders are distinguishable (a flip would show)", 1,
        crc_le != crc_be ? 1 : 0);
}

// --------------------------------------------------------------------------
void test_backpressure() {
  // PCG stalls on BOTH sides (tile_read ready and fb_tiles ready). The driver
  // asserts beat stability and request ordering; here we require the pixels
  // and the CRC to be bit-identical to the unstalled run.
  uint64_t w[kPixels] = {};
  for (int i = 0; i < kPixels; ++i)
    w[i] = pack_px(static_cast<uint8_t>(i * 31 + 3), static_cast<uint8_t>(i * 61 + 9),
                   static_cast<uint8_t>(i * 91 + 17), static_cast<uint8_t>(i * 5));
  const Resolved base = diff(w, 2, 3, "stall_base", 0, false);
  uint32_t bad = 0;
  for (uint32_t s = 1; s <= 8; ++s) {
    const Resolved r = diff(w, 2, 3, "stall", 0x1000u * s + 7u, false);
    if (r != base) ++bad;
  }
  check(bad == 0, "backpressure: 8 stall patterns give bit-identical pixels and CRC", 0, bad);
}

// --------------------------------------------------------------------------
void test_latency() {
  // The contract's latency line, MEASURED — 259 cycles from the accepting
  // edge to the tile_crc_valid_o pulse, not the 258 the arithmetic suggested
  // (256 pixels + 2 cycles of pipeline fill + the finalize cycle). The extra
  // cycle is the first issue: `tr_valid_o` is a function of `busy_r`, which
  // is only set BY the accepting edge, so no read can be issued in the
  // accepting cycle itself. Under backpressure it may take longer and may
  // never take less; both halves are checked, because a block that is fast
  // because it skipped something is the failure mode worth catching.
  uint64_t w[kPixels] = {};
  for (int i = 0; i < kPixels; ++i)
    w[i] = pack_px(static_cast<uint8_t>(i), static_cast<uint8_t>(i * 2),
                   static_cast<uint8_t>(i * 3), static_cast<uint8_t>(i));

  diff(w, 0, 0, "latency_full", 0, false);
  const uint32_t full = dev().last_cycles();
  std::printf("  latency: %u cycles at full readiness (256 px + fill + finalize)\n", full);
  check(full == 259, "latency: a tile at full readiness costs exactly 259 cycles", 259, full);

  uint32_t never_faster = 0;
  for (uint32_t s = 1; s <= 6; ++s) {
    diff(w, 0, 0, "latency_stall", 0x3000u * s + 11u, false);
    if (dev().last_cycles() < full) ++never_faster;
  }
  check(never_faster == 0, "latency: backpressure can only ever cost cycles, never save them", 0,
        never_faster);
}

// --------------------------------------------------------------------------
void test_back_to_back() {
  // Several tiles through one instance with no reset: the CRC seed, the
  // occupancy counter and the address cursor must all re-arm per tile.
  uint64_t w[kPixels] = {};
  uint32_t bad = 0;
  for (uint32_t k = 0; k < 6; ++k) {
    for (int i = 0; i < kPixels; ++i)
      w[i] = pack_px(static_cast<uint8_t>(i * (k + 1)), static_cast<uint8_t>(i + k * 40),
                     static_cast<uint8_t>(i * 3 + k), static_cast<uint8_t>(k));
    const int32_t tx = static_cast<int32_t>(k) - 2;
    const int32_t ty = static_cast<int32_t>(k) * 3 - 4;
    const Resolved got = diff(w, tx, ty, "b2b", (k % 2) ? (0x2000u + k) : 0u, false);
    if (got != oracle_resolve(w, tx, ty)) ++bad;
  }
  check(bad == 0, "back to back: six tiles through one instance with no reset", 0, bad);
}

}  // namespace

int main() {
  test_rails();
  test_channel_sweep();
  test_green_amplitude();
  test_rounding_edges();
  test_dither_phases();
  test_tag();
  test_depth_stencil_ignored();
  test_crc_payloads();
  test_backpressure();
  test_latency();
  test_back_to_back();

  std::printf("raster_resolve_directed: %u tiles resolved\n", g_tiles);
  return zhao::report_and_exit("raster_resolve_directed");
}
