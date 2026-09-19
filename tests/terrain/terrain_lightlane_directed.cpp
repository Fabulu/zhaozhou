// terrain_lightlane_directed.cpp -- TERRAIN's lit normals, end to end:
// the world-vertex store beside the projector's arena, the ratified face
// normal, and the ratified flat shade, driven by a reference stream at the
// replay's own rate (owner ruling R21).
//
// WHAT IS PROVED, and against what:
//
//   1. THE VALUE TRAVERSES. Vertices are written on the FILL beat -- the same
//      beat that writes `zhao_proj_subsystem`'s arena -- and a reference three
//      rows later produces a base light that is bit-identical to
//      `zref::terrain::face_normal` followed by
//      `zref::render::shade_from_world_normal_unclamped`, with the sun the
//      driver supplies. Every corner is a value the store had to hold: the
//      lattice is random, so a misrouted row is a WRONG NUMBER, not a
//      plausible one.
//   2. THE ORDER AND THE COUNT. src_id rides each triangle and comes back with
//      its light, so a reordering is caught; N references produce N lights.
//   3. THE RATE, MEASURED (R21's throughput demand). The driver offers
//      references every clock and measures the interval between lights. The
//      lane's cost is `zhao_terrain_shade`'s 147 clocks per triangle, and the
//      check is against the frame: 2,000 terrain triangles per frame (both
//      block headers derive it from design/budgets/workloads.yml) must fit in
//      the 1,666,666-clock compute frame.
//   4. THE STALE-GENERATION REFUSAL FIRES. Re-opening an arena bumps its
//      generation; a reference carrying the old one is refused, counted in
//      `stale_reads_o`, and emits no light -- the same event the projector's
//      own arena refuses, made by the same fact rather than a second opinion.
//   5. DEGENERACY travels: a collapsed triangle raises `degenerate_o` and
//      shades 0, and `degenerate_count_o` counts exactly those.
//
// POSITIVE CONTROL: --break-oracle corrupts one expected base by +1 and the
// suite must then FAIL. A checker never seen to fail is not a checker.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_lightlane.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_normals.hpp"
#include "zref/zref_terrain_shade.hpp"

using zhao::check;

namespace {

constexpr int kDepth = 81;
constexpr uint64_t kFrameClocks = 1666666ull;  // the compute frame
constexpr uint32_t kTerrainTrisPerFrame = 2000u;

struct Tri {
  uint32_t ia, ib, ic;
  uint16_t src;
};

uint32_t lcg(uint32_t& s) {
  s = s * 1664525u + 1013904223u;
  return s;
}

struct Lane {
  Vtb_terrain_lightlane& d;
  std::vector<zref::terrain::NormalVertex> world;  // the driver's own copy
  std::vector<int32_t> got_base;
  std::vector<uint16_t> got_src;
  std::vector<uint8_t> got_degen;
  uint64_t cycles = 0;
  uint64_t first_light_cycle = 0;
  uint64_t last_light_cycle = 0;

  explicit Lane(Vtb_terrain_lightlane& dut) : d(dut), world(kDepth * 4) {}

  void tick() {
    d.eval();
    if (d.light_valid && d.light_ready) {
      got_base.push_back(static_cast<int32_t>(d.light_base));
      got_src.push_back(static_cast<uint16_t>(d.light_src));
      got_degen.push_back(static_cast<uint8_t>(d.light_degenerate));
      if (got_base.size() == 1) first_light_cycle = cycles;
      last_light_cycle = cycles;
    }
    zhao::tick(d);
    ++cycles;
  }

  void idle(int n) {
    for (int i = 0; i < n; ++i) tick();
  }

  void open_arena(uint32_t arena, uint32_t gen) {
    d.open_v = 1;
    d.open_arena = static_cast<uint8_t>(arena);
    d.open_gen = static_cast<uint8_t>(gen);
    tick();
    d.open_v = 0;
    tick();
  }

  // The FILL beat: exactly the handshake that writes the projector's arena.
  void fill(uint32_t arena, uint32_t index, int32_t x, int32_t y, int32_t z) {
    d.fill_valid = 1;
    d.fill_ready = 1;
    d.fill_arena = static_cast<uint8_t>(arena);
    d.fill_index = static_cast<uint8_t>(index);
    d.fill_vx = static_cast<uint32_t>(x);
    d.fill_vy = static_cast<uint32_t>(y);
    d.fill_vz = static_cast<uint32_t>(z);
    tick();
    d.fill_valid = 0;
    d.fill_ready = 0;
    world[arena * kDepth + index] = {x, y, z};
  }
};

int32_t expect_base(const Lane& L, const Tri& t, uint32_t arena, int32_t sx, int32_t sy,
                    int32_t sz, bool* degen) {
  const zref::terrain::NormalVertex& a = L.world[arena * kDepth + t.ia];
  const zref::terrain::NormalVertex& b = L.world[arena * kDepth + t.ib];
  const zref::terrain::NormalVertex& c = L.world[arena * kDepth + t.ic];
  const zref::terrain::FaceNormal n = zref::terrain::face_normal(a, b, c, nullptr);
  *degen = n.degenerate;
  return zref::render::shade_from_world_normal_unclamped(n.x, n.y, n.z, sx, sy, sz, nullptr);
}

void reset(Vtb_terrain_lightlane& d) {
  d.rst_n = 0;
  d.fill_valid = 0;
  d.fill_ready = 0;
  d.open_v = 0;
  d.ref_valid = 0;
  d.light_ready = 1;
  d.eval();
  for (int i = 0; i < 6; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool break_oracle = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--break-oracle") == 0) break_oracle = true;

  Vtb_terrain_lightlane d;
  reset(d);
  Lane L(d);

  // The sun the bank publishes at power-on is the zenith; this run uses the
  // renderer's ratified key light, which is what zref's own constants carry.
  const int32_t sun_x = 26758, sun_y = 53521, sun_z = 26758;
  d.sun_x = static_cast<uint32_t>(sun_x);
  d.sun_y = static_cast<uint32_t>(sun_y);
  d.sun_z = static_cast<uint32_t>(sun_z);

  // ---- 1. a lattice in arena 1, generation 5 ------------------------------
  L.open_arena(1, 5);
  uint32_t seed = 0xA5133700u;
  for (int i = 0; i < kDepth; ++i) {
    // sub-metre spacing in Q16.16, the regime terrain lives in -- and a
    // pseudo-random height, so a misrouted row is a wrong NUMBER.
    const int32_t x = static_cast<int32_t>((i % 9) * 39322);        // 0.6 m steps
    const int32_t z = static_cast<int32_t>((i / 9) * 39322);
    const int32_t y = static_cast<int32_t>(lcg(seed) % 65536u) - 32768;
    L.fill(1, static_cast<uint32_t>(i), x, y, z);
  }

  // ---- 2. references at the replay's own rate -----------------------------
  std::vector<Tri> tris;
  for (int r = 0; r < 8; ++r) {
    for (int c = 0; c < 8; ++c) {
      const uint32_t i0 = static_cast<uint32_t>(r * 9 + c);
      tris.push_back({i0, i0 + 1, i0 + 9, static_cast<uint16_t>(0x1000 + tris.size())});
      tris.push_back({i0 + 1, i0 + 10, i0 + 9, static_cast<uint16_t>(0x1000 + tris.size())});
      if (tris.size() >= 24) break;
    }
    if (tris.size() >= 24) break;
  }
  // A COLLAPSED triangle: all three corners the same row. The law returns 0.
  tris.push_back({7, 7, 7, 0x2000});

  size_t next = 0;
  const uint64_t start_cycle = L.cycles;
  while (L.got_base.size() < tris.size() && (L.cycles - start_cycle) < 200000ull) {
    if (next < tris.size()) {
      d.ref_valid = 1;
      d.ref_arena = 1;
      d.ref_gen = 5;
      d.ref_ia = static_cast<uint8_t>(tris[next].ia);
      d.ref_ib = static_cast<uint8_t>(tris[next].ib);
      d.ref_ic = static_cast<uint8_t>(tris[next].ic);
      d.ref_src = tris[next].src;
    } else {
      d.ref_valid = 0;
    }
    d.eval();
    const bool taken = d.ref_valid && d.ref_ready;
    L.tick();
    if (taken) ++next;
  }
  d.ref_valid = 0;
  L.idle(4);

  check(L.got_base.size() == tris.size(), "1.every reference produced one light",
        tris.size(), L.got_base.size());

  int mismatches = 0, src_bad = 0, degen_bad = 0;
  int expect_degen_count = 0;
  for (size_t i = 0; i < L.got_base.size() && i < tris.size(); ++i) {
    bool dg = false;
    int32_t want = expect_base(L, tris[i], 1, sun_x, sun_y, sun_z, &dg);
    if (break_oracle && i == 3) want += 1;
    if (dg) ++expect_degen_count;
    if (L.got_base[i] != want) {
      if (mismatches < 4)
        std::printf("   tri %zu: got %d want %d (src %04x)\n", i, L.got_base[i], want,
                    L.got_src[i]);
      ++mismatches;
    }
    if (L.got_src[i] != tris[i].src) ++src_bad;
    if ((L.got_degen[i] != 0) != dg) ++degen_bad;
  }
  check(mismatches == 0,
        "1.every base light is bit-identical to face_normal -> "
        "shade_from_world_normal_unclamped over the stored world vertices",
        0, mismatches);
  check(src_bad == 0, "2.every light came back under its own src_id, in order", 0, src_bad);
  check(degen_bad == 0, "5.the degenerate verdict travels with the light", 0, degen_bad);
  check(d.degenerate_count == static_cast<uint32_t>(expect_degen_count),
        "5.and the counter counts exactly those", uint32_t(expect_degen_count),
        d.degenerate_count);
  check(d.degen_mismatch == 0,
        "5.the seam check between the normal producer and the shade law is quiet", 0,
        d.degen_mismatch);
  check(d.triangles_shaded == tris.size(), "2.the shade block saw every triangle",
        tris.size(), d.triangles_shaded);
  check(d.refs_taken == tris.size(), "2.and the lane took every reference", tris.size(),
        d.refs_taken);
  check(d.lights_emitted == tris.size(), "2.and emitted every light", tris.size(),
        d.lights_emitted);

  // ---- 3. THE RATE, against the frame -------------------------------------
  const uint64_t span = L.last_light_cycle - L.first_light_cycle;
  const uint64_t per_tri = span / (tris.size() - 1);
  std::printf("   %zu triangles in %llu clocks: %llu clocks per triangle\n", tris.size(),
              (unsigned long long)span, (unsigned long long)per_tri);
  check(per_tri <= 160,
        "3.the lane retires a triangle at zhao_terrain_shade's own rate (147 clocks, "
        "one in flight) with the reference stream offering every clock",
        160, per_tri);
  const uint64_t frame_cost = per_tri * kTerrainTrisPerFrame;
  std::printf("   %u terrain triangles per frame = %llu clocks of %llu (%.1f%%)\n",
              kTerrainTrisPerFrame, (unsigned long long)frame_cost,
              (unsigned long long)kFrameClocks, 100.0 * double(frame_cost) / double(kFrameClocks));
  check(frame_cost < kFrameClocks,
        "3.so the ruled 2,000 terrain triangles per frame fit inside the compute frame, "
        "which is R21's throughput demand answered in clocks",
        1, frame_cost < kFrameClocks ? 1 : 0);

  // ---- 4. the stale-generation refusal FIRES ------------------------------
  const uint32_t stale_before = d.stale_reads;
  const uint32_t lights_before = d.lights_emitted;
  // The arena is RE-OPENED at generation 6 and NOT refilled, so its rows still
  // carry 5 -- the exact state the projector's arena refuses a read in. A
  // reference from the new generation must not be shaded out of last
  // generation's world vertices.
  L.open_arena(1, 6);
  d.ref_valid = 1;
  d.ref_arena = 1;
  d.ref_gen = 6;
  d.ref_ia = 0;
  d.ref_ib = 1;
  d.ref_ic = 9;
  d.ref_src = 0x3000;
  for (int i = 0; i < 40; ++i) {
    d.eval();
    if (d.ref_valid && d.ref_ready) {
      L.tick();
      d.ref_valid = 0;
    } else {
      L.tick();
    }
  }
  d.ref_valid = 0;
  L.idle(400);
  check(d.stale_reads == stale_before + 1,
        "4.a reference whose generation the stored rows do not carry is REFUSED and counted",
        stale_before + 1, d.stale_reads);
  check(d.lights_emitted == lights_before,
        "4.and it emits no light -- a stale corner never becomes a shade", lights_before,
        d.lights_emitted);

  d.final();
  return zhao::report_and_exit("terrain_lightlane_directed");
}
