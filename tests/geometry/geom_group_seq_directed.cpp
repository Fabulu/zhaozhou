// geom_group_seq_directed.cpp -- the GEOMETRY CLIENT-A PRODUCER, directed.
//
// SELF-CONSISTENCY BENCH, SAID OUT LOUD. `zhao_geom_group_seq` has NO entry in
// the oracle: it is a sequencer, not arithmetic, and `tools/budget/
// refmodel_liveness.py` would report it unresolved if it were declared. So
// every expectation below is derived from the contracts and headers this block
// cites -- design/contracts/GEOM.WCACHE.md, zhao_geom_proj_lane.sv,
// zhao_project_service.sv -- and NOT from agreement with a ratified model.
// This bench can prove the block does what the prose says; it cannot prove the
// prose is right.
//
// The bench plays three parts the block talks to, because the block is the only
// thing under test:
//
//   * THE LANE  -- `open_gen_i` (the generation the open is about to install,
//     combinational) and `rider_payload_i` (the packed {arena,index} word). The
//     payload fed in is a RECOGNISABLE marker, deliberately NOT the natural
//     pack, so a block that built its own rider instead of forwarding the
//     lane's would show up on `a_payload_o` instead of passing silently.
//   * THE SERVICE -- `a_ready_i`, and the result port's landing `kLatency`
//     cycles after each accept. The latency is the whole point: the projector
//     core is deep, and a block that sealed on ACCEPTS would seal an arena
//     whose last vertices are still in flight.
//   * THE REPLAY CUSTOMER -- `grp_ready_i` and the release port.
//
// EVERY COUNTER IS ASSERTED AS A DELTA ACROSS ITS OWN CASE, never as a non-zero
// total: a total can be inherited from an earlier case and read as evidence
// about this one. `seal_early_o` is the one counter no legal stimulus can move
// -- it watches for a seal issued before the landings arrive, which the state
// machine cannot do -- so its evidence is the committed mutant
// tests/mutants/zhao_geom_group_seq_mutant.sv, built as
// geom_group_seq_seal_early_mutant with this same file at inverted polarity.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_geom_group_seq.h"
#include "zhao_sim.hpp"

using zhao::check;

namespace {

constexpr int kArenas = 4;
constexpr int kDepth = 8;
// The service's result comes back deep. Anything >= 2 separates "sealed on
// accepts" from "sealed on landings"; 6 makes the gap obvious in a trace.
constexpr int kLatency = 6;

// The marker the bench hands back as the lane's packed rider. It is NOT
// (arena << INDEX_W) | index, so a block that packed its own word would fail
// the a_payload_o check rather than agree with itself.
uint32_t lane_pack(uint32_t arena, uint32_t index) {
  return 0xA000u | (arena << 4) | (index & 0xFu);
}

struct Env {
  Vzhao_geom_group_seq* v;
  int cycle = 0;
  int opens = 0;
  // scheduled landings: {due cycle, arena}
  std::vector<std::pair<int, uint32_t>> pending;

  // observations
  int accepts = 0;         // client-A beats accepted
  int vertices_taken = 0;  // beats accepted on the VERTEX stream
  int seal_pulses = 0;
  int seal_while_short = 0;  // seals observed before every landing arrived
  int landed_total = 0;

  explicit Env(Vzhao_geom_group_seq* d) : v(d) {}

  void idle_inputs() {
    v->job_valid_i = 0;
    v->v_valid_i = 0;
    v->a_ready_i = 0;
    v->grp_ready_i = 0;
    v->rel_valid_i = 0;
    v->fill_landed_i = 0;
    v->fill_arena_i = 0;
  }

  // One cycle with every modelled peer live. Returns nothing; all observation
  // is accumulated in the members above.
  void step() {
    // --- the lane's two combinational replies, settled against whatever the
    // block is currently asking for. Two evals: the block drives the address,
    // the bench answers, the block's outputs settle on the answer.
    v->eval();
    v->rider_payload_i = lane_pack(v->rider_arena_o, v->rider_index_o);
    v->open_gen_i = 0x50 + opens;
    v->eval();

    // --- the result port: anything due this cycle lands now.
    v->fill_landed_i = 0;
    for (size_t i = 0; i < pending.size(); ++i) {
      if (pending[i].first == cycle) {
        v->fill_landed_i = 1;
        v->fill_arena_i = pending[i].second;
        pending.erase(pending.begin() + static_cast<long>(i));
        break;
      }
    }
    v->eval();
    v->rider_payload_i = lane_pack(v->rider_arena_o, v->rider_index_o);
    v->open_gen_i = 0x50 + opens;
    v->eval();

    // --- observe what this cycle is about to do, before the edge.
    const bool a_take = v->a_valid_o && v->a_ready_i;
    const uint32_t a_arena = v->rider_arena_o;
    const bool v_take = v->v_valid_i && v->v_ready_o;
    const bool opening = v->open_o;
    const bool sealing = v->seal_o;
    const bool landing = v->fill_landed_i;

    // The seal must never be issued while a landing is still owed. This is the
    // CORRECT behaviour asserted positively -- not the detector's silence.
    if (sealing && !pending.empty()) ++seal_while_short;

    // --- the edge
    v->clk = 0;
    v->eval();
    v->clk = 1;
    v->eval();
    ++cycle;

    if (a_take) {
      ++accepts;
      pending.push_back({cycle + kLatency, a_arena});
    }
    if (v_take) ++vertices_taken;
    if (opening) ++opens;
    if (sealing) ++seal_pulses;
    if (landing) ++landed_total;
  }

  void reset() {
    idle_inputs();
    v->rst_n = 0;
    step();
    step();
    v->rst_n = 1;
    idle_inputs();
    step();
  }

  // Offer a job until it is taken. Returns the cycle it was accepted.
  void offer_job(uint32_t count, uint32_t mask, uint32_t src_id) {
    v->job_valid_i = 1;
    v->job_count_i = count;
    v->job_view_mask_i = mask;
    v->job_src_id_i = src_id;
    for (int guard = 0; guard < 64; ++guard) {
      v->eval();
      const bool taken = v->job_valid_i && v->job_ready_o;
      step();
      if (taken) break;
    }
    v->job_valid_i = 0;
  }

  // Run the machine with the vertex stream and the projector live, until the
  // block has consumed `count` vertices or the guard expires.
  void feed_vertices(int count, int guard_cycles = 400) {
    const int target = vertices_taken + count;
    v->v_valid_i = 1;
    v->v_x_i = 0x00010000;
    v->v_y_i = 0x00020000;
    v->v_z_i = 0x00030000;
    v->a_ready_i = 1;
    for (int g = 0; g < guard_cycles && vertices_taken < target; ++g) {
      v->v_x_i = 0x00010000 + vertices_taken;
      step();
    }
    v->v_valid_i = 0;
    step();
  }

  // Let the machine drain, seal and offer its handles; collect them.
  struct Handle {
    uint32_t arena, gen, count, view, src;
  };
  std::vector<Handle> collect(int want, int guard_cycles = 400) {
    std::vector<Handle> out;
    v->a_ready_i = 1;
    v->grp_ready_i = 1;
    for (int g = 0; g < guard_cycles && static_cast<int>(out.size()) < want; ++g) {
      v->eval();
      if (v->grp_valid_o && v->grp_ready_i) {
        out.push_back(
            Handle{v->grp_arena_o, v->grp_gen_o, v->grp_count_o, v->grp_view_o, v->grp_src_id_o});
      }
      step();
    }
    v->grp_ready_i = 0;
    return out;
  }

  void settle(int n) {
    v->a_ready_i = 1;
    for (int i = 0; i < n; ++i) step();
  }

  void release(uint32_t arena) {
    v->rel_valid_i = 1;
    v->rel_arena_i = arena;
    step();
    v->rel_valid_i = 0;
    step();
  }
};

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Verilated::traceEverOn(false);

  // HEAP, NEVER DELETED, exit through report_and_exit -- the workaround
  // zhao_sim.hpp documents for the VlThreadPool exit-time deadlock.
  auto* top = new Vzhao_geom_group_seq;
  Vzhao_geom_group_seq& r = *top;
  Env e(top);
  e.reset();

#ifdef ZHAO_EXPECT_SEAL_EARLY_MUTANT
  // ---- INVERTED POLARITY: evidence about the INSTRUMENT, not the design ----
  //
  // Built against tests/mutants/zhao_geom_group_seq_mutant.sv, whose only
  // substantive change is that StSealWait advances without the drain
  // condition -- it seals on the ACCEPT count, the exact fault seal_early_o
  // exists to catch. This build PASSES when the counter FIRES. It must never
  // be run against production sources: there the machine cannot reach the
  // state, which is why the counter needs a mutant at all.
  e.offer_job(5, 0x1, 0x1234);
  e.feed_vertices(5);
  e.settle(60);
  check(r.seal_early_o == 1,
        "MUTANT CONTROL: seal_early_o must FIRE when the machine seals before "
        "the landings arrive -- if it stays 0 the detector is blind and its "
        "silence on production is worth nothing",
        1, r.seal_early_o);
  check(e.seal_while_short >= 1,
        "MUTANT CONTROL: the bench itself must observe a seal issued with "
        "landings still owed, independently of the counter",
        1, e.seal_while_short >= 1 ? 1 : 0);
  zhao::exit_hard(zhao::report_and_exit("geom_group_seq_seal_early_mutant"));
#else

  // =========================================================================
  // CASE 1 -- malformed jobs are CONSUMED and COUNTED, never held
  // =========================================================================
  {
    const uint32_t ref0 = r.jobs_refused_o;
    const uint32_t op0 = r.groups_opened_o;

    e.offer_job(0, 0x1, 0x0001);  // count == 0
    check(r.jobs_refused_o - ref0 == 1, "refuse: count == 0", 1, r.jobs_refused_o - ref0);

    e.offer_job(kDepth + 1, 0x1, 0x0002);  // count > DEPTH
    check(r.jobs_refused_o - ref0 == 2, "refuse: count > DEPTH", 2, r.jobs_refused_o - ref0);

    e.offer_job(4, 0x0, 0x0003);  // empty view mask
    check(r.jobs_refused_o - ref0 == 3, "refuse: empty view mask", 3, r.jobs_refused_o - ref0);

    // A refused job must not have touched an arena. If it did, the refusal is
    // a leak rather than a refusal.
    check(r.groups_opened_o - op0 == 0, "refuse: a refused job opens NO arena", 0,
          r.groups_opened_o - op0);
    // ... and the machine is still accepting jobs, i.e. it did not wedge.
    r.eval();
    check(r.job_ready_o == 1, "refuse: the job port is still open afterwards", 1, r.job_ready_o);
  }

  // =========================================================================
  // CASE 2 -- one view, five vertices: open, fill, LAND, seal, hand over
  // =========================================================================
  uint32_t case2_arena = 0;
  {
    const uint32_t op0 = r.groups_opened_o;
    const uint32_t sl0 = r.groups_sealed_o;
    const uint32_t vs0 = r.vertices_sent_o;
    const uint32_t ld0 = r.landings_o;
    const int seals0 = e.seal_pulses;
    const int short0 = e.seal_while_short;

    e.offer_job(5, 0x1, 0xBEEF);
    e.settle(2);
    check(r.groups_opened_o - op0 == 1, "one view opens exactly one arena", 1,
          r.groups_opened_o - op0);

    e.feed_vertices(5);
    check(r.vertices_sent_o - vs0 == 5, "five vertices, one view: five client-A accepts", 5,
          r.vertices_sent_o - vs0);

    // The seal must NOT have happened yet: the landings are still in flight.
    // This is the load-bearing check of the whole file.
    check(e.seal_pulses - seals0 == 0,
          "no seal while landings are still in flight -- a seal on the ACCEPT "
          "count would seal an arena whose last vertices are not yet written",
          0, e.seal_pulses - seals0);

    const auto handles = e.collect(1);
    check(handles.size() == 1, "one view hands over exactly one group", 1, handles.size());
    check(r.landings_o - ld0 == 5, "five landings counted", 5, r.landings_o - ld0);
    check(r.groups_sealed_o - sl0 == 1, "one seal", 1, r.groups_sealed_o - sl0);
    check(e.seal_while_short - short0 == 0, "the seal was issued only after every landing arrived",
          0, e.seal_while_short - short0);

    if (!handles.empty()) {
      case2_arena = handles[0].arena;
      check(handles[0].count == 5, "handle carries the job's count", 5, handles[0].count);
      check(handles[0].view == 0, "handle carries view 0 for mask 0b01", 0, handles[0].view);
      check(handles[0].src == 0xBEEF, "handle carries src_id unchanged", 0xBEEF, handles[0].src);
      // The generation the LANE offered in the open cycle, not one sampled a
      // cycle late. e.opens was 0 before this open, so 0x50 was offered.
      check(handles[0].gen == 0x50,
            "handle carries the generation offered IN the open cycle -- a "
            "generation sampled after the edge makes every later lookup miss "
            "with a perfect payload",
            0x50, handles[0].gen);
    }
    check(r.seal_early_o == 0, "no early seal on a legal single-view group", 0, r.seal_early_o);
  }

  // =========================================================================
  // CASE 3 -- both views: ONE vertex stream, TWO arenas, TWO projections
  //
  // "Counters see what pictures cannot": a block that re-consumed the vertex
  // stream per view would produce identical arena contents and cost GEOM.SKIN
  // twice. The check that catches it is 3 vertices taken against 6 sent.
  // =========================================================================
  {
    const uint32_t op0 = r.groups_opened_o;
    const uint32_t sl0 = r.groups_sealed_o;
    const uint32_t vs0 = r.vertices_sent_o;
    const uint32_t ld0 = r.landings_o;
    const int vt0 = e.vertices_taken;
    const int short0 = e.seal_while_short;

    e.offer_job(3, 0x3, 0xF00D);
    e.settle(4);
    check(r.groups_opened_o - op0 == 2, "a dual-view job opens TWO arenas", 2,
          r.groups_opened_o - op0);

    e.feed_vertices(3);
    check(e.vertices_taken - vt0 == 3,
          "the vertex stream is consumed ONCE -- skinning is view-independent "
          "and must not be paid for twice",
          3, e.vertices_taken - vt0);
    check(r.vertices_sent_o - vs0 == 6, "... and each vertex is projected TWICE, once per view", 6,
          r.vertices_sent_o - vs0);

    const auto handles = e.collect(2);
    check(handles.size() == 2, "a dual-view job hands over TWO groups", 2, handles.size());
    check(r.landings_o - ld0 == 6, "six landings counted", 6, r.landings_o - ld0);
    check(r.groups_sealed_o - sl0 == 2, "two seals", 2, r.groups_sealed_o - sl0);
    check(e.seal_while_short - short0 == 0, "neither seal was issued while a landing was owed", 0,
          e.seal_while_short - short0);

    if (handles.size() == 2) {
      check(handles[0].view == 0, "slot 0 is view 0", 0, handles[0].view);
      check(handles[1].view == 1, "slot 1 is view 1", 1, handles[1].view);
      check(handles[0].arena != handles[1].arena,
            "the two views hold DISTINCT arenas -- one arena for both would "
            "make view 1 overwrite view 0 slot for slot",
            1, handles[0].arena != handles[1].arena ? 1 : 0);
      check(handles[0].arena != case2_arena && handles[1].arena != case2_arena,
            "neither view reuses the arena case 2 still holds", 1,
            (handles[0].arena != case2_arena && handles[1].arena != case2_arena) ? 1 : 0);
      check(handles[0].count == 3 && handles[1].count == 3, "both handles carry the job's count", 3,
            handles[0].count);
      check(handles[0].src == 0xF00D && handles[1].src == 0xF00D,
            "both handles carry src_id unchanged", 0xF00D, handles[0].src);
    }
    check(r.seal_early_o == 0, "no early seal on a legal dual-view group", 0, r.seal_early_o);
  }

  // =========================================================================
  // CASE 4 -- the rider: the block states {arena,index}, it does NOT pack
  // =========================================================================
  {
    // Walk a fresh single-view group and watch the offered address and the
    // forwarded payload on every beat.
    e.offer_job(4, 0x1, 0x0055);
    e.settle(2);

    int seen = 0;
    bool index_ok = true;
    bool payload_ok = true;
    bool arena_stable = true;
    uint32_t first_arena = 0;
    r.v_valid_i = 1;
    r.a_ready_i = 1;
    for (int g = 0; g < 200 && seen < 4; ++g) {
      r.eval();
      r.rider_payload_i = lane_pack(r.rider_arena_o, r.rider_index_o);
      r.eval();
      if (r.a_valid_o && r.a_ready_i) {
        if (seen == 0) first_arena = r.rider_arena_o;
        if (r.rider_index_o != static_cast<uint32_t>(seen)) index_ok = false;
        if (r.a_payload_o != lane_pack(r.rider_arena_o, r.rider_index_o)) payload_ok = false;
        if (r.rider_arena_o != first_arena) arena_stable = false;
        ++seen;
      }
      e.step();
    }
    r.v_valid_i = 0;

    check(seen == 4, "four beats observed", 4, seen);
    check(index_ok,
          "the index the block offers the lane is the RUNNING VERTEX INDEX, "
          "0,1,2,3 -- an index that restarted or skipped puts a vertex one "
          "slot over and looks like nothing at all",
          1, index_ok ? 1 : 0);
    check(arena_stable, "every beat of one group names the same arena", 1, arena_stable ? 1 : 0);
    check(payload_ok,
          "a_payload_o is the LANE's word, forwarded -- a block that packed "
          "its own rider would agree with itself and disagree with the lane",
          1, payload_ok ? 1 : 0);

    e.collect(1);
  }

  // =========================================================================
  // CASE 5 -- arena exhaustion STALLS, it does not overwrite
  //
  // Four arenas; cases 2, 3 and 4 each still hold theirs and nothing has been
  // released, so every arena is held. A dual-view job must now WAIT.
  // =========================================================================
  {
    const uint32_t st0 = r.alloc_stall_cycles_o;
    const uint32_t op0 = r.groups_opened_o;

    e.offer_job(2, 0x3, 0x00AA);
    e.settle(8);
    check(r.alloc_stall_cycles_o - st0 > 0,
          "with every arena held, allocation STALLS and the stall is counted "
          "-- silently reusing a live arena is the failure this counter names",
          1, r.alloc_stall_cycles_o - st0 > 0 ? 1 : 0);
    check(r.groups_opened_o - op0 == 0, "... and nothing was opened while stalled", 0,
          r.groups_opened_o - op0);

    // Release two arenas and the same job completes.
    const uint32_t ru0 = r.rel_unheld_o;
    e.release(case2_arena);
    check(r.rel_unheld_o - ru0 == 0, "releasing a HELD arena is not a fault", 0,
          r.rel_unheld_o - ru0);
    e.settle(4);
    check(r.groups_opened_o - op0 == 1, "one release unblocks one slot", 1,
          r.groups_opened_o - op0);
  }

  // =========================================================================
  // CASE 6 -- releasing an arena nobody holds is a FAULT, and it is counted
  // =========================================================================
  {
    const uint32_t ru0 = r.rel_unheld_o;
    // case2_arena was released in case 5 and nothing re-opened it yet other
    // than the unblocked slot; pick an arena the encoding allows but that
    // ARENAS does not contain -- it can never be held.
    e.release(kArenas + 1);
    check(r.rel_unheld_o - ru0 == 1,
          "releasing an arena that is not held is counted, not ignored -- a "
          "double release would otherwise free a live arena silently",
          1, r.rel_unheld_o - ru0);
  }

  // =========================================================================
  // The detector's own claim, stated as what it is
  // =========================================================================
  check(r.seal_early_o == 0,
        "seal_early_o reads 0 across the whole suite. That is a CLAIM about "
        "unreachability, not evidence the detector works; the evidence is the "
        "committed mutant build geom_group_seq_seal_early_mutant",
        0, r.seal_early_o);

  zhao::exit_hard(zhao::report_and_exit("geom_group_seq_directed"));
#endif
}
