// geom_clipdoor_mutant_control.cpp -- THE POSITIVE CONTROL for
// `zhao_geom_clipdoor.err_hold_broken_o`.
//
// ITS POLARITY IS INVERTED. This binary passes when the counter FIRES. It
// elaborates `tests/mutants/zhao_geom_clipdoor_mutant.sv`, whose single
// substantive change removes the `!owner_offering_c` term from `take_new_c` --
// the hold law itself.
//
// It is evidence about the INSTRUMENT, not about the design.
// `geom_clipdoor_directed.cpp` asserts the counter stays at zero under every
// legal stimulus; a negative control alone is a claim, and this is the other
// half of it.
//
// THE STIMULUS THE MUTANT NEEDS, and why it is exactly the composed console's
// commonest state: client 0 holds a beat into a sink that is NOT READY, while
// client 1 asserts valid. `zhao_geom_clip` produces that state whenever its
// stage 3 is holding an accepted packet the shell's triangle door will not
// take, which on the attrpack fork's own numbers is roughly thirteen clocks in
// every fourteen.
#include <cstdint>
#include <cstdio>

#include "Vtb_geom_clipdoor_mutant.h"
#include "zhao_sim.hpp"

using zhao::check;
using zhao::tick;

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  auto* top = new Vtb_geom_clipdoor_mutant;
  auto& d = *top;

  d.rst_n = 0;
  d.c0_valid_i = 0;
  d.c1_valid_i = 0;
  d.o_ready_i = 0;
  d.c0_ax_i = 0x0000'AAA;
  d.c0_material_id_i = 0x1234;
  d.c0_material_mode_i = 0;
  d.c0_attr_witness_i = 0x1111'2222u;
  d.c1_ax_i = 0x0000'555;
  d.c1_material_id_i = 0x5678;
  d.c1_material_mode_i = 1;
  d.c1_attr_witness_i = 0x9999'8888u;
  d.eval();
  for (int i = 0; i < 3; ++i) tick(d);
  d.rst_n = 1;
  d.eval();
  tick(d);

  // Client 0 alone takes the door.
  d.o_ready_i = 1;
  d.c0_valid_i = 1;
  d.eval();
  tick(d);
  d.eval();
  check(d.o_owner_o == 0x1, "the mutant still grants client 0 first", 0x1,
        d.o_owner_o);

  // THE FAULT. Stall the sink with the beat offered, and let client 1 ask.
  d.o_ready_i = 0;
  d.c1_valid_i = 1;
  d.eval();

  const uint16_t held_id = d.o_material_id_o;
  const uint8_t held_owner = d.o_owner_o;

  tick(d);
  d.eval();

  check(held_owner == 0x1 && held_id == 0x1234,
        "the beat under the stall started as client 0's", 1,
        (held_owner == 0x1 && held_id == 0x1234));
  check(d.o_owner_o == 0x2,
        "THE FAULT IS REAL: the grant moved while the beat was offered and "
        "unaccepted",
        0x2, d.o_owner_o);
  check(d.o_material_id_o == 0x5678,
        "and the MATERIAL half moved with it -- this is the swap the counter "
        "watches for",
        0x5678, d.o_material_id_o);
  check(d.err_hold_broken_o != 0,
        "INVERTED POLARITY: err_hold_broken_o FIRED on the broken hold law", 1,
        d.err_hold_broken_o != 0);

  std::printf(
      "[geom_clipdoor_mutant_control] err_hold_broken=%u (MUST be non-zero; "
      "this control passes when the counter fires)\n",
      d.err_hold_broken_o);

  top->final();
  return zhao::report_and_exit("geom_clipdoor_mutant_control");
}
