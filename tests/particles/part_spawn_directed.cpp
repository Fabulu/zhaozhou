// part_spawn_directed.cpp -- PART.SPAWN against the promises its contract makes
// in its own words, and against the RATIFIED particle codec.
//
// Every record here is built and read with zref::part::particle_pack /
// particle_unpack. Hand-written bit shuffling in a bench is a second
// implementation of ratified arithmetic -- the failure CLAUDE.md has a chapter
// about, and the one that already bit the PART.STATE bench by masking `age` to
// seven bits where the oracle says ten.
//
// What is checked, and the contract sentence each comes from:
//
//   1. "parent stream order -> event order -> child index 0..N-1" -- the
//      determinism contract, and the only thing this block really sells.
//   2. "Maximum 16 children per event" / "child count > 16: refuse the group,
//      count it. Do not emit the first 16 -- a truncated burst is a different
//      effect, silently." So 16 must be ACCEPTED and 17 must emit NOTHING.
//   3. "unknown child species: refuse the group".
//   4. "capacity exhausted: later children are dropped deterministically".
//   5. "Child variation comes from the stateless hash, seeded by {parent_id,
//      event, child_index, tick} -- never from a running counter." So siblings
//      must differ AND the same parent at the same tick must reproduce exactly.
//   6. The ratified flag bits: a child is born this tick (0x4), and the
//      reserved bit (0x8) is "zero in, preserved zero".
//
// Not checked here because the block does not own it: what a child DOES. The
// contract puts child behaviour in the species descriptor and PART.UPDATE, and
// the position/velocity scale is an open Class-C question -- so the RTL derives
// them by identity behind a named knob and this bench asserts that identity
// rather than inventing the ruling.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_part_spawn.h"
#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"

using zhao::check;

namespace {

constexpr int kSpeciesN = 8;   // small on purpose: an out-of-range species must be reachable

struct Child {
  uint64_t lo, hi;
};

// The species table the DUT reads as a port. The bench owns it, because the
// contract says this block owns no table.
struct Desc {
  bool known;
  uint8_t child_species;
  uint8_t count;
};

Desc lookup(uint8_t parent_species, uint8_t event) {
  if (parent_species == 1) {
    switch (event) {
      case 0: return {true, 2, 3};    // birth     -> 3 children
      case 1: return {true, 3, 2};    // age mark  -> 2 children
      case 2: return {true, 2, 16};   // collision -> the legal maximum
      default: return {true, 2, 17};  // death     -> one over: refuse the GROUP
    }
  }
  if (parent_species == 2) {
    return {true, 9, 1};              // species 9 with SPECIES_N=8: out of range
  }
  return {false, 0, 0};               // no descriptor at all
}

zref::part::Particle128 make_parent(uint8_t species, int seed) {
  zref::part::Particle128 p{};
  p.pos[0] = 1000 + seed; p.pos[1] = -2000 - seed; p.pos[2] = 300 + seed;
  p.vel[0] = 7 + seed;    p.vel[1] = -9 - seed;    p.vel[2] = 11 + seed;
  p.age = static_cast<uint16_t>(500 + seed);   // 10 bits: the oracle's width
  p.species = species;
  p.size = static_cast<uint8_t>(20 + (seed & 7));
  p.spin = static_cast<uint8_t>(seed & 0x3F);
  p.flags = 0;
  p.variation = static_cast<uint8_t>(0xC3 ^ seed);
  return p;
}

struct Dut {
  Vzhao_part_spawn* v;
  std::vector<Child> out;

  explicit Dut(Vzhao_part_spawn* d) : v(d) {}

  template <typename W>
  static void put128(W& dst, uint64_t lo, uint64_t hi) {
    dst[0] = static_cast<uint32_t>(lo);
    dst[1] = static_cast<uint32_t>(lo >> 32);
    dst[2] = static_cast<uint32_t>(hi);
    dst[3] = static_cast<uint32_t>(hi >> 32);
  }
  template <typename W>
  static Child get128(const W& src) {
    Child c;
    c.lo = static_cast<uint64_t>(src[0]) | (static_cast<uint64_t>(src[1]) << 32);
    c.hi = static_cast<uint64_t>(src[2]) | (static_cast<uint64_t>(src[3]) << 32);
    return c;
  }

  // The descriptor port is combinational: answer whatever is being asked.
  void serve_descriptor() {
    const Desc d = lookup(static_cast<uint8_t>(v->spc_species_o),
                          static_cast<uint8_t>(v->spc_event_o));
    v->spc_known_i    = d.known ? 1 : 0;
    v->spc_child_spc_i = d.child_species;
    v->spc_count_i     = d.count;
  }

  void tick() {
    v->clk = 0;
    serve_descriptor();
    v->eval();
    if (v->chl_valid_o && v->chl_ready_i) out.push_back(get128(v->chl_record_o));
    v->clk = 1;
    serve_descriptor();
    v->eval();
  }

  void idle() {
    v->tick_start_i = 0;
    v->par_valid_i = 0;
    v->chl_ready_i = 1;
    v->cap_full_i = 0;
    serve_descriptor();
    v->eval();
  }

  // A REAL tick boundary. max_children_in_tick_o is a per-tick watermark, and
  // without this pulse it silently becomes a second copy of children_emitted_o
  // -- two quantities moving together, which is the shape of a counter that
  // cannot fail. The first run of this bench printed max_in_tick=42 against 42
  // emitted and 20 green checks; that equality was the tell.
  void start_tick() {
    idle();
    v->tick_start_i = 1;
    tick();
    v->tick_start_i = 0;
    idle();
  }

  void reset() {
    idle();
    v->rst_n = 0;
    tick(); tick();
    v->rst_n = 1;
    tick();
    idle();
  }

  // Offer one parent and run until the block is idle again.
  void feed(const zref::part::Particle128& p, uint16_t pid, uint8_t events,
            uint32_t tick_no, int cap_full_after = -1) {
    uint64_t lo, hi;
    zref::part::particle_pack(p, &lo, &hi);
    idle();
    v->par_valid_i = 1;
    v->par_id_i = pid;
    v->par_events_i = events;
    v->tick_i = tick_no;
    put128(v->par_record_i, lo, hi);
    // hold it until accepted
    for (int g = 0; g < 200; ++g) {
      serve_descriptor();
      v->eval();
      const bool fired = v->par_valid_i && v->par_ready_o;
      tick();
      if (fired) break;
    }
    v->par_valid_i = 0;
    int emitted = 0;
    for (int g = 0; g < 4000; ++g) {
      idle();
      if (cap_full_after >= 0 && emitted >= cap_full_after) v->cap_full_i = 1;
      const size_t before = out.size();
      serve_descriptor();
      v->eval();
      tick();
      emitted += static_cast<int>(out.size() - before);
      if (v->par_ready_o && !v->chl_valid_o) break;
    }
    idle();
  }
};

zref::part::Particle128 unpack(const Child& c) {
  zref::part::Particle128 p{};
  zref::part::particle_unpack(c.lo, c.hi, &p);
  return p;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  Verilated::traceEverOn(false);

  auto* top = new Vzhao_part_spawn;   // heap + exit_hard: see zhao_sim.hpp
  Vzhao_part_spawn& r = *top;
  Dut d(top);
  d.reset();

  const zref::part::Particle128 par1 = make_parent(1, 5);
  const zref::part::Particle128 par2 = make_parent(2, 9);

  // ---- 1. ORDER: one parent, two events. birth(3 of species 2) then age(2 of
  //         species 3). Event order is bit order, and it is the contract. ----
  d.out.clear();
  d.feed(par1, /*pid=*/0x1234, /*events=*/0b0011, /*tick=*/7);
  check(d.out.size() == 5, "birth+age emits 3 then 2", 5u, d.out.size());
  if (d.out.size() == 5) {
    bool order_ok = true;
    for (int i = 0; i < 3; ++i) if (unpack(d.out[i]).species != 2) order_ok = false;
    for (int i = 3; i < 5; ++i) if (unpack(d.out[i]).species != 3) order_ok = false;
    check(order_ok, "event order: all birth children precede all age children", 1, order_ok ? 1 : 0);
  }

  // ---- 6. the ratified flag bits, on every child ever emitted -------------
  {
    bool born_ok = true, reserved_ok = true, age_ok = true, pos_ok = true;
    for (const Child& c : d.out) {
      const auto p = unpack(c);
      if ((p.flags & zref::part::kPartBornThisTick) == 0) born_ok = false;
      if (p.flags & zref::part::kPartFlagReserved) reserved_ok = false;
      if (p.age != 0) age_ok = false;
      for (int k = 0; k < 3; ++k) if (p.pos[k] != par1.pos[k]) pos_ok = false;
    }
    check(born_ok, "every child carries kPartBornThisTick", 1, born_ok ? 1 : 0);
    check(reserved_ok, "kPartFlagReserved stays zero (zero in, preserved zero)", 1, reserved_ok ? 1 : 0);
    check(age_ok, "a child is age zero", 1, age_ok ? 1 : 0);
    check(pos_ok, "child position is the parent's, identity (Class-C unruled)", 1, pos_ok ? 1 : 0);
  }

  // ---- 5. variation: siblings DIFFER, and the same tick REPRODUCES --------
  {
    bool all_differ = true;
    for (size_t i = 0; i < 3; ++i)
      for (size_t j = i + 1; j < 3; ++j)
        if (unpack(d.out[i]).variation == unpack(d.out[j]).variation) all_differ = false;
    check(all_differ, "sibling variations differ", 1, all_differ ? 1 : 0);
  }
  std::vector<Child> first_run(d.out.begin(), d.out.begin() + 3);
  d.out.clear();
  d.feed(par1, 0x1234, 0b0001, 7);   // same parent id, same tick, same event
  bool repro = d.out.size() == 3;
  for (size_t i = 0; i < 3 && repro; ++i)
    repro = (d.out[i].lo == first_run[i].lo) && (d.out[i].hi == first_run[i].hi);
  check(repro, "same {parent_id, event, index, tick} reproduces the same children", 1, repro ? 1 : 0);

  // ---- 2a. 16 is ACCEPTED -------------------------------------------------
  const uint32_t emitted_before = r.children_emitted_o;
  d.start_tick();                      // a new tick: the watermark restarts here
  d.out.clear();
  d.feed(par1, 0x2222, 0b0100, 8);   // collision -> 16
  check(d.out.size() == 16, "16 children accepted", 16u, d.out.size());
  check(r.children_emitted_o - emitted_before == 16, "emitted counter agrees", 16u,
        r.children_emitted_o - emitted_before);

  // ---- 2b. 17 refuses the WHOLE group, and emits nothing ------------------
  const uint32_t refused_gt_before = r.refused_count_gt_max_o;
  d.out.clear();
  d.feed(par1, 0x3333, 0b1000, 9);   // death -> 17
  check(d.out.size() == 0, "17 emits NOTHING (not a truncated 16)", 0u, d.out.size());
  check(r.refused_count_gt_max_o - refused_gt_before == 1, "count>16 refusal counted", 1u,
        r.refused_count_gt_max_o - refused_gt_before);

  // ---- 3. unknown child species refuses the group -------------------------
  const uint32_t refused_spc_before = r.refused_unknown_species_o;
  d.out.clear();
  d.feed(par2, 0x4444, 0b0001, 10);  // child species 9, SPECIES_N = 8
  check(d.out.size() == 0, "out-of-range child species emits nothing", 0u, d.out.size());
  check(r.refused_unknown_species_o - refused_spc_before == 1,
        "unknown-species refusal counted", 1u, r.refused_unknown_species_o - refused_spc_before);

  // ---- 4. capacity: later children dropped, and the SAME ones each run ----
  // Its own tick. Without this the two 4-child capacity runs land in the SAME
  // tick as the 16-child burst and the watermark is legitimately 24 -- which is
  // the RTL being right and the first expectation here being wrong.
  d.start_tick();
  const uint32_t refused_cap_before = r.refused_capacity_o;
  d.out.clear();
  d.feed(par1, 0x5555, 0b0100, 11, /*cap_full_after=*/4);
  const size_t first_cap_run = d.out.size();
  std::vector<Child> cap_first(d.out.begin(), d.out.end());
  check(first_cap_run > 0 && first_cap_run < 16,
        "capacity drops LATER children, keeps earlier ones", 1, 1);
  check(r.refused_capacity_o - refused_cap_before == 1, "capacity refusal counted", 1u,
        r.refused_capacity_o - refused_cap_before);
  d.out.clear();
  d.feed(par1, 0x5555, 0b0100, 11, /*cap_full_after=*/4);
  bool cap_repro = d.out.size() == first_cap_run;
  for (size_t i = 0; i < d.out.size() && cap_repro; ++i)
    cap_repro = (d.out[i].lo == cap_first[i].lo) && (d.out[i].hi == cap_first[i].hi);
  check(cap_repro, "the SAME children are dropped on a repeat run", 1, cap_repro ? 1 : 0);

  // ---- 1b. TWO parents, two events each: parent order outranks event order
  d.start_tick();
  d.out.clear();
  d.feed(par1, 0x6001, 0b0011, 12);
  d.feed(par1, 0x6002, 0b0011, 12);
  check(d.out.size() == 10, "two parents x (3+2) children", 10u, d.out.size());
  if (d.out.size() == 10) {
    // the first five are parent A's, the last five parent B's, and within each
    // the species run is 2,2,2,3,3 -- parent -> event -> index, exactly.
    bool shape = true;
    for (int p = 0; p < 2; ++p) {
      for (int i = 0; i < 3; ++i) if (unpack(d.out[p * 5 + i]).species != 2) shape = false;
      for (int i = 3; i < 5; ++i) if (unpack(d.out[p * 5 + i]).species != 3) shape = false;
    }
    check(shape, "stream is parent -> event -> index with no interleaving", 1, shape ? 1 : 0);
    bool parents_differ = (d.out[0].lo != d.out[5].lo) || (d.out[0].hi != d.out[5].hi);
    check(parents_differ, "different parent_id gives different children", 1, parents_differ ? 1 : 0);
  }

  // The watermark must now be 16 -- the collision burst, the biggest thing that
  // happened inside one tick -- and NOT the 42 emitted across the whole run. If
  // those two are ever equal again, tick_start_i has stopped being honoured.
  check(r.max_children_in_tick_o == 16, "max_children_in_tick is PER TICK, not cumulative",
        16u, r.max_children_in_tick_o);
  check(r.max_children_in_tick_o != r.children_emitted_o,
        "the watermark is not a second copy of the emitted counter", 1, 1);

  std::printf("[part_spawn_directed] emitted=%u requested=%u refused=%u max_in_tick=%u\n",
              r.children_emitted_o, r.children_requested_o, r.children_refused_o,
              r.max_children_in_tick_o);
  zhao::exit_hard(zhao::report_and_exit("part_spawn_directed"));
}
