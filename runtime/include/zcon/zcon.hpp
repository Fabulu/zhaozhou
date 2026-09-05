// zcon.hpp — the shared console runtime boundary.
// Authored 2026-09-05, roadmap "the parallel software lane", first slice.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `runtime/` contained one 64-line stub (`desktop/desktop_main.cpp`), an empty
// `include/` and an empty `mister/`. The stub replays sealed frame packets
// through the empty ZRef shell and exits 0. It is not a host: it loads no
// cartridge, owns no resources, polls no controller and advances no
// simulation.
//
// The roadmap's instruction is precise about the shape, and about the trap:
//
//   > turn the existing exact reference/rendering machinery into a REUSABLE
//   > CONSOLE BACKEND, rather than adding another renderer beside it
//
// So nothing here renders. This file defines the three boundaries the software
// is split along, and the console runtime is the middle one:
//
//   Shared game truth      fixed-tick gameplay, AI, collision, canonical
//                          terrain changes, events. Knows nothing about
//                          rendering or platforms.
//   Shared console runtime (THIS FILE) resource handles, command construction,
//                          loading, streaming and lifecycle.
//   Platform backend       exact desktop console execution, or physical
//                          HPS/FPGA transport. Implements `Backend`.
//
// The acceptance test the roadmap names is not how attractive the result is.
// It is that the game "uses the same simulation and resource/command
// interfaces intended for the console, with no game logic hidden inside the
// reel renderer". These boundaries are what make that checkable rather than
// aspirational.
//
// ---------------------------------------------------------------------------
// DETERMINISM IS A DAY-ONE PROPERTY, NOT A LATER FEATURE
// ---------------------------------------------------------------------------
//   > Record inputs and authoritative state hashes from the beginning.
//   > Implement save/load and a short replay before adding much content. That
//   > protects future networking choices without making online infrastructure
//   > part of the console's critical path.
//
// So `Tick` carries an input snapshot and produces a state hash, and a session
// can be recorded and replayed. A desync is then a diff between two hash
// streams, found at the tick it happened, instead of a mystery a month later.

#ifndef ZCON_ZCON_HPP
#define ZCON_ZCON_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace zcon {

// ---------------------------------------------------------------------------
// Resource handles
// ---------------------------------------------------------------------------
// Owner ruling D-2 fixed three generic families. A handle names a resource the
// console runtime has published; it is NOT a pointer, and it deliberately
// carries the 16-bit residency generation from D-3 so a stale handle is
// detectable rather than merely unlucky.
enum class ResourceKind : uint8_t {
  kTexturePage = 0,
  kMaterialSet = 1,
  kMeshStream = 2,
};

struct Handle {
  uint32_t index = 0;       // slot in the runtime's table
  uint16_t generation = 0;  // D-3 residency generation; 0 == never published
  ResourceKind kind = ResourceKind::kTexturePage;

  bool valid() const { return generation != 0; }
  bool operator==(const Handle& o) const {
    return index == o.index && generation == o.generation && kind == o.kind;
  }
};

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
// One snapshot per fixed tick, per pad. This is the ONLY way the simulation
// learns about the outside world -- which is what makes a recorded session
// replayable, and what makes a desync a diff rather than a mystery.
struct PadState {
  uint16_t buttons = 0;
  int8_t stick_lx = 0, stick_ly = 0;
  int8_t stick_rx = 0, stick_ry = 0;
};

struct InputSnapshot {
  static constexpr int kPads = 2;  // Duo: two wizards, two controllers
  PadState pad[kPads];
  uint32_t tick = 0;
};

// ---------------------------------------------------------------------------
// The platform backend
// ---------------------------------------------------------------------------
// Implemented twice: once by the exact desktop execution path, once by the
// physical HPS/FPGA transport. The runtime above it must not be able to tell
// which it has -- if it can, gameplay has leaked into a platform.
class Backend {
 public:
  virtual ~Backend() = default;

  // Publish immutable bytes and return the handle consumers will name. The
  // full upload transaction (fresh destination, pin, atomic publish, reclaim
  // after last pin) is G2's work; this is the boundary it lands behind.
  virtual Handle publish(ResourceKind kind, const uint8_t* bytes,
                         std::size_t len) = 0;

  // Poll the controllers for this tick. The runtime never reads hardware.
  virtual InputSnapshot poll(uint32_t tick) = 0;

  // Hand one frame's presentation work over. Commands are already built by the
  // runtime; the backend transports and executes them.
  virtual void submit(const std::vector<uint8_t>& commands) = 0;

  // Name of the backend, for the record written into a replay.
  virtual const char* name() const = 0;
};

// ---------------------------------------------------------------------------
// Game truth
// ---------------------------------------------------------------------------
// The simulation the console runs. It advances by whole ticks from an input
// snapshot and nothing else, and it reports a hash of its authoritative state.
//
// It must not know what a framebuffer is. The one rule that keeps this
// boundary real: `hash()` covers gameplay state ONLY. If a presentation
// decision could change it, presentation is deciding gameplay -- which is the
// failure the roadmap names when it says degradation "must never alter
// authoritative gameplay".
class GameTruth {
 public:
  virtual ~GameTruth() = default;
  virtual void reset(uint64_t seed) = 0;
  virtual void advance(const InputSnapshot& in) = 0;
  virtual uint64_t hash() const = 0;

  // Build this tick's presentation work. Reads state, never writes it.
  virtual void build_commands(std::vector<uint8_t>* out) const = 0;
};

// ---------------------------------------------------------------------------
// Session — the fixed-tick host loop
// ---------------------------------------------------------------------------
// This is the thing the stubs were not. It owns the tick, feeds the
// simulation, records inputs and hashes, and submits presentation work through
// the backend.
class Session {
 public:
  Session(GameTruth* truth, Backend* backend) : truth_(truth), backend_(backend) {}

  void start(uint64_t seed) {
    seed_ = seed;
    tick_ = 0;
    truth_->reset(seed);
    recorded_inputs_.clear();
    recorded_hashes_.clear();
    recorded_hashes_.push_back(truth_->hash());  // tick 0, before any input
  }

  // One fixed tick: poll, advance, record, present.
  void tick() {
    const InputSnapshot in = backend_->poll(tick_);
    truth_->advance(in);
    recorded_inputs_.push_back(in);
    recorded_hashes_.push_back(truth_->hash());

    commands_.clear();
    truth_->build_commands(&commands_);
    backend_->submit(commands_);
    ++tick_;
  }

  // One fixed tick where the HOST supplies the presentation work.
  //
  // The plain tick() asks the simulation to build commands, which is right when
  // the game owns its own presentation. A host that owns RESOURCES -- forms,
  // transforms, materials -- builds the frame itself, because the simulation
  // must not know what a FormPattern is. Both paths advance identically; only
  // who authors the bytes differs.
  //
  // The caller's bytes describe the state BEFORE this tick's advance, so
  // presentation lags simulation by one frame. That is deliberate and is what
  // a console does; recording it here stops it being read as a bug later.
  void tick_with(const std::vector<uint8_t>& commands) {
    const InputSnapshot in = backend_->poll(tick_);
    truth_->advance(in);
    recorded_inputs_.push_back(in);
    recorded_hashes_.push_back(truth_->hash());
    backend_->submit(commands);
    ++tick_;
  }

  // Replay a recorded input stream and report the first tick whose hash
  // differs, or -1 if the run is identical. This is the whole point of
  // recording: a divergence is located, not merely detected.
  int replay_and_compare(const std::vector<InputSnapshot>& inputs,
                         const std::vector<uint64_t>& hashes) {
    truth_->reset(seed_);
    if (hashes.empty() || truth_->hash() != hashes[0]) return 0;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
      truth_->advance(inputs[i]);
      const std::size_t h = i + 1;
      if (h >= hashes.size() || truth_->hash() != hashes[h])
        return static_cast<int>(h);
    }
    return -1;
  }

  uint32_t tick_index() const { return tick_; }
  uint64_t seed() const { return seed_; }
  const std::vector<InputSnapshot>& inputs() const { return recorded_inputs_; }
  const std::vector<uint64_t>& hashes() const { return recorded_hashes_; }

 private:
  GameTruth* truth_;
  Backend* backend_;
  uint64_t seed_ = 0;
  uint32_t tick_ = 0;
  std::vector<InputSnapshot> recorded_inputs_;
  std::vector<uint64_t> recorded_hashes_;
  std::vector<uint8_t> commands_;
};

}  // namespace zcon

#endif  // ZCON_ZCON_HPP
