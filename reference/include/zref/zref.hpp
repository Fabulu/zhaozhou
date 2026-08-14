// zref.hpp — the empty-frame ZRef shell loop (charter §20.1/§23 Phase-1
// build item "empty ZRef"; plan W6). The per-frame semantics (fail-safe
// validation order + implemented-commands-as-counted-no-ops) are owned by
// zref_frame.hpp per spec/capture_format.md §3.2-3.3; this header adds the
// SESSION layer the host products (ZEmu, desktop runtime stub) link:
//
//   submit sealed packet -> validate -> execute implemented commands as
//   counted no-ops -> {status, completion_flags, counters}, with the
//   counters accumulated across every frame the session has seen.
//
// Phase-1 scope note (charter §23 Phase 1): the shell is deliberately EMPTY
// of game semantics — BeginFrame/SetView/SetPresentationContract/Nop are
// counted no-ops, reserved commands report ZH_ABI_UNIMPLEMENTED_COMMAND at
// execution time while remaining structurally valid packets (spec §3.3:
// packet validity and execution capability are separate axes).
//
// Law: spec/capture_format.md (§3 frame packet, §3.2 fail-safe order, §3.3
// execution semantics). Layout constants and error codes come from the
// GENERATED runtime/include/zhao_abi.h — never re-derived here (charter
// §29-5/29-6).

#pragma once

#include "zref/zref_frame.hpp"

#include <cstdint>
#include <vector>

namespace zhao {

/** Cumulative session counters (spec §3.3 counters, summed over frames). */
struct ZhaoShellCounters {
  uint32_t frames_submitted = 0;  // every submit() call
  uint32_t frames_accepted = 0;   // validated AND executed
  uint32_t frames_rejected = 0;   // failed validation or execution
  uint32_t bytes_consumed = 0;    // sum of per-frame bytes_consumed
  uint32_t commands_total = 0;
  uint32_t begin_frames = 0;
  uint32_t end_frames = 0;
  uint32_t nops = 0;
};

/**
 * The empty ZRef shell. One instance = one replay session (a .zcap's frames
 * or a live builder stream). Thread-safety: none — one shell per consumer,
 * as with the RTL stub it mirrors.
 */
class ZhaoZrefShell {
 public:
  /** Validate + execute one sealed frame packet (spec §3.2 order, §3.3). */
  ZhaoExecutionResult submit(const uint8_t* pkt, size_t len,
                             uint32_t slot_bytes = zhao_abi::FRAME_SLOT_BYTES);
  ZhaoExecutionResult submit(const std::vector<uint8_t>& pkt,
                             uint32_t slot_bytes = zhao_abi::FRAME_SLOT_BYTES);

  const ZhaoShellCounters& session() const { return session_; }
  uint8_t last_status() const { return last_.status; }
  uint8_t last_completion_flags() const { return last_.completion_flags; }
  const ZhaoFrameCounters& last_frame_counters() const {
    return last_.counters;
  }

  /** Clear the session (counters + last-frame latch). */
  void reset();

 private:
  ZhaoShellCounters session_;
  ZhaoExecutionResult last_;
};

}  // namespace zhao
