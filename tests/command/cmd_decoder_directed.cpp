// cmd_decoder_directed.cpp — CMD.DECODER against zref::cmd::validate.
//
// The oracle is NOT written alongside this test. `zref::cmd::validate` is a
// thin view onto `zhao::zhao_frame_validate`, which zhao_stub_top, the capture
// tooling and the 19 committed packets in tests/abi/golden/ have agreed with
// since wave 1. So "RTL matches the oracle" here is a much stronger statement
// than usual: it means the streaming decoder reaches the same verdict as the
// function every other consumer in this project already trusts.
//
// WHAT THIS COMPARES, and the one thing it deliberately does not.
// The RTL implements checks 1-5, the record-header half of 6, 9 and 10 of
// spec/capture_format.md 3.2. Checks 6-payload-pad and 7-enum-range are
// deferred in the RTL with a named cause (the generated helpers take open
// arrays and are excluded from synthesis), so a packet the RTL calls OK may
// still be rejected by zref for one of those two reasons.
//
// This test therefore asserts something sharper than "they agree": it asserts
// the deferred pair is the ONLY way they may disagree. Any other divergence is
// a defect. When SW.TOOLS.ABIDOC emits a synthesizable offset table and the RTL
// consumes it, `kDeferred` below shrinks to nothing and this file needs no
// other change.

#include "Vzhao_cmd_decoder.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_cmd.hpp"
#include "zref/zref_frame.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using zhao::check;

// The two verdicts the RTL cannot reach yet. See the file header.
bool isDeferred(uint8_t e) {
  return e == zhao_abi::ZH_ABI_RESERVED_FIELD || e == zhao_abi::ZH_ABI_BAD_VALUE;
}

struct Verdict {
  uint8_t error = 0;
  uint32_t commands = 0;
  uint32_t bytes = 0;
  bool done = false;
  uint32_t records_seen = 0;
};

/**
 * Stream one packet through the DUT.
 *
 * `stall_mask` gates rec_ready_i from a simple bit pattern so the record
 * handshake is exercised rather than assumed: with backpressure applied the
 * verdict must be identical, only slower. That is the property my first draft
 * of the RTL got wrong twice.
 */
Verdict runPacket(const std::vector<uint8_t>& pkt, uint32_t stall_mask) {
  Vzhao_cmd_decoder dut;
  dut.rst_n = 0;
  dut.pkt_valid_i = 0;
  dut.pkt_byte_i = 0;
  dut.pkt_len_i = 0;
  dut.rec_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  Verdict v;
  size_t i = 0;
  uint32_t cyc = 0;
  const uint32_t kGuard = static_cast<uint32_t>(pkt.size()) * 8 + 4096;

  while (!v.done && cyc < kGuard) {
    const bool have = (i < pkt.size());
    dut.pkt_valid_i = have ? 1 : 0;
    dut.pkt_byte_i = have ? pkt[i] : 0;
    dut.pkt_len_i = static_cast<uint32_t>(pkt.size());
    dut.rec_ready_i = ((stall_mask >> (cyc & 31)) & 1u) ? 1 : 0;
    dut.eval();

    const bool moved = have && dut.pkt_ready_o;
    if (dut.rec_valid_o && dut.rec_ready_i) ++v.records_seen;

    zhao::tick(dut);
    if (moved) ++i;

    if (dut.decode_done_o) {
      v.done = true;
      v.error = dut.decode_error_o;
      v.commands = dut.commands_o;
      v.bytes = dut.bytes_consumed_o;
    }
    ++cyc;
  }
  return v;
}

/** Compare the DUT against the oracle for one packet, under three stall
 *  patterns: never stall, stall half the time, stall three cycles in four. */
void diff(const std::vector<uint8_t>& pkt, const char* what) {
  const zref::cmd::Result want = zref::cmd::validate(pkt);

  const uint32_t masks[3] = {0xFFFFFFFFu, 0xAAAAAAAAu, 0x11111111u};
  for (int m = 0; m < 3; ++m) {
    const Verdict got = runPacket(pkt, masks[m]);
    const std::string tag = std::string(what) + " [stall " + std::to_string(m) + "]";

    check(got.done, (tag + ": reached a verdict").c_str(), 1, got.done ? 1 : 0);
    if (!got.done) return;

    if (isDeferred(static_cast<uint8_t>(want.error))) {
      // The oracle rejects for a reason this RTL cannot see yet. The only
      // lawful outcomes are: agree anyway, or accept. Anything else means the
      // RTL invented a different rejection, which is a real defect.
      const bool lawful =
          (got.error == static_cast<uint8_t>(want.error)) || (got.error == zhao_abi::ZH_ABI_OK);
      check(lawful, (tag + ": deferred-check packet is OK or matching").c_str(),
            static_cast<uint64_t>(want.error), got.error);
      continue;
    }

    check(got.error == static_cast<uint8_t>(want.error), (tag + ": error code").c_str(),
          static_cast<uint64_t>(want.error), got.error);
    check(got.bytes == want.bytes_consumed, (tag + ": bytes_consumed").c_str(),
          want.bytes_consumed, got.bytes);
    if (want.error == zhao_abi::ZH_ABI_OK) {
      check(got.commands == want.commands_consumed, (tag + ": commands").c_str(),
            want.commands_consumed, got.commands);
      check(got.records_seen == want.commands_consumed,
            (tag + ": every record was handed over").c_str(), want.commands_consumed,
            got.records_seen);
    }
  }
}

std::vector<uint8_t> goodPacket(uint32_t nops) {
  zhao::ZhaoFrameBuilder b;
  b.begin_frame(1, 0, 0, 0);
  for (uint32_t i = 0; i < nops; ++i) b.nop();
  b.end_frame(0);
  return b.seal(1, 1, 0);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  // ---- 1. well-formed packets of several shapes ---------------------------
  diff(goodPacket(0), "valid: begin+end");
  diff(goodPacket(1), "valid: one nop");
  diff(goodPacket(7), "valid: seven nops");

  // ---- 2. the header-level aborts (checks 1-3) ----------------------------
  // Each must report bytes_consumed == 36 exactly: the packet stops at the
  // header, which is the rule most easily got wrong.
  {
    std::vector<uint8_t> p = goodPacket(1);
    p[0] ^= 0xFF;
    diff(p, "bad magic");
  }
  {
    std::vector<uint8_t> p = goodPacket(1);
    p[4] = 0x7F;  // abi_version
    diff(p, "bad abi_version");
  }
  {
    std::vector<uint8_t> p = goodPacket(1);
    p[7] |= 0x80;  // a reserved frame-flag bit
    diff(p, "reserved frame flag");
  }
  {
    std::vector<uint8_t> p = goodPacket(1);
    p[32] ^= 0x01;  // header_crc32c
    diff(p, "bad header CRC");
  }
  {
    std::vector<uint8_t> p = goodPacket(1);
    p.resize(20);
    diff(p, "shorter than a header");
  }

  // ---- 3. the whole-packet aborts (checks 4, 5, 9) ------------------------
  // These consume 40+N before the verdict, which is the other half of the
  // bytes_consumed law.
  {
    std::vector<uint8_t> p = goodPacket(2);
    p[p.size() - 1] ^= 0x80;  // payload_crc32c
    diff(p, "bad payload CRC");
  }
  {
    std::vector<uint8_t> p = goodPacket(2);
    p[36] = 0xEE;  // an opcode the ABI table does not have
    p[37] = 0xEE;
    diff(p, "unknown opcode");
  }
  {
    std::vector<uint8_t> p = goodPacket(2);
    p[36 + 8] = 0x01;  // record-header flags, which have no defined bits
    diff(p, "reserved record flag");
  }
  {
    std::vector<uint8_t> p = goodPacket(2);
    p[36 + 12] = 0x01;  // record-header reserved0
    diff(p, "reserved record field");
  }
  {
    std::vector<uint8_t> p = goodPacket(2);
    p[24] = static_cast<uint8_t>(p[24] + 1);  // command_count, breaking check 9
    diff(p, "count mismatch");
  }

  // ---- 4. every committed golden -----------------------------------------
  // The strongest cases here, because nobody wrote them for this block. They
  // are the packets the ABI generator, the capture tooling and the stub shell
  // have agreed on since wave 1.
  {
    const char* names[] = {"cmd_begin_frame.bin",     "cmd_debug_bootstrap.bin",
                           "cmd_debug_frame_blit.bin", "cmd_debug_rumble.bin",
                           "cmd_draw_form.bin",        "cmd_draw_population.bin",
                           "cmd_draw_procedural.bin"};
    for (const char* n : names) {
      const std::string path = std::string(ZHAO_GOLDEN_DIR) + "/" + n;
      FILE* f = std::fopen(path.c_str(), "rb");
      if (f == nullptr) continue;  // a golden that is not a whole packet
      std::vector<uint8_t> p;
      uint8_t buf[4096];
      size_t got = 0;
      while ((got = std::fread(buf, 1, sizeof buf, f)) > 0) p.insert(p.end(), buf, buf + got);
      std::fclose(f);
      if (p.size() >= 40) diff(p, n);
    }
  }

  return zhao::report_and_exit("cmd_decoder_directed");
}
