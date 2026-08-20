// field_stamp_modes.cpp — the five FIELD.STAMP.* ops, RTL against the oracle.
//
// WHY THIS FILE EXISTS. `design/ops.yml` declares FIELD.STAMP.{MAX, ADD, SUB,
// REPLACE, AGE} and names `tests/differential/field_stamp_modes.cpp` as their
// differential coverage. That file did not exist. Ledger rule V10 only said so
// once SURFACE.STAMP was advanced past SPECIFIED — the op layer had NO
// differential coverage at all, and the block layer above it was hiding that.
//
// The ops also cite `zref::fieldir::stamp_max` and friends, which do not exist:
// part of the forty phantom `zref::fieldir::*` references in ops.yml. But the
// LAW does exist and always did — `zref::surface::blend_apply` implements all
// five, with each ops.yml semantic quoted beside its enumerator. So this is a
// naming failure, not a missing implementation, and the fix is to test the real
// law rather than to write a second one.
//
// EXHAUSTIVE, NOT SAMPLED. The blend is a pure function of (blend, dst, src,
// age_shift) = 7 x 256 x 256 x 8. The RTL's own header calls its formal proof
// TOTAL for the same reason. A random lane here would be strictly weaker than
// simply trying every input, so every input is tried.

#include "Vzhao_surface_blend.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_surface.hpp"

#include <cstdint>
#include <cstdio>

namespace {

using zhao::check;
namespace zs = zref::surface;

struct Case {
  zs::Blend blend;
  const char* op;      // the ops.yml id this enumerator implements
  const char* law;     // its stated semantics, verbatim from ops.yml
};

const Case kCases[] = {
    {zs::kBlendMax, "FIELD.STAMP.MAX", "dst = max(dst, src)"},
    {zs::kBlendAdd, "FIELD.STAMP.ADD", "dst = sat(dst + src)"},
    {zs::kBlendSub, "FIELD.STAMP.SUB", "dst = sat(dst - src)"},
    {zs::kBlendReplace, "FIELD.STAMP.REPLACE", "dst = src"},
    {zs::kBlendAge, "FIELD.STAMP.AGE", "dst >>= age_shift"},
};

}  // namespace

int main() {
  Vzhao_surface_blend dut;

  // ---- 1. the five ops, exhaustively against the oracle -------------------
  for (const Case& c : kCases) {
    uint64_t mismatches = 0;
    uint32_t first_dst = 0, first_src = 0, first_shift = 0, first_want = 0, first_got = 0;

    for (uint32_t dst = 0; dst < 256; ++dst) {
      for (uint32_t src = 0; src < 256; ++src) {
        // age_shift only reaches kBlendAge; sweeping it for the others costs
        // little and proves they IGNORE it, which is itself a law worth pinning.
        for (uint32_t sh = 0; sh < 8; ++sh) {
          dut.mode_i = static_cast<uint8_t>(c.blend);
          dut.dst_i = static_cast<uint8_t>(dst);
          dut.src_i = static_cast<uint8_t>(src);
          dut.age_shift_i = static_cast<uint8_t>(sh);
          dut.eval();

          const uint8_t want = zs::blend_apply(c.blend, static_cast<uint8_t>(dst),
                                               static_cast<uint8_t>(src),
                                               static_cast<uint8_t>(sh));
          const uint8_t got = static_cast<uint8_t>(dut.out_o);
          if (want != got && mismatches++ == 0) {
            first_dst = dst; first_src = src; first_shift = sh;
            first_want = want; first_got = got;
          }
        }
      }
    }

    char what[160];
    std::snprintf(what, sizeof what, "%s (%s): exhaustive over dst x src x age_shift", c.op,
                  c.law);
    check(mismatches == 0, what, 0, mismatches);
    if (mismatches != 0) {
      std::printf("  first divergence: dst=%u src=%u age_shift=%u want=%u got=%u\n", first_dst,
                  first_src, first_shift, first_want, first_got);
    }
  }

  // ---- 2. the three laws the exhaustive sweep cannot state for itself ------
  // A sweep proves the RTL matches the oracle. It does not prove the ORACLE
  // matches ops.yml, so the semantics are asserted directly at their edges.
  {
    check(zs::blend_apply(zs::kBlendAdd, 200, 100, 0) == 255,
          "FIELD.STAMP.ADD saturates rather than wrapping", 255,
          zs::blend_apply(zs::kBlendAdd, 200, 100, 0));
    check(zs::blend_apply(zs::kBlendSub, 40, 100, 0) == 0,
          "FIELD.STAMP.SUB saturates at zero rather than wrapping", 0,
          zs::blend_apply(zs::kBlendSub, 40, 100, 0));
    check(zs::blend_apply(zs::kBlendReplace, 255, 7, 0) == 7,
          "FIELD.STAMP.REPLACE ignores dst entirely", 7,
          zs::blend_apply(zs::kBlendReplace, 255, 7, 0));
    check(zs::blend_apply(zs::kBlendMax, 9, 200, 0) == 200,
          "FIELD.STAMP.MAX keeps the peak", 200, zs::blend_apply(zs::kBlendMax, 9, 200, 0));
    // AGE terminates. This is the whole argument for choosing a shift over a
    // unit8 multiply: a rate of 255/256 never reaches zero, so an aged scar
    // would sit at strength 1 for hundreds of frames. A shift always gets there.
    uint8_t v = 255;
    int steps = 0;
    while (v != 0 && steps < 64) { v = zs::blend_apply(zs::kBlendAge, v, 0, 1); ++steps; }
    check(v == 0, "FIELD.STAMP.AGE reaches zero, and does so in bounded steps", 0, v);
    check(steps <= 8, "FIELD.STAMP.AGE: 255 decays to 0 within 8 shifts of 1", 8,
          static_cast<uint64_t>(steps));
  }

  // ---- 3. the ABI mapping, which is where a capture gets replayed ---------
  // stamp_surface's branch is `operation == 1 ? decay-accumulate : max`, with
  // an `else` rather than a table. So operation 7 is a MAX stamp, not an error,
  // and a capture carrying it must replay that way forever.
  check(zs::blend_of_abi_operation(1) == zs::kBlendDecayAcc,
        "ABI operation 1 is decay-accumulate", zs::kBlendDecayAcc,
        zs::blend_of_abi_operation(1));
  check(zs::blend_of_abi_operation(0) == zs::kBlendStamp, "ABI operation 0 is the max stamp",
        zs::kBlendStamp, zs::blend_of_abi_operation(0));
  check(zs::blend_of_abi_operation(7) == zs::kBlendStamp,
        "ABI operation 7 replays as a max stamp (the else branch, not an error)",
        zs::kBlendStamp, zs::blend_of_abi_operation(7));

  dut.final();
  return zhao::report_and_exit("field_stamp_modes");
}
