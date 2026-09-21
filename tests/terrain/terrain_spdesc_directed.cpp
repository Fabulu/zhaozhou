// terrain_spdesc_directed.cpp -- TERRAIN.SPDESC, the subpatch descriptor
// assembler that `zhao_console_core.sv` entry I21 needs and that gz/jobissue
// named on 2026-09-21 as the last block between a BUILT TERRAIN group and a
// COMPOSED one.
//
// Contract: design/contracts/TERRAIN.SPDESC.md.
//
// WHAT THIS GUARDS, and every one is silent in a result-only test:
//
//   * A DESCRIPTOR CARRYING ANOTHER PATCH'S POSITION makes TERRAIN.LOD measure
//     the camera distance to the wrong place, so a near patch is tessellated
//     coarse and a far one fine. Nothing downstream can tell: every level is a
//     legal level and every triangle is a real triangle. Case 2 runs two
//     patches with DIFFERENT slots, DIFFERENT src_ids and DIFFERENT placements
//     back to back and requires patch B's sixteen descriptors to carry B's
//     positions AND B's deviations -- the whole reason this block exists
//     rather than a wire.
//   * A CENTRE PAIRED WITH THE WRONG SUBPATCH'S DEVIATIONS is the same fault
//     one level down: subpatch 3's stored deviation against subpatch 4's
//     centre. Case 1 checks all sixteen pairings against the model, by value.
//   * A PASS-THROUGH THAT DROPS THE UPSTREAM CLIENT'S REQUEST. The compose
//     cache's `lat_req_i` HAS NO READY, so a request that is not forwarded on
//     the cycle it is made is not delayed, it is DESTROYED -- and TESS would
//     read a stale datum with every counter agreeing. Case 4 drives the
//     upstream port on EVERY cycle of a patch and requires that this block
//     forwarded all of it, unaltered, while still assembling correctly.
//   * A CENTRE SAMPLED AT THE WRONG VERTEX. `zhao_terrain_lodfeed` sampled the
//     centre HEIGHT at (ox+4, oz+4); a block reading x and z at (ox, oz) would
//     produce a descriptor whose y belongs to a different point than its x and
//     z. Case 1 requires the exact vertex.
//
// R95: EVERY FAULT COUNTER IS FIRED ON PURPOSE AND SHOWN SILENT BESIDE IT.
// All five are reachable from this block's own boundary with legal stimulus,
// so NO COMMITTED MUTANT IS OWED here -- and each fire is paired with a
// negative control in the same case, because a counter that fires on
// everything is as useless as one that fires on nothing.
//
//   door_refused_o        case 5   a fifth door entry against DOORD = 4
//   serve_no_door_o       case 6   a serve with an empty door queue
//   door_src_mismatch_o   case 7   a serve whose src_id is not the head's
//   patches_unfresh_o     case 8   a slot the store has no records for
//   sp_order_bad_o        case 9   a store that re-orders its own read
//
//   store_wait_clocks_o   case 10  climbs while the store is slow, flat at rest
//   lat_wait_clocks_o     case 4   climbs while the upstream client holds the bus
//
// TWO CLOCK INSTRUMENTS, NOT FAULT FLAGS, and this is `zhao_terrain_jobissue`'s
// correction from the same run applied BEFORE the mistake instead of after it.
// Both are non-zero in ordinary operation -- a busy tessellator and a store
// answering at its own pace are not faults -- so neither may be read as one.
// The pair to read is either climbing while `patches_assembled_o` is FLAT.
//
// CASE 8 RECORDS A DEFECT THIS TEST FOUND IN THE BLOCK IT TESTS, which is the
// most useful thing here. `patches_unfresh_o` was first sampled in `StStart`,
// on the cycle `r_start_o` is presented. `zhao_terrain_devstore` loads
// `r_fresh_q` INSIDE its own `R_IDLE: if (r_start_i)` arm, so on that cycle
// the port still carries the PREVIOUS patch's answer. The counter therefore
// attributed page A's freshness to page B, and it did it silently, in the
// flattering direction whenever A was fresh. Case 8 runs a fresh patch and
// then an unfresh one back to back specifically so the stale reading would
// score ZERO, and it did before the repair.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_spdesc.h"

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

// One descriptor as `zhao_terrain_lod` receives it on its `sp_*` port.
struct Desc {
  int32_t cx, cy, cz;
  uint32_t dev1, dev2, dev3;
  uint32_t prev_level, prev_morph, hold;
  uint32_t src_id;
};

// ---------------------------------------------------------------------------
// THE MODELS
// ---------------------------------------------------------------------------
// `zhao_terrain_devstore`'s read port, with its real timing: `r_ready_o` is
// high only at rest, a start costs one FETCH cycle before the first record,
// and `r_fresh_o` is loaded BY the start (so it is the previous patch's value
// on the start cycle itself -- which is what case 8 is about).
struct StoreModel {
  enum { IDLE, FETCH, STREAM } st = IDLE;
  uint32_t slot = 0;
  uint32_t ptr = 0;
  bool fresh = false;

  // Per-slot content, so a case can tell two slots apart by value.
  bool fresh_of_slot[16] = {};
  // How many cycles to sit in FETCH; 1 is the real store. Case 10 raises it.
  int fetch_cost = 1;
  int fetch_left = 0;
  // Case 9 makes the store re-order its own read.
  bool scramble = false;

  uint32_t dev1(uint32_t sp) const { return 0x001000u + slot * 0x100u + sp; }
  uint32_t dev2(uint32_t sp) const { return 0x002000u + slot * 0x100u + sp; }
  uint32_t dev3(uint32_t sp) const { return 0x003000u + slot * 0x100u + sp; }
  int16_t cy(uint32_t sp) const {
    return static_cast<int16_t>(static_cast<int>(slot) * 37 + static_cast<int>(sp) * 11 - 200);
  }
  uint32_t prev_level(uint32_t sp) const { return (sp + slot) & 3u; }
  uint32_t prev_morph(uint32_t sp) const { return (0x100u + sp * 3u + slot) & 0x1FFFFu; }
  uint32_t hold(uint32_t sp) const { return (sp * 5u + slot) & 0xFFu; }

  // The `sp` this model offers for walk position n.
  uint32_t sp_at(uint32_t n) const { return scramble ? ((n + 1u) & 15u) : n; }
};

// `zhao_terrain_compcache_front`'s lattice port: the datum is present the
// cycle AFTER the request, and `lat_wx_o`/`lat_wz_o` are the placed column and
// row positions, indexed by vi and vj alone.
struct LatticeModel {
  // Which patch's placement is on the SERVE parity right now. Changed between
  // patches so a descriptor carrying the wrong patch's position is visible.
  int32_t base = 0;

  int32_t wx(uint32_t vi) const { return base + static_cast<int32_t>(vi) * 0x10000; }
  int32_t wz(uint32_t vj) const { return base + 0x400000 + static_cast<int32_t>(vj) * 0x10000; }
};

// The reference answer: what descriptor n of a patch MUST be.
Desc expected(const StoreModel& store, const LatticeModel& lat, uint32_t sp, uint32_t src_id) {
  Desc d{};
  const uint32_t ox = (sp & 3u) * 8u;
  const uint32_t oz = (sp >> 2) * 8u;
  d.cx = lat.wx(ox + 4u);
  d.cz = lat.wz(oz + 4u);
  // height16 -> fx16 is `raw << 8`, EXACT (spec/qformats.md 9).
  d.cy = static_cast<int32_t>(store.cy(sp)) * 256;
  d.dev1 = store.dev1(sp);
  d.dev2 = store.dev2(sp);
  d.dev3 = store.dev3(sp);
  d.prev_level = store.prev_level(sp);
  d.prev_morph = store.prev_morph(sp);
  d.hold = store.hold(sp);
  d.src_id = src_id;
  return d;
}

class Rig {
 public:
  Vzhao_terrain_spdesc t;
  StoreModel store;
  LatticeModel lat;

  std::vector<Desc> descs;

  // The sink's readiness, so a case can stall TERRAIN.LOD.
  bool sp_ready = true;

  // The upstream lattice client (TESS, through HEIGHTTAP). When it asks, this
  // block MUST forward its request untouched on the same cycle.
  bool up_req = false;
  uint32_t up_vi = 0, up_vj = 0, up_surface = 0;
  int up_requests = 0;
  int up_forwarded = 0;
  int up_corrupted = 0;

  // The lattice response pipeline: what the cache will present next cycle.
  bool rsp_pending = false;
  uint32_t rsp_vi = 0, rsp_vj = 0;

  // THE PLANT FOR CASE 8. A fire test proves nothing until the state it needs
  // is shown to be REACHED (CLAUDE.md: "a gate that cannot reach the state is
  // not evidence about the state"). These record, on the cycle `r_start_o` is
  // presented, whether the freshness ON THE PORT differs from the freshness OF
  // THE SLOT BEING STARTED. If that never happens, case 8 cannot tell a
  // start-cycle sample from a stream-cycle one and its pass is worthless.
  int start_cycles = 0;
  int start_cycles_where_fresh_differs = 0;

  void quiet() {
    t.door_valid_i = 0;
    t.door_slot_i = 0;
    t.door_src_id_i = 0;
    t.door_dual_i = 0;
    t.serve_valid_i = 0;
    t.serve_src_id_i = 0;
    t.o_lat_req_i = 0;
    t.o_lat_vi_i = 0;
    t.o_lat_vj_i = 0;
    t.o_lat_surface_i = 0;
    t.c_lat_h_i = 0;
    t.c_lat_wx_i = 0;
    t.c_lat_wz_i = 0;
    t.r_ready_i = 1;
    t.r_valid_i = 0;
    t.r_sp_i = 0;
    t.r_dev1_i = 0;
    t.r_dev2_i = 0;
    t.r_dev3_i = 0;
    t.r_cy_i = 0;
    t.r_prev_level_i = 0;
    t.r_prev_morph_i = 0;
    t.r_hold_i = 0;
    t.r_fresh_i = 0;
    t.sp_ready_i = 1;
  }

  void reset() {
    quiet();
    t.rst_n = 0;
    for (int i = 0; i < 3; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
  }

  // Drive every model input for this cycle, then evaluate.
  void drive() {
    // ---- the upstream lattice client --------------------------------------
    t.o_lat_req_i = up_req ? 1 : 0;
    t.o_lat_vi_i = up_vi;
    t.o_lat_vj_i = up_vj;
    t.o_lat_surface_i = up_surface;

    // ---- the compose cache's response to LAST cycle's request -------------
    if (rsp_pending) {
      t.c_lat_wx_i = lat.wx(rsp_vi);
      t.c_lat_wz_i = lat.wz(rsp_vj);
      t.c_lat_h_i = 0x0BADF00D;
    } else {
      // POISON, as the cache presents when no request was made. A block that
      // captured on the wrong cycle would take this and it would be loud.
      t.c_lat_wx_i = static_cast<int32_t>(0x7FFFFFFF);
      t.c_lat_wz_i = static_cast<int32_t>(0x7FFFFFFF);
      t.c_lat_h_i = static_cast<int32_t>(0x7FFFFFFF);
    }

    // ---- the store's read port --------------------------------------------
    const bool streaming = (store.st == StoreModel::STREAM);
    t.r_ready_i = (store.st == StoreModel::IDLE) ? 1 : 0;
    t.r_valid_i = streaming ? 1 : 0;
    // `r_fresh_o` is a registered level: it describes the slot being streamed
    // from the cycle after the start, and the PREVIOUS one on the start cycle.
    t.r_fresh_i = store.fresh ? 1 : 0;
    if (streaming) {
      const uint32_t sp = store.sp_at(store.ptr);
      t.r_sp_i = sp;
      t.r_dev1_i = store.dev1(sp);
      t.r_dev2_i = store.dev2(sp);
      t.r_dev3_i = store.dev3(sp);
      t.r_cy_i = store.cy(sp);
      t.r_prev_level_i = store.prev_level(sp);
      t.r_prev_morph_i = store.prev_morph(sp);
      t.r_hold_i = store.hold(sp);
    }

    t.sp_ready_i = sp_ready ? 1 : 0;
    t.eval();
  }

  // Sample what the block emitted this cycle, advance the models, then clock.
  void step() {
    drive();

    // ---- THE PASS-THROUGH'S SAFETY PROPERTY -------------------------------
    // Checked on EVERY cycle of every case, not in one case: a request the
    // upstream client made must appear on the cache port this same cycle,
    // with its own vi/vj/surface. There is no ready to delay it with.
    if (up_req) {
      ++up_requests;
      if (t.c_lat_req_o) ++up_forwarded;
      if (t.c_lat_vi_o != up_vi || t.c_lat_vj_o != up_vj ||
          t.c_lat_surface_o != up_surface) {
        ++up_corrupted;
      }
    }

    if (t.sp_valid_o && t.sp_ready_i) {
      Desc d{};
      d.cx = t.sp_cx_o;
      d.cy = t.sp_cy_o;
      d.cz = t.sp_cz_o;
      d.dev1 = t.sp_dev1_o;
      d.dev2 = t.sp_dev2_o;
      d.dev3 = t.sp_dev3_o;
      d.prev_level = t.sp_prev_level_o;
      d.prev_morph = t.sp_prev_morph_o;
      d.hold = t.sp_hold_o;
      d.src_id = t.sp_src_id_o;
      descs.push_back(d);
    }

    // The cache will answer next cycle whatever was requested this cycle.
    rsp_pending = t.c_lat_req_o != 0;
    rsp_vi = t.c_lat_vi_o;
    rsp_vj = t.c_lat_vj_o;

    // ---- advance the store model ------------------------------------------
    const bool take = t.r_valid_i && t.r_ready_o;
    switch (store.st) {
      case StoreModel::IDLE:
        if (t.r_start_o) {
          ++start_cycles;
          // The port still carries the PREVIOUS patch's flag on this cycle --
          // that is the store's own timing, not a quirk of this model.
          if ((t.r_fresh_i != 0) != store.fresh_of_slot[t.r_slot_o & 15u]) {
            ++start_cycles_where_fresh_differs;
          }
          store.slot = t.r_slot_o;
          store.fresh = store.fresh_of_slot[t.r_slot_o & 15u];
          store.ptr = 0;
          store.fetch_left = store.fetch_cost;
          store.st = StoreModel::FETCH;
        }
        break;
      case StoreModel::FETCH:
        if (--store.fetch_left <= 0) store.st = StoreModel::STREAM;
        break;
      case StoreModel::STREAM:
        if (take) {
          if (store.ptr == 15) {
            store.st = StoreModel::IDLE;
          } else {
            ++store.ptr;
          }
        }
        break;
    }

    zhao::tick(t);
  }

  void idle(int n) {
    for (int i = 0; i < n; ++i) step();
  }

  // Push one {slot, src_id, dual} at the compose door. Returns true if it was
  // taken. `dual` defaults false so every case written before 2026-09-21 means
  // exactly what it meant: a page with no underside is the legacy
  // single-surface page, which is what an unwritten flag should say.
  bool push_door(uint32_t slot, uint32_t src_id, bool dual = false) {
    t.door_valid_i = 1;
    t.door_slot_i = slot;
    t.door_src_id_i = src_id;
    t.door_dual_i = dual ? 1 : 0;
    drive();
    const bool taken = t.door_ready_o != 0;
    step();
    t.door_valid_i = 0;
    return taken;
  }

  // Raise the serve level for one patch, run the assembly, then lower it --
  // which is what the retirement pulse does in the console.
  void serve(uint32_t src_id, int max_cycles = 2000) {
    t.serve_valid_i = 1;
    t.serve_src_id_i = src_id;
    int n = 0;
    // Run until the block goes quiet again, or the budget runs out.
    step();
    while (n++ < max_cycles) {
      step();
      if (!t.busy_o && store.st == StoreModel::IDLE) break;
    }
    t.serve_valid_i = 0;
    idle(2);
  }
};

// ---------------------------------------------------------------------------

void check_patch(Rig& r, size_t first, const StoreModel& store, const LatticeModel& lat,
                 uint32_t src_id, const char* tag) {
  char buf[192];
  for (uint32_t sp = 0; sp < 16; ++sp) {
    if (first + sp >= r.descs.size()) {
      std::snprintf(buf, sizeof buf, "%s: descriptor %u is missing", tag, sp);
      check_true(false, buf);
      return;
    }
    const Desc& got = r.descs[first + sp];
    const Desc want = expected(store, lat, sp, src_id);
    std::snprintf(buf, sizeof buf, "%s sp %u: cx", tag, sp);
    check_eq(static_cast<uint32_t>(got.cx), static_cast<uint32_t>(want.cx), buf);
    std::snprintf(buf, sizeof buf, "%s sp %u: cy", tag, sp);
    check_eq(static_cast<uint32_t>(got.cy), static_cast<uint32_t>(want.cy), buf);
    std::snprintf(buf, sizeof buf, "%s sp %u: cz", tag, sp);
    check_eq(static_cast<uint32_t>(got.cz), static_cast<uint32_t>(want.cz), buf);
    std::snprintf(buf, sizeof buf, "%s sp %u: dev1", tag, sp);
    check_eq(got.dev1, want.dev1, buf);
    std::snprintf(buf, sizeof buf, "%s sp %u: dev2", tag, sp);
    check_eq(got.dev2, want.dev2, buf);
    std::snprintf(buf, sizeof buf, "%s sp %u: dev3", tag, sp);
    check_eq(got.dev3, want.dev3, buf);
    std::snprintf(buf, sizeof buf, "%s sp %u: prev_level", tag, sp);
    check_eq(got.prev_level, want.prev_level, buf);
    std::snprintf(buf, sizeof buf, "%s sp %u: prev_morph", tag, sp);
    check_eq(got.prev_morph, want.prev_morph, buf);
    std::snprintf(buf, sizeof buf, "%s sp %u: hold", tag, sp);
    check_eq(got.hold, want.hold, buf);
    std::snprintf(buf, sizeof buf, "%s sp %u: src_id", tag, sp);
    check_eq(got.src_id, want.src_id, buf);
  }
}

// CASE 1 -- one clean patch, checked by value, with every fault counter silent.
void case1_one_patch() {
  Rig r;
  r.reset();
  r.store.fresh_of_slot[3] = true;
  r.lat.base = 0x00200000;

  check_true(r.push_door(3, 0xA1A1), "case1: the door takes the identity");
  r.serve(0xA1A1);

  check_eq(r.descs.size(), 16, "case1: sixteen descriptors");
  check_eq(r.t.patches_assembled_o, 1, "case1: one patch assembled");
  check_eq(r.t.descriptors_emitted_o, 16, "case1: sixteen emitted");
  check_patch(r, 0, r.store, r.lat, 0xA1A1, "case1");

  // The negative controls for every fault counter in this block.
  check_eq(r.t.door_refused_o, 0, "case1: door_refused silent");
  check_eq(r.t.serve_no_door_o, 0, "case1: serve_no_door silent");
  check_eq(r.t.door_src_mismatch_o, 0, "case1: door_src_mismatch silent");
  check_eq(r.t.sp_order_bad_o, 0, "case1: sp_order_bad silent");
  check_eq(r.t.patches_unfresh_o, 0, "case1: patches_unfresh silent");
  check_eq(r.t.lat_wait_clocks_o, 0, "case1: no lattice contention, so no wait");
  check_true(r.t.busy_o == 0, "case1: idle when done");
}

// CASE 2 -- TWO PATCHES, DIFFERENT SLOTS AND DIFFERENT PLACEMENTS. The reason
// this block exists rather than a wire.
void case2_two_patches() {
  Rig r;
  r.reset();
  r.store.fresh_of_slot[3] = true;
  r.store.fresh_of_slot[9] = true;

  r.lat.base = 0x00200000;
  // PAGE A IS DUAL AND PAGE B IS NOT. That asymmetry is the point: the console
  // used to read this flag off TERRAIN.PAGESTREAM's LIVE `v_flags_o`, which is
  // the FILLING page's -- so with one page filling and another served it was
  // the wrong patch's flag entirely, not merely an unstable one. Here A is
  // served while B is at the door, and `patch_dual_o` must still say A.
  check_true(r.push_door(3, 0xA1A1, true), "case2: door takes A");
  r.serve(0xA1A1);
  const StoreModel store_a = r.store;
  const LatticeModel lat_a = r.lat;

  // A new patch: new slot, new src_id, and the cache's serve parity has
  // swapped to a different placement.
  r.lat.base = 0x05500000;
  check_true(r.push_door(9, 0xB2B2, false), "case2: door takes B");
  // A's flag must still be the one on the port while A is the served patch --
  // sampled BEFORE B is served, because B's push has already happened and a
  // live net would have moved on B's door beat.
  check_eq(r.t.patch_dual_o, 1, "case2: patch_dual_o is A's while A is served");
  r.serve(0xB2B2);
  check_eq(r.t.patch_dual_o, 0, "case2: and B's once B is served");

  check_eq(r.descs.size(), 32, "case2: thirty-two descriptors");
  check_eq(r.t.patches_assembled_o, 2, "case2: two patches assembled");
  check_patch(r, 0, store_a, lat_a, 0xA1A1, "case2 A");
  check_patch(r, 16, r.store, r.lat, 0xB2B2, "case2 B");
  check_eq(r.t.door_src_mismatch_o, 0, "case2: both pairings agreed");
  check_eq(r.t.patches_unfresh_o, 0, "case2: both slots had records");
}

// CASE 3 -- TERRAIN.LOD backpressure. A descriptor must hold, unchanged, and
// be taken exactly once.
void case3_sink_backpressure() {
  Rig r;
  r.reset();
  r.store.fresh_of_slot[3] = true;
  r.lat.base = 0x00200000;
  check_true(r.push_door(3, 0xA1A1), "case3: the door takes the identity");

  r.t.serve_valid_i = 1;
  r.t.serve_src_id_i = 0xA1A1;

  // Run with the sink refusing every other burst.
  int guard = 0;
  int held_cycles = 0;
  while (guard++ < 4000) {
    r.sp_ready = (guard % 7) < 2;   // ready for 2 cycles in every 7
    r.drive();
    if (r.t.sp_valid_o && !r.t.sp_ready_i) ++held_cycles;
    r.step();
    if (!r.t.busy_o && r.store.st == StoreModel::IDLE && r.descs.size() == 16) break;
  }
  r.t.serve_valid_i = 0;
  r.sp_ready = true;
  r.idle(2);

  check_true(held_cycles > 0, "case3: the sink actually refused some offers");
  check_eq(r.descs.size(), 16, "case3: exactly sixteen, none duplicated");
  check_eq(r.t.descriptors_emitted_o, 16, "case3: counted sixteen");
  check_patch(r, 0, r.store, r.lat, 0xA1A1, "case3");
}

// CASE 4 -- THE UPSTREAM LATTICE CLIENT IS NEVER DELAYED, and losing the race
// is a DURATION. This is the case that guards the pass-through.
void case4_lattice_contention() {
  Rig r;
  r.reset();
  r.store.fresh_of_slot[7] = true;
  r.lat.base = 0x01100000;
  check_true(r.push_door(7, 0xC3C3), "case4: the door takes the identity");

  r.t.serve_valid_i = 1;
  r.t.serve_src_id_i = 0xC3C3;

  // TESS asks on two cycles in every three, for the whole patch.
  int guard = 0;
  while (guard++ < 4000) {
    r.up_req = (guard % 3) != 0;
    r.up_vi = static_cast<uint32_t>(guard % 33);
    r.up_vj = static_cast<uint32_t>((guard * 5) % 33);
    r.up_surface = static_cast<uint32_t>(guard & 1);
    r.step();
    if (!r.t.busy_o && r.store.st == StoreModel::IDLE && r.descs.size() == 16) break;
  }
  r.up_req = false;
  r.t.serve_valid_i = 0;
  r.idle(2);

  check_true(r.up_requests > 50, "case4: the upstream client really did contend");
  check_eq(r.up_forwarded, r.up_requests, "case4: EVERY upstream request reached the cache");
  check_eq(r.up_corrupted, 0, "case4: and none had its vi/vj/surface altered");
  check_eq(r.descs.size(), 16, "case4: the patch still assembled");
  check_patch(r, 0, r.store, r.lat, 0xC3C3, "case4");
  // THE DURATION FIRES. It is not a fault; it is the price of losing the race.
  check_true(r.t.lat_wait_clocks_o > 0, "case4: lat_wait_clocks climbed under contention");
}

// CASE 5 -- `door_refused_o`. A fifth identity against DOORD = 4.
void case5_door_refused() {
  Rig r;
  r.reset();
  check_true(r.push_door(1, 0x0001), "case5: first taken");
  check_true(r.push_door(2, 0x0002), "case5: second taken");
  check_true(r.push_door(3, 0x0003), "case5: third taken");
  check_true(r.push_door(4, 0x0004), "case5: fourth taken");
  check_eq(r.t.door_refused_o, 0, "case5: NEGATIVE CONTROL -- four fit, nothing refused");

  const bool fifth = r.push_door(5, 0x0005);
  check_true(!fifth, "case5: the fifth is refused at the door");
  check_true(r.t.door_refused_o > 0, "case5: door_refused_o FIRED");
}

// CASE 6 -- `serve_no_door_o`. A patch served that this block has no identity
// for cannot be assembled: there is no slot to read the store with.
void case6_serve_no_door() {
  Rig r;
  r.reset();
  r.store.fresh_of_slot[3] = true;

  // Negative control first: a served patch WITH an identity.
  check_true(r.push_door(3, 0xA1A1), "case6: the door takes one identity");
  r.serve(0xA1A1);
  check_eq(r.t.serve_no_door_o, 0, "case6: NEGATIVE CONTROL -- served with an identity");

  // Now serve again with nothing at the door.
  r.serve(0xD4D4, 40);
  check_eq(r.t.serve_no_door_o, 1, "case6: serve_no_door_o FIRED, exactly once");
  check_eq(r.t.patches_assembled_o, 1, "case6: and no second patch was invented");
  check_eq(r.descs.size(), 16, "case6: no descriptors emitted for it");
}

// CASE 7 -- `door_src_mismatch_o`. The identity at the head is not the patch
// the cache is serving. The two operands are written by different enables --
// see the block header -- which is why this counter CAN fire at all.
void case7_src_mismatch() {
  Rig r;
  r.reset();
  r.store.fresh_of_slot[3] = true;
  r.store.fresh_of_slot[4] = true;

  check_true(r.push_door(3, 0xA1A1), "case7: door takes A");
  r.serve(0xA1A1);
  check_eq(r.t.door_src_mismatch_o, 0, "case7: NEGATIVE CONTROL -- matching serve is silent");

  // The door says this patch is 0xB2B2; the cache serves 0xEEEE.
  check_true(r.push_door(4, 0xB2B2), "case7: door takes B");
  r.serve(0xEEEE);
  check_eq(r.t.door_src_mismatch_o, 1, "case7: door_src_mismatch_o FIRED");
  // THE PATCH IS STILL ASSEMBLED AND STILL CARRIES THE DOOR'S ID. The mismatch
  // is REPORTED, not repaired: this block cannot know which of the two is
  // wrong, and dropping the patch would turn an identity fault into a hole.
  check_eq(r.t.patches_assembled_o, 2, "case7: the patch is assembled, not dropped");
  check_eq(r.descs[16].src_id, 0xB2B2, "case7: and it carries the door's id");
}

// CASE 8 -- `patches_unfresh_o`, AND THE ONE-CYCLE DEFECT IT FOUND. A fresh
// patch first, then an unfresh one: a block sampling `r_fresh_i` on the START
// cycle reads the fresh patch's flag for the unfresh patch and scores zero.
void case8_unfresh() {
  Rig r;
  r.reset();
  r.store.fresh_of_slot[3] = true;    // has records
  r.store.fresh_of_slot[8] = false;   // has none

  check_true(r.push_door(3, 0xA1A1), "case8: door takes the fresh one");
  r.serve(0xA1A1);
  check_eq(r.t.patches_unfresh_o, 0, "case8: NEGATIVE CONTROL -- a fresh slot is silent");

  check_true(r.push_door(8, 0xB2B2), "case8: door takes the unfresh one");
  r.serve(0xB2B2);
  // THE PLANT, PROVEN BEFORE THE FIRE IS QUOTED. On the second patch's start
  // cycle the port carried the FIRST patch's freshness, which is the exact
  // state a start-cycle sample would have read. Without this check, case 8
  // passing would say only "the counter fired", not "it fired for the right
  // reason and could have been fooled".
  check_true(r.start_cycles == 2, "case8: two reads were started");
  check_true(r.start_cycles_where_fresh_differs > 0,
             "case8 PLANT: the start cycle really did carry the other patch's freshness");

  check_eq(r.t.patches_unfresh_o, 1, "case8: patches_unfresh_o FIRED on the stale slot");
  // PER PATCH, NOT PER READ. Sixteen records were read and the counter moved
  // once; the store's own `read_unwritten_o` is the per-read number.
  check_eq(r.t.patches_assembled_o, 2, "case8: the patch is still assembled");
  check_eq(r.descs.size(), 32, "case8: with all sixteen descriptors");
}

// CASE 9 -- `sp_order_bad_o`. A store that re-orders its own read would pair
// one subpatch's deviations with another's centre, and nothing downstream
// could see it.
void case9_order() {
  Rig r;
  r.reset();
  r.store.fresh_of_slot[3] = true;
  check_true(r.push_door(3, 0xA1A1), "case9: door takes the identity");
  r.serve(0xA1A1);
  check_eq(r.t.sp_order_bad_o, 0, "case9: NEGATIVE CONTROL -- an in-order store is silent");

  r.store.fresh_of_slot[5] = true;
  r.store.scramble = true;
  check_true(r.push_door(5, 0xB2B2), "case9: door takes the second");
  r.serve(0xB2B2);
  check_true(r.t.sp_order_bad_o > 0, "case9: sp_order_bad_o FIRED on a re-ordered read");
}

// CASE 10 -- `store_wait_clocks_o` is a DURATION. It climbs while the store is
// slow and is flat while the block is idle.
void case10_store_wait() {
  Rig r;
  r.reset();
  r.store.fresh_of_slot[3] = true;
  r.idle(50);
  check_eq(r.t.store_wait_clocks_o, 0, "case10: flat while idle -- it is not a free runner");

  r.store.fetch_cost = 12;   // a deliberately slow store
  check_true(r.push_door(3, 0xA1A1), "case10: door takes the identity");
  r.serve(0xA1A1);
  check_true(r.t.store_wait_clocks_o > 0, "case10: store_wait_clocks climbed while waiting");
  check_eq(r.descs.size(), 16, "case10: and the patch still assembled correctly");
  check_patch(r, 0, r.store, r.lat, 0xA1A1, "case10");

  const uint32_t parked = r.t.store_wait_clocks_o;
  r.idle(60);
  check_eq(r.t.store_wait_clocks_o, parked, "case10: flat again once the patch is done");
}

// CASE 11 -- the door queue's accounting under a push landing on the same
// cycle as a pop. `zhao_terrain_jobissue`'s header records what an earlier
// draft of this same queue cost when the count moved in four places.
void case11_queue_accounting() {
  Rig r;
  r.reset();
  for (int i = 0; i < 16; ++i) r.store.fresh_of_slot[i] = true;

  // Keep the door full while four patches are served back to back.
  check_true(r.push_door(1, 0x0101), "case11: A");
  check_true(r.push_door(2, 0x0202), "case11: B");
  r.serve(0x0101);
  check_true(r.push_door(3, 0x0303), "case11: C");
  r.serve(0x0202);
  check_true(r.push_door(4, 0x0404), "case11: D");
  r.serve(0x0303);
  r.serve(0x0404);

  check_eq(r.t.patches_assembled_o, 4, "case11: four patches, in order");
  check_eq(r.descs.size(), 64, "case11: sixty-four descriptors");
  check_eq(r.t.door_src_mismatch_o, 0, "case11: every pop matched its serve");
  check_eq(r.t.serve_no_door_o, 0, "case11: the queue never ran dry");
  check_eq(r.t.door_refused_o, 0, "case11: and never overflowed");
  // The ids came out in door order, which is the accounting being right.
  check_eq(r.descs[0].src_id, 0x0101, "case11: patch A's id");
  check_eq(r.descs[16].src_id, 0x0202, "case11: patch B's id");
  check_eq(r.descs[32].src_id, 0x0303, "case11: patch C's id");
  check_eq(r.descs[48].src_id, 0x0404, "case11: patch D's id");
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  case1_one_patch();
  case2_two_patches();
  case3_sink_backpressure();
  case4_lattice_contention();
  case5_door_refused();
  case6_serve_no_door();
  case7_src_mismatch();
  case8_unfresh();
  case9_order();
  case10_store_wait();
  case11_queue_accounting();

  std::printf("terrain_spdesc_directed: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
