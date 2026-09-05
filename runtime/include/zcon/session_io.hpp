// session_io.hpp — save, load and replay for a console session.
// Authored 2026-09-05 (software lane).
//
//   > Record inputs and authoritative state hashes from the beginning.
//   > Implement save/load and a short replay before adding much content. That
//   > protects future networking choices without making online infrastructure
//   > part of the console's critical path.
//
// So this lands now, while the game is small enough that the format is easy to
// get right, rather than after content makes it expensive.
//
// ---------------------------------------------------------------------------
// WHAT A RECORDING IS, AND WHAT IT IS NOT
// ---------------------------------------------------------------------------
// A recording is the SEED plus the INPUT STREAM plus the HASH STREAM. It is
// NOT a snapshot of game state, deliberately:
//
//   * a state snapshot goes stale the moment the simulation changes shape,
//     while an input stream stays meaningful and simply stops matching -- and
//     "stops matching at tick N" is a diagnosis;
//   * the hash stream is what turns a replay from "it ran again" into "it ran
//     again IDENTICALLY, and here is the first tick where it did not".
//
// The format is little-endian and fixed-width. No padding, no alignment
// assumptions, no struct dumps: a struct dump is a format that changes when a
// compiler feels like it.

#ifndef ZCON_SESSION_IO_HPP
#define ZCON_SESSION_IO_HPP

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "zcon/zcon.hpp"

namespace zcon {

// ---------------------------------------------------------------------------
// WHAT A CLEAN REPLAY DOES AND DOES NOT PROVE
// ---------------------------------------------------------------------------
// Measured 2026-09-05 by flipping one bit at three offsets in a 300-tick
// recording and replaying each:
//
//   offset 8263  -> DIVERGED at hash index 279
//   offset 7167  -> DIVERGED at hash index 142
//   offset  843  -> OK, identical hash stream
//
// The first two are the point of the format working: a desync is LOCATED, at
// the tick it happened, which is the whole reason inputs and hashes are
// recorded separately.
//
// **The third is not a bug and it is the thing to understand.** A recording
// carries input bytes that provably cannot change authoritative state -- the
// right stick is read only on a tick that fires a bolt, so its value on every
// other tick is inert. Corrupt one of those and the simulation replays
// identically, because it genuinely would have.
//
// So the replay answers "did the simulation diverge", and it does NOT answer
// "is this file intact". Those are different questions and only the first is
// what the roadmap asks for ("a desync is a diff between two hash streams,
// found at the tick it happened"). **There is deliberately no CRC over the
// recording**: adding one would answer the second question and would also
// invite the mistake of treating a passing CRC as evidence about the
// simulation, which it is not. If recordings ever cross a network or a machine
// boundary, integrity belongs in that transport, not smuggled in here.
//
// "ZREC", version 1. The version is checked, not assumed: a recording from a
// future format must be refused rather than misread.
constexpr uint32_t kRecMagic = 0x43455250u;  // 'PREC' little-endian on disk
constexpr uint32_t kRecVersion = 1;

struct Recording {
  uint64_t seed = 0;
  std::vector<InputSnapshot> inputs;
  std::vector<uint64_t> hashes;   // one before the first input, then one per tick
  std::string backend_name;
};

namespace detail {

inline void put32(std::vector<uint8_t>* v, uint32_t x) {
  v->push_back(uint8_t(x & 0xFF));
  v->push_back(uint8_t((x >> 8) & 0xFF));
  v->push_back(uint8_t((x >> 16) & 0xFF));
  v->push_back(uint8_t((x >> 24) & 0xFF));
}
inline void put64(std::vector<uint8_t>* v, uint64_t x) {
  put32(v, uint32_t(x & 0xFFFFFFFFu));
  put32(v, uint32_t(x >> 32));
}
inline bool get32(const std::vector<uint8_t>& v, std::size_t* p, uint32_t* out) {
  if (*p + 4 > v.size()) return false;
  *out = uint32_t(v[*p]) | (uint32_t(v[*p + 1]) << 8) | (uint32_t(v[*p + 2]) << 16) |
         (uint32_t(v[*p + 3]) << 24);
  *p += 4;
  return true;
}
inline bool get64(const std::vector<uint8_t>& v, std::size_t* p, uint64_t* out) {
  uint32_t lo, hi;
  if (!get32(v, p, &lo) || !get32(v, p, &hi)) return false;
  *out = uint64_t(lo) | (uint64_t(hi) << 32);
  return true;
}

}  // namespace detail

inline std::vector<uint8_t> serialize(const Recording& r) {
  std::vector<uint8_t> v;
  detail::put32(&v, kRecMagic);
  detail::put32(&v, kRecVersion);
  detail::put64(&v, r.seed);
  detail::put32(&v, uint32_t(r.inputs.size()));
  detail::put32(&v, uint32_t(r.hashes.size()));
  for (const InputSnapshot& s : r.inputs) {
    detail::put32(&v, s.tick);
    for (int p = 0; p < InputSnapshot::kPads; ++p) {
      detail::put32(&v, s.pad[p].buttons);
      v.push_back(uint8_t(s.pad[p].stick_lx));
      v.push_back(uint8_t(s.pad[p].stick_ly));
      v.push_back(uint8_t(s.pad[p].stick_rx));
      v.push_back(uint8_t(s.pad[p].stick_ry));
    }
  }
  for (uint64_t h : r.hashes) detail::put64(&v, h);
  return v;
}

// Returns false on ANY malformation: bad magic, wrong version, truncation. A
// half-read recording that silently replays the part it managed to parse would
// be the worst possible outcome -- it would look like a desync.
inline bool deserialize(const std::vector<uint8_t>& v, Recording* out) {
  std::size_t p = 0;
  uint32_t magic = 0, version = 0, n_in = 0, n_h = 0;
  if (!detail::get32(v, &p, &magic) || magic != kRecMagic) return false;
  if (!detail::get32(v, &p, &version) || version != kRecVersion) return false;
  if (!detail::get64(v, &p, &out->seed)) return false;
  if (!detail::get32(v, &p, &n_in) || !detail::get32(v, &p, &n_h)) return false;

  out->inputs.clear();
  out->hashes.clear();
  for (uint32_t i = 0; i < n_in; ++i) {
    InputSnapshot s;
    if (!detail::get32(v, &p, &s.tick)) return false;
    for (int q = 0; q < InputSnapshot::kPads; ++q) {
      uint32_t b = 0;
      if (!detail::get32(v, &p, &b)) return false;
      if (p + 4 > v.size()) return false;
      s.pad[q].buttons = uint16_t(b);
      s.pad[q].stick_lx = int8_t(v[p + 0]);
      s.pad[q].stick_ly = int8_t(v[p + 1]);
      s.pad[q].stick_rx = int8_t(v[p + 2]);
      s.pad[q].stick_ry = int8_t(v[p + 3]);
      p += 4;
    }
    out->inputs.push_back(s);
  }
  for (uint32_t i = 0; i < n_h; ++i) {
    uint64_t h = 0;
    if (!detail::get64(v, &p, &h)) return false;
    out->hashes.push_back(h);
  }
  return true;
}

inline Recording record_of(const Session& s, const char* backend_name) {
  Recording r;
  r.seed = s.seed();
  r.inputs = s.inputs();
  r.hashes = s.hashes();
  r.backend_name = backend_name ? backend_name : "";
  return r;
}

}  // namespace zcon

#endif  // ZCON_SESSION_IO_HPP
