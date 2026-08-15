// test_empty_frame_replay.cpp — the W4 differential gate (plan W4, charter
// Phase-1 "one empty frame replays through ZRef and a stub RTL model"):
//
//   1. build BeginFrame/Nop/EndFrame in C++ and byte-compare against the
//      committed golden (the TS testee already proved TS == same golden in
//      the compiler workspace — that IS the C++/TS byte-comparison);
//   2. replay the packet through the empty ZRef frame loop AND the
//      Verilated stub (now importing the generated zhao_abi_pkg): identical
//      status / completion flags / counters;
//   3. corrupted header-CRC, payload-CRC and reserved-field cases: identical
//      error codes from the C++ validator and the SV validator (the TS
//      validator's agreement over the full corpus is proven by the compiler
//      tests + test_abi_fuzz_parity);
//   4. .zcap write -> read -> resolve: source IDs and program hashes intact.
//
// Driving rule (matches test_stub_top): header-level aborts feed only the
// 36-byte header (the stub consumes exactly 36 bytes on those, per
// capture_format.md 3.2); payload-level verdicts feed the whole packet.

#include "Vzhao_stub_top.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zhao_abi.h"  // generated
#include "zref/zref_frame.hpp"
#include "zref/zref_sha256.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

using namespace zhao_abi;

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

std::vector<uint8_t> minimal_frame() {
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

void feed(Vzhao_stub_top& top, const std::vector<uint8_t>& bytes) {
  for (uint8_t b : bytes) {
    if (!zhao::send_byte(top, b)) {
      zhao::check(false, "stub feed hung", 0, 1);
      return;
    }
  }
  zhao::idle(top, 4);
}

std::vector<uint8_t> resealed(std::vector<uint8_t> pkt) {
  const uint32_t command_bytes = uint32_t(pkt[28]) | (uint32_t(pkt[29]) << 8) |
                                 (uint32_t(pkt[30]) << 16) | (uint32_t(pkt[31]) << 24);
  const auto put32 = [&](uint32_t off, uint32_t v) {
    for (int i = 0; i < 4; i++) pkt[off + i] = uint8_t(v >> (8 * i));
  };
  put32(ZHAO_OFF_HEADER_CRC, zhao_crc32c(0, pkt.data(), 32));
  put32(ZHAO_FRAME_HEADER_BYTES + command_bytes,
        zhao_crc32c(0, pkt.data() + ZHAO_FRAME_HEADER_BYTES, command_bytes));
  return pkt;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_stub_top top;
  const auto root = repo_root();
  const auto frame = minimal_frame();

  // ---- 1. C++-built packet == committed golden (== TS-built, proven in npm) --
  {
    const auto golden = read_file(root / "tests" / "abi" / "golden" / "frame_minimal.bin");
    zhao::check(frame == golden, "C++ minimal frame == golden frame_minimal.bin", golden.size(),
                frame.size());
  }

  // ---- 2. replay: ZRef executor vs Verilated stub -----------------------------
  {
    zhao::reset(top);
    feed(top, frame);

    const auto zref = zhao::zhao_frame_execute_empty(frame.data(), frame.size());
    zhao::check(zref.status == top.status, "replay status parity", zref.status, top.status);
    zhao::check(zref.completion_flags == top.completion_flags, "replay completion parity",
                zref.completion_flags, top.completion_flags);
    zhao::check(zref.counters.frames_accepted == top.frames_accepted,
                "replay frames_accepted parity", zref.counters.frames_accepted,
                top.frames_accepted);
    zhao::check(zref.counters.frames_rejected == top.frames_rejected,
                "replay frames_rejected parity", zref.counters.frames_rejected,
                top.frames_rejected);
    zhao::check(zref.counters.bytes_consumed == top.bytes_consumed, "replay bytes_consumed parity",
                zref.counters.bytes_consumed, top.bytes_consumed);
    zhao::check(zref.counters.commands_total == top.commands_consumed, "replay commands parity",
                zref.counters.commands_total, top.commands_consumed);
    // and the specific Phase-1 expectations
    zhao::check(zref.status == ZH_ABI_OK, "replay accepted", ZH_ABI_OK, zref.status);
    zhao::check(
        zref.counters.begin_frames == 1 && zref.counters.nops == 1 && zref.counters.end_frames == 1,
        "replay command counters", 1, zref.counters.nops);
  }

  // ---- 3. corrupted / reserved cases: C++ validator vs SV stub, error parity --
  struct Case {
    const char* name;
    std::vector<uint8_t> pkt;    // what the STUB is fed
    std::vector<uint8_t> whole;  // what ZRef validates (full packet)
  };
  std::vector<Case> cases;
  {
    auto bad_hcrc = frame;
    bad_hcrc[33] ^= 0x40;  // corrupt header CRC word (no re-seal)
    bad_hcrc.resize(36);
    cases.push_back({"bad header crc", bad_hcrc, [&] {
                       auto w = frame;
                       w[33] ^= 0x40;
                       return w;
                     }()});
  }
  {
    auto bad_pcrc = frame;
    bad_pcrc[bad_pcrc.size() - 1] ^= 0x80;
    cases.push_back({"bad payload crc", bad_pcrc, bad_pcrc});
  }
  {
    // nonzero record-header reserved0 (EndFrame record @ stream 48): mutate,
    // re-seal so CRCs pass and the walk reaches the reserved-field check
    auto r = frame;
    r[ZHAO_FRAME_HEADER_BYTES + 48 + 12] = 0x01;
    r = resealed(r);
    cases.push_back({"record reserved0 nonzero", r, r});
  }
  {
    // nonzero payload pad byte (EndFrame pad[4] @ payload +12)
    auto r = frame;
    r[ZHAO_FRAME_HEADER_BYTES + 48 + 16 + 12] = 0x42;
    r = resealed(r);
    cases.push_back({"payload pad nonzero", r, r});
  }
  {
    // record-header flags nonzero (BeginFrame @ stream 0)
    auto r = frame;
    r[ZHAO_FRAME_HEADER_BYTES + 0 + 8] = 0x01;
    r = resealed(r);
    cases.push_back({"record flags nonzero", r, r});
  }

  for (const auto& c : cases) {
    zhao::reset(top);
    feed(top, c.pkt);
    const auto zref = zhao::zhao_frame_validate(c.whole);
    zhao::check(zref.error == top.status, c.name, zref.error, top.status);
    // both sides must also agree the frame was REJECTED
    zhao::check(top.frames_rejected == 1 && zref.error != ZH_ABI_OK, c.name, 1,
                top.frames_rejected);
    // W6 conformance pin: bytes_consumed parity on REJECTED frames too
    // (spec 3.2: 36 on header-level abort, else the whole packet). This
    // check was missing in W4, which let the C++ validator report 0 on
    // header-level aborts while the stub reported 36.
    zhao::check(zref.bytes_consumed == top.bytes_consumed, c.name, zref.bytes_consumed,
                top.bytes_consumed);
    if (zref.error != top.status) {
      zhao::save_failing_vector("replay_error_parity", c.whole,
                                "zref error=" + std::to_string(zref.error),
                                "stub status=" + std::to_string(top.status));
    }
  }

  // ---- 4. .zcap round-trip preserves source IDs + program hashes ---------------
  {
    const std::string tmp = (fs::temp_directory_path() / "zhao_replay.zcap").string();
    const auto program = std::string("crater_ring.zprog");
    const auto digest = zhao::sha256(std::vector<uint8_t>(program.begin(), program.end()));

    {
      zhao::ZhaoZcapWriter w(tmp);
      w.add_section(zhao::ZhaoZcapSection::ZHAO_ZCAP_ABI_INFO, 1, zhao::zhao_zcap_build_abi_info());
      w.add_section(zhao::ZhaoZcapSection::ZHAO_ZCAP_FRAME_PACKET, 1, frame);
      w.add_section(zhao::ZhaoZcapSection::ZHAO_ZCAP_SOURCE_MAP, 1, [&] {
        std::vector<zhao::ZhaoSourceMapEntry> entries;
        entries.push_back(
            {zhao::zhao_source_id(5, 1, 1), 1, 5, 0, 10, "begin_frame", "demo_form.zf"});
        entries.push_back({zhao::zhao_source_id(5, 1, 0), 1, 5, 0, 20, "nop", "demo_form.zf"});
        entries.push_back(
            {zhao::zhao_source_id(5, 1, 2), 1, 5, 0, 30, "end_frame", "demo_form.zf"});
        return zhao::zhao_zcap_build_source_map(entries);
      }());
      zhao::ZhaoResourcePage page;
      page.kind = 3;
      page.page_id = 7;
      page.byte_length = 4321;
      page.sha256 = digest;
      page.ref = "form://wound_lab/crater_ring";
      w.add_section(zhao::ZhaoZcapSection::ZHAO_ZCAP_RESOURCE_PAGES, 1,
                    zhao::zhao_zcap_build_resource_pages({page}));
      zhao::check(w.close(), "replay zcap write", 0, 1);
    }

    zhao::ZhaoZcapReader r(tmp);
    zhao::check(r.open() == zhao::ZhaoZcapError::kOk, "replay zcap read", 0, int(r.open()));
    std::vector<uint8_t> body;

    // source-map resolution: every command source_id in the frame resolves
    zhao::check(r.read_body(*r.find(zhao::ZhaoZcapSection::ZHAO_ZCAP_SOURCE_MAP), body),
                "replay SOURCE_MAP", 1, 0);
    const auto entries = zhao::zhao_zcap_parse_source_map(body.data(), body.size());
    std::vector<uint32_t> resolved;
    for (const auto& e : entries) resolved.push_back(e.source_id);
    const uint32_t want_ids[3] = {zhao::zhao_source_id(5, 1, 1), zhao::zhao_source_id(5, 1, 0),
                                  zhao::zhao_source_id(5, 1, 2)};
    for (uint32_t want : want_ids) {
      bool found = false;
      for (uint32_t got : resolved) found = found || (got == want);
      zhao::check(found, "source id resolves through capture", want, found ? want : 0);
    }

    // program hash intact
    zhao::check(r.read_body(*r.find(zhao::ZhaoZcapSection::ZHAO_ZCAP_RESOURCE_PAGES), body),
                "replay RESOURCE_PAGES", 1, 0);
    const auto pages = zhao::zhao_zcap_parse_resource_pages(body.data(), body.size());
    zhao::check(pages.size() == 1, "replay resource page", 1, pages.size());
    if (!pages.empty()) {
      zhao::check(pages[0].sha256 == digest, "program hash survives capture", 0,
                  pages[0].sha256 == digest ? 0 : 1);
    }

    // embedded frame replays identically after the round trip
    zhao::check(r.read_body(*r.find(zhao::ZhaoZcapSection::ZHAO_ZCAP_FRAME_PACKET), body),
                "replay FRAME_PACKET", 1, 0);
    zhao::check(body == frame, "embedded frame unchanged by capture", frame.size(), body.size());
    std::remove(tmp.c_str());
  }

  top.final();
  return zhao::report_and_exit("test_empty_frame_replay");
}
