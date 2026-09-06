// island_stream_directed.cpp -- an 8 km island streamed, evicted, and returned to.
//
// The audit's closing instruction: "connect asset/resource/terrain lifecycle
// models to the frame route and prove representative complete scenes,
// including eviction and return."
//
// EVICTION AND RETURN IS THE TEST, not streaming. Streaming a set in proves
// little: the first frame publishes whatever is visible and nothing is under
// pressure. The failures live where a patch leaves the view, loses its page to
// someone else, and then comes BACK -- reclamation, generation bump and
// republication in the order they really occur, which is exactly where a stale
// handle would still match if a generation were quietly reused.
//
// So the camera here does not merely move. It crosses the island and comes
// home, and the test requires that patches returned to are recognised as the
// SAME resources rather than as new ones.

#include <cstdint>
#include <cstdio>

#include "zref/zref_island_stream.hpp"

namespace {

int g_checks = 0;
bool g_failed = false;

void check(bool ok, const char* what, long want, long got) {
  ++g_checks;
  if (!ok) {
    g_failed = true;
    std::printf("FAIL: %s: expected %ld, got %ld\n", what, want, got);
  }
}

constexpr int32_t kSide = 125;   // 8 km at 64 m patches
constexpr int32_t kPages = 256;  // a residency far smaller than the island
constexpr uint32_t kPageBytes = 256;

}  // namespace

int main() {
  namespace isl = zref::island;
  namespace res = zref::residency;

  isl::Desc d;
  d.island_id = 0x515Au;
  d.pitch_log2 = 1;
  d.extent_ix = static_cast<uint16_t>(kSide);
  d.extent_iz = static_cast<uint16_t>(kSide);
  d.origin_y = 40 << 16;

  // Solid ground: a broad band across the island, so a camera crossing it is
  // continuously over terrain rather than over sky.
  isl::Directory dir(d);
  uint32_t h = 1;
  for (int32_t iz = 40; iz < 85; ++iz)
    for (int32_t ix = 0; ix < kSide; ++ix) dir.set(ix, iz, h++);

  std::printf("  island %d x %d, solid %zu patches, residency %d pages\n", kSide, kSide,
              dir.resident_count(), kPages);
  check(static_cast<int64_t>(dir.resident_count()) > kPages * 20,
        "the island's ground is more than twenty times the residency, so the "
        "camera cannot simply hold all of it and eviction is forced rather "
        "than incidental",
        1, static_cast<int64_t>(dir.resident_count()) > kPages * 20 ? 1 : 0);

  res::Arena arena(0x100000, kPageBytes, kPages);
  res::Ledger L{};
  isl::Streamer st(dir, arena);
  zref::mem::GuardRegion hps{0, 0x1000000};
  st.configure(kPageBytes, hps);

  // ---- walk east across the island, then come home ------------------------
  const int32_t radius = 4;  // a 9x9 patch window
  uint32_t total_published = 0, total_evicted = 0, total_returned = 0, total_refused = 0;

  for (int32_t pass = 0; pass < 2; ++pass) {
    for (int32_t step = 0; step < 40; ++step) {
      const int32_t cx = (pass == 0) ? (10 + step * 2) : (88 - step * 2);
      isl::View v;
      v.centre_ix = cx;
      v.centre_iz = 62;
      v.radius = radius;
      const isl::Stats s = st.update(v, &L);
      total_published += s.published;
      total_evicted += s.evicted;
      total_returned += s.returned;
      total_refused += s.refused;
    }
  }

  std::printf(
      "  after 80 frames: published %u, evicted %u, RETURNED %u, refused %u; "
      "live %zu, peak %u\n",
      total_published, total_evicted, total_returned, total_refused, st.live_count(), st.peak());

  check(total_published > 0, "patches streamed in", 1, total_published > 0 ? 1 : 0);
  check(total_evicted > 0,
        "and patches were EVICTED -- the camera outran the residency, which is "
        "the ordinary case for an island this size and the reason the "
        "directory is sparse",
        1, total_evicted > 0 ? 1 : 0);

  // THE ONE THAT MATTERS. A run with evictions but no returns has not tested
  // the interesting half: reclaim, bump, republish, in that order.
  check(total_returned > 0,
        "and patches EVICTED EARLIER CAME BACK when the camera returned -- "
        "reclamation, generation bump and republication exercised in the order "
        "they actually occur, which is where a silently reused generation would "
        "let a stale handle still match",
        1, total_returned > 0 ? 1 : 0);

  check(total_refused == 0,
        "with nothing refused for want of storage -- evicting BEFORE publishing "
        "means a view that merely shifts never demands a page it already owns",
        0, static_cast<long>(total_refused));

  check(st.peak() <= static_cast<uint32_t>(kPages),
        "and the resident set never exceeded the page count", kPages, static_cast<long>(st.peak()));

  check(L.refused_no_storage == 0, "the arena never ran out", 0,
        static_cast<long>(L.refused_no_storage));
  check(L.refused_oversize == 0, "and never saw an oversize upload", 0,
        static_cast<long>(L.refused_oversize));

  // ---- a pinned patch is not evicted --------------------------------------
  // The pin is the promise that a frame in flight may still read the page.
  // Streaming must not quietly break it, and the refusal must be COUNTED --
  // a pin that is honoured silently is indistinguishable from one that is not.
  {
    isl::View v;
    v.centre_ix = 20;
    v.centre_iz = 62;
    v.radius = radius;
    st.update(v, &L);

    const uint32_t idx = isl::Streamer::resource_index(d.island_id, 20, 62);
    const bool pinned = arena.pin(idx);
    check(pinned, "a visible patch can be pinned", 1, pinned ? 1 : 0);

    const uint32_t before = L.reclaim_blocked_by_pin;
    isl::View far_v;
    far_v.centre_ix = 100;
    far_v.centre_iz = 62;
    far_v.radius = radius;
    st.update(far_v, &L);

    check(L.reclaim_blocked_by_pin > before,
          "and moving the camera away does NOT evict it -- the refusal is "
          "counted, because a pin honoured silently looks exactly like a pin "
          "that was ignored",
          1, L.reclaim_blocked_by_pin > before ? 1 : 0);
    check(arena.mapping(idx) != nullptr, "the pinned patch is still mapped after the camera left",
          1, arena.mapping(idx) != nullptr ? 1 : 0);
  }

  // ---- TWO ISLANDS, ONE LOCAL COORDINATE ----------------------------------
  // The residency key used to be (iz << 16) | ix -- patch coordinates alone.
  // TERRAIN.RESIDENCY's ledger entry records that exact shape as ALREADY
  // SUPERSEDED, and says why in its own words: "two islands may legally overlap
  // in local patch coordinates". Its canonical key carries the island id.
  //
  // A collision here does not look like a crash. It looks like one island's
  // page answering for another island's patch -- a stale handle that still
  // matches, which is the precise failure the evict-and-return path above
  // exists to detect and would therefore be masked by.
  {
    const uint32_t a = isl::Streamer::resource_index(0x51u, 20, 62);
    const uint32_t b = isl::Streamer::resource_index(0x52u, 20, 62);
    check(a != b,
          "two DIFFERENT islands sharing a local patch coordinate get different "
          "resource indices -- the island id is in the key, as "
          "TERRAIN.RESIDENCY's canonical key already required",
          1, a != b ? 1 : 0);

    const uint32_t same = isl::Streamer::resource_index(0x51u, 20, 62);
    check(a == same,
          "and the same island's same patch is still the SAME resource, so a "
          "patch returning after eviction keeps its identity rather than "
          "getting a fresh one",
          1, a == same ? 1 : 0);

    // Neighbouring coordinates must not alias either -- a field-packed key
    // gets that wrong by one shift, silently.
    check(isl::Streamer::resource_index(0x51u, 21, 62) != a &&
              isl::Streamer::resource_index(0x51u, 20, 63) != a,
          "and neighbours in x and z are distinct from it, so the packing is "
          "not off by a shift",
          1, 1);

    check(isl::Streamer::key_fits(0x51u, 124, 124) && !isl::Streamer::key_fits(0x51u, 4096, 0),
          "the key's field widths are CHECKED rather than assumed -- an island "
          "larger than the key can address must be told, not aliased",
          1, 1);
  }

  std::printf("[island_stream_directed] %d checks %s\n", g_checks, g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
