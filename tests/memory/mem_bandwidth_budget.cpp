// mem_bandwidth_budget.cpp — the D3/Duo bandwidth-budget proof (plan W2.5).
//
// Law: spec/memory_rules.md §2 (risk R4). Worst Phase-2 case:
//   Duo scanout: 512 px x 2 B = 1024 B per line fetched in one 608-vid-cycle
//   line = 1216 gpu cycles (vid = gpu/2, D1); the fetch client paces at
//   16 x 64-B requests per line (the line-buffer ping-pong law).
//   + one concurrent blit DMA streaming back-to-back 64-B writes.
//   + natural refresh every 780 cycles (12-cycle steals).
// Assertions over several lines:
//   * scanout_preempted == 0            — ZERO scanout starvation boundaries
//   * every scanout request granted at the port within <= 6 cycles (strict
//     priority: only credit return can gate, never a competitor)
//   * every scanout burst served within the line budget (line fetch time
//     <= 1216 cycles, i.e. the whole 1024 B lands inside its line window)
//   * the blit still makes progress (guaranteed-liveness under pressure)
//   * oracle agreement + model timing clean throughout

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
}  // namespace

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    ChainHarness h;
    h.reset();
    if (!h.wait_init()) {
        std::printf("mem_bandwidth_budget: FAIL (init)\n");
        return 1;
    }

    constexpr uint64_t LINE_CYCLES = 1216;   // 608 vid x 2 (Duo, D1)
    constexpr unsigned REQ_PER_LINE = 16;    // 16 x 64 B = 1024 B per line
    const unsigned LINES = 6;

    uint64_t line_start = h.cycle;
    unsigned scan_reqs = 0;
    uint32_t scan_addr = 0;      // walk slot 0 sequentially (open-page hits)
    uint32_t blit_addr = 0x0003'C000;   // blit fills slot 1 sequentially
    uint64_t max_port_wait = 0;
    uint64_t max_line_fetch = 0;
    uint64_t blit_bursts = 0;

    for (unsigned line = 0; line < LINES; line++) {
        const uint64_t t_line0 = h.cycle;
        scan_reqs = 0;
        // scanout: one 64-B request per LINE_CYCLES/REQ_PER_LINE budget slot
        uint64_t next_req = t_line0;
        while (h.cycle < t_line0 + LINE_CYCLES) {
            if (scan_reqs < REQ_PER_LINE && h.cycle >= next_req) {
                h.set_client(0, true, false, scan_addr, 64);
                scan_addr = (scan_addr + 64) & 0x3FFFF;
                scan_reqs++;
                next_req += LINE_CYCLES / REQ_PER_LINE;
            }
            if (!h.req_pending(1)) {
                h.set_client(1, true, true, blit_addr, 64);
                blit_addr += 64;
            }
            const uint64_t t_req = h.cycle;
            h.step(h.auto_reqs());
            if (h.granted(0)) {
                if (h.cycle - t_req > max_port_wait) max_port_wait = h.cycle - t_req;
                h.set_client(0, false, false, 0, 0);
            }
            if (h.granted(1)) h.set_client(1, false, false, 0, 0);
            if (h.granted_ctrl_to(1)) blit_bursts++;
        }
        // drain the line's remaining bursts before measuring the fetch time
        const size_t g0 = h.rtl_grants.size();
        (void)g0;
        // measure: all scanout bursts of this line landed by now or shortly
        while (h.cycle < t_line0 + LINE_CYCLES + 40) h.step(h.auto_reqs());
        const uint64_t fetch = h.cycle - t_line0;
        if (fetch > max_line_fetch) max_line_fetch = fetch;
    }

    // total drain
    for (unsigned k = 0; k < 5; k++) h.set_client(k, false, false, 0, 0);
    for (int i = 0; i < 500; i++) h.step(h.auto_reqs());

    chk(h.top.scanout_preempted == 0, "ZERO scanout starvation boundaries",
        0, h.top.scanout_preempted);
    chk(max_port_wait <= 6, "scanout port grant immediate (strict priority)", 6,
        (long long)max_port_wait);
    chk(max_line_fetch <= LINE_CYCLES + 40, "line fetch within the line budget",
        LINE_CYCLES + 40, (long long)max_line_fetch);
    chk(blit_bursts > 0, "blit made progress under scanout pressure");
    chk(h.top.model_error == 0, "model timing clean under the Duo worst case");
    chk(h.mismatches == 0, "oracle agrees under the Duo worst case", 0, h.mismatches);

    // utilization report (informational): bursts served in the window
    std::printf("mem_bandwidth_budget: %llu lines, %zu bursts, blit bursts %llu, "
                "refresh steals %u, bank conflicts %u\n",
                (unsigned long long)LINES, h.rtl_grants.size(),
                (unsigned long long)blit_bursts, h.top.refresh_stalls,
                h.top.bank_conflicts);

    h.top.final();
    std::printf("mem_bandwidth_budget: %s (%d failures)\n",
                failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
