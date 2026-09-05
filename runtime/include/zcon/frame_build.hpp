// frame_build.hpp — command construction, the console runtime's own job.
// Authored 2026-09-05 (software lane).
//
// ---------------------------------------------------------------------------
// WHERE THIS SITS, AND WHY IT STOPS WHERE IT DOES
// ---------------------------------------------------------------------------
// The three boundaries put "resource handles, command construction, loading,
// streaming and lifecycle" in the console runtime. This is the command
// construction half: it turns a frame's presentation intent into a SEALED
// PACKET that the existing machinery already understands -- the same packet
// `zref::cmd::validate` checks and `zref::render::render_frame` consumes.
//
// It builds real records through `zhao_pack_*` and seals with
// `ZhaoFrameBuilder`. Nothing here invents a format.
//
// **It deliberately stops at DrawForm's handles.** `ZhCmdDrawForm` names a
// form, a material set and a transform BY HANDLE:
//
//     uint32_t form;          // handle32 {index:24, generation:8}
//     uint32_t material_set;
//     uint32_t transform;
//
// Publishing those is the general immutable-resource upload transaction, which
// is G2 and is not built. So a wizard cannot yet be drawn authentically, and
// this file does not pretend otherwise: it emits the frame scaffolding and the
// draw records, and the handles it puts in them are whatever the backend
// published. When G2 lands, the same call sites start naming real resources
// and nothing here changes.
//
// That is the roadmap's own dependency -- G3 needs G1 AND G2 -- arrived at from
// the code rather than assumed from the plan.

#ifndef ZCON_FRAME_BUILD_HPP
#define ZCON_FRAME_BUILD_HPP

#include <cstdint>
#include <vector>

#include "zcon/zcon.hpp"

#include "zref/zref_cmd.hpp"
#include "zref/zref_frame.hpp"

namespace zcon {

// One thing to draw this frame. Deliberately semantic: the game says WHAT, the
// runtime decides how it is encoded. If the game had to know the record layout,
// the boundary would be a comment rather than a boundary.
struct DrawItem {
  Handle form;
  Handle material_set;
  Handle transform;
  uint8_t viewport_mask = 0x3;  // Duo: both views by default
  uint8_t semantic_weight = 0;
  uint16_t flags = 0;
};

struct FramePlan {
  uint32_t frame_id = 0;
  uint32_t sequence = 0;
  uint32_t resource_epoch = 0;
  uint32_t deadline_cycles = 0;
  uint16_t flags = 0;
  std::vector<DrawItem> draws;
};

namespace detail {

// handle32 is {index:24, generation:8}. The runtime's Handle carries a 16-bit
// generation (D-3), so the low 8 bits go on the wire and the full value stays
// in the runtime's own table. Narrowing is stated rather than silent: a
// generation that wraps every 256 publishes is a real limit of the WIRE
// format, not of the residency law.
inline uint32_t handle32(const Handle& h) {
  return ((h.index & 0x00FFFFFFu) << 8) | (h.generation & 0xFFu);
}

}  // namespace detail

// Build and seal one frame. Returns the packet bytes.
//
// The record order is the one the executor expects: BEGIN_FRAME, the draws,
// END_FRAME. `ZhaoFrameBuilder::seal` computes both CRCs per
// capture_format.md §3 -- this file does not compute a checksum of its own,
// because a second checksum law is the same defect class as a second unit8
// multiply.
inline std::vector<uint8_t> build_frame(const FramePlan& plan) {
  ::zhao::ZhaoFrameBuilder b;
  b.begin_frame(plan.frame_id, plan.resource_epoch, plan.flags,
                plan.deadline_cycles);

  for (const DrawItem& d : plan.draws) {
    // Header fields follow zhao_abi::zhao_sample_draw_form exactly rather than
    // being guessed: record_bytes is 32 by the record's own static_assert, and
    // flags/reserved0 have no defined bits in v1 and MUST be zero.
    zhao_abi::ZhRecordDrawForm r{};
    r.hdr.opcode = zhao_abi::ZHAO_OP_DRAW_FORM;
    r.hdr.record_bytes = static_cast<uint16_t>(sizeof(zhao_abi::ZhRecordDrawForm));
    r.hdr.source_id = 0u;
    r.hdr.flags = 0u;
    r.hdr.reserved0 = 0u;
    r.payload.form = detail::handle32(d.form);
    r.payload.material_set = detail::handle32(d.material_set);
    r.payload.transform = detail::handle32(d.transform);
    r.payload.viewport_mask = d.viewport_mask;
    r.payload.semantic_weight = d.semantic_weight;
    r.payload.flags = d.flags;

    std::vector<uint8_t> bytes;
    zhao_abi::zhao_pack_draw_form(r, bytes);
    b.append_record(bytes);
  }

  b.end_frame(0);
  return b.seal(plan.frame_id, plan.sequence, plan.resource_epoch,
                plan.deadline_cycles, plan.flags);
}

// Build and immediately validate. A packet the console's own validator refuses
// is a bug in construction, and finding that here costs nothing -- finding it
// after it has been submitted, transported and half-executed costs a session.
struct BuiltFrame {
  std::vector<uint8_t> bytes;
  ::zref::cmd::Result verdict;
  bool ok = false;
};

inline BuiltFrame build_and_validate(const FramePlan& plan) {
  BuiltFrame f;
  f.bytes = build_frame(plan);
  f.verdict = ::zref::cmd::validate(f.bytes);
  f.ok = (f.verdict.error == 0);
  return f;
}

}  // namespace zcon

#endif  // ZCON_FRAME_BUILD_HPP
