// cmd_sim.hpp — W2.6 shared test driver: the Verilated zhao_cmd_scheduler
// and the zref::CmdScheduler oracle behind ONE cycle() interface, so the
// directed and random tests drive both through the identical event stream
// and compare cycle-for-cycle (the W2.4 audio_dev.hpp pattern).
//
// Law: design/contracts/CMD.SCHEDULER.md, charter 7.4, spec/counters.md.
// Cycle contract: inputs are sampled at the gpu rising edge; registered
// outputs are read post-edge. rec_ready is combinational (pre-edge view)
// and is captured after the input settle, before the edge.

#pragma once

#include "Vzhao_cmd_scheduler.h"
#include "verilated.h"

#include "zref/zref_cmd2.hpp"

#include <cstdint>

namespace zhao_cmd {

// zhao_frame_tick_t wire word: {pulse[33], frame_id[32:1], repeated[0]}
inline uint64_t tickWord(bool pulse, uint32_t frame_id, bool repeated) {
  return (static_cast<uint64_t>(pulse ? 1u : 0u) << 33) | (static_cast<uint64_t>(frame_id) << 1) |
         static_cast<uint64_t>(repeated ? 1u : 0u);
}

// zhao_counter_snap_t wire words: {valid[80], counter_id[79:64], value[63:0]}
inline bool snapWordValid(uint32_t w2) { return (w2 & 0x10000u) != 0; }
inline uint16_t snapWordId(uint32_t w2) { return static_cast<uint16_t>(w2); }
inline uint64_t snapWordVal(uint32_t w0, uint32_t w1) {
  return static_cast<uint64_t>(w0) | (static_cast<uint64_t>(w1) << 32);
}

// ---- the Verilated scheduler ----------------------------------------------
class RtlSchedDev {
 public:
  RtlSchedDev() : top_(new Vzhao_cmd_scheduler) { reset(); }
  ~RtlSchedDev() {
    top_->final();
    delete top_;
  }
  RtlSchedDev(const RtlSchedDev&) = delete;
  RtlSchedDev& operator=(const RtlSchedDev&) = delete;

  void reset() {
    top_->rst_n = 0;
    park();
    top_->eval();
    for (int i = 0; i < 2; ++i) edge();
    top_->rst_n = 1;
    top_->eval();
  }

  // One gpu cycle under `e`; returns the post-edge observables.
  zref::SchedObs cycle(const zref::SchedEvents& e) {
    top_->hps_state_i[0] = static_cast<uint8_t>(e.hps_state[0]);
    top_->hps_state_i[1] = static_cast<uint8_t>(e.hps_state[1]);
    top_->hps_state_i[2] = static_cast<uint8_t>(e.hps_state[2]);
    top_->hps_byte_len_i[0] = e.hps_byte_len[0];
    top_->hps_byte_len_i[1] = e.hps_byte_len[1];
    top_->hps_byte_len_i[2] = e.hps_byte_len[2];
    top_->frame_tick_i = tickWord(e.tick, e.frame_id, e.repeated);
    top_->frame_complete_i = e.frame_complete ? 1 : 0;
    top_->frame_complete_slot_i = e.frame_complete_slot;
    top_->dma_done_i = e.dma_done ? 1 : 0;
    top_->dma_slot_i = e.dma_slot;
    top_->dma_status_i = e.dma_status;
    top_->rec_valid_i = e.rec_valid ? 1 : 0;
    top_->rec_opcode_i = e.opcode;
    top_->rec_w0_i = e.w0;
    top_->rec_w1_i = e.w1;
    top_->rec_w2_i = e.w2;
    top_->rec_w3_i = e.w3;
    top_->dpy_blit_ready_i = e.blit_ready ? 1 : 0;
    top_->ring_wr_ready_i = e.ring_wr_ready ? 1 : 0;
    top_->fetch_req_ready_i = e.fetch_req_ready ? 1 : 0;
    top_->eval();  // settle: combinational rec_ready is the pre-edge view
    zref::SchedObs o;
    o.rec_ready = top_->rec_ready_o != 0;
    edge();
    readObs(&o);
    return o;
  }

 private:
  void park() {
    for (int s = 0; s < 3; ++s) {
      top_->hps_state_i[s] = 0;
      top_->hps_byte_len_i[s] = 0;
    }
    top_->frame_tick_i = 0;
    top_->frame_complete_i = 0;
    top_->frame_complete_slot_i = 0;
    top_->dma_done_i = 0;
    top_->dma_slot_i = 0;
    top_->dma_status_i = 0;
    top_->rec_valid_i = 0;
    top_->rec_opcode_i = 0;
    top_->rec_w0_i = 0;
    top_->rec_w1_i = 0;
    top_->rec_w2_i = 0;
    top_->rec_w3_i = 0;
    top_->dpy_blit_ready_i = 1;
    top_->ring_wr_ready_i = 1;
    top_->fetch_req_ready_i = 1;
  }

  void edge() {
    top_->clk = 0;
    top_->eval();
    top_->clk = 1;
    top_->eval();
    top_->clk = 0;
    top_->eval();
  }

  void readObs(zref::SchedObs* o) {
    for (int s = 0; s < 3; ++s) {
      o->state[s] = static_cast<zref::SlotState>(top_->slot_state_o[s]);
    }
    o->fence = top_->fence_valid_o != 0;
    o->fence_slot = static_cast<uint8_t>(top_->fence_slot_o);
    o->fence_ok = top_->fence_ok_o != 0;
    o->fence_status = top_->fence_status_o;
    o->ring_wr = top_->ring_wr_valid_o != 0;
    o->ring_wr_slot = static_cast<uint8_t>(top_->ring_wr_slot_o);
    o->ring_wr_state = static_cast<uint8_t>(top_->ring_wr_state_o);
    o->fetch_req = top_->fetch_req_valid_o != 0;
    o->fetch_slot = static_cast<uint8_t>(top_->fetch_slot_o);
    o->fetch_addr = top_->fetch_addr_o;
    o->fetch_byte_len = top_->fetch_byte_len_o;
    o->fetch_epoch = top_->fetch_epoch_o;
    o->blit_valid = top_->dpy_blit_valid_o != 0;
    o->blit_dst_slot = top_->dpy_blit_dst_slot_o;
    o->blit_mode = top_->dpy_blit_mode_o;
    o->blit_src = top_->dpy_blit_src_o;
    o->blit_len = top_->dpy_blit_len_o;
    o->blit_crc = top_->dpy_blit_crc_o;
    o->rumble_valid = top_->dpy_rumble_valid_o != 0;
    o->rumble_pad = top_->dpy_rumble_pad_o;
    o->rumble_en = top_->dpy_rumble_en_o;
    o->rumble_str = top_->dpy_rumble_str_o;
    o->snap_valid = snapWordValid(top_->snap_cycles_o[2]);
    o->shadow_cycles = snapWordVal(top_->snap_cycles_o[0], top_->snap_cycles_o[1]);
    o->shadow_faults = snapWordVal(top_->snap_faults_o[0], top_->snap_faults_o[1]);
    o->shadow_cmds = snapWordVal(top_->snap_cmds_o[0], top_->snap_cmds_o[1]);
    o->mode = static_cast<uint8_t>(top_->mode_o);
  }

 public:
  Vzhao_cmd_scheduler* top_;
};

// ---- the oracle behind the identical interface -----------------------------
class OracleSchedDev {
 public:
  explicit OracleSchedDev(uint32_t ring_base = 0u) : o_(ring_base) { reset(); }
  void reset() { o_.reset(); }
  zref::SchedObs cycle(const zref::SchedEvents& e) { return o_.step(e); }
  zref::CmdScheduler o_;
};

// ---- bit-exact observable compare ------------------------------------------
inline bool obsEqual(const zref::SchedObs& a, const zref::SchedObs& b, const char** where) {
  struct Cmp {
    const char* name;
    bool ok;
  };
  const Cmp checks[] = {
      {"state[0]", a.state[0] == b.state[0]},
      {"state[1]", a.state[1] == b.state[1]},
      {"state[2]", a.state[2] == b.state[2]},
      {"fence", a.fence == b.fence},
      {"fence_slot", !a.fence || a.fence_slot == b.fence_slot},
      {"fence_ok", !a.fence || a.fence_ok == b.fence_ok},
      {"fence_status", !a.fence || a.fence_status == b.fence_status},
      {"ring_wr", a.ring_wr == b.ring_wr},
      {"ring_wr_slot", !a.ring_wr || a.ring_wr_slot == b.ring_wr_slot},
      {"ring_wr_state", !a.ring_wr || a.ring_wr_state == b.ring_wr_state},
      {"fetch_req", a.fetch_req == b.fetch_req},
      {"fetch_slot", !a.fetch_req || a.fetch_slot == b.fetch_slot},
      {"fetch_addr", !a.fetch_req || a.fetch_addr == b.fetch_addr},
      {"fetch_byte_len", !a.fetch_req || a.fetch_byte_len == b.fetch_byte_len},
      {"fetch_epoch", !a.fetch_req || a.fetch_epoch == b.fetch_epoch},
      {"blit_valid", a.blit_valid == b.blit_valid},
      {"blit fields", !a.blit_valid || (a.blit_dst_slot == b.blit_dst_slot &&
                                        a.blit_mode == b.blit_mode && a.blit_src == b.blit_src &&
                                        a.blit_len == b.blit_len && a.blit_crc == b.blit_crc)},
      {"rumble_valid", a.rumble_valid == b.rumble_valid},
      {"rumble fields",
       !a.rumble_valid || (a.rumble_pad == b.rumble_pad && a.rumble_en == b.rumble_en &&
                           a.rumble_str == b.rumble_str)},
      {"snap_valid", a.snap_valid == b.snap_valid},
      {"shadow_cycles", !a.snap_valid || a.shadow_cycles == b.shadow_cycles},
      {"shadow_faults", !a.snap_valid || a.shadow_faults == b.shadow_faults},
      {"shadow_cmds", !a.snap_valid || a.shadow_cmds == b.shadow_cmds},
      {"mode", a.mode == b.mode},
      {"rec_ready", a.rec_ready == b.rec_ready},
  };
  for (const Cmp& c : checks) {
    if (!c.ok) {
      *where = c.name;
      return false;
    }
  }
  return true;
}

}  // namespace zhao_cmd
