// part_state_tick_boundary.cpp -- the child that arrives at the tick boundary.
//
// WHY THIS IS A SEPARATE SUITE. part_state_directed.cpp's check count (78) is
// quoted as evidence in the packet ledger and the run log, and
// part_state_capacity_backstop.cpp already set the precedent: a number that
// moves whenever somebody adds a case is a number nobody can use. So this gets
// its own file and the four existing counts do not move.
//
// ---------------------------------------------------------------------------
// WHAT IT IS FOR
// ---------------------------------------------------------------------------
// `reports/DEFECT-PART-STATE-LAST-CHILD-20260919.md`: PART.SPAWN emitted six
// children into the composed console, PART.STATE wrote five, and
// `children_refused_o`, `children_dropped_capacity_o` and
// `staging_stall_cycles_o` all read ZERO. A record left the machine and every
// counter balanced.
//
// The composed smoke bench shows it. It cannot LOCALISE it, and it cannot gate
// against it returning, because it exercises exactly one arrival time -- the
// one PART.SPAWN happens to produce. This file drives the arrival time itself.
//
// ---------------------------------------------------------------------------
// THE METHOD: SWEEP THE ARRIVAL, DO NOT GUESS IT
// ---------------------------------------------------------------------------
// The defect lives in a window two cycles wide. A single hand-placed offer is
// a guess about where that window is, and a guess that misses reads as a pass
// -- this tree's broken-instrument law in test form. So the child's offer
// CYCLE is swept across the whole tick, and the offer is HELD until it is
// taken, exactly as a real producer holds `chl_valid_o`.
//
// At some delay the offer lands inside the survivor pass (it is staged and
// drained normally). At some delay it lands on the edge the append phase
// decides staging is empty. At some delay it lands in `S_DONE`, and at some
// delay the tick has already ended. All four are visited by construction, and
// two of them are POSITIVE CONTROLS asserted below -- because a sweep that
// silently missed the interesting window would pass, and passing is the
// flattering direction.
//
// ---------------------------------------------------------------------------
// WHAT IS ASSERTED, AND WHY IT IS THE CORRECT BEHAVIOUR RATHER THAN THE BUG
// ---------------------------------------------------------------------------
// CLAUDE.md: "Do not write a test that asserts the bug ... assert the CORRECT
// behaviour and keep the detector's positive control separate." So nothing
// here asserts that a counter fires on a loss. What is asserted is the
// conservation law the block's counters imply and did not enforce:
//
//   * a child ACCEPTED at the handshake is written in the tick that accepted
//     it (or dropped at capacity and counted -- never neither);
//   * a child REFUSED at the tick boundary is written by the NEXT generation.
//     The refusal is the repaired behaviour: `chl_ready_o` is low once no
//     phase remains that could drain staging, the producer holds, and the
//     child belongs to the next tick. It is a stall, which the contract allows
//     twice over, and not a drop, which it forbids;
//   * no child is written TWICE -- a deferral that also wrote in the first
//     tick would be a duplicate wearing the costume of a repair;
//   * every refused OFFER moves `staging_stall_cycles_o`. The old condition
//     was `chl_full_c && st_q != S_IDLE`, structurally incapable of seeing a
//     boundary refusal; a repair that swapped a silent loss for a silent stall
//     would have kept the thing that made the defect expensive;
//   * accepted == written + dropped, summed over the whole sweep;
//   * survivors stay dense and in order, children strictly after, in every
//     tick of the sweep -- the ordering law must survive the repair.
//
// Parameters are PART.STATE's own bench parameters (CAPACITY = 8,
// SPECIES_N = 4, CHILD_D = 8): at the required tier of 32,768 none of this is
// reachable in a test at all.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_part_state.h"
#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"

using zhao::check;

namespace {

constexpr int kCapacity = 8;
constexpr int kSpecies = 4;
constexpr int kSurvivors = 4;  // leaves room in the tier, so a drop here would
                               // be a fault and not the capacity law
constexpr int kNextSurvivors = 2;
// The tick is roughly a dozen cycles at these parameters; the sweep runs well
// past its end so the S_IDLE refusal is visited too.
constexpr int kMaxDelay = 24;

struct Rec {
  uint64_t lo, hi;
  bool operator==(const Rec& o) const { return lo == o.lo && hi == o.hi; }
};

// Built with the RATIFIED particle128 codec (amendment C2 / ruling R3), never
// by hand-written shifts: the PART.STATE benches were already bitten once by a
// hand-rolled pack masking `age` to seven bits where the oracle says ten.
Rec pack_rec(int seed, int species) {
  zref::part::Particle128 p{};
  p.pos[0] = 1000 + seed * 3;
  p.pos[1] = -(400 + seed);
  p.pos[2] = 77 - seed;
  p.vel[0] = seed - 5;
  p.vel[1] = 3 - seed;
  p.vel[2] = seed;
  p.age = static_cast<uint16_t>((seed * 11) & 0x3FF);
  p.species = static_cast<uint8_t>(species & 0x7F);
  p.size = static_cast<uint8_t>(seed & 0x3F);
  p.spin = static_cast<uint8_t>((seed * 5) & 0x3F);
  p.flags = 0;
  p.variation = static_cast<uint8_t>(0x5A ^ seed);
  uint64_t lo = 0, hi = 0;
  zref::part::particle_pack(p, &lo, &hi);
  return Rec{lo, hi};
}

Rec survivor(int i) { return pack_rec(i, i % kSpecies); }

struct Dut {
  Vzhao_part_state* v;
  std::vector<Rec> written;

  explicit Dut(Vzhao_part_state* d) : v(d) {}

  template <typename W>
  void set_rec(W& dst, const Rec& r) {
    dst[0] = static_cast<uint32_t>(r.lo);
    dst[1] = static_cast<uint32_t>(r.lo >> 32);
    dst[2] = static_cast<uint32_t>(r.hi);
    dst[3] = static_cast<uint32_t>(r.hi >> 32);
  }

  // Captured on the HANDSHAKE, before the edge that transfers, which is what
  // makes the write ORDER observable rather than inferred.
  void collect() {
    if (v->wr_valid_o && v->wr_ready_i) {
      const uint64_t lo = static_cast<uint64_t>(v->wr_record_o[0]) |
                          (static_cast<uint64_t>(v->wr_record_o[1]) << 32);
      const uint64_t hi = static_cast<uint64_t>(v->wr_record_o[2]) |
                          (static_cast<uint64_t>(v->wr_record_o[3]) << 32);
      written.push_back(Rec{lo, hi});
    }
  }

  void tick() {
    v->clk = 0;
    v->eval();
    collect();
    v->clk = 1;
    v->eval();
  }

  void idle() {
    v->tick_start_i = 0;
    v->rd_valid_i = 0;
    v->rd_last_i = 0;
    v->prt_ready_i = 1;
    v->vrd_valid_i = 0;
    v->vrd_survive_i = 0;
    v->chl_valid_i = 0;
    v->chl_busy_i = 0;
    v->wr_ready_i = 1;
    v->eval();
  }

  void reset() {
    idle();
    v->rst_n = 0;
    tick();
    tick();
    v->rst_n = 1;
    tick();
    idle();
    written.clear();
  }
};

struct TickObs {
  bool accepted = false;
  int accept_cycle = -1;
  bool accepted_after_pass = false;  // the handshake fired after the LAST
                                     // verdict was taken, i.e. in S_APPEND or
                                     // later -- the window the defect lived in
  bool offered_while_busy = false;
  int refused_cycles = 0;
  uint32_t d_written = 0, d_dropped = 0, d_stall = 0, d_survivors = 0;
  std::vector<Rec> stream;
};

// Drive one whole tick. `child_taken` is carried BETWEEN ticks on purpose: a
// producer that has had its child accepted does not offer it again, and a
// producer that was refused holds the same record.
// `producer_busy`: the producer reports `chl_busy_i` HIGH for as long as it
// holds a child of this generation it has not had accepted -- which is what
// PART.SPAWN (not idle) or the fork branch feeding it asserts in the console.
TickObs run_tick(Dut& d, int n_surv, const Rec& child, int child_delay, bool offer_child,
                 bool& child_taken, bool producer_busy = false) {
  Vzhao_part_state& r = *d.v;
  TickObs o;
  const uint32_t w0 = r.children_written_o;
  const uint32_t p0 = r.children_dropped_capacity_o;
  const uint32_t s0 = r.staging_stall_cycles_o;
  const uint32_t v0 = r.survivors_o;
  d.written.clear();

  r.tick_start_i = 1;
  d.tick();
  r.tick_start_i = 0;

  int fed = 0, verdicts = 0, after_done = 0;
  bool done_seen = false;

  for (int c = 0; c < 400; ++c) {
    d.idle();

    // VALID DOES NOT CONSULT READY, here as in part_state_directed: a producer
    // that withdraws when refused can never be SEEN to be refused, and the
    // refusal is half of what this file exists to check.
    if (fed < n_surv) {
      r.rd_valid_i = 1;
      r.rd_last_i = (fed == n_surv - 1);
      d.set_rec(r.rd_record_i, survivor(fed));
    }
    if (r.prt_valid_o && verdicts < n_surv) {
      r.vrd_valid_i = 1;
      r.vrd_survive_i = 1;
      d.set_rec(r.vrd_record_i, survivor(verdicts));
    }
    const bool want_child = offer_child && !child_taken && (c >= child_delay);
    if (want_child) {
      r.chl_valid_i = 1;
      d.set_rec(r.chl_record_i, child);
    }
    // Busy from the start of the tick until the child is taken: the parent is
    // held BEFORE its child is ready, which is exactly the window the plain
    // sweep loses.
    if (producer_busy && offer_child && !child_taken) r.chl_busy_i = 1;

    r.eval();
    const bool rd_fire = r.rd_valid_i && r.rd_ready_o;
    const bool vr_fire = r.vrd_valid_i && r.vrd_ready_o;
    const bool ch_fire = r.chl_valid_i && r.chl_ready_o;
    if (want_child) {
      if (r.tick_busy_o) o.offered_while_busy = true;
      if (!ch_fire) ++o.refused_cycles;
    }
    if (ch_fire && !child_taken) {
      o.accepted = true;
      o.accept_cycle = c;
      o.accepted_after_pass = (verdicts >= n_surv);
      child_taken = true;
    }

    d.tick();
    if (rd_fire) ++fed;
    if (vr_fire) ++verdicts;
    if (r.tick_done_o) done_seen = true;
    // Three cycles past the done pulse, so an offer that lands in S_IDLE is
    // still made and still measured.
    if (done_seen && ++after_done >= 3) break;
  }

  o.stream = d.written;
  o.d_written = r.children_written_o - w0;
  o.d_dropped = r.children_dropped_capacity_o - p0;
  o.d_stall = r.staging_stall_cycles_o - s0;
  o.d_survivors = r.survivors_o - v0;
  return o;
}

int count_child(const TickObs& o, const Rec& child) {
  int n = 0;
  for (size_t i = 0; i < o.stream.size(); ++i)
    if (o.stream[i] == child) ++n;
  return n;
}

// The ordering law, per tick: n_surv survivors dense and in stream order, then
// every child written, and nothing else.
bool stream_ok(const TickObs& o, int n_surv, const Rec& child, int child_writes) {
  if (o.stream.size() != static_cast<size_t>(n_surv + child_writes)) return false;
  for (int i = 0; i < n_surv; ++i)
    if (!(o.stream[i] == survivor(i))) return false;
  for (int i = 0; i < child_writes; ++i)
    if (!(o.stream[n_surv + i] == child)) return false;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Verilated::traceEverOn(false);

  auto* top = new Vzhao_part_state;  // heap + exit_hard: see zhao_sim.hpp
  Dut d(top);

  int orphans = 0;            // accepted, then neither written nor dropped
  int deferred_lost = 0;      // refused at the boundary and never written after
  int duplicates = 0;         // written in both generations
  int silent_refusals = 0;    // offered and refused with no counter moving
  int stream_faults = 0;      // survivors or ordering disturbed
  int boundary_accepts = 0;   // accepted after the survivor pass ended
  int boundary_refusals = 0;  // offered while the tick was live and refused all tick
  int accepted_total = 0, written_total = 0, dropped_total = 0;

  int first_orphan_delay = -1;

  for (int delay = 0; delay <= kMaxDelay; ++delay) {
    // A fresh reset per delay, so one sweep point cannot explain another's
    // result through leftover state.
    d.reset();
    const Rec child = pack_rec(400 + delay, 1);

    bool taken = false;
    const TickObs t1 = run_tick(d, kSurvivors, child, delay, true, taken);
    const int w1 = count_child(t1, child);

    // The second generation exists to answer "deferred or destroyed?". If the
    // child was taken in tick 1 it is NOT offered again, and must not appear.
    const TickObs t2 = run_tick(d, kNextSurvivors, child, 0, !taken, taken);
    const int w2 = count_child(t2, child);

    accepted_total += (t1.accepted ? 1 : 0) + (t2.accepted ? 1 : 0);
    written_total += static_cast<int>(t1.d_written + t2.d_written);
    dropped_total += static_cast<int>(t1.d_dropped + t2.d_dropped);

    if (t1.accepted && w1 == 0 && t1.d_dropped == 0) {
      ++orphans;
      if (first_orphan_delay < 0) first_orphan_delay = delay;
    }
    if (!t1.accepted && w2 == 0 && t2.d_dropped == 0) ++deferred_lost;
    if (w1 + w2 > 1) ++duplicates;
    if (t1.refused_cycles > 0 && t1.d_stall == 0) ++silent_refusals;
    if (t1.accepted && t1.accepted_after_pass) ++boundary_accepts;
    if (!t1.accepted && t1.offered_while_busy) ++boundary_refusals;

    if (!stream_ok(t1, kSurvivors, child, w1)) ++stream_faults;
    if (!stream_ok(t2, kNextSurvivors, child, w2)) ++stream_faults;
    if (t1.d_survivors != static_cast<uint32_t>(kSurvivors)) ++stream_faults;
    if (t2.d_survivors != static_cast<uint32_t>(kNextSurvivors)) ++stream_faults;

    std::printf(
        "[part_state_tick_boundary] delay=%2d  t1{accept=%d@%2d after_pass=%d wrote=%d "
        "drop=%u stall=%u}  t2{accept=%d wrote=%d}\n",
        delay, t1.accepted ? 1 : 0, t1.accept_cycle, t1.accepted_after_pass ? 1 : 0, w1,
        t1.d_dropped, t1.d_stall, t2.accepted ? 1 : 0, w2);
  }

  // THE DEFECT ITSELF. On the unrepaired block this is non-zero: the child is
  // accepted at `chl_ready_o`, PART.SPAWN counts it emitted, and it is gone.
  check(orphans == 0,
        "a child ACCEPTED at the handshake is written in the tick that accepted it "
        "(0 = no record left the machine uncounted)",
        0, static_cast<uint64_t>(orphans));

  // The repaired behaviour, stated positively: a refusal is a deferral.
  check(deferred_lost == 0,
        "a child REFUSED at the tick boundary is written by the NEXT generation, not lost", 0,
        static_cast<uint64_t>(deferred_lost));

  check(duplicates == 0, "no child is written twice -- a deferral is not a re-issue", 0,
        static_cast<uint64_t>(duplicates));

  // A silent stall is a smaller version of the same disease.
  check(silent_refusals == 0,
        "every refused OFFER moved staging_stall_cycles_o -- no refusal is silent", 0,
        static_cast<uint64_t>(silent_refusals));

  check(accepted_total == written_total + dropped_total,
        "conservation across the sweep: accepted == written + dropped_capacity",
        static_cast<uint64_t>(written_total + dropped_total),
        static_cast<uint64_t>(accepted_total));

  check(stream_faults == 0,
        "the ordering law survives the repair: survivors dense and in order, children after", 0,
        static_cast<uint64_t>(stream_faults));

  // POSITIVE CONTROLS FOR THE SWEEP ITSELF. A sweep that never reached the
  // window would satisfy every check above while testing nothing, which is
  // this tree's "a gate that cannot reach the state is not evidence about the
  // state" in its cheapest form.
  check(boundary_accepts > 0,
        "the sweep actually REACHED the append-phase window (children accepted after the "
        "survivor pass ended)",
        1, static_cast<uint64_t>(boundary_accepts));
  check(boundary_refusals > 0,
        "the sweep actually REACHED the closed boundary (a child offered to a live tick and "
        "refused for all of it)",
        1, static_cast<uint64_t>(boundary_refusals));

  // ---- SEVERAL RECORDS IN FLIGHT BELOW THIS BLOCK ------------------------------
  // Added 2026-09-19 with `out_q`. Every case above returns a verdict while the
  // NEXT record is being offered, i.e. at most one record in flight below
  // PART.STATE -- the one depth at which the old exit ("a verdict taken after
  // the last hand-off") happened to be right. Here ALL records are handed off
  // first and the verdicts come back afterwards, one per cycle, the way a
  // deeper UPDATE -> ... -> COLLIDE pipeline returns them. The survivor pass
  // must take every one of them in THIS tick.
  {
    d.reset();
    Vzhao_part_state& r = *d.v;
    const int n = kSurvivors;
    const uint32_t v0 = r.survivors_o;
    r.tick_start_i = 1;
    d.tick();
    r.tick_start_i = 0;
    int fed = 0, handed = 0, judged = 0;
    bool done = false, done_before_all_judged = false;
    for (int c = 0; c < 200 && !done; ++c) {
      d.idle();
      if (fed < n) {
        r.rd_valid_i = 1;
        r.rd_last_i = (fed == n - 1);
        d.set_rec(r.rd_record_i, survivor(fed));
      }
      // Verdicts only once EVERY record has been handed off.
      if (handed == n && judged < n) {
        r.vrd_valid_i = 1;
        r.vrd_survive_i = 1;
        d.set_rec(r.vrd_record_i, survivor(judged));
      }
      r.eval();
      const bool rd_fire = r.rd_valid_i && r.rd_ready_o;
      const bool pr_fire = r.prt_valid_o && r.prt_ready_i;
      const bool vr_fire = r.vrd_valid_i && r.vrd_ready_o;
      d.tick();
      if (rd_fire) ++fed;
      if (pr_fire) ++handed;
      if (vr_fire) ++judged;
      if (r.tick_done_o) {
        done = true;
        if (judged < n) done_before_all_judged = true;
      }
    }
    check(!done_before_all_judged,
          "with every record in flight at once, the tick does not end before its last verdict",
          0, done_before_all_judged ? 1 : 0);
    check(judged == n && r.survivors_o - v0 == static_cast<uint32_t>(n),
          "and every verdict is taken, as a survivor, in the tick that issued it",
          static_cast<uint64_t>(n), static_cast<uint64_t>(r.survivors_o - v0));
  }

  // ---- OPTION (a): THE PRODUCER SAYS IT IS BUSY ------------------------------
  // Added 2026-09-19 with `chl_busy_i`. The same sweep, with the producer
  // reporting that it still holds a parent of this generation until its child
  // is accepted. The append phase must now WAIT, so every child lands in the
  // tick that made it -- including every delay the plain sweep above had to
  // defer. The plain sweep's checks are untouched: tied low, the port changes
  // nothing.
  int busy_not_in_tick1 = 0, busy_rescued = 0, busy_stream_faults = 0;
  for (int delay = 0; delay <= kMaxDelay; ++delay) {
    d.reset();
    const Rec child = pack_rec(800 + delay, 1);
    bool taken = false;
    const TickObs t1 = run_tick(d, kSurvivors, child, delay, true, taken, true);
    const int w1 = count_child(t1, child);
    bool taken2 = taken;
    const TickObs t2 = run_tick(d, kNextSurvivors, child, 0, !taken, taken2, true);
    const int w2 = count_child(t2, child);
    if (!(t1.accepted && w1 == 1 && w2 == 0)) ++busy_not_in_tick1;
    if (!stream_ok(t1, kSurvivors, child, w1)) ++busy_stream_faults;
    // Accepted after the survivor pass ended: only an open append phase can
    // take such a child, so this counts the window the busy producer held.
    if (t1.accepted && t1.accepted_after_pass) ++busy_rescued;
  }
  check(busy_not_in_tick1 == 0,
        "with the producer BUSY, every child is written in the tick whose parent made it", 0,
        static_cast<uint64_t>(busy_not_in_tick1));
  check(busy_stream_faults == 0,
        "and the ordering law still holds: survivors dense and in order, then the child", 0,
        static_cast<uint64_t>(busy_stream_faults));
  // POSITIVE CONTROL: the busy sweep really held the append phase open for a
  // child that arrived after the survivor pass -- the very window the plain
  // sweep's boundary_refusals shows being closed.
  check(busy_rescued > boundary_accepts,
        "the busy producer kept the append phase open past where the plain sweep closed it",
        static_cast<uint64_t>(boundary_accepts + 1), static_cast<uint64_t>(busy_rescued));

  std::printf(
      "[part_state_tick_boundary] busy sweep: not_in_tick1=%d accepted_after_pass=%d\n",
      busy_not_in_tick1, busy_rescued);

  std::printf(
      "[part_state_tick_boundary] delays=%d accepted=%d written=%d dropped=%d "
      "boundary_accepts=%d boundary_refusals=%d orphans=%d (first at delay %d)\n",
      kMaxDelay + 1, accepted_total, written_total, dropped_total, boundary_accepts,
      boundary_refusals, orphans, first_orphan_delay);
  zhao::exit_hard(zhao::report_and_exit("part_state_tick_boundary"));
}
