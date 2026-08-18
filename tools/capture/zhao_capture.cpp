// zhao_capture.cpp — .zcap CLI (plan W4, SW.TOOLS.CAPTURE, spec/
// capture_format.md 4). Subcommands:
//   info   <file.zcap>           dump header + section table
//   verify <file.zcap>           verify header CRC + every section CRC +
//                                validate each FRAME_PACKET section; nonzero
//                                exit on any failure
//   write  <out.zcap> <frame.bin> minimal capture: ABI_INFO + FRAME_PACKET
// Exit codes: 0 ok, 1 verification failure, 2 usage/IO error.

#include "zhao_abi.h"  // generated

#include "zref/zref_frame.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int usage() {
  std::fprintf(stderr,
               "usage: zhao-capture info   <file.zcap>\n"
               "       zhao-capture verify <file.zcap>\n"
               "       zhao-capture write  <out.zcap> <frame.bin>\n");
  return 2;
}

std::vector<uint8_t> read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
}

const char* section_name(uint16_t type) {
  switch (type) {
    case 0x0001:
      return "ABI_INFO";
    case 0x0002:
      return "FRAME_PACKET";
    case 0x0003:
      return "RESOURCE_PAGES";
    case 0x0004:
      return "CONTROLLER_SNAPSHOT";
    case 0x0005:
      return "FRAMEBUFFER_EXPECTED";
    case 0x0006:
      return "TILE_CRC";
    case 0x0007:
      return "DEPTH_STENCIL_CRC";
    case 0x0008:
      return "COUNTERS";
    case 0x0009:
      return "SOURCE_MAP";
    case 0x000A:
      return "TRACE";
    case 0x000B:  // [v3] stars_and_flares 8 (capture_format.md 4.2)
      return "CELESTIAL_STATE";
    case 0x000C:  // [v3] sky_and_beams 4a
      return "ENVIRONMENT_STATE";
    default:
      return (type >= 0x8000) ? "private" : "unknown";
  }
}

int cmd_info(const std::string& path) {
  zhao::ZhaoZcapReader r(path);
  const auto err = r.open();
  if (err != zhao::ZhaoZcapError::kOk) {
    std::fprintf(stderr, "error: cannot open/validate %s (err=%d)\n", path.c_str(), int(err));
    return 2;
  }
  std::printf("%s: format_version=%u sections=%llu total=%llu B\n", path.c_str(),
              r.info().format_version, static_cast<unsigned long long>(r.sections().size()),
              static_cast<unsigned long long>(r.info().total_file_length));
  for (const auto& s : r.sections()) {
    std::printf("  type=0x%04X %-22s ver=%u crc=%s off=%llu len=%llu\n", s.type,
                section_name(s.type), s.version, (s.flags & 0x0001) ? "present" : "none",
                static_cast<unsigned long long>(s.body_offset),
                static_cast<unsigned long long>(s.body_length));
  }
  return 0;
}

int cmd_verify(const std::string& path) {
  zhao::ZhaoZcapReader r(path);
  const auto err = r.open();
  if (err != zhao::ZhaoZcapError::kOk) {
    std::fprintf(stderr, "error: cannot open/validate %s (err=%d)\n", path.c_str(), int(err));
    return 1;
  }
  int failures = 0;
  std::vector<uint8_t> body;
  for (const auto& s : r.sections()) {
    if (!r.read_body(s, body)) {
      std::fprintf(stderr, "FAIL: section type=0x%04X (%s) at offset %llu: %s\n", s.type,
                   section_name(s.type), static_cast<unsigned long long>(s.body_offset),
                   r.error().c_str());
      failures++;
      continue;  // never trust contents; keep checking the remaining sections
    }
    if (s.type == zhao::ZhaoZcapSection::ZHAO_ZCAP_FRAME_PACKET) {
      const auto v = zhao::zhao_frame_validate(body);
      if (v.error != zhao_abi::ZH_ABI_OK) {
        std::fprintf(stderr, "FAIL: embedded frame packet: error %d (%s)\n", int(v.error),
                     zhao_abi::ZH_ABI_OK == v.error ? "" : "invalid");
        failures++;
        continue;
      }
      std::printf("ok: frame packet (%u commands, %u B stream)\n", v.commands_consumed,
                  static_cast<unsigned>(body.size() - zhao_abi::ZHAO_FRAME_OVERHEAD));
    }
  }
  if (failures == 0) {
    std::printf("%s: all %llu sections verified\n", path.c_str(),
                static_cast<unsigned long long>(r.sections().size()));
    return 0;
  }
  std::fprintf(stderr, "%s: %d section(s) FAILED\n", path.c_str(), failures);
  return 1;
}

int cmd_write(const std::string& out, const std::string& frame_path) {
  const auto frame = read_file(frame_path);
  if (frame.empty()) {
    std::fprintf(stderr, "error: cannot read frame packet %s\n", frame_path.c_str());
    return 2;
  }
  const auto v = zhao::zhao_frame_validate(frame);
  if (v.error != zhao_abi::ZH_ABI_OK) {
    std::fprintf(stderr, "error: %s is not a valid sealed packet (error %d)\n", frame_path.c_str(),
                 int(v.error));
    return 2;
  }
  zhao::ZhaoZcapWriter w(out);
  w.add_section(zhao::ZhaoZcapSection::ZHAO_ZCAP_ABI_INFO, 1, zhao::zhao_zcap_build_abi_info());
  w.add_section(zhao::ZhaoZcapSection::ZHAO_ZCAP_FRAME_PACKET, 1, frame);
  if (!w.close() || !w.error().empty()) {
    std::fprintf(stderr, "error: %s\n", w.error().c_str());
    return 2;
  }
  std::printf("wrote %s (1 frame packet, %u commands)\n", out.c_str(), v.commands_consumed);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) return usage();
  const std::string cmd = argv[1];
  const std::string path = argv[2];
  if (cmd == "info") return cmd_info(path);
  if (cmd == "verify") return cmd_verify(path);
  if (cmd == "write") {
    if (argc < 4) return usage();
    return cmd_write(path, argv[3]);
  }
  return usage();
}
