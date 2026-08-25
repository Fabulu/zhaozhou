// field_sinks_directed.cpp — the three Earth sinks, RTL against the oracle.
//
// WHY THIS FILE EXISTS. `tests/differential/field_write_earth_sinks.cpp` pins
// the three composition laws, but it drives the REFERENCE only — there was no
// DUT in it, because there was no RTL. It is unit evidence, not hardware
// evidence, and advancing FIELD.SEQ.EARTH on the strength of it would have been
// exactly the confusion the maturity ladder exists to prevent.
//
// `zhao_field_sinks.sv` is now that RTL, and this is its differential against
// `zref::fieldir::compose_material`, `compose_nav` and `compose_hazard`.
//
// The cases are chosen where the OBVIOUS alternative is also defensible:
// last-enabled rather than first or highest-id, MAX rather than SUM, and the
// one that is easiest to get wrong — nav saturates after EVERY delta but floors
// at zero only ONCE, on the way out, so an intermediate negative recovers.

#include "Vzhao_field_sinks.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_fieldir.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

namespace {

using zhao::check;
namespace fi = zref::fieldir;

constexpr int32_t kOne = 1 << 16;

struct Beat {
  bool mat_en = false;
  fi::MaterialState mat{};
  int32_t nav_delta = 0;
  int32_t haz_sev = 0;
};

struct Bench {
  Vzhao_field_sinks& dut;

  explicit Bench(Vzhao_field_sinks& d) : dut(d) {
    dut.rst_n = 0;
    dut.load_i = 0;
    dut.wr_valid_i = 0;
    zhao::tick(dut);
    dut.rst_n = 1;
    zhao::tick(dut);
  }

  void run(const fi::MaterialState& am, int32_t anav, uint8_t ahaz,
           const std::vector<Beat>& beats) {
    dut.load_i = 1;
    dut.auth_mat_a_i = am.mat_a;
    dut.auth_mat_b_i = am.mat_b;
    dut.auth_weight_i = am.weight;
    dut.auth_nav_i = anav;
    dut.auth_hazard_i = ahaz;
    dut.wr_valid_i = 0;
    zhao::tick(dut);
    dut.load_i = 0;

    for (const Beat& x : beats) {
      dut.wr_valid_i = 1;
      dut.wr_mat_en_i = x.mat_en ? 1 : 0;
      dut.wr_mat_a_i = x.mat.mat_a;
      dut.wr_mat_b_i = x.mat.mat_b;
      dut.wr_mat_weight_i = x.mat.weight;
      dut.wr_nav_delta_i = x.nav_delta;
      dut.wr_haz_sev_i = x.haz_sev;
      zhao::tick(dut);
    }
    dut.wr_valid_i = 0;
    dut.eval();
  }
};

// The oracle, driven from the same beat stream.
void oracle(const fi::MaterialState& am, int32_t anav, uint8_t ahaz,
            const std::vector<Beat>& beats, fi::MaterialState* om, int32_t* onav,
            uint8_t* ohaz) {
  std::vector<fi::MaterialWrite> mw;
  std::vector<int32_t> nd, hs;
  for (const Beat& x : beats) {
    mw.push_back({x.mat_en, x.mat});
    nd.push_back(x.nav_delta);
    hs.push_back(x.haz_sev);
  }
  *om = fi::compose_material(am, mw.data(), mw.size());
  *onav = fi::compose_nav(anav, nd.data(), nd.size());
  *ohaz = fi::compose_hazard(ahaz, hs.data(), hs.size());
}

void diff(Bench& b, const fi::MaterialState& am, int32_t anav, uint8_t ahaz,
          const std::vector<Beat>& beats, const char* name) {
  fi::MaterialState om;
  int32_t onav = 0;
  uint8_t ohaz = 0;
  oracle(am, anav, ahaz, beats, &om, &onav, &ohaz);
  b.run(am, anav, ahaz, beats);

  char nm[192];
  std::snprintf(nm, sizeof nm, "%s: mat_a", name);
  check(b.dut.mat_a_o == om.mat_a, nm, om.mat_a, b.dut.mat_a_o);
  std::snprintf(nm, sizeof nm, "%s: mat_b", name);
  check(b.dut.mat_b_o == om.mat_b, nm, om.mat_b, b.dut.mat_b_o);
  std::snprintf(nm, sizeof nm, "%s: weight", name);
  check(b.dut.weight_o == om.weight, nm, om.weight, b.dut.weight_o);
  std::snprintf(nm, sizeof nm, "%s: nav", name);
  check(static_cast<int32_t>(b.dut.nav_o) == onav, nm,
        static_cast<uint64_t>(static_cast<uint32_t>(onav)),
        static_cast<uint64_t>(b.dut.nav_o));
  std::snprintf(nm, sizeof nm, "%s: hazard", name);
  check(b.dut.hazard_o == ohaz, nm, ohaz, b.dut.hazard_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_field_sinks dut;
  Bench b(dut);

  const fi::MaterialState authored{3, 4, 128};

  // ---- 1. no writes: authored layer E passes through untouched ------------
  // The field is LIVE composition, so an expired program leaves no trace.
  diff(b, authored, kOne, 40, {}, "1.no writes");

  // ---- 2. MATERIAL: last ENABLED wins ------------------------------------
  // Not first, and not highest material id. A disabled write must not win
  // despite being last — the alternative, a precedence table in hardware, is
  // what the ruling refused: priority belongs to command order.
  {
    std::vector<Beat> v(3);
    v[0].mat_en = true;  v[0].mat = {10, 11, 32};
    v[1].mat_en = true;  v[1].mat = {20, 21, 64};
    v[2].mat_en = false; v[2].mat = {30, 31, 96};
    diff(b, authored, 0, 0, v, "2.last enabled wins, disabled skipped");
  }
  {
    std::vector<Beat> v(1);
    v[0].mat_en = false; v[0].mat = {99, 98, 1};
    diff(b, authored, 0, 0, v, "2b.a disabled write changes nothing");
  }

  // ---- 3. NAV: additive, saturating, floored at zero ----------------------
  {
    std::vector<Beat> v(2);
    v[0].nav_delta = kOne;
    v[1].nav_delta = kOne;
    diff(b, authored, kOne, 0, v, "3.deltas accumulate rather than replace");
  }
  {
    std::vector<Beat> v(1);
    v[0].nav_delta = -kOne / 2;
    diff(b, authored, 2 * kOne, 0, v, "3b.a negative delta makes ground cheaper");
  }
  {
    std::vector<Beat> v(1);
    v[0].nav_delta = -10 * kOne;
    diff(b, authored, kOne, 0, v, "3c.cost floors at zero, never negative");
  }
  {
    std::vector<Beat> v(2);
    v[0].nav_delta = INT32_MAX;
    v[1].nav_delta = INT32_MAX;
    diff(b, authored, INT32_MAX, 0, v, "3d.accumulation saturates instead of wrapping");
  }

  // THE CASE THAT SEPARATES TWO DEFENSIBLE IMPLEMENTATIONS. The reference
  // clamps to the int32 range after every delta but floors at zero only once,
  // on the way out. So a large negative followed by a large positive RECOVERS:
  // from 1.0, {-10.0, +20.0} is 11.0, not 20.0. Flooring per step would give
  // 20.0 and pass every other nav case in this file.
  {
    std::vector<Beat> v(2);
    v[0].nav_delta = -10 * kOne;
    v[1].nav_delta = 20 * kOne;
    diff(b, authored, kOne, 0, v, "3e.an intermediate negative RECOVERS, not floored per step");
  }
  // The same shape at the negative rail: saturation there is NOT recoverable,
  // because the accumulator genuinely clamped.
  {
    std::vector<Beat> v(2);
    v[0].nav_delta = INT32_MIN;
    v[1].nav_delta = INT32_MAX;
    diff(b, authored, 0, 0, v, "3f.saturation at the low rail is not recoverable");
  }

  // ---- 4. HAZARD: MAX, not SUM -------------------------------------------
  // The single most important case: two independent fields overlapping must
  // not double the damage merely because both were active.
  {
    std::vector<Beat> v(2);
    v[0].haz_sev = kOne / 2;
    v[1].haz_sev = kOne / 2;
    diff(b, authored, 0, 0, v, "4.two half-severity fields give 128, NOT 255");
  }
  {
    std::vector<Beat> v(2);
    v[0].haz_sev = 0;
    v[1].haz_sev = kOne / 4;
    diff(b, authored, 0, 0, v, "4b.zero is neutral");
  }
  {
    std::vector<Beat> v(1);
    v[0].haz_sev = kOne / 8;
    diff(b, authored, 0, 200, v, "4c.a weak field cannot lower authored danger");
  }
  {
    std::vector<Beat> v(2);
    v[0].haz_sev = 4 * kOne;
    v[1].haz_sev = -kOne;
    diff(b, authored, 0, 0, v, "4d.clamps at 1.0, negative floored");
  }
  // Rounding is round-half-up at the u8 conversion, so the half-step boundary
  // is where an off-by-one would live. Sweep every u8 target.
  for (int k = 0; k < 256; ++k) {
    std::vector<Beat> v(1);
    v[0].haz_sev = static_cast<int32_t>((static_cast<int64_t>(k) * kOne + 127) / 255);
    char nm[64];
    std::snprintf(nm, sizeof nm, "4e.u8 rounding at k=%d", k);
    diff(b, authored, 0, 0, v, nm);
  }

  // ---- 5. random streams --------------------------------------------------
  // The three layers are composed independently over one shared beat stream, so
  // a random lane is where a CROSS-LAYER leak would show: a nav delta writing
  // material, a hazard max clobbering nav, an enable bit shared by mistake.
  {
    std::mt19937 rng(0xEA27u);
    for (int t = 0; t < 4000; ++t) {
      const int n = static_cast<int>(rng() % 8);
      std::vector<Beat> v(n);
      for (Beat& x : v) {
        x.mat_en = (rng() & 3) != 0;
        x.mat = {static_cast<uint8_t>(rng()), static_cast<uint8_t>(rng()),
                 static_cast<uint8_t>(rng())};
        x.nav_delta = static_cast<int32_t>(rng());
        // Mix full-range severities with in-band ones so the rounding is
        // exercised and not only the clamp.
        x.haz_sev = (rng() & 1) ? static_cast<int32_t>(rng())
                                : static_cast<int32_t>(rng() % (kOne + 1));
      }
      const fi::MaterialState am{static_cast<uint8_t>(rng()), static_cast<uint8_t>(rng()),
                                 static_cast<uint8_t>(rng())};
      char nm[64];
      std::snprintf(nm, sizeof nm, "5.random stream %d (n=%d)", t, n);
      diff(b, am, static_cast<int32_t>(rng()), static_cast<uint8_t>(rng()), v, nm);
    }
  }

  dut.final();
  return zhao::report_and_exit("field_sinks_directed");
}
