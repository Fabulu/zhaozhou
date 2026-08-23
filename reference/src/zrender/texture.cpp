// texture.cpp — zref::TextureCache and zref::Tmu (TEXTURE.CACHE ZH-061,
// TEXTURE.TMU ZH-027). Rationale, delegation list and the derivation of the
// bilinear law live in reference/include/zref/zref_texture.hpp and
// fpga/rtl/texture/zhao_texture_bilerp.sv.

#include "zref/zref_texture.hpp"

#include "zref/zref_sky.hpp"

namespace zref {
namespace {

/**
 * Arithmetic (floor) right shift on an int64. C++17 leaves `>>` on a negative
 * value implementation-defined, and the whole point of the UV lane is that a
 * negative coordinate wraps EXACTLY as the RTL's two's-complement shift does,
 * so the floor is spelled out rather than assumed.
 */
int64_t asr64(int64_t x, int k) {
  if (x >= 0) return x >> k;
  const int64_t step = int64_t{1} << k;
  return -((-x + step - 1) / step);
}

/** Bits [hi:lo] of a two's-complement value, read as the RTL reads them. */
uint8_t byte_of(int64_t x, int lo) {
  return static_cast<uint8_t>((static_cast<uint64_t>(x) >> lo) & 0xFFu);
}

/** 5-bit and 4-bit replication expansions (see the TMU header's format decode). */
uint8_t exp5(uint8_t c) { return static_cast<uint8_t>((c << 3) | (c >> 2)); }
uint8_t exp4(uint8_t c) { return static_cast<uint8_t>((c << 4) | c); }

}  // namespace

// ============================================================ TextureMemory ==

uint8_t TextureMemory::byte_at(uint32_t addr) const {
  if (addr < base) return 0;
  const uint32_t off = addr - base;
  if (off >= bytes.size()) return 0;
  return bytes[off];
}

uint16_t TextureMemory::halfword(uint32_t addr) const {
  const uint32_t a = addr & ~1u;
  return static_cast<uint16_t>(byte_at(a) | (static_cast<uint16_t>(byte_at(a + 1)) << 8));
}

// ============================================================= TextureCache ==

void TextureCache::reset() {
  for (int k = 0; k < kLanes; ++k) {
    for (int i = 0; i < kLines; ++i) {
      valid_[k][i] = false;
      tag_[k][i] = 0;
      for (int b = 0; b < kLineBytes; ++b) data_[k][i][b] = 0;
    }
  }
  hits_ = 0;
  misses_ = 0;
}

void TextureCache::invalidate_all() {
  for (int k = 0; k < kLanes; ++k)
    for (int i = 0; i < kLines; ++i) valid_[k][i] = false;
}

void TextureCache::invalidate_line(uint32_t addr) {
  const int idx = static_cast<int>((addr / kLineBytes) % kLines);
  for (int k = 0; k < kLanes; ++k) valid_[k][idx] = false;
}

bool TextureCache::resident(int lane, uint32_t addr) const {
  const int idx = static_cast<int>((addr / kLineBytes) % kLines);
  const uint32_t tag = addr / (kLineBytes * kLines);
  return valid_[lane][idx] && tag_[lane][idx] == tag;
}

TextureCache::Out TextureCache::access(const Access& a, const TextureMemory& mem) {
  Out o;
  // The FIRST look decides the counters (see the RTL's THE COUNTERS COUNT THE
  // FIRST LOOK): counting at acceptance would make the hit rate identically 1.
  for (int k = 0; k < kLanes; ++k) {
    if (a.en[k] && resident(k, a.addr[k])) ++o.first_hits;
  }
  hits_ += o.first_hits;

  // Fills, lowest lane first -- a deterministic order, so the VRAM request
  // sequence does not depend on backpressure timing.
  for (int k = 0; k < kLanes; ++k) {
    if (!a.en[k] || resident(k, a.addr[k])) continue;
    const uint32_t line = a.addr[k] & ~static_cast<uint32_t>(kLineBytes - 1);
    const int idx = static_cast<int>((a.addr[k] / kLineBytes) % kLines);
    o.fill_addr[o.fills] = line;
    ++o.fills;
    ++misses_;
    for (int b = 0; b < kLineBytes; ++b)
      data_[k][idx][b] = mem.byte_at(line + static_cast<uint32_t>(b));
    tag_[k][idx] = a.addr[k] / (kLineBytes * kLines);
    valid_[k][idx] = true;
  }

  // Every enabled lane is resident now; the data comes out of the ARRAY and
  // never straight from the fill (the RTL has no bypass either).
  for (int k = 0; k < kLanes; ++k) {
    if (!a.en[k]) continue;
    const int idx = static_cast<int>((a.addr[k] / kLineBytes) % kLines);
    const int off = static_cast<int>(a.addr[k] % kLineBytes) & ~1;
    o.data[k] = static_cast<uint16_t>(data_[k][idx][off] |
                                      (static_cast<uint16_t>(data_[k][idx][off + 1]) << 8));
  }
  return o;
}

// ======================================================================= Tmu ==

uint32_t Tmu::Mode::pack() const {
  return (static_cast<uint32_t>(fmt & 7u)) | (static_cast<uint32_t>(bilinear) << 3) |
         (static_cast<uint32_t>(wrap_u & 3u) << 4) | (static_cast<uint32_t>(wrap_v & 3u) << 6) |
         (static_cast<uint32_t>(log2w & 15u) << 8) | (static_cast<uint32_t>(log2h & 15u) << 12) |
         (static_cast<uint32_t>(max_level & 15u) << 16) | (static_cast<uint32_t>(mip_en) << 20) |
         (static_cast<uint32_t>(rsvd & 0x7FFu) << 21);
}

Tmu::Mode Tmu::Mode::unpack(uint32_t w) {
  Mode m;
  m.fmt = static_cast<uint8_t>(w & 7u);
  m.bilinear = ((w >> 3) & 1u) != 0u;
  m.wrap_u = static_cast<uint8_t>((w >> 4) & 3u);
  m.wrap_v = static_cast<uint8_t>((w >> 6) & 3u);
  m.log2w = static_cast<uint8_t>((w >> 8) & 15u);
  m.log2h = static_cast<uint8_t>((w >> 12) & 15u);
  m.max_level = static_cast<uint8_t>((w >> 16) & 15u);
  m.mip_en = ((w >> 20) & 1u) != 0u;
  m.rsvd = static_cast<uint16_t>((w >> 21) & 0x7FFu);
  return m;
}

uint32_t Tmu::wrap(int32_t t, uint8_t mode, uint32_t mask) {
  const uint32_t tu = static_cast<uint32_t>(t);
  if (mode == kClamp) {
    if (t < 0) return 0;
    return tu > mask ? mask : tu;
  }
  if (mode == kMirror) {
    const uint32_t per = tu & ((mask << 1) | 1u);
    const uint32_t lo = per & mask;
    return per > mask ? (mask - lo) : lo;
  }
  return tu & mask;  // kRepeat, and the reserved code 3
}

uint32_t Tmu::level_offset_texels(uint8_t log2w, uint8_t log2h, uint8_t level) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < level; ++i) {
    sum += (1u << (log2w - i)) * (1u << (log2h - i));
  }
  return sum;
}

uint8_t Tmu::bilerp(uint8_t t00, uint8_t t10, uint8_t t01, uint8_t t11, uint8_t fu, uint8_t fv) {
  const uint32_t iu = 256u - fu;
  const uint32_t iv = 256u - fv;
  const uint32_t acc = static_cast<uint32_t>(t00) * (iu * iv) +
                       static_cast<uint32_t>(t10) * (fu * iv) +
                       static_cast<uint32_t>(t01) * (iu * fv) +
                       static_cast<uint32_t>(t11) * (static_cast<uint32_t>(fu) * fv);
  return static_cast<uint8_t>((acc + 32768u) >> 16);
}

Tmu::Plan Tmu::plan(const Req& r) {
  const Mode m = Mode::unpack(r.mode);
  Plan p;

  const bool clut = (m.fmt == kClut8) || (m.fmt == kClut4);
  const bool is16 = (m.fmt == kRgb565) || (m.fmt == kArgb1555) || (m.fmt == kArgb4444);
  const bool fmt_bad = !clut && !is16;

  const uint8_t chain = m.log2w < m.log2h ? m.log2w : m.log2h;
  const bool chain_bad = m.max_level > chain;
  const uint8_t cap = chain_bad ? chain : m.max_level;

  p.clut = clut;
  // spec/stars_and_flares.md 1, enforced rather than trusted: a palette is
  // never filtered. The request still produces a sample -- nearest -- and the
  // error is reported.
  p.bilinear = m.bilinear && !clut;
  p.mode_error = (m.bilinear && clut) || (m.rsvd != 0) || chain_bad || fmt_bad;
  p.lanes = p.bilinear ? 4 : 1;

  const uint8_t lvl_req = m.mip_en ? static_cast<uint8_t>(r.lod >> 4) : 0;
  p.level = lvl_req > cap ? cap : lvl_req;

  const uint8_t lw = static_cast<uint8_t>(m.log2w - p.level);
  const uint8_t lh = static_cast<uint8_t>(m.log2h - p.level);
  const uint32_t mask_u = (1u << lw) - 1u;
  const uint32_t mask_v = (1u << lh) - 1u;
  const uint32_t lvl_off = level_offset_texels(m.log2w, m.log2h, p.level);

  // texel_q16 = u_raw << log2s -- an exact shift; a multiply, because
  // shifting a negative left is undefined in C++17 and this lane must be
  // exactly the RTL's two's-complement shift on negative coordinates.
  int64_t tu = static_cast<int64_t>(r.u) * (int64_t{1} << lw);
  int64_t tv = static_cast<int64_t>(r.v) * (int64_t{1} << lh);
  if (p.bilinear) {
    // The half-texel bias: a texel owns [k, k+1) under the frozen 6.2 floor,
    // so its CENTRE is k + 1/2 and bilinear samples about it. This is what
    // makes the two filters agree at texel centres.
    tu -= 32768;
    tv -= 32768;
  }
  const int32_t iu0 = static_cast<int32_t>(asr64(tu, 16));
  const int32_t iv0 = static_cast<int32_t>(asr64(tv, 16));
  p.fu = p.bilinear ? byte_of(tu, 8) : 0;
  p.fv = p.bilinear ? byte_of(tv, 8) : 0;

  for (int k = 0; k < 4; ++k) {
    const int32_t iu = iu0 + (k & 1);
    const int32_t iv = iv0 + ((k >> 1) & 1);
    const uint32_t wu = wrap(iu, m.wrap_u, mask_u);
    const uint32_t wv = wrap(iv, m.wrap_v, mask_v);
    // ROW-MAJOR, matching the only concrete texture layout in this repository
    // (zref::render::Tileset, zref_render.hpp:166); charter 15's Morton bullet has no
    // ratified formula anywhere. Recorded as a choice in the contract.
    const uint32_t total = lvl_off + (wv << lw) + wu;
    if (is16) {
      p.addr[k] = r.base + (total << 1);
    } else if (m.fmt == kClut4) {
      p.addr[k] = r.base + (total >> 1);
    } else {
      p.addr[k] = r.base + total;
    }
    if (k == 0) {
      p.nibble = (total & 1u) != 0u;
      p.byte_sel = (p.addr[0] & 1u) != 0u;
    }
  }
  return p;
}

Tmu::Sample Tmu::sample(const Req& r, const TextureMemory& mem) {
  const Plan p = plan(r);
  const Mode m = Mode::unpack(r.mode);
  Sample s;
  s.mode_error = p.mode_error;
  s.src_id = r.src_id;

  uint16_t hw[4] = {};
  for (int k = 0; k < p.lanes; ++k) hw[k] = mem.halfword(p.addr[k]);

  if (p.clut) {
    const uint8_t byte =
        p.byte_sel ? static_cast<uint8_t>(hw[0] >> 8) : static_cast<uint8_t>(hw[0] & 0xFFu);
    s.idx = (m.fmt == kClut4)
                ? (p.nibble ? static_cast<uint8_t>(byte >> 4) : static_cast<uint8_t>(byte & 0x0Fu))
                : byte;
    const uint16_t entry = mem.halfword(r.pal_base + (static_cast<uint32_t>(s.idx) << 1));
    sky::rgb565 c;
    c.bits = entry;
    c.to_rgb888(s.r, s.g, s.b);
    // A CLUT texel's transparency is a property of its INDEX (stars 1, and
    // RASTER.FRAGMENT's THE ALPHA TEST IS AN INDEX TEST), never of an alpha
    // the RGB565 palette entry has no room for.
    s.a = 255;
    return s;
  }

  // Direct colour: decode all four taps, then one bilerp per channel. With
  // nearest the fractions are zero, w00 is 65,536 and the filter is the exact
  // identity on tap 0 -- one datapath, which is the charter 26 shape.
  uint8_t cr[4] = {};
  uint8_t cg[4] = {};
  uint8_t cb[4] = {};
  uint8_t ca[4] = {};
  const uint8_t fmt = (m.fmt == kArgb1555 || m.fmt == kArgb4444) ? m.fmt : kRgb565;
  for (int k = 0; k < 4; ++k) {
    const uint16_t h = hw[k];
    if (fmt == kArgb1555) {
      ca[k] = (h & 0x8000u) ? 255 : 0;
      cr[k] = exp5(static_cast<uint8_t>((h >> 10) & 0x1Fu));
      cg[k] = exp5(static_cast<uint8_t>((h >> 5) & 0x1Fu));
      cb[k] = exp5(static_cast<uint8_t>(h & 0x1Fu));
    } else if (fmt == kArgb4444) {
      ca[k] = exp4(static_cast<uint8_t>((h >> 12) & 0x0Fu));
      cr[k] = exp4(static_cast<uint8_t>((h >> 8) & 0x0Fu));
      cg[k] = exp4(static_cast<uint8_t>((h >> 4) & 0x0Fu));
      cb[k] = exp4(static_cast<uint8_t>(h & 0x0Fu));
    } else {
      sky::rgb565 c;
      c.bits = h;
      c.to_rgb888(cr[k], cg[k], cb[k]);
      ca[k] = 255;
    }
  }
  s.r = bilerp(cr[0], cr[1], cr[2], cr[3], p.fu, p.fv);
  s.g = bilerp(cg[0], cg[1], cg[2], cg[3], p.fu, p.fv);
  s.b = bilerp(cb[0], cb[1], cb[2], cb[3], p.fu, p.fv);
  s.a = bilerp(ca[0], ca[1], ca[2], ca[3], p.fu, p.fv);
  // A direct-colour texel has no index; the two RASTER.FRAGMENT state bits
  // that read one are set by no direct-colour recipe.
  s.idx = 0;
  return s;
}

}  // namespace zref
