// geom_proj_lane_directed.cpp — GEOMETRY'S HALF OF THE SHARED PROJECTOR, proven
// to be wiring and proven to be the RIGHT wiring.
//
// `zhao_geom_proj_lane` computes nothing. That is exactly why it needs a test:
// a module whose entire content is a field order and an address is a module
// whose entire failure mode is a silently transposed field or a slot that is
// off by one, and neither shows up as a crash. It shows up as one vertex
// wearing another vertex's depth, months later, in a frame nobody can bisect.
//
// The suite fills EVERY slot of a small arena with a distinct vertex, seals,
// and reads them all back. That shape is forced by the arena's own law and is
// better for it:
//
//   * `zhao_vertex_arena` requires a DENSE seal -- "an unfilled arena can never
//     seal" -- so a test that fills one slot of 1089 and seals proves nothing,
//     which is how the first version of this file failed;
//   * filling every slot with a DIFFERENT payload means a lane that dropped the
//     rider, or transposed two fields, cannot pass by coincidence. Slot i must
//     hold vertex i and no other.
//
// Built at DEPTH=8 through VERILATOR_ARGS, exactly as `geom_wcache_directed`
// uses 16: the walk-clear cost is not what this suite is testing.

#include <cstdint>
#include <cstdio>

#include "Vzhao_geom_proj_lane.h"
#include "zhao_sim.hpp"

using zhao::check;

namespace {

constexpr int kArenas = 2;
constexpr int kDepth = 8;

// The field order, stated once so the test does not silently agree with a
// transposition in the RTL: {behind, w[30:0], d[31:0], y[20:0], x[20:0]}.
constexpr int kOffX = 0;
constexpr int kOffY = 21;
constexpr int kOffD = 42;
constexpr int kOffW = 74;
constexpr int kOffBehind = 105;

struct Vertex {
  uint32_t x;       // 21 bits
  uint32_t y;       // 21 bits
  uint32_t d;       // 32 bits
  uint32_t w;       // 31 bits
  uint32_t behind;  // 1 bit
};

// Verilator hands a 106-bit signal back as an array of 32-bit words. Pulling a
// field out by hand is where an off-by-a-word hides, so it is done ONCE here.
uint32_t field(const uint32_t* words, int lsb, int width) {
  uint64_t acc = 0;
  for (int i = 0; i < 3; ++i) {
    acc |= static_cast<uint64_t>(words[(lsb / 32) + i]) << (32 * i);
  }
  acc >>= (lsb % 32);
  const uint64_t mask = (width >= 64) ? ~0ull : ((1ull << width) - 1);
  return static_cast<uint32_t>(acc & mask);
}

// Distinct, and distinct in EVERY field, so a transposition cannot alias.
Vertex make(int i) {
  return Vertex{static_cast<uint32_t>(0x00001u + i * 0x10101u) & 0x1FFFFFu,
                static_cast<uint32_t>(0x1F0002u - i * 0x02020u) & 0x1FFFFFu,
                static_cast<uint32_t>(0xBADF00D0u + i * 0x01000001u),
                static_cast<uint32_t>(0x2A2A2A2Au ^ (i * 0x11111111u)) & 0x7FFFFFFFu,
                static_cast<uint32_t>(i & 1)};
}

struct Dut {
  Vzhao_geom_proj_lane* v;

  explicit Dut(Vzhao_geom_proj_lane* d) : v(d) {}

  void tick() {
    v->clk = 0;
    v->eval();
    v->clk = 1;
    v->eval();
  }

  void idle() {
    v->a_valid_i = 0;
    v->open_i = 0;
    v->org_we_i = 0;
    v->seal_i = 0;
    v->look_valid_i = 0;
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

  uint32_t open(int arena) {
    idle();
    v->open_i = 1;
    v->open_arena_i = arena;
    v->eval();
    // SAMPLED BEFORE THE EDGE, and that is not a style choice. The arena drives
    // `assign open_gen_o = gen_q[arena] + 1` -- the generation this open is
    // about to install, combinationally. Reading it after the tick returns
    // gen+1 again, and every subsequent lookup misses with a perfect payload,
    // which is precisely how the first version of this test failed: 50 of 58
    // checks green and every `hit` low.
    const uint32_t gen = v->open_gen_o;
    tick();
    idle();
    return gen;
  }

  // Build the rider the way a PRODUCER would: hand the lane {arena, index} and
  // take back the payload it says to attach. The test never packs it itself --
  // doing so would assert its own arithmetic rather than the RTL's.
  uint32_t rider(int arena, int index) {
    v->rider_arena_i = arena;
    v->rider_index_i = index;
    v->eval();
    return v->rider_payload_o;
  }

  // Present one client-A result, exactly as `zhao_project_service` would.
  void result(const Vertex& vx, uint32_t payload) {
    idle();
    v->a_valid_i = 1;
    v->a_x_i = vx.x;
    v->a_y_i = vx.y;
    v->a_d_i = vx.d;
    v->a_w_i = vx.w;
    v->a_behind_i = vx.behind;
    v->a_payload_i = payload;
    tick();
    idle();
  }

  void seal(int arena) {
    idle();
    v->seal_i = 1;
    v->seal_arena_i = arena;
    tick();
    idle();
  }

  void lookup(int arena, uint32_t gen, int index) {
    idle();
    v->look_valid_i = 1;
    v->look_arena_i = arena;
    v->look_gen_i = gen & 0xFF;
    v->look_index_i = index;
    tick();
    idle();
    v->eval();
  }
};

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Verilated::traceEverOn(false);

  // HEAP, NEVER DELETED, AND EXIT THROUGH report_and_exit -- the workaround
  // zhao_sim.hpp documents: Verilator 5.051 with winlibs libwinpthread
  // intermittently deadlocks in VlThreadPool's destructor during exit-time
  // static destruction, at ~0 CPU, with the verdict still in an unflushed pipe
  // buffer. The first version of this file used a stack object and hung with no
  // output at all, which reads exactly like a test that never ran.
  auto* top = new Vzhao_geom_proj_lane;
  Vzhao_geom_proj_lane& r = *top;
  Dut d(top);
  d.reset();

  // ---- fill EVERY slot of arena 1, then seal -------------------------------
  const uint32_t gen1 = d.open(1);
  for (int i = 0; i < kDepth; ++i) {
    d.result(make(i), d.rider(1, i));
  }
  d.seal(1);

  // ---- every slot holds ITS OWN vertex, field for field --------------------
  for (int i = 0; i < kDepth; ++i) {
    const Vertex want = make(i);
    d.lookup(1, gen1, i);

    char what[96];
    std::snprintf(what, sizeof(what), "slot %d hit", i);
    check(r.rep_valid_o == 1, what, 1, r.rep_valid_o);
    check(r.rep_hit_o == 1, what, 1, r.rep_hit_o);

    const uint32_t* w = &r.rep_payload_o[0];
    std::snprintf(what, sizeof(what), "slot %d field x", i);
    check(field(w, kOffX, 21) == want.x, what, want.x, field(w, kOffX, 21));
    std::snprintf(what, sizeof(what), "slot %d field y", i);
    check(field(w, kOffY, 21) == want.y, what, want.y, field(w, kOffY, 21));
    std::snprintf(what, sizeof(what), "slot %d field d", i);
    check(field(w, kOffD, 32) == want.d, what, want.d, field(w, kOffD, 32));
    std::snprintf(what, sizeof(what), "slot %d field w", i);
    check(field(w, kOffW, 31) == want.w, what, want.w, field(w, kOffW, 31));
    std::snprintf(what, sizeof(what), "slot %d field behind", i);
    check(field(w, kOffBehind, 1) == want.behind, what, want.behind, field(w, kOffBehind, 1));
  }

  // ---- THE CONTROL: the arena half of the rider must matter ----------------
  //
  // Arena 0 was never opened or filled. If a lane dropped the arena bits, its
  // lookups would land in arena 1's storage and hit. They must not.
  d.lookup(0, gen1, 0);
  check(r.rep_valid_o == 1, "arena control: reply valid", 1, r.rep_valid_o);
  check(r.rep_hit_o == 0,
        "arena control: an arena that was never opened must NOT hit -- if it "
        "does, the arena field of the rider is being dropped and every arena "
        "shares one store",
        0, r.rep_hit_o);

  zhao::exit_hard(zhao::report_and_exit("geom_proj_lane_directed"));
}
