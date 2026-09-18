// vertex_arena_bitmap_open_fill_race.cpp — the VALID_BITMAP update law, stated
// so a rewrite of the bitmap process cannot change it quietly.
//
// WHY THIS SUITE EXISTS. On 2026-09-18 the bitmap's open-clear was rewritten:
// it had been DEPTH runtime-variable bit-selects into one packed register
// (`valid_q[open_arena_i * DEPTH + vb]`), which the whole-console census
// measured as 1,480,718 combinational ALUTs — 92.2% of the design. It is now
// ARENAS*DEPTH per-bit flops whose two update terms are equalities against
// COMPILE-TIME CONSTANTS.
//
// Nothing about the behaviour was supposed to move, and the existing suites
// agreed — `geom_wcache_directed` (73 checks) and `geom_wcache_random` (8,726
// lookups) were byte-identical before and after. But neither of them can
// actually SEE the property the rewrite is most likely to get backwards,
// because neither ever drives `open_i` and `fill_valid_i` on the same edge:
//
//     v' = set OR (v AND NOT clear)
//
// The SET wins. In the old process that was "last assignment wins" — an
// ordering nobody wrote down, produced by the two statements' order in one
// always_ff. In the new one it is `set_this` being tested before `clear_this`.
// An incautious rewrite tests the clear first, and then a fill accepted on the
// same edge as an open of its own arena is SILENTLY LOST: the payload lands in
// the memory, the valid bit does not, and the slot misses for the rest of that
// lifetime. Every existing test passes. That is the gap this file closes.
//
// THE ORACLE CANNOT BE THE INSTRUMENT HERE. zref::geom::VertexArena is a
// sequential model — `open()` then `fill()` — so it has no way to express two
// commands on one edge. The checks below therefore state the RTL's required
// behaviour directly, which is also what keeps them honest under repair:
// CLAUDE.md's rule is to assert the CORRECT behaviour, never the defect.
//
// NON-POWER-OF-TWO BY CONSTRUCTION. ARENAS = 3 and DEPTH = 11 are both
// deliberately not powers of two, and that is load-bearing twice over:
//
//   * The bank partition. The new code derives each bit's arena at elaboration
//     as `k / DEPTH`. At a power-of-two depth a padded/concatenated partition
//     (`{arena, index}`) and the linear one (`arena*DEPTH + index`) coincide
//     bit-for-bit, so a wrong partition is INVISIBLE — that is exactly how the
//     addressing defect repaired on 2026-09-09 survived a directed suite at
//     DEPTH=16 and a formal shape at DEPTH=4. Case C below sits astride the
//     arena 0 / arena 1 seam, where the two disagree.
//   * The range checks. ARENAS = 3 makes arena 3 EXPRESSIBLE and illegal, so
//     `open_bad_arena` and `fill_bad_arena` can both be fired rather than
//     assumed.
//
// Registered at ARENAS=3, DEPTH=11, PAYLOAD_W=64, GEN_W=8, VALID_MODE=0.

#include <cstdint>
#include <cstdio>

#include "Vzhao_vertex_arena.h"
#include "zhao_sim.hpp"

using zhao::check;

namespace {

constexpr int kArenas = 3;   // not a power of two: arena 3 is expressible
constexpr int kDepth = 11;   // not a power of two: linear != concat addressing
constexpr int kBits = kArenas * kDepth;  // 33 valid bits in one packed vector

// A payload that identifies its slot exactly, so a bit landing in the wrong
// place reports WHICH wrong place rather than just "not what I wanted".
uint64_t mark(int arena, int index) {
  return 0xA5A5'0000'0000'0000ull | (static_cast<uint64_t>(arena) << 32) |
         static_cast<uint64_t>(index);
}

struct Dut {
  Vzhao_vertex_arena* v;
  uint32_t gen[kArenas] = {0, 0, 0};

  explicit Dut(Vzhao_vertex_arena* d) : v(d) {}

  void tick() {
    v->clk = 0;
    v->eval();
    v->clk = 1;
    v->eval();
  }

  void idle() {
    v->open_i = 0;
    v->org_we_i = 0;
    v->fill_valid_i = 0;
    v->seal_i = 0;
    v->look_valid_i = 0;
  }

  void reset() {
    idle();
    v->rst_n = 0;
    tick();
    tick();
    v->rst_n = 1;
    tick();
  }

  void open(int arena) {
    idle();
    v->open_i = 1;
    v->open_arena_i = arena;
    tick();
    idle();
    if (arena < kArenas) gen[arena] = (gen[arena] + 1) & 0xFF;
  }

  void seal(int arena) {
    idle();
    v->seal_i = 1;
    v->seal_arena_i = arena;
    tick();
    idle();
  }

  void fill(int arena, int index, uint64_t payload) {
    idle();
    v->fill_valid_i = 1;
    v->fill_arena_i = arena;
    v->fill_index_i = index;
    v->fill_payload_i = payload;
    tick();
    idle();
  }

  // OPEN AND FILL ON THE SAME EDGE. The whole point of the file: two commands,
  // one clock, and the rewrite's priority is what decides the outcome.
  void open_and_fill(int open_arena, int fill_arena, int fill_index,
                     uint64_t payload) {
    idle();
    v->open_i = 1;
    v->open_arena_i = open_arena;
    v->fill_valid_i = 1;
    v->fill_arena_i = fill_arena;
    v->fill_index_i = fill_index;
    v->fill_payload_i = payload;
    tick();
    idle();
    if (open_arena < kArenas) gen[open_arena] = (gen[open_arena] + 1) & 0xFF;
  }

  // Drive one lookup and return the reply. `arena`/`index` may be out of range
  // on purpose -- both ports are one bit wider than their address.
  struct Reply {
    int valid, hit, refuse;
    uint64_t payload;
  };

  Reply look(int arena, uint32_t generation, int index) {
    idle();
    v->look_valid_i = 1;
    v->look_arena_i = arena;
    v->look_gen_i = generation & 0xFF;
    v->look_index_i = index;
    tick();
    idle();
    v->eval();
    return Reply{static_cast<int>(v->rep_valid_o), static_cast<int>(v->rep_hit_o),
                 static_cast<int>(v->rep_refuse_o),
                 static_cast<uint64_t>(v->rep_payload_o)};
  }

  void expect_hit(int arena, int index, uint64_t payload, const char* what) {
    const Reply r = look(arena, gen[arena], index);
    check(r.hit == 1 && r.refuse == 0, what, 1, static_cast<uint64_t>(r.hit));
    if (r.hit) check(r.payload == payload, what, payload, r.payload);
  }

  void expect_miss(int arena, int index, const char* what) {
    const Reply r = look(arena, gen[arena], index);
    check(r.valid == 1 && r.hit == 0 && r.refuse == 0, what, 0,
          static_cast<uint64_t>(r.hit) | (static_cast<uint64_t>(r.refuse) << 1));
  }

  // Fill every slot of an arena, in a deliberately SCATTERED order. Bitmap mode
  // places no ordering requirement on the producer (that is the whole reason it
  // exists beside DENSE_SEAL), and a rewrite that quietly introduced one would
  // otherwise pass a suite that always fills 0,1,2,...
  void fill_all_scattered(int arena) {
    for (int i = kDepth - 1; i >= 0; i -= 2) fill(arena, i, mark(arena, i));
    for (int i = kDepth - 2; i >= 0; i -= 2) fill(arena, i, mark(arena, i));
  }
};

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Verilated::traceEverOn(false);
  auto* top = new Vzhao_vertex_arena;
  Dut d(top);
  d.reset();

  // ==========================================================================
  // A. RESET WINS OVER BOTH, and it clears EVERY bit of the vector.
  // ==========================================================================
  // Held first because everything after it assumes a known starting state, and
  // because `valid_q <= '0` became ARENAS*DEPTH separate `if (!rst_n)` arms --
  // a rewrite that dropped the reset from some of them would leave those slots
  // hitting out of an unwritten memory, and only a sweep of the whole vector
  // sees it.
  for (int a = 0; a < kArenas; ++a) d.seal(a);
  for (int a = 0; a < kArenas; ++a) {
    for (int i = 0; i < kDepth; ++i) {
      char what[96];
      std::snprintf(what, sizeof(what), "A: reset cleared arena %d slot %d", a, i);
      d.expect_miss(a, i, what);
    }
  }

  // ==========================================================================
  // B. THE RACE. A fill accepted on the same edge as an OPEN OF ITS OWN ARENA
  //    keeps its bit; every other bit of that arena is dropped.
  // ==========================================================================
  // This is the priority an incautious rewrite gets backwards, and the reason
  // the new process tests `set_this` before `clear_this`.
  for (int a = 0; a < kArenas; ++a) {
    d.open(a);
    d.fill_all_scattered(a);
    d.seal(a);
  }
  for (int a = 0; a < kArenas; ++a) {
    for (int i = 0; i < kDepth; ++i) {
      char what[96];
      std::snprintf(what, sizeof(what), "B: arena %d slot %d hits before the race", a, i);
      d.expect_hit(a, i, mark(a, i), what);
    }
  }

  constexpr int kRaceArena = 1;
  constexpr int kRaceSlot = 4;
  const uint64_t race_payload = 0x00FF'00FF'0BAD'0001ull;

  // B0. FIRST, THE RACE AGAINST A *SEALED* ARENA, which is a different law and
  //     was found by getting this file wrong on its first run.
  //
  // `fill_sealed` reads `sealed_q` combinationally, i.e. the value BEFORE this
  // edge. An open unseals, but not until the edge completes -- so a fill racing
  // the open of an arena that is still sealed is DROPPED, and the open's clear
  // then takes the bit like any other. That is true of both the old process and
  // the new one (neither touches `fill_ok`), and it is worth pinning precisely
  // because it is the case that makes the interesting one below hard to reach:
  // set-versus-clear can only be observed on an arena that is already OPEN.
  d.open_and_fill(kRaceArena, kRaceArena, kRaceSlot, 0xDEAD'DEAD'DEAD'DEADull);
  d.seal(kRaceArena);
  d.expect_miss(kRaceArena, kRaceSlot,
                "B0: a fill racing the open of a still-SEALED arena is dropped");
  top->eval();
  check(top->arena_overflow_o == 1,
        "B0: and that drop is sticky on overflow like any other", 1, top->arena_overflow_o);

  // B1. NOW THE ONE THAT MATTERS. Reopen and refill so the arena is UNSEALED
  //     and populated, then race an open of it against a fill to one of its
  //     own slots. `fill_ok` is satisfied, the clear covers the whole bank, and
  //     the SET must win for its own bit.
  d.open(kRaceArena);
  d.fill_all_scattered(kRaceArena);
  d.open_and_fill(kRaceArena, kRaceArena, kRaceSlot, race_payload);
  d.seal(kRaceArena);

  // The raced slot is the ONE survivor, and it carries the NEW payload -- not
  // the one that lived there before the open.
  d.expect_hit(kRaceArena, kRaceSlot, race_payload,
               "B1: the raced fill WINS its own bit against the same edge's open");
  for (int i = 0; i < kDepth; ++i) {
    if (i == kRaceSlot) continue;
    char what[112];
    std::snprintf(what, sizeof(what),
                  "B: and the open still cleared arena %d slot %d", kRaceArena, i);
    d.expect_miss(kRaceArena, i, what);
  }
  // THE CLEAR IS PER-BANK, so the other arenas are untouched by all of this.
  for (int a = 0; a < kArenas; ++a) {
    if (a == kRaceArena) continue;
    for (int i = 0; i < kDepth; ++i) {
      char what[112];
      std::snprintf(what, sizeof(what),
                    "B: arena %d slot %d survived the open of arena %d", a, i, kRaceArena);
      d.expect_hit(a, i, mark(a, i), what);
    }
  }

  // ==========================================================================
  // C. THE BANK SEAM, at a depth where a padded partition would disagree.
  // ==========================================================================
  // arena 0 slot 10 is bit 10; arena 1 slot 0 is bit 11. They are ADJACENT in
  // the packed vector and belong to different banks. Opening arena 1 must take
  // bit 11 and leave bit 10 standing. At a power-of-two depth these two bits
  // are not adjacent and the case proves nothing; at DEPTH = 11 a `BANK`
  // computed by masking instead of dividing lands on the wrong side of it.
  d.open(0);
  d.fill(0, kDepth - 1, mark(0, kDepth - 1));  // bit 10, the last of bank 0
  d.seal(0);
  d.open(1);
  d.fill(1, 0, mark(1, 0));                    // bit 11, the first of bank 1
  d.seal(1);
  d.expect_hit(0, kDepth - 1, mark(0, kDepth - 1), "C: bank 0's last slot is filled");
  d.expect_hit(1, 0, mark(1, 0), "C: bank 1's first slot is filled");

  d.open(1);   // drops bit 11 and everything above it in bank 1, nothing below
  d.seal(1);
  d.expect_miss(1, 0, "C: opening bank 1 dropped its first slot (bit 11)");
  d.expect_hit(0, kDepth - 1, mark(0, kDepth - 1),
               "C: and left bank 0's last slot (bit 10) standing across the seam");

  // ==========================================================================
  // D. OUT-OF-RANGE CONTROLS, all reachable because the ports are a bit wide.
  // ==========================================================================
  // D1. An open of a NONEXISTENT arena clears nothing at all. `open_bad_arena`
  //     gates the clear; without that gate `open_arena_i * DEPTH` would index
  //     past the vector, and in the rewritten form the constant comparison
  //     `open_arena_i == ARENA_W'(BANK)` simply never matches -- which is the
  //     same answer by a different route, and worth a check rather than an
  //     argument.
  d.open(2);
  d.fill_all_scattered(2);
  d.seal(2);
  const uint32_t gen2_before = d.gen[2];
  d.open(kArenas);          // arena == ARENAS: illegal, expressible
  d.open(kArenas + 4);      // and well past it
  check(d.gen[2] == gen2_before, "D1: the illegal opens did not bump a real generation",
        gen2_before, d.gen[2]);
  for (int i = 0; i < kDepth; ++i) {
    char what[112];
    std::snprintf(what, sizeof(what), "D1: an illegal open cleared nothing (arena 2 slot %d)", i);
    d.expect_hit(2, i, mark(2, i), what);
  }

  // D2. An ILLEGAL open racing a LEGAL fill: the fill lands, nothing is cleared.
  //     The arena must be left OPEN for the fill to be acceptable at all -- see
  //     B0 -- so the sequence is open, verify clear, then race without sealing
  //     in between.
  d.open(0);
  d.seal(0);
  d.expect_miss(0, 5, "D2: arena 0 slot 5 starts clear");
  d.open(0);   // unseal, so fill_ok can be satisfied on the raced edge
  d.open_and_fill(kArenas, 0, 5, 0x1234'5678'9ABC'DEF0ull);
  d.seal(0);
  d.expect_hit(0, 5, 0x1234'5678'9ABC'DEF0ull,
               "D2: a fill racing an ILLEGAL open still lands");

  // D3. Out-of-range FILLS are dropped and never set a bit. An index of DEPTH
  //     is the wrap the contract's leading law forbids: at DEPTH = 11 it would
  //     land on bit 11, which is arena 1 slot 0 -- a real slot of another
  //     vertex. A power-of-two depth hides this too.
  d.open(1);
  d.seal(1);
  d.expect_miss(1, 0, "D3: arena 1 slot 0 starts clear");
  d.open(0);
  d.fill(0, kDepth, 0xBAD0'0000'0000'0001ull);      // index == DEPTH
  d.fill(0, kDepth + 3, 0xBAD0'0000'0000'0002ull);  // and past it
  d.fill(kArenas, 0, 0xBAD0'0000'0000'0003ull);     // arena == ARENAS
  d.seal(0);
  d.expect_miss(1, 0, "D3: a fill at index == DEPTH did not wrap into arena 1 slot 0");
  d.expect_miss(0, 0, "D3: nor into arena 0 slot 0");
  top->eval();
  check(top->arena_overflow_o == 1, "D3: and the dropped fills are sticky on overflow", 1,
        top->arena_overflow_o);

  // D4. A fill to a SEALED arena is dropped -- the mode-blind acceptance term
  //     still decides, and the rewrite did not take it over.
  d.open(2);
  d.fill(2, 7, mark(2, 7));
  d.seal(2);
  d.expect_hit(2, 7, mark(2, 7), "D4: the pre-seal fill hit");
  d.fill(2, 8, 0xFFFF'FFFF'FFFF'FFFFull);  // arena 2 is sealed now
  d.expect_miss(2, 8, "D4: a fill after seal set no bit");

  // ==========================================================================
  // E. EVERY BIT OF THE VECTOR, individually addressed.
  // ==========================================================================
  // 33 bits, each set on its own and read back, so a partition error anywhere
  // in the generate loop is named by slot rather than summarised. This is the
  // sweep the census's own lesson asks for: a per-bit rewrite deserves a
  // per-bit check, not a sample.
  for (int a = 0; a < kArenas; ++a) {
    d.open(a);
    d.seal(a);
  }
  int swept = 0;
  for (int a = 0; a < kArenas; ++a) {
    for (int i = 0; i < kDepth; ++i) {
      d.open(a);
      d.fill(a, i, mark(a, i));
      d.seal(a);
      char what[96];
      std::snprintf(what, sizeof(what), "E: bit %d (arena %d slot %d) sets alone",
                    a * kDepth + i, a, i);
      d.expect_hit(a, i, mark(a, i), what);
      // and it is the ONLY bit of its bank that is set
      for (int j = 0; j < kDepth; ++j) {
        if (j == i) continue;
        char w2[112];
        std::snprintf(w2, sizeof(w2), "E: bit %d set, arena %d slot %d stayed clear",
                      a * kDepth + i, a, j);
        d.expect_miss(a, j, w2);
      }
      ++swept;
    }
  }
  check(swept == kBits, "E: every bit of the vector was swept", kBits, swept);

  top->final();
  delete top;
  return zhao::report_and_exit("vertex_arena_bitmap_open_fill_race");
}
