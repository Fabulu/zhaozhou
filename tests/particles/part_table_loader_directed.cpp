// part_table_loader_directed.cpp -- PART.TABLE's host against
// `zref::species_page`, the frozen SPECIES_TABLE layout (owner ruling R42,
// core entry I33).
//
// The bench is BOTH ends of the block: the publication MEM.UPLOAD makes, and
// the asset window it reads through. The page under test is built by the
// REFERENCE (`zref::species_page::build`), so the RTL is read against a page
// nothing in the RTL wrote -- which is the only arrangement in which a layout
// disagreement can show up at all.
//
// WHAT ACTUALLY DISCRIMINATES, named up front:
//
//   1. THE WHOLE STREAM, IN ORDER, BIT FOR BIT. Every entry's sel, index,
//      event and 141-bit data word is compared against the reference's decode
//      of the same bytes. A loader that mixed up the two entries in a line, or
//      that read a line twice, fails here and nowhere else -- and reading a
//      line twice is invisible to any check that only looks at the table's
//      final contents, because loading the same descriptor twice is
//      idempotent. The exact READ COUNT is asserted for that reason.
//   2. A REFUSED PAGE LOADS NOTHING. Wrong magic, wrong version and a count
//      that runs past the declared extent each refuse the page WHOLE, and the
//      bench requires ZERO entries to have been handed over -- not "fewer".
//   3. A DENIED READ ABANDONS THE PAGE. MEM.GUARD drops a denied request, so
//      continuing would load the table out of whatever the beat bus held.
//   4. A PUBLICATION WHILE BUSY IS DROPPED AND COUNTED. Two species tables in
//      flight is two authors' physics interleaved; the page that lost is a
//      fact the host needs rather than a silence.
//   5. A PUBLICATION OF ANOTHER KIND IS IGNORED ENTIRELY. The console
//      publishes MATERIAL_SET and MESH_STREAM pages on the same wire.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vtb_part_table_loader.h"
#include "zhao_sim.hpp"
#include "zref/zref_species_page.hpp"

using zhao::check;
namespace sp = zref::species_page;

namespace {

constexpr uint32_t kBase = 0x0100'0000u;
constexpr uint8_t kOtherKind = 11;  // MATERIAL_SET, published on the same wire

struct Bench {
  Vtb_part_table_loader& d;
  std::vector<uint8_t> mem;   // the asset window, at kBase
  // what the loader handed over
  std::vector<sp::Entry> got;
  int reads = 0;              // requests accepted
  int deny_after = -1;        // deny the Nth request (0-based); -1 = never
  bool stall_load = false;    // hold ld_ready low

  // one in-flight read
  int beats_left = 0;
  uint32_t beat_addr = 0;

  explicit Bench(Vtb_part_table_loader& dut) : d(dut) {}

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
  }

  void cycle() {
    // ---- the asset window, answering before the edge -----------------------
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
        // request whose byte mask does not match its length, and a loader that
        // asked for 64 bytes with a partial mask would be denied in silicon
        // while passing every content check here.
        check(d.req_len_o == 64, "the read asks for a whole 64-byte line", 64, d.req_len_o);
        check(d.req_be_o == 0xFFFFFFFFFFFFFFFFull,
              "with the mask its length requires", 1,
              d.req_be_o == 0xFFFFFFFFFFFFFFFFull);
        check(d.req_write_o == 0, "and it is a READ", 0, d.req_write_o);
        // The guard address is already absolute -- it is the low 27 bits of
        // the publication's base, and kBase fits in 27. Adding kBase again is
        // the mistake this comment exists to stop being made twice.
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

    d.ld_ready_i = stall_load ? 0 : 1;
    d.eval();

    const bool ld_fire = d.ld_valid_o && d.ld_ready_i;
    sp::Entry e;
    if (ld_fire) {
      e.sel = static_cast<uint8_t>(d.ld_sel_o);
      e.event = static_cast<uint8_t>(d.ld_event_o);
      e.index = static_cast<uint8_t>(d.ld_index_o);
      for (int w = 0; w < 3; ++w) {
        uint64_t v = 0;
        for (int k = 0; k < 2; ++k) {
          const int word_index = 2 * w + k;
          if (word_index * 32 < sp::kLdW)
            v |= static_cast<uint64_t>(d.ld_data_o[word_index]) << (32 * k);
        }
        e.data[w] = v;
      }
    }
    zhao::tick(d);
    if (ld_fire) got.push_back(e);
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
};

sp::Entry make(uint8_t sel, uint8_t index, uint8_t ev, uint32_t seed) {
  sp::Entry e;
  e.sel = sel;
  e.index = index;
  e.event = ev;
  // A distinctive, wide pattern: any bit-offset error shows as a mismatch.
  e.data[0] = 0x9E37'79B9'7F4A'7C15ull * (seed + 1);
  e.data[1] = 0xC2B2'AE3D'27D4'EB4Full * (seed + 3);
  e.data[2] = static_cast<uint64_t>(seed) & 0x1FFFull;  // kLdW = 141 -> 13 bits here
  return e;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vtb_part_table_loader;  // heap + exit_hard: see zhao_sim.hpp
  Bench b(*top);
  b.idle_inputs();
  top->ld_ready_i = 1;
  top->rsp_ready_i = 0;
  top->rsp_ok_i = 0;
  top->rsp_violation_i = 0;
  top->beat_valid_i = 0;
  top->rst_n = 0;
  top->eval();
  for (int i = 0; i < 3; ++i) zhao::tick(*top);
  top->rst_n = 1;
  top->eval();

  // ---- 1. a seven-entry page: an ODD count, so the last line's second
  // entry must NOT be handed over ------------------------------------------
  std::vector<sp::Entry> want;
  for (int i = 0; i < 7; ++i)
    want.push_back(make(static_cast<uint8_t>(i % 4), static_cast<uint8_t>(3 * i + 1),
                        static_cast<uint8_t>(i % 4), static_cast<uint32_t>(i)));
  b.mem = sp::build(want);
  {
    std::vector<sp::Entry> ref;
    const sp::Verdict v = sp::decode(b.mem.data(), b.mem.size(), ref);
    check(v == sp::Verdict::kOk, "the reference decodes its own page", 0,
          static_cast<uint32_t>(v));
    check(ref.size() == want.size(), "and round-trips every entry", want.size(), ref.size());
  }
  b.publish(sp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
  b.run(400);

  check(top->pages_o == 1, "the page loaded", 1, top->pages_o);
  check(top->entries_o == 7, "seven entries handed over", 7, top->entries_o);
  check(b.got.size() == 7, "...and the bench saw seven", 7, b.got.size());
  // FOUR reads: the header line plus ceil(7/2) = 4 entry lines. Asserted
  // exactly, because a loader that re-read a line would still load the right
  // descriptors and every content check would still pass.
  check(b.reads == 5, "one header read plus four entry lines, and no line read twice", 5,
        b.reads);
  for (size_t i = 0; i < b.got.size() && i < want.size(); ++i) {
    char nm[96];
    std::snprintf(nm, sizeof nm, "entry %zu: sel", i);
    check(b.got[i].sel == want[i].sel, nm, want[i].sel, b.got[i].sel);
    std::snprintf(nm, sizeof nm, "entry %zu: index", i);
    check(b.got[i].index == want[i].index, nm, want[i].index, b.got[i].index);
    std::snprintf(nm, sizeof nm, "entry %zu: event", i);
    check(b.got[i].event == want[i].event, nm, want[i].event, b.got[i].event);
    for (int w = 0; w < 3; ++w) {
      // Only kLdW bits are meaningful; mask the reference the same way the
      // wire does.
      uint64_t mask = ~0ull;
      const int lo = 64 * w;
      if (lo >= sp::kLdW) {
        mask = 0;
      } else if (lo + 64 > sp::kLdW) {
        mask = (1ull << (sp::kLdW - lo)) - 1ull;
      }
      std::snprintf(nm, sizeof nm, "entry %zu: data word %d", i, w);
      check(b.got[i].data[w] == (want[i].data[w] & mask), nm, want[i].data[w] & mask,
            b.got[i].data[w]);
    }
  }
  check(top->bad_magic_o == 0 && top->truncated_o == 0 && top->denied_o == 0,
        "a good page is refused by nothing", 1,
        top->bad_magic_o == 0 && top->truncated_o == 0 && top->denied_o == 0);

  // ---- 2. a publication of ANOTHER KIND is ignored entirely ---------------
  {
    const uint32_t reads_before = static_cast<uint32_t>(b.reads);
    b.publish(kOtherKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(60);
    check(top->pages_o == 1, "a MATERIAL_SET publication is not a species table", 1,
          top->pages_o);
    check(static_cast<uint32_t>(b.reads) == reads_before, "and nothing was read for it",
          reads_before, static_cast<uint32_t>(b.reads));
  }

  // ---- 3. an EMPTY page is a legal load ----------------------------------
  {
    b.mem = sp::build({});
    const size_t before = b.got.size();
    b.publish(sp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(80);
    check(top->pages_o == 2, "an empty page IS a load -- 'no descriptors' is a statement", 2,
          top->pages_o);
    check(b.got.size() == before, "and hands over nothing", before, b.got.size());
  }

  // ---- 4. THREE REFUSALS, each loading NOTHING ---------------------------
  const uint32_t entries_before = top->entries_o;
  {
    b.mem = sp::build(want);
    b.mem[0] = 0xFFu;  // wrong magic
    b.publish(sp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(120);
    check(top->bad_magic_o == 1, "a wrong magic refuses the page", 1, top->bad_magic_o);
    check(top->entries_o == entries_before, "and loads NOTHING", entries_before,
          top->entries_o);
  }
  {
    b.mem = sp::build(want);
    b.mem[4] = 0x02u;  // wrong version
    b.publish(sp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(120);
    check(top->bad_magic_o == 2, "a wrong version refuses the page the same way", 2,
          top->bad_magic_o);
    check(top->entries_o == entries_before, "and loads NOTHING", entries_before,
          top->entries_o);
  }
  {
    // The page is honest; the PUBLICATION declares less of it than the count
    // needs, which is the shape a short upload has.
    b.mem = sp::build(want);
    b.publish(sp::kPageKind, kBase, 128u);
    b.run(120);
    check(top->truncated_o == 1, "a count past the declared extent refuses the page", 1,
          top->truncated_o);
    check(top->entries_o == entries_before, "and loads NOTHING", entries_before,
          top->entries_o);
  }
  {
    // Shorter than a header. Refused before a single byte is asked for.
    const uint32_t reads_before = static_cast<uint32_t>(b.reads);
    b.publish(sp::kPageKind, kBase, 32u);
    b.run(60);
    check(top->truncated_o == 2, "a page too short to hold a header is truncated", 2,
          top->truncated_o);
    check(static_cast<uint32_t>(b.reads) == reads_before,
          "and nothing was read for it at all", reads_before,
          static_cast<uint32_t>(b.reads));
  }

  // ---- 5. A DENIED READ ABANDONS THE PAGE --------------------------------
  {
    b.mem = sp::build(want);
    b.deny_after = b.reads;  // deny the very next request: the header's
    b.publish(sp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(120);
    b.deny_after = -1;
    check(top->denied_o == 1, "a denied header read is counted", 1, top->denied_o);
    check(top->entries_o == entries_before, "and the page loads NOTHING", entries_before,
          top->entries_o);
  }
  {
    // ...and mid-page, after entries have already been handed over. The table
    // keeps what it has; the loader does not invent the rest.
    b.mem = sp::build(want);
    b.deny_after = b.reads + 2;   // header, first entry line, then DENY
    const uint32_t before = top->entries_o;
    b.publish(sp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(200);
    b.deny_after = -1;
    check(top->denied_o == 2, "a denial mid-page is counted too", 2, top->denied_o);
    check(top->entries_o == before + 2, "the entries already handed over stand", before + 2,
          top->entries_o);
    check(top->pages_o == 2, "but the PAGE is not counted as loaded", 2, top->pages_o);
  }

  // ---- 6. A PUBLICATION WHILE BUSY IS DROPPED AND COUNTED ----------------
  {
    b.mem = sp::build(want);
    b.stall_load = true;   // hold the table's ready low so the loader stays busy
    b.publish(sp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(80);
    check(top->busy_o == 1, "the loader is busy", 1, top->busy_o);
    b.publish(sp::kPageKind, kBase, static_cast<uint32_t>(b.mem.size()));
    check(top->pages_dropped_o == 1, "a second publication while busy is DROPPED and counted",
          1, top->pages_dropped_o);
    b.stall_load = false;
    b.run(400);
    check(top->busy_o == 0, "and the first page still finishes", 0, top->busy_o);
    check(top->pages_o == 3, "counted as the third completed page", 3, top->pages_o);
  }

  std::printf(
      "[part_table_loader_directed] pages=%u entries=%u dropped=%u bad_magic=%u truncated=%u "
      "denied=%u reads=%d\n",
      top->pages_o, top->entries_o, top->pages_dropped_o, top->bad_magic_o, top->truncated_o,
      top->denied_o, b.reads);
  top->final();
  return zhao::report_and_exit("part_table_loader_directed");
}
