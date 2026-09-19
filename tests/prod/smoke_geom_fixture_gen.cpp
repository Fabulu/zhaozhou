// smoke_geom_fixture_gen.cpp -- the console smoke bench's GEOMETRY FIXTURE and
// every number the bench asserts about it, DERIVED FROM THE REFERENCE.
//
// WHY THIS EXISTS. Until 2026-09-19 the smoke bench pushed sixteen copies of one
// hand-placed screen triangle through a triangle "door" on the core's edge, and
// pinned `raster pixels=1536` -- a number read off a run. The door is gone:
// GEOM.REPLAY feeds GEOM.CLIP from the real meshlet (descriptor in SDRAM ->
// MESHFETCH -> ASSETFETCH -> VDECODE -> POSE palette -> SKIN -> GROUP_SEQ ->
// the shared projector -> the arena -> REPLAY). So the expectation has to come
// from the same law the machine implements, not from the machine:
//
//   zref::render::project_vertex   the projector's own oracle (both views)
//   zref::Clip::clip               GEOM.CLIP's oracle, on the Z60 canvas scissor
//   zref::Setup::setup             GEOM.SETUP's
//   zref::Binner::bin              the binner's -- which tiles a triangle touches
//
// and the pixel count is (union of tiles over the frame) x 16 x 16, because
// `render_pixels_o` is `zhao_raster_fbwrite`'s pixels WRITTEN and the pipeline
// resolves WHOLE tiles (tests/shell/shell_draw_directed.cpp established that,
// oracle 3328 = counter 3328 = memory 3328).
//
// THE FIXTURE ITSELF LIVES HERE, ONCE. This program writes
// tests/prod/smoke_geom_fixture.svh, which the bench `include`s; the ctest
// `smoke_geom_fixture_fresh` re-runs it with --check and fails if the committed
// header differs. So the vertex list, the camera and the expectations cannot
// drift apart: they are one file's output.
//
// THE SKINNED POSITION IS THE MODEL POSITION, and that is a ruling rather than a
// shortcut: no clip page reaches GEOM.POSE in this bench (core entry I29), so
// every bone is unset and `zhao_geom_pose_palette` substitutes the IDENTITY bind
// pose ("safe no-op palette + error counter, never a wild read"); every record
// is rigid (w0 = 64, bone0 == bone1). The transform is the identity.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "zref/zref_creature.hpp"
#include "zref/zref_geom.hpp"
#include "zrender/internal.hpp"

namespace zr = zref::render;

namespace {

constexpr int32_t kOne = 65536;  // fx16 1.0

// ---- THE FIXTURE ------------------------------------------------------------
// Eight vertices, fx16 world = model. z is the depth the camera divides by.
struct V { int32_t x, y, z; };
const V kVerts[] = {
    {-kOne / 2, -kOne / 2, kOne},              // 0
    { kOne / 2, -kOne / 2, kOne},              // 1
    { 0,         kOne / 2, kOne + kOne / 2},   // 2
    {-4 * kOne / 5, 3 * kOne / 5, 2 * kOne},   // 3
    { 4 * kOne / 5, 7 * kOne / 10, 2 * kOne},  // 4
    { 3 * kOne / 10, -4 * kOne / 5, 3 * kOne}, // 5
    { 0,          0,        -kOne},            // 6  BEHIND the eye (w = z < 0)
    { 9 * kOne / 10, 9 * kOne / 10, kOne + kOne / 4},  // 7
};
constexpr int kNV = sizeof(kVerts) / sizeof(kVerts[0]);

// Eight triangles, u8 local indices. Triangle 5 names the behind-the-eye vertex,
// so GEOM.CLIP rejects it WHOLE in both views (the near plane is a whole-
// primitive rejection here -- zhao_geom_clip.sv law 1) and counts it `clipped`.
const uint8_t kTris[][3] = {
    {0, 1, 2}, {0, 2, 3}, {1, 4, 2}, {3, 2, 4},
    {5, 1, 0}, {0, 1, 6}, {7, 4, 1}, {3, 4, 7},
};
constexpr int kNT = sizeof(kTris) / sizeof(kTris[0]);

// The camera, BOTH views, row-major fx16: x and y pass through, w = z. A real
// perspective divide, so depth varies across every triangle and the near plane
// is reachable. Row 2 is inert in the projector (its header).
const int32_t kMat[16] = {
    kOne, 0,    0,    0,
    0,    kOne, 0,    0,
    0,    0,    kOne, 0,
    0,    0,    kOne, 0,
};

// The two views side by side inside the shell's 4x4-tile (64x64 px) render
// grid: view 0 on the left half, view 1 on the right. Each view's projected
// triangles therefore land in DIFFERENT tiles, so both views are visible in the
// pixel count rather than hidden under each other.
struct VP { uint32_t x0, y0, w, h; };
const VP kVp[2] = {{0, 0, 32, 64}, {32, 0, 32, 64}};

// ---- THE LIGHT (GEOM.LIGHT = zhao_light_stream, owner ruling R2) -----------
// One light, gain 1.0 on all three channels, no emission, zero environment --
// light_stream_directed.cpp's own convention, under which each output channel
// IS the light response and is compared against the SHIPPED law. The direction
// is (0.6, 0.8, 0), so the response to the fixture's normal is a real fraction.
constexpr int32_t kLightX = 39322, kLightY = 52429, kLightZ = 0;
constexpr uint32_t kGain = 0x10000;
// GEOM.SKIN.NORM's world normal for every fixture record: the packed normal is
// (127, 0, 0) and w0 = 64 through the IDENTITY bind pose (no pose decoded, I29),
// so n = (64 * 65536 * 127, 0, 0) and |n| = n.x (a perfect square). The smoke
// bench asserts this same value on its SKIN.NORM tap; it is the one input here
// that is hand-derived rather than called, and the bench checks it.
constexpr int64_t kSnNx = int64_t(64) * 65536 * 127;

// The GEOM.CLIP scissor: the Z60 canvas the core derives from `mode_act_o`
// (POST_W_Z60_C x POST_H_FULL_C in zhao_console_core.sv).
constexpr uint32_t kCanvasW = 384, kCanvasH = 240;
// The shell's render grid, in tiles (`render_grid_w_i`/`render_grid_h_i`).
constexpr int kGridTiles = 4;

struct Result {
  int replayed = 0;   // view-triangles GEOM.REPLAY emits
  int clipped = 0;    // rejected by GEOM.CLIP for WHERE they are
  int culled = 0;     // rejected for WHAT they are (zero area, backface)
  int accepted = 0;   // reach GEOM.SETUP
  std::set<std::pair<int, int>> tiles;
  bool outside_grid = false;
};

Result derive() {
  Result r;
  zref::mat4fx m{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) m.m[i][j] = zref::fx16{kMat[i * 4 + j]};
  zref::Clip::Viewport cvp;
  cvp.w = kCanvasW;
  cvp.h = kCanvasH;
  for (int view = 0; view < 2; ++view) {
    zr::Viewport vp;
    vp.x0 = kVp[view].x0;
    vp.y0 = kVp[view].y0;
    vp.w = kVp[view].w;
    vp.h = kVp[view].h;
    zr::ProjOut p[kNV];
    for (int i = 0; i < kNV; ++i)
      p[i] = zr::project_vertex(m, vp, zref::fx16{kVerts[i].x}, zref::fx16{kVerts[i].y},
                                zref::fx16{kVerts[i].z}, nullptr);
    for (int t = 0; t < kNT; ++t) {
      ++r.replayed;
      const zr::ProjOut& a = p[kTris[t][0]];
      const zr::ProjOut& b = p[kTris[t][1]];
      const zr::ProjOut& c = p[kTris[t][2]];
      zref::Clip::In in;
      in.ax = a.s.x; in.ay = a.s.y;
      in.bx = b.s.x; in.by = b.s.y;
      in.cx = c.s.x; in.cy = c.s.y;
      in.behind = static_cast<uint8_t>((a.in ? 0 : 1) | (b.in ? 0 : 2) | (c.in ? 0 : 4));
      const zref::Clip::Out o = zref::Clip::clip(in, cvp, zref::Clip::kCullNone);
      if (o.verdict != zref::Clip::kAccept) {
        // The oracle's own split: WHERE (near plane, empty box) vs WHAT.
        if (o.verdict == zref::Clip::kNearPlane || o.verdict == zref::Clip::kOffscreen) ++r.clipped;
        else ++r.culled;
        continue;
      }
      ++r.accepted;
      const zref::Setup::Out s = zref::Setup::setup(o.ax, o.ay, o.bx, o.by, o.cx, o.cy, o.area2);
      for (const auto& ref : zref::Binner::bin(s, o.min_x, o.max_x, o.min_y, o.max_y)) {
        if (ref.tx < 0 || ref.ty < 0 || ref.tx >= kGridTiles || ref.ty >= kGridTiles)
          r.outside_grid = true;
        r.tiles.insert({ref.tx, ref.ty});
      }
    }
  }
  return r;
}

std::string hex32(int32_t v) {
  char b[32];
  std::snprintf(b, sizeof b, "32'sh%08X", static_cast<uint32_t>(v));
  return b;
}

std::string emit(const Result& r) {
  std::string s;
  char b[256];
  s += "// GENERATED by tests/prod/smoke_geom_fixture_gen.cpp -- DO NOT EDIT.\n";
  s += "// Regenerate: build `smoke_geom_fixture_gen` and run it with the path of\n";
  s += "// this file. The ctest `smoke_geom_fixture_fresh` fails if it is stale.\n";
  s += "//\n";
  s += "// Every SGF_EXP_* below is DERIVED FROM THE REFERENCE, not read off a run:\n";
  s += "// zref::render::project_vertex -> zref::Clip -> zref::Setup ->\n";
  s += "// zref::Binner, union of tiles x 256 (whole-tile resolve).\n";
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_N_VERTS = %d;\n", kNV); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_N_TRIS  = %d;\n", kNT); s += b;
  auto arr = [&](const char* name, int which) {
    s += "localparam logic signed [31:0] ";
    s += name;
    std::snprintf(b, sizeof b, " [0:%d] = '{", kNV - 1); s += b;
    for (int i = 0; i < kNV; ++i) {
      const int32_t v = which == 0 ? kVerts[i].x : which == 1 ? kVerts[i].y : kVerts[i].z;
      s += hex32(v);
      s += (i + 1 < kNV) ? ", " : "};\n";
    }
  };
  arr("SGF_VX", 0);
  arr("SGF_VY", 1);
  arr("SGF_VZ", 2);
  std::snprintf(b, sizeof b, "localparam logic [7:0] SGF_IX [0:%d] = '{", 3 * kNT - 1); s += b;
  for (int t = 0; t < kNT; ++t)
    for (int k = 0; k < 3; ++k) {
      std::snprintf(b, sizeof b, "8'd%u", kTris[t][k]);
      s += b;
      s += (t * 3 + k + 1 < 3 * kNT) ? ", " : "};\n";
    }
  s += "localparam logic signed [31:0] SGF_MAT [0:15] = '{";
  for (int i = 0; i < 16; ++i) {
    s += hex32(kMat[i]);
    s += (i < 15) ? ", " : "};\n";
  }
  for (int v = 0; v < 2; ++v) {
    // cfg addr 16 = {y0[27:16], x0[11:0]}, addr 17 = {h[27:16], w[11:0]}
    std::snprintf(b, sizeof b,
                  "localparam logic [31:0] SGF_VP%d_ORG = 32'h%08X;  // x0=%u y0=%u\n", v,
                  (kVp[v].y0 << 16) | kVp[v].x0, kVp[v].x0, kVp[v].y0);
    s += b;
    std::snprintf(b, sizeof b,
                  "localparam logic [31:0] SGF_VP%d_EXT = 32'h%08X;  // w=%u h=%u\n", v,
                  (kVp[v].h << 16) | kVp[v].w, kVp[v].w, kVp[v].h);
    s += b;
  }
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_REPLAYED = %d;  // view-triangles out of GEOM.REPLAY\n", r.replayed); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_CLIPPED  = %d;  // GEOM.CLIP `clipped` (near plane / empty box)\n", r.clipped); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_CULLED   = %d;  // GEOM.CLIP `culled` (zero area / backface)\n", r.culled); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_ACCEPTED = %d;  // into GEOM.SETUP\n", r.accepted); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_TILES    = %zu;  // union over both views\n", r.tiles.size()); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_PIXELS   = %zu;  // tiles x 16 x 16\n", r.tiles.size() * 256); s += b;
  {
    const int64_t n[3] = {kSnNx, 0, 0};
    const int32_t ndl = zref::creature::lambert_from_world_normal(n, kSnNx, kLightX, kLightY, kLightZ);
    // rhu16(gain * ndl), then + ambient + spill (both zero), saturated at 1.0.
    uint64_t lit = (uint64_t(kGain) * uint64_t(uint32_t(ndl)) + 32768u) >> 16;
    if (lit > 65536u) lit = 65536u;
    std::snprintf(b, sizeof b, "localparam logic signed [31:0] SGF_LIGHT_X = %s, SGF_LIGHT_Y = %s, SGF_LIGHT_Z = %s;\n",
                  hex32(kLightX).c_str(), hex32(kLightY).c_str(), hex32(kLightZ).c_str());
    s += b;
    std::snprintf(b, sizeof b, "localparam logic [19:0] SGF_LIGHT_GAIN = 20'h%05X;\n", kGain); s += b;
    std::snprintf(b, sizeof b, "localparam logic [16:0] SGF_EXP_LIT = 17'd%llu;  // zref::creature::lambert_from_world_normal = %d\n",
                  static_cast<unsigned long long>(lit), ndl);
    s += b;
  }
  s += "// Tiles, (tx,ty):";
  for (const auto& t : r.tiles) {
    std::snprintf(b, sizeof b, " (%d,%d)", t.first, t.second);
    s += b;
  }
  s += "\n";
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  const Result r = derive();
  if (r.outside_grid) {
    std::printf("smoke_geom_fixture_gen: a tile falls OUTSIDE the %dx%d render grid -- move the fixture\n",
                kGridTiles, kGridTiles);
    return 1;
  }
  if (r.clipped == 0 || r.accepted < 8 || r.tiles.size() < 4) {
    std::printf("smoke_geom_fixture_gen: the fixture no longer exercises a clipped triangle, "
                "both views and several tiles (clipped=%d accepted=%d tiles=%zu)\n",
                r.clipped, r.accepted, r.tiles.size());
    return 1;
  }
  const std::string text = emit(r);
  const bool check = (argc >= 3) && (std::strcmp(argv[1], "--check") == 0);
  const char* path = check ? argv[2] : (argc >= 2 ? argv[1] : nullptr);
  if (!path) {
    std::fputs(text.c_str(), stdout);
    return 0;
  }
  if (check) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
      std::printf("smoke_geom_fixture_gen: cannot read %s\n", path);
      return 1;
    }
    std::string have;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) have.append(buf, n);
    std::fclose(f);
    std::string norm;
    for (char c : have)
      if (c != '\r') norm += c;
    if (norm != text) {
      std::printf("smoke_geom_fixture_gen: %s is STALE against the reference -- regenerate it\n", path);
      return 1;
    }
    std::printf("smoke_geom_fixture_gen: fresh (replayed=%d clipped=%d accepted=%d tiles=%zu pixels=%zu)\n",
                r.replayed, r.clipped, r.accepted, r.tiles.size(), r.tiles.size() * 256);
    return 0;
  }
  FILE* f = std::fopen(path, "wb");
  if (!f) {
    std::printf("smoke_geom_fixture_gen: cannot write %s\n", path);
    return 1;
  }
  std::fwrite(text.data(), 1, text.size(), f);
  std::fclose(f);
  std::printf("smoke_geom_fixture_gen: wrote %s (replayed=%d clipped=%d accepted=%d tiles=%zu pixels=%zu)\n",
              path, r.replayed, r.clipped, r.accepted, r.tiles.size(), r.tiles.size() * 256);
  return 0;
}
