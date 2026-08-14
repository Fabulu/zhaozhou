// zref.cpp — the empty-frame ZRef shell loop (charter §20.1/§23 Phase-1
// "empty ZRef"; plan W6). Session layer over the per-frame executor in
// zref_frame.cpp. Law: spec/capture_format.md §3.2 (fail-safe order) and
// §3.3 (implemented commands execute as no-ops with counters; reserved
// commands validate structurally but report ZH_ABI_UNIMPLEMENTED_COMMAND at
// execution time). No game semantics — this is the differential anchor for
// the stub RTL, byte-for-byte in status/completion/counters.

#include "zref/zref.hpp"

namespace zhao {

ZhaoExecutionResult ZhaoZrefShell::submit(const uint8_t* pkt, size_t len,
                                          uint32_t slot_bytes) {
  // The per-frame verdict comes from the single W4 executor (never a second
  // implementation — charter §29-6); the shell only accumulates.
  last_ = zhao_frame_execute_empty(pkt, len, slot_bytes);
  session_.frames_submitted++;
  session_.frames_accepted += last_.counters.frames_accepted;
  session_.frames_rejected += last_.counters.frames_rejected;
  session_.bytes_consumed += last_.counters.bytes_consumed;
  session_.commands_total += last_.counters.commands_total;
  session_.begin_frames += last_.counters.begin_frames;
  session_.end_frames += last_.counters.end_frames;
  session_.nops += last_.counters.nops;
  return last_;
}

ZhaoExecutionResult ZhaoZrefShell::submit(const std::vector<uint8_t>& pkt,
                                          uint32_t slot_bytes) {
  return submit(pkt.data(), pkt.size(), slot_bytes);
}

void ZhaoZrefShell::reset() {
  session_ = ZhaoShellCounters{};
  last_ = ZhaoExecutionResult{};
}

}  // namespace zhao
