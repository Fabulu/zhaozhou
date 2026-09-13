#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "Vzhao_shell_fit_smoke_tb.h"
#include "verilated.h"

double sc_time_stamp() { return 0.0; }

namespace {

constexpr unsigned kDeadReasonsPerDomain = 8;

struct RunMode {
  int control_domain = -1;
  unsigned control_arm = 0;
  unsigned protocol_fault = 0;
  // Covers the first counter window and a later FRAME_RING DONE -> FREE edge.
  std::uint64_t half_steps = 1050000;
};

int domain_index(const char* value) {
  if (std::strcmp(value, "gpu") == 0) return 0;
  if (std::strcmp(value, "video") == 0) return 1;
  if (std::strcmp(value, "audio") == 0) return 2;
  return -1;
}

unsigned detector_arm(const char* value) {
  if (std::strcmp(value, "edges") == 0) return 1;
  if (std::strcmp(value, "activity") == 0) return 2;
  if (std::strcmp(value, "wraps") == 0) return 3;
  if (std::strcmp(value, "misr") == 0) return 4;
  if (std::strcmp(value, "snapshot") == 0) return 5;
  if (std::strcmp(value, "epoch") == 0) return 6;
  if (std::strcmp(value, "capture") == 0) return 7;
  if (std::strcmp(value, "serializer") == 0) return 8;
  if (std::strcmp(value, "payload") == 0) return 9;
  return 0;
}

unsigned protocol_fault(const char* value) {
  if (std::strcmp(value, "ring") == 0) return 1;
  if (std::strcmp(value, "hps") == 0) return 2;
  if (std::strcmp(value, "counter") == 0) return 3;
  if (std::strcmp(value, "render") == 0) return 4;
  if (std::strcmp(value, "guard") == 0) return 5;
  if (std::strcmp(value, "sdr") == 0) return 6;
  if (std::strcmp(value, "guard-verdict") == 0) return 7;
  if (std::strcmp(value, "guard-early") == 0) return 8;
  if (std::strcmp(value, "guard-late") == 0) return 9;
  if (std::strcmp(value, "guard-extra") == 0) return 10;
  if (std::strcmp(value, "hps-timing") == 0) return 11;
  if (std::strcmp(value, "render-stability") == 0) return 12;
  if (std::strcmp(value, "guard-verdict-extra") == 0) return 13;
  if (std::strcmp(value, "guard-post-denial") == 0) return 14;
  if (std::strcmp(value, "guard-preownership") == 0) return 15;
  return 0;
}

bool parse_mode(int argc, char** argv, RunMode& mode) {
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--control") == 0 && index + 2 < argc) {
      mode.control_domain = domain_index(argv[++index]);
      mode.control_arm = detector_arm(argv[++index]);
      if (mode.control_domain < 0 || mode.control_arm == 0 || mode.protocol_fault != 0) {
        std::fprintf(stderr, "invalid or conflicting detector control\n");
        return false;
      }
      mode.half_steps = 4000;
    } else if (std::strcmp(argv[index], "--protocol-control") == 0 && index + 1 < argc) {
      mode.protocol_fault = protocol_fault(argv[++index]);
      if (mode.protocol_fault == 0 || mode.control_domain >= 0) {
        std::fprintf(stderr, "invalid or conflicting protocol control\n");
        return false;
      }
      // Protocol arms observe completed transactions; FRAME_RING DONE -> FREE is latest.
      mode.half_steps = 1050000;
    } else if (std::strcmp(argv[index], "--half-steps") == 0 && index + 1 < argc) {
      char* end = nullptr;
      const auto parsed = std::strtoull(argv[++index], &end, 10);
      if (end == argv[index] || *end != '\0' || parsed == 0) {
        std::fprintf(stderr, "invalid --half-steps value: %s\n", argv[index]);
        return false;
      }
      mode.half_steps = parsed;
    } else {
      std::fprintf(stderr,
                   "usage: %s [--control gpu|video|audio "
                   "edges|activity|wraps|misr|snapshot|epoch|capture|serializer|payload] "
                   "[--protocol-control ring|hps|counter|render|guard|sdr|"
                   "guard-verdict|guard-early|guard-late|guard-extra|hps-timing|"
                   "render-stability|guard-verdict-extra|guard-post-denial|"
                   "guard-preownership] "
                   "[--half-steps N]\n",
                   argv[0]);
      return false;
    }
  }
  return true;
}

void step(Vzhao_shell_fit_smoke_tb& top,
          VerilatedContext& context,
          std::uint64_t phase) {
  top.gpu_clk = !top.gpu_clk;
  if ((phase & 1u) == 0) top.vid_clk = !top.vid_clk;
  if ((phase & 3u) == 0) top.audio_clk = !top.audio_clk;
  top.eval();
  context.timeInc(5);
}

}  // namespace

int main(int argc, char** argv) {
  RunMode mode;
  if (!parse_mode(argc, argv, mode)) return 2;

  auto context = std::make_unique<VerilatedContext>();
  context->commandArgs(argc, argv);
  auto top = std::make_unique<Vzhao_shell_fit_smoke_tb>(context.get());
  top->gpu_clk = 0;
  top->vid_clk = 0;
  top->audio_clk = 0;
  top->rst_n = 0;
  top->check_i = 0;
  top->control_domain_i = mode.control_domain < 0
                              ? 3
                              : static_cast<std::uint8_t>(mode.control_domain);
  top->control_arm_i = mode.control_arm;
  top->protocol_fault_i = mode.protocol_fault;
  top->eval();

  for (std::uint64_t phase = 0; phase < 32; ++phase) step(*top, *context, phase);
  top->gpu_clk = 0;
  top->vid_clk = 0;
  top->audio_clk = 0;
  top->eval();
  top->rst_n = 1;
  top->eval();

  for (std::uint64_t phase = 0; phase < mode.half_steps; ++phase)
    step(*top, *context, phase);

  top->check_i = 1;
  top->eval();
  top->check_i = 0;
  top->eval();

  const auto dead = static_cast<unsigned>(top->dead_domains_o);
  const auto reasons = static_cast<unsigned>(top->dead_reasons_o);
  if (mode.control_domain >= 0) {
    const unsigned domain_bit = 1u << static_cast<unsigned>(mode.control_domain);
    const unsigned reason_base =
        static_cast<unsigned>(mode.control_domain) * kDeadReasonsPerDomain;
    const unsigned expected_reasons = mode.control_arm == 9
                                          ? (0xd8u << reason_base)
                                          : (1u << (reason_base + mode.control_arm - 1u));
    if (dead != domain_bit || reasons != expected_reasons ||
        (mode.control_arm == 9 && !top->control_group_failure_o)) {
      std::fprintf(stderr,
                   "SHELL_FIT_SMOKE_CONTROL_FAIL domain=%d arm=%u dead=%u "
                   "reasons=0x%x expected=0x%x group=%u\n",
                   mode.control_domain,
                   mode.control_arm,
                   dead,
                   reasons,
                   expected_reasons,
                   static_cast<unsigned>(top->control_group_failure_o));
      top->final();
      return 1;
    }
    std::printf("SHELL_FIT_SMOKE_CONTROL_PASS domain=%d arm=%u dead=%u reasons=0x%x\n",
                mode.control_domain,
                mode.control_arm,
                dead,
                reasons);
    top->final();
    return 0;
  }

  if (mode.protocol_fault != 0) {
    const unsigned expected_arm =
        (mode.protocol_fault == 13 || mode.protocol_fault == 14 ||
         mode.protocol_fault == 15)
            ? (1u << 9u)
            : (1u << (mode.protocol_fault - 1u));
    const unsigned observed_arms = static_cast<unsigned>(top->protocol_failures_o);
    if (!top->protocol_failure_o || observed_arms != expected_arm) {
      std::fprintf(stderr,
                   "SHELL_FIT_SMOKE_PROTOCOL_CONTROL_FAIL fault=%u observed=0x%x expected=0x%x\n",
                   mode.protocol_fault,
                   observed_arms,
                   expected_arm);
      top->final();
      return 1;
    }
    std::printf("SHELL_FIT_SMOKE_PROTOCOL_CONTROL_PASS fault=%u arm=0x%x\n",
                mode.protocol_fault,
                observed_arms);
    top->final();
    return 0;
  }

  if (top->baseline_failed_o) {
    std::fprintf(stderr,
                 "SHELL_FIT_SMOKE_FAIL baseline dead=%u reasons=0x%x\n",
                 dead,
                 reasons);
    top->final();
    return 1;
  }
  std::printf("SHELL_FIT_SMOKE_PASS half_steps=%llu domains=3\n",
              static_cast<unsigned long long>(mode.half_steps));
  top->final();
  return 0;
}
