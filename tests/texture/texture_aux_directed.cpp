// texture_aux_directed.cpp — TEXTURE.AUX directed tests (phase 6, ZH-060).
//
// The oracle `zref::AuxSource` did not exist before 2026-08-19 — the ledger
// named it and nothing under reference/ defined it — so lane 1 is not optional
// garnish: it proves the new header is a VIEW onto the ratified, executed law
// (`zref::render::sample_sheet`) and not a second implementation of it. If
// lane 1 is red, nothing after it means anything.
//
// What each lane would catch (the "could have been red" statement):
//   1. the view is faithful — `zref::AuxSource::sample` composed from
//      `axis_texel` agrees with `zref::render::sample_sheet` on the STRENGTH
//      byte over a sweep of world positions x envelopes, including positions
//      outside the envelope on each side and a degenerate one.
//      Red on: a u/v transposition, a dropped 64x, a floor/centre mix-up, a
//      clamp applied jointly instead of per axis.
//   2. axis anchors — hand-computed `axis_texel` values at the first texel,
//      the last texel, both clamp rails and the exact texel boundaries.
//      Red on: an off-by-one at either rail, a rounding bias.
//   3. RTL vs the oracle at CONSTRUCTED boundaries — for a fixed envelope,
//      the exact world position at which the quotient steps from k-1 to k, on
//      BOTH sides, for every k in 1..63; both clamp rails; and the int32
//      extremes. Uniform random positions never land on those, and they are
//      what separates a floor from a round.
//      Red on: an off-by-one in the six-step restoring divide, a wrong
//      saturation compare, a lost sign.
//   4. the degenerate envelope (F4) — the answer is {0,0} with the flag set
//      AND NO SHEET READ IS ISSUED. Red on: a block that computes zeros but
//      still burns a sheet-port cycle, or one that reads a garbage texel.
//   5. the miss (A2) — a non-resident sheet reads as ZERO with `smp_miss_o`,
//      never as stale bytes. Red on: a block that presents the previous
//      sample's bytes on a miss.
//   6. handshake, latency and the counter — the worst accept-to-retire is
//      MEASURED against the ledger's `variable_bounded:8`, the sustained rate
//      is measured and reported against the ledger's "1 aux sample per clock"
//      (which this shape does NOT meet — see the contract), stalls on both the
//      consumer and the sheet port leave the stream bit-identical, and
//      `texture_samples_o` counts retired samples.
//   7. composition with the REAL SURFACE.SHEET — acquire a sheet, write layer
//      F through the block that owns it, then read it back through
//      TEXTURE.AUX's own master port. This is the only place the port
//      semantics are actually pinned; the C++ SheetModel the random lanes use
//      is a convenience, not the law.
//      Red on: a swapped texel index (j*64+i vs i*64+j), a wrong opcode, a
//      response taken on the wrong cycle.

#include "texture_aux_dev.hpp"
#include "zref/zref_aux.hpp"
#include "zref/zref_render.hpp"
#include "zrender/internal.hpp"  // white-box: sample_sheet IS the law

#include "Vzhao_surface_sheet.h"

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

namespace at = aux_test;

// ---- 1. the view is faithful to the executed law ---------------------------

void test_view_is_faithful() {
  // a real sheet with a recognisable pattern, and a real patch carrying only
  // the envelope (that is all sample_sheet reads from it)
  static zref::render::SurfaceSheet sheet;
  for (int k = 0; k < 4096; ++k) {
    sheet.strength[k] = static_cast<uint8_t>((k * 7 + 13) & 0xFF);
    sheet.tag[k] = static_cast<uint8_t>((k * 31 + 5) & 0xFF);
  }
  const int32_t envs[5][4] = {
      {-(2 << 16), -(2 << 16), (2 << 16), (2 << 16)},  // the render tests' 4 m patch
      {0, 0, 1 << 16, 1 << 16},                        // 1 m
      {-1000, -7, 1000, 9},                            // sub-metre, asymmetric
      {1 << 20, -(1 << 20), (1 << 20) + 4096, 1 << 20},
      {0, 0, 0, 1 << 16},  // DEGENERATE in x
  };
  int mismatches = 0, outside = 0, inside = 0, degen = 0;
  for (int e = 0; e < 5; ++e) {
    zref::render::TerrainPatch patch;
    patch.env_x0 = envs[e][0];
    patch.env_z0 = envs[e][1];
    patch.env_x1 = envs[e][2];
    patch.env_z1 = envs[e][3];
    const int64_t span_x = static_cast<int64_t>(patch.env_x1) - patch.env_x0;
    const int64_t span_z = static_cast<int64_t>(patch.env_z1) - patch.env_z0;
    for (int s = -20; s <= 84; ++s) {
      // s/64 of the envelope, so s < 0 and s > 63 leave it on each side
      const int32_t wx = static_cast<int32_t>(patch.env_x0 + span_x * s / 64);
      const int32_t wz = static_cast<int32_t>(patch.env_z0 + span_z * (83 - s) / 64);
      const uint8_t want = zref::render::sample_sheet(sheet, patch, zref::fx16{wx}, zref::fx16{wz});
      zref::aux::Envelope env;
      env.x0 = patch.env_x0;
      env.z0 = patch.env_z0;
      env.x1 = patch.env_x1;
      env.z1 = patch.env_z1;
      const zref::aux::Sample got =
          zref::AuxSource::sample(env, wx, wz, sheet.tag, sheet.strength, true);
      if (got.strength != want) ++mismatches;
      if (got.degenerate) {
        ++degen;
      } else if (s < 0 || s > 63) {
        ++outside;
      } else {
        ++inside;
      }
    }
  }
  check(mismatches == 0, "zref::AuxSource is a faithful VIEW onto zref::render::sample_sheet");
  check(inside > 0 && outside > 0 && degen > 0,
        "the faithfulness sweep reaches inside, outside AND degenerate envelopes");
  std::printf("  view sweep: inside=%d outside=%d degenerate=%d mismatches=%d\n", inside, outside,
              degen, mismatches);
}

// ---- 2. axis anchors, hand-computed ----------------------------------------

void test_axis_anchors() {
  using zref::aux::axis_texel;
  // a 64-unit envelope: one raw unit per texel, so the mapping is the identity
  check(axis_texel(0, 0, 64) == 0, "w at e0 -> texel 0");
  check(axis_texel(1, 0, 64) == 1, "one unit in -> texel 1");
  check(axis_texel(63, 0, 64) == 63, "the last unit -> texel 63");
  check(axis_texel(64, 0, 64) == 63, "w AT e1 -> clamped to 63 (the envelope is half-open here)");
  check(axis_texel(-1, 0, 64) == 0, "one unit left of e0 -> clamped to 0");
  check(axis_texel(1 << 20, 0, 64) == 63, "far right -> 63");
  check(axis_texel(-(1 << 20), 0, 64) == 0, "far left -> 0");
  // a 128-unit envelope: two raw units per texel, so the FLOOR is visible
  check(axis_texel(1, 0, 128) == 0, "128-unit envelope: 1 unit floors to texel 0");
  check(axis_texel(2, 0, 128) == 1, "128-unit envelope: 2 units -> texel 1");
  check(axis_texel(3, 0, 128) == 1, "128-unit envelope: 3 units still texel 1 (FLOOR, not round)");
  // a negative origin, so the subtraction's sign matters
  check(axis_texel(-64, -128, 0) == 32, "negative envelope: the midpoint is texel 32");
  check(axis_texel(-128, -128, 0) == 0, "negative envelope: the origin is texel 0");
  // degenerate
  check(axis_texel(5, 10, 10) == 0, "e1 == e0 -> 0 (the early return)");
  check(axis_texel(5, 10, 9) == 0, "e1 < e0 -> 0");
}

// ---- shared RTL plumbing ---------------------------------------------------

void check_stream(const std::vector<at::Req>& in, const std::vector<at::Smp>& got,
                  const at::SheetModel& sheet, const char* what) {
  if (got.size() != in.size()) {
    std::fprintf(stderr, "FAIL: %s - %zu samples for %zu requests\n", what, got.size(), in.size());
    ++failures;
    return;
  }
  for (size_t k = 0; k < in.size(); ++k) {
    const at::Smp want = at::oracle(in[k], sheet.tag, sheet.strength, sheet.resident);
    if (!(got[k] == want)) {
      std::fprintf(stderr,
                   "FAIL: %s [%zu] wx=%d wz=%d env=(%d,%d)-(%d,%d) -> "
                   "u %u v %u tag %u str %u deg %d miss %d, oracle u %u v %u tag %u str %u "
                   "deg %d miss %d\n",
                   what, k, in[k].wx, in[k].wz, in[k].ex0, in[k].ez0, in[k].ex1, in[k].ez1,
                   got[k].u, got[k].v, got[k].tag, got[k].strength, got[k].degenerate ? 1 : 0,
                   got[k].miss ? 1 : 0, want.u, want.v, want.tag, want.strength,
                   want.degenerate ? 1 : 0, want.miss ? 1 : 0);
      ++failures;
      return;
    }
  }
}

void fill_sheet(at::SheetModel& s) {
  for (int k = 0; k < 4096; ++k) {
    s.tag[k] = static_cast<uint8_t>((k * 31 + 5) & 0xFF);
    s.strength[k] = static_cast<uint8_t>((k * 7 + 13) & 0xFF);
  }
  s.resident = true;
  s.stall_mask = 0;
  s.busy = false;
}

// ---- 3. the RTL at CONSTRUCTED texel boundaries -----------------------------

void test_rtl_boundaries(Vzhao_texture_aux& dut) {
  at::Dev dev(dut);
  dev.reset();
  at::SheetModel sheet;
  fill_sheet(sheet);

  // A 4 m envelope with a span that is NOT a multiple of 64, so every texel
  // boundary lands at an irregular raw offset and a shift-instead-of-divide
  // would be visibly wrong.
  const int32_t e0 = -(2 << 16), e1 = (2 << 16) + 37;
  const int64_t D = static_cast<int64_t>(e1) - e0;

  std::vector<at::Req> in;
  // for every k in 1..63: the LAST position that still gives k-1, and the
  // FIRST that gives k. The first w with floor((w-e0)*64/D) >= k is
  // w = e0 + ceil(k*D/64).
  int constructed = 0;
  for (int64_t k = 1; k < 64; ++k) {
    const int64_t off = (k * D + 63) / 64;  // ceil
    for (int side = 0; side < 2; ++side) {
      at::Req r;
      r.ex0 = e0;
      r.ex1 = e1;
      r.ez0 = e0;
      r.ez1 = e1;
      r.wx = static_cast<int32_t>(e0 + off - (side == 0 ? 1 : 0));
      // put the v axis a different distance in, so a u/v swap cannot hide
      r.wz = static_cast<int32_t>(e0 + (63 - k) * D / 64);
      r.handle = 0x1234;
      r.src_id = static_cast<uint16_t>(k * 2 + side);
      in.push_back(r);
      ++constructed;
    }
  }
  check(constructed == 126, "constructed both sides of all 63 texel boundaries");

  // both clamp rails and the int32 extremes on each axis
  const int32_t rails[6] = {INT32_MIN, INT32_MIN + 1, e0 - 1, e1, e1 + 1, INT32_MAX};
  for (int a = 0; a < 6; ++a) {
    for (int b = 0; b < 6; ++b) {
      at::Req r;
      r.ex0 = e0;
      r.ex1 = e1;
      r.ez0 = e0;
      r.ez1 = e1;
      r.wx = rails[a];
      r.wz = rails[b];
      r.handle = 0x1234;
      in.push_back(r);
    }
  }
  // and the widest legal envelope, where the divisor is 2^32 - 1
  for (int s = 0; s < 8; ++s) {
    at::Req r;
    r.ex0 = INT32_MIN;
    r.ex1 = INT32_MAX;
    r.ez0 = INT32_MIN;
    r.ez1 = INT32_MAX;
    r.wx = static_cast<int32_t>(INT32_MIN + (static_cast<int64_t>(s) << 29));
    r.wz = static_cast<int32_t>(INT32_MAX - (static_cast<int64_t>(s) << 29));
    r.handle = 0x1234;
    in.push_back(r);
  }

  const std::vector<at::Smp> got = dev.run(in, sheet);
  check_stream(in, got, sheet, "RTL at constructed texel boundaries and the domain rails");
  if (got.size() != in.size()) return;
  // read the first pair back explicitly: the step from texel 0 to texel 1
  check(got[0].u == 0 && got[1].u == 1,
        "RTL: the u boundary steps exactly where the FLOOR says it does");
  bool saw_0 = false, saw_63 = false;
  for (size_t k = 0; k < got.size(); ++k) {
    if (got[k].u == 0) saw_0 = true;
    if (got[k].u == 63) saw_63 = true;
  }
  check(saw_0 && saw_63, "RTL: both clamp rails are reached");
}

// ---- 4. the degenerate envelope (F4) ---------------------------------------

void test_degenerate(Vzhao_texture_aux& dut) {
  at::Dev dev(dut);
  dev.reset();
  at::SheetModel sheet;
  fill_sheet(sheet);

  std::vector<at::Req> in;
  const int32_t bad[4][4] = {
      {0, 0, 0, 1 << 16},            // x span zero
      {0, 0, 1 << 16, 0},            // z span zero
      {10, 10, 9, 9},                // both inverted
      {INT32_MAX, 0, INT32_MIN, 1},  // x inverted at the extreme
  };
  for (int k = 0; k < 4; ++k) {
    at::Req r;
    r.ex0 = bad[k][0];
    r.ez0 = bad[k][1];
    r.ex1 = bad[k][2];
    r.ez1 = bad[k][3];
    r.wx = 12345;
    r.wz = -6789;
    r.handle = 0x1234;
    r.src_id = static_cast<uint16_t>(k);
    in.push_back(r);
  }
  const std::vector<at::Smp> got = dev.run(in, sheet);
  check_stream(in, got, sheet, "RTL degenerate envelopes");
  if (got.size() != in.size()) return;
  bool all_zero = true;
  for (size_t k = 0; k < got.size(); ++k) {
    if (!got[k].degenerate || got[k].tag != 0 || got[k].strength != 0 || got[k].u != 0 ||
        got[k].v != 0) {
      all_zero = false;
    }
  }
  check(all_zero, "RTL: a degenerate envelope answers {0,0} at texel (0,0) with the flag set");
  // F4's second half, and the part a differential alone cannot see: NO sheet
  // read is issued, so a malformed envelope cannot burn a sheet-port cycle.
  check(dev.sheet_reads() == 0, "RTL: a degenerate envelope issues NO sheet read at all");
}

// ---- 5. the miss (A2) -------------------------------------------------------

void test_miss_reads_zero(Vzhao_texture_aux& dut) {
  at::Dev dev(dut);
  dev.reset();
  at::SheetModel resident;
  fill_sheet(resident);

  // first a resident read, so the block is holding real bytes...
  std::vector<at::Req> warm;
  at::Req r;
  r.ex0 = 0;
  r.ez0 = 0;
  r.ex1 = 64;
  r.ez1 = 64;
  r.wx = 40;
  r.wz = 40;
  r.handle = 7;
  r.src_id = 1;
  warm.push_back(r);
  const std::vector<at::Smp> w = dev.run(warm, resident);
  check(w.size() == 1 && w[0].strength != 0 && !w[0].miss, "RTL: a resident read returns bytes");

  // ...then the SAME texel on a non-resident sheet: it must read ZERO and say
  // miss, not present the bytes it was just holding.
  at::SheetModel absent;
  fill_sheet(absent);
  absent.resident = false;
  const std::vector<at::Smp> m = dev.run(warm, absent);
  check_stream(warm, m, absent, "RTL miss");
  if (m.size() != 1) return;
  check(m[0].miss && m[0].tag == 0 && m[0].strength == 0,
        "RTL: a non-resident sheet reads as ZERO, never as the previous sample's bytes");
  check(m[0].u == w[0].u && m[0].v == w[0].v,
        "RTL: a miss still reports the texel the mapping chose");
}

// ---- 6. handshake, latency, throughput, counter -----------------------------

void test_latency_bound(Vzhao_texture_aux& dut) {
  at::Dev dev(dut);
  dev.reset();
  check(dev.idle(), "RTL: idle out of reset");
  check(dev.samples() == 0, "RTL: the counter is zero out of reset");

  at::SheetModel sheet;
  fill_sheet(sheet);
  std::vector<at::Req> in;
  uint32_t rng = 0x13579BDFu;
  const auto next = [&rng]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };
  for (int k = 0; k < 256; ++k) {
    at::Req r;
    r.ex0 = -(2 << 16);
    r.ez0 = -(2 << 16);
    r.ex1 = (2 << 16);
    r.ez1 = (2 << 16);
    r.wx = static_cast<int32_t>(next() % (4u << 16)) - (2 << 16);
    r.wz = static_cast<int32_t>(next() % (4u << 16)) - (2 << 16);
    r.handle = 0x1234;
    r.src_id = static_cast<uint16_t>(k);
    in.push_back(r);
  }

  const std::vector<at::Smp> got = dev.run(in, sheet);
  check_stream(in, got, sheet, "RTL free-running stream");
  const int worst = dev.worst_latency();
  const long span = dev.span_clocks();
  std::printf(
      "  aux worst accept-to-retire: %d clocks (ledger variable_bounded:8); "
      "256 samples in %ld clocks = %.2f clocks/sample\n",
      worst, span, static_cast<double>(span) / 256.0);
  check(worst >= 0 && worst <= 8, "RTL: accept-to-retire is within the ledger's bound of 8");
  // The ledger asks for one aux sample per clock. This shape does NOT meet it,
  // and the shortfall is MEASURED rather than derived from the state count: one
  // request is in flight and the sheet read cannot start until the divide
  // answers. design/contracts/TEXTURE.AUX.md's Target throughput section
  // records the number this line prints.
  check(span == 1536, "RTL: the sustained rate is one sample per SIX clocks (256 in 1,536)");
  check(dev.samples() == 256, "RTL: texture_samples_o counts retired samples");
  check(dev.sheet_reads() == 256, "RTL: one sheet read per non-degenerate sample, never two");
  check(dev.idle(), "RTL: idle again once the stream drains");

  // stalls on BOTH back-pressuring ports must leave the stream identical
  const uint32_t out_masks[3] = {0xAAAAAAAAu, 0x0F0F0F0Fu, 0xFFFFFFFEu};
  const uint32_t sheet_masks[3] = {0u, 0xCCCCCCCCu, 0x7FFFFFFFu};
  for (int m = 0; m < 3; ++m) {
    at::Dev d2(dut);
    d2.reset();
    at::SheetModel s2;
    fill_sheet(s2);
    s2.stall_mask = sheet_masks[m];
    const std::vector<at::Smp> g2 = d2.run(in, s2, out_masks[m]);
    bool same = g2.size() == got.size();
    for (size_t k = 0; same && k < got.size(); ++k) same = (g2[k] == got[k]);
    check(same, "RTL: stalls on the consumer and on the sheet port change nothing");
    check(d2.samples() == 256, "RTL: the counter is unaffected by stalls");
  }
}

// ---- 7. composition with the REAL SURFACE.SHEET -----------------------------

// The two real blocks, wired the way the machine wires them: TEXTURE.AUX
// masters SURFACE.SHEET's read port. Nothing here models a port; both sides
// are the RTL.
void test_chain_real_sheet(Vzhao_texture_aux& aux, Vzhao_surface_sheet& sh) {
  // reset both
  aux.rst_n = 0;
  sh.rst_n = 0;
  aux.req_valid_i = 0;
  aux.smp_ready_i = 0;
  sh.req_valid_i = 0;
  sh.pg_ready_i = 0;
  sh.wr_valid_i = 0;
  aux.eval();
  sh.eval();
  for (int i = 0; i < 3; ++i) {
    zhao::tick(aux);
    zhao::tick(sh);
  }
  aux.rst_n = 1;
  sh.rst_n = 1;
  aux.eval();
  sh.eval();
  zhao::tick(aux);
  zhao::tick(sh);

  const uint32_t handle = 0x00002A01u;

  // --- ACQUIRE the sheet directly (this is CMD/SURFACE.STAMP's job in the
  //     machine; here the test plays that role) and let the clear sweep run
  const auto sheet_op = [&](uint8_t op, uint32_t h, uint16_t texel, int max_cycles) {
    for (int c = 0; c < max_cycles; ++c) {
      sh.req_valid_i = 1;
      sh.req_op_i = op;
      sh.req_handle_i = h;
      sh.req_texel_i = texel;
      sh.req_src_id_i = 9;
      sh.pg_ready_i = 1;
      sh.eval();
      const bool took = sh.req_ready_o != 0;
      zhao::tick(sh);
      if (took) break;
    }
    sh.req_valid_i = 0;
    // drain the response
    for (int c = 0; c < max_cycles; ++c) {
      sh.pg_ready_i = 1;
      sh.eval();
      const bool got = sh.pg_valid_o != 0;
      zhao::tick(sh);
      if (got) break;
    }
    sh.pg_ready_i = 0;
    sh.eval();
  };
  sheet_op(0 /*OpAcquire*/, handle, 0, 8192);

  // --- write a recognisable pattern through the sheet's own write port
  uint8_t want_tag[4096] = {};
  uint8_t want_str[4096] = {};
  for (int t = 0; t < 4096; ++t) {
    want_tag[t] = static_cast<uint8_t>((t * 13 + 3) & 0xFF);
    want_str[t] = static_cast<uint8_t>((t * 5 + 200) & 0xFF);
    for (int c = 0; c < 64; ++c) {
      sh.wr_valid_i = 1;
      sh.wr_handle_i = handle;
      sh.wr_texel_i = static_cast<uint16_t>(t);
      sh.wr_tag_i = want_tag[t];
      sh.wr_strength_i = want_str[t];
      sh.wr_we_tag_i = 1;
      sh.wr_we_strength_i = 1;
      sh.wr_src_id_i = 9;
      sh.eval();
      const bool took = sh.wr_ready_o != 0;
      zhao::tick(sh);
      if (took) break;
    }
    sh.wr_valid_i = 0;
    sh.eval();
  }

  // --- now read it back THROUGH TEXTURE.AUX's master port
  const int32_t e0 = -(2 << 16), e1 = (2 << 16);
  const int64_t D = static_cast<int64_t>(e1) - e0;
  struct Probe {
    int32_t wx, wz;
  };
  std::vector<Probe> probes;
  for (int k = 0; k < 64; ++k) {
    probes.push_back({static_cast<int32_t>(e0 + (k * 997 % 64) * D / 64),
                      static_cast<int32_t>(e0 + (k * 613 % 64) * D / 64)});
  }
  probes.push_back({e0 - 1000, e1 + 1000});  // both clamp rails at once

  int mism = 0;
  for (size_t p = 0; p < probes.size(); ++p) {
    aux.req_valid_i = 1;
    aux.req_wx_i = probes[p].wx;
    aux.req_wz_i = probes[p].wz;
    aux.req_env_x0_i = e0;
    aux.req_env_z0_i = e0;
    aux.req_env_x1_i = e1;
    aux.req_env_z1_i = e1;
    aux.req_handle_i = handle;
    aux.req_src_id_i = static_cast<uint16_t>(p);

    bool done = false;
    at::Smp got;
    for (int c = 0; c < 256 && !done; ++c) {
      aux.smp_ready_i = 1;
      // the wiring: aux masters the sheet's read port
      sh.req_valid_i = aux.shr_valid_o;
      sh.req_op_i = aux.shr_op_o;
      sh.req_handle_i = aux.shr_handle_o;
      sh.req_texel_i = aux.shr_texel_o;
      sh.req_src_id_i = aux.shr_src_id_o;
      sh.pg_ready_i = aux.shp_ready_o;
      sh.eval();
      aux.shr_ready_i = sh.req_ready_o;
      aux.shp_valid_i = sh.pg_valid_o;
      aux.shp_status_i = sh.pg_status_o;
      aux.shp_tag_i = sh.pg_tag_o;
      aux.shp_strength_i = sh.pg_strength_o;
      aux.eval();
      // The FSM samples `req_valid_i` AT the clock edge, so the offer must be
      // withdrawn AFTER the tick, never before it. (Withdrawing it here was
      // the first version's bug: the block never saw a request at all and the
      // chain deadlocked with the differential lanes still green.)
      const bool accepted = aux.req_valid_i && aux.req_ready_o;
      if (aux.smp_valid_o) {
        got.tag = static_cast<uint8_t>(aux.smp_tag_o);
        got.strength = static_cast<uint8_t>(aux.smp_strength_o);
        got.u = static_cast<uint8_t>(aux.smp_u_o);
        got.v = static_cast<uint8_t>(aux.smp_v_o);
        got.miss = aux.smp_miss_o != 0;
        done = true;
      }
      zhao::tick(aux);
      zhao::tick(sh);
      if (accepted) aux.req_valid_i = 0;
    }
    aux.smp_ready_i = 0;
    aux.eval();
    if (!done) {
      ++mism;
      continue;
    }
    const int32_t wu = zref::aux::axis_texel(probes[p].wx, e0, e1);
    const int32_t wv = zref::aux::axis_texel(probes[p].wz, e0, e1);
    const int idx = wv * 64 + wu;
    if (got.u != wu || got.v != wv || got.miss || got.tag != want_tag[idx] ||
        got.strength != want_str[idx]) {
      ++mism;
      if (mism == 1) {
        std::fprintf(stderr,
                     "FAIL: chain probe %zu wx=%d wz=%d -> u %u v %u tag %u str %u (miss %d), "
                     "want u %d v %d tag %u str %u\n",
                     p, probes[p].wx, probes[p].wz, got.u, got.v, got.tag, got.strength,
                     got.miss ? 1 : 0, wu, wv, want_tag[idx], want_str[idx]);
      }
    }
  }
  check(mism == 0, "chain: TEXTURE.AUX reads back what SURFACE.STAMP's port wrote, texel-exact");

  // a handle that was never acquired must come back as a MISS reading zero
  bool miss_ok = false;
  aux.req_valid_i = 1;
  aux.req_wx_i = 0;
  aux.req_wz_i = 0;
  aux.req_env_x0_i = e0;
  aux.req_env_z0_i = e0;
  aux.req_env_x1_i = e1;
  aux.req_env_z1_i = e1;
  aux.req_handle_i = 0x0BADF00Du;
  aux.req_src_id_i = 77;
  for (int c = 0; c < 256; ++c) {
    aux.smp_ready_i = 1;
    sh.req_valid_i = aux.shr_valid_o;
    sh.req_op_i = aux.shr_op_o;
    sh.req_handle_i = aux.shr_handle_o;
    sh.req_texel_i = aux.shr_texel_o;
    sh.req_src_id_i = aux.shr_src_id_o;
    sh.pg_ready_i = aux.shp_ready_o;
    sh.eval();
    aux.shr_ready_i = sh.req_ready_o;
    aux.shp_valid_i = sh.pg_valid_o;
    aux.shp_status_i = sh.pg_status_o;
    aux.shp_tag_i = sh.pg_tag_o;
    aux.shp_strength_i = sh.pg_strength_o;
    aux.eval();
    const bool accepted = aux.req_valid_i && aux.req_ready_o;
    if (aux.smp_valid_o) {
      miss_ok = (aux.smp_miss_o != 0) && aux.smp_tag_o == 0 && aux.smp_strength_o == 0;
      break;
    }
    zhao::tick(aux);
    zhao::tick(sh);
    if (accepted) aux.req_valid_i = 0;
  }
  aux.smp_ready_i = 0;
  aux.eval();
  check(miss_ok, "chain: a handle SURFACE.SHEET never allocated reads as a zero MISS (A2)");
}

}  // namespace

int main() {
  test_view_is_faithful();
  test_axis_anchors();

  Vzhao_texture_aux dut;
  test_rtl_boundaries(dut);
  test_degenerate(dut);
  test_miss_reads_zero(dut);
  test_latency_bound(dut);

  Vzhao_surface_sheet sheet_dut;
  test_chain_real_sheet(dut, sheet_dut);

  if (failures == 0) std::printf("texture_aux_directed: all green\n");
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
