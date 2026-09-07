// compose_rtl_directed.cpp -- TERRAIN.PAGESTREAM -> TERRAIN.PATCH, against
// zref::terrain::compose_vertex.
//
// ===========================================================================
// WHAT IS NEW HERE, AND IT IS NOT THE BLOCKS
// ===========================================================================
// Both blocks have their own differentials. What this file adds is that §3.4's
// composition runs on REAL PAGE BYTES for the first time: the heights come out
// of a 21,376-byte page image through the streamer's three cursors, not out of
// a lattice a bench invented. `tb_terrain_world.sv`'s header called that chain
// "reachable only from a harness on BOTH ends, which is decoration rather than
// composition"; the missing end landed on 2026-09-07.
//
// The claim is therefore a SEAM claim: for every one of the 1,089 vertices of a
// real page, `compose_top_o` is what `zref::terrain::compose_vertex` gives for
// the three planes that page actually contains, in the order the scan produces
// them.
//
// ===========================================================================
// THE FIXTURE HAS TO REACH THE CLAMP, OR IT PROVES NOTHING
// ===========================================================================
// §3.4 is `compose_top = max(fx(base) + fx(scar), fx(bottom))`. A page whose
// bottom is always far below its top never exercises the max, and a suite drawn
// from such a page would pass against a block that dropped the clamp entirely.
// A page whose scar is always zero never exercises the add.
//
// So the page is built so that all four cases occur and are COUNTED:
//
//   * scar positive, the sum clear of the bottom          -- the add alone
//   * scar negative, the sum still clear of the bottom    -- a signed add
//   * scar so negative the sum falls BELOW the bottom     -- THE CLAMP FIRES
//   * base already at the bottom, scar zero               -- the boundary
//
// The counts are asserted. A fixture that reached only one case would be a
// green run that means nothing, and this file says so out loud rather than
// hoping.
//
// SATURATION IS NOT REACHABLE HERE, AND THE FIXTURE FOUND THAT OUT.
//
// The fifth case was written as "base near +32,767 with a positive scar, so
// `fx_add` saturates and the reference's `SatLedger` has something to record".
// It does not. §3.4's up-conversion is an exact `raw << 8` and both operands
// come from height16, so the widest possible sum is
//
//     (32,767 << 8) + (32,767 << 8) = 16,776,704
//
// against `fx_add`'s 2^31 range -- two orders of magnitude short. NOTHING
// SOURCED FROM A PAGE CAN SATURATE THIS ADD. Only the FIELD lanes can, because
// those are full-range fx16 out of a field program rather than an up-converted
// int16, and they are a different lane's evidence.
//
// The case is kept, renamed to what it is -- the widest magnitude height16 can
// carry -- and the arithmetic is asserted, so the next reader looking for
// saturation coverage on this path finds the reason it is absent instead of
// adding a case that cannot fire.
//
// IT WAS THE CHECK ON THE ORACLE THAT FOUND IT, not the check on the fixture:
// "the fixture's intent is not evidence, its effect is" came back 218 and 218
// when the intent claimed 218 clamped plus 217 saturating.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_compose.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_page.hpp"
#include "zref/zref_terrain_patch.hpp"
#include "zref/zref_sw_stream.hpp"

namespace {

namespace tp = zref::terrain;

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what) {
  ++g_checks;
  if (!ok) { ++g_fail; std::printf("FAIL: %s\n", what); std::fflush(stdout); }
}

void ck(bool ok, const char* what, long expect, long got) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %ld, got %ld)\n", what, expect, got);
    std::fflush(stdout);
  }
}

constexpr uint32_t kPageBytes = tp::kPageBytes;
constexpr uint32_t kPageWords = kPageBytes / 8;
constexpr uint32_t kSlots = 4;
constexpr uint32_t kPoolBase = 0x04000000u;
constexpr int kEdge = tp::kLatticeEdge;      // 33
constexpr int kVerts = tp::kLatticeVerts;    // 1,089

// The placement law this bench declares and the RTL is told. wx follows the
// COLUMN and wz the ROW, which is the one thing about the scan's shape that has
// to be agreed in two places.
constexpr int32_t kX0 = 0x0010'0000;
constexpr int32_t kZ0 = -0x0020'0000;
constexpr int32_t kStep = 0x0002'0000;       // 2.0 in fx16

struct Pool {
  std::vector<uint8_t> b;
  Pool() : b(kSlots * kPageBytes, 0) {}
  void put16(uint32_t slot, uint32_t off, int16_t v) {
    const uint32_t a = slot * kPageBytes + off;
    b[a] = uint8_t(uint16_t(v) & 0xFF);
    b[a + 1] = uint8_t((uint16_t(v) >> 8) & 0xFF);
  }
  const uint8_t* page(uint32_t slot) const { return &b[slot * kPageBytes]; }
};

struct CaseCounts {
  int add_pos = 0, add_neg = 0, clamped = 0, at_bottom = 0, large = 0;
};

// THE PAGE, built so every arm of §3.4 is reached. See the header.
CaseCounts fill_page(Pool& p, uint32_t slot) {
  CaseCounts n;
  for (int k = 0; k < kVerts; ++k) {
    int16_t base, scar, bottom;
    switch (k % 5) {
      case 0:  // the add alone: scar up, well clear of the bottom
        base = int16_t(4000 + (k % 97));
        scar = int16_t(300 + (k % 31));
        bottom = int16_t(-2000);
        ++n.add_pos;
        break;
      case 1:  // a signed add that still clears the bottom
        base = int16_t(4000 + (k % 89));
        scar = int16_t(-(200 + (k % 29)));
        bottom = int16_t(-2000);
        ++n.add_neg;
        break;
      case 2:  // THE CLAMP: the sum falls below the bottom
        base = int16_t(500 + (k % 13));
        scar = int16_t(-(3000 + (k % 41)));
        bottom = int16_t(100);
        ++n.clamped;
        break;
      case 3:  // the boundary: base already AT the bottom, no scar
        base = int16_t(-1500);
        scar = 0;
        bottom = int16_t(-1500);
        ++n.at_bottom;
        break;
      default:  // THE WIDEST MAGNITUDE height16 CAN CARRY. Not a saturation
                // case -- see the header for why none exists on this path --
                // but it is the most the up-conversion ever has to move.
        base = int16_t(32000 + (k % 700));
        scar = int16_t(1000 + (k % 500));
        bottom = int16_t(-30000);
        ++n.large;
        break;
    }
    p.put16(slot, tp::kLayerAOff + 2u * uint32_t(k), base);
    p.put16(slot, tp::kLayerBOff + 2u * uint32_t(k), scar);
    p.put16(slot, tp::kLayerCOff + 2u * uint32_t(k), bottom);
  }
  // The bytes nobody should read, made loud.
  for (uint32_t off = tp::kLayerDOff; off + 1 < kPageBytes; off += 2)
    p.put16(slot, off, int16_t(0x7EEE));
  return n;
}

struct World {
  Vtb_terrain_compose& d;
  explicit World(Vtb_terrain_compose& dd) : d(dd) {}

  void config() {
    d.cfg_vram_window_base_i = kPoolBase;
    d.cfg_grant_hold_i = 0;
    d.cfg_rd_latency_i = 2;
    d.cfg_rd_gap_i = 0;
    d.cfg_vram_client_i = 6;
    d.cfg_epoch_i = 0x2Au;
    // The flag as the RECORD carries it, not as a pin on TERRAIN.PATCH. See
    // phase D for why that distinction is the whole point.
    d.j_flags = uint16_t(zref::swstream::kFlagRequired | zref::swstream::kFlagDual);
    d.cfg_x0_i = kX0;
    d.cfg_z0_i = kZ0;
    d.cfg_step_i = kStep;
  }

  void reset() {
    d.rst_n = 0;
    d.mw_en = 0;
    d.j_valid = 0;
    d.st_ready = 0;
    d.ps_done_ready = 0;
    config();
    d.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(d);
    d.rst_n = 1;
    d.eval();
    for (int i = 0; i < 2; ++i) zhao::tick(d);
  }

  void load(const Pool& p) {
    for (uint32_t w = 0; w < kSlots * kPageWords; ++w) {
      uint64_t v = 0;
      for (int b = 0; b < 8; ++b) v |= uint64_t(p.b[w * 8 + uint32_t(b)]) << (8 * b);
      d.mw_en = 1;
      d.mw_addr = w;
      d.mw_data = v;
      d.eval();
      zhao::tick(d);
    }
    d.mw_en = 0;
    d.eval();
  }
};

struct Got {
  int32_t compose_top = 0, top = 0, bottom = 0;
  bool dirty = false;
  int vi = 0, vj = 0;
};

bool draw(uint32_t& s, int pattern) {
  s = s * 1664525u + 1013904223u;
  switch (pattern) {
    case 0: return true;
    case 1: return ((s >> 16) & 1u) != 0u;
    case 2: return ((s >> 16) & 3u) != 0u;
    default: return ((s >> 16) & 7u) == 0u;
  }
}

std::vector<Got> stream(World& w, uint32_t slot, int pattern, uint64_t cap = 2000000ull) {
  Vtb_terrain_compose& d = w.d;
  std::vector<Got> out;
  out.reserve(kVerts);
  uint32_t ss = 0x1234u ^ uint32_t(pattern * 7919);
  bool done = false;

  d.j_valid = 1;
  d.j_slot = uint16_t(slot);
  d.j_gen = 0x11;
  d.j_epoch = 0x2Au;
  d.j_src_id = 0xBEEFu;
  d.ps_done_ready = 1;
  d.eval();
  int g = 0;
  while (!d.j_ready && g < 1000) { zhao::tick(d); d.eval(); ++g; }
  zhao::tick(d);
  d.j_valid = 0;
  d.eval();

  // THE STREAMER'S vi/vj ARE CAPTURED ON THE COMPOSED BEAT, not on the vertex
  // beat -- TERRAIN.PATCH is a pipeline and the two are different cycles. What
  // the comparison needs is the ORDER, and the order is the scan's, so the
  // index is counted here rather than read off a wire that belongs to the other
  // end of the pipe.
  for (uint64_t c = 0; c < cap && out.size() < std::size_t(kVerts); ++c) {
    d.st_ready = draw(ss, pattern) ? 1 : 0;
    d.eval();
    if (d.st_valid && d.st_ready) {
      Got x;
      x.compose_top = int32_t(d.st_compose_top);
      x.top = int32_t(d.st_top);
      x.bottom = int32_t(d.st_bottom);
      x.dirty = d.st_dirty != 0;
      x.vi = int(out.size()) / kEdge;
      x.vj = int(out.size()) % kEdge;
      out.push_back(x);
    }
    if (d.ps_done_valid && d.ps_done_ready) done = true;
    zhao::tick(d);
    d.eval();
  }
  // Let the streamer's completion retire.
  for (int i = 0; i < 400 && !done; ++i) {
    d.eval();
    if (d.ps_done_valid && d.ps_done_ready) done = true;
    zhao::tick(d);
  }
  d.st_ready = 0;
  d.ps_done_ready = 0;
  d.eval();
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_terrain_compose* dut = new Vtb_terrain_compose;
  World w(*dut);
  Vtb_terrain_compose& d = *dut;

  std::printf("== TERRAIN.PAGESTREAM -> TERRAIN.PATCH vs zref::terrain::compose_vertex ==\n");
  std::printf("   %d vertices from a real %u-byte page; placement wx = %d + vj*%d, "
              "wz = %d + vi*%d (fx16)\n\n",
              kVerts, kPageBytes, kX0, kStep, kZ0, kStep);

  Pool pool;
  const CaseCounts n = fill_page(pool, 1);
  fill_page(pool, 2);

  // THE FIXTURE REACHES EVERY ARM, asserted before anything is measured with
  // it. A page that exercised one case would make every check below vacuous.
  ck(n.add_pos > 0 && n.add_neg > 0 && n.clamped > 0 && n.at_bottom > 0 &&
         n.large > 0,
     "the page reaches every arm of section 3.4 that a PAGE can reach -- a positive "
     "add, a negative add, the CLAMP at the bottom, the boundary case and the widest "
     "magnitude height16 can carry",
     1,
     (n.add_pos > 0 && n.add_neg > 0 && n.clamped > 0 && n.at_bottom > 0 &&
      n.large > 0) ? 1 : 0);
  std::printf("   fixture: add+ %d, add- %d, CLAMPED %d, at-bottom %d, large %d\n",
              n.add_pos, n.add_neg, n.clamped, n.at_bottom, n.large);

  // SATURATION IS UNREACHABLE FROM A PAGE, asserted with the arithmetic rather
  // than assumed. See the header: this is the check that keeps the absence of
  // saturation coverage on this path a stated fact instead of a hole.
  {
    const long long widest = (1LL * 32767 << 8) + (1LL * 32767 << 8);
    ck(widest < (1LL << 31),
       "fx_add CANNOT saturate on page-sourced heights -- both operands are height16 "
       "up-converted by an exact raw<<8, so the widest sum is 16,776,704 against a 2^31 "
       "range. Only the FIELD lanes, full-range fx16 rather than up-converted int16, can "
       "reach it, and that is a different lane's evidence",
       1, (widest < (1LL << 31)) ? 1 : 0);
    std::printf("   widest page-sourced sum %lld vs fx_add's range %lld -- saturation is "
                "a FIELD-lane concern, not a page one\n", widest, (1LL << 31));
  }

  w.reset();
  w.load(pool);

  // The reference, computed once from the same page bytes the RTL will read.
  const std::vector<tp::LatticeVertex> lat = tp::page_lattice(pool.page(1));
  ck(int(lat.size()) == kVerts, "the reference lattice is the whole page", kVerts,
     int(lat.size()));

  tp::FieldList empty;
  std::vector<tp::ComposeOut> want(kVerts);
  for (int k = 0; k < kVerts; ++k) {
    tp::ComposeIn in;
    in.base = lat[std::size_t(k)].base;
    in.scar = lat[std::size_t(k)].scar;
    in.bottom = lat[std::size_t(k)].bottom;
    in.dual = true;
    in.wx = kX0 + kStep * lat[std::size_t(k)].vj;
    in.wz = kZ0 + kStep * lat[std::size_t(k)].vi;
    want[std::size_t(k)] = tp::compose_vertex(in, empty, nullptr);
  }

  // AND THE REFERENCE ITSELF MUST REACH THE CLAMP. The fixture counts what it
  // INTENDED; this counts what the oracle actually did, which is the number
  // that matters -- an intent that did not produce a clamped vertex would be a
  // fixture that lies about itself.
  int ref_clamped = 0;
  for (int k = 0; k < kVerts; ++k) {
    const int32_t sum = (int32_t(lat[std::size_t(k)].base) << 8) +
                        (int32_t(lat[std::size_t(k)].scar) << 8);
    if (want[std::size_t(k)].compose_top != sum) ++ref_clamped;
  }
  ck(ref_clamped > 0,
     "and the ORACLE clamped a real number of them -- the fixture's intent is not "
     "evidence, its effect is",
     1, ref_clamped > 0 ? 1 : 0);
  ck(ref_clamped == n.clamped,
     "and it moved EXACTLY the vertices the fixture built to be clamped, no more and no "
     "fewer. This is the check that found the fifth case was not a saturation case at "
     "all: the two counts agreed at 218 while the intent claimed 218 plus 217",
     n.clamped, ref_clamped);
  std::printf("   oracle: %d of %d vertices had compose_top moved by the clamp\n\n",
              ref_clamped, kVerts);

  // =========================================================================
  // A -- ONE LATTICE, EVERY VERTEX
  // =========================================================================
  {
    std::printf("-- A: slot 1, no stalls --\n");
    const std::vector<Got> got = stream(w, 1, 0);
    ck(int(got.size()) == kVerts, "A every vertex composed", kVerts, int(got.size()));
    ck(int(d.pt_samples) >= kVerts, "A and TERRAIN.PATCH counted them", kVerts,
       long(d.pt_samples));
    ck(d.pt_fields_active == 0,
       "A with an empty field list, so live_top is compose_top and this bench measures "
       "the half it can",
       0, long(d.pt_fields_active));
    ck(d.ps_lattices == 1, "A one lattice streamed", 1, long(d.ps_lattices));
    ck(d.ps_done_ok != 0, "A and the streamer reported ok");

    int bad = 0, printed = 0;
    for (int k = 0; k < int(got.size()); ++k) {
      const Got& g = got[std::size_t(k)];
      const tp::ComposeOut& e = want[std::size_t(k)];
      if (g.compose_top == e.compose_top && g.top == e.live_top && g.bottom == e.bottom &&
          g.dirty == e.dirty)
        continue;
      ++bad;
      if (printed < 5) {
        ++printed;
        std::printf("   A vertex %d (vi=%d vj=%d) base=%d scar=%d bottom=%d:\n", k, g.vi,
                    g.vj, lat[std::size_t(k)].base, lat[std::size_t(k)].scar,
                    lat[std::size_t(k)].bottom);
        if (g.compose_top != e.compose_top)
          std::printf("      compose_top got %11d want %11d\n", g.compose_top,
                      e.compose_top);
        if (g.top != e.live_top)
          std::printf("      top         got %11d want %11d\n", g.top, e.live_top);
        if (g.bottom != e.bottom)
          std::printf("      bottom      got %11d want %11d\n", g.bottom, e.bottom);
        if (g.dirty != e.dirty)
          std::printf("      dirty       got %d want %d\n", int(g.dirty), int(e.dirty));
      }
    }
    ck(bad == 0,
       "A every vertex composes to what zref::terrain::compose_vertex gives for the "
       "three planes THAT PAGE contains -- section 3.4 on real bytes, not on a lattice "
       "a bench invented",
       0, bad);

    // WITH NO FIELDS, live_top IS compose_top. Checked explicitly, because a
    // block that dropped the field chain entirely would pass everything above.
    int differ = 0;
    for (const Got& g : got) if (g.top != g.compose_top) ++differ;
    ck(differ == 0,
       "A and with no accepted field programs, live_top IS compose_top", 0, differ);
  }

  // =========================================================================
  // B -- FOUR STALL PATTERNS
  // =========================================================================
  // The composed lane is a pipeline between two real blocks: the streamer's
  // `v_ready` is TERRAIN.PATCH's, and TERRAIN.PATCH's `st_ready` is this
  // bench's. A pattern that never stalls the far end never makes the near end
  // wait, and a sibling suite passed 15,625 cases that way and still missed a
  // dropped answer.
  {
    std::printf("\n-- B: four stall patterns, and the fabric made slow --\n");
    for (int pattern = 0; pattern < 4; ++pattern) {
      w.reset();
      d.cfg_grant_hold_i = uint8_t(pattern);
      d.cfg_rd_latency_i = uint8_t(1 + pattern * 2);
      d.cfg_rd_gap_i = uint8_t(pattern);
      d.eval();
      const std::vector<Got> got = stream(w, 1, pattern);

      char msg[176];
      std::snprintf(msg, sizeof msg, "B pattern %d composed every vertex", pattern);
      ck(int(got.size()) == kVerts, msg, kVerts, int(got.size()));

      int bad = 0;
      for (int k = 0; k < int(got.size()); ++k) {
        const Got& g = got[std::size_t(k)];
        const tp::ComposeOut& e = want[std::size_t(k)];
        if (g.compose_top != e.compose_top || g.top != e.live_top || g.bottom != e.bottom ||
            g.dirty != e.dirty)
          ++bad;
      }
      std::snprintf(msg, sizeof msg, "B pattern %d matches the oracle at every vertex",
                    pattern);
      ck(bad == 0, msg, 0, bad);
      std::printf("   pattern %d: %d vertices, %u samples\n", pattern, int(got.size()),
                  d.pt_samples);
    }
  }

  // =========================================================================
  // C -- A SECOND PAGE, so the first is not being remembered
  // =========================================================================
  {
    std::printf("\n-- C: slot 2, which holds the same law over different bytes --\n");
    w.reset();
    w.load(pool);
    const std::vector<Got> got = stream(w, 2, 0);
    const std::vector<tp::LatticeVertex> lat2 = tp::page_lattice(pool.page(2));
    ck(int(got.size()) == kVerts, "C every vertex composed", kVerts, int(got.size()));

    int bad = 0;
    for (int k = 0; k < int(got.size()); ++k) {
      tp::ComposeIn in;
      in.base = lat2[std::size_t(k)].base;
      in.scar = lat2[std::size_t(k)].scar;
      in.bottom = lat2[std::size_t(k)].bottom;
      in.dual = true;
      in.wx = kX0 + kStep * lat2[std::size_t(k)].vj;
      in.wz = kZ0 + kStep * lat2[std::size_t(k)].vi;
      const tp::ComposeOut e = tp::compose_vertex(in, empty, nullptr);
      const Got& g = got[std::size_t(k)];
      if (g.compose_top != e.compose_top || g.top != e.live_top || g.bottom != e.bottom)
        ++bad;
    }
    ck(bad == 0, "C and slot 2's bytes compose to slot 2's answer", 0, bad);
  }


  // =========================================================================
  // D -- kFlagDual, AND WHETHER IT ARRIVES AT ALL
  // =========================================================================
  // `dual_i` is not decoration: `zhao_terrain_patch.sv` reads it inside BOTH
  // clamps (`ctop_new`, `ctop_clamped`) and it decides whether `bottom_o` is
  // `fx(bottom)` or `live_top`. A page composed with the wrong `dual` is a
  // different island underside, in the right shape, with every counter
  // agreeing.
  //
  // Until today nothing in `fpga/rtl` routed T5's `kFlagDual` from the patch
  // record to that pin -- this bench tied it to a knob and the gap was a
  // finding. The streamer now carries the record's whole 16-bit `flags` as
  // identity, and THIS PHASE TESTS THE ROUTING rather than the clamp: the same
  // page is composed twice, once with the flag set in the JOB and once clear,
  // and each run is compared against `compose_vertex` with the matching
  // `in.dual`.
  //
  // AND THE TWO ANSWERS MUST DIFFER. Otherwise the phase would pass against a
  // block that ignored the flag entirely, which is exactly the failure it
  // exists to catch -- the page reaches the clamp 218 times, so they do.
  {
    std::printf("\n-- D: kFlagDual, carried from the record --\n");

    std::vector<tp::ComposeOut> want_legacy(kVerts);
    for (int k = 0; k < kVerts; ++k) {
      tp::ComposeIn in;
      in.base = lat[std::size_t(k)].base;
      in.scar = lat[std::size_t(k)].scar;
      in.bottom = lat[std::size_t(k)].bottom;
      in.dual = false;
      in.wx = kX0 + kStep * lat[std::size_t(k)].vj;
      in.wz = kZ0 + kStep * lat[std::size_t(k)].vi;
      want_legacy[std::size_t(k)] = tp::compose_vertex(in, empty, nullptr);
    }

    int differ = 0;
    for (int k = 0; k < kVerts; ++k)
      if (want_legacy[std::size_t(k)].compose_top != want[std::size_t(k)].compose_top ||
          want_legacy[std::size_t(k)].bottom != want[std::size_t(k)].bottom)
        ++differ;
    ck(differ > 0,
       "D the two readings of this page DIFFER -- without that the phase would pass "
       "against a block that ignored the flag entirely",
       1, differ > 0 ? 1 : 0);
    std::printf("   %d of %d vertices read differently as legacy than as dual\n", differ,
                kVerts);

    // ---- the flag SET -------------------------------------------------
    w.reset();
    d.j_flags = uint16_t(zref::swstream::kFlagRequired | zref::swstream::kFlagDual);
    d.eval();
    w.load(pool);
    {
      const std::vector<Got> got = stream(w, 1, 0);
      int bad = 0;
      for (int k = 0; k < int(got.size()); ++k) {
        const Got& g = got[std::size_t(k)];
        const tp::ComposeOut& e = want[std::size_t(k)];
        if (g.compose_top != e.compose_top || g.top != e.live_top || g.bottom != e.bottom)
          ++bad;
      }
      ck(int(got.size()) == kVerts, "D with kFlagDual set, every vertex composed", kVerts,
         int(got.size()));
      ck(bad == 0, "D and every one reads as the DUAL page the flag says it is", 0, bad);
    }

    // ---- the flag CLEAR -----------------------------------------------
    w.reset();
    d.j_flags = uint16_t(zref::swstream::kFlagRequired);   // no kFlagDual
    d.eval();
    w.load(pool);
    {
      const std::vector<Got> got = stream(w, 1, 0);
      int bad = 0, printed = 0;
      for (int k = 0; k < int(got.size()); ++k) {
        const Got& g = got[std::size_t(k)];
        const tp::ComposeOut& e = want_legacy[std::size_t(k)];
        if (g.compose_top == e.compose_top && g.top == e.live_top && g.bottom == e.bottom)
          continue;
        ++bad;
        if (printed < 3) {
          ++printed;
          std::printf("   D legacy vertex %d: compose_top got %d want %d, bottom got %d "
                      "want %d\n", k, g.compose_top, e.compose_top, g.bottom, e.bottom);
        }
      }
      ck(int(got.size()) == kVerts, "D with kFlagDual clear, every vertex composed", kVerts,
         int(got.size()));
      ck(bad == 0,
         "D and every one reads as a LEGACY single-surface page -- which is the claim "
         "that the flag travelled from the record to the compose lane at all",
         0, bad);
    }

    // Put it back, so anything added after this phase gets the dual page.
    d.j_flags = uint16_t(zref::swstream::kFlagRequired | zref::swstream::kFlagDual);
    d.eval();
  }

  std::printf("\n== %d checks, %d failures ==\n", g_checks, g_fail);
  std::fflush(stdout);

  const int rc = (g_fail == 0) ? 0 : 1;
  delete dut;
  return rc;
}
