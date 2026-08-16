// shell_probe.cpp — W2.7 bring-up probe (not a ctest): measures the shell's
// steady-state scanout margin with and without a competing blit, and dumps
// per-tick starvation deltas. Diagnostic tool for the composition bring-up;
// kept for the next integrator.

#include <cstdio>
#include <cstring>

#include "shell_harness.hpp"

using namespace zhao_shell;

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  const bool with_blit = argc > 1 && !std::strcmp(argv[1], "--blit");

  ShellHarness h;
  h.audio_enable = false;
  h.reset();

  // P0: mode set to DUO
  PacketSpec s0;
  s0.mode = 2;
  h.publish(0, build_packet(s0));

  std::vector<uint8_t> canvas;
  MarkerState p1{60, 90}, p2{188, 94};
  uint64_t prev_starve = 0;
  uint32_t published = 0;

  // reference CRCs for forensics
  std::vector<uint8_t> zero_slot(zref::render::kSlotBytes, 0);
  std::printf("black_z60=%08X black_duo=%08X\n",
              zref::render::displayed_crc32c(zhao_abi::VIDEO_Z60, zero_slot.data()),
              zref::render::displayed_crc32c(zhao_abi::VIDEO_DUO, zero_slot.data()));
  std::vector<uint8_t> mirror(zref::render::kSlotBytes, 0);
  size_t crc_seen = 0;

  const bool linemap = argc > 2 && !std::strcmp(argv[2], "--linemap");
  size_t blits_seen = 0;
  uint64_t starve_prev_line = 0;
  int prev_y = -1;
  for (int t = 0; t < 8; ++t) {
    for (uint64_t i = 0; i < 3'000'000; ++i) {
      h.step();
      if (linemap && t == 3) {   // map one steady frame
        const int y = h.top.px_valid_o ? int(h.top.px_y_o) : prev_y;
        if (y != prev_y) {
          const uint64_t st = h.top.starvation_o;
          if (st != starve_prev_line && prev_y >= 0) {
            std::printf("    line %d: +%llu starved\n", prev_y,
                        (unsigned long long)(st - starve_prev_line));
          }
          starve_prev_line = st;
          prev_y = y;
        }
      }
      if (h.blit_log.size() > blits_seen) {
        std::printf("  blit %zu done at n=%llu (status %u)\n", blits_seen,
                    (unsigned long long)h.n, h.blit_log.back().status);
        ++blits_seen;
      }
      if (h.tick_seen_last_step) break;
    }
    std::printf("  [tick at n=%llu]\n", (unsigned long long)h.n);
    const TickEvent tk = h.ticks.back();
    const uint64_t st = h.top.starvation_o;
    std::printf("tick %u repeated=%d starve=%llu (+%llu) crcs=%zu blits=%zu "
                "faults=%llu err=%llx\n",
                tk.frame_id, tk.repeated ? 1 : 0, (unsigned long long)st,
                (unsigned long long)(st - prev_starve), h.crcs.size(),
                h.blit_log.size(), (unsigned long long)h.top.deadline_faults_o,
                (unsigned long long)h.sticky_errors());
    prev_starve = st;

    while (crc_seen < h.crcs.size()) {
      std::printf("  crc[%zu] = %08X\n", crc_seen, h.crcs[crc_seen]);
      ++crc_seen;
    }

    if (with_blit && tk.frame_id >= 1 && published < 6) {
      ++published;
      const uint32_t f = published;
      compose_duo_frame(canvas, f, p1, p2);
      const uint32_t crc =
          zhao_abi::zhao_crc32c(0, canvas.data(), canvas.size());
      std::memcpy(mirror.data(), canvas.data(), canvas.size());
      std::printf("  P%u: payload=%08X displayed=%08X\n", f, crc,
                  zref::render::displayed_crc32c(zhao_abi::VIDEO_DUO,
                                                 mirror.data()));
      const uint32_t arena = (f & 1u) ? kArena1 : kArena0;
      h.mem_write(arena, canvas);
      PacketSpec s;
      s.frame_id = f;
      s.sequence = f;
      s.mode = 2;
      s.has_blit = true;
      s.blit_dst = uint8_t(f & 1u);
      s.blit_src = arena;
      s.blit_len = uint32_t(canvas.size());
      s.blit_crc = crc;
      h.publish(int(f % 3), build_packet(s));
    }
  }
  zhao::exit_hard(0);
}
