// geom_ladderbank_directed.cpp -- GEOM.LOD's constant bank against
// `zref::creature_page`, the frozen CREATURE_FORM ladder layout (owner rulings
// R26 and R68, core entry FORGE.SHADOW's whole refusal).
//
// The bench is BOTH ends of the block: the publication MEM.UPLOAD makes, and
// the asset window it reads through. Every page under test is built by the
// REFERENCE (`zref::creature_page::build`), so the RTL is read against bytes
// nothing in the RTL wrote -- the only arrangement in which a layout
// disagreement can show up at all.
//
// WHAT ACTUALLY DISCRIMINATES, named up front:
//
//   1. EVERY ROW, LOOKED UP BY KEY, FOUR CONSTANTS AT A TIME. A bank that mixed
//      up the two records in a line, or that read a line twice, fails here --
//      and the exact READ COUNT is asserted, because re-reading a line loads
//      the same constants again and is invisible to any content check.
//   2. A REFUSED PAGE CHANGES NOTHING. Wrong magic, wrong version, a count past
//      the declared extent, a count past ROWS, and an ILLEGAL RECORD each
//      refuse the page whole -- and the bench requires the PREVIOUS page's rows
//      to still answer, bit for bit, afterwards. "Fewer rows" is not the test;
//      "the same rows" is.
//   3. THE ILLEGAL RECORD IS THE INTERESTING ONE. It is illegal at record five
//      of seven, so four rows have already been written into the filling half
//      when the refusal fires. Only the double bank makes the live half survive
//      that, and only this case can see it.
//   4. A MISS IS A MISS. An unknown form answers `a_hit_o` low with ZERO
//      constants, never the nearest row and never a held previous answer.
//   5. A DENIED READ ABANDONS THE PAGE, and a publication of another kind is
//      ignored entirely -- the console publishes MATERIAL_SET and MESH_STREAM
//      on the same wire.
//   6. EVERY COUNTER IS SEEN TO MOVE. A detector reading zero is a claim.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vtb_geom_ladderbank.h"
#include "zhao_sim.hpp"
#include "zref/zref_creature_page.hpp"

using zhao::check;
namespace cp = zref::creature_page;

namespace {

constexpr uint32_t kBase = 0x0100'0000u;
constexpr uint8_t kOtherKind = 11;  // MATERIAL_SET, published on the same wire
constexpr size_t kRows = 16;        // the DUT's ROWS

struct Bench {
  Vtb_geom_ladderbank& d;
  std::vector<uint8_t> mem;   // the asset window, at kBase
  int reads = 0;              // requests accepted
  int deny_after = -1;        // deny the Nth request (0-based); -1 = never

  // one in-flight read
  int beats_left = 0;
  uint32_t beat_addr = 0;

  explicit Bench(Vtb_geom_ladderbank& dut) : d(dut) {}

  uint64_t word(uint32_t addr) const {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
      const size_t o = static_cast<size_t>(addr - kBase) + static_cast<size_t>(i);
      const uint8_t b = o < mem.size() ? mem[o] : 0xEEu;
      v |= static_cast<uint64_t>(b) << (8 * i);
    }
    return v;
  }

  void idle_inputs() {
    d.pub_valid_i = 0;
    d.pub_tag_i = 0;
    d.pub_base_i = 0;
    d.pub_extent_i = 0;
    d.q_valid_i = 0;
    d.q_form_i = 0;
  }

  void cycle() {
    d.rsp_ready_i = 0;
    d.rsp_ok_i = 0;
    d.rsp_violation_i = 0;
    d.beat_valid_i = 0;
    d.beat_data_i = 0;
    const bool req = (d.req_valid_o != 0);
    if (req && beats_left == 0) {
      const bool deny = (deny_after >= 0 && reads == deny_after);
      d.rsp_ready_i = 1;
      if (deny) {
        d.rsp_violation_i = 1;
      } else {
        d.rsp_ok_i = 1;
        // THE SHAPE RULE, CHECKED RATHER THAN ASSUMED. MEM.GUARD refuses a
        // request whose byte mask does not match its length, so a bank that
        // asked for 64 bytes with a partial mask would be denied in silicon
        // while passing every content check here.
        check(d.req_len_o == 64, "the read asks for a whole 64-byte line", 64, d.req_len_o);
        check(d.req_be_o == 0xFFFFFFFFFFFFFFFFull,
              "with the mask its length requires", 1,
              d.req_be_o == 0xFFFFFFFFFFFFFFFFull);
        check(d.req_write_o == 0, "and it is a READ", 0, d.req_write_o);
        beat_addr = d.req_addr_o;
        beats_left = 8;
      }
      ++reads;
    } else if (beats_left > 0) {
      d.beat_valid_i = 1;
      d.beat_data_i = word(beat_addr);
      beat_addr += 8;
      --beats_left;
    }

    d.eval();
    zhao::tick(d);
    idle_inputs();
    d.eval();
  }

  void publish(uint8_t kind, uint32_t base, uint32_t extent) {
    d.pub_valid_i = 1;
    d.pub_tag_i = kind;
    d.pub_base_i = base;
    d.pub_extent_i = extent;
    d.eval();
    zhao::tick(d);
    idle_inputs();
    d.eval();
  }

  void run(int cycles) {
    for (int i = 0; i < cycles; ++i) cycle();
  }

  // One lookup: present the key for a cycle and read the registered answer.
  bool lookup(uint32_t form, cp::Record& out) {
    d.q_valid_i = 1;
    d.q_form_i = form & cp::kFormIndexMask;
    d.eval();
    zhao::tick(d);
    d.q_valid_i = 0;
    d.q_form_i = 0;
    d.eval();
    const bool hit = (d.a_hit_o != 0);
    check(d.a_valid_o != 0, "the answer is valid the cycle after the query", 1, d.a_valid_o);
    out.form_index = form & cp::kFormIndexMask;
    out.bound_radius = static_cast<int32_t>(d.a_bound_o);
    out.micro_error = static_cast<int32_t>(d.a_micro_o);
    out.splat_error = static_cast<int32_t>(d.a_splat_o);
    out.glint_error = static_cast<int32_t>(d.a_glint_o);
    return hit;
  }
};

cp::Record make(uint32_t form, int32_t seed) {
  cp::Record r;
  r.form_index = form;
  // Distinctive, wide, and ordered the way a compiled creature's are:
  // splat = bound/2 and glint = bound (zref_creature.hpp), micro measured.
  r.bound_radius = 0x0001'0000 + seed * 0x0000'3B9Du;
  r.micro_error = 0x0000'0400 + seed * 0x0000'0137u;
  r.splat_error = r.bound_radius / 2;
  r.glint_error = r.bound_radius;
  return r;
}

void check_row(Bench& b, const cp::Record& want, const char* what) {
  cp::Record got;
  const bool hit = b.lookup(want.form_index, got);
  char nm[128];
  std::snprintf(nm, sizeof nm, "%s: form 0x%06X is resident", what, want.form_index);
  check(hit, nm, 1, hit);
  std::snprintf(nm, sizeof nm, "%s: form 0x%06X bound_radius", what, want.form_index);
  check(got.bound_radius == want.bound_radius, nm, want.bound_radius, got.bound_radius);
  std::snprintf(nm, sizeof nm, "%s: form 0x%06X micro_error", what, want.form_index);
  check(got.micro_error == want.micro_error, nm, want.micro_error, got.micro_error);
  std::snprintf(nm, sizeof nm, "%s: form 0x%06X splat_error", what, want.form_index);
  check(got.splat_error == want.splat_error, nm, want.splat_error, got.splat_error);
  std::snprintf(nm, sizeof nm, "%s: form 0x%06X glint_error", what, want.form_index);
  check(got.glint_error == want.glint_error, nm, want.glint_error, got.glint_error);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vtb_geom_ladderbank;  // heap + exit_hard: see zhao_sim.hpp
  Bench b(*top);
  b.idle_inputs();
  top->rsp_ready_i = 0;
  top->rsp_ok_i = 0;
  top->rsp_violation_i = 0;
  top->beat_valid_i = 0;
  top->rst_n = 0;
  top->eval();
  for (int i = 0; i < 3; ++i) zhao::tick(*top);
  top->rst_n = 1;
  top->eval();

  // ---- 0. an empty bank misses, and answers ZERO ---------------------------
  {
    cp::Record got;
    const bool hit = b.lookup(0x00ABCDu, got);
    check(!hit, "an unloaded bank misses", 0, hit);
    check(got.bound_radius == 0, "and answers a zero bound radius, not a stale one", 0,
          got.bound_radius);
    check(top->lookup_miss_o == 1, "the miss is counted", 1, top->lookup_miss_o);
  }

  // ---- 1. a seven-record page: an ODD count, so the last line's second
  // record must not be stored -----------------------------------------------
  std::vector<cp::Record> want;
  for (int i = 0; i < 7; ++i)
    want.push_back(make(0x000100u + static_cast<uint32_t>(11 * i), i));
  b.mem = cp::build(want);
  {
    std::vector<cp::Record> ref;
    const cp::Verdict v = cp::decode(b.mem.data(), b.mem.size(), kRows, ref);
    check(v == cp::Verdict::kOk, "the reference decodes its own page", 0,
          static_cast<uint32_t>(v));
    check(ref.size() == want.size(), "and round-trips every record", want.size(), ref.size());
    for (size_t i = 0; i < ref.size(); ++i)
      check(ref[i].bound_radius == want[i].bound_radius && ref[i].form_index == want[i].form_index,
            "round-trip is exact", 1,
            ref[i].bound_radius == want[i].bound_radius);
  }
  const int reads_before_page1 = b.reads;
  b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
  b.run(400);

  check(top->pages_o == 1, "the page was adopted", 1, top->pages_o);
  check(top->records_o == 7, "seven rows stored", 7, top->records_o);
  // FIVE reads: the header line plus ceil(7/2) = 4 record lines. Asserted
  // exactly, because a bank that re-read a line would still hold the right
  // constants and every content check would still pass.
  check(b.reads - reads_before_page1 == 5,
        "one header read plus four record lines, and no line read twice", 5,
        b.reads - reads_before_page1);
  for (const cp::Record& r : want) check_row(b, r, "page 1");
  check(top->bad_magic_o == 0 && top->truncated_o == 0 && top->denied_o == 0
            && top->bad_record_o == 0 && top->overflow_o == 0,
        "a good page is refused by nothing", 1,
        top->bad_magic_o == 0 && top->truncated_o == 0 && top->denied_o == 0
            && top->bad_record_o == 0 && top->overflow_o == 0);

  // A form NOT in the page misses, with the bank fully loaded. This is the
  // check that separates "answers a miss" from "answers row zero".
  {
    cp::Record got;
    const bool hit = b.lookup(0x00FFFFu, got);
    check(!hit, "a form with no row misses even on a loaded bank", 0, hit);
    check(got.bound_radius == 0, "and still answers zero, not the nearest row", 0,
          got.bound_radius);
  }

  // ---- 2. a publication of ANOTHER KIND is ignored entirely ---------------
  {
    const int reads_before = b.reads;
    b.publish(kOtherKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(60);
    check(top->pages_o == 1, "a MATERIAL_SET publication is not a creature form", 1,
          top->pages_o);
    check(b.reads == reads_before, "and nothing was read for it", reads_before, b.reads);
  }

  // ---- 3. FIVE REFUSALS, each leaving page 1's rows exactly as they were --
  {
    b.mem = cp::build(want);
    b.mem[0] = 0xFFu;  // wrong magic
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(200);
    check(top->bad_magic_o == 1, "a wrong magic refuses the page", 1, top->bad_magic_o);
    check(top->pages_o == 1, "and adopts nothing", 1, top->pages_o);
    for (const cp::Record& r : want) check_row(b, r, "after bad magic");
  }
  {
    b.mem = cp::build(want);
    b.mem[4] = 0x02u;  // wrong version
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(200);
    check(top->bad_magic_o == 2, "a wrong version refuses the page the same way", 2,
          top->bad_magic_o);
    for (const cp::Record& r : want) check_row(b, r, "after bad version");
  }
  {
    // The page is honest; the PUBLICATION declares less of it than the count
    // needs, which is the shape a short upload has.
    b.mem = cp::build(want);
    b.publish(cp::kPageKind, kBase, 128u);
    b.run(200);
    check(top->truncated_o == 1, "a count past the declared extent refuses the page", 1,
          top->truncated_o);
    for (const cp::Record& r : want) check_row(b, r, "after truncation");
  }
  {
    // Shorter than a header. Refused before a single byte is asked for.
    const int reads_before = b.reads;
    b.publish(cp::kPageKind, kBase, 32u);
    b.run(60);
    check(top->truncated_o == 2, "a page too short to hold a header is truncated", 2,
          top->truncated_o);
    check(b.reads == reads_before, "and nothing was read for it at all", reads_before,
          b.reads);
  }
  {
    // More records than the bank has rows. Refused WHOLE, not truncated to
    // ROWS -- a bank keeping the first sixteen would answer some lookups from
    // this page and others from the last one.
    std::vector<cp::Record> big;
    for (int i = 0; i < static_cast<int>(kRows) + 1; ++i)
      big.push_back(make(0x000900u + static_cast<uint32_t>(i), i + 40));
    b.mem = cp::build(big);
    std::vector<cp::Record> ref;
    check(cp::decode(b.mem.data(), b.mem.size(), kRows, ref) == cp::Verdict::kOverflow,
          "the reference calls an over-long page kOverflow", 1, 1);
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(300);
    check(top->overflow_o == 1, "more records than ROWS refuses the page", 1,
          top->overflow_o);
    check(top->pages_o == 1, "and adopts nothing", 1, top->pages_o);
    for (const cp::Record& r : want) check_row(b, r, "after overflow");
  }

  // ---- 4. AN ILLEGAL RECORD, FOUR ROWS IN -- the double bank's whole point -
  {
    std::vector<cp::Record> bad = want;
    bad[4].bound_radius = 0;  // `zref::lod_raw` divides by this
    check(!cp::record_legal(bad[4]), "the reference calls a zero bound radius illegal", 1,
          !cp::record_legal(bad[4]));
    b.mem = cp::build(bad);
    std::vector<cp::Record> ref;
    check(cp::decode(b.mem.data(), b.mem.size(), kRows, ref) == cp::Verdict::kBadRecord,
          "...and the whole page kBadRecord", 1, 1);
    const uint32_t records_before = top->records_o;
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(300);
    check(top->bad_record_o == 1, "a zero bound radius refuses the page", 1,
          top->bad_record_o);
    check(top->pages_o == 1, "and adopts nothing", 1, top->pages_o);
    // Four legal rows were written into the FILLING half before record five
    // was judged. They are counted; they are not visible.
    check(top->records_o == records_before + 4,
          "the four legal rows before it were written into the filling half",
          records_before + 4, top->records_o);
    for (const cp::Record& r : want)
      check_row(b, r, "after a bad record four rows in");
  }
  {
    // A negative error is illegal for the same reason: the ladder's
    // divide-free identities hold only for a non-negative numerator.
    std::vector<cp::Record> bad = want;
    bad[1].micro_error = -1;
    b.mem = cp::build(bad);
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(300);
    check(top->bad_record_o == 2, "a negative error refuses the page too", 2,
          top->bad_record_o);
    for (const cp::Record& r : want) check_row(b, r, "after a negative error");
  }

  // ---- 5. A DENIED READ ABANDONS THE PAGE --------------------------------
  {
    b.mem = cp::build(want);
    b.deny_after = b.reads;  // deny the very next request: the header's
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(200);
    b.deny_after = -1;
    check(top->denied_o == 1, "a denied header read is counted", 1, top->denied_o);
    check(top->pages_o == 1, "and the page is not adopted", 1, top->pages_o);
    for (const cp::Record& r : want) check_row(b, r, "after a denied header read");
  }
  {
    // ...and mid-page, after rows have already been written into the filling
    // half. The live half is untouched.
    b.mem = cp::build(want);
    b.deny_after = b.reads + 2;   // header, first record line, then DENY
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(200);
    b.deny_after = -1;
    check(top->denied_o == 2, "a denied record read is counted", 2, top->denied_o);
    check(top->pages_o == 1, "and the page is still not adopted", 1, top->pages_o);
    for (const cp::Record& r : want) check_row(b, r, "after a denied record read");
  }

  // ---- 6. A PUBLICATION WHILE BUSY IS DROPPED AND COUNTED ----------------
  {
    b.mem = cp::build(want);
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(3);                                     // still walking the header
    check(top->busy_o != 0, "the bank is mid-load", 1, top->busy_o);
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(300);
    check(top->pages_dropped_o == 1, "a publication while busy is dropped and counted", 1,
          top->pages_dropped_o);
    check(top->pages_o == 2, "the FIRST page still completes", 2, top->pages_o);
  }

  // ---- 7. A SECOND, DIFFERENT PAGE REPLACES THE BANK WHOLE ---------------
  {
    std::vector<cp::Record> two;
    for (int i = 0; i < 3; ++i)
      two.push_back(make(0x000500u + static_cast<uint32_t>(i), i + 20));
    b.mem = cp::build(two);
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(300);
    check(top->pages_o == 3, "the replacement page was adopted", 3, top->pages_o);
    for (const cp::Record& r : two) check_row(b, r, "page 2");
    // And page 1's rows are GONE -- the bank is the page, not the union of
    // every page ever published.
    cp::Record got;
    check(!b.lookup(want[0].form_index, got),
          "page 1's rows are gone: a bank is one page, not a union", 0, 1);
  }

  // ---- 8. AN EMPTY PAGE IS A LEGAL LOAD, and empties the bank ------------
  {
    b.mem = cp::build({});
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(120);
    check(top->pages_o == 4, "an empty page IS a load -- 'no creature types' is a statement",
          4, top->pages_o);
    cp::Record got;
    check(!b.lookup(0x000500u, got), "and every lookup then misses honestly", 0, 1);
  }

  // ---- 8a. THE THREE STATEMENTS OF THE LAYOUT ARE PINNED TO ONE ARTEFACT --
  // `tools/pack/mkcreatureladder.py` writes the committed golden from its own
  // constants; `zref::creature_page::build` must reproduce it byte for byte
  // here; and the RTL below loads that same file. Packer, model and reader are
  // then pinned to an artefact rather than to each other's good intentions,
  // and a layout edit that misses one of the three goes red.
  std::vector<uint8_t> golden;
  {
    const char* path = ZHAO_SOURCE_DIR "/tests/golden/creature_ladder/ladder_page_v1.bin";
    std::FILE* f = std::fopen(path, "rb");
    check(f != nullptr, "the committed ladder-page golden opens", 1, f != nullptr);
    if (f) {
      uint8_t buf[4096];
      size_t n;
      while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) golden.insert(golden.end(), buf, buf + n);
      std::fclose(f);
    }
    // The packer's own three records, restated here for the same reason the
    // RTL restates the bit offsets: a change on either side must FAIL, not
    // track silently.
    std::vector<cp::Record> gr(3);
    gr[0].form_index = 0x000100u; gr[0].bound_radius = 49152;
    gr[0].micro_error = 1024; gr[0].splat_error = 24576; gr[0].glint_error = 49152;
    gr[1].form_index = 0x000101u; gr[1].bound_radius = 65536;
    gr[1].micro_error = 1311; gr[1].splat_error = 32768; gr[1].glint_error = 65536;
    gr[2].form_index = 0x00A017u; gr[2].bound_radius = 163840;
    gr[2].micro_error = 4096; gr[2].splat_error = 81920; gr[2].glint_error = 163840;
    const std::vector<uint8_t> model = cp::build(gr);
    check(model.size() == golden.size(),
          "zref::creature_page::build agrees with the packer on the page LENGTH",
          golden.size(), model.size());
    bool same = model.size() == golden.size();
    for (size_t i = 0; same && i < model.size(); ++i) same = model[i] == golden[i];
    check(same, "...and on every byte of it", 1, same);
  }

  // ---- 8b. and the RTL reads THAT FILE ------------------------------------
  {
    const uint32_t pages_before = top->pages_o;
    const uint32_t records_before = top->records_o;
    b.mem = golden;
    b.publish(cp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(300);
    check(top->pages_o == pages_before + 1, "the RTL adopts the packer's own page",
          pages_before + 1, top->pages_o);
    check(top->records_o == records_before + 3, "all three rows", records_before + 3,
          top->records_o);
    cp::Record g0, g1, g2;
    check(b.lookup(0x000100u, g0) && g0.bound_radius == 49152 && g0.splat_error == 24576,
          "golden row 0 reads back through the RTL", 1,
          g0.bound_radius == 49152 && g0.splat_error == 24576);
    check(b.lookup(0x000101u, g1) && g1.micro_error == 1311,
          "golden row 1 reads back through the RTL", 1, g1.micro_error == 1311);
    check(b.lookup(0x00A017u, g2) && g2.glint_error == 163840,
          "golden row 2 reads back through the RTL", 1, g2.glint_error == 163840);
  }

  // ---- 9. every counter was seen to move ---------------------------------
  check(top->pages_o > 0 && top->records_o > 0 && top->pages_dropped_o > 0
            && top->bad_magic_o > 0 && top->truncated_o > 0 && top->bad_record_o > 0
            && top->overflow_o > 0 && top->denied_o > 0 && top->lookup_miss_o > 0,
        "every counter this block owns was FIRED by stimulus", 1,
        top->pages_o > 0 && top->records_o > 0 && top->pages_dropped_o > 0
            && top->bad_magic_o > 0 && top->truncated_o > 0 && top->bad_record_o > 0
            && top->overflow_o > 0 && top->denied_o > 0 && top->lookup_miss_o > 0);

  std::printf(
      "[geom_ladderbank_directed] pages=%u records=%u dropped=%u bad_magic=%u truncated=%u "
      "bad_record=%u overflow=%u denied=%u miss=%u reads=%d\n",
      top->pages_o, top->records_o, top->pages_dropped_o, top->bad_magic_o, top->truncated_o,
      top->bad_record_o, top->overflow_o, top->denied_o, top->lookup_miss_o, b.reads);
  top->final();
  return zhao::report_and_exit("geom_ladderbank_directed");
}
