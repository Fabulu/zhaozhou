// terrain_place_cache_directed.cpp -- TERRAIN.PLACE's burst, read back out of
// TERRAIN.COMPCACHE's serve port.
//
// THE ACCEPTANCE QUESTION, stated once so the budget for this file is bounded:
// `zhao_console_core` now depends on one seam that no committed test covered --
// the placement shifter writing the cache's two position planes, and the
// tessellator's read port answering with those values at the right vertex.
// Neither block is under test here; the JOIN is.
//
// So this file checks four things and stops:
//
//   1. a placed patch's 66 writes are readable at every (vi, vj) the serve port
//      is asked about, and they agree with the law in spec/terrain_rules.md 1.3
//      -- computed here from the spec, never from the RTL's shifts;
//   2. wx follows the COLUMN and wz follows the ROW. A transposition leaves
//      every value a real placement of the wrong vertex, and this tree has
//      shipped that exact fault once already;
//   3. the two consumers of one placement AGREE -- what TERRAIN.PATCH is handed
//      combinationally for (vi, vj) is the number the cache serves for (vi, vj);
//   4. a REFUSED patch writes NOTHING. `pos_we_o` stays low for the whole
//      header cycle and the next hundred, and the patch already in the cache is
//      unchanged. This is the property `zhao_console_core`'s decision (c) rests
//      on: a refused placement must not half-write the next patch's position
//      planes over the one being served.
//
// WHAT IT DELIBERATELY DOES NOT CHECK, because two other files do:
// the arithmetic across all four pitches and the four census counters firing
// (terrain_place_directed.cpp), and the cache's own fill cursor, overrun and
// out-of-range behaviour (compcache_front_rtl_directed.cpp).

#include <cstdint>
#include <cstdio>

#include "verilated.h"
#include "Vtb_terrain_place_cache.h"
#include "zhao_sim.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

constexpr int kEdge = 33;
constexpr int kVerts = kEdge * kEdge;

// The law, written from spec/terrain_rules.md 1.3 and 2.1 rather than from the
// RTL: lattice index `idx` of patch `coord` sits at (coord * 32 + idx) cells of
// 2^pitch_log2 metres, in fx16 (16 fractional bits).
int32_t law_place(int32_t coord, int32_t idx, int pitch_log2) {
  const int64_t units = static_cast<int64_t>(coord) * 32 + idx;
  return static_cast<int32_t>(units << (16 + pitch_log2));
}

// The bench's own height for a vertex. Arbitrary and INJECTIVE over the
// lattice, which is what makes "the record at (vi, vj) reads back at (vi, vj)"
// a check rather than a coincidence.
int32_t bench_height(int vi, int vj) { return 0x00010000 + vj * 4096 + vi; }

void reset(Vtb_terrain_place_cache& d) {
  d.rst_n = 0;
  d.hdr_valid = 0;
  d.hdr_pitch_log2 = 0;
  d.hdr_patch_ix = 0;
  d.hdr_patch_iz = 0;
  d.hdr_env_x0 = 0;
  d.hdr_env_z0 = 0;
  d.hdr_src_id = 0;
  d.vtx_vi = 0;
  d.vtx_vj = 0;
  d.fill_start = 0;
  d.st_valid = 0;
  d.st_top = 0;
  d.st_bottom = 0;
  d.st_src_id = 0;
  d.dual = 0;
  d.serve_release = 0;
  d.lat_req = 0;
  d.lat_vi = 0;
  d.lat_vj = 0;
  d.lat_surface = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
}

// One header cycle. `env_ok == false` corrupts the envelope by one fx16 tick,
// which is the subtlest corruption the redundancy can catch.
void present(Vtb_terrain_place_cache& d, int32_t ix, int32_t iz, int pitch_log2,
             uint16_t src_id, bool env_ok = true) {
  d.hdr_valid = 1;
  d.hdr_pitch_log2 = static_cast<uint8_t>(pitch_log2 & 0xff);
  d.hdr_patch_ix = static_cast<uint16_t>(ix & 0xffff);
  d.hdr_patch_iz = static_cast<uint16_t>(iz & 0xffff);
  const int32_t ex = law_place(ix, 0, pitch_log2);
  d.hdr_env_x0 = env_ok ? ex : (ex + 1);
  d.hdr_env_z0 = law_place(iz, 0, pitch_log2);
  d.hdr_src_id = src_id;
  zhao::tick(d);
  d.hdr_valid = 0;
  zhao::tick(d);
}

// Drive a whole 1,089-record fill. The cache accepts on alternate clocks -- one
// record is two writes, top then bottom -- so this obeys `st_ready` rather than
// assuming a rate, which is the same thing TERRAIN.PATCH does in the core.
// Returns false if the fill did not complete inside a generous bound.
bool run_fill(Vtb_terrain_place_cache& d, uint16_t src_id) {
  // `fill_start` rides the first record, exactly as the composition drives it
  // from the streamer's `v_first_o`.
  d.fill_start = 1;
  int vi = 0, vj = 0, landed = 0, guard = 0;
  d.st_src_id = src_id;
  while (landed < kVerts && guard++ < 40000) {
    d.st_valid = 1;
    d.st_top = static_cast<uint32_t>(bench_height(vi, vj));
    d.st_bottom = static_cast<uint32_t>(bench_height(vi, vj) - 0x8000);
    d.eval();
    const bool taken = d.st_ready != 0;
    zhao::tick(d);
    d.fill_start = 0;
    if (taken) {
      ++landed;
      if (++vi == kEdge) { vi = 0; ++vj; }
    }
  }
  d.st_valid = 0;
  // The handover needs one more clock after the last record's bottom write.
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  return landed == kVerts;
}

// The serve port is REGISTERED: the datum is valid the cycle AFTER the request,
// which means the answer is readable IMMEDIATELY AFTER the clock edge that took
// the request and is GONE on the next one.
//
// The first version of this helper ticked twice and read every port as
// 0x5BADF00D -- the cache's poison for "nothing was asked". That is the block
// doing exactly what its header says: `req_ok_q <= lat_req_i && ...`, so with
// `lat_req_i` already low the second edge clears it and the combinational
// output reverts to poison. It reads like a store that lost its contents and it
// is a harness that asked twice, which is worth writing down because the
// symptom points at the wrong file.
void serve_read(Vtb_terrain_place_cache& d, int vi, int vj) {
  d.lat_req = 1;
  d.lat_vi = static_cast<uint8_t>(vi);
  d.lat_vj = static_cast<uint8_t>(vj);
  d.lat_surface = 0;
  zhao::tick(d);   // the request is taken here and the answer lands with it
  d.lat_req = 0;   // lowered, but NOT clocked: the datum is read before the
  d.eval();        // next edge retires it
}

// TERRAIN.PATCH's combinational view of the same placement.
void patch_probe(Vtb_terrain_place_cache& d, int vi, int vj) {
  d.vtx_vi = static_cast<uint8_t>(vi);
  d.vtx_vj = static_cast<uint8_t>(vj);
  d.eval();
}

// ---------------------------------------------------------------------------
// 1-3. A placed patch, filled, handed over, and read back.
// ---------------------------------------------------------------------------
void test_burst_reaches_the_serve_port() {
  Vtb_terrain_place_cache d;
  reset(d);

  // ix and iz are DIFFERENT, and so are every (vi, vj) probed below. With
  // ix == iz a transposed store returns the right number for the wrong reason.
  const int32_t kIx = 3, kIz = 7;
  const int kPitch = 1;  // spec 1.3: 2.0 m, the canonical battlefield pitch

  present(d, kIx, kIz, kPitch, 0x00A1);
  check(d.place_valid == 1, "a legal header places the patch", 1, d.place_valid);
  check(d.place_patches == 1, "and is counted once", 1, d.place_patches);
  check(d.place_env_mismatch == 0, "with no envelope disagreement", 0, d.place_env_mismatch);

  check(run_fill(d, 0x00A1), "the 1,089-record fill completes", kVerts, d.fill_records);
  check(d.patches_filled == 1, "and the buffer is handed to the serve side", 1,
        d.patches_filled);
  check(d.serve_valid == 1, "a patch is available to serve", 1, d.serve_valid);
  check(d.serve_src_id == 0x00A1, "and it names the patch that filled it", 0x00A1,
        d.serve_src_id);

  // The corners and two deliberately asymmetric interior vertices.
  const int probes[6][2] = {{0, 0}, {32, 0}, {0, 32}, {32, 32}, {1, 30}, {30, 1}};
  for (int p = 0; p < 6; ++p) {
    const int vi = probes[p][0], vj = probes[p][1];
    const int32_t want_x = law_place(kIx, vi, kPitch);
    const int32_t want_z = law_place(kIz, vj, kPitch);

    serve_read(d, vi, vj);
    const int32_t got_x = static_cast<int32_t>(d.lat_wx);
    const int32_t got_z = static_cast<int32_t>(d.lat_wz);
    const int32_t got_h = static_cast<int32_t>(d.lat_h);

    check(got_x == want_x, "the cache serves wx from the COLUMN", want_x, got_x);
    check(got_z == want_z, "the cache serves wz from the ROW", want_z, got_z);
    check(got_h == bench_height(vi, vj), "and the height that was written at that vertex",
          bench_height(vi, vj), got_h);

    // 3. The two consumers of one placement agree.
    patch_probe(d, vi, vj);
    check(static_cast<int32_t>(d.vtx_wx) == got_x,
          "TERRAIN.PATCH's lane and the cache agree about wx", got_x,
          static_cast<int32_t>(d.vtx_wx));
    check(static_cast<int32_t>(d.vtx_wz) == got_z,
          "TERRAIN.PATCH's lane and the cache agree about wz", got_z,
          static_cast<int32_t>(d.vtx_wz));
  }

  // 2, stated as its own check rather than left implicit in the six above: at
  // (1, 30) the two axes carry visibly different numbers, so a store that had
  // them swapped could not pass.
  serve_read(d, 1, 30);
  check(static_cast<int32_t>(d.lat_wx) != static_cast<int32_t>(d.lat_wz),
        "wx and wz are distinguishable at an asymmetric vertex", 1,
        static_cast<int32_t>(d.lat_wx) != static_cast<int32_t>(d.lat_wz));

  // fill_overrun is asserted zero HERE and is fired elsewhere: this bench never
  // offers a 1,090th record, so its silence is a statement about the stimulus.
  // `compcache_front_rtl_directed.cpp` is where it is driven to move.
  check(d.fill_overrun == 0, "no record was refused by the store", 0, d.fill_overrun);
  check(d.lat_oob == 0, "and no request left the grid", 0, d.lat_oob);
}

// ---------------------------------------------------------------------------
// 4. A REFUSED patch writes nothing, and does not disturb the served one.
// ---------------------------------------------------------------------------
// This is the property `zhao_console_core`'s decision (c) rests on. The cache's
// position planes are written through a port that is NOT gated on
// `fill_active_q`, so a refusal that emitted even a partial burst would land in
// the buffer parity the tessellator is reading.
void test_refused_patch_writes_nothing() {
  Vtb_terrain_place_cache d;
  reset(d);

  const int32_t kIx = 3, kIz = 7;
  const int kPitch = 1;
  present(d, kIx, kIz, kPitch, 0x00A1);
  check(run_fill(d, 0x00A1), "the good patch fills", kVerts, d.fill_records);
  check(d.serve_valid == 1, "and is being served", 1, d.serve_valid);

  const uint32_t patches_before = d.place_patches;

  // (i) an ILLEGAL PITCH. spec 1.3 freezes pitch_log2 to {-1, 0, +1, +2}.
  present(d, 11, 13, 3, 0x00B2);
  check(d.place_pitch_bad == 1, "an out-of-range pitch is counted", 1, d.place_pitch_bad);
  check(d.place_valid == 0, "and UNPLACES the block", 0, d.place_valid);
  check(d.vtx_placed == 0, "so the per-vertex lane reports unplaced", 0, d.vtx_placed);

  // (ii) an ENVELOPE that disagrees with the coordinate by one fx16 tick.
  present(d, 11, 13, kPitch, 0x00C3, /*env_ok=*/false);
  check(d.place_env_mismatch == 1, "a corrupt envelope is counted", 1, d.place_env_mismatch);
  check(d.place_valid == 0, "and also unplaces", 0, d.place_valid);

  check(d.place_patches == patches_before,
        "neither refusal counted as a placed patch", patches_before, d.place_patches);

  // NOT ONE WRITE, watched at the port rather than inferred. 200 clocks is
  // three times the 66-write burst a placed patch emits.
  int writes = 0;
  for (int i = 0; i < 200; ++i) {
    d.eval();
    if (d.pos_we) ++writes;
    zhao::tick(d);
  }
  check(writes == 0, "a refused patch emits no position write", 0, writes);

  // And the served patch is untouched: same placement, same heights.
  const int probes[3][2] = {{0, 0}, {5, 29}, {32, 32}};
  for (int p = 0; p < 3; ++p) {
    const int vi = probes[p][0], vj = probes[p][1];
    serve_read(d, vi, vj);
    check(static_cast<int32_t>(d.lat_wx) == law_place(kIx, vi, kPitch),
          "the served patch's wx survived two refusals", law_place(kIx, vi, kPitch),
          static_cast<int32_t>(d.lat_wx));
    check(static_cast<int32_t>(d.lat_wz) == law_place(kIz, vj, kPitch),
          "the served patch's wz survived two refusals", law_place(kIz, vj, kPitch),
          static_cast<int32_t>(d.lat_wz));
    check(static_cast<int32_t>(d.lat_h) == bench_height(vi, vj),
          "and so did its heights", bench_height(vi, vj),
          static_cast<int32_t>(d.lat_h));
  }
  check(d.patches_filled == 1, "and no second patch was filled", 1, d.patches_filled);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  test_burst_reaches_the_serve_port();
#ifndef ZHAO_EXPECT_FAIL
  // The refusal case is not part of the axis-swap control: a swapped axis says
  // nothing about whether a refused patch writes, and running it there would
  // mean the control passes on failures it is not about.
  test_refused_patch_writes_nothing();
#endif

  std::printf("terrain_place_cache_directed: %d checks, %d failed\n", g_checks, g_failed);

#ifdef ZHAO_EXPECT_FAIL
  // INVERTED POLARITY. This build is wired through `ZHAO_PLACE_CACHE_AXIS_SWAP`
  // and passes only when the checks FAIL -- it is evidence about the checks,
  // not about the design. See the note in `tb_terrain_place_cache.sv`.
  if (g_failed == 0) {
    std::printf(
        "CONTROL FAILED: the axis-swap mutant produced NO check failure. The "
        "placement checks cannot tell the column plane from the row plane, so "
        "the primary target's zero is worth nothing.\n");
    return 1;
  }
  std::printf("axis-swap control: %d of %d checks failed, as required\n", g_failed, g_checks);
  return 0;
#else
  return g_failed == 0 ? 0 : 1;
#endif
}
