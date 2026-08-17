// audio_fifo_directed.cpp — W2.4 AUDIO.FIFO directed tests (plan W2.4;
// design/contracts/AUDIO.FIFO.md "Directed tests"; law spec/audio_rules.md).
//
// Every scenario drives the Verilated zhao_audio_fifo AND the cycle-accurate
// oracle zref::AudioFifo through the identical gpu/audio protocol
// (audio_clk = gpu_clk/4 seam, tests/audio/audio_dev.hpp) and requires the
// FULL output stream bit-exact; on top of that each scenario asserts its
// semantic law:
//
//   1. reset silence   — no writes: zero pairs (NOT repeats), no underrun
//   2. steady fill     — 800 pairs/frame for 3 frames: stream == pairs in
//                        order, plateau occupancy, ZERO underruns
//   3. deliberate underrun — drain: last pair repeated bit-exactly, stream
//                        continuous across the event, audio_underruns == 1
//                        (ONE continuous event); refill: resumes with the
//                        next pair, no loss/dup; second drain -> 2 events
//   4. full backpressure — offer 4096 pairs continuously: exactly 2048
//                        accepted until drain (no accept when full), then
//                        the rest; nothing dropped, stream in order
//   5. tone passthrough — the FIFO carries zref::MixerTone pairs: output
//                        stream == tone stream bit-exactly, underruns == 0
//   6. counter snapshot — frame_tick latches the u64 shadow (gray crossing),
//                        counter_id == ZHAO_CNT_AUDIO_UNDERRUNS

#include "audio_dev.hpp"
#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>

namespace {

using zhao_audio::Burst;
using zhao_audio::OracleDev;
using zhao_audio::RtlDev;
using zhao_audio::RunResult;
using zref::AudioFifo;

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

void check_eq(uint64_t expected, uint64_t actual, const char* what) {
  zhao::check(expected == actual, what, expected, actual);
  if (expected != actual) ++failures;
}

// RTL-vs-oracle differential core (stream + occupancy + shadows + counters)
bool diff(const std::vector<Burst>& bursts, uint32_t cycles,
          const std::vector<uint32_t>& frame_ticks, RunResult* out_rtl, RunResult* out_oracle,
          const char* scenario) {
  RtlDev rtl;
  OracleDev orc;
  *out_rtl = zhao_audio::run_schedule(rtl, bursts, cycles, frame_ticks);
  *out_oracle = zhao_audio::run_schedule(orc, bursts, cycles, frame_ticks);
  std::string where;
  const bool eq = zhao_audio::results_equal(*out_rtl, *out_oracle, &where);
  check(eq, (std::string(scenario) + ": RTL == oracle (" + where + ")").c_str());
  return eq;
}

}  // namespace

int main() {
  // ---- 1. reset silence: zeros, NOT repeats; no underrun count -----------
  {
    RtlDev rtl;
    OracleDev orc;
    const RunResult r = zhao_audio::run_schedule(rtl, {}, 64, {});
    const RunResult o = zhao_audio::run_schedule(orc, {}, 64, {});
    std::string where;
    check(zhao_audio::results_equal(r, o, &where),
          ("reset silence: RTL == oracle (" + where + ")").c_str());
    check_eq(r.stream.size(), 16, "reset silence: 16 audio ticks in 64 cycles");
    check_eq(r.underruns_final, 0, "reset silence: no underrun counted");
    check_eq(r.accepted, 0, "reset silence: nothing accepted");
    for (size_t i = 0; i < r.stream.size(); ++i) {
      check(!r.stream[i].valid, "reset silence: pcm_valid stays 0");
      check_eq(r.stream[i].l, 0, "reset silence: L is zero");
      check_eq(r.stream[i].r, 0, "reset silence: R is zero");
      check(!r.stream[i].underrun, "reset silence: not an underrun repeat");
    }
  }

  // ---- 2. steady fill: exactly 800 pairs/frame, 3 frames ------------------
  {
    // Fill 512 pairs at the start (2 bursts), then one 256 burst per 1024
    // gpu cycles = 256 pairs per 256 audio ticks: the long-run average
    // exactly matches consumption (1 pair / 4 gpu cycles), so occupancy
    // plateaus around/below the watermark band and NEVER underruns.
    std::vector<Burst> bursts;
    bursts.push_back(Burst{0, 256});
    bursts.push_back(Burst{16, 256});
    for (uint32_t base = 1024; base < 3 * 4 * zref::kAudioPairsPerFrame; base += 1024) {
      bursts.push_back(Burst{base, 256});
    }
    const uint32_t cycles = 3 * 4 * zref::kAudioPairsPerFrame;  // 9600
    RunResult r, o;
    diff(bursts, cycles, {3200, 6400}, &r, &o, "steady fill");
    check_eq(r.underruns_final, 0, "steady fill: zero underruns");
    check_eq(r.stream.size(), 3 * zref::kAudioPairsPerFrame,
             "steady fill: exactly 800 pairs x 3 frames of audio ticks");
    // leading silence: 1-3 ticks before the first pair crosses the CDC
    // (gray-sync visibility), then EVERY remaining tick is a valid pair
    size_t i = 0;
    while (i < r.stream.size() && !r.stream[i].valid) {
      check(!r.stream[i].underrun, "steady fill: leading tick is silence, not repeat");
      ++i;
    }
    check(i >= 1 && i <= 4, "steady fill: small CDC-sync silence head");
    size_t real = 0;
    uint64_t expect = 0;
    for (; i < r.stream.size(); ++i) {
      uint16_t l, rr;
      zhao_audio::pair_k(expect++, &l, &rr);
      const bool ok =
          r.stream[i].valid && !r.stream[i].underrun && r.stream[i].l == l && r.stream[i].r == rr;
      if (!ok) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "steady fill: pair %llu mismatch (%04x,%04x vs %04x,%04x)",
                      (unsigned long long)(expect - 1), r.stream[i].l, r.stream[i].r, l, rr);
        check(false, buf);
        break;
      }
      ++real;
    }
    // every tick after the head was a real in-order pair (continuity)
    check_eq(expect, real, "steady fill: real ticks == walked pairs (no gaps)");
    // plateau: occupancy stays inside a healthy band and the watermark
    // request fires (the refill law is visible on the interface)
    bool req_seen = false;
    uint32_t occ_max = 0;
    for (size_t c = 0; c < r.occupancy.size(); ++c) {
      if (r.occupancy[c] > occ_max) occ_max = r.occupancy[c];
      if (r.occupancy[c] <= AudioFifo::kWatermark) req_seen = true;
    }
    check(req_seen, "steady fill: refill_req fires (occupancy <= watermark)");
    check(occ_max <= AudioFifo::kDepth, "steady fill: occupancy <= depth");
    // band measured AFTER the startup transient (cycle 512 = initial fill
    // done): plateau below/around the watermark, never drained, never full
    uint32_t band_min = 4096, band_max = 0;
    for (size_t c = 512; c < r.occupancy.size(); ++c) {
      if (r.occupancy[c] < band_min) band_min = r.occupancy[c];
      if (r.occupancy[c] > band_max) band_max = r.occupancy[c];
    }
    check(band_min >= 128 && band_max <= 768,
          "steady fill: occupancy plateaus in a band (no drain, no fill-up)");
  }

  // ---- 3. deliberate underrun: repeat + count once + continuous ----------
  {
    std::vector<Burst> bursts;
    bursts.push_back(Burst{0, 256});
    bursts.push_back(Burst{16, 256});
    // 512 pairs fill; audio drains 512 ticks by ~gpu cycle 2080; starve for
    // ~40 ticks, then refill; starve again later (a SECOND event)
    bursts.push_back(Burst{2560, 256});
    bursts.push_back(Burst{2576, 256});
    const uint32_t cycles = 9000;
    RunResult r, o;
    diff(bursts, cycles, {100, 4000, 8000}, &r, &o, "deliberate underrun");
    check_eq(r.underruns_final, 2, "deliberate underrun: exactly 2 events");
    check_eq(r.accepted, 1024, "deliberate underrun: all 1024 pairs accepted");

    // semantic walk over the RTL stream (identical to the oracle's):
    //   [silence head] (pair-run event-repeat)* — every tick VALID (the
    //   stream is continuous across underruns), every pair-run tick matches
    //   the written sequence in order, every repeat tick matches the most
    //   recent real pair bit-exactly, and the counter advances exactly once
    //   per continuous event.
    size_t i = 0;
    while (i < r.stream.size() && !r.stream[i].valid) {
      check(!r.stream[i].underrun, "deliberate underrun: leading silence, not repeat");
      ++i;
    }
    check(i >= 1 && i <= 4, "deliberate underrun: small CDC-sync silence head");
    uint64_t expect = 0;
    int events = 0;
    size_t first_starve_len = 0;
    while (i < r.stream.size()) {
      // (a) real pair run
      uint16_t cur_l = 0, cur_r = 0;
      size_t run = 0;
      while (i < r.stream.size() && r.stream[i].valid && !r.stream[i].underrun) {
        uint16_t l, rr;
        zhao_audio::pair_k(expect++, &l, &rr);
        if (r.stream[i].l != l || r.stream[i].r != rr) {
          char buf[128];
          std::snprintf(buf, sizeof buf, "deliberate underrun: pair %llu out of order",
                        (unsigned long long)(expect - 1));
          check(false, buf);
        }
        cur_l = r.stream[i].l;
        cur_r = r.stream[i].r;
        ++i;
        ++run;
      }
      if (run == 0) {
        check(false, "deliberate underrun: back-to-back underrun events");
        break;
      }
      if (i == r.stream.size()) break;
      // (b) underrun run: repeats the last real pair, counts once
      ++events;
      size_t starved = 0;
      while (i < r.stream.size() && r.stream[i].underrun) {
        const bool ok = r.stream[i].valid && r.stream[i].l == cur_l && r.stream[i].r == cur_r;
        if (!ok) check(false, "deliberate underrun: repeat tick bit-exact");
        check_eq(r.stream[i].underruns, static_cast<uint64_t>(events),
                 "deliberate underrun: ONE count per continuous event");
        ++i;
        ++starved;
      }
      if (events == 1) first_starve_len = starved;
    }
    check(first_starve_len >= 16, "deliberate underrun: a real starvation run observed");
    check_eq(expect, 1024, "deliberate underrun: every written pair emitted exactly once");
    check_eq(events, 2, "deliberate underrun: exactly two continuous events");
  }

  // ---- 4. full-FIFO backpressure: no accept when full, nothing lost ------
  {
    // 4096 pairs offered continuously from cycle 0. The FIFO accepts exactly
    // 2048 then stalls (ready/valid law) until drained; every offered pair
    // is eventually accepted and emitted IN ORDER.
    std::vector<Burst> bursts;
    bursts.push_back(Burst{0, 4096});
    RunResult r, o;
    diff(bursts, 25000, {8000, 16000}, &r, &o, "full backpressure");
    // 25000 gpu cycles = 6250 audio ticks for 4096 pairs: after the last
    // pair the FIFO starves — exactly ONE trailing underrun event (which
    // conveniently exercises the repeat law once more)
    check_eq(r.underruns_final, 1, "full backpressure: one trailing event");
    check_eq(r.accepted, 4096, "full backpressure: every offered pair accepted");
    check_eq(r.stream.size(), 6250, "full backpressure: 6250 audio ticks observed");
    // structural law from the trace: occupancy hits exactly 2048 (full) and
    // never exceeds it; wr_ready was polled by the driver each cycle (an
    // accept at full would have appeared as accepted > accepted-at-full)
    uint32_t occ_max = 0;
    size_t full_cycles = 0;
    for (uint32_t occ : r.occupancy) {
      if (occ > occ_max) occ_max = occ;
      if (occ == AudioFifo::kDepth) ++full_cycles;
    }
    check_eq(occ_max, AudioFifo::kDepth, "full backpressure: occupancy reaches full");
    check(occ_max <= AudioFifo::kDepth, "full backpressure: never beyond depth");
    check(full_cycles >= 512, "full backpressure: a real sustained stall at full observed");
    // stream: [silence head] pairs 0..4095 in order, then repeats of the
    // last pair once starved (never a gap)
    size_t i = 0;
    while (i < r.stream.size() && !r.stream[i].valid) ++i;
    check(i >= 1 && i <= 4, "full backpressure: small CDC-sync silence head");
    uint64_t expect = 0;
    for (; i < r.stream.size(); ++i) {
      uint16_t l, rr;
      zhao_audio::pair_k(expect, &l, &rr);
      if (!r.stream[i].underrun) {
        if (!(r.stream[i].l == l && r.stream[i].r == rr)) {
          check(false, "full backpressure: emitted stream in write order");
          break;
        }
        ++expect;
      }
    }
    check_eq(expect, 4096, "full backpressure: all pairs emitted in order");
  }

  // ---- 5. tone passthrough: FIFO output == zref::MixerTone bit-exact -----
  {
    // A4 tone, 3 frames: sustained refill (256 pairs per 1024 gpu cycles
    // after an initial 768 fill — same pacing as scenario 2).
    zref::MixerTone tone(zref::ToneId::TONE_A4);
    std::vector<zref::AudioPair> want;
    for (int f = 0; f < 3; ++f) {
      std::vector<zref::AudioPair> fr = tone.frame();
      want.insert(want.end(), fr.begin(), fr.end());
    }
    std::vector<Burst> bursts;
    bursts.push_back(Burst{0, 256});
    bursts.push_back(Burst{16, 256});
    bursts.push_back(Burst{32, 256});
    for (uint32_t base = 1024; base < 9600; base += 1024) {
      bursts.push_back(Burst{base, 256});
    }
    // Feed the tone pairs themselves: override pair_k by writing through a
    // dedicated driver pass (tone stream replaces the synthetic pattern).
    RtlDev rtl;
    OracleDev orc;
    zref::MixerTone tone_rtl(zref::ToneId::TONE_A4);
    zref::MixerTone tone_orc(zref::ToneId::TONE_A4);
    RunResult r, o2;
    {
      // mini-runner identical to run_schedule but sourcing pairs from the
      // tone generators (one generator per device, kept in lockstep)
      struct Feeder {
        zref::MixerTone* t;
        bool operator()(uint64_t, uint16_t* l, uint16_t* r) {
          const zref::AudioPair p = t->tick();
          *l = static_cast<uint16_t>(p.l);
          *r = static_cast<uint16_t>(p.r);
          return true;
        }
      };
      auto drive = [&](auto& dev, zref::MixerTone* t, RunResult* res) {
        std::vector<int64_t> credit(9601, 0);
        for (const Burst& b : bursts) credit[b.start_cycle] += b.len;
        int64_t credits = 0;
        uint64_t emitted_offers = 0;  // tone pairs already generated+pending
        uint16_t pend_l = 0, pend_r = 0;
        bool have = false;
        for (uint32_t c = 0; c < 9600; ++c) {
          credits += credit[c];
          // generate the next tone pair only when the previous one was
          // ACCEPTED (ready/valid law: held data, never skipped) — mirrors
          // run_schedule's pair_k(accepted_count) indexing
          if (!have && credits > 0) {
            Feeder feed{t};
            feed(emitted_offers++, &pend_l, &pend_r);
            have = true;
          }
          const bool offer = credits > 0;
          const bool acc = dev.cycle(offer && have, pend_l, pend_r, false);
          if (acc) {
            --credits;
            have = false;
          }
          res->occupancy.push_back(dev.occupancy());
          if (dev.audio_edge_fired) {
            res->stream.push_back(RunResult::Tick{dev.pcm_valid, dev.pcm_l, dev.pcm_r,
                                                  dev.underrun_status, dev.underruns()});
          }
        }
        res->underruns_final = dev.underruns();
        res->accepted = emitted_offers;
      };
      drive(rtl, &tone_rtl, &r);
      drive(orc, &tone_orc, &o2);
    }
    std::string where;
    check(zhao_audio::results_equal(r, o2, &where),
          ("tone passthrough: RTL == oracle (" + where + ")").c_str());
    check_eq(r.underruns_final, 0, "tone passthrough: zero underruns");
    check_eq(r.stream.size(), 2400, "tone passthrough: 2400 audio ticks");
    size_t i = 0;
    while (i < r.stream.size() && !r.stream[i].valid) ++i;  // CDC silence head
    size_t k = 0;
    for (; i < r.stream.size() && k < want.size(); ++i, ++k) {
      const bool ok = r.stream[i].valid && !r.stream[i].underrun &&
                      r.stream[i].l == static_cast<uint16_t>(want[k].l) &&
                      r.stream[i].r == static_cast<uint16_t>(want[k].r);
      if (!ok) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "tone passthrough: tick %zu (%04x,%04x) vs tone (%04x,%04x)",
                      k, r.stream[i].l, r.stream[i].r, (uint16_t)want[k].l, (uint16_t)want[k].r);
        check(false, buf);
        break;
      }
    }
    // every tick after the silence head was a bit-exact tone pair
    check(i == r.stream.size(), "tone passthrough: walked every tick clean");
  }

  // ---- 6. counter snapshot law (frame_tick -> u64 shadow, gray crossing) --
  {
    // Underrun events happen before the second frame_tick; the shadow
    // latches a STABLE view (old-or-new, never torn) and the id is the
    // catalog index. Compare RTL vs oracle shadow trace (already asserted
    // inside every diff() above via shadows); here check the id + valid.
    RtlDev rtl;
    // drain into underrun quickly: 1 pair then nothing
    rtl.cycle(true, 0x1234, 0x5678, false);  // offered pair (accepted)
    for (int i = 0; i < 8; ++i) rtl.cycle(false, 0, 0, false);
    uint64_t live = 0;
    for (int i = 0; i < 64; ++i) {  // drains + starves
      rtl.cycle(false, 0, 0, false);
      live = rtl.underruns();
    }
    check(live >= 1, "snapshot: at least one underrun happened");
    rtl.cycle(false, 0, 0, true);  // frame_tick pulse
    check(rtl.snap_valid(), "snapshot: valid pulses with frame_tick");
    check_eq(rtl.snap_id(), 31, "snapshot: counter_id == ZHAO_CNT_AUDIO_UNDERRUNS");
    check_eq(rtl.snap_value(), live, "snapshot: shadow == live counter value");
    // valid is a one-cycle pulse
    rtl.cycle(false, 0, 0, false);
    check(!rtl.snap_valid(), "snapshot: valid is a one-cycle pulse");
  }

  const int rc = zhao::report_and_exit("audio_fifo_directed");
  return rc == 0 && failures == 0 ? 0 : 1;
}
