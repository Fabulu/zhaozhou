// u02_meshcheck — the COMMITTED mesh-integrity probe for creature 02.
//
// Bounded to this creature's acceptance questions (the bounded-validation
// law): the four balls and (later) the loop chain and the eye lenses must
// be watertight closed surfaces that cannot open under any pose.
//
//   1. MANIFOLD EDGES, position-keyed. Group vertices by exact bind
//      position (the same key the generated normals use), then count every
//      undirected edge of every meshlet against those groups: a closed
//      surface has every edge on exactly TWO triangles. One = a hole; three
//      or more = a non-manifold pinch. Meshlet splits duplicate the seam
//      ring, which position-keying reconciles by construction.
//   2. THE SEAM LAW (from zixx_meshcheck fault 1): coincident bind
//      positions must carry identical {b0,b1,w0}, or the skin is only
//      closed by luck and opens the moment the bones disagree. Trivial for
//      today's rigid balls; load-bearing the day the loop chain lands.
//   3. DEGENERATE TRIANGLES: zero-area faces in bind space (the sliver
//      class the segment-taper zipper produced, S4).
//
// Exit 0 = clean. Any failure prints and exits nonzero.

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <tuple>
#include <vector>

namespace zc = zref::creature;
#include "manafold.h"

namespace {

using PosKey = std::tuple<int32_t, int32_t, int32_t>;

struct EdgeInfo {
  int count = 0;
};

}  // namespace

int main() {
  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::printf("u02-meshcheck: FAIL compile produced no meshlets\n");
    return 1;
  }
  int rc = 0;

  // ---- group by bind position; check bind agreement --------------------
  std::map<PosKey, uint32_t> group_of;   // position -> group id
  std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> group_bind;  // b0,b1,w0
  std::vector<bool> group_bind_ok;
  uint32_t next_group = 0;
  std::vector<std::vector<uint32_t>> vert_group(T.mesh.size());
  for (size_t mi = 0; mi < T.mesh.size(); ++mi) {
    const zc::Meshlet& m = T.mesh[mi];
    vert_group[mi].resize(m.verts.size());
    for (size_t vi = 0; vi < m.verts.size(); ++vi) {
      const zc::SkinVertex& v = m.verts[vi];
      const PosKey key{v.x, v.y, v.z};
      auto it = group_of.find(key);
      if (it == group_of.end()) {
        it = group_of.emplace(key, next_group++).first;
        group_bind.emplace_back(v.b0, v.b1, v.w0);
        group_bind_ok.push_back(true);
      } else {
        const auto& gb = group_bind[it->second];
        if (std::get<0>(gb) != v.b0 || std::get<1>(gb) != v.b1 || std::get<2>(gb) != v.w0)
          group_bind_ok[it->second] = false;
      }
      vert_group[mi][vi] = it->second;
    }
  }
  uint32_t bind_bad = 0;
  for (bool ok : group_bind_ok)
    if (!ok) ++bind_bad;
  if (bind_bad != 0) {
    std::printf("u02-meshcheck: FAIL %u coincident-bind groups disagree on {b0,b1,w0}\n",
                bind_bad);
    rc = 1;
  }

  // ---- manifold edge count + degenerate faces --------------------------
  std::map<std::pair<uint32_t, uint32_t>, EdgeInfo> edges;
  uint32_t degenerate = 0;
  uint32_t tris = 0;
  for (size_t mi = 0; mi < T.mesh.size(); ++mi) {
    const zc::Meshlet& m = T.mesh[mi];
    for (size_t ti = 0; ti + 2 < m.idx.size(); ti += 3) {
      ++tris;
      const uint32_t g[3] = {vert_group[mi][m.idx[ti]], vert_group[mi][m.idx[ti + 1]],
                             vert_group[mi][m.idx[ti + 2]]};
      if (g[0] == g[1] || g[1] == g[2] || g[0] == g[2]) {
        ++degenerate;  // two corners share a position: zero area
        continue;      // do not count its edges (it contributes no surface)
      }
      // bind-space area check (exact cross product, s64)
      const zc::SkinVertex& a = m.verts[m.idx[ti]];
      const zc::SkinVertex& b = m.verts[m.idx[ti + 1]];
      const zc::SkinVertex& c = m.verts[m.idx[ti + 2]];
      const int64_t ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
      const int64_t vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
      const int64_t cx = uy * vz - uz * vy;
      const int64_t cy = uz * vx - ux * vz;
      const int64_t cz = ux * vy - uy * vx;
      if (cx == 0 && cy == 0 && cz == 0) {
        ++degenerate;
        continue;
      }
      for (int e = 0; e < 3; ++e) {
        uint32_t u = g[e], v = g[(e + 1) % 3];
        if (u > v) std::swap(u, v);
        ++edges[{u, v}].count;
      }
    }
  }
  // reverse lookup for diagnostics: group id -> one representative position
  std::vector<PosKey> group_pos(next_group);
  for (const auto& [k, gid] : group_of) group_pos[gid] = k;
  uint32_t open_edges = 0, pinched_edges = 0;
  int printed = 0;
  for (const auto& [k, info] : edges) {
    if (info.count == 1) {
      ++open_edges;
      if (printed < 8) {
        const PosKey& a = group_pos[k.first];
        const PosKey& b = group_pos[k.second];
        const auto mm = [](int32_t fx) {
          return static_cast<int>((static_cast<int64_t>(fx) * 1000) >> 16);
        };
        std::printf("  open edge (%d,%d,%d)-(%d,%d,%d) mm\n", mm(std::get<0>(a)),
                    mm(std::get<1>(a)), mm(std::get<2>(a)), mm(std::get<0>(b)),
                    mm(std::get<1>(b)), mm(std::get<2>(b)));
        ++printed;
      }
    }
    if (info.count > 2) ++pinched_edges;
  }
  if (degenerate != 0) {
    std::printf("u02-meshcheck: FAIL %u degenerate (zero-area) triangles\n", degenerate);
    rc = 1;
  }
  if (open_edges != 0) {
    std::printf("u02-meshcheck: FAIL %u open edges (holes)\n", open_edges);
    rc = 1;
  }
  if (pinched_edges != 0) {
    std::printf("u02-meshcheck: FAIL %u non-manifold edges (>2 faces)\n", pinched_edges);
    rc = 1;
  }

  std::printf(
      "u02-meshcheck: %zu meshlets, %u tris, %zu position groups, %zu edges — %s\n",
      T.mesh.size(), tris, group_of.size(), edges.size(), rc == 0 ? "CLEAN" : "FAILED");
  return rc;
}
