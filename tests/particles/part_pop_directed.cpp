// part_pop_directed.cpp -- PART.POP against `zref::population`, the bridge
// from SetPopulation 0x0303 to the particle engine (owner ruling R41).
//
// The RTL and the reference are differenced field for field after every
// record, which is the only check worth having here: this block's whole job is
// to carry a ratified record into the engine's formats WITHOUT changing it,
// and to refuse rather than narrow the two fields that do not fit.
//
// WHY THE PARAMETERS ARE SMALL. CAPACITY = 8, for the reason PART.STATE's own
// bench runs at 8: at the production tier of 32,768, `active_count` is a u32
// on the wire and a 16-bit count in the store, so an out-of-range count is
// reachable only with a 4-billion-record population. Shrinking the store is
// the only way to reach the refusal with a plausible number.
//
// THE FOUR THINGS THAT DISCRIMINATE, named up front:
//
//   1. A PLANE COMPONENT OUTSIDE SIGNED 12 BITS IS REFUSED, NOT TRUNCATED.
//      0x0800 truncated to twelve bits is -2048: a +Y floor becomes a -Y
//      ceiling and every particle falls through the ground it was resting on.
//      The bench offers exactly 0x0800 and requires the bank not to move.
//   2. AN `active_count` ABOVE CAPACITY IS REFUSED, NOT WRAPPED. 9 with
//      CAPACITY = 8 wraps to 1 in the store's own counter shape only at larger
//      widths; what matters is that the store is never seeded with a length it
//      did not ask for, so the refusal is asserted against the BANK's state.
//   3. A REFUSED RECORD IS REFUSED WHOLE. The refusing records below carry a
//      DIFFERENT origin from the one already held, and the held origin must
//      not move -- a half-applied descriptor is one nobody wrote.
//   4. A RESERVED FLAG BIT IS REFUSED. R41 defines b0 and b1; b2..b15 are
//      reserved zero, and a record setting one is a record from a future this
//      block does not implement.
//
// The SEED is the other half: `active_count` with flag b0 owes the store a
// seed on buffer 0, and the bank holds it until the store takes it. A newer
// seed replaces one not yet taken -- the same "latest committed state is the
// state" rule SetEnvironment lands under -- and that is checked too.
#include <cstdint>
#include <cstdio>

#include "Vzhao_part_pop.h"
#include "zhao_sim.hpp"
#include "zref/zref_population.hpp"

using zhao::check;

namespace {

constexpr uint32_t kCapacity = 8;

struct Bench {
  Vzhao_part_pop& d;
  zref::population::Bank model;

  explicit Bench(Vzhao_part_pop& dut) : d(dut) {}

  void idle() {
    d.rec_valid_i = 0;
    d.seed_ready_i = 0;
  }

  /** Offer one record for exactly one cycle, and advance the model with it. */
  void offer(const zref::population::Record& r) {
    d.rec_valid_i = 1;
    d.rec_population_i = r.population;
    d.rec_origin_x_i = static_cast<uint32_t>(r.origin_x);
    d.rec_origin_y_i = static_cast<uint32_t>(r.origin_y);
    d.rec_origin_z_i = static_cast<uint32_t>(r.origin_z);
    d.rec_active_count_i = r.active_count;
    d.rec_plane_c_i = static_cast<uint32_t>(r.plane_c);
    d.rec_plane_nx_i = static_cast<uint16_t>(r.plane_nx);
    d.rec_plane_ny_i = static_cast<uint16_t>(r.plane_ny);
    d.rec_plane_nz_i = static_cast<uint16_t>(r.plane_nz);
    d.rec_flags_i = r.flags;
    d.eval();
    check(d.rec_ready_o == 1, "a record is taken the cycle it is offered", 1, d.rec_ready_o);
    zhao::tick(d);
    zref::population::apply(model, r, kCapacity);
    idle();
    d.eval();
  }

  /** Sign-extend the DUT's NRM_W-wide normal ports (12 bits) for comparison. */
  static int32_t sx12(uint32_t v) {
    const uint32_t m = v & 0xFFFu;
    return static_cast<int32_t>(m & 0x800u ? (m | 0xFFFFF000u) : m);
  }

  void expect(const char* where) {
    char nm[160];
    std::snprintf(nm, sizeof nm, "%s: origin_x", where);
    check(static_cast<int32_t>(d.origin_x_o) == model.origin[0], nm,
          static_cast<uint32_t>(model.origin[0]), d.origin_x_o);
    std::snprintf(nm, sizeof nm, "%s: origin_y", where);
    check(static_cast<int32_t>(d.origin_y_o) == model.origin[1], nm,
          static_cast<uint32_t>(model.origin[1]), d.origin_y_o);
    std::snprintf(nm, sizeof nm, "%s: origin_z", where);
    check(static_cast<int32_t>(d.origin_z_o) == model.origin[2], nm,
          static_cast<uint32_t>(model.origin[2]), d.origin_z_o);
    std::snprintf(nm, sizeof nm, "%s: plane_en", where);
    check(d.plane_en_o == (model.plane_en ? 1 : 0), nm, model.plane_en ? 1 : 0, d.plane_en_o);
    std::snprintf(nm, sizeof nm, "%s: plane_nx", where);
    check(sx12(d.plane_nx_o) == model.plane_n[0], nm,
          static_cast<uint32_t>(model.plane_n[0]), static_cast<uint32_t>(sx12(d.plane_nx_o)));
    std::snprintf(nm, sizeof nm, "%s: plane_ny", where);
    check(sx12(d.plane_ny_o) == model.plane_n[1], nm,
          static_cast<uint32_t>(model.plane_n[1]), static_cast<uint32_t>(sx12(d.plane_ny_o)));
    std::snprintf(nm, sizeof nm, "%s: plane_nz", where);
    check(sx12(d.plane_nz_o) == model.plane_n[2], nm,
          static_cast<uint32_t>(model.plane_n[2]), static_cast<uint32_t>(sx12(d.plane_nz_o)));
    std::snprintf(nm, sizeof nm, "%s: plane_c", where);
    check(static_cast<int32_t>(d.plane_c_o) == model.plane_c, nm,
          static_cast<uint32_t>(model.plane_c), d.plane_c_o);
    std::snprintf(nm, sizeof nm, "%s: population", where);
    check(d.population_o == model.population, nm, model.population, d.population_o);
    std::snprintf(nm, sizeof nm, "%s: taken", where);
    check(d.taken_o == model.taken, nm, model.taken, d.taken_o);
    std::snprintf(nm, sizeof nm, "%s: refused_normal", where);
    check(d.refused_normal_o == model.refused_normal, nm, model.refused_normal,
          d.refused_normal_o);
    std::snprintf(nm, sizeof nm, "%s: refused_count", where);
    check(d.refused_count_o == model.refused_count, nm, model.refused_count, d.refused_count_o);
    std::snprintf(nm, sizeof nm, "%s: refused_flags", where);
    check(d.refused_flags_o == model.refused_flags, nm, model.refused_flags, d.refused_flags_o);
    std::snprintf(nm, sizeof nm, "%s: seed offered", where);
    check(d.seed_valid_o == (model.seed_pending ? 1 : 0), nm, model.seed_pending ? 1 : 0,
          d.seed_valid_o);
    if (model.seed_pending) {
      std::snprintf(nm, sizeof nm, "%s: seed count", where);
      check(d.seed_count_o == model.seed_count, nm, model.seed_count, d.seed_count_o);
      std::snprintf(nm, sizeof nm, "%s: seed buffer is 0 by ratification", where);
      check(d.seed_buf_o == 0, nm, 0, d.seed_buf_o);
    }
  }

  /** The store takes the offered seed. */
  void take_seed() {
    d.seed_ready_i = 1;
    d.eval();
    zhao::tick(d);
    model.seed_pending = false;
    idle();
    d.eval();
  }
};

zref::population::Record good(uint32_t handle, int32_t ox, int32_t oy, int32_t oz) {
  zref::population::Record r;
  r.population = handle;
  r.origin_x = ox;
  r.origin_y = oy;
  r.origin_z = oz;
  r.active_count = 4;
  r.plane_c = 100 << 10;
  r.plane_nx = 0;
  r.plane_ny = 1024;  // NRM_Q = 10: a unit +Y normal
  r.plane_nz = 0;
  r.flags = zref::population::kFlagPlaneEnable;
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vzhao_part_pop;  // heap + exit_hard: see zhao_sim.hpp
  Bench b(*top);
  b.idle();
  top->rst_n = 0;
  top->eval();
  for (int i = 0; i < 3; ++i) zhao::tick(*top);
  top->rst_n = 1;
  top->eval();
  b.expect("after reset");

  // ---- 1. a descriptor with no seed ---------------------------------------
  b.offer(good(0x0051'C0DEu, -12345, 6789, -1));
  b.expect("one descriptor");
  check(top->seed_valid_o == 0, "a record without flag b0 owes no seed", 0, top->seed_valid_o);

  // ---- 2. a descriptor that DOES seed, and the store takes it -------------
  {
    auto r = good(0x0000'0002u, 256, 0, -256);
    r.active_count = 6;
    r.flags = zref::population::kFlagSeed | zref::population::kFlagPlaneEnable;
    b.offer(r);
    b.expect("a seeding descriptor");
    check(top->seed_valid_o == 1, "the seed is offered to the store", 1, top->seed_valid_o);
    check(top->seed_count_o == 6, "with active_count as its length", 6, top->seed_count_o);
    b.take_seed();
    b.expect("after the store took the seed");
    check(top->seeds_issued_o == 1, "and the hand-off is counted", 1, top->seeds_issued_o);
  }

  // ---- 3. a NEWER seed replaces one not yet taken --------------------------
  {
    auto r1 = good(0x0000'0003u, 1, 2, 3);
    r1.active_count = 2;
    r1.flags = zref::population::kFlagSeed;
    b.offer(r1);
    check(top->seed_count_o == 2, "the first seed is pending", 2, top->seed_count_o);
    auto r2 = good(0x0000'0004u, 4, 5, 6);
    r2.active_count = 7;
    r2.flags = zref::population::kFlagSeed;
    b.offer(r2);
    b.expect("a second seed before the first was taken");
    check(top->seed_count_o == 7, "the LATEST committed descriptor is the descriptor", 7,
          top->seed_count_o);
    b.take_seed();
    check(top->seeds_issued_o == 2, "one hand-off, not two", 2, top->seeds_issued_o);
  }

  // ---- 4. the plane is DISABLED by clearing b1, and the terrain still counts
  {
    auto r = good(0x0000'0005u, 7, 8, 9);
    r.flags = 0;
    b.offer(r);
    b.expect("plane_enable clear");
    check(top->plane_en_o == 0, "b1 clear disables the analytic plane", 0, top->plane_en_o);
  }

  // ---- 5. A PLANE COMPONENT THAT CANNOT BE REPRESENTED IS REFUSED ----------
  // 0x0800 truncated to twelve bits is -2048. The bench asks for exactly that.
  for (int axis = 0; axis < 3; ++axis) {
    auto r = good(0xDEAD'0000u + static_cast<uint32_t>(axis), 999, 999, 999);
    const int16_t bad = static_cast<int16_t>(0x0800);
    if (axis == 0) r.plane_nx = bad;
    if (axis == 1) r.plane_ny = bad;
    if (axis == 2) r.plane_nz = bad;
    const uint32_t held_x = top->origin_x_o;
    const uint32_t held_pop = top->population_o;
    b.offer(r);
    b.expect("an unrepresentable plane component");
    check(top->origin_x_o == held_x, "a refused record is refused WHOLE: the origin did not move",
          held_x, top->origin_x_o);
    check(top->population_o == held_pop, "and the handle did not move", held_pop,
          top->population_o);
  }
  check(top->refused_normal_o == 3, "each unrepresentable component fired the counter", 3,
        top->refused_normal_o);
  // the NEGATIVE side of the same boundary: -2048 fits and must be taken.
  {
    auto r = good(0x0000'0006u, 11, 12, 13);
    r.plane_ny = static_cast<int16_t>(-2048);
    b.offer(r);
    b.expect("the negative end of the representable range");
    check(top->refused_normal_o == 3, "-2048 is inside the range and is NOT refused", 3,
          top->refused_normal_o);
  }
  {
    auto r = good(0x0000'0007u, 14, 15, 16);
    r.plane_ny = 2047;
    b.offer(r);
    b.expect("the positive end of the representable range");
    check(top->refused_normal_o == 3, "2047 is inside the range and is NOT refused", 3,
          top->refused_normal_o);
  }

  // ---- 6. an active_count above CAPACITY is refused, not wrapped -----------
  {
    const uint32_t held_x = top->origin_x_o;
    auto r = good(0x0000'0008u, 777, 777, 777);
    r.active_count = kCapacity + 1;
    r.flags = zref::population::kFlagSeed;
    b.offer(r);
    b.expect("active_count above CAPACITY");
    check(top->refused_count_o == 1, "the count refusal fired", 1, top->refused_count_o);
    check(top->seed_valid_o == 0, "and NO seed was offered from a count that does not fit", 0,
          top->seed_valid_o);
    check(top->origin_x_o == held_x, "refused whole: the origin did not move", held_x,
          top->origin_x_o);
  }
  // CAPACITY itself is legal -- the boundary is not off by one.
  {
    auto r = good(0x0000'0009u, 21, 22, 23);
    r.active_count = kCapacity;
    r.flags = zref::population::kFlagSeed;
    b.offer(r);
    b.expect("active_count exactly at CAPACITY");
    check(top->seed_count_o == kCapacity, "a full generation is a legal seed", kCapacity,
          top->seed_count_o);
    b.take_seed();
  }

  // ---- 7. a reserved flag bit is refused -----------------------------------
  {
    const uint32_t held_x = top->origin_x_o;
    auto r = good(0x0000'000Au, 333, 333, 333);
    r.flags = 0x0004;  // b2: reserved by R41
    b.offer(r);
    b.expect("a reserved flag bit");
    check(top->refused_flags_o == 1, "the reserved-flag refusal fired", 1, top->refused_flags_o);
    check(top->origin_x_o == held_x, "refused whole: the origin did not move", held_x,
          top->origin_x_o);
  }

  // ---- 8. and a good record still lands after all of that ------------------
  b.offer(good(0x1234'5678u, -1, -2, -3));
  b.expect("a good record after four refusals");

  std::printf("[part_pop_directed] taken=%u refused[normal/count/flags]=[%u %u %u] seeds=%u\n",
              top->taken_o, top->refused_normal_o, top->refused_count_o, top->refused_flags_o,
              top->seeds_issued_o);
  top->final();
  return zhao::report_and_exit("part_pop_directed");
}
