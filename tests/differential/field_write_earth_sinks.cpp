// field_write_earth_sinks.cpp — the three Earth sink composition laws.
//
// FIELD.WRITE.MATERIAL / NAV / HAZARD were declared in design/ops.yml with
// reference_function names that resolved to NOTHING. zref_fieldir.hpp said so
// in as many words and refused to add wrappers, because "a wrapper around
// nothing would resolve the ledger check while leaving the reference exactly as
// absent as before".
//
// The owner ruling of 2026-08-24 supplied the behaviour, so these are now laws
// with content. This suite pins the parts a plausible-looking implementation
// would get wrong -- each case is chosen because the OBVIOUS alternative is
// also defensible, and the ruling picked one.

#include <cstdint>
#include <cstdio>

#include "zhao_sim.hpp"
#include "zref/zref_fieldir.hpp"

using zhao::check;
namespace fi = zref::fieldir;

namespace {

constexpr int32_t kOne = 1 << 16;  // 1.0 in Q16.16

void test_material() {
  const fi::MaterialState authored{3, 4, 128};

  // No writes at all: authored layer E passes through untouched. The field is
  // LIVE composition, so an expired program leaves no trace.
  {
    const fi::MaterialState r = fi::compose_material(authored, nullptr, 0);
    check(r.mat_a == 3 && r.mat_b == 4 && r.weight == 128,
          "material: no writes leaves authored layer E untouched", 3, r.mat_a);
  }

  // LAST ENABLED WINS -- not first, and not "highest material id". The obvious
  // alternative (a precedence table in hardware) is exactly what the ruling
  // refused: priority belongs to command order, so software owns it.
  {
    fi::MaterialWrite w[3];
    w[0] = {true, {10, 11, 32}};
    w[1] = {true, {20, 21, 64}};
    w[2] = {false, {30, 31, 96}};  // disabled: must not win despite being last
    const fi::MaterialState r = fi::compose_material(authored, w, 3);
    check(r.mat_a == 20 && r.mat_b == 21 && r.weight == 64,
          "material: last ENABLED writer wins, disabled ones are skipped", 20, r.mat_a);
  }

  // A single disabled write is not a write.
  {
    fi::MaterialWrite w[1];
    w[0] = {false, {99, 98, 1}};
    const fi::MaterialState r = fi::compose_material(authored, w, 1);
    check(r.mat_a == 3, "material: a disabled write changes nothing", 3, r.mat_a);
  }
  std::printf("  WRITE.MATERIAL: last-enabled-wins from layer E\n");
}

void test_nav() {
  // Deltas ADD, they do not replace -- two slows should compound.
  {
    const int32_t d[2] = {kOne, kOne};
    check(fi::compose_nav(kOne, d, 2) == 3 * kOne, "nav: deltas accumulate rather than replace",
          3u * kOne, static_cast<uint64_t>(fi::compose_nav(kOne, d, 2)));
  }

  // A negative delta makes ground CHEAPER, which is legal.
  {
    const int32_t d[1] = {-kOne / 2};
    check(fi::compose_nav(2 * kOne, d, 1) == kOne + kOne / 2,
          "nav: a negative delta makes ground cheaper", 0, 0);
  }

  // COST FLOORS AT ZERO. It never goes negative, because a negative cost would
  // make a path attractive for being dangerous.
  {
    const int32_t d[1] = {-10 * kOne};
    check(fi::compose_nav(kOne, d, 1) == 0, "nav: cost floors at zero, never negative", 0,
          static_cast<uint64_t>(fi::compose_nav(kOne, d, 1)));
  }

  // Saturating, not wrapping: a runaway program must not turn an impassable
  // tile into a free one by overflowing.
  {
    const int32_t d[2] = {INT32_MAX, INT32_MAX};
    check(fi::compose_nav(INT32_MAX, d, 2) == INT32_MAX,
          "nav: accumulation saturates instead of wrapping", 0, 0);
  }
  std::printf("  WRITE.NAV: additive, saturating, floored at zero\n");
}

void test_hazard() {
  // MAX, NOT SUM. Two independent fields overlapping must not double the
  // damage merely because both were active -- that is an emergent rule nobody
  // chose. This is the single most important case in this file.
  {
    const int32_t s[2] = {kOne / 2, kOne / 2};
    const uint8_t r = fi::compose_hazard(0, s, 2);
    check(r == 128, "hazard: two half-severity fields give 128, NOT 255", 128, r);
  }

  // Zero is neutral: an inactive field contributes nothing.
  {
    const int32_t s[2] = {0, kOne / 4};
    check(fi::compose_hazard(0, s, 2) == 64, "hazard: zero is neutral", 64,
          fi::compose_hazard(0, s, 2));
  }

  // The authored value participates in the max, so a field cannot make a
  // dangerous place safe.
  {
    const int32_t s[1] = {kOne / 8};
    check(fi::compose_hazard(200, s, 1) == 200, "hazard: a weak field cannot lower authored danger",
          200, fi::compose_hazard(200, s, 1));
  }

  // Clamped to 1.0 and non-negative by law.
  {
    const int32_t s[2] = {4 * kOne, -kOne};
    check(fi::compose_hazard(0, s, 2) == 255, "hazard: clamps at 1.0", 255,
          fi::compose_hazard(0, s, 2));
    const int32_t neg[1] = {-kOne};
    check(fi::compose_hazard(0, neg, 1) == 0, "hazard: negative severity is floored to zero", 0,
          fi::compose_hazard(0, neg, 1));
  }
  std::printf("  WRITE.HAZARD: max-combined, clamped, zero-neutral\n");
}

}  // namespace

int main() {
  test_material();
  test_nav();
  test_hazard();
  return zhao::report_and_exit("field_write_earth_sinks");
}
