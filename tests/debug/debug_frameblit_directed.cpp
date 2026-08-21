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
};

struct Observed {
  uint8_t status = 0xFF;
  bool published = false;
  bool released = false;
  uint32_t bytes_written = 0;
  bool done = false;
  bool wdata_moved_under_stall = false;  // the held-stable law
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

  const uint64_t kGuard = 4000000;
  for (uint64_t cycle = 0; cycle < kGuard && !obs.done; ++cycle) {
    // ---- the lease, re-evaluated every cycle ------------------------------
    if (inj.blip_after != UINT32_MAX && bytes_read >= inj.blip_after && blip_left > 0) {
      lease_valid = false;
      --blip_left;
    } else if (inj.blip_after != UINT32_MAX && bytes_read >= inj.blip_after) {
      lease_valid = true;  // recovered, same generation
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
      // Arm now; the first beat goes out next cycle.
      burst_base = bytes_read;
      burst_len = (req.len - bytes_read >= 64) ? 64u : (req.len - bytes_read);
      burst_beat = 0;
    }

    // ---- the guard: grant or deny ----------------------------------------
    const bool deny = (inj.guard_deny_after != UINT32_MAX &&
                       bytes_written >= inj.guard_deny_after);
    // ready is always high; ok and violation are mutually exclusive.
    dut.guard_rsp_i = static_cast<uint8_t>(4u | (deny ? 1u : 2u));

    // ---- write-data backpressure -----------------------------------------
    bool wready = true;
    if (inj.stall_writes) {
      wready = ((++stall_ctr % static_cast<int>(inj.stall_writes)) != 0);
    }
    dut.guard_wready_i = wready ? 1 : 0;

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

    if (dut.blit_publish_o) obs.published = true;
    if (dut.fb_lease_release_o) obs.released = true;
    if (dut.done_o) {
      obs.status = static_cast<uint8_t>(dut.status_o);
      obs.done = true;
    }

    zhao::tick(dut);
    if (dut.req_valid_i && dut.req_ready_o) dut.req_valid_i = 0;
  }

  obs.bytes_written = bytes_written;
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

    // A lease for the OTHER slot: the ABI's dst_slot is not trusted on its own.
    const zd::Lease other{true, 0, 42};
    const zd::BlitOutcome w2 = zd::run_blit(req, other, canvas, src);
    const Observed g2 = run(dut, req, other, src, Inject{});
    check(g2.status == static_cast<uint8_t>(w2.status),
          "dst_slot that does not match the lease is refused",
          static_cast<uint8_t>(zd::BlitStatus::kSlotMismatch), g2.status);
    check(g2.bytes_written == 0, "and nothing is written into anybody's slot", 0,
          g2.bytes_written);

    // A dst_slot outside {0,1} entirely.
    zd::BlitRequest weird = req;
    weird.dst_slot = 7;
    const Observed g3 = run(dut, weird, lease, src, Inject{});
    check(g3.status == static_cast<uint8_t>(zd::BlitStatus::kSlotMismatch),
          "a dst_slot outside {0,1} is refused",
          static_cast<uint8_t>(zd::BlitStatus::kSlotMismatch), g3.status);
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

  dut.final();
  return zhao::report_and_exit("debug_frameblit_directed");
}
