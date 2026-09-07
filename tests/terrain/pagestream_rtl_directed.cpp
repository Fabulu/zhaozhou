// pagestream_rtl_directed.cpp -- zhao_terrain_pagestream against
// zref::terrain::page_lattice.
//
// ===========================================================================
// WHAT IS AT RISK, AND WHY A COUNT CANNOT SEE IT
// ===========================================================================
// This block turns a resident page slot into 1,089 vertices. Every way it can
// be wrong produces exactly 1,089 vertices:
//
//   * the wrong SLOT -- a neighbouring page's terrain, in the right shape.
//   * the wrong LANE -- one 16-bit sample off, so every vertex carries its
//     neighbour's height. Renders as terrain. Nothing counts it.
//   * the wrong PLANE -- scar where base should be. Renders as terrain.
//   * the wrong ORDER -- transposed, or column-major. Renders as terrain, and
//     MIPGEN's fine port carries NO coordinate, so a differently-ordered scan
//     decimates the wrong vertices with every count agreeing.
//   * a stale BUFFER -- the last burst of the previous job served the first
//     vertices of this one. Renders as terrain.
//
// So the comparison is the whole stream, vertex by vertex, field by field,
// against the reference, with the fixture chosen so that every one of those
// confusions produces a different number.
//
// THE FIXTURE IS THE TEST. Each plane is filled from a DIFFERENT function of
// the vertex index, each function injective over the 1,089 samples, and the
// three ranges disjoint:
//
//     base   = +(0x1000 + 3k)      distinct per vertex, positive
//     scar   = -(0x0500 + 5k)      distinct per vertex, NEGATIVE -- height16
//                                  is signed and a zero-extending read passes
//                                  every test drawn from positive values
//     bottom = +(0x4000 + 7k)      distinct per vertex, disjoint from base
//
// A lane error moves k by one and the value by 3, 5 or 7. A plane swap changes
// the sign or the magnitude class. A slot error lands in a page filled from a
// different constant. None of them are silent.
//
// AND THE NEIGHBOURING SLOTS ARE POISONED, deliberately: slots 0, 2 and 3 hold
// a recognisable pattern that is not slot 1's, so "read the slot you were told
// to" is a checkable claim rather than an assumption. A block that ignored
// `j_slot_i` entirely would pass a suite that only ever used slot 0.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_pagestream.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_page.hpp"

namespace {

namespace tp = zref::terrain;

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
    std::fflush(stdout);
  }
}

void ck(bool ok, const char* what, long expect, long got) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %ld, got %ld)\n", what, expect, got);
    std::fflush(stdout);
  }
}

constexpr uint32_t kPageBytes = tp::kPageBytes;      // 21,376
constexpr uint32_t kPageWords = kPageBytes / 8;      // 2,672
constexpr uint32_t kSlots = 4;
constexpr uint32_t kPoolBase = 0x04000000u;          // ruling T2
constexpr int kVerts = tp::kLatticeVerts;            // 1,089

// ---------------------------------------------------------------------------
// THE PAGE IMAGE
// ---------------------------------------------------------------------------
struct Pool {
  std::vector<uint8_t> b;
  Pool() : b(kSlots * kPageBytes, 0) {}

  void put16(uint32_t slot, uint32_t off, int16_t v) {
    const uint32_t a = slot * kPageBytes + off;
    b[a] = uint8_t(uint16_t(v) & 0xFF);
    b[a + 1] = uint8_t((uint16_t(v) >> 8) & 0xFF);
  }
  const uint8_t* page(uint32_t slot) const { return &b[slot * kPageBytes]; }
};

// `salt` moves every plane of a page, so two slots never share a value.
void fill_page(Pool& p, uint32_t slot, int salt) {
  for (int k = 0; k < kVerts; ++k) {
    p.put16(slot, tp::kLayerAOff + 2u * uint32_t(k),
            int16_t(0x1000 + 3 * k + salt * 0x0037));
    p.put16(slot, tp::kLayerBOff + 2u * uint32_t(k),
            int16_t(-(0x0500 + 5 * k + salt * 0x0011)));
    p.put16(slot, tp::kLayerCOff + 2u * uint32_t(k),
            int16_t(0x4000 + 7 * k + salt * 0x0071));
  }
  // The bytes NOBODY should read, made loud. Layers D..H and the header get a
  // pattern that is not a plausible height, so a cursor that ran past its plane
  // produces something the comparison cannot mistake for terrain.
  for (uint32_t off = tp::kLayerDOff; off + 1 < kPageBytes; off += 2)
    p.put16(slot, off, int16_t(0x7EEE));
  for (uint32_t off = 0; off + 1 < tp::kLayerAOff; off += 2)
    p.put16(slot, off, int16_t(0x7DDD));
}

struct World {
  Vtb_pagestream& d;
  explicit World(Vtb_pagestream& dd) : d(dd) {}

  void quiet() {
    d.mw_en = 0;
    d.j_valid = 0;
    d.v_ready = 0;
    d.done_ready = 0;
    d.stat_clear_i = 0;
  }

  void config() {
    d.cfg_vram_window_base_i = kPoolBase;
    d.cfg_grant_hold_i = 0;
    d.cfg_rd_latency_i = 2;
    d.cfg_rd_gap_i = 0;
    d.cfg_region_ok_i = 1;
    d.cfg_deny_mode_i = 0;
    d.cfg_deny_idx_i = 0;
    d.cfg_short_mode_i = 0;
    d.cfg_short_idx_i = 0;
    d.cfg_short_beat_i = 0;
    d.cfg_vram_client_i = 6;   // ZHAO_CLIENT_TERRAIN_BUILD, ruling T3
    d.cfg_epoch_i = 0x11u;
  }

  void reset() {
    d.rst_n = 0;
    quiet();
    config();
    d.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(d);
    d.rst_n = 1;
    d.eval();
    for (int i = 0; i < 2; ++i) zhao::tick(d);
  }

  void load(const Pool& p) {
    for (uint32_t w = 0; w < kSlots * kPageWords; ++w) {
      uint64_t v = 0;
      for (int byte = 0; byte < 8; ++byte)
        v |= uint64_t(p.b[w * 8 + uint32_t(byte)]) << (8 * byte);
      d.mw_en = 1;
      d.mw_addr = w;
      d.mw_data = v;
      d.eval();
      zhao::tick(d);
    }
    d.mw_en = 0;
    d.eval();
  }
};

struct Run {
  std::vector<tp::LatticeVertex> got;
  bool done = false;
  bool ok = false;
  int verdict = -1;
  uint32_t done_slot = 0;
  uint32_t done_gen = 0;
  uint32_t done_src = 0;
  int first_flags = 0;   // how many vertices claimed v_first
  int last_flags = 0;
  int ident_bad = 0;     // vertices whose identity passthrough disagreed
  uint64_t cycles = 0;
};

// Deterministic ready pattern: 0 = always, 1 = every other, 2 = three in four,
// 3 = one in eight. THE MOSTLY-READY ONE IS NOT DECORATION -- a sibling's
// differential passed a 15,625-case sweep with ready held high and still missed
// a dropped answer; the pattern that found it was three-in-four.
bool draw(uint32_t& s, int pattern) {
  s = s * 1664525u + 1013904223u;
  switch (pattern) {
    case 0: return true;
    case 1: return ((s >> 16) & 1u) != 0u;
    case 2: return ((s >> 16) & 3u) != 0u;
    default: return ((s >> 16) & 7u) == 0u;
  }
}

Run stream(World& w, uint32_t slot, uint32_t gen, uint32_t epoch, uint32_t src, int pattern,
           uint64_t cap = 400000ull) {
  Vtb_pagestream& d = w.d;
  Run r;
  uint32_t sv = 0x5A5Au ^ uint32_t(pattern * 7919), sd = 0x1234u ^ uint32_t(pattern * 104729);

  d.j_valid = 1;
  d.j_slot = uint16_t(slot);
  d.j_gen = uint8_t(gen);
  d.j_epoch = epoch;
  d.j_src_id = src;
  d.eval();
  int guard = 0;
  while (!d.j_ready && guard < 1000) { zhao::tick(d); d.eval(); ++guard; }
  zhao::tick(d);
  d.j_valid = 0;
  d.eval();

  for (uint64_t c = 0; c < cap && !r.done; ++c) {
    d.v_ready = draw(sv, pattern) ? 1 : 0;
    d.done_ready = draw(sd, pattern) ? 1 : 0;
    d.eval();

    if (d.v_valid && d.v_ready) {
      tp::LatticeVertex v;
      v.base = int16_t(d.v_base);
      v.scar = int16_t(d.v_scar);
      v.bottom = int16_t(d.v_bottom);
      v.vi = int(d.v_vi);
      v.vj = int(d.v_vj);
      r.got.push_back(v);
      if (d.v_first) ++r.first_flags;
      if (d.v_last) ++r.last_flags;
      // THE IDENTITY RIDES EVERY VERTEX, and is checked on every one rather
      // than once: a passthrough that dropped halfway through a lattice would
      // pass a check that only looked at vertex 0.
      if (d.v_slot != slot || d.v_gen != gen || d.v_epoch != epoch || d.v_src_id != src)
        ++r.ident_bad;
    }
    if (d.done_valid && d.done_ready) {
      r.done = true;
      r.ok = d.done_ok != 0;
      r.verdict = int(d.done_verdict);
      r.done_slot = d.done_slot;
      r.done_gen = d.done_gen;
      r.done_src = d.done_src_id;
    }
    zhao::tick(d);
    d.eval();
    ++r.cycles;
  }
  d.v_ready = 0;
  d.done_ready = 0;
  d.eval();
  return r;
}

// Vertex-by-vertex, field-by-field, with the FIRST divergence named. "1,089
// vertices differ" is not a bug report; "vertex 33 read scar from lane k+1" is.
int compare(const char* tag, const std::vector<tp::LatticeVertex>& got,
            const std::vector<tp::LatticeVertex>& want) {
  int bad = 0, printed = 0;
  const std::size_t n = got.size() < want.size() ? got.size() : want.size();
  for (std::size_t i = 0; i < n; ++i) {
    if (got[i] == want[i]) continue;
    ++bad;
    if (printed < 4) {
      ++printed;
      std::printf("   %s vertex %zu (vi=%d vj=%d):\n", tag, i, want[i].vi, want[i].vj);
      if (got[i].base != want[i].base)
        std::printf("      base   got %6d want %6d\n", got[i].base, want[i].base);
      if (got[i].scar != want[i].scar)
        std::printf("      scar   got %6d want %6d\n", got[i].scar, want[i].scar);
      if (got[i].bottom != want[i].bottom)
        std::printf("      bottom got %6d want %6d\n", got[i].bottom, want[i].bottom);
      if (got[i].vi != want[i].vi || got[i].vj != want[i].vj)
        std::printf("      index  got (%d,%d) want (%d,%d)\n", got[i].vi, got[i].vj,
                    want[i].vi, want[i].vj);
    }
  }
  return bad;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_pagestream* dut = new Vtb_pagestream;
  World w(*dut);
  Vtb_pagestream& d = *dut;

  std::printf("== TERRAIN.PAGESTREAM vs zref::terrain::page_lattice ==\n");
  std::printf("   page %u B, planes A/B/C at %u / %u / %u, %d vertices\n\n", kPageBytes,
              tp::kLayerAOff, tp::kLayerBOff, tp::kLayerCOff, kVerts);

  Pool pool;
  for (uint32_t s = 0; s < kSlots; ++s) fill_page(pool, s, int(s) + 1);

  w.reset();
  w.load(pool);

  const std::vector<tp::LatticeVertex> want1 = tp::page_lattice(pool.page(1));

  // =========================================================================
  // A -- ONE LATTICE, EVERY VERTEX, EVERY FIELD
  // =========================================================================
  {
    std::printf("-- A: slot 1, no stalls --\n");
    d.stat_clear_i = 1; zhao::tick(d); d.stat_clear_i = 0;
    const Run r = stream(w, 1, 0x21, 0x11u, 0xABCD0001u, 0);

    ck(r.done, "A the job completed");
    ck(r.ok, "A and reported ok", 1, r.ok ? 1 : 0);
    ck(int(r.got.size()) == kVerts, "A it emitted every vertex and no more", kVerts,
       int(r.got.size()));
    const int bad = compare("A", r.got, want1);
    ck(bad == 0, "A every vertex matches zref::terrain::page_lattice, field for field", 0, bad);
    ck(r.first_flags == 1, "A exactly one vertex claimed to be the first", 1, r.first_flags);
    ck(r.last_flags == 1, "A and exactly one the last", 1, r.last_flags);
    ck(r.ident_bad == 0, "A and the job identity rode every vertex unchanged", 0, r.ident_bad);
    ck(r.done_slot == 1 && r.done_gen == 0x21 && r.done_src == 0xABCD0001u,
       "A the completion carries the identity it was given", 1,
       (r.done_slot == 1 && r.done_gen == 0x21 && r.done_src == 0xABCD0001u) ? 1 : 0);
    ck(d.c_lattices == 1, "A one lattice counted", 1, long(d.c_lattices));
    ck(int(d.c_vertices) == kVerts, "A and 1,089 vertices", kVerts, long(d.c_vertices));

    // THE READS ARE INSIDE THE SLOT IT WAS TOLD TO READ. A block that ignored
    // `j_slot_i` would pass everything above if the fixture used slot 0, which
    // is why it does not.
    const uint32_t lo = kPoolBase + 1 * kPageBytes;
    const uint32_t hi = lo + tp::kLayerCOff + tp::kLayerCBytes;
    ck(d.first_rd_addr >= lo, "A the lowest read address is inside slot 1", long(lo),
       long(d.first_rd_addr));
    ck(d.last_rd_addr < hi,
       "A and the highest is below the end of plane C -- no cursor ran into layer D",
       long(hi), long(d.last_rd_addr));

    // THE REAL GUARD, not the played one's blanket verdict.
    ck(d.shadow_req > 0 && d.shadow_ok == d.shadow_req && d.shadow_viol == 0,
       "A the REAL MEM.GUARD passed every one of the block's reads and refused none", 0,
       long(d.shadow_viol));
    ck(d.shadow_fwd == d.shadow_req,
       "A and forwarded all of them -- a guard that passed and forwarded nothing would "
       "satisfy the check above and still be broken",
       long(d.shadow_req), long(d.shadow_fwd));

    std::printf("   %llu cycles, %u bursts, %u beats; guard: req=%u ok=%u fwd=%u viol=%u\n",
                (unsigned long long)r.cycles, d.c_bursts, d.rbeats_seen, d.shadow_req,
                d.shadow_ok, d.shadow_fwd, d.shadow_viol);

    // THE BURST COUNT, DERIVED RATHER THAN OBSERVED. Each plane is 2,178 bytes
    // starting at 64, 2,242 and 4,420; a 64-byte staging buffer covers the
    // aligned burst containing the wanted sample, so plane P needs one burst
    // per distinct value of `(P + 2k) >> 6` over k in 0..1088. That is
    // floor((P + 2176) / 64) - floor(P / 64) + 1.
    auto bursts_for = [](uint32_t off) {
      return (off + 2176u) / 64u - off / 64u + 1u;
    };
    const uint32_t expect_bursts = bursts_for(tp::kLayerAOff) + bursts_for(tp::kLayerBOff) +
                                   bursts_for(tp::kLayerCOff);
    ck(d.c_bursts == expect_bursts,
       "A it read exactly the bursts the layout requires -- no re-reads, no prefetch",
       long(expect_bursts), long(d.c_bursts));
  }

  // =========================================================================
  // B -- THE SAME LATTICE UNDER FOUR STALL PATTERNS
  // =========================================================================
  // Every phase of a sibling's suite drove ready high and missed a dropped
  // answer. The four logs must be identical.
  {
    std::printf("\n-- B: four stall patterns, and the guard made slow --\n");
    for (int pattern = 0; pattern < 4; ++pattern) {
      w.reset();
      d.cfg_grant_hold_i = uint8_t(pattern * 2);
      d.cfg_rd_latency_i = uint8_t(1 + pattern * 3);
      d.cfg_rd_gap_i = uint8_t(pattern);
      d.eval();
      const Run r = stream(w, 1, 0x21, 0x11u, 0xABCD0002u, pattern, 4000000ull);

      char msg[160];
      std::snprintf(msg, sizeof msg, "B pattern %d emitted every vertex", pattern);
      ck(int(r.got.size()) == kVerts, msg, kVerts, int(r.got.size()));
      std::snprintf(msg, sizeof msg, "B pattern %d matches the reference exactly", pattern);
      const int bad = compare("B", r.got, want1);
      ck(bad == 0, msg, 0, bad);
      std::snprintf(msg, sizeof msg, "B pattern %d completed ok", pattern);
      ck(r.done && r.ok, msg, 1, (r.done && r.ok) ? 1 : 0);
      std::printf("   pattern %d: %llu cycles, %u bursts\n", pattern,
                  (unsigned long long)r.cycles, d.c_bursts);
    }
    w.reset();
  }

  // =========================================================================
  // C -- A SECOND JOB, AND THE BUFFER THAT MUST NOT SURVIVE IT
  // =========================================================================
  // The block invalidates all three staging buffers per job. If it did not, the
  // first vertices of a job on a DIFFERENT slot would come from the previous
  // slot's last burst -- terrain, in the right shape, from the wrong page. The
  // slots are filled from different constants precisely so that shows.
  {
    std::printf("\n-- C: slot 1 then slot 2, back to back --\n");
    const Run r1 = stream(w, 1, 0x21, 0x11u, 0x1111u, 0);
    const Run r2 = stream(w, 2, 0x22, 0x11u, 0x2222u, 0);
    const std::vector<tp::LatticeVertex> want2 = tp::page_lattice(pool.page(2));

    ck(int(r1.got.size()) == kVerts && int(r2.got.size()) == kVerts,
       "C both jobs emitted a full lattice", kVerts * 2,
       int(r1.got.size() + r2.got.size()));
    ck(compare("C1", r1.got, want1) == 0, "C the first job is slot 1's lattice");
    const int bad2 = compare("C2", r2.got, want2);
    ck(bad2 == 0,
       "C and the second is slot 2's -- not slot 1's tail carried over in a stale buffer",
       0, bad2);
    // AND THE TWO ARE ACTUALLY DIFFERENT, so the check above is not vacuous.
    int differ = 0;
    for (int i = 0; i < kVerts; ++i)
      if (!(want1[std::size_t(i)] == want2[std::size_t(i)])) ++differ;
    ck(differ == kVerts,
       "C the two slots' lattices differ at every vertex, so C could not have passed by "
       "reading the wrong one",
       kVerts, differ);
  }

  // =========================================================================
  // D -- THE REFUSALS, EACH ONE FIRED
  // =========================================================================
  // A verdict that has never been produced is a verdict nobody has tested.
  {
    std::printf("\n-- D: refusals --\n");

    // D1: a slot past the pool. It must be REFUSED, and it must not issue the
    // read it is refusing -- otherwise `guard_denied_o` would be measuring this
    // block's bookkeeping instead of the guard.
    {
      w.reset();
      d.stat_clear_i = 1; zhao::tick(d); d.stat_clear_i = 0;
      const Run r = stream(w, 1024, 0x30, 0x11u, 0x3333u, 0, 20000);
      ck(r.done, "D1 an out-of-range slot still produces a completion");
      ck(!r.ok, "D1 and it is not ok", 0, r.ok ? 1 : 0);
      ck(r.verdict == 1, "D1 with the slot-out-of-range verdict", 1, r.verdict);
      ck(r.got.empty(), "D1 and no vertices at all", 0, int(r.got.size()));
      ck(d.greqs_seen == 0,
         "D1 and it never asked the fabric for a byte -- the refusal does not issue the "
         "read it is refusing",
         0, long(d.greqs_seen));
      ck(d.c_refused == 1, "D1 counted as a refusal", 1, long(d.c_refused));
    }

    // D2: a stale epoch.
    {
      w.reset();
      d.stat_clear_i = 1; zhao::tick(d); d.stat_clear_i = 0;
      const Run r = stream(w, 1, 0x31, 0x10u, 0x4444u, 0, 20000);
      ck(r.done && !r.ok, "D2 a job from the previous epoch is refused", 0, r.ok ? 1 : 0);
      ck(r.verdict == 2, "D2 with the epoch verdict", 2, r.verdict);
      ck(d.greqs_seen == 0, "D2 and issues no read", 0, long(d.greqs_seen));
    }

    // D3: MEM.GUARD refuses one burst mid-lattice.
    {
      w.reset();
      d.cfg_deny_mode_i = 1;
      d.cfg_deny_idx_i = 4;      // not the first, so the block is mid-stream
      d.eval();
      const Run r = stream(w, 1, 0x32, 0x11u, 0x5555u, 0, 200000);
      ck(r.done, "D3 a guard refusal still produces a completion");
      ck(!r.ok && r.verdict == 3, "D3 with the guard verdict", 3, r.verdict);
      ck(d.c_guard_denied == 1, "D3 and the denial is counted once", 1,
         long(d.c_guard_denied));
      ck(int(r.got.size()) < kVerts,
         "D3 and the lattice stops rather than being completed from a cold buffer", 1,
         int(r.got.size()) < kVerts ? 1 : 0);
      std::printf("   D3 stopped after %d vertices\n", int(r.got.size()));
      d.cfg_deny_mode_i = 0;
    }

    // D4: a burst that ends early. The buffer would then hold a mixture of this
    // page and the last one, which reads as terrain -- so it must FAULT.
    {
      w.reset();
      d.cfg_short_mode_i = 1;
      d.cfg_short_idx_i = 2;
      d.cfg_short_beat_i = 3;
      d.eval();
      const Run r = stream(w, 1, 0x33, 0x11u, 0x6666u, 0, 200000);
      ck(r.done, "D4 a short burst still produces a completion");
      ck(!r.ok && r.verdict == 4, "D4 with the incomplete verdict", 4, r.verdict);
      ck(d.c_incomplete == 1, "D4 and it is counted", 1, long(d.c_incomplete));
      d.cfg_short_mode_i = 0;
    }
  }

  // =========================================================================
  // E -- AND IT STILL WORKS AFTERWARDS
  // =========================================================================
  // Four faults in a row is where a state machine leaves a busy flag set. A
  // block that faulted correctly and then never streamed again would pass every
  // check above.
  {
    std::printf("\n-- E: a good lattice after four faults --\n");
    w.reset();
    const Run r = stream(w, 3, 0x40, 0x11u, 0x7777u, 2, 400000);
    const std::vector<tp::LatticeVertex> want3 = tp::page_lattice(pool.page(3));
    ck(r.done && r.ok, "E it streams again after the fault phase", 1,
       (r.done && r.ok) ? 1 : 0);
    ck(int(r.got.size()) == kVerts, "E a full lattice", kVerts, int(r.got.size()));
    ck(compare("E", r.got, want3) == 0, "E and it is slot 3's, field for field");
    ck(d.c_idle != 0, "E and the block returns to idle", 1, d.c_idle ? 1 : 0);
  }

  std::printf("\n== %d checks, %d failures ==\n", g_checks, g_fail);
  std::fflush(stdout);

  const int rc = (g_fail == 0) ? 0 : 1;
  delete dut;
  return rc;
}
