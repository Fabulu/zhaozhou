// geom_clipread_ownerblind_mutant.cpp -- THE INVERTED-POLARITY DRIVER for
// `tests/mutants/zhao_geom_clipread_ownerblind_mutant.sv`.
//
// IT PASSES WHEN THE BLOCK IS WRONG. That is the whole point: it is evidence
// about an instrument, not about a design.
//
// The owner ruling of 2026-09-21 (kind-8 / kind-9 ownership), section 6 test J:
//
//     "At least one deliberate mutation bypassing each new ownership
//      comparison must turn the corresponding negative test red, while valid
//      traffic stays green. Verify the fault was actually planted before
//      citing the result."
//
// WHAT THIS ANSWERS THAT `geom_clipread_directed` CANNOT. That bench fires
// `owner_mismatch_o` five times, so the counter demonstrably moves. What it
// cannot show is that cases B, C and D go red BECAUSE OF THE OWNERSHIP
// COMPARISON rather than because of something else that happens to refuse the
// same traffic -- the pages there are legal, adopt cleanly, have the same bone
// count, the same clip id, the same frame count and the same frame number, and
// every other refusal in the block is asserted silent. Removing ONE expression
// separates those two explanations, and nothing else does.
//
// THE PLANT IS PROVEN BEFORE THE RESULT IS QUOTED. A fire test compiled
// against an unmutated copy passes for the wrong reason and reports success,
// which is the broken-instrument law's favourite shape. So this driver reads
// the mutant's own source and requires:
//   * the mutated line to be PRESENT, and
//   * the production expression to be ABSENT.
// Both, because either alone is satisfied by a file that contains the two.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Vtb_geom_clipread_ownerblind.h"
#include "zhao_sim.hpp"
#include "zref/zref_clip_page.hpp"
#include "zref/zref_creature_page.hpp"

using zhao::check;
namespace cp = zref::creature_page;
namespace bd = zref::creature_page::body;
namespace clp = zref::clip_page;

namespace {

constexpr uint32_t kBase = 0x0100'0000u;
// The same two forms `geom_clipread_directed` uses, differing ONLY in bits
// 23:16 -- so this fire test is the fire test for the case a truncated key
// would also have got wrong.
constexpr uint32_t kFormA = 0x000123u;
constexpr uint32_t kFormB = 0x010123u;
constexpr int kBones = 6;

std::string read_text(const char* path) {
  std::string out;
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return out;
  char buf[8192];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
  std::fclose(f);
  return out;
}

std::vector<bd::BoneRecord> skeleton(int n) {
  std::vector<bd::BoneRecord> v;
  for (int i = 0; i < n; ++i) {
    bd::BoneRecord r;
    r.parent = static_cast<uint8_t>(i == 0 ? 0 : i - 1);
    r.tx = 0x0001'0000 + i * 0x0000'1111;
    r.ty = -(0x0000'2000 + i * 0x0000'0303);
    r.tz = 0x0004'0000 - i * 0x0000'0707;
    v.push_back(r);
  }
  return v;
}

std::vector<cp::Record> one_row(uint32_t form) {
  cp::Record r;
  r.form_index = form;
  r.bound_radius = 0x0002'0000;
  r.micro_error = 0x0000'0400;
  r.splat_error = 0x0001'0000;
  r.glint_error = 0x0002'0000;
  return std::vector<cp::Record>{r};
}

clp::Clip make_clip(uint16_t slot, int frames, int seed) {
  clp::Clip c;
  c.slot_id = slot;
  for (int f = 0; f < frames; ++f) {
    clp::Frame fr;
    fr.root_dx = 0x0000'1000 * (f + 1) + seed;
    fr.root_dy = -0x0000'0800 * (f + 1) - seed;
    fr.root_dz = 0x0000'2000 + f * 7 + seed;
    for (int b = 0; b < kBones; ++b) {
      clp::Quat16 q;
      q.w = static_cast<int16_t>(16000 - 13 * b - 3 * f - seed);
      q.x = static_cast<int16_t>(100 + b);
      q.y = static_cast<int16_t>(-200 - b - f);
      q.z = static_cast<int16_t>(300 + 2 * b + f);
      fr.bones.push_back(q);
    }
    c.frames.push_back(fr);
  }
  return c;
}

struct Bench {
  Vtb_geom_clipread_ownerblind& d;
  std::vector<uint8_t> mem;
  int beats_left = 0;
  uint32_t beat_addr = 0;
  std::vector<uint64_t> quat_fills;

  bool pub_pending = false;
  uint8_t pub_kind = 0;
  uint32_t pub_index = 0, pub_base = 0, pub_extent = 0;
  uint16_t pub_gen = 0;
  bool req_pending = false;
  uint32_t req_form = 0;
  uint16_t req_clip = 0, req_frame = 0;

  explicit Bench(Vtb_geom_clipread_ownerblind& dut) : d(dut) {}

  uint64_t word(uint32_t addr) const {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
      const size_t o = static_cast<size_t>(addr - kBase) + static_cast<size_t>(i);
      v |= static_cast<uint64_t>(o < mem.size() ? mem[o] : 0xEEu) << (8 * i);
    }
    return v;
  }

  void idle() {
    d.pub_valid_i = 0;
    d.pub_tag_i = 0;
    d.pub_index_i = 0;
    d.pub_generation_i = 0;
    d.pub_base_i = 0;
    d.pub_extent_i = 0;
    d.p_valid_i = 0;
    d.p_form_idx_i = 0;
    d.p_clip_id_i = 0;
    d.p_frame_no_i = 0;
  }

  void cycle() {
    if (pub_pending) {
      d.pub_valid_i = 1;
      d.pub_tag_i = pub_kind;
      d.pub_index_i = pub_index;
      d.pub_generation_i = pub_gen;
      d.pub_base_i = pub_base;
      d.pub_extent_i = pub_extent;
      pub_pending = false;
    }
    if (req_pending) {
      d.p_valid_i = 1;
      d.p_form_idx_i = req_form;
      d.p_clip_id_i = req_clip;
      d.p_frame_no_i = req_frame;
      req_pending = false;
    }
    d.rsp_ready_i = 0;
    d.rsp_ok_i = 0;
    d.rsp_violation_i = 0;
    d.beat_valid_i = 0;
    d.beat_data_i = 0;
    if (d.req_valid_o && beats_left == 0) {
      d.rsp_ready_i = 1;
      d.rsp_ok_i = 1;
      beat_addr = d.req_addr_o;
      beats_left = 8;
    } else if (beats_left > 0) {
      d.beat_valid_i = 1;
      d.beat_data_i = word(beat_addr);
      beat_addr += 8;
      --beats_left;
    }
    d.eval();
    if (d.fill_we_o && d.fill_sel_o) quat_fills.push_back(d.fill_data_o);
    zhao::tick(d);
    idle();
    d.eval();
  }

  void run(int n) { for (int i = 0; i < n; ++i) cycle(); }

  void load(uint8_t kind, const std::vector<uint8_t>& page, uint32_t index, uint16_t gen) {
    mem = page;
    pub_pending = true;
    pub_kind = kind;
    pub_index = index;
    pub_gen = gen;
    pub_base = kBase;
    pub_extent = static_cast<uint32_t>(page.size());
    cycle();
    run(600);
  }

  void request(uint32_t form, uint16_t clip_id, uint16_t frame_no) {
    quat_fills.clear();
    req_pending = true;
    req_form = form;
    req_clip = clip_id;
    req_frame = frame_no;
    cycle();
    run(400);
  }
};

}  // namespace

int main() {
  // ---- THE PLANT, proven before anything is measured ---------------------
  const std::string mut = read_text(
      ZHAO_SOURCE_DIR "/tests/mutants/zhao_geom_clipread_ownerblind_mutant.sv");
  check(!mut.empty(), "the mutant source is readable", 1, !mut.empty());
  const bool planted = mut.find("wire owner_ok_c = 1'b1;") != std::string::npos;
  const bool prod_gone =
      mut.find("wire owner_ok_c = (body_owner_q == p_form_idx_i) && "
               "(clip_owner_q == p_form_idx_i);") == std::string::npos;
  check(planted, "PLANT: the mutated `owner_ok_c = 1'b1` is present in the file that "
        "was compiled", 1, planted);
  check(prod_gone, "PLANT: and the production comparison is GONE -- a copy holding both "
        "would compile the real guard and this fire test would report success while "
        "measuring nothing", 1, prod_gone);
  // The module was renamed, so no source list can elaborate it by accident.
  check(mut.find("module zhao_geom_clipread_ownerblind_mutant") != std::string::npos,
        "PLANT: and the module is renamed", 1, 1);
  if (!planted || !prod_gone) return zhao::report_and_exit("geom_clipread_ownerblind_mutant");

  Vtb_geom_clipread_ownerblind* top = new Vtb_geom_clipread_ownerblind;
  Bench b(*top);
  b.idle();
  top->src_ready_i = 1;
  top->rst_n = 0;
  top->eval();
  for (int i = 0; i < 3; ++i) zhao::tick(*top);
  top->rst_n = 1;
  top->eval();

  const std::vector<bd::BoneRecord> bones = skeleton(kBones);
  std::vector<clp::Clip> clipsA, clipsB;
  clipsA.push_back(make_clip(3, 4, 0));
  clipsB.push_back(make_clip(3, 4, 0));   // IDENTICAL CONTENT, different owner
  const std::vector<uint8_t> page8A = cp::build_with_body(one_row(kFormA), bones, kFormA);
  const std::vector<uint8_t> page9B = clp::build(clipsB, kBones, kFormB);
  check(!page8A.empty() && !page9B.empty(), "the fixtures build", 1,
        !page8A.empty() && !page9B.empty());

  // Body A, clips B -- the ruling's case B, at identical bone counts, clip ids,
  // frame counts and frame numbers.
  b.load(cp::kPageKind, page8A, 0x00A001u, 1);
  b.load(clp::kPageKind, page9B, 0x00B002u, 2);
  check(top->res_body_owner_o == kFormA && top->res_clip_owner_o == kFormB,
        "the two resident sections name DIFFERENT forms", 1,
        top->res_body_owner_o == kFormA && top->res_clip_owner_o == kFormB);

  const uint32_t frames_before = top->frames_o;
  b.mem = page9B;
  b.request(kFormA, 3, 2);

  // ---- THE INVERTED VERDICT ----------------------------------------------
  // In PRODUCTION every one of these is the opposite. Here each is the defect.
  check(top->owner_mismatch_o == 0,
        "MUTANT: `owner_mismatch_o` stays ZERO with the comparison removed -- the "
        "negative test's refusal came from the ownership check and from nothing else",
        0, top->owner_mismatch_o);
  check(top->frames_o == frames_before + 1,
        "MUTANT: draw A is served from creature B's clip bank", frames_before + 1,
        top->frames_o);
  check(top->bone_mismatch_o == 0,
        "MUTANT: and the bone-count detector is SILENT throughout -- 6 bones under 6 "
        "bones agrees, which is exactly why a bone check cannot stand in for an "
        "ownership check", 0, top->bone_mismatch_o);
  check(top->not_resident_o == 0,
        "MUTANT: and residency is satisfied -- the pages are legal and adopted", 0,
        top->not_resident_o);
  check(!b.quat_fills.empty(),
        "MUTANT: A WELL-FORMED PALETTE FOR THE WRONG ANIMAL reaches the store. This "
        "is the defect the ruling exists to make impossible, and it is reachable "
        "only with the guard removed.", 1, !b.quat_fills.empty());

  std::printf("[geom_clipread_ownerblind_mutant] frames=%u owner_mismatch=%u "
              "bone_mismatch=%u quat_words=%zu\n",
              top->frames_o, top->owner_mismatch_o, top->bone_mismatch_o,
              b.quat_fills.size());
  top->final();
  return zhao::report_and_exit("geom_clipread_ownerblind_mutant");
}
