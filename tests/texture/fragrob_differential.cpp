// fragrob_differential.cpp — the new transaction centre against the old one.
//
// ---------------------------------------------------------------------------
// WHY A DIFFERENTIAL AND NOT A FRESH SPEC
// ---------------------------------------------------------------------------
// reports/islandrearchitecture5.md §6.1: build `zhao_texture_fragrob` beside
// `zhao_raster_texjoin_v2` and "keep v2 as the oracle for token allocation,
// multi-sample completion, generation rejection and allocation-order
// retirement."
//
// v2 is wrong about STORAGE — 7,151 registers holding a 7,056-bit table — and
// right about BEHAVIOUR, which its own suite pins. So the replacement is
// checked against it rather than against a re-reading of the brief: a
// behavioural divergence becomes a test failure instead of an argument.
//
// ---------------------------------------------------------------------------
// WHAT IS COMPARED, AND WHAT DELIBERATELY IS NOT
// ---------------------------------------------------------------------------
// NOT cycle-by-cycle. fragrob has an extra pipeline stage on issue and one on
// retire, on purpose: a descriptor living in a bank cannot be picked
// combinationally, and that latency is the price of the storage being memory.
// A cycle-exact comparison would fail by construction and would be testing the
// wrong thing.
//
// What is compared is the CONTRACT: the same fragments come out, in the same
// order, carrying the same values. Allocation order is retirement order, so
// "same order" is a real property and not an artefact.
//
// ---------------------------------------------------------------------------
// THE SHARED RESPONDER IS THE POINT
// ---------------------------------------------------------------------------
// Both DUTs are driven by ONE model of the outside world, which answers a TMU
// request by returning a colour derived from the request itself. That is what
// makes the comparison meaningful under different latencies: the answer
// depends on what was asked, never on when.
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_texjoin_v2.h"
#include "Vzhao_texture_fragrob.h"

#include "zhao_sim.hpp"

namespace {

constexpr int kDepth = 16;

struct Frag {
  int count;
  uint32_t u[3], v[3];
  int recipe;
  uint64_t ctx;
  bool aux;
};

// What comes out. `ctx` identifies the fragment, so a reordering is visible
// rather than merely a value mismatch.
struct Retired {
  uint64_t ctx;
  uint32_t rgb;
  uint32_t a;
  uint32_t aux_rgb;
  uint32_t aux_a;
  int has_aux;
};

bool operator!=(const Retired& x, const Retired& y) {
  return x.ctx != y.ctx || x.rgb != y.rgb || x.a != y.a || x.aux_rgb != y.aux_rgb ||
         x.aux_a != y.aux_a || x.has_aux != y.has_aux;
}

// The world both blocks see. A sample's colour is a pure function of the
// request, so latency cannot change an answer -- only when it arrives.
uint32_t sample_rgb(int slot, int sidx) {
  return static_cast<uint32_t>(((slot * 7 + sidx * 3) & 0xFF) | ((slot * 11) & 0xFF) << 8 |
                               ((sidx * 37) & 0xFF) << 16);
}
uint32_t sample_a(int slot, int sidx) { return static_cast<uint32_t>((slot * 5 + sidx) & 0xFF); }
uint32_t aux_rgb_of(int slot) { return static_cast<uint32_t>(((slot * 13) & 0xFF) | 0x5A00); }
uint32_t aux_a_of(int slot) { return static_cast<uint32_t>((slot * 3 + 1) & 0xFF); }

// One in-flight TMU or AUX return, held so it can be delivered LATE. Delay is
// what exercises the identity check: a return that arrives after its slot was
// reused must be refused by generation, not accepted by position.
struct Pending {
  int delay;
  int slot, sidx, gen;
  bool is_aux;
};

// Run `frags` through one DUT and collect what it retires.
//
// Templated on the DUT because the two have identical port lists by design --
// which is itself part of the contract, and would stop compiling if it broke.
template <typename Dut>
std::vector<Retired> run(const std::vector<Frag>& frags, unsigned seed, bool stall_out,
                         int rsp_delay) {
  Dut t;
  std::vector<Retired> out;
  std::deque<Pending> inflight;
  uint32_t g = seed;
  auto rnd = [&]() {
    g = g * 1664525u + 1013904223u;
    return g;
  };

  t.f_valid_i = 0;
  t.tmu_ready_i = 1;
  t.tmu_rvalid_i = 0;
  t.aux_ready_i = 1;
  t.aux_rvalid_i = 0;
  t.o_ready_i = 1;
  t.rst_n = 0;
  for (int i = 0; i < 8; ++i) zhao::tick(t);
  t.rst_n = 1;
  zhao::tick(t);

  size_t next = 0;
  // Generous bound: every fragment needs several clocks and the block may
  // legitimately stall. A hang shows up as a short output list, not a hang.
  for (int cycle = 0; cycle < 40000; ++cycle) {
    // ---- offer the next fragment ----------------------------------------
    if (next < frags.size()) {
      const Frag& f = frags[next];
      t.f_valid_i = 1;
      t.f_sample_count_i = f.count;
      for (int j = 0; j < 3; ++j) {
        t.f_u_i[j] = f.u[j];
        t.f_v_i[j] = f.v[j];
        t.f_binding_i[j] = static_cast<uint8_t>(j + 1);
        t.f_lod_i[j] = 0;
      }
      t.f_recipe_i = f.recipe;
      t.f_ctx_i = f.ctx;
      t.f_aux_i = f.aux ? 1 : 0;
      t.f_uv_sat_i = 0;
    } else {
      t.f_valid_i = 0;
    }

    t.o_ready_i = stall_out ? ((rnd() >> 27) & 1u) : 1u;
    t.tmu_ready_i = 1;
    t.aux_ready_i = 1;

    // ---- deliver a due return -------------------------------------------
    t.tmu_rvalid_i = 0;
    t.aux_rvalid_i = 0;
    for (auto& p : inflight) {
      // EXACTLY zero. `> 0` let an already-delivered entry (marked -1) be
      // delivered again every cycle, so everything behind it starved and the
      // harness deadlocked the DUT rather than the DUT deadlocking itself.
      if (p.delay != 0) continue;
      if (p.is_aux) {
        t.aux_rvalid_i = 1;
        t.aux_rslot_i = p.slot;
        t.aux_rgen_i = p.gen;
        t.aux_rgb_i = aux_rgb_of(p.slot);
        t.aux_a_i = aux_a_of(p.slot);
      } else {
        t.tmu_rvalid_i = 1;
        t.tmu_rslot_i = p.slot;
        t.tmu_rsidx_i = p.sidx;
        t.tmu_rgen_i = p.gen;
        t.tmu_rgb_i = sample_rgb(p.slot, p.sidx);
        t.tmu_a_i = sample_a(p.slot, p.sidx);
      }
      p.delay = -1;  // marked delivered
      break;
    }

    t.eval();

    const bool took_frag = t.f_valid_i && t.f_ready_o;
    if (t.tmu_valid_o && t.tmu_ready_i) {
      inflight.push_back({rsp_delay, static_cast<int>(t.tmu_slot_o), static_cast<int>(t.tmu_sidx_o),
                          static_cast<int>(t.tmu_gen_o), false});
    }
    if (t.aux_valid_o && t.aux_ready_i) {
      inflight.push_back(
          {rsp_delay, static_cast<int>(t.aux_slot_o), 0, static_cast<int>(t.aux_gen_o), true});
    }
    if (t.o_valid_o && t.o_ready_i) {
      out.push_back({static_cast<uint64_t>(t.o_ctx_o), static_cast<uint32_t>(t.o_rgb_o),
                     static_cast<uint32_t>(t.o_a_o), static_cast<uint32_t>(t.o_aux_rgb_o),
                     static_cast<uint32_t>(t.o_aux_a_o), static_cast<int>(t.o_has_aux_o)});
    }

    zhao::tick(t);
    if (took_frag) ++next;

    for (auto& p : inflight) {
      if (p.delay > 0) --p.delay;
    }
    while (!inflight.empty() && inflight.front().delay < 0) inflight.pop_front();
    // and drop delivered entries that are not at the front
    for (size_t i = 0; i < inflight.size();) {
      if (inflight[i].delay < 0)
        inflight.erase(inflight.begin() + static_cast<long>(i));
      else
        ++i;
    }

    if (next >= frags.size() && out.size() >= frags.size()) break;
  }
  return out;
}

// ===========================================================================
// THE OBSERVABLE THIS SUITE DID NOT HAVE
// ===========================================================================
// `run()` above records aux_slot_o and aux_gen_o on the AUX handshake -- and
// never once reads aux_ctx_o. That single gap is why the AUX context defect
// survived every green run of this file:
//
//   zhao_texture_fragrob.sv popped a new slot into ax_slot_q and raised
//   ax_pend_q on the same edge that the bank process ran the UNCONDITIONAL
//   `ax_ctx_r <= ctx_m[ax_slot_q]`. Both nonblocking, so the context loaded was
//   the OLD slot's. The IDENTITY travelled correctly -- which is all `run()`
//   ever looked at -- while the PAYLOAD was the previous AUX fragment's. And
//   because aux_ready_i is tied high through the island
//   (island_top.sv:1102 -> aux_pipe.sv:165 `req_ready_o = 1'b1`), that
//   mismatched cycle was the ONLY cycle any request ever got: not a window, an
//   always. The island reads that word as world X/Z, so every AUX sheet lookup
//   sampled the wrong place on the map.
//
// A passing test that never samples the broken signal is not evidence. This
// probe samples it.
//
// WHY IT IS FRAGROB-ONLY AND NOT PART OF THE DIFFERENTIAL: knowing what
// aux_ctx_o is SUPPOSED to contain means knowing which slot each fragment
// landed in, which is what `alloc_slot_o`/`alloc_valid_o` are for -- and
// zhao_raster_texjoin_v2 does not have those ports. The differential template
// must compile against both, so the comparison lives here instead.
struct AuxProbe {
  int handshakes = 0;   // AUX requests actually accepted
  int ctx_wrong = 0;    // ... carrying a context that is not their slot's
  int held_mutated = 0; // ... whose held tuple changed while valid was up
  int retired = 0;
};

AuxProbe aux_ctx_probe(int n, unsigned seed, bool stall_aux, int rsp_delay) {
  Vzhao_texture_fragrob t;
  AuxProbe pr;
  std::deque<Pending> inflight;

  // What the block itself told us, at allocation, about where each fragment
  // went. This is the ORACLE for aux_ctx_o.
  uint64_t slot_ctx[kDepth];
  bool slot_known[kDepth];
  for (int i = 0; i < kDepth; ++i) {
    slot_ctx[i] = 0;
    slot_known[i] = false;
  }

  uint32_t g = seed;
  auto rnd = [&]() {
    g = g * 1664525u + 1013904223u;
    return g;
  };

  t.f_valid_i = 0;
  t.tmu_ready_i = 1;
  t.tmu_rvalid_i = 0;
  t.aux_ready_i = 1;
  t.aux_rvalid_i = 0;
  t.o_ready_i = 1;
  t.rst_n = 0;
  for (int i = 0; i < 8; ++i) zhao::tick(t);
  t.rst_n = 1;
  zhao::tick(t);

  bool held = false;  // a request was offered last cycle and NOT accepted
  uint64_t held_ctx = 0;
  int held_slot = 0, held_gen = 0;
  int next = 0;

  for (int cycle = 0; cycle < 40000; ++cycle) {
    if (next < n) {
      t.f_valid_i = 1;
      t.f_sample_count_i = 1;
      for (int j = 0; j < 3; ++j) {
        t.f_u_i[j] = 1;
        t.f_v_i[j] = 1;
        t.f_binding_i[j] = 1;
        t.f_lod_i[j] = 0;
      }
      t.f_recipe_i = 0;
      // Every fragment's context is DISTINCT and encodes its index, so a
      // request carrying the previous fragment's word is a value mismatch and
      // not an unlucky collision. Both halves are non-zero because the island
      // splits this word into world X and world Z.
      t.f_ctx_i = 0x00A0C000ull + static_cast<uint64_t>(next) * 0x0001000100010101ull;
      t.f_tok_i = static_cast<uint8_t>(next & 0x3F);
      t.f_aux_i = 1;  // every fragment wants AUX: this is the path under test
      t.f_uv_sat_i = 0;
    } else {
      t.f_valid_i = 0;
    }
    t.o_ready_i = 1;
    t.tmu_ready_i = 1;
    // The island holds aux_ready HIGH forever, which is what made the defect
    // unconditional -- so that is the default case. The stalled case is the
    // other half of what the addendum asks for: proof that the held tuple does
    // not MUTATE under a receiver that is slow to take it.
    t.aux_ready_i = stall_aux ? ((rnd() >> 28) & 1u) : 1u;

    t.tmu_rvalid_i = 0;
    t.aux_rvalid_i = 0;
    for (auto& p : inflight) {
      if (p.delay != 0) continue;
      if (p.is_aux) {
        t.aux_rvalid_i = 1;
        t.aux_rslot_i = p.slot;
        t.aux_rgen_i = p.gen;
        t.aux_rgb_i = aux_rgb_of(p.slot);
        t.aux_a_i = aux_a_of(p.slot);
      } else {
        t.tmu_rvalid_i = 1;
        t.tmu_rslot_i = p.slot;
        t.tmu_rsidx_i = p.sidx;
        t.tmu_rgen_i = p.gen;
        t.tmu_rgb_i = sample_rgb(p.slot, p.sidx);
        t.tmu_a_i = sample_a(p.slot, p.sidx);
      }
      p.delay = -1;
      break;
    }

    t.eval();

    const bool took_frag = t.f_valid_i && t.f_ready_o;
    if (took_frag) {
      const int sl = static_cast<int>(t.alloc_slot_o);
      slot_ctx[sl] = static_cast<uint64_t>(t.f_ctx_i);
      slot_known[sl] = true;
    }

    if (t.aux_valid_o) {
      const uint64_t c = static_cast<uint64_t>(t.aux_ctx_o);
      const int sl = static_cast<int>(t.aux_slot_o);
      const int gn = static_cast<int>(t.aux_gen_o);
      // Held-request stability. valid was up last cycle and was refused, so
      // NOTHING in the tuple may have moved: a ready/valid producer that
      // mutates its payload under a stalled receiver is broken even when every
      // individual word it emits is correct.
      if (held && (c != held_ctx || sl != held_slot || gn != held_gen)) ++pr.held_mutated;
      held = true;
      held_ctx = c;
      held_slot = sl;
      held_gen = gn;

      if (t.aux_ready_i) {
        ++pr.handshakes;
        // ---- THE COMPARISON THAT WAS MISSING ----------------------------
        // The context on the wire must be the context of the fragment that
        // owns aux_slot_o -- not the one before it.
        if (!slot_known[sl] || c != slot_ctx[sl]) ++pr.ctx_wrong;
        inflight.push_back({rsp_delay, sl, 0, gn, true});
        held = false;
      }
    } else {
      held = false;
    }

    if (t.tmu_valid_o && t.tmu_ready_i) {
      inflight.push_back({rsp_delay, static_cast<int>(t.tmu_slot_o), static_cast<int>(t.tmu_sidx_o),
                          static_cast<int>(t.tmu_gen_o), false});
    }
    if (t.o_valid_o && t.o_ready_i) ++pr.retired;

    zhao::tick(t);
    if (took_frag) ++next;

    for (auto& p : inflight) {
      if (p.delay > 0) --p.delay;
    }
    for (size_t i = 0; i < inflight.size();) {
      if (inflight[i].delay < 0)
        inflight.erase(inflight.begin() + static_cast<long>(i));
      else
        ++i;
    }

    if (next >= n && pr.retired >= n) break;
  }
  return pr;
}

std::vector<Frag> make_frags(int n, unsigned seed) {
  std::vector<Frag> v;
  uint32_t g = seed;
  auto rnd = [&]() {
    g = g * 1103515245u + 12345u;
    return g;
  };
  for (int i = 0; i < n; ++i) {
    Frag f{};
    // 1..3 samples. Zero-sample fragments are a separate directed case: they
    // retire with no TMU traffic at all and would otherwise be lost in a
    // random mix.
    f.count = 1 + static_cast<int>((rnd() >> 13) % 3u);
    for (int j = 0; j < 3; ++j) {
      f.u[j] = rnd();
      f.v[j] = rnd();
    }
    f.recipe = static_cast<int>((rnd() >> 9) % 6u);
    f.ctx = 0xC000ull + static_cast<uint64_t>(i);
    f.aux = ((rnd() >> 17) & 1u) != 0u;
    v.push_back(f);
  }
  return v;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  struct Case {
    const char* name;
    int n;
    unsigned seed;
    bool stall;
    int delay;
  };
  const Case cases[] = {
      {"no stalls, immediate returns", 40, 0x11u, false, 0},
      {"no stalls, late returns", 40, 0x22u, false, 3},
      {"stalling consumer", 40, 0x33u, true, 1},
      {"stalling consumer + late returns", 64, 0x44u, true, 5},
      // More fragments than slots, so allocation must block and the free list
      // must recycle -- the case where a slot is reused and a stale return
      // could be mismatched by position.
      {"deeper than the slot count", 96, 0x55u, true, 4},
  };

  int diverged = 0;
  int compared = 0;
  for (const Case& c : cases) {
    const auto frags = make_frags(c.n, c.seed);
    const auto a = run<Vzhao_raster_texjoin_v2>(frags, c.seed, c.stall, c.delay);
    const auto b = run<Vzhao_texture_fragrob>(frags, c.seed, c.stall, c.delay);

    if (a.size() != b.size()) {
      std::printf("    %-34s v2 retired %zu, fragrob %zu\n", c.name, a.size(), b.size());
      ++diverged;
      continue;
    }
    for (size_t i = 0; i < a.size(); ++i) {
      ++compared;
      if (a[i] != b[i]) {
        if (diverged < 4) {
          std::printf(
              "    %-30s #%zu  v2 ctx=%llu rgb=%06X a=%02X aux=%d | "
              "fragrob ctx=%llu rgb=%06X a=%02X aux=%d\n",
              c.name, i, (unsigned long long)a[i].ctx, a[i].rgb, a[i].a, a[i].has_aux,
              (unsigned long long)b[i].ctx, b[i].rgb, b[i].a, b[i].has_aux);
        }
        ++diverged;
      }
    }
  }

  // A differential that compared nothing would pass. This is the guard that
  // says the stimulus actually reached both blocks.
  zhao::check(compared > 200,
              "the differential actually retired fragments from both blocks "
              "rather than comparing two empty lists",
              1, compared > 200 ? 1 : 0);

  zhao::check(diverged == 0,
              "zhao_texture_fragrob retires the SAME fragments in the SAME "
              "order with the SAME values as zhao_raster_texjoin_v2 -- the "
              "storage changed, the behaviour did not",
              0, diverged);

  std::printf("  %d retired fragments compared across %d scenarios\n", compared,
              static_cast<int>(sizeof(cases) / sizeof(cases[0])));

  // =========================================================================
  // DIRECTED: the refusal paths, which a differential cannot reach
  // =========================================================================
  // v2 and fragrob agree on the happy path by construction now. These are the
  // cases where the block is supposed to REFUSE something, and a differential
  // against a block with the same blind spot would agree about being wrong.
  {
    Vzhao_texture_fragrob t;
    t.f_valid_i = 0;
    t.tmu_ready_i = 1;
    t.tmu_rvalid_i = 0;
    t.aux_ready_i = 1;
    t.aux_rvalid_i = 0;
    t.o_ready_i = 0;  // never retire: fills the slot table
    t.rst_n = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(t);
    t.rst_n = 1;
    zhao::tick(t);

    // ---- the sixteen-cycle init sweep holds f_ready low ------------------
    // A block that accepted a fragment before its free list existed would
    // allocate slot garbage. `f_ready_o` must be low until the sweep ends.
    t.eval();
    const int ready_at_reset = t.f_ready_o;
    for (int i = 0; i < 20; ++i) zhao::tick(t);
    t.eval();
    zhao::check(ready_at_reset == 0 && t.f_ready_o == 1,
                "the free-list sweep holds f_ready low until it has slots to "
                "hand out, then raises it",
                1, (ready_at_reset == 0 && t.f_ready_o == 1) ? 1 : 0);

    // ---- allocation BLOCKS when full, and is counted --------------------
    Frag f{};
    f.count = 1;
    f.ctx = 0xAAA;
    int accepted = 0;
    for (int i = 0; i < 40; ++i) {
      t.f_valid_i = 1;
      t.f_sample_count_i = f.count;
      for (int j = 0; j < 3; ++j) {
        t.f_u_i[j] = 1;
        t.f_v_i[j] = 1;
        t.f_binding_i[j] = 1;
        t.f_lod_i[j] = 0;
      }
      t.f_recipe_i = 0;
      t.f_ctx_i = f.ctx + i;
      t.f_aux_i = 0;
      t.f_uv_sat_i = 0;
      t.eval();
      if (t.f_ready_o) ++accepted;
      zhao::tick(t);
    }
    t.f_valid_i = 0;
    t.eval();
    zhao::check(accepted == kDepth,
                "exactly DEPTH fragments are accepted while nothing retires -- "
                "allocation blocks rather than overwriting a live slot",
                kDepth, accepted);
    zhao::check(t.full_clocks_o > 0,
                "and the clocks spent full are COUNTED, so a starved raster is "
                "visible rather than merely slow",
                1, t.full_clocks_o > 0 ? 1 : 0);

    // ---- A STALE RETURN IS REFUSED BY IDENTITY, NOT POSITION ------------
    // The property GENW=8 exists for. Ruling X5: a 2-bit generation wraps
    // after four reuses, so a return delayed longer matches the WRONG fragment
    // and is silently accepted -- worse than the stale return it was meant to
    // catch. Here the generation is deliberately wrong for a live slot.
    const uint32_t id_before = t.id_errors_o;
    t.tmu_rvalid_i = 1;
    t.tmu_rslot_i = 0;
    t.tmu_rsidx_i = 0;
    t.tmu_rgen_i = 0xEE;  // no slot ever had this generation
    t.tmu_rgb_i = 0xBADBAD;
    t.tmu_a_i = 0xFF;
    t.eval();
    zhao::tick(t);
    t.tmu_rvalid_i = 0;
    t.eval();
    zhao::check(t.id_errors_o == id_before + 1,
                "a return whose generation matches no live slot is REFUSED and "
                "counted -- never applied to whatever now occupies that slot",
                1, static_cast<int>(t.id_errors_o - id_before));

    // ---- SIMULTANEOUS TMU AND AUX FAULTS COUNT TWO, NOT ONE -------------
    // The block used to carry two independent `id_errors_o <= id_errors_o + 1`
    // statements in ONE always_ff -- one in the TMU return branch, one in the
    // AUX return branch. Two nonblocking assignments to the same register on
    // the same edge: the last one wins, and a clock carrying two faults moved
    // the counter by one. The counter under-reported exactly when the machine
    // was in the most trouble, and a low error count reads as health.
    //
    // Both returns below are bogus for the same reason the one above is -- a
    // generation no slot ever held -- and they are presented on the SAME clock,
    // which is the case the two-writer form could not represent.
    const uint32_t id_before_pair = t.id_errors_o;
    t.tmu_rvalid_i = 1;
    t.tmu_rslot_i = 0;
    t.tmu_rsidx_i = 0;
    t.tmu_rgen_i = 0xEE;
    t.tmu_rgb_i = 0xBADBAD;
    t.tmu_a_i = 0xFF;
    t.aux_rvalid_i = 1;
    t.aux_rslot_i = 1;
    t.aux_rgen_i = 0xEF;
    t.aux_rgb_i = 0xBADBAD;
    t.aux_a_i = 0xFF;
    t.eval();
    zhao::tick(t);
    t.tmu_rvalid_i = 0;
    t.aux_rvalid_i = 0;
    t.eval();
    zhao::check(t.id_errors_o == id_before_pair + 2,
                "a TMU fault and an AUX fault on the SAME clock increment "
                "id_errors by exactly TWO -- one accumulator, one delta, so "
                "neither event is lost to the other's assignment",
                2, static_cast<int>(t.id_errors_o - id_before_pair));
  }

  // =========================================================================
  // DIRECTED: an out-of-range / unrequested / duplicate return
  // =========================================================================
  // The acceptance predicate used to be two terms -- live slot, matching
  // generation -- and nothing else. Everything below passed that test and was
  // APPLIED, and every one of them wedges the block permanently rather than
  // corrupting one pixel: `arr_q` is compared for EQUALITY against `req_q`, and
  // retirement is strictly allocation-ordered with no timeout. Set one bit
  // req_q does not have and the head fragment is never done, so the head never
  // advances, so nothing behind it retires either. The whole block stops.
  //
  // The six-term predicate is live && generation && index-in-range &&
  // requested && issued && !arrived. Here is one return failing each of the
  // new terms, and the genuine one passing.
  {
    Vzhao_texture_fragrob t;
    t.f_valid_i = 0;
    t.tmu_ready_i = 1;
    t.tmu_rvalid_i = 0;
    t.aux_ready_i = 1;
    t.aux_rvalid_i = 0;
    t.o_ready_i = 0;  // hold the fragment live while the bogus returns land
    t.rst_n = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(t);
    t.rst_n = 1;
    for (int i = 0; i < 24; ++i) zhao::tick(t);  // let the free-list sweep finish

    // ONE fragment asking for ONE sample, so sample 0 is requested and samples
    // 1 and 2 provably are not.
    int slot = -1, gen = -1;
    bool sent = false;
    for (int i = 0; i < 64 && slot < 0; ++i) {
      t.f_valid_i = sent ? 0 : 1;
      t.f_sample_count_i = 1;
      for (int j = 0; j < 3; ++j) {
        t.f_u_i[j] = 7;
        t.f_v_i[j] = 9;
        t.f_binding_i[j] = 1;
        t.f_lod_i[j] = 0;
      }
      t.f_recipe_i = 0;
      t.f_ctx_i = 0xD00Dull;
      t.f_tok_i = 0;
      t.f_aux_i = 0;
      t.f_uv_sat_i = 0;
      t.eval();
      if (!sent && t.f_valid_i && t.f_ready_o) sent = true;
      // The block's own request tells us the identity a genuine answer carries.
      if (t.tmu_valid_o && t.tmu_ready_i) {
        slot = static_cast<int>(t.tmu_slot_o);
        gen = static_cast<int>(t.tmu_gen_o);
      }
      zhao::tick(t);
    }
    t.f_valid_i = 0;
    zhao::check(slot >= 0,
                "the return-validation fixture actually got a TMU request out, "
                "so the refusals below are being aimed at a real transaction",
                1, slot >= 0 ? 1 : 0);

    auto inject = [&](int sl, int sidx, int gn, uint32_t rgb, uint32_t a) {
      t.tmu_rvalid_i = 1;
      t.tmu_rslot_i = static_cast<uint8_t>(sl);
      t.tmu_rsidx_i = static_cast<uint8_t>(sidx);
      t.tmu_rgen_i = static_cast<uint8_t>(gn);
      t.tmu_rgb_i = rgb;
      t.tmu_a_i = static_cast<uint8_t>(a);
      t.eval();
      zhao::tick(t);
      t.tmu_rvalid_i = 0;
      t.eval();
    };

    const uint32_t e_unreq = t.id_errors_o;
    inject(slot, 2, gen, 0x111111u, 0x11u);  // in range, but never requested
    const int d_unreq = static_cast<int>(t.id_errors_o - e_unreq);

    const uint32_t e_oor = t.id_errors_o;
    inject(slot, 3, gen, 0x222222u, 0x22u);  // sample index 3: not a sample
    const int d_oor = static_cast<int>(t.id_errors_o - e_oor);

    const uint32_t e_good = t.id_errors_o;
    inject(slot, 0, gen, 0x123456u, 0x77u);  // the one genuine answer
    const int d_good = static_cast<int>(t.id_errors_o - e_good);

    const uint32_t e_dup = t.id_errors_o;
    inject(slot, 0, gen, 0xBADBADu, 0xEEu);  // the same answer a second time
    const int d_dup = static_cast<int>(t.id_errors_o - e_dup);

    zhao::check(d_unreq == 1,
                "a return for a sample the fragment never requested is REFUSED "
                "and counted -- accepted, it sets a bit outside req_q and "
                "arr_q can never again equal req_q",
                1, d_unreq);
    zhao::check(d_oor == 1,
                "a return with sample index 3 is REFUSED and counted -- it used "
                "to be accepted as valid while its write and its bit-select "
                "were silently dropped, so the sample simply never arrived",
                1, d_oor);
    zhao::check(d_good == 0,
                "and the one return that satisfies all six terms is taken "
                "silently, so the predicate did not simply become a wall",
                0, d_good);
    zhao::check(d_dup == 1,
                "a DUPLICATE of an already-arrived sample is REFUSED and "
                "counted -- the issued bit is cleared on acceptance, which is "
                "what makes the second copy a refusal instead of an overwrite",
                1, d_dup);

    // ---- AND THE BLOCK IS NOT WEDGED ------------------------------------
    // The counters above say the returns were refused. This says the refusal
    // left the transaction intact: the fragment still completes, in order, and
    // its colour is the GENUINE answer rather than the duplicate's payload.
    t.o_ready_i = 1;
    int got = 0;
    uint64_t got_ctx = 0;
    uint32_t got_rgb = 0, got_a = 0;
    for (int i = 0; i < 64 && !got; ++i) {
      t.eval();
      if (t.o_valid_o && t.o_ready_i) {
        got = 1;
        got_ctx = static_cast<uint64_t>(t.o_ctx_o);
        got_rgb = static_cast<uint32_t>(t.o_rgb_o);
        got_a = static_cast<uint32_t>(t.o_a_o);
      }
      zhao::tick(t);
    }
    zhao::check(got == 1 && got_ctx == 0xD00Dull,
                "and the fragment still RETIRES after three refused returns -- "
                "any one of them, accepted, would have frozen the "
                "allocation-ordered head and taken the whole block with it",
                1, got);
    zhao::check(got_rgb == 0x123456u && got_a == 0x77u,
                "carrying the GENUINE sample, not the duplicate's payload -- a "
                "refused duplicate must not overwrite a result that has already "
                "arrived",
                0x123456, static_cast<int>(got_rgb));
  }

  // =========================================================================
  // DIRECTED: the AUX context belongs to the AUX slot
  // =========================================================================
  // See the aux_ctx_probe comment above for why this observable was absent and
  // what it cost. Two runs: aux_ready tied high, which is the island's actual
  // wiring and the case where the wrong context was the ONLY context ever
  // presented; and a stalling receiver, which is the case that proves the held
  // tuple is stable rather than merely correct on its first cycle.
  {
    const AuxProbe always_ready = aux_ctx_probe(48, 0x77u, false, 0);
    const AuxProbe stalled = aux_ctx_probe(48, 0x88u, true, 3);

    zhao::check(always_ready.handshakes >= 48 && stalled.handshakes >= 48,
                "the AUX probe actually completed its requests, so the context "
                "comparison below ran on real traffic rather than on nothing",
                96, always_ready.handshakes + stalled.handshakes);

    zhao::check(always_ready.ctx_wrong == 0,
                "with aux_ready tied high -- the island's actual wiring -- every "
                "AUX request carries the context of the fragment in aux_slot_o, "
                "not the previous AUX fragment's. This is the ONLY cycle a "
                "request ever gets, and the island reads that word as world X/Z",
                0, always_ready.ctx_wrong);

    zhao::check(stalled.ctx_wrong == 0,
                "and the same holds when the receiver stalls, so the fix is an "
                "AUX_READ phase and not a receiver that happened to be late",
                0, stalled.ctx_wrong);

    zhao::check(stalled.held_mutated == 0 && always_ready.held_mutated == 0,
                "a held AUX request never mutates: while aux_valid_o is up and "
                "unaccepted, ctx, slot and generation are all frozen",
                0, stalled.held_mutated + always_ready.held_mutated);

    zhao::check(always_ready.retired == 48 && stalled.retired == 48,
                "and every AUX fragment retired, so the added read phase costs "
                "a cycle of latency and not a request",
                96, always_ready.retired + stalled.retired);
  }

  // ---- a zero-sample fragment retires with no TMU traffic at all --------
  // Excluded from the random mix because it would be lost among fragments that
  // do request samples, and it is the one case with no TMU handshake to wait
  // on: it must not hang waiting for a sample it never asked for.
  {
    std::vector<Frag> zero;
    Frag f{};
    f.count = 0;
    f.ctx = 0x5E70;
    f.aux = false;
    zero.push_back(f);
    const auto out = run<Vzhao_texture_fragrob>(zero, 0x99u, false, 0);
    zhao::check(out.size() == 1 && out[0].ctx == 0x5E70,
                "a zero-sample fragment retires without a single TMU request, "
                "rather than waiting forever for a sample it never asked for",
                1, static_cast<int>(out.size()));
  }

  return zhao::report_and_exit("fragrob_differential");
}
