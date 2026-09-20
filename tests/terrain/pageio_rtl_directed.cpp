// pageio_rtl_directed.cpp -- zhao_terrain_pageio against the page image and
// against zref::terrain::nobake_corner_shadow.
//
// ===========================================================================
// WHAT IS AT RISK, AND WHY A COUNT CANNOT SEE ANY OF IT
// ===========================================================================
// This block reads layer D out of a page and writes layers B and D back into
// it. Every way it can be wrong moves exactly the same number of bursts:
//
//   * THE FOREIGN BYTES. Layer B starts 2 bytes into burst 35 and ends 60
//     bytes before the end of burst 69; layer D starts 6 bytes into burst 103
//     and ends 58 bytes before the end of burst 119. The guard has NO byte
//     enables (`be_ok = (req.be == mask_of(req.len))`), so a whole burst is
//     the smallest thing that can be written, and a block that wrote those
//     bursts from a zero-filled buffer would destroy layer A's last 2 bytes,
//     layer C's first 4 and last 6, and layer E's first 58. NOTHING IN THE
//     MACHINE READS LAYERS A, C OR E BACK, so nothing would ever notice. This
//     bench reads the image back and asserts they are BYTE-IDENTICAL.
//
//   * THE WRONG SLOT. A neighbouring page's scar, in the right shape. Slots
//     0, 2 and 3 are filled from a different salt and asserted unchanged.
//
//   * THE WRONG LANE. Layer B's samples are 16 bits at page byte 2242 + 2k,
//     which is buffer byte 2 + 2k -- an ODD-INDEXED halfword inside the
//     aligned window. An implementation that forgot the 2-byte lead writes
//     every scar one vertex early, and every scar is still a scar.
//
//   * THE SCATTER. `nb_o` is built by scattering each protected cell onto the
//     four vertices it corners; the oracle GATHERS the four cells of each
//     vertex. They are the same law and this is where that is checked.
//
//   * ONE LAYER-D BUFFER SERVING BOTH DIRECTIONS (contract section 5's
//     "further halving"). If a cell were re-read after it was written the
//     block would hand TERRAIN.BAKE the NEW substance while section 3.4 was
//     still deciding, which changes breach decisions only -- the exact fault
//     class no counter can see. The breach drive here interleaves a cell read
//     of k+1 with the cs write of k, exactly as TERRAIN.BAKE does, and
//     asserts every read returns the ORIGINAL byte.
//
//   * THE HELD CELL ANSWER. `cell_ci_i`/`cell_cj_i` are bake's LIVE cursor.
//     Delivering a held byte against a moved cursor is cell A's data under
//     cell B's address with every handshake agreeing -- CLAUDE.md's
//     metadata-swap defect. Driven deliberately below; `cell_refetch_o` must
//     move, and it must read ZERO on the clean path.
//
// ===========================================================================
// EVERY COUNTER IS FIRED, AND EVERY COUNTER IS ALSO PROVED SILENT
// ===========================================================================
// A detector reading zero is a claim. So each of `guard_denied_o`,
// `stale_gen_o`, `jobs_refused_o`, `nobake_mutated_o` and `cell_refetch_o` is
// asserted ZERO on the clean bake (the negative control) and then FIRED by
// stimulus in its own case (the positive control). None of them needs a
// committed mutant: at THIS block's boundary the illegal input is legal
// stimulus, because the bench is standing where TERRAIN.BAKE will stand.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_pageio.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_page.hpp"

namespace {

namespace tp = zref::terrain;

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
    std::fflush(stdout);
  }
}

void ck(bool ok, const char* what, long expect, long got) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %ld, got %ld)\n", what, expect, got);
    std::fflush(stdout);
  }
}

constexpr uint32_t kPageBytes = tp::kPageBytes;  // 21,376
constexpr uint32_t kPageWords = kPageBytes / 8;  // 2,672
constexpr uint32_t kSlots = 4;
constexpr uint32_t kPoolBase = 0x04000000u;  // ruling T2
constexpr int kEdge = 33;
constexpr int kCells = 32;
constexpr int kVerts = kEdge * kEdge;   // 1,089
constexpr int kNCell = kCells * kCells; // 1,024

constexpr uint32_t kSlotUT = 1;  // the slot under test

// Verdicts, mirroring the RTL's localparams.
constexpr int V_OK = 0, V_SLOT_OOR = 1, V_EPOCH = 2, V_GUARD = 3;
constexpr int V_INCOMPLETE = 4, V_SHORT_B = 5;

// ---------------------------------------------------------------------------
// THE PAGE IMAGE
// ---------------------------------------------------------------------------
struct Pool {
  std::vector<uint8_t> b;
  Pool() : b(kSlots * kPageBytes, 0) {}

  void put16(uint32_t slot, uint32_t off, int16_t v) {
    const uint32_t a = slot * kPageBytes + off;
    b[a] = uint8_t(uint16_t(v) & 0xFF);
    b[a + 1] = uint8_t((uint16_t(v) >> 8) & 0xFF);
  }
  int16_t get16(uint32_t slot, uint32_t off) const {
    const uint32_t a = slot * kPageBytes + off;
    return int16_t(uint16_t(b[a]) | (uint16_t(b[a + 1]) << 8));
  }
  uint8_t get8(uint32_t slot, uint32_t off) const { return b[slot * kPageBytes + off]; }
};

// The layer-D cell byte the fixture starts from. ~8% of cells carry
// kNoBakeBit, which puts the corner shadow at roughly 28% of vertices -- a
// mix, rather than the all-ones a 50% plane would produce and which would pass
// against an `nb_o` tied high.
uint8_t cell_fixture(uint32_t slot, int k) {
  const uint32_t h = (uint32_t(k) * 2654435761u) ^ (slot * 0x9E3779B9u);
  uint8_t st = uint8_t(((k * 53 + 7) & 0xF8) | uint8_t(k % 3));
  if (((h >> 12) % 12u) == 0u) st |= tp::kNoBakeBit;
  else                         st = uint8_t(st & uint8_t(~tp::kNoBakeBit));
  return st;
}

// What "bake" writes back: substance := kVoidBreached, FLAGS PRESERVED, which
// is `zhao_terrain_bake_v2`'s own `cs_state_o <= {cell_state_i[7:2], sub_out}`.
uint8_t cell_baked(uint8_t orig) {
  return uint8_t((orig & uint8_t(~tp::kSubstanceMask)) | tp::kVoidBreached);
}

// What "bake" writes into layer B. Negative and distinct per vertex: height16
// is SIGNED and a zero-extending write passes every test drawn from positive
// values.
int16_t scar_baked(int k) { return int16_t(-(0x0300 + 11 * k)); }

// `salt` moves every plane of a page, so two slots never share a value.
void fill_page(Pool& p, uint32_t slot, int salt) {
  for (int k = 0; k < kVerts; ++k) {
    p.put16(slot, tp::kLayerAOff + 2u * uint32_t(k), int16_t(0x1000 + 3 * k + salt * 0x0037));
    p.put16(slot, tp::kLayerBOff + 2u * uint32_t(k), int16_t(0x0500 + 5 * k + salt * 0x0011));
    p.put16(slot, tp::kLayerCOff + 2u * uint32_t(k), int16_t(0x4000 + 7 * k + salt * 0x0071));
  }
  for (int k = 0; k < kNCell; ++k)
    p.b[slot * kPageBytes + tp::kLayerDOff + uint32_t(k)] = cell_fixture(slot, k);
  // Layers E..H and the header, made loud. A cursor that ran past its plane
  // produces something the comparison cannot mistake for anything legitimate.
  for (uint32_t off = tp::kLayerEOff; off < kPageBytes; ++off)
    p.b[slot * kPageBytes + off] = uint8_t(0xE0 + ((off + uint32_t(salt)) & 0x1F));
  for (uint32_t off = 0; off < tp::kLayerAOff; ++off)
    p.b[slot * kPageBytes + off] = uint8_t(0xD0 + ((off + uint32_t(salt)) & 0x0F));
}

// ---------------------------------------------------------------------------
// THE WORLD
// ---------------------------------------------------------------------------
struct World {
  Vtb_pageio& d;
  explicit World(Vtb_pageio& dd) : d(dd) {}

  void quiet() {
    d.mw_en = 0;
    d.mr_addr = 0;
    d.j_valid = 0;
    d.nb_req = 0;
    d.nb_vi = 0;
    d.nb_vj = 0;
    d.cell_ci = 0;
    d.cell_cj = 0;
    d.cell_ready = 0;
    d.sc_valid = 0;
    d.sc_scar = 0;
    d.sc_vi = 0;
    d.sc_vj = 0;
    d.cs_valid = 0;
    d.cs_state = 0;
    d.cs_ci = 0;
    d.cs_cj = 0;
    d.dig_done = 0;
    d.bake_done = 0;
    d.dm_ready = 1;
    d.done_ready = 1;
    d.stat_clear_i = 0;
  }

  void config() {
    d.cfg_vram_window_base_i = kPoolBase;
    d.cfg_grant_hold_i = 1;
    d.cfg_rd_latency_i = 2;
    d.cfg_rd_gap_i = 0;
    d.cfg_wr_latency_i = 2;
    d.cfg_wr_gap_i = 0;
    d.cfg_region_ok_i = 1;
    d.cfg_deny_mode_i = 0;
    d.cfg_deny_idx_i = 0;
    d.cfg_short_mode_i = 0;
    d.cfg_short_idx_i = 0;
    d.cfg_short_beat_i = 0;
    d.cfg_vram_client_i = 6;  // ZHAO_CLIENT_TERRAIN_BUILD, ruling T3
    d.cfg_epoch_i = 0x11u;
  }

  void reset() {
    d.rst_n = 0;
    quiet();
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
      for (int byte = 0; byte < 8; ++byte) v |= uint64_t(p.b[w * 8 + uint32_t(byte)]) << (8 * byte);
      d.mw_en = 1;
      d.mw_addr = uint16_t(w);
      d.mw_data = v;
      d.eval();
      zhao::tick(d);
    }
    d.mw_en = 0;
    d.eval();
  }

  // Read the image back. THIS IS THE ACCEPTANCE TEST for section 4's
  // read-modify-write, and nothing else in the machine can do it.
  void readback(Pool& p) {
    for (uint32_t w = 0; w < kSlots * kPageWords; ++w) {
      d.mr_addr = uint16_t(w);
      d.eval();
      zhao::tick(d);
      const uint64_t v = d.mr_data;
      for (int byte = 0; byte < 8; ++byte)
        p.b[w * 8 + uint32_t(byte)] = uint8_t((v >> (8 * byte)) & 0xFF);
    }
  }

  bool offer_job(uint32_t slot, uint32_t gen, uint32_t epoch, uint32_t src) {
    d.j_valid = 1;
    d.j_slot = uint16_t(slot);
    d.j_gen = uint8_t(gen);
    d.j_epoch = epoch;
    d.j_src_id = src;
    d.eval();
    int guard = 0;
    while (!d.j_ready && guard < 2000) {
      zhao::tick(d);
      d.eval();
      ++guard;
    }
    const bool got = d.j_ready != 0;
    zhao::tick(d);
    d.j_valid = 0;
    d.eval();
    return got;
  }

  // Spin until `done_valid`, consuming the deformation mark on the way.
  bool settle(int& verdict, int& ok, uint64_t cap = 2000000ull) {
    for (uint64_t c = 0; c < cap; ++c) {
      d.eval();
      if (d.done_valid) {
        verdict = int(d.done_verdict);
        ok = int(d.done_ok);
        zhao::tick(d);
        return true;
      }
      zhao::tick(d);
    }
    return false;
  }
};

// ---------------------------------------------------------------------------
// THE BAKE DRIVE -- standing exactly where zhao_terrain_bake_v2 will stand
// ---------------------------------------------------------------------------
struct BakeResult {
  int nb_mismatch = 0;
  int cell_mismatch = 0;
  bool stalled = false;
  int dm_seen = 0;
  uint32_t dm_slot = 0, dm_gen = 0, dm_epoch = 0;
  int dm_bd = 0, dm_f = 0, dm_mips = 0;
};

// Wait until the block is serving: `sc_ready` is high only in S_SERVE with no
// buffer operation in flight, so it is the honest "the page window is open".
bool wait_serving(World& w, uint64_t cap = 200000ull) {
  for (uint64_t c = 0; c < cap; ++c) {
    w.d.eval();
    if (w.d.sc_ready) return true;
    zhao::tick(w.d);
  }
  return false;
}

// Collect the mark whenever it is offered. `dm_ready` is held high.
void pump_mark(World& w, BakeResult& r) {
  if (w.d.dm_valid && w.d.dm_ready) {
    ++r.dm_seen;
    r.dm_slot = uint32_t(w.d.dm_slot);
    r.dm_gen = uint32_t(w.d.dm_gen);
    r.dm_epoch = uint32_t(w.d.dm_epoch);
    r.dm_bd = int(w.d.dm_bd);
    r.dm_f = int(w.d.dm_f);
    r.dm_mips = int(w.d.dm_mips);
  }
}

// `dig_verts` short of kVerts leaves layer B PART-FILLED on purpose, which is
// the V_SHORT_B case. `mutate_cell` >= 0 flips kNoBakeBit on that one cs write,
// which is the `nobake_mutated_o` positive control.
BakeResult run_bake(World& w, const Pool& before, int dig_verts = kVerts,
                    int mutate_cell = -1, bool do_breach = true) {
  Vtb_pageio& d = w.d;
  BakeResult r;

  if (!wait_serving(w)) {
    r.stalled = true;
    return r;
  }

  // ---- DIG: read nb_o for every vertex, write every scar -----------------
  // The cell cursor is parked at (0,0) for the whole dig sweep, which is what
  // TERRAIN.BAKE does: `cell_ci_o`/`cell_cj_o` are its `ci`/`cj` registers and
  // they do not advance until the breach phase.
  d.cell_ci = 0;
  d.cell_cj = 0;
  d.cell_ready = 0;
  d.nb_req = 1;

  std::vector<uint8_t> dfix(static_cast<size_t>(kNCell), uint8_t(0));
  for (int k = 0; k < kNCell; ++k) dfix[size_t(k)] = before.get8(kSlotUT, tp::kLayerDOff + uint32_t(k));

  for (int k = 0; k < dig_verts; ++k) {
    const int vi = k % kEdge, vj = k / kEdge;

    d.nb_vi = uint8_t(vi);
    d.nb_vj = uint8_t(vj);
    d.eval();
    const bool want = tp::nobake_corner_shadow(dfix.data(), kCells, kCells, vi, vj);
    if (int(d.nb_out) != (want ? 1 : 0)) ++r.nb_mismatch;

    d.sc_valid = 1;
    d.sc_vi = uint8_t(vi);
    d.sc_vj = uint8_t(vj);
    d.sc_scar = uint16_t(scar_baked(k));
    d.eval();
    int guard = 0;
    while (!d.sc_ready && guard < 5000) {
      pump_mark(w, r);
      zhao::tick(d);
      d.eval();
      ++guard;
    }
    if (guard >= 5000) { r.stalled = true; return r; }
    zhao::tick(d);
    d.sc_valid = 0;
    d.eval();
  }
  d.nb_req = 0;
  d.eval();

  d.dig_done = 1;
  d.eval();
  zhao::tick(d);
  d.dig_done = 0;
  d.eval();

  // ---- BREACH: read cell k, advance the cursor, write cell k -------------
  // THE CURSOR ADVANCES AT THE ACCEPT, which is what `zhao_terrain_bake_v2`
  // does, so the block is reading cell k+1 while the cs write for cell k is
  // still pending. That interleave is the whole test of the single layer-D
  // buffer: every read must return the ORIGINAL byte.
  if (do_breach) {
    for (int k = 0; k < kNCell; ++k) {
      const int ci = k % kCells, cj = k / kCells;

      d.cell_ci = uint8_t(ci);
      d.cell_cj = uint8_t(cj);
      d.cell_ready = 1;
      d.eval();
      int guard = 0;
      while (!d.cell_valid && guard < 5000) {
        pump_mark(w, r);
        zhao::tick(d);
        d.eval();
        ++guard;
      }
      if (guard >= 5000) { r.stalled = true; return r; }
      const uint8_t got = uint8_t(d.cell_state);
      if (got != dfix[size_t(k)]) ++r.cell_mismatch;
      zhao::tick(d);
      d.cell_ready = 0;

      // Advance the cursor to k+1 BEFORE the write of k retires, exactly as
      // bake does.
      const int nk = (k + 1 < kNCell) ? (k + 1) : k;
      d.cell_ci = uint8_t(nk % kCells);
      d.cell_cj = uint8_t(nk / kCells);

      uint8_t put = cell_baked(dfix[size_t(k)]);
      if (k == mutate_cell) put = uint8_t(put ^ tp::kNoBakeBit);
      d.cs_valid = 1;
      d.cs_ci = uint8_t(ci);
      d.cs_cj = uint8_t(cj);
      d.cs_state = put;
      d.eval();
      guard = 0;
      while (!d.cs_ready && guard < 5000) {
        pump_mark(w, r);
        zhao::tick(d);
        d.eval();
        ++guard;
      }
      if (guard >= 5000) { r.stalled = true; return r; }
      zhao::tick(d);
      d.cs_valid = 0;
      d.eval();
    }
  }

  d.bake_done = 1;
  d.eval();
  zhao::tick(d);
  d.bake_done = 0;
  d.eval();

  // Drain: the write phase runs on its own from here, and the mark is offered
  // somewhere inside it.
  for (int c = 0; c < 400000 && !d.done_valid; ++c) {
    pump_mark(w, r);
    zhao::tick(d);
    d.eval();
  }
  pump_mark(w, r);
  return r;
}

// ---------------------------------------------------------------------------
// CASES
// ---------------------------------------------------------------------------
void case_clean(Vtb_pageio& dut) {
  World w(dut);
  Pool before;
  for (uint32_t s = 0; s < kSlots; ++s) fill_page(before, s, int(s) + 1);

  w.reset();
  w.load(before);
  ck(w.offer_job(kSlotUT, 0x5Au, 0x11u, 0xC0FFEEu), "clean: the job was accepted");

  BakeResult r = run_bake(w, before);
  ck(!r.stalled, "clean: the bake face did not stall");
  if (r.stalled) return;

  int verdict = -1, ok = -1;
  ck(w.settle(verdict, ok), "clean: a completion arrived");
  ck(verdict == V_OK, "clean: verdict", V_OK, verdict);
  ck(ok == 1, "clean: done_ok", 1, ok);

  // ---- the counters, every one ------------------------------------------
  ck(dut.c_pages_read == 1u, "clean: pages_read", 1, long(dut.c_pages_read));
  ck(dut.c_pages_written == 1u, "clean: pages_written", 1, long(dut.c_pages_written));
  // 17 layer-D bursts + 2 layer-B edge bursts. The edge reads are the whole
  // reason layers A and C survive; if they were dropped this number would fall
  // to 17 and the foreign-byte checks below would go red together.
  ck(dut.c_bursts_read == 19u, "clean: bursts_read (17 D + 2 B edge)", 19,
     long(dut.c_bursts_read));
  // 35 layer-B bursts + 17 layer-D bursts.
  ck(dut.c_bursts_written == 52u, "clean: bursts_written (35 B + 17 D)", 52,
     long(dut.c_bursts_written));
  ck(dut.c_marks == 1u, "clean: marks_emitted", 1, long(dut.c_marks));

  // ---- THE NEGATIVE CONTROLS. Each of these is FIRED in its own case -----
  ck(dut.c_guard_denied == 0u, "clean: guard_denied silent", 0, long(dut.c_guard_denied));
  ck(dut.c_stale_gen == 0u, "clean: stale_gen silent", 0, long(dut.c_stale_gen));
  ck(dut.c_refused == 0u, "clean: jobs_refused silent", 0, long(dut.c_refused));
  ck(dut.c_nobake_mutated == 0u, "clean: nobake_mutated silent", 0,
     long(dut.c_nobake_mutated));
  ck(dut.c_cell_refetch == 0u, "clean: cell_refetch silent", 0, long(dut.c_cell_refetch));

  // ---- the real guard admitted every request ----------------------------
  ck(dut.shadow_viol == 0u, "clean: the REAL guard raised no violation", 0,
     long(dut.shadow_viol));
  ck(dut.shadow_ok == dut.shadow_req, "clean: the REAL guard passed every request",
     long(dut.shadow_req), long(dut.shadow_ok));
  ck(dut.shadow_fwd == dut.shadow_req, "clean: the REAL guard forwarded every request",
     long(dut.shadow_req), long(dut.shadow_fwd));

  // ---- the bake face ----------------------------------------------------
  ck(r.nb_mismatch == 0, "clean: nb_o matched zref::terrain::nobake_corner_shadow on all "
                         "1,089 vertices (scatter == gather)", 0, r.nb_mismatch);
  ck(r.cell_mismatch == 0, "clean: every cell read returned the ORIGINAL byte "
                           "(one layer-D buffer, read before write)", 0, r.cell_mismatch);

  // ---- the deformation mark ---------------------------------------------
  ck(r.dm_seen == 1, "clean: exactly one deformation mark", 1, r.dm_seen);
  ck(r.dm_slot == kSlotUT, "clean: dm_slot", long(kSlotUT), long(r.dm_slot));
  // OWNER DECISION 2: a bake does NOT bump the generation.
  ck(r.dm_gen == 0x5Au, "clean: dm_gen is the job's, unbumped", 0x5A, long(r.dm_gen));
  ck(r.dm_epoch == 0x11u, "clean: dm_epoch", 0x11, long(r.dm_epoch));
  ck(r.dm_bd == 1, "clean: dm_bd -- layers B and D moved", 1, r.dm_bd);
  ck(r.dm_f == 0, "clean: dm_f -- a bake does not touch the F sheet", 0, r.dm_f);

  // ---- THE PAGE, READ BACK ----------------------------------------------
  Pool after;
  w.readback(after);

  int b_bad = 0, d_bad = 0;
  for (int k = 0; k < kVerts; ++k)
    if (after.get16(kSlotUT, tp::kLayerBOff + 2u * uint32_t(k)) != scar_baked(k)) ++b_bad;
  for (int k = 0; k < kNCell; ++k)
    if (after.get8(kSlotUT, tp::kLayerDOff + uint32_t(k)) !=
        cell_baked(before.get8(kSlotUT, tp::kLayerDOff + uint32_t(k)))) ++d_bad;
  ck(b_bad == 0, "clean: layer B holds every baked scar", 0, b_bad);
  ck(d_bad == 0, "clean: layer D holds every baked cell state", 0, d_bad);

  // THE HEADLINE. Layers A, C and E and the header are untouched, byte for
  // byte -- and each of them SHARES A BURST with a plane this block writes.
  auto span_same = [&](const char* what, uint32_t lo, uint32_t hi) {
    int bad = 0;
    for (uint32_t off = lo; off < hi; ++off)
      if (after.get8(kSlotUT, off) != before.get8(kSlotUT, off)) ++bad;
    ck(bad == 0, what, 0, bad);
  };
  span_same("clean: the 64-byte page header is byte-identical", 0, tp::kLayerAOff);
  span_same("clean: LAYER A is byte-identical (it shares burst 35 with layer B)",
            tp::kLayerAOff, tp::kLayerBOff);
  span_same("clean: LAYER C is byte-identical (it shares bursts 69 and 103)",
            tp::kLayerCOff, tp::kLayerDOff);
  span_same("clean: LAYER E is byte-identical (it shares burst 119 with layer D)",
            tp::kLayerEOff, tp::kLayerFOff);
  span_same("clean: layers F, G and H are byte-identical", tp::kLayerFOff, kPageBytes);

  // ---- and the neighbouring slots ---------------------------------------
  int nb_bad = 0;
  for (uint32_t s = 0; s < kSlots; ++s) {
    if (s == kSlotUT) continue;
    for (uint32_t off = 0; off < kPageBytes; ++off)
      if (after.get8(s, off) != before.get8(s, off)) ++nb_bad;
  }
  ck(nb_bad == 0, "clean: no byte of any OTHER slot moved", 0, nb_bad);
}

// The write is REFUSED when layer B is part-filled, and refusing means the
// page does not move. A block that wrote what it had would put the PREVIOUS
// bake's words into the untouched tail under a clean handshake.
void case_short_layer_b(Vtb_pageio& dut) {
  World w(dut);
  Pool before;
  for (uint32_t s = 0; s < kSlots; ++s) fill_page(before, s, int(s) + 1);

  w.reset();
  w.load(before);
  ck(w.offer_job(kSlotUT, 0x5Au, 0x11u, 0xBEEFu), "short-B: the job was accepted");

  BakeResult r = run_bake(w, before, kVerts - 1, -1, false);
  ck(!r.stalled, "short-B: the bake face did not stall");
  if (r.stalled) return;

  int verdict = -1, ok = -1;
  ck(w.settle(verdict, ok), "short-B: a completion arrived");
  ck(verdict == V_SHORT_B, "short-B: verdict", V_SHORT_B, verdict);
  ck(ok == 0, "short-B: done_ok is LOW", 0, ok);
  ck(dut.c_refused == 1u, "short-B: jobs_refused FIRED", 1, long(dut.c_refused));
  ck(dut.c_bursts_written == 0u, "short-B: NOT ONE burst was written", 0,
     long(dut.c_bursts_written));
  ck(dut.c_marks == 0u, "short-B: no deformation mark for a refused bake", 0,
     long(dut.c_marks));

  Pool after;
  w.readback(after);
  int bad = 0;
  for (uint32_t off = 0; off < kPageBytes; ++off)
    if (after.get8(kSlotUT, off) != before.get8(kSlotUT, off)) ++bad;
  ck(bad == 0, "short-B: the page is byte-identical -- a refusal is not a partial write",
     0, bad);
}

void case_slot_out_of_range(Vtb_pageio& dut) {
  World w(dut);
  Pool before;
  for (uint32_t s = 0; s < kSlots; ++s) fill_page(before, s, int(s) + 1);
  w.reset();
  w.load(before);
  dut.stat_clear_i = 1;
  dut.eval();
  zhao::tick(dut);
  dut.stat_clear_i = 0;
  dut.eval();

  ck(w.offer_job(1024u, 0x5Au, 0x11u, 0x1u), "slot-oor: the job was accepted for judgement");
  int verdict = -1, ok = -1;
  ck(w.settle(verdict, ok), "slot-oor: a completion arrived");
  ck(verdict == V_SLOT_OOR, "slot-oor: verdict", V_SLOT_OOR, verdict);
  ck(ok == 0, "slot-oor: done_ok is LOW", 0, ok);
  ck(dut.c_refused == 1u, "slot-oor: jobs_refused FIRED", 1, long(dut.c_refused));
  // A REFUSAL NEVER REACHES THE GUARD, so `guard_denied_o` keeps measuring the
  // guard rather than this block's own bookkeeping.
  ck(dut.greqs_seen == 0u, "slot-oor: not one request reached the guard", 0,
     long(dut.greqs_seen));
  ck(dut.c_guard_denied == 0u, "slot-oor: guard_denied still silent", 0,
     long(dut.c_guard_denied));
}

void case_stale_epoch(Vtb_pageio& dut) {
  World w(dut);
  Pool before;
  for (uint32_t s = 0; s < kSlots; ++s) fill_page(before, s, int(s) + 1);
  w.reset();
  w.load(before);
  dut.stat_clear_i = 1;
  dut.eval();
  zhao::tick(dut);
  dut.stat_clear_i = 0;
  dut.eval();

  ck(w.offer_job(kSlotUT, 0x5Au, 0x99u, 0x2u), "stale: the job was accepted for judgement");
  int verdict = -1, ok = -1;
  ck(w.settle(verdict, ok), "stale: a completion arrived");
  ck(verdict == V_EPOCH, "stale: verdict", V_EPOCH, verdict);
  ck(dut.c_stale_gen == 1u, "stale: stale_gen FIRED", 1, long(dut.c_stale_gen));
  ck(dut.greqs_seen == 0u, "stale: not one request reached the guard", 0,
     long(dut.greqs_seen));
}

void case_guard_denies(Vtb_pageio& dut) {
  World w(dut);
  Pool before;
  for (uint32_t s = 0; s < kSlots; ++s) fill_page(before, s, int(s) + 1);
  w.reset();
  w.load(before);
  dut.cfg_deny_mode_i = 1;
  dut.cfg_deny_idx_i = 0;  // the very first layer-D read
  dut.eval();

  ck(w.offer_job(kSlotUT, 0x5Au, 0x11u, 0x3u), "deny: the job was accepted");
  int verdict = -1, ok = -1;
  ck(w.settle(verdict, ok), "deny: a completion arrived");
  ck(verdict == V_GUARD, "deny: verdict", V_GUARD, verdict);
  ck(dut.c_guard_denied == 1u, "deny: guard_denied FIRED", 1, long(dut.c_guard_denied));
  ck(dut.c_bursts_written == 0u, "deny: nothing was written", 0,
     long(dut.c_bursts_written));
}

void case_short_burst(Vtb_pageio& dut) {
  World w(dut);
  Pool before;
  for (uint32_t s = 0; s < kSlots; ++s) fill_page(before, s, int(s) + 1);
  w.reset();
  w.load(before);
  dut.cfg_short_mode_i = 1;
  dut.cfg_short_idx_i = 3;   // the fourth layer-D read
  dut.cfg_short_beat_i = 4;  // last on beat 4 of 8
  dut.eval();

  ck(w.offer_job(kSlotUT, 0x5Au, 0x11u, 0x4u), "short-burst: the job was accepted");
  int verdict = -1, ok = -1;
  ck(w.settle(verdict, ok), "short-burst: a completion arrived");
  ck(verdict == V_INCOMPLETE, "short-burst: verdict", V_INCOMPLETE, verdict);
  ck(dut.c_bursts_written == 0u, "short-burst: nothing was written", 0,
     long(dut.c_bursts_written));
}

// POSITIVE CONTROL for `nobake_mutated_o`. TERRAIN.BAKE preserves bits 7:2, so
// this is unreachable under a legal bake -- but at THIS block's port it is
// ordinary stimulus, which is why it needs no committed mutant.
void case_nobake_mutated(Vtb_pageio& dut) {
  World w(dut);
  Pool before;
  for (uint32_t s = 0; s < kSlots; ++s) fill_page(before, s, int(s) + 1);
  w.reset();
  w.load(before);
  ck(w.offer_job(kSlotUT, 0x5Au, 0x11u, 0x5u), "mutate: the job was accepted");

  BakeResult r = run_bake(w, before, kVerts, 300);
  ck(!r.stalled, "mutate: the bake face did not stall");
  if (r.stalled) return;
  int verdict = -1, ok = -1;
  ck(w.settle(verdict, ok), "mutate: a completion arrived");
  ck(dut.c_nobake_mutated == 1u, "mutate: nobake_mutated FIRED exactly once", 1,
     long(dut.c_nobake_mutated));
  // IT IS COUNTED, NOT CLAMPED. Narrowing a write is how a silent divergence
  // becomes permanent, so the mutated byte still lands and the counter is what
  // makes it visible.
  ck(verdict == V_OK, "mutate: the bake still completed", V_OK, verdict);
  Pool after;
  w.readback(after);
  const uint8_t got = after.get8(kSlotUT, tp::kLayerDOff + 300u);
  const uint8_t want = uint8_t(cell_baked(before.get8(kSlotUT, tp::kLayerDOff + 300u)) ^
                               tp::kNoBakeBit);
  ck(got == want, "mutate: the mutated byte LANDED (counted, not clamped)", long(want),
     long(got));
}

// POSITIVE CONTROL for `cell_refetch_o`: hold an answer, move the cursor
// underneath it, and watch the stale answer be dropped rather than delivered.
void case_cell_cursor_moves(Vtb_pageio& dut) {
  World w(dut);
  Pool before;
  for (uint32_t s = 0; s < kSlots; ++s) fill_page(before, s, int(s) + 1);
  w.reset();
  w.load(before);
  // THE CURSOR STARTS WHERE THE BLOCK'S OWN FIRST READ LANDED. `wait_serving`
  // returns when `sc_ready` rises, and `sc_ready` is gated on `!cell_want_c`,
  // so by then the block has already read and is HOLDING the answer for the
  // cursor as presented -- cell (0,0), the reset value. Presenting anything
  // else first would move the cursor under that held answer and fire the
  // counter before the test had begun, which is how the first version of this
  // case read a correct block as a broken one.
  dut.cell_ci = 0;
  dut.cell_cj = 0;
  dut.cell_ready = 0;
  ck(w.offer_job(kSlotUT, 0x5Au, 0x11u, 0x6u), "refetch: the job was accepted");
  ck(wait_serving(w), "refetch: the page window opened");

  dut.eval();
  int guard = 0;
  while (!dut.cell_valid && guard < 5000) {
    zhao::tick(dut);
    dut.eval();
    ++guard;
  }
  ck(guard < 5000, "refetch: the first answer arrived");
  ck(uint8_t(dut.cell_state) == before.get8(kSlotUT, tp::kLayerDOff),
     "refetch: the held answer is cell 0's",
     long(before.get8(kSlotUT, tp::kLayerDOff)), long(dut.cell_state));
  ck(dut.c_cell_refetch == 0u, "refetch: silent before the cursor moves", 0,
     long(dut.c_cell_refetch));

  // Move the cursor. The held answer is for cell 0 and the cursor now says 9,
  // so delivering it would be cell 0's data under cell 9's address.
  dut.cell_ci = 9;
  dut.cell_cj = 0;
  dut.eval();
  ck(dut.cell_valid == 0, "refetch: the stale answer is NOT offered against the new "
                          "address");
  zhao::tick(dut);
  dut.eval();
  ck(dut.c_cell_refetch >= 1u, "refetch: cell_refetch FIRED", 1,
     long(dut.c_cell_refetch));

  // And the re-read answers the NEW address correctly.
  dut.cell_ready = 1;
  dut.eval();
  guard = 0;
  while (!dut.cell_valid && guard < 5000) {
    zhao::tick(dut);
    dut.eval();
    ++guard;
  }
  ck(guard < 5000, "refetch: the re-read arrived");
  ck(uint8_t(dut.cell_state) == before.get8(kSlotUT, tp::kLayerDOff + 9u),
     "refetch: the re-read answered CELL 9, not cell 0",
     long(before.get8(kSlotUT, tp::kLayerDOff + 9u)), long(dut.cell_state));
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  {
    Vtb_pageio dut;
    case_clean(dut);
  }
  { Vtb_pageio dut; case_short_layer_b(dut); }
  { Vtb_pageio dut; case_slot_out_of_range(dut); }
  { Vtb_pageio dut; case_stale_epoch(dut); }
  { Vtb_pageio dut; case_guard_denies(dut); }
  { Vtb_pageio dut; case_short_burst(dut); }
  { Vtb_pageio dut; case_nobake_mutated(dut); }
  { Vtb_pageio dut; case_cell_cursor_moves(dut); }

  std::printf("pageio_rtl_directed: %d checks, %d failures\n", g_checks, g_fail);
  std::fflush(stdout);
  return g_fail == 0 ? 0 : 1;
}
