// debug_trace_directed.cpp — DEBUG.TRACE's reference, pinned.
//
// There is no RTL for this block yet, so this file pins the REFERENCE. That is
// deliberate and it is what REFERENCE_COMPLETE means here: the three questions
// the specification leaves open have been answered in `zref_trace.hpp`, and an
// answer nobody tests is an answer that drifts. When the RTL lands, the same
// cases become the differential and nothing in this file needs rewriting.
//
// What is RATIFIED and merely checked for transcription: the 32-byte TRACE
// record of capture_format.md chunk 0x000A, and the `{kind:4, module:12,
// index:16}` source-id scheme of §5.
//
// What is CHOSEN and therefore genuinely pinned here: the stage numbering,
// arming as a mask, the zeroing of raster fields on a decoder event, and
// dropping rather than stalling when full.

#include "zhao_sim.hpp"
#include "zref/zref_trace.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using zhao::check;
namespace zt = zref::trace;

void test_source_id_scheme() {
  // §5: source_id = { kind:4, module:12, index:16 }. The spec's own worked
  // example is `kind=9, module=0x301, index=0x00AF` (capture_format §5), so it
  // is used verbatim rather than a number invented here.
  const uint32_t s = zt::pack_source_id(9, 0x301, 0x00AF);
  check(zt::source_kind(s) == 9, "source_id: kind round-trips", 9, zt::source_kind(s));
  check(zt::source_module(s) == 0x301, "source_id: module round-trips", 0x301,
        zt::source_module(s));
  check(zt::source_index(s) == 0x00AF, "source_id: index round-trips", 0x00AF, zt::source_index(s));
  // The packed word itself, so a change to the field ORDER fails here rather
  // than quietly reinterpreting every source id in every capture.
  check(s == 0x930100AFu, "source_id: packs to the spec's own worked example", 0x930100AFu, s);
}

void test_stage_numbering() {
  // The charter names seven sources and numbers none. The CHOSEN numbering is
  // its listed order; if anyone renumbers, this fails rather than silently
  // reinterpreting every capture ever taken.
  check(zt::kCommandDecoder == 0, "stage: command decoder is 0", 0, zt::kCommandDecoder);
  check(zt::kVertexOutput == 1, "stage: vertex output is 1", 1, zt::kVertexOutput);
  check(zt::kClippedTriangle == 2, "stage: clipped triangle is 2", 2, zt::kClippedTriangle);
  check(zt::kTileInsertion == 3, "stage: tile insertion is 3", 3, zt::kTileInsertion);
  check(zt::kTextureAddress == 4, "stage: texture address is 4", 4, zt::kTextureAddress);
  check(zt::kDepthTest == 5, "stage: depth test is 5", 5, zt::kDepthTest);
  check(zt::kFinalPixel == 6, "stage: final pixel is 6", 6, zt::kFinalPixel);
  check(zt::kStageCount == 7, "stage: exactly seven, per charter 20.6", 7, zt::kStageCount);
  check(zt::kEventBytes == 32, "event is 32 bytes (chunk 0x000A)", 32, zt::kEventBytes);
}

void test_arming_is_a_mask() {
  zt::Ring r(8);
  check(r.armed() == 0, "arming: nothing armed after construction", 0, r.armed());

  zt::DecodedRecord rec;
  rec.opcode = 0x0000;
  rec.source_id = zt::pack_source_id(5, 0x012, 0x0003);
  rec.index = 7;

  r.on_record(rec);
  check(r.events().empty(), "arming: an unarmed stage emits NOTHING", 0,
        static_cast<uint64_t>(r.events().size()));

  // A mask, not a selector: two stages armed at once is the whole reason the
  // mask was chosen over a three-bit index.
  r.arm((1u << zt::kCommandDecoder) | (1u << zt::kDepthTest));
  check(r.stage_armed(zt::kCommandDecoder), "arming: decoder armed", 1,
        r.stage_armed(zt::kCommandDecoder) ? 1 : 0);
  check(r.stage_armed(zt::kDepthTest), "arming: depth test armed at the same time", 1,
        r.stage_armed(zt::kDepthTest) ? 1 : 0);
  check(!r.stage_armed(zt::kFinalPixel), "arming: an unlisted stage stays off", 0,
        r.stage_armed(zt::kFinalPixel) ? 1 : 0);

  // Bits above 6 name no stage and must be discarded, not stored.
  r.arm(0xFF);
  check(r.armed() == 0x7F, "arming: bits above stage 6 are dropped", 0x7F, r.armed());
}

void test_decoder_event_fields() {
  zt::Ring r(8);
  r.arm(1u << zt::kCommandDecoder);

  zt::DecodedRecord rec;
  rec.opcode = 0x0002;
  rec.bytes = 32;
  rec.source_id = zt::pack_source_id(5, 0x012, 0x0003);
  rec.index = 41;
  r.on_record(rec);

  check(r.events().size() == 1, "decoder event: one record, one event", 1,
        static_cast<uint64_t>(r.events().size()));
  const zt::Event& e = r.events()[0];
  check(e.stage == zt::kCommandDecoder, "decoder event: stage", zt::kCommandDecoder, e.stage);
  check(e.source_id == rec.source_id, "decoder event: source_id propagates", rec.source_id,
        e.source_id);
  check(e.command_seq == 41, "decoder event: command_seq is the record index", 41, e.command_seq);

  // The CHOSEN zeroing. These fields describe a raster divergence; leaving them
  // undefined would make a byte comparison of two captures meaningless.
  check(e.tile == 0 && e.primitive == 0 && e.pixel == 0,
        "decoder event: raster fields are zero, not undefined", 0, e.tile | e.primitive | e.pixel);
  check(e.expected_fx == 0 && e.actual_fx == 0, "decoder event: fx fields are zero", 0,
        e.expected_fx | e.actual_fx);
  check(e.rsv[0] == 0 && e.rsv[1] == 0 && e.rsv[2] == 0, "decoder event: rsv bytes are zero", 0,
        e.rsv[0] | e.rsv[1] | e.rsv[2]);
}

void test_full_ring_drops() {
  // THE load-bearing choice: a full ring discards and counts, it does not
  // stall. Stalling would make tracing alter the timing being traced.
  const uint32_t kDepth = 4;
  zt::Ring r(kDepth);
  r.arm(1u << zt::kCommandDecoder);

  for (uint32_t i = 0; i < kDepth + 3; ++i) {
    zt::DecodedRecord rec;
    rec.source_id = zt::pack_source_id(5, 1, static_cast<uint16_t>(i));
    rec.index = i;
    r.on_record(rec);
  }

  check(r.events().size() == kDepth, "full ring: holds exactly its depth", kDepth,
        static_cast<uint64_t>(r.events().size()));
  check(r.dropped() == 3, "full ring: counts every discarded event", 3, r.dropped());
  // The kept events are the FIRST ones. A ring that dropped the oldest instead
  // would also be defensible, and this pins which was chosen.
  check(r.events().front().command_seq == 0, "full ring: keeps the earliest event", 0,
        r.events().front().command_seq);
  check(r.events().back().command_seq == kDepth - 1, "full ring: drops the latest", kDepth - 1,
        r.events().back().command_seq);
}

// PCG RXS-M-XS, the committed test PRNG shape (qformats.md §7.5).
struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t x = s;
    s = x * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((x >> 22) ^ x) >> 29);
    const uint32_t v = (static_cast<uint32_t>(x >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

/**
 * The random lane, PROPERTY-BASED rather than differential.
 *
 * There is one implementation today, so there is nothing to diff against. What
 * can still be checked on random input are the invariants the reference must
 * hold whatever it is fed, and those are exactly the CHOSEN laws: arming gates
 * emission completely, occupancy never exceeds depth, and every record offered
 * is either stored or counted as dropped — never silently lost.
 *
 * That last one is the property worth having. A ring that loses events without
 * counting them is indistinguishable from a working ring right up until an
 * investigation depends on it.
 */
void random_lane(uint32_t iters, uint64_t seed) {
  Prng rng(seed);
  for (uint32_t k = 0; k < iters && zhao::check_failures() == 0; ++k) {
    const uint32_t depth = 1 + rng.below(16);
    const uint8_t mask = static_cast<uint8_t>(rng.next() & 0xFF);
    zt::Ring r(depth);
    r.arm(mask);

    const uint32_t n = rng.below(40);
    for (uint32_t i = 0; i < n; ++i) {
      zt::DecodedRecord rec;
      rec.opcode = static_cast<uint16_t>(rng.next());
      rec.source_id = rng.next();
      rec.index = i;
      r.on_record(rec);
    }

    const bool armed = ((mask >> zt::kCommandDecoder) & 1u) != 0u;
    const uint64_t stored = r.events().size();

    check(stored <= depth, "random: occupancy never exceeds depth", depth, stored);
    if (!armed) {
      check(stored == 0 && r.dropped() == 0, "random: an unarmed stage neither stores nor drops", 0,
            stored + r.dropped());
    } else {
      check(stored + r.dropped() == n,
            "random: every offered record is stored OR counted as dropped", n,
            stored + r.dropped());
      check(stored == (n < depth ? n : depth), "random: stores min(offered, depth)",
            (n < depth ? n : depth), stored);
      for (uint64_t i = 0; i < stored; ++i) {
        check(r.events()[i].stage == zt::kCommandDecoder,
              "random: every stored event carries the decoder stage", zt::kCommandDecoder,
              r.events()[i].stage);
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_lane(static_cast<uint32_t>(std::atoi(argv[i + 1])), 0xDEBC0DEULL);
      return zhao::report_and_exit("debug_trace_random");
    }
  }

  test_stage_numbering();
  test_source_id_scheme();
  test_arming_is_a_mask();
  test_decoder_event_fields();
  test_full_ring_drops();
  return zhao::report_and_exit("debug_trace_directed");
}
