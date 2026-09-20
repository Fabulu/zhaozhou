// terrain_bake_v2_mutant_control.cpp -- THE DRIVER ruling R93 says
// `tests/mutants/zhao_terrain_bake_v2_mutant.sv` must have or be retired.
//
// WHY THIS EXISTS AT ALL
// ----------------------------------------------------------------------------
// The mutant was committed on 2026-09-09, refreshed on 2026-09-20, and NOTHING
// HAS EVER ELABORATED IT. Ruling R93: "an uninstantiated MODULE costs the
// fitter nothing, but an unrun MUTANT is cited as evidence" --
// `design/prod_manifest.yml` quotes TERRAIN.BAKE at "267/267 against the zref
// oracle with a committed mutant seen to fail", and until this file existed
// nobody had seen it fail, because no target built it. R93 gives the next
// terrain packet the choice of a driver or retirement. This is the driver, and
// the reasoning for choosing it over retirement is in FINDINGS-terrain8.
//
// WHAT THE MUTANT BREAKS, AND WHY NO ORDINARY TEST CAN SEE IT
// ----------------------------------------------------------------------------
// `zhao_terrain_bake_v2` moved v1's 1,089-flop `meets` plane into an M10K and
// reads it back through a TWO-ROW REGISTER WINDOW with a one-row prefetch:
//
//     zhao_terrain_bake_v2.sv:537   m_raddr = (cj <= 30) ? (cj + 2) : PadRow
//     zhao_terrain_bake_v2.sv:553   all4 = mrow_lo[ci] & mrow_lo[ci+1]
//                                        & mrow_hi[ci] & mrow_hi[ci+1]
//     zhao_terrain_bake_v2.sv:865   mrow_lo <= mrow_hi;  mrow_hi <= mq;
//
// so at cell row `cj` the window must hold (row cj, row cj+1). The mutant
// changes the prefetch to `cj + 1`, and the ONE consequence is that from the
// FIRST ROW ADVANCE onward `mrow_hi` receives the row `mrow_lo` already holds:
// the window becomes (row cj, row cj). Row 0 is still correct, because the
// StBrA/StBrB/StBrC fill sequence addresses rows 0 and 1 explicitly.
//
// Nothing else moves. The DIG phase is untouched, every handshake completes,
// the vertex and cell scans are the same length, and each counter stays
// consistent with the stream it counts. Only the section 3.4 breach/heal
// DECISIONS change -- which is the mutant header's claim and is asserted here
// rather than repeated: fixture B proves layer B and the counters agree while
// layer D is wrong, so a suite that checked the scar plane and read the
// counters would be GREEN on a machine putting ground in the wrong place.
//
// THE MUTANT HEADER'S OWN DESCRIPTION OF THE FAULT IS WRONG, AND THAT IS
// EXACTLY WHAT AN UNRUN CONTROL BUYS YOU
// ----------------------------------------------------------------------------
// The mutant file says "at each row advance `mrow_hi` receives the row the
// window ALREADY holds in `mrow_lo`". That is true of the FIRST advance only.
// The slide is `mrow_lo <= mrow_hi; mrow_hi <= mq` (:865), so:
//
//   production  window(0)   = (0, 1)          <- the StBrA/StBrB/StBrC fill
//               window(cj)  = (cj, cj+1)      <- prefetch cj+2 during row cj
//   mutant      window(0)   = (0, 1)          IDENTICAL: the fill is explicit
//               window(1)   = (1, 1)          the header's described fault
//               window(cj>=2) = (cj-1, cj)    A ONE-ROW LAG, thereafter
//
// The first fixture written for this control assumed the header and used a
// row-alternating `meets` plane, for which `(cj, cj+1)` and `(cj-1, cj)` are
// indistinguishable -- it reported ZERO disagreements against a mutant that is
// genuinely broken. A description nothing has ever executed is a claim, and
// this one had been read by at least three passes. The fixture below
// discriminates the lag, and the header has been corrected.
//
// THE SIGNATURE IS CONSTRUCTED, NOT SAMPLED
// ----------------------------------------------------------------------------
// Fixture A makes `meets` true on exactly three BANDS of two lattice rows --
// {0,1}, {5,6}, {20,21} -- and false everywhere else, by putting the composed
// height exactly on the underside inside a band and one raw unit above it
// outside. A cell breaches iff both its rows meet, so:
//
//   oracle   all4(cj) = meets(cj) && meets(cj+1)   -> breaches rows 0, 5, 20
//   mutant   row 0 = (0,1) -> breaches row 0   AGREES (the fill is correct)
//            row 1 = (1,1) -> breaches row 1   the first-advance fault
//            row cj = (cj-1, cj)               -> breaches rows 6 and 21
//
// so the disagreement is EXACTLY rows {1, 5, 6, 20, 21} -- 5 rows x 32 cells
// = 160 -- and row 0 agrees. That exact row set is asserted, not merely "the
// planes differ": a difference count alone would pass for a mutant broken some
// other way, and a positive control has to show the instrument catches THIS
// fault. The three agreeing facts carry as much as the two disagreeing ones:
// row 0 correct pins the fill sequence, row 1 pins the first advance, and rows
// 6 and 21 pin the lag.
//
// BUILD NOTE: the mutant is verilated with `--prefix Vzhao_terrain_bake_v2` so
// that `bake_dev.hpp` (the shared device driver, template over the DUT type)
// and this file compile unchanged; in THIS executable the class
// Vzhao_terrain_bake_v2 IS the mutant. The same trick the forge cliff-RAM
// controls use (tests/CMakeLists.txt, "the mutants: each verilated under the
// golden's PREFIX").
//
// THIS TEST PASSES WHEN THE ORACLE COMPARISON FAILS. Inverse polarity,
// deliberately: it is evidence about the instrument, not about the design.
// CLAUDE.md, "Do not write a test that asserts the bug" -- the bug being
// asserted here lives in a committed mutant that is not in any production
// closure, and the CORRECT behaviour is asserted by `terrain_bake_v2_directed`
// against the real block. The two are separate targets on purpose.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_bake_v2.h"  // THE MUTANT, by prefix (see BUILD NOTE)

#include "bake_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_terrain.hpp"

namespace {

namespace zt = zref::terrain;

using MutantDut = Vzhao_terrain_bake_v2;

constexpr int32_t kM = 1 << 16;     // one world metre in fx16 raw
constexpr int32_t kSpan = 64 * kM;  // a 64 m patch: 2 m pitch over 32 cells

int g_failures = 0;

void check(bool ok, const char* what, uint64_t want = 0, uint64_t got = 0) {
  if (ok) return;
  std::fprintf(stderr, "FAIL: %s (want %llu, got %llu)\n", what,
               static_cast<unsigned long long>(want), static_cast<unsigned long long>(got));
  ++g_failures;
}

// ---------------------------------------------------------------------------
// The fixture: `meets` true on three BANDS of two lattice rows.
//
// NAMED AND EDITABLE (CLAUDE.md art-law rule 6 applied to a fixture): the band
// starts are the whole construction, and the expected row sets below are
// derived from them rather than written out by hand.
//   * a band at row 0 exercises the explicit StBrA/StBrB/StBrC fill, which the
//     mutation does not touch -- the control's only AGREEING breach;
//   * a band at row 5 sits after the first row advance, in the lag regime;
//   * a band at row 20 is a second, independent instance far down the scan.
// A band must not start at row 1 (the first-advance anomaly would mask it) and
// two bands must not be adjacent.
// ---------------------------------------------------------------------------
constexpr int kMeetsBands[] = {0, 5, 20};
constexpr int kBandRows = 2;  // a cell needs BOTH its rows, so a band is 2 deep

bool row_meets(int j) {
  for (const int b : kMeetsBands)
    if (j >= b && j < b + kBandRows) return true;
  return false;
}

// height16 is S 1.7.8 metres and the section 3.4 equality is
// `base + scar <= bottom`, so with bottom pinned at 0 a composed height of 0
// MEETS and a composed height of 100 raw units does NOT -- the same
// construction `test_breach_law` uses per corner, applied per row.
zref::render::TerrainPatch fixture_bands() {
  zref::render::TerrainPatch p = bdev::make_patch(0, 0, kSpan);
  p.bottom.assign(bdev::kVerts, 0);
  p.cell_state.assign(bdev::kCellCount, zt::kSolid);
  for (int j = 0; j < bdev::kLat; ++j) {
    for (int i = 0; i < bdev::kLat; ++i) {
      const size_t k = static_cast<size_t>(j) * bdev::kLat + i;
      p.heights[k] = 1000;
      p.scar[k] = static_cast<int16_t>(row_meets(j) ? -1000 : -900);
    }
  }
  return p;
}

/** The production window at cell row cj is (cj, cj+1). */
bool oracle_breaches_row(int cj) { return row_meets(cj) && row_meets(cj + 1); }

/**
 * The MUTANT's window, derived from the RTL rather than from its header:
 * row 0 is the explicit fill (0,1); row 1 is (1,1); row cj >= 2 lags to
 * (cj-1, cj), because the slide is `mrow_lo <= mrow_hi; mrow_hi <= mq` and the
 * prefetch issued during row cj-1 fetched row cj instead of row cj+1.
 */
bool mutant_breaches_row(int cj) {
  if (cj == 0) return row_meets(0) && row_meets(1);
  if (cj == 1) return row_meets(1);
  return row_meets(cj - 1) && row_meets(cj);
}

/** Cells of row `cj` whose substance disagrees with the reference. */
int row_disagreements(const bdev::BakeOut& got, const zref::render::TerrainPatch& ref, int cj) {
  int n = 0;
  for (int ci = 0; ci < bdev::kCells; ++ci) {
    const size_t k = static_cast<size_t>(cj) * bdev::kCells + ci;
    if ((got.cell_state[k] & zt::kSubstanceMask) !=
        (ref.cell_state[k] & zt::kSubstanceMask))
      ++n;
  }
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  MutantDut dut;  // the MUTANT, by prefix
  bdev::reset_dut(dut);

  const zref::render::TerrainPatch p = fixture_bands();

  // =========================================================================
  // FIXTURE A -- radius 0, so layer D is decided by the banded `meets` plane
  // alone and the expected row set is exact.
  // =========================================================================
  {
    bdev::StampRec st;
    st.radius = 0;  // decide layer D alone; no dig

    std::vector<zt::BreachEvent> ev;
    const zref::render::TerrainPatch ref = bdev::oracle_bake(p, st, &ev);
    const bdev::BakeOut got = bdev::run_bake(dut, p, st, 0);

    check(!got.timed_out, "fixture A: the mutant completes every handshake (the fault is not a hang)",
          0, got.timed_out ? 1 : 0);
    check(got.breach_ran, "fixture A: the breach phase ran", 1, got.breach_ran ? 1 : 0);

    // THE FIXTURE'S CONSTRUCTION IS A CLAIM; check it on the ORACLE before it
    // is used as a yardstick (the forge cliff-RAM control's habit).
    size_t want_oracle = 0;
    for (int cj = 0; cj < bdev::kCells; ++cj)
      if (oracle_breaches_row(cj)) want_oracle += bdev::kCells;
    check(ev.size() == want_oracle,
          "fixture A: the oracle breaches exactly one row per band (rows 0, 5, 20)",
          static_cast<uint64_t>(want_oracle), static_cast<uint64_t>(ev.size()));

    int total = 0;
    int bad_rows = 0;
    int want_total = 0;
    for (int cj = 0; cj < bdev::kCells; ++cj) {
      const int n = row_disagreements(got, ref, cj);
      total += n;
      const int want = (oracle_breaches_row(cj) != mutant_breaches_row(cj)) ? bdev::kCells : 0;
      want_total += want;
      if (n != want) {
        ++bad_rows;
        std::fprintf(stderr, "  row %2d: %d cells disagree, expected %d\n", cj, n, want);
      }
    }
    std::printf(
        "  fixture A: %d of %d layer-D cells disagree with apply_breach_law; "
        "%zu transitions emitted against the oracle's %zu\n",
        total, bdev::kCellCount, got.events.size(), ev.size());
    check(bad_rows == 0,
          "fixture A: the disagreement is EXACTLY the derived row set {1,5,6,20,21} -- row 0 "
          "agrees (the explicit fill), row 1 is the first advance, rows 6 and 21 are the lag",
          0, static_cast<uint64_t>(bad_rows));
    check(total == want_total, "fixture A: 5 wrong rows x 32 cells", static_cast<uint64_t>(want_total),
          static_cast<uint64_t>(total));
    check(total > 0, "fixture A: THE INSTRUMENT FIRED -- the oracle comparison sees the fault", 1,
          total > 0 ? 1 : 0);
    check(row_disagreements(got, ref, 0) == 0,
          "fixture A: ROW 0 AGREES -- the mutation is downstream of the explicit window fill, so "
          "a control that only looked at the first row would report the block healthy",
          0, static_cast<uint64_t>(row_disagreements(got, ref, 0)));

    // The mutant header's own claim, asserted rather than repeated: every
    // counter still balances against the stream it counts.
    check(got.breach_events == got.events.size(),
          "fixture A: breach_events_o balances the emitted transition stream -- the counter is "
          "CONSISTENT and WRONG, which is why a counter cannot see this",
          static_cast<uint64_t>(got.events.size()), got.breach_events);
    check(got.cell_order.size() == static_cast<size_t>(bdev::kCellCount),
          "fixture A: the cell scan is the same length as a correct one", bdev::kCellCount,
          static_cast<uint64_t>(got.cell_order.size()));
  }

  // =========================================================================
  // FIXTURE B -- the same patch with a REAL dig, to show what stays green.
  // The mutation is downstream of the whole DIG phase, so layer B, the
  // per-vertex flags and the dig counters are all bit-identical to the oracle
  // while layer D is wrong. This is the half that makes the fault dangerous.
  // =========================================================================
  {
    bdev::StampRec st;
    st.patch_id = 7;
    st.cx = 32 * kM;
    st.cz = 32 * kM;
    st.radius = 12 * kM;
    st.depth_from = 0;
    st.depth_to = 3 * kM;

    std::vector<zt::BreachEvent> ev;
    const zref::render::TerrainPatch ref = bdev::oracle_bake(p, st, &ev);
    const bdev::BakeOut got = bdev::run_bake(dut, p, st, 0);

    check(!got.timed_out, "fixture B: the mutant completes every handshake", 0,
          got.timed_out ? 1 : 0);

    int bad_scar = 0;
    for (int k = 0; k < bdev::kVerts; ++k)
      if (got.scar[static_cast<size_t>(k)] != ref.scar[static_cast<size_t>(k)]) ++bad_scar;
    check(bad_scar == 0,
          "fixture B: LAYER B IS BIT-IDENTICAL to the oracle -- every scar-plane test stays GREEN "
          "on this machine",
          0, static_cast<uint64_t>(bad_scar));

    int touched = 0;
    for (int k = 0; k < bdev::kVerts; ++k) touched += got.touched[static_cast<size_t>(k)] ? 1 : 0;
    check(got.texels_touched == static_cast<uint32_t>(touched),
          "fixture B: surface_texels_touched_o balances the touched stream", touched,
          got.texels_touched);
    check(got.breach_events == got.events.size(),
          "fixture B: breach_events_o balances the transition stream",
          static_cast<uint64_t>(got.events.size()), got.breach_events);

    int badc = 0;
    for (int k = 0; k < bdev::kCellCount; ++k)
      if ((got.cell_state[static_cast<size_t>(k)] & zt::kSubstanceMask) !=
          (ref.cell_state[static_cast<size_t>(k)] & zt::kSubstanceMask))
        ++badc;
    std::printf(
        "  fixture B: layer B matches the oracle on all %d vertices and every counter balances, "
        "while %d of %d layer-D cells are wrong\n",
        bdev::kVerts, badc, bdev::kCellCount);
    check(badc > 0,
          "fixture B: THE INSTRUMENT FIRED under a real dig too, with the dig phase clean", 1,
          badc > 0 ? 1 : 0);
  }

  if (g_failures == 0) {
    std::printf(
        "terrain_bake_v2_mutant_control: the meets-plane ROW WINDOW fault was CAUGHT by the "
        "apply_breach_law comparison (as it must be), and was invisible to layer B and to every "
        "counter\n");
  } else {
    std::fprintf(stderr,
                 "terrain_bake_v2_mutant_control: %d checks failed -- the mutant did NOT present "
                 "its documented fault, so it is not evidence about anything\n",
                 g_failures);
  }
  zhao::exit_hard(g_failures == 0 ? 0 : 1);
}
