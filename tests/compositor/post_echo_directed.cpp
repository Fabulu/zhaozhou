// post_echo_directed.cpp -- POST.ECHO against zref::post::echo.
//
// Contract: design/contracts/POST.ECHO.md. Every word the RTL writes is
// differenced against the reference address law, and the three things an echo
// can get wrong are each provoked on purpose:
//
//   1. WHERE a pixel lands (views stacked, stride = view width);
//   2. HOW it drops when starved -- whole 16-pixel chunks, never a stall, never
//      a torn burst, and a pass with a drop is never reported complete;
//   3. WHAT it refuses -- a width that is not whole chunks, a refused burst.
//
// The tap is driven at one pixel per clock whatever the memory does. The echo
// has no ready, so nothing here CAN wait for it; what is checked is that
// everything offered is either written at its law address or counted dropped.
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

#include "verilated.h"

#include "Vzhao_post_echo.h"

#include "post_mem_model.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_post.hpp"

namespace echo = zref::post::echo;

namespace {

uint16_t tapval(unsigned view, unsigned x, unsigned y) {
  uint32_t h = (view * 0x9E3779B9u) ^ (x * 2654435761u) ^ (y * 40503u + 0x1234u);
  h ^= h >> 13;
  return uint16_t(h * 2246822519u >> 16);
}

struct PassOut {
  unsigned complete = 0, torn = 0, written = 0, dropped = 0;
  bool fault = false;
};

// One pass: pulse pass_start, offer the whole view, then run until the echo
// settles. PACED means the tap arrives the way the composition delivers it: the
// compositor's output is accepted by a RASTER.FBWRITE identical to the echo's
// own, which takes sixteen pixels and then spends six clocks on the burst
// (request, verdict, four data beats) -- 16 on, 6 off; the bench leaves one
// clock of margin (16 on, 7 off), because a tap paced EXACTLY at the writer's
// best case is a tie that any extra guard-busy clock breaks. UNPACED is one pixel per
// clock forever, faster than any FBWRITE can write, i.e. a starving echo.
PassOut pass(Vzhao_post_echo& top, postmem::Model& m, unsigned view, unsigned w, unsigned h,
             unsigned gap_num, unsigned gap_den, uint64_t seed, bool paced = true) {
  postmem::Pcg pcg(seed);
  const unsigned c0 = top.passes_complete_o, t0 = top.passes_torn_o;
  const unsigned w0 = top.pixels_written_o, d0 = top.pixels_dropped_o;
  unsigned x = 0, y = 0;
  bool tapping = true;
  bool first = true;
  const uint64_t limit = uint64_t(w) * h * 30 + 100000;
  for (uint64_t cyc = 0; cyc < limit; ++cyc) {
    const auto d = m.drive();
    top.guard_rsp_i = d.rsp;
    top.guard_wready_i = d.wready;
    top.retire_words_i = d.credits;
    top.pass_start_i = first;
    top.view_i = view;
    top.w_i = w;
    top.h_i = h;
    const bool paced_off = paced && ((cyc % 23) >= 16);
    const bool offer = !first && tapping && !paced_off && !(gap_num && pcg.chance(gap_num, gap_den));
    top.tap_valid_i = offer;
    top.tap_rgb_i = tapval(view, x, y);
    top.tap_x_i = x;
    top.tap_y_i = y;
    top.clk = 0;
    top.eval();
    const auto req = postmem::decode(top.guard_req_o);
    const bool wv = top.guard_wvalid_o;
    const uint64_t wd = top.guard_wdata_o;
    const bool wl = top.guard_wlast_o;
    top.clk = 1;
    top.eval();
    m.edge(d, req, wv, wd, wl);
    first = false;
    if (offer) {
      if (++x == w) {
        x = 0;
        if (++y == h) tapping = false;
      }
    }
    if (!tapping && !top.busy_o) break;
  }
  top.tap_valid_i = 0;
  top.pass_start_i = 0;
  PassOut o;
  o.complete = top.passes_complete_o - c0;
  o.torn = top.passes_torn_o - t0;
  o.written = top.pixels_written_o - w0;
  o.dropped = top.pixels_dropped_o - d0;
  o.fault = top.fault_o;
  return o;
}

// Every pixel of a view is either at its law address with its tap value, or
// absent as part of a WHOLE absent chunk. Returns {present pixels, absent
// chunks, wrong words, partial chunks}.
struct Audit { unsigned present = 0, absent_chunks = 0, wrong = 0, partial = 0; };
Audit audit(const postmem::Model& m, unsigned view, unsigned w, unsigned h) {
  Audit a;
  for (unsigned y = 0; y < h; ++y) {
    for (unsigned c = 0; c < w / echo::kChunkPx; ++c) {
      unsigned here = 0;
      for (unsigned i = 0; i < echo::kChunkPx; ++i) {
        const unsigned x = c * echo::kChunkPx + i;
        const uint32_t addr = echo::capture_addr(view, x, y, w, h);
        auto it = m.mem.find(addr);
        if (it == m.mem.end()) continue;
        ++here;
        if (it->second != tapval(view, x, y)) ++a.wrong;
      }
      if (here == echo::kChunkPx) a.present += here;
      else if (here == 0) ++a.absent_chunks;
      else ++a.partial;
    }
  }
  return a;
}

// Nothing may be written outside the capture window, nor outside the rows the
// passes named.
unsigned strays(const postmem::Model& m, unsigned views, unsigned w, unsigned h) {
  unsigned n = 0;
  const uint32_t lo = echo::kCaptureBase;
  const uint32_t hi = echo::kCaptureBase + views * h * w * 2;
  for (const auto& kv : m.mem)
    if (kv.first < lo || kv.first >= hi) ++n;
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_post_echo top;

  top.rst_n = 0;
  top.pass_start_i = 0;
  top.tap_valid_i = 0;
  top.guard_rsp_i = 0;
  top.guard_wready_i = 0;
  top.retire_words_i = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  // The window the guard would open: echo writes land only in the capture.
  auto capture_only = [](const postmem::Req& q) {
    return q.write && q.client == 2 && q.addr >= echo::kCaptureBase &&
           q.addr + q.len <= echo::kCaptureBase + echo::kCaptureSpan;
  };

  // ---- 1. Z60, paced, fast memory: every pixel, byte for byte, one capture --
  {
    postmem::Model m;
    m.allow = capture_only;
    const auto o = pass(top, m, 0, 384, 240, 0, 1, 1);
    const auto a = audit(m, 0, 384, 240);
    zhao::check(echo::geometry_ok(384, 240, 1), "the reference accepts Z60", 1, 1);
    zhao::check(a.present == 384u * 240u, "all 92,160 pixels land at capture_addr", 92160, a.present);
    zhao::check(a.wrong == 0 && a.partial == 0, "each one byte-identical to its tap", 0,
                a.wrong + a.partial);
    zhao::check(strays(m, 1, 384, 240) == 0, "nothing lands outside the view's rows", 0,
                strays(m, 1, 384, 240));
    zhao::check(o.complete == 1 && o.torn == 0, "one WHOLE capture reported", 1, o.complete);
    zhao::check(o.dropped == 0 && o.written == 92160, "no drops; every pixel handed to the writer",
                92160, o.written);
    zhao::check(m.bad_write_order == 0 && m.violations == 0, "every data beat belonged to a passed write",
                0, m.bad_write_order + m.violations);
  }

  // ---- 2. Duo: views STACKED -- view 1 lands h rows below view 0 ------------
  {
    postmem::Model m;
    m.allow = capture_only;
    m.pcg = postmem::Pcg(3);
    m.read_lat = 3;   // stacking is the question here, not starvation
    const auto o0 = pass(top, m, 0, 256, 192, 0, 1, 2);
    const auto o1 = pass(top, m, 1, 256, 192, 0, 1, 3);
    const auto a0 = audit(m, 0, 256, 192);
    const auto a1 = audit(m, 1, 256, 192);
    zhao::check(a0.present == 256u * 192u && a1.present == 256u * 192u,
                "both Duo views land whole", 2 * 256 * 192, a0.present + a1.present);
    zhao::check(a0.wrong + a1.wrong + a0.partial + a1.partial == 0,
                "each view at its OWN rows, byte-identical", 0,
                a0.wrong + a1.wrong + a0.partial + a1.partial);
    zhao::check(echo::capture_addr(1, 0, 0, 256, 192) == echo::kCaptureBase + 192u * 512u,
                "view 1 row 0 is capture row 192 (the stacking law)", 1, 1);
    zhao::check(strays(m, 2, 256, 192) == 0, "the stacked image and nothing else", 0,
                strays(m, 2, 256, 192));
    zhao::check(o0.complete == 1 && o1.complete == 1, "two whole captures", 2,
                o0.complete + o1.complete);
  }

  // ---- 3. a starving memory: chunks drop WHOLE, the pass is TORN ------------
  for (uint64_t seed : {101ull, 202ull}) {
    postmem::Model m;
    m.allow = capture_only;
    m.pcg = postmem::Pcg(seed);
    m.busy_min = 3;
    m.busy_rand = 30;
    m.wready_num = 1;
    m.wready_den = 6;
    m.credit_lat = 20;
    const auto o = pass(top, m, 0, 320, 40, 0, 1, seed, /*paced=*/false);
    const auto a = audit(m, 0, 320, 40);
    zhao::check(a.absent_chunks > 0, "the starved memory really did force drops", 1,
                a.absent_chunks > 0 ? 1 : 0);
    zhao::check(a.partial == 0, "never a PARTIAL chunk -- drops are whole chunks", 0, a.partial);
    zhao::check(a.wrong == 0, "every word that landed is its tap, at its law address", 0, a.wrong);
    zhao::check(o.dropped == a.absent_chunks * echo::kChunkPx,
                "dropped == 16 x the chunks absent from memory", a.absent_chunks * 16u, o.dropped);
    zhao::check(o.written == a.present, "written == the pixels present in memory", a.present,
                o.written);
    zhao::check(o.torn == 1 && o.complete == 0, "a pass with a drop is TORN, never complete", 1,
                o.torn);
    zhao::check(!o.fault, "starvation is not a fault", 0, o.fault ? 1 : 0);
  }

  // ---- 4. gaps in the tap (a compositor that stalls) change nothing ---------
  {
    postmem::Model m;
    m.allow = capture_only;
    const auto o = pass(top, m, 0, 256, 16, 1, 3, 4);
    const auto a = audit(m, 0, 256, 16);
    zhao::check(a.present == 256u * 16u && a.wrong == 0, "a gappy tap still lands whole", 4096,
                a.present);
    zhao::check(o.complete == 1, "and is one whole capture", 1, o.complete);
  }

  // ---- 5. a width that is not whole chunks is REFUSED -----------------------
  {
    postmem::Model m;
    m.allow = capture_only;
    const auto o = pass(top, m, 0, 100, 4, 0, 1, 5);
    zhao::check(!echo::geometry_ok(100, 4, 1), "the reference refuses 100 px", 0, 0);
    zhao::check(m.accepts == 0, "a refused pass writes nothing", 0, m.accepts);
    zhao::check(o.dropped == 400 && o.torn == 1 && o.complete == 0,
                "every pixel of it dropped, the pass torn", 400, o.dropped);
  }

  // ---- 5b. w == 0 and h == 0 are REFUSED too (Q003 F3) ----------------------
  // `bad_geom_c` has three terms and only the width-not-whole-chunks one was
  // ever driven. A zero-sized view opens nothing, writes nothing and is a torn
  // pass, so "nothing happened" can never read as a capture.
  for (const auto& wh : {std::pair<unsigned, unsigned>{0u, 8u}, std::pair<unsigned, unsigned>{64u, 0u}}) {
    postmem::Model m;
    m.allow = capture_only;
    const unsigned t0 = top.passes_torn_o, c0 = top.passes_complete_o, w0 = top.pixels_written_o;
    top.pass_start_i = 1;
    top.view_i = 0;
    top.w_i = wh.first;
    top.h_i = wh.second;
    top.tap_valid_i = 0;
    for (int cyc = 0; cyc < 64; ++cyc) {
      const auto d = m.drive();
      top.guard_rsp_i = d.rsp;
      top.guard_wready_i = d.wready;
      top.retire_words_i = d.credits;
      top.clk = 0;
      top.eval();
      const auto req = postmem::decode(top.guard_req_o);
      top.clk = 1;
      top.eval();
      m.edge(d, req, false, 0, false);
      top.pass_start_i = 0;
    }
    zhao::check(!echo::geometry_ok(wh.first, wh.second, 1), "the reference refuses a zero-sized view",
                0, 0);
    zhao::check(top.passes_torn_o - t0 == 1 && top.passes_complete_o == c0,
                wh.first == 0 ? "w == 0 is a torn pass, never complete"
                              : "h == 0 is a torn pass, never complete",
                1, top.passes_torn_o - t0);
    zhao::check(m.accepts == 0 && top.pixels_written_o == w0 && !top.busy_o,
                "and it opens nothing and writes nothing", 0,
                m.accepts + (top.pixels_written_o - w0) + top.busy_o);
  }

  // ---- 5c. a pass_start over a LIVE pass tears it (Q003 F4) -----------------
  // torn_inc_c's second term: a new start while the previous pass is still
  // open. The abandoned pass (view 1, cut after 200 of its 512 pixels) must be
  // counted torn ONCE, and the pass that replaced it must land whole.
  {
    postmem::Model m;
    m.allow = capture_only;
    const unsigned t0 = top.passes_torn_o, c0 = top.passes_complete_o;
    // Pass A: offer 200 pixels and walk away without waiting for it to settle.
    {
      unsigned x = 0, y = 0, offered = 0;
      bool first = true;
      for (int cyc = 0; offered < 200 && cyc < 100000; ++cyc) {
        const auto d = m.drive();
        top.guard_rsp_i = d.rsp;
        top.guard_wready_i = d.wready;
        top.retire_words_i = d.credits;
        top.pass_start_i = first;
        top.view_i = 1;
        top.w_i = 64;
        top.h_i = 8;
        const bool offer = !first && ((cyc % 23) < 16);
        top.tap_valid_i = offer;
        top.tap_rgb_i = tapval(1, x, y);
        top.tap_x_i = x;
        top.tap_y_i = y;
        top.clk = 0;
        top.eval();
        const auto req = postmem::decode(top.guard_req_o);
        const bool wv = top.guard_wvalid_o;
        const uint64_t wd = top.guard_wdata_o;
        const bool wl = top.guard_wlast_o;
        top.clk = 1;
        top.eval();
        m.edge(d, req, wv, wd, wl);
        first = false;
        if (offer) {
          ++offered;
          if (++x == 64) { x = 0; ++y; }
        }
      }
      top.tap_valid_i = 0;
    }
    const bool a_open = top.busy_o;
    // Pass B replaces it.
    const auto ob = pass(top, m, 0, 64, 8, 0, 1, 55);
    const auto ab = audit(m, 0, 64, 8);
    std::printf("    5c: B complete=%u torn=%u written=%u dropped=%u fault=%d | model bad_order=%u viol=%u accepts=%u wbeats=%u\n",
                ob.complete, ob.torn, ob.written, ob.dropped, ob.fault ? 1 : 0, m.bad_write_order,
                m.violations, m.accepts, m.write_beats);
    zhao::check(a_open, "pass A was still open when B started", 1, a_open ? 1 : 0);
    zhao::check(top.passes_torn_o - t0 == 1, "the abandoned pass is counted torn exactly once", 1,
                top.passes_torn_o - t0);
    zhao::check(top.passes_complete_o - c0 == 1 && ob.complete == 1,
                "and the pass that replaced it is one WHOLE capture", 1, ob.complete);
    zhao::check(ab.present == 64u * 8u && ab.wrong == 0 && ab.partial == 0,
                "B lands whole at view 0's rows", 512, ab.present);
    zhao::check(!ob.fault && m.bad_write_order == 0, "no fault, every data beat owned", 0,
                (ob.fault ? 1 : 0) + m.bad_write_order);
  }

  // ---- 6. a refused burst: fault_o latches and the pass is torn -------------
  //      (Late, because FBWRITE's fatal latch is sticky until reset.)
  {
    postmem::Model m;
    m.allow = [](const postmem::Req& q) { return q.addr != echo::kCaptureBase + 64u; };
    const auto o = pass(top, m, 0, 64, 4, 0, 1, 6);
    zhao::check(o.fault, "a refused capture burst latches fault_o", 1, o.fault ? 1 : 0);
    zhao::check(o.torn == 1 && o.complete == 0, "and the pass is torn, not complete", 1, o.torn);
  }

  // ---- 6b. every LATER pass is torn while the fault is latched (Q003 F5) ----
  // POST.ECHO.md: a fault is sticky and no later capture may be called whole.
  // This pass is legal, fully written and fully accepted by the guard.
  {
    postmem::Model m;
    m.allow = capture_only;
    const auto o = pass(top, m, 0, 64, 4, 0, 1, 66);
    zhao::check(o.fault, "the fault is still latched", 1, o.fault ? 1 : 0);
    zhao::check(o.torn == 1 && o.complete == 0,
                "so a clean pass after it is TORN, never complete", 1, o.torn);
  }

  // ---- 7. a geometry that OVERRUNS the capture span (Q003 F1) ---------------
  // The echo has no span check of its own; the contract's row 110 says the
  // GUARD refuses the first burst past the window, which latches fault_o and
  // tears the pass. View 1 of a 384x240 pair starts at capture row 240, and the
  // window holds 320 rows of 768 bytes, so rows 320.. are outside it.
  // Reset first: 6/6b left FBWRITE's fatal latch set.
  {
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
    postmem::Model m;
    m.allow = capture_only;
    zhao::check(!echo::geometry_ok(384, 240, 2), "the reference refuses a 384x240 pair (span)", 0, 0);
    const auto o = pass(top, m, 1, 384, 240, 0, 1, 7);
    zhao::check(m.violations >= 1, "the guard refused the first burst past the window", 1,
                m.violations >= 1 ? 1 : 0);
    zhao::check(o.fault, "fault_o latches", 1, o.fault ? 1 : 0);
    zhao::check(o.torn == 1 && o.complete == 0, "and the pass is TORN", 1, o.torn);
    zhao::check(strays(m, 2, 384, 240) == 0 &&
                    [&] {
                      for (const auto& kv : m.mem)
                        if (kv.first >= echo::kCaptureBase + echo::kCaptureSpan) return false;
                      return true;
                    }(),
                "and nothing lands past the capture window", 0, 0);
  }

  std::printf("  post_echo: complete=%u torn=%u written=%u dropped=%u\n", top.passes_complete_o,
              top.passes_torn_o, top.pixels_written_o, top.pixels_dropped_o);
  return zhao::report_and_exit("post_echo_directed");
}
