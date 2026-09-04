// assetfetch_rtl_directed.cpp — zhao_geom_assetfetch.sv against zref::assetfetch.
//
// ---------------------------------------------------------------------------
// WHAT IS ACTUALLY BEING COMPARED
// ---------------------------------------------------------------------------
// The block owns three things, so those three are what this file differentiates:
//
//   * the ADMISSION VERDICT, including WHICH refusal -- a wrong reason is as
//     wrong as a wrong verdict, because the counters are per reason and a
//     misclassified meshlet is a fault reported in the wrong place;
//   * the ADDRESSES it asks the guard for, against the oracle's plan rather
//     than against "some line in range" -- a fetch of the wrong 64 bytes still
//     returns 64 perfectly plausible bytes;
//   * the BYTES it serves, triplet for triplet and record for record.
//
// The testbench PLAYS MEM.GUARD rather than instantiating it. That block has
// its own differential and a formal no-escape proof; instantiating it here
// would re-prove the region check while hiding whether THIS block asked for the
// right addresses. Same refusal `geom_meshfetch_rtl_directed` makes about the
// cull service.
//
// THE PLAYED GUARD IS DELIBERATELY STRICTER THAN THE REAL ONE: it fails the
// test if asked for anything but an aligned 64-byte read inside the pool. The
// real guard would merely deny such a request, and a denial is easy to mistake
// for correct behaviour when the block should never have asked at all.
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

constexpr uint32_t kPoolImage    = 1u << 17;   // 128 KiB of the pool modelled
constexpr uint8_t  kClientEngine1 = 3;         // ZHAO_CLIENT_ENGINE1
constexpr uint16_t kSrcId        = 0x5A5A;

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

// The pool image, indexed by POOL-RELATIVE byte. Position dependent with the
// high byte in the mix -- see assetfetch_directed.cpp, where a pattern constant
// modulo 256 at aligned offsets left a real check unable to fail.
std::vector<uint8_t> g_pool;

void build_pool() {
  g_pool.assign(kPoolImage, 0);
  for (uint32_t i = 0; i < kPoolImage; ++i) {
    g_pool[i] = static_cast<uint8_t>((i * 31u + (i >> 8) * 131u + 7u) & 0xFFu);
  }
}

uint8_t pool_abs_byte(uint32_t abs) { return g_pool[abs - af::kAssetPoolBase]; }

// ---------------------------------------------------------------------------
// The played guard.
// ---------------------------------------------------------------------------
struct Guard {
  Vtb_assetfetch& d;
  std::vector<uint32_t> asked;    // every line address, IN ORDER
  bool deny = false;              // deny the next accepted request
  bool shape_error = false;       // the block asked for something illegal

  // Beat delivery state for the line currently in flight.
  bool     streaming = false;
  uint32_t line_addr = 0;
  int      beat = 0;
  // Whether a beat was actually PRESENTED this cycle. The first version
  // advanced `beat` in post_edge whenever `streaming` was set -- including on
  // the acceptance cycle, where no beat is driven -- so beat 0 was never
  // delivered and every buffer held its line shifted by one word. The
  // differential caught it as an exact 8-byte offset in both streams.
  bool     drove = false;

  explicit Guard(Vtb_assetfetch& dut) : d(dut) {}

  void drive() {
    drove = false;
    d.g_ready = 0;
    d.g_ok = 0;
    d.g_violation = 0;
    d.beat_valid = 0;
    d.beat_last = 0;
    d.beat_data = 0;

    if (streaming) {
      d.beat_valid = 1;
      d.beat_data = [&] {
        uint64_t w = 0;
        const uint32_t at = line_addr + static_cast<uint32_t>(beat) * 8;
        for (int b = 0; b < 8; ++b) {
          w |= static_cast<uint64_t>(pool_abs_byte(at + b)) << (8 * b);
        }
        return w;
      }();
      d.beat_last = (beat == 7);
      drove = true;
      return;   // one line at a time; no new request while beats flow
    }

    if (d.g_valid) {
      // Stricter than the real guard, on purpose (see the header).
      if (d.g_write != 0) shape_error = true;
      if (d.g_len != 64) shape_error = true;
      if (d.g_client != kClientEngine1) shape_error = true;
      if ((d.g_addr % 64u) != 0) shape_error = true;
      if (d.g_addr < af::kAssetPoolBase ||
          d.g_addr + 64u > af::kAssetPoolBase + af::kAssetPoolSpan) {
        shape_error = true;
      }

      d.g_ready = 1;
      if (deny) {
        d.g_violation = 1;
        deny = false;
      } else {
        d.g_ok = 1;
        asked.push_back(d.g_addr);
        line_addr = d.g_addr;
        beat = 0;
        streaming = true;   // beats begin the cycle after acceptance
      }
    }
  }

  void post_edge() {
    if (streaming && drove) {
      if (beat == 7) {
        streaming = false;
      } else {
        ++beat;
      }
    }
  }
};

struct Sim {
  Vtb_assetfetch& d;
  Guard g;

  explicit Sim(Vtb_assetfetch& dut) : d(dut), g(dut) {}

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

  // Offer one meshlet with a PROPER SINGLE-BEAT HANDSHAKE and run until it is
  // either servable or disposed of. Returns true if it became servable.
  //
  // The valid is dropped the moment it is accepted, which is what a real
  // producer does and what the first version of this got wrong: holding
  // m_valid high let the block re-accept a REFUSED meshlet on every cycle it
  // sat in S_IDLE, so refused_footprint climbed by dozens instead of one.
  bool offer(const af::Request& r, int budget = 20000) {
    d.m_vertex_offset = r.vertex_offset;
    d.m_index_offset = r.index_offset;
    d.m_vertex_count = r.vertex_count;
    d.m_triangle_count = r.triangle_count;
    d.m_src_id = kSrcId;
    d.m_client = kClientEngine1;
    d.m_valid = 1;

    int waited = 0;
    while (!d.m_ready && waited++ < 200) step();
    step();                 // the accepting edge
    d.m_valid = 0;

    // Admitted meshlets leave S_IDLE, so m_ready falls; a refusal never
    // leaves, so it is still high right here. That is the whole distinction.
    d.s_ready = 1;
    for (int i = 0; i < budget; ++i) {
      if (d.s_valid) {
        step();             // s_ready is high, so this accepts it
        d.s_ready = 0;
        return true;
      }
      if (d.m_ready) {      // back to idle with nothing to show
        d.s_ready = 0;
        return false;
      }
      step();
    }
    d.s_ready = 0;
    return false;
  }

  // Ask for one triplet and wait for the answer.
  af::Triplet triplet(uint32_t n, int budget = 200) {
    d.ix_req = 1;
    d.ix_index = static_cast<uint16_t>(n);
    step();
    d.ix_req = 0;
    for (int i = 0; i < budget; ++i) {
      if (d.ix_valid) {
        return af::Triplet{static_cast<uint8_t>(d.ix_a),
                           static_cast<uint8_t>(d.ix_b),
                           static_cast<uint8_t>(d.ix_c)};
      }
      step();
    }
    return af::Triplet{0xFF, 0xFF, 0xFF};   // a hang, reported by the compare
  }

  // Consume the next vertex record. Returns false on timeout.
  bool next_record(uint8_t out[af::kVertexRecordBytes], int budget = 400) {
    d.v_ready = 1;
    for (int i = 0; i < budget; ++i) {
      if (d.v_valid) {
        for (int b = 0; b < af::kVertexRecordBytes; ++b) {
          // Verilator presents a 256-bit port as eight 32-bit words.
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

// ---------------------------------------------------------------------------
// The expected line run: exactly what zref's plan implies, in order.
// ---------------------------------------------------------------------------
std::vector<uint32_t> expected_lines(const af::Plan& p) {
  std::vector<uint32_t> v;
  auto run = [&](uint32_t addr, uint32_t bytes) {
    if (bytes == 0) return;
    const uint32_t first = (addr / af::kLineBytes) * af::kLineBytes;
    const uint32_t n = af::lines_covering(addr, bytes);
    for (uint32_t i = 0; i < n; ++i) v.push_back(first + i * af::kLineBytes);
  };
  run(p.index_addr, p.index_bytes);     // index stream FIRST
  run(p.vertex_addr, p.vertex_bytes);
  return v;
}

// ---------------------------------------------------------------------------
// One admitted meshlet, end to end.
// ---------------------------------------------------------------------------
void serve_case(Sim& s, uint32_t voff, uint32_t ioff, int vc, int tc,
                const char* label) {
  af::Request r;
  r.vertex_offset = voff;
  r.index_offset = ioff;
  r.vertex_count = static_cast<uint8_t>(vc);
  r.triangle_count = static_cast<uint8_t>(tc);
  const af::Plan p = af::plan(r);

  s.g.asked.clear();
  const bool servable = s.offer(r);

  char what[192];
  std::snprintf(what, sizeof what, "%s: admitted matches the oracle", label);
  ck(servable == p.admitted, what);
  if (!servable) return;

  std::snprintf(what, sizeof what, "%s: the guard was asked for legal shapes only", label);
  ck(!s.g.shape_error, what);

  const std::vector<uint32_t> want = expected_lines(p);
  std::snprintf(what, sizeof what, "%s: the line run matches the plan, in order", label);
  ck(s.g.asked == want, what);
  if (s.g.asked != want) {
    std::printf("      wanted %zu lines, got %zu\n", want.size(), s.g.asked.size());
  }

  // Every triplet.
  bool trips_ok = true;
  for (int n = 0; n < tc && trips_ok; ++n) {
    const af::Triplet got = s.triplet(static_cast<uint32_t>(n));
    const uint32_t at = p.index_addr + static_cast<uint32_t>(n) * 3;
    if (got.a != pool_abs_byte(at) || got.b != pool_abs_byte(at + 1) ||
        got.c != pool_abs_byte(at + 2)) {
      trips_ok = false;
      std::printf("      triplet %d at %08x: got %02x %02x %02x want %02x %02x %02x\n",
                  n, at, got.a, got.b, got.c, pool_abs_byte(at),
                  pool_abs_byte(at + 1), pool_abs_byte(at + 2));
    }
  }
  std::snprintf(what, sizeof what, "%s: all %d triplets byte-exact", label, tc);
  ck(trips_ok, what);

  // Every vertex record.
  bool recs_ok = true;
  for (int v = 0; v < vc && recs_ok; ++v) {
    uint8_t got[af::kVertexRecordBytes];
    if (!s.next_record(got)) {
      recs_ok = false;
      std::printf("      vertex %d never arrived\n", v);
      break;
    }
    const uint32_t at = p.vertex_addr + static_cast<uint32_t>(v) * af::kVertexRecordBytes;
    for (int b = 0; b < af::kVertexRecordBytes; ++b) {
      if (got[b] != pool_abs_byte(at + static_cast<uint32_t>(b))) {
        recs_ok = false;
        std::printf("      vertex %d byte %d: got %02x want %02x\n", v, b, got[b],
                    pool_abs_byte(at + static_cast<uint32_t>(b)));
        break;
      }
    }
  }
  std::snprintf(what, sizeof what, "%s: all %d vertex records byte-exact", label, vc);
  ck(recs_ok, what);

  s.release();
}

void refuse_case(Sim& s, uint32_t voff, uint32_t ioff, int vc, int tc,
                 const char* label) {
  af::Request r;
  r.vertex_offset = voff;
  r.index_offset = ioff;
  r.vertex_count = static_cast<uint8_t>(vc);
  r.triangle_count = static_cast<uint8_t>(tc);
  const af::Plan p = af::plan(r);

  const uint32_t before = s.d.refused_footprint;
  s.g.asked.clear();
  const bool servable = s.offer(r);

  char what[192];
  std::snprintf(what, sizeof what, "%s: refused, as the oracle says", label);
  ck(!servable && !p.admitted, what);

  // A refused meshlet emits NOTHING -- and asks for nothing. Reading even the
  // first line of a footprint that was going to be rejected is the "partial
  // geometry" failure one level down.
  std::snprintf(what, sizeof what, "%s: no guard request was made at all", label);
  ck(s.g.asked.empty(), what);

  std::snprintf(what, sizeof what, "%s: refused_footprint counted it", label);
  ck(s.d.refused_footprint == before + 1, what);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  build_pool();

  Vtb_assetfetch dut;
  Sim s(dut);
  s.reset();

  // ---- admitted meshlets, over the shapes that exercise the layout --------
  // Aligned to a line, so both streams start at byte 0.
  serve_case(s, 0, 4096, 8, 5, "line-aligned");

  // Vertex block at byte 32 of its line -- the straddling case the alignment
  // ruling permits and the buffer must handle.
  serve_case(s, 32, 4104, 7, 9, "vertex at byte 32, index at byte 8");

  // The ruling maximum, which is also the buffer's worst case.
  serve_case(s, 64, 8192, 64, 126, "the ruling maximum");

  // Degenerate but legal: no triangles, so the index stream is never read.
  serve_case(s, 96, 8192, 3, 0, "no triangles");

  // And no vertices either -- a meshlet that reads nothing at all.
  serve_case(s, 96, 8192, 0, 0, "wholly empty");

  // ---- refusals, each for its own reason ---------------------------------
  refuse_case(s, 0, 4096, 65, 1, "65 vertices");
  refuse_case(s, 0, 4096, 1, 127, "127 triangles");
  refuse_case(s, 16, 4096, 1, 1, "vertex offset misaligned");
  refuse_case(s, 0, 4100, 1, 1, "index offset misaligned");
  refuse_case(s, af::kAssetPoolSpan, 4096, 1, 1, "vertex block past the pool");

  // ---- a guard denial is not an asset fault ------------------------------
  {
    const uint32_t before = dut.guard_denied;
    s.g.deny = true;
    af::Request r;
    r.vertex_offset = 0;
    r.index_offset = 4096;
    r.vertex_count = 4;
    r.triangle_count = 4;
    const bool servable = s.offer(r);
    ck(!servable, "a denied read yields no servable meshlet");
    ck(dut.guard_denied == before + 1, "a denied read is counted as guard_denied");
    ck(dut.refused_footprint == 5,
       "a denial is NOT counted as a footprint refusal -- different faults");
  }

  // ---- the block recovers and serves the next meshlet --------------------
  serve_case(s, 128, 4096, 5, 3, "after a denial");

  if (g_fail != 0) {
    std::printf("[assetfetch_rtl_directed] %d of %d checks FAILED\n", g_fail, g_checks);
    zhao::exit_hard(1);
  }
  std::printf("[assetfetch_rtl_directed] %d checks passed\n", g_checks);
  zhao::exit_hard(0);
}
