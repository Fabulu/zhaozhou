// zref_drawjob.hpp -- the DRAW-JOB LAW: how one DrawForm becomes GEOM.MESHFETCH
// jobs. Owner ruling R29 (provisional, coordinator, 2026-09-19):
//
//   "Ratify handle32->desc_addr, format and xform[12] from the existing
//    cartridge/asset page specs. The packet PROPOSES the layout from what those
//    specs already imply, citing each field, and freezes it."
//
// This header IS that freeze, and `fpga/rtl/geometry/zhao_geom_drawjob.sv` is
// its RTL (differential: tests/geometry/geom_drawjob_directed.cpp).
//
// ---------------------------------------------------------------------------
// EVERY JOB FIELD, AND WHERE IT COMES FROM
// ---------------------------------------------------------------------------
// GEOM.MESHFETCH's job is {instance_id, desc_addr, format, generation,
// active_mask, xform[12]} (its RTL ports j_*). Per field:
//
//   instance_id  = the DrawForm record's source_id. GEOM.MESHFETCH.md, Counters:
//                  "Source ids propagate, so a refused descriptor is
//                  attributable to the command that introduced the instance."
//   active_mask  = DrawForm.viewport_mask[1:0] (commands.zidl DrawForm; the
//                  MESHFETCH contract: "the caller drives it ... the same mask
//                  GEOM.PROJECT uses"). A zero mask draws nothing and fetches
//                  nothing (counted `masked`, not refused).
//   desc_addr    = row.base + hdr.desc_offset + 64*i, for meshlet i.
//                  * `row` is the RESIDENCY DIRECTORY row for the form handle's
//                    24-bit index, spec/memory_rules.md 5f.1 ("a published slot
//                    is named by the handle index of the resource it holds";
//                    row {slot, base, extent, kind}), restricted to kind 12
//                    MESH_STREAM (spec/cartridge.md 3/4a: "immutable geometry:
//                    meshlet descriptors, vertex stream, local-index stream,
//                    offsets/counts, format + generation metadata").
//                  * `hdr` is the MESH_STREAM HEADER frozen below -- the
//                    "offsets/counts, format + generation metadata" that
//                    cartridge.md 4a says the page carries, given a byte layout.
//                  * 64 is the frozen descriptor stride (GEOM.MESHFETCH.md,
//                    "64 bytes, 64-byte aligned, so one descriptor is exactly
//                    one aligned burst").
//   format       = hdr.format_id: the descriptor format the stream declares.
//                  The drawer REFUSES a stream whose format this hardware does
//                  not speak (kSupportedFormat), and GEOM.MESHFETCH then refuses
//                  any descriptor that disagrees with its own stream -- "Unknown
//                  value => refuse, never guess" applied at both levels.
//   generation   = hdr.generation, the stream's "generation metadata"
//                  (cartridge.md 4a). A descriptor carrying another stream's
//                  generation is refused by GEOM.MESHFETCH's existing row 2.
//                  The HANDLE's generation is checked against the DIRECTORY's
//                  (low 8 of the 16-bit residency generation), which is
//                  MATERIAL.RESOLVE's law for the same handle32 shape
//                  (zhao_material_resolve.sv, "matches the handle's LOW 8
//                  GENERATION BITS against the table's 16-bit generation").
//   xform[12]    = the INSTANCE TRANSFORM PALETTE row named by the transform
//                  handle's index. The palette's WRITER is GEOM.LOOM's emit
//                  stream {node_index, transform[12]}: GEOM.LOOM.md, Purpose:
//                  "producing instance transforms"; Out: "a 3x4 affine,
//                  row-major, fx16 S15.16". The MESHFETCH RTL's own words: "the
//                  contract's job packet names instance_transform_id. Resolving
//                  an id to a matrix is a PALETTE LOOKUP, and this block does
//                  not own it". DrawForm's reference meaning (language-
//                  semantics.md 5: "transform = world3 + fx16 size";
//                  zref::render::FormTransform) is the ROOT/SCALE special case
//                  of a Loom node, so nothing it can express is lost.
//                  A row never written (or an index past the palette) REFUSES
//                  the draw: an unset matrix is not the identity.
//
// Two SIDEBAND values travel WITH every job, through GEOM.MESHFETCH and
// GEOM.ASSETFETCH beside the meshlet, so no meshlet can be paired with another
// draw's state (the fault core entry I39 refuses by name):
//
//   stream_base  = row.base - ZHAO_RENDER_ASSET_BASE: the page's POOL-RELATIVE
//                  byte base. The descriptor's vertex_offset/index_offset are
//                  PAGE-RELATIVE (GEOM.MESHFETCH.md: "byte offset into the
//                  mesh's vertex stream", and the MESH_STREAM page IS the mesh's
//                  streams), while GEOM.ASSETFETCH takes POOL-RELATIVE offsets
//                  (its m_vertex_offset_i). GEOM.MESHFETCH adds the base, once,
//                  saturating at 0xFFFFFFFF so an overflow reaches ASSETFETCH's
//                  pool-end refusal instead of wrapping onto a legal address.
//   side         = {semantic_weight[7:0], material_set[31:0], raster_state[31:0]}
//                  raster_state = zref::raster_state::compose(DrawForm.flags,
//                  kV1MaterialRaster) -- R28's word. Its material half
//                  [31:2] is 0 for EVERY v1 material by R28 ("no bit of it has a
//                  ratified consumer in v1, so a v1 material writes 0 there"),
//                  so it is the NAMED constant kV1MaterialRaster, owner ruling
//                  R48's precedent (ALPHA_C): when a material format gains a
//                  raster bit, the resolved record replaces the constant at the
//                  same seam. material_set and semantic_weight ride beside it to
//                  their consumers (MATERIAL.RESOLVE's request, core entry I49;
//                  the Measure policy, MEASURE.GOVERNOR).
//
// ---------------------------------------------------------------------------
// THE MESH_STREAM HEADER -- FROZEN HERE (R29), 64 BYTES AT PAGE OFFSET 0
// ---------------------------------------------------------------------------
// Same framing as the meshlet descriptor, deliberately: one aligned burst, a
// reserved-zero fence, and the frozen CRC step over bytes 0..59 at 60, so the
// RTL checks it with the SAME walker (`zhao_geom_desc_crc`) and no second CRC.
//
//   off size field           notes
//   0   1    format_id       descriptor format of every meshlet in the stream
//   1   1    flags           reserved, must be 0
//   2   2    meshlet_count   0 is legal: an empty stream draws nothing
//   4   2    generation      the stream's generation metadata
//   6   2    reserved        must be 0
//   8   4    desc_offset     PAGE-relative byte offset of descriptor 0;
//                            64-aligned and >= 64 (past this header)
//   12  48   reserved        must be 0
//   60  4    crc32c          over bytes 0..59, zhao_abi::zhao_crc32c
//
// REFUSED (the draw emits no job): unknown format, CRC mismatch, any nonzero
// reserved byte or flag, a misaligned or header-overlapping desc_offset, a
// page base that is not 64-aligned, or a descriptor table that ends past the
// page's published extent. A table inside its own page is what keeps a job
// from fetching a NEIGHBOUR'S bytes as descriptors -- the pool bound alone
// would admit them.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "zhao_abi.h"
#include "zref/zref_raster_state.hpp"

namespace zref {
namespace drawjob {

inline constexpr uint8_t kMeshStreamKind = 12;     // spec/cartridge.md 3, kind 12
inline constexpr uint8_t kSupportedFormat = 1;     // the descriptor format this reader speaks
inline constexpr uint32_t kHeaderBytes = 64;
inline constexpr uint32_t kDescBytes = 64;
inline constexpr int kHdrCrcCovered = 60;
inline constexpr int kHdrCrcOff = 60;
/** R28's material half for every v1 material (R48's named-constant seam). */
inline constexpr uint32_t kV1MaterialRaster = 0u;

/** Refusals, in the RTL's counter order AND its priority order. The first four
 *  are decided before any memory is read; the rest after the header lands. */
enum class Refusal : uint8_t {
  kNone = 0xFF,
  kCullReserved = 0,   // DrawForm.flags[3:2] == 3 (R28: refused, never aliased)
  kNotResident = 1,    // no MESH_STREAM directory row for the form's index
  kStale = 2,          // handle generation != directory generation [7:0]
  kXformMissing = 3,   // transform index past the palette, or row never written
  kFetchDenied = 4,    // the guard refused the header read
  kHeaderFormat = 5,   // format_id not kSupportedFormat
  kHeaderCrc = 6,      // crc32c mismatch
  kHeaderReserved = 7, // nonzero flags / reserved byte
  kHeaderLayout = 8,   // misaligned base or desc_offset, or table past extent
};
inline constexpr int kRefusalCount = 9;

struct DirRow {
  bool valid = false;
  uint32_t index = 0;       // handle index, 24 bits
  uint16_t generation = 0;  // MEM.UPLOAD's 16-bit residency generation
  uint32_t base = 0;        // VRAM byte address of the page
  uint32_t extent = 0;      // page length in bytes
};

struct Header {
  uint8_t format_id;
  uint8_t flags;
  uint16_t meshlet_count;
  uint16_t generation;
  uint16_t rsv6;
  uint32_t desc_offset;
  uint32_t crc32c;
};

inline uint16_t rd16(const uint8_t* b) { return uint16_t(b[0] | (b[1] << 8)); }
inline uint32_t rd32(const uint8_t* b) {
  return uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
}

inline Header decode_header(const uint8_t* b) {
  Header h{};
  h.format_id = b[0];
  h.flags = b[1];
  h.meshlet_count = rd16(b + 2);
  h.generation = rd16(b + 4);
  h.rsv6 = rd16(b + 6);
  h.desc_offset = rd32(b + 8);
  h.crc32c = rd32(b + kHdrCrcOff);
  return h;
}

/** Header validation, in the RTL's priority order. `base`/`extent` are the
 *  directory row's. */
inline Refusal validate_header(const uint8_t* b, uint32_t base, uint32_t extent) {
  const Header h = decode_header(b);
  if (h.format_id != kSupportedFormat) return Refusal::kHeaderFormat;
  if (zhao_abi::zhao_crc32c(0, b, kHdrCrcCovered) != h.crc32c) return Refusal::kHeaderCrc;
  bool rsv = (h.flags != 0) || (h.rsv6 != 0);
  for (int k = 12; k < kHdrCrcCovered; ++k) rsv = rsv || (b[k] != 0);
  if (rsv) return Refusal::kHeaderReserved;
  const uint64_t table_end = uint64_t(h.desc_offset) + uint64_t(kDescBytes) * h.meshlet_count;
  if ((base & 63u) != 0 || (h.desc_offset & 63u) != 0 || h.desc_offset < kHeaderBytes ||
      table_end > uint64_t(extent))
    return Refusal::kHeaderLayout;
  return Refusal::kNone;
}

/** The instance transform palette: GEOM.LOOM's emit stream writes it. */
struct XformPalette {
  std::vector<bool> valid;
  std::vector<std::array<int32_t, 12>> m;
  explicit XformPalette(uint32_t rows) : valid(rows, false), m(rows) {}
  void write(uint32_t node_index, const int32_t mat[12]) {
    if (node_index >= valid.size()) return;  // not draw-addressable; counted by the RTL
    valid[node_index] = true;
    for (int k = 0; k < 12; ++k) m[node_index][k] = mat[k];
  }
};

struct DrawForm {
  uint32_t form = 0, material_set = 0, transform = 0;
  uint8_t viewport_mask = 0, semantic_weight = 0;
  uint16_t flags = 0, src_id = 0;
};

struct Job {
  uint16_t instance_id;
  uint32_t desc_addr;   // 27-bit VRAM byte address
  uint8_t format;
  uint16_t generation;
  uint8_t active_mask;
  int32_t xform[12];
  uint32_t stream_base; // pool-relative
  uint32_t raster_state;
  uint32_t material_set;
  uint8_t semantic_weight;
};

struct Outcome {
  Refusal refusal = Refusal::kNone;
  bool masked = false;          // active mask 0: nothing fetched, nothing drawn
  std::vector<Job> jobs;
};

/** Stage 1 -- decided with no memory read. Returns the directory row used. */
inline Refusal pre_fetch(const DrawForm& d, const std::vector<DirRow>& dir, const XformPalette& pal,
                         DirRow* row_out) {
  bool ok = true;
  (void)raster_state::compose(d.flags, kV1MaterialRaster, &ok);
  if (!ok) return Refusal::kCullReserved;
  const uint32_t idx = d.form >> 8, gen8 = d.form & 0xFFu;
  const DirRow* hit = nullptr;
  for (const DirRow& r : dir)
    if (r.valid && r.index == idx) { hit = &r; break; }
  if (!hit) return Refusal::kNotResident;
  if ((hit->generation & 0xFFu) != gen8) return Refusal::kStale;
  const uint32_t xi = d.transform >> 8;
  if (xi >= pal.valid.size() || !pal.valid[xi]) return Refusal::kXformMissing;
  *row_out = *hit;
  return Refusal::kNone;
}

/** The whole law. `header` is the 64 bytes at row.base (nullptr = fetch denied). */
inline Outcome expand(const DrawForm& d, const std::vector<DirRow>& dir, const XformPalette& pal,
                      const uint8_t* header, uint32_t pool_base) {
  Outcome o;
  if ((d.viewport_mask & 0x3u) == 0) {
    // Refusals that need no memory still win over the mask: a reserved cull
    // mode is malformed whether or not anything would have been drawn.
    bool ok = true;
    (void)raster_state::compose(d.flags, kV1MaterialRaster, &ok);
    if (!ok) { o.refusal = Refusal::kCullReserved; return o; }
    o.masked = true;
    return o;
  }
  DirRow row;
  o.refusal = pre_fetch(d, dir, pal, &row);
  if (o.refusal != Refusal::kNone) return o;
  if (!header) { o.refusal = Refusal::kFetchDenied; return o; }
  o.refusal = validate_header(header, row.base, row.extent);
  if (o.refusal != Refusal::kNone) return o;
  const Header h = decode_header(header);
  bool ok = true;
  const uint32_t raster = raster_state::compose(d.flags, kV1MaterialRaster, &ok);
  const uint32_t xi = d.transform >> 8;
  for (uint32_t i = 0; i < h.meshlet_count; ++i) {
    Job j{};
    j.instance_id = d.src_id;
    j.desc_addr = (row.base + h.desc_offset + kDescBytes * i) & 0x07FFFFFFu;
    j.format = h.format_id;
    j.generation = h.generation;
    j.active_mask = d.viewport_mask & 0x3u;
    for (int k = 0; k < 12; ++k) j.xform[k] = pal.m[xi][k];
    j.stream_base = row.base - pool_base;
    j.raster_state = raster;
    j.material_set = d.material_set;
    j.semantic_weight = d.semantic_weight;
    o.jobs.push_back(j);
  }
  return o;
}

/** GEOM.MESHFETCH's rebase of a PAGE-relative descriptor offset. */
inline uint32_t rebase(uint32_t stream_base, uint32_t page_offset) {
  const uint64_t s = uint64_t(stream_base) + page_offset;
  return s > 0xFFFFFFFFull ? 0xFFFFFFFFu : uint32_t(s);
}

/** Build a valid header (test and fixture helper; writes the CRC). */
inline void make_header(uint8_t* b, uint8_t format, uint16_t count, uint16_t gen, uint32_t desc_offset) {
  std::memset(b, 0, kHeaderBytes);
  b[0] = format;
  b[2] = uint8_t(count);
  b[3] = uint8_t(count >> 8);
  b[4] = uint8_t(gen);
  b[5] = uint8_t(gen >> 8);
  for (int k = 0; k < 4; ++k) b[8 + k] = uint8_t(desc_offset >> (8 * k));
  const uint32_t c = zhao_abi::zhao_crc32c(0, b, kHdrCrcCovered);
  for (int k = 0; k < 4; ++k) b[kHdrCrcOff + k] = uint8_t(c >> (8 * k));
}

}  // namespace drawjob
}  // namespace zref
