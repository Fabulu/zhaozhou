// forge_prim_directed.cpp — six families, and the order that IS the contract.
//
// ---------------------------------------------------------------------------
// WHY THE ORDER IS CHECKED AND NOT JUST THE SET
// ---------------------------------------------------------------------------
//   > A vertex stream to GEOM.SETUP, in a DECLARED DETERMINISTIC ORDER —
//   > because two orderings of the same primitive produce the same picture but
//   > different capture CRCs, and the capture is the contract.
//
// A generator that emitted the same triangles in a different sequence would
// look identical on screen and break every capture in the repository. So the
// stream is compared position by position against the declared walk, and the
// same job run twice must produce the identical sequence.
//
// ---------------------------------------------------------------------------
// AND THE FOUR DELETED FAMILIES
// ---------------------------------------------------------------------------
// The ruling removed shard burst, chain, spline wall and low cone by
// recognising them as uses of something else — shard burst most sharply, as "a
// particle population", because building it here would put a second particle
// system in the geometry path.
//
// They are NOT reserved for later. A job naming a seventh family is refused,
// and that is tested, because the way a deleted feature comes back is a
// generator that quietly accepts its encoding.
//
// ---------------------------------------------------------------------------
// THE ONE THAT SEPARATES A TUBE FROM A RIBBON
// ---------------------------------------------------------------------------
// A tube's ring CLOSES: the last side's second edge is side 0 again. A ribbon's
// does not. Get that backwards and a ribbon welds its two edges together, which
// is invisible in a triangle count and obvious in a picture.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_forge_prim.h"

#include "zhao_sim.hpp"
#include "zref/zref_forge.hpp"

namespace {

constexpr int RIBBON = 0, FAN = 1, TUBE = 2, SHELL = 3, BILLBOARD = 4,
              CLIFF = 5;

struct Tri { int i0, i1, i2, last; };

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_forge_prim top;

  auto reset = [&]() {
    top.j_valid_i = 0;
    top.t_ready_i = 1;
    top.view_sel_i = 1;
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  auto run = [&](int family, int segments, int sides, int mask, bool stall) {
    std::vector<Tri> out;
    top.j_valid_i = 1;
    top.j_family_i = family;
    top.j_segments_i = segments;
    top.j_sides_i = sides;
    top.j_material_i = 0x77;
    top.j_view_mask_i = mask;
    top.j_src_id_i = 0xBEEF;
    top.eval();
    zhao::tick(top);
    top.j_valid_i = 0;

    uint32_t g = 0x99u;
    for (int c = 0; c < 40000; ++c) {
      g = g * 1664525u + 1013904223u;
      top.t_ready_i = stall ? (((g >> 26) & 3u) != 0u) : 1;
      top.eval();
      if (top.t_valid_o && top.t_ready_i)
        out.push_back({static_cast<int>(top.t_i0_o), static_cast<int>(top.t_i1_o),
                       static_cast<int>(top.t_i2_o),
                       static_cast<int>(top.t_last_o)});
      const bool done = !top.t_valid_o;
      zhao::tick(top);
      if (done && !out.empty()) break;
    }
    top.t_ready_i = 1;
    return out;
  };

  reset();

  // ---- 1: the triangle count of every family ----------------------------
  {
    struct C { int fam; int seg, sides; int want; const char* why; };
    const C cases[] = {
        {RIBBON,    8, 5, 16,   "a ribbon collapses sides to 1: 8 quads"},
        {CLIFF,     8, 5, 16,   "a cliff/skirt likewise"},
        {FAN,       9, 6, 12,   "a fan collapses segments to 1: 6 quads"},
        {TUBE,      8, 6, 96,   "a tube is the two-dimensional case"},
        {SHELL,     4, 4, 32,   "and so is a shell"},
        {BILLBOARD, 9, 7,  2,   "a billboard is one quad, whatever it is asked"},
        {TUBE,     64, 8, 1024, "and the worst case is the contract's own"},
    };
    int bad = 0;
    for (const C& c : cases) {
      const auto out = run(c.fam, c.seg, c.sides, 1, false);
      if (static_cast<int>(out.size()) != c.want) {
        ++bad;
        std::printf("    family %d %dx%d -> %d triangles, wanted %d (%s)\n",
                    c.fam, c.seg, c.sides, static_cast<int>(out.size()), c.want,
                    c.why);
      }
    }
    zhao::check(bad == 0,
                "every family emits its own triangle count, and the worst case "
                "is the contract's 64 x 8 = 1,024",
                0, bad);
  }

  // ---- 2: A TUBE CLOSES, A RIBBON DOES NOT ------------------------------
  // The last side of a tube's ring wraps back to vertex 0. A ribbon's does not,
  // and welding its two edges together is invisible in a triangle count.
  {
    const auto tube = run(TUBE, 2, 4, 1, false);
    // the last quad of the first segment must reference side 0 again
    bool wraps = false;
    for (const Tri& t : tube)
      if (t.i1 == 0 || t.i2 == 0) wraps = true;
    zhao::check(wraps,
                "a TUBE's ring closes -- the last side's edge is side 0 again",
                1, wraps ? 1 : 0);

    const auto ribbon = run(RIBBON, 4, 1, 1, false);
    int maxidx = 0;
    for (const Tri& t : ribbon) {
      if (t.i0 > maxidx) maxidx = t.i0;
      if (t.i1 > maxidx) maxidx = t.i1;
      if (t.i2 > maxidx) maxidx = t.i2;
    }
    // an open 4-segment ribbon has 5 rings of 2 vertices: indices 0..9
    zhao::check(maxidx == 9,
                "and a RIBBON stays OPEN -- 4 segments of an open ring is ten "
                "vertices, not eight; closing it would weld its two edges",
                9, maxidx);
  }

  // ---- 3: THE ORDER IS THE CONTRACT -------------------------------------
  {
    const auto a = run(TUBE, 5, 4, 1, false);
    const auto b = run(TUBE, 5, 4, 1, false);
    int diff = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i)
      if (a[i].i0 != b[i].i0 || a[i].i1 != b[i].i1 || a[i].i2 != b[i].i2) ++diff;
    zhao::check(a.size() == b.size() && diff == 0,
                "the same job twice produces the IDENTICAL sequence -- two "
                "orderings look the same on screen and break every capture",
                0, diff);

    // and stalling the consumer does not reorder it
    const auto c = run(TUBE, 5, 4, 1, /*stall=*/true);
    int diff2 = 0;
    for (size_t i = 0; i < a.size() && i < c.size(); ++i)
      if (a[i].i0 != c[i].i0 || a[i].i1 != c[i].i1 || a[i].i2 != c[i].i2) ++diff2;
    zhao::check(a.size() == c.size() && diff2 == 0,
                "and a stalling consumer changes neither the order nor the "
                "count",
                0, diff2);
  }

  // ---- 4: `last` marks exactly one triangle -----------------------------
  {
    const auto out = run(SHELL, 3, 3, 1, false);
    int lasts = 0, last_at = -1;
    for (int i = 0; i < static_cast<int>(out.size()); ++i)
      if (out[i].last) { ++lasts; last_at = i; }
    zhao::check(lasts == 1 && last_at == static_cast<int>(out.size()) - 1,
                "`last` marks exactly the final triangle, once", 1,
                (lasts == 1 && last_at == static_cast<int>(out.size()) - 1) ? 1
                                                                           : 0);
  }

  // ---- 5: THE FOUR DELETED FAMILIES ARE REFUSED -------------------------
  // shard burst, chain, spline wall and low cone were removed by recognising
  // them as uses of something else. They are not reserved for later, and the
  // way a deleted feature comes back is a generator that quietly accepts its
  // encoding.
  {
    const uint32_t before = top.refused_family_o;
    int emitted = 0;
    for (int fam = 6; fam < 8; ++fam) {
      const auto out = run(fam, 4, 4, 1, false);
      emitted += static_cast<int>(out.size());
    }
    zhao::check(emitted == 0 && top.refused_family_o == before + 2,
                "a seventh family encoding is REFUSED and emits nothing -- the "
                "deleted four are not reserved, they are things this block must "
                "not grow back",
                2, static_cast<int>(top.refused_family_o - before));
  }

  // ---- 6: the frozen limits ---------------------------------------------
  {
    const uint32_t before = top.refused_limit_o;
    int emitted = 0;
    emitted += static_cast<int>(run(TUBE, 65, 4, 1, false).size());  // > 64
    emitted += static_cast<int>(run(TUBE, 4, 9, 1, false).size());   // > 8
    emitted += static_cast<int>(run(TUBE, 0, 4, 1, false).size());   // zero
    emitted += static_cast<int>(run(TUBE, 4, 0, 1, false).size());   // zero
    zhao::check(emitted == 0 && top.refused_limit_o == before + 4,
                "MAX_SEGMENTS = 64 and MAX_SIDES = 8 are frozen, and zero is "
                "not a primitive",
                4, static_cast<int>(top.refused_limit_o - before));
  }

  // ---- 7: the view mask --------------------------------------------------
  {
    const uint32_t before = top.skipped_view_o;
    const auto out = run(TUBE, 4, 4, /*mask=*/2, false);
    zhao::check(out.empty() && top.skipped_view_o == before + 1,
                "a job not masked for this view emits nothing, and is counted "
                "as skipped rather than refused",
                1, (out.empty() && top.skipped_view_o == before + 1) ? 1 : 0);
  }

  // ---- 8: RTL AGAINST THE ORACLE, POSITION BY POSITION -------------------
  // The contract cited `zref::ForgePrim` and no such symbol existed. Writing it
  // is only half the fix -- an oracle nobody compares against is the same
  // phantom wearing a definition. So every legal family is walked against
  // zref::forge::prim_triangle at every index, in order.
  {
    int mismatched = 0, compared = 0, counts_wrong = 0, biggest = 0;
    for (int fam = 0; fam <= 5; ++fam) {
      const int seg_cases[] = {1, 3, 8, 64};
      const int side_cases[] = {1, 2, 5, 8};
      for (int si = 0; si < 4; ++si) {
        for (int ki = 0; ki < 4; ++ki) {
          const int seg = seg_cases[si], sides = side_cases[ki];
          const auto out = run(fam, seg, sides, 1, false);
          const int want = zref::forge::prim_triangles(fam, seg, sides);
          if (static_cast<int>(out.size()) != want) {
            ++counts_wrong;
            std::printf("    fam %d %dx%d: %d triangles, oracle says %d\n", fam,
                        seg, sides, static_cast<int>(out.size()), want);
            continue;
          }
          if (want > biggest) biggest = want;
          for (int n = 0; n < want; ++n) {
            const zref::forge::Tri t =
                zref::forge::prim_triangle(fam, seg, sides, n);
            ++compared;
            if (out[n].i0 != t.i0 || out[n].i1 != t.i1 || out[n].i2 != t.i2) {
              if (mismatched < 4)
                std::printf(
                    "    fam %d %dx%d tri %d: rtl (%d,%d,%d) oracle (%d,%d,%d)\n",
                    fam, seg, sides, n, out[n].i0, out[n].i1, out[n].i2, t.i0,
                    t.i1, t.i2);
              ++mismatched;
            }
          }
        }
      }
    }
    // The stimulus has to have reached the worst case, or this proves nothing
    // about the number that sizes everything downstream.
    // Ask for the worst case BY NAME rather than for a triangle total: a
    // total is a number that can be reached by many small primitives, which is
    // exactly the coverage this guard exists to refuse.
    zhao::check(biggest == 1024 && compared == 6240,
                "the differential walk reached the contract's 1,024-triangle "
                "worst case, not merely a large count of small primitives",
                1024, biggest);
    zhao::check(counts_wrong == 0 && mismatched == 0,
                "every triangle of every legal family matches "
                "zref::forge::prim_triangle at its own index -- the ORDER, not "
                "just the set",
                0, mismatched + counts_wrong);
  }

  std::printf("  %u jobs, %u triangles, %u family-refused, %u limit-refused, "
              "%u view-skipped\n",
              top.jobs_o, top.triangles_o, top.refused_family_o,
              top.refused_limit_o, top.skipped_view_o);

  return zhao::report_and_exit("forge_prim_directed");
}
