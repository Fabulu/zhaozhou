// zhao_mem_chain.hpp — shared driver/observer for the W2.5 memory tests.
//
// Wraps Vtb_zhao_mem_chain (arbiter + ctrl + behavioural model) and runs the
// zref::VramArbiter + zref::SdramController oracles in lockstep, one call per
// clock edge, with a 64-KiB shadow memory (spec/memory_rules.md §7). The
// observation convention (identical for RTL and oracle, so traces align):
//
//   iteration i = clock cycle i:
//     1. set client requests valid during cycle i (RTL pins + oracle args)
//     2. eval low: read combinational RTL (the offered ctrl burst, wr_beat)
//     3. oracle steps; its rsp = registered outputs during cycle i
//     4. read RTL registered outputs during cycle i (client grants, ctrl
//        grant/credits, rdata) and compare against the oracle
//     5. supply wdata for a requested write beat, then tick the edge
//
// Write data is a pure function of the word address (word_data), so the
// shadow memory updates at write-burst grant time: the ctrl executes one
// burst at a time, so a later read can only start after the write fully
// retired (the model stores beats at the beat edges).

#pragma once

#include "Vtb_zhao_mem_chain.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_mem.hpp"

#include <array>
#include <cstdio>
#include <type_traits>
#include <vector>

namespace zhao_mem {

inline uint16_t word_data(uint32_t waddr) {
    return uint16_t((waddr * 2654435761u) >> 13);
}

constexpr unsigned SHADOW_WORDS = 32768;   // 64-KiB compare window

struct Shadow {
    std::array<uint16_t, SHADOW_WORDS> w{};
    uint16_t& at(uint32_t waddr) { return w[waddr & (SHADOW_WORDS - 1)]; }
    const uint16_t& at(uint32_t waddr) const { return w[waddr & (SHADOW_WORDS - 1)]; }
};

// ---- wide-port bit helpers (scalar ports <= 64 bits, VlWide above) --------
template <typename Wide>
inline void set_wide_bit(Wide& v, unsigned bit, uint32_t val) {
    if constexpr (std::is_class_v<Wide>) {
        const unsigned word = bit / 32, b = bit % 32;
        if (val) v.m_storage[word] |= (1u << b);
        else     v.m_storage[word] &= ~(1u << b);
    } else {
        if (val) v |= (Wide(1) << bit);
        else     v &= ~(Wide(1) << bit);
    }
}
template <typename Wide>
inline uint32_t get_wide_bit(const Wide& v, unsigned bit) {
    if constexpr (std::is_class_v<Wide>)
        return (v.m_storage[bit / 32] >> (bit % 32)) & 1u;
    else
        return uint32_t((v >> bit) & 1u);
}
template <typename Wide>
inline uint32_t get_wide_field(const Wide& v, unsigned base, unsigned nbits) {
    uint32_t r = 0;
    for (unsigned b = 0; b < nbits; b++) r |= get_wide_bit(v, base + b) << b;
    return r;
}
template <typename Wide>
inline void set_wide_field(Wide& v, unsigned base, unsigned nbits, uint32_t x) {
    for (unsigned b = 0; b < nbits; b++) set_wide_bit(v, base + b, (x >> b) & 1u);
}

struct ChainHarness {
    Vtb_zhao_mem_chain top;
    zref::VramArbiter arb_o;
    zref::SdramController sd_o;
    uint64_t cycle = 0;

    struct Burst {                 // in-flight burst (wdata supply / compare)
        bool active = false;
        bool write = false;
        uint32_t addr = 0;         // burst byte address
        unsigned words = 0;
        unsigned beat = 0;
    } cur;

    Shadow shadow;

    struct Grant {                 // RTL-side ctrl grant trace
        uint64_t cycle;
        unsigned client;
        uint32_t addr;
        unsigned words;
    };
    std::vector<Grant> rtl_grants;

    unsigned mismatches = 0;

    void reset(int cycles = 4) {
        top.rst_n = 0;
        top.c_valid = 0;
        top.c_write = 0;
        top.peek_en = 0;
        top.frame_tick = 0;
        top.wdata = 0;
        top.eval();
        for (int i = 0; i < cycles; i++) tick_raw();
        top.rst_n = 1;
        top.eval();          // combinational settle only: the FIRST post-reset
                             // edge happens inside the first step(), so the
                             // oracles (reset to their own cycle 0 below) and
                             // the RTL advance in lockstep
        arb_o.reset();
        sd_o.reset();
        cycle = 0;
        cur = {};
        rtl_grants.clear();
        mismatches = 0;
        sd_rsp_prev_ = {};
        for (unsigned k = 0; k < 5; k++) req_state_[k] = zref::ArbClientReq{};
        for (unsigned k = 0; k < 5; k++) granted_[k] = false;
        granted_ctrl_ = false;
        granted_ctrl_client_ = 0;
        refresh_pulse_seen_ = false;
    }

    void tick_raw() {
        top.clk = 0;
        top.eval();
        top.clk = 1;
        top.eval();
        top.clk = 0;
        top.eval();
    }

    void set_client(unsigned k, bool valid, bool write, uint32_t addr, unsigned len) {
        if (valid) {
            top.c_valid |= uint8_t(1u << k);
            top.c_write = (top.c_write & ~(1u << k)) | uint8_t((write ? 1u : 0u) << k);
            set_wide_field(top.c_addr, k * 27, 27, addr);
            set_wide_field(top.c_len, k * 7, 7, len);
            req_state_[k].valid = true;
            req_state_[k].write = write;
            req_state_[k].addr = addr;
            req_state_[k].len = len;
        } else {
            top.c_valid &= uint8_t(~(1u << k));
            req_state_[k].valid = false;
        }
    }

    // the oracle request view matching the current RTL pins
    const zref::ArbClientReq* auto_reqs() const { return req_state_; }
    bool req_pending(unsigned k) const { return req_state_[k].valid; }

    // pulses observed DURING the last step() (post-tick reads would miss them)
    bool granted(unsigned k) const { return granted_[k]; }
    bool granted_ctrl() const { return granted_ctrl_; }
    unsigned granted_ctrl_client() const { return granted_ctrl_client_; }
    bool granted_ctrl_to(unsigned k) const {
        return granted_ctrl_ && granted_ctrl_client_ == k;
    }
    bool refresh_pulse_seen() const { return refresh_pulse_seen_; }

    void clear_all_clients() { top.c_valid = 0; }

    // one full observation cycle (see the header comment)
    void step(const zref::ArbClientReq* reqs) {
        for (unsigned k = 0; k < 5; k++) granted_[k] = false;
        granted_ctrl_ = false;
        granted_ctrl_client_ = 0;
        refresh_pulse_seen_ = false;
        top.clk = 0;
        top.eval();                       // combinational view of cycle i

        // oracle phase 1: the offered burst + the registered outputs of the
        // controller DURING this cycle
        const auto& port = arb_o.offer();
        const bool hold = arb_o.hold_now();
        const zref::SdramRsp sd_rsp = sd_o.now();

        // ---- compare: per-client grants + routed credits -------------------
        for (unsigned k = 0; k < 5; k++) {
            const bool g = ((top.c_grant >> k) & 1u) != 0;
            granted_[k] = g;
            if (g != arb_o.grant_now(k)) note("client grant");
            const unsigned rtl_cr = get_wide_field(top.c_credits, k * 8, 8);
            if (rtl_cr != arb_o.credits_now(k, sd_rsp.credits)) note("client credits");
        }

        // ---- compare: aging state (debug-grade, pinpoints divergence) ------
        for (unsigned k = 0; k < 5; k++) {
            const unsigned rtl_age = get_wide_field(top.dbg_age, k * 6, 6);
            if (rtl_age != arb_o.dbg_age(k)) note("age");
        }

        // ---- compare: the offered burst (combinational, this cycle) ---------
        if ((top.ctrl_req_valid != 0) != port.valid) note("offer valid");
        if (port.valid) {
            const unsigned rtl_w = top.ctrl_words == 0 ? 8u : top.ctrl_words;
            if (top.ctrl_addr != port.addr || rtl_w != port.words
                || (top.ctrl_write != 0) != port.write)
                note("offer fields");
        }

        // ---- compare: the SDRAM edge itself --------------------------------
        granted_ctrl_ = (top.ctrl_grant != 0);
        granted_ctrl_client_ = top.ctrl_client;
        if ((top.ctrl_grant != 0) != sd_rsp.grant) note("ctrl grant");
        if (top.ctrl_grant) {
            const unsigned words = top.ctrl_words == 0 ? 8u : top.ctrl_words;
            rtl_grants.push_back(Grant{cycle, top.ctrl_client, top.ctrl_addr, words});
            cur = Burst{true, top.ctrl_write != 0, top.ctrl_addr, words, 0};
            if (top.ctrl_write) {
                for (unsigned b = 0; b < words; b++)
                    shadow.at((top.ctrl_addr >> 1) + b) =
                        word_data((top.ctrl_addr >> 1) + b);
            }
        }

        refresh_pulse_seen_ = (top.refresh_pulse != 0);
        if (refresh_pulse_seen_ != sd_rsp.refresh_pulse) note("refresh pulse");

        // ---- oracle phase 2: advance both edges -----------------------------
        sd_o.edge(zref::SdramReq{port.valid, port.write, port.addr, port.words},
                  hold);
        arb_o.edge(reqs, sd_rsp.grant, sd_rsp.credits);

        // grant trace element-for-element (after the oracle's edge appended)
        if (top.ctrl_grant) {
            const auto& tr = arb_o.trace();
            if (rtl_grants.size() <= tr.size()) {
                const auto& e = tr[rtl_grants.size() - 1];
                if (e.cycle != cycle || e.addr != top.ctrl_addr
                    || e.words != rtl_grants.back().words
                    || e.client != top.ctrl_client)
                    note("grant trace");
            } else {
                note("grant trace length");
            }
        }

        // ---- read data integrity vs the shadow ------------------------------
        if (top.rdata_valid) {
            if (!cur.active || cur.write) note("rdata_valid w/o read burst");
            else {
                const uint32_t waddr = (cur.addr >> 1) + cur.beat;
                if (top.rdata != shadow.at(waddr)) {
                    std::printf("  rdmm: waddr=%u rdata=%04x shadow=%04x "
                                "gen=%04x curaddr=%x beat=%u words=%u modelerr=%d\n",
                                waddr, (unsigned)top.rdata,
                                (unsigned)shadow.at(waddr),
                                (unsigned)word_data(waddr), cur.addr, cur.beat,
                                cur.words, (int)top.model_error);
                    note("read data vs shadow");
                }
                if (++cur.beat >= cur.words) cur.active = false;
            }
        }

        // ---- write beat data supply -----------------------------------------
        if (top.wr_beat) {
            if (!cur.active || !cur.write) note("wr_beat w/o write burst");
            else {
                top.wdata = word_data((cur.addr >> 1) + cur.beat);
                if (++cur.beat >= cur.words) cur.active = false;
            }
        }

        tick_raw();
        cycle++;
    }

    bool wait_init(uint64_t max_cycles = 200) {
        zref::ArbClientReq idle[5];
        while (!top.init_done && cycle < max_cycles) step(idle);
        return top.init_done != 0;
    }

    uint16_t model_peek(uint32_t waddr) {
        top.peek_en = 1;
        top.peek_waddr = waddr;
        top.clk = 0;
        top.eval();
        const uint16_t d = top.peek_data;
        top.peek_en = 0;
        top.clk = 0;
        top.eval();
        return d;
    }

    // drive one client request to completion (holds valid until granted);
    // returns the grant cycle
    uint64_t drive_to_grant(unsigned k, bool write, uint32_t addr, unsigned len,
                            const zref::ArbClientReq (&others)[5],
                            uint64_t max_cycles = 500) {
        zref::ArbClientReq reqs[5];
        for (unsigned i = 0; i < 5; i++) reqs[i] = others[i];
        set_client(k, true, write, addr, len);
        reqs[k].valid = true;
        reqs[k].write = write;
        reqs[k].addr = addr;
        reqs[k].len = len;
        const uint64_t t0 = cycle;
        while (cycle < t0 + max_cycles) {
            step(reqs);
            if ((top.c_grant >> k) & 1u) {
                set_client(k, false, false, 0, 0);
                reqs[k].valid = false;
                return cycle;
            }
        }
        return ~0ull;
    }

    // run `n` idle cycles (no client requests)
    void idle_cycles(uint64_t n) {
        zref::ArbClientReq idle[5];
        for (uint64_t i = 0; i < n; i++) step(idle);
    }

private:
    zref::SdramRsp sd_rsp_prev_{};
    zref::ArbClientReq req_state_[5]{};
    bool granted_[5] = {};
    bool granted_ctrl_ = false;
    unsigned granted_ctrl_client_ = 0;
    bool refresh_pulse_seen_ = false;

    void note(const char* what) {
        mismatches++;
        if (mismatches <= 10)
            std::printf("MISMATCH[%s] at cycle %llu\n", what,
                        (unsigned long long)cycle);
    }
};

}  // namespace zhao_mem
