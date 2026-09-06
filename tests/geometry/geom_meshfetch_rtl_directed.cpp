// geom_meshfetch_rtl_directed.cpp — the RTL against zref::MeshFetch.
//
// ---------------------------------------------------------------------------
// WHAT IS ACTUALLY BEING COMPARED
// ---------------------------------------------------------------------------
// `zhao_geom_meshfetch.sv` owns three things and delegates the rest, so those
// three are what this file differentiates:
//
//   * the REFUSAL, including which of the seven reasons -- a wrong reason is as
//     wrong as a wrong verdict, because the counters are per reason;
//   * the WORLD BOUND it presents to the cull service, centre and radius, EXACT
//     -- this is the arithmetic the block owns and the only place its widths
//     can disagree with the oracle's `int64_t`;
//   * the fields it carries through unchanged;
//   * the frozen descriptor request is 64 bytes/eight words. A recovery brief
//     called it 32 bytes, but bytes 32..63 hold required schema and CRC data.
//
// The testbench PLAYS the cull service rather than instantiating
// `zhao_geom_cull`, which is deliberate: that block has its own differential
// (`tests/differential/geom_cull_directed.cpp`, and the ledger's own rule that
// one law is proved once). Instantiating it here would re-prove the frustum
// test and hide whether THIS block presented the right sphere to it.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "verilated.h"

#include "Vzhao_geom_meshfetch.h"

#include "zhao_sim.hpp"
#include "zref/zref_meshfetch.hpp"

using MF = zref::MeshFetch;
namespace mf = zref::meshfetch;
namespace zc = zref::cull;

namespace {

constexpr int32_t ONE = 65536;
constexpr uint8_t kFormat = 1;
constexpr uint16_t kGeneration = 0x2A2A;

void wr16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}
void wr32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

struct Desc {
  uint8_t b[mf::kDescBytes];
  Desc() {
    std::memset(b, 0, sizeof b);
    b[0] = kFormat;
    b[2] = 32;
    b[3] = 40;
    wr16(b + 4, 0x0777);
    wr32(b + 8, static_cast<uint32_t>(3 * ONE));
    wr32(b + 12, static_cast<uint32_t>(-2 * ONE));
    wr32(b + 16, static_cast<uint32_t>(5 * ONE));
    wr32(b + 20, static_cast<uint32_t>(4 * ONE));
    wr32(b + 24, 0x1000);
    wr32(b + 28, 0x2000);
    wr16(b + 32, kGeneration);
    wr16(b + 34, 9);
    stamp();
  }
  void stamp() { wr32(b + mf::kCrcOff, zhao_abi::zhao_crc32c(0, b, mf::kCrcCovered)); }
};

// `zhao_guard_req_t` is {valid, write, client[2:0], addr[26:0], len[6:0],
// be[63:0]} = 103 bits, so Verilator hands it over as a VlWide<4> and the
// fields have to be picked out by position. `valid` is the MSB, bit 102, which
// lives at offset 6 of word 3; `len` is bits 64..70 at the bottom of word 2.
bool guard_valid(const Vzhao_geom_meshfetch& t) { return ((t.guard_req_o[3] >> 6) & 1u) != 0u; }
int guard_len(const Vzhao_geom_meshfetch& t) { return static_cast<int>(t.guard_req_o[2] & 0x7fu); }

// What the RTL presented to the cull port, and what it emitted.
struct Observed {
  bool saw_cull = false;
  int request_len = -1;
  int32_t cx = 0, cy = 0, cz = 0;
  uint32_t radius = 0;
  bool emitted = false;
  uint32_t vertex_offset = 0, index_offset = 0;
  uint8_t vcount = 0, tcount = 0;
  uint16_t material = 0;
  uint8_t vis = 0;
};

// Drive one job to completion, playing the guard and the cull service.
// `cull_verdict` is what the played cull answers; the caller supplies the
// oracle's, so a block that presented the wrong sphere still gets the right
// answer and the DIFFERENCE shows up in `Observed`, not in the verdict.
Observed run(Vzhao_geom_meshfetch& t, const Desc& d, const MF::InstanceXform& x, uint8_t active,
             const zc::Verdict& cull_verdict, bool crc_ok = true) {
  Observed o;

  t.j_valid_i = 1;
  t.j_instance_id_i = 0x1234;
  t.j_desc_addr_i = 0x40;
  t.j_format_i = kFormat;
  t.j_generation_i = kGeneration;
  t.j_active_mask_i = active;
  t.j_client_i = 0;
  t.crc_ok_i = crc_ok ? 1 : 0;
  for (int i = 0; i < 12; ++i) t.j_xform_i[i] = static_cast<uint32_t>(x.m[i]);
  t.guard_rsp_i = 0;
  t.beat_valid_i = 0;
  t.cull_ready_i = 0;
  t.cull_valid_i = 0;
  t.r_ready_i = 1;
  t.eval();
  zhao::tick(t);
  t.j_valid_i = 0;

  // The beats may only start AFTER the guard grants. Feeding them from cycle
  // zero -- which the first version did -- drops all eight while the block is
  // still in S_REQ, and it then waits forever in S_FILL for beats that already
  // came and went. The symptom was every bound reading (0,0,0) r=0, which looks
  // like broken arithmetic and is actually a testbench that answered out of
  // order.
  // THE GUARD'S VERDICT IS A SEPARATE CYCLE (D22 tread 10, 2026-09-06).
  //
  // This played guard used to drive {ready, ok} = 0b110 together. No guard in
  // the tree does that: `zhao_mem_guard` answers `ready = !fwd_active` -- a
  // LEVEL -- and pulses `ok` the cycle AFTER the accept, which is the cycle
  // that raised `fwd_active`. So the two are never high at once on a passing
  // request, and GEOM.MESHFETCH had been written to match this model rather
  // than the block.
  //
  // Three phases now: ready on the offer cycle, the verdict on the next, and
  // beats from the one after that.
  bool granted = false, accepted = false, ticked = false, done = false;
  int beat = 0, cull_delay = 0;

  for (int c = 0; c < 200 && !done; ++c) {
    if (guard_valid(t) && !accepted) {
      o.request_len = guard_len(t);
      t.guard_rsp_i = 0b100;              // ready, no verdict yet
    } else if (accepted && !granted) {
      t.guard_rsp_i = 0b010;              // ok, one cycle later
    } else {
      t.guard_rsp_i = 0;
    }

    t.beat_valid_i = 0;
    t.beat_last_i = 0;
    if (granted && beat < 8) {
      uint64_t w = 0;
      for (int k = 0; k < 8; ++k) w |= static_cast<uint64_t>(d.b[beat * 8 + k]) << (8 * k);
      t.beat_valid_i = 1;
      t.beat_data_i = w;
      t.beat_last_i = (beat == 7) ? 1 : 0;
    }

    t.cull_ready_i = 1;
    t.cull_valid_i = 0;
    if (ticked) {
      if (cull_delay > 0) {
        --cull_delay;
      } else {
        t.cull_valid_i = 1;
        t.cull_vis_i = cull_verdict.visible_mask;
        t.cull_reject_i = cull_verdict.reject ? 1 : 0;
      }
    }

    t.eval();

    if (accepted && !granted) granted = true;
    if (guard_valid(t) && !accepted) accepted = true;
    if (t.beat_valid_i) ++beat;
    if (t.cull_tick_o && !ticked) {
      ticked = true;
      o.saw_cull = true;
      o.cx = static_cast<int32_t>(t.cull_cx_o);
      o.cy = static_cast<int32_t>(t.cull_cy_o);
      o.cz = static_cast<int32_t>(t.cull_cz_o);
      o.radius = static_cast<uint32_t>(t.cull_radius_o);
      cull_delay = 1;
    }
    if (t.r_valid_o) {
      o.emitted = true;
      o.vertex_offset = t.r_vertex_offset_o;
      o.index_offset = t.r_index_offset_o;
      o.vcount = t.r_vertex_count_o;
      o.tcount = t.r_triangle_count_o;
      o.material = t.r_material_id_o;
      o.vis = t.r_visible_mask_o;
      done = true;
    }

    zhao::tick(t);
  }

  t.beat_valid_i = 0;
  t.cull_valid_i = 0;
  return o;
}

MF::InstanceXform xform(int32_t sx, int32_t sy, int32_t sz, int32_t tx) {
  MF::InstanceXform x{};
  x.m[0] = sx;
  x.m[5] = sy;
  x.m[10] = sz;
  x.m[3] = tx;
  return x;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_meshfetch top;

  top.j_valid_i = 0;
  top.beat_valid_i = 0;
  top.cull_ready_i = 0;
  top.cull_valid_i = 0;
  top.r_ready_i = 1;
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  // ---- 1: THE BOUND THE RTL PRESENTS IS THE ORACLE'S, EXACTLY -------------
  // This is the arithmetic the block owns. The RTL accumulates in 64 bits
  // because the oracle does; if either narrowed, the two would part company on
  // large coordinates and nowhere else.
  {
    int bad = 0, bad_len = 0, compared = 0;
    const MF::InstanceXform xs[4] = {
        xform(ONE, ONE, ONE, 0),
        xform(3 * ONE, ONE, 2 * ONE, 7 * ONE),
        xform(-2 * ONE, 5 * ONE, ONE, -11 * ONE),
        xform(64 * ONE, 64 * ONE, 64 * ONE, 0),
    };
    for (int k = 0; k < 5; ++k) {
      const MF::InstanceXform x = (k < 4) ? xs[k] : xform(65537, 65537, 65537, 0);
      Desc d;
      if (k == 4) {
        // THE TIE CASE, and it exists because its absence let a mutation
        // through. Every transform above uses round numbers, so the product's
        // low 16 bits are zero and truncation agrees with round-half-up on all
        // of them -- replacing the RTL's `(v + 32768) >>> 16` with `v >>> 16`
        // passed 7 of 7. That is the exact blindness the contract warns about
        // for this rescale: "agree everywhere except on TIES".
        //
        // 98304 * 65537 = 6,442,549,248, which is 98305.5 << 16 -- a remainder
        // of exactly 32768. Round-half-up gives 98306; truncation gives 98305.
        for (int lane = 0; lane < 3; ++lane) wr32(d.b + 8 + 4 * lane, static_cast<uint32_t>(98304));
        d.stamp();
      }
      int32_t wc[3];
      uint32_t wr;
      const mf::Descriptor dd = mf::decode(d.b);
      mf::world_bound(x, dd.bound_centre, dd.bound_radius, wc, &wr);

      zc::Verdict v{};
      v.visible_mask = 0b01;
      v.reject = false;
      const Observed o = run(top, d, x, 0b01, v);
      ++compared;
      if (o.request_len != static_cast<int>(mf::kDescBytes)) ++bad_len;
      if (!o.saw_cull || o.cx != wc[0] || o.cy != wc[1] || o.cz != wc[2] || o.radius != wr) {
        if (bad < 3)
          std::printf("    rtl (%d,%d,%d) r=%u   oracle (%d,%d,%d) r=%u\n", o.cx, o.cy, o.cz,
                      o.radius, wc[0], wc[1], wc[2], wr);
        ++bad;
      }
    }
    zhao::check(compared == 5, "every transform reached the cull port", 5, compared);
    zhao::check(bad_len == 0,
                "every descriptor request is the frozen 64 bytes/eight words -- "
                "never the recovery brief's stale 32-byte shorthand",
                0, bad_len);
    zhao::check(bad == 0,
                "the world bound the RTL presents to the cull service is the "
                "oracle's, centre and radius, EXACTLY -- including the 64x case "
                "where the radius saturates rather than wrapping",
                0, bad);
  }

  // ---- 2: the refusal taxonomy, reason by reason -------------------------
  // A wrong reason is as wrong as a wrong verdict: the counters are per reason,
  // so a block that refused everything as "CRC" would look healthy in the
  // aggregate and name the wrong cause on every trace.
  {
    struct Case {
      const char* name;
      int reason_index;  // 0-based into refused_o
      void (*mutate)(Desc&);
    };
    const Case cases[] = {
        {"format", 0,
         [](Desc& d) {
           d.b[0] = 0xEE;
           d.stamp();
         }},
        {"generation", 2,
         [](Desc& d) {
           wr16(d.b + 32, kGeneration + 1);
           d.stamp();
         }},
        {"vertex_count", 3,
         [](Desc& d) {
           d.b[2] = 65;
           d.stamp();
         }},
        {"triangle_count", 4,
         [](Desc& d) {
           d.b[3] = 127;
           d.stamp();
         }},
        {"reserved", 5,
         [](Desc& d) {
           d.b[40] = 0xA5;
           d.stamp();
         }},
        {"zero_bound", 6,
         [](Desc& d) {
           wr32(d.b + 20, 0);
           d.stamp();
         }},
    };

    int bad = 0;
    for (const auto& c : cases) {
      Desc d;
      c.mutate(d);

      const uint32_t before = top.refused_o[c.reason_index];
      zc::Verdict v{};
      v.visible_mask = 0b01;
      const Observed o = run(top, d, xform(ONE, ONE, ONE, 0), 0b01, v);

      const mf::Refusal want = MF::validate(d.b, kFormat, kGeneration);
      const int want_index = static_cast<int>(want) - 1;

      if (o.emitted || o.saw_cull) ++bad;                      // refused: no cull, no emit
      if (want_index != c.reason_index) ++bad;                 // the oracle agrees
      if (top.refused_o[c.reason_index] != before + 1) ++bad;  // the RTL's own counter
    }
    zhao::check(bad == 0,
                "each of the six restampable refusals is counted in its OWN "
                "reason, emits nothing, and never reaches the cull service -- "
                "the oracle agreeing on the reason index is half the check",
                0, bad);

    // The CRC refusal is driven through the port rather than by corrupting the
    // descriptor, because the fold is the caller's block, not this one.
    const uint32_t before = top.refused_o[1];
    Desc d;
    zc::Verdict v{};
    const Observed o = run(top, d, xform(ONE, ONE, ONE, 0), 0b01, v, /*crc_ok=*/false);
    zhao::check(!o.emitted && !o.saw_cull && top.refused_o[1] == before + 1,
                "a failed CRC refuses as kCrc, emits nothing, and does not cull "
                "-- a descriptor that is not trustworthy in any field must not "
                "have its bound used either",
                1, 1);
  }

  // ---- 3: DUO — the mask the cull returns is the mask emitted -------------
  {
    Desc d;
    zc::Verdict v{};
    v.visible_mask = 0b10;
    v.reject = false;
    const Observed o = run(top, d, xform(ONE, ONE, ONE, 0), 0b11, v);
    zhao::check(o.emitted && o.vis == 0b10,
                "a meshlet visible only in camera 1 emits with visible_mask 0b10", 0b10, o.vis);
    zhao::check(o.vertex_offset == 0x1000 && o.index_offset == 0x2000 && o.vcount == 32 &&
                    o.tcount == 40 && o.material == 0x0777,
                "and the carried fields arrive unchanged -- this block moves "
                "them, it does not interpret them",
                1, 1);
  }

  // ---- 4: REJECTED IS NOT REFUSED ----------------------------------------
  {
    const uint32_t r_before = top.culled_all_cameras_o;
    uint32_t ref_before = 0;
    for (int i = 0; i < 7; ++i) ref_before += top.refused_o[i];

    Desc d;
    zc::Verdict v{};
    v.visible_mask = 0;
    v.reject = true;
    const Observed o = run(top, d, xform(ONE, ONE, ONE, 0), 0b11, v);

    uint32_t ref_after = 0;
    for (int i = 0; i < 7; ++i) ref_after += top.refused_o[i];

    zhao::check(!o.emitted && top.culled_all_cameras_o == r_before + 1 && ref_after == ref_before,
                "a meshlet no camera can see emits nothing and is counted as "
                "CULLED, not refused -- a counter that conflated them would "
                "report corruption every time the camera turned around",
                1, 1);
  }

  std::printf("  %u considered, %u fetched, %u culled, %u guard-denied\n",
              top.meshlets_considered_o, top.descriptors_fetched_o, top.culled_all_cameras_o,
              top.guard_denied_o);

  return zhao::report_and_exit("geom_meshfetch_rtl_directed");
}
