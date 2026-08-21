// surface_sheet_store_diff.cpp — SURFACE.SHEET against `zref::surface::SheetStore`.
//
// WHY THIS FILE EXISTS. `surface_sheet_directed.cpp` is a good suite and it is
// not a differential: it includes `zref_surface.hpp`, aliases the namespace, and
// then never touches `SheetStore`. It checks what the block promises about
// itself. Ledger rule V17 refused to accept it as evidence that the declared
// oracle is implemented, and V17 was right — "an alias, not evidence".
//
// So this runs the real RTL and the real `SheetStore` over the same operation
// stream and requires them to agree on everything both can see: the residency
// verdict, the occupancy mask, and the contents of every texel.
//
// The three laws that carry the block, all of them CHOSEN in the reference and
// so all of them things an independent implementation would get wrong:
//
//   * RE-ACQUIRING A RESIDENT HANDLE DOES NOT CLEAR IT. That is persistence,
//     which is the entire reason the sheet exists — a scar a player burned into
//     the ground stays there. A fresh slot IS cleared. An implementation that
//     cleared on every acquire passes any test that writes and reads inside one
//     acquire, and erases the world between frames.
//   * OVERFLOW REJECTS, IT NEVER EVICTS. The ledger's one hard sentence.
//     A full directory refuses, and the stamp writes nothing — not one texel.
//   * A WRITE TO A NON-RESIDENT HANDLE LANDS NOWHERE. Not in slot 0, not in the
//     nearest slot. This is the same sentence from the other end, and it is the
//     one that would silently corrupt a neighbouring patch.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_surface_sheet.h"

#include "surface_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_surface.hpp"

namespace {

using zhao::check;
namespace zs = zref::surface;

constexpr int kSlots = 2;  // the RTL's Slots default and the SheetStore default

/** The reference's verdict, in the RTL's status encoding. */
uint8_t status_of(zs::Residency r) {
  switch (r) {
    case zs::Residency::kHit: return sdev::kStHit;
    case zs::Residency::kAllocated: return sdev::kStAllocated;
    default: return sdev::kStOverflow;
  }
}

struct Pair {
  Vzhao_surface_sheet& dut;
  zs::SheetStore ref{kSlots};

  explicit Pair(Vzhao_surface_sheet& d) : dut(d) { sdev::reset_sheet(dut); }

  /** ACQUIRE on both; the verdicts must match. */
  void acquire(uint32_t handle, const char* what) {
    const zs::AcquireResult want = ref.acquire(handle);
    const sdev::SheetResponse got =
        sdev::sheet_request(dut, sdev::kOpAcquire, handle, 0, 0x100);
    const std::string t(what);
    check(got.got, (t + ": the block answered").c_str(), 1, got.got ? 1 : 0);
    check(got.status == status_of(want.status), (t + ": residency verdict").c_str(),
          status_of(want.status), got.status);
    check_occupancy(what);
  }

  void release(uint32_t handle, const char* what) {
    ref.release(handle);
    sdev::sheet_request(dut, sdev::kOpRelease, handle, 0, 0x101);
    check_occupancy(what);
  }

  /**
   * Write one texel to both. The reference store has no byte enables -- they
   * are the RTL's way of letting a blend move strength without disturbing tag
   * -- so the enables are applied here, against the resident slot only.
   */
  void write(uint32_t handle, uint16_t texel, uint8_t tag, uint8_t strength, bool we_tag,
             bool we_str) {
    const int slot = ref.find(handle);
    if (slot >= 0) {
      if (we_tag) ref.at(slot).tag[texel] = tag;
      if (we_str) ref.at(slot).strength[texel] = strength;
    }
    // A write naming a non-resident handle is offered to the RTL anyway: that
    // it lands nowhere is the law being tested, not a case to avoid.
    sdev::sheet_write(dut, handle, texel, tag, strength, we_tag, we_str, 0x200);
  }

  /** READ one texel from both and compare. */
  void read_check(uint32_t handle, uint16_t texel, const char* what) {
    const int slot = ref.find(handle);
    const sdev::SheetResponse got = sdev::sheet_request(dut, sdev::kOpRead, handle, texel, 0x300);
    const std::string t(what);
    if (slot < 0) {
      check(got.status == sdev::kStMiss, (t + ": read of a non-resident handle misses").c_str(),
            sdev::kStMiss, got.status);
      return;
    }
    check(got.status == sdev::kStHit, (t + ": read of a resident handle hits").c_str(),
          sdev::kStHit, got.status);
    check(got.tag == ref.at(slot).tag[texel], (t + ": tag").c_str(), ref.at(slot).tag[texel],
          got.tag);
    check(got.strength == ref.at(slot).strength[texel], (t + ": strength").c_str(),
          ref.at(slot).strength[texel], got.strength);
  }

  /** Compare every texel of a resident handle. Slow, so used at section ends. */
  void read_all(uint32_t handle, const char* what) {
    const int slot = ref.find(handle);
    if (slot < 0) return;
    uint64_t bad = 0;
    uint16_t first = 0;
    for (uint16_t tx = 0; tx < zs::kSheetTexels; ++tx) {
      const sdev::SheetResponse got = sdev::sheet_request(dut, sdev::kOpRead, handle, tx, 0x400);
      if (got.tag != ref.at(slot).tag[tx] || got.strength != ref.at(slot).strength[tx]) {
        if (bad++ == 0) first = tx;
      }
    }
    const std::string t(what);
    check(bad == 0, (t + ": all 4096 texels match the oracle").c_str(), 0, bad);
    if (bad) std::printf("  first divergence at texel %u\n", first);
  }

  void check_occupancy(const char* what) {
    const std::string t(what);
    check(dut.res_occupancy_o == ref.occupancy(), (t + ": occupancy mask").c_str(),
          ref.occupancy(), dut.res_occupancy_o);
  }
};

}  // namespace

int main(int argc, char** argv) {
  Vzhao_surface_sheet dut;

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    sdev::Rng rng(0x5EE7u);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      Pair p(dut);
      // A small handle pool so collisions, re-acquires and overflow all happen
      // often. With two slots and four handles, overflow is the common case
      // rather than a corner -- which is what makes the reject-never-evict law
      // exercised rather than merely present.
      const uint32_t pool[4] = {0x11, 0x22, 0x33, 0x44};
      const int ops = 12 + static_cast<int>(rng.range(0, 24));
      for (int k = 0; k < ops; ++k) {
        const uint32_t h = pool[rng.range(0, 3)];
        char tag[80];
        std::snprintf(tag, sizeof tag, "random[%u] op %d h=%u", it, k, h);
        switch (rng.range(0, 3)) {
          case 0: p.acquire(h, tag); break;
          case 1: p.release(h, tag); break;
          case 2:
            p.write(h, static_cast<uint16_t>(rng.range(0, zs::kSheetTexels - 1)),
                    static_cast<uint8_t>(rng.range(0, 255)),
                    static_cast<uint8_t>(rng.range(0, 255)), rng.range(0, 1) != 0,
                    rng.range(0, 1) != 0);
            break;
          default:
            p.read_check(h, static_cast<uint16_t>(rng.range(0, zs::kSheetTexels - 1)), tag);
            break;
        }
      }
    }
    dut.final();
    return zhao::report_and_exit("surface_sheet_store_random");
  }

  // ---- 1. a fresh acquire allocates and CLEARS ----------------------------
  {
    Pair p(dut);
    p.acquire(0xA1, "fresh acquire");
    p.read_check(0xA1, 0, "fresh sheet, texel 0");
    p.read_check(0xA1, 4095, "fresh sheet, last texel");
    p.read_all(0xA1, "fresh sheet");
  }

  // ---- 2. PERSISTENCE: a re-acquire does not clear -------------------------
  // The law the block exists for. An implementation that cleared on every
  // acquire would pass any test that writes and reads within one acquire, and
  // would erase every scar in the world between frames.
  {
    Pair p(dut);
    p.acquire(0xB2, "acquire for persistence");
    p.write(0xB2, 100, 7, 200, true, true);
    p.write(0xB2, 4000, 9, 250, true, true);
    p.acquire(0xB2, "RE-acquire of a resident handle");
    p.read_check(0xB2, 100, "after re-acquire, texel 100 persists");
    p.read_check(0xB2, 4000, "after re-acquire, texel 4000 persists");
    p.read_all(0xB2, "after re-acquire");
  }

  // ---- 3. release then re-acquire DOES clear ------------------------------
  {
    Pair p(dut);
    p.acquire(0xC3, "acquire before release");
    p.write(0xC3, 55, 3, 99, true, true);
    p.release(0xC3, "release");
    p.read_check(0xC3, 55, "a released handle reads as a miss");
    p.acquire(0xC3, "acquire after release");
    p.read_check(0xC3, 55, "and the fresh slot is cleared");
    p.read_all(0xC3, "after release and re-acquire");
  }

  // ---- 4. OVERFLOW REJECTS, IT NEVER EVICTS -------------------------------
  // Two slots, three handles. The third acquire must be refused, and -- the
  // part that matters -- the two resident sheets must be untouched afterwards.
  {
    Pair p(dut);
    p.acquire(0xD1, "overflow: first slot");
    p.write(0xD1, 10, 1, 11, true, true);
    p.acquire(0xD2, "overflow: second slot");
    p.write(0xD2, 20, 2, 22, true, true);
    p.acquire(0xD3, "overflow: the third handle is REFUSED");
    p.read_check(0xD1, 10, "overflow did not disturb the first sheet");
    p.read_check(0xD2, 20, "overflow did not disturb the second sheet");
    p.read_check(0xD3, 10, "the refused handle is not resident");
    p.read_all(0xD1, "first sheet after overflow");
    p.read_all(0xD2, "second sheet after overflow");
  }

  // ---- 5. A WRITE TO A NON-RESIDENT HANDLE LANDS NOWHERE ------------------
  // The same law from the other end, and the one that would silently corrupt a
  // neighbour. The write is offered with a handle nobody holds; afterwards both
  // resident sheets must be byte-identical to the oracle.
  {
    Pair p(dut);
    p.acquire(0xE1, "write-miss: first slot");
    p.acquire(0xE2, "write-miss: second slot");
    p.write(0xE1, 30, 5, 55, true, true);
    p.write(0xE2, 30, 6, 66, true, true);
    for (uint16_t tx = 0; tx < 8; ++tx) {
      p.write(0xEE, tx, 0xFF, 0xFF, true, true);  // nobody holds 0xEE
    }
    p.read_check(0xE1, 30, "the stranger's write did not reach the first sheet");
    p.read_check(0xE2, 30, "nor the second");
    p.read_check(0xE1, 0, "nor texel 0 of the first");
    p.read_all(0xE1, "first sheet after a stranger's writes");
    p.read_all(0xE2, "second sheet after a stranger's writes");
  }

  // ---- 6. the byte enables ------------------------------------------------
  // A blend that only moves strength must leave tag alone. Without separate
  // enables, an age or decay pass would rewrite the material tag of every texel
  // it touched.
  {
    Pair p(dut);
    p.acquire(0xF1, "byte enables");
    p.write(0xF1, 77, 42, 100, true, true);
    p.read_check(0xF1, 77, "both lanes written");
    p.write(0xF1, 77, 99, 150, false, true);   // strength only
    p.read_check(0xF1, 77, "strength-only write leaves the tag alone");
    p.write(0xF1, 77, 13, 200, true, false);   // tag only
    p.read_check(0xF1, 77, "tag-only write leaves the strength alone");
    p.read_all(0xF1, "after selective writes");
  }

  // ---- 7. occupancy tracks the oracle through a full cycle ----------------
  {
    Pair p(dut);
    p.check_occupancy("occupancy: empty");
    p.acquire(0x91, "occupancy: one");
    p.acquire(0x92, "occupancy: two");
    p.acquire(0x93, "occupancy: refused");
    p.release(0x91, "occupancy: one freed");
    p.acquire(0x93, "occupancy: the refused handle now fits");
    p.release(0x92, "occupancy: the other freed");
    p.release(0x93, "occupancy: empty again");
  }

  dut.final();
  return zhao::report_and_exit("surface_sheet_store_diff");
}
