// texture_cache_pipe_directed.cpp — does one line get fetched once, and does
// the consumer's ready still reach the accept line?
//
// ---------------------------------------------------------------------------
// THE TWO GATES
// ---------------------------------------------------------------------------
//   > Four lanes requesting one line cause one fill.
//   > No cache output-ready signal reaches TMU request acceptance in one
//     combinational path.
//
// The first is measurable and unambiguous: point all four taps inside one
// 16-byte line and count fetches. The shipped cache picks the lowest-numbered
// needing lane, fills it, re-checks, and repeats -- four fetches for one line.
// A bilinear footprint on an interior texel has all four taps in one line most
// of the time, so that is 4x the memory traffic in the COMMON case, not a
// corner.
//
// The second is the one a plausible implementation gets wrong. Splitting the
// cache into stages but leaving `acc_ready_o` reading `smp_ready_i` reproduces
// the defect exactly while looking pipelined, so the test stalls the consumer
// permanently and requires accesses to keep being accepted until local storage
// fills -- and no further.
//
// A THIRD GATE, added 2026-09-06 (case 6). Accepting an access is not
// answering it. Case 3 stalls the consumer and checks only that accesses keep
// being TAKEN; a live defect in the response-FIFO capacity check destroyed
// results that were already in flight, and every case in this file ran green
// over it. Case 6 warms a line, stalls, and then checks IDENTITY on the way
// out: every src_id issued comes back exactly once, in order.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_cache_pipe.h"

#include "zhao_sim.hpp"

namespace {

constexpr int REQN = 4;

// A trivial memory: every halfword at byte address a reads back (a >> 1) + 0x100.
uint16_t mem_at(uint32_t byte_addr) {
  return static_cast<uint16_t>(((byte_addr >> 1) + 0x100) & 0xFFFF);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_cache_pipe top;

  // The fill model: 8 halfword beats per line, streamed from `fill_addr_o`.
  int fill_beats_left = 0;
  uint32_t fill_base = 0;
  int fill_beat = 0;

  auto step = [&](bool stall_mem) {
    // offer beats when a fill is outstanding
    top.fill_data_valid_i = 0;
    if (fill_beats_left > 0 && !stall_mem) {
      top.fill_data_valid_i = 1;
      top.fill_data_i = mem_at(fill_base + static_cast<uint32_t>(fill_beat) * 2u);
      ++fill_beat;
      --fill_beats_left;
    }
    top.fill_ready_i = 1;
    top.eval();
    if (top.fill_valid_o && top.fill_ready_i && fill_beats_left == 0 && fill_beat == 0) {
      fill_base = top.fill_addr_o;
      fill_beats_left = 8;
    }
    zhao::tick(top);
    if (fill_beats_left == 0 && fill_beat == 8) fill_beat = 0;
  };

  auto reset = [&]() {
    top.acc_valid_i = 0;
    top.smp_ready_i = 1;
    top.fill_data_valid_i = 0;
    top.fill_ready_i = 1;
    fill_beats_left = 0;
    fill_beat = 0;
    top.rst_n = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // ---- 1: FOUR LANES, ONE LINE -> ONE FILL --------------------------------
  {
    reset();
    // All four taps inside the same 16-byte line (base 0x2000).
    top.acc_valid_i = 1;
    top.acc_en_i = 0xF;
    top.acc_addr_i[0] = 0x2000;
    top.acc_addr_i[1] = 0x2002;
    top.acc_addr_i[2] = 0x2004;
    top.acc_addr_i[3] = 0x2006;
    top.acc_src_id_i = 0x11;
    top.eval();
    zhao::tick(top);
    top.acc_valid_i = 0;

    for (int c = 0; c < 200 && !top.smp_valid_o; ++c) step(false);

    zhao::check(top.fills_o == 1, "four lanes inside ONE line cause exactly ONE line fetch", 1,
                top.fills_o);
    zhao::check(top.smp_valid_o == 1, "and the access completes", 1, top.smp_valid_o);

    // and the data is what the memory held
    bool data_ok = true;
    for (int k = 0; k < 4; ++k) {
      const uint16_t want = mem_at(0x2000 + static_cast<uint32_t>(k) * 2u);
      // smp_data_o is LANES*16 = 64 bits, so Verilator gives one QData -- not
      // a word array. Indexing it like an array compiled as a pointer subscript
      // and failed loudly, which is the good case; a 128-bit port would have
      // been an array and the same code would have silently read lane 2.
      const uint16_t got = static_cast<uint16_t>((top.smp_data_o >> (16 * k)) & 0xFFFFu);
      if (got != want) data_ok = false;
    }
    zhao::check(data_ok, "and every lane reads the byte the memory held", 1, data_ok ? 1 : 0);
    std::printf("  one line, four taps: %u fill(s), %u lane misses\n", top.fills_o,
                top.cache_misses_o);
  }

  // ---- 2: FOUR LANES, FOUR LINES -> FOUR FILLS ----------------------------
  // The multicast must not over-merge: distinct lines still cost distinct
  // fetches, or the cache would be returning one line's data for another's.
  {
    reset();
    top.acc_valid_i = 1;
    top.acc_en_i = 0xF;
    top.acc_addr_i[0] = 0x3000;
    top.acc_addr_i[1] = 0x3010;  // next line
    top.acc_addr_i[2] = 0x3020;
    top.acc_addr_i[3] = 0x3030;
    top.acc_src_id_i = 0x22;
    top.eval();
    zhao::tick(top);
    top.acc_valid_i = 0;

    for (int c = 0; c < 400 && !top.smp_valid_o; ++c) step(false);
    zhao::check(top.fills_o == 4, "four lanes in FOUR different lines cause four fetches", 4,
                top.fills_o);
    std::printf("  four lines, four taps: %u fills\n", top.fills_o);
  }

  // ---- 3: THE ACCEPT LINE IS LOCAL ---------------------------------------
  // Consumer permanently stalled. Accesses must keep being accepted until the
  // local request FIFO fills, and then stop -- not stop immediately.
  {
    reset();
    top.smp_ready_i = 0;  // the consumer never takes anything
    int accepted = 0;
    for (int c = 0; c < 40; ++c) {
      top.acc_valid_i = 1;
      top.acc_en_i = 0xF;
      for (int k = 0; k < 4; ++k) top.acc_addr_i[k] = 0x4000 + static_cast<uint32_t>(k) * 2u;
      top.acc_src_id_i = static_cast<uint16_t>(c);
      top.eval();
      if (top.acc_ready_o) ++accepted;
      step(false);
    }
    top.acc_valid_i = 0;
    zhao::check(accepted >= REQN,
                "with the consumer STALLED, accesses are still accepted into "
                "local storage",
                1, accepted >= REQN ? 1 : 0);
    std::printf("  consumer stalled: %d accesses accepted\n", accepted);
  }

  // ---- 4: random memory stalls must not change the outcome ---------------
  {
    reset();
    top.acc_valid_i = 1;
    top.acc_en_i = 0xF;
    for (int k = 0; k < 4; ++k) top.acc_addr_i[k] = 0x5000 + static_cast<uint32_t>(k) * 2u;
    top.acc_src_id_i = 0x33;
    top.eval();
    zhao::tick(top);
    top.acc_valid_i = 0;

    uint32_t s = 0x777u;
    for (int c = 0; c < 400 && !top.smp_valid_o; ++c) {
      s = s * 1664525u + 1013904223u;
      step(((s >> 16) & 3u) == 0u);  // stall the memory a quarter of the time
    }
    bool data_ok = true;
    for (int k = 0; k < 4; ++k) {
      const uint16_t want = mem_at(0x5000 + static_cast<uint32_t>(k) * 2u);
      // smp_data_o is LANES*16 = 64 bits, so Verilator gives one QData -- not
      // a word array. Indexing it like an array compiled as a pointer subscript
      // and failed loudly, which is the good case; a 128-bit port would have
      // been an array and the same code would have silently read lane 2.
      const uint16_t got = static_cast<uint16_t>((top.smp_data_o >> (16 * k)) & 0xFFFFu);
      if (got != want) data_ok = false;
    }
    zhao::check(top.smp_valid_o == 1 && data_ok,
                "random memory stalls change the timing, not the data", 1,
                (top.smp_valid_o == 1 && data_ok) ? 1 : 0);
  }

  // ---- SUSTAINED ALL-HIT THROUGHPUT: one access a clock -------------------
  //
  // The C0..C4 rebuild put three stages between a request and its answer, and
  // a miss now REWINDS the issue pointer and squashes what is in flight. Both
  // are new ways to lose throughput, and no earlier case here would notice:
  // every one of them measures whether the right bytes come back, not how
  // often they come back.
  //
  // TEXTURE.TMU's II = 2 sample rate rests on this cache taking one access a
  // clock on the hit path. So warm one line, then hammer it with the consumer
  // always ready.
  {
    reset();
    // warm the line: one access, let the fill run
    top.acc_valid_i = 1;
    top.acc_en_i = 0xF;
    top.acc_addr_i[0] = 0x5000;
    top.acc_addr_i[1] = 0x5002;
    top.acc_addr_i[2] = 0x5004;
    top.acc_addr_i[3] = 0x5006;
    top.acc_src_id_i = 0x55;
    top.eval();
    zhao::tick(top);
    top.acc_valid_i = 0;
    for (int c = 0; c < 400 && !top.smp_valid_o; ++c) step(false);
    top.smp_ready_i = 1;
    step(false);

    const uint32_t fills_warm = top.fills_o;
    const uint32_t replays_warm = top.replays_o;

    // now the same line, over and over, nothing in the way
    const int WANT = 256;
    int accepted = 0, answered = 0, clocks = 0;
    for (int c = 0; c < 4000 && answered < WANT; ++c) {
      top.acc_valid_i = (accepted < WANT);
      top.acc_en_i = 0xF;
      top.acc_addr_i[0] = 0x5000;
      top.acc_addr_i[1] = 0x5002;
      top.acc_addr_i[2] = 0x5004;
      top.acc_addr_i[3] = 0x5006;
      top.acc_src_id_i = 0x56;
      top.smp_ready_i = 1;
      top.fill_data_valid_i = 0;
      top.fill_ready_i = 1;
      top.eval();
      if (top.acc_valid_i && top.acc_ready_o) ++accepted;
      if (top.smp_valid_o && top.smp_ready_i) ++answered;
      zhao::tick(top);
      ++clocks;
    }
    top.acc_valid_i = 0;

    zhao::check(answered == WANT, "every warm access was answered", WANT, answered);
    zhao::check(top.fills_o == fills_warm,
                "a warm line is never refetched -- 256 accesses, no new fill", fills_warm,
                top.fills_o);
    zhao::check(top.replays_o == replays_warm, "and nothing is replayed, because nothing missed",
                replays_warm, top.replays_o);
    // The pipeline depth is a fixed cost paid once; after that it is one a
    // clock. Anything near 2x would mean the rebuild bought timing by
    // spending throughput, which is not a trade this block may make.
    zhao::check(clocks <= WANT + 16,
                "256 all-hit accesses complete within 256 clocks plus the "
                "pipeline depth -- the rebuild did not cost throughput",
                1, clocks <= WANT + 16 ? 1 : 0);
    std::printf("  sustained: %d answered in %d clocks (%.2f each)\n", answered, clocks,
                static_cast<double>(clocks) / answered);
  }

  // ---- 6: A WARM WORKING SET, A STALLED CONSUMER, AND NOTHING MAY VANISH --
  //
  // THE DEFECT THIS CATCHES. `zhao_texture_cache_pipe.sv` used to gate both
  // issue and retirement on `rs_room` -- the response FIFO's occupancy TODAY --
  // while C1 and C2 advanced unconditionally (`c2_v <= c1_v`, outside any
  // `if`). So with the FIFO full and two hit results in flight, C2's result was
  // overwritten by C1's and then cleared, with `rq_ip` already past both
  // requests. Every later retire still advanced `rq_rp`, so the vanished
  // requests' slots were freed while a YOUNGER request's `src_id` came back in
  // their place. Not a hang: correct-looking texels under the wrong tag, which
  // is worse.
  //
  //     accepted   0 1 2 3 4 5 6 7
  //     returned   0 1 2 3     6 7      <- 4 and 5 destroyed, silently
  //
  // Verified in reports/ZHAOZHOU-PREFIT-VERIFICATION-AND-REARCHITECT-20260906
  // .txt section 2 and reproduced by a cycle model of the control logic. The
  // repair is the reservation counter (`rs_resv`) in C4.
  //
  // WHY CASE 3 ABOVE DOES NOT CATCH IT, having stalled the consumer already.
  // Two reasons, and both are the point of this case:
  //   * its working set is COLD, so every probe misses, `fb_busy_r` serialises
  //     the pipe, and two HIT results never coexist in C1/C2 with a full FIFO.
  //     The defect needs an all-resident line. So warm one first.
  //   * it asserts only that accesses were ACCEPTED. It never checks that any
  //     of them came back. A test that watches the input side of a block cannot
  //     see the output side lose things.
  // So: warm, stall, push eight accesses with DISTINCT src_ids, release, drain,
  // and check the identities -- not the timing, and not the byte values, which
  // were never what broke.
  {
    reset();

    // Warm line 0x6000 with one access and let the fill complete. Everything
    // after this point must be an all-hit probe; `fills_o` below proves it.
    top.smp_ready_i = 1;
    top.acc_valid_i = 1;
    top.acc_en_i = 0xF;
    for (int k = 0; k < 4; ++k) top.acc_addr_i[k] = 0x6000 + static_cast<uint32_t>(k) * 2u;
    top.acc_src_id_i = 0x60;
    top.eval();
    zhao::tick(top);
    top.acc_valid_i = 0;
    for (int c = 0; c < 400 && !top.smp_valid_o; ++c) step(false);
    zhao::check(top.smp_valid_o == 1, "the warm-up access completed before the stall test", 1,
                top.smp_valid_o);
    step(false);  // smp_ready_i is still 1, so this pops the warm-up response
    const uint32_t fills_warm = top.fills_o;

    // STALL the consumer, then push eight accesses at the now-resident line.
    // Eight is exactly what fits: REQN=4 retire into the response FIFO, freeing
    // four request slots, and then issue must stop. Each carries its own
    // src_id, because identity is the thing under test.
    top.smp_ready_i = 0;
    const int N = 8;
    std::vector<uint16_t> issued;
    std::vector<uint16_t> returned;
    for (int c = 0; c < 200 && static_cast<int>(issued.size()) < N; ++c) {
      const uint16_t id = static_cast<uint16_t>(0x100 + issued.size());
      top.acc_valid_i = 1;
      top.acc_en_i = 0xF;
      for (int k = 0; k < 4; ++k) top.acc_addr_i[k] = 0x6000 + static_cast<uint32_t>(k) * 2u;
      top.acc_src_id_i = id;
      top.fill_data_valid_i = 0;
      top.fill_ready_i = 1;
      top.eval();
      // Only count it when the block actually took it; on a cycle where
      // `acc_ready_o` is low the SAME id is offered again next clock, so the
      // issued sequence stays dense and the expected return order is trivially
      // 0x100, 0x101, ... in order.
      if (top.acc_ready_o) issued.push_back(id);
      zhao::tick(top);
    }
    top.acc_valid_i = 0;

    // RELEASE and drain. Nothing new is offered, so every response from here is
    // one of the eight.
    for (int c = 0; c < 600 && static_cast<int>(returned.size()) < N; ++c) {
      top.acc_valid_i = 0;
      top.smp_ready_i = 1;
      top.fill_data_valid_i = 0;
      top.fill_ready_i = 1;
      top.eval();
      if (top.smp_valid_o) returned.push_back(static_cast<uint16_t>(top.smp_src_id_o));
      zhao::tick(top);
    }

    std::printf("  warm+stalled: issued");
    for (uint16_t id : issued) std::printf(" %03X", id);
    std::printf(" | returned");
    for (uint16_t id : returned) std::printf(" %03X", id);
    std::printf("\n");

    // The case is only meaningful if the burst really was all-hit: a fill here
    // would mean `fb_busy_r` serialised the pipe and the test had quietly
    // become case 3 again.
    zhao::check(top.fills_o == fills_warm,
                "the eight stalled accesses all HIT -- no fill, so C1 and C2 "
                "really did hold two live results at once",
                fills_warm, top.fills_o);
    const int n_issued = static_cast<int>(issued.size());
    const int n_returned = static_cast<int>(returned.size());
    zhao::check(n_issued == N, "eight accesses were accepted behind the stalled consumer", N,
                n_issued);

    // THE ASSERTION THE DEFECT FAILS. Count first -- six of eight came back
    // before the repair -- then identity and order.
    zhao::check(n_returned == n_issued,
                "every access issued behind the stall comes back EXACTLY ONCE -- "
                "a full response FIFO must not destroy a result already in C1/C2",
                n_issued, n_returned);

    bool ids_in_order = (n_returned == n_issued);
    for (int i = 0; i < n_returned && i < n_issued; ++i)
      if (returned[i] != issued[i]) ids_in_order = false;
    zhao::check(ids_in_order,
                "and each src_id returns in ISSUE order -- a younger request's "
                "id must never appear in an older request's place",
                1, ids_in_order ? 1 : 0);

    // Cheap and worth having: the ids are a permutation of nothing, they are
    // the exact set. A duplicate would satisfy a count check on its own.
    bool no_duplicates = true;
    for (int i = 0; i < n_returned; ++i)
      for (int j = i + 1; j < n_returned; ++j)
        if (returned[i] == returned[j]) no_duplicates = false;
    zhao::check(no_duplicates, "and no src_id is answered twice", 1, no_duplicates ? 1 : 0);
  }

  return zhao::report_and_exit("texture_cache_pipe_directed");
}
