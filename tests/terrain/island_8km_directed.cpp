// island_8km_directed.cpp -- an 8 km floating island, and what makes it affordable.
//
// The claim under test is not "a big number fits in a u16". It is that an 8 km
// island is REPRESENTABLE and RESIDENT-AFFORDABLE at the canonical pitch,
// which are two different things and only the second one is interesting.
//
//   8000 m / 64 m per patch = 125 patches per side = 15,625 patches
//   residency                                      =  1,024 pages
//
// A dense directory cannot express this island at all. terrain_rules §1.4
// resolves it by making the directory SPARSE: "a patch that is entirely sky
// simply does not exist -- no page, no sheet, no draw. Sacrifice pays for its
// void; we do not."
//
// So the test builds the shape the spec costs out -- solid ground of roughly
// 3.25 km², the figure §1.4 uses -- and requires it to fit the residency with
// room to spare, at 8 km across. If sparseness is real, the sky between the
// ground is free and the island is affordable. If it is not, this fails by a
// factor of fifteen and no amount of streaming rescues it.

#include <cstdint>
#include <cstdio>

#include "zref/zref_island.hpp"

namespace {

int g_checks = 0;
bool g_failed = false;

void check(bool ok, const char* what, long want, long got) {
  ++g_checks;
  if (!ok) {
    g_failed = true;
    std::printf("FAIL: %s: expected %ld, got %ld\n", what, want, got);
  }
}

constexpr int32_t kSide = 125;  // patches per side for 8 km at 64 m
constexpr int32_t kResidency = 1024;

}  // namespace

int main() {
  namespace isl = zref::island;

  // ---- the canonical island ------------------------------------------------
  isl::Desc d;
  d.island_id = 0x515Au;  // any id; the residency hash over it lives elsewhere
  d.pitch_log2 = 1;       // canonical 2.0 m -> 64 m patch
  d.extent_ix = static_cast<uint16_t>(kSide);
  d.extent_iz = static_cast<uint16_t>(kSide);
  d.origin_y = 40 << 16;  // the DATUM: this island floats 40 m up
  d.tileset_id = 7;

  check(isl::pitch_legal(d.pitch_log2), "the canonical pitch is one of the ratified four", 1,
        isl::pitch_legal(d.pitch_log2) ? 1 : 0);
  check(isl::patch_metres(d.pitch_log2) == 64,
        "a patch at canonical pitch is 64 m per side -- 32 cells at 2.0 m", 64,
        isl::patch_metres(d.pitch_log2));

  const int64_t mx = isl::extent_metres_x(d);
  std::printf("  island: %d x %d patches, %lld x %lld m, datum %d m\n", kSide, kSide,
              static_cast<long long>(mx), static_cast<long long>(isl::extent_metres_z(d)),
              d.origin_y >> 16);

  check(mx == 8000, "the island spans 8000 m across", 8000, static_cast<long>(mx));

  // ---- the sparse directory ------------------------------------------------
  isl::Directory dir(d);

  check(dir.dense_count() == static_cast<int64_t>(kSide) * kSide,
        "a DENSE grid of this island would be 15,625 patches", 15625,
        static_cast<long>(dir.dense_count()));
  check(dir.dense_count() > kResidency * 15,
        "which is more than fifteen times the residency -- a dense directory "
        "cannot express this island at all, which is why sparseness is the "
        "feature and not an optimisation",
        1, dir.dense_count() > kResidency * 15 ? 1 : 0);

  // Solid ground of ~3.25 km², the figure terrain_rules §1.4 costs out: a disc,
  // because a floating island is not a square and the corners are exactly the
  // sky that must cost nothing.
  //
  //   3.25 km² / (64 m)² = 793.5 patches  ->  radius ~= sqrt(793.5/pi) = 15.9
  const double kTargetPatches = 3.25e6 / (64.0 * 64.0);
  const double radius = 15.9;
  const int32_t cx = kSide / 2, cz = kSide / 2;
  uint32_t handle = 1;
  for (int32_t iz = 0; iz < kSide; ++iz) {
    for (int32_t ix = 0; ix < kSide; ++ix) {
      const double dx = ix - cx, dz = iz - cz;
      if (dx * dx + dz * dz <= radius * radius) dir.set(ix, iz, handle++);
    }
  }

  const std::size_t solid = dir.resident_count();
  std::printf(
      "  solid ground: %zu patches of %lld (%.1f%% of the grid), "
      "target from terrain_rules 1.4 was ~%.0f\n",
      solid, static_cast<long long>(dir.dense_count()), 100.0 * solid / dir.dense_count(),
      kTargetPatches);

  check(solid <= static_cast<std::size_t>(kResidency),
        "a Sacrifice-scale island's solid ground FITS the 1,024 residency -- at "
        "8 km across, because the sky between the ground costs nothing",
        1, solid <= static_cast<std::size_t>(kResidency) ? 1 : 0);
  check(solid > 700,
        "and it is a real island rather than a token one -- comparable to the "
        "~793 patches terrain_rules 1.4 costs out",
        1, solid > 700 ? 1 : 0);

  // ---- sky is an ANSWER, not a failure ------------------------------------
  isl::Ledger L{};
  const isl::Lookup centre = dir.find(cx, cz, &L);
  check(centre.outcome == isl::Outcome::kResident, "the island's centre is solid ground", 0,
        static_cast<long>(centre.outcome));

  const isl::Lookup corner = dir.find(1, 1, &L);
  check(corner.outcome == isl::Outcome::kOpenSky,
        "a corner of the grid is OPEN SKY -- reported as sky and not as a miss, "
        "because for most of an island's grid this is the ordinary answer",
        static_cast<long>(isl::Outcome::kOpenSky), static_cast<long>(corner.outcome));

  const isl::Lookup beyond = dir.find(kSide + 10, 0, &L);
  check(beyond.outcome == isl::Outcome::kOutOfExtent,
        "and a coordinate past the extent is OUT OF EXTENT, which is a "
        "different thing from sky: one is inside an island and empty, the other "
        "is not this island at all",
        static_cast<long>(isl::Outcome::kOutOfExtent), static_cast<long>(beyond.outcome));

  check(L.resident == 1 && L.open_sky == 1 && L.out_of_extent == 1,
        "and each outcome is counted separately, so a run that answered "
        "everything with sky could not look like a run that found ground",
        3, static_cast<long>(L.resident + L.open_sky + L.out_of_extent));

  // ---- an island cannot own a patch it does not span ----------------------
  check(!dir.set(kSide, 0, 999),
        "publishing outside the extent is refused -- the directory is the thing "
        "that says what the island HAS",
        0, dir.set(kSide, 0, 999) ? 1 : 0);

  // ---- the floating part ---------------------------------------------------
  // Heights are relative to the island DATUM, which is what lets an island sit
  // anywhere in the fx16 +/-32 km world while height16 stays within +/-128 m of
  // local relief. Without a datum, "floating at 40 m" would spend the height
  // budget on altitude instead of on terrain.
  check(d.origin_y == (40 << 16),
        "the island carries a fx16 datum, so its relief is local and it can "
        "float at any altitude without spending height16 range on getting there",
        40, d.origin_y >> 16);

  std::printf("[island_8km_directed] %d checks %s\n", g_checks, g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
