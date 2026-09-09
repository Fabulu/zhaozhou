// proj_service_rowmux_smoke.cpp — the shared projector's arbiter under the
// row sequencer's backpressure (ROWS_PER_PASS=1).
//
// What is under test is exactly the service's NEW three lines: `take_* =
// en_i && grant_* && core_ready`, and the round-robin/contended update gated
// on `core_ready`. Projection arithmetic is NOT re-proven here — that chain
// is proj_rowmux_directed (RPP=1 == RPP=3) plus the oracle suites (RPP=3 ==
// zref). This smoke asserts, with both clients saturated:
//   * every vertex comes back to ITS OWN client, in order, payload intact
//     (the rider-id routing under II=3 gating);
//   * grants split exactly evenly (round-robin fairness survives the gating);
//   * the aggregate acceptance rate is one per three en-cycles;
//   * contended_o counts contended GRANTS, not contended waits — at II=3 a
//     both-asking cycle where the core is mid-vertex must NOT tick it;
//   * POSITIVE CONTROL: with only client A offering, A gets every grant and
//     contended_o stays zero — which also proves the counter CAN move in the
//     dual-load run, because there it must equal the grant total.

#include "Vtb_proj_service_rowmux.h"
#include "verilated.h"

#include <cstdint>
#include <cstdio>
#include <vector>

double sc_time_stamp() { return 0; }

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond, ...)                                   \
  do {                                                     \
    ++g_checks;                                            \
    if (!(cond)) {                                         \
      ++g_fails;                                           \
      std::printf("FAIL %s:%d: ", __FILE__, __LINE__);     \
      std::printf(__VA_ARGS__);                            \
      std::printf("\n");                                   \
    }                                                      \
  } while (0)

static void settle0(Vtb_proj_service_rowmux* tb) {
  tb->clk = 0;
  tb->eval();
}
static void edge(Vtb_proj_service_rowmux* tb) {
  tb->clk = 1;
  tb->eval();
}

static void write_cfg(Vtb_proj_service_rowmux* tb, bool view, uint32_t addr,
                      uint32_t data) {
  tb->cfg_we_i = 1;
  tb->cfg_view_i = view;
  tb->cfg_addr_i = addr;
  tb->cfg_data_i = data;
  settle0(tb);
  edge(tb);
  tb->cfg_we_i = 0;
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* tb = new Vtb_proj_service_rowmux;

  // reset
  tb->rst_n = 0;
  tb->en_i = 1;
  tb->a_valid_i = 0;
  tb->b_valid_i = 0;
  tb->cfg_we_i = 0;
  for (int i = 0; i < 4; ++i) {
    settle0(tb);
    edge(tb);
  }
  tb->rst_n = 1;
  settle0(tb);
  edge(tb);

  // a simple projective matrix on both views: identity 3x3 scaled, w = z + 1
  for (uint32_t v = 0; v < 2; ++v) {
    for (uint32_t k = 0; k < 16; ++k) write_cfg(tb, v != 0, k, 0);
    write_cfg(tb, v != 0, 0, 0x00010000u);
    write_cfg(tb, v != 0, 5, 0x00010000u);
    write_cfg(tb, v != 0, 14, 0x00010000u);  // w row: z
    write_cfg(tb, v != 0, 15, 0x00010000u);  // + 1.0
    write_cfg(tb, v != 0, 16, (16u << 16) | 8u);
    write_cfg(tb, v != 0, 17, (240u << 16) | 320u);
  }

  // ---- dual saturation: 60 vertices per client ----------------------------
  const int kPer = 60;
  int a_sent = 0, b_sent = 0;
  std::vector<uint16_t> a_got;
  std::vector<uint64_t> b_got;
  uint64_t cyc = 0, first_accept = 0, last_accept = 0;
  int accepts = 0;
  const uint64_t kTimeout = 20000;

  while ((static_cast<int>(a_got.size()) < kPer ||
          static_cast<int>(b_got.size()) < kPer) &&
         cyc < kTimeout) {
    tb->a_valid_i = (a_sent < kPer);
    tb->a_vx_i = static_cast<uint32_t>(a_sent) << 16;
    tb->a_vy_i = 0x00020000u;
    tb->a_vz_i = 0x00010000u;
    tb->a_view_i = 0;
    tb->a_payload_i = static_cast<uint16_t>(0xA000 + a_sent);

    tb->b_valid_i = (b_sent < kPer);
    tb->b_vx_i = static_cast<uint32_t>(b_sent) << 15;
    tb->b_vy_i = 0x00030000u;
    tb->b_vz_i = 0x00020000u;
    tb->b_view_i = 1;
    tb->b_payload_i =
        (0x2AAull << 32) | static_cast<uint64_t>(0xB000u + b_sent);

    settle0(tb);
    const bool acc_a = tb->a_valid_i && tb->a_ready_o;
    const bool acc_b = tb->b_valid_i && tb->b_ready_o;
    CHECK(!(acc_a && acc_b), "both clients granted in one cycle");
    if (tb->a_valid_o) a_got.push_back(tb->a_payload_o);
    if (tb->b_valid_o) b_got.push_back(tb->b_payload_o);
    edge(tb);
    if (acc_a) ++a_sent;
    if (acc_b) ++b_sent;
    if (acc_a || acc_b) {
      if (accepts == 0) first_accept = cyc;
      last_accept = cyc;
      ++accepts;
    }
    ++cyc;
  }
  CHECK(cyc < kTimeout, "smoke timed out (a %zu/%d, b %zu/%d)", a_got.size(),
        kPer, b_got.size(), kPer);

  // routing, order, payload integrity
  CHECK(static_cast<int>(a_got.size()) == kPer, "A got %zu of %d",
        a_got.size(), kPer);
  CHECK(static_cast<int>(b_got.size()) == kPer, "B got %zu of %d",
        b_got.size(), kPer);
  bool a_order = true, b_order = true;
  for (int i = 0; i < kPer && i < static_cast<int>(a_got.size()); ++i) {
    if (a_got[i] != 0xA000 + i) a_order = false;
  }
  for (int i = 0; i < kPer && i < static_cast<int>(b_got.size()); ++i) {
    uint64_t want = (0x2AAull << 32) | static_cast<uint64_t>(0xB000u + i);
    if (b_got[i] != want) b_order = false;
  }
  CHECK(a_order, "client A records out of order or payload-corrupt");
  CHECK(b_order, "client B records out of order or payload-corrupt");

  // fairness and rate under II=3 gating
  CHECK(tb->a_grants_o == static_cast<uint32_t>(kPer), "a_grants %u != %d",
        tb->a_grants_o, kPer);
  CHECK(tb->b_grants_o == static_cast<uint32_t>(kPer), "b_grants %u != %d",
        tb->b_grants_o, kPer);
  // 120 accepts, one per 3 en-cycles: the span from first to last acceptance
  // is exactly 3*(accepts-1) when the gating is right.
  CHECK(accepts == 2 * kPer, "accept count %d != %d", accepts, 2 * kPer);
  CHECK(last_accept - first_accept == 3ull * (2 * kPer - 1),
        "acceptance span %llu != %llu — the II=3 gating is wrong",
        static_cast<unsigned long long>(last_accept - first_accept),
        static_cast<unsigned long long>(3ull * (2 * kPer - 1)));
  // Both clients saturated, so every grant was contended EXCEPT the last:
  // the alternation drains both queues together, and whichever client takes
  // the final slot no longer has a rival offering. 2*kPer - 1, exactly — and
  // this is the counter SEEN TO MOVE (first run of this check expected 120
  // and the design's 119 was the correct number).
  CHECK(tb->contended_o == static_cast<uint32_t>(2 * kPer - 1),
        "contended %u != %d (must count contended GRANTS, one per accept "
        "under dual saturation minus the drained-rival final slot, not "
        "contended waits)",
        tb->contended_o, 2 * kPer - 1);

  // ---- solo client: contended stays zero, A takes every slot --------------
  const uint32_t a_before = tb->a_grants_o;
  const uint32_t cont_before = tb->contended_o;
  int solo_accepts = 0;
  uint64_t solo_first = 0, solo_last = 0;
  tb->b_valid_i = 0;
  for (uint64_t c = 0; c < 64; ++c) {
    tb->a_valid_i = 1;
    tb->a_vx_i = 0x00010000u;
    tb->a_payload_i = 0x5555;
    settle0(tb);
    const bool acc_a = tb->a_ready_o != 0;
    edge(tb);
    if (acc_a) {
      if (solo_accepts == 0) solo_first = c;
      solo_last = c;
      ++solo_accepts;
    }
  }
  tb->a_valid_i = 0;
  CHECK(solo_accepts == 22, "solo A accepts %d in 64 cycles (want 22 at II=3)",
        solo_accepts);
  CHECK(solo_last - solo_first == 3ull * (solo_accepts - 1),
        "solo A acceptance spacing broken");
  CHECK(tb->a_grants_o == a_before + static_cast<uint32_t>(solo_accepts),
        "solo A grants did not track accepts");
  CHECK(tb->contended_o == cont_before,
        "contended_o moved with one client offering (%u -> %u)", cont_before,
        tb->contended_o);

  // drain
  for (int c = 0; c < 80; ++c) {
    settle0(tb);
    edge(tb);
  }
  settle0(tb);
  CHECK(!tb->busy_o, "service busy after drain");

  delete tb;
  if (g_fails == 0) {
    std::printf("proj_service_rowmux_smoke: %d checks passed\n", g_checks);
    return 0;
  }
  std::printf("proj_service_rowmux_smoke: %d of %d checks FAILED\n", g_fails,
              g_checks);
  return 1;
}
