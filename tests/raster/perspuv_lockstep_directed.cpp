// perspuv_lockstep_directed.cpp
//
// ---------------------------------------------------------------------------
// THE EMPIRICAL BACKSTOP FOR THE AXIS-LOCKSTEP PROOF
// ---------------------------------------------------------------------------
// `reports/PERSPUV-AXIS-LOCKSTEP-PROOF-20260908.md` proves by induction that
// `zhao_raster_perspuv_svc`'s two per-axis work queues cannot diverge: there are
// exactly four assignments to `wq_wp`/`wq_rp` in the file, the push guard has no
// `ax` dependence, and the pop guard `pk_v[ax] = (wq_wp[ax] != wq_rp[ax])` is
// equal across axes whenever the invariant holds.
//
// That proof is what licenses the paired rewrite to keep ONE scheduler. A proof
// that licenses a deletion is the comfortable kind, and this repository's own law
// says the comfortable explanation is the one to check hardest. So the invariant
// is also asserted every cycle against the real RTL here.
//
// WHY THIS IS NOT REDUNDANT WITH THE PROOF. The proof is about the source as
// written; this is about the source as ELABORATED, and it will keep being true
// only while nobody adds a fifth writer. If the rewrite lands and someone later
// reintroduces per-axis control, this test fails on the day it happens rather
// than at the next architecture review.
//
// The internals are reached with `--public-flat-rw` on this target only. No
// source edit is needed, which is why this could be written while
// `zhao_raster_perspuv_svc.sv` sat inside a running fit's closure.
#include "Vzhao_raster_perspuv_svc.h"
#include "Vzhao_raster_perspuv_svc___024root.h"

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vzhao_raster_perspuv_svc* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_raster_perspuv_svc* d = new Vzhao_raster_perspuv_svc;
  auto* R = d->rootp;

  d->clk = 0;
  d->rst_n = 0;
  d->v_valid_i = 0;
  d->r_ready_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  int wp_diverge = 0, rp_diverge = 0, pkv_diverge = 0, pki_diverge = 0;
  int cycles = 0, accepted = 0, retired = 0, depth_zeros = 0;
  int saw_empty = 0, saw_nonempty = 0;
  int probe_control = 0;  // see the control check below
  unsigned max_occ = 0;
  uint32_t rng = 0x51C0FFEEu;

  const int kFragments = 600;
  for (int cyc = 0; cyc < 40000 && retired < kFragments; ++cyc) {
    rng = rng * 1664525u + 1013904223u;

    // Backpressure on BOTH ends, uneven, so the queues fill and drain rather
    // than running at a fixed occupancy where a divergence could not appear.
    d->r_ready_i = ((rng >> 9) & 3u) != 0u;

    if (accepted < kFragments) {
      // DEPTH-ZERO FRAGMENTS ARE THE POINT OF THE MIX. A depth-zero fragment
      // takes a table slot and pushes NO work, which is exactly what makes the
      // table pointer and the queue pointer diverge from each other -- the case
      // a comment at zhao_raster_perspuv_svc.sv:470 says once cost one fragment
      // of 335. If any input can break the axis invariant, it is this one.
      const bool dz = ((rng >> 21) % 5u) == 0u;
      d->v_valid_i = 1;
      d->u_over_w_i = static_cast<int32_t>(0x00100000 + accepted * 7919);
      d->v_over_w_i = static_cast<int32_t>(0x00200000 - accepted * 6271);
      d->r_mant_i = 0x800000u + ((rng >> 3) & 0x7FFFFFu);
      d->r_k_i = static_cast<uint8_t>((rng >> 27) & 0x3Fu);
      d->depth_zero_i = dz ? 1 : 0;
      d->tag_i = static_cast<uint16_t>(accepted & 0xFFFF);
    } else {
      d->v_valid_i = 0;
    }
    d->eval();

    if (d->v_valid_i && d->v_ready_o) {
      ++accepted;
      if (d->depth_zero_i) ++depth_zeros;
    }
    if (d->r_valid_o && d->r_ready_i) ++retired;

    // ---- THE INVARIANT, EVERY CYCLE ---------------------------------------
    const unsigned wp0 = R->zhao_raster_perspuv_svc__DOT__wq_wp[0];
    const unsigned wp1 = R->zhao_raster_perspuv_svc__DOT__wq_wp[1];
    const unsigned rp0 = R->zhao_raster_perspuv_svc__DOT__wq_rp[0];
    const unsigned rp1 = R->zhao_raster_perspuv_svc__DOT__wq_rp[1];
    const unsigned pv0 = R->zhao_raster_perspuv_svc__DOT__pk_v[0];
    const unsigned pv1 = R->zhao_raster_perspuv_svc__DOT__pk_v[1];
    const unsigned pi0 = R->zhao_raster_perspuv_svc__DOT__pk_i[0];
    const unsigned pi1 = R->zhao_raster_perspuv_svc__DOT__pk_i[1];

    if (wp0 != wp1) ++wp_diverge;
    if (rp0 != rp1) ++rp_diverge;
    if (pv0 != pv1) ++pkv_diverge;
    if (pv0 && (pi0 != pi1)) ++pki_diverge;

    // Non-vacuity: the queues must actually be seen both empty and occupied.
    if (wp0 == rp0)
      ++saw_empty;
    else
      ++saw_nonempty;

    // THE PROBE CONTROL. Four counters that are all exactly zero is what a
    // dead probe looks like, and this repository treats a clean zero as a
    // broken instrument until shown otherwise. So one comparison is made
    // between two signals that genuinely DO differ -- a write pointer against
    // its own read pointer, which differ whenever the queue is occupied.
    // If this stays zero the reads are not live and the four zeros above mean
    // nothing.
    if (wp0 != rp0) ++probe_control;
    if (d->occupancy_o > max_occ) max_occ = d->occupancy_o;

    ++cycles;
    tick(d);
  }
  d->v_valid_i = 0;
  d->eval();

  std::printf("  %d cycles, accepted %d (depth-zero %d), retired %d, peak occupancy %u\n", cycles,
              accepted, depth_zeros, retired, max_occ);
  std::printf("  queue seen empty %d cycles, occupied %d cycles\n", saw_empty, saw_nonempty);
  std::printf("  divergences: wq_wp %d, wq_rp %d, pk_v %d, pk_i %d\n", wp_diverge, rp_diverge,
              pkv_diverge, pki_diverge);

  // ---- the workload actually exercised the interesting states -------------
  // Without these the four zeros below would be a broken instrument: a test
  // that never fills a queue reports no divergence in it.
  zhao::check(retired == kFragments, "the run completed", kFragments, retired);
  zhao::check(depth_zeros > 0,
              "the workload contains DEPTH-ZERO fragments -- the input that "
              "makes the table pointer and the queue pointer diverge from each "
              "other, and the one most likely to break an axis invariant",
              1, depth_zeros > 0 ? 1 : 0);
  zhao::check(saw_empty > 0 && saw_nonempty > 0,
              "the queues were seen both EMPTY and OCCUPIED -- a run at fixed "
              "occupancy could not expose a pop-side divergence at all",
              1, (saw_empty > 0 && saw_nonempty > 0) ? 1 : 0);
  zhao::check(max_occ > 1, "and the pipeline carried more than one fragment at a time", 1,
              max_occ > 1 ? 1 : 0);

  // ---- the invariant itself ------------------------------------------------
  zhao::check(probe_control > 0,
              "the internal probes are LIVE: comparing wq_wp[0] against its own "
              "wq_rp[0] -- two signals that genuinely differ -- reports "
              "differences. Without this, four zeros below would be equally "
              "consistent with a probe reading nothing at all",
              1, probe_control > 0 ? 1 : 0);

  zhao::check(wp_diverge == 0,
              "the two per-axis WRITE pointers never diverge -- the push guard "
              "has no ax dependence, so both increment or neither does",
              0, static_cast<uint64_t>(wp_diverge));
  zhao::check(rp_diverge == 0,
              "and the two READ pointers never diverge: the pop guard is "
              "pk_v[ax] = (wq_wp[ax] != wq_rp[ax]), which is equal across axes "
              "whenever the invariant already holds -- the induction step",
              0, static_cast<uint64_t>(rp_diverge));
  zhao::check(pkv_diverge == 0, "so the two schedulers' go/no-go agrees on every cycle", 0,
              static_cast<uint64_t>(pkv_diverge));
  zhao::check(pki_diverge == 0,
              "and they select the SAME TOKEN, not merely agree on whether to "
              "launch -- both queues store the same tail_q at the same index. "
              "This is what licenses one scheduler in the paired rewrite; the "
              "two arithmetic LANES stay two",
              0, static_cast<uint64_t>(pki_diverge));

  const int rc = zhao::report_and_exit("perspuv_lockstep_directed");
  delete d;
  zhao::exit_hard(rc);
}
