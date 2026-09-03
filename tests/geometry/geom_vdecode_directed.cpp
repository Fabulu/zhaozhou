// geom_vdecode_directed.cpp — does format 0 decode to the bytes the spec says?
//
// ---------------------------------------------------------------------------
// WHY THIS ONE MATTERS MORE THAN ITS SIZE SUGGESTS
// ---------------------------------------------------------------------------
// Format 0 is the DIFFERENTIAL REFERENCE. Ruling R11 makes packed formats 1
// and 2 bake-off gated, and says every later format must decode to
// bit-identical output for the same source mesh. Format 0 is what "the same"
// will be measured against — so an error here does not produce one wrong mesh,
// it produces a wrong definition of correct that formats 1 and 2 are then
// built to match.
//
// The checks are therefore against `zref::geom::vdecode0`, written from the
// contract's byte table, and against FIELD ISOLATION: change one byte of the
// record and only the fields that own it may move. A decoder whose position
// and normal overlap by a byte still agrees with a reference that shares the
// mistake, and still round-trips.
//
// The malformed cases are checked as REFUSALS. GEOM.VDECODE.md ratifies the
// taxonomy — "each of the 8 reserved bytes nonzero in turn: 8 refusals" and
// "unknown format_id: refused, and NO VERTEX EMITTED — the safety case" — so
// a refused record emits nothing and `d_valid_o` is the batch engine's
// guarantee that what reaches it is decodable.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_vdecode.h"

#include "zhao_sim.hpp"
#include "zref/zref_geom.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

int32_t sx(uint32_t v, int bits) {
  const uint32_t m = 1u << (bits - 1);
  return static_cast<int32_t>((v ^ m) - m);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_vdecode top;

  auto reset = [&]() {
    top.v_valid_i = 0;
    top.d_ready_i = 1;
    top.v_format_i = 0;
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // v_bytes_i is 256 bits: eight 32-bit words, byte 0 at bit 0.
  auto set_bytes = [&](const uint8_t* b) {
    for (int w = 0; w < 8; ++w) {
      uint32_t word = 0;
      for (int k = 0; k < 4; ++k)
        word |= static_cast<uint32_t>(b[w * 4 + k]) << (8 * k);
      top.v_bytes_i[w] = word;
    }
  };

  // Feed one record and return with the decoded vertex on the outputs.
  auto feed = [&](const uint8_t* b, int format = 0) {
    set_bytes(b);
    top.v_format_i = format;
    top.v_src_id_i = 0x4242;
    top.v_valid_i = 1;
    top.d_ready_i = 1;
    top.eval();
    zhao::tick(top);
    top.v_valid_i = 0;
    top.eval();
  };

  reset();

  // ---- 1: against the oracle, over random records -------------------------
  int bad = 0, checked = 0;
  uint32_t s = 0x1D0DEu;
  for (int i = 0; i < 1500; ++i) {
    uint8_t b[32];
    for (int k = 0; k < 24; ++k) b[k] = static_cast<uint8_t>(rnd(&s));
    for (int k = 24; k < 32; ++k) b[k] = 0;      // reserved MUST be zero
    b[15] = static_cast<uint8_t>(rnd(&s) % 65u);  // a legal w0

    const zref::geom::Vertex0 want = zref::geom::vdecode0(b);
    feed(b);

    const bool ok =
        sx(top.d_x_o, 32) == want.x && sx(top.d_y_o, 32) == want.y &&
        sx(top.d_z_o, 32) == want.z &&
        static_cast<int8_t>(top.d_nx_o) == want.nx &&
        static_cast<int8_t>(top.d_ny_o) == want.ny &&
        static_cast<int8_t>(top.d_nz_o) == want.nz &&
        top.d_w0_o == want.w0 && top.d_rigid_o == (want.rigid ? 1 : 0) &&
        static_cast<int16_t>(top.d_u_o) == want.u &&
        static_cast<int16_t>(top.d_v_o) == want.v &&
        top.d_bone0_o == want.bone0 && top.d_bone1_o == want.bone1 &&
        top.d_reserved_nz_o == 0 && top.d_w0_illegal_o == 0;
    if (!ok) ++bad;
    ++checked;
  }
  zhao::check(bad == 0,
              "every field matches zref::geom::vdecode0 over 1,500 random "
              "records -- format 0 IS the differential reference, so this is "
              "the definition later formats will be measured against",
              0, bad);

  // ---- 2: FIELD ISOLATION -------------------------------------------------
  // Change one byte; only the fields that own it may move.
  {
    uint8_t base[32];
    std::memset(base, 0, sizeof(base));
    base[15] = 32;                            // a legal mid-range w0
    feed(base);
    const int32_t bx = sx(top.d_x_o, 32), by = sx(top.d_y_o, 32),
                  bz = sx(top.d_z_o, 32);
    const int bnx = static_cast<int8_t>(top.d_nx_o);
    const int bw0 = top.d_w0_o;
    const int bu = static_cast<int16_t>(top.d_u_o);
    const int bb0 = top.d_bone0_o, bb1 = top.d_bone1_o;

    int leak = 0;
    // byte 0 belongs to x alone
    uint8_t b[32];
    std::memcpy(b, base, sizeof(b));
    b[0] = 0xFF;
    feed(b);
    if (sx(top.d_y_o, 32) != by || sx(top.d_z_o, 32) != bz ||
        static_cast<int8_t>(top.d_nx_o) != bnx || top.d_w0_o != bw0 ||
        static_cast<int16_t>(top.d_u_o) != bu || top.d_bone0_o != bb0)
      ++leak;
    if (sx(top.d_x_o, 32) == bx) ++leak;      // and it must actually move x

    // byte 12 belongs to nx alone
    std::memcpy(b, base, sizeof(b));
    b[12] = 0x80;
    feed(b);
    if (sx(top.d_x_o, 32) != bx || top.d_w0_o != bw0 ||
        static_cast<int16_t>(top.d_u_o) != bu)
      ++leak;
    if (static_cast<int8_t>(top.d_nx_o) != -128) ++leak;

    // bytes 20..23 are the two bones, and they are SEPARATE
    std::memcpy(b, base, sizeof(b));
    b[20] = 0x34; b[21] = 0x12;
    feed(b);
    if (top.d_bone0_o != 0x1234 || top.d_bone1_o != bb1) ++leak;
    if (top.d_rigid_o != 0) ++leak;           // bone0 != bone1 now

    zhao::check(leak == 0,
                "each byte moves only the fields that own it, and rigid "
                "follows bone1 == bone0",
                0, leak);
  }

  // ---- 3: RIGID is bone1 == bone0 ----------------------------------------
  {
    uint8_t b[32];
    std::memset(b, 0, sizeof(b));
    b[15] = 64;                               // 64 == rigid weight
    b[20] = 0x07; b[21] = 0x00;               // bone0 = 7
    b[22] = 0x07; b[23] = 0x00;               // bone1 = 7
    feed(b);
    zhao::check(top.d_rigid_o == 1 && top.d_w0_o == 64,
                "equal bones decode as RIGID, and w0 == 64 is the rigid quanta",
                1, (top.d_rigid_o == 1 && top.d_w0_o == 64) ? 1 : 0);
  }

  // ---- 4: the malformed cases are REFUSED, and emit nothing --------------
  {
    const uint32_t rz_before = top.reserved_nz_o;
    uint8_t b[32];
    std::memset(b, 0, sizeof(b));
    b[15] = 10;
    b[0] = 0xAB;                              // a position we can look for
    b[31] = 0x01;                             // ONE bit in the reserved field
    feed(b);
    zhao::check(top.d_reserved_nz_o == 1 && top.d_refused_o == 1,
                "a single nonzero bit in the reserved eight bytes REFUSES the "
                "record -- this is what stops an older decoder reading a newer "
                "file",
                1, (top.d_reserved_nz_o && top.d_refused_o) ? 1 : 0);
    zhao::check(top.reserved_nz_o == rz_before + 1, "and counted", 1,
                static_cast<int>(top.reserved_nz_o - rz_before));
    // The contract's safety case: refused means NO VERTEX EMITTED. My first
    // version emitted it with a flag and argued for that in a comment; the
    // refusal taxonomy is already ratified and was not mine to reopen.
    zhao::check(top.d_valid_o == 0,
                "and NO VERTEX IS EMITTED -- `d_valid_o` is the batch engine's "
                "guarantee that what it receives is decodable",
                0, top.d_valid_o);

    // w0 above 64 is a malformed asset, not a saturating weight
    const uint32_t w_before = top.w0_illegal_o;
    std::memset(b, 0, sizeof(b));
    b[15] = 65;
    feed(b);
    zhao::check(top.d_w0_illegal_o == 1 && top.w0_illegal_o == w_before + 1 &&
                    top.d_valid_o == 0,
                "w0 above 64 REFUSES the record, and is not saturated -- "
                "saturating turns a bad file into a silently wrong skin",
                1, (top.d_w0_illegal_o == 1 && top.d_valid_o == 0) ? 1 : 0);

    // a non-zero format id on a format-0 decoder
    const uint32_t f_before = top.format_bad_o;
    std::memset(b, 0, sizeof(b));
    b[15] = 1;
    feed(b, /*format=*/1);
    zhao::check(top.d_format_bad_o == 1 && top.format_bad_o == f_before + 1 &&
                    top.d_valid_o == 0,
                "an unknown format id refuses and emits NO VERTEX -- the "
                "contract's named safety case; formats 1 and 2 are bake-off "
                "gated",
                1, (top.d_format_bad_o == 1 && top.d_valid_o == 0) ? 1 : 0);
  }

  std::printf("  %d records decoded, %u reserved-nonzero, %u illegal w0, %u bad format\n",
              checked, top.reserved_nz_o, top.w0_illegal_o, top.format_bad_o);

  return zhao::report_and_exit("geom_vdecode_directed");
}
