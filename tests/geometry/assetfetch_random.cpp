// assetfetch_random.cpp — randomized differential for zhao_geom_assetfetch.sv.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS SEPARATELY FROM THE DIRECTED FILE
// ---------------------------------------------------------------------------
// `assetfetch_directed.cpp` and `assetfetch_rtl_directed.cpp` check the EDGES,
// which is where `plan()` can be wrong in an interesting way. This file checks
// the middle: many meshlets, arbitrary legal shapes, every triplet and every
// record compared. It is not a substitute for the edges and it is not trying to
// be -- a sweep over random offsets would take an implausible number of draws
// to land on "a footprint ending exactly at the pool's last byte".
//
// What it DOES buy, and the directed files cannot:
//
//   * the block RE-ARMS correctly. Every failure mode of a single-buffered
//     fetcher is a state that survives one meshlet into the next, and a test
//     that runs one meshlet per process cannot see any of them.
//   * BACKPRESSURE, applied pseudo-randomly on both service ports, so the
//     vertex stream and the index service are exercised against a consumer
//     that stalls rather than one that is always ready.
//   * the counters ACCUMULATE right over a long run rather than once.
//
// The generator is deliberately biased toward SMALL counts. Uniform draws over
// 0..64 would spend nearly all their time in large meshlets and almost never
// produce the 0- and 1-vertex cases where an off-by-one in the stream's end
// condition lives.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_assetfetch.h"

#include "zhao_sim.hpp"
#include "zref/zref_assetfetch.hpp"

namespace af = zref::assetfetch;

namespace {

constexpr uint32_t kPoolImage     = 1u << 17;
constexpr uint8_t  kClientEngine1 = 3;
constexpr uint16_t kSrcId         = 0x1234;
constexpr int      kMeshlets      = 240;

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

std::vector<uint8_t> g_pool;

void build_pool() {
  g_pool.assign(kPoolImage, 0);
  for (uint32_t i = 0; i < kPoolImage; ++i) {
    g_pool[i] = static_cast<uint8_t>((i * 31u + (i >> 8) * 131u + 7u) & 0xFFu);
  }
}

uint8_t pool_abs_byte(uint32_t abs) { return g_pool[abs - af::kAssetPoolBase]; }

// A named, reproducible generator. Not std::mt19937: a failing seed must be
// re-runnable from the printed number alone, on any toolchain.
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return static_cast<uint32_t>(s >> 32);
  }
  uint32_t below(uint32_t n) { return n ? next() % n : 0; }
  bool chance(uint32_t pct) { return below(100) < pct; }
};

struct Guard {
  Vtb_assetfetch& d;
  std::vector<uint32_t> asked;
  bool shape_error = false;
  bool streaming = false;
  bool drove = false;
  uint32_t line_addr = 0;
  int beat = 0;
  Rng& rng;
  // Stall the guard sometimes: a fetcher that only works when memory answers
  // instantly is a fetcher that has never met memory.
  bool stall_enabled = true;

  Guard(Vtb_assetfetch& dut, Rng& r) : d(dut), rng(r) {}

  void drive() {
    drove = false;
    d.g_ready = 0;
    d.g_ok = 0;
    d.g_violation = 0;
    d.beat_valid = 0;
    d.beat_last = 0;
    d.beat_data = 0;

    if (streaming) {
      if (stall_enabled && rng.chance(20)) return;   // a beat-less cycle
      uint64_t w = 0;
      const uint32_t at = line_addr + static_cast<uint32_t>(beat) * 8;
      for (int b = 0; b < 8; ++b) {
        w |= static_cast<uint64_t>(pool_abs_byte(at + b)) << (8 * b);
      }
      d.beat_data = w;
      d.beat_valid = 1;
      d.beat_last = (beat == 7);
      drove = true;
      return;
    }

    if (d.g_valid) {
      if (stall_enabled && rng.chance(25)) return;   // hold the request off
      if (d.g_write != 0) shape_error = true;
      if (d.g_len != 64) shape_error = true;
      if (d.g_client != kClientEngine1) shape_error = true;
      if ((d.g_addr % 64u) != 0) shape_error = true;
      if (d.g_addr < af::kAssetPoolBase ||
          d.g_addr + 64u > af::kAssetPoolBase + af::kAssetPoolSpan) {
        shape_error = true;
      }
      d.g_ready = 1;
      d.g_ok = 1;
      asked.push_back(d.g_addr);
      line_addr = d.g_addr;
      beat = 0;
      streaming = true;
    }
  }

  void post_edge() {
    if (streaming && drove) {
      if (beat == 7) streaming = false;
      else ++beat;
    }
  }
};

struct Sim {
  Vtb_assetfetch& d;
  Guard g;
  Rng& rng;

  Sim(Vtb_assetfetch& dut, Rng& r) : d(dut), g(dut, r), rng(r) {}

  void step() {
    g.drive();
    zhao::tick(d);
    g.post_edge();
  }

  void reset() {
    d.rst_n = 0;
    d.m_valid = 0;
    d.s_ready = 0;
    d.release_pulse = 0;
    d.ix_req = 0;
    d.ix_index = 0;
    d.v_ready = 0;
    d.g_ready = 0;
    d.g_ok = 0;
    d.g_violation = 0;
    d.beat_valid = 0;
    d.beat_last = 0;
    d.beat_data = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(d);
    d.rst_n = 1;
    zhao::tick(d);
  }

  bool offer(const af::Request& r) {
    d.m_vertex_offset = r.vertex_offset;
    d.m_index_offset = r.index_offset;
    d.m_vertex_count = r.vertex_count;
    d.m_triangle_count = r.triangle_count;
    d.m_src_id = kSrcId;
    d.m_client = kClientEngine1;
    d.m_valid = 1;
    int waited = 0;
    while (!d.m_ready && waited++ < 500) step();
    step();
    d.m_valid = 0;

    // A stalling consumer for the servable handshake too.
    for (int i = 0; i < 40000; ++i) {
      d.s_ready = rng.chance(70) ? 1 : 0;
      if (d.s_valid && d.s_ready) {
        step();
        d.s_ready = 0;
        return true;
      }
      if (d.m_ready && !d.s_valid) {
        d.s_ready = 0;
        return false;
      }
      step();
    }
    d.s_ready = 0;
    return false;
  }

  af::Triplet triplet(uint32_t n) {
    d.ix_req = 1;
    d.ix_index = static_cast<uint16_t>(n);
    step();
    d.ix_req = 0;
    for (int i = 0; i < 500; ++i) {
      if (d.ix_valid) {
        return af::Triplet{static_cast<uint8_t>(d.ix_a),
                           static_cast<uint8_t>(d.ix_b),
                           static_cast<uint8_t>(d.ix_c)};
      }
      step();
    }
    return af::Triplet{0xFF, 0xFF, 0xFF};
  }

  bool next_record(uint8_t out[af::kVertexRecordBytes]) {
    for (int i = 0; i < 2000; ++i) {
      d.v_ready = rng.chance(65) ? 1 : 0;
      if (d.v_valid && d.v_ready) {
        for (int b = 0; b < af::kVertexRecordBytes; ++b) {
          const uint32_t w = d.v_bytes[b / 4];
          out[b] = static_cast<uint8_t>((w >> (8 * (b % 4))) & 0xFFu);
        }
        step();
        d.v_ready = 0;
        return true;
      }
      step();
    }
    d.v_ready = 0;
    return false;
  }

  void release() {
    d.release_pulse = 1;
    step();
    d.release_pulse = 0;
    step();
  }
};

}  // namespace

int main(int argc, char** argv) {
  uint64_t seed = 0xC0FFEEull;
  if (argc > 1) seed = std::strtoull(argv[1], nullptr, 0);
  std::printf("assetfetch_random: seed %llu\n",
              static_cast<unsigned long long>(seed));

  build_pool();
  Rng rng(seed);
  Vtb_assetfetch dut;
  Sim s(dut, rng);
  s.reset();

  uint32_t admitted = 0, refused = 0, empty_meshlets = 0;
  uint64_t expect_beats = 0;
  bool all_bytes_ok = true;
  bool all_verdicts_ok = true;

  for (int m = 0; m < kMeshlets && all_bytes_ok && all_verdicts_ok; ++m) {
    af::Request r;
    // Biased small (see the header) with an occasional large one, and a slice
    // of deliberately ILLEGAL shapes so refusal and re-arm are exercised in
    // the same stream as success rather than in a separate test.
    const bool go_big = rng.chance(15);
    r.vertex_count = static_cast<uint8_t>(go_big ? 32 + rng.below(33) : rng.below(6));
    r.triangle_count = static_cast<uint8_t>(go_big ? 60 + rng.below(67) : rng.below(7));
    r.vertex_offset = rng.below(1200) * af::kVertexAlign;
    r.index_offset = rng.below(4000) * af::kIndexAlign;

    if (rng.chance(12)) {
      switch (rng.below(3)) {
        case 0: r.vertex_count = 65; break;
        case 1: r.triangle_count = 127; break;
        default: r.vertex_offset += 8; break;    // misaligned
      }
    }

    const af::Plan p = af::plan(r);
    s.g.asked.clear();
    const bool servable = s.offer(r);

    if (servable != p.admitted) {
      all_verdicts_ok = false;
      std::printf("  meshlet %d: RTL %s, oracle %s (v=%u t=%u voff=%u ioff=%u)\n",
                  m, servable ? "admitted" : "refused",
                  p.admitted ? "admitted" : "refused", r.vertex_count,
                  r.triangle_count, r.vertex_offset, r.index_offset);
      break;
    }

    if (!servable) {
      ++refused;
      if (!s.g.asked.empty()) {
        all_verdicts_ok = false;
        std::printf("  meshlet %d: refused but asked for %zu line(s)\n", m,
                    s.g.asked.size());
      }
      continue;
    }

    ++admitted;
    if (r.vertex_count == 0 && r.triangle_count == 0) ++empty_meshlets;
    expect_beats += p.beats * 8;

    for (uint32_t n = 0; n < r.triangle_count && all_bytes_ok; ++n) {
      const af::Triplet got = s.triplet(n);
      const uint32_t at = p.index_addr + n * 3;
      if (got.a != pool_abs_byte(at) || got.b != pool_abs_byte(at + 1) ||
          got.c != pool_abs_byte(at + 2)) {
        all_bytes_ok = false;
        std::printf("  meshlet %d triplet %u at %08x: got %02x %02x %02x\n", m, n,
                    at, got.a, got.b, got.c);
      }
    }

    for (uint32_t v = 0; v < r.vertex_count && all_bytes_ok; ++v) {
      uint8_t got[af::kVertexRecordBytes];
      if (!s.next_record(got)) {
        all_bytes_ok = false;
        std::printf("  meshlet %d vertex %u never arrived\n", m, v);
        break;
      }
      const uint32_t at = p.vertex_addr + v * af::kVertexRecordBytes;
      for (int b = 0; b < af::kVertexRecordBytes; ++b) {
        if (got[b] != pool_abs_byte(at + static_cast<uint32_t>(b))) {
          all_bytes_ok = false;
          std::printf("  meshlet %d vertex %u byte %d: got %02x want %02x\n", m, v,
                      b, got[b], pool_abs_byte(at + static_cast<uint32_t>(b)));
          break;
        }
      }
    }

    s.release();
  }

  ck(all_verdicts_ok, "every admission verdict matched the oracle");
  ck(all_bytes_ok, "every triplet and every vertex record was byte-exact");
  ck(!s.g.shape_error, "the guard was never asked for an illegal shape");

  // The generator must actually have produced the shapes it claims to. A test
  // that silently generated 240 identical meshlets would pass everything above
  // and mean nothing -- the same failure as a fixture that cannot distinguish
  // positions.
  ck(admitted > 100, "the sweep admitted a substantial majority");
  ck(refused > 10, "the sweep produced refusals too");
  ck(empty_meshlets > 0, "the sweep included at least one empty meshlet");

  // Counters accumulate over the whole run, not just the last meshlet.
  ck(dut.meshlets_fetched == admitted, "meshlets_fetched counted every admission");
  ck(dut.refused_footprint == refused, "refused_footprint counted every refusal");
  ck(dut.beats_read == expect_beats, "beats_read matches the oracle's line count x8");

  std::printf("assetfetch_random: %u admitted, %u refused, %u empty, %llu beats\n",
              admitted, refused, empty_meshlets,
              static_cast<unsigned long long>(expect_beats));

  if (g_fail != 0) {
    std::printf("[assetfetch_random] %d of %d checks FAILED (seed %llu)\n", g_fail,
                g_checks, static_cast<unsigned long long>(seed));
    zhao::exit_hard(1);
  }
  std::printf("[assetfetch_random] %d checks passed\n", g_checks);
  zhao::exit_hard(0);
}
