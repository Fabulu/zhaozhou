// attribute_plane_equivalence.cpp — is a stepped plane form bit-identical to
// the shipped attribute-interpolation oracle?
//
// THE QUESTION. reports/ATTRIBUTE_INTERPOLATION_LAW.md found that
// reference/src/zrender/rast.cpp interpolates every attribute as
//
//     attr(x,y) = round_half_up( (w0*A + w1*B + w2*C) / area )
//
// with a 128-bit numerator and a divide PER ATTRIBUTE PER PIXEL, while
// spec/qformats.md says "interpolate by plane equation". Those read as different
// laws, and if they are, GEOM.ATTRSETUP cannot be differentially tested against
// zref and its output format is an open spec decision.
//
// THE CLAIM THIS FILE TESTS. They are the same law, because the NUMERATOR is
// itself an exact plane. The edge functions step by constants
// (`w0 += dw0_dx` in rast.cpp), so
//
//     N(x,y) = w0(x,y)*A + w1(x,y)*B + w2(x,y)*C
//
// is linear in x and y with INTEGER increments, and therefore
//
//     N(x,y) = N0 + x*dNdx + y*dNdy      exactly, in integers
//
// If that holds, a setup stage may emit {N0, dNdx, dNdy, area}, a raster stage
// may step the numerator with ADDS, and one divide per attribute per pixel
// reproduces the oracle bit for bit. The expensive half is the divide; the plane
// half is free and exact.
//
// WHY IT MATTERS EITHER WAY. If the claim holds, step 6 of
// reports/RENDERER_ARCHITECTURE.md is unblocked and ATTRSETUP has a testable
// output format. If it fails, the law has to change and every golden capture
// CRC moves with it. That is worth an afternoon before writing any RTL.
//
//   g++ -std=c++17 -O2 tests/proofs/attribute_plane_equivalence.cpp -o attrplane

#include <cstdint>
#include <cstdio>

namespace {

// rast.cpp's rounding, restated: round-half-up on the QUOTIENT, not the
// numerator. Restated rather than included because the point is to compare two
// independent statements of the law.
int64_t div_rhu(__int128 n, int64_t d) {
  // area is > 0 for a winding-normalised triangle.
  const __int128 dd = d;
  const __int128 q = (n >= 0) ? ((2 * n + dd) / (2 * dd)) : -((-2 * n + dd) / (2 * dd));
  return static_cast<int64_t>(q);
}

// The oracle's edge function, at a pixel centre in the same 1/256 units the
// raster uses.
int64_t orient(int64_t ax, int64_t ay, int64_t bx, int64_t by, int64_t px, int64_t py) {
  return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

struct Tri {
  int64_t ax, ay, bx, by, cx, cy;
  int64_t va, vb, vc;  // one attribute at the three vertices
};

}  // namespace

int main() {
  // A spread of shapes: thin, fat, mirrored, and with attribute values across
  // the S 8.24 range the spec gives u_over_w and v_over_w.
  const Tri tris[] = {
      {0, 0, 4096, 0, 0, 4096, 0, 1 << 24, -(1 << 24)},
      {256, 512, 9000, 700, 300, 6000, 12345, -98765, 4242424},
      {-2048, -1024, 5000, 33, 77, 4096, 1 << 30, -(1 << 30), 7},
      {100, 100, 120, 8000, 9000, 110, -1, 1, 0},
      {0, 0, 65535, 1, 1, 65535, 1 << 23, 1 << 23, 1 << 23},
  };

  long checked = 0, bad = 0;
  for (const Tri& t : tris) {
    const int64_t area = orient(t.ax, t.ay, t.bx, t.by, t.cx, t.cy);
    if (area == 0) continue;
    // Winding-normalised, as GEOM.CLIP guarantees before SETUP.
    Tri n = t;
    if (area < 0) {
      n.bx = t.cx;
      n.by = t.cy;
      n.cx = t.bx;
      n.cy = t.by;
      n.vb = t.vc;
      n.vc = t.vb;
    }
    const int64_t A = orient(n.ax, n.ay, n.bx, n.by, n.cx, n.cy);
    if (A <= 0) continue;

    // ---- the plane the SETUP stage would emit ---------------------------
    // Numerator at the origin, and its two exact integer increments.
    auto num_at = [&](int64_t px, int64_t py) -> __int128 {
      const int64_t w0 = orient(n.bx, n.by, n.cx, n.cy, px, py);
      const int64_t w1 = orient(n.cx, n.cy, n.ax, n.ay, px, py);
      const int64_t w2 = orient(n.ax, n.ay, n.bx, n.by, px, py);
      return static_cast<__int128>(w0) * n.va + static_cast<__int128>(w1) * n.vb +
             static_cast<__int128>(w2) * n.vc;
    };
    const __int128 N0 = num_at(0, 0);
    const __int128 dNdx = num_at(1, 0) - N0;
    const __int128 dNdy = num_at(0, 1) - N0;

    // ---- compare, over a patch of the plane ------------------------------
    for (int64_t y = -40; y <= 40; ++y)
      for (int64_t x = -40; x <= 40; ++x) {
        const int64_t want = div_rhu(num_at(x, y), A);      // the oracle
        const __int128 stepped = N0 + dNdx * x + dNdy * y;  // the plane
        const int64_t got = div_rhu(stepped, A);
        if (got != want) {
          if (bad < 5)
            std::printf("  MISMATCH at (%lld,%lld): oracle %lld, plane %lld\n",
                        static_cast<long long>(x), static_cast<long long>(y),
                        static_cast<long long>(want), static_cast<long long>(got));
          ++bad;
        }
        ++checked;
      }
  }

  std::printf("attribute plane equivalence: %ld pixel-attributes checked, %ld mismatches\n",
              checked, bad);
  if (bad == 0)
    std::printf(
        "  The NUMERATOR is an exact integer plane, so {N0, dNdx, dNdy, area} plus ONE\n"
        "  divide per attribute per pixel reproduces the oracle bit for bit. GEOM.ATTRSETUP\n"
        "  has a testable output format; the divide is the cost, and the plane is free.\n");
  return bad != 0;
}
