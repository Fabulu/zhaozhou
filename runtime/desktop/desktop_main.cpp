// desktop_main.cpp — the ZSDK desktop runtime product stub main (charter
// §3.5 ZSDK: runtime, tools and assets). Like ZEmu, the charter §23
// Phase-1 build list delivers this product EMPTY; unlike ZEmu (§20.2) the
// desktop runtime has no charter-mandated Phase-1 behavior beyond existing
// and linking — so this main hosts the same empty ZRef shell as its frame
// loop, prints a documented-empty banner, and exits 0 on a green replay of
// the frames it is handed.
//
// Usage: zhao-desktop <sealed-frame-packet.bin> [...]
//   Exit code: 0 iff every frame validated AND executed green.

#include "zref/zref.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {

std::vector<uint8_t> read_file(const char* path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
}

}  // namespace

int main(int argc, char** argv) {
  std::printf("zhao-desktop — Zhaozhou desktop runtime (STUB, charter §23 "
              "Phase-1: empty products; the SDK surfaces land Phase 2+)\n");

  if (argc < 2) {
    std::fprintf(stderr,
                 "usage: %s <sealed-frame-packet.bin> [...]\n"
                 "  Phase-1 stub: replays each packet through the empty ZRef\n"
                 "  shell (the runtime's frame loop) and exits 0 iff green.\n",
                 argv[0]);
    return 2;
  }

  zhao::ZhaoZrefShell shell;
  bool all_green = true;
  for (int i = 1; i < argc; i++) {
    const std::vector<uint8_t> pkt = read_file(argv[i]);
    if (pkt.empty()) {
      std::fprintf(stderr, "zhao-desktop: cannot read %s\n", argv[i]);
      all_green = false;
      continue;
    }
    const zhao::ZhaoExecutionResult r = shell.submit(pkt);
    std::printf("  %s: status=%u completion=%s commands=%u\n", argv[i], r.status,
                (r.completion_flags & zhao_abi::ZHAO_COMPL_ERR) != 0 ? "ERR" : "DONE",
                r.counters.commands_total);
    if (r.status != zhao_abi::ZH_ABI_OK ||
        r.completion_flags != zhao_abi::ZHAO_COMPL_DONE) {
      all_green = false;
    }
  }
  std::printf(all_green ? "zhao-desktop stub: all frames green\n"
                        : "zhao-desktop stub: frame errors present\n");
  return all_green ? 0 : 1;
}
