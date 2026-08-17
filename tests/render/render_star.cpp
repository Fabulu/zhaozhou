// render_star.cpp — the stars_and_flares.md §13 anchor tests: every anchor
// below was computed BY HAND from the spec formulas before the
// implementation ran (the values are in the comments), so a green here is
// a green that could have been red.
//
// Law: spec/stars_and_flares.md §2 (gamut table, pulsar duty), §3 (ramp
// build/slew, boil, SATUR), §4 (corona bake), §5 (flare law, ghosts, fade,
// border, budget), §6 (LOD ladder, glint clamp), §7 (starfield hash — the
// imported harness goldens are the oracle, README in tests/golden/
// starfield/), §8 (celestial_state round-trip), §13 (this plan), §15
// (motion trails: ring, subtract-8 decay, exact asymmetric diffusion,
// static-skip, replay-exactness).

#include "zref/zref_star.hpp"

#include "zhao_abi.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

using namespace zref;

// S00 identity with the class-table undertone (no hash draws needed)
star::StarIdentity s00_identity() {
  star::StarIdentity id;
  id.cls = 0;
  id.radius_milli = 5000;
  for (int c = 0; c < 3; ++c) id.under6[c] = star::kGamut[0].under6[c];
  return id;
}

// ---- §13 star_ramp_anchor --------------------------------------------------
// Hand computation (S00, ×4 domain): P1 = (256,216,112), P2 = (252,232,160).
// ramp[32] (segment [24..40), i−a = 8, n = 16):
//   R: 256 + rhu((252−256)·8,16) = 256 + rhu(−32,16) = 256 − 2 = 254
//   G: 216 + rhu((232−216)·8,16) = 216 + 8  = 224
//   B: 112 + rhu((160−112)·8,16) = 112 + 24 = 136
// Also: ramp[24] = P1 clamped = (255,216,112); ramp[40] = P2 = (252,232,160);
// ramp[63]: P2 + rhu((P3−P2)·23,24) = (256,278,298) → clamps to white.
void test_ramp_anchor() {
  int16_t pts[12];
  star::ramp_points(s00_identity(), pts);
  uint8_t ramp[64][3];
  star::ramp_build(pts, ramp);
  check(ramp[32][0] == 254 && ramp[32][1] == 224 && ramp[32][2] == 136,
        "S00 ramp[32] == (254,224,136) — the §13 anchor");
  check(ramp[0][0] == 0 && ramp[0][1] == 0 && ramp[0][2] == 0, "ramp[0] = P0 black");
  check(ramp[24][0] == 255 && ramp[24][1] == 216 && ramp[24][2] == 112,
        "ramp[24] = P1 (undertone ×4, R clamps 256→255)");
  check(ramp[40][0] == 252 && ramp[40][1] == 232 && ramp[40][2] == 160, "ramp[40] = P2 (class ×4)");
  check(ramp[63][0] == 255 && ramp[63][1] == 255 && ramp[63][2] == 255,
        "ramp[63] saturates white (the deliberate early per-channel clamp)");
}

// ---- §13 star_palette_boil -------------------------------------------------
// tick 96 → rot = (96/3) mod 63 = 32. Index e=40 → 1 + ((39+32) mod 63) = 9.
// d = 4r → SATUR = 12·4 = 48. d ≥ 5.25r → SATUR = 63 → every entry white.
void test_palette_boil() {
  check(star::boil_rot(96) == 32, "tick 96 → rot 32");
  check(star::boil_index(40, 32, 0) == 9, "index 40 at rot 32 → ramp 9");
  check(star::satur_of(4000, 1000) == 48, "d = 4r → SATUR 48");
  check(star::satur_of(5250, 1000) == 63, "d = 5.25r → SATUR 63");

  int16_t pts[12];
  star::ramp_points(s00_identity(), pts);
  uint8_t ramp[64][3];
  star::ramp_build(pts, ramp);
  uint8_t pal[64][3];
  star::palette_disc(ramp, 96, 0, 1000, pal);  // d = 0: no washout
  check(pal[40][0] == ramp[9][0] && pal[40][1] == ramp[9][1] && pal[40][2] == ramp[9][2],
        "pal_d[40] at tick 96 == ramp[9]");
  star::palette_disc(ramp, 96, 5250, 1000, pal);  // washed white
  bool all_white = true;
  for (int e = 1; e < 64; ++e)
    if (pal[e][0] != 255 || pal[e][1] != 255 || pal[e][2] != 255) all_white = false;
  check(all_white, "d ≥ 5.25r: the whole disc palette is white (SATUR floor)");
  // boil is a PERMUTATION of ramp indices: the colour SET never changes
  // (the reel palette law rests on this)
  star::palette_disc(ramp, 33, 0, 1000, pal);
  bool subset = true;
  for (int e = 1; e < 64; ++e) {
    bool found = false;
    for (int i = 1; i < 64; ++i)
      if (pal[e][0] == ramp[i][0] && pal[e][1] == ramp[i][1] && pal[e][2] == ramp[i][2])
        found = true;
    if (!found) subset = false;
  }
  check(subset, "boil palette entries are always ramp entries (index permutation)");
}

// ---- §3 ramp slew: ±1/tick per channel, never pops -------------------------
void test_ramp_slew() {
  star::RampState st;
  star::StarIdentity a = s00_identity();
  star::ramp_retarget(st, a);  // first: snap
  check(st.cur[6] == 252, "first retarget snaps cur = tgt (P2.R = 252)");
  star::StarIdentity b;  // S03 red giant: P2 = (252,120,80)
  b.cls = 3;
  for (int c = 0; c < 3; ++c) b.under6[c] = star::kGamut[3].under6[c];
  star::ramp_retarget(st, b);
  check(st.cur[7] == 232 && st.tgt[7] == 120, "retarget sets target, keeps current");
  star::ramp_slew_step(st);
  check(st.cur[7] == 231, "slew walks −1/tick");
  int steps = 0;
  while (std::memcmp(st.cur, st.tgt, sizeof st.cur) != 0 && steps < 1000) {
    star::ramp_slew_step(st);
    ++steps;
  }
  // largest channel delta S00→S03 is P1.G: 216→96 = 120 steps; one was
  // taken above, so 119 remain
  check(steps == 119, "slew converges in exactly max|Δ| ticks (120 for S00→S03)");
}

// ---- §13 corona_bake_anchor ------------------------------------------------
// halo_space (core16 = 5): fgm_h = rhu(120·5,16) = 38, k = rhu(63<<16,82) =
// 50351. Texel (105,63): dx=83, dy=−1 → rr = isqrt(6890) = 83 →
// pix = 63 − rescale_u(45·50351,16) = 63 − 35 = 28.
// Texel (64,63): dx=1, dy=−1 → rr = 1 ≤ 38 → 63. Texel (127,63): rr = 127
// ≥ 120 → 0.
void test_corona_anchor() {
  const star::Sprite8 space = star::corona_sprite(5);
  check(space.w == 128 && space.h == 128, "corona sprite is 128×128");
  check(space.pix[63 * 128 + 105] == 28, "halo_space (105,63) == 28 — the §13 anchor");
  check(space.pix[63 * 128 + 64] == 63, "halo_space (64,63) == 63 (flat core)");
  check(space.pix[63 * 128 + 127] == 0, "halo_space rr ≥ 120 == 0 (outside)");
  // halo_atmo (core16 = 0): fgm 0, k = rhu(63<<16,120) = 34406. Texel
  // (64,63): rr = 1 > 0 → 63 − rescale_u(1·34406,16) = 63 − 1 = 62 (the
  // pure glow ball has NO flat core).
  const star::Sprite8 atmo = star::corona_sprite(0);
  check(atmo.pix[63 * 128 + 64] == 62, "halo_atmo (64,63) == 62 (no flat core)");
  // halo_airless (core16 = 8): fgm 60. Texel (93,63): dx = 59, dy = −1 →
  // rr = isqrt(3482) = 59 ≤ 60 → hard core 63.
  const star::Sprite8 airless = star::corona_sprite(8);
  check(airless.pix[63 * 128 + 93] == 63, "halo_airless (93,63) == 63 (hard core to 60)");
  // ...and (105,63): rr = 83 → 63 − rescale_u(23·68813,16) = 63 − 24 = 39
  check(airless.pix[63 * 128 + 105] == 39, "halo_airless (105,63) == 39 (steeper skirt)");
}

// ---- §13 flare_ghost_anchor ------------------------------------------------
// light (300,80), centre (192,120): Δ = (108,−40).
//   ghost0: 192 + rescale(−26·108, 8) = 192 − 11 = 181; 120 + 4 = 124
//   ghost1: 192 − 32 = 160; 120 + 12 = 132
//   ghost2: 192 − 97 = 95;  120 + 36 = 156
void test_flare_ghosts() {
  const flare::LightLaw law = flare::light_law(40000, 1000);  // d/r 40 → k 40, b 3
  check(law.k == 40 && law.bucket == 3 && law.sprite == 0, "d/r 40 → k 40, bucket 3, burst12");
  flare::Splat s[4];
  const int n = flare::emit(300, 80, 192, 120, law, s);
  check(n == 4, "burst + 3 ghosts emitted");
  check(s[0].cx_px == 300 && s[0].cy_px == 80 && s[0].alpha == 255 && s[0].half_x_px == 40,
        "burst at the light, half k, alpha 255");
  check(s[1].cx_px == 181 && s[1].cy_px == 124, "ghost0 at (181,124) — the §13 anchor");
  check(s[2].cx_px == 160 && s[2].cy_px == 132, "ghost1 at (160,132)");
  check(s[3].cx_px == 95 && s[3].cy_px == 156, "ghost2 at (95,156)");
  // ghost half-sizes: rescale(40·26,8)=4, rescale(40·102,8)=16, rescale(40·410,8)=64
  check(s[1].half_x_px == 4 && s[2].half_x_px == 16 && s[3].half_x_px == 64,
        "ghost half-sizes k·26/256, k·102/256, k·410/256");
  check(s[1].alpha == 64 && s[3].alpha == 64, "ghost alpha 64");
}

// ---- §13 flare_far_streak + flare_fade + flare_border ----------------------
void test_flare_laws() {
  const flare::LightLaw far = flare::light_law(600000, 1000);
  check(far.k == 384 && far.sprite == 2, "d/r 600 → streak, k = 384 — the §13 anchor");
  const flare::LightLaw near = flare::light_law(4500, 1000);
  check(near.k == 5 && near.sprite == 0, "d/r 4.5 → burst12, k = 5 — the §13 anchor");

  // fade: 255 → 0 over exactly 15 ticks, no step exceeds 17
  uint8_t ctr = 15;
  uint8_t prev = flare::fade_alpha(ctr);
  check(prev == 255, "fade counter 15 → alpha 255");
  int ticks = 0;
  while (ctr > 0) {
    ctr = flare::fade_step(ctr, false);
    const uint8_t a = flare::fade_alpha(ctr);
    check(prev - a == 17, "fade step is exactly 17 alpha/frame");
    prev = a;
    ++ticks;
  }
  check(ticks == 15, "fade 255 → 0 takes exactly 15 ticks — the §13 anchor");

  // border fade
  check(flare::border_alpha(-3) == 0, "outside the edge → 0");
  check(flare::border_alpha(0) == 0, "at the edge → 0");
  check(flare::border_alpha(8) == 128, "8 px in → 128");
  check(flare::border_alpha(16) == 255, "16 px in → 255 (min clamp)");
  check(flare::border_alpha(400) == 255, "deep interior → 255");
}

// ---- §5 baked sprites ------------------------------------------------------
void test_flare_sprites() {
  const flare::Sprites& sp = flare::sprites();
  check(sp.burst12.w == 64 && sp.burst12.h == 64, "burst12 is 64×64");
  check(sp.burst4.w == 64 && sp.burst4.h == 64, "burst4 is 64×64");
  check(sp.streak.w == 96 && sp.streak.h == 16, "streak is 96×16");
  // the centre saturates (24 half-lines × 32 through the centre block)
  check(sp.burst12.pix[32 * 64 + 32] > 128, "burst12 centre is hot");
  // burst12 has more lit texels than burst4 (12 spokes vs 4)
  auto lit = [](const star::Sprite8& s) {
    int n = 0;
    for (uint8_t v : s.pix)
      if (v) ++n;
    return n;
  };
  check(lit(sp.burst12) > lit(sp.burst4), "burst12 lights more texels than burst4");
  // the streak is a horizontal line: lit texels only in the middle rows
  int lit_off_rows = 0;
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 96; ++x)
      if (sp.streak.pix[y * 96 + x] && (y < 7 || y > 8)) ++lit_off_rows;
  check(lit_off_rows == 0, "streak energy confined to the centre rows");
  // determinism: the bake is a pure function of frozen constants
  check(&flare::sprites() == &sp, "sprites baked once (same instance)");
  // observation for the report: peak arm value away from the core
  int peak_arm = 0;
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 64; ++x)
      if ((x - 32) * (x - 32) + (y - 32) * (y - 32) > 100 && sp.burst12.pix[y * 64 + x] > peak_arm)
        peak_arm = sp.burst12.pix[y * 64 + x];
  std::printf("render_star: burst12 peak arm value beyond r=10: %d/255\n", peak_arm);
}

// ---- §13 glint_min_brightness + pulsar_duty --------------------------------
void test_glint_and_pulsar() {
  check(star::glint_intensity6(1550 * 1000, 1000) == 48, "d = 1550r → 48 (75% floor)");
  check(star::glint_intensity6(100 * 1000, 1000) == 63, "close glint saturates 63");
  bool never_dimmer = true;
  for (int64_t d = 0; d <= 1600; d += 25)
    if (star::glint_intensity6(d * 1000, 1000) < 48) never_dimmer = false;
  check(never_dimmer, "glint never dimmer than 48 — the §13 anchor");
  check(star::pulsar_active(15400), "pulsar phase 15,400 → flash on — the §13 anchor");
  check(!star::pulsar_active(16500), "pulsar phase 16,500 → off");
  check(star::pulse_gain_u35(15400) == 160 && star::pulse_gain_u35(16500) == 32,
        "pulse gain 5.0 during flash, 1.0 off");
}

// ---- §13 starfield_harness_equivalence (BEFORE any rendering test) ---------
void test_starfield_equivalence() {
  const char* path = ZHAO_GOLDEN_DIR "/oracle.bin";
  FILE* f = std::fopen(path, "rb");
  if (!f) {
    std::fprintf(stderr, "FAIL: cannot open %s\n", path);
    ++failures;
    return;
  }
  int records = 0, mismatches = 0;
  for (int kx = -3; kx <= 3; ++kx) {
    for (int ky = -3; ky <= 3; ++ky) {
      for (int kz = -3; kz <= 3; ++kz) {
        uint8_t rec[20];
        if (std::fread(rec, 1, 20, f) != 20) {
          std::fprintf(stderr, "FAIL: golden truncated at record %d\n", records);
          ++failures;
          std::fclose(f);
          return;
        }
        auto le32 = [&](int i) {
          return static_cast<uint32_t>(rec[i]) | (static_cast<uint32_t>(rec[i + 1]) << 8) |
                 (static_cast<uint32_t>(rec[i + 2]) << 16) |
                 (static_cast<uint32_t>(rec[i + 3]) << 24);
        };
        const sky::SectorStar s = sky::starfield(kx, ky, kz);
        if (static_cast<uint32_t>(s.x) != le32(0) || static_cast<uint32_t>(s.y) != le32(4) ||
            static_cast<uint32_t>(s.z) != le32(8) || s.netpos != le32(12) || s.no_star != le32(16))
          ++mismatches;
        ++records;
      }
    }
  }
  std::fclose(f);
  check(records == 343, "343 golden sectors consumed");
  check(mismatches == 0, "starfield transliteration byte-exact vs the harness oracle");
}

// ---- §7 rarity gate --------------------------------------------------------
void test_starfield_rarity() {
  check(!sky::starfield_rarity_skip(0, 0, 0), "origin never skipped (e = 0)");
  check(!sky::starfield_rarity_skip(100, 5, -200), "near the core: e = 0, nothing skipped");
  // the ×30 y-crush: |ky| = 134 → e = 1 (30·134 = 4020) → parity gate
  check(!sky::starfield_rarity_skip(0, 134, 0), "ky 134, even sum: kept");
  check(sky::starfield_rarity_skip(1, 134, 0), "ky 134, odd sum: skipped (the disc edge)");
  // in-plane distance 4000 sectors → e = 1 as well
  check(sky::starfield_rarity_skip(4001, 0, 0), "|kx| 4001 odd: skipped");
  check(!sky::starfield_rarity_skip(4000, 0, 0), "|kx| 4000 even: kept");
  // magnitude law
  check(sky::starfield_intensity6(0) == 63, "rz 0 → 63");
  check(sky::starfield_intensity6(8192) == 62, "rz 8192 → 62");
  check(sky::starfield_intensity6(63LL << 13) == 0, "rz at the horizon → 0 (skip)");
}

// ---- §13 star_lod_ladder ---------------------------------------------------
void test_lod_ladder() {
  using star::LodRung;
  check(star::lod_rung_raw(6 * 256, 0, 1000) == LodRung::kDisc, "≥6 px → disc rung");
  check(star::lod_rung_raw(384, 0, 1000) == LodRung::kCorona, "1.5 px → corona rung");
  check(star::lod_rung_raw(100, 1000 * 1000, 1000) == LodRung::kGlint, "<1.5 px, d≤1550r → glint");
  check(star::lod_rung_raw(100, 2000 * 1000, 1000) == LodRung::kPoint, "beyond → starfield point");

  star::LodState st;
  check(star::lod_select(st, 2000, 0, 1000) == LodRung::kDisc, "init at disc");
  // a radius below the hysteresis-shifted boundary (1382): must HOLD 15
  int flips_early = 0;
  for (int t = 0; t < 15; ++t)
    if (star::lod_select(st, 1300, 0, 1000) != LodRung::kDisc) ++flips_early;
  check(flips_early == 0, "no rung flip within the 15-tick hold — the §13 anchor");
  LodRung r = LodRung::kDisc;
  for (int t = 0; t < 5; ++t) r = star::lod_select(st, 1300, 0, 1000);
  check(r == LodRung::kCorona, "after the hold the rung follows the radius");
  // 10% hysteresis: 1600 q8 (6.25 px > 6 px raw) must NOT flip back —
  // promotion needs > 1690
  for (int t = 0; t < 40; ++t) r = star::lod_select(st, 1600, 0, 1000);
  check(r == LodRung::kCorona, "radius inside the 10% band does not flip back");
  for (int t = 0; t < 40; ++t) r = star::lod_select(st, 1700, 0, 1000);
  check(r == LodRung::kDisc, "crossing the +10% boundary promotes after the hold");
}

// ---- §13 capture_star_state_roundtrip --------------------------------------
void test_state_roundtrip() {
  star::CelestialState a;
  star::ramp_retarget(a.ramp[0], s00_identity());
  a.ramp[0].tgt[3] = 17;
  a.ramp[1].init = 1;
  a.ramp[1].cur[11] = -302;  // s16 pre-clamp domain is signed
  a.slots.light_id[0] = 0xBEEF;
  a.slots.fade_ctr[1] = 9;
  a.slots.latched_tag[2] = star::glow_tag(41);
  a.stars[0] = {11, 15400, 259, 0xDEADBEEFu};
  a.stars[1] = {3, 0, 27123, 42};
  a.galaxy_seed = 0x5EED5EEDu;
  a.cam_sector[0] = -100;
  a.cam_sector[2] = 4000;
  // §15 trail rings (v1.1): one filling, one full with a mid-ring head
  for (uint32_t k = 0; k < star::kTrailN; ++k) {
    a.trails[0].x_px[k] = static_cast<uint16_t>(1000 + k);
    a.trails[0].y_px[k] = static_cast<uint16_t>(2000 + k * 7);
    a.trails[1].x_px[k] = static_cast<uint16_t>(k * 511);
    a.trails[1].y_px[k] = static_cast<uint16_t>(65000 - k);
  }
  a.trails[0].head = 0;
  a.trails[0].length = 3;
  a.trails[1].head = 5;
  a.trails[1].length = star::kTrailN;

  uint8_t buf[star::kCelestialStateBytes];
  star::celestial_state_serialize(a, buf);
  star::CelestialState b;
  star::celestial_state_deserialize(buf, b);
  uint8_t buf2[star::kCelestialStateBytes];
  star::celestial_state_serialize(b, buf2);
  check(std::memcmp(buf, buf2, sizeof buf) == 0, "serialize∘deserialize is byte-stable");
  check(b.ramp[0].tgt[3] == 17 && b.ramp[1].cur[11] == -302, "ramp state survives (signed s16)");
  check(b.slots.light_id[0] == 0xBEEF && b.slots.fade_ctr[1] == 9 &&
            star::tag_is_glow(b.slots.latched_tag[2]),
        "flare slots survive");
  check(b.stars[0].cls == 11 && b.stars[0].spin_phase == 15400 && b.stars[0].radius_milli == 259 &&
            b.stars[1].texture_seed == 42,
        "star slots survive");
  check(b.galaxy_seed == 0x5EED5EEDu && b.cam_sector[2] == 4000, "seed + camera sector survive");
  check(b.trails[0].length == 3 && b.trails[1].head == 5 && b.trails[0].x_px[7] == 1007 &&
            b.trails[1].y_px[2] == 64998,
        "trail rings survive (§15, including unwritten slots of a filling ring)");
  check(star::kCelestialStateBytes == 236, "the §8 layout is 236 bytes since v1.1 (168 + 2×34)");
}

// ---- §7 identity schedule --------------------------------------------------
void test_identity() {
  const star::StarIdentity id = star::identity(3, -1, 2, 0xC0FFEEu);
  check(id.cls < 12, "class in range");
  const star::StarClass& c = star::kGamut[id.cls];
  check(id.radius_milli >= c.ray_milli && id.radius_milli < c.ray_milli + c.rayvar_milli,
        "radius_milli = class_ray + (h1 mod rayvar)");
  // determinism + seed sensitivity
  const star::StarIdentity id2 = star::identity(3, -1, 2, 0xC0FFEEu);
  check(id.texture_seed == id2.texture_seed && id.cls == id2.cls, "identity is deterministic");
  const star::StarIdentity id3 = star::identity(3, -1, 2, 0xC0FFEFu);
  check(id.texture_seed != id3.texture_seed, "a different galaxy seed is a different star");
  // the class distribution follows CLASS_PICK (spot: all classes reachable)
  bool seen[12] = {};
  for (int i = 0; i < 4000; ++i) seen[star::identity(i, i * 7 + 1, i * 13 + 5, 1).cls] = true;
  int nseen = 0;
  for (bool s : seen)
    if (s) ++nseen;
  check(nseen == 12, "every gamut class is drawn by the identity schedule");
  // infant undertone is per-identity (varies), others are the table row
  const star::StarClass& s9 = star::kGamut[9];
  (void)s9;
  bool infant_varies = false;
  uint8_t first_u[3] = {0, 0, 0};
  bool have_first = false;
  for (int i = 0; i < 20000 && !infant_varies; ++i) {
    const star::StarIdentity x = star::identity(i, 2 * i + 1, 3 * i + 2, 7);
    if (x.cls != 9) continue;
    if (!have_first) {
      for (int ch = 0; ch < 3; ++ch) first_u[ch] = x.under6[ch];
      have_first = true;
    } else if (std::memcmp(first_u, x.under6, 3) != 0) {
      infant_varies = true;
    }
  }
  check(infant_varies, "S09 infant undertone varies per identity (§2)");
}

// ---- §3 starface bake ------------------------------------------------------
void test_starface() {
  const star::Sprite8 a = star::starface(0x1234u, 2);
  check(a.w == 128 && a.h == 128, "starface is 128×128");
  check(a.pix[0] == 0, "corner outside the disc → index 0");
  const uint8_t centre = a.pix[63 * 128 + 63];
  check(centre >= 1 && centre <= 63, "centre inside → intensity 1..63");
  int lit = 0;
  for (uint8_t v : a.pix)
    if (v) ++lit;
  // disc area ≈ π·60² ≈ 11310 of 16384 texels
  check(lit > 10000 && lit < 12000, "disc fill matches the 120-half-texel radius");
  const star::Sprite8 b = star::starface(0x1234u, 2);
  check(a.pix == b.pix, "starface bake is deterministic");
  const star::Sprite8 c = star::starface(0x1234u, 5);
  check(a.pix != c.pix, "compact-class smoothing (5 passes) changes the face");
  const star::Sprite8 d = star::starface(0x9999u, 2);
  check(a.pix != d.pix, "a different texture seed is a different face");
  // asin16 sanity: the ONE table's inverse
  check(star::asin16(0) == 0 && star::asin16(65536) == 0x4000 && star::asin16(-65536) == -0x4000,
        "asin16 endpoints");
  check(star::asin16(zref::fx_sin(zref::angle16{0x2000}).raw) == 0x2000,
        "asin16 inverts fx_sin at a quarter-turn midpoint");
}

// ---- the compositor: occlusion probe, fade, budget, determinism ------------
void test_compose() {
  const uint32_t W = 384, H = 240;
  std::vector<uint8_t> rgb(W * H * 3, 0);
  std::vector<int32_t> depth(W * H, 0);
  // an occluder covering the right half (depth 25 = the under-plane rung)
  for (uint32_t y = 0; y < H; ++y)
    for (uint32_t x = W / 2; x < W; ++x) depth[y * W + x] = 25;

  const star::StarIdentity id = s00_identity();
  int16_t pts[12];
  star::ramp_points(id, pts);
  uint8_t ramp[64][3];
  star::ramp_build(pts, ramp);
  const star::Sprite8 face = star::starface(1, 2);
  const star::Sprite8 corona = star::corona_sprite(5);

  star::ComposeLight L;
  L.x_px = 96;
  L.y_px = 120;
  L.disc_r_px = 20;
  L.halo_r_px = 60;
  L.d_milli = 40000;
  L.r_milli = 5000;
  L.ramp = ramp;
  L.face = &face;
  L.corona = &corona;
  L.flare_mode = 1;
  L.tint[0] = 255;
  L.tint[1] = 235;
  L.tint[2] = 162;
  L.probe_x = 96;
  L.probe_y = 120;

  star::FlareSlots slots;
  star::ComposeStats stats;
  star::compose_view(rgb.data(), depth.data(), W, H, 0, 0, W, H, 0, &L, 1, nullptr, 0, slots,
                     &stats);
  check(stats.star_fragments > 0, "disc + halo fragments drawn");
  check(star::tag_is_glow(slots.latched_tag[0]), "probe over the disc latches a GLOW tag");
  check(slots.fade_ctr[0] == 1, "fade counter steps +1 on the first visible frame");
  check(stats.flare_texels > 0 && stats.flare_texels <= flare::kFlareTexelBudget,
        "flare splats accumulated inside the §5 budget");

  // determinism: same inputs, same bytes
  std::vector<uint8_t> rgb2(W * H * 3, 0);
  std::vector<int32_t> depth2 = depth;
  for (uint32_t y = 0; y < H; ++y)
    for (uint32_t x = W / 2; x < W; ++x) depth2[y * W + x] = 25;
  star::FlareSlots slots2;
  star::compose_view(rgb2.data(), depth2.data(), W, H, 0, 0, W, H, 0, &L, 1, nullptr, 0, slots2,
                     nullptr);
  check(rgb == rgb2, "compose_view is deterministic");

  // occlusion: move the light behind the occluder → the probe reads no
  // glow (the disc never drew there) → the fade walks DOWN 1/frame
  L.x_px = L.probe_x = 300;
  uint8_t before = slots.fade_ctr[0];
  star::compose_view(rgb.data(), depth.data(), W, H, 0, 0, W, H, 1, &L, 1, nullptr, 0, slots,
                     nullptr);
  check(!star::tag_is_glow(slots.latched_tag[0]), "occluded probe latches a non-GLOW tag");
  check(slots.fade_ctr[0] + 1 == before || (before == 0 && slots.fade_ctr[0] == 0),
        "occlusion fades by exactly one counter step per frame (never pops)");

  // §5 budget: a huge streak-range light must stay inside 16384 texels
  star::ComposeLight far = L;
  far.x_px = far.probe_x = 96;
  far.d_milli = 600LL * 5000;
  far.r_milli = 5000;
  star::FlareSlots fslots;
  fslots.fade_ctr[0] = 15;  // pre-faded-in
  star::ComposeStats fstats;
  std::vector<uint8_t> rgb3(W * H * 3, 0);
  std::vector<int32_t> depth3(W * H, 0);
  star::compose_view(rgb3.data(), depth3.data(), W, H, 0, 0, W, H, 0, &far, 1, nullptr, 0, fslots,
                     &fstats);
  check(fstats.flare_texels <= flare::kFlareTexelBudget,
        "flare_texels ≤ 16384 even at streak range (§5 bound; the §13 cost assertion)");
  std::printf("render_star: far-streak flare_texels %u, splats dropped %u\n", fstats.flare_texels,
              fstats.splats_dropped);
}

// ---- §13 star_trail_anchor (§15) --------------------------------------------
// Hand computations:
//   fade: 63 -> 55, 9 -> 1, 8 -> 0.
//   A one-pixel source at (100,100), after fade and two exact smoothers,
//   leaves its peak 13 at (99,97) and no energy down/right of the source.
//   Ring: push (10,20) then (30,40), so age 1 is (30,40). The ninth push
//   evicts the oldest entry while length remains eight.
void test_trail_anchor() {
  check(star::trail_fade(63) == 55 && star::trail_fade(9) == 1 && star::trail_fade(8) == 0,
        "subtract-8 six-bit fade saturates exactly");

  star::TrailHistory t;
  star::trail_push(t, 10, 20);
  star::trail_push(t, 30, 40);
  uint16_t x, y;
  star::trail_at(t, 1, x, y);
  check(x == 30 && y == 40, "age 1 is the newest past position");
  star::trail_at(t, 2, x, y);
  check(x == 10 && y == 20, "age 2 is the older position");
  check(t.length == 2, "length counts valid trail entries");
  for (uint32_t k = 0; k < 6; ++k) star::trail_push(t, 50 + k, 60 + k);
  check(t.length == star::kTrailN && t.head == 0, "eight pushes fill the ring and wrap head");
  star::trail_push(t, 90, 90);
  star::trail_at(t, 1, x, y);
  check(x == 90 && y == 90, "the ninth push is newest");
  star::trail_at(t, star::kTrailN, x, y);
  check(x == 30 && y == 40, "the ninth push evicts only the oldest entry");
  check(t.length == star::kTrailN, "trail length saturates at eight");

  const uint32_t W = 384, H = 240;
  uint8_t gray[64][3];
  for (uint32_t i = 0; i < 64; ++i) gray[i][0] = gray[i][1] = gray[i][2] = static_cast<uint8_t>(i);
  const auto sample = [&](const std::vector<uint8_t>& rgb, uint32_t sx, uint32_t sy) {
    return rgb[(sy * W + sx) * 3];
  };

  // Exact asymmetric-kernel anchor using one six-bit source texel.
  star::Sprite8 impulse;
  impulse.w = impulse.h = 128;
  impulse.pix.assign(128 * 128, 0);
  impulse.pix[64 * 128 + 64] = 63;
  star::TrailHistory ti;
  star::trail_push(ti, 100, 100);
  star::ComposeLight I;
  I.x_px = 200;
  I.y_px = 100;
  I.ramp = gray;
  I.corona = &impulse;
  I.trail = &ti;
  I.ghost_r_px = 64;
  std::vector<uint8_t> irgb(W * H * 3, 0);
  std::vector<int32_t> idepth(W * H, 0);
  star::FlareSlots islots;
  star::compose_view(irgb.data(), idepth.data(), W, H, 0, 0, W, H, 0, &I, 1, nullptr, 0, islots,
                     nullptr);
  check(sample(irgb, 99, 97) == 13,
        "one source texel peaks at 13 after subtract-8 and two smoothers");
  check(sample(irgb, 101, 97) == 0 && sample(irgb, 99, 99) == 0,
        "diffusion moves up and left, never down or right");

  // Graded connected trail. The age-g centre moves about (-g,-3g) under two
  // source smoothers per step, so the probes follow that asymmetric axis.
  const star::Sprite8 corona = star::corona_sprite(5);
  star::TrailHistory tr;
  star::trail_push(tr, 80, 120);
  star::trail_push(tr, 92, 120);
  star::trail_push(tr, 104, 120);
  star::trail_push(tr, 116, 120);
  star::ComposeLight L;
  L.x_px = 128;
  L.y_px = 120;
  L.ramp = gray;
  L.corona = &corona;
  L.trail = &tr;
  L.ghost_r_px = 16;
  std::vector<uint8_t> rgb(W * H * 3, 0);
  std::vector<int32_t> depth(W * H, 0);
  star::FlareSlots slots;
  star::compose_view(rgb.data(), depth.data(), W, H, 0, 0, W, H, 0, &L, 1, nullptr, 0, slots,
                     nullptr);

  const uint8_t newest = sample(rgb, 115, 117);
  const uint8_t oldest = sample(rgb, 76, 108);
  check(newest > oldest && oldest > 0, "newest-to-oldest trail intensity decays");
  check(sample(rgb, 108, 116) > 0 && sample(rgb, 96, 113) > 0 && sample(rgb, 83, 110) > 0,
        "historical silhouettes join with lit continuity");
  const uint8_t shoulder = sample(rgb, 122, 117);
  check(shoulder > 0 && shoulder < newest && sample(rgb, 136, 117) == 0,
        "graded trail has off-axis falloff instead of a solid circular mask");

  std::vector<uint32_t> colours;
  colours.reserve(W * H);
  for (size_t p = 0; p < rgb.size(); p += 3)
    colours.push_back((static_cast<uint32_t>(rgb[p]) << 16) |
                      (static_cast<uint32_t>(rgb[p + 1]) << 8) | rgb[p + 2]);
  std::sort(colours.begin(), colours.end());
  colours.erase(std::unique(colours.begin(), colours.end()), colours.end());
  check(colours.size() <= 64, "trail composite performs one 64-entry class-ramp lookup");

  // A static light with a full ring emits no trail and remains byte-identical
  // to trails disabled. This also checks that old motion ages out by eviction.
  std::vector<uint8_t> a(W * H * 3, 0), b(W * H * 3, 0);
  std::vector<int32_t> da(W * H, 0), db(W * H, 0);
  star::TrailHistory ts;
  star::FlareSlots sa, sb;
  star::ComposeLight S;
  S.ramp = gray;
  S.corona = &corona;
  S.x_px = 192;
  S.y_px = 120;
  S.halo_r_px = 48;
  star::ComposeLight ST = S;
  ST.trail = &ts;
  ST.ghost_r_px = 16;
  for (uint32_t f = 0; f < 10; ++f) {
    std::fill(a.begin(), a.end(), 0);
    std::fill(b.begin(), b.end(), 0);
    std::fill(da.begin(), da.end(), 0);
    std::fill(db.begin(), db.end(), 0);
    star::compose_view(a.data(), da.data(), W, H, 0, 0, W, H, f, &S, 1, nullptr, 0, sa, nullptr);
    star::compose_view(b.data(), db.data(), W, H, 0, 0, W, H, f, &ST, 1, nullptr, 0, sb, nullptr);
    check(a == b, "a static light with trails enabled is byte-identical to trails off");
  }
  check(ts.length == star::kTrailN, "static history still obeys eight-sample eviction");
}

// ---- §13 star_trail_replay (§15: capture, not warm-up) ----------------------
void test_trail_replay() {
  const uint32_t W = 384, H = 240;
  int16_t pts[12];
  star::ramp_points(s00_identity(), pts);
  uint8_t ramp[64][3];
  star::ramp_build(pts, ramp);
  const star::Sprite8 corona = star::corona_sprite(5);

  const uint32_t kCut = 5, kKeep = 4;  // capture after frame 5, replay 4 more
  const uint32_t x0 = 60, step = 24, y0 = 120;

  // Run A: straight through; capture the chunk after frame kCut-1 and record
  // frames kCut..kCut+kKeep
  star::TrailHistory trA;
  star::FlareSlots slA;
  star::ComposeLight A;
  A.ramp = ramp;
  A.corona = &corona;
  A.trail = &trA;
  A.ghost_r_px = 20;
  A.y_px = y0;
  std::vector<std::vector<uint8_t>> ran;
  uint8_t buf[star::kCelestialStateBytes];
  bool captured = false;
  for (uint32_t f = 0; f < kCut + kKeep; ++f) {
    A.x_px = x0 + step * f;
    std::vector<uint8_t> rgb(W * H * 3, 0);
    std::vector<int32_t> depth(W * H, 0);
    star::compose_view(rgb.data(), depth.data(), W, H, 0, 0, W, H, f * 8, &A, 1, nullptr, 0, slA,
                       nullptr);
    if (f >= kCut) ran.push_back(rgb);
    if (f == kCut - 1) {  // the chunk the spec freezes, right after the frame
      star::CelestialState st;
      st.slots = slA;
      st.trails[0] = trA;
      star::celestial_state_serialize(st, buf);
      captured = true;
    }
  }
  check(captured, "replay capture taken after frame kCut-1");
  star::CelestialState rst;
  star::celestial_state_deserialize(buf, rst);

  // Run B: fresh process state, deserialized chunk, same authored lights
  star::ComposeLight B = A;
  B.trail = &rst.trails[0];
  for (uint32_t f = kCut; f < kCut + kKeep; ++f) {
    B.x_px = x0 + step * f;
    std::vector<uint8_t> rgb(W * H * 3, 0);
    std::vector<int32_t> depth(W * H, 0);
    star::compose_view(rgb.data(), depth.data(), W, H, 0, 0, W, H, f * 8, &B, 1, nullptr, 0,
                       rst.slots, nullptr);
    check(ran[f - kCut] == rgb, "replay from the captured chunk is byte-exact (frame)");
  }
}

// ---- §13 star_gamut_sheet: 12 classes × rungs, CRC-locked ------------------
// Regenerate by deleting the constant and rerunning once; any change to the
// bake/ramp/compose laws moves this and MUST be justified in the commit.
constexpr uint32_t kGamutSheetCrc = 0x87A069EDu;  // pinned 2026-08-16 (first
// render: 255-value bake lines per the spec's implementation clarification 2)

void test_gamut_sheet() {
  const uint32_t W = 256, H = 256;
  uint32_t crc = 0;
  for (int cls = 0; cls < 12; ++cls) {
    star::StarIdentity id = star::identity(cls * 17 + 1, cls * 5 + 2, cls * 3 + 3, 0xA5A5A5A5u);
    id.cls = static_cast<uint8_t>(cls);  // force the class; keep the seeds
    const star::StarClass& c = star::kGamut[cls];
    for (int ch = 0; ch < 3; ++ch) id.under6[ch] = cls == 9 ? id.under6[ch] : c.under6[ch];
    int16_t pts[12];
    star::ramp_points(id, pts);
    uint8_t ramp[64][3];
    star::ramp_build(pts, ramp);
    const star::Sprite8 face = star::starface(id.texture_seed, c.smooth);
    const star::Sprite8 corona = star::corona_sprite(5);

    // rungs: near disc / washed disc / corona-only / corona clamped Duo
    struct Case {
      int32_t disc_r, halo_r, halo_max;
      int64_t d;
    };
    const Case cases[4] = {{40, 100, star::kHaloRMaxZ60Px, 0},
                           {40, 100, star::kHaloRMaxZ60Px, 4LL * c.ray_milli},
                           {0, 60, star::kHaloRMaxZ60Px, 20LL * c.ray_milli},
                           {12, 300, star::kHaloRMaxDuoPx, 8LL * c.ray_milli}};
    for (const Case& k : cases) {
      std::vector<uint8_t> rgb(W * H * 3, 0);
      std::vector<int32_t> depth(W * H, 0);
      star::ComposeLight L;
      L.x_px = 128;
      L.y_px = 128;
      L.disc_r_px = k.disc_r;
      L.halo_r_px = k.halo_r;
      L.halo_r_max_px = k.halo_max;
      L.d_milli = k.d;
      L.r_milli = c.ray_milli;
      L.ramp = ramp;
      L.face = &face;
      L.corona = &corona;
      L.flare_mode = c.flare;
      L.spin_phase = 15000;  // pulsar duty ON for the sheet
      L.probe_x = 128;
      L.probe_y = 128;
      star::FlareSlots slots;
      slots.fade_ctr[0] = 15;
      star::compose_view(rgb.data(), depth.data(), W, H, 0, 0, W, H, 96, &L, 1, nullptr, 0, slots,
                         nullptr);
      crc = zhao_abi::zhao_crc32c(crc, rgb.data(), rgb.size());
    }
  }
  if (kGamutSheetCrc == 0) {
    std::printf("render_star: RECORD gamut sheet CRC 0x%08X\n", crc);
  } else {
    if (crc != kGamutSheetCrc)
      std::fprintf(stderr, "  gamut sheet CRC got %08X want %08X\n", crc, kGamutSheetCrc);
    check(crc == kGamutSheetCrc, "gamut sheet CRC (12 classes × 4 rungs, Duo clamp included)");
  }
}

}  // namespace

int main() {
  test_starfield_equivalence();  // §7: byte-exact BEFORE anything renders
  test_ramp_anchor();
  test_palette_boil();
  test_ramp_slew();
  test_corona_anchor();
  test_flare_ghosts();
  test_flare_laws();
  test_flare_sprites();
  test_glint_and_pulsar();
  test_starfield_rarity();
  test_lod_ladder();
  test_state_roundtrip();
  test_trail_anchor();
  test_trail_replay();
  test_identity();
  test_starface();
  test_compose();
  test_gamut_sheet();
  if (failures == 0) std::printf("render_star: all green\n");
  return failures == 0 ? 0 : 1;
}
