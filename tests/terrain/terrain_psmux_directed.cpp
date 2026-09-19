// terrain_psmux_directed.cpp -- does one streamer serve two clients without
// either of them seeing the other's page?
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS ACTUALLY GUARDING
// ---------------------------------------------------------------------------
// `zhao_terrain_psmux` exists to NOT spend a second `zhao_terrain_pagestream`
// (1,649 ALM, measured). What it buys instead is a failure mode that a second
// instance could not have: a vertex beat or a completion delivered to the
// WRONG CLIENT. That fault is silent by construction --
//
//   * a compose pass that receives the mip pass's beats composes a real
//     lattice from a real page at a real world position. Every handshake is
//     legal. `terrain_samples_evaluated_o` counts a full patch. The ground is
//     simply the wrong ground, and nothing anywhere says so.
//   * a completion delivered to the wrong client unpins a page that is still
//     being read, or reports a mip pass that never happened.
//
// So this file does not check that the block "works". It checks OWNERSHIP,
// beat by beat, with the two clients presenting DISTINGUISHABLE jobs and every
// delivered beat attributed. A test that fed both clients the same page could
// not see any of this.
//
// THE FAIRNESS CASE IS NOT DECORATION EITHER. Strict priority to the compose
// door would starve the mip pass, and the mip pass is what makes pages
// resident -- so the compose door would starve itself, and present as a
// machine that STOPS with every counter balanced. Case 4 holds both clients
// asking for the whole run and requires the grants to alternate.
//
// AND THE TWO STRAY COUNTERS ARE FIRED (case 5) rather than asserted zero.
// They watch for a beat arriving with no job outstanding, which correct
// upstream behaviour never produces -- but the streamer's ports are INPUTS to
// this module, so a bench owns them and the state is reachable with legal
// stimulus. No mutant is needed, and a counter never seen to move is a claim
// rather than evidence.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_psmux.h"

#include "zhao_sim.hpp"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

void check_eq(uint64_t got, uint64_t want, const char* what) {
  if (got != want) {
    std::printf("FAIL: %s -- expected %llu, got %llu\n", what,
                static_cast<unsigned long long>(want), static_cast<unsigned long long>(got));
    ++failures;
  }
}

// One page's worth of beats, short enough to keep the run quick and long
// enough that an interleave would be visible.
constexpr int kBeats = 6;

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_psmux top;

  auto idle = [&]() {
    top.a_j_valid_i = 0;
    top.b_j_valid_i = 0;
    top.a_v_ready_i = 1;
    top.b_v_ready_i = 1;
    top.a_done_ready_i = 1;
    top.b_done_ready_i = 1;
    top.p_j_ready_i = 1;
    top.p_v_valid_i = 0;
    top.p_done_valid_i = 0;
    top.a_j_slot_i = 0;
    top.b_j_slot_i = 0;
    top.a_j_gen_i = 0;
    top.b_j_gen_i = 0;
    top.a_j_epoch_i = 0;
    top.b_j_epoch_i = 0;
    top.a_j_src_id_i = 0;
    top.b_j_src_id_i = 0;
    top.a_j_flags_i = 0;
    top.b_j_flags_i = 0;
  };

  auto reset = [&]() {
    idle();
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    top.eval();
    zhao::tick(top);
    top.eval();
  };

  // Run one whole page for whichever client the share grants, and report which
  // client received the beats and the completion. The caller sets the `valid`
  // lines; this drives the streamer side.
  struct Served {
    int owner_at_grant = -1;  // 0 = A, 1 = B
    uint32_t slot = 0;
    uint32_t epoch = 0;
    int beats_a = 0;
    int beats_b = 0;
    int done_a = 0;
    int done_b = 0;
  };

  // A REAL CLIENT DROPS ITS `valid` WHEN THE JOB IS TAKEN, and the first
  // version of this harness did not -- so the share granted the same client
  // again the instant its page finished, `a_jobs_o` read 2 after one page, and
  // the contention case then found no grant to wait for. The block was right
  // and the model of the client was wrong. Both clients have ONE job
  // outstanding here, exactly as TERRAIN.SEQ and TERRAIN.MIPFEED do.
  auto serve_one = [&](Served& s, bool keep_a, bool keep_b) {
    int guard = 0;
    // SETTLE FIRST, THEN LOOK. Testing `p_j_valid_o` before an `eval()` reads
    // last cycle's combinational value, and the tick that follows then takes
    // the grant the loop was waiting to see -- so the wait timed out on a
    // share that was already serving. It cost one check in case 4 and nothing
    // else, which is precisely why it was worth chasing rather than widening
    // the guard.
    top.eval();
    while (!(top.p_j_valid_o && top.p_j_ready_i) && guard++ < 64) {
      zhao::tick(top);
      top.eval();
    }
    check(guard < 64, "a grant arrived");
    s.slot = top.p_j_slot_o;
    s.epoch = top.p_j_epoch_o;
    // THE GRANT LANDS ON THIS EDGE, so the withdrawal has to come AFTER it.
    // Clearing `valid` first removed the grant instead of ending it --
    // `p_j_valid_o` is combinational on the clients' valids, so there was
    // nothing left to take and every beat went nowhere. The order below is the
    // handshake's own: present, be taken, then withdraw.
    top.eval();
    zhao::tick(top);
    top.eval();
    top.a_j_valid_i = 0;
    top.b_j_valid_i = 0;
    top.eval();
    s.owner_at_grant = top.owner_o;

    // the beats
    for (int i = 0; i < kBeats; ++i) {
      top.p_v_valid_i = 1;
      top.eval();
      if (top.a_v_valid_o) ++s.beats_a;
      if (top.b_v_valid_o) ++s.beats_b;
      zhao::tick(top);
      top.eval();
    }
    top.p_v_valid_i = 0;

    // the completion
    top.p_done_valid_i = 1;
    top.eval();
    if (top.a_done_valid_o) ++s.done_a;
    if (top.b_done_valid_o) ++s.done_b;
    zhao::tick(top);
    top.eval();
    top.p_done_valid_i = 0;
    top.eval();
    zhao::tick(top);
    top.eval();
    // The clients present their NEXT jobs only AFTER the last edge of this
    // page, which is what makes case 4 a contention case rather than a
    // sequence of uncontended grants. Presenting them before that edge let the
    // share take the next grant here, so the caller's wait-for-a-grant loop
    // found nothing and timed out -- the block was serving; the harness was
    // looking for an event it had already caused.
    top.a_j_valid_i = keep_a ? 1 : 0;
    top.b_j_valid_i = keep_b ? 1 : 0;
    top.eval();
  };

  // ---- 1: one client alone takes the streamer immediately -----------------
  // The fair rule must cost nothing when there is no contention.
  {
    reset();
    top.a_j_valid_i = 1;
    top.a_j_slot_i = 0x11;
    top.a_j_epoch_i = 0xAAAA;
    top.a_j_flags_i = 0x8;
    top.eval();
    check(top.p_j_valid_o != 0, "case 1: an uncontended A is granted at once");
    check_eq(top.p_j_slot_o, 0x11, "case 1: A's slot reaches the streamer");
    check_eq(top.p_j_flags_o, 0x8, "case 1: A's flags reach the streamer");
    check(top.a_j_ready_o != 0, "case 1: A sees its own ready");
    check(top.b_j_ready_o == 0, "case 1: B does not");

    Served s;
    serve_one(s, false, false);
    check_eq(s.owner_at_grant, 0, "case 1: the captured owner is A");
    check_eq(s.beats_a, kBeats, "case 1: every beat reached A");
    check_eq(s.beats_b, 0, "case 1: no beat reached B");
    check_eq(s.done_a, 1, "case 1: the completion reached A");
    check_eq(s.done_b, 0, "case 1: and not B");
    check_eq(top.a_jobs_o, 1, "case 1: A's job counted");
    check_eq(top.b_jobs_o, 0, "case 1: B's not");
  }

  // ---- 2: B alone, the same ----------------------------------------------
  {
    reset();
    top.b_j_valid_i = 1;
    top.b_j_slot_i = 0x2C;
    top.b_j_epoch_i = 0xBBBB;
    top.eval();
    check(top.p_j_valid_o != 0, "case 2: an uncontended B is granted at once");
    check_eq(top.p_j_slot_o, 0x2C, "case 2: B's slot reaches the streamer");

    Served s;
    serve_one(s, false, false);
    check_eq(s.owner_at_grant, 1, "case 2: the captured owner is B");
    check_eq(s.beats_b, kBeats, "case 2: every beat reached B");
    check_eq(s.beats_a, 0, "case 2: no beat reached A");
    check_eq(s.done_b, 1, "case 2: the completion reached B");
    check_eq(s.done_a, 0, "case 2: and not A");
  }

  // ---- 3: no client may be granted while a job is in flight ---------------
  // The whole point of the share is that a page is served WHOLE. A second
  // grant mid-page is the interleave that produces one client's lattice under
  // the other's identity.
  {
    reset();
    top.a_j_valid_i = 1;
    top.a_j_slot_i = 0x30;
    top.eval();
    // take the grant
    zhao::tick(top);
    top.eval();
    top.b_j_valid_i = 1;
    top.b_j_slot_i = 0x31;
    top.eval();
    check(top.p_j_valid_o == 0, "case 3: no grant while busy");
    check(top.b_j_ready_o == 0, "case 3: B is not readied mid-page");
    check(top.busy_o != 0, "case 3: the share reports busy");

    int stray_grants = 0;
    for (int i = 0; i < kBeats; ++i) {
      top.p_v_valid_i = 1;
      top.eval();
      if (top.p_j_valid_o) ++stray_grants;
      if (top.b_v_valid_o) ++stray_grants;
      zhao::tick(top);
      top.eval();
    }
    top.p_v_valid_i = 0;
    check_eq(stray_grants, 0, "case 3: nothing leaked to B during A's page");
    top.a_j_valid_i = 0;
    top.b_j_valid_i = 0;
  }

  // ---- 4: both asking for the whole run -- the grants must ALTERNATE ------
  // This is the starvation case, and it is the one that would present as a
  // machine that stops rather than one that is slow.
  {
    reset();
    top.a_j_valid_i = 1;
    top.b_j_valid_i = 1;
    top.a_j_slot_i = 0x40;
    top.b_j_slot_i = 0x41;

    std::vector<int> order;
    for (int page = 0; page < 8; ++page) {
      Served s;
      serve_one(s, true, true);
      order.push_back(s.owner_at_grant);
      // the page went to exactly one client, whole
      check_eq(s.beats_a + s.beats_b, kBeats, "case 4: every beat delivered once");
      check_eq(s.owner_at_grant == 0 ? s.beats_b : s.beats_a, 0,
               "case 4: no beat reached the other client");
      check_eq(s.slot, s.owner_at_grant == 0 ? 0x40u : 0x41u,
               "case 4: the granted client's own slot went out");
    }
    top.a_j_valid_i = 0;
    top.b_j_valid_i = 0;

    int alternations = 0;
    for (size_t i = 1; i < order.size(); ++i)
      if (order[i] != order[i - 1]) ++alternations;
    check_eq(alternations, static_cast<uint64_t>(order.size() - 1),
             "case 4: the turn alternates on every page under full contention");
    check_eq(top.a_jobs_o, 4, "case 4: A got half the pages");
    check_eq(top.b_jobs_o, 4, "case 4: B got the other half");
  }

  // ---- 5: THE POSITIVE CONTROLS ------------------------------------------
  // `stray_v_o` and `stray_done_o` read zero in every case above, which makes
  // them a claim. Fire them: assert the streamer's outputs with no job
  // granted. This is legal stimulus at this module's port -- they are inputs
  // -- and it is the one place a beat that belongs to nobody can be seen.
  {
    reset();
    check_eq(top.stray_v_o, 0, "case 5: the stray counters start at zero");
    check_eq(top.stray_done_o, 0, "case 5: both of them");

    for (int i = 0; i < 3; ++i) {
      top.p_v_valid_i = 1;
      top.eval();
      check(top.a_v_valid_o == 0, "case 5: a stray beat reaches neither client");
      check(top.b_v_valid_o == 0, "case 5: neither of them");
      check(top.p_v_ready_o != 0, "case 5: and it is consumed rather than left to wedge");
      zhao::tick(top);
      top.eval();
    }
    top.p_v_valid_i = 0;

    for (int i = 0; i < 2; ++i) {
      top.p_done_valid_i = 1;
      top.eval();
      check(top.a_done_valid_o == 0, "case 5: a stray completion reaches neither client");
      check(top.b_done_valid_o == 0, "case 5: neither of them");
      zhao::tick(top);
      top.eval();
    }
    top.p_done_valid_i = 0;
    top.eval();
    zhao::tick(top);
    top.eval();

    check_eq(top.stray_v_o, 3, "case 5: stray_v_o FIRED, once per stray beat");
    check_eq(top.stray_done_o, 2, "case 5: stray_done_o FIRED, once per stray completion");
    check(top.busy_o == 0, "case 5: and the share never went busy on them");
  }

  // ---- 6: backpressure from a client is the streamer's backpressure -------
  // The share stores nothing, so a client that is not ready must stall the
  // streamer rather than have its beat dropped on the floor.
  {
    reset();
    top.b_j_valid_i = 1;
    top.b_j_slot_i = 0x60;
    top.eval();
    zhao::tick(top);
    top.eval();
    top.b_j_valid_i = 0;

    top.b_v_ready_i = 0;
    top.p_v_valid_i = 1;
    top.eval();
    check(top.p_v_ready_o == 0, "case 6: B's stall is passed to the streamer");
    check(top.b_v_valid_o != 0, "case 6: and the beat is still offered to B");
    zhao::tick(top);
    top.eval();
    top.b_v_ready_i = 1;
    top.eval();
    check(top.p_v_ready_o != 0, "case 6: releasing B releases the streamer");
    check_eq(top.stray_v_o, 0, "case 6: a stalled beat is not a stray beat");
  }

  if (failures == 0) {
    std::printf(
        "PASS terrain_psmux_directed -- one streamer, two clients, every beat "
        "attributed; the turn alternates under contention; both stray counters "
        "fired on demand.\n");
    return 0;
  }
  std::printf("FAILED terrain_psmux_directed -- %d check(s)\n", failures);
  return 1;
}
