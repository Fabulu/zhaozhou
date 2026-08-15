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
//   8. blit engine: full-canvas fetch + CRC-verified VRAM commit through
//      the guard port (Z60: 184,320 B, 64-B beats, exact addresses);
//      corrupt blit CRC => ZERO guard writes; bad length => rejected
//      before the first byte

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"
#include "Vzhao_cmd_dma.h"

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
  w[0] = static_cast<uint32_t>(sh & 0xFFFFFFFFull) | (last ? 2u : 0u) |
         (err ? 1u : 0u);
  w[1] = static_cast<uint32_t>((sh >> 32) & 0xFFFFFFFFull);
  w[2] = static_cast<uint32_t>((data >> 62) & 3ull) | (beat_valid ? 4u : 0u);
}

// ---- the harness-as-HPS device ----------------------------------------------
constexpr uint32_t kRingBase = 0x0;
constexpr uint32_t kSlotBody0 = kRingBase + 4096;      // slot 0 body
constexpr uint32_t kBlitSrc = 0x00100000;             // blit source arena
constexpr int kFirstBeatLatency = 16;                  // D10 sim profile

class DmaBench {
 public:
  DmaBench() : top_(new Vzhao_cmd_dma) { reset(); }
  ~DmaBench() { top_->final(); delete top_; }
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
    top_->blit_req_valid_i = 0;
    top_->blit_dst_slot_i = 0;
    top_->blit_mode_i = 0;
    top_->blit_src_i = 0;
    top_->blit_len_i = 0;
    top_->blit_crc_i = 0;
    top_->pkt_ready_i = 1;
    setRsp(&top_->hps_rsp_i[0], false, 0, false, false);
    top_->guard_rsp_i = 4;  // packed {ready(MSB),ok,violation}: ready=1
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
    // observe the guard port (the MEM.GUARD seam)
    if (guardValid()) {
      const uint32_t gaddr = guardAddr();
      const uint8_t glen = guardLen();
      const uint64_t gdata = top_->guard_wdata_o;
      guard_writes_.push_back(GuardWrite{gaddr, glen, gdata});
    }
    // capture the verdict pulse + packet stream
    if (top_->dma_done_o) {
      verdicts_.push_back(Verdict{top_->dma_slot_o, top_->dma_status_o,
                                  top_->dma_bytes_consumed_o,
                                  top_->dma_cmds_consumed_o});
    }
    if (top_->pkt_valid_o && top_->pkt_ready_i) {
      pkt_bytes_.push_back(static_cast<uint8_t>(top_->pkt_byte_o));
    }
    if (top_->blit_done_o) {
      blit_results_.push_back(top_->blit_status_o);
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

  void blit(uint8_t dst, uint8_t mode, uint32_t src, uint32_t len, uint32_t crc,
            int max_cycles = 400000) {
    blit_results_.clear();
    bursts_.clear();
    guard_writes_.clear();
    top_->blit_req_valid_i = 1;
    top_->blit_dst_slot_i = dst;
    top_->blit_mode_i = mode;
    top_->blit_src_i = src;
    top_->blit_len_i = len;
    top_->blit_crc_i = crc;
    cycle();
    top_->blit_req_valid_i = 0;
    for (int i = 0; i < max_cycles && blit_results_.empty(); ++i) cycle();
  }

  uint8_t& mem(uint32_t a) {
    if (a >= mem_.size()) mem_.resize(a + 4096, 0);
    return mem_[a];
  }

  // guard packed view: {valid,write,client[2:0],addr[26:0],len[6:0],be[63:0]}
  // 103 bits -> words[3:0]; valid = bit102 (w3 bit6), len = bits[70:64]
  // (w2 bits[6:0]), addr = bits[97:71] (w2 bits[26:0] << ... )
  bool guardValid() const {
    return (top_->guard_req_o[3] & 0x40u) != 0;
  }
  uint8_t guardLen() const {
    return static_cast<uint8_t>(top_->guard_req_o[2] & 0x7Fu);
  }
  uint32_t guardAddr() const {
    // addr[26:0] = bits[97:71]: word2 bits [31:7] = addr[24:0],
    // word3 bit 0 = addr[25], bit 1 = addr[26]
    const uint32_t lo = top_->guard_req_o[2] >> 7;
    const uint32_t hi = top_->guard_req_o[3] & 0x3u;
    return lo | (hi << 25);
  }

  struct Verdict {
    uint8_t slot;
    uint8_t status;
    uint32_t bytes;
    uint32_t cmds;
  };
  struct GuardWrite {
    uint32_t addr;
    uint8_t len;
    uint64_t data;
  };

  Vzhao_cmd_dma* top_;
  std::vector<uint8_t> mem_ = std::vector<uint8_t>(1u << 20);
  std::vector<Verdict> verdicts_;
  std::vector<uint8_t> pkt_bytes_;
  std::vector<ReqView> bursts_;
  std::vector<GuardWrite> guard_writes_;
  std::vector<uint8_t> blit_results_;
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
std::vector<uint8_t> makePacket(uint32_t frame_id, uint32_t epoch,
                                uint32_t deadline, uint16_t flags,
                                const std::vector<std::vector<uint8_t>>& records) {
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
  put32(0, 0x314B505Au);  // magic
  put16(4, 2);            // abi_version
  put16(6, flags);
  put32(8, frame_id);
  put32(12, frame_id);    // sequence
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
// given the (possibly corrupted) bytes, the descriptor length and the
// current epoch, the verdict is fixed in advance. Record-semantic checks
// (6/7/8) are decoder-side (wave 3) and intentionally absent here too.
using zhao_abi::ZHAO_OP_BEGIN_FRAME;
using zhao_abi::ZHAO_OP_DEBUG_FRAME_BLIT;
using zhao_abi::ZHAO_OP_DEBUG_RUMBLE;
using zhao_abi::ZHAO_OP_END_FRAME;
using zhao_abi::ZHAO_OP_NOP;
using zhao_abi::ZHAO_OP_SET_PRESENTATION_CONTRACT;


static uint8_t predictVerdict(const std::vector<uint8_t>& p, uint32_t f_len,
                              uint32_t f_epoch) {
  auto get16 = [&](uint32_t o) {
    return static_cast<uint16_t>(p[o] | (p[o + 1] << 8));
  };
  auto get32 = [&](uint32_t o) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(p[o + i]) << (8 * i);
    return v;
  };
  if (f_len < 36) return zhao_abi::ZH_ABI_BAD_LENGTH;
  if (get32(0) != 0x314B505Au) return zhao_abi::ZH_ABI_BAD_MAGIC;
  if (get16(4) != 2) return zhao_abi::ZH_ABI_BAD_ABI_VERSION;
  if ((get16(6) & 0xFFFE) != 0) return zhao_abi::ZH_ABI_RESERVED_FLAG;
  const uint32_t cb = get32(28);
  const uint32_t cc = get32(24);
  if ((cb & 15) != 0) return zhao_abi::ZH_ABI_BAD_LENGTH;
  if (cc > (cb >> 4)) return zhao_abi::ZH_ABI_BAD_LENGTH;
  if (40 + cb > f_len) return zhao_abi::ZH_ABI_BAD_LENGTH;
  if (zhao_abi::zhao_crc32c(0, p.data(), 32) != get32(32)) {
    return zhao_abi::ZH_ABI_BAD_HEADER_CRC;
  }
  if (get32(16) != f_epoch) return zref::ZHAO_DMA_EPOCH_MISMATCH;
  if (zhao_abi::zhao_crc32c(0, p.data() + 36, cb) != get32(36 + cb)) {
    return zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC;
  }
  uint32_t off = 0;
  uint32_t walked = 0;
  while (off < cb) {
    const uint16_t op = get16(36 + off);
    const uint16_t rb = get16(36 + off + 2);
    uint16_t size;
    switch (op) {
      case ZHAO_OP_NOP: size = 16; break;
      case ZHAO_OP_BEGIN_FRAME: size = 32; break;
      case ZHAO_OP_END_FRAME: size = 32; break;
      case zhao_abi::ZHAO_OP_SET_VIEW: size = 96; break;
      case ZHAO_OP_SET_PRESENTATION_CONTRACT: size = 48; break;
      case zhao_abi::ZHAO_OP_TERRAIN_FIELD: size = 112; break;
      case zhao_abi::ZHAO_OP_SURFACE_STAMP: size = 64; break;
      case zhao_abi::ZHAO_OP_DRAW_FORM: size = 32; break;
      case zhao_abi::ZHAO_OP_DRAW_POPULATION: size = 32; break;
      case zhao_abi::ZHAO_OP_DRAW_PROCEDURAL: size = 64; break;
      case zhao_abi::ZHAO_OP_DRAW_SKY: size = 176; break;
      case zhao_abi::ZHAO_OP_EMIT_AUDIO_EVENT: size = 32; break;
      case zhao_abi::ZHAO_OP_DEBUG_BOOTSTRAP: size = 64; break;
      case ZHAO_OP_DEBUG_FRAME_BLIT: size = 48; break;
      case ZHAO_OP_DEBUG_RUMBLE: size = 32; break;
      default: size = 0; break;
    }
    if ((rb & 15) != 0 || rb < 16) return zhao_abi::ZH_ABI_BAD_LENGTH;
    if (size == 0) return zhao_abi::ZH_ABI_UNKNOWN_OPCODE;
    if (size != rb) return zhao_abi::ZH_ABI_BAD_LENGTH;
    if (off + rb > cb) return zhao_abi::ZH_ABI_TRUNCATED;
    if (op >= 0xF000 && op <= 0xF0FF && (get16(6) & 1) == 0) {
      return zhao_abi::ZH_ABI_DEBUG_FLAG_REQUIRED;
    }
    off += rb;
    ++walked;
  }
  if (walked != cc) return zhao_abi::ZH_ABI_COUNT_MISMATCH;
  return 0;
}

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
        recs.push_back(makeRecord(ZHAO_OP_SET_PRESENTATION_CONTRACT, 48,
                                  {rng(3), 0, 0, 0}));
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
    const uint8_t expect =
        predictVerdict(pkt, static_cast<uint32_t>(pkt.size()), fetch_epoch);
    if (corrupt == 5 && cb > 0) {
      // descriptor truncated to the header: the bound 40+N > byte_len fires
      t.load(kSlotBody0, pkt);
      t.fetch(kSlotBody0, 40, 0);
      zhao::check(t.verdicts_.size() == 1, "rand: one verdict (trunc)", 1,
                  t.verdicts_.size());
      if (t.verdicts_.size() == 1) {
        zhao::check(t.verdicts_[0].status == zhao_abi::ZH_ABI_BAD_LENGTH,
                    "rand: truncated descriptor -> BAD_LENGTH",
                    zhao_abi::ZH_ABI_BAD_LENGTH, t.verdicts_[0].status);
      }
      continue;
    }
    t.load(kSlotBody0, pkt);
    t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), fetch_epoch);
    zhao::check(t.verdicts_.size() == 1, "rand: one verdict", 1, t.verdicts_.size());
    if (t.verdicts_.size() == 1) {
      zhao::check(t.verdicts_[0].status == expect, "rand: predicted verdict", expect,
                  t.verdicts_[0].status);
      const uint32_t want_bytes =
          (expect == zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC ||
           expect == zhao_abi::ZH_ABI_COUNT_MISMATCH ||
           expect == zhao_abi::ZH_ABI_TRUNCATED || expect == 0)
              ? static_cast<uint32_t>(pkt.size())
              : 36;
      zhao::check(t.verdicts_[0].bytes == want_bytes, "rand: bytes_consumed law",
                  want_bytes, t.verdicts_[0].bytes);
      if (expect != 0) {
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
        {makeRecord(ZHAO_OP_BEGIN_FRAME, 32, {9, 0, 0, 50}),
         makeRecord(ZHAO_OP_NOP, 16, {}),
         makeRecord(ZHAO_OP_END_FRAME, 32, {0, 0, 0, 0})});
    t.load(kSlotBody0, pkt);
    t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
    check(t.verdicts_.size() == 1, "happy: one verdict", 1, t.verdicts_.size());
    check(t.verdicts_[0].status == 0, "happy: status OK", 0, t.verdicts_[0].status);
    check(t.verdicts_[0].bytes == pkt.size(),
          "happy: bytes_consumed = 40+N", pkt.size(), t.verdicts_[0].bytes);
    check(t.verdicts_[0].cmds == 3, "happy: cmds_consumed = 3", 3,
          t.verdicts_[0].cmds);
    t.idle(static_cast<int>(pkt.size()) + 8);  // drain the verified stream
    check(t.pkt_bytes_ == pkt, "happy: verified stream bit-exact", pkt.size(),
          t.pkt_bytes_.size());
    // burst geometry: first burst at the slot base, 64 B, 64-B aligned steps
    check(!t.bursts_.empty() && t.bursts_[0].addr == kSlotBody0,
          "happy: first burst at slot base", kSlotBody0,
          t.bursts_.empty() ? 0 : t.bursts_[0].addr);
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
    std::vector<uint8_t> pkt = makePacket(
        1, 0, 20, 0, {makeRecord(ZHAO_OP_NOP, 16, {})});
    pkt[8] ^= 0xFF;  // corrupt a header byte INSIDE the CRC window [0,32)
    t.load(kSlotBody0, pkt);
    t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
    check(t.verdicts_.size() == 1, "crc-h: one verdict", 1, t.verdicts_.size());
    check(t.verdicts_[0].status == zhao_abi::ZH_ABI_BAD_HEADER_CRC,
          "crc-h: status BAD_HEADER_CRC", zhao_abi::ZH_ABI_BAD_HEADER_CRC,
          t.verdicts_[0].status);
    check(t.verdicts_[0].bytes == 36, "crc-h: bytes_consumed = 36", 36,
          t.verdicts_[0].bytes);
    check(t.bursts_.size() == 1, "crc-h: ONLY the header burst issued", 1,
          t.bursts_.size());
    check(t.pkt_bytes_.empty(), "crc-h: ZERO bytes downstream", 0,
          t.pkt_bytes_.size());
    t.idle(4);
    check(t.pkt_bytes_.empty(), "crc-h: still zero bytes after idle", 0,
          t.pkt_bytes_.size());
  }

  // ---- 3. corrupt payload CRC -------------------------------------------------
  {
    DmaBench t;
    std::vector<uint8_t> pkt = makePacket(
        2, 0, 20, 0, {makeRecord(ZHAO_OP_NOP, 16, {}),
                      makeRecord(ZHAO_OP_NOP, 16, {})});
    pkt[40] ^= 0xAA;  // corrupt a payload byte (header CRC window intact)
    // NOTE: header CRC covers [0,32) only — still valid; payload CRC fails.
    t.load(kSlotBody0, pkt);
    t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
    check(t.verdicts_[0].status == zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC,
          "crc-p: status BAD_PAYLOAD_CRC", zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC,
          t.verdicts_[0].status);
    check(t.verdicts_[0].bytes == pkt.size(), "crc-p: bytes = 40+N", pkt.size(),
          t.verdicts_[0].bytes);
    check(t.pkt_bytes_.empty(), "crc-p: ZERO bytes downstream (no partial)", 0,
          t.pkt_bytes_.size());
  }

  // ---- 4. epoch mismatch ------------------------------------------------------
  {
    DmaBench t;
    const std::vector<uint8_t> pkt = makePacket(
        3, 7, 20, 0, {makeRecord(ZHAO_OP_NOP, 16, {})});
    t.load(kSlotBody0, pkt);
    t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);  // cur epoch 0 != 7
    check(t.verdicts_[0].status == zref::ZHAO_DMA_EPOCH_MISMATCH,
          "epoch: status 15 (local)", 15, t.verdicts_[0].status);
    check(t.verdicts_[0].bytes == 36, "epoch: header-level abort", 36,
          t.verdicts_[0].bytes);
    check(t.bursts_.size() == 1, "epoch: dropped before the first payload byte", 1,
          t.bursts_.size());
    check(t.pkt_bytes_.empty(), "epoch: no bytes", 0, t.pkt_bytes_.size());
  }

  // ---- 5. truncated descriptor -------------------------------------------------
  {
    DmaBench t;
    const std::vector<uint8_t> pkt = makePacket(
        4, 0, 20, 0, {makeRecord(ZHAO_OP_NOP, 16, {})});
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
    check(t.verdicts_.size() == 1 && t.verdicts_[0].bytes == 36,
          "trunc1: header-level abort", 36, t.verdicts_[0].bytes);
  }

  // ---- 6. walk errors with intact CRCs -----------------------------------------
  {
    // unknown opcode (0x9999) with a record size that satisfies nothing
    {
      DmaBench t;
      const std::vector<uint8_t> pkt =
          makePacket(5, 0, 20, 0, {makeRecord(0x9999, 16, {})});
      t.load(kSlotBody0, pkt);
      t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
      check(t.verdicts_[0].status == zhao_abi::ZH_ABI_UNKNOWN_OPCODE, "walk: UNKNOWN_OPCODE",
            zhao_abi::ZH_ABI_UNKNOWN_OPCODE, t.verdicts_[0].status);
      check(t.pkt_bytes_.empty(), "walk: no bytes", 0, t.pkt_bytes_.size());
    }
    // count mismatch: records sum exactly to cb (2 x 48 B = 96), count says
    // 3 (3*16 = 48 <= 96, so the fail-safe bounds pass; intact CRCs)
    {
      DmaBench t;
      std::vector<uint8_t> p = makePacket(
          6, 0, 20, 1,  // flags bit0: the 48-B records are debug opcodes
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
      check(t.verdicts_[0].status == zhao_abi::ZH_ABI_COUNT_MISMATCH,
            "walk: COUNT_MISMATCH", zhao_abi::ZH_ABI_COUNT_MISMATCH,
            t.verdicts_[0].status);
      check(t.verdicts_[0].cmds == 2, "walk: 2 records walked before abort", 2,
            t.verdicts_[0].cmds);
      check(t.pkt_bytes_.empty(), "walk: no bytes (count mismatch)", 0,
            t.pkt_bytes_.size());
    }
    // debug opcode without flags bit0
    {
      DmaBench t;
      const std::vector<uint8_t> pkt = makePacket(
          7, 0, 20, 0,  // flags = 0
          {makeRecord(ZHAO_OP_DEBUG_FRAME_BLIT, 48,
                      {1, 0x00100000u, 184320, 0})});
      t.load(kSlotBody0, pkt);
      t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
      check(t.verdicts_[0].status == zhao_abi::ZH_ABI_DEBUG_FLAG_REQUIRED,
            "walk: DEBUG_FLAG_REQUIRED", zhao_abi::ZH_ABI_DEBUG_FLAG_REQUIRED,
            t.verdicts_[0].status);
    }
    // the same debug opcode WITH flags bit0: OK
    {
      DmaBench t;
      const std::vector<uint8_t> pkt = makePacket(
          8, 0, 20, 1,  // flags bit0 = contains_debug_commands
          {makeRecord(ZHAO_OP_DEBUG_FRAME_BLIT, 48,
                      {1, 0x00100000u, 184320, 0})});
      t.load(kSlotBody0, pkt);
      t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
      check(t.verdicts_[0].status == 0, "walk: debug frame with bit0 OK", 0,
            t.verdicts_[0].status);
      t.idle(static_cast<int>(pkt.size()) + 8);
      check(t.pkt_bytes_.size() == pkt.size(), "walk: debug frame streamed", 1,
            t.pkt_bytes_.size());
    }
  }

  // ---- 7. bridge error ----------------------------------------------------------
  {
    DmaBench t;
    const std::vector<uint8_t> pkt = makePacket(
        10, 0, 20, 0, {makeRecord(ZHAO_OP_NOP, 16, {})});
    t.load(kSlotBody0, pkt);
    t.force_err_ = true;
    t.fetch(kSlotBody0, static_cast<uint32_t>(pkt.size()), 0);
    check(t.verdicts_[0].status == 17, "bridge: status 17 (local)", 17,
          t.verdicts_[0].status);
    check(t.pkt_bytes_.empty(), "bridge: nothing downstream", 0, t.pkt_bytes_.size());
  }

  // ---- 8. blit engine -------------------------------------------------------------
  {
    DmaBench t;
    // Z60 canvas: 184,320 bytes, deterministic pattern
    std::vector<uint8_t> canvas(184320);
    for (uint32_t i = 0; i < canvas.size(); ++i) {
      canvas[i] = static_cast<uint8_t>((i * 7u + 11u) & 0xFF);
    }
    const uint32_t want_crc = zhao_abi::zhao_crc32c(0, canvas.data(), canvas.size());
    t.load(kBlitSrc, canvas);

    // 8a. good blit into FB slot 1
    t.blit(1, 0 /*VIDEO_Z60*/, kBlitSrc, 184320, want_crc);
    check(t.blit_results_.size() == 1, "blit: one completion", 1,
          t.blit_results_.size());
    check(t.blit_results_[0] == 0, "blit: committed (status 0)", 0,
          t.blit_results_[0]);
    uint32_t got_bytes = 0;
    bool data_ok = true;
    uint32_t off = 0;
    for (const auto& g : t.guard_writes_) {
      check(g.addr == (0x0003C000u /* ZHAO_FB_SLOT1_BASE (zhao_pkg) */ + off),
            "blit: guard addr exact (slot-1 region)", 0x0003C000u /* ZHAO_FB_SLOT1_BASE (zhao_pkg) */ + off,
            g.addr);
      check(g.len == 64, "blit: 64-B beats", 64, g.len);
      for (int b = 0; b < 8; ++b) {
        const uint8_t want = canvas[off + static_cast<uint32_t>(b)];
        const uint8_t got =
            static_cast<uint8_t>((g.data >> (8 * b)) & 0xFF);
        if (want != got) data_ok = false;
      }
      got_bytes += g.len;
      off += 64;
    }
    check(got_bytes == 184320, "blit: exactly canvas bytes committed", 184320,
          got_bytes);
    check(data_ok, "blit: committed bytes bit-exact", 1, data_ok);

    // 8b. corrupt blit CRC: ZERO guard writes (the gate)
    t.blit(0, 0, kBlitSrc, 184320, want_crc ^ 1);
    check(t.blit_results_[0] == zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC,
          "blit-crc: BAD_PAYLOAD_CRC", zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC, t.blit_results_[0]);
    check(t.guard_writes_.empty(), "blit-crc: ZERO VRAM writes", 0,
          t.guard_writes_.size());

    // 8c. wrong length (not canvas_bytes(mode)): rejected before any byte
    t.blit(0, 0, kBlitSrc, 100000, want_crc);
    check(t.blit_results_[0] == 18, "blit-len: status 18 (rejected)", 18,
          t.blit_results_[0]);
    check(t.bursts_.empty(), "blit-len: rejected before the first byte", 0,
          t.bursts_.size());
    check(t.guard_writes_.empty(), "blit-len: zero writes", 0,
          t.guard_writes_.size());
  }

  return zhao::report_and_exit("cmd_dma_directed");
}
