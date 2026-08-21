// zref_progcache.hpp — FIELD.PROGCACHE's reference model.
//
// The ledger declared `zref::ProgCache` and that symbol never existed: one of
// the twenty-five phantoms audited in reports/PHANTOM_REFERENCES.md. This block
// is a HYBRID of the three kinds that audit identified, and the split matters:
//
//   * THE VALIDATION HALF IS ALREADY LAW. `zfield::decode` loads and fully
//     re-validates a `.zprog` byte image against spec/form/field-ir.md §4/§5,
//     with thirteen named error classes (V1..V12 plus bad magic). It is the
//     single implementation of that law and this file FORWARDS to it. Writing a
//     second validator here would be exactly the failure the phantom-reference
//     rules exist to catch.
//   * THE CACHE HALF HAS NO LAW ANYWHERE. `design/blocks.yml` gives one
//     sentence — "Cache Field IR microprograms + constant tables with
//     hash/version check; a program that fails validation is safely rejected
//     (charter §19.4), never executed" — and field-ir.md says nothing about
//     residency at all. So the policy below is CHOSEN, and every choice is
//     recorded with the alternative it beat.
//
// This block does NOT evaluate programs, which keeps it clear of field-ir.md
// §1's grep-audit law ("no RTL-side re-derivation ahead of the profile engine").
// It decides what is resident and what is rejected; execution is the sequencers'.
//
// ---------------------------------------------------------------------------
// THE CHOSEN POLICY
// ---------------------------------------------------------------------------
//
// 1. KEYED BY `program_hash`, WHICH IS CONTENT, NOT IDENTITY. `decode` computes
//    `program_hash` from the bytes, so two images with the same hash are the
//    same program. REJECTED: keying by `source_id`, which is authoring identity
//    — two different programs can legitimately carry one source id, and a hit on
//    that key would execute the wrong program while every counter looked healthy.
//
// 2. A REJECTED PROGRAM IS NEVER CACHED, AND REJECTION IS NOT REMEMBERED.
//    The ledger's sentence gives the first half literally. The second half is
//    chosen: a rejected image is re-validated every time it is offered.
//    REJECTED: a negative cache. It would save re-validating a program that
//    should not be running at all, and it would make a program that BECOMES
//    valid — patched between frames, or a partially-written upload completing —
//    stay rejected until something evicted the memory of it. The cost of the
//    choice is real and is stated rather than hidden: an image that fails
//    validation costs a full decode every time it is offered.
//
// 3. FULLY ASSOCIATIVE WITH LRU EVICTION, `kEntries` entries.
//    REJECTED: direct-mapped on the hash's low bits, which is one comparator
//    instead of `kEntries` — and which makes two programs whose hashes collide
//    in those bits evict each other on every alternate use, for as long as both
//    are live. A field program is re-used every frame by every sequencer that
//    references it, so that thrash is not a corner case, it is the steady state.
//
// 4. A HIT DOES NOT RE-VALIDATE, AND THE LOOKUP KEY ARRIVES WITH THE REQUEST.
//    The ledger gives this block `inputs: [program_bytes, hash_check]`, so the
//    hash comes from the command rather than from a decode the cache performs
//    first. That is what makes a hit cheap. Validation happens once, on the way
//    in, and the residency store is hardware-owned afterwards.
//    REJECTED: deriving the hash locally by decoding every image before the
//    lookup. It removes the need to trust the declared hash, and it makes the
//    block pointless — a cache whose hit path costs a full validation has saved
//    nothing. The trust model that replaces it is stated on `acquire`: verified
//    on a miss, trusted on a hit, and a hash only ever enters the directory from
//    a decode that computed it.
//
// 5. VERSION AND HASH CHECKING BELONG TO `decode`, NOT HERE. The ledger's
//    "hash/version check" is `kBadVersion` and `kBadHash`, both already inside
//    `zfield::decode`. Re-checking them here would be a second implementation of
//    two of the thirteen validation rules, and the one most likely to drift.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "zfield/zfield.hpp"

namespace zref {
namespace field {

/** What an acquire did. */
enum class ProgStatus : uint8_t {
  kHit = 0,       // already resident and validated; nothing was decoded
  kInserted = 1,  // validated and inserted, evicting the LRU entry if needed
  kRejected = 2,  // failed validation: NOT cached, NOT executable
};

struct AcquireResult {
  ProgStatus status = ProgStatus::kRejected;
  int slot = -1;                                             // valid for kHit/kInserted
  ::zfield::DecodeError error = ::zfield::DecodeError::kOk;  // why, for kRejected
  uint32_t program_hash = 0;                                 // 0 when rejected
};

/**
 * The resident program directory.
 *
 * Deliberately small: field programs are a few hundred bytes of microcode plus
 * their constant tables, and the working set is the programs referenced by live
 * spells and terrain effects, not the whole cartridge.
 */
class ProgCache {
 public:
  static constexpr int kEntries = 16;

  struct Counters {
    uint32_t hits = 0;
    uint32_t misses = 0;             // an acquire that had to decode
    uint32_t programs_rejected = 0;  // decode said no
    uint32_t evictions = 0;
  };

  explicit ProgCache(int entries = kEntries) : entries_(entries) {
    slots_.resize(static_cast<size_t>(entries));
  }

  int entries() const { return entries_; }

  /**
   * Offer one `.zprog` image together with the hash the command declared.
   *
   * `design/blocks.yml` gives this block `inputs: [program_bytes, hash_check]`,
   * so the hash arrives WITH the request; it is not something the cache derives
   * for itself before it can look anything up. That is what makes the cache
   * worth having: a hit costs a lookup, not a decode.
   *
   * THE TRUST MODEL, stated because it is the one thing here worth getting
   * wrong:
   *
   *   * ON A MISS the declared hash is VERIFIED, not trusted. `zfield::decode`
   *     computes the hash from the bytes and returns `kBadHash` when it does not
   *     match, so nothing enters the cache under a hash it does not have.
   *   * ON A HIT the declared hash is trusted, because the program it names was
   *     verified on the way in. A caller that declares a hash belonging to a
   *     DIFFERENT resident program gets that program. This is the ordinary trust
   *     model of any content-addressed cache and it is bounded by the same
   *     thing: a hash is only ever entered by a decode that computed it.
   *
   * An earlier draft removed the declared hash and decoded on every acquire.
   * That is safer in the narrow sense and it makes the block pointless -- a
   * cache whose hit path costs a full validation has not saved anything.
   */
  AcquireResult acquire(const uint8_t* bytes, std::size_t n, uint32_t declared_hash) {
    const int hit = find(declared_hash);
    if (hit >= 0) {
      ++ctr_.hits;
      slots_[static_cast<size_t>(hit)].lru = ++lru_ctr_;
      AcquireResult r;
      r.status = ProgStatus::kHit;
      r.slot = hit;
      r.program_hash = declared_hash;
      return r;
    }
    ::zfield::DecodeResult d = ::zfield::decode(bytes, n);
    if (d.error != ::zfield::DecodeError::kOk) {
      // Policy 2: not cached, not remembered, counted.
      ++ctr_.programs_rejected;
      AcquireResult r;
      r.status = ProgStatus::kRejected;
      r.error = d.error;
      return r;
    }
    return place(d.prog);
  }

  bool resident(uint32_t hash) const {
    for (int i = 0; i < entries_; ++i) {
      const Slot& s = slots_[static_cast<size_t>(i)];
      if (s.valid && s.hash == hash) return true;
    }
    return false;
  }

  int find(uint32_t hash) const {
    for (int i = 0; i < entries_; ++i) {
      const Slot& s = slots_[static_cast<size_t>(i)];
      if (s.valid && s.hash == hash) return i;
    }
    return -1;
  }

  const ::zfield::Decoded& at(int slot) const { return slots_[static_cast<size_t>(slot)].prog; }

  const Counters& counters() const { return ctr_; }

  std::size_t occupied() const {
    std::size_t n = 0;
    for (int i = 0; i < entries_; ++i) {
      if (slots_[static_cast<size_t>(i)].valid) ++n;
    }
    return n;
  }

  void clear() {
    for (Slot& s : slots_) s = Slot{};
    lru_ctr_ = 0;
    ctr_ = Counters{};
  }

 private:
  struct Slot {
    bool valid = false;
    uint32_t hash = 0;
    uint64_t lru = 0;
    ::zfield::Decoded prog{};
  };

  AcquireResult place(const ::zfield::Decoded& prog) {
    ++ctr_.misses;
    int victim = -1;
    for (int i = 0; i < entries_; ++i) {
      if (!slots_[static_cast<size_t>(i)].valid) {
        victim = i;
        break;
      }
    }
    if (victim < 0) {
      uint64_t best = UINT64_MAX;
      for (int i = 0; i < entries_; ++i) {
        if (slots_[static_cast<size_t>(i)].lru < best) {
          best = slots_[static_cast<size_t>(i)].lru;
          victim = i;
        }
      }
      ++ctr_.evictions;
    }
    Slot& s = slots_[static_cast<size_t>(victim)];
    s.valid = true;
    s.hash = prog.program_hash;
    s.lru = ++lru_ctr_;
    s.prog = prog;
    AcquireResult r;
    r.status = ProgStatus::kInserted;
    r.slot = victim;
    r.program_hash = s.hash;
    return r;
  }

  int entries_;
  std::vector<Slot> slots_;
  uint64_t lru_ctr_ = 0;
  Counters ctr_{};
};

}  // namespace field
}  // namespace zref
