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
  return x.ctx != y.ctx || x.rgb != y.rgb || x.a != y.a ||
         x.aux_rgb != y.aux_rgb || x.aux_a != y.aux_a ||
         x.has_aux != y.has_aux;
}

// The world both blocks see. A sample's colour is a pure function of the
// request, so latency cannot change an answer -- only when it arrives.
uint32_t sample_rgb(int slot, int sidx) {
  return static_cast<uint32_t>(((slot * 7 + sidx * 3) & 0xFF) |
                               ((slot * 11) & 0xFF) << 8 |
                               ((sidx * 37) & 0xFF) << 16);
}
uint32_t sample_a(int slot, int sidx) {
  return static_cast<uint32_t>((slot * 5 + sidx) & 0xFF);
}
uint32_t aux_rgb_of(int slot) {
  return static_cast<uint32_t>(((slot * 13) & 0xFF) | 0x5A00);
}
uint32_t aux_a_of(int slot) {
  return static_cast<uint32_t>((slot * 3 + 1) & 0xFF);
}

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
std::vector<Retired> run(const std::vector<Frag>& frags, unsigned seed,
                         bool stall_out, int rsp_delay) {
  Dut t;
  std::vector<Retired> out;
  std::deque<Pending> inflight;
  uint32_t g = seed;
  auto rnd = [&]() { g = g * 1664525u + 1013904223u; return g; };

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
      inflight.push_back({rsp_delay, static_cast<int>(t.tmu_slot_o),
                          static_cast<int>(t.tmu_sidx_o),
                          static_cast<int>(t.tmu_gen_o), false});
    }
    if (t.aux_valid_o && t.aux_ready_i) {
      inflight.push_back({rsp_delay, static_cast<int>(t.aux_slot_o), 0,
                          static_cast<int>(t.aux_gen_o), true});
    }
    if (t.o_valid_o && t.o_ready_i) {
      out.push_back({static_cast<uint64_t>(t.o_ctx_o),
                     static_cast<uint32_t>(t.o_rgb_o),
                     static_cast<uint32_t>(t.o_a_o),
                     static_cast<uint32_t>(t.o_aux_rgb_o),
                     static_cast<uint32_t>(t.o_aux_a_o),
                     static_cast<int>(t.o_has_aux_o)});
    }

    zhao::tick(t);
    if (took_frag) ++next;

    for (auto& p : inflight) {
      if (p.delay > 0) --p.delay;
    }
    while (!inflight.empty() && inflight.front().delay < 0) inflight.pop_front();
    // and drop delivered entries that are not at the front
    for (size_t i = 0; i < inflight.size();) {
      if (inflight[i].delay < 0) inflight.erase(inflight.begin() + static_cast<long>(i));
      else ++i;
    }

    if (next >= frags.size() && out.size() >= frags.size()) break;
  }
  return out;
}

std::vector<Frag> make_frags(int n, unsigned seed) {
  std::vector<Frag> v;
  uint32_t g = seed;
  auto rnd = [&]() { g = g * 1103515245u + 12345u; return g; };
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
      std::printf("    %-34s v2 retired %zu, fragrob %zu\n", c.name, a.size(),
                  b.size());
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
              c.name, i, (unsigned long long)a[i].ctx, a[i].rgb, a[i].a,
              a[i].has_aux, (unsigned long long)b[i].ctx, b[i].rgb, b[i].a,
              b[i].has_aux);
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

  return zhao::report_and_exit("fragrob_differential");
}
