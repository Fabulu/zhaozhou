// geom_fogfactor_directed.cpp
//
// ---------------------------------------------------------------------------
// THE PER-VERTEX FOG FACTOR, AGAINST THE ORACLE
// ---------------------------------------------------------------------------
// `zhao_geom_fogfactor` implements the half of owner ruling D-5 that lives in
// geometry: "the fog factor is computed once per vertex from the frozen
// view/fog law". `reference/include/zref/zref_fog.hpp` implements the same law.
//
// This test compares against THAT, not against §8's prose. Re-deriving the
// arithmetic from the spec would produce a second implementation, and two
// arithmetics that agree until they do not is the defect class this repository
// names by hand.
#include "Vzhao_geom_fogfactor.h"

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "zref/zref_fixp.hpp"
#include "zref/zref_fog.hpp"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vzhao_geom_fogfactor* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

}  // namespace

int main() {
  Vzhao_geom_fogfactor* d = new Vzhao_geom_fogfactor;
  zref::SatLedger led;

  d->rst_n = 0;
  d->cfg_en_i = 0;
  d->cfg_far_i = 0;
  d->cfg_near_i = 0;
  d->cfg_k_i = 0;
  d->v_valid_i = 0;
  d->behind_i = 0;
  d->r_ready_i = 1;
  d->w_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  // A realistic frame: fog from 20 m to 300 m.
  const zref::fx16 kNear{20 << 16};
  const zref::fx16 kFar{300 << 16};
  const zref::fx16 kK = zref::fog::frame_k(kNear, kFar, &led);

  d->cfg_en_i = 1;
  d->cfg_near_i = kNear.raw;
  d->cfg_far_i = kFar.raw;
  d->cfg_k_i = kK.raw;

  // ---- the differential ----------------------------------------------------
  // Sweep w across and BEYOND the fog band, so both clamps are exercised: well
  // inside near (clear), across the ramp, and past far (fully fogged).
  int compared = 0, mismatches = 0;
  int saw_clear = 0, saw_opaque = 0, saw_middle = 0;
  // STRIDE MATTERS, and the first version of this sweep got it wrong. Stepping w
  // at exact metre boundaries (m << 16) leaves the product's low 16 bits in a
  // narrow, aligned set, and the round-half-up term never changes an outcome --
  // so DELETING THE ROUNDING ENTIRELY still produced 0 mismatches. The sweep
  // was measuring the clamp and not the arithmetic. A sub-metre stride that is
  // not a power of two exercises the low bits properly, and the mutation now
  // fails as it should.
  for (int32_t m = 0; m <= 400; ++m) {
    const int32_t w_raw = m * 65536 + m * 1237;  // deliberately unaligned
    d->v_valid_i = 1;
    d->behind_i = 0;
    d->w_i = static_cast<uint32_t>(w_raw) & 0x7FFFFFFFu;
    d->tag_i = static_cast<uint16_t>(m);
    d->eval();
    const int32_t expect =
        zref::fog::vertex_factor(zref::fx16{w_raw}, kNear, kFar, kK, &led);
    tick(d);
    d->eval();
    if (d->r_valid_o) {
      ++compared;
      if (d->fogf_o != expect) ++mismatches;
      if (expect == 0x10000) ++saw_clear;
      else if (expect == 0) ++saw_opaque;
      else ++saw_middle;
    }
  }
  d->v_valid_i = 0;
  d->eval();

  std::printf("  differential: %d vertices, %d mismatches (clear %d, ramp %d, opaque %d)\n",
              compared, mismatches, saw_clear, saw_middle, saw_opaque);

  zhao::check(compared > 300, "the sweep actually ran", 1, compared > 300 ? 1 : 0);
  zhao::check(mismatches == 0,
              "RTL per-vertex fog factor is BIT-IDENTICAL to zref_fog's frozen "
              "law across the whole sweep",
              0, static_cast<uint64_t>(mismatches));

  // Non-vacuity in all three regions. Without this the differential could pass
  // on a sweep that never left one clamp -- a constant matching a constant.
  zhao::check(saw_clear > 0 && saw_opaque > 0 && saw_middle > 0,
              "the sweep covered CLEAR, the ramp and FULLY FOGGED -- both "
              "clamps and the interpolating region, not one flat rail",
              1, (saw_clear > 0 && saw_opaque > 0 && saw_middle > 0) ? 1 : 0);

  // ---- polarity, stated as its own assertion -------------------------------
  // This is the check that catches an inverted implementation, which is the
  // specific mistake §8's superseded mix formula invites.
  {
    d->v_valid_i = 1;
    d->behind_i = 0;
    d->w_i = 5 << 16;  // well inside near
    d->eval();
    tick(d);
    d->eval();
    zhao::check(d->fogf_o == 0x10000,
                "a vertex NEARER than fog_near is CLEAR (0x10000), not fogged -- "
                "§8's surviving polarity",
                0x10000, static_cast<uint64_t>(d->fogf_o));
    d->w_i = 380 << 16;  // well past far
    d->eval();
    tick(d);
    d->eval();
    zhao::check(d->fogf_o == 0,
                "and a vertex BEYOND fog_far is fully fogged (0)", 0,
                static_cast<uint64_t>(d->fogf_o));
    d->v_valid_i = 0;
    d->eval();
  }

  // ---- §8's disabled case is a deterministic no-op, not full fog -----------
  {
    d->cfg_en_i = 1;
    d->cfg_near_i = 300 << 16;  // far <= near: DISABLED by §8
    d->cfg_far_i = 300 << 16;
    d->v_valid_i = 1;
    d->w_i = 380 << 16;  // a distance that WOULD be fully fogged if enabled
    d->eval();
    tick(d);
    d->eval();
    zhao::check(d->fogf_o == 0x10000,
                "fog_far <= fog_near DISABLES fog and emits CLEAR -- §8's "
                "deterministic no-op. Emitting 0 here would fog the whole world "
                "on a frame that asked for none.",
                0x10000, static_cast<uint64_t>(d->fogf_o));
    d->v_valid_i = 0;
    d->eval();
    d->cfg_near_i = kNear.raw;
    d->cfg_far_i = kFar.raw;
  }

  // ---- a behind-the-eye vertex is CLEAR, matching the reference ------------
  {
    d->cfg_en_i = 1;
    d->v_valid_i = 1;
    d->behind_i = 1;
    d->w_i = 0;
    d->eval();
    tick(d);
    d->eval();
    zhao::check(d->fogf_o == 0x10000,
                "a behind-the-eye vertex is left CLEAR -- its primitive is "
                "culled, and the reference's apply_vertex_fog leaves the lane at "
                "its default for exactly the same reason",
                0x10000, static_cast<uint64_t>(d->fogf_o));
    d->v_valid_i = 0;
    d->behind_i = 0;
    d->eval();
  }

  const int rc = zhao::report_and_exit("geom_fogfactor_directed");
  delete d;
  zhao::exit_hard(rc);
}
