// terrain_normalmap_directed.cpp
//
// TERRAIN.NORMALMAP's acceptance suite: the rebuilt zhao_terrain_normalmap
// against zref::terrain::normalmap_delta_s9 (the block law appended to the
// ratified oracle header 2026-09-09) and the pyramid addressing law
// zref::terrain::normalmap_pyramid_addr.
//
// PLACEMENT NOTE: this lives in tests/texture/ rather than tests/terrain/
// because tests/terrain/ was owned by a live rearchitecture lane on the day
// this was written, and the block is a fragment-stream organ beside the
// texture path anyway. Moving it later is a rename, not a rewrite.
//
// What it pins, in order:
//   1. COLD LAW      — after reset, before any epoch, live fragments emit
//                      delta 0 and count in cold_o; detail=0 counts zeroed.
//   2. EPOCH FILL    — a cfg write to sun/strength refills the K tables in a
//                      bounded window; a write landing mid-fill re-runs it.
//   3. ROUNDING      — round-half-up on NEGATIVE products, pinned with two
//                      LITERAL vectors (0 and -1 either side of the tie), not
//                      just oracle agreement — a floor-shift and a truncation
//                      both disagree here, and so does the contract's
//                      original off-by-one shift of 23 (these literals rail
//                      the factor-of-two shut).
//   4. CORNERS/RAILS — full-scale texel and sun both signs; railed_o counts.
//   5. STRENGTH 0    — bit-exact off across random texels.
//   6. UV WRAP/SHIFT — uv_shift 0 and 15, negative UV, the 64-texel seam.
//   7. MIP TAIL      — distinct texels per level, f_lod_i sweep, lod_bias,
//                      max_level clamp; max_level 0 = un-mipped, bit-exact.
//   8. BACKPRESSURE  — random consumer stalls: no drop, no reorder, src_id
//                      echo intact; then II=1 measured exactly (64 fragments
//                      in 64 cycles), not asserted.
//   9. RANDOM        — 2000-fragment differential with coverage asserts:
//                      the run must have SEEN negative, positive, railed,
//                      zeroed and nonzero-level fragments or it fails.
//  10. COUNTERS      — exact equality against the model, every counter seen
//                      to move (fragments_o, zeroed_o, railed_o, cold_o).
//
// POSITIVE CONTROL: --break-oracle corrupts ONE expected delta; the suite
// must then FAIL. Run it whenever you doubt the checker is alive.
#include "Vzhao_terrain_normalmap.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

#include "verilated.h"

#include "zref/zref_terrain_normalmap.hpp"

// verilated.cpp of this oss-cad-suite build references the legacy SystemC
// time hook even in plain C++ mode; the harness lib hosts this shim, and a
// standalone build has to bring its own (tests/harness/zhao_sim.cpp, P1
// gotcha 6).
double sc_time_stamp() { return 0.0; }

namespace {

int g_checks = 0;
int g_fails = 0;

#define CHECK(cond, ...)                       \
  do {                                         \
    ++g_checks;                                \
    if (!(cond)) {                             \
      ++g_fails;                               \
      std::printf("FAIL(%d): ", __LINE__);     \
      std::printf(__VA_ARGS__);                \
      std::printf("\n");                       \
    }                                          \
  } while (0)

struct Frag {
  int32_t u, v;
  bool detail;
  uint8_t lod;
  uint16_t src;
};

struct Exp {
  int32_t delta;
  uint16_t src;
};

// The model's mirror of the machine.
struct Model {
  uint16_t tile[zref::terrain::kNormalmapPyramidWords] = {};
  int sun_x15 = 0, sun_z15 = 0;
  int strength = 0;
  int uv_shift = 0;
  int lod_bias = 0;
  int max_level = 0;
  bool tables_ready = false;

  uint64_t n_zeroed = 0, n_railed = 0, n_cold = 0, n_frag = 0;

  Exp expect(const Frag& f) {
    ++n_frag;
    if (!f.detail) {
      ++n_zeroed;
      return {0, f.src};
    }
    if (!tables_ready) {
      ++n_cold;
      return {0, f.src};
    }
    const int lvl = zref::terrain::normalmap_level_select(f.lod, lod_bias, max_level);
    const int u6 = (static_cast<uint32_t>(f.u) >> uv_shift) & 63;
    const int v6 = (static_cast<uint32_t>(f.v) >> uv_shift) & 63;
    const int addr = zref::terrain::normalmap_pyramid_addr(lvl, u6, v6);
    const zref::terrain::DetailNormal d = zref::terrain::normalmap_decode(tile[addr]);
    const int32_t delta =
        zref::terrain::normalmap_delta_s9(d, sun_x15, sun_z15, /*b*/ 0, 0, strength);
    if (delta == 255 || delta == -256) {
      // The s9 rails are only reachable through saturation with these suns.
      const int64_t raw = zref::terrain::rshift_round(
          (static_cast<int64_t>(d.nx) * sun_x15 + static_cast<int64_t>(d.nz) * sun_z15) *
              strength,
          zref::terrain::kNormalmapDeltaShift);
      if (raw > 255 || raw < -256) ++n_railed;
    }
    return {delta, f.src};
  }
};

struct Bench {
  Vzhao_terrain_normalmap* d;
  Model m;
  std::deque<Frag> pending;
  std::deque<Exp> expected;
  std::deque<Exp> got;
  uint64_t cycles = 0;
  uint32_t rng = 0x2eb00b1e;

  uint32_t rnd() {
    rng = rng * 1664525u + 1013904223u;
    return rng >> 8;
  }

  void half() {  // one full clock with pre-edge sampling
    d->clk = 0;
    d->eval();
    const bool in_fire = d->f_valid_i && d->f_ready_o;
    const bool out_fire = d->d_valid_o && d->d_ready_i;
    if (out_fire) {
      const int32_t delta = (static_cast<int32_t>(d->d_delta_o & 0x1FF) ^ 0x100) - 0x100;
      got.push_back({delta, static_cast<uint16_t>(d->d_src_id_o)});
    }
    d->clk = 1;
    d->eval();
    ++cycles;
    if (in_fire && !pending.empty()) pending.pop_front();
  }

  // Drive everything queued, with a consumer-stall pattern; returns cycles used.
  uint64_t run(int stall_percent, uint64_t limit = 200000) {
    const uint64_t start = cycles;
    uint64_t guard = 0;
    while ((!pending.empty() || got.size() < expected.size()) && guard++ < limit) {
      if (!pending.empty()) {
        const Frag& f = pending.front();
        d->f_valid_i = 1;
        d->f_u_i = static_cast<uint32_t>(f.u);
        d->f_v_i = static_cast<uint32_t>(f.v);
        d->f_detail_i = f.detail;
        d->f_lod_i = f.lod & 0xF;
        d->f_src_id_i = f.src;
      } else {
        d->f_valid_i = 0;
      }
      d->d_ready_i = (static_cast<int>(rnd() % 100) >= stall_percent) ? 1 : 0;
      half();
    }
    d->f_valid_i = 0;
    d->d_ready_i = 1;
    CHECK(guard < limit, "run() hung: pending=%u got=%u expected=%u",
          static_cast<unsigned>(pending.size()), static_cast<unsigned>(got.size()),
          static_cast<unsigned>(expected.size()));
    return cycles - start;
  }

  void quiesce(int n = 12) {
    d->f_valid_i = 0;
    d->d_ready_i = 1;
    for (int i = 0; i < n; ++i) half();
  }

  void offer(const Frag& f) {
    pending.push_back(f);
    expected.push_back(m.expect(f));
  }

  void cfg(uint8_t addr, uint32_t data) {
    d->cfg_we_i = 1;
    d->cfg_addr_i = addr & 7;
    d->cfg_data_i = data;
    half();
    d->cfg_we_i = 0;
  }

  // cfg writes that mirror into the model
  void set_sun(int sx15, int sz15) {
    m.sun_x15 = sx15;
    m.sun_z15 = sz15;
    m.tables_ready = false;
    cfg(0, (static_cast<uint32_t>(sz15 & 0xFFFF) << 16) | static_cast<uint32_t>(sx15 & 0xFFFF));
  }
  void set_strength(int s) {
    m.strength = s;
    m.tables_ready = false;
    cfg(2, static_cast<uint32_t>(s & 0xFF));
  }
  void set_uv_shift(int s) {
    m.uv_shift = s;
    cfg(3, static_cast<uint32_t>(s & 0xF));
  }
  void set_lodcfg(int bias, int max_level) {
    m.lod_bias = bias;
    m.max_level = max_level;
    cfg(4, (static_cast<uint32_t>(bias & 0x1F) << 4) | static_cast<uint32_t>(max_level & 7));
  }

  // Wait for the epoch fill; returns cycles it took.
  uint64_t wait_ready(uint64_t cap = 600) {
    const uint64_t start = cycles;
    while (!d->table_ready_o && cycles - start < cap) half();
    CHECK(d->table_ready_o, "table_ready_o did not rise within %llu cycles",
          static_cast<unsigned long long>(cap));
    m.tables_ready = true;
    return cycles - start;
  }

  void upload(int addr, uint16_t data) {
    m.tile[addr] = data;
    d->tw_we_i = 1;
    d->tw_addr_i = addr & 0x1FFF;
    d->tw_data_i = data;
    half();
    d->tw_we_i = 0;
  }

  // Compare everything drained so far, in order.
  void drain_and_compare(const char* phase) {
    quiesce(16);
    CHECK(got.size() == expected.size(), "%s: got %u deltas, expected %u", phase,
          static_cast<unsigned>(got.size()), static_cast<unsigned>(expected.size()));
    const size_t n = got.size() < expected.size() ? got.size() : expected.size();
    for (size_t i = 0; i < n; ++i) {
      CHECK(got[i].src == expected[i].src, "%s[%u]: src_id %u != expected %u", phase,
            static_cast<unsigned>(i), got[i].src, expected[i].src);
      CHECK(got[i].delta == expected[i].delta, "%s[%u] src=%u: delta %d != expected %d", phase,
            static_cast<unsigned>(i), got[i].src, got[i].delta, expected[i].delta);
    }
    got.clear();
    expected.clear();
  }
};

uint16_t texel(int dx, int dz) {
  return static_cast<uint16_t>(((dz & 0xFF) << 8) | (dx & 0xFF));
}

}  // namespace

int main(int argc, char** argv) {
  bool break_oracle = false;
  for (int i = 1; i < argc; ++i)
    if (!std::strcmp(argv[i], "--break-oracle")) break_oracle = true;

  Bench b;
  b.d = new Vzhao_terrain_normalmap;
  Vzhao_terrain_normalmap* d = b.d;

  d->rst_n = 0;
  d->f_valid_i = 0;
  d->d_ready_i = 0;
  d->cfg_we_i = 0;
  d->tw_we_i = 0;
  b.half();
  b.half();
  d->rst_n = 1;
  b.half();

  // ---- 1. COLD LAW --------------------------------------------------------
  CHECK(!d->table_ready_o, "tables claim ready straight out of reset");
  for (int i = 0; i < 4; ++i) b.offer({0x00010000 * i, 0x00020000, true, 0, static_cast<uint16_t>(0x100 + i)});
  b.offer({0, 0, false, 0, 0x1FF});  // detail=0 while cold: zeroed wins
  b.run(0);
  b.drain_and_compare("cold");
  CHECK(d->cold_o == 4, "cold_o=%u, want 4", d->cold_o);
  CHECK(d->zeroed_o == 1, "zeroed_o=%u, want 1", d->zeroed_o);

  // ---- 2. EPOCH FILL ------------------------------------------------------
  b.set_sun(32767, 0);  // unit sun along +X (s1.15)
  b.set_strength(255);
  const uint64_t fill = b.wait_ready();
  CHECK(fill >= 250 && fill <= 400, "epoch fill took %llu cycles, want ~267",
        static_cast<unsigned long long>(fill));
  // A write landing mid-fill must re-run the fill, not publish torn tables.
  b.set_sun(32767, 0);
  for (int i = 0; i < 5; ++i) b.half();
  CHECK(!d->table_ready_o, "table_ready_o high 5 cycles after an epoch write");
  b.set_sun(32767, 0);  // land a second write mid-fill
  b.wait_ready(1200);

  // ---- 3/4. CORNERS, RAILS, ROUNDING --------------------------------------
  b.set_uv_shift(4);
  // texel index law: u6 = (u >> 4) & 63. Park distinct texels at a few spots.
  b.upload(zref::terrain::normalmap_pyramid_addr(0, 5, 9), texel(127, 0));
  b.upload(zref::terrain::normalmap_pyramid_addr(0, 6, 9), texel(-128, -128));
  b.upload(zref::terrain::normalmap_pyramid_addr(0, 7, 9), texel(127, 127));
  b.offer({5 << 4, 9 << 4, true, 0, 1});  // +127 * +32767 -> near full scale
  b.offer({6 << 4, 9 << 4, true, 0, 2});
  b.offer({7 << 4, 9 << 4, true, 0, 3});
  b.run(0);
  b.drain_and_compare("corner-sunX");
  // Full-scale check against the LAW, not just the oracle: strength 255,
  // d=(127,·), sun=(+~1,0): delta must land at 252 (= 255*127*32767/2^22
  // rounded), i.e. essentially full colour scale. The contract's original
  // shift of 23 would give 126 — this literal is the factor-of-two's grave.
  {
    const zref::terrain::DetailNormal dn{127, 0};
    const int32_t v = zref::terrain::normalmap_delta_s9(dn, 32767, 0, 0, 0, 255);
    // 255*127*32767 = 1,061,159,295; rescale_s(.,22) = 253 -- essentially the
    // full colour scale. The contract's original shift of 23 gives 127: half
    // the relief. This literal is the factor-of-two's grave.
    CHECK(v == 253, "full-scale delta law: got %d, want 253", v);
  }

  // Rails, both signs, sun at the (legal, ugly) register rails.
  b.set_sun(-32768, -32768);
  b.wait_ready();
  const uint32_t railed_before = d->railed_o;
  b.offer({6 << 4, 9 << 4, true, 0, 4});  // d=(-128,-128): dot=+2^23 -> +510 -> +255 rail
  b.offer({7 << 4, 9 << 4, true, 0, 5});  // d=(+127,+127): -> -506 -> -256 rail
  b.run(0);
  CHECK(b.expected.size() == 2 && b.expected[0].delta == 255 && b.expected[1].delta == -256,
        "rail expectations wrong in the model itself");
  b.drain_and_compare("rails");
  CHECK(d->railed_o == railed_before + 2, "railed_o=%u, want %u", d->railed_o,
        railed_before + 2);

  // Round-half-up on negative products, pinned with literals.
  // dot = -128*16384 = -2^21, strength 1: (-2^21 + 2^21) >> 22 == 0.
  // dot = -2^21 - 1: rounds to -1. Floor or truncation would disagree.
  b.set_sun(16384, 1);
  b.set_strength(1);
  b.wait_ready();
  b.upload(zref::terrain::normalmap_pyramid_addr(0, 8, 9), texel(-128, 0));
  b.upload(zref::terrain::normalmap_pyramid_addr(0, 9, 9), texel(-128, -1));
  b.offer({8 << 4, 9 << 4, true, 0, 6});
  b.offer({9 << 4, 9 << 4, true, 0, 7});
  b.run(0);
  CHECK(b.expected.size() == 2 && b.expected[0].delta == 0 && b.expected[1].delta == -1,
        "tie-rounding expectations wrong in the model itself: %d %d",
        b.expected.empty() ? 999 : b.expected[0].delta,
        b.expected.size() < 2 ? 999 : b.expected[1].delta);
  b.drain_and_compare("neg-tie");

  // ---- 5. STRENGTH 0 = BIT-EXACT OFF --------------------------------------
  b.set_sun(23170, 23170);
  b.set_strength(0);
  b.wait_ready();
  for (int i = 0; i < 32; ++i)
    b.offer({static_cast<int32_t>(b.rnd()), static_cast<int32_t>(b.rnd()), true,
             static_cast<uint8_t>(i & 7), static_cast<uint16_t>(0x300 + i)});
  b.run(0);
  for (const Exp& e : b.expected) CHECK(e.delta == 0, "strength-0 model nonzero?!");
  b.drain_and_compare("strength0");

  // ---- 6. UV WRAP AND SHIFT EXTREMES --------------------------------------
  b.set_strength(200);
  b.wait_ready();
  b.set_uv_shift(0);
  b.upload(zref::terrain::normalmap_pyramid_addr(0, 63, 0), texel(64, -3));
  b.upload(zref::terrain::normalmap_pyramid_addr(0, 0, 63), texel(-9, 88));
  b.offer({-1, 0, true, 0, 10});           // u=-1 wraps to texel 63
  b.offer({64, -64, true, 0, 11});         // u=64 wraps to 0, v=-64 wraps to 0... v=-64 -> 0? (-64)&63 = 0
  b.run(0);
  b.drain_and_compare("wrap-shift0");
  b.set_uv_shift(15);
  b.upload(zref::terrain::normalmap_pyramid_addr(0, 21, 42), texel(-77, 33));
  b.offer({21 << 15, 42 << 15, true, 0, 12});
  b.offer({(21 << 15) + 0x7FFF, (42 << 15) + 1, true, 0, 13});  // same texel, sub-texel offsets
  b.run(0);
  b.drain_and_compare("shift15");

  // ---- 7. MIP TAIL --------------------------------------------------------
  b.set_uv_shift(4);
  b.set_lodcfg(0, 6);
  const int u6 = 37, v6 = 22;
  for (int lvl = 0; lvl < 7; ++lvl)
    b.upload(zref::terrain::normalmap_pyramid_addr(lvl, u6, v6),
             texel(10 + lvl * 7, -20 - lvl * 5));
  for (int lod = 0; lod < 8; ++lod)  // 7 must clamp to 6
    b.offer({u6 << 4, v6 << 4, true, static_cast<uint8_t>(lod),
             static_cast<uint16_t>(0x400 + lod)});
  b.run(0);
  b.drain_and_compare("mip-sweep");
  // A cfg write lands immediately; offers are only queued. Drain around each
  // change so the driver and the model describe the same machine.
  b.set_lodcfg(-3, 6);  // negative bias clamps at 0
  b.offer({u6 << 4, v6 << 4, true, 1, 0x410});
  b.run(0);
  b.drain_and_compare("mip-bias-neg");
  b.set_lodcfg(2, 6);   // positive bias
  b.offer({u6 << 4, v6 << 4, true, 1, 0x411});
  b.run(0);
  b.drain_and_compare("mip-bias-pos");
  b.set_lodcfg(0, 2);   // max_level clamp
  b.offer({u6 << 4, v6 << 4, true, 6, 0x412});
  b.run(0);
  b.drain_and_compare("mip-maxlevel");
  b.set_lodcfg(0, 0);   // reset state: un-mipped, must equal level 0
  b.offer({u6 << 4, v6 << 4, true, 5, 0x413});
  b.run(0);
  b.drain_and_compare("mip-off");

  // ---- 8. BACKPRESSURE, THEN II=1 MEASURED --------------------------------
  for (int i = 0; i < 200; ++i)
    b.offer({static_cast<int32_t>(b.rnd()), static_cast<int32_t>(b.rnd()), (b.rnd() % 8) != 0,
             static_cast<uint8_t>(b.rnd() % 16), static_cast<uint16_t>(0x500 + i)});
  b.run(60);  // consumer stalls 60% of cycles
  b.drain_and_compare("backpressure");

  {
    b.quiesce();
    for (int i = 0; i < 64; ++i)
      b.offer({static_cast<int32_t>(i << 4), 0, true, 0, static_cast<uint16_t>(0x600 + i)});
    const uint64_t took = b.run(0);
    // 64 fragments, latency 6, unstalled: II=1 means 64 accepts back to back.
    CHECK(took <= 64 + 10, "II: 64 fragments took %llu cycles (II=1 wants ~70)",
          static_cast<unsigned long long>(took));
    b.drain_and_compare("ii");
  }

  // ---- 9. RANDOM DIFFERENTIAL WITH COVERAGE -------------------------------
  b.set_sun(-27000, 18000);
  b.set_strength(255);
  b.wait_ready();
  b.set_lodcfg(0, 6);
  // Fill the WHOLE pyramid with random texels: with sparse fill, the >=lod-6
  // majority of a uniform lod sweep funnels into the single 1x1-level word
  // and the coverage asserts starve (seen happen: 15 positive deltas).
  for (int a = 0; a < zref::terrain::kNormalmapPyramidWords; ++a)
    b.upload(a, static_cast<uint16_t>(b.rnd()));
  // Guarantee the rails are reachable inside the sweep.
  b.upload(zref::terrain::normalmap_pyramid_addr(0, 1, 1), texel(-128, 127));
  b.upload(zref::terrain::normalmap_pyramid_addr(0, 2, 2), texel(127, -128));
  int cov_neg = 0, cov_pos = 0, cov_rail = 0, cov_zero_in = 0, cov_lvl = 0;
  for (int i = 0; i < 2000; ++i) {
    Frag f{static_cast<int32_t>(b.rnd()), static_cast<int32_t>(b.rnd()), (b.rnd() % 10) != 0,
           static_cast<uint8_t>(b.rnd() % 16), static_cast<uint16_t>(0x800 + (i & 0x7FF))};
    if (i == 100) f = {1 << 4, 1 << 4, true, 0, 0xF00};  // aimed at a rail texel
    if (i == 200) f = {2 << 4, 2 << 4, true, 0, 0xF01};
    b.offer(f);
    const Exp& e = b.expected.back();
    if (!f.detail) ++cov_zero_in;
    if (e.delta < 0) ++cov_neg;
    if (e.delta > 0) ++cov_pos;
    if (e.delta == 255 || e.delta == -256) ++cov_rail;
    if (f.detail && zref::terrain::normalmap_level_select(f.lod, 0, 6) > 0) ++cov_lvl;
  }
  if (break_oracle && !b.expected.empty()) {
    std::printf("POSITIVE CONTROL: corrupting expected[%zu] by +1\n", b.expected.size() / 2);
    b.expected[b.expected.size() / 2].delta += 1;
  }
  b.run(30);
  b.drain_and_compare("random");
  CHECK(cov_neg > 50, "coverage: only %d negative deltas sampled", cov_neg);
  CHECK(cov_pos > 50, "coverage: only %d positive deltas sampled", cov_pos);
  CHECK(cov_rail >= 2, "coverage: only %d railed deltas sampled", cov_rail);
  CHECK(cov_zero_in > 20, "coverage: only %d detail=0 fragments sampled", cov_zero_in);
  CHECK(cov_lvl > 200, "coverage: only %d nonzero-level fragments sampled", cov_lvl);

  // ---- 10. COUNTERS: EXACT, AND SEEN TO MOVE ------------------------------
  b.quiesce();
  CHECK(d->fragments_o == b.m.n_frag, "fragments_o=%u, model says %llu", d->fragments_o,
        static_cast<unsigned long long>(b.m.n_frag));
  CHECK(d->zeroed_o == b.m.n_zeroed, "zeroed_o=%u, model says %llu", d->zeroed_o,
        static_cast<unsigned long long>(b.m.n_zeroed));
  CHECK(d->railed_o == b.m.n_railed, "railed_o=%u, model says %llu", d->railed_o,
        static_cast<unsigned long long>(b.m.n_railed));
  CHECK(d->cold_o == b.m.n_cold, "cold_o=%u, model says %llu", d->cold_o,
        static_cast<unsigned long long>(b.m.n_cold));
  CHECK(d->fragments_o > 0 && d->zeroed_o > 0 && d->railed_o > 0 && d->cold_o > 0,
        "a counter never fired: frag=%u zero=%u rail=%u cold=%u", d->fragments_o, d->zeroed_o,
        d->railed_o, d->cold_o);
  CHECK(d->idle_o == 1, "idle_o low after quiesce");

  std::printf("terrain_normalmap_directed: %d checks, %d failures\n", g_checks, g_fails);
  delete d;
  return g_fails == 0 ? 0 : 1;
}
