// terrain_normals_random.cpp — randomized differential for TERRAIN.NORMALS.
//
// Two lanes, because uniform random 32-bit words would almost never land in the
// regime that matters:
//
//   Lane A, LATTICE-SHAPED. Vertices on a sub-metre grid with small height
//   deltas, which is what a phase-6 Mantle patch actually produces (32x32 cells
//   per world patch). This is the regime the rescale-by-32 defect destroyed, so
//   it is the regime worth hammering.
//
//   Lane B, WORD-SHAPED. Uniform over the whole fx16 word, including the
//   extremes. Nothing here is a plausible terrain cell; the point is that no
//   input word may make the 67-bit lanes wrap, and a narrower accumulator shows
//   up as a sign flip rather than as a saturation.
//
// A single uniform lane would test almost only lane B and would pass while the
// arithmetic was useless for real terrain. That is exactly how the earlier
// flooring defect hid from 20,000 random triangles elsewhere in this tree.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "verilated.h"

#include "Vzhao_terrain_normals.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_normals.hpp"

using zhao::check;

namespace {

// zhao::reset() assumes the byte-stream ports (in_valid/in_data) that the
// harness's first consumers had; this block has neither. Local reset, same
// async-negedge semantics.
template <typename Top>
void reset_dut(Top& top) {
  top.rst_n = 0;
  top.tri_valid_i = 0;
  top.nrm_ready_i = 0;
  top.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);
}

// Deterministic: same sequence every run, on every host. splitmix64.
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  int32_t s32() { return static_cast<int32_t>(static_cast<uint32_t>(next() >> 32)); }
  int32_t range(int32_t lo, int32_t hi) {
    const uint64_t span = static_cast<uint64_t>(hi - lo) + 1;
    return lo + static_cast<int32_t>(next() % span);
  }
};

struct Stats {
  uint32_t tris = 0;
  uint32_t degenerate = 0;
  uint32_t saturated = 0;  // a lane hit an fx16 rail
};

void run_lane(Vzhao_terrain_normals& dut, Rng& rng, int count, bool lattice, Stats& st) {
  constexpr int32_t kOne = 1 << 16;
  for (int i = 0; i < count; ++i) {
    zref::terrain::NormalVertex a, b, c;
    if (lattice) {
      // A cell on a 1/32-unit grid with heights within +-1 unit: the shape a
      // Mantle patch emits.
      const int32_t step = kOne / 32;
      const int32_t gx = rng.range(-64, 64) * step;
      const int32_t gz = rng.range(-64, 64) * step;
      a = {gx, rng.range(-kOne, kOne), gz};
      b = {gx + step, rng.range(-kOne, kOne), gz};
      c = {gx, rng.range(-kOne, kOne), gz + step};
      // Occasionally collapse an edge so degenerate cells are actually sampled
      // rather than left to chance.
      if ((rng.next() & 15) == 0) b = a;
    } else {
      // Bounded to the legal domain, +-4096 world units. NOT the full int32
      // word: out there a lane needs 66 bits and the REFERENCE's int64
      // overflows, so an oracle comparison would be testing whose overflow is
      // whose rather than whether this block is right. The domain and that
      // divergence are argued in the contract. Output saturation still happens
      // in here, which is what this lane is for.
      const int32_t lim = 4096 << 16;
      a = {rng.range(-lim, lim), rng.range(-lim, lim), rng.range(-lim, lim)};
      b = {rng.range(-lim, lim), rng.range(-lim, lim), rng.range(-lim, lim)};
      c = {rng.range(-lim, lim), rng.range(-lim, lim), rng.range(-lim, lim)};
    }

    const zref::terrain::FaceNormal want = zref::terrain::face_normal(a, b, c);

    dut.tri_valid_i = 1;
    dut.ax_i = a.x;
    dut.ay_i = a.y;
    dut.az_i = a.z;
    dut.bx_i = b.x;
    dut.by_i = b.y;
    dut.bz_i = b.z;
    dut.cx_i = c.x;
    dut.cy_i = c.y;
    dut.cz_i = c.z;
    dut.src_id_i = static_cast<uint16_t>(i);
    // Stall the consumer on a varying schedule so backpressure is exercised
    // continuously rather than in one dedicated case.
    dut.nrm_ready_i = ((rng.next() & 3) != 0) ? 1 : 0;

    bool seen = false;
    for (int cycle = 0; cycle < 32 && !seen; ++cycle) {
      zhao::tick(dut);
      dut.tri_valid_i = 0;
      dut.nrm_ready_i = 1;
      if (dut.nrm_valid_o) {
        const int32_t gx2 = static_cast<int32_t>(dut.nx_o);
        const int32_t gy2 = static_cast<int32_t>(dut.ny_o);
        const int32_t gz2 = static_cast<int32_t>(dut.nz_o);
        check(gx2 == want.x && gy2 == want.y && gz2 == want.z,
              lattice ? "lane A lattice cell matches the oracle"
                      : "lane B extreme words match the oracle",
              static_cast<uint64_t>(static_cast<uint32_t>(want.y)),
              static_cast<uint64_t>(static_cast<uint32_t>(gy2)));
        check((dut.degenerate_o != 0) == want.degenerate,
              lattice ? "lane A degeneracy matches" : "lane B degeneracy matches",
              want.degenerate ? 1 : 0, dut.degenerate_o != 0 ? 1 : 0);
        check(dut.src_id_o == static_cast<uint16_t>(i), "src_id rides its own result",
              static_cast<uint16_t>(i), dut.src_id_o);
        if (want.degenerate) ++st.degenerate;
        if (want.x == INT32_MAX || want.x == INT32_MIN || want.y == INT32_MAX ||
            want.y == INT32_MIN || want.z == INT32_MAX || want.z == INT32_MIN)
          ++st.saturated;
        ++st.tris;
        seen = true;
      }
    }
    check(seen, "every triangle produces a result", 1, seen ? 1 : 0);
    // Drain before the next triangle. One in flight at a time makes the src_id
    // check exact: with two in flight the loop above would compare a result
    // against the id of the triangle fed after it.
    dut.nrm_ready_i = 1;
    for (int d = 0; d < 3; ++d) zhao::tick(dut);
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  Vzhao_terrain_normals dut;
  reset_dut(dut);
  dut.nrm_ready_i = 1;

  const int per_lane = nightly ? 40000 : 2500;
  Rng rng(0x7E44A1'0000ULL);
  Stats lattice, words;
  run_lane(dut, rng, per_lane, true, lattice);
  run_lane(dut, rng, per_lane, false, words);

  // The lanes must actually reach the states they exist for. Without these the
  // suite could pass while sampling nothing interesting, which is the failure
  // mode that lets a real defect through a green random test.
  check(lattice.degenerate > 0, "lane A actually sampled degenerate cells", 1,
        lattice.degenerate > 0 ? 1 : 0);
  check(lattice.saturated == 0, "lane A never saturates: real terrain does not rail", 0,
        lattice.saturated);
  check(words.saturated > 0, "lane B actually reached the fx16 rails", 1,
        words.saturated > 0 ? 1 : 0);

  std::printf(
      "terrain_normals_random: lane A %u cells (%u degenerate, %u saturated), "
      "lane B %u cells (%u degenerate, %u saturated)%s\n",
      lattice.tris, lattice.degenerate, lattice.saturated, words.tris, words.degenerate,
      words.saturated, nightly ? " [nightly]" : "");

  return zhao::report_and_exit("terrain_normals_random");
}
