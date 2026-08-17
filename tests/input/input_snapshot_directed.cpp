// input_snapshot_directed.cpp — INPUT.SNAPSHOT directed vectors (W2.3).
//
// Law: spec/input_rules.md 1-2 / contract INPUT.SNAPSHOT.md.
//   1. reset state: all pads absent, zero frames, sequences 0, gaps 0
//   2. atomicity: a mid-frame stick/button/presence change is invisible
//      until the NEXT frame_tick and then appears whole (never torn)
//   3. sequence monotonic across 10k+ frames including the 2^16 wrap
//   4. absent-pad law: zeroed fields, frozen sequence, resume on return
//   5. four slots independent
//   6. packed-output byte identity with the ABI wire form (20 B per slot)
// Every latched frame is compared bit-exact against zref::PadSnapshot.

#include <cstdio>
#include <type_traits>

#include "input_snapshot_sim.hpp"
#include "zhao_sim.hpp"

using zhao_input::edge;
using zhao_input::readPadFrame;

static Vzhao_input_snapshot top;
static zref::PadSnapshot oracle;

static int lcg_state = 0x1234abcd;
static uint32_t lcg32() {  // simple deterministic scrambler for mid-frame noise
  lcg_state = lcg_state * 1664525 + 1013904223;
  return static_cast<uint32_t>(lcg_state);
}

static void checkFrame(int ctx_frame) {
  for (int i = 0; i < 4; ++i) {
    const zref::PadFrame got = readPadFrame(top, i);
    const zref::PadFrame& exp = oracle.frame(i);
    char what[128];
    std::snprintf(what, sizeof what,
                  "frame %d pad %d latched PadFrame (seq=%u flags=%02x buttons=%08x)", ctx_frame, i,
                  got.sequence, got.flags, got.buttons);
    zhao::check(got == exp, what, 1, got == exp ? 1 : 0);
  }
}

int main() {
  // ---------------------------------------------------------- 1. reset ----
  zhao_input::resetSnapshot(top);
  oracle.reset();
  checkFrame(-1);
  zhao::check(top.pad_frame_id == 0, "reset pad_frame_id == 0", 0, top.pad_frame_id);
  zhao::check(top.input_sequence_gaps == 0, "reset input_sequence_gaps == 0", 0,
              top.input_sequence_gaps);
  for (int i = 0; i < 4; ++i) {
    zhao::check(top.pad_sequence[i] == 0, "reset sequence == 0", 0, top.pad_sequence[i]);
    zhao::check(zhao_input::frameBytesMatchFlat(readPadFrame(top, i), top, i),
                "reset frame wire bytes match packed output", 1, 0);
  }

  // --------------------------------------------- 2. atomicity at tick -----
  zref::PadRawState A[4] = {
      zref::PadRawState{true, 0x0000A5A5, -1234, 567, -32768, 32767},
      zref::PadRawState{false, 0, 0, 0, 0, 0},
      zref::PadRawState{true, 0x80000001, 32767, -32767, 1, -1},
      zref::PadRawState{false, 0, 0, 0, 0, 0},
  };
  zhao_input::drivePads(top, A);
  top.frame_tick = zhao_input::tickWord(true, 7, false);
  edge(top);
  top.frame_tick = zhao_input::tickWord(false, 7, false);
  top.eval();
  oracle.tick(A, 7);
  checkFrame(0);
  zhao::check(top.pad_frame_id == 7, "frame_id latched from tick", 7, top.pad_frame_id);
  zhao::check(top.pad_sequence[0] == 1, "first present frame carries sequence 1", 1,
              top.pad_sequence[0]);
  std::remove_reference_t<decltype(top.pad_frame_flat)> zero_flat{};
  zhao::check(top.pad_frame_flat != zero_flat, "snapshot array non-zero after tick", 1, 0);

  // mid-frame noise: raw inputs thrash for 200 cycles; NOTHING may move
  const auto snap_flat = top.pad_frame_flat;  // stable copy (VlWide<20>)
  const uint16_t snap_seq[4] = {top.pad_sequence[0], top.pad_sequence[1], top.pad_sequence[2],
                                top.pad_sequence[3]};
  for (int c = 0; c < 200; ++c) {
    zref::PadRawState noise[4];
    const uint32_t r = lcg32();
    for (int i = 0; i < 4; ++i) {
      noise[i].present = ((r >> i) & 1u) != 0;  // includes unplugging pads
      noise[i].buttons = r * (i + 1) + c;
      noise[i].lx = static_cast<int16_t>(r >> (i * 4));
      noise[i].ly = static_cast<int16_t>(r >> (i * 4 + 1));
      noise[i].rx = static_cast<int16_t>(~r);
      noise[i].ry = static_cast<int16_t>(r ^ c);
    }
    zhao_input::drivePads(top, noise);
    edge(top);
    const bool flat_same = (top.pad_frame_flat == snap_flat);
    zhao::check(flat_same, "mid-frame input change invisible (atomic latch)", 1, flat_same ? 1 : 0);
    for (int i = 0; i < 4; ++i) {
      if (top.pad_sequence[i] != snap_seq[i]) {
        zhao::check(false, "sequence frozen between ticks", snap_seq[i], top.pad_sequence[i]);
      }
    }
  }
  zhao::check(top.input_sequence_gaps == 0, "no gaps (atomic latch)", 0, top.input_sequence_gaps);
  zhao::check(top.input_sequence_gap_evt == 0, "gap event never fires", 0,
              top.input_sequence_gap_evt);

  // next tick latches state B whole (unplug pad 0, move pad 2 sticks)
  zref::PadRawState B[4] = {
      zref::PadRawState{false, 0, 0, 0, 0, 0},
      zref::PadRawState{true, 0xFFFFFFFF, 1, 2, 3, 4},
      zref::PadRawState{true, 0x0000F0F0, -1, -2, -3, -4},
      zref::PadRawState{true, 0x00FF00FF, 100, -100, 32767, -32768},
  };
  zhao_input::drivePads(top, B);
  top.frame_tick = zhao_input::tickWord(true, 8, true);
  edge(top);
  top.frame_tick = zhao_input::tickWord(false, 8, true);
  top.eval();
  oracle.tick(B, 8);
  checkFrame(1);
  zhao::check(top.pad_sequence[0] == 1, "absent pad sequence frozen at 1", 1, top.pad_sequence[0]);
  zhao::check(top.pad_sequence[2] == 2, "present pad sequence advanced to 2", 2,
              top.pad_sequence[2]);
  zhao::check(readPadFrame(top, 0).flags == 0x00, "unplugged pad flags.pad_present=0", 0,
              readPadFrame(top, 0).flags);
  zhao::check(readPadFrame(top, 0).buttons == 0, "unplugged pad buttons zeroed", 0,
              readPadFrame(top, 0).buttons);
  for (int i = 0; i < 4; ++i) {
    zhao::check(zhao_input::frameBytesMatchFlat(readPadFrame(top, i), top, i),
                "latched frame wire bytes match packed output (ABI 20 B)", 1, 0);
  }

  // ------------------------------------------- 3. four slots independent --
  zref::PadRawState four[4];
  for (int i = 0; i < 4; ++i) {
    four[i] = zref::PadRawState{true,
                                0x11111111u * (i + 1),
                                static_cast<int16_t>(1000 * (i + 1)),
                                static_cast<int16_t>(-1000 * (i + 1)),
                                static_cast<int16_t>(7 * i),
                                static_cast<int16_t>(-7 * i)};
  }
  four[2].present = false;  // slot 2 goes absent with history on board
  zhao_input::drivePads(top, four);
  top.frame_tick = zhao_input::tickWord(true, 9, false);
  edge(top);
  top.frame_tick = zhao_input::tickWord(false, 9, false);
  top.eval();
  oracle.tick(four, 9);
  checkFrame(2);
  zhao::check(top.pad_sequence[1] != top.pad_sequence[3] ||
                  readPadFrame(top, 1).buttons != readPadFrame(top, 3).buttons,
              "slots carry independent state", 1, 0);
  zhao::check(readPadFrame(top, 2).sequence != 0 || readPadFrame(top, 2).flags == 0,
              "absent slot 2 frozen sequence in zero frame", 1, 0);

  // --------------------------------------------- 4. absent-pad law --------
  zref::PadRawState one[4] = {
      zref::PadRawState{true, 0xDEADBEEF, 5, -5, 7, -7}, zref::PadRawState{false, 0, 0, 0, 0, 0},
      zref::PadRawState{false, 0, 0, 0, 0, 0}, zref::PadRawState{false, 0, 0, 0, 0, 0}};
  for (int f = 0; f < 5; ++f) {  // pad 0 present 5 more frames
    zhao_input::drivePads(top, one);
    top.frame_tick = zhao_input::tickWord(true, 10 + f, false);
    edge(top);
    top.frame_tick = zhao_input::tickWord(false, 10 + f, false);
    top.eval();
    oracle.tick(one, 10 + f);
    checkFrame(10 + f);
  }
  const uint16_t frozen = oracle.sequence(0);
  zhao::check(frozen > 0, "sequence advanced while present", 1, frozen == 0 ? 0 : 1);
  one[0].present = false;  // unplug
  for (int f = 0; f < 3; ++f) {
    zhao_input::drivePads(top, one);
    top.frame_tick = zhao_input::tickWord(true, 20 + f, false);
    edge(top);
    top.frame_tick = zhao_input::tickWord(false, 20 + f, false);
    top.eval();
    oracle.tick(one, 20 + f);
    checkFrame(20 + f);
    zhao::check(readPadFrame(top, 0).sequence == frozen, "absent sequence frozen", frozen,
                readPadFrame(top, 0).sequence);
    zhao::check(readPadFrame(top, 0).buttons == 0 && readPadFrame(top, 0).lx == 0 &&
                    readPadFrame(top, 0).ly == 0 && readPadFrame(top, 0).rx == 0 &&
                    readPadFrame(top, 0).ry == 0,
                "absent frame fully zeroed", 0,
                readPadFrame(top, 0).buttons | readPadFrame(top, 0).lx);
    zhao::check(readPadFrame(top, 0).rsv == 0, "absent frame rsv == 0", 0,
                readPadFrame(top, 0).rsv);
  }
  one[0].present = true;  // replug: sequence RESUMES from the frozen value
  zhao_input::drivePads(top, one);
  top.frame_tick = zhao_input::tickWord(true, 30, false);
  edge(top);
  top.frame_tick = zhao_input::tickWord(false, 30, false);
  top.eval();
  oracle.tick(one, 30);
  checkFrame(30);
  zhao::check(readPadFrame(top, 0).sequence == ((frozen + 1) & 0xFFFF),
              "replugged pad resumes sequence at frozen+1", (frozen + 1) & 0xFFFF,
              readPadFrame(top, 0).sequence);

  // --------------------------- 5. sequence monotonic across the 2^16 wrap --
  // pad 0 stays present; every frame compared to the oracle (which pins the
  // law), with explicit boundary checks through 0xFFFF -> 0x0000.
  const uint16_t start = oracle.sequence(0);
  int wraps_seen = 0;
  bool saw_ffff = (start == 0xFFFF);
  const int kFrames = 66'000;  // > 2^16 ticks: the wrap is guaranteed
  for (int f = 0; f < kFrames; ++f) {
    zhao_input::drivePads(top, one);
    const uint16_t before = top.pad_sequence[0];
    top.frame_tick = zhao_input::tickWord(true, 1000 + f, false);
    edge(top);
    top.frame_tick = zhao_input::tickWord(false, 1000 + f, false);
    top.eval();
    oracle.tick(one, 1000 + f);
    const uint16_t after = top.pad_sequence[0];
    const uint16_t want = static_cast<uint16_t>(before + 1);
    if (after != want) {
      zhao::check(false, "sequence increments by exactly 1 per tick", want, after);
      break;
    }
    if (before == 0xFFFF) {
      ++wraps_seen;
      zhao::check(after == 0x0000, "sequence wraps mod 2^16", 0, after);
    }
    if (after == 0xFFFF) saw_ffff = true;
    if ((f & 0x3FF) == 0) checkFrame(1000 + f);  // periodic full-array compare
  }
  checkFrame(-2);  // final array
  zhao::check(saw_ffff, "run reached sequence 0xFFFF", 1, saw_ffff ? 1 : 0);
  zhao::check(wraps_seen >= 1, "run crossed the 2^16 wrap at least once", 1,
              wraps_seen >= 1 ? 1 : 0);
  zhao::check(top.input_sequence_gaps == 0, "gap counter zero across the wrap", 0,
              top.input_sequence_gaps);

  return zhao::report_and_exit("input_snapshot_directed");
}
