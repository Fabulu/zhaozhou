// field_dist_svc_directed.cpp — the Field v3 distance-service probe against
// zref::isqrt_u64 + zref::len_of's saturation, plus the probe's ACCEPTANCE
// GATE: measured four-point initiation interval <= 20 clocks
// (reports/Fieldv3.md Phase 3, probe 3).
//
// LAWS:
//   1. THE ROOT IS A FLOOR, exactly zhao_field_isqrt's — perfect squares and
//      their neighbours are where floor and nearest disagree by construction.
//   2. SATURATION IS zref::len_of's: len > INT32_MAX -> INT32_MAX and the
//      per-lane sat flag (the rescale lane), NEVER a wrapped low word.
//   3. REPLIES DRAIN IN ACCEPT ORDER with their own tag: the service reply
//      returning to its issuing requester is a Fieldv3 formal property, and
//      the probe implements it as a 1-bit order FIFO the test hammers.
//   4. NOTHING IS LOST OR DUPLICATED under reply backpressure.
//   5. THE GATE: streaming four-point requests must sustain II <= 20. The
//      probe target comes from the demand model: every committed Earth
//      program binds on this service at 273 x II clocks/association.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "verilated.h"

#include "Vzhao_probe_dist_svc.h"

#include "zhao_sim.hpp"
#include "zref/zref_trig.hpp"

namespace {

using zhao::check;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint64_t next64() {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    uint64_t x = s;
    x ^= x >> 33;
    x *= 0xFF51AFD7ED558CCDULL;
    x ^= x >> 33;
    return x;
  }
  uint32_t below(uint32_t n) { return n ? (uint32_t)(next64() % n) : 0; }
};

uint64_t interesting_n2(Prng& r) {
  switch (r.below(10)) {
    case 0:
      return 0;
    case 1:
      return 1;
    case 2:
      return 2;
    case 3: {  // a perfect square
      const uint64_t k = r.next64() & 0xFFFFFFFFull;
      return k * k;
    }
    case 4: {  // one below a perfect square (floor vs nearest disagree)
      const uint64_t k = (r.next64() & 0xFFFFFFFFull) | 1;
      return k * k - 1;
    }
    case 5: {  // one above
      const uint64_t k = r.next64() & 0xFFFFFFFFull;
      return k * k + 1;
    }
    case 6:
      return 0xFFFFFFFFFFFFFFFFull;  // root 2^32-1: saturates
    case 7:
      return (uint64_t)INT32_MAX * (uint64_t)INT32_MAX;  // root INT32_MAX: no sat
    case 8:
      return ((uint64_t)INT32_MAX + 1) * ((uint64_t)INT32_MAX + 1);  // first sat root
    default:
      return r.next64();
  }
}

struct Want {
  uint32_t len[4];
  bool sat[4];
};

Want oracle(const uint64_t n2[4]) {
  Want w;
  for (int l = 0; l < 4; ++l) {
    const uint64_t len = zref::isqrt_u64(n2[l]);
    w.sat[l] = len > (uint64_t)INT32_MAX;
    w.len[l] = w.sat[l] ? 0x7FFFFFFFu : (uint32_t)len;
  }
  return w;
}

void drive_req(Vzhao_probe_dist_svc& dut, const uint64_t n2[4], uint8_t tag) {
  dut.req_n2_0_i = n2[0];
  dut.req_n2_1_i = n2[1];
  dut.req_n2_2_i = n2[2];
  dut.req_n2_3_i = n2[3];
  dut.req_tag_i = tag;
  dut.req_valid_i = 1;
}

void check_rsp(Vzhao_probe_dist_svc& dut, const Want& w, uint8_t tag, const char* what) {
  const std::string t(what);
  const uint32_t got[4] = {dut.rsp_len_0_o, dut.rsp_len_1_o, dut.rsp_len_2_o, dut.rsp_len_3_o};
  for (int l = 0; l < 4; ++l) {
    check(got[l] == w.len[l], (t + ": lane value").c_str(), w.len[l], got[l]);
    check(((dut.rsp_sat_o >> l) & 1) == (w.sat[l] ? 1 : 0), (t + ": lane sat").c_str(),
          w.sat[l] ? 1 : 0, (dut.rsp_sat_o >> l) & 1);
  }
  check(dut.rsp_tag_o == tag, (t + ": tag").c_str(), tag, dut.rsp_tag_o);
}

/** One lone request through an idle service; returns reply latency. */
int run_one(Vzhao_probe_dist_svc& dut, const uint64_t n2[4], uint8_t tag, const char* what) {
  drive_req(dut, n2, tag);
  dut.rsp_ready_i = 1;
  dut.eval();
  int guard = 0;
  while (!dut.req_ready_o && guard++ < 128) {
    zhao::tick(dut);
    dut.eval();
  }
  zhao::tick(dut);  // accepted
  dut.req_valid_i = 0;
  dut.eval();
  int cycles = 0;
  while (!dut.rsp_valid_o && cycles < 256) {
    zhao::tick(dut);
    dut.eval();
    ++cycles;
  }
  check_rsp(dut, oracle(n2), tag, what);
  zhao::tick(dut);  // reply taken
  dut.eval();
  return cycles;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int random_n = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--random") && i + 1 < argc) random_n = atoi(argv[i + 1]);
  }

  Vzhao_probe_dist_svc dut;
  // local reset: the shared zhao::reset assumes the byte-stream harness ports
  dut.rst_n = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);

  Prng rng(random_n ? 0xD157 + random_n : 0xD157);

  if (random_n == 0) {
    printf("== section 1: directed exactness ==\n");
    {
      const uint64_t zeros[4] = {0, 0, 0, 0};
      run_one(dut, zeros, 0x11, "all-zero group");
      const uint64_t small[4] = {1, 2, 3, 4};
      run_one(dut, small, 0x12, "1..4 (floor at the very bottom)");
      // perfect squares and both neighbours at a mid-size k
      const uint64_t k = 0x12345;
      const uint64_t sq[4] = {k * k, k * k - 1, k * k + 1, (k + 1) * (k + 1) - 1};
      run_one(dut, sq, 0x13, "perfect square and neighbours");
      // the saturation boundary: INT32_MAX root exact, first saturating root,
      // u64 max, and a lane that stays clean — all in ONE group so the
      // per-lane flags cannot be a single OR.
      const uint64_t sat[4] = {(uint64_t)INT32_MAX * (uint64_t)INT32_MAX,
                               ((uint64_t)INT32_MAX + 1) * ((uint64_t)INT32_MAX + 1),
                               0xFFFFFFFFFFFFFFFFull, 100};
      run_one(dut, sat, 0x14, "saturation boundary mixed with clean lanes");
    }

    printf("== section 2: lone-request latency ==\n");
    {
      const uint64_t v[4] = {12345, 67890, 111, 222};
      const int lat = run_one(dut, v, 0x21, "latency probe");
      printf("   measured lone-reply latency: %d cycles\n", lat);
      check(lat <= 40, "lone reply within 40 cycles (32-step root + handshakes)", 40, lat);
    }

    printf("== section 3: II gate — streaming groups ==\n");
    {
      // Stream 32 back-to-back groups, replies drained immediately.
      // II = (last_accept_cycle - first_accept_cycle) / (N-1). THE GATE: <= 20.
      const int N = 32;
      uint64_t pending[N][4];
      Want want[N];
      for (int i = 0; i < N; ++i) {
        for (int l = 0; l < 4; ++l) pending[i][l] = interesting_n2(rng);
        want[i] = oracle(pending[i]);
      }
      dut.rsp_ready_i = 1;
      int accepted = 0, drained = 0, cycle = 0;
      int first_accept = -1, last_accept = -1;
      drive_req(dut, pending[0], (uint8_t)(0x40 + 0));
      dut.eval();
      int guard = 0;
      while (drained < N && guard++ < 20000) {
        const bool fire_req = accepted < N && dut.req_valid_i && dut.req_ready_o;
        const bool fire_rsp = dut.rsp_valid_o != 0;
        if (fire_rsp) {
          char msg[48];
          snprintf(msg, sizeof msg, "stream reply %d", drained);
          check_rsp(dut, want[drained], (uint8_t)(0x40 + drained), msg);
          ++drained;
        }
        zhao::tick(dut);
        ++cycle;
        if (fire_req) {
          if (first_accept < 0) first_accept = cycle;
          last_accept = cycle;
          ++accepted;
          if (accepted < N) {
            drive_req(dut, pending[accepted], (uint8_t)(0x40 + accepted));
          } else {
            dut.req_valid_i = 0;
          }
        }
        dut.eval();
      }
      check(drained == N, "all streamed replies arrived", N, drained);
      const int ii_num = last_accept - first_accept;
      const int ii = (N > 1) ? (ii_num + (N - 2)) / (N - 1) : 0;  // ceil
      printf("   MEASURED four-point II over %d groups: %d clocks (span %d)\n", N, ii, ii_num);
      check(ii <= 20, "THE GATE: four-point DIST2 II <= 20", 20, ii);
    }

    printf("== section 4: reply backpressure — nothing lost, order intact ==\n");
    {
      // With rsp_ready LOW the service legally holds THREE requests: one
      // reply parked in the output register (a skid buffer — the first
      // finished bank drains into it and frees up), plus one finished group
      // held in each bank. The FOURTH offer must be refused for as long as
      // the reply stays blocked, and the three replies must then drain in
      // ACCEPT order with their own tags.
      dut.rsp_ready_i = 0;
      const uint64_t grp[3][4] = {
          {1000, 2000, 3000, 4000}, {5000, 6000, 7000, 8000}, {9000, 10000, 11000, 12000}};
      const uint8_t tags[3] = {0xA1, 0xB2, 0xC3};
      for (int g = 0; g < 3; ++g) {
        drive_req(dut, grp[g], tags[g]);
        dut.eval();
        int guard = 0;
        while (!(dut.req_valid_i && dut.req_ready_o) && guard++ < 200) {
          zhao::tick(dut);
          dut.eval();
        }
        check(guard < 200, "backpressure: request g accepted", 1, guard < 200 ? 1 : 0);
        zhao::tick(dut);
        dut.req_valid_i = 0;
        dut.eval();
      }
      // a fourth offer must be refused while the reply is blocked — give the
      // service ample time to finish every root first
      const uint64_t d[4] = {13, 14, 15, 16};
      for (int i = 0; i < 60; ++i) {
        zhao::tick(dut);
        dut.eval();
      }
      drive_req(dut, d, 0xD4);
      dut.eval();
      int refused_ok = 1;
      for (int i = 0; i < 40; ++i) {
        if (dut.req_ready_o) refused_ok = 0;
        zhao::tick(dut);
        dut.eval();
      }
      dut.req_valid_i = 0;
      dut.eval();
      check(refused_ok == 1, "the fourth request is refused while the reply is blocked", 1,
            refused_ok);
      // release: three replies in accept order with their tags
      dut.rsp_ready_i = 1;
      dut.eval();
      for (int g = 0; g < 3; ++g) {
        int guard = 0;
        while (!dut.rsp_valid_o && guard++ < 64) {
          zhao::tick(dut);
          dut.eval();
        }
        char msg[48];
        snprintf(msg, sizeof msg, "backpressure reply %d (accept order)", g + 1);
        check_rsp(dut, oracle(grp[g]), tags[g], msg);
        zhao::tick(dut);
        dut.eval();
      }
    }
  } else {
    printf("== random differential: %d groups ==\n", random_n);
    for (int i = 0; i < random_n; ++i) {
      uint64_t v[4];
      for (int l = 0; l < 4; ++l) v[l] = interesting_n2(rng);
      char msg[48];
      snprintf(msg, sizeof msg, "random group %d", i);
      run_one(dut, v, (uint8_t)rng.below(256), msg);
    }
  }

  dut.final();
  return zhao::report_and_exit(random_n ? "field_dist_svc_random" : "field_dist_svc_directed");
}
