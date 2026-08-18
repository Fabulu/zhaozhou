// terrain_patch_random.cpp — randomized differential for TERRAIN.PATCH.
//
// TWO LANES, because one uniform lane would test almost only the second one and
// would pass while the block was useless for real terrain:
//
//   Lane A, LATTICE-SHAPED. height16 planes in the range an authored Island
//   Patch actually carries (relief of a few tens of metres, a deep keel below
//   it, small negative scars), field height lanes of a few metres, and 0..4
//   live programs — which is what §9.1's derivation says the ordinary frame
//   looks like. This is the regime the §3.4 clamps and the footprint test
//   actually decide in. It must NEVER saturate: real terrain does not rail.
//
//   Lane B, DOMAIN-LIMIT. Both height16 rails, field lanes at the fx16 word
//   extremes, and 0..24 offered records so the §9.1 reject path is hammered.
//   The point is that every saturating add saturates exactly where the
//   reference's does and that the intake never overruns.
//
// Each lane ASSERTS it reached the states it exists for. Without those a green
// run could mean the lane sampled nothing worth sampling, which is how a
// flooring defect elsewhere in this tree survived 20,000 random triangles.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_patch.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_patch.hpp"

using zhao::check;
namespace zt = zref::terrain;

namespace {

void reset_dut(Vzhao_terrain_patch& top) {
  top.rst_n = 0;
  top.list_clear_i = 0;
  top.patch_id_i = 0;
  top.fld_add_valid_i = 0;
  top.vtx_valid_i = 0;
  top.fld_valid_i = 0;
  top.st_ready_i = 0;
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
  int32_t range(int32_t lo, int32_t hi) {
    const uint64_t span = static_cast<uint64_t>(hi - lo) + 1;
    return lo + static_cast<int32_t>(next() % span);
  }
  bool chance(int n) { return (next() % static_cast<uint64_t>(n)) == 0; }
};

struct Stats {
  uint32_t vertices = 0;
  uint32_t patches = 0;
  uint32_t clamp_compose = 0;  // §3.4 clamp 1 fired
  uint32_t clamp_live = 0;     // §3.4 clamp 2 fired
  uint32_t uncovered = 0;      // a lane's footprint missed the vertex
  uint32_t saturated = 0;      // the chain hit an fx16 rail
  uint32_t rejected = 0;       // §9.1 reject path taken
  uint32_t dirty = 0;
  uint32_t edge_probes = 0;  // vertices placed EXACTLY on a footprint edge
};

/** Load a field list into the DUT, mirroring the oracle's verdicts. */
void load_list(Vzhao_terrain_patch& dut, zt::FieldList& oracle,
               const std::vector<zt::FieldRecord>& offered, uint16_t patch_id, Stats& st) {
  oracle.clear();
  for (size_t i = 0; i < offered.size(); ++i) {
    dut.list_clear_i = (i == 0) ? 1 : 0;
    dut.patch_id_i = patch_id;
    dut.fld_add_valid_i = 1;
    dut.fld_add_x0_i = offered[i].x0;
    dut.fld_add_z0_i = offered[i].z0;
    dut.fld_add_x1_i = offered[i].x1;
    dut.fld_add_z1_i = offered[i].z1;
    dut.fld_add_hash_i = offered[i].program_hash;
    dut.fld_add_cmd_i = offered[i].cmd_index;
    zhao::tick(dut);
    dut.fld_add_valid_i = 0;
    dut.list_clear_i = 0;
    const bool want = oracle.offer(offered[i], patch_id);
    const bool got = dut.fld_add_accept_o != 0;
    check(got == want, "the accept/reject verdict matches the oracle", want ? 1 : 0, got ? 1 : 0);
    if (!want) {
      ++st.rejected;
      check(dut.fld_add_reject_o != 0, "a rejected record raises the reject pulse", 1,
            dut.fld_add_reject_o);
      check(dut.trace_hash_o == offered[i].program_hash, "the trace event carries the hash",
            offered[i].program_hash, dut.trace_hash_o);
      check(dut.trace_cmd_o == offered[i].cmd_index, "the trace event carries the command index",
            offered[i].cmd_index, dut.trace_cmd_o);
      check(dut.trace_patch_id_o == patch_id, "the trace event carries the patch id", patch_id,
            dut.trace_patch_id_o);
    }
  }
  if (offered.empty()) {
    dut.list_clear_i = 1;
    zhao::tick(dut);
    dut.list_clear_i = 0;
  }
  check(dut.fields_active_o == oracle.size(), "fields_active matches the oracle list length",
        static_cast<uint32_t>(oracle.size()), dut.fields_active_o);
}

void run_lane(Vzhao_terrain_patch& dut, Rng& rng, int patches, bool lattice, Stats& st) {
  constexpr int32_t kOne = 1 << 16;
  zt::FieldList oracle;
  oracle.reset();
  uint32_t rejects_seen = 0;

  for (int p = 0; p < patches; ++p) {
    ++st.patches;
    const uint16_t patch_id = static_cast<uint16_t>(rng.range(0, 65535));

    // ---- the patch envelope and its live-field records -------------------
    // Lane A puts the envelope somewhere plausible on a 32 m-half-extent patch;
    // lane B uses the whole fx16 span so the footprint test sees extremes.
    const int32_t half = lattice ? (32 * kOne) : (1 << 30);
    const int n_offer = lattice ? rng.range(0, 4) : rng.range(0, 24);
    std::vector<zt::FieldRecord> offered;
    offered.reserve(static_cast<size_t>(n_offer));
    for (int i = 0; i < n_offer; ++i) {
      zt::FieldRecord r;
      // A footprint that sometimes covers everything, sometimes a corner, and
      // sometimes (lane B) is inverted so it covers nothing.
      const int32_t ax = rng.range(-half, half);
      const int32_t bx = rng.range(-half, half);
      const int32_t az = rng.range(-half, half);
      const int32_t bz = rng.range(-half, half);
      const bool invert = !lattice && rng.chance(8);
      r.x0 = invert ? (ax > bx ? ax : bx) : (ax < bx ? ax : bx);
      r.x1 = invert ? (ax < bx ? ax : bx) : (ax > bx ? ax : bx);
      r.z0 = az < bz ? az : bz;
      r.z1 = az > bz ? az : bz;
      if (rng.chance(3)) {  // often: the whole patch
        r.x0 = -half;
        r.x1 = half;
        r.z0 = -half;
        r.z1 = half;
      }
      r.program_hash = static_cast<uint32_t>(rng.next() >> 32);
      r.cmd_index = static_cast<uint16_t>(i);
      offered.push_back(r);
    }
    load_list(dut, oracle, offered, patch_id, st);
    // The subpatch dirty mask accumulates over the patch and is cleared by the
    // same `list_clear` that empties the field list, so the model resets here.
    uint16_t exp_mask = 0;
    check(dut.subpatch_dirty_o == 0, "list_clear cleared the subpatch dirty mask", 0,
          dut.subpatch_dirty_o);
    rejects_seen = st.rejected;
    check(dut.programs_rejected_o == rejects_seen,
          "programs_rejected accumulates across patches, unaffected by list_clear", rejects_seen,
          dut.programs_rejected_o);

    // ---- the vertices ----------------------------------------------------
    const int n_vtx = lattice ? 12 : 8;
    for (int v = 0; v < n_vtx; ++v) {
      zt::ComposeIn in;
      if (lattice) {
        // An authored island: relief of a few tens of metres over a deep keel,
        // small negative scars. height16 is S 1.7.8 metres, so 256 raw = 1 m.
        const int32_t base_m4 = rng.range(-8, 40);  // -2 .. +10 m
        in.base = static_cast<int16_t>(base_m4 * 64);
        in.scar = static_cast<int16_t>(-rng.range(0, 40) * 64);
        // the keel: 50 m below, with the occasional thin lip ABOVE base so the
        // §3.4 clamp-1 path is genuinely sampled
        in.bottom = rng.chance(6) ? static_cast<int16_t>(in.base + rng.range(1, 400))
                                  : static_cast<int16_t>(in.base - rng.range(200, 12800));
        in.dual = true;
        in.wx = rng.range(-half, half);
        in.wz = rng.range(-half, half);
      } else {
        in.base = static_cast<int16_t>(rng.range(-32768, 32767));
        in.scar = static_cast<int16_t>(rng.range(-32768, 32767));
        in.bottom = static_cast<int16_t>(rng.range(-32768, 32767));
        in.dual = !rng.chance(4);  // the legacy page is a quarter of lane B
        in.wx = rng.range(-half, half);
        in.wz = rng.range(-half, half);
      }

      // THE CLOSED INTERVAL IS A MEASURE-ZERO EVENT. Uniform random world
      // coordinates never land exactly on a footprint edge, so a `<` that
      // should be `<=` survives any amount of uniform sampling — a mutation
      // sweep proved exactly that, with the directed suite red and this lane
      // green. Snap onto (and one raw unit either side of) a real footprint
      // edge often enough that the boundary is genuinely exercised.
      if (oracle.size() > 0 && !rng.chance(3)) {
        const int lane_i = rng.range(0, oracle.size() - 1);
        const int32_t xs[4] = {oracle[lane_i].x0 - 1, oracle[lane_i].x0, oracle[lane_i].x1,
                               oracle[lane_i].x1 + 1};
        const int32_t zs[4] = {oracle[lane_i].z0 - 1, oracle[lane_i].z0, oracle[lane_i].z1,
                               oracle[lane_i].z1 + 1};
        in.wx = xs[rng.range(0, 3)];
        in.wz = zs[rng.range(0, 3)];
        ++st.edge_probes;
      }

      int32_t fh[zt::kMaxPatchFields];
      for (int i = 0; i < zt::kMaxPatchFields; ++i) fh[i] = 0;
      for (int i = 0; i < oracle.size(); ++i) {
        if (lattice) {
          fh[i] = rng.range(-20, 20) * kOne;  // a wave of a few metres
        } else {
          // the fx16 word, including the exact rails
          const int pick = static_cast<int>(rng.next() % 8);
          if (pick == 0)
            fh[i] = INT32_MAX;
          else if (pick == 1)
            fh[i] = INT32_MIN;
          else
            fh[i] = static_cast<int32_t>(static_cast<uint32_t>(rng.next() >> 32));
        }
        if (!zt::covers(oracle[i], in.wx, in.wz)) ++st.uncovered;
      }

      const zt::ComposeOut want = zt::compose_vertex(in, oracle, fh);

      const int vi = rng.range(0, 32);
      const int vj = rng.range(0, 32);
      const uint16_t src = static_cast<uint16_t>(rng.range(0, 65535));

      // Drive the vertex and its field-result burst, stalling the consumer on a
      // varying schedule so backpressure is exercised continuously rather than
      // in one dedicated case.
      dut.base_i = in.base;
      dut.scar_i = in.scar;
      dut.bottom_i = in.bottom;
      dut.dual_i = in.dual ? 1 : 0;
      dut.wx_i = in.wx;
      dut.wz_i = in.wz;
      dut.vi_i = static_cast<uint8_t>(vi);
      dut.vj_i = static_cast<uint8_t>(vj);
      dut.src_id_i = src;
      dut.vtx_valid_i = 1;
      dut.st_ready_i = rng.chance(3) ? 0 : 1;

      int lane = 0;
      bool seen = false;
      for (int cycle = 0; cycle < 256 && !seen; ++cycle) {
        if (lane < oracle.size()) {
          dut.fld_valid_i = rng.chance(4) ? 0 : 1;
          dut.fld_height_i = fh[lane];
        } else {
          dut.fld_valid_i = 0;
        }
        dut.eval();
        const bool took_vtx = dut.vtx_valid_i && dut.vtx_ready_o;
        const bool took_fld = dut.fld_valid_i && dut.fld_ready_o;
        const bool publishing = dut.st_valid_o && dut.st_ready_i;
        if (publishing) {
          check(
              static_cast<int32_t>(dut.top_o) == want.live_top,
              lattice ? "lane A live_top matches the oracle" : "lane B live_top matches the oracle",
              static_cast<uint32_t>(want.live_top), static_cast<uint32_t>(dut.top_o));
          check(static_cast<int32_t>(dut.compose_top_o) == want.compose_top,
                lattice ? "lane A compose_top matches" : "lane B compose_top matches",
                static_cast<uint32_t>(want.compose_top), static_cast<uint32_t>(dut.compose_top_o));
          check(static_cast<int32_t>(dut.bottom_o) == want.bottom,
                lattice ? "lane A bottom matches" : "lane B bottom matches",
                static_cast<uint32_t>(want.bottom), static_cast<uint32_t>(dut.bottom_o));
          check((dut.st_dirty_o != 0) == want.dirty,
                lattice ? "lane A dirty matches" : "lane B dirty matches", want.dirty ? 1 : 0,
                dut.st_dirty_o != 0 ? 1 : 0);
          check(dut.st_src_id_o == src, "src_id rides its own record", src, dut.st_src_id_o);
          seen = true;
        }
        zhao::tick(dut);
        if (took_vtx) dut.vtx_valid_i = 0;
        if (took_fld) ++lane;
        dut.st_ready_i = rng.chance(3) ? 0 : 1;
      }
      dut.st_ready_i = 1;
      dut.fld_valid_i = 0;
      check(seen, "every vertex produces a patch_state record", 1, seen ? 1 : 0);

      // Drain: one vertex in flight at a time makes the src_id check exact.
      for (int d = 0; d < 3; ++d) zhao::tick(dut);

      ++st.vertices;
      if (want.dirty) {
        ++st.dirty;
        exp_mask = static_cast<uint16_t>(exp_mask | zt::subpatch_mask(vi, vj));
      }
      // The mask is charter §11.1's 4x4 subpatch grid with SHARED border
      // vertices marking both neighbours. A mutation sweep found this
      // unchecked here while the directed suite caught it, so it is checked on
      // every vertex now.
      if (dut.subpatch_dirty_o != exp_mask)
        std::fprintf(stderr, "  mask at v(%d,%d) dirty=%d p=%d v=%d want=%04X got=%04X\n", vi, vj,
                     want.dirty ? 1 : 0, p, v, exp_mask,
                     static_cast<uint32_t>(dut.subpatch_dirty_o));
      check(dut.subpatch_dirty_o == exp_mask,
            lattice ? "lane A subpatch dirty mask matches" : "lane B subpatch dirty mask matches",
            exp_mask, dut.subpatch_dirty_o);
      if (in.dual && want.compose_top == (static_cast<int32_t>(in.bottom) << 8)) ++st.clamp_compose;
      if (in.dual && want.live_top == (static_cast<int32_t>(in.bottom) << 8)) ++st.clamp_live;
      if (want.live_top == INT32_MAX || want.live_top == INT32_MIN) ++st.saturated;
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  Vzhao_terrain_patch dut;

  const int patches = nightly ? 1200 : 90;
  Stats lat, words;

  reset_dut(dut);
  dut.st_ready_i = 1;
  Rng rng_a(0x7A7C4E'0001ULL);
  run_lane(dut, rng_a, patches, true, lat);

  reset_dut(dut);
  dut.st_ready_i = 1;
  Rng rng_b(0x7A7C4E'0002ULL);
  run_lane(dut, rng_b, patches, false, words);

  // The lanes must actually reach the states they exist for.
  check(lat.clamp_compose > 0, "lane A actually sampled the thin-lip compose clamp", 1,
        lat.clamp_compose);
  check(lat.clamp_live > 0, "lane A actually sampled the live clamp at bottom", 1, lat.clamp_live);
  check(lat.uncovered > 0, "lane A actually sampled uncovered footprints", 1, lat.uncovered);
  check(lat.edge_probes > 0, "lane A actually probed footprint edges exactly", 1, lat.edge_probes);
  check(lat.dirty > 0, "lane A actually sampled moved ground", 1, lat.dirty);
  check(lat.saturated == 0, "lane A never saturates: real terrain does not rail", 0, lat.saturated);

  check(words.saturated > 0, "lane B actually reached the fx16 rails", 1, words.saturated);
  check(words.rejected > 0, "lane B actually took the §9.1 reject path", 1, words.rejected);
  check(words.uncovered > 0, "lane B actually sampled uncovered footprints", 1, words.uncovered);
  check(words.clamp_live > 0, "lane B actually sampled the live clamp", 1, words.clamp_live);
  check(words.edge_probes > 0, "lane B actually probed footprint edges exactly", 1,
        words.edge_probes);

  std::printf(
      "terrain_patch_random: lane A %u vertices over %u patches "
      "(compose-clamp %u, live-clamp %u, uncovered %u, dirty %u, edge-probes %u, "
      "saturated %u); "
      "lane B %u vertices over %u patches "
      "(live-clamp %u, uncovered %u, edge-probes %u, saturated %u, rejected %u)%s\n",
      lat.vertices, lat.patches, lat.clamp_compose, lat.clamp_live, lat.uncovered, lat.dirty,
      lat.edge_probes, lat.saturated, words.vertices, words.patches, words.clamp_live,
      words.uncovered, words.edge_probes, words.saturated, words.rejected,
      nightly ? " [nightly]" : "");

  return zhao::report_and_exit("terrain_patch_random");
}
