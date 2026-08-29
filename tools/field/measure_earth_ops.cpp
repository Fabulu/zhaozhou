// measure_earth_ops.cpp — WHAT DOES A REAL EARTH PROGRAM ACTUALLY ASK FOR?
//
// COMMITTED, like measure_sreg_hwm.cpp beside it, because the answer decides
// what still has to be built and a number nobody can recheck is not evidence.
//
// The Field engine implements 25 of 31 canonical opcodes. The six that are
// missing are LEN2, LEN3, DIST2, RCP, SIN, COS and the deliberately-cold
// OP_RING. Whether that matters for Earth is not a question of opinion: it is
// whichever opcodes the shipped Earth programs contain, and how many vector
// uops and multiplier slots they cost per association.
//
// Build (no build tree, no RTL):
//
//   g++ -std=c++20 -O1 -I reference/include -I runtime/include \
//       -o measure_earth_ops.exe tools/field/measure_earth_ops.cpp \
//       reference/src/zfield/zfield_plan.cpp reference/src/zfield/zfield_decode.cpp
//
//   ./measure_earth_ops.exe compiler/tests/generated/*.zprog
//
// The Earth mask is 0b11 -- x and z vary per point, y does not -- and that is
// what zfield_plan's own differential uses, so this measures the same shape
// the reference does rather than a mask chosen here.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "zfield/zfield.hpp"
#include "zfield/zfield_plan.hpp"

namespace {

std::vector<uint8_t> slurp(const char* path) {
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

const char* op_name(uint8_t op) {
  switch (op) {
    case zfield::OP_END: return "END";
    case zfield::OP_MOV: return "MOV";
    case zfield::OP_LDC: return "LDC";
    case zfield::OP_ADD: return "ADD";
    case zfield::OP_SUB: return "SUB";
    case zfield::OP_MUL: return "MUL";
    case zfield::OP_MAD: return "MAD";
    case zfield::OP_MIN: return "MIN";
    case zfield::OP_MAX: return "MAX";
    case zfield::OP_ABS: return "ABS";
    case zfield::OP_CLAMP: return "CLAMP";
    case zfield::OP_SELECT: return "SELECT";
    case zfield::OP_CMP: return "CMP";
    case zfield::OP_DOT2: return "DOT2";
    case zfield::OP_DOT3: return "DOT3";
    case zfield::OP_LEN2: return "LEN2";
    case zfield::OP_LEN3: return "LEN3";
    case zfield::OP_DIST2: return "DIST2";
    case zfield::OP_NORMALIZE2: return "NORMALIZE2";
    case zfield::OP_NORMALIZE3: return "NORMALIZE3";
    case zfield::OP_RCP: return "RCP";
    case zfield::OP_SIN: return "SIN";
    case zfield::OP_COS: return "COS";
    case zfield::OP_CURVE: return "CURVE";
    case zfield::OP_SPLINE: return "SPLINE";
    case zfield::OP_NOISE2: return "NOISE2";
    case zfield::OP_DCURVE: return "DCURVE";
    case zfield::OP_RING: return "RING";
    case zfield::OP_RIDGE: return "RIDGE";
    case zfield::OP_ROT2: return "ROT2";
    case zfield::OP_ROT3: return "ROT3";
    case zfield::PREP_RING_MID: return "PREP_RING_MID";
    case zfield::UOP_RING_PREP: return "UOP_RING_PREP";
    default: return "?";
  }
}

// What the hardware serves today, so the report says BUILT or MISSING rather
// than leaving the reader to cross-reference two files.
bool served(uint8_t op) {
  switch (op) {
    // the ALU
    case zfield::OP_END: case zfield::OP_MOV: case zfield::OP_LDC:
    case zfield::OP_ADD: case zfield::OP_SUB: case zfield::OP_MUL:
    case zfield::OP_MAD: case zfield::OP_MIN: case zfield::OP_MAX:
    case zfield::OP_ABS: case zfield::OP_CLAMP: case zfield::OP_SELECT:
    case zfield::OP_CMP: case zfield::OP_DOT2: case zfield::OP_DOT3:
    // the service path
    case zfield::OP_CURVE: case zfield::OP_DCURVE: case zfield::OP_SPLINE:
    case zfield::OP_NOISE2: case zfield::OP_RIDGE:
    case zfield::OP_NORMALIZE2: case zfield::OP_NORMALIZE3:
    case zfield::OP_ROT2: case zfield::OP_ROT3:
    case zfield::UOP_RING_PREP:
      return true;
    default:
      return false;
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::map<uint8_t, int> vec_hist, prep_hist;
  std::map<uint8_t, int> missing;

  for (int i = 1; i < argc; ++i) {
    const std::vector<uint8_t> bytes = slurp(argv[i]);
    if (bytes.empty()) { printf("  UNREADABLE %s\n", argv[i]); continue; }
    const zfield::DecodeResult dec = zfield::decode(bytes.data(), bytes.size());
    if (dec.error != zfield::DecodeError::kOk) {
      printf("  UNDECODABLE %s\n", argv[i]);
      continue;
    }
    // Earth: x and z vary per point.
    const zfield::Fplan fp = zfield::plan(dec.prog, 0b11);

    int miss_here = 0;
    for (const zfield::VecUop& v : fp.uops) {
      ++vec_hist[v.op];
      if (!served(v.op)) { ++missing[v.op]; ++miss_here; }
    }
    for (const zfield::PrepUop& p : fp.prep) ++prep_hist[p.op];

    printf("%-46s class=%s  vec_uops=%3zu prep=%2zu  vec_issue=%u vmul=%u "
           "curve_req=%u dist_req=%u cold=%u  MISSING=%d\n",
           argv[i], fp.perf_class == zfield::PlanClass::kHot ? "HOT " : "cold",
           fp.uops.size(), fp.prep.size(), fp.demand.vec_issue, fp.demand.vmul_slots,
           fp.demand.curve_req, fp.demand.dist_req, fp.demand.cold_ops, miss_here);
  }

  printf("\n== vector uops across all programs ==\n");
  for (const auto& kv : vec_hist)
    printf("   %-16s %4d   %s\n", op_name(kv.first), kv.second,
           served(kv.first) ? "served" : "*** NOT SERVED ***");

  printf("\n== uniform prep ops (ARM side) ==\n");
  for (const auto& kv : prep_hist)
    printf("   %-16s %4d\n", op_name(kv.first), kv.second);

  printf("\n== VERDICT ==\n");
  if (missing.empty()) {
    printf("   Every op these programs use is served by the hardware.\n");
  } else {
    printf("   %zu opcode(s) used and NOT served:\n", missing.size());
    for (const auto& kv : missing)
      printf("     %-16s used %d time(s)\n", op_name(kv.first), kv.second);
  }
  return 0;
}
