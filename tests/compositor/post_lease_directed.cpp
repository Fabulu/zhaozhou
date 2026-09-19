// post_lease_directed.cpp -- POST.COMPOSITE's lease, standalone: the sequencer,
// the ENGINE0 share and the retirement ledger, with the REAL RASTER.FBWRITE on
// its socket (tests/compositor/tb_post_lease_top.sv).
//
// Until this bench the lease was proven only by the console smoke, whose memory
// system is well behaved and whose one frame is Z60. Three review findings
// (Q004 F1/F2/F4) were about behaviour that bench cannot reach -- the view on the
// start cycle of a SECOND pass, a frame admitted under a live pass, and a slow
// credit stream -- so each is provoked here on purpose.
//
// THE MEMORY IS IN ORDER, like the real one. MEM.GUARD answers ready as a level
// and the verdict as a pulse one cycle after the accept; every passed request
// then executes STRICTLY IN VERDICT ORDER (the arbiter and the controller are
// in order), a read returning len/8 packed beats with `last` on the final one,
// a write consuming len/8 beats from the one write queue; and every request,
// READ OR WRITE, returns its words as credits per <=8-word burst, in order --
// which is what `zhao_vram_arbiter` does and what the lease's ledger assumes.
//
// THE COMPOSITOR is a stand-in that transforms every pixel (`src ^ 0x0F0F`) so a
// framebuffer that post never wrote cannot pass for one it did, and taps the
// same value to the echo. It samples `view_o` ON the `pass_start_o` cycle,
// which is when POST.COMPOSITE's `frame_start_i` is.
//
// Cases:
//   1. Duo, then Duo again: the view on each pass_start cycle is 0 then 1,
//      TWICE (Q004 F1: the second pass opened reading view 0, and the frame
//      after a Duo frame opened reading view 1). Framebuffer and capture exact.
//   2. Z60 slice, a hostile memory: exact framebuffer and capture, no fault.
//   3. Credits held back hundreds of clocks: the retire ledger must not wrap
//      (Q004 F4: RQ=4 with no full guard).
//   4. A frame admitted mid-pass: the pass FINISHES, the port changes hands
//      only at S_DONE, and fault_o says so (Q004 F2).
//   5. A frame admitted while ARMED: the arming is abandoned, nothing is read,
//      fault_o says so; the next frame is clean and clears it.
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vtb_post_lease_top.h"

#include "post_mem_model.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_post.hpp"

namespace echo = zref::post::echo;

namespace {

constexpr uint16_t kGrade = 0x0F0F;   // the stand-in compositor's "look"

uint16_t pattern(uint32_t addr) {
  uint32_t h = addr * 2654435761u;
  h ^= h >> 15;
  return uint16_t((h * 2246822519u) >> 16);
}

// ---------------------------------------------------------------------------
// An in-order memory system behind ENGINE0.
// ---------------------------------------------------------------------------
struct OrderedMem {
  // knobs
  unsigned busy_min = 1, busy_rand = 0;
  unsigned read_lat = 6;
  unsigned beat_gap_num = 0, beat_gap_den = 1;
  unsigned wready_num = 1, wready_den = 1;
  unsigned credit_lat = 3;
  std::function<bool(const postmem::Req&)> allow = [](const postmem::Req&) { return true; };
  postmem::Pcg pcg{1};

  std::map<uint32_t, uint16_t> mem;
  uint16_t rd16(uint32_t a) const {
    auto it = mem.find(a);
    return it == mem.end() ? 0 : it->second;
  }

  // evidence
  unsigned accepts = 0, oks = 0, violations = 0, reads = 0, writes = 0;
  unsigned bad_write_order = 0, max_uncredited = 0;

  // state
  unsigned busy = 0;
  int verdict = 0;
  postmem::Req pending{};
  bool pend_v = false;
  struct Job { bool write; uint32_t addr; unsigned len; uint64_t be; unsigned done_beats; unsigned wait; unsigned words_left; };
  static unsigned beats_of(unsigned len) { return (len + 7) / 8; }
  std::deque<Job> jobs;                       // passed, strictly in order
  struct Beat { uint64_t d; bool last; };
  std::deque<Beat> wq;                        // the one write queue (data beats)
  std::deque<unsigned> wexp;                  // beats owed by passed writes, in order
  struct Cr { unsigned words; unsigned wait; bool fin; };
  std::deque<Cr> crq;
  unsigned uncredited = 0;                    // passed requests whose last credit is still owed

  struct Drive { unsigned rsp; bool beat_v; uint64_t beat_d; bool beat_last; bool wready; unsigned credits; };

  Drive drive() {
    Drive d{0, false, 0, false, false, 0};
    if (verdict > 0) d.rsp |= 2u;
    if (verdict < 0) d.rsp |= 1u;
    if (busy == 0) d.rsp |= 4u;
    if (!jobs.empty() && !jobs.front().write && jobs.front().wait == 0 &&
        !(beat_gap_num && pcg.chance(beat_gap_num, beat_gap_den))) {
      const Job& j = jobs.front();
      const uint32_t a = j.addr + 8u * j.done_beats;
      uint64_t v = 0;
      for (unsigned k = 0; k < 4; ++k) v |= uint64_t(rd16(a + 2 * k)) << (16 * k);
      d.beat_v = true;
      d.beat_d = v;
      d.beat_last = (j.done_beats + 1 == beats_of(j.len));
    }
    d.wready = (wq.size() < 16) && pcg.chance(wready_num, wready_den);
    if (!crq.empty() && crq.front().wait == 0) d.credits = crq.front().words;
    return d;
  }

  // One <=8-word burst of the head job has retired: its words come back.
  void retire_burst(Job& j, bool fin) {
    const unsigned w = fin ? j.words_left : (j.words_left < 8 ? j.words_left : 8u);
    j.words_left -= w;
    crq.push_back(Cr{w, credit_lat, fin});
  }

  void edge(const Drive& d, const postmem::Req& req, bool wvalid, uint64_t wdata, bool wlast) {
    verdict = 0;
    const bool accept = req.valid && (d.rsp & 4u);
    if (pend_v) {
      pend_v = false;
      const bool ok = allow(pending);
      verdict = ok ? 1 : -1;
      if (ok) {
        ++oks;
        if (pending.write) {
          ++writes;
          wexp.push_back(beats_of(pending.len));
        } else {
          ++reads;
        }
        jobs.push_back(Job{pending.write, pending.addr, pending.len, pending.be, 0,
                           pending.write ? 0u : read_lat, (pending.len + 1) / 2});
        ++uncredited;
        if (uncredited > max_uncredited) max_uncredited = uncredited;
      } else {
        ++violations;
      }
    }
    if (accept) {
      ++accepts;
      pending = req;
      pend_v = true;
    }
    if (busy) --busy;
    if (accept) busy = busy_min + (busy_rand ? pcg.next() % busy_rand : 0);

    // write data into the one queue, checked against the passed writes' order
    if (wvalid && d.wready) {
      if (wexp.empty()) {
        ++bad_write_order;
      } else {
        const unsigned left = --wexp.front();
        if ((left == 0) != wlast) ++bad_write_order;
        if (left == 0) wexp.pop_front();
      }
      wq.push_back(Beat{wdata, wlast});
    }

    // the head job, strictly in order
    if (!jobs.empty()) {
      Job& j = jobs.front();
      if (!j.write) {
        if (d.beat_v) {
          ++j.done_beats;
          const bool fin = (j.done_beats == beats_of(j.len));
          if (j.done_beats % 2 == 0 || fin) retire_burst(j, fin);
          if (fin) jobs.pop_front();
        } else if (j.wait) {
          --j.wait;
        }
      } else if (!wq.empty()) {
        const Beat b = wq.front();
        wq.pop_front();
        for (unsigned k = 0; k < 4; ++k) {
          const unsigned byte = j.done_beats * 8 + 2 * k;
          if (byte < j.len && ((j.be >> byte) & 3u) == 3u) mem[j.addr + byte] = uint16_t(b.d >> (16 * k));
        }
        ++j.done_beats;
        const bool fin = (j.done_beats == beats_of(j.len));
        if (j.done_beats % 2 == 0 || fin) retire_burst(j, fin);
        if (fin) jobs.pop_front();
      }
    }
    if (d.credits) {
      if (crq.front().fin) --uncredited;
      crq.pop_front();
    }
    for (auto& c : crq) if (c.wait) --c.wait;
  }
};

// ---------------------------------------------------------------------------
// The compositor stand-in.
// ---------------------------------------------------------------------------
struct Px { unsigned x, y; uint16_t rgb; bool last; };
struct Comp {
  unsigned w = 0, h = 0;
  bool accepting = false;
  unsigned n_in = 0;
  std::deque<Px> q;
  std::vector<int> views_at_start;            // view_o sampled ON each pass_start
  unsigned src_num = 1, src_den = 1, out_num = 1, out_den = 1;
  postmem::Pcg pcg{9};
  bool src_ready = false, out_valid = false;

  void pre() {
    src_ready = accepting && q.size() < 64 && pcg.chance(src_num, src_den);
    out_valid = !q.empty() && pcg.chance(out_num, out_den);
  }
};

struct Bench {
  Vtb_post_lease_top& top;
  OrderedMem& m;
  Comp& c;
  uint64_t cyc = 0;

  void drive_mem(const OrderedMem::Drive& d) {
    top.e0_rsp_i = d.rsp;
    top.e0_beat_valid_i = d.beat_v;
    top.e0_beat_data_i = d.beat_d;
    top.e0_beat_last_i = d.beat_last;
    top.e0_wready_i = d.wready;
    top.e0_credits_i = d.credits;
  }

  // One clock. Pulses (admit, frame_end) are the caller's, cleared after.
  void step() {
    const auto d = m.drive();
    drive_mem(d);
    c.pre();
    top.src_ready_i = c.src_ready;
    top.out_valid_i = c.out_valid;
    if (c.out_valid) {
      const Px& p = c.q.front();
      top.out_rgb_i = p.rgb;
      top.out_x_i = p.x;
      top.out_y_i = p.y;
      top.out_last_i = p.last;
      top.echo_valid_i = 1;
      top.echo_rgb_i = p.rgb;
    } else {
      top.out_last_i = 0;
      top.echo_valid_i = 0;
    }
    top.clk = 0;
    top.eval();
    const auto req = postmem::decode(top.e0_req_o);
    const bool wv = top.e0_wvalid_o;
    const uint64_t wd = top.e0_wdata_o;
    const bool wl = top.e0_wlast_o;
    const bool start = top.pass_start_o;
    const int view = top.view_o;
    const bool src_fire = top.src_valid_o && c.src_ready;
    const uint16_t src_rgb = uint16_t(top.src_rgb_o);
    const bool out_fire = c.out_valid && top.out_ready_o;
    top.clk = 1;
    top.eval();
    m.edge(d, req, wv, wd, wl);
    if (out_fire) c.q.pop_front();
    if (src_fire) {
      const unsigned x = c.n_in % c.w, y = c.n_in / c.w;
      ++c.n_in;
      c.q.push_back(Px{x, y, uint16_t(src_rgb ^ kGrade), c.n_in == c.w * c.h});
      if (c.n_in == c.w * c.h) c.accepting = false;
    }
    if (start) {
      c.views_at_start.push_back(view);
      c.n_in = 0;
      c.accepting = true;
    }
    top.frame_admit_i = 0;
    top.frame_end_i = 0;
    ++cyc;
  }

  void run_until(const std::function<bool()>& done, uint64_t limit) {
    for (uint64_t i = 0; i < limit && !done(); ++i) step();
  }
};

void fill_frame(OrderedMem& m, uint32_t base, unsigned stride, unsigned w, unsigned rows) {
  for (unsigned y = 0; y < rows; ++y)
    for (unsigned x = 0; x < w; ++x) m.mem[base + y * stride + 2 * x] = pattern(base + y * stride + 2 * x);
}

struct FrameAudit { unsigned fb_bad = 0, cap_bad = 0; };
FrameAudit audit(const OrderedMem& m, uint32_t base, unsigned stride, unsigned w, unsigned h,
                 unsigned views) {
  FrameAudit a;
  for (unsigned v = 0; v < views; ++v)
    for (unsigned y = 0; y < h; ++y)
      for (unsigned x = 0; x < w; ++x) {
        const uint32_t fa = base + (v * h + y) * stride + 2 * x;
        const uint16_t want = uint16_t(pattern(fa) ^ kGrade);
        if (m.rd16(fa) != want) {
          if (a.fb_bad < 3) std::printf("    fb v%u (%u,%u) = %04x want %04x\n", v, x, y, m.rd16(fa), want);
          ++a.fb_bad;
        }
        if (m.rd16(echo::capture_addr(v, x, y, w, h)) != want) ++a.cap_bad;
      }
  return a;
}

// The window the guard opens: the leased slot, and the capture.
std::function<bool(const postmem::Req&)> window(uint32_t base, uint32_t span) {
  return [base, span](const postmem::Req& q) {
    const bool in_slot = q.addr >= base && q.addr + q.len <= base + span;
    const bool in_cap = q.write && q.addr >= echo::kCaptureBase &&
                        q.addr + q.len <= echo::kCaptureBase + echo::kCaptureSpan;
    return q.client == 2 && (in_slot || in_cap);
  };
}

void reset(Vtb_post_lease_top& top) {
  top.rst_n = 0;
  top.lease_live_i = 0;
  top.frame_admit_i = 0;
  top.frame_end_i = 0;
  top.raster_quiet_i = 0;
  top.src_ready_i = 0;
  top.out_valid_i = 0;
  top.echo_valid_i = 0;
  top.e0_rsp_i = 0;
  top.e0_beat_valid_i = 0;
  top.e0_wready_i = 0;
  top.e0_credits_i = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);
}

// Admit a frame, end it, and run the post phase to S_DONE.
struct FrameOut {
  unsigned frames = 0, passes = 0, echo_complete = 0, echo_torn = 0, unowned = 0;
  bool fault = false, fbw_bad = false, done = false;
};
FrameOut frame(Bench& b, uint32_t base, unsigned stride, unsigned w, unsigned h, bool duo,
               const std::function<void(Bench&)>& mid = nullptr) {
  Vtb_post_lease_top& top = b.top;
  const unsigned f0 = top.frames_o, p0 = top.passes_o, ec0 = top.echo_passes_complete_o,
                 et0 = top.echo_passes_torn_o;
  top.lease_live_i = 1;
  top.fb_base_i = base;
  top.fb_stride_i = stride;
  top.frame_w_i = w;
  top.frame_h_i = h;
  top.duo_i = duo;
  b.c.w = w;
  b.c.h = h;
  top.frame_admit_i = 1;
  b.step();
  for (int i = 0; i < 4; ++i) b.step();
  top.raster_quiet_i = 1;
  top.frame_end_i = 1;
  b.step();
  if (mid) mid(b);
  b.run_until([&] { return top.frames_o != f0 && !top.busy_o; },
              uint64_t(w) * h * (duo ? 2 : 1) * 200 + 200000);
  for (int i = 0; i < 16; ++i) b.step();
  FrameOut o;
  o.frames = top.frames_o - f0;
  o.passes = top.passes_o - p0;
  o.echo_complete = top.echo_passes_complete_o - ec0;
  o.echo_torn = top.echo_passes_torn_o - et0;
  o.unowned = top.retire_unowned_o;
  o.fault = top.fault_o;
  o.fbw_bad = top.fbw_fatal_o || top.fbw_stream_err_o || (top.fbw_issued_words_o != top.fbw_retired_words_o);
  o.done = (o.frames == 1);
  return o;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_post_lease_top top;

  // ---- 1. Duo, twice: the view ON each pass_start cycle ----------------------
  {
    reset(top);
    OrderedMem m;
    Comp c;
    Bench b{top, m, c};
    const uint32_t base = 0x02000000u;
    m.allow = window(base, 0x3C000);
    fill_frame(m, base, 512, 256, 32);
    const auto o1 = frame(b, base, 512, 256, 16, true);
    const auto a1 = audit(m, base, 512, 256, 16, 2);
    fill_frame(m, base, 512, 256, 32);
    const auto o2 = frame(b, base, 512, 256, 16, true);
    const auto a2 = audit(m, base, 512, 256, 16, 2);
    const bool views_ok = c.views_at_start == std::vector<int>{0, 1, 0, 1};
    std::printf("    views sampled on pass_start: [");
    for (size_t i = 0; i < c.views_at_start.size(); ++i)
      std::printf("%s%d", i ? " " : "", c.views_at_start[i]);
    std::printf("]\n");
    zhao::check(views_ok, "the view ON each pass_start cycle is 0,1 then 0,1 (Duo, then Duo again)",
                1, views_ok ? 1 : 0);
    zhao::check(o1.done && o2.done && o1.passes == 2 && o2.passes == 2,
                "two Duo frames, two passes each", 4, o1.passes + o2.passes);
    zhao::check(a1.fb_bad + a2.fb_bad == 0, "both views of both frames written back exact", 0,
                a1.fb_bad + a2.fb_bad);
    zhao::check(a1.cap_bad + a2.cap_bad == 0, "and captured at their stacked rows", 0,
                a1.cap_bad + a2.cap_bad);
    zhao::check(o2.echo_complete == 2 && o2.echo_torn == 0 && o1.echo_complete == 2,
                "every echo pass WHOLE", 4, o1.echo_complete + o2.echo_complete);
    zhao::check(o2.unowned == 0 && !o2.fault && !o2.fbw_bad && m.bad_write_order == 0 &&
                    m.violations == 0,
                "no unowned credit, no fault, FBWRITE drained, every data beat ordered", 0,
                o2.unowned + (o2.fault ? 1 : 0) + (o2.fbw_bad ? 1 : 0) + m.bad_write_order +
                    m.violations);
  }

  // ---- 2. a Z60 slice against a hostile (but in-order) memory ---------------
  {
    reset(top);
    OrderedMem m;
    m.pcg = postmem::Pcg(21);
    m.busy_min = 2;
    m.busy_rand = 7;
    m.read_lat = 11;
    m.beat_gap_num = 1;
    m.beat_gap_den = 4;
    m.wready_num = 2;
    m.wready_den = 3;
    m.credit_lat = 9;
    Comp c;
    c.src_num = 3;
    c.src_den = 4;
    c.out_num = 4;
    c.out_den = 5;
    Bench b{top, m, c};
    m.allow = window(0, 0x3C000);
    fill_frame(m, 0, 768, 384, 24);
    const auto o = frame(b, 0, 768, 384, 24, false);
    const auto a = audit(m, 0, 768, 384, 24, 1);
    zhao::check(o.done && o.passes == 1, "a Z60 slice completes against a hostile memory", 1, o.passes);
    zhao::check(a.fb_bad == 0 && a.cap_bad == 0, "framebuffer and capture exact", 0, a.fb_bad + a.cap_bad);
    zhao::check(c.views_at_start == std::vector<int>{0}, "one pass, view 0", 1,
                c.views_at_start.size());
    zhao::check(o.unowned == 0 && !o.fault && !o.fbw_bad, "no unowned credit, no fault, drained", 0,
                o.unowned + (o.fault ? 1 : 0) + (o.fbw_bad ? 1 : 0));
    zhao::check(top.src_reads_o == 24u * 12u, "twelve reads per row", 24 * 12, top.src_reads_o);
  }

  // ---- 3. credits held back: the retire ledger must not wrap ----------------
  {
    reset(top);
    OrderedMem m;
    m.pcg = postmem::Pcg(31);
    m.credit_lat = 400;
    Comp c;
    Bench b{top, m, c};
    m.allow = window(0, 0x3C000);
    fill_frame(m, 0, 768, 384, 16);
    const auto o = frame(b, 0, 768, 384, 16, false);
    const auto a = audit(m, 0, 768, 384, 16, 1);
    std::printf("    most passed requests awaiting credits at once: %u\n", m.max_uncredited);
    zhao::check(o.done, "the pass completes with credits 400 clocks late", 1, o.done ? 1 : 0);
    zhao::check(o.unowned == 0, "no credit is attributed to the wrong requester", 0, o.unowned);
    zhao::check(!o.fbw_bad && o.echo_complete == 1 && o.echo_torn == 0,
                "FBWRITE drains and the echo settles WHOLE", 1, o.echo_complete);
    zhao::check(a.fb_bad == 0 && a.cap_bad == 0, "framebuffer and capture exact", 0, a.fb_bad + a.cap_bad);
  }

  // ---- 4. a frame admitted MID-PASS -----------------------------------------
  {
    reset(top);
    OrderedMem m;
    Comp c;
    Bench b{top, m, c};
    m.allow = window(0, 0x3C000);
    fill_frame(m, 0, 768, 384, 16);
    bool phase_dropped_early = false;
    const auto o = frame(b, 0, 768, 384, 16, false, [&](Bench& bb) {
      // run into the pass, then admit
      bb.run_until([&] { return bb.top.src_pixels_o > 2000; }, 200000);
      bb.top.frame_admit_i = 1;
      bb.step();
      // the port must stay post's until the pass is DONE
      bb.run_until([&] {
        if (!bb.top.phase_post_o && bb.top.frames_o == 0) phase_dropped_early = true;
        return bb.top.frames_o != 0;
      }, 400000);
    });
    const auto a = audit(m, 0, 768, 384, 16, 1);
    zhao::check(o.done && o.passes == 1, "the interrupted pass still FINISHES", 1, o.passes);
    zhao::check(!phase_dropped_early, "FBWRITE's port is not handed back before S_DONE", 0,
                phase_dropped_early ? 1 : 0);
    zhao::check(a.fb_bad == 0 && a.cap_bad == 0, "and writes the whole frame back exact", 0,
                a.fb_bad + a.cap_bad);
    zhao::check(o.fault, "fault_o says the frame was admitted under a live pass", 1, o.fault ? 1 : 0);
    zhao::check(!top.phase_post_o && !top.busy_o, "the held admit is taken at S_DONE", 0,
                top.phase_post_o + top.busy_o);
    zhao::check(o.unowned == 0 && !o.fbw_bad, "the ledger and FBWRITE are clean", 0,
                o.unowned + (o.fbw_bad ? 1 : 0));
  }

  // ---- 5. a frame admitted while ARMED, then a clean frame -------------------
  {
    reset(top);
    OrderedMem m;
    Comp c;
    Bench b{top, m, c};
    m.allow = window(0, 0x3C000);
    fill_frame(m, 0, 768, 384, 8);
    top.lease_live_i = 1;
    top.fb_base_i = 0;
    top.fb_stride_i = 768;
    top.frame_w_i = 384;
    top.frame_h_i = 8;
    top.duo_i = 0;
    c.w = 384;
    c.h = 8;
    top.frame_admit_i = 1;
    b.step();
    top.raster_quiet_i = 0;   // the raster has not drained: post stays ARMED
    top.frame_end_i = 1;
    b.step();
    for (int i = 0; i < 20; ++i) b.step();
    const bool armed = top.busy_o;
    top.frame_admit_i = 1;
    b.step();
    for (int i = 0; i < 20; ++i) b.step();
    zhao::check(armed, "post was ARMED (busy) waiting for the raster", 1, armed ? 1 : 0);
    zhao::check(!top.busy_o && top.src_reads_o == 0 && c.views_at_start.empty(),
                "an admit while ARMED abandons it: no pass, nothing read", 0,
                top.busy_o + top.src_reads_o + c.views_at_start.size());
    zhao::check(top.fault_o, "and fault_o says so", 1, top.fault_o);
    const auto o = frame(b, 0, 768, 384, 8, false);
    const auto a = audit(m, 0, 768, 384, 8, 1);
    zhao::check(o.done && !o.fault && a.fb_bad == 0, "the next frame is clean and clears the flag", 0,
                (o.done ? 0 : 1) + (o.fault ? 1 : 0) + a.fb_bad);
  }

  return zhao::report_and_exit("post_lease_directed");
}
