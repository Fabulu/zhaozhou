// geom_tidq_directed.cpp -- the triangle-identity queue between GEOM.VERTID
// and the shell door. Console entries I54 and I55.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS, AND IT IS NOT A TIDINESS ARGUMENT
// ---------------------------------------------------------------------------
// `zhao_console_core.sv` and `reports/HANDOVER-20260919.md` BOTH cited
// "`geom_tidq_directed` drives a real clock and passes" as the evidence that
// this block was sound while its composition was not. NO SUCH FILE EXISTED.
// `zhao_geom_tidq` had exactly one test -- `lint_geom_tidq` -- and the block's
// own header asserted that "both counters ARE REACHABLE WITH LEGAL STIMULUS
// ... so neither owes a committed mutant", which had never once been
// demonstrated.
//
// That is the campaign's false-PRESENCE shape: not "X does not exist" wrongly
// claimed, but a named instrument cited as having already passed. It is worse
// than a false absence, because a reader who greps the name finds the citation
// and stops.
//
// ---------------------------------------------------------------------------
// WHAT ACTUALLY DISCRIMINATES HERE
// ---------------------------------------------------------------------------
// A bench that pushes an id and pops it back would pass against the DEFECTIVE
// console this file was written to repair. The defect was never in the queue's
// data path; it was in WHEN THE DOOR POPS. So every check below is about
// ORDER, OCCUPANCY or the FRAME EDGE, and never about whether 18 bits survive
// a memory.
//
//  1. THE STARTUP SKEW, WHICH IS THE WHOLE DEFECT. In the composed console the
//     shell door fires three GEOM.SETUP stages after the fork, while
//     GEOM.VERTID needs `S_PUB` x3 plus `S_TD` before the arena hands back an
//     index -- so THE DOOR IS ALWAYS AHEAD. Ungated, the first beat popped an
//     empty queue and took `ID_POISON`, and every later beat then took its
//     PREDECESSOR'S id. Measured in the smoke as
//     `pushed=0 1 2 3 4 5 6 7 | popped=262143 0 1 2 3 4 5 6`.
//     Case 2 reproduces that ungated behaviour so the counter is seen to fire;
//     case 3 runs the SAME stimulus through the console's occupancy gate and
//     requires `popped[k] == pushed[k]`.
//
//  2. THE FLUSH MUST NOT STRAND AN OWED ENTRY -- THE DEADLOCK MODE. Every
//     entry in this queue is owed to a triangle that is ALREADY INSIDE
//     GEOM.SETUP. The old flush reset the pointers and the level to zero, so
//     with the door gated on occupancy those triangles would wait forever for
//     a push that can never come. Case 5 asserts the queue still DELIVERS one
//     entry per owed triangle after a flush, poisoned rather than absent. It
//     is the check that fails against the previous RTL, and it fails by the
//     queue going empty rather than by a wrong value -- which is why it is
//     written as "the gated door got its beats", not as "the id was poison".
//
//  3. A PUSH ON THE FLUSH CLOCK MUST SURVIVE. The flush used to be an
//     `else if`, so a push arriving on that edge was dropped. That edge is
//     exactly when the console pushes GEOM.VERTID's SEAL-ABORT entry -- the
//     triangle the seal threw away mid-resolution, which GEOM.SETUP still
//     delivers. Dropping it re-opens the same deadlock from the other side.
//     Case 6.
//
//  4. THE POISON WINDOW IS A WINDOW, NOT A LATCH. Case 7 is the negative
//     control for cases 5 and 6: an id pushed AFTER a flush must come out
//     NAMED. An implementation that poisoned everything forever after the
//     first seal would satisfy every other check in this file and would make
//     the console render nothing from frame two onward.
//
//  5. ALL THREE COUNTERS ARE FIRED BY LEGAL STIMULUS, which is what retires
//     the block header's claim. `underflow_o` in case 2, `overflow_o` in case
//     4, `unnamed_o` in cases 5 and 8. None owes a committed mutant, and this
//     file is the reason that sentence may now be quoted.
//
// A NOTE ON WHAT IS NOT ASSERTED. This file does not assert that the console's
// door gate exists -- that is `zhao_console_core`'s wiring and the console
// smoke's `tidqjoin` assertion. Here the gate is MODELLED, by `pop_if_ready`,
// so that the queue can be shown correct under the discipline the console
// applies to it. Modelling it is the point: the queue's contract is "if you
// only pop what I hold, I hand you the right thing in the right order", and a
// bench that popped freely could never state that contract.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_geom_tidq.h"
#include "zhao_sim.hpp"

using zhao::check;
using zhao::tick;

namespace {

// `ID_POISON` for ID_W = 18. The queue emits this, never zero, when it does
// not know the answer: zero is a perfectly legal TriangleDescriptor index and
// would silently aim a tile list at whatever triangle happens to be first.
constexpr uint32_t kPoison = 0x3FFFFu;
constexpr int kDepth = 8;   // the module's default DEPTH

void idle(Vzhao_geom_tidq& d) {
  d.push_i = 0;
  d.pop_i = 0;
  d.flush_i = 0;
  d.id_ok_i = 0;
  d.id_i = 0;
}

void hard_reset(Vzhao_geom_tidq& d) {
  d.rst_n = 0;
  idle(d);
  d.eval();
  for (int i = 0; i < 3; ++i) tick(d);
  d.rst_n = 1;
  d.eval();
  tick(d);
}

// One clock with whatever is currently driven, then back to idle.
void beat(Vzhao_geom_tidq& d) {
  d.eval();
  tick(d);
  idle(d);
  d.eval();
}

void push(Vzhao_geom_tidq& d, uint32_t id, bool ok = true) {
  idle(d);
  d.push_i = 1;
  d.id_ok_i = ok ? 1 : 0;
  d.id_i = id;
  beat(d);
}

// THE CONSOLE'S DOOR GATE, MODELLED. `zhao_console_core` ANDs
// `tidq_have_w = (level_o != 0)` into the join's valid AND into both upstream
// readys, so a door beat happens ONLY when the queue holds the entry it is
// about to consume. Returns true if the beat actually fired, and captures the
// id the door would have sampled -- `id_o` is combinational, so this is the
// same wire GEOM.ARENABIN reads on that clock, not a reconstruction.
bool pop_if_ready(Vzhao_geom_tidq& d, uint32_t* got) {
  idle(d);
  d.eval();
  if (d.level_o == 0) return false;   // the gate holds the door shut
  *got = d.id_o;
  d.pop_i = 1;
  beat(d);
  return true;
}

// The UNGATED door, which is what the console did until 2026-09-27. Pops
// unconditionally and takes whatever is on the port.
uint32_t pop_ungated(Vzhao_geom_tidq& d) {
  idle(d);
  d.eval();
  uint32_t got = d.id_o;
  d.pop_i = 1;
  beat(d);
  return got;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  auto* top = new Vzhao_geom_tidq;
  auto& d = *top;

  hard_reset(d);

  // =========================================================================
  // 1. OUT OF RESET
  // =========================================================================
  check(d.level_o == 0, "case1: the queue is empty out of reset", 0, d.level_o);
  check(d.id_o == kPoison,
        "case1: an empty queue names NOTHING, and says so as ID_POISON rather "
        "than as the legal index zero",
        kPoison, d.id_o);
  check(d.underflow_o == 0, "case1: underflow starts at zero", 0, d.underflow_o);
  check(d.overflow_o == 0, "case1: overflow starts at zero", 0, d.overflow_o);
  check(d.unnamed_o == 0, "case1: unnamed starts at zero", 0, d.unnamed_o);

  // =========================================================================
  // 2. THE DEFECT, REPRODUCED -- AN UNGATED DOOR THAT RUNS AHEAD OF THE IDS
  //
  // This is the console's behaviour before the repair, and it is here for two
  // reasons: it FIRES `underflow_o` with legal stimulus (retiring the block
  // header's unproven claim), and it shows the resulting id stream so that
  // case 3's stream can be compared against something other than an argument.
  //
  // NOTE WHAT IS ASSERTED. The bug itself is NOT asserted as correct -- the
  // check is on the COUNTER, which is the instrument, and on the poison value,
  // which is the block's own stated law. "popped[k] == pushed[k-1]" is
  // PRINTED, not required, because after the console's repair there is no skew
  // for this block to produce.
  // =========================================================================
  hard_reset(d);
  {
    uint32_t first = pop_ungated(d);   // the door beats before anything is pushed
    check(first == kPoison,
          "case2: an ungated door that beats before the first push takes "
          "ID_POISON, so the downstream range guard REFUSES rather than "
          "silently binning descriptor zero",
          kPoison, first);
    check(d.underflow_o == 1,
          "case2: `underflow_o` FIRED on legal stimulus -- the counter is an "
          "instrument, not an argument",
          1, d.underflow_o);

    // NOW THE CONSOLE'S STEADY STATE, IN THE CONSOLE'S ORDER: the door beats
    // FIRST and the push follows, because GEOM.SETUP's three stages deliver
    // triangle N while GEOM.VERTID is still resolving it. That ordering is the
    // entire defect, and reproducing it here -- rather than pushing first and
    // calling the result equivalent -- is what makes this a reproduction.
    std::vector<uint32_t> pushed, popped;
    popped.push_back(first);          // the beat already taken above
    for (uint32_t k = 0; k < 6; ++k) {
      push(d, 100 + k);
      pushed.push_back(100 + k);
      popped.push_back(pop_ungated(d));
    }
    std::printf("[geom_tidq_directed] ungated door: pushed=");
    for (uint32_t v : pushed) std::printf("%u ", v);
    std::printf("| popped=");
    for (uint32_t v : popped) std::printf("%u ", v);
    std::printf("\n");
    // THE MEASURED STREAM, REPRODUCED. The smoke read
    // `pushed=0 1 2 3 4 5 6 7 | popped=262143 0 1 2 3 4 5 6`; this reads
    // `popped[0] = ID_POISON` and `popped[k] == pushed[k-1]` thereafter. The
    // check is on the SHAPE of the ungated door's output, which is a statement
    // about the console arrangement that was repaired -- the block itself is
    // behaving exactly as specified throughout.
    for (size_t k = 1; k < popped.size(); ++k) {
      check(popped[k] == pushed[k - 1],
            "case2: an UNGATED door hands every triangle its PREDECESSOR'S "
            "index -- in range, decoding cleanly, and invisible to every "
            "range guard downstream. This is console entry I54's named "
            "failure and it is why the door is now gated",
            pushed[k - 1], popped[k]);
    }
  }

  // =========================================================================
  // 3. THE REPAIR: A DOOR GATED ON OCCUPANCY, WITH THE PUSH STREAM STARTING
  //    LATE. This is the regression test for the console defect.
  //
  // The door is offered a beat FIVE TIMES before the first id exists, which is
  // more skew than GEOM.SETUP's three stages can produce. The gate must refuse
  // all five, and then every id must come out in order, named, with no
  // underflow at all. A gate tuned to the measured skew would pass a three-
  // beat version of this and fail here; a handshake passes both.
  // =========================================================================
  hard_reset(d);
  {
    for (int i = 0; i < 5; ++i) {
      uint32_t got = 0;
      const bool fired = pop_if_ready(d, &got);
      check(!fired,
            "case3: the gated door REFUSES to fire while no identity has been "
            "pushed -- the triangle waits in GEOM.SETUP instead of crossing "
            "with somebody else's index",
            0, fired ? 1 : 0);
    }
    check(d.underflow_o == 0,
          "case3: a gated door never pops an empty queue, so `underflow_o` is "
          "structurally unreachable in the console -- that is WHY the smoke "
          "may assert it at zero",
          0, d.underflow_o);

    std::vector<uint32_t> pushed, popped;
    for (uint32_t k = 0; k < 6; ++k) {
      uint32_t id = 200 + k * 7;
      push(d, id);
      pushed.push_back(id);
      uint32_t got = 0;
      const bool fired = pop_if_ready(d, &got);
      check(fired, "case3: the door fires as soon as the identity exists", 1,
            fired ? 1 : 0);
      popped.push_back(got);
    }
    for (size_t k = 0; k < pushed.size(); ++k) {
      check(popped[k] == pushed[k],
            "case3: popped[k] == pushed[k] -- each triangle carries ITS OWN "
            "arena descriptor index, not its predecessor's",
            pushed[k], popped[k]);
    }
    check(d.underflow_o == 0 && d.unnamed_o == 0,
          "case3: no underflow and no nameless pop across the whole stream",
          0, d.underflow_o + d.unnamed_o);
  }

  // =========================================================================
  // 4. `overflow_o` FIRES ON LEGAL STIMULUS
  //
  // Hold the door shut and push DEPTH+1. The full guard must refuse the extra
  // entry AND count it, and the DEPTH entries already held must be undamaged --
  // a queue that overwrote its head on overflow would also "count" it.
  // =========================================================================
  hard_reset(d);
  {
    for (uint32_t k = 0; k < kDepth; ++k) push(d, 300 + k);
    check(d.level_o == kDepth, "case4: the queue is full at DEPTH", kDepth,
          d.level_o);
    push(d, 999);
    check(d.overflow_o == 1,
          "case4: `overflow_o` FIRED -- a push into a full queue is refused "
          "and counted",
          1, d.overflow_o);
    check(d.level_o == kDepth, "case4: a refused push does not grow the queue",
          kDepth, d.level_o);
    for (uint32_t k = 0; k < kDepth; ++k) {
      uint32_t got = 0;
      const bool fired = pop_if_ready(d, &got);
      check(fired, "case4: the held entries are still there", 1, fired ? 1 : 0);
      check(got == 300 + k,
            "case4: an overflow REFUSES the newcomer and does not overwrite "
            "what it already owes -- the head is intact and in order",
            300 + k, got);
    }
  }

  // =========================================================================
  // 5. THE DEADLOCK MODE: A FLUSH WITH ENTRIES STILL OWED
  //
  // THIS IS THE CASE THAT FAILS AGAINST THE PREVIOUS RTL, and it fails in the
  // shape that matters. Three ids are queued for three triangles that are
  // still inside GEOM.SETUP. The arena seals. The old flush zeroed `lvl_q`, so
  // `pop_if_ready` -- which is the console's gate -- would find the queue EMPTY
  // and never fire again: the three triangles wait forever and the geometry
  // front end stops.
  //
  // The requirement is therefore stated as DELIVERY, not as value: the door
  // must still get exactly three beats. That each of them reads ID_POISON is
  // the second requirement, and it is the honest answer -- those ids name a
  // frame the arena has stopped owning, so the downstream guard must refuse
  // them rather than bin them into the next frame's chunk region.
  // =========================================================================
  hard_reset(d);
  {
    for (uint32_t k = 0; k < 3; ++k) push(d, 400 + k);
    check(d.level_o == 3, "case5: three identities are owed", 3, d.level_o);

    const uint32_t unnamed_before = d.unnamed_o;
    idle(d);
    d.flush_i = 1;
    beat(d);

    check(d.level_o == 3,
          "case5: THE FLUSH DID NOT DISCARD THE OWED ENTRIES. Three triangles "
          "are still in GEOM.SETUP and the gated door will still ask three "
          "times; a queue that went empty here would stall the console "
          "forever",
          3, d.level_o);

    for (uint32_t k = 0; k < 3; ++k) {
      uint32_t got = 0;
      const bool fired = pop_if_ready(d, &got);
      check(fired,
            "case5: the door still gets its beat after the seal -- no deadlock",
            1, fired ? 1 : 0);
      check(got == kPoison,
            "case5: and the id is WITHDRAWN, not carried into the next frame: "
            "the arena's cursor has moved, so the honest answer is the one "
            "that makes the range guard fire",
            kPoison, got);
    }
    check(d.unnamed_o == unnamed_before + 3,
          "case5: `unnamed_o` FIRED once per withdrawn identity, so the "
          "shortfall downstream has a source attribution rather than being a "
          "reference that merely vanished",
          unnamed_before + 3, d.unnamed_o);
    check(d.level_o == 0, "case5: and the queue drained", 0, d.level_o);
    check(d.underflow_o == 0,
          "case5: the gated door never once popped empty across the seal", 0,
          d.underflow_o);
  }

  // =========================================================================
  // 6. A PUSH ON THE FLUSH CLOCK SURVIVES -- THE SEAL-ABORT ENTRY
  //
  // `zhao_console_core` drives `push_i` with
  // `vid_tri_id_retire || (pa_seal_fire && vid_busy_w)`. The second term is
  // GEOM.VERTID's abort: the seal threw away the triangle it was holding, so
  // no descriptor will ever retire for it -- but GEOM.SETUP took that same
  // triangle from the same fork and WILL deliver it to the door. Its entry
  // therefore arrives on the flush edge itself.
  //
  // While the flush was an `else if`, that push was dropped, the count went one
  // short, and the gated door deadlocked. Here: two entries owed, then a flush
  // AND the abort push on one clock, and the door must get THREE beats.
  // =========================================================================
  hard_reset(d);
  {
    for (uint32_t k = 0; k < 2; ++k) push(d, 500 + k);

    idle(d);
    d.flush_i = 1;
    d.push_i = 1;
    d.id_ok_i = 0;       // the abort names nothing, by construction
    d.id_i = 0x1AAAA;    // a legal-looking index, deliberately: if the ok bit
                         // were ignored this value would come out and be binned
    beat(d);

    check(d.level_o == 3,
          "case6: THE PUSH ON THE FLUSH CLOCK WAS KEPT. Flush, push and pop are "
          "three independent facts about one edge; an `else if` that lets the "
          "flush swallow the push loses the seal-abort triangle's entry and "
          "deadlocks the gated door",
          3, d.level_o);

    for (uint32_t k = 0; k < 3; ++k) {
      uint32_t got = 0;
      const bool fired = pop_if_ready(d, &got);
      check(fired, "case6: all three owed beats are delivered", 1,
            fired ? 1 : 0);
      check(got == kPoison,
            "case6: every one of them names nothing -- the two flushed ids "
            "because their frame is gone, the abort entry because GEOM.VERTID "
            "never resolved it. 0x1AAAA must NOT appear here",
            kPoison, got);
    }
    check(d.level_o == 0, "case6: and the queue drained", 0, d.level_o);
  }

  // =========================================================================
  // 7. THE NEGATIVE CONTROL FOR CASES 5 AND 6: THE POISON IS A WINDOW
  //
  // An implementation that simply latched "poisoned" on the first flush would
  // pass every check above and would make the console render nothing from the
  // second frame onward. So: flush with entries owed, drain them, then push a
  // FRESH id and require it to come out NAMED.
  //
  // And the sharper half -- a fresh id pushed while stale entries are STILL in
  // the queue must come out named too, once the stale ones ahead of it are
  // gone. That is what says the window is measured from the flush rather than
  // applied to everything the queue subsequently holds.
  // =========================================================================
  hard_reset(d);
  {
    for (uint32_t k = 0; k < 2; ++k) push(d, 600 + k);
    idle(d);
    d.flush_i = 1;
    beat(d);

    // The fresh id goes in BEHIND the two stale ones, while they are still
    // queued. This is the ordering the console produces on any frame whose
    // first triangles are admitted before the previous frame's tail has left
    // GEOM.SETUP.
    push(d, 0x2BEEF);
    check(d.level_o == 3, "case7: two stale entries and one fresh one", 3,
          d.level_o);

    for (uint32_t k = 0; k < 2; ++k) {
      uint32_t got = 0;
      const bool fired = pop_if_ready(d, &got);
      check(fired, "case7: the stale entries are delivered", 1, fired ? 1 : 0);
      check(got == kPoison, "case7: ...and they are withdrawn", kPoison, got);
    }
    uint32_t got = 0;
    const bool fresh_fired = pop_if_ready(d, &got);
    check(fresh_fired, "case7: the fresh entry is delivered", 1,
          fresh_fired ? 1 : 0);
    check(got == 0x2BEEF,
          "case7: THE POISON WINDOW CLOSED. An id pushed after the seal names "
          "its triangle normally -- a latched poison would pass every other "
          "check in this file and render an empty console from frame two",
          0x2BEEF, got);
    check(d.level_o == 0, "case7: and the queue drained", 0, d.level_o);
  }

  // =========================================================================
  // 8. THE ACCEPTANCE BIT: A REFUSED DESCRIPTOR NAMES NOTHING AND IS COUNTED
  //
  // `zhao_geom_vertid` retires a TriangleDescriptor the arena REFUSED -- no
  // frame sealed, or the frame already faulted -- and the triangle still goes
  // to GEOM.SETUP. The queue is pushed on the RETIRE beat and carries the
  // acceptance as a BIT, so the count stays 1:1 and the unnamed entry says so.
  // The interleave is deliberate: named, unnamed, named.
  // =========================================================================
  hard_reset(d);
  {
    const uint32_t unnamed_before = d.unnamed_o;
    push(d, 0x00111, true);
    push(d, 0x00222, false);   // retired, NOT accepted
    push(d, 0x00333, true);

    uint32_t got = 0;
    check(pop_if_ready(d, &got) && got == 0x00111,
          "case8: a named entry comes out named", 0x00111, got);
    check(pop_if_ready(d, &got) && got == kPoison,
          "case8: the REFUSED descriptor's entry names nothing -- and it is "
          "still an entry, so the stream stays 1:1 with the triangles and no "
          "later triangle inherits an index",
          kPoison, got);
    check(pop_if_ready(d, &got) && got == 0x00333,
          "case8: and the entry BEHIND it is unaffected -- this is the exact "
          "failure the retire-beat push exists to prevent",
          0x00333, got);
    check(d.unnamed_o == unnamed_before + 1,
          "case8: `unnamed_o` FIRED exactly once", unnamed_before + 1,
          d.unnamed_o);
  }

  // =========================================================================
  // EVERY COUNTER SEEN TO MOVE
  // =========================================================================
  {
    // Re-fire the two that the last reset cleared, so the summary below is a
    // statement about THIS run rather than about a register that happened to
    // survive.
    hard_reset(d);
    pop_ungated(d);                 // underflow
    push(d, 0x00ABC, false);        // the nameless entry goes in FIRST, while
                                    // there is still room -- pushing it after
                                    // the queue is full only fires overflow
                                    // again, which is how the first version of
                                    // this block read `unnamed=0` and said so.
    uint32_t got = 0;
    for (uint32_t k = 0; k < kDepth + 1; ++k) push(d, 700 + k);  // overflow
    for (uint32_t k = 0; k < kDepth; ++k) (void)pop_if_ready(d, &got);
    check(d.underflow_o > 0 && d.overflow_o > 0 && d.unnamed_o > 0,
          "all three counters were FIRED by legal stimulus in this run, so "
          "none owes a committed mutant -- the block header has asserted that "
          "since entry I54 and nothing had ever shown it",
          1,
          (d.underflow_o > 0 && d.overflow_o > 0 && d.unnamed_o > 0) ? 1 : 0);
    std::printf(
        "[geom_tidq_directed] counters fired: underflow=%u overflow=%u "
        "unnamed=%u\n",
        d.underflow_o, d.overflow_o, d.unnamed_o);
  }

  top->final();
  return zhao::report_and_exit("geom_tidq_directed");
}
