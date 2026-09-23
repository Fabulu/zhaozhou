// normalloader_directed.cpp -- TERRAIN.NORMALMAP's host against
// `zref::normal_page`, the frozen DETAIL_NORMAL layout (owner ruling R243
// D-NORMALS-A, spec/cartridge.md 4g).
//
// The bench is BOTH ends of the block: the publication MEM.UPLOAD makes, and
// the asset window it reads through. The page under test is built by the
// REFERENCE (`zref::normal_page::build_from_tile`), so the RTL is read against
// a page nothing in the RTL wrote -- the only arrangement in which a layout
// disagreement can show up at all.
//
// ---------------------------------------------------------------------------
// THE GUARD RESPONDER IS THE REAL ONE, AND THAT IS THE FIRST THING TO SAY
// ---------------------------------------------------------------------------
// `tools/rtl/check_guard_verdict.py`'s header records why this matters more
// than anything else in the file:
//
//     "every bench PLAYED the guard and every played responder raised ready
//      and ok together -- so the RTL, the harnesses and the measurements all
//      agreed with each other about a machine that does not exist."
//
// `tests/particles/part_table_loader_directed.cpp` -- the bench this one is
// modelled on -- STILL does that: its responder sets `rsp_ready_i` and
// `rsp_ok_i` in the same cycle (its cycle(), the `d.rsp_ok_i = 1` beside
// `d.rsp_ready_i = 1`). A loader written to the correct two-state shape would
// HANG against that responder, and a loader written to the broken one-cycle
// shape passes against it and reads every pass as a denial in silicon.
//
// So this responder models `zhao_mem_guard.sv:604-680` instead of modelling
// the convenient fiction:
//
//     rsp.ready     = !fwd_active          -- a LEVEL, high whenever idle
//     rsp.ok        = rsp_ok_q             -- a PULSE, the cycle AFTER accept
//     rsp.violation = rsp_violation_q      -- likewise
//
// `check_guard_shape` below then ASSERTS that the two are never high together,
// on every cycle of every test. That is the positive control for the responder
// itself: a bench whose model of the guard drifts back toward the fiction
// fails loudly rather than quietly re-blessing the defect.
//
// ---------------------------------------------------------------------------
// WHAT ACTUALLY DISCRIMINATES, named up front
// ---------------------------------------------------------------------------
//   1. THE WHOLE PYRAMID, IN ORDER, BIT FOR BIT, BY ADDRESS. Every one of the
//      5,461 words is compared against the reference's decode of the same
//      bytes AND against the address the loader wrote it to. A loader that
//      dropped a line, read one twice, or slipped by one word inside a line
//      fails here. Writing the right words to the wrong addresses is the
//      failure a "final contents" check cannot see, so the bench records
//      (addr, data) pairs rather than a filled array.
//   2. THE EXACT READ COUNT. 1 header line + 171 word lines = 172. A loader
//      that re-read a line would write the same words to the same addresses
//      and every content check would still pass.
//   3. A REFUSED PAGE WRITES NOTHING. Wrong magic, wrong version, a word count
//      above the layout ceiling, and a count that runs past the declared
//      extent each refuse the page WHOLE -- the bench requires ZERO writes,
//      not "fewer".
//   4. A DENIED READ ABANDONS THE PAGE, mid-pyramid. MEM.GUARD drops a denied
//      request, so continuing would fill the pyramid out of whatever the beat
//      bus held.
//   5. A PUBLICATION WHILE BUSY IS DROPPED AND COUNTED, and the load in
//      progress is UNDISTURBED -- the bench checks the words that had already
//      landed still stand.
//   6. A PUBLICATION OF ANOTHER KIND IS IGNORED ENTIRELY. The console
//      publishes SPECIES_TABLE (13) and FORGE_PROGRAM (14) on the same wire.
//   7. A LEGAL EMPTY PAGE IS A LOAD, not a silence: pages_o advances and
//      nothing is written.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vtb_terrain_normalloader.h"
#include "zhao_sim.hpp"
#include "zref/zref_normal_page.hpp"

using zhao::check;
namespace np = zref::normal_page;

namespace {

constexpr uint32_t kBase = 0x0100'0000u;
constexpr uint8_t kKind = np::kPageKind;  // 16
constexpr uint8_t kOtherKind = 13;        // SPECIES_TABLE, on the same wire

struct Write {
  uint16_t addr;
  uint16_t data;
};

struct Bench {
  Vtb_terrain_normalloader& d;
  std::vector<uint8_t> mem;   // the asset window, at kBase
  std::vector<Write> got;     // every (addr, data) the loader emitted
  int reads = 0;              // requests ACCEPTED (not offered)
  int deny_after = -1;        // deny the Nth request (0-based); -1 = never
  int shape_violations = 0;   // ready and ok high together -- must stay 0

  // ---- the guard's own registers, modelled (zhao_mem_guard.sv:604-680) ----
  bool fwd_active = false;    // set on accept, cleared when the burst starts
  bool ok_q = false;          // the PULSE, one cycle after the accept
  bool violation_q = false;
  int beats_left = 0;
  uint32_t beat_addr = 0;

  explicit Bench(Vtb_terrain_normalloader& dut) : d(dut) {}

  uint64_t word(uint32_t addr) const {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
      const size_t o = static_cast<size_t>(addr - kBase) + static_cast<size_t>(i);
      const uint8_t b = o < mem.size() ? mem[o] : 0xEEu;
      v |= static_cast<uint64_t>(b) << (8 * i);
    }
    return v;
  }

  // A publication is presented on the NEXT ordinary cycle rather than by
  // ticking the clock in publish(). That matters and it was found the hard
  // way: a publish() that ticked on its own drove the guard's inputs to idle
  // for that cycle AND skipped the upload capture, so a publication landing
  // mid-burst silently lost one word and every later word compared against
  // the wrong index -- 5,241 spurious mismatches from one unmodelled cycle.
  // The BENCH was wrong, not the block. A bench that stops modelling the
  // world for one cycle is a bench that reports the block doing whatever
  // happened in it.
  bool pub_pending = false;
  uint8_t pub_kind = 0;
  uint32_t pub_base = 0, pub_extent = 0;

  void idle_inputs() {
    d.pub_valid_i = 0;
    d.pub_tag_i = 0;
    d.pub_base_i = 0;
    d.pub_extent_i = 0;
  }

  void cycle() {
    if (pub_pending) {
      d.pub_valid_i = 1;
      d.pub_tag_i = pub_kind;
      d.pub_base_i = pub_base;
      d.pub_extent_i = pub_extent;
      pub_pending = false;
    }
    // ---- present the guard's REGISTERED verdict from the previous edge ----
    d.rsp_ready_i = fwd_active ? 0 : 1;   // the LEVEL
    d.rsp_ok_i = ok_q ? 1 : 0;            // the PULSE
    d.rsp_violation_i = violation_q ? 1 : 0;
    d.beat_valid_i = 0;
    d.beat_data_i = 0;

    // THE INVARIANT THIS WHOLE FILE EXISTS TO KEEP. If these are ever high
    // together the responder has drifted back into the fiction, and every
    // result after it is worthless.
    if (d.rsp_ready_i && d.rsp_ok_i) ++shape_violations;

    // ---- beats, once a read has been accepted and granted ------------------
    if (beats_left > 0) {
      d.beat_valid_i = 1;
      d.beat_data_i = word(beat_addr);
      beat_addr += 8;
      --beats_left;
    }

    d.eval();

    // ---- capture the upload write this cycle, BEFORE the edge --------------
    const bool we = (d.tw_we_o != 0);
    const Write w{static_cast<uint16_t>(d.tw_addr_o), static_cast<uint16_t>(d.tw_data_o)};

    // ---- the guard's next state, computed from what it sees now ------------
    const bool req = (d.req_valid_o != 0);
    bool next_ok = false, next_violation = false;
    if (ok_q) {
      // The accept's pulse cycle is over; the burst begins.
      fwd_active = false;
      beats_left = 8;
    }
    if (violation_q) fwd_active = false;
    if (req && !fwd_active && beats_left == 0) {
      const bool deny = (deny_after >= 0 && reads == deny_after);
      if (deny) {
        next_violation = true;
      } else {
        // THE SHAPE RULE, CHECKED RATHER THAN ASSUMED. MEM.GUARD refuses a
        // request whose byte mask does not match its length, and a loader that
        // asked for 64 bytes with a partial mask would be denied in silicon
        // while passing every content check here.
        check(d.req_len_o == 64, "the read asks for a whole 64-byte line", 64, d.req_len_o);
        check(d.req_be_o == 0xFFFFFFFFFFFFFFFFull,
              "with the mask its length requires", 1,
              d.req_be_o == 0xFFFFFFFFFFFFFFFFull);
        check(d.req_write_o == 0, "and it is a READ", 0, d.req_write_o);
        next_ok = true;
        beat_addr = d.req_addr_o;
      }
      fwd_active = true;
      ++reads;
    }

    zhao::tick(d);
    if (we) got.push_back(w);
    ok_q = next_ok;
    violation_q = next_violation;
    idle_inputs();
    d.eval();
  }

  void publish(uint8_t kind, uint32_t base, uint32_t extent) {
    pub_kind = kind;
    pub_base = base;
    pub_extent = extent;
    pub_pending = true;
    cycle();   // an ordinary cycle: guard modelled, upload captured
  }

  void run(int cycles) {
    for (int i = 0; i < cycles; ++i) cycle();
  }

  void reset_capture() {
    got.clear();
    reads = 0;
  }
};

/** The golden tile, in the reference's own terms. Deliberately the SAME tile
 *  `tools/pack/mkdetailnormal.py --check` builds, so this test and the packer
 *  are looking at one artefact rather than at two conventions. */
std::vector<np::Texel> golden_tile() {
  std::vector<np::Texel> t;
  for (int v = 0; v < np::kTileDim; ++v)
    for (int u = 0; u < np::kTileDim; ++u) {
      np::Texel x;
      x.dx = static_cast<int8_t>(((u * 7 + v * 3) % 255) - 127);
      x.dz = static_cast<int8_t>(100 - ((u * 5 + v * 11) % 229));
      t.push_back(x);
    }
  t[0].dx = static_cast<int8_t>(127);
  t[0].dz = static_cast<int8_t>(-128);
  return t;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vtb_terrain_normalloader;  // heap + exit_hard: see zhao_sim.hpp
  Bench b(*top);
  b.idle_inputs();
  top->rsp_ready_i = 0;
  top->rsp_ok_i = 0;
  top->rsp_violation_i = 0;
  top->beat_valid_i = 0;
  top->beat_data_i = 0;
  // `zhao::reset` pokes `in_valid`/`in_data`, which this block does not have,
  // so the reset is written out -- the same way the species-page loader's
  // bench does it, for the same reason.
  top->rst_n = 0;
  top->eval();
  for (int i = 0; i < 3; ++i) zhao::tick(*top);
  top->rst_n = 1;
  top->eval();

  const auto tile = golden_tile();
  const auto page = np::build_from_tile(tile, 7);
  std::vector<uint16_t> want;
  const auto verdict = np::decode(page.data(), page.size(), want);
  check(verdict == np::Verdict::kOk, "the reference page decodes", 0,
        static_cast<int>(verdict));
  check(want.size() == 5461, "and carries the full seven-level pyramid", 5461,
        static_cast<int>(want.size()));

  // =========================================================================
  // 1. A WHOLE PAGE, WORD FOR WORD, ADDRESS FOR ADDRESS
  // =========================================================================
  b.mem = page;
  b.publish(kKind, kBase, static_cast<uint32_t>(page.size()));
  b.run(9000);

  check(top->busy_o == 0, "the loader finished", 0, top->busy_o);
  check(top->pages_o == 1, "one page loaded whole", 1, top->pages_o);
  check(top->words_o == 5461, "every pyramid word written", 5461, top->words_o);
  check(top->denied_o == 0, "nothing denied", 0, top->denied_o);
  check(top->bad_magic_o == 0, "the magic was accepted", 0, top->bad_magic_o);
  check(top->truncated_o == 0, "and it fitted", 0, top->truncated_o);
  check(top->oversize_o == 0, "and was within the layout", 0, top->oversize_o);
  check(b.shape_violations == 0,
        "the responder never raised ready and ok together", 0, b.shape_violations);

  // 1 header line + ceil(5461/32) = 171 word lines.
  check(b.reads == 1 + 171, "the exact read count -- no line read twice",
        1 + 171, b.reads);

  check(b.got.size() == want.size(), "one write per word", static_cast<int>(want.size()),
        static_cast<int>(b.got.size()));
  int mismatches = 0, addr_wrong = 0;
  for (size_t i = 0; i < b.got.size() && i < want.size(); ++i) {
    if (b.got[i].addr != static_cast<uint16_t>(i)) ++addr_wrong;
    if (b.got[i].data != want[i]) ++mismatches;
  }
  check(addr_wrong == 0, "every word went to its own flat pyramid address", 0, addr_wrong);
  check(mismatches == 0, "and carried the reference's bytes", 0, mismatches);

  // The pyramid's own law, read back through the ADDRESSER rather than by
  // index: the 1x1 apex must be the signed mean of the tile, not a unit
  // direction. This is the "never normalise" property, checked on the wire.
  {
    const int apex = zref::terrain::normalmap_pyramid_addr(6, 0, 0);
    const np::Texel t = np::unpack_word(b.got[static_cast<size_t>(apex)].data);
    check(t.dx == 3 && t.dz == -14,
          "the 1x1 apex is the signed mean of the tile (3, -14), not a normalised direction",
          1, (t.dx == 3 && t.dz == -14));
  }

  // =========================================================================
  // 2. A PUBLICATION OF ANOTHER KIND IS NOT THIS BLOCK'S
  // =========================================================================
  b.reset_capture();
  const uint32_t pages_before = top->pages_o;
  b.publish(kOtherKind, kBase, static_cast<uint32_t>(page.size()));
  b.run(200);
  check(b.got.empty(), "a SPECIES_TABLE publication writes nothing here", 0,
        static_cast<int>(b.got.size()));
  check(top->pages_o == pages_before, "and is not counted as a page", pages_before,
        top->pages_o);
  check(b.reads == 0, "and asks for no line at all", 0, b.reads);

  // =========================================================================
  // 3. A REFUSED PAGE WRITES NOTHING -- three ways, each refusing WHOLE
  // =========================================================================
  // (a) wrong magic
  {
    b.reset_capture();
    b.mem = page;
    b.mem[0] ^= 0xFFu;
    const uint32_t before = top->bad_magic_o;
    b.publish(kKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(600);
    check(b.got.empty(), "a wrong magic writes NOTHING -- not fewer words", 0,
          static_cast<int>(b.got.size()));
    check(top->bad_magic_o == before + 1, "and is counted", before + 1, top->bad_magic_o);
    check(top->busy_o == 0, "and the loader is idle again", 0, top->busy_o);
  }

  // (b) wrong version
  {
    b.reset_capture();
    b.mem = page;
    b.mem[4] = 0x02u;
    const uint32_t before = top->bad_magic_o;
    b.publish(kKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(600);
    check(b.got.empty(), "a wrong version writes NOTHING", 0, static_cast<int>(b.got.size()));
    check(top->bad_magic_o == before + 1, "and is counted with the magic", before + 1,
          top->bad_magic_o);
  }

  // (c) `words` above the LAYOUT ceiling -- a page that cannot be this kind
  {
    b.reset_capture();
    b.mem = page;
    b.mem[6] = 0xFFu;
    b.mem[7] = 0x7Fu;   // 32,767 words
    const uint32_t before = top->oversize_o;
    b.publish(kKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(600);
    check(b.got.empty(), "a word count above the layout ceiling writes NOTHING", 0,
          static_cast<int>(b.got.size()));
    check(top->oversize_o == before + 1, "and is counted as oversize", before + 1,
          top->oversize_o);
  }

  // (d) `words` that runs past the DECLARED EXTENT
  {
    b.reset_capture();
    b.mem = page;
    const uint32_t before = top->truncated_o;
    b.publish(kKind, kBase, 1024u);   // far less than 64 + 2*5461
    b.run(600);
    check(b.got.empty(), "a page that runs past its extent writes NOTHING", 0,
          static_cast<int>(b.got.size()));
    check(top->truncated_o == before + 1, "and is counted as truncated", before + 1,
          top->truncated_o);
  }

  // (e) an extent too short to hold even a header -- refused before any read
  {
    b.reset_capture();
    const uint32_t before = top->truncated_o;
    b.publish(kKind, kBase, 32u);
    b.run(200);
    check(b.reads == 0, "an extent below one line asks for nothing at all", 0, b.reads);
    check(top->truncated_o == before + 1, "and is counted", before + 1, top->truncated_o);
  }

  // =========================================================================
  // 4. A LEGAL EMPTY PAGE IS A LOAD, NOT A SILENCE
  // =========================================================================
  {
    b.reset_capture();
    b.mem = np::build(std::vector<np::Texel>{});
    const uint32_t before = top->pages_o;
    b.publish(kKind, kBase, static_cast<uint32_t>(b.mem.size()));
    b.run(400);
    check(b.got.empty(), "an empty page writes nothing", 0, static_cast<int>(b.got.size()));
    check(top->pages_o == before + 1, "but IS counted as a page loaded", before + 1,
          top->pages_o);
    check(b.reads == 1, "having read only its header", 1, b.reads);
  }

  // =========================================================================
  // 5. A DENIED READ ABANDONS THE PAGE, MID-PYRAMID
  // =========================================================================
  {
    b.reset_capture();
    b.mem = page;
    b.deny_after = 3;   // header, two word lines, then the denial
    const uint32_t before = top->denied_o;
    const uint32_t pages_at = top->pages_o;
    b.publish(kKind, kBase, static_cast<uint32_t>(page.size()));
    b.run(3000);
    b.deny_after = -1;
    check(top->denied_o == before + 1, "the denial is counted", before + 1, top->denied_o);
    check(top->pages_o == pages_at, "the page is NOT counted as loaded", pages_at,
          top->pages_o);
    check(top->busy_o == 0, "and the loader is idle, not stuck", 0, top->busy_o);
    // The words already written are the first two lines' -- 64 of them, right.
    check(b.got.size() == 64, "exactly the two lines that were read landed", 64,
          static_cast<int>(b.got.size()));
    int bad = 0;
    for (size_t i = 0; i < b.got.size(); ++i)
      if (b.got[i].addr != static_cast<uint16_t>(i) || b.got[i].data != want[i]) ++bad;
    check(bad == 0, "and they are correct -- nothing was filled from a dropped read", 0, bad);
  }

  // =========================================================================
  // 6. A PUBLICATION WHILE BUSY IS DROPPED, AND THE LOAD IS UNDISTURBED
  // =========================================================================
  {
    b.reset_capture();
    b.mem = page;
    const uint32_t dropped_before = top->pages_dropped_o;
    b.publish(kKind, kBase, static_cast<uint32_t>(page.size()));
    b.run(300);                       // mid-pyramid
    check(top->busy_o == 1, "the loader is mid-page", 1, top->busy_o);
    const size_t landed = b.got.size();
    check(landed > 0, "and has already written words", 1, landed > 0);
    b.publish(kKind, kBase, static_cast<uint32_t>(page.size()));
    b.run(9000);
    check(top->pages_dropped_o == dropped_before + 1, "the second publication is DROPPED",
          dropped_before + 1, top->pages_dropped_o);
    // The first load ran to completion, once, undisturbed.
    check(b.got.size() == 5461, "and the load in progress completed, once", 5461,
          static_cast<int>(b.got.size()));
    int bad = 0;
    for (size_t i = 0; i < b.got.size() && i < want.size(); ++i)
      if (b.got[i].addr != static_cast<uint16_t>(i) || b.got[i].data != want[i]) ++bad;
    check(bad == 0, "every word still correct after the dropped publication", 0, bad);
  }

  check(b.shape_violations == 0,
        "the responder never once raised ready and ok together, across every test",
        0, b.shape_violations);

  std::printf(
      "[normalloader_directed] pages=%u words=%u dropped=%u bad_magic=%u oversize=%u "
      "truncated=%u denied=%u shape_violations=%d\n",
      top->pages_o, top->words_o, top->pages_dropped_o, top->bad_magic_o, top->oversize_o,
      top->truncated_o, top->denied_o, b.shape_violations);
  top->final();
  return zhao::report_and_exit("normalloader_directed");
}
