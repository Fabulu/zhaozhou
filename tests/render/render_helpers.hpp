// render_helpers.hpp — tests/render shared packet/canvas helpers (tests only,
// never installed). Every helper mirrors the generated ABI (zhao_abi.h) —
// no layout is re-derived.

#pragma once

#include "zhao_abi.h"
#include "zref/zref_frame.hpp"
#include "zref/zref_render.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace rtest {

// A minimal EARTH-profile .zprog serialized BY HAND per field-ir.md §5
// (header §5, code §5.1 word layout §1.1, io map §5.3, hash §5.4, body CRC
// §5.5) — the W3.4 field programs are not merged into this worktree yet.
// Semantics (5 instrs): height <- p0, velocity <- x, material <- age,
// nav_cost <- phase. The renderer consumes it through the ONE interpreter
// (zfield::decode/interpret) exactly like a cartridge program.
inline std::vector<uint8_t> make_earth_prog() {
  // registers: R0..R11 inputs (x,z,age,phase,p0..p7); outs 14,15,16,17.
  // §1.1 word: opcode | dst<<8 | srcA<<14 | srcB<<20 | srcC<<26, imm32 hi
  const uint32_t words[5] = {
      0x01u | (14u << 8) | (4u << 14),  // MOV height <- p0
      0x01u | (15u << 8) | (0u << 14),  // MOV velocity <- x
      0x01u | (16u << 8) | (2u << 14),  // MOV material <- age
      0x01u | (17u << 8) | (3u << 14),  // MOV nav_cost <- phase
      0x00u,                            // END
  };
  const char* names[16] = {"x",      "z",        "age",      "phase",   "p0", "p1",
                           "p2",     "p3",       "p4",       "p5",      "p6", "p7",
                           "height", "velocity", "material", "nav_cost"};
  std::vector<uint8_t> map;
  for (int i = 0; i < 16; ++i) {
    const uint8_t reg = static_cast<uint8_t>(i < 12 ? i : 14 + (i - 12));
    const uint8_t kind = static_cast<uint8_t>(i < 12 ? 0 : 1);
    const uint8_t type =
        static_cast<uint8_t>(i == 2 || i == 14 ? 3 : 0);  // age/material are u32 lanes
    map.push_back(reg);
    map.push_back(kind);
    map.push_back(type);
    map.push_back(static_cast<uint8_t>(i));  // name_id = ordinal (V6)
    map.insert(map.end(), 8, 0);             // min/max bounds (0,0)
  }
  std::vector<uint8_t> srcmap(5 * 8, 0);
  std::vector<uint8_t> namepool;
  for (int i = 0; i < 16; ++i) {
    for (const char* p = names[i]; *p; ++p) namepool.push_back(static_cast<uint8_t>(*p));
    namepool.push_back(0);
  }
  const size_t map_bytes = map.size() + srcmap.size() + namepool.size();

  std::vector<uint8_t> prog;
  auto u16 = [&](uint16_t v) {
    prog.push_back(v & 0xFF);
    prog.push_back(v >> 8);
  };
  auto u32 = [&](uint32_t v) {
    for (int i = 0; i < 4; ++i) prog.push_back(static_cast<uint8_t>(v >> (8 * i)));
  };
  u32(0x5049465Au);    // magic ZFIP
  u16(1);              // version
  prog.push_back(0);   // profile earth
  prog.push_back(0);   // flags
  u32(0);              // source_id
  u16(5);              // instr_count
  prog.push_back(0);   // table_count
  prog.push_back(16);  // io_lane_count
  u16(0);              // table_section_bytes
  u16(static_cast<uint16_t>(map_bytes));
  u32(0);  // program_hash (patched below)
  u32(0);  // body_crc32c (patched below)
  for (int i = 0; i < 5; ++i) {
    u32(words[i]);  // opcode/register half (LE)
    u32(0);         // imm32 = 0 (V9: zero where unused)
  }
  prog.insert(prog.end(), map.begin(), map.end());
  prog.insert(prog.end(), srcmap.begin(), srcmap.end());
  prog.insert(prog.end(), namepool.begin(), namepool.end());

  // §5.4 program hash: crc32c(crc32c(0, code) over tables) + instr_count
  const uint8_t* codep = prog.data() + 28;
  uint32_t h = zhao_abi::zhao_crc32c(0, codep, 5 * 8);
  h = zhao_abi::zhao_crc32c(h, codep + 5 * 8, 0);  // no tables
  h += 5;
  for (int i = 0; i < 4; ++i) prog[20 + i] = static_cast<uint8_t>(h >> (8 * i));
  // §5.5 body CRC over the image with bytes 24..27 zero (hash field still 0
  // there — the hash lives at 20..23)
  const uint32_t bc = zhao_abi::zhao_crc32c(0, prog.data(), prog.size());
  for (int i = 0; i < 4; ++i) prog[24 + i] = static_cast<uint8_t>(bc >> (8 * i));
  return prog;
}

inline std::vector<uint8_t> seal_frame(uint32_t frame_id,
                                       const std::function<void(zhao::ZhaoFrameBuilder&)>& body) {
  zhao::ZhaoFrameBuilder b;
  b.begin_frame(frame_id, 1, 0, 0);
  body(b);
  b.end_frame(0);
  return b.seal(frame_id, frame_id, 1);
}

// row-major fx16 matrix from 16 raws; ZhMat4fx has named fields, not an array
inline zhao_abi::ZhMat4fx mat(const int32_t m[16]) {
  return {m[0], m[1], m[2],  m[3],  m[4],  m[5],  m[6],  m[7],
          m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]};
}

inline zref::mat4fx to_zref(const zhao_abi::ZhMat4fx& src) {
  return {{{zref::fx16{src.m00}, zref::fx16{src.m01}, zref::fx16{src.m02}, zref::fx16{src.m03}},
           {zref::fx16{src.m10}, zref::fx16{src.m11}, zref::fx16{src.m12}, zref::fx16{src.m13}},
           {zref::fx16{src.m20}, zref::fx16{src.m21}, zref::fx16{src.m22}, zref::fx16{src.m23}},
           {zref::fx16{src.m30}, zref::fx16{src.m31}, zref::fx16{src.m32}, zref::fx16{src.m33}}}};
}

// ortho top-down "camera": screen x <- world x, screen y <- world Z, w = 1
// (world Y is the up/height axis — qformats §9 height law).
// scale is the fx16 raw multiplier (2048 = 1/32: +-8 world -> ndc +-0.25).
inline zhao_abi::ZhMat4fx ortho_topdown(int32_t scale) {
  const int32_t m[16] = {scale, 0, 0, 0, 0, 0, scale, 0, 0, 0, 1 << 16, 0, 0, 0, 0, 1 << 16};
  return mat(m);
}

// perspective-ish: x' = kx, y' = ky, w = z (the hand-computed projection
// fixture of test_render_directed case 1)
inline zhao_abi::ZhMat4fx persp2x() {
  const int32_t m[16] = {2 << 16, 0, 0, 0, 0, 2 << 16, 0, 0, 0, 0, 1 << 16, 0, 0, 0, 1 << 16, 0};
  return mat(m);
}

inline zhao_abi::ZhTransform2fx xform_identity() {
  zhao_abi::ZhTransform2fx t;
  t.tx = 0;
  t.ty = 0;
  t.r00 = 1 << 16;
  t.r01 = 0;
  t.r10 = 0;
  t.r11 = 1 << 16;
  return t;
}

// canvas pixel as RGB565 halfword (video_rules.md §3 layout)
inline uint16_t px(const zref::render::RenderCanvas& c, uint32_t slot, uint32_t x, uint32_t y,
                   uint32_t canvas_w) {
  const size_t off = static_cast<size_t>(slot) * zref::render::kSlotBytes +
                     (static_cast<size_t>(y) * canvas_w + x) * 2;
  return static_cast<uint16_t>(c.slot[slot][off] |
                               (static_cast<uint16_t>(c.slot[slot][off + 1]) << 8));
}

// a flat island-shaped patch: radial bump, w x h, envelope +-ext world m
inline zref::render::TerrainPatch bump_patch(uint16_t w, uint16_t h, int32_t ext_m,
                                             int32_t peak_m) {
  zref::render::TerrainPatch p;
  p.width = w;
  p.height = h;
  p.env_x0 = -ext_m * (1 << 16);
  p.env_z0 = -ext_m * (1 << 16);
  p.env_x1 = ext_m * (1 << 16);
  p.env_z1 = ext_m * (1 << 16);
  p.heights.assign(static_cast<size_t>(w) * h, 0);
  const double cx = (w - 1) / 2.0;  // host-side ASSET AUTHORING (fixtures
  const double cz = (h - 1) / 2.0;  // only — the RENDER path stays integer)
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      const double dx = (i - cx) / (w > 1 ? (w - 1) : 1);
      const double dz = (j - cz) / (h > 1 ? (h - 1) : 1);
      const double r2 = dx * dx + dz * dz;
      const double hgt = r2 <= 1.0 ? peak_m * (1.0 - r2) : 0.0;
      p.heights[static_cast<size_t>(j) * w + i] = static_cast<int16_t>(hgt * 256.0 + 0.5);
    }
  }
  return p;
}

}  // namespace rtest
