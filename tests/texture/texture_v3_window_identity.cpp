// texture_v3_window_identity.cpp
//
// ---------------------------------------------------------------------------
// T2'S FIRST DELIVERABLE, BEFORE ANY RTL MOVES
// ---------------------------------------------------------------------------
// The master recovery handoff's T2 asks for "independent literal-owner/
// generation checks, THEN replace per-slot generation access", and T1 says to
// "preserve a tested identity-only comparison rather than one giant patch".
// This is that comparison, and it is deliberately pure C++: it compares two
// REPRESENTATIONS of the same live set, which is a mathematical claim and not a
// timing one. Nothing here needs a DUT, so it cannot be invalidated by whatever
// the owner RTL is doing this week.
//
// V3.1 6.1 proposes replacing a 64-entry table of live bits and generation
// bytes with one bounded interval:
//
//   distance = unsigned_14(t - retire_ticket);
//   live(t)  = distance < used;
//
// 6.2 fixes the encoding, and the permutation is the part most likely to be got
// wrong by hand:
//
//   slot = ticket[5:0];  generation = ticket[13:6];
//   public_owner = { ticket[5:0], ticket[13:6] }   -- SLOT IN THE HIGH BITS
//
// and initialises alloc = retire = 64, used = 0, "to match first-use generation
// one for slot zero".
//
// 6.2 also states the trap this file exists to make un-fallable-into: "Do not
// apply a numerical subtraction directly to public_owner. Its slot bits are in
// the high position. Decode it into internal ticket order first."
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../harness/zhao_sim.hpp"

namespace {

constexpr int      kSlots   = 64;
constexpr int      kTBits   = 14;
constexpr uint32_t kModulus = 1u << kTBits;   // 16384
constexpr uint32_t kMask    = kModulus - 1u;

// ---- 6.2's encoding, in one place ----------------------------------------
inline uint16_t slot_of(uint16_t ticket) { return ticket & 0x3F; }
inline uint16_t gen_of(uint16_t ticket)  { return (ticket >> 6) & 0xFF; }
inline uint16_t public_of(uint16_t ticket) {
  return static_cast<uint16_t>((slot_of(ticket) << 8) | gen_of(ticket));
}

// ---- the PROPOSED representation: one bounded interval --------------------
struct Window {
  uint16_t alloc = 64;
  uint16_t retire = 64;
  int      used = 0;

  bool live(uint16_t t) const {
    const uint32_t distance = (static_cast<uint32_t>(t) - retire) & kMask;
    return distance < static_cast<uint32_t>(used);
  }
  uint16_t admit() {
    const uint16_t t = alloc;
    alloc = static_cast<uint16_t>((alloc + 1) & kMask);
    ++used;
    return t;
  }
  void retire_oldest() {
    retire = static_cast<uint16_t>((retire + 1) & kMask);
    --used;
  }
};

// ---- the EXISTING representation: per-slot live bits and generation bytes --
struct Literal {
  bool    live_bit[kSlots] = {false};
  uint8_t gen_byte[kSlots] = {0};
  std::vector<uint16_t> order;   // allocation order, for oldest-first retire

  void admit(uint16_t ticket) {
    const uint16_t s = slot_of(ticket);
    ++gen_byte[s];               // a slot's generation increments on REUSE
    live_bit[s] = true;
    order.push_back(ticket);
  }
  void retire_oldest() {
    live_bit[slot_of(order.front())] = false;
    order.erase(order.begin());
  }
  bool live(uint16_t t) const {
    for (uint16_t o : order) {
      if (o == t) return true;
    }
    return false;
  }
};

uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }

}  // namespace

int main() {
  // ---- the encoding itself -------------------------------------------------
  {
    Window w;
    const uint16_t first = w.admit();
    zhao::check(slot_of(first) == 0,
                "6.2: initialising alloc/retire to 64 makes the first token "
                "slot zero", 0, slot_of(first));
    zhao::check(gen_of(first) == 1,
                "and generation ONE, not zero -- to match first-use generation "
                "one for slot zero", 1, gen_of(first));
  }
  {
    // Slot 0 must not come back until 64 allocations later, and with its
    // generation incremented by exactly one.
    Window w;
    const uint16_t t0 = w.admit();
    for (int i = 0; i < kSlots - 1; ++i) (void)w.admit();
    const uint16_t t64 = w.admit();
    zhao::check(slot_of(t64) == slot_of(t0),
                "slot 0 is reused after exactly 64 allocations", slot_of(t0),
                slot_of(t64));
    zhao::check(gen_of(t64) == gen_of(t0) + 1,
                "and its generation has advanced by one",
                static_cast<uint64_t>(gen_of(t0) + 1), gen_of(t64));
  }

  // ---- 6.1's empty/full claim does NOT hold in this encoding ---------------
  // 6.1 says: "Both empty and full must be represented explicitly by used.
  // Pointer equality alone cannot distinguish them."
  //
  // That is the classic same-width-pointer FIFO caution, and this test was
  // first written to confirm it. It FAILED, and the document is what is wrong
  // here -- not by much, but in a way worth pinning down, because it is offered
  // as the reason a field exists.
  //
  // The ticket space is 14 bits (16,384) while capacity is 64. So
  // (alloc - retire) mod 16384 EQUALS used for every reachable state, alloc ==
  // retire happens only at used == 0, and full sits 64 apart. Pointer equality
  // distinguishes empty from full perfectly well; `used` is DERIVABLE.
  //
  // `used` is still right to keep -- but for 6.4's reason, not 6.1's: "Use
  // pre-edge state for permission: admit_allowed = ... && (used_q < 64)". That
  // wants a REGISTERED count, not a subtraction evaluated on the admission
  // path. A correct field with a wrong justification is worth catching, because
  // the justification is what the next person reasons from.
  {
    Window empty;                       // used = 0
    Window full;
    for (int i = 0; i < kSlots; ++i) (void)full.admit();   // used = 64
    zhao::check(full.used == kSlots, "the full window holds 64 owners", kSlots,
                full.used);
    zhao::check(empty.alloc == empty.retire,
                "the empty window has equal pointers", 1,
                (empty.alloc == empty.retire) ? 1 : 0);
    zhao::check(full.alloc != full.retire,
                "but the FULL window does not -- so, contrary to 6.1, pointer "
                "equality DOES distinguish empty from full at this ticket "
                "width, and `used` is justified by 6.4's pre-edge permission "
                "rather than by ambiguity",
                1, (full.alloc != full.retire) ? 1 : 0);
    const uint32_t span = (static_cast<uint32_t>(full.alloc) - full.retire) & kMask;
    zhao::check(span == static_cast<uint32_t>(full.used),
                "and the pointer span equals used exactly, which is what makes "
                "it derivable", static_cast<uint64_t>(full.used), span);
    zhao::check(!empty.live(empty.retire),
                "the empty window reports its own retire ticket dead", 0,
                empty.live(empty.retire) ? 1 : 0);
    zhao::check(full.live(full.retire),
                "the full window reports that same ticket live", 1,
                full.live(full.retire) ? 1 : 0);
  }

  // ---- 6.2's named trap: subtracting on public_owner ------------------------
  // The permutation puts slot in the HIGH bits, so distance arithmetic on the
  // public token is meaningless. The counterexample is SEARCHED FOR rather than
  // asserted, so this check fails loudly if somebody "simplifies" the decode.
  {
    Window w;
    for (int i = 0; i < 40; ++i) (void)w.admit();     // used = 40
    int disagreements = 0;
    for (uint32_t t = 0; t < kModulus; ++t) {
      const uint16_t tt = static_cast<uint16_t>(t);
      const bool correct = w.live(tt);
      const uint32_t pub_dist =
          (static_cast<uint32_t>(public_of(tt)) - public_of(w.retire)) & kMask;
      const bool naive = pub_dist < static_cast<uint32_t>(w.used);
      if (correct != naive) ++disagreements;
    }
    zhao::check(disagreements > 0,
                "6.2's trap is real: subtracting directly on public_owner "
                "disagrees with the decoded window on at least one token, so "
                "the decode is not optional",
                1, disagreements > 0 ? 1 : 0);
  }

  // ---- the identity, through many namespace wraps ---------------------------
  // 6.2: "The included model compares these two representations through many
  // namespace wraps." Membership is compared over the WHOLE 14-bit token space
  // periodically, not only over live tokens -- a window that reported spurious
  // live for DEAD tokens would otherwise pass every check here.
  {
    Window w;
    Literal L;
    uint32_t rng = 0xC0FFEEu;
    int  full_sweeps = 0;
    long token_checks = 0;
    int  membership_mismatches = 0;
    int  gen_mismatches = 0;
    int  admits = 0;
    int  retires = 0;

    for (int step = 0; step < 60000; ++step) {
      const uint32_t r = lcg(rng) >> 16;
      const bool want_admit = ((r & 1u) != 0u) || (w.used == 0);
      if (want_admit && w.used < kSlots) {
        const uint16_t t = w.admit();
        L.admit(t);
        ++admits;
        // The literal table's own generation byte must equal the ticket's.
        if (L.gen_byte[slot_of(t)] != gen_of(t)) ++gen_mismatches;
      } else if (w.used > 0) {
        w.retire_oldest();
        L.retire_oldest();
        ++retires;
      }

      if ((step % 500) == 0) {
        for (uint32_t t = 0; t < kModulus; ++t) {
          const uint16_t tt = static_cast<uint16_t>(t);
          if (w.live(tt) != L.live(tt)) ++membership_mismatches;
          ++token_checks;
        }
        ++full_sweeps;
      }
    }

    zhao::check(admits > 20000, "the sequence actually allocated (not vacuous)",
                1, admits > 20000 ? 1 : 0);
    zhao::check(retires > 20000, "and actually retired", 1,
                retires > 20000 ? 1 : 0);
    zhao::check(admits > static_cast<int>(kModulus),
                "and ran past a full namespace wrap, which is the case 6.6 "
                "says the arithmetic alone cannot handle",
                1, admits > static_cast<int>(kModulus) ? 1 : 0);
    zhao::check(full_sweeps >= 100,
                "membership was swept over the whole token space many times", 1,
                full_sweeps >= 100 ? 1 : 0);
    zhao::check(token_checks > 1000000,
                "over a million token membership comparisons", 1,
                token_checks > 1000000 ? 1 : 0);
    zhao::check(membership_mismatches == 0,
                "the window interval and the literal 64-entry live/generation "
                "table agree on EVERY token, through many namespace wraps",
                0, membership_mismatches);
    zhao::check(gen_mismatches == 0,
                "and the per-slot generation byte always equals ticket[13:6]", 0,
                gen_mismatches);
  }

  // ---- 6.6: where the namespace fence must sit ------------------------------
  // "The first partial interval starting at ticket 64 reaches this fence after
  // 16,320 allocations; later complete intervals span 16,384." Counted here
  // rather than quoted, because a number carried across a document boundary is
  // exactly the kind that goes stale silently.
  {
    Window w;
    int allocations = 0;
    for (;;) {
      const uint16_t t = w.admit();
      ++allocations;
      w.retire_oldest();
      if (((t + 1u) & kMask) == 0u) break;   // alloc_ticket would wrap to zero
    }
    zhao::check(allocations == 16320,
                "6.6: the first interval starting at ticket 64 reaches the "
                "namespace-wrap fence after exactly 16,320 allocations",
                16320, allocations);
  }

  // ---- 6.3: the ordered-retirement invariant is load-bearing ----------------
  // "allocation never leaves a hole in the live interval; retirement is
  // strictly oldest-first." 6.3 warns: "Do not quietly assume away holes."
  // If a hole could occur, the interval representation is simply WRONG -- and a
  // test that only ever retires in order would never reveal it. So punch one.
  {
    Window w;
    Literal L;
    for (int i = 0; i < 8; ++i) {
      const uint16_t t = w.admit();
      L.admit(t);
    }

    const uint16_t interior = L.order[4];
    // The literal table can express the hole; the interval cannot.
    L.live_bit[slot_of(interior)] = false;
    L.order.erase(L.order.begin() + 4);

    const bool diverged = w.live(interior) && !L.live(interior);
    zhao::check(diverged,
                "6.3: retiring an INTERIOR owner makes the interval and the "
                "literal table disagree -- so oldest-first retirement is a "
                "precondition of the replacement, not a stylistic preference",
                1, diverged ? 1 : 0);
  }

  // ---- 6.4: the SNAPSHOT RACE, which is a rule and not an accident ----------
  // T2 asks to "prove full token order, membership and snapshot races". Order
  // and membership are above. This is the third, and 6.4 states it as a
  // deliberate conservative choice rather than a consequence:
  //
  //   "Use pre-edge state for permission ... Admission and retirement may occur
  //    together when pre-edge used is below 64. At pre-edge used == 64,
  //    INITIALLY REFUSE ADMISSION even if an output retires in that same cycle.
  //    That conservative rule avoids a combinational downstream ready bypass
  //    and same-slot read/write complications."
  //
  // So the interesting case is exactly the boundary: full, and retiring this
  // cycle. A post-edge reading would admit; the rule says do not. Both are
  // modelled here so the difference is visible rather than asserted.
  {
    Window w;
    for (int i = 0; i < kSlots; ++i) (void)w.admit();   // pre-edge used == 64

    const bool pre_edge_permits  = (w.used < kSlots);
    const bool post_edge_permits = ((w.used - 1) < kSlots);   // if a retire lands
    zhao::check(!pre_edge_permits,
                "6.4: at pre-edge used == 64 admission is refused", 0,
                pre_edge_permits ? 1 : 0);
    zhao::check(post_edge_permits,
                "and a POST-edge reading would have permitted it -- so the two "
                "readings genuinely differ at the boundary and the rule is a "
                "real choice, not a restatement",
                1, post_edge_permits ? 1 : 0);

    // Simultaneous admit+retire BELOW the boundary is legal and must leave the
    // window consistent: used unchanged, both pointers advanced by one.
    Window x;
    Literal M;
    for (int i = 0; i < 10; ++i) { const uint16_t t = x.admit(); M.admit(t); }
    const int      used_before   = x.used;
    const uint16_t alloc_before  = x.alloc;
    const uint16_t retire_before = x.retire;

    const uint16_t t = x.admit();   // same cycle, both events
    M.admit(t);
    x.retire_oldest();
    M.retire_oldest();

    zhao::check(x.used == used_before,
                "6.4: a simultaneous admit and retire below the boundary leaves "
                "used unchanged", static_cast<uint64_t>(used_before), x.used);
    zhao::check(x.alloc == static_cast<uint16_t>((alloc_before + 1) & kMask),
                "with alloc advanced by one",
                static_cast<uint16_t>((alloc_before + 1) & kMask), x.alloc);
    zhao::check(x.retire == static_cast<uint16_t>((retire_before + 1) & kMask),
                "and retire advanced by one",
                static_cast<uint16_t>((retire_before + 1) & kMask), x.retire);

    int mismatches = 0;
    for (uint32_t q = 0; q < kModulus; ++q) {
      const uint16_t qq = static_cast<uint16_t>(q);
      if (x.live(qq) != M.live(qq)) ++mismatches;
    }
    zhao::check(mismatches == 0,
                "and the two representations still agree on every token after "
                "the simultaneous update", 0, mismatches);

    // The retired token must be dead and the freshly admitted one live -- the
    // race would show up as either being wrong.
    zhao::check(x.live(t), "the token admitted in the racing cycle is live", 1,
                x.live(t) ? 1 : 0);
    zhao::check(!x.live(retire_before),
                "and the one retired in that same cycle is dead", 0,
                x.live(retire_before) ? 1 : 0);
  }

  // ---- 22.1's EXPLICIT CASE LIST -------------------------------------------
  // 22.1 does not leave the identity model's coverage to judgement; it names
  // the cases: "Test zero, one, 63 and 64 live owners, intervals crossing
  // numeric zero, and same-slot handles from earlier and later generations",
  // at "every head position and every legal occupancy".
  //
  // Zero, 64 and the wrap were already covered above. One, 63, every head
  // position, and the LATER-generation handle were not -- checked against the
  // list rather than assumed, the same audit that found the gap in 16.3's list.
  {
    int occ_mismatches = 0;
    int head_positions = 0;

    // EVERY head position, and the occupancies 22.1 names. `retire` is walked
    // across a full slot ring AND across the generation boundary, so intervals
    // that cross numeric zero are included rather than hoped for.
    for (uint32_t base = 0; base < kModulus; base += 251u) {   // 66 positions, coprime stride
      for (int used : {0, 1, 63, 64}) {
        Window w;
        w.retire = static_cast<uint16_t>(base);
        w.alloc  = static_cast<uint16_t>((base + used) & kMask);
        w.used   = used;
        ++head_positions;

        // Membership must be exactly the interval [retire, retire+used).
        for (int k = -2; k < 66; ++k) {
          const uint16_t t =
              static_cast<uint16_t>((base + static_cast<uint32_t>(k)) & kMask);
          const bool expect = (k >= 0) && (k < used);
          if (w.live(t) != expect) ++occ_mismatches;
        }
      }
    }
    zhao::check(head_positions == 264,
                "every head position x occupancy was actually visited", 264,
                head_positions);
    zhao::check(occ_mismatches == 0,
                "22.1: membership is exactly the interval at 0, 1, 63 and 64 "
                "live owners, at every head position, including intervals that "
                "cross numeric zero",
                0, occ_mismatches);
  }

  // SAME-SLOT HANDLES FROM EARLIER **AND LATER** GENERATIONS.
  // The earlier-generation case is the classic stale token. The LATER one is
  // the case a `>=` instead of a `<`, or a signed compare, would wave through:
  // a handle from a generation that has not been allocated yet must be just as
  // dead as one from a generation already retired.
  {
    Window w;
    Literal L;
    for (int i = 0; i < 40; ++i) { const uint16_t t = w.admit(); L.admit(t); }

    int earlier_alive = 0, later_alive = 0, checked = 0;
    for (uint16_t t : L.order) {
      const uint16_t slot = slot_of(t);
      const uint16_t gen  = gen_of(t);
      // Same slot, one generation back and one forward.
      const uint16_t earlier = static_cast<uint16_t>(((gen - 1) & 0xFF) << 6) | slot;
      const uint16_t later   = static_cast<uint16_t>(((gen + 1) & 0xFF) << 6) | slot;
      if (w.live(earlier)) ++earlier_alive;
      if (w.live(later)) ++later_alive;
      ++checked;
    }
    zhao::check(checked == 40, "every live owner was probed on both sides", 40,
                checked);
    zhao::check(earlier_alive == 0,
                "22.1: a same-slot handle from an EARLIER generation is dead",
                0, earlier_alive);
    zhao::check(later_alive == 0,
                "and a same-slot handle from a LATER generation is dead too -- "
                "the case a >= instead of a < would wave through",
                0, later_alive);
  }

  // ---- V02 (owner T2 brief): EVERY occupancy, EVERY head residue -----------
  // The brief's required case V02 asks for "every occupancy 0..64 and every
  // head residue", not a sample. The block above walks 66 head positions on a
  // coprime stride at four occupancies, which is a sample -- good enough to
  // catch a gross error, not good enough to satisfy the case as written.
  //
  // Exhaustive at the boundary instead: for every one of the 16,384 head
  // positions and every occupancy 0..64, probe the tickets immediately inside
  // and immediately outside the interval. That is where <=, a signed compare,
  // or a six-bit distance all fail, and it is 65 x 16,384 x 4 checks rather
  // than the 65 x 16,384 x 16,384 a full membership sweep would cost.
  {
    long probes = 0;
    int  errors = 0;
    for (uint32_t base = 0; base < kModulus; ++base) {
      for (int used = 0; used <= kSlots; ++used) {
        Window w;
        w.retire = static_cast<uint16_t>(base);
        w.alloc  = static_cast<uint16_t>((base + used) & kMask);
        w.used   = used;
        // one before the interval, first inside, last inside, one past the end
        const int ks[4] = {-1, 0, used - 1, used};
        for (int j = 0; j < 4; ++j) {
          const int k = ks[j];
          const uint16_t t =
              static_cast<uint16_t>((base + static_cast<uint32_t>(k)) & kMask);
          const bool expect = (k >= 0) && (k < used);
          if (w.live(t) != expect) ++errors;
          ++probes;
        }
      }
    }
    zhao::check(probes == 65L * kModulus * 4,
                "V02 swept every head residue at every occupancy 0..64", 1,
                probes == 65L * kModulus * 4 ? 1 : 0);
    zhao::check(errors == 0,
                "V02 membership is exact at both interval boundaries for all "
                "16,384 head residues and all 65 occupancies",
                0, errors);
  }

  const int rc = zhao::report_and_exit("texture_v3_window_identity");
  zhao::exit_hard(rc);
}
