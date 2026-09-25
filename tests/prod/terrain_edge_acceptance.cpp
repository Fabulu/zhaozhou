// terrain_edge_acceptance.cpp -- THE ACCEPTANCE DRIVER for entry I21's
// neighbour-edge level producer.
//
// IT ASKS ONE QUESTION AND EVERYTHING ELSE SERVES IT: on a seam shared by two
// patches that chose DIFFERENT levels, do the two sides tessellate at the same
// density?
//
//   `zhao_terrain_tess` stitches a shared edge at MAX(neighbour, own):
//       lv_px = (job_lvl_px_i > job_level_i) ? job_lvl_px_i : job_level_i;
//   so patch P's +x seam and patch N's -x seam are the SAME EDGE IN THE WORLD
//   and must come out at the same level. If they do not, the two sides emit
//   different vertex counts along it and there is a hole in the ground.
//
// `tess_edge()` below is three lines reproducing that expression, so a failure
// here IS a crack rather than a proxy for one.
//
// ===========================================================================
// THE POSITIVE CONTROL IS THE CONSOLE THIS PACKET REPLACED
// ===========================================================================
// Section 1 runs the EMIT pass with NO PREPARE, which is exactly the console
// as it shipped until today: `prep_valid` low, every edge answered `8'h00`.
// It ASSERTS THE SEAM DISAGREES. That is not a bug being tested for -- it is
// the measurement of what the retained constant costs, and it is the control
// that makes section 2's agreement mean something. Entry I21's own text said
// `8'h00` gives "more triangles and NO CRACK"; `max(0, own) == own`, so the
// seam is not stitched at all, and this is where that correction is measured
// on the LIVE COMPOSED PATH rather than in a block's own directed suite.
//
// Section 2 runs the same stimulus WITH the PREPARE pass and asserts the seam
// AGREES -- and, separately, that at least one lane's tessellation level
// actually MOVED between the two sections. A repair that changed no number
// would pass an agreement test trivially (two patches at the same level agree
// under any constant), so the "it changed" assertion is what stops this bench
// going green on a fixture that could not have failed.
//
// ===========================================================================
// THE PLACEMENT MIRROR, DECLARED
// ===========================================================================
// `place_cx()` below is a THREE-LINE MIRROR of `zhao_terrain_place_law_pkg`'s
// `place32(units_of(...))`, used to build the EMIT-side descriptors -- because
// in the console those come from `zhao_terrain_spdesc`, which reads the
// placement the compose cache already holds, and this bench models SPDESC.
//
// IT IS A SECOND IMPLEMENTATION AND IT IS NOT LOAD-BEARING, which is the only
// reason it is allowed. If it disagreed with the package, PREPARE and EMIT
// would decide different levels for the same patch and the seam assertions
// would FAIL -- so its correctness is checked by the thing it feeds, not
// assumed. That is the opposite arrangement from an oracle, and it is stated
// here so nobody later promotes it into one.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_edge_acceptance.h"

#include "zhao_sim.hpp"
#include "zhao_abi.h"

namespace {

int fails = 0;
int checks = 0;

void check(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++fails;
    std::printf("  FAIL: %s\n", what);
  }
}

void check_eq(uint64_t got, uint64_t want, const char* what) {
  ++checks;
  if (got != want) {
    ++fails;
    std::printf("  FAIL: %s -- expected %llu (0x%llx), got %llu (0x%llx)\n", what,
                (unsigned long long)want, (unsigned long long)want,
                (unsigned long long)got, (unsigned long long)got);
  }
}

// `zhao_terrain_tess`'s own stitch expression, three lines, nothing else.
inline int tess_edge(int neighbour_level, int own_level) {
  return (neighbour_level > own_level) ? neighbour_level : own_level;
}

// The lane packing TERRAIN.EDGERECON and `edge_lane()` both use: lane k in
// bits [2k+1:2k].
inline int edge_lane(uint32_t word, int k) { return (int)((word >> (2 * k)) & 3u); }

// ---- the placement mirror (see the header) --------------------------------
constexpr int kSubEdge = 8;
constexpr int kCentreOff = 4;
inline int32_t place_cx(int ix, int vi, int pitch_log2) {
  const int64_t units = (int64_t)ix * 32 + vi;
  return (int32_t)(units << (16 + pitch_log2));
}

// ---- the T5 record, 32 bytes, as `zref::swstream::encode_record` writes it -
struct Rec {
  uint32_t island;
  int16_t  ix;
  int16_t  iz;
  uint64_t page_addr;
  uint32_t page_crc;
  uint16_t flags;
  uint8_t  view_mask;
  uint8_t  priority;
  uint32_t source_id;
};

void encode_record(const Rec& r, uint8_t out[32]) {
  std::memset(out, 0, 32);
  auto put32 = [&](int off, uint32_t v) {
    out[off] = (uint8_t)v; out[off + 1] = (uint8_t)(v >> 8);
    out[off + 2] = (uint8_t)(v >> 16); out[off + 3] = (uint8_t)(v >> 24);
  };
  auto put16 = [&](int off, uint16_t v) {
    out[off] = (uint8_t)v; out[off + 1] = (uint8_t)(v >> 8);
  };
  put32(0, r.island);
  put16(4, (uint16_t)r.ix);
  put16(6, (uint16_t)r.iz);
  put32(8, (uint32_t)r.page_addr);
  put32(12, (uint32_t)(r.page_addr >> 32));
  put32(16, r.page_crc);
  put16(20, r.flags);
  out[22] = r.view_mask;
  out[23] = r.priority;
  put32(24, r.source_id);
  put32(28, 0);
}

// One EMIT decision beat, collected.
struct Beat {
  int ox, oz, level, nz, pz, nx, px, surface, src;
};

// ==========================================================================
// THE BENCH
// ==========================================================================
class Bench {
 public:
  Bench() {
    auto& t = top_;
    t.clk = 0;
    t.rst_n = 0;
    t.aw_valid_i = 0; t.aw_word_i = 0; t.aw_data_i = 0; t.hps_stall_i = 0;
    t.rw_valid_i = 0; t.rw_idx_i = 0; t.rw_ix_i = 0; t.rw_iz_i = 0;
    t.rw_hit_i = 0; t.rw_slot_i = 0; t.rw_gen_i = 0; t.rw_count_i = 0;
    t.dw_valid_i = 0; t.dw_slot_i = 0; t.dw_sp_i = 0;
    t.dw_dev1_i = 0; t.dw_dev2_i = 0; t.dw_dev3_i = 0; t.dw_cy_i = 0;
    t.dw_prev_level_i = 0; t.dw_prev_morph_i = 0; t.dw_hold_i = 0;
    t.dw_fresh_i = 1; t.dev_latency_i = 0;
    t.cfg_valid_i = 0; t.cfg_epoch_i = 7; t.cfg_arena_base_i = 0;
    t.cfg_arena_bytes_i = 8192;
    t.j_valid_i = 0; t.j_epoch_i = 7; t.j_list_off_i = 0;
    t.j_list_bytes_i = 0; t.j_list_crc_i = 0; t.j_patch_count_i = 0;
    t.frame_i = 0;
    t.gv_cam0_x_i = 0; t.gv_cam0_y_i = 0; t.gv_cam0_z_i = 0;
    t.gv_cam0_scale_i = 0; t.gv_cam0_en_i = 0;
    t.gv_cam1_x_i = 0; t.gv_cam1_y_i = 0; t.gv_cam1_z_i = 0;
    t.gv_cam1_scale_i = 0; t.gv_cam1_en_i = 0;
    t.gv_hyst_i = 0; t.gv_min_hold_i = 0; t.gv_morph_step_i = 0;
    t.dual_i = 0; t.witness_bump_i = 0;
    t.hdr_valid_i = 0; t.hdr_pitch_log2_i = 0; t.hdr_island_i = 0;
    t.hdr_ix_i = 0; t.hdr_iz_i = 0; t.hdr_env_x0_i = 0; t.hdr_env_z0_i = 0;
    t.rec_valid_i = 0; t.rec_ix_i = 0; t.rec_iz_i = 0; t.rec_src_id_i = 0;
    t.door_valid_i = 0; t.door_src_id_i = 0;
    t.serve_valid_i = 0; t.serve_src_id_i = 0;
    t.e_sp_valid_i = 0; t.e_sp_cx_i = 0; t.e_sp_cy_i = 0; t.e_sp_cz_i = 0;
    t.e_sp_dev1_i = 0; t.e_sp_dev2_i = 0; t.e_sp_dev3_i = 0;
    t.e_sp_prev_level_i = 0; t.e_sp_prev_morph_i = 0; t.e_sp_hold_i = 0;
    t.e_sp_src_id_i = 0;
    t.h_valid_i = 0; t.h_level_i = 0; t.h_morph_i = 0; t.h_hold_i = 0;
    t.a_lu_valid_i = 0; t.a_lu_ix_i = 0; t.a_lu_iz_i = 0; t.a_hog_i = 0;
    t.lu_spurious_i = 0; t.force_sweep_i = 0;
    t.e_out_ready_i = 1;
    for (int i = 0; i < 6; ++i) tick();
    t.rst_n = 1;
    for (int i = 0; i < 4; ++i) tick();
  }

  Vtb_terrain_edge_acceptance& t() { return top_; }

  void tick() {
    top_.eval();
    // COLLECT BEFORE THE EDGE, on the handshake, exactly as the consumer would.
    if (top_.e_out_valid_o && top_.e_out_ready_i) {
      Beat b;
      b.ox = top_.e_out_ox_o;   b.oz = top_.e_out_oz_o;
      b.level = top_.e_out_level_o;
      b.nz = top_.e_out_lvl_nz_o; b.pz = top_.e_out_lvl_pz_o;
      b.nx = top_.e_out_lvl_nx_o; b.px = top_.e_out_lvl_px_o;
      b.surface = top_.e_out_surface_o;
      b.src = top_.e_out_src_id_o;
      beats_.push_back(b);
    }
    if (top_.h_out_valid_o) ++history_writes_;
    top_.clk = 1; top_.eval();
    top_.clk = 0; top_.eval();
    ++clocks_;
  }

  void tick(int n) { for (int i = 0; i < n; ++i) tick(); }

  // ---- fixture loading ----------------------------------------------------
  void write_arena(uint32_t word, uint64_t data) {
    top_.aw_valid_i = 1; top_.aw_word_i = word; top_.aw_data_i = data;
    tick();
    top_.aw_valid_i = 0;
  }

  void load_list(const std::vector<Rec>& recs, uint32_t off_bytes) {
    std::vector<uint8_t> bytes(recs.size() * 32, 0);
    for (size_t i = 0; i < recs.size(); ++i) encode_record(recs[i], &bytes[i * 32]);
    for (size_t w = 0; w * 8 < bytes.size(); ++w) {
      uint64_t d = 0;
      for (int b = 0; b < 8; ++b) d |= (uint64_t)bytes[w * 8 + b] << (8 * b);
      write_arena((uint32_t)((off_bytes / 8) + w), d);
    }
    list_crc_ = zhao_abi::zhao_crc32c(0, bytes.data(), bytes.size());
    list_bytes_ = (uint32_t)bytes.size();
    list_off_ = off_bytes;
    list_count_ = (uint16_t)recs.size();
  }

  void residency(int idx, int ix, int iz, bool hit, int slot, int gen) {
    top_.rw_valid_i = 1; top_.rw_idx_i = idx;
    top_.rw_ix_i = (int16_t)ix; top_.rw_iz_i = (int16_t)iz;
    top_.rw_hit_i = hit ? 1 : 0; top_.rw_slot_i = slot; top_.rw_gen_i = gen;
    tick();
    top_.rw_valid_i = 0;
    if (idx + 1 > (int)top_.rw_count_i) top_.rw_count_i = idx + 1;
  }

  // THE NONDEGENERATE FIXTURE. Sixteen DIFFERENT deviation triples and a
  // per-subpatch centre height, so the ladder has something to decide from --
  // the smoke's all-zero page is exactly what this replaces.
  void devstore(int slot, uint32_t dev_base, int cy_base, int prev_level, int hold) {
    for (int sp = 0; sp < 16; ++sp) {
      top_.dw_valid_i = 1;
      top_.dw_slot_i = slot; top_.dw_sp_i = sp;
      top_.dw_dev1_i = dev_base + 37u * sp;
      top_.dw_dev2_i = dev_base / 2 + 11u * sp;
      top_.dw_dev3_i = dev_base / 4 + 3u * sp;
      top_.dw_cy_i = (int16_t)(cy_base + 13 * sp);
      top_.dw_prev_level_i = prev_level;
      top_.dw_prev_morph_i = 0;
      top_.dw_hold_i = hold;
      top_.dw_fresh_i = 1;
      tick();
    }
    top_.dw_valid_i = 0;
  }

  // ---- the frame --------------------------------------------------------
  void freeze_frame() {
    top_.frame_i = 1; tick(); top_.frame_i = 0;
  }

  // ONE PLACE ARMS THE JOB. Every field of SubmitTerrainSet is set here and
  // nowhere else: a section that armed four of the five would get
  // `jobs_refused_o` and a walk that never ran, which reads exactly like the
  // thing under test failing.
  void start_walk() {
    top_.j_valid_i = 1;
    top_.j_epoch_i = top_.cfg_epoch_i;
    top_.j_list_off_i = list_off_;
    top_.j_list_bytes_i = list_bytes_;
    top_.j_list_crc_i = list_crc_;
    top_.j_patch_count_i = list_count_;
    int n = 0;
    while (!(top_.j_valid_i && top_.j_ready_o) && n++ < 2000) tick();
    tick();
    top_.j_valid_i = 0;
  }

  bool wait_walk(int budget = 400000) {
    const bool ok = wait_walk_exact(budget);
    tick(4);
    return ok;
  }

  // NO TRAILING TICKS. `wait_walk` runs four clocks past the end for
  // settling, which is right for everything except a measurement whose window
  // IS the PREPARE pass: those four clocks are EMIT, and a history offer held
  // across them passes legitimately. Section 8 measured four "leaks" that way
  // before this split existed, which is the measurement window being wrong and
  // not the gate.
  bool wait_walk_exact(int budget = 400000) {
    int n = 0;
    while (top_.prep_busy_o && n++ < budget) tick();
    return n < budget;
  }

  // Run one PREPARE pass and wait for it to finish. Returns false on timeout.
  bool prepare(int budget = 400000) { start_walk(); return wait_walk(budget); }

  // Serve one patch on the EMIT side and collect its sixteen decisions.
  // This models `zhao_terrain_spdesc`'s port contract: the record at the
  // compose job's accept, the door on the fill acceptance, the serve edge, and
  // sixteen descriptors.
  std::vector<Beat> emit_patch(int ix, int iz, int src, int pitch_log2,
                               uint32_t dev_base, int cy_base,
                               int prev_level, int hold, int budget = 200000) {
    beats_.clear();
    top_.rec_valid_i = 1; top_.rec_ix_i = (int16_t)ix; top_.rec_iz_i = (int16_t)iz;
    top_.rec_src_id_i = src;
    tick();
    top_.rec_valid_i = 0;

    top_.door_valid_i = 1; top_.door_src_id_i = src;
    tick();
    top_.door_valid_i = 0;

    top_.serve_src_id_i = src;
    top_.serve_valid_i = 1;
    tick();

    for (int sp = 0; sp < 16; ++sp) {
      const int vi = (sp & 3) * kSubEdge + kCentreOff;
      const int vj = (sp >> 2) * kSubEdge + kCentreOff;
      top_.e_sp_valid_i = 1;
      top_.e_sp_cx_i = place_cx(ix, vi, pitch_log2);
      top_.e_sp_cz_i = place_cx(iz, vj, pitch_log2);
      top_.e_sp_cy_i = ((int32_t)(int16_t)(cy_base + 13 * sp)) << 8;
      top_.e_sp_dev1_i = dev_base + 37u * sp;
      top_.e_sp_dev2_i = dev_base / 2 + 11u * sp;
      top_.e_sp_dev3_i = dev_base / 4 + 3u * sp;
      top_.e_sp_prev_level_i = prev_level;
      top_.e_sp_prev_morph_i = 0;
      top_.e_sp_hold_i = hold;
      top_.e_sp_src_id_i = src;
      int n = 0;
      while (!(top_.e_sp_valid_i && top_.e_sp_ready_o) && n++ < budget) tick();
      tick();
    }
    top_.e_sp_valid_i = 0;
    int n = 0;
    while ((int)beats_.size() < 16 && n++ < budget) tick();
    top_.serve_valid_i = 0;
    tick(4);
    return beats_;
  }

  uint64_t clocks() const { return clocks_; }
  uint32_t history_writes() const { return history_writes_; }
  void reset_history_writes() { history_writes_ = 0; }

 private:
  Vtb_terrain_edge_acceptance top_;
  std::vector<Beat> beats_;
  uint64_t clocks_ = 0;
  uint32_t history_writes_ = 0;
  uint32_t list_crc_ = 0, list_bytes_ = 0, list_off_ = 0;
  uint16_t list_count_ = 0;
};

// Index a patch's sixteen beats by subpatch n = {oz[4:3], ox[4:3]}.
std::map<int, Beat> by_subpatch(const std::vector<Beat>& v) {
  std::map<int, Beat> m;
  for (const auto& b : v) {
    if (b.surface) continue;               // the underside replay, consumed
    const int i = (b.ox >> 3) & 3;
    const int j = (b.oz >> 3) & 3;
    m[(j << 2) | i] = b;
  }
  return m;
}

// The camera and governor knobs that make TWO ADJACENT PATCHES CHOOSE
// DIFFERENT LEVELS. Both views enabled, at different positions, which is the
// directive's "both views".
void set_camera(Bench& b, int32_t eye_x, int32_t eye_z) {
  auto& t = b.t();
  t.gv_cam0_x_i = eye_x; t.gv_cam0_y_i = 0; t.gv_cam0_z_i = eye_z;
  t.gv_cam0_scale_i = 0x0180; t.gv_cam0_en_i = 1;
  t.gv_cam1_x_i = eye_x + (1 << 20); t.gv_cam1_y_i = 0; t.gv_cam1_z_i = eye_z;
  t.gv_cam1_scale_i = 0x0080; t.gv_cam1_en_i = 1;
  t.gv_hyst_i = 0; t.gv_min_hold_i = 0;
  // MORPH STEP ZERO, AND IT IS THE FIXTURE'S MOST IMPORTANT KNOB.
  // `zhao_terrain_lod:466-486` moves a level by AT MOST ONE STEP PER PASS, and
  // with a non-zero morph step a coarsening pass does not move the level at
  // all -- it walks the morph factor and holds the level until the factor
  // reaches unity, which is ~6 passes at the console's 10,923. A bench that
  // left the console's value would see EVERY PATCH AT ITS PREVIOUS LEVEL and
  // would conclude the producer does nothing. `morph_step = 0` is the block's
  // own documented immediate-swap configuration and is what makes a single
  // pass observable.
  t.gv_morph_step_i = 0;
}

constexpr int kPitch = 1;            // pitch_log2 = +1, the canonical 2.0 m
constexpr uint32_t kIsland = 0x42;

std::vector<Rec> three_in_a_row() {
  std::vector<Rec> v;
  for (int k = 0; k < 3; ++k) {
    Rec r{};
    r.island = kIsland;
    r.ix = (int16_t)k;
    r.iz = 0;
    r.page_addr = 0x1000 + 0x100 * k;
    r.page_crc = 0xdeadbeef;
    r.flags = 0;
    r.view_mask = 3;
    r.priority = 0;
    r.source_id = 100 + k;
    v.push_back(r);
  }
  return v;
}

// Present a page header for each patch, so TERRAIN.ISLANDSEAL seals and then
// checks. The envelope is computed with the SAME law the seal recomputes,
// which is what makes a deliberate corruption in section 6 detectable.
void present_header(Bench& b, int ix, int iz, int pitch, uint32_t island) {
  auto& t = b.t();
  t.hdr_valid_i = 1;
  t.hdr_pitch_log2_i = (int8_t)pitch;
  t.hdr_island_i = island;
  t.hdr_ix_i = (int16_t)ix;
  t.hdr_iz_i = (int16_t)iz;
  t.hdr_env_x0_i = place_cx(ix, 0, pitch);
  t.hdr_env_z0_i = place_cx(iz, 0, pitch);
  b.tick();
  t.hdr_valid_i = 0;
  b.tick();
}

void load_common(Bench& b, bool middle_resident = true) {
  b.load_list(three_in_a_row(), 0);
  b.residency(0, 0, 0, true, 10, 1);
  b.residency(1, 1, 0, middle_resident, 11, 1);
  b.residency(2, 2, 0, true, 12, 1);
  // THE FIXTURE IS NONDEGENERATE, AND IN THREE INDEPENDENT WAYS: three
  // different deviation scales, three different centre heights, and -- this is
  // the one that makes the seam interesting -- THREE DIFFERENT PREVIOUS
  // LEVELS. The last is the directive's "history" and "mixed neighbouring
  // LODs" in one: the ladder starts each patch from `sp_prev_level_i` and moves
  // one step, so patches that arrive at 0 / 2 / 1 leave at 1 / 3 / 2 and the
  // two seams in this frame are both between DIFFERENT levels. A fixture whose
  // patches all chose the same level would make every seam agree under any
  // constant, which is a test that cannot fail.
  b.devstore(10, 0x000100, 100, /*prev_level=*/0, /*hold=*/0);
  b.devstore(11, 0x004000, 900, /*prev_level=*/2, /*hold=*/0);
  b.devstore(12, 0x020000, 4000, /*prev_level=*/1, /*hold=*/0);
  for (int k = 0; k < 3; ++k) present_header(b, k, 0, kPitch, kIsland);
}

// The seam between patch A (at ix) and patch B (at ix+1): A's +x edge and B's
// -x edge, four lanes each (j = 0..3), compared the way TESS compares them.
struct Seam { int a[4]; int bb[4]; bool agree; };

Seam seam_px(const std::map<int, Beat>& A, const std::map<int, Beat>& B) {
  Seam s{};
  s.agree = true;
  for (int j = 0; j < 4; ++j) {
    const Beat& ba = A.at((j << 2) | 3);   // A's i = 3 column
    const Beat& bb = B.at((j << 2) | 0);   // B's i = 0 column
    s.a[j]  = tess_edge(ba.px, ba.level);
    s.bb[j] = tess_edge(bb.nx, bb.level);
    if (s.a[j] != s.bb[j]) s.agree = false;
  }
  return s;
}

void print_seam(const char* what, const Seam& s) {
  std::printf("  %s: A=[%d %d %d %d] B=[%d %d %d %d] %s\n", what,
              s.a[0], s.a[1], s.a[2], s.a[3],
              s.bb[0], s.bb[1], s.bb[2], s.bb[3],
              s.agree ? "AGREE" : "DISAGREE");
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  int control_levels[4] = {-1, -1, -1, -1};

  // =======================================================================
  // SECTION 1 -- THE CONTROL: THE CONSOLE AS IT SHIPPED, WITH NO PREPARE
  // =======================================================================
  // `prep_valid` is low, so `zhao_terrain_edgequery` answers every edge with
  // the conservative `8'h00` WITHOUT asking the bank -- which is the literal
  // this packet removed. The seam must DISAGREE, and that disagreement is the
  // measurement entry I21's own text got wrong.
  {
    std::printf("SECTION 1 -- control: no PREPARE, the retained 8'h00\n");
    Bench b;
    load_common(b);
    set_camera(b, 0, 0);
    b.freeze_frame();

    auto p0 = by_subpatch(b.emit_patch(0, 0, 100, kPitch, 0x000100, 100, 0, 0));
    auto p1 = by_subpatch(b.emit_patch(1, 0, 101, kPitch, 0x004000, 900, 2, 0));

    check_eq(p0.size(), 16, "control: patch 0 emitted sixteen decisions");
    check_eq(p1.size(), 16, "control: patch 1 emitted sixteen decisions");
    check(b.t().prep_valid_o == 0, "control: prep_valid is LOW (no walk ran)");
    check(b.t().eq_fallback_patches_o >= 2,
          "control: both patches took the conservative fallback");
    check_eq(b.t().eq_queries_answered_o, 0,
             "control: the bank was never queried");
    check_eq(b.t().edge_nz_o, 0, "control: the held answer is 8'h00");
    check_eq(b.t().edge_px_o, 0, "control: the held answer is 8'h00");

    // THE TWO PATCHES MUST HAVE CHOSEN DIFFERENT LEVELS, or the seam test is
    // vacuous. This is the fixture's own positive control.
    bool differ = false;
    for (int j = 0; j < 4; ++j)
      if (p0.at((j << 2) | 3).level != p1.at((j << 2) | 0).level) differ = true;
    check(differ,
          "control: THE FIXTURE IS NONDEGENERATE -- the two patches chose "
          "different levels across the seam");

    Seam s = seam_px(p0, p1);
    print_seam("seam(0,0)+x / (1,0)-x", s);
    check(!s.agree,
          "control: the seam DISAGREES under 8'h00 -- max(0, own) == own, so "
          "the seam is not stitched at all");
    for (int j = 0; j < 4; ++j) control_levels[j] = s.a[j];
    std::printf("  clocks=%llu\n", (unsigned long long)b.clocks());
  }

  // =======================================================================
  // SECTION 2 -- THE REPAIR: THE SAME STIMULUS WITH THE PREPARE PASS
  // =======================================================================
  {
    std::printf("SECTION 2 -- the composed producer: PREPARE, then EMIT\n");
    Bench b;
    load_common(b);
    set_camera(b, 0, 0);
    b.freeze_frame();

    check(b.prepare(), "walk: the PREPARE pass terminated");
    check(b.t().prep_valid_o == 1, "walk: prep_valid is HIGH");
    check_eq(b.t().pw_walks_completed_o, 1, "walk: one walk completed");
    check_eq(b.t().pw_records_walked_o, 3, "walk: three records walked");
    check_eq(b.t().pw_patches_prepared_o, 3, "walk: three patches prepared");
    check_eq(b.t().pw_descriptors_emitted_o, 48, "walk: 3 x 16 descriptors");
    check_eq(b.t().pw_list_crc_mismatch_o, 0, "walk: the list CRC re-folded");
    check_eq(b.t().pw_freeze_broken_o, 0, "walk: the freeze held");
    check_eq(b.t().pw_bridge_errs_o, 0, "walk: no bridge error");
    check_eq(b.t().pw_sub_order_bad_o, 0, "walk: devstore streamed in order");
    check_eq(b.t().pw_place_range_o, 0, "walk: every placement fitted s32");
    check_eq(b.t().pw_pitch_illegal_o, 0, "walk: the island pitch is legal");
    check_eq(b.t().pw_jobs_refused_o, 0, "walk: the job passed the pre-checks");
    check_eq(b.t().er_records_filed_o, 3, "bank: three records filed");
    check_eq(b.t().er_lanes_filed_o, 48, "bank: 3 x 16 lanes filed");
    check_eq(b.t().er_collisions_o, 0, "bank: no collision in a 3-patch frame");
    check_eq(b.t().er_file_out_of_phase_o, 0, "bank: no file outside PREPARE");
    check_eq(b.t().ls_prep_decisions_o, 48, "share: 48 PREPARE decisions");
    check_eq(b.t().ls_ident_mismatch_o, 0, "share: no identity mismatch");
    check_eq(b.t().ls_hist_leak_o, 0, "share: PREPARE committed no history");
    check_eq(b.t().ls_sel_midpatch_o, 0, "share: ownership never moved mid-patch");
    check_eq(b.t().ls_idq_overflow_o, 0,
             "share: the identity queue never overflowed (its POSITIVE CONTROL "
             "is the committed mutant, not this run)");
    check_eq(b.t().ps_lu_ans_unowned_o, 0, "share: every lookup answer had an owner");
    check(b.t().ps_b_dev_grants_o >= 3, "share: PREPARE was granted the store");
    check_eq(b.t().isl_seals_o, 1, "seal: one island generation sealed");
    check_eq(b.t().isl_headers_checked_o, 2, "seal: two headers checked against it");
    check_eq(b.t().isl_pitch_mismatch_o, 0, "seal: every header agreed on pitch");
    check_eq(b.t().isl_envelope_bad_o, 0, "seal: every envelope agreed");
    check_eq((int)b.t().island_pitch_o, kPitch, "seal: the authoritative pitch");

    check_eq(b.t().phase_o, 2, "bank: EMIT is open");

    auto p0 = by_subpatch(b.emit_patch(0, 0, 100, kPitch, 0x000100, 100, 0, 0));
    auto p1 = by_subpatch(b.emit_patch(1, 0, 101, kPitch, 0x004000, 900, 2, 0));

    check_eq(p0.size(), 16, "emit: patch 0 emitted sixteen decisions");
    check_eq(p1.size(), 16, "emit: patch 1 emitted sixteen decisions");
    check_eq(b.t().eq_queries_answered_o, 2, "emit: two queries answered");
    check_eq(b.t().eq_serve_src_mismatch_o, 0,
             "emit: the door queue tracked the serve (THE DETECTOR)");
    check_eq(b.t().eq_serve_no_door_o, 0, "emit: no serve found the queue empty");
    check_eq(b.t().eq_door_src_unknown_o, 0,
             "emit: every door matched a held admitted record");
    check_eq(b.t().eq_descriptor_unarmed_o, 0,
             "emit: the safety valve never had to fire");
    check(b.t().eq_edges_real_o > 0,
          "emit: at least one edge was answered from a REAL decision");
    check_eq(b.t().er_query_out_of_phase_o, 0, "bank: no query outside EMIT");
    check_eq(b.t().er_edges_real_o + b.t().er_edges_fallback_o,
             4u * b.t().er_queries_o,
             "bank: THE COUNTER INVARIANT -- real + fallback == 4 x queries");

    Seam s = seam_px(p0, p1);
    print_seam("seam(0,0)+x / (1,0)-x", s);
    check(s.agree,
          "THE ANSWER: the seam AGREES from both sides -- max(neighbour, own) "
          "is equal, so the two patches tessellate it at one density");

    // AND THE LEVELS ACTUALLY MOVED. An agreement test alone can go green on a
    // fixture that could not have failed; this is what says a number changed.
    bool moved = false;
    for (int j = 0; j < 4; ++j) if (s.a[j] != control_levels[j]) moved = true;
    check(moved,
          "PIXELS MOVED: at least one lane's tessellation level differs from "
          "the control's");
    std::printf("  control=[%d %d %d %d] -> producer=[%d %d %d %d]\n",
                control_levels[0], control_levels[1], control_levels[2],
                control_levels[3], s.a[0], s.a[1], s.a[2], s.a[3]);
    std::printf("  clocks=%llu\n", (unsigned long long)b.clocks());
  }

  // =======================================================================
  // SECTION 3 -- A MISSING PAGE FALLS BACK SYMMETRICALLY
  // =======================================================================
  // The directive names "missing pages". Patch (1,0) is not resident, so the
  // walk SKIPS it -- architecture 2.6 forbids waiting on a page load mid-walk
  // -- and its record is absent from the bank. `ok(P) && ok(N)` is symmetric,
  // so BOTH sides of every seam touching it fall back together. That is a cost
  // in triangles and NOT a crack, and this section is where that is measured
  // rather than asserted.
  {
    std::printf("SECTION 3 -- a MISSING PAGE, and the symmetry that survives it\n");
    Bench b;
    load_common(b, /*middle_resident=*/false);
    set_camera(b, 0, 0);
    b.freeze_frame();
    check(b.prepare(), "missing: the walk terminated");
    check_eq(b.t().pw_skipped_not_resident_o, 1, "missing: one patch skipped");
    check_eq(b.t().pw_patches_prepared_o, 2, "missing: two patches prepared");
    check_eq(b.t().er_records_filed_o, 2, "missing: two records filed");
    check(b.t().prep_valid_o == 1,
          "missing: a miss is NOT a fault -- the walk is still valid");

    auto p0 = by_subpatch(b.emit_patch(0, 0, 100, kPitch, 0x000100, 100, 0, 0));
    auto p1 = by_subpatch(b.emit_patch(1, 0, 101, kPitch, 0x004000, 900, 2, 0));
    Seam s = seam_px(p0, p1);
    print_seam("seam across the missing patch", s);
    check(b.t().er_query_own_missing_o >= 1,
          "missing: the queried patch's own record was reported unusable");
    check(b.t().er_edges_fallback_o >= 4,
          "missing: lanes fell back, and they are COUNTED");

    // AND THE HONEST STATEMENT ABOUT WHAT THE FALLBACK IS WORTH, asserted
    // rather than left as prose, because the directive asks for exactly this:
    // "A bank collision/missing neighbor CANNOT EARN A CRACK-FREE CLAIM merely
    // because both sides return the same sentinel: test the resulting
    // tessellation levels."
    //
    // Both sides DO return the same sentinel here -- 8'h00 -- and the seam
    // STILL DISAGREES, because max(0, own) == own and the two patches' own
    // levels differ. The symmetry law is intact and is not the same property
    // as crack freedom.
    //
    // What makes this a cost and not a shipped hole: in the console a patch
    // that is not resident is not COMPOSED either, so it presents no geometry
    // for the seam to crack against. THIS BENCH EMITS IT ANYWAY, which the
    // console cannot do, and that is why the assertion below is about the
    // SENTINEL and the COUNTERS rather than about agreement.
    check(!s.agree,
          "missing: the seam disagrees under the shared sentinel -- the "
          "fallback is conservative in TRIANGLES and not in CRACKS, measured "
          "here on the live path rather than inherited from entry I21's text");
    check_eq(b.t().eq_edges_real_o, 0,
             "missing: NOT ONE edge was answered from a real decision, so the "
             "disagreement above is the constant's and not the producer's");
    std::printf("  edges_real=%u edges_fallback=%u queries=%u\n",
                b.t().er_edges_real_o, b.t().er_edges_fallback_o,
                b.t().er_queries_o);
  }

  // =======================================================================
  // SECTION 4 -- DEFORMATION UNDER THE WALK BREAKS THE FREEZE, DETECTABLY
  // =======================================================================
  // The directive: "A bake or residency change either waits for the pinned
  // lifetime or produces a DETECTED RESTART; never combine half of one
  // generation with half of another." `witness_bump_i` is the console's
  // deformation mark and residency publication folded into one witness.
  {
    std::printf("SECTION 4 -- DEFORMATION mid-walk: a DETECTED restart\n");
    Bench b;
    load_common(b);
    set_camera(b, 0, 0);
    b.freeze_frame();
    b.t().dev_latency_i = 12;          // slow the store so the walk is long
    check(b.prepare(), "bake: a clean walk terminated first");
    check_eq(b.t().pw_freeze_broken_o, 0, "bake: and its freeze held");
    // Now bump the witness DURING a second walk.
    b.freeze_frame();
    b.start_walk();
    b.tick(3);
    b.t().witness_bump_i = 1; b.tick(); b.t().witness_bump_i = 0;
    b.wait_walk();
    check(b.t().pw_freeze_broken_o >= 1,
          "bake: freeze_broken_o FIRED -- the witness moved under the walk");
    check_eq(b.t().prep_valid_o, 0,
             "bake: prep_valid is LOW, so the whole frame falls back");

    auto p0 = by_subpatch(b.emit_patch(0, 0, 100, kPitch, 0x000100, 100, 0, 0));
    check_eq(p0.size(), 16, "bake: the EMIT pass still completes");
    check_eq(b.t().edge_px_o, 0, "bake: the conservative fallback is served");
    check(b.t().eq_fallback_patches_o >= 1, "bake: the fallback is counted");
  }

  // =======================================================================
  // SECTION 5 -- BACKPRESSURE, ON BOTH SOCKETS AND ON THE CONSUMER
  // =======================================================================
  // The directive names backpressure. Three independent kinds here: the HPS
  // bridge withholding grants, the devstore taking twelve clocks before its
  // first record, and the EMIT consumer refusing beats. The SAME seam must
  // come out the same, because backpressure changes WHEN and never WHAT.
  {
    std::printf("SECTION 5 -- BACKPRESSURE on three sockets\n");
    Bench b;
    load_common(b);
    set_camera(b, 0, 0);
    b.freeze_frame();
    b.t().dev_latency_i = 12;
    // Stall the bridge for a while before letting the walk proceed.
    b.t().hps_stall_i = 1;
    b.start_walk();
    b.tick(50);
    b.t().hps_stall_i = 0;
    b.wait_walk();
    check(b.t().prep_valid_o == 1, "backpressure: the walk still completed");
    check(b.t().pw_store_wait_clocks_o > 0,
          "backpressure: store_wait_clocks_o moved -- THE BUDGET INSTRUMENT, "
          "which is clocks and not bursts");
    auto p0 = by_subpatch(b.emit_patch(0, 0, 100, kPitch, 0x000100, 100, 0, 0));
    auto p1 = by_subpatch(b.emit_patch(1, 0, 101, kPitch, 0x004000, 900, 2, 0));
    Seam s = seam_px(p0, p1);
    print_seam("seam under backpressure", s);
    check(s.agree, "backpressure: the seam STILL agrees");
    // MEASURED AT THE END, not mid-run: the shared store is contended by BOTH
    // requesters during the EMIT pass too, and a check taken before it would
    // have read the walk's share alone.
    check(b.t().ps_b_dev_blocked_clocks_o > 0,
          "backpressure: PREPARE was blocked on the shared store, and it is "
          "charged in clocks");
    std::printf("  store_wait=%u b_dev_blocked=%u gate_wait=%u contended=%u\n",
                b.t().pw_store_wait_clocks_o, b.t().ps_b_dev_blocked_clocks_o,
                b.t().eq_gate_wait_clocks_o, b.t().ps_dev_contended_o);
  }

  // =======================================================================
  // SECTION 6 -- THE ADMISSION CHECK: A HEADER THAT DISAGREES WITH THE ISLAND
  // =======================================================================
  // Owner directive section 2: "Check page pitch, island identity and declared
  // origin/envelope consistently at admission/load. Refuse and COUNT a
  // mismatch before publishing residency; do not silently rescale a page."
  {
    std::printf("SECTION 6 -- ADMISSION: a page header that disagrees\n");
    Bench b;
    load_common(b);
    set_camera(b, 0, 0);
    b.freeze_frame();
    const uint32_t before = b.t().isl_pitch_mismatch_o;
    // The seal holds pitch_log2 = +1. Present a page claiming +2: legal in
    // itself, and NOT this island's.
    present_header(b, 1, 0, kPitch + 1, kIsland);
    check_eq(b.t().isl_pitch_mismatch_o, before + 1,
             "admission: pitch_mismatch_o FIRED on a disagreeing header");
    check_eq((int)b.t().island_pitch_o, kPitch,
             "admission: the SEAL did not move -- the page is not rescaled");

    // An envelope that does not match the sealed pitch.
    const uint32_t env_before = b.t().isl_envelope_bad_o;
    auto& t = b.t();
    t.hdr_valid_i = 1; t.hdr_pitch_log2_i = (int8_t)kPitch;
    t.hdr_island_i = kIsland; t.hdr_ix_i = 2; t.hdr_iz_i = 0;
    t.hdr_env_x0_i = place_cx(2, 0, kPitch) + 4;   // off by four units
    t.hdr_env_z0_i = place_cx(0, 0, kPitch);
    b.tick(); t.hdr_valid_i = 0; b.tick();
    check_eq(b.t().isl_envelope_bad_o, env_before + 1,
             "admission: envelope_bad_o FIRED on a corrupted origin");

    // An illegal pitch, outside spec 1.3's four values.
    const uint32_t ill_before = b.t().isl_pitch_illegal_o;
    present_header(b, 0, 0, 127, kIsland);        // HDR_PITCH_REFUSE
    check_eq(b.t().isl_pitch_illegal_o, ill_before + 1,
             "admission: pitch_illegal_o FIRED on spec-1.3-illegal pitch");

    // A refusal moves the witness, so a walk running across it is invalidated.
    check(b.t().isl_reseals_o == 0,
          "admission: a disagreement is a REFUSAL, never a silent re-seal");
  }

  // =======================================================================
  // SECTION 7 -- CHANGING CAMERA STATE, AND THE FREEZE THAT WAS NOT THERE
  // =======================================================================
  // This is entry I21 blocker C, measured. `veye0_*`/`veye1_*` are
  // HOST-WRITE-SCOPED in the console, so before this packet the SINGLE pass
  // was already re-sampling them ~256 times a frame. `freeze_drift_o` is the
  // instrument that did not exist; here the camera is MOVED mid-pass and the
  // decisions must not follow it.
  {
    std::printf("SECTION 7 -- CHANGING CAMERA STATE mid-pass, and the freeze\n");
    Bench b;
    load_common(b);
    set_camera(b, 0, 0);
    b.freeze_frame();
    check_eq(b.t().ls_freezes_o, 1, "freeze: one freeze taken");
    check_eq(b.t().ls_freeze_drift_o, 0,
             "freeze: no drift before the camera moves");

    check(b.prepare(), "freeze: the walk terminated");
    auto ref0 = by_subpatch(b.emit_patch(0, 0, 100, kPitch, 0x000100, 100, 0, 0));
    auto ref1 = by_subpatch(b.emit_patch(1, 0, 101, kPitch, 0x004000, 900, 2, 0));
    Seam ref = seam_px(ref0, ref1);
    check_eq(b.t().ls_freeze_drift_o, 0,
             "freeze: no drift while the camera is still");

    // MOVE THE CAMERA WITHOUT A FRAME BOUNDARY, AND DO IT WHILE A PASS IS
    // RUNNING. `freeze_drift_o` is deliberately gated on the identity queue
    // being non-empty -- it measures the hazard, which only exists while
    // descriptors are in flight -- so a camera moved between passes moves no
    // counter and would have read as "the instrument does not work". This is
    // exactly a host `SetView` landing mid-frame.
    Bench c;
    load_common(c);
    set_camera(c, 0, 0);
    c.freeze_frame();
    c.t().dev_latency_i = 8;
    c.start_walk();
    c.tick(40);
    set_camera(c, 96 << 16, 0);
    check(c.wait_walk(), "freeze: the walk terminated with the camera moving");
    check(c.t().ls_freeze_drift_o > 0,
          "freeze: freeze_drift_o FIRED -- the live camera left the frozen one, "
          "which is the hazard the old per-patch re-latch absorbed in silence");
    check_eq(c.t().pw_freeze_broken_o, 0,
             "freeze: and a camera move is NOT a freeze BREAK -- the witness "
             "covers residency and bake, not the eye, which the freeze holds");

    auto mov0 = by_subpatch(c.emit_patch(0, 0, 100, kPitch, 0x000100, 100, 0, 0));
    auto mov1 = by_subpatch(c.emit_patch(1, 0, 101, kPitch, 0x004000, 900, 2, 0));
    Seam mov = seam_px(mov0, mov1);
    print_seam("seam after the camera moved (frozen)", mov);
    bool held = true;
    for (int j = 0; j < 4; ++j) if (mov.a[j] != ref.a[j]) held = false;
    check(held,
          "freeze: THE DECISIONS DID NOT FOLLOW THE CAMERA -- the frame's "
          "frozen eye is what both passes decided against");
    check(mov.agree, "freeze: and the seam still agrees");

    // A NEW FRAME re-freezes, and now the decisions may move.
    c.freeze_frame();
    check_eq(c.t().ls_freezes_o, 2, "freeze: the new frame re-froze");
  }

  // =======================================================================
  // SECTION 8 -- HISTORY: PREPARE COMMITS NOTHING
  // =======================================================================
  // The directive: "PREPARE must not commit hysteresis history or advance the
  // state EMIT treats as last frame." The suppression is structural -- the
  // producer of `h_valid` is starved during PREPARE -- and the gate is here
  // anyway, so this section drives the history port DURING the walk and asserts
  // that nothing reaches the store.
  {
    std::printf("SECTION 8 -- HISTORY: PREPARE commits nothing\n");
    Bench b;
    load_common(b);
    set_camera(b, 0, 0);
    b.freeze_frame();
    b.t().dev_latency_i = 6;
    b.start_walk();
    b.reset_history_writes();
    // Offer a history writeback for the whole PREPARE pass.
    b.t().h_valid_i = 1; b.t().h_level_i = 3; b.t().h_morph_i = 77; b.t().h_hold_i = 5;
    b.wait_walk_exact();
    const uint32_t during = b.history_writes();
    b.t().h_valid_i = 0;
    b.tick(4);
    check_eq(during, 0,
             "history: NOTHING reached the store during PREPARE -- the gate held");
    // AND THE COUNTER BESIDE IT IS SHOWN TO FIRE. `hist_leak_o` counts a
    // history beat OFFERED while PREPARE owns the ladder; in the console it
    // reads zero for a STRUCTURAL reason (TERRAIN.JOBISSUE, the producer of
    // `h_valid`, is starved for the whole pass), so its silence there is the
    // kind that has to be earned somewhere else. This is that somewhere else:
    // the offer is made deliberately, the counter moves, and the beat still
    // does not pass.
    check(b.t().ls_hist_leak_o > 0,
          "history: hist_leak_o FIRED on the deliberate offer -- so its zero "
          "in the composed console is a measurement and not a dead wire");

    // In EMIT the SAME offer must pass through, or the gate is a stuck-at.
    b.reset_history_writes();
    b.t().h_valid_i = 1;
    b.tick(8);
    const uint32_t after = b.history_writes();
    b.t().h_valid_i = 0;
    check(after > 0,
          "history: THE NEGATIVE CONTROL -- the identical offer DOES pass in "
          "EMIT, so the zero above is a gate and not a dead wire");
    check_eq(b.t().h_out_level_o, 3, "history: the level passed through unchanged");
  }


  // =======================================================================
  // SECTION 9 -- CONTENTION: BOTH REQUESTERS ASKING AT ONCE
  // =======================================================================
  // `zhao_terrain_prepshare` exists for a case a sequential bench never
  // reaches. Sections 1-8 run PREPARE and EMIT one after the other, so the
  // two requesters never contend, and every contention counter reads zero for
  // a reason that has NOTHING TO DO with the arbiter. That zero would be the
  // flattering kind: it looks like "no contention happened" and means "this
  // run could not produce any".
  //
  // Here the EMIT side is served WHILE the walk is running, which is what the
  // console does -- the compose spine has its own schedule and does not wait
  // for PREPARE.
  {
    std::printf("SECTION 9 -- CONTENTION: both requesters on both shared ports\n");
    Bench b;
    load_common(b);
    set_camera(b, 0, 0);
    b.freeze_frame();
    b.t().dev_latency_i = 20;          // make the store slow enough to overlap
    b.t().a_hog_i = 1;                 // the EMIT assembler holds its start

    // THE PAGER'S LOOKUPS, HELD FOR THE WHOLE WALK. A burst that happens to
    // fall between PREPARE's lookups contends with nothing -- the first
    // writing of this section did exactly that and read `b_lu_blocked = 0`,
    // which is a stimulus gap wearing an arbiter's clothes.
    b.t().a_lu_valid_i = 1;
    b.t().a_lu_ix_i = 0;
    b.t().a_lu_iz_i = 0;
    b.start_walk();

    // AND THE EMIT ASSEMBLER'S devstore read, REPEATEDLY, while the walker
    // holds the port. The bench's requester-A model asks on each RISING serve
    // edge, so one long level asks once; the port is only contended if it asks
    // again while PREPARE owns it.
    for (int burst = 0; burst < 12; ++burst) {
      b.t().serve_src_id_i = (uint16_t)(100 + (burst % 3));
      b.t().serve_valid_i = 1;
      b.tick(10);
      b.t().serve_valid_i = 0;
      b.tick(2);
      b.t().a_lu_ix_i = (int16_t)(burst % 3);
    }

    check(b.wait_walk(), "contention: the walk still terminated");
    b.t().a_lu_valid_i = 0;
    b.t().a_hog_i = 0;
    check(b.t().ps_a_lu_grants_o > 0,
          "contention: the PAGER was granted the lookup port (A-side grants)");
    check(b.t().ps_b_lu_grants_o > 0,
          "contention: and so was PREPARE (B-side grants)");
    check(b.t().ps_a_lu_blocked_clocks_o > 0,
          "contention: a_lu_blocked_clocks_o FIRED -- the pager waited, and the "
          "cost of sharing is charged to the frame-critical side too");
    check(b.t().ps_b_lu_blocked_clocks_o > 0,
          "contention: b_lu_blocked_clocks_o FIRED");
    check(b.t().ps_a_dev_grants_o > 0,
          "contention: the EMIT assembler was granted the store");
    check(b.t().ps_dev_contended_o > 0,
          "contention: dev_contended_o FIRED -- THE ARBITER'S POSITIVE CONTROL. "
          "Without this the blocked-clock counters' silence would be quoting an "
          "arbiter that was never contended");
    check(b.t().ps_a_dev_blocked_clocks_o > 0,
          "contention: a_dev_blocked_clocks_o FIRED -- the EMIT side waited");
    check_eq(b.t().ps_lu_ans_unowned_o, 0,
             "contention: EVERY lookup answer had an owner -- THE DETECTOR, "
             "silent while two requesters share one pipelined port, which is "
             "the only condition under which its silence is worth anything");
    std::printf("  a_lu_grants=%u b_lu_grants=%u a_lu_blocked=%u b_lu_blocked=%u\n",
                b.t().ps_a_lu_grants_o, b.t().ps_b_lu_grants_o,
                b.t().ps_a_lu_blocked_clocks_o, b.t().ps_b_lu_blocked_clocks_o);
    std::printf("  a_dev_grants=%u b_dev_grants=%u contended=%u a_dev_blocked=%u\n",
                b.t().ps_a_dev_grants_o, b.t().ps_b_dev_grants_o,
                b.t().ps_dev_contended_o, b.t().ps_a_dev_blocked_clocks_o);
  }

  // =======================================================================
  // SECTION 10 -- EVERY REMAINING COUNTER, FIRED ON PURPOSE
  // =======================================================================
  // "A detector that has not been shown to FIRE has not been tested." Sections
  // 1-9 leave six of `zhao_terrain_edgequery`'s counters and two of
  // `zhao_terrain_islandseal`'s at zero, which is correct behaviour and
  // therefore no evidence at all. Each is driven here with LEGAL STIMULUS at
  // the block's own boundary, with the healthy case beside it.
  {
    std::printf("SECTION 10 -- every remaining counter, fired on purpose\n");
    Bench b;
    load_common(b);
    set_camera(b, 0, 0);
    b.freeze_frame();

    // --- serve_no_door_o: a serve edge with nothing in the queue -----------
    b.t().serve_src_id_i = 555;
    b.t().serve_valid_i = 1;
    b.tick(6);
    b.t().serve_valid_i = 0;
    b.tick(2);
    check(b.t().eq_serve_no_door_o >= 1,
          "fire: serve_no_door_o -- a serve found the door queue empty");

    // --- door_src_unknown_o: a door whose identity matches no record -------
    b.t().door_valid_i = 1; b.t().door_src_id_i = 0xBEEF;
    b.tick(); b.t().door_valid_i = 0; b.tick();
    check(b.t().eq_door_src_unknown_o >= 1,
          "fire: door_src_unknown_o -- the door matched neither held record, "
          "so it was queued with a coordinate no patch can carry");

    // --- serve_src_mismatch_o: THE DETECTOR ---------------------------------
    // A door pushed under one identity and a serve arriving under another is
    // the exact desync the side queue exists to be caught by. The two values
    // are written by two DIFFERENT enables through two different ports, which
    // is what makes this counter capable of firing at all.
    b.t().rec_valid_i = 1; b.t().rec_ix_i = 0; b.t().rec_iz_i = 0;
    b.t().rec_src_id_i = 100;
    b.tick(); b.t().rec_valid_i = 0;
    b.t().door_valid_i = 1; b.t().door_src_id_i = 100;
    b.tick(); b.t().door_valid_i = 0; b.tick();
    const uint32_t mm_before = b.t().eq_serve_src_mismatch_o;
    b.t().serve_src_id_i = 101;          // NOT the id the door carried
    b.t().serve_valid_i = 1;
    b.tick(6);
    b.t().serve_valid_i = 0;
    b.tick(2);
    check(b.t().eq_serve_src_mismatch_o == mm_before + 1,
          "fire: serve_src_mismatch_o -- THE DETECTOR moved on a deliberate "
          "desync, so its zero in sections 2 and 5 is a measurement");

    // --- door_refused_o: DOORD+1 pushes with no serve -----------------------
    const uint32_t dr_before = b.t().eq_door_refused_o;
    for (int k = 0; k < 6; ++k) {
      b.t().door_valid_i = 1; b.t().door_src_id_i = (uint16_t)(200 + k);
      b.tick();
    }
    b.t().door_valid_i = 0; b.tick();
    check(b.t().eq_door_refused_o > dr_before,
          "fire: door_refused_o -- the fifth push into a four-deep door queue "
          "was refused and COUNTED rather than overwriting an entry");

    // --- descriptor_unarmed_o: UNREACHABLE BY LEGAL STIMULUS ----------------
    // This is the SAFETY VALVE on the descriptor gate, and it is
    // `idq_overflow_o`'s case exactly: no legal input can move it while the
    // query FSM is correct, so "it can fire" would stay an argument for ever.
    //
    // WHY it cannot: `armed_q` is cleared only on a serve edge, and EVERY exit
    // from the query FSM sets it again -- `q_done_i` in Q_WAIT, and
    // `!bank_emit_i` in both Q_REQ and Q_WAIT. So `armed_q == 0` implies the
    // FSM is not idle, and `stuck_c` requires both.
    //
    // The demonstration is therefore a COMMITTED MUTANT with inverted
    // polarity, `tests/mutants/zhao_terrain_edgequery_unarmed_mutant.sv`,
    // driven by `terrain_edgequery_unarmed_mutant`. It is evidence about the
    // INSTRUMENT; this assertion is evidence about the DESIGN.
    check_eq(b.t().eq_descriptor_unarmed_o, 0,
             "fire: descriptor_unarmed_o is ZERO, which is the CORRECT "
             "behaviour -- its ability to fire is the committed mutant's job");

    // --- lu_ans_unowned_o: THE LOOKUP ARBITER'S FAULT DETECTOR --------------
    // An answer arriving with no outstanding grant. The two sides of this
    // comparison are loaded by two DIFFERENT enables -- the owner register on
    // the GRANT handshake, the answer on the directory's own pipeline -- which
    // is the property CLAUDE.md's metadata-bank defect lacked and the reason
    // that block's counter could never fire. `d_lu_ans_valid_i` is an INPUT of
    // `zhao_terrain_prepshare`, so presenting one with the arbiter idle is
    // legal stimulus at its boundary and not a mutation.
    {
      const uint32_t un_before = b.t().ps_lu_ans_unowned_o;
      b.tick(8);                       // let any real lookup settle
      b.t().lu_spurious_i = 1;
      b.tick();
      b.t().lu_spurious_i = 0;
      b.tick(2);
      check(b.t().ps_lu_ans_unowned_o > un_before,
            "fire: lu_ans_unowned_o -- THE DETECTOR moved on an answer the "
            "arbiter never asked for, so its zero in section 9 is a reading");
    }

    // --- query_abandoned_o: A FRAME BOUNDARY LANDING MID-QUERY --------------
    // Real, and not a fault: the bank sweeps on `frame_begin_i` whenever it
    // arrives, and a query in flight at that moment cannot be answered from a
    // bank that is being erased. The query driver abandons it and serves the
    // conservative fallback, which is the console's existing behaviour.
    {
      Bench d;
      load_common(d);
      set_camera(d, 0, 0);
      d.freeze_frame();
      check(d.prepare(), "abandon: a clean walk first");
      check_eq(d.t().phase_o, 2, "abandon: EMIT is open");

      // Arm a query, then sweep the bank underneath it.
      d.t().rec_valid_i = 1; d.t().rec_ix_i = 0; d.t().rec_iz_i = 0;
      d.t().rec_src_id_i = 100;
      d.tick(); d.t().rec_valid_i = 0;
      d.t().door_valid_i = 1; d.t().door_src_id_i = 100;
      d.tick(); d.t().door_valid_i = 0;
      d.t().serve_src_id_i = 100;
      d.t().serve_valid_i = 1;
      d.tick();                        // the serve edge starts the query
      d.t().force_sweep_i = 1;
      d.tick();
      d.t().force_sweep_i = 0;
      d.tick(8);
      d.t().serve_valid_i = 0;
      d.tick(2);
      check(d.t().eq_query_abandoned_o >= 1,
            "fire: query_abandoned_o -- the bank left EMIT with a query in "
            "flight and the driver fell back rather than answering from a "
            "bank that was being erased");
      check_eq(d.t().edge_px_o, 0,
               "abandon: and the answer served is the conservative fallback");
    }

    // --- reseals_o and island_bounced_o -------------------------------------
    // A SECOND ISLAND under one resource epoch. Legal -- nothing forbids a
    // frame spanning two islands -- and it RE-SEALS rather than reporting a
    // mismatch, because the directive scopes authority to "that island
    // generation" and a different generation is a different authority.
    const uint32_t rs_before = b.t().isl_reseals_o;
    present_header(b, 0, 0, kPitch, kIsland + 1);
    check(b.t().isl_reseals_o == rs_before + 1,
          "fire: reseals_o -- a new island generation re-sealed");
    check(b.t().isl_island_bounced_o >= 1,
          "fire: island_bounced_o -- and the bounce inside one epoch is "
          "counted separately, because a frame that alternates re-seals per "
          "page and every page after the first is checked against the wrong "
          "island for one beat");
    check_eq((int)b.t().island_pitch_o, kPitch,
             "fire: and the re-seal took the NEW island's pitch, which here "
             "happens to be the same -- the seal moved, the value did not");
    std::printf("  serve_no_door=%u src_unknown=%u mismatch=%u refused=%u "
                "unarmed=%u reseals=%u bounced=%u\n",
                b.t().eq_serve_no_door_o, b.t().eq_door_src_unknown_o,
                b.t().eq_serve_src_mismatch_o, b.t().eq_door_refused_o,
                b.t().eq_descriptor_unarmed_o, b.t().isl_reseals_o,
                b.t().isl_island_bounced_o);
  }

  std::printf("terrain_edge_acceptance: %d check(s), %d failure(s)\n", checks, fails);
  zhao::exit_hard(fails == 0 ? 0 : 1);
}
