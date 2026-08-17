// cmd_scheduler_directed.cpp — CMD.SCHEDULER directed vectors (plan W2.6 /
// design/contracts/CMD.SCHEDULER.md "Directed tests").
//
// Every scenario drives the Verilated RTL AND the zref::CmdScheduler oracle
// through the identical event stream (cmd_sim.hpp) and asserts BOTH the
// law (charter 7.4 / D8) and bit-equality with the oracle:
//   1. reset-idle: every slot FREE, no fence, no writes
//   2. happy path: seal -> claim -> fetch request (address law) -> DMA OK
//      verdict -> BeginFrame/SPC/blit/rumble dispatch -> frame_complete ->
//      ONE success fence -> DONE word -> DONE->FREE at the next tick
//   3. late seal: the slot never reaches READY before the boundary —
//      repeat frame, NO fence, deadline_faults++
//   4. CRC-corrupt header verdict: DMA error -> immediate fault fence,
//      safe error status, slot released; ZERO records dispatched
//   5. deadline miss: BeginFrame deadline_cycles=10 -> fault at the limit
//   6. fence-exactly-once under adversarial timings: wrong-slot
//      frame_completes, duplicate verdicts, tick storms
//   7. claim window: three sealed slots — one RUN at a time, next claim
//      only after the next frame boundary
//
// Ring model: the harness-as-HPS OWNS the word register (memory_rules.md
// 4.1): it holds ARM_WRITING/READY until the FPGA's DONE/FREE write lands
// (observed on ring_wr and folded back into the model), then it may re-arm.

#include <cstdint>
#include <cstdio>

#include "zhao_sim.hpp"

#include "cmd_sim.hpp"

using zhao::check;
using zhao_abi::ZHAO_OP_BEGIN_FRAME;
using zhao_abi::ZHAO_OP_DEBUG_FRAME_BLIT;
using zhao_abi::ZHAO_OP_DEBUG_RUMBLE;
using zhao_abi::ZHAO_OP_END_FRAME;
using zhao_abi::ZHAO_OP_SET_PRESENTATION_CONTRACT;
using zref::RingWord;
using zref::SchedEvents;
using zref::SlotState;

namespace {

// harness-as-HPS: the persistent ring word state + the dual drive loop
class Bench {
 public:
  Bench() {
    for (int s = 0; s < 3; ++s) {
      word[s] = RingWord::Free;
      len[s] = 0;
    }
  }

  SchedEvents base() const {
    SchedEvents e;
    for (int s = 0; s < 3; ++s) {
      e.hps_state[s] = word[s];
      e.hps_byte_len[s] = len[s];
    }
    return e;
  }

  // drive both devices one cycle; law invariants + oracle equality
  zref::SchedObs dual(const SchedEvents& e) {
    const zref::SchedObs a = rtl.cycle(e);
    const zref::SchedObs b = orc.cycle(e);
    const char* where = nullptr;
    if (!zhao_cmd::obsEqual(a, b, &where)) {
      char what[96];
      std::snprintf(what, sizeof(what), "directed: RTL != oracle (%s)", where);
      check(false, what, 1, 0);
    }
    int running = 0;
    for (int s = 0; s < 3; ++s) running += (a.state[s] == SlotState::FpgaRunning);
    check(running <= 1, "directed: at most one FPGA_RUNNING slot", 1, running);
    if (a.fence) {
      ++fences;
      ok_fence_seen |= a.fence_ok;
      check(a.state[a.fence_slot] == SlotState::Done, "directed: fence rides a DONE slot", 1,
            static_cast<uint64_t>(a.state[a.fence_slot]));
    }
    // fold the FPGA's ring write back into the HPS word model
    if (a.ring_wr && e.ring_wr_ready) {
      word[a.ring_wr_slot] = static_cast<RingWord>(a.ring_wr_state);
    }
    return a;
  }

  zref::SchedObs idle(int n = 1) {
    zref::SchedObs a;
    for (int i = 0; i < n; ++i) a = dual(base());
    return a;
  }

  void seal(int s, uint32_t byte_len) {
    len[s] = byte_len;
    word[s] = RingWord::ArmWriting;
    dual(base());
    word[s] = RingWord::Ready;
    dual(base());
  }

  // run idle cycles until slot s is FPGA_RUNNING (claim window law)
  zref::SchedObs claimAndWait(int s) {
    zref::SchedObs a = idle(1);
    int guard = 0;
    while (a.state[s] != SlotState::FpgaRunning && guard++ < 8) a = idle(1);
    check(a.state[s] == SlotState::FpgaRunning, "directed: claim fired", 3,
          static_cast<uint64_t>(a.state[s]));
    return a;
  }

  RingWord word[3];
  uint32_t len[3];
  uint64_t fences = 0;
  bool ok_fence_seen = false;
  zhao_cmd::RtlSchedDev rtl;
  zhao_cmd::OracleSchedDev orc;
};

}  // namespace

int main() {
  // ---- 1. reset-idle -------------------------------------------------------
  {
    Bench t;
    const zref::SchedObs a = t.idle();
    for (int s = 0; s < 3; ++s) {
      check(a.state[s] == SlotState::Free, "reset: slot FREE", 0,
            static_cast<uint64_t>(a.state[s]));
    }
    check(!a.fence, "reset: no fence", 0, a.fence);
    check(!a.ring_wr, "reset: no ring write", 0, a.ring_wr);
    check(a.mode == 0, "reset: mode VIDEO_Z60", 0, a.mode);
  }

  // ---- 2. happy path -------------------------------------------------------
  {
    Bench t;
    t.seal(0, 200);
    zref::SchedObs a = t.claimAndWait(0);
    check(a.fetch_req, "happy: fetch request issued", 1, a.fetch_req);
    check(a.fetch_slot == 0, "happy: fetch slot 0", 0, a.fetch_slot);
    check(a.fetch_addr == 4096u, "happy: fetch addr = base+table", 4096, a.fetch_addr);
    check(a.fetch_byte_len == 200, "happy: fetch len = descriptor len", 200, a.fetch_byte_len);
    check(a.fetch_epoch == 0, "happy: fetch epoch = current epoch", 0, a.fetch_epoch);

    {
      SchedEvents e = t.base();
      e.dma_done = true;
      e.dma_slot = 0;
      a = t.dual(e);
    }
    check(a.state[0] == SlotState::FpgaRunning, "happy: still running after OK", 3,
          static_cast<uint64_t>(a.state[0]));

    {
      SchedEvents e = t.base();
      e.rec_valid = true;
      e.opcode = ZHAO_OP_BEGIN_FRAME;
      e.w0 = 7;   // frame_id
      e.w1 = 3;   // resource_epoch (latched for the NEXT claim's check)
      e.w3 = 80;  // deadline_cycles override
      a = t.dual(e);

      e.opcode = ZHAO_OP_SET_PRESENTATION_CONTRACT;
      e.w0 = 1;  // VIDEO_STORM
      a = t.dual(e);
      check(a.mode == 0, "happy: mode NOT switched mid-frame (D6)", 0, a.mode);

      e.opcode = ZHAO_OP_DEBUG_FRAME_BLIT;
      e.w0 = (2u << 8) | 1u;  // mode byte Duo @15:8, dst_slot 1
      e.w1 = 0xDEADBEEFu;     // src
      e.w2 = 245760;          // byte_len
      e.w3 = 0x12345678u;     // expected crc
      a = t.dual(e);
      check(a.blit_valid, "happy: blit dispatched", 1, a.blit_valid);
      check(a.blit_dst_slot == 1 && a.blit_mode == 2 && a.blit_src == 0xDEADBEEFu &&
                a.blit_len == 245760 && a.blit_crc == 0x12345678u,
            "happy: blit fields verbatim", 1, a.blit_src);

      e.opcode = ZHAO_OP_DEBUG_RUMBLE;
      e.w0 = (77u << 16) | (1u << 8) | 2u;  // str, en, pad
      a = t.dual(e);
      check(a.rumble_valid && a.rumble_pad == 2 && a.rumble_en == 1 && a.rumble_str == 77,
            "happy: rumble dispatched", 1, a.rumble_pad);

      e.opcode = ZHAO_OP_END_FRAME;
      e.w0 = e.w1 = e.w2 = e.w3 = 0;
      t.dual(e);
    }

    // blit held until the sink accepts (backpressure law): re-issue the blit
    // with the sink stalled, hold, then release
    {
      SchedEvents e = t.base();
      e.blit_ready = false;
      e.rec_valid = true;
      e.opcode = ZHAO_OP_DEBUG_FRAME_BLIT;
      e.w0 = (2u << 8) | 1u;
      e.w1 = 0xDEADBEEFu;
      e.w2 = 245760;
      e.w3 = 0x12345678u;
      zref::SchedObs h = t.dual(e);  // dispatched into a stalled sink
      check(h.blit_valid, "happy: blit dispatched under !ready", 1, h.blit_valid);
      SchedEvents hold = t.base();
      hold.blit_ready = false;
      h = t.dual(hold);
      check(h.blit_valid, "happy: blit held under !ready", 1, h.blit_valid);
      h = t.idle(1);  // sink ready again -> accept
      check(!h.blit_valid, "happy: blit drained after accept", 0, h.blit_valid);
    }

    // frame boundary: frame_complete for slot 0 -> THE success fence
    {
      SchedEvents e = t.base();
      e.tick = true;
      e.frame_id = 7;
      e.frame_complete = true;
      e.frame_complete_slot = 0;
      a = t.dual(e);
      check(a.fence, "happy: completion fence", 1, a.fence);
      check(a.fence_ok, "happy: fence is a success fence", 1, a.fence_ok);
      check(a.state[0] == SlotState::Done, "happy: slot DONE", 4,
            static_cast<uint64_t>(a.state[0]));
      check(a.ring_wr, "happy: DONE word posted", 1, a.ring_wr);
      check(a.ring_wr_state == 3, "happy: ring word = DONE", 3, a.ring_wr_state);
      check(a.mode == 1, "happy: mode switched at the boundary", 1, a.mode);
      check(a.shadow_cycles == 1, "happy: frame_cycles shadow", 1, a.shadow_cycles);
      check(a.shadow_cmds == 6, "happy: commands shadow = 6 records", 6, a.shadow_cmds);
      check(a.snap_valid, "happy: D9 snapshot pulse", 1, a.snap_valid);

      t.idle(3);
      SchedEvents e2 = t.base();
      e2.tick = true;
      e2.frame_id = 8;
      a = t.dual(e2);
      check(a.state[0] == SlotState::Free, "happy: DONE -> FREE", 0,
            static_cast<uint64_t>(a.state[0]));
      check(t.word[0] == RingWord::Free, "happy: HPS word folded to FREE", 0,
            static_cast<uint64_t>(t.word[0]));
    }
    t.idle(2);
    check(t.fences == 1, "happy: EXACTLY one fence", 1, t.fences);
    check(t.ok_fence_seen, "happy: the one fence was a success", 1, t.ok_fence_seen);
  }

  // ---- 3. late seal: repeat, NO fence --------------------------------------
  {
    Bench t;
    t.word[1] = RingWord::ArmWriting;  // still writing at the boundary
    t.len[1] = 100;
    t.idle(1);
    SchedEvents e = t.base();
    e.tick = true;
    e.repeated = true;  // the video side repeated: 60 Hz law
    const zref::SchedObs a = t.dual(e);
    check(a.state[1] == SlotState::ArmWriting, "late: never claimed", 1,
          static_cast<uint64_t>(a.state[1]));
    check(!a.fence, "late: NO fence for the dead frame", 0, a.fence);
    check(a.shadow_faults == 1, "late: deadline_faults counted the repeat", 1, a.shadow_faults);
    t.idle(4);
    check(t.fences == 0, "late: zero fences total", 0, t.fences);
  }

  // ---- 4. CRC-corrupt header verdict: safe error, no dispatch --------------
  {
    Bench t;
    t.seal(2, 160);
    t.claimAndWait(2);
    SchedEvents e = t.base();
    e.dma_done = true;
    e.dma_slot = 2;
    e.dma_status = 5;  // ZH_ABI_BAD_HEADER_CRC (corrupt header)
    const zref::SchedObs a = t.dual(e);
    check(a.fence, "crc: fault fence posted", 1, a.fence);
    check(!a.fence_ok, "crc: fence is an error fence", 0, a.fence_ok);
    check(a.fence_status == 5, "crc: safe error status verbatim", 5, a.fence_status);
    check(a.state[2] == SlotState::Done, "crc: slot DONE-with-error", 4,
          static_cast<uint64_t>(a.state[2]));
    check(a.ring_wr && a.ring_wr_state == 3, "crc: DONE word posted", 3, a.ring_wr_state);
    check(!a.blit_valid && !a.rumble_valid, "crc: zero sink traffic", 0, a.blit_valid);
    t.idle(6);
    check(t.fences == 1, "crc: exactly one (fault) fence", 1, t.fences);
  }

  // ---- 5. deadline miss (BeginFrame override) -------------------------------
  {
    Bench t;
    t.seal(0, 120);
    t.claimAndWait(0);
    SchedEvents e = t.base();
    e.rec_valid = true;
    e.opcode = ZHAO_OP_BEGIN_FRAME;
    e.w3 = 10;  // tight deadline
    t.dual(e);
    zref::SchedObs a = t.idle(1);
    int cycles = 0;
    for (int i = 0; i < 30 && !a.fence; ++i) {
      a = t.idle(1);
      ++cycles;
    }
    check(a.fence, "deadline: fault fence", 1, a.fence);
    check(!a.fence_ok, "deadline: error fence", 0, a.fence_ok);
    check(a.fence_status == zref::ZHAO_SCHED_DEADLINE_MISS, "deadline: status = deadline-miss (16)",
          16, a.fence_status);
    check(cycles <= 12, "deadline: fault at the 10-cycle limit", 12, cycles);
  }

  // ---- 6. fence-exactly-once under adversarial timings ----------------------
  {
    Bench t;
    t.seal(0, 100);
    t.seal(1, 100);
    zref::SchedObs a = t.claimAndWait(0);
    check(a.fetch_slot == 0, "adv: lowest READY slot claimed first", 0, a.fetch_slot);

    // wrong-slot verdict (slot 1 not running): ignored, no state change
    {
      SchedEvents e = t.base();
      e.dma_done = true;
      e.dma_slot = 1;
      e.dma_status = 5;
      a = t.dual(e);
      check(a.state[0] == SlotState::FpgaRunning, "adv: wrong-slot verdict ignored", 3,
            static_cast<uint64_t>(a.state[0]));
    }
    // OK verdict then a duplicate: idempotent
    {
      SchedEvents e = t.base();
      e.dma_done = true;
      e.dma_slot = 0;
      e.dma_status = 0;
      t.dual(e);
      a = t.dual(e);
      check(a.state[0] == SlotState::FpgaRunning, "adv: duplicate OK verdict", 3,
            static_cast<uint64_t>(a.state[0]));
    }
    // frame_complete for the WRONG slot at the boundary: window miss
    {
      SchedEvents e = t.base();
      e.tick = true;
      e.repeated = true;
      e.frame_complete = true;
      e.frame_complete_slot = 1;
      a = t.dual(e);
      check(a.fence && !a.fence_ok, "adv: window miss = error fence", 1, a.fence_ok);
      check(a.fence_slot == 0, "adv: fence names the running slot", 0, a.fence_slot);
    }
    // slot 0 DONE -> FREE at the next boundary; slot 1 claimable after it
    {
      SchedEvents e = t.base();
      e.tick = true;
      t.dual(e);
    }
    a = t.claimAndWait(1);
    check(a.fetch_slot == 1, "adv: fetch names slot 1", 1, a.fetch_slot);
    {
      SchedEvents e = t.base();
      e.tick = true;
      e.frame_complete = true;
      e.frame_complete_slot = 1;
      t.dual(e);
    }
    t.idle(8);
    check(t.fences == 2, "adv: exactly two fences over the storm", 2, t.fences);
  }

  // ---- 7. claim window: one claim per frame boundary ------------------------
  {
    Bench t;
    t.seal(0, 100);
    t.seal(1, 100);
    t.seal(2, 100);
    t.claimAndWait(0);
    {
      SchedEvents e = t.base();
      e.tick = true;
      e.frame_complete = true;
      e.frame_complete_slot = 0;
      t.dual(e);
    }
    {
      SchedEvents e = t.base();
      e.tick = true;
      t.dual(e);
    }
    const zref::SchedObs a = t.claimAndWait(1);
    int running = 0;
    for (int s = 0; s < 3; ++s) running += (a.state[s] == SlotState::FpgaRunning);
    check(running == 1, "window: exactly one owner", 1, running);
  }

  return zhao::report_and_exit("cmd_scheduler_directed");
}
