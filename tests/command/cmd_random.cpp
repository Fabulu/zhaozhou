// cmd_random.cpp — CMD.SCHEDULER random differential (plan W2.6).
//
// PCG claim/seal timelines driven into BOTH the Verilated RTL and the
// zref::CmdScheduler oracle through the identical per-cycle event stream
// (cmd_sim.hpp); every observable compared EVERY cycle (bit-exact):
//   - per-slot FSM states, fences (+1:1 with FPGA_RUNNING -> DONE via the
//     directed lane and the formal property), ring writes, fetch requests
//   - dispatch sinks (blit/rumble), mode register, D9 shadow snapshots
// - HPS model: lawful ring words (FREE -> ARM_WRITING -> READY, folded with
//   the FPGA's DONE/FREE writes), random seal timing across the 3 slots
// - verdict injection at random latency, random status (incl. OK)
// - record bursts with small BeginFrame deadlines (both deadline paths),
//   mode switches, blit/rumble dispatch, unknown opcodes
// - ticks at random intervals with random `repeated`, frame_complete for
//   right/wrong slots — the adversarial fence-exactly-once corner
// - run-twice transcript hash determinism (plan R1)
//
// Modes: default 1,000 frames (fast); `--frames 100000` (nightly soak).

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "zhao_sim.hpp"

#include "cmd_sim.hpp"

using zhao::check;
using zhao_abi::ZHAO_OP_BEGIN_FRAME;
using zhao_abi::ZHAO_OP_DEBUG_FRAME_BLIT;
using zhao_abi::ZHAO_OP_DEBUG_RUMBLE;
using zhao_abi::ZHAO_OP_NOP;
using zhao_abi::ZHAO_OP_SET_PRESENTATION_CONTRACT;
using zref::RingWord;
using zref::SchedEvents;
using zref::SlotState;

// ---- PCG32 (Melissa O'Neill) — the wave-1 PCG lane convention --------------
struct Pcg32 {
  uint64_t state;
  uint64_t inc;
  uint32_t next() {
    const uint64_t old = state;
    state = old * 6364136223846793005ull + inc;
    const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18) ^ old) >> 27);
    const uint32_t rot = static_cast<uint32_t>(old >> 59);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
  }
  uint32_t operator()(uint32_t bound) { return next() % bound; }
  uint32_t bit() { return next() & 1u; }
};

namespace {

struct PendingVerdict {
  bool valid = false;
  int cycles_left = 0;
  uint8_t slot = 0;
  uint8_t status = 0;
};

// one full random timeline; transcript hash + accumulated check failures
uint64_t runTimeline(uint32_t frames, uint64_t seed) {
  zhao_cmd::RtlSchedDev rtl;
  zhao_cmd::OracleSchedDev orc;
  Pcg32 rng{seed, (seed << 1) | 1u};

  RingWord word[3] = {RingWord::Free, RingWord::Free, RingWord::Free};
  uint32_t len[3] = {0, 0, 0};
  int arm_delay[3] = {0, 0, 0};  // cycles until word advances FREE->ARM->READY
  int fetch_pending = 0;         // cycles until a pending fetch_req is offered
  PendingVerdict verdict;
  uint64_t fences_rtl = 0, fences_orc = 0;
  uint64_t run2done = 0;

  uint32_t next_tick = 20 + rng(50);
  uint32_t frame_id = 0;
  uint64_t cycle = 0;
  uint64_t hash = 1469598103934665603ull;  // FNV-1a 64
  auto mix = [&hash](uint64_t v) {
    for (int b = 0; b < 8; ++b) {
      hash ^= (v >> (b * 8)) & 0xFF;
      hash *= 1099511628211ull;
    }
  };

  // per-frame record program (built at the frame boundary)
  std::vector<uint16_t> opcodes;
  std::vector<uint32_t> w0s, w1s, w2s, w3s;
  size_t rec_idx = 0;
  int recs_left = 0;

  for (uint32_t f = 0; f < frames; ++f) {
    // build this frame's record program
    opcodes.clear();
    w0s.clear();
    w1s.clear();
    w2s.clear();
    w3s.clear();
    const int n_rec = static_cast<int>(rng(6));
    opcodes.push_back(ZHAO_OP_BEGIN_FRAME);  // frame opens with BeginFrame
    w0s.push_back(f);
    w1s.push_back(rng(3));
    w2s.push_back(0);
    w3s.push_back(20 + rng(90));  // small deadline: both paths hit
    for (int r = 0; r < n_rec; ++r) {
      const uint32_t k = rng(6);
      if (k == 0) {
        opcodes.push_back(ZHAO_OP_SET_PRESENTATION_CONTRACT);
        w0s.push_back(rng(3));
        w1s.push_back(0);
        w2s.push_back(0);
        w3s.push_back(0);
      } else if (k == 1) {
        opcodes.push_back(ZHAO_OP_DEBUG_FRAME_BLIT);
        w0s.push_back((rng(3) << 8) | rng(2));
        w1s.push_back(rng.next());
        w2s.push_back(rng.next());
        w3s.push_back(rng.next());
      } else if (k == 2) {
        opcodes.push_back(ZHAO_OP_DEBUG_RUMBLE);
        w0s.push_back(rng.next());
        w1s.push_back(0);
        w2s.push_back(0);
        w3s.push_back(0);
      } else if (k == 3) {
        opcodes.push_back(ZHAO_OP_NOP);
        w0s.push_back(0);
        w1s.push_back(0);
        w2s.push_back(0);
        w3s.push_back(0);
      } else {
        opcodes.push_back(static_cast<uint16_t>(rng.next()));  // unknown/etc
        w0s.push_back(rng.next());
        w1s.push_back(rng.next());
        w2s.push_back(rng.next());
        w3s.push_back(rng.next());
      }
    }
    rec_idx = 0;
    recs_left = static_cast<int>(opcodes.size());

    const uint32_t tick_at = next_tick;
    next_tick += 40 + rng(60);

    while (cycle < tick_at) {
      SchedEvents e;
      for (int s = 0; s < 3; ++s) {
        e.hps_state[s] = word[s];
        e.hps_byte_len[s] = len[s];
      }
      // lawful HPS word advance: FREE -> (delay) ARM -> READY
      for (int s = 0; s < 3; ++s) {
        if (word[s] == RingWord::Free && arm_delay[s] > 0) {
          if (--arm_delay[s] == 0) {
            word[s] = RingWord::ArmWriting;
            arm_delay[s] = 1 + static_cast<int>(rng(4));
            len[s] = 40 + rng(200);
          }
        } else if (word[s] == RingWord::ArmWriting && arm_delay[s] > 0) {
          if (--arm_delay[s] == 0) word[s] = RingWord::Ready;
        } else if (word[s] == RingWord::Free && arm_delay[s] == 0 && rng(16) == 0) {
          arm_delay[s] = 1 + static_cast<int>(rng(20));
        }
      }
      // verdict injection
      if (verdict.valid) {
        if (--verdict.cycles_left == 0) {
          e.dma_done = true;
          e.dma_slot = verdict.slot;
          e.dma_status = verdict.status;
          verdict.valid = false;
        }
      }
      // records: one per cycle while programmed
      if (recs_left > 0 && rng(3) != 0) {
        e.rec_valid = true;
        e.opcode = opcodes[rec_idx];
        e.w0 = w0s[rec_idx];
        e.w1 = w1s[rec_idx];
        e.w2 = w2s[rec_idx];
        e.w3 = w3s[rec_idx];
        ++rec_idx;
        --recs_left;
      }
      e.blit_ready = rng(8) != 0;
      e.ring_wr_ready = rng(12) != 0;

      const bool tick = (cycle + 1 == tick_at);
      if (tick) {
        e.tick = true;
        ++frame_id;
        e.frame_id = frame_id;
        e.repeated = rng(4) == 0;
        // frame_complete: the running slot (when the verdict was OK) usually
        e.frame_complete = rng(6) != 0;
        e.frame_complete_slot = static_cast<uint8_t>(rng(3));
      }

      // pre-edge state snapshot for the RUN->DONE differential count
      zref::SlotState pre_rtl[3];
      for (int s = 0; s < 3; ++s)
        pre_rtl[s] = static_cast<zref::SlotState>(rtl.top_->slot_state_o[s]);

      const zref::SchedObs a = rtl.cycle(e);
      const zref::SchedObs b = orc.cycle(e);
      const char* where = nullptr;
      if (!zhao_cmd::obsEqual(a, b, &where)) {
        char what[160];
        std::snprintf(
            what, sizeof(what),
            "random: RTL != oracle (%s) @%llu [mode r=%u o=%u pend_o=%u rec=%u op=%04x w0=%08x]",
            where, static_cast<unsigned long long>(cycle), static_cast<unsigned>(a.mode),
            static_cast<unsigned>(b.mode), static_cast<unsigned>(b.mode_pending),
            e.rec_valid ? 1u : 0u, static_cast<unsigned>(e.opcode), e.w0);
        check(false, what, 1, 0);
      }
      // fence bookkeeping + law
      if (a.fence) {
        ++fences_rtl;
        check(a.state[a.fence_slot] == SlotState::Done, "random: fence rides a DONE slot", 1,
              static_cast<uint64_t>(a.state[a.fence_slot]));
      }
      if (b.fence) ++fences_orc;
      for (int s = 0; s < 3; ++s) {
        if (pre_rtl[s] == SlotState::FpgaRunning && a.state[s] == SlotState::Done) {
          ++run2done;
        }
      }
      int running = 0;
      for (int s = 0; s < 3; ++s) running += (a.state[s] == SlotState::FpgaRunning);
      check(running <= 1, "random: at most one FPGA_RUNNING slot", 1, running);
      // fold ring writes into the HPS word model
      if (a.ring_wr && e.ring_wr_ready) {
        word[a.ring_wr_slot] = static_cast<RingWord>(a.ring_wr_state);
      }
      // offer the pending fetch (schedule its verdict)
      if (a.fetch_req && fetch_pending == 0) {
        fetch_pending = 1 + static_cast<int>(rng(20));
      }
      if (fetch_pending > 0 && !verdict.valid) {
        --fetch_pending;
        if (fetch_pending == 0) {
          verdict.valid = true;
          verdict.cycles_left = 1 + static_cast<int>(rng(25));
          verdict.slot = a.fetch_req ? a.fetch_slot : 0;
          verdict.status = (rng(5) == 0) ? static_cast<uint8_t>(rng(14) + 1) : 0;
        }
      }
      // transcript
      for (int s = 0; s < 3; ++s) mix(static_cast<uint64_t>(a.state[s]));
      mix(a.fence);
      if (a.fence) mix((a.fence_slot << 9) | (a.fence_ok << 8) | a.fence_status);
      mix((static_cast<uint64_t>(a.mode) << 32) | a.shadow_cmds);
      ++cycle;
    }
  }
  check(fences_rtl == fences_orc, "random: fence count RTL == oracle", fences_orc, fences_rtl);
  check(fences_rtl == run2done, "random: fences == FPGA_RUNNING->DONE count", run2done, fences_rtl);
  return hash;
}

}  // namespace

int main(int argc, char** argv) {
  uint32_t frames = 1000;
  uint64_t seed = 0x5A0C0D0620260815ull;  // frozen: run-twice determinism
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      frames = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = std::strtoull(argv[++i], nullptr, 0);
    }
  }

  const uint64_t hash1 = runTimeline(frames, seed);
  const uint64_t hash2 = runTimeline(frames, seed);  // plan R1: run twice
  check(hash1 == hash2, "run-twice transcript hash identical", hash1, hash2);
  std::printf("cmd_random: %u frames, seed 0x%016llx, transcript hash 0x%016llx\n", frames,
              static_cast<unsigned long long>(seed), static_cast<unsigned long long>(hash1));

  return zhao::report_and_exit("cmd_random");
}
