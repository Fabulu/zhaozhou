// zhao_binding_seal.hpp -- the binding-page seal, modelled once.
//
// `zhao_texture_binding_resolver_v2` seals a staging page by folding the
// generation byte and then all 256 rows into a CRC, and refuses the page with
// CFG_BAD_CRC if the value the owner presented does not match. Any test that
// wants a page to ACTIVATE has to produce that value, and there is now more
// than one such test.
//
// This header exists because the second one would otherwise have written the
// fold again. CLAUDE.md is explicit about that shape -- a second implementation
// of ratified arithmetic is how the projector came to exist twice -- and a seal
// is exactly the kind of thing that gets re-derived slightly differently and
// then disagrees only for pages nobody happened to test.
//
// THE ALGORITHM, read off the RTL rather than restated from a contract:
//
//   crc = crc_byte(0xFFFFFFFF, generation)          // the generation first
//   for selector in 0..255:
//       row  = present[selector] ? packed_row : 0   // 75 bits
//       wide = row zero-extended to 80 bits         // `{5'b0, crc_row_q}`
//       for b in 0..9: crc = crc_byte(crc, wide[b]) // ten bytes, always
//   seal = crc ^ 0xFFFFFFFF
//
// Note the two things that are easy to get wrong and are load-bearing:
//
//  * an INVALID selector contributes ten ZERO bytes, not nothing and not its
//    stale payload. The RTL says so: `crc_canonical_row_c` is
//    `crc_row_present_q ? {5'b0, crc_row_q} : 80'd0`, which is why a page that
//    only writes selector 0 still folds 2,560 bytes.
//  * the polynomial is 0xEDB88320 -- REFLECTED CRC-32, the zlib/IEEE one. It
//    is NOT CRC-32C: Castagnoli is 0x82F63B78. Several documents in this repo
//    call this seal "the CRC32C seal" and they are wrong about the name; the
//    silicon is the authority and the silicon folds 0xEDB88320.
#ifndef ZHAO_BINDING_SEAL_HPP
#define ZHAO_BINDING_SEAL_HPP

#include <array>
#include <cstdint>

namespace zhao_binding_seal {

// One binding row as the configuration channel carries it.
struct Row {
  uint32_t base = 0;
  uint32_t mode = 0;
  uint8_t palette_slot = 0;
  uint8_t palette_generation = 0;
  bool valid = true;
};

inline std::array<uint32_t, 3> pack_row(const Row& r) {
  std::array<uint32_t, 3> words{};
  words[0] = r.base;
  words[1] = r.mode;
  words[2] = static_cast<uint32_t>(r.palette_slot & 3u) |
             (static_cast<uint32_t>(r.palette_generation) << 2) |
             (static_cast<uint32_t>(r.valid ? 1u : 0u) << 10);
  return words;
}

inline uint32_t mode(uint8_t format, bool filter, uint8_t wrap_u, uint8_t wrap_v, uint8_t log2w,
                     uint8_t log2h, uint8_t max_level, bool mip_enable) {
  return static_cast<uint32_t>(format & 7u) | (static_cast<uint32_t>(filter) << 3) |
         (static_cast<uint32_t>(wrap_u & 3u) << 4) | (static_cast<uint32_t>(wrap_v & 3u) << 6) |
         (static_cast<uint32_t>(log2w & 15u) << 8) | (static_cast<uint32_t>(log2h & 15u) << 12) |
         (static_cast<uint32_t>(max_level & 15u) << 16) | (static_cast<uint32_t>(mip_enable) << 20);
}

inline uint32_t crc_byte(uint32_t crc, uint8_t data) {
  for (unsigned bit = 0; bit < 8; ++bit)
    crc = ((crc ^ (data >> bit)) & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
  return crc;
}

// The ten canonical bytes a selector contributes: its packed row when present,
// ten zeroes when not.
inline std::array<uint8_t, 10> row_bytes(const Row& row, bool present) {
  std::array<uint8_t, 10> bytes{};
  if (present) {
    const auto words = pack_row(row);
    for (unsigned bit = 0; bit < 75; ++bit) {
      if ((words[bit / 32] >> (bit % 32)) & 1u)
        bytes[bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
    }
  }
  return bytes;
}

inline uint32_t page_crc(uint8_t generation, const std::array<Row, 256>& rows,
                         const std::array<bool, 256>& present) {
  uint32_t crc = crc_byte(0xFFFFFFFFu, generation);
  for (unsigned selector = 0; selector < 256; ++selector) {
    for (uint8_t byte : row_bytes(rows[selector], present[selector])) crc = crc_byte(crc, byte);
  }
  return crc ^ 0xFFFFFFFFu;
}

// The seal of a page holding a single row at `selector`. The common case for a
// test that just needs SOME page to activate, and the case where forgetting
// that the other 255 selectors still fold ten zero bytes each gives a value
// that is wrong in a way no amount of staring at one row reveals.
inline uint32_t page_crc_single(uint8_t generation, unsigned selector, const Row& row) {
  std::array<Row, 256> rows{};
  std::array<bool, 256> present{};
  rows[selector] = row;
  present[selector] = true;
  return page_crc(generation, rows, present);
}

}  // namespace zhao_binding_seal

#endif  // ZHAO_BINDING_SEAL_HPP
