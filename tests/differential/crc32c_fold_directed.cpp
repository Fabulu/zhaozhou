// crc32c_fold_directed.cpp — zhao_crc32c_fold against the SHIPPED CRC-32C.
//
// The module exists to make a timing problem go away, and a faster CRC that
// computes a different number than the rest of the machine is worse than the
// slow one. So the only thing under test here is EQUIVALENCE: for every state,
// every data word and every byte count, the parallel fold must equal the
// bit-serial `zhao_crc32c_step` chained n times.
//
// The oracle is `zhao_abi::zhao_crc32c_step` from the generated ABI header —
// the same function the reference renderer, the capture format and both
// existing hardware CRC users call. Not a restatement of it: the ABS defect in
// zhao_field_alu is the standing reminder that a test which restates a law can
// agree with a wrong implementation forever.
//
// The property is purely combinational, so it can be driven far harder than a
// sequential block: this lane runs an exhaustive sweep over every byte count
// crossed with structured corner states, plus a large random draw.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_crc32c_fold.h"

#include "zhao_sim.hpp"
#include "zhao_abi.h"  // generated: the shipped CRC-32C

namespace {

using zhao::check;

/**
 * The shipped law, chained n times over the low n bytes of d.
 *
 * `zhao_abi::zhao_crc32c` is the generated, shipped CRC and it wraps the raw
 * running state in an inversion at each end:
 *
 *   zhao_crc32c(c, buf, n) == ~raw_chain(~c, buf, n)
 *
 * so the RAW transform this module implements is recovered exactly by undoing
 * both inversions. That keeps the oracle the shipped function rather than a
 * second copy of the algorithm written beside the test -- the failure mode the
 * ABS defect in zhao_field_alu is the standing example of.
 */
uint32_t oracle(uint32_t c, uint64_t d, unsigned n) {
  uint8_t b[8];
  for (unsigned k = 0; k < n; ++k) b[k] = static_cast<uint8_t>((d >> (8 * k)) & 0xFFu);
  return ~zhao_abi::zhao_crc32c(~c, b, n);
}

uint32_t dut_fold(Vzhao_crc32c_fold& dut, uint32_t c, uint64_t d, unsigned n) {
  dut.c_i = c;
  dut.d_i = d;
  dut.n_i = static_cast<uint8_t>(n);
  dut.eval();
  return static_cast<uint32_t>(dut.c_o);
}

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint64_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int random_iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_iters = std::atoi(argv[++i]);
    }
  }

  Vzhao_crc32c_fold dut;

  // ---- 1. the identity: folding nothing changes nothing -------------------
  {
    const uint32_t states[] = {0u, 0xFFFFFFFFu, 0x12345678u, 1u, 0x80000000u};
    for (uint32_t c : states) {
      char nm[64];
      std::snprintf(nm, sizeof nm, "1.n=0 is the identity for c=0x%08X", c);
      check(dut_fold(dut, c, 0xDEADBEEFCAFEBABEull, 0) == c, nm, c,
            dut_fold(dut, c, 0xDEADBEEFCAFEBABEull, 0));
    }
  }

  // ---- 2. every byte count against the shipped chain ----------------------
  // The corner states are the ones a CRC actually starts from: the seed, zero,
  // a single set bit at each end, and the finalised form.
  {
    const uint32_t states[] = {0xFFFFFFFFu, 0x00000000u, 0x00000001u, 0x80000000u,
                               0x82F63B78u, 0x7D09C487u, 0xAAAAAAAAu, 0x55555555u};
    const uint64_t datas[] = {0x0000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0x0123456789ABCDEFull,
                              0x0000000000000001ull, 0x8000000000000000ull, 0x00FF00FF00FF00FFull,
                              0xDEADBEEFCAFEBABEull, 0x0101010101010101ull};
    for (uint32_t c : states) {
      for (uint64_t d : datas) {
        for (unsigned n = 0; n <= 8; ++n) {
          const uint32_t want = oracle(c, d, n);
          const uint32_t got = dut_fold(dut, c, d, n);
          char nm[96];
          std::snprintf(nm, sizeof nm, "2.fold c=0x%08X d=0x%016llX n=%u", c,
                        static_cast<unsigned long long>(d), n);
          check(got == want, nm, want, got);
        }
      }
    }
  }

  // ---- 3. ONE SET BIT AT A TIME, which is what the columns ARE -------------
  // The module derives its matrices from basis vectors, so a wrong column is
  // exactly a wrong answer for one basis input and can hide behind any amount
  // of random traffic that never isolates it.
  {
    for (unsigned n = 1; n <= 8; ++n) {
      for (int i = 0; i < 32; ++i) {
        const uint32_t c = 1u << i;
        const uint32_t want = oracle(c, 0, n);
        const uint32_t got = dut_fold(dut, c, 0, n);
        char nm[96];
        std::snprintf(nm, sizeof nm, "3.state basis bit %d, n=%u", i, n);
        check(got == want, nm, want, got);
      }
      for (unsigned j = 0; j < 8 * n; ++j) {
        const uint64_t d = 1ull << j;
        const uint32_t want = oracle(0, d, n);
        const uint32_t got = dut_fold(dut, 0, d, n);
        char nm[96];
        std::snprintf(nm, sizeof nm, "3.data basis bit %u, n=%u", j, n);
        check(got == want, nm, want, got);
      }
    }
  }

  // ---- 4. bytes ABOVE the count must not participate -----------------------
  // The cheapest way to be wrong here is to fold all eight bytes regardless.
  // A caller folding a partial final beat would then mix in whatever the bridge
  // left on the wire.
  {
    for (unsigned n = 0; n < 8; ++n) {
      const uint64_t low = 0x0123456789ABCDEFull & ((n == 0) ? 0ull : (~0ull >> (64 - 8 * n)));
      const uint64_t noise = 0xFFFFFFFFFFFFFFFFull << (8 * n);
      const uint32_t a = dut_fold(dut, 0xFFFFFFFFu, low, n);
      const uint32_t b = dut_fold(dut, 0xFFFFFFFFu, low | noise, n);
      char nm[96];
      std::snprintf(nm, sizeof nm, "4.bytes above n=%u are ignored", n);
      check(a == b, nm, a, b);
      check(a == oracle(0xFFFFFFFFu, low, n), "4.and the answer is still the shipped one",
            oracle(0xFFFFFFFFu, low, n), a);
    }
  }

  // ---- 5. a real message, folded eight bytes at a time ---------------------
  // The way the hardware will actually use it: a running state across beats,
  // compared against the byte-at-a-time chain over the same bytes.
  {
    std::vector<uint8_t> msg(256);
    Prng rng(0xC0FFEEu);
    for (size_t i = 0; i < msg.size(); ++i) msg[i] = static_cast<uint8_t>(rng.next());
    for (size_t total = 0; total <= msg.size(); total += 7) {
      uint32_t c = 0xFFFFFFFFu;
      size_t off = 0;
      while (off < total) {
        const unsigned n = static_cast<unsigned>(std::min<size_t>(8, total - off));
        uint64_t d = 0;
        for (unsigned k = 0; k < n; ++k) d |= static_cast<uint64_t>(msg[off + k]) << (8 * k);
        c = dut_fold(dut, c, d, n);
        off += n;
      }
      const uint32_t want = ~zhao_abi::zhao_crc32c(~0xFFFFFFFFu, msg.data(), total);
      char nm[80];
      std::snprintf(nm, sizeof nm, "5.running fold over %zu bytes", total);
      check(c == want, nm, want, c);
    }
  }

  // ---- 6. random -----------------------------------------------------------
  if (random_iters > 0) {
    Prng rng(0x5EED1234u);
    for (int it = 0; it < random_iters; ++it) {
      const uint32_t c = static_cast<uint32_t>(rng.next());
      const uint64_t d = rng.next();
      const unsigned n = static_cast<unsigned>(rng.next() % 9);
      const uint32_t want = oracle(c, d, n);
      const uint32_t got = dut_fold(dut, c, d, n);
      if (got != want) {
        char nm[128];
        std::snprintf(nm, sizeof nm, "6.random[%d] c=0x%08X d=0x%016llX n=%u", it, c,
                      static_cast<unsigned long long>(d), n);
        check(false, nm, want, got);
      }
    }
    std::printf("random: %d folds\n", random_iters);
    check(true, "6.random folds all match the shipped chain", 1, 1);
  }

  return zhao::report_and_exit("crc32c_fold_directed");
}
