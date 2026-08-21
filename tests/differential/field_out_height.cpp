// field_out_height.cpp — the FIELD.OUT.HEIGHT op, RTL against the oracle.
//
// WHY THIS FILE EXISTS. `design/ops.yml` declares FIELD.OUT.HEIGHT and names
// this path as its differential coverage. The file did not exist. Ledger rule
// V10 surfaced it the moment TERRAIN.PATCH was advanced past SPECIFIED.
//
// The op cites `zref::fieldir::sink_out_height`, which does not exist — one of
// the twenty-five phantoms in reports/PHANTOM_REFERENCES.md. But this op is the
// FIRST of them whose law was already fully implemented under another name: the
// height out-lane is `field_h[i]` in `zref::terrain::compose_vertex`, and its
// routing rule is that function's §3.4 chain. The op has no dedicated opcode
// either — ops.yml says so itself, "profile output map, no dedicated opcode".
// So the op IS the routing, and the routing is what is tested here.
//
// SCOPE, deliberately narrow. `terrain_patch_directed.cpp` already checks the
// composition as a whole against `compose_lattice`. This file tests only what
// the OP owns: how a height lane reaches the compose chain, in what order, and
// what the chain does with it. Four laws, each of which a reasonable
// implementation gets wrong in a different way:
//
//   1. THE CLAMP HAPPENS ONCE, AFTER THE WHOLE CHAIN. Not per lane. A lane that
//      dips the surface below the modelled underside, followed by one that
//      lifts it back, must end where the sum ends -- clamping per lane pins the
//      surface to the underside and the second lane lifts from the wrong place.
//      This is §3.4's "a transient wave can never punch below the underside, so
//      it can never fake a breach", and per-lane clamping breaks it in the
//      direction that DOES fake one.
//   2. THE CHAIN IS COMMAND ORDER AND SATURATING. fx_add saturates, so addition
//      is not associative near the rails: the same two lanes in the other order
//      give a different answer.
//   3. height16 -> fx16 IS AN EXACT `raw << 8`. No rounding exists in the
//      up-conversion (qformats §2/§9), so no rounding may appear here.
//   4. A LANE WHOSE FOOTPRINT MISSES THE VERTEX DOES NOT REACH THE CHAIN.
//      Value-identical to adding zero -- which is exactly why it is checked on
//      `fld_covers_o` rather than on the result.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_patch.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_patch.hpp"

namespace {

using zhao::check;
namespace zt = zref::terrain;

constexpr int32_t kOne = 1 << 16;

void reset_dut(Vzhao_terrain_patch& top) {
  top.rst_n = 0;
  top.list_clear_i = 0;
  top.patch_id_i = 0;
  top.fld_add_valid_i = 0;
  top.vtx_valid_i = 0;
  top.fld_valid_i = 0;
  top.st_ready_i = 0;
  top.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);
}

/** A footprint rectangle covering everything, or a named box. */
zt::FieldRecord rect(int32_t x0, int32_t z0, int32_t x1, int32_t z1, uint32_t hash, uint16_t cmd) {
  zt::FieldRecord f;
  f.x0 = x0;
  f.z0 = z0;
  f.x1 = x1;
  f.z1 = z1;
  f.program_hash = hash;
  f.cmd_index = cmd;
  return f;
}

zt::FieldRecord everywhere(uint32_t hash, uint16_t cmd) {
  return rect(-1000 * kOne, -1000 * kOne, 1000 * kOne, 1000 * kOne, hash, cmd);
}

void load_list(Vzhao_terrain_patch& dut, const zt::FieldList& list, uint16_t patch_id) {
  bool first = true;
  for (int i = 0; i < list.size(); ++i) {
    dut.list_clear_i = first ? 1 : 0;
    dut.patch_id_i = patch_id;
    dut.fld_add_valid_i = 1;
    dut.fld_add_x0_i = static_cast<uint32_t>(list[i].x0);
    dut.fld_add_z0_i = static_cast<uint32_t>(list[i].z0);
    dut.fld_add_x1_i = static_cast<uint32_t>(list[i].x1);
    dut.fld_add_z1_i = static_cast<uint32_t>(list[i].z1);
    dut.fld_add_hash_i = list[i].program_hash;
    dut.fld_add_cmd_i = list[i].cmd_index;
    zhao::tick(dut);
    dut.fld_add_valid_i = 0;
    dut.list_clear_i = 0;
    first = false;
  }
  if (first) {
    dut.list_clear_i = 1;
    zhao::tick(dut);
    dut.list_clear_i = 0;
  }
}

struct RtlOut {
  int32_t top = 0, bottom = 0, compose_top = 0;
  bool dirty = false;
  int covers_seen = 0;  // how many lanes the block reported as covering
  bool seen = false;
};

RtlOut compose_one(Vzhao_terrain_patch& dut, const zt::ComposeIn& in, int n_fields,
                   const int32_t* field_h) {
  dut.st_ready_i = 1;
  dut.base_i = static_cast<uint16_t>(in.base);
  dut.scar_i = static_cast<uint16_t>(in.scar);
  dut.bottom_i = static_cast<uint16_t>(in.bottom);
  dut.dual_i = in.dual ? 1 : 0;
  dut.wx_i = static_cast<uint32_t>(in.wx);
  dut.wz_i = static_cast<uint32_t>(in.wz);
  dut.vi_i = 4;
  dut.vj_i = 4;
  dut.src_id_i = 7;
  dut.vtx_valid_i = 1;
  dut.fld_valid_i = 0;

  RtlOut out;
  int lane = 0;
  for (int cycle = 0; cycle < 128 && !out.seen; ++cycle) {
    if (lane < n_fields) {
      dut.fld_valid_i = 1;
      dut.fld_height_i = static_cast<uint32_t>(field_h[lane]);
    } else {
      dut.fld_valid_i = 0;
    }
    dut.eval();
    const bool took_vtx = dut.vtx_valid_i && dut.vtx_ready_o;
    const bool took_fld = dut.fld_valid_i && dut.fld_ready_o;
    if (took_fld && dut.fld_covers_o) ++out.covers_seen;
    zhao::tick(dut);
    if (took_vtx) dut.vtx_valid_i = 0;
    if (took_fld) ++lane;
    if (dut.st_valid_o) {
      out.top = static_cast<int32_t>(dut.top_o);
      out.bottom = static_cast<int32_t>(dut.bottom_o);
      out.compose_top = static_cast<int32_t>(dut.compose_top_o);
      out.dirty = dut.st_dirty_o != 0;
      out.seen = true;
    }
  }
  dut.fld_valid_i = 0;
  zhao::tick(dut);
  return out;
}

/** Run one vertex through both and compare the height result. */
RtlOut diff(Vzhao_terrain_patch& dut, const zt::ComposeIn& in, const zt::FieldList& list,
            const int32_t* field_h, const char* what) {
  const zt::ComposeOut want = zt::compose_vertex(in, list, field_h);
  const RtlOut got = compose_one(dut, in, list.size(), field_h);
  check(got.seen, "a vertex produces a record", 1, got.seen ? 1 : 0);
  check(got.top == want.live_top, what, static_cast<uint32_t>(want.live_top),
        static_cast<uint32_t>(got.top));
  check(got.compose_top == want.compose_top, what, static_cast<uint32_t>(want.compose_top),
        static_cast<uint32_t>(got.compose_top));
  check(got.bottom == want.bottom, what, static_cast<uint32_t>(want.bottom),
        static_cast<uint32_t>(got.bottom));
  return got;
}

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

}  // namespace

int main(int argc, char** argv) {
  Vzhao_terrain_patch dut;
  reset_dut(dut);

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0x0E14u);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      zt::FieldList list;
      list.reset();
      const int n = static_cast<int>(rng.below(8));
      for (int k = 0; k < n; ++k) {
        // Half the footprints cover the origin, half do not, so the covers gate
        // is exercised rather than assumed.
        if (rng.below(2) == 0) {
          list.offer(everywhere(0x1000u + k, static_cast<uint16_t>(k)), 1);
        } else {
          list.offer(rect(100 * kOne, 100 * kOne, 200 * kOne, 200 * kOne, 0x2000u + k,
                          static_cast<uint16_t>(k)),
                     1);
        }
      }
      load_list(dut, list, 1);

      int32_t fh[zt::kMaxPatchFields] = {};
      for (int k = 0; k < list.size(); ++k) {
        fh[k] = static_cast<int32_t>(rng.next()) >> static_cast<int>(rng.below(12));
      }
      zt::ComposeIn in;
      in.base = static_cast<int16_t>(rng.next());
      in.scar = static_cast<int16_t>(rng.next());
      in.bottom = static_cast<int16_t>(rng.next());
      in.dual = rng.below(4) != 0;
      in.wx = 0;
      in.wz = 0;
      char tag[80];
      std::snprintf(tag, sizeof tag, "random[%u] lanes=%d", it, list.size());
      diff(dut, in, list, fh, tag);
    }
    dut.final();
    return zhao::report_and_exit("field_out_height_random");
  }

  // ---- 1. one lane, the plain route ---------------------------------------
  {
    zt::FieldList list;
    list.reset();
    list.offer(everywhere(1, 0), 1);
    load_list(dut, list, 1);
    int32_t fh[zt::kMaxPatchFields] = {};
    fh[0] = 3 * kOne;
    zt::ComposeIn in;
    in.base = 100;
    in.scar = 0;
    in.bottom = -1000;
    in.dual = true;
    in.wx = 0;
    in.wz = 0;
    diff(dut, in, list, fh, "one height lane reaches the chain");
  }

  // ---- 2. THE CLAMP IS ONCE, AFTER THE CHAIN ------------------------------
  // Two lanes: the first drives the surface well below the underside, the second
  // lifts it back above. §3.4 clamps only after both, so the answer is the sum.
  // An implementation that clamped per lane would pin at the underside after
  // lane 1 and then lift from there -- a HIGHER surface than the law gives, and
  // one that hides a dip the player should see.
  {
    zt::FieldList list;
    list.reset();
    list.offer(everywhere(10, 0), 1);
    list.offer(everywhere(11, 1), 1);
    load_list(dut, list, 1);

    zt::ComposeIn in;
    in.base = 1000;
    in.scar = 0;
    in.bottom = 0;
    in.dual = true;
    in.wx = 0;
    in.wz = 0;
    int32_t fh[zt::kMaxPatchFields] = {};
    fh[0] = -50 * kOne;  // far below the underside
    fh[1] = 60 * kOne;   // back above it

    const zt::ComposeOut want = zt::compose_vertex(in, list, fh);
    // The fixture only means something if the two orders of clamping differ.
    const int32_t base_fx = static_cast<int32_t>(in.base) << 8;
    const int32_t bot_fx = static_cast<int32_t>(in.bottom) << 8;
    const int32_t per_lane = (base_fx + fh[0] < bot_fx ? bot_fx : base_fx + fh[0]) + fh[1];
    check(per_lane != want.live_top, "the fixture distinguishes clamp-once from clamp-per-lane", 1,
          per_lane != want.live_top ? 1 : 0);

    diff(dut, in, list, fh, "the underside clamp happens once, after the whole chain");
  }

  // ---- 3. COMMAND ORDER, and it matters because fx_add SATURATES ----------
  // Near the s32 rail addition stops being associative. Three lanes at the rail
  // in two orders give genuinely different surfaces, so the chain's order is a
  // law and not a convenience.
  //
  // The first attempt at this section used +BIG then -BIG/2, which never
  // reaches the rail at all -- both orders gave the same answer and the section
  // proved nothing. The fixture-validity check below is what caught that, and
  // it stays for the same reason.
  {
    const int32_t kMax = 0x7FFF'FFFF;
    zt::ComposeIn in;
    in.base = 0;
    in.scar = 0;
    in.bottom = -30000;
    in.dual = false;
    in.wx = 0;
    in.wz = 0;

    zt::FieldList a;
    a.reset();
    a.offer(everywhere(20, 0), 1);
    a.offer(everywhere(21, 1), 1);
    a.offer(everywhere(22, 2), 1);

    load_list(dut, a, 1);
    int32_t fh1[zt::kMaxPatchFields] = {};
    fh1[0] = kMax;
    fh1[1] = kMax;
    fh1[2] = -kMax;  // saturates, then comes back
    const zt::ComposeOut w1 = zt::compose_vertex(in, a, fh1);
    diff(dut, in, a, fh1, "saturating chain: +MAX +MAX -MAX");

    load_list(dut, a, 1);
    int32_t fh2[zt::kMaxPatchFields] = {};
    fh2[0] = -kMax;
    fh2[1] = kMax;
    fh2[2] = kMax;  // never saturates the same way
    const zt::ComposeOut w2 = zt::compose_vertex(in, a, fh2);
    diff(dut, in, a, fh2, "saturating chain: -MAX +MAX +MAX");

    check(w1.live_top != w2.live_top,
          "and the two orders genuinely differ -- fx_add saturates, so it is not associative", 1,
          w1.live_top != w2.live_top ? 1 : 0);
  }

  // ---- 4. height16 -> fx16 is an EXACT shift, with no rounding ------------
  // Every base value whose low bits would expose a rounding step. `raw << 8` is
  // the whole conversion (qformats §2/§9); anything that rounded would show up
  // here as an off-by-one on odd inputs.
  {
    zt::FieldList empty;
    empty.reset();
    load_list(dut, empty, 1);
    const int32_t none[zt::kMaxPatchFields] = {};
    const int16_t vals[] = {0, 1, -1, 2, -2, 127, -128, 255, -256, 32767, -32768, 12345, -12345};
    for (int16_t v : vals) {
      zt::ComposeIn in;
      in.base = v;
      in.scar = 0;
      in.bottom = -32768;
      in.dual = false;
      in.wx = 0;
      in.wz = 0;
      char tag[96];
      std::snprintf(tag, sizeof tag, "height16 -> fx16 is raw<<8, base=%d", v);
      const RtlOut got = diff(dut, in, empty, none, tag);
      check(got.compose_top == (static_cast<int32_t>(v) << 8), tag,
            static_cast<uint32_t>(static_cast<int32_t>(v) << 8),
            static_cast<uint32_t>(got.compose_top));
    }
  }

  // ---- 5. A LANE THAT MISSES THE VERTEX DOES NOT REACH THE CHAIN ----------
  // Skipping a lane and adding zero give the SAME value -- the reference says so
  // explicitly -- so the law is checked where it is observable: the block's own
  // `fld_covers_o`. Three lanes, one covering, and exactly one must report
  // coverage.
  {
    zt::FieldList list;
    list.reset();
    list.offer(rect(-kOne, -kOne, kOne, kOne, 30, 0), 1);                    // covers the origin
    list.offer(rect(50 * kOne, 50 * kOne, 60 * kOne, 60 * kOne, 31, 1), 1);  // misses
    list.offer(rect(-90 * kOne, -90 * kOne, -80 * kOne, -80 * kOne, 32, 2), 1);  // misses
    load_list(dut, list, 1);

    int32_t fh[zt::kMaxPatchFields] = {};
    fh[0] = 5 * kOne;
    fh[1] = 1000 * kOne;
    fh[2] = -1000 * kOne;
    zt::ComposeIn in;
    in.base = 0;
    in.scar = 0;
    in.bottom = -30000;
    in.dual = true;
    in.wx = 0;
    in.wz = 0;

    const RtlOut got = diff(dut, in, list, fh, "only the covering lane reaches the chain");
    check(got.covers_seen == 1, "exactly one of the three lanes reported coverage", 1,
          static_cast<uint64_t>(got.covers_seen));

    // The closed-interval edges (§9.1): a vertex exactly on a footprint border
    // IS covered. An open interval would drop a whole seam of vertices.
    zt::FieldList edge;
    edge.reset();
    edge.offer(rect(0, 0, 10 * kOne, 10 * kOne, 33, 0), 1);
    load_list(dut, edge, 1);
    int32_t efh[zt::kMaxPatchFields] = {};
    efh[0] = 2 * kOne;
    zt::ComposeIn on_corner = in;
    on_corner.wx = 0;
    on_corner.wz = 0;
    const RtlOut c0 = diff(dut, on_corner, edge, efh, "a vertex on the low corner is covered");
    check(c0.covers_seen == 1, "the low corner counts as covered (closed interval)", 1,
          static_cast<uint64_t>(c0.covers_seen));

    load_list(dut, edge, 1);
    zt::ComposeIn far_corner = in;
    far_corner.wx = 10 * kOne;
    far_corner.wz = 10 * kOne;
    const RtlOut c1 = diff(dut, far_corner, edge, efh, "a vertex on the high corner is covered");
    check(c1.covers_seen == 1, "the high corner too", 1, static_cast<uint64_t>(c1.covers_seen));

    load_list(dut, edge, 1);
    zt::ComposeIn just_out = in;
    just_out.wx = 10 * kOne + 1;
    just_out.wz = 10 * kOne;
    const RtlOut c2 = diff(dut, just_out, edge, efh, "one raw unit past the corner is not");
    check(c2.covers_seen == 0, "and one raw unit outside is not covered", 0,
          static_cast<uint64_t>(c2.covers_seen));
  }

  dut.final();
  return zhao::report_and_exit("field_out_height");
}
