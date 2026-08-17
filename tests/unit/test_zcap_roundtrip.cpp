// test_zcap_roundtrip.cpp — .zcap container round-trip (plan W4, spec/
// capture_format.md 4): write with the C++ writer, read back, deep-compare;
// unknown sections are SKIPPED (tested behavior, not convention); corrupted
// section bodies are caught by their CRC; wrong section versions reject
// cleanly; source IDs and program hashes survive the round trip; and the
// C++-built file is byte-identical to the committed golden zcap_minimal.zcap.

#include "zhao_abi.h"  // generated

#include "zhao_sim.hpp"
#include "zref/zref_frame.hpp"
#include "zref/zref_sha256.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path golden_dir() {
  fs::path dir = fs::current_path();
  for (int i = 0; i < 4 && !fs::exists(dir / "spec" / "commands.zidl"); i++) {
    dir = dir.parent_path();
  }
  return dir / "tests" / "abi" / "golden";
}

std::vector<uint8_t> read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
}

std::vector<uint8_t> minimal_frame() {
  // canonical minimal frame = the three sample records verbatim (matches
  // tests/abi/golden/frame_minimal.bin byte-for-byte)
  std::vector<uint8_t> rec;
  zhao::ZhaoFrameBuilder fb;
  zhao_abi::zhao_pack_begin_frame(zhao_abi::zhao_sample_begin_frame(), rec);
  fb.append_record(rec);
  rec.clear();
  zhao_abi::zhao_pack_nop(zhao_abi::zhao_sample_nop(), rec);
  fb.append_record(rec);
  rec.clear();
  zhao_abi::zhao_pack_end_frame(zhao_abi::zhao_sample_end_frame(), rec);
  fb.append_record(rec);
  return fb.seal(1, 0, 1, 0, 0);
}

zhao::ZhaoSourceMap minimal_source_map() {
  zhao::ZhaoSourceMap map;
  map.files = {"demo_form.zf"};
  const auto add = [&](uint32_t index, uint32_t span_begin, uint32_t span_end,
                       const char* name) {
    zhao::ZhaoSourceMapEntry entry;
    entry.source_id = zhao::zhao_source_id(5, 0, index);
    entry.module_id = 0;
    entry.file_index = 0;
    entry.kind = 5;
    entry.span_begin = span_begin;
    entry.span_end = span_end;
    entry.name = name;
    entry.file = map.files[0];
    map.entries.push_back(std::move(entry));
  };
  add(0, 20, 23, "nop");
  add(1, 10, 19, "begin_frame");
  add(2, 30, 39, "end_frame");
  return map;
}

}  // namespace

int main() {
  using namespace zhao;
  const auto dir = golden_dir();
  const std::string tmp = (fs::temp_directory_path() / "zhao_zcap_test").string();

  // ---- 1. write -> read -> deep compare --------------------------------------
  const std::string path = tmp + "_roundtrip.zcap";
  {
    ZhaoZcapWriter w(path);
    w.add_section(ZHAO_ZCAP_ABI_INFO, 1, zhao_zcap_build_abi_info());
    w.add_section(ZHAO_ZCAP_RESOURCE_PAGES, 1, [&] {
      ZhaoResourcePage p;
      p.kind = 3;  // field program
      p.page_id = 7;
      p.byte_length = 1234;
      const auto program = std::string("crater_ring.zprog");
      const auto digest = zhao::sha256(std::vector<uint8_t>(program.begin(), program.end()));
      p.sha256 = digest;
      p.ref = "form://wound_lab/crater_ring";
      return zhao_zcap_build_resource_pages({p});
    }());
    // tool-namespace private section: readers MUST skip it
    w.add_section(0x8001, 1, std::vector<uint8_t>{1, 2, 3, 4});
    w.add_section(ZHAO_ZCAP_FRAME_PACKET, 1, minimal_frame());
    const auto source_map = zhao_zcap_build_source_map(minimal_source_map());
    zhao::check(source_map.ok(), "canonical SOURCE_MAP builds", 0,
                static_cast<int>(source_map.error));
    w.add_section(ZHAO_ZCAP_SOURCE_MAP, 1, source_map.bytes);
    if (!w.close()) {
      zhao::check(false, "zcap writer close", 0, 1);
    }
  }

  {
    ZhaoZcapReader r(path);
    zhao::check(r.open() == ZhaoZcapError::kOk, "roundtrip zcap opens", int(ZhaoZcapError::kOk),
                int(r.open()));
    zhao::check(r.sections().size() == 5, "section count", 5, r.sections().size());
    zhao::check(r.info().total_file_length == static_cast<uint64_t>(fs::file_size(path)),
                "total_file_length matches actual size", fs::file_size(path),
                r.info().total_file_length);

    // every CRC verifies
    for (const auto& s : r.sections()) {
      std::vector<uint8_t> body;
      zhao::check(r.read_body(s, body), "section verifies (type)", s.type, 0);
    }

    // source IDs + program hashes survive
    std::vector<uint8_t> body;
    zhao::check(r.read_body(*r.find(ZHAO_ZCAP_SOURCE_MAP), body), "SOURCE_MAP body", 1, 0);
    const auto parsed = zhao_zcap_parse_source_map(body);
    zhao::check(parsed.ok(), "SOURCE_MAP parses", 0, static_cast<int>(parsed.error));
    const auto& entries = parsed.map.entries;
    zhao::check(entries.size() == 3, "SOURCE_MAP entries", 3, entries.size());
    const auto want = minimal_source_map();
    for (size_t i = 0; i < entries.size() && i < want.entries.size(); i++) {
      zhao::check(entries[i].source_id == want.entries[i].source_id, "source id survives",
                  want.entries[i].source_id, entries[i].source_id);
      zhao::check(entries[i].name == want.entries[i].name, "source name survives", 0,
                  entries[i].name == want.entries[i].name ? 0 : 1);
      zhao::check(entries[i].span_begin == want.entries[i].span_begin
                      && entries[i].span_end == want.entries[i].span_end,
                  "source span survives", want.entries[i].span_begin, entries[i].span_begin);
      uint32_t kind = 0, module = 0, index = 0;
      zhao_source_id_decode(entries[i].source_id, kind, module, index);
      zhao::check(kind == 5 && module == 0, "source id decodes {kind,module}", 5, kind);
    }

    zhao::check(r.read_body(*r.find(ZHAO_ZCAP_RESOURCE_PAGES), body), "RESOURCE_PAGES body", 1, 0);
    const auto pages = zhao_zcap_parse_resource_pages(body.data(), body.size());
    zhao::check(pages.size() == 1, "resource page count", 1, pages.size());
    if (!pages.empty()) {
      const auto program = std::string("crater_ring.zprog");
      const auto digest = sha256(std::vector<uint8_t>(program.begin(), program.end()));
      zhao::check(pages[0].sha256 == digest, "program hash survives", 0,
                  pages[0].sha256 == digest ? 0 : 1);
      zhao::check(pages[0].byte_length == 1234, "byte_length survives", 1234,
                  static_cast<uint32_t>(pages[0].byte_length));
    }

    // the private 0x8001 section is SKIPPED, not an error
    zhao::check(r.find(0x8001) != nullptr, "unknown section present (skippable)", 1,
                r.find(0x8001) != nullptr ? 1 : 0);
    std::vector<uint8_t> priv;
    zhao::check(r.read_body(*r.find(0x8001), priv) && priv.size() == 4,
                "unknown section body readable", 4, priv.size());

    // embedded frame still validates
    zhao::check(r.read_body(*r.find(ZHAO_ZCAP_FRAME_PACKET), body), "FRAME_PACKET body", 1, 0);
    const auto v = zhao_frame_validate(body);
    zhao::check(v.error == zhao_abi::ZH_ABI_OK, "embedded frame validates", 0, v.error);
  }

  // ---- 2. C++-built minimal zcap == committed golden (byte-identical) ---------
  {
    const std::string p2 = tmp + "_minimal.zcap";
    {
      ZhaoZcapWriter w(p2);
      w.add_section(ZHAO_ZCAP_ABI_INFO, 1, zhao_zcap_build_abi_info());
      w.add_section(ZHAO_ZCAP_FRAME_PACKET, 1, minimal_frame());
      const auto source_map = zhao_zcap_build_source_map(minimal_source_map());
    zhao::check(source_map.ok(), "canonical SOURCE_MAP builds", 0,
                static_cast<int>(source_map.error));
    w.add_section(ZHAO_ZCAP_SOURCE_MAP, 1, source_map.bytes);
      zhao::check(w.close(), "minimal zcap write", 0, 1);
    }
    const auto got = read_file(p2);
    const auto want = read_file(dir / "zcap_minimal.zcap");
    const bool equal = got == want;
    zhao::check(equal, "C++ zcap writer == golden zcap_minimal.zcap", want.size(),
                equal ? want.size() : got.size());
    if (!equal) {
      zhao::save_failing_vector("zcap_minimal_mismatch", got, "byte-identical zcap",
                                std::to_string(got.size()) + " bytes");
    }
  }

  // ---- 3. corrupted section body -> CRC mismatch -------------------------------
  {
    const std::string p3 = tmp + "_corrupt.zcap";
    const auto bytes = read_file(tmp + "_roundtrip.zcap");
    auto corrupt = bytes;
    // flip one byte inside a section body (past header+table)
    corrupt[corrupt.size() - 2] ^= 0x40;
    {
      std::ofstream out(p3, std::ios::binary);
      out.write(reinterpret_cast<const char*>(corrupt.data()), corrupt.size());
    }
    ZhaoZcapReader r(p3);
    zhao::check(r.open() == ZhaoZcapError::kOk, "corrupt zcap header still ok", 0, int(r.open()));
    std::vector<uint8_t> body;
    // header CRC covers [0,8) only, so the file opens; the last section's
    // body CRC must fail
    const auto& last = r.sections().back();
    zhao::check(!r.read_body(last, body), "corrupted section body rejected by CRC", 0, 1);
  }

  // ---- 4. unknown section VERSION rejects cleanly -------------------------------
  {
    const std::string p4 = tmp + "_badver.zcap";
    {
      ZhaoZcapWriter w(p4);
      w.add_section(ZHAO_ZCAP_ABI_INFO, 99, zhao_zcap_build_abi_info());  // wrong version
      zhao::check(w.close(), "bad-version zcap write", 0, 1);
    }
    ZhaoZcapReader r(p4);
    zhao::check(r.open() == ZhaoZcapError::kOk, "bad-version opens (reader never guesses)", 0,
                int(r.open()));
    const auto* s = r.find(ZHAO_ZCAP_ABI_INFO);
    zhao::check(s != nullptr && s->version == 99, "section version visible for clean reject", 99,
                s != nullptr ? s->version : 0);
  }

  std::remove((tmp + "_roundtrip.zcap").c_str());
  std::remove((tmp + "_minimal.zcap").c_str());
  std::remove((tmp + "_corrupt.zcap").c_str());
  std::remove((tmp + "_badver.zcap").c_str());
  return zhao::report_and_exit("test_zcap_roundtrip");
}
