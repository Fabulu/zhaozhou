// field_progcache_directed.cpp — FIELD.PROGCACHE against `zref::field::ProgCache`.
//
// REAL PROGRAMS, NOT SYNTHETIC HASHES. The three committed `.zprog` images
// (crater_ring, impact_wave, wave_pool) are the fixtures, and the rejection
// cases are those images CORRUPTED — a flipped body byte, a broken magic, a
// truncated tail. That matters because the block's one hard sentence is "a
// program that fails validation is safely rejected, never executed", and a test
// that invented an `ok = false` flag would be asserting its own opinion about
// what failure looks like rather than the loader's.
//
// The cache is instantiated with TWO entries here. Sixteen is the shipping size
// and there are only three real programs to fill it with; at two entries three
// programs exercise every residency law there is — fill, hit, evict, and the LRU
// choice between two live candidates — using images the loader actually accepts.
// A larger cache driven by made-up hashes would test more slots and less law.
//
// THE SPLIT THIS FILE RESPECTS. The RTL owns residency and takes the decode
// verdict as one bit; `zfield::decode` owns validation. So the testbench decodes
// (exactly as the real caller will), hands the RTL the verdict, and hands the
// reference the bytes. If they agree on hits, misses, rejections, evictions,
// occupancy and slot choice, the directory is right.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_progcache.h"

#include "crater_ring.hpp"
#include "impact_wave.hpp"
#include "wave_pool.hpp"

#include "zhao_sim.hpp"
#include "zfield/zfield.hpp"
#include "zref/zref_progcache.hpp"

namespace {

using zhao::check;
namespace zf = zref::field;

constexpr int kEntries = 2;  // must match the RTL parameter used below

struct Image {
  const char* name;
  std::vector<uint8_t> bytes;
  uint32_t hash;
};

Image image_of(const char* name, const uint8_t* p, size_t n, uint32_t h) {
  Image im;
  im.name = name;
  im.bytes.assign(p, p + n);
  im.hash = h;
  return im;
}

/** A corrupted copy: still an image, no longer a valid one. */
Image corrupt(const Image& src, const char* name, size_t byte, uint8_t xor_with) {
  Image im = src;
  im.name = name;
  im.bytes[byte] ^= xor_with;
  return im;
}

Image truncated(const Image& src, const char* name) {
  Image im = src;
  im.name = name;
  im.bytes.pop_back();
  return im;
}

class Dut {
 public:
  explicit Dut(Vzhao_field_progcache& d) : dut_(d) { reset(); }

  void reset() {
    dut_.rst_n = 0;
    dut_.lu_valid_i = 0;
    dut_.cm_valid_i = 0;
    dut_.lu_resp_ready_i = 1;
    dut_.cm_resp_ready_i = 1;
    dut_.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(dut_);
    dut_.rst_n = 1;
    dut_.eval();
  }

  /** Phase A. Returns true on a hit, and writes the slot. */
  bool lookup(uint32_t hash, int& slot) {
    dut_.lu_valid_i = 1;
    dut_.lu_hash_i = hash;
    dut_.lu_resp_ready_i = 1;
    dut_.eval();
    int guard = 0;
    while (!dut_.lu_ready_o && guard++ < 64) {
      zhao::tick(dut_);
      dut_.eval();
    }
    zhao::tick(dut_);
    dut_.lu_valid_i = 0;
    dut_.eval();
    guard = 0;
    while (!dut_.lu_resp_valid_o && guard++ < 64) {
      zhao::tick(dut_);
      dut_.eval();
    }
    const bool hit = dut_.lu_hit_o != 0;
    slot = static_cast<int>(dut_.lu_slot_o);
    zhao::tick(dut_);
    dut_.eval();
    return hit;
  }

  /** Phase B. Returns true if inserted, and writes the slot and evicted flag. */
  bool commit(uint32_t hash, bool ok, int& slot, bool& evicted) {
    dut_.cm_valid_i = 1;
    dut_.cm_hash_i = hash;
    dut_.cm_ok_i = ok ? 1 : 0;
    dut_.cm_resp_ready_i = 1;
    dut_.eval();
    int guard = 0;
    while (!dut_.cm_ready_o && guard++ < 64) {
      zhao::tick(dut_);
      dut_.eval();
    }
    zhao::tick(dut_);
    dut_.cm_valid_i = 0;
    dut_.eval();
    guard = 0;
    while (!dut_.cm_resp_valid_o && guard++ < 64) {
      zhao::tick(dut_);
      dut_.eval();
    }
    const bool ins = dut_.cm_inserted_o != 0;
    slot = static_cast<int>(dut_.cm_slot_o);
    evicted = dut_.cm_evicted_o != 0;
    zhao::tick(dut_);
    dut_.eval();
    return ins;
  }

  uint32_t hits() const { return dut_.hits_o; }
  uint32_t misses() const { return dut_.misses_o; }
  uint32_t rejected() const { return dut_.programs_rejected_o; }
  uint32_t evictions() const { return dut_.evictions_o; }
  uint32_t occupancy() const { return dut_.occupancy_o; }

 private:
  Vzhao_field_progcache& dut_;
};

/**
 * One acquire, on both sides. The testbench decodes -- exactly as the real
 * caller will -- and hands the RTL a verdict while the reference gets bytes.
 */
void acquire(Dut& dut, zf::ProgCache& ref, const Image& im, const char* what) {
  // The reference's declared hash is the one the command carries. For a valid
  // image that is the real hash; for a corrupted one it is whatever the command
  // claimed, and the decode is what catches the mismatch.
  const zf::AcquireResult want = ref.acquire(im.bytes.data(), im.bytes.size(), im.hash);

  int slot = -1;
  const bool hit = dut.lookup(im.hash, slot);
  const std::string t(what);

  if (hit) {
    check(want.status == zf::ProgStatus::kHit, (t + ": both say HIT").c_str(), 1,
          want.status == zf::ProgStatus::kHit ? 1 : 0);
    check(slot == want.slot, (t + ": the same slot").c_str(), static_cast<uint64_t>(want.slot),
          static_cast<uint64_t>(slot));
    return;
  }

  check(want.status != zf::ProgStatus::kHit, (t + ": both say MISS").c_str(), 1,
        want.status != zf::ProgStatus::kHit ? 1 : 0);

  // The caller decodes on a miss. This is the only place validation happens.
  const zfield::DecodeResult d = zfield::decode(im.bytes.data(), im.bytes.size());
  const bool ok = d.error == zfield::DecodeError::kOk;

  int cslot = -1;
  bool evicted = false;
  const bool inserted = dut.commit(ok ? d.prog.program_hash : im.hash, ok, cslot, evicted);

  if (!ok) {
    check(!inserted, (t + ": a rejected program is not inserted").c_str(), 0, inserted ? 1 : 0);
    check(want.status == zf::ProgStatus::kRejected, (t + ": the oracle rejected it too").c_str(), 1,
          want.status == zf::ProgStatus::kRejected ? 1 : 0);
  } else {
    check(inserted, (t + ": a valid program is inserted").c_str(), 1, inserted ? 1 : 0);
    check(want.status == zf::ProgStatus::kInserted, (t + ": the oracle inserted it too").c_str(), 1,
          want.status == zf::ProgStatus::kInserted ? 1 : 0);
    check(cslot == want.slot, (t + ": the same slot was chosen").c_str(),
          static_cast<uint64_t>(want.slot), static_cast<uint64_t>(cslot));
  }
}

void compare_counters(Dut& dut, const zf::ProgCache& ref, const char* what) {
  const std::string t(what);
  const zf::ProgCache::Counters& c = ref.counters();
  check(dut.hits() == c.hits, (t + ": hits").c_str(), c.hits, dut.hits());
  check(dut.misses() == c.misses, (t + ": misses").c_str(), c.misses, dut.misses());
  check(dut.rejected() == c.programs_rejected, (t + ": programs_rejected").c_str(),
        c.programs_rejected, dut.rejected());
  check(dut.evictions() == c.evictions, (t + ": evictions").c_str(), c.evictions, dut.evictions());
  check(dut.occupancy() == ref.occupied(), (t + ": occupancy").c_str(),
        static_cast<uint64_t>(ref.occupied()), dut.occupancy());
}

// PCG RXS-M-XS, the committed test PRNG shape (qformats §7.5).
struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

}  // namespace

int main(int argc, char** argv) {
  Vzhao_field_progcache raw;
  Dut dut(raw);

  const Image crater =
      image_of("crater_ring", zfield_gen::crater_ring::kProgramBytes.data(),
               zfield_gen::crater_ring::kProgramBytesLen, zfield_gen::crater_ring::kProgramHash);
  const Image impact =
      image_of("impact_wave", zfield_gen::impact_wave::kProgramBytes.data(),
               zfield_gen::impact_wave::kProgramBytesLen, zfield_gen::impact_wave::kProgramHash);
  const Image pool =
      image_of("wave_pool", zfield_gen::wave_pool::kProgramBytes.data(),
               zfield_gen::wave_pool::kProgramBytesLen, zfield_gen::wave_pool::kProgramHash);

  // The fixtures only mean anything if the three programs are actually distinct.
  check(
      crater.hash != impact.hash && impact.hash != pool.hash && crater.hash != pool.hash,
      "the three fixture programs have three distinct hashes", 1,
      (crater.hash != impact.hash && impact.hash != pool.hash && crater.hash != pool.hash) ? 1 : 0);

  // ---- 1. cold miss, insert, then a hit -----------------------------------
  {
    zf::ProgCache ref(kEntries);
    dut.reset();
    acquire(dut, ref, crater, "first acquire of crater_ring");
    acquire(dut, ref, crater, "second acquire of crater_ring");
    acquire(dut, ref, crater, "third acquire of crater_ring");
    compare_counters(dut, ref, "one program: one miss, two hits");
    check(dut.misses() == 1, "exactly one decode was demanded", 1, dut.misses());
    check(dut.hits() == 2, "and the rest were hits", 2, dut.hits());
  }

  // ---- 2. two programs fill the cache -------------------------------------
  {
    zf::ProgCache ref(kEntries);
    dut.reset();
    acquire(dut, ref, crater, "fill: crater_ring");
    acquire(dut, ref, impact, "fill: impact_wave");
    acquire(dut, ref, crater, "both resident: crater_ring hits");
    acquire(dut, ref, impact, "both resident: impact_wave hits");
    compare_counters(dut, ref, "two programs fill a two-entry cache");
    check(dut.occupancy() == 2, "the cache is full", 2, dut.occupancy());
    check(dut.evictions() == 0, "and nothing was evicted", 0, dut.evictions());
  }

  // ---- 3. THE LRU CHOICE --------------------------------------------------
  // Fill with crater and impact, touch crater so it is the younger, then bring
  // in a third. IMPACT must go. A most-recently-used policy would take crater,
  // and the re-acquire below is what tells them apart -- the totals alone would
  // not, because either choice gives one eviction and one later miss.
  {
    zf::ProgCache ref(kEntries);
    dut.reset();
    acquire(dut, ref, crater, "lru: insert crater_ring");
    acquire(dut, ref, impact, "lru: insert impact_wave");
    acquire(dut, ref, crater, "lru: touch crater_ring, making impact_wave the oldest");
    acquire(dut, ref, pool, "lru: insert wave_pool, evicting the oldest");
    check(dut.evictions() == 1, "exactly one eviction", 1, dut.evictions());

    // The asymmetric probe: ask only about the one that should have survived.
    acquire(dut, ref, crater, "lru: crater_ring survived and HITS");
    check(dut.hits() == 2, "crater_ring was still resident -- the OLDEST was evicted", 2,
          dut.hits());
    // And the one that should have gone.
    acquire(dut, ref, impact, "lru: impact_wave was evicted and misses");
    compare_counters(dut, ref, "LRU picks the same victim as the oracle");
  }

  // ---- 4. A PROGRAM THAT FAILS VALIDATION IS REJECTED, NEVER CACHED -------
  // The block's one hard sentence, exercised with three genuinely different
  // failure classes rather than an invented flag: a corrupted body (kBadCrc),
  // broken magic (kBadMagic) and a truncated image (kBadLength).
  {
    zf::ProgCache ref(kEntries);
    dut.reset();
    const Image bad_crc = corrupt(crater, "corrupted body", 27, 0xFF);
    const Image bad_magic = corrupt(crater, "broken magic", 0, 'X' ^ 'Z');
    const Image short_img = truncated(crater, "truncated image");

    // The loader really does reject all three, for three different reasons.
    check(zfield::decode(bad_crc.bytes.data(), bad_crc.bytes.size()).error ==
              zfield::DecodeError::kBadCrc,
          "fixture: the corrupted body is a CRC failure", 1, 1);
    check(zfield::decode(short_img.bytes.data(), short_img.bytes.size()).error ==
              zfield::DecodeError::kBadLength,
          "fixture: the truncated image is a length failure", 1, 1);

    acquire(dut, ref, bad_crc, "reject: corrupted body");
    acquire(dut, ref, bad_magic, "reject: broken magic");
    acquire(dut, ref, short_img, "reject: truncated");
    check(dut.rejected() == 3, "three rejections", 3, dut.rejected());
    check(dut.occupancy() == 0, "and NOTHING was cached", 0, dut.occupancy());
    check(dut.misses() == 0, "a rejection is not a miss", 0, dut.misses());
    compare_counters(dut, ref, "three rejections disturb nothing");

    // A valid program still works afterwards, in slot 0 -- the rejections did
    // not consume a slot or move the LRU.
    acquire(dut, ref, crater, "a valid program after three rejections");
    check(dut.occupancy() == 1, "which occupies exactly one slot", 1, dut.occupancy());
  }

  // ---- 5. REJECTION IS NOT REMEMBERED -------------------------------------
  // Chosen policy 2. The same bad image offered twice is re-validated and
  // rejected twice. A negative cache would count one rejection and a later hit,
  // and would keep a program rejected after it became valid.
  {
    zf::ProgCache ref(kEntries);
    dut.reset();
    const Image bad = corrupt(impact, "corrupted impact_wave", 31, 0x5A);
    acquire(dut, ref, bad, "rejected once");
    acquire(dut, ref, bad, "rejected again, not remembered");
    acquire(dut, ref, bad, "and again");
    check(dut.rejected() == 3, "every offer of a bad image is re-validated and re-counted", 3,
          dut.rejected());
    check(dut.hits() == 0, "a rejected program never becomes a hit", 0, dut.hits());
    compare_counters(dut, ref, "rejection is not remembered");
  }

  // ---- 6. a rejection does not disturb a resident program -----------------
  {
    zf::ProgCache ref(kEntries);
    dut.reset();
    acquire(dut, ref, crater, "resident before the bad image");
    acquire(dut, ref, impact, "and a second resident");
    const Image bad = corrupt(pool, "corrupted wave_pool", 40, 0xC3);
    acquire(dut, ref, bad, "a bad image against a FULL cache");
    check(dut.evictions() == 0, "a rejected program evicts nothing, even when the cache is full", 0,
          dut.evictions());
    acquire(dut, ref, crater, "the first resident still hits");
    acquire(dut, ref, impact, "and so does the second");
    compare_counters(dut, ref, "a rejection against a full cache");
  }

  // ---- 7. random sequences over the real fixtures -------------------------
  // The pool is the three real programs and three corrupted variants, so every
  // sequence mixes hits, misses, evictions and rejections in an order no
  // directed case would think to write. The counters and the slot choice must
  // agree with the oracle after every single acquire, not merely at the end.
  {
    bool random_mode = false;
    uint32_t iters = 0;
    for (int i = 1; i < argc; ++i) {
      if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
        random_mode = true;
        iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
      }
    }
    if (random_mode) {
      std::vector<Image> pool_imgs = {
          crater,
          impact,
          pool,
          corrupt(crater, "bad crater", 27, 0xFF),
          corrupt(impact, "bad impact", 31, 0x5A),
          truncated(pool, "short pool"),
      };
      Prng rng(0xF1E1u);
      for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
        zf::ProgCache ref(kEntries);
        dut.reset();
        const uint32_t n = 3 + rng.below(30);
        for (uint32_t k = 0; k < n; ++k) {
          const Image& im = pool_imgs[rng.below(static_cast<uint32_t>(pool_imgs.size()))];
          char tag[112];
          std::snprintf(tag, sizeof tag, "random[%u] op %u %s", it, k, im.name);
          acquire(dut, ref, im, tag);
        }
        char tag[64];
        std::snprintf(tag, sizeof tag, "random[%u] final counters", it);
        compare_counters(dut, ref, tag);
      }
      raw.final();
      return zhao::report_and_exit("field_progcache_random");
    }
  }

  // ---- concurrent lookup and commit ---------------------------------------
  // THE ARBITRATION LAW, AND THE SWEEP IS WHY IT IS HERE. Every case above
  // drives a lookup, waits for its reply, and only then drives a commit. So
  // `cm_ready_o`'s `&& !lu_fire` term -- the thing that stops a lookup and a
  // commit touching the entry table on the SAME clock -- was never exercised.
  // Mutation M79 deletes that term and SURVIVED the whole suite.
  //
  // This raises both request lines together and asserts the block never accepts
  // both on one edge. It is a structural law of the block, not a value the
  // reference model can answer: `zref::field::ProgCache` is sequential by
  // construction and has no notion of two requests in flight.
  {
    Dut d2(raw);
    zf::ProgCache ref2;
    (void)ref2;

    raw.lu_valid_i = 1;
    raw.lu_hash_i = 0xA5A50001u;
    raw.lu_resp_ready_i = 1;
    raw.cm_valid_i = 1;
    raw.cm_hash_i = 0xA5A50002u;
    raw.cm_ok_i = 1;
    raw.cm_resp_ready_i = 1;
    raw.eval();

    uint64_t both = 0;
    for (int i = 0; i < 32; ++i) {
      const bool lu_fire = raw.lu_valid_i && raw.lu_ready_o;
      const bool cm_fire = raw.cm_valid_i && raw.cm_ready_o;
      if (lu_fire && cm_fire) ++both;
      zhao::tick(raw);
      raw.eval();
    }
    check(both == 0, "a lookup and a commit never fire on the same clock", 0, both);

    raw.lu_valid_i = 0;
    raw.cm_valid_i = 0;
    raw.eval();
  }

  raw.final();
  return zhao::report_and_exit("field_progcache_directed");
}
