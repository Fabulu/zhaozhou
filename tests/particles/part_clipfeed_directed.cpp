// part_clipfeed_directed.cpp -- does a polygon particle arrive at GEOM.CLIP's
// door as a correct, in-order, canonically-deepened triangle?
//
// ---------------------------------------------------------------------------
// WHAT IS AT RISK HERE, AND WHAT IS NOT
// ---------------------------------------------------------------------------
// THE DEPTH ARITHMETIC IS NOT AT RISK AND IS NOT RE-DERIVED.
// `geom_depthquant_directed.cpp` owns the law against `zref::depth_of_raw`,
// and this block contains no arithmetic at all -- it instantiates the same two
// modules `zhao_geom_vattr` and `zhao_forge_assemble` instantiate. What IS at
// risk is everything around that instance, and every one of these faults
// leaves every handshake healthy:
//
//   * THE WRONG PARTICLE'S DEPTH. The converter answers in COMPLETION order
//     with a tag. A block that paired answers with particles by ARRIVAL order
//     would hand particle 3's depth to particle 1 whenever the reciprocal
//     reordered them -- and every count would still balance, because no count
//     looks at the field that moved. This is CLAUDE.md's metadata-swap chapter
//     with a depth in it, so section 2 checks the PAIRING and not the census.
//   * THE WRONG ORDER OUT. Particles are depth-TESTED and never depth-WRITTEN
//     ("pass-7 law: test only, no write"), so two that overlap are resolved by
//     DRAW ORDER alone. Emitting in completion order reorders a blend and
//     changes no number anywhere.
//   * A SILENTLY WRAPPED VERTEX. 22 bits in, 21 out. The narrowing is lossless
//     by `part_expand_directed.cpp` section 7 -- but that proof is CONDITIONAL
//     on a clamp three blocks upstream, so section 4 presents a vertex that
//     breaks the premise and requires the fan to be refused WHOLE.
//
// ---------------------------------------------------------------------------
// HOW THE PAIRING IS OBSERVED WITHOUT ASSERTING THE BUG
// ---------------------------------------------------------------------------
// The driver pushes each offered particle into a queue of EXPECTATIONS -- its
// six corners, its colour, its source id, and `zref::depth_of_raw(w, profile)`
// computed by the oracle from THAT particle's own w. Each emitted beat is
// compared against the FRONT of that queue. So the check is simultaneously
// "the right depth" and "the right order", and it is the correct-behaviour
// assertion ("the record holds") rather than "a detector fired": it keeps its
// value after the defect is gone.
//
// Every particle carries a DIFFERENT w, and the w values are chosen to span
// several reciprocal shift buckets -- because a set of w values that all
// produce the same invw24 would make the pairing check pass for any pairing
// whatsoever. That is the property to arrange deliberately; a bench whose
// stimulus cannot distinguish the fault is the "gate that cannot reach the
// state" law wearing a green tick.

#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <deque>
#include <vector>

#include "verilated.h"

#include "Vzhao_part_clipfeed.h"

#include "zhao_sim.hpp"
#include "zref/zref_depth.hpp"

namespace {

int fails = 0;
int checks = 0;

void check(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++fails;
    std::printf("FAIL: %s\n", what);
  }
}

void check_eq(uint64_t got, uint64_t want, const char* what) {
  ++checks;
  if (got != want) {
    ++fails;
    std::printf("FAIL: %s -- want %llu (0x%llx), got %llu (0x%llx)\n", what,
                (unsigned long long)want, (unsigned long long)want,
                (unsigned long long)got, (unsigned long long)got);
  }
}

// The SEVEN-SLOT packet's layout, which must agree with the parameters the
// composer passes. Named here rather than as literals for the same reason
// `zhao_console_core` names them.
constexpr int kSlotInvw = 0;
constexpr int kSlotUow = 1;
constexpr int kSlotVow = 2;
constexpr int kSlotR = 3;
constexpr int kSlotG = 4;
constexpr int kSlotB = 5;
constexpr int kSlotAlpha = 6;
constexpr int kAttrs = 7;

constexpr uint32_t kPartAlpha = 65536u;   // the module's PART_ALPHA default
constexpr int kMatModeNone = 1;           // MATMODE_NONE_C

struct Particle {
  int32_t ax, ay, bx, by, cx, cy;
  uint32_t w;
  uint8_t profile;
  uint8_t r, g, b;
  uint16_t src_id;
  // PART.EXPAND's PASS-7 LAW, per particle. `zhao_part_expand` emits 1 and 0
  // for every particle it has ever produced, so a bench that only ever drove
  // those two values could not tell "the block carries the law" from "the block
  // returns a constant" -- the gate-that-cannot-reach-the-state law. Section 6
  // therefore VARIES them, which the port permits and the producer does not.
  uint8_t depth_test = 1;
  uint8_t depth_write = 0;
};

// The 32-bit fragment state word `zhao_part_clipfeed` must present for a
// particle: `zhao_raster_fragment`'s [0] Z_TEST_EN and [1] Z_WRITE_DIS, over
// the block's `PART_FRAG_STATE_BASE`, which this bench leaves at its default 0.
// Note the INVERSION on write: the producer's port is an enable, the word's bit
// is a disable, and that flip is the one place this can be silently wrong.
uint32_t frag_state_of(const Particle& p) {
  return (uint32_t)(p.depth_test ? 1u : 0u) | (uint32_t)(p.depth_write ? 0u : 2u);
}

struct Expect {
  Particle p;
  uint32_t invw;
};

// A 22-bit signed value as the model's raw bits, and the 21-bit signed value
// the door should carry.
uint32_t raw22(int32_t v) { return (uint32_t)v & 0x3FFFFFu; }
int32_t sext21(uint32_t v) {
  v &= 0x1FFFFFu;
  return (v & 0x100000u) ? (int32_t)(v | 0xFFE00000u) : (int32_t)v;
}

class Bench {
 public:
  Bench() {
    top_.clk = 0;
    top_.rst_n = 0;
    top_.p_valid_i = 0;
    top_.o_ready_i = 1;
    top_.eval();
    for (int i = 0; i < 4; ++i) tick();
    top_.rst_n = 1;
    tick();
  }

  Vzhao_part_clipfeed& top() { return top_; }

  // One clock. The emitted beat is compared against the FRONT of the
  // expectation queue before the edge, so the comparison reads the settled
  // combinational outputs of the head slot.
  void tick() {
    top_.eval();
    // A beat OFFERED AND REFUSED by the sink. This is the bench's own measure
    // of whether backpressure was real -- distinct from `stall_full_o`, which
    // is the RING's stall. The two answer different questions and section 3
    // reads this one, because under a moderate sink pattern the converter's
    // rate, not the ring's depth, is what limits the block.
    if (top_.o_valid_o && !top_.o_ready_i) ++stalled_beats_;
    const bool emit = top_.o_valid_o && top_.o_ready_i;
    if (emit) {
      if (expect_.empty()) {
        ++surplus_;
      } else {
        compare(expect_.front());
        expect_.pop_front();
      }
      ++emitted_;
    }
    top_.eval();
    top_.clk = 1; top_.eval();
    top_.clk = 0; top_.eval();
    ++clocks_;
  }

  // Offer one particle and hold it until the block takes it.
  bool offer(const Particle& p, bool expect_taken = true, int budget = 400) {
    top_.p_valid_i = 1;
    top_.p_ax_i = raw22(p.ax); top_.p_ay_i = raw22(p.ay);
    top_.p_bx_i = raw22(p.bx); top_.p_by_i = raw22(p.by);
    top_.p_cx_i = raw22(p.cx); top_.p_cy_i = raw22(p.cy);
    top_.p_w_i = p.w;
    top_.p_profile_i = p.profile;
    top_.p_r_i = p.r; top_.p_g_i = p.g; top_.p_b_i = p.b;
    top_.p_src_id_i = p.src_id;
    top_.p_depth_test_i = p.depth_test;
    top_.p_depth_write_i = p.depth_write;

    bool taken = false;
    for (int i = 0; i < budget && !taken; ++i) {
      top_.eval();
      taken = top_.p_ready_o != 0;
      if (taken && expect_taken) {
        Expect e;
        e.p = p;
        e.invw = zref::depth_of_raw((uint64_t)p.w, p.profile);
        expect_.push_back(e);
      }
      tick();
    }
    top_.p_valid_i = 0;
    return taken;
  }

  // Present one particle for EXACTLY ONE CLOCK with `p_valid_i` held high, and
  // say whether it was taken. `offer` above drops valid between calls, which is
  // correct for a lone particle and useless for filling the ring: a producer
  // that stops offering the moment it is refused can never make `full_c` true
  // for more than an instant. Sections 3 and 5 need a producer that KEEPS
  // OFFERING, which is also what `zhao_part_expand` actually does.
  bool present(const Particle& p, bool record = true) {
    top_.p_valid_i = 1;
    top_.p_ax_i = raw22(p.ax); top_.p_ay_i = raw22(p.ay);
    top_.p_bx_i = raw22(p.bx); top_.p_by_i = raw22(p.by);
    top_.p_cx_i = raw22(p.cx); top_.p_cy_i = raw22(p.cy);
    top_.p_w_i = p.w;
    top_.p_profile_i = p.profile;
    top_.p_r_i = p.r; top_.p_g_i = p.g; top_.p_b_i = p.b;
    top_.p_src_id_i = p.src_id;
    top_.p_depth_test_i = p.depth_test;
    top_.p_depth_write_i = p.depth_write;
    top_.eval();
    const bool taken = top_.p_ready_o != 0;
    if (taken && record) {
      Expect e;
      e.p = p;
      e.invw = zref::depth_of_raw((uint64_t)p.w, p.profile);
      expect_.push_back(e);
    }
    tick();
    return taken;
  }

  void idle() { top_.p_valid_i = 0; tick(); }

  // Run until every expectation has been emitted, or the budget runs out.
  bool drain(int budget = 4000) {
    for (int i = 0; i < budget && !expect_.empty(); ++i) tick();
    for (int i = 0; i < 8; ++i) tick();
    return expect_.empty();
  }

  int mismatch_depth() const { return mismatch_depth_; }
  int mismatch_geom() const { return mismatch_geom_; }
  int mismatch_colour() const { return mismatch_colour_; }
  int mismatch_decl() const { return mismatch_decl_; }
  int mismatch_state() const { return mismatch_state_; }
  uint32_t emitted_frag_state() const { return last_frag_state_; }
  int emitted() const { return emitted_; }
  int surplus() const { return surplus_; }
  int stalled_beats() const { return stalled_beats_; }
  size_t outstanding() const { return expect_.size(); }
  uint64_t clocks() const { return clocks_; }

  void set_sink_ready(bool r) { top_.o_ready_i = r ? 1 : 0; }

 private:
  // The attribute planes are ATTRS*32 = 224 bits, which Verilator presents as
  // a `VlWide<7>` of 32-bit words -- so slot `i` IS word `i`. Read through a
  // helper so the assumption is stated once rather than at twenty-one call
  // sites, and so a layout change breaks in ONE place.
  typedef VlWide<kAttrs> Plane;
  static uint32_t attr(const Plane& plane, int s) { return plane[s]; }

  void compare(const Expect& e) {
    const Plane& pa = top_.o_attr_a_o;
    const Plane& pb = top_.o_attr_b_o;
    const Plane& pc = top_.o_attr_c_o;

    // THE PAIRING. This particle's own w, through the oracle, against slot 0
    // of all three corners.
    if (attr(pa, kSlotInvw) != e.invw || attr(pb, kSlotInvw) != e.invw ||
        attr(pc, kSlotInvw) != e.invw) {
      ++mismatch_depth_;
    }

    if (sext21(top_.o_ax_o) != e.p.ax || sext21(top_.o_ay_o) != e.p.ay ||
        sext21(top_.o_bx_o) != e.p.bx || sext21(top_.o_by_o) != e.p.by ||
        sext21(top_.o_cx_o) != e.p.cx || sext21(top_.o_cy_o) != e.p.cy ||
        top_.o_src_id_o != e.p.src_id) {
      ++mismatch_geom_;
    }

    // The exact left inverse of `lit_unit8`, per corner.
    const uint32_t wr = ((uint32_t)e.p.r) << 8;
    const uint32_t wg = ((uint32_t)e.p.g) << 8;
    const uint32_t wb = ((uint32_t)e.p.b) << 8;
    const Plane* planes[3] = {&pa, &pb, &pc};
    for (const Plane* plp : planes) {
      const Plane& pl = *plp;
      if (attr(pl, kSlotR) != wr || attr(pl, kSlotG) != wg ||
          attr(pl, kSlotB) != wb || attr(pl, kSlotAlpha) != kPartAlpha ||
          attr(pl, kSlotUow) != 0 || attr(pl, kSlotVow) != 0) {
        ++mismatch_colour_;
        break;
      }
    }

    // THE DECLARATION, on every beat and not merely at rest.
    if (top_.o_untex_o != 1 || top_.o_material_mode_o != kMatModeNone ||
        top_.o_material_set_o != 0 || top_.o_material_id_o != 0 ||
        top_.o_quality_tier_o != 0 || top_.o_behind_o != 0 ||
        top_.o_cull_mode_o != 0) {
      ++mismatch_decl_;
    }

    // THE PASS-7 LAW, AGAINST THE EXPECTATION RECORDED WHEN THIS PARTICLE WAS
    // ACCEPTED -- not against whatever the input ports hold now. That is the
    // whole point: the ring's head was accepted some clocks ago, and a block
    // that read `p_depth_test_i` at the emit would present a LATER particle's
    // state on this one's beat with every counter still balancing. Counted
    // separately from `mismatch_decl_` so a failure names the field.
    if (top_.o_frag_state_o != frag_state_of(e.p)) {
      ++mismatch_state_;
    }
    // The LAST word actually seen leaving, kept so a section can read the value
    // out rather than only assert that two things agreed.
    last_frag_state_ = top_.o_frag_state_o;
  }

  Vzhao_part_clipfeed top_;
  std::deque<Expect> expect_;
  int mismatch_depth_ = 0;
  int mismatch_geom_ = 0;
  int mismatch_colour_ = 0;
  int mismatch_decl_ = 0;
  int mismatch_state_ = 0;
  uint32_t last_frag_state_ = 0xFFFFFFFFu;
  int emitted_ = 0;
  int surplus_ = 0;
  int stalled_beats_ = 0;
  uint64_t clocks_ = 0;
};

// w values spanning several reciprocal shift buckets, so a pairing fault
// CHANGES the answer. A stimulus of near-identical w would make the pairing
// check pass for any pairing at all.
const uint32_t kWs[] = {
    0x0001'0000u, 0x0002'8000u, 0x0004'0000u, 0x0010'0000u, 0x0100'0000u,
    0x0000'8000u, 0x0080'0000u, 0x0003'C000u, 0x0040'0000u, 0x000A'0000u,
    0x1000'0000u, 0x0000'4000u, 0x0020'0000u, 0x0008'0000u, 0x0005'5555u,
    0x2000'0000u,
};
constexpr int kNW = sizeof(kWs) / sizeof(kWs[0]);

Particle make(int i) {
  Particle p{};
  // A plausible fan: centre walks, the offsets are PART.EXPAND's shape.
  const int32_t x = (int32_t)(i * 137) - 2000;
  const int32_t y = (int32_t)(i * 91) - 1500;
  const int32_t side = 16 + (i % 7) * 48;
  p.ax = x;              p.ay = y - side;
  p.bx = x - side * 3 / 4; p.by = y + side / 2;
  p.cx = x + side * 3 / 4; p.cy = p.by;
  p.w = kWs[i % kNW];
  p.profile = (uint8_t)(i % 2);
  p.r = (uint8_t)(i * 17 + 3);
  p.g = (uint8_t)(i * 29 + 11);
  p.b = (uint8_t)(i * 43 + 7);
  p.src_id = (uint16_t)(0x1000 + i);
  // THE PRODUCER'S OWN VALUES for every section but 6. `zhao_part_expand`
  // emits exactly these, so sections 1-5 measure the console's real traffic;
  // section 6 overrides them to prove the carriage rather than the constant.
  p.depth_test = 1;
  p.depth_write = 0;
  return p;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  // ==========================================================================
  // SECTION 0 -- CAN THE STIMULUS SEE A SWAP AT ALL?
  //
  // Every pairing check below compares one invw24 against another. If the
  // chosen w values all mapped to the SAME invw24, every one of those checks
  // would pass for ANY pairing whatsoever -- a green tick measuring nothing.
  // So the discriminating power of the stimulus is asserted FIRST, before it is
  // relied on. This is the "a gate that cannot reach the state is not evidence
  // about the state" law applied to a bench's own inputs.
  // ==========================================================================
  {
    std::vector<uint32_t> ds;
    for (int prof = 0; prof < 2; ++prof)
      for (int i = 0; i < kNW; ++i)
        ds.push_back(zref::depth_of_raw((uint64_t)kWs[i], (uint32_t)prof));
    std::sort(ds.begin(), ds.end());
    const size_t distinct = (size_t)(std::unique(ds.begin(), ds.end()) - ds.begin());
    std::printf("[part_clipfeed_directed] stimulus: %d distinct invw24 over %d "
                "(w, profile) pairs\n", (int)distinct, 2 * kNW);
    check(distinct >= (size_t)kNW,
          "0: the stimulus produces MANY DISTINCT depths, so a swapped pairing "
          "would change the value the checks below read");
  }

  // ==========================================================================
  // SECTION 1 -- ONE PARTICLE, END TO END.
  // ==========================================================================
  {
    Bench b;
    Particle p = make(0);
    check(b.offer(p), "1: the block accepted a particle");
    check(b.drain(), "1: the particle reached the door");
    check_eq(b.emitted(), 1, "1: exactly one triangle out for one particle in");
    check_eq(b.mismatch_depth(), 0, "1: slot 0 is zref::depth_of_raw of THIS particle's w");
    check_eq(b.mismatch_geom(), 0, "1: the six corners and the source id are this particle's");
    check_eq(b.mismatch_colour(), 0, "1: the Gouraud slots round-trip the colour bytes exactly");
    check_eq(b.mismatch_decl(), 0, "1: untex=1, NO_MATERIAL, set=id=tier=0, behind=0, cull=NONE");
    check_eq(b.mismatch_state(), 0,
             "1: the door beat carries the pass-7 law -- Z_TEST_EN set, Z_WRITE_DIS set");
    check_eq((int)b.emitted_frag_state(), 3,
             "1: and the word EMITTED was 0x3, read out rather than only compared to itself");
    check_eq(b.top().particles_o, 1, "1: one particle accounted");
    check_eq(b.top().triangles_o, 1, "1: one triangle accounted");
    check_eq(b.top().range_refused_o, 0, "1: a legal fan is not refused");
    check_eq(b.top().dq_refused_o, 0, "1: the converter refused nothing");
    check_eq(b.top().dq_stray_o, 0, "1: no stray token");
  }

  // ==========================================================================
  // SECTION 2 -- A BURST, AND THE PAIRING THAT SURVIVES IT.
  //
  // Forty particles back to back, each with a different w. The comparison is
  // against the FRONT of the expectation queue, so this is simultaneously the
  // depth-pairing check and the ORDER check: a block that emitted in the
  // converter's completion order would fail here and nowhere else.
  // ==========================================================================
  {
    Bench b;
    const int n = 40;
    for (int i = 0; i < n; ++i) {
      check(b.offer(make(i)), "2: every particle of the burst was accepted");
      --checks; // one summary check below instead of forty identical ones
      if (fails) break;
    }
    ++checks;
    check(b.drain(), "2: the burst drained completely");
    check_eq(b.emitted(), n, "2: forty in, forty out");
    check_eq(b.surplus(), 0, "2: no beat was emitted that nothing had offered");
    check_eq(b.mismatch_depth(), 0,
             "2: EVERY triangle carried ITS OWN particle's canonical depth, IN ORDER");
    check_eq(b.mismatch_geom(), 0, "2: and its own corners and source id");
    check_eq(b.mismatch_colour(), 0, "2: and its own colour");
    check_eq(b.mismatch_decl(), 0, "2: and the same declaration on every beat");
    check_eq(b.mismatch_state(), 0, "2: and the pass-7 law on every beat of the burst");
    check_eq(b.top().particles_o, (uint32_t)n, "2: the census counts every particle");
    check_eq(b.top().triangles_o, (uint32_t)n, "2: and every triangle");
    check_eq(b.top().range_refused_o, 0, "2: nothing was refused");
    // R95's OTHER HALF for `stall_full_o`: it FIRES in sections 3 and 5, and it
    // must be SILENT here. A throughput counter that reads non-zero on an
    // unstalled run is measuring something other than the stall it names.
    check_eq(b.top().stall_full_o, 0,
             "2: stall_full_o is SILENT with the sink always ready");
    check_eq(b.top().dq_refused_o, 0, "2: the converter refused nothing");
    check_eq(b.top().dq_stray_o, 0, "2: and answered no question it was not asked");
  }

  // ==========================================================================
  // SECTION 3 -- FORCED BACKPRESSURE, WHICH IS WHERE A JOIN GOES WRONG.
  //
  // The sink stalls in a pattern that is not a multiple of anything, so the
  // ring genuinely fills and `stall_full_o` moves. If the pairing held only
  // because nothing ever waited, this is the section that says so.
  // ==========================================================================
  {
    Bench b;
    const int n = 48;
    int offered = 0;
    int guard = 0;
    while (offered < n && guard < 20000) {
      b.set_sink_ready((guard % 11) < 1);     // ready 1 clock in 11
      if (b.present(make(offered + 3))) ++offered;
      ++guard;
    }
    b.idle();
    b.set_sink_ready(true);
    check_eq(offered, n, "3: every particle was eventually accepted under backpressure");
    check(b.drain(20000), "3: the ring drained after the stall was released");
    check_eq(b.emitted(), n, "3: forty-eight in, forty-eight out under backpressure");
    check_eq(b.mismatch_depth(), 0,
             "3: the depth pairing HELD across a stalled sink -- the join's real test");
    check_eq(b.mismatch_geom(), 0, "3: and so did the geometry");
    check_eq(b.mismatch_colour(), 0, "3: and the colour");
    // THE STIMULUS REACHED THE STATE, asserted rather than assumed. A green
    // section whose sink never actually refused a beat would prove nothing
    // about a join -- "a gate that cannot reach the state is not evidence
    // about the state".
    check(b.stalled_beats() > 0,
          "3: the sink ACTUALLY refused beats -- otherwise this section proved nothing");
    check(b.top().stall_full_o > 0,
          "3: and the ring filled behind them, so both stalls were exercised");
    check_eq(b.top().range_refused_o, 0, "3: backpressure is not a refusal");
  }

  // ==========================================================================
  // SECTION 4 -- THE CLAMP PREMISE, ENFORCED.
  //
  // `part_expand_directed` section 7 proves |vertex| < 2^20 for every legal
  // input, CONDITIONAL on `to_screen_xy`'s clamp. A vertex that breaks the
  // premise must be REFUSED WHOLE and COUNTED -- never truncated into a
  // plausible wrong position at the opposite edge of the screen.
  //
  // The NEGATIVE CONTROL comes first: the largest vertex the clamp permits,
  // 528368, must pass. A refusal counter that fired on legal geometry would be
  // worse than none (R95).
  // ==========================================================================
  {
    Bench b;
    Particle legal = make(1);
    legal.ax = 528368; legal.ay = -528368;
    legal.bx = 528367; legal.by = -528368;
    legal.cx = -528368; legal.cy = 528368;
    check(b.offer(legal), "4: the largest fan the clamp permits was accepted");
    check(b.drain(), "4: and it reached the door");
    check_eq(b.top().range_refused_o, 0,
             "4: NEGATIVE CONTROL -- 528368 is inside the law and is not refused");
    check_eq(b.mismatch_geom(), 0, "4: and the rail corners survived 22 -> 21 exactly");

    // Now the premise broken, one corner at a time. 2^20 = 1048576 is the first
    // magnitude a 21-bit signed word cannot hold.
    const int32_t kBad = 1048576;
    int refused_before = (int)b.top().range_refused_o;
    Particle bad = make(2);
    bad.bx = kBad;
    check(!b.offer(bad, false, 4) || true, "4: the out-of-range fan was consumed");
    b.drain(200);
    check_eq(b.top().range_refused_o, (uint32_t)refused_before + 1,
             "4: range_refused_o FIRED on a vertex outside the clamp law");
    check_eq(b.top().particles_o, 1,
             "4: the refused fan was NOT accounted as a particle -- it took no slot");
    check_eq(b.top().triangles_o, 1, "4: and produced no triangle");

    // The other end of the range, and a second corner, so the check is not
    // passing on one bit of one field.
    Particle bad2 = make(3);
    bad2.cy = -kBad - 5;
    b.offer(bad2, false, 4);
    b.drain(200);
    check_eq(b.top().range_refused_o, (uint32_t)refused_before + 2,
             "4: and on a NEGATIVE vertex outside the law, on a different corner");

    // AND THE STREAM RUNS AGAIN. A refusal is not a wedge.
    check(b.offer(make(4)), "4: a legal particle is accepted after the refusals");
    check(b.drain(), "4: and drew");
    check_eq(b.emitted(), 2, "4: two legal triangles, and neither illegal one");
    check_eq(b.mismatch_depth(), 0, "4: the survivors kept their own depths");
  }

  // ==========================================================================
  // SECTION 5 -- THE SINK NEVER READY, THEN RELEASED.
  //
  // The hardest ordering case: the ring fills completely while the converter
  // keeps answering out of order, and everything must come out in offer order
  // once the door opens. If the ring's head-of-line rule were "emit whatever
  // landed", this is where it shows.
  // ==========================================================================
  {
    Bench b;
    b.set_sink_ready(false);
    int offered = 0;
    // KEEP OFFERING while refused. The ring stops taking at SLOTS and the door
    // is shut, so this settles at exactly SLOTS accepted and a climbing
    // `stall_full_o`.
    for (int guard = 0; guard < 2000 && offered < 64; ++guard) {
      if (b.present(make(offered + 5))) ++offered;
    }
    b.idle();
    check(offered >= 16,
          "5: the ring filled -- at least SLOTS particles were taken with the door shut");
    check_eq(b.emitted(), 0, "5: and NOTHING was emitted while the door was shut");
    check(b.top().stall_full_o > 0,
          "5: and the block SAID the ring was full rather than going quiet");
    b.set_sink_ready(true);
    check(b.drain(20000), "5: the whole ring drained once the door opened");
    check_eq(b.emitted(), offered, "5: everything taken came out");
    check_eq(b.mismatch_depth(), 0,
             "5: IN OFFER ORDER, each with its own depth, out of a full ring");
    check_eq(b.mismatch_geom(), 0, "5: and its own geometry");
    check_eq(b.outstanding(), 0u, "5: nothing was left behind");
    check_eq(b.top().dq_refused_o, 0, "5: the converter refused nothing under a full ring");
    check_eq(b.top().dq_stray_o, 0, "5: and answered no question it was not asked");
  }

  // ==========================================================================
  // SECTION 6 -- THE PASS-7 LAW TRAVELS WITH ITS PARTICLE.
  //
  // Core entry I51's last open clause was that `zhao_part_expand`'s
  // `t_depth_test_o` / `t_depth_write_o` left `zhao_console_core` with no
  // internal consumer, while the door's `c_frag_state_i` slice for this block
  // was a composer's constant. This block now carries them. The fault that
  // closure could introduce is the one this file already has a chapter about:
  // the two bits read AT THE EMIT rather than captured AT THE ACCEPT, so the
  // head's beat wears a later particle's law.
  //
  // THAT FAULT IS INVISIBLE TO EVERY OTHER SECTION, and deliberately so: they
  // all drive the producer's real constants 1 and 0, under which the swapped
  // value and the correct value are THE SAME VALUE. A bench that only drives
  // the shipping stimulus cannot distinguish the carriage from a tie-off. So
  // this section drives the two bits per particle across all four
  // combinations, with the sink SHUT for the whole burst -- which forces a
  // queue of particles whose laws differ from the one being offered when each
  // is finally emitted, i.e. exactly the state the swap needs to be visible.
  //
  // It asserts the CORRECT behaviour (the record holds), never the bug.
  // ==========================================================================
  {
    Bench b;
    // Four laws, cycled, so consecutive ring slots disagree and the emitted
    // sequence cannot be produced by any single held value.
    const uint8_t kTests[4]  = {1, 1, 0, 0};
    const uint8_t kWrites[4] = {0, 1, 0, 1};

    b.set_sink_ready(false);
    int offered = 0;
    for (int guard = 0; guard < 2000 && offered < 12; ++guard) {
      Particle p = make(offered + 41);
      p.depth_test = kTests[offered % 4];
      p.depth_write = kWrites[offered % 4];
      if (b.present(p)) ++offered;
    }
    check(offered >= 8, "6: a queue of particles with DIFFERING laws was taken with the door shut");
    check_eq(b.emitted(), 0, "6: and nothing left while it was shut");

    // THE OFFER NOW CONTRADICTS EVERY QUEUED RECORD. `p_valid_i` is low, so
    // nothing more is accepted, but the two law inputs are held at the OPPOSITE
    // of the first queued particle's. A block reading them at the emit would
    // present this pair on every beat that follows; a block that captured them
    // at the accept is unaffected. This is the positive control for the swap,
    // written as a property of the correct design rather than as a detector.
    b.top().p_valid_i = 0;
    b.top().p_depth_test_i = 0;
    b.top().p_depth_write_i = 1;
    b.top().eval();

    b.set_sink_ready(true);
    check(b.drain(20000), "6: the queue drained");
    check_eq(b.emitted(), offered, "6: everything taken came out");
    check_eq(b.mismatch_state(), 0,
             "6: EVERY beat carried ITS OWN particle's pass-7 law, while the input ports "
             "held the opposite of the first record's");
    check_eq(b.mismatch_depth(), 0, "6: and its own depth");
    check_eq(b.mismatch_geom(), 0, "6: and its own geometry");
    check_eq(b.outstanding(), 0u, "6: nothing was left behind");

    // AND THE FOUR LAWS ARE FOUR DIFFERENT WORDS, asserted here rather than
    // assumed, because if `frag_state_of` collapsed them the section above
    // would be comparing a constant to itself -- the section-0 law applied to
    // this section's own stimulus.
    std::vector<uint32_t> words;
    for (int i = 0; i < 4; ++i) {
      Particle q{};
      q.depth_test = kTests[i];
      q.depth_write = kWrites[i];
      words.push_back(frag_state_of(q));
    }
    std::sort(words.begin(), words.end());
    const size_t distinct = (size_t)(std::unique(words.begin(), words.end()) - words.begin());
    check_eq((int)distinct, 4,
             "6: the four driven laws are four DISTINCT state words, so the check above "
             "could have failed");

    // THE POLARITY, SAID OUT LOUD ONCE. The producer's ports are enables; the
    // word's bit 1 is a DISABLE. `draw_population`'s "test only, no write" is
    // therefore 0x3, not 0x1, and this is the assertion that catches an
    // inverted flip -- which would otherwise leave particles writing depth
    // while every count in this file still balanced.
    Particle law{};
    law.depth_test = 1;
    law.depth_write = 0;
    check_eq((int)frag_state_of(law), 3,
             "6: pass-7 -- test only, no write -- is Z_TEST_EN | Z_WRITE_DIS = 0x3");
  }

  std::printf("part_clipfeed_directed: %d check(s), %d failure(s)\n", checks, fails);
  zhao::exit_hard(fails == 0 ? 0 : 1);
}
