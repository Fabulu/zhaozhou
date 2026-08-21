// geom_pose_decode_directed.cpp — the pose chain against
// `zref::creature::decode_pose`.
//
// This is the first test in the creature path that is a WHOLE-CHAIN
// differential rather than a single operation: the RTL runs one quaternion
// engine and one shared multiply engine over a real skeleton, and every bone
// matrix it emits must equal the reference's, element for element.
//
// The chain is where the interesting failures live, and none of them are
// arithmetic:
//
//   * BONE 0 IS NOT A MULTIPLY. `a[0] = lr` outright, because bone 0 has no
//     parent to multiply by. (Not a rounding argument: an identity multiply
//     would be exactly lossless here. The point is that there is no parent
//     matrix, so a block that multiplied anyway would fold in whatever its
//     ancestor store last held -- and every other bone inherits the root.)
//   * THE ROOT DISPLACEMENT LANDS ON BONE 0 ONLY. Applying it to every bone
//     translates the creature's limbs apart; applying it to none pins the
//     creature to the origin while its animation plays.
//   * THE ANCESTOR STORE MUST SURVIVE. `A_parent` is read back out of a memory
//     many bones later. A deep chain is the only shape that tests it, so
//     section 4 builds one.
//   * ORDER: `A_parent * LR`, never the reverse.

#include "Vzhao_geom_pose_decode.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using zhao::check;
namespace zc = zref::creature;

const int32_t ONE = 1 << 16;

struct Fixture {
  zc::Skeleton sk;
  zc::SkeletonBake bake;
  zc::Clip clip;
  uint16_t frame = 0;
};

/** Build a skeleton + one-frame clip. `parents[i]` must satisfy parents[i] <= i. */
Fixture make_fixture(const std::vector<uint8_t>& parents, const std::vector<zc::quat16>& quats,
                     int32_t dx, int32_t dy, int32_t dz, int32_t step = ONE) {
  Fixture f;
  const uint8_t n = static_cast<uint8_t>(parents.size());
  f.sk.bone_count = n;
  for (uint8_t i = 0; i < n; ++i) {
    f.sk.bones[i].parent = parents[i];
    // Distinct rest translations per bone: a fixture where every bone sits at
    // the same offset would hide an indexing error in the skeleton fetch.
    f.sk.bones[i].tx = step * (i + 1);
    f.sk.bones[i].ty = -step * static_cast<int32_t>(i);
    f.sk.bones[i].tz = step / 2 * (i + 2);
  }
  const bool ok = zc::bake_skeleton(f.sk, f.bake);
  check(ok, "fixture: bake_skeleton accepts the skeleton", 1, ok ? 1 : 0);

  f.clip.frame_count = 1;
  f.clip.root = {dx, dy, dz};
  f.clip.quats = quats;
  f.frame = 0;
  return f;
}

/** Run the RTL over a fixture, returning the emitted palette. */
void run_rtl(Vzhao_geom_pose_decode& dut, const Fixture& f,
             std::array<zc::mat3x4fx, zc::kMaxBones>& got, int& beats, int& cycles) {
  const uint8_t n = f.sk.bone_count;
  for (auto& m : got) m = zc::mat3x4fx{};

  // Present the source data for whichever bone the block is asking about. The
  // fetch is combinational by contract, so this is re-evaluated every settle.
  auto present = [&]() {
    const uint32_t bi = dut.bone_idx_o < n ? dut.bone_idx_o : 0;
    dut.bone_parent_i = f.sk.bones[bi].parent;
    dut.bone_tx_i = static_cast<uint32_t>(f.sk.bones[bi].tx);
    dut.bone_ty_i = static_cast<uint32_t>(f.sk.bones[bi].ty);
    dut.bone_tz_i = static_cast<uint32_t>(f.sk.bones[bi].tz);
    const zc::quat16& q = f.clip.quats[bi];
    dut.quat_w_i = static_cast<uint16_t>(q.q[0]);
    dut.quat_x_i = static_cast<uint16_t>(q.q[1]);
    dut.quat_y_i = static_cast<uint16_t>(q.q[2]);
    dut.quat_z_i = static_cast<uint16_t>(q.q[3]);
    for (int i = 0; i < 12; ++i) {
      dut.inv_rest_i[i] = static_cast<uint32_t>(f.bake.inv_rest[bi].m[i]);
    }
    dut.eval();
  };

  dut.bone_count_i = n;
  dut.root_dx_i = static_cast<uint32_t>(f.clip.root[0]);
  dut.root_dy_i = static_cast<uint32_t>(f.clip.root[1]);
  dut.root_dz_i = static_cast<uint32_t>(f.clip.root[2]);
  dut.out_ready_i = 1;
  dut.start_i = 1;
  present();
  zhao::tick(dut);
  dut.start_i = 0;
  present();

  beats = 0;
  cycles = 0;
  const int kGuard = 200000;
  while (cycles < kGuard) {
    if (dut.out_valid_o && dut.out_ready_i) {
      const uint32_t bi = dut.out_bone_o;
      if (bi < zc::kMaxBones) {
        for (int i = 0; i < 12; ++i) got[bi].m[i] = static_cast<int32_t>(dut.out_m_o[i]);
      }
      ++beats;
    }
    if (dut.done_o) break;
    zhao::tick(dut);
    present();
    ++cycles;
  }
}

void diff(Vzhao_geom_pose_decode& dut, const Fixture& f, const char* what) {
  zc::CreatureType type;
  type.skeleton = f.sk;
  type.baked = f.bake;
  type.bank.bone_count = f.sk.bone_count;

  std::array<zc::mat3x4fx, zc::kMaxBones> want{};
  zc::decode_pose(type, f.clip, f.frame, want, nullptr);

  std::array<zc::mat3x4fx, zc::kMaxBones> got{};
  int beats = 0, cycles = 0;
  run_rtl(dut, f, got, beats, cycles);

  const std::string t(what);
  check(beats == f.sk.bone_count, (t + ": one beat per bone").c_str(), f.sk.bone_count,
        static_cast<uint64_t>(beats));

  for (int bi = 0; bi < f.sk.bone_count; ++bi) {
    for (int i = 0; i < 12; ++i) {
      char lbl[192];
      std::snprintf(lbl, sizeof lbl, "%s: bone %d m[%d]", t.c_str(), bi, i);
      check(got[bi].m[i] == want[bi].m[i], lbl,
            static_cast<uint64_t>(static_cast<uint32_t>(want[bi].m[i])),
            static_cast<uint32_t>(got[bi].m[i]));
    }
  }
}

zc::quat16 q_of(int16_t w, int16_t x, int16_t y, int16_t z) {
  zc::quat16 q;
  q.q[0] = w; q.q[1] = x; q.q[2] = y; q.q[3] = z;
  return q;
}

// PCG RXS-M-XS, the committed test PRNG shape (qformats §7.5).
struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

}  // namespace

int main(int argc, char** argv) {
  Vzhao_geom_pose_decode dut;
  dut.rst_n = 0;
  dut.start_i = 0;
  dut.out_ready_i = 1;
  dut.bone_count_i = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  const int16_t QONE = 16384;  // zc::kQuatOne
  const int16_t H = 11585;     // cos(45) == sin(45) in S1.0.14

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0xB07Eu);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      const uint8_t n = static_cast<uint8_t>(1 + rng.below(zc::kMaxBones));
      std::vector<uint8_t> parents(n);
      parents[0] = 0;
      // parent < b, which is what bake_skeleton validates. Drawing from the
      // whole prefix produces both deep chains and wide fans.
      for (uint8_t i = 1; i < n; ++i) parents[i] = static_cast<uint8_t>(rng.below(i));
      std::vector<zc::quat16> quats(n);
      for (uint8_t i = 0; i < n; ++i) {
        quats[i] = q_of(static_cast<int16_t>(rng.next()), static_cast<int16_t>(rng.next()),
                        static_cast<int16_t>(rng.next()), static_cast<int16_t>(rng.next()));
      }
      Fixture f = make_fixture(parents, quats, static_cast<int32_t>(rng.next()) >> 12,
                               static_cast<int32_t>(rng.next()) >> 12,
                               static_cast<int32_t>(rng.next()) >> 12);
      char tag[96];
      std::snprintf(tag, sizeof tag, "random[%u] bones=%u", it, n);
      diff(dut, f, tag);
    }
    dut.final();
    return zhao::report_and_exit("geom_pose_decode_random");
  }

  // ---- 1. one bone, identity rotation, no displacement --------------------
  // The smallest whole chain: bone 0 takes the no-multiply path, and
  // S_0 = A_0 * inv_rest[0] must land back on identity because the rest
  // inverse is exactly the negated rest translation.
  {
    Fixture f = make_fixture({0}, {q_of(QONE, 0, 0, 0)}, 0, 0, 0);
    diff(dut, f, "one bone, identity, no root displacement");
  }

  // ---- 2. the root displacement is bone 0's alone -------------------------
  // With a non-zero displacement and more than one bone, an implementation
  // that applied it everywhere -- or nowhere -- diverges on every bone but the
  // root, which is exactly the shape that would look like "the creature is
  // slightly wrong" rather than "the creature is broken".
  {
    Fixture f = make_fixture({0, 0, 1}, {q_of(QONE, 0, 0, 0), q_of(QONE, 0, 0, 0),
                                         q_of(QONE, 0, 0, 0)},
                             3 * ONE, -7 * ONE, 2 * ONE);
    diff(dut, f, "root displacement on bone 0 only");
  }

  // ---- 3. real rotations through a parent chain ---------------------------
  {
    Fixture f = make_fixture({0, 0, 1, 2},
                             {q_of(H, H, 0, 0), q_of(H, 0, H, 0), q_of(H, 0, 0, H),
                              q_of(0, QONE, 0, 0)},
                             ONE, ONE, ONE);
    diff(dut, f, "rotations composed down a chain");
  }

  // ---- 4. THE ANCESTOR STORE, at depth ------------------------------------
  // A 32-bone straight chain: bone 31's matrix depends on all thirty-one
  // ancestors, each read back out of the store long after it was written. A
  // store that dropped or aliased an entry cannot survive this, and no shallow
  // fixture would notice.
  {
    std::vector<uint8_t> parents(32);
    std::vector<zc::quat16> quats(32);
    parents[0] = 0;
    for (uint8_t i = 1; i < 32; ++i) parents[i] = static_cast<uint8_t>(i - 1);
    for (uint8_t i = 0; i < 32; ++i) {
      // A different small rotation per bone so no two ancestors are alike.
      quats[i] = q_of(static_cast<int16_t>(QONE - 40 * i), static_cast<int16_t>(37 * i),
                      static_cast<int16_t>(-23 * i), static_cast<int16_t>(11 * i));
    }
    Fixture f = make_fixture(parents, quats, 5 * ONE, -2 * ONE, ONE);
    diff(dut, f, "32-bone straight chain (the deepest the format allows)");
  }

  // ---- 5. a wide fan: every bone a child of the root ----------------------
  // The opposite shape. Here the store is read from the SAME address thirty-one
  // times, which is where a read-after-write hazard would show.
  {
    std::vector<uint8_t> parents(16, 0);
    std::vector<zc::quat16> quats(16);
    for (uint8_t i = 0; i < 16; ++i) {
      quats[i] = q_of(H, static_cast<int16_t>(100 * i), static_cast<int16_t>(-50 * i), 0);
    }
    Fixture f = make_fixture(parents, quats, 0, 0, 0);
    diff(dut, f, "wide fan, every bone parented to the root");
  }

  // ---- 6. a zero-bone palette ---------------------------------------------
  // Legal and decodes to nothing. Treating it as an error would make an empty
  // creature type a hang rather than an empty palette.
  {
    dut.bone_count_i = 0;
    dut.out_ready_i = 1;
    dut.start_i = 1;
    dut.eval();
    zhao::tick(dut);
    dut.start_i = 0;
    dut.eval();
    int guard = 0;
    while (!dut.done_o && guard++ < 64) {
      zhao::tick(dut);
      dut.eval();
    }
    check(dut.done_o == 1, "a zero-bone palette completes rather than hanging", 1, dut.done_o);
    zhao::tick(dut);
    dut.eval();
    check(dut.busy_o == 0, "and returns to idle", 0, dut.busy_o);
  }

  // ---- 7. backpressure stalls the decode, it does not drop bones ----------
  // The palette is a stream. A consumer that goes away mid-palette must get
  // every bone when it comes back, in order.
  {
    Fixture f = make_fixture({0, 0, 1, 2, 3}, std::vector<zc::quat16>(5, q_of(H, H, 0, 0)),
                             ONE, ONE, ONE);
    zc::CreatureType type;
    type.skeleton = f.sk;
    type.baked = f.bake;
    type.bank.bone_count = f.sk.bone_count;
    std::array<zc::mat3x4fx, zc::kMaxBones> want{};
    zc::decode_pose(type, f.clip, f.frame, want, nullptr);

    auto present = [&]() {
      const uint32_t bi = dut.bone_idx_o < f.sk.bone_count ? dut.bone_idx_o : 0;
      dut.bone_parent_i = f.sk.bones[bi].parent;
      dut.bone_tx_i = static_cast<uint32_t>(f.sk.bones[bi].tx);
      dut.bone_ty_i = static_cast<uint32_t>(f.sk.bones[bi].ty);
      dut.bone_tz_i = static_cast<uint32_t>(f.sk.bones[bi].tz);
      const zc::quat16& q = f.clip.quats[bi];
      dut.quat_w_i = static_cast<uint16_t>(q.q[0]);
      dut.quat_x_i = static_cast<uint16_t>(q.q[1]);
      dut.quat_y_i = static_cast<uint16_t>(q.q[2]);
      dut.quat_z_i = static_cast<uint16_t>(q.q[3]);
      for (int i = 0; i < 12; ++i) {
        dut.inv_rest_i[i] = static_cast<uint32_t>(f.bake.inv_rest[bi].m[i]);
      }
      dut.eval();
    };

    dut.bone_count_i = f.sk.bone_count;
    dut.root_dx_i = static_cast<uint32_t>(f.clip.root[0]);
    dut.root_dy_i = static_cast<uint32_t>(f.clip.root[1]);
    dut.root_dz_i = static_cast<uint32_t>(f.clip.root[2]);
    dut.out_ready_i = 0;          // consumer is asleep from the very start
    dut.start_i = 1;
    present();
    zhao::tick(dut);
    dut.start_i = 0;
    present();

    // Let it run a long while with no consumer. It must be holding, not losing.
    for (int i = 0; i < 300; ++i) {
      zhao::tick(dut);
      present();
    }
    check(dut.done_o == 0, "a stalled palette does not complete", 0, dut.done_o);

    std::vector<int> order;
    std::array<zc::mat3x4fx, zc::kMaxBones> got{};
    dut.out_ready_i = 1;
    present();
    int guard = 0;
    while (guard++ < 100000) {
      if (dut.out_valid_o && dut.out_ready_i) {
        const uint32_t bi = dut.out_bone_o;
        order.push_back(static_cast<int>(bi));
        if (bi < zc::kMaxBones) {
          for (int i = 0; i < 12; ++i) got[bi].m[i] = static_cast<int32_t>(dut.out_m_o[i]);
        }
      }
      if (dut.done_o) break;
      zhao::tick(dut);
      present();
    }

    check(order.size() == f.sk.bone_count, "every bone arrives after the stall lifts",
          f.sk.bone_count, static_cast<uint64_t>(order.size()));
    bool in_order = true;
    for (size_t i = 0; i < order.size(); ++i) in_order = in_order && (order[i] == static_cast<int>(i));
    check(in_order, "and they arrive in bone order", 1, in_order ? 1 : 0);
    bool same = true;
    for (int bi = 0; bi < f.sk.bone_count; ++bi) {
      for (int i = 0; i < 12; ++i) same = same && (got[bi].m[i] == want[bi].m[i]);
    }
    check(same, "and the stall changed none of the values", 1, same ? 1 : 0);
  }

  dut.final();
  return zhao::report_and_exit("geom_pose_decode_directed");
}
