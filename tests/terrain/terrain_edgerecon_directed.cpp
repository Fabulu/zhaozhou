// terrain_edgerecon_directed.cpp -- TERRAIN.EDGERECON, the neighbour-edge
// level producer commissioned by owner ruling 2026-09-22 item 6.
//
// Contract: design/contracts/TERRAIN.EDGERECON.md.
//
// WHAT THIS GUARDS, and every one of these is silent in a result-only test:
//
//   * AN ASYMMETRIC ANSWER TEARS THE ISLAND. `zhao_terrain_tess` tessellates a
//     shared edge at max(neighbour, own). If P is told Q's true level and Q is
//     told the fallback, the two sides emit different vertex counts on the
//     same seam and the ground cracks along it -- with every counter in the
//     console balancing, because no counter looks at the pair. Case 2 is the
//     SYMMETRY THEOREM checked by exhaustion over a 3 x 3 patch block: for
//     every shared seam and every lane of it, max(own, given) computed from
//     BOTH sides must be equal. That is the tessellator's own arithmetic, not
//     a proxy for it.
//   * A LOOKUP THAT TRANSPOSES A BORDER ROW is individually self-consistent and
//     globally wrong -- the same defect `terrain_lod_tess` caught inside one
//     patch, one level up. Case 1 gives all sixteen subpatches of two patches
//     DIFFERENT levels, so a lookup that reads the wrong row or the wrong end
//     of the right row cannot accidentally be right.
//   * AN ANSWER THAT DEPENDS ON ARRIVAL ORDER would make the island's geometry
//     a function of the paging schedule. Case 3 files the same set in four
//     different orders and requires byte-identical answers.
//   * A PREVIOUS-FRAME ANSWER is the one shortcut the ruling names and
//     forbids. Case 7 files a frame, proves the answers real, pulses
//     `frame_begin_i`, and requires every lane to fall back.
//
// R95: EVERY COUNTER IS FIRED ON PURPOSE AND SHOWN SILENT BESIDE IT. All nine
// are reachable from this block's own boundary with legal stimulus, so no
// committed mutant is owed here.
//
//   records_filed_o       case 1  sixteen lanes complete a record
//   lanes_filed_o         case 10 ... and the underside beat does not add to it
//   collisions_o          case 6  two live patches on one direct-mapped index
//   queries_o             case 1
//   edges_real_o          case 1  a lane answered from a decision
//   edges_fallback_o      case 4  a lane answered 8'h00
//   query_own_missing_o   case 5  the queried patch's own record is unusable
//   file_out_of_phase_o   case 8  a file beat offered during EMIT
//   query_out_of_phase_o  case 8  a query offered during PREPARE
//
// AND CASE 12 IS THE ONE THE OTHER ELEVEN COULD NOT SEE. They all query and
// then read, so none of them looks at `edge_*` while a walk is in flight --
// and the block's first version cleared the published registers on the query
// ACCEPT, dropping the port to 8'h00 for seven clocks. Every case passed.
// Case 12 walks the second query cycle by cycle and requires the FIRST
// answer to stand until `q_done_o`.
//
// AND THE INVARIANT THAT MAKES THE TWO EDGE COUNTERS WORTH READING:
// `edges_real_o + edges_fallback_o == 4 * queries_o`. The two are incremented
// on one event from complementary predicates over four bits written by four
// different states from four different bank words, so a fault in the predicate
// moves one sum and not the other. This repository has shipped a mismatch
// counter whose two operands were loaded by the same enable and which
// therefore read zero for ever; case 11 asserts the invariant AND case 4
// proves the fallback side moves, so the silence of neither is quoted alone.
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_edgerecon.h"

#include "zhao_sim.hpp"

namespace {

int failures = 0;
int checks = 0;

void check_eq(uint64_t got, uint64_t want, const char* what) {
  ++checks;
  if (got != want) {
    std::printf("FAIL: %s -- expected 0x%llx, got 0x%llx\n", what,
                static_cast<unsigned long long>(want), static_cast<unsigned long long>(got));
    ++failures;
  }
}

void check_true(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

// A patch's sixteen subpatch levels, indexed n = j*4 + i exactly as
// TERRAIN.LOD's fixed emit order pins them.
struct Patch {
  uint32_t ix = 0;
  uint32_t iz = 0;
  uint32_t lvl[16] = {0};

  uint32_t at(uint32_t i, uint32_t j) const { return lvl[j * 4 + i]; }

  // The four border words, in `zhao_terrain_lod`'s `edge_lane()` packing:
  // lane k in bits [2k+1:2k].
  uint8_t row_j3() const {  // shared with the +z neighbour
    uint8_t w = 0;
    for (uint32_t i = 0; i < 4; ++i) w |= static_cast<uint8_t>(at(i, 3) << (2 * i));
    return w;
  }
  uint8_t row_j0() const {  // shared with the -z neighbour
    uint8_t w = 0;
    for (uint32_t i = 0; i < 4; ++i) w |= static_cast<uint8_t>(at(i, 0) << (2 * i));
    return w;
  }
  uint8_t col_i3() const {  // shared with the +x neighbour
    uint8_t w = 0;
    for (uint32_t j = 0; j < 4; ++j) w |= static_cast<uint8_t>(at(3, j) << (2 * j));
    return w;
  }
  uint8_t col_i0() const {  // shared with the -x neighbour
    uint8_t w = 0;
    for (uint32_t j = 0; j < 4; ++j) w |= static_cast<uint8_t>(at(0, j) << (2 * j));
    return w;
  }
};

// The answer the block publishes for one patch.
struct Answer {
  uint8_t nz = 0, pz = 0, nx = 0, px = 0;
  uint8_t real = 0;
  bool operator==(const Answer& o) const {
    return nz == o.nz && pz == o.pz && nx == o.nx && px == o.px && real == o.real;
  }
};

class Rig {
 public:
  Vzhao_terrain_edgerecon t;

  void quiet() {
    t.frame_begin_i = 0;
    t.prepare_done_i = 0;
    t.f_valid_i = 0;
    t.f_ix_i = 0;
    t.f_iz_i = 0;
    t.f_ox_i = 0;
    t.f_oz_i = 0;
    t.f_level_i = 0;
    t.f_surface_i = 0;
    t.q_valid_i = 0;
    t.q_ix_i = 0;
    t.q_iz_i = 0;
  }

  void reset() {
    quiet();
    t.rst_n = 0;
    for (int i = 0; i < 3; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
    // The reset sweep walks every entry's valid bit to zero. Wait it out
    // rather than assuming a length: `busy_o` is the block's own statement
    // that it is done, and a test that counted clocks instead would go green
    // on a sweep that never ran.
    idle(4096, /*bound_on_busy=*/true);
  }

  void idle(int n, bool bound_on_busy = false) {
    for (int i = 0; i < n; ++i) {
      quiet();
      t.eval();
      if (bound_on_busy && !t.busy_o && t.phase_o == 2) return;
      zhao::tick(t);
      t.eval();
    }
    if (bound_on_busy) {
      check_true(false, "reset sweep finished inside its bound");
    }
  }

  // ---- phases ------------------------------------------------------------
  void frame_begin() {
    quiet();
    t.frame_begin_i = 1;
    t.eval();
    zhao::tick(t);
    t.frame_begin_i = 0;
    t.eval();
    // The frame sweep runs before PREPARE opens.
    for (int i = 0; i < 4096; ++i) {
      quiet();
      t.eval();
      if (t.phase_o == 1) return;
      zhao::tick(t);
      t.eval();
    }
    check_true(false, "frame sweep finished inside its bound");
  }

  void prepare_done() {
    quiet();
    t.prepare_done_i = 1;
    t.eval();
    zhao::tick(t);
    t.prepare_done_i = 0;
    t.eval();
  }

  // ---- filing ------------------------------------------------------------
  // One lane. `surface` = 1 is the underside replay: TERRAIN.LOD emits it with
  // the top's level and this block must consume it without filing it.
  void file_lane(const Patch& p, uint32_t n, uint32_t surface = 0, int gap = 0) {
    for (int g = 0; g < gap; ++g) {
      quiet();
      t.eval();
      zhao::tick(t);
      t.eval();
    }
    for (int spin = 0; spin < 64; ++spin) {
      quiet();
      t.f_valid_i = 1;
      t.f_ix_i = p.ix;
      t.f_iz_i = p.iz;
      t.f_ox_i = (n & 3u) * 8u;
      t.f_oz_i = (n >> 2) * 8u;
      t.f_level_i = p.lvl[n];
      t.f_surface_i = surface;
      t.eval();
      bool taken = t.f_ready_o != 0;
      zhao::tick(t);
      t.eval();
      if (taken) {
        // The bank is a read-modify-write: the accept presents the address and
        // the NEXT clock commits the word and moves the counters. Settle that
        // clock here, so a caller reading `lanes_filed_o` straight after a
        // patch sees the sixteenth lane. Without it the counter reads 47 of 48
        // and the shortfall looks like a filing defect rather than a sampling
        // one -- which is how it first presented.
        quiet();
        t.eval();
        zhao::tick(t);
        t.eval();
        return;
      }
    }
    check_true(false, "a file beat was accepted inside its bound");
  }

  void file_patch(const Patch& p, int gap = 0) {
    for (uint32_t n = 0; n < 16; ++n) file_lane(p, n, 0, gap);
  }

  // ---- querying ----------------------------------------------------------
  Answer query(uint32_t ix, uint32_t iz) {
    for (int spin = 0; spin < 64; ++spin) {
      quiet();
      t.q_valid_i = 1;
      t.q_ix_i = ix;
      t.q_iz_i = iz;
      t.eval();
      bool taken = t.q_ready_o != 0;
      zhao::tick(t);
      t.eval();
      if (taken) break;
      if (spin == 63) check_true(false, "a query was accepted inside its bound");
    }
    for (int spin = 0; spin < 64; ++spin) {
      quiet();
      t.eval();
      bool done = t.q_done_o != 0;
      zhao::tick(t);
      t.eval();
      if (done) break;
      if (spin == 63) check_true(false, "a query completed inside its bound");
    }
    Answer a{};
    a.nz = static_cast<uint8_t>(t.edge_nz_o);
    a.pz = static_cast<uint8_t>(t.edge_pz_o);
    a.nx = static_cast<uint8_t>(t.edge_nx_o);
    a.px = static_cast<uint8_t>(t.edge_px_o);
    a.real = static_cast<uint8_t>(t.edge_real_o);
    return a;
  }
};

// A deterministic level grid whose ROWS and COLUMNS each take all four levels,
// so a lookup that reads the wrong cell cannot accidentally be right.
//
// THE FIRST VERSION WAS `n * 7 + ...`, AND IT WAS QUIETLY WEAKER THAN ITS OWN
// COMMENT CLAIMED. `n` steps by 4 between rows and 7 x 4 = 28 is 0 mod 4, so
// every COLUMN of every patch came out UNIFORM: `col_i0()` and `col_i3()` were
// four copies of one level. Every case still passed -- they compare exact
// values -- but a transposed or reversed COLUMN lookup would have been
// accidentally right, which is the one defect case 1 exists to catch.
//
// It was found by a CONTROL in case 12 asserting that two answers it was
// comparing actually differ; the control failed because one of them was all
// zeroes. A coverage assertion earning its keep, exactly as the LOD suite's
// three zero-on-first-run counters did.
//
// `i + 3j` walks 0,3,2,1 down a column and 0,1,2,3 along a row, so BOTH axes
// take all four levels.
Patch make_patch(uint32_t ix, uint32_t iz, uint32_t salt) {
  Patch p;
  p.ix = ix;
  p.iz = iz;
  for (uint32_t n = 0; n < 16; ++n) {
    const uint32_t i = n & 3u;
    const uint32_t j = n >> 2;
    p.lvl[n] = (i + 3u * j + ix * 5u + iz * 11u + salt * 3u) & 3u;
  }
  return p;
}

// And the claim above is ASSERTED rather than trusted: a generator that
// silently went uniform again would make several cases vacuous.
void check_generator() {
  const Patch p = make_patch(7, 5, 2);
  bool rows_full = true, cols_full = true;
  for (uint32_t k = 0; k < 4; ++k) {
    uint32_t seen_row = 0, seen_col = 0;
    for (uint32_t m = 0; m < 4; ++m) {
      seen_row |= 1u << p.at(m, k);
      seen_col |= 1u << p.at(k, m);
    }
    rows_full = rows_full && (seen_row == 0xFu);
    cols_full = cols_full && (seen_col == 0xFu);
  }
  check_true(rows_full, "0 every ROW of the test grid takes all four levels");
  check_true(cols_full, "0 every COLUMN of the test grid takes all four levels");
}

// TERRAIN.TESS's own arithmetic for the level a shared edge is tessellated at.
uint32_t tess_edge(uint32_t own, uint32_t neighbour) {
  return (neighbour > own) ? neighbour : own;
}

uint32_t lane(uint8_t word, uint32_t k) { return (word >> (2 * k)) & 3u; }

// ===========================================================================
// 1. The bank stores a patch and hands back its neighbours' border rows
// ===========================================================================
void case1() {
  Rig r;
  r.reset();

  Patch p = make_patch(5, 7, 0);
  Patch q = make_patch(6, 7, 1);   // P's +x neighbour
  Patch s = make_patch(5, 8, 2);   // P's +z neighbour

  uint32_t filed0 = r.t.records_filed_o;
  uint32_t real0 = r.t.edges_real_o;
  uint32_t queries0 = r.t.queries_o;

  r.frame_begin();
  r.file_patch(p);
  check_eq(r.t.records_filed_o - filed0, 1, "1 a record completes on its sixteenth lane");
  r.file_patch(q);
  r.file_patch(s);
  check_eq(r.t.records_filed_o - filed0, 3, "1 three records filed");
  check_eq(r.t.lanes_filed_o, 48, "1 forty-eight lanes, sixteen a patch");
  r.prepare_done();

  Answer ap = r.query(5, 7);
  check_eq(ap.px, q.col_i0(), "1 P's +x edge is Q's i=0 column");
  check_eq(ap.pz, s.row_j0(), "1 P's +z edge is S's j=0 row");
  check_eq(ap.nz, 0, "1 P's -z edge falls back -- no patch filed there");
  check_eq(ap.nx, 0, "1 P's -x edge falls back -- no patch filed there");
  check_eq(ap.real, 0xA, "1 edge_real_o names exactly the two real edges");

  Answer aq = r.query(6, 7);
  check_eq(aq.nx, p.col_i3(), "1 Q's -x edge is P's i=3 column");
  check_eq(aq.real & 0x4, 0x4, "1 ... and is flagged real");

  Answer as = r.query(5, 8);
  check_eq(as.nz, p.row_j3(), "1 S's -z edge is P's j=3 row");
  check_eq(as.real & 0x1, 0x1, "1 ... and is flagged real");

  check_eq(r.t.queries_o - queries0, 3, "1 three queries counted");
  check_true(r.t.edges_real_o - real0 == 4, "1 edges_real_o counted the four real lanes");
}

// ===========================================================================
// 2. THE SYMMETRY THEOREM, by exhaustion over a 3 x 3 patch block
// ===========================================================================
// This is the case the block exists for. For every seam of the block and every
// one of its four lanes, the level TERRAIN.TESS would tessellate that lane at,
// computed from EACH side with that side's own level and the answer this block
// gave it, must be the SAME NUMBER. A crack is exactly the failure of this
// equality, so the assertion is the geometry and not a proxy for it.
void case2() {
  Rig r;
  r.reset();

  const uint32_t X0 = 4, Z0 = 9;
  std::map<std::pair<uint32_t, uint32_t>, Patch> set;
  for (uint32_t dz = 0; dz < 3; ++dz) {
    for (uint32_t dx = 0; dx < 3; ++dx) {
      set[{X0 + dx, Z0 + dz}] = make_patch(X0 + dx, Z0 + dz, dx * 3 + dz);
    }
  }

  r.frame_begin();
  for (const auto& kv : set) r.file_patch(kv.second);
  r.prepare_done();

  std::map<std::pair<uint32_t, uint32_t>, Answer> ans;
  for (const auto& kv : set) ans[kv.first] = r.query(kv.first.first, kv.first.second);

  int interior_seams = 0;
  int real_lanes = 0;
  for (const auto& kv : set) {
    uint32_t ix = kv.first.first, iz = kv.first.second;
    const Patch& a = kv.second;
    const Answer& aa = ans[kv.first];

    // the +x seam, against the patch at (ix+1, iz)
    auto it = set.find({ix + 1, iz});
    if (it != set.end()) {
      ++interior_seams;
      const Patch& b = it->second;
      const Answer& ab = ans[it->first];
      check_eq(aa.real & 0x8, 0x8, "2 the +x seam of an interior pair is real");
      check_eq(ab.real & 0x4, 0x4, "2 ... and so is the -x seam facing it");
      for (uint32_t j = 0; j < 4; ++j) {
        uint32_t from_a = tess_edge(a.at(3, j), lane(aa.px, j));
        uint32_t from_b = tess_edge(b.at(0, j), lane(ab.nx, j));
        check_eq(from_a, from_b, "2 CRACK-FREE: the two sides of an x seam agree");
        ++real_lanes;
      }
    }

    // the +z seam, against the patch at (ix, iz+1)
    it = set.find({ix, iz + 1});
    if (it != set.end()) {
      ++interior_seams;
      const Patch& b = it->second;
      const Answer& ab = ans[it->first];
      check_eq(aa.real & 0x2, 0x2, "2 the +z seam of an interior pair is real");
      check_eq(ab.real & 0x1, 0x1, "2 ... and so is the -z seam facing it");
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t from_a = tess_edge(a.at(i, 3), lane(aa.pz, i));
        uint32_t from_b = tess_edge(b.at(i, 0), lane(ab.nz, i));
        check_eq(from_a, from_b, "2 CRACK-FREE: the two sides of a z seam agree");
        ++real_lanes;
      }
    }
  }
  check_eq(interior_seams, 12, "2 a 3 x 3 block has twelve interior seams");
  check_eq(real_lanes, 48, "2 ... and forty-eight lanes across them");

  // THE POSITIVE CONTROL ON THE CHECKER ITSELF. The equality above is only
  // evidence if it can fail, and this repository has shipped a checker whose
  // two operands moved together. Compute the SAME comparison with the answer
  // this block would have given under the retained `8'h00` tie-off, and
  // require it to DISAGREE somewhere -- which is the measured statement that
  // the conservative constant is not crack-free across a patch border.
  int tieoff_disagreements = 0;
  for (const auto& kv : set) {
    uint32_t ix = kv.first.first, iz = kv.first.second;
    const Patch& a = kv.second;
    auto it = set.find({ix + 1, iz});
    if (it == set.end()) continue;
    const Patch& b = it->second;
    for (uint32_t j = 0; j < 4; ++j) {
      if (tess_edge(a.at(3, j), 0) != tess_edge(b.at(0, j), 0)) ++tieoff_disagreements;
    }
  }
  check_true(tieoff_disagreements > 0,
             "2 CONTROL: the checker can see a disagreement -- 8'h00 on both "
             "sides leaves adjacent patches at different levels disagreeing");
  std::printf("  case 2: %d of 48 x-seam lanes disagree under the 8'h00 tie-off\n",
              tieoff_disagreements);
}

// ===========================================================================
// 3. Job-order permutations give byte-identical answers
// ===========================================================================
void case3() {
  const uint32_t X0 = 2, Z0 = 3;
  std::vector<Patch> ps;
  for (uint32_t dz = 0; dz < 2; ++dz) {
    for (uint32_t dx = 0; dx < 3; ++dx) {
      ps.push_back(make_patch(X0 + dx, Z0 + dz, dx + dz * 4));
    }
  }

  // Four filing orders and two query orders. Every combination must produce
  // the same six answers.
  std::vector<std::vector<int>> file_orders = {
      {0, 1, 2, 3, 4, 5}, {5, 4, 3, 2, 1, 0}, {0, 3, 1, 4, 2, 5}, {2, 0, 5, 3, 1, 4}};
  std::vector<std::vector<int>> query_orders = {{0, 1, 2, 3, 4, 5}, {4, 2, 0, 5, 3, 1}};

  std::vector<Answer> golden;
  bool have_golden = false;

  for (const auto& fo : file_orders) {
    for (const auto& qo : query_orders) {
      Rig r;
      r.reset();
      r.frame_begin();
      // A non-uniform gap between lanes, so a scheme that depended on a
      // back-to-back stream would show here.
      int gap = 0;
      for (int k : fo) {
        r.file_patch(ps[static_cast<size_t>(k)], gap);
        gap = (gap + 1) % 3;
      }
      r.prepare_done();

      std::vector<Answer> got(ps.size());
      for (int k : qo) {
        got[static_cast<size_t>(k)] =
            r.query(ps[static_cast<size_t>(k)].ix, ps[static_cast<size_t>(k)].iz);
      }
      if (!have_golden) {
        golden = got;
        have_golden = true;
      } else {
        bool same = true;
        for (size_t k = 0; k < got.size(); ++k) same = same && (got[k] == golden[k]);
        check_true(same, "3 the answers do not depend on file or query order");
      }
    }
  }
  // And the golden itself is real, not six fallbacks agreeing.
  check_eq(golden[1].real & 0x4, 0x4, "3 the order-independent answer is a real one");
  check_eq(golden[1].nx, ps[0].col_i3(), "3 ... and carries the left neighbour's column");
}

// ===========================================================================
// 4. An absent neighbour falls back, and the fallback counter moves
// ===========================================================================
void case4() {
  Rig r;
  r.reset();

  Patch p = make_patch(20, 20, 5);
  r.frame_begin();
  r.file_patch(p);
  r.prepare_done();

  uint32_t fb0 = r.t.edges_fallback_o;
  uint32_t real0 = r.t.edges_real_o;
  Answer a = r.query(20, 20);
  check_eq(a.real, 0, "4 a patch with no filed neighbour has four fallback edges");
  check_eq(a.nz, 0, "4 -z is 8'h00");
  check_eq(a.pz, 0, "4 +z is 8'h00");
  check_eq(a.nx, 0, "4 -x is 8'h00");
  check_eq(a.px, 0, "4 +x is 8'h00");
  check_eq(r.t.edges_fallback_o - fb0, 4, "4 edges_fallback_o counted all four");
  check_eq(r.t.edges_real_o - real0, 0, "4 CONTROL: edges_real_o stayed put");
  check_eq(r.t.query_own_missing_o, 0,
           "4 CONTROL: the patch's OWN record was fine -- it is the neighbours that are absent");
}

// ===========================================================================
// 5. An incomplete record is unusable, from both directions
// ===========================================================================
void case5() {
  Rig r;
  r.reset();

  Patch p = make_patch(9, 9, 1);   // complete
  Patch q = make_patch(10, 9, 2);  // fifteen lanes only

  r.frame_begin();
  r.file_patch(p);
  for (uint32_t n = 0; n < 15; ++n) r.file_lane(q, n);
  check_eq(r.t.records_filed_o, 1, "5 only the complete patch counted as a record");
  r.prepare_done();

  uint32_t miss0 = r.t.query_own_missing_o;

  // P is complete; its +x neighbour Q is not, so that lane falls back.
  Answer ap = r.query(9, 9);
  check_eq(ap.real, 0, "5 a partly filed neighbour is not a usable decision");
  check_eq(r.t.query_own_missing_o - miss0, 0,
           "5 CONTROL: P's own record is complete, so query_own_missing_o stays put");

  // Q's own record is unusable, so ALL FOUR of its edges fall back -- which is
  // the half of the symmetry law that keeps the P/Q seam consistent.
  Answer aq = r.query(10, 9);
  check_eq(aq.real, 0, "5 a patch with an unusable own record falls back on every edge");
  check_eq(r.t.query_own_missing_o - miss0, 1, "5 query_own_missing_o fired for it");
}

// ===========================================================================
// 6. A direct-mapped collision POISONS the entry, symmetrically
// ===========================================================================
void case6() {
  Rig r;
  r.reset();

  // At IXW = 4 the index keeps ix[3:0], so 1 and 17 collide.
  Patch p = make_patch(1, 1, 1);
  Patch pc = make_patch(17, 1, 2);
  Patch nbr = make_patch(1, 2, 3);  // P's +z neighbour, on its own index

  uint32_t col0 = r.t.collisions_o;
  r.frame_begin();
  r.file_patch(p);
  r.file_patch(pc);
  r.file_patch(nbr);
  check_eq(r.t.collisions_o - col0, 1,
           "6 the collision is counted once per entry, not once per lane");
  r.prepare_done();

  Answer ap = r.query(1, 1);
  check_eq(ap.real, 0, "6 the poisoned entry's OWNER falls back on every edge");

  Answer apc = r.query(17, 1);
  check_eq(apc.real, 0, "6 ... and so does the patch that collided with it");

  // And the symmetric half: a healthy neighbour asking ABOUT the poisoned
  // entry is refused too, so the seam between them is fallback on both sides.
  Answer an = r.query(1, 2);
  check_eq(an.real & 0x1, 0, "6 a healthy patch asking about a poisoned one falls back");

  check_true(r.t.query_own_missing_o >= 2, "6 query_own_missing_o fired for both owners");
}

// ===========================================================================
// 7. NO PREVIOUS-FRAME SHORTCUT -- the sweep is the enforcement
// ===========================================================================
void case7() {
  Rig r;
  r.reset();

  Patch p = make_patch(6, 6, 1);
  Patch q = make_patch(7, 6, 2);

  r.frame_begin();
  r.file_patch(p);
  r.file_patch(q);
  r.prepare_done();

  Answer a1 = r.query(6, 6);
  check_eq(a1.px, q.col_i0(), "7 frame N answers from frame N's decisions");
  check_eq(a1.real & 0x8, 0x8, "7 ... and flags the lane real");

  // Frame N+1 files NOTHING. Every record of frame N is gone.
  uint16_t fc = r.t.frame_count_o;
  r.frame_begin();
  r.prepare_done();
  check_eq(r.t.frame_count_o, fc + 1, "7 the frame counter advanced");

  Answer a2 = r.query(6, 6);
  check_eq(a2.real, 0, "7 NO PREVIOUS-FRAME SHORTCUT: last frame's record is gone");
  check_eq(a2.px, 0, "7 ... and the lane reads the conservative 8'h00");

  // Refiling in frame N+1 restores the real answer, so the sweep is a sweep
  // and not a permanent disable.
  r.frame_begin();
  r.file_patch(p);
  r.file_patch(q);
  r.prepare_done();
  Answer a3 = r.query(6, 6);
  check_eq(a3.px, q.col_i0(), "7 CONTROL: refiling this frame restores the real answer");
}

// ===========================================================================
// 8. Phase gating, both ways, with controls
// ===========================================================================
void case8() {
  Rig r;
  r.reset();

  Patch p = make_patch(3, 3, 1);

  // A query during PREPARE is refused and counted.
  r.frame_begin();
  uint32_t qoop0 = r.t.query_out_of_phase_o;
  uint32_t foop0 = r.t.file_out_of_phase_o;
  r.quiet();
  r.t.q_valid_i = 1;
  r.t.q_ix_i = 3;
  r.t.q_iz_i = 3;
  r.t.eval();
  check_eq(r.t.q_ready_o, 0, "8 q_ready_o is low outside EMIT");
  zhao::tick(r.t);
  r.t.eval();
  check_eq(r.t.query_out_of_phase_o - qoop0, 1, "8 query_out_of_phase_o fired");
  check_eq(r.t.file_out_of_phase_o - foop0, 0, "8 CONTROL: the file counter stayed put");

  // A file beat during PREPARE is NOT counted -- the control for the above.
  r.file_patch(p);
  check_eq(r.t.file_out_of_phase_o - foop0, 0,
           "8 CONTROL: sixteen in-phase file beats moved no out-of-phase counter");

  // A file beat during EMIT is refused and counted.
  r.prepare_done();
  r.quiet();
  r.t.f_valid_i = 1;
  r.t.f_ix_i = 3;
  r.t.f_iz_i = 3;
  r.t.f_ox_i = 0;
  r.t.f_oz_i = 0;
  r.t.f_level_i = 3;
  r.t.eval();
  check_eq(r.t.f_ready_o, 0, "8 f_ready_o is low outside PREPARE");
  zhao::tick(r.t);
  r.t.eval();
  check_eq(r.t.file_out_of_phase_o - foop0, 1, "8 file_out_of_phase_o fired");

  // ... and the refused beat did not reach the bank: the record is unchanged.
  r.quiet();
  r.t.eval();
  Answer a = r.query(3, 3);
  check_eq(a.real, 0, "8 the refused beat changed nothing that a query can see");
  check_eq(r.t.query_out_of_phase_o - qoop0, 1,
           "8 CONTROL: an in-phase query moved no out-of-phase counter");
}

// ===========================================================================
// 9. The answer is HELD, and the walk cannot be interrupted
// ===========================================================================
void case9() {
  Rig r;
  r.reset();

  Patch p = make_patch(8, 4, 1);
  Patch q = make_patch(9, 4, 2);
  r.frame_begin();
  r.file_patch(p);
  r.file_patch(q);
  r.prepare_done();

  Answer a = r.query(8, 4);
  check_eq(a.px, q.col_i0(), "9 the answer is real to begin with");

  // TERRAIN.LOD.md: the governor's targets "must be held stable across a patch
  // job". A patch job is ~784 clocks; hold for more than that.
  for (int i = 0; i < 900; ++i) {
    r.quiet();
    r.t.eval();
    check_true(r.t.edge_px_o == a.px && r.t.edge_nx_o == a.nx && r.t.edge_nz_o == a.nz &&
                   r.t.edge_pz_o == a.pz && r.t.edge_real_o == a.real,
               "9 the answer is held between queries");
    if (failures) break;
    zhao::tick(r.t);
    r.t.eval();
  }

  // q_ready_o is low for the whole walk, so a second query cannot be injected
  // into one in flight.
  r.quiet();
  r.t.q_valid_i = 1;
  r.t.q_ix_i = 9;
  r.t.q_iz_i = 4;
  r.t.eval();
  check_eq(r.t.q_ready_o, 1, "9 q_ready_o is high at rest in EMIT");
  zhao::tick(r.t);
  r.t.eval();
  int low = 0;
  for (int i = 0; i < 6; ++i) {
    r.quiet();
    r.t.q_valid_i = 1;
    r.t.q_ix_i = 8;
    r.t.q_iz_i = 4;
    r.t.eval();
    if (!r.t.q_ready_o) ++low;
    zhao::tick(r.t);
    r.t.eval();
  }
  check_true(low >= 5, "9 q_ready_o stays low for the whole five-record walk");
}

// ===========================================================================
// 10. The underside replay is consumed, not filed
// ===========================================================================
void case10() {
  Rig r;
  r.reset();

  Patch p = make_patch(11, 12, 3);
  Patch q = make_patch(12, 12, 4);

  r.frame_begin();
  // Emit each subpatch followed by its underside, which is TERRAIN.LOD's dual
  // page order exactly.
  for (uint32_t n = 0; n < 16; ++n) {
    r.file_lane(p, n, 0);
    r.file_lane(p, n, 1);
  }
  check_eq(r.t.lanes_filed_o, 16, "10 thirty-two beats, sixteen decisions");
  check_eq(r.t.records_filed_o, 1, "10 ... and the record still completed");
  r.file_patch(q);
  r.prepare_done();

  Answer a = r.query(11, 12);
  check_eq(a.px, q.col_i0(), "10 the dual page's record answers like any other");
}

// ===========================================================================
// 11. The counter invariant
// ===========================================================================
void case11() {
  Rig r;
  r.reset();

  // A mixed set: two adjacent, one isolated, one incomplete.
  Patch a = make_patch(14, 1, 1);
  Patch b = make_patch(15, 1, 2);
  Patch c = make_patch(2, 11, 3);
  Patch d = make_patch(3, 11, 4);

  r.frame_begin();
  r.file_patch(a);
  r.file_patch(b);
  r.file_patch(c);
  for (uint32_t n = 0; n < 10; ++n) r.file_lane(d, n);
  r.prepare_done();

  (void)r.query(14, 1);
  (void)r.query(15, 1);
  (void)r.query(2, 11);
  (void)r.query(3, 11);
  (void)r.query(60, 60);  // nothing filed anywhere near

  check_eq(r.t.queries_o, 5, "11 five queries");
  check_eq(r.t.edges_real_o + r.t.edges_fallback_o, 4u * r.t.queries_o,
           "11 INVARIANT: every query answers exactly four lanes");
  check_true(r.t.edges_real_o > 0, "11 ... and some of them were real");
  check_true(r.t.edges_fallback_o > 0, "11 ... and some of them were the fallback");
  check_eq(r.t.query_own_missing_o, 2,
           "11 the incomplete patch and the unfiled one both reported a missing own record");
}


// ===========================================================================
// 12. THE PUBLISHED ANSWER DOES NOT MOVE UNTIL THE NEXT WALK FINISHES
// ===========================================================================
// Case 9 holds the answer BETWEEN queries. This one holds it DURING the next
// one, which is a different claim and the one that was wrong.
//
// The block's first version accumulated into the published registers and
// cleared them on the query ACCEPT, so `edge_*` dropped to 8'h00 for the seven
// clocks of the walk. ALL ELEVEN CASES ABOVE PASSED, because each queries and
// then reads -- none of them looks at the port while a walk is in flight. A
// caller that started patch N+1's query while TERRAIN.LOD was still emitting
// patch N's descriptors would have fed that block the fallback MID-PATCH,
// against TERRAIN.LOD.md's 'must be held stable across a patch job'. That is a
// crack whose cause is a handshake, with every counter agreeing.
//
// This asserts the CORRECT behaviour -- the answer holds -- rather than the
// defect. The walk registers are separate now and the publish is one edge.
void case12() {
  Rig r;
  r.reset();

  Patch a = make_patch(12, 3, 1);
  Patch b = make_patch(13, 3, 2);   // A's +x neighbour
  Patch c = make_patch(2, 14, 3);   // isolated, so its answer is all fallback

  r.frame_begin();
  r.file_patch(a);
  r.file_patch(b);
  r.file_patch(c);
  r.prepare_done();

  const Answer first = r.query(12, 3);
  check_eq(first.px, b.col_i0(), "12 the first answer is real");
  check_eq(first.real, 0x8, "12 ... on exactly the +x edge");

  // Now start a query for the ISOLATED patch, whose answer is all fallback and
  // therefore differs from the held one in every field. Walk it cycle by cycle
  // and require the port to carry the FIRST answer until `q_done_o`.
  r.quiet();
  r.t.q_valid_i = 1;
  r.t.q_ix_i = 2;
  r.t.q_iz_i = 14;
  r.t.eval();
  check_eq(r.t.q_ready_o, 1, "12 the second query is accepted");
  zhao::tick(r.t);
  r.t.eval();

  int held_cycles = 0;
  bool done_seen = false;
  for (int i = 0; i < 32 && !done_seen; ++i) {
    r.quiet();
    r.t.eval();
    // BEFORE the edge that publishes, the port must still read the first
    // answer. This is what a downstream TERRAIN.LOD would sample.
    check_true(r.t.edge_px_o == first.px && r.t.edge_nz_o == first.nz &&
                   r.t.edge_pz_o == first.pz && r.t.edge_nx_o == first.nx &&
                   r.t.edge_real_o == first.real,
               "12 the published answer is HELD for the whole next walk");
    ++held_cycles;
    if (failures) break;
    zhao::tick(r.t);
    r.t.eval();
    if (r.t.q_done_o) done_seen = true;
  }
  check_true(done_seen, "12 the second walk completed");
  // ... and it really was a WALK, not a one-cycle answer: if the port had been
  // held for zero or one cycle the check above would have been vacuous.
  check_true(held_cycles >= 5,
             "12 the hold was measured across the whole five-record walk");

  // And once it publishes, it publishes the NEW answer -- the hold is a hold,
  // not a freeze.
  check_eq(r.t.edge_real_o, 0, "12 the new answer lands, all four fallback");
  check_eq(r.t.edge_px_o, 0, "12 ... and the +x word with it");
  check_true(first.px != 0,
             "12 CONTROL: the two answers really do differ, so the hold was "
             "testable at all");
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  check_generator();
  case1();
  case2();
  case3();
  case4();
  case5();
  case6();
  case7();
  case8();
  case9();
  case10();
  case11();
  case12();

  std::printf("terrain_edgerecon_directed: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
