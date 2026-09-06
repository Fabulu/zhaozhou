// manafold_hinge_traj.cpp — the COMMITTED antenna-hinge trajectory dump for
// creature 02 (Manafold pass 7, Owner Direction 5 §2a: "do the antenna
// hinges move separately, with a real range of motion?").
//
// WHY THIS EXISTS. This question has been misjudged TWICE. The pass-6
// by-eye reviewer explicitly refused to score it: "On an orbiting camera,
// hinge motion cannot be separated from camera rotation." A trajectory
// plot of the POSED 3D SKELETON settles it independent of any camera or
// rendering choice at all. Per CLAUDE.md this project has ALREADY shipped
// one confident, wrong answer from measuring a rendered frame instead of
// the thing itself (the "94.8% submerged" terrain-clearance fault, caused
// by trying to separate "in front of" from "inside" from a single 2D
// image) -- so this tool reads positions from zc::decode_pose, never from
// pixels.
//
// Each hinge's posed ORIGIN is read exactly the way manafold_probe.cpp's
// REST ANCHOR TABLE does it: a synthetic SkinVertex sitting at the bone's
// own bind position, skinned with full rigid weight on that one bone. That
// reproduces the bone's posed world transform with no separate "bone
// origin" API needed.
//
// THREE frames of reference are emitted per hinge, per frame, per sub-key:
//   world:  the raw posed position (creature hover bob, travel, and any
//           whole-body attitude all included)
//   local:  ROOT-LOCAL -- world position with the ROOT bone's own
//           translation and rotation divided out (same transpose-of-a-
//           rigid-rotation trick manafold_probe.cpp uses for its ellipsoid
//           tests). A flat LOCAL trajectory means the bone's ORIGIN is not
//           moving relative to the body -- world position minus the body's
//           own bob/travel/tilt.
//   own:    PARENT-RELATIVE motion of a point one segment further out along
//           the bone's own rest axis, rigidly skinned to THIS bone and
//           expressed in its PARENT's frame. THIS is the number that
//           isolates the hinge's OWN rotation.
//
// WHY "own" EXISTS AND "local" ALONE IS NOT ENOUGH -- a finding from this
// tool's first run, kept here so nobody re-derives it the hard way. The
// bone chain is root -> junctionF -> neck -> hingeA -> hingeB -> hingeC ->
// hingeD, and decode_pose's own composition rule is A_b = A_parent * LR_b.
// LR_b(origin) = LR_b(0,0,0) = the bone's REST TRANSLATION alone -- a
// bone's own local ROTATION never moves its own origin, only the origins of
// its DESCENDANTS. So junctionF's root-local position is a flat 0 on every
// axis not because junctionF never rotates, but because rotating a pivot
// about itself does not translate it: junctionF's rotation shows up in
// NECK's position instead, neck's rotation shows up (mixed with
// junctionF's) in hingeA's position, and so on -- each bone's root-local
// position is the CUMULATIVE effect of every ANCESTOR's rotation, which is
// also why adjacent hinges correlate strongly (they share most of that
// history). "own" strips exactly the ancestor chain by re-basing into the
// immediate PARENT's frame instead of the root's, isolating one joint's own
// contribution the way an animator would read a rig: hingeD's isolated
// motion needs a synthetic probe point (it is the chain's last named bone),
// placed one more rest-length out along y using kLoopArcMm[5] -- the same
// "one more tube segment" the closure math already continues past D with,
// not a new invented distance.
//
// Committed (never a throwaway probe), per CLAUDE.md: "a probe that does
// this was written once and thrown away, so its numbers are unreproducible
// -- commit the probe."
//
// Usage: manafold-hinge-traj.exe [slot_id]
//   Default slot 2 ("channel"): manafold_art.h's antenna_knead gain table
//   (kKneadClipPm) gives channel the second-highest gain in the bank, and
//   build_channel() never touches kBRoot (no yaw, no lateral travel) -- so
//   any LOCAL/OWN motion this tool reports on slot 2 is genuinely the
//   hinges, not disguised whole-body rotation.
// Output: CSV to stdout, one row per hinge per sub-key per frame:
//   frame,sub,bone,x_world_mm,y_world_mm,z_world_mm,x_local_mm,y_local_mm,z_local_mm,x_own_mm,y_own_mm,z_own_mm

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <array>

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"
#include "render_helpers.hpp"
#include "zrender/internal.hpp"

namespace zc = zref::creature;
#include "manafold.h"

int main(int argc, char** argv) {
  const uint16_t want_slot = static_cast<uint16_t>(argc > 1 ? std::atoi(argv[1]) : 2);
  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::fprintf(stderr, "manafold-hinge-traj: FAIL compile produced no meshlets\n");
    return 1;
  }
  const zc::Clip* clip = nullptr;
  for (const zc::Clip& c : T.bank.clips) {
    if (c.slot_id == want_slot) {
      clip = &c;
      break;
    }
  }
  if (!clip) {
    std::fprintf(stderr, "manafold-hinge-traj: no clip with slot_id %u\n", want_slot);
    return 1;
  }

  struct HingeBone {
    const char* name;
    uint8_t id;
    uint8_t parent;   // frame the "own" column is expressed in
    int32_t probe_y;  // fx16 world-REST y of the probe point (rest rotations
                      // are identity, so this bone's local +y IS world +y
                      // at rest -- no rotation needed to place it)
  };
  const HingeBone bones[6] = {
      {"junctionF", u02::kBJunctionF, u02::kBRoot, T.baked.world_y[u02::kBNeck]},
      {"neck", u02::kBNeck, u02::kBJunctionF, T.baked.world_y[u02::kBHingeA]},
      {"hingeA", u02::kBHingeA, u02::kBNeck, T.baked.world_y[u02::kBHingeB]},
      {"hingeB", u02::kBHingeB, u02::kBHingeA, T.baked.world_y[u02::kBHingeC]},
      {"hingeC", u02::kBHingeC, u02::kBHingeB, T.baked.world_y[u02::kBHingeD]},
      // hingeD has no further NAMED bone: probe one more rest-length out
      // (kLoopArcMm[5]), the same length the closure math already carries
      // the tube on past D with -- not a distance invented for this tool.
      {"hingeD", u02::kBHingeD, u02::kBHingeC,
       T.baked.world_y[u02::kBHingeD] + u02::fxu(u02::kLoopArcMm[5])}};

  std::fprintf(stderr, "manafold-hinge-traj: slot %u, %u keys (both sub-keys)\n",
               want_slot, clip->frame_count);
  std::printf(
      "frame,sub,bone,x_world_mm,y_world_mm,z_world_mm,"
      "x_local_mm,y_local_mm,z_local_mm,x_own_mm,y_own_mm,z_own_mm\n");
  // Same transpose-of-rigid-rotation trick manafold_probe.cpp's ellipsoid
  // tests use throughout: reading a mat3x4fx's rotation columns across ROWS
  // (m[0], m[4], m[8], ...) gives the transpose of its 3x3, which is also
  // its inverse for a pure rotation -- so this maps a world delta into any
  // bone's own local frame with no matrix inversion.
  const auto to_local = [](const zc::mat3x4fx& rm, int32_t x, int32_t y, int32_t z,
                           int32_t& lx_mm, int32_t& ly_mm, int32_t& lz_mm) {
    const int64_t dx = x - rm.m[3], dy = y - rm.m[7], dz = z - rm.m[11];
    const int64_t lx = (rm.m[0] * dx + rm.m[4] * dy + rm.m[8] * dz) >> 16;
    const int64_t ly = (rm.m[1] * dx + rm.m[5] * dy + rm.m[9] * dz) >> 16;
    const int64_t lz = (rm.m[2] * dx + rm.m[6] * dy + rm.m[10] * dz) >> 16;
    lx_mm = static_cast<int32_t>((lx * 1000) >> 16);
    ly_mm = static_cast<int32_t>((ly * 1000) >> 16);
    lz_mm = static_cast<int32_t>((lz * 1000) >> 16);
  };
  for (uint16_t f = 0; f < clip->frame_count; ++f) {
    for (uint8_t sub = 0; sub < 2; ++sub) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, *clip, f, pose, nullptr, sub);
      const zc::mat3x4fx& rm = pose[u02::kBRoot];
      for (const HingeBone& hb : bones) {
        // world/local: the bone's OWN origin (cumulative ancestor rotation).
        zc::SkinVertex sv{T.baked.world_x[hb.id], T.baked.world_y[hb.id],
                          T.baked.world_z[hb.id], hb.id, hb.id, 64, 0, 0};
        int32_t x, y, z;
        zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
        int32_t lx_mm, ly_mm, lz_mm;
        to_local(rm, x, y, z, lx_mm, ly_mm, lz_mm);
        // own: a point one rest-segment further out, rigidly skinned to
        // THIS bone (weight 64 on hb.id, not the child), expressed in the
        // PARENT's frame -- isolates this bone's own rotation.
        zc::SkinVertex psv{T.baked.world_x[hb.id], hb.probe_y, T.baked.world_z[hb.id],
                           hb.id, hb.id, 64, 0, 0};
        int32_t px, py, pz;
        zc::skin_vertex(pose.data(), psv, px, py, pz, nullptr);
        int32_t ox_mm, oy_mm, oz_mm;
        to_local(pose[hb.parent], px, py, pz, ox_mm, oy_mm, oz_mm);
        std::printf(
            "%u,%u,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", f, sub, hb.name,
            static_cast<int32_t>((static_cast<int64_t>(x) * 1000) >> 16),
            static_cast<int32_t>((static_cast<int64_t>(y) * 1000) >> 16),
            static_cast<int32_t>((static_cast<int64_t>(z) * 1000) >> 16),
            lx_mm, ly_mm, lz_mm, ox_mm, oy_mm, oz_mm);
      }
    }
  }
  return 0;
}
