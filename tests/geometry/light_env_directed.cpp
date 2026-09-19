// light_env_directed.cpp -- zhao_light_env, SetEnvironment -> GEOM.LIGHT's bank
// (owner ruling R25, entry I48), differenced word for word against the bridge
// `zref::light_env::bank_of` (reference/include/zref/zref_light_env.hpp).
//
// The bench plays zhao_light_stream's cfg port: it records every write by
// address and every commit, and it plays the stream's `idle_o`.
//
//   1  POWER-ON: with no record offered, reset alone loads the bank with
//      bank_of(EnvState{}) -- the parameter defaults ARE 4a's default record.
//   2  400 random records (angles over the full circle, every rgb565): every
//      word of every load equals the bridge; 14 writes, then ONE commit, after
//      the last write; nlights = 1.
//   3  HOLD: while the stream reports busy, hold_o is up and NOTHING is
//      written; the writes begin only after idle, and hold drops after commit.
//   4  SUPERSEDED: two records before a load starts -- the counter fires, and
//      the bank carries the SECOND (4a: the last record wins).
//   5  A record arriving MID-LOAD is loaded next, not lost and not mixed in.
#include <cstdint>
#include <cstdio>
#include <map>
#include <random>

#include "verilated.h"
#include "Vzhao_light_env.h"
#include "zhao_sim.hpp"
#include "zref/zref_light_env.hpp"

namespace {

int g_checks = 0, g_fail = 0;
void ck(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

struct Bench {
  Vzhao_light_env t;
  std::map<unsigned, uint32_t> words;   // writes since the last commit
  int writes = 0, commits = 0;
  bool wrote_while_busy = false;
  bool idle = true;

  void step() {
    t.stream_idle_i = idle ? 1 : 0;
    zhao::tick(t);
    // outputs are registered: sample after the edge
    if (t.cfg_we_o) {
      words[t.cfg_addr_o] = t.cfg_data_o;
      ++writes;
      if (!idle) wrote_while_busy = true;
    }
    if (t.cfg_commit_o) ++commits;
  }

  void offer(const zref::sky::EnvState& e) {
    t.e_valid_i = 1;
    t.e_sun_yaw_i = e.sun_yaw.raw;
    t.e_sun_pitch_i = e.sun_pitch.raw;
    t.e_sun_colour_i = e.sun_colour.bits;
    t.e_ambient_i = e.ambient.bits;
    step();
    t.e_valid_i = 0;
  }

  // Run until a commit lands; returns false on timeout.
  bool until_commit(int limit = 200) {
    const int c0 = commits;
    for (int i = 0; i < limit; ++i) {
      step();
      if (commits != c0) return true;
    }
    return false;
  }

  bool matches(const zref::light_env::Bank& b, const char* what) {
    bool ok = true;
    for (int w = 0; w < 4; ++w) {
      ok &= words.count(w) && words[w] == static_cast<uint32_t>(b.light0_a[w]);
      ok &= words.count(8 + w) && words[8 + w] == b.light0_b[w];
    }
    for (int w = 0; w < 6; ++w) ok &= words.count(0xF0 + w) && words[0xF0 + w] == b.env[w];
    ok &= words.size() == 14;
    ok &= t.nlights_o == b.nlights;
    if (!ok) {
      std::printf("  %s: got", what);
      for (auto& kv : words) std::printf(" [%02X]=%08X", kv.first, kv.second);
      std::printf("\n  want Lx=%08X Ly=%08X Lz=%08X B=%08X,%08X env=%X,%X,%X\n",
                  static_cast<uint32_t>(b.light0_a[0]), static_cast<uint32_t>(b.light0_a[1]),
                  static_cast<uint32_t>(b.light0_a[2]), b.light0_b[0], b.light0_b[1], b.env[0],
                  b.env[1], b.env[2]);
    }
    return ok;
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Bench b;
  b.t.e_valid_i = 0;
  b.t.stream_idle_i = 1;
  b.t.rst_n = 0;
  b.t.eval();
  zhao::tick(b.t);
  zhao::tick(b.t);
  b.t.rst_n = 1;
  b.t.eval();

  // ---- 1 power-on ----------------------------------------------------------
  {
    ck(b.until_commit(), "1: reset alone publishes a bank");
    ck(b.matches(zref::light_env::bank_of(zref::sky::EnvState{}), "power-on"),
       "1: the power-on bank is bank_of(EnvState{}) (4a's default record)");
    ck(b.writes == 14 && b.commits == 1, "1: 14 writes then one commit");
    ck(b.t.loads_o == 1 && b.t.records_o == 0, "1: loads=1 records=0");
    b.step();
    ck(!b.t.hold_o, "1: hold released after the commit");
  }

  // ---- 2 random records -------------------------------------------------------
  {
    std::mt19937 rng(0x5E7E4Fu);
    int bad = 0;
    for (int n = 0; n < 400; ++n) {
      zref::sky::EnvState e;
      e.sun_yaw = zref::angle16{static_cast<uint16_t>(rng())};
      e.sun_pitch = zref::angle16{static_cast<uint16_t>(rng())};
      if (n < 8) {  // the cardinal angles, where sin/cos hit exactly +-1.0
        e.sun_yaw = zref::angle16{static_cast<uint16_t>(n * 0x2000)};
        e.sun_pitch = zref::angle16{static_cast<uint16_t>((7 - n) * 0x2000)};
      }
      e.sun_colour.bits = static_cast<uint16_t>(rng());
      e.ambient.bits = static_cast<uint16_t>(rng());
      if (n == 8) { e.sun_colour.bits = 0xFFFF; e.ambient.bits = 0x0000; }
      if (n == 9) { e.sun_colour.bits = 0x0000; e.ambient.bits = 0xFFFF; }
      b.words.clear();
      const int w0 = b.writes, c0 = b.commits;
      b.offer(e);
      const bool done = b.until_commit();
      const bool ok = done && b.matches(zref::light_env::bank_of(e), "random") &&
                      b.writes - w0 == 14 && b.commits - c0 == 1;
      if (!ok) ++bad;
      if (!ok && bad < 4) std::printf("  record %d yaw=%04X pitch=%04X\n", n, e.sun_yaw.raw, e.sun_pitch.raw);
    }
    ck(bad == 0, "2: 400 records, every bank word equals zref::light_env::bank_of");
    ck(b.t.records_o == 400 && b.t.loads_o == 401 && b.t.superseded_o == 0,
       "2: records=400 loads=401 superseded=0");
  }

  // ---- 3 hold ------------------------------------------------------------------
  {
    zref::sky::EnvState e;
    e.sun_yaw = zref::angle16{0x1234};
    e.sun_pitch = zref::angle16{0x2345};
    b.words.clear();
    b.idle = false;
    b.offer(e);
    const int w0 = b.writes;
    bool held = true;
    for (int i = 0; i < 50; ++i) {
      b.step();
      held &= b.t.hold_o != 0;
    }
    ck(held, "3: hold_o is raised and stays up while the stream is busy");
    ck(b.writes == w0 && !b.wrote_while_busy, "3: nothing written while the stream is busy");
    b.idle = true;
    bool held_through = true;
    const int c0 = b.commits;
    for (int i = 0; i < 100 && b.commits == c0; ++i) {
      b.step();
      if (b.commits == c0) held_through &= b.t.hold_o != 0;
    }
    ck(b.commits == c0 + 1, "3: the load completes once the stream is idle");
    ck(held_through, "3: hold_o stays up until the commit");
    ck(b.matches(zref::light_env::bank_of(e), "hold"), "3: the held load's bank equals the bridge");
    b.step();
    ck(!b.t.hold_o, "3: hold released");
  }

  // ---- 4 superseded -------------------------------------------------------------
  {
    zref::sky::EnvState e1, e2;
    e1.sun_yaw = zref::angle16{0x1111};
    e1.sun_colour.bits = 0xF800;
    e2.sun_yaw = zref::angle16{0x7777};
    e2.sun_pitch = zref::angle16{0x0800};
    e2.ambient.bits = 0x07E0;
    b.words.clear();
    const uint32_t s0 = b.t.superseded_o;
    const int c0 = b.commits;
    b.offer(e1);
    b.offer(e2);
    ck(b.until_commit(), "4: a load follows");
    ck(b.commits == c0 + 1, "4: ONE load for the two records");
    ck(b.t.superseded_o == s0 + 1, "4: superseded_o FIRES for the replaced record");
    ck(b.matches(zref::light_env::bank_of(e2), "superseded"), "4: the bank carries the SECOND record");
  }

  // ---- 5 mid-load record ---------------------------------------------------------
  {
    zref::sky::EnvState e1, e2;
    e1.sun_pitch = zref::angle16{0x3000};
    e2.sun_pitch = zref::angle16{0xC100};
    e2.sun_colour.bits = 0x001F;
    b.words.clear();
    const int c0 = b.commits;
    b.offer(e1);
    for (int i = 0; i < 6; ++i) b.step();  // e1's load is in its trig phase
    b.offer(e2);
    ck(b.until_commit() && b.commits == c0 + 1, "5: e1's load completes");
    ck(b.matches(zref::light_env::bank_of(e1), "mid-load e1"), "5: e1's bank is e1's, unmixed");
    b.words.clear();
    ck(b.until_commit() && b.commits == c0 + 2, "5: the mid-load record is loaded next");
    ck(b.matches(zref::light_env::bank_of(e2), "mid-load e2"), "5: then e2's bank");
  }

  std::printf("light_env_directed: %d checks, %d failed\n", g_checks, g_fail);
  zhao::exit_hard(g_fail ? 1 : 0);
}
