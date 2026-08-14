// test_stub_top.cpp — Phase-1 gate (a) stub test (plan W1).
//
// Drives the Verilated zhao_stub_top model:
//   1. valid empty frame  -> completion OK, zero error;
//   2. corrupt magic      -> safe error code (STATUS_ERR_MAGIC), no accept;
//   3. bad abi / lengths  -> STATUS_ERR_ABI / STATUS_ERR_LENGTH;
//   4. small fuzz corpus  -> never hangs, never writes outside its arena
//      (counters stay consistent: accepted + rejected <= frames started,
//      bytes_consumed <= bytes fed; the model has no write port at all —
//      this asserts the shell stays inside its own counters/state).
//
// Header layout source: fpga/rtl/common/zhao_frame_pkg.sv (placeholder for
// the W4-generated zhao_abi_pkg; constants kept in sync manually in W1).

#include "Vzhao_stub_top.h"
#include "verilated.h"

#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

// status codes (zhao_frame_pkg::status_e)
constexpr uint8_t STATUS_IDLE = 0x00;
constexpr uint8_t STATUS_OK = 0x01;
constexpr uint8_t STATUS_ERR_MAGIC = 0x02;
constexpr uint8_t STATUS_ERR_ABI = 0x03;
constexpr uint8_t STATUS_ERR_LENGTH = 0x04;
// completion flags
constexpr uint8_t COMPL_DONE = 0x01;
constexpr uint8_t COMPL_ERR = 0x02;

constexpr uint32_t FRAME_MAGIC_LE = 0x314B505A;  // wire 'Z','P','K','1', LE
constexpr uint16_t ABI_VERSION = 1;
constexpr size_t HEADER_BYTES = 36;

void put16(std::vector<uint8_t>& v, uint16_t x) {
  v.push_back(static_cast<uint8_t>(x & 0xFF));
  v.push_back(static_cast<uint8_t>(x >> 8));
}
void put32(std::vector<uint8_t>& v, uint32_t x) {
  for (int i = 0; i < 4; ++i) {
    v.push_back(static_cast<uint8_t>((x >> (8 * i)) & 0xFF));
  }
}

std::vector<uint8_t> make_header(uint32_t magic = FRAME_MAGIC_LE,
                                 uint16_t abi = ABI_VERSION,
                                 uint16_t flags = 0,
                                 uint32_t frame_id = 1,
                                 uint32_t sequence = 0,
                                 uint32_t resource_epoch = 1,
                                 uint32_t deadline = 0,
                                 uint32_t command_count = 0,
                                 uint32_t command_bytes = 0,
                                 uint32_t header_crc = 0) {
  std::vector<uint8_t> v;
  v.reserve(HEADER_BYTES);
  put32(v, magic);
  put16(v, abi);
  put16(v, flags);
  put32(v, frame_id);
  put32(v, sequence);
  put32(v, resource_epoch);
  put32(v, deadline);
  put32(v, command_count);
  put32(v, command_bytes);
  put32(v, header_crc);
  return v;
}

// Feed a full frame (header [+ payload]) and return the header checksum-ish
// observed status after the model settled.
void feed_frame(Vzhao_stub_top& top, const std::vector<uint8_t>& bytes) {
  for (uint8_t b : bytes) {
    if (!zhao::send_byte(top, b)) {
      zhao::check(false, "send_byte hung (ready stalled)", 0, 1);
      return;
    }
  }
  zhao::idle(top, 4);  // let S_CHECK + completion settle
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_stub_top top;

  // ---- 1. valid empty frame -> completion OK -----------------------------
  zhao::reset(top);
  {
    auto frame = make_header();
    feed_frame(top, frame);
    zhao::check(top.status == STATUS_OK, "empty frame status", STATUS_OK, top.status);
    zhao::check((top.completion_flags & COMPL_DONE) != 0,
                "empty frame completion DONE", COMPL_DONE, top.completion_flags);
    zhao::check((top.completion_flags & COMPL_ERR) == 0,
                "empty frame no error flag", 0, top.completion_flags & COMPL_ERR);
    zhao::check(top.frames_accepted == 1, "frames_accepted", 1, top.frames_accepted);
    zhao::check(top.frames_rejected == 0, "frames_rejected", 0, top.frames_rejected);
    zhao::check(top.bytes_consumed == HEADER_BYTES, "bytes_consumed",
                HEADER_BYTES, top.bytes_consumed);
    // hdr_parity covers the ignored fields: flags=0, frame_id=1, seq=0,
    // epoch=1, deadline=0, crc=0 -> parity = XOR of all their assembled bits.
    const uint32_t words[6] = {0, 1, 0, 1, 0, 0};  // flags,fid,seq,epoch,deadline,crc
    uint8_t parity = 0;
    for (uint32_t w : words) {
      for (int i = 0; i < 32; ++i) {
        parity ^= static_cast<uint8_t>((w >> i) & 1u);
      }
    }
    zhao::check(top.hdr_parity == parity, "hdr_parity over ignored fields",
                parity, top.hdr_parity);
  }

  // ---- 2. corrupt magic -> safe error, frame rejected ---------------------
  {
    auto frame = make_header(/*magic=*/0xDEADBEEF);
    feed_frame(top, frame);
    zhao::check(top.status == STATUS_ERR_MAGIC, "bad magic status",
                STATUS_ERR_MAGIC, top.status);
    zhao::check((top.completion_flags & COMPL_ERR) != 0, "bad magic ERR flag",
                COMPL_ERR, top.completion_flags & COMPL_ERR);
    zhao::check(top.frames_accepted == 1, "accepted unchanged after reject",
                1, top.frames_accepted);
    zhao::check(top.frames_rejected == 1, "frames_rejected", 1, top.frames_rejected);
    zhao::check(top.bytes_consumed == 2 * HEADER_BYTES, "bytes_consumed",
                2 * HEADER_BYTES, top.bytes_consumed);
    if (top.status != STATUS_ERR_MAGIC) {
      zhao::save_failing_vector("stub_bad_magic", frame,
                                "status=STATUS_ERR_MAGIC",
                                "status=" + std::to_string(top.status));
    }
  }

  // ---- 3. bad abi version -------------------------------------------------
  {
    auto frame = make_header(FRAME_MAGIC_LE, /*abi=*/2);
    feed_frame(top, frame);
    zhao::check(top.status == STATUS_ERR_ABI, "bad abi status",
                STATUS_ERR_ABI, top.status);
    zhao::check(top.frames_rejected == 2, "frames_rejected", 2, top.frames_rejected);
    if (top.status != STATUS_ERR_ABI) {
      zhao::save_failing_vector("stub_bad_abi", frame,
                                "status=STATUS_ERR_ABI",
                                "status=" + std::to_string(top.status));
    }
  }

  // ---- 4. bad lengths -----------------------------------------------------
  {
    // misaligned command_bytes
    auto f1 = make_header(FRAME_MAGIC_LE, ABI_VERSION, 0, 2, 0, 1, 0, 0, 8);
    feed_frame(top, f1);
    zhao::check(top.status == STATUS_ERR_LENGTH, "misaligned length status",
                STATUS_ERR_LENGTH, top.status);

    // command_count exceeds aligned record capacity
    auto f2 = make_header(FRAME_MAGIC_LE, ABI_VERSION, 0, 3, 0, 1, 0, 2, 16);
    feed_frame(top, f2);
    zhao::check(top.status == STATUS_ERR_LENGTH, "count>capacity status",
                STATUS_ERR_LENGTH, top.status);

    // payload larger than the frame slot
    auto f3 = make_header(FRAME_MAGIC_LE, ABI_VERSION, 0, 4, 0, 1, 0, 0, 1048576 - 16);
    feed_frame(top, f3);
    zhao::check(top.status == STATUS_ERR_LENGTH, "oversize payload status",
                STATUS_ERR_LENGTH, top.status);

    zhao::check(top.frames_rejected == 5, "frames_rejected", 5, top.frames_rejected);
    zhao::check(top.frames_accepted == 1, "accepted still 1", 1, top.frames_accepted);
  }

  // ---- 5. frame WITH payload completes after exact byte count -------------
  {
    auto hdr = make_header(FRAME_MAGIC_LE, ABI_VERSION, 0, 5, 0, 1, 0, 1, 16);
    feed_frame(top, hdr);  // header only; S_CHECK -> S_PAYLOAD, not complete
    zhao::check(top.frames_accepted == 1, "not accepted before payload",
                1, top.frames_accepted);
    std::vector<uint8_t> payload(16, 0xAB);
    for (uint8_t b : payload) {
      zhao::send_byte(top, b);
    }
    zhao::idle(top, 4);
    zhao::check(top.status == STATUS_OK, "payload frame status", STATUS_OK, top.status);
    zhao::check(top.frames_accepted == 2, "payload frame accepted",
                2, top.frames_accepted);
    uint64_t expect_bytes = 7ULL * HEADER_BYTES + 16;  // 7 headers fed so far
    zhao::check(top.bytes_consumed == expect_bytes, "bytes after payload frame",
                expect_bytes, top.bytes_consumed);
  }

  // ---- 6. fuzz corpus: no hang, no arena escape ---------------------------
  {
    zhao::reset(top);
    uint32_t lcg = 0x5A175A17u;  // FORM 16 demo seed echo, arbitrary for fuzz
    const int FUZZ_BYTES = 4096;
    for (int i = 0; i < FUZZ_BYTES; ++i) {
      lcg = lcg * 1664525u + 1013904223u;
      const uint8_t b = static_cast<uint8_t>(lcg >> 24);
      if (!zhao::send_byte(top, b, /*max_wait=*/1000)) {
        zhao::check(false, "fuzz: model hung (ready stalled >1000 cycles)", 0, i);
        break;
      }
      // arena invariant: the model cannot have consumed more than fed
      if (top.bytes_consumed > static_cast<uint32_t>(i + 1)) {
        zhao::check(false, "fuzz: bytes_consumed exceeds bytes fed", i + 1,
                    top.bytes_consumed);
        break;
      }
    }
    zhao::idle(top, 8);
    // every accepted-or-rejected frame decision must account consistently
    const uint64_t decided =
        static_cast<uint64_t>(top.frames_accepted) + top.frames_rejected;
    zhao::check(decided * HEADER_BYTES <= top.bytes_consumed + 36 + 35,
                "fuzz: frame decisions imply more header bytes than consumed",
                0, decided);
    zhao::check(top.frames_accepted <= top.bytes_consumed,
                "fuzz: accepted frames without consumed bytes", 1, 0);
    std::printf("fuzz: accepted=%u rejected=%u bytes=%u\n",
                static_cast<unsigned>(top.frames_accepted),
                static_cast<unsigned>(top.frames_rejected),
                static_cast<unsigned>(top.bytes_consumed));
  }

  top.final();
  return zhao::report_and_exit("test_stub_top");
}
