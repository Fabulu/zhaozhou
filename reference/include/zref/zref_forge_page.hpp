// zref_forge_page.hpp -- the FORGE_PROGRAM page, frozen.
//
// Owner decision R234 D2 (reports/OWNER-RULINGS-20260919-EVENING.md,
// 2026-09-21, `(owner, explicit)`): *"R199 IS REVERSED by this decision. I
// deferred the forge program page kind because four of six families have no
// evaluator; the owner has chosen to pay for the evaluators rather than accept
// the deferral. The page kind is to be frozen and FORGE.PRIM / FORGE.PRIM_EVAL
// built."*
//
// This header IS that layout. `spec/cartridge.md` 4d states it normatively,
// `tools/pack/mkforgeprogram.py` emits it, and the committed golden
// `tests/golden/forge_program/forge_page_v1.bin` pins all three to one artefact
// rather than to each other's good intentions -- the discipline R26's ladder
// table established and `tools/pack/mkcreatureladder.py` documents.
//
// ---------------------------------------------------------------------------
// WHAT A FORGE PROGRAM IS, AND WHY IT IS ONE RECORD
// ---------------------------------------------------------------------------
// `DrawProcedural 0x0302` carries `handle32[forge_program] program` and says,
// in as many words: *"forge parameters do NOT travel inline"*. Until today that
// handle named a resource type occurring in exactly two places in the whole
// tree -- that command and its generated ABI table -- so the draw arm pointed
// at a format nobody had written.
//
// A forge program is ONE primitive: its TOPOLOGY (which family, how finely
// subdivided) and its POSITIONS (the anchors, axes and radii the evaluators
// place vertices from). `zhao_forge_prim` owns the first and
// `zhao_forge_prim_eval` / `zhao_forge_ring_eval` own the second, and they are
// two halves of one meshlet rather than two stages of a chain -- they are
// joined only by the ring-major ordering convention. So the page carries BOTH
// halves in one record, because splitting them across two pages would create a
// second mapping law between them for no gain.
//
// ---------------------------------------------------------------------------
// THE KEY IS THE HANDLE INDEX, NOT AN INVENTED PROGRAM ID
// ---------------------------------------------------------------------------
// `DrawProcedural.program` is a `handle32`, and a handle32 is
// {index[31:8], generation[7:0]}. The record key is that INDEX and nothing
// else -- the same decision R26 took for the ladder table's `form_index` and
// R45 took for the stamp's patch. A second id space would need a second
// mapping law, and the two would then be able to disagree.
//
// ---------------------------------------------------------------------------
// THE FAMILY IS IN THE PAGE, AND THE COMMAND'S `kind` MUST AGREE
// ---------------------------------------------------------------------------
// A ribbon's parameters and a tube's parameters are not the same parameters, so
// a reader cannot interpret a record at all without knowing its family: the
// family belongs in the page. `DrawProcedural.kind` then declares it a second
// time, and the two are authored in different places -- the page by the packer,
// the command by the game's own draw -- so comparing them is a REAL check and
// not two operands moving together.
//
// **RULED HERE: the PAGE's `family` governs, and a `kind` that disagrees
// REFUSES the draw and is counted.** Never substitute, never prefer one --
// FORGE.PRIM.md's own refusal law for an unknown family, applied to a
// disagreement about a known one.
//
// AND THE COMPARISON IS A ROTATION, WHICH IS THE TRAP `spec/commands.zidl`
// SHOUTS ABOUT. That file says in capitals that `forge_kind` **is not**
// `zhao_forge_prim`'s `j_family_i` encoding: member 0 there is frozen by the
// v2 pad-byte precedent and cannot be renumbered, so
//
//     forge_kind = (family + 1) mod 6
//
// and *"a straight-through assignment is silently wrong for all six values"*.
// `kind_of_family` and `family_of_kind` below are the ONE place that conversion
// is written, and `tests/forge/forge_page_directed.cpp` walks all six both
// ways. Whoever wires `DrawProcedural` to a job calls these; it does not write
// the arithmetic again.
//
// ---------------------------------------------------------------------------
// WHY EVERY NUMBER IS 64-BYTE SHAPED
// ---------------------------------------------------------------------------
// MEM.GUARD's read is at most 64 bytes and its shape rule requires the byte
// mask to match the length, so the cheapest honest reader is one that asks for
// whole 64-byte lines -- `zref::species_page` and `zref::creature_page` both
// say so and this is deliberately the same discipline rather than a third one.
//
// Those two pages chose a 32-byte record so TWO fit a line. A forge program
// does not fit in 32 bytes and no amount of packing will make it: the ribbon
// alone needs five fx16 three-vectors, four fx16 scalars, a seed, a phase and
// two branch descriptors. So a record is **192 bytes = THREE whole lines**.
// The property that matters is not "two per line", it is **no record ever
// straddles a read**, and any multiple of 64 has it.
//
// ---------------------------------------------------------------------------
// A REFUSED PAGE IS REFUSED WHOLE
// ---------------------------------------------------------------------------
// Magic, version and the record count against the declared extent are judged
// from the header line before any record is stored; beyond that an illegal
// record takes its page with it. A bank holding three programs of one author's
// effect and the rest of another's reads as a tuning problem and is not one --
// `zref::species_page`'s sentence, and it is true for the same reason.
//
// ---------------------------------------------------------------------------
// TWO DECLARED HOLES, NAMED SO THEY ARE NOT MISTAKEN FOR OVERSIGHTS
// ---------------------------------------------------------------------------
// 1. **A RIBBON'S TWO RADII MUST BE EQUAL.** `zhao_forge_prim_eval`'s job
//    carries ONE `half_width`, so a tapering ribbon is a capability the
//    evaluator does not have. The page REFUSES `radius0 != radius1` on
//    FAM_RIBBON rather than carrying a number nothing reads. When a tapering
//    ribbon is built, the refusal lifts and not one byte moves.
//
// 2. **`tick_phase_base` IS A BASE, NOT THE LIVE PHASE.** A cartridge page is
//    immutable and uploaded once; a lightning bolt animates per frame. So the
//    evaluator's `tick_phase` is `tick_phase_base + frame_tick`, and
//    `frame_tick` is sourced at DISPATCH, not from here. Where the dispatch
//    gets it is an open question recorded for the owner -- `DrawProcedural`'s
//    `pad[11]` could carry it under the same mandatory-zero reinterpretation
//    `forge_kind` itself used, or the console could broadcast its frame
//    sequence. **This file does not decide it**, and with `frame_tick == 0` the
//    page alone is a complete, deterministic, static primitive.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace zref {
namespace forge_page {

/** `spec/cartridge.md` 4's page-kind registry: FORGE_PROGRAM is 14. */
inline constexpr uint8_t kPageKind = 14;

/** `spec/cartridge.md` 2's section-type table: FORGE_PROGRAM is 0x0012. */
inline constexpr uint16_t kSectionType = 0x0012;

inline constexpr uint32_t kMagic = 0x4746505Au;  // 'Z','F','P','G' little-endian
inline constexpr uint16_t kVersion = 1;
inline constexpr size_t kHeaderBytes = 64;
inline constexpr size_t kRecordBytes = 192;  // THREE MEM.GUARD lines

/** The handle index is 24 bits (`handle32` = {index[31:8], generation[7:0]}). */
inline constexpr uint32_t kProgramIndexMask = 0x00FFFFFFu;

// ---- the silicon family encoding, `zhao_forge_prim.sv`'s FAM_* -------------
// NOT `forge_kind`. See the rotation note above.
enum Family : uint8_t {
  kFamRibbon = 0,
  kFamFan = 1,
  kFamTube = 2,
  kFamShell = 3,
  kFamBillboard = 4,
  kFamCliff = 5,
};
inline constexpr uint8_t kFamilyCount = 6;

/** The sweep law the ring evaluator applies between anchor0 and anchor1. */
enum Sweep : uint8_t {
  /** Centre and radius both lerp linearly. Cones, tubes, fans, sheets. */
  kSweepLinear = 0,
  /** Centre and radius follow a quarter wave -- a dome or bowl. SHELL only. */
  kSweepDome = 1,
};
inline constexpr uint8_t kSweepCount = 2;

// ---- the frozen limits, restated from the contracts ------------------------
inline constexpr int kMaxSegments = 64;        // FORGE.PRIM.md, frozen
inline constexpr int kMaxSides = 8;            // FORGE.PRIM.md, frozen
inline constexpr int kMaxRibbonSegments = 24;  // FORGE.PRIM.EVAL.md, the owner's bound
inline constexpr int kMaxBranches = 2;
inline constexpr int kMaxBranchSegments = 8;

// ---- byte offsets inside a record, frozen ----------------------------------
// Line 0 -- identity, topology and the common frame.
inline constexpr size_t kOffProgramIndex = 0;
inline constexpr size_t kOffFamily = 4;
inline constexpr size_t kOffSweep = 5;
inline constexpr size_t kOffSegments = 6;
inline constexpr size_t kOffSides = 7;
inline constexpr size_t kOffViewMask = 8;
inline constexpr size_t kOffBranchCount = 9;
inline constexpr size_t kOffSrcId = 10;
inline constexpr size_t kOffRsv0 = 12;
inline constexpr size_t kOffAnchor0 = 16;
inline constexpr size_t kOffAnchor1 = 28;
inline constexpr size_t kOffAxisU = 40;
inline constexpr size_t kOffAxisV = 52;
// Line 1 -- radii and the ribbon's jitter law.
inline constexpr size_t kOffRadius0 = 64;
inline constexpr size_t kOffRadius1 = 68;
inline constexpr size_t kOffAmp = 72;
inline constexpr size_t kOffBranchAmp = 76;
inline constexpr size_t kOffBranchRadius = 80;
inline constexpr size_t kOffSeed = 84;
inline constexpr size_t kOffTickPhaseBase = 88;
inline constexpr size_t kOffRsv1 = 90;
inline constexpr size_t kOffAxisW = 92;
inline constexpr size_t kOffRsv2 = 104;  // 24 bytes
// Line 2 -- the ribbon's branches.
inline constexpr size_t kOffBr0Attach = 128;
inline constexpr size_t kOffBr0Segments = 129;
inline constexpr size_t kOffRsv3 = 130;
inline constexpr size_t kOffBr0End = 132;
inline constexpr size_t kOffBr1Attach = 144;
inline constexpr size_t kOffBr1Segments = 145;
inline constexpr size_t kOffRsv4 = 146;
inline constexpr size_t kOffBr1End = 148;
inline constexpr size_t kOffRsv5 = 160;  // 32 bytes

/** fx16 three-vector, the geometry path's S15.16. */
struct Vec3 {
  int32_t x = 0, y = 0, z = 0;
};

/** One branch descriptor. Inspected only for `b < branch_count`. */
struct Branch {
  uint8_t attach = 0;    //!< 0..segments, the main point it grows from
  uint8_t segments = 0;  //!< 1..kMaxBranchSegments
  Vec3 end;              //!< fx16 anchor
};

/**
 * One forge program: a complete primitive, topology and positions.
 *
 * The three axes are a FRAME and are used as supplied -- caller-normalised,
 * exactly as FORGE.PRIM.EVAL.md rules ("deriving a unit frame in hardware
 * needs a square root and a divider this block has no business owning").
 * The ring families use `axis_u` and `axis_v`; the ribbon uses all three,
 * `axis_u` as its width axis and `axis_v`/`axis_w` as its two jitter axes.
 */
struct Record {
  uint32_t program_index = 0;  //!< handle32 index, bits 23:0
  uint8_t family = kFamRibbon;
  uint8_t sweep = kSweepLinear;
  uint8_t segments = 1;    //!< 1..kMaxSegments (1..kMaxRibbonSegments on ribbon)
  uint8_t sides = 1;       //!< 1..kMaxSides
  uint8_t view_mask = 3;   //!< bits 1:0
  uint8_t branch_count = 0;
  uint16_t src_id = 0;

  Vec3 anchor0;  //!< the centre of ring 0
  Vec3 anchor1;  //!< the centre of ring N
  Vec3 axis_u;   //!< ring U axis / ribbon width axis
  Vec3 axis_v;   //!< ring V axis / ribbon jitter axis 1

  int32_t radius0 = 0;        //!< fx16, R(0); ribbon half width
  int32_t radius1 = 0;        //!< fx16, R(N); MUST equal radius0 on a ribbon
  int32_t amp = 0;            //!< fx16, ribbon jitter amplitude
  int32_t branch_amp = 0;     //!< fx16
  int32_t branch_radius = 0;  //!< fx16, branch half width
  uint32_t seed = 0;
  uint16_t tick_phase_base = 0;

  Vec3 axis_w;   //!< ribbon jitter axis 2 (perp2); zero on every other family
  Branch br[2];
};

// ---------------------------------------------------------------------------
// THE ROTATION. The ONE place `forge_kind` and FAM_* are converted.
// ---------------------------------------------------------------------------

/** `forge_kind` for a silicon family. Returns 0xFF for an illegal family. */
inline uint8_t kind_of_family(uint8_t family) {
  if (family >= kFamilyCount) return 0xFFu;
  return static_cast<uint8_t>((family + 1u) % kFamilyCount);
}

/** The silicon family for a `forge_kind`. Returns 0xFF for an illegal kind. */
inline uint8_t family_of_kind(uint8_t kind) {
  if (kind >= kFamilyCount) return 0xFFu;
  return static_cast<uint8_t>((kind + kFamilyCount - 1u) % kFamilyCount);
}

// ---------------------------------------------------------------------------
// LEGALITY. A record that fails ANY of these refuses its whole page.
// ---------------------------------------------------------------------------

/** True when this family carries the ribbon's jitter law. */
inline bool family_is_ribbon(uint8_t family) { return family == kFamRibbon; }

/**
 * True when the family's ring closes on itself -- `zhao_forge_prim`'s
 * `closed_c`, restated. An open family's ring is the width-axis PAIR and its
 * `sides` must therefore be 1.
 */
inline bool family_ring_closed(uint8_t family) {
  return family == kFamTube || family == kFamShell || family == kFamFan;
}

/**
 * Why a record is illegal, or `kOk`. The order is the order a reader applies
 * it, so hardware and model refuse for the SAME reason and not merely at the
 * same time -- a counter that cannot say which rule fired is a counter that
 * cannot discriminate (R95).
 */
enum Illegality : int {
  kOk = 0,
  kBadIndex = 1,        //!< high byte of program_index nonzero
  kBadFamily = 2,       //!< family > 5
  kBadSweep = 3,        //!< sweep > 1, or DOME on a family that is not SHELL
  kBadSegments = 4,     //!< 0, or past the family's cap
  kBadSides = 5,        //!< 0, past 8, or != 1 on an open family
  kBadViewMask = 6,     //!< 0, or a bit above bit 1 set
  kBadRadius = 7,       //!< a negative radius, or a ribbon whose radii differ
  kBadBranch = 8,       //!< branch_count, a branch's segments or its attach
  kBadRibbonOnly = 9,   //!< a ribbon-only field nonzero on another family
  kBadReserved = 10,    //!< a reserved byte nonzero
};

inline Illegality record_legal(const Record& r) {
  if ((r.program_index & ~kProgramIndexMask) != 0u) return kBadIndex;
  if (r.family >= kFamilyCount) return kBadFamily;
  if (r.sweep >= kSweepCount) return kBadSweep;
  // DOME is the radial shell's own sweep. Allowing it elsewhere would make
  // `sweep` a second, silent family selector.
  if (r.sweep == kSweepDome && r.family != kFamShell) return kBadSweep;

  const int seg_cap = family_is_ribbon(r.family) ? kMaxRibbonSegments : kMaxSegments;
  if (r.segments < 1 || r.segments > seg_cap) return kBadSegments;

  if (r.sides < 1 || r.sides > kMaxSides) return kBadSides;
  // An OPEN family's ring is the two width-axis vertices and nothing else, so
  // a `sides` above 1 there is a caller describing a mesh the topology walker
  // will not build. FORGE.PRIM refuses rather than clamps; so does this.
  if (!family_ring_closed(r.family) && r.sides != 1) return kBadSides;

  if (r.view_mask == 0 || (r.view_mask & 0xFCu) != 0u) return kBadViewMask;

  if (r.radius0 < 0 || r.radius1 < 0) return kBadRadius;
  if (family_is_ribbon(r.family) && r.radius0 != r.radius1) return kBadRadius;

  if (r.branch_count > kMaxBranches) return kBadBranch;
  for (int b = 0; b < r.branch_count; ++b) {
    if (r.br[b].segments < 1 || r.br[b].segments > kMaxBranchSegments) return kBadBranch;
    if (r.br[b].attach > r.segments) return kBadBranch;
  }

  if (!family_is_ribbon(r.family)) {
    // The ribbon's law is the ribbon's. A tube carrying a seed and an
    // amplitude is a page whose author believed something untrue about it, and
    // shipping it quietly is how that belief survives to the next reader.
    if (r.branch_count != 0 || r.amp != 0 || r.branch_amp != 0 || r.branch_radius != 0 ||
        r.seed != 0u || r.tick_phase_base != 0u)
      return kBadRibbonOnly;
    if (r.axis_w.x != 0 || r.axis_w.y != 0 || r.axis_w.z != 0) return kBadRibbonOnly;
    if (r.br[0].attach != 0 || r.br[0].segments != 0 || r.br[1].attach != 0 ||
        r.br[1].segments != 0)
      return kBadRibbonOnly;
  }
  return kOk;
}

/** How many vertices a program's topology grid holds: rings x ring vertices. */
inline int record_vertex_count(const Record& r) {
  const int eff_seg = (r.family == kFamFan || r.family == kFamBillboard) ? 1 : r.segments;
  const int ring = family_ring_closed(r.family) ? r.sides : 2;
  return (eff_seg + 1) * ring;
}

// ---- little-endian scalar helpers -----------------------------------------

inline void put_u16(std::vector<uint8_t>& b, size_t off, uint16_t v) {
  b[off + 0] = static_cast<uint8_t>(v & 0xFFu);
  b[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
}
inline void put_u32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
  b[off + 0] = static_cast<uint8_t>(v & 0xFFu);
  b[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
  b[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
  b[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
}
inline void put_i32(std::vector<uint8_t>& b, size_t off, int32_t v) {
  put_u32(b, off, static_cast<uint32_t>(v));
}
inline void put_vec3(std::vector<uint8_t>& b, size_t off, const Vec3& v) {
  put_i32(b, off + 0, v.x);
  put_i32(b, off + 4, v.y);
  put_i32(b, off + 8, v.z);
}
inline uint16_t get_u16(const uint8_t* b, size_t off) {
  return static_cast<uint16_t>(b[off] | (static_cast<uint16_t>(b[off + 1]) << 8));
}
inline uint32_t get_u32(const uint8_t* b, size_t off) {
  return static_cast<uint32_t>(b[off]) | (static_cast<uint32_t>(b[off + 1]) << 8) |
         (static_cast<uint32_t>(b[off + 2]) << 16) | (static_cast<uint32_t>(b[off + 3]) << 24);
}
inline int32_t get_i32(const uint8_t* b, size_t off) {
  return static_cast<int32_t>(get_u32(b, off));
}
inline Vec3 get_vec3(const uint8_t* b, size_t off) {
  Vec3 v;
  v.x = get_i32(b, off + 0);
  v.y = get_i32(b, off + 4);
  v.z = get_i32(b, off + 8);
  return v;
}

// ---------------------------------------------------------------------------
// BUILD -- the packer's emission, and the test's expectation, one function.
// ---------------------------------------------------------------------------

/**
 * Serialise a page. Returns an empty vector if any record is illegal or two
 * records share a program index -- a page whose meaning depends on row order
 * is not a page, it is a race (`mkcreatureladder.py`'s duplicate rule).
 */
inline std::vector<uint8_t> build(const std::vector<Record>& recs) {
  for (size_t i = 0; i < recs.size(); ++i) {
    if (record_legal(recs[i]) != kOk) return {};
    for (size_t j = 0; j < i; ++j)
      if (recs[j].program_index == recs[i].program_index) return {};
  }
  if (recs.size() > 0xFFFFu) return {};

  std::vector<uint8_t> b(kHeaderBytes + kRecordBytes * recs.size(), 0u);
  put_u32(b, 0, kMagic);
  put_u16(b, 4, kVersion);
  put_u16(b, 6, static_cast<uint16_t>(recs.size()));
  // bytes 8..63 stay zero

  for (size_t i = 0; i < recs.size(); ++i) {
    const Record& r = recs[i];
    const size_t o = kHeaderBytes + kRecordBytes * i;
    put_u32(b, o + kOffProgramIndex, r.program_index);
    b[o + kOffFamily] = r.family;
    b[o + kOffSweep] = r.sweep;
    b[o + kOffSegments] = r.segments;
    b[o + kOffSides] = r.sides;
    b[o + kOffViewMask] = r.view_mask;
    b[o + kOffBranchCount] = r.branch_count;
    put_u16(b, o + kOffSrcId, r.src_id);
    put_vec3(b, o + kOffAnchor0, r.anchor0);
    put_vec3(b, o + kOffAnchor1, r.anchor1);
    put_vec3(b, o + kOffAxisU, r.axis_u);
    put_vec3(b, o + kOffAxisV, r.axis_v);

    put_i32(b, o + kOffRadius0, r.radius0);
    put_i32(b, o + kOffRadius1, r.radius1);
    put_i32(b, o + kOffAmp, r.amp);
    put_i32(b, o + kOffBranchAmp, r.branch_amp);
    put_i32(b, o + kOffBranchRadius, r.branch_radius);
    put_u32(b, o + kOffSeed, r.seed);
    put_u16(b, o + kOffTickPhaseBase, r.tick_phase_base);
    put_vec3(b, o + kOffAxisW, r.axis_w);

    b[o + kOffBr0Attach] = r.br[0].attach;
    b[o + kOffBr0Segments] = r.br[0].segments;
    put_vec3(b, o + kOffBr0End, r.br[0].end);
    b[o + kOffBr1Attach] = r.br[1].attach;
    b[o + kOffBr1Segments] = r.br[1].segments;
    put_vec3(b, o + kOffBr1End, r.br[1].end);
  }
  return b;
}

// ---------------------------------------------------------------------------
// DECODE -- refuse-whole, in the order a reader applies it.
// ---------------------------------------------------------------------------

enum DecodeStatus : int {
  kDecodeOk = 0,
  kDecodeShort = 1,      //!< fewer bytes than the header needs
  kDecodeBadMagic = 2,
  kDecodeBadVersion = 3,
  kDecodeCountPastEnd = 4,   //!< `records` runs past the declared extent
  kDecodeCapacity = 5,       //!< more records than the reader's row capacity
  kDecodeBadHeaderRsv = 6,   //!< a reserved header byte is nonzero
  kDecodeBadRecordRsv = 7,   //!< a reserved record byte is nonzero
  kDecodeIllegalRecord = 8,  //!< `record_legal` refused one
  kDecodeDuplicate = 9,      //!< two records share a program index
};

/**
 * Decode a page body. `capacity` is the reader's row capacity (hardware's
 * ROWS parameter); pass 0 for "no limit". On any refusal `out` is left
 * untouched -- the page is refused WHOLE and never partially loaded.
 */
inline DecodeStatus decode(const uint8_t* body, size_t len, std::vector<Record>& out,
                           size_t capacity = 0) {
  if (len < kHeaderBytes) return kDecodeShort;
  if (get_u32(body, 0) != kMagic) return kDecodeBadMagic;
  if (get_u16(body, 4) != kVersion) return kDecodeBadVersion;
  for (size_t i = 8; i < kHeaderBytes; ++i)
    if (body[i] != 0u) return kDecodeBadHeaderRsv;

  const size_t n = get_u16(body, 6);
  if (kHeaderBytes + kRecordBytes * n > len) return kDecodeCountPastEnd;
  if (capacity != 0 && n > capacity) return kDecodeCapacity;

  std::vector<Record> tmp;
  tmp.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const uint8_t* p = body + kHeaderBytes + kRecordBytes * i;
    if (get_u32(p, kOffRsv0) != 0u) return kDecodeBadRecordRsv;
    if (get_u16(p, kOffRsv1) != 0u) return kDecodeBadRecordRsv;
    for (size_t j = 0; j < 24; ++j)
      if (p[kOffRsv2 + j] != 0u) return kDecodeBadRecordRsv;
    if (get_u16(p, kOffRsv3) != 0u) return kDecodeBadRecordRsv;
    if (get_u16(p, kOffRsv4) != 0u) return kDecodeBadRecordRsv;
    for (size_t j = 0; j < 32; ++j)
      if (p[kOffRsv5 + j] != 0u) return kDecodeBadRecordRsv;

    Record r;
    r.program_index = get_u32(p, kOffProgramIndex);
    r.family = p[kOffFamily];
    r.sweep = p[kOffSweep];
    r.segments = p[kOffSegments];
    r.sides = p[kOffSides];
    r.view_mask = p[kOffViewMask];
    r.branch_count = p[kOffBranchCount];
    r.src_id = get_u16(p, kOffSrcId);
    r.anchor0 = get_vec3(p, kOffAnchor0);
    r.anchor1 = get_vec3(p, kOffAnchor1);
    r.axis_u = get_vec3(p, kOffAxisU);
    r.axis_v = get_vec3(p, kOffAxisV);
    r.radius0 = get_i32(p, kOffRadius0);
    r.radius1 = get_i32(p, kOffRadius1);
    r.amp = get_i32(p, kOffAmp);
    r.branch_amp = get_i32(p, kOffBranchAmp);
    r.branch_radius = get_i32(p, kOffBranchRadius);
    r.seed = get_u32(p, kOffSeed);
    r.tick_phase_base = get_u16(p, kOffTickPhaseBase);
    r.axis_w = get_vec3(p, kOffAxisW);
    r.br[0].attach = p[kOffBr0Attach];
    r.br[0].segments = p[kOffBr0Segments];
    r.br[0].end = get_vec3(p, kOffBr0End);
    r.br[1].attach = p[kOffBr1Attach];
    r.br[1].segments = p[kOffBr1Segments];
    r.br[1].end = get_vec3(p, kOffBr1End);

    if (record_legal(r) != kOk) return kDecodeIllegalRecord;
    for (size_t j = 0; j < tmp.size(); ++j)
      if (tmp[j].program_index == r.program_index) return kDecodeDuplicate;
    tmp.push_back(r);
  }
  out.swap(tmp);
  return kDecodeOk;
}

/**
 * The bank's answer: the FIRST record whose index matches, or false. `build`
 * and `decode` both refuse duplicates, so "first" and "only" agree -- the
 * lookup is written this way so that a bank loaded by some other path still
 * has one defined answer.
 */
inline bool lookup(const std::vector<Record>& recs, uint32_t program_index, Record& out) {
  for (size_t i = 0; i < recs.size(); ++i) {
    if (recs[i].program_index == (program_index & kProgramIndexMask)) {
      out = recs[i];
      return true;
    }
  }
  return false;
}

/** The index a `handle32` names. */
inline uint32_t index_of_handle(uint32_t handle) { return (handle >> 8) & kProgramIndexMask; }

}  // namespace forge_page
}  // namespace zref
