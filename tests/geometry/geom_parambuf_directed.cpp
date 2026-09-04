// geom_parambuf_directed.cpp — the three records, and the one thing that makes
// a stale chunk detectable at all.
//
// ---------------------------------------------------------------------------
// WHY frame_generation IS THE INTERESTING FIELD
// ---------------------------------------------------------------------------
// A tile-reference chunk from LAST frame reads as a perfectly valid chunk in
// every other respect: its count is sane, its next pointer is inside the arena,
// its triangle ids index real triangles. Nothing about its content says it is
// old. The generation is the only thing that does.
//
// So the checks below are not "does the walk work". They are: does a chunk
// that is wrong ONLY in its generation get refused, and does refusing it stop
// the walk rather than merely flagging it — because following a stale pointer
// is how one bad record becomes a traversal of arbitrary memory.
//
// ---------------------------------------------------------------------------
// AND THE TWO LEGALITY RULES THAT LOOK LIKE CLAMPS AND ARE NOT
// ---------------------------------------------------------------------------
// A screen coordinate outside s21 and a vertex id past the sealed count are
// both MALFORMED, not values to be brought into range. Clamping either one
// produces a triangle somewhere plausible, drawn from somebody else's data.
// The tests require refusal, and would fail against an implementation that
// helpfully clamped.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "verilated.h"

#include "Vzhao_geom_parambuf.h"

#include "zhao_sim.hpp"
#include "zref/zref_geom.hpp"

namespace {

// Little-endian byte packing into a Verilator wide signal. Templated on the
// CONTAINER rather than on a raw array: Verilator gives a VlWide<N> for wide
// ports, not a uint32_t[N], and a template that only matches the array form
// compiles for the 24-byte record and fails on the 64-byte one.
template <typename W>
void set_words(W& dst, const uint8_t* b, int nbytes) {
  for (int i = 0; i < (nbytes + 3) / 4; ++i) dst[i] = 0;
  for (int i = 0; i < nbytes; ++i) dst[i / 4] |= static_cast<uint32_t>(b[i]) << (8 * (i % 4));
}

void put32(uint8_t* b, int off, uint32_t v) {
  for (int i = 0; i < 4; ++i) b[off + i] = static_cast<uint8_t>(v >> (8 * i));
}
void put16(uint8_t* b, int off, uint16_t v) {
  b[off] = static_cast<uint8_t>(v);
  b[off + 1] = static_cast<uint8_t>(v >> 8);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_parambuf top;

  top.pv_valid_i = 0;
  top.td_valid_i = 0;
  top.ck_valid_i = 0;
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  // ---- 1: ProjectedVertex, every field at its own offset -----------------
  {
    uint8_t b[24];
    std::memset(b, 0, sizeof(b));
    put32(b, 0, static_cast<uint32_t>(-1000));    // screen_x
    put32(b, 4, 2000);                            // screen_y
    put32(b, 8, 0x00ABCDEF);                      // invw24 (low 24) ...
    b[11] = 0x5A;                                 // ... status byte
    put32(b, 12, static_cast<uint32_t>(-77777));  // u_over_w
    put32(b, 16, 88888);                          // v_over_w
    put32(b, 20, 0xDEADBEEF);                     // rgba8

    set_words(top.pv_bytes_i, b, 24);
    top.pv_valid_i = 1;
    top.eval();

    const bool ok = static_cast<int32_t>(top.pv_x_o) == -1000 &&
                    static_cast<int32_t>(top.pv_y_o) == 2000 && top.pv_invw_o == 0xABCDEF &&
                    top.pv_status_o == 0x5A && static_cast<int32_t>(top.pv_uow_o) == -77777 &&
                    static_cast<int32_t>(top.pv_vow_o) == 88888 && top.pv_rgba_o == 0xDEADBEEF;
    zhao::check(ok, "a ProjectedVertex decodes every field at its own offset", 1, ok ? 1 : 0);
    zhao::check(top.pv_illegal_o == 0, "and a coordinate inside s21 is legal", 0, top.pv_illegal_o);
    zhao::tick(top);
    top.pv_valid_i = 0;
  }

  // ---- 2: s21 is a LEGALITY rule, not a clamp ---------------------------
  {
    struct C {
      int32_t v;
      bool legal;
      const char* why;
    };
    const C cases[] = {
        {1048575, true, "the largest positive s21"},
        {1048576, false, "one past it"},
        {-1048576, true, "the most negative s21"},
        {-1048577, false, "one below it"},
        {0, true, "zero"},
        {1 << 30, false, "far outside"},
    };
    int bad = 0;
    const uint32_t before = top.pv_illegal_count_o;
    int expect_illegal = 0;
    for (const C& c : cases) {
      uint8_t b[24];
      std::memset(b, 0, sizeof(b));
      put32(b, 0, static_cast<uint32_t>(c.v));
      set_words(top.pv_bytes_i, b, 24);
      top.pv_valid_i = 1;
      top.eval();
      // The oracle is checked against the same table, so the RTL and the
      // reference are not merely agreeing with each other.
      if (zref::geom::parambuf_fits_s21(c.v) != c.legal) ++bad;
      if ((top.pv_illegal_o != 0) == c.legal) {
        ++bad;
        std::printf("    s21: %d -> illegal=%d, expected legal=%d (%s)\n", c.v, top.pv_illegal_o,
                    c.legal ? 1 : 0, c.why);
      }
      if (!c.legal) ++expect_illegal;
      // AND the coordinate still comes out unchanged: a refusal reports, it
      // does not correct. An implementation that clamped would pass a test
      // that only looked at the flag.
      if (static_cast<int32_t>(top.pv_x_o) != c.v) ++bad;
      zhao::tick(top);
      top.pv_valid_i = 0;
    }
    zhao::check(bad == 0,
                "s21 legality is exact at both boundaries, and the coordinate "
                "is REPORTED not clamped -- clamping would place a triangle "
                "somewhere plausible from somebody else's data",
                0, bad);
    zhao::check(top.pv_illegal_count_o == before + expect_illegal,
                "and every illegal one is counted", expect_illegal,
                static_cast<int>(top.pv_illegal_count_o - before));
  }

  // ---- 3: TriangleDescriptor, and the sealed vertex count ---------------
  {
    uint8_t b[16];
    std::memset(b, 0, sizeof(b));
    put16(b, 0, 10);
    put16(b, 2, 20);
    put16(b, 4, 30);
    put16(b, 6, 0x0777);
    put32(b, 8, 0x12345678);
    put32(b, 12, 0xA5A5A5A5);

    set_words(top.td_bytes_i, b, 16);
    top.td_sealed_vertices_i = 100;
    top.td_valid_i = 1;
    top.eval();
    const bool ok = top.td_v0_o == 10 && top.td_v1_o == 20 && top.td_v2_o == 30 &&
                    top.td_material_o == 0x0777 && top.td_raster_o == 0x12345678 &&
                    top.td_source_o == 0xA5A5A5A5;
    zhao::check(ok && top.td_illegal_o == 0,
                "a TriangleDescriptor decodes, and ids inside the sealed count "
                "are legal",
                1, (ok && !top.td_illegal_o) ? 1 : 0);
    zhao::tick(top);

    // one id past the sealed count
    const uint32_t before = top.td_illegal_count_o;
    put16(b, 4, 100);  // == sealed count, so out of range
    set_words(top.td_bytes_i, b, 16);
    top.eval();
    zhao::check(top.td_illegal_o == 1,
                "a vertex id AT the sealed count is out of range -- the count "
                "is a count, not a last index",
                1, top.td_illegal_o);
    zhao::tick(top);
    top.td_valid_i = 0;
    zhao::check(top.td_illegal_count_o == before + 1, "and is counted", 1,
                static_cast<int>(top.td_illegal_count_o - before));
  }

  // ---- 4: THE CHUNK, and the generation that is its only tell -----------
  {
    uint8_t b[64];
    std::memset(b, 0, sizeof(b));
    put32(b, 0, 1234);    // next_chunk, inside the arena
    put16(b, 4, 14);      // count, exactly the capacity
    put16(b, 6, 0x00AA);  // frame_generation

    set_words(top.ck_bytes_i, b, 64);
    top.ck_frame_gen_i = 0x00AA;
    top.ck_valid_i = 1;
    top.eval();
    zhao::check(top.ck_stale_o == 0 && top.ck_illegal_o == 0 && top.ck_follow_o == 1,
                "a current chunk is neither stale nor malformed, and may be "
                "followed",
                1, (top.ck_follow_o && !top.ck_stale_o) ? 1 : 0);
    zhao::tick(top);

    // THE CASE: identical in every respect except the generation.
    const uint32_t stale_before = top.ck_stale_count_o;
    top.ck_frame_gen_i = 0x00AB;
    top.eval();
    zhao::check(top.ck_stale_o == 1,
                "the SAME chunk, with only the frame generation moved on, is "
                "stale -- nothing about its content says so, which is why the "
                "generation is per chunk",
                1, top.ck_stale_o);
    zhao::check(zref::geom::parambuf_chunk_follow(0x00AA, 0x00AB, 14, 1234, 65536) == false,
                "zref::geom::parambuf_chunk_follow agrees that a stale chunk is "
                "not followable",
                1, 1);
    zhao::check(top.ck_follow_o == 0,
                "and it may NOT be followed -- following a stale pointer is how "
                "one bad record becomes a walk through arbitrary memory",
                0, top.ck_follow_o);
    zhao::tick(top);
    zhao::check(top.ck_stale_count_o == stale_before + 1, "and it is counted", 1,
                static_cast<int>(top.ck_stale_count_o - stale_before));
  }

  // ---- 5: a chunk malformed in its own fields ---------------------------
  {
    const uint32_t before = top.ck_illegal_count_o;
    uint8_t b[64];
    std::memset(b, 0, sizeof(b));
    put32(b, 0, 1234);
    put16(b, 4, 15);  // count ABOVE the 14-id capacity
    put16(b, 6, 0x0055);
    set_words(top.ck_bytes_i, b, 64);
    top.ck_frame_gen_i = 0x0055;
    top.ck_valid_i = 1;
    top.eval();
    zhao::check(top.ck_illegal_o == 1 && top.ck_follow_o == 0,
                "a count above the chunk's own capacity is malformed and stops "
                "the walk",
                1, (top.ck_illegal_o && !top.ck_follow_o) ? 1 : 0);
    zhao::tick(top);

    // next_chunk outside the arena
    put16(b, 4, 3);
    put32(b, 0, 65536);  // == ARENA_CHUNKS, so out of range
    set_words(top.ck_bytes_i, b, 64);
    top.eval();
    zhao::check(top.ck_illegal_o == 1, "and a next_chunk at the arena's size is out of range", 1,
                top.ck_illegal_o);
    zhao::tick(top);

    // the NULL sentinel is not an address and is not malformed
    put32(b, 0, 0xFFFFFFFF);
    set_words(top.ck_bytes_i, b, 64);
    top.eval();
    zhao::check(top.ck_illegal_o == 0 && top.ck_follow_o == 0,
                "the all-ones sentinel ends the list without being malformed -- "
                "'no next chunk' and 'a bad next chunk' are different answers",
                1, (!top.ck_illegal_o && !top.ck_follow_o) ? 1 : 0);
    zhao::tick(top);
    top.ck_valid_i = 0;
    zhao::check(top.ck_illegal_count_o == before + 2,
                "and exactly the two malformed ones were counted", 2,
                static_cast<int>(top.ck_illegal_count_o - before));
  }

  std::printf(
      "  %u illegal vertices, %u illegal triangles, %u stale chunks, "
      "%u malformed chunks\n",
      top.pv_illegal_count_o, top.td_illegal_count_o, top.ck_stale_count_o, top.ck_illegal_count_o);

  return zhao::report_and_exit("geom_parambuf_directed");
}
