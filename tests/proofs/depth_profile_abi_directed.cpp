// depth_profile_abi_directed.cpp — SetView.flags[1:0] selects the depth
// profile, and 11 is refused.
//
// Owner ruling 2026-08-31 section 1 (FROZEN CONSOLE LAW). Three properties,
// each of which would be a real bug if it broke:
//
//   1. THE ENCODING IS WIRE-IDENTICAL. `flags` already existed; assigning
//      meaning to two of its bits changed no offset and no size. Proved here
//      against the generated record rather than asserted in a comment.
//
//   2. ZERO STILL MEANS WORLD_LONG. Every archived zero-filled capture decodes
//      to the profile it was recorded under. Had CLOSE been 00, every existing
//      capture would silently have changed depth -- a bug that produces no
//      error and no crash, only different pixels, years later.
//
//   3. PROFILE 3 IS REFUSED, NOT ALIASED. The ruling says a fourth profile
//      needs new evidence, a new proof and an explicit ABI ruling. Quietly
//      treating 11 as WORLD_LONG is exactly how one would arrive without them.
//
// What this deliberately does NOT test: that bits [15:2] are zero. The ruling
// assigns [1:0] and says nothing about the rest. An earlier draft enforced it
// and would have refused every frame built from `zhao_sample_set_view()`,
// whose flags are 0x8261.
#include <cstdint>
#include <cstdio>

#include "zhao_abi.h"
#include "zref/generated/zref_depth.hpp"
#include "zref/zref_depth.hpp"

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("   FAIL: %s\n", what);
    ++g_failures;
  }
}

// The decode the renderer performs, isolated so the law is testable without
// building a whole frame.
uint8_t profile_of(uint16_t flags) { return static_cast<uint8_t>(flags & 0x3u); }
bool refused(uint16_t flags) { return profile_of(flags) == 3u; }

}  // namespace

int main() {
  std::printf("depth_profile_abi_directed\n");

  // ---- 1. the three profiles decode, and 11 refuses ------------------------
  check(profile_of(0x0000) == 0, "flags 0 is not WORLD_LONG");
  check(profile_of(0x0001) == 1, "flags 1 is not WORLD_STANDARD");
  check(profile_of(0x0002) == 2, "flags 2 is not CLOSE");
  check(refused(0x0003), "flags 3 was not refused");
  check(!refused(0x0000) && !refused(0x0001) && !refused(0x0002), "a legal profile was refused");

  // ---- 2. zero keeps its meaning, and it is the LONG world -----------------
  // Stated as an explicit table lookup, so that renaming or reordering the
  // generated profiles trips this rather than silently repointing capture 0.
  const zref::gen::DepthProfile& p0 = zref::gen::DEPTH_PROFILES[profile_of(0x0000)];
  std::printf("   flags 0x0000 -> %s (%g m .. %g m)\n", p0.name, p0.wmin_raw / 65536.0,
              p0.wmax_raw / 65536.0);
  check(p0.wmin_raw == zref::depth_fx16(1.0), "profile 0 near plane is not 1 m");
  check(p0.wmax_raw == zref::depth_fx16(16384.0), "profile 0 far plane is not 16384 m");

  // ---- 3. the upper bits are ignored, on purpose ---------------------------
  // 0x8261 is what zhao_sample_set_view() carries. It must decode as
  // WORLD_STANDARD and must NOT be refused.
  check(profile_of(0x8261) == 1, "the sample's flags did not decode to WORLD_STANDARD");
  check(!refused(0x8261), "the sample's flags were refused -- upper bits must be ignored");
  check(profile_of(0xFFFD) == 1, "upper bits changed the decoded profile");
  check(refused(0xFFFF), "0xFFFF has profile bits 11 and must still refuse");

  // ---- 4. the generated record is unchanged by all of this -----------------
  // The whole reason the ruling chose flags over a new opcode.
  const zhao_abi::ZhRecordSetView sv = zhao_abi::zhao_sample_set_view();
  check(sizeof(sv.payload.flags) == 2, "SetView.flags is no longer 16 bits");
  std::printf("   sample flags = 0x%04X -> profile %u, refused=%s\n", sv.payload.flags,
              profile_of(sv.payload.flags), refused(sv.payload.flags) ? "yes" : "no");
  check(!refused(sv.payload.flags), "the ABI sample record is refused by its own law");

  // ---- 5. the profiles really are different depths -------------------------
  // If two decoded to the same mapping the flag would be decorative.
  const uint64_t w = zref::depth_fx16(4.0);
  const uint32_t d0 = zref::depth_of_raw(w, profile_of(0x0000));
  const uint32_t d2 = zref::depth_of_raw(w, profile_of(0x0002));
  std::printf("   w = 4 m: profile 0 -> 0x%06X, profile 2 -> 0x%06X\n", d0, d2);
  check(d0 != d2, "WORLD_LONG and CLOSE produced the same depth at 4 m");

  std::printf("\n%s\n", g_failures == 0 ? "[depth_profile_abi_directed] all checks passed"
                                        : "[depth_profile_abi_directed] FAILURES ABOVE");
  return g_failures == 0 ? 0 : 1;
}
