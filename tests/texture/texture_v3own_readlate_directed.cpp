// texture_v3own_readlate_directed.cpp -- v3own with READ_LATE=1: the seam's
// data path, its boundary assertions, and the POSITIVE CONTROL for
// `ev_src_unpub_o`.
//
// reports/TEXTURE-READLATE-COMBINE-20260910.md (roadmap 4.2 / Commit4).
//
// WHY THIS TEST EXISTS BESIDE THE 541-CHECK ADVERSARIAL SUITE. That suite
// elaborates READ_LATE=0 -- deliberately, so the seam does not disturb what it
// proves -- and so it never sees the read-late port. This one elaborates
// READ_LATE=1 and does three things the suite cannot:
//
//   1. THE DATA PATH. A TMU return committed to sample plane 0 is read back
//      through `src_rd_slot_i` -> `src_s0_o` one edge later, bit-exact, while
//      the legacy `cmb_s0_o` lane reads zero (tied off). That is the seam.
//
//   2. THE COUNTER IS SEEN TO FIRE, on every class it names, by STIMULUS at
//      this block's boundary -- the combiner is outside, so an illegal read is
//      legal input here. This is the "three could be fired with stimulus
//      alone" case of CLAUDE.md's committed-mutant law: no mutant needed for
//      the instrument at THIS boundary. (The composed island needs the
//      combiner mutant, because there the reads come from the combiner.)
//        a. a slot that is not live;
//        b. a live owner that COMBINE has not yet accepted;
//        c. an owner whose FINAL has already been claimed;
//        d. an owner that has been released at the output.
//      The simulation assertions fire on the same events -- independent
//      corroboration -- and are made non-fatal for the duration so the
//      synthesizable counter can be read; the counter is the thing that ships.
//
//   3. THE BLIND SPOT, PINNED. A read of a live, accepted, unfinished owner
//      that is simply THE WRONG ONE moves the counter by exactly zero. The
//      metadata-bank law: a checker that inspects fields the fault leaves
//      intact cannot fire. This is asserted as a known property, not hidden --
//      so that if someone later widens the counter to catch it, this line
//      says the map changed.
//
// Ports are v3own's (SLOTW=6, GENW=8: owner = {slot[5:0], gen[7:0]}, sample
// handle = {slot[5:0], sidx[1:0], gen[7:0]}, result40 = {status8, a8, rgb24}).

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_texture_v3own_rl.h"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

using Dut = Vzhao_texture_v3own_rl;

void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
}

void idle_inputs(Dut& d) {
  d.adm_valid_i = 0;
  d.iss_tmu_valid_i = 0;
  d.iss_aux_valid_i = 0;
  d.tmu_rvalid_i = 0;
  d.aux_rvalid_i = 0;
  d.cmb_ready_i = 0;
  d.src_rd_valid_i = 0;
  d.src_rd_slot_i = 0;
  d.fin_valid_i = 0;
  d.out_ready_i = 0;
}

struct Owner {
  uint16_t handle = 0;
  uint8_t slot = 0;
  uint8_t gen = 0;
};

// Admit one owner with the given required mask; returns its handle.
Owner admit(Dut& d, uint8_t req, uint64_t ctx) {
  Owner o;
  d.adm_valid_i = 1;
  d.adm_req_i = req;
  d.adm_ctx_i = ctx;
  for (int i = 0; i < 16; ++i) {
    d.eval();
    if (d.adm_valid_i && d.adm_ready_o) {
      o.handle = static_cast<uint16_t>(d.adm_owner_o);
      check(d.adm_accept_o != 0, "adm_accept_o rises with the handshake", 1, d.adm_accept_o);
      tick(d);
      break;
    }
    tick(d);
  }
  d.adm_valid_i = 0;
  o.slot = static_cast<uint8_t>(o.handle >> 8);
  o.gen = static_cast<uint8_t>(o.handle & 0xFF);
  return o;
}

uint16_t sample_handle(const Owner& o, unsigned sidx) {
  return static_cast<uint16_t>((o.slot << 10) | ((sidx & 3) << 8) | o.gen);
}

// Issue and return sample 0 for the owner with a given result40.
void return_s0(Dut& d, const Owner& o, uint64_t result40) {
  d.iss_tmu_valid_i = 1;
  d.iss_tmu_handle_i = sample_handle(o, 0);
  tick(d);
  d.iss_tmu_valid_i = 0;
  d.tmu_rvalid_i = 1;
  d.tmu_rhandle_i = sample_handle(o, 0);
  d.tmu_rresult_i = result40;
  tick(d);
  d.tmu_rvalid_i = 0;
}

// Wait for the job to appear, then accept it (event 3: actual acceptance).
bool accept_job(Dut& d, const Owner& o) {
  for (int i = 0; i < 40; ++i) {
    d.eval();
    if (d.cmb_valid_o) {
      check(static_cast<uint16_t>(d.cmb_owner_o) == o.handle,
            "the job at cmb_owner_o is the owner just published", o.handle, d.cmb_owner_o);
      check(d.cmb_s0_o == 0 && d.cmb_s1_o == 0 && d.cmb_s2_o == 0 && d.cmb_aux_o == 0,
            "the legacy payload lanes read ZERO under READ_LATE=1 -- the ticket is the "
            "handle alone",
            0, static_cast<long long>(d.cmb_s0_o | d.cmb_s1_o | d.cmb_s2_o | d.cmb_aux_o));
      d.cmb_ready_i = 1;
      tick(d);
      d.cmb_ready_i = 0;
      return true;
    }
    tick(d);
  }
  return false;
}

// One plane read: present the slot with valid for a cycle, return s0 next edge.
uint64_t read_slot(Dut& d, uint8_t slot) {
  d.src_rd_valid_i = 1;
  d.src_rd_slot_i = slot;
  d.eval();
  tick(d);
  d.src_rd_valid_i = 0;
  d.eval();
  return d.src_s0_o;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  VerilatedContext* ctx = Verilated::threadContextp();

  Dut d;
  idle_inputs(d);
  d.rst_n = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  d.rst_n = 1;
  for (int i = 0; i < 4; ++i) tick(d);

  // ---- 1. THE DATA PATH ----------------------------------------------------
  const uint64_t kCtxA = 0xA5A5DEADBEEF0001ull;
  const uint64_t kResA = 0x12345678A9ull;   // 40 bits: status 0x12, a 0x34, rgb 0x5678A9
  Owner A = admit(d, 0x1, kCtxA);
  check(A.handle != 0 || A.gen != 0, "owner A admitted", 1, 1);
  return_s0(d, A, kResA);
  check(accept_job(d, A), "A's ticket reached the job queue and was accepted", 1, 1);
  check(d.ev_tickets_o == 1, "exactly one ticket was created for A", 1, d.ev_tickets_o);
  check(d.ev_commits_o == 1, "exactly one publication for A", 1, d.ev_commits_o);

  const uint64_t got_s0 = read_slot(d, A.slot);
  check(got_s0 == kResA,
        "the plane read at A's slot returns the committed result40 one edge later, "
        "bit-exact (the seam's data path)",
        static_cast<long long>(kResA), static_cast<long long>(got_s0));
  check(d.ev_src_unpub_o == 0, "a legal read (live, accepted, unfinished) moves the counter "
                                "by ZERO",
        0, d.ev_src_unpub_o);

  // ---- 2. POSITIVE CONTROLS: the counter is seen to fire -------------------
  // The boundary assertions fire on the same events; make them non-fatal for
  // the injections so the counter can be read, and require that they DID fire.
  ctx->fatalOnError(false);
  const bool err_before = ctx->gotError();
  check(!err_before, "no assertion had fired before the injections", 0, err_before ? 1 : 0);

  // a. a slot that is not live
  uint32_t c0 = d.ev_src_unpub_o;
  (void)read_slot(d, static_cast<uint8_t>((A.slot + 7) & 63));
  check(d.ev_src_unpub_o == c0 + 1, "(a) a read of a slot that is NOT LIVE fires the counter",
        c0 + 1, d.ev_src_unpub_o);

  // b. a live owner COMBINE has not accepted
  Owner B = admit(d, 0x1, 0xB0B0B0B000000002ull);
  c0 = d.ev_src_unpub_o;
  (void)read_slot(d, B.slot);
  check(d.ev_src_unpub_o == c0 + 1,
        "(b) a read of a live owner COMBINE has NOT ACCEPTED fires the counter", c0 + 1,
        d.ev_src_unpub_o);

  // c. an owner whose FINAL is claimed: send A's final, wait past C2 (fcl set),
  //    read A before its output is released.
  d.fin_valid_i = 1;
  d.fin_owner_i = A.handle;
  d.fin_result_i = 0x000000CAFEull;
  tick(d);
  d.fin_valid_i = 0;
  for (int i = 0; i < 6; ++i) tick(d);   // C0, C1, C2 (claim), C3 (write), C4 (publish)
  c0 = d.ev_src_unpub_o;
  (void)read_slot(d, A.slot);
  check(d.ev_src_unpub_o == c0 + 1,
        "(c) a read of an owner whose FINAL IS CLAIMED fires the counter -- the last "
        "reader must precede the final",
        c0 + 1, d.ev_src_unpub_o);

  // d. release A at the output, then read it.
  d.out_ready_i = 1;
  bool emitted = false;
  for (int i = 0; i < 40 && !emitted; ++i) {
    d.eval();
    if (d.out_valid_o) {
      check(static_cast<uint16_t>(d.out_owner_o) == A.handle, "A is the owner emitted first",
            A.handle, d.out_owner_o);
      check((d.out_result_o & 0xFFFFFFFFull) == 0xCAFEull, "A's final result is emitted",
            0xCAFE, static_cast<long long>(d.out_result_o & 0xFFFFFFFFull));
      check(d.out_ctx_o == kCtxA, "A's 64-bit context is emitted untouched", 1,
            d.out_ctx_o == kCtxA ? 1 : 0);
      emitted = true;
    }
    tick(d);
  }
  d.out_ready_i = 0;
  check(emitted, "A was emitted", 1, emitted ? 1 : 0);
  c0 = d.ev_src_unpub_o;
  (void)read_slot(d, A.slot);
  check(d.ev_src_unpub_o == c0 + 1,
        "(d) a read of an owner RELEASED at the output fires the counter", c0 + 1,
        d.ev_src_unpub_o);

  const bool err_after = ctx->gotError();
  check(err_after,
        "the boundary assertions fired on the injections too (independent corroboration "
        "of the counter)",
        1, err_after ? 1 : 0);
  ctx->gotError(false);
  ctx->gotFinish(false);

  // ---- 3. THE BLIND SPOT, PINNED -------------------------------------------
  // Complete B; admit and complete C. Read B's slot while "meaning" C: every
  // bit the counter inspects says B is a legal read target, so it cannot move.
  return_s0(d, B, 0x0000BBBBBBull);
  check(accept_job(d, B), "B accepted", 1, 1);
  Owner C = admit(d, 0x1, 0xC0C0C0C000000003ull);
  return_s0(d, C, 0x0000CCCCCCull);
  check(accept_job(d, C), "C accepted", 1, 1);
  c0 = d.ev_src_unpub_o;
  const uint64_t wrong = read_slot(d, B.slot);   // the combiner meant C
  check(wrong == 0x0000BBBBBBull, "the wrong-owner read returns B's data, plausibly", 1,
        wrong == 0x0000BBBBBBull ? 1 : 0);
  check(d.ev_src_unpub_o == c0,
        "BLIND SPOT PINNED: a read of the WRONG but live, accepted, unfinished owner "
        "moves the counter by exactly ZERO -- only the differential sees this class",
        c0, d.ev_src_unpub_o);
  check(!ctx->gotError(), "and no boundary assertion sees it either", 0,
        ctx->gotError() ? 1 : 0);
  ctx->fatalOnError(true);

  std::printf("  ev_src_unpub_o = %u after four injections; tickets %u commits %u\n",
              d.ev_src_unpub_o, d.ev_tickets_o, d.ev_commits_o);

  if (g_failed) {
    std::printf("[texture_v3own_readlate_directed] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[texture_v3own_readlate_directed] %d checks passed\n", g_checks);
  return 0;
}
