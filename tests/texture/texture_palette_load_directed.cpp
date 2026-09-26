// texture_palette_load_directed.cpp -- TEXTURE.PALETTELOAD, the palette
// identity's producer.
//
// ---------------------------------------------------------------------------
// WHAT IS AT RISK HERE
// ---------------------------------------------------------------------------
// This block is a four-entry CACHE with a PROGRAMMING PROTOCOL on its far side
// and a MEMORY CLIENT on its near one, and every one of its faults is quiet:
//
//   * A WRONG GENERATION. The value it offers must never be the one the
//     resolver already holds for that slot, or the load is refused with
//     `err_same_gen_o` and the palette silently keeps its old bytes under a new
//     name. The counter that would say so lives in ANOTHER block, so this test
//     asserts the SEQUENCE this block produces rather than the other block's
//     silence.
//   * A LOAD THAT DID NOT COMPLETE, PUBLISHED ANYWAY. The END carries
//     `ld_crc_ok_o`, and a denied fetch must take it LOW so the resolver
//     refuses residency -- if this block decided on the resolver's behalf, a
//     truncated palette would be advertised as resident and every fragment
//     would read whatever the RAM happened to hold.
//   * A STALE TAG. The block answers `owned` from its own table. A tag that
//     said "resident" for a base whose load failed is the exact
//     two-records-moved-by-two-enables shape CLAUDE.md names, and the fault is
//     invisible downstream because the pair looks perfectly well formed.
//   * A REFUSAL THAT BECAME A CLAMP. A null, misaligned or out-of-VRAM base
//     must produce an ANSWER with `owned` LOW and a counter, never a corrected
//     address -- "a clamped address writes real bytes into a real slot
//     belonging to something else, and nothing downstream can tell".
//
// ---------------------------------------------------------------------------
// EVERY COUNTER THIS BLOCK OWNS IS FIRED HERE, BY LEGAL STIMULUS
// ---------------------------------------------------------------------------
// `lookups_o`, `hits_o`, `loads_o`, `evictions_o`, `entries_written_o`,
// `denied_o`, `base_refused_o` and `gen_zero_loads_o`. None of them owes a
// committed mutant: every one is reachable from this block's own ports, which
// is the `t_ack_i` shape and not the `wq_overflow_o` shape.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_palette_load.h"

#include "zhao_sim.hpp"

namespace {

constexpr int kEntries = 256;
constexpr uint32_t kVramBytes = 1u << 27;

int fails = 0;
int checks = 0;

void check(bool ok, const char* what, long want = 0, long got = 0) {
  ++checks;
  if (!ok) {
    ++fails;
    std::printf("FAIL: %s (expected %ld, got %ld)\n", what, want, got);
  }
}

// What the BEGIN/WRITE/END port was driven with, recorded in order. The block's
// contract is a sequence, so the model is a sequence.
struct Op {
  int op = 0;
  int slot = 0;
  int gen = 0;
  int idx = 0;
  uint16_t rgb = 0;
  bool crc_ok = false;
};

// The memory this block reads. Word `w` of the palette at `base` is
// `word_of(base, w)`, four RGB565 entries packed low-first -- the same shape
// MEM.UPLOAD lands a TEXTURE_PAGE in.
uint64_t word_of(uint32_t base, uint32_t word_index) {
  uint64_t w = 0;
  for (int lane = 0; lane < 4; ++lane) {
    const uint32_t entry = word_index * 4 + static_cast<uint32_t>(lane);
    const uint16_t value =
        static_cast<uint16_t>((base >> 4) + entry * 3u + 0x1234u);
    w |= static_cast<uint64_t>(value) << (16 * lane);
  }
  return w;
}

class Bench {
 public:
  Bench() {
    top_.clk = 0;
    top_.rst_n = 0;
    top_.q_valid_i = 0;
    top_.q_base_i = 0;
    top_.r_ready_i = 1;
    top_.mem_req_ready_i = 1;
    top_.mem_rsp_valid_i = 0;
    top_.mem_rsp_data_i = 0;
    top_.mem_rsp_denied_i = 0;
    top_.ld_ready_i = 1;
    top_.eval();
    for (int i = 0; i < 4; ++i) tick();
    top_.rst_n = 1;
    tick();
  }

  Vzhao_texture_palette_load& top() { return top_; }
  const std::vector<Op>& ops() const { return ops_; }
  void clear_ops() { ops_.clear(); }

  // `deny_after` counts ACCEPTED memory requests; the one at that index is
  // answered with a violation instead of a beat. -1 never denies.
  void set_deny_after(int n) { deny_after_ = n; }

  void tick() {
    top_.eval();

    // ---- the programming port's recorder, on the settled pre-edge values ---
    if (top_.ld_valid_o && top_.ld_ready_i) {
      Op o;
      o.op = top_.ld_op_o;
      o.slot = top_.ld_slot_o;
      o.gen = top_.ld_gen_o;
      o.idx = top_.ld_idx_o;
      o.rgb = top_.ld_rgb565_o;
      o.crc_ok = top_.ld_crc_ok_o != 0;
      ops_.push_back(o);
    }

    // ---- the ENGINE1 model: accept a request, answer ONE beat next clock ---
    // The guard's verdict arrives the cycle AFTER it accepted the request,
    // which is the contract MATERIAL.RESOLVE's shim follows, so a model that
    // answered in the offer cycle would be testing a protocol nobody speaks.
    const bool req_fire = top_.mem_req_valid_o && top_.mem_req_ready_i;
    const uint32_t req_addr = top_.mem_req_addr_o;

    top_.mem_rsp_valid_i = 0;
    top_.mem_rsp_denied_i = 0;
    if (pending_) {
      if (deny_this_) {
        top_.mem_rsp_denied_i = 1;
      } else {
        top_.mem_rsp_valid_i = 1;
        top_.mem_rsp_data_i = pending_data_;
      }
      pending_ = false;
    }
    if (req_fire) {
      if (req_addr < base_) {
        std::printf("FAIL: memory request at %08x is below the palette base %08x\n",
                    req_addr, base_);
        ++fails;
      }
      const uint32_t off = req_addr - base_;
      if ((off & 7u) != 0) {
        std::printf("FAIL: memory request at %08x is not eight-byte aligned\n", req_addr);
        ++fails;
      }
      deny_this_ = (deny_after_ >= 0) && (accepted_ == deny_after_);
      pending_data_ = word_of(base_, off >> 3);
      pending_ = true;
      ++accepted_;
      last_addr_ = req_addr;
    }

    top_.eval();
    top_.clk = 1; top_.eval();
    top_.clk = 0; top_.eval();
  }

  // One lookup, run to its answer. Returns {owned, slot, generation}.
  struct Answer { bool owned; int slot; int gen; };
  Answer ask(uint32_t base, int budget = 40000) {
    base_ = base;
    accepted_ = 0;
    top_.q_base_i = base;
    top_.q_valid_i = 1;
    int spent = 0;
    while (spent < budget) {
      top_.eval();
      if (top_.q_valid_i && top_.q_ready_o) {
        tick();
        top_.q_valid_i = 0;
        break;
      }
      tick();
      ++spent;
    }
    Answer a{false, 0, 0};
    while (spent < budget) {
      top_.eval();
      if (top_.r_valid_o) {
        a.owned = top_.r_owned_o != 0;
        a.slot = top_.r_slot_o;
        a.gen = top_.r_gen_o;
        tick();
        return a;
      }
      tick();
      ++spent;
    }
    check(false, "the lookup answered within a bounded wait", 1, 0);
    return a;
  }

 private:
  Vzhao_texture_palette_load top_;
  std::vector<Op> ops_;
  uint32_t base_ = 0;
  uint32_t last_addr_ = 0;
  bool pending_ = false;
  bool deny_this_ = false;
  uint64_t pending_data_ = 0;
  int accepted_ = 0;
  int deny_after_ = -1;
};

// The BEGIN / 256 WRITE / END sequence, checked as a SEQUENCE. A block that
// wrote 255 entries, or wrote them out of order, or ended at a different
// generation from the one it began at, is a block whose palette is wrong in a
// way no single counter sees.
void check_sequence(const std::vector<Op>& ops, int slot, int gen, uint32_t base,
                    bool expect_crc_ok, const char* tag) {
  char what[192];
  std::snprintf(what, sizeof(what), "%s: the programming port saw BEGIN + %d WRITE + END", tag,
                kEntries);
  check(ops.size() == static_cast<size_t>(kEntries + 2), what, kEntries + 2,
        static_cast<long>(ops.size()));
  if (ops.size() != static_cast<size_t>(kEntries + 2)) return;

  std::snprintf(what, sizeof(what), "%s: it opens with BEGIN at the offered slot", tag);
  check(ops.front().op == 0 && ops.front().slot == slot && ops.front().gen == gen, what, gen,
        ops.front().gen);

  bool order_ok = true;
  bool data_ok = true;
  for (int e = 0; e < kEntries; ++e) {
    const Op& o = ops[static_cast<size_t>(e) + 1];
    if (o.op != 1 || o.idx != e) order_ok = false;
    const uint32_t word = static_cast<uint32_t>(e) >> 2;
    const uint16_t want =
        static_cast<uint16_t>(word_of(base, word) >> (16 * (e & 3)));
    if (o.rgb != want) data_ok = false;
  }
  std::snprintf(what, sizeof(what), "%s: every entry is written ONCE, in index order", tag);
  check(order_ok, what, 1, order_ok ? 1 : 0);
  std::snprintf(what, sizeof(what),
                "%s: each entry carries the memory word's own lane, not a neighbour's", tag);
  check(data_ok, what, 1, data_ok ? 1 : 0);

  const Op& end = ops.back();
  std::snprintf(what, sizeof(what), "%s: it closes with END at the SAME slot and generation", tag);
  check(end.op == 2 && end.slot == slot && end.gen == gen, what, gen, end.gen);
  std::snprintf(what, sizeof(what), "%s: the END's integrity verdict is the transfer's", tag);
  check(end.crc_ok == expect_crc_ok, what, expect_crc_ok ? 1 : 0, end.crc_ok ? 1 : 0);
}

// ===========================================================================
// CASE 1 -- THE FIRST LOAD IS AT GENERATION ZERO, and that is the whole point
// of this block's existence. `zhao_texture_palette_res_v2`'s BEGIN guard used
// to refuse generation zero outright, so a producer that allocated from a
// counter resetting to zero could not be built. The repair is in that block;
// this case is the statement that the producer USES it.
// ===========================================================================
void case1_first_load() {
  Bench b;
  const uint32_t base = 0x0040'0000u;
  const auto a = b.ask(base);
  check(a.owned, "case1: the first palette is OWNED", 1, a.owned ? 1 : 0);
  check(a.slot == 0, "case1: it lands in the first free slot", 0, a.slot);
  check(a.gen == 0, "case1: at GENERATION ZERO", 0, a.gen);
  check(b.top().gen_zero_loads_o == 1, "case1: and the generation-zero counter says so", 1,
        b.top().gen_zero_loads_o);
  check(b.top().loads_o == 1, "case1: exactly one load", 1, b.top().loads_o);
  check(b.top().hits_o == 0, "case1: a cold base is not a hit", 0, b.top().hits_o);
  check(b.top().lookups_o == 1, "case1: one lookup", 1, b.top().lookups_o);
  check(b.top().entries_written_o == kEntries, "case1: 256 entries written", kEntries,
        b.top().entries_written_o);
  check(b.top().evictions_o == 0, "case1: nothing was evicted", 0, b.top().evictions_o);
  check(b.top().denied_o == 0 && b.top().base_refused_o == 0,
        "case1: no fault counter moved", 0, b.top().denied_o + b.top().base_refused_o);
  check_sequence(b.ops(), 0, 0, base, true, "case1");
}

// ===========================================================================
// CASE 2 -- A SECOND ASK FOR THE SAME BASE IS A HIT AND TOUCHES NOTHING.
// `entries_written_o` NOT MOVING is the assertion that matters: a cache that
// re-loaded on every ask would answer identically and be invisible to every
// other counter here. That is this repository's "counters see what pictures
// cannot" law at this seam.
// ===========================================================================
void case2_hit() {
  Bench b;
  const uint32_t base = 0x0040'0000u;
  const auto first = b.ask(base);
  const uint32_t wrote = b.top().entries_written_o;
  b.clear_ops();
  const auto again = b.ask(base);
  check(again.owned && again.slot == first.slot && again.gen == first.gen,
        "case2: the same base answers the SAME identity", first.gen, again.gen);
  check(b.top().hits_o == 1, "case2: and it is counted as a hit", 1, b.top().hits_o);
  check(b.top().loads_o == 1, "case2: no second load was issued", 1, b.top().loads_o);
  check(b.top().entries_written_o == wrote,
        "case2: NOT ONE further entry was written -- the hit did no work",
        static_cast<long>(wrote), b.top().entries_written_o);
  check(b.ops().empty(), "case2: the programming port was silent", 0,
        static_cast<long>(b.ops().size()));
}

// ===========================================================================
// CASE 3 -- FOUR DISTINCT BASES FILL FOUR DISTINCT SLOTS AT GENERATION ZERO,
// and the FIFTH evicts. The generation a re-used slot is loaded at must differ
// from the one it held, or the resolver refuses the load -- so the sequence
// 0,0,0,0 then 1 on the victim is the contract, not an accident.
// ===========================================================================
void case3_fill_and_evict() {
  Bench b;
  const uint32_t bases[5] = {0x0040'0000u, 0x0041'0000u, 0x0042'0000u, 0x0043'0000u,
                             0x0044'0000u};
  int slots[5] = {0, 0, 0, 0, 0};
  int gens[5] = {0, 0, 0, 0, 0};
  for (int i = 0; i < 4; ++i) {
    const auto a = b.ask(bases[i]);
    slots[i] = a.slot;
    gens[i] = a.gen;
    check(a.owned, "case3: each of the first four bases is owned", 1, a.owned ? 1 : 0);
    check(a.gen == 0, "case3: each COLD slot is loaded at generation zero", 0, a.gen);
  }
  bool distinct = true;
  for (int i = 0; i < 4; ++i)
    for (int j = i + 1; j < 4; ++j)
      if (slots[i] == slots[j]) distinct = false;
  check(distinct, "case3: four distinct bases occupy four distinct slots", 1, distinct ? 1 : 0);
  check(b.top().gen_zero_loads_o == 4, "case3: four generation-zero loads", 4,
        b.top().gen_zero_loads_o);
  check(b.top().evictions_o == 0, "case3: nothing evicted while a slot was free", 0,
        b.top().evictions_o);

  b.clear_ops();
  const auto fifth = b.ask(bases[4]);
  slots[4] = fifth.slot;
  gens[4] = fifth.gen;
  check(fifth.owned, "case3: the fifth base is owned too", 1, fifth.owned ? 1 : 0);
  check(b.top().evictions_o == 1, "case3: and EXACTLY ONE slot was evicted for it", 1,
        b.top().evictions_o);
  check(b.top().loads_o == 5, "case3: five loads in all", 5, b.top().loads_o);
  // The victim's generation MUST have advanced. Equality with the generation it
  // held is the one value the resolver refuses.
  int victim_prev = -1;
  for (int i = 0; i < 4; ++i)
    if (slots[i] == slots[4]) victim_prev = gens[i];
  check(victim_prev >= 0, "case3: the fifth base reused one of the four slots", 1,
        victim_prev >= 0 ? 1 : 0);
  check(fifth.gen != victim_prev,
        "case3: a re-used slot is NEVER loaded at the generation it already holds", victim_prev,
        fifth.gen);
  check(fifth.gen == victim_prev + 1, "case3: it advances by exactly one", victim_prev + 1,
        fifth.gen);
  check_sequence(b.ops(), slots[4], fifth.gen, bases[4], true, "case3");

  // AND THE EVICTED BASE IS NO LONGER RESIDENT. A tag that kept answering for
  // a base whose bytes have been replaced is the stale-metadata fault, and it
  // would look perfectly well formed from outside.
  const uint32_t before = b.top().loads_o;
  b.ask(bases[victim_prev >= 0 ? 0 : 0]);
  check(b.top().loads_o > before,
        "case3: the evicted base RE-LOADS rather than answering from a stale tag",
        static_cast<long>(before) + 1, b.top().loads_o);
}

// ===========================================================================
// CASE 4 -- THE THREE REFUSED BASES. Each is an ANSWER with `owned` LOW and a
// counter, never a corrected address: "a clamped address writes real bytes
// into a real slot belonging to something else, and nothing downstream can
// tell". `base_refused_o` is fired here THREE times, by three different
// illegalities, so its zero in the console smoke is a measurement.
// ===========================================================================
void case4_refused_bases() {
  Bench b;
  const uint32_t before_loads = b.top().loads_o;

  const auto null_base = b.ask(0);
  check(!null_base.owned, "case4: a ZERO palette_base is refused", 0, null_base.owned ? 1 : 0);
  check(null_base.slot == 0 && null_base.gen == 0,
        "case4: and the refusal publishes a ZERO pair, not a slot", 0, null_base.slot);

  const auto misaligned = b.ask(0x0040'0004u);
  check(!misaligned.owned, "case4: a base that is not eight-byte aligned is refused", 0,
        misaligned.owned ? 1 : 0);

  // 512 bytes from here runs off the end of VRAM. The test is done one bit
  // wider than the address so the sum cannot wrap into a small, legal-looking
  // number -- MEM.UPLOAD's 33-bit containment reasoning at this block's scale.
  const auto past_end = b.ask(kVramBytes - 8u);
  check(!past_end.owned, "case4: a palette whose EXTENT leaves VRAM is refused", 0,
        past_end.owned ? 1 : 0);

  check(b.top().base_refused_o == 3, "case4: all three refusals are counted", 3,
        b.top().base_refused_o);
  check(b.top().loads_o == before_loads,
        "case4: and NOT ONE of them started a load", static_cast<long>(before_loads),
        b.top().loads_o);
  check(b.top().lookups_o == 3, "case4: each refusal is still a lookup", 3, b.top().lookups_o);

  // THE LAST LEGAL BASE IS ACCEPTED, which is what makes the refusal above a
  // bound rather than an off-by-one. 512 bytes ending exactly at the arena end.
  const auto last_legal = b.ask(kVramBytes - 512u);
  check(last_legal.owned, "case4: a palette ending EXACTLY at the VRAM end is accepted", 1,
        last_legal.owned ? 1 : 0);
  check(b.top().base_refused_o == 3, "case4: and refuses nothing further", 3,
        b.top().base_refused_o);
}

// ===========================================================================
// CASE 5 -- A DENIED FETCH. The block does NOT decide residency on the
// resolver's behalf: it still runs the END, with `ld_crc_ok_o` LOW, so the
// store refuses the load and counts it. `denied_o` fires, and the base is NOT
// left in the tag -- a later ask for it must re-load.
// ===========================================================================
void case5_denied_fetch() {
  Bench b;
  const uint32_t base = 0x0050'0000u;
  b.set_deny_after(7);       // the eighth accepted request is refused
  const auto a = b.ask(base);
  check(!a.owned, "case5: a denied fetch answers UNOWNED", 0, a.owned ? 1 : 0);
  check(b.top().denied_o == 1, "case5: the denial is counted exactly once", 1, b.top().denied_o);
  check(b.top().loads_o == 1, "case5: the load was still ISSUED -- the BEGIN happened", 1,
        b.top().loads_o);
  check(b.top().entries_written_o < kEntries,
        "case5: fewer than 256 entries reached the store", kEntries - 1,
        b.top().entries_written_o);

  const auto& ops = b.ops();
  check(!ops.empty() && ops.front().op == 0, "case5: it opened with a BEGIN", 0,
        ops.empty() ? -1 : ops.front().op);
  check(!ops.empty() && ops.back().op == 2,
        "case5: and it CLOSED with an END rather than abandoning the load", 2,
        ops.empty() ? -1 : ops.back().op);
  check(!ops.empty() && !ops.back().crc_ok,
        "case5: the END's integrity verdict is LOW, so the store refuses residency", 0,
        (!ops.empty() && ops.back().crc_ok) ? 1 : 0);

  // A FAILED LOAD LEAVES NO TAG. The next ask for the same base must load
  // again -- and at a DIFFERENT generation, because the resolver already saw
  // the failed one.
  b.set_deny_after(-1);
  b.clear_ops();
  const auto retry = b.ask(base);
  check(retry.owned, "case5: the retry succeeds", 1, retry.owned ? 1 : 0);
  check(b.top().loads_o == 2, "case5: it was a real second load, not a tag hit", 2,
        b.top().loads_o);
  check(b.top().hits_o == 0, "case5: a failed load never becomes a hit", 0, b.top().hits_o);
  check(retry.gen != a.gen,
        "case5: the retry uses a generation the failed attempt did not", a.gen, retry.gen);
  check_sequence(b.ops(), retry.slot, retry.gen, base, true, "case5");
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  case1_first_load();
  case2_hit();
  case3_fill_and_evict();
  case4_refused_bases();
  case5_denied_fetch();
  if (fails != 0) {
    std::printf("[texture_palette_load_directed] %d/%d checks FAILED\n", fails, checks);
    zhao::exit_hard(1);
  }
  std::printf("[texture_palette_load_directed] %d checks passed\n", checks);
  zhao::exit_hard(0);
}
