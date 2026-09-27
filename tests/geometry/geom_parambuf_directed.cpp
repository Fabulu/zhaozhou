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
// AND THE LEGALITY RULES THAT LOOK LIKE CLAMPS AND ARE NOT
// ---------------------------------------------------------------------------
// A malformed status byte and a vertex id past the sealed count are MALFORMED,
// not values to be brought into range. Clamping either one produces a triangle
// somewhere plausible, drawn from somebody else's data. The tests require
// refusal, and would fail against an implementation that helpfully clamped.
//
// ---------------------------------------------------------------------------
// SCHEMA v2, 2026-09-27 (packet PVSCHEMA) -- AND THE TEST THAT FAILS AGAINST v1
// ---------------------------------------------------------------------------
// v1 stored colour as `rgba8 u32`: eight bits per channel through
// `unit8_of_fx16`, which is `(v + 128) >> 8` railed at both ends. But
// `zhao_geom_attrpack` builds its six plane equations from the FULL 32-bit
// attribute slots those channels arrive in, so the three Gouraud planes owner
// ruling R234 D1 added were NOT reconstructible from the record by any back
// end whatever. That is entry I55's real blocker and it is a RECORD.
//
// Section 2 is the evidence, and it is built so that it CANNOT PASS against
// v1: it stores two colours that v1's quantiser maps to THE SAME BYTE and
// requires them to come back DISTINCT. A test using {255,255,255} would prove
// nothing -- saturated white survives any quantiser.
//
// Section 1b is the other half: the s21 refusal that used to live here is
// GONE, because v2 stores x and y in 21 bits and a decoded coordinate is legal
// by construction. A term that cannot reach its own fault is not enforcement,
// so the refusal moved to `zhao_geom_paramarena`, where the 32-bit value still
// exists to be judged, and `pv_illegal_o` here now watches the two status-byte
// laws that previously had NO detector at all.
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

// SCHEMA v2's field table, mirroring `zhao_pkg`. Written out here rather
// than imported so the test is an INDEPENDENT statement of the layout: if the
// RTL's offsets move and these do not, the test fails, which is the whole
// point of a directed test against a schema.
constexpr int kPvBytes = 32;
// SCHEMA v2. Named rather than repeated as a literal at each `set_words` call,
// which is how the 16 below outlived the record it described.
constexpr int kTdBytes = 32;

// The four scan-box bounds are s12 fields at bits 176/188/200/212, so two of
// them do NOT start on a byte. A byte-granular writer would have silently
// tested a layout four bits away from the one `zhao_pkg` declares.
//
// AND IT IS A SECOND FUNCTION ON PURPOSE. The `put_field` below this file
// already had is SET-ONLY -- it ORs a 1 in and leaves a 0 alone -- which is
// correct for filling a freshly zeroed buffer and silently WRONG for putting a
// field back. The reserved-field control needs exactly that: set one bit, watch
// the detector fire, CLEAR it, watch it go quiet. Written with the set-only
// helper the clear would be a no-op, the detector would go on firing, and the
// honest reading of that is "stuck bit" -- a false alarm manufactured by the
// test's own instrument.
// VERILATOR HANDS A NARROW SIGNED PORT BACK AS RAW BITS, and this cost three
// failures before it was believed. `pv_x_o` is `logic signed [31:0]`, so a plain
// `static_cast<int32_t>` is correct on it and every existing check in this file
// does exactly that. `td_min_x_o` is s12 and `td_area2_o` is s48 -- the widths
// `zhao_geom_setup` actually declares -- so the model returns 0x800 for -2048
// and 0x0000_9ABC... for a negative area, and a cast reads them as large
// POSITIVE numbers. The RTL is right and the reader has to sign-extend; the
// direction is the safe one, because it fails loudly rather than agreeing.
int64_t sx48(uint64_t v) {
  v &= 0x0000FFFFFFFFFFFFull;
  return (v & 0x0000800000000000ull)
             ? static_cast<int64_t>(v | 0xFFFF000000000000ull)
             : static_cast<int64_t>(v);
}

int32_t sx12(uint32_t v) {
  v &= 0xFFFu;
  return (v & 0x800u) ? static_cast<int32_t>(v | 0xFFFFF000u)
                      : static_cast<int32_t>(v);
}

void put_field_exact(uint8_t* b, int lo, int w, uint32_t v) {
  for (int i = 0; i < w; ++i) {
    const int p = lo + i;
    if ((v >> i) & 1u)
      b[p / 8] = static_cast<uint8_t>(b[p / 8] | (1u << (p % 8)));
    else
      b[p / 8] = static_cast<uint8_t>(b[p / 8] & ~(1u << (p % 8)));
  }
}
constexpr int kPvXLo = 0, kPvXW = 21;
constexpr int kPvYLo = 21, kPvYW = 21;
constexpr int kPvInvwLo = 42, kPvInvwW = 24;
constexpr int kPvStatusLo = 66, kPvStatusW = 8;
constexpr int kPvUowLo = 74, kPvUowW = 32;
constexpr int kPvVowLo = 106, kPvVowW = 32;
constexpr int kPvRLo = 138, kPvRW = 32;
constexpr int kPvGLo = 170, kPvGW = 32;
constexpr int kPvBLo = 202, kPvBW = 32;
constexpr int kPvALo = 234, kPvAW = 22;

// Place `v`'s low `w` bits at bit offset `lo` of a little-endian byte buffer.
// The record is not byte-aligned past the status byte, so a byte-granular
// helper cannot express it.
void put_field(uint8_t* b, int lo, int w, uint32_t v) {
  for (int i = 0; i < w; ++i) {
    const uint32_t bit = (v >> i) & 1u;
    const int p = lo + i;
    if (bit) b[p / 8] |= static_cast<uint8_t>(1u << (p % 8));
  }
}

// zref::unit8_from_fx16, transcribed. The RTL implements the same function --
// `zhao_unit8_of_fx16` in `zhao_pkg` -- and this is the independent copy the
// two are differenced against. Schema v2 moved the conversion from the
// producer to this decoder; the law did not change.
uint8_t unit8_from_fx16(int32_t r) {
  if (r < 0) return 0;
  if (r > 0xFFFF) return 255;
  int32_t q = (r + 128) >> 8;
  return static_cast<uint8_t>(q > 255 ? 255 : q);
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

  // ---- 1: ProjectedVertex v2, every field at its own offset --------------
  // The offsets below are the SCHEMA v2 table in `zhao_pkg`, written out in
  // bits because that is what the record is: it is not byte-aligned past the
  // status byte and pretending otherwise is how a field gets read one nibble
  // over.
  //
  //   x 0+21 | y 21+21 | invw 42+24 | status 66+8 | u/w 74+32 | v/w 106+32
  //   | r 138+32 | g 170+32 | b 202+32 | alpha 234+22   = 256 bits
  {
    uint8_t b[kPvBytes];
    std::memset(b, 0, sizeof(b));
    put_field(b, kPvXLo, kPvXW, static_cast<uint32_t>(-1000));
    put_field(b, kPvYLo, kPvYW, static_cast<uint32_t>(2000));
    put_field(b, kPvInvwLo, kPvInvwW, 0x00ABCDEFu);
    put_field(b, kPvStatusLo, kPvStatusW, 0x0Au);   // shared=1, untex=0, dom=2
    put_field(b, kPvUowLo, kPvUowW, static_cast<uint32_t>(-77777));
    put_field(b, kPvVowLo, kPvVowW, static_cast<uint32_t>(88888));
    put_field(b, kPvRLo, kPvRW, 0x00011234u);
    put_field(b, kPvGLo, kPvGW, 0x0000ABCDu);
    put_field(b, kPvBLo, kPvBW, static_cast<uint32_t>(-4242));
    put_field(b, kPvALo, kPvAW, 0x00010000u);       // fx16 1.0, opaque

    set_words(top.pv_bytes_i, b, kPvBytes);
    top.pv_valid_i = 1;
    top.eval();

    const bool ok = static_cast<int32_t>(top.pv_x_o) == -1000 &&
                    static_cast<int32_t>(top.pv_y_o) == 2000 && top.pv_invw_o == 0xABCDEF &&
                    top.pv_status_o == 0x0A && static_cast<int32_t>(top.pv_uow_o) == -77777 &&
                    static_cast<int32_t>(top.pv_vow_o) == 88888 &&
                    static_cast<int32_t>(top.pv_r_o) == 0x00011234 &&
                    static_cast<int32_t>(top.pv_g_o) == 0x0000ABCD &&
                    static_cast<int32_t>(top.pv_b_o) == -4242 &&
                    static_cast<int32_t>(top.pv_alpha_o) == 0x00010000;
    zhao::check(ok, "a ProjectedVertex v2 decodes every field at its own offset", 1, ok ? 1 : 0);
    zhao::check(top.pv_illegal_o == 0, "and a well-formed status byte is legal", 0,
                top.pv_illegal_o);

    // The 8-bit view is DERIVED here now, by the same law that used to be a
    // private function in `zhao_geom_vertid`. { a, b, g, r }, r in the low
    // byte. -4242 is negative, so blue rails to 0 rather than wrapping.
    const uint32_t want_rgba = (static_cast<uint32_t>(unit8_from_fx16(0x00010000)) << 24) |
                               (static_cast<uint32_t>(unit8_from_fx16(-4242)) << 16) |
                               (static_cast<uint32_t>(unit8_from_fx16(0x0000ABCD)) << 8) |
                               static_cast<uint32_t>(unit8_from_fx16(0x00011234));
    zhao::check(top.pv_rgba_o == want_rgba,
                "and rgba8 is DERIVED from the stored channels by zref::unit8_from_fx16, "
                "r in the low byte -- not stored beside them",
                static_cast<int>(want_rgba), static_cast<int>(top.pv_rgba_o));
    zhao::tick(top);
    top.pv_valid_i = 0;
  }

  // ---- 1b: s21 and s22 are STORED domains, and they round-trip ----------
  // Sign extension out of a 21-bit field is the thing that makes storing s21
  // lossless rather than a truncation, and it is the one property the whole
  // narrowing rests on. Both rails and both signs.
  {
    // x and y carry DIFFERENT values in every case, so a decoder that read
    // one field's bits for the other would fail rather than agree with
    // itself. The y list is the x list rotated -- note that it is a separate
    // list and NOT `-x`: s21 is asymmetric, so negating -1,048,576 leaves the
    // domain and the test would be asserting a truncation it asked for.
    const int32_t xs[] = {0, 1, -1, 1048575, -1048576, 12345, -12345};
    const int32_t ys[] = {-1, 0, 1, -1048576, 1048575, -12345, 12345};
    const int n_s21 = static_cast<int>(sizeof(xs) / sizeof(xs[0]));
    int bad = 0;
    for (int i = 0; i < n_s21; ++i) {
      const int32_t v = xs[i];
      const int32_t y = ys[i];
      // The oracle agrees these are all inside the declared domain, so the
      // test is not merely asserting that the RTL agrees with itself.
      if (!zref::geom::parambuf_fits_s21(v)) ++bad;
      if (!zref::geom::parambuf_fits_s21(y)) ++bad;
      uint8_t b[kPvBytes];
      std::memset(b, 0, sizeof(b));
      put_field(b, kPvXLo, kPvXW, static_cast<uint32_t>(v));
      put_field(b, kPvYLo, kPvYW, static_cast<uint32_t>(y));
      set_words(top.pv_bytes_i, b, kPvBytes);
      top.pv_valid_i = 1;
      top.eval();
      if (static_cast<int32_t>(top.pv_x_o) != v) ++bad;
      if (static_cast<int32_t>(top.pv_y_o) != y) ++bad;
      zhao::tick(top);
      top.pv_valid_i = 0;
    }
    zhao::check(bad == 0,
                "every s21 screen coordinate round-trips through the 21-bit field, "
                "sign-extended at both rails -- which is what makes v2's narrowing "
                "a declaration of the enforced domain rather than a truncation",
                0, bad);

    // Alpha's s22, at its own rails.
    const int32_t as[] = {0, 65536, 2097151, -2097152, -1};
    int abad = 0;
    for (int32_t v : as) {
      uint8_t b[kPvBytes];
      std::memset(b, 0, sizeof(b));
      put_field(b, kPvALo, kPvAW, static_cast<uint32_t>(v));
      set_words(top.pv_bytes_i, b, kPvBytes);
      top.pv_valid_i = 1;
      top.eval();
      if (static_cast<int32_t>(top.pv_alpha_o) != v) ++abad;
      zhao::tick(top);
      top.pv_valid_i = 0;
    }
    zhao::check(abad == 0,
                "and alpha round-trips through s22, which carries its whole declared "
                "domain (fx16 1.0 == 0x1_0000) with a sign bit and five bits spare",
                0, abad);
  }

  // ---- 2: THE COLOUR ROUND-TRIPS AT FULL PRECISION ----------------------
  // THIS IS THE CHECK THAT CANNOT PASS AGAINST v1. Both values below are
  // mapped to the SAME BYTE by `unit8_from_fx16` -- v1 stored that byte and
  // nothing else, so the two vertices were literally the same record. v2
  // stores the slots, so they come back distinct.
  //
  // Chosen deliberately at a value that is NOT saturated: {255,255,255} is
  // recoverable under any quantiser and would prove nothing.
  {
    // BOTH LAND IN ONE unit8 BUCKET, and the bucket is computed rather than
    // guessed: (v + 128) >> 8 == 18 holds for 4,480 <= v <= 4,735, so these
    // are its two ends. 255 counts apart in fx16 and indistinguishable in v1.
    const int32_t lo = 0x00001180;   // 4,480
    const int32_t hi = 0x0000127F;   // 4,735
    zhao::check(unit8_from_fx16(lo) == unit8_from_fx16(hi),
                "premise: v1's quantiser maps both test colours to ONE byte, so a "
                "record storing only rgba8 cannot tell them apart",
                unit8_from_fx16(lo), unit8_from_fx16(hi));

    int32_t got[2] = {0, 0};
    const int32_t in[2] = {lo, hi};
    for (int k = 0; k < 2; ++k) {
      uint8_t b[kPvBytes];
      std::memset(b, 0, sizeof(b));
      put_field(b, kPvRLo, kPvRW, static_cast<uint32_t>(in[k]));
      put_field(b, kPvGLo, kPvGW, static_cast<uint32_t>(in[k]));
      put_field(b, kPvBLo, kPvBW, static_cast<uint32_t>(in[k]));
      set_words(top.pv_bytes_i, b, kPvBytes);
      top.pv_valid_i = 1;
      top.eval();
      got[k] = static_cast<int32_t>(top.pv_r_o);
      const bool same = static_cast<int32_t>(top.pv_g_o) == in[k] &&
                        static_cast<int32_t>(top.pv_b_o) == in[k];
      zhao::check(got[k] == in[k] && same,
                  "every Gouraud channel round-trips BIT FOR BIT -- these are the "
                  "words zhao_geom_attrpack builds R234 D1's planes from, so a back "
                  "end fed from this record produces them bit-identically",
                  static_cast<int>(in[k]), static_cast<int>(got[k]));
      zhao::tick(top);
      top.pv_valid_i = 0;
    }
    zhao::check(got[0] != got[1],
                "and the two colours v1 could not distinguish come back DISTINCT -- "
                "this check fails against the v1 record by construction",
                1, got[0] != got[1] ? 1 : 0);
  }

  // ---- 2b: what MALFORMED means in v2, and the detector FIRES -----------
  // Both rules are written in design/contracts/GEOM.VERTID.md and NEITHER had
  // a detector before v2. They are reachable with legal stimulus, so no mutant
  // is needed -- the counter is seen to move here.
  {
    struct C {
      uint32_t status;
      bool legal;
      const char* why;
    };
    const C cases[] = {
        {0x00, true, "domain MESH, nothing else set"},
        // WAS `false` UNTIL 2026-09-27 AND IT ASSERTED A DEFECT.
        // 0x0F has a ZERO reserved nibble and domain 3, so it fired on the
        // domain rule alone -- the rule that refused every terrain vertex in
        // the arena. Domain 3 is TERRAIN and is legal, so this record is
        // well formed.
        {0x0F, true, "domain 3 is TERRAIN -- legal, and the reserve is zero"},
        {0x0A, true, "shared_capable + domain PARTICLE"},
        {0x03, true, "domain 3 (TERRAIN) alone -- legal"},
        // A REPLACEMENT ILLEGAL CASE, so the table still contains one that
        // fires on the rule that remains: a nonzero reserved nibble.
        {0x10, false, "reserved bit 4 set -- a malformed record"},
        {0x10, false, "a reserved bit set: nonzero [7:4] is malformed"},
        {0x80, false, "the top reserved bit"},
        {0x0C, true, "shared_capable + untextured, domain MESH"},
    };
    int bad = 0;
    int fired = 0;
    const uint32_t before = top.pv_illegal_count_o;
    for (const C& c : cases) {
      uint8_t b[kPvBytes];
      std::memset(b, 0, sizeof(b));
      put_field(b, kPvStatusLo, kPvStatusW, c.status);
      set_words(top.pv_bytes_i, b, kPvBytes);
      top.pv_valid_i = 1;
      top.eval();
      if ((top.pv_illegal_o != 0) == c.legal) {
        ++bad;
        std::printf("    status 0x%02X -> illegal=%d, expected legal=%d (%s)\n", c.status,
                    top.pv_illegal_o, c.legal ? 1 : 0, c.why);
      }
      // A refusal REPORTS; it does not correct. The byte still comes out whole.
      if (top.pv_status_o != c.status) ++bad;
      if (!c.legal) ++fired;
      zhao::tick(top);
      top.pv_valid_i = 0;
    }
    zhao::check(bad == 0,
                "a malformed status byte is refused and REPORTED not corrected -- "
                "the reserved-bits rule, which is the rule that remains, neither "
                "of which had any detector before schema v2",
                0, bad);
    zhao::check(fired > 0, "and the malformed cases are not vacuous: some were offered",
                1, fired > 0 ? 1 : 0);
    zhao::check(top.pv_illegal_count_o == before + fired,
                "and pv_illegal_o's counter MOVED by exactly that many -- it is seen "
                "to fire rather than quoted at zero",
                fired, static_cast<int>(top.pv_illegal_count_o - before));
  }

  // ---- 3: TriangleDescriptor SCHEMA v2, and the sealed vertex count -----
  {
    uint8_t b[kTdBytes];
    std::memset(b, 0, sizeof(b));
    put16(b, 0, 10);
    put16(b, 2, 20);
    put16(b, 4, 30);
    put16(b, 6, 0x0777);
    put32(b, 8, 0x12345678);
    put32(b, 12, 0xA5A5A5A5);
    // SCHEMA v2's second sixteen bytes. `area2` is s48 at bit 128 -- byte
    // aligned, so two writes -- and the value has BITS ABOVE 31 SET and is
    // NEGATIVE, because those are the two ways a carriage narrower than s48, or
    // one sign-extended from the wrong bit, would go unnoticed against a small
    // positive number.
    // 0x1ABC_DEF0_1234 is about 1.88e13, comfortably inside s48's
    // -140,737,488,355,328 .. +140,737,488,355,327 -- and the first value tried
    // here was NOT: 0x9ABC_DEF0_1234 is 1.70e14, so its negation does not fit
    // s48 at all and the check failed against a field that was storing exactly
    // what s48 can store. That is PVSCHEMA's own mistake in a new width ("my
    // s21 stimulus asked for a truncation and then complained about it"), and
    // the lesson is the same: the RTL was right and the stimulus was illegal.
    const int64_t kArea2 = -static_cast<int64_t>(0x00001ABCDEF01234ll);
    const uint64_t a2 = static_cast<uint64_t>(kArea2) & 0x0000FFFFFFFFFFFFull;
    put32(b, 16, static_cast<uint32_t>(a2 & 0xFFFFFFFFu));
    put16(b, 20, static_cast<uint16_t>((a2 >> 32) & 0xFFFFu));
    // Four DIFFERENT bounds, two of them negative, so a decoder cutting one
    // bound's bits for another fails rather than agreeing with itself.
    put_field_exact(b, 176, 12, static_cast<uint32_t>(-2048) & 0xFFFu);   // min_x
    put_field_exact(b, 188, 12, 2047u);                                   // max_x
    put_field_exact(b, 200, 12, static_cast<uint32_t>(-7) & 0xFFFu);      // min_y
    put_field_exact(b, 212, 12, 1365u);                                   // max_y

    set_words(top.td_bytes_i, b, kTdBytes);
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
    // v2's five, each at its own width and each with its sign.
    zhao::check(sx48(static_cast<uint64_t>(top.td_area2_o)) == kArea2,
                "area2 decodes as s48, NEGATIVE, with bits above 31 set", 1,
                sx48(static_cast<uint64_t>(top.td_area2_o)) == kArea2 ? 1 : 0);
    zhao::check(sx12(static_cast<uint32_t>(top.td_min_x_o)) == -2048,
                "min_x decodes as s12 at bit 176", 1,
                static_cast<int>(sx12(static_cast<uint32_t>(top.td_min_x_o))));
    zhao::check(sx12(static_cast<uint32_t>(top.td_max_x_o)) == 2047,
                "max_x decodes as s12 at bit 188 -- NOT byte aligned", 1,
                static_cast<int>(sx12(static_cast<uint32_t>(top.td_max_x_o))));
    zhao::check(sx12(static_cast<uint32_t>(top.td_min_y_o)) == -7,
                "min_y decodes as s12 at bit 200", 1,
                static_cast<int>(sx12(static_cast<uint32_t>(top.td_min_y_o))));
    zhao::check(sx12(static_cast<uint32_t>(top.td_max_y_o)) == 1365,
                "max_y decodes as s12 at bit 212 -- NOT byte aligned", 1,
                static_cast<int>(sx12(static_cast<uint32_t>(top.td_max_y_o))));
    zhao::tick(top);

    // one id past the sealed count
    const uint32_t before = top.td_illegal_count_o;
    put16(b, 4, 100);  // == sealed count, so out of range
    set_words(top.td_bytes_i, b, kTdBytes);
    top.eval();
    zhao::check(top.td_illegal_o == 1,
                "a vertex id AT the sealed count is out of range -- the count "
                "is a count, not a last index",
                1, top.td_illegal_o);
    zhao::tick(top);
    top.td_valid_i = 0;
    zhao::check(top.td_illegal_count_o == before + 1, "and is counted", 1,
                static_cast<int>(top.td_illegal_count_o - before));

    // ---- SCHEMA v2's RESERVE, SHOWN SILENT AND THEN FIRED --------------
    // `zhao_pkg` rules bits 224..255 "WRITTEN 0. Nonzero is a malformed
    // record". The status byte's own reserved nibble went the whole of v1 with
    // no detector at all, which `zhao_geom_parambuf`'s header records as the
    // thing schema v2 fixed for the vertex; this is the same law for the
    // descriptor, and it is checked rather than promised.
    put16(b, 4, 30);  // put the id back so the ONLY fault is the reserve
    set_words(top.td_bytes_i, b, kTdBytes);
    top.td_valid_i = 1;
    top.eval();
    const uint32_t q0 = top.td_illegal_count_o;
    zhao::check(top.td_illegal_o == 0,
                "NEGATIVE CONTROL: a sound v2 descriptor with a ZERO reserve is "
                "legal -- so the firing below is the reserve and not the ids",
                1, top.td_illegal_o ? 0 : 1);
    zhao::tick(top);
    put_field_exact(b, 224, 32, 0x00000001u);  // one bit, the smallest breach there is
    set_words(top.td_bytes_i, b, kTdBytes);
    top.eval();
    zhao::check(top.td_illegal_o == 1,
                "td_illegal_o FIRES on a SINGLE nonzero bit in the reserved "
                "field -- the reserve has a detector, not a promise",
                1, top.td_illegal_o);
    zhao::tick(top);
    zhao::check(top.td_illegal_count_o == q0 + 1,
                "and the reserved breach is counted exactly once", 1,
                static_cast<int>(top.td_illegal_count_o - q0));
    // Put it back and prove it STOPS. A detector that keeps firing on healthy
    // input is a stuck bit.
    put_field_exact(b, 224, 32, 0u);
    set_words(top.td_bytes_i, b, kTdBytes);
    top.eval();
    zhao::check(top.td_illegal_o == 0,
                "and it goes quiet again once the reserve is zero", 1,
                top.td_illegal_o ? 0 : 1);
    zhao::tick(top);
    top.td_valid_i = 0;
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
