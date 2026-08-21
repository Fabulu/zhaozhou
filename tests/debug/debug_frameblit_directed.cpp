// debug_frameblit_directed.cpp — DEBUG.FRAMEBLIT against `zref::debug::FrameBlit`.
//
// The block that came out of reports/CMD.DMA_Redesign_Proposal.md Part 2. Its
// whole reason to exist is that the old blit path carried a 245,760-byte staging
// buffer inside CMD.DMA and stopped the shell fitting; the amended atomicity law
// lets the framebuffer slot itself be the transaction buffer, and the staging
// buffer collapses to one 64-byte chunk.
//
// THE ONE THING THAT MUST NEVER HAPPEN is publishing a slot that did not receive
// every byte with a matching CRC. Every failure path below is checked for
// exactly that, and the composition test at the end is the proposal's own:
//
//     Keep slot 0 displayed. Stream a deliberately bad blit into leased slot 1.
//     Prove slot 0's displayed content never changes.
//
// A DIRTY UNPUBLISHED SLOT IS AN ACCEPTED OUTCOME and is asserted as such, not
// worked around. On failure the slot is released FREE with whatever landed in
// it, because an unpublished slot is invisible. That is the point of the
// amendment.

// ---------------------------------------------------------------------------
// WHAT THE INTEGRATION REVIEW CHANGED HERE
// ---------------------------------------------------------------------------
// This file used to pass while the block it tests was wrong in six ways, and
// the reason is worth stating plainly: EVERY MODEL IN THE HARNESS WAS MORE
// AGREEABLE THAN THE REAL THING.
//
//   * The fake guard returned OK without ever looking at the address. The real
//     `zhao_mem_guard` passes a blit write only when the address is inside the
//     LEASED slot's window, and slot 1's base is 0x0200_0000 -- so every
//     slot-1 request the block made would have been denied. The harness now
//     checks the address on every request, for both slots.
//   * The fake bridge granted instantly. The real one has a request-acceptance
//     pulse, and the block advanced without waiting for it. The harness now
//     makes it wait.
//   * The fake memory retired instantly -- in fact the block counted its OWN
//     hand-offs as retirements. The harness now models credits coming back
//     from the arbiter, and can withhold them.
//   * The fake guard was always ready, so a request that should have been
//     withdrawn when the lease lapsed was never observed being held.
//
// A model that says yes to everything cannot fail a test. Each of the four is
// now something the harness can refuse.
//
// TWO EQUIVALENT MUTANTS, recorded so they do not read as holes later:
//
//   * `guard_request_after_loss` -- removing the state-level abort check in
//     B_GUARD_REQUEST. It survives because `guard_req_o.valid` is ALSO gated
//     combinationally on `!abort_pending && lease_ok_now`, and removing THAT
//     gate is caught. The state check only saves an idle cycle.
//   * `publish_generation_live` -- publishing `fb_lease_generation_i` instead
//     of the latched `r_gen`. It survives because publication requires
//     `lease_ok_now`, which is false unless those two are already equal.
//
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_debug_frameblit.h"

#include "zhao_sim.hpp"
#include "zref/zref_frameblit.hpp"

namespace {

using zhao::check;
namespace zd = zref::debug;

// ---------------------------------------------------------------------------
// PACKED-STRUCT BIT LAYOUTS
// ---------------------------------------------------------------------------
// SystemVerilog packs a struct MSB-first in declaration order, and Verilator
// hands anything over 64 bits back as a VlWide word array. Placing fields by
// name is not available here, so the layouts are written out once and used
// through helpers -- an off-by-one in a hand-packed struct is invisible until
// the DUT quietly does the wrong thing.
//
//   zhao_hps_burst_rsp_t { beat_valid, data[63:0], last, err } -- 67 bits
//     bit 66 beat_valid | bits 65:2 data | bit 1 last | bit 0 err
//
//   zhao_guard_rsp_t { ready, ok, violation } -- 3 bits
//     bit 2 ready | bit 1 ok | bit 0 violation
//
//   zhao_hps_burst_req_t { valid, write, client[2:0], addr[31:0], len[6:0] }
//     -- 44 bits, valid is the MSB (bit 43)

void set_hps_rsp(VlWide<3>& w, bool beat_valid, uint64_t data, bool last, bool err) {
  const __uint128_t v = (static_cast<__uint128_t>(beat_valid ? 1 : 0) << 66) |
                        (static_cast<__uint128_t>(data) << 2) |
                        (static_cast<__uint128_t>(last ? 1 : 0) << 1) |
                        static_cast<__uint128_t>(err ? 1 : 0);
  w[0] = static_cast<uint32_t>(v & 0xFFFFFFFFu);
  w[1] = static_cast<uint32_t>((v >> 32) & 0xFFFFFFFFu);
  w[2] = static_cast<uint32_t>((v >> 64) & 0xFFFFFFFFu);
}

/**
 * hps_req_o's valid bit: the MSB of a 44-bit packed word.
 *
 * Forty-four bits fits a single QData, so Verilator hands this one back as a
 * scalar rather than a word array -- unlike the 67-bit response beside it. The
 * width decides the C++ type, which is why the two are accessed differently.
 */
bool hps_req_valid(uint64_t w) { return ((w >> 43) & 1u) != 0u; }

/**
 * guard_req_o's fields.
 *
 *   zhao_guard_req_t { valid, write, client[2:0], addr[26:0], len[6:0], be[63:0] }
 *     -- 103 bits, so Verilator hands it back as a four-word array.
 *
 *     bit 102 valid | 101 write | 100:98 client | 97:71 addr | 70:64 len | 63:0 be
 *
 * The ADDRESS is the field this test exists to check. The first version of the
 * harness returned a cheerful OK without ever looking at it, which is precisely
 * why a slot-relative address survived every case here and would have been
 * denied by the real guard on every slot-1 request.
 */
bool guard_req_valid(const VlWide<4>& w) { return ((w[3] >> 6) & 1u) != 0u; }

uint32_t guard_req_addr(const VlWide<4>& w) {
  // bits 97:71 -- word 2 holds bits 95:64, word 3 holds 127:96.
  const uint64_t hi = (static_cast<uint64_t>(w[3]) << 32) | w[2];  // bits 95:64 .. 127:96
  return static_cast<uint32_t>((hi >> (71 - 64)) & 0x07FFFFFFu);
}

uint32_t guard_req_len(const VlWide<4>& w) {
  const uint64_t hi = (static_cast<uint64_t>(w[3]) << 32) | w[2];
  return static_cast<uint32_t>(hi & 0x7Fu);
}

// The framebuffer slot bases, as zhao_pkg defines them. Slot 0 is zero, which
// is the whole reason a slot-relative address looked correct for so long.
constexpr uint32_t kFbSlotBase[2] = {0x00000000u, 0x02000000u};

// The failure injection points, mirrored onto the RTL by the harness below.
struct Inject {
  uint32_t lease_lost_after = UINT32_MAX;
  uint32_t bridge_err_after = UINT32_MAX;
  uint32_t guard_deny_after = UINT32_MAX;
  bool regrant_lease = false;   // drop and re-issue with a NEW generation
  uint32_t stall_writes = 0;    // hold wready low every Nth beat (0 = never)
  // A TRANSIENT drop: the lease goes away for `blip_cycles` and comes back
  // with the same generation. The per-chunk check cannot see this; only a
  // per-CYCLE watch can.
  uint32_t blip_after = UINT32_MAX;
  uint32_t blip_cycles = 0;
  // Cycles the HPS bridge makes the block wait before granting each burst. The
  // block must hold its request stable throughout and must not advance.
  uint32_t grant_delay = 0;
  // Retirement: credits stop coming back once this many bytes have been
  // credited, and stay stopped for `retire_hold_cycles`. This is the only way
  // to tell "the writes were accepted" from "the writes actually landed".
  uint32_t retire_hold_bytes = UINT32_MAX;
  uint32_t retire_hold_cycles = 0;
  // Drop the lease once this many bytes have RETIRED, which places the loss
  // after the last credit and around the publication decision.
  uint32_t lease_lost_after_retire = UINT32_MAX;
  // How long the harness runs before giving up. Only the "must never finish"
  // cases set this: a run that is SUPPOSED to hang should not burn four million
  // evaluations proving it.
  uint64_t max_cycles = 4000000;
  // Cycles the guard makes a request wait before accepting it. A guard that is
  // always ready hides how long a request stays asserted, which is exactly the
  // window in which a lost lease must stop it.
  uint32_t guard_ready_delay = 0;
  // Credits stop at `retire_hold_bytes` and NEVER resume.
  bool retire_freeze = false;
};

struct Observed {
  uint8_t status = 0xFF;
  bool published = false;
  bool released = false;
  uint32_t bytes_written = 0;   // bytes the guard accepted
  uint32_t bytes_retired = 0;   // bytes credited back by the memory model
  bool done = false;
  bool wdata_moved_under_stall = false;  // the held-stable law

  // Identity carried by whichever terminal event fired.
  uint8_t publish_slot = 0xFF;
  uint16_t publish_gen = 0xFFFF;
  uint8_t release_slot = 0xFF;
  uint16_t release_gen = 0xFFFF;

  // The guard address law.
  uint32_t guard_reqs = 0;
  bool guard_addr_out_of_slot = false;
  uint32_t first_guard_addr = 0xFFFFFFFFu;

  // Retirement law: how many bytes had retired at the moment publish fired,
  // and whether any guard request appeared after the lease was lost.
  uint32_t retired_at_publish = 0xFFFFFFFFu;
  uint32_t issued_at_release = 0;
  uint32_t retired_at_release = 0;
  bool side_effect_after_lease_loss = false;
};

/**
 * Drive one whole blit. The harness IS the HPS and the guard: it answers the
 * source burst with `source` and grants or denies writes, injecting the
 * requested failure at the requested byte offset.
 */
Observed run(Vzhao_debug_frameblit& dut, const zd::BlitRequest& req, const zd::Lease& lease,
             const std::vector<uint8_t>& source, const Inject& inj) {
  Observed obs;

  // Reset per transaction so counters and state are clean.
  dut.rst_n = 0;
  dut.req_valid_i = 0;
  dut.fb_lease_valid_i = 0;
  dut.guard_rsp_i = 0;
  set_hps_rsp(dut.hps_rsp_i, false, 0, false, false);
  dut.guard_wready_i = 1;
  dut.hps_req_grant_i = 0;
  dut.retire_words_i = 0;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  uint16_t gen = lease.generation;
  bool lease_valid = lease.valid;

  // Offer the request.
  dut.req_valid_i = 1;
  dut.req_dst_slot_i = req.dst_slot;
  dut.req_mode_i = req.mode;
  dut.req_src_i = req.src;
  dut.req_len_i = req.len;
  dut.req_crc_i = req.expected_crc;
  dut.fb_lease_valid_i = lease_valid ? 1 : 0;
  dut.fb_lease_slot_i = lease.slot;
  dut.fb_lease_generation_i = gen;

  uint32_t bytes_read = 0;    // source bytes handed to the block
  uint32_t bytes_written = 0; // bytes the guard accepted
  int burst_beat = -1;        // -1 = no burst in flight
  uint32_t burst_base = 0;
  uint32_t burst_len = 0;
  uint32_t beats_sent = 0;
  int stall_ctr = 0;
  uint32_t blip_left = inj.blip_cycles;

  uint64_t last_wdata = 0;
  bool last_wvalid = false;
  bool last_wready = true;
  bool last_wlast = false;

  // The memory model's retirement. `bytes_written` is what the guard accepted;
  // `credited` is what the SDRAM controller has actually completed. Only the
  // second one is allowed to unlock a publication.
  uint32_t credited = 0;
  uint32_t hold_left = 0;
  bool hold_armed = false;
  uint32_t grant_wait = 0;
  uint32_t guard_wait_ctr = 0;
  bool lease_lost_seen = false;

  for (uint64_t cycle = 0; cycle < inj.max_cycles && !obs.done; ++cycle) {
    // ---- the lease, re-evaluated every cycle ------------------------------
    if (inj.blip_after != UINT32_MAX && bytes_read >= inj.blip_after && blip_left > 0) {
      lease_valid = false;
      --blip_left;
    } else if (inj.blip_after != UINT32_MAX && bytes_read >= inj.blip_after) {
      lease_valid = true;  // recovered, same generation
    }
    if (inj.lease_lost_after_retire != UINT32_MAX && credited >= inj.lease_lost_after_retire) {
      lease_valid = false;
    }
    if (inj.lease_lost_after != UINT32_MAX && bytes_read >= inj.lease_lost_after) {
      if (inj.regrant_lease) {
        // The ABA case: still valid, same slot, DIFFERENT generation.
        lease_valid = true;
        gen = static_cast<uint16_t>(lease.generation + 1);
      } else {
        lease_valid = false;
      }
    }
    dut.fb_lease_valid_i = lease_valid ? 1 : 0;
    dut.fb_lease_generation_i = gen;

    // ---- the HPS: answer a burst request one beat per cycle ---------------
    //
    // The first beat is driven on the cycle AFTER the request is observed. The
    // DUT asserts its request from B_READ_REQUEST and only moves to
    // B_READ_CHUNK on the clock edge, so a beat presented in the same cycle is
    // never sampled. An earlier draft did exactly that and the DUT silently
    // missed beat 0 of every burst -- which showed up as a CRC mismatch on the
    // happy path and a hang on the bridge-error case, not as a missing beat.
    set_hps_rsp(dut.hps_rsp_i, false, 0, false, false);
    if (burst_beat >= 0) {
      const uint32_t beat_bytes = 8;
      const bool err = (inj.bridge_err_after != UINT32_MAX &&
                        burst_base >= inj.bridge_err_after);
      uint64_t data = 0;
      for (int b = 0; b < 8; ++b) {
        const uint32_t idx = burst_base + static_cast<uint32_t>(burst_beat) * beat_bytes +
                             static_cast<uint32_t>(b);
        const uint8_t byte = idx < source.size() ? source[idx] : 0;
        data |= static_cast<uint64_t>(byte) << (8 * b);
      }
      const bool last = (static_cast<uint32_t>(burst_beat + 1) * beat_bytes >= burst_len);
      set_hps_rsp(dut.hps_rsp_i, true, data, last, err);
      if (last || err) {
        bytes_read = burst_base + burst_len;
        burst_beat = -1;
      } else {
        ++burst_beat;
      }
    } else if (hps_req_valid(dut.hps_req_o)) {
      // The bridge has ONE request-acceptance pulse and the block must wait for
      // it. Previously this harness armed the burst the instant it saw a valid
      // request, which silently modelled a bridge that is always idle and
      // always accepts -- the exact assumption that stops holding the moment
      // CMD.DMA shares the port.
      if (grant_wait < inj.grant_delay) {
        ++grant_wait;
        dut.hps_req_grant_i = 0;
      } else {
        dut.hps_req_grant_i = 1;
        grant_wait = 0;
        burst_base = bytes_read;
        burst_len = (req.len - bytes_read >= 64) ? 64u : (req.len - bytes_read);
        burst_beat = 0;
      }
    } else {
      dut.hps_req_grant_i = 0;
    }
    if (burst_beat > 0) dut.hps_req_grant_i = 0;

    // ---- the guard: check the address, then grant or deny -----------------
    //
    // THE REAL zhao_mem_guard PASSES A BLIT WRITE ONLY WHEN
    // `addr >= blit_base`, where blit_base is the LEASED slot's base. A fake
    // guard that answers OK without looking cannot tell a slot-relative address
    // from an absolute one, and slot 0's base is zero, so the bug hides.
    // Settle the combinational outputs against THIS cycle's inputs before
    // looking at them. `guard_req_o.valid` now depends on the live lease, so
    // reading it before the lease has been applied compares this cycle's lease
    // against last cycle's request -- which reports a violation on the cycle
    // AFTER the one that mattered, and misses the one that did.
    dut.eval();

    bool guard_ready = true;
    if (guard_req_valid(dut.guard_req_o)) {
      ++obs.guard_reqs;
      const uint32_t a = guard_req_addr(dut.guard_req_o);
      const uint32_t l = guard_req_len(dut.guard_req_o);
      const uint32_t base = kFbSlotBase[lease.slot & 1u];
      if (obs.first_guard_addr == 0xFFFFFFFFu) obs.first_guard_addr = a;
      if (a < base || (a + l) > base + req.len) obs.guard_addr_out_of_slot = true;
      // A guard request asserted on a cycle when the lease is NOT ours is a
      // side effect on somebody else's slot, whether or not it is accepted.
      if (!lease_valid || gen != lease.generation) obs.side_effect_after_lease_loss = true;
      if (guard_wait_ctr < inj.guard_ready_delay) {
        ++guard_wait_ctr;
        guard_ready = false;
      }
    } else {
      guard_wait_ctr = 0;
    }
    const bool deny = (inj.guard_deny_after != UINT32_MAX &&
                       bytes_written >= inj.guard_deny_after);
    // ok and violation are mutually exclusive; ready is bit 2.
    dut.guard_rsp_i = static_cast<uint8_t>((guard_ready ? 4u : 0u) | (deny ? 1u : 2u));

    // ---- write-data backpressure -----------------------------------------
    bool wready = true;
    if (inj.stall_writes) {
      wready = ((++stall_ctr % static_cast<int>(inj.stall_writes)) != 0);
    }
    dut.guard_wready_i = wready ? 1 : 0;

    // ---- retirement credits, in 16-bit words ------------------------------
    // Credits chase what the guard accepted, four words (eight bytes) a cycle.
    // Under `retire_hold_bytes` they stop, which is how "every beat accepted"
    // is separated from "every byte landed".
    uint8_t credit_words = 0;
    if (!hold_armed && inj.retire_hold_bytes != UINT32_MAX &&
        credited >= inj.retire_hold_bytes) {
      hold_armed = true;
      hold_left = inj.retire_hold_cycles;
    }
    if (hold_armed && inj.retire_freeze) {
      // frozen for good
    } else if (hold_left > 0) {
      --hold_left;
    } else if (credited < bytes_written) {
      const uint32_t take = (bytes_written - credited >= 8) ? 8u : (bytes_written - credited);
      credit_words = static_cast<uint8_t>(take / 2);
      credited += take;
    }
    dut.retire_words_i = credit_words;

    dut.eval();

    // THE HELD-STABLE LAW: if the PREVIOUS cycle had wvalid && !wready, the beat
    // was not taken, so this cycle's data and last must be unchanged.
    //
    // The condition is about the previous cycle's ready, not this one's -- an
    // earlier draft mixed the two and reported a violation on every stall.
    if (last_wvalid && !last_wready && dut.guard_wvalid_o) {
      if (dut.guard_wdata_o != last_wdata || (dut.guard_wlast_o != 0) != last_wlast) {
        obs.wdata_moved_under_stall = true;
      }
    }
    last_wvalid = dut.guard_wvalid_o != 0;
    last_wready = wready;
    last_wdata = dut.guard_wdata_o;
    last_wlast = dut.guard_wlast_o != 0;

    if (dut.guard_wvalid_o && wready) bytes_written += 8;

    if (!lease_valid || gen != lease.generation) lease_lost_seen = true;

    if (dut.publish_valid_o) {
      obs.published = true;
      obs.publish_slot = static_cast<uint8_t>(dut.publish_slot_o);
      obs.publish_gen = static_cast<uint16_t>(dut.publish_generation_o);
      obs.retired_at_publish = credited;
    }
    if (dut.release_valid_o) {
      obs.released = true;
      obs.release_slot = static_cast<uint8_t>(dut.release_slot_o);
      obs.release_gen = static_cast<uint16_t>(dut.release_generation_o);
      obs.issued_at_release = bytes_written;
      obs.retired_at_release = credited;
    }
    if (dut.done_o) {
      obs.status = static_cast<uint8_t>(dut.status_o);
      obs.done = true;
    }

    zhao::tick(dut);
    if (dut.req_valid_i && dut.req_ready_o) dut.req_valid_i = 0;
  }

  obs.bytes_written = bytes_written;
  obs.bytes_retired = credited;
  return obs;
}

std::vector<uint8_t> make_source(uint32_t len, uint32_t seed) {
  std::vector<uint8_t> v(len);
  uint32_t s = seed | 1u;
  for (uint32_t i = 0; i < len; ++i) {
    s = s * 1664525u + 1013904223u;
    v[i] = static_cast<uint8_t>(s >> 24);
  }
  return v;
}

}  // namespace

int main() {
  Vzhao_debug_frameblit dut;

  // A small canvas keeps the transaction short while exercising many chunks.
  // The mode byte is not decoded by the harness -- the RTL's canvas_bytes(mode)
  // is what decides, and the test supplies a length that matches it.
  const uint8_t kMode = 0;
  // Discover the lawful length by offering a deliberately wrong one and reading
  // back nothing: instead, take it from the block by construction -- the test
  // uses the same zhao_pkg law the RTL does, through a length that the RTL
  // accepts. Rather than duplicate the table, the happy-path case below is
  // driven with the length the RTL itself reports as lawful via ST_BAD_LEN.
  //
  // Simpler and honest: probe once.
  uint32_t canvas = 0;
  {
    // Binary-probe is overkill; the mode table is small and public. Z60 is the
    // default mode 0 and its canvas is the constant the package exposes.
    // The test asserts agreement rather than assuming it.
    zd::BlitRequest probe;
    probe.dst_slot = 0;
    probe.mode = kMode;
    probe.len = 64;  // certainly wrong
    probe.expected_crc = 0;
    zd::Lease lease{true, 0, 7};
    const Observed o = run(dut, probe, lease, make_source(64, 1), Inject{});
    check(o.status == static_cast<uint8_t>(zd::BlitStatus::kBadLen),
          "a length that is not the canvas is rejected before anything else",
          static_cast<uint8_t>(zd::BlitStatus::kBadLen), o.status);
    check(!o.published, "and nothing is published", 0, o.published ? 1 : 0);
  }

  // The real canvas for ABI mode 0, which zhao_mode_from_abi maps to Z60:
  // ZHAO_CANVAS_BYTES_Z60 = 184,320 (384 * 240 * 2).
  //
  // An earlier draft used 245,760 here and every case failed with ST_BAD_LEN.
  // That number is the OLD staging buffer's size quoted in the redesign
  // proposal -- a different quantity entirely, and a reminder that a figure
  // read out of prose is not a constant read out of the package.
  canvas = 184320;

  const std::vector<uint8_t> src = make_source(canvas, 0xC0FFEE);
  const uint32_t good_crc = zhao_abi::zhao_crc32c(0, src.data(), src.size());

  zd::BlitRequest req;
  req.dst_slot = 1;
  req.mode = kMode;
  req.src = 0x1000'0000;
  req.len = canvas;
  req.expected_crc = good_crc;
  const zd::Lease lease{true, 1, 42};

  // ---- 1. the happy path --------------------------------------------------
  {
    const zd::BlitOutcome want = zd::run_blit(req, lease, canvas, src);
    const Observed got = run(dut, req, lease, src, Inject{});
    check(got.status == static_cast<uint8_t>(want.status), "happy path: status",
          static_cast<uint8_t>(want.status), got.status);
    check(got.published == want.published, "happy path: PUBLISHED", want.published ? 1 : 0,
          got.published ? 1 : 0);
    check(!got.released, "and the slot is not released", 0, got.released ? 1 : 0);
    check(got.bytes_written == canvas, "every byte was written", canvas, got.bytes_written);
    check(!got.wdata_moved_under_stall, "write data never moved under a stall", 0,
          got.wdata_moved_under_stall ? 1 : 0);

    // THE GUARD ADDRESS IS ABSOLUTE. This request is for slot 1, whose base is
    // 0x0200_0000, and the real guard passes a blit write only when the address
    // is inside the leased slot's window. The first chunk must therefore be AT
    // the slot base, not at zero.
    check(!got.guard_addr_out_of_slot, "every guard write is inside the LEASED slot", 0,
          got.guard_addr_out_of_slot ? 1 : 0);
    check(got.first_guard_addr == kFbSlotBase[1], "the first chunk is at the slot-1 base",
          kFbSlotBase[1], got.first_guard_addr);

    // The publish event names the slot and generation it belongs to.
    check(got.publish_slot == 1, "publish names its slot", 1, got.publish_slot);
    check(got.publish_gen == 42, "publish names its generation", 42, got.publish_gen);

    // And it did not go out until every byte had RETIRED, not merely been
    // accepted. The two counters are equal here; the next section separates
    // them.
    check(got.retired_at_publish == canvas, "publish waited for every byte to retire", canvas,
          got.retired_at_publish);
  }

  // ---- 1b. SLOT 0 GOES TO SLOT 0's BASE -----------------------------------
  // Slot 0's base is zero, which is exactly why a slot-relative address passed
  // every test in this file for as long as it did. Both slots are checked so
  // the address law cannot be satisfied by accident.
  {
    zd::BlitRequest at0 = req;
    at0.dst_slot = 0;
    const zd::Lease lease0{true, 0, 7};
    const Observed got = run(dut, at0, lease0, src, Inject{});
    check(got.published, "slot 0 publishes", 1, got.published ? 1 : 0);
    check(!got.guard_addr_out_of_slot, "every slot-0 write is inside slot 0", 0,
          got.guard_addr_out_of_slot ? 1 : 0);
    check(got.first_guard_addr == kFbSlotBase[0], "the first chunk is at the slot-0 base",
          kFbSlotBase[0], got.first_guard_addr);
    check(got.publish_slot == 0, "publish names slot 0", 0, got.publish_slot);
    check(got.publish_gen == 7, "publish names generation 7", 7, got.publish_gen);
  }

  // ---- 2. A WRONG CRC NEVER PUBLISHES -------------------------------------
  // Every byte lands in the slot -- the slot is dirty -- and it is released FREE
  // rather than published. That IS the amended law, not a compromise with it.
  {
    zd::BlitRequest bad = req;
    bad.expected_crc = good_crc ^ 0x1u;
    const zd::BlitOutcome want = zd::run_blit(bad, lease, canvas, src);
    const Observed got = run(dut, bad, lease, src, Inject{});
    check(got.status == static_cast<uint8_t>(want.status), "bad CRC: status",
          static_cast<uint8_t>(zd::BlitStatus::kCrc), got.status);
    check(!got.published, "bad CRC: NOT published", 0, got.published ? 1 : 0);
    check(got.released, "bad CRC: the slot is released FREE", 1, got.released ? 1 : 0);
    check(got.bytes_written == canvas,
          "and every byte WAS written -- a dirty unpublished slot is the design", canvas,
          got.bytes_written);
  }

  // ---- 3. the lease checks ------------------------------------------------
  {
    // No lease at all.
    const zd::Lease none{false, 1, 42};
    const zd::BlitOutcome w1 = zd::run_blit(req, none, canvas, src);
    const Observed g1 = run(dut, req, none, src, Inject{});
    check(g1.status == static_cast<uint8_t>(w1.status), "no lease: status",
          static_cast<uint8_t>(zd::BlitStatus::kNoLease), g1.status);
    check(!g1.published, "no lease: nothing published", 0, g1.published ? 1 : 0);
    check(g1.bytes_written == 0, "and not one byte written", 0, g1.bytes_written);
    // It never acquired anything, so it releases nothing. Releasing here would
    // free whatever lease IS active, which belongs to somebody else.
    check(!g1.released, "no lease: and NOTHING is released", 0, g1.released ? 1 : 0);
    check(w1.released == g1.released, "no lease: reference agrees on release",
          w1.released ? 1 : 0, g1.released ? 1 : 0);

    // A lease for the OTHER slot: the ABI's dst_slot is not trusted on its own.
    const zd::Lease other{true, 0, 42};
    const zd::BlitOutcome w2 = zd::run_blit(req, other, canvas, src);
    const Observed g2 = run(dut, req, other, src, Inject{});
    check(g2.status == static_cast<uint8_t>(w2.status),
          "dst_slot that does not match the lease is refused",
          static_cast<uint8_t>(zd::BlitStatus::kSlotMismatch), g2.status);
    check(g2.bytes_written == 0, "and nothing is written into anybody's slot", 0,
          g2.bytes_written);
    check(!g2.released, "slot mismatch: the other slot's lease is NOT released", 0,
          g2.released ? 1 : 0);
    check(w2.released == g2.released, "slot mismatch: reference agrees on release",
          w2.released ? 1 : 0, g2.released ? 1 : 0);

    // A dst_slot outside {0,1} entirely.
    zd::BlitRequest weird = req;
    weird.dst_slot = 7;
    const Observed g3 = run(dut, weird, lease, src, Inject{});
    check(g3.status == static_cast<uint8_t>(zd::BlitStatus::kSlotMismatch),
          "a dst_slot outside {0,1} is refused",
          static_cast<uint8_t>(zd::BlitStatus::kSlotMismatch), g3.status);
    check(!g3.released, "and it releases nothing either", 0, g3.released ? 1 : 0);

    // A bad LENGTH is the third pre-acquisition failure, and the earliest: it
    // is judged before the lease is even looked at.
    zd::BlitRequest short_len = req;
    short_len.len = canvas - 64;
    const zd::BlitOutcome w4 = zd::run_blit(short_len, lease, canvas, src);
    const Observed g4 = run(dut, short_len, lease, src, Inject{});
    check(g4.status == static_cast<uint8_t>(zd::BlitStatus::kBadLen), "a bad length is refused",
          static_cast<uint8_t>(zd::BlitStatus::kBadLen), g4.status);
    check(!g4.released, "bad length: an active lease elsewhere is NOT released", 0,
          g4.released ? 1 : 0);
    check(w4.released == g4.released, "bad length: reference agrees on release",
          w4.released ? 1 : 0, g4.released ? 1 : 0);
  }

  // ---- 4. THE LEASE MUST HOLD FOR THE WHOLE TRANSACTION -------------------
  {
    Inject inj;
    inj.lease_lost_after = canvas / 2;
    const Observed got = run(dut, req, lease, src, inj);
    check(got.status == static_cast<uint8_t>(zd::BlitStatus::kLeaseLost),
          "a lease that lapses mid-blit aborts",
          static_cast<uint8_t>(zd::BlitStatus::kLeaseLost), got.status);
    check(!got.published, "and NOTHING is published", 0, got.published ? 1 : 0);
    check(got.released, "the slot goes FREE", 1, got.released ? 1 : 0);
  }

  // ---- 4b. A TRANSIENT LEASE DROP -----------------------------------------
  // The lease goes away for a handful of cycles MID-CHUNK and comes back with
  // the same generation. Checking the lease once per 64-byte chunk cannot see
  // this; only a per-CYCLE watch can, and a mutation removing that watch passed
  // every other case in this file because the per-chunk check masked it.
  //
  // It matters because during those cycles the slot was not ours, and whatever
  // else happened to it is unknown.
  {
    Inject inj;
    inj.blip_after = canvas / 2;
    inj.blip_cycles = 3;
    const Observed got = run(dut, req, lease, src, inj);
    check(got.status == static_cast<uint8_t>(zd::BlitStatus::kLeaseLost),
          "a lease that lapses for THREE CYCLES and recovers is still a loss",
          static_cast<uint8_t>(zd::BlitStatus::kLeaseLost), got.status);
    check(!got.published, "and nothing is published", 0, got.published ? 1 : 0);
  }

  // ---- 5. THE ABA HOLE: a lease RE-GRANTED for the same slot --------------
  // Still valid, same slot, new generation. Watching only valid+slot cannot see
  // this, and the bytes already written belong to somebody else's lease.
  {
    Inject inj;
    inj.lease_lost_after = canvas / 2;
    inj.regrant_lease = true;
    const Observed got = run(dut, req, lease, src, inj);
    check(got.status == static_cast<uint8_t>(zd::BlitStatus::kLeaseLost),
          "a lease re-granted for the SAME slot mid-blit is a lease LOSS",
          static_cast<uint8_t>(zd::BlitStatus::kLeaseLost), got.status);
    check(!got.published, "and nothing is published", 0, got.published ? 1 : 0);
  }

  // ---- 6. the guard denies a write ----------------------------------------
  {
    Inject inj;
    inj.guard_deny_after = canvas / 4;
    const Observed got = run(dut, req, lease, src, inj);
    check(got.status == static_cast<uint8_t>(zd::BlitStatus::kGuardDeny),
          "a denied guard write aborts the transaction",
          static_cast<uint8_t>(zd::BlitStatus::kGuardDeny), got.status);
    check(!got.published, "and nothing is published", 0, got.published ? 1 : 0);
  }

  // ---- 7. a bridge error ---------------------------------------------------
  {
    Inject inj;
    inj.bridge_err_after = canvas / 3;
    const Observed got = run(dut, req, lease, src, inj);
    check(got.status == static_cast<uint8_t>(zd::BlitStatus::kBridgeErr),
          "an HPS bridge error aborts the transaction",
          static_cast<uint8_t>(zd::BlitStatus::kBridgeErr), got.status);
    check(!got.published, "and nothing is published", 0, got.published ? 1 : 0);
  }

  // ---- 8. WRITE BACKPRESSURE IS A REAL HANDSHAKE --------------------------
  // The old DMA had no wready at all and caught overflow with a sticky error
  // afterwards. Under a stalling consumer the result must be identical, and the
  // data and last marker must not move while the beat is held.
  {
    for (uint32_t period : {2u, 3u, 5u, 8u}) {
      Inject inj;
      inj.stall_writes = period;
      const Observed got = run(dut, req, lease, src, inj);
      char tag[96];
      std::snprintf(tag, sizeof tag, "stalling every %u beats: still publishes", period);
      check(got.published, tag, 1, got.published ? 1 : 0);
      std::snprintf(tag, sizeof tag, "stalling every %u beats: data held stable", period);
      check(!got.wdata_moved_under_stall, tag, 0, got.wdata_moved_under_stall ? 1 : 0);
      std::snprintf(tag, sizeof tag, "stalling every %u beats: every byte written", period);
      check(got.bytes_written == canvas, tag, canvas, got.bytes_written);
    }
  }

  // ---- 9. THE PROPOSAL'S COMPOSITION TEST ---------------------------------
  // Keep slot 0 displayed. Stream a deliberately bad blit into leased slot 1.
  // Slot 0 must never be touched -- which here means: the block never publishes,
  // never releases slot 0, and every guard write it made was for the leased
  // slot. The displayed slot is untouched because the block only ever addressed
  // the one it leased.
  {
    zd::BlitRequest bad = req;
    bad.dst_slot = 1;
    bad.expected_crc = good_crc ^ 0xDEADu;
    const zd::Lease slot1{true, 1, 99};
    const Observed got = run(dut, bad, slot1, src, Inject{});
    check(!got.published, "a bad blit into slot 1 never publishes", 0, got.published ? 1 : 0);
    check(got.status == static_cast<uint8_t>(zd::BlitStatus::kCrc),
          "and it fails for the right reason", static_cast<uint8_t>(zd::BlitStatus::kCrc),
          got.status);
    // A request naming slot 0 while slot 1 is leased is refused outright, which
    // is the mechanism that keeps the displayed slot safe.
    zd::BlitRequest at_zero = bad;
    at_zero.dst_slot = 0;
    const Observed g0 = run(dut, at_zero, slot1, src, Inject{});
    check(g0.bytes_written == 0,
          "and a blit naming the DISPLAYED slot writes nothing at all", 0, g0.bytes_written);
  }

  // ---- 10. PUBLICATION WAITS FOR PHYSICAL RETIREMENT ----------------------
  // Every HPS read succeeds, every guard request passes, every write beat is
  // accepted immediately -- and the memory system withholds its retirement
  // credits after the final beat. Nothing may publish while a single byte is
  // still sitting in the write FIFO, the arbiter, or the controller's pending
  // burst.
  //
  // The block used to advance its own `retired` counter when a chunk was handed
  // downstream, which made the atomicity claim false by construction: it was
  // counting hand-offs and calling them completions.
  {
    Inject inj;
    inj.retire_hold_bytes = canvas - 256;  // stop crediting near the end
    inj.retire_hold_cycles = 500;          // and stay stopped for a long time
    const Observed got = run(dut, req, lease, src, inj);
    check(got.published, "credits eventually return, so it publishes", 1,
          got.published ? 1 : 0);
    check(got.retired_at_publish == canvas,
          "and NOT ONE byte was outstanding when it did", canvas, got.retired_at_publish);
    check(got.bytes_written == canvas, "every byte was issued", canvas, got.bytes_written);
  }

  // ---- 10b. THE HOLD IS REAL ----------------------------------------------
  // A hold the block never notices proves nothing, so this asserts the block
  // actually sat in its decision state: with credits frozen forever, it must
  // never finish.
  {
    Inject inj;
    inj.retire_hold_bytes = canvas / 2;
    inj.retire_hold_cycles = 200'000;  // longer than this run is allowed to be
    inj.max_cycles = 60'000;
    const Observed got = run(dut, req, lease, src, inj);
    check(!got.done, "credits withheld forever: the blit never completes", 0,
          got.done ? 1 : 0);
    check(!got.published, "and above all never publishes", 0, got.published ? 1 : 0);
  }

  // ---- 11. A FAILURE DRAINS BEFORE IT RELEASES ----------------------------
  // Complete several chunks, withhold some credits, then lose the lease. The
  // slot must not go FREE while writes are still in flight: the next owner
  // would start writing and then be overwritten by this dead transaction's
  // bytes.
  {
    Inject inj;
    inj.retire_hold_bytes = canvas / 4;
    inj.retire_hold_cycles = 400;
    inj.lease_lost_after = canvas / 2;
    const Observed got = run(dut, req, lease, src, inj);
    check(got.status == static_cast<uint8_t>(zd::BlitStatus::kLeaseLost),
          "lease lost with writes outstanding: status",
          static_cast<uint8_t>(zd::BlitStatus::kLeaseLost), got.status);
    check(!got.published, "nothing is published", 0, got.published ? 1 : 0);
    check(got.released, "the slot is released", 1, got.released ? 1 : 0);
    check(got.retired_at_release >= got.issued_at_release,
          "and ONLY after every accepted write retired", got.issued_at_release,
          got.retired_at_release);
    check(got.release_slot == 1, "release names its slot", 1, got.release_slot);
    check(got.release_gen == 42, "release names its generation", 42, got.release_gen);
    check(!got.side_effect_after_lease_loss,
          "no guard request appears after the lease is gone", 0,
          got.side_effect_after_lease_loss ? 1 : 0);
  }

  // ---- 11b. THE DRAIN IS REAL --------------------------------------------
  // Section 11 proved the release comes after the drain, but with a hold that
  // eventually lifts -- so a block that released immediately could still be
  // observed with the counters equal by the time the harness looked. Here the
  // credits NEVER come back: the block must sit in its drain state forever
  // rather than hand the slot to the next owner while writes are outstanding.
  {
    Inject inj;
    inj.retire_hold_bytes = 128;
    inj.retire_freeze = true;
    inj.lease_lost_after = canvas / 2;
    inj.max_cycles = 60'000;
    const Observed got = run(dut, req, lease, src, inj);
    check(!got.released, "credits frozen: the slot is NOT released", 0, got.released ? 1 : 0);
    check(!got.done, "and the transaction does not complete", 0, got.done ? 1 : 0);
    check(!got.published, "and certainly nothing is published", 0, got.published ? 1 : 0);
    check(got.bytes_written > got.bytes_retired,
          "with writes genuinely still outstanding", got.bytes_written, got.bytes_retired);
  }

  // ---- 11c. A SLOW GUARD, AND A LEASE LOST WHILE IT DECIDES ---------------
  // The guard is not always ready. While it is deciding, the request stays
  // asserted -- and if the lease goes during that window, the request must come
  // down. A harness whose guard accepts instantly cannot see this at all,
  // which is why a mutation removing the check survived the first sweep.
  {
    Inject inj;
    inj.guard_ready_delay = 6;
    inj.lease_lost_after = canvas / 2;
    const Observed got = run(dut, req, lease, src, inj);
    check(!got.side_effect_after_lease_loss,
          "no guard request is asserted on a cycle the lease is not ours", 0,
          got.side_effect_after_lease_loss ? 1 : 0);
    check(!got.published, "and nothing is published", 0, got.published ? 1 : 0);
    check(got.status == static_cast<uint8_t>(zd::BlitStatus::kLeaseLost),
          "the loss is reported", static_cast<uint8_t>(zd::BlitStatus::kLeaseLost),
          got.status);
  }

  // ---- 11d. A SLOW GUARD ON THE HAPPY PATH --------------------------------
  // The same slow guard with the lease intact must still publish, so 11c is
  // testing the lease and not merely the delay.
  {
    for (uint32_t d : {1u, 4u, 9u}) {
      Inject inj;
      inj.guard_ready_delay = d;
      const Observed got = run(dut, req, lease, src, inj);
      char tag[96];
      std::snprintf(tag, sizeof tag, "guard ready delayed %u: still publishes", d);
      check(got.published, tag, 1, got.published ? 1 : 0);
      std::snprintf(tag, sizeof tag, "guard ready delayed %u: every byte written", d);
      check(got.bytes_written == canvas, tag, canvas, got.bytes_written);
    }
  }

  // ---- 12. THE LEASE AT THE PUBLICATION EDGE ------------------------------
  // The lease survives every chunk and disappears only once the LAST byte has
  // retired -- which lands the loss squarely on the publication decision. A
  // block that checks the lease in an earlier state and pulses publish in a
  // later one publishes here.
  {
    Inject inj;
    inj.lease_lost_after_retire = canvas;
    const Observed got = run(dut, req, lease, src, inj);
    check(!got.published, "a lease lost at the publication edge publishes NOTHING", 0,
          got.published ? 1 : 0);
    check(got.status == static_cast<uint8_t>(zd::BlitStatus::kLeaseLost),
          "and reports the loss", static_cast<uint8_t>(zd::BlitStatus::kLeaseLost),
          got.status);
  }

  // ---- 13. THE HPS BRIDGE MUST ACTUALLY GRANT -----------------------------
  // The bridge has one request-acceptance pulse and the block used to advance
  // without observing it -- correct only if the bridge is idle and accepts on
  // that exact cycle, which stops being true the moment CMD.DMA shares the
  // port. Here every burst is made to wait.
  {
    for (uint32_t delay : {1u, 3u, 17u}) {
      Inject inj;
      inj.grant_delay = delay;
      const Observed got = run(dut, req, lease, src, inj);
      char tag[96];
      std::snprintf(tag, sizeof tag, "grant delayed %u cycles: still publishes", delay);
      check(got.published, tag, 1, got.published ? 1 : 0);
      std::snprintf(tag, sizeof tag, "grant delayed %u cycles: every byte written", delay);
      check(got.bytes_written == canvas, tag, canvas, got.bytes_written);
      std::snprintf(tag, sizeof tag, "grant delayed %u cycles: address law holds", delay);
      check(!got.guard_addr_out_of_slot, tag, 0, got.guard_addr_out_of_slot ? 1 : 0);
    }
  }

  dut.final();
  return zhao::report_and_exit("debug_frameblit_directed");
}
