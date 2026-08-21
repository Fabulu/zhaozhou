// cmd_dma_directed.cpp — CMD.DMA directed vectors (plan W2.6 /
// design/contracts/CMD.DMA.md "Directed tests").
//
// The harness C++ IS the HPS (plan D10): it hosts the HPS DDR image
// (sealed frame packets via the zref ZhaoFrameBuilder, blit source
// buffers) and answers bridge bursts with the frozen sim profile —
// 16 gpu cycles request->first beat, then 1 beat (8 B) per cycle, `last`
// on the final beat. Scenarios:
//   1. happy fetch: burst exchange geometry, verdict OK, the verified
//      byte stream == the sealed packet bit-exact, bytes/cmds consumed
//   2. corrupt header CRC: ONE burst only, ZERO bytes downstream,
//      status BAD_HEADER_CRC, bytes_consumed = 36
//   3. corrupt payload CRC: full fetch, verdict, ZERO bytes downstream
//   4. epoch mismatch: dropped before the first payload byte (status 15)
//   5. truncated descriptor (byte_len < 40+N): BAD_LENGTH, no stream
//   6. walk errors with INTACT CRCs: unknown opcode, count mismatch,
//      debug opcode without flags bit0
//   7. bridge error: safe drop, nothing issued downstream
//   (8. the blit engine was REMOVED from CMD.DMA in step 6 of the
//       DEBUG.FRAMEBLIT integration. The blit is DEBUG.FRAMEBLIT's now,
//       and its tests are tests/debug/debug_frameblit_directed.cpp. This
//       module has no MEM.GUARD client at all any more.)
//      before the first byte

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"
#include "Vzhao_cmd_dma.h"

#include "zhao_abi.h"  // generated: ZHAO_ABI_VERSION
#include "zhao_sim.hpp"
#include "zref/zref.hpp"
#include "zref/zref_cmd2.hpp"
#include "zref/zref_frame.hpp"

using zhao::check;

namespace {

// ---- packed-port helpers ----------------------------------------------------
// zhao_hps_burst_req_t {valid,write,client[2:0],addr[31:0],len[6:0]} = 44 b
struct ReqView {
  bool valid;
  bool write;
  uint8_t client;
  uint32_t addr;
  uint8_t len;
};

ReqView reqView(uint64_t w) {
  ReqView r;
  r.valid = (w >> 43) & 1;
  r.write = (w >> 42) & 1;
  r.client = (w >> 39) & 7;
  r.addr = static_cast<uint32_t>((w >> 7) & 0xFFFFFFFFull);
  r.len = static_cast<uint8_t>(w & 0x7F);
  return r;
}

// zhao_hps_burst_rsp_t {beat_valid,data[63:0],last,err} = 67 b -> 3 words:
// word0 = {data[29:0], last, err}, word1 = data[61:30],
// word2 = {beat_valid(bit2), data[63:62](bits1:0)}
void setRsp(uint32_t* w, bool beat_valid, uint64_t data, bool last, bool err) {
  const uint64_t sh = (data << 2) & 0xFFFFFFFFFFFFFFFFull;
  w[0] = static_cast<uint32_t>(sh & 0xFFFFFFFFull) | (last ? 2u : 0u) | (err ? 1u : 0u);
  w[1] = static_cast<uint32_t>((sh >> 32) & 0xFFFFFFFFull);
  w[2] = static_cast<uint32_t>((data >> 62) & 3ull) | (beat_valid ? 4u : 0u);
}

// ---- the harness-as-HPS device ----------------------------------------------
constexpr uint32_t kRingBase = 0x0;
constexpr uint32_t kSlotBody0 = kRingBase + 4096;  // slot 0 body
constexpr int kFirstBeatLatency = 16;              // D10 sim profile

class DmaBench {
 public:
  DmaBench() : top_(new Vzhao_cmd_dma) { reset(); }
  ~DmaBench() {
    top_->final();
    delete top_;
  }
  DmaBench(const DmaBench&) = delete;
  DmaBench& operator=(const DmaBench&) = delete;

  void reset() {
    top_->rst_n = 0;
    park();
    top_->eval();
    for (int i = 0; i < 2; ++i) edge();
    top_->rst_n = 1;
    top_->eval();
  }

  bool dmaBusy() const { return top_->fetch_req_ready_o == 0; }

  void park() {
    top_->fetch_req_valid_i = 0;
    top_->fetch_slot_i = 0;
    top_->fetch_addr_i = 0;
    top_->fetch_byte_len_i = 0;
    top_->fetch_epoch_i = 0;
    top_->pkt_ready_i = 1;
    setRsp(&top_->hps_rsp_i[0], false, 0, false, false);
    top_->frame_tick_i = 0;
  }

  // one gpu cycle: the HPS responder runs before the edge
  void cycle() {
    top_->eval();
    // service the bridge (D10 profile)
    const ReqView r = reqView(top_->hps_req_o);
    if (!burst_active_) {
      if (r.valid) {
        burst_active_ = true;
        burst_beat_ = 0;
        burst_len_ = r.len;
        burst_addr_ = r.addr;
        latency_ = kFirstBeatLatency;
        bursts_.push_back(r);
      }
      setRsp(&top_->hps_rsp_i[0], false, 0, false, false);
    } else if (latency_ > 0) {
      --latency_;
      setRsp(&top_->hps_rsp_i[0], false, 0, false, false);
    } else {
      const uint32_t off = burst_addr_ + burst_beat_ * 8;
      uint64_t data = 0;
      for (int b = 7; b >= 0; --b) {
        data = (data << 8) | mem(off + static_cast<uint32_t>(b));
      }
      const bool last = (burst_beat_ + 1) * 8 >= burst_len_;
      setRsp(&top_->hps_rsp_i[0], true, data, last, force_err_ && burst_beat_ == 0);
      if (last) {
        burst_active_ = false;
      }
      ++burst_beat_;
    }
    // capture the verdict pulse + packet stream
    if (top_->dma_done_o) {
      verdicts_.push_back(Verdict{top_->dma_slot_o, top_->dma_status_o, top_->dma_bytes_consumed_o,
                                  top_->dma_cmds_consumed_o});
    }
    if (top_->pkt_valid_o && top_->pkt_ready_i) {
      pkt_bytes_.push_back(static_cast<uint8_t>(top_->pkt_byte_o));
    }
    edge();
  }

  void idle(int n) {
    for (int i = 0; i < n; ++i) cycle();
  }

  // ---- fetch helpers -------------------------------------------------------
  // mem(addr, n) <- bytes; run a fetch to completion (verdict collected)
  void load(uint32_t addr, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) mem(addr + i) = bytes[i];
  }

  void fetch(uint32_t addr, uint32_t byte_len, uint32_t epoch, int max_cycles = 200000) {
    // drain any previous verified stream first: the DMA accepts a fetch
    // only in IDLE, and a request issued mid-stream would be silently lost
    for (int i = 0; i < 70000 && (top_->pkt_valid_o || dmaBusy()); ++i) cycle();
    verdicts_.clear();
    pkt_bytes_.clear();
    bursts_.clear();
    top_->fetch_req_valid_i = 1;
    top_->fetch_slot_i = 0;
    top_->fetch_addr_i = addr;
    top_->fetch_byte_len_i = byte_len;
    top_->fetch_epoch_i = epoch;
    cycle();  // accepted (IDLE)
    top_->fetch_req_valid_i = 0;
    for (int i = 0; i < max_cycles && verdicts_.empty(); ++i) cycle();
  }

  uint8_t& mem(uint32_t a) {
    if (a >= mem_.size()) mem_.resize(a + 4096, 0);
    return mem_[a];
  }

  struct Verdict {
    uint8_t slot;
    uint8_t status;
    uint32_t bytes;
    uint32_t cmds;
  };
  Vzhao_cmd_dma* top_;
  std::vector<uint8_t> mem_ = std::vector<uint8_t>(1u << 20);
  std::vector<Verdict> verdicts_;
  std::vector<uint8_t> pkt_bytes_;
  std::vector<ReqView> bursts_;
  bool force_err_ = false;

 private:
  void edge() {
    top_->clk = 0;
    top_->eval();
    top_->clk = 1;
    top_->eval();
    top_->clk = 0;
    top_->eval();
  }

  bool burst_active_ = false;
  uint32_t burst_addr_ = 0;
  uint8_t burst_len_ = 0;
  int burst_beat_ = 0;
  int latency_ = 0;
};

// ---- packet crafting ---------------------------------------------------------
std::vector<uint8_t> makePacket(uint32_t frame_id, uint32_t epoch, uint32_t deadline,
                                uint16_t flags, const std::vector<std::vector<uint8_t>>& records) {
  // hand-built sealed packet (capture_format.md 3) so malformed-but-CRC-intact
  // variants are possible (the ZhaoFrameBuilder only emits lawful packets)
  uint32_t cb = 0;
  for (const auto& r : records) cb += static_cast<uint32_t>(r.size());
  std::vector<uint8_t> p(40 + cb, 0);
  auto put16 = [&](uint32_t off, uint16_t v) {
    p[off] = v & 0xFF;
    p[off + 1] = v >> 8;
  };
  auto put32 = [&](uint32_t off, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[off + i] = static_cast<uint8_t>(v >> (8 * i));
  };
  put32(0, 0x314B505Au);                 // magic
  put16(4, zhao_abi::ZHAO_ABI_VERSION);  // abi_version (track the wire, never a literal)
  put16(6, flags);
  put32(8, frame_id);
  put32(12, frame_id);  // sequence
  put32(16, epoch);
  put32(20, deadline);
  put32(24, static_cast<uint32_t>(records.size()));
  put32(28, cb);
  uint32_t off = 36;
  for (const auto& r : records) {
    std::memcpy(&p[off], r.data(), r.size());
    off += static_cast<uint32_t>(r.size());
  }
  const uint32_t hcrc = zhao_abi::zhao_crc32c(0, p.data(), 32);
  const uint32_t pcrc = zhao_abi::zhao_crc32c(0, p.data() + 36, cb);
  put32(32, hcrc);
  put32(36 + cb, pcrc);
  return p;
}

// record header + payload helper: opcode, record_bytes, payload words
std::vector<uint8_t> makeRecord(uint16_t opcode, uint16_t rb,
                                const std::vector<uint32_t>& payload) {
  std::vector<uint8_t> r(rb, 0);
  r[0] = opcode & 0xFF;
  r[1] = opcode >> 8;
  r[2] = rb & 0xFF;
  r[3] = rb >> 8;
  for (size_t i = 0; i < payload.size() && 16 + 4 * i + 4 <= rb; ++i) {
    for (int b = 0; b < 4; ++b) {
      r[16 + 4 * static_cast<uint32_t>(i) + static_cast<uint32_t>(b)] =
          static_cast<uint8_t>(payload[i] >> (8 * b));
    }
  }
  return r;
}

}  // namespace

// ---- random lane -------------------------------------------------------------
// PCG packet timelines: lawful sealed packets (zero-pad records), one
// deterministic corruption per packet from a family whose verdict the
// fail-safe order fixes in advance. The verdict, bytes_consumed and the
// gate (zero bytes downstream on any error) are asserted per packet.
namespace {

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
};

// The fail-safe order (capture_format.md 3.2) as the packet-level oracle:
// zref::CmdDma::verdict (reference/include/zref/zref_cmd2.hpp) — given the
// (possibly corrupted) bytes, the descriptor length and the current epoch,
// the verdict, bytes_consumed and walked-record count are fixed in advance.
// Record-semantic checks (6/7/8) are decoder-side (wave 3) and intentionally
// absent there too.
using zhao_abi::ZHAO_OP_BEGIN_FRAME;
using zhao_abi::ZHAO_OP_DEBUG_FRAME_BLIT;
using zhao_abi::ZHAO_OP_DEBUG_RUMBLE;
using zhao_abi::ZHAO_OP_END_FRAME;
using zhao_abi::ZHAO_OP_NOP;
using zhao_abi::ZHAO_OP_SET_PRESENTATION_CONTRACT;
int runRandom(uint32_t packets, uint64_t seed) {
  Pcg32 rng{seed, (seed << 1) | 1u};
  DmaBench t;
  for (uint32_t n = 0; n < packets; ++n) {
    // random lawful record mix (all payloads zero => pads zero on the wire)
    std::vector<std::vector<uint8_t>> recs;
    uint16_t flags = 0;
    const int nr = static_cast<int>(rng(6));
    recs.push_back(makeRecord(ZHAO_OP_BEGIN_FRAME, 32, {n, 0, 0, 0}));
    for (int r = 0; r < nr; ++r) {
      const uint32_t k = rng(5);
      if (k == 0) {
        recs.push_back(makeRecord(ZHAO_OP_NOP, 16, {}));
      } else if (k == 1) {
        recs.push_back(makeRecord(ZHAO_OP_END_FRAME, 32, {0, 0, 0, 0}));
      } else if (k == 2) {
        flags |= 1;  // debug umbrella needs bit0
        recs.push_back(makeRecord(ZHAO_OP_DEBUG_FRAME_BLIT, 48, {0, 0, 0, 0}));
      } else if (k == 3) {
        flags |= 1;
        recs.push_back(makeRecord(ZHAO_OP_DEBUG_RUMBLE, 32, {0, 0, 0, 0}));
      } else {
        recs.push_back(makeRecord(ZHAO_OP_SET_PRESENTATION_CONTRACT, 48, {rng(3), 0, 0, 0}));
      }
    }
    std::vector<uint8_t> pkt = makePacket(n, 0, 20, flags, recs);
    const uint32_t cb = pkt.size() - 40;
    // one deterministic corruption per packet (the ladder oracle above
    // recomputes the verdict from the corrupted bytes — no hand-prediction)
    uint32_t fetch_epoch = 0;
    const uint32_t corrupt = rng(6);
    if (corrupt == 1) {
      pkt[rng(32)] ^= static_cast<uint8_t>(1 + rng(255));  // header window
    } else if (corrupt == 2 && cb > 0) {
      pkt[36 + rng(cb)] ^= static_cast<uint8_t>(1 + rng(255));  // payload
    } else if (corrupt == 3) {
      const uint32_t nc = static_cast<uint32_t>(recs.size()) + 1;
      for (int i = 0; i < 4; ++i) pkt[24 + i] = static_cast<uint8_t>(nc >> (8 * i));
      const uint32_t hcrc = zhao_abi::zhao_crc32c(0, pkt.data(), 32);
      for (int i = 0; i < 4; ++i) pkt[32 + i] = static_cast<uint8_t>(hcrc >> (8 * i));
    } else if (corrupt == 4) {
      fetch_epoch = 1;  // epoch mismatch: drop before the first payload byte
    }
    const zref::CmdDma::Verdict expect =
        zref::CmdDma::verdict(pkt, static_cast<uint32_t>(pkt.size()), fetch_epoch);
    if (corrupt == 5 && cb > 0) {
      // descriptor truncated to the header: the bound 40+N > byte_len fires
      t.load(kSlotBody0, pkt);
      t.fetch(kSlotBody0, 40, 0);
      zhao::check(t.verdicts_.size() == 1, "rand: one verdict (trunc)", 1, t.verdicts_.size());
      if (t.verdicts_.size() == 1) {
        zhao::check(t.verdicts_[0].status == zhao_abi::ZH_ABI_BAD_LENGTH,
                    "rand: truncated descriptor -> BAD_LENGTH", zhao_abi::ZH_ABI_BAD_LENGTH,
                    t.verdicts_[0].status);
      }
      continue;
    }
    t.load(kSlotBody0, pkt);
    t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), fetch_epoch);
    zhao::check(t.verdicts_.size() == 1, "rand: one verdict", 1, t.verdicts_.size());
    if (t.verdicts_.size() == 1) {
      zhao::check(t.verdicts_[0].status == expect.status, "rand: predicted verdict", expect.status,
                  t.verdicts_[0].status);
      zhao::check(t.verdicts_[0].bytes == expect.bytes_consumed, "rand: bytes_consumed law",
                  expect.bytes_consumed, t.verdicts_[0].bytes);
      if (expect.status == 0) {
        zhao::check(t.verdicts_[0].cmds == expect.cmds_walked, "rand: cmds_consumed law",
                    expect.cmds_walked, t.verdicts_[0].cmds);
      }
      if (expect.status != 0) {
        zhao::check(t.pkt_bytes_.empty(), "rand: ZERO bytes downstream on error", 0,
                    t.pkt_bytes_.size());
      }
    }
  }
  std::printf("cmd_dma random: %u packets done", packets);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  uint32_t random_packets = 0;
  uint64_t seed = 0xDA0C060820260815ull;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_packets = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = std::strtoull(argv[++i], nullptr, 0);
    }
  }
  if (random_packets > 0) {
    runRandom(random_packets, seed);
    return zhao::report_and_exit("cmd_dma_random");
  }

  using zhao_abi::ZHAO_OP_BEGIN_FRAME;
  using zhao_abi::ZHAO_OP_DEBUG_FRAME_BLIT;
  using zhao_abi::ZHAO_OP_END_FRAME;
  using zhao_abi::ZHAO_OP_NOP;

  // ---- 1. happy fetch --------------------------------------------------------
  {
    DmaBench t;
    const std::vector<uint8_t> pkt = makePacket(
        9, 0, 50, 0,
        {makeRecord(ZHAO_OP_BEGIN_FRAME, 32, {9, 0, 0, 50}), makeRecord(ZHAO_OP_NOP, 16, {}),
         makeRecord(ZHAO_OP_END_FRAME, 32, {0, 0, 0, 0})});
    t.load(kSlotBody0, pkt);
    t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
    check(t.verdicts_.size() == 1, "happy: one verdict", 1, t.verdicts_.size());
    check(t.verdicts_[0].status == 0, "happy: status OK", 0, t.verdicts_[0].status);
    check(t.verdicts_[0].bytes == pkt.size(), "happy: bytes_consumed = 40+N", pkt.size(),
          t.verdicts_[0].bytes);
    check(t.verdicts_[0].cmds == 3, "happy: cmds_consumed = 3", 3, t.verdicts_[0].cmds);
    t.idle(static_cast<int>(pkt.size()) + 8);  // drain the verified stream
    check(t.pkt_bytes_ == pkt, "happy: verified stream bit-exact", pkt.size(), t.pkt_bytes_.size());
    // burst geometry: first burst at the slot base, 64 B, 64-B aligned steps
    check(!t.bursts_.empty() && t.bursts_[0].addr == kSlotBody0, "happy: first burst at slot base",
          kSlotBody0, t.bursts_.empty() ? 0 : t.bursts_[0].addr);
    check(!t.bursts_.empty() && t.bursts_[0].len == 64, "happy: 64-B burst", 64,
          t.bursts_.empty() ? 0 : t.bursts_[0].len);
    for (const auto& b : t.bursts_) {
      check((b.addr & 0x3F) == 0, "happy: bursts 64-B aligned", 0, b.addr & 0x3F);
      check(b.len >= 8 && b.len <= 64, "happy: burst len in [8,64]", 8, b.len);
    }
    t.idle(4);
  }

  // ---- 2. corrupt header CRC: the GATE ---------------------------------------
  {
    DmaBench t;
    std::vector<uint8_t> pkt = makePacket(1, 0, 20, 0, {makeRecord(ZHAO_OP_NOP, 16, {})});
    pkt[8] ^= 0xFF;  // corrupt a header byte INSIDE the CRC window [0,32)
    t.load(kSlotBody0, pkt);
    t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
    check(t.verdicts_.size() == 1, "crc-h: one verdict", 1, t.verdicts_.size());
    check(t.verdicts_[0].status == zhao_abi::ZH_ABI_BAD_HEADER_CRC, "crc-h: status BAD_HEADER_CRC",
          zhao_abi::ZH_ABI_BAD_HEADER_CRC, t.verdicts_[0].status);
    check(t.verdicts_[0].bytes == 36, "crc-h: bytes_consumed = 36", 36, t.verdicts_[0].bytes);
    check(t.bursts_.size() == 1, "crc-h: ONLY the header burst issued", 1, t.bursts_.size());
    check(t.pkt_bytes_.empty(), "crc-h: ZERO bytes downstream", 0, t.pkt_bytes_.size());
    t.idle(4);
    check(t.pkt_bytes_.empty(), "crc-h: still zero bytes after idle", 0, t.pkt_bytes_.size());
  }

  // ---- 3. corrupt payload CRC -------------------------------------------------
  {
    DmaBench t;
    std::vector<uint8_t> pkt =
        makePacket(2, 0, 20, 0, {makeRecord(ZHAO_OP_NOP, 16, {}), makeRecord(ZHAO_OP_NOP, 16, {})});
    pkt[40] ^= 0xAA;  // corrupt a payload byte (header CRC window intact)
    // NOTE: header CRC covers [0,32) only — still valid; payload CRC fails.
    t.load(kSlotBody0, pkt);
    t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
    check(t.verdicts_[0].status == zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC,
          "crc-p: status BAD_PAYLOAD_CRC", zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC, t.verdicts_[0].status);
    check(t.verdicts_[0].bytes == pkt.size(), "crc-p: bytes = 40+N", pkt.size(),
          t.verdicts_[0].bytes);
    check(t.pkt_bytes_.empty(), "crc-p: ZERO bytes downstream (no partial)", 0,
          t.pkt_bytes_.size());
  }

  // ---- 4. epoch mismatch ------------------------------------------------------
  {
    DmaBench t;
    const std::vector<uint8_t> pkt = makePacket(3, 7, 20, 0, {makeRecord(ZHAO_OP_NOP, 16, {})});
    t.load(kSlotBody0, pkt);
    t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);  // cur epoch 0 != 7
    check(t.verdicts_[0].status == zref::ZHAO_DMA_EPOCH_MISMATCH, "epoch: status 15 (local)", 15,
          t.verdicts_[0].status);
    check(t.verdicts_[0].bytes == 36, "epoch: header-level abort", 36, t.verdicts_[0].bytes);
    check(t.bursts_.size() == 1, "epoch: dropped before the first payload byte", 1,
          t.bursts_.size());
    check(t.pkt_bytes_.empty(), "epoch: no bytes", 0, t.pkt_bytes_.size());
  }

  // ---- 5. truncated descriptor -------------------------------------------------
  {
    DmaBench t;
    const std::vector<uint8_t> pkt = makePacket(4, 0, 20, 0, {makeRecord(ZHAO_OP_NOP, 16, {})});
    t.load(kSlotBody0, pkt);
    t.fetch(kSlotBody0, 20, 0);  // descriptor len < 36: cannot hold a header
    check(t.verdicts_[0].status == zhao_abi::ZH_ABI_BAD_LENGTH, "trunc0: BAD_LENGTH",
          zhao_abi::ZH_ABI_BAD_LENGTH, t.verdicts_[0].status);
    check(t.bursts_.empty(), "trunc0: no burst issued at all", 0, t.bursts_.size());
    check(t.pkt_bytes_.empty(), "trunc0: no bytes", 0, t.pkt_bytes_.size());

    // descriptor len shorter than the sealed packet: 40+N > byte_len
    t.fetch(kSlotBody0, 48, 0);
    check(t.verdicts_[0].status == zhao_abi::ZH_ABI_BAD_LENGTH, "trunc1: BAD_LENGTH",
          zhao_abi::ZH_ABI_BAD_LENGTH, t.verdicts_[0].status);
    check(t.verdicts_.size() == 1 && t.verdicts_[0].bytes == 36, "trunc1: header-level abort", 36,
          t.verdicts_[0].bytes);
  }

  // ---- 6. walk errors with intact CRCs -----------------------------------------
  {// unknown opcode (0x9999) with a record size that satisfies nothing
   {DmaBench t;
  const std::vector<uint8_t> pkt = makePacket(5, 0, 20, 0, {makeRecord(0x9999, 16, {})});
  t.load(kSlotBody0, pkt);
  t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
  check(t.verdicts_[0].status == zhao_abi::ZH_ABI_UNKNOWN_OPCODE, "walk: UNKNOWN_OPCODE",
        zhao_abi::ZH_ABI_UNKNOWN_OPCODE, t.verdicts_[0].status);
  check(t.pkt_bytes_.empty(), "walk: no bytes", 0, t.pkt_bytes_.size());
  const zref::CmdDma::Verdict ov = zref::CmdDma::verdict(pkt, static_cast<uint32_t>(pkt.size()), 0);
  check(t.verdicts_[0].status == ov.status, "walk: oracle status", ov.status,
        t.verdicts_[0].status);
  check(t.verdicts_[0].bytes == ov.bytes_consumed,
        "walk: oracle bytes_consumed (walk abort consumes 40+N)", ov.bytes_consumed,
        t.verdicts_[0].bytes);
  check(t.verdicts_[0].cmds == ov.cmds_walked, "walk: oracle cmds_walked", ov.cmds_walked,
        t.verdicts_[0].cmds);
}
// count mismatch: records sum exactly to cb (2 x 48 B = 96), count says
// 3 (3*16 = 48 <= 96, so the fail-safe bounds pass; intact CRCs)
{
  DmaBench t;
  std::vector<uint8_t> p =
      makePacket(6, 0, 20, 1,  // flags bit0: the 48-B records are debug opcodes
                 {makeRecord(ZHAO_OP_DEBUG_FRAME_BLIT, 48, {0, 0, 0, 0}),
                  makeRecord(ZHAO_OP_DEBUG_FRAME_BLIT, 48, {0, 0, 0, 0})});
  for (int i = 0; i < 4; ++i) {
    p[24 + i] = static_cast<uint8_t>(3 >> (8 * i));  // command_count = 3
  }
  // recompute the header CRC (bytes [0,32)) — the count lives inside it
  const uint32_t hcrc = zhao_abi::zhao_crc32c(0, p.data(), 32);
  for (int i = 0; i < 4; ++i) p[32 + i] = static_cast<uint8_t>(hcrc >> (8 * i));
  t.load(kSlotBody0, p);
  t.fetch(kSlotBody0, static_cast<uint32_t>(p.size()), 0);
  check(t.verdicts_[0].status == zhao_abi::ZH_ABI_COUNT_MISMATCH, "walk: COUNT_MISMATCH",
        zhao_abi::ZH_ABI_COUNT_MISMATCH, t.verdicts_[0].status);
  check(t.verdicts_[0].cmds == 2, "walk: 2 records walked before abort", 2, t.verdicts_[0].cmds);
  const zref::CmdDma::Verdict ov = zref::CmdDma::verdict(p, static_cast<uint32_t>(p.size()), 0);
  check(ov.status == zhao_abi::ZH_ABI_COUNT_MISMATCH && ov.cmds_walked == 2 &&
            t.verdicts_[0].bytes == ov.bytes_consumed,
        "walk: oracle agrees (status/cmds/bytes)", ov.bytes_consumed, t.verdicts_[0].bytes);
  check(t.pkt_bytes_.empty(), "walk: no bytes (count mismatch)", 0, t.pkt_bytes_.size());
}
// debug opcode without flags bit0
{
  DmaBench t;
  const std::vector<uint8_t> pkt =
      makePacket(7, 0, 20, 0,  // flags = 0
                 {makeRecord(ZHAO_OP_DEBUG_FRAME_BLIT, 48, {1, 0x00100000u, 184320, 0})});
  t.load(kSlotBody0, pkt);
  t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
  check(t.verdicts_[0].status == zhao_abi::ZH_ABI_DEBUG_FLAG_REQUIRED, "walk: DEBUG_FLAG_REQUIRED",
        zhao_abi::ZH_ABI_DEBUG_FLAG_REQUIRED, t.verdicts_[0].status);
}
// the same debug opcode WITH flags bit0: OK
{
  DmaBench t;
  const std::vector<uint8_t> pkt =
      makePacket(8, 0, 20, 1,  // flags bit0 = contains_debug_commands
                 {makeRecord(ZHAO_OP_DEBUG_FRAME_BLIT, 48, {1, 0x00100000u, 184320, 0})});
  t.load(kSlotBody0, pkt);
  t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
  check(t.verdicts_[0].status == 0, "walk: debug frame with bit0 OK", 0, t.verdicts_[0].status);
  t.idle(static_cast<int>(pkt.size()) + 8);
  check(t.pkt_bytes_.size() == pkt.size(), "walk: debug frame streamed", 1, t.pkt_bytes_.size());
}
}

// ---- 7. bridge error ----------------------------------------------------------
{
  DmaBench t;
  const std::vector<uint8_t> pkt = makePacket(10, 0, 20, 0, {makeRecord(ZHAO_OP_NOP, 16, {})});
  t.load(kSlotBody0, pkt);
  t.force_err_ = true;
  t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
  check(t.verdicts_[0].status == 17, "bridge: status 17 (local)", 17, t.verdicts_[0].status);
  check(t.pkt_bytes_.empty(), "bridge: nothing downstream", 0, t.pkt_bytes_.size());
}

return zhao::report_and_exit("cmd_dma_directed");
}
