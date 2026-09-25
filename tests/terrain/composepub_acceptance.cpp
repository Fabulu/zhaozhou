// composepub_acceptance.cpp -- A LIVE EARTH FIELD, ALL THE WAY TO A CONSUMER.
//
// Drives `tb_terrain_composepub.sv`: four real blocks wired port-for-port as
// `zhao_console_core.sv` wires them --
//
//   zhao_field_earth_adapter -> zhao_terrain_patch
//     -> zhao_terrain_compcache_front -> zhao_terrain_heighttap
//
// ===========================================================================
// THE CLAIM, AND WHY NOTHING IN THE TREE HAD MADE IT
// ===========================================================================
// `spec/terrain_rules.md` section 3.4 is
//
//     live_top = max(compose_top + SUM(field lanes), fx(bottom))
//
// and until this file ran, THE SUM HAD NEVER HAD A NON-EMPTY TERM IN ANY BENCH
// OF THE REAL CHAIN. `tb_terrain_compose.sv` -- the nearest existing bench --
// ties the lane off in its own wiring (`.fld_valid_i(1'b0)`,
// `.fld_height_i(32'sd0)`, `:443,445`) and says so in its header: "The field
// list is EMPTY ... The field half needs FIELD.SEQ.EARTH."
//
// So a TerrainField command could reach CMD.EXEC, be sealed into the field
// list, be replayed into the patch's section 9.1 list, be evaluated by the one
// field engine -- and no test anywhere asked whether the resulting height
// reached anything that READS it. Entry I34's note that the sum "now has a
// non-empty sum" describes ARITHMETIC REACHING A VALUE. Cases 2 and 6 below
// are the first evidence that the value arrives at a consumer.
//
// THE CONSUMER IS THE ONE THE CONSOLE SHIPS. `zhao_terrain_heighttap` is
// TERRAIN.TAPSHARE's single service; its two clients are PART.COLLIDE
// (`zhao_part_terrain_tap` -> `zhao_part_collide`) and FORGE.SHADOW
// (`zhao_forge_shadow.sv:131-137`). A height this block returns is a height a
// particle collides against and a shadow conforms to.
//
// ===========================================================================
// THE ORACLE IS `zref::terrain::column_query`, NOT A NUMBER THIS FILE INVENTS
// ===========================================================================
// Every tap is differenced against the reference point query (terrain_rules
// section 4.3) evaluated on a `ComposedLattice` built from THE VERY WORDS THE
// HARDWARE STREAMED into the compose cache -- captured off `st_top_o` as they
// land, not recomputed. So the differential asks exactly one question, which is
// the question this bench exists for:
//
//     given the composed lattice the hardware actually produced, does the
//     hardware's inverse map agree with the reference's?
//
// and the SEPARATE assertion that `st_top_o != st_compose_top_o` is what says
// the field moved the lattice in the first place. Splitting those two is
// deliberate: a bench that only compared tap-against-reference would pass
// perfectly on a lattice no field had touched, which is precisely the vacuous
// green `tb_terrain_compose.sv` already has.
//
// ===========================================================================
// WHAT IS DRIVER-MODELLED, DECLARED RATHER THAN HIDDEN
// ===========================================================================
// THE SHARED FIELD ENGINE. The adapter is client 3 of the one engine
// (`zhao_field_host_v2`), whose own proof is `field_host_v2_directed.cpp` and
// whose seam against this adapter is `field_earth_adapter_directed.cpp`.
// Modelling it here is what lets a case place an EXACT out-lane value and an
// EXACT presence mask, which is the only way "absent optional output is no
// write, not a write of zero" (owner directive section 13.3) becomes testable
// at all -- see case 5.
//
// A NOPROG REQUEST IS STILL OFFERED TO THE ENGINE, and the model answers it
// with the engine's ratified refusal status 0xF0, because that is what the
// adapter expects: "it is offered to the engine with `req_noprog_o` raised, so
// the refusal is the ENGINE's ratified one (status 0xF0)"
// (`zhao_field_earth_adapter.sv:729-730`). A model that answered noprog locally
// would be testing a protocol the console does not run.
//
// THE AUTHORED PAGE LATTICE. `tb_terrain_compose.sv` already gates
// TERRAIN.PAGESTREAM -> TERRAIN.PATCH on real 21,376-byte page bytes.
// Re-streaming a page here would re-gate that seam and not this one.
//
// ===========================================================================
// THE UNIFORM INTAKE IS JOINED IN THE CONSOLE AND IS NOT JOINED HERE
// ===========================================================================
// In `zhao_console_core` the adapter's `rec_ready_o` is ANDed with
// `zhao_terrain_fieldlist`'s, so the adapter's bank index and the list's entry
// index are THE SAME NUMBER BY CONSTRUCTION. TERRAIN.FIELDLIST is not in this
// bench, so this driver carries that invariant itself: `bank()` and `add()` are
// called the same number of times in the same order, and `efa_lane_desync_o`
// -- whose two operands are loaded by different blocks' state machines -- is
// asserted zero at the end of every case as the independent check that they
// stayed aligned.
// ===========================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_composepub.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"

using Dut = Vtb_terrain_composepub;

namespace {

constexpr int kLatW  = 33;
constexpr int kLatH  = 33;
constexpr int kVerts = kLatW * kLatH;

// The island's cell pitch. `spec/terrain_rules.md` 1.3 freezes the set to
// powers of two; the tap derives its cell index by shifting, so the placement
// law below and the tap's own must use the same one.
constexpr int kPitchLog2 = 1;
constexpr int kSh        = 16 + kPitchLog2;   // fx16 raw per cell
constexpr int32_t kD     = static_cast<int32_t>(1) << kSh;

// `zhao_terrain_heighttap`'s placement law, transcribed from
// tests/terrain/terrain_heighttap_directed.cpp:76 so the two cannot drift:
//     wx(i) = (patch_ix * 32 + i) <<< (16 + pitch_log2)
inline int32_t place_axis(int patch_index, int i) {
  return static_cast<int32_t>((static_cast<int64_t>(patch_index) * 32 + i) << kSh);
}

// section 3.4's up-conversion, height16 -> fx16 raw. Exact, per
// tests/terrain/compose_rtl_directed.cpp's own analysis of the same shift.
inline int32_t fx_of_h16(int16_t h) { return static_cast<int32_t>(h) << 8; }

int g_checks = 0;
int g_fails  = 0;

void ck(bool cond, const char* what) {
  ++g_checks;
  if (!cond) {
    ++g_fails;
    std::printf("  FAIL: %s\n", what);
  }
}

void ck_eq(int64_t got, int64_t want, const char* what) {
  ++g_checks;
  if (got != want) {
    ++g_fails;
    std::printf("  FAIL: %s  (got %lld, want %lld)\n", what,
                static_cast<long long>(got), static_cast<long long>(want));
  }
}

// ---------------------------------------------------------------------------
// THE FIELD ENGINE MODEL -- FIELD.HOST's subsystem, at its client seam
// ---------------------------------------------------------------------------
struct Engine {
  // the answer this engine gives, per run
  int32_t out[4]   = {0, 0, 0, 0};
  uint8_t present  = 0x0F;   // ordinals 0..3 are VALUES
  uint8_t status   = 0x00;   // StOk
  int     latency  = 3;      // clocks between accept and answer
  bool    ready    = true;   // the engine's own backpressure

  // PER-PROGRAM ANSWERS, keyed on `req_slot_o`. Two fields in one list are two
  // BOUND PROGRAMS, and `req_slot_o` is `b_obj[lane]` -- the object index the
  // replay banked from `add_obj_i`, "the identical value a BIND reply puts on
  // `fh2_resp_slot_o`" (zhao_field_earth_adapter.sv:186-189). Keying here is
  // therefore how the real engine tells two fields apart, and it is the only
  // way case 6 can vary the ORDER of two lanes while holding their VALUES
  // fixed. The first draft of case 6 varied the value instead and compared two
  // different experiments -- it failed, correctly, and this is the repair.
  bool    per_slot = false;
  int32_t slot_out0[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  // observation
  uint64_t runs        = 0;
  uint64_t noprog_runs = 0;
  uint32_t last_in[12] = {};

  // in flight
  bool    pending    = false;
  int     countdown  = 0;
  bool    was_noprog = false;
  uint8_t held_slot  = 0;

  void drive(Dut& d) const {
    d.req_ready_i   = ready ? 1 : 0;
    const bool answering = pending && countdown <= 0;
    d.resp_valid_i  = answering ? 1 : 0;
    // A noprog run is refused by the ENGINE, with its ratified status.
    d.resp_status_i = answering && was_noprog ? 0xF0 : status;
    for (int i = 0; i < 7; ++i) d.resp_out_i[i] = 0;
    if (answering && !was_noprog) {
      for (int i = 0; i < 4; ++i) d.resp_out_i[i] = static_cast<uint32_t>(out[i]);
      if (per_slot)
        d.resp_out_i[0] = static_cast<uint32_t>(slot_out0[held_slot & 7]);
      d.resp_present_i = present & 0x7F;
    } else {
      d.resp_present_i = 0;
    }
  }

  void observe(Dut& d) {
    if (d.req_valid_o && d.req_ready_i && !pending) {
      pending    = true;
      countdown  = latency;
      was_noprog = d.req_noprog_o != 0;
      held_slot  = static_cast<uint8_t>(d.req_slot_o);
      ++runs;
      if (was_noprog) ++noprog_runs;
      for (int i = 0; i < 12; ++i) last_in[i] = d.req_in_o[i];
    }
    if (pending && countdown <= 0 && d.resp_ready_o) pending = false;
  }

  void age() { if (pending && countdown > 0) --countdown; }
};

// ---------------------------------------------------------------------------
// THE BENCH DRIVER
// ---------------------------------------------------------------------------
struct Sim {
  Dut     d;
  Engine  eng;
  // every word the hardware streamed into the compose cache, in fill order
  std::vector<int32_t> streamed_top;
  std::vector<int32_t> streamed_compose_top;
  uint64_t answers_taken = 0;   // adapter -> PATCH field-lane handshakes

  void step() {
    eng.drive(d);
    d.eval();
    eng.observe(d);
    // CAPTURE ON THE HANDSHAKE, NOT ON VALID. The cache accepts on alternate
    // clocks (one record is two writes), so `st_valid_o` alone records every
    // record twice -- 2,177 entries for a 1,089-vertex lattice, which is
    // exactly what this driver did on its first run.
    if (d.st_valid_o && d.st_ready_o) {
      streamed_top.push_back(static_cast<int32_t>(d.st_top_o));
      streamed_compose_top.push_back(static_cast<int32_t>(d.st_compose_top_o));
    }
    if (d.efa_ans_valid_o && d.efa_ans_ready_o) ++answers_taken;
    zhao::tick(d);
    eng.age();
  }

  void steps(int n) { for (int i = 0; i < n; ++i) step(); }

  void quiesce() {
    d.rec_valid_i     = 0;
    d.fld_add_valid_i = 0;
    d.vtx_valid_i     = 0;
    d.pos_we_i        = 0;
    d.cs_we_i         = 0;
    d.fill_start_i    = 0;
    d.serve_release_i = 0;
    d.req_tap_valid_i = 0;
    d.o_lat_req_i     = 0;
    d.patch_open_i    = 0;
    d.list_clear_i    = 0;
  }

  void reset() {
    d.clk   = 0;
    d.rst_n = 0;
    quiesce();
    d.tick_i        = 0;
    d.pitch_log2_i  = static_cast<uint8_t>(static_cast<int8_t>(kPitchLog2));
    d.dual_i        = 0;
    d.patch_id_i    = 0;
    d.add_obj_i     = 0;
    d.add_resident_i = 1;
    d.req_tap_surface_i = 0;
    d.o_lat_surface_i   = 0;
    eng.drive(d);
    d.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(d);
    d.rst_n = 1;
    d.eval();
    steps(4);
  }

  // ---- bank one uniform record into the adapter --------------------------
  void bank(uint32_t start_tick, uint32_t duration, bool last,
            const uint32_t params[8]) {
    d.rec_start_tick_i = start_tick;
    d.rec_duration_i   = duration;
    d.rec_last_i       = last ? 1 : 0;
    for (int i = 0; i < 8; ++i) d.rec_params_i[i] = params[i];
    d.rec_valid_i = 1;
    int guard = 0;
    for (;;) {
      eng.drive(d);
      d.eval();
      const bool fire = d.rec_ready_o != 0;
      step();
      if (fire) break;
      if (++guard > 10000) { ck(false, "bank(): rec_ready_o never rose"); break; }
    }
    d.rec_valid_i = 0;
    step();

    // DRAIN THE INTAKE BEFORE ANY PATCH REPLAY, AND THIS IS A REAL ORDERING
    // LAW rather than a bench convenience. `rec_ready_o` falling is NOT the
    // record being banked: section 7.1's phase is an EXACT ROUNDED INTEGER
    // DIVIDE and the adapter's divider "is therefore exact and sequential"
    // (zhao_field_earth_adapter.sv:64-69), so `b_uni[wr_a]` -- and beside it
    // `b_res[wr_a] <= 1'b0` (:1005) -- land several clocks AFTER the
    // handshake. Replaying the section 9.1 list in that window lets the
    // intake's clear CLOBBER the `add_resident_i` the replay just banked, and
    // every evaluation then comes back NOPROG.
    //
    // THIS BENCH MEASURED EXACTLY THAT on its first run: 1,089 engine runs,
    // `noprog_o` == 1,089, and a composed height identical to the authored
    // one -- a field that ran, cost the engine every cycle it should, and
    // moved nothing.
    //
    // THE DEFECT IS REPAIRED (packet EARTHLOCK, 2026-09-25) AND THIS DRAIN
    // STAYS, for a reason that is no longer the original one. The separation
    // it models is real: in `zhao_console_core` the uniform intake is a FRAME
    // event joined with TERRAIN.FIELDLIST's seal, while the replay is a
    // per-patch event much later. Every case below wants that well-separated
    // console, so `bank()` keeps reproducing it.
    //
    // BUT A WORKAROUND LEFT IN PLACE IS A PLACE A REGRESSION CAN HIDE. With
    // this drain in every path, reverting EARTHLOCK's interlock would leave
    // all eighty checks GREEN -- the bench would go on passing about a machine
    // that had started racing again. `bank_no_drain()` beside it is the
    // answer: case 10 banks WITHOUT draining, replays inside the window, and
    // demands the field still reach the consumer. That case is the one this
    // comment is evidence for.
    int drain = 0;
    while (!d.efa_idle_o && drain++ < 100000) step();
    ck(d.efa_idle_o != 0, "bank(): the adapter's uniform intake drained");
    steps(2);
  }

  // ---- THE SAME INTAKE, STOPPED INSIDE THE WINDOW -------------------------
  // `bank()` above waits for `efa_idle_o`. This one deliberately does NOT: it
  // returns on the handshake beat, which is the beat the rest of the console
  // is synchronised to, with the divider still running and the banks not yet
  // written. Everything a caller does next lands in the eighteen-clock window
  // the repair closed. It is the console-shaped twin of
  // `tests/field/field_earth_adapter_directed.cpp` case 13.
  void bank_no_drain(uint32_t start_tick, uint32_t duration, bool last,
                     const uint32_t par[8]) {
    d.rec_start_tick_i = start_tick;
    d.rec_duration_i   = duration;
    for (int i = 0; i < 8; ++i) d.rec_params_i[i] = par[i];
    d.rec_last_i  = last ? 1 : 0;
    d.rec_valid_i = 1;
    int guard = 0;
    for (;;) {
      eng.drive(d);
      d.eval();
      const bool fire = d.rec_ready_o != 0;
      step();
      if (fire) break;
      if (++guard > 10000) { ck(false, "bank_no_drain(): rec_ready_o never rose"); break; }
    }
    d.rec_valid_i = 0;
    d.rec_last_i  = 0;
    // NO DRAIN. The caller replays into the window on purpose.
  }

  // ---- replay one section 9.1 list entry into TERRAIN.PATCH ---------------
  void add(int32_t x0, int32_t z0, int32_t x1, int32_t z1, uint16_t cmd,
           bool resident = true, uint8_t obj = 0) {
    d.fld_add_x0_i    = static_cast<uint32_t>(x0);
    d.fld_add_z0_i    = static_cast<uint32_t>(z0);
    d.fld_add_x1_i    = static_cast<uint32_t>(x1);
    d.fld_add_z1_i    = static_cast<uint32_t>(z1);
    d.fld_add_hash_i  = 0xABCD0000u | cmd;
    d.fld_add_cmd_i   = cmd;
    d.add_obj_i       = obj;
    d.add_resident_i  = resident ? 1 : 0;
    d.fld_add_valid_i = 1;
    int guard = 0;
    for (;;) {
      eng.drive(d);
      d.eval();
      const bool fire = d.fld_add_ready_o != 0;
      step();
      if (fire) break;
      if (++guard > 10000) { ck(false, "add(): fld_add_ready_o never rose"); break; }
    }
    d.fld_add_valid_i = 0;
    step();
  }

  void open_patch(uint16_t patch_id) {
    d.list_clear_i = 1;
    d.patch_id_i   = patch_id;
    step();
    d.list_clear_i = 0;
    d.patch_open_i = 1;
    step();
    d.patch_open_i = 0;
    step();
  }

  // ---- write the compose cache's placement plane --------------------------
  void place(int patch_ix, int patch_iz) {
    for (int i = 0; i < kLatW; ++i) {
      d.pos_we_i   = 1;
      d.pos_axis_i = 0;
      d.pos_idx_i  = static_cast<uint8_t>(i);
      d.pos_val_i  = static_cast<uint32_t>(place_axis(patch_ix, i));
      step();
    }
    for (int j = 0; j < kLatH; ++j) {
      d.pos_we_i   = 1;
      d.pos_axis_i = 1;
      d.pos_idx_i  = static_cast<uint8_t>(j);
      d.pos_val_i  = static_cast<uint32_t>(place_axis(patch_iz, j));
      step();
    }
    d.pos_we_i = 0;
    step();
  }

  void write_cell(int ci, int cj, int substance) {
    d.cs_we_i          = 1;
    d.cs_w_ci_i        = static_cast<uint8_t>(ci);
    d.cs_w_cj_i        = static_cast<uint8_t>(cj);
    d.cs_w_substance_i = static_cast<uint8_t>(substance);
    step();
    d.cs_we_i = 0;
    step();
  }

  // ---- stream the authored lattice through PATCH into the cache -----------
  // z-then-x, which is the compose cache's own index order ("the order IS the
  // index", `zhao_terrain_compcache_front.sv`) and the reference's.
  void fill(int patch_ix, int patch_iz, int16_t base, int16_t scar, int16_t bottom) {
    streamed_top.clear();
    streamed_compose_top.clear();
    answers_taken = 0;

    d.fill_start_i = 1;
    step();
    d.fill_start_i = 0;
    step();

    place(patch_ix, patch_iz);

    for (int vj = 0; vj < kLatH; ++vj) {
      for (int vi = 0; vi < kLatW; ++vi) {
        d.base_i     = static_cast<uint16_t>(base);
        d.scar_i     = static_cast<uint16_t>(scar);
        d.bottom_i   = static_cast<uint16_t>(bottom);
        d.wx_i       = static_cast<uint32_t>(place_axis(patch_ix, vi));
        d.wz_i       = static_cast<uint32_t>(place_axis(patch_iz, vj));
        d.vi_i       = static_cast<uint8_t>(vi);
        d.vj_i       = static_cast<uint8_t>(vj);
        d.src_id_i   = static_cast<uint16_t>(vj * kLatW + vi);
        d.vtx_valid_i = 1;
        int guard = 0;
        for (;;) {
          eng.drive(d);
          d.eval();
          const bool fire = d.vtx_ready_o != 0;
          step();
          if (fire) break;
          if (++guard > 500000) { ck(false, "fill(): vtx_ready_o never rose"); return; }
        }
        d.vtx_valid_i = 0;
      }
    }
    // drain the compose lane into the cache
    int guard = 0;
    while (!d.fill_done_o && guard++ < 500000) step();
    ck(d.fill_done_o != 0, "fill(): the cache reported LAT_W*LAT_H records landed");
    steps(4);
  }

  // ---- ask the CONSUMER ---------------------------------------------------
  struct Answer {
    int32_t height    = 0;
    bool    no_ground = true;
    int32_t ny        = 0;
  };

  Answer tap(int32_t wx, int32_t wz) {
    d.req_tap_x_i     = static_cast<uint32_t>(wx);
    d.req_tap_z_i     = static_cast<uint32_t>(wz);
    d.req_tap_valid_i = 1;
    int guard = 0;
    for (;;) {
      eng.drive(d);
      d.eval();
      const bool fire = d.req_tap_ready_o != 0;
      step();
      if (fire) break;
      if (++guard > 100000) { ck(false, "tap(): req_tap_ready_o never rose"); break; }
    }
    d.req_tap_valid_i = 0;
    Answer a;
    guard = 0;
    for (;;) {
      eng.drive(d);
      d.eval();
      if (d.rsp_tap_valid_o) {
        a.height    = static_cast<int32_t>(d.rsp_tap_height_o);
        a.no_ground = d.rsp_tap_no_ground_o != 0;
        a.ny        = static_cast<int32_t>(d.rsp_tap_ny_o);
        step();
        break;
      }
      step();
      if (++guard > 100000) { ck(false, "tap(): rsp_tap_valid_o never rose"); break; }
    }
    return a;
  }

  // ---- the reference lattice, built from the words the HARDWARE streamed --
  zref::terrain::ComposedLattice oracle_lattice(int patch_ix, int patch_iz) const {
    zref::terrain::ComposedLattice lat;
    lat.w    = kLatW;
    lat.h    = kLatH;
    lat.dual = false;
    lat.wx.resize(kLatW);
    lat.wz.resize(kLatH);
    for (int i = 0; i < kLatW; ++i) lat.wx[i] = place_axis(patch_ix, i);
    for (int j = 0; j < kLatH; ++j) lat.wz[j] = place_axis(patch_iz, j);
    lat.top = streamed_top;
    lat.bottom = streamed_top;
    return lat;
  }
};

// a field program's eight Q16.16 parameters; this bench's engine does not
// interpret them, but they must ride the record so the adapter banks a real one
const uint32_t kParams[8] = {1, 2, 3, 4, 5, 6, 7, 8};

// a footprint covering the whole patch, in fx16 raw, CLOSED interval
struct Foot { int32_t x0, z0, x1, z1; };
Foot whole_patch(int patch_ix, int patch_iz) {
  return Foot{place_axis(patch_ix, 0), place_axis(patch_iz, 0),
              place_axis(patch_ix, kLatW - 1), place_axis(patch_iz, kLatH - 1)};
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  std::printf("composepub_acceptance -- a live Earth field, all the way to a consumer\n");
  std::printf("  chain: EARTH.ADAPTER -> TERRAIN.PATCH -> TERRAIN.COMPCACHE -> TERRAIN.HEIGHTTAP\n\n");

  const int16_t kBase   = 100;
  const int16_t kScar   = 0;
  const int16_t kBottom = -4000;

  // the tap point: the middle of cell (4,4), off every lattice node so the
  // section 4.3 interpolation actually runs instead of landing on a corner
  const int32_t kTapX = (4 << kSh) + kD / 3;
  const int32_t kTapZ = (4 << kSh) + kD / 3;

  int32_t baseline_tap_height = 0;

  // =========================================================================
  // CASE 1 -- NO-FIELD BASELINE IDENTITY
  // With an empty section 9.1 list, section 3.4 collapses to `compose_top` and
  // the consumer must see exactly the authored lattice. This is the case that
  // says the bench is not manufacturing motion merely by existing -- and it is
  // the control every later case is read against.
  // =========================================================================
  {
    std::printf("case 1: no-field baseline identity\n");
    Sim s;
    s.reset();
    s.open_patch(1);
    // no add() at all: fields_active_o must be 0
    s.d.eval();
    ck_eq(s.d.fields_active_o, 0, "empty list -> fields_active_o == 0");

    s.fill(0, 0, kBase, kScar, kBottom);

    ck_eq(static_cast<int64_t>(s.streamed_top.size()), kVerts,
          "the cache took LAT_W*LAT_H records");
    bool all_identical = true;
    for (size_t i = 0; i < s.streamed_top.size(); ++i)
      if (s.streamed_top[i] != s.streamed_compose_top[i]) all_identical = false;
    ck(all_identical, "with no field, live_top == compose_top at every vertex");
    ck_eq(s.streamed_top.empty() ? -1 : s.streamed_top[0],
          fx_of_h16(kBase) + fx_of_h16(kScar),
          "compose_top == fx(base) + fx(scar), clear of the bottom clamp");

    const Sim::Answer a = s.tap(kTapX, kTapZ);
    ck(!a.no_ground, "the consumer answered (not no_ground)");

    const zref::terrain::ComposedLattice lat = s.oracle_lattice(0, 0);
    const zref::terrain::ColumnResult r =
        zref::terrain::column_query(lat, zref::fx16{kTapX}, zref::fx16{kTapZ});
    ck_eq(a.height, r.top.raw, "tap height == zref::terrain::column_query.top");

    ck_eq(s.d.efa_runs_o, 0, "no list entries -> the engine was never run");
    ck_eq(s.d.efa_lane_desync_o, 0, "lane shadow stayed aligned");
    ck_eq(s.d.place_mismatch_o, 0, "no placement fault");
    ck_eq(s.d.pitch_bad_o, 0, "no pitch fault");
    ck_eq(s.d.cc_fill_overrun_o, 0, "no fill overrun");
    baseline_tap_height = a.height;
    std::printf("  baseline tap height = %d (fx16 raw)\n", baseline_tap_height);
  }

  // =========================================================================
  // CASE 2 -- THE PACKET'S CENTRAL CLAIM
  // ONE live Earth field covering the whole patch, out-lane 0 nonzero. The
  // composed lattice must MOVE, and the CONSUMER must see it move. This is the
  // first time in this repository that a field's contribution is observed at
  // something that reads the composed height.
  // =========================================================================
  {
    std::printf("case 2: a live Earth field changes what the CONSUMER reads\n");
    Sim s;
    s.reset();
    const int32_t kLift = 7 << 16;   // +7.0 in fx16
    s.eng.out[0]  = kLift;
    s.eng.present = 0x0F;

    s.d.tick_i = 50;
    s.bank(/*start_tick=*/0, /*duration=*/100, /*last=*/true, kParams);
    s.open_patch(2);
    const Foot f = whole_patch(0, 0);
    s.add(f.x0, f.z0, f.x1, f.z1, /*cmd=*/1);
    s.d.eval();
    ck_eq(s.d.fields_active_o, 1, "one accepted list entry -> fields_active_o == 1");

    s.fill(0, 0, kBase, kScar, kBottom);

    std::printf("  engine runs=%llu  adapter runs_o=%u  answers taken=%llu  "
                "skipped=%u  not_begun=%u  noprog=%u  faults=%u  short=%u\n",
                static_cast<unsigned long long>(s.eng.runs), s.d.efa_runs_o,
                static_cast<unsigned long long>(s.answers_taken),
                s.d.efa_skipped_uncovered_o, s.d.efa_not_begun_o, s.d.efa_noprog_o,
                s.d.efa_faults_o, s.d.efa_short_record_o);
    std::printf("  streamed[0] top=%d compose_top=%d  (delta %d, want %d)\n",
                s.streamed_top.empty() ? 0 : s.streamed_top[0],
                s.streamed_compose_top.empty() ? 0 : s.streamed_compose_top[0],
                s.streamed_top.empty() ? 0 : s.streamed_top[0] - s.streamed_compose_top[0],
                kLift);
    ck(s.eng.runs > 0, "the field engine actually ran");
    ck_eq(static_cast<int64_t>(s.eng.runs), kVerts,
          "a whole-patch footprint runs the engine once per vertex");
    ck_eq(s.d.efa_skipped_uncovered_o, 0, "every vertex was covered");

    bool every_vertex_lifted = true;
    for (size_t i = 0; i < s.streamed_top.size(); ++i)
      if (s.streamed_top[i] - s.streamed_compose_top[i] != kLift)
        every_vertex_lifted = false;
    ck(every_vertex_lifted, "live_top - compose_top == the field's out-lane 0, every vertex");

    const Sim::Answer a = s.tap(kTapX, kTapZ);
    ck(!a.no_ground, "the consumer answered");
    ck_eq(a.height - baseline_tap_height, kLift,
          "THE CONSUMER'S HEIGHT MOVED BY THE FIELD'S CONTRIBUTION");

    const zref::terrain::ComposedLattice lat = s.oracle_lattice(0, 0);
    const zref::terrain::ColumnResult r =
        zref::terrain::column_query(lat, zref::fx16{kTapX}, zref::fx16{kTapZ});
    ck_eq(a.height, r.top.raw, "tap height == zref::terrain::column_query.top");

    ck_eq(s.d.efa_lane_desync_o, 0, "lane shadow stayed aligned");
    ck_eq(s.d.efa_faults_o, 0, "no engine fault");
    ck_eq(s.d.place_mismatch_o, 0, "no placement fault");
  }

  // =========================================================================
  // CASE 3 -- SECTION 9.1: A FIELD THAT DOES NOT COVER THE VERTEX
  // The footprint test lives in TERRAIN.PATCH (its chosen law 2) and is
  // consumed by the adapter for exactly one purpose: a lane that misses is
  // answered with zero and NO RUN. So the height must be the baseline AND the
  // engine must not have run for the missed vertices.
  // =========================================================================
  {
    std::printf("case 3: a field whose footprint misses -- zero, and NO engine run\n");
    Sim s;
    s.reset();
    s.eng.out[0]  = 9 << 16;
    s.eng.present = 0x0F;

    s.d.tick_i = 50;
    s.bank(0, 100, true, kParams);
    s.open_patch(3);
    // a footprint on cells 20..24, nowhere near the tap point at cell 4
    s.add(place_axis(0, 20), place_axis(0, 20), place_axis(0, 24), place_axis(0, 24), 1);

    s.fill(0, 0, kBase, kScar, kBottom);

    ck(s.d.efa_skipped_uncovered_o > 0, "section 9.1 misses were counted");
    ck(s.eng.runs < static_cast<uint64_t>(kVerts),
       "the engine ran for FEWER than every vertex -- the miss skipped the run");
    ck_eq(static_cast<int64_t>(s.eng.runs) + s.d.efa_skipped_uncovered_o, kVerts,
          "runs + skipped == one lane per vertex: no lane went missing");

    const Sim::Answer a = s.tap(kTapX, kTapZ);
    ck_eq(a.height, baseline_tap_height,
          "an uncovered tap point is the authored height exactly");
    ck_eq(s.d.efa_lane_desync_o, 0, "lane shadow stayed aligned");
  }

  // =========================================================================
  // CASE 4 -- A FIELD THAT HAS NOT BEGUN
  // `frame_tick < start_tick` is the reference's `continue`. The adapter
  // answers the additive zero and does NOT run the engine, and the height is
  // the baseline. A crater that has not started must not dent the ground.
  // =========================================================================
  {
    std::printf("case 4: a field whose start_tick is in the future\n");
    Sim s;
    s.reset();
    s.eng.out[0]  = 11 << 16;
    s.eng.present = 0x0F;

    s.d.tick_i = 10;
    s.bank(/*start_tick=*/900, /*duration=*/100, true, kParams);
    s.open_patch(4);
    const Foot f = whole_patch(0, 0);
    s.add(f.x0, f.z0, f.x1, f.z1, 1);

    s.fill(0, 0, kBase, kScar, kBottom);

    ck(s.d.efa_not_begun_o > 0, "not-begun was counted");
    ck_eq(s.eng.runs, 0, "a not-begun field never ran the engine");

    const Sim::Answer a = s.tap(kTapX, kTapZ);
    ck_eq(a.height, baseline_tap_height,
          "a field that has not begun leaves the ground exactly as authored");
    ck_eq(s.d.efa_lane_desync_o, 0, "lane shadow stayed aligned");
  }

  // =========================================================================
  // CASE 5 -- ABSENT IS NOT ZERO
  // Owner directive section 13.3: "A genuinely absent optional lane under an
  // explicit compatible program signature is not a write of zero." The engine
  // answers with ordinal 0 CLEAR in the presence mask. The HEIGHT is unchanged
  // either way -- an add of zero and a skipped add are the same arithmetic on
  // this lane, which the adapter's header says out loud -- so the thing that
  // must differ is the ADAPTER'S REPORT, not the number. `short_record_o` is
  // the instrument, and this case is its firing evidence.
  // =========================================================================
  {
    std::printf("case 5: a present zero and an absent lane are distinguishable\n");

    // (a) PRESENT and zero
    uint32_t present_zero_short = 0;
    {
      Sim s;
      s.reset();
      s.eng.out[0]  = 0;
      s.eng.present = 0x0F;   // ordinal 0 IS a value, and that value is zero
      s.d.tick_i = 50;
      s.bank(0, 100, true, kParams);
      s.open_patch(5);
      const Foot f = whole_patch(0, 0);
      s.add(f.x0, f.z0, f.x1, f.z1, 1);
      s.fill(0, 0, kBase, kScar, kBottom);
      const Sim::Answer a = s.tap(kTapX, kTapZ);
      ck_eq(a.height, baseline_tap_height, "a present ZERO leaves the height unchanged");
      present_zero_short = s.d.efa_short_record_o;
      ck_eq(present_zero_short, 0, "a fully present record is not a short record");
      ck(s.eng.runs > 0, "the engine ran -- a present zero is a real evaluation");
    }

    // (b) ABSENT -- ordinal 0 is a HOLE
    {
      Sim s;
      s.reset();
      s.eng.out[0]  = 0;
      s.eng.present = 0x00;   // no ordinal is a value: every lane is a hole
      s.d.tick_i = 50;
      s.bank(0, 100, true, kParams);
      s.open_patch(6);
      const Foot f = whole_patch(0, 0);
      s.add(f.x0, f.z0, f.x1, f.z1, 1);
      s.fill(0, 0, kBase, kScar, kBottom);
      const Sim::Answer a = s.tap(kTapX, kTapZ);
      ck_eq(a.height, baseline_tap_height, "an ABSENT lane also leaves the height unchanged");
      ck(s.d.efa_short_record_o > 0,
         "THE INSTRUMENT FIRES: an absent ordinal is reported as a short record");
      ck(s.d.efa_short_record_o != present_zero_short,
         "absent and present-zero are DISTINGUISHABLE at the adapter's report");
    }
  }

  // =========================================================================
  // CASE 6 -- TWO OVERLAPPING FIELDS, BOTH COMMAND ORDERS
  // Section 13.3: height uses the existing command-ordered saturating
  // operations. Two fields covering the same vertex contribute both lanes, and
  // -- clear of saturation -- addition commutes, so BOTH orders must give the
  // identical composed height. The case that would separate them is case 7.
  // =========================================================================
  {
    std::printf("case 6: two overlapping fields, both command orders\n");
    const int32_t kA = 3 << 16;
    const int32_t kB = 5 << 16;

    // TWO FIELDS WITH FIXED, DIFFERENT VALUES; ONLY THE ORDER CHANGES.
    // Field A is bound to program slot 1 and always lifts by kA; field B is
    // bound to slot 2 and always lifts by kB. The engine answers on the slot,
    // so swapping the list order swaps which lane arrives first WITHOUT
    // changing either field's contribution -- which is the only way this case
    // tests order rather than testing two different experiments.
    int32_t h_ab = 0, h_ba = 0;
    for (int order = 0; order < 2; ++order) {
      Sim s;
      s.reset();
      s.eng.per_slot     = true;
      s.eng.slot_out0[1] = kA;
      s.eng.slot_out0[2] = kB;
      s.eng.present      = 0x0F;
      s.d.tick_i = 50;
      s.bank(0, 100, false, kParams);
      s.bank(0, 100, true,  kParams);
      s.open_patch(static_cast<uint16_t>(7 + order));
      const Foot f = whole_patch(0, 0);
      const uint8_t first  = order == 0 ? 1 : 2;
      const uint8_t second = order == 0 ? 2 : 1;
      s.add(f.x0, f.z0, f.x1, f.z1, first,  true, first);
      s.add(f.x0, f.z0, f.x1, f.z1, second, true, second);
      s.d.eval();
      ck_eq(s.d.fields_active_o, 2, "two accepted entries -> fields_active_o == 2");

      s.fill(0, 0, kBase, kScar, kBottom);
      ck_eq(static_cast<int64_t>(s.eng.runs), 2 * kVerts,
            "two covering lanes run the engine twice per vertex");
      ck_eq(s.streamed_top.empty() ? -1 : s.streamed_top[0] - s.streamed_compose_top[0],
            kA + kB, "both lanes contributed to live_top");

      const Sim::Answer a = s.tap(kTapX, kTapZ);
      if (order == 0) h_ab = a.height; else h_ba = a.height;
      ck_eq(s.d.efa_lane_desync_o, 0, "lane shadow stayed aligned across two lanes");
    }
    ck_eq(h_ab, h_ba,
          "clear of saturation the two command orders give the identical height");
    ck_eq(h_ab - baseline_tap_height, kA + kB,
          "and the consumer saw BOTH overlapping fields, in either order");
  }

  // =========================================================================
  // CASE 7 -- SATURATION ON THE FIELD LANE
  // `tests/terrain/compose_rtl_directed.cpp` proved that NOTHING SOURCED FROM A
  // PAGE CAN SATURATE section 3.4's add -- both operands are up-converted
  // height16, two orders of magnitude short of fx_add's range -- and named the
  // field lanes as the only thing that can, "a different lane's evidence".
  // THIS IS THAT LANE. The field carries a full-range fx16 out of a program.
  // =========================================================================
  {
    std::printf("case 7: the field lane is the only thing that can saturate 3.4\n");
    Sim s;
    s.reset();
    s.eng.out[0]  = 0x7FFF'FFFF;   // the widest positive fx16 a program can return
    s.eng.present = 0x0F;
    s.d.tick_i = 50;
    s.bank(0, 100, true, kParams);
    s.open_patch(9);
    const Foot f = whole_patch(0, 0);
    s.add(f.x0, f.z0, f.x1, f.z1, 1);

    s.fill(0, 0, kBase, kScar, kBottom);

    bool saturated_everywhere = true;
    for (int32_t v : s.streamed_top) if (v != 0x7FFF'FFFF) saturated_everywhere = false;
    ck(saturated_everywhere,
       "a full-range field lane saturates live_top at +INT32_MAX rather than wrapping");
    ck(!s.streamed_compose_top.empty() && s.streamed_compose_top[0] != 0x7FFF'FFFF,
       "and compose_top beside it did NOT saturate -- the page cannot reach it");

    const Sim::Answer a = s.tap(kTapX, kTapZ);
    ck_eq(s.d.interp_overflow_o, 0,
          "the consumer's interpolation of a saturated lattice still fits s32");
    ck(!a.no_ground, "the consumer still answered");
  }

  // =========================================================================
  // CASE 8 -- THE OWNER STARVES THE TAP
  // TERRAIN.TESS owns the compose cache's lattice port and cannot be stalled;
  // the tap sits in front as a pass-through and injects only on cycles the
  // owner did not want. With the owner requesting every cycle the tap must
  // still answer, and must still answer CORRECTLY -- and `tap_stall_clocks_o`
  // is what that cost looks like measured instead of argued.
  // =========================================================================
  {
    std::printf("case 8: TERRAIN.TESS starves the tap -- the answer must survive\n");
    Sim s;
    s.reset();
    s.eng.out[0]  = 2 << 16;
    s.eng.present = 0x0F;
    s.d.tick_i = 50;
    s.bank(0, 100, true, kParams);
    s.open_patch(10);
    const Foot f = whole_patch(0, 0);
    s.add(f.x0, f.z0, f.x1, f.z1, 1);
    s.fill(0, 0, kBase, kScar, kBottom);

    const uint32_t stall_before = s.d.tap_stall_clocks_o;

    // the owner takes the port on three cycles out of four
    s.d.o_lat_vi_i = 1;
    s.d.o_lat_vj_i = 1;
    s.d.req_tap_x_i     = static_cast<uint32_t>(kTapX);
    s.d.req_tap_z_i     = static_cast<uint32_t>(kTapZ);
    s.d.req_tap_valid_i = 1;
    int guard = 0;
    bool taken = false;
    while (!taken && guard++ < 100000) {
      s.d.o_lat_req_i = (guard % 4) != 0;
      s.eng.drive(s.d);
      s.d.eval();
      taken = s.d.req_tap_ready_o != 0;
      s.step();
    }
    s.d.req_tap_valid_i = 0;
    Sim::Answer a;
    guard = 0;
    for (;;) {
      s.d.o_lat_req_i = (guard % 4) != 0;
      s.eng.drive(s.d);
      s.d.eval();
      if (s.d.rsp_tap_valid_o) {
        a.height    = static_cast<int32_t>(s.d.rsp_tap_height_o);
        a.no_ground = s.d.rsp_tap_no_ground_o != 0;
        s.step();
        break;
      }
      s.step();
      if (++guard > 100000) { ck(false, "case 8: the tap never answered under starvation"); break; }
    }
    s.d.o_lat_req_i = 0;

    ck(!a.no_ground, "the tap answered despite the owner taking the port");
    ck(s.d.tap_stall_clocks_o > stall_before,
       "THE INSTRUMENT FIRES: tap_stall_clocks_o counted the borrowed cycles");

    const zref::terrain::ComposedLattice lat = s.oracle_lattice(0, 0);
    const zref::terrain::ColumnResult r =
        zref::terrain::column_query(lat, zref::fx16{kTapX}, zref::fx16{kTapZ});
    ck_eq(a.height, r.top.raw,
          "and the STARVED answer still equals zref::terrain::column_query.top");
  }

  // =========================================================================
  // CASE 9 -- AN OFF-PATCH TAP IS CORRECT, NOT A FAULT
  // The tap's own law, and the reason this packet does not read an off-patch
  // answer as a defect: "`taps_void_o` and `taps_off_patch_o` are CORRECT -- a
  // creature over a chasm, OR OVER A PATCH THAT IS NOT STAGED THIS CYCLE, has
  // no ground and must say so. `place_mismatch_o` and `pitch_bad_o` are
  // FAULTS." A bench that let the two totals merge would report a healthy scene
  // as broken. This case is also the MEASUREMENT behind the packet's finding
  // that the compose cache stages exactly one patch.
  // =========================================================================
  {
    std::printf("case 9: an off-patch tap is a CORRECT refusal, counted apart from faults\n");
    Sim s;
    s.reset();
    s.open_patch(11);
    s.fill(/*patch_ix=*/0, /*patch_iz=*/0, kBase, kScar, kBottom);

    const uint32_t off_before   = s.d.taps_off_patch_o;
    const uint32_t fault_before = s.d.place_mismatch_o + s.d.pitch_bad_o;

    // a world point on patch 3, while patch 0 is the one staged
    const Sim::Answer a = s.tap(place_axis(3, 4) + kD / 3, place_axis(0, 4) + kD / 3);

    ck(a.no_ground, "an off-patch tap answers no_ground -- the safe answer");
    ck(s.d.taps_off_patch_o > off_before,
       "THE INSTRUMENT FIRES: taps_off_patch_o counted the correct refusal");
    ck_eq(s.d.place_mismatch_o + s.d.pitch_bad_o, fault_before,
          "and NO fault counter moved -- a correct refusal is not a defect");

    // the same point, on the patch that IS staged, still answers
    const Sim::Answer b = s.tap(kTapX, kTapZ);
    ck(!b.no_ground, "the staged patch still answers beside it");
  }

  // =========================================================================
  // CASE 10 -- THE REPLAY LANDS INSIDE THE INTAKE WINDOW, AT CONSOLE SCALE
  // (packet EARTHLOCK, 2026-09-25)
  //
  // Every case above calls `bank()`, which DRAINS the adapter's intake before
  // replaying. That models the real console's separation faithfully -- and it
  // means none of them can see the ordering COMPOSEPUB found. This case is
  // case 2 with the drain removed and nothing else changed: the section 9.1
  // replay lands while the divider is still running and the banks are still
  // the previous frame's.
  //
  // BEFORE THE REPAIR this is the 1,089-run / 1,089-noprog measurement -- a
  // field that ran, cost the engine every cycle it should, and moved nothing,
  // with every other census balancing perfectly. It is asserted here the RIGHT
  // WAY ROUND: the field MUST reach the consumer. Nothing below mentions the
  // race, so this case keeps its meaning now that there is no race to miss --
  // and it FAILS if the interlock is ever reverted, which the drained cases
  // cannot do.
  // =========================================================================
  {
    std::printf("case 10: the replay lands INSIDE the intake window -- the field must still land\n");
    Sim s;
    s.reset();
    const int32_t kLift = 7 << 16;
    s.eng.out[0]  = kLift;
    s.eng.present = 0x0F;

    s.d.tick_i = 50;
    s.bank_no_drain(/*start_tick=*/0, /*duration=*/100, /*last=*/true, kParams);
    // NO DRAIN between these two lines. That is the whole case.
    s.open_patch(10);
    const Foot f = whole_patch(0, 0);
    s.add(f.x0, f.z0, f.x1, f.z1, /*cmd=*/1);
    s.d.eval();
    ck_eq(s.d.fields_active_o, 1, "one accepted list entry -> fields_active_o == 1");

    s.fill(0, 0, kBase, kScar, kBottom);

    std::printf("  engine runs=%llu  adapter runs_o=%u  noprog=%u  faults=%u\n",
                static_cast<unsigned long long>(s.eng.runs), s.d.efa_runs_o,
                s.d.efa_noprog_o, s.d.efa_faults_o);

    ck_eq(s.d.efa_noprog_o, 0,
          "THE RESIDENT FLAG SURVIVED THE IN-FLIGHT INTAKE: no evaluation was refused");
    ck_eq(static_cast<int64_t>(s.eng.runs), kVerts,
          "the engine ran once per vertex, as a whole-patch footprint must");
    ck_eq(s.d.efa_runs_o, static_cast<uint32_t>(kVerts),
          "and every one of those runs RETIRED -- runs that move nothing are the defect");

    bool every_vertex_lifted = true;
    for (size_t i = 0; i < s.streamed_top.size(); ++i)
      if (s.streamed_top[i] - s.streamed_compose_top[i] != kLift)
        every_vertex_lifted = false;
    ck(every_vertex_lifted,
       "live_top - compose_top == the field's out-lane 0 at EVERY vertex");

    const Sim::Answer a = s.tap(kTapX, kTapZ);
    ck(!a.no_ground, "the consumer answered");
    ck_eq(a.height - baseline_tap_height, kLift,
          "THE CONSUMER'S HEIGHT MOVED BY THE FIELD'S CONTRIBUTION");

    const zref::terrain::ComposedLattice lat = s.oracle_lattice(0, 0);
    const zref::terrain::ColumnResult r =
        zref::terrain::column_query(lat, zref::fx16{kTapX}, zref::fx16{kTapZ});
    ck_eq(a.height, r.top.raw, "tap height == zref::terrain::column_query.top");

    ck_eq(s.d.efa_lane_desync_o, 0, "lane shadow stayed aligned across the hold");
    ck_eq(s.d.efa_faults_o, 0, "no engine fault");
    ck_eq(s.d.place_mismatch_o, 0, "no placement fault");
  }

  std::printf("\ncomposepub_acceptance: %d checks, %d failures\n", g_checks, g_fails);
  return g_fails == 0 ? 0 : 1;
}
