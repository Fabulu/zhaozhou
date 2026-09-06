// zref_island_scene.hpp — an ISLAND, drawn. The join between the sparse patch
// directory and the software reference renderer's terrain path.
//
// ---------------------------------------------------------------------------
// WHAT WAS ACTUALLY MISSING, STATED PRECISELY
// ---------------------------------------------------------------------------
// `reports/Missingterrain` says of the reference renderer:
//
//     "The software reference renderer can also render multiple manually
//      supplied patch commands in one frame: it gathers every DrawProcedural
//      terrain record and loops over the resulting patch list. So even the
//      reference path is not fundamentally restricted to one patch."
//
// That is TRUE, and it was checked rather than believed:
// `render_frame.cpp:308` pushes one `TerrainInst` per DrawProcedural record
// into a `std::vector`, and `render_frame.cpp:505` loops over every element of
// it, per view. Nothing in the packet caps the count either -- a
// DrawProcedural record is 64 B (`zhao_abi.h`, static_assert) against a
// 1 MiB frame slot, so an entire 793-patch island is ~50 KiB of command
// stream, comfortably inside one legal frame.
//
// The missing word in that sentence is MANUALLY. Every frame in this tree ---
// `tests/render/render_heightfield.cpp`, `render_golden.cpp`,
// `render_budget.cpp`, `tools/reel/zhao_reel.cpp` --- emits exactly ONE
// `zhao_pack_draw_procedural` call, against exactly one hand-registered
// `TerrainPatch`. So the same report's other sentence is the operative one:
//
//     "The existing example island is consequently a manually registered patch
//      resource, not the visible portion of a world selected by a world
//      manager."
//
// The capability existed. The route from `island::Directory` to that
// capability did not, and this header is that route and nothing else.
//
// ---------------------------------------------------------------------------
// WHY THIS IS A SEPARATE HEADER AND NOT A SECOND RENDERER
// ---------------------------------------------------------------------------
// It builds an ABI command stream and hands it to `SoftwareRenderer`. It does
// not project, shade, tessellate or resolve anything: if this file ever grows
// a triangle, the design has gone wrong, because a second terrain path would
// be a second thing to keep bit-exact.
//
// It also does not decide WHICH patches are visible. `island::visible_set`
// (zref_island.hpp) owns that rule, because TERRAIN.VISIBLE's RTL is written
// against it; this header calls it and would be a drift source if it looped
// over the window itself.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES NOT DO -- SAID OUT LOUD SO THE GAP IS NOT MISREAD AS CLOSED
// ---------------------------------------------------------------------------
// * It does not stream. Residency lifetime is `island::Streamer` +
//   `residency::Arena`; a caller that wants churn drives both and compares.
// * It does not LOD. Every issued patch draws at its authored lattice
//   resolution, so a window's cost is linear in patches. TERRAIN.LOD's
//   projected-error decision is not modelled here.
// * It does not cull to the frustum. The view window is square, conservative
//   patch-space hysteresis (see `View`), not a frustum test, so a frame draws
//   a superset of what a frustum would.
// * It does not author heights. `page()` returns a reference and the CALLER
//   fills the lattice. A harness that invented terrain would be deciding a
//   value, which is the owner's to choose.

#ifndef ZREF_ISLAND_SCENE_HPP
#define ZREF_ISLAND_SCENE_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "zhao_abi.h"

#include "zref/zref_frame.hpp"
#include "zref/zref_island.hpp"
#include "zref/zref_render.hpp"

namespace zref {
namespace island {

// ---------------------------------------------------------------- placement --

// The world rectangle patch (ix, iz) occupies, fx16 raw, from the island's own
// origin and pitch. This is the ONE place patch coordinates become metres.
//
// ADJACENT PATCHES SHARE AN EDGE, and that is deliberate: patch ix spans
// [ix*P, (ix+1)*P] and patch ix+1 starts at exactly (ix+1)*P, so the 33rd
// vertex column of one lattice lands on the 1st of the next (charter §11.1:
// 32 cells, 33 vertices). Making the envelopes disjoint instead would open a
// one-cell crack down every patch boundary in the island.
struct Envelope {
  int32_t x0 = 0, z0 = 0, x1 = 0, z1 = 0;  // fx16 raw
};

// fx16 raw has ±32,768 m of range (terrain_rules §1.3). An island large enough
// to run off it is a real error and is REPORTED, never wrapped: a silently
// wrapped envelope puts a patch on the far side of the world and the picture
// still looks plausible because the patch simply is not where you were looking.
inline bool patch_envelope(const Desc& d, int32_t ix, int32_t iz, Envelope* out) {
  const int64_t p = static_cast<int64_t>(patch_metres(d.pitch_log2)) << 16;  // fx16 raw
  const int64_t x0 = static_cast<int64_t>(d.origin_x) + static_cast<int64_t>(ix) * p;
  const int64_t z0 = static_cast<int64_t>(d.origin_z) + static_cast<int64_t>(iz) * p;
  const int64_t x1 = x0 + p, z1 = z0 + p;
  const int64_t lo = -2147483648LL, hi = 2147483647LL;
  if (x0 < lo || x1 > hi || z0 < lo || z1 > hi) return false;
  if (out) {
    out->x0 = static_cast<int32_t>(x0);
    out->z0 = static_cast<int32_t>(z0);
    out->x1 = static_cast<int32_t>(x1);
    out->z1 = static_cast<int32_t>(z1);
  }
  return true;
}

// World position -> the patch coordinate containing it. FLOOR, not truncation:
// a camera at x = -1 m is in patch -1, and C's `/` would put it in patch 0,
// which makes the visible window jump by one patch as the camera crosses the
// island origin -- a discontinuity that only shows up on maps whose origin is
// not at the corner.
inline int32_t patch_of_world(const Desc& d, int32_t origin_raw, int32_t world_raw) {
  const int64_t p = static_cast<int64_t>(patch_metres(d.pitch_log2)) << 16;
  const int64_t rel = static_cast<int64_t>(world_raw) - static_cast<int64_t>(origin_raw);
  int64_t q = rel / p;
  if (rel % p != 0 && rel < 0) --q;  // floor
  return static_cast<int32_t>(q);
}

// The camera's view window, in the patch coordinates the directory speaks.
// `radius` is the caller's hysteresis choice and stays a knob.
inline View view_for_camera(const Desc& d, int32_t cam_x_raw, int32_t cam_z_raw, int32_t radius) {
  View v;
  v.centre_ix = patch_of_world(d, d.origin_x, cam_x_raw);
  v.centre_iz = patch_of_world(d, d.origin_z, cam_z_raw);
  v.radius = radius;
  return v;
}

// ------------------------------------------------------------------- scene ---

// What one frame's worth of island-to-command-stream actually cost.
//
// `issued` and `with_page` are DIFFERENT NUMBERS and are kept apart on purpose.
// The directory saying a patch is ground is a statement about the WORLD; a page
// body being registered is a statement about MEMORY, and the whole point of the
// residency layer is that the second can be false while the first is true. A
// single "drawn" counter would have merged them and a scene missing half its
// pages would look identical to a scene over open sky.
struct FrameLedger {
  WindowTally tally;         // what the directory was asked, and answered
  uint32_t issued = 0;       // DrawProcedural records emitted
  uint32_t with_page = 0;    // ...whose page body is registered here
  uint32_t no_page_body = 0; // ...whose is NOT: the renderer will count a miss
  uint32_t unplaceable = 0;  // envelope ran off fx16; NOT issued
  uint32_t records = 0;      // total records in the packet, incl. state-setting
};

struct FramePlan {
  std::vector<uint8_t> packet;   // a sealed, validatable ABI frame
  std::vector<Visible> issued;   // in visible_set's row-major emission order
  FrameLedger ledger;
};

// The scene: an island directory, the page bodies that back its patches, and
// the ability to turn a camera into one frame of ABI commands.
//
// PAGE BODIES ARE OWNED BY POINTER-STABLE STORAGE. `RenderResources` holds raw
// `const TerrainPatch*`, so anything that reallocates (a bare std::vector of
// bodies) dangles every previously handed-out pointer the moment the 793rd
// patch is added. Each body therefore gets its own allocation.
class Scene {
 public:
  Scene(const Directory& dir, uint32_t material_handle, const render::Material& mat)
      : dir_(dir), material_handle_(material_handle) {
    res_.materials.push_back({material_handle, mat});
  }

  const Directory& directory() const { return dir_; }
  const render::RenderResources& resources() const { return res_; }
  uint32_t material_handle() const { return material_handle_; }

  // Register (or fetch) the page body for a patch the directory already owns.
  //
  // The handle is THE DIRECTORY'S, never one this class invents: that is the
  // whole link being built. A DrawProcedural whose `program` came from
  // anywhere else would prove nothing about the island.
  //
  // The lattice extent is the caller's (33x33 is the canonical patch, but a
  // test may want a cheaper one and should be able to say so); the ENVELOPE is
  // this class's, because patch placement is a property of the island and not
  // of the page. Returns nullptr if the patch is not ground, or if it cannot be
  // placed in fx16.
  render::TerrainPatch* page(int32_t ix, int32_t iz, uint16_t lattice_w, uint16_t lattice_h) {
    const Lookup l = dir_.find(ix, iz);
    if (l.outcome != Outcome::kResident) return nullptr;
    Envelope e;
    if (!patch_envelope(dir_.desc(), ix, iz, &e)) return nullptr;

    const auto it = bodies_.find(l.page_handle);
    if (it != bodies_.end()) return it->second.get();

    std::unique_ptr<render::TerrainPatch> p(new render::TerrainPatch());
    p->width = lattice_w;
    p->height = lattice_h;
    p->env_x0 = e.x0;
    p->env_z0 = e.z0;
    p->env_x1 = e.x1;
    p->env_z1 = e.z1;
    p->heights.assign(static_cast<size_t>(lattice_w) * lattice_h, 0);
    render::TerrainPatch* raw = p.get();
    bodies_.emplace(l.page_handle, std::move(p));
    res_.terrain_patches.push_back({l.page_handle, raw});
    return raw;
  }

  std::size_t page_count() const { return bodies_.size(); }

  bool has_page(uint32_t handle) const { return bodies_.find(handle) != bodies_.end(); }

  // ---- the frame ---------------------------------------------------------
  //
  // One SetPresentationContract, one SetView, then ONE DrawProcedural PER
  // VISIBLE PATCH, in `visible_set`'s row-major emission order.
  //
  // A patch the directory calls ground but for which no page body exists IS
  // STILL ISSUED. That is the fail-safe reading capture_format.md §3 asks for
  // and render_frame.cpp implements: the record resolves to nothing, the
  // renderer counts it in `resource_misses`, and geometry that DID draw is
  // unaffected. Skipping it here would hide a missing page inside a smaller
  // command count, which is exactly the kind of quiet subtraction that makes a
  // half-loaded island look like a small one.
  FramePlan build_frame(uint32_t frame_id, const zhao_abi::ZhMat4fx& view_projection,
                        const View& v, zhao_abi::video_mode mode = zhao_abi::VIDEO_Z60,
                        uint16_t view_id = 0) const {
    FramePlan plan;
    const std::vector<Visible> vis = visible_set(dir_, v, &plan.ledger.tally);

    zhao::ZhaoFrameBuilder b;
    b.begin_frame(frame_id, 1, 0, 0);
    ++plan.ledger.records;

    {
      zhao_abi::ZhRecordSetPresentationContract spc =
          zhao_abi::zhao_sample_set_presentation_contract();
      spc.payload.mode = mode;
      std::vector<uint8_t> bytes;
      zhao_abi::zhao_pack_set_presentation_contract(spc, bytes);
      b.append_record(bytes);
      ++plan.ledger.records;
    }
    {
      zhao_abi::ZhRecordSetView sv = zhao_abi::zhao_sample_set_view();
      sv.payload.view_id = view_id;
      sv.payload.view_projection = view_projection;
      std::vector<uint8_t> bytes;
      zhao_abi::zhao_pack_set_view(sv, bytes);
      b.append_record(bytes);
      ++plan.ledger.records;
    }

    for (const Visible& p : vis) {
      if (!patch_envelope(dir_.desc(), p.ix, p.iz, nullptr)) {
        ++plan.ledger.unplaceable;
        continue;
      }
      zhao_abi::ZhRecordDrawProcedural dp = zhao_abi::zhao_sample_draw_procedural();
      dp.payload.program = p.page_handle;  // THE DIRECTORY'S handle
      dp.payload.material = material_handle_;
      dp.payload.transform = identity_xform();
      dp.payload.screen_error = 1 << 16;
      dp.payload.kind = zhao_abi::FORGE_HEIGHTFIELD_PATCH;
      std::vector<uint8_t> bytes;
      zhao_abi::zhao_pack_draw_procedural(dp, bytes);
      b.append_record(bytes);
      plan.issued.push_back(p);
      ++plan.ledger.issued;
      ++plan.ledger.records;
      if (has_page(p.page_handle))
        ++plan.ledger.with_page;
      else
        ++plan.ledger.no_page_body;
    }

    b.end_frame(0);
    ++plan.ledger.records;
    plan.packet = b.seal(frame_id, frame_id, 1);
    return plan;
  }

 private:
  // The patch is already placed by its ENVELOPE, so the record's transform is
  // identity. Placing it twice -- once in the envelope and once in the
  // transform -- is how a patch ends up at double its offset.
  static zhao_abi::ZhTransform2fx identity_xform() {
    zhao_abi::ZhTransform2fx t;
    t.tx = 0;
    t.ty = 0;
    t.r00 = 1 << 16;
    t.r01 = 0;
    t.r10 = 0;
    t.r11 = 1 << 16;
    return t;
  }

  const Directory& dir_;
  uint32_t material_handle_ = 0;
  std::map<uint32_t, std::unique_ptr<render::TerrainPatch>> bodies_;
  render::RenderResources res_;
};

// ------------------------------------------------------------------ camera ---

// A top-down orthographic map camera, translated to a world point.
//
// ORTHO ON PURPOSE for the island-scale route: with w == 1 every vertex is at
// the same depth, screen x/y come only from world x/z, and NO patch can overlap
// another in screen space. So "which patches are on screen" is decidable by
// hand from the envelopes, and a patch-count claim is not resting on a
// perspective divide. `rows`: clip.x = s*x + tx, clip.y = s*z + tz, w = 1
// (qformats §2 exact row sum with the 4th component = fx16 1).
//
// A perspective island vista is a DIFFERENT and still-undemonstrated frame; see
// this header's "what it does not do".
inline zhao_abi::ZhMat4fx ortho_map_at(int32_t scale_raw, int32_t cam_x_raw, int32_t cam_z_raw) {
  const int32_t tx =
      static_cast<int32_t>(-((static_cast<int64_t>(scale_raw) * cam_x_raw) >> 16));
  const int32_t tz =
      static_cast<int32_t>(-((static_cast<int64_t>(scale_raw) * cam_z_raw) >> 16));
  zhao_abi::ZhMat4fx m{};
  m.m00 = scale_raw; m.m01 = 0; m.m02 = 0;         m.m03 = tx;
  m.m10 = 0;         m.m11 = 0; m.m12 = scale_raw; m.m13 = tz;
  m.m20 = 0;         m.m21 = 0; m.m22 = 1 << 16;   m.m23 = 0;
  m.m30 = 0;         m.m31 = 0; m.m32 = 0;         m.m33 = 1 << 16;
  return m;
}

}  // namespace island
}  // namespace zref

#endif  // ZREF_ISLAND_SCENE_HPP
