// zref_creature_page.hpp -- the CREATURE_FORM page's LADDER TABLE, frozen.
// Owner ruling R26 (reports/OWNER-RULINGS-20260919-EVENING.md, 2026-09-19):
// "Lift the kind-8 freeze for GEOM.LOD's FOUR constants only (bound radius,
// micro/splat/glint error). Their layout is frozen now and the packer emits
// them." Scheduled as sub-build 1 of ruling R68.
//
// This header IS that layout, and `zhao_geom_ladderbank`
// (fpga/rtl/geometry/zhao_geom_ladderbank.sv) reads exactly it.
// tests/geometry/geom_ladderbank_directed.cpp differences the two.
//
// ---------------------------------------------------------------------------
// WHAT IS LIFTED, AND WHAT IS STILL FROZEN
// ---------------------------------------------------------------------------
// `spec/cartridge.md` 4 says of kinds 8 and 9: "Byte-exact layouts freeze with
// SW.TOOLS.ASSET at Phase-12 entry (creature_rules 9); until then the packer
// refuses to emit them (deterministic refusal, never a guessed layout)."
//
// R26 lifts that sentence for FOUR fields and no others. So this file freezes:
//
//   * a 64-byte page HEADER, and
//   * a LADDER TABLE of 32-byte records immediately after it,
//
// and it freezes NOTHING about parts, meshlet ids, the bone hierarchy,
// attachments or hitboxes. The header carries `body_off`, the byte offset at
// which the Phase-12 body begins, precisely so that the frozen half can be
// read today and the unfrozen half can be appended later without moving a byte
// of it. `body_off == 0` means "no body yet", which is what the packer emits
// until SW.TOOLS.ASSET exists.
//
// This is the difference between lifting a freeze and guessing a layout. A
// guessed layout invents offsets for fields nobody has defined; this one
// defines offsets for four fields the reference has carried since phase 8
// (`zref::creature::CreatureType::bound_radius` and its three siblings) and
// leaves a declared hole for everything else.
//
// ---------------------------------------------------------------------------
// THE KEY IS THE MESH_STREAM HANDLE INDEX, NOT AN INVENTED TYPE ID
// ---------------------------------------------------------------------------
// `zref::creature::CreatureType::type_id` is "index into the type table" --
// software's own key, which no hardware port carries. What hardware DOES carry
// is `DrawForm.form`, a handle32 whose index `zhao_geom_drawjob` already keys
// its MESH_STREAM residency directory by (`d_form_i[31:8]` -> `form_idx_q`,
// directory rule 5f.1). Two instances of one creature share that index, so it
// IS the per-creature-type key at the hardware boundary.
//
// Keying the ladder by it means the bank and the mesh directory agree by
// construction rather than by a second mapping law, and it is the same
// decision R45 made for the stamp's patch ("No second mapping law").
//
// ---------------------------------------------------------------------------
// WHY EVERY NUMBER IS 64-BYTE SHAPED
// ---------------------------------------------------------------------------
// MEM.GUARD's read is at most 64 bytes and its shape rule requires the byte
// mask to match the length, so the cheapest honest reader is one that asks for
// whole 64-byte lines. The header is one line; a record is 32 bytes, so every
// line after the header carries exactly TWO records and no record ever
// straddles a read. That is `zref::species_page`'s shape, deliberately: a
// second page reader in this tree should not have a second read discipline.
//
// A record spends 32 bytes on 20 bytes of fields. The 12 reserved bytes are
// the price of that property, on a page uploaded once.
//
// ---------------------------------------------------------------------------
// A REFUSED PAGE IS REFUSED WHOLE, AND A BAD RECORD REFUSES ITS PAGE
// ---------------------------------------------------------------------------
// Magic, version and the record count against the declared extent are judged
// from the header line before any record is stored. Beyond that, a record with
// `bound_radius <= 0` or a negative error is REFUSED and takes the page with
// it, because `zref::creature::lod_raw`'s divide-free identities
// (`fpga/rtl/geometry/zhao_geom_lod.sv`, "THE IDENTITIES HOLD ONLY FOR A
// NON-NEGATIVE NUMERATOR") hold only for a non-negative numerator and a
// positive bound radius. A ladder fed a zero bound radius does not produce a
// slightly wrong rung; it produces a rung the oracle never computes.
//
// Half-loading is the failure this refuses: a bank holding four records of one
// author's creature and four of another's reads as an art problem and is not
// one.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace zref {
namespace creature_page {

/** `spec/cartridge.md` 4's page-kind registry: CREATURE_FORM is 8. */
inline constexpr uint8_t kPageKind = 8;

inline constexpr uint32_t kMagic = 0x4D46435Au;  // 'Z','C','F','M' little-endian
inline constexpr uint16_t kVersion = 1;
inline constexpr size_t kHeaderBytes = 64;
inline constexpr size_t kRecordBytes = 32;

/** Byte offsets inside a record, frozen. */
inline constexpr size_t kOffFormIndex = 0;
inline constexpr size_t kOffBoundRadius = 4;
inline constexpr size_t kOffMicroError = 8;
inline constexpr size_t kOffSplatError = 12;
inline constexpr size_t kOffGlintError = 16;

/** The handle index is 24 bits (`handle32` = {index[31:8], generation[7:0]}). */
inline constexpr uint32_t kFormIndexMask = 0x00FFFFFFu;

/**
 * One creature type's ladder constants, as `zref::creature::CreatureType`
 * carries them. Nothing here interprets them; they are handed to
 * `zref::creature::lod_raw` unchanged.
 */
struct Record {
  uint32_t form_index = 0;    //!< MESH_STREAM handle index, bits 23:0
  int32_t bound_radius = 0;   //!< fx16, > 0
  int32_t micro_error = 0;    //!< fx16, >= 0
  int32_t splat_error = 0;    //!< fx16, >= 0
  int32_t glint_error = 0;    //!< fx16, >= 0
};

enum class Verdict : uint8_t {
  kOk = 0,
  kBadMagic = 1,
  kBadVersion = 2,
  kTruncated = 3,     //!< `records` does not fit in the declared length
  kBadRecord = 4,     //!< bound_radius <= 0, a negative error, or a reserved
                      //!< high byte in `form_index`
  kOverflow = 5,      //!< more records than the bank has rows
};

inline void put_u32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFFu);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
  p[2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
  p[3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
}

inline uint32_t get_u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

/** Write one record into a 32-byte little-endian slice. */
inline void put_record(uint8_t* p, const Record& r) {
  for (size_t i = 0; i < kRecordBytes; ++i) p[i] = 0;
  put_u32(p + kOffFormIndex, r.form_index & kFormIndexMask);
  put_u32(p + kOffBoundRadius, static_cast<uint32_t>(r.bound_radius));
  put_u32(p + kOffMicroError, static_cast<uint32_t>(r.micro_error));
  put_u32(p + kOffSplatError, static_cast<uint32_t>(r.splat_error));
  put_u32(p + kOffGlintError, static_cast<uint32_t>(r.glint_error));
}

/** Read one record back out of a 32-byte little-endian slice. */
inline Record get_record(const uint8_t* p) {
  Record r;
  r.form_index = get_u32(p + kOffFormIndex);
  r.bound_radius = static_cast<int32_t>(get_u32(p + kOffBoundRadius));
  r.micro_error = static_cast<int32_t>(get_u32(p + kOffMicroError));
  r.splat_error = static_cast<int32_t>(get_u32(p + kOffSplatError));
  r.glint_error = static_cast<int32_t>(get_u32(p + kOffGlintError));
  return r;
}

/** The record legality the loader enforces, in one place so the RTL and the
 *  packer cannot disagree about it. */
inline bool record_legal(const Record& r) {
  if ((r.form_index & ~kFormIndexMask) != 0u) return false;
  if (r.bound_radius <= 0) return false;
  if (r.micro_error < 0 || r.splat_error < 0 || r.glint_error < 0) return false;
  return true;
}

/**
 * Build a page. `body_off` is the byte offset of the Phase-12 body, or 0 while
 * SW.TOOLS.ASSET does not exist -- see the header. The result is padded to a
 * multiple of 64 bytes, which is what MEM.UPLOAD's length rule requires of
 * every upload.
 */
inline std::vector<uint8_t> build(const std::vector<Record>& records,
                                  uint32_t body_off = 0u) {
  size_t n = kHeaderBytes + kRecordBytes * records.size();
  if (n % 64u != 0u) n += 64u - (n % 64u);
  std::vector<uint8_t> page(n, 0u);
  put_u32(page.data(), kMagic);
  page[4] = static_cast<uint8_t>(kVersion & 0xFFu);
  page[5] = static_cast<uint8_t>((kVersion >> 8) & 0xFFu);
  page[6] = static_cast<uint8_t>(records.size() & 0xFFu);
  page[7] = static_cast<uint8_t>((records.size() >> 8) & 0xFFu);
  put_u32(page.data() + 8, body_off);
  for (size_t i = 0; i < records.size(); ++i)
    put_record(page.data() + kHeaderBytes + kRecordBytes * i, records[i]);
  return page;
}

/**
 * Decode a page exactly as `zhao_geom_ladderbank` does. `out` is left EMPTY on
 * any verdict but kOk -- a refused page is refused whole. `rows` is the bank's
 * capacity; a page declaring more records than that is kOverflow, because a
 * bank that silently kept the first `rows` would answer some lookups from this
 * page and others from the last one.
 */
inline Verdict decode(const uint8_t* page, size_t bytes, size_t rows,
                      std::vector<Record>& out) {
  out.clear();
  if (bytes < kHeaderBytes) return Verdict::kTruncated;
  if (get_u32(page) != kMagic) return Verdict::kBadMagic;
  const uint16_t version =
      static_cast<uint16_t>(page[4] | (static_cast<uint16_t>(page[5]) << 8));
  if (version != kVersion) return Verdict::kBadVersion;
  const uint16_t count =
      static_cast<uint16_t>(page[6] | (static_cast<uint16_t>(page[7]) << 8));
  if (kHeaderBytes + kRecordBytes * static_cast<size_t>(count) > bytes)
    return Verdict::kTruncated;
  if (static_cast<size_t>(count) > rows) return Verdict::kOverflow;
  std::vector<Record> staged;
  staged.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    const Record r = get_record(page + kHeaderBytes + kRecordBytes * i);
    if (!record_legal(r)) return Verdict::kBadRecord;
    staged.push_back(r);
  }
  out.swap(staged);
  return Verdict::kOk;
}

/**
 * The bank's lookup, as `zhao_geom_ladderbank` answers it: the FIRST record
 * whose `form_index` matches. A miss is a miss -- never the nearest row and
 * never row zero, because a ladder run against another creature's bound radius
 * picks a rung the oracle never picks and nothing downstream can tell.
 */
inline bool lookup(const std::vector<Record>& bank, uint32_t form_index,
                   Record& out) {
  for (const Record& r : bank) {
    if (r.form_index == (form_index & kFormIndexMask)) {
      out = r;
      return true;
    }
  }
  return false;
}

}  // namespace creature_page
}  // namespace zref
