// terrain_jobissue_directed.cpp -- TERRAIN.JOBISSUE, the subpatch job issuer
// that `zhao_console_core.sv` entry I21 says does not exist.
//
// Contract: design/contracts/TERRAIN.JOBISSUE.md.
//
// WHAT THIS GUARDS, and every one of these is silent in a result-only test:
//
//   * A JOB CARRYING THE WRONG PATCH'S VIEW MASK projects terrain into the
//     wrong player's half of a Duo screen. Nothing downstream can tell: the
//     mask is consumed by `zhao_terrain_group_seq`'s slot fan-out, which will
//     happily open one arena instead of two. Case 3 runs two patches with
//     DIFFERENT masks back to back and requires patch B's sixteen jobs to carry
//     B's mask -- the whole reason this block exists rather than a wire.
//   * A RELEASE PULSED EARLY retires a patch the tessellator is still reading.
//     `zhao_terrain_compcache_front`'s own header records that a wrong reading
//     of this port already cost "a whole patch retired without one vertex being
//     read, with `patches_served_o` counting it as consumed". Cases 1 and 5
//     require the release to stay low through fifteen jobs and to wait for the
//     sequencer to go idle after the sixteenth.
//   * A RELEASE THAT IS A LEVEL retires several patches per edge. Case 1
//     measures its width: exactly one cycle.
//   * A DECISION CONSUMED BY ONE SINK ONLY loses the deviation store's history
//     writeback, so TERRAIN.LOD's hysteresis silently restarts every frame.
//     Case 4 holds each sink's `ready` low in turn and requires the decision to
//     stay put and to be offered to the other sink exactly once.
//
// R95: EVERY FAULT COUNTER IS FIRED ON PURPOSE AND SHOWN SILENT BESIDE IT.
// All five are reachable from this block's own boundary with legal stimulus,
// so no committed mutant is owed here -- and each fire is paired with a
// negative control in the same case, because a counter that fires on
// everything is as useless as one that fires on nothing.
//
//   ctx_refused_o       case 6   a fifth context against CTXD = 4
//   serve_no_ctx_o      case 7   a serve with an empty context queue
//   ctx_src_mismatch_o  case 8   a serve whose src_id is not the head's
//   patches_dropped_o   case 8   ... and the drop it causes, counted apart
//   lod_src_mismatch_o  case 9   a decision from a patch we did not arm for
//
//   issue_clocks_o         case 11  climbs in flight, flat at rest
//   decision_wait_clocks_o case 10  measures the producer's wait
//
// TWO CLOCK INSTRUMENTS, NOT FAULT FLAGS, and case 10 is why. `issue_clocks_o`
// and `decision_wait_clocks_o` measure the two sides of the join's wait. The
// block first carried an EVENT counter here called `stray_decision_o`, and
// THIS TEST FAILED IT ON A CLEAN PATCH: TERRAIN.LOD has the next decision
// ready before the cache serves the next patch, and a producer valid in the
// very cycle the patch ARMS is early by zero clocks. Both are ordinary
// backpressure. Narrowing the window until the counter went quiet would have
// been tuning the instrument to this test; the counter was replaced by the
// duration instead. Recorded here because the failing check is the evidence.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_jobissue.h"

#include "zhao_sim.hpp"

namespace {

int failures = 0;
int checks = 0;

void check_eq(uint64_t got, uint64_t want, const char* what) {
  ++checks;
  if (got != want) {
    std::printf("FAIL: %s -- expected 0x%llx, got 0x%llx\n", what,
                static_cast<unsigned long long>(want), static_cast<unsigned long long>(got));
    ++failures;
  }
}

void check_true(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

// One `lod_target` packet as TERRAIN.LOD emits it.
struct Dec {
  uint32_t ox, oz, level, nz, pz, nx, px;
  uint32_t morph;
  uint32_t surface, dual;
  uint32_t src_id, hold;
};

// One job as zhao_terrain_group_seq receives it, plus the two fields this
// block owns.
struct Job {
  uint32_t ox, oz, level, nz, pz, nx, px;
  uint32_t morph;
  uint32_t surface, dual;
  uint32_t src_id;
  uint32_t view_mask, sparse;
};

// One history record as zhao_terrain_devstore receives it.
struct Hist {
  uint32_t level, morph, hold;
};

// TERRAIN.LOD's own emit order: x fastest, and on a dual page each subpatch is
// immediately followed by its underside.
std::vector<Dec> patch_decisions(uint32_t src_id, bool dual) {
  std::vector<Dec> out;
  for (uint32_t n = 0; n < 16; ++n) {
    for (uint32_t s = 0; s < (dual ? 2u : 1u); ++s) {
      Dec d{};
      d.ox = (n & 3u) * 8u;
      d.oz = (n >> 2) * 8u;
      d.level = n & 3u;
      d.nz = (n + 1u) & 3u;
      d.pz = (n + 2u) & 3u;
      d.nx = (n + 3u) & 3u;
      d.px = n & 3u;
      d.morph = 0x1000u + n * 7u + s;
      d.surface = s;
      d.dual = dual ? 1u : 0u;
      d.src_id = src_id;
      d.hold = (n * 3u + s) & 0xFFu;
      out.push_back(d);
    }
  }
  return out;
}

class Rig {
 public:
  Vzhao_terrain_jobissue t;

  std::vector<Job> jobs;
  std::vector<Hist> hists;
  int releases = 0;
  int release_cycles = 0;   // total cycles serve_release_o was high
  int max_release_run = 0;  // the widest contiguous run of it

  // Sink readiness, so a case can stall either leg.
  bool job_ready = true;
  bool h_ready = true;

  void quiet() {
    t.ctx_valid_i = 0;
    t.ctx_src_id_i = 0;
    t.ctx_view_mask_i = 0;
    t.ctx_sparse_fill_i = 0;
    t.serve_valid_i = 0;
    t.serve_src_id_i = 0;
    t.lod_valid_i = 0;
    t.job_ready_i = 1;
    t.h_ready_i = 1;
  }

  void reset() {
    quiet();
    t.rst_n = 0;
    for (int i = 0; i < 3; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
  }

  // Advance one clock, recording everything the block emitted this cycle.
  // Sampling is COMBINATIONAL-BEFORE-EDGE, which is what a downstream block
  // sees: `t.eval()` has already been called by the caller.
  void step() {
    if (t.job_valid_o && t.job_ready_i) {
      Job j{};
      j.ox = t.job_ox_o;
      j.oz = t.job_oz_o;
      j.level = t.job_level_o;
      j.nz = t.job_lvl_nz_o;
      j.pz = t.job_lvl_pz_o;
      j.nx = t.job_lvl_nx_o;
      j.px = t.job_lvl_px_o;
      j.morph = t.job_morph_o;
      j.surface = t.job_surface_o;
      j.dual = t.job_dual_o;
      j.src_id = t.job_src_id_o;
      j.view_mask = t.job_view_mask_o;
      j.sparse = t.sparse_fill_o;
      jobs.push_back(j);
    }
    if (t.h_valid_o && t.h_ready_i) {
      Hist h{};
      h.level = t.h_level_o;
      h.morph = t.h_morph_o;
      h.hold = t.h_hold_o;
      hists.push_back(h);
    }
    zhao::tick(t);
    // serve_release_o is REGISTERED, so its value after the edge is this
    // cycle's output. Measure the run width here, not before the edge.
    if (t.serve_release_o) {
      ++release_cycles;
      ++run_;
      if (run_ > max_release_run) max_release_run = run_;
      if (run_ == 1) ++releases;
    } else {
      run_ = 0;
    }
    t.eval();
  }

  void idle(int n) {
    for (int i = 0; i < n; ++i) {
      t.job_ready_i = job_ready ? 1 : 0;
      t.h_ready_i = h_ready ? 1 : 0;
      t.eval();
      step();
    }
  }

  // Push a draw context. Returns whether it was accepted.
  bool push_ctx(uint32_t src_id, uint32_t mask, uint32_t sparse) {
    t.ctx_valid_i = 1;
    t.ctx_src_id_i = src_id;
    t.ctx_view_mask_i = mask;
    t.ctx_sparse_fill_i = sparse;
    t.job_ready_i = job_ready ? 1 : 0;
    t.h_ready_i = h_ready ? 1 : 0;
    t.eval();
    const bool took = t.ctx_ready_o != 0;
    step();
    t.ctx_valid_i = 0;
    t.eval();
    return took;
  }

  // Hold the compose cache's serve level up. The cache drops it on release.
  void serve(uint32_t src_id) {
    t.serve_valid_i = 1;
    t.serve_src_id_i = src_id;
    t.eval();
  }
  void unserve() {
    t.serve_valid_i = 0;
    t.eval();
  }

  // Offer one decision until it is consumed, or give up after `budget` cycles.
  // Returns the number of cycles it took; `budget` means it was never taken.
  int offer(const Dec& d, int budget = 64) {
    t.lod_valid_i = 1;
    t.lod_ox_i = d.ox;
    t.lod_oz_i = d.oz;
    t.lod_level_i = d.level;
    t.lod_lvl_nz_i = d.nz;
    t.lod_lvl_pz_i = d.pz;
    t.lod_lvl_nx_i = d.nx;
    t.lod_lvl_px_i = d.px;
    t.lod_morph_i = d.morph;
    t.lod_surface_i = d.surface;
    t.lod_dual_i = d.dual;
    t.lod_src_id_i = d.src_id;
    t.lod_hold_i = d.hold;
    for (int c = 0; c < budget; ++c) {
      t.job_ready_i = job_ready ? 1 : 0;
      t.h_ready_i = h_ready ? 1 : 0;
      t.eval();
      const bool taken = t.lod_ready_o != 0;
      step();
      if (taken) {
        t.lod_valid_i = 0;
        t.eval();
        return c + 1;
      }
    }
    t.lod_valid_i = 0;
    t.eval();
    return budget;
  }

 private:
  int run_ = 0;
};

// Drive one complete patch: context, serve, decisions, retirement.
// `sequencer_busy` models zhao_terrain_group_seq dropping `job_ready_o` while
// it fills and replays, so the block must WAIT rather than release on faith.
void run_patch(Rig& r, uint32_t src_id, uint32_t mask, uint32_t sparse, bool dual,
               int sequencer_busy) {
  r.push_ctx(src_id, mask, sparse);
  r.serve(src_id);
  const std::vector<Dec> decs = patch_decisions(src_id, dual);
  for (const Dec& d : decs) r.offer(d);
  if (sequencer_busy > 0) {
    r.job_ready = false;
    r.idle(sequencer_busy);
    r.job_ready = true;
  }
  r.idle(4);
  r.unserve();
  r.idle(2);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  // ========================================================================
  // CASE 1 -- ONE PATCH, SIXTEEN JOBS, ONE RELEASE, AND THE RELEASE IS LATE.
  // ========================================================================
  {
    Rig r;
    r.reset();
    r.push_ctx(0xA001u, 0x3u, 0u);
    r.serve(0xA001u);

    const std::vector<Dec> decs = patch_decisions(0xA001u, false);
    check_eq(decs.size(), 16u, "1 TERRAIN.LOD emits sixteen on a single page");

    for (size_t i = 0; i < decs.size(); ++i) {
      r.offer(decs[i]);
      if (i + 1 < decs.size()) {
        check_eq(r.releases, 0, "1 no release while jobs remain");
      }
    }
    // The sequencer is busy for a while after the last job.
    r.job_ready = false;
    r.idle(12);
    check_eq(r.releases, 0, "1 the release WAITS for the sequencer to go idle");
    check_eq(r.t.patches_retired_o, 0u, "1 ... and nothing is retired meanwhile");
    r.job_ready = true;
    r.idle(3);

    check_eq(r.releases, 1, "1 exactly one release");
    check_eq(r.max_release_run, 1, "1 the release is a PULSE, not a level");
    check_eq(r.t.patches_retired_o, 1u, "1 patches_retired_o");
    check_eq(r.jobs.size(), 16u, "1 sixteen jobs issued");
    check_eq(r.t.jobs_issued_o, 16u, "1 jobs_issued_o agrees with the port");
    check_eq(r.hists.size(), 16u, "1 sixteen history records written back");

    // Every field forwarded, field for field, and the two this block owns.
    bool fields_ok = true, mask_ok = true, hist_ok = true;
    for (size_t i = 0; i < decs.size(); ++i) {
      const Dec& d = decs[i];
      const Job& j = r.jobs[i];
      if (j.ox != d.ox || j.oz != d.oz || j.level != d.level || j.nz != d.nz ||
          j.pz != d.pz || j.nx != d.nx || j.px != d.px || j.morph != d.morph ||
          j.surface != d.surface || j.dual != d.dual || j.src_id != d.src_id) {
        fields_ok = false;
      }
      if (j.view_mask != 0x3u || j.sparse != 0u) mask_ok = false;
      const Hist& h = r.hists[i];
      if (h.level != d.level || h.morph != d.morph || h.hold != d.hold) hist_ok = false;
    }
    check_true(fields_ok, "1 all eleven decision fields forwarded unchanged");
    check_true(mask_ok, "1 view_mask and sparse_fill come from the patch's context");
    check_true(hist_ok, "1 the history rides back to the deviation store intact");

    // Every fault counter silent on a clean patch -- the negative control for
    // cases 6 to 10.
    check_eq(r.t.ctx_refused_o, 0u, "1 ctx_refused_o silent");
    check_eq(r.t.serve_no_ctx_o, 0u, "1 serve_no_ctx_o silent");
    check_eq(r.t.ctx_src_mismatch_o, 0u, "1 ctx_src_mismatch_o silent");
    check_eq(r.t.patches_dropped_o, 0u, "1 patches_dropped_o silent");
    check_eq(r.t.lod_src_mismatch_o, 0u, "1 lod_src_mismatch_o silent");
    check_true(r.t.decision_wait_clocks_o < 64u,
               "1 decision_wait_clocks_o stays small on a patch that is never starved");
  }

  // ========================================================================
  // CASE 2 -- A DUAL PAGE OWES THIRTY-TWO, AND THE COUNT IS LOD'S, NOT OURS.
  // ========================================================================
  {
    Rig r;
    r.reset();
    r.push_ctx(0xB002u, 0x1u, 1u);
    r.serve(0xB002u);
    const std::vector<Dec> decs = patch_decisions(0xB002u, true);
    check_eq(decs.size(), 32u, "2 a dual page emits thirty-two");

    for (size_t i = 0; i < 31; ++i) {
      r.offer(decs[i]);
      check_eq(r.releases, 0, "2 no release before the thirty-second");
    }
    r.offer(decs[31]);
    r.idle(3);
    check_eq(r.releases, 1, "2 released after the thirty-second");
    check_eq(r.jobs.size(), 32u, "2 thirty-two jobs");
    check_eq(r.t.jobs_issued_o, 32u, "2 jobs_issued_o");

    bool dual_ok = true, sparse_ok = true;
    int undersides = 0;
    for (const Job& j : r.jobs) {
      if (j.dual != 1u) dual_ok = false;
      if (j.sparse != 1u) sparse_ok = false;
      if (j.surface) ++undersides;
    }
    check_true(dual_ok, "2 job_dual_o set on every job");
    check_true(sparse_ok, "2 sparse_fill_o follows the context, not a constant");
    check_eq(undersides, 16, "2 sixteen of the thirty-two are the underside");
  }

  // ========================================================================
  // CASE 3 -- THE VIEW MASK RIDES **THE JOB**. Two patches, two masks.
  //           This is the case entry I21 is about.
  // ========================================================================
  {
    Rig r;
    r.reset();
    // Both contexts are queued BEFORE either is served, which is the shape the
    // compose door actually produces: the door runs ahead of the cache.
    check_true(r.push_ctx(0xC001u, 0x3u, 0u), "3 first context accepted");
    check_true(r.push_ctx(0xC002u, 0x2u, 1u), "3 second context accepted");

    r.serve(0xC001u);
    for (const Dec& d : patch_decisions(0xC001u, false)) r.offer(d);
    r.idle(3);
    r.unserve();
    r.idle(2);

    r.serve(0xC002u);
    for (const Dec& d : patch_decisions(0xC002u, false)) r.offer(d);
    r.idle(3);
    r.unserve();
    r.idle(2);

    check_eq(r.jobs.size(), 32u, "3 two patches, sixteen jobs each");
    check_eq(r.releases, 2, "3 two releases");

    bool a_ok = true, b_ok = true;
    for (size_t i = 0; i < 16; ++i) {
      if (r.jobs[i].view_mask != 0x3u || r.jobs[i].sparse != 0u) a_ok = false;
      if (r.jobs[i + 16].view_mask != 0x2u || r.jobs[i + 16].sparse != 1u) b_ok = false;
    }
    check_true(a_ok, "3 patch A's sixteen jobs carry A's mask 2'b11 and sparse 0");
    check_true(b_ok, "3 patch B's sixteen jobs carry B's mask 2'b10 and sparse 1");
    check_eq(r.t.ctx_src_mismatch_o, 0u, "3 the join is clean, so the detector is silent");
  }

  // ========================================================================
  // CASE 4 -- THE FORK. A decision is consumed only when BOTH sinks take it,
  //           and neither sink is offered it twice.
  // ========================================================================
  {
    Rig r;
    r.reset();
    r.push_ctx(0xD004u, 0x3u, 0u);
    r.serve(0xD004u);
    const std::vector<Dec> decs = patch_decisions(0xD004u, false);

    // (a) the history store stalls: the job goes, the decision does not.
    r.h_ready = false;
    int spent = r.offer(decs[0], 8);
    check_eq(spent, 8, "4a a decision is NOT consumed while the store stalls");
    check_eq(r.jobs.size(), 1u, "4a ... but the sequencer was offered it exactly once");
    check_eq(r.hists.size(), 0u, "4a ... and the store got nothing");
    r.h_ready = true;
    r.offer(decs[0]);
    check_eq(r.jobs.size(), 1u, "4a the sequencer is NOT offered it a second time");
    check_eq(r.hists.size(), 1u, "4a the store gets it once the stall clears");

    // (b) the sequencer stalls: the store takes it, the decision does not move.
    r.job_ready = false;
    spent = r.offer(decs[1], 8);
    check_eq(spent, 8, "4b a decision is NOT consumed while the sequencer stalls");
    check_eq(r.hists.size(), 2u, "4b ... but the store was offered it exactly once");
    check_eq(r.jobs.size(), 1u, "4b ... and no job went out");
    r.job_ready = true;
    r.offer(decs[1]);
    check_eq(r.hists.size(), 2u, "4b the store is NOT offered it a second time");
    check_eq(r.jobs.size(), 2u, "4b the job goes once the stall clears");
    check_eq(r.t.jobs_issued_o, 2u, "4b two decisions consumed in total");
  }

  // ========================================================================
  // CASE 5 -- THE RELEASE IS THE SEQUENCER'S IDLE EDGE, NOT A COUNT.
  // ========================================================================
  {
    Rig r;
    r.reset();
    run_patch(r, 0xE005u, 0x3u, 0u, false, /*sequencer_busy=*/40);
    check_eq(r.releases, 1, "5 released after a forty-cycle sequencer drain");
    check_eq(r.release_cycles, 1, "5 and only for one cycle");
    check_eq(r.t.patches_retired_o, 1u, "5 patches_retired_o");
  }

  // ========================================================================
  // CASE 6 -- `ctx_refused_o` FIRES, and is silent on the four that fit.
  // ========================================================================
  {
    Rig r;
    r.reset();
    for (uint32_t i = 0; i < 4; ++i) {
      check_true(r.push_ctx(0xF000u + i, 0x3u, 0u), "6 the first four contexts fit");
    }
    check_eq(r.t.ctx_refused_o, 0u, "6 NEGATIVE CONTROL: silent while they fit");
    check_true(!r.push_ctx(0xF004u, 0x3u, 0u), "6 the fifth is refused, not overwritten");
    check_eq(r.t.ctx_refused_o, 1u, "6 FIRED: ctx_refused_o");

    // And the refusal did not corrupt the queue: the first patch still works.
    r.serve(0xF000u);
    for (const Dec& d : patch_decisions(0xF000u, false)) r.offer(d);
    r.idle(3);
    check_eq(r.releases, 1, "6 the queue survived the refusal");
    check_eq(r.jobs.size(), 16u, "6 ... and issued the head patch's jobs");
    check_eq(r.t.ctx_src_mismatch_o, 0u, "6 ... with the right context");
  }

  // ========================================================================
  // CASE 7 -- `serve_no_ctx_o` FIRES, once per serve and not once per cycle.
  // ========================================================================
  {
    Rig r;
    r.reset();
    check_eq(r.t.serve_no_ctx_o, 0u, "7 NEGATIVE CONTROL: silent at rest");
    r.serve(0x7007u);
    r.idle(20);
    check_eq(r.t.serve_no_ctx_o, 1u, "7 FIRED: one event for a twenty-cycle level");
    check_eq(r.releases, 0, "7 a patch with no context is NOT retired");
    check_eq(r.t.patches_retired_o, 0u, "7 ... and nothing is counted home");
    check_eq(r.jobs.size(), 0u, "7 ... and no job is issued");
    r.unserve();
    r.idle(2);
    r.serve(0x7008u);
    r.idle(5);
    check_eq(r.t.serve_no_ctx_o, 2u, "7 a SECOND serve is a second event");
  }

  // ========================================================================
  // CASE 8 -- `ctx_src_mismatch_o` FIRES, the patch is DROPPED and counted
  //           apart, and the release still goes so nothing wedges.
  // ========================================================================
  {
    Rig r;
    r.reset();
    r.push_ctx(0x8001u, 0x3u, 0u);

    // NEGATIVE CONTROL first, in the same rig: a matching serve is silent.
    r.serve(0x8001u);
    for (const Dec& d : patch_decisions(0x8001u, false)) r.offer(d);
    r.idle(3);
    r.unserve();
    r.idle(2);
    check_eq(r.t.ctx_src_mismatch_o, 0u, "8 NEGATIVE CONTROL: a matching serve is silent");
    check_eq(r.t.patches_dropped_o, 0u, "8 NEGATIVE CONTROL: nothing dropped");
    const int jobs_before = static_cast<int>(r.jobs.size());

    // Now a context and a serve that disagree -- the skew the join exists to
    // see. The two operands reach this block through different ports.
    r.push_ctx(0x8002u, 0x1u, 0u);
    r.serve(0x8099u);
    r.idle(10);
    check_eq(r.t.ctx_src_mismatch_o, 1u, "8 FIRED: ctx_src_mismatch_o");
    check_eq(r.t.patches_dropped_o, 1u, "8 FIRED: patches_dropped_o, counted apart");
    check_eq(r.releases, 2, "8 the release still goes -- no silent wedge");
    check_eq(r.t.patches_retired_o, 1u, "8 a DROP is not a RETIREMENT");
    check_eq(static_cast<int>(r.jobs.size()), jobs_before,
             "8 NOT ONE JOB was issued with the wrong patch's mask");
    r.unserve();
    r.idle(2);

    // And the queue moved on, so the next patch is not mis-keyed for ever.
    r.push_ctx(0x8003u, 0x2u, 0u);
    r.serve(0x8003u);
    for (const Dec& d : patch_decisions(0x8003u, false)) r.offer(d);
    r.idle(3);
    check_eq(static_cast<int>(r.jobs.size()), jobs_before + 16,
             "8 the patch after the drop issues normally");
    check_eq(r.jobs[jobs_before].view_mask, 0x2u, "8 ... with its OWN mask");
    check_eq(r.t.ctx_src_mismatch_o, 1u, "8 ... and the detector goes quiet again");
  }

  // ========================================================================
  // CASE 9 -- `lod_src_mismatch_o` FIRES and is a REPORT, not a drop.
  // ========================================================================
  {
    Rig r;
    r.reset();
    r.push_ctx(0x9001u, 0x3u, 0u);
    r.serve(0x9001u);
    std::vector<Dec> decs = patch_decisions(0x9001u, false);
    // Three decisions carry a src_id from a patch we did not arm for.
    decs[2].src_id = 0x9999u;
    decs[5].src_id = 0x9999u;
    decs[9].src_id = 0x9999u;
    for (const Dec& d : decs) r.offer(d);
    r.idle(3);
    check_eq(r.t.lod_src_mismatch_o, 3u, "9 FIRED: once per offending decision");
    check_eq(r.jobs.size(), 16u, "9 every job still went out (a report, not a drop)");
    check_eq(r.releases, 1, "9 and the patch still retires");
    check_eq(r.jobs[2].src_id, 0x9999u, "9 the decision's own src_id is forwarded, not the patch's");
    check_eq(r.t.ctx_src_mismatch_o, 0u,
             "9 the two src detectors are INDEPENDENT -- the context one stayed silent");
  }

  // ========================================================================
  // CASE 10 -- `decision_wait_clocks_o` MEASURES the producer's wait, and it
  //            is a duration rather than a fault flag. See the note at the top
  //            of this file: the event counter this replaced fired on a clean
  //            patch, which is a counter that cannot be read.
  // ========================================================================
  {
    Rig r;
    r.reset();
    Dec d = patch_decisions(0xAA01u, false)[0];

    r.idle(10);
    check_eq(r.t.decision_wait_clocks_o, 0u, "10 zero with nothing offered");

    // A decision offered with no patch armed: never consumed, and the wait is
    // measured cycle by cycle.
    const int spent = r.offer(d, 12);
    check_eq(spent, 12, "10 a decision with no armed patch is never consumed");
    check_eq(r.t.decision_wait_clocks_o, 12u, "10 twelve clocks offered, twelve measured");
    check_eq(r.jobs.size(), 0u, "10 ... and no job went out");
    check_eq(r.t.patches_retired_o, 0u,
             "10 THE TELL: the wait climbs while patches_retired_o is FLAT");

    // Once a patch is armed the same decisions cost nothing more, which is
    // what makes the number readable.
    r.push_ctx(0xAA01u, 0x3u, 0u);
    r.serve(0xAA01u);
    const uint32_t before = r.t.decision_wait_clocks_o;
    for (const Dec& dd : patch_decisions(0xAA01u, false)) r.offer(dd);
    r.idle(4);
    check_true(r.t.decision_wait_clocks_o <= before + 2u,
               "10 an armed patch's sixteen decisions add at most the arming cycle");
    check_eq(r.releases, 1, "10 and the armed patch retired normally");
    check_eq(r.jobs.size(), 16u, "10 ... having issued all sixteen");
  }

  // ========================================================================
  // CASE 11 -- `issue_clocks_o` is the stall instrument: it climbs in flight
  //            and is FLAT at rest. No timeout is invented; this is what makes
  //            a wedge readable instead.
  // ========================================================================
  {
    Rig r;
    r.reset();
    r.idle(20);
    check_eq(r.t.issue_clocks_o, 0u, "11 flat while idle");
    check_eq(r.t.busy_o, 0u, "11 busy_o low with nothing queued");

    r.push_ctx(0xB011u, 0x3u, 0u);
    check_eq(r.t.busy_o, 1u, "11 busy_o high with a context queued");
    r.serve(0xB011u);
    r.idle(1);
    const uint32_t armed = r.t.issue_clocks_o;
    // A patch armed but starved of decisions: the counter climbs, which is the
    // wedge a timeout would have hidden behind a policy.
    r.idle(30);
    check_true(r.t.issue_clocks_o >= armed + 30u, "11 climbs while a patch is starved");
    check_eq(r.t.patches_retired_o, 0u, "11 ... with patches_retired_o FLAT: the tell");

    for (const Dec& d : patch_decisions(0xB011u, false)) r.offer(d);
    r.idle(3);
    const uint32_t after = r.t.issue_clocks_o;
    r.unserve();
    r.idle(25);
    check_eq(r.t.issue_clocks_o, after, "11 flat again once the patch retires");
    check_eq(r.t.busy_o, 0u, "11 busy_o low once the queue drains");
  }

  // ========================================================================
  // 12. THE 8 -> 2 VIEW-MASK NARROWING, AND ITS COUNTER FIRED
  //
  //     `zhao_terrain_pagestream` forwards T5's `view_mask:u8` whole (core
  //     entry I21), so this port sees eight bits and discards [7:2].  Entry
  //     I21 left the disposition of those bits to whoever composed the
  //     consumer and required the discard to be COUNTED.
  //
  //     BOTH DIRECTIONS ARE ASSERTED, because a counter that fired on every
  //     context would also "fire": a ratified two-bit mask must leave it
  //     FLAT, and a mask with a high bit must move it by exactly one.  And the
  //     job must still be ISSUED with the low two bits -- the bits are
  //     ignored, not refused, which is the decision and not an accident.
  //
  //     This counter is here and not in `zhao_console_core` for a stated
  //     reason: the console smoke fails the CRC of every terrain page it
  //     plays, so the compose door never opens in it and a counter placed
  //     there could not be fired by any legal stimulus.  It would have read
  //     zero for ever.
  // ========================================================================
  {
    Rig r;
    r.reset();

    // -- the negative control: a ratified mask moves nothing --------------
    r.push_ctx(0xC0DEu, 0x3u, 0u);
    r.serve(0xC0DEu);
    for (const Dec& d : patch_decisions(0xC0DEu, false)) r.offer(d);
    r.idle(4);
    check_eq(r.t.view_mask_high_o, 0u, "12 a ratified two-bit mask leaves the counter FLAT");
    const uint32_t jobs_a = static_cast<uint32_t>(r.jobs.size());
    check_true(jobs_a > 0, "12 the ratified patch issued");
    check_eq(r.jobs[jobs_a - 1].view_mask, 0x3u, "12 ... carrying both views");
    r.unserve();
    r.idle(4);

    // -- the positive control: a bit this console has no view for ---------
    // 0xF1 is views {0} plus four bits no ratified document gives a meaning.
    r.push_ctx(0xC0DFu, 0xF1u, 0u);
    check_eq(r.t.view_mask_high_o, 1u, "12 view_mask_high_o FIRES on an unratified view bit");
    r.serve(0xC0DFu);
    for (const Dec& d : patch_decisions(0xC0DFu, false)) r.offer(d);
    r.idle(4);
    const uint32_t jobs_b = static_cast<uint32_t>(r.jobs.size());
    check_true(jobs_b > jobs_a, "12 the patch is ISSUED anyway -- ignored, not refused");
    check_eq(r.jobs[jobs_b - 1].view_mask, 0x1u, "12 ... with the low two bits only");
    check_eq(r.t.view_mask_high_o, 1u, "12 the counter counts CONTEXTS, not jobs");
    r.unserve();
    r.idle(4);
  }

  if (failures == 0) {
    std::printf(
        "PASS terrain_jobissue_directed -- %d checks: the thirteen-field job, the view mask "
        "riding it, the 8 -> 2 narrowing with its counter fired AND shown flat, the "
        "both-sinks fork, the late release, six fault counters each FIRED "
        "and each shown silent beside it, and two clock instruments\n",
        checks);
    zhao::exit_hard(0);
  }
  std::printf("FAILED terrain_jobissue_directed -- %d of %d checks\n", failures, checks);
  zhao::exit_hard(1);
}
