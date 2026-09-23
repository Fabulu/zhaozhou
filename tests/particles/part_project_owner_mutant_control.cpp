// part_project_owner_mutant_control.cpp -- INVERTED POLARITY. This passes when
// `owner_unroutable_o` FIRES.
//
// WHY IT EXISTS
// -------------
// `zhao_part_project`'s client-A rider carries a TWO-BIT owner field. Until
// 2026-09-23 three of its four codes were routed and the fourth was unclaimed,
// so `part_project_directed` could fire `owner_unroutable_o` by forcing that
// code onto a returning rider -- legal stimulus, at a port the bench owns.
//
// The fourth request arm claims 2'd3. After it the field is FULL, every code
// routes, and NO INPUT TO THE BLOCK CAN MOVE THAT COUNTER any more. Its zero
// stopped being a measurement and became a tautology, which is precisely the
// state CLAUDE.md says needs a committed mutant:
//
//     "wq_overflow_o watches for a queue holding more entries than it owns.
//      That state is unreachable while the full-guard is correct, so no legal
//      input can move the counter, and 'it can fire' stays an argument
//      forever."
//
// So `tests/mutants/zhao_part_project_owner_mutant.sv` is the same block with
// the fourth RESULT arm removed and the fourth code dropped from the
// detector's exclusion list -- the block as it would be if somebody had added
// the request arm and forgotten the demux. A result carrying OWNER_LOD is then
// routed nowhere, and the counter is what says so.
//
// THIS IS EVIDENCE ABOUT THE INSTRUMENT, NOT ABOUT THE DESIGN. The production
// assertion lives in `part_project_directed` and is the opposite one: all four
// codes are DELIVERED and the counter stays put.
//
// THE NEGATIVE CONTROL IS THE PRODUCTION TEST. If this driver were pointed at
// production it could not pass, because production routes 2'd3 -- and
// `part_project_directed`'s G2 asserts exactly that, from the same stimulus.
// The two together are the fired-and-quiet pair; either alone is not evidence.

#include <cstdint>
#include <cstdio>

#include "Vzhao_part_project_owner_mutant.h"
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

// The owner field sits in the TOP OWNER_W bits of the 17-bit rider.
constexpr int kPayW = 17;
constexpr int kOwnerW = 2;
constexpr uint32_t kOwnerGeom = 0u;
constexpr uint32_t kOwnerLod = 3u;

uint32_t with_owner(uint32_t low, uint32_t owner) {
  return (low & ((1u << (kPayW - kOwnerW)) - 1u)) |
         (owner << (kPayW - kOwnerW));
}

void tick(Vzhao_part_project_owner_mutant& d) { zhao::tick(d); }

void reset(Vzhao_part_project_owner_mutant& d) {
  d.rst_n = 0;
  d.g_valid_i = 0;
  d.p_valid_i = 0;
  d.f_valid_i = 0;
  d.l_valid_i = 0;
  d.a_ready_i = 1;
  d.a_valid_i = 0;
  d.q_ready_i = 1;
  d.lad_ready_i = 1;
  d.rng_valid_i = 0;
  d.cfg_view_i = 0;
  for (int i = 0; i < 8; ++i) tick(d);
  d.rst_n = 1;
  for (int i = 0; i < 4; ++i) tick(d);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_part_project_owner_mutant d;
  reset(d);

  check(d.owner_unroutable_o == 0, "the counter starts silent", 0,
        d.owner_unroutable_o);

  // Hand back a client-A RESULT owned by GEOM.LODSTATE. Production routes it on
  // `rl_*`; this mutant routes it nowhere.
  for (int beat = 0; beat < 4; ++beat) {
    d.a_valid_i = 1;
    d.a_x_i = 0x100 + beat;
    d.a_y_i = 0x200 + beat;
    d.a_d_i = 0x00010000;
    d.a_w_i = 0x00020000;
    d.a_behind_i = 0;
    d.a_profile_i = 0;
    d.a_payload_i = with_owner(0x0041u, kOwnerLod);
    d.eval();
    check(d.rl_valid_o == 0,
          "the mutant's fourth demux arm is dead -- that IS the mutation", 0,
          d.rl_valid_o);
    tick(d);
  }
  d.a_valid_i = 0;
  for (int i = 0; i < 8; ++i) tick(d);

  check(d.owner_unroutable_o >= 4,
        "owner_unroutable_o FIRED once per unrouted result -- the counter is "
        "not deaf",
        4, d.owner_unroutable_o);
  check(d.h_valid_o == 0, "the unrouted result did NOT leak onto geometry", 0,
        d.h_valid_o);
  check(d.rf_valid_o == 0, "nor onto the forge arm", 0, d.rf_valid_o);
  check(d.geom_tag_collision_o == 0,
        "and the INGRESS detector stayed quiet -- a different fault, on a "
        "different port",
        0, d.geom_tag_collision_o);

  // A well-owned result still routes in the mutant, so the fire above is about
  // the missing arm and not about the block being broken outright.
  const uint32_t before = d.owner_unroutable_o;
  d.a_valid_i = 1;
  d.a_payload_i = with_owner(0x0041u, kOwnerGeom);
  d.eval();
  check(d.h_valid_o == 1, "a geometry-owned result still routes in the mutant",
        1, d.h_valid_o);
  tick(d);
  d.a_valid_i = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  check(d.owner_unroutable_o == before,
        "and it did not move the counter", before, d.owner_unroutable_o);

  std::printf("part_project_owner_mutant_control: %d check(s), %d failure(s) "
              "-- owner_unroutable_o=%u (MUST be non-zero)\n",
              g_checks, g_failed, d.owner_unroutable_o);
  std::fflush(stdout);
  zhao::exit_hard(g_failed == 0 ? 0 : 1);
}
