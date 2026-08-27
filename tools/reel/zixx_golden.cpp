// zixx_golden — the GOLDEN SNAPSHOT dumper for the Zixxtrixx model.
//
// PRESENTATION-V2-PLAN.md §8 task 1: "Freeze the evidence before touching
// anything ... current walk + attack clip bytes, pose CRCs per key,
// zixx_probe full output, 60 Hz contact sheets, and the source commit hash."
// This tool provides the first two, for EVERY clip (walk and attack are the
// frozen goldens; idle and fall come along because they are approved too):
//
//   <out>/clip-<slot>.bin      raw authored clip payload: frame_count,
//                              bone_count, then quats then root, little-end
//   <out>/pose-crcs.txt        per-key CRC32C of the DECODED pose palette
//                              (bone_count mat3x4fx), plus a whole-clip CRC
//
// A later run claiming "the walk is unchanged" cites these files: byte-equal
// clip bins and equal pose CRCs are the proof. Build exactly like zixx_probe
// (no cmake).
#include "zref/zref_creature.hpp"
#include "zrender/internal.hpp"
#include "zhao_abi.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace zc = zref::creature;
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) / 1000);
}
#include "zixxtrixx.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: zixx-golden <out-dir>\n");
    return 2;
  }
  const std::string out = argv[1];
  const zc::CreatureType& T = zixx::type();
  std::string crcpath = out + "/pose-crcs.txt";
  std::FILE* crcs = std::fopen(crcpath.c_str(), "w");
  if (!crcs) {
    std::fprintf(stderr, "cannot open %s\n", crcpath.c_str());
    return 2;
  }
  std::fprintf(crcs, "# Zixxtrixx golden pose CRCs (CRC32C over decoded mat3x4fx palette per key)\n");
  std::fprintf(crcs, "# bones=%d\n", (int)T.bank.bone_count);
  for (const zc::Clip& clip : T.bank.clips) {
    // raw clip payload
    const std::string bin = out + "/clip-" + std::to_string(clip.slot_id) + ".bin";
    std::FILE* f = std::fopen(bin.c_str(), "wb");
    if (!f) {
      std::fprintf(stderr, "cannot open %s\n", bin.c_str());
      return 2;
    }
    const uint32_t hdr[2] = {clip.frame_count, T.bank.bone_count};
    std::fwrite(hdr, sizeof(hdr), 1, f);
    std::fwrite(clip.quats.data(), sizeof(zc::quat16), clip.quats.size(), f);
    std::fwrite(clip.root.data(), sizeof(int32_t), clip.root.size(), f);
    std::fclose(f);
    // per-key decoded-pose CRCs
    uint32_t whole = 0;
    std::fprintf(crcs, "clip %d keys %d\n", clip.slot_id, clip.frame_count);
    for (uint16_t k = 0; k < clip.frame_count; ++k) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, clip, k, pose, nullptr, 0);
      const uint32_t c = zhao_abi::zhao_crc32c(
          0, pose.data(), sizeof(zc::mat3x4fx) * T.bank.bone_count);
      whole = zhao_abi::zhao_crc32c(whole, &c, sizeof(c));
      std::fprintf(crcs, "  k%03d %08x\n", k, c);
    }
    std::fprintf(crcs, "clip %d WHOLE %08x\n", clip.slot_id, whole);
  }
  std::fclose(crcs);
  std::printf("golden snapshot written to %s\n", out.c_str());
  return 0;
}
