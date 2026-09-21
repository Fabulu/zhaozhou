// forge_page_directed.cpp -- the FORGE_PROGRAM page (spec/cartridge.md 4d).
//
// Owner decision R234 D2 froze this layout. There are THREE statements of it --
// the spec section, `zref::forge_page` and `tools/pack/mkforgeprogram.py` -- and
// one artefact, `tests/golden/forge_program/forge_page_v1.bin`. This test pins
// the model to that artefact, so a layout edit that misses one of the three goes
// red rather than producing two pages that disagree about byte 88.
//
// It is the discipline `geom_ladderbank_directed` applies to the kind-8 ladder
// table, deliberately the same rather than a second one.
//
// WHAT IT CHECKS, AND WHY EACH ONE IS HERE
// ----------------------------------------
//  1. The model's `build` reproduces the committed golden BYTE FOR BYTE.
//  2. The golden round-trips: decode(build(r)) == r, field by field, so a
//     serialiser/parser pair that is wrong in the SAME direction is caught.
//  3. The comparator is SEEN TO FAIL on a planted one-bit corruption -- R95's
//     rule that an instrument reading "equal" is a claim, and the claim to
//     check hardest. Without this the whole file could be vacuous.
//  4. Every member of the refusal taxonomy fires, one legal record mutated one
//     way at a time, and the LEGAL boundary is accepted. A refusal nobody has
//     seen fire is an argument, not a guard.
//  5. The `forge_kind` <-> FAM_* ROTATION, all six values, BOTH directions,
//     against the table `spec/commands.zidl` writes out by hand -- the trap
//     that file shouts about in capitals ("a straight-through assignment is
//     silently wrong for all six values"). The rotation is asserted to be a
//     rotation and NOT an identity, so a future straight-through edit fails.
//  6. Refuse-whole: a page whose LAST record is illegal leaves the output
//     vector untouched, not holding the legal ones before it.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "zhao_sim.hpp"
#include "zref/zref_forge_page.hpp"

namespace fp = zref::forge_page;
using zhao::check;

namespace {

constexpr int32_t kOne = 65536;

fp::Vec3 v3(int32_t x, int32_t y, int32_t z) {
  fp::Vec3 v;
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}

/** The golden's six records, restated so the C++ side is not reading the
 *  packer's Python. If the two ever disagree the byte compare below says so. */
std::vector<fp::Record> golden_records() {
  std::vector<fp::Record> r(6);

  r[0].program_index = 0x000100;
  r[0].family = fp::kFamRibbon;
  r[0].segments = fp::kMaxRibbonSegments;
  r[0].sides = 1;
  r[0].view_mask = 3;
  r[0].src_id = 0x0101;
  r[0].anchor0 = v3(0, 0, 0);
  r[0].anchor1 = v3(0, 8 * kOne, 0);
  r[0].axis_u = v3(kOne, 0, 0);
  r[0].axis_v = v3(0, 0, kOne);
  r[0].axis_w = v3(0, kOne, 0);
  r[0].radius0 = kOne / 8;
  r[0].radius1 = kOne / 8;
  r[0].amp = kOne / 2;
  r[0].branch_amp = kOne / 4;
  r[0].branch_radius = kOne / 16;
  r[0].seed = 0x1234ABCDu;
  r[0].tick_phase_base = 0x0555;
  r[0].branch_count = 2;
  r[0].br[0].attach = 8;
  r[0].br[0].segments = 6;
  r[0].br[0].end = v3(3 * kOne, 5 * kOne, -kOne);
  r[0].br[1].attach = 17;
  r[0].br[1].segments = 8;
  r[0].br[1].end = v3(-2 * kOne, 7 * kOne, 2 * kOne);

  r[1].program_index = 0x000200;
  r[1].family = fp::kFamFan;
  r[1].segments = 1;
  r[1].sides = 8;
  r[1].view_mask = 3;
  r[1].src_id = 0x0202;
  r[1].axis_u = v3(kOne, 0, 0);
  r[1].axis_v = v3(0, 0, kOne);
  r[1].radius0 = 0;
  r[1].radius1 = 2 * kOne;

  r[2].program_index = 0x000300;
  r[2].family = fp::kFamTube;
  r[2].segments = fp::kMaxSegments;
  r[2].sides = fp::kMaxSides;
  r[2].view_mask = 3;
  r[2].src_id = 0x0303;
  r[2].anchor1 = v3(0, 16 * kOne, 0);
  r[2].axis_u = v3(kOne, 0, 0);
  r[2].axis_v = v3(0, 0, kOne);
  r[2].radius0 = kOne;
  r[2].radius1 = kOne / 4;

  r[3].program_index = 0x000400;
  r[3].family = fp::kFamShell;
  r[3].sweep = fp::kSweepDome;
  r[3].segments = 16;
  r[3].sides = 8;
  r[3].view_mask = 3;
  r[3].src_id = 0x0404;
  r[3].anchor1 = v3(0, 4 * kOne, 0);
  r[3].axis_u = v3(kOne, 0, 0);
  r[3].axis_v = v3(0, 0, kOne);
  r[3].radius0 = 4 * kOne;
  r[3].radius1 = 4 * kOne;

  r[4].program_index = 0x000500;
  r[4].family = fp::kFamBillboard;
  r[4].segments = 1;
  r[4].sides = 1;
  r[4].view_mask = 1;
  r[4].src_id = 0x0505;
  r[4].anchor1 = v3(0, 2 * kOne, 0);
  r[4].axis_u = v3(kOne, 0, 0);
  r[4].axis_v = v3(0, 0, kOne);
  r[4].radius0 = kOne;
  r[4].radius1 = kOne;

  r[5].program_index = 0x000600;
  r[5].family = fp::kFamCliff;
  r[5].segments = 32;
  r[5].sides = 1;
  r[5].view_mask = 2;
  r[5].src_id = 0x0606;
  r[5].anchor0 = v3(-8 * kOne, 0, 0);
  r[5].anchor1 = v3(8 * kOne, 0, 0);
  r[5].axis_u = v3(0, kOne, 0);
  r[5].axis_v = v3(0, 0, kOne);
  r[5].radius0 = kOne;
  r[5].radius1 = kOne;
  return r;
}

bool same_vec(const fp::Vec3& a, const fp::Vec3& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool same_record(const fp::Record& a, const fp::Record& b) {
  if (a.program_index != b.program_index || a.family != b.family || a.sweep != b.sweep) return false;
  if (a.segments != b.segments || a.sides != b.sides || a.view_mask != b.view_mask) return false;
  if (a.branch_count != b.branch_count || a.src_id != b.src_id) return false;
  if (!same_vec(a.anchor0, b.anchor0) || !same_vec(a.anchor1, b.anchor1)) return false;
  if (!same_vec(a.axis_u, b.axis_u) || !same_vec(a.axis_v, b.axis_v)) return false;
  if (!same_vec(a.axis_w, b.axis_w)) return false;
  if (a.radius0 != b.radius0 || a.radius1 != b.radius1) return false;
  if (a.amp != b.amp || a.branch_amp != b.branch_amp || a.branch_radius != b.branch_radius)
    return false;
  if (a.seed != b.seed || a.tick_phase_base != b.tick_phase_base) return false;
  for (int k = 0; k < 2; ++k) {
    if (a.br[k].attach != b.br[k].attach || a.br[k].segments != b.br[k].segments) return false;
    if (!same_vec(a.br[k].end, b.br[k].end)) return false;
  }
  return true;
}

/** A known-legal ribbon, the base every refusal below mutates one field of. */
fp::Record legal_ribbon() {
  fp::Record r;
  r.program_index = 0x00ABCD;
  r.family = fp::kFamRibbon;
  r.sweep = fp::kSweepLinear;
  r.segments = 4;
  r.sides = 1;
  r.view_mask = 3;
  r.branch_count = 1;
  r.axis_u = v3(kOne, 0, 0);
  r.axis_v = v3(0, kOne, 0);
  r.axis_w = v3(0, 0, kOne);
  r.radius0 = kOne;
  r.radius1 = kOne;
  r.amp = kOne;
  r.seed = 7u;
  r.br[0].attach = 2;
  r.br[0].segments = 3;
  return r;
}

/** A known-legal tube, for the rules that only bite off the ribbon. */
fp::Record legal_tube() {
  fp::Record r;
  r.program_index = 0x001234;
  r.family = fp::kFamTube;
  r.segments = 8;
  r.sides = 6;
  r.view_mask = 3;
  r.axis_u = v3(kOne, 0, 0);
  r.axis_v = v3(0, 0, kOne);
  r.radius0 = kOne;
  r.radius1 = kOne / 2;
  return r;
}

void expect_refusal(const fp::Record& r, fp::Illegality want, const char* what) {
  const fp::Illegality got = fp::record_legal(r);
  check(got == want, what, (uint64_t)want, (uint64_t)got);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  const std::vector<fp::Record> recs = golden_records();

  // ---- 1. the model reproduces the committed golden byte for byte ---------
  std::vector<uint8_t> golden;
  {
    const char* path = ZHAO_SOURCE_DIR "/tests/golden/forge_program/forge_page_v1.bin";
    std::FILE* f = std::fopen(path, "rb");
    check(f != nullptr, "the committed forge-program golden opens", 1, f != nullptr ? 1 : 0);
    if (f == nullptr) {
      std::fprintf(stderr, "forge_page_directed: cannot open %s\n", path);
      return zhao::report_and_exit("forge_page_directed");
    }
    uint8_t buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) golden.insert(golden.end(), buf, buf + n);
    std::fclose(f);
  }

  const std::vector<uint8_t> model = fp::build(recs);
  check(!model.empty(), "the six golden records are all legal", 1, model.empty() ? 0 : 1);
  check(model.size() == fp::kHeaderBytes + 6 * fp::kRecordBytes, "page is header + 6 x 192 bytes",
        (uint64_t)(fp::kHeaderBytes + 6 * fp::kRecordBytes), (uint64_t)model.size());
  check(model.size() == golden.size(), "model page length equals the golden's",
        (uint64_t)golden.size(), (uint64_t)model.size());

  size_t first_diff = model.size();
  bool identical = model.size() == golden.size();
  for (size_t i = 0; identical && i < model.size(); ++i) {
    if (model[i] != golden[i]) {
      identical = false;
      first_diff = i;
    }
  }
  check(identical, "zref::forge_page::build reproduces the committed golden byte for byte",
        (uint64_t)model.size(), (uint64_t)first_diff);

  // Every record starts on a 64-byte line and occupies three whole ones --
  // the property that makes a MEM.GUARD reader correct.
  check(fp::kHeaderBytes % 64 == 0 && fp::kRecordBytes % 64 == 0,
        "header and record are whole MEM.GUARD lines", 0,
        (uint64_t)((fp::kHeaderBytes % 64) | (fp::kRecordBytes % 64)));

  // ---- 2. round trip ------------------------------------------------------
  {
    std::vector<fp::Record> back;
    const fp::DecodeStatus st = fp::decode(golden.data(), golden.size(), back);
    check(st == fp::kDecodeOk, "the golden decodes", (uint64_t)fp::kDecodeOk, (uint64_t)st);
    check(back.size() == recs.size(), "six records come back", (uint64_t)recs.size(),
          (uint64_t)back.size());
    for (size_t i = 0; i < back.size() && i < recs.size(); ++i)
      check(same_record(back[i], recs[i]), "record survives the round trip",
            (uint64_t)recs[i].program_index, (uint64_t)back[i].program_index);

    // The lookup answers by handle INDEX, and a handle's low byte is its
    // generation -- two generations of one program find the same record.
    fp::Record hit;
    check(fp::lookup(back, 0x000300u, hit) && hit.family == fp::kFamTube,
          "lookup finds the tube by index", fp::kFamTube, hit.family);
    check(fp::index_of_handle(0x00030041u) == 0x000300u, "handle32 index is bits 31:8",
          0x000300u, fp::index_of_handle(0x00030041u));
    check(!fp::lookup(back, 0x000999u, hit), "lookup misses an absent index", 0, 1);
  }

  // ---- 3. THE COMPARATOR IS SEEN TO FAIL ---------------------------------
  // Without this, every "equal" above could be an instrument reading zero.
  {
    std::vector<uint8_t> bent = golden;
    bent[fp::kHeaderBytes + fp::kOffRadius1] ^= 0x01u;  // one bit, deep in a record
    bool bent_same = bent.size() == golden.size();
    for (size_t i = 0; bent_same && i < bent.size(); ++i) bent_same = bent[i] == golden[i];
    check(!bent_same, "the byte comparator FAILS on a planted one-bit corruption", 0,
          bent_same ? 1 : 0);

    // And the DECODER catches it too, because that bit makes the ribbon's two
    // radii differ -- the declared hole, doing its job.
    std::vector<fp::Record> back;
    const fp::DecodeStatus st = fp::decode(bent.data(), bent.size(), back);
    check(st == fp::kDecodeIllegalRecord, "the decoder refuses the corrupted page",
          (uint64_t)fp::kDecodeIllegalRecord, (uint64_t)st);
    check(back.empty(), "a refused page leaves the output UNTOUCHED", 0, (uint64_t)back.size());
  }

  // ---- 4. the refusal taxonomy, every member fired -----------------------
  check(fp::record_legal(legal_ribbon()) == fp::kOk, "the base ribbon is legal", fp::kOk,
        fp::record_legal(legal_ribbon()));
  check(fp::record_legal(legal_tube()) == fp::kOk, "the base tube is legal", fp::kOk,
        fp::record_legal(legal_tube()));

  { fp::Record r = legal_ribbon(); r.program_index = 0x01000000u;
    expect_refusal(r, fp::kBadIndex, "a nonzero high byte in program_index refuses"); }
  { fp::Record r = legal_ribbon(); r.program_index = fp::kProgramIndexMask;
    check(fp::record_legal(r) == fp::kOk, "the widest legal program index is accepted", fp::kOk,
          fp::record_legal(r)); }
  { fp::Record r = legal_ribbon(); r.family = 6;
    expect_refusal(r, fp::kBadFamily, "family 6 refuses -- the six are CLOSED"); }
  { fp::Record r = legal_ribbon(); r.sweep = 2;
    expect_refusal(r, fp::kBadSweep, "sweep 2 refuses"); }
  { fp::Record r = legal_tube(); r.sweep = fp::kSweepDome;
    expect_refusal(r, fp::kBadSweep, "DOME on a TUBE refuses -- shell's sweep is shell's"); }
  { fp::Record r; r.program_index = 1; r.family = fp::kFamShell; r.sweep = fp::kSweepDome;
    r.segments = 4; r.sides = 4; r.view_mask = 3;
    check(fp::record_legal(r) == fp::kOk, "DOME on a SHELL is accepted", fp::kOk,
          fp::record_legal(r)); }
  { fp::Record r = legal_ribbon(); r.segments = 0;
    expect_refusal(r, fp::kBadSegments, "zero segments refuses"); }
  { fp::Record r = legal_ribbon(); r.segments = fp::kMaxRibbonSegments + 1;
    expect_refusal(r, fp::kBadSegments, "25 segments refuses on a ribbon (the owner's bound)"); }
  { fp::Record r = legal_ribbon(); r.segments = fp::kMaxRibbonSegments; r.br[0].attach = 0;
    check(fp::record_legal(r) == fp::kOk, "24 segments is accepted on a ribbon", fp::kOk,
          fp::record_legal(r)); }
  { fp::Record r = legal_tube(); r.segments = fp::kMaxSegments;
    check(fp::record_legal(r) == fp::kOk, "64 segments is accepted off the ribbon", fp::kOk,
          fp::record_legal(r)); }
  { fp::Record r = legal_tube(); r.segments = fp::kMaxSegments + 1;
    expect_refusal(r, fp::kBadSegments, "65 segments refuses -- FORGE.PRIM's frozen limit"); }
  { fp::Record r = legal_tube(); r.sides = 0;
    expect_refusal(r, fp::kBadSides, "zero sides refuses"); }
  { fp::Record r = legal_tube(); r.sides = fp::kMaxSides + 1;
    expect_refusal(r, fp::kBadSides, "9 sides refuses -- FORGE.PRIM's frozen limit"); }
  { fp::Record r = legal_tube(); r.sides = fp::kMaxSides;
    check(fp::record_legal(r) == fp::kOk, "8 sides is accepted", fp::kOk, fp::record_legal(r)); }
  { fp::Record r = legal_ribbon(); r.sides = 2;
    expect_refusal(r, fp::kBadSides, "sides > 1 on an OPEN family refuses"); }
  { fp::Record r = legal_ribbon(); r.view_mask = 0;
    expect_refusal(r, fp::kBadViewMask, "a zero view_mask refuses"); }
  { fp::Record r = legal_ribbon(); r.view_mask = 4;
    expect_refusal(r, fp::kBadViewMask, "a view_mask bit above bit 1 refuses"); }
  { fp::Record r = legal_tube(); r.radius1 = -1;
    expect_refusal(r, fp::kBadRadius, "a negative radius refuses"); }
  { fp::Record r = legal_ribbon(); r.radius1 = r.radius0 + 1;
    expect_refusal(r, fp::kBadRadius,
                   "a TAPERING ribbon refuses -- the declared hole, not an oversight"); }
  { fp::Record r = legal_tube(); r.radius0 = 4 * kOne; r.radius1 = kOne;
    check(fp::record_legal(r) == fp::kOk, "a tapering TUBE is accepted", fp::kOk,
          fp::record_legal(r)); }
  { fp::Record r = legal_ribbon(); r.branch_count = 3;
    expect_refusal(r, fp::kBadBranch, "three branches refuses -- the owner bounded it at two"); }
  { fp::Record r = legal_ribbon(); r.br[0].segments = fp::kMaxBranchSegments + 1;
    expect_refusal(r, fp::kBadBranch, "a 9-segment branch refuses"); }
  { fp::Record r = legal_ribbon(); r.br[0].segments = 0;
    expect_refusal(r, fp::kBadBranch, "a zero-segment ACTIVE branch refuses"); }
  { fp::Record r = legal_ribbon(); r.br[0].attach = (uint8_t)(r.segments + 1);
    expect_refusal(r, fp::kBadBranch, "an attach past the main polyline refuses"); }
  { fp::Record r = legal_ribbon(); r.br[0].attach = r.segments;
    check(fp::record_legal(r) == fp::kOk, "attach == segments is the legal boundary", fp::kOk,
          fp::record_legal(r)); }
  { fp::Record r = legal_ribbon(); r.branch_count = 0; r.br[1].segments = 99;
    check(fp::record_legal(r) == fp::kOk, "an INACTIVE branch is not inspected", fp::kOk,
          fp::record_legal(r)); }
  { fp::Record r = legal_tube(); r.seed = 1u;
    expect_refusal(r, fp::kBadRibbonOnly, "a seed on a tube refuses"); }
  { fp::Record r = legal_tube(); r.amp = 1;
    expect_refusal(r, fp::kBadRibbonOnly, "a jitter amplitude on a tube refuses"); }
  { fp::Record r = legal_tube(); r.axis_w = v3(0, 0, 1);
    expect_refusal(r, fp::kBadRibbonOnly, "a second jitter axis on a tube refuses"); }
  { fp::Record r = legal_tube(); r.br[1].segments = 1;
    expect_refusal(r, fp::kBadRibbonOnly, "a branch descriptor on a tube refuses"); }
  { fp::Record r = legal_tube(); r.tick_phase_base = 1;
    expect_refusal(r, fp::kBadRibbonOnly, "a tick phase on a tube refuses"); }

  // ---- 5. THE ROTATION, both ways, against the zidl's own table ----------
  // `spec/commands.zidl` writes this table out by hand; it is transcribed here
  // so the two can be compared, which is the whole point of transcribing it.
  {
    static const uint8_t kZidlKindForFamily[6] = {
        /* FAM_RIBBON    0 -> */ 1,  // FORGE_RIBBON
        /* FAM_FAN       1 -> */ 2,  // FORGE_RADIAL_FAN
        /* FAM_TUBE      2 -> */ 3,  // FORGE_TUBE
        /* FAM_SHELL     3 -> */ 4,  // FORGE_RADIAL_SHELL
        /* FAM_BILLBOARD 4 -> */ 5,  // FORGE_BILLBOARD_SHEET
        /* FAM_CLIFF     5 -> */ 0,  // FORGE_HEIGHTFIELD_PATCH
    };
    int identities = 0;
    for (uint8_t f = 0; f < fp::kFamilyCount; ++f) {
      const uint8_t k = fp::kind_of_family(f);
      check(k == kZidlKindForFamily[f], "kind_of_family matches spec/commands.zidl's table",
            kZidlKindForFamily[f], k);
      check(fp::family_of_kind(k) == f, "family_of_kind inverts kind_of_family", f,
            fp::family_of_kind(k));
      if (k == f) ++identities;
    }
    // "A straight-through assignment is silently wrong for ALL SIX values."
    check(identities == 0, "the rotation agrees with the identity on NO family", 0,
          (uint64_t)identities);
    check(fp::kind_of_family(6) == 0xFFu, "an illegal family has no kind", 0xFFu,
          fp::kind_of_family(6));
    check(fp::family_of_kind(6) == 0xFFu, "an illegal kind has no family", 0xFFu,
          fp::family_of_kind(6));
  }

  // ---- 6. refuse-whole, and the header's own guards ----------------------
  {
    std::vector<fp::Record> two = {legal_tube(), legal_ribbon()};
    two[1].segments = 99;  // illegal, and it is the LAST record
    check(fp::build(two).empty(), "build refuses a page whose last record is illegal", 1,
          fp::build(two).empty() ? 1 : 0);

    std::vector<fp::Record> dup = {legal_tube(), legal_tube()};
    check(fp::build(dup).empty(), "build refuses two records sharing a program index", 1,
          fp::build(dup).empty() ? 1 : 0);
  }
  {
    std::vector<fp::Record> back;
    std::vector<uint8_t> p = golden;
    p[0] ^= 0xFFu;
    check(fp::decode(p.data(), p.size(), back) == fp::kDecodeBadMagic, "a wrong magic refuses",
          fp::kDecodeBadMagic, fp::decode(p.data(), p.size(), back));

    p = golden;
    p[4] = 2;
    check(fp::decode(p.data(), p.size(), back) == fp::kDecodeBadVersion, "a wrong version refuses",
          fp::kDecodeBadVersion, fp::decode(p.data(), p.size(), back));

    p = golden;
    p[9] = 1;  // a reserved header byte
    check(fp::decode(p.data(), p.size(), back) == fp::kDecodeBadHeaderRsv,
          "a nonzero reserved header byte refuses", fp::kDecodeBadHeaderRsv,
          fp::decode(p.data(), p.size(), back));

    p = golden;
    p[6] = 7;  // records = 7, but only six bodies are present
    check(fp::decode(p.data(), p.size(), back) == fp::kDecodeCountPastEnd,
          "a count running past the declared extent refuses", fp::kDecodeCountPastEnd,
          fp::decode(p.data(), p.size(), back));

    p = golden;
    p[fp::kHeaderBytes + fp::kOffRsv5] = 1;  // a reserved RECORD byte
    check(fp::decode(p.data(), p.size(), back) == fp::kDecodeBadRecordRsv,
          "a nonzero reserved record byte refuses", fp::kDecodeBadRecordRsv,
          fp::decode(p.data(), p.size(), back));

    check(fp::decode(golden.data(), golden.size(), back, 5) == fp::kDecodeCapacity,
          "six records into a five-row reader refuses", fp::kDecodeCapacity,
          fp::decode(golden.data(), golden.size(), back, 5));
    check(fp::decode(golden.data(), golden.size(), back, 6) == fp::kDecodeOk,
          "six records into a six-row reader is accepted", fp::kDecodeOk,
          fp::decode(golden.data(), golden.size(), back, 6));
    check(fp::decode(golden.data(), 8, back) == fp::kDecodeShort, "a truncated header refuses",
          fp::kDecodeShort, fp::decode(golden.data(), 8, back));
  }

  // ---- the topology grid the page describes ------------------------------
  // The vertex count is FORGE.PRIM's walk, restated: a reader that sizes a
  // buffer from the page must get the same number the topology walker emits.
  check(fp::record_vertex_count(recs[2]) == 65 * 8,
        "the worst-case tube is 65 rings of 8 -- 1,024 triangles", 520u,
        (uint64_t)fp::record_vertex_count(recs[2]));
  check(fp::record_vertex_count(recs[4]) == 4, "a billboard is one quad", 4u,
        (uint64_t)fp::record_vertex_count(recs[4]));
  check(fp::record_vertex_count(recs[0]) == 25 * 2, "a 24-segment ribbon is 25 pairs", 50u,
        (uint64_t)fp::record_vertex_count(recs[0]));
  check(fp::record_vertex_count(recs[1]) == 2 * 8, "a fan is two rings of 8", 16u,
        (uint64_t)fp::record_vertex_count(recs[1]));

  return zhao::report_and_exit("forge_page_directed");
}
