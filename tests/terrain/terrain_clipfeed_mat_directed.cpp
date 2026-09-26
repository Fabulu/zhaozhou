// terrain_clipfeed_mat_directed.cpp -- TERRAIN's MATERIAL IDENTITY at
// GEOM.CLIP's door: the derivation, the orphan fault, and the span stability.
//
// Console entry I13, items (a) and (b). TERRAINMAT, 2026-09-26.
//
// ---------------------------------------------------------------------------
// WHAT IS PROVED, AND WHAT IS DELIBERATELY NOT
// ---------------------------------------------------------------------------
// Until 2026-09-26 `zhao_terrain_clipfeed` declared `MATMODE_NONE` from a
// localparam and drove {set, id} to literal zeros. It now takes the host's
// `SetEnvironment.terrain_material_set` / `.terrain_material_id` and DERIVES
// the mode. The derivation is the new law and this file is its test:
//
//   1. THE THREE ARMS OF THE DERIVATION, each by stimulus:
//        set != 0                 -> MATMODE_BACKED, the pair passed WHOLE
//        set == 0 && id == 0      -> MATMODE_NONE, the zero pair
//        set == 0 && id != 0      -> MATMODE_NONE, the zero pair, AND the
//                                    orphan counter fires
//      The third arm is the one that matters and it is not a tidiness case.
//      `zhao_material_window`'s `mode_contra_c` REFUSES a non-zero {set, id}
//      under MATMODE_NONE -- it is a fault there, and a refused triangle does
//      not reach GEOM.CLIP. An environment that named an id with no set would
//      therefore drop the frame's ENTIRE terrain arm. The block discards the
//      id and counts it instead, which is the owner directive's "diagnosed
//      and handled by the declared failure/fallback policy, never silently
//      truncated or made token 0".
//
//   2. THE FULL WIDTH SURVIVES. A 32-bit set and a 16-bit id with every byte
//      distinct and the top bits set arrive at the door unchanged. The
//      directive's numerical policy says a material token "remains an opaque,
//      full-width u32, never narrowed to fit an older consumer", and a
//      narrowing here would be invisible against the small handles the smoke
//      fixture uses.
//
//   3. THE IDENTITY IS PART OF THE TRIANGLE, NOT READ LIVE. `mat_set_i` is a
//      frame-global register in the composer, so the tempting implementation
//      is a combinational path from it to `o_material_set_o`. Section 3
//      accepts a triangle under one identity, CHANGES the inputs while that
//      triangle is still in flight, and requires the door to present the
//      identity the triangle was ACCEPTED under. A live path passes every
//      other check in this file and fails only this one -- it is CLAUDE.md's
//      metadata-swap chapter, where two quantities move together and every
//      counter still balances.
//
//   4. THE CENSUS CANNOT RUN AHEAD OF THE TRAFFIC. `mat_backed_o` is counted
//      at the door's GRANT, so it is bounded by `emitted_o` at every instant,
//      not merely at the end.
//
// NOT PROVED HERE, and named so the scope is not mistaken for the whole debt:
// entry I13 records that this block has no directed test AT ALL. This file
// does not pay that in full. It says nothing about the perspective multiply,
// the palette ladder's five rungs, the depth converter's tag discipline or
// the three-way join's `src_id` guard. Those are the block's other subjects
// and they remain owed. What is paid is the part TERRAINMAT created.
//
// A NOTE ON THE STIMULUS. The triangle is a fixed, well-formed, non-degenerate
// one declared in the bench wrapper; it exists to carry a beat through the
// block. Every assertion below is about the identity travelling with it, so
// the triangle's own values are deliberately uninteresting and constant.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "verilated.h"

#include "Vtb_terrain_clipfeed_mat.h"

#include "zhao_sim.hpp"

using zhao::check;

namespace {

// A legal raw `w`: fx16, comfortably away from zero so the depth converter's
// reciprocal is an ordinary one and this bench is not measuring its edges.
constexpr uint32_t kW = 0x0010'0000u;
// Mid-rung: inside the ladder's domain [0, 65536] so `shade_clamped_o` stays
// silent. The shade's VALUE is not this file's subject.
constexpr uint32_t kShade = 32768u;

struct Dut {
  Vtb_terrain_clipfeed_mat& d;
  uint64_t cycles = 0;
  // Every identity the door was seen to present, in order.
  struct Seen {
    uint32_t set;
    uint16_t id;
    uint8_t mode;
    uint16_t src;
  };
  Seen last{};
  int seen_count = 0;
  bool census_ok = true;  // section 4's running bound

  explicit Dut(Vtb_terrain_clipfeed_mat& dut) : d(dut) {}

  void tick() {
    d.eval();
    if (d.o_valid && d.o_ready) {
      last.set = d.o_material_set;
      last.id = static_cast<uint16_t>(d.o_material_id);
      last.mode = static_cast<uint8_t>(d.o_material_mode);
      last.src = static_cast<uint16_t>(d.o_src_id);
      ++seen_count;
    }
    // SECTION 4, EVALUATED EVERY CYCLE rather than once at the end. A counter
    // checked only after the run can be ahead in the middle and right at the
    // end; this is the statement that it never leads.
    if (d.mat_backed > d.emitted) census_ok = false;
    zhao::tick(d);
    ++cycles;
  }

  void idle(int n) {
    for (int i = 0; i < n; ++i) tick();
  }

  // Offer one triangle on all three join handshakes and run until the door
  // grants it. The block is strictly serial, so exactly one is in flight.
  void offer(uint16_t src) {
    d.t_valid = 1;
    d.l_valid = 1;
    d.u_valid = 1;
    d.t_src = src;
    d.t_w = kW;
    d.l_shade = kShade;
    d.l_degenerate = 0;
    // The join accepts only when all three are offered; hold until it takes.
    for (int i = 0; i < 64; ++i) {
      d.eval();
      if (d.t_valid && d.t_ready && d.l_ready && d.u_ready) {
        tick();
        break;
      }
      tick();
    }
    d.t_valid = 0;
    d.l_valid = 0;
    d.u_valid = 0;
  }

  // Run until the emitted count reaches `target`, or give up loudly.
  bool drain_to(uint32_t target, int budget = 4000) {
    for (int i = 0; i < budget; ++i) {
      d.eval();
      if (d.emitted >= target) return true;
      tick();
    }
    return false;
  }
};

void reset(Vtb_terrain_clipfeed_mat& d) {
  d.rst_n = 0;
  d.t_valid = 0;
  d.l_valid = 0;
  d.u_valid = 0;
  d.t_src = 0;
  d.t_w = kW;
  d.l_shade = kShade;
  d.l_degenerate = 0;
  d.mat_set = 0;
  d.mat_id = 0;
  d.o_ready = 1;
  d.eval();
  for (int i = 0; i < 8; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

constexpr uint8_t kBacked = 0;  // zhao_material_window::MATMODE_BACKED_C
constexpr uint8_t kNone = 1;    // zhao_material_window::MATMODE_NONE_C

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_terrain_clipfeed_mat dv;
  reset(dv);
  Dut t(dv);

  // =========================================================================
  // 1. THE BACKED ARM, AND THE FULL WIDTH (sections 1 and 2)
  // =========================================================================
  // Every byte distinct and the top bit of both fields set: a narrowing, a
  // sign-extension or a byte swap anywhere on the path is a different number
  // here, and is the SAME number under the small handles the smoke uses.
  const uint32_t kSet = 0xDEAD'BE01u;
  const uint16_t kId = 0xF00Du;
  {
    dv.mat_set = kSet;
    dv.mat_id = kId;
    t.offer(0x1234);
    check(t.drain_to(1), "1.a BACKED triangle reaches the door", 1u, dv.emitted);
    check(t.last.mode == kBacked, "1.a non-zero material_set is declared MATMODE_BACKED",
          kBacked, t.last.mode);
    check(t.last.set == kSet, "2.the 32-bit set arrives at the door UNNARROWED", kSet, t.last.set);
    check(t.last.id == kId, "2.the 16-bit id arrives at the door UNNARROWED", kId, t.last.id);
    check(t.last.src == 0x1234, "1.the triangle's own src_id rides beside its material",
          0x1234u, t.last.src);
    check(dv.mat_backed == 1, "1.the BACKED census counts exactly one", 1u, dv.mat_backed);
    check(dv.mat_id_orphan == 0, "1.a legal pair raises no orphan", 0u, dv.mat_id_orphan);
    check(dv.src_id_mismatch == 0, "1.the three-way join agreed", 0u, dv.src_id_mismatch);
    check(dv.o_untex == 0, "1.terrain still declares itself TEXTURED", 0u, dv.o_untex);
  }

  // =========================================================================
  // 2. THE LAWFUL NO-MATERIAL ARM -- the pre-2026-09-26 behaviour
  // =========================================================================
  // This is what every capture written before the ABI field existed presents,
  // because those two bytes were `pad` and were therefore zero. It must be
  // bit-identical to what this block did then: MATMODE_NONE and a ZERO PAIR,
  // which is what `mode_contra_c` REQUIRES rather than a tie-off.
  {
    const uint32_t backed_before = dv.mat_backed;
    dv.mat_set = 0;
    dv.mat_id = 0;
    t.offer(0x5678);
    check(t.drain_to(2), "2.a NONE triangle reaches the door", 2u, dv.emitted);
    check(t.last.mode == kNone, "2.a zero material_set is declared MATMODE_NONE", kNone,
          t.last.mode);
    check(t.last.set == 0, "2.MATMODE_NONE presents a ZERO set -- mode_contra_c requires it", 0u,
          t.last.set);
    check(t.last.id == 0, "2.MATMODE_NONE presents a ZERO id -- mode_contra_c requires it", 0u,
          t.last.id);
    check(dv.mat_backed == backed_before,
          "2.a NONE triangle does NOT move the BACKED census", backed_before, dv.mat_backed);
    check(dv.mat_id_orphan == 0, "2.a zero pair raises no orphan", 0u, dv.mat_id_orphan);
  }

  // =========================================================================
  // 3. THE ORPHAN RULE -- THE FAULT, FIRED BY LEGAL STIMULUS
  // =========================================================================
  // An id with no set. This is reachable at this block's boundary with nothing
  // broken, so it owes NO committed mutant -- it is fired here, by exact
  // amount, and the console smoke then asserts its SILENCE with this as the
  // positive control.
  //
  // THE ASSERTION IS THE CORRECT BEHAVIOUR, not the defect: the door is
  // presented the ZERO PAIR under MATMODE_NONE. If the block were ever
  // repaired to reject the environment instead, the counter would move and
  // these three checks would still describe what must be true of whatever
  // reaches GEOM.CLIP.
  {
    const uint32_t backed_before = dv.mat_backed;
    dv.mat_set = 0;
    dv.mat_id = 0x00A5;
    t.offer(0x9ABC);
    check(t.drain_to(3), "3.an ORPHANED id still lets the triangle through", 3u, dv.emitted);
    check(dv.mat_id_orphan == 1, "3.the orphan counter FIRES, exactly once", 1u,
          dv.mat_id_orphan);
    check(t.last.mode == kNone, "3.an id with no set is declared MATMODE_NONE", kNone,
          t.last.mode);
    check(t.last.id == 0,
          "3.THE ORPHANED ID IS DISCARDED at the door -- presenting it would trip "
          "mode_contra_c and drop the frame's whole terrain arm",
          0u, t.last.id);
    check(t.last.set == 0, "3.and the set stays zero beside it", 0u, t.last.set);
    check(dv.mat_backed == backed_before, "3.an orphan does not move the BACKED census",
          backed_before, dv.mat_backed);
  }

  // =========================================================================
  // 4. THE IDENTITY IS LATCHED ON THE TRIANGLE'S OWN BEAT
  // =========================================================================
  // THE CHECK THIS FILE EXISTS FOR, after the orphan. Accept a triangle under
  // one identity, then move the inputs to a DIFFERENT legal identity while it
  // is still in flight, and require the door to present the one it was
  // accepted under.
  //
  // WHY THIS IS NOT PARANOIA ABOUT A WIRE THAT CANNOT MOVE: in the composed
  // console `mat_set_i` is driven from a frame-global register that CMD.EXEC
  // reloads on every committed SetEnvironment. A combinational path from that
  // register to this port would repaint an already-accepted triangle with a
  // later frame's material -- and NOTHING would count it, because no counter
  // in the arm looks at the field that moved. That is the metadata-swap
  // chapter exactly: two quantities corrupted in lockstep, every census
  // balancing.
  //
  // The stall is real backpressure: `o_ready` is held LOW so the block sits in
  // S_EMIT with the triangle complete and the door refusing it.
  {
    const uint32_t kSetA = 0x1111'2222u;
    const uint16_t kIdA = 0x3333u;
    const uint32_t kSetB = 0x4444'5555u;
    const uint16_t kIdB = 0x6666u;

    dv.o_ready = 0;  // the door refuses, so the emit beat waits
    dv.mat_set = kSetA;
    dv.mat_id = kIdA;
    t.offer(0x0F0F);

    // Let it walk all the way to the emit state under backpressure.
    t.idle(300);
    dv.eval();
    check(dv.o_valid == 1,
          "4.the block reaches its emit beat and HOLDS it while the door refuses", 1u,
          dv.o_valid);
    check(dv.emitted == 3, "4.nothing was emitted while the door refused", 3u, dv.emitted);

    // NOW MOVE THE INPUTS. A live path changes the offered identity here.
    dv.mat_set = kSetB;
    dv.mat_id = kIdB;
    dv.eval();
    check(dv.o_material_set == kSetA,
          "4.THE DOOR STILL PRESENTS THE IDENTITY THE TRIANGLE WAS ACCEPTED UNDER, "
          "not the one now on the input port",
          kSetA, dv.o_material_set);
    check(static_cast<uint16_t>(dv.o_material_id) == kIdA,
          "4.and the same for the id", kIdA, static_cast<uint16_t>(dv.o_material_id));

    // Release the door; the granted beat must still carry A.
    dv.o_ready = 1;
    check(t.drain_to(4), "4.the held triangle is granted once the door opens", 4u, dv.emitted);
    check(t.last.set == kSetA, "4.and the GRANTED beat carries A, not B", kSetA, t.last.set);
    check(t.last.id == kIdA, "4.and the granted id is A's", kIdA, t.last.id);
    check(t.last.mode == kBacked, "4.under a non-zero set it is still BACKED", kBacked,
          t.last.mode);
  }

  // =========================================================================
  // 5. THE NEXT TRIANGLE DOES TAKE THE NEW IDENTITY
  // =========================================================================
  // The complement of section 4, and it is required: a block that simply
  // FROZE the identity at the first triangle would pass every check above.
  // The latch must reload on each accept.
  {
    const uint32_t kSetB = 0x4444'5555u;
    const uint16_t kIdB = 0x6666u;
    t.offer(0x0E0E);
    check(t.drain_to(5), "5.the next triangle reaches the door", 5u, dv.emitted);
    check(t.last.set == kSetB,
          "5.a triangle accepted AFTER the change carries the NEW identity -- the latch "
          "reloads per triangle and is not frozen at the first",
          kSetB, t.last.set);
    check(t.last.id == kIdB, "5.and the new id with it", kIdB, t.last.id);
  }

  // =========================================================================
  // 6. THE CENSUS AGREES WITH THE TRAFFIC, AT EVERY INSTANT
  // =========================================================================
  check(t.census_ok,
        "6.the BACKED census NEVER led the emitted count on any cycle -- it is taken on "
        "the door's own grant",
        1u, t.census_ok ? 1u : 0u);
  check(dv.mat_backed == 3,
        "6.exactly three of the five triangles declared a material (sections 1, 4 and 5)", 3u,
        dv.mat_backed);
  check(dv.emitted == 5, "6.five triangles were emitted in all", 5u, dv.emitted);
  check(dv.triangles == dv.emitted,
        "6.every triangle accepted was also emitted -- nothing is stuck in the block",
        dv.emitted, dv.triangles);
  check(dv.mat_id_orphan == 1, "6.the orphan fired exactly once in the whole run", 1u,
        dv.mat_id_orphan);
  check(dv.src_id_mismatch == 0, "6.the three-way join never disagreed", 0u,
        dv.src_id_mismatch);

  dv.final();
  return zhao::report_and_exit("terrain_clipfeed_mat_directed");
}
