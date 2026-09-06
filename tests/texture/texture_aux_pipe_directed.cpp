// texture_aux_pipe_directed.cpp — does AUX accept every clock, does a
// degenerate envelope still keep its place in line, and — the 2026-09-06
// addition — does the block survive a sink that says no?
//
// ---------------------------------------------------------------------------
// THE REQUIRED RESULTS, FROM THE BRIEF
// ---------------------------------------------------------------------------
//   > AUX acceptance II = 1
//   > Surface Sheet request II = 1
//   > AUX response II = 1 when the sheet is unstalled
//
// The shipped FSM does three quotient bits, three more, one sheet read, waits,
// presents, and only then accepts again -- about 277,778/frame against a
// 276,480 estimate. A block sized at 1.005x its own demand has no reserve, so
// the throughput here is the whole point and is asserted rather than described.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE GREW FOUR PHASES (2026-09-06)
// ---------------------------------------------------------------------------
// reports/ZHAOZHOU-PREFIT-VERIFICATION-AND-REARCHITECT-20260906.txt section 4:
//
//   > There is no sheet backpressure anywhere in the suite:
//   > texture_aux_pipe_directed ties sheet_ready_i and out_ready_i high for
//   > the entire run.
//
// That was exactly true, and it is the reason three verified defects lived in
// this block while every check in this file passed. A fixture that never says
// no cannot see a machine that cannot be told no:
//
//   * the sheet offer was a one-cycle shadow of the divider's output, so a
//     stalled request was LOST rather than delayed -- invisible when ready is
//     nailed high, because the offer was always taken on its only cycle;
//   * the return queue pushed unconditionally with a 4-bit count for an
//     8-deep queue, so it could wrap to zero holding data -- invisible when
//     the consumer drains every cycle;
//   * req_ready_o was constant 1 -- trivially "correct" when nothing downstream
//     ever refuses.
//
// So the phases below are organised by WHO SAYS NO, and every one of them
// checks the two ready/valid stability laws from outside the module. The
// module carries the same invariants as assertions; Verilator only elaborates
// those under --assert, which this target does not pass, so THESE are the
// checks with evidence behind them.
//
//   A  nobody says no          the original test, kept whole
//   B  the SHEET says no       28 clocks, far longer than the 6-clock divider
//   C  the sheet is SLOW       4-clock responses + a duty-cycled ready, so a
//                              response lands while a degenerate is at the
//                              offer head
//   D  the CONSUMER says no    45 clocks, which must exhaust the credit and
//                              must NOT drop out_valid_o on a queue with data
//
// ---------------------------------------------------------------------------
// THE ORDERING CLAIM, CORRECTED
// ---------------------------------------------------------------------------
// This file used to say, and the RTL used to say, that degenerate requests
// "enter the return queue in the same order". THEY DO NOT, and phase C now
// MEASURES the reordering rather than leaving the correction as prose: a
// degenerate completes on the cycle it reaches the offer head, while a
// non-degenerate that has already been issued waits an unbounded sheet round
// trip. With a one-clock sheet the two happen to coincide and order is
// preserved -- which is precisely why the old fixture never noticed. Phase A
// still requires exact submission order, because with THAT fixture it is a
// real property; phase C requires only that every request returns exactly once
// with its own payload, and asserts that the reordering is actually observed,
// so the corrected claim has a firing detector rather than a comment.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_aux_pipe.h"

#include "zhao_sim.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

struct Req {
  int32_t wx, wz, x0, x1, z0, z1;
  uint8_t tok;
  bool degen;
};

// The sheet's answer for a token, so a queue that overwrites an entry shows up
// as a WRONG PAYLOAD and not merely as a wrong count. A count-only check is
// how a return queue with no reservation passed for weeks.
uint8_t want_tag(uint8_t tok) { return static_cast<uint8_t>(0x40 + tok); }
uint8_t want_str(uint8_t tok) { return static_cast<uint8_t>(0x80 + tok); }

struct Cfg {
  int sheet_latency = 1;       // clocks from accepted read to sheet_rvalid_i
  int sr_lo_from = -1;         // sheet_ready_i forced low on [from, to)
  int sr_lo_to = -1;
  int sr_duty_hi = 0;          // and/or a repeating duty: hi clocks high,
  int sr_duty_lo = 0;          //   lo clocks low. lo == 0 disables it.
  int or_lo_from = -1;         // out_ready_i forced low on [from, to)
  int or_lo_to = -1;
  int max_cycles = 4000;
};

struct Trace {
  size_t fed = 0;
  int cycles = 0;
  int accept_clocks = 0;
  int req_stall_clocks = 0;    // we offered and req_ready_o was low
  int sheet_reads = 0;

  int offer_hold_clocks = 0;   // sheet_valid_o high while sheet_ready_i low
  int offer_retracted = 0;     // valid dropped before acceptance -- MODE 1
  int offer_unstable = 0;      // payload moved while valid -- MODE 2

  int out_hold_clocks = 0;     // out_valid_o high while out_ready_i low
  int out_retracted = 0;       // the 4-bit-rqcnt wrap signature
  int out_unstable = 0;

  std::vector<uint8_t> out_tok, out_deg, out_tag, out_str;
  std::map<int, int> got_u, got_v;
};

Trace run(Vzhao_texture_aux_pipe& top, const std::vector<Req>& rq, const Cfg& cfg) {
  Trace t;

  top.req_valid_i = 0;
  top.sheet_ready_i = 1;
  top.sheet_rvalid_i = 0;
  top.out_ready_i = 1;
  top.rst_n = 0;
  for (int i = 0; i < 8; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  std::deque<std::pair<int, uint8_t> > pending;  // (due cycle, token)

  // One-clock shadows of both output channels. Ready/valid stability is a
  // statement about two adjacent cycles, so it needs a previous cycle.
  bool pv_sv = false, pv_sr = false;
  uint8_t pv_su = 0, pv_svc = 0, pv_stok = 0;
  bool pv_ov = false, pv_or = false;
  uint8_t pv_otok = 0, pv_otag = 0, pv_ostr = 0, pv_odeg = 0;

  size_t fed = 0;
  int c = 0;
  for (; c < cfg.max_cycles && t.out_tok.size() < rq.size(); ++c) {
    bool sready = true;
    if (cfg.sr_duty_lo != 0) {
      sready = (c % (cfg.sr_duty_hi + cfg.sr_duty_lo)) < cfg.sr_duty_hi;
    }
    if (c >= cfg.sr_lo_from && c < cfg.sr_lo_to) sready = false;
    const bool oready = !(c >= cfg.or_lo_from && c < cfg.or_lo_to);

    top.sheet_ready_i = sready ? 1 : 0;
    top.out_ready_i = oready ? 1 : 0;

    const bool feeding = fed < rq.size();
    top.req_valid_i = feeding;
    if (feeding) {
      const Req& r = rq[fed];
      top.req_wx_i = r.wx;
      top.req_wz_i = r.wz;
      top.req_env_x0_i = r.x0;
      top.req_env_x1_i = r.x1;
      top.req_env_z0_i = r.z0;
      top.req_env_z1_i = r.z1;
      top.req_tok_i = r.tok;
    }

    // The sheet answers `sheet_latency` clocks after it took the read. It has
    // no ready line in either direction: the model presents the response and
    // the leaf is obliged to sink it, which is the whole reason the return
    // queue must reserve for it.
    if (!pending.empty() && pending.front().first <= c) {
      const uint8_t tk = pending.front().second;
      pending.pop_front();
      top.sheet_rvalid_i = 1;
      top.sheet_rtok_i = tk;
      top.sheet_tag_i = want_tag(tk);
      top.sheet_str_i = want_str(tk);
    } else {
      top.sheet_rvalid_i = 0;
    }

    top.eval();

    // ---- LAW 1: a valid offer is held to the acceptance edge --------------
    if (pv_sv && !pv_sr) {
      if (!top.sheet_valid_o) {
        ++t.offer_retracted;
      } else if (top.sheet_u_o != pv_su || top.sheet_v_o != pv_svc ||
                 top.sheet_tok_o != pv_stok) {
        ++t.offer_unstable;
      }
    }
    // ---- LAW 2: the same rule on the return channel -----------------------
    // Retraction here is the exact signature of the old 4-bit rqcnt wrapping
    // to zero on a queue that still held entries.
    if (pv_ov && !pv_or) {
      if (!top.out_valid_o) {
        ++t.out_retracted;
      } else if (top.out_tok_o != pv_otok || top.out_tag_o != pv_otag ||
                 top.out_str_o != pv_ostr || top.out_degenerate_o != pv_odeg) {
        ++t.out_unstable;
      }
    }

    if (top.sheet_valid_o && !sready) ++t.offer_hold_clocks;
    if (top.out_valid_o && !oready) ++t.out_hold_clocks;
    if (feeding && !top.req_ready_o) ++t.req_stall_clocks;

    if (top.out_valid_o && oready) {
      t.out_tok.push_back(top.out_tok_o);
      t.out_deg.push_back(top.out_degenerate_o);
      t.out_tag.push_back(top.out_tag_o);
      t.out_str.push_back(top.out_str_o);
    }
    if (top.sheet_valid_o && sready) {
      pending.push_back(std::make_pair(c + cfg.sheet_latency, static_cast<uint8_t>(top.sheet_tok_o)));
      t.got_u[top.sheet_tok_o] = top.sheet_u_o;
      t.got_v[top.sheet_tok_o] = top.sheet_v_o;
      ++t.sheet_reads;
    }

    const bool took = feeding && top.req_ready_o;
    if (took) ++t.accept_clocks;

    pv_sv = top.sheet_valid_o;
    pv_sr = sready;
    pv_su = top.sheet_u_o;
    pv_svc = top.sheet_v_o;
    pv_stok = top.sheet_tok_o;
    pv_ov = top.out_valid_o;
    pv_or = oready;
    pv_otok = top.out_tok_o;
    pv_otag = top.out_tag_o;
    pv_ostr = top.out_str_o;
    pv_odeg = top.out_degenerate_o;

    zhao::tick(top);
    if (took) ++fed;
  }

  top.req_valid_i = 0;
  top.sheet_rvalid_i = 0;
  t.fed = fed;
  t.cycles = c;
  return t;
}

std::string tag(const char* phase, const char* what) {
  return std::string(phase) + ": " + what;
}

// THE INVARIANT THE ADDENDUM ASKS FOR IN SO MANY WORDS: "Every accepted AUX
// request must yield exactly one correct terminal result." Checked as a
// multiset with payloads, so a lost request, a duplicated one, an overwritten
// queue entry and a mis-flagged degenerate are four distinct failures rather
// than one vague count mismatch.
int exactly_one_result_each(const char* phase, const std::vector<Req>& rq, const Trace& t) {
  zhao::check(t.fed == rq.size(), tag(phase, "every request was accepted").c_str(), rq.size(),
              t.fed);
  zhao::check(t.out_tok.size() == rq.size(),
              tag(phase, "every accepted request yields exactly one terminal result").c_str(),
              rq.size(), t.out_tok.size());

  std::map<int, int> seen;
  for (size_t i = 0; i < t.out_tok.size(); ++i) seen[t.out_tok[i]] += 1;

  int missing = 0, duplicated = 0, flag_bad = 0, payload_bad = 0;
  int want_reads = 0;
  for (size_t i = 0; i < rq.size(); ++i) {
    const int n = seen.count(rq[i].tok) ? seen[rq[i].tok] : 0;
    if (n == 0) ++missing;
    if (n > 1) ++duplicated;
    if (!rq[i].degen) ++want_reads;
  }
  for (size_t i = 0; i < t.out_tok.size(); ++i) {
    // tokens are 0..n-1 in submission order, so the token indexes the request
    const uint8_t tk = t.out_tok[i];
    if (static_cast<size_t>(tk) >= rq.size()) {
      ++payload_bad;
      continue;
    }
    const Req& r = rq[tk];
    if (static_cast<bool>(t.out_deg[i]) != r.degen) ++flag_bad;
    const uint8_t etag = r.degen ? 0 : want_tag(tk);
    const uint8_t estr = r.degen ? 0 : want_str(tk);
    if (t.out_tag[i] != etag || t.out_str[i] != estr) ++payload_bad;
  }

  zhao::check(missing == 0, tag(phase, "no request is lost").c_str(), 0, missing);
  zhao::check(duplicated == 0, tag(phase, "and none returns twice").c_str(), 0, duplicated);
  zhao::check(flag_bad == 0, tag(phase, "each result is flagged degenerate or not, correctly").c_str(),
              0, flag_bad);
  zhao::check(payload_bad == 0,
              tag(phase, "and carries the sheet payload for ITS OWN token -- a queue "
                         "that overwrote an entry fails here, not merely on a count")
                  .c_str(),
              0, payload_bad);
  zhao::check(t.sheet_reads == want_reads,
              tag(phase, "a sheet read is issued for every NON-degenerate request only").c_str(),
              want_reads, t.sheet_reads);
  return missing + duplicated + flag_bad + payload_bad;
}

// The two ready/valid laws, checked on both output channels in every phase.
void handshake_laws(const char* phase, const Trace& t) {
  zhao::check(t.offer_retracted == 0,
              tag(phase, "the sheet offer is never RETRACTED before acceptance -- the "
                         "hang mode, where a stalled request evaporated")
                  .c_str(),
              0, t.offer_retracted);
  zhao::check(t.offer_unstable == 0,
              tag(phase, "and never MUTATES while valid -- coordinates and token are "
                         "stable to the acceptance edge")
                  .c_str(),
              0, t.offer_unstable);
  zhao::check(t.out_retracted == 0,
              tag(phase, "out_valid_o is never retracted while the consumer is not "
                         "ready -- the signature of a wrapped queue count")
                  .c_str(),
              0, t.out_retracted);
  zhao::check(t.out_unstable == 0,
              tag(phase, "and the presented result does not change under it").c_str(), 0,
              t.out_unstable);
}

std::vector<Req> make_stimulus(int n, int degen_every, uint32_t seed) {
  std::vector<Req> rq;
  uint32_t s = seed;
  for (int i = 0; i < n; ++i) {
    Req r;
    r.tok = static_cast<uint8_t>(i);
    r.degen = (i % degen_every) == (degen_every - 1);
    r.x0 = 1000;
    r.x1 = r.degen ? 1000 : 1000 + 64 + static_cast<int32_t>(rnd(&s) % 512);
    r.z0 = 2000;
    r.z1 = 2000 + 64 + static_cast<int32_t>(rnd(&s) % 512);
    r.wx = r.x0 + static_cast<int32_t>(rnd(&s) % 600);
    r.wz = r.z0 + static_cast<int32_t>(rnd(&s) % 600);
    rq.push_back(r);
  }
  return rq;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_aux_pipe top;

  // =========================================================== PHASE A ======
  // Nobody says no. This is the original test, unchanged in what it demands.
  const std::vector<Req> rqa = make_stimulus(64, 3, 0x5EED01u);
  Cfg cfa;
  cfa.sheet_latency = 1;
  cfa.max_cycles = 600;
  const Trace ta = run(top, rqa, cfa);

  exactly_one_result_each("A/free-running", rqa, ta);
  handshake_laws("A/free-running", ta);

  zhao::check(ta.accept_clocks == static_cast<int>(rqa.size()),
              "A/free-running: AUX acceptance is II=1 -- one per clock, no refusals",
              static_cast<int>(rqa.size()), ta.accept_clocks);
  zhao::check(ta.req_stall_clocks == 0,
              "A/free-running: and the credit never has to refuse when the sinks keep up "
              "-- 16 credits against a 10-clock round trip",
              0, ta.req_stall_clocks);
  // Counted from the stimulus rather than from a closed form of it: an
  // expectation derived by re-deriving the generator is a second chance to get
  // the same arithmetic wrong in the same direction.
  uint32_t want_degen = 0;
  for (size_t i = 0; i < rqa.size(); ++i) {
    if (rqa[i].degen) ++want_degen;
  }
  zhao::check(top.degenerate_o == want_degen,
              "A/free-running: and the degenerate ones are counted", want_degen,
              top.degenerate_o);

  // THE ORDERING RULE, IN THE ONE FIXTURE WHERE IT IS REAL.
  // With a one-clock sheet the response for request i and the degenerate
  // completion of request i+1 land on the SAME clock, and the sheet writes the
  // earlier slot, so submission order survives. That is a property of this
  // fixture, not of the block -- phase C is where it stops holding, and phase
  // C measures it rather than asserting it.
  int order_bad = 0;
  for (size_t i = 0; i < ta.out_tok.size() && i < rqa.size(); ++i) {
    if (ta.out_tok[i] != rqa[i].tok) ++order_bad;
  }
  zhao::check(order_bad == 0,
              "A/free-running: with a ONE-CLOCK sheet, returns come back in submission "
              "order and degenerate ones keep their place",
              0, order_bad);

  // ---- THE TEXEL COORDINATE, WHICH NOTHING HERE USED TO CHECK ------------
  // Everything above tests the handshake, the counts, the ordering and the
  // degenerate flag. None of it looks at the number the block exists to
  // produce, and a whole class of fault is invisible to all of it: the
  // saturation flags travel on a SIDE CHANNEL indexed by the divider's tag,
  // and if that write lands one clock away from the divider's issue, slot N's
  // clamp flags are stored under slot N+1's tag. Every count still balances,
  // every request still returns exactly once, in order, flagged correctly --
  // and the clamped envelopes come back clamped on the wrong request.
  //
  // That is not hypothetical. Splitting this block's input cone (2026-09-03,
  // to fix a 54.95 MHz fit whose worst path started at an input PORT) moved
  // the divider's issue point by one clock, and the side-channel write had to
  // move with it. This case is what makes that provable rather than argued.
  //
  // The model is the contract's law, written from the contract: the texel is
  // floor(((w - e0) << 6) / (e1 - e0)), below the envelope clamps to 0 and at
  // or above it clamps to 63.
  int uv_bad = 0, uv_checked = 0, uv_saturated = 0;
  for (size_t i = 0; i < rqa.size(); ++i) {
    const Req& r = rqa[i];
    if (r.degen) continue;  // a degenerate envelope reads no texel
    if (ta.got_u.find(r.tok) == ta.got_u.end()) {
      ++uv_bad;
      continue;
    }
    const int64_t du = static_cast<int64_t>(r.x1) - r.x0;
    const int64_t dv = static_cast<int64_t>(r.z1) - r.z0;
    const int64_t nu = (static_cast<int64_t>(r.wx) - r.x0) * 64;
    const int64_t nv = (static_cast<int64_t>(r.wz) - r.z0) * 64;
    const int64_t qu = nu < 0 ? 0 : nu / du;
    const int64_t qv = nv < 0 ? 0 : nv / dv;
    const int wu = static_cast<int>(qu > 63 ? 63 : qu);
    const int wv = static_cast<int>(qv > 63 ? 63 : qv);
    if (wu == 63 || wv == 63) ++uv_saturated;
    if (ta.got_u.find(r.tok)->second != wu || ta.got_v.find(r.tok)->second != wv) ++uv_bad;
    ++uv_checked;
  }
  zhao::check(uv_bad == 0,
              "A/free-running: every non-degenerate request reads the texel its own "
              "envelope and world point imply -- the side channel is indexed by the "
              "divider's tag, and this is what proves it stays aligned",
              0, uv_bad);
  zhao::check(uv_saturated > 0,
              "A/free-running: and the stimulus really does saturate the clamp, so the "
              "flags being checked are not all zero",
              1, uv_saturated > 0 ? 1 : 0);
  std::printf("  A: %d texel coordinates checked, %d saturated\n", uv_checked, uv_saturated);

  // =========================================================== PHASE B ======
  // THE SHEET SAYS NO, for 28 clocks. That is more than four times the
  // divider's six-clock latency and nearly three times the whole pipeline, so
  // divider results keep arriving throughout the stall with nowhere to go.
  // The old A7 register lost every one of them.
  const std::vector<Req> rqb = make_stimulus(64, 3, 0xB00B1Eu);
  Cfg cfb;
  cfb.sheet_latency = 1;
  cfb.sr_lo_from = 12;
  cfb.sr_lo_to = 40;  // 28 clocks
  cfb.max_cycles = 1500;
  const Trace tb = run(top, rqb, cfb);

  exactly_one_result_each("B/sheet-stalled", rqb, tb);
  handshake_laws("B/sheet-stalled", tb);

  // THE FIXTURE MUST BE SHOWN TO HAVE BITTEN. A backpressure test where the
  // backpressure never coincided with an offer proves nothing, and reads
  // exactly like a pass.
  zhao::check(tb.offer_hold_clocks > 6,
              "B/sheet-stalled: the offer really was held with the sheet refusing, for "
              "LONGER than the six-clock divider latency -- the stall is not a fixture "
              "that missed",
              1, tb.offer_hold_clocks > 6 ? 1 : 0);
  zhao::check(tb.req_stall_clocks > 0,
              "B/sheet-stalled: and the credit ran out and refused a request -- the gate "
              "that replaced `req_ready_o = 1'b1` is shown FIRING, not merely present",
              1, tb.req_stall_clocks > 0 ? 1 : 0);
  std::printf("  B: %d clocks holding the offer, %d clocks refusing input, %d cycles\n",
              tb.offer_hold_clocks, tb.req_stall_clocks, tb.cycles);

  // =========================================================== PHASE C ======
  // THE SHEET IS SLOW AND INTERMITTENT: responses four clocks after the read,
  // and a ready line that is high for two clocks in every five. This is the
  // case the addendum says has NEVER been exercised -- a delayed sheet_rvalid_i
  // landing on the same clock as a degenerate at the offer head, which is the
  // only way the return queue sees its peak push of two against a pop of one.
  //
  // It is also where the "same order" claim dies. A degenerate completes the
  // clock it reaches the head; the non-degenerate ahead of it is four clocks
  // out at the sheet. The reordering is REQUIRED to be observed below, so the
  // correction to that comment has a detector instead of an opinion.
  const std::vector<Req> rqc = make_stimulus(96, 3, 0xC0FFEEu);
  Cfg cfc;
  cfc.sheet_latency = 4;
  cfc.sr_duty_hi = 2;
  cfc.sr_duty_lo = 3;
  cfc.max_cycles = 3000;
  const Trace tc = run(top, rqc, cfc);

  exactly_one_result_each("C/slow-sheet", rqc, tc);
  handshake_laws("C/slow-sheet", tc);

  int reordered = 0;
  for (size_t i = 0; i < tc.out_tok.size() && i < rqc.size(); ++i) {
    if (tc.out_tok[i] != rqc[i].tok) ++reordered;
  }
  zhao::check(reordered > 0,
              "C/slow-sheet: results DO come back out of submission order -- the RTL "
              "comment that claimed otherwise was false, and this is the evidence",
              1, reordered > 0 ? 1 : 0);
  zhao::check(tc.offer_hold_clocks > 0,
              "C/slow-sheet: the duty-cycled ready really did hold offers", 1,
              tc.offer_hold_clocks > 0 ? 1 : 0);
  std::printf("  C: %d of %zu results out of submission order, %d clocks holding the offer\n",
              reordered, tc.out_tok.size(), tc.offer_hold_clocks);

  // =========================================================== PHASE D ======
  // THE CONSUMER SAYS NO, for 45 clocks, while the sheet keeps answering. The
  // return queue must fill, the credit must exhaust and refuse input, and
  // out_valid_o must NOT drop -- a 4-bit count on an 8-deep queue reached 15,
  // wrapped on a push of two, landed on zero with entries resident and stopped
  // delivering. `out_retracted` in handshake_laws() is that exact signature.
  const std::vector<Req> rqd = make_stimulus(64, 4, 0xD00Du);
  Cfg cfd;
  cfd.sheet_latency = 2;
  cfd.or_lo_from = 15;
  cfd.or_lo_to = 60;  // 45 clocks
  cfd.max_cycles = 1500;
  const Trace td = run(top, rqd, cfd);

  exactly_one_result_each("D/consumer-stalled", rqd, td);
  handshake_laws("D/consumer-stalled", td);

  zhao::check(td.out_hold_clocks > 8,
              "D/consumer-stalled: the return queue really did sit on finished results "
              "with the consumer refusing",
              1, td.out_hold_clocks > 8 ? 1 : 0);
  zhao::check(td.req_stall_clocks > 0,
              "D/consumer-stalled: and the credit exhausted, so backpressure reached the "
              "input -- a credit released at the SHEET handshake instead of at terminal "
              "acceptance would never get here",
              1, td.req_stall_clocks > 0 ? 1 : 0);
  std::printf("  D: %d clocks holding results, %d clocks refusing input, %d cycles\n",
              td.out_hold_clocks, td.req_stall_clocks, td.cycles);

  std::printf("  A %zu reqs / %d accepts / %d reads | B %zu | C %zu | D %zu\n", rqa.size(),
              ta.accept_clocks, ta.sheet_reads, rqb.size(), rqc.size(), rqd.size());

  return zhao::report_and_exit("texture_aux_pipe_directed");
}
