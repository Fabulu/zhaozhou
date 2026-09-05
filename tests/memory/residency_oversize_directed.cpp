// residency_oversize_directed.cpp -- AUDIT R8.
//
// THE DEFECT. `Arena::publish` allocates ONE fixed-size page and then validates
// the upload with `upload_verdict`, passing the request's full `length` against
// a guard region covering the WHOLE ARENA:
//
//     guard_.bytes = page_bytes * page_count;
//
// So a length larger than one page is "in bounds" -- it merely runs off the end
// of the page it was given and into the next one. Nothing checks
// `length <= page_bytes_`, and nothing reserves a second page for a longer
// upload.
//
// THE COUNTEREXAMPLE, from the audit, reproduced here as a test:
//
//   1. publish A -> page 0
//   2. publish B -> page 1, and PIN B
//   3. republish A -> page 2, which frees page 0
//   4. publish C with length = 2 * page_bytes -> page 0 is chosen, and the
//      whole-arena bounds check accepts it
//
// C's accepted footprint is then [base, base + 2*page) while pinned B sits at
// [base + page, base + 2*page). They overlap, and the pin refusal counters stay
// at zero -- the model authorises a write over a page it is holding for someone
// else.
//
// WHAT THIS TEST IS AND IS NOT. The model does not copy DMA bytes, so nothing
// is actually corrupted inside it. What is demonstrated is that its
// ACCEPTANCE semantics permit overlapping storage -- and acceptance semantics
// are the whole reason this model exists. A caller might separately restrict
// upload sizes; the reference is supposed to be the thing that says so.

#include <cstdint>
#include <cstdio>

#include "zref/zref_residency.hpp"

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

constexpr uint32_t kBase = 0x1000;
constexpr uint32_t kPage = 256;
constexpr int kPages = 4;

// A guard region for the HPS side that is generous enough never to be the
// reason a publish is refused -- the point is the VRAM-side page bound.
zref::mem::GuardRegion hps_region() {
  zref::mem::GuardRegion g;
  g.base = 0;
  g.bytes = 0x100000;
  return g;
}

}  // namespace

int main() {
  namespace res = zref::residency;

  res::Arena arena(kBase, kPage, kPages);
  res::Ledger led{};
  const zref::mem::GuardRegion hps = hps_region();

  // ---- 1. A into page 0 ----------------------------------------------------
  const res::PublishResult a0 =
      arena.publish(/*resource_index=*/1, res::Kind::kTexturePage, /*hps_addr=*/0,
                    /*length=*/kPage, hps, /*request_epoch=*/1, /*current_epoch=*/1,
                    /*verify_ok=*/true, &led);
  check(a0.outcome == res::Outcome::kPublished, "A publishes", 0, static_cast<long>(a0.outcome));

  // ---- 2. B into page 1, pinned -------------------------------------------
  const res::PublishResult b0 =
      arena.publish(2, res::Kind::kTexturePage, 0, kPage, hps, 1, 1, true, &led);
  check(b0.outcome == res::Outcome::kPublished, "B publishes", 0, static_cast<long>(b0.outcome));
  const bool pinned = arena.pin(2);
  check(pinned, "B is pinned", 1, pinned ? 1 : 0);

  // ---- 3. republish A, freeing page 0 -------------------------------------
  const res::PublishResult a1 =
      arena.publish(1, res::Kind::kTexturePage, 0, kPage, hps, 1, 1, true, &led);
  check(a1.outcome == res::Outcome::kPublished, "A republishes elsewhere", 0,
        static_cast<long>(a1.outcome));

  // ---- 4. C, two pages long, into the page A vacated ----------------------
  // THE ASSERTION. A single-page allocator handed an upload twice its page size
  // must refuse it. Accepting it authorises a footprint that runs into the
  // neighbouring page, and that neighbour is PINNED.
  const uint32_t oversize = kPage * 2;
  const res::PublishResult c =
      arena.publish(3, res::Kind::kTexturePage, 0, oversize, hps, 1, 1, true, &led);

  std::printf("  oversize publish: outcome %d, length %u into a %u-byte page\n",
              static_cast<int>(c.outcome), oversize, kPage);

  check(c.outcome != res::Outcome::kPublished,
        "an upload LONGER THAN ONE PAGE is refused -- accepting it authorises a "
        "footprint that overlaps the neighbouring page, and here that neighbour "
        "is pinned",
        1, c.outcome == res::Outcome::kPublished ? 0 : 1);

  // And the refusal must be diagnosable rather than lumped in with the others:
  // a page WAS free, and the guard genuinely accepted the bounds, so neither
  // kRefusedNoStorage nor kRefusedValidation describes what happened.
  check(c.outcome == res::Outcome::kRefusedOversize,
        "and refused AS an oversize request -- a page was free and the guard "
        "accepted the bounds, so neither no-storage nor validation says what "
        "went wrong",
        static_cast<long>(res::Outcome::kRefusedOversize), static_cast<long>(c.outcome));

  check(led.refused_oversize == 1, "counted once", 1, static_cast<long>(led.refused_oversize));

  // ---- the pin is still intact --------------------------------------------
  // If the oversize publish had been accepted it would have written over B's
  // page without ever touching a pin counter, which is precisely why counting
  // pin refusals is not sufficient evidence that pins are honoured.
  check(led.write_blocked_by_pin == 0 && led.reclaim_blocked_by_pin == 0,
        "and no pin counter moved, because the request never got far enough to "
        "threaten one -- the refusal happened at the size check",
        0, static_cast<long>(led.write_blocked_by_pin + led.reclaim_blocked_by_pin));

  // ---- exactly one page still fits ----------------------------------------
  // The bound must be `> page_bytes`, not `>= page_bytes`. A full-page upload
  // is legal and common; refusing it would trade one defect for another.
  const res::PublishResult d =
      arena.publish(4, res::Kind::kTexturePage, 0, kPage, hps, 1, 1, true, &led);
  check(d.outcome == res::Outcome::kPublished,
        "an upload of EXACTLY one page still publishes -- the bound is greater "
        "than, not greater or equal",
        0, static_cast<long>(d.outcome));

  std::printf("[residency_oversize_directed] %d checks %s\n", g_checks,
              g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
