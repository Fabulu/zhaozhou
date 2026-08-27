// zfield_plan.hpp — the exact software FPLAN planner (Field v3 Phase 2,
// reports/Fieldv3.md). Runs on the ARM in production; runs everywhere in
// test.
//
// An FPLAN is a DERIVED artifact — a CPU-style decoded-uop cache entry, not a
// second ISA. It is keyed by {canonical_program_hash, FPLAN_ABI_VERSION,
// FPLAN_FABRIC_VERSION} (design/contracts/FIELD.PROGCACHE.md amendment). The
// canonical .zprog, its validator, its hash and zfield::interpret remain the
// single semantic law.
//
// THE REQUIRED DIFFERENTIAL (the Phase 2 gate; no v3 RTL until it is green):
//
//     full canonical zfield::interpret
//       ==  prepare() once + execute_point() per point
//
// on every output, every saturation lane, rcp0, boundary inputs, random legal
// programs and the three committed Earth programs.
//
// Uniform/varying split: forward taint from the profile's varying input
// lanes, kill on overwrite. Uniform instructions execute ONCE per field
// instance (prepare) on a scalar bank; their SatLedger events form the
// uniform Status, ORed with the varying result exactly as if every point had
// executed them — sound because uniform instructions compute identical
// values and identical sticky events at every point.
//
// The canonical->uop translation is driven ONLY by the generated operation
// table (zfield/generated/zfield_optable.hpp) and canonical opcodes pass
// through unchanged into the uops. There is no second opcode numbering — the
// failure class of the v2 private encoding is structurally excluded.

#pragma once

#include <cstdint>
#include <vector>

#include "zfield/zfield.hpp"

namespace zfield {

constexpr uint32_t FPLAN_ABI_VERSION = 1;
constexpr uint32_t FPLAN_FABRIC_VERSION = 1;

// Synthetic operation codes — deliberately OUTSIDE the canonical opcode space
// (>= 0xF0; canonical v1 tops out at 0x29 and reserves the rest, field-ir.md
// §2). They are plan-internal and never appear in a .zprog.
constexpr uint8_t UOP_RING_PREP = 0xF1;  // ring with prepared m/rA/rB (9 vmul slots)
constexpr uint8_t PREP_RING_MID = 0xF0;  // rescale_s32((s64)r0 + r1, 1)

enum class PlanClass : uint8_t { kHot = 0, kCold = 1, kSoftware = 2 };

enum class SrcKind : uint8_t { kVec = 0, kSca = 1 };

struct UopSrc {
  SrcKind kind;
  uint16_t idx;  // vector register or scalar-bank slot
};

/** One vector uop: a canonical operation (or UOP_RING_PREP) over tagged
 *  operands. dst is always a vector register group. */
struct VecUop {
  uint8_t op;
  uint8_t dst;    // vector register (group start)
  uint8_t n_src;  // flattened member count
  UopSrc src[9];
  uint32_t imm;     // canonical imm (cmp mode / table id / seed / axis)
  uint16_t src_pc;  // originating canonical pc (source mapping)
};

/** One uniform-block op, executed once per field instance on the scalar
 *  bank. op is canonical, or PREP_RING_MID. dst slots are consecutive. */
struct PrepUop {
  uint8_t op;
  uint16_t dst;
  uint8_t n_src;
  uint16_t src[9];  // scalar-bank slots
  uint32_t imm;
  uint16_t src_pc;
};

struct OutTag {
  SrcKind kind;
  uint16_t idx;
};

/** Per-resource demand per full 1,089-vertex association
 *  (spec/form/cost-model.md §5). */
struct Demand {
  uint32_t vec_groups = 0;
  uint32_t vec_issue = 0;    // uops issued per point-group
  uint32_t vmul_slots = 0;   // vector multiply bank slots per group
  uint32_t curve_req = 0;    // curve service requests per group
  uint32_t dist_req = 0;     // distance service requests per group
  uint32_t cold_ops = 0;     // cold-lane scalar ops per POINT
  uint32_t uniform_ops = 0;  // prep ops per field instance
  uint32_t table_bytes = 0;
  uint32_t vreg_hwm = 0;
  uint32_t sreg_hwm = 0;
};

struct Fplan {
  uint32_t canonical_hash = 0;
  uint32_t plan_abi_version = FPLAN_ABI_VERSION;
  uint32_t fabric_version = FPLAN_FABRIC_VERSION;
  uint8_t profile = 0;
  uint32_t varying_mask = 0;  // bit i: in_lane i varies per point
  PlanClass perf_class = PlanClass::kCold;

  std::vector<PrepUop> prep;      // the uniform instruction block
  std::vector<uint16_t> in_slot;  // per in_lane: scalar slot, or 0xFFFF if varying/unused
  std::vector<uint8_t> in_vreg;   // per in_lane: vector reg, or 0xFF if uniform/unused
  uint16_t n_scalar = 0;
  uint8_t n_vreg = 0;

  std::vector<VecUop> uops;
  std::vector<OutTag> out_map;  // per out_lane
  Demand demand;
};

struct Prepared {
  std::vector<int32_t> scalar;
  Status uniform_status;
  zref::SatLedger uniform_ledger;  // full per-lane ledger of the uniform block
};

/** Lower a decoded (validated) program. varying_mask bit i marks in-lane i
 *  as per-point varying (Earth profile: lanes 0 and 1 — x and z). */
Fplan plan(const Decoded& prog, uint32_t varying_mask);

/** Run the uniform block once per field instance. `in` is the full input
 *  record in canonical lane order; varying lanes are ignored here. */
Prepared prepare(const Fplan& fp, const Decoded& prog, const int32_t* in, size_t n_in);

/** Reference-execute the vector uops for ONE point. `in` supplies this
 *  point's varying lanes (uniform lanes ignored — they live in `prep`).
 *  Returns uniform-OR-varying Status, as if the point had run the whole
 *  canonical program. */
Status execute_point(const Fplan& fp, const Decoded& prog, const Prepared& prep, const int32_t* in,
                     size_t n_in, int32_t* out, size_t n_out);

/** As above, additionally exposing the VARYING half's SatLedger (the uniform
 *  half's is in Prepared::uniform_ledger). `ledger_out` may be nullptr. */
Status execute_point(const Fplan& fp, const Decoded& prog, const Prepared& prep, const int32_t* in,
                     size_t n_in, int32_t* out, size_t n_out, zref::SatLedger* ledger_out);

}  // namespace zfield
