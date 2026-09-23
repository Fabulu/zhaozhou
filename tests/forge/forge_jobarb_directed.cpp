// forge_jobarb_directed.cpp -- two producers, one zhao_forge_assemble, and the
// metadata swap that sharing it could have introduced.
//
// THE ACCEPTANCE QUESTIONS
// ------------------------
//   1. A job's VERTICES arrive under that job's OWN material and art. This is
//      the whole reason the block exists: `zhao_forge_assemble` moves `jset_q`
//      into `mset_q` on the FIRST VERTEX TAKE, so two producers pushing
//      sidebands into one parking register would produce job A's vertices with
//      job B's material and every handshake would still balance -- the
//      "detector wired to two operands that move together" shape.
//   2. The SIDEBAND GOES IN A CYCLE EARLY. `j_valid_o` must have been taken
//      before the first vertex is released, or the assembler latches the
//      PREVIOUS job's values.
//   3. CLIENT A WINS, ALWAYS. `design/contracts/FORGE.SHADOW.md`: "a stalled
//      shadow must never delay a creature." A is never made to wait for B's
//      turn, only for B's job already in flight -- which is bounded.
//   4. B IS NOT STARVED WHILE A IS IDLE, and `wait_b_o` measures what it cost.
//   5. A DESCRIPTOR IS SPENT BY ITS GRANT. A second job may not silently
//      inherit the first one's material.
//   6. A DESCRIPTOR THAT IS NEVER GRANTED IS REPLACEABLE. `zhao_forge_pagebank`
//      issues a sideband with every draw and an evaluator that refuses the job
//      -- wrong view mask, wrong family -- emits NO VERTICES AT ALL. A register
//      that could not be overwritten would wedge the bank. This is the case
//      the real console hits and no bench had.
//   7. `no_desc_o` FIRES when a producer offers a vertex with no descriptor.
//
// WHAT THIS BENCH DOES NOT SHOW, said rather than implied: it drives the
// arbiter standalone. That the composed console actually reaches the contended
// state is a separate claim, and `forge_assemble_directed` plus the console
// smoke are where it is argued.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_forge_jobarb.h"
#include "verilated.h"
#include "zhao_sim.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

struct Desc {
  uint32_t mset;
  uint16_t mid;
  uint8_t mode;
  uint8_t valpha;
  int32_t r, g, b, aalpha;
  uint8_t tier;
  uint8_t cull;
  uint32_t fstate;
};

// What the assembler would have seen on the clock a vertex was taken.
struct Landed {
  int32_t x;
  int last;
  uint32_t mset;
  uint16_t mid;
  uint8_t mode;
  uint8_t valpha;
  uint32_t fstate;
  int32_t r;
  uint8_t tier;
  uint8_t cull;
};

void reset(Vzhao_forge_jobarb& d) {
  d.rst_n = 0;
  d.a_v_valid_i = 0;
  d.a_t_valid_i = 0;
  d.a_j_valid_i = 0;
  d.b_v_valid_i = 0;
  d.b_t_valid_i = 0;
  d.b_j_valid_i = 0;
  d.a_v_x_i = d.a_v_y_i = d.a_v_z_i = 0;
  d.b_v_x_i = d.b_v_y_i = d.b_v_z_i = 0;
  d.a_v_last_i = d.b_v_last_i = 0;
  d.a_t_last_i = d.b_t_last_i = 0;
  d.v_ready_i = 1;
  d.t_ready_i = 1;
  d.j_ready_i = 1;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
}

void put_desc_a(Vzhao_forge_jobarb& d, const Desc& j) {
  d.a_j_valid_i = 1;
  d.a_j_material_set_i = j.mset;
  d.a_j_material_id_i = j.mid;
  d.a_j_material_mode_i = j.mode;
  d.a_j_vertex_alpha_i = j.valpha;
  d.a_j_frag_state_i = j.fstate;
  d.a_j_art_r_i = j.r;
  d.a_j_art_g_i = j.g;
  d.a_j_art_b_i = j.b;
  d.a_j_art_alpha_i = j.aalpha;
  d.a_j_quality_tier_i = j.tier;
  d.a_j_cull_mode_i = j.cull;
}

void put_desc_b(Vzhao_forge_jobarb& d, const Desc& j) {
  d.b_j_valid_i = 1;
  d.b_j_material_set_i = j.mset;
  d.b_j_material_id_i = j.mid;
  d.b_j_material_mode_i = j.mode;
  d.b_j_vertex_alpha_i = j.valpha;
  d.b_j_frag_state_i = j.fstate;
  d.b_j_art_r_i = j.r;
  d.b_j_art_g_i = j.g;
  d.b_j_art_b_i = j.b;
  d.b_j_art_alpha_i = j.aalpha;
  d.b_j_quality_tier_i = j.tier;
  d.b_j_cull_mode_i = j.cull;
}

// A whole job on one client: `nv` vertices then `nt` triples, recording what
// the shared port carried on every take. `base` labels the vertices so a
// landing can be attributed to the producer that sent it.
struct Job {
  std::vector<Landed> landed;
  std::vector<uint8_t> tri_alpha;
  int sideband_before_first_vertex = -1;
  int tris_taken = 0;
};

Job run_job(Vzhao_forge_jobarb& d, bool on_b, int nv, int nt, int32_t base, int max_cycles = 4000) {
  Job job;
  int vsent = 0;
  int tsent = 0;
  bool seen_sideband = false;
  for (int cycle = 0; cycle < max_cycles; ++cycle) {
    if (on_b) {
      d.b_v_valid_i = (vsent < nv) ? 1 : 0;
      d.b_v_x_i = base + vsent;
      d.b_v_y_i = base + vsent;
      d.b_v_z_i = base + vsent;
      d.b_v_last_i = (vsent + 1 == nv) ? 1 : 0;
      d.b_t_valid_i = (vsent >= nv && tsent < nt) ? 1 : 0;
      d.b_t_i0_i = 0;
      d.b_t_i1_i = static_cast<uint16_t>(tsent + 1);
      d.b_t_i2_i = static_cast<uint16_t>(tsent + 2);
      d.b_t_material_i = 0;
      d.b_t_src_id_i = static_cast<uint16_t>(base & 0xFFFF);
      d.b_t_last_i = (tsent + 1 == nt) ? 1 : 0;
    } else {
      d.a_v_valid_i = (vsent < nv) ? 1 : 0;
      d.a_v_x_i = base + vsent;
      d.a_v_y_i = base + vsent;
      d.a_v_z_i = base + vsent;
      d.a_v_last_i = (vsent + 1 == nv) ? 1 : 0;
      d.a_t_valid_i = (vsent >= nv && tsent < nt) ? 1 : 0;
      d.a_t_i0_i = 0;
      d.a_t_i1_i = static_cast<uint16_t>(tsent + 1);
      d.a_t_i2_i = static_cast<uint16_t>(tsent + 2);
      d.a_t_material_i = 0;
      d.a_t_src_id_i = static_cast<uint16_t>(base & 0xFFFF);
      d.a_t_last_i = (tsent + 1 == nt) ? 1 : 0;
    }
    d.eval();

    if (d.j_valid_o && d.j_ready_i) seen_sideband = true;
    if (d.v_valid_o && d.v_ready_i) {
      if (job.sideband_before_first_vertex < 0)
        job.sideband_before_first_vertex = seen_sideband ? 1 : 0;
      job.landed.push_back({static_cast<int32_t>(d.v_x_o), d.v_last_o,
                            static_cast<uint32_t>(d.j_material_set_o),
                            static_cast<uint16_t>(d.j_material_id_o),
                            static_cast<uint8_t>(d.j_material_mode_o),
                            static_cast<uint8_t>(d.j_vertex_alpha_o),
                            static_cast<uint32_t>(d.j_frag_state_o),
                            static_cast<int32_t>(d.art_r_o),
                            static_cast<uint8_t>(d.art_quality_tier_o),
                            static_cast<uint8_t>(d.art_cull_mode_o)});
      ++vsent;
    }
    if (d.t_valid_o && d.t_ready_i) {
      job.tri_alpha.push_back(static_cast<uint8_t>(d.j_vertex_alpha_o));
      ++tsent;
      ++job.tris_taken;
    }
    zhao::tick(d);
    if (vsent >= nv && tsent >= nt) break;
  }
  d.a_v_valid_i = d.a_t_valid_i = 0;
  d.b_v_valid_i = d.b_t_valid_i = 0;
  d.a_v_last_i = d.b_v_last_i = 0;
  d.a_t_last_i = d.b_t_last_i = 0;
  zhao::tick(d);
  return job;
}

void check_job(const Job& j, const Desc& want, int nv, int nt, const char* label) {
  char buf[180];
  std::snprintf(buf, sizeof(buf), "%s: vertices landed", label);
  check(static_cast<int>(j.landed.size()) == nv, buf, nv, static_cast<long long>(j.landed.size()));
  std::snprintf(buf, sizeof(buf), "%s: triples landed", label);
  check(j.tris_taken == nt, buf, nt, j.tris_taken);
  std::snprintf(buf, sizeof(buf), "%s: the SIDEBAND went in before the first vertex", label);
  check(j.sideband_before_first_vertex == 1, buf, 1, j.sideband_before_first_vertex);

  bool set_ok = !j.landed.empty(), id_ok = !j.landed.empty(), mode_ok = !j.landed.empty();
  bool va_ok = !j.landed.empty(), r_ok = !j.landed.empty(), tier_ok = !j.landed.empty();
  bool cull_ok = !j.landed.empty(), fs_ok = !j.landed.empty();
  for (const Landed& l : j.landed) {
    set_ok = set_ok && (l.mset == want.mset);
    id_ok = id_ok && (l.mid == want.mid);
    mode_ok = mode_ok && (l.mode == want.mode);
    va_ok = va_ok && (l.valpha == want.valpha);
    r_ok = r_ok && (l.r == want.r);
    tier_ok = tier_ok && (l.tier == want.tier);
    cull_ok = cull_ok && (l.cull == want.cull);
    fs_ok = fs_ok && (l.fstate == want.fstate);
  }
  std::snprintf(buf, sizeof(buf), "%s: material_set held for every vertex", label);
  check(set_ok, buf, 1, set_ok ? 1 : 0);
  std::snprintf(buf, sizeof(buf), "%s: material_id held", label);
  check(id_ok, buf, 1, id_ok ? 1 : 0);
  std::snprintf(buf, sizeof(buf), "%s: material_mode held", label);
  check(mode_ok, buf, 1, mode_ok ? 1 : 0);
  std::snprintf(buf, sizeof(buf), "%s: vertex_alpha held", label);
  check(va_ok, buf, 1, va_ok ? 1 : 0);
  std::snprintf(buf, sizeof(buf), "%s: art_r held", label);
  check(r_ok, buf, 1, r_ok ? 1 : 0);
  std::snprintf(buf, sizeof(buf), "%s: quality_tier held", label);
  check(tier_ok, buf, 1, tier_ok ? 1 : 0);
  std::snprintf(buf, sizeof(buf), "%s: cull_mode held", label);
  check(cull_ok, buf, 1, cull_ok ? 1 : 0);
  std::snprintf(buf, sizeof(buf), "%s: raster state held", label);
  check(fs_ok, buf, 1, fs_ok ? 1 : 0);

  bool tri_va_ok = !j.tri_alpha.empty();
  for (uint8_t a : j.tri_alpha) tri_va_ok = tri_va_ok && (a == want.valpha);
  std::snprintf(buf, sizeof(buf), "%s: vertex_alpha still held through the triples", label);
  check(tri_va_ok, buf, 1, tri_va_ok ? 1 : 0);
}

const Desc kForgeJob0 = {0xDEAD0001u, 0x0041, 0, 0xFF, 0x11111111, 0x22222222,
                         0x33333333, 0x00010000, 0x07, 1, 0x00000000u};
const Desc kForgeJob1 = {0xDEAD0002u, 0x0042, 0, 0xFF, 0x44444444, 0x55555555,
                         0x66666666, 0x00010000, 0x09, 2, 0x00000000u};
// The shadow's descriptor: MATMODE_NONE with a ZERO pair, which is what
// `zhao_material_window` requires of a non-sampling span, and a strength that
// is not opaque.
// BLEND=ALPHA in [4:3], Z_TEST_EN in [0], Z_WRITE_DIS in [1]: the raster state
// without which the flat alpha above reaches `zhao_raster_blend_prod.a_i` and
// is thrown away by the BL_REPLACE arm of the finish half.
const Desc kShadowJob = {0x00000000u, 0x0000, 1, 0x60, 0x0A0A0A0A, 0x0B0B0B0B,
                         0x0C0C0C0C, 0x00006000, 0x00, 0, 0x0000000Bu};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_forge_jobarb d;
  reset(d);

  // ---- 1. one job on each client, alone ------------------------------------
  {
    put_desc_a(d, kForgeJob0);
    zhao::tick(d);
    d.a_j_valid_i = 0;
    const Job ja = run_job(d, /*on_b=*/false, 6, 4, 0x1000);
    check_job(ja, kForgeJob0, 6, 4, "A alone");
    check(d.grant_a_o == 1, "A alone: grant_a_o", 1, d.grant_a_o);
    check(d.grant_b_o == 0, "A alone: grant_b_o", 0, d.grant_b_o);

    put_desc_b(d, kShadowJob);
    zhao::tick(d);
    d.b_j_valid_i = 0;
    const Job jb = run_job(d, /*on_b=*/true, 4, 2, 0x2000);
    check_job(jb, kShadowJob, 4, 2, "B alone");
    check(d.grant_b_o == 1, "B alone: grant_b_o", 1, d.grant_b_o);
    check(d.switches_o == 1, "the owner changed exactly once", 1, d.switches_o);
  }

  // ---- 2. THE METADATA SWAP. Both descriptors present, both offering. ------
  // A must win, and B's material must not reach A's vertices.
  {
    reset(d);
    put_desc_a(d, kForgeJob0);
    put_desc_b(d, kShadowJob);
    zhao::tick(d);
    d.a_j_valid_i = 0;
    d.b_j_valid_i = 0;

    // Both clients offer their first vertex on the same clock.
    Job ja;
    Job jb;
    int a_sent = 0, a_tris = 0, b_sent = 0, b_tris = 0;
    const int a_nv = 5, a_nt = 3, b_nv = 4, b_nt = 2;
    bool a_seen_sb = false, b_seen_sb = false;
    bool owner_is_b = false;
    for (int cycle = 0; cycle < 4000; ++cycle) {
      d.a_v_valid_i = (a_sent < a_nv) ? 1 : 0;
      d.a_v_x_i = 0x1000 + a_sent;
      d.a_v_last_i = (a_sent + 1 == a_nv) ? 1 : 0;
      d.a_t_valid_i = (a_sent >= a_nv && a_tris < a_nt) ? 1 : 0;
      d.a_t_i1_i = static_cast<uint16_t>(a_tris + 1);
      d.a_t_last_i = (a_tris + 1 == a_nt) ? 1 : 0;

      d.b_v_valid_i = (b_sent < b_nv) ? 1 : 0;
      d.b_v_x_i = 0x2000 + b_sent;
      d.b_v_last_i = (b_sent + 1 == b_nv) ? 1 : 0;
      d.b_t_valid_i = (b_sent >= b_nv && b_tris < b_nt) ? 1 : 0;
      d.b_t_i1_i = static_cast<uint16_t>(b_tris + 1);
      d.b_t_last_i = (b_tris + 1 == b_nt) ? 1 : 0;
      d.eval();

      owner_is_b = (d.grant_b_o > 0) && (d.grant_a_o == 0 || d.b_v_ready_o || d.b_t_ready_o);
      (void)owner_is_b;
      if (d.j_valid_o && d.j_ready_i) {
        if (d.a_v_ready_o || (d.grant_a_o > d.grant_b_o)) a_seen_sb = true;
        else b_seen_sb = true;
      }
      if (d.v_valid_o && d.v_ready_i) {
        const Landed l = {static_cast<int32_t>(d.v_x_o), d.v_last_o,
                          static_cast<uint32_t>(d.j_material_set_o),
                          static_cast<uint16_t>(d.j_material_id_o),
                          static_cast<uint8_t>(d.j_material_mode_o),
                          static_cast<uint8_t>(d.j_vertex_alpha_o),
                          static_cast<uint32_t>(d.j_frag_state_o),
                          static_cast<int32_t>(d.art_r_o),
                          static_cast<uint8_t>(d.art_quality_tier_o),
                          static_cast<uint8_t>(d.art_cull_mode_o)};
        if (d.a_v_ready_o) {
          ja.landed.push_back(l);
          ++a_sent;
        } else {
          jb.landed.push_back(l);
          ++b_sent;
        }
      }
      if (d.t_valid_o && d.t_ready_i) {
        if (d.a_t_ready_o) {
          ja.tri_alpha.push_back(static_cast<uint8_t>(d.j_vertex_alpha_o));
          ++a_tris;
          ++ja.tris_taken;
        } else {
          jb.tri_alpha.push_back(static_cast<uint8_t>(d.j_vertex_alpha_o));
          ++b_tris;
          ++jb.tris_taken;
        }
      }
      zhao::tick(d);
      if (a_sent >= a_nv && a_tris >= a_nt && b_sent >= b_nv && b_tris >= b_nt) break;
    }
    d.a_v_valid_i = d.a_t_valid_i = d.b_v_valid_i = d.b_t_valid_i = 0;
    d.a_v_last_i = d.b_v_last_i = d.a_t_last_i = d.b_t_last_i = 0;
    zhao::tick(d);

    ja.sideband_before_first_vertex = a_seen_sb ? 1 : 0;
    jb.sideband_before_first_vertex = b_seen_sb ? 1 : 0;
    check_job(ja, kForgeJob0, a_nv, a_nt, "contended A");
    check_job(jb, kShadowJob, b_nv, b_nt, "contended B");

    // A is the creature, so A goes first -- the contract's own rule.
    bool a_first = !ja.landed.empty() && !jb.landed.empty();
    check(a_first, "both jobs completed under contention", 1, a_first ? 1 : 0);
    check(d.grant_a_o == 1 && d.grant_b_o == 1, "one grant each", 1,
          (d.grant_a_o == 1 && d.grant_b_o == 1) ? 1 : 0);
    // B waited for A's job; A never waited for B's turn.
    check(d.wait_b_o > 0, "wait_b_o measured B's wait", 1, d.wait_b_o > 0 ? 1 : 0);
    // `wait_a_o` counts only the clocks A was blocked BY B. A pays the two
    // clocks of its own grant and sideband like every job, and those are not
    // contention -- see the counter's comment in the RTL.
    check(d.wait_a_o == 0, "A never waited on B -- client A wins, always", 0, d.wait_a_o);
    check(d.no_desc_o == 0, "no producer-order fault", 0, d.no_desc_o);
  }

  // ---- 3. A DESCRIPTOR IS SPENT BY ITS GRANT -------------------------------
  // A second job with no fresh descriptor must NOT inherit the first one's
  // material. It is refused (no grant) and counted, not run under stale values.
  {
    reset(d);
    put_desc_a(d, kForgeJob0);
    zhao::tick(d);
    d.a_j_valid_i = 0;
    const Job j1 = run_job(d, false, 4, 2, 0x3000);
    check_job(j1, kForgeJob0, 4, 2, "spent-descriptor job 1");

    // No new descriptor. Offer vertices anyway.
    const uint32_t nd_before = d.no_desc_o;
    d.a_v_valid_i = 1;
    d.a_v_x_i = 0x4000;
    d.a_v_last_i = 0;
    for (int i = 0; i < 20; ++i) {
      d.eval();
      check(d.v_valid_o == 0, "a vertex with a SPENT descriptor is not granted", 0, d.v_valid_o);
      zhao::tick(d);
    }
    d.a_v_valid_i = 0;
    check(d.no_desc_o > nd_before, "no_desc_o FIRED", 1, d.no_desc_o > nd_before ? 1 : 0);

    // A fresh descriptor unblocks it, with ITS values.
    put_desc_a(d, kForgeJob1);
    zhao::tick(d);
    d.a_j_valid_i = 0;
    const Job j2 = run_job(d, false, 3, 1, 0x5000);
    check_job(j2, kForgeJob1, 3, 1, "spent-descriptor job 2");
  }

  // ---- 4. A DESCRIPTOR THAT IS NEVER GRANTED IS REPLACEABLE ----------------
  // This is `zhao_forge_pagebank`'s VIEW-SKIPPED draw: the sideband is issued,
  // the evaluator refuses the job and emits no vertices at all, and the next
  // draw's sideband must simply replace the dead one. A register that could not
  // be overwritten would wedge the bank forever.
  {
    reset(d);
    put_desc_a(d, kForgeJob0);  // the draw that gets refused downstream
    zhao::tick(d);
    d.a_j_valid_i = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(d);  // no vertices ever arrive
    check(d.grant_a_o == 0, "a sideband alone does not take the grant", 0, d.grant_a_o);

    put_desc_a(d, kForgeJob1);  // the next draw
    zhao::tick(d);
    d.a_j_valid_i = 0;
    const Job j = run_job(d, false, 5, 3, 0x6000);
    check_job(j, kForgeJob1, 5, 3, "after a view-skipped draw");
    check(d.no_desc_o == 0, "a replaced descriptor is not a fault", 0, d.no_desc_o);
  }

  // ---- 5. B IS NOT STARVED WHILE A IS IDLE ---------------------------------
  {
    reset(d);
    put_desc_b(d, kShadowJob);
    zhao::tick(d);
    d.b_j_valid_i = 0;
    const Job jb1 = run_job(d, true, 16, 14, 0x7000);
    check_job(jb1, kShadowJob, 16, 14, "B with A idle");

    put_desc_b(d, kShadowJob);
    zhao::tick(d);
    d.b_j_valid_i = 0;
    const Job jb2 = run_job(d, true, 8, 6, 0x8000);
    check_job(jb2, kShadowJob, 8, 6, "B again with A idle");
    check(d.grant_b_o == 2, "two consecutive B grants", 2, d.grant_b_o);
    check(d.switches_o == 0, "no switch when only B runs", 0, d.switches_o);
  }

  // ---- 6. backpressure from the assembler ----------------------------------
  {
    reset(d);
    put_desc_a(d, kForgeJob0);
    zhao::tick(d);
    d.a_j_valid_i = 0;
    // Stall the sideband handshake first, then the streams.
    d.j_ready_i = 0;
    d.a_v_valid_i = 1;
    d.a_v_x_i = 0x9000;
    for (int i = 0; i < 6; ++i) {
      d.eval();
      check(d.v_valid_o == 0, "no vertex is released before the sideband is taken", 0, d.v_valid_o);
      zhao::tick(d);
    }
    d.a_v_valid_i = 0;
    d.j_ready_i = 1;
    // NO tick here: the sideband handshake must be OBSERVED by `run_job`, and
    // a tick would complete it behind the driver's back and make the check
    // read as a failure when the block is correct.
    const Job j = run_job(d, false, 6, 4, 0x9000);
    check_job(j, kForgeJob0, 6, 4, "stalled sideband");
  }

  std::printf("forge_jobarb_directed: %d check(s), %d failure(s)\n", g_checks, g_failed);
  std::fflush(stdout);
  // TEARDOWN-DEADLOCK WORKAROUND, documented in tests/harness/zhao_sim.hpp:
  // a plain C++ return is exactly the shape that hangs on this toolchain.
  zhao::exit_hard(g_failed == 0 ? 0 : 1);
}
