// sheet_shape_equivalence.cpp — RUN-20260824-0317.
//
// A DIRECT differential between the SHAPE THAT WAS and the SHAPE THAT IS: the
// pre-change `zhao_surface_sheet` (one 16-bit array written with byte enables)
// and the post-change one (two 8-bit planes written whole), driven from ONE
// stimulus stream and compared on EVERY output port EVERY cycle.
//
// WHY THIS EXISTS AND THE SHIPPED SUITE IS NOT ENOUGH. The three shipped tests
// pass, and one of them is a real differential against `zref::surface::
// SheetStore`. But the reference cannot see read-during-write at all — it is a
// C++ model with no notion of a cycle — and the contract's C5 makes the
// read-during-write answer LOAD-BEARING:
//
//     "Read-during-write at the same address returns the OLD word (both
//      accesses live in one `always_ff`). SURFACE.STAMP never does that ... but
//      the semantics are stated rather than left to the synthesiser."
//
// A semantic that is stated, that no consumer exercises, and that no test
// covers is exactly the kind of thing a storage-shape change silently moves.
// So this harness drives same-address read/write collisions ON PURPOSE, along
// with every other traffic shape, and requires the two RTLs to be
// indistinguishable.
//
// The stimulus is deliberately hostile to the change:
//   * same-cycle read and write to the SAME address (the C5 collision),
//   * tag-only and strength-only writes (the enables that used to be byte
//     enables and are now per-plane), including on the same texel back to back,
//   * writes during and around the 4,096-cycle clear sweep,
//   * writes to non-resident handles, overflowing acquires, releases,
//   * randomised ready/valid backpressure on both streams.
//
// Not a permanent test: it needs the pre-change RTL, which exists only in git
// history. It is evidence for one commit, kept with the run.

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "verilated.h"

#include "Vzhao_surface_sheet.h"
#include "Vzhao_surface_sheet_pre.h"

namespace {

uint64_t g_rng = 0x9E3779B97F4A7C15ull;
uint32_t rnd() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 7;
  g_rng ^= g_rng << 17;
  return static_cast<uint32_t>(g_rng >> 32);
}

struct Both {
  Vzhao_surface_sheet a;      // post-change: two byte planes
  Vzhao_surface_sheet_pre b;  // pre-change: one word, byte enables

  long mismatches = 0;
  long cycles = 0;
  long collisions = 0;  // same-cycle read and write of the same address

  template <typename F>
  void drive(F f) {
    f(a);
    f(b);
  }

  void tick() {
    drive([](auto& d) { d.clk = 0; d.eval(); });
    drive([](auto& d) { d.clk = 1; d.eval(); });
    ++cycles;
    compare();
  }

#define CMP(field)                                                                          \
  if (a.field != b.field) {                                                                 \
    if (mismatches < 10)                                                                    \
      std::printf("  cycle %ld: %-24s new=%llu old=%llu\n", cycles, #field,                 \
                  static_cast<unsigned long long>(a.field),                                 \
                  static_cast<unsigned long long>(b.field));                                \
    ++mismatches;                                                                           \
  }

  void compare() {
    CMP(req_ready_o)
    CMP(pg_valid_o)
    CMP(pg_op_o)
    CMP(pg_status_o)
    CMP(pg_tag_o)
    CMP(pg_strength_o)
    CMP(pg_src_id_o)
    CMP(wr_ready_o)
    CMP(wr_miss_o)
    CMP(wr_miss_src_id_o)
    CMP(res_occupancy_o)
    CMP(res_busy_o)
    CMP(res_overflow_o)
    CMP(surface_texels_touched_o)
    CMP(idle_o)
  }
#undef CMP

  void reset() {
    drive([](auto& d) {
      d.rst_n = 0;
      d.req_valid_i = 0;
      d.req_op_i = 0;
      d.req_handle_i = 0;
      d.req_texel_i = 0;
      d.req_src_id_i = 0;
      d.pg_ready_i = 1;
      d.wr_valid_i = 0;
      d.wr_handle_i = 0;
      d.wr_texel_i = 0;
      d.wr_tag_i = 0;
      d.wr_strength_i = 0;
      d.wr_we_tag_i = 0;
      d.wr_we_strength_i = 0;
      d.wr_src_id_i = 0;
      d.clk = 0;
      d.eval();
    });
    for (int i = 0; i < 4; ++i) tick();
    drive([](auto& d) { d.rst_n = 1; });
    for (int i = 0; i < 4; ++i) tick();
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* p = new Both();
  Both& t = *p;
  t.reset();

  // Two handles that both fit, plus a third that must overflow.
  const uint32_t kH[3] = {0x0001'0001u, 0x0002'0002u, 0x0003'0003u};

  // ---- phase 1: acquire, so both slots are live and swept ------------------
  for (int h = 0; h < 3; ++h) {
    t.drive([&](auto& d) {
      d.req_valid_i = 1;
      d.req_op_i = 0;  // OP_ACQUIRE
      d.req_handle_i = kH[h];
      d.req_src_id_i = static_cast<uint16_t>(0x200 + h);
    });
    // Hold until accepted, then let the sweep run to completion.
    for (int i = 0; i < 4200; ++i) {
      t.tick();
      if (t.a.req_ready_o == 0 && t.b.req_ready_o == 0) t.drive([](auto& d) { d.req_valid_i = 0; });
    }
    t.drive([](auto& d) { d.req_valid_i = 0; });
    for (int i = 0; i < 8; ++i) t.tick();
  }

  // ---- phase 2: the C5 collision, driven on purpose ------------------------
  // Same texel, same cycle, read and write. The pre-change RTL answers with the
  // PRE-write word because both accesses share one `always_ff`; the post-change
  // RTL must answer identically.
  // EVERY random draw is taken OUTSIDE the lambda. `drive()` applies the lambda
  // to BOTH models, so an `rnd()` call inside it advances the generator twice
  // and hands the two DUTs DIFFERENT stimulus — which is exactly what the first
  // version of this file did, and it reported 624 mismatches that were entirely
  // its own. A differential harness that does not drive one stream is not a
  // differential; see the positive control at the end of this run's log.
  for (int rep = 0; rep < 400; ++rep) {
    const uint16_t texel = static_cast<uint16_t>(rnd() & 0x0FFFu);
    const uint32_t h = kH[rnd() % 2];
    const uint8_t tg = static_cast<uint8_t>(rnd());
    const uint8_t st = static_cast<uint8_t>(rnd());
    const uint8_t et = static_cast<uint8_t>(rnd() & 1);
    const uint8_t es = static_cast<uint8_t>(rnd() & 1);
    t.drive([&](auto& d) {
      d.req_valid_i = 1;
      d.req_op_i = 1;  // OP_READ
      d.req_handle_i = h;
      d.req_texel_i = texel;
      d.req_src_id_i = 0x300;
      d.pg_ready_i = 1;
      d.wr_valid_i = 1;
      d.wr_handle_i = h;
      d.wr_texel_i = texel;  // THE SAME TEXEL
      d.wr_tag_i = tg;
      d.wr_strength_i = st;
      d.wr_we_tag_i = et;
      d.wr_we_strength_i = es;
      d.wr_src_id_i = 0x301;
    });
    ++t.collisions;
    t.tick();
    t.tick();
  }

  // ---- phase 3: tag-only / strength-only on one texel, back to back --------
  // The enables that used to be BYTE enables on one word and are now per-plane
  // write enables. A plane written when it should not be, or not written when
  // it should, shows here.
  for (int rep = 0; rep < 300; ++rep) {
    const uint16_t texel = static_cast<uint16_t>(rep & 0x0FFFu);
    for (int pass = 0; pass < 3; ++pass) {
      t.drive([&](auto& d) {
        d.wr_valid_i = 1;
        d.wr_handle_i = kH[0];
        d.wr_texel_i = texel;
        d.wr_tag_i = static_cast<uint8_t>(0xA0 + pass);
        d.wr_strength_i = static_cast<uint8_t>(0x50 + pass);
        d.wr_we_tag_i = (pass != 1);       // pass 1 is strength-only
        d.wr_we_strength_i = (pass != 2);  // pass 2 is tag-only
        d.wr_src_id_i = 0x400;
        d.req_valid_i = 0;
      });
      t.tick();
    }
    // Read it back.
    t.drive([&](auto& d) {
      d.wr_valid_i = 0;
      d.req_valid_i = 1;
      d.req_op_i = 1;
      d.req_handle_i = kH[0];
      d.req_texel_i = texel;
      d.req_src_id_i = 0x401;
      d.pg_ready_i = 1;
    });
    t.tick();
    t.drive([](auto& d) { d.req_valid_i = 0; });
    t.tick();
    t.tick();
  }

  // ---- phase 4: unconstrained random traffic, with backpressure ------------
  for (int i = 0; i < 200000; ++i) {
    const uint32_t r = rnd();
    t.drive([&](auto& d) {
      d.req_valid_i = (r & 3) != 0;
      d.req_op_i = (r >> 2) & 3;
      d.req_handle_i = kH[(r >> 4) % 3];
      d.req_texel_i = (r >> 6) & 0x0FFF;
      d.req_src_id_i = static_cast<uint16_t>(r);
      d.pg_ready_i = (r >> 18) & 1;
      d.wr_valid_i = (r >> 19) & 1;
      d.wr_handle_i = kH[(r >> 20) % 3];
      d.wr_texel_i = (r >> 6) & 0x0FFF;  // often collides with req_texel_i
      d.wr_tag_i = static_cast<uint8_t>(r >> 8);
      d.wr_strength_i = static_cast<uint8_t>(r >> 16);
      d.wr_we_tag_i = (r >> 22) & 1;
      d.wr_we_strength_i = (r >> 23) & 1;
      d.wr_src_id_i = static_cast<uint16_t>(r >> 3);
    });
    t.tick();
  }

  // ---- phase 5: release and re-acquire, so a sweep runs against traffic ----
  for (int rep = 0; rep < 3; ++rep) {
    t.drive([&](auto& d) {
      d.req_valid_i = 1;
      d.req_op_i = 2;  // OP_RELEASE
      d.req_handle_i = kH[rep % 2];
      d.req_src_id_i = 0x500;
      d.pg_ready_i = 1;
      d.wr_valid_i = 0;
    });
    t.tick();
    t.drive([](auto& d) { d.req_valid_i = 0; });
    t.tick();
    t.drive([&](auto& d) {
      d.req_valid_i = 1;
      d.req_op_i = 0;
      d.req_handle_i = kH[2];
      d.req_src_id_i = 0x501;
    });
    t.tick();
    t.drive([](auto& d) { d.req_valid_i = 0; });
    // Hammer the write port through the whole sweep.
    for (int i = 0; i < 4300; ++i) {
      const uint32_t r = rnd();
      t.drive([&](auto& d) {
        d.wr_valid_i = 1;
        d.wr_handle_i = kH[(r >> 20) % 3];
        d.wr_texel_i = (r >> 6) & 0x0FFF;
        d.wr_tag_i = static_cast<uint8_t>(r);
        d.wr_strength_i = static_cast<uint8_t>(r >> 8);
        d.wr_we_tag_i = (r >> 22) & 1;
        d.wr_we_strength_i = (r >> 23) & 1;
        d.wr_src_id_i = static_cast<uint16_t>(r);
      });
      t.tick();
    }
    t.drive([](auto& d) { d.wr_valid_i = 0; });
    t.tick();
  }

  std::printf("sheet_shape_equivalence: %ld cycles compared, %ld same-address collisions driven\n",
              t.cycles, t.collisions);
  if (t.mismatches != 0) {
    std::printf("FAIL: %ld port-cycle mismatches between the pre- and post-change RTL\n",
                t.mismatches);
    return 1;
  }
  std::printf("PASS: the two shapes are indistinguishable on every output port, every cycle\n");
  return 0;
}
