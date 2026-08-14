// test_stub_top.cpp — Verilated stub top test (W1 gate (a), W4 semantics).
//
// The stub now consumes the FULL sealed packet (40 + N, capture_format.md 3)
// and validates it with the generated zhao_abi_pkg: status carries the
// zhao_abi_error codes, header_crc32c is genuinely checked, and a
// commands_consumed counter exists. Packets are built with the ZRef frame
// builder (same bytes as the committed goldens).
//
// Driving rule for error cases: header-level aborts (spec 3.2 checks 1-3)
// consume exactly 36 bytes and the stub resyncs, so the test feeds ONLY the
// 36-byte header for those; payload-level verdicts consume the full packet.

#include "Vzhao_stub_top.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zhao_abi.h"  // generated
#include "zref/zref_frame.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace zhao_abi;

// error codes under test (zhao_abi_error values)
constexpr uint8_t E_OK = ZH_ABI_OK;
constexpr uint8_t E_BAD_MAGIC = ZH_ABI_BAD_MAGIC;
constexpr uint8_t E_BAD_ABI = ZH_ABI_BAD_ABI_VERSION;
constexpr uint8_t E_RESERVED_FLAG = ZH_ABI_RESERVED_FLAG;
constexpr uint8_t E_BAD_LENGTH = ZH_ABI_BAD_LENGTH;
constexpr uint8_t E_BAD_HEADER_CRC = ZH_ABI_BAD_HEADER_CRC;
constexpr uint8_t E_BAD_PAYLOAD_CRC = ZH_ABI_BAD_PAYLOAD_CRC;
constexpr uint8_t COMPL_DONE = ZHAO_COMPL_DONE;
constexpr uint8_t COMPL_ERR = ZHAO_COMPL_ERR;

std::vector<uint8_t> golden_frame() {
  // the canonical minimal frame (== tests/abi/golden/frame_minimal.bin)
  std::vector<uint8_t> rec;
  zhao::ZhaoFrameBuilder fb;
  zhao_pack_begin_frame(zhao_sample_begin_frame(), rec);
  fb.append_record(rec);
  rec.clear();
  zhao_pack_nop(zhao_sample_nop(), rec);
  fb.append_record(rec);
  rec.clear();
  zhao_pack_end_frame(zhao_sample_end_frame(), rec);
  fb.append_record(rec);
  return fb.seal(1, 0, 1, 0, 0);
}

std::vector<uint8_t> resealed(std::vector<uint8_t> pkt) {
  // recompute both CRCs over the mutated packet
  const uint32_t command_bytes =
      uint32_t(pkt[28]) | (uint32_t(pkt[29]) << 8) | (uint32_t(pkt[30]) << 16) | (uint32_t(pkt[31]) << 24);
  const auto put32 = [&](uint32_t off, uint32_t v) {
    for (int i = 0; i < 4; i++) pkt[off + i] = uint8_t(v >> (8 * i));
  };
  put32(ZHAO_OFF_HEADER_CRC, zhao_crc32c(0, pkt.data(), 32));
  put32(ZHAO_FRAME_HEADER_BYTES + command_bytes,
        zhao_crc32c(0, pkt.data() + ZHAO_FRAME_HEADER_BYTES, command_bytes));
  return pkt;
}

void feed(Vzhao_stub_top& top, const std::vector<uint8_t>& bytes) {
  for (uint8_t b : bytes) {
    if (!zhao::send_byte(top, b)) {
      zhao::check(false, "send_byte hung (ready stalled)", 0, 1);
      return;
    }
  }
  zhao::idle(top, 4);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_stub_top top;

  const auto frame = golden_frame();

  // ---- 1. valid minimal frame -> accept, DONE, counters ----------------------
  zhao::reset(top);
  {
    feed(top, frame);
    zhao::check(top.status == E_OK, "minimal frame status", E_OK, top.status);
    zhao::check((top.completion_flags & COMPL_DONE) != 0, "minimal frame DONE",
                COMPL_DONE, top.completion_flags);
    zhao::check((top.completion_flags & COMPL_ERR) == 0, "minimal frame no ERR",
                0, top.completion_flags & COMPL_ERR);
    zhao::check(top.frames_accepted == 1, "frames_accepted", 1, top.frames_accepted);
    zhao::check(top.frames_rejected == 0, "frames_rejected", 0, top.frames_rejected);
    zhao::check(top.bytes_consumed == frame.size(), "bytes_consumed",
                frame.size(), top.bytes_consumed);
    zhao::check(top.commands_consumed == 3, "commands_consumed", 3, top.commands_consumed);
  }

  // ---- 2. valid empty frame (40 bytes) ----------------------------------------
  {
    zhao::ZhaoFrameBuilder fb;  // no commands
    const auto empty = fb.seal(2, 1, 1, 0, 0);
    feed(top, empty);
    zhao::check(top.status == E_OK, "empty frame status", E_OK, top.status);
    zhao::check(top.frames_accepted == 2, "empty frame accepted", 2, top.frames_accepted);
    zhao::check(top.bytes_consumed == frame.size() + 40, "bytes after empty frame",
                frame.size() + 40, top.bytes_consumed);
    zhao::check(top.commands_consumed == 0, "empty frame commands", 0, top.commands_consumed);
  }

  // ---- 3. header-level aborts (feed the 36-byte header only) ------------------
  {
    struct Case {
      const char* name;
      std::vector<uint8_t> hdr;  // 36 bytes
      uint8_t want;
    };
    auto hdr_of = [](std::vector<uint8_t> pkt, int abi_override = 0) {
      pkt.resize(36);
      if (abi_override) {
        pkt[4] = uint8_t(abi_override);
        pkt[5] = uint8_t(abi_override >> 8);
      }
      return pkt;
    };
    std::vector<Case> cases;
    {
      auto h = hdr_of(frame);
      h[0] = 0xEF;  // corrupt magic (no re-seal: corruption IS the test)
      cases.push_back({"bad magic", h, E_BAD_MAGIC});
    }
    cases.push_back({"bad abi", hdr_of(frame, 2), E_BAD_ABI});
    {
      // reserved frame flag bit set, resealed (so earlier checks pass)
      auto full = resealed([&] { auto f = frame; f[6] = 0x08; return f; }());
      full.resize(36);
      cases.push_back({"reserved flag", full, E_RESERVED_FLAG});
    }
    {
      auto h = hdr_of(frame);
      h[28] = 13;  // misaligned command_bytes
      cases.push_back({"misaligned length", h, E_BAD_LENGTH});
    }
    {
      auto h = hdr_of(frame);
      h[25] = 9;  // command_count = 0x900 > capacity (no re-seal needed)
      cases.push_back({"count over capacity", h, E_BAD_LENGTH});
    }
    {
      auto h = hdr_of(frame);
      h[33] ^= 0x40;  // corrupt header CRC word
      cases.push_back({"bad header crc", h, E_BAD_HEADER_CRC});
    }

    for (const auto& c : cases) {
      zhao::reset(top);
      feed(top, c.hdr);
      zhao::check(top.status == c.want, c.name, c.want, top.status);
      zhao::check((top.completion_flags & COMPL_ERR) != 0, c.name, COMPL_ERR,
                  top.completion_flags);
      zhao::check(top.frames_rejected == 1, c.name, 1, top.frames_rejected);
      // header-level abort: exactly 36 bytes consumed (spec 3.2)
      zhao::check(top.bytes_consumed == 36, c.name, 36, top.bytes_consumed);
      if (top.status != c.want) {
        zhao::save_failing_vector("stub_abort_case", c.hdr,
                                  "status=" + std::to_string(c.want),
                                  "status=" + std::to_string(top.status));
      }
    }
  }

  // ---- 4. payload-level verdict: corrupted payload CRC ------------------------
  {
    zhao::reset(top);
    auto bad = frame;
    bad[bad.size() - 1] ^= 0x80;  // flip a bit in payload_crc32c
    feed(top, bad);
    zhao::check(top.status == E_BAD_PAYLOAD_CRC, "bad payload crc", E_BAD_PAYLOAD_CRC,
                top.status);
    zhao::check(top.frames_rejected == 1, "payload crc rejected", 1, top.frames_rejected);
    zhao::check(top.bytes_consumed == bad.size(), "payload crc bytes", bad.size(),
                top.bytes_consumed);
  }

  // ---- 5. back-to-back valid frames stream seamlessly --------------------------
  {
    zhao::reset(top);
    feed(top, frame);
    feed(top, frame);
    zhao::check(top.status == E_OK, "second frame status", E_OK, top.status);
    zhao::check(top.frames_accepted == 2, "two frames accepted", 2, top.frames_accepted);
    zhao::check(top.bytes_consumed == 2 * frame.size(), "two frames bytes",
                2 * frame.size(), top.bytes_consumed);
  }

  // ---- 6. fuzz: no hang, counters self-consistent ------------------------------
  {
    zhao::reset(top);
    uint32_t lcg = 0x5A175A17u;
    const int FUZZ_BYTES = 4096;
    for (int i = 0; i < FUZZ_BYTES; i++) {
      lcg = lcg * 1664525u + 1013904223u;
      const uint8_t b = static_cast<uint8_t>(lcg >> 24);
      if (!zhao::send_byte(top, b, /*max_wait=*/1000)) {
        zhao::check(false, "fuzz: model hung", 0, i);
        break;
      }
      if (top.bytes_consumed > static_cast<uint32_t>(i + 1)) {
        zhao::check(false, "fuzz: consumed more than fed", i + 1, top.bytes_consumed);
        break;
      }
    }
    zhao::idle(top, 8);
    const uint64_t decided = uint64_t(top.frames_accepted) + top.frames_rejected;
    zhao::check(decided * 36 <= top.bytes_consumed + 71,
                "fuzz: decisions vs consumed bytes", 0, decided);
    std::printf("fuzz: accepted=%u rejected=%u bytes=%u\n",
                static_cast<unsigned>(top.frames_accepted),
                static_cast<unsigned>(top.frames_rejected),
                static_cast<unsigned>(top.bytes_consumed));
  }

  top.final();
  return zhao::report_and_exit("test_stub_top");
}
