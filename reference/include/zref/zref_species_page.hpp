// zref_species_page.hpp -- the SPECIES_TABLE page, frozen. Owner ruling R42
// (reports/OWNER-RULINGS-20260919-EVENING.md, 2026-09-19): "A SPECIES_TABLE
// page kind, published through PublishResource/MEM.UPLOAD (R17/R32), plus a
// loader into PART.TABLE. The byte layout is frozen by the packet with a zref
// model."
//
// This header IS that layout, and `zhao_part_table_loader`
// (fpga/rtl/particles/zhao_part_table_loader.sv) reads exactly it.
// tests/particles/part_table_loader_directed.cpp differences the two.
//
// ---------------------------------------------------------------------------
// WHAT THIS PAGE IS NOT
// ---------------------------------------------------------------------------
// It is NOT a definition of what a species is. `zref_particle.hpp` is explicit
// that "there is no species table ... That is a DATA/ABI question and it is
// properly the owner's", and core entry I33 refused to invent one for exactly
// that reason. What is frozen here is the ENVELOPE: how a load word that
// PART.TABLE already accepts is carried in a page. The CONTENTS of each word
// remain the owner's, authored by whoever writes the page, and this file makes
// no claim about a single field inside `data`.
//
// The word itself is `zhao_part_table.sv`'s load port, unchanged and
// unreinterpreted: {sel, index, event, data}. A page is a sequence of the
// writes a host would otherwise make one at a time.
//
// ---------------------------------------------------------------------------
// THE LAYOUT, AND WHY EVERY NUMBER IN IT IS 64-BYTE SHAPED
// ---------------------------------------------------------------------------
// MEM.GUARD's read is at most 64 bytes and its shape rule requires the byte
// mask to match the length, so the cheapest honest reader is one that reads
// whole 64-byte lines. So:
//
//   HEADER -- 64 bytes, one line, at the page's base
//     u32 magic     'ZSPT' (0x5450'535A little-endian on the wire)
//     u16 version   1
//     u16 entries   how many entries follow
//     u8  rsv[56]   zero
//
//   ENTRY -- 32 bytes, TWO per line, starting at byte 64
//     bit   1:0   sel     0 UPD, 1 COL, 2 SPW, 3 CRV (zhao_part_table.sv)
//     bit   3:2   event   SPW only: 0 birth, 1 mark, 2 collision, 3 death
//     bit   7:4   rsv     zero
//     bit  14:8   index   species, or curve bucket in its low four bits
//     bit  31:15  rsv     zero
//     bit 172:32  data    the LD_W-wide load word, LSB-aligned
//     bit 255:173 rsv     zero
//
// An entry is 32 bytes rather than the 19 the fields need, so that two fit a
// line exactly and the reader never has an entry straddling a read. The cost
// is 13 bytes per species descriptor in a page that is uploaded once.
//
// A page whose magic or version is wrong is REFUSED WHOLE and counted -- never
// partially loaded. A half-loaded species table is a particle engine running
// on a mixture of two authors' physics, which is the kind of wrong that looks
// like a tuning problem.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace zref {
namespace species_page {

/** `spec/cartridge.md` 4's page-kind registry. Allocated by R42. */
inline constexpr uint8_t kPageKind = 13;

inline constexpr uint32_t kMagic = 0x5450'535Au;  // 'Z','S','P','T' little-endian
inline constexpr uint16_t kVersion = 1;
inline constexpr size_t kHeaderBytes = 64;
inline constexpr size_t kEntryBytes = 32;

/** `zhao_part_table.sv`'s load word, at the console's widths. */
inline constexpr int kAgeW = 10;
inline constexpr int kVelW = 11;
inline constexpr int kPosW = 18;
inline constexpr int kLdW = 12 + (2 * kAgeW) + (5 * kVelW) + (3 * kPosW);  // 141

inline constexpr int kSelBit = 0;
inline constexpr int kEventBit = 2;
inline constexpr int kIndexBit = 8;
inline constexpr int kDataBit = 32;

/** One load PART.TABLE would otherwise be given a word at a time. */
struct Entry {
  uint8_t sel = 0;
  uint8_t event = 0;
  uint8_t index = 0;
  // The load word, low bits first. kLdW bits are meaningful.
  uint64_t data[3] = {0, 0, 0};
};

enum class Verdict : uint8_t {
  kOk = 0,
  kBadMagic = 1,
  kBadVersion = 2,
  kTruncated = 3,  // `entries` does not fit in the page's declared length
};

/** Write one entry into a 32-byte little-endian slice. */
inline void put_entry(uint8_t* p, const Entry& e) {
  for (size_t i = 0; i < kEntryBytes; ++i) p[i] = 0;
  p[0] = static_cast<uint8_t>((e.sel & 0x3u) | ((e.event & 0x3u) << 2));
  p[1] = static_cast<uint8_t>(e.index & 0x7Fu);
  for (int b = 0; b < kLdW; ++b) {
    const int src = b;
    const uint64_t bit = (e.data[src / 64] >> (src % 64)) & 1u;
    const int dst = kDataBit + b;
    if (bit) p[dst / 8] = static_cast<uint8_t>(p[dst / 8] | (1u << (dst % 8)));
  }
}

/** Read one entry back out of a 32-byte little-endian slice. */
inline Entry get_entry(const uint8_t* p) {
  Entry e;
  e.sel = static_cast<uint8_t>(p[0] & 0x3u);
  e.event = static_cast<uint8_t>((p[0] >> 2) & 0x3u);
  e.index = static_cast<uint8_t>(p[1] & 0x7Fu);
  for (int b = 0; b < kLdW; ++b) {
    const int src = kDataBit + b;
    if ((p[src / 8] >> (src % 8)) & 1u) e.data[b / 64] |= (1ull << (b % 64));
  }
  return e;
}

/** Build a page. The result is padded to a multiple of 64 bytes, which is
 *  what MEM.UPLOAD's length rule requires of every upload. */
inline std::vector<uint8_t> build(const std::vector<Entry>& entries) {
  size_t n = kHeaderBytes + kEntryBytes * entries.size();
  if (n % 64u != 0u) n += 64u - (n % 64u);
  std::vector<uint8_t> page(n, 0u);
  page[0] = static_cast<uint8_t>(kMagic & 0xFFu);
  page[1] = static_cast<uint8_t>((kMagic >> 8) & 0xFFu);
  page[2] = static_cast<uint8_t>((kMagic >> 16) & 0xFFu);
  page[3] = static_cast<uint8_t>((kMagic >> 24) & 0xFFu);
  page[4] = static_cast<uint8_t>(kVersion & 0xFFu);
  page[5] = static_cast<uint8_t>((kVersion >> 8) & 0xFFu);
  page[6] = static_cast<uint8_t>(entries.size() & 0xFFu);
  page[7] = static_cast<uint8_t>((entries.size() >> 8) & 0xFFu);
  for (size_t i = 0; i < entries.size(); ++i)
    put_entry(page.data() + kHeaderBytes + kEntryBytes * i, entries[i]);
  return page;
}

/** Decode a page exactly as the loader does. `out` is left EMPTY on any
 *  verdict but kOk -- a refused page is refused whole. */
inline Verdict decode(const uint8_t* page, size_t bytes, std::vector<Entry>& out) {
  out.clear();
  if (bytes < kHeaderBytes) return Verdict::kTruncated;
  const uint32_t magic = static_cast<uint32_t>(page[0]) |
                         (static_cast<uint32_t>(page[1]) << 8) |
                         (static_cast<uint32_t>(page[2]) << 16) |
                         (static_cast<uint32_t>(page[3]) << 24);
  if (magic != kMagic) return Verdict::kBadMagic;
  const uint16_t version =
      static_cast<uint16_t>(page[4] | (static_cast<uint16_t>(page[5]) << 8));
  if (version != kVersion) return Verdict::kBadVersion;
  const uint16_t count =
      static_cast<uint16_t>(page[6] | (static_cast<uint16_t>(page[7]) << 8));
  if (kHeaderBytes + kEntryBytes * static_cast<size_t>(count) > bytes)
    return Verdict::kTruncated;
  out.reserve(count);
  for (uint16_t i = 0; i < count; ++i)
    out.push_back(get_entry(page + kHeaderBytes + kEntryBytes * i));
  return Verdict::kOk;
}

}  // namespace species_page
}  // namespace zref
