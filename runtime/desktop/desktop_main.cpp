// desktop_main.cpp — the desktop console host.
// Rewritten 2026-09-05 (software lane). It was a 64-line stub.
//
// ---------------------------------------------------------------------------
// WHAT CHANGED AND WHY
// ---------------------------------------------------------------------------
// The previous file replayed sealed frame packets through the empty ZRef shell
// and exited 0. The roadmap's audit was blunt about it:
//
//   > zemu_main.cpp and desktop_main.cpp currently replay packets through the
//   > empty shell. They are not interactive game hosts or complete console
//   > emulators. ... somebody must turn those pieces into products.
//
// This is a HOST: it owns the fixed tick, drives a simulation through the
// shared console-runtime boundary, records the session, and can replay it and
// report the exact tick of any divergence.
//
//   host --ticks N [--seed S] [--record FILE]      run a match
//   host --replay FILE                             replay and verify one
//
// ---------------------------------------------------------------------------
// WHAT IS HONESTLY NOT HERE YET
// ---------------------------------------------------------------------------
// **Video and audio output.** The backend below builds and accepts command
// streams but does not yet drive the reference renderer. That wiring is the
// next software-lane increment and it is deliberately NOT faked here: printing
// a frame counter and calling it video is exactly the "documented-empty
// banner" this file is replacing.
//
// **Real controllers.** Input is scripted or idle. The `Backend::poll`
// boundary is where a real pad arrives, and nothing above it changes when one
// does -- which is the point of the boundary.
//
// **Cartridge loading.** `--cart` reads a file and publishes it as one
// MESH_STREAM resource so the lifecycle is exercised end to end; the real
// cartridge parse is G2's resource-format work, and pretending otherwise would
// invent a format before it is frozen.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "zcon/session_io.hpp"
#include "zcon/zcon.hpp"
#include "zgame/wizards.hpp"

namespace {

// C stdio rather than <fstream>, and the reason is measured rather than
// stylistic: on this toolchain (winlibs g++ 15.x, MinGW) an `std::ofstream`
// write faults at -O1 and works at -O0. It was bisected to exactly this
// 592 bytes..." printed, and the process died inside the stream write.
// stdio has no such problem and this file has no need of iostreams.
std::vector<uint8_t> read_file(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return {};
  std::vector<uint8_t> v;
  uint8_t buf[4096];
  std::size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) v.insert(v.end(), buf, buf + n);
  std::fclose(f);
  return v;
}

bool write_file(const std::string& path, const std::vector<uint8_t>& bytes) {
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const std::size_t n =
      bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), f);
  const bool ok = (n == bytes.size());
  return (std::fclose(f) == 0) && ok;
}

// ---------------------------------------------------------------------------
// The desktop backend. Owns resource memory and accepts presentation work.
// ---------------------------------------------------------------------------
class DesktopBackend : public zcon::Backend {
 public:
  struct Resource {
    zcon::ResourceKind kind;
    std::vector<uint8_t> bytes;
    uint16_t generation;
  };

  zcon::Handle publish(zcon::ResourceKind kind, const uint8_t* bytes,
                       std::size_t len) override {
    Resource r;
    r.kind = kind;
    r.bytes.assign(bytes, bytes + len);
    // Generations start at 1 because 0 means "never published" -- a stale
    // handle must be detectable, not merely unlucky (D-3).
    r.generation = static_cast<uint16_t>(resources_.size() + 1);
    resources_.push_back(std::move(r));
    zcon::Handle h;
    h.index = static_cast<uint32_t>(resources_.size() - 1);
    h.generation = resources_.back().generation;
    h.kind = kind;
    return h;
  }

  zcon::InputSnapshot poll(uint32_t tick) override {
    if (tick < scripted_.size()) return scripted_[tick];
    zcon::InputSnapshot s;
    s.tick = tick;
    return s;
  }

  void submit(const std::vector<uint8_t>& commands) override {
    ++frames_;
    command_bytes_ += commands.size();
  }

  const char* name() const override { return "desktop"; }

  void set_script(std::vector<zcon::InputSnapshot> s) { scripted_ = std::move(s); }
  std::size_t resource_count() const { return resources_.size(); }
  uint64_t frames() const { return frames_; }
  uint64_t command_bytes() const { return command_bytes_; }

 private:
  std::vector<Resource> resources_;
  std::vector<zcon::InputSnapshot> scripted_;
  uint64_t frames_ = 0;
  uint64_t command_bytes_ = 0;
};

// A deterministic demo script, so `--ticks N` alone produces a real match
// rather than two wizards standing still. Lateral movement with periodic
// casting: bolts fired with no aim travel toward the opponent.
std::vector<zcon::InputSnapshot> demo_script(int n) {
  std::vector<zcon::InputSnapshot> v;
  for (int t = 0; t < n; ++t) {
    zcon::InputSnapshot s;
    s.tick = static_cast<uint32_t>(t);
    s.pad[0].stick_lx = static_cast<int8_t>((t * 7) % 11 - 5);
    s.pad[0].buttons = static_cast<uint16_t>((t % 31 == 0) ? 1 : 0);
    s.pad[1].stick_lx = static_cast<int8_t>((t * 5) % 9 - 4);
    s.pad[1].buttons = static_cast<uint16_t>((t % 33 == 0) ? 1 : 0);
    v.push_back(s);
  }
  return v;
}

int usage() {
  std::printf(
      "zhao-desktop -- the desktop console host\n"
      "  --ticks N [--seed S] [--cart FILE] [--record FILE]   run a match\n"
      "  --replay FILE                                        verify a recording\n"
      "\n"
      "Video and audio output are NOT wired yet; this host owns the tick, the\n"
      "simulation, resources and the recording. See the header comment.\n");
  return 2;
}

int do_replay(const std::string& path) {
  const std::vector<uint8_t> bytes = read_file(path);
  if (bytes.empty()) {
    std::printf("replay: cannot read %s\n", path.c_str());
    return 1;
  }
  zcon::Recording rec;
  if (!zcon::deserialize(bytes, &rec)) {
    // Refusing a malformed recording matters: a half-parsed one would replay
    // the part it managed to read and present as a desync.
    std::printf("replay: %s is not a valid recording (magic/version/truncation)\n",
                path.c_str());
    return 1;
  }

  DesktopBackend be;
  zgame::Wizards truth;
  zcon::Session s(&truth, &be);
  s.start(rec.seed);
  const int diverged = s.replay_and_compare(rec.inputs, rec.hashes);
  if (diverged < 0) {
    std::printf("replay: %s OK -- %zu ticks, identical hash stream\n", path.c_str(),
                rec.inputs.size());
    return 0;
  }
  std::printf("replay: %s DIVERGED at hash index %d of %zu\n", path.c_str(),
              diverged, rec.hashes.size());
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  int ticks = 0;
  uint64_t seed = 0x5A5A5A5A;
  std::string cart, record_path, replay_path;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&](std::string* dst) {
      if (i + 1 < argc) *dst = argv[++i];
    };
    if (a == "--ticks" && i + 1 < argc) {
      ticks = std::atoi(argv[++i]);
    } else if (a == "--seed" && i + 1 < argc) {
      seed = std::strtoull(argv[++i], nullptr, 0);
    } else if (a == "--cart") {
      next(&cart);
    } else if (a == "--record") {
      next(&record_path);
    } else if (a == "--replay") {
      next(&replay_path);
    } else {
      return usage();
    }
  }

  if (!replay_path.empty()) return do_replay(replay_path);
  if (ticks <= 0) return usage();

  DesktopBackend be;
  be.set_script(demo_script(ticks));

  if (!cart.empty()) {
    const std::vector<uint8_t> bytes = read_file(cart);
    if (bytes.empty()) {
      std::printf("cart: cannot read %s\n", cart.c_str());
      return 1;
    }
    const zcon::Handle h =
        be.publish(zcon::ResourceKind::kMeshStream, bytes.data(), bytes.size());
    std::printf("cart: %s -> handle{index=%u generation=%u} (%zu bytes)\n",
                cart.c_str(), h.index, h.generation, bytes.size());
  }

  zgame::Wizards truth;
  zcon::Session s(&truth, &be);
  s.start(seed);
  for (int t = 0; t < ticks; ++t) s.tick();

  std::printf(
      "host: %d ticks, seed 0x%llX, backend '%s'\n"
      "      wizards: p0 hp=%d deaths=%u | p1 hp=%d deaths=%u\n"
      "      frames submitted %llu, command bytes %llu, resources %zu\n"
      "      final state hash 0x%016llX\n",
      ticks, static_cast<unsigned long long>(seed), be.name(),
      truth.wizard(0).health, truth.wizard(0).deaths, truth.wizard(1).health,
      truth.wizard(1).deaths, static_cast<unsigned long long>(be.frames()),
      static_cast<unsigned long long>(be.command_bytes()), be.resource_count(),
      static_cast<unsigned long long>(truth.hash()));
  // Flush before the long tail of work. Buffered output lost in a crash makes a
  // late fault look like an early one, which cost real time here: the record
  // path faulted AFTER this summary and presented as "no output at all".
  std::fflush(stdout);

  // Self-verify before writing: a recording that does not replay in the process
  // that produced it will certainly not replay anywhere else, and finding that
  // out at write time is free.
  const int diverged = s.replay_and_compare(s.inputs(), s.hashes());
  if (diverged >= 0) {
    std::printf("host: INTERNAL -- session does not replay, diverges at %d\n",
                diverged);
    return 1;
  }

  if (!record_path.empty()) {
    const zcon::Recording rec = zcon::record_of(s, be.name());
    const std::vector<uint8_t> blob = zcon::serialize(rec);
    if (!write_file(record_path, blob)) {
      std::printf("host: cannot write %s\n", record_path.c_str());
      return 1;
    }
    std::printf("host: recorded %zu ticks to %s\n", rec.inputs.size(),
                record_path.c_str());
  }
  return 0;
}
