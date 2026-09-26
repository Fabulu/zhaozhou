// geom_binner_v2_cntw_wrap.cpp -- the per-tile reference count does not wrap.
//
// WHAT THIS IS FOR
// ----------------
// `zhao_geom_binner_v2`'s per-tile reference count `CNT_W` shipped as a
// HARDCODED localparam 11, hand-sized to the DEFAULT arena (CHUNKS=256 x
// CHUNK_REFS=4 = 1,024 references) and derived from nothing. GIANTREFS made it
// `$clog2(CHUNKS*CHUNK_REFS + 1)`.
//
// The fault that hides behind a hand-sized count is NOT an overflow. When a
// tile's count wraps past its width it returns to ZERO, and `cur_count == 0`
// is exactly the condition the push logic uses to decide "this tile's list is
// empty, seed its HEAD" -- so the next push re-roots the list and orphans every
// reference already chained on it. The drain then reads the wrapped count and
// emits a fraction of what was pushed, while `overflow_o` stays LOW, because
// neither the chunk arena nor the triangle store ever ran out. Silent loss
// wearing a healthy frame's clothes.
//
// The frame-wide instrument `tile_references_o` is a SEPARATE 32-bit counter
// incremented on every accepted push, so it reports the pushes that were made,
// not the references the machine can still reach. Under the mutant it reads
// 2,049 while the tile holds one. That is the broken-instrument law with two
// operands that do not move together for once -- and it is why this test reads
// the DRAIN, not the counter.
//
// TWO POLARITIES, ONE SOURCE (CLAUDE.md, "A guard you cannot reach with legal
// stimulus needs a COMMITTED MUTANT"):
//
//   built plain                     -> asserts the CORRECT behaviour: every
//                                      reference pushed is a reference drained,
//                                      and the count never wraps. This is the
//                                      assertion about the shipped design and
//                                      it does not mention the bug.
//   built with the committed mutant -> polarity INVERTED: passes only when the
//     EXPECT_GEOM_BINNER_V2_CNTW_WRAP  wrap FIRES and eats the references.
//                                      Evidence about the instrument, not about
//                                      the design.
//
// THE PARAMETERISATION, and why it is not the shipped one. A per-tile count is
// incremented once per (triangle, tile) pair, and the bin cursor visits each
// tile of a triangle's range exactly once, so the count's reachable maximum is
// min(TRI_CAP, CHUNKS*CHUNK_REFS). Reaching 2,048 therefore needs BOTH raised.
// This runs TRI_CAP=4096 / CHUNKS=8192 and sends 2,049 triangles at one tile.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "Vzhao_geom_binner_v2.h"
#include "verilated.h"

// zhao::exit_hard -- tests/harness/zhao_sim.hpp: a plain return from a
// Verilated main can deadlock in VlThreadPool's destructor at ~0 CPU.
#include "../harness/zhao_sim.hpp"

double sc_time_stamp() { return 0.0; }

namespace {

constexpr int kGridW = 24;
constexpr int kGridH = 24;

// METAW stays at the block's default. It is NOT a free knob: the metadata
// field offsets are hardcoded up to bit 424 (`meta_area_bad_c`), so a narrower
// bank is a build error, not a smaller test. Measured by trying 64.
constexpr int kMetaBits = 1157;
constexpr int kMetaWords = (kMetaBits + 31) / 32;
// bit 378 of the area field, so `meta_area_bad_c` is not asserted on every
// fixture triangle. The verdict is carried, not consulted, but a bench that
// drives a permanent profile fault is not a bench about widths.
constexpr int kAreaWord = 378 / 32;
constexpr uint32_t kAreaBit = 1u << (378 % 32);

// 2,049 = one more than the 2,048 a hardcoded 11-bit count can hold.
constexpr int kPushes = 2049;
// Must match the -GTRI_CAP the CMake target passes; phase 2 fills the store.
constexpr int kTriCap = 4096;

#if defined(EXPECT_GEOM_BINNER_V2_CNTW_WRAP)
constexpr bool kExpectWrap = true;
// The hardcoded width holds 0..2047, so 2,049 pushes leave 2,049 mod 2,048 = 1.
constexpr int kWrappedSurvivors = kPushes - 2048;
#else
constexpr bool kExpectWrap = false;
#endif

Vzhao_geom_binner_v2* dut = nullptr;  // heap-resident: avoids MinGW teardown stalls
uint64_t cycles = 0;
uint64_t checks = 0;
int failures = 0;

void fail(const char* what) {
  if (failures < 20) std::printf("FAIL: %s\n", what);
  ++failures;
}

void require(bool ok, const char* what) {
  ++checks;
  if (!ok) fail(what);
}

uint32_t m21(int32_t v) { return static_cast<uint32_t>(v) & 0x1fffffu; }
uint32_t m23(int32_t v) { return static_cast<uint32_t>(v) & 0x7fffffu; }
uint32_t m12(int32_t v) { return static_cast<uint32_t>(v) & 0x0fffu; }
uint64_t m48(int64_t v) { return static_cast<uint64_t>(v) & 0x0000ffffffffffffull; }

void settle() { dut->eval(); }

void tick() {
  dut->clk = 0;
  settle();
  dut->clk = 1;
  settle();
  ++cycles;
}

// One triangle wholly inside tile (0,0) -- pixels 0..15 in both axes. Every
// push therefore lands on the same tile list, which is the only way to drive a
// single count to 2,048 at all.
void drive_tile0_tri(uint16_t src) {
  const int64_t ax = 1 * 256, ay = 1 * 256;
  const int64_t bx = 14 * 256, by = 2 * 256;
  const int64_t cx = 2 * 256, cy = 14 * 256;
  const int64_t vx[3] = {bx, cx, ax};
  const int64_t vy[3] = {by, cy, ay};
  const int64_t wx[3] = {cx, ax, bx};
  const int64_t wy[3] = {cy, ay, by};
  uint8_t tl = 0;
  for (int i = 0; i < 3; ++i) {
    const int32_t kx = static_cast<int32_t>(-(wy[i] - vy[i]));
    const int32_t ky = static_cast<int32_t>(wx[i] - vx[i]);
    const int64_t kc = vx[i] * wy[i] - vy[i] * wx[i];
    const bool tlb = (vy[i] == wy[i]) ? (vx[i] < wx[i]) : (vy[i] < wy[i]);
    if (tlb) tl = static_cast<uint8_t>(tl | (1u << i));
    switch (i) {
      case 0:
        dut->tri_kx0_i = m23(kx);
        dut->tri_ky0_i = m23(ky);
        dut->tri_kc0_i = m48(kc);
        break;
      case 1:
        dut->tri_kx1_i = m23(kx);
        dut->tri_ky1_i = m23(ky);
        dut->tri_kc1_i = m48(kc);
        break;
      default:
        dut->tri_kx2_i = m23(kx);
        dut->tri_ky2_i = m23(ky);
        dut->tri_kc2_i = m48(kc);
        break;
    }
  }
  dut->tri_tl_i = tl;
  dut->tri_ax_i = m21(static_cast<int32_t>(ax));
  dut->tri_ay_i = m21(static_cast<int32_t>(ay));
  dut->tri_bx_i = m21(static_cast<int32_t>(bx));
  dut->tri_by_i = m21(static_cast<int32_t>(by));
  dut->tri_cx_i = m21(static_cast<int32_t>(cx));
  dut->tri_cy_i = m21(static_cast<int32_t>(cy));
  dut->tri_min_x_i = m12(1);
  dut->tri_max_x_i = m12(13);
  dut->tri_min_y_i = m12(1);
  dut->tri_max_y_i = m12(13);
  dut->tri_src_id_i = src;
  for (int i = 0; i < kMetaWords; ++i) dut->tri_meta_i[i] = 0;
  dut->tri_meta_i[0] = src;
  dut->tri_meta_i[kAreaWord] |= kAreaBit;
  dut->tok_grant_i = 1;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  dut = new Vzhao_geom_binner_v2;

  dut->clk = 0;
  dut->rst_n = 0;
  dut->frame_begin_i = 0;
  dut->frame_end_i = 0;
  dut->grid_w_i = kGridW;
  dut->grid_h_i = kGridH;
  dut->tri_valid_i = 0;
  dut->tok_grant_i = 0;
  dut->job_ready_i = 1;
  dut->ser_req_i = 0;
  dut->ser_ready_i = 0;
  for (int i = 0; i < 8; ++i) tick();
  dut->rst_n = 1;
  // S_CLEAR walks every tile of the grid before the block will accept anything.
  for (int i = 0; i < kGridW * kGridH + 32; ++i) tick();

  dut->frame_begin_i = 1;
  tick();
  dut->frame_begin_i = 0;
  for (int i = 0; i < kGridW * kGridH + 32; ++i) tick();

  // ---- push kPushes references at ONE tile ---------------------------------
  int accepted = 0;
  uint64_t guard = 0;
  while (accepted < kPushes && guard < 4000000) {
    drive_tile0_tri(static_cast<uint16_t>(accepted & 0xffff));
    dut->tri_valid_i = 1;
    settle();
    const bool took = dut->tri_ready_o != 0;
    tick();
    if (took) ++accepted;
    ++guard;
  }
  dut->tri_valid_i = 0;
  dut->tok_grant_i = 0;
  settle();
  require(accepted == kPushes, "every fixture triangle was accepted");

  // Let the last triangle finish enumerating before the frame ends.
  for (int i = 0; i < 64; ++i) tick();

  const uint32_t refs_counted = dut->tile_references_o;
  const uint32_t depth_seen = dut->max_tile_list_depth_o;
  const uint32_t culled = dut->triangles_culled_o;
  const bool overflow = dut->overflow_o != 0;

  // ---- drain, and COUNT WHAT SURVIVES --------------------------------------
  dut->frame_end_i = 1;
  tick();
  dut->frame_end_i = 0;

  int emitted = 0;
  guard = 0;
  bool drained = false;
  while (!drained && guard < 8000000) {
    settle();
    if (dut->job_valid_o && dut->job_ready_i) ++emitted;
    tick();
    if (dut->drain_done_o) drained = true;
    ++guard;
  }
  require(drained, "the drain completed");

  std::printf(
      "CNTW: pushed=%d drained=%d tile_references_o=%u max_depth=%u culled=%u overflow=%d "
      "cycles=%llu\n",
      kPushes, emitted, refs_counted, depth_seen, culled, overflow ? 1 : 0,
      static_cast<unsigned long long>(cycles));

  // The wall must be silent in BOTH builds: nothing here exhausts the arena or
  // the triangle store, so a wrap is precisely the fault `overflow_o` cannot
  // see. Quoting that silence is the point of measuring it.
  require(!overflow, "overflow_o stayed low (the arena never ran out)");
  require(culled == 0, "no triangle was culled");
  require(refs_counted == static_cast<uint32_t>(kPushes),
          "tile_references_o counted every push");

  if (kExpectWrap) {
#if defined(EXPECT_GEOM_BINNER_V2_CNTW_WRAP)
    // INVERTED POLARITY: this build passes only when the hardcoded width
    // actually loses the references.
    require(emitted == kWrappedSurvivors,
            "MUTANT: the hardcoded 11-bit count wrapped and orphaned the list");
    require(depth_seen == 2048, "MUTANT: max_tile_list_depth_o saturated at the wrapped width");
    if (failures == 0)
      std::printf(
          "PASS: CNT_W wrap mutant fired exactly -- %d references pushed, %d survived, "
          "overflow_o=0\n",
          kPushes, emitted);
#endif
  } else {
    require(emitted == kPushes, "every pushed reference was drained");
    require(depth_seen == static_cast<uint32_t>(kPushes),
            "max_tile_list_depth_o reported the true list depth");
    if (failures == 0)
      std::printf("PASS: derived CNT_W held %d references in one tile with no wrap\n", kPushes);
  }

  // ---- PHASE 2: THE OVERFLOW COUNTER'S POSITIVE CONTROL -------------------
  // Phase 1 quotes `overflow_o == 0`, and a detector reading zero is a claim,
  // not a result -- it is the claim to check hardest. So the same binary, the
  // same instance, fires it deliberately: a fresh frame, then TRI_CAP + 1
  // triangles. The store fills, S_IDLE raises the wall (LAWS CHOSEN D), and
  // every triangle after it is dropped whole and counted.
  //
  // This is reachable with LEGAL STIMULUS, so it needs no mutant. It is kept
  // separate from phase 1 deliberately: phase 1 asserts the CORRECT behaviour
  // of the shipped capacity, and this asserts the instrument. Mixing them is
  // how "the counter fires on the bug" becomes a test that passes only while
  // the bug exists.
  if (!kExpectWrap) {
    const uint32_t culled_before = dut->triangles_culled_o;
    dut->frame_begin_i = 1;
    tick();
    dut->frame_begin_i = 0;
    for (int i = 0; i < kGridW * kGridH + 32; ++i) tick();
    require(dut->overflow_o == 0, "the wall dropped at the frame boundary");

    int fed = 0;
    guard = 0;
    while (fed < kTriCap + 1 && guard < 4000000) {
      drive_tile0_tri(static_cast<uint16_t>(fed & 0xffff));
      dut->tri_valid_i = 1;
      settle();
      const bool took = dut->tri_ready_o != 0;
      tick();
      if (took) ++fed;
      ++guard;
    }
    dut->tri_valid_i = 0;
    dut->tok_grant_i = 0;
    for (int i = 0; i < 64; ++i) tick();
    settle();

    const uint32_t culled_after = dut->triangles_culled_o;
    std::printf("CNTW: overflow control  fed=%d overflow=%d culled_delta=%u\n", fed,
                dut->overflow_o ? 1 : 0, culled_after - culled_before);
    require(dut->overflow_o != 0, "overflow_o FIRED when the triangle store filled");
    require(culled_after > culled_before,
            "triangles_culled_o counted the walled-off triangles");
    if (failures == 0)
      std::printf("PASS: overflow_o and triangles_culled_o fire on a real wall\n");
  }

  std::printf("CNTW: checks=%llu failures=%d\n", static_cast<unsigned long long>(checks),
              failures);
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
