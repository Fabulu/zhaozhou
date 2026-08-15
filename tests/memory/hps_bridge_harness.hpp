// hps_bridge_harness.hpp — the shared MEM.HPS.BRIDGE simulation harness.
//
// Extracted from hps_bridge_directed.cpp so the directed test and the
// randomized differential (hps_bridge_random.cpp) drive the SAME harness —
// charter §21 step 5 wants a real randomized differential, not an alias of
// the directed file.
//
// The C++ harness IS the HPS: it answers bursts with the frozen sim latency
// profile (16 gpu cycles request->first beat, 1 beat/cycle after — plan D10)
// on the generic burst port.

#pragma once

#include "Vtb_zhao_hps_bridge.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_mem.hpp"

#include <cstdio>
#include <vector>

using namespace zref;

namespace zhao_hps {
inline uint64_t beat_data(uint32_t beat_idx, uint64_t salt) {
    return (uint64_t(beat_idx) * 0x9E3779B97F4A7C15ull) ^ salt;
}

// the harness-as-HPS: services one outstanding request per the sim profile
struct HpsSim {
    // pending read delivery
    bool delivering = false;
    uint64_t first_beat_cycle = 0;
    unsigned nbeats = 0, beat = 0;
    uint64_t salt = 0;

    void on_request_seen(uint64_t cycle_now, bool write, unsigned len) {
        // observed hps_req_valid this cycle; the profile counts 16 cycles
        // from HERE to the first read beat
        if (!write) {
            delivering = true;
            first_beat_cycle = cycle_now + HpsBridge::LAT_TO_FIRST;
            nbeats = (len + 7) / 8;
            beat = 0;
            salt = cycle_now * 2654435761u;
        }
    }

    // drive the hps-side pins for this cycle (called before the edge)
    void drive(Vtb_zhao_hps_bridge& top, uint64_t cycle_now) {
        top.hps_req_grant = top.hps_req_valid;   // accept immediately
        top.hps_rd_valid = 0;
        top.hps_rd_last = 0;
        top.hps_rd_data = 0;
        if (delivering && cycle_now >= first_beat_cycle && beat < nbeats) {
            top.hps_rd_valid = 1;
            top.hps_rd_data = beat_data(beat, salt);
            top.hps_rd_last = (beat + 1 == nbeats);
            beat++;
            if (beat >= nbeats) delivering = false;
        }
    }
};

struct BridgeHarness {
    Vtb_zhao_hps_bridge top;
    HpsSim hps;
    uint64_t cycle = 0;
    std::vector<uint64_t> client_beat_cycles;   // rsp beat pulses
    std::vector<uint64_t> client_beat_data;
    uint64_t err_pulses = 0;
    uint64_t hps_seen_requests = 0;

    void reset() {
        top.rst_n = 0;
        top.req_valid = 0;
        top.wr_valid = 0;
        top.hps_rd_valid = 0;
        top.frame_tick = 0;
        top.eval();
        for (int i = 0; i < 4; i++) tick();
        top.rst_n = 1;
        top.eval();
        tick();
        cycle = 0;
    }

    void tick() {
        // combinational hps-side reaction first (valid during this cycle)
        if (top.hps_req_valid && !top.hps_req_write) {
            hps.on_request_seen(cycle, false, top.hps_req_len);
            hps_seen_requests++;
        } else if (top.hps_req_valid && top.hps_req_write) {
            hps_seen_requests++;
        }
        hps.drive(top, cycle);
        top.clk = 0;
        top.eval();
        // observe registered client responses DURING this cycle
        if (top.rsp_beat_valid) {
            client_beat_cycles.push_back(cycle);
            client_beat_data.push_back(top.rsp_data);
        }
        if (top.rsp_err) err_pulses++;
        top.clk = 1;
        top.eval();
        top.clk = 0;
        top.eval();
        cycle++;
    }

    // issue a read burst and wait for completion; returns (grant_cycle,
    // first_beat_cycle)
    std::pair<uint64_t, uint64_t> read_burst(unsigned client, uint32_t addr,
                                             unsigned len) {
        const size_t beats0 = client_beat_cycles.size();
        top.req_valid = 1;
        top.req_write = 0;
        top.req_client = client;
        top.req_addr = addr;
        top.req_len = len;
        uint64_t grant_cycle = 0;
        tick();                       // accept at this edge
        top.req_valid = 0;
        grant_cycle = cycle;          // req_grant reads high during cycle+1
        const unsigned nbeats = (len + 7) / 8;
        while (client_beat_cycles.size() < beats0 + nbeats && cycle < grant_cycle + 200)
            tick();
        return {grant_cycle, beats0};
    }

    // issue a write burst: stream beats after grant
    void write_burst(unsigned client, uint32_t addr, unsigned len) {
        top.req_valid = 1;
        top.req_write = 1;
        top.req_client = client;
        top.req_addr = addr;
        top.req_len = len;
        tick();
        top.req_valid = 0;
        const unsigned nbeats = (len + 7) / 8;
        for (unsigned b = 0; b < nbeats; b++) {
            top.wr_valid = 1;
            top.wr_data = beat_data(b, 0xDEAD);
            top.wr_last = (b + 1 == nbeats);
            tick();
        }
        top.wr_valid = 0;
        top.wr_last = 0;
        for (int i = 0; i < 4; i++) tick();
    }

    void idle(uint64_t n) {
        for (uint64_t i = 0; i < n; i++) tick();
    }
};

}  // namespace zhao_hps
