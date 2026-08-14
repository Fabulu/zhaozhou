// zemu_main.cpp — the ZEmu desktop emulator stub main (charter §23 Phase-1
// build list item "empty ZEmu"; charter §20.2: ZEmu loads a cartridge and
// runs the complete console on desktop — NONE of that exists in Phase 1).
//
// Phase-1 scope (plan W6): a documented-EMPTY shell that links the ZRef
// library and replays sealed frame packets through the empty-frame shell
// loop (reference/src/zref.cpp, spec/capture_format.md §3.2-3.3). Its job
// today is to prove the product target exists, links and exits 0 on the
// golden frame — the cartridge loader, tile parallelism and debug views are
// later-phase scope and deliberately absent (charter §20.2's "may not
// silently switch renderers" has nothing to switch yet).
//
// Usage: zemu <frame-packet.bin> [...]
//   Each file is one sealed frame packet (or a debug-umbrella frame).
//   Exit code: 0 iff every frame validated AND executed green.

#include "zref/zref.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> read_file(const char* path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
}

const char* completion_name(uint8_t flags) {
  if ((flags & zhao_abi::ZHAO_COMPL_ERR) != 0) return "ERR";
  if ((flags & zhao_abi::ZHAO_COMPL_DONE) != 0) return "DONE";
  return "PENDING";
}

}  // namespace

int main(int argc, char** argv) {
  std::printf("ZEmu — Zhaozhou desktop emulator (STUB, charter §23 Phase-1 "
              "build list: 'empty ZEmu')\n");

  if (argc < 2) {
    std::fprintf(stderr,
                 "usage: %s <sealed-frame-packet.bin> [...]\n"
                 "  Phase-1 stub: replays each packet through the empty ZRef\n"
                 "  shell and exits 0 iff all frames replay green.\n",
                 argv[0]);
    return 2;
  }

  zhao::ZhaoZrefShell shell;
  bool all_green = true;
  for (int i = 1; i < argc; i++) {
    const std::vector<uint8_t> pkt = read_file(argv[i]);
    if (pkt.empty()) {
      std::fprintf(stderr, "zemu: cannot read %s\n", argv[i]);
      all_green = false;
      continue;
    }
    const zhao::ZhaoExecutionResult r = shell.submit(pkt);
    std::printf("  %s: status=%u completion=%s bytes=%u commands=%u "
                "(begin=%u nop=%u end=%u)\n",
                argv[i], r.status, completion_name(r.completion_flags),
                r.counters.bytes_consumed, r.counters.commands_total,
                r.counters.begin_frames, r.counters.nops, r.counters.end_frames);
    if (r.status != zhao_abi::ZH_ABI_OK ||
        r.completion_flags != zhao_abi::ZHAO_COMPL_DONE) {
      all_green = false;
    }
  }

  const zhao::ZhaoShellCounters& s = shell.session();
  std::printf("session: %u submitted, %u accepted, %u rejected, %u commands, "
              "%u bytes\n",
              s.frames_submitted, s.frames_accepted, s.frames_rejected,
              s.commands_total, s.bytes_consumed);
  std::printf(all_green ? "ZEmu stub: all frames green\n"
                        : "ZEmu stub: frame errors present\n");
  return all_green ? 0 : 1;
}
