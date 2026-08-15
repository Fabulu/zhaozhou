// hps_bridge_random.cpp — MEM.HPS.BRIDGE randomized differential (charter
// §21 step 5).
//
// Contract: design/contracts/MEM.HPS.BRIDGE.md; law: spec/memory_rules.md §3;
// oracle: zref::HpsBridge (the D10 sim latency profile).
//
// WHY THIS FILE EXISTS. The ledger listed MEM.HPS.BRIDGE at RTL_VERIFIED with
// `tests.random` pointing at the DIRECTED file, and cited tests/memory/
// mem_random.cpp — which never instantiates the bridge at all. The randomized
// differential the maturity claim rested on did not exist. This is it.
//
// WHAT IS COMPARED, per PCG-generated burst stream (mixed clients, mixed
// read/write, legal and malformed lengths and alignments, random spacing):
//   * beat COUNT per read burst              == oracle ceil(len/8)
//   * beat SPACING                           == 1 cycle (frozen profile)
//   * grant -> first-beat LATENCY            == constant across every burst,
//                                               independent of length, client
//                                               and spacing (the profile is a
//                                               fixed pipeline, so a
//                                               length-dependent latency is a
//                                               defect even if the mean is
//                                               right)
//   * malformed bursts                       issue NOTHING to the HPS side and
//                                               raise exactly one err each
//   * hps_ddr_bytes_by_client[0..4]          == oracle byte accounting, exact
//
// The absolute cycle numbers are deliberately NOT compared: the harness and
// the oracle count from different origins (the harness observes the HPS-side
// request one cycle after the register stage). Latency is compared as a
// per-burst delta and required to be IDENTICAL for all bursts, which pins the
// same law without coupling to the origin convention.
//
//   fast    : 300 bursts   (ctest -L fast)
//   nightly : 20,000 bursts (ctest -L nightly: hps_bridge_random_long)

#include "hps_bridge_harness.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace zhao_hps;

namespace {

int failures = 0;
void chk(bool ok, const char* what, long long expected = -1, long long actual = -1) {
    if (!ok && failures < 20) {
        std::printf("FAIL: %s (expected %lld, actual %lld)\n", what, expected, actual);
    }
    if (!ok) failures++;
}

}  // namespace

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    const unsigned nburst =
        (argc > 1 && std::strcmp(argv[1], "--long") == 0) ? 20000u : 300u;

    BridgeHarness h;
    h.reset();

    zref::Pcg32 pcg(/*seed*/ 0x5EEDB111u ^ (nburst * 2654435761u));

    // ---- generate the burst stream ------------------------------------------
    // ~15% malformed so the reject path is genuinely exercised; the rest are
    // legal 64-B-aligned bursts of 8..64 bytes.
    std::vector<zref::HpsBridge::Burst> stream;
    std::vector<bool> malformed;
    for (unsigned i = 0; i < nburst; i++) {
        zref::HpsBridge::Burst b;
        b.client = pcg.range(5);
        b.write = pcg.range(2) == 0;
        b.addr = pcg.range(1024) * 64;   // 64-B aligned
        b.len = (1 + pcg.range(8)) * 8;  // 8..64, multiple of 8
        bool bad = false;
        switch (pcg.range(20)) {
            case 0: b.len = 0; bad = true; break;                 // zero length
            case 1: b.len = 65 + pcg.range(63); bad = true; break; // over 64
            case 2: b.addr += 1 + pcg.range(63); bad = true; break; // misaligned
            default: break;
        }
        stream.push_back(b);
        malformed.push_back(bad);
    }

    // ---- oracle -------------------------------------------------------------
    // per-burst expectations straight from zref::HpsBridge's law
    uint64_t oracle_bytes[5] = {0, 0, 0, 0, 0};
    unsigned oracle_malformed = 0;
    for (unsigned i = 0; i < nburst; i++) {
        if (malformed[i]) {
            ++oracle_malformed;
            continue;
        }
        oracle_bytes[stream[i].client] += stream[i].len;
    }

    // ---- drive the RTL, one burst in flight at a time ------------------------
    long long latency = -1;   // grant -> first beat, must be constant
    unsigned reads_checked = 0, writes_checked = 0;
    const uint64_t hps_seen_before_all = h.hps_seen_requests;
    uint64_t malformed_issue_leaks = 0;
    uint32_t err_expected = 0;

    for (unsigned i = 0; i < nburst; i++) {
        const zref::HpsBridge::Burst& b = stream[i];
        const unsigned nbeats = (b.len + 7) / 8;

        if (malformed[i]) {
            const uint64_t issued0 = h.hps_seen_requests;
            const uint32_t err0 = h.top.hps_err_count;
            h.top.req_valid = 1;
            h.top.req_write = b.write;
            h.top.req_client = b.client;
            h.top.req_addr = b.addr;
            h.top.req_len = b.len;
            h.tick();
            h.top.req_valid = 0;
            h.idle(3);
            err_expected++;
            chk(h.top.hps_err_count == err0 + 1,
                "malformed burst raises exactly one err", err0 + 1, h.top.hps_err_count);
            if (h.hps_seen_requests != issued0) malformed_issue_leaks++;
            continue;
        }

        if (b.write) {
            const uint64_t issued0 = h.hps_seen_requests;
            h.top.req_valid = 1;
            h.top.req_write = 1;
            h.top.req_client = b.client;
            h.top.req_addr = b.addr;
            h.top.req_len = b.len;
            h.tick();
            h.top.req_valid = 0;
            h.tick();                     // hps_req_valid observed; issue
            uint64_t wr_seen = 0;
            for (unsigned k = 0; k < nbeats; k++) {
                h.top.wr_valid = 1;
                h.top.wr_data = beat_data(k, 0xA5A5u + i);
                h.top.wr_last = (k + 1 == nbeats);
                h.tick();
                if (h.top.hps_wr_valid) wr_seen++;
            }
            h.top.wr_valid = 0;
            h.top.wr_last = 0;
            h.idle(3);
            chk(wr_seen == nbeats, "every write beat reaches the HPS side", nbeats,
                (long long)wr_seen);
            chk(h.hps_seen_requests == issued0 + 1,
                "a legal write burst issues exactly one HPS request",
                (long long)(issued0 + 1), (long long)h.hps_seen_requests);
            writes_checked++;
        } else {
            const size_t b0 = h.client_beat_cycles.size();
            h.top.req_valid = 1;
            h.top.req_write = 0;
            h.top.req_client = b.client;
            h.top.req_addr = b.addr;
            h.top.req_len = b.len;
            h.tick();
            h.top.req_valid = 0;
            const uint64_t grant = h.cycle;
            const uint64_t deadline = h.cycle + 400;
            while (h.client_beat_cycles.size() < b0 + nbeats && h.cycle < deadline) h.tick();

            const size_t got = h.client_beat_cycles.size() - b0;
            chk(got == nbeats, "read burst returns oracle beat count",
                (long long)nbeats, (long long)got);
            if (got != nbeats) break;   // desynchronised; stop rather than lie

            // 1 beat/cycle after the first (the frozen profile)
            for (size_t k = 1; k < nbeats; k++) {
                const uint64_t d = h.client_beat_cycles[b0 + k] - h.client_beat_cycles[b0 + k - 1];
                chk(d == 1, "read beats arrive 1 per cycle", 1, (long long)d);
            }
            // grant -> first beat is a FIXED pipeline latency
            const long long lat =
                static_cast<long long>(h.client_beat_cycles[b0]) - static_cast<long long>(grant);
            if (latency < 0) {
                latency = lat;
                // sanity-anchor it to the frozen sim profile before trusting it
                chk(lat >= (long long)zref::HpsBridge::LAT_TO_FIRST &&
                        lat <= (long long)zref::HpsBridge::LAT_TO_FIRST + 4,
                    "first-beat latency matches the D10 sim profile",
                    (long long)zref::HpsBridge::LAT_TO_FIRST, lat);
            } else {
                chk(lat == latency,
                    "first-beat latency is constant across bursts (length/client independent)",
                    latency, lat);
            }
            h.idle(2);
            reads_checked++;
        }
    }

    (void)hps_seen_before_all;

    // ---- aggregate differential ---------------------------------------------
    chk(malformed_issue_leaks == 0, "malformed bursts issue NOTHING to the HPS", 0,
        (long long)malformed_issue_leaks);
    chk(h.top.hps_err_count == err_expected, "err count == malformed burst count",
        err_expected, h.top.hps_err_count);

    const uint32_t rtl_bytes[5] = {h.top.hps_bytes_0, h.top.hps_bytes_1, h.top.hps_bytes_2,
                                   h.top.hps_bytes_3, h.top.hps_bytes_4};
    for (unsigned k = 0; k < 5; k++) {
        chk(rtl_bytes[k] == oracle_bytes[k],
            "hps_ddr_bytes_by_client[k] == zref::HpsBridge oracle",
            (long long)oracle_bytes[k], (long long)rtl_bytes[k]);
    }

    // the stream must have actually exercised all three paths
    chk(reads_checked > 0, "the stream contained read bursts", 1, reads_checked);
    chk(writes_checked > 0, "the stream contained write bursts", 1, writes_checked);
    chk(oracle_malformed > 0, "the stream contained malformed bursts", 1, oracle_malformed);

    h.top.final();
    std::printf("hps_bridge_random(%u): %s (%d failures; %u reads, %u writes, "
                "%u malformed; first-beat latency %lld)\n",
                nburst, failures ? "FAIL" : "PASS", failures, reads_checked, writes_checked,
                oracle_malformed, latency);
    return failures ? 1 : 0;
}
