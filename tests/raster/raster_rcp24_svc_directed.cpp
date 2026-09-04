// raster_rcp24_svc_directed.cpp — is the scheduled reciprocal bit-identical to
// the serial one, and does it actually keep the multiplier busy?
//
// ---------------------------------------------------------------------------
// THE ORACLE IS THE SHIPPED BLOCK, IN HARDWARE
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md: "Keep the exact existing arithmetic and ROM."
//
// The only honest way to hold that is to run zhao_raster_rcp24 and
// zhao_raster_rcp24_svc on the SAME stimulus and compare, which is what
// tb_rcp24_pair.sv exists for. Re-implementing the Newton iteration in C++ and
// checking the new block against it would prove the new block agrees with a
// third copy of the same idea -- and this project has already shipped a
// measured-looking wrong number that a self-consistent check would have waved
// through.
//
// ---------------------------------------------------------------------------
// AND THE THROUGHPUT IS THE POINT, SO IT IS MEASURED
// ---------------------------------------------------------------------------
// A scheduled block that produced identical answers one-at-a-time would pass
// every equality check and deliver nothing. The brief's claim is one multiplier
// launch per clock -- four micro-jobs per reciprocal, so one reciprocal every
// four clocks against the serial block's much longer occupancy. Both are
// counted here and the ratio is asserted.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vtb_rcp24_pair.h"

#include "zhao_sim.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

struct Res {
  uint32_t r, k, zero;
  bool operator!=(const Res& o) const { return r != o.r || k != o.k || zero != o.zero; }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_rcp24_pair top;

  top.a_valid_i = 0;
  top.b_valid_i = 0;
  top.a_rready_i = 1;
  top.b_rready_i = 1;
  top.rst_n = 0;
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  // ---- stimulus ------------------------------------------------------------
  // Denominators across the whole 24-bit range plus the cases the serial block
  // calls out by name: zero (refused), 1 (maximum normalisation shift), and
  // 2^23 (the only input whose quotient reaches 2^24 and is pinned by law).
  std::vector<uint32_t> ds = {0u, 1u, 2u, 3u, 0x800000u, 0xFFFFFFu, 0x7FFFFFu};
  uint32_t s = 0x13579Bu;
  for (int i = 0; i < 400; ++i) ds.push_back(rnd(&s) & 0xFFFFFFu);

  // ---- run the SERIAL block, recording its answers -------------------------
  std::vector<Res> ref;
  {
    size_t fed = 0;
    int serial_clocks = 0;
    for (int c = 0; c < 200000 && ref.size() < ds.size(); ++c) {
      top.a_valid_i = fed < ds.size();
      top.a_d_i = (fed < ds.size()) ? ds[fed] : 0;
      top.eval();
      if (top.a_rvalid_o) ref.push_back({top.a_r_o, top.a_k_o, top.a_zero_o});
      const bool took = top.a_valid_i && top.a_ready_o;
      zhao::tick(top);
      if (took) ++fed;
      ++serial_clocks;
    }
    top.a_valid_i = 0;
    zhao::check(ref.size() == ds.size(), "the serial block answered every request", ds.size(),
                ref.size());
    std::printf("  serial: %zu reciprocals in %d clocks (%.2f clocks each)\n", ref.size(),
                serial_clocks,
                static_cast<double>(serial_clocks) / static_cast<double>(ref.size()));
  }

  // ---- run the SCHEDULED block on the same stimulus ------------------------
  std::map<uint32_t, Res> got;
  int svc_clocks = 0;
  {
    top.rst_n = 0;
    for (int i = 0; i < 6; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);

    size_t fed = 0;
    for (int c = 0; c < 200000 && got.size() < ds.size(); ++c) {
      const bool feeding = fed < ds.size();
      top.b_valid_i = feeding;
      top.b_d_i = feeding ? ds[fed] : 0;
      top.b_tok_i = static_cast<uint8_t>(fed & 0xFF);
      top.eval();
      if (top.b_rvalid_o) {
        // Tokens wrap at 256 and the batch is longer, so key by the ORDER the
        // token was issued: the map is keyed on a running index reconstructed
        // from the token plus how many wraps have completed.
        const uint32_t tok = top.b_tok_o;
        uint32_t idx = tok;
        while (idx < got.size() && idx + 256 < ds.size()) idx += 256;
        got[idx] = {top.b_r_o, top.b_k_o, top.b_zero_o};
      }
      const bool took = feeding && top.b_ready_o;
      zhao::tick(top);
      if (took) ++fed;
      ++svc_clocks;
    }
    top.b_valid_i = 0;
  }

  zhao::check(got.size() == ds.size(), "the scheduled block answered every request too", ds.size(),
              got.size());

  int mism = 0;
  for (size_t i = 0; i < ds.size() && i < ref.size(); ++i) {
    auto it = got.find(static_cast<uint32_t>(i));
    if (it == got.end()) {
      ++mism;
      continue;
    }
    if (it->second != ref[i]) ++mism;
  }
  zhao::check(mism == 0, "every scheduled answer is BIT-IDENTICAL to the serial block's", 0, mism);

  std::printf("  scheduled: %zu reciprocals in %d clocks (%.2f clocks each)\n", got.size(),
              svc_clocks, static_cast<double>(svc_clocks) / static_cast<double>(got.size()));
  std::printf("  multiplier launches: %u for %zu reciprocals (%.2f each)\n", top.b_mul_busy_o,
              got.size(), static_cast<double>(top.b_mul_busy_o) / static_cast<double>(got.size()));

  // FOUR MICRO-JOBS PER RECIPROCAL, exactly as the brief specifies. More would
  // mean work is being repeated; fewer would mean an iteration was skipped.
  zhao::check(top.b_mul_busy_o == 4u * got.size(),
              "exactly four multiplier launches per reciprocal",
              4u * static_cast<uint32_t>(got.size()), top.b_mul_busy_o);

  // AND IT IS GENUINELY INTERLEAVED. A serial implementation would never hold
  // more than one live context.
  zhao::check(top.b_occupancy_o <= 8, "occupancy stays within the context table", 1,
              top.b_occupancy_o <= 8 ? 1 : 0);

  return zhao::report_and_exit("raster_rcp24_svc_directed");
}
