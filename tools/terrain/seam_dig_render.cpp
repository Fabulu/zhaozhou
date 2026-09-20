// seam_dig_render.cpp -- owner ruling R65: a dig across a PATCH SEAM, before
// and after, at FINAL RESOLUTION, so the owner can decide by eye whether the
// declared half-cell step reads as a fault.
//
// WHAT THE RULING IS ABOUT
//
// Ruling R56 asked for layer F to become 65x65 and vertex-aligned. R65 found
// that pricing wrong -- it is +38.3% of the page stride, not +1.2%, because
// `zhao_terrain_jdoorbell.sv` 143 needs a POWER-OF-TWO page size -- and
// authorised the fallback: NEAREST TEXEL, with the error measured and
// declared. `zref::terrain::sheet_texel_for_vertex` is that law and
// `spec/terrain_rules.md` 9.3 carries the measured numbers:
//
//     vertices 0..31   +1/4 cell      texel centre sits a quarter cell late
//     vertex 32        -1/4 cell      texel 63's centre is at 31.75 cells
//     across a seam     1/2 cell      and from two DIFFERENT pages
//
// At the 1 m pitch this file uses, that is 0.25 m per vertex and a 0.50 m tear
// at every patch seam. Those are measurements (`stamp_to_bake_laws_directed`
// makes them, this file does not re-derive them). What a measurement cannot
// say is whether half a metre of jog in the rim of a crater READS as a crack at
// 384x240 under one key light -- and CLAUDE.md's first law is that only looking
// settles that. R65: "a half-cell step at every seam is an art defect, and only
// looking settles whether it reads. If it reads, the format moves (the packer
// does not exist yet, so it is cheaper now than ever)."
//
// WHAT IS RENDERED, three columns, the same camera and the same light:
//
//   BEFORE     the undug terrain. The seam is invisible here BY CONSTRUCTION --
//              both patches sample one continuous heightfield and share their
//              border vertices exactly -- so anything visible in the other two
//              columns is the DIG's doing and not the terrain's.
//   SHIPPED    the dig baked through `sheet_texel_for_vertex`: what v1 draws.
//   ALIGNED    the same dig evaluated at each vertex's own world position --
//              what a 65x65 vertex-aligned layer F would produce. This is the
//              thing R56 wanted, rendered so the difference has a size.
//
// ALIGNED IS NOT A SECOND IMPLEMENTATION OF THE LAW. It calls exactly the same
// `zref::surface::covers` with exactly the same envelope and geometry; the only
// difference is the POINT it asks about -- the vertex instead of the nearest
// texel centre. That is the whole of what the format change would buy, which is
// why it is the honest comparison.
//
// FINAL RESOLUTION IS THE POINT. 384x240 is VIDEO_Z60's canvas
// (`spec/commands.zidl`, video_mode). A step that is obvious at 640x400 and
// invisible at 240p is not a defect, and the crayon-grain lesson in CLAUDE.md
// is exactly this mistake in the other direction. The contact sheet shows the
// frames at 1:1 AND a labelled 3x magnification, because the owner needs both
// "does it read" and "what is it".
//
// Everything geometric comes from the reference: `zref::surface::covers` and
// `texel_wx/wz` (the stamp), `zref::terrain::stamp_depth` (strength -> depth),
// `zref::terrain::sheet_texel_for_vertex` (the resample under test) and
// `zref::terrain::tessellate` (the mesh). This file authors a terrain by eye in
// named constants (CLAUDE.md rule 6), places a camera and rasterises. It is the
// same shape as `tools/terrain/lod_readings_render.cpp`, which is the tool this
// campaign already used to put a terrain decision in front of the owner.
//
// Output: raw PPM frames into argv[1]; `tools/terrain/seam_dig_sheet.py`
// assembles the committed PNG. The PNG is the evidence; the PPMs are
// intermediates and are deleted.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "zref/zref_surface.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_page.hpp"
#include "zref/zref_terrain_tess.hpp"

namespace zt = zref::terrain;
namespace zs = zref::surface;

namespace {

// ---- the terrain and the dig: named, editable, authored by eye -------------
constexpr int kLat = 33;          // vertices per patch side (the lattice edge)
constexpr int kCells = kLat - 1;  // 32 cells
constexpr double kPitchM = 1.0;   // THE 1 m PITCH the declared error is quoted at
constexpr double kRollM = 1.10;   // gentle relief, so the dig is the subject
constexpr double kBumpM = 0.14;   // a little fine break-up, not enough to hide a step

// THE DIG, AND WHERE IT IS PUT IS THE WHOLE EXPERIMENT.
//
// The first version of this file centred the dig exactly ON the seam, which
// felt like the obvious choice and is the one placement where the fault
// CANNOT APPEAR. The left patch's vertex 32 samples texel 63, whose centre is
// half a cell short of the seam; the next patch's vertex 0 samples texel 0,
// mirror images about the seam, so a disc centred there covers both or neither
// at every row, the two pages agree everywhere, and the render shows a clean
// crater. It measured zero disagreements and it proved nothing -- a picture
// that cannot show the fault is not evidence that there is none.
//
// So the dig is placed with its RIM TANGENT to the seam: centre at x = 55 with
// radius 9 puts the rightmost point of the circle at exactly x = 64. The rim
// then runs ALONGSIDE the seam rather than across it, which is the case where
// a half-cell sampling offset flips coverage over a RUN of vertices instead of
// one. Four shared border vertices disagree between the two pages at this
// placement, against fewer as the centre moves away and NONE at all when the
//
// THAT IS DELIBERATELY THE WORST CASE, and the caption says so. The owner is
// being asked whether the fault reads when it is as bad as the law allows; if
// it does not read here, it does not read. Sweep the placement with argv[2] to
// see the milder ones.
constexpr double kDigCx = 55.0;      // metres; rim tangent to the seam at x = 64
constexpr double kDigCz = 48.0;      // metres, mid-patch in z (patch row 1)
constexpr double kDigRadiusM = 9.0;  // metres
constexpr uint8_t kDigStrength = 128;  // -> -3.25 m, "a spell impact" in the art table

// ---- the cameras, authored by eye against the rendered frames -------------
// Both look at the TANGENT POINT (64, 48) -- where the dig's rim runs along the
// seam -- from the +x side, so the crater fills the frame and the seam is seen
// obliquely rather than in plan. A plan view shows the jog as a kink in a
// curve, which is the easy picture and the wrong one: the game is played from
// about here.
//
// THE FRUSTUM IS KEPT ON THE TERRAIN. The first version put the eye outside the
// patches' z range with a wide field, so the bottom third of every frame was
// sky under the horizon and the crater sat small and off-centre. Both cameras
// now have their whole lower field on ground that exists.
constexpr double kEyeX = 88.0, kEyeY = 29.0, kEyeZ = 26.0;    // wide: ~45 m out
constexpr double kLookX = 60.0, kLookY = 0.0, kLookZ = 46.0;
constexpr double kFovDeg = 45.0;

// The close camera: about a character's viewing distance, the tear centred.
constexpr double kEyeX2 = 82.0, kEyeY2 = 11.0, kEyeZ2 = 36.0;  // close: ~24 m out, OUTSIDE the crater
constexpr double kLookX2 = 62.0, kLookY2 = -1.5, kLookZ2 = 48.0;
constexpr double kFovDeg2 = 28.0;

// VIDEO_Z60's canvas. FINAL RESOLUTION, and the whole point of the exercise.
constexpr int kW = 384, kH = 240;

double height_m(double x, double z) {
  const double roll = kRollM * std::sin(x * 0.047 + 0.7) * std::cos(z * 0.039 - 0.3);
  const double bumps = kBumpM * std::sin(x * 0.81) * std::sin(z * 0.97);
  return roll + bumps;
}

int32_t fx(double m) { return static_cast<int32_t>(std::lround(m * 65536.0)); }

// ---- a tiny rasteriser (the shape lod_readings_render.cpp uses) -----------
struct Img {
  std::vector<float> r, g, b, z;
  Img() : r(kW * kH, 0.40f), g(kW * kH, 0.50f), b(kW * kH, 0.64f), z(kW * kH, 1e30f) {}
};

struct Cam {
  double fx_, fy_, fz_, rx, ry, rz, ux, uy, uz, f, ex, ey, ez;
  Cam(double eX, double eY, double eZ, double lX, double lY, double lZ, double fov)
      : ex(eX), ey(eY), ez(eZ) {
    double dx = lX - eX, dy = lY - eY, dz = lZ - eZ;
    double n = std::sqrt(dx * dx + dy * dy + dz * dz);
    fx_ = dx / n; fy_ = dy / n; fz_ = dz / n;
    // right = WORLD-UP x forward, so screen-right is world +x when the camera
    // looks along +z. The mirrored convention renders a legible picture too and
    // is the harder one to check a world coordinate against, which is a real
    // cost when the whole point of the frame is "which side of x = 32".
    //
    // AND `up` MUST BE RECOMPUTED WITH IT, which is the trap. `up` is derived
    // from `right`, so flipping one flips the other and the frame comes out
    // ROTATED 180 DEGREES rather than mirrored -- sky at the bottom, which reads
    // as a broken camera rather than as a sign error. tools/terrain/
    // lod_readings_render.cpp records the same mistake in its own comment; it
    // was made again here anyway, and caught by looking at the sheet.
    rx = fz_; ry = 0; rz = -fx_;
    n = std::sqrt(rx * rx + rz * rz); rx /= n; rz /= n;
    ux = fy_ * rz - fz_ * ry; uy = fz_ * rx - fx_ * rz; uz = fx_ * ry - fy_ * rx;
    f = (kH / 2.0) / std::tan(fov * 3.14159265 / 360.0);
  }
  bool project(double x, double y, double z, double& sx, double& sy, double& d) const {
    x -= ex; y -= ey; z -= ez;
    d = x * fx_ + y * fy_ + z * fz_;
    if (d < 0.25) return false;
    sx = kW / 2.0 + f * (x * rx + y * ry + z * rz) / d;
    sy = kH / 2.0 - f * (x * ux + y * uy + z * uz) / d;
    return true;
  }
};

void raster(Img& im, const Cam& cam, const zt::MeshTri& t, float cr, float cg, float cb) {
  const double ax = t.ax / 65536.0, ay = t.ay / 65536.0, az = t.az / 65536.0;
  const double bx = t.bx / 65536.0, by = t.by / 65536.0, bz = t.bz / 65536.0;
  const double cx = t.cx / 65536.0, cy = t.cy / 65536.0, cz = t.cz / 65536.0;
  double ex = bx - ax, ey = by - ay, ez = bz - az, gx = cx - ax, gy = cy - ay, gz = cz - az;
  double nx = ey * gz - ez * gy, ny = ez * gx - ex * gz, nz = ex * gy - ey * gx;
  double nn = std::sqrt(nx * nx + ny * ny + nz * nz);
  if (nn == 0) return;
  nx /= nn; ny /= nn; nz /= nn;
  if (ny < 0) { nx = -nx; ny = -ny; nz = -nz; }
  // ONE key light, low and to the side. A step in a surface is read from the
  // SHADING BREAK across it, so a light straight down would hide the very thing
  // this render exists to show -- and a render that cannot show the fault is
  // not evidence that there is none.
  const double lx = 0.62, ly = 0.55, lz = -0.56;
  const double ln = std::sqrt(lx * lx + ly * ly + lz * lz);
  const double lam = std::max(0.0, (nx * lx + ny * ly + nz * lz) / ln) * 0.82 + 0.18;
  double s[3][3];
  if (!cam.project(ax, ay, az, s[0][0], s[0][1], s[0][2])) return;
  if (!cam.project(bx, by, bz, s[1][0], s[1][1], s[1][2])) return;
  if (!cam.project(cx, cy, cz, s[2][0], s[2][1], s[2][2])) return;
  const int x0 = std::max(0, static_cast<int>(std::floor(std::min({s[0][0], s[1][0], s[2][0]}))));
  const int x1 = std::min(kW - 1, static_cast<int>(std::ceil(std::max({s[0][0], s[1][0], s[2][0]}))));
  const int y0 = std::max(0, static_cast<int>(std::floor(std::min({s[0][1], s[1][1], s[2][1]}))));
  const int y1 = std::min(kH - 1, static_cast<int>(std::ceil(std::max({s[0][1], s[1][1], s[2][1]}))));
  const double area =
      (s[1][0] - s[0][0]) * (s[2][1] - s[0][1]) - (s[2][0] - s[0][0]) * (s[1][1] - s[0][1]);
  if (std::fabs(area) < 1e-9) return;
  for (int py = y0; py <= y1; ++py)
    for (int px = x0; px <= x1; ++px) {
      const double qx = px + 0.5, qy = py + 0.5;
      const double w0 =
          ((s[1][0] - qx) * (s[2][1] - qy) - (s[2][0] - qx) * (s[1][1] - qy)) / area;
      const double w1 =
          ((s[2][0] - qx) * (s[0][1] - qy) - (s[0][0] - qx) * (s[2][1] - qy)) / area;
      const double w2 = 1.0 - w0 - w1;
      // A HAIR OF SLACK ON THE EDGE TEST, and it is not cosmetic. With an exact
      // `< 0` the two triangles sharing an edge can BOTH reject a pixel the edge
      // passes through, and the background shows as a dotted line along it. In a
      // frame whose whole purpose is "is there a crack at the seam", a
      // rasteriser artefact that looks exactly like a crack is the worst
      // possible defect: it would have the owner ruling on my arithmetic error.
      // Double-covering costs nothing -- the z-buffer settles it.
      if (w0 < -1e-4 || w1 < -1e-4 || w2 < -1e-4) continue;
      const double depth = w0 * s[0][2] + w1 * s[1][2] + w2 * s[2][2];
      const int k = py * kW + px;
      if (depth >= im.z[k]) continue;
      im.z[k] = static_cast<float>(depth);
      im.r[k] = static_cast<float>(cr * lam);
      im.g[k] = static_cast<float>(cg * lam);
      im.b[k] = static_cast<float>(cb * lam);
    }
}

void write_ppm(const Img& im, const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) { std::printf("cannot write %s\n", path.c_str()); return; }
  std::fprintf(f, "P6\n%d %d\n255\n", kW, kH);
  for (int k = 0; k < kW * kH; ++k) {
    unsigned char px[3] = {
        static_cast<unsigned char>(std::clamp(im.r[k], 0.f, 1.f) * 255),
        static_cast<unsigned char>(std::clamp(im.g[k], 0.f, 1.f) * 255),
        static_cast<unsigned char>(std::clamp(im.b[k], 0.f, 1.f) * 255)};
    std::fwrite(px, 1, 3, f);
  }
  std::fclose(f);
}

// THE PATCH GRID. Three by three, and the size is not decoration: the first
// version used TWO patches (64 x 32 m) and every camera wide enough to show the
// crater also showed the terrain's own outer edge, so a third of each frame was
// void or horizon. A frame whose subject competes with an artefact of the test
// rig is not a frame the owner can judge. 96 x 96 m keeps the whole frustum on
// ground that exists.
constexpr int kPatchesX = 3;
constexpr int kPatchesZ = 3;
constexpr int kPatchCount = kPatchesX * kPatchesZ;

// THE SEAM UNDER TEST: between patch column 1 and column 2, world x = 64 m.
constexpr double kSeamX = 2.0 * kCells * kPitchM;

zs::Envelope envelope_of(int px, int pz) {
  zs::Envelope e;
  e.x0 = fx(px * kCells * kPitchM);
  e.x1 = fx((px + 1) * kCells * kPitchM);
  e.z0 = fx(pz * kCells * kPitchM);
  e.z1 = fx((pz + 1) * kCells * kPitchM);
  return e;
}

double g_dig_cx = kDigCx;   // argv[2] overrides, so the placement can be swept

zs::StampGeom dig_geom() {
  zs::StampGeom g;
  g.tx = fx(g_dig_cx);
  g.ty = fx(kDigCz);   // `ty` IS the z translation: transform2fx is 2D (x, y-as-z)
  g.radius = fx(kDigRadiusM);
  g.ring_width = 0;
  return g;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string out = argc > 1 ? argv[1] : ".";
  if (argc > 2) g_dig_cx = std::atof(argv[2]);

  const zs::StampGeom g = dig_geom();

  // ---- SURFACE.STAMP: one dig, written into EVERY patch's own sheet -------
  // One sheet per patch, because a patch owns its own layer F. That is the
  // fact the whole ruling turns on: the two samples either side of a seam come
  // from DIFFERENT PAGES, so no rounding rule inside one page can reconcile
  // them.
  std::vector<uint8_t> strength[kPatchCount];
  for (int pz = 0; pz < kPatchesZ; ++pz)
    for (int px = 0; px < kPatchesX; ++px) {
      const int p = pz * kPatchesX + px;
      strength[p].assign(zs::kSheetTexels, 0);
      const zs::Envelope e = envelope_of(px, pz);
      for (int j = 0; j < zs::kSheetDim; ++j)
        for (int i = 0; i < zs::kSheetDim; ++i)
          if (zs::covers(e, g, i, j)) strength[p][j * zs::kSheetDim + i] = kDigStrength;
    }

  // ---- TERRAIN.BAKE, twice: the shipped resample and the aligned ideal ----
  // Column 0 BEFORE, 1 SHIPPED, 2 ALIGNED.
  std::vector<zt::ComposedLattice> lat(3 * kPatchCount);
  for (int col = 0; col < 3; ++col)
    for (int pz = 0; pz < kPatchesZ; ++pz)
      for (int px = 0; px < kPatchesX; ++px) {
        const int p = pz * kPatchesX + px;
        zt::ComposedLattice& L = lat[col * kPatchCount + p];
        L.w = kLat; L.h = kLat; L.dual = false;
        L.wx.resize(kLat); L.wz.resize(kLat);
        L.top.resize(kLat * kLat);
        for (int i = 0; i < kLat; ++i) {
          L.wx[i] = fx((px * kCells + i) * kPitchM);
          L.wz[i] = fx((pz * kCells + i) * kPitchM);
        }
        for (int vj = 0; vj < kLat; ++vj)
          for (int vi = 0; vi < kLat; ++vi) {
            const double wx = (px * kCells + vi) * kPitchM;
            const double wz = (pz * kCells + vj) * kPitchM;
            int32_t h = fx(height_m(wx, wz));
            if (col == 1) {
              // THE SHIPPED LAW. Nearest texel, exactly as
              // `zref::terrain::sheet_texel_for_vertex` defines it.
              const uint32_t ti = zt::sheet_texel_for_vertex(static_cast<uint32_t>(vi));
              const uint32_t tj = zt::sheet_texel_for_vertex(static_cast<uint32_t>(vj));
              h += zt::stamp_depth(strength[p][tj * zs::kSheetDim + ti]);
            } else if (col == 2) {
              // THE ALIGNED IDEAL: the SAME circle, asked about the VERTEX's own
              // world position instead of the nearest texel centre. That is the
              // whole of what a 65x65 vertex-aligned layer F would buy, and it
              // is why this column is not a second implementation of anything --
              // `zref::surface::covers` is `d2 <= r2` on a texel centre, and
              // this is `d2 <= r2` on a vertex.
              const int64_t dx = static_cast<int64_t>(fx(wx)) - g.tx;
              const int64_t dz = static_cast<int64_t>(fx(wz)) - g.ty;
              const int64_t rr = static_cast<int64_t>(g.radius);
              h += (dx * dx + dz * dz <= rr * rr) ? zt::stamp_depth(kDigStrength) : 0;
            }
            L.top[vj * kLat + vi] = h;
          }
      }

  // ---- what the pictures are ABOUT, stated as a number ---------------------
  // A shared border vertex is patch (1,pz)'s vertex 32 AND patch (2,pz)'s
  // vertex 0 -- ONE world position. Under the shipped law those two read
  // DIFFERENT texels, 63 of one sheet and 0 of the next, whose centres are half
  // a cell apart in world space. Count where that makes them disagree, so the
  // sheet's caption is measured rather than asserted.
  const uint32_t t_last = zt::sheet_texel_for_vertex(kLat - 1);
  const uint32_t t_first = zt::sheet_texel_for_vertex(0);
  int disagreements = 0;
  double worst_m = 0.0;
  for (int pz = 0; pz < kPatchesZ; ++pz) {
    const int pl = pz * kPatchesX + 1, pr = pz * kPatchesX + 2;
    for (int vj = 0; vj < kLat; ++vj) {
      const uint32_t tj = zt::sheet_texel_for_vertex(static_cast<uint32_t>(vj));
      const uint8_t a = strength[pl][tj * zs::kSheetDim + t_last];
      const uint8_t b = strength[pr][tj * zs::kSheetDim + t_first];
      if (a == b) continue;
      ++disagreements;
      const double step = std::fabs((zt::stamp_depth(a) - zt::stamp_depth(b)) / 65536.0);
      worst_m = std::max(worst_m, step);
    }
  }
  std::printf("seam x = %.1f m; patch-left v32 -> texel %u, patch-right v0 -> texel %u\n",
              kSeamX, t_last, t_first);
  std::printf("shared border vertices whose two pages disagree: %d of %d\n", disagreements,
              kPatchesZ * kLat);
  std::printf("worst height tear at a shared vertex: %.4f m\n", worst_m);
  std::printf("dig depth at full strength %u: %.4f m (centre x=%.1f z=%.1f r=%.1f)\n",
              kDigStrength, zt::stamp_depth(kDigStrength) / 65536.0, g_dig_cx, kDigCz,
              kDigRadiusM);

  // ---- render ------------------------------------------------------------
  static const float kSoil[3] = {0.74f, 0.63f, 0.46f};
  const char* colname[3] = {"before", "shipped", "aligned"};
  const Cam cams[2] = {Cam(kEyeX, kEyeY, kEyeZ, kLookX, kLookY, kLookZ, kFovDeg),
                       Cam(kEyeX2, kEyeY2, kEyeZ2, kLookX2, kLookY2, kLookZ2, kFovDeg2)};
  const char* camname[2] = {"wide", "close"};
  for (int cam = 0; cam < 2; ++cam)
    for (int col = 0; col < 3; ++col) {
      Img im;
      for (int p = 0; p < kPatchCount; ++p)
        for (int sp = 0; sp < 16; ++sp) {
          zt::SubpatchJob job;
          job.ox = (sp & 3) * 8;
          job.oz = (sp >> 2) * 8;
          // LEVEL 0 EVERYWHERE, and that is deliberate: LOD is a different
          // ruling and a coarsened subpatch would drop the very vertices whose
          // placement is the question.
          job.level = 0;
          for (int s2 = 0; s2 < 4; ++s2) job.nlevel[s2] = 0;
          const zt::TessResult r = zt::tessellate(lat[col * kPatchCount + p], job);
          for (const zt::MeshTri& t : r.tris) raster(im, cams[cam], t, kSoil[0], kSoil[1], kSoil[2]);
        }
      write_ppm(im, out + "/seam_" + camname[cam] + "_" + colname[col] + ".ppm");
    }
  std::printf("wrote 6 frames at %dx%d (VIDEO_Z60's canvas) into %s\n", kW, kH, out.c_str());
  return 0;
}
