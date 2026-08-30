// zixx_meshcheck — the COMMITTED mesh-integrity probe (RUN 1939).
#include <string_view>
#include <cstdlib>
//
// Two faults this run proved need an instrument, both invisible to
// probe/choreo/planner/CRC (all green while the owner could SEE both):
//
//   1. THE SEAM CHECK. The head part and the body part deliberately share
//      the junction ring "bit-identical by construction" — but identical
//      BIND positions are only half the contract: if the two copies carry
//      different {b0,b1,w0}, they skin APART the moment the bones they
//      disagree about move. That is a pose-dependent OPEN SEAM: invisible
//      at rest, visible "in some positions" (the owner's exact report).
//      So: group every vertex by quantized bind position; for groups whose
//      members disagree on bind, skin every member through every key of
//      every clip and print the worst split in mm. The gate is ZERO
//      disagreeing groups — coincident bind positions must carry
//      identical binds, or the skin is only closed by luck.
//
//   2. THE STRETCH CHECK. A vertex bound to the wrong bone sits harmlessly
//      at rest (neighbouring bones are nearly aligned) and flies off in a
//      pose that separates them, dragging its triangles into a spike (the
//      owner's "weird stray triangle in the right eye" on death2). A
//      texture cannot cause it and a CRC cannot see it. So: every meshlet
//      edge's POSED length against its BIND length, every key of every
//      clip; flag ratio > 2.0 with absolute growth > 80 mm. The gate is
//      zero flagged edges.
//
// Ground-contact doctrine applied to the mesh itself: MEASURE the posed
// vertices, never infer from the rendered frame; and COMMIT the probe so
// the numbers are reproducible.
#include "zref/zref_creature.hpp"
#include "zrender/internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

namespace zc = zref::creature;
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) / 1000);
}
#include "zixxtrixx.h"

namespace {
int32_t to_mm(int64_t fx) { return static_cast<int32_t>(fx * 1000 >> 16); }
int64_t d2_mm(int32_t ax, int32_t ay, int32_t az, int32_t bx, int32_t by, int32_t bz) {
  const int64_t dx = to_mm(ax) - to_mm(bx);
  const int64_t dy = to_mm(ay) - to_mm(by);
  const int64_t dz = to_mm(az) - to_mm(bz);
  return dx * dx + dy * dy + dz * dz;
}

int32_t legacy_clamped_response(const zc::mat3x4fx* pose, const zc::SkinVertex& v,
                                int32_t lx, int32_t ly, int32_t lz) {
  // The pre-v10 law: transform/clamp each influence independently, then blend
  // the two scalar answers. Making each temporary vertex rigid lets the probe
  // reuse the production normal transform instead of copying its arithmetic.
  zc::SkinVertex a = v, b = v;
  a.b1 = a.b0;
  a.w0 = 64;
  b.b0 = b.b1;
  b.w0 = 64;
  const int32_t la = zc::skin_normal_lambert(pose, a, lx, ly, lz);
  const int32_t lb = zc::skin_normal_lambert(pose, b, lx, ly, lz);
  return static_cast<int32_t>((static_cast<int64_t>(v.w0) * la +
                               static_cast<int64_t>(64 - v.w0) * lb + 32) >> 6);
}
}  // namespace

int main(int argc, char** argv) {
  const zc::CreatureType& T = zixx::type();

  // Exact ownership companion for the triangle-ID render. It turns a rendered
  // meshlet/triangle ID into bind position, UV and skin data, avoiding visual
  // guesses about whether a shard belongs to the shell or an eye decal.
  if (argc == 4 && (std::string_view(argv[1]) == "--triangle" ||
                    std::string_view(argv[1]) == "--triangle-micro")) {
    const bool micro = std::string_view(argv[1]) == "--triangle-micro";
    const auto& rung = micro ? T.micro : T.mesh;
    const int mi = std::atoi(argv[2]);
    const int ti = std::atoi(argv[3]);
    if (mi < 0 || mi >= static_cast<int>(rung.size()) || ti < 0 ||
        3 * ti + 2 >= static_cast<int>(rung[mi].idx.size())) {
      std::printf("triangle %s %d/%d out of range\n", micro ? "micro" : "full", mi,
                  ti);
      return 2;
    }
    const auto& mesh = rung[mi];
    std::printf("triangle rung=%s mesh=%d tri=%d page=%u\n", micro ? "micro" : "full",
                mi, ti, mesh.page);
    for (int corner = 0; corner < 3; ++corner) {
      const int vi = mesh.idx[3 * ti + corner];
      const auto& v = mesh.verts[vi];
      std::printf("corner=%d vi=%d xyz_mm=%d,%d,%d uv=%u,%u bind=%u/%u/%u\n",
                  corner, vi, to_mm(v.x), to_mm(v.y), to_mm(v.z), v.u, v.v,
                  v.b0, v.b1, v.w0);
    }
    return 0;
  }

  // ---- V10 lighting root diagnostic: --light-trace <slot> ---------------
  // Badness-rank mixed-influence vertices in one bounded deforming clip,
  // choose the surface point where the old pre-clamped scalar blend disagrees
  // most with the normalised deformed normal, then print its 60 Hz temporal
  // trace alongside one incident posed face. This answers one acceptance
  // question without rerendering the catalogue.
  if (argc == 3 && std::string_view(argv[1]) == "--light-trace") {
    const int slot = std::atoi(argv[2]);
    const zc::Clip* clip = nullptr;
    for (const zc::Clip& c : T.bank.clips)
      if (c.slot_id == slot) clip = &c;
    if (clip == nullptr) {
      std::printf("slot %d not found\n", slot);
      return 2;
    }
    struct Pick {
      int m = -1, v = -1, tri = -1;
      int32_t max_diff = -1, old_jump = 0, new_jump = 0;
      int64_t score = -1;
    } best;
    const int ticks = static_cast<int>(clip->frame_count) * 2;
    for (int mi = 0; mi < static_cast<int>(T.mesh.size()); ++mi) {
      const auto& mesh = T.mesh[mi];
      for (int vi = 0; vi < static_cast<int>(mesh.verts.size()); ++vi) {
        const auto& v = mesh.verts[vi];
        if ((v.nx == 0 && v.ny == 0 && v.nz == 0) || v.b0 == v.b1 || v.w0 == 0 ||
            v.w0 == 64)
          continue;
        int32_t max_diff = 0, old_jump = 0, new_jump = 0;
        int32_t prev_old = 0, prev_new = 0;
        bool have_prev = false;
        for (int tick = 0; tick < ticks; ++tick) {
          std::array<zc::mat3x4fx, zc::kMaxBones> pose;
          zc::decode_pose(T, *clip, static_cast<uint16_t>(tick / 2), pose, nullptr,
                          static_cast<uint8_t>(tick & 1));
          const int32_t old_l = legacy_clamped_response(
              pose.data(), v, zref::render::kLightX, zref::render::kLightY,
              zref::render::kLightZ);
          const int32_t new_l = zc::skin_normal_lambert(
              pose.data(), v, zref::render::kLightX, zref::render::kLightY,
              zref::render::kLightZ);
          max_diff = std::max(max_diff, std::abs(old_l - new_l));
          if (have_prev) {
            old_jump = std::max(old_jump, std::abs(old_l - prev_old));
            new_jump = std::max(new_jump, std::abs(new_l - prev_new));
          }
          prev_old = old_l;
          prev_new = new_l;
          have_prev = true;
        }
        // Disagreement proves the laws differ; excess old temporal jump ranks
        // the visible weight-following failure without requiring it to exist at
        // every sample.
        const int64_t score = static_cast<int64_t>(max_diff) * 4 +
                              std::max(0, old_jump - new_jump);
        if (score <= best.score) continue;
        int tri = -1;
        for (size_t ti = 0; ti + 2 < mesh.idx.size(); ti += 3)
          if (mesh.idx[ti] == vi || mesh.idx[ti + 1] == vi || mesh.idx[ti + 2] == vi) {
            tri = static_cast<int>(ti);
            break;
          }
        best = Pick{mi, vi, tri, max_diff, old_jump, new_jump, score};
      }
    }
    if (best.m < 0 || best.tri < 0) {
      std::printf("slot %d has no traceable mixed-influence surface\n", slot);
      return 2;
    }
    const auto& mesh = T.mesh[best.m];
    const auto& v = mesh.verts[best.v];
    std::printf("light-trace slot=%d ticks=%d m=%d v=%d b=%d/%d w=%d "
                "max_old_new=%d max_jump_old=%d max_jump_new=%d\n",
                slot, ticks, best.m, best.v, v.b0, v.b1, v.w0, best.max_diff,
                best.old_jump, best.new_jump);
    std::printf("tick,key,sub,legacy,normalised,face_inward,face_outward\n");
    for (int tick = 0; tick < ticks; ++tick) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, *clip, static_cast<uint16_t>(tick / 2), pose, nullptr,
                      static_cast<uint8_t>(tick & 1));
      const int32_t old_l = legacy_clamped_response(
          pose.data(), v, zref::render::kLightX, zref::render::kLightY,
          zref::render::kLightZ);
      const int32_t new_l = zc::skin_normal_lambert(
          pose.data(), v, zref::render::kLightX, zref::render::kLightY,
          zref::render::kLightZ);
      int32_t xyz[3][3] = {};
      for (int k = 0; k < 3; ++k) {
        const auto& tv = mesh.verts[mesh.idx[static_cast<size_t>(best.tri + k)]];
        zc::skin_vertex(pose.data(), tv, xyz[k][0], xyz[k][1], xyz[k][2], nullptr);
      }
      const int32_t face_inward = zref::render::shade_flat_tri(
          xyz[0][0], xyz[0][1], xyz[0][2], xyz[1][0], xyz[1][1], xyz[1][2],
          xyz[2][0], xyz[2][1], xyz[2][2], nullptr);
      const int32_t face_outward = zref::render::shade_flat_tri(
          xyz[0][0], xyz[0][1], xyz[0][2], xyz[2][0], xyz[2][1], xyz[2][2],
          xyz[1][0], xyz[1][1], xyz[1][2], nullptr);
      std::printf("%d,%d,%d,%d,%d,%d,%d\n", tick, tick / 2, tick & 1, old_l,
                  new_l, face_inward, face_outward);
    }
    return 0;
  }

  // ---- hunt mode: --hunt <slot> <key> prints the top stretched edges of
  // BOTH rungs at one pose, so a visible spike can be named instead of
  // guessed at (the death2 right-eye triangle, item 13).
  if (argc == 4 && std::string_view(argv[1]) == "--hunt") {
    const int slot = std::atoi(argv[2]);
    const int key = std::atoi(argv[3]);
    for (const zc::Clip& clip : T.bank.clips) {
      if (clip.slot_id != slot) continue;
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, clip, static_cast<uint16_t>(key), pose, nullptr, 0);
      for (int rung = 0; rung < 2; ++rung) {
        const auto& mesh = rung == 0 ? T.mesh : T.micro;
        struct Hit {
          double ratio;
          int m, a, b;
          int32_t bind, posed;
        };
        std::vector<Hit> hits;
        for (int m = 0; m < static_cast<int>(mesh.size()); ++m) {
          const auto& M = mesh[m];
          for (size_t i = 0; i + 2 < M.idx.size(); i += 3) {
            for (int e = 0; e < 3; ++e) {
              const uint8_t a = M.idx[i + e], b = M.idx[i + (e + 1) % 3];
              const auto& va = M.verts[a];
              const auto& vb = M.verts[b];
              const double bl = std::sqrt(static_cast<double>(
                  d2_mm(va.x, va.y, va.z, vb.x, vb.y, vb.z)));
              int32_t ax, ay, az, bx, by, bz;
              zc::skin_vertex(pose.data(), va, ax, ay, az, nullptr);
              zc::skin_vertex(pose.data(), vb, bx, by, bz, nullptr);
              const double pl = std::sqrt(static_cast<double>(
                  d2_mm(ax, ay, az, bx, by, bz)));
              if (bl > 0.5 && pl / bl > 1.5)
                hits.push_back({pl / bl, m, a, b, static_cast<int32_t>(bl),
                                static_cast<int32_t>(pl)});
            }
          }
        }
        std::sort(hits.begin(), hits.end(),
                  [](const Hit& x, const Hit& y) { return x.ratio > y.ratio; });
        std::printf("slot %d key %d rung %d: %zu edges over 1.5x\n", slot, key,
                    rung, hits.size());
        for (size_t i = 0; i < hits.size() && i < 10; ++i) {
          const auto& h = hits[i];
          const auto& va = mesh[h.m].verts[h.a];
          const auto& vb = mesh[h.m].verts[h.b];
          std::printf("  m%d %.2fx %d->%d mm  v%d(bind %d,%d,%d b%d/%d w%d uv "
                      "%d,%d) v%d(bind %d,%d,%d b%d/%d w%d uv %d,%d)\n",
                      h.m, h.ratio, h.bind, h.posed, h.a, to_mm(va.x),
                      to_mm(va.y), to_mm(va.z), va.b0, va.b1, va.w0, va.u, va.v,
                      h.b, to_mm(vb.x), to_mm(vb.y), to_mm(vb.z), vb.b0, vb.b1,
                      vb.w0, vb.u, vb.v);
        }
      }
      return 0;
    }
    std::printf("slot %d not found\n", slot);
    return 2;
  }

  // ---- index every vertex of the base rung ------------------------------
  struct VRef {
    int m, v;
  };
  std::vector<VRef> all;
  for (int m = 0; m < static_cast<int>(T.mesh.size()); ++m)
    for (int v = 0; v < static_cast<int>(T.mesh[m].verts.size()); ++v)
      all.push_back({m, v});

  // ---- 1. bind-position groups with DISAGREEING binds -------------------
  std::map<std::tuple<int32_t, int32_t, int32_t>, std::vector<VRef>> groups;
  for (const VRef& r : all) {
    const zc::SkinVertex& v = T.mesh[r.m].verts[r.v];
    groups[{to_mm(v.x), to_mm(v.y), to_mm(v.z)}].push_back(r);
  }
  std::vector<std::vector<VRef>> suspects;
  for (auto& [key, refs] : groups) {
    if (refs.size() < 2) continue;
    const zc::SkinVertex& a = T.mesh[refs[0].m].verts[refs[0].v];
    bool differ = false;
    for (const VRef& r : refs) {
      const zc::SkinVertex& b = T.mesh[r.m].verts[r.v];
      if (b.b0 != a.b0 || b.b1 != a.b1 || b.w0 != a.w0) differ = true;
    }
    if (differ) suspects.push_back(refs);
  }
  std::printf("meshcheck: %zu verts, %zu shared-position groups, "
              "%zu with DISAGREEING binds\n",
              all.size(), groups.size(), suspects.size());
  for (const auto& refs : suspects) {
    const zc::SkinVertex& a = T.mesh[refs[0].m].verts[refs[0].v];
    std::printf("  bind (%d,%d,%d)mm:", to_mm(a.x), to_mm(a.y), to_mm(a.z));
    for (const VRef& r : refs) {
      const zc::SkinVertex& b = T.mesh[r.m].verts[r.v];
      std::printf(" [m%d v%d b0=%d b1=%d w0=%d]", r.m, r.v, b.b0, b.b1, b.w0);
    }
    std::printf("\n");
  }

  // ---- 2. per-clip worst seam split + worst edge stretch ----------------
  // edges once, deduped per meshlet
  struct Edge {
    int m;
    uint8_t a, b;
    int32_t bind_mm;
  };
  std::vector<Edge> edges;
  for (int m = 0; m < static_cast<int>(T.mesh.size()); ++m) {
    const auto& M = T.mesh[m];
    std::map<std::pair<uint8_t, uint8_t>, bool> seen;
    for (size_t i = 0; i + 2 < M.idx.size(); i += 3) {
      const uint8_t t[3] = {M.idx[i], M.idx[i + 1], M.idx[i + 2]};
      for (int e = 0; e < 3; ++e) {
        uint8_t a = t[e], b = t[(e + 1) % 3];
        if (a > b) std::swap(a, b);
        if (seen[{a, b}]) continue;
        seen[{a, b}] = true;
        const auto& va = M.verts[a];
        const auto& vb = M.verts[b];
        const int32_t l =
            static_cast<int32_t>(std::sqrt(static_cast<double>(
                d2_mm(va.x, va.y, va.z, vb.x, vb.y, vb.z))));
        edges.push_back({m, a, b, l});
      }
    }
  }

  int fail = 0;
  for (const zc::Clip& clip : T.bank.clips) {
    int64_t worst_split2 = 0;
    int split_key = -1;
    double worst_ratio = 0;
    int32_t worst_grow = 0;
    int stretch_key = -1, stretch_m = -1;
    uint8_t stretch_a = 0, stretch_b = 0;
    std::vector<std::array<int32_t, 3>> posed;
    for (uint16_t f = 0; f < clip.frame_count; ++f) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, clip, f, pose, nullptr, 0);
      // seam splits
      for (const auto& refs : suspects) {
        int32_t x0 = 0, y0 = 0, z0 = 0;
        for (size_t i = 0; i < refs.size(); ++i) {
          const auto& v = T.mesh[refs[i].m].verts[refs[i].v];
          int32_t x, y, z;
          zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
          if (i == 0) {
            x0 = x;
            y0 = y;
            z0 = z;
          } else {
            const int64_t d2 = d2_mm(x, y, z, x0, y0, z0);
            if (d2 > worst_split2) {
              worst_split2 = d2;
              split_key = f;
            }
          }
        }
      }
      // edge stretches — skin per meshlet once
      for (const Edge& e : edges) {
        const auto& M = T.mesh[e.m];
        int32_t ax, ay, az, bx, by, bz;
        zc::skin_vertex(pose.data(), M.verts[e.a], ax, ay, az, nullptr);
        zc::skin_vertex(pose.data(), M.verts[e.b], bx, by, bz, nullptr);
        const double l = std::sqrt(static_cast<double>(d2_mm(ax, ay, az, bx, by, bz)));
        const double ratio = e.bind_mm > 0 ? l / e.bind_mm : 1.0;
        const int32_t grow = static_cast<int32_t>(l) - e.bind_mm;
        if (ratio > worst_ratio && grow > 80) {  // report the worst
          worst_ratio = ratio;
          worst_grow = grow;
          stretch_key = f;
          stretch_m = e.m;
          stretch_a = e.a;
          stretch_b = e.b;
        }
      }
    }
    const int32_t split_mm =
        static_cast<int32_t>(std::sqrt(static_cast<double>(worst_split2)));
    std::printf("clip slot %d: worst seam split %d mm (key %d)", clip.slot_id,
                split_mm, split_key);
    // the FAIL threshold is tear-class only (RUN 1939 tuning): a bendy
    // serpent legitimately stretches adjacent-ring skin ~2-3x under the
    // dive's sharpest waves; a wrong-bone vertex or an open construction
    // reads far past that with hundreds of mm of growth.
    // DECLARED EXCEPTION (RUN 1939, item 8): the hit-family FOLD -- the
    // owner's "really bend the hit part of the snake out of shape" --
    // measures up to 4.93x / +291 mm for its two-key window (hit key 2,
    // dmg-top key 2), and the worst-key renders were judged: the crush
    // reads as violence done to the animal, no tearing artifact at 240p
    // (evidence/damage-fold-peaks.png, RUN 1939). The gate sits just
    // above the fold's measured worst, not at a round number.
    if (worst_ratio > 5.2 && worst_grow > 320) {
      const auto& va = T.mesh[stretch_m].verts[stretch_a];
      const auto& vb = T.mesh[stretch_m].verts[stretch_b];
      std::printf("  STRETCH FAIL ratio %.2f (+%d mm) key %d m%d "
                  "[v%d b%d/%d w%d uv %d,%d]-[v%d b%d/%d w%d uv %d,%d]",
                  worst_ratio, worst_grow, stretch_key, stretch_m, stretch_a,
                  va.b0, va.b1, va.w0, va.u, va.v, stretch_b, vb.b0, vb.b1,
                  vb.w0, vb.u, vb.v);
      ++fail;
    }
    if (split_mm > 1) ++fail;  // >1 mm of open seam anywhere is a fault
    std::printf("\n");
  }
  std::printf(fail == 0 ? "meshcheck: OK\n" : "meshcheck: %d FAULTS\n", fail);
  return fail == 0 ? 0 : 1;
}
