// normalloader_onecycle_mutant_driver.cpp -- INVERTED POLARITY. This PASSES
// when the mutant FAILS to load, and it is evidence about the INSTRUMENT
// rather than about the design.
//
// WHAT IS BEING DEMONSTRATED
// --------------------------
// `tests/terrain/normalloader_directed.cpp` plays the guard the way
// `zhao_mem_guard` actually behaves: `ready` is a LEVEL, `ok` is a PULSE one
// cycle later, and the two are never high together on a passing request. That
// responder is the whole reason `zhao_terrain_normalloader` reads its verdict
// in a separate state.
//
// But "the bench would catch the one-cycle defect" is an ARGUMENT until
// somebody fires it, and no legal stimulus can produce that defect -- it is a
// property of the RTL's shape, not a reachable state. So the demonstration is
// the committed mutant beside this file, whose verdict arms are collapsed back
// into the broken form, driven by THE SAME responder.
//
// EXPECTED: the mutant never leaves S_HREQ. `pages_o` stays 0, `words_o` stays
// 0, `busy_o` stays 1 forever, and -- the part worth staring at -- `denied_o`
// ALSO stays 0. There is no error anywhere. A block with this defect looks
// IDLE, which is why it survived in two geometry fetchers until a tool went
// looking for the shape.
//
// This driver therefore asserts the SILENT SIGNATURE, not just "it failed".
//
// It does NOT assert the bug in the shipped block. The shipped block's correct
// behaviour is asserted by `normalloader_directed`, separately, and this
// positive control is kept apart from it exactly as CLAUDE.md requires.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vtb_terrain_normalloader_onecycle_mutant.h"
#include "zhao_sim.hpp"
#include "zref/zref_normal_page.hpp"

using zhao::check;
namespace np = zref::normal_page;

namespace {
constexpr uint32_t kBase = 0x0100'0000u;
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* d = new Vtb_terrain_normalloader_onecycle_mutant;

  // Build a page the REAL block loads whole -- a small one, so a pass would be
  // quick and unmistakable.
  std::vector<np::Texel> tile(np::kLevel0Words);
  for (size_t i = 0; i < tile.size(); ++i) {
    tile[i].dx = static_cast<int8_t>((i * 5) % 251 - 125);
    tile[i].dz = static_cast<int8_t>((i * 3) % 251 - 125);
  }
  const auto page = np::build_from_tile(tile, 7);

  d->pub_valid_i = 0;
  d->pub_tag_i = 0;
  d->pub_base_i = 0;
  d->pub_extent_i = 0;
  d->rsp_ready_i = 0;
  d->rsp_ok_i = 0;
  d->rsp_violation_i = 0;
  d->beat_valid_i = 0;
  d->beat_data_i = 0;
  d->rst_n = 0;
  d->eval();
  for (int i = 0; i < 3; ++i) zhao::tick(*d);
  d->rst_n = 1;
  d->eval();

  // ---- the REAL guard, modelled exactly as the directed bench models it ----
  bool fwd_active = false, ok_q = false, violation_q = false;
  int beats_left = 0, writes = 0, shape_violations = 0;
  uint32_t beat_addr = 0;
  bool published = false;

  auto word = [&](uint32_t addr) -> uint64_t {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
      const size_t o = static_cast<size_t>(addr - kBase) + static_cast<size_t>(i);
      v |= static_cast<uint64_t>(o < page.size() ? page[o] : 0xEEu) << (8 * i);
    }
    return v;
  };

  for (int cyc = 0; cyc < 20000; ++cyc) {
    if (!published) {
      d->pub_valid_i = 1;
      d->pub_tag_i = np::kPageKind;
      d->pub_base_i = kBase;
      d->pub_extent_i = static_cast<uint32_t>(page.size());
      published = true;
    } else {
      d->pub_valid_i = 0;
    }
    d->rsp_ready_i = fwd_active ? 0 : 1;   // the LEVEL
    d->rsp_ok_i = ok_q ? 1 : 0;            // the PULSE, one cycle later
    d->rsp_violation_i = violation_q ? 1 : 0;
    if (d->rsp_ready_i && d->rsp_ok_i) ++shape_violations;
    d->beat_valid_i = 0;
    d->beat_data_i = 0;
    if (beats_left > 0) {
      d->beat_valid_i = 1;
      d->beat_data_i = word(beat_addr);
      beat_addr += 8;
      --beats_left;
    }
    d->eval();
    if (d->tw_we_o) ++writes;

    bool next_ok = false, next_violation = false;
    if (ok_q) { fwd_active = false; beats_left = 8; }
    if (violation_q) fwd_active = false;
    if (d->req_valid_o && !fwd_active && beats_left == 0) {
      next_ok = true;
      beat_addr = d->req_addr_o;
      fwd_active = true;
    }
    zhao::tick(*d);
    ok_q = next_ok;
    violation_q = next_violation;
    d->pub_valid_i = 0;
    d->eval();
  }

  check(shape_violations == 0,
        "the responder is the REAL guard -- ready and ok never high together",
        0, shape_violations);

  // ---- THE INVERTED ASSERTIONS -------------------------------------------
  check(d->pages_o == 0, "MUTANT: no page ever loads", 0, d->pages_o);
  check(d->words_o == 0, "MUTANT: not one pyramid word is written", 0, d->words_o);
  check(writes == 0, "MUTANT: the upload port never strobes", 0, writes);
  check(d->busy_o == 1, "MUTANT: it is stuck busy, forever", 1, d->busy_o);

  // THE SIGNATURE THAT MAKES THIS DEFECT DANGEROUS: nothing is reported.
  check(d->denied_o == 0, "MUTANT: and denied_o reads ZERO -- the failure is SILENT",
        0, d->denied_o);
  check(d->bad_magic_o == 0, "MUTANT: no magic error either", 0, d->bad_magic_o);
  check(d->truncated_o == 0, "MUTANT: no truncation error either", 0, d->truncated_o);

  std::printf(
      "[normalloader_onecycle_mutant] FIRED AS INTENDED: pages=%u words=%u writes=%d "
      "busy=%u denied=%u (silent) shape_violations=%d\n",
      d->pages_o, d->words_o, writes, d->busy_o, d->denied_o, shape_violations);
  d->final();
  return zhao::report_and_exit("normalloader_onecycle_mutant");
}
