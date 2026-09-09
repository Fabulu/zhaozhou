// vertex_arena_dense_directed.cpp — VALID_MODE=1 (DENSE_SEAL) differential
// against zref::geom::VertexArena in dense mode.
//
// The dense mode's contract, and therefore this suite's shape:
//   * an accepted fill's index IS the arena's fill count (in order, once);
//   * a seal below a full count is REFUSED, sticky on arena_seal_short_o;
//   * a sealed arena is a COMPLETE arena, so a legal lookup NEVER misses --
//     the miss counter must sit at zero for this whole file, and its
//     positive control is the committed seal-guard mutant
//     (tests/mutants/zhao_vertex_arena_dense_mutant.sv, driven with inverted
//     polarity by vertex_arena_dense_seal_control.cpp).
//
// The shape is TERRAIN'S OWN: ARENAS=4, DEPTH=81 (9x9 subpatch) -- and 81 is
// deliberately NOT a power of two, so this suite also exercises the linear
// arena*DEPTH+index addressing repair of 2026-09-09 at a depth where the old
// {arena, index} concat addressing corrupted arena >= 1 (see the wr_addr
// comment in the RTL). Payload stays 64 wide for uint64_t convenience, as in
// geom_wcache_directed.
//
// The DUT class is macro-overridable for exactly one reason: compiling THIS
// UNCHANGED FILE against the committed mutant must FAIL (the checker seen to
// fail on a deliberate break). It is not a hook for other arenas.

#include <cstdint>
#include <cstdio>

#ifndef ZHAO_ARENA_DUT_HEADER
#define ZHAO_ARENA_DUT_HEADER "Vzhao_vertex_arena.h"
#define ZHAO_ARENA_DUT_CLASS Vzhao_vertex_arena
#endif
#include ZHAO_ARENA_DUT_HEADER

#include "zhao_sim.hpp"
#include "zref/zref_geom_wcache.hpp"

using zhao::check;
using namespace zref::geom;

namespace {

constexpr int kArenas = 4;  // terrain: 2 views x 2 working generations
constexpr int kDepth = 81;  // 9x9 subpatch lattice, NOT a power of two

// A recognisable per-slot payload so a wrong row is loud.
uint64_t pay(int arena, int index, uint32_t gen) {
  return (uint64_t(0xA0 | arena) << 56) | (uint64_t(gen & 0xFF) << 48) |
         (uint64_t(index) << 8) | 0x5Aull;
}

struct Dut {
  ZHAO_ARENA_DUT_CLASS* v;
  VertexArena ref{kArenas, kDepth, /*dense=*/true};

  explicit Dut(ZHAO_ARENA_DUT_CLASS* d) : v(d) {}

  void tick() {
    v->clk = 0;
    v->eval();
    v->clk = 1;
    v->eval();
  }

  void idle() {
    v->open_i = 0;
    v->org_we_i = 0;
    v->fill_valid_i = 0;
    v->seal_i = 0;
    v->look_valid_i = 0;
  }

  void reset() {
    idle();
    v->rst_n = 0;
    tick();
    tick();
    v->rst_n = 1;
    tick();
  }

  uint32_t open(int arena) {
    idle();
    v->open_i = 1;
    v->open_arena_i = arena;
    tick();
    idle();
    return ref.open(arena);
  }

  bool seal(int arena) {
    idle();
    v->seal_i = 1;
    v->seal_arena_i = arena;
    tick();
    idle();
    return ref.seal(arena);
  }

  void fill(int arena, int index, uint64_t payload) {
    idle();
    v->fill_valid_i = 1;
    v->fill_arena_i = arena;
    v->fill_index_i = index;
    v->fill_payload_i = payload;
    tick();
    idle();
    ref.fill(arena, index, payload);
  }

  // Drive one lookup and compare the whole reply against the oracle.
  void expect_lookup(int arena, uint32_t gen, int index, const char* what) {
    idle();
    v->look_valid_i = 1;
    v->look_arena_i = arena;
    v->look_gen_i = gen & 0xFF;
    v->look_index_i = index;
    tick();  // accepted; reply is registered and appears now
    idle();
    v->eval();

    const LookupResult r = ref.lookup(arena, gen, index);

    check(v->rep_valid_o == 1, what, 1, v->rep_valid_o);
    check(v->rep_hit_o == (r.hit() ? 1 : 0), what, r.hit() ? 1 : 0, v->rep_hit_o);
    check(v->rep_refuse_o == (r.refused() ? 1 : 0), what, r.refused() ? 1 : 0,
          v->rep_refuse_o);
    if (r.hit()) {
      check(v->rep_payload_o == r.payload, what, r.payload, v->rep_payload_o);
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Verilated::traceEverOn(false);
  auto* top = new ZHAO_ARENA_DUT_CLASS;
  Dut d(top);
  d.reset();

  // ---- 0. both sticky fault bits start clear ------------------------------
  top->eval();
  check(top->arena_overflow_o == 0, "overflow clear at reset", 0, top->arena_overflow_o);
  check(top->arena_seal_short_o == 0, "seal_short clear at reset", 0,
        top->arena_seal_short_o);

  // ---- 1. lookups before any open refuse ----------------------------------
  d.expect_lookup(0, 0, 0, "lookup before open refuses");

  // ---- 2. THE MECHANISM: in-order fill, short seal refused, full seal hits -
  const uint32_t g0 = d.open(0);
  for (int i = 0; i < 10; ++i) d.fill(0, i, pay(0, i, g0));
  const bool short_sealed = d.seal(0);  // count = 10 of 81
  check(!short_sealed, "oracle refuses the short seal", 0, short_sealed ? 1 : 0);
  top->eval();
  check(top->arena_seal_short_o == 1, "SEEN TO FIRE: seal_short sticky after short seal",
        1, top->arena_seal_short_o);
  d.expect_lookup(0, g0, 5, "after refused seal the arena still refuses lookups");
  for (int i = 10; i < kDepth; ++i) d.fill(0, i, pay(0, i, g0));
  const bool sealed = d.seal(0);
  check(sealed, "full seal accepted", 1, sealed ? 1 : 0);
  // Every row hits with its exact payload -- rows past the old power-of-two
  // stride boundary included, which is the addressing repair speaking.
  for (int i = 0; i < kDepth; ++i)
    d.expect_lookup(0, g0, i, "sealed dense arena hits every row exactly");

  // ---- 3. refusals: index, arena, generation -------------------------------
  d.expect_lookup(0, g0, kDepth, "index == DEPTH refuses");
  d.expect_lookup(0, g0, kDepth + 7, "index past DEPTH refuses");
  d.expect_lookup(kArenas, g0, 0, "arena == ARENAS refuses");
  d.expect_lookup(0, g0 + 1, 3, "future generation refuses");
  d.expect_lookup(0, g0 - 1, 3, "past generation refuses");

  // ---- 4. reopening must not resurrect, and partial reopen stays closed ----
  const uint32_t g1 = d.open(0);
  d.expect_lookup(0, g0, 3, "old generation refuses after reopen");
  for (int i = 0; i < 3; ++i) d.fill(0, i, pay(0, i, g1));
  d.expect_lookup(0, g1, 0,
                  "a partially refilled arena refuses -- dense never shows a partial lifetime");
  for (int i = 3; i < kDepth; ++i) d.fill(0, i, pay(0, i, g1));
  d.seal(0);
  d.expect_lookup(0, g1, 3, "resealed arena hits the NEW payload, not the old");
  d.expect_lookup(0, g1, 80, "last row of the new lifetime hits");

  // ---- 5. misordered fills are DROPPED, sticky, and advance nothing --------
  const uint32_t ga = d.open(1);
  d.fill(1, 5, pay(1, 5, ga));  // count is 0: out of order, dropped
  top->eval();
  check(top->arena_overflow_o == 1, "SEEN TO FIRE: overflow sticky after misordered fill",
        1, top->arena_overflow_o);
  d.fill(1, 0, pay(1, 0, ga));          // in order: accepted
  d.fill(1, 0, pay(1, 0, ga) ^ 0xFFu);  // repeat of a written row: dropped
  for (int i = 1; i < kDepth; ++i) d.fill(1, i, pay(1, i, ga));
  d.seal(1);
  d.expect_lookup(1, ga, 0, "row 0 holds the FIRST accepted fill, not the dropped repeat");
  d.expect_lookup(1, ga, 5, "row 5 holds its in-order fill, not the early misorder");
  d.expect_lookup(1, ga, kDepth - 1, "the drops advanced nothing: the arena still completed");

  // ---- 6. the open races, resolved the way the PROOF demanded --------------
  // A seal racing an open of the same arena LOSES (the first prove_dense run
  // exhibited sealed-but-empty when it won). The oracle is transactional, so
  // the race is modelled as open-then-refused-seal -- same observable.
  {
    const uint32_t gb = d.open(2);
    for (int i = 0; i < kDepth; ++i) d.fill(2, i, pay(2, i, gb));
    // Same cycle: open arena 2 AND seal arena 2, with the count full.
    d.idle();
    top->open_i = 1;
    top->open_arena_i = 2;
    top->seal_i = 1;
    top->seal_arena_i = 2;
    d.tick();
    d.idle();
    const uint32_t gc = d.ref.open(2);
    const bool raced = d.ref.seal(2);
    check(!raced, "oracle: the racing seal is refused", 0, raced ? 1 : 0);
    d.expect_lookup(2, gc, 0, "after the open+seal race the arena is OPEN and refuses");
    d.expect_lookup(2, gb, 0, "and the pre-race generation is stale");
    // A fill racing an open is refused too. Modelled transactionally as
    // open-then-fill(1): out of order on both sides. (Index 1, not 0, so the
    // transactional oracle and the cycle-true RTL agree -- a race with index
    // 0 has no transactional transcript, and the RTL refuses it regardless.)
    d.idle();
    top->open_i = 1;
    top->open_arena_i = 2;
    top->fill_valid_i = 1;
    top->fill_arena_i = 2;
    top->fill_index_i = 1;
    top->fill_payload_i = 0xBAD0BAD0BAD0BAD0ull;
    d.tick();
    d.idle();
    const uint32_t gd = d.ref.open(2);
    d.ref.fill(2, 1, 0xBAD0BAD0BAD0BAD0ull);  // kDropOrder on the oracle
    for (int i = 0; i < kDepth; ++i) d.fill(2, i, pay(2, i, gd));
    d.seal(2);
    d.expect_lookup(2, gd, 1, "the racing fill left no trace: row 1 is the in-order fill");
  }

  // ---- 7. THE DENSE LAW: zero misses, and every counter matches the oracle -
  top->eval();
  check(top->arena_misses_o == 0, "a dense arena NEVER misses (the mutant fires this)",
        0, top->arena_misses_o);
  check(top->arena_hits_o == d.ref.hits(), "hit count matches oracle", d.ref.hits(),
        top->arena_hits_o);
  check(top->arena_misses_o == d.ref.misses(), "miss count matches oracle", d.ref.misses(),
        top->arena_misses_o);
  check(top->arena_refusals_o == d.ref.refusals(), "refusal count matches oracle",
        d.ref.refusals(), top->arena_refusals_o);
  check(top->arena_overflow_o == (d.ref.overflow() ? 1 : 0), "overflow matches oracle",
        d.ref.overflow() ? 1 : 0, top->arena_overflow_o);
  check(top->arena_seal_short_o == (d.ref.seal_short() ? 1 : 0),
        "seal_short matches oracle", d.ref.seal_short() ? 1 : 0, top->arena_seal_short_o);

  delete top;
  return zhao::report_and_exit("vertex_arena_dense_directed");
}
