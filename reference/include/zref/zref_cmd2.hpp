// zref_cmd2.hpp — ZRef oracles for the W2.6 command path + debug blocks
// (plan RUN-20260814-2154 W2.6, decisions D8/D9/D10).
//
// Law (in citation order):
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md 7.4 — the frame-slot ownership
//       FSM FREE -> ARM_WRITING -> READY -> FPGA_RUNNING -> DONE -> FREE
//       (state names are law; one owner per slot, forward-only cycle)
//   design/contracts/CMD.SCHEDULER.md        — deadline vs
//       BeginFrame.deadline_cycles (default = mode frame period), exactly
//       one completion fence per FPGA_RUNNING -> DONE, Phase-2 dispatch
//       sinks (blit DMA, counter snapshot, rumble passthrough)
//   design/contracts/CMD.DMA.md              — the verdict this scheduler
//       consumes (status codes, safe-error handling)
//   spec/counters.md                         — D9: counter_id = catalog
//       index, distributed shadows latched at frame_tick, u64 saturating
//   spec/memory_rules.md 4.1                 — FRAME_RING descriptor law
//       (word values 0/1/2/3, HPS owns FREE->ARM->READY, FPGA posts DONE
//       and returns DONE->FREE)
//   spec/video_rules.md 1 (D1/D6)            — mode frame periods: the
//       deadline default table (frozen in zhao_pkg ZHAO_TIMING)
//
// The CmdScheduler oracle is CYCLE-EXACT against
// fpga/rtl/command/zhao_cmd_scheduler.sv: one step() call models one gpu
// rising edge. The evaluation order inside step() (handshake clears ->
// per-slot transitions -> claim -> frame_tick globals -> record dispatch ->
// ring-write drain) mirrors the RTL always_ff statement order one-for-one;
// the differential tests (tests/command/) compare every observable every
// cycle. Do not reorder either side alone.
//
// Status-code space (8-bit): values 0..14 are the generated
// zhao_abi_error codes (ABI law, shared with the frame validator). The
// scheduler/dma pair own 15..16 as MODULE-LOCAL extensions (documented in
// both RTL headers; never on the wire):
//   15  ZHAO_DMA_EPOCH_MISMATCH  slot resource epoch != current epoch
//   16  ZHAO_SCHED_DEADLINE_MISS frame missed its deadline / display window

#pragma once

#include "zhao_abi.h"  // generated (runtime/include): opcode constants

#include <cstdint>
#include <cstring>
#include <vector>

namespace zref {

// ---- status-code extensions (module-local, see header comment) -------------
constexpr uint8_t ZHAO_DMA_EPOCH_MISMATCH = 15;
constexpr uint8_t ZHAO_SCHED_DEADLINE_MISS = 16;

// ---- charter 7.4 slot states ------------------------------------------------
enum class SlotState : uint8_t {
  Free = 0,
  ArmWriting = 1,
  Ready = 2,
  FpgaRunning = 3,
  Done = 4,
};

inline const char* slotStateName(SlotState s) {
  switch (s) {
    case SlotState::Free: return "FREE";
    case SlotState::ArmWriting: return "ARM_WRITING";
    case SlotState::Ready: return "READY";
    case SlotState::FpgaRunning: return "FPGA_RUNNING";
    case SlotState::Done: return "DONE";
  }
  return "?";
}

// ---- HPS-owned ring word values (memory_rules.md 4.1) -----------------------
enum class RingWord : uint8_t { Free = 0, ArmWriting = 1, Ready = 2, Done = 3 };

// ===========================================================================
// CmdScheduler — the 3-slot FSM oracle
// ===========================================================================

/** Inputs sampled at one gpu rising edge (mirror of the RTL input ports). */
struct SchedEvents {
  // harness-as-HPS ring words + descriptor lengths
  RingWord hps_state[3] = {RingWord::Free, RingWord::Free, RingWord::Free};
  uint32_t hps_byte_len[3] = {0, 0, 0};
  // frame boundary (VIDEO.FRAMECTL)
  bool tick = false;
  uint32_t frame_id = 0;
  bool repeated = false;
  bool frame_complete = false;  // the displayed frame came from this slot
  uint8_t frame_complete_slot = 0;
  // CMD.DMA verdict for the claimed slot
  bool dma_done = false;
  uint8_t dma_slot = 0;
  uint8_t dma_status = 0;  // 0 = OK (zhao_abi_error / local extensions)
  // decoded record stream (post-verification, decoder-facing)
  bool rec_valid = false;
  uint16_t opcode = 0;
  uint32_t w0 = 0, w1 = 0, w2 = 0, w3 = 0;
  // sink readiness (combinational, sampled at the edge)
  bool blit_ready = true;
  bool ring_wr_ready = true;
  bool fetch_req_ready = true;
};

/** Registered outputs visible after the edge (mirror of the RTL outputs). */
struct SchedObs {
  SlotState state[3] = {SlotState::Free, SlotState::Free, SlotState::Free};
  bool fence = false;
  uint8_t fence_slot = 0;
  bool fence_ok = false;
  uint8_t fence_status = 0;
  bool ring_wr = false;
  uint8_t ring_wr_slot = 0;
  uint8_t ring_wr_state = 0;
  bool fetch_req = false;
  uint8_t fetch_slot = 0;
  uint32_t fetch_addr = 0;
  uint32_t fetch_byte_len = 0;
  uint32_t fetch_epoch = 0;
  bool blit_valid = false;
  uint8_t blit_dst_slot = 0;
  uint8_t blit_mode = 0;
  uint32_t blit_src = 0, blit_len = 0, blit_crc = 0;
  bool rumble_valid = false;
  uint8_t rumble_pad = 0, rumble_en = 0, rumble_str = 0;
  bool snap_valid = false;  // D9: shadow set latched this edge
  uint64_t shadow_cycles = 0, shadow_faults = 0, shadow_cmds = 0;
  uint64_t frame_cycles = 0, deadline_faults = 0, commands = 0;
  uint8_t mode = 0;        // active mode (latched at frame boundary, D6)
  uint8_t mode_pending = 0;
  uint32_t epoch = 0;
  bool rec_ready = true;  // combinational (pre-edge view)
};

class CmdScheduler {
 public:
  /** Mode frame periods in gpu cycles (spec/video_rules.md 1; the frozen
   *  zhao_pkg ZHAO_TIMING table — Z60 / Storm / Duo). */
  static constexpr uint32_t kFramePeriod[3] = {251520u, 217984u, 318592u};

  // Ring geometry (memory_rules.md 4.1): 4-KiB descriptor table, then 3 x
  // 1-MiB slot bodies (FRAME_SLOT_BYTES from the generated ABI).
  static constexpr uint32_t kDescTableBytes = 4096;
  static constexpr uint32_t kSlotBytes = 1048576;

  explicit CmdScheduler(uint32_t ring_base = 0) : ring_base_(ring_base) { reset(); }

  void reset() {
    for (int s = 0; s < 3; ++s) {
      state_[s] = SlotState::Free;
      dead_cnt_[s] = 0;
      dead_lim_[s] = 0;
      wr_pend_v_[s] = false;
      wr_pend_val_[s] = 0;
    }
    claimed_window_ = false;
    fetch_v_ = false;
    fetch_slot_ = 0;
    fetch_addr_ = 0;
    fetch_len_ = 0;
    fetch_epoch_ = 0;
    blit_v_ = false;
    blit_dst_ = blit_mode_ = 0;
    blit_src_ = blit_len_ = blit_crc_ = 0;
    rumble_v_ = false;
    rumble_pad_ = rumble_en_ = rumble_str_ = 0;
    mode_ = mode_pend_ = 0;  // VIDEO_Z60
    epoch_ = 0;
    fence_v_ = fence_ok_ = false;
    fence_slot_ = fence_status_ = 0;
    snap_v_ = false;
    sh_cycles_ = sh_faults_ = sh_cmds_ = 0;
    live_cycles_ = live_faults_ = live_cmds_ = 0;
    publish();
    obs_.rec_ready = true;
  }

  /** One gpu rising edge. Returns the post-edge observables. */
  const SchedObs& step(const SchedEvents& e) {
    // ---- pre-edge snapshot (the RTL reads register values, not new ones)
    SlotState pre[3];
    for (int s = 0; s < 3; ++s) pre[s] = state_[s];
    const bool fetch_busy_pre = fetch_v_;
    const bool blit_busy_pre = blit_v_;
    bool any_run_pre = false;
    int run_slot_pre = -1;
    for (int s = 0; s < 3; ++s) {
      if (pre[s] == SlotState::FpgaRunning) {
        any_run_pre = true;
        run_slot_pre = s;
      }
    }

    // combinational record acceptance (pre-edge blit occupancy)
    obs_.rec_ready = !(blit_busy_pre && !e.blit_ready);

    // ---- handshake drains (cleared first; may be re-set below) -----------
    if (fetch_busy_pre && e.fetch_req_ready) fetch_v_ = false;
    if (blit_busy_pre && e.blit_ready) blit_v_ = false;
    // ring-write drain: only the slot presented this cycle (lowest OLD
    // pending) and only when accepted — pendings set by the transitions
    // below first appear next cycle (held-until-accepted request).
    {
      int d = -1;
      for (int s = 0; s < 3; ++s) {
        if (wr_pend_v_[s] && d < 0) d = s;
      }
      if (d >= 0 && e.ring_wr_ready) wr_pend_v_[d] = false;
    }

    fence_v_ = false;
    snap_v_ = false;

    // ---- claim evaluation (before per-slot transitions: the claim picks
    //      its slot from the PRE-edge state; identical priority to RTL) ---
    int claim = -1;
    if (!any_run_pre && !claimed_window_ && !fetch_busy_pre) {
      for (int s = 0; s < 3 && claim < 0; ++s) {
        if (pre[s] == SlotState::Ready && e.hps_state[s] == RingWord::Ready) claim = s;
      }
    }

    // ---- per-slot state transitions (forward-only charter cycle) ---------
    for (int s = 0; s < 3; ++s) {
      bool to_done = false;
      bool done_ok = false;
      uint8_t done_status = 0;
      switch (pre[s]) {
        case SlotState::Free:
          if (e.hps_state[s] == RingWord::ArmWriting) state_[s] = SlotState::ArmWriting;
          break;
        case SlotState::ArmWriting:
          if (e.hps_state[s] == RingWord::Ready) state_[s] = SlotState::Ready;
          break;
        case SlotState::Ready:
          if (s == claim) {
            state_[s] = SlotState::FpgaRunning;
            dead_cnt_[s] = 0;
            dead_lim_[s] = kFramePeriod[mode_];  // default: mode frame period
          }
          break;
        case SlotState::FpgaRunning:
          if (e.dma_done && e.dma_slot == static_cast<uint8_t>(s) && e.dma_status != 0) {
            to_done = true;
            done_ok = false;
            done_status = e.dma_status;
          } else if (dead_cnt_[s] >= dead_lim_[s]) {
            to_done = true;
            done_ok = false;
            done_status = ZHAO_SCHED_DEADLINE_MISS;
          } else if (e.tick && e.frame_complete && e.frame_complete_slot == static_cast<uint8_t>(s)) {
            to_done = true;
            done_ok = true;
            done_status = 0;
          } else if (e.tick) {
            // frame boundary reached without this frame completing: it lost
            // its display window (repeat path) — fail-safe termination
            to_done = true;
            done_ok = false;
            done_status = ZHAO_SCHED_DEADLINE_MISS;
          } else {
            dead_cnt_[s] += 1;
          }
          break;
        case SlotState::Done:
          if (e.tick) {
            state_[s] = SlotState::Free;  // DONE -> FREE (FPGA-owned release)
            wr_pend_v_[s] = true;
            wr_pend_val_[s] = 0;  // ring word FREE
          }
          break;
      }
      if (to_done) {
        state_[s] = SlotState::Done;
        // exactly ONE fence per FPGA_RUNNING -> DONE (fault or success)
        fence_v_ = true;
        fence_slot_ = static_cast<uint8_t>(s);
        fence_ok_ = done_ok;
        fence_status_ = done_status;
        wr_pend_v_[s] = true;
        wr_pend_val_[s] = 3;  // ring word DONE
      }
    }

    // ---- the claim side effects (fetch request to CMD.DMA) ----------------
    if (claim >= 0) {
      claimed_window_ = true;
      fetch_v_ = true;
      fetch_slot_ = static_cast<uint8_t>(claim);
      fetch_addr_ = ring_base_ + kDescTableBytes + static_cast<uint32_t>(claim) * kSlotBytes;
      fetch_len_ = e.hps_byte_len[claim];
      fetch_epoch_ = epoch_;
    }

    // ---- frame_tick globals ------------------------------------------------
    if (e.tick) {
      live_cycles_ += 1;
      if (e.repeated) live_faults_ += 1;  // deadline_faults: repeat path only
      mode_ = mode_pend_;                 // mode latch at frame start (D6)
      claimed_window_ = false;
      rumble_v_ = false;  // INPUT.RUMBLE consumed the per-frame update
      sh_cycles_ = live_cycles_;
      sh_faults_ = live_faults_;
      sh_cmds_ = live_cmds_;
      snap_v_ = true;
    }

    // ---- record dispatch (after tick handling: a record on the tick cycle
    //      wins the dispatch registers) ---------------------------------------
    if (e.rec_valid && obs_.rec_ready) {
      live_cmds_ += 1;
      switch (e.opcode) {
        case zhao_abi::ZHAO_OP_BEGIN_FRAME:
          if (e.w3 != 0 && run_slot_pre >= 0) dead_lim_[run_slot_pre] = e.w3;
          epoch_ = e.w1;
          break;
        case zhao_abi::ZHAO_OP_SET_PRESENTATION_CONTRACT:
          mode_pend_ = e.w0 & 0xFF;
          break;
        case zhao_abi::ZHAO_OP_DEBUG_FRAME_BLIT:
          blit_v_ = true;
          blit_dst_ = e.w0 & 0xFF;
          blit_mode_ = (e.w0 >> 8) & 0xFF;
          blit_src_ = e.w1;
          blit_len_ = e.w2;
          blit_crc_ = e.w3;
          break;
        case zhao_abi::ZHAO_OP_DEBUG_RUMBLE:
          rumble_v_ = true;
          rumble_pad_ = e.w0 & 0xFF;
          rumble_en_ = (e.w0 >> 8) & 0xFF;
          rumble_str_ = (e.w0 >> 16) & 0xFF;
          break;
        default:
          break;  // engine sinks are no-op ports in Phase 2 (plan D8)
      }
    }

    // ---- ring-write presentation (combinational view of the post-edge
    //      pending set — mirrors the RTL output mux) -------------------------
    obs_.ring_wr = false;
    for (int s = 0; s < 3; ++s) {
      if (wr_pend_v_[s]) {
        obs_.ring_wr = true;
        obs_.ring_wr_slot = static_cast<uint8_t>(s);
        obs_.ring_wr_state = wr_pend_val_[s];
        break;
      }
    }

    publish();
    return obs_;
  }

  const SchedObs& obs() const { return obs_; }

 private:
  void publish() {
    for (int s = 0; s < 3; ++s) obs_.state[s] = state_[s];
    obs_.fence = fence_v_;
    obs_.fence_slot = fence_slot_;
    obs_.fence_ok = fence_ok_;
    obs_.fence_status = fence_status_;
    obs_.fetch_req = fetch_v_;
    obs_.fetch_slot = fetch_slot_;
    obs_.fetch_addr = fetch_addr_;
    obs_.fetch_byte_len = fetch_len_;
    obs_.fetch_epoch = fetch_epoch_;
    obs_.blit_valid = blit_v_;
    obs_.blit_dst_slot = blit_dst_;
    obs_.blit_mode = blit_mode_;
    obs_.blit_src = blit_src_;
    obs_.blit_len = blit_len_;
    obs_.blit_crc = blit_crc_;
    obs_.rumble_valid = rumble_v_;
    obs_.rumble_pad = rumble_pad_;
    obs_.rumble_en = rumble_en_;
    obs_.rumble_str = rumble_str_;
    obs_.snap_valid = snap_v_;
    obs_.shadow_cycles = sh_cycles_;
    obs_.shadow_faults = sh_faults_;
    obs_.shadow_cmds = sh_cmds_;
    obs_.frame_cycles = live_cycles_;
    obs_.deadline_faults = live_faults_;
    obs_.commands = live_cmds_;
    obs_.mode = mode_;
    obs_.mode_pending = mode_pend_;
    obs_.epoch = epoch_;
  }

  uint32_t ring_base_;
  SlotState state_[3];
  uint32_t dead_cnt_[3], dead_lim_[3];
  bool wr_pend_v_[3];
  uint8_t wr_pend_val_[3];
  bool claimed_window_;
  bool fetch_v_;
  uint8_t fetch_slot_;
  uint32_t fetch_addr_, fetch_len_, fetch_epoch_;
  bool blit_v_;
  uint8_t blit_dst_, blit_mode_;
  uint32_t blit_src_, blit_len_, blit_crc_;
  bool rumble_v_;
  uint8_t rumble_pad_, rumble_en_, rumble_str_;
  uint8_t mode_, mode_pend_;
  uint32_t epoch_;
  bool fence_v_, fence_ok_;
  uint8_t fence_slot_, fence_status_;
  bool snap_v_;
  uint64_t sh_cycles_, sh_faults_, sh_cmds_;
  uint64_t live_cycles_, live_faults_, live_cmds_;
  SchedObs obs_;
};

// ===========================================================================
// DebugCounters — D9 snapshot aggregation oracle (spec/counters.md)
// ===========================================================================

/** One read-window beat: (counter_id = catalog index, u64 shadow value). */
struct CounterBeat {
  uint16_t id;
  uint64_t value;
};

inline bool operator==(const CounterBeat& a, const CounterBeat& b) {
  return a.id == b.id && a.value == b.value;
}

/**
 * The DEBUG.COUNTERS aggregation oracle: providers register live shadow
 * values (each block owns its counters locally, D9); tick() is the
 * frame_tick latch; sweep() is the read-mux window — ascending counter_id
 * over the whole catalog, ownerless counters read 0. Reading never touches
 * the live values.
 *
 * Catalog size = design/blocks.yml counter_catalog (40 entries at wave 2).
 */
class DebugCounters {
 public:
  static constexpr uint16_t kCatalogIds = 40;

  void reset() {
    live_.assign(kCatalogIds, 0);
    shadow_.assign(kCatalogIds, 0);
    violation_ = false;
  }

  /** Provider presents its latched shadow (id = catalog index). An
   *  out-of-catalog id is a protocol violation: flagged, never silently
   *  mapped (contract "no silent fallback"). */
  void provide(uint16_t id, uint64_t value) {
    if (id >= kCatalogIds) {
      violation_ = true;
      return;
    }
    live_[id] = value;
  }

  /** frame_tick: latch the provider set into the stable shadow bank. */
  void tick() { shadow_ = live_; }

  /** The read-mux window: every catalog id ascending (ownerless = 0). */
  std::vector<CounterBeat> sweep() const {
    std::vector<CounterBeat> out;
    out.reserve(kCatalogIds);
    for (uint16_t id = 0; id < kCatalogIds; ++id) out.push_back({id, shadow_[id]});
    return out;
  }

  /** .zcap COUNTERS section body (capture_format.md 4.2): u32 count +
   *  count x {u16 counter_id; u16 rsv; u64 expected_value} (12 B each,
   *  little-endian), ascending id. */
  static std::vector<uint8_t> zcapSection(const std::vector<CounterBeat>& beats) {
    std::vector<uint8_t> b;
    auto put16 = [&b](uint16_t v) {
      for (int i = 0; i < 2; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
    };
    auto put32 = [&b](uint32_t v) {
      for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
    };
    auto put64 = [&b](uint64_t v) {
      for (int i = 0; i < 8; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
    };
    put32(static_cast<uint32_t>(beats.size()));
    for (const CounterBeat& bt : beats) {
      put16(bt.id);
      put16(0);  // rsv
      put64(bt.value);
    }
    return b;
  }

  bool violation() const { return violation_; }

 private:
  std::vector<uint64_t> live_;
  std::vector<uint64_t> shadow_;
  bool violation_ = false;
};

}  // namespace zref
