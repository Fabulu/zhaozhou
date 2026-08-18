// raster_tilestore_random.cpp — RASTER.TILESTORE randomized differential test
// (design/contracts/RASTER.TILESTORE.md "Randomized differential tests";
// oracle reference/src/zrender/tilestore.cpp).
//
// Two lanes, both fully deterministic from fixed seeds (the PCG shape used by
// every other random lane in this tree — audio_fifo_random.cpp):
//
//   LANE A — free-running traffic. Every cycle, each of the five channels is
//     independently activated by its own PCG stream over a narrow address
//     window (so collisions between the write and the two read ports are
//     COMMON rather than rare — a wide window would make the bypass paths
//     almost never fire). Every RTL output is compared against
//     zref::TileStore on every cycle.
//
//   LANE B — the render/resolve duty cycle. Repeats the real pass shape:
//     clear the working tile, write all 256 pixels of it (in a PCG order,
//     with PCG read-modify-write reads mixed in), swap, then stream the
//     finished tile out of the resolve port WHILE the next tile is being
//     written into the working bank. Asserts what the ping-pong is for: the
//     tile the resolve port streams is bit-identical to the tile that was
//     written one pass earlier, no matter what the working bank is doing.
//
// Modes: default = 20,000 cycles for lane A and 24 passes for lane B (CTest
// fast); --nightly = 300,000 / 360. Failing vectors are saved (charter §29-17).

#include "raster_tilestore_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using zhao::check;
using zhao_raster::Cycle;
using zhao_raster::kWords;
using zhao_raster::make_word;
using zhao_raster::same;
using zhao_raster::StoreDev;
using zhao_raster::StoreOut;

namespace {

// PCG RXS-M-XS — the committed test PRNG shape (qformats.md §7.5 constants).
uint32_t pcg_perm(uint32_t s) {
  const uint32_t w = static_cast<uint32_t>(((s >> ((s >> 28) + 4)) ^ s) * 277803737u);
  return (w >> 22) ^ w;
}

struct Prng {
  uint32_t state;
  explicit Prng(uint32_t seed) : state(seed) {}
  uint32_t draw() {
    state = state * 747796405u + 2891336453u;
    return pcg_perm(state);
  }
  uint32_t below(uint32_t n) { return draw() % n; }
  bool chance(uint32_t num, uint32_t den) { return below(den) < num; }
  uint64_t word() { return (static_cast<uint64_t>(draw()) << 32) | draw(); }
};

std::vector<uint8_t> serialize(const Cycle& c) {
  std::vector<uint8_t> v;
  auto put64 = [&v](uint64_t x) {
    for (int i = 0; i < 8; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
  };
  put64(c.clear_data);
  put64(c.wr_data);
  v.push_back(c.wr_addr);
  v.push_back(c.rd_addr);
  v.push_back(c.res_addr);
  v.push_back(static_cast<uint8_t>((c.clear ? 1 : 0) | (c.wr ? 2 : 0) | (c.rd ? 4 : 0) |
                                   (c.res ? 8 : 0) | (c.swap ? 16 : 0)));
  return v;
}

uint32_t g_failures = 0;

bool one(StoreDev& dev, zref::TileStore& model, const Cycle& c, const char* lane, uint32_t i,
         StoreOut* out) {
  const StoreOut want = model.step(c);
  const StoreOut got = dev.step(c);
  *out = got;
  if (same(want, got)) return true;
  if (g_failures < 8) {
    const std::string body = zhao_raster::describe(c, want, got);
    std::printf("  %s[%u]: RTL != zref::TileStore\n    %s\n", lane, i, body.c_str());
    char name[64];
    std::snprintf(name, sizeof(name), "raster_tilestore_%s_%u", lane, i);
    zhao::save_failing_vector(name, serialize(c), "zref::TileStore", body);
  }
  ++g_failures;
  return false;
}

// --------------------------------------------------------------------- A ---
void lane_a(StoreDev& dev, uint32_t cycles) {
  zref::TileStore model;
  dev.reset();
  Prng rng(0x5EED'0021u);

  uint32_t hits_bypass = 0;
  uint32_t hits_clear_wr = 0;
  uint32_t hits_swap = 0;

  for (uint32_t i = 0; i < cycles; ++i) {
    // A NARROW address window makes the write/read collisions that exercise
    // the bypass common instead of a 1-in-256 accident.
    const uint32_t win = 1u << (2u + rng.below(5u));  // 4..64 addresses
    const uint32_t base = rng.below(256u);
    auto addr = [&]() { return static_cast<uint8_t>((base + rng.below(win)) & 0xFFu); };

    Cycle c;
    c.clear = rng.chance(1, 40);
    c.clear_data = rng.word();
    c.wr = rng.chance(3, 5);
    c.wr_addr = addr();
    c.wr_data = rng.word();
    c.rd = rng.chance(3, 5);
    c.rd_addr = addr();
    c.rd_src_id = static_cast<uint16_t>(rng.draw());
    c.res = rng.chance(1, 2);
    c.res_addr = addr();
    c.swap = rng.chance(1, 60);

    if (c.wr && c.rd && !c.clear && c.wr_addr == c.rd_addr) ++hits_bypass;
    if (c.wr && c.clear) ++hits_clear_wr;
    if (c.swap) ++hits_swap;

    StoreOut got;
    one(dev, model, c, "laneA", i, &got);
  }

  std::printf(
      "raster_tilestore_random lane A: %u cycles, %u write-through-read bypasses, "
      "%u clear/write races, %u swaps\n",
      cycles, hits_bypass, hits_clear_wr, hits_swap);
  // The lane is worthless if the interesting cases never fired.
  check(hits_bypass > 0, "lane A: the read-during-write bypass actually fired", 1,
        hits_bypass > 0 ? 1 : 0);
  check(hits_clear_wr > 0, "lane A: the clear/write race actually fired", 1,
        hits_clear_wr > 0 ? 1 : 0);
  check(hits_swap > 0, "lane A: swaps actually happened", 1, hits_swap > 0 ? 1 : 0);
}

// --------------------------------------------------------------------- B ---
void lane_b(StoreDev& dev, uint32_t passes) {
  zref::TileStore model;
  dev.reset();
  Prng rng(0x5EED'0022u);

  std::vector<uint64_t> prev(kWords, 0);  // the tile currently on the resolve port
  bool have_prev = false;
  uint32_t resolve_bad = 0;
  uint32_t resolved = 0;

  for (uint32_t p = 0; p < passes; ++p) {
    // ---- pass 1: clear the working tile ---------------------------------
    const uint64_t sky = rng.word();
    Cycle c;
    c.clear = true;
    c.clear_data = sky;
    StoreOut o;
    one(dev, model, c, "laneB", p, &o);

    // ---- passes 2..8: write the tile, in a PCG order, with RMW reads -----
    std::vector<uint64_t> tile(kWords, sky);
    std::vector<uint8_t> order(kWords);
    for (int i = 0; i < kWords; ++i) order[i] = static_cast<uint8_t>(i);
    for (int i = kWords - 1; i > 0; --i) {
      const int j = static_cast<int>(rng.below(static_cast<uint32_t>(i + 1)));
      const uint8_t t = order[i];
      order[i] = order[j];
      order[j] = t;
    }

    uint32_t stream = 0;  // resolve-port cursor for the PREVIOUS tile
    for (int i = 0; i < kWords; ++i) {
      Cycle w;
      w.wr = true;
      w.wr_addr = order[i];
      w.wr_data = rng.word();
      tile[order[i]] = w.wr_data;
      // a read-modify-write style read of the pixel being written, or a
      // neighbour — both are what the fragment pipeline does
      w.rd = true;
      w.rd_addr = rng.chance(1, 2) ? order[i] : static_cast<uint8_t>(rng.below(256u));
      // ...WHILE the finished tile streams out of the resolve port
      if (have_prev && stream < static_cast<uint32_t>(kWords)) {
        w.res = true;
        w.res_addr = static_cast<uint8_t>(stream);
      }
      StoreOut so;
      one(dev, model, w, "laneB", p, &so);
      if (w.res) {
        if (so.res_data != prev[stream]) ++resolve_bad;
        ++stream;
        ++resolved;
      }
    }
    // drain any remainder of the previous tile
    while (have_prev && stream < static_cast<uint32_t>(kWords)) {
      Cycle r;
      r.res = true;
      r.res_addr = static_cast<uint8_t>(stream);
      StoreOut so;
      one(dev, model, r, "laneB", p, &so);
      if (so.res_data != prev[stream]) ++resolve_bad;
      ++stream;
      ++resolved;
    }

    // ---- pass 9: swap; the tile just written becomes the resolve tile ----
    Cycle s;
    s.swap = true;
    StoreOut so2;
    one(dev, model, s, "laneB", p, &so2);
    prev = tile;
    have_prev = true;
  }

  std::printf("raster_tilestore_random lane B: %u passes, %u resolve-port words checked\n", passes,
              resolved);
  check(resolve_bad == 0,
        "lane B: the resolve port streams the tile written one pass earlier, byte for byte", 0,
        resolve_bad);
  check(resolved > 0, "lane B: the resolve port was actually streamed", 1, resolved > 0 ? 1 : 0);
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  static StoreDev dev;
  lane_a(dev, nightly ? 300000u : 20000u);
  lane_b(dev, nightly ? 360u : 24u);

  check(g_failures == 0, "randomized differential: RTL == zref::TileStore on every cycle", 0,
        g_failures);
  return zhao::report_and_exit("raster_tilestore_random");
}
