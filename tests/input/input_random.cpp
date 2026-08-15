// input_random.cpp — INPUT subsystem random differential (W2.3).
//
// Law: spec/input_rules.md (whole file). PCG pad timelines + DebugRumble
// command timelines are driven into BOTH the Verilated RTL
// (zhao_input_snapshot + zhao_input_rumble) and the zref oracles
// (zref::PadSnapshot + zref::RumbleBridge):
//
//   - per frame: presence toggles, mid-frame change points (atomicity under
//     noise), 0-4 rumble commands per pad (valid + out-of-range indices,
//     duplicate-pad replacements), random frame lengths
//   - compared at every frame_tick: all four latched PadFrames (bit-exact),
//     sequences, frame_id, gap counter, duty table, drop shadow
//   - compared at EVERY cycle: the PWM carrier bits vs the zref model
//   - run-twice determinism: the whole timeline runs twice from the same
//     seed and the transcript hash must be identical (plan R1)
//
// Modes: default 1,000 frames (fast); `--frames 100000` (nightly soak).
// Failing vectors are saved by the harness registry (charter 29-17).

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "input_rumble_sim.hpp"
#include "input_snapshot_sim.hpp"
#include "zhao_sim.hpp"

using zhao_input::edge;

// ---- PCG32 (Melissa O'Neill) — the wave-1 PCG lane convention -----------
struct Pcg32 {
  uint64_t state;
  uint64_t inc;
  uint32_t next() {
    const uint64_t old = state;
    state = old * 6364136223846793005ull + inc;
    const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18) ^ old) >> 27);
    const uint32_t rot = static_cast<uint32_t>(old >> 59);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
  }
  uint32_t operator()(uint32_t bound) { return next() % bound; }
  uint32_t bit() { return next() & 1u; }
};

struct BothTops {
  Vzhao_input_snapshot snap;
  Vzhao_input_rumble rum;
};

struct RumbleCmd {
  uint8_t pad_index;
  uint8_t enable;
  uint8_t strength;
};

static const int kMaxCmdsPerFrame = 16;

// one full random timeline; returns the transcript hash and accumulates
// zhao::check failures on any divergence RTL vs oracle
static uint64_t runTimeline(uint32_t frames, uint64_t seed) {
  BothTops tops;
  zref::PadSnapshot padOracle;
  zref::RumbleBridge rumOracle;
  Pcg32 rng{seed, (seed << 1) | 1u};

  zhao_input::resetSnapshot(tops.snap);
  zhao_input::resetRumble(tops.rum);
  padOracle.reset();
  rumOracle.reset();
  uint64_t cycles = 0;  // rising edges since reset release (PWM phase)

  zref::PadRawState pads[4];
  for (int i = 0; i < 4; ++i) pads[i] = zref::absentPad();

  auto cyc = [&]() {
    edge(tops.snap);
    edge(tops.rum);
    ++cycles;
    // differential PWM compare EVERY cycle (carrier never resets)
    for (int i = 0; i < 4; ++i) {
      const bool want = zref::RumbleBridge::pwm(rumOracle.duty(i), cycles);
      const bool got = (tops.rum.rumble_pwm & (1u << i)) != 0;
      zhao::check(got == want, "random: PWM carrier bit-exact", want, got);
    }
    // mid-frame stability: duty targets hold between ticks
    for (int i = 0; i < 4; ++i) {
      zhao::check(tops.rum.rumble_duty[i] == rumOracle.duty(i),
                  "random: duty stable between ticks", rumOracle.duty(i),
                  tops.rum.rumble_duty[i]);
    }
  };

  uint64_t hash = 1469598103934665603ull;  // FNV-1a 64
  auto mix = [&hash](uint64_t v) {
    for (int b = 0; b < 8; ++b) {
      hash ^= (v >> (b * 8)) & 0xFF;
      hash *= 1099511628211ull;
    }
  };

  for (uint32_t f = 0; f < frames; ++f) {
    // ---- build the frame's stimulus ------------------------------------
    const uint32_t len = 2 + rng(12);  // cycles before the tick
    // pad change points: at most one raw-state change per pad per frame
    int change_at[4];
    zref::PadRawState next_state[4];
    for (int i = 0; i < 4; ++i) {
      next_state[i] = pads[i];
      change_at[i] = -1;
      if (rng(4) == 0) {  // 25% of pads change this frame
        change_at[i] = static_cast<int>(rng(len));
        if (rng(6) == 0) {
          next_state[i].present = !next_state[i].present;  // plug/unplug
          next_state[i].buttons = 0;
          next_state[i].lx = next_state[i].ly = 0;
          next_state[i].rx = next_state[i].ry = 0;
        } else {
          next_state[i].buttons = rng.next();
          next_state[i].lx = static_cast<int16_t>(rng.next() >> 16);
          next_state[i].ly = static_cast<int16_t>(rng.next() >> 16);
          next_state[i].rx = static_cast<int16_t>(rng.next() >> 16);
          next_state[i].ry = static_cast<int16_t>(rng.next() >> 16);
        }
      }
    }
    // rumble commands: 0..5 this frame, spread over the frame's cycles.
    // The RTL command port takes ONE command per cycle, so each scheduled
    // command gets a unique cycle (extras are dropped unscheduled — the
    // oracle is not told about them either; deterministic both sides).
    const int n_want = static_cast<int>(rng(6));
    RumbleCmd cmds[kMaxCmdsPerFrame];
    int cmd_at[kMaxCmdsPerFrame];
    bool used_cycle[16] = {};
    int n_cmds = 0;
    for (int c = 0; c < n_want; ++c) {
      RumbleCmd cmd;
      cmd.pad_index = static_cast<uint8_t>(rng(8));  // 4..7 = illegal
      cmd.enable = static_cast<uint8_t>(rng(4) == 0 ? 0 : 1);  // bias on
      cmd.strength = static_cast<uint8_t>(rng(256));
      int at = static_cast<int>(rng(len));
      int guard = 0;
      while (used_cycle[at] && guard++ < 32) at = (at + 1) % len;
      if (used_cycle[at]) continue;  // no free cycle: unscheduled
      used_cycle[at] = true;
      cmds[n_cmds] = cmd;
      cmd_at[n_cmds] = at;  // never on the tick cycle (tick is after len)
      ++n_cmds;
    }
    // stable-start duties for the mid-frame check inside this frame
    const uint8_t duty_at_start[4] = {tops.rum.rumble_duty[0], tops.rum.rumble_duty[1],
                                      tops.rum.rumble_duty[2], tops.rum.rumble_duty[3]};

    // ---- run the frame --------------------------------------------------
    zhao_input::drivePads(tops.snap, pads);
    for (uint32_t c = 0; c < len; ++c) {
      for (int i = 0; i < 4; ++i) {
        if (change_at[i] == static_cast<int>(c)) {
          pads[i] = next_state[i];  // mid-frame change (atomicity probe)
          zhao_input::drivePad(tops.snap, i, pads[i]);
        }
      }
      bool issued[kMaxCmdsPerFrame] = {};
      for (int k = 0; k < n_cmds && k < kMaxCmdsPerFrame; ++k) {
        if (cmd_at[k] == static_cast<int>(c)) {
          tops.rum.rumble_cmd_valid = 1;
          tops.rum.rumble_pad_index = cmds[k].pad_index;
          tops.rum.rumble_enable = cmds[k].enable;
          tops.rum.rumble_strength = cmds[k].strength;
          issued[k] = true;
        }
      }
      cyc();
      tops.rum.rumble_cmd_valid = 0;
      for (int k = 0; k < n_cmds && k < kMaxCmdsPerFrame; ++k) {
        if (issued[k]) rumOracle.command(cmds[k].pad_index, cmds[k].enable, cmds[k].strength);
      }
      for (int i = 0; i < 4; ++i) {
        zhao::check(tops.rum.rumble_duty[i] == duty_at_start[i],
                    "random: duty immune to mid-frame commands", duty_at_start[i],
                    tops.rum.rumble_duty[i]);
      }
    }

    // ---- the frame_tick: latch everything --------------------------------
    // The oracles advance BEFORE the edge (they model the state AT the tick,
    // which the RTL samples at this edge), so every post-edge compare — the
    // per-cycle PWM/duty differential in cyc() included — sees both sides.
    const uint32_t frame_id = f * 3 + 1;
    const zref::PadFrame* latched = padOracle.tick(pads, frame_id);
    rumOracle.tick();
    const uint64_t tick_bits = zhao_input::tickWord(true, frame_id, rng.bit());
    tops.snap.frame_tick = tick_bits;
    tops.rum.frame_tick = tick_bits;
    cyc();
    tops.snap.frame_tick = zhao_input::tickWord(false, frame_id, false);
    tops.rum.frame_tick = tops.snap.frame_tick;
    tops.snap.eval();
    tops.rum.eval();

    // ---- differential compare -------------------------------------------
    for (int i = 0; i < 4; ++i) {
      const zref::PadFrame got = zhao_input::readPadFrame(tops.snap, i);
      zhao::check(got == latched[i], "random: latched PadFrame bit-exact", 1,
                  got == latched[i] ? 1 : 0);
      zhao::check(tops.snap.pad_sequence[i] == padOracle.sequence(i),
                  "random: sequence matches oracle", padOracle.sequence(i),
                  tops.snap.pad_sequence[i]);
    }
    zhao::check(tops.snap.pad_frame_id == frame_id, "random: frame_id latched", frame_id,
                tops.snap.pad_frame_id);
    zhao::check(tops.snap.input_sequence_gaps == padOracle.gaps(),
                "random: gap counter matches oracle", padOracle.gaps(),
                tops.snap.input_sequence_gaps);
    for (int i = 0; i < 4; ++i) {
      zhao::check(tops.rum.rumble_duty[i] == rumOracle.duty(i),
                  "random: duty table matches oracle", rumOracle.duty(i),
                  tops.rum.rumble_duty[i]);
      zhao::check(((tops.rum.rumble_active >> i) & 1u) == (rumOracle.duty(i) != 0 ? 1u : 0u),
                  "random: active flag matches oracle", rumOracle.duty(i) != 0 ? 1 : 0,
                  ((tops.rum.rumble_active >> i) & 1u));
    }
    zhao::check(tops.rum.rumble_frames_dropped == rumOracle.droppedShadow(),
                "random: dropped shadow matches oracle", rumOracle.droppedShadow(),
                tops.rum.rumble_frames_dropped);

    // ---- transcript -------------------------------------------------------
    for (int i = 0; i < 4; ++i) {
      mix(latched[i].sequence);
      mix(latched[i].buttons);
      mix(static_cast<uint16_t>(latched[i].lx));
      mix(tops.rum.rumble_duty[i]);
    }
    mix(tops.rum.rumble_frames_dropped);
    mix(tops.rum.rumble_pwm);
  }
  return hash;
}

int main(int argc, char** argv) {
  uint32_t frames = 1000;
  uint64_t seed = 0x5A0CAFE20260814ull;  // frozen: run-twice determinism
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      frames = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = std::strtoull(argv[++i], nullptr, 0);
    }
  }

  const uint64_t hash1 = runTimeline(frames, seed);
  const uint64_t hash2 = runTimeline(frames, seed);  // plan R1: run twice
  zhao::check(hash1 == hash2, "run-twice transcript hash identical", hash1, hash2);
  std::printf("input_random: %u frames, seed 0x%016llx, transcript hash 0x%016llx\n",
              frames, static_cast<unsigned long long>(seed),
              static_cast<unsigned long long>(hash1));

  return zhao::report_and_exit("input_random");
}
