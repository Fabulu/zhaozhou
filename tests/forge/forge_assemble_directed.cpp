// forge_assemble_directed.cpp -- the forge meshlet's join, under stimulus.
//
// WHAT THIS BLOCK IS AND WHY IT EXISTS: `zhao_forge_prim` emits INDEX TRIPLES
// and no coordinate; `zhao_forge_prim_eval` and `zhao_forge_ring_eval` emit
// WORLD positions and no topology; `zref_forge_page.hpp` names the ONLY thing
// joining them -- the ring-major ORDERING CONVENTION. Nothing in the tree held
// the vertices an index could name, which is why a forge primitive could not be
// composed. This is that store.
//
// WHAT ACTUALLY DISCRIMINATES HERE, named up front, because a bench that only
// showed vertices going in and a triangle coming out would pass against a
// block that emitted the LAST three vertices for every triple:
//
//   1. THE CORNERS ARE THE INDEXED VERTICES, NOT THE RECENT ONES. The fake
//      projector's arithmetic is INVERTIBLE (screen x is the world x's low
//      bits), so a triple (i0, i1, i2) must produce corners carrying vertex
//      i0's, i1's and i2's own numbers. The triples are deliberately NOT in
//      ascending order and NOT adjacent -- (7, 2, 11) discriminates where
//      (0, 1, 2) does not, because a block emitting a sliding window of three
//      passes the ascending case perfectly.
//
//   2. THE SECOND AND THIRD INDICES ARE LATCHED, NOT RE-READ. The index
//      stream's handshake completes on the accept cycle, so by the time the
//      walk reaches corner B the port already carries the NEXT triple. Two
//      triples offered back to back with disjoint indices is what separates a
//      latched walk from a port-reading one; with one triple in the job, both
//      behave identically and the bug ships.
//
//   3. THE IN-FLIGHT BOUND IS MEASURED, NOT ASSERTED. The projector's result
//      port has NO READY -- it fires 36 clocks after the accept whatever the
//      consumer is doing -- so the only thing between a burst of offers and a
//      lost result is the DUT's own throttle. The bench publishes
//      `fake_max_inflight_o` and the test requires it never to exceed
//      INFLIGHT. A throttle nobody measured is a claim.
//
//   4. AN OUT-OF-RANGE INDEX IS REFUSED, NOT WRAPPED. A triple naming a vertex
//      the job never emitted must increment `index_oor_o` and emit NOTHING. A
//      wrapped index reads a real vertex of the same job and draws a triangle
//      nobody authored, which no downstream check could see. We assert the
//      CORRECT behaviour -- the triangle count does not move -- rather than
//      asserting the bug.
//
//   5. `busy_o` COVERS THE WHOLE PRIMITIVE. It is what `zhao_forge_pagebank`
//      holds its next draw against, and the fault it prevents is draw B's
//      material set arriving under draw A's triangles -- with every counter in
//      the chain balancing, because no counter looks at the field that moved.
//      So it is checked HIGH from the first accepted vertex and LOW only after
//      the last triangle is taken.
//
//   6. TWO CONSECUTIVE JOBS, and the second's vertex store must not be
//      readable at the first's indices. A store that did not reset its count
//      would let job 2's triple (0,1,2) read job 1's leftovers and every
//      handshake would look healthy.
//
//   7. `dq_stray_o` AND `proj_stray_o` ARE NEGATIVE CONTROLS and stay at zero
//      throughout. Neither is reachable by legal stimulus at these ports, and
//      that is stated rather than hidden: `proj_stray_o` needs a rider the DUT
//      never issued, which only a broken demux can produce.
//
// THE ATTRIBUTE PACKET IS CHECKED FOR CONTENT, NOT SHAPE: slot 0 must carry a
// NON-ZERO canonical invw24 per corner (the depth converter ran), slot 1 must
// be ZERO (R197's declared-untextured profile) and slots 3..6 must carry the
// AUTHORED values verbatim. Asserting the colour arrives unchanged is what
// catches a packer that swapped two slots, which no picture at 240p would show.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vtb_forge_assemble.h"
#include "zhao_sim.hpp"

using zhao::check;
using zhao::tick;

namespace {

constexpr int kMaxVerts = 64;
constexpr int kInflight = 40;

constexpr int32_t kArtR = 62259;
constexpr int32_t kArtG = 63897;
constexpr int32_t kArtB = 65536;
constexpr int32_t kArtA = 65536;

struct Tri {
  uint16_t i0, i1, i2;
};

struct Got {
  int32_t ax, ay, bx, by, cx, cy;
  uint32_t a_invw, b_invw, c_invw;
  uint32_t a_uow, a_r, a_alpha;
  uint16_t src_id, material_id;
  uint32_t material_set;
  uint8_t untex, cull, tier;
};

void hard_reset(Vtb_forge_assemble& d) {
  d.rst_n = 0;
  d.v_valid_i = 0;
  d.t_valid_i = 0;
  d.o_ready_i = 1;
  d.view_sel_i = 0;
  d.art_r_i = kArtR;
  d.art_g_i = kArtG;
  d.art_b_i = kArtB;
  d.art_alpha_i = kArtA;
  d.art_quality_tier_i = 128;
  d.art_cull_mode_i = 0;
  d.j_material_set_i = 0;
  for (int i = 0; i < 8; ++i) tick(d);
  d.rst_n = 1;
  for (int i = 0; i < 4; ++i) tick(d);
}

// The world position a vertex index carries. Chosen so x, y and z are all
// DIFFERENT functions of the index: a join that returned the right vertex's x
// and the wrong one's y would otherwise pass.
int32_t wx(int i) { return 1000 + i * 7; }
int32_t wy(int i) { return 2000 + i * 13; }
int32_t wz(int i) { return 3000 + i * 3; }

// The fake projector's law, restated here by HAND rather than read back from
// the bench. Two expressions of one rule is the point: if the bench's
// arithmetic drifts, this disagrees, and a test that computed its expectation
// from the thing under test would agree with anything.
int32_t sx(int i) { return wx(i) & 0x1FFFFF; }
int32_t sy(int i) { return wy(i) & 0x1FFFFF; }

// Offer `n` vertices with NO `last`, honouring the DUT's ready, and return how
// many it refused. Used to overfill the store: the last vertex must not be
// marked, because a job whose store is full can never accept the vertex that
// would carry the marker.
int offer_without_last(Vtb_forge_assemble& d, int n, int* refused) {
  int sent = 0;
  int guard = 0;
  *refused = 0;
  while (sent < n && guard++ < 200000) {
    d.v_valid_i = 1;
    d.v_x_i = wx(sent);
    d.v_y_i = wy(sent);
    d.v_z_i = wz(sent);
    d.v_last_i = 0;
    d.eval();
    bool taken = d.v_ready_o != 0;
    tick(d);
    if (taken) ++sent; else (*refused)++;
    if (*refused > 4000) break;
  }
  d.v_valid_i = 0;
  d.eval();
  return sent;
}

// Drive `n` vertices in, honouring the DUT's ready. Returns the max number of
// cycles it stalled, which the caller uses to prove the throttle ENGAGED --
// a test that never saw a stall has not exercised the bound it asserts.
int feed_vertices(Vtb_forge_assemble& d, int n, uint32_t mset, int* stalls) {
  int sent = 0;
  int guard = 0;
  *stalls = 0;
  d.j_material_set_i = mset;
  while (sent < n && guard++ < 200000) {
    d.v_valid_i = 1;
    d.v_x_i = wx(sent);
    d.v_y_i = wy(sent);
    d.v_z_i = wz(sent);
    d.v_last_i = (sent == n - 1) ? 1 : 0;
    d.eval();
    bool taken = d.v_ready_o != 0;
    if (!taken) (*stalls)++;
    tick(d);
    if (taken) ++sent;
  }
  d.v_valid_i = 0;
  d.v_last_i = 0;
  d.eval();
  return sent;
}

// Offer the triples and collect every triangle that comes out.
std::vector<Got> run_triples(Vtb_forge_assemble& d, const std::vector<Tri>& tris,
                             uint16_t material, uint16_t src_id) {
  std::vector<Got> out;
  size_t sent = 0;
  int guard = 0;
  d.o_ready_i = 1;
  while ((sent < tris.size() || d.busy_o) && guard++ < 200000) {
    if (sent < tris.size()) {
      d.t_valid_i = 1;
      d.t_i0_i = tris[sent].i0;
      d.t_i1_i = tris[sent].i1;
      d.t_i2_i = tris[sent].i2;
      d.t_material_i = material;
      d.t_src_id_i = src_id;
      d.t_last_i = (sent == tris.size() - 1) ? 1 : 0;
    } else {
      d.t_valid_i = 0;
      d.t_last_i = 0;
    }
    d.eval();
    bool t_take = (d.t_valid_i && d.t_ready_o);
    if (d.o_valid_o && d.o_ready_i) {
      Got g{};
      g.ax = static_cast<int32_t>(d.o_ax_o);
      g.ay = static_cast<int32_t>(d.o_ay_o);
      g.bx = static_cast<int32_t>(d.o_bx_o);
      g.by = static_cast<int32_t>(d.o_by_o);
      g.cx = static_cast<int32_t>(d.o_cx_o);
      g.cy = static_cast<int32_t>(d.o_cy_o);
      g.a_invw = d.o_a_invw_o;
      g.b_invw = d.o_b_invw_o;
      g.c_invw = d.o_c_invw_o;
      g.a_uow = d.o_a_uow_o;
      g.a_r = d.o_a_r_o;
      g.a_alpha = d.o_a_alpha_o;
      g.src_id = d.o_src_id_o;
      g.material_id = d.o_material_id_o;
      g.material_set = d.o_material_set_o;
      g.untex = d.o_untex_o;
      g.cull = d.o_cull_mode_o;
      g.tier = d.o_quality_tier_o;
      out.push_back(g);
    }
    tick(d);
    if (t_take) ++sent;
  }
  d.t_valid_i = 0;
  d.eval();
  return out;
}

}  // namespace

int main() {
  Vtb_forge_assemble d;
  hard_reset(d);

  // ==========================================================================
  // 1. ONE PRIMITIVE, NON-ADJACENT TRIPLES -- the join, exactly
  // ==========================================================================
  check(d.busy_o == 0, "busy is LOW before any vertex", 0, d.busy_o);

  int stalls = 0;
  const int kN = 24;
  int sent = feed_vertices(d, kN, 0xDEADBEEFu, &stalls);
  check(sent == kN, "every offered vertex was accepted", kN, sent);
  check(d.busy_o == 1, "busy RISES with the primitive", 1, d.busy_o);

  // DELIBERATELY not ascending, not adjacent, and reusing indices across
  // triples. A block emitting a sliding window of three passes (0,1,2),(1,2,3)
  // and fails these.
  std::vector<Tri> tris = {{7, 2, 11}, {23, 0, 15}, {4, 4, 9}, {20, 1, 6}};
  auto got = run_triples(d, tris, 0x00A5, 0x1234);

  check(got.size() == tris.size(), "one triangle per legal triple",
        tris.size(), got.size());

  int corner_ok = 0;
  for (size_t k = 0; k < got.size() && k < tris.size(); ++k) {
    const Tri& t = tris[k];
    const Got& g = got[k];
    if (g.ax == sx(t.i0) && g.ay == sy(t.i0)) ++corner_ok;
    if (g.bx == sx(t.i1) && g.by == sy(t.i1)) ++corner_ok;
    if (g.cx == sx(t.i2) && g.cy == sy(t.i2)) ++corner_ok;
    check(g.ax == sx(t.i0), "corner A is vertex i0's x", sx(t.i0), g.ax);
    check(g.ay == sy(t.i0), "corner A is vertex i0's y", sy(t.i0), g.ay);
    // THE LATCH CHECK. i1 and i2 are read one and two cycles after the
    // handshake, so a walk reading the PORT gets the next triple's numbers.
    check(g.bx == sx(t.i1), "corner B is vertex i1's x (the LATCHED index)",
          sx(t.i1), g.bx);
    check(g.by == sy(t.i1), "corner B is vertex i1's y", sy(t.i1), g.by);
    check(g.cx == sx(t.i2), "corner C is vertex i2's x (the LATCHED index)",
          sx(t.i2), g.cx);
    check(g.cy == sy(t.i2), "corner C is vertex i2's y", sy(t.i2), g.cy);
  }
  check(corner_ok == 12, "all twelve corner coordinates come from their own index",
        12, corner_ok);

  // ---- the attribute packet, by CONTENT ------------------------------------
  if (!got.empty()) {
    const Got& g = got[0];
    check(g.a_invw != 0, "slot 0 carries a NON-ZERO canonical invw24 (the converter ran)",
          1, g.a_invw != 0);
    check(g.b_invw != 0, "corner B's invw24 is non-zero too", 1, g.b_invw != 0);
    check(g.c_invw != 0, "corner C's invw24 is non-zero too", 1, g.c_invw != 0);
    check(g.a_invw <= 0xFFFFFFu, "invw24 fits twenty-four bits", 1,
          g.a_invw <= 0xFFFFFFu);
    // The three corners index DIFFERENT vertices with different z, so their
    // depths must differ. Three equal depths is what a store written at one
    // address looks like, and every handshake would still balance.
    check(!(g.a_invw == g.b_invw && g.b_invw == g.c_invw),
          "three corners of one triangle carry THREE depths", 1,
          !(g.a_invw == g.b_invw && g.b_invw == g.c_invw));
    check(g.a_uow == 0u, "slot 1 (u/w) is ZERO -- R197's untextured profile", 0,
          g.a_uow);
    check(static_cast<int32_t>(g.a_r) == kArtR,
          "slot 3 carries the AUTHORED red verbatim", kArtR,
          static_cast<int32_t>(g.a_r));
    check(static_cast<int32_t>(g.a_alpha) == kArtA,
          "slot 6 carries the AUTHORED alpha verbatim", kArtA,
          static_cast<int32_t>(g.a_alpha));
    check(g.untex == 1, "the primitive DECLARES untextured (R197)", 1, g.untex);
    check(g.cull == 0, "the authored cull mode arrives", 0, g.cull);
    check(g.tier == 128, "the authored quality tier arrives", 128, g.tier);
    check(g.material_id == 0x00A5, "the material id rides the triangle", 0x00A5,
          g.material_id);
    check(g.material_set == 0xDEADBEEFu,
          "the material SET is the one latched at vertex zero", 0xDEADBEEFu,
          g.material_set);
    check(g.src_id == 0x1234, "the source id rides the triangle", 0x1234,
          g.src_id);
  }

  check(d.busy_o == 0, "busy FALLS only after the last triangle is taken", 0,
        d.busy_o);
  check(d.jobs_o == 1, "one primitive retired", 1, d.jobs_o);
  check(d.triangles_o == 4, "four triangles offered", 4, d.triangles_o);
  check(d.vertices_o == static_cast<uint32_t>(kN),
        "every vertex landed a canonical depth", kN, d.vertices_o);
  check(d.index_oor_o == 0, "no index was refused", 0, d.index_oor_o);
  check(d.vtx_overflow_o == 0, "the store did not overflow", 0, d.vtx_overflow_o);
  check(d.proj_stray_o == 0, "NEGATIVE CONTROL: no stray projector result", 0,
        d.proj_stray_o);
  check(d.dq_stray_o == 0, "NEGATIVE CONTROL: no stray depth token", 0,
        d.dq_stray_o);
  check(d.dq_refused_o == 0, "the converter refused nothing", 0, d.dq_refused_o);

  // ==========================================================================
  // 2. THE IN-FLIGHT BOUND -- measured at the fake, not asserted at the DUT
  // ==========================================================================
  check(d.fake_max_inflight_o <= static_cast<uint32_t>(kInflight),
        "the throttle NEVER let more than INFLIGHT requests fly", 1,
        d.fake_max_inflight_o <= static_cast<uint32_t>(kInflight));
  check(d.fake_accepted_o == d.fake_returned_o,
        "every projector request came back -- nothing was lost",
        d.fake_accepted_o, d.fake_returned_o);
  check(d.fake_accepted_o == static_cast<uint32_t>(kN),
        "the projector saw exactly the job's vertices", kN, d.fake_accepted_o);
  // The throttle must have ENGAGED, or the bound above is a claim about a
  // situation that never arose. 24 vertices against a 36-clock latency and a
  // 40-deep bound does not stall, so this is checked on the LONG job below.

  // ==========================================================================
  // 3. A SECOND PRIMITIVE -- the store must not carry the first's leftovers
  // ==========================================================================
  const int kN2 = 6;
  uint32_t v_before = d.vertices_o;
  sent = feed_vertices(d, kN2, 0x0BADF00Du, &stalls);
  check(sent == kN2, "the second primitive's vertices were accepted", kN2, sent);

  std::vector<Tri> tris2 = {{5, 0, 3}};
  auto got2 = run_triples(d, tris2, 0x0042, 0x5678);
  check(got2.size() == 1, "one triangle from the second primitive", 1,
        got2.size());
  if (!got2.empty()) {
    check(got2[0].ax == sx(5), "the second job's corner A is ITS vertex 5",
          sx(5), got2[0].ax);
    check(got2[0].material_set == 0x0BADF00Du,
          "the second job's material set replaced the first's", 0x0BADF00Du,
          got2[0].material_set);
  }
  check(d.jobs_o == 2, "two primitives retired", 2, d.jobs_o);
  check(d.vertices_o == v_before + kN2, "only the new vertices were projected",
        v_before + kN2, d.vertices_o);

  // ==========================================================================
  // 4. AN OUT-OF-RANGE INDEX IS REFUSED, NOT WRAPPED
  // ==========================================================================
  const int kN3 = 8;
  sent = feed_vertices(d, kN3, 0x11112222u, &stalls);
  check(sent == kN3, "the third primitive's vertices were accepted", kN3, sent);

  uint32_t tri_before = d.triangles_o;
  // 8 is one past the end of an 8-vertex job; 40 is far past it. Both must be
  // refused, and the legal triple between them must still come out.
  std::vector<Tri> tris3 = {{0, 1, 8}, {2, 3, 4}, {40, 0, 1}};
  auto got3 = run_triples(d, tris3, 0x0007, 0x9999);
  check(got3.size() == 1, "only the LEGAL triple produced a triangle", 1,
        got3.size());
  check(d.triangles_o == tri_before + 1, "the triangle count moved by exactly one",
        tri_before + 1, d.triangles_o);
  check(d.index_oor_o == 2, "both out-of-range triples were COUNTED", 2,
        d.index_oor_o);
  if (!got3.empty()) {
    check(got3[0].ax == sx(2), "the surviving triangle is the legal one", sx(2),
          got3[0].ax);
  }

  // ==========================================================================
  // 5. THE LONG JOB -- the throttle ENGAGES and is seen to
  // ==========================================================================
  uint32_t sp_before = d.slot_pressure_o;
  sent = feed_vertices(d, kMaxVerts, 0x33334444u, &stalls);
  check(sent == kMaxVerts, "a full store's worth of vertices was accepted",
        kMaxVerts, sent);
  check(stalls > 0, "the vertex stream STALLED -- the throttle engaged", 1,
        stalls > 0);
  check(d.slot_pressure_o > sp_before,
        "slot_pressure_o MOVED: the refusal is counted, not silent", 1,
        d.slot_pressure_o > sp_before);
  check(d.fake_max_inflight_o <= static_cast<uint32_t>(kInflight),
        "the bound held under a full store", 1,
        d.fake_max_inflight_o <= static_cast<uint32_t>(kInflight));

  std::vector<Tri> tris4 = {{63, 0, 32}, {1, 62, 31}};
  auto got4 = run_triples(d, tris4, 0x00FF, 0xABCD);
  check(got4.size() == 2, "both triples of the full job came out", 2,
        got4.size());
  if (got4.size() == 2) {
    check(got4[0].ax == sx(63), "the LAST vertex of a full store is readable",
          sx(63), got4[0].ax);
    check(got4[1].bx == sx(62), "and so is the one before it", sx(62),
          got4[1].bx);
  }

  check(d.fake_accepted_o == d.fake_returned_o,
        "still nothing lost after four primitives", d.fake_accepted_o,
        d.fake_returned_o);
  check(d.vtx_overflow_o == 0,
        "vtx_overflow_o stayed SILENT -- its positive control is the committed "
        "mutant, because no legal stimulus can reach it",
        0, d.vtx_overflow_o);
  check(d.proj_stray_o == 0, "NEGATIVE CONTROL held throughout", 0,
        d.proj_stray_o);
  check(d.dq_stray_o == 0, "NEGATIVE CONTROL held throughout", 0, d.dq_stray_o);
  check(d.jobs_o == 4, "four primitives retired in total", 4, d.jobs_o);

  // ==========================================================================
  // 6. vtx_overflow_o FIRES -- and it is reachable by LEGAL STIMULUS, so it
  //    owes no committed mutant. It owes this instead, and the DISCRIMINATION
  //    is the pair: SILENT at exactly MAX_VERTS (section 5 above, and again
  //    immediately below), FIRING at MAX_VERTS + 1.
  //
  //    A counter that only ever fires proves nothing; a counter only ever
  //    asserted zero proves less. CLAUDE.md's rule is that a detector reading
  //    zero is the claim to check hardest, and this is that check.
  //
  //    THE DECLARED CONSEQUENCE, stated because it is a real property and not
  //    an oversight: a job offering more than MAX_VERTS vertices can never
  //    complete, because the vertex carrying `last` can never be accepted. In
  //    the COMPOSED console that is unreachable -- the evaluators' own bounds
  //    are (MAX_SEGMENTS+1) * MAX_SIDES, which is exactly FORGE_MAX_VERTS --
  //    and the counter exists precisely to catch a future bound raised on one
  //    side only. The test therefore RESETS out of the state rather than
  //    pretending the block recovers from it.
  // ==========================================================================
  hard_reset(d);
  check(d.vtx_overflow_o == 0, "reset clears the counter", 0, d.vtx_overflow_o);

  int refused = 0;
  sent = offer_without_last(d, kMaxVerts, &refused);
  check(sent == kMaxVerts, "exactly MAX_VERTS vertices were accepted", kMaxVerts,
        sent);
  check(d.vtx_overflow_o == 0,
        "SILENT at exactly MAX_VERTS -- the negative half of the pair", 0,
        d.vtx_overflow_o);

  uint32_t ovf_before = d.vtx_overflow_o;
  // One more. It cannot be accepted, and the refusal is an OVERFLOW rather
  // than back-pressure, so it must reach the overflow counter and NOT
  // slot_pressure_o -- R95: the two refusals are different faults and must be
  // separable by whoever reads them.
  uint32_t sp_at_full = d.slot_pressure_o;
  d.v_valid_i = 1;
  d.v_x_i = wx(kMaxVerts);
  d.v_y_i = wy(kMaxVerts);
  d.v_z_i = wz(kMaxVerts);
  d.v_last_i = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  d.v_valid_i = 0;
  d.eval();

  check(d.vtx_overflow_o > ovf_before,
        "vtx_overflow_o FIRED on the vertex past MAX_VERTS", 1,
        d.vtx_overflow_o > ovf_before);
  check(d.slot_pressure_o == sp_at_full,
        "and slot_pressure_o stayed PUT -- the two refusals DISCRIMINATE (R95)",
        sp_at_full, d.slot_pressure_o);

  return zhao::report_and_exit("forge_assemble_directed");
}
