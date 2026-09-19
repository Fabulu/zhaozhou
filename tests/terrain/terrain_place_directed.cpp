// terrain_place_directed.cpp -- TERRAIN.PLACE against the frozen placement law.
//
// The law is spec/terrain_rules.md 1.3 and 2.1, and it is short enough to state
// as the acceptance criterion in full:
//
//     wx(i) = (patch_ix * 32 + i) * pitch,  pitch = 2^pitch_log2 metres,
//     pitch_log2 in {-1, 0, +1, +2}, lattice 33 x 33, fx16 = s32 with 16
//     fractional bits.
//
// So this file checks the ARITHMETIC against numbers computed here from that
// sentence, never against a second transcription of the RTL's own shifts. Every
// expectation below is written as metres times 65536 so that a reader can check
// it against the spec rather than against the implementation.
//
// AND IT FIRES ALL FOUR COUNTERS. CLAUDE.md is explicit that a detector reading
// zero is a claim and the claim to check hardest, so each of
// `place_env_mismatch_o`, `place_pitch_bad_o`, `place_range_o` and
// `place_patches_o` is driven to move here with stimulus that is legal to
// present and illegal in content. None of them needed a mutant: unlike a queue
// overflow behind a correct full-guard, every refusal this block makes is
// reachable from its own input port, because a corrupt page header is exactly
// what the envelope redundancy exists to catch.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"
#include "Vzhao_terrain_place.h"
#include "zhao_sim.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

constexpr int32_t kOneMetre = 65536;  // fx16, spec/qformats.md: S 1.15.16

// The law, written from the spec rather than from the RTL.
int64_t law_place(int32_t patch_coord, int32_t idx, int32_t pitch_log2) {
  // (patch_coord * 32 + idx) cells, each of 2^pitch_log2 metres.
  const int64_t units = static_cast<int64_t>(patch_coord) * 32 + idx;
  const int64_t metres_num = units;  // times 2^pitch_log2
  // fx16 raw = units * 2^pitch_log2 * 65536 = units << (16 + pitch_log2)
  const int shift = 16 + pitch_log2;
  return metres_num << shift;
}

void reset(Vzhao_terrain_place& d) {
  d.rst_n = 0;
  d.hdr_valid_i = 0;
  d.hdr_pitch_log2_i = 0;
  d.hdr_patch_ix_i = 0;
  d.hdr_patch_iz_i = 0;
  d.hdr_env_x0_i = 0;
  d.hdr_env_z0_i = 0;
  d.hdr_src_id_i = 0;
  d.vtx_vi_i = 0;
  d.vtx_vj_i = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
}

// Present one header for exactly one cycle. `env_ok` false corrupts the
// envelope, which is the fault the spec's redundancy exists to detect.
void present(Vzhao_terrain_place& d, int32_t ix, int32_t iz, int32_t pitch_log2,
             bool env_ok = true) {
  d.hdr_valid_i = 1;
  d.hdr_pitch_log2_i = static_cast<uint8_t>(pitch_log2 & 0xff);
  d.hdr_patch_ix_i = static_cast<uint16_t>(ix & 0xffff);
  d.hdr_patch_iz_i = static_cast<uint16_t>(iz & 0xffff);
  const int32_t ex = static_cast<int32_t>(law_place(ix, 0, pitch_log2));
  const int32_t ez = static_cast<int32_t>(law_place(iz, 0, pitch_log2));
  d.hdr_env_x0_i = env_ok ? ex : (ex + 1);  // one fx16 tick out: the subtlest corruption
  d.hdr_env_z0_i = ez;
  d.hdr_src_id_i = 0x5A5A;
  zhao::tick(d);
  d.hdr_valid_i = 0;
}

// Read the per-vertex placement combinationally, as TERRAIN.PATCH does.
void probe(Vzhao_terrain_place& d, int vi, int vj) {
  d.vtx_vi_i = static_cast<uint8_t>(vi);
  d.vtx_vj_i = static_cast<uint8_t>(vj);
  d.eval();
}

// ---------------------------------------------------------------------------
// 1. The canonical pitch, which is the one the battlefield actually uses.
// ---------------------------------------------------------------------------
void test_canonical_pitch_places_two_metre_cells() {
  Vzhao_terrain_place d;
  reset(d);
  present(d, 0, 0, 1);  // pitch_log2 = +1 -> 2.0 m, spec 1.3 "canonical"

  check(d.place_valid_o == 1, "a legal header places the patch", 1, d.place_valid_o);
  check(d.vtx_placed_o == 1, "and the per-vertex lane reports placed", 1, d.vtx_placed_o);

  probe(d, 0, 0);
  check(d.vtx_wx_o == 0, "patch 0 column 0 sits on the island datum", 0,
        static_cast<int32_t>(d.vtx_wx_o));

  probe(d, 1, 0);
  check(static_cast<int32_t>(d.vtx_wx_o) == 2 * kOneMetre, "column 1 at 2.0 m pitch is 2.0 m",
        2 * kOneMetre, static_cast<int32_t>(d.vtx_wx_o));

  probe(d, 32, 32);
  check(static_cast<int32_t>(d.vtx_wx_o) == 64 * kOneMetre,
        "column 32 is 64 m -- spec 1.3: a patch is 64 m per side", 64 * kOneMetre,
        static_cast<int32_t>(d.vtx_wx_o));
  check(static_cast<int32_t>(d.vtx_wz_o) == 64 * kOneMetre, "and rows obey the identical law in z",
        64 * kOneMetre, static_cast<int32_t>(d.vtx_wz_o));
}

// ---------------------------------------------------------------------------
// 2. All four legal pitches, and the patch offset.
// ---------------------------------------------------------------------------
void test_every_legal_pitch_and_offset() {
  const int pitches[4] = {-1, 0, 1, 2};
  for (int pi = 0; pi < 4; ++pi) {
    const int p = pitches[pi];
    Vzhao_terrain_place d;
    reset(d);
    // A patch well inside range at every pitch.
    const int32_t ix = 3, iz = -5;
    present(d, ix, iz, p);
    check(d.place_valid_o == 1, "legal pitch places", 1, d.place_valid_o);

    for (int i = 0; i <= 32; i += 8) {
      probe(d, i, i);
      const int64_t wx = law_place(ix, i, p);
      const int64_t wz = law_place(iz, i, p);
      check(static_cast<int32_t>(d.vtx_wx_o) == static_cast<int32_t>(wx),
            "wx follows the frozen law at this pitch", static_cast<long long>(wx),
            static_cast<int32_t>(d.vtx_wx_o));
      check(static_cast<int32_t>(d.vtx_wz_o) == static_cast<int32_t>(wz),
            "wz follows the frozen law at this pitch", static_cast<long long>(wz),
            static_cast<int32_t>(d.vtx_wz_o));
    }
  }
}

// ---------------------------------------------------------------------------
// 3. The COMPCACHE fill: 33 x's then 33 z's, in index order, and the order is
//    part of the contract because two orderings give different capture CRCs.
// ---------------------------------------------------------------------------
void test_compcache_fill_is_66_writes_in_declared_order() {
  Vzhao_terrain_place d;
  reset(d);
  const int32_t ix = 2, iz = 7, p = 1;
  present(d, ix, iz, p);

  std::vector<int> axes;
  std::vector<int> idxs;
  std::vector<int32_t> vals;
  bool saw_done = false;
  for (int cycle = 0; cycle < 80 && !saw_done; ++cycle) {
    d.eval();
    if (d.pos_we_o) {
      axes.push_back(d.pos_axis_o);
      idxs.push_back(d.pos_idx_o);
      vals.push_back(static_cast<int32_t>(d.pos_val_o));
    }
    if (d.pos_done_o) saw_done = true;
    zhao::tick(d);
  }

  check(saw_done, "the fill announces completion", 1, saw_done ? 1 : 0);
  check(static_cast<int>(axes.size()) == 66, "33 columns plus 33 rows is 66 writes", 66,
        static_cast<long long>(axes.size()));
  if (axes.size() != 66) return;

  bool order_ok = true;
  for (int i = 0; i < 33; ++i) {
    if (axes[i] != 0 || idxs[i] != i) order_ok = false;
    if (vals[i] != static_cast<int32_t>(law_place(ix, i, p))) order_ok = false;
  }
  for (int j = 0; j < 33; ++j) {
    const int k = 33 + j;
    if (axes[k] != 1 || idxs[k] != j) order_ok = false;
    if (vals[k] != static_cast<int32_t>(law_place(iz, j, p))) order_ok = false;
  }
  check(order_ok, "columns on axis 0 then rows on axis 1, ascending, values per the law", 1,
        order_ok ? 1 : 0);
}

// ---------------------------------------------------------------------------
// 4. THE COUNTERS. Each is fired; none is quoted at zero.
// ---------------------------------------------------------------------------
void test_envelope_mismatch_fires() {
  Vzhao_terrain_place d;
  reset(d);
  check(d.place_env_mismatch_o == 0, "the census starts clean", 0, d.place_env_mismatch_o);

  present(d, 4, 4, 1, /*env_ok=*/false);

  check(d.place_env_mismatch_o == 1, "a header whose envelope is ONE fx16 tick out is caught", 1,
        d.place_env_mismatch_o);
  check(d.place_valid_o == 0, "and the patch is refused rather than placed", 0, d.place_valid_o);
  check(d.vtx_placed_o == 0, "so the per-vertex lane reports NOT placed", 0, d.vtx_placed_o);
}

void test_illegal_pitch_fires_and_does_not_default() {
  Vzhao_terrain_place d;
  reset(d);
  present(d, 0, 0, 3);  // spec 1.3 allows -1..+2 only

  check(d.place_pitch_bad_o == 1, "pitch_log2 = 3 is refused", 1, d.place_pitch_bad_o);
  check(d.place_valid_o == 0, "an illegal pitch does not silently pick a default", 0,
        d.place_valid_o);
  check(d.place_env_mismatch_o == 0,
        "and it does not ALSO report an envelope fault -- one fault, one counter", 0,
        d.place_env_mismatch_o);
}

void test_out_of_range_patch_fires() {
  Vzhao_terrain_place d;
  reset(d);
  // At 4.0 m pitch the shift is 18, so only 13 bits of cell index fit in fx16.
  // patch_ix = 300 puts the patch past the representable world.
  present(d, 300, 0, 2);
  check(d.place_range_o == 1, "a patch outside representable fx16 world is refused", 1,
        d.place_range_o);
  check(d.place_valid_o == 0, "and is not placed wrapped", 0, d.place_valid_o);

  // The same coordinate is perfectly legal at a finer pitch, which shows the
  // test is about REACH and not about the number 300.
  Vzhao_terrain_place e;
  reset(e);
  present(e, 300, 0, -1);
  check(e.place_range_o == 0, "the same patch at 0.5 m pitch is in range", 0, e.place_range_o);
  check(e.place_valid_o == 1, "and places normally", 1, e.place_valid_o);
}

void test_accepted_patches_are_counted() {
  Vzhao_terrain_place d;
  reset(d);
  for (int k = 0; k < 5; ++k) {
    present(d, k, k, 1);
    // let each fill drain so the sequencer is idle before the next header
    for (int c = 0; c < 70; ++c) zhao::tick(d);
  }
  check(d.place_patches_o == 5, "every accepted patch is counted", 5, d.place_patches_o);
  check(d.place_src_id_o == 0x5A5A, "and the placed patch names its source page", 0x5A5A,
        d.place_src_id_o);
}

// ---------------------------------------------------------------------------
// 5. A refusal must UNPLACE. Leaving the previous patch standing would place
//    this page's vertices at the last page's coordinates -- plausible geometry
//    in the wrong place, which is the worst of the available failures.
// ---------------------------------------------------------------------------
void test_refusal_unplaces_the_previous_patch() {
  Vzhao_terrain_place d;
  reset(d);
  present(d, 1, 1, 1);
  for (int c = 0; c < 70; ++c) zhao::tick(d);
  check(d.place_valid_o == 1, "first patch placed", 1, d.place_valid_o);

  present(d, 2, 2, 1, /*env_ok=*/false);
  check(d.place_valid_o == 0,
        "a corrupt header unplaces rather than leaving the old placement live", 0, d.place_valid_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  test_canonical_pitch_places_two_metre_cells();
  test_every_legal_pitch_and_offset();
  test_compcache_fill_is_66_writes_in_declared_order();
  test_envelope_mismatch_fires();
  test_illegal_pitch_fires_and_does_not_default();
  test_out_of_range_patch_fires();
  test_accepted_patches_are_counted();
  test_refusal_unplaces_the_previous_patch();

  std::printf("terrain_place_directed: %d checks, %d failed\n", g_checks, g_failed);
  return g_failed == 0 ? 0 : 1;
}
