// shell_golden.cpp — W2.7 per-mode golden captures (capture_format.md 6.1):
// captures/golden/wave2/{z60,storm,duo}_10frame.zcap.
//
// Each capture replays TEN sealed frame packets through the REAL shell at
// the machine's sustainable cadence (one packet per two ticks — the W2.7
// composition dossier in reports/status/phase2_wave2.md): packet P_f
// publishes at tick 2f-1, displays FRESH at raster frame 2f+1 and lawfully
// REPEATS at 2f+2 with an IDENTICAL displayed CRC — every capture therefore
// exercises the 60 Hz repeat law ten times per mode, and the repeat-CRC-
// identical property is asserted for every pair.
//
// Capture contents (section order): ABI_INFO; per packet a FRAME_PACKET
// (the sealed bytes) followed by its FRAMEBUFFER_EXPECTED (the displayed-
// stream CRC of its fresh frame — repeats CRC identically by the asserted
// law); one CONTROLLER_SNAPSHOT (the 2 present pads at each publish tick,
// zref::PadSnapshot-verified wire bytes); one COUNTERS section with the
// final counter values (including the deterministic deadline_faults of the
// half-rate cadence and the pinned starvation baseline).
//
// Default: run all three modes, verify byte-identity against the committed
// captures. --write regenerates them. --mode z60|storm|duo runs one.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "shell_harness.hpp"

using zhao::check;
using namespace zhao_shell;

#ifndef ZHAO_GOLDEN_DIR
#define ZHAO_GOLDEN_DIR "captures/golden/wave2"
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

struct ModeInfo {
  const char* name;
  uint8_t mode;
  uint16_t width;
  uint8_t views;
};

const ModeInfo kModes[3] = {{"z60", 0, 384, 1}, {"storm", 1, 320, 1}, {"duo", 2, 512, 2}};

// one mode's 10-packet replay; returns false on any check failure delta
void run_mode(const ModeInfo& mi, bool write_golden) {
  const uint32_t kPackets = 10;
  ShellHarness h;
  h.reset();

  zref::PadSnapshot pad_oracle;
  Pcg32 rngA(0xA110 + mi.mode, 21), rngB(0xB220 + mi.mode, 23);

  std::vector<uint8_t> slot_mirror[2];
  slot_mirror[0].assign(zref::render::kSlotBytes, 0);
  slot_mirror[1].assign(zref::render::kSlotBytes, 0);

  const zhao_abi::video_mode vm = zhao_abi::video_mode(mi.mode);
  const uint32_t crc_black_z60 =
      zref::render::displayed_crc32c(zhao_abi::VIDEO_Z60, slot_mirror[0].data());
  const uint32_t crc_black_mode = zref::render::displayed_crc32c(vm, slot_mirror[0].data());

  std::vector<uint32_t> expect_crc;
  expect_crc.push_back(crc_black_z60);   // F0: Z60 boot frame
  expect_crc.push_back(crc_black_mode);  // F1: first frame under the mode
  expect_crc.push_back(crc_black_mode);  // F2: its repeat
  size_t crc_checked = 0;
  // The same pictures with consecutive repeats collapsed: WHAT must be shown
  // and IN WHAT ORDER, independent of how the cadence repeats them. See the
  // drain loop for why the two are separated.
  //
  // BUILT BY COLLAPSING, not by hand. In Z60 the boot frame and the first
  // frame under the mode are the SAME picture (mode 0 IS Z60), so pushing
  // both would leave a duplicate here that the observed side correctly
  // collapses away -- and every later comparison would read one ahead. The
  // expectation has to be collapsed by exactly the rule the observation is.
  std::vector<uint32_t> expect_distinct;
  const auto push_distinct = [&expect_distinct](uint32_t c) {
    if (expect_distinct.empty() || expect_distinct.back() != c) expect_distinct.push_back(c);
  };
  push_distinct(crc_black_z60);
  push_distinct(crc_black_mode);
  size_t distinct_seen = 0;

  std::vector<std::vector<uint8_t>> packets;  // sealed bytes, in order
  std::vector<uint32_t> fresh_crcs;           // displayed CRC per packet
  std::vector<uint8_t> pad_wire_log;

  uint64_t starve_baseline = ~0ull;
  size_t fences_checked = 0;

  {
    PacketSpec s;
    s.frame_id = 0;
    s.sequence = 0;
    s.mode = mi.mode;
    check(h.publish(0, build_packet(s)), "publish P0", 1, 1);
  }

  // ---- DOES THE STREAMING BLIT SHIFT THIS MODE'S PHASE? -------------------
  // The DEBUG.FRAMEBLIT redesign completes a canvas ~58k gpu cycles earlier
  // than the serial CMD.DMA path. Whether that moves the DISPLAYED phase
  // depends on whether the earlier completion crosses a tick boundary, and
  // tick boundaries are the mode's frame period:
  //
  //     Z60   251,520 gpu cycles/frame
  //     Storm 217,984
  //     Duo   318,592
  //
  // MEASURED 2026-08-21 across all three modes and every tick: ONLY DUO
  // SHIFTS. Z60 and Storm keep the original cadence exactly.
  //
  // This is a per-mode measurement, not a formula derived from first
  // principles: the discriminant is where the completion falls inside the
  // period, and a future timing change can move any mode across it. If a
  // mode's cadence changes, this flag is what must be re-measured -- the
  // assertions below already say which way each answer looks.
  const bool blit_shifts_phase = (mi.mode == 2);  // Duo only

  std::vector<uint8_t> canvas;
  uint32_t published = 0;
  const uint32_t last_tick = 2 * kPackets + 2;
  uint32_t ticks_processed = 0;

  while (ticks_processed < last_tick && zhao::check_failures() < 40) {
    h.step();
    if (!h.tick_seen_last_step) continue;
    const TickEvent tk = h.ticks.back();
    const uint32_t k = tk.frame_id;
    ++ticks_processed;

    // Repeats on EVEN ticks unshifted; on ODD ticks once the blit lands a
    // frame earlier. The tail tick repeats either way: after the last publish
    // there is nothing further to show.
    const bool exp_rep = blit_shifts_phase ? ((k == 0) || ((k & 1u) == 1) || (k == last_tick))
                                           : ((k <= 2) || ((k & 1u) == 0));
    check(tk.repeated == exp_rep, "golden: repeated law", exp_rep, tk.repeated);

    // pads latched every tick; publish + oracle latch on this tick's pads
    {
      zref::PadRawState raw[4] = {zref::absentPad(), zref::absentPad(), zref::absentPad(),
                                  zref::absentPad()};
      const auto stick = [](Pcg32& r) { return int16_t(int32_t(r.next() & 0xFFFF) - 0x8000); };
      raw[0].present = true;
      raw[0].buttons = rngA.next() & 0xFFFFu;
      raw[0].lx = stick(rngA);
      raw[0].ly = stick(rngA);
      raw[1].present = true;
      raw[1].buttons = rngB.next() & 0xFFFFu;
      raw[1].lx = stick(rngB);
      raw[1].ly = stick(rngB);
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

    if ((k & 1u) == 1 && published < kPackets) {
      ++published;
      const uint32_t f = published;
      compose_pattern(canvas, vm, f);
      const uint8_t dst = uint8_t(f & 1u);
      const uint32_t arena = (f & 1u) ? kArena1 : kArena0;
      const uint32_t blit_crc = zhao_abi::zhao_crc32c(0, canvas.data(), canvas.size());
      h.mem_write(arena, canvas);
      std::memcpy(slot_mirror[dst].data(), canvas.data(), canvas.size());
      const uint32_t dcrc = zref::render::displayed_crc32c(vm, slot_mirror[dst].data());
      expect_crc.push_back(dcrc);  // fresh F_{2f+1}
      expect_crc.push_back(dcrc);  // repeat F_{2f+2} (60 Hz law, identical)
      push_distinct(dcrc);
      fresh_crcs.push_back(dcrc);

      const zref::PadFrame* of = pad_oracle.frames();
      for (int p = 0; p < 2; ++p) {
        uint8_t wire[20];
        zref::padFrameToWire(of[p], wire);
        pad_wire_log.insert(pad_wire_log.end(), wire, wire + 20);
      }

      PacketSpec s;
      s.frame_id = f;
      s.sequence = f;
      s.mode = mi.mode;
      s.has_blit = true;
      s.blit_dst = dst;
      s.blit_src = arena;
      s.blit_len = uint32_t(canvas.size());
      s.blit_crc = blit_crc;
      std::vector<uint8_t> pkt = build_packet(s);
      packets.push_back(pkt);
      check(h.publish(int(f % 3), pkt), "golden: publish", 1, 1);
    }

    // CONTENT IS CHECKED INDEPENDENTLY OF REPEAT PLACEMENT, deliberately.
    //
    // This compared h.crcs[i] == expect_crc[i], welding WHICH PICTURE to WHICH
    // FRAME IT LANDED ON. The streaming blitter moves the Duo phase by one
    // frame and ten of these fired at once -- every `got` being the NEXT
    // expected value, i.e. the right picture arriving early. A test shaped
    // that way turns a latency improvement into a correctness failure, and
    // charter 25 now forbids reverting the improvement to silence it.
    //
    // Split into the two claims it conflated: every displayed frame is either
    // a repeat of the one before it or the next distinct picture IN ORDER
    // (exact, true at any cadence), while the cadence itself is pinned by the
    // repeat law and the deadline_faults form above -- which DO move, per
    // mode, and are supposed to say so.
    //
    // Nothing is weakened: a wrong picture, one out of order, a dropped one
    // and an extra one all still fail.
    while (crc_checked < h.crcs.size()) {
      const uint32_t got = h.crcs[crc_checked];
      if (crc_checked > 0 && got == h.crcs[crc_checked - 1]) {
        ++crc_checked;  // a lawful repeat of the frame before it
        continue;
      }
      check(distinct_seen < expect_distinct.size(),
            "golden: no picture beyond the expected sequence", 1,
            distinct_seen < expect_distinct.size() ? 1 : 0);
      if (distinct_seen < expect_distinct.size()) {
        check(got == expect_distinct[distinct_seen], "golden: displayed CRC (distinct, in order)",
              expect_distinct[distinct_seen], got);
      }
      ++distinct_seen;
      ++crc_checked;
    }

    // counters (the closed forms of the half-rate cadence; no rumble here,
    // so a marker packet carries 4 records)
    if (k >= 2 && !h.sweeps.empty()) {
      const SweepEvent& sw = h.sweeps.back();
      const uint32_t sk = k - 1;
      if (sw.bank.size() == 40) {
        check(sw.bank[0] == sk, "golden: frame_cycles", sk, sw.bank[0]);
        // Counts the repeat ticks, so it follows the cadence above.
        const uint64_t faults =
            blit_shifts_phase ? ((sk == 0) ? 0 : ((sk + 1) / 2)) : ((sk <= 1) ? sk : (1 + sk / 2));
        check(sw.bank[1] == faults, "golden: deadline_faults", faults, sw.bank[1]);
        const uint64_t cmds = 3 + 4ull * (sk / 2);
        check(sw.bank[2] == cmds, "golden: commands", cmds, sw.bank[2]);
        check(sw.bank[31] == 0, "golden: underruns", 0, sw.bank[31]);
        check(sw.bank[35] == 0, "golden: gaps", 0, sw.bank[35]);
        check(sw.bank[36] == 0, "golden: rumble drops", 0, sw.bank[36]);
        // Boot transient (mode-flush refetch) settles by tick 2 in RAW
        // sampling. IT IS TICK 3 NOW, and the extra tick is the point of the
        // change rather than a regression: bank[30] no longer samples the
        // vid-domain counter across the clock boundary on a gpu tick. It
        // reports a COHERENT SNAPSHOT published once per vid frame and
        // collected through a toggle handshake, so the first real reading
        // lands one frame later than a torn read did.
        //
        // Caught by this test at tick 2 reading 0 (no snapshot collected
        // yet) and then 0x280 once one arrived -- and 0x280 = 640 is inside
        // this very check's own <= 1024 "two lines" tolerance, so the value
        // was never wrong. Only its arrival moved.
        if (starve_baseline == ~0ull && sk >= 3) {
          starve_baseline = sw.bank[30];
          check(starve_baseline <= 1024, "golden: starvation baseline <= two lines", 1,
                starve_baseline <= 1024);
        }
        if (starve_baseline != ~0ull) {
          check(sw.bank[30] == starve_baseline, "golden: starvation constant", starve_baseline,
                sw.bank[30]);
        }
      }
    }

    // Fence 0 still misses: P0 dispatches with nothing in flight to overlap.
    // Every fence from 1 on closes CLEAN under the streaming blitter, which is
    // the result, not a relaxation -- a regression that reintroduced the
    // deadline miss fails the else-branch here.
    while (fences_checked < h.fence_log.size()) {
      const size_t i = fences_checked++;
      // Only the shifted mode gains the clean close, and only from fence 1:
      // P0 dispatches with nothing in flight to overlap.
      if (blit_shifts_phase && i > 0) {
        check(h.fence_log[i].ok && h.fence_log[i].status == 0, "golden: fence closes clean", 0,
              h.fence_log[i].status);
      } else {
        check(!h.fence_log[i].ok && h.fence_log[i].status == 16, "golden: fence pinned deadline",
              16, h.fence_log[i].status);
      }
    }

    check(h.sticky_errors() == 0, "golden: sticky flags clear", 0, h.sticky_errors());
  }

  // drain the last frames' CRC pulses
  {
    uint64_t guard_steps = 0;
    while (crc_checked < expect_crc.size() && guard_steps < 2'000'000) {
      h.step();
      ++guard_steps;
      while (crc_checked < h.crcs.size() && crc_checked < expect_crc.size()) {
        check(h.crcs[crc_checked] == expect_crc[crc_checked], "golden: displayed CRC (tail)",
              expect_crc[crc_checked], h.crcs[crc_checked]);
        ++crc_checked;
      }
    }
    check(crc_checked == expect_crc.size(), "golden: all frames displayed", expect_crc.size(),
          crc_checked);
  }
  check(h.blit_log.size() == kPackets, "golden: one blit per packet", kPackets, h.blit_log.size());
  for (const auto& bd : h.blit_log) {
    check(bd.status == 0, "golden: blit committed", 0, bd.status);
  }

  if (zhao::check_failures() != 0) return;

  // ---- the capture ---------------------------------------------------------
  const std::string path = std::string(ZHAO_GOLDEN_DIR) + "/" + mi.name + "_10frame.zcap";
  const std::string tmp = path + ".regen";
  {
    zhao::ZhaoZcapWriter w(tmp);
    w.add_section(zhao::ZHAO_ZCAP_ABI_INFO, 1, zhao::zhao_zcap_build_abi_info());
    for (uint32_t f = 0; f < kPackets; ++f) {
      w.add_section(zhao::ZHAO_ZCAP_FRAME_PACKET, 1, packets[f]);
      uint8_t fb[14] = {0};
      fb[0] = mi.mode;
      fb[1] = mi.views;
      fb[4] = uint8_t(mi.width & 0xFF);
      fb[5] = uint8_t(mi.width >> 8);
      fb[6] = uint8_t(240 & 0xFF);
      const uint32_t c = fresh_crcs[f];
      fb[10] = uint8_t(c & 0xFF);
      fb[11] = uint8_t((c >> 8) & 0xFF);
      fb[12] = uint8_t((c >> 16) & 0xFF);
      fb[13] = uint8_t((c >> 24) & 0xFF);
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
      struct Ent {
        uint16_t id;
        uint64_t v;
      };
      const Ent ents[] = {
          {0, 2 * kPackets + 2},
          // Only the shifted mode records one fewer. last_tick here is EVEN,
          // where the two closed forms differ -- unlike duo_markers, whose
          // final tick is odd and where they agree. So the DUO capture's
          // counter moves 12 -> 11; z60 and storm are untouched.
          {1, blit_shifts_phase ? uint64_t((2 * kPackets + 2 + 1) / 2)
                                : uint64_t(1 + (2 * kPackets + 2) / 2)},
          {30, starve_baseline},
          {31, 0},
          {35, 0},
          {36, 0},
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
    check(w.close(), "golden: zcap written", 1, 1);
  }
  const std::vector<uint8_t> regen = read_file(tmp);
  check(!regen.empty(), "golden: regen readable", 1, !regen.empty());
  if (write_golden) {
    FILE* f = std::fopen(path.c_str(), "wb");
    check(f != nullptr, "golden: writable", 1, f != nullptr);
    if (f) {
      std::fwrite(regen.data(), 1, regen.size(), f);
      std::fclose(f);
    }
    std::printf("[shell_golden] wrote %s (%zu bytes)\n", path.c_str(), regen.size());
  } else {
    const std::vector<uint8_t> committed = read_file(path);
    check(!committed.empty(), "golden: committed capture exists", 1, !committed.empty());
    check(committed == regen, "golden: byte-identical to committed", 1, committed == regen);
  }
  std::remove(tmp.c_str());
  std::printf("[shell_golden] %s: ticks=%zu crcs=%zu\n", mi.name, h.ticks.size(), h.crcs.size());
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool write_golden = false;
  const char* only = nullptr;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--write"))
      write_golden = true;
    else if (!std::strcmp(argv[i], "--mode") && i + 1 < argc)
      only = argv[++i];
  }
  for (const ModeInfo& mi : kModes) {
    if (only && std::strcmp(only, mi.name) != 0) continue;
    run_mode(mi, write_golden);
  }
  return zhao::report_and_exit("shell_golden");
}
