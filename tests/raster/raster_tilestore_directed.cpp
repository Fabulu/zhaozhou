// raster_tilestore_directed.cpp — RASTER.TILESTORE directed vectors
// (design/contracts/RASTER.TILESTORE.md "Directed tests"; law
// ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §8 "Active tile storage";
// oracle reference/src/zrender/tilestore.cpp).
//
// Every case runs the Verilated zhao_raster_tilestore AND zref::TileStore
// over the same cycle stimulus and requires every output to be IDENTICAL on
// every cycle. On top of that each case asserts its own law:
//
//   1. reset          — both banks read as all-zero at every one of the 256
//                       addresses, on both ports, before anything is written
//   2. word roundtrip — every address, every field: the charter §8 packing
//                       survives write→read bit for bit (exhaustive, 256)
//   3. clear          — one cycle, every address of the front bank reads the
//                       clear word afterwards, and a previously WRITTEN word
//                       is gone (the present bit really was dropped)
//   4. clear locks wr — wr_ready_o is low in a clear cycle and the write is
//                       NOT taken; the ordering rule that makes 3 sound
//   5. clear + read   — a read in the SAME cycle as a clear returns the NEW
//                       clear word (clear-then-read ordering)
//   6. bypass         — read-during-write: same address returns NEW data
//                       (write-first), adjacent addresses are untouched;
//                       swept across all 256 addresses
//   7. ping-pong      — a tile written to the front bank appears on the
//                       RESOLVE port after a swap and not before; the two
//                       banks hold different tiles simultaneously
//   8. isolation      — writes and clears of the front bank never disturb the
//                       back bank, INCLUDING a write aliasing the address the
//                       resolve port is reading in the same cycle (the dead
//                       bypass on port B, asserted rather than assumed)
//   9. swap ordering  — an access accepted in the same cycle as a swap uses
//                       the PRE-swap roles
//  10. src_id         — the read port carries source_id through untouched
//  11. counters       — tile_references counts accepted data accesses only;
//                       clear and swap are commands and do not count
//  12. full traffic   — all five channels busy on the same cycle, repeatedly

#include "raster_tilestore_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

using zhao::check;
using zhao_raster::Cycle;
using zhao_raster::kWords;
using zhao_raster::make_word;
using zhao_raster::same;
using zhao_raster::StoreDev;
using zhao_raster::StoreOut;
using zhao_raster::Word;

namespace {

// One Verilated model + one oracle for the whole suite, stepped in lockstep.
// A function-local static, NOT a global pointer to main's local (cppcheck
// danglingLifetime, and it is right).
StoreDev& dev() {
  static StoreDev d;
  return d;
}
zref::TileStore& ref() {
  static zref::TileStore r;
  return r;
}

uint32_t g_mismatch = 0;
uint32_t g_cycles = 0;

// The differential core: one cycle of stimulus through both models.
StoreOut cyc(const Cycle& c, const char* what) {
  const StoreOut want = ref().step(c);
  const StoreOut got = dev().step(c);
  ++g_cycles;
  if (!same(want, got)) {
    if (g_mismatch < 8) {
      std::printf("  %s: RTL != zref::TileStore\n    %s\n", what,
                  zhao_raster::describe(c, want, got).c_str());
    }
    ++g_mismatch;
  }
  return got;
}

Cycle nop() { return Cycle{}; }

// Read one address and return the word the RTL presented.
uint64_t rd(uint8_t addr, const char* what, uint16_t src = 0) {
  Cycle c = nop();
  c.rd = true;
  c.rd_addr = addr;
  c.rd_src_id = src;
  return cyc(c, what).rd_data;
}

uint64_t rd_back(uint8_t addr, const char* what) {
  Cycle c = nop();
  c.res = true;
  c.res_addr = addr;
  return cyc(c, what).res_data;
}

void wr(uint8_t addr, uint64_t data, const char* what) {
  Cycle c = nop();
  c.wr = true;
  c.wr_addr = addr;
  c.wr_data = data;
  cyc(c, what);
}

void clear(uint64_t data, const char* what) {
  Cycle c = nop();
  c.clear = true;
  c.clear_data = data;
  cyc(c, what);
}

void swap(const char* what) {
  Cycle c = nop();
  c.swap = true;
  cyc(c, what);
}

// --------------------------------------------------------------------------
void test_reset() {
  dev().reset();
  ref().reset();
  uint32_t nonzero = 0;
  for (int i = 0; i < kWords; ++i) {
    if (rd(static_cast<uint8_t>(i), "reset_front") != 0) ++nonzero;
    if (rd_back(static_cast<uint8_t>(i), "reset_back") != 0) ++nonzero;
  }
  check(nonzero == 0, "reset: both banks read all-zero at every address", 0, nonzero);
}

// --------------------------------------------------------------------------
void test_word_roundtrip() {
  // Exhaustive over the 256 addresses, with a distinct value in every field
  // so a colour/tag/depth/stencil swap in the packing cannot cancel out.
  for (int i = 0; i < kWords; ++i)
    wr(static_cast<uint8_t>(i), make_word(static_cast<uint32_t>(i)), "roundtrip_wr");
  uint32_t bad = 0;
  uint32_t field_bad = 0;
  for (int i = 0; i < kWords; ++i) {
    const uint64_t want = make_word(static_cast<uint32_t>(i));
    const uint64_t got = rd(static_cast<uint8_t>(i), "roundtrip_rd");
    if (got != want) ++bad;
    const Word a = Word::unpack(want);
    const Word b = Word::unpack(got);
    if (a.r != b.r || a.g != b.g || a.b != b.b || a.tag != b.tag || a.depth != b.depth ||
        a.stencil != b.stencil)
      ++field_bad;
  }
  check(bad == 0, "word roundtrip: all 256 addresses survive write->read", 0, bad);
  check(field_bad == 0, "word roundtrip: every charter 8 field survives", 0, field_bad);
}

// --------------------------------------------------------------------------
void test_clear() {
  const uint64_t sky = 0x1234'5678'9ABC'DEF0ull;
  // the store is full of test_word_roundtrip's data here — that is the point
  clear(sky, "clear_cmd");
  uint32_t bad = 0;
  for (int i = 0; i < kWords; ++i)
    if (rd(static_cast<uint8_t>(i), "clear_rd") != sky) ++bad;
  check(bad == 0, "clear: every address of the front bank reads the clear word", 0, bad);

  // a write after the clear is visible; its neighbours are still the clear
  wr(0x42, 0xAAAA'BBBB'CCCC'DDDDull, "clear_then_wr");
  check(rd(0x42, "clear_then_rd") == 0xAAAA'BBBB'CCCC'DDDDull,
        "clear: a write after the clear is visible", 1, 1);
  check(rd(0x41, "clear_neighbour") == sky, "clear: neighbours stay at the clear word", 1, 1);

  // and a second clear removes it again (the present bit really was dropped)
  const uint64_t sky2 = 0x0F0F'0F0F'0F0F'0F0Full;
  clear(sky2, "clear_cmd2");
  check(rd(0x42, "clear2_rd") == sky2, "clear: a re-clear drops a previously written word", 1, 1);
}

// --------------------------------------------------------------------------
void test_clear_locks_write() {
  const uint64_t sky = 0x0102'0304'0506'0708ull;
  Cycle c = nop();
  c.clear = true;
  c.clear_data = sky;
  c.wr = true;
  c.wr_addr = 0x10;
  c.wr_data = 0xDEAD'BEEF'DEAD'BEEFull;
  const StoreOut o = cyc(c, "clear_locks_wr");
  check(!o.wr_ready, "clear cycle: wr_ready_o is low", 0, o.wr_ready ? 1 : 0);
  check(rd(0x10, "clear_locks_rd") == sky, "clear cycle: the write was NOT taken", 1, 1);
}

// --------------------------------------------------------------------------
void test_clear_read_same_cycle() {
  const uint64_t sky = 0xFEED'FACE'CAFE'BABEull;
  wr(0x20, 0x1111'2222'3333'4444ull, "ccr_seed");
  Cycle c = nop();
  c.clear = true;
  c.clear_data = sky;
  c.rd = true;
  c.rd_addr = 0x20;
  c.res = true;
  c.res_addr = 0x20;
  const StoreOut o = cyc(c, "clear_read_same_cycle");
  check(o.rd_data == sky, "clear+read same cycle: the read sees the NEW clear word", 1,
        o.rd_data == sky ? 1 : 0);
  check(o.res_data == 0, "clear+read same cycle: the BACK bank is untouched", 0,
        o.res_data == 0 ? 0 : 1);
}

// --------------------------------------------------------------------------
void test_bypass() {
  // read-during-write, swept across every address: same address must return
  // the NEW word (write-first), the neighbour must be untouched.
  clear(0, "bypass_clear");
  uint32_t bad_same = 0;
  uint32_t bad_other = 0;
  for (int i = 0; i < kWords; ++i) {
    const uint8_t a = static_cast<uint8_t>(i);
    const uint8_t b = static_cast<uint8_t>((i + 1) & 0xFF);
    const uint64_t w = make_word(static_cast<uint32_t>(i) ^ 0x5A5Au);

    Cycle c = nop();
    c.wr = true;
    c.wr_addr = a;
    c.wr_data = w;
    c.rd = true;
    c.rd_addr = a;
    if (cyc(c, "bypass_same").rd_data != w) ++bad_same;

    // now write a DIFFERENT address while reading a: a must be unchanged
    Cycle d = nop();
    d.wr = true;
    d.wr_addr = b;
    d.wr_data = ~w;
    d.rd = true;
    d.rd_addr = a;
    if (cyc(d, "bypass_other").rd_data != w) ++bad_other;
  }
  check(bad_same == 0, "bypass: same-cycle same-address read returns the NEW word", 0, bad_same);
  check(bad_other == 0, "bypass: a write elsewhere does not disturb the read", 0, bad_other);
}

// --------------------------------------------------------------------------
void test_pingpong() {
  dev().reset();
  ref().reset();

  // tile A into the front bank
  for (int i = 0; i < kWords; ++i)
    wr(static_cast<uint8_t>(i), make_word(static_cast<uint32_t>(i) + 100u), "pp_wrA");

  // before the swap the resolve port still sees the (empty) back bank
  uint32_t early = 0;
  for (int i = 0; i < kWords; ++i)
    if (rd_back(static_cast<uint8_t>(i), "pp_earlyB") != 0) ++early;
  check(early == 0, "ping-pong: the resolve port does not see the working tile before a swap", 0,
        early);

  swap("pp_swap");

  uint32_t bad = 0;
  for (int i = 0; i < kWords; ++i)
    if (rd_back(static_cast<uint8_t>(i), "pp_rdB") != make_word(static_cast<uint32_t>(i) + 100u))
      ++bad;
  check(bad == 0, "ping-pong: after a swap the finished tile is on the resolve port", 0, bad);

  // the new front bank is the OTHER one — still empty
  uint32_t dirty = 0;
  for (int i = 0; i < kWords; ++i)
    if (rd(static_cast<uint8_t>(i), "pp_newfront") != 0) ++dirty;
  check(dirty == 0, "ping-pong: the new working bank is the other one", 0, dirty);

  // write tile B into the new front while tile A is still readable on B
  clear(0x00FF'00FF'00FF'00FFull, "pp_clearB");
  for (int i = 0; i < kWords; ++i)
    wr(static_cast<uint8_t>(i), make_word(static_cast<uint32_t>(i) + 900u), "pp_wrB");

  uint32_t both_bad = 0;
  for (int i = 0; i < kWords; ++i) {
    Cycle c = nop();
    c.rd = true;
    c.rd_addr = static_cast<uint8_t>(i);
    c.res = true;
    c.res_addr = static_cast<uint8_t>(i);
    const StoreOut o = cyc(c, "pp_both");
    if (o.rd_data != make_word(static_cast<uint32_t>(i) + 900u)) ++both_bad;
    if (o.res_data != make_word(static_cast<uint32_t>(i) + 100u)) ++both_bad;
  }
  check(both_bad == 0, "ping-pong: both tiles are live and distinct at the same time", 0, both_bad);

  swap("pp_swap2");
  uint32_t back_bad = 0;
  for (int i = 0; i < kWords; ++i)
    if (rd_back(static_cast<uint8_t>(i), "pp_rdB2") != make_word(static_cast<uint32_t>(i) + 900u))
      ++back_bad;
  check(back_bad == 0, "ping-pong: a second swap hands over tile B", 0, back_bad);
}

// --------------------------------------------------------------------------
void test_pingpong_isolation() {
  // The write port targets the FRONT bank only, so the port-B bypass is dead
  // by construction. Assert it: write address a on the front in the very
  // cycle the resolve port reads address a on the back.
  dev().reset();
  ref().reset();
  for (int i = 0; i < kWords; ++i)
    wr(static_cast<uint8_t>(i), make_word(static_cast<uint32_t>(i) + 7u), "iso_seed");
  swap("iso_swap");
  clear(0, "iso_clear_front");

  uint32_t leaked = 0;
  for (int i = 0; i < kWords; ++i) {
    Cycle c = nop();
    c.wr = true;
    c.wr_addr = static_cast<uint8_t>(i);
    c.wr_data = 0xFFFF'FFFF'FFFF'FFFFull;  // maximally different from the tile
    c.res = true;
    c.res_addr = static_cast<uint8_t>(i);
    if (cyc(c, "iso_alias").res_data != make_word(static_cast<uint32_t>(i) + 7u)) ++leaked;
  }
  check(leaked == 0, "isolation: a front-bank write never leaks into a same-address back read", 0,
        leaked);

  // ...and a front-bank CLEAR does not touch the back bank either
  clear(0xDEAD'DEAD'DEAD'DEADull, "iso_clear2");
  uint32_t clr_leak = 0;
  for (int i = 0; i < kWords; ++i)
    if (rd_back(static_cast<uint8_t>(i), "iso_after_clear") !=
        make_word(static_cast<uint32_t>(i) + 7u))
      ++clr_leak;
  check(clr_leak == 0, "isolation: a front-bank clear never touches the back bank", 0, clr_leak);
}

// --------------------------------------------------------------------------
void test_swap_ordering() {
  // An access accepted in the same cycle as a swap uses the PRE-swap roles.
  dev().reset();
  ref().reset();
  wr(0x33, 0xAAAA'AAAA'AAAA'AAAAull, "so_frontseed");
  swap("so_swap0");
  wr(0x33, 0xBBBB'BBBB'BBBB'BBBBull, "so_backseed");  // now the OTHER bank
  // bank0 = 0xAAAA..., bank1 = 0xBBBB..., front = bank1

  Cycle c = nop();
  c.rd = true;
  c.rd_addr = 0x33;
  c.res = true;
  c.res_addr = 0x33;
  c.swap = true;
  const StoreOut o = cyc(c, "so_swapcycle");
  check(o.rd_data == 0xBBBB'BBBB'BBBB'BBBBull,
        "swap ordering: the read in the swap cycle used the PRE-swap front bank", 1,
        o.rd_data == 0xBBBB'BBBB'BBBB'BBBBull ? 1 : 0);
  check(o.res_data == 0xAAAA'AAAA'AAAA'AAAAull,
        "swap ordering: the resolve read in the swap cycle used the PRE-swap back bank", 1,
        o.res_data == 0xAAAA'AAAA'AAAA'AAAAull ? 1 : 0);
  check(o.front_bank == false, "swap ordering: front_bank_o shows the POST-swap role", 0,
        o.front_bank ? 1 : 0);
  check(rd(0x33, "so_after") == 0xAAAA'AAAA'AAAA'AAAAull,
        "swap ordering: the next cycle sees the swapped roles", 1, 1);
}

// --------------------------------------------------------------------------
void test_src_id() {
  uint32_t bad = 0;
  for (uint32_t k = 0; k < 64; ++k) {
    const uint16_t src = static_cast<uint16_t>(k * 1013u + 7u);
    Cycle c = nop();
    c.rd = true;
    c.rd_addr = static_cast<uint8_t>(k);
    c.rd_src_id = src;
    if (cyc(c, "srcid").rd_src_id != src) ++bad;
  }
  check(bad == 0, "source_id: the read port carries it through untouched", 0, bad);
}

// --------------------------------------------------------------------------
void test_counters() {
  dev().reset();
  ref().reset();

  // commands do not count
  clear(0, "cnt_clear");
  swap("cnt_swap");
  Cycle idle = nop();
  const StoreOut after_cmds = cyc(idle, "cnt_idle");
  check(after_cmds.tile_references == 0, "counters: clear and swap are not tile references", 0,
        after_cmds.tile_references);

  // one accepted access per port per cycle
  Cycle c = nop();
  c.wr = true;
  c.wr_addr = 1;
  c.wr_data = 1;
  c.rd = true;
  c.rd_addr = 1;
  c.res = true;
  c.res_addr = 1;
  const StoreOut o = cyc(c, "cnt_three");
  check(o.tile_references == 3, "counters: three accepted accesses in one cycle count three", 3,
        o.tile_references);

  // a write refused by a clear does not count
  Cycle d = nop();
  d.clear = true;
  d.wr = true;
  d.wr_addr = 2;
  d.wr_data = 2;
  const StoreOut e = cyc(d, "cnt_refused");
  check(e.tile_references == 3, "counters: a write refused by a clear does not count", 3,
        e.tile_references);
}

// --------------------------------------------------------------------------
void test_full_traffic() {
  // Every channel busy on the same cycle, over and over: the case where the
  // clear/write/read/swap ordering rules all interact at once.
  dev().reset();
  ref().reset();
  for (uint32_t k = 0; k < 512; ++k) {
    Cycle c = nop();
    c.clear = (k % 37u) == 0u;
    c.clear_data = make_word(k);
    c.wr = true;
    c.wr_addr = static_cast<uint8_t>(k * 5u);
    c.wr_data = make_word(k + 1000u);
    c.rd = true;
    c.rd_addr = static_cast<uint8_t>(k * 5u + (k % 3u));
    c.rd_src_id = static_cast<uint16_t>(k);
    c.res = true;
    c.res_addr = static_cast<uint8_t>(k * 11u);
    c.swap = (k % 53u) == 0u;
    cyc(c, "full_traffic");
  }
  check(true, "full traffic: 512 cycles with every channel active", 1, 1);
}

}  // namespace

int main() {
  test_reset();
  test_word_roundtrip();
  test_clear();
  test_clear_locks_write();
  test_clear_read_same_cycle();
  test_bypass();
  test_pingpong();
  test_pingpong_isolation();
  test_swap_ordering();
  test_src_id();
  test_counters();
  test_full_traffic();

  check(dev().always_ready(),
        "backpressure: clear/read/resolve/swap have no stall condition of their own", 1,
        dev().always_ready() ? 1 : 0);
  std::printf("raster_tilestore_directed: %u cycles stepped in lockstep\n", g_cycles);
  check(g_mismatch == 0, "every cycle: RTL == zref::TileStore", 0, g_mismatch);

  return zhao::report_and_exit("raster_tilestore_directed");
}
