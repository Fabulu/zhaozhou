// terrain_shade_oracle.cpp — the terrain light law, checked against the law
// that already exists rather than against a second one.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE WAS REWRITTEN
// ---------------------------------------------------------------------------
// Its first version tested a `shade_base` that re-implemented the terrain
// light: its own restoring square root, its own divide, s1.15 output, sign
// preserved. All twelve checks passed, and the thing they were checking was a
// SECOND IMPLEMENTATION of a ratified law — `zref::render::shade_flat_tri_dir`,
// which `reference/src/zrender/internal.hpp` calls "The ONE flat-shade law" and
// which the golden captures pin.
//
// A passing test against a duplicate law is worse than no test. It says "RTL
// == oracle" while the oracle has quietly stopped meaning "what the captures
// already pin". The sibling header states the discipline plainly:
// zref_terrain_normals.hpp is "a THIN view onto an existing ratified law, not
// a second implementation of it."
//
// So the oracle now exposes only what the ratified function does not, and this
// file checks THOSE pieces plus the format agreement between them.
//
// ---------------------------------------------------------------------------
// THE FORMAT ERROR IT WOULD HAVE SHIPPED
// ---------------------------------------------------------------------------
// The first version assumed the light was s1.15. Reading the source, the
// renderer's light is Q16.16 — hand-normalised unit (1,2,1)/sqrt(6), 26758 /
// 53521 / 26758. That is a factor of two in the relief, and it would have
// looked like a tuning problem rather than a units problem.
#include <cstdint>
#include <cstdio>

#include "zhao_sim.hpp"
#include "zref/zref_terrain_normalmap.hpp"
#include "zref/zref_terrain_shade.hpp"

using namespace zref::terrain;

int main() {
  // ---- 1: THE LIGHT IS THE RENDERER'S, AND IT IS A UNIT VECTOR ----------
  // If these drift from reference/src/zrender/internal.hpp the hardware is
  // lit by a different sun than the reference, and every capture disagrees
  // for a reason no pixel diff explains.
  {
    const int64_t x = kShadeLightX, y = kShadeLightY, z = kShadeLightZ;
    const int64_t mag2 = x * x + y * y + z * z;
    const int64_t one = int64_t(kShadeOne) * kShadeOne;
    // |L|^2 within 0.2% of 1.0 in Q32.32 — it is hand-normalised, not exact.
    const int64_t err = (mag2 > one) ? (mag2 - one) : (one - mag2);
    zhao::check(err * 500 < one,
                "the key light is the renderer's own hand-normalised unit "
                "(1,2,1)/sqrt(6) in Q16.16 -- not s1.15, which the first "
                "version of this oracle assumed",
                1, (err * 500 < one) ? 1 : 0);
    // NOT exactly 2x. The source says "hand-normalized to Q16.16 (0.40825 ->
    // 26758, 0.81650 -> 53521)" -- two components rounded INDEPENDENTLY, so
    // 53521 is five off the 53516 that doubling 26758 would give. This check
    // asserted the exact relationship first and failed, which is the test
    // working: a plausible invariant that the constants do not actually have.
    // What is true, and what the hardware must reproduce, is the ratio within
    // rounding.
    const int64_t ratio_err = kShadeLightY - 2 * static_cast<int64_t>(kShadeLightX);
    zhao::check(ratio_err > -16 && ratio_err < 16,
                "and its Y is twice its X to within hand-rounding -- the two "
                "components were normalised independently, so the hardware "
                "must take these exact integers rather than derive one from "
                "the other",
                0, static_cast<int>(ratio_err));
  }

  // ---- 2: THE SQUARED NORM DOES NOT OVERFLOW AT THE RAIL ----------------
  // A Q16.16 component at the fx16 rail squares to 2^62; three of those reach
  // 1.38e19 against signed 64's 9.22e18. The ratified law accumulates in
  // uint64 and so must this. In RTL the same mistake is a silent wrap giving a
  // SMALL norm and therefore a huge, wrong shade.
  {
    FaceNormal n{};
    n.x = 2147483647; n.y = 2147483647; n.z = 2147483647;
    const uint64_t sq = shade_nmag2(n);
    // 3 * (2^31-1)^2 ~= 1.383e19, which only fits unsigned.
    zhao::check(sq > 13000000000000000000ull,
                "three components at the fx16 rail need UNSIGNED 64 -- signed "
                "accumulation is undefined behaviour here and a wrap in RTL",
                1, sq > 13000000000000000000ull ? 1 : 0);

    FaceNormal m{};
    m.x = -2147483647; m.y = -2147483647; m.z = -2147483647;
    zhao::check(shade_nmag2(m) == sq,
                "and the negative rail gives the same squared norm", 1,
                shade_nmag2(m) == sq ? 1 : 0);
  }

  // ---- 3: A DEGENERATE TRIANGLE IS RECOGNISED, NOT DIVIDED BY ------------
  // The ratified law returns 0 for zero area. The hardware must agree rather
  // than dividing by zero, and it must decide it from the same quantity.
  {
    FaceNormal zero{};
    zero.x = 0; zero.y = 0; zero.z = 0;
    zhao::check(shade_degenerate(zero),
                "a zero-area triangle is degenerate and shades to ambient, "
                "matching the ratified law's nmag2 == 0 guard",
                1, shade_degenerate(zero) ? 1 : 0);
    FaceNormal up{};
    up.x = 0; up.y = 65536; up.z = 0;
    zhao::check(!shade_degenerate(up),
                "and a real one is not", 0, shade_degenerate(up) ? 1 : 0);
  }

  // ---- 4: THE DOT PRODUCT AGREES WITH THE LAW'S SHAPE -------------------
  // dot(n, L) widened to __int128, exactly as the ratified law does. A flat
  // face under this light is dominated by the Y term.
  {
    FaceNormal up{};
    up.x = 0; up.y = 65536; up.z = 0;
    const __int128 d = shade_ndot(up, kShadeLightX, kShadeLightY, kShadeLightZ);
    const __int128 want = static_cast<__int128>(65536) * kShadeLightY;
    zhao::check(d == want,
                "dot(n, L) for a flat face is exactly its Y component times "
                "the light's Y, in the same Q16.16 pair the law uses",
                1, (d == want) ? 1 : 0);

    // A face turned away gives a NEGATIVE dot. The ratified law clamps that to
    // zero AFTER the divide -- which is why the detail term has to be added
    // before the clamp, and why TERRAIN.SHADE cannot simply call the existing
    // function and hand the result on.
    FaceNormal down{};
    down.x = 0; down.y = -65536; down.z = 0;
    zhao::check(shade_ndot(down, kShadeLightX, kShadeLightY, kShadeLightZ) < 0,
                "a face turned from the sun has a NEGATIVE dot before the "
                "clamp -- relief on it must still be able to catch light, so "
                "the detail is added before the clamp, not after",
                1, shade_ndot(down, kShadeLightX, kShadeLightY, kShadeLightZ) < 0 ? 1 : 0);
  }

  // ---- 5: ROUNDING IS ROUND-HALF-UP AT BOTH SIGNS ------------------------
  {
    struct C { int64_t v; int sh; int64_t want; const char* why; };
    const C cases[] = {
        { 100, 1,  50, "exact" },
        { 101, 1,  51, "positive half rounds up" },
        {-100, 1, -50, "exact, negative" },
        {-101, 1, -50, "negative half rounds UP -- a shift would floor to -51" },
        {  -1, 1,   0, "and minus one half is zero, not minus one" },
    };
    int bad = 0;
    for (const C& c : cases) {
      if (rshift_round(c.v, c.sh) != c.want) {
        ++bad;
        std::printf("    rshift_round(%lld,%d) wrong (%s)\n",
                    (long long)c.v, c.sh, c.why);
      }
    }
    zhao::check(bad == 0,
                "rounding is round-half-up at every sign -- qformats §3, one "
                "rounding per result, and a shift is not it",
                0, bad);
  }

  // ---- 6: THE DETAIL TERM IS IN THE BASE'S OWN FORMAT --------------------
  // The two are added before the clamp, so they must share a scale. The first
  // version had the detail in s1.15 against a base it believed was s1.15 --
  // both wrong, and consistently wrong, which is how a units error survives a
  // green test.
  {
    const DetailNormal d = normalmap_decode(0x2040);  // dx=0x40, dz=0x20
    const int32_t low = normalmap_detail(d, kShadeLightX, kShadeLightZ, 255);

    // A full-strength detail must be a visible fraction of full scale, and
    // must not swamp it: relief modulates the light, it does not replace it.
    zhao::check(low > 0 && low < kShadeOne / 2,
                "a full-strength detail term is a real fraction of Q16.16 full "
                "scale and does not swamp the base -- relief modulates the "
                "light rather than replacing it",
                1, (low > 0 && low < kShadeOne / 2) ? 1 : 0);

    // THE ZENITH FADE, declared rather than discovered: d has no Y component
    // by construction, so a sun straight overhead produces no relief at all.
    zhao::check(normalmap_detail(d, 0, 0, 255) == 0,
                "detail fades to nothing under a sun at the zenith -- there is "
                "no Y component, on purpose, which is why the look-gate is a "
                "MOVING sun and not a still frame",
                0, normalmap_detail(d, 0, 0, 255));

    // THE CUT SEAM: strength 0 is bit-exact nothing.
    zhao::check(normalmap_detail(d, kShadeLightX, kShadeLightZ, 0) == 0 &&
                    normalmap_is_noop(0),
                "and strength 0 is a bit-exact no-op, so cutting the detail "
                "organ changes nothing else",
                0, normalmap_detail(d, kShadeLightX, kShadeLightZ, 0));
  }

  return zhao::report_and_exit("terrain_shade_oracle");
}
