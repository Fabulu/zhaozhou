// zref_clip_page.hpp -- the CLIP_BANK page (cartridge kind 9), frozen.
//
// Owner ruling R90 (reports/OWNER-RULINGS-20260919-EVENING.md, amended
// 2026-09-20), recommendation item 1, in its own words:
//
//     "author/freeze the body section AND A MINIMAL KIND-9 FRAME WITH A ZREF
//      MODEL"
//
// The body section was authored on 2026-09-21 by the POSEABI packet
// (`zref::creature_page::body`). The kind-9 half of that same sentence was NOT
// reached, and this file is it.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE HAD TO EXIST, AGAINST A SENTENCE SAYING IT DID NOT
// ---------------------------------------------------------------------------
// `zref_creature_page.hpp` currently says, immediately above its `body`
// namespace:
//
//     "THE CLIP FRAME (kind 9) IS NOT DEFINED HERE BECAUSE IT IS ALREADY
//      FROZEN ELSEWHERE ... A kind-9 frame reader therefore has a layout to
//      read and needs no lift; what it needs is a producer."
//
// That sentence is TRUE OF A FRAME AND FALSE OF A PAGE, and the distinction is
// the whole reason entry I29 could not be closed by building a reader:
//
//   * `spec/creature_rules.md` 2.1 is headed "Storage (frozen; the Q formats
//     are frozen -- qformats 7.6, C1)" and freezes WHAT ONE FRAME CONTAINS:
//     12 B of root displacement (3 x fx16) then `bone_count` x 8 B of
//     `quat16`. That is frozen and this file does not touch it.
//   * `spec/creature_rules.md` 5 sketches the PAGE in one clause -- "clip
//     directory {slot_id u16, frame_count u16, event_count u16} + frames +
//     event tags" -- under a heading that reads "Cartridge pages (additive;
//     LAYOUTS FREEZE WITH SW.TOOLS.ASSET AT PHASE 12 ENTRY)". There is no
//     magic, no version, no field order, no offsets, no alignment and no
//     statement of where in the page a given clip's frame `f` begins.
//
// A reader cannot read a frozen frame it cannot LOCATE. So the container is
// exactly the "guessed layout" `spec/cartridge.md` 4's deterministic-refusal
// sentence forbids inventing, and exactly the thing R90's lift authorises
// AUTHORING. Same mechanism as R26 and the body section: define offsets for
// data the reference has carried since phase 8, and leave a declared hole for
// everything still frozen.
//
// STILL FROZEN, and this file freezes nothing about them: event tags (the
// directory carries their offset and length and this model never interprets a
// byte of them), baked 60 Hz midpoint channels, and deformation samples. A
// reader that meets a non-zero `event_bytes` skips those bytes; it does not
// form an opinion about them.
//
// ---------------------------------------------------------------------------
// WHY EVERY OFFSET IS 64-BYTE SHAPED, AND WHAT THAT COSTS
// ---------------------------------------------------------------------------
// `zref_creature_page.hpp` states the rule and the price already: MEM.GUARD's
// read is at most 64 bytes and its shape rule requires the byte mask to match
// the length, so the cheapest honest reader asks for whole 64-byte lines, and
// a record that straddles a read is a record two reads have to be joined for.
// A second page reader in this tree should not have a second read discipline.
//
// Applied here that gives a frame SLOT rather than a packed frame:
//
//     slot + 0    a 64-byte FRAME HEADER whose first twelve bytes are
//                 creature_rules 2.1's root displacement, unchanged
//     slot + 64   `bone_count` x 8 B of `quat16`, unchanged
//
// The 52 spare bytes of the frame header are the price, and they buy two
// properties that matter to the silicon rather than to this file:
//
//   1. **A bone's quaternion is one 64-bit beat, on a beat boundary.**
//      `fpga/rtl/geometry/zhao_geom_bonesrc.sv`'s quat store takes one 64-bit
//      word per bone with lanes {w, x, y, z} at [15:0], [31:16], [47:32],
//      [63:48] -- which is byte for byte what `quat16` already is. Packed
//      immediately after a 12-byte root, bone 0 would begin at byte 12 and
//      every quaternion in the page would straddle two beats, so the reader
//      would carry a 32-bit rotate across beats for no reason but spacing.
//   2. **A frame begins on a line boundary**, so "frame f of clip c" is a
//      multiply and not a walk.
//
// THE FRAME'S CONTENTS ARE NOT CHANGED BY THIS. 2.1's "<= 268 B/frame at 32
// bones" is a statement about the authored frame and remains exactly true of
// the bytes; 320 is what one frame OCCUPIES on the page's read grid. Saying so
// explicitly is the point -- a container that silently renumbered the frozen
// half would be the thing R26's sentence exists to forbid.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS PINNED BY
// ---------------------------------------------------------------------------
// This is a layout, so it is stated in three places -- here, in
// `tools/pack/mkclipbank.py`, and (when it is built) in the RTL reader -- and
// three statements of one layout is two too many unless one ARTEFACT pins
// them. `tests/golden/creature_clip/clip_page_v1.bin` is that artefact:
// `mkclipbank.py --check` rebuilds it and compares byte for byte, and
// `tests/geometry/clip_page_directed.cpp` requires `zref::clip_page::build` to
// reproduce THAT SAME FILE. This is `zref_creature_page.hpp`'s own discipline
// and it is copied deliberately.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace zref {
namespace clip_page {

/** `spec/cartridge.md` 4's page-kind registry: CLIP_BANK is 9. */
inline constexpr uint8_t kPageKind = 9;

inline constexpr uint32_t kMagic = 0x504C435Au;  // 'Z','C','L','P' little-endian

/** VERSION 2 -- the OWNER FORM INDEX, owner ruling of 2026-09-21 (kind-8 /
 *  kind-9 ownership), section 2. A v1 bank carries no statement of WHICH form
 *  its frames animate, so a resident bank could answer any draw and produce a
 *  well-formed palette for the wrong animal with every counter green.
 *
 *  `kVersionV1` is kept NAMED rather than deleted because the ruling requires
 *  historical v1 fixtures to stay IDENTIFIABLE:
 *  `tests/golden/creature_clip/clip_page_v1.bin` is still committed, is still
 *  a legal v1 page, and `decode` refuses it -- which is the ruling's "unowned
 *  v1 CLIP_BANK data is not accepted on the new posed-render path", stated as
 *  a refusal rather than as prose. */
inline constexpr uint16_t kVersionV1 = 1;
inline constexpr uint16_t kVersion = 2;

/** The page header, the clip-directory record and the per-frame header are all
 *  64/32-byte shaped for the reason in the file header. */
inline constexpr size_t kHeaderBytes = 64;
inline constexpr size_t kClipBytes = 32;
inline constexpr size_t kFrameHeaderBytes = 64;
inline constexpr size_t kQuatBytes = 8;   //!< creature_rules 2.1, one bone
inline constexpr size_t kRootBytes = 12;  //!< creature_rules 2.1, 3 x fx16
inline constexpr size_t kLineBytes = 64;

/** creature_rules 1.2's bone ceiling, and a bank's clip ceiling. The clip
 *  ceiling is NAMED AND EDITABLE rather than implied by the u16 field: it is
 *  what a reader's directory store must hold, and the page refuses rather than
 *  truncating (`zhao_geom_ladderbank`'s `overflow_o` rule, one page over). */
inline constexpr int kMaxBones = 32;
inline constexpr int kMaxClips = 64;  //!< creature_rules 2.1, "64 authored slots"

/** The authored bytes of one frame, creature_rules 2.1 exactly. */
inline constexpr size_t frame_bytes(int bone_count) {
  return kRootBytes + kQuatBytes * static_cast<size_t>(bone_count);
}

/** What one frame OCCUPIES on the page's 64-byte read grid. See the header:
 *  this is spacing, not content. */
inline constexpr size_t frame_stride(int bone_count) {
  const size_t q = kQuatBytes * static_cast<size_t>(bone_count);
  const size_t qpad = (q % kLineBytes == 0) ? q : q + (kLineBytes - q % kLineBytes);
  return kFrameHeaderBytes + qpad;
}

/** Page-header byte offsets, frozen. */
inline constexpr size_t kOffMagic = 0;
inline constexpr size_t kOffVersion = 4;
inline constexpr size_t kOffBoneCount = 6;
inline constexpr size_t kOffHdrRsv0 = 7;
inline constexpr size_t kOffClipCount = 8;
inline constexpr size_t kOffHdrRsv1 = 10;
inline constexpr size_t kOffDirOff = 12;
inline constexpr size_t kOffFramesOff = 16;
/** v2's owner word. ONE ALIGNED LITTLE-ENDIAN u32 for a SEMANTIC u24: bits
 *  23:0 are the owner's MESH_STREAM form index, bits 31:24 MUST be zero. The
 *  word sits in EXISTING header padding -- the header was and remains 64
 *  bytes, and no clip record, frame stride or offset moves. */
inline constexpr size_t kOffOwnerForm = 20;

/** The handle index is 24 bits (`handle32` = {index[31:8], generation[7:0]}),
 *  the same mask `zref::creature_page::kFormIndexMask` states for the ladder.
 *
 *  INDEX ZERO IS NOT A SENTINEL. The ruling is explicit: "Index zero must not
 *  be used as an implicit 'owner absent' sentinel: presence follows version
 *  and validated section presence, not the numeric value of the index." So a
 *  bank owned by form 0 is an ordinary bank and is compared like any other. */
inline constexpr uint32_t kOwnerFormMask = 0x00FFFFFFu;

/** Clip-directory record byte offsets, frozen. The first three fields are
 *  creature_rules 5's own `{slot_id u16, frame_count u16, event_count u16}`,
 *  in that order and at offset zero, deliberately. */
inline constexpr size_t kOffSlotId = 0;
inline constexpr size_t kOffFrameCount = 2;
inline constexpr size_t kOffEventCount = 4;
inline constexpr size_t kOffFrameOff = 8;
inline constexpr size_t kOffEventOff = 12;
inline constexpr size_t kOffEventBytes = 16;

/** One quantized rotation. Lanes are qformats 7.6 amendment C1: four s16,
 *  S 1.0.14, hemisphere-canonical, in w,x,y,z order -- which is byte for byte
 *  `zhao_geom_bonesrc`'s 64-bit quat word. */
struct Quat16 {
  int16_t w = 16384, x = 0, y = 0, z = 0;
};

/** One authored key. `root` is fx16 (Q16.16) world metres. */
struct Frame {
  int32_t root_dx = 0, root_dy = 0, root_dz = 0;
  std::vector<Quat16> bones;
};

/** One clip slot. `event_bytes` are carried through OPAQUELY: this model
 *  records their offset and length and never interprets one, because event
 *  tags are sim-side (creature_rules 2.1/4.2) and are not part of R90's lift. */
struct Clip {
  uint16_t slot_id = 0;
  uint16_t event_count = 0;
  std::vector<Frame> frames;
  std::vector<uint8_t> event_bytes;
};

enum class Verdict : uint8_t {
  kOk = 0,
  kBadMagic = 1,
  kBadVersion = 2,
  kTruncated = 3,       //!< a declared extent runs past the page
  kBadBoneCount = 4,    //!< 0, or past creature_rules 1.2's ceiling
  kBadClipCount = 5,    //!< 0, or more clips than the bank has rows
  kBadFrameCount = 6,   //!< a clip declaring zero frames
  kMisaligned = 7,      //!< a frame or event extent off the 64-byte read grid
  kDuplicateSlot = 8,   //!< two directory rows for one slot id: the bank
                        //!< answers the FIRST match, so the page's meaning
                        //!< would depend on row order
  kReservedNz = 9,      //!< a reserved field is not zero
  kBadOwner = 10,       //!< the owner word's bits 31:24 are not zero. NOT a
                        //!< statement about WHICH form owns the bank -- that
                        //!< comparison belongs to the loader, which has the
                        //!< request to compare against; this is the FORMAT
                        //!< rule that keeps the 24-bit semantic value from
                        //!< being read out of 32 stored bits.
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
inline void put_u16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFFu);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
}
inline uint16_t get_u16(const uint8_t* p) {
  return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) |
                               (static_cast<uint16_t>(p[1]) << 8));
}
inline void put_i32(uint8_t* p, int32_t v) { put_u32(p, static_cast<uint32_t>(v)); }
inline int32_t get_i32(const uint8_t* p) { return static_cast<int32_t>(get_u32(p)); }
inline void put_i16(uint8_t* p, int16_t v) { put_u16(p, static_cast<uint16_t>(v)); }
inline int16_t get_i16(const uint8_t* p) { return static_cast<int16_t>(get_u16(p)); }

inline size_t round_up_line(size_t n) {
  return (n % kLineBytes == 0) ? n : n + (kLineBytes - n % kLineBytes);
}

/**
 * The legality the loader enforces, in ONE place so the packer, this model and
 * the RTL cannot disagree about it. `rows` is the reader's directory capacity;
 * a bank declaring more clips than that is refused WHOLE rather than truncated,
 * because a reader keeping the first `rows` would answer some lookups from this
 * page and others from the last one.
 */
inline bool bank_legal(const std::vector<Clip>& clips, int bone_count,
                       uint32_t owner_form_index, int rows, Verdict* why) {
  const auto no = [&](Verdict v) { if (why) *why = v; return false; };
  if ((owner_form_index & ~kOwnerFormMask) != 0u) return no(Verdict::kBadOwner);
  if (bone_count <= 0 || bone_count > kMaxBones) return no(Verdict::kBadBoneCount);
  if (clips.empty() || static_cast<int>(clips.size()) > rows ||
      static_cast<int>(clips.size()) > kMaxClips)
    return no(Verdict::kBadClipCount);
  for (size_t i = 0; i < clips.size(); ++i) {
    if (clips[i].frames.empty()) return no(Verdict::kBadFrameCount);
    for (const Frame& f : clips[i].frames)
      if (static_cast<int>(f.bones.size()) != bone_count)
        return no(Verdict::kBadBoneCount);
    for (size_t j = 0; j < i; ++j)
      if (clips[j].slot_id == clips[i].slot_id) return no(Verdict::kDuplicateSlot);
  }
  if (why) *why = Verdict::kOk;
  return true;
}

/**
 * Build a bank page. Empty on any refusal `bank_legal` makes -- a page this
 * packer will not emit is emitted as nothing, never as a page with a hole in
 * it (`spec/cartridge.md` 4's "deterministic refusal, never a guessed layout").
 */
inline std::vector<uint8_t> build(const std::vector<Clip>& clips, int bone_count,
                                  uint32_t owner_form_index,
                                  int rows = kMaxClips) {
  Verdict why = Verdict::kOk;
  if (!bank_legal(clips, bone_count, owner_form_index, rows, &why)) return {};

  const size_t stride = frame_stride(bone_count);
  const size_t dir_off = kHeaderBytes;
  const size_t frames_off = round_up_line(dir_off + kClipBytes * clips.size());

  // Lay the clips out: every clip's frames, then that clip's event bytes, each
  // starting on a line boundary.
  std::vector<size_t> frame_off(clips.size()), event_off(clips.size());
  size_t cur = frames_off;
  for (size_t i = 0; i < clips.size(); ++i) {
    frame_off[i] = cur;
    cur += stride * clips[i].frames.size();
    if (clips[i].event_bytes.empty()) {
      event_off[i] = 0;
    } else {
      event_off[i] = cur;
      cur += round_up_line(clips[i].event_bytes.size());
    }
  }

  std::vector<uint8_t> page(round_up_line(cur), 0u);
  put_u32(page.data() + kOffMagic, kMagic);
  put_u16(page.data() + kOffVersion, kVersion);
  page[kOffBoneCount] = static_cast<uint8_t>(bone_count);
  put_u16(page.data() + kOffClipCount, static_cast<uint16_t>(clips.size()));
  put_u32(page.data() + kOffDirOff, static_cast<uint32_t>(dir_off));
  put_u32(page.data() + kOffFramesOff, static_cast<uint32_t>(frames_off));
  // The owner is SUPPLIED, never inferred. There is nothing in a bank's own
  // bytes that could name its creature -- the ruling forbids deriving it from
  // bone count, row order or publication index, and this packer has no way to
  // do so even if it were allowed to.
  put_u32(page.data() + kOffOwnerForm, owner_form_index & kOwnerFormMask);

  for (size_t i = 0; i < clips.size(); ++i) {
    uint8_t* d = page.data() + dir_off + kClipBytes * i;
    put_u16(d + kOffSlotId, clips[i].slot_id);
    put_u16(d + kOffFrameCount, static_cast<uint16_t>(clips[i].frames.size()));
    put_u16(d + kOffEventCount, clips[i].event_count);
    put_u32(d + kOffFrameOff, static_cast<uint32_t>(frame_off[i]));
    put_u32(d + kOffEventOff, static_cast<uint32_t>(event_off[i]));
    put_u32(d + kOffEventBytes,
            static_cast<uint32_t>(clips[i].event_bytes.size()));

    for (size_t f = 0; f < clips[i].frames.size(); ++f) {
      const Frame& fr = clips[i].frames[f];
      uint8_t* s = page.data() + frame_off[i] + stride * f;
      put_i32(s + 0, fr.root_dx);
      put_i32(s + 4, fr.root_dy);
      put_i32(s + 8, fr.root_dz);
      uint8_t* q = s + kFrameHeaderBytes;
      for (int b = 0; b < bone_count; ++b) {
        put_i16(q + kQuatBytes * static_cast<size_t>(b) + 0, fr.bones[b].w);
        put_i16(q + kQuatBytes * static_cast<size_t>(b) + 2, fr.bones[b].x);
        put_i16(q + kQuatBytes * static_cast<size_t>(b) + 4, fr.bones[b].y);
        put_i16(q + kQuatBytes * static_cast<size_t>(b) + 6, fr.bones[b].z);
      }
    }
    if (!clips[i].event_bytes.empty())
      for (size_t k = 0; k < clips[i].event_bytes.size(); ++k)
        page[event_off[i] + k] = clips[i].event_bytes[k];
  }
  return page;
}

/**
 * Decode a page exactly as a reader must. `out` is left EMPTY on any verdict
 * but kOk -- a refused page is refused whole, which is what keeps a bank from
 * holding four frames of one author's clip and four of another's.
 */
inline Verdict decode(const uint8_t* page, size_t bytes, int rows,
                      std::vector<Clip>& out, int* bone_count_o = nullptr,
                      uint32_t* owner_form_index_o = nullptr) {
  out.clear();
  if (bone_count_o) *bone_count_o = 0;
  if (owner_form_index_o) *owner_form_index_o = 0;
  if (bytes < kHeaderBytes) return Verdict::kTruncated;
  if (get_u32(page + kOffMagic) != kMagic) return Verdict::kBadMagic;
  // A v1 bank is REFUSED here, not upgraded. An offline conversion needs a
  // SUPPLIED, validated owner; it cannot discover one from the old bytes.
  if (get_u16(page + kOffVersion) != kVersion) return Verdict::kBadVersion;
  if (page[kOffHdrRsv0] != 0u) return Verdict::kReservedNz;
  if (get_u16(page + kOffHdrRsv1) != 0u) return Verdict::kReservedNz;
  const uint32_t owner_w = get_u32(page + kOffOwnerForm);
  if ((owner_w & ~kOwnerFormMask) != 0u) return Verdict::kBadOwner;

  const int bone_count = static_cast<int>(page[kOffBoneCount]);
  if (bone_count <= 0 || bone_count > kMaxBones) return Verdict::kBadBoneCount;
  const int clip_count = static_cast<int>(get_u16(page + kOffClipCount));
  if (clip_count <= 0 || clip_count > kMaxClips) return Verdict::kBadClipCount;
  if (clip_count > rows) return Verdict::kBadClipCount;

  const size_t dir_off = get_u32(page + kOffDirOff);
  const size_t frames_off = get_u32(page + kOffFramesOff);
  if (dir_off != kHeaderBytes) return Verdict::kMisaligned;
  if (frames_off % kLineBytes != 0u) return Verdict::kMisaligned;
  if (dir_off + kClipBytes * static_cast<size_t>(clip_count) > bytes)
    return Verdict::kTruncated;
  if (frames_off < dir_off + kClipBytes * static_cast<size_t>(clip_count))
    return Verdict::kMisaligned;

  const size_t stride = frame_stride(bone_count);
  std::vector<Clip> staged;
  for (int i = 0; i < clip_count; ++i) {
    const uint8_t* d = page + dir_off + kClipBytes * static_cast<size_t>(i);
    Clip c;
    c.slot_id = get_u16(d + kOffSlotId);
    const size_t fcount = get_u16(d + kOffFrameCount);
    c.event_count = get_u16(d + kOffEventCount);
    if (get_u16(d + kOffSlotId + 6) != 0u) return Verdict::kReservedNz;
    for (size_t k = 20; k < kClipBytes; ++k)
      if (d[k] != 0u) return Verdict::kReservedNz;
    if (fcount == 0u) return Verdict::kBadFrameCount;

    const size_t foff = get_u32(d + kOffFrameOff);
    const size_t eoff = get_u32(d + kOffEventOff);
    const size_t ebytes = get_u32(d + kOffEventBytes);
    if (foff % kLineBytes != 0u || foff < frames_off) return Verdict::kMisaligned;
    if (foff + stride * fcount > bytes) return Verdict::kTruncated;
    if (ebytes != 0u) {
      if (eoff % kLineBytes != 0u || eoff < frames_off) return Verdict::kMisaligned;
      if (eoff + ebytes > bytes) return Verdict::kTruncated;
      c.event_bytes.assign(page + eoff, page + eoff + ebytes);
    } else if (eoff != 0u) {
      return Verdict::kMisaligned;
    }

    for (size_t f = 0; f < fcount; ++f) {
      const uint8_t* s = page + foff + stride * f;
      Frame fr;
      fr.root_dx = get_i32(s + 0);
      fr.root_dy = get_i32(s + 4);
      fr.root_dz = get_i32(s + 8);
      for (size_t k = kRootBytes; k < kFrameHeaderBytes; ++k)
        if (s[k] != 0u) return Verdict::kReservedNz;
      const uint8_t* q = s + kFrameHeaderBytes;
      for (int b = 0; b < bone_count; ++b) {
        Quat16 t;
        t.w = get_i16(q + kQuatBytes * static_cast<size_t>(b) + 0);
        t.x = get_i16(q + kQuatBytes * static_cast<size_t>(b) + 2);
        t.y = get_i16(q + kQuatBytes * static_cast<size_t>(b) + 4);
        t.z = get_i16(q + kQuatBytes * static_cast<size_t>(b) + 6);
        fr.bones.push_back(t);
      }
      c.frames.push_back(fr);
    }
    for (const Clip& prev : staged)
      if (prev.slot_id == c.slot_id) return Verdict::kDuplicateSlot;
    staged.push_back(c);
  }
  out.swap(staged);
  if (bone_count_o) *bone_count_o = bone_count;
  if (owner_form_index_o) *owner_form_index_o = owner_w;
  return Verdict::kOk;
}

/** The header's declared version, WITHOUT judging the page. This is how a v1
 *  fixture stays identifiable after `decode` starts refusing it. */
inline uint16_t page_version(const uint8_t* page, size_t bytes) {
  if (bytes < kHeaderBytes) return 0;
  return get_u16(page + kOffVersion);
}

/** The owner word AS STORED, high byte included, without judging it. The RTL
 *  reader is differenced against this rather than against a recomputation. */
inline uint32_t page_owner_word(const uint8_t* page, size_t bytes) {
  if (bytes < kHeaderBytes) return 0;
  return get_u32(page + kOffOwnerForm);
}

/**
 * The reader's lookup: the FIRST directory row whose `slot_id` matches. A miss
 * is a miss -- never slot zero and never the nearest, for
 * `zref_creature_page.hpp`'s reason one page kind over: a clip played from
 * another slot's frames is a pose the oracle never computes and nothing
 * downstream can tell that it is.
 */
inline bool find_clip(const std::vector<Clip>& bank, uint16_t slot_id,
                      Clip const** out) {
  for (const Clip& c : bank) {
    if (c.slot_id == slot_id) { *out = &c; return true; }
  }
  return false;
}

/**
 * The byte offset of frame `f` of the clip in directory row `i`, read from the
 * page itself rather than recomputed from a layout rule. This is what an RTL
 * reader's address arithmetic must agree with, so it is stated once here and
 * differenced in the directed test.
 */
inline bool frame_offset(const uint8_t* page, size_t bytes, int row, size_t f,
                         size_t* off_o) {
  if (bytes < kHeaderBytes) return false;
  const int bone_count = static_cast<int>(page[kOffBoneCount]);
  if (bone_count <= 0 || bone_count > kMaxBones) return false;
  const int clip_count = static_cast<int>(get_u16(page + kOffClipCount));
  if (row < 0 || row >= clip_count) return false;
  const uint8_t* d = page + get_u32(page + kOffDirOff) +
                     kClipBytes * static_cast<size_t>(row);
  if (f >= get_u16(d + kOffFrameCount)) return false;
  *off_o = get_u32(d + kOffFrameOff) + frame_stride(bone_count) * f;
  return true;
}

}  // namespace clip_page
}  // namespace zref
