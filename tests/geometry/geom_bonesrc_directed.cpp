// geom_bonesrc_directed.cpp — entry I29's bone source, against the REFERENCE
// and against the REAL `zhao_geom_pose_decode`.
//
// Owner ruling R90's amendment made the ALM price of this block a precondition
// of building it, and the price came back 829 ALM for the arrangement here
// against 14,056 for the asynchronous one R90 described — a 17.0x difference
// bought by ONE structural claim:
//
//     the decoder's combinational source contract can be met by a PREFETCH in
//     front of synchronous memories, because `bone_idx_o` is a register that
//     moves once every ~115 cycles and a synchronous read is short by exactly
//     one cycle.
//
// That claim is about the JOIN between two blocks, so this bench holds BOTH of
// them, wired as the console would wire them. A bench that stubbed the decoder
// could not see a prefetch arriving a cycle late; this one decodes a real
// palette and compares every element against `zref::creature::decode_pose`.
//
// WHAT ACTUALLY DISCRIMINATES, named up front:
//
//   1. THE PALETTE, ELEMENT FOR ELEMENT, AGAINST THE REFERENCE. A prefetch one
//      cycle late feeds bone b−1's quaternion and rest translation into bone
//      b's chain. Every handshake still completes, `done_o` still rises and
//      `palettes_decoded_o` still increments — the palette is simply WRONG.
//      Only comparing the numbers can see it, which is why no counter is
//      trusted to stand in for this case.
//   2. A SECOND PALETTE, BACK TO BACK. The first draft of the block cleared
//      its held index on the REQUEST while the decoder still held the previous
//      palette's last bone, so a correct second run tripped the jump detector.
//      One palette could never have shown it. This case is why case 5 asserts
//      the counter reads ZERO rather than merely "did not crash".
//   3. THE BYTES COME FROM THE PACKER'S GOLDEN, not from this file. The RTL is
//      filled from `ladder_page_body_v1.bin`, decoded through
//      `zref::creature_page::body::decode_body` — so packer, reference model
//      and RTL are pinned to one artefact rather than to each other's good
//      intentions.
//   4. THE BAKE IS PINNED TO THE RATIFIED ONE. `body::bake_body` is a
//      restatement of `zref::creature::bake_skeleton`'s running sum, and case 2
//      requires the two to produce identical inverse-rest matrices over the
//      same skeleton. A restatement nothing compares is a second owner.
//   5. EVERY COUNTER IS SEEN TO MOVE, and the one that cannot be moved by legal
//      stimulus says so and names its mutant. A detector reading zero is a
//      claim.
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vtb_geom_bonesrc.h"
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

// The golden's own skeleton, restated here ONLY as the input the packer was
// given; every byte the RTL sees comes from the file, never from this list.
const std::vector<cb::BoneRecord> kGoldenBones = {
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 65536, 0, 0, 0, 0},
    {1, 16384, 32768, 0, 0, 0, 0},
    {2, 0, 49152, 0, 0, 0, 0},
    {0, -32768, 0, 16384, 0, 0, 0},
    {4, 0, -65536, -8192, 0, 0, 0},
};


// A distinct, non-identity rotation per bone, built from literal lanes so the
// fixture is deterministic and needs no table. IDENTITY WOULD HIDE THE BUG:
// with every bone carrying the same rotation, a prefetch one cycle late feeds
// bone b-1's quaternion into bone b's chain and the palette comes out RIGHT.
// The lanes must differ per bone for case 3 to discriminate at all.
zref::creature::quat16 bone_quat(size_t b) {
  const int16_t i = static_cast<int16_t>(b);
  return zref::creature::quat16{{static_cast<int16_t>(16384 - 300 * i),
                                 static_cast<int16_t>(1500 * (i + 1)),
                                 static_cast<int16_t>(700 * i),
                                 static_cast<int16_t>(-400 * i)}};
}

struct Bench {
  Vtb_geom_bonesrc& d;
  long cycles = 0;

  explicit Bench(Vtb_geom_bonesrc& dut) : d(dut) {}

  void tick() {
    d.clk = 0; d.eval();
    d.clk = 1; d.eval();
    ++cycles;
  }

  void reset() {
    d.rst_n = 0;
    d.fill_we_i = 0; d.fill_sel_i = 0; d.fill_bone_i = 0;
    d.fill_word_i = 0; d.fill_data_i = 0;
    d.req_i = 0; d.bone_count_i = 0;
    d.root_dx_i = 0; d.root_dy_i = 0; d.root_dz_i = 0;
    d.out_ready_i = 1;
    for (int i = 0; i < 4; ++i) tick();
    d.rst_n = 1;
    tick();
  }

  void fill(bool sel, int bone, int word, uint64_t data) {
    d.fill_we_i = 1; d.fill_sel_i = sel ? 1 : 0;
    d.fill_bone_i = static_cast<uint8_t>(bone);
    d.fill_word_i = static_cast<uint8_t>(word);
    d.fill_data_i = data;
    tick();
    d.fill_we_i = 0; d.fill_data_i = 0;
  }

  // Load one bone's 32-byte record as four little-endian 64-bit words, exactly
  // as a page read would deliver it.
  void fill_bone_bytes(int bone, const uint8_t* rec) {
    for (int w = 0; w < 4; ++w) {
      uint64_t v = 0;
      for (int b = 7; b >= 0; --b)
        v = (v << 8) | static_cast<uint64_t>(rec[w * 8 + b]);
      fill(false, bone, w, v);
    }
  }

  void fill_quat(int bone, const zref::creature::quat16& q) {
    uint64_t v = (static_cast<uint64_t>(static_cast<uint16_t>(q.q[3])) << 48) |
                 (static_cast<uint64_t>(static_cast<uint16_t>(q.q[2])) << 32) |
                 (static_cast<uint64_t>(static_cast<uint16_t>(q.q[1])) << 16) |
                 static_cast<uint64_t>(static_cast<uint16_t>(q.q[0]));
    fill(true, bone, 0, v);
  }

  // Run one palette and collect it. Returns false on a timeout, which is a
  // REAL failure here: the whole arrangement is a handshake and a wedge is the
  // failure mode a stub bench would never reach.
  bool run_palette(int bone_count,
                   std::vector<std::array<int32_t, 12>>& out,
                   long budget = 400000) {
    out.assign(static_cast<size_t>(bone_count), {});
    std::vector<bool> seen(static_cast<size_t>(bone_count), false);
    d.bone_count_i = static_cast<uint8_t>(bone_count);
    d.req_i = 1; tick(); d.req_i = 0;

    int got = 0;
    const long start = cycles;
    while (cycles - start < budget) {
      if (d.out_valid_o && d.out_ready_i) {
        const int b = d.out_bone_o;
        if (b >= 0 && b < bone_count && !seen[static_cast<size_t>(b)]) {
          seen[static_cast<size_t>(b)] = true;
          auto& m = out[static_cast<size_t>(b)];
          m[0] = d.out_m0_o;  m[1] = d.out_m1_o;  m[2]  = d.out_m2_o;
          m[3] = d.out_m3_o;  m[4] = d.out_m4_o;  m[5]  = d.out_m5_o;
          m[6] = d.out_m6_o;  m[7] = d.out_m7_o;  m[8]  = d.out_m8_o;
          m[9] = d.out_m9_o;  m[10] = d.out_m10_o; m[11] = d.out_m11_o;
          ++got;
        }
      }
      tick();
      if (got == bone_count && !d.busy_o) return true;
    }
    return false;
  }
};

// Build the reference's own CreatureType for the same skeleton, so the oracle
// is `decode_pose` itself and not a second copy of the chain written here.
zref::creature::CreatureType make_type(const std::vector<cb::BoneRecord>& bones) {
  zref::creature::CreatureType t;
  t.skeleton.bone_count = static_cast<uint8_t>(bones.size());
  for (size_t b = 0; b < bones.size(); ++b) {
    t.skeleton.bones[b].parent = bones[b].parent;
    t.skeleton.bones[b].tx = bones[b].tx;
    t.skeleton.bones[b].ty = bones[b].ty;
    t.skeleton.bones[b].tz = bones[b].tz;
  }
  // NOT skeleton.bone_count: `decode_pose` reads `type.bank.bone_count` for the
  // per-frame quaternion stride. Leaving it zero makes the ORACLE return a full
  // identity palette, which on first run read as an RTL fault and was not one --
  // the RTL's 64437 was the correct rescale(1500^2, 11) all along. An oracle
  // that silently degrades to identity is the broken-instrument law with the
  // roles swapped, so it is named here rather than fixed quietly.
  t.bank.bone_count = static_cast<uint8_t>(bones.size());
  const bool ok = zref::creature::bake_skeleton(t.skeleton, t.baked);
  check(ok, "reference bake_skeleton accepted the golden skeleton");
  return t;
}

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

}  // namespace

int main() {
  int cases = 0;

  // =========================================================================
  // CASE 1 — the reference model reproduces the PACKER's golden, byte for byte
  // =========================================================================
  char golden_path[512];
  std::snprintf(golden_path, sizeof golden_path,
                "%s/tests/golden/creature_ladder/ladder_page_body_v1.bin", ZHAO_SOURCE_DIR);
  const std::vector<uint8_t> golden = read_file(golden_path);
  check(!golden.empty(), "the body golden is present");

  const std::vector<cp::Record> recs = {
      {0x000100, 49152, 1024, 24576, 49152},
      {0x000101, 65536, 1311, 32768, 65536},
      {0x00A017, 163840, 4096, 81920, 163840},
  };
  const std::vector<uint8_t> built = cp::build_with_body(recs, kGoldenBones);
  check(built.size() == golden.size(), "model and packer agree on the page LENGTH");
  bool same = true;
  for (size_t i = 0; i < golden.size() && i < built.size(); ++i)
    if (golden[i] != built[i]) { std::printf("  byte %zu: %02X vs %02X\n", i, built[i], golden[i]); same = false; break; }
  check(same, "zref::creature_page::build_with_body reproduces the packer's golden byte for byte");
  ++cases;

  // The frozen half did not move. `body_off` is the ONLY header word an append
  // is allowed to touch, and the bodyless golden must still say 0.
  char bodyless_path[512];
  std::snprintf(bodyless_path, sizeof bodyless_path,
                "%s/tests/golden/creature_ladder/ladder_page_v1.bin", ZHAO_SOURCE_DIR);
  const std::vector<uint8_t> bodyless = read_file(bodyless_path);
  check(!bodyless.empty(), "the bodyless golden is still present");
  const uint32_t body_off = cp::get_u32(golden.data() + 8);
  check(cp::get_u32(bodyless.data() + 8) == 0u, "the bodyless page still declares body_off == 0");
  check(body_off == bodyless.size(), "body_off names the byte after the frozen half");
  bool frozen_same = true;
  for (size_t i = 0; i < bodyless.size(); ++i) {
    if (i >= 8 && i < 12) continue;  // the body_off word itself
    if (golden[i] != bodyless[i]) { frozen_same = false; break; }
  }
  check(frozen_same, "the append moved NO byte of the frozen half outside body_off");
  ++cases;

  // =========================================================================
  // CASE 2 — the page's bake agrees with the RATIFIED bake, matrix for matrix
  // =========================================================================
  std::vector<cb::BoneRecord> decoded;
  const cb::BodyVerdict v =
      cb::decode_body(golden.data() + body_off, golden.size() - body_off, decoded);
  check(v == cb::BodyVerdict::kOk, "the golden's body decodes kOk");
  check(decoded.size() == kGoldenBones.size(), "every bone came back");

  const zref::creature::CreatureType type = make_type(kGoldenBones);
  bool bake_same = true;
  for (size_t b = 0; b < decoded.size(); ++b) {
    int32_t m[12];
    cb::inv_rest_matrix(decoded[b], m);
    for (int i = 0; i < 12; ++i)
      if (m[i] != type.baked.inv_rest[b].m[i]) {
        std::printf("  bone %zu element %d: page %d vs bake_skeleton %d\n",
                    b, i, m[i], type.baked.inv_rest[b].m[i]);
        bake_same = false;
      }
  }
  check(bake_same,
        "the PAGE's baked inv_rest equals zref::creature::bake_skeleton's, all 12 elements");
  ++cases;

  // =========================================================================
  // CASE 3 — THE PALETTE. The real decoder, fed by the real source, against
  // the reference. This is the case the packet's structural claim lives in.
  // =========================================================================
  Vtb_geom_bonesrc dut;
  Bench bx(dut);
  bx.reset();

  // A clip frame with a real rotation on every bone, so a stale quaternion
  // cannot coincide with the right answer. Identity quaternions would make a
  // one-cycle-late prefetch INVISIBLE, which is the trap this case exists to
  // avoid.
  zref::creature::Clip clip;
  clip.slot_id = 0;
  clip.frame_count = 1;
  clip.root.assign(3, 0);
  for (size_t b = 0; b < kGoldenBones.size(); ++b) clip.quats.push_back(bone_quat(b));
  zref::creature::CreatureType type_c = type;
  type_c.bank.clips.push_back(clip);

  const int nb = static_cast<int>(kGoldenBones.size());
  for (int b = 0; b < nb; ++b) {
    bx.fill_bone_bytes(b, golden.data() + body_off + cb::kHeaderBytes + cb::kBoneBytes * b);
    bx.fill_quat(b, clip.quats[static_cast<size_t>(b)]);
  }
  check(dut.bone_fills_o == static_cast<uint32_t>(nb * 5),
        "bone_fills_o counted every fill word (4 body + 1 quat per bone)");
  check(dut.bone_reserved_nz_o == 0u, "no reserved field was set in the golden");
  check(dut.bone_rest_nonrigid_o == 0u, "every golden record declares RIGID_REST");

  std::vector<std::array<int32_t, 12>> pal;
  check(bx.run_palette(nb, pal), "the palette completed without wedging");

  std::array<zref::creature::mat3x4fx, zref::creature::kMaxBones> want{};
  zref::creature::decode_pose(type_c, type_c.bank.clips[0], 0, want, nullptr);

  int worst = 0, bad = 0;
  for (int b = 0; b < nb; ++b)
    for (int i = 0; i < 12; ++i) {
      const int32_t g = pal[static_cast<size_t>(b)][static_cast<size_t>(i)];
      const int32_t w = want[static_cast<size_t>(b)].m[i];
      if (g != w) {
        if (bad < 6)
          std::printf("  bone %d element %d: rtl %d vs zref %d\n", b, i, g, w);
        ++bad;
        const int d = g > w ? g - w : w - g;
        if (d > worst) worst = d;
      }
    }
  check(bad == 0,
        "every palette element equals zref::creature::decode_pose — the prefetch is never late");
  check(dut.bone_prefetch_late_o == 0u, "bone_prefetch_late_o did not move on a correct run");
  check(dut.palettes_decoded_o == 1u, "one palette decoded");
  ++cases;

  // =========================================================================
  // CASE 4 — A SECOND PALETTE, BACK TO BACK. The case that caught the block's
  // own false-positive detector.
  // =========================================================================
  std::vector<std::array<int32_t, 12>> pal2;
  check(bx.run_palette(nb, pal2), "the second palette completed without wedging");
  bool same2 = true;
  for (int b = 0; b < nb && same2; ++b)
    for (int i = 0; i < 12; ++i)
      if (pal2[static_cast<size_t>(b)][static_cast<size_t>(i)] !=
          pal[static_cast<size_t>(b)][static_cast<size_t>(i)]) { same2 = false; break; }
  check(same2, "the second palette is bit-identical to the first");
  check(dut.bone_prefetch_late_o == 0u,
        "bone_prefetch_late_o STILL reads zero across a back-to-back pair");
  check(dut.palettes_decoded_o == 2u, "two palettes decoded");
  ++cases;

  // =========================================================================
  // CASE 5 — THE COUNTERS FIRE. Each one is driven by the fault it names.
  // =========================================================================
  {
    Vtb_geom_bonesrc d2;
    Bench b2(d2);
    b2.reset();

    // RIGID_REST clear in word 0 -> bone_rest_nonrigid_o.
    b2.fill(false, 0, 0, 0x0000'0000'0000'0000ull);  // flags bit0 == 0
    check(d2.bone_rest_nonrigid_o == 1u, "POSITIVE CONTROL: bone_rest_nonrigid_o fired");
    // A record that DOES declare it must not move the counter.
    b2.fill(false, 1, 0, 0x0000'0000'0000'0100ull);  // flags bit0 == 1
    check(d2.bone_rest_nonrigid_o == 1u, "NEGATIVE CONTROL: a rigid record did not move it");

    // A reserved bit set in word 0 -> bone_reserved_nz_o.
    const uint32_t before = d2.bone_reserved_nz_o;
    b2.fill(false, 2, 0, 0x0000'0000'0001'0100ull);  // the +2 u16 is non-zero
    check(d2.bone_reserved_nz_o == before + 1u,
          "POSITIVE CONTROL: bone_reserved_nz_o fired on a reserved u16");
    b2.fill(false, 3, 3, 0x0000'0001'0000'0000ull);  // the +28 u32 is non-zero
    check(d2.bone_reserved_nz_o == before + 2u,
          "POSITIVE CONTROL: bone_reserved_nz_o fired on the trailing reserved u32");
    b2.fill(false, 4, 3, 0x0000'0000'DEAD'BEEFull);  // +24 inv_tz, +28 zero
    check(d2.bone_reserved_nz_o == before + 2u,
          "NEGATIVE CONTROL: a clean word 3 did not move it");
    ++cases;
  }

  // bone_prefetch_late_o: UNREACHABLE BY LEGAL STIMULUS, and that is stated
  // rather than glossed. The prefetch has ~115 cycles of notice for a fill that
  // needs seven, so no legal clip can make it late — which is exactly the shape
  // CLAUDE.md says needs a COMMITTED MUTANT rather than an argument.
  // `tests/mutants/zhao_geom_bonesrc_latefetch_mutant.sv` is that mutant and
  // `geom_bonesrc_latefetch_mutant` is its driver, whose polarity is inverted:
  // it passes when the counter FIRES. Cases 3 and 4 above are this counter's
  // negative control — it reads zero across two correct palettes.

  std::printf("geom_bonesrc_directed: %d cases PASSED\n", cases);
  return 0;
}
