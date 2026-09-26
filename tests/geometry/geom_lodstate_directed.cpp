// geom_lodstate_directed.cpp -- THE WHOLE R68 CHAIN against
// `zref::creature::lod_update`, the law it implements.
//
// Every block in the bench is real: the configuration snooper, the ladder bank
// loaded from a page `zref::creature_page::build` wrote, the state owner, the
// projected-radius evaluator and the ladder. The driver supplies only what
// genuinely comes from outside -- the projector bus, the asset window, the job
// tap, the instance centre's `w`, and the governor's threshold.
//
// THE ORACLE IS A RUNNING MODEL, NOT A TABLE. A `zref::creature::LodState` is
// kept per instance on the C++ side and stepped by `zref::creature::lod_update`
// with the SAME projected radius the RTL computed, so the hysteresis and the
// minimum hold are compared across a whole sequence of frames rather than at
// one point. A ladder that lost its state, or reset it, or held the wrong
// instance's, only shows up over time.
//
// WHAT ACTUALLY DISCRIMINATES:
//
//   1. STATE PERSISTS ACROSS FRAMES, PER INSTANCE. Four creatures walk toward
//      the camera over sixty frames; every rung and every hold count is
//      compared against the model at every frame. A block holding ONE ladder
//      for all four, or clearing on `frame_i`, fails here and nowhere else.
//   2. ONE TICK PER DRAW, NOT PER MESHLET. Each creature's draw is eight jobs
//      with the same instance id; `ticks_o` must count ONE and
//      `skipped_repeat_o` seven. A block ticking per meshlet burns
//      `kLodHoldTicks` in a single frame and the hysteresis silently stops
//      existing -- and every rung it reported would still be a legal rung.
//   3. THE FRAME BOUNDARY RE-ARMS THE SAME INSTANCE. A creature drawn last in
//      frame N and first in frame N+1 must tick again.
//   4. A BANK MISS DOES NOT TICK AND DOES NOT CORRUPT. An unknown form leaves
//      that instance's ladder exactly as it was, which the model check after it
//      proves.
//   5. BEHIND THE EYE DOES NOT TICK. `zref` skips the creature entirely; the
//      state must stand, not fall to the coarsest rung.
//   6. THE CASTER CARRIES THE RUNG WITH ITS OWN INSTANCE. Instance id, world x
//      and z, the type's bound radius and the rung are checked together on
//      every emission, under a stalled consumer.
//   7. EVERY COUNTER IS SEEN TO MOVE, including `out_of_range_o` and
//      `dropped_o`.
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "Vtb_geom_lodstate.h"
#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_creature_page.hpp"
#include "zref/zref_fixp.hpp"

using zhao::check;
namespace cp = zref::creature_page;
namespace zc = zref::creature;

namespace {

constexpr int32_t kOne = 1 << 16;
constexpr uint32_t kBase = 0x0100'0000u;
constexpr int kInstances = 8;   // the DUT's INSTANCES
constexpr uint8_t kRectWhAddr = 17;

struct Caster {
  uint16_t instance_id = 0;
  int32_t x = 0, z = 0, radius = 0;
  uint8_t rung = 0;
  bool view = false;
};

struct Bench {
  Vtb_geom_lodstate& d;
  std::vector<uint8_t> mem;
  std::vector<Caster> got;

  // the asset window
  // THE PRICE, AND WHAT THIS BENCH CAN AND CANNOT SEE (FLOPARRAY, 2026-09-26).
  //
  // `st_q` moved out of flip-flops into an M10K (9,216 bits, 10,826 -> 2,118
  // registers on the block), which required a REGISTERED read where production
  // had a combinational read-modify-write. A registered read normally inserts a
  // pipeline stage, so the honest question is what that costs.
  //
  // BOTH COUNTERS BELOW ARE INSENSITIVE TO A STAGE ON THIS PATH, and that was
  // MEASURED, not assumed: a deliberately inserted EXTRA read register --
  // strictly more latency than the conversion adds -- produced
  //
  //     rungs mesh=60 micro=93 splat=21 glint=66, ticks=286, casters=286,
  //     clocks=106827, busy=43420, 40 checks passed
  //
  // which is byte-identical to production. So DO NOT quote `clocks` or `busy`
  // as proof that the conversion is free; they cannot resolve one cycle here.
  //
  // What the control DOES establish is the thing that matters: this path
  // carries at least two cycles of slack, because adding a whole extra stage
  // changes nothing observable. The structural reason is that `slot_c` is
  // stable from the exit of S_IDLE through S_LOD -- `idx_q` latches on the way
  // out of S_IDLE and `view_q` only ever changes on a transition to
  // S_PROJ/S_IDLE, never on S_RAD -> S_LOD -- and the ladder's own multi-cycle
  // evaluation absorbs the rest.
  //
  // The counters are kept anyway: they are what makes the insensitivity a
  // recorded negative result instead of something the next person rediscovers.
  long g_clocks = 0;
  long g_busy = 0;

int beats_left = 0;
  uint32_t beat_addr = 0;

  // the instance centre's projection, answered from a queue of one
  bool proj_pending = false;
  uint32_t proj_w = 0;
  bool proj_behind = false;
  int proj_delay = 0;
  // PER VIEW, because owner ruling R74 / D-LADDER-A makes a dual-view job two
  // projections through two different cameras -- and the case that matters is
  // the one where they ANSWER DIFFERENTLY (behind camera 0, in front of camera
  // 1). A single held answer could not express it, and the block's per-camera
  // `no_radius` path would have looked correct against a model that could not
  // tell the two apart.
  uint32_t next_w[2] = {0, 0};
  bool next_behind[2] = {false, false};
  int proj_stall = 0;        // hold pr_ready low for this many offers
  bool consumer_stall = false;

  explicit Bench(Vtb_geom_lodstate& dut) : d(dut) {}

  uint64_t word(uint32_t addr) const {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
      const size_t o = static_cast<size_t>(addr - kBase) + static_cast<size_t>(i);
      const uint8_t b = o < mem.size() ? mem[o] : 0xEEu;
      v |= static_cast<uint64_t>(b) << (8 * i);
    }
    return v;
  }

  void idle() {
    d.cfg_we_i = 0;
    d.cfg_addr_i = 0;
    d.cfg_data_i = 0;
    d.cfg_view_i = 0;
    d.pub_valid_i = 0;
    d.pub_tag_i = 0;
    d.pub_base_i = 0;
    d.pub_extent_i = 0;
    d.frame_i = 0;
    d.j_fire_i = 0;
    d.j_instance_id_i = 0;
    d.j_form_index_i = 0;
    d.j_cx_i = 0;
    d.j_cy_i = 0;
    d.j_cz_i = 0;
    d.j_view_mask_i = 0;
  }

  void cycle() {
    // the asset window
    d.rsp_ready_i = 0;
    d.rsp_ok_i = 0;
    d.rsp_violation_i = 0;
    d.beat_valid_i = 0;
    d.beat_data_i = 0;
    if (d.req_valid_o && beats_left == 0) {
      d.rsp_ready_i = 1;
      d.rsp_ok_i = 1;
      beat_addr = d.req_addr_o;
      beats_left = 8;
    } else if (beats_left > 0) {
      d.beat_valid_i = 1;
      d.beat_data_i = word(beat_addr);
      beat_addr += 8;
      --beats_left;
    }

    // client A, as a service with a latency
    d.pr_ready_i = 0;
    d.pr_ans_valid_i = 0;
    d.pr_w_i = 0;
    d.pr_behind_i = 0;
    if (!proj_pending) {
      if (proj_stall > 0) {
        --proj_stall;
      } else if (d.pr_valid_o) {
        d.pr_ready_i = 1;
        proj_pending = true;
        proj_delay = 12;          // the projector's core latency, abbreviated
        const int v = d.pr_view_o ? 1 : 0;
        proj_w = next_w[v];
        proj_behind = next_behind[v] ? 1 : 0;
      }
    } else if (proj_delay > 0) {
      --proj_delay;
    } else {
      d.pr_ans_valid_i = 1;
      d.pr_w_i = proj_w & 0x7FFFFFFFu;
      d.pr_behind_i = proj_behind ? 1 : 0;
      proj_pending = false;
    }

    d.c_ready_i = consumer_stall ? 0 : 1;
    d.eval();
    const bool fire = d.c_valid_o && d.c_ready_i;
    Caster c;
    if (fire) {
      c.instance_id = static_cast<uint16_t>(d.c_instance_id_o);
      c.x = static_cast<int32_t>(d.c_x_o);
      c.z = static_cast<int32_t>(d.c_z_o);
      c.radius = static_cast<int32_t>(d.c_radius_o);
      c.rung = static_cast<uint8_t>(d.c_rung_o);
      c.view = d.c_view_o != 0;
    }
    if (d.busy_o) ++g_busy; zhao::tick(d); ++g_clocks;
    if (fire) got.push_back(c);
    idle();
    d.eval();
  }

  void run(int n) {
    for (int i = 0; i < n; ++i) cycle();
  }

  void cfg(int view, uint8_t addr, uint32_t data) {
    d.cfg_we_i = 1;
    d.cfg_view_i = view ? 1 : 0;
    d.cfg_addr_i = addr;
    d.cfg_data_i = data;
    d.eval();
    if (d.busy_o) ++g_busy; zhao::tick(d); ++g_clocks;
    idle();
    d.eval();
  }

  void publish(uint8_t kind, uint32_t base, uint32_t extent) {
    d.pub_valid_i = 1;
    d.pub_tag_i = kind;
    d.pub_base_i = base;
    d.pub_extent_i = extent;
    d.eval();
    if (d.busy_o) ++g_busy; zhao::tick(d); ++g_clocks;
    idle();
    d.eval();
  }

  void frame() {
    d.frame_i = 1;
    d.eval();
    if (d.busy_o) ++g_busy; zhao::tick(d); ++g_clocks;
    idle();
    d.eval();
  }

  /** One observed job. `w`/`behind` are what the projector will answer for
   *  this instance's centre. */
  void job(uint16_t iid, uint32_t form, int32_t cx, int32_t cy, int32_t cz, int view,
           uint32_t w, bool behind) {
    next_w[0] = next_w[1] = w;
    next_behind[0] = next_behind[1] = behind;
    d.j_fire_i = 1;
    d.j_instance_id_i = iid;
    d.j_form_index_i = form & cp::kFormIndexMask;
    d.j_cx_i = cx;
    d.j_cy_i = cy;
    d.j_cz_i = cz;
    d.j_view_mask_i = static_cast<uint8_t>(view ? 2 : 1);
    d.eval();
    if (d.busy_o) ++g_busy; zhao::tick(d); ++g_clocks;
    idle();
    d.eval();
  }

  /** One observed job with an explicit two-view MASK and a per-camera
   *  projector answer. Owner ruling R74 / D-LADDER-A: mask 2'b11 is TWO
   *  evaluations, and the two cameras may legitimately answer differently --
   *  a creature behind camera 0 can be in front of camera 1. */
  void job_mask(uint16_t iid, uint32_t form, int32_t cx, int32_t cy, int32_t cz,
                uint8_t mask, uint32_t w0, bool behind0, uint32_t w1,
                bool behind1) {
    next_w[0] = w0;
    next_behind[0] = behind0;
    next_w[1] = w1;
    next_behind[1] = behind1;
    d.j_fire_i = 1;
    d.j_instance_id_i = iid;
    d.j_form_index_i = form & cp::kFormIndexMask;
    d.j_cx_i = cx;
    d.j_cy_i = cy;
    d.j_cz_i = cz;
    d.j_view_mask_i = mask;
    d.eval();
    if (d.busy_o) ++g_busy; zhao::tick(d); ++g_clocks;
    idle();
    d.eval();
  }

  /** Both cameras draw this instance: mask 2'b11. */
  void job_dual(uint16_t iid, uint32_t form, int32_t cx, int32_t cy, int32_t cz,
                uint32_t w0, bool behind0, uint32_t w1, bool behind1) {
    job_mask(iid, form, cx, cy, cz, 0x3, w0, behind0, w1, behind1);
  }
};

cp::Record mkrec(uint32_t form, int32_t bound, int32_t micro) {
  cp::Record r;
  r.form_index = form;
  r.bound_radius = bound;
  r.micro_error = micro;
  r.splat_error = bound / 2;   // compile_creature's own relation
  r.glint_error = bound;
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vtb_geom_lodstate;  // heap + exit_hard: see zhao_sim.hpp
  Bench b(*top);
  b.idle();
  top->rsp_ready_i = 0;
  top->rsp_ok_i = 0;
  top->rsp_violation_i = 0;
  top->beat_valid_i = 0;
  top->pr_ready_i = 0;
  top->pr_ans_valid_i = 0;
  top->c_ready_i = 1;
  top->thresh0_i = 0;
  top->thresh1_i = 0;
  top->rst_n = 0;
  top->eval();
  for (int i = 0; i < 3; ++i) zhao::tick(*top);
  top->rst_n = 1;
  top->eval();

  // ---- the camera -----------------------------------------------------------
  // kx = 1.75 in element m[0][0]; a 256-pixel-wide viewport.
  const uint32_t kx_raw = static_cast<uint32_t>(1.75 * kOne);
  b.cfg(0, 0, kx_raw);
  b.cfg(0, 1, static_cast<uint32_t>(kOne / 2));
  b.cfg(0, 2, static_cast<uint32_t>(kOne / 4));
  b.cfg(0, kRectWhAddr, 256u);
  check(top->kx0_o == kx_raw, "the snooper has the camera", kx_raw, top->kx0_o);
  check(top->vw0_o == 256, "and the viewport", 256, top->vw0_o);

  // ---- the ladder page ------------------------------------------------------
  // Four creature types: a 0.75 m flier, a 1 m ordinary, a 2.5 m hero and a
  // 6 m siege beast. Their errors follow `compile_creature`'s own relations.
  std::vector<cp::Record> recs;
  recs.push_back(mkrec(0x000100u, (3 * kOne) / 4, 1024));
  recs.push_back(mkrec(0x000101u, kOne, 1311));
  recs.push_back(mkrec(0x000102u, (5 * kOne) / 2, 4096));
  recs.push_back(mkrec(0x000103u, 6 * kOne, 9000));
  b.mem = cp::build(recs);
  b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
  b.run(300);
  check(top->bank_pages_o == 1, "the ladder page loaded", 1, top->bank_pages_o);
  check(top->bank_records_o == 4, "with four creature types", 4, top->bank_records_o);

  // ---- the governor's threshold: 2.0 px on view 0 --------------------------
  top->thresh0_i = 2 * 256;
  top->thresh1_i = 1 * 256;
  top->eval();

  // The C++ model, one LodState per instance, exactly as the RTL holds one.
  std::map<uint16_t, zc::LodState> model;
  std::map<uint16_t, zc::CreatureType> types;
  for (int i = 0; i < 4; ++i) {
    zc::CreatureType t;
    t.bound_radius = recs[i].bound_radius;
    t.micro_error = recs[i].micro_error;
    t.splat_error = recs[i].splat_error;
    t.glint_error = recs[i].glint_error;
    types[static_cast<uint16_t>(i)] = t;
  }

  // The radius law, in the form the RTL computes it -- but its VALUE is not
  // what is under test here (geom_projradius_directed owns that). What is under
  // test is the ladder state, so the model is stepped with the RTL's own
  // radius, computed the same way from the same inputs.
  auto radius_of = [&](int32_t bound, uint32_t w) {
    const uint64_t rnum = static_cast<uint64_t>(kx_raw) * static_cast<uint64_t>(bound)
                        * 256ull * 128ull;
    const uint64_t rden = static_cast<uint64_t>(w) << 16;
    return static_cast<int32_t>((rnum + rden / 2) / rden);
  };

  // ---- 1/2/3. sixty frames, four creatures walking in ----------------------
  {
    int32_t depth[4] = {300 * kOne, 220 * kOne, 160 * kOne, 90 * kOne};
    const int32_t step[4] = {-4 * kOne, -3 * kOne, -2 * kOne, -1 * kOne};
    uint32_t ticks_want = 0, skipped_want = 0;
    for (int f = 0; f < 60; ++f) {
      b.frame();
      for (int i = 0; i < 4; ++i) {
        const uint16_t iid = static_cast<uint16_t>(i);
        const uint32_t form = 0x000100u + static_cast<uint32_t>(i);
        const int32_t cx = (i + 1) * kOne;
        const int32_t cz = depth[i];
        const uint32_t w = static_cast<uint32_t>(depth[i]);
        // ONE DRAW, EIGHT MESHLETS. Only the first is a tick.
        for (int m = 0; m < 8; ++m) {
          b.job(iid, form, cx, 0, cz, 0, w, false);
          if (m == 0) ++ticks_want;
          else ++skipped_want;
          b.run(4);
        }
        b.run(320);   // let the evaluation finish before the next instance
        // step the model with the same radius the RTL saw
        const int32_t r = radius_of(types[iid].bound_radius, w);
        zc::lod_update(model[iid], r, 2 * 256, types[iid]);
        depth[i] += step[i];
        if (depth[i] < 4 * kOne) depth[i] = 4 * kOne;
      }
    }
    b.run(400);
    check(top->ticks_o == ticks_want, "one tick per DRAW, not per meshlet", ticks_want,
          top->ticks_o);
    check(top->skipped_repeat_o == skipped_want,
          "and the other seven meshlets of each draw are counted as repeats",
          skipped_want, top->skipped_repeat_o);
    check(top->dropped_o == 0, "nothing was dropped at this rate", 0, top->dropped_o);
    check(b.got.size() == ticks_want, "every tick emitted one caster", ticks_want,
          b.got.size());

    // Every caster, in order, against the model run alongside it.
    {
      std::map<uint16_t, zc::LodState> replay;
      int32_t d2[4] = {300 * kOne, 220 * kOne, 160 * kOne, 90 * kOne};
      size_t k = 0;
      bool all_match = true;
      int mismatches = 0;
      for (int f = 0; f < 60 && k < b.got.size(); ++f) {
        for (int i = 0; i < 4 && k < b.got.size(); ++i, ++k) {
          const uint16_t iid = static_cast<uint16_t>(i);
          const int32_t r = radius_of(types[iid].bound_radius, static_cast<uint32_t>(d2[i]));
          const zc::LodRung want = zc::lod_update(replay[iid], r, 2 * 256, types[iid]);
          const Caster& g = b.got[k];
          if (g.instance_id != iid || g.rung != static_cast<uint8_t>(want) ||
              g.x != (i + 1) * kOne || g.z != d2[i] ||
              g.radius != types[iid].bound_radius) {
            all_match = false;
            if (++mismatches <= 3) {
              std::printf(
                  "  frame %d instance %d: rung want %d got %d, z want %d got %d\n", f, i,
                  static_cast<int>(want), g.rung, d2[i], g.z);
            }
          }
          d2[i] += step[i];
          if (d2[i] < 4 * kOne) d2[i] = 4 * kOne;
        }
      }
      check(all_match,
            "every caster matches zref::creature::lod_update, frame by frame, "
            "instance by instance -- rung, world x, world z and bound radius",
            1, all_match);
    }

    // The ladder actually MOVED. A test in which every creature stayed at rung
    // 0 would pass all of the above and prove nothing.
    const uint32_t settled = top->rung0_o + top->rung1_o + top->rung2_o + top->rung3_o;
    check(settled == ticks_want, "every tick settled at some rung", ticks_want, settled);
    int rungs_used = 0;
    if (top->rung0_o) ++rungs_used;
    if (top->rung1_o) ++rungs_used;
    if (top->rung2_o) ++rungs_used;
    if (top->rung3_o) ++rungs_used;
    check(rungs_used >= 2, "and the ladder visited more than one rung", 1,
          rungs_used >= 2);
    std::printf("[geom_lodstate_directed] rungs: mesh=%u micro=%u splat=%u glint=%u\n",
                top->rung0_o, top->rung1_o, top->rung2_o, top->rung3_o);
  }

  // ---- 4. a bank MISS does not tick and does not corrupt -------------------
  {
    const uint32_t ticks_before = top->ticks_o;
    const uint32_t miss_before = top->bank_miss_o;
    const size_t got_before = b.got.size();
    b.frame();
    b.job(1, 0x00DEADu, kOne, 0, 50 * kOne, 0, 50u * static_cast<uint32_t>(kOne), false);
    b.run(400);
    check(top->bank_miss_o == miss_before + 1, "an unknown form is a counted MISS",
          miss_before + 1, top->bank_miss_o);
    check(top->ticks_o == ticks_before, "and the ladder is NOT ticked", ticks_before,
          top->ticks_o);
    check(b.got.size() == got_before, "and no caster is emitted", got_before,
          b.got.size());
    // Instance 1's state is untouched: the next honest tick continues the model.
    b.frame();
    b.job(1, 0x000101u, kOne, 0, 50 * kOne, 0, 50u * static_cast<uint32_t>(kOne), false);
    b.run(400);
    const int32_t r = radius_of(types[1].bound_radius, 50u * static_cast<uint32_t>(kOne));
    const zc::LodRung want = zc::lod_update(model[1], r, 2 * 256, types[1]);
    check(b.got.size() == got_before + 1, "the honest tick after it DOES emit",
          got_before + 1, b.got.size());
    check(b.got.back().rung == static_cast<uint8_t>(want),
          "and continues instance 1's ladder from where the miss left it",
          static_cast<uint64_t>(want), b.got.back().rung);
  }

  // ---- 5. behind the eye does not tick ------------------------------------
  {
    const uint32_t ticks_before = top->ticks_o;
    const uint32_t nor_before = top->no_radius_o;
    const size_t got_before = b.got.size();
    b.frame();
    b.job(2, 0x000102u, kOne, 0, -20 * kOne, 0, 0u, true);
    b.run(400);
    check(top->no_radius_o == nor_before + 1,
          "behind the eye is counted and does NOT tick", nor_before + 1,
          top->no_radius_o);
    check(top->ticks_o == ticks_before, "the ladder stands", ticks_before, top->ticks_o);
    check(b.got.size() == got_before, "and nothing is cast", got_before, b.got.size());
    check(top->rad_behind_o > 0, "the radius service's own behind counter fired", 1,
          top->rad_behind_o > 0);
  }

  // ---- 7. out of range, and a drop ----------------------------------------
  {
    const uint32_t oor_before = top->out_of_range_o;
    b.frame();
    b.job(static_cast<uint16_t>(kInstances + 3), 0x000101u, 0, 0, 40 * kOne, 0,
          40u * static_cast<uint32_t>(kOne), false);
    b.run(40);
    check(top->out_of_range_o == oor_before + 1,
          "an instance id at or above INSTANCES is REFUSED, never wrapped",
          oor_before + 1, top->out_of_range_o);
  }
  {
    // Two different instances back to back with no time between: the second is
    // dropped, and COUNTED, because the tap cannot stall what it watches.
    const uint32_t drop_before = top->dropped_o;
    b.frame();
    b.job(3, 0x000103u, 0, 0, 30 * kOne, 0, 30u * static_cast<uint32_t>(kOne), false);
    b.run(2);
    check(top->busy_o != 0, "the first evaluation is in flight", 1, top->busy_o);
    b.job(0, 0x000100u, 0, 0, 30 * kOne, 0, 30u * static_cast<uint32_t>(kOne), false);
    b.run(400);
    check(top->dropped_o == drop_before + 1,
          "a new instance arriving while busy is DROPPED and counted, never silent",
          drop_before + 1, top->dropped_o);
  }

  // ---- 6. the caster is held under a stalled consumer ---------------------
  {
    const size_t got_before = b.got.size();
    b.consumer_stall = true;
    b.frame();
    b.job(0, 0x000100u, 7 * kOne, 0, 33 * kOne, 0, 33u * static_cast<uint32_t>(kOne),
          false);
    b.run(400);
    check(top->c_valid_o != 0, "the caster is offered and HELD", 1, top->c_valid_o);
    check(b.got.size() == got_before, "and not taken", got_before, b.got.size());
    const int32_t held_x = static_cast<int32_t>(top->c_x_o);
    b.run(100);
    check(static_cast<int32_t>(top->c_x_o) == held_x, "its fields do not move", 1,
          static_cast<int32_t>(top->c_x_o) == held_x);
    b.consumer_stall = false;
    b.run(20);
    check(b.got.size() == got_before + 1, "and it is taken when the consumer returns",
          got_before + 1, b.got.size());
    check(b.got.back().x == 7 * kOne, "with its own world x", 7 * kOne,
          static_cast<uint64_t>(b.got.back().x));
  }

  // ---- 7. OWNER RULING R74 / D-LADDER-A: one ladder per (INSTANCE, CAMERA) -
  //
  // "Decided: one more bit x INSTANCES, so each creature's ladder measures
  //  against the right camera ... For `active_mask == 2'b11` there was no
  //  honest answer in the tree."
  //
  // The two thresholds set above are DIFFERENT on purpose -- view 0 is 2.0 px
  // and view 1 is 1.0 px -- so a creature at one distance can legitimately sit
  // at two different rungs. That is the whole point of paying the bit, and a
  // test run with equal thresholds would pass against a block that still held
  // ONE ladder.
  {
    const uint32_t ticks_before = top->ticks_o;
    const size_t got_before = b.got.size();

    b.frame();
    // Both cameras draw instance 5. One job, mask 2'b11.
    b.job_dual(5, 0x000102u, 11 * kOne, 0, 40 * kOne,
               40u * static_cast<uint32_t>(kOne), false,
               40u * static_cast<uint32_t>(kOne), false);
    b.run(600);

    check(top->ticks_o == ticks_before + 2,
          "a dual-view job ticks TWICE -- once per camera", ticks_before + 2,
          top->ticks_o);
    check(b.got.size() == got_before + 2, "and emits TWO casters",
          got_before + 2, b.got.size());
    const Caster &c0 = b.got[got_before];
    const Caster &c1 = b.got[got_before + 1];
    check(!c0.view && c1.view,
          "VIEW 0 FIRST, then view 1 -- the declared emission order, so a "
          "capture CRC does not move for a reason nobody authored",
          1, (!c0.view && c1.view) ? 1 : 0);
    check(c0.instance_id == 5 && c1.instance_id == 5,
          "both name the same instance", 5, c0.instance_id);
    check(c0.x == c1.x && c0.z == c1.z && c0.radius == c1.radius,
          "the world position and bound radius are the INSTANCE's, not the "
          "camera's -- only the rung may differ",
          1, (c0.x == c1.x && c0.z == c1.z && c0.radius == c1.radius) ? 1 : 0);
    // The finer threshold demands the finer rung, so view 1's rung is <= view
    // 0's. Asserting the INEQUALITY rather than two literals keeps this a test
    // of the per-camera law and not of one distance's arithmetic, which
    // geom_lod_directed already owns.
    check(c1.rung <= c0.rung,
          "the FINER threshold (view 1, 1.0 px) never picks a COARSER rung",
          1, (c1.rung <= c0.rung) ? 1 : 0);
  }

  // ---- 8. THE TWO LADDERS ARE GENUINELY SEPARATE STORAGE -------------------
  //
  // The case above could still pass against a single shared ladder that simply
  // re-evaluated twice. THIS is the one that cannot: drive view 0 alone for
  // long enough that its hysteresis settles at a coarse rung, then ask view 1
  // for the FIRST time at the same distance. A shared store would hand view 1
  // the rung view 0 walked to; separate stores start view 1 at kMesh and let
  // its own hold count up. That is the "creature that pops LOD rungs in the
  // second view for reasons nothing records" the ruling names.
  {
    b.frame();
    for (int f = 0; f < 40; ++f) {
      b.frame();
      b.job(6, 0x000103u, 0, 0, 400 * kOne, 0,
            400u * static_cast<uint32_t>(kOne), false);
      b.run(400);
    }
    const uint8_t v0_settled = b.got.back().rung;

    const size_t before = b.got.size();
    b.frame();
    b.job(6, 0x000103u, 0, 0, 400 * kOne, 1,
          400u * static_cast<uint32_t>(kOne), false);
    b.run(400);
    check(b.got.size() == before + 1, "view 1's first evaluation emitted",
          before + 1, b.got.size());
    check(b.got.back().view, "and it is tagged view 1", 1,
          b.got.back().view ? 1 : 0);
    // zref::creature::LodState initialises to kMesh (rung 0) with hold 0, and
    // the stability law needs kLodHoldTicks before it may leave. So view 1's
    // very first answer is rung 0 whatever view 0 walked to.
    check(b.got.back().rung == 0,
          "VIEW 1 STARTS AT kMESH -- it did not inherit view 0's settled rung, "
          "which is the Duo fairness defect D-LADDER-A was ruled to remove",
          0, b.got.back().rung);
    std::printf(
        "[geom_lodstate_directed] view0 settled at rung %u; view1's first "
        "answer was rung %u\n",
        v0_settled, b.got.back().rung);
  }

  // ---- 9. A JOB NO CAMERA DRAWS EVALUATES NOTHING -------------------------
  // Defensive: zhao_geom_drawjob does not emit a fully masked job (its own
  // `masked_o` counts them). Written as a case rather than assumed away.
  {
    const uint32_t ticks_before = top->ticks_o;
    b.frame();
    b.job_mask(7, 0x000100u, 0, 0, 50 * kOne, 0,
               50u * static_cast<uint32_t>(kOne), false,
               50u * static_cast<uint32_t>(kOne), false);
    b.run(400);
    check(top->ticks_o == ticks_before,
          "a mask of 2'b00 ticks no ladder at all", ticks_before, top->ticks_o);
    check(top->busy_o == 0, "and leaves the block idle rather than wedged", 0,
          top->busy_o);
  }

  // ---- every counter moved -------------------------------------------------
  check(top->ticks_o > 0 && top->skipped_repeat_o > 0 && top->bank_miss_o > 0
            && top->no_radius_o > 0 && top->dropped_o > 0 && top->out_of_range_o > 0
            && top->rad_evaluations_o > 0 && top->rad_behind_o > 0,
        "every counter this block owns was FIRED by stimulus", 1,
        top->ticks_o > 0 && top->skipped_repeat_o > 0 && top->bank_miss_o > 0
            && top->no_radius_o > 0 && top->dropped_o > 0 && top->out_of_range_o > 0
            && top->rad_evaluations_o > 0 && top->rad_behind_o > 0);

  std::printf(
      "[geom_lodstate_directed] ticks=%u repeats=%u miss=%u no_radius=%u dropped=%u "
      "out_of_range=%u casters=%zu\n",
      top->ticks_o, top->skipped_repeat_o, top->bank_miss_o, top->no_radius_o,
      top->dropped_o, top->out_of_range_o, b.got.size());
  top->final();
  std::printf("[geom_lodstate_directed] clocks=%ld busy=%ld\n", b.g_clocks, b.g_busy);
  return zhao::report_and_exit("geom_lodstate_directed");
}
