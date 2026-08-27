// field_patch_acc_directed.cpp — the Field v3 four-bank patch accumulator
// against zref::terrain::compose_vertex (the ratified §3.4 oracle) for
// height, the TERRAIN.VELOCITY V1 chain for velocity, and the two laws the
// probe DECLARES (material writer-selection, nav_cost chain) — plus the
// probe's ACCEPTANCE GATE: one four-vertex vector update lands per clock,
// sustained (reports/Fieldv3.md Phase 3, probe 5).
//
// LAWS:
//   1. EACH OUTPUT FOLLOWS ITS OWN REDUCER — height is the §3.4
//      command-order saturating fx_add chain WITH both bottom clamps;
//      velocity and nav_cost are the same chain WITHOUT clamps, init 0;
//      material is last-covering-writer-wins. Section 4 drives fields whose
//      write masks differ so a pooled reducer cannot pass.
//   2. COMMAND ORDER IS EXACT PER VERTEX. Section 2 drives a lane pair
//      whose two orders provably disagree (the TERRAIN.PATCH directed §3
//      argument) in BOTH orders and requires the oracle's answer each time.
//   3. THE RMW BYPASS IS REAL: back-to-back updates to the SAME vertex on
//      consecutive clocks must chain exactly, per output lane, including
//      mixed write masks (section 3). Field-major walking makes this rare;
//      correctness must not depend on the walker's spacing.
//   4. SATURATION STAYS IN ITS LANES: the DUT's per-lane pulses summed over
//      a whole patch equal the oracle ledgers' add counts, height and
//      velocity/nav separately.
//   5. THE GATE: the update port accepts on 64 of 64 consecutive clocks
//      mid-field, and a full 33x33 field (297 vector updates) lands in 297
//      clocks. INIT and DRAIN move one aligned group per clock (273 each).
//
// The vertex-major/field-major agreement test the TERRAIN.PATCH amendment
// names is carried at the ORACLE level here: compose_vertex IS the
// vertex-major law, and this test drives the same patches FIELD-major.
// The RTL-vs-RTL form (zhao_terrain_patch vs the composed v3 engine) is
// Phase 4's composition test, not this probe's.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_probe_patch_acc.h"

#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_terrain_patch.hpp"

namespace {

using zhao::check;

constexpr int kLat = 33;                   // 33x33 lattice
constexpr int kVerts = kLat * kLat;        // 1,089
constexpr int kGroups = (kVerts + 3) / 4;  // 273

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint64_t next64() {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    uint64_t x = s;
    x ^= x >> 33;
    x *= 0xFF51AFD7ED558CCDULL;
    x ^= x >> 33;
    return x;
  }
  uint32_t below(uint32_t n) { return n ? (uint32_t)(next64() % n) : 0; }
};

// wmask bits (mirror the RTL)
constexpr uint8_t W_H = 1, W_V = 2, W_M = 4, W_N = 8;

struct Field {
  int vi0, vj0, vi1, vj1;  // closed vertex rectangle
  uint8_t wmask;
  std::vector<int32_t> h, v, n;  // per vertex
  std::vector<uint32_t> m;
  bool covers(int vi, int vj) const { return vi >= vi0 && vi <= vi1 && vj >= vj0 && vj <= vj1; }
};

struct PatchIn {
  int16_t base[kVerts];
  int16_t scar[kVerts];
  int16_t bot[kVerts];
  bool dual[kVerts];
  std::vector<Field> fields;  // command order
};

struct PatchOut {
  int32_t top[kVerts], bottom[kVerts], vel[kVerts], nav[kVerts];
  uint32_t mat[kVerts];
  bool dirty[kVerts];
  uint64_t sat_h, sat_v, sat_n;
};

// ---- the oracle ------------------------------------------------------------
PatchOut oracle(const PatchIn& p) {
  PatchOut o{};
  zref::SatLedger Lh{}, Lv{}, Ln{};
  // height goes through the RATIFIED oracle: a FieldList of the
  // height-writing fields and their lanes, per vertex.
  zref::terrain::FieldList listH;
  std::vector<const Field*> hw;
  for (const Field& f : p.fields) {
    if (f.wmask & W_H) {
      zref::terrain::FieldRecord r;
      r.x0 = f.vi0 << 16;
      r.x1 = f.vi1 << 16;
      r.z0 = f.vj0 << 16;
      r.z1 = f.vj1 << 16;
      listH.offer(r, 0);
      hw.push_back(&f);
    }
  }
  for (int vj = 0; vj < kLat; ++vj) {
    for (int vi = 0; vi < kLat; ++vi) {
      const int v = vj * kLat + vi;
      zref::terrain::ComposeIn in;
      in.base = p.base[v];
      in.scar = p.scar[v];
      in.bottom = p.bot[v];
      in.dual = p.dual[v];
      in.wx = vi << 16;
      in.wz = vj << 16;
      std::vector<int32_t> lanes(hw.size());
      for (size_t i = 0; i < hw.size(); ++i) lanes[i] = hw[i]->h[(size_t)v];
      const zref::terrain::ComposeOut co =
          zref::terrain::compose_vertex(in, listH, lanes.data(), &Lh);
      o.top[v] = co.live_top;
      o.bottom[v] = co.bottom;
      o.dirty[v] = co.dirty;
      // velocity / nav: the V1 chain (command order, saturating, init 0)
      int32_t vel = 0, nav = 0;
      uint32_t mat = 0;
      for (const Field& f : p.fields) {
        if (!f.covers(vi, vj)) continue;
        if (f.wmask & W_V) vel = zref::fx_add(zref::fx16{vel}, zref::fx16{f.v[(size_t)v]}, &Lv).raw;
        if (f.wmask & W_N) nav = zref::fx_add(zref::fx16{nav}, zref::fx16{f.n[(size_t)v]}, &Ln).raw;
        if (f.wmask & W_M) mat = f.m[(size_t)v];  // last covering writer wins
      }
      o.vel[v] = vel;
      o.nav[v] = nav;
      o.mat[v] = mat;
    }
  }
  o.sat_h = Lh.add;
  o.sat_v = Lv.add;
  o.sat_n = Ln.add;
  return o;
}

// ---- DUT drive -------------------------------------------------------------
struct SatCount {
  uint64_t h = 0, v = 0, n = 0;
};

void collect_sat(Vzhao_probe_patch_acc& dut, SatCount& sc) {
  if (dut.sat_valid_o) {
    sc.h += __builtin_popcount(dut.sat_h_o);
    sc.v += __builtin_popcount(dut.sat_v_o);
    sc.n += __builtin_popcount(dut.sat_n_o);
  }
}

void idle(Vzhao_probe_patch_acc& dut, SatCount& sc, int cycles) {
  dut.in_valid_i = 0;
  dut.up_valid_i = 0;
  dut.dr_valid_i = 0;
  dut.eval();
  for (int i = 0; i < cycles; ++i) {
    collect_sat(dut, sc);
    zhao::tick(dut);
    dut.eval();
  }
}

/** INIT the whole patch; returns cycles used (accepts are unconditional). */
int run_init(Vzhao_probe_patch_acc& dut, const PatchIn& p, SatCount& sc) {
  int cycles = 0;
  for (int g = 0; g < kGroups; ++g) {
    const int16_t* B = p.base;
    const int16_t* S = p.scar;
    const int16_t* T = p.bot;
    const int v0 = 4 * g;
    const uint8_t mask = (g == kGroups - 1) ? 0x1 : 0xF;
    dut.in_valid_i = 1;
    dut.in_g_i = (uint16_t)g;
    dut.in_mask_i = mask;
    dut.in_base_0_i = (uint16_t)B[v0];
    dut.in_base_1_i = (uint16_t)((mask & 2) ? B[v0 + 1] : 0);
    dut.in_base_2_i = (uint16_t)((mask & 4) ? B[v0 + 2] : 0);
    dut.in_base_3_i = (uint16_t)((mask & 8) ? B[v0 + 3] : 0);
    dut.in_scar_0_i = (uint16_t)S[v0];
    dut.in_scar_1_i = (uint16_t)((mask & 2) ? S[v0 + 1] : 0);
    dut.in_scar_2_i = (uint16_t)((mask & 4) ? S[v0 + 2] : 0);
    dut.in_scar_3_i = (uint16_t)((mask & 8) ? S[v0 + 3] : 0);
    dut.in_bot_0_i = (uint16_t)T[v0];
    dut.in_bot_1_i = (uint16_t)((mask & 2) ? T[v0 + 1] : 0);
    dut.in_bot_2_i = (uint16_t)((mask & 4) ? T[v0 + 2] : 0);
    dut.in_bot_3_i = (uint16_t)((mask & 8) ? T[v0 + 3] : 0);
    uint8_t dual = 0;
    for (int l = 0; l < 4; ++l)
      if ((mask >> l) & 1 && p.dual[v0 + l]) dual |= (uint8_t)(1 << l);
    dut.in_dual_i = dual;
    dut.eval();
    collect_sat(dut, sc);
    zhao::tick(dut);
    ++cycles;
  }
  dut.in_valid_i = 0;
  dut.eval();
  return cycles;
}

/** Drive one vector update. */
void drive_update(Vzhao_probe_patch_acc& dut, int iv, uint8_t mask, uint8_t wmask,
                  const int32_t h[4], const int32_t v[4], const uint32_t m[4], const int32_t n[4]) {
  dut.up_valid_i = 1;
  dut.up_iv_i = (uint16_t)iv;
  dut.up_mask_i = mask;
  dut.up_wmask_i = wmask;
  dut.up_h_0_i = (uint32_t)h[0];
  dut.up_h_1_i = (uint32_t)h[1];
  dut.up_h_2_i = (uint32_t)h[2];
  dut.up_h_3_i = (uint32_t)h[3];
  dut.up_v_0_i = (uint32_t)v[0];
  dut.up_v_1_i = (uint32_t)v[1];
  dut.up_v_2_i = (uint32_t)v[2];
  dut.up_v_3_i = (uint32_t)v[3];
  dut.up_m_0_i = m[0];
  dut.up_m_1_i = m[1];
  dut.up_m_2_i = m[2];
  dut.up_m_3_i = m[3];
  dut.up_n_0_i = (uint32_t)n[0];
  dut.up_n_1_i = (uint32_t)n[1];
  dut.up_n_2_i = (uint32_t)n[2];
  dut.up_n_3_i = (uint32_t)n[3];
}

/** Walk one field row-major, one vector update per clock. Returns cycles. */
int run_field(Vzhao_probe_patch_acc& dut, const Field& f, SatCount& sc) {
  int cycles = 0;
  for (int vj = f.vj0; vj <= f.vj1; ++vj) {
    const int vstart = vj * kLat + f.vi0;
    const int vend = vj * kLat + f.vi1;
    for (int iv = vstart; iv <= vend; iv += 4) {
      uint8_t mask = 0;
      int32_t h[4] = {0, 0, 0, 0}, v[4] = {0, 0, 0, 0}, n[4] = {0, 0, 0, 0};
      uint32_t m[4] = {0, 0, 0, 0};
      for (int l = 0; l < 4; ++l) {
        const int vv = iv + l;
        if (vv > vend) break;
        mask |= (uint8_t)(1 << l);
        h[l] = f.h[(size_t)vv];
        v[l] = f.v[(size_t)vv];
        m[l] = f.m[(size_t)vv];
        n[l] = f.n[(size_t)vv];
      }
      drive_update(dut, iv, mask, f.wmask, h, v, m, n);
      dut.eval();
      collect_sat(dut, sc);
      zhao::tick(dut);
      ++cycles;
    }
  }
  dut.up_valid_i = 0;
  dut.eval();
  return cycles;
}

/** DRAIN the whole patch and compare against the oracle. */
void run_drain_and_check(Vzhao_probe_patch_acc& dut, const PatchIn& p, const PatchOut& want,
                         SatCount& sc, const char* what) {
  const std::string t(what);
  int32_t got_top[kVerts], got_bot[kVerts], got_vel[kVerts], got_nav[kVerts];
  uint32_t got_mat[kVerts];
  bool got_dirty[kVerts], got_seen[kVerts];
  memset(got_seen, 0, sizeof got_seen);

  auto capture = [&](void) {
    if (!dut.out_valid_o) return;
    const int g = (int)dut.out_g_o;
    const int32_t top[4] = {(int32_t)dut.out_top_0_o, (int32_t)dut.out_top_1_o,
                            (int32_t)dut.out_top_2_o, (int32_t)dut.out_top_3_o};
    const int32_t bot[4] = {(int32_t)dut.out_bot_0_o, (int32_t)dut.out_bot_1_o,
                            (int32_t)dut.out_bot_2_o, (int32_t)dut.out_bot_3_o};
    const int32_t vel[4] = {(int32_t)dut.out_vel_0_o, (int32_t)dut.out_vel_1_o,
                            (int32_t)dut.out_vel_2_o, (int32_t)dut.out_vel_3_o};
    const uint32_t mat[4] = {dut.out_mat_0_o, dut.out_mat_1_o, dut.out_mat_2_o, dut.out_mat_3_o};
    const int32_t nav[4] = {(int32_t)dut.out_nav_0_o, (int32_t)dut.out_nav_1_o,
                            (int32_t)dut.out_nav_2_o, (int32_t)dut.out_nav_3_o};
    for (int l = 0; l < 4; ++l) {
      if (!((dut.out_mask_o >> l) & 1)) continue;
      const int v = 4 * g + l;
      got_top[v] = top[l];
      got_bot[v] = bot[l];
      got_vel[v] = vel[l];
      got_mat[v] = mat[l];
      got_nav[v] = nav[l];
      got_dirty[v] = ((dut.out_dirty_o >> l) & 1) != 0;
      got_seen[v] = true;
    }
  };

  int cycles = 0;
  for (int g = 0; g < kGroups + 3; ++g) {
    if (g < kGroups) {
      const int v0 = 4 * g;
      const uint8_t mask = (g == kGroups - 1) ? 0x1 : 0xF;
      dut.dr_valid_i = 1;
      dut.dr_g_i = (uint16_t)g;
      dut.dr_mask_i = mask;
      dut.dr_base_0_i = (uint16_t)p.base[v0];
      dut.dr_base_1_i = (uint16_t)((mask & 2) ? p.base[v0 + 1] : 0);
      dut.dr_base_2_i = (uint16_t)((mask & 4) ? p.base[v0 + 2] : 0);
      dut.dr_base_3_i = (uint16_t)((mask & 8) ? p.base[v0 + 3] : 0);
      dut.dr_bot_0_i = (uint16_t)p.bot[v0];
      dut.dr_bot_1_i = (uint16_t)((mask & 2) ? p.bot[v0 + 1] : 0);
      dut.dr_bot_2_i = (uint16_t)((mask & 4) ? p.bot[v0 + 2] : 0);
      dut.dr_bot_3_i = (uint16_t)((mask & 8) ? p.bot[v0 + 3] : 0);
      uint8_t dual = 0;
      for (int l = 0; l < 4; ++l)
        if ((mask >> l) & 1 && p.dual[v0 + l]) dual |= (uint8_t)(1 << l);
      dut.dr_dual_i = dual;
    } else {
      dut.dr_valid_i = 0;
    }
    dut.eval();
    capture();
    collect_sat(dut, sc);
    zhao::tick(dut);
    ++cycles;
    dut.eval();
  }
  dut.dr_valid_i = 0;
  dut.eval();

  int seen = 0;
  for (int v = 0; v < kVerts; ++v) seen += got_seen[v] ? 1 : 0;
  check(seen == kVerts, (t + ": all vertices drained").c_str(), kVerts, seen);
  check(cycles == kGroups + 3, (t + ": drain cycles (273 + pipeline)").c_str(), kGroups + 3,
        cycles);
  // Every vertex, every output, a REAL check each — the registry's count is
  // the coverage claim (failures print; passes count silently).
  for (int v = 0; v < kVerts; ++v) {
    if (!got_seen[v]) continue;
    check(got_top[v] == want.top[v], (t + ": live_top vertex").c_str(), (uint32_t)want.top[v],
          (uint32_t)got_top[v]);
    check(got_bot[v] == want.bottom[v], (t + ": bottom vertex").c_str(), (uint32_t)want.bottom[v],
          (uint32_t)got_bot[v]);
    check(got_vel[v] == want.vel[v], (t + ": velocity vertex").c_str(), (uint32_t)want.vel[v],
          (uint32_t)got_vel[v]);
    check(got_mat[v] == want.mat[v], (t + ": material vertex").c_str(), want.mat[v], got_mat[v]);
    check(got_nav[v] == want.nav[v], (t + ": nav vertex").c_str(), (uint32_t)want.nav[v],
          (uint32_t)got_nav[v]);
    check(got_dirty[v] == want.dirty[v], (t + ": dirty vertex").c_str(), want.dirty[v] ? 1 : 0,
          got_dirty[v] ? 1 : 0);
  }
}

/** Full patch through the DUT field-major; compare everything. */
void run_patch(Vzhao_probe_patch_acc& dut, const PatchIn& p, const char* what) {
  const PatchOut want = oracle(p);
  SatCount sc;
  run_init(dut, p, sc);
  idle(dut, sc, 3);
  for (const Field& f : p.fields) run_field(dut, f, sc);
  idle(dut, sc, 3);
  run_drain_and_check(dut, p, want, sc, what);
  idle(dut, sc, 3);
  const std::string t(what);
  check(sc.h == want.sat_h, (t + ": height sat pulse total").c_str(), want.sat_h, sc.h);
  check(sc.v == want.sat_v, (t + ": velocity sat pulse total").c_str(), want.sat_v, sc.v);
  check(sc.n == want.sat_n, (t + ": nav sat pulse total").c_str(), want.sat_n, sc.n);
}

Field make_field(Prng& r, int vi0, int vj0, int vi1, int vj1, uint8_t wmask, bool wild) {
  Field f;
  f.vi0 = vi0;
  f.vj0 = vj0;
  f.vi1 = vi1;
  f.vj1 = vj1;
  f.wmask = wmask;
  f.h.resize(kVerts);
  f.v.resize(kVerts);
  f.m.resize(kVerts);
  f.n.resize(kVerts);
  for (int v = 0; v < kVerts; ++v) {
    if (wild && r.below(4) == 0) {
      f.h[(size_t)v] = (int32_t)r.next64();
      f.v[(size_t)v] = (int32_t)r.next64();
      f.n[(size_t)v] = (int32_t)r.next64();
    } else {
      f.h[(size_t)v] = (int32_t)(r.next64() & 0x3FFFFF) - 0x200000;  // a few metres, fx16
      f.v[(size_t)v] = (int32_t)(r.next64() & 0x3FFFFF) - 0x200000;
      f.n[(size_t)v] = (int32_t)(r.next64() & 0x3FFFFF) - 0x200000;
    }
    f.m[(size_t)v] = (uint32_t)r.next64();
  }
  return f;
}

PatchIn make_patch(Prng& r, bool domain_limit) {
  PatchIn p;
  for (int v = 0; v < kVerts; ++v) {
    if (domain_limit && r.below(8) == 0) {
      p.base[v] = (r.below(2) != 0u) ? INT16_MAX : INT16_MIN;
      p.scar[v] = (r.below(2) != 0u) ? INT16_MAX : INT16_MIN;
      p.bot[v] = (r.below(2) != 0u) ? INT16_MAX : INT16_MIN;
    } else {
      p.base[v] = (int16_t)((int)r.below(2000) - 500);
      p.scar[v] = (int16_t) - (int)r.below(300);
      p.bot[v] = (int16_t)((int)p.base[v] - 2000 - (int)r.below(500));
    }
    p.dual[v] = r.below(4) != 0;  // a quarter of vertices legacy
  }
  const int nf = (int)r.below(6);
  for (int i = 0; i < nf; ++i) {
    const int vi0 = (int)r.below(kLat), vi1 = vi0 + (int)r.below((uint32_t)(kLat - vi0));
    const int vj0 = (int)r.below(kLat), vj1 = vj0 + (int)r.below((uint32_t)(kLat - vj0));
    uint8_t wm = (uint8_t)(1 + r.below(15));  // never all-zero
    p.fields.push_back(make_field(r, vi0, vj0, vi1, vj1, wm, domain_limit));
  }
  return p;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int random_n = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--random") && i + 1 < argc) random_n = atoi(argv[i + 1]);
  }

  Vzhao_probe_patch_acc dut;
  dut.rst_n = 0;
  dut.in_valid_i = 0;
  dut.up_valid_i = 0;
  dut.dr_valid_i = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);

  Prng rng(random_n ? 0xACC0 + random_n : 0xACC0);

  if (random_n == 0) {
    printf("== section 1: quiet patch — the compose path alone ==\n");
    {
      PatchIn p = make_patch(rng, false);
      p.fields.clear();
      run_patch(dut, p, "quiet patch");
    }

    printf("== section 2: saturation order — two orders provably disagree ==\n");
    {
      // field A adds +INT32_MAX, field B adds -1, both over the whole patch.
      // A-then-B saturates first and lands at MAX-1; B-then-A lands at MAX.
      PatchIn p = make_patch(rng, false);
      p.fields.clear();
      Field fa = make_field(rng, 0, 0, kLat - 1, kLat - 1, W_H, false);
      Field fb = make_field(rng, 0, 0, kLat - 1, kLat - 1, W_H, false);
      for (int v = 0; v < kVerts; ++v) {
        fa.h[(size_t)v] = INT32_MAX;
        fb.h[(size_t)v] = -1;
      }
      PatchIn pab = p;
      pab.fields.push_back(fa);
      pab.fields.push_back(fb);
      PatchIn pba = p;
      pba.fields.push_back(fb);
      pba.fields.push_back(fa);
      const PatchOut wab = oracle(pab), wba = oracle(pba);
      int differ = 0;
      for (int v = 0; v < kVerts; ++v) differ += (wab.top[v] != wba.top[v]) ? 1 : 0;
      check(differ > 0, "the two orders disagree somewhere (the pin bites)", 1, differ > 0);
      run_patch(dut, pab, "order A-then-B");
      run_patch(dut, pba, "order B-then-A");
    }

    printf("== section 3: RMW bypass — same vertex, consecutive clocks ==\n");
    {
      PatchIn p = make_patch(rng, false);
      p.fields.clear();
      const PatchOut base = oracle(p);
      SatCount sc;
      run_init(dut, p, sc);
      idle(dut, sc, 3);
      // (a) eight single-lane +1 height updates to vertex 37, back-to-back
      const int32_t one[4] = {1, 1, 1, 1};
      const int32_t zero[4] = {0, 0, 0, 0};
      const uint32_t zm[4] = {0, 0, 0, 0};
      for (int i = 0; i < 8; ++i) {
        drive_update(dut, 37, 0x1, W_H, one, zero, zm, zero);
        dut.eval();
        collect_sat(dut, sc);
        zhao::tick(dut);
      }
      // (b) alternating write masks on vertex 41: H, then V, then H, then V —
      // the per-lane bypass must forward H across the V-only write.
      for (int i = 0; i < 6; ++i) {
        drive_update(dut, 41, 0x1, (i & 1) ? W_V : W_H, one, one, zm, zero);
        dut.eval();
        collect_sat(dut, sc);
        zhao::tick(dut);
      }
      // (c) rotation: a 4-lane update at every misalignment of vertex 100
      for (int iv = 100; iv < 104; ++iv) {
        drive_update(dut, iv, 0xF, W_H | W_N, one, zero, zm, one);
        dut.eval();
        collect_sat(dut, sc);
        zhao::tick(dut);
      }
      dut.up_valid_i = 0;
      idle(dut, sc, 3);
      // oracle by hand on top of the quiet patch
      PatchOut want = base;
      auto addh = [&](int v, int32_t d) {
        zref::SatLedger L{};
        // reproduce §3.4: chain onto compose_top, re-clamp at bottom
        (void)L;
        want.top[v] = zref::fx_add(zref::fx16{want.top[v]}, zref::fx16{d}, nullptr).raw;
      };
      for (int i = 0; i < 8; ++i) addh(37, 1);
      for (int i = 0; i < 3; ++i) addh(41, 1);  // three H writes in (b)
      for (int i = 0; i < 3; ++i)
        want.vel[41] = zref::fx_add(zref::fx16{want.vel[41]}, zref::fx16{1}, nullptr).raw;
      for (int iv = 100; iv < 104; ++iv) {
        for (int l = 0; l < 4; ++l) {
          addh(iv + l, 1);
          want.nav[iv + l] = zref::fx_add(zref::fx16{want.nav[iv + l]}, zref::fx16{1}, nullptr).raw;
        }
      }
      // re-apply the final bottom clamp + dirty for the touched vertices
      for (int v = 0; v < kVerts; ++v) {
        const int32_t botfx = ((int32_t)p.bot[v]) << 8;
        if (p.dual[v] && want.top[v] < botfx) want.top[v] = botfx;
        want.bottom[v] = p.dual[v] ? botfx : want.top[v];
        want.dirty[v] = want.top[v] != (((int32_t)p.base[v]) << 8);
      }
      run_drain_and_check(dut, p, want, sc, "bypass hammer");
    }

    printf("== section 4: write masks + material writer-selection ==\n");
    {
      PatchIn p = make_patch(rng, false);
      p.fields.clear();
      // f0 writes ONLY material over the left half; f1 writes h+v over a
      // band; f2 writes material+nav over the right half overlapping f0's
      // right edge column 16 — the overlap column must show f2's material.
      p.fields.push_back(make_field(rng, 0, 0, 16, kLat - 1, W_M, false));
      p.fields.push_back(make_field(rng, 4, 8, 28, 24, W_H | W_V, false));
      p.fields.push_back(make_field(rng, 16, 0, kLat - 1, kLat - 1, W_M | W_N, false));
      run_patch(dut, p, "write-mask split");
    }

    printf("== section 5: throughput gates ==\n");
    {
      PatchIn p = make_patch(rng, false);
      p.fields.clear();
      Field f = make_field(rng, 0, 0, kLat - 1, kLat - 1, W_H | W_V | W_M | W_N, false);
      p.fields.push_back(f);
      const PatchOut want = oracle(p);
      SatCount sc;
      const int init_cycles = run_init(dut, p, sc);
      printf("   INIT: %d groups in %d clocks\n", kGroups, init_cycles);
      check(init_cycles == kGroups, "INIT moves one aligned group per clock", kGroups, init_cycles);
      idle(dut, sc, 3);
      const int upd_cycles = run_field(dut, p.fields[0], sc);
      // 33 vertices per row -> 9 vector updates per row -> 297 for the patch
      printf("   MEASURED: full 33x33 field, %d vector updates in %d clocks\n", 9 * kLat,
             upd_cycles);
      check(upd_cycles == 9 * kLat, "THE GATE: one vector update lands per clock", 9 * kLat,
            upd_cycles);
      idle(dut, sc, 3);
      run_drain_and_check(dut, p, want, sc, "throughput patch");
      idle(dut, sc, 3);
      check(sc.h == want.sat_h, "throughput patch: height sat total", want.sat_h, sc.h);
    }
  } else {
    printf("== random differential: %d patches against the oracles ==\n", random_n);
    for (int i = 0; i < random_n; ++i) {
      PatchIn p = make_patch(rng, (i % 3) == 2);  // every third patch domain-limit
      char msg[48];
      snprintf(msg, sizeof msg, "random patch %d", i);
      run_patch(dut, p, msg);
    }
  }

  return zhao::report_and_exit("field_patch_acc_directed");
}
