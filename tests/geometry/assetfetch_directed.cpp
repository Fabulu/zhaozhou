// assetfetch_directed.cpp — GEOM.ASSETFETCH's oracle, checked against the
// rulings it claims to mirror rather than against itself.
//
// The point of this file is the EDGES. `plan()` is short enough that a random
// sweep would pass on the first try and prove nothing; every check below is a
// boundary that a plausible wrong implementation sits on the far side of:
// 64 vs 65 vertices, a footprint ending exactly at the pool's last byte, an
// offset large enough to wrap 32 bits, and a record stream that starts
// mid-line so the line count is not simply bytes/64.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "zref/zref_assetfetch.hpp"

namespace af = zref::assetfetch;

static int g_checks = 0;
static int g_fail = 0;

static void check(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

static af::Request req(uint32_t voff, uint32_t ioff, int vc, int tc) {
  af::Request r;
  r.vertex_offset = voff;
  r.index_offset = ioff;
  r.vertex_count = static_cast<uint8_t>(vc);
  r.triangle_count = static_cast<uint8_t>(tc);
  return r;
}

// --------------------------------------------------------------------------
// 1. The ruling limits, at the boundary and one past it.
// --------------------------------------------------------------------------
static void test_limits() {
  check(af::plan(req(0, 4096, 64, 126)).admitted,
        "64 vertices / 126 triangles is the ruling maximum and is admitted");

  const af::Plan over_v = af::plan(req(0, 4096, 65, 1));
  check(!over_v.admitted && over_v.refusal == af::Refusal::kVertexCount,
        "65 vertices is refused as kVertexCount");

  const af::Plan over_t = af::plan(req(0, 4096, 1, 127));
  check(!over_t.admitted && over_t.refusal == af::Refusal::kTriangleCount,
        "127 triangles is refused as kTriangleCount");

  // The taxonomy's ORDER is part of the law: an over-count meshlet that is also
  // outside the pool reports the over-count, because that is the actionable
  // fault. A wrong order here silently reclassifies counters.
  const af::Plan both = af::plan(req(af::kAssetPoolSpan, af::kAssetPoolSpan, 65, 127));
  check(both.refusal == af::Refusal::kVertexCount,
        "over-count wins over out-of-pool: the taxonomy is ORDERED");

  // A degenerate meshlet is legal and reads nothing. It must not be refused --
  // "no triangles" is a valid meshlet, and refusing it would count a fault
  // where there is none.
  const af::Plan empty = af::plan(req(0, 0, 0, 0));
  check(empty.admitted && empty.beats == 0,
        "an empty meshlet is admitted and asks for zero beats");
}

// --------------------------------------------------------------------------
// 2. The pool edges, which is where address arithmetic goes wrong.
// --------------------------------------------------------------------------
static void test_pool_edges() {
  // A vertex block whose LAST byte is the pool's last byte. Exactly legal.
  const uint32_t vbytes = 64u * af::kVertexRecordBytes;   // 2048
  const af::Plan flush = af::plan(req(af::kAssetPoolSpan - vbytes, 0, 64, 0));
  check(flush.admitted,
        "a footprint ending exactly at the pool's last byte is admitted");

  // One byte further and it leaves the pool.
  const af::Plan over = af::plan(req(af::kAssetPoolSpan - vbytes + 1, 0, 64, 0));
  check(!over.admitted && over.refusal == af::Refusal::kOutsidePool,
        "one byte past the pool end is refused as kOutsidePool");

  // THE WRAP CASE. An offset near 2^32 makes base+offset wrap to a small
  // address, which lands INSIDE the pool if you only test the end. This is the
  // same shape as the guard's blit-wrap defect, and it is the reason the start
  // is compared against the base at all.
  const uint32_t wrapping = 0u - af::kAssetPoolBase;   // base + this == 0
  const af::Plan wrapped = af::plan(req(wrapping, 0, 1, 0));
  check(!wrapped.admitted && wrapped.refusal == af::Refusal::kOutsidePool,
        "an offset that wraps 32 bits is refused, not silently admitted at 0");

  // The index stream is checked independently of the vertex stream: a legal
  // vertex block must not launder an illegal index block.
  const af::Plan idx_bad = af::plan(req(0, af::kAssetPoolSpan - 2, 1, 1));
  check(!idx_bad.admitted && idx_bad.refusal == af::Refusal::kOutsidePool,
        "an out-of-pool INDEX stream is refused even when the vertices fit");
}

// --------------------------------------------------------------------------
// 3. Line counting. Records are not line-aligned, so this is not bytes/64.
// --------------------------------------------------------------------------
static void test_line_counting() {
  check(af::lines_covering(0, 0) == 0, "zero bytes covers zero lines");
  check(af::lines_covering(0, 1) == 1, "one byte at a line start covers one line");
  check(af::lines_covering(0, 64) == 1, "a full aligned line is one line");
  check(af::lines_covering(0, 65) == 2, "one byte past a line needs a second");

  // The case a bytes/64 implementation gets wrong: 64 bytes starting at offset
  // 1 spans TWO lines.
  check(af::lines_covering(1, 64) == 2,
        "64 bytes starting mid-line spans two lines, not one");
  check(af::lines_covering(63, 2) == 2,
        "two bytes straddling a line boundary spans two lines");

  // And the same at pool scale, through plan().
  const af::Plan p = af::plan(req(1, 0, 2, 0));   // 64 bytes at pool_base+1
  check(p.beats == 2, "plan() counts a straddling vertex block as two beats");
}

// --------------------------------------------------------------------------
// 4. Serving: the bytes come back exactly as they lie, at the right addresses.
// --------------------------------------------------------------------------
static void test_serving() {
  // A pool image big enough for the offsets used here, filled with a position
  // dependent pattern so a WRONG address cannot coincidentally match.
  //
  // THE HIGH BYTE IS IN THE MIX ON PURPOSE. The first version of this was
  // `i * 31 + 7`, which is constant modulo 256 at every 256-ALIGNED offset --
  // so the two streams at 1024 and 8192 held the identical byte and the
  // distinctness check below failed against a correct oracle. A pattern that
  // cannot distinguish positions cannot prove positions are distinct, which is
  // the same class of mistake as verifying a duplicate against itself.
  const uint32_t kImage = 1u << 16;
  std::vector<uint8_t> pool(af::kAssetPoolBase + kImage, 0);
  for (uint32_t i = 0; i < kImage; ++i) {
    pool[af::kAssetPoolBase + i] =
        static_cast<uint8_t>((i * 31u + (i >> 8) * 131u + 7u) & 0xFFu);
  }

  const uint32_t voff = 1024, ioff = 8192;
  const af::Plan p = af::plan(req(voff, ioff, 64, 126));
  check(p.admitted, "the serving fixture is admitted");

  // Every triplet, against the pool image read directly.
  bool triplets_ok = true;
  for (uint32_t n = 0; n < 126; ++n) {
    const af::Triplet t = af::triplet(p, pool.data(), n);
    const uint32_t at = af::kAssetPoolBase + ioff + n * 3;
    if (t.a != pool[at] || t.b != pool[at + 1] || t.c != pool[at + 2]) {
      triplets_ok = false;
      break;
    }
  }
  check(triplets_ok, "all 126 triplets read the right three bytes");

  // Every vertex record, byte for byte.
  bool records_ok = true;
  for (uint32_t v = 0; v < 64; ++v) {
    uint8_t got[af::kVertexRecordBytes];
    af::vertex_record(p, pool.data(), v, got);
    const uint32_t at = af::kAssetPoolBase + voff + v * af::kVertexRecordBytes;
    if (std::memcmp(got, &pool[at], af::kVertexRecordBytes) != 0) {
      records_ok = false;
      break;
    }
  }
  check(records_ok, "all 64 vertex records are 32 bytes copied exactly");

  // The two streams must not alias: triplet 0 and vertex 0 come from different
  // addresses. A block that confused the two offsets would pass every check
  // above if the pattern were uniform, which is why the pattern is not.
  const af::Triplet t0 = af::triplet(p, pool.data(), 0);
  uint8_t v0[af::kVertexRecordBytes];
  af::vertex_record(p, pool.data(), 0, v0);
  check(t0.a != v0[0], "the index and vertex streams are read from distinct addresses");
}

int main() {
  test_limits();
  test_pool_edges();
  test_line_counting();
  test_serving();

  if (g_fail != 0) {
    std::printf("[assetfetch_directed] %d of %d checks FAILED\n", g_fail, g_checks);
    return 1;
  }
  std::printf("[assetfetch_directed] %d checks passed\n", g_checks);
  return 0;
}
