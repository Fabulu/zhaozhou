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
// attachments or hitboxes.
//
// AMENDED under owner ruling R90 (2026-09-20): the BONE HIERARCHY is now
// frozen too, in the `body` namespace at the bottom of this file, and the
// sentence above is left standing because it is the record of what R26 alone
// did. Parts, meshlet ids, attachments and hitboxes remain frozen. The header
// carries `body_off`, the byte offset at
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

// ===========================================================================
// THE BODY SECTION — owner ruling R90's SECOND PARTIAL LIFT of the kind-8
// freeze, and the oracle `zhao_geom_bonesrc` is differentiated against.
// ===========================================================================
//
// R26 lifted four ladder constants. R90 lifts the BONE HIERARCHY, on the same
// terms and through the same mechanism: `body_off` names where the body starts,
// so the frozen half above is not touched. It is not touched — the bodyless
// golden `tests/golden/creature_ladder/ladder_page_v1.bin` is byte-identical
// before and after this section existed, which `mkcreatureladder.py --check`
// asserts on every run rather than leaving as a claim.
//
// STILL FROZEN, and this file still freezes nothing about them: parts, meshlet
// ids, attachments and hitboxes. R90 named the bone hierarchy and a clip frame
// and nothing else.
//
// THE CLIP FRAME (kind 9) IS NOT DEFINED HERE BECAUSE IT IS ALREADY FROZEN
// ELSEWHERE, and the note at zref_creature.hpp's top calling the quaternion
// lane format "PROPOSED, NOT FROZEN" is STALE — it predates the amendment that
// settled it. `spec/creature_rules.md` §2.1 is headed "Storage (frozen; the Q
// formats are frozen — qformats §7.6, C1)" and gives the bytes outright: per
// frame, 12 B of root displacement (3 × fx16) then `bone_count` × 8 B of
// `quat16`, ≤ 268 B at 32 bones. `spec/qformats.md` §7.6 ratifies the lane
// format under amendment C1 — four s16 lanes, S 1.0.14, hemisphere-canonical —
// and §7's table row states it again. A kind-9 frame reader therefore has a
// layout to read and needs no lift; what it needs is a producer.
//
// CORRECTED 2026-09-21 (POSEPAGE). THE PARAGRAPH ABOVE IS TRUE OF A FRAME AND
// FALSE OF A PAGE, and it is left standing because it is the record of what was
// believed when the body section landed. §2.1 freezes what one frame CONTAINS.
// §5 sketches the PAGE in one clause — "clip directory {slot_id u16,
// frame_count u16, event_count u16} + frames + event tags" — under a heading
// reading "Cartridge pages (additive; LAYOUTS FREEZE WITH SW.TOOLS.ASSET AT
// PHASE 12 ENTRY)". No magic, no version, no field order, no offsets, no
// alignment, and no statement of where frame `f` of clip `c` begins.
//
// A READER CANNOT READ A FROZEN FRAME IT CANNOT LOCATE, so the container was
// exactly the guessed layout `spec/cartridge.md` §4's deterministic-refusal
// sentence forbids inventing — and exactly what R90's recommendation item 1
// authorised AUTHORING: "author/freeze the body section AND A MINIMAL KIND-9
// FRAME WITH A ZREF MODEL". The body half landed here; the kind-9 half is now
// `reference/include/zref/zref_clip_page.hpp`, with `tools/pack/mkclipbank.py`
// and the golden `tests/golden/creature_clip/clip_page_v1.bin` pinning it the
// way this file's own layout is pinned.
//
// The sentence "what it needs is a producer" remains correct and is now the
// WHOLE of what core entry I29 waits on — see that entry's blocker (b), which
// is that nothing in `spec/commands.zidl` carries a clip or a frame number.
namespace body {

inline constexpr uint32_t kMagic = 0x38424354u;  //!< 'T','C','B','8' LE

/** VERSION 2 -- the OWNER FORM INDEX, owner ruling of 2026-09-21 (kind-8 /
 *  kind-9 ownership), section 2. A v1 body says which BONES it has and never
 *  which FORM they belong to, so a resident skeleton could be composed against
 *  any draw. `bone_mismatch` is blind to that whenever the two creatures have
 *  the same bone count, which is the common case.
 *
 *  THE OUTER kind-8 HEADER AND THE LADDER TABLE STAY AT v1. The ruling says so
 *  in terms, and it is why the version constants are SEPARATE here and in
 *  `zhao_geom_clipread` rather than one shared symbol: bumping one constant to
 *  2 would reject the still-v1 outer header the body is appended to.
 *
 *  `kVersionV1` stays NAMED so historical fixtures remain identifiable --
 *  `tests/golden/creature_ladder/ladder_page_body_v1.bin` is still committed,
 *  is still a legal v1 body, and `decode_body` refuses it. */
inline constexpr uint16_t kVersionV1 = 1u;
inline constexpr uint16_t kVersion = 2u;
inline constexpr size_t kHeaderBytes = 64;
inline constexpr size_t kBoneBytes = 32;
inline constexpr int kMaxBones = 32;  //!< creature_rules §1.2

/** `flags` bit 0: inv_rest is translate(−world_rest). Must be set in v1. */
inline constexpr uint8_t kFlagRigidRest = 0x01u;

// Bone record offsets, little-endian, matching
// `fpga/rtl/geometry/zhao_geom_bonesrc.sv` field for field.
inline constexpr size_t kOffParent = 0;
inline constexpr size_t kOffFlags = 1;
inline constexpr size_t kOffRestTx = 4;
inline constexpr size_t kOffInvTx = 16;

// BODY HEADER byte offsets. Bytes 0..15 are v1's and do not move:
//   +0  u32 magic   +4 u16 version   +6 u8 bone_count   +7 u8 flags
//   +8  u32 bones_off                +12 u32 reserved, still zero in v2.
/** v2's owner word: ONE ALIGNED LITTLE-ENDIAN u32 for a SEMANTIC u24, bits
 *  23:0 the owner's MESH_STREAM form index, bits 31:24 MUST be zero. It sits
 *  in EXISTING padding -- the body header was and remains 64 bytes, and every
 *  bone record offset and `body_off` semantic is unchanged. */
inline constexpr size_t kOffOwnerForm = 16;

/** The same 24-bit handle index the ladder record keys on, named once more in
 *  this scope so the two statements of the mask cannot drift.
 *
 *  INDEX ZERO IS NOT A SENTINEL. Presence of an owner follows the VERSION and
 *  the validated presence of the section, never the numeric value. A body
 *  owned by form 0 is an ordinary body. */
inline constexpr uint32_t kOwnerFormMask = kFormIndexMask;

enum class BodyVerdict : uint8_t {
  kOk = 0,
  kBadMagic = 1,
  kBadVersion = 2,
  kTruncated = 3,
  kBadBoneCount = 4,   //!< 0, or past the 32-bone ceiling
  kBadParent = 5,      //!< a parent at or after its child, or a non-zero root
  kNotRigidRest = 6,   //!< a record claiming a rest rotation this build cannot
                       //!< decode — refused, never decoded wrongly
  kReservedNz = 7,     //!< a reserved field is not zero
  kBadOwner = 8,       //!< the owner word's bits 31:24 are not zero. The
                       //!< FORMAT rule only; WHICH form owns the body is the
                       //!< loader's comparison, because only the loader holds
                       //!< the request to compare it against.
  kOwnerNotInLadder = 9,  //!< a body-bearing kind-8 page with no LADDER RECORD
                       //!< for the body's own owner. The ruling: a page that
                       //!< carries a skeleton for a form it does not otherwise
                       //!< describe is a page whose two halves disagree.
};

/**
 * One bone AS STORED. `parent` and the local rest translation are authored;
 * `inv_*` is BAKED — see `bake_body`.
 */
struct BoneRecord {
  uint8_t parent = 0;
  int32_t tx = 0, ty = 0, tz = 0;      //!< LOCAL rest translation, fx16
  int32_t inv_tx = 0, inv_ty = 0, inv_tz = 0;  //!< −world_rest, fx16
};

inline void put_i32(uint8_t* p, int32_t v) { put_u32(p, static_cast<uint32_t>(v)); }
inline int32_t get_i32(const uint8_t* p) { return static_cast<int32_t>(get_u32(p)); }

/**
 * `zref::creature::bake_skeleton`'s running sum, restated over the page's own
 * record type. Returns false on exactly the legalities that function enforces
 * — parent-before-child, a zero root parent, the bone ceiling — plus the i32
 * overflow a long rest chain can reach and C++ would otherwise wrap silently.
 *
 * WHY THIS EXISTS BESIDE `bake_skeleton` RATHER THAN CALLING IT: that function
 * takes `zref::creature::Skeleton` and fills a `SkeletonBake` of twelve-element
 * matrices, which is the SIM's shape. This one produces the three numbers the
 * PAGE stores. They must agree, and `tests/geometry/creature_page_body_directed`
 * requires exactly that against the same skeleton rather than trusting the
 * restatement — the three-way pin this file already lives under.
 */
inline bool bake_body(const std::vector<BoneRecord>& in,
                      std::vector<BoneRecord>& out) {
  out.clear();
  if (in.empty() || static_cast<int>(in.size()) > kMaxBones) return false;
  std::vector<int64_t> wx, wy, wz;
  for (size_t b = 0; b < in.size(); ++b) {
    const BoneRecord& bn = in[b];
    if (b == 0 && bn.parent != 0) return false;
    if (static_cast<size_t>(bn.parent) > b) return false;
    const int64_t px = b == 0 ? 0 : wx[bn.parent];
    const int64_t py = b == 0 ? 0 : wy[bn.parent];
    const int64_t pz = b == 0 ? 0 : wz[bn.parent];
    const int64_t x = px + bn.tx, y = py + bn.ty, z = pz + bn.tz;
    const int64_t lo = -(int64_t{1} << 31), hi = (int64_t{1} << 31) - 1;
    if (x < lo || x > hi || y < lo || y > hi || z < lo || z > hi) return false;
    wx.push_back(x);
    wy.push_back(y);
    wz.push_back(z);
    BoneRecord r = bn;
    r.inv_tx = static_cast<int32_t>(-x);
    r.inv_ty = static_cast<int32_t>(-y);
    r.inv_tz = static_cast<int32_t>(-z);
    out.push_back(r);
  }
  return true;
}

/**
 * Expand a baked record into the twelve-element inverse-rest matrix
 * `zhao_geom_pose_decode` consumes: identity rotation, negated world
 * translation in m[3]/m[7]/m[11], in Q16.16. This is what the RTL's
 * `INV_REST_RIGID` path builds out of constants, stated once here so the two
 * cannot drift.
 */
inline void inv_rest_matrix(const BoneRecord& r, int32_t m[12]) {
  for (int i = 0; i < 12; ++i) m[i] = 0;
  m[0] = 65536; m[5] = 65536; m[10] = 65536;
  m[3] = r.inv_tx; m[7] = r.inv_ty; m[11] = r.inv_tz;
}

/** The body section bytes: a 64-byte header then one 32-byte record per bone,
 *  padded to 64. Empty on any refusal `bake_body` makes. */
inline std::vector<uint8_t> build_body(const std::vector<BoneRecord>& bones,
                                       uint32_t owner_form_index) {
  std::vector<BoneRecord> baked;
  if ((owner_form_index & ~kOwnerFormMask) != 0u) return {};
  if (!bake_body(bones, baked)) return {};
  size_t n = kHeaderBytes + kBoneBytes * baked.size();
  if (n % 64u != 0u) n += 64u - (n % 64u);
  std::vector<uint8_t> body(n, 0u);
  put_u32(body.data(), kMagic);
  body[4] = static_cast<uint8_t>(kVersion & 0xFFu);
  body[5] = static_cast<uint8_t>((kVersion >> 8) & 0xFFu);
  body[6] = static_cast<uint8_t>(baked.size() & 0xFFu);
  body[7] = kFlagRigidRest;
  put_u32(body.data() + 8, static_cast<uint32_t>(kHeaderBytes));
  // SUPPLIED, never inferred. The ruling forbids deriving the owner from row
  // zero of the ladder or from a matching bone count; this function is given
  // the number or it emits nothing.
  put_u32(body.data() + kOffOwnerForm, owner_form_index & kOwnerFormMask);
  for (size_t i = 0; i < baked.size(); ++i) {
    uint8_t* p = body.data() + kHeaderBytes + kBoneBytes * i;
    p[kOffParent] = baked[i].parent;
    p[kOffFlags] = kFlagRigidRest;
    put_i32(p + kOffRestTx + 0, baked[i].tx);
    put_i32(p + kOffRestTx + 4, baked[i].ty);
    put_i32(p + kOffRestTx + 8, baked[i].tz);
    put_i32(p + kOffInvTx + 0, baked[i].inv_tx);
    put_i32(p + kOffInvTx + 4, baked[i].inv_ty);
    put_i32(p + kOffInvTx + 8, baked[i].inv_tz);
  }
  return body;
}

/** Decode a body section. `out` is left EMPTY on any verdict but kOk — a
 *  refused body is refused whole, exactly as a refused page is. */
inline BodyVerdict decode_body(const uint8_t* body, size_t bytes,
                               std::vector<BoneRecord>& out,
                               uint32_t* owner_form_index_o = nullptr) {
  out.clear();
  if (owner_form_index_o) *owner_form_index_o = 0;
  if (bytes < kHeaderBytes) return BodyVerdict::kTruncated;
  if (get_u32(body) != kMagic) return BodyVerdict::kBadMagic;
  const uint16_t version =
      static_cast<uint16_t>(body[4] | (static_cast<uint16_t>(body[5]) << 8));
  // A v1 body is REFUSED, not upgraded: it carries no owner and no conversion
  // can invent one out of the bytes that are there.
  if (version != kVersion) return BodyVerdict::kBadVersion;
  const uint8_t count = body[6];
  if (body[7] != kFlagRigidRest) return BodyVerdict::kNotRigidRest;
  if (count == 0 || count > kMaxBones) return BodyVerdict::kBadBoneCount;
  const uint32_t bones_off = get_u32(body + 8);
  if (get_u32(body + 12) != 0u) return BodyVerdict::kReservedNz;
  const uint32_t owner_w = get_u32(body + kOffOwnerForm);
  if ((owner_w & ~kOwnerFormMask) != 0u) return BodyVerdict::kBadOwner;
  if (bones_off + kBoneBytes * static_cast<size_t>(count) > bytes)
    return BodyVerdict::kTruncated;
  std::vector<BoneRecord> staged;
  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t* p = body + bones_off + kBoneBytes * i;
    BoneRecord r;
    r.parent = p[kOffParent];
    if ((p[kOffFlags] & kFlagRigidRest) == 0u) return BodyVerdict::kNotRigidRest;
    if ((p[kOffFlags] & ~kFlagRigidRest) != 0u) return BodyVerdict::kReservedNz;
    if (p[2] != 0u || p[3] != 0u) return BodyVerdict::kReservedNz;
    if (get_u32(p + 28) != 0u) return BodyVerdict::kReservedNz;
    if (i == 0 && r.parent != 0) return BodyVerdict::kBadParent;
    if (static_cast<size_t>(r.parent) > i) return BodyVerdict::kBadParent;
    r.tx = get_i32(p + kOffRestTx + 0);
    r.ty = get_i32(p + kOffRestTx + 4);
    r.tz = get_i32(p + kOffRestTx + 8);
    r.inv_tx = get_i32(p + kOffInvTx + 0);
    r.inv_ty = get_i32(p + kOffInvTx + 4);
    r.inv_tz = get_i32(p + kOffInvTx + 8);
    staged.push_back(r);
  }
  out.swap(staged);
  if (owner_form_index_o) *owner_form_index_o = owner_w;
  return BodyVerdict::kOk;
}

/** The body header's declared version, WITHOUT judging it -- how a v1 fixture
 *  stays identifiable once `decode_body` refuses it. */
inline uint16_t body_version(const uint8_t* body, size_t bytes) {
  if (bytes < kHeaderBytes) return 0;
  return static_cast<uint16_t>(body[4] | (static_cast<uint16_t>(body[5]) << 8));
}

/** The owner word AS STORED, high byte included and unjudged. The RTL reader
 *  is differenced against this rather than against a recomputation. */
inline uint32_t body_owner_word(const uint8_t* body, size_t bytes) {
  if (bytes < kHeaderBytes) return 0;
  return get_u32(body + kOffOwnerForm);
}

}  // namespace body

/**
 * THE PAGE'S TWO HALVES MUST AGREE ABOUT WHO THE BODY BELONGS TO.
 *
 * Owner ruling of 2026-09-21, section 2: "Validate that a body-bearing kind-8
 * page contains a ladder record for its body owner; the other ladder records
 * remain independently owned metadata, NOT users of that body."
 *
 * The record does not have to be ROW ZERO and the bank stays a MULTI-FORM
 * TABLE -- the committed golden's body is owned by its SECOND record on
 * purpose, so a packer or reader that quietly reads row zero is caught by the
 * fixture rather than by an argument.
 */
inline bool body_owner_in_ladder(const std::vector<Record>& records,
                                 uint32_t body_owner_form_index) {
  if ((body_owner_form_index & ~kFormIndexMask) != 0u) return false;
  for (const Record& r : records)
    if (r.form_index == body_owner_form_index) return true;
  return false;
}

/**
 * Build a page WITH a body appended. The bodyless `build` above is untouched
 * and still emits `body_off == 0`; this one computes the offset of the body it
 * actually writes, which is the only honest source for that number.
 *
 * `body_owner_form_index` is SUPPLIED by the asset definition. It is never
 * inferred from row zero, from a matching bone count, from the publication
 * index or from the loader's order.
 */
inline std::vector<uint8_t> build_with_body(
    const std::vector<Record>& records,
    const std::vector<body::BoneRecord>& bones,
    uint32_t body_owner_form_index) {
  if (!body_owner_in_ladder(records, body_owner_form_index)) return {};
  std::vector<uint8_t> page = build(records, 0u);
  std::vector<uint8_t> b = body::build_body(bones, body_owner_form_index);
  if (b.empty()) return {};
  put_u32(page.data() + 8, static_cast<uint32_t>(page.size()));
  page.insert(page.end(), b.begin(), b.end());
  return page;
}

}  // namespace creature_page
}  // namespace zref
