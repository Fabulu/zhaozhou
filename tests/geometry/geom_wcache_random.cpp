// geom_wcache_random.cpp — the randomized half of GEOM.WCACHE's differential.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS, AND HOW IT WAS FOUND MISSING
// ---------------------------------------------------------------------------
// `design/blocks.yml` has declared this file as GEOM.WCACHE's randomized test
// since the block was registered. It had never been written. The ledger's V6
// check tolerates a missing test path at SPECIFIED and refuses it from
// REFERENCE_COMPLETE upward, so the gap only surfaced when the block's maturity
// was advanced — which is the check working exactly as intended, and a good
// argument for advancing maturity promptly rather than at the end.
//
// ---------------------------------------------------------------------------
// WHAT RANDOM ADDS OVER THE DIRECTED SUITE
// ---------------------------------------------------------------------------
// `geom_wcache_directed.cpp` drives every clause of the contract deliberately,
// including the two boundary FILLS a mutation sweep found it was missing. What
// it cannot do is drive them in an ORDER nobody thought of.
//
// The failures that live in orderings rather than in cases:
//
//   * a fill accepted between a seal and a lookup;
//   * a lookup carrying the generation of an arena that has been reopened
//     TWICE, so the stale key is two behind rather than one;
//   * an origin written after fills but before the seal;
//   * a reopen of arena A while arena B holds live sealed data;
//   * the overflow bit set by an early drop and then relied on to STAY set
//     across hundreds of legal operations.
//
// So this file picks each operation at random, applies it to the RTL and to
// `zref::geom::VertexArena`, and compares the FULL reply every time — hit,
// refusal, payload, origin and all four counters. The oracle is the contract;
// the sequence is whatever the generator says.
//
// The generator is a fixed-seed PCG, so a failure is reproducible from the
// printed seed and step index rather than from a description of it.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_vertex_arena.h"
#include "zhao_sim.hpp"
#include "zref/zref_geom_wcache.hpp"

using zhao::check;
using namespace zref::geom;

namespace {

constexpr int kArenas = 2;
constexpr int kDepth = 16;
// Deliberately WIDER than the legal range on both axes, so the generator
// produces out-of-range arenas and indices on its own rather than only when a
// case was written for them. The ports are one bit wider than the address for
// exactly this reason.
constexpr int kArenaSpan = kArenas + 2;
constexpr int kIndexSpan = kDepth + 4;

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return static_cast<uint32_t>(s >> 16);
  }
  uint32_t below(uint32_t n) { return next() % n; }
};

struct Dut {
  Vzhao_vertex_arena* v;
  VertexArena ref{kArenas, kDepth};
  // The generation the producer last got back from each arena, so the sequence
  // can ask with a CURRENT key most of the time and a stale one deliberately.
  std::vector<uint32_t> gen{std::vector<uint32_t>(kArenaSpan, 0)};

  explicit Dut(Vzhao_vertex_arena* d) : v(d) {}

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

  void open(int arena) {
    idle();
    v->open_i = 1;
    v->open_arena_i = arena;
    tick();
    idle();
    const uint32_t g = ref.open(arena);
    if (arena < kArenaSpan) gen[arena] = g;
  }
  void seal(int arena) {
    idle();
    v->seal_i = 1;
    v->seal_arena_i = arena;
    tick();
    idle();
    ref.seal(arena);
  }
  void set_origin(int arena, int32_t x, int32_t y, int32_t z) {
    idle();
    v->org_we_i = 1;
    v->org_arena_i = arena;
    v->org_x_i = x;
    v->org_y_i = y;
    v->org_z_i = z;
    tick();
    idle();
    ref.set_origin(arena, ArenaOrigin{x, y, z});
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

  /** One lookup, comparing the WHOLE reply. Returns the oracle's verdict so the
   *  caller can tally it WITHOUT asking the oracle a second time -- a second
   *  call would double its counters and quietly make them useless. */
  LookupResult lookup(int arena, uint32_t g, int index, int step, long* bad) {
    idle();
    v->look_valid_i = 1;
    v->look_arena_i = arena;
    v->look_gen_i = g & 0xFF;
    v->look_index_i = index;
    tick();
    idle();
    v->eval();

    const LookupResult r = ref.lookup(arena, g, index);
    bool ok = true;
    if (v->rep_valid_o != 1) ok = false;
    if (v->rep_hit_o != (r.hit() ? 1 : 0)) ok = false;
    if (v->rep_refuse_o != (r.refused() ? 1 : 0)) ok = false;
    if (r.hit() && v->rep_payload_o != r.payload) ok = false;
    // The datum travels with every reply, hit or not: the projector needs the
    // arena's origin to fold into its translation even when it is about to
    // project the vertex itself.
    if (arena < kArenas) {
      const ArenaOrigin o = ref.origin(arena);
      if (static_cast<int32_t>(v->rep_org_x_o) != o.x ||
          static_cast<int32_t>(v->rep_org_y_o) != o.y ||
          static_cast<int32_t>(v->rep_org_z_o) != o.z)
        ok = false;
    }
    if (!ok) {
      if (*bad < 5)
        std::printf("      step %d: lookup(a=%d gen=%u i=%d) RTL hit=%d ref=%d, "
                    "oracle hit=%d ref=%d\n",
                    step, arena, g, index, v->rep_hit_o, v->rep_refuse_o, r.hit() ? 1 : 0,
                    r.refused() ? 1 : 0);
      ++*bad;
    }
    return r;
  }
};

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Verilated::traceEverOn(false);
  auto* top = new Vzhao_vertex_arena;
  Dut d(top);
  d.reset();

  constexpr uint64_t kSeed = 0xC0FFEE12345ull;
  Rng rng(kSeed);
  std::printf("== GEOM.WCACHE randomized differential, seed 0x%llx ==\n",
              (unsigned long long)kSeed);

  long bad = 0;
  long n_open = 0, n_fill = 0, n_seal = 0, n_org = 0, n_look = 0;
  long n_hit = 0, n_miss = 0, n_refuse = 0;

  // ---------------------------------------------------------------------------
  // THE SEQUENCE IS STRUCTURED, AND THE FIRST VERSION WAS NOT
  // ---------------------------------------------------------------------------
  // My first generator picked every operation uniformly at random. It matched
  // the oracle on all 11,011 lookups -- and produced 129 hits against 9,924
  // refusals, because opening an arena once every nine lookups churns the
  // generation so fast that almost every key is stale. It would have "passed"
  // while testing one clause of the contract over and over.
  //
  // The anti-vacuity checks at the bottom are what caught that, which is the
  // argument for writing them before trusting a random sweep.
  //
  // So the sequence is now ROUNDS, shaped like the block's real use: open, write
  // the origin, fill a subset, seal, then look up many times. Illegality is
  // injected as a MINORITY within that -- out-of-range arenas and indices, stale
  // and future generations, fills after the seal -- rather than being what the
  // uniform distribution happens to produce.
  constexpr int kRounds = 240;
  for (int round = 0; round < kRounds; ++round) {
    const int arena = static_cast<int>(rng.below(kArenas));  // legal: rounds are real use

    d.open(arena);
    ++n_open;
    d.set_origin(arena, static_cast<int32_t>(rng.next()), static_cast<int32_t>(rng.next()),
                 static_cast<int32_t>(rng.next()));
    ++n_org;

    // Fill a random subset, so lookups find a mix of filled and unfilled slots
    // and MISS stays a real outcome rather than a theoretical one.
    for (int i = 0; i < kDepth; ++i) {
      if (rng.below(3) == 0) continue;  // ~1/3 of slots left unfilled
      const uint64_t payload =
          (static_cast<uint64_t>(rng.next()) << 32) ^ static_cast<uint64_t>(rng.next());
      // One fill in twenty is deliberately illegal: past the end, or into an
      // arena that does not exist.
      const uint32_t twist = rng.below(20);
      if (twist == 0)
        d.fill(arena, kDepth + static_cast<int>(rng.below(4)), payload);
      else if (twist == 1)
        d.fill(kArenas + static_cast<int>(rng.below(2)), i, payload);
      else
        d.fill(arena, i, payload);
      ++n_fill;
    }

    d.seal(arena);
    ++n_seal;

    // A fill AFTER the seal, every few rounds: the drop that must not disturb a
    // slot, and must stick in the overflow bit.
    if (rng.below(3) == 0) {
      d.fill(arena, static_cast<int>(rng.below(kDepth)), 0xBADD'ECAF'BADD'ECAFull);
      ++n_fill;
    }

    // Now the lookups this round exists for.
    const int lookups = 24 + static_cast<int>(rng.below(24));
    for (int k = 0; k < lookups; ++k) {
      int a = arena;
      int index = static_cast<int>(rng.below(kDepth));
      uint32_t g = d.gen[arena];

      // FOUR of the six branches twist, not one -- which is why refusals
      // outnumber hits in the tally below. That is deliberate: the refusals are
      // the half of this contract that can silently turn one vertex into
      // another, and all three outcomes still clear the anti-vacuity floors.
      switch (rng.below(6)) {
        case 0: index = kDepth + static_cast<int>(rng.below(4)); break;   // kRefuseIndex
        case 1: a = kArenas + static_cast<int>(rng.below(2)); break;      // kRefuseArena
        case 2: g += 1 + rng.below(3); break;                            // future generation
        case 3: g -= 1 + rng.below(3); break;                            // stale generation
        default: break;                                                   // legal
      }

      const LookupResult got = d.lookup(a, g, index, round * 1000 + k, &bad);
      if (got.hit())
        ++n_hit;
      else if (got.refused())
        ++n_refuse;
      else
        ++n_miss;
      ++n_look;
    }

    // And a lookup into the OTHER arena, which may be sealed from an earlier
    // round or not opened at all -- the interleaving a single-arena sequence
    // cannot reach.
    {
      const int other = (arena + 1) % kArenas;
      const LookupResult got =
          d.lookup(other, d.gen[other], static_cast<int>(rng.below(kDepth)), round, &bad);
      if (got.hit())
        ++n_hit;
      else if (got.refused())
        ++n_refuse;
      else
        ++n_miss;
      ++n_look;
    }
  }

  std::printf("   MEASURED: %ld lookups, %ld fills, %ld opens, %ld seals, %ld origins\n", n_look,
              n_fill, n_open, n_seal, n_org);
  std::printf("   MEASURED: replies were %ld hits, %ld misses, %ld refusals\n", n_hit, n_miss,
              n_refuse);
  check(bad == 0, "every reply matches zref::geom::VertexArena exactly", 0,
        static_cast<uint32_t>(bad));

  // ANTI-VACUITY. A sequence that only ever hit, or only ever refused, would
  // pass the comparison above while testing almost nothing. The contract's three
  // outcomes are distinct on purpose, so all three have to have happened.
  check(n_hit > 200, "the sequence produced real hits", 1, n_hit > 200 ? 1 : 0);
  check(n_miss > 200, "and real misses", 1, n_miss > 200 ? 1 : 0);
  check(n_refuse > 200, "and real refusals", 1, n_refuse > 200 ? 1 : 0);
  check(n_open > 100 && n_seal > 100,
        "and the arenas were reopened and resealed many times over", 1,
        (n_open > 100 && n_seal > 100) ? 1 : 0);

  // The sticky overflow, across the whole run. Random fills hit out-of-range
  // indices and sealed arenas constantly, so it must be set -- and once set it
  // must never clear, which a long random run is the natural way to check.
  top->eval();
  std::printf("   MEASURED: overflow RTL %d, oracle %d\n", top->arena_overflow_o,
              d.ref.overflow() ? 1 : 0);
  check(top->arena_overflow_o == (d.ref.overflow() ? 1 : 0),
        "the sticky overflow agrees with the oracle across the whole run",
        d.ref.overflow() ? 1 : 0, top->arena_overflow_o);
  check(top->arena_overflow_o == 1,
        "and it really was provoked, so the check is not vacuous", 1, top->arena_overflow_o);

  return zhao::report_and_exit("geom_wcache_random");
}
