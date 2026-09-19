// post_fbread_directed.cpp -- POST.COMPOSITE's source reads the back buffer
// back in EXACTLY the order the compositor counts, with the bytes it was stored
// with, and never overruns its own queue.
//
// ---------------------------------------------------------------------------
// WHAT IS CHECKED, AND WHY THESE
// ---------------------------------------------------------------------------
// 1. ORDER AND BYTES. The compositor's `s_*` is addressless: the Nth pixel it
//    takes IS frame pixel (N mod W, N div W). So the only correct output is the
//    view read row-major at `origin + y*stride + 2x`, little-endian halfwords
//    (spec/video_rules.md 3). Every pixel of every pass is compared.
// 2. SHORT LAST REQUEST. A width that is not a multiple of 32 px ends each row
//    with a shorter read; a width that is not a multiple of 4 px cannot be
//    walked without inventing pixels and must be REFUSED, not truncated.
// 3. CREDIT BEFORE REQUEST. With the consumer stalled for thousands of cycles
//    and memory answering instantly, the queue must never overflow
//    (`overflow_o` stays 0) and the request count must be exactly the rows'
//    worth -- a reader that over-fetches and drops passes (1) on a slow memory.
// 4. THE GUARD'S VERDICT IS WAITED FOR. The model answers one cycle late, and
//    later again when the guard is kept busy; a refusal is fatal to the pass.
// 5. DUO IS ONE TALL READ. 256 x 384 at 512 bytes/row (the stored surface).
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_post_fbread.h"

#include "post_mem_model.hpp"
#include "zhao_sim.hpp"

namespace {

uint16_t pattern(uint32_t addr) {
  uint32_t h = addr * 2654435761u;
  h ^= h >> 15;
  return uint16_t(h * 2246822519u >> 16);
}

struct Result {
  std::vector<uint16_t> px;
  bool done = false;
  bool fault = false;
  unsigned reads = 0;
  unsigned overflow = 0;
};

Result run(Vzhao_post_fbread& top, postmem::Model& m, uint32_t origin, unsigned stride,
           unsigned w, unsigned h, unsigned ready_num, unsigned ready_den, uint64_t seed,
           unsigned stall_first = 0) {
  Result r;
  postmem::Pcg pcg(seed);
  top.start_i = 1;
  top.origin_i = origin;
  top.stride_i = stride;
  top.w_i = w;
  top.h_i = h;
  const uint32_t reads0 = top.reads_o;
  const uint64_t limit = uint64_t(w) * h * 40 + 200000 + stall_first;
  for (uint64_t cyc = 0; cyc < limit; ++cyc) {
    const auto d = m.drive();
    top.guard_rsp_i = d.rsp;
    top.beat_valid_i = d.beat_v;
    top.beat_data_i = d.beat_d;
    top.px_ready_i = (cyc >= stall_first) && pcg.chance(ready_num, ready_den);
    top.clk = 0;
    top.eval();
    const auto req = postmem::decode(top.guard_req_o);
    if (top.px_valid_o && top.px_ready_i) r.px.push_back(uint16_t(top.px_rgb_o));
    const bool done_now = top.done_o;
    top.clk = 1;
    top.eval();
    m.edge(d, req, false, 0, false);
    top.start_i = 0;   // one pulse
    if (done_now) { r.done = true; break; }
    if (top.fault_o && top.px_valid_o == 0 && m.rdq.empty() && cyc > 64) break;
  }
  top.start_i = 0;
  for (int i = 0; i < 8; ++i) zhao::tick(top);
  r.fault = top.fault_o;
  r.reads = top.reads_o - reads0;
  r.overflow = top.overflow_o;
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_post_fbread top;

  top.rst_n = 0;
  top.start_i = 0;
  top.px_ready_i = 0;
  top.guard_rsp_i = 0;
  top.beat_valid_i = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  auto fill = [](postmem::Model& m, uint32_t base, uint32_t bytes) {
    for (uint32_t a = base; a < base + bytes; a += 2) m.mem[a] = pattern(a);
  };
  auto expect_pass = [&](const Result& r, const postmem::Model& m, uint32_t origin,
                         unsigned stride, unsigned w, unsigned h, const char* what) {
    unsigned bad = 0;
    const size_t n = size_t(w) * h;
    for (size_t i = 0; i < n && i < r.px.size(); ++i) {
      const uint32_t a = origin + uint32_t(i / w) * stride + uint32_t(i % w) * 2;
      if (r.px[i] != m.rd16(a)) {
        if (bad < 4)
          std::printf("    %s: pixel %zu (x=%zu y=%zu) = %04x, memory %04x\n", what, i, i % w,
                      i / w, r.px[i], m.rd16(a));
        ++bad;
      }
    }
    zhao::check(r.done, what, 1, r.done ? 1 : 0);
    zhao::check(r.px.size() == n, "every pixel of the view, and not one more", n, r.px.size());
    zhao::check(bad == 0, "every pixel is the stored halfword, in raster order", 0, bad);
    zhao::check(!r.fault, "a legal pass raises no fault", 0, r.fault ? 1 : 0);
    zhao::check(r.overflow == 0, "the queue never overflows (credit before request)", 0,
                r.overflow);
  };

  // ---- 1. Z60, fast memory, consumer always ready -------------------------
  {
    postmem::Model m;
    fill(m, 0, 768 * 240);
    const auto r = run(top, m, 0x0, 768, 384, 240, 1, 1, 11);
    expect_pass(r, m, 0x0, 768, 384, 240, "Z60 pass completes");
    zhao::check(r.reads == 240 * 12, "twelve 64-byte reads per 384-pixel row", 240 * 12, r.reads);
    zhao::check(m.violations == 0 && m.reads == r.reads, "every read the guard passed was counted",
                r.reads, m.reads);
  }

  // ---- 2. slot 1, slow hostile memory, consumer stalls at random ----------
  {
    postmem::Model m;
    m.pcg = postmem::Pcg(77);
    m.busy_min = 2;
    m.busy_rand = 9;
    m.read_lat = 9;
    m.read_rand = 25;
    m.beat_gap_num = 1;
    m.beat_gap_den = 3;
    fill(m, 0x02000000, 640 * 240);
    const auto r = run(top, m, 0x02000000, 640, 320, 240, 2, 5, 12);
    expect_pass(r, m, 0x02000000, 640, 320, 240, "Storm pass completes on slot 1 against a slow memory");
    zhao::check(r.reads == 240 * 10, "exactly the rows' worth of reads -- no over-fetch", 240 * 10,
                r.reads);
  }

  // ---- 3. the consumer stops dead while memory answers at once ------------
  {
    postmem::Model m;
    m.read_lat = 1;
    fill(m, 0, 512 * 8);
    const auto r = run(top, m, 0x0, 512, 256, 8, 1, 1, 13, 20000);
    expect_pass(r, m, 0x0, 512, 256, 8, "a pass whose consumer stalls 20,000 cycles still completes");
  }

  // ---- 4. Duo: ONE tall read of the stored 256 x 384 surface --------------
  {
    postmem::Model m;
    m.pcg = postmem::Pcg(5);
    m.read_rand = 7;
    fill(m, 0, 512 * 384);
    const auto r = run(top, m, 0x0, 512, 256, 384, 3, 4, 14);
    expect_pass(r, m, 0x0, 512, 256, 384, "Duo's two views read as one tall surface");
    zhao::check(r.reads == 384 * 8, "eight reads per 256-pixel row", 384 * 8, r.reads);
  }

  // ---- 5. a width with a SHORT last request (100 = 32+32+32+4) -------------
  {
    postmem::Model m;
    fill(m, 0x1000, 256 * 6);
    const auto r = run(top, m, 0x1000, 256, 100, 6, 1, 2, 15);
    expect_pass(r, m, 0x1000, 256, 100, 6, "a 100-pixel row ends in a 4-pixel read");
    zhao::check(r.reads == 6 * 4, "four reads per 100-pixel row, the last of them 8 bytes", 24,
                r.reads);
  }

  // ---- 6. a width that is not a whole beat is REFUSED ----------------------
  {
    postmem::Model m;
    fill(m, 0, 256 * 4);
    const unsigned acc0 = m.accepts;
    const auto r = run(top, m, 0x0, 256, 102, 4, 1, 1, 16);
    zhao::check(r.fault, "a 102-pixel width is refused, not truncated", 1, r.fault ? 1 : 0);
    zhao::check(m.accepts == acc0 && r.px.empty(), "a refused pass issues nothing and emits nothing",
                0, m.accepts - acc0 + r.px.size());
  }

  // ---- 7. a guard refusal is fatal to the pass ------------------------------
  {
    postmem::Model m;
    fill(m, 0, 768 * 4);
    // Refuse the third row's first read.
    m.allow = [](const postmem::Req& q) { return q.addr != 2u * 768u; };
    const auto r = run(top, m, 0x0, 768, 384, 4, 1, 1, 17);
    zhao::check(r.fault, "a refused read latches fault_o", 1, r.fault ? 1 : 0);
    zhao::check(!r.done, "a pass with a refused read never completes", 0, r.done ? 1 : 0);
    zhao::check(m.accepts == 2 * 12 + 1, "no request is issued after the refusal", 25, m.accepts);
    zhao::check(r.px.size() == 2 * 384, "what was read before the refusal still drains", 768,
                r.px.size());
  }

  // ---- 8. and the block recovers on the next pass ---------------------------
  {
    postmem::Model m;
    fill(m, 0, 768 * 2);
    const auto r = run(top, m, 0x0, 768, 384, 2, 1, 1, 18);
    expect_pass(r, m, 0x0, 768, 384, 2, "the pass after a refusal starts clean");
  }

  std::printf("  post_fbread: %u reads, %u pixels, overflow %u\n", top.reads_o, top.pixels_o,
              top.overflow_o);
  return zhao::report_and_exit("post_fbread_directed");
}
