// field_v3_sbank_directed.cpp — the uniform (scalar) bank.
//
// There is no software oracle to differ against here: the bank is storage, and
// its contract is storage behaviour. So this checks the three things that can
// actually be got wrong, and each of them has already been got wrong once
// somewhere in this engine.
//
//   1. THE REGISTERED READ'S TIMING. The datum for the address presented on
//      cycle T arrives on T+1. The curve service's neighbour phase declared
//      itself finished on the cycle its last read was still arriving and
//      handed the arithmetic a value that had not been written yet -- 96
//      failures that looked like an edge case and were a structural error.
//      That timing is asserted here rather than assumed.
//
//   2. OUT OF RANGE REFUSES AND DOES NOT LAND. A slot number above the bank
//      must raise and must NOT write, because a wrapped index silently
//      overwrites somebody else's uniform and produces an answer that is
//      individually plausible and completely wrong.
//
//   3. THE FAULT LATCHES. A single bad write is the whole finding; a pulse
//      would be missed by any test that samples.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_v3_sbank.h"

#include "zhao_sim.hpp"

namespace {

constexpr int kSlots = 64;

void reset(Vzhao_field_v3_sbank& t) {
  t.rst_n = 0;
  t.we_i = 0;
  t.waddr_i = 0;
  t.wdata_i = 0;
  t.raddr_i = 0;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();
  zhao::tick(t);
}

void write_slot(Vzhao_field_v3_sbank& t, uint32_t addr, int32_t data) {
  t.we_i = 1;
  t.waddr_i = (uint16_t)addr;
  t.wdata_i = (uint32_t)data;
  t.eval();
  zhao::tick(t);
  t.we_i = 0;
  t.eval();
}

/** Present an address, take one clock, return what the registered port gives. */
int32_t read_slot(Vzhao_field_v3_sbank& t, uint32_t addr) {
  t.raddr_i = (uint8_t)addr;
  t.eval();
  zhao::tick(t);
  t.eval();
  return (int32_t)t.rdata_o;
}

/** A value that is distinctive per slot, signed, and not a small integer --
 *  so a wrong slot is obvious and a sign-extension bug cannot hide. */
int32_t pattern(int slot) {
  return (int32_t)(0xA5A50000u ^ (uint32_t)(slot * 0x01010101u)) ^ (slot & 1 ? -1 : 0);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_field_v3_sbank top;

  printf("== section 1: every slot holds what was written, and only that ==\n");
  {
    reset(top);
    zhao::check(top.we_bad_o == 0, "reset clears the fault", 0, (uint32_t)top.we_bad_o);

    for (int s = 0; s < kSlots; ++s) write_slot(top, (uint32_t)s, pattern(s));

    // Read back in a DIFFERENT order than written. Sequential read-back would
    // pass even if the read address were ignored and the port simply counted.
    int wrong = 0;
    for (int i = 0; i < kSlots; ++i) {
      const int s = (i * 37 + 11) % kSlots;  // 37 is coprime with 64
      const int32_t got = read_slot(top, (uint32_t)s);
      if (got != pattern(s)) {
        if (wrong < 4)
          printf("      slot %2d want %08X got %08X\n", s, (uint32_t)pattern(s), (uint32_t)got);
        ++wrong;
      }
    }
    zhao::check(wrong == 0, "all 64 slots read back correctly, out of order", 0, wrong);
  }

  printf("== section 2: the registered read arrives on the NEXT clock ==\n");
  {
    // The trap the curve service fell into. Presenting an address does not
    // make the datum available in the same cycle, and a consumer that believes
    // it does gets the PREVIOUS address's value -- which is right often enough
    // to pass a careless test.
    reset(top);
    write_slot(top, 5, (int32_t)0x11111111);
    write_slot(top, 9, (int32_t)0x22222222);

    top.raddr_i = 5;
    top.eval();
    const int32_t same_cycle = (int32_t)top.rdata_o;
    zhao::check(same_cycle != (int32_t)0x11111111,
                "the datum is NOT available in the address's own cycle", 1,
                same_cycle != (int32_t)0x11111111 ? 1 : 0);

    zhao::tick(top);
    top.eval();
    zhao::check((int32_t)top.rdata_o == (int32_t)0x11111111,
                "and it IS available on the next clock", (uint32_t)0x11111111,
                (uint32_t)top.rdata_o);

    // Back to back: address 9 presented while 5's datum is on the port.
    top.raddr_i = 9;
    top.eval();
    zhao::check((int32_t)top.rdata_o == (int32_t)0x11111111,
                "a new address does not disturb the datum already presented",
                (uint32_t)0x11111111, (uint32_t)top.rdata_o);
    zhao::tick(top);
    top.eval();
    zhao::check((int32_t)top.rdata_o == (int32_t)0x22222222,
                "and the next clock delivers the new one", (uint32_t)0x22222222,
                (uint32_t)top.rdata_o);
  }

  printf("== section 3: a slot the bank does not have is REFUSED, not wrapped ==\n");
  {
    reset(top);
    write_slot(top, 0, (int32_t)0x0BADF00D);
    zhao::check(top.we_bad_o == 0, "a legal write raises nothing", 0, (uint32_t)top.we_bad_o);

    // 64 is the first out-of-range slot and the one a wrap would fold onto 0.
    write_slot(top, 64, (int32_t)0xDEADBEEF);
    zhao::check(top.we_bad_o == 1, "slot 64 raises the fault", 1, (uint32_t)top.we_bad_o);
    zhao::check(read_slot(top, 0) == (int32_t)0x0BADF00D,
                "and slot 0 is UNTOUCHED -- the refused write did not land",
                (uint32_t)0x0BADF00D, (uint32_t)read_slot(top, 0));

    // The planner's slot numbers are uint16_t, so a program really can name a
    // slot in the thousands. 4000 & 63 == 32, which is a perfectly ordinary
    // slot -- exactly the silent corruption this guard exists to stop.
    reset(top);
    write_slot(top, 32, (int32_t)0x5150C0DE);
    write_slot(top, 4000, (int32_t)0x0);
    zhao::check(top.we_bad_o == 1, "a far out-of-range slot raises too", 1,
                (uint32_t)top.we_bad_o);
    zhao::check(read_slot(top, 32) == (int32_t)0x5150C0DE,
                "and slot 4000 & 63 == 32 was NOT overwritten", (uint32_t)0x5150C0DE,
                (uint32_t)read_slot(top, 32));
  }

  printf("== section 4: the fault LATCHES ==\n");
  {
    reset(top);
    write_slot(top, 99, 0);
    zhao::check(top.we_bad_o == 1, "the bad write raises", 1, (uint32_t)top.we_bad_o);
    for (int i = 0; i < 20; ++i) {
      write_slot(top, (uint32_t)(i % kSlots), (int32_t)i);
    }
    zhao::check(top.we_bad_o == 1, "and twenty good writes later it is STILL raised", 1,
                (uint32_t)top.we_bad_o);
    zhao::check(read_slot(top, 3) == 3, "while the good writes still landed", 3,
                (uint32_t)read_slot(top, 3));
  }

  return zhao::report_and_exit("field_v3_sbank_directed");
}
