// geom_meshfetch_directed.cpp — GEOM.MESHFETCH's oracle, at the boundaries.
//
// ---------------------------------------------------------------------------
// ORACLE ONLY, AND DELIBERATELY SO
// ---------------------------------------------------------------------------
// `zhao_geom_meshfetch.sv` does not exist. `fpga/rtl/geometry/` holds twenty
// files and that is not one of them, which is why `tools/design/compose_order.py`
// puts this block at position 1 of the declared geometry order with nothing
// built behind it. Maturity advances one rung at a time, so this commit is
// REFERENCE_COMPLETE: when the RTL lands it gains a DUT in this same file and
// these same cases become the differential.
//
// ---------------------------------------------------------------------------
// WHAT IS WORTH TESTING, per the contract, is the BOUNDARIES
// ---------------------------------------------------------------------------
// "The cases that matter are the boundaries, not the happy path." Every case
// below is named in the contract, and two of them are there because they are
// the ones a plausible implementation gets wrong:
//
//   * REJECTED IS NOT REFUSED. One is geometry that is not visible; the other
//     is a descriptor that is not trustworthy. The counters must not conflate
//     them, so the test asserts the pair, not just the boolean.
//   * THE BOUND INEQUALITY, not a bound value. Under non-uniform scale the
//     world radius must be >= the exact transformed bound, because a loose
//     bound costs decode work and a tight one DELETES GEOMETRY. Asserting a
//     number would pin the rounding; asserting the inequality pins the
//     DIRECTION, which is what the ruling actually chose.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "zhao_sim.hpp"
#include "zref/zref_meshfetch.hpp"

namespace mf = zref::meshfetch;
namespace zc = zref::cull;

namespace {

constexpr int32_t ONE = 65536;
constexpr uint8_t kFormat = 1;
constexpr uint16_t kGeneration = 0x2A2A;

// A perspective view-projection built in fx16 the way the renderer would.
// Same construction as tests/differential/geom_cull_directed.cpp -- duplicated
// rather than shared because it is three lines of test scaffolding and a
// shared header between two directed tests would couple them.
zref::mat4fx make_vp(double fov_deg, double aspect, double eye_z) {
  const double f = 1.0 / std::tan(fov_deg * 3.14159265358979 / 360.0);
  zref::mat4fx m{};
  auto set = [&](int i, int j, double v) {
    double s = v * 65536.0;
    if (s > 2147483647.0) s = 2147483647.0;
    if (s < -2147483648.0) s = -2147483648.0;
    m.m[i][j] = zref::fx16{static_cast<int32_t>(s)};
  };
  set(0, 0, f / aspect);
  set(1, 1, f);
  set(2, 2, 1.0);
  set(3, 2, -1.0);  // w = eye_z - z, so w > 0 in front of the eye
  set(3, 3, eye_z);
  return m;
}

void wr16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}
void wr32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

// A descriptor that passes every check, so a case can break exactly one thing.
// The CRC is computed LAST and over bytes 0..59, which is the frozen window --
// a helper that stamped it earlier would silently validate a stale byte.
struct Desc {
  uint8_t b[mf::kDescBytes];

  Desc() {
    std::memset(b, 0, sizeof b);
    b[0] = kFormat;
    b[1] = 0;
    b[2] = 32;  // vertex_count
    b[3] = 40;  // triangle_count
    wr16(b + 4, 7);
    wr16(b + 6, 0);
    wr32(b + 8, 0);   // bound_centre, meshlet-local
    wr32(b + 12, 0);
    wr32(b + 16, 0);
    wr32(b + 20, static_cast<uint32_t>(2 * ONE));  // bound_radius = 2.0
    wr32(b + 24, 0);
    wr32(b + 28, 0);
    wr16(b + 32, kGeneration);
    wr16(b + 34, 3);
    stamp();
  }
  void stamp() { wr32(b + mf::kCrcOff, zhao_abi::zhao_crc32c(0, b, mf::kCrcCovered)); }
};

mf::InstanceXform identity_xform() {
  mf::InstanceXform x{};
  x.m[0] = ONE;
  x.m[5] = ONE;
  x.m[10] = ONE;
  return x;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  const zref::mat4fx front = make_vp(60.0, 4.0 / 3.0, 40.0);
  zref::mat4fx back = make_vp(60.0, 4.0 / 3.0, 40.0);
  // Flip the w row: this camera looks the other way, so w > 0 BEHIND. That is
  // what makes a genuine "in one eye and not the other" case rather than two
  // cameras that agree about everything.
  back.m[3][2] = zref::fx16{ONE};
  back.m[3][3] = zref::fx16{40 * ONE};

  zc::View views[zc::kViewCount];
  views[0] = zc::make_view(front);
  views[1] = zc::make_view(back);

  const mf::InstanceXform ident = identity_xform();

  // ---- 1: a clean descriptor is accepted, and the counts at their limits ---
  {
    Desc d;
    zhao::check(mf::validate(d.b, kFormat, kGeneration) == mf::Refusal::kNone,
                "a clean descriptor passes validation", 1, 1);

    int bad = 0;
    for (int i = 0; i < 2; ++i) {
      Desc a;
      a.b[2] = mf::kMaxVertexCount;  // 64 -- AT the limit, legal
      a.b[3] = mf::kMaxTriangleCount;
      a.stamp();
      if (mf::validate(a.b, kFormat, kGeneration) != mf::Refusal::kNone) ++bad;
    }
    zhao::check(bad == 0,
                "vertex_count 64 and triangle_count 126 are ACCEPTED -- the "
                "limits are inclusive, and an off-by-one here silently deletes "
                "the densest meshlets the compiler emits",
                0, bad);

    Desc v65;
    v65.b[2] = 65;
    v65.stamp();
    zhao::check(mf::validate(v65.b, kFormat, kGeneration) == mf::Refusal::kVertexCount,
                "vertex_count 65 is refused, and refused for the RIGHT reason -- "
                "a u8 local index cannot address it", 1, 1);

    Desc t127;
    t127.b[3] = 127;
    t127.stamp();
    zhao::check(mf::validate(t127.b, kFormat, kGeneration) == mf::Refusal::kTriangleCount,
                "triangle_count 127 is refused", 1, 1);
  }

  // ---- 2: one bit flipped in each CRC-covered byte -> 60 refusals ----------
  // The contract asks for exactly this and for NO acceptances. A CRC that
  // covered the wrong window would still pass a happy-path test.
  {
    int refused = 0, accepted = 0, wrong_reason = 0;
    for (int i = 0; i < mf::kCrcCovered; ++i) {
      Desc d;
      d.b[i] ^= 0x01;  // corrupt AFTER the CRC was stamped
      const mf::Refusal r = mf::validate(d.b, kFormat, kGeneration);
      if (r == mf::Refusal::kNone) {
        ++accepted;
      } else {
        ++refused;
        // Byte 0 is format_id, which is checked BEFORE the CRC on purpose: an
        // unknown format means the layout itself is not agreed, so the CRC
        // window may be in the wrong place. Every other byte must be a CRC
        // refusal.
        if (i != 0 && r != mf::Refusal::kCrc) ++wrong_reason;
      }
    }
    zhao::check(refused == mf::kCrcCovered && accepted == 0,
                "a single flipped bit in ANY of the 60 covered bytes is refused, "
                "and none is accepted", mf::kCrcCovered, refused);
    zhao::check(wrong_reason == 0,
                "and every one of them except byte 0 refuses as kCrc -- byte 0 is "
                "format_id, checked first because an unknown format means the "
                "CRC window itself may be misplaced",
                0, wrong_reason);
  }

  // ---- 3: every reserved byte, nonzero in turn -> 24 refusals -------------
  {
    int refused = 0, wrong_reason = 0;
    for (int i = 0; i < mf::kReservedLen; ++i) {
      Desc d;
      d.b[mf::kReservedOff + i] = 0xA5;
      d.stamp();  // a VALID descriptor that merely uses a future field
      const mf::Refusal r = mf::validate(d.b, kFormat, kGeneration);
      if (r != mf::Refusal::kNone) ++refused;
      if (r != mf::Refusal::kReserved) ++wrong_reason;
    }
    zhao::check(refused == mf::kReservedLen && wrong_reason == 0,
                "each of the 24 reserved bytes refuses as kReserved when set -- "
                "the CRC is CORRECT in these cases, so this catches an older "
                "reader consuming a future field rather than corruption",
                mf::kReservedLen, refused);
  }

  // ---- 4: generation off by one, and a zero bound -------------------------
  {
    Desc d;
    zhao::check(mf::validate(d.b, kFormat, kGeneration + 1) == mf::Refusal::kGeneration,
                "generation off by one is refused -- the asset moved under a "
                "live instance", 1, 1);

    Desc z;
    wr32(z.b + 20, 0);
    z.stamp();
    zhao::check(mf::validate(z.b, kFormat, kGeneration) == mf::Refusal::kZeroBound,
                "bound_radius 0 on a non-empty meshlet is refused -- a zero "
                "bound culls everything, silently", 1, 1);

    Desc e;
    wr32(e.b + 20, 0);
    e.b[3] = 0;  // an EMPTY meshlet has nothing to bound
    e.stamp();
    zhao::check(mf::validate(e.b, kFormat, kGeneration) == mf::Refusal::kNone,
                "but a zero bound on an EMPTY meshlet is legal -- refusing it "
                "would turn a degenerate case into a fault", 1, 1);

    Desc f;
    f.b[0] = 0xEE;
    f.stamp();
    zhao::check(mf::validate(f.b, kFormat, kGeneration) == mf::Refusal::kFormat,
                "an unknown format_id is refused, never guessed", 1, 1);
  }

  // ---- 5: DUO — in one eye and not the other ------------------------------
  // The contract calls this "the single most valuable case in the file".
  {
    Desc d;
    // z = +100, and the SIGN is the whole case. `front` has w = eye_z - z, so
    // at z = +100 its w is -60 and the sphere is behind that eye. `back` has
    // the w row flipped, so its w is +140 and it sees the sphere. Writing -100
    // here -- which I did first -- puts the sphere in FRONT of camera 0 and the
    // case silently becomes 0b01: still a pass for a naive assertion, and the
    // exact opposite of the Duo property it is supposed to pin.
    wr32(d.b + 16, static_cast<uint32_t>(100 * ONE));  // bound_centre.z
    d.stamp();

    const mf::Result r = mf::decide(d.b, kFormat, kGeneration, ident, views, 0b11);
    zhao::check(r.accepted && r.visible_mask == 0b10,
                "a meshlet outside camera 0 and inside camera 1 is ACCEPTED with "
                "visible_mask 0b10 -- a block that emitted one mask for both eyes "
                "would give the second eye the first eye's geometry",
                0b10, r.visible_mask);
    zhao::check(r.refusal == mf::Refusal::kNone,
                "and it is not a refusal: the descriptor was fine", 0,
                static_cast<int>(r.refusal));
  }

  // ---- 6: REJECTED IS NOT REFUSED ----------------------------------------
  {
    Desc d;
    // Far off to the side of both cameras.
    // 10,000 units, not 100,000: the latter overflows int32 at fx16 and the
    // compiler says so. A wrong-signed centre would have put the sphere back
    // inside a frustum and turned this case into a silent pass.
    wr32(d.b + 8, static_cast<uint32_t>(10000 * ONE));  // bound_centre.x
    d.stamp();

    const mf::Result r = mf::decide(d.b, kFormat, kGeneration, ident, views, 0b11);
    zhao::check(!r.accepted && r.visible_mask == 0,
                "geometry outside every camera is not accepted", 0, r.visible_mask);
    zhao::check(r.refusal == mf::Refusal::kNone,
                "and it is REJECTED, not REFUSED -- one is geometry that is not "
                "visible, the other is a descriptor that is not trustworthy, and "
                "a counter that conflated them would report corruption every time "
                "the camera turned around",
                0, static_cast<int>(r.refusal));

    // With no active camera the verdict is reject, and that is correct rather
    // than a special case: nothing is drawn, so rejecting deletes nothing.
    Desc c;
    const mf::Result none = mf::decide(c.b, kFormat, kGeneration, ident, views, 0b00);
    zhao::check(!none.accepted && none.refusal == mf::Refusal::kNone,
                "active_camera_mask 0 rejects without refusing", 1, 1);
  }

  // ---- 7: the bound INEQUALITY under non-uniform scale --------------------
  // Assert the direction, not a value. Maximum-absolute scale rounds the bound
  // OUTWARD, and that is the ruling's choice: a loose bound costs decode work,
  // a tight one deletes geometry.
  {
    mf::InstanceXform x{};
    x.m[0] = 3 * ONE;  // x scaled 3x
    x.m[5] = ONE;      // y unscaled
    x.m[10] = 2 * ONE; // z scaled 2x

    const int32_t local_c[3] = {ONE, 2 * ONE, -ONE};
    const uint32_t local_r = static_cast<uint32_t>(5 * ONE);

    int32_t wc[3];
    uint32_t wr;
    mf::world_bound(x, local_c, local_r, wc, &wr);

    // The exact transformed bound along each axis is |scale_axis| * r. The
    // world radius must be >= the largest of them, or geometry disappears.
    const int64_t need = static_cast<int64_t>(3) * local_r;  // the 3x axis
    zhao::check(static_cast<int64_t>(wr) >= need,
                "the world radius is at least the exact transformed bound on "
                "every axis -- maximum-ABSOLUTE scale rounds outward, and the "
                "direction is the point rather than the value",
                1, (static_cast<int64_t>(wr) >= need) ? 1 : 0);

    zhao::check(wc[0] == 3 * ONE && wc[1] == 2 * ONE && wc[2] == -2 * ONE,
                "and the centre is transformed exactly, not bounded", 1,
                (wc[0] == 3 * ONE && wc[1] == 2 * ONE && wc[2] == -2 * ONE) ? 1 : 0);
  }

  // ---- 8: a refused descriptor never reports visibility -------------------
  // The ordering property: validation runs first, so a corrupt descriptor
  // cannot produce a mask that looks like a real answer.
  {
    Desc d;
    d.b[10] ^= 0x80;  // corrupt a bound byte, CRC not restamped
    const mf::Result r = mf::decide(d.b, kFormat, kGeneration, ident, views, 0b11);
    zhao::check(r.refusal == mf::Refusal::kCrc && !r.accepted && r.visible_mask == 0,
                "a refused descriptor emits no meshlet and no visibility -- it is "
                "not trustworthy in ANY field, including the bound the cull would "
                "have used",
                0, r.visible_mask);
  }

  return zhao::report_and_exit("geom_meshfetch_directed");
}
