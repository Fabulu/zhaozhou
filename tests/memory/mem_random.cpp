// mem_random.cpp — W2.5 three-way random differential (plan W2.5).
//
// RTL (arbiter + ctrl + behavioural model) vs the zref::VramArbiter +
// zref::SdramController oracles on PCG request streams (spec/memory_rules.md
// §7): every cycle is compared (client grants, credit routing, ctrl grant
// order/addr/words, refresh pulses) plus a 64-KiB shadow-memory integrity
// compare at the end (word-for-word through the model peek port) and
// per-client byte-counter equality.
//
//   fast    : 1,000 requests   (ctest -L fast)
//   nightly : 100,000 requests (ctest -L nightly: mem_random_long)

#include "zhao_mem_chain.hpp"

#include <cstdio>
#include <cstring>

using namespace zhao_mem;

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  const unsigned nreq = (argc > 1 && std::strcmp(argv[1], "--long") == 0) ? 100000u : 1000u;
  ChainHarness h;
  h.reset();
  if (!h.wait_init()) {
    std::printf("mem_random: FAIL (init)\n");
    return 1;
  }

  // PCG request streams: per-client {arrive_cycle, write, addr, len}.
  // Traffic law: scanout READS, blit WRITES, engines mixed; addresses
  // word-aligned inside the 64-KiB compare window.
  struct Sched {
    uint64_t arrive;
    bool write;
    uint32_t addr;
    unsigned len;
  };
  zref::Pcg32 pcg(/*seed*/ 0xC0FFEE12u * nreq);

  struct Gen {
    Sched next;
    bool have = false;
  } gen[5];
  auto make = [&](unsigned k) {
    Sched s;
    s.arrive = h.cycle + 1 + pcg.range(30);
    s.write = (k == 0) ? false : (k == 1 ? true : pcg.range(2) == 0);
    // word-aligned INSIDE the compare window with headroom for one max
    // request (32 words): requests crossing the 64-KiB window boundary
    // would alias in the shadow while the model uses real addresses
    s.addr = (pcg.range(SHADOW_WORDS - 32) * 2) & ~1u;
    s.len = 1 + pcg.range(64);
    if (s.len & 1) s.len = s.len < 64 ? s.len + 1 : 63;  // even lengths
    return s;
  };
  for (unsigned k = 0; k < 5; k++) {
    gen[k].next = make(k);
    gen[k].have = true;
  }

  unsigned issued = 0;
  unsigned grants_seen = 0;
  uint64_t blit_t0_ = 0;

  for (;;) {  // exits via the completion break or the safety valve
    // terminate when everything is done and drained
    bool any_pending = false;
    for (unsigned k = 0; k < 5; k++) any_pending = any_pending || h.req_pending(k);
    // terminate only when every issued request has been granted at the
    // port (the last one may still be waiting for credit returns —
    // bounded by the loop's safety valve below)
    if (issued >= nreq && grants_seen >= issued) {
      for (int i = 0; i < 400; i++) h.step(h.auto_reqs());  // retire
      break;
    }

    // offer new requests whose arrival time has come
    for (unsigned k = 0; k < 5; k++) {
      if (!h.req_pending(k) && gen[k].have && gen[k].next.arrive <= h.cycle && issued < nreq) {
        h.set_client(k, true, gen[k].next.write, gen[k].next.addr, gen[k].next.len);
        gen[k].have = false;
        issued++;
        if (k == 1) blit_t0_ = h.cycle;
      }
    }
    // replenish schedules
    for (unsigned k = 0; k < 5; k++) {
      if (!gen[k].have && issued < nreq) {
        gen[k].next = make(k);
        gen[k].have = true;
      }
    }

    h.step(h.auto_reqs());

    for (unsigned k = 0; k < 5; k++) {
      if (h.granted(k)) {
        grants_seen++;
        h.set_client(k, false, false, 0, 0);
      }
    }
    // safety valve
    if (h.cycle > 200ull * nreq + 100000) {
      std::printf("mem_random: FAIL (stuck at %u/%u)\n", issued, nreq);
      return 1;
    }
  }

  int failures = 0;
  auto chk = [&](bool ok, const char* what, long long e = -1, long long a = -1) {
    if (!ok) {
      failures++;
      std::printf("FAIL: %s (expected %lld, actual %lld)\n", what, e, a);
    }
  };
  chk(h.mismatches == 0, "oracle agreement (grants, credits, trace)", 0, h.mismatches);
  chk(h.top.model_error == 0, "behavioural model timing clean");
  chk(grants_seen == issued, "every request granted at the port", issued, grants_seen);
  chk(h.rtl_grants.size() == h.arb_o.trace().size(), "grant trace lengths equal",
      (long long)h.arb_o.trace().size(), (long long)h.rtl_grants.size());

  // 64-KiB shadow vs the model, word for word
  {
    unsigned bad = 0;
    for (uint32_t w = 0; w < SHADOW_WORDS && bad < 8; w++) {
      const uint16_t m = h.model_peek(w);
      if (m != h.shadow.at(w)) {
        bad++;
        std::printf("shadow mismatch at word %u: model %04x shadow %04x\n", w, m, h.shadow.at(w));
      }
    }
    chk(bad == 0, "64-KiB shadow-memory integrity", 0, bad);
  }

  // per-client byte counters (the D9 shadow latch law is exercised by the
  // frame_tick pulse path inside the arbiter; here we verify the live set)
  for (unsigned k = 0; k < 5; k++) {
    const uint32_t rtl = k == 0   ? h.top.vram_bytes_0
                         : k == 1 ? h.top.vram_bytes_1
                         : k == 2 ? h.top.vram_bytes_2
                         : k == 3 ? h.top.vram_bytes_3
                                  : h.top.vram_bytes_4;
    chk(rtl == h.arb_o.bytes_by_client(k), "vram_bytes_by_client[] == oracle",
        (long long)h.arb_o.bytes_by_client(k), rtl);
  }

  h.top.final();
  std::printf(
      "mem_random(%u): %s (%d failures, oracle mismatches %u, "
      "grants %zu)\n",
      nreq, failures ? "FAIL" : "PASS", failures, h.mismatches, h.rtl_grants.size());
  zhao::exit_hard(failures ? 1 : 0);  // teardown-deadlock workaround (zhao_sim.hpp)
}
