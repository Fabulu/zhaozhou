// texture_tmu_plan_directed.cpp — are the planned addresses bit-identical, and
// is the planner ACTUALLY elastic?
//
// ---------------------------------------------------------------------------
// TWO CLAIMS, TWO KINDS OF EVIDENCE
// ---------------------------------------------------------------------------
// 1. The addresses must match `zhao_texture_tmu_pipe` exactly. A wrong texel
//    address gives a picture that is subtly wrong everywhere and obviously
//    wrong nowhere, so the oracle is the shipped block driven on the same
//    stimulus -- not a C++ re-derivation of the same address maths, which would
//    only prove two copies agree.
//
// 2. It must be ELASTIC. The brief's words: "There must be no single a0_v that
//    prevents accepting request N+1 while request N advances." That is a
//    THROUGHPUT property, and a planner that computes perfect addresses one
//    request at a time would pass claim 1 completely and deliver nothing. So
//    the test holds the downstream ready LOW and requires the planner to keep
//    accepting until all five stages are full -- which the current pipe cannot
//    do, and is the entire reason this block exists.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vtb_tmu_plan_pair.h"

#include "zhao_sim.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

struct Req {
  uint32_t u, v, base, mode;
  uint8_t lod;
  uint16_t src;
};

// mode = fmt[2:0] filter[3] wrapU[5:4] wrapV[7:6] log2w[11:8] log2h[15:12]
//        maxlvl[19:16] mip[20]
uint32_t mk_mode(uint32_t fmt, uint32_t filt, uint32_t wu, uint32_t wv, uint32_t lw,
                 uint32_t lh, uint32_t maxl, uint32_t mip) {
  return (fmt & 7u) | ((filt & 1u) << 3) | ((wu & 3u) << 4) | ((wv & 3u) << 6) |
         ((lw & 15u) << 8) | ((lh & 15u) << 12) | ((maxl & 15u) << 16) | ((mip & 1u) << 20);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_tmu_plan_pair top;

  auto reset = [&]() {
    top.a_valid_i = 0;
    top.b_valid_i = 0;
    top.a_acc_ready_i = 1;
    top.b_acc_ready_i = 1;
    top.rst_n = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // ---- stimulus ------------------------------------------------------------
  // NON-CLUT FORMATS ONLY, and the reason is a real scope difference rather
  // than a convenience. The shipped pipe issues EXTRA palette-fetch accesses on
  // its `pf_v` path for CLUT4/CLUT8, interleaved with the planned ones. The
  // planner emits exactly one access per request and no palette fetch, because
  // the brief moves palette translation off the hot path entirely:
  //
  //   > translation to a resident slot happens once in T1 or at material
  //   > binding -- not after every CLUT index returns.
  //
  // Comparing the two streams with palette fetches mixed in compares different
  // things and misaligns every entry after the first CLUT request. Restricting
  // to fmt 2..4 compares like with like; CLUT address planning is covered when
  // the resident-palette interface lands, and this file says so rather than
  // quietly generating only the formats that happen to pass.
  std::vector<Req> rq;
  // Non-CLUT formats are 1, 3 and 4 -- RGB565, ARGB1555, ARGB4444.
  // NOT 2..4: the shipped encoding is CLUT8=0, RGB565=1, CLUT4=2, so the
  // first version of this sweep fed CLUT4 while calling it RGB565.
  for (uint32_t fmt : {1u, 3u, 4u})
    for (uint32_t filt = 0; filt <= 1; ++filt)
      for (uint32_t wu = 0; wu <= 2; ++wu)
        for (uint32_t wv = 0; wv <= 2; ++wv) {
          Req r{};
          r.u = 0x0001'8000u;   // 1.5 in the coordinate's fixed point
          r.v = 0xFFFE'8000u;   // negative, to exercise CLAMP's sign path
          r.base = 0x0010'0000u;
          r.mode = mk_mode(fmt, filt, wu, wv, 8, 6, 3, 1);
          r.lod = 0x20;
          r.src = static_cast<uint16_t>(rq.size() + 1);   // never 0
          rq.push_back(r);
        }
  // level clamping: maxlvl beyond the chain, and mip disabled
  rq.push_back({0x1234u, 0x5678u, 0x2000u, mk_mode(1, 0, 2, 2, 4, 4, 15, 1), 0x70, 0x900});
  rq.push_back({0x1234u, 0x5678u, 0x2000u, mk_mode(1, 0, 2, 2, 8, 8, 0, 0), 0xF0, 0x901});
  // a reserved-bit error case
  rq.push_back({1u, 1u, 0u, mk_mode(1, 0, 2, 2, 4, 4, 0, 0) | (1u << 21), 0, 0x902});

  uint32_t s = 0xFEED01u;
  for (int i = 0; i < 300; ++i) {
    Req r{};
    r.u = rnd(&s);
    r.v = rnd(&s);
    r.base = rnd(&s) & 0x00FF'FF00u;
    const uint32_t nonclut[3] = {1u, 3u, 4u};
    r.mode = mk_mode(nonclut[rnd(&s) % 3u], rnd(&s) & 1u, rnd(&s) % 3u, rnd(&s) % 3u,
                     2u + (rnd(&s) % 9u), 2u + (rnd(&s) % 9u), rnd(&s) % 5u, rnd(&s) & 1u);
    r.lod = static_cast<uint8_t>(rnd(&s));
    r.src = static_cast<uint16_t>(0x1000 + i);   // never 0
    rq.push_back(r);
  }

  // ---- reference: the shipped pipe ---------------------------------------
  struct Acc { uint32_t a0, a1, a2, a3, en, filt, err, fu, fv, fmt, src; };
  std::vector<Acc> ref;
  // KEYED BY src_id, which separates the ARITHMETIC question from the
  // STREAMING one. Comparing by index conflates them: one extra access in
  // either stream shifts every later entry and reports a total mismatch that
  // says nothing about whether an address is right.
  std::map<uint32_t, Acc> refmap, gotmap;
  {
    reset();
    size_t fed = 0;
    for (int c = 0; c < 300000 && ref.size() < rq.size(); ++c) {
      const bool feeding = fed < rq.size();
      top.a_valid_i = feeding;
      if (feeding) {
        top.a_u_i = rq[fed].u;   top.a_v_i = rq[fed].v;
        top.a_base_i = rq[fed].base; top.a_mode_i = rq[fed].mode;
        top.a_lod_i = rq[fed].lod;   top.a_src_i = rq[fed].src;
      }
      top.eval();
      if (top.a_acc_valid_o) {
        Acc a{};
        a.a0 = top.a_acc_addr_o[0]; a.a1 = top.a_acc_addr_o[1];
        a.a2 = top.a_acc_addr_o[2]; a.a3 = top.a_acc_addr_o[3];
        a.en = top.a_acc_en_o; a.src = top.a_acc_src_id_o;
        ref.push_back(a);
        if (a.src != 0 && refmap.find(a.src) == refmap.end()) refmap[a.src] = a;
      }
      const bool took = feeding && top.a_req_ready_o;
      zhao::tick(top);
      if (took) ++fed;
    }
    top.a_valid_i = 0;
    zhao::check(ref.size() == rq.size(), "the shipped pipe planned every request",
                rq.size(), ref.size());
  }

  // ---- candidate: the elastic planner ------------------------------------
  std::vector<Acc> got;
  {
    reset();
    size_t fed = 0;
    for (int c = 0; c < 300000 && got.size() < rq.size(); ++c) {
      const bool feeding = fed < rq.size();
      top.b_valid_i = feeding;
      if (feeding) {
        top.b_u_i = rq[fed].u;   top.b_v_i = rq[fed].v;
        top.b_base_i = rq[fed].base; top.b_mode_i = rq[fed].mode;
        top.b_lod_i = rq[fed].lod;   top.b_src_i = rq[fed].src;
      }
      top.eval();
      if (top.b_acc_valid_o && top.b_acc_ready_i) {
        Acc a{};
        a.a0 = top.b_acc_addr_o[0]; a.a1 = top.b_acc_addr_o[1];
        a.a2 = top.b_acc_addr_o[2]; a.a3 = top.b_acc_addr_o[3];
        a.en = top.b_acc_en_o; a.src = top.b_acc_src_id_o;
        got.push_back(a);
        if (a.src != 0 && gotmap.find(a.src) == gotmap.end()) gotmap[a.src] = a;
      }
      const bool took = feeding && top.b_req_ready_o;
      zhao::tick(top);
      if (took) ++fed;
    }
    top.b_valid_i = 0;
    zhao::check(got.size() == rq.size(), "the elastic planner planned every request",
                rq.size(), got.size());
  }

  // Isolate the level path: 0x901 has mip DISABLED, so level is 0 and lvl_off
  // is 0 on both sides. If that one agrees while mip-enabled ones do not, the
  // divergence is in level selection / lvl_off and nowhere else.
  for (uint32_t k : {1u, 2u, 3u, 10u, 0x900u, 0x901u}) {
    auto r = refmap.find(k), g = gotmap.find(k);
    if (r != refmap.end() && g != gotmap.end())
      std::printf("  src %03x  ref a0=%08x  got a0=%08x  %s\n", k, r->second.a0,
                  g->second.a0, (r->second.a0 == g->second.a0) ? "SAME" : "DIFF");
  }

  for (size_t i = 0; i < 4 && i < ref.size() && i < got.size(); ++i)
    std::printf("  [%zu] ref a0=%08x en=%x src=%04x | got a0=%08x en=%x src=%04x\n",
                i, ref[i].a0, ref[i].en, ref[i].src, got[i].a0, got[i].en, got[i].src);

  int bad_addr = 0, bad_en = 0, bad_src = 0, unmatched = 0;
  for (const auto& kv : gotmap) {
    auto it = refmap.find(kv.first);
    if (it == refmap.end()) { ++unmatched; continue; }
    const Acc& r = it->second;
    const Acc& g = kv.second;
    if (r.a0 != g.a0 || r.a1 != g.a1 || r.a2 != g.a2 || r.a3 != g.a3) ++bad_addr;
    if (r.en != g.en) ++bad_en;
  }
  std::printf("  matched by src_id: %zu of %zu planner accesses (%d unmatched)\n",
              gotmap.size() - static_cast<size_t>(unmatched), gotmap.size(), unmatched);
  // RESOLVED. Both encodings this file's planner had GUESSED were wrong, and
  // both produced addresses that looked entirely plausible:
  //
  //   format  assumed CLUT4=0 CLUT8=1 RGB565=2; shipped is CLUT8=0 RGB565=1
  //           CLUT4=2, so every 16bpp request took the CLUT4 shift in one
  //           block and the 16bpp shift in the other -- while the underlying
  //           totals were IDENTICAL.
  //   wrap    assumed CLAMP=0 MIRROR=1 REPEAT=2; shipped is REPEAT=0 CLAMP=1
  //           MIRROR=2, so a negative V clamped to row 0 here and wrapped to
  //           row 8 there.
  //
  // Neither was found by reading the transcription, which looked right. Both
  // were found by taking ONE request and deriving its address by hand against
  // the pipe's own constants. Guessing an encoding is the same error as
  // guessing an arithmetic law.
  zhao::check(bad_addr == 0,
              "all four texel addresses are BIT-IDENTICAL to the shipped pipe",
              0, bad_addr);
  zhao::check(bad_en == 0, "and the lane enables match", 0, bad_en);
  zhao::check(unmatched == 0, "and every planner access matches a reference one",
              0, unmatched);

  // ---- THE ELASTICITY CLAIM ----------------------------------------------
  // Downstream ready held LOW. The planner must keep accepting until all five
  // stages hold a request. The shipped pipe stops at one, which is the defect.
  {
    reset();
    top.b_acc_ready_i = 0;
    int accepted = 0;
    for (int c = 0; c < 40; ++c) {
      top.b_valid_i = 1;
      top.b_u_i = 0x8000u; top.b_v_i = 0x8000u;
      top.b_base_i = 0; top.b_mode_i = mk_mode(2, 0, 2, 2, 4, 4, 0, 0);
      top.b_lod_i = 0; top.b_src_i = static_cast<uint16_t>(c);
      top.eval();
      if (top.b_req_ready_o) ++accepted;
      zhao::tick(top);
    }
    zhao::check(accepted >= 5,
                "with the sink STALLED the planner still accepts 5 requests "
                "(no single a0_v)",
                5, accepted);
    top.eval();
    zhao::check(top.b_occupancy_o == 5, "and all five stages are occupied", 5,
                top.b_occupancy_o);

    // The same stimulus against the shipped pipe, to show the difference is
    // real rather than a property of the stimulus.
    reset();
    top.a_acc_ready_i = 0;
    int a_accepted = 0;
    for (int c = 0; c < 40; ++c) {
      top.a_valid_i = 1;
      top.a_u_i = 0x8000u; top.a_v_i = 0x8000u;
      top.a_base_i = 0; top.a_mode_i = mk_mode(2, 0, 2, 2, 4, 4, 0, 0);
      top.a_lod_i = 0; top.a_src_i = static_cast<uint16_t>(c);
      top.eval();
      if (top.a_req_ready_o) ++a_accepted;
      zhao::tick(top);
    }
    std::printf("  with the sink stalled: shipped pipe accepted %d, elastic planner %d\n",
                a_accepted, accepted);
    zhao::check(accepted > a_accepted,
                "the elastic planner accepts strictly more than the shipped pipe",
                1, accepted > a_accepted ? 1 : 0);
  }

  return zhao::report_and_exit("texture_tmu_plan_directed");
}
