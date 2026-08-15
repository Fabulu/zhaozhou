// input_rumble_directed.cpp — INPUT.RUMBLE directed vectors (W2.3).
//
// Law: spec/input_rules.md 3 / contract INPUT.RUMBLE.md.
//   1. reset: duties 0 (motors off), PWM phase 0, dropped 0
//   2. DebugRumble -> duty latches at the NEXT frame_tick (frame-gated;
//      mid-frame the previous target holds)
//   3. PWM duty stream: over one 256-phase carrier period the high count is
//      exactly `strength`, bit-exact vs the zref carrier model every cycle
//   4. two commands for one pad in a frame -> last writer wins at the latch
//      + rumble_frames_dropped counts the dropped one (three -> two drops)
//   5. pad_index > 3 -> request dropped entirely + counted, no wrap onto a pad
//   6. enable=0 forces duty 0 regardless of strength; strength 0 = off
//   7. hold: frames without commands keep the previous target (no timeout)
// Every step is mirrored by zref::RumbleBridge.

#include <cstdio>

#include "input_rumble_sim.hpp"
#include "zhao_sim.hpp"

using zhao_input::edge;

static Vzhao_input_rumble top;
static zref::RumbleBridge oracle;
static uint64_t cycles = 0;  // rising edges since reset release (PWM phase)

static void cyc() {
  edge(top);
  ++cycles;
}

static void cmd(uint8_t pad_index, uint8_t enable, uint8_t strength) {
  top.rumble_cmd_valid = 1;
  top.rumble_pad_index = pad_index;
  top.rumble_enable = enable;
  top.rumble_strength = strength;
  cyc();
  top.rumble_cmd_valid = 0;
  oracle.command(pad_index, enable, strength);
}

static void frameTick(uint32_t frame_id) {
  top.frame_tick = zhao_input::tickWord(true, frame_id, false);
  cyc();
  top.frame_tick = zhao_input::tickWord(false, frame_id, false);
  top.eval();
  oracle.tick();
}

static void checkVsOracle(const char* what) {
  for (int i = 0; i < 4; ++i) {
    zhao::check(top.rumble_duty[i] == oracle.duty(i), what, oracle.duty(i),
                top.rumble_duty[i]);
    const uint8_t active = static_cast<uint8_t>((top.rumble_active >> i) & 1u);
    zhao::check(active == (oracle.duty(i) != 0 ? 1u : 0u), "active == duty!=0",
                oracle.duty(i) != 0 ? 1 : 0, active);
  }
  zhao::check(top.rumble_frames_dropped == oracle.droppedShadow(),
              "rumble_frames_dropped shadow matches oracle", oracle.droppedShadow(),
              top.rumble_frames_dropped);
}

static void checkPwmAll() {
  for (int i = 0; i < 4; ++i) {
    const bool want = zref::RumbleBridge::pwm(oracle.duty(i), cycles);
    const bool got = (top.rumble_pwm & (1u << i)) != 0;
    zhao::check(got == want, "PWM carrier bit-exact vs model (pad)", want, got);
  }
}

int main() {
  // ---------------------------------------------------------- 1. reset ----
  zhao_input::resetRumble(top);
  oracle.reset();
  cycles = 0;
  checkVsOracle("reset duties 0");
  zhao::check(top.rumble_frames_dropped == 0, "reset dropped == 0", 0,
              top.rumble_frames_dropped);
  checkPwmAll();

  // ---------------------------------------- 2. frame-gated first latch ---
  cmd(0, 1, 200);
  checkVsOracle("pending command does not touch duty before the tick");
  const uint8_t duty_before = top.rumble_duty[0];
  zhao::check(duty_before == 0, "duty still 0 between command and tick", 0, duty_before);
  for (int i = 0; i < 5; ++i) {  // idle cycles: still gated
    cyc();
    checkPwmAll();
    zhao::check(top.rumble_duty[0] == 0, "mid-frame duty unchanged", 0, top.rumble_duty[0]);
  }
  frameTick(1);
  checkVsOracle("duty latched at the tick after the command");
  zhao::check(top.rumble_duty[0] == 200, "duty == strength when enabled", 200,
              top.rumble_duty[0]);

  // ----------------------------------------------- 3. PWM duty stream ----
  // one full carrier period: exactly `strength` high phases per pad, and the
  // bit stream equals the zref model cycle-for-cycle.
  {
    unsigned highs = 0;
    for (int p = 0; p < 256; ++p) {
      cyc();
      checkPwmAll();
      highs += (top.rumble_pwm & 1u) != 0;
    }
    zhao::check(highs == 200, "PWM high phases == strength over one period", 200, highs);
  }
  frameTick(2);  // hold through a frame with no command
  checkVsOracle("no command: previous target HOLDS");
  zhao::check(top.rumble_duty[0] == 200, "hold: duty unchanged", 200, top.rumble_duty[0]);

  // ------------------------------- 4. double command: last wins + count ---
  cmd(0, 1, 100);
  cmd(0, 1, 50);  // replaces the pending 100: one dropped update
  frameTick(3);
  checkVsOracle("last writer wins at the latch");
  zhao::check(top.rumble_duty[0] == 50, "duty == 50 (last command)", 50, top.rumble_duty[0]);
  zhao::check(top.rumble_frames_dropped == 1, "one dropped update counted", 1,
              top.rumble_frames_dropped);
  cmd(1, 1, 10);
  cmd(1, 1, 20);
  cmd(1, 1, 30);  // three commands one frame: two dropped
  frameTick(4);
  checkVsOracle("triple command: last wins, two dropped");
  zhao::check(top.rumble_duty[1] == 30, "pad 1 duty == 30", 30, top.rumble_duty[1]);
  zhao::check(top.rumble_frames_dropped == 3, "total drops 1+2", 3, top.rumble_frames_dropped);

  // ------------------------------------- 5. bad pad_index: drop + count ---
  const uint8_t duty_snapshot[4] = {top.rumble_duty[0], top.rumble_duty[1],
                                    top.rumble_duty[2], top.rumble_duty[3]};
  cmd(7, 1, 255);
  cmd(4, 1, 255);
  frameTick(5);
  checkVsOracle("out-of-range index dropped + counted");
  for (int i = 0; i < 4; ++i) {
    zhao::check(top.rumble_duty[i] == duty_snapshot[i],
                "bad index never wraps onto another pad", duty_snapshot[i],
                top.rumble_duty[i]);
  }
  zhao::check(top.rumble_frames_dropped == 5, "total drops 3+2", 5,
              top.rumble_frames_dropped);

  // ------------------------------- 6. enable / strength corner behaviour --
  cmd(2, 0, 200);  // enable=0 forces duty 0 regardless of strength
  frameTick(6);
  checkVsOracle("enable=0 -> duty 0");
  zhao::check(top.rumble_duty[2] == 0, "disabled: duty 0", 0, top.rumble_duty[2]);
  zhao::check((top.rumble_active & 4u) == 0, "disabled: inactive", 0, (top.rumble_active & 4u));

  cmd(2, 1, 0);  // enabled, strength 0 = off
  frameTick(7);
  checkVsOracle("strength 0 -> duty 0");
  zhao::check(top.rumble_duty[2] == 0 && (top.rumble_active & 4u) == 0,
              "strength 0: off", 0, (top.rumble_active & 4u));

  cmd(2, 1, 255);  // max duty ~99.6%
  frameTick(8);
  checkVsOracle("strength 255 -> duty 255");
  zhao::check(top.rumble_duty[2] == 255, "max duty 255", 255, top.rumble_duty[2]);
  {  // carrier: 255 of 256 phases high
    unsigned highs = 0;
    for (int p = 0; p < 256; ++p) {
      cyc();
      checkPwmAll();
      highs += (top.rumble_pwm & 4u) != 0;
    }
    zhao::check(highs == 255, "duty 255: 255 high phases per period", 255, highs);
  }

  // --------------------------------------------------- 7. hold, no timeout -
  for (int f = 0; f < 10; ++f) {
    frameTick(9 + f);
  }
  checkVsOracle("10 frames without commands: targets hold (no auto-stop)");
  zhao::check(top.rumble_duty[0] == 50 && top.rumble_duty[1] == 30 &&
                  top.rumble_duty[2] == 255 && top.rumble_duty[3] == 0,
              "hold: all duties unchanged", 1,
              (top.rumble_duty[0] == 50 && top.rumble_duty[1] == 30 &&
               top.rumble_duty[2] == 255 && top.rumble_duty[3] == 0)
                  ? 1
                  : 0);
  zhao::check(top.rumble_frames_dropped == 5, "no new drops while holding", 5,
              top.rumble_frames_dropped);

  // mid-frame duty stability with pending commands in flight
  cmd(3, 1, 77);
  for (int i = 0; i < 8; ++i) {
    cyc();
    checkPwmAll();
    zhao::check(top.rumble_duty[3] == 0, "pending pad 3 duty stable until tick", 0,
                top.rumble_duty[3]);
  }
  frameTick(20);
  checkVsOracle("delayed tick still applies the pending target");
  zhao::check(top.rumble_duty[3] == 77, "pad 3 duty 77", 77, top.rumble_duty[3]);

  return zhao::report_and_exit("input_rumble_directed");
}
