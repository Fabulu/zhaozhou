// terrain_shade_oracle.cpp — the terrain light law, before any RTL exists.
//
// ---------------------------------------------------------------------------
// WHY A TEST WITH NO DUT
// ---------------------------------------------------------------------------
// Owner brief 2026-09-03, the recommended order: put the terrain-light and
// normal-detail law into ZRef and LOOK at the island under a moving sun BEFORE
// fitting anything. The art law says the same thing from the other side —
// measurement belongs on the comparison side, and the shipped value is chosen
// by looking.
//
// So this exercises the law itself. It cannot tell anyone whether the terrain
// looks right; only the render can. What it CAN do is stop the law being wrong
// in the ways it was already wrong once, which is what the four cases below
// are for.
//
// The draft this replaces had a divider that returned zero for every realistic
// triangle. Nothing here would have caught that — it was an RTL fault — but
// the overflow and the rounding disagreement below were real oracle faults and
// these are the checks that pin them.
#include <cstdint>
#include <cstdio>

#include "zhao_sim.hpp"
#include "zref/zref_terrain_normalmap.hpp"
#include "zref/zref_terrain_shade.hpp"

using namespace zref::terrain;

int main() {
  // ---- 1: THE RAIL DOES NOT OVERFLOW ------------------------------------
  // A Q16.16 component at the fx16 rail squares to 2^62, and three of those is
  // 1.38e19 against int64's 9.22e18. The first version added them in int64 and
  // was undefined behaviour on input the contract says is reachable.
  {
    FaceNormal n{};
    n.x = 2147483647;
    n.y = 2147483647;
    n.z = 2147483647;
    const int64_t len = shade_length(n);
    // sqrt(3) * 2^31 ~= 3.72e9, and it must be positive and larger than any
    // single component: a wrap would come back small or negative.
    zhao::check(len > 3700000000LL && len < 3800000000LL,
                "the sum of squares at the fx16 rail does not overflow -- "
                "three components at 2^62 need unsigned 64, not signed",
                1, (len > 3700000000LL && len < 3800000000LL) ? 1 : 0);

    FaceNormal m{};
    m.x = -2147483647;
    m.y = -2147483647;
    m.z = -2147483647;
    zhao::check(shade_length(m) == len,
                "and the negative rail gives the same length", 1,
                shade_length(m) == len ? 1 : 0);
  }

  // ---- 2: ROUNDING IS ROUND-HALF-UP, INCLUDING NEGATIVES ----------------
  // qformats §3 is round-half-up. A shift FLOORS, so the two disagree on every
  // negative value -- which is exactly half of what a detail normal produces.
  {
    struct C { int64_t v; int sh; int64_t want; const char* why; };
    const C cases[] = {
        { 100, 1,  50, "exact" },
        { 101, 1,  51, "positive half rounds up" },
        {-100, 1, -50, "exact, negative" },
        {-101, 1, -50, "negative half rounds UP, toward zero -- a shift would "
                       "floor to -51" },
        {  -1, 1,   0, "and minus one half is zero, not minus one" },
    };
    int bad = 0;
    for (const C& c : cases) {
      const int64_t got = rshift_round(c.v, c.sh);
      if (got != c.want) {
        ++bad;
        std::printf("    rshift_round(%lld,%d) = %lld, wanted %lld (%s)\n",
                    (long long)c.v, c.sh, (long long)got, (long long)c.want,
                    c.why);
      }
    }
    zhao::check(bad == 0,
                "rounding is round-half-up at every sign -- a `>>` floors and "
                "disagrees on exactly the negative half of the detail term",
                0, bad);
  }

  // ---- 3: A FLAT FACE UNDER AN OVERHEAD SUN IS FULLY LIT -----------------
  // The case the draft RTL got wrong: its divider produced quotient bits 63..32
  // while the true quotient is under 2^15, so this returned ZERO and every
  // triangle shaded to ambient. The oracle must say 32767.
  {
    FaceNormal up{};
    up.x = 0;
    up.y = 65536;  // one world unit, Q16.16
    up.z = 0;
    const int base = shade_base(up, 0, 32767, 0);
    zhao::check(base > 32000,
                "a flat face under a sun straight overhead is fully lit -- the "
                "draft RTL returned 0 here and shaded the whole island to "
                "ambient",
                32767, base);

    // and facing away is negative, not clamped: the detail term is added to it
    const int away = shade_base(up, 0, -32767, 0);
    zhao::check(away < -32000,
                "a face turned away keeps its SIGN, because the per-fragment "
                "detail is added to this and a clamp would discard what the "
                "addition needs",
                -32767, away);
  }

  // ---- 4: A DEGENERATE TRIANGLE HAS NO DIRECTION TO BE LIT FROM ---------
  {
    FaceNormal zero{};
    zero.x = 0; zero.y = 0; zero.z = 0;
    zhao::check(shade_base(zero, 0, 32767, 0) == 0,
                "a degenerate triangle shades to ambient rather than dividing "
                "by zero",
                0, shade_base(zero, 0, 32767, 0));
  }

  // ---- 5: AMBIENT IS ADDED, NOT A FLOOR ---------------------------------
  // The draft made ambient a floor and argued for it. That re-legislated
  // `SetEnvironment 0x0311`, which carries ambient as a COLOUR beside the sun.
  {
    const uint8_t lit_no_amb  = shade_pack(32767, 0, 0);
    const uint8_t lit_amb     = shade_pack(16384, 0, 40);
    const uint8_t unlit_amb   = shade_pack(-32768, 0, 40);
    zhao::check(lit_no_amb == 255,
                "a fully lit face with no ambient saturates at unit8 255, "
                "which is the largest representable and not 1.0",
                255, lit_no_amb);
    zhao::check(unlit_amb == 40,
                "an unlit face receives exactly ambient -- the lit term is "
                "clamped at zero first, because a surface facing away gets no "
                "sun rather than negative sun that eats the ambient",
                40, unlit_amb);
    zhao::check(lit_amb > 40 + 100,
                "and a half-lit face is ambient PLUS its light, which is what "
                "an addend means", 1, lit_amb > 140 ? 1 : 0);
  }

  // ---- 6: THE DETAIL TERM, AND ITS DECLARED ZENITH FADE ------------------
  // d has no Y by construction, so under a sun at the zenith dot(d, L) is zero
  // and the relief fades out. Declared, not discovered -- and the reason the
  // look-gate is a MOVING sun rather than a still frame.
  {
    const DetailNormal d = normalmap_decode(0x2040);  // dx=0x40, dz=0x20
    const int low  = normalmap_detail(d, 32767, 0, 255);   // sun near horizon
    const int high = normalmap_detail(d, 0, 0, 255);       // sun at zenith
    zhao::check(low > 0 && high == 0,
                "detail responds to a low sun and fades to nothing at the "
                "zenith -- there is no Y component, on purpose, and that fade "
                "is the declared behaviour",
                1, (low > 0 && high == 0) ? 1 : 0);

    zhao::check(normalmap_detail(d, 32767, 0, 0) == 0,
                "and strength 0 is exactly no detail, so the cut seam is a "
                "bit-exact no-op",
                0, normalmap_detail(d, 32767, 0, 0));
  }

  // ---- 7: MULTIPLE SUNS SATURATE RATHER THAN WRAP -----------------------
  {
    const int bases[3]   = {32767, 32767, 32767};
    const int details[3] = {0, 0, 0};
    const uint8_t three = shade_pack_multi(bases, details, 3, 0);
    zhao::check(three == 255,
                "three suns on one face is brighter, never darker -- the "
                "accumulator saturates instead of wrapping",
                255, three);
  }

  return zhao::report_and_exit("terrain_shade_oracle");
}
