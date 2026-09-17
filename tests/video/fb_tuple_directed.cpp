// fb_tuple_directed.cpp -- the 84-bit READY/swap tuple layout, checked against
// bit positions written here rather than read from the package.
//
// The point of this file is the one thing a round trip cannot do. `pack` and
// the six accessors share the package's constants, so they agree with each
// other whatever those constants say; a pack-then-unpack test passes on a
// layout with every field in the wrong place. So the expected tuple below is
// assembled from shifts written out longhand, and the field origins are
// asserted as numbers.
//
// The mutant swaps the writer and slot bits. It tiles the 84 bits perfectly, so
// the package's elaboration coverage check passes, and it round-trips
// perfectly. Only the literal sees it -- and in the shell it would produce a
// swap echo whose writer and slot are each other's, which the manager refuses
// as stale while every lifecycle counter balances.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vtb_fb_tuple.h"

#include "zhao_sim.hpp"

namespace {

using Dut = Vtb_fb_tuple;

// The sample record, and the expected 84 bits assembled independently.
//
// WRITER AND SLOT MUST DIFFER, and the first version of this file had both set
// to 1. Swapping two identical bits produces an identical tuple, so the mutant
// reported MISSED -- not because the layout check is weak but because the
// stimulus could not reach the fault. A gate that cannot reach the state is not
// evidence about the state, and a one-bit field is the easiest place in the
// world to write that mistake.
constexpr bool kWriter = true;
constexpr bool kSlot = false;
constexpr uint16_t kGeneration = 0xBEEFu;
constexpr uint8_t kMode = 2;
constexpr uint32_t kBase = 0xDEADBEEFu;
constexpr uint32_t kSpan = 0x12345678u;

// 84 bits, MSB-first: span at 0, base at 32, mode at 64, generation at 66,
// slot at 82, writer at 83. Written as shifts, not as a call into the thing
// under test.
//
// THESE LITERALS WERE WRONG ONCE, in a way this file could not catch. The
// first version had the layout reversed -- writer at bit 0 -- and so did the
// package, so every check here passed. Literals written from the same wrong
// premise as the code are not independent of it. What settled it was
// `zhao_video_ready_bridge_v2.sv`, which documents the order in its header
// AND reads the slot as `pending_tuple_q[82]`.
__uint128_t expected_tuple() {
  __uint128_t t = 0;
  t |= static_cast<__uint128_t>(kSpan) << 0;
  t |= static_cast<__uint128_t>(kBase) << 32;
  t |= static_cast<__uint128_t>(kMode) << 64;
  t |= static_cast<__uint128_t>(kGeneration) << 66;
  t |= static_cast<__uint128_t>(kSlot ? 1u : 0u) << 82;
  t |= static_cast<__uint128_t>(kWriter ? 1u : 0u) << 83;
  return t;
}

__uint128_t read_packed(const Dut& d) {
  // Verilator widens an 84-bit output into 32-bit words.
  __uint128_t t = 0;
  for (int w = 2; w >= 0; --w) {
    t <<= 32;
    t |= static_cast<__uint128_t>(d.packed_o[w]);
  }
  return t;
}

void write_tuple(Dut& d, __uint128_t t) {
  for (int w = 0; w < 3; ++w) {
    d.tuple_i[w] = static_cast<uint32_t>(t & 0xFFFFFFFFu);
    t >>= 32;
  }
}

void print_128(const char* label, __uint128_t t) {
  const uint32_t hi = static_cast<uint32_t>(t >> 64);
  const uint64_t lo = static_cast<uint64_t>(t);
  std::printf("%s = %05x_%016llx\n", label, hi, static_cast<unsigned long long>(lo));
}

[[noreturn]] void mutant_result(const char* name, bool detected) {
  std::printf("[%s] %s\n", name, detected ? "DETECTED" : "MISSED");
  zhao::exit_hard(detected ? 0 : 1);
}

}  // namespace

int main() {
  Dut d;

  d.writer_i = kWriter;
  d.slot_i = kSlot;
  d.generation_i = kGeneration;
  d.mode_i = kMode;
  d.base_i = kBase;
  d.span_i = kSpan;
  write_tuple(d, expected_tuple());
  d.eval();

#if defined(ZHAO_EXPECT_FB_TUPLE_MUTANT_SWAP_WRITER_SLOT)
  const bool origins_moved = (d.writer_lo_o != 83) || (d.slot_lo_o != 82);
  const bool literal_disagrees = (read_packed(d) != expected_tuple());
  // Both must be true, and the second is the one that matters: it says the
  // LITERAL caught it. If only the origins moved, this test would be reporting
  // that it can read the package's own constants back, which proves nothing.
  mutant_result("fb_tuple_swap_writer_slot", origins_moved && literal_disagrees);
#else
  using zhao::check;

  check(d.span_lo_o == 0, "span sits at bit 0", 0, d.span_lo_o);
  check(d.base_lo_o == 32, "base sits at bit 32", 32, d.base_lo_o);
  check(d.mode_lo_o == 64, "mode sits at bit 64", 64, d.mode_lo_o);
  check(d.gen_lo_o == 66, "generation sits at bit 66", 66, d.gen_lo_o);
  check(d.slot_lo_o == 82, "slot sits at bit 82", 82, d.slot_lo_o);
  // 82 is not an arbitrary number: zhao_video_ready_bridge_v2.sv:164 reads
  // the slot from exactly there. If this check ever fails, that line is why.
  check(d.writer_lo_o == 83, "writer sits at bit 83", 83, d.writer_lo_o);

  const __uint128_t got = read_packed(d);
  const __uint128_t want = expected_tuple();
  check(got == want, "the packed tuple matches bit positions written here", 1, got == want);
  if (got != want) {
    print_128("  got ", got);
    print_128("  want", want);
  }

  check(d.writer_o == kWriter, "writer unpacks", kWriter, d.writer_o);
  check(d.slot_o == kSlot, "slot unpacks", kSlot, d.slot_o);
  check(d.generation_o == kGeneration, "generation unpacks", kGeneration, d.generation_o);
  check(d.mode_o == kMode, "mode unpacks", kMode, d.mode_o);
  check(d.base_o == kBase, "base unpacks", kBase, d.base_o);
  check(d.span_o == kSpan, "span unpacks", kSpan, d.span_o);

  // A field must not bleed into its neighbour. Setting one field to all ones
  // and the rest to zero is the cheap version of the coverage check the package
  // does at elaboration, done from outside where a wrong constant cannot hide.
  d.writer_i = 0;
  d.slot_i = 0;
  d.generation_i = 0xFFFFu;
  d.mode_i = 0;
  d.base_i = 0;
  d.span_i = 0;
  d.eval();
  const __uint128_t only_gen = read_packed(d);
  const __uint128_t want_gen = static_cast<__uint128_t>(0xFFFFu) << 66;
  check(only_gen == want_gen,
        "an all-ones generation touches bits 81:66 and nothing else", 1,
        only_gen == want_gen);

  d.generation_i = 0;
  d.span_i = 0xFFFFFFFFu;
  d.eval();
  const __uint128_t only_span = read_packed(d);
  const __uint128_t want_span = static_cast<__uint128_t>(0xFFFFFFFFu) << 0;
  check(only_span == want_span,
        "an all-ones span touches bits 31:0 and nothing else", 1,
        only_span == want_span);

  std::printf("[fb_tuple] layout writer=%u slot=%u gen=%u mode=%u base=%u span=%u\n", d.writer_lo_o,
              d.slot_lo_o, d.gen_lo_o, d.mode_lo_o, d.base_lo_o, d.span_lo_o);

  d.final();
  zhao::exit_hard(zhao::report_and_exit("fb_tuple_directed"));
#endif
}
