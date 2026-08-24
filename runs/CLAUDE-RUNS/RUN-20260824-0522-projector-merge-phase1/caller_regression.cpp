// caller_regression.cpp -- RUN-20260824-0522, step 3.
//
// EACH CALLER AGAINST ITS OWN PRE-MERGE SELF, EVERY OUTPUT PORT, EVERY CYCLE.
//
// `pair_equivalence.cpp` established that the two shipped projectors compute
// the same projected vertex. It deliberately did NOT compare timing between
// them, because their latencies legitimately differ (36 and 38) and the merge
// must PRESERVE that difference rather than reconcile it.
//
// This file is the comparison that can actually detect the failure the brief
// warns about: **the merge must not quietly change either caller's timing or
// ordering.** So each rewritten block is run beside a verbatim copy of its own
// pre-merge self, recovered from git, and every output port is compared on
// every cycle -- handshake outputs included.
//
// WHY THE STIMULUS DOES NOT DEPEND ON THE DUTs
// -------------------------------------------
// The obvious harness advances a queue pointer on `valid && ready`. That would
// be WRONG here, because `ready` is one of the outputs under test: if the two
// models ever disagreed about `ready`, they would immediately start receiving
// DIFFERENT stimulus, and the comparison would stop being a comparison. It is
// RUN-20260824-0317's failure 1 in a subtler dress -- there the divergence came
// from an RNG called twice, here it would come from feedback.
//
// So the schedule is a pure function of the cycle number: which data word is
// presented, whether `valid` is asserted, whether `ready` is asserted, and
// whether a configuration write happens are all decided in advance and applied
// bit-identically to both models. Neither model can influence what it is fed.
// Protocol-level "correctness" of the stream is irrelevant; what is being
// asserted is that two pieces of hardware given identical inputs produce
// identical outputs on identical cycles.
//
// That also makes the check strictly STRONGER than a protocol-respecting one:
// it exercises cycles a well-behaved driver would never produce -- `valid` held
// through a stall, `valid` dropped mid-handshake, configuration written while
// vertices are in flight -- and any of those is a place a rewritten handshake
// could differ.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_project.h"
#include "Vzhao_geom_project_pre.h"
#include "Vzhao_terrain_project.h"
#include "Vzhao_terrain_project_pre.h"

namespace {

int g_fail = 0;
long g_cyc = 0;
long g_cmp = 0;

template <class T>
void tick(T& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
  d.clk = 0;
  d.eval();
}

void diff(const char* blk, long cyc, const char* port, uint64_t nw, uint64_t pre) {
  ++g_fail;
  if (g_fail <= 24) {
    std::printf("  MISMATCH %s cycle %ld port %s: new=0x%llx pre=0x%llx\n", blk, cyc, port,
                static_cast<unsigned long long>(nw), static_cast<unsigned long long>(pre));
  }
}

#define CMP(blk, cyc, a, b, port)          \
  do {                                     \
    ++g_cmp;                               \
    if ((a).port != (b).port) {            \
      diff(blk, cyc, #port, (uint64_t)(a).port, (uint64_t)(b).port); \
    }                                      \
  } while (0)

// xorshift64*, so the schedule is reproducible and host-independent.
uint64_t g_s = 0x243F6A8885A308D3ull;
uint64_t rnd() {
  g_s ^= g_s >> 12;
  g_s ^= g_s << 25;
  g_s ^= g_s >> 27;
  return g_s * 0x2545F4914F6CDD1Dull;
}
int32_t rnd32() { return static_cast<int32_t>(rnd() >> 32); }

int32_t rnd_world() {
  const uint32_t k = static_cast<uint32_t>(rnd() >> 59);
  if (k == 0) return 0x7FFFFFFF;
  if (k == 1) return static_cast<int32_t>(0x80000000u);
  if (k == 2) return rnd32();
  if (k < 8) return rnd32() >> 8;
  return (rnd32() >> 14) * 256;
}

// One fully-decided cycle of stimulus. Everything a DUT can see is in here, and
// nothing in here can be influenced by a DUT.
struct Beat {
  int32_t w[9];  // nine world words: one vertex for GEOM, one triangle for TERRAIN
  uint16_t src;
  uint8_t view;
  uint8_t mat_a, mat_b, weight;
  bool valid;
  bool ready;
  bool cfg_we;
  uint8_t cfg_view;
  uint8_t cfg_addr;
  uint32_t cfg_data;
};

std::vector<Beat> make_schedule(size_t n) {
  std::vector<Beat> s(n);
  for (size_t i = 0; i < n; ++i) {
    Beat& b = s[i];
    for (int k = 0; k < 9; ++k) b.w[k] = rnd_world();
    b.src = static_cast<uint16_t>(rnd() >> 48);
    b.view = static_cast<uint8_t>((rnd() >> 40) & 1u);
    b.mat_a = static_cast<uint8_t>(rnd() >> 56);
    b.mat_b = static_cast<uint8_t>(rnd() >> 56);
    b.weight = static_cast<uint8_t>(rnd() >> 56);

    // `valid` and `ready` each held for runs rather than tossed per cycle, so
    // long stalls and long bursts both occur.
    b.valid = ((i / (1 + (i % 7))) & 1u) == 0 || (rnd() & 3u) != 0;
    b.ready = ((i / (1 + (i % 5))) & 1u) == 0 || (rnd() & 1u) != 0;

    // Configuration written WHILE vertices are in flight -- the "stale matrix
    // after a reconfiguration" case, and the only place a latched copy of the
    // register file would show. About one cycle in sixteen.
    b.cfg_we = (rnd() & 15u) == 0;
    b.cfg_view = static_cast<uint8_t>((rnd() >> 3) & 1u);
    b.cfg_addr = static_cast<uint8_t>(rnd() % 18u);  // 0..17: matrix + both vp words
    b.cfg_data = static_cast<uint32_t>(rnd() >> 32);
  }
  return s;
}

// ------------------------------------------------------------------- GEOM --
template <class A, class B>
void run_geom(A& nw, B& pre, const std::vector<Beat>& sch) {
  nw.rst_n = 0;
  pre.rst_n = 0;
  nw.cfg_we_i = pre.cfg_we_i = 0;
  nw.v_valid_i = pre.v_valid_i = 0;
  nw.out_ready_i = pre.out_ready_i = 0;
  nw.eval();
  pre.eval();
  for (int i = 0; i < 3; ++i) {
    tick(nw);
    tick(pre);
  }
  nw.rst_n = 1;
  pre.rst_n = 1;

  for (size_t c = 0; c < sch.size(); ++c) {
    const Beat& b = sch[c];
    nw.cfg_we_i = pre.cfg_we_i = b.cfg_we;
    nw.cfg_view_i = pre.cfg_view_i = b.cfg_view;
    nw.cfg_addr_i = pre.cfg_addr_i = b.cfg_addr;
    nw.cfg_data_i = pre.cfg_data_i = b.cfg_data;
    nw.v_valid_i = pre.v_valid_i = b.valid;
    nw.vx_i = pre.vx_i = static_cast<uint32_t>(b.w[0]);
    nw.vy_i = pre.vy_i = static_cast<uint32_t>(b.w[1]);
    nw.vz_i = pre.vz_i = static_cast<uint32_t>(b.w[2]);
    nw.view_i = pre.view_i = b.view;
    nw.src_id_i = pre.src_id_i = b.src;
    nw.out_ready_i = pre.out_ready_i = b.ready;
    nw.eval();
    pre.eval();

    CMP("geom", (long)c, nw, pre, v_ready_o);
    CMP("geom", (long)c, nw, pre, out_valid_o);
    CMP("geom", (long)c, nw, pre, out_x_o);
    CMP("geom", (long)c, nw, pre, out_y_o);
    CMP("geom", (long)c, nw, pre, out_d_o);
    CMP("geom", (long)c, nw, pre, out_behind_o);
    CMP("geom", (long)c, nw, pre, out_src_id_o);
    CMP("geom", (long)c, nw, pre, vertices_transformed_o);

    tick(nw);
    tick(pre);
    ++g_cyc;
  }
}

// ---------------------------------------------------------------- TERRAIN --
template <class A, class B>
void run_terrain(A& nw, B& pre, const std::vector<Beat>& sch) {
  nw.rst_n = 0;
  pre.rst_n = 0;
  nw.cfg_we_i = pre.cfg_we_i = 0;
  nw.tri_valid_i = pre.tri_valid_i = 0;
  nw.out_ready_i = pre.out_ready_i = 0;
  nw.eval();
  pre.eval();
  for (int i = 0; i < 3; ++i) {
    tick(nw);
    tick(pre);
  }
  nw.rst_n = 1;
  pre.rst_n = 1;

  for (size_t c = 0; c < sch.size(); ++c) {
    const Beat& b = sch[c];
    nw.cfg_we_i = pre.cfg_we_i = b.cfg_we;
    nw.cfg_view_i = pre.cfg_view_i = b.cfg_view;
    nw.cfg_addr_i = pre.cfg_addr_i = b.cfg_addr;
    nw.cfg_data_i = pre.cfg_data_i = b.cfg_data;
    nw.tri_valid_i = pre.tri_valid_i = b.valid;
    nw.ax_i = pre.ax_i = static_cast<uint32_t>(b.w[0]);
    nw.ay_i = pre.ay_i = static_cast<uint32_t>(b.w[1]);
    nw.az_i = pre.az_i = static_cast<uint32_t>(b.w[2]);
    nw.bx_i = pre.bx_i = static_cast<uint32_t>(b.w[3]);
    nw.by_i = pre.by_i = static_cast<uint32_t>(b.w[4]);
    nw.bz_i = pre.bz_i = static_cast<uint32_t>(b.w[5]);
    nw.cx_i = pre.cx_i = static_cast<uint32_t>(b.w[6]);
    nw.cy_i = pre.cy_i = static_cast<uint32_t>(b.w[7]);
    nw.cz_i = pre.cz_i = static_cast<uint32_t>(b.w[8]);
    nw.src_id_i = pre.src_id_i = b.src;
    nw.view_i = pre.view_i = b.view;
    nw.mat_a_i = pre.mat_a_i = b.mat_a;
    nw.mat_b_i = pre.mat_b_i = b.mat_b;
    nw.weight_i = pre.weight_i = b.weight;
    nw.out_ready_i = pre.out_ready_i = b.ready;
    nw.eval();
    pre.eval();

    CMP("terrain", (long)c, nw, pre, tri_ready_o);
    CMP("terrain", (long)c, nw, pre, out_valid_o);
    CMP("terrain", (long)c, nw, pre, out_ax_o);
    CMP("terrain", (long)c, nw, pre, out_ay_o);
    CMP("terrain", (long)c, nw, pre, out_bx_o);
    CMP("terrain", (long)c, nw, pre, out_by_o);
    CMP("terrain", (long)c, nw, pre, out_cx_o);
    CMP("terrain", (long)c, nw, pre, out_cy_o);
    CMP("terrain", (long)c, nw, pre, out_behind_o);
    CMP("terrain", (long)c, nw, pre, out_src_id_o);
    CMP("terrain", (long)c, nw, pre, out_ad_o);
    CMP("terrain", (long)c, nw, pre, out_bd_o);
    CMP("terrain", (long)c, nw, pre, out_cd_o);
    CMP("terrain", (long)c, nw, pre, out_view_o);
    CMP("terrain", (long)c, nw, pre, out_mat_a_o);
    CMP("terrain", (long)c, nw, pre, out_mat_b_o);
    CMP("terrain", (long)c, nw, pre, out_weight_o);
    CMP("terrain", (long)c, nw, pre, terrain_triangles_emitted_o);
    CMP("terrain", (long)c, nw, pre, idle_o);

    tick(nw);
    tick(pre);
    ++g_cyc;
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  size_t n = 40000;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--cycles") == 0 && i + 1 < argc)
      n = static_cast<size_t>(std::atol(argv[i + 1]));

  std::printf("caller_regression: each caller against its own pre-merge self, %zu cycles each\n",
              n);

  const std::vector<Beat> sch = make_schedule(n);

  {
    Vzhao_geom_project nw;
    Vzhao_geom_project_pre pre;
    const int before = g_fail;
    run_geom(nw, pre, sch);
    std::printf("  %-46s %s\n", "zhao_geom_project    (all 8 outputs/cycle)",
                g_fail == before ? "cycle-identical" : "*** DIFFERS ***");
  }
  {
    Vzhao_terrain_project nw;
    Vzhao_terrain_project_pre pre;
    const int before = g_fail;
    run_terrain(nw, pre, sch);
    std::printf("  %-46s %s\n", "zhao_terrain_project (all 19 outputs/cycle)",
                g_fail == before ? "cycle-identical" : "*** DIFFERS ***");
  }

  std::printf("----\n%ld cycles driven, %ld port-cycles compared, %d mismatches\n", g_cyc, g_cmp,
              g_fail);
  if (g_fail == 0) {
    std::printf("RESULT: NEITHER CALLER'S TIMING OR ORDERING CHANGED.\n");
  } else {
    std::printf("RESULT: a caller changed. The merge is not transparent.\n");
  }
  std::fflush(nullptr);
  std::_Exit(g_fail == 0 ? 0 : 1);
}
