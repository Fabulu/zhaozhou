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

constexpr uint32_t kPoolImage = 1u << 17;  // 128 KiB of the pool modelled
constexpr uint8_t kClientEngine1 = 3;      // ZHAO_CLIENT_ENGINE1
constexpr uint16_t kSrcId = 0x5A5A;

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

uint8_t pool_abs_byte(uint32_t abs) {
  // BOUNDS-CHECKED, and the check is part of the model rather than defensive
  // padding. An OVERRUN beat is by definition a word the guard never
  // authorised, so there is no pool byte for it -- returning 0 says that,
  // where an unchecked index reads whatever happens to sit next in the host
  // process and calls it asset data. The first version was unchecked and the
  // overrun scenario segfaulted with no output at all, which is the buffered-
  // output trap CLAUDE.md names: a late fault presenting as no start.
  const uint32_t off = abs - af::kAssetPoolBase;
  return (off < g_pool.size()) ? g_pool[off] : 0u;
}

// ---------------------------------------------------------------------------
// The played guard.
// ---------------------------------------------------------------------------
struct Guard {
  Vtb_assetfetch& d;
  std::vector<uint32_t> asked;  // every line address, IN ORDER
  bool deny = false;            // deny the next accepted request
  // BEAT-PROTOCOL INJECTION (owner recovery brief 11.4). A 64-byte line is
  // exactly eight packed 64-bit words; these make the responder end one early
  // or run one long, which is what the block must refuse instead of believing.
  int truncate_at = -1;         // assert `last` on this beat instead of 7
  bool overrun = false;         // withhold `last` on beat 7 and send a ninth
  bool shape_error = false;     // the block asked for something illegal

  // Beat delivery state for the line currently in flight.
  bool streaming = false;
  // THE GUARD'S VERDICT IS A SEPARATE CYCLE (D22 tread 10, 2026-09-06).
  //
  // This played guard used to raise ready and ok in the SAME cycle. No guard in
  // the tree does that: `zhao_mem_guard` answers `ready = !fwd_active` -- a
  // LEVEL, "the forwarding stage is free" -- and pulses `ok` the cycle AFTER
  // the accept, which is the cycle that raised `fwd_active`. The two are
  // therefore never high together on a passing request, and GEOM.ASSETFETCH
  // had been written to match this model rather than the block it talks to.
  //
  // `verdict_pending` is the accept cycle's successor: ready drops, exactly one
  // of ok and violation pulses, and beats begin the cycle after that.
  bool verdict_pending = false;
  bool verdict_ok = false;

  uint32_t line_addr = 0;
  int beat = 0;
  // Whether a beat was actually PRESENTED this cycle. The first version
  // advanced `beat` in post_edge whenever `streaming` was set -- including on
  // the acceptance cycle, where no beat is driven -- so beat 0 was never
  // delivered and every buffer held its line shifted by one word. The
  // differential caught it as an exact 8-byte offset in both streams.
  bool drove = false;

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
      d.beat_last = (truncate_at >= 0) ? (beat == truncate_at)
                                       : (overrun ? (beat == 8) : (beat == 7));
      drove = true;
      return;  // one line at a time; no new request while beats flow
    }

    if (verdict_pending) {
      // The verdict cycle: ready is LOW (the forwarding stage is occupied) and
      // exactly one of ok/violation pulses.
      verdict_pending = false;
      if (verdict_ok) {
        d.g_ok = 1;
        streaming = true;   // beats begin the cycle after the VERDICT
      } else {
        d.g_violation = 1;
      }
      // NOT `drove`: no beat was presented this cycle, and post_edge()
      // advances the beat counter on `streaming && drove`. Setting it here
      // would skip beat 0 -- the whole first word of every line.
      return;
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
      verdict_pending = true;
      if (deny) {
        verdict_ok = false;
        deny = false;
      } else {
        verdict_ok = true;
        asked.push_back(d.g_addr);
        line_addr = d.g_addr;
        beat = 0;
      }
    }
  }

  void post_edge() {
    if (streaming && drove) {
      const int last_beat = (truncate_at >= 0) ? truncate_at : (overrun ? 8 : 7);
      if (beat == last_beat) {
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
    step();  // the accepting edge
    d.m_valid = 0;

    // Admitted meshlets leave S_IDLE, so m_ready falls; a refusal never
    // leaves, so it is still high right here. That is the whole distinction.
    d.s_ready = 1;
    for (int i = 0; i < budget; ++i) {
      if (d.s_valid) {
        step();  // s_ready is high, so this accepts it
        d.s_ready = 0;
        return true;
      }
      if (d.m_ready) {  // back to idle with nothing to show
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
        return af::Triplet{static_cast<uint8_t>(d.ix_a), static_cast<uint8_t>(d.ix_b),
                           static_cast<uint8_t>(d.ix_c)};
      }
      step();
    }
    return af::Triplet{0xFF, 0xFF, 0xFF};  // a hang, reported by the compare
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
  run(p.index_addr, p.index_bytes);  // index stream FIRST
  run(p.vertex_addr, p.vertex_bytes);
  return v;
}

// ---------------------------------------------------------------------------
// One admitted meshlet, end to end.
// ---------------------------------------------------------------------------
void serve_case(Sim& s, uint32_t voff, uint32_t ioff, int vc, int tc, const char* label) {
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
      std::printf("      triplet %d at %08x: got %02x %02x %02x want %02x %02x %02x\n", n, at,
                  got.a, got.b, got.c, pool_abs_byte(at), pool_abs_byte(at + 1),
                  pool_abs_byte(at + 2));
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

void refuse_case(Sim& s, uint32_t voff, uint32_t ioff, int vc, int tc, const char* label) {
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

  // ---- EARLY RELEASE: the stream state must be torn down with the buffer --
  // A consumer may finish with a meshlet before its vertex stream is drained --
  // GEOM.ASSEMBLE can refuse every triplet and want nothing more. The first
  // version of the RTL returned to S_IDLE without clearing v_full_q, so
  // v_valid_o stayed asserted over the NEXT meshlet's buffer while the
  // PREVIOUS meshlet's record sat on the wires. Stale valid is the worst kind
  // of wrong: the consumer cannot tell it from a fresh one.
  {
    af::Request r;
    r.vertex_offset = 160;
    r.index_offset = 4096;
    r.vertex_count = 8;
    r.triangle_count = 4;
    ck(s.offer(r), "early-release fixture is servable");

    // Take exactly one record of eight, then release.
    uint8_t got[af::kVertexRecordBytes];
    ck(s.next_record(got), "early release: the first record arrives");
    s.release();

    ck(dut.v_valid == 0, "early release: v_valid is DEASSERTED, not left standing");

    // And the next meshlet is served correctly rather than inheriting state.
    serve_case(s, 192, 4104, 6, 5, "after an early release");
  }

  // THESE RUN LAST, AND THAT IS NOT TIDINESS. Both scenarios ABANDON a job
  // mid-line, which is the whole point of them, so each one adds an admission
  // that no oracle above accounts for. Placed earlier they made four later
  // cumulative checks fail -- "after a denial: admitted matches the oracle" and
  // its neighbours -- which looked like the new fault handling breaking the
  // block and was the test's own bookkeeping.
  // ================= A LINE THAT ENDS EARLY, AND ONE THAT DOES NOT ==========
  // Owner recovery brief 11.4: "Do not make beat_last alone authority for
  // arbitrary received length. Early last is truncation; late/extra data is a
  // protocol fault. Old RAM contents cannot supply missing words of a
  // supposedly complete vertex."
  //
  // Before this, the block counted beats and believed `beat_last`. A line that
  // stopped at word five produced a meshlet whose last three words were
  // whatever the RAM held from the PREVIOUS meshlet -- a vertex record that
  // decodes cleanly, passes its format check, and is partly somebody else's.
  // That is a silent wrong picture, so the evidence has to be a counter and a
  // refusal, not a stall or a timeout.
  {
    const uint32_t before = dut.err_beat_truncated;
    s.g.truncate_at = 5;              // `last` on word five of eight
    af::Request r;
    r.vertex_offset = 0;
    r.index_offset = 4096;
    r.vertex_count = 4;
    r.triangle_count = 4;
    const bool servable = s.offer(r);
    s.g.truncate_at = -1;

    ck(!servable,
       "a TRUNCATED line yields no servable meshlet -- a partial meshlet emits "
       "NOTHING rather than the part that fits, the same rule a refused "
       "footprint follows");
    ck(dut.err_beat_truncated == before + 1,
       "and it is counted as a TRUNCATION, distinctly");
    ck(dut.err_beat_overrun == 0,
       "not as an overrun -- the two have different causes and different fixes");
  }

  {
    const uint32_t before = dut.err_beat_overrun;
    s.g.overrun = true;               // no `last` on word eight; a ninth follows
    af::Request r;
    r.vertex_offset = 0;
    r.index_offset = 4096;
    r.vertex_count = 4;
    r.triangle_count = 4;
    const bool servable = s.offer(r);
    s.g.overrun = false;

    ck(!servable, "an OVERRUNNING line yields no servable meshlet either");
    ck(dut.err_beat_overrun == before + 1,
       "and it is counted as an OVERRUN -- the ninth word would land outside "
       "the line's reserved destination range");
  }

  ck(dut.err_beat_unowned == 0,
     "and no beat ever arrived without a request outstanding, which is the "
     "counter the two-bank rework will make load-bearing");

  if (g_fail != 0) {
    std::printf("[assetfetch_rtl_directed] %d of %d checks FAILED\n", g_fail, g_checks);
    zhao::exit_hard(1);
  }
  std::printf("[assetfetch_rtl_directed] %d checks passed\n", g_checks);
  zhao::exit_hard(0);
}
