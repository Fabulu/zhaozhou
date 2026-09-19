// lod_readings_render.cpp -- owner ruling R8's side-by-side: the SAME terrain,
// the SAME camera, the SAME ladder, tessellated under each reading of the LOD
// deviation law, so the owner can pick by eye.
//
//   morph reading (DEV_INCLUDE_BOUNDARY = 0): border ring excluded -- the
//                 provisional default, zref::terrain::kLodDevIncludeBoundary
//   mesh  reading (DEV_INCLUDE_BOUNDARY = 1): border ring included
//
// Everything that decides geometry is the REFERENCE's, not this file's:
// zref::terrain::lod_deviation (both readings), zref::terrain::lod_ladder
// (the level a camera allows), zref::terrain::tessellate (the mesh, stitched to
// the real neighbour levels, including across patch borders). This file only
// builds a terrain, places a camera and rasterises what the reference emits.
// The terrain is ART, authored by eye in named constants below (CLAUDE.md rule
// 6); it is not measured from anything.
//
// Output: raw PPM frames into the directory given as argv[1]; the PNG contact
// sheet is assembled by tools/terrain/lod_readings_sheet.py. The PNG is the
// evidence and the only thing committed; the PPMs are intermediates.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_lod.hpp"
#include "zref/zref_terrain_tess.hpp"

namespace zt = zref::terrain;

namespace {

// ---- the terrain: named, editable, authored by eye ------------------------
constexpr int kPatches = 4;                    // 4 x 4 patches
constexpr int kLat = 33;                       // vertices per patch side
constexpr double kPitchM = 2.0;                // canonical 2.0 m pitch (T-rulings)
constexpr double kRollM = 7.0;                 // rolling relief amplitude
constexpr double kRidgeM = 9.0;                // a sharp ridge running diagonally
constexpr double kCraterM = 6.0;               // a crater, the look of a scar
constexpr double kBumpM = 0.6;                 // fine bumps the ladder can drop

// ---- the camera ------------------------------------------------------------
constexpr double kEyeX = -70.0, kEyeY = 95.0, kEyeZ = -60.0;   // metres
constexpr double kLookX = 128.0, kLookY = 0.0, kLookZ = 118.0;
constexpr double kFovDeg = 58.0;
constexpr int kW = 640, kH = 400;

double height_m(double x, double z) {
  const double roll = kRollM * std::sin(x * 0.031 + 0.4) * std::cos(z * 0.026 - 0.2);
  const double d = (x - z * 0.85 - 30.0) / 6.0;                  // ridge line
  const double ridge = kRidgeM * std::exp(-d * d);
  const double cx = x - 170.0, cz = z - 90.0, r = std::sqrt(cx * cx + cz * cz);
  const double crater = -kCraterM * std::exp(-(r - 0.0) * (r - 0.0) / 300.0) +
                        0.35 * kCraterM * std::exp(-(r - 22.0) * (r - 22.0) / 30.0);
  const double bumps = kBumpM * std::sin(x * 0.9) * std::sin(z * 1.1);
  return roll + ridge + crater + bumps;
}

int32_t fx(double m) { return static_cast<int32_t>(std::lround(m * 65536.0)); }

struct Patch {
  zt::ComposedLattice lat;
  int level[16] = {};
};

// ---- a tiny rasteriser -----------------------------------------------------
struct Img {
  std::vector<float> r, g, b, z;
  Img() : r(kW * kH, 0.55f), g(kW * kH, 0.68f), b(kW * kH, 0.82f), z(kW * kH, 1e30f) {}
};

struct Cam {
  double fx_, fy_, fz_, rx, ry, rz, ux, uy, uz, f;
  Cam() {
    double dx = kLookX - kEyeX, dy = kLookY - kEyeY, dz = kLookZ - kEyeZ;
    double n = std::sqrt(dx * dx + dy * dy + dz * dz);
    fx_ = dx / n; fy_ = dy / n; fz_ = dz / n;
    // right = forward x world-up, up = right x forward: y-up, right-handed.
    // (The first version had right's sign flipped, so every frame came out
    // mirrored AND upside down; the inspection of the sheet caught it.)
    rx = -fz_; ry = 0; rz = fx_;
    n = std::sqrt(rx * rx + rz * rz); rx /= n; rz /= n;
    ux = ry * fz_ - rz * fy_; uy = rz * fx_ - rx * fz_; uz = rx * fy_ - ry * fx_;
    f = (kH / 2.0) / std::tan(kFovDeg * 3.14159265 / 360.0);
  }
  bool project(double x, double y, double z, double& sx, double& sy, double& d) const {
    x -= kEyeX; y -= kEyeY; z -= kEyeZ;
    d = x * fx_ + y * fy_ + z * fz_;
    if (d < 0.5) return false;
    sx = kW / 2.0 + f * (x * rx + y * ry + z * rz) / d;
    sy = kH / 2.0 - f * (x * ux + y * uy + z * uz) / d;
    return true;
  }
};

void raster(Img& im, const Cam& cam, const zt::MeshTri& t, float cr, float cg, float cb,
            bool wire) {
  const double ax = t.ax / 65536.0, ay = t.ay / 65536.0, az = t.az / 65536.0;
  const double bx = t.bx / 65536.0, by = t.by / 65536.0, bz = t.bz / 65536.0;
  const double cx = t.cx / 65536.0, cy = t.cy / 65536.0, cz = t.cz / 65536.0;
  // face normal and a fixed key light
  double ex = bx - ax, ey = by - ay, ez = bz - az, gx = cx - ax, gy = cy - ay, gz = cz - az;
  double nx = ey * gz - ez * gy, ny = ez * gx - ex * gz, nz = ex * gy - ey * gx;
  double nn = std::sqrt(nx * nx + ny * ny + nz * nz);
  if (nn == 0) return;
  nx /= nn; ny /= nn; nz /= nn;
  if (ny < 0) { nx = -nx; ny = -ny; nz = -nz; }
  const double lx = 0.45, ly = 0.8, lz = 0.35;
  const double lam = std::max(0.0, nx * lx + ny * ly + nz * lz) * 0.78 + 0.22;
  double s[3][3];
  if (!cam.project(ax, ay, az, s[0][0], s[0][1], s[0][2])) return;
  if (!cam.project(bx, by, bz, s[1][0], s[1][1], s[1][2])) return;
  if (!cam.project(cx, cy, cz, s[2][0], s[2][1], s[2][2])) return;
  const int x0 = std::max(0, static_cast<int>(std::floor(std::min({s[0][0], s[1][0], s[2][0]}))));
  const int x1 = std::min(kW - 1, static_cast<int>(std::ceil(std::max({s[0][0], s[1][0], s[2][0]}))));
  const int y0 = std::max(0, static_cast<int>(std::floor(std::min({s[0][1], s[1][1], s[2][1]}))));
  const int y1 = std::min(kH - 1, static_cast<int>(std::ceil(std::max({s[0][1], s[1][1], s[2][1]}))));
  const double area = (s[1][0] - s[0][0]) * (s[2][1] - s[0][1]) - (s[2][0] - s[0][0]) * (s[1][1] - s[0][1]);
  if (std::fabs(area) < 1e-9) return;
  for (int py = y0; py <= y1; ++py)
    for (int px = x0; px <= x1; ++px) {
      const double qx = px + 0.5, qy = py + 0.5;
      const double w0 = ((s[1][0] - qx) * (s[2][1] - qy) - (s[2][0] - qx) * (s[1][1] - qy)) / area;
      const double w1 = ((s[2][0] - qx) * (s[0][1] - qy) - (s[0][0] - qx) * (s[2][1] - qy)) / area;
      const double w2 = 1.0 - w0 - w1;
      if (w0 < 0 || w1 < 0 || w2 < 0) continue;
      const double depth = w0 * s[0][2] + w1 * s[1][2] + w2 * s[2][2];
      const int k = py * kW + px;
      if (depth >= im.z[k]) continue;
      im.z[k] = static_cast<float>(depth);
      const bool edge = wire && std::min({w0, w1, w2}) < 0.035;
      const float e = edge ? 0.35f : 1.0f;
      im.r[k] = static_cast<float>(cr * lam) * e;
      im.g[k] = static_cast<float>(cg * lam) * e;
      im.b[k] = static_cast<float>(cb * lam) * e;
    }
}

void write_ppm(const Img& im, const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) { std::printf("cannot write %s\n", path.c_str()); return; }
  std::fprintf(f, "P6\n%d %d\n255\n", kW, kH);
  for (int k = 0; k < kW * kH; ++k) {
    unsigned char px[3] = {static_cast<unsigned char>(std::clamp(im.r[k], 0.f, 1.f) * 255),
                           static_cast<unsigned char>(std::clamp(im.g[k], 0.f, 1.f) * 255),
                           static_cast<unsigned char>(std::clamp(im.b[k], 0.f, 1.f) * 255)};
    std::fwrite(px, 1, 3, f);
  }
  std::fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
  const std::string out = argc > 1 ? argv[1] : ".";
  const uint16_t scale = static_cast<uint16_t>(argc > 2 ? std::atoi(argv[2]) : 16000);

  // ---- the island: 4 x 4 patches cut from ONE heightfield, edges shared ----
  std::vector<Patch> patches(kPatches * kPatches);
  for (int pz = 0; pz < kPatches; ++pz)
    for (int px = 0; px < kPatches; ++px) {
      zt::ComposedLattice& lat = patches[pz * kPatches + px].lat;
      lat.w = kLat; lat.h = kLat; lat.dual = false;
      lat.wx.resize(kLat); lat.wz.resize(kLat);
      lat.top.resize(kLat * kLat);
      for (int i = 0; i < kLat; ++i) {
        lat.wx[i] = fx((px * 32 + i) * kPitchM);
        lat.wz[i] = fx((pz * 32 + i) * kPitchM);
      }
      for (int vj = 0; vj < kLat; ++vj)
        for (int vi = 0; vi < kLat; ++vi)
          lat.top[vj * kLat + vi] = fx(height_m((px * 32 + vi) * kPitchM, (pz * 32 + vj) * kPitchM));
    }

  zt::LodCamera cam;
  cam.ex = fx(kEyeX); cam.ey = fx(kEyeY); cam.ez = fx(kEyeZ);
  cam.scale = scale;

  const char* names[2] = {"morph", "mesh"};
  uint32_t tris[2] = {0, 0};
  int levels_hist[2][4] = {};
  std::vector<int> lv[2];
  for (int reading = 0; reading < 2; ++reading) {
    lv[reading].assign(kPatches * kPatches * 16, 0);
    for (size_t p = 0; p < patches.size(); ++p)
      for (int sp = 0; sp < 16; ++sp) {
        const int ox = (sp & 3) * 8, oz = (sp >> 2) * 8;
        zt::LodSubpatch s;
        const zt::ComposedLattice& lat = patches[p].lat;
        s.cx = lat.wx[ox + 4];
        s.cz = lat.wz[oz + 4];
        s.cy = lat.top[(oz + 4) * kLat + ox + 4];
        for (int L = 1; L < zt::kLodLevels; ++L)
          s.dev[L] = zt::lod_deviation(lat, zt::Surface::kTop, ox, oz, L, reading == 1);
        const int level = zt::lod_ladder(s, cam, 256);
        lv[reading][p * 16 + sp] = level;
        ++levels_hist[reading][level];
      }
  }
  int differ = 0;
  for (size_t i = 0; i < lv[0].size(); ++i) differ += lv[0][i] != lv[1][i];

  // global subpatch grid (16 x 16) -> level, for neighbour lookup across patches
  auto level_at = [&](int reading, int gx, int gz) -> int {
    gx = std::clamp(gx, 0, kPatches * 4 - 1);
    gz = std::clamp(gz, 0, kPatches * 4 - 1);
    const int p = (gz / 4) * kPatches + (gx / 4), sp = (gz % 4) * 4 + (gx % 4);
    return lv[reading][p * 16 + sp];
  };

  Cam c;
  for (int reading = 0; reading < 2; ++reading)
    for (int wire = 0; wire < 2; ++wire) {
      Img im;
      for (int pz = 0; pz < kPatches; ++pz)
        for (int px = 0; px < kPatches; ++px)
          for (int sp = 0; sp < 16; ++sp) {
            const int gx = px * 4 + (sp & 3), gz = pz * 4 + (sp >> 2);
            zt::SubpatchJob job;
            job.ox = (sp & 3) * 8; job.oz = (sp >> 2) * 8;
            job.level = level_at(reading, gx, gz);
            job.nlevel[zt::kSideNegZ] = level_at(reading, gx, gz - 1);
            job.nlevel[zt::kSidePosZ] = level_at(reading, gx, gz + 1);
            job.nlevel[zt::kSideNegX] = level_at(reading, gx - 1, gz);
            job.nlevel[zt::kSidePosX] = level_at(reading, gx + 1, gz);
            const zt::TessResult r = zt::tessellate(patches[pz * kPatches + px].lat, job);
            if (!wire) tris[reading] += static_cast<uint32_t>(r.tris.size());
            // level tint on the wire pass only: 0 cream, 1 green, 2 ochre, 3 rust
            static const float tint[4][3] = {{0.92f, 0.88f, 0.78f}, {0.62f, 0.78f, 0.52f},
                                             {0.86f, 0.70f, 0.38f}, {0.80f, 0.45f, 0.35f}};
            const float* col = wire ? tint[job.level] : tint[0];
            for (const zt::MeshTri& t : r.tris) raster(im, c, t, col[0], col[1], col[2], wire != 0);
          }
      write_ppm(im, out + "/lod_" + names[reading] + (wire ? "_wire" : "_shade") + ".ppm");
    }

  std::printf("scale=%u  subpatches=%zu  level differs on %d\n", scale, lv[0].size(), differ);
  for (int r = 0; r < 2; ++r)
    std::printf("%-5s levels L0=%d L1=%d L2=%d L3=%d  triangles=%u\n", names[r], levels_hist[r][0],
                levels_hist[r][1], levels_hist[r][2], levels_hist[r][3], tris[r]);
  return 0;
}
