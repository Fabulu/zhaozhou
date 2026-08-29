// measure_sreg_hwm.cpp -- HOW DEEP THE SCALAR (UNIFORM) BANK HAS TO BE.
//
// COMMITTED BECAUSE A THROWN-AWAY PROBE LEAVES UNREPRODUCIBLE NUMBERS. That is
// a standing rule in CLAUDE.md, learned when a ground-penetration probe was
// written once, quoted, and deleted -- so its figure could never be rechecked.
// The bank geometry below is an architectural commitment; the thing that
// justifies it must outlive the session that ran it.
//
// Build (no build tree, no RTL -- it cannot collide with a running sweep):
//
//   g++ -std=c++20 -O1 -I reference/include -I runtime/include //       -o measure_sreg_hwm.exe tools/field/measure_sreg_hwm.cpp //       reference/src/zfield/zfield_plan.cpp reference/src/zfield/zfield_decode.cpp
//
//   ./measure_sreg_hwm.exe tests/fuzz/corpus/field/*.zprog compiler/tests/generated/*.zprog
//
// Result on 2026-08-29, 11 programs, 0 failures:
//
//   WORST sreg_hwm = 41   impact_wave, mask 0 (everything uniform)
//   Earth mask            crater_ring 29, impact_wave 23, wave_pool 19
//   all inputs varying    6 - 8
//   vreg_hwm never above 26, against a declared cap of 32
//
// THE MASK SWEEP IS THE POINT. The bank is worst when EVERYTHING is uniform,
// which is the opposite of the intuition that a busier program needs more
// scalars: a value that varies lives in a vector register, and a value that
// does not becomes a scalar slot. Planning only the Earth mask reports 29 and
// undersizes the bank by 40%.
//
// Chosen: 64 slots, 6-bit index. Four ring indices x 6 bits = 24 bits, which
// fits the existing 32-bit immediate with 8 to spare, so the instruction word
// does not grow. Above 64 the hardware refuses rather than wraps.
//
// Measure the scalar (uniform) bank depth a legal program actually needs.
//
// The cost model caps vreg_hwm at 32 and says nothing about sreg_hwm, so the
// bank geometry has to be DERIVED. This walks the real corpus rather than
// picking a round number, and prints the distribution so the choice can be
// argued from data.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "zfield/zfield.hpp"
#include "zfield/zfield_plan.hpp"

static std::vector<uint8_t> slurp(const char* path) {
  std::vector<uint8_t> v;
  FILE* f = fopen(path, "rb");
  if (!f) return v;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  v.resize((size_t)(n > 0 ? n : 0));
  if (!v.empty() && fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
  fclose(f);
  return v;
}

int main(int argc, char** argv) {
  int worst = 0;
  const char* worst_name = "(none)";
  int decoded = 0, failed = 0;

  // Every varying mask that matters: Earth varies x,z (0b11 in the diff), but
  // a mask that makes MORE inputs uniform allocates MORE scalar slots, so the
  // worst case is swept rather than assumed.
  const uint32_t masks[] = {0u, 0b01u, 0b10u, 0b11u, 0b111u, 0xFFFFFFFFu};

  for (int i = 1; i < argc; ++i) {
    const std::vector<uint8_t> bytes = slurp(argv[i]);
    if (bytes.empty()) { ++failed; continue; }
    const zfield::DecodeResult dec = zfield::decode(bytes.data(), bytes.size());
    if (dec.error != zfield::DecodeError::kOk) { ++failed; printf("  UNDECODABLE %s\n", argv[i]); continue; }
    ++decoded;
    for (uint32_t m : masks) {
      const zfield::Fplan fp = zfield::plan(dec.prog, m);
      const int s = (int)fp.demand.sreg_hwm;
      if (s > worst) { worst = s; worst_name = argv[i]; }
      printf("  %-58s mask %08X  sreg_hwm %3d  vreg_hwm %3d  prep %zu\n", argv[i], m, s,
             (int)fp.demand.vreg_hwm, fp.prep.size());
    }
  }
  printf("\nDECODED %d, FAILED %d\n", decoded, failed);
  printf("WORST sreg_hwm = %d  (%s)\n", worst, worst_name);
  return 0;
}
