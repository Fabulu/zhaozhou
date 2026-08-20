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
#include <cstdlib>
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

// PCG RXS-M-XS, the committed test PRNG shape (qformats.md 7.5) that every
// other random lane in this tree uses.
struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t x = s;
    s = x * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((x >> 22) ^ x) >> 29);
    const uint32_t v = (static_cast<uint32_t>(x >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

std::vector<uint8_t> goodPacket(uint32_t nops) {
  zhao::ZhaoFrameBuilder b;
  b.begin_frame(1, 0, 0, 0);
  for (uint32_t i = 0; i < nops; ++i) b.nop();
  b.end_frame(0);
  return b.seal(1, 1, 0);
}

}  // namespace

/**
 * The random lane. A well-formed packet is built, then with high probability a
 * SINGLE byte somewhere in it is corrupted, and the verdict is compared.
 *
 * Corrupting one byte at a uniformly random offset is the point: it lands in
 * the magic, the version, a length, a CRC word, a record header or a payload
 * with no bias, and the oracle decides what that should mean. Constructing
 * "interesting" corruptions by hand would only ever test the failures I had
 * already thought of -- and the ordering defect this file found on its first
 * run was one I had not.
 */
int randomLane(uint32_t iters, uint64_t seed) {
  Prng rng(seed);
  for (uint32_t k = 0; k < iters; ++k) {
    std::vector<uint8_t> p = goodPacket(rng.below(6));
    char tag[96];
    if (rng.below(8) != 0) {
      const uint32_t off = rng.below(static_cast<uint32_t>(p.size()));
      const uint8_t was = p[off];
      p[off] = static_cast<uint8_t>(p[off] ^ (1u << rng.below(8)));
      std::snprintf(tag, sizeof tag, "random[%u] flip @%u (0x%02X->0x%02X)", k, off, was, p[off]);
    } else {
      std::snprintf(tag, sizeof tag, "random[%u] clean", k);
    }
    diff(p, tag);
    if (zhao::check_failures() != 0) return 1;  // stop at the first divergence
  }
  return 0;
}

int main(int argc, char** argv) {
  // --random N runs the differential over N generated packets instead of the
  // directed cases; the fast lane uses a small N and nightly a large one.
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      const uint32_t n = static_cast<uint32_t>(std::atoi(argv[i + 1]));
      randomLane(n, 0x5A17C0DEULL);
      return zhao::report_and_exit("cmd_decoder_random");
    }
  }

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

  // ---- 3b. boundaries that must be CONSTRUCTED ---------------------------
  // A mutation sweep removing the `record_bytes >= 16` guard SURVIVED both
  // lanes, which means nothing here was building a record too small to hold
  // its own header. Uniform random never finds this: it flips bits in
  // well-formed packets, and the only multiple of 16 below 16 is zero, which
  // no single bit flip of 0x0010 produces alongside a valid CRC. This project
  // has now learned the same lesson six times -- exact-equality boundaries
  // have to be built on purpose.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    std::vector<uint8_t> bad(16, 0);
    bad[0] = 0x00; bad[1] = 0x00;   // opcode NOP
    bad[2] = 0x00; bad[3] = 0x00;   // record_bytes = 0: a multiple of 16, and
                                    // smaller than the 16-byte record header
    b.append_record(bad);
    b.end_frame(0);
    diff(b.seal(1, 1, 0), "record_bytes == 0");
  }
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    std::vector<uint8_t> bad(16, 0);
    bad[2] = 0x08;                  // record_bytes = 8: below 16 AND not a
                                    // multiple of 16, the other side of the guard
    b.append_record(bad);
    b.end_frame(0);
    diff(b.seal(1, 1, 0), "record_bytes == 8");
  }

  // ---- 3c. two more boundaries that mutations proved were unreachable -----
  // A sweep removing check 10 (the debug-flag gate) and check 9's count law
  // BOTH survived. Neither is an equivalent mutation; both were simply never
  // reached, and for instructive reasons.
  {
    // Check 10 needs a debug-umbrella opcode (0xF000-0xF0FF) in a frame whose
    // flags bit0 is CLEAR. Nothing above used one at all, so the whole gate was
    // dead code as far as the suite was concerned.
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    std::vector<uint8_t> rec(32, 0);
    rec[0] = 0x04; rec[1] = 0xF0;    // ZHAO_OP_DEBUG_RUMBLE = 0xF004
    rec[2] = 32;   rec[3] = 0;       // its ABI size, so check 5 passes cleanly
    b.append_record(rec);
    b.end_frame(0);
    diff(b.seal(1, 1, 0, 0, /*flags=*/0), "debug opcode without the debug flag");
    // ...and the same packet WITH the flag set must be accepted, so the test
    // pins the gate in both directions rather than only the failing one.
    diff(b.seal(1, 1, 0, 0, /*flags=*/1), "debug opcode with the debug flag");
  }
  {
    // Check 9's count law was unreachable because the obvious way to break it
    // -- editing command_count in the header -- also breaks header_crc32c, and
    // check 3 fires first. The count must be made wrong while the header CRC
    // stays RIGHT, which means resealing the header after the edit.
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.nop();
    b.end_frame(0);
    std::vector<uint8_t> p = b.seal(1, 1, 0);
    p[24] = static_cast<uint8_t>(p[24] + 1);            // one record too many
    const uint32_t c = zhao_abi::zhao_crc32c(0, p.data(), 32);
    p[32] = static_cast<uint8_t>(c);
    p[33] = static_cast<uint8_t>(c >> 8);
    p[34] = static_cast<uint8_t>(c >> 16);
    p[35] = static_cast<uint8_t>(c >> 24);
    diff(p, "count mismatch with a VALID header CRC");
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
