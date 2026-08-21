// zref_frameblit.hpp — DEBUG.FRAMEBLIT's reference model.
//
// A NEW BLOCK, and a new reference. `reports/CMD.DMA_Redesign_Proposal.md`
// Part 2 is the design; this file is that design as an executable law so the RTL
// has something to be a differential against.
//
// The CRC arithmetic DELEGATES to `zhao_abi::zhao_crc32c`, the one generated
// implementation, exactly as `zref::cmd2::Crc32c` does. Nothing here
// re-implements a polynomial.
//
// ---------------------------------------------------------------------------
// THE AMENDED ATOMICITY LAW
// ---------------------------------------------------------------------------
// OLD: no byte is written to VRAM before CRC verification. That is what forced a
//      whole-canvas staging buffer inside CMD.DMA — 1,966,080 bits — and that
//      buffer is what stopped the command front end fitting.
//
// NEW: **no framebuffer slot becomes visible or READY before every byte has been
//      written, all writes have retired, and the CRC matches.**
//
// The amendment is sound because raw writes into an inactive, uncommitted slot
// are not visible: the shell only toggles slot readiness when `blit_status == 0`
// and FRAMECTL only swaps to a committed READY slot. The externally meaningful
// commit point was never the first write. So the framebuffer slot IS the
// transaction buffer, and the staging buffer collapses to one 64-byte chunk.
//
// A DIRTY UNPUBLISHED SLOT IS AN ACCEPTED OUTCOME. On any failure the slot is
// released FREE with whatever bytes happened to land in it. That is not
// sloppiness; it is the point. What must never happen is publishing it.
//
// ---------------------------------------------------------------------------
// THE LEASE
// ---------------------------------------------------------------------------
// `dst_slot` from the frozen ABI is no longer trusted merely because it is 0 or
// 1 — it must MATCH the slot the shell leased. And the lease must hold for the
// WHOLE transaction, generation included: watching only valid+slot has an ABA
// hole, where a lease that drops and is re-granted for the same slot looks
// exactly like one that never lapsed, while the bytes already written belong to
// somebody else's lease.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "zhao_abi.h"  // generated (runtime/include): zhao_crc32c

namespace zref {
namespace debug {

/** Why a blit ended. Zero is the only success. */
enum class BlitStatus : uint8_t {
  kOk = 0,
  kBadLen = 1,        // len != canvas_bytes(mode)
  kNoLease = 2,       // no lease at start
  kSlotMismatch = 3,  // dst_slot != the leased slot
  kLeaseLost = 4,     // the lease lapsed or was re-granted mid-transaction
  kBridgeErr = 5,     // HPS burst error
  kGuardDeny = 6,     // the guard refused a write
  kCrc = 7,           // every byte written, CRC wrong
};

/** One blit request, as CMD.SCHEDULER dispatches it. */
struct BlitRequest {
  uint8_t dst_slot = 0;
  uint8_t mode = 0;
  uint32_t src = 0;
  uint32_t len = 0;
  uint32_t expected_crc = 0;
};

/** What the shell's lease seam grants. */
struct Lease {
  bool valid = false;
  uint8_t slot = 0;
  uint16_t generation = 0;
};

struct BlitOutcome {
  BlitStatus status = BlitStatus::kOk;
  bool published = false;   // the slot became READY
  bool released = false;    // the slot went FREE
  uint32_t bytes_written = 0;
  uint32_t computed_crc = 0;
};

/**
 * Run one blit transaction.
 *
 * `source` is the bytes the HPS actually returns. `canvas_bytes` is the mode's
 * lawful length — supplied rather than computed, because the mode table is
 * `zhao_pkg`'s and not this file's. `lease_lost_after` models the lease lapsing
 * (or being re-granted, which is the same thing) once that many bytes have been
 * read; `UINT32_MAX` means it never lapses. `bridge_err_after` and
 * `guard_deny_after` do the same for their failures.
 *
 * The order of the checks is the law, not a convenience: a bad length is
 * rejected before a lease is even looked at, because a malformed command should
 * not consume a slot.
 */
inline BlitOutcome run_blit(const BlitRequest& req, const Lease& lease, uint32_t canvas_bytes,
                            const std::vector<uint8_t>& source,
                            uint32_t lease_lost_after = UINT32_MAX,
                            uint32_t bridge_err_after = UINT32_MAX,
                            uint32_t guard_deny_after = UINT32_MAX) {
  BlitOutcome o;

  if (req.len != canvas_bytes) {
    o.status = BlitStatus::kBadLen;
    o.released = true;
    return o;
  }
  if (!lease.valid) {
    o.status = BlitStatus::kNoLease;
    o.released = true;
    return o;
  }
  if (req.dst_slot > 1 || req.dst_slot != lease.slot) {
    o.status = BlitStatus::kSlotMismatch;
    o.released = true;
    return o;
  }

  uint32_t crc = 0;
  uint32_t done = 0;
  while (done < req.len) {
    const uint32_t chunk = (req.len - done >= 64) ? 64u : (req.len - done);

    if (done >= bridge_err_after) {
      o.status = BlitStatus::kBridgeErr;
      o.released = true;
      o.bytes_written = done;
      return o;
    }
    if (done >= lease_lost_after) {
      o.status = BlitStatus::kLeaseLost;
      o.released = true;
      o.bytes_written = done;
      return o;
    }

    // The CRC covers the SOURCE stream in order, chunk by chunk, and is
    // accumulated as the bytes pass through -- there is no second read.
    crc = zhao_abi::zhao_crc32c(crc, source.data() + done, chunk);

    if (done >= guard_deny_after) {
      o.status = BlitStatus::kGuardDeny;
      o.released = true;
      o.bytes_written = done;
      return o;
    }

    done += chunk;
    o.bytes_written = done;
  }

  o.computed_crc = crc;
  if (crc != req.expected_crc) {
    // Every byte is in the slot and the slot is dirty. It is never published.
    o.status = BlitStatus::kCrc;
    o.released = true;
    return o;
  }

  o.status = BlitStatus::kOk;
  o.published = true;
  return o;
}

}  // namespace debug
}  // namespace zref
