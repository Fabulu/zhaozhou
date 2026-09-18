// part_state_directed.cpp — PART.STATE against the four things its contract
// actually promises, and nothing else.
//
// The contract's own words decide what is worth asserting here:
//
//   1. "the packing is exact and lossless. A record read and written back
//      untouched must be bit-identical" — the one property that does NOT wait
//      on the unruled position/velocity scales, so it is the first test.
//   2. "survivors are compacted first, in stream order" — a dense stream in the
//      order it arrived, with the dead removed.
//   3. "children are appended after survivors" and "on exhaustion, survivors
//      always outrank new children".
//   4. "species index out of range: refuse the record, count it; do not
//      fabricate a species."
//
// And one thing that is NOT tested because the block does not own it: the DDR
// buffers. They are off chip, behind MEM.HPS.BRIDGE, and a test that mocked
// them would be testing the mock.
//
// ---------------------------------------------------------------------------
// THE RECORDS COME FROM THE RATIFIED CODEC, NOT FROM HAND-WRITTEN SHIFTS
// ---------------------------------------------------------------------------
// The first version of this file built its stimulus with local `rec_lo`/
// `rec_hi` helpers full of literal shifts. They agreed with the frozen layout
// on every offset and still masked `age` to SEVEN bits where amendment C2 says
// TEN — a second implementation of ratified arithmetic, drifting in the
// flattering direction, which is precisely the failure CLAUDE.md names.
//
// Every record here is now built by `zref::part::particle_pack` and read back
// by `zref::part::particle_unpack` (`reference/include/zref/zref_particle.hpp`,
// qformats §10 / amendment C2 / ruling R3). That makes the bit-identity claim
// DIFFERENTIAL against the oracle rather than against a transcription of it.
//
// The RTL's own offset table in `zhao_part_state.sv` was compared field by
// field against that oracle and agrees on all eight:
//     pos 0/54, vel 54/33, age 87/10, species 97/7,
//     size 104/6, spin 110/6, flags 116/4, variation 120/8.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE DELIBERATELY DOES NOT ASSERT
// ---------------------------------------------------------------------------
// It does not require PART.STATE to set `kPartBornThisTick` on an appended
// child. PART.SPAWN's contract owns child derivation — "this block only places
// it into the stream" — and PART.STATE's contract asks it to carry the bits
// without interpreting them. A flag set here would be a second author of the
// same field.
//
// ---------------------------------------------------------------------------
// WHY THE ORDERING CHECK IS BELIEVED
// ---------------------------------------------------------------------------
// This suite passed on its first run, which is a claim and not evidence. The
// ordering check is shown to DISCRIMINATE by
// `tests/mutants/zhao_part_state_child_order_mutant.sv` and its inverted-
// polarity driver `part_state_child_order_control.cpp`: that copy lets children
// into the write stream during the survivor pass, leaves every count balanced,
// and is caught here and only here.

#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include "Vzhao_part_state.h"
#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"

using zhao::check;

namespace {

constexpr int kCapacity = 8;    // small on purpose: exhaustion must be reachable
constexpr int kSpecies = 4;     // so an out-of-range species is reachable too

using Rec = std::pair<uint64_t, uint64_t>;

// Build a record with a distinctive value in every field, so a dropped or
// transposed field cannot alias with another particle's. Every value is inside
// its field's ratified width, and bit 3 of `flags` — `kPartFlagReserved`, "zero
// in, preserved zero" — is never set.
zref::part::Particle128 make_particle(int i, int species) {
  zref::part::Particle128 p{};
  p.pos[0] = 1000 + i * 7;
  p.pos[1] = -(500 + i * 3);
  p.pos[2] = 250 - i * 11;
  p.vel[0] = i * 3 - 10;
  p.vel[1] = 7 - i;
  p.vel[2] = i * 2;
  p.age = static_cast<uint16_t>((100 + i * 13) & 0x3FF);   // u10, not u7
  p.species = static_cast<uint8_t>(species & 0x7F);
  p.size = static_cast<uint8_t>((i * 5) & 0x3F);
  p.spin = static_cast<uint8_t>((i * 9) & 0x3F);
  p.flags = static_cast<uint8_t>(i & 0x7);                 // reserved bit stays 0
  p.variation = static_cast<uint8_t>(0xA5 ^ i);
  return p;
}

Rec make_rec(int i, int species) {
  uint64_t lo = 0, hi = 0;
  zref::part::particle_pack(make_particle(i, species), &lo, &hi);
  return Rec(lo, hi);
}

struct Dut {
  Vzhao_part_state* v;
  std::vector<Rec> written;

  explicit Dut(Vzhao_part_state* d) : v(d) {}

  template <typename W>
  void set_rec(W& dst, const Rec& r) {
    dst[0] = static_cast<uint32_t>(r.first);
    dst[1] = static_cast<uint32_t>(r.first >> 32);
    dst[2] = static_cast<uint32_t>(r.second);
    dst[3] = static_cast<uint32_t>(r.second >> 32);
  }

  void collect() {
    // The write channel is always ready in this bench; capturing on the
    // handshake is what makes the ORDER observable.
    if (v->wr_valid_o && v->wr_ready_i) {
      const uint64_t lo = static_cast<uint64_t>(v->wr_record_o[0]) |
                          (static_cast<uint64_t>(v->wr_record_o[1]) << 32);
      const uint64_t hi = static_cast<uint64_t>(v->wr_record_o[2]) |
                          (static_cast<uint64_t>(v->wr_record_o[3]) << 32);
      written.emplace_back(lo, hi);
    }
  }

  void tick() {
    v->clk = 0;
    v->eval();
    collect();
    v->clk = 1;
    v->eval();
  }

  void idle() {
    v->tick_start_i = 0;
    v->rd_valid_i = 0;
    v->rd_last_i = 0;
    v->prt_ready_i = 1;
    v->vrd_valid_i = 0;
    v->chl_valid_i = 0;
    v->wr_ready_i = 1;
    v->eval();
  }

  void reset() {
    idle();
    v->rst_n = 0;
    tick();
    tick();
    v->rst_n = 1;
    tick();
    idle();
  }
};

// One tick's stimulus, stated as data so a case is a table rather than a
// re-written driver loop.
struct TickSpec {
  std::vector<int> species;    // one entry per input record
  std::vector<bool> survive;   // the verdict PART.UPDATE would give
  int children = 0;            // offered DURING the survivor pass
  int child_base = 100;        // so children cannot alias a survivor
};

// Drive one tick to completion and return the records the block wrote, in the
// order the write channel emitted them.
std::vector<Rec> run_tick(Dut& d, const TickSpec& s) {
  Vzhao_part_state& r = *d.v;
  const int kIn = static_cast<int>(s.species.size());
  d.written.clear();

  r.tick_start_i = 1;
  d.tick();
  r.tick_start_i = 0;

  int fed = 0, verdicts = 0, children_sent = 0;

  for (int guard = 0; guard < 4000 && verdicts < kIn; ++guard) {
    d.idle();

    if (fed < kIn && r.rd_ready_o) {
      r.rd_valid_i = 1;
      r.rd_last_i = (fed == kIn - 1);
      d.set_rec(r.rd_record_i, make_rec(fed, s.species[fed]));
    }

    // The verdict answers whatever is being offered. Offers are strictly in
    // order and one at a time, so `verdicts` IS the index on the wire.
    if (r.prt_valid_o && verdicts < kIn) {
      r.vrd_valid_i = 1;
      r.vrd_survive_i = s.survive[verdicts] ? 1 : 0;
      d.set_rec(r.vrd_record_i, make_rec(verdicts, s.species[verdicts]));
    }

    // Children are offered DURING the survivor pass -- they must come out
    // after it.
    if (children_sent < s.children && r.chl_ready_o) {
      r.chl_valid_i = 1;
      d.set_rec(r.chl_record_i, make_rec(s.child_base + children_sent, 0));
    }

    r.eval();
    const bool rd_fire = r.rd_valid_i && r.rd_ready_o;
    const bool vr_fire = r.vrd_valid_i && r.vrd_ready_o;
    const bool ch_fire = r.chl_valid_i && r.chl_ready_o;
    d.tick();
    if (rd_fire) ++fed;
    if (vr_fire) ++verdicts;
    if (ch_fire) ++children_sent;
  }

  // let the append phase drain
  for (int guard = 0; guard < 400 && !r.tick_done_o; ++guard) {
    d.idle();
    d.tick();
  }
  return d.written;
}

// Compare an emitted stream against the expected one, per record, through the
// ORACLE's unpack -- so a failure names the field that moved rather than a
// 64-bit blob.
void compare_stream(const char* tag, const std::vector<Rec>& got,
                    const std::vector<Rec>& want) {
  char what[160];
  std::snprintf(what, sizeof(what), "%s: written record count", tag);
  check(got.size() == want.size(), what, want.size(), got.size());

  const size_t n = got.size() < want.size() ? got.size() : want.size();
  for (size_t i = 0; i < n; ++i) {
    std::snprintf(what, sizeof(what), "%s: record %zu low 64 bits BIT-IDENTICAL", tag, i);
    check(got[i].first == want[i].first, what, want[i].first, got[i].first);
    std::snprintf(what, sizeof(what), "%s: record %zu high 64 bits BIT-IDENTICAL", tag, i);
    check(got[i].second == want[i].second, what, want[i].second, got[i].second);
  }
}

struct Counters {
  uint32_t survivors, children_written, dropped_capacity, refused_staging, species_refused;
};

Counters snap(const Vzhao_part_state& r) {
  return Counters{r.survivors_o, r.children_written_o, r.children_dropped_capacity_o,
                  r.children_refused_staging_o, r.species_refused_o};
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Verilated::traceEverOn(false);

  auto* top = new Vzhao_part_state;   // heap + exit_hard: see zhao_sim.hpp
  Vzhao_part_state& r = *top;
  Dut d(top);
  d.reset();

  // =========================================================================
  // CASE A -- clauses 1, 2 and 3.
  // 6 particles in, #1 and #4 die, 3 children offered during the pass.
  // =========================================================================
  {
    TickSpec s;
    for (int i = 0; i < 6; ++i) {
      s.species.push_back(i % kSpecies);
      s.survive.push_back(i != 1 && i != 4);
    }
    s.children = 3;

    const Counters before = snap(r);
    const std::vector<Rec> got = run_tick(d, s);

    std::vector<Rec> want;
    for (int i = 0; i < 6; ++i)
      if (s.survive[i]) want.push_back(make_rec(i, s.species[i]));
    for (int c = 0; c < s.children; ++c) want.push_back(make_rec(s.child_base + c, 0));

    compare_stream("A dense stream, survivors then children", got, want);

    const Counters after = snap(r);
    check(after.survivors - before.survivors == 4, "A survivors counted", 4,
          after.survivors - before.survivors);
    check(after.children_written - before.children_written == 3, "A children appended", 3,
          after.children_written - before.children_written);
    check(after.species_refused - before.species_refused == 0,
          "A no species refused on a clean tick", 0,
          after.species_refused - before.species_refused);

    // The flag law of amendment C2: `kPartFlagReserved` (0x8) is zero in and
    // preserved zero. Checked through the ORACLE's unpack on every record the
    // block wrote, not on the one we happened to look at.
    int reserved_set = 0;
    for (size_t i = 0; i < got.size(); ++i) {
      zref::part::Particle128 p{};
      zref::part::particle_unpack(got[i].first, got[i].second, &p);
      if (p.flags & zref::part::kPartFlagReserved) ++reserved_set;
    }
    check(reserved_set == 0,
          "A kPartFlagReserved stays zero through the stream (amendment C2)", 0,
          reserved_set);

    // And a field-by-field round trip on one record, so a failure says WHICH
    // field moved. Record 0 survives, so it is written first.
    if (!got.empty()) {
      zref::part::Particle128 src = make_particle(0, 0);
      zref::part::Particle128 out{};
      zref::part::particle_unpack(got[0].first, got[0].second, &out);
      check(out.pos[0] == src.pos[0] && out.pos[1] == src.pos[1] && out.pos[2] == src.pos[2],
            "A position survives the round trip through the oracle", 1,
            (out.pos[0] == src.pos[0] && out.pos[1] == src.pos[1] &&
             out.pos[2] == src.pos[2]) ? 1 : 0);
      check(out.vel[0] == src.vel[0] && out.vel[1] == src.vel[1] && out.vel[2] == src.vel[2],
            "A velocity survives the round trip through the oracle", 1,
            (out.vel[0] == src.vel[0] && out.vel[1] == src.vel[1] &&
             out.vel[2] == src.vel[2]) ? 1 : 0);
      check(out.age == src.age, "A age (u10, not u7) survives the round trip", src.age, out.age);
      check(out.species == src.species, "A species survives", src.species, out.species);
      check(out.size == src.size, "A size survives", src.size, out.size);
      check(out.spin == src.spin, "A spin survives", src.spin, out.spin);
      check(out.flags == src.flags, "A flags survive", src.flags, out.flags);
      check(out.variation == src.variation, "A variation survives", src.variation, out.variation);
    }
  }

  // =========================================================================
  // CASE B -- clause 4, first half: an out-of-range species is REFUSED and
  // COUNTED. `species_refused_o` asserted zero above is a hopeful zero until
  // it is seen to move; this is where it moves.
  //
  // SPECIES_N is 4, so species 5 does not exist. The record claims to survive:
  // the refusal must outrank the verdict, or the block has fabricated a
  // species by letting one through.
  // =========================================================================
  {
    TickSpec s;
    const int spc[5] = {0, 1, 5, 2, 3};
    for (int i = 0; i < 5; ++i) {
      s.species.push_back(spc[i]);
      s.survive.push_back(true);
    }
    s.children = 0;

    const Counters before = snap(r);
    const std::vector<Rec> got = run_tick(d, s);

    std::vector<Rec> want;
    for (int i = 0; i < 5; ++i)
      if (spc[i] < kSpecies) want.push_back(make_rec(i, spc[i]));

    compare_stream("B out-of-range species dropped from the stream", got, want);

    const Counters after = snap(r);
    check(after.species_refused - before.species_refused == 1,
          "B species_refused_o MOVED -- the counter is a detector, not a hopeful zero",
          1, after.species_refused - before.species_refused);
    check(after.survivors - before.survivors == 4,
          "B the refused record was not counted as a survivor", 4,
          after.survivors - before.survivors);

    // No fabrication: nothing in the emitted stream carries species 5.
    int fabricated = 0;
    for (size_t i = 0; i < got.size(); ++i) {
      zref::part::Particle128 p{};
      zref::part::particle_unpack(got[i].first, got[i].second, &p);
      if (p.species >= kSpecies) ++fabricated;
    }
    check(fabricated == 0, "B no record with an unknown species reached the stream", 0,
          fabricated);
  }

  // =========================================================================
  // CASE C -- clause 4, second half: capacity exhaustion drops CHILDREN and
  // never survivors. `children_dropped_capacity_o` is the second counter that
  // was asserted zero and never seen to move.
  //
  // CAPACITY is 8. Eight survivors fill the tier exactly, so every one of the
  // three children offered must be dropped, and all eight survivors must still
  // be in the stream in order.
  // =========================================================================
  {
    TickSpec s;
    for (int i = 0; i < kCapacity; ++i) {
      s.species.push_back(i % kSpecies);
      s.survive.push_back(true);
    }
    s.children = 3;
    s.child_base = 200;

    const Counters before = snap(r);
    const std::vector<Rec> got = run_tick(d, s);

    std::vector<Rec> want;
    for (int i = 0; i < kCapacity; ++i) want.push_back(make_rec(i, i % kSpecies));

    compare_stream("C survivors retained whole, children dropped", got, want);

    const Counters after = snap(r);
    check(after.dropped_capacity - before.dropped_capacity == 3,
          "C children_dropped_capacity_o MOVED -- three children met a full tier",
          3, after.dropped_capacity - before.dropped_capacity);
    check(after.children_written - before.children_written == 0,
          "C not one child was written past the tier", 0,
          after.children_written - before.children_written);
    check(after.survivors - before.survivors == static_cast<uint32_t>(kCapacity),
          "C every survivor kept its place", kCapacity,
          after.survivors - before.survivors);

    // Determinism: the ruling's drop is by stream order, never by arrival, so
    // the identical tick must drop the identical children.
    const std::vector<Rec> again = run_tick(d, s);
    compare_stream("C repeat run is bit-identical", again, got);
  }

  std::printf("[part_state_directed] survivors=%u children=%u dropped=%u refused_staging=%u "
              "species_refused=%u\n",
              r.survivors_o, r.children_written_o, r.children_dropped_capacity_o,
              r.children_refused_staging_o, r.species_refused_o);
  zhao::exit_hard(zhao::report_and_exit("part_state_directed"));
}
