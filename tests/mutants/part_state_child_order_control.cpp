// part_state_child_order_control.cpp -- INVERTED POLARITY. IT PASSES WHEN THE
// ORDERING IS BROKEN.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS EVIDENCE ABOUT
// ---------------------------------------------------------------------------
// `zhao_part_state.sv` states its ordering law -- survivors compacted first in
// stream order, children strictly after -- and then says out loud that it
// carries NO assertion for it, because the property is structural and the first
// draft's assertion ended in `&& 1'b0`.
//
// So the entire defence of that law is `part_state_directed.cpp` comparing the
// emitted stream, position by position, against an expected stream. That suite
// passed on its first run. Under this tree's rules that is a claim: a check
// nobody has watched fail is not a check.
//
// This driver runs `tests/mutants/zhao_part_state_child_order_mutant.sv`, a
// committed copy whose only substantive difference is that children may enter
// the write stream DURING the survivor pass. It asserts, positively:
//
//   1. every count still balances -- 7 records, 4 survivors, 3 children. So a
//      test built out of counters alone would pass on a block with the
//      ordering destroyed, which is the ledger's "two operands that move
//      together" shape one level out;
//   2. and the ORDER is nevertheless wrong -- a child is emitted before the
//      last survivor.
//
// (2) is the positive control. The production suite's per-position comparison
// is the only check in the tree that can see it, and this is the demonstration
// that it does.
//
// It is NOT a test of the design and it must never be read as one. It is a test
// of the instrument, in the shape `tests/texture/frag_expand_overflow_control.cpp`
// established.

#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include "Vzhao_part_state_child_order_mutant.h"
#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"

using Dut = Vzhao_part_state_child_order_mutant;
using Rec = std::pair<uint64_t, uint64_t>;

namespace {

constexpr int kSpecies = 4;
constexpr int kIn = 6;
constexpr int kChildren = 3;
constexpr int kChildBase = 100;

// Byte-for-byte the stimulus builder in part_state_directed.cpp, through the
// ratified codec, so the two runs are comparing the same records.
Rec make_rec(int i, int species) {
  zref::part::Particle128 p{};
  p.pos[0] = 1000 + i * 7;
  p.pos[1] = -(500 + i * 3);
  p.pos[2] = 250 - i * 11;
  p.vel[0] = i * 3 - 10;
  p.vel[1] = 7 - i;
  p.vel[2] = i * 2;
  p.age = static_cast<uint16_t>((100 + i * 13) & 0x3FF);
  p.species = static_cast<uint8_t>(species & 0x7F);
  p.size = static_cast<uint8_t>((i * 5) & 0x3F);
  p.spin = static_cast<uint8_t>((i * 9) & 0x3F);
  p.flags = static_cast<uint8_t>(i & 0x7);
  p.variation = static_cast<uint8_t>(0xA5 ^ i);
  uint64_t lo = 0, hi = 0;
  zref::part::particle_pack(p, &lo, &hi);
  return Rec(lo, hi);
}

std::vector<Rec> g_written;

template <typename W>
void set_rec(W& dst, const Rec& r) {
  dst[0] = static_cast<uint32_t>(r.first);
  dst[1] = static_cast<uint32_t>(r.first >> 32);
  dst[2] = static_cast<uint32_t>(r.second);
  dst[3] = static_cast<uint32_t>(r.second >> 32);
}

void collect(Dut* v) {
  if (v->wr_valid_o && v->wr_ready_i) {
    const uint64_t lo =
        static_cast<uint64_t>(v->wr_record_o[0]) | (static_cast<uint64_t>(v->wr_record_o[1]) << 32);
    const uint64_t hi =
        static_cast<uint64_t>(v->wr_record_o[2]) | (static_cast<uint64_t>(v->wr_record_o[3]) << 32);
    g_written.emplace_back(lo, hi);
  }
}

void tick(Dut* v) {
  v->clk = 0;
  v->eval();
  collect(v);
  v->clk = 1;
  v->eval();
}

void idle(Dut* v) {
  v->tick_start_i = 0;
  v->rd_valid_i = 0;
  v->rd_last_i = 0;
  v->prt_ready_i = 1;
  v->vrd_valid_i = 0;
  v->chl_valid_i = 0;
  v->wr_ready_i = 1;
  v->eval();
}

bool survives(int i) { return i != 1 && i != 4; }

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut* v = new Dut;

  idle(v);
  v->rst_n = 0;
  tick(v);
  tick(v);
  v->rst_n = 1;
  tick(v);
  idle(v);

  v->tick_start_i = 1;
  tick(v);
  v->tick_start_i = 0;

  int fed = 0, verdicts = 0, children_sent = 0;
  for (int guard = 0; guard < 4000 && verdicts < kIn; ++guard) {
    idle(v);

    if (fed < kIn && v->rd_ready_o) {
      v->rd_valid_i = 1;
      v->rd_last_i = (fed == kIn - 1);
      set_rec(v->rd_record_i, make_rec(fed, fed % kSpecies));
    }
    if (v->prt_valid_o && verdicts < kIn) {
      v->vrd_valid_i = 1;
      v->vrd_survive_i = survives(verdicts) ? 1 : 0;
      set_rec(v->vrd_record_i, make_rec(verdicts, verdicts % kSpecies));
    }
    if (children_sent < kChildren && v->chl_ready_o) {
      v->chl_valid_i = 1;
      set_rec(v->chl_record_i, make_rec(kChildBase + children_sent, 0));
    }

    v->eval();
    const bool rd_fire = v->rd_valid_i && v->rd_ready_o;
    const bool vr_fire = v->vrd_valid_i && v->vrd_ready_o;
    const bool ch_fire = v->chl_valid_i && v->chl_ready_o;
    tick(v);
    if (rd_fire) ++fed;
    if (vr_fire) ++verdicts;
    if (ch_fire) ++children_sent;
  }
  for (int guard = 0; guard < 400 && !v->tick_done_o; ++guard) {
    idle(v);
    tick(v);
  }

  // ---- classify the emitted stream ---------------------------------------
  std::vector<Rec> child_recs;
  for (int c = 0; c < kChildren; ++c) child_recs.push_back(make_rec(kChildBase + c, 0));

  int last_survivor_pos = -1;
  int first_child_pos = -1;
  for (size_t i = 0; i < g_written.size(); ++i) {
    bool is_child = false;
    for (size_t c = 0; c < child_recs.size(); ++c)
      if (g_written[i] == child_recs[c]) is_child = true;
    if (is_child) {
      if (first_child_pos < 0) first_child_pos = static_cast<int>(i);
    } else {
      last_survivor_pos = static_cast<int>(i);
    }
  }

  std::printf(
      "  mutant stream: %zu records, survivors_o=%u children_written_o=%u; "
      "first child at %d, last survivor at %d\n",
      g_written.size(), v->survivors_o, v->children_written_o, first_child_pos, last_survivor_pos);

  // (1) The accounting is untouched. This is the half that makes the mutant
  //     dangerous rather than obvious.
  zhao::check(g_written.size() == static_cast<size_t>(4 + kChildren),
              "the mutant wrote the SAME NUMBER of records as a correct block -- "
              "a count check cannot see this fault",
              4 + kChildren, g_written.size());
  zhao::check(v->survivors_o == 4,
              "and survivors_o still reads 4 -- every counter in the block balances "
              "while the stream is wrong",
              4, v->survivors_o);
  zhao::check(v->children_written_o == static_cast<uint32_t>(kChildren),
              "and children_written_o still reads 3", kChildren, v->children_written_o);

  // (2) The positive control: the ORDER is broken, and only a per-position
  //     comparison of the stream can say so.
  zhao::check(
      first_child_pos >= 0 && last_survivor_pos >= 0 && first_child_pos < last_survivor_pos,
      "a CHILD was emitted BEFORE the last SURVIVOR. The ordering law of "
      "PART.STATE -- survivors compacted first, children strictly after -- "
      "is violated by this copy, and part_state_directed.cpp's "
      "position-by-position stream comparison is the only check in the tree "
      "that sees it. That comparison is therefore a real check",
      1,
      (first_child_pos >= 0 && last_survivor_pos >= 0 && first_child_pos < last_survivor_pos) ? 1
                                                                                              : 0);

  // (3) And the production check itself, run against the mutant and required to
  //     go RED. This is the part that is not an inference: part_state_directed's
  //     expected stream is survivors-in-order then children-in-order, and it is
  //     rebuilt here identically. If it matched the mutant's output, the check
  //     would be incapable of catching a broken ordering.
  std::vector<Rec> want;
  for (int i = 0; i < kIn; ++i)
    if (survives(i)) want.push_back(make_rec(i, i % kSpecies));
  for (int c = 0; c < kChildren; ++c) want.push_back(make_rec(kChildBase + c, 0));

  int first_mismatch = -1;
  const size_t n = g_written.size() < want.size() ? g_written.size() : want.size();
  for (size_t i = 0; i < n && first_mismatch < 0; ++i)
    if (g_written[i] != want[i]) first_mismatch = static_cast<int>(i);

  std::printf("  production expectation first differs at record %d of %zu\n", first_mismatch,
              want.size());

  zhao::check(first_mismatch >= 0,
              "part_state_directed.cpp's OWN expected stream -- survivors in order, then "
              "children -- disagrees with what the mutant emitted. The production check is "
              "run here against a block whose ordering is broken and it goes RED, which is "
              "the demonstration that its green on the real block means something",
              1, first_mismatch >= 0 ? 1 : 0);

  const int rc = zhao::report_and_exit("part_state_child_order_control");
  zhao::exit_hard(rc);
}
