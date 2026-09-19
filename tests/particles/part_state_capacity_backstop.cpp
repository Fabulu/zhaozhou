// part_state_capacity_backstop.cpp -- PART.STATE's `capacity_full_o`, gap I8.
//
// WHY THIS IS A SEPARATE SUITE AND NOT THREE MORE CHECKS IN
// part_state_directed.cpp: that suite's check count is quoted as evidence in
// the packet ledger and in the run log. A number that moves every time somebody
// adds a case is a number nobody can use, so the backstop gets its own file and
// part_state_directed stays at 78.
//
// WHAT IS CHECKED, and the sentence each comes from:
//
//   1. "PART.STATE knows when the generation is full and exposes no such
//      output" (zhao_console_core.sv, I8). So the bit must actually RISE when
//      the generation fills. A level output never seen to move is the same
//      claim as a counter reading zero.
//   2. It must count the STAGED children, not only the written ones. This is
//      the discriminating check, and it is TICK 3: a backstop built on
//      `written_q` alone reads LOW while the staging FIFO already holds
//      everything the remaining room can take, and the children it then lets
//      through are dropped one buffer later. TICK 3 fills staging with
//      `written_q` still at zero and requires the bit HIGH.
//   3. It must be LOW in S_IDLE. Between ticks `written_q` still holds the
//      previous generation's total; a stale "full" there would refuse the first
//      child of the NEXT tick. That is the one way this bit can lie, so it is
//      checked directly rather than argued.
//   4. It does NOT replace `children_dropped_capacity_o`. TICK 2 stages a child
//      and then fills the generation with survivors; the child is still dropped
//      in S_APPEND and still counted. Asserting that the backstop makes drops
//      unreachable would be asserting the wrong law.
//
// Parameters are PART.STATE's own bench parameters -- CAPACITY = 8,
// SPECIES_N = 4, CHILD_D = 8 -- because at the required tier of 32,768 a full
// generation is not reachable in a test at all.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_part_state.h"
#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"

using zhao::check;

namespace {

constexpr int kCapacity = 8;

struct Rec {
  uint64_t lo, hi;
};

// Built with the RATIFIED codec, never by hand-written shifts: the PART.STATE
// bench was already bitten once by a hand-rolled pack masking `age` to seven
// bits where the oracle says ten.
Rec pack(uint8_t species, int seed) {
  zref::part::Particle128 p{};
  p.pos[0] = 100 + seed;
  p.pos[1] = -200 - seed;
  p.pos[2] = 30 + seed;
  p.vel[0] = seed;
  p.vel[1] = -seed;
  p.vel[2] = 2 * seed;
  p.age = static_cast<uint16_t>(seed);
  p.species = species;
  p.size = static_cast<uint8_t>(8 + (seed & 7));
  p.spin = 0;
  p.flags = 0;
  p.variation = static_cast<uint8_t>(seed);
  uint64_t lo = 0, hi = 0;
  zref::part::particle_pack(p, &lo, &hi);
  return Rec{lo, hi};
}

template <typename W>
void put128(W& dst, const Rec& r) {
  dst[0] = static_cast<uint32_t>(r.lo);
  dst[1] = static_cast<uint32_t>(r.lo >> 32);
  dst[2] = static_cast<uint32_t>(r.hi);
  dst[3] = static_cast<uint32_t>(r.hi >> 32);
}

struct Dut {
  Vzhao_part_state* v;

  explicit Dut(Vzhao_part_state* d) : v(d) {}

  void tick() {
    v->clk = 0;
    v->eval();
    v->clk = 1;
    v->eval();
    v->clk = 0;
    v->eval();
  }

  void reset() {
    v->rst_n = 0;
    v->tick_start_i = 0;
    v->rd_valid_i = 0;
    v->rd_last_i = 0;
    v->prt_ready_i = 1;
    v->vrd_valid_i = 0;
    v->vrd_survive_i = 0;
    v->chl_valid_i = 0;
    v->wr_ready_i = 1;
    for (int i = 0; i < 4; ++i) {
      v->rd_record_i[i] = 0;
      v->vrd_record_i[i] = 0;
      v->chl_record_i[i] = 0;
    }
    tick();
    tick();
    v->rst_n = 1;
    tick();
  }

  void start_tick() {
    v->tick_start_i = 1;
    tick();
    v->tick_start_i = 0;
    v->eval();
  }

  // Ready is sampled BEFORE the edge that transfers, so no beat is ever
  // transferred twice -- the re-submission fault this repository has a chapter
  // about is exactly what a "tick then re-check" loop produces.
  bool offer_record(const Rec& rec, bool last) {
    put128(v->rd_record_i, rec);
    v->rd_valid_i = 1;
    v->rd_last_i = last ? 1 : 0;
    v->eval();
    int guard = 0;
    while (!v->rd_ready_o && guard++ < 128) tick();
    const bool ok = v->rd_ready_o != 0;
    if (ok) tick();
    v->rd_valid_i = 0;
    v->rd_last_i = 0;
    v->eval();
    return ok;
  }

  bool offer_verdict(const Rec& rec, bool survive) {
    put128(v->vrd_record_i, rec);
    v->vrd_valid_i = 1;
    v->vrd_survive_i = survive ? 1 : 0;
    v->eval();
    int guard = 0;
    while (!v->vrd_ready_o && guard++ < 128) tick();
    const bool ok = v->vrd_ready_o != 0;
    if (ok) tick();
    v->vrd_valid_i = 0;
    v->vrd_survive_i = 0;
    v->eval();
    return ok;
  }

  bool push_child(const Rec& rec) {
    put128(v->chl_record_i, rec);
    v->chl_valid_i = 1;
    v->eval();
    int guard = 0;
    while (!v->chl_ready_o && guard++ < 128) tick();
    const bool ok = v->chl_ready_o != 0;
    if (ok) tick();
    v->chl_valid_i = 0;
    v->eval();
    return ok;
  }

  void run_to_idle() {
    int guard = 0;
    while (v->tick_busy_o && guard++ < 1024) tick();
  }
};

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Verilated::traceEverOn(false);

  auto* top = new Vzhao_part_state;  // heap + exit_hard: see zhao_sim.hpp
  Vzhao_part_state& r = *top;
  Dut d(top);
  d.reset();

  // ==========================================================================
  // 3. LOW BEFORE ANY TICK. Nothing written, nothing staged.
  // ==========================================================================
  check(r.capacity_full_o == 0, "idle before any tick: the backstop is LOW", 0u, r.capacity_full_o);

  // ==========================================================================
  // TICK 1 -- CAPACITY survivors, no children. The bit must stay low while the
  // generation has room and rise on exactly the record that fills it.
  // ==========================================================================
  d.start_tick();

  bool low_while_room = true;
  int rose_after = -1;
  for (int i = 0; i < kCapacity; ++i) {
    const Rec rec = pack(1, i + 1);
    d.offer_record(rec, i == kCapacity - 1);
    d.offer_verdict(rec, true);
    if (i < kCapacity - 1 && r.capacity_full_o) low_while_room = false;
    if (r.capacity_full_o && rose_after < 0) rose_after = i + 1;
  }

  check(low_while_room, "the backstop stays LOW while the generation has room", 1,
        low_while_room ? 1 : 0);
  check(rose_after == kCapacity, "the backstop RISES on exactly the record that fills the tier",
        static_cast<uint64_t>(kCapacity), static_cast<uint64_t>(rose_after < 0 ? 0 : rose_after));
  check(r.capacity_full_o == 1, "the backstop is HIGH with the generation full", 1u,
        r.capacity_full_o);
  check(r.survivors_o == static_cast<uint32_t>(kCapacity), "all CAPACITY survivors were written",
        static_cast<uint64_t>(kCapacity), r.survivors_o);

  d.run_to_idle();

  // ==========================================================================
  // 3 again, and this is the one that catches a stale level: the tick has
  // ended, `written_q` still holds CAPACITY, and the bit must be LOW.
  // ==========================================================================
  check(r.capacity_full_o == 0, "back in idle with written_q still full: the backstop is LOW", 0u,
        r.capacity_full_o);

  // ==========================================================================
  // TICK 2 -- 4. THE BACKSTOP DOES NOT REPLACE THE DROP COUNTER. One child is
  // staged first, then CAPACITY survivors take every place. S_APPEND must still
  // drop the child and still count it.
  // ==========================================================================
  const uint32_t dropped0 = r.children_dropped_capacity_o;
  d.start_tick();
  d.push_child(pack(1, 99));
  for (int i = 0; i < kCapacity; ++i) {
    const Rec rec = pack(1, 20 + i);
    d.offer_record(rec, i == kCapacity - 1);
    d.offer_verdict(rec, true);
  }
  d.run_to_idle();
  check(r.children_dropped_capacity_o - dropped0 == 1,
        "a child staged past capacity is still DROPPED and still counted", 1u,
        r.children_dropped_capacity_o - dropped0);

  // ==========================================================================
  // TICK 3 -- 2. THE DISCRIMINATING CHECK. No survivors at all: fill staging
  // with CAPACITY children and require the bit HIGH while nothing whatever has
  // been written this tick. A backstop reading `written_q` alone is LOW here.
  // ==========================================================================
  const uint32_t survivors_before = r.survivors_o;
  const uint32_t written_before = r.children_written_o;
  d.start_tick();
  for (int i = 0; i < kCapacity; ++i) d.push_child(pack(1, 40 + i));

  const bool nothing_written_this_tick =
      (r.survivors_o == survivors_before) && (r.children_written_o == written_before);
  check(nothing_written_this_tick, "TICK 3 really has written nothing yet", 1,
        nothing_written_this_tick ? 1 : 0);
  check(r.capacity_full_o == 1, "STAGED children count toward the backstop, not only written ones",
        1u, r.capacity_full_o);

  std::printf("[part_state_capacity_backstop] survivors=%u children_written=%u dropped=%u\n",
              r.survivors_o, r.children_written_o, r.children_dropped_capacity_o);
  zhao::exit_hard(zhao::report_and_exit("part_state_capacity_backstop"));
}
