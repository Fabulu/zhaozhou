// test_abi_fuzz_parity.cpp — tri-language fuzz parity + the SV pack/unpack
// byte-identity matrix (plan W4 gate (e), spec/capture_format.md 6):
//
//   1. every corpus case (tests/abi/golden/abi_corpus.zcorpus, oracle
//      expectations baked in by abi-gen): the C++ validator AND the SV
//      validator (via the Verilated probe) must return exactly the recorded
//      error code. The TS validator's agreement is asserted by the compiler
//      workspace tests over the same corpus file — three languages, one
//      corpus, identical bytes required (protobuf-conformance pattern).
//
//   2. for EVERY command, the golden record bytes are fed through the
//      probe's unpack -> pack path: the re-packed stream must equal the
//      input byte-for-byte (the plan-R1 reverse-field-order guard).
//
//   3. SV streaming CRC-32C matches the C++ table form on the corpus, and
//      the check-constant property holds in the SV engine too.

#include "Vzhao_abi_probe.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zhao_abi.h"  // generated
#include "zref/zref_frame.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path repo_root() {
  fs::path dir = fs::current_path();
  for (int i = 0; i < 4 && !fs::exists(dir / "spec" / "commands.zidl"); i++) {
    dir = dir.parent_path();
  }
  return dir;
}

std::vector<uint8_t> read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
}

uint16_t rd16(const std::vector<uint8_t>& v, size_t off) {
  return uint16_t(v[off]) | (uint16_t(v[off + 1]) << 8);
}
uint32_t rd32(const std::vector<uint8_t>& v, size_t off) {
  return uint32_t(v[off]) | (uint32_t(v[off + 1]) << 8) | (uint32_t(v[off + 2]) << 16) |
         (uint32_t(v[off + 3]) << 24);
}

/** feed one record through the probe's pu engine; returns repacked bytes (or {}) */
std::vector<uint8_t> probe_pack_roundtrip(Vzhao_abi_probe& probe,
                                          const std::vector<uint8_t>& record) {
  std::vector<uint8_t> out;
  probe.pu_out_ready = 1;
  // start: first byte in IDLE
  probe.clk = 0;
  probe.pu_valid = 1;
  probe.pu_in_data = record[0];
  probe.eval();
  zhao::tick(probe);
  probe.pu_valid = 0;
  probe.eval();
  zhao::tick(probe);
  // remaining bytes
  for (size_t i = 1; i < record.size(); i++) {
    if (!probe.pu_ready) {
      zhao::check(false, "probe pu not ready", 1, 0);
      return out;
    }
    probe.clk = 0;
    probe.pu_valid = 1;
    probe.pu_in_data = record[i];
    probe.eval();
    zhao::tick(probe);
    probe.pu_valid = 0;
    probe.eval();
    // collect any presented output byte
    if (probe.pu_out_valid) out.push_back(probe.pu_out_data);
    zhao::tick(probe);
  }
  // drain the emitted stream
  for (int guard = 0; guard < 512 && probe.pu_active; guard++) {
    if (probe.pu_out_valid) out.push_back(probe.pu_out_data);
    zhao::tick(probe);
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_abi_probe probe;
  const auto root = repo_root();

  // reset: latches fv_layout_ok
  probe.clk = 0;
  probe.rst_n = 0;
  probe.pu_valid = 0;
  probe.pu_in_data = 0;
  probe.pu_out_ready = 1;
  probe.crc_in_valid = 0;
  probe.crc_in_data = 0;
  probe.crc_seed = 0;
  probe.fv_valid = 0;
  probe.fv_data = 0;
  probe.fv_expect_len = 0;
  probe.fv_trigger = 0;
  probe.eval();
  for (int i = 0; i < 2; i++) zhao::tick(probe);
  probe.rst_n = 1;
  probe.eval();
  zhao::tick(probe);

  zhao::check(probe.fv_layout_ok == 1, "SV $bits layout self-check", 1, probe.fv_layout_ok);

  // ---- 1. per-command golden round-trip through SV unpack -> pack -------------
  {
    const struct { const char* snake; } cmds[] = {
      "nop", "begin_frame", "end_frame", "set_view", "set_presentation_contract",
      "terrain_field", "surface_stamp", "draw_form", "draw_population",
      "draw_procedural", "emit_audio_event", "debug_bootstrap",
      "draw_sky", "debug_frame_blit", "debug_rumble",
    };
    for (const auto& c : cmds) {
      const auto record = read_file(root / "tests" / "abi" / "golden" / ("cmd_" + std::string(c.snake) + ".bin"));
      zhao::check(record.size() >= 16, (std::string("golden record ") + c.snake).c_str(),
                  16, record.size());
      const auto repacked = probe_pack_roundtrip(probe, record);
      bool equal = repacked.size() == record.size();
      for (size_t i = 0; equal && i < record.size(); i++) equal = repacked[i] == record[i];
      zhao::check(equal, (std::string("SV pack/unpack byte-identity: ") + c.snake).c_str(),
                  record.size(), equal ? record.size() : repacked.size());
      zhao::check(probe.pu_mismatch == 0, (std::string("SV mismatch flag: ") + c.snake).c_str(),
                  0, probe.pu_mismatch);
    }
  }

  // ---- 2. SV streaming CRC-32C == C++ table form + check constant --------------
  {
    const std::string msg = "123456789";
    probe.crc_seed = 1;
    probe.eval();
    zhao::tick(probe);
    probe.crc_seed = 0;
    probe.eval();
    for (char ch : msg) {
      probe.clk = 0;
      probe.crc_in_valid = 1;
      probe.crc_in_data = uint8_t(ch);
      probe.eval();
      zhao::tick(probe);
      probe.crc_in_valid = 0;
      probe.eval();
    }
    const uint32_t sv_crc = ~probe.crc_reg;
    const uint32_t cpp_crc = zhao_abi::zhao_crc32c(0, msg.data(), msg.size());
    zhao::check(sv_crc == cpp_crc, "SV streaming CRC == C++ table CRC", cpp_crc, sv_crc);
    zhao::check(cpp_crc == 0xE3069283u, "check vector", 0xE3069283u, cpp_crc);
  }

  // ---- 3. corpus parity: C++ validator and SV validator vs oracle codes --------
  {
    const auto data = read_file(root / "tests" / "abi" / "golden" / "abi_corpus.zcorpus");
    zhao::check(data.size() > 12, "corpus present", 12, data.size());
    zhao::check(rd32(data, 0) == 0x524f435au, "corpus magic", 0x524f435a, rd32(data, 0));
    const uint32_t count = rd32(data, 8);
    size_t off = 12;
    uint32_t ran = 0;
    uint32_t nonzero_errors = 0;

    for (uint32_t i = 0; i < count; i++) {
      const uint16_t name_len = rd16(data, off);
      off += 2;
      const std::string name(reinterpret_cast<const char*>(data.data()) + off, name_len);
      off += name_len;
      const uint32_t pkt_len = rd32(data, off);
      off += 4;
      std::vector<uint8_t> pkt(data.begin() + off, data.begin() + off + pkt_len);
      off += pkt_len;
      const uint32_t expected = rd32(data, off);
      off += 4;

      // C++ testee
      const auto v = zhao::zhao_frame_validate(pkt);
      zhao::check(v.error == expected, ("C++ corpus: " + name).c_str(), expected, v.error);

      // SV testee: collect the packet, then strobe the validator
      probe.fv_expect_len = pkt_len;
      probe.eval();
      for (uint8_t b : pkt) {
        probe.clk = 0;
        probe.fv_valid = 1;
        probe.fv_data = b;
        probe.eval();
        zhao::tick(probe);
        probe.fv_valid = 0;
        probe.eval();
      }
      // trigger: combinational verdict during this eval
      probe.fv_trigger = 1;
      probe.eval();
      const uint32_t sv_err = probe.fv_error;
      const uint32_t sv_cmds = probe.fv_commands;
      probe.fv_trigger = 0;
      probe.eval();
      zhao::tick(probe);  // let the collector rearm

      zhao::check(sv_err == expected, ("SV corpus: " + name).c_str(), expected, sv_err);
      if (expected == 0) {
        zhao::check(sv_cmds == v.commands_consumed, ("SV commands: " + name).c_str(),
                    v.commands_consumed, sv_cmds);
      } else {
        nonzero_errors++;
      }
      if (v.error != expected || sv_err != expected) {
        zhao::save_failing_vector("fuzz_parity_" + name, pkt,
                                  "expected error=" + std::to_string(expected),
                                  "cpp=" + std::to_string(v.error) +
                                      " sv=" + std::to_string(sv_err));
      }
      ran++;
    }
    zhao::check(ran == count, "corpus cases run", count, ran);
    zhao::check(nonzero_errors > 0, "corpus contains failing cases", 1, nonzero_errors);
    std::printf("corpus: %u cases, %u with expected errors\n", ran, nonzero_errors);
  }

  probe.final();
  return zhao::report_and_exit("test_abi_fuzz_parity");
}
