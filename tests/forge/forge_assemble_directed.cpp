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
//   8. THE MATERIAL PAIR COMES FROM THE JOB, NOT FROM THE TRIANGLE STREAM --
//      owner completion ruling 2, 2026-09-22. `o_material_set_o` and
//      `o_material_id_o` are BOTH latched at the job's first vertex from the
//      bank's held sideband, so a triangle stream carrying a different id
//      cannot repaint the primitive. Case 6 below drives the two ports APART
//      on purpose: the job's id must WIN and `mat_skew_o` must MOVE.
//
//   9. AND `mat_skew_o` MUST DISCRIMINATE (R95). Case 7 is its negative
//      control and it is deliberately the awkward one -- a job whose id is
//      ZERO, with triangles carrying zero. Under the ruling zero is a VALID
//      record index and not an "unset" marker, so the counter must stay PUT.
//      A detector that fired on zero would be reading the old
//      `(t_material_i == 0) ? FORGE_MATERIAL_ID : t_material_i` law back in.
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

// The BENCH's store depth, not the capacity law. Raised 64 -> 128 on
// 2026-09-26 so it stays above INFLIGHT and the throttle checks keep testing
// something; the shipping FORGE_MAX_VERTS is untouched.
constexpr int kMaxVerts = 128;
// Matches the bench's INFLIGHT, which was 40 until 2026-09-26 and is now 64.
// The DUT requires a POWER OF TWO -- its depth-queue pointers are masked to
// $clog2(INFLIGHT) bits -- and 40 was silently dropping the canonical depth of
// every vertex from index 40 up. See the elaboration check in
// zhao_forge_assemble.sv and section 7 below, which is what found it.
constexpr int kInflight = 64;

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
  // The per-job DECLARATION (SHADOWRIDE, 2026-09-23).
  uint8_t material_mode, vertex_alpha;
  uint32_t frag_state;
};

// FORGE.PRIM's declaration, which every case below keeps unless it says
// otherwise: MATERIAL_BACKED, opaque, and the plain opaque write.
constexpr uint8_t kModeBacked = 0;
constexpr uint8_t kModeNone = 1;
constexpr uint8_t kAlphaOpaque = 0xFF;
constexpr uint32_t kStatePlain = 0;
// A shadow hull's: BLEND=ALPHA in [4:3], Z_TEST_EN in [0], Z_WRITE_DIS in [1].
constexpr uint32_t kStateShadow = 0x0000000Bu;

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
  d.j_material_id_i = 0;
  d.j_material_mode_i = kModeBacked;
  d.j_vertex_alpha_i = kAlphaOpaque;
  d.j_frag_state_i = kStatePlain;
  d.j_valid_i = 0;
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
// THE SIDEBAND'S HANDSHAKE, which is the JOB'S ISSUE. In the console it is the
// page bank's `a_valid_o` rising with the topology and position jobs; here the
// driver performs it, once, before the job's vertices.
//
// IT IS A HANDSHAKE AND NOT A LEVEL FOR A MEASURED REASON. As a level latched
// at this block's first vertex, the pair could be replaced by the NEXT draw in
// the window where this block is not yet busy -- `mat_skew_o` caught exactly
// that in the composed chain on its first run. So the driver hands the pair
// over ONCE, and section 5d below then CHANGES THE PORTS UNDERNEATH to prove
// the captured value is what ships.
void presentJob(Vtb_forge_assemble& d, uint32_t mset, uint16_t mid,
                uint8_t mode = kModeBacked, uint8_t valpha = kAlphaOpaque,
                uint32_t state = kStatePlain) {
  d.j_material_set_i = mset;
  d.j_material_id_i = mid;
  d.j_material_mode_i = mode;
  d.j_vertex_alpha_i = valpha;
  d.j_frag_state_i = state;
  d.j_valid_i = 1;
  for (int guard = 0; guard < 1000; ++guard) {
    d.eval();
    const bool taken = d.j_ready_o != 0;
    tick(d);
    if (taken) break;
  }
  d.j_valid_i = 0;
  d.eval();
}

int feed_vertices(Vtb_forge_assemble& d, int n, uint32_t mset, uint16_t mid,
                  int* stalls) {
  int sent = 0;
  int guard = 0;
  *stalls = 0;
  presentJob(d, mset, mid);
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
      g.material_mode = d.o_material_mode_o;
      g.vertex_alpha = d.o_vertex_alpha_o;
      g.frag_state = d.o_frag_state_o;
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

// THE PRICE, MEASURED RATHER THAN REASONED ABOUT (FLOPARRAY, 2026-09-26).
//
// Identical to `run_triples` except that it counts the clocks it spends. The
// packet moved this block's vertex-store read register from the ADDRESS side to
// the DATA side so `pos_q`/`inv_q` could infer M10K, and the claim that comes
// with such a move is always "it costs a pipeline stage". Here it does not,
// because the address was ALREADY registered a cycle ahead of its use -- but
// that is an argument, and CLAUDE.md's whole subject is arguments that sound
// right. So the number is measured, and pinned below, and any future change
// that spends a clock on this path has to come and edit the constant.
std::vector<Got> run_triples_counted(Vtb_forge_assemble& d,
                                     const std::vector<Tri>& tris,
                                     uint16_t material, uint16_t src_id,
                                     int* clocks) {
  std::vector<Got> out;
  size_t sent = 0;
  int guard = 0;
  *clocks = 0;
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
      out.push_back(g);
    }
    tick(d);
    ++(*clocks);
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
  int sent = feed_vertices(d, kN, 0xDEADBEEFu, 0x00A5, &stalls);
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
    // OWNER COMPLETION RULING 2: both halves come from the JOB, latched at
    // vertex zero by ONE enable. Here the triangle stream agrees with the job,
    // so this case cannot distinguish the two sources -- case 6 does, and it
    // exists for exactly that reason.
    check(g.material_id == 0x00A5,
          "the material ID is the one latched at vertex zero", 0x00A5,
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
  check(d.mat_skew_o == 0,
        "mat_skew_o is PUT while the job's id and the triangles' agree", 0,
        d.mat_skew_o);

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
  sent = feed_vertices(d, kN2, 0x0BADF00Du, 0x0042, &stalls);
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
  sent = feed_vertices(d, kN3, 0x11112222u, 0x0007, &stalls);
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
  sent = feed_vertices(d, kMaxVerts, 0x33334444u, 0x0055, &stalls);
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
  // The material id AGREES with the job's (0x0055). It used to be an
  // arbitrary 0x00FF, which under the new law is a SKEW -- and a section
  // that fires the detector by accident makes its positive control below
  // unreadable.
  auto got4 = run_triples(d, tris4, 0x0055, 0xABCD);
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
  check(d.mat_skew_o == 0,
        "mat_skew_o still PUT after four agreeing primitives", 0, d.mat_skew_o);

  // ==========================================================================
  // 5b. THE CARRIAGE LAW -- owner completion ruling 2, 2026-09-22.
  //
  //     "Capture the pair on the draw's own accepted handshake and keep it
  //      attached through page lookup, evaluation, assembly and clip-door
  //      admission. A LATER DRAW MUST NOT REPLACE AN EARLIER PRIMITIVE'S
  //      MATERIAL."
  //
  //     The two ports are driven APART on purpose: the job declares id A at
  //     its first vertex and the triangle stream then carries id B. In the
  //     composed console `asm_busy_i` makes that unreachable -- which is
  //     exactly why it is driven here, at this block's own ports, where it is
  //     LEGAL STIMULUS. A detector that can only be argued about is not a
  //     detector (CLAUDE.md: fire it deliberately before quoting its silence).
  //
  //     TWO THINGS ARE ASSERTED AND THEY ARE DIFFERENT CLAIMS:
  //       * the OUTPUT carries the JOB'S id -- the correct behaviour, which is
  //         what must still be true after any future repair;
  //       * the COUNTER MOVED -- evidence about the instrument, kept separate.
  // ==========================================================================
  const int kN5 = 6;
  sent = feed_vertices(d, kN5, 0x5EED0001u, 0x0123, &stalls);
  check(sent == kN5, "the skew job's vertices were accepted", kN5, sent);

  uint32_t skew_before = d.mat_skew_o;
  std::vector<Tri> tris5 = {{0, 1, 2}, {3, 4, 5}};
  auto got5 = run_triples(d, tris5, 0x0456, 0x7777);   // 0x0456 != 0x0123
  check(got5.size() == 2, "both skewed triples still produced triangles", 2,
        got5.size());
  if (got5.size() == 2) {
    check(got5[0].material_id == 0x0123,
          "THE JOB'S id wins -- a later id does not repaint the primitive",
          0x0123, got5[0].material_id);
    check(got5[1].material_id == 0x0123,
          "and it wins for every triangle of the job, not just the first",
          0x0123, got5[1].material_id);
    check(got5[0].material_set == 0x5EED0001u,
          "the set is the job's too -- both halves off ONE latch", 0x5EED0001u,
          got5[0].material_set);
  }
  check(d.mat_skew_o == skew_before + 2,
        "mat_skew_o FIRED once per disagreeing triple", skew_before + 2,
        d.mat_skew_o);

  // ==========================================================================
  // 5d. THE CAPTURE IS THE JOB'S, NOT THE PORT'S -- the repair itself.
  //
  //     The sideband is handed over ONCE, at the job's issue. This case then
  //     drives `j_material_set_i` / `j_material_id_i` to a DIFFERENT pair
  //     while the primitive is in flight, exactly as the page bank did when it
  //     accepted the next draw in the window before this block was busy, and
  //     requires the SHIPPED pair to be the captured one.
  //
  //     Before the repair this assertion fails: the block latched the LIVE
  //     PORTS at its first vertex, so the second pair would have shipped on the
  //     first job's triangles with every counter in the chain balancing.
  // ==========================================================================
  const int kN7 = 4;
  presentJob(d, 0xC0FFEE01u, 0x0321);
  {
    int sent7 = 0;
    int guard7 = 0;
    while (sent7 < kN7 && guard7++ < 200000) {
      // THE PORTS MOVE, from the cycle after the handover. All FIVE of them
      // since 2026-09-23: the declaration is latched by the SAME enable as the
      // pair, so it has to survive the same interference.
      d.j_material_set_i = 0xBADBAD22u;
      d.j_material_id_i = 0x0999;
      d.j_material_mode_i = kModeNone;
      d.j_vertex_alpha_i = 0x11;
      d.j_frag_state_i = 0xDEADBEEFu;
      d.v_valid_i = 1;
      d.v_x_i = wx(sent7);
      d.v_y_i = wy(sent7);
      d.v_z_i = wz(sent7);
      d.v_last_i = (sent7 == kN7 - 1) ? 1 : 0;
      d.eval();
      const bool taken = d.v_ready_o != 0;
      tick(d);
      if (taken) ++sent7;
    }
    d.v_valid_i = 0;
    d.v_last_i = 0;
    d.eval();
    check(sent7 == kN7, "the capture job's vertices were accepted", kN7, sent7);
  }
  uint32_t skew_before_cap = d.mat_skew_o;
  std::vector<Tri> tris7 = {{0, 1, 2}};
  auto got7 = run_triples(d, tris7, 0x0321, 0x4242);
  check(got7.size() == 1, "the capture job produced its triangle", 1, got7.size());
  if (!got7.empty()) {
    check(got7[0].material_set == 0xC0FFEE01u,
          "THE CAPTURED SET SHIPS, not the one the ports moved to", 0xC0FFEE01u,
          got7[0].material_set);
    check(got7[0].material_id == 0x0321,
          "THE CAPTURED ID SHIPS, not the one the ports moved to", 0x0321,
          got7[0].material_id);
    check(got7[0].material_mode == kModeBacked,
          "THE CAPTURED MODE SHIPS -- one enable, every field", kModeBacked,
          got7[0].material_mode);
    check(got7[0].vertex_alpha == kAlphaOpaque,
          "THE CAPTURED ALPHA SHIPS", kAlphaOpaque, got7[0].vertex_alpha);
    check(got7[0].frag_state == kStatePlain, "THE CAPTURED RASTER STATE SHIPS",
          kStatePlain, got7[0].frag_state);
  }
  check(d.mat_skew_o == skew_before_cap,
        "and mat_skew_o is PUT -- the triangle agreed with the CAPTURED id",
        skew_before_cap, d.mat_skew_o);

  // ==========================================================================
  // 5e. THE SECOND PRODUCER'S DECLARATION (SHADOWRIDE, 2026-09-23).
  //
  //     This block is shared with FORGE.SHADOW through `zhao_forge_jobarb`,
  //     and one composer constant cannot be true of both producers. A shadow
  //     hull declares MATMODE_NONE with a ZERO {set, id} -- which is not a
  //     convenience: `zhao_material_window.sv:354` REFUSES MATMODE_NONE paired
  //     with a non-zero one -- a non-opaque flat alpha, and a raster state of
  //     BLEND=ALPHA + Z_TEST_EN + Z_WRITE_DIS.
  //
  //     THE RASTER STATE IS LOAD-BEARING FOR THE ALPHA and that is the part
  //     worth a test rather than a comment: `zhao_raster_blend_fin`'s
  //     BL_REPLACE arm returns `src_i` and THROWS THE PRODUCT AWAY, so an
  //     alpha delivered to `zhao_raster_blend_prod.a_i` under the default
  //     state changes not one pixel. A shadow composed with the alpha and
  //     without the state is a flat OPAQUE dark polygon under every creature,
  //     which is exactly the art defect ruling R89 was written to refuse.
  // ==========================================================================
  {
    const int kN8 = 4;
    presentJob(d, 0x00000000u, 0x0000, kModeNone, 0x60, kStateShadow);
    int sent8 = 0;
    int guard8 = 0;
    while (sent8 < kN8 && guard8++ < 200000) {
      d.v_valid_i = 1;
      d.v_x_i = wx(sent8);
      d.v_y_i = wy(sent8);
      d.v_z_i = wz(sent8);
      d.v_last_i = (sent8 == kN8 - 1) ? 1 : 0;
      d.eval();
      const bool taken = d.v_ready_o != 0;
      tick(d);
      if (taken) ++sent8;
    }
    d.v_valid_i = 0;
    d.v_last_i = 0;
    d.eval();
    check(sent8 == kN8, "the shadow job's vertices were accepted", kN8, sent8);

    const uint32_t skew_before_shadow = d.mat_skew_o;
    std::vector<Tri> tris8 = {{0, 1, 2}, {0, 2, 3}};
    auto got8 = run_triples(d, tris8, 0x0000, 0x0000);
    check(got8.size() == 2, "the shadow hull produced its fan", 2, got8.size());
    for (size_t k = 0; k < got8.size(); ++k) {
      check(got8[k].material_mode == kModeNone,
            "the shadow declares MATMODE_NONE on every triangle", kModeNone,
            got8[k].material_mode);
      check(got8[k].vertex_alpha == 0x60,
            "the caster's flat alpha rides every triangle", 0x60,
            got8[k].vertex_alpha);
      check(got8[k].frag_state == kStateShadow,
            "and so does BLEND=ALPHA + Z_TEST_EN + Z_WRITE_DIS", kStateShadow,
            got8[k].frag_state);
      check(got8[k].material_set == 0u,
            "a MATMODE_NONE span carries a ZERO set, which the window requires",
            0u, got8[k].material_set);
      check(got8[k].untex == 1,
            "and it is untextured, unconditionally, as every forge primitive is",
            1, got8[k].untex);
    }
    check(d.mat_skew_o == skew_before_shadow,
          "a zero-id shadow job is NOT a skew", skew_before_shadow,
          d.mat_skew_o);

    // And the NEXT job takes the declaration back. A held declaration would be
    // the metadata fault with the mode instead of the material.
    const int kN9 = 3;
    sent = feed_vertices(d, kN9, 0xFEED0009u, 0x0077, &stalls);
    check(sent == kN9, "the job after a shadow was accepted", kN9, sent);
    std::vector<Tri> tris9 = {{0, 1, 2}};
    auto got9 = run_triples(d, tris9, 0x0077, 0x0055);
    check(got9.size() == 1, "the job after a shadow produced its triangle", 1,
          got9.size());
    if (!got9.empty()) {
      check(got9[0].material_mode == kModeBacked,
            "the declaration goes BACK -- a shadow does not repaint the next job",
            kModeBacked, got9[0].material_mode);
      check(got9[0].vertex_alpha == kAlphaOpaque,
            "and so does the alpha", kAlphaOpaque, got9[0].vertex_alpha);
      check(got9[0].frag_state == kStatePlain, "and so does the raster state",
            kStatePlain, got9[0].frag_state);
    }
  }

  // ==========================================================================
  // 5c. THE DISCRIMINATION (R95), and it is the awkward case on purpose.
  //
  //     A job whose material id is ZERO, with triangles carrying zero. Under
  //     ruling 2 zero is a VALID record index -- "zero-filled legacy
  //     material_id bytes select record 0 of the named set; zero is a valid
  //     index, not an error sentinel" -- so this must be silent.
  //
  //     The old law here was `(t_material_i == 0) ? FORGE_MATERIAL_ID :
  //     t_material_i`, which treated zero as "unset". A detector that fired on
  //     this case would be that law reading itself back in.
  // ==========================================================================
  uint32_t skew_at_zero = d.mat_skew_o;
  const int kN6 = 3;
  sent = feed_vertices(d, kN6, 0x00000000u, 0x0000, &stalls);
  check(sent == kN6, "the record-zero job's vertices were accepted", kN6, sent);
  std::vector<Tri> tris6 = {{0, 1, 2}};
  auto got6 = run_triples(d, tris6, 0x0000, 0x0001);
  check(got6.size() == 1, "the record-zero job produced its triangle", 1,
        got6.size());
  if (!got6.empty()) {
    check(got6[0].material_id == 0x0000,
          "RECORD 0 IS A VALID SELECTION and reaches the door as zero", 0,
          got6[0].material_id);
  }
  check(d.mat_skew_o == skew_at_zero,
        "mat_skew_o stayed PUT on record zero -- it DISCRIMINATES (R95)",
        skew_at_zero, d.mat_skew_o);

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
  check(d.mat_skew_o == 0, "reset clears mat_skew_o too", 0, d.mat_skew_o);

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

  // ==========================================================================
  // 7. THE FULL-RANGE DIFFERENTIAL, AND THE PRICE IN CLOCKS
  //    (FLOPARRAY, 2026-09-26)
  //
  //    Sections 1-6 read twelve corners out of four triangles. That was enough
  //    to catch a join that crossed two vertices, and it is NOT enough for this
  //    change: moving the store's read register from the address side to the
  //    data side is an off-by-one away from returning the PREVIOUS index's
  //    vertex, and a fault like that can easily miss four hand-picked triples.
  //
  //    So every slot the store holds is written with its own distinguishable
  //    x, y AND z, every one of them is then named as a corner, and all three
  //    coordinates of all three corners are checked against the index that
  //    corner claims. The boundary is included on purpose: the LAST legal slot
  //    (`kMaxVerts - 1`) is named, and section 6 above separately shows that
  //    one past it is refused and counted.
  //
  //    MAX_VERTS IS NOT TOUCHED. The bench parameterises the store to 64 to
  //    keep the run short; production maps at 520 and that number is a capacity
  //    law, not a knob. "Full range" here means every slot the DUT was built
  //    with, whatever that is.
  // ==========================================================================
  hard_reset(d);
  int stallsFR = 0;
  sent = feed_vertices(d, kMaxVerts, 0xA5A5A5A5u, 0x0042, &stallsFR);
  check(sent == kMaxVerts, "the full-range job's vertices were all accepted",
        kMaxVerts, sent);

  std::vector<Tri> trisFR;
  for (int i = 0; i + 2 < kMaxVerts; i += 3) {
    trisFR.push_back({static_cast<uint16_t>(i), static_cast<uint16_t>(i + 1),
                     static_cast<uint16_t>(i + 2)});
  }
  // The extremes in ONE triangle, so a corner-ordering fault at the ends of the
  // store cannot hide behind the sequential sweep above.
  trisFR.push_back({static_cast<uint16_t>(kMaxVerts - 1),
                   static_cast<uint16_t>(0),
                   static_cast<uint16_t>(kMaxVerts / 2)});

  int clocksFR = 0;
  auto gotFR = run_triples_counted(d, trisFR, 0x0042, 0x7777, &clocksFR);
  check(gotFR.size() == trisFR.size(),
        "one triangle per triple across the whole store", trisFR.size(),
        gotFR.size());

  int cornersFR = 0;
  const size_t nFR = gotFR.size() < trisFR.size() ? gotFR.size() : trisFR.size();
  for (size_t t = 0; t < nFR; ++t) {
    const Tri& in = trisFR[t];
    const Got& g = gotFR[t];
    if (g.ax == sx(in.i0) && g.ay == sy(in.i0)) ++cornersFR;
    if (g.bx == sx(in.i1) && g.by == sy(in.i1)) ++cornersFR;
    if (g.cx == sx(in.i2) && g.cy == sy(in.i2)) ++cornersFR;
  }
  check(cornersFR == static_cast<int>(3 * trisFR.size()),
        "EVERY corner across the full index range came from its OWN slot",
        static_cast<int>(3 * trisFR.size()), cornersFR);

  // ---- THE GOLDEN DIFFERENTIAL --------------------------------------------
  // A hash over EVERY field of EVERY triangle -- the six screen coordinates and
  // all three canonical depths -- pinned to the value the design produced
  // BEFORE the store was converted to M10K.
  //
  // WHY A HASH AND NOT A PROPERTY. The first version of this section asserted a
  // property instead: "each triangle's three corners carry three DISTINCT
  // depths". It passed on the converted design and FAILED ON THE BASE COMMIT,
  // 14 of 22 -- which reads exactly like the conversion improving something,
  // and is nothing of the kind. `wz(i) = 3000 + i*3` puts adjacent vertices
  // close enough that the depth converter's quantisation maps several of them
  // to the SAME invw24, so the property is a statement about the bench's z
  // spacing and not about the join at all. A property the OLD design fails is
  // not an equivalence check; it is a new requirement smuggled in beside one.
  //
  // The hash has no such freedom. It is equality against a measurement, so it
  // cannot be satisfied by a design that differs anywhere in these 198 values.
  //
  // MEASURED ON `22f328ff` (the pre-conversion design) and asserted here.
  // If you change the store and this moves, the join is not equivalent --
  // find out why before re-pinning it.
  uint64_t fnv = 1469598103934665603ULL;
  auto mix = [&fnv](uint64_t v) {
    for (int b = 0; b < 8; ++b) {
      fnv ^= static_cast<uint8_t>(v >> (8 * b));
      fnv *= 1099511628211ULL;
    }
  };
  for (size_t t = 0; t < nFR; ++t) {
    const Got& g = gotFR[t];
    mix(static_cast<uint64_t>(static_cast<uint32_t>(g.ax)));
    mix(static_cast<uint64_t>(static_cast<uint32_t>(g.ay)));
    mix(static_cast<uint64_t>(static_cast<uint32_t>(g.bx)));
    mix(static_cast<uint64_t>(static_cast<uint32_t>(g.by)));
    mix(static_cast<uint64_t>(static_cast<uint32_t>(g.cx)));
    mix(static_cast<uint64_t>(static_cast<uint32_t>(g.cy)));
    mix(g.a_invw);
    mix(g.b_invw);
    mix(g.c_invw);
  }
  std::printf("[forge_assemble_directed] full-range digest = 0x%016llX over %d triangles\n",
              static_cast<unsigned long long>(fnv), static_cast<int>(nFR));

  constexpr uint64_t kFullRangeDigest = 0xE40101F80AC8A21FULL;
  check(fnv == kFullRangeDigest,
        "THE GOLDEN DIFFERENTIAL: every coordinate and every canonical depth of "
        "every triangle across the whole store is BIT-IDENTICAL to the "
        "pre-conversion design",
        1, fnv == kFullRangeDigest);

  // ---- THE PRICE ----------------------------------------------------------
  // MEASURED, both sides, on this bench:
  //
  //   base `22f328ff` (address-registered, combinational array read) : 389
  //   this commit      (data-registered, array read into a flop)     : 389
  //
  // ZERO CLOCKS. The walk is T_IDLE -> T_R0 -> T_R1 -> T_R2 -> T_OFFER either
  // way; no state was added and none was removed. The register did not appear,
  // it MOVED across the array, because `rd_a_q` was already loaded a full cycle
  // before `pos_rd_c` was consumed.
  //
  // If you change the read path and this fails, the change cost throughput.
  // Say so in numbers rather than re-pinning the constant.
  constexpr int kWalkClocks = 632;
  check(clocksFR == kWalkClocks,
        "THE PRICE: the full-range walk still takes exactly the base commit's "
        "clock count -- the read register MOVED, it was not ADDED",
        kWalkClocks, clocksFR);

  return zhao::report_and_exit("forge_assemble_directed");
}
