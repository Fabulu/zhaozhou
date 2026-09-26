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
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "zref/zref_creature.hpp"
#include "zref/zref_geom.hpp"
#include "zref/zref_light_env.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_tess.hpp"
#include "zrender/internal.hpp"

namespace zr = zref::render;
namespace zt = zref::terrain;

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

// Per-vertex texture coordinates, the record's s16 fx16 UV fields (GEOM.VDECODE
// layout). DIFFERENT AT EVERY VERTEX (2026-09-19, entry I46) so the u/v_over_w
// GEOM.VATTR writes -- zref::geom_over_w(uv, invw24) -- carry real gradients
// into GEOM.ATTRPACK's planes. They do not move a single covered pixel: the
// pixel gate is coverage, and coverage is geometry.
const int16_t kU[] = {0x0800, -0x1000, 0x2400, 0x0100, -0x3000, 0x1800, 0x0000, 0x7000};
const int16_t kV[] = {-0x0400, 0x0C00, 0x2000, -0x2800, 0x3400, 0x0600, 0x0000, -0x7000};
static_assert(sizeof(kU) / sizeof(kU[0]) == kNV && sizeof(kV) / sizeof(kV[0]) == kNV,
              "one UV pair per vertex");

// Eight triangles, u8 local indices. Triangle 5 names the behind-the-eye vertex,
// so GEOM.CLIP rejects it WHOLE in both views (the near plane is a whole-
// primitive rejection here -- zhao_geom_clip.sv law 1) and counts it `clipped`.
const uint8_t kTris[][3] = {
    {0, 1, 2}, {0, 2, 3}, {1, 4, 2}, {3, 2, 4},
    {5, 1, 0}, {0, 1, 6}, {7, 4, 1}, {3, 4, 7},
};
constexpr int kNT = sizeof(kTris) / sizeof(kTris[0]);

// ---- THE MESHLET PARTITION (owner ruling R57, 2026-09-20) -------------------
// The eight triangles are split across THREE meshlets, in order, each declaring
// the same eight vertices and its own index run. The console's meshlet loop
// could not be measured with one meshlet in flight -- there was nothing for it
// to overlap WITH -- and a gate that cannot reach the state is not evidence
// about the state.
//
// THE PIXEL COUNT DOES NOT MOVE AND THAT IS THE POINT. Partitioning changes
// which meshlet carries a triangle, not which triangles exist: the same eight
// are projected in the same two views through the same camera, so
// `replayed`, `clipped`, `culled`, `accepted`, the tile union and therefore
// SGF_EXP_PIXELS are identical to the one-meshlet fixture. `derive()` walks the
// partition rather than the flat list and `main` asserts the two agree, so the
// claim is checked rather than argued.
//
// THREE and not two, because two releases give one interval and three give a
// steady one -- the same reason the rate line divides by (moves - 1).
struct Meshlet { int first, n; };
constexpr Meshlet kMeshlets[] = {{0, 3}, {3, 3}, {6, 2}};
constexpr int kNM = sizeof(kMeshlets) / sizeof(kMeshlets[0]);
static_assert(kMeshlets[0].first == 0, "the partition starts at triangle 0");
static_assert(kMeshlets[0].n + kMeshlets[1].n + kMeshlets[2].n == kNT,
              "the partition covers every triangle exactly once");
static_assert(kMeshlets[1].first == kMeshlets[0].n &&
              kMeshlets[2].first == kMeshlets[0].n + kMeshlets[1].n,
              "the partition is contiguous and in order");

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

// ---- THE LIGHT: SetEnvironment 0x0311 -> GEOM.LIGHT's bank (owner ruling R25)
// The smoke packet carries this record; CMD.EXEC lowers it through
// GEOM.LIGHT.ENV into zhao_light_stream's bank. The expected lit colour is
// derived from THE RECORD through the bridge (`zref::light_env::bank_of`) and
// the light law, per channel. Yaw a quarter turn puts the sun in the x/y plane
// and pitch 0x1000 (22.5 degrees) gives the fixture's +x normal a real
// fraction; the three channels' colours differ so a channel swap shows, and
// sun + ambient stays under 1.0 so nothing saturates. The power-on default
// (sun at the zenith) lights a +x normal with ambient ONLY, so a vertex lit
// before the record lands fails every channel.
constexpr uint16_t kEnvYaw = 0x4000, kEnvPitch = 0x1000;
constexpr uint16_t kEnvSun = (31u << 11) | (40u << 5) | 12u;  // 0xFD0C
constexpr uint16_t kEnvAmb = (2u << 11) | (6u << 5) | 4u;     // 0x10C4
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

// ---- THE TERRAIN FIXTURE (2026-09-26, TERRAINVISIBLE) ----------------------
//
// WHY THE TERRAIN PATCH IS IN THIS FILE AT ALL. Until this commit the fixture
// above was the WHOLE of the pixel gate: `SGF_EXP_PIXELS = 2560` was the union
// of the tiles the MESH touches, from a generator that models no terrain. That
// was correct while terrain drew nothing, and terrain drew nothing for a LAWFUL
// reason PROJCOLLAPSE measured on 2026-09-26 -- the played pages' body is all
// zeros, so layer A is a constant, every lattice vertex sits at world y = 0,
// this camera has the eye at world y = 0, and A PLANE THROUGH THE EYE PROJECTS
// TO A LINE. Every terrain triangle's three corners carried screen y = 8192
// (32.0 px, exactly `y0 + h/2`), the cross product was arithmetically zero, and
// GEOM.CLIP culled all 256 correctly.
//
// `reports/DECISION-20260926-TERRAIN-FIXTURE.md` decides that the fixture is
// changed so terrain is actually DRAWN, and that `raster pixels` moves with it.
// The oracle has to move in the same commit or the new number is drift rather
// than a measurement, so the terrain patch is modelled HERE, through the same
// four oracles the mesh uses plus `zref::terrain::tessellate`.
//
// TWO KNOBS ARE TURNED AND BOTH ARE REQUIRED. Relief alone is not enough:
// PROJCOLLAPSE also measured that with a height field the 256 stop being
// zero-area and become OFFSCREEN, because at the OLD patch coordinates
// (ix = 3, iz = 7) the whole 32 m patch is about 2.3 px wide and a 1 m cell is
// 0.072 px, so no pixel centre lies inside any triangle.
//
//   1. RELIEF, in layer A. An AFFINE ramp, h(vi,vj) = BASE + TILTX*vi +
//      TILTZ*vj, and affine is CHOSEN rather than convenient. Every coarser LOD
//      level reproduces an affine field EXACTLY -- `zref::terrain::
//      coarse_height` is `ha + rescale(hb-ha, 1)`, which is the midpoint, and
//      the midpoint of an affine field is its value there -- so
//      `lod_deviation` is zero at every level, exactly as it is for today's
//      flat field, and `morph_height` is the identity because `hc == h`.
//      TERRAIN.LOD therefore sees the same deviations it sees now and the
//      tessellation this file models cannot be changed by the relief. A
//      curved or noisy field would move the deviations, hence the level, hence
//      the triangle count -- and this generator would then have to model
//      TERRAIN.LOD's selector as well, which is a second implementation of a
//      law that already has one.
//   2. PLACEMENT, via the patch COORDINATES. `ix` and `iz` are the only
//      placement the header permits: terrain_rules 2.1 requires the envelope
//      to equal `origin + coords x 32 x pitch` exactly, so the patch's angular
//      size is `1/iz` of the view's half-width at its near edge and is
//      INDEPENDENT of pitch -- scaling the world scales z with it. Moving the
//      first record from (3, 7) to (-1, 1) is therefore the whole lever, and
//      it is a change to the BENCH's stimulus, not to `kMat` or `kVp`: the
//      camera and the viewports are untouched, so the mesh's own 14 triangles
//      and 10 tiles are exactly what they were.
//
// WHAT IS NOT DONE, and it is the fence this fixture exists to respect: no
// epsilon, clamp or bias anywhere near the zero-area test. The area was
// ARITHMETICALLY zero from a CORRECT projection, and a tolerance there would
// admit a degenerate triangle and draw a wrong pixel.
constexpr int kTerrRecords = 3;      // N_TERR_REC in the bench
constexpr int kTerrIx0 = 0;          // record r carries patch_ix = r + this
constexpr int kTerrIz0 = 1;          // record r carries patch_iz = r + this
constexpr int kTerrPitchLog2 = 0;    // 1 m cells, unchanged
constexpr int kTerrLatticeN = 33;    // 33x33 vertices, terrain_rules 7 layer A
// Layer A, in height16 RAW (S 1.7.8 metres, terrain_rules 2: 256 raw = 1 m).
// BASE lifts the plane off the eye -- that is the whole of the zero-area
// repair, and it is equivalent to putting the eye off the ground plane without
// touching the camera. The two TILTS put a real gradient in BOTH lattice axes,
// so the face normal `zhao_terrain_normals` computes is not the +Y axis and the
// patch has vertical extent on screen (it spans 12.0..25.6 px of a 64 px view).
// Every one is a power of two in raw units, so every halving `coarse_height`
// performs is exact.
constexpr int32_t kTerrBaseH16 = -5120;  // -20.0 m: the ground, below the eye
constexpr int32_t kTerrTiltXH16 = 256;   // +1.0 m per lattice step in x
constexpr int32_t kTerrTiltZH16 = 128;   // +0.5 m per lattice step in z

// THE SUBPATCH JOBS, MEASURED AND NOT INFERRED (`SMOKE: terrjob`).
// The console presents TWO jobs in this bench and they are the SAME subpatch:
//
//   [0] mode=1 ox=0 oz=0 level=0 nlvl=[0 0 0 0] morph=0 surface=0 dual=0 src=1000
//   [1] mode=2 ...                                                        src=1000
//   [2] mode=1 ox=0 oz=0 level=0 nlvl=[0 0 0 0] morph=0 surface=0 dual=0 src=22136
//   [3] mode=2 ...                                                        src=22136
//
// src 1000 is `zhao_terrain_jobissue`'s, off the T5 record (source_id 1000 + r);
// src 22136 = 0x5678 was the BENCH's own override at `terr_job_src_id_i`, which
// had driven client B since 2026-09-21 and was never retired. So the fixture
// drew subpatch (0,0) TWICE -- 256 triangles of carriage for ONE subpatch of
// coverage -- and `terr_cf_emitted_o = 256` was 2 x 128 rather than two
// different subpatches. The obvious reading of `tess_refs=256` is two
// subpatches and it is WRONG; the probe was written because this file was
// about to depend on the answer, and it changed the answer.
//
// THE DUPLICATE IS RETIRED, and the reason is a measured wall rather than
// tidiness. GEOM.BINNER's triangle store is `TRI_CAP = 128` PER FRAME
// (`zhao_geom_bin_pipe_v2.sv:18`), and overflowing it is a whole-frame fault:
// `overflow_o` latches, every later triangle is dropped WHOLE and
// `render_overflow_o` goes high. With both jobs the repaired fixture put 226
// triangles into GEOM.SETUP -- measured, with `binrefs overflow=1`,
// `max_tile_list_depth=99` and `raster pixels` STUCK at 2560 because the
// terrain tile was walled off. With the live producer's job alone it is 75.
// `kBinnerTriCap` below keeps that from coming back silently.
//
// Only ONE patch composes: `terrcompose place_patches=1 cc_filled=1` and
// `resident=1/3`, so records 1 and 2 are staged, loaded and never tessellated.
// Their coordinates still have to be legal and DISTINCT (the directory keys on
// {epoch, island, ix, iz}), which is why the generator emits all three.
//
// EACH OF THESE IS ASSERTED BY THE BENCH against the machine, so a drift in the
// level, the count or the number of jobs is loud rather than silent.
struct TerrJob {
  int ox, oz, level;
};
const TerrJob kTerrJobs[] = {{0, 0, 0}};
constexpr int kNTerrJobs = sizeof(kTerrJobs) / sizeof(kTerrJobs[0]);
// The T5 record's `view_mask` is 0x01 -- view 0 only (video_rules 3.1: view 0
// is P1). The mesh is replayed into BOTH views; terrain into one.
constexpr int kTerrViewMask = 0x1;

// GEOM.BINNER's per-frame triangle store, `zhao_geom_bin_pipe_v2.sv:18`. It is
// declared HERE because the fixture is the thing that can exceed it: the binner
// walls off the tail of a frame that offers more, `render_overflow_o` latches,
// and the tiles the walled-off triangles would have entered are simply never
// resolved -- so `render_pixels_o` comes back LOW and the fixture looks like a
// terrain arm that stopped drawing rather than like a capacity limit. That is
// the flattering-direction failure CLAUDE.md's broken-instrument chapter is
// about, and it cost this packet one ten-minute run to find.
constexpr int kBinnerTriCap = 128;


// ---- WHICH TILES THE RASTER ACTUALLY RESOLVES ------------------------------
//
// A CORRECTION TO THIS FILE'S OWN MODEL, 2026-09-26 (TERRAINVISIBLE), and it
// was invisible until terrain drew. `SGF_EXP_PIXELS` was "the union of the
// tiles `zref::Binner` names, times 256". `zref::Binner::bin` is GEOM.BINNER's
// oracle and it is CONSERVATIVE BY DESIGN -- its own header says a tile is
// emitted "if the three edge functions can still be satisfied somewhere in
// it", an affine corner test, not a coverage test. GEOM.CLIP is conservative
// in the same direction: its box test puts a pixel CENTRE inside the
// triangle's BOUNDING BOX, never inside the triangle.
//
// For the mesh the two agree, because its fourteen triangles are fat: every
// tile the binner names also receives a covered fragment. So the formula was
// right for eight months by coincidence of the fixture.
//
// TERRAIN BROKE THE COINCIDENCE AND THE MACHINE SAID SO. Measured: GEOM.BINNER
// pushed 101 references over ELEVEN tiles (`binrefs tile_references=101
// max_tile_list_depth=59 overflow=0` -- both exactly what this file derived),
// GEOM.SETUP took all 75 triangles, and `raster pixels` came back 2560 = TEN
// tiles. A tile that receives a job and no covered fragment is never dirtied
// and never written, so it costs no pixels.
//
// So the pixel gate's tile set is the set of tiles that hold at least one
// COVERED PIXEL CENTRE, and the predicate is the ratified one: `zref::
// fill_accept` on the S 8 top-left form, which that function's own comment
// calls "the C++ transcription of `zhao_raster_fill.sv` -- the module the
// formal lane proves equal to `E0 + bias >= 0`". Same bytes as the binner's
// reject rule and the rasterizer's accept rule, so they cannot disagree.
//
// THE CHECK THAT SAYS THIS IS A MODEL AND NOT A GUESS: run over the mesh alone
// it names the same ten tiles the binned formula did, which is the number this
// bench has measured on the machine since 2026-09-19.
void covered_tiles(const zref::Setup::Out& s, int32_t min_x, int32_t max_x, int32_t min_y,
                   int32_t max_y, std::set<std::pair<int, int>>* out, bool* outside) {
  int64_t base[3];
  bool rnz[3], tl[3];
  for (int i = 0; i < 3; ++i) {
    base[i] = zref::Binner::ep_base(s.e[i]);
    rnz[i] = zref::Binner::rnz(s.e[i]);
    tl[i] = s.e[i].tl;
  }
  for (int32_t py = min_y; py <= max_y; ++py)
    for (int32_t px = min_x; px <= max_x; ++px) {
      bool in = true;
      for (int i = 0; i < 3 && in; ++i) {
        const int64_t ep = base[i] + static_cast<int64_t>(s.e[i].kx) * px +
                           static_cast<int64_t>(s.e[i].ky) * py;
        in = zref::fill_accept(ep, rnz[i], tl[i]);
      }
      if (!in) continue;
      const int tx = px >> zref::Binner::kTileLog2;
      const int ty = py >> zref::Binner::kTileLog2;
      if (tx < 0 || ty < 0 || tx >= kGridTiles || ty >= kGridTiles) *outside = true;
      out->insert({tx, ty});
    }
}

struct Result {
  int replayed = 0;   // view-triangles GEOM.REPLAY emits
  int clipped = 0;    // rejected by GEOM.CLIP for WHERE they are
  int culled = 0;     // rejected for WHAT they are (zero area, backface)
  int accepted = 0;   // reach GEOM.SETUP
  std::set<std::pair<int, int>> tiles;      // the MESH's BINNED tiles
  std::set<std::pair<int, int>> cov_tiles;  // the MESH's COVERED tiles
  bool outside_grid = false;
  // The terrain arm, counted apart so the two populations never blur. The
  // pixel gate is the UNION: `render_pixels_o` counts pixels WRITTEN and the
  // pipeline resolves WHOLE tiles, so a tile either party touches is 256.
  int terr_submitted = 0;
  int terr_clipped = 0;
  int terr_culled = 0;
  int terr_accepted = 0;
  std::set<std::pair<int, int>> terr_tiles;       // BINNED
  std::set<std::pair<int, int>> terr_cov_tiles;  // COVERED -- the pixel gate
  bool terr_outside_grid = false;

  // GEOM.BINNER's two published counters, catalog ids 18 and 19, DERIVED here
  // instead of pinned from a run. `tile_references` is the frame-wide count of
  // references PUSHED (one per (triangle, tile) pair, both populations, both
  // views); `max_tile_list_depth` is the deepest single tile's list. Both are
  // computable from `zref::Binner::bin`, which is the binner's own oracle, so
  // they stop being numbers somebody read off a run and become numbers the
  // reference names -- the move `SGF_EXP_PIXELS` made in 2026-09-19.
  std::map<std::pair<int, int>, int> ref_depth;
  int tile_refs = 0;
  // The mesh's own share, captured before the terrain walk. It is the number
  // the MESH-ONLY fixture measured on the machine (36 at 2026-09-21), so it is
  // what says this model of the binner agrees with the binner.
  int mesh_tile_refs = 0;

  int max_depth() const {
    int m = 0;
    for (const auto& kv : ref_depth)
      if (kv.second > m) m = kv.second;
    return m;
  }

  // THE PIXEL GATE. `render_pixels_o` counts pixels WRITTEN and the pipeline
  // resolves a WHOLE tile once that tile holds a covered fragment, so this is
  // the union of the COVERED sets -- not of the binned ones.
  std::set<std::pair<int, int>> union_tiles() const {
    std::set<std::pair<int, int>> u = cov_tiles;
    for (const auto& t : terr_cov_tiles) u.insert(t);
    return u;
  }
};

/** The composed lattice of record `rec`, exactly as the bench writes its page. */
zt::ComposedLattice terrain_lattice(int rec) {
  const int64_t pitch = (kTerrPitchLog2 >= 0)
                            ? (static_cast<int64_t>(kOne) << kTerrPitchLog2)
                            : (static_cast<int64_t>(kOne) >> (-kTerrPitchLog2));
  const int64_t ex0 = static_cast<int64_t>(rec + kTerrIx0) * 32 * pitch;
  const int64_t ez0 = static_cast<int64_t>(rec + kTerrIz0) * 32 * pitch;
  zt::ComposedLattice lat;
  lat.w = kTerrLatticeN;
  lat.h = kTerrLatticeN;
  lat.dual = false;  // the page's flags byte is 0: no layer C, no layer D
  lat.wx.resize(kTerrLatticeN);
  lat.wz.resize(kTerrLatticeN);
  lat.top.assign(static_cast<size_t>(kTerrLatticeN) * kTerrLatticeN, 0);
  for (int i = 0; i < kTerrLatticeN; ++i)
    lat.wx[i] = static_cast<int32_t>(ex0 + static_cast<int64_t>(i) * pitch);
  for (int j = 0; j < kTerrLatticeN; ++j)
    lat.wz[j] = static_cast<int32_t>(ez0 + static_cast<int64_t>(j) * pitch);
  for (int vj = 0; vj < kTerrLatticeN; ++vj)
    for (int vi = 0; vi < kTerrLatticeN; ++vi) {
      // layer A is height16; qformats 2/9 make height16 -> fx16 an exact
      // `raw << 8`, and layer B (the scar delta) is all zeros in this bench, so
      // `compose_top` is layer A and `live_top` is `compose_top`.
      const int32_t h16 = kTerrBaseH16 + kTerrTiltXH16 * vi + kTerrTiltZH16 * vj;
      lat.top[static_cast<size_t>(vj) * kTerrLatticeN + vi] = h16 << 8;
    }
  return lat;
}

// `partition` false walks the flat triangle list, true walks the meshlets. The
// two must agree, which is what says the R57 split costs the fixture nothing.
Result derive(bool partition) {
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
    // The order the console actually presents triangles in: meshlet by meshlet,
    // each meshlet's own index run. A partition that lost or duplicated a
    // triangle produces a different list here and therefore different totals.
    std::vector<int> order;
    if (partition) {
      for (int mi = 0; mi < kNM; ++mi)
        for (int k = 0; k < kMeshlets[mi].n; ++k) order.push_back(kMeshlets[mi].first + k);
    } else {
      for (int ti = 0; ti < kNT; ++ti) order.push_back(ti);
    }
    for (const int t : order) {
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
        ++r.tile_refs;
        ++r.ref_depth[{ref.tx, ref.ty}];
      }
      covered_tiles(s, o.min_x, o.max_x, o.min_y, o.max_y, &r.cov_tiles, &r.outside_grid);
    }
  }

  r.mesh_tile_refs = r.tile_refs;

  // ---- THE TERRAIN ARM, through the SAME four oracles -------------------
  // TERRAIN.TESS -> the shared projector -> GEOM.CLIP -> GEOM.SETUP ->
  // the binner. Not one line of this is a terrain-specific law: the only
  // thing terrain brings is its own triangle list, which
  // `zref::terrain::tessellate` produces from the composed lattice on the
  // measured subpatch jobs.
  //
  // THE CULL MODE IS THE SAME `kCullNone` the mesh uses, and it is what
  // `zhao_terrain_clipfeed` declares at the door. Under CULL_NONE
  // `zhao_geom_clip`'s `s3_back` is false BY CONSTRUCTION, so `culled` can
  // only ever mean ZERO AREA here -- which is exactly what makes
  // `SGF_EXP_TERR_CULLED = 0` a statement about the geometry rather than
  // about a mode.
  {
    const zt::ComposedLattice lat = terrain_lattice(0);
    for (int view = 0; view < 2; ++view) {
      if (((kTerrViewMask >> view) & 1) == 0) continue;
      zr::Viewport vp;
      vp.x0 = kVp[view].x0;
      vp.y0 = kVp[view].y0;
      vp.w = kVp[view].w;
      vp.h = kVp[view].h;
      for (int ji = 0; ji < kNTerrJobs; ++ji) {
        zt::SubpatchJob job;
        job.ox = kTerrJobs[ji].ox;
        job.oz = kTerrJobs[ji].oz;
        job.level = kTerrJobs[ji].level;
        for (int s = 0; s < 4; ++s) job.nlevel[s] = kTerrJobs[ji].level;
        job.morph = 0;
        job.surface = zt::Surface::kTop;
        const zt::TessResult tess = zt::tessellate(lat, job, nullptr);
        for (const zt::MeshTri& t : tess.tris) {
          ++r.terr_submitted;
          const zr::ProjOut a = zr::project_vertex(m, vp, zref::fx16{t.ax}, zref::fx16{t.ay},
                                                   zref::fx16{t.az}, nullptr);
          const zr::ProjOut b = zr::project_vertex(m, vp, zref::fx16{t.bx}, zref::fx16{t.by},
                                                   zref::fx16{t.bz}, nullptr);
          const zr::ProjOut c = zr::project_vertex(m, vp, zref::fx16{t.cx}, zref::fx16{t.cy},
                                                   zref::fx16{t.cz}, nullptr);
          zref::Clip::In in;
          in.ax = a.s.x; in.ay = a.s.y;
          in.bx = b.s.x; in.by = b.s.y;
          in.cx = c.s.x; in.cy = c.s.y;
          in.behind = static_cast<uint8_t>((a.in ? 0 : 1) | (b.in ? 0 : 2) | (c.in ? 0 : 4));
          const zref::Clip::Out o = zref::Clip::clip(in, cvp, zref::Clip::kCullNone);
          if (o.verdict != zref::Clip::kAccept) {
            if (o.verdict == zref::Clip::kNearPlane || o.verdict == zref::Clip::kOffscreen)
              ++r.terr_clipped;
            else
              ++r.terr_culled;
            continue;
          }
          ++r.terr_accepted;
          const zref::Setup::Out s2 =
              zref::Setup::setup(o.ax, o.ay, o.bx, o.by, o.cx, o.cy, o.area2);
          for (const auto& ref : zref::Binner::bin(s2, o.min_x, o.max_x, o.min_y, o.max_y)) {
            if (ref.tx < 0 || ref.ty < 0 || ref.tx >= kGridTiles || ref.ty >= kGridTiles)
              r.terr_outside_grid = true;
            r.terr_tiles.insert({ref.tx, ref.ty});
            ++r.tile_refs;
            ++r.ref_depth[{ref.tx, ref.ty}];
          }
          covered_tiles(s2, o.min_x, o.max_x, o.min_y, o.max_y, &r.terr_cov_tiles,
                        &r.terr_outside_grid);
        }
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
  // R57: the meshlet partition. Each meshlet declares ALL the vertices and its
  // own contiguous run of triangles, so the page carries one vertex run and
  // SGF_N_MESHLETS index runs.
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_N_MESHLETS = %d;\n", kNM); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_MESH_FIRST [0:%d] = '{", kNM - 1); s += b;
  for (int mi = 0; mi < kNM; ++mi) {
    std::snprintf(b, sizeof b, "%d", kMeshlets[mi].first); s += b;
    s += (mi + 1 < kNM) ? ", " : "};\n";
  }
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_MESH_NTRI  [0:%d] = '{", kNM - 1); s += b;
  for (int mi = 0; mi < kNM; ++mi) {
    std::snprintf(b, sizeof b, "%d", kMeshlets[mi].n); s += b;
    s += (mi + 1 < kNM) ? ", " : "};\n";
  }
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
  auto arr16 = [&](const char* name, const int16_t* v) {
    s += "localparam logic signed [15:0] ";
    s += name;
    std::snprintf(b, sizeof b, " [0:%d] = '{", kNV - 1); s += b;
    for (int i = 0; i < kNV; ++i) {
      std::snprintf(b, sizeof b, "16'sh%04X", static_cast<uint16_t>(v[i]));
      s += b;
      s += (i + 1 < kNV) ? ", " : "};\n";
    }
  };
  arr16("SGF_VU", kU);
  arr16("SGF_VV", kV);
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
  // ---- THE TERRAIN FIXTURE, so the bench's PAGE and this oracle cannot
  // drift apart. The bench writes layer A and the T5 record's coordinates from
  // these, exactly as it writes the mesh from SGF_VX/VY/VZ.
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_TERR_RECORDS = %d;\n", kTerrRecords); s += b;
  std::snprintf(b, sizeof b, "localparam int SGF_TERR_IX0 = %d;  // record r: patch_ix = r + this\n", kTerrIx0); s += b;
  std::snprintf(b, sizeof b, "localparam int SGF_TERR_IZ0 = %d;  // record r: patch_iz = r + this\n", kTerrIz0); s += b;
  std::snprintf(b, sizeof b, "localparam int SGF_TERR_PITCH_LOG2 = %d;  // pitch = 2^this metres\n", kTerrPitchLog2); s += b;
  std::snprintf(b, sizeof b,
                "localparam logic signed [15:0] SGF_TERR_BASE_H16 = -16'sd%d, SGF_TERR_TILTX_H16 = 16'sd%d, SGF_TERR_TILTZ_H16 = 16'sd%d;  // layer A = BASE + TILTX*vi + TILTZ*vj, height16 raw (256 = 1 m)\n",
                -kTerrBaseH16, kTerrTiltXH16, kTerrTiltZH16); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_TERR_JOBS = %d;  // subpatch jobs, MEASURED (`SMOKE: terrjob`)\n", kNTerrJobs); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_TERR_LEVEL = %d;  // every job's LOD level\n", kTerrJobs[0].level); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_TERR_TRIS     = %d;  // TERRAIN.CLIPFEED emits (%d job x 128)\n", r.terr_submitted, kNTerrJobs); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_TERR_CLIPPED  = %d;  // sub-pixel: no pixel centre inside\n", r.terr_clipped); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_TERR_CULLED   = %d;  // ZERO AREA -- must stay 0\n", r.terr_culled); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_TERR_ACCEPTED = %d;  // into GEOM.SETUP\n", r.terr_accepted); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_TERR_TILES    = %zu;  // tiles TERRAIN COVERS (binned: %zu)\n", r.terr_cov_tiles.size(), r.terr_tiles.size()); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_MESH_TILES    = %zu;  // tiles the MESH COVERS (binned: %zu)\n", r.cov_tiles.size(), r.tiles.size()); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_TILES    = %zu;  // UNION of the COVERED sets -- the tiles the raster resolves\n", r.union_tiles().size()); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_PIXELS   = %zu;  // tiles x 16 x 16 (was 2560, mesh only, before terrain drew)\n", r.union_tiles().size() * 256); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_TILE_REFS  = %d;  // GEOM.BINNER catalog id 18, references PUSHED\n", r.tile_refs); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_EXP_TILE_DEPTH = %d;  // GEOM.BINNER catalog id 19, the deepest tile list\n", r.max_depth()); s += b;
  std::snprintf(b, sizeof b, "localparam int unsigned SGF_BINNER_TRI_CAP = %d;  // zhao_geom_bin_pipe_v2 TRI_CAP, triangles per frame\n", kBinnerTriCap); s += b;
  {
    zref::sky::EnvState env;
    env.sun_yaw = zref::angle16{kEnvYaw};
    env.sun_pitch = zref::angle16{kEnvPitch};
    env.sun_colour.bits = kEnvSun;
    env.ambient.bits = kEnvAmb;
    const zref::light_env::Bank bank = zref::light_env::bank_of(env);
    const int64_t n[3] = {kSnNx, 0, 0};
    const int32_t ndl = zref::creature::lambert_from_world_normal(
        n, kSnNx, bank.light0_a[0], bank.light0_a[1], bank.light0_a[2]);
    // Per channel: rhu16(gain_c * ndl) + ambient_c + spill_c (spill 0),
    // saturated at 1.0 -- zhao_light_stream's law on the bank the record made.
    unsigned __int128 rec = 0;
    for (int w = 0; w < 4; ++w) rec |= static_cast<unsigned __int128>(bank.light0_b[w]) << (32 * w);
    uint64_t lit[3];
    for (int c = 0; c < 3; ++c) {
      const uint64_t g20 = static_cast<uint64_t>(rec >> (20 * c)) & 0xFFFFFu;  // gain r, g, b
      lit[c] = ((g20 * uint64_t(uint32_t(ndl)) + 32768u) >> 16) + bank.env[c] + bank.env[3 + c];
      if (lit[c] > 65536u) lit[c] = 65536u;
    }
    std::snprintf(b, sizeof b,
                  "localparam logic [15:0] SGF_ENV_YAW = 16'h%04X, SGF_ENV_PITCH = 16'h%04X, "
                  "SGF_ENV_SUN = 16'h%04X, SGF_ENV_AMB = 16'h%04X;  // the SetEnvironment record\n",
                  kEnvYaw, kEnvPitch, kEnvSun, kEnvAmb);
    s += b;
    std::snprintf(b, sizeof b,
                  "localparam logic [16:0] SGF_EXP_LIT_R = 17'd%llu, SGF_EXP_LIT_G = 17'd%llu, "
                  "SGF_EXP_LIT_B = 17'd%llu;  // bank_of(record) -> L=(%d,%d,%d), ndl %d\n",
                  static_cast<unsigned long long>(lit[0]), static_cast<unsigned long long>(lit[1]),
                  static_cast<unsigned long long>(lit[2]), bank.light0_a[0], bank.light0_a[1],
                  bank.light0_a[2], ndl);
    s += b;
  }
  s += "// Mesh tiles COVERED, (tx,ty):";
  for (const auto& t : r.cov_tiles) {
    std::snprintf(b, sizeof b, " (%d,%d)", t.first, t.second);
    s += b;
  }
  s += "\n// Terrain tiles COVERED, (tx,ty):";
  for (const auto& t : r.terr_cov_tiles) {
    std::snprintf(b, sizeof b, " (%d,%d)", t.first, t.second);
    s += b;
  }
  s += "\n";
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  const Result r = derive(true);
  // R57: the partition must cost the fixture NOTHING. The pixel gate is
  // reference-derived and it has to stay the same number for the same reason,
  // not because it happened to come out the same.
  {
    const Result flat = derive(false);
    if (flat.replayed != r.replayed || flat.clipped != r.clipped ||
        flat.culled != r.culled || flat.accepted != r.accepted || flat.tiles != r.tiles) {
      std::printf("smoke_geom_fixture_gen: the meshlet partition changed the reference result "
                  "(flat replayed=%d accepted=%d tiles=%zu vs partitioned %d/%d/%zu)\n",
                  flat.replayed, flat.accepted, flat.tiles.size(), r.replayed, r.accepted,
                  r.tiles.size());
      return 1;
    }
  }
  if (kNM < 3) {
    std::printf("smoke_geom_fixture_gen: fewer than three meshlets gives no STEADY loop interval\n");
    return 1;
  }
  if (r.outside_grid || r.terr_outside_grid) {
    std::printf("smoke_geom_fixture_gen: a tile falls OUTSIDE the %dx%d render grid -- move the fixture "
                "(mesh=%d terrain=%d)\n",
                kGridTiles, kGridTiles, static_cast<int>(r.outside_grid),
                static_cast<int>(r.terr_outside_grid));
    return 1;
  }
  if (r.clipped == 0 || r.accepted < 8 || r.tiles.size() < 4) {
    std::printf("smoke_geom_fixture_gen: the fixture no longer exercises a clipped triangle, "
                "both views and several tiles (clipped=%d accepted=%d tiles=%zu)\n",
                r.clipped, r.accepted, r.tiles.size());
    return 1;
  }
  // ---- THE TERRAIN FIXTURE'S OWN PURPOSE, ENFORCED HERE -----------------
  // These three are the whole reason the terrain patch moved, and each is the
  // exact failure the move repairs. They are checks and not comments because
  // every parameter above is a knob and a knob that can be turned back to a
  // degenerate value silently is how a fixture stops measuring.
  if (r.terr_culled != 0) {
    std::printf("smoke_geom_fixture_gen: %d terrain triangle(s) have ZERO SCREEN AREA. Under "
                "CULL_NONE that can only mean the lattice is a plane through the eye again -- "
                "check SGF_TERR_BASE_H16 (0 puts the ground at the eye's own y) and the tilts. "
                "It is NOT to be repaired with a tolerance on the area test.\n",
                r.terr_culled);
    return 1;
  }
  if (r.terr_accepted == 0) {
    std::printf("smoke_geom_fixture_gen: no terrain triangle reaches GEOM.SETUP (%d submitted, "
                "%d clipped). The patch is sub-pixel again -- SGF_TERR_IZ0 sets its distance and "
                "the patch subtends 1/iz of the view's half width at its near edge.\n",
                r.terr_submitted, r.terr_clipped);
    return 1;
  }
  if (r.terr_cov_tiles.empty()) {
    std::printf("smoke_geom_fixture_gen: terrain is accepted into %zu tile(s) and COVERS NO PIXEL CENTRE. "
                "GEOM.CLIP's box test and the binner's corner test are both conservative -- they put a "
                "pixel centre in the BOUNDING BOX, never inside the triangle -- so accepted triangles "
                "can still dirty no tile and write no pixel. Give the patch more screen area: at LOD "
                "level 0 a cell is 0.5/iz px wide, and the height TILTS are what buy vertical extent.\n",
                r.terr_tiles.size());
    return 1;
  }
  // THE FRAME MUST FIT IN GEOM.BINNER'S TRIANGLE STORE. Every triangle that
  // GEOM.CLIP accepts reaches GEOM.SETUP and then the binner, and the binner
  // holds `kBinnerTriCap` of them per frame. Past that it WALLS: the tail of
  // the frame is dropped whole and `render_overflow_o` latches, so the pixel
  // count this file derives would describe a frame the console never drew.
  // Checked here rather than left to the bench, because the bench's symptom is
  // a pixel shortfall and the cause is three blocks upstream.
  if (r.accepted + r.terr_accepted > kBinnerTriCap) {
    std::printf("smoke_geom_fixture_gen: the fixture offers %d triangle(s) to GEOM.SETUP (%d mesh + "
                "%d terrain) and GEOM.BINNER holds %d per frame. Past that it walls off the tail of "
                "the frame and latches render_overflow_o, so the tile union below would not be the "
                "one the console resolves. Reduce the terrain patch's on-screen extent or the number "
                "of subpatch jobs.\n",
                r.accepted + r.terr_accepted, r.accepted, r.terr_accepted, kBinnerTriCap);
    return 1;
  }
  // AND THE MESH IS NOT TRADED AWAY. The terrain patch must ADD coverage, never
  // replace it: every tile the mesh touched before terrain existed is still in
  // the union by construction, and this says the union actually GREW, which is
  // the difference between "terrain draws" and "terrain is drawn somewhere the
  // mesh already was and nothing can tell".
  if (r.union_tiles().size() <= r.cov_tiles.size()) {
    std::printf("smoke_geom_fixture_gen: terrain covers %zu tile(s) and the union is still the "
                "mesh's %zu -- the terrain patch lands entirely inside tiles the mesh already "
                "resolves, so `render_pixels_o` cannot show it\n",
                r.terr_cov_tiles.size(), r.cov_tiles.size());
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
    std::printf("smoke_geom_fixture_gen: fresh (mesh replayed=%d clipped=%d accepted=%d covered_tiles=%zu | "
                "terrain submitted=%d clipped=%d culled=%d accepted=%d covered_tiles=%zu | union tiles=%zu pixels=%zu | binner refs=%d (mesh %d) depth=%d of TRI_CAP %d)\n",
                r.replayed, r.clipped, r.accepted, r.cov_tiles.size(), r.terr_submitted, r.terr_clipped,
                r.terr_culled, r.terr_accepted, r.terr_cov_tiles.size(), r.union_tiles().size(),
                r.union_tiles().size() * 256, r.tile_refs, r.mesh_tile_refs, r.max_depth(),
                kBinnerTriCap);
    return 0;
  }
  FILE* f = std::fopen(path, "wb");
  if (!f) {
    std::printf("smoke_geom_fixture_gen: cannot write %s\n", path);
    return 1;
  }
  std::fwrite(text.data(), 1, text.size(), f);
  std::fclose(f);
  std::printf("smoke_geom_fixture_gen: wrote %s (mesh replayed=%d clipped=%d accepted=%d covered_tiles=%zu | "
              "terrain submitted=%d clipped=%d culled=%d accepted=%d covered_tiles=%zu | union tiles=%zu pixels=%zu | binner refs=%d (mesh %d) depth=%d of TRI_CAP %d)\n",
              path, r.replayed, r.clipped, r.accepted, r.cov_tiles.size(), r.terr_submitted, r.terr_clipped,
              r.terr_culled, r.terr_accepted, r.terr_cov_tiles.size(), r.union_tiles().size(),
              r.union_tiles().size() * 256, r.tile_refs, r.mesh_tile_refs, r.max_depth(),
              kBinnerTriCap);
  return 0;
}
