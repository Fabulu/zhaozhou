// terrainaux_acceptance.cpp -- THE SURFACE SHEET'S LOOP, CLOSED, AND A PIXEL
// THAT MOVES BECAUSE OF IT.
//
// Bench: tests/prod/tb_terrainaux_acceptance.sv (four PRODUCTION modules).
// Packet: TERRAINAUX, 2026-09-25, owner vacation directive 2026-09-23 section 6.
//
// ---------------------------------------------------------------------------
// THE CLAIM, AND WHY IT NEEDED FOUR REAL BLOCKS RATHER THAN A COUNTER
// ---------------------------------------------------------------------------
// The console could WRITE layer F and could not SEE it. Three separate, local,
// reasonable-looking refusals stood between the stamp and the pixel:
//
//   1. `zhao_shell_top_v2` tied `pg_*` to zero -- "no producer exists in this
//      shell yet" -- while the REQUEST half left the shell, the core AND the
//      board as a dangling top-level output group, and `zhao_surface_sheet`
//      sat composed inside `zhao_console_core` the whole time;
//   2. `zhao_console_core`'s flat request pinned `aux_required` to zero,
//      correctly, BECAUSE of (1): a fragment that asked would never retire;
//   3. both texture contracts said "tag and strength are reserved for a later
//      visible terrain-effect composition and Packet B does not claim that
//      effect is connected".
//
// So the measurement that settles it is not "a port is driven" and not "a
// counter moved". It is: STAMP A SCAR, SAMPLE IT THROUGH THE REAL ARBITER AND
// THE REAL STORE, AND SHOW THE COLOUR DIFFERENT.
//
// SECTIONS
//   1. THE LOOP EXISTS       a READ issued by the real AUX pipe reaches the
//                            real store through the real share and comes back
//                            HIT with the byte that was stamped.
//   2. THE PIXEL MOVES       the same colour, the same material declaration,
//                            two different scars -> two different RGB values,
//                            each equal to the oracle's section 12 law.
//   3. THE IDENTITY ARM      a material that declares no AUX is bit-identical
//                            to the pre-2026-09-25 console. This is the
//                            compatibility claim and it is asserted, not said.
//   4. THE MISS IS ZERO      an unstamped texel reads 0 and tints by the 0.4%
//                            the oracle's own comment names; a NON-RESIDENT
//                            handle is ST_MISS, strength 0, and `sheet_misses_o`
//                            moves -- zref_aux.hpp choice A2, "zero, never
//                            stale".
//   5. THE ADDRESS IS REAL   `zref::aux::axis_texel`'s floor-across-envelope
//                            mapping, differenced texel by texel over a sweep.
//                            A u/v transposition dies here.
//   6. THE SHARE ARBITRATES  client A runs SURFACE.STAMP-shaped traffic while
//                            client C reads. Both counters move, neither
//                            starves, and `pg_orphan_o`/`pg_op_mismatch_o`
//                            stay zero -- which is a claim about THREE
//                            independent quantities, not one.
//   7. THE DEGENERATE ARM    an inverted envelope returns 0 and reads nothing.
//
// POSITIVE CONTROL: --break-oracle corrupts ONE expected value; the suite must
// then FAIL.
#include "Vtb_terrainaux_acceptance.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "zref/zref_aux.hpp"

#include "../harness/zhao_sim.hpp"

namespace {

using Dut = Vtb_terrainaux_acceptance;

bool g_break_oracle = false;
int g_checks = 0;
int g_fails = 0;

void fail(const char* what, long got, long want) {
  ++g_fails;
  std::printf("FAIL %s: got %ld expected %ld\n", what, got, want);
  if (g_fails > 20) {
    std::printf("TERRAINAUX: too many failures, stopping\n");
    zhao::exit_hard(1);
  }
}

void check_eq(const char* what, long got, long want) {
  ++g_checks;
  if (got != want) fail(what, got, want);
}

struct Bench {
  Dut* d = nullptr;
  uint64_t cyc = 0;

  void tick() {
    d->clk = 0;
    d->eval();
    d->clk = 1;
    d->eval();
    ++cyc;
  }

  void reset() {
    d->clk = 0;
    d->rst_n = 0;
    d->wr_valid_i = 0;
    d->wr_handle_i = 0;
    d->wr_texel_i = 0;
    d->wr_tag_i = 0;
    d->wr_strength_i = 0;
    d->wr_we_tag_i = 0;
    d->wr_we_strength_i = 0;
    d->a_req_valid_i = 0;
    d->a_req_op_i = 0;
    d->a_req_handle_i = 0;
    d->a_req_texel_i = 0;
    d->a_req_src_id_i = 0;
    d->a_pg_ready_i = 1;
    d->job_valid_i = 0;
    d->job_wx_i = 0;
    d->job_wz_i = 0;
    d->job_env_x0_i = 0;
    d->job_env_x1_i = 0;
    d->job_env_z0_i = 0;
    d->job_env_z1_i = 0;
    d->job_sheet_handle_i = 0;
    d->job_owner_i = 0;
    d->job_force_refuse_i = 0;
    d->out_ready_i = 1;
    d->sm_en_i = 0;
    d->sm_rgb_i = 0;
    d->eval();
    for (int i = 0; i < 8; ++i) tick();
    d->rst_n = 1;
    for (int i = 0; i < 4; ++i) tick();
  }

  // ---- SURFACE.STAMP's own two acts, through the real ports ---------------
  // ACQUIRE is issued on CLIENT A because it is the stamp's opcode and the AUX
  // pipe issues READ and nothing else. A bench that acquired on client C would
  // have measured an arrangement the console does not have.
  bool acquire(uint32_t handle) {
    d->a_req_valid_i = 1;
    d->a_req_op_i = 0;  // OpAcquire
    d->a_req_handle_i = handle;
    d->a_req_texel_i = 0;
    d->a_req_src_id_i = 0x1234;
    // THE BOUND IS 40,000 AND IT IS NOT GENEROSITY. `zhao_surface_sheet`'s
    // ST_ALLOCATED arm hands back a "fresh slot, CLEARED TO ZERO", and the
    // clear is a real sweep over 4,096 texels -- `res_busy_o` holds
    // `req_ready_o` low while it runs. A 64-cycle wait here reported "SURFACE
    // .SHEET never acquired the fixture handle", which reads exactly like a
    // dead handshake and was a bench that did not know what it had asked for.
    bool took = false;
    for (int i = 0; i < 40000 && !took; ++i) {
      d->eval();
      took = d->a_req_valid_i && d->a_req_ready_o;
      tick();
    }
    d->a_req_valid_i = 0;
    if (!took) return false;
    for (int i = 0; i < 40000; ++i) {
      d->eval();
      if (d->a_pg_valid_o) {
        tick();
        // and let the clear sweep finish before anything reads the slot
        for (int k = 0; k < 8192; ++k) tick();
        return true;
      }
      tick();
    }
    return false;
  }

  void stamp(uint32_t handle, int u, int v, uint8_t tag, uint8_t strength) {
    d->wr_valid_i = 1;
    d->wr_handle_i = handle;
    d->wr_texel_i = static_cast<uint16_t>(v * 64 + u);
    d->wr_tag_i = tag;
    d->wr_strength_i = strength;
    d->wr_we_tag_i = 1;
    d->wr_we_strength_i = 1;
    for (int i = 0; i < 40000; ++i) {
      d->eval();
      const bool took = d->wr_valid_i && d->wr_ready_o;
      tick();
      if (took) break;
    }
    d->wr_valid_i = 0;
    d->eval();
  }

  // ---- one AUX job, start to typed result ---------------------------------
  struct AuxOut {
    bool got = false;
    uint8_t status = 0;
    uint8_t tag = 0;
    uint8_t strength = 0;
    uint16_t owner = 0;
  };

  AuxOut aux(const zref::aux::Envelope& e, int32_t wx, int32_t wz, uint32_t handle,
             uint16_t owner, bool force_refuse = false) {
    AuxOut r;
    d->job_valid_i = 1;
    d->job_wx_i = wx;
    d->job_wz_i = wz;
    d->job_env_x0_i = e.x0;
    d->job_env_x1_i = e.x1;
    d->job_env_z0_i = e.z0;
    d->job_env_z1_i = e.z1;
    d->job_sheet_handle_i = handle;
    d->job_owner_i = owner;
    d->job_force_refuse_i = force_refuse ? 1 : 0;
    d->out_ready_i = 1;
    // THE ORDER HERE IS THE WHOLE HANDSHAKE, and getting it wrong cost a run:
    // `job_valid_i` must be LOWERED AFTER the clock edge that accepted it, not
    // before. The first version cleared it on the same eval that saw
    // `job_ready_o`, so valid was already low at the rising edge and the job
    // was never latched -- `client C got a grant: 0`, which reads like a dead
    // arbiter and was a dead driver.
    for (int i = 0; i < 20000; ++i) {
      d->eval();
      const bool accepted = d->job_valid_i && d->job_ready_o;
      const bool retired = d->out_valid_o && d->out_ready_i;
      if (retired) {
        r.got = true;
        r.owner = static_cast<uint16_t>(d->out_owner_o);
        const uint64_t res = d->out_result_o;
        r.status = static_cast<uint8_t>((res >> 40) & 0xFF);
        r.tag = static_cast<uint8_t>((res >> 32) & 0xFF);
        r.strength = static_cast<uint8_t>((res >> 24) & 0xFF);
      }
      tick();
      if (accepted) d->job_valid_i = 0;
      if (retired) break;
    }
    d->job_valid_i = 0;
    d->eval();
    return r;
  }

  // ---- the sheetmod, on the plane the store just answered with ------------
  uint32_t shade(bool en, uint32_t rgb) {
    d->sm_en_i = en ? 1 : 0;
    d->sm_rgb_i = rgb;
    d->eval();
    return d->sm_rgb_o & 0xFFFFFFu;
  }
};

// The section 12 law, transcribed from reference/src/zrender/terrain.cpp:718
// and :756 -- NOT from the RTL and not from the sheetmod's own directed test.
uint8_t tinted(uint8_t v, uint8_t strength) {
  const int32_t t = 255 - (strength >> 1);
  return static_cast<uint8_t>((static_cast<int32_t>(v) * t + 128) >> 8);
}
uint32_t tinted_rgb(uint32_t rgb, uint8_t strength) {
  return (static_cast<uint32_t>(tinted(static_cast<uint8_t>((rgb >> 16) & 0xFF), strength)) << 16) |
         (static_cast<uint32_t>(tinted(static_cast<uint8_t>((rgb >> 8) & 0xFF), strength)) << 8) |
         tinted(static_cast<uint8_t>(rgb & 0xFF), strength);
}

constexpr uint32_t kHandle = 0x00AB0001u;   // {index 0xAB, generation 1}
constexpr uint32_t kOtherHandle = 0x00CD0002u;

}  // namespace

double sc_time_stamp() { return 0.0; }

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--break-oracle") == 0) g_break_oracle = true;

  Bench b;
  b.d = new Dut;
  b.reset();

  zref::aux::Envelope env;
  env.x0 = 0;
  env.z0 = 0;
  env.x1 = 64 << 16;  // 64 metres across, so one texel is one metre
  env.z1 = 64 << 16;

  // =========================================================================
  // 1. THE LOOP EXISTS
  // =========================================================================
  if (!b.acquire(kHandle)) {
    std::printf("TERRAINAUX: SURFACE.SHEET never acquired the fixture handle\n");
    zhao::exit_hard(1);
  }
  check_eq("1 slot occupied after acquire", b.d->sheet_res_occupancy_o != 0, 1);

  // A scar at texel (17, 29), strength 200, tag 7.
  b.stamp(kHandle, 17, 29, 7, 200);

  const int32_t wx = (17 << 16) + (1 << 15);  // the centre of texel 17
  const int32_t wz = (29 << 16) + (1 << 15);
  const int u_ref = zref::aux::axis_texel(wx, env.x0, env.x1);
  const int v_ref = zref::aux::axis_texel(wz, env.z0, env.z1);
  check_eq("1 oracle texel u", u_ref, 17);
  check_eq("1 oracle texel v", v_ref, 29);

  Bench::AuxOut r = b.aux(env, wx, wz, kHandle, 0x0101);
  check_eq("1 a typed AUX result came back", r.got, 1);
  check_eq("1 owner echo", r.owner, 0x0101);
  check_eq("1 status is clean", r.status, 0);
  check_eq("1 strength is the stamped byte", r.strength, 200);
  check_eq("1 tag is the stamped byte", r.tag, 7);
  check_eq("1 the store was actually read", b.d->aux_sheet_reads_o >= 1, 1);
  check_eq("1 it was a HIT", b.d->aux_hits_o, 1);
  check_eq("1 no miss", b.d->aux_misses_o, 0);
  check_eq("1 client C got a grant", b.d->share_c_reqs_o >= 1, 1);
  check_eq("1 no orphan response", b.d->share_pg_orphan_o, 0);
  check_eq("1 no opcode mismatch", b.d->share_pg_op_mismatch_o, 0);
  check_eq("1 no wrong-src response", b.d->aux_wrong_src_o, 0);
  check_eq("1 no wrong-op response", b.d->aux_wrong_op_o, 0);
  check_eq("1 no unsolicited response", b.d->aux_unsolicited_o, 0);
  check_eq("1 no credit fault", b.d->aux_credit_fault_o, 0);
  check_eq("1 no frame fault", b.d->aux_frame_fault_o, 0);
  std::printf("TERRAINAUX: 1. the loop is closed -- AUX read -> share -> store -> HIT strength=%u tag=%u\n",
              r.strength, r.tag);

  // =========================================================================
  // 2. THE PIXEL MOVES
  // =========================================================================
  // Same colour, same material declaration, two different scars.
  const uint32_t kBase = 0x8040C0u;
  check_eq("2 the held strength is the returned strength", b.d->sm_strength_o, 200);
  const uint32_t heavy = b.shade(true, kBase);
  uint32_t want_heavy = tinted_rgb(kBase, 200);
  if (g_break_oracle) want_heavy ^= 1u;
  check_eq("2 heavy scar rgb", heavy, want_heavy);

  // A LIGHT scar at a different texel, sampled through the same loop.
  b.stamp(kHandle, 3, 5, 9, 40);
  const int32_t wx2 = (3 << 16) + (1 << 15);
  const int32_t wz2 = (5 << 16) + (1 << 15);
  Bench::AuxOut r2 = b.aux(env, wx2, wz2, kHandle, 0x0202);
  check_eq("2 second result came back", r2.got, 1);
  check_eq("2 second strength", r2.strength, 40);
  check_eq("2 the held strength followed the second read", b.d->sm_strength_o, 40);
  const uint32_t light = b.shade(true, kBase);
  check_eq("2 light scar rgb", light, tinted_rgb(kBase, 40));

  // THE TWO DIFFER. That sentence is the whole packet, so it is a check and
  // not a comment -- and it is a check the pre-2026-09-25 console could not
  // have passed, because the colour did not depend on the sheet at all.
  ++g_checks;
  if (heavy == light)
    fail("2 a heavy scar and a light scar produced the SAME colour", heavy, light);
  ++g_checks;
  if (!(heavy < light))
    fail("2 the heavier scar is not the darker colour", heavy, light);
  std::printf("TERRAINAUX: 2. PIXEL: base %06X -> %06X under strength 200, -> %06X under strength 40\n",
              kBase, heavy, light);

  // =========================================================================
  // 3. THE IDENTITY ARM -- the compatibility claim, asserted
  // =========================================================================
  // A material that declares no AUX is bit-identical to the console before this
  // packet. This is what makes every existing capture, golden and CRC safe, and
  // it is NOT the same statement as "strength 0 changes nothing" -- 255/256 is
  // not 1, which section 4 below measures.
  for (uint32_t k = 0; k < 256; ++k) {
    const uint32_t rgb = (k << 16) | ((255u - k) << 8) | ((k * 7u) & 0xFFu);
    check_eq("3 no-AUX passthrough", b.shade(false, rgb), rgb);
    check_eq("3 applied_o low", b.d->sm_applied_o, 0);
  }
  check_eq("3 no frame fault through sections 1-3", b.d->aux_frame_fault_o, 0);
  std::printf("TERRAINAUX: 3. a material without AUX is bit-identical over 256 colours\n");

  // =========================================================================
  // 4. AN UNSTAMPED TEXEL, AND A NON-RESIDENT HANDLE
  // =========================================================================
  // zref_aux.hpp choice A2: "A NON-RESIDENT SHEET READS AS ZERO and raises
  // miss, rather than stalling until residency."
  Bench::AuxOut r3 = b.aux(env, (40 << 16) + (1 << 15), (40 << 16) + (1 << 15),
                           kHandle, 0x0303);
  check_eq("4 unstamped result came back", r3.got, 1);
  check_eq("4 unstamped strength is zero", r3.strength, 0);
  const uint32_t unstamped = b.shade(true, kBase);
  check_eq("4 unstamped tint is the 0.4% arm", unstamped, tinted_rgb(kBase, 0));
  ++g_checks;
  if (unstamped == kBase)
    fail("4 a present sheet did not tint an unstamped texel -- the oracle says it must",
         unstamped, kBase);

  const uint32_t misses_before = b.d->aux_misses_o;
  Bench::AuxOut r4 = b.aux(env, wx, wz, kOtherHandle, 0x0404);
  check_eq("4 non-resident result came back", r4.got, 1);
  check_eq("4 non-resident strength is zero", r4.strength, 0);
  check_eq("4 sheet_misses_o moved", b.d->aux_misses_o, misses_before + 1);
  // AND THE MISS IS A FRAME FAULT. `zhao_texture_aux_pipe_v2` raises
  // `frame_fault_o` on `response_miss_c`, which is a CONSEQUENCE of closing
  // this loop worth stating out loud: a material that declares AUX against a
  // sheet handle that is not resident does not merely read zero -- it faults
  // the island's frame, and the island clears that through
  // `frame_fault_clear_i` per frame. This bench ties the clear LOW, so the
  // level stays up from here and the drain below asserts it UP, not down.
  check_eq("4 the miss raised the island frame fault", b.d->aux_frame_fault_o, 1);
  std::printf("TERRAINAUX: 4. unstamped reads 0 and still tints; a non-resident handle is a counted MISS\n");

  // =========================================================================
  // 5. THE ADDRESS IS REAL -- axis_texel, differenced
  // =========================================================================
  // 32 distinct texels, each stamped with a strength that IS its index, so a
  // u/v transposition or a dropped 64x returns the wrong number rather than a
  // plausible one. This is SURFACE.STAMP.md's own sentence about why the read
  // and write mappings must be checked together.
  for (int k = 0; k < 32; ++k) {
    const int uu = (k * 7) & 63;
    const int vv = (k * 13 + 5) & 63;
    b.stamp(kHandle, uu, vv, static_cast<uint8_t>(k), static_cast<uint8_t>(k * 8 + 1));
  }
  int swept = 0;
  for (int k = 0; k < 32; ++k) {
    const int uu = (k * 7) & 63;
    const int vv = (k * 13 + 5) & 63;
    const int32_t sx = (uu << 16) + (1 << 15);
    const int32_t sz = (vv << 16) + (1 << 15);
    check_eq("5 oracle u", zref::aux::axis_texel(sx, env.x0, env.x1), uu);
    check_eq("5 oracle v", zref::aux::axis_texel(sz, env.z0, env.z1), vv);
    Bench::AuxOut rk = b.aux(env, sx, sz, kHandle, static_cast<uint16_t>(0x0500 + k));
    check_eq("5 result came back", rk.got, 1);
    check_eq("5 strength at texel", rk.strength, static_cast<uint8_t>(k * 8 + 1));
    check_eq("5 tag at texel", rk.tag, static_cast<uint8_t>(k));
    check_eq("5 owner echo", rk.owner, 0x0500 + k);
    ++swept;
  }
  check_eq("5 swept count", swept, 32);
  std::printf("TERRAINAUX: 5. 32 distinct texels, each identified by its own byte\n");

  // =========================================================================
  // 6. THE SHARE ARBITRATES -- client A busy while client C reads
  // =========================================================================
  // A three-client arbiter measured with one client asking is a wire.
  const uint32_t a_before = b.d->share_a_reqs_o;
  const uint32_t c_before = b.d->share_c_reqs_o;
  b.d->a_req_valid_i = 1;
  b.d->a_req_op_i = 1;  // OpRead -- SURFACE.STAMP's read-modify-write half
  b.d->a_req_handle_i = kHandle;
  b.d->a_req_texel_i = 29 * 64 + 17;
  b.d->a_req_src_id_i = 0x7777;
  b.d->a_pg_ready_i = 1;
  // ... and eight AUX reads through client C at the same time.
  int c_done = 0;
  for (int k = 0; k < 8; ++k) {
    Bench::AuxOut rk = b.aux(env, wx, wz, kHandle, static_cast<uint16_t>(0x0600 + k));
    if (rk.got && rk.strength == 200) ++c_done;
  }
  b.d->a_req_valid_i = 0;
  for (int i = 0; i < 64; ++i) b.tick();
  check_eq("6 all eight AUX reads completed under contention", c_done, 8);
  ++g_checks;
  if (!(b.d->share_a_reqs_o > a_before))
    fail("6 client A was starved", b.d->share_a_reqs_o, a_before + 1);
  ++g_checks;
  if (!(b.d->share_c_reqs_o >= c_before + 8))
    fail("6 client C did not get its grants", b.d->share_c_reqs_o, c_before + 8);
  check_eq("6 still no orphan", b.d->share_pg_orphan_o, 0);
  check_eq("6 still no opcode mismatch", b.d->share_pg_op_mismatch_o, 0);
  check_eq("6 still no wrong-src", b.d->aux_wrong_src_o, 0);
  std::printf("TERRAINAUX: 6. under contention A=%u C=%u grants, orphan=0 op_mismatch=0 wrong_src=0\n",
              b.d->share_a_reqs_o, b.d->share_c_reqs_o);

  // =========================================================================
  // 7. THE DEGENERATE ARM -- sample_sheet's early return
  // =========================================================================
  zref::aux::Envelope bad;
  bad.x0 = 100 << 16;
  bad.x1 = 100 << 16;  // x1 <= x0
  bad.z0 = 0;
  bad.z1 = 64 << 16;
  ++g_checks;
  if (!bad.degenerate()) fail("7 the oracle does not call this envelope degenerate", 0, 1);
  const uint32_t deg_before = b.d->aux_degenerate_o;
  const uint32_t reads_before = b.d->aux_sheet_reads_o;
  Bench::AuxOut rd = b.aux(bad, wx, wz, kHandle, 0x0707);
  check_eq("7 degenerate result came back", rd.got, 1);
  check_eq("7 degenerate strength is zero", rd.strength, 0);
  check_eq("7 degenerate counted", b.d->aux_degenerate_o, deg_before + 1);
  check_eq("7 and NOTHING was read", b.d->aux_sheet_reads_o, reads_before);
  std::printf("TERRAINAUX: 7. a degenerate envelope returns 0 and reads nothing\n");

  // ---- the drain ----------------------------------------------------------
  for (int i = 0; i < 256; ++i) b.tick();
  check_eq("drain: aux idle", b.d->aux_idle_o, 1);
  check_eq("drain: no response owed", b.d->aux_sheet_rsp_owed_o, 0);
  check_eq("drain: share not busy", b.d->share_busy_o, 0);
  check_eq("drain: accepted == completed", b.d->aux_accepted_o, b.d->aux_completed_o);
  // THE FRAME FAULT IS EXPECTED UP, and asserting it up is the point: section
  // 4 raised it deliberately with a non-resident handle, `frame_fault_clear_i`
  // is tied low in this bench, and a zero here would mean the fault the block
  // says it raises does not survive to be read.
  check_eq("drain: the deliberate frame fault is still latched", b.d->aux_frame_fault_o, 1);
  check_eq("drain: client B never asked", b.d->share_b_reqs_o, 0);

  delete b.d;

  std::printf("TERRAINAUX: %d checks, %d failures%s\n", g_checks, g_fails,
              g_break_oracle ? "  (--break-oracle: a failure is the PASS)" : "");
  // THE POLARITY IS CTEST'S, NOT THIS FILE'S, and getting it backwards makes
  // the control read GREEN-WHEN-BROKEN. The registration marks the
  // --break-oracle run `WILL_FAIL TRUE`, so the control PASSES when this
  // process exits NON-ZERO. A corruption that was CAUGHT therefore exits
  // through the ordinary failure path below; the only special case is the
  // dangerous one -- a corruption that produced NO failure means the checker
  // is blind, and that must exit ZERO so the WILL_FAIL entry goes RED.
  if (g_break_oracle && (g_fails == 0)) {
    std::printf("TERRAINAUX: --break-oracle produced NO failure -- THE CHECKER IS BLIND\n");
    zhao::exit_hard(0);
  }
  zhao::exit_hard(g_fails == 0 ? 0 : 1);
}
