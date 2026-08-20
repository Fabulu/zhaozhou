// zref_trace.hpp — DEBUG.TRACE's reference model.
//
// The ledger declared `zref::DebugTrace` and that symbol never existed: the
// NINTH phantom reference_model in this tree. Unlike CMD.DECODER, whose law was
// already shipped under another name, this one genuinely had no implementation
// anywhere — so this file is a real reference, not a view.
//
// WHAT IS RATIFIED, and it is more than it first appears:
//   - the TRACE record layout, capture_format.md chunk 0x000A:
//       { u32 tile; u32 primitive; u32 pixel; u8 stage; u8 rsv[3];
//         u32 expected_fx; u32 actual_fx; u32 source_id; u32 command_seq }
//     32 bytes, and this block does not get to invent a different one.
//   - the source-id scheme, capture_format.md §5:
//       source_id: u32 = { kind: 4 bits, module: 12 bits, index: 16 bits }
//   - the seven selectable trace sources, charter §20.6.
//
// WHAT IS NOT, and is therefore CHOSEN here with the alternative recorded:
//
//   1. THE `stage` BYTE VALUES. The charter NAMES seven sources in a list and
//      numbers none of them. Chosen: their listed order, 0..6, because a list
//      in a normative document is the only ordering anyone can rediscover.
//      Rejected: grouping by subsystem, which reads better and is unguessable.
//      A stage byte outside 0..6 is not a legal event.
//
//   2. ARMING IS A MASK, NOT A SELECTOR. The charter says "selectable" without
//      saying whether one source or several may be armed. Chosen: a seven-bit
//      mask, so several stages can be traced in one run. Rejected: a single
//      three-bit index, which is smaller but makes correlating two stages
//      impossible in one capture, and correlating stages is the entire reason
//      a trace ring exists.
//
//   3. THE UNUSED FIELDS OF A DECODER-STAGE EVENT ARE ZERO. `tile`,
//      `primitive`, `pixel`, `expected_fx` and `actual_fx` describe a raster
//      divergence and mean nothing for a decoded command record. Chosen: zero,
//      because zero is checkable. Rejected: leaving them undefined, which makes
//      a byte-comparison of captures impossible and is how a trace format rots.
//
// None of the three is inventing a WIRE layout — that came from the spec. They
// are the three questions the spec leaves open, answered in one place.
#pragma once

#include <cstdint>
#include <vector>

namespace zref {
namespace trace {

/** charter §20.6's seven sources, in the order the charter lists them. */
enum Stage : uint8_t {
  kCommandDecoder = 0,
  kVertexOutput   = 1,
  kClippedTriangle= 2,
  kTileInsertion  = 3,
  kTextureAddress = 4,
  kDepthTest      = 5,
  kFinalPixel     = 6,
  kStageCount     = 7
};

/** capture_format.md chunk 0x000A, exactly. 32 bytes on the wire. */
struct Event {
  uint32_t tile = 0;
  uint32_t primitive = 0;
  uint32_t pixel = 0;
  uint8_t  stage = 0;
  uint8_t  rsv[3] = {0, 0, 0};
  uint32_t expected_fx = 0;
  uint32_t actual_fx = 0;
  uint32_t source_id = 0;
  uint32_t command_seq = 0;

  bool operator==(const Event& o) const {
    return tile == o.tile && primitive == o.primitive && pixel == o.pixel &&
           stage == o.stage && rsv[0] == o.rsv[0] && rsv[1] == o.rsv[1] &&
           rsv[2] == o.rsv[2] && expected_fx == o.expected_fx &&
           actual_fx == o.actual_fx && source_id == o.source_id &&
           command_seq == o.command_seq;
  }
};

inline constexpr uint32_t kEventBytes = 32;

/** §5: source_id = { kind:4, module:12, index:16 }. Provided so nothing
 *  downstream re-derives the packing by hand. */
inline constexpr uint32_t pack_source_id(uint8_t kind, uint16_t module_, uint16_t index) {
  return (static_cast<uint32_t>(kind & 0xF) << 28) |
         (static_cast<uint32_t>(module_ & 0x0FFF) << 16) | static_cast<uint32_t>(index);
}
inline constexpr uint8_t  source_kind(uint32_t s)   { return static_cast<uint8_t>(s >> 28); }
inline constexpr uint16_t source_module(uint32_t s) { return static_cast<uint16_t>((s >> 16) & 0x0FFF); }
inline constexpr uint16_t source_index(uint32_t s)  { return static_cast<uint16_t>(s & 0xFFFF); }

/** One decoded record as CMD.DECODER presents it. */
struct DecodedRecord {
  uint16_t opcode = 0;
  uint16_t bytes = 0;
  uint32_t source_id = 0;
  uint32_t index = 0;
};

/**
 * The ring. Bounded, and it DROPS rather than stalling when full.
 *
 * That is chosen and it is the important one. A trace ring exists to observe a
 * machine without changing it; a ring that stalled its producer would make the
 * act of tracing alter the timing being traced, which is the one thing a
 * debugging aid must never do. So a full ring counts what it lost and the
 * pipeline runs on unaffected.
 *
 * Rejected: back-pressuring CMD.DECODER. It would preserve every event and
 * silently destroy the determinism the whole capture system rests on.
 */
class Ring {
 public:
  explicit Ring(uint32_t depth = 64) : depth_(depth ? depth : 1) {}

  /** Which stages are armed; bit n arms Stage n. */
  void arm(uint8_t mask) { mask_ = static_cast<uint8_t>(mask & 0x7F); }
  uint8_t armed() const { return mask_; }

  bool stage_armed(Stage s) const {
    return s < kStageCount && ((mask_ >> static_cast<uint8_t>(s)) & 1u) != 0u;
  }

  /** Offer one decoded record to the command-decoder stage. */
  void on_record(const DecodedRecord& r) {
    if (!stage_armed(kCommandDecoder)) return;
    Event e;
    e.stage = static_cast<uint8_t>(kCommandDecoder);
    e.source_id = r.source_id;
    e.command_seq = r.index;
    // tile/primitive/pixel/expected/actual stay zero: see choice 3 in the
    // header. They describe a raster divergence and this is not one.
    push(e);
  }

  void push(const Event& e) {
    if (events_.size() >= depth_) {
      ++dropped_;
      return;
    }
    events_.push_back(e);
  }

  const std::vector<Event>& events() const { return events_; }
  uint32_t dropped() const { return dropped_; }
  uint32_t depth() const { return depth_; }
  void clear() { events_.clear(); dropped_ = 0; }

 private:
  uint32_t depth_;
  uint8_t  mask_ = 0;
  uint32_t dropped_ = 0;
  std::vector<Event> events_;
};

}  // namespace trace
}  // namespace zref
