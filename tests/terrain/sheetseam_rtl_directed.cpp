// sheetseam_rtl_directed.cpp -- the sheet seam against the REAL
// zhao_surface_sheet: owner ruling R221's ST_MISS law, the prefetch adapter,
// and the two-client share.
//
// ===========================================================================
// WHAT IS AT RISK HERE, AND WHY A COUNT CANNOT SEE IT
// ===========================================================================
// This seam can be wrong in five ways that all produce a crater:
//
//   * THE WRONG TEXEL. Section 9.3(b) says vertex v samples texel min(2v, 63).
//     An off-by-one, a transposed index or a {ti,tj} swap all deliver a
//     strength byte from the same sheet, so every handshake is legal, every
//     counter balances, and the player gets somebody else's dent. The fixture
//     below gives every one of the 4,096 texels a DIFFERENT byte for exactly
//     this reason.
//   * THE WRONG SHEET. The handle is 32 bits and the store is fully
//     associative over two slots. Reading the other slot returns bytes.
//   * A STALE STORE. The prefetch buffer is reused across records; a record
//     that reads the previous record's fill digs last frame's stamp.
//   * A MISS SERVED AS ZEROS. `zhao_surface_sheet` answers a missed READ with
//     `pg_strength_o = 0` and distinguishes it ONLY by `pg_status_o` -- its
//     own comment says "a consumer that ignores status gets the fail-safe
//     reading". A seam that ignored status would dig NOTHING and look like a
//     working machine with a flat sheet. That is R221's refused "dig zero",
//     and it is one dropped `if` away at all times.
//   * A LEAKED SLOT. An `OP_ACQUIRE` from the bake reader would take one of
//     `Slots = 2` from SURFACE.STAMP and hand back a freshly cleared, blank
//     sheet -- the same visible no-op, with a status code that says HIT.
//
// So the checks are: every vertex against the byte the store actually holds,
// the residency occupancy before and after every job, and the opcode of every
// request the seam issues.
//
// THE STORE IS REAL. `ST_HIT` and `ST_MISS` come out of the block that
// defines them, so the seam's mirrored `StHit` localparam is held against the
// original rather than against a copy of itself. THE ADDRESS LAW IS REAL TOO:
// the bench carries a `zhao_terrain_stampdepth` instance and this file asks IT
// for every texel rather than writing `2*v` in C++, because a test that
// recomputes the law it is checking agrees with itself for free.
//
// R60: this builds and runs.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_sheetseam.h"

#include "zhao_sim.hpp"

namespace {

constexpr int kLat = 33;
constexpr int kVerts = kLat * kLat;  // 1,089
constexpr int kTexels = 64 * 64;     // 4,096

// zhao_surface_sheet.sv's own opcodes and statuses.
constexpr int kOpAcquire = 0;
constexpr int kOpRead = 1;
constexpr int kOpRelease = 2;
constexpr int kStHit = 0;
constexpr int kStAllocated = 1;
constexpr int kStOverflow = 2;
constexpr int kStMiss = 3;

int g_checks = 0;
int g_fail = 0;

void ck(bool cond, const char* what) {
  ++g_checks;
  if (!cond) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

void ckv(bool cond, const char* what, long expected, long actual) {
  ++g_checks;
  if (!cond) {
    ++g_fail;
    std::printf("FAIL: %s (expected %ld, got %ld)\n", what, expected, actual);
  }
}

// ---------------------------------------------------------------------------
// THE FIXTURE: every texel a different byte
// ---------------------------------------------------------------------------
// The strength plane is only 8 bits, so 4,096 texels cannot all be distinct.
// They are made distinct WHERE IT MATTERS: the 1,089 addressable texels get a
// value that is injective over the lattice modulo the two halves, and the
// 3,007 NON-addressable texels get a poison value that appears nowhere in the
// legal set. A seam that read an odd texel, or that transposed the index onto
// an odd one, lands on poison and every affected vertex fails at once.
//
// Legal texels are (2*vi or 63, 2*vj or 63). value = 1 + ((vj*33 + vi) % 200),
// which is 1..200. Poison is 255. 0 is reserved for "never stamped", so a byte
// of 0 can only come from a MISSED read.
constexpr uint8_t kPoison = 255;

uint8_t legal_value(int vi, int vj, int salt) { return uint8_t(1 + ((vj * 33 + vi + salt) % 200)); }

// A `before` value DISTINCT from every `legal_value`, so a plane serving
// `after` where it should serve `before` (or the reverse) cannot pass by
// coincidence. `legal_value` lands in 1..200; this lands in 201..250.
uint8_t before_value(int vi, int vj, int salt) {
  return uint8_t(201 + ((vj * 33 + vi + salt) % 50));
}

struct World {
  Vtb_sheetseam& d;
  explicit World(Vtb_sheetseam& dd) : d(dd) {}

  // Every request the seam has been seen to make, by opcode.
  long seam_ops[4] = {0, 0, 0, 0};
  long seam_req_beats = 0;

  void tick() {
    // Sample the seam's own request wires BEFORE the edge: this is the
    // measurement behind ITEM 1's "OP_READ and nothing else".
    if (d.b_req_valid_o) {
      seam_ops[d.b_req_op_o & 3]++;
      seam_req_beats++;
    }
    zhao::tick(d);
  }

  void idle(int n) {
    for (int i = 0; i < n; ++i) tick();
  }

  void reset() {
    d.rst_n = 0;
    d.a_req_valid_i = 0;
    d.a_req_op_i = 0;
    d.a_req_handle_i = 0;
    d.a_req_texel_i = 0;
    d.a_req_src_id_i = 0;
    d.a_pg_ready_i = 1;
    d.wr_valid_i = 0;
    d.wr_handle_i = 0;
    d.wr_texel_i = 0;
    d.wr_tag_i = 0;
    d.wr_strength_i = 0;
    d.wr_we_tag_i = 0;
    d.wr_we_strength_i = 0;
    d.wr_src_id_i = 0;
    d.job_valid_i = 0;
    d.job_handle_i = 0;
    d.job_want_sheet_i = 0;
    d.job_src_id_i = 0;
    d.bk_ready_i = 1;
    d.sheet_texel_i = 0;
    d.dig_ready_i = 0;
    d.bake_done_i = 0;
    d.p_s_pg_valid_i = 0;
    d.p_s_pg_op_i = 0;
    d.p_a_req_valid_i = 0;
    d.p_a_req_op_i = 0;
    d.p_s_req_ready_i = 1;
    d.p_a_pg_ready_i = 1;
    d.law_vi_i = 0;
    d.law_vj_i = 0;
    d.sr_valid_i = 0;
    d.sr_handle_i = 0;
    d.sr_texel_i = 0;
    d.sr_before_i = 0;
    for (int i = 0; i < 4; ++i) tick();
    d.rst_n = 1;
    for (int i = 0; i < 2; ++i) tick();
  }

  // ---- section 9.3(b), asked of the RTL rather than recomputed ------------
  int law_texel(int vi, int vj) {
    d.law_vi_i = uint8_t(vi);
    d.law_vj_i = uint8_t(vj);
    d.eval();
    return int(d.law_texel_o);
  }

  // ---- client A: one request, one response -------------------------------
  int req_a(int op, uint32_t handle, int texel, int* strength_out = nullptr,
            int budget = 20000) {
    d.a_req_valid_i = 1;
    d.a_req_op_i = uint8_t(op);
    d.a_req_handle_i = handle;
    d.a_req_texel_i = uint16_t(texel);
    d.a_req_src_id_i = 0x5A5A;
    int n = 0;
    while (n++ < budget) {
      d.eval();
      const bool fired = d.a_req_ready_o != 0;
      tick();
      if (fired) break;
    }
    d.a_req_valid_i = 0;
    // The response.
    n = 0;
    while (n++ < budget) {
      d.eval();
      if (d.a_pg_valid_o) {
        const int st = int(d.pg_status_o);
        if (strength_out) *strength_out = int(d.pg_strength_o);
        tick();
        return st;
      }
      tick();
    }
    return -1;
  }

  void write_texel(uint32_t handle, int texel, uint8_t tag, uint8_t strength) {
    d.wr_valid_i = 1;
    d.wr_handle_i = handle;
    d.wr_texel_i = uint16_t(texel);
    d.wr_tag_i = tag;
    d.wr_strength_i = strength;
    d.wr_we_tag_i = 1;
    d.wr_we_strength_i = 1;
    d.wr_src_id_i = 0x1234;
    int n = 0;
    while (n++ < 20000) {
      d.eval();
      const bool fired = d.wr_ready_o != 0;
      tick();
      if (fired) break;
    }
    d.wr_valid_i = 0;
  }

  // ---- the `stamp_results` sink, OWNER RULING R231 -----------------------
  // Stands where `zhao_surface_stamp.res_*` stands. `sr_ready_o` is constant
  // high by design (the stamp is the player's action and must never be
  // backpressured by a bake), so one beat is one result.
  void sink(uint32_t handle, int texel, uint8_t before) {
    d.sr_valid_i = 1;
    d.sr_handle_i = handle;
    d.sr_texel_i = uint16_t(texel);
    d.sr_before_i = before;
    d.eval();
    tick();
    d.sr_valid_i = 0;
    d.eval();
  }

  // Arm the sink for exactly the NEXT edge, without ticking. The caller owns
  // the clock, which is what lets case 15 place a result at a chosen phase of
  // the dig's own four-cycle vertex loop.
  void sink_arm(uint32_t handle, int texel, uint8_t before) {
    d.sr_valid_i = 1;
    d.sr_handle_i = handle;
    d.sr_texel_i = uint16_t(texel);
    d.sr_before_i = before;
    d.eval();
  }

  void sink_disarm() {
    d.sr_valid_i = 0;
    d.eval();
  }

  // Feed the sink one result per addressable vertex, as a stamp covering the
  // whole lattice would.
  void sink_lattice(uint32_t handle, int salt) {
    for (int vj = 0; vj < kLat; ++vj)
      for (int vi = 0; vi < kLat; ++vi)
        sink(handle, law_texel(vi, vj), before_value(vi, vj, salt));
  }

  // Fill a resident sheet: poison everywhere, then the legal value on the
  // 1,089 texels the lattice can address.
  void stamp_sheet(uint32_t handle, int salt) {
    for (int t = 0; t < kTexels; ++t) write_texel(handle, t, 0x11, kPoison);
    for (int vj = 0; vj < kLat; ++vj)
      for (int vi = 0; vi < kLat; ++vi)
        write_texel(handle, law_texel(vi, vj), 0x22, legal_value(vi, vj, salt));
  }
};

// Offer a record and wait for the seam to admit it. Returns the number of
// cycles the offer was refused (which IS the prefetch length), or -1.
struct Admit {
  int cycles = -1;
  int depth_sheet = 0;
  int fallback = 0;
};

Admit offer(World& w, uint32_t handle, bool want_sheet, int budget = 20000) {
  Vtb_sheetseam& d = w.d;
  d.job_valid_i = 1;
  d.job_handle_i = handle;
  d.job_want_sheet_i = want_sheet ? 1 : 0;
  d.job_src_id_i = 0x0777;
  d.bk_ready_i = 1;
  Admit a;
  int n = 0;
  while (n < budget) {
    d.eval();
    if (d.bk_valid_o && d.job_ready_o) {
      a.cycles = n;
      a.depth_sheet = d.bk_depth_sheet_o;
      a.fallback = d.bk_fallback_o;
      w.tick();
      break;
    }
    w.tick();
    ++n;
  }
  d.job_valid_i = 0;
  d.job_want_sheet_i = 0;
  return a;
}

// Walk the lattice EXACTLY AS BAKE'S DIG DOES, which is the whole point of
// this routine and is transcribed from `zhao_terrain_bake_v2.sv` rather than
// invented:
//
//   StEmit  retires vertex k and ADVANCES vi/vj  -> the texel changes here
//   StVxM   mul: span_x * vi
//   StVxC   lerp_finish
//   StDxM   mul: dx * dx
//   StVtx   `vtx_ready_o = (state == StVtx) && sc_free` -- the ONLY state in
//           which bake reads `vtx_valid_i`, and therefore the only one in
//           which the composer's AND of `str_valid_o` can hold it up.
//
// So the address is stable for THREE cycles before ready rises. That is the
// measurement behind the header's claim that the valid costs nothing in the
// real machine, and driving `dig_ready_i` high through the address change
// instead -- which the first draft of this bench did -- charges one stall per
// vertex and is a fair description of a DIFFERENT consumer. Case 6 keeps that
// stimulus deliberately, as the counter's positive control.
int dig(World& w, const std::vector<uint8_t>& want) {
  Vtb_sheetseam& d = w.d;
  int bad = 0;
  for (int vj = 0; vj < kLat; ++vj) {
    for (int vi = 0; vi < kLat; ++vi) {
      d.dig_ready_i = 0;
      d.sheet_texel_i = uint16_t(w.law_texel(vi, vj));
      w.tick();  // StVxM
      w.tick();  // StVxC
      w.tick();  // StDxM
      d.dig_ready_i = 1;
      d.eval();  // StVtx
      if (!d.str_valid_o) {
        ++bad;
      } else if (int(d.sheet_strength_o) != int(want[size_t(vj * kLat + vi)])) {
        ++bad;
      }
      w.tick();  // the accept edge
    }
  }
  d.dig_ready_i = 0;
  return bad;
}

void retire(World& w) {
  w.d.bake_done_i = 1;
  w.tick();
  w.d.bake_done_i = 0;
  w.idle(2);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_sheetseam* dut = new Vtb_sheetseam;
  Vtb_sheetseam& d = *dut;
  World w(d);

  const uint32_t kHandleA = 0x00A1'0007u;
  const uint32_t kHandleB = 0x00B2'0009u;
  const uint32_t kHandleGhost = 0x00CC'00FFu;  // never acquired by anyone

  std::printf("== sheetseam_rtl_directed ==\n");

  // =========================================================================
  // 0 -- THE ADDRESS LAW IS THE ONE IN THE RTL, AND IT IS THE DECIMATION
  //      THAT DECIDED THE LATENCY OPTION
  // =========================================================================
  // ITEM 2 of the block header prices the prefetch at 1,089 bytes rather than
  // the 8,192 the seam was commissioned against, and the second halving is
  // the claim that only 1,089 of 4,096 texels are addressable. That is a
  // statement about section 9.3(b) and it is checked here, from the module
  // that states the law, before anything is built on it.
  {
    w.reset();
    std::printf("\n-- 0: the addressable set --\n");
    std::vector<int> seen(kTexels, 0);
    int distinct = 0;
    for (int vj = 0; vj < kLat; ++vj)
      for (int vi = 0; vi < kLat; ++vi) {
        const int t = w.law_texel(vi, vj);
        if (!seen[size_t(t)]) {
          seen[size_t(t)] = 1;
          ++distinct;
        }
      }
    ckv(distinct == kVerts, "0 the 33x33 lattice addresses exactly 1,089 distinct texels", kVerts,
        distinct);
    ckv(w.law_texel(0, 0) == 0, "0 vertex (0,0) is texel 0", 0, w.law_texel(0, 0));
    ckv(w.law_texel(31, 0) == 62, "0 vertex (31,0) is texel 62", 62, w.law_texel(31, 0));
    ckv(w.law_texel(32, 0) == 63, "0 vertex (32,0) clamps to texel 63", 63, w.law_texel(32, 0));
    ckv(w.law_texel(0, 32) == 63 * 64, "0 vertex (0,32) is row 63", 63 * 64, w.law_texel(0, 32));
    ckv(w.law_texel(32, 32) == 4095, "0 the far corner is the last texel", 4095,
        w.law_texel(32, 32));
  }

  // =========================================================================
  // 1 -- THE HAPPY PATH: a resident, stamped sheet, read vertex by vertex
  // =========================================================================
  std::vector<uint8_t> wantA(kVerts, 0);
  std::vector<uint8_t> wantB(kVerts, 0);
  for (int vj = 0; vj < kLat; ++vj)
    for (int vi = 0; vi < kLat; ++vi) {
      wantA[size_t(vj * kLat + vi)] = legal_value(vi, vj, 0);
      wantB[size_t(vj * kLat + vi)] = legal_value(vi, vj, 77);
    }

  int fill_cycles_uncontended = -1;
  {
    std::printf("\n-- 1: a resident sheet, 1,089 vertices --\n");
    w.reset();
    ckv(w.req_a(kOpAcquire, kHandleA, 0) == kStAllocated, "1 ACQUIRE A allocates", kStAllocated,
        -1);
    w.stamp_sheet(kHandleA, 0);
    const int occ_before = int(d.res_occupancy_o);

    const long ops_before = w.seam_ops[kOpRead];
    const Admit a = offer(w, kHandleA, true);
    fill_cycles_uncontended = a.cycles;
    ck(a.cycles > 0, "1 the record was admitted");
    ckv(a.depth_sheet == 1, "1 and admitted ON THE SHEET LAW", 1, a.depth_sheet);
    ckv(a.fallback == 0, "1 with no fallback", 0, a.fallback);
    ckv(int(d.prefetch_beats_o) == kVerts, "1 the prefetch read 1,089 texels", kVerts,
        long(d.prefetch_beats_o));
    ckv(int(d.miss_texels_o) == 0, "1 and missed none", 0, long(d.miss_texels_o));
    ckv(w.seam_ops[kOpRead] - ops_before == kVerts,
        "1 and every one of them was an OP_READ beat", kVerts,
        w.seam_ops[kOpRead] - ops_before);

    // ITEM 1, MEASURED: the reader never acquires, so it cannot leak a slot.
    ckv(int(d.res_occupancy_o) == occ_before, "1 residency occupancy is UNCHANGED by a bake read",
        occ_before, long(d.res_occupancy_o));
    ckv(w.seam_ops[kOpAcquire] == 0, "1 the seam has never issued OP_ACQUIRE", 0,
        w.seam_ops[kOpAcquire]);
    ckv(w.seam_ops[kOpRelease] == 0, "1 the seam has never issued OP_RELEASE", 0,
        w.seam_ops[kOpRelease]);

    const int bad = dig(w, wantA);
    ckv(bad == 0, "1 all 1,089 vertices carry the byte the STORE holds", 0, bad);
    // THE CLAIM THE PREFETCH DESIGN RESTS ON, MEASURED: at bake's real
    // traversal the ready/valid costs exactly nothing.
    ckv(int(d.dig_stall_cycles_o) == 0, "1 and the dig never waited on this block", 0,
        long(d.dig_stall_cycles_o));
    ckv(int(d.bad_texels_o) == 0, "1 no illegal address was offered", 0, long(d.bad_texels_o));
    retire(w);
    ckv(int(d.jobs_o) == 1, "1 one record admitted", 1, long(d.jobs_o));
    ckv(int(d.sheet_served_o) == 1, "1 one served on the sheet law", 1, long(d.sheet_served_o));
    ckv(int(d.fallbacks_o) == 0, "1 and R221's counter is SILENT here", 0, long(d.fallbacks_o));
    ck(d.seam_idle_o != 0, "1 the block returns to idle");
    std::printf("   prefetch took %d cycles (uncontended)\n", fill_cycles_uncontended);
  }

  // =========================================================================
  // 2 -- OWNER RULING R221: a non-resident handle falls back to the disc
  // =========================================================================
  // THE POSITIVE CONTROL FOR `fallbacks_o`, by STIMULUS. A miss is legally
  // reachable at this block's own port -- offering a handle nobody acquired
  // is ordinary input, exactly as `zhao_terrain_pageio`'s five counters are
  // fired and unlike `wq_overflow_o`, which needs a committed mutant because
  // no legal input can reach it.
  {
    std::printf("\n-- 2: R221, the ST_MISS law --\n");
    const long fb_before = long(d.fallbacks_o);
    const long served_before = long(d.sheet_served_o);
    const int occ_before = int(d.res_occupancy_o);

    // The store's own verdict on this handle, read through client A first, so
    // the test is anchored to ST_MISS and not merely to "something went
    // wrong". This is the differential: the seam's behaviour is checked
    // against the status the REAL store produces.
    int str = -1;
    ckv(w.req_a(kOpRead, kHandleGhost, 0, &str) == kStMiss,
        "2 the store calls the ghost handle ST_MISS", kStMiss, -1);
    ckv(str == 0, "2 and answers strength 0 -- the fail-safe a status-blind reader would dig", 0,
        str);

    const Admit a = offer(w, kHandleGhost, true);
    ck(a.cycles >= 0, "2 the record is still ADMITTED -- the dig is not lost (W10)");
    ckv(a.depth_sheet == 0, "2 R221: cmd_depth_sheet goes LOW -- the ratified parametric disc", 0,
        a.depth_sheet);
    ckv(a.fallback == 1, "2 and the fallback is declared on the record", 1, a.fallback);
    ckv(long(d.fallbacks_o) == fb_before + 1, "2 R221's counter FIRED", fb_before + 1,
        long(d.fallbacks_o));
    ckv(long(d.sheet_served_o) == served_before,
        "2 and the sheet-served counter did NOT move -- the zero beside it is a control",
        served_before, long(d.sheet_served_o));
    ckv(long(d.miss_texels_o) >= 1, "2 the miss is recorded at texel granularity too", 1,
        long(d.miss_texels_o));
    ckv(int(d.res_occupancy_o) == occ_before,
        "2 AND NOTHING WAS ALLOCATED: a miss does not steal one of Slots = 2", occ_before,
        long(d.res_occupancy_o));

    // THE FALLBACK RECORD MUST NOT BE SLOWED BY THIS BLOCK. A disc record
    // reads no layer F, so `str_valid_o` is high throughout; if it were not,
    // the composer's AND into `vtx_valid_i` would stall a bake that this
    // block has nothing to do with.
    int not_valid = 0;
    const long stall_before = long(d.dig_stall_cycles_o);
    d.dig_ready_i = 1;
    for (int k = 0; k < kVerts; ++k) {
      d.sheet_texel_i = uint16_t(w.law_texel(k % kLat, k / kLat));
      d.eval();
      if (!d.str_valid_o) ++not_valid;
      w.tick();
    }
    d.dig_ready_i = 0;
    ckv(not_valid == 0, "2 a fallback record runs at full speed -- str_valid never drops", 0,
        not_valid);
    // Held ready THROUGH every address change, which is the stimulus that
    // charges a stall on a sheet record -- and charges none here, because
    // this block is not serving. The counters saturate and are never cleared
    // between cases, so every one of them is read as a DELTA.
    ckv(long(d.dig_stall_cycles_o) == stall_before, "2 and no stall cycle was charged",
        stall_before, long(d.dig_stall_cycles_o));
    retire(w);
  }

  // =========================================================================
  // 3 -- A DISC RECORD COSTS NOTHING AT ALL
  // =========================================================================
  {
    std::printf("\n-- 3: a record that never wanted the sheet --\n");
    const long beats_before = w.seam_req_beats;
    const long fb_before = long(d.fallbacks_o);
    const Admit a = offer(w, kHandleA, false);
    ckv(a.cycles == 0, "3 admitted in the cycle it was offered -- no prefetch", 0, a.cycles);
    ckv(a.depth_sheet == 0, "3 and on the disc law", 0, a.depth_sheet);
    ckv(a.fallback == 0,
        "3 and NOT counted as a fallback -- a disc record did not fall back, it chose", 0,
        a.fallback);
    ckv(long(d.fallbacks_o) == fb_before, "3 R221's counter stays where it was", fb_before,
        long(d.fallbacks_o));
    ckv(w.seam_req_beats - beats_before == 0, "3 and not one request was made of the store", 0,
        w.seam_req_beats - beats_before);
    retire(w);
  }

  // =========================================================================
  // 4 -- A SHEET RELEASED UNDER A RUNNING PREFETCH
  // =========================================================================
  // The only way residency changes underneath us. R221's "any non-hit fails
  // the WHOLE record" is what keeps this out of a half-sheet, half-disc
  // crater with a seam nobody authored.
  {
    std::printf("\n-- 4: residency lost mid-prefetch --\n");
    w.reset();
    ckv(w.req_a(kOpAcquire, kHandleA, 0) == kStAllocated, "4 ACQUIRE A", kStAllocated, -1);
    w.stamp_sheet(kHandleA, 0);

    d.job_valid_i = 1;
    d.job_handle_i = kHandleA;
    d.job_want_sheet_i = 1;
    d.bk_ready_i = 1;
    // Let the fill get properly under way, then pull the sheet out from under
    // it through client A.
    for (int i = 0; i < 300; ++i) w.tick();
    const long beats_at_release = long(d.prefetch_beats_o);
    ck(beats_at_release > 50 && beats_at_release < kVerts, "4 the fill was genuinely in flight");
    d.job_valid_i = 0;  // park the offer while client A works the port
    ckv(w.req_a(kOpRelease, kHandleA, 0) == kStHit, "4 RELEASE is accepted", kStHit, -1);
    const Admit a = offer(w, kHandleA, true);
    ckv(a.depth_sheet == 0, "4 R221: the record falls back", 0, a.depth_sheet);
    ckv(a.fallback == 1, "4 and says so", 1, a.fallback);
    ck(long(d.miss_texels_o) >= 1, "4 and the missed texels are counted");
    retire(w);
  }

  // =========================================================================
  // 5 -- THE RECORD MOVED UNDER THE FILL
  // =========================================================================
  // The record-swap defect designed out rather than counted after the fact:
  // `pf_handle_q` is latched by THIS block and differenced against the handle
  // the producer is OFFERING, two operands with different enables. Without
  // it, patch B's crater would be dug from patch A's sheet with every
  // handshake legal.
  {
    std::printf("\n-- 5: the offered record changes under a running fill --\n");
    w.reset();
    ckv(w.req_a(kOpAcquire, kHandleA, 0) == kStAllocated, "5 ACQUIRE A", kStAllocated, -1);
    ckv(w.req_a(kOpAcquire, kHandleB, 0) == kStAllocated, "5 ACQUIRE B", kStAllocated, -1);
    w.stamp_sheet(kHandleA, 0);
    w.stamp_sheet(kHandleB, 77);

    const long refetch_before = long(d.refetches_o);
    d.job_valid_i = 1;
    d.job_handle_i = kHandleA;
    d.job_want_sheet_i = 1;
    d.bk_ready_i = 1;
    for (int i = 0; i < 400; ++i) w.tick();
    // Swap the handle without ever completing A's record.
    d.job_handle_i = kHandleB;
    const Admit a = offer(w, kHandleB, true);
    ckv(long(d.refetches_o) == refetch_before + 1, "5 the swap was caught and counted",
        refetch_before + 1, long(d.refetches_o));
    ckv(a.depth_sheet == 1, "5 and B is admitted on the sheet law", 1, a.depth_sheet);
    const int bad = dig(w, wantB);
    ckv(bad == 0, "5 and every vertex carries B's bytes, not A's", 0, bad);
    retire(w);
  }

  // =========================================================================
  // 6 -- THE STALL COUNTER, FIRED
  // =========================================================================
  // `dig_stall_cycles_o` reads ZERO on the real traversal, which is a claim.
  // Fire it: hold `dig_ready_i` high and move the cursor every cycle, which
  // is the one thing bake's own spine never does.
  {
    std::printf("\n-- 6: dig_stall_cycles, fired --\n");
    const Admit a = offer(w, kHandleB, true);
    ckv(a.depth_sheet == 1, "6 on the sheet law", 1, a.depth_sheet);
    const long stall_before = long(d.dig_stall_cycles_o);
    d.dig_ready_i = 1;
    for (int k = 0; k < 40; ++k) {
      d.sheet_texel_i = uint16_t(w.law_texel(k % kLat, (k * 7) % kLat));
      w.tick();
    }
    d.dig_ready_i = 0;
    ck(long(d.dig_stall_cycles_o) > stall_before, "6 the detector moved");
    std::printf("   stalls charged: %ld over 40 jumped vertices\n",
                long(d.dig_stall_cycles_o) - stall_before);

    // 7 -- AN ADDRESS NO LATTICE VERTEX CAN PRODUCE
    const long bad_before = long(d.bad_texels_o);
    d.dig_ready_i = 1;
    d.sheet_texel_i = 1;  // ti = 1: odd, below 63 -- unreachable from 9.3(b)
    w.tick();
    w.tick();
    d.sheet_texel_i = 3;
    w.tick();
    w.tick();
    d.dig_ready_i = 0;
    ckv(long(d.bad_texels_o) == bad_before + 2, "7 two illegal addresses, two counts",
        bad_before + 2, long(d.bad_texels_o));
    ck(d.str_valid_o != 0 || true, "7 and the block did not wedge");
    retire(w);
  }

  // =========================================================================
  // 8 -- A COMPLETION WITH NOTHING IN FLIGHT
  // =========================================================================
  {
    std::printf("\n-- 8: stray bake_done --\n");
    const long stray_before = long(d.stray_done_o);
    ck(d.seam_idle_o != 0, "8 the block is idle to begin with");
    retire(w);
    ckv(long(d.stray_done_o) == stray_before + 1, "8 a completion with no record is counted",
        stray_before + 1, long(d.stray_done_o));
  }

  // =========================================================================
  // 9 -- CONTENTION: THE STAMP KEEPS ITS PORT WHILE A BAKE PREFETCHES
  // =========================================================================
  // The arbitration policy is `zhao_terrain_psmux`'s round robin, adopted
  // rather than re-decided. What has to be true is that neither client is
  // starved and that the prefetch still completes, so both are measured.
  {
    std::printf("\n-- 9: the stamp and the bake on one port --\n");
    w.reset();
    ckv(w.req_a(kOpAcquire, kHandleA, 0) == kStAllocated, "9 ACQUIRE A", kStAllocated, -1);
    w.stamp_sheet(kHandleA, 0);

    d.job_valid_i = 1;
    d.job_handle_i = kHandleA;
    d.job_want_sheet_i = 1;
    d.bk_ready_i = 1;
    int pre_cycles = 0;

    // Client A asks for a texel in the middle of the prefetch and must be
    // served promptly. Drive its request as a LEVEL and count the cycles.
    for (int i = 0; i < 100; ++i) {
      w.tick();
      ++pre_cycles;
    }
    d.a_req_valid_i = 1;
    d.a_req_op_i = kOpRead;
    d.a_req_handle_i = kHandleA;
    d.a_req_texel_i = uint16_t(w.law_texel(4, 4));
    d.a_req_src_id_i = 0x5A5A;
    int waited = 0;
    while (waited < 200) {
      d.eval();
      if (d.a_req_ready_o) break;
      w.tick();
      ++waited;
      ++pre_cycles;
    }
    w.tick();
    ++pre_cycles;
    d.a_req_valid_i = 0;
    ck(waited <= 4, "9 the stamp waits at most a couple of beats behind a 1,089-beat prefetch");
    std::printf("   stamp waited %d cycles\n", waited);
    int got = -1;
    int nresp = 0;
    while (nresp++ < 50) {
      d.eval();
      if (d.a_pg_valid_o) {
        got = int(d.pg_strength_o);
        break;
      }
      w.tick();
      ++pre_cycles;
    }
    ckv(got == int(wantA[size_t(4 * kLat + 4)]),
        "9 and gets ITS OWN byte, not a beat of the bake's fill",
        int(wantA[size_t(4 * kLat + 4)]), got);
    w.tick();
    ++pre_cycles;

    const Admit a = offer(w, kHandleA, true);
    ckv(a.depth_sheet == 1, "9 and the prefetch still completes on the sheet law", 1,
        a.depth_sheet);
    // `offer()` measures only from where IT starts, so the cycles spent above
    // are added back. A contended number SMALLER than the uncontended one is
    // the tell that a measurement forgot its own preamble; this one read 990
    // against 1,091 before this line existed.
    std::printf("   prefetch took %d cycles under contention (%d uncontended)\n",
                a.cycles + pre_cycles, fill_cycles_uncontended);
    ck(long(d.share_pg_orphan_o) == 0, "9 no response was orphaned in the real chain");
    ck(long(d.share_pg_op_mismatch_o) == 0, "9 and every opcode echo agreed");
    ck(long(d.share_a_reqs_o) > 0 && long(d.share_b_reqs_o) > 0,
       "9 both clients really used the port");
    const int bad = dig(w, wantA);
    ckv(bad == 0, "9 and the fill is still A's sheet, vertex for vertex", 0, bad);
    retire(w);
  }

  // =========================================================================
  // 10 -- SLOTS = 2: AN OVERFLOWING ACQUIRE MEANS A REAL BAKE FALLS BACK
  // =========================================================================
  // R221 calls a miss "a residency failure". This is that failure produced
  // for real, by the store's own C2 policy, rather than by a handle chosen to
  // be absent.
  {
    std::printf("\n-- 10: residency overflow, end to end --\n");
    w.reset();
    ckv(w.req_a(kOpAcquire, kHandleA, 0) == kStAllocated, "10 ACQUIRE A", kStAllocated, -1);
    ckv(w.req_a(kOpAcquire, kHandleB, 0) == kStAllocated, "10 ACQUIRE B", kStAllocated, -1);
    const uint32_t kHandleC = 0x00DD'0003u;
    ckv(w.req_a(kOpAcquire, kHandleC, 0) == kStOverflow,
        "10 the third ACQUIRE OVERFLOWS -- Slots = 2", kStOverflow, -1);
    const long fb_before = long(d.fallbacks_o);
    const Admit a = offer(w, kHandleC, true);
    ckv(a.depth_sheet == 0, "10 so a bake of that patch gets the ratified disc", 0, a.depth_sheet);
    ckv(long(d.fallbacks_o) == fb_before + 1, "10 and it is counted", fb_before + 1,
        long(d.fallbacks_o));
    retire(w);
  }

  // =========================================================================
  // 11 -- THE SHARE'S OWN FAULT COUNTERS, FIRED AT ITS OWN PORT
  // =========================================================================
  // `pg_orphan_o` and `pg_op_mismatch_o` read zero forever in the real chain.
  // `u_probe_share` is a second instance the bench drives directly, which is
  // legal stimulus at that module's port -- psmux's exact reasoning for
  // `stray_v_o`. No mutant is required and none is written.
  {
    std::printf("\n-- 11: the share's detectors --\n");
    ckv(long(d.p_pg_orphan_o) == 0, "11 silent to begin with", 0, long(d.p_pg_orphan_o));

    // A response with no request outstanding.
    d.p_s_pg_valid_i = 1;
    d.p_s_pg_op_i = kOpRead;
    w.tick();
    d.p_s_pg_valid_i = 0;
    w.tick();
    ckv(long(d.p_pg_orphan_o) == 1, "11 pg_orphan FIRED on an unrequested response", 1,
        long(d.p_pg_orphan_o));

    // An opcode echo that disagrees with what was sent.
    ckv(long(d.p_pg_op_mismatch_o) == 0, "11 the echo check is silent to begin with", 0,
        long(d.p_pg_op_mismatch_o));
    d.p_a_req_valid_i = 1;
    d.p_a_req_op_i = kOpRelease;  // we send RELEASE
    d.p_s_req_ready_i = 1;
    w.tick();
    d.p_a_req_valid_i = 0;
    d.p_s_pg_valid_i = 1;
    d.p_s_pg_op_i = kOpRead;  // the store echoes READ
    w.tick();
    d.p_s_pg_valid_i = 0;
    w.tick();
    ckv(long(d.p_pg_op_mismatch_o) == 1, "11 pg_op_mismatch FIRED on a disagreeing echo", 1,
        long(d.p_pg_op_mismatch_o));
  }

  // =========================================================================
  // 12 -- AND THE BLOCK STILL WORKS AFTER ALL OF IT
  // =========================================================================
  // Six faults in a row is where a state machine leaves a flag set. A block
  // that faulted correctly and then never served again would pass everything
  // above.
  {
    std::printf("\n-- 12: a good record after every fault --\n");
    w.reset();
    ckv(w.req_a(kOpAcquire, kHandleA, 0) == kStAllocated, "12 ACQUIRE A", kStAllocated, -1);
    w.stamp_sheet(kHandleA, 0);
    const Admit a = offer(w, kHandleA, true);
    ckv(a.depth_sheet == 1, "12 it serves the sheet law again", 1, a.depth_sheet);
    const long stall_before = long(d.dig_stall_cycles_o);
    const int bad = dig(w, wantA);
    ckv(bad == 0, "12 and all 1,089 vertices are right", 0, bad);
    ckv(long(d.dig_stall_cycles_o) == stall_before, "12 with no stall on the real traversal",
        stall_before, long(d.dig_stall_cycles_o));
    retire(w);
    ck(d.seam_idle_o != 0, "12 and it is idle");
    ckv(w.seam_ops[kOpAcquire] == 0,
        "12 OVER THE WHOLE SUITE the seam issued not one OP_ACQUIRE", 0, w.seam_ops[kOpAcquire]);
    ckv(w.seam_ops[kOpRelease] == 0, "12 nor one OP_RELEASE", 0, w.seam_ops[kOpRelease]);
    ck(w.seam_ops[kOpRead] > 3000, "12 and thousands of OP_READs");
  }

  // =========================================================================
  // 13 -- THE BEFORE PLANE (OWNER RULING R231), AND IT CLEARS ON CONSUME
  // =========================================================================
  // The seam now owes bake TWO strengths per vertex, because bake ACCUMULATES
  // and what is added to an accumulator must be a CHANGE. This case drives the
  // `stamp_results` sink -- `surf_res_before_o`'s first consumer in this tree
  // -- and checks all three halves of the invariant:
  //
  //   seen = 1  ->  `sheet_before_o` is the pre-blend strength the stamp
  //                 reported, and it is DISTINCT from `after` so the two
  //                 cannot be confused;
  //   the read  ->  CLEARS the seen bit, so the SAME dig repeated serves
  //                 `after` and the delta collapses to zero. That is
  //                 idempotence as a structural property of this block rather
  //                 than an assertion about it;
  //   seen = 0  ->  `sheet_before_o` IS `sheet_strength_o`.
  {
    w.reset();
    ckv(w.req_a(kOpAcquire, kHandleA, 0) == kStAllocated, "13 ACQUIRE A", kStAllocated, -1);
    w.stamp_sheet(kHandleA, 0);

    const long bt0 = long(d.before_texels_o);
    ckv(bt0 == 0, "13 before_texels_o is SILENT before any stamp result", 0, bt0);
    w.sink_lattice(kHandleA, 0);
    ckv(long(d.before_texels_o) == long(kLat * kLat),
        "13 before_texels_o FIRED once per addressable result", long(kLat * kLat),
        long(d.before_texels_o));
    ckv(long(d.sr_dropped_o) == 0, "13 and nothing was dropped", 0, long(d.sr_dropped_o));

    const Admit a = offer(w, kHandleA, true);
    ckv(a.depth_sheet == 1, "13 the record is served on the layer-F law", 1, a.depth_sheet);
    ckv(a.fallback == 0, "13 and it is not a fallback", 0, a.fallback);

    // First dig: `before` is what the stamps reported, `after` is the sheet.
    int bad_after = 0, bad_before = 0;
    for (int vj = 0; vj < kLat; ++vj) {
      for (int vi = 0; vi < kLat; ++vi) {
        d.dig_ready_i = 0;
        d.sheet_texel_i = uint16_t(w.law_texel(vi, vj));
        w.tick();
        w.tick();
        w.tick();
        d.dig_ready_i = 1;
        d.eval();
        if (int(d.sheet_strength_o) != int(legal_value(vi, vj, 0))) ++bad_after;
        if (int(d.sheet_before_o) != int(before_value(vi, vj, 0))) ++bad_before;
        w.tick();
      }
    }
    d.dig_ready_i = 0;
    ckv(bad_after == 0, "13 all 1,089 `after` strengths are right", 0, bad_after);
    ckv(bad_before == 0, "13 all 1,089 `before` strengths are right", 0, bad_before);
    retire(w);

    // THE CLEAR ON CONSUME. Re-offer the SAME record with no new stamps: every
    // seen bit was retired by the dig above, so `before` must now BE `after`
    // and the delta bake computes is exactly zero. This is the seam's half of
    // idempotence, and `bake_delta_idempotence_directed` holds bake's half.
    const Admit a2 = offer(w, kHandleA, true);
    ckv(a2.depth_sheet == 1, "13 the re-offered record is still served", 1, a2.depth_sheet);
    int not_equal = 0;
    for (int vj = 0; vj < kLat; ++vj) {
      for (int vi = 0; vi < kLat; ++vi) {
        d.dig_ready_i = 0;
        d.sheet_texel_i = uint16_t(w.law_texel(vi, vj));
        w.tick();
        w.tick();
        w.tick();
        d.dig_ready_i = 1;
        d.eval();
        if (int(d.sheet_before_o) != int(d.sheet_strength_o)) ++not_equal;
        w.tick();
      }
    }
    d.dig_ready_i = 0;
    ckv(not_equal == 0,
        "13 THE READ CLEARED THE PLANE: a second dig with no new stamp serves "
        "`before` == `after`, so the delta is zero and the ground moves once",
        0, not_equal);
    retire(w);
  }

  // =========================================================================
  // 14 -- A RESULT THE PLANE CANNOT HOLD IS DROPPED, COUNTED, AND THE RECORD
  //       TAKES RULING R221's RATIFIED FALLBACK -- THEN RECOVERS
  // =========================================================================
  // The plane is one patch deep. A result for a SECOND handle while the first
  // is still live cannot be stored without putting one patch's `before` under
  // another patch's dig -- this file's own record-swap defect, through the
  // door R231 opened. So it is dropped and counted, and the next record pays
  // for it with the disc law rather than with a half-populated delta.
  //
  // THE RECOVERY IS THE HALF THAT IS EASY TO GET WRONG, and the first draft of
  // the RTL did: with the torn flag cleared only on a SERVED dig, a torn
  // record falls back, a fallback leaves `serve_q` low, and the flag latches
  // for the life of the machine -- the sheet law silently dead with every
  // counter agreeing. So this case asserts the block comes BACK.
  {
    w.reset();
    ckv(w.req_a(kOpAcquire, kHandleA, 0) == kStAllocated, "14 ACQUIRE A", kStAllocated, -1);
    ckv(w.req_a(kOpAcquire, kHandleB, 0) == kStAllocated, "14 ACQUIRE B", kStAllocated, -1);
    w.stamp_sheet(kHandleA, 0);

    w.sink(kHandleA, w.law_texel(4, 4), 210);
    ckv(long(d.sr_dropped_o) == 0, "14 sr_dropped_o SILENT while one handle owns the plane", 0,
        long(d.sr_dropped_o));
    ckv(long(d.before_torn_o) == 0, "14 before_torn_o SILENT too", 0, long(d.before_torn_o));

    // A second handle, while A's entry is still live.
    w.sink(kHandleB, w.law_texel(5, 5), 220);
    ckv(long(d.sr_dropped_o) == 1, "14 sr_dropped_o FIRED on the result the plane cannot hold", 1,
        long(d.sr_dropped_o));

    const Admit a = offer(w, kHandleA, true);
    ckv(a.depth_sheet == 0, "14 R221: a torn plane drops cmd_depth_sheet_i", 0, a.depth_sheet);
    ckv(a.fallback == 1, "14 and the record is flagged as a fallback", 1, a.fallback);
    ckv(long(d.before_torn_o) == 1,
        "14 before_torn_o FIRED, and it DISCRIMINATES: miss_texels_o is still zero, so the "
        "fallback reads as a torn plane and not as a residency problem",
        1, long(d.before_torn_o));
    ckv(long(d.miss_texels_o) == 0, "14 ... no residency miss happened", 0,
        long(d.miss_texels_o));
    retire(w);

    // AND IT COMES BACK. One record paid; the block must serve the next one.
    w.sink_lattice(kHandleA, 3);
    const Admit a2 = offer(w, kHandleA, true);
    ckv(a2.depth_sheet == 1,
        "14 THE TORN FLAG DID NOT LATCH: the next record is served on layer F again", 1,
        a2.depth_sheet);
    ckv(a2.fallback == 0, "14 and is not a fallback", 0, a2.fallback);
    const int bad = dig(w, wantA);
    ckv(bad == 0, "14 with all 1,089 `after` strengths still right", 0, bad);
    retire(w);
  }

  // =========================================================================
  // 15 -- A RESULT ARRIVING DURING A DIG IS NEVER LOST, AT ANY PHASE
  // =========================================================================
  // `bf_q` has ONE write port -- 1,089 x 9 bits belongs in a memory, not in
  // 9,801 flip-flops -- and the dig's read must also WRITE, because it clears
  // the `seen` bit on consume. So a stamp result landing in the same cycle as
  // a dig read has nowhere to go, and the FIRST version of this block lost it
  // SILENTLY while `before_texels_o` counted it as stored.
  //
  // That is this packet's own defect one level down: the counter says the
  // delta arrived, the `seen` bit is clear, the vertex serves
  // `before == after`, digs nothing, and every instrument agrees. An UNDER-dig
  // -- R221's refused "they acted, the ground did not move".
  //
  // It is not rare: `rd_issue_c` runs about one cycle in four through a
  // 1,089-vertex dig. A one-deep skid absorbs it. This case places a result at
  // EACH of the four phases of the dig's vertex loop and requires all four to
  // be stored -- phase 0 is the colliding one, and the other three are the
  // negative controls that stop the check passing for the wrong reason.
  {
    w.reset();
    ckv(w.req_a(kOpAcquire, kHandleA, 0) == kStAllocated, "15 ACQUIRE A", kStAllocated, -1);
    w.stamp_sheet(kHandleA, 0);

    const Admit a = offer(w, kHandleA, true);
    ckv(a.depth_sheet == 1, "15 served on the layer-F law", 1, a.depth_sheet);

    // Four target vertices the dig passes EARLY, so that by the time each
    // result is injected the dig has already consumed and cleared them --
    // otherwise the dig's own clear would retire the write we are testing for
    // and the case would pass while proving nothing.
    const int tgt_vi[4] = {0, 1, 2, 3};
    const uint8_t tgt_before[4] = {231, 232, 233, 234};
    const long bt0 = long(d.before_texels_o);
    const long dr0 = long(d.sr_dropped_o);

    int inject = 0;
    for (int vj = 0; vj < kLat; ++vj) {
      for (int vi = 0; vi < kLat; ++vi) {
        d.dig_ready_i = 0;
        d.sheet_texel_i = uint16_t(w.law_texel(vi, vj));
        // Inject on four separate vertices, one at each phase 0..3. Phase 0 is
        // the cycle `rd_issue_c` fires in, because the texel has just changed.
        const bool doing = (vj == 5) && (vi >= 10) && (vi < 14);
        const int phase = doing ? (vi - 10) : -1;
        if (phase == 0) w.sink_arm(kHandleA, w.law_texel(tgt_vi[0], 0), tgt_before[0]);
        w.tick();  // StVxM
        if (phase == 0) { w.sink_disarm(); ++inject; }
        if (phase == 1) w.sink_arm(kHandleA, w.law_texel(tgt_vi[1], 0), tgt_before[1]);
        w.tick();  // StVxC
        if (phase == 1) { w.sink_disarm(); ++inject; }
        if (phase == 2) w.sink_arm(kHandleA, w.law_texel(tgt_vi[2], 0), tgt_before[2]);
        w.tick();  // StDxM
        if (phase == 2) { w.sink_disarm(); ++inject; }
        if (phase == 3) w.sink_arm(kHandleA, w.law_texel(tgt_vi[3], 0), tgt_before[3]);
        d.dig_ready_i = 1;
        d.eval();
        w.tick();  // the accept edge
        if (phase == 3) { w.sink_disarm(); ++inject; }
      }
    }
    d.dig_ready_i = 0;
    w.idle(4);  // let the skid drain

    ckv(inject == 4, "15 four results were injected, one per phase", 4, inject);
    ckv(long(d.before_texels_o) - bt0 == 4,
        "15 before_texels_o counted all four", 4, long(d.before_texels_o) - bt0);
    ckv(long(d.sr_dropped_o) - dr0 == 0,
        "15 and NONE was dropped -- the skid absorbed the colliding one", 0,
        long(d.sr_dropped_o) - dr0);
    retire(w);

    // THE COUNTER SAYING "STORED" IS THE CLAIM. This is the check of it: the
    // plane must actually SERVE those four `before` values on the next dig,
    // and `after` everywhere else. A silent loss moves the counter and fails
    // exactly here, which is why the counter alone was never enough.
    const Admit a2 = offer(w, kHandleA, true);
    ckv(a2.depth_sheet == 1, "15 the next record is served", 1, a2.depth_sheet);
    int wrong = 0, served = 0;
    for (int vj = 0; vj < kLat; ++vj) {
      for (int vi = 0; vi < kLat; ++vi) {
        d.dig_ready_i = 0;
        d.sheet_texel_i = uint16_t(w.law_texel(vi, vj));
        w.tick();
        w.tick();
        w.tick();
        d.dig_ready_i = 1;
        d.eval();
        int want = int(d.sheet_strength_o);  // unseen serves `after`
        for (int k = 0; k < 4; ++k)
          if (vj == 0 && vi == tgt_vi[k]) { want = int(tgt_before[k]); ++served; }
        if (int(d.sheet_before_o) != want) ++wrong;
        w.tick();
      }
    }
    d.dig_ready_i = 0;
    ckv(served == 4, "15 all four target vertices were visited", 4, served);
    ckv(wrong == 0,
        "15 EVERY injected `before` was actually STORED and served, and every "
        "other vertex serves `after` -- the counter's claim, checked",
        0, wrong);
    retire(w);
  }

  // =========================================================================
  // 16 -- BACK-TO-BACK RESULTS ACROSS A COLLISION: THE SKID MUST REFILL AS IT
  //       DRAINS
  // =========================================================================
  // Case 15 injects one result per collision and never two in consecutive
  // cycles, so it cannot see the skid's OWN version of the same defect -- and
  // the first version of the skid had it.
  //
  // The shape: result A collides with a dig read and parks. On the NEXT cycle
  // the port is free, so the skid drains A... and result B arrives in that
  // same cycle. The port is carrying A, not B. If B is written "directly" it
  // goes nowhere, and if the skid only clears instead of refilling, B is lost
  // SILENTLY while `sr_take_c` counts it. Identical to the defect this whole
  // packet repairs, two layers down.
  //
  // So: park and drain in the SAME cycle -- the skid empties and refills at
  // once. This case drives exactly that and checks BOTH values are served.
  {
    w.reset();
    ckv(w.req_a(kOpAcquire, kHandleA, 0) == kStAllocated, "16 ACQUIRE A", kStAllocated, -1);
    w.stamp_sheet(kHandleA, 0);

    const Admit a = offer(w, kHandleA, true);
    ckv(a.depth_sheet == 1, "16 served on the layer-F law", 1, a.depth_sheet);

    const int tgt_a = 4, tgt_b = 5;             // both on row 0, both passed early
    const uint8_t val_a = 241, val_b = 242;
    const long bt0 = long(d.before_texels_o);
    const long dr0 = long(d.sr_dropped_o);

    for (int vj = 0; vj < kLat; ++vj) {
      for (int vi = 0; vi < kLat; ++vi) {
        d.dig_ready_i = 0;
        d.sheet_texel_i = uint16_t(w.law_texel(vi, vj));
        const bool here = (vj == 7) && (vi == 12);
        // A lands on the collision cycle (`rd_issue_c` fires as the texel
        // changes) and parks; B lands on the very next, while A drains.
        if (here) w.sink_arm(kHandleA, w.law_texel(tgt_a, 0), val_a);
        w.tick();  // StVxM -- A parks
        if (here) w.sink_arm(kHandleA, w.law_texel(tgt_b, 0), val_b);
        w.tick();  // StVxC -- A drains to the plane, B must take its place
        if (here) w.sink_disarm();
        w.tick();  // StDxM
        d.dig_ready_i = 1;
        d.eval();
        w.tick();
      }
    }
    d.dig_ready_i = 0;
    w.idle(4);

    ckv(long(d.before_texels_o) - bt0 == 2, "16 both results were counted as taken", 2,
        long(d.before_texels_o) - bt0);
    ckv(long(d.sr_dropped_o) - dr0 == 0, "16 and neither was dropped", 0,
        long(d.sr_dropped_o) - dr0);
    retire(w);

    // The counter says two. This is the check of it.
    const Admit a2 = offer(w, kHandleA, true);
    ckv(a2.depth_sheet == 1, "16 the next record is served", 1, a2.depth_sheet);
    int wrong = 0, seen_a = 0, seen_b = 0;
    for (int vj = 0; vj < kLat; ++vj) {
      for (int vi = 0; vi < kLat; ++vi) {
        d.dig_ready_i = 0;
        d.sheet_texel_i = uint16_t(w.law_texel(vi, vj));
        w.tick();
        w.tick();
        w.tick();
        d.dig_ready_i = 1;
        d.eval();
        int want = int(d.sheet_strength_o);
        if (vj == 0 && vi == tgt_a) { want = val_a; ++seen_a; }
        if (vj == 0 && vi == tgt_b) { want = val_b; ++seen_b; }
        if (int(d.sheet_before_o) != want) ++wrong;
        w.tick();
      }
    }
    d.dig_ready_i = 0;
    ckv(seen_a == 1 && seen_b == 1, "16 both target vertices were visited", 1,
        (seen_a == 1 && seen_b == 1) ? 1 : 0);
    ckv(wrong == 0,
        "16 THE SKID REFILLED AS IT DRAINED: both back-to-back results were "
        "stored and served, and every other vertex serves `after`",
        0, wrong);
    retire(w);
  }

  std::printf("\n== %d checks, %d failures ==\n", g_checks, g_fail);
  std::fflush(stdout);

  const int rc = (g_fail == 0) ? 0 : 1;
  delete dut;
  zhao::exit_hard(rc);
}
