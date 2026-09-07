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
#include "zref/zref_terrain_tess.hpp"

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
// COLUMN (`vi`) and wz the ROW (`vj`) -- the tree's convention, which
// TERRAIN.COMPCACHE writes into hardware as `wx_m[rd_vi_c]` and
// `wz_m[rd_vj_c]`.
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

// A PAGE THAT MOVES THE GROUND IN ONE CORNER ONLY.
//
// The mask check needs a fixture whose answer is ASYMMETRIC under transposition,
// and the main page is no good for it: four vertices in five are dirty, spread
// over the whole lattice, so the mask is 0xFFFF and would pass with the indices
// swapped. That is the same shape of vacuous green the file's other phases warn
// about, and it is worth saying that the transposition bug this check exists for
// would have survived it.
//
// So: flat ground everywhere -- base == bottom, no scar, which composes to
// exactly `fx(base)` and is NOT dirty -- except a block of low COLUMNS at high
// ROWS, which lands in one corner of the 4x4 and nowhere near its transpose.
void fill_corner_page(Pool& p, uint32_t slot, int col_lo, int col_hi, int row_lo,
                      int row_hi) {
  for (int vj = 0; vj < kEdge; ++vj) {          // ROW
    for (int vi = 0; vi < kEdge; ++vi) {        // COLUMN
      const int k = vj * kEdge + vi;
      const bool moved = (vi >= col_lo && vi <= col_hi && vj >= row_lo && vj <= row_hi);
      p.put16(slot, tp::kLayerAOff + 2u * uint32_t(k), int16_t(1000));
      p.put16(slot, tp::kLayerBOff + 2u * uint32_t(k), int16_t(moved ? 400 : 0));
      p.put16(slot, tp::kLayerCOff + 2u * uint32_t(k), int16_t(1000));
    }
  }
}

// `sp_mask`, transcribed from `zhao_terrain_patch.sv:205` rather than
// paraphrased. A BORDER VERTEX MARKS BOTH NEIGHBOURS and a corner marks four --
// they are physically shared, which is the block's own chosen law 3.
uint16_t sp_mask(int vi, int vj) {
  const int col_lo = (vi == 0) ? 0 : ((vi - 1) >> 3);
  const int col_hi = ((vi >> 3) > 3) ? 3 : (vi >> 3);
  const int row_lo = (vj == 0) ? 0 : ((vj - 1) >> 3);
  const int row_hi = ((vj >> 3) > 3) ? 3 : (vj >> 3);
  uint16_t m = 0;
  for (int r = row_lo; r <= row_hi; ++r)
    for (int c = col_lo; c <= col_hi; ++c) m = uint16_t(m | (1u << (r * 4 + c)));
  return m;
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
    d.pos_we = 0;
    d.cc_serve_release = 0;
    d.cc_lat_req = 0;
    d.pt_list_clear = 0;
    d.cfg_tess_i = 0;
    d.ts_job_valid = 0;
    d.ts_tri_ready = 0;
    config();
    d.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(d);
    d.rst_n = 1;
    d.eval();
    for (int i = 0; i < 2; ++i) zhao::tick(d);
  }

  // THE PLACEMENT, INTO THE CACHE. It arrives on its own port rather than on
  // the record stream, because it is the island directory's and not the
  // composition's -- `zhao_terrain_compcache_front`'s own contract draws that
  // line. The bench writes the SAME law it tells TERRAIN.PATCH, so a bench that
  // wrote one and expected another fails rather than agreeing with itself.
  void write_placement() {
    for (int axis = 0; axis < 2; ++axis) {
      for (int i = 0; i < kEdge; ++i) {
        d.pos_we = 1;
        d.pos_axis = uint8_t(axis);
        d.pos_idx = uint8_t(i);
        d.pos_val = uint32_t(axis ? (kZ0 + kStep * i) : (kX0 + kStep * i));
        d.eval();
        zhao::tick(d);
      }
    }
    d.pos_we = 0;
    d.eval();
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
      x.vi = int(out.size()) % kEdge;   // COLUMN, the fast axis
      x.vj = int(out.size()) / kEdge;   // ROW
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

// READ ONE VERTEX BACK OUT OF THE CACHE. The datum is present the cycle AFTER
// the request -- `design/contracts/TERRAIN.TESS.md`, and this block's own port
// comment repeats it -- so the request and the capture are one tick apart. A
// helper that read on the same cycle would return the PREVIOUS request's answer
// and be wrong by exactly one vertex, everywhere, plausibly.
struct LatRead {
  int32_t h = 0, wx = 0, wz = 0;
};

LatRead lat_read(Vtb_terrain_compose& d, int vi, int vj, int surface) {
  d.cc_lat_req = 1;
  d.cc_lat_vi = uint8_t(vi);
  d.cc_lat_vj = uint8_t(vj);
  d.cc_lat_surface = uint8_t(surface);
  d.eval();
  zhao::tick(d);
  d.cc_lat_req = 0;
  d.eval();
  LatRead r;
  r.h = int32_t(d.cc_lat_h);
  r.wx = int32_t(d.cc_lat_wx);
  r.wz = int32_t(d.cc_lat_wz);
  return r;
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
    in.wx = kX0 + kStep * lat[std::size_t(k)].vi;   // COLUMN
    in.wz = kZ0 + kStep * lat[std::size_t(k)].vj;   // ROW
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
      in.wx = kX0 + kStep * lat2[std::size_t(k)].vi;
      in.wz = kZ0 + kStep * lat2[std::size_t(k)].vj;
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
      in.wx = kX0 + kStep * lat[std::size_t(k)].vi;   // COLUMN
      in.wz = kZ0 + kStep * lat[std::size_t(k)].vj;   // ROW
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


  // =========================================================================
  // E -- THE COMPOSED-HEIGHT CACHE, FILLED FROM A PAGE
  // =========================================================================
  // PAGESTREAM -> PATCH could be checked as the stream went past. The cache
  // cannot: it can only be checked by READING IT BACK, which is a different
  // question -- did the right value land at the right (vi, vj) -- and it is the
  // one a wrong write cursor, a wrong buffer parity or a wrong row stride would
  // fail while every counter agreed.
  //
  // So this phase fills the cache from the page and then reads every one of the
  // 1,089 vertices back on BOTH surfaces, against the same
  // `zref::terrain::compose_vertex` answers phase A used and against the
  // placement law the bench wrote into the position port.
  {
    std::printf("\n-- E: the composed-height cache, filled from a page --\n");
    w.reset();
    d.j_flags = uint16_t(zref::swstream::kFlagRequired | zref::swstream::kFlagDual);
    d.eval();
    w.load(pool);
    w.write_placement();

    const std::vector<Got> got = stream(w, 1, 0);
    ck(int(got.size()) == kVerts, "E the composed stream is complete", kVerts,
       int(got.size()));

    // THE LAST RECORD IS NOT THE LAST CLOCK. The cache's cursor reaches
    // LAT_W*LAT_H on the clock AFTER the record that completes the fill, and
    // the buffer swap happens there -- so `fill_records_o` reads one short and
    // the serve port still points at the OTHER buffer if either is read on the
    // cycle the stream ends. The first version of this phase did exactly that
    // and got 1,088 records, a serve port that was not valid, and vertex (0,0)
    // reading 1,538,125,837 out of a buffer nothing had written.
    //
    // A few ticks, not a settle loop: there is nothing to wait FOR here beyond
    // one clock, and a loop would hide a swap that never came.
    for (int i = 0; i < 4; ++i) zhao::tick(d);
    d.eval();

    ck(d.cc_fill_records == uint32_t(kVerts),
       "E the cache took every record the composer produced", kVerts,
       long(d.cc_fill_records));
    ck(d.cc_fill_overrun == 0,
       "E and refused none -- an overrun would mean a record was offered past the "
       "lattice's end, which is the shape of a write cursor that did not reset",
       0, long(d.cc_fill_overrun));
    ck(d.cc_patches_filled == 1, "E one patch filled", 1, long(d.cc_patches_filled));
    ck(d.cc_fill_done != 0 || d.cc_serve_valid != 0,
       "E and the fill completed", 1,
       (d.cc_fill_done != 0 || d.cc_serve_valid != 0) ? 1 : 0);
    ck(d.cc_serve_valid != 0,
       "E a complete patch is on the serve port -- the swap happened", 1,
       d.cc_serve_valid ? 1 : 0);

    // THE READBACK. Every vertex, both surfaces, height AND placement.
    int bad_h = 0, bad_pos = 0, printed = 0;
    for (int vj = 0; vj < kEdge; ++vj) {        // ROW
      for (int vi = 0; vi < kEdge; ++vi) {      // COLUMN
        const int k = vj * kEdge + vi;
        const tp::ComposeOut& e = want[std::size_t(k)];

        const LatRead top = lat_read(d, vi, vj, 0);
        const LatRead bot = lat_read(d, vi, vj, 1);

        if (top.h != e.live_top || bot.h != e.bottom) {
          ++bad_h;
          if (printed < 4) {
            ++printed;
            std::printf("   E (%d,%d) top got %d want %d, bottom got %d want %d\n", vi, vj,
                        top.h, e.live_top, bot.h, e.bottom);
          }
        }
        // The placement, which the cache stores per COLUMN and per ROW rather
        // than per vertex -- so a transposed store is exactly the fault this
        // catches, and it would leave every height correct.
        const int32_t want_wx = kX0 + kStep * vi;   // COLUMN
        const int32_t want_wz = kZ0 + kStep * vj;   // ROW
        if (top.wx != want_wx || top.wz != want_wz) {
          ++bad_pos;
          if (printed < 6) {
            ++printed;
            std::printf("   E (%d,%d) wx got %d want %d, wz got %d want %d\n", vi, vj,
                        top.wx, want_wx, top.wz, want_wz);
          }
        }
      }
    }
    ck(bad_h == 0,
       "E every vertex reads back the height section 3.4 gives for the page's own bytes "
       "-- on BOTH surfaces, at the coordinate it was written to",
       0, bad_h);
    ck(bad_pos == 0,
       "E and at the world position the placement law puts it -- wx follows vi, the "
       "COLUMN, and wz follows vj, the ROW. A transposed store fails here while every "
       "height stays right, which is exactly how this bench found the streamer had the "
       "two the wrong way round",
       0, bad_pos);
    ck(d.cc_lat_oob == 0, "E with no request outside the grid", 0, long(d.cc_lat_oob));

    // AND THE SERVED PATCH IS NAMED. `serve_src_id_o` is the only thing on the
    // serve side that says WHICH patch, which is the whole reason this block
    // carries it: every other signal is self-consistent under a swap that did
    // not happen.
    std::printf("   %u records filled, serve_src_id = 0x%04X, %u served\n",
                d.cc_fill_records, d.cc_serve_src_id, d.cc_patches_served);

    // RETIRE IT, and the count must move. A release that retired nothing would
    // leave the next fill with no buffer and the failure would surface a patch
    // later, somewhere else.
    const uint32_t served_before = d.cc_patches_served;
    d.cc_serve_release = 1;
    d.eval();
    zhao::tick(d);
    d.cc_serve_release = 0;
    d.eval();
    zhao::tick(d);
    d.eval();
    ck(d.cc_patches_served == served_before + 1,
       "E and releasing it retires exactly one patch", long(served_before + 1),
       long(d.cc_patches_served));
  }


  // =========================================================================
  // F -- THE SUBPATCH DIRTY MASK, WHICH IS WHAT THE TRANSPOSITION BROKE
  // =========================================================================
  // Phase E found that this block's `vi`/`vj` were swapped against the tree's
  // convention. The heights survived it -- COMPCACHE stores by arrival cursor --
  // and the ONE thing that did not is `subpatch_dirty_o`, a 4x4 mask at bit
  // `row*4 + col`. Transposed, it requests the wrong quarter of a patch:
  // terrain_rules §4.4's "dirty patches only" pointed at ground that did not
  // move, and the ground that did left unrequested.
  //
  // THE FIX WENT IN WITH NO TEST FOR IT. This is that test.
  //
  // The main page cannot serve: four vertices in five are dirty over the whole
  // lattice, so its mask is 0xFFFF and would have passed transposed. This page
  // moves the ground in ONE CORNER only -- low columns, high rows -- so the
  // correct answer and its transpose share no bits at all.
  {
    std::printf("\n-- F: the subpatch dirty mask --\n");
    constexpr int kColLo = 0, kColHi = 7, kRowLo = 24, kRowHi = 32;
    fill_corner_page(pool, 3, kColLo, kColHi, kRowLo, kRowHi);

    // The expected mask, from the same law the RTL states, over the vertices
    // the reference calls dirty.
    uint16_t want_mask = 0;
    int dirty_verts = 0;
    for (int vj = 0; vj < kEdge; ++vj) {
      for (int vi = 0; vi < kEdge; ++vi) {
        tp::ComposeIn in;
        in.base = 1000;
        in.scar = int16_t((vi >= kColLo && vi <= kColHi && vj >= kRowLo && vj <= kRowHi)
                              ? 400 : 0);
        in.bottom = 1000;
        in.dual = true;
        in.wx = kX0 + kStep * vi;
        in.wz = kZ0 + kStep * vj;
        const tp::ComposeOut e = tp::compose_vertex(in, empty, nullptr);
        if (!e.dirty) continue;
        ++dirty_verts;
        want_mask = uint16_t(want_mask | sp_mask(vi, vj));
      }
    }

    // THE FIXTURE MUST DISTINGUISH THE TRANSPOSE, asserted before it is used.
    uint16_t transposed = 0;
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        if (want_mask & (1u << (r * 4 + c))) transposed = uint16_t(transposed | (1u << (c * 4 + r)));
    ck(want_mask != 0 && want_mask != 0xFFFFu && (want_mask & transposed) == 0,
       "F the expected mask is asymmetric and shares NO bit with its transpose -- "
       "otherwise this phase would pass against the very bug it exists for",
       1, (want_mask != 0 && want_mask != 0xFFFFu && (want_mask & transposed) == 0) ? 1 : 0);
    std::printf("   %d dirty vertices; mask want 0x%04X, its transpose 0x%04X\n",
                dirty_verts, want_mask, transposed);

    w.reset();
    d.j_flags = uint16_t(zref::swstream::kFlagRequired | zref::swstream::kFlagDual);
    d.eval();
    w.load(pool);
    w.write_placement();

    // CLEAR THE MASK BEFORE THE FIRST RECORD, which is what the block's
    // contract asks for. Without it the mask still carries phase E's page and
    // reads 0xFFFF -- a pass that would mean nothing.
    d.pt_list_clear = 1;
    d.eval();
    zhao::tick(d);
    d.pt_list_clear = 0;
    d.eval();

    const std::vector<Got> got = stream(w, 3, 0);
    for (int i = 0; i < 4; ++i) zhao::tick(d);
    d.eval();

    ck(int(got.size()) == kVerts, "F the corner page composed in full", kVerts,
       int(got.size()));

    int got_dirty = 0;
    for (const Got& g : got) if (g.dirty) ++got_dirty;
    ck(got_dirty == dirty_verts,
       "F and exactly the vertices the reference calls dirty came back dirty",
       dirty_verts, got_dirty);

    ck(uint16_t(d.subpatch_dirty) == want_mask,
       "F THE 4x4 MASK IS THE ONE THE LAW GIVES -- bit row*4 + col, with border "
       "vertices marking both neighbours. This is the check the vi/vj transposition "
       "would have failed, and the one that did not exist when it was fixed",
       long(want_mask), long(d.subpatch_dirty));
    std::printf("   mask got 0x%04X\n", uint16_t(d.subpatch_dirty));
  }


  // =========================================================================
  // G -- PAGE BYTES TO TRIANGLES
  // =========================================================================
  // `zhao_terrain_tess`'s `lat_*` port is port-for-port TERRAIN.COMPCACHE's
  // serve side, and its `cs_*` port is that block's cell-state read, so the two
  // go together with no adapter. THAT IS THE CLAIM: the one-cycle read latency
  // is the cache's own contract and TESS was built to it, and this bench does
  // not bridge anything.
  //
  // What comes out is compared against `zref::terrain::tessellate` fed a
  // `ComposedLattice` built from THE SAME PAGE -- so the statement is the whole
  // path in one line: the triangles the machine emits for a 21,376-byte page
  // are the triangles the reference emits for that page's composed lattice.
  //
  // The JOB is driven from the bench, because choosing it is TERRAIN.LOD's
  // decision and LOD is a separate block with its own reference. Composing the
  // selector too would put two claims in one phase and make a failure ambiguous.
  {
    std::printf("\n-- G: page bytes to triangles --\n");

    w.reset();
    d.j_flags = uint16_t(zref::swstream::kFlagRequired | zref::swstream::kFlagDual);
    d.eval();
    w.load(pool);
    w.write_placement();

    d.pt_list_clear = 1; d.eval(); zhao::tick(d); d.pt_list_clear = 0; d.eval();
    const std::vector<Got> filled = stream(w, 1, 0);
    for (int i = 0; i < 4; ++i) zhao::tick(d);
    d.eval();
    ck(int(filled.size()) == kVerts, "G the cache is filled from the page", kVerts,
       int(filled.size()));

    // The reference's view of the SAME lattice: placement by the same law, and
    // heights from the same `compose_vertex` answers phase A checked.
    tp::ComposedLattice ref;
    ref.w = kEdge;
    ref.h = kEdge;
    ref.dual = true;
    ref.wx.resize(std::size_t(kEdge));
    ref.wz.resize(std::size_t(kEdge));
    for (int i = 0; i < kEdge; ++i) {
      ref.wx[std::size_t(i)] = kX0 + kStep * i;   // by COLUMN
      ref.wz[std::size_t(i)] = kZ0 + kStep * i;   // by ROW
    }
    ref.top.resize(std::size_t(kVerts));
    ref.bottom.resize(std::size_t(kVerts));
    for (int k = 0; k < kVerts; ++k) {
      ref.top[std::size_t(k)] = want[std::size_t(k)].live_top;
      ref.bottom[std::size_t(k)] = want[std::size_t(k)].bottom;
    }

    // ONE SUBPATCH, LEVEL 0, NO STITCHING, NO MORPH. The simplest job there is,
    // deliberately: TESS's stitching and geomorph laws have their own
    // differential, and what is new here is only where the lattice CAME FROM.
    tp::SubpatchJob job;
    job.ox = 8;
    job.oz = 16;
    job.level = 0;
    job.nlevel[0] = job.nlevel[1] = job.nlevel[2] = job.nlevel[3] = 0;
    job.morph = 0;
    job.surface = tp::Surface::kTop;
    const tp::TessResult want_tess = tp::tessellate(ref, job);
    ck(!want_tess.tris.empty(),
       "G the reference emits triangles for this job -- a job that produced none would "
       "make every check below vacuous",
       1, want_tess.tris.empty() ? 0 : 1);

    // ---- hand it to the machine ---------------------------------------
    d.cfg_tess_i = 1;
    d.ts_job_ox = 8;
    d.ts_job_oz = 16;
    d.ts_job_level = 0;
    d.ts_job_lvl_nz = 0;
    d.ts_job_lvl_pz = 0;
    d.ts_job_lvl_nx = 0;
    d.ts_job_lvl_px = 0;
    d.ts_job_morph = 0;
    d.ts_job_surface = 0;
    d.ts_job_src_id = 0x00A5;
    d.ts_job_valid = 1;
    d.ts_tri_ready = 1;
    d.eval();
    int g = 0;
    while (!d.ts_job_ready && g < 2000) { zhao::tick(d); d.eval(); ++g; }
    zhao::tick(d);
    d.ts_job_valid = 0;
    d.eval();

    std::vector<tp::MeshTri> got_tris;
    int quiet = 0;
    for (int c = 0; c < 200000 && quiet < 500; ++c) {
      d.eval();
      if (d.ts_tri_valid && d.ts_tri_ready) {
        tp::MeshTri t;
        t.ax = int32_t(d.ts_ax); t.ay = int32_t(d.ts_ay); t.az = int32_t(d.ts_az);
        t.bx = int32_t(d.ts_bx); t.by = int32_t(d.ts_by); t.bz = int32_t(d.ts_bz);
        t.cx = int32_t(d.ts_cx); t.cy = int32_t(d.ts_cy); t.cz = int32_t(d.ts_cz);
        got_tris.push_back(t);
        quiet = 0;
      } else {
        ++quiet;
      }
      zhao::tick(d);
    }
    d.ts_tri_ready = 0;
    d.cfg_tess_i = 0;
    d.eval();

    std::printf("   reference %zu triangles, machine %zu\n", want_tess.tris.size(),
                got_tris.size());
    ck(got_tris.size() == want_tess.tris.size(),
       "G the machine emitted the reference's triangle count",
       long(want_tess.tris.size()), long(got_tris.size()));

    int bad = 0, printed = 0;
    const std::size_t n = got_tris.size() < want_tess.tris.size() ? got_tris.size()
                                                                 : want_tess.tris.size();
    for (std::size_t i = 0; i < n; ++i) {
      const tp::MeshTri& a = got_tris[i];
      const tp::MeshTri& e = want_tess.tris[i];
      if (a.ax == e.ax && a.ay == e.ay && a.az == e.az && a.bx == e.bx && a.by == e.by &&
          a.bz == e.bz && a.cx == e.cx && a.cy == e.cy && a.cz == e.cz)
        continue;
      ++bad;
      if (printed < 3) {
        ++printed;
        std::printf("   G triangle %zu:\n", i);
        std::printf("      got  A(%d,%d,%d) B(%d,%d,%d) C(%d,%d,%d)\n", a.ax, a.ay, a.az,
                    a.bx, a.by, a.bz, a.cx, a.cy, a.cz);
        std::printf("      want A(%d,%d,%d) B(%d,%d,%d) C(%d,%d,%d)\n", e.ax, e.ay, e.az,
                    e.bx, e.by, e.bz, e.cx, e.cy, e.cz);
      }
    }
    ck(bad == 0,
       "G AND EVERY TRIANGLE IS THE REFERENCE'S, corner for corner and in order -- the "
       "whole path in one claim: a 21,376-byte page, streamed, composed by section 3.4, "
       "held in the cache and tessellated, with no adapter anywhere in it",
       0, bad);
    ck(uint16_t(d.ts_src_id) == 0x00A5,
       "G and the job's source id rode through to the mesh", 0x00A5,
       long(d.ts_src_id));
  }

  std::printf("\n== %d checks, %d failures ==\n", g_checks, g_fail);
  std::fflush(stdout);

  const int rc = (g_fail == 0) ? 0 : 1;
  delete dut;
  return rc;
}
