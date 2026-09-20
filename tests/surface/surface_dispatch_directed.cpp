// surface_dispatch_directed.cpp -- SURFACE.DISPATCH against the world->patch
// law `zhao_terrain_heighttap` inverts (owner ruling R45, core entry I30).
//
// THE ORACLE IS THE OTHER BLOCK'S ARITHMETIC, restated here in C++ from
// `zhao_terrain_place.sv`'s placement rather than from this block's RTL:
//
//     wx(i) = (patch_ix * 32 + i) <<< (16 + pitch_log2)
//
// so with SH = 16 + pitch_log2 + 5,
//
//     patch_ix = floor(world_x / 2^SH)     env_x0 = patch_ix * 2^SH
//                                          env_x1 = (patch_ix + 1) * 2^SH
//
// WHAT ACTUALLY DISCRIMINATES, named up front:
//
//   1. NEGATIVE WORLD COORDINATES FLOOR. `zhao_terrain_heighttap`'s header
//      names the exact trap: "a camera at x = -1 m is in patch -1, and C's `/`
//      would put it in patch 0". A block that truncates toward zero puts every
//      stamp in the negative quadrant one patch too far in, and the sheet then
//      lands 32 metres from where the author put it -- plausible, wrong, and
//      invisible in any counter. Every negative case here is checked against
//      a floor computed with C++ integer division CORRECTED, not with it.
//   2. THE PATCH BOUNDARY IS CLOSED BELOW AND OPEN ABOVE. x = env_x1 exactly
//      belongs to the NEXT patch, so the two edges of a shared boundary must
//      resolve to different patches or two stamps on a seam land on one page.
//   3. AN UNRATIFIED PITCH PLACES NOTHING. `zhao_terrain_place` refuses a
//      pitch outside [-1, 2] and places nothing; this block gives the same
//      answer (a zero rectangle, which covers no texel) and counts it, rather
//      than picking a default shift and landing the stamp somewhere.
//   4. THE POLICY IS CONSTANT AND IS blend_en = 0, which is what R45 ratified.
#include <cstdint>
#include <cstdio>

#include "Vzhao_surface_dispatch.h"
#include "zhao_sim.hpp"

using zhao::check;

namespace {

constexpr int kFxShift = 16;
constexpr int kPatchCellsLog2 = 5;

/** floor(v / 2^sh) for a signed v -- the arithmetic shift, in C++. */
int32_t floor_shift(int32_t v, int sh) {
  int64_t q = static_cast<int64_t>(v) >> sh;  // arithmetic on a signed type
  return static_cast<int32_t>(q);
}

struct Bench {
  Vzhao_surface_dispatch& d;
  explicit Bench(Vzhao_surface_dispatch& dut) : d(dut) {}

  void present(int8_t pitch, int32_t tx, int32_t tz, bool fire) {
    d.pitch_log2_i = static_cast<uint8_t>(pitch);
    d.cmd_tx_i = static_cast<uint32_t>(tx);
    d.cmd_ty_i = static_cast<uint32_t>(tz);
    d.cmd_fire_i = fire ? 1 : 0;
    d.eval();
  }

  void step() {
    zhao::tick(d);
    d.cmd_fire_i = 0;
    d.eval();
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vzhao_surface_dispatch;  // heap + exit_hard: see zhao_sim.hpp
  Bench b(*top);
  top->rst_n = 0;
  top->cmd_fire_i = 0;
  top->eval();
  for (int i = 0; i < 3; ++i) zhao::tick(*top);
  top->rst_n = 1;
  top->eval();

  check(top->blend_en_o == 0, "R45's ratified policy: blend_en = 0", 0, top->blend_en_o);
  check(top->blend_o == 0, "blend mode 0 (the ABI mapping)", 0, top->blend_o);
  check(top->age_shift_o == 0, "age shift 0", 0, top->age_shift_o);

  const int32_t kMetre = 0x0001'0000;
  char nm[192];
  uint32_t fires = 0;

  for (int8_t pitch : {static_cast<int8_t>(-1), static_cast<int8_t>(0), static_cast<int8_t>(1),
                       static_cast<int8_t>(2)}) {
    const int sh = kFxShift + kPatchCellsLog2 + pitch;
    const int32_t width = static_cast<int32_t>(1) << sh;
    // Inside the stamp's stated +-4,096 m domain, and on both sides of zero
    // and of a patch boundary.
    const int32_t xs[] = {0,
                          kMetre,
                          -kMetre,
                          width - 1,
                          width,
                          -width,
                          -width - 1,
                          3 * width + 7,
                          -3 * width - 7,
                          100 * kMetre,
                          -100 * kMetre};
    for (int32_t x : xs) {
      for (int32_t z : {static_cast<int32_t>(0), x, -x}) {
        b.present(pitch, x, z, true);
        const int32_t want_ix = floor_shift(x, sh);
        const int32_t want_iz = floor_shift(z, sh);
        const int32_t want_x0 = want_ix << sh;
        const int32_t want_x1 = (want_ix + 1) << sh;
        const int32_t want_z0 = want_iz << sh;
        const int32_t want_z1 = (want_iz + 1) << sh;

        std::snprintf(nm, sizeof nm, "pitch %d, x %d: patch_ix floors", pitch, x);
        check(static_cast<int16_t>(top->patch_ix_o) == static_cast<int16_t>(want_ix), nm,
              static_cast<uint32_t>(static_cast<int16_t>(want_ix)),
              static_cast<uint32_t>(static_cast<int16_t>(top->patch_ix_o)));
        std::snprintf(nm, sizeof nm, "pitch %d, z %d: patch_iz floors", pitch, z);
        check(static_cast<int16_t>(top->patch_iz_o) == static_cast<int16_t>(want_iz), nm,
              static_cast<uint32_t>(static_cast<int16_t>(want_iz)),
              static_cast<uint32_t>(static_cast<int16_t>(top->patch_iz_o)));
        std::snprintf(nm, sizeof nm, "pitch %d, x %d: env_x0", pitch, x);
        check(static_cast<int32_t>(top->env_x0_o) == want_x0, nm,
              static_cast<uint32_t>(want_x0), top->env_x0_o);
        std::snprintf(nm, sizeof nm, "pitch %d, x %d: env_x1", pitch, x);
        check(static_cast<int32_t>(top->env_x1_o) == want_x1, nm,
              static_cast<uint32_t>(want_x1), top->env_x1_o);
        std::snprintf(nm, sizeof nm, "pitch %d, z %d: env_z0", pitch, z);
        check(static_cast<int32_t>(top->env_z0_o) == want_z0, nm,
              static_cast<uint32_t>(want_z0), top->env_z0_o);
        std::snprintf(nm, sizeof nm, "pitch %d, z %d: env_z1", pitch, z);
        check(static_cast<int32_t>(top->env_z1_o) == want_z1, nm,
              static_cast<uint32_t>(want_z1), top->env_z1_o);
        std::snprintf(nm, sizeof nm, "pitch %d: the rectangle is ordered", pitch);
        check(static_cast<int32_t>(top->env_x0_o) < static_cast<int32_t>(top->env_x1_o), nm, 1,
              static_cast<int32_t>(top->env_x0_o) < static_cast<int32_t>(top->env_x1_o));
        check(top->patch_valid_o == 1, "a ratified pitch resolves a patch", 1, top->patch_valid_o);
        b.step();
        ++fires;
      }
    }
  }

  // THE BOUNDARY, stated as its own check because the loop above could pass
  // while treating x = env_x1 as the same patch: the two sides of one seam.
  {
    const int sh = kFxShift + kPatchCellsLog2 + 0;
    const int32_t width = static_cast<int32_t>(1) << sh;
    b.present(0, width - 1, 0, true);
    const int32_t below = top->patch_ix_o;
    b.step();
    ++fires;
    b.present(0, width, 0, true);
    const int32_t above = top->patch_ix_o;
    b.step();
    ++fires;
    check(above == below + 1, "the patch boundary is closed below and open above",
          static_cast<uint32_t>(below + 1), static_cast<uint32_t>(above));
  }

  check(top->pitch_refused_o == 0, "no ratified pitch was refused", 0, top->pitch_refused_o);
  check(top->env_clamped_o == 0, "nothing inside the domain was clamped", 0, top->env_clamped_o);
  check(top->dispatched_o == fires, "every accepted stamp was counted", fires, top->dispatched_o);

  // ---- 3. AN UNRATIFIED PITCH PLACES NOTHING, AND IS COUNTED --------------
  for (int8_t bad : {static_cast<int8_t>(-2), static_cast<int8_t>(3), static_cast<int8_t>(127)}) {
    b.present(bad, 12345, -6789, true);
    std::snprintf(nm, sizeof nm, "pitch %d: no patch is resolved", bad);
    check(top->patch_valid_o == 0, nm, 0, top->patch_valid_o);
    check(top->env_x0_o == 0 && top->env_x1_o == 0 && top->env_z0_o == 0 && top->env_z1_o == 0,
          "and the rectangle is EMPTY, which covers no texel", 1,
          top->env_x0_o == 0 && top->env_x1_o == 0 && top->env_z0_o == 0 && top->env_z1_o == 0);
    b.step();
    ++fires;
  }
  check(top->pitch_refused_o == 3, "each unratified pitch fired the counter", 3,
        top->pitch_refused_o);

  // ---- THE CLAMP, fired on purpose ---------------------------------------
  // Far outside `zhao_surface_stamp`'s stated +-4,096 m domain, where the
  // upper edge of the last patch leaves s32. It SATURATES rather than wrapping,
  // because a wrapped envelope inverts the rectangle and the coverage test
  // then passes for the whole world.
  {
    const uint32_t before = top->env_clamped_o;
    b.present(2, 0x7FFF'FFFF, 0, true);
    check(static_cast<int32_t>(top->env_x0_o) < static_cast<int32_t>(top->env_x1_o),
          "a clamped rectangle is still ordered", 1,
          static_cast<int32_t>(top->env_x0_o) < static_cast<int32_t>(top->env_x1_o));
    b.step();
    check(top->env_clamped_o == before + 1, "and the clamp is counted", before + 1,
          top->env_clamped_o);
  }

  std::printf("[surface_dispatch_directed] dispatched=%u pitch_refused=%u env_clamped=%u\n",
              top->dispatched_o, top->pitch_refused_o, top->env_clamped_o);
  top->final();
  return zhao::report_and_exit("surface_dispatch_directed");
}
