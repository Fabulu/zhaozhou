// ident_encoding_directed.cpp
//
// ---------------------------------------------------------------------------
// THE ENCODING FREEZE, CHECKED AGAINST THE BITS ALREADY ON THE WIRES
// ---------------------------------------------------------------------------
// Owner brief §4.1 requires the shared vocabulary to be a BIT-IDENTICAL
// migration: "It must not widen, truncate or reorder existing wires." So the
// model this checks against is not a re-derivation from the prose -- it is the
// literal concatenation each site performs today, written out once here:
//
//     owner  = {slot[5:0], gen[7:0]}
//     sample = {slot[5:0], sidx[1:0], gen[7:0]}
//     token  = {cls[1:0], sample[15:0]}
//     ticket = {gen[7:0], slot[5:0]}          <-- REVERSED vs owner
//
// If the package and these expressions ever disagree, adopting the package
// would change bits, and the whole point is that it must not.
//
// The brief also names what the test must actually exercise: "slot bits 4 and
// 5, generation 0/255, and every adapter in the actual path" -- the bits the
// 4-to-6 widening created, and the generation extremes. Those are asserted as
// non-vacuity checks below rather than assumed from a loop bound.
#include "Vzhao_texture_ident_probe.h"

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_ident_probe* d = new Vzhao_texture_ident_probe;

  long compared = 0;
  long owner_bad = 0, sample_bad = 0, token_bad = 0, ticket_bad = 0;
  long sowner_bad = 0, sidx_bad = 0, legal_bad = 0, roundtrip_bad = 0;
  long saw_slot_b4 = 0, saw_slot_b5 = 0, saw_gen_0 = 0, saw_gen_255 = 0;
  long saw_illegal = 0, ticket_differs_from_owner = 0;

  // All 64 x 256 x 4 x 4 = 262,144 combinations -- the same space the brief's
  // own bundled check covers, so the two can be compared directly.
  for (uint32_t slot = 0; slot < 64; ++slot) {
    for (uint32_t gen = 0; gen < 256; ++gen) {
      for (uint32_t sidx = 0; sidx < 4; ++sidx) {
        for (uint32_t cls = 0; cls < 4; ++cls) {
          d->slot_i = slot;
          d->gen_i = gen;
          d->sidx_i = sidx;
          d->cls_i = cls;
          d->eval();
          ++compared;

          const uint32_t owner = (slot << 8) | gen;
          const uint32_t sample = (slot << 10) | (sidx << 8) | gen;
          const uint32_t token = (cls << 16) | sample;
          const uint32_t ticket = (gen << 6) | slot;

          if (d->owner_o != owner) ++owner_bad;
          if (d->sample_o != sample) ++sample_bad;
          if (d->token_o != token) ++token_bad;
          if (d->ticket_o != ticket) ++ticket_bad;

          if (d->owner_slot_o != slot || d->owner_gen_o != gen) ++owner_bad;
          if (d->sample_owner_o != owner) ++sowner_bad;
          if (d->sample_idx_o != sidx) ++sidx_bad;
          if (d->token_class_o != cls || d->token_sample_o != sample) ++token_bad;
          if (d->sample_legal_o != (sidx != 3 ? 1 : 0)) ++legal_bad;
          if (d->ticket_back_o != owner) ++roundtrip_bad;

          if (slot & 0x10u) ++saw_slot_b4;
          if (slot & 0x20u) ++saw_slot_b5;
          if (gen == 0) ++saw_gen_0;
          if (gen == 255) ++saw_gen_255;
          if (sidx == 3) ++saw_illegal;
          if (ticket != owner) ++ticket_differs_from_owner;
        }
      }
    }
  }

  std::printf(
      "  %ld combinations | slot b4 %ld, b5 %ld | gen0 %ld, gen255 %ld"
      " | sidx=3 %ld\n",
      compared, saw_slot_b4, saw_slot_b5, saw_gen_0, saw_gen_255, saw_illegal);

  zhao::check(compared == 262144, "all 262,144 encodings were exercised", 262144, compared);
  zhao::check(owner_bad == 0,
              "the OWNER handle is bit-identical to {slot[5:0], gen[7:0]} and "
              "unpacks to the same fields",
              0, owner_bad);
  zhao::check(sample_bad == 0, "the SAMPLE handle is bit-identical to {slot, sidx, gen}", 0,
              sample_bad);
  zhao::check(sowner_bad == 0,
              "and every sample of a fragment maps to the SAME owner -- the "
              "index is dropped, not folded in",
              0, sowner_bad);
  zhao::check(sidx_bad == 0, "the sample index unpacks exactly", 0, sidx_bad);
  zhao::check(token_bad == 0,
              "the ROUTE token is bit-identical to {class[1:0], sample[15:0]} "
              "and unpacks to the same class and handle. This is the slice that "
              "was hardcoded [15:14] and stayed legal while the token widened "
              "to 18 bits",
              0, token_bad);
  zhao::check(ticket_bad == 0, "the T2 TICKET is bit-identical to {gen[7:0], slot[5:0]}", 0,
              ticket_bad);
  zhao::check(roundtrip_bad == 0, "and ticket -> owner round-trips exactly", 0, roundtrip_bad);
  zhao::check(legal_bad == 0,
              "sample index 3 is representable and reported ILLEGAL -- a value "
              "to reject, not a value that cannot arrive",
              0, legal_bad);

  // ---- non-vacuity, and the trap the brief names ---------------------------
  zhao::check(saw_slot_b4 > 0 && saw_slot_b5 > 0,
              "slot bits 4 and 5 were exercised -- the two bits the 4-to-6 "
              "widening created, and the ones every stale slice dropped",
              1, (saw_slot_b4 > 0 && saw_slot_b5 > 0) ? 1 : 0);
  zhao::check(saw_gen_0 > 0 && saw_gen_255 > 0, "generation 0 and 255 were both exercised", 1,
              (saw_gen_0 > 0 && saw_gen_255 > 0) ? 1 : 0);
  zhao::check(saw_illegal > 0,
              "and sample index 3 actually occurred, so the legality check is "
              "not vacuous",
              1, saw_illegal > 0 ? 1 : 0);

  // THE OWNER AND THE TICKET ARE NOT THE SAME INTEGER. Both are 14 bits, so no
  // width check can catch a confusion between them; this is the assertion that
  // records that they genuinely differ for most inputs.
  zhao::check(ticket_differs_from_owner > 200000,
              "the T2 ticket differs from the public owner for the vast "
              "majority of inputs. Both are 14 bits wide, so assigning one to "
              "the other elaborates, lints and silently addresses a different "
              "owner -- which is exactly why they get separate named functions",
              1, ticket_differs_from_owner > 200000 ? 1 : 0);

  const int rc = zhao::report_and_exit("ident_encoding_directed");
  delete d;
  zhao::exit_hard(rc);
}
