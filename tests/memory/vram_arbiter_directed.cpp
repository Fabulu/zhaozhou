// vram_arbiter_directed.cpp — MEM.VRAM.ARBITER directed test (plan W2.5).
//
// Contract: design/contracts/MEM.VRAM.ARBITER.md; law: spec/memory_rules.md
// §2 (D3). Through the chain harness (RTL arbiter+ctrl+model vs the
// zref::VramArbiter + zref::SdramController oracles instantiated by
// zhao_mem_chain.hpp, every cycle):
//   1. scanout PREEMPTS AT A BURST BOUNDARY (never mid-burst)
//   2. strict scanout priority: zero scanout_preempted under mixed load
//   3. RR fairness among {blit, engine0, engine1}
//   4. liveness under load: a fresh guaranteed request is granted its first
//      burst within the bound B (zhao_pkg law) while scanout streams
//      back-to-back; and every pending request completes (no drop)

#include "zhao_mem_chain.hpp"

#include <cstdio>

using namespace zhao_mem;

namespace {
int failures = 0;
void chk(bool ok, const char* what, long long expected = -1, long long actual = -1) {
    if (!ok) {
        failures++;
        std::printf("FAIL: %s (expected %lld, actual %lld)\n", what, expected, actual);
    } else {
        std::printf("ok: %s\n", what);
    }
}
// ZHAO_ARB_LIVENESS_BOUND (RR class, operational: 52 refresh-free + 13 refresh
// steal). Corrected from the never-proven 40 — spec/memory_rules.md §2.1.
// This directed test drives realistic traffic, not the adversarial worst case
// the formal harness constructs, so it normally lands far under B; it is a
// regression net, not the source of the number.
constexpr unsigned B = 65;
constexpr unsigned B_NOREF = 52;  // the formally proven, tight, refresh-free bound
}  // namespace

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    ChainHarness h;

    // ---- 1. preemption at a burst boundary ----------------------------------
    {
        h.reset();
        h.wait_init();
        // blit streams a 64-B request (4 bursts); scanout arrives right
        // after the first blit burst is granted
        h.set_client(1, true, true, 0x0000, 64);
        // step until the first ctrl grant to blit
        while (!h.granted_ctrl()) h.step(h.auto_reqs());
        const uint64_t blit_g0 = h.cycle;
        // scanout wants the same row (read)
        h.set_client(0, true, false, 0x0000, 64);
        // run until scanout's first burst is served
        uint64_t scan_g0 = 0;
        for (uint64_t i = 0; i < 300; i++) {
            h.step(h.auto_reqs());
            if (h.granted(0)) h.set_client(0, false, false, 0, 0);
            if (h.rtl_grants.size() >= 2
                && h.rtl_grants[h.rtl_grants.size() - 1].client == 0) {
                scan_g0 = h.rtl_grants.back().cycle;
                break;
            }
        }
        chk(scan_g0 != 0, "scanout served after arriving");
        chk(scan_g0 > blit_g0, "preemption not mid-burst (after the grant cycle)",
            (long long)blit_g0, (long long)scan_g0);
        chk(scan_g0 - blit_g0 <= 24,
            "preempt at the NEXT burst boundary (span + offer-latch gap)", 24,
            (long long)(scan_g0 - blit_g0));
        // drain
        for (int i = 0; i < 300; i++) {
            h.step(h.auto_reqs());
            if (h.granted(1)) h.set_client(1, false, false, 0, 0);
        }
        chk(h.mismatches == 0, "oracle agrees (preemption phase)", 0, h.mismatches);
    }

    // ---- 2. strict scanout priority: ZERO preemptions under mixed load ------
    {
        h.reset();
        h.wait_init();
        // scanout paced at the line-fetch rate: ONE 16-B burst per 19 cycles
        // (the isochronous Duo budget law — a line is 64 such fetches), blit
        // + engines back-to-back
        uint64_t next_scan = h.cycle + 5;
        uint32_t scan_addr = 0x0000;
        uint32_t blit_addr = 0x4000, e0_addr = 0x8000;
        for (uint64_t i = 0; i < 4000; i++) {
            if (h.cycle >= next_scan) {
                h.set_client(0, true, false, scan_addr, 16);
                scan_addr = (scan_addr + 16) & 0xFFFF;
                next_scan += 19;
            }
            if (!h.req_pending(1)) { h.set_client(1, true, true, blit_addr, 64);
                                     blit_addr = (blit_addr + 64) & 0xFFFF; }
            if (!h.req_pending(2)) { h.set_client(2, true, false, e0_addr, 32);
                                     e0_addr = (e0_addr + 64) & 0xFFFF; }
            h.step(h.auto_reqs());
            for (unsigned k = 0; k < 5; k++)
                if (h.granted(k)) h.set_client(k, false, false, 0, 0);
        }
        for (unsigned k = 0; k < 5; k++) h.set_client(k, false, false, 0, 0);
        chk(h.top.scanout_preempted == 0, "zero scanout starvation boundaries",
            0, h.top.scanout_preempted);
        chk(h.top.model_error == 0, "model timing clean (mixed load)");
        if (h.top.model_error)
            std::printf("  model_err_kind = %02x\n",
                        (unsigned)h.top.model_err_kind);
        chk(h.mismatches == 0, "oracle agrees (mixed load)", 0, h.mismatches);
    }

    // ---- 3. RR fairness among {blit, engine0, engine1} -----------------------
    {
        h.reset();
        h.wait_init();
        // all three guaranteed RR members pending continuously (no scanout)
        size_t g0 = h.rtl_grants.size();
        for (int r = 0; r < 3; r++) {   // three full rotations
            for (unsigned k = 1; k <= 3; k++) {
                h.set_client(k, true, false, 0x1000 * k, 16);
                while (!h.granted_ctrl_to(k)) {
                    h.step(h.auto_reqs());
                    if (h.granted(0) || h.granted(4)) { /* unused ports */ }
                }
                h.set_client(k, false, false, 0, 0);
            }
        }
        // the grant sequence over the RR members must be a strict rotation of
        // {1,2,3} (any starting phase — rr_ptr carries over from earlier)
        bool fair = h.rtl_grants.size() >= g0 + 9;
        if (fair) {
            const unsigned first = h.rtl_grants[g0].client;
            for (size_t i = 0; fair && i < 9; i++) {
                const unsigned want = 1 + ((first - 1 + i) % 3);
                fair = h.rtl_grants[g0 + i].client == want;
            }
        }
        chk(fair, "RR rotation is a strict 1,2,3 cycle");
        // drain
        for (int i = 0; i < 100; i++) h.step(h.auto_reqs());
        chk(h.mismatches == 0, "oracle agrees (RR phase)", 0, h.mismatches);
    }

    // ---- 4. liveness: fresh blit request vs streaming scanout ----------------
    {
        h.reset();
        h.wait_init();
        // scanout streams back-to-back reads (worst-case priority pressure)
        uint32_t saddr = 0;
        uint64_t max_wait = 0;
        uint32_t baddr = 0x4000;
        for (int rep = 0; rep < 40; rep++) {
            if (!h.req_pending(0)) { h.set_client(0, true, false, saddr, 64);
                                     saddr = (saddr + 64) & 0xFFFF; }
            // drop a fresh 64-B blit request (4 bursts) and measure the
            // first-burst grant latency under scanout pressure
            size_t blit_bursts0 = 0;
            for (const auto& g : h.rtl_grants) blit_bursts0 += (g.client == 1);
            h.set_client(1, true, true, baddr, 64);
            baddr = (baddr + 128) & 0xFFFF;
            const uint64_t t0 = h.cycle;
            uint64_t t_first = 0;
            bool served1 = false;
            for (uint64_t w = 0; w < 500 && !served1; w++) {
                if (!h.req_pending(0)) { h.set_client(0, true, false, saddr, 64);
                                         saddr = (saddr + 64) & 0xFFFF; }
                h.step(h.auto_reqs());
                if (h.granted(0)) h.set_client(0, false, false, 0, 0);
                if (h.granted(1)) { h.set_client(1, false, false, 0, 0); t_first = h.cycle; }
                if (h.granted_ctrl_to(1)) served1 = true;
            }
            chk(t_first != 0, "blit request accepted at the port");
            chk(served1, "blit first burst served under scanout pressure");
            if (t_first) {
                const uint64_t wait = t_first - t0 - 1;   // t0 was pre-request
                if (wait > max_wait) max_wait = wait;
            }
            // finish the blit request (no drop): all 4 bursts served
            bool complete = false;
            for (uint64_t w = 0; w < 800 && !complete; w++) {
                if (!h.req_pending(0)) { h.set_client(0, true, false, saddr, 64);
                                         saddr = (saddr + 64) & 0xFFFF; }
                h.step(h.auto_reqs());
                if (h.granted(0)) h.set_client(0, false, false, 0, 0);
                size_t n = 0;
                for (const auto& g : h.rtl_grants) n += (g.client == 1);
                if (n >= blit_bursts0 + 4) complete = true;
            }
            chk(complete, "blit request fully served (no drop)");
        }
        chk(max_wait <= B, "first-burst grant within B=65 under load", B,
            (long long)max_wait);
        // Tighter net: this test's traffic never provokes a refresh inside a
        // wait window, so it must also stay under the refresh-free bound. If
        // this one ever fires while the B check passes, the refresh-steal
        // reasoning in spec/memory_rules.md §2.1 is what needs re-examining.
        chk(max_wait <= B_NOREF, "first-burst grant within the refresh-free 52",
            B_NOREF, (long long)max_wait);
        for (unsigned k = 0; k < 5; k++) h.set_client(k, false, false, 0, 0);
        for (int i = 0; i < 200; i++) h.step(h.auto_reqs());
        chk(h.mismatches == 0, "oracle agrees (liveness phase)", 0, h.mismatches);
        // per-client byte counters agree with the oracle
        chk(h.top.vram_bytes_1 == h.arb_o.bytes_by_client(1),
            "vram_bytes_by_client[blit] == oracle", (long long)h.arb_o.bytes_by_client(1),
            h.top.vram_bytes_1);
        chk(h.top.vram_bytes_0 == h.arb_o.bytes_by_client(0),
            "vram_bytes_by_client[scanout] == oracle",
            (long long)h.arb_o.bytes_by_client(0), h.top.vram_bytes_0);
    }

    h.top.final();
    std::printf("vram_arbiter_directed: %s (%d failures, oracle mismatches %u)\n",
                failures ? "FAIL" : "PASS", failures, h.mismatches);
    zhao::exit_hard(failures ? 1 : 0);  // teardown-deadlock workaround (zhao_sim.hpp)
}
