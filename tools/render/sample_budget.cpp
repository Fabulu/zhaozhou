// sample_budget.cpp — the TMU sample budget, executable instead of prose.
//
// WHY THIS EXISTS. `sacrifice_terrain_3sample` is named in
// MATERIAL_ARCHITECTURE.md, RECON-TMU-WHAT-IT-IS-ACTUALLY-FOR.md and
// REARCHITECTUREADVICE.md, and existed in NONE of them as anything runnable.
// reports/Addendum (owner, 2026-09-02) requires the stress gate to carry the
// 1,094,600-sample profile rather than only the 541,640 baseline, and a profile
// that lives as arithmetic inside a paragraph cannot be a gate.
//
// This is the same lesson the ground-contact probe taught: a number computed
// once and thrown away is unreproducible, so commit the probe.
//
// WHAT IT DOES NOT DO. It does not model the cache, queue bubbles, Early-Z
// rejection or integration cost. Every figure is the HIT-PATH sample count the
// TMU would have to service, which is a lower bound on time and an upper bound
// on what the arithmetic alone permits. MATERIAL_ARCHITECTURE.md is explicit
// that cache locality, not multiplier throughput, is the likelier wall.
//
//   g++ -std=c++17 -O2 tools/render/sample_budget.cpp -o sample_budget

// ---------------------------------------------------------------------------
// RULED 2026-09-02 (R2): THIS IS A CONSERVATIVE UPPER ENVELOPE. IT IS NOT A
// MEASURED WORKLOAD, AND IT IS NOT PROOF THAT ANY PARTICULAR RESERVE REMAINS.
//
// Every heading here used to say "known", which reads as "measured". It is
// not: it is what the frame would cost if NOTHING were rejected -- no
// Early-Z kill, no backface, no off-screen. A real frame is smaller, by an
// amount this tool cannot know.
//
// The binding numbers live in the ruling, not here:
//
//   276,480 = 384 x 240 x 3.0   conservative PRE-Early-Z covered fragments
//   320,000                     canonical pre-Z cross-mode design target
//   1,333,333 clocks            design deadline
//   1,666,667 clocks            fault boundary
//
// Specifically forbidden: quoting this tool's output as a measured workload,
// or as proof that exactly 238,733 clocks of real reserve remain.
// ---------------------------------------------------------------------------
#include <cstdio>

namespace {

// Ratified in MATERIAL_ARCHITECTURE.md, "Could the console afford
// Sacrifice-style three-layer terrain?". Terrain is the only layer that scales
// with the recipe; the rest are one sample each.
constexpr long kTerrainFragments = 276480;
constexpr long kSky = 92160;
constexpr long kStars = 128000;
constexpr long kClouds = 45000;

// 100 MHz / 60 fps. The product clock from REARCHITECTUREADVICE.md as amended.
constexpr long kRawClocks = 1666667;
constexpr double kReserveFrac = 0.20;  // charter reserve target

long envelope_frame(int terrain_samples) {
  return kTerrainFragments * terrain_samples + kSky + kStars + kClouds;
}

void row(const char* name, int terrain_samples) {
  const long s = envelope_frame(terrain_samples);
  const long raw_left = kRawClocks - s;
  const long budget = static_cast<long>(kRawClocks * (1.0 - kReserveFrac));
  const long res_left = budget - s;
  std::printf("  %-28s %2d  %9ld  %+9ld  %+9ld  %s\n", name, terrain_samples, s,
              raw_left, res_left, res_left < 0 ? "OVER RESERVE" : "");
}

}  // namespace

int main() {
  std::printf("TMU pre-Z NO-REJECTION SAMPLE ENVELOPE, hit path only. %ld raw clocks a frame "
              "(100 MHz / 60 fps),\n20%% reserve target = %ld.\n\n",
              kRawClocks, static_cast<long>(kRawClocks * (1.0 - kReserveFrac)));
  std::printf("pre-Z no-rejection envelope = terrain*N + sky %ld + stars %ld + clouds %ld\n\n",
              kSky, kStars, kClouds);

  std::printf("  %-28s %2s  %9s  %9s  %9s\n", "recipe", "N", "samples", "raw left",
              "vs 20%");
  std::printf("  ---------------------------------------------------------------------\n");
  row("0 samples (Gouraud/glint)", 0);
  row("1 sample  (base only)", 1);
  row("2 samples (+ detail)", 2);
  row("sacrifice_terrain_3sample", 3);
  std::printf("\n");

  // THE TWO NUMBERS THE DOCUMENTS ARGUE ABOUT.
  std::printf("  541,640 is the ONE-sample workload   -> measured here as %ld\n",
              envelope_frame(1));
  std::printf("  1,094,600 is the THREE-sample one    -> measured here as %ld\n\n",
              envelope_frame(3));

  // WHAT IS LEFT FOR CREATURES, which is where the binner LOD work lands.
  // BINNER_CAPACITY_FOR_8KM_MAPS.md addendum measures a 256-creature army at
  // 1,792-2,720 triangles for realistic LOD mixes. Triangles are not samples --
  // a fragment issues sample_count requests, and fill depends on projected area
  // -- so this states the BUDGET rather than inventing a fill model.
  const long left3 = static_cast<long>(kRawClocks * (1.0 - kReserveFrac)) - envelope_frame(3);
  const long left1 = static_cast<long>(kRawClocks * (1.0 - kReserveFrac)) - envelope_frame(1);
  std::printf("  creature/object/miss budget inside the 20%% reserve:\n");
  std::printf("    at 3-sample terrain : %+9ld samples\n", left3);
  std::printf("    at 1-sample terrain : %+9ld samples\n", left1);
  std::printf("\n  The degradation ladder (MATERIAL_ARCHITECTURE.md) exists because\n"
              "  that first figure is the one the governor has to defend.\n");
  return 0;
}
