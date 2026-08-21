// debug_trace_rtl_directed.cpp — DEBUG.TRACE's RTL against `zref::trace::Ring`.
//
// The reference-only laws (stage numbering, mask arming, drop-not-stall as a
// model property) are pinned by debug_trace_directed.cpp. This file is the
// hardware differential: the Verilated `zhao_debug_trace` and the reference ring
// see the same offered events, and afterwards the stored records must match
// WORD FOR WORD in capture order, along with the event count and the drop count.
//
// One modelling note, because it is a real difference and not a convenience.
// `Ring::push` does not itself check arming — `Ring::on_record` checks, then
// pushes. The RTL gates inside the block. So the mirror here pushes to the
// reference only when the stage is armed and legal, which is exactly what
// `on_record` does for its own stage, generalised to all seven. Getting that
// wrong in the other direction (pushing everything to the reference) would make
// the reference count drops the hardware correctly never had.
//
// THE LAW THIS FILE EXISTS FOR: the ring drops and never stalls. There is no
// ready signal to test, so the property is tested the only way it can be —
// section 5 offers events every single cycle, far past the ring's depth, and
// requires that the first DEPTH are stored perfectly, that every later one is
// counted, and that nothing about the block's behaviour depends on how fast
// they arrive.

#include "Vzhao_debug_trace.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_trace.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using zhao::check;
namespace zt = zref::trace;

constexpr int kDepth = 64;  // must match the RTL default

struct Offer {
  uint8_t stage;
  uint32_t tile, primitive, pixel, expected_fx, actual_fx, source_id, command_seq;
};

void arm(Vzhao_debug_trace& dut, zt::Ring& ring, uint8_t mask) {
  dut.arm_we_i = 1;
  dut.arm_mask_i = mask & 0x7F;
  dut.eval();
  zhao::tick(dut);
  dut.arm_we_i = 0;
  dut.eval();
  ring.arm(mask);
}

void clear(Vzhao_debug_trace& dut, zt::Ring& ring) {
  dut.clear_i = 1;
  dut.eval();
  zhao::tick(dut);
  dut.clear_i = 0;
  dut.eval();
  ring.clear();
}

/** Offer one event to both. `gap` idle cycles follow, to vary the arrival rate. */
void offer(Vzhao_debug_trace& dut, zt::Ring& ring, const Offer& o, int gap = 0) {
  dut.ev_valid_i = 1;
  dut.ev_stage_i = o.stage;
  dut.ev_tile_i = o.tile;
  dut.ev_primitive_i = o.primitive;
  dut.ev_pixel_i = o.pixel;
  dut.ev_expected_fx_i = o.expected_fx;
  dut.ev_actual_fx_i = o.actual_fx;
  dut.ev_source_id_i = o.source_id;
  dut.ev_command_seq_i = o.command_seq;
  dut.eval();
  zhao::tick(dut);
  dut.ev_valid_i = 0;
  dut.eval();

  // The reference has no notion of an unarmed or illegal stage arriving: its
  // push() stores whatever it is given. So the gate lives here, mirroring what
  // on_record does for the decoder stage.
  if (o.stage < zt::kStageCount && ring.stage_armed(static_cast<zt::Stage>(o.stage))) {
    zt::Event e;
    e.tile = o.tile;
    e.primitive = o.primitive;
    e.pixel = o.pixel;
    e.stage = o.stage;
    e.expected_fx = o.expected_fx;
    e.actual_fx = o.actual_fx;
    e.source_id = o.source_id;
    e.command_seq = o.command_seq;
    ring.push(e);
  }

  for (int g = 0; g < gap; ++g) {
    zhao::tick(dut);
    dut.eval();
  }
}

/** Read one 32-bit word of the ring. The read is registered: one cycle. */
uint32_t read_word(Vzhao_debug_trace& dut, int event_idx, int word) {
  dut.rd_addr_i = static_cast<uint32_t>(event_idx * 8 + word);
  dut.eval();
  zhao::tick(dut);
  dut.eval();
  return dut.rd_data_o;
}

/** The eight words the capture layout says an Event serialises to. */
void expected_words(const zt::Event& e, uint32_t out[8]) {
  out[0] = e.tile;
  out[1] = e.primitive;
  out[2] = e.pixel;
  out[3] = static_cast<uint32_t>(e.stage) | (static_cast<uint32_t>(e.rsv[0]) << 8) |
           (static_cast<uint32_t>(e.rsv[1]) << 16) | (static_cast<uint32_t>(e.rsv[2]) << 24);
  out[4] = e.expected_fx;
  out[5] = e.actual_fx;
  out[6] = e.source_id;
  out[7] = e.command_seq;
}

void compare(Vzhao_debug_trace& dut, const zt::Ring& ring, const char* what) {
  const std::string t(what);
  check(dut.count_o == ring.events().size(), (t + ": event count").c_str(),
        static_cast<uint64_t>(ring.events().size()), dut.count_o);
  check(dut.dropped_o == ring.dropped(), (t + ": dropped").c_str(), ring.dropped(), dut.dropped_o);

  const size_t n = ring.events().size();
  for (size_t ei = 0; ei < n; ++ei) {
    uint32_t want[8];
    expected_words(ring.events()[ei], want);
    for (int wi = 0; wi < 8; ++wi) {
      const uint32_t got = read_word(dut, static_cast<int>(ei), wi);
      char lbl[176];
      std::snprintf(lbl, sizeof lbl, "%s: event %zu word %d", t.c_str(), ei, wi);
      check(got == want[wi], lbl, want[wi], got);
    }
  }
}

Offer make(uint8_t stage, uint32_t seed) {
  Offer o;
  o.stage = stage;
  o.tile = 0x1000'0000u + seed;
  o.primitive = 0x2000'0000u + seed * 3u;
  o.pixel = 0x3000'0000u + seed * 5u;
  o.expected_fx = 0x4000'0000u + seed * 7u;
  o.actual_fx = 0x5000'0000u + seed * 11u;
  o.source_id =
      zt::pack_source_id(static_cast<uint8_t>(seed & 0xF), static_cast<uint16_t>(seed & 0x0FFF),
                         static_cast<uint16_t>(seed));
  o.command_seq = seed;
  return o;
}

// PCG RXS-M-XS, the committed test PRNG shape (qformats §7.5).
struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

void reset(Vzhao_debug_trace& dut) {
  dut.rst_n = 0;
  dut.ev_valid_i = 0;
  dut.arm_we_i = 0;
  dut.clear_i = 0;
  dut.rd_addr_i = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Vzhao_debug_trace dut;
  reset(dut);

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0x7BA5u);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      zt::Ring ring(kDepth);
      reset(dut);
      arm(dut, ring, static_cast<uint8_t>(rng.next() & 0x7F));
      const uint32_t n = 1 + rng.below(120);
      for (uint32_t k = 0; k < n; ++k) {
        // Stages 0..8 so illegal ones (7, 8) arrive mixed in with legal ones,
        // and the arrival rate varies from back-to-back to sparse.
        offer(dut, ring, make(static_cast<uint8_t>(rng.below(9)), rng.next()),
              static_cast<int>(rng.below(3)));
      }
      char tag[64];
      std::snprintf(tag, sizeof tag, "random[%u] n=%u", it, n);
      compare(dut, ring, tag);
    }
    dut.final();
    return zhao::report_and_exit("debug_trace_rtl_random");
  }

  // ---- 1. nothing armed: nothing is an event ------------------------------
  // Not stored, and NOT counted as dropped. `dropped` has to mean "a real event
  // was lost", or a capture's drop count tells you nothing.
  {
    zt::Ring ring(kDepth);
    reset(dut);
    arm(dut, ring, 0x00);
    for (uint8_t s = 0; s < 7; ++s) offer(dut, ring, make(s, 100 + s));
    compare(dut, ring, "nothing armed");
    check(dut.count_o == 0, "nothing armed: nothing stored", 0, dut.count_o);
    check(dut.dropped_o == 0, "nothing armed: and nothing counted as dropped", 0, dut.dropped_o);
  }

  // ---- 2. one stage armed, the other six ignored --------------------------
  {
    zt::Ring ring(kDepth);
    reset(dut);
    arm(dut, ring, 1u << zt::kDepthTest);
    for (uint8_t s = 0; s < 7; ++s) offer(dut, ring, make(s, 200 + s));
    compare(dut, ring, "only the depth-test stage armed");
    check(dut.count_o == 1, "exactly the armed stage was stored", 1, dut.count_o);
  }

  // ---- 3. THE MASK IS A MASK ----------------------------------------------
  // Several stages at once. A single-selector implementation passes section 2
  // and fails here, and correlating two stages in one run is the reason the
  // mask was chosen over a selector in the first place.
  {
    zt::Ring ring(kDepth);
    reset(dut);
    const uint8_t mask =
        (1u << zt::kCommandDecoder) | (1u << zt::kTileInsertion) | (1u << zt::kFinalPixel);
    arm(dut, ring, mask);
    check(dut.armed_o == mask, "the armed mask reads back", mask, dut.armed_o);
    for (uint8_t s = 0; s < 7; ++s) offer(dut, ring, make(s, 300 + s));
    compare(dut, ring, "three stages armed at once");
    check(dut.count_o == 3, "all three armed stages stored, in offer order", 3, dut.count_o);
  }

  // ---- 4. an illegal stage is not an event --------------------------------
  // The charter names seven sources. 7 and above are outside the enum, so they
  // are neither stored nor counted -- nothing this block understood was offered.
  {
    zt::Ring ring(kDepth);
    reset(dut);
    arm(dut, ring, 0x7F);
    offer(dut, ring, make(7, 400));
    offer(dut, ring, make(8, 401));
    offer(dut, ring, make(255, 402));
    check(dut.count_o == 0, "stages outside 0..6 are not stored", 0, dut.count_o);
    check(dut.dropped_o == 0, "and are not counted as drops either", 0, dut.dropped_o);
    offer(dut, ring, make(6, 403));
    compare(dut, ring, "illegal stages ignored, a legal one after them still lands");
  }

  // ---- 5. DROP, NEVER STALL -----------------------------------------------
  // Events offered on EVERY cycle, well past the ring's depth. The first DEPTH
  // must be stored perfectly; every one after must be counted. There is no
  // ready signal to check, so this is the only way the property is observable:
  // the block cannot have slowed anyone down, because there was never anything
  // for a producer to wait on.
  {
    zt::Ring ring(kDepth);
    reset(dut);
    arm(dut, ring, 0x7F);
    const int kOver = 37;
    for (int k = 0; k < kDepth + kOver; ++k) {
      offer(dut, ring, make(static_cast<uint8_t>(k % 7), static_cast<uint32_t>(1000 + k)), 0);
    }
    compare(dut, ring, "back-to-back overflow");
    check(dut.count_o == kDepth, "the ring holds exactly its depth", kDepth, dut.count_o);
    check(dut.dropped_o == kOver, "and counted every event past it", kOver, dut.dropped_o);
  }

  // ---- 5b. A FULL RING STILL IGNORES UNARMED EVENTS -----------------------
  // Section 1 shows an unarmed event is not counted as a drop -- but only while
  // the ring has room, where nothing would be dropped anyway. A mutation that
  // counted every offered event as a drop once full passed section 1 cleanly
  // and was caught only by the random lane.
  //
  // "Dropped" has to mean "a real event was lost". An unarmed stage was never
  // an event, full ring or not, and a drop count inflated by traffic nobody
  // asked to see would make a capture's loss figure meaningless.
  {
    zt::Ring ring(kDepth);
    reset(dut);
    arm(dut, ring, 1u << zt::kFinalPixel);
    for (int k = 0; k < kDepth; ++k) {
      offer(dut, ring, make(zt::kFinalPixel, static_cast<uint32_t>(6000 + k)));
    }
    check(dut.count_o == kDepth, "the ring is full", kDepth, dut.count_o);
    check(dut.dropped_o == 0, "and nothing has been dropped yet", 0, dut.dropped_o);

    // Now offer the six UNARMED stages, and an illegal one, into a full ring.
    for (uint8_t st = 0; st < 7; ++st) {
      if (st == zt::kFinalPixel) continue;
      offer(dut, ring, make(st, 7000 + st));
    }
    offer(dut, ring, make(9, 7100));
    check(dut.dropped_o == 0, "unarmed and illegal stages are not drops, even against a full ring",
          0, dut.dropped_o);

    // One ARMED event against the full ring is a drop, and exactly one.
    offer(dut, ring, make(zt::kFinalPixel, 7200));
    check(dut.dropped_o == 1, "but one armed event against a full ring is exactly one drop", 1,
          dut.dropped_o);
    compare(dut, ring, "full ring, mixed armed and unarmed traffic");
  }

  // ---- 6. the same events, arriving slowly, give the same ring ------------
  // The ring's contents must be a function of the events, not of their timing.
  // If they differed, a capture would not be replayable.
  {
    zt::Ring ring(kDepth);
    reset(dut);
    arm(dut, ring, 0x7F);
    for (int k = 0; k < kDepth + 5; ++k) {
      offer(dut, ring, make(static_cast<uint8_t>(k % 7), static_cast<uint32_t>(1000 + k)),
            3 + (k % 4));
    }
    compare(dut, ring, "same events, sparse arrival");
    check(dut.count_o == kDepth, "still exactly the depth", kDepth, dut.count_o);
    check(dut.dropped_o == 5, "still exactly the overflow", 5, dut.dropped_o);
  }

  // ---- 7. clear empties the ring and the drop count -----------------------
  {
    zt::Ring ring(kDepth);
    reset(dut);
    arm(dut, ring, 0x7F);
    for (int k = 0; k < kDepth + 3; ++k) {
      offer(dut, ring, make(static_cast<uint8_t>(k % 7), static_cast<uint32_t>(k)));
    }
    check(dut.dropped_o == 3, "drops accumulated before the clear", 3, dut.dropped_o);
    clear(dut, ring);
    check(dut.count_o == 0, "clear empties the ring", 0, dut.count_o);
    check(dut.dropped_o == 0, "and zeroes the drop count", 0, dut.dropped_o);
    check(dut.armed_o == 0x7F, "but does NOT disarm: clearing is not reconfiguring", 0x7F,
          dut.armed_o);
    for (int k = 0; k < 5; ++k) {
      offer(dut, ring, make(static_cast<uint8_t>(k % 7), static_cast<uint32_t>(5000 + k)));
    }
    compare(dut, ring, "the ring refills after a clear");
  }

  // ---- 8. the record layout, field by field -------------------------------
  // The differential above would pass if BOTH sides serialised in the same
  // wrong order. capture_format.md chunk 0x000A is the authority, so the word
  // positions are asserted directly against distinguishable values.
  {
    zt::Ring ring(kDepth);
    reset(dut);
    arm(dut, ring, 1u << zt::kTextureAddress);
    Offer o;
    o.stage = zt::kTextureAddress;
    o.tile = 0xAAAA'0001u;
    o.primitive = 0xBBBB'0002u;
    o.pixel = 0xCCCC'0003u;
    o.expected_fx = 0xDDDD'0004u;
    o.actual_fx = 0xEEEE'0005u;
    o.source_id = zt::pack_source_id(0x9, 0x123, 0x4567);
    o.command_seq = 0xFFFF'0006u;
    offer(dut, ring, o);

    check(read_word(dut, 0, 0) == 0xAAAA0001u, "word 0 is tile", 0xAAAA0001u, read_word(dut, 0, 0));
    check(read_word(dut, 0, 1) == 0xBBBB0002u, "word 1 is primitive", 0xBBBB0002u,
          read_word(dut, 0, 1));
    check(read_word(dut, 0, 2) == 0xCCCC0003u, "word 2 is pixel", 0xCCCC0003u,
          read_word(dut, 0, 2));
    check(read_word(dut, 0, 3) == zt::kTextureAddress,
          "word 3 is the stage byte with the three reserved bytes ZERO", zt::kTextureAddress,
          read_word(dut, 0, 3));
    check(read_word(dut, 0, 4) == 0xDDDD0004u, "word 4 is expected_fx", 0xDDDD0004u,
          read_word(dut, 0, 4));
    check(read_word(dut, 0, 5) == 0xEEEE0005u, "word 5 is actual_fx", 0xEEEE0005u,
          read_word(dut, 0, 5));
    check(read_word(dut, 0, 6) == zt::pack_source_id(0x9, 0x123, 0x4567),
          "word 6 is source_id, packed { kind:4, module:12, index:16 }",
          zt::pack_source_id(0x9, 0x123, 0x4567), read_word(dut, 0, 6));
    check(read_word(dut, 0, 7) == 0xFFFF0006u, "word 7 is command_seq", 0xFFFF0006u,
          read_word(dut, 0, 7));
    compare(dut, ring, "record layout");
  }

  dut.final();
  return zhao::report_and_exit("debug_trace_rtl_directed");
}
