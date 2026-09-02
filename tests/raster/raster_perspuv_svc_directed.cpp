// raster_perspuv_svc_directed.cpp — is the scheduled perspective lane
// bit-identical to the serial block, and does it launch one product a clock?
//
// ---------------------------------------------------------------------------
// THE ORACLE IS THE SHIPPED BLOCK
// ---------------------------------------------------------------------------
// Same discipline as the reciprocal: `zhao_raster_perspuv` runs on the stimulus
// and its answers ARE the expected values. The lane has a narrower scope -- it
// takes a reciprocal that has already returned, where the reference contains
// its own -- so tb_perspuv_pair.sv carries a standalone rcp24 to derive the
// identical mantissa and exponent. Approximating that in C++ would compare the
// lane against a guess at the reference's input.
//
// ---------------------------------------------------------------------------
// THE GATES THE BRIEF NAMES
// ---------------------------------------------------------------------------
//   > Bit-exact against the existing zhao_raster_rcp24 and zhao_raster_perspuv.
//   > Zero, saturation and every exponent boundary.
//   > Repeated external src_id.
//   > Returns intentionally reordered by token.
//   > Randomized output stalls.
//
// All are exercised here except reordering, which this lane cannot exhibit --
// it retires in allocation order by construction, and the check asserts that
// rather than pretending to test something the design forbids.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vtb_perspuv_pair.h"

#include "zhao_sim.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

struct Frag {
  int32_t uow, vow;
  uint32_t invw;
  uint16_t tag;
};

struct Res {
  int32_t u, v;
  uint32_t sat, dz;
  bool operator!=(const Res& o) const {
    return u != o.u || v != o.v || sat != o.sat || dz != o.dz;
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_perspuv_pair top;

  auto reset = [&]() {
    top.a_valid_i = 0;
    top.b_valid_i = 0;
    top.c_valid_i = 0;
    top.a_rready_i = 1;
    top.b_rready_i = 1;
    top.c_rready_i = 1;
    top.rst_n = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // ---- stimulus: boundaries first, then random ----------------------------
  std::vector<Frag> fr;
  // depth zero (a caller bug the block flags rather than computes)
  fr.push_back({12345, -6789, 0u, 0x0001});
  // every exponent boundary: invw with a single bit set walks k across range
  for (int b = 0; b < 24; ++b)
    fr.push_back({0x0040'0000, -0x0040'0000, 1u << b, static_cast<uint16_t>(0x100 + b)});
  // saturation: the largest numerators against the smallest denominators
  fr.push_back({0x7FFF'FFFF, 0x7FFF'FFFF, 1u, 0x0200});
  fr.push_back({static_cast<int32_t>(0x8000'0000), static_cast<int32_t>(0x8000'0000), 1u, 0x0201});
  // REPEATED TAG -- the brief's "repeated external src_id". Every fragment of
  // one triangle carries the same value, so identity must be internal.
  for (int i = 0; i < 8; ++i)
    fr.push_back({1000 + i, -2000 - i, 0x00A0'0000u, 0x0300});

  uint32_t s = 0x2468ACu;
  for (int i = 0; i < 300; ++i) {
    Frag f{};
    f.uow = static_cast<int32_t>(rnd(&s));
    f.vow = static_cast<int32_t>(rnd(&s));
    f.invw = rnd(&s) & 0xFFFFFFu;
    f.tag = static_cast<uint16_t>(i);
    fr.push_back(f);
  }

  // ---- run the SERIAL reference -------------------------------------------
  std::vector<Res> ref;
  int serial_clocks = 0;
  {
    reset();
    size_t fed = 0;
    for (int c = 0; c < 400000 && ref.size() < fr.size(); ++c) {
      const bool feeding = fed < fr.size();
      top.a_valid_i = feeding;
      if (feeding) {
        top.a_uow_i = fr[fed].uow;
        top.a_vow_i = fr[fed].vow;
        top.a_invw_i = fr[fed].invw;
        top.a_tag_i = fr[fed].tag;
      }
      top.eval();
      if (top.a_rvalid_o)
        ref.push_back({static_cast<int32_t>(top.a_u_o), static_cast<int32_t>(top.a_v_o),
                       top.a_sat_o, top.a_dz_o});
      const bool took = feeding && top.a_ready_o;
      zhao::tick(top);
      if (took) ++fed;
      ++serial_clocks;
    }
    top.a_valid_i = 0;
    zhao::check(ref.size() == fr.size(), "the serial block answered every fragment",
                fr.size(), ref.size());
  }

  // ---- derive mant/k per fragment from a standalone rcp24 -----------------
  std::vector<std::pair<uint32_t, uint32_t>> mk;  // (mant, k)
  {
    reset();
    size_t fed = 0;
    for (int c = 0; c < 400000 && mk.size() < fr.size(); ++c) {
      const bool feeding = fed < fr.size();
      top.c_valid_i = feeding;
      top.c_d_i = feeding ? fr[fed].invw : 0;
      top.eval();
      if (top.c_rvalid_o) mk.push_back({top.c_r_o, top.c_k_o});
      const bool took = feeding && top.c_ready_o;
      zhao::tick(top);
      if (took) ++fed;
    }
    top.c_valid_i = 0;
    zhao::check(mk.size() == fr.size(), "a reciprocal was derived for every fragment",
                fr.size(), mk.size());
  }

  // ---- run the SCHEDULED lane, WITH RANDOMISED OUTPUT STALLS --------------
  std::vector<Res> got;
  int svc_clocks = 0;
  uint32_t ss = 0x99AA55u;
  {
    reset();
    size_t fed = 0;
    for (int c = 0; c < 400000 && got.size() < fr.size(); ++c) {
      const bool feeding = fed < fr.size();
      top.b_valid_i = feeding;
      if (feeding) {
        top.b_uow_i = fr[fed].uow;
        top.b_vow_i = fr[fed].vow;
        top.b_mant_i = mk[fed].first;
        top.b_k_i = mk[fed].second;
        top.b_dz_i = (fr[fed].invw == 0);
        top.b_tag_i = fr[fed].tag;
      }
      // The brief asks for randomised output stalls: a consumer that is not
      // always ready is the normal case, and a lane that only works when the
      // sink never blocks is not finished.
      top.b_rready_i = (rnd(&ss) & 3u) != 0;
      top.eval();
      if (top.b_rvalid_o && top.b_rready_i)
        got.push_back({static_cast<int32_t>(top.b_u_o), static_cast<int32_t>(top.b_v_o),
                       top.b_sat_o, top.b_dz_o});
      const bool took = feeding && top.b_ready_o;
      zhao::tick(top);
      if (took) ++fed;
      ++svc_clocks;
    }
    top.b_valid_i = 0;
  }

  zhao::check(got.size() == fr.size(), "the scheduled lane answered every fragment",
              fr.size(), got.size());

  int mism = 0;
  for (size_t i = 0; i < ref.size() && i < got.size(); ++i)
    if (got[i] != ref[i]) ++mism;
  zhao::check(mism == 0,
              "every scheduled answer is BIT-IDENTICAL to the serial block's",
              0, mism);

  // TWO PRODUCTS PER FRAGMENT, except the depth-zero one which computes none.
  const uint32_t expect_prod = 2u * static_cast<uint32_t>(fr.size() - 1);
  zhao::check(top.b_products_o == expect_prod,
              "exactly two multiplier launches per fragment (none for depth-zero)",
              expect_prod, top.b_products_o);

  std::printf("  serial:    %zu fragments in %d clocks (%.2f each)\n", ref.size(),
              serial_clocks,
              static_cast<double>(serial_clocks) / static_cast<double>(ref.size()));
  std::printf("  scheduled: %zu fragments in %d clocks (%.2f each, WITH stalls)\n",
              got.size(), svc_clocks,
              static_cast<double>(svc_clocks) / static_cast<double>(got.size()));

  return zhao::report_and_exit("raster_perspuv_svc_directed");
}
