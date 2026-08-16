// duo_markers.cpp — THE PHASE-2 GATE DEMO (plan W2.7): two controllers move
// independent 2D markers in Duo, on the REAL console shell (tb_zhao_shell:
// scheduler + DMA + guards + arbiter + SDRAM ctrl/model + video + input +
// audio + debug), capture-exact for 600 frames.
//
// Protocol (the frame loop this binary is the SW.RUNTIME.HPS of):
//   * packet P0 (mode-set, no blit) is published immediately after reset;
//   * packet P_f is published at tick 2f-1 (every SECOND tick): BeginFrame
//     + SetPresentationContract(DUO) + DebugRumble + DebugFrameBlit(slot
//     f&1) + EndFrame — blit source = a marker canvas composed from the
//     pads LATCHED AT the publish tick (zref::PadSnapshot is the oracle);
//   * the blit completes early in the SECOND frame after its claim and the
//     swap lands at tick 2f+1: raster frame 2f+1 displays P_f FRESH and
//     frame 2f+2 lawfully REPEATS it (60 Hz law) with an IDENTICAL CRC —
//     this demo mechanically proves the repeat law 600 times.
//
// WHY HALF-RATE (the W2.7 composition dossier, measured on this shell):
//   a full-canvas Duo blit costs ~338k gpu cycles end to end — HPS fetch
//   ~93k (3,072 serial one-in-flight 64-B bursts x the frozen 16-cycle D10
//   profile) + starvation-free paced VRAM commit ~245k — against a
//   318,592-cycle frame, and CMD.SCHEDULER's D8 law closes every packet at
//   its first tick. 60 Hz fresh-frame cadence with zero deadline faults is
//   therefore INFEASIBLE on the composed Phase-2 machine as frozen (it
//   remains infeasible with the FB bank split and with a read-ahead shim;
//   raw Z60 demand even exceeds total SDRAM bandwidth). The quantified
//   dossier and the ratification-scale paths that would close 60 Hz (wider
//   or pipelined bridge bursts, streaming blit CRC, claim decoupled from
//   vblank) are in reports/status/phase2_wave2.md.
//
// ASSERTED PER FRAME (the plan's acceptance, adapted to the sustainable
// cadence — deviations are stated, never silent):
//   * RTL displayed-stream CRC == zref::render::displayed_crc32c of the
//     composed canvas for EVERY displayed frame — startup frames, fresh
//     marker frames AND their lawful repeats (repeat CRC == fresh CRC);
//   * the repeat pattern is EXACT: ticks 1, 2 and every even tick >= 4 are
//     repeats; every odd tick >= 3 is a fresh swap. deadline_faults reads
//     the EXACT closed-form count of those repeats (the plan's "all zero"
//     is infeasible at this cadence by the dossier above — the counter's
//     own law says repeats COUNT, and this demo pins every single one);
//   * every fence is STATUS_DEADLINE by the same law (a blit cannot finish
//     inside one frame) — pinned exactly, slot and status;
//   * scanout_starvation_cycles CONSTANT from tick 1 on (baseline pinned;
//     zero new starvation ever — the blit pacer law);
//   * audio_underruns / input_sequence_gaps / rumble_frames_dropped == 0
//     absolutely; PCM output is bit-equal to the fed MixerTone stream;
//   * PadFrame sequences gapless at full 60 Hz, snapshots bit-equal to
//     zref::PadSnapshot; rumble duty latches exactly;
//   * counter sweeps: ids 0/1/2 exact closed-form every tick; ids 28/29
//     exact two-tick deltas in steady state; ids 30/31/35/36 pinned;
//   * every blit commits with status 0; zero guard violations, zero shell
//     tripwires, zero SDRAM model errors.
//
// The golden capture (captures/golden/wave2/duo_markers.zcap, capture_format
// 6.1): ABI_INFO + 600 FRAMEBUFFER_EXPECTED (one per marker frame, in frame
// order) + CONTROLLER_SNAPSHOT (all 600 x 2 PadFrames) + COUNTERS with the
// final counter values AND the trajectory hash — CRC-32C over the 600
// displayed-frame CRCs concatenated LE, recorded as counter_id 0xFFFF (not
// a catalog id; the capture-local convention documented here and in the
// status report). Default run VERIFIES byte-identity against the committed
// file; --write regenerates it.
//
// Flags: --frames N (default 600) · --soak N (nightly: N frames with PCG
// jitter on publish timing, pads, rumble; no capture compare) · --write.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "shell_harness.hpp"

using zhao::check;
using namespace zhao_shell;

#ifndef ZHAO_GOLDEN_ZCAP
#define ZHAO_GOLDEN_ZCAP "captures/golden/wave2/duo_markers.zcap"
#endif

namespace {

std::vector<uint8_t> read_file(const std::string& path) {
  std::vector<uint8_t> out;
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return out;
  std::fseek(f, 0, SEEK_END);
  const long len = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  out.resize(size_t(len));
  if (len > 0 && std::fread(out.data(), 1, size_t(len), f) != size_t(len)) {
    out.clear();
  }
  std::fclose(f);
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  uint32_t frames = 600;
  bool soak = false;
  bool write_golden = false;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) {
      frames = uint32_t(std::atoi(argv[++i]));
    } else if (!std::strcmp(argv[i], "--soak") && i + 1 < argc) {
      soak = true;
      frames = uint32_t(std::atoi(argv[++i]));
    } else if (!std::strcmp(argv[i], "--write")) {
      write_golden = true;
    }
  }

  ShellHarness h;
  h.reset();

  // scenario state ----------------------------------------------------------
  zref::PadSnapshot pad_oracle;
  Pcg32 rngA(0x5eedA001, 11), rngB(0x5eedB002, 13), rngJ(0x1057, 17);
  MarkerState p1{60, 90}, p2{188, 94};

  // mirrors
  std::vector<uint8_t> slot_mirror[2];
  slot_mirror[0].assign(zref::render::kSlotBytes, 0);
  slot_mirror[1].assign(zref::render::kSlotBytes, 0);
  uint8_t duty_mirror[4] = {0, 0, 0, 0};
  bool rum_pend_valid = false;
  uint8_t rum_pend_pad = 0, rum_pend_duty = 0;

  // expectations
  std::vector<uint32_t> expect_crc;      // per raster frame, in order
  std::vector<uint32_t> marker_crcs;     // displayed CRC of P1..Pn (frames)
  std::vector<uint8_t> pad_wire_log;     // 2 pads x 20 B per marker frame
  size_t crc_checked = 0;

  const uint32_t crc_black_z60 = zref::render::displayed_crc32c(
      zhao_abi::VIDEO_Z60, slot_mirror[0].data());
  const uint32_t crc_black_duo = zref::render::displayed_crc32c(
      zhao_abi::VIDEO_DUO, slot_mirror[0].data());
  // Tick/frame law (measured + probe-confirmed): tick k is the vswap
  // decision at line 244 of raster frame k-1. P0's mode write lands in
  // frame 0's vblank and latches at frame_start_1, so F1 is ALREADY Duo.
  // P_f publishes at tick 2f-1, its blit completes early in the following
  // frame, the swap lands at tick 2f+1: F_{2f+1} is fresh, F_{2f+2} the
  // lawful repeat with the identical CRC.
  expect_crc.push_back(crc_black_z60);   // F0 (Z60 boot frame)
  expect_crc.push_back(crc_black_duo);   // F1 (first Duo frame, black)
  expect_crc.push_back(crc_black_duo);   // F2 (repeat of the black frame)

  // startup baselines (asserted once, then constant)
  uint64_t starve_baseline = ~0ull;

  // publish P0 (mode-set): claims + executes before tick 1
  {
    PacketSpec s;
    s.frame_id = 0;
    s.sequence = 0;
    s.mode = 2;  // VIDEO_DUO
    check(h.publish(0, build_packet(s)), "publish P0", 1, 1);
  }

  std::vector<uint8_t> canvas;   // scratch: current packet's blit source
  uint32_t published = 0;        // marker packets published (P1..)
  uint64_t publish_at = 0;       // soak jitter: delayed publish
  bool publish_pending = false;
  PacketSpec pending_spec;
  std::vector<uint8_t> pending_pkt;

  uint32_t prev_sweep_28 = 0, prev_sweep_29 = 0;
  uint64_t prev_28 = 0, prev_29 = 0;
  size_t fences_checked = 0;

  const uint32_t last_tick_needed = 2 * frames + 2;
  uint32_t ticks_processed = 0;

  while (ticks_processed < last_tick_needed && zhao::check_failures() < 40) {
    h.step();

    if (publish_pending && h.n >= publish_at) {
      publish_pending = false;
      check(h.publish(int(pending_spec.sequence % 3), pending_pkt),
            "publish Pk (ring slot FREE)", 1, 1);
    }

    if (!h.tick_seen_last_step) continue;

    // ---------------- tick k observed -------------------------------------
    const TickEvent tk = h.ticks.back();
    const uint32_t k = tk.frame_id;
    ++ticks_processed;

    // repeated law (half-rate cadence): ticks 1, 2 and every even tick
    // >= 4 repeat; every odd tick >= 3 is a fresh swap
    {
      const bool exp_rep = (k <= 2) || ((k & 1u) == 0);
      check(tk.repeated == exp_rep, "tick repeated flag (half-rate law)",
            exp_rep, tk.repeated);
    }

    // ---- pad snapshot compare: RTL latch of tick k-1 vs oracle ----------
    if (k >= 2) {
      // oracle out() holds the frames of its last tick() call (tick k-1)
      const zref::PadFrame* of = pad_oracle.frames();
      bool eq = true;
      for (int p = 0; p < 4 && eq; ++p) {
        uint8_t wire[20];
        zref::padFrameToWire(of[p], wire);
        for (int b = 0; b < 20; ++b) {
          const int bit = (p * 20 + b) * 8;
          const uint8_t got =
              uint8_t((h.top.pad_frame_flat_o[bit / 32] >> (bit % 32)) & 0xFF);
          if (got != wire[b]) {
            eq = false;
            break;
          }
        }
      }
      check(eq, "PadFrame latch == zref::PadSnapshot", 1, eq);
      check(h.top.pad_sequence_o[0] == uint16_t(k - 1),
            "pad 0 sequence gapless", uint16_t(k - 1), h.top.pad_sequence_o[0]);
      check(h.top.pad_sequence_o[1] == uint16_t(k - 1),
            "pad 1 sequence gapless", uint16_t(k - 1), h.top.pad_sequence_o[1]);
      check(h.top.input_gaps_o == 0, "input_sequence_gaps zero", 0,
            uint64_t(h.top.input_gaps_o));
    }

    // ---- rumble mirror: P_f (published at tick 2f-1) latches at the RTL
    // edge of tick 2f, one cycle AFTER that tick's observation — so it is
    // VISIBLE from tick 2f+1 on. Apply the pending mirror update at the
    // next odd tick, before the compare.
    if ((k & 1u) == 1 && rum_pend_valid) {
      duty_mirror[rum_pend_pad] = rum_pend_duty;
      rum_pend_valid = false;
    }
    if (k >= 2) {
      bool duty_eq = true;
      for (int p = 0; p < 4; ++p) {
        if (h.top.rumble_duty_o[p] != duty_mirror[p]) duty_eq = false;
      }
      if (!duty_eq) {
        std::printf("  [dbg] tick %u duty rtl={%02x %02x %02x %02x} "
                    "mirror={%02x %02x %02x %02x}\n", k,
                    h.top.rumble_duty_o[0], h.top.rumble_duty_o[1],
                    h.top.rumble_duty_o[2], h.top.rumble_duty_o[3],
                    duty_mirror[0], duty_mirror[1], duty_mirror[2],
                    duty_mirror[3]);
      }
      check(duty_eq, "rumble duty == mirror", 1, duty_eq);
      check(h.top.rumble_drops_o == 0, "rumble_frames_dropped zero", 0,
            uint64_t(h.top.rumble_drops_o));
    }

    // ---- new pad state for this tick + oracle latch ----------------------
    {
      zref::PadRawState raw[4] = {zref::absentPad(), zref::absentPad(),
                                  zref::absentPad(), zref::absentPad()};
      const auto stick = [](Pcg32& r) {
        return int16_t(int32_t(r.next() & 0xFFFF) - 0x8000);
      };
      raw[0].present = true;
      raw[0].buttons = rngA.next() & 0x0000FFFFu;
      raw[0].lx = stick(rngA);
      raw[0].ly = stick(rngA);
      raw[0].rx = stick(rngA);
      raw[0].ry = stick(rngA);
      raw[1].present = true;
      raw[1].buttons = rngB.next() & 0x0000FFFFu;
      raw[1].lx = stick(rngB);
      raw[1].ly = stick(rngB);
      raw[1].rx = stick(rngB);
      raw[1].ry = stick(rngB);
      for (int p = 0; p < 4; ++p) {
        h.pad_present[p] = raw[p].present;
        h.pad_buttons[p] = raw[p].buttons;
        h.pad_lx[p] = raw[p].lx;
        h.pad_ly[p] = raw[p].ly;
        h.pad_rx[p] = raw[p].rx;
        h.pad_ry[p] = raw[p].ry;
      }
      pad_oracle.tick(raw, k);
    }

    // ---- publish the next marker packet (every SECOND tick) --------------
    if ((k & 1u) == 1 && published < frames) {
      ++published;
      const uint32_t f = published;      // P_f: fresh at F_{2f+1}, repeat after
      const zref::PadFrame* of = pad_oracle.frames();
      marker_move(p1, of[0].lx, of[0].ly);
      marker_move(p2, of[1].lx, of[1].ly);
      compose_duo_frame(canvas, f, p1, p2);

      // record the pads that PRODUCED this frame (the capture body)
      for (int p = 0; p < 2; ++p) {
        uint8_t wire[20];
        zref::padFrameToWire(of[p], wire);
        pad_wire_log.insert(pad_wire_log.end(), wire, wire + 20);
      }

      const uint8_t dst = uint8_t(f & 1u);
      const uint32_t arena = (f & 1u) ? kArena1 : kArena0;
      const uint32_t blit_crc =
          zhao_abi::zhao_crc32c(0, canvas.data(), canvas.size());
      h.mem_write(arena, canvas);

      // mirror the commit + the expected displayed frame (F = f + 2)
      std::memcpy(slot_mirror[dst].data(), canvas.data(), canvas.size());
      const uint32_t dcrc = zref::render::displayed_crc32c(
          zhao_abi::VIDEO_DUO, slot_mirror[dst].data());
      expect_crc.push_back(dcrc);   // fresh frame F_{2f+1}
      expect_crc.push_back(dcrc);   // its lawful repeat F_{2f+2}
      marker_crcs.push_back(dcrc);

      PacketSpec s;
      s.frame_id = f;
      s.sequence = f;                    // ring slot = f % 3
      s.mode = 2;
      s.has_blit = true;
      s.blit_dst = dst;
      s.blit_src = arena;
      s.blit_len = uint32_t(canvas.size());
      s.blit_crc = blit_crc;
      s.has_rumble = true;
      s.rumble_pad = uint8_t(f & 1u);
      s.rumble_en = 1;
      s.rumble_str = soak ? uint8_t(rngJ.next() | 0x01u)
                          : uint8_t((f & 0x7Fu) | 0x10u);
      pending_spec = s;
      pending_pkt = build_packet(s);
      // rumble mirror applies at the next odd tick (latch visibility law)
      rum_pend_valid = true;
      rum_pend_pad = s.rumble_pad;
      rum_pend_duty = s.rumble_str;

      if (soak) {
        publish_pending = true;
        publish_at = h.n + rngJ.below(8000);
      } else {
        check(h.publish(int(f % 3), pending_pkt), "publish Pk", 1, 1);
      }
    }

    // ---- drain CRC pulses against the expectation queue ------------------
    while (crc_checked < h.crcs.size() && crc_checked < expect_crc.size()) {
      check(h.crcs[crc_checked] == expect_crc[crc_checked],
            "displayed CRC == zref-composed", expect_crc[crc_checked],
            h.crcs[crc_checked]);
      ++crc_checked;
    }
    check(h.crcs.size() <= expect_crc.size(), "no unexpected CRC pulses",
          1, h.crcs.size() <= expect_crc.size());

    // ---- counter sweep of this tick --------------------------------------
    // (the 40-beat window completes within ~41 cycles of the tick; it is
    // still in flight right now — check the PREVIOUS tick's sweep)
    if (k >= 2 && !h.sweeps.empty()) {
      const SweepEvent& sw = h.sweeps.back();   // tick k-1's completed sweep
      const uint32_t sk = k - 1;
      if (sw.bank.size() == 40) {
        check(sw.bank[0] == sk, "cnt frame_cycles", sk, sw.bank[0]);
        // repeats at ticks 1, 2, 4, 6, ... -> F(k) = 1 + floor(k/2), k >= 2
        const uint64_t faults = (sk <= 1) ? sk : (1 + sk / 2);
        check(sw.bank[1] == faults, "cnt deadline_faults (closed form)",
              faults, sw.bank[1]);
        // P0 = 3 records before tick 1; P_f (5 records) dispatched between
        // ticks 2f-1 and 2f -> cmds(k) = 3 + 5*floor(k/2)
        const uint64_t cmds = (sk >= 1) ? (3 + 5ull * (sk / 2)) : 0;
        check(sw.bank[2] == cmds, "cnt commands", cmds, sw.bank[2]);
        check(sw.bank[31] == 0, "cnt audio_underruns", 0, sw.bank[31]);
        check(sw.bank[35] == 0, "cnt input_sequence_gaps", 0, sw.bank[35]);
        check(sw.bank[36] == 0, "cnt rumble_frames_dropped", 0, sw.bank[36]);
        // starvation: the boot transient (frame-0 line 0 plus the
        // mode-switch flush at frame_start_2, which refetches with no
        // prefetch margin — for Storm that is exactly 2 lines; Duo's
        // border absorbs it) settles by tick 2; pinned constant after.
        if (starve_baseline == ~0ull && sk >= 2) {
          starve_baseline = sw.bank[30];
          check(starve_baseline <= 1024,
                "starvation baseline is <= two lines", 1,
                starve_baseline <= 1024);
        }
        if (starve_baseline != ~0ull) {
          check(sw.bank[30] == starve_baseline,
                "cnt scanout_starvation constant after boot", starve_baseline,
                sw.bank[30]);
        }
        // steady-state byte deltas over one full publish period (2 ticks):
        // reads every frame (2 x 196,608) + one blit's writes (196,608);
        // one packet (232 B) + one blit (196,608 B) over the bridge
        if ((sk & 1u) == 0 && sk >= 8) {
          if (prev_sweep_28) {
            const uint64_t d28 = sw.bank[28] - prev_28;
            const uint64_t d29 = sw.bank[29] - prev_29;
            if (!soak) {
              // deterministic cadence: the deltas are EXACT
              check(d28 == 589824,
                    "vram bytes 2-tick delta (2 reads + 1 blit)", 589824,
                    d28);
              check(d29 == 196840,
                    "hps bytes 2-tick delta (packet + blit)", 196840, d29);
            } else {
              // jittered publishes shift which side of the shadow latch a
              // handful of blit bursts land on: the per-period delta may
              // wobble by a few bursts, but never by more than one line's
              // worth — and the wobble must cancel over the run (the
              // cumulative totals are asserted at the end)
              const bool ok28 = d28 >= 589824 - 8192 && d28 <= 589824 + 8192;
              const bool ok29 = d29 >= 196840 - 8192 && d29 <= 196840 + 8192;
              check(ok28, "vram bytes 2-tick delta within jitter window",
                    589824, d28);
              check(ok29, "hps bytes 2-tick delta within jitter window",
                    196840, d29);
            }
          }
          prev_28 = sw.bank[28];
          prev_29 = sw.bank[29];
          prev_sweep_28 = 1;
          prev_sweep_29 = 1;
        }
      } else {
        check(false, "sweep delivered 40 beats", 40, sw.bank.size());
      }
    }

    // ---- fences (lazy drain) ---------------------------------------------
    // fence 0 = P0 at tick 1; fence f = P_f at tick 2f. EVERY fence is
    // STATUS_DEADLINE by the composition dossier (a full-canvas blit cannot
    // finish inside one frame period) — pinned, not tolerated.
    while (fences_checked < h.fence_log.size()) {
      const size_t i = fences_checked++;
      const ShellHarness::Fence& fe = h.fence_log[i];
      check(!fe.ok && fe.status == 16, "fence: pinned STATUS_DEADLINE",
            16, fe.status);
      check(fe.slot == uint8_t(i % 3), "fence ring slot", uint8_t(i % 3),
            fe.slot);
    }

    // ---- sticky integrity -------------------------------------------------
    check(h.sticky_errors() == 0, "sticky integrity flags all clear", 0,
          h.sticky_errors());
    check(h.top.audio_underruns_o == 0, "audio underruns zero", 0,
          h.top.audio_underruns_o);
  }

  // ---- drain the remaining CRC pulses (the last frames' raster lags) ------
  {
    uint64_t guard_steps = 0;
    while (crc_checked < expect_crc.size() && guard_steps < 3'000'000) {
      h.step();
      ++guard_steps;
      while (crc_checked < h.crcs.size() && crc_checked < expect_crc.size()) {
        check(h.crcs[crc_checked] == expect_crc[crc_checked],
              "displayed CRC == zref-composed (tail)", expect_crc[crc_checked],
              h.crcs[crc_checked]);
        ++crc_checked;
      }
    }
    check(crc_checked == expect_crc.size(), "all expected frames displayed",
          expect_crc.size(), crc_checked);
  }

  // ---- audio identity (checked on the fly, bounded memory) ----------------
  {
    check(h.top.audio_underruns_o == 0, "audio underruns zero (final)", 0,
          h.top.audio_underruns_o);
    check(h.pcm_pop_count > 0, "audio stream flowed", 1, h.pcm_pop_count > 0);
    check(h.pcm_mismatches == 0,
          "PCM output bit-equal to the fed MixerTone stream", 0,
          h.pcm_mismatches);
    check(h.aud_ring_overruns == 0, "audio compare ring never overran", 0,
          h.aud_ring_overruns);
  }

  // ---- blit log ------------------------------------------------------------
  {
    check(h.blit_log.size() == published, "one blit completion per packet",
          published, h.blit_log.size());
    bool all_ok = true;
    for (const auto& bd : h.blit_log) all_ok = all_ok && (bd.status == 0);
    check(all_ok, "every blit committed (status 0)", 1, all_ok);
  }

  // ---- the golden capture (600-frame shape only, not in soak) --------------
  if (!soak && frames == 600 && zhao::check_failures() == 0) {
    const std::string tmp = std::string(ZHAO_GOLDEN_ZCAP) + ".regen";
    {
      zhao::ZhaoZcapWriter w(tmp);
      w.add_section(zhao::ZHAO_ZCAP_ABI_INFO, 1, zhao::zhao_zcap_build_abi_info());
      for (uint32_t f = 0; f < frames; ++f) {
        uint8_t fb[14] = {0};
        fb[0] = 2;                       // mode DUO
        fb[1] = 2;                       // view_count
        fb[4] = uint8_t(512 & 0xFF);     // width
        fb[5] = uint8_t(512 >> 8);
        fb[6] = uint8_t(240 & 0xFF);     // height
        fb[7] = 0;
        const uint32_t c = marker_crcs[f];
        fb[10] = uint8_t(c & 0xFF);
        fb[11] = uint8_t((c >> 8) & 0xFF);
        fb[12] = uint8_t((c >> 16) & 0xFF);
        fb[13] = uint8_t((c >> 24) & 0xFF);
        // layout: {u8 mode; u8 view_count; u16 flags; u16 width; u16 height;
        //          u16 rsv; u32 expected_crc32c} = 14 B -> 16 with align? The
        // spec struct is 16 B with the crc at +10 (u16 rsv at +8).
        w.add_section(zhao::ZHAO_ZCAP_FRAMEBUFFER_EXPECTED, 1, fb, sizeof fb);
      }
      {
        std::vector<uint8_t> body(4);
        const uint32_t cnt = uint32_t(pad_wire_log.size() / 20);
        body[0] = uint8_t(cnt & 0xFF);
        body[1] = uint8_t((cnt >> 8) & 0xFF);
        body[2] = uint8_t((cnt >> 16) & 0xFF);
        body[3] = uint8_t((cnt >> 24) & 0xFF);
        body.insert(body.end(), pad_wire_log.begin(), pad_wire_log.end());
        w.add_section(zhao::ZHAO_ZCAP_CONTROLLER_SNAPSHOT, 1, body);
      }
      {
        // trajectory hash: CRC-32C over the 600 displayed CRCs (LE u32s)
        std::vector<uint8_t> chain;
        for (uint32_t c : marker_crcs) {
          chain.push_back(uint8_t(c & 0xFF));
          chain.push_back(uint8_t((c >> 8) & 0xFF));
          chain.push_back(uint8_t((c >> 16) & 0xFF));
          chain.push_back(uint8_t((c >> 24) & 0xFF));
        }
        const uint32_t traj = zhao_abi::zhao_crc32c(0, chain.data(), chain.size());
        struct Ent {
          uint16_t id;
          uint64_t v;
        };
        // the shadow values AT the last VERIFIED tick (2F+1): frame_cycles
        // = 2F+1 and deadline_faults = 1 + floor((2F+1)/2) = F+1 — the
        // half-rate closed form the demo pins every tick. (An earlier
        // revision recorded the abandoned 60 Hz model's constants here;
        // byte-identity could never catch that, since the writer and the
        // committed file agreed — found by decoding the capture with an
        // independent reader.)
        const Ent ents[] = {
            {0, 2ull * frames + 1},       // frame_cycles at tick 2F+1
            {1, uint64_t(frames) + 1},    // deadline_faults = F+1
            {30, starve_baseline},        // boot constant (0 for Duo)
            {31, 0},
            {35, 0},
            {36, 0},
            {0xFFFF, traj},               // trajectory hash (capture-local id)
        };
        std::vector<uint8_t> body(4);
        body[0] = uint8_t(sizeof(ents) / sizeof(ents[0]));
        for (const Ent& e : ents) {
          uint8_t rec[12];
          rec[0] = uint8_t(e.id & 0xFF);
          rec[1] = uint8_t(e.id >> 8);
          rec[2] = rec[3] = 0;
          for (int b = 0; b < 8; ++b) rec[4 + b] = uint8_t((e.v >> (8 * b)) & 0xFF);
          body.insert(body.end(), rec, rec + 12);
        }
        w.add_section(zhao::ZHAO_ZCAP_COUNTERS, 1, body);
      }
      check(w.close(), "zcap write ok", 1, 1);
    }
    const std::vector<uint8_t> regen = read_file(tmp);
    check(!regen.empty(), "zcap regenerated", 1, !regen.empty());
    if (write_golden) {
      FILE* f = std::fopen(ZHAO_GOLDEN_ZCAP, "wb");
      check(f != nullptr, "golden zcap writable", 1, f != nullptr);
      if (f) {
        std::fwrite(regen.data(), 1, regen.size(), f);
        std::fclose(f);
      }
      std::printf("[duo_markers] wrote %s (%zu bytes)\n", ZHAO_GOLDEN_ZCAP,
                  regen.size());
    } else {
      const std::vector<uint8_t> committed = read_file(ZHAO_GOLDEN_ZCAP);
      check(!committed.empty(),
            "committed golden exists (run --write once to create)", 1,
            !committed.empty());
      check(committed == regen, "golden zcap byte-identical", 1,
            committed == regen);
    }
    std::remove(tmp.c_str());
  }

  std::printf("[duo_markers] frames=%u ticks=%zu crcs=%zu pcm=%llu%s\n",
              frames, h.ticks.size(), h.crcs.size(),
              (unsigned long long)h.pcm_pop_count, soak ? " (soak)" : "");
  return zhao::report_and_exit("duo_markers");
}
