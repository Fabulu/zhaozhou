// field_write_tag.cpp — the FIELD.WRITE.TAG op, RTL against the oracle.
//
// WHY THIS FILE EXISTS. `design/ops.yml` declares FIELD.WRITE.TAG and names this
// path as its differential coverage. The file did not exist. Ledger rule V10
// only said so once SURFACE.SHEET advanced past SPECIFIED — the op layer had no
// differential coverage at all, and the block above it was hiding that.
//
// The op also cites `zref::fieldir::sink_write_tag`, which does not exist: one
// of the twenty-five phantom reference models audited in
// reports/PHANTOM_REFERENCES.md, and one of the forty `zref::fieldir::*` names
// specifically. But the LAW does exist and always did — ops.yml states it as
// "write tag + strength into the Scar Scribe surface sheet (64x64 per patch)",
// and `zref::surface::SheetStore` is where that sheet lives. So this is a naming
// failure, not a missing implementation, and the fix is to test the real law
// rather than to write a second one beside it.
//
// EXHAUSTIVE OVER THE OPERAND, because the operand is small. A tag/strength pair
// is two bytes: 65,536 possibilities, every one of them tried. A sampled lane
// would be strictly weaker than simply trying them all.
//
// The op's `rounding: saturating` has no work to do at this layer — tag and
// strength arrive already as bytes, and the saturation happened in the blend
// that produced them (covered by tests/differential/field_stamp_modes.cpp).
// What this layer owes is that the bytes are stored EXACTLY, in the right texel,
// of the right sheet, and only when that sheet is resident.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_surface_sheet.h"

#include "surface_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_surface.hpp"

namespace {

using zhao::check;
namespace zs = zref::surface;

}  // namespace

int main() {
  Vzhao_surface_sheet dut;
  sdev::reset_sheet(dut);

  zs::SheetStore ref(2);
  const uint32_t H = 0x5CA2;

  ref.acquire(H);
  const sdev::SheetResponse acq = sdev::sheet_request(dut, sdev::kOpAcquire, H, 0, 1);
  check(acq.status == sdev::kStAllocated, "FIELD.WRITE.TAG: the target sheet is resident",
        sdev::kStAllocated, acq.status);
  const int slot = ref.find(H);

  // ---- 1. exhaustive over the two-byte operand ----------------------------
  // 65,536 pairs, in sixteen passes of 4,096 texels. Each pass writes a distinct
  // pair to every texel and then reads every texel back, so a write that landed
  // in the WRONG texel fails as loudly as one that stored the wrong bytes --
  // which a single-texel test would not catch.
  {
    uint64_t mismatches = 0;
    uint32_t first_texel = 0, first_want_tag = 0, first_got_tag = 0;
    uint32_t first_want_str = 0, first_got_str = 0;

    for (uint32_t pass = 0; pass < 16; ++pass) {
      for (uint32_t tx = 0; tx < static_cast<uint32_t>(zs::kSheetTexels); ++tx) {
        const uint32_t pair = pass * static_cast<uint32_t>(zs::kSheetTexels) + tx;
        const uint8_t tag = static_cast<uint8_t>(pair >> 8);
        const uint8_t strength = static_cast<uint8_t>(pair & 0xFF);
        ref.at(slot).tag[tx] = tag;
        ref.at(slot).strength[tx] = strength;
        sdev::sheet_write(dut, H, static_cast<uint16_t>(tx), tag, strength, true, true, 2);
      }
      for (uint32_t tx = 0; tx < static_cast<uint32_t>(zs::kSheetTexels); ++tx) {
        const sdev::SheetResponse r =
            sdev::sheet_request(dut, sdev::kOpRead, H, static_cast<uint16_t>(tx), 3);
        if (r.tag != ref.at(slot).tag[tx] || r.strength != ref.at(slot).strength[tx]) {
          if (mismatches++ == 0) {
            first_texel = tx;
            first_want_tag = ref.at(slot).tag[tx];
            first_got_tag = r.tag;
            first_want_str = ref.at(slot).strength[tx];
            first_got_str = r.strength;
          }
        }
      }
    }

    check(mismatches == 0,
          "FIELD.WRITE.TAG: all 65,536 tag/strength pairs stored exactly, in the right texel", 0,
          mismatches);
    if (mismatches) {
      std::printf("  first divergence: texel %u want tag=%u str=%u got tag=%u str=%u\n",
                  first_texel, first_want_tag, first_want_str, first_got_tag, first_got_str);
    }
  }

  // ---- 2. the byte enables, all four combinations -------------------------
  // The op writes "tag + strength", but the sink is reached by blends that move
  // only one of them -- an AGE pass decays strength and must not rewrite the
  // material tag of every texel it touches.
  {
    const uint16_t tx = 1234;
    ref.at(slot).tag[tx] = 40;
    ref.at(slot).strength[tx] = 50;
    sdev::sheet_write(dut, H, tx, 40, 50, true, true, 4);

    struct Combo {
      bool we_tag, we_str;
      const char* what;
    };
    const Combo combos[4] = {
        {true, true, "both lanes enabled: both change"},
        {true, false, "tag only: strength is untouched"},
        {false, true, "strength only: the material tag is untouched"},
        {false, false, "neither enabled: the texel does not change at all"},
    };
    uint8_t t = 111, s = 222;
    for (const Combo& c : combos) {
      if (c.we_tag) ref.at(slot).tag[tx] = t;
      if (c.we_str) ref.at(slot).strength[tx] = s;
      sdev::sheet_write(dut, H, tx, t, s, c.we_tag, c.we_str, 5);
      const sdev::SheetResponse r = sdev::sheet_request(dut, sdev::kOpRead, H, tx, 6);
      char lbl[160];
      std::snprintf(lbl, sizeof lbl, "FIELD.WRITE.TAG: %s (tag)", c.what);
      check(r.tag == ref.at(slot).tag[tx], lbl, ref.at(slot).tag[tx], r.tag);
      std::snprintf(lbl, sizeof lbl, "FIELD.WRITE.TAG: %s (strength)", c.what);
      check(r.strength == ref.at(slot).strength[tx], lbl, ref.at(slot).strength[tx], r.strength);
      t = static_cast<uint8_t>(t + 37);
      s = static_cast<uint8_t>(s + 53);
    }
  }

  // ---- 3. the sink is REFUSED when the sheet is not resident --------------
  // ops.yml puts this op in SURFACE.SHEET, and the block's one hard sentence is
  // "overflow rejects the stamp, never partial-writes". So the op is not
  // unconditional: a WRITE.TAG naming a patch nobody holds writes nothing, and
  // in particular does not write into whichever sheet happens to be in slot 0.
  {
    const uint16_t tx = 77;
    sdev::sheet_write(dut, H, tx, 8, 9, true, true, 7);
    ref.at(slot).tag[tx] = 8;
    ref.at(slot).strength[tx] = 9;

    const uint32_t stranger = 0xDEAD;
    check(ref.find(stranger) < 0, "FIELD.WRITE.TAG: the stranger's patch is not resident", 1,
          ref.find(stranger) < 0 ? 1 : 0);
    for (uint16_t k = 0; k < 16; ++k) {
      sdev::sheet_write(dut, stranger, k, 0xFF, 0xFF, true, true, 8);
    }

    uint64_t disturbed = 0;
    for (uint16_t k = 0; k < 16; ++k) {
      const sdev::SheetResponse r = sdev::sheet_request(dut, sdev::kOpRead, H, k, 9);
      if (r.tag != ref.at(slot).tag[k] || r.strength != ref.at(slot).strength[k]) ++disturbed;
    }
    const sdev::SheetResponse r77 = sdev::sheet_request(dut, sdev::kOpRead, H, tx, 10);
    check(disturbed == 0, "FIELD.WRITE.TAG: a write to a non-resident patch lands nowhere", 0,
          disturbed);
    check(r77.tag == 8 && r77.strength == 9,
          "FIELD.WRITE.TAG: and the resident sheet is byte-identical afterwards", 1,
          (r77.tag == 8 && r77.strength == 9) ? 1 : 0);
  }

  // ---- 4. a released patch stops accepting the sink -----------------------
  {
    ref.release(H);
    sdev::sheet_request(dut, sdev::kOpRelease, H, 0, 11);
    sdev::sheet_write(dut, H, 5, 0x77, 0x88, true, true, 12);
    const sdev::SheetResponse r = sdev::sheet_request(dut, sdev::kOpRead, H, 5, 13);
    check(r.status == sdev::kStMiss, "FIELD.WRITE.TAG: a released patch is not a sink any more",
          sdev::kStMiss, r.status);
  }

  dut.final();
  return zhao::report_and_exit("field_write_tag");
}
