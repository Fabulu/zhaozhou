// proj_cfgvalid_directed.cpp -- the shared projector's CONFIG-VALID ARM:
// `zhao_proj_cfgvalid`, the producer `zhao_console_core.sv`'s entry I14 names
// as the thing `proj_en_i` has never had.
//
// WHAT THIS LANE WOULD CATCH:
//
//   1. THE ARM IS A COVERAGE SET, NOT A COUNT. Sixteen writes to ONE address
//      must not arm the projector. This is the case that catches the obvious
//      implementation -- a write counter -- which would report a full bank
//      with fifteen holes in it, and would do so while every instrument read
//      right. Red also if the mask is set by the wrong index.
//   2. THE ARM IS A SET, NOT A SEQUENCE. The same sixteen addresses delivered
//      out of order, with idle gaps and the other view interleaved, must arm
//      exactly as the tidy walk does. The producer's cadence has already
//      changed twice (16 -> 17 -> 21 steps) and a block that encoded it would
//      have gone silently wrong at each.
//   3. NON-MATRIX ADDRESSES DO NOT ARM IT. Addresses 16, 17 and 18 are the
//      viewport rectangle and the depth profile. They are real configuration
//      and they are not a camera, so a bank holding only those must stay
//      disarmed -- projecting through a zero matrix is the exact fault the
//      block exists to prevent, and a viewport rectangle does nothing about
//      it.
//   4. EITHER VIEW ARMS IT, AND THIS IS THE ONE THAT WOULD HANG THE CONSOLE.
//      View 0's bank alone must arm, and view 1's bank alone must arm. A
//      conjunction reads as the conservative choice and never arms in a
//      single-view presentation contract, because `zhao_cmd_exec` runs its
//      view walk for view 0 only in Z60. The failure mode is a console that
//      draws nothing, so it is asserted from both sides here rather than
//      discovered there. The third sub-case is its opposite: two HALF banks
//      must not add up to one whole one, which is what a single shared mask
//      would do.
//   5. A WRITE WITHOUT `cfg_we_i` DOES NOTHING. The address and view lines
//      move continuously on a merged bus; only the write strobe is the event.
//   6. THE ARM LATCHES AND NEVER FALLS. Hundreds of idle cycles, then further
//      walks rewriting words in place, and the enable is still high and the
//      event count is still one. A block that re-armed per walk would freeze
//      the projector for the length of every SetView -- a throttle nobody
//      asked for.
//   7. RESET CLEARS IT -- the masks as well as the arm, which is why fifteen
//      words afterwards are not enough.
//   8. BOTH COUNTERS FIRE AND BOTH DISCRIMINATE (R95). `arm_events_o` is the
//      positive control for the arm itself. `held_offers_o` must count offers
//      made while disarmed and must STOP counting once armed -- a counter that
//      kept moving would be counting offers, not WITHHELD offers, and the
//      difference is the whole claim the port makes. Its negative control is
//      in the same breath: forty offers after the arm move it by zero.
#include "Vzhao_proj_cfgvalid.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int kMatWords = 16;  // zhao_proj_cfgvalid MAT_WORDS

void reset_dut(Vzhao_proj_cfgvalid& dut) {
  dut.rst_n = 0;
  dut.clk = 0;
  dut.cfg_we_i = 0;
  dut.cfg_view_i = 0;
  dut.cfg_addr_i = 0;
  dut.offer_i = 0;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

/** One configuration write. Nothing here is a term in any handshake. */
void cfg_write(Vzhao_proj_cfgvalid& dut, int view, int addr) {
  dut.cfg_we_i = 1;
  dut.cfg_view_i = static_cast<uint8_t>(view);
  dut.cfg_addr_i = static_cast<uint8_t>(addr);
  zhao::tick(dut);
  dut.cfg_we_i = 0;
  dut.cfg_addr_i = 0;
  dut.cfg_view_i = 0;
  dut.eval();
}

/** The address and view lines move; the strobe does not. */
void cfg_quiet_move(Vzhao_proj_cfgvalid& dut, int view, int addr) {
  dut.cfg_we_i = 0;
  dut.cfg_view_i = static_cast<uint8_t>(view);
  dut.cfg_addr_i = static_cast<uint8_t>(addr);
  zhao::tick(dut);
  dut.eval();
}

/** `cycles` clocks on which a client offers a vertex to the projector. */
void offer(Vzhao_proj_cfgvalid& dut, int cycles) {
  dut.offer_i = 1;
  for (int i = 0; i < cycles; ++i) zhao::tick(dut);
  dut.offer_i = 0;
  dut.eval();
}

void idle(Vzhao_proj_cfgvalid& dut, int cycles) {
  for (int i = 0; i < cycles; ++i) zhao::tick(dut);
}

/** The whole matrix bank of one view, in address order. */
void write_bank(Vzhao_proj_cfgvalid& dut, int view) {
  for (int a = 0; a < kMatWords; ++a) cfg_write(dut, view, a);
}

// ---------------------------------------------------------------------------
// 1. THE ARM IS A COVERAGE SET, NOT A COUNT
// ---------------------------------------------------------------------------
void case_not_a_count() {
  Vzhao_proj_cfgvalid dut;
  reset_dut(dut);

  // Sixteen writes. One address. A write counter would call this a full bank.
  for (int i = 0; i < kMatWords; ++i) cfg_write(dut, 0, 3);
  check(dut.en_o == 0, "16 writes to one address do not arm", 0, dut.en_o);
  check(dut.arm_events_o == 0, "... and record no arm event", 0,
        dut.arm_events_o);

  // The other fifteen addresses complete the SET, and only then.
  for (int a = 0; a < kMatWords; ++a) {
    if (a == 3) continue;
    check(dut.en_o == 0, "bank incomplete while an address is missing", 0,
          dut.en_o);
    cfg_write(dut, 0, a);
  }
  check(dut.en_o == 1, "the completed coverage set arms it", 1, dut.en_o);
  check(dut.arm_events_o == 1, "exactly one arm event", 1, dut.arm_events_o);
}

// ---------------------------------------------------------------------------
// 2. THE ARM IS A SET, NOT A SEQUENCE
// ---------------------------------------------------------------------------
void case_not_a_sequence() {
  Vzhao_proj_cfgvalid dut;
  reset_dut(dut);

  for (int a = kMatWords - 1; a >= 0; --a) {
    check(dut.en_o == 0, "still disarmed mid-walk", 0, dut.en_o);
    cfg_write(dut, 0, a);
    idle(dut, 2);
    cfg_write(dut, 1, a);
    idle(dut, 1);
  }
  check(dut.en_o == 1, "an out-of-order, gapped, interleaved bank arms it", 1,
        dut.en_o);
  check(dut.arm_events_o == 1, "exactly one arm event", 1, dut.arm_events_o);
}

// ---------------------------------------------------------------------------
// 3. NON-MATRIX ADDRESSES DO NOT ARM IT
// ---------------------------------------------------------------------------
void case_viewport_is_not_a_camera() {
  Vzhao_proj_cfgvalid dut;
  reset_dut(dut);

  for (int rep = 0; rep < 4; ++rep)
    for (int a = kMatWords; a < 32; ++a) {
      cfg_write(dut, 0, a);
      cfg_write(dut, 1, a);
    }
  check(dut.en_o == 0, "viewport and profile words do not arm the projector",
        0, dut.en_o);
  check(dut.arm_events_o == 0, "... and record no arm event", 0,
        dut.arm_events_o);

  // And the matrix still arms it afterwards, so the case above is a REFUSAL
  // and not a wedged block.
  write_bank(dut, 0);
  check(dut.en_o == 1, "the matrix bank still arms it after them", 1,
        dut.en_o);
}

// ---------------------------------------------------------------------------
// 4. EITHER VIEW ARMS IT -- BOTH SIDES, AND THE HALF-BANK CONTROL
// ---------------------------------------------------------------------------
void case_either_view() {
  {
    Vzhao_proj_cfgvalid dut;
    reset_dut(dut);
    write_bank(dut, 0);
    check(dut.en_o == 1, "view 0's bank alone arms it", 1, dut.en_o);
    check(dut.arm_events_o == 1, "view 0 alone: one arm event", 1,
          dut.arm_events_o);
  }
  {
    // THE CASE THAT WOULD HAVE HUNG Z60 IF THE LAW WERE A CONJUNCTION -- and
    // the mirror that catches a mask indexed by a constant instead of
    // `cfg_view_i`.
    Vzhao_proj_cfgvalid dut;
    reset_dut(dut);
    write_bank(dut, 1);
    check(dut.en_o == 1, "view 1's bank alone arms it", 1, dut.en_o);
    check(dut.arm_events_o == 1, "view 1 alone: one arm event", 1,
          dut.arm_events_o);
  }
  {
    // Eight words of each view. A block sharing one mask would arm here.
    Vzhao_proj_cfgvalid dut;
    reset_dut(dut);
    for (int a = 0; a < 8; ++a) cfg_write(dut, 0, a);
    for (int a = 8; a < kMatWords; ++a) cfg_write(dut, 1, a);
    check(dut.en_o == 0, "two half banks are not one whole bank", 0, dut.en_o);
    check(dut.arm_events_o == 0, "... and record no arm event", 0,
          dut.arm_events_o);
  }
}

// ---------------------------------------------------------------------------
// 5. A WRITE WITHOUT THE STROBE DOES NOTHING
// ---------------------------------------------------------------------------
void case_strobe_is_the_event() {
  Vzhao_proj_cfgvalid dut;
  reset_dut(dut);

  for (int rep = 0; rep < 3; ++rep)
    for (int a = 0; a < kMatWords; ++a) {
      cfg_quiet_move(dut, 0, a);
      cfg_quiet_move(dut, 1, a);
    }
  check(dut.en_o == 0, "address traffic without `cfg_we_i` does not arm it", 0,
        dut.en_o);
  write_bank(dut, 0);
  check(dut.en_o == 1, "and the strobed bank does", 1, dut.en_o);
}

// ---------------------------------------------------------------------------
// 6. THE ARM LATCHES AND NEVER FALLS
// ---------------------------------------------------------------------------
void case_latches() {
  Vzhao_proj_cfgvalid dut;
  reset_dut(dut);

  write_bank(dut, 0);
  check(dut.en_o == 1, "armed", 1, dut.en_o);
  idle(dut, 500);
  check(dut.en_o == 1, "still armed after 500 idle clocks", 1, dut.en_o);

  // A second and third walk, rewriting words in place.
  write_bank(dut, 0);
  write_bank(dut, 1);
  for (int a = kMatWords; a < 19; ++a) cfg_write(dut, 0, a);
  check(dut.en_o == 1, "still armed across further walks", 1, dut.en_o);
  check(dut.arm_events_o == 1, "and it is still ONE arm event, not three", 1,
        dut.arm_events_o);
}

// ---------------------------------------------------------------------------
// 7. RESET CLEARS IT
// ---------------------------------------------------------------------------
void case_reset() {
  Vzhao_proj_cfgvalid dut;
  reset_dut(dut);

  write_bank(dut, 0);
  offer(dut, 3);  // after the arm, so this must not move the held counter
  check(dut.en_o == 1, "armed before reset", 1, dut.en_o);
  check(dut.held_offers_o == 0, "offers after the arm are not withheld", 0,
        dut.held_offers_o);

  reset_dut(dut);
  check(dut.en_o == 0, "reset disarms it", 0, dut.en_o);
  check(dut.arm_events_o == 0, "reset clears the arm event count", 0,
        dut.arm_events_o);
  check(dut.held_offers_o == 0, "reset clears the held-offer count", 0,
        dut.held_offers_o);

  // The MASKS are cleared too, not merely the arm.
  for (int a = 0; a < kMatWords - 1; ++a) cfg_write(dut, 0, a);
  check(dut.en_o == 0, "the masks were cleared by reset too", 0, dut.en_o);
  cfg_write(dut, 0, kMatWords - 1);
  check(dut.en_o == 1, "and the sixteenth arms it again", 1, dut.en_o);
}

// ---------------------------------------------------------------------------
// 8. BOTH COUNTERS FIRE, AND `held_offers_o` DISCRIMINATES
// ---------------------------------------------------------------------------
void case_counters_discriminate() {
  Vzhao_proj_cfgvalid dut;
  reset_dut(dut);

  check(dut.held_offers_o == 0, "no offers withheld yet", 0,
        dut.held_offers_o);

  // SEVEN clocks of a client offering a vertex into a projector that has no
  // camera. This is the fault the block exists to prevent, and this counter is
  // the only evidence that it was prevented.
  offer(dut, 7);
  check(dut.held_offers_o == 7, "seven withheld offers counted", 7,
        dut.held_offers_o);
  check(dut.en_o == 0, "still disarmed", 0, dut.en_o);
  check(dut.arm_events_o == 0, "and no arm event", 0, dut.arm_events_o);

  // A partial bank still withholds, so the counter must still move.
  for (int a = 0; a < kMatWords - 1; ++a) cfg_write(dut, 0, a);
  offer(dut, 5);
  check(dut.held_offers_o == 12, "a partial bank still withholds, and counts",
        12, dut.held_offers_o);

  // THE ARM, AND THE COUNTER'S NEGATIVE CONTROL IN THE SAME BREATH.
  cfg_write(dut, 0, kMatWords - 1);
  check(dut.en_o == 1, "the bank completed and armed", 1, dut.en_o);
  check(dut.arm_events_o == 1, "one arm event, fired by stimulus", 1,
        dut.arm_events_o);
  offer(dut, 40);
  check(dut.held_offers_o == 12,
        "forty offers after the arm move the counter by zero", 12,
        dut.held_offers_o);
}

}  // namespace

int main() {
  case_not_a_count();
  case_not_a_sequence();
  case_viewport_is_not_a_camera();
  case_either_view();
  case_strobe_is_the_event();
  case_latches();
  case_reset();
  case_counters_discriminate();

  zhao::exit_hard(zhao::report_and_exit("proj_cfgvalid_directed"));
}
