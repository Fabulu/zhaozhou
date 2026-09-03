// mem_upload_oracle.cpp — the upload acceptance law, before its RTL exists.
//
// ---------------------------------------------------------------------------
// WHY NOW
// ---------------------------------------------------------------------------
// MEM.UPLOAD is the mover the animation ruling assumes and the console does
// not have: HPS DDR owns every clip bank because — the owner's words — "we
// can't fit the anims into the fast ram", and nothing today can move a byte
// from there to where the renderer reads.
//
// The contract was written first, and the owner brief
// (reports/BRO-20260903-NORMALMAP-AND-ANIMATION-PATH.md §3) then found two
// holes in it. Both are corrected in the oracle, and the point of this file is
// that the corrections are PINNED before any RTL is written against them —
// because both were the kind of hole that produces working-looking hardware.
//
// ---------------------------------------------------------------------------
// THE TWO CORRECTIONS THESE CASES EXIST FOR
// ---------------------------------------------------------------------------
// 1. A generation bit does NOT make an in-place upload atomic. If new bytes
//    are written over a slot that still advertises the old generation, a
//    consumer holding that generation reads a MIXTURE. So the upload lands in
//    a fresh unpublished slot and publication is a MAPPING update.
// 2. The source was unguarded and its width disagreed with the hardware. The
//    bridge exports a 32-bit HPS address; a descriptor with a non-zero upper
//    half is REFUSED, never truncated. And a descriptor may read only from the
//    staging arena registered for the active epoch — the first version checked
//    the destination only, which is a capability hole rather than a missing
//    bounds check.
#include <cstdint>
#include <cstdio>

#include "zhao_sim.hpp"
#include "zref/zref_mem_upload.hpp"

using namespace zref::mem;

int main() {
  // A plausible map: a 1 MiB HPS staging arena and a 256 KiB local region.
  const GuardRegion hps{0x3000'0000u, 1u << 20};
  const GuardRegion vram{0x0800'0000u, 1u << 18};

  // ---- 1: the ordinary accept -------------------------------------------
  {
    const UploadVerdict v =
        upload_verdict(vram, hps, 0x3000'0000ull, 0x0800'0000u, 4096, 7, 7);
    zhao::check(v == kUploadOk, "a well-formed request inside both regions is "
                                "accepted", kUploadOk, v);
    zhao::check(upload_bursts(4096) == 64,
                "and decomposes into exact 64-byte bursts", 64,
                static_cast<int>(upload_bursts(4096)));
  }

  // ---- 2: THE SOURCE CAPABILITY, which the first version did not have ----
  // The bridge can see a broad HPS range. A descriptor naming memory outside
  // the epoch's staging arena is not a bounds slip, it is a read of arbitrary
  // ARM or kernel memory, and it must be refused as such.
  {
    struct C { uint64_t src; uint32_t len; UploadVerdict want; const char* why; };
    const C cases[] = {
      {0x3000'0000ull,          64, kUploadOk,                 "the arena's first burst"},
      {0x300F'FFC0ull,          64, kUploadOk,                 "and its last"},
      {0x3010'0000ull,          64, kUploadSourceOutsideArena, "one burst past the end"},
      {0x2FFF'FFC0ull,          64, kUploadSourceOutsideArena, "one burst before the start"},
      {0x300F'FFC0ull,         128, kUploadSourceOutsideArena, "starts inside and RUNS OFF the end"},
      {0x0000'0001'0000'0000ull, 64, kUploadSourceUnreachable, "upper half set -- refused, never truncated"},
    };
    int bad = 0;
    for (const C& c : cases) {
      const UploadVerdict got =
          upload_verdict(vram, hps, c.src, 0x0800'0000u, c.len, 3, 3);
      if (got != c.want) {
        ++bad;
        std::printf("    src %016llx len %u -> %d, wanted %d (%s)\n",
                    (unsigned long long)c.src, c.len, got, c.want, c.why);
      }
    }
    zhao::check(bad == 0,
                "the HPS source is a CAPABILITY: only the staging arena "
                "registered for the active epoch, and an address with a "
                "non-zero upper half is refused rather than narrowed",
                0, bad);
  }

  // ---- 3: THE WRAP, on both sides ---------------------------------------
  // `addr + length` in 32 bits can wrap, and a wrapped sum compares as a small
  // number -- so a request running off the end of a region reads as
  // comfortably inside it. Both containment tests are done in 64 bits for
  // exactly this input.
  {
    const GuardRegion high{0xFFFF'0000u, 0x0001'0000u};
    zhao::check(!upload_in_guard(high, 0xFFFF'FFC0u, 128),
                "a destination whose end WRAPS past 2^32 is refused, not "
                "accepted because the wrapped sum looked small",
                0, upload_in_guard(high, 0xFFFF'FFC0u, 128) ? 1 : 0);
    zhao::check(upload_in_guard(high, 0xFFFF'FFC0u, 64),
                "while the burst that exactly fills the region is fine", 1,
                upload_in_guard(high, 0xFFFF'FFC0u, 64) ? 1 : 0);
  }

  // ---- 4: alignment and zero are MALFORMED, not fixable ------------------
  // Padding an unaligned length would write bytes the producer never staged.
  {
    int bad = 0;
    if (upload_verdict(vram, hps, 0x3000'0000ull, 0x0800'0000u, 0, 1, 1) !=
        kUploadZeroLength) ++bad;
    if (upload_verdict(vram, hps, 0x3000'0020ull, 0x0800'0000u, 64, 1, 1) !=
        kUploadUnaligned) ++bad;
    if (upload_verdict(vram, hps, 0x3000'0000ull, 0x0800'0020u, 64, 1, 1) !=
        kUploadUnaligned) ++bad;
    if (upload_verdict(vram, hps, 0x3000'0000ull, 0x0800'0000u, 65, 1, 1) !=
        kUploadUnaligned) ++bad;
    zhao::check(bad == 0,
                "zero length and every unaligned form are refused rather than "
                "padded -- padding writes bytes nobody staged",
                0, bad);
  }

  // ---- 5: THE ORDER OF THE TESTS IS PART OF THE LAW ---------------------
  // A malformed request is reported as malformed even when it is ALSO stale,
  // so a producer bug is never hidden behind an epoch that happened to close.
  {
    const UploadVerdict v =
        upload_verdict(vram, hps, 0x3000'0020ull, 0x0800'0000u, 64, 2, 9);
    zhao::check(v == kUploadUnaligned,
                "a request that is BOTH malformed and stale reports the "
                "malformation -- otherwise a producer bug hides behind a "
                "closed epoch",
                kUploadUnaligned, v);
  }

  // ---- 6: a stale epoch is refused --------------------------------------
  {
    const UploadVerdict v =
        upload_verdict(vram, hps, 0x3000'0000ull, 0x0800'0000u, 64, 4, 5);
    zhao::check(v == kUploadEpochStale,
                "an upload belonging to a closed epoch is refused -- it would "
                "write correct bytes into a slot somebody else now owns",
                kUploadEpochStale, v);
  }

  // ---- 7: THE ATOMICITY LAW ---------------------------------------------
  // The visible generation is the OLD one until the completion beat. This is
  // only sound because the bytes land in a fresh slot: the corrected law is
  // that publication is a mapping update, not an in-place overwrite.
  {
    zhao::check(upload_visible_generation(4, 5, false) == 4,
                "mid-upload a consumer sees the OLD generation", 4,
                upload_visible_generation(4, 5, false));
    zhao::check(upload_visible_generation(4, 5, true) == 5,
                "and the new one only on completion", 5,
                upload_visible_generation(4, 5, true));
  }

  // ---- 8: burst decomposition is exact and strides agree ----------------
  {
    int bad = 0;
    const uint64_t src = 0x3000'0000ull;
    const uint32_t dst = 0x0800'0000u;
    for (uint32_t n = 0; n < upload_bursts(1024); ++n) {
      if (upload_src_of(src, n) != src + n * 64ull) ++bad;
      if (upload_dst_of(dst, n) != dst + n * 64u) ++bad;
    }
    zhao::check(bad == 0 && upload_bursts(1024) == 16,
                "source and destination strides agree burst for burst -- a "
                "stride recomputed at each call site is one that can disagree "
                "with itself between the reader and the writer",
                0, bad);
  }

  return zhao::report_and_exit("mem_upload_oracle");
}
