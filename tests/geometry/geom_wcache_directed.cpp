// geom_wcache_directed.cpp — GEOM.WCACHE differential against zref::geom::VertexArena.
//
// The refusal semantics are the point of this suite. A hit is trivial; a WRONG
// refusal is how a vertex silently becomes another vertex, and the contract's
// first formal property is exactly that a lookup must never wrap into another
// vertex.
//
// So every case here drives the RTL and the oracle with the same command and
// compares the FULL reply — hit, refuse and payload — rather than only the
// payload on the happy path.

#include <cstdint>
#include <cstdio>

#include "Vzhao_vertex_arena.h"
#include "zhao_sim.hpp"
#include "zref/zref_geom_wcache.hpp"

using zhao::check;
using namespace zref::geom;

namespace {

constexpr int kArenas = 2;
constexpr int kDepth = 16;  // small on purpose: the walk-clear cost is not what
                            // this suite is testing, and 16 keeps traces legible

struct Dut {
  Vzhao_vertex_arena* v;
  VertexArena ref{kArenas, kDepth};

  explicit Dut(Vzhao_vertex_arena* d) : v(d) {}

  void tick() {
    v->clk = 0; v->eval();
    v->clk = 1; v->eval();
  }

  void idle() {
    v->open_i = 0; v->org_we_i = 0; v->fill_valid_i = 0;
    v->seal_i = 0; v->look_valid_i = 0;
  }

  void reset() {
    idle();
    v->rst_n = 0;
    tick(); tick();
    v->rst_n = 1;
    tick();
  }

  uint32_t open(int arena) {
    idle();
    v->open_i = 1; v->open_arena_i = arena;
    tick();
    idle();
    return ref.open(arena);
  }

  void seal(int arena) {
    idle();
    v->seal_i = 1; v->seal_arena_i = arena;
    tick();
    idle();
    ref.seal(arena);
  }

  void set_origin(int arena, int32_t x, int32_t y, int32_t z) {
    idle();
    v->org_we_i = 1; v->org_arena_i = arena;
    v->org_x_i = x; v->org_y_i = y; v->org_z_i = z;
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

  // Drive one lookup and compare the whole reply against the oracle.
  void expect_lookup(int arena, uint32_t gen, int index, const char* what) {
    idle();
    v->look_valid_i = 1;
    v->look_arena_i = arena;
    v->look_gen_i = gen & 0xFF;
    v->look_index_i = index;
    tick();          // accepted; reply is registered and appears now
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
    // The datum travels with the reply whether or not the slot hit: the
    // projector needs the arena's origin to fold into its translation even when
    // it is about to project the vertex itself.
    const ArenaOrigin o = ref.origin(arena < kArenas ? arena : 0);
    if (arena < kArenas) {
      check(static_cast<int32_t>(v->rep_org_x_o) == o.x, what, static_cast<uint64_t>(o.x),
            static_cast<uint64_t>(static_cast<int32_t>(v->rep_org_x_o)));
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  Verilated::traceEverOn(false);
  auto* top = new Vzhao_vertex_arena;
  Dut d(top);
  d.reset();

  // ---- 1. a lookup before ANY open refuses (unsealed), and reads nothing ---
  d.expect_lookup(0, 0, 0, "lookup before open refuses");

  // ---- 2. fill / seal / hit, payload bit-identical -------------------------
  const uint32_t g0 = d.open(0);
  d.set_origin(0, 0x0012'3456, -0x0000'8000, 0x7FFF'0000);
  d.fill(0, 3, 0xDEAD'BEEF'0BAD'F00Dull);
  d.expect_lookup(0, g0, 3, "lookup before seal refuses");  // still unsealed
  d.seal(0);
  d.expect_lookup(0, g0, 3, "filled slot hits with exact payload");

  // ---- 3. a slot never filled MISSES, and a miss is not a refusal ----------
  d.expect_lookup(0, g0, 4, "unfilled slot misses");

  // ---- 4. out-of-range index refuses --------------------------------------
  d.expect_lookup(0, g0, kDepth - 1, "last legal index is legal");
  // Reachable only because the index port is one bit wider than the address.
  // With the port sized at exactly $clog2(DEPTH) this case cannot be expressed,
  // and the refusal would exist in the contract, the oracle and this file --
  // and nowhere in the silicon. Verilator said so: "Comparison is constant due
  // to unsigned arithmetic".
  d.expect_lookup(0, g0, kDepth, "index == DEPTH refuses");
  d.expect_lookup(0, g0, kDepth + 7, "index past DEPTH refuses");
  d.expect_lookup(kArenas, g0, 0, "arena == ARENAS refuses");

  // ---- 5. stale generation refuses ----------------------------------------
  d.expect_lookup(0, g0 + 1, 3, "future generation refuses");
  d.expect_lookup(0, g0 - 1, 3, "past generation refuses");

  // ---- 6. THE ONE THAT MATTERS: reopening must not resurrect --------------
  // Same arena, same index, same payload physically still in the memory. The
  // valid bit is gone and the generation moved, so the old key must refuse and
  // the new key must MISS -- not hit, and not return the stale payload.
  const uint32_t g1 = d.open(0);
  d.seal(0);
  d.expect_lookup(0, g0, 3, "old generation refuses after reopen");
  d.expect_lookup(0, g1, 3, "reopened slot misses, does not resurrect");

  // ---- 7. arenas are independent ------------------------------------------
  const uint32_t ga = d.open(1);
  d.set_origin(1, 0x0100'0000, 0x0200'0000, 0x0300'0000);
  d.fill(1, 3, 0x1111'2222'3333'4444ull);
  d.seal(1);
  d.expect_lookup(1, ga, 3, "arena 1 hit does not disturb arena 0");
  d.expect_lookup(0, g1, 3, "arena 0 still misses after arena 1 filled");

  // ---- 8. fill after seal is dropped and sticky ---------------------------
  d.fill(1, 5, 0xFFFF'FFFF'FFFF'FFFFull);  // arena 1 is sealed
  d.expect_lookup(1, ga, 5, "fill after seal was dropped, slot still misses");
  top->eval();
  check(top->arena_overflow_o == 1, "overflow is sticky after a dropped fill", 1,
        top->arena_overflow_o);

  // ---- 9. counters agree with the oracle ----------------------------------
  top->eval();
  check(top->arena_hits_o == d.ref.hits(), "hit count matches oracle",
        d.ref.hits(), top->arena_hits_o);
  check(top->arena_misses_o == d.ref.misses(), "miss count matches oracle",
        d.ref.misses(), top->arena_misses_o);
  check(top->arena_refusals_o == d.ref.refusals(), "refusal count matches oracle",
        d.ref.refusals(), top->arena_refusals_o);

  top->final();
  delete top;
  return zhao::report_and_exit("geom_wcache_directed");
}
