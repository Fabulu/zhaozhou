// visible_rtl_directed.cpp -- TERRAIN.VISIBLE against zref::island::visible_set.
//
// The block's job is to turn a camera's View into the LIST of patches that have
// ground in it, by asking TERRAIN.ISLAND about every cell of a square window and
// forwarding the ones that came back RESIDENT.
//
// SO THE THING UNDER TEST IS A LIST, NOT A SET. A downstream consumer receives
// these in a stream and depends on their ORDER -- row-major, iz outer, ix inner
// -- so this compares the emitted sequence element by element against the
// reference. A test that compared membership only would pass a block that
// emitted every correct patch in the wrong order, and there is no picture in
// which that is visible: the same ground gets drawn.
//
// AND THE COST IS THE REJECTIONS. 793 of an 8 km island's 15,625 patches are
// ground, so a window overwhelmingly asks about sky. The test therefore measures
// and reports cycles per REJECTED patch on a window that emits nothing at all,
// which is the number the block is actually sized against.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vtb_island_visible.h"

#include "zhao_sim.hpp"
#include "zref/zref_island.hpp"

namespace isl = zref::island;

namespace {

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what, long long want = 1, long long got = 0) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %lld, got %lld)\n", what, want, got);
  }
}

constexpr int32_t kSide = 125;  // 8 km at 64 m patches
constexpr double kRadius = 15.9;  // 3.25 km^2 of ground -> 793 patches

// The SAME shaped island tests/terrain/island_dir_rtl_directed.cpp uses, and for
// the same stated reason: two tests describing one island. A disc, not a
// rectangle -- the corners are exactly the sky that must cost nothing.
bool is_ground(int32_t ix, int32_t iz) {
  const double dx = ix - kSide / 2, dz = iz - kSide / 2;
  return dx * dx + dz * dz <= kRadius * kRadius;
}

struct Emitted {
  int32_t ix, iz;
  uint32_t handle;
};

struct Snapshot {
  uint32_t examined, emitted, sky, out_of_extent, bad_pitch;
  uint32_t isl_res, isl_sky, isl_out, isl_bad;
};

Snapshot snap(Vtb_island_visible& d) {
  return Snapshot{d.cnt_examined,  d.cnt_emitted,          d.cnt_sky,
                  d.cnt_out_of_extent, d.cnt_bad_pitch,
                  d.isl_cnt_resident, d.isl_cnt_open_sky, d.isl_cnt_out_of_extent,
                  d.isl_cnt_bad_pitch};
}

struct WinResult {
  std::vector<Emitted> got;
  uint64_t cycles = 0;   // from the cycle after acceptance to the done pulse
  bool done = false;
  bool accepted = false;
};

// Run one window to completion. `stall_seed` non-zero drives p_ready from a
// deterministic PRNG, so the same window can be replayed under backpressure and
// the two lists compared -- a block that dropped a patch when its consumer
// stalled would emit a shorter, otherwise-correct list.
WinResult run_window(Vtb_island_visible& d, int32_t cx, int32_t cz, uint8_t radius,
                     uint32_t stall_seed = 0) {
  WinResult R;
  d.v_valid = 1;
  d.v_centre_ix = static_cast<uint32_t>(cx);
  d.v_centre_iz = static_cast<uint32_t>(cz);
  d.v_radius = radius;
  d.p_ready = 1;

  for (int cyc = 0; cyc < 10000; ++cyc) {
    d.eval();
    const bool took = d.v_valid && d.v_ready;
    zhao::tick(d);
    if (took) {
      R.accepted = true;
      break;
    }
  }
  d.v_valid = 0;

  uint32_t st = stall_seed;
  // A CAP THAT REPORTS RATHER THAN ONE THAT HIDES. The first version capped at
  // 40 million cycles per window and simply returned `done == false`, so a
  // window that stalled cost 40 seconds of silence and the next 239 did the
  // same. A cap has to say where it was when it gave up, or it converts a
  // deadlock into a hang.
  const uint64_t kCap = 400000ull;
  for (uint64_t cyc = 0; cyc < kCap; ++cyc) {
    if (stall_seed) {
      // READY ONE CYCLE IN EIGHT, and the number was chosen after measuring.
      //
      // The first version stalled three cycles in four and the stalled run was
      // not one cycle slower -- correctly, and the check that said it should be
      // was wrong. The query path costs ~3 cycles per patch, so a consumer
      // ready 75% of the time drains the emit register far faster than it
      // fills and backpressure never reaches the issue side at all. That is a
      // genuine property of the design and worth knowing; it is not a test of
      // backpressure. At one-in-eight the consumer IS the bottleneck, the
      // stall propagates through the retire cursor into the directory and out
      // to the issue cursor, and the list still has to come out whole.
      st = st * 1664525u + 1013904223u;
      d.p_ready = ((st >> 17) & 7u) == 0u;
    } else {
      d.p_ready = 1;
    }
    d.eval();
    if (d.p_valid && d.p_ready)
      R.got.push_back(Emitted{static_cast<int32_t>(d.p_ix), static_cast<int32_t>(d.p_iz),
                              d.p_handle});
    ++R.cycles;
    const bool fin = d.v_done;
    zhao::tick(d);
    if (fin) {
      R.done = true;
      break;
    }
  }
  if (!R.done) {
    d.eval();
    std::printf(
        "    STALLED window (%d,%d) r=%u stall=%u after %llu cycles: "
        "emitted %zu, p_valid %d, v_busy %d, examined %u emitted %u sky %u out %u bad %u\n",
        cx, cz, static_cast<unsigned>(radius), stall_seed,
        static_cast<unsigned long long>(R.cycles), R.got.size(), static_cast<int>(d.p_valid),
        static_cast<int>(d.v_busy), d.cnt_examined, d.cnt_emitted, d.cnt_sky,
        d.cnt_out_of_extent, d.cnt_bad_pitch);
    std::fflush(stdout);
  }
  d.p_ready = 1;
  return R;
}

// Compare one window against the oracle. Returns the number of faults it found
// and prints the first few, so a systematic break is distinguishable from a
// single bad cell at a glance.
int compare(const char* label, const WinResult& R, const std::vector<isl::Visible>& want,
            bool verbose = true) {
  int bad = 0;
  if (R.got.size() != want.size()) {
    if (verbose)
      std::printf("    %s: emitted %zu, oracle %zu\n", label, R.got.size(), want.size());
    ++bad;
  }
  const std::size_t n = R.got.size() < want.size() ? R.got.size() : want.size();
  int shown = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (R.got[i].ix != want[i].ix || R.got[i].iz != want[i].iz ||
        R.got[i].handle != want[i].page_handle) {
      ++bad;
      if (verbose && shown < 3) {
        std::printf("    %s[%zu]: rtl (%d,%d)h%u  oracle (%d,%d)h%u\n", label, i, R.got[i].ix,
                    R.got[i].iz, R.got[i].handle, want[i].ix, want[i].iz, want[i].page_handle);
        ++shown;
      }
    }
  }
  return bad;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  // UNBUFFERED. A hang with a full stdout buffer looks like a program that
  // never started, which points the investigation at set-up instead of at the
  // last thing it did -- the same trap CLAUDE.md records for the ofstream
  // crash.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  Vtb_island_visible d;

  isl::Desc desc;
  desc.island_id = 0x515Au;
  desc.pitch_log2 = 1;
  desc.extent_ix = static_cast<uint16_t>(kSide);
  desc.extent_iz = static_cast<uint16_t>(kSide);

  isl::Directory dir(desc);
  {
    uint32_t h = 1;
    for (int32_t iz = 0; iz < kSide; ++iz)
      for (int32_t ix = 0; ix < kSide; ++ix)
        if (is_ground(ix, iz)) dir.set(ix, iz, h++);
  }

  d.rst_n = 0;
  d.v_valid = 0;
  d.gw_en = 0;
  d.p_ready = 1;
  d.desc_extent_ix = static_cast<uint16_t>(kSide);
  d.desc_extent_iz = static_cast<uint16_t>(kSide);
  d.desc_pitch_log2 = 1;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;

  // The modelled store gets exactly the same ground, with the same handles.
  {
    uint32_t h = 1;
    for (int32_t iz = 0; iz < kSide; ++iz)
      for (int32_t ix = 0; ix < kSide; ++ix) {
        if (!is_ground(ix, iz)) continue;
        d.gw_en = 1;
        d.gw_addr = static_cast<uint16_t>((iz << 7) | ix);
        d.gw_handle = h++;
        zhao::tick(d);
      }
  }
  d.gw_en = 0;
  zhao::tick(d);

  std::printf("  island %d x %d, %zu patches of ground\n", kSide, kSide, dir.resident_count());

  // =====================================================================
  // DIRECTED 1 -- A WINDOW ENTIRELY IN SKY.
  //
  // The common case and the one the block is sized against: inside the
  // island's extent, over open sky, emitting nothing. Its cost per query is
  // the number reported below.
  // =====================================================================
  {
    const int32_t cx = 12, cz = 12;  // well inside the extent, well outside the disc
    const uint8_t r = 6;
    isl::View v{cx, cz, r};
    isl::WindowTally T{};
    const std::vector<isl::Visible> want = isl::visible_set(dir, v, &T);

    const Snapshot before = snap(d);
    const WinResult R = run_window(d, cx, cz, r);
    const Snapshot after = snap(d);

    const uint32_t examined = after.examined - before.examined;
    const uint32_t sky = after.sky - before.sky;
    const uint32_t emitted = after.emitted - before.emitted;

    std::printf(
        "  sky window (%d,%d) r=%u: examined %u, emitted %u, sky %u in %llu cycles"
        "  -> %.2f cycles per REJECTED patch\n",
        cx, cz, r, examined, emitted, sky, static_cast<unsigned long long>(R.cycles),
        examined ? static_cast<double>(R.cycles) / examined : 0.0);

    ck(R.accepted && R.done, "the sky window was accepted and completed", 1,
       (R.accepted && R.done) ? 1 : 0);
    ck(want.empty(), "the oracle agrees there is nothing there", 0,
       static_cast<long long>(want.size()));
    ck(compare("sky", R, want) == 0,
       "a window entirely in open sky emits NOTHING -- the ordinary case on an "
       "island whose grid is 94.9% nothing, and not an error",
       0, compare("sky", R, want, false));
    ck(examined == static_cast<uint32_t>((2 * r + 1) * (2 * r + 1)),
       "and it still ASKED about every cell of the square window: (2r+1)^2 "
       "questions, which is the work the block actually does",
       (2 * r + 1) * (2 * r + 1), examined);
    ck(sky == T.sky && emitted == T.emitted,
       "with the sky and emitted counters matching the oracle's own tally", T.sky, sky);
  }

  // =====================================================================
  // DIRECTED 2 -- STRADDLING THE EXTENT EDGE.
  //
  // The camera near a shore sees a window half of which is not the island at
  // all. Those cells must be counted OUT OF EXTENT and not as sky: one means
  // "no ground here", the other means "you have left the island", and a block
  // that conflated them would report a camera off the map as a camera over
  // water.
  // =====================================================================
  {
    struct Edge { int32_t cx, cz; uint8_t r; const char* where; };
    const Edge edges[] = {
        {2, 2, 5, "the (0,0) corner"},
        {kSide - 2, kSide - 2, 5, "the far corner"},
        {kSide - 1, 62, 4, "the +x shore, centred on the last legal column"},
        {-3, 62, 4, "entirely outside, to the west"},
    };
    int bad = 0, out_seen = 0;
    for (const Edge& e : edges) {
      isl::View v{e.cx, e.cz, e.r};
      isl::WindowTally T{};
      const std::vector<isl::Visible> want = isl::visible_set(dir, v, &T);
      const Snapshot before = snap(d);
      const WinResult R = run_window(d, e.cx, e.cz, e.r);
      const Snapshot after = snap(d);
      bad += compare(e.where, R, want);
      const uint32_t out = after.out_of_extent - before.out_of_extent;
      if (out != T.out_of_extent) {
        std::printf("    %s: out-of-extent rtl %u oracle %u\n", e.where, out, T.out_of_extent);
        ++bad;
      }
      if (T.out_of_extent > 0) ++out_seen;
      if (!R.done) ++bad;
    }
    ck(bad == 0,
       "a window straddling the extent edge emits exactly the patches inside "
       "it, in order, and counts the rest as OUT OF EXTENT rather than as sky",
       0, bad);
    ck(out_seen == 4,
       "and all four edge windows genuinely contained out-of-extent cells, so "
       "the check above is not satisfied by a window that never left the grid",
       4, out_seen);
  }

  // =====================================================================
  // DIRECTED 3 -- THE DENSEST AREA.
  //
  // Centred on the disc, every cell is ground, so this is the one window where
  // the emission order is fully exercised: 169 patches, all emitted, and any
  // permutation is a failure.
  // =====================================================================
  {
    const int32_t cx = kSide / 2, cz = kSide / 2;
    const uint8_t r = 6;
    isl::View v{cx, cz, r};
    isl::WindowTally T{};
    const std::vector<isl::Visible> want = isl::visible_set(dir, v, &T);

    const Snapshot before = snap(d);
    const WinResult R = run_window(d, cx, cz, r);
    const Snapshot after = snap(d);
    const uint32_t emitted = after.emitted - before.emitted;

    std::printf("  dense window (%d,%d) r=%u: %zu patches emitted in %llu cycles\n", cx, cz, r,
                R.got.size(), static_cast<unsigned long long>(R.cycles));

    ck(want.size() == static_cast<std::size_t>((2 * r + 1) * (2 * r + 1)),
       "the window on the island's densest area is entirely ground, so every "
       "cell of it must be emitted",
       (2 * r + 1) * (2 * r + 1), static_cast<long long>(want.size()));
    ck(compare("dense", R, want) == 0,
       "and the RTL emits exactly that list, IN ORDER -- row-major, iz outer, "
       "ix inner -- because a stream consumer depends on the order and a "
       "set comparison would pass a permutation",
       0, compare("dense", R, want, false));
    ck(emitted == T.emitted, "with the emitted counter agreeing", T.emitted, emitted);

    // ORDER, ASSERTED DIRECTLY. The comparison above would also catch this,
    // but stating it as its own property means a future change to `compare`
    // cannot quietly stop checking it.
    bool ordered = true;
    for (std::size_t i = 1; i < R.got.size(); ++i) {
      const Emitted& a = R.got[i - 1];
      const Emitted& b = R.got[i];
      if (!(b.iz > a.iz || (b.iz == a.iz && b.ix > a.ix))) ordered = false;
    }
    ck(ordered,
       "and the sequence is strictly increasing in (iz, ix), which is the "
       "row-major order zref::island::visible_set defines",
       1, ordered ? 1 : 0);

    // SAME WINDOW, STALLING CONSUMER. A block that dropped a patch while its
    // consumer was not ready would produce a shorter list and nothing else
    // would notice, because the patches it did emit are all correct.
    const WinResult S = run_window(d, cx, cz, r, /*stall_seed=*/0xC0FFEEu);
    ck(compare("dense-stalled", S, want) == 0,
       "and the identical list comes out under a consumer that is ready only "
       "three cycles in four -- backpressure costs throughput, never a patch",
       0, compare("dense-stalled", S, want, false));
    ck(S.cycles > R.cycles * 2,
       "which cost more than double the cycles, so the consumer really was the "
       "bottleneck -- a stall that the query path absorbs is not a test of "
       "backpressure, and at three-ready-in-four it absorbed it entirely",
       1, S.cycles > R.cycles * 2 ? 1 : 0);
  }

  // =====================================================================
  // DIRECTED 4 -- RADIUS 0: ONE PATCH.
  //
  // The degenerate window. (2*0+1)^2 = 1, so the block must ask exactly one
  // question -- an off-by-one in the loop bound shows here and nowhere else,
  // because at radius 6 an extra row is 13 cells lost in 169.
  // =====================================================================
  {
    struct One { int32_t ix, iz; const char* what; };
    const One ones[] = {
        {kSide / 2, kSide / 2, "a resident patch"},
        {5, 5, "a sky patch"},
        {-1, 60, "a negative coordinate"},
        {kSide, 60, "one past the extent"},
    };
    int bad = 0;
    uint32_t total_examined = 0;
    for (const One& o : ones) {
      isl::View v{o.ix, o.iz, 0};
      isl::WindowTally T{};
      const std::vector<isl::Visible> want = isl::visible_set(dir, v, &T);
      const Snapshot before = snap(d);
      const WinResult R = run_window(d, o.ix, o.iz, 0);
      const Snapshot after = snap(d);
      const uint32_t ex = after.examined - before.examined;
      total_examined += ex;
      if (ex != 1) {
        std::printf("    radius 0 at %s: examined %u, expected 1\n", o.what, ex);
        ++bad;
      }
      bad += compare(o.what, R, want);
      if (!R.done) ++bad;
    }
    ck(bad == 0,
       "a radius-0 window asks exactly ONE question and emits the one patch if "
       "it is ground -- resident, sky, negative and past-the-end all behave",
       0, bad);
    ck(total_examined == 4, "four radius-0 windows asked four questions in total", 4,
       total_examined);
  }

  // =====================================================================
  // DIRECTED 5 -- A MALFORMED DESCRIPTOR.
  //
  // The block does not own the pitch rule and must not short-circuit on it:
  // it walks the whole window, forwards every answer TERRAIN.ISLAND gives, and
  // emits nothing. The reference does the same, which is why the two agree.
  // =====================================================================
  {
    isl::Desc bad_desc = desc;
    bad_desc.pitch_log2 = 7;
    isl::Directory bdir(bad_desc);
    for (int32_t iz = 0; iz < kSide; ++iz)
      for (int32_t ix = 0; ix < kSide; ++ix)
        if (is_ground(ix, iz)) bdir.set(ix, iz, 1);

    isl::View v{kSide / 2, kSide / 2, 3};
    isl::WindowTally T{};
    const std::vector<isl::Visible> want = isl::visible_set(bdir, v, &T);

    d.desc_pitch_log2 = 7;
    const Snapshot before = snap(d);
    const WinResult R = run_window(d, kSide / 2, kSide / 2, 3);
    const Snapshot after = snap(d);
    d.desc_pitch_log2 = 1;  // RESTORED, or every later window returns BAD_PITCH

    const uint32_t bp = after.bad_pitch - before.bad_pitch;
    ck(R.done && R.got.empty() && want.empty(),
       "a window on an island whose pitch is illegal emits nothing at all", 0,
       static_cast<long long>(R.got.size()));
    ck(bp == T.bad_pitch && bp == 49,
       "and every one of its 49 cells is counted BAD PITCH -- the window is "
       "walked rather than short-circuited, because this block does not own "
       "the pitch rule and a second copy of it would be a second thing to "
       "keep in step",
       T.bad_pitch, bp);
  }

  // =====================================================================
  // RANDOMISED -- CENTRES AND RADII NOBODY CHOSE.
  //
  // Deterministic generator, so a failure is reproducible. Centres are drawn
  // across and beyond the island so windows land wholly inside, wholly outside,
  // and straddling; radii from 0 to 7.
  // =====================================================================
  {
    uint32_t st = 0x5EEDBEEFu;
    auto nxt = [&st]() {
      st = st * 1664525u + 1013904223u;
      return st;
    };

    const int kN = 240;
    int bad = 0, tally_bad = 0, invariant_bad = 0, dir_bad = 0, incomplete = 0;
    isl::WindowTally total{};
    int wholly_in = 0, wholly_out = 0, straddling = 0, with_ground = 0;
    bool radius_drawn[8] = {false, false, false, false, false, false, false, false};
    const Snapshot phase_start = snap(d);

    for (int i = 0; i < kN; ++i) {
      const uint32_t r0 = nxt(), r1 = nxt(), r2 = nxt(), r3 = nxt();
      // A QUARTER OF THE DRAWS ARE AIMED AT THE ISLAND, and that is a
      // correction rather than a convenience. Uniform centres over
      // [-15, 145]^2 put only 12 of 240 windows over ground, because the disc
      // is 5% of the grid -- so the ORDER comparison, which is the property
      // this phase exists to stress, had almost nothing to disagree about.
      // Drawing uniformly and then asserting good coverage is asserting a
      // property of the island, not of the test.
      //
      // AND EVERY DRAW TAKES THE HIGH BITS. A power-of-two-modulus LCG's low
      // bits are almost not random at all -- bit k has period 2^(k+1) -- and
      // with four draws per iteration the collapse was total: `r3 & 3` was
      // never zero, so NOT ONE window was aimed, and `r2 % 8` produced exactly
      // two radii, 0 and 4, 120 times each. The phase reported 240 windows and
      // had tested four.
      //
      // It looked fine. Nothing failed except a coverage assertion, and the
      // obvious reading of that failure was "the thresholds are too strict".
      // That is the tell CLAUDE.md records: the comfortable explanation
      // arrives first and absolves the test. `island_dir_rtl_directed` already
      // takes `(r >> 8)` for exactly this reason.
      const bool aimed = ((r3 >> 24) & 3u) == 0u;
      const int32_t cx = aimed ? (kSide / 2 + static_cast<int32_t>((r0 >> 8) % 33u) - 16)
                               : (static_cast<int32_t>((r0 >> 8) % 160u) - 15);
      const int32_t cz = aimed ? (kSide / 2 + static_cast<int32_t>((r1 >> 8) % 33u) - 16)
                               : (static_cast<int32_t>((r1 >> 8) % 160u) - 15);
      const uint8_t rad = static_cast<uint8_t>((r2 >> 13) % 8u);

      isl::View v{cx, cz, rad};
      isl::WindowTally T{};
      const std::vector<isl::Visible> want = isl::visible_set(dir, v, &T);

      const Snapshot before = snap(d);
      const WinResult R = run_window(d, cx, cz, rad, (i % 5 == 0) ? (0x1000u + i) : 0u);
      const Snapshot after = snap(d);
      if (!R.done) ++incomplete;

      char label[64];
      std::snprintf(label, sizeof label, "rnd#%d(%d,%d,r%u)", i, cx, cz, rad);
      bad += compare(label, R, want, bad < 12);

      const uint32_t ex = after.examined - before.examined;
      const uint32_t em = after.emitted - before.emitted;
      const uint32_t sk = after.sky - before.sky;
      const uint32_t ou = after.out_of_extent - before.out_of_extent;
      const uint32_t bp = after.bad_pitch - before.bad_pitch;

      if (ex != T.examined || em != T.emitted || sk != T.sky || ou != T.out_of_extent ||
          bp != T.bad_pitch) {
        if (tally_bad < 3)
          std::printf("    %s tally: rtl ex%u em%u sky%u out%u | oracle ex%u em%u sky%u out%u\n",
                      label, ex, em, sk, ou, T.examined, T.emitted, T.sky, T.out_of_extent);
        ++tally_bad;
      }
      // THE COUNTERS ARE NOT ONE NUMBER RESTATED. `examined` is incremented at
      // ISSUE and the other four at ANSWER, so this equality is a real claim:
      // every question asked came back exactly once. A query lost in the
      // composition would leave the list correct and this sum short.
      if (ex != em + sk + ou + bp) ++invariant_bad;

      // The composed directory's own ledger must move by the same amounts --
      // this block is its only client, so a discrepancy means the block either
      // invented a query or swallowed an answer.
      if ((after.isl_res - before.isl_res) != em || (after.isl_sky - before.isl_sky) != sk ||
          (after.isl_out - before.isl_out) != ou)
        ++dir_bad;

      radius_drawn[rad & 7] = true;
      total.examined += T.examined;
      total.emitted += T.emitted;
      total.sky += T.sky;
      total.out_of_extent += T.out_of_extent;
      if (T.out_of_extent == 0) ++wholly_in;
      else if (T.out_of_extent == T.examined) ++wholly_out;
      else ++straddling;
      if (T.emitted > 0) ++with_ground;
    }

    int radii_seen = 0;
    for (bool b : radius_drawn) if (b) ++radii_seen;

    const Snapshot phase_end = snap(d);
    std::printf(
        "  randomised: %d windows -> examined %u, emitted %u, sky %u, out %u"
        "  (%d wholly in, %d wholly out, %d straddling, %d with ground)\n",
        kN, total.examined, total.emitted, total.sky, total.out_of_extent, wholly_in, wholly_out,
        straddling, with_ground);

    ck(incomplete == 0, "every randomised window ran to completion", 0, incomplete);
    ck(bad == 0,
       "and every emitted list matches zref::island::visible_set exactly -- "
       "same patches, same handles, same order",
       0, bad);
    ck(tally_bad == 0,
       "with all five counters agreeing with the oracle's own WindowTally on "
       "every window, not merely in total",
       0, tally_bad);
    ck(invariant_bad == 0,
       "and examined == emitted + sky + out_of_extent + bad_pitch on every "
       "window -- examined is counted at ISSUE and the rest at ANSWER, so this "
       "says every question asked was answered exactly once",
       0, invariant_bad);
    ck(dir_bad == 0,
       "and the composed TERRAIN.ISLAND's own ledger moved by exactly the same "
       "amounts, so no query was invented and no answer swallowed",
       0, dir_bad);
    ck(phase_end.examined - phase_start.examined == total.examined,
       "with the phase's total examined count matching the oracle's",
       total.examined, phase_end.examined - phase_start.examined);

    // NOT VACUOUS. A randomised phase whose draws all landed in one region
    // would compare a lot of identical windows and prove one case.
    ck(wholly_in > 20 && wholly_out > 5 && straddling > 20 && with_ground > 20,
       "and the draws genuinely covered windows wholly inside the island, "
       "wholly outside it, straddling its edge, and containing ground -- so "
       "this is not a randomised walk over one kind of window",
       1,
       (wholly_in > 20 && wholly_out > 5 && straddling > 20 && with_ground > 20) ? 1 : 0);
    ck(radii_seen == 8,
       "and every radius from 0 to 7 was actually drawn -- the first version "
       "took the LCG's low bits and produced exactly two radii, 0 and 4, while "
       "reporting 240 windows",
       8, radii_seen);
    ck(total.emitted > 500,
       "with enough ground emitted for the ORDER comparison to have had "
       "something to disagree about",
       1, total.emitted > 500 ? 1 : 0);
  }

  // The cursor-alignment self-check must never have fired. It is the block's
  // own proof that the retire cursor is the coordinate of the answer arriving,
  // which is what makes the two-cursor design legal instead of a FIFO.
  ck(d.err_tag == 0,
     "and the block's own answer-alignment guard never fired: every answer "
     "carried the tag the retire cursor expected, which is the whole basis for "
     "having no in-flight coordinate queue",
     0, static_cast<long long>(d.err_tag));

  if (g_fail) {
    std::printf("[visible_rtl_directed] %d of %d checks FAILED\n", g_fail, g_checks);
    zhao::exit_hard(1);
  }
  std::printf("[visible_rtl_directed] %d checks passed\n", g_checks);
  zhao::exit_hard(0);
}
