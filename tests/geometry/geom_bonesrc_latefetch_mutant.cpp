// geom_bonesrc_latefetch_mutant.cpp — THE INVERTED-POLARITY DRIVER.
//
// It PASSES WHEN `bone_prefetch_late_o` FIRES. It is evidence about the
// instrument, not about the design, and a green run here means the mutation in
// `tests/mutants/zhao_geom_bonesrc_latefetch_mutant.sv` is still present.
//
// WHY IT EXISTS. `bone_prefetch_late_o` is unreachable by legal stimulus: the
// decoder spends ~115 cycles per bone and the refill takes seven, so no clip,
// bone count or backpressure can make the prefetch late while the guard is
// correct. CLAUDE.md names that exact case — "a guard you cannot reach with
// legal stimulus needs a COMMITTED MUTANT" — because otherwise "it can fire"
// stays an argument forever and the next person inherits the argument and no
// evidence.
//
// THE MUTANT'S FAULT IS A REAL RISK. It stalls the body fill to one word per
// 256 cycles, which is what a kind-8 page read through MEM.GUARD to SDRAM could
// plausibly cost. Under it the decoder advances onto a bone whose data has not
// landed and emits a WRONG PALETTE with every handshake intact — so this driver
// also asserts that the palette DISAGREES with the correct one, because a
// counter that fires while the output is still right would be measuring
// something other than the fault it names (owner ruling R95's lesson, and the
// terrain mutant whose header described the wrong fault entirely).
//
// THE NEGATIVE CONTROL IS SEPARATE, in `geom_bonesrc_directed` cases 3 and 4:
// the same counter must read ZERO across two correct back-to-back palettes.
// The pair is what proves the detector DISCRIMINATES rather than merely moves.
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vtb_geom_bonesrc_mutant.h"
#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_creature_page.hpp"

// The harness check takes (cond, what, expected, actual). Every assertion
// here is a boolean predicate, so this states the expectation once rather
// than writing `1, cond` at sixty call sites.
static void check(bool cond, const char* what) { zhao::check(cond, what, 1u, cond ? 1u : 0u); }
namespace cp = zref::creature_page;
namespace cb = zref::creature_page::body;

namespace {

// C stdio and a char buffer rather than std::string: CLAUDE.md's build note
// records std::ofstream faulting on this toolchain, and gcc 16's libstdc++ here
// leaves std::string's MOVE CONSTRUCTOR undefined at link time under -static.
// Neither is worth a dependency for opening one fixture.
std::vector<uint8_t> read_file(const char* path) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return {};
  std::vector<uint8_t> v;
  uint8_t buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) v.insert(v.end(), buf, buf + n);
  std::fclose(f);
  return v;
}


// Must match geom_bonesrc_directed.cpp's fixture exactly: the two runs differ
// by the mutation and by nothing else.
static zref::creature::quat16 bone_quat(size_t b) {
  const int16_t i = static_cast<int16_t>(b);
  return zref::creature::quat16{{static_cast<int16_t>(16384 - 300 * i),
                                 static_cast<int16_t>(1500 * (i + 1)),
                                 static_cast<int16_t>(700 * i),
                                 static_cast<int16_t>(-400 * i)}};
}

}  // namespace

int main() {
  Vtb_geom_bonesrc_mutant d;
  long cycles = 0;
  auto tick = [&]() { d.clk = 0; d.eval(); d.clk = 1; d.eval(); ++cycles; };

  d.rst_n = 0;
  d.fill_we_i = 0; d.fill_sel_i = 0; d.fill_bone_i = 0;
  d.fill_word_i = 0; d.fill_data_i = 0;
  d.req_i = 0; d.bone_count_i = 0;
  d.root_dx_i = 0; d.root_dy_i = 0; d.root_dz_i = 0;
  d.out_ready_i = 1;
  for (int i = 0; i < 4; ++i) tick();
  d.rst_n = 1;
  tick();

  char golden_path[512];
  std::snprintf(golden_path, sizeof golden_path,
                "%s/tests/golden/creature_ladder/ladder_page_body_v1.bin", ZHAO_SOURCE_DIR);
  const std::vector<uint8_t> golden = read_file(golden_path);
  check(!golden.empty(), "the body golden is present");
  const uint32_t body_off = cp::get_u32(golden.data() + 8);

  std::vector<cb::BoneRecord> bones;
  check(cb::decode_body(golden.data() + body_off, golden.size() - body_off, bones) ==
            cb::BodyVerdict::kOk,
        "the golden's body decodes");
  const int nb = static_cast<int>(bones.size());

  auto fill = [&](bool sel, int bone, int word, uint64_t data) {
    d.fill_we_i = 1; d.fill_sel_i = sel ? 1 : 0;
    d.fill_bone_i = static_cast<uint8_t>(bone);
    d.fill_word_i = static_cast<uint8_t>(word);
    d.fill_data_i = data;
    tick();
    d.fill_we_i = 0; d.fill_data_i = 0;
  };

  for (int b = 0; b < nb; ++b) {
    const uint8_t* rec = golden.data() + body_off + cb::kHeaderBytes + cb::kBoneBytes * b;
    for (int w = 0; w < 4; ++w) {
      uint64_t v = 0;
      for (int k = 7; k >= 0; --k) v = (v << 8) | static_cast<uint64_t>(rec[w * 8 + k]);
      fill(false, b, w, v);
    }
    const zref::creature::quat16 q = bone_quat(static_cast<size_t>(b));
    uint64_t qv = (static_cast<uint64_t>(static_cast<uint16_t>(q.q[3])) << 48) |
                  (static_cast<uint64_t>(static_cast<uint16_t>(q.q[2])) << 32) |
                  (static_cast<uint64_t>(static_cast<uint16_t>(q.q[1])) << 16) |
                  static_cast<uint64_t>(static_cast<uint16_t>(q.q[0]));
    fill(true, b, 0, qv);
  }

  // Run one palette under the stalled fill.
  std::vector<std::array<int32_t, 12>> pal(static_cast<size_t>(nb));
  std::vector<bool> seen(static_cast<size_t>(nb), false);
  d.bone_count_i = static_cast<uint8_t>(nb);
  d.req_i = 1; tick(); d.req_i = 0;

  int got = 0;
  const long budget = 4000000;
  const long start = cycles;
  while (cycles - start < budget) {
    if (d.out_valid_o && d.out_ready_i) {
      const int b = d.out_bone_o;
      if (b >= 0 && b < nb && !seen[static_cast<size_t>(b)]) {
        seen[static_cast<size_t>(b)] = true;
        auto& m = pal[static_cast<size_t>(b)];
        m[0] = d.out_m0_o;  m[1] = d.out_m1_o;  m[2]  = d.out_m2_o;
        m[3] = d.out_m3_o;  m[4] = d.out_m4_o;  m[5]  = d.out_m5_o;
        m[6] = d.out_m6_o;  m[7] = d.out_m7_o;  m[8]  = d.out_m8_o;
        m[9] = d.out_m9_o;  m[10] = d.out_m10_o; m[11] = d.out_m11_o;
        ++got;
      }
    }
    tick();
    if (got == nb && !d.busy_o) break;
  }

  // A HANG READS AS "NOT FIRED", which is the toolchain trap this repository
  // has already paid for once. Say which it was.
  check(got == nb,
        "the mutant still completed its palette -- a wedge here would be "
        "indistinguishable from a silent counter");

  std::printf("geom_bonesrc_latefetch_mutant: bone_prefetch_late_o = %u after %ld cycles\n",
              static_cast<unsigned>(d.bone_prefetch_late_o), cycles - start);

  // THE POINT OF THE FILE.
  check(d.bone_prefetch_late_o > 0u,
        "POSITIVE CONTROL: bone_prefetch_late_o FIRED under a stalled fill");

  // And it fired about the right thing: the palette is actually wrong.
  zref::creature::CreatureType t;
  t.skeleton.bone_count = static_cast<uint8_t>(nb);
  for (int b = 0; b < nb; ++b) {
    t.skeleton.bones[b].parent = bones[static_cast<size_t>(b)].parent;
    t.skeleton.bones[b].tx = bones[static_cast<size_t>(b)].tx;
    t.skeleton.bones[b].ty = bones[static_cast<size_t>(b)].ty;
    t.skeleton.bones[b].tz = bones[static_cast<size_t>(b)].tz;
  }
  // See geom_bonesrc_directed.cpp: decode_pose's stride is bank.bone_count.
  t.bank.bone_count = static_cast<uint8_t>(nb);
  check(zref::creature::bake_skeleton(t.skeleton, t.baked), "reference bake accepted it");
  zref::creature::Clip clip;
  clip.slot_id = 0;
  clip.frame_count = 1;
  clip.root.assign(3, 0);
  for (int b = 0; b < nb; ++b) clip.quats.push_back(bone_quat(static_cast<size_t>(b)));
  t.bank.clips.push_back(clip);

  std::array<zref::creature::mat3x4fx, zref::creature::kMaxBones> want{};
  zref::creature::decode_pose(t, t.bank.clips[0], 0, want, nullptr);

  int differing = 0;
  for (int b = 0; b < nb; ++b)
    for (int i = 0; i < 12; ++i)
      if (pal[static_cast<size_t>(b)][static_cast<size_t>(i)] !=
          want[static_cast<size_t>(b)].m[i])
        ++differing;
  std::printf("geom_bonesrc_latefetch_mutant: %d palette elements disagree with zref\n",
              differing);
  check(differing > 0,
        "the mutant's palette DISAGREES with the reference -- the counter fired "
        "about the fault it names, not beside it");

  std::printf("geom_bonesrc_latefetch_mutant: PASSED (the mutation is present)\n");
  return 0;
}
