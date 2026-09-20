// field_host_mask_type_confusion_mutant.cpp -- THE COMMITTED MUTANT FOR A
// TYPE SYSTEM, not for RTL.
//
// WHAT IS MUTATED: one line, selected by `-DZFH_MASK_CONFUSION=1`. With the
// macro undefined this file does the LEGAL thing and must COMPILE. With it
// defined it assigns a RequiredMask where a WindowMask is wanted, and must
// FAIL TO COMPILE.
//
// DRIVER:  python tools/field/gen_field_host_schema.py --compile-fail-control
//          which runs BOTH polarities and fails if either is wrong. Registered
//          as the ctest `field_host_mask_type_separation`.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// `required_mask` is indexed by CANONICAL OUTPUT ORDINAL. `window_mask` is
// indexed by CONTIGUOUS CAPTURE-WINDOW POSITION. They coincide only when the
// output registers are contiguous from out_base, and R111 measured that they
// never are: the three shipped Earth programs give window masks
// 0x17 / 0x1D / 0x17, every one leaving three of seven window lanes unwritten.
//
// crater_ring writes R13,R14,R15,R17 with out_base=13, so its window mask is
// 0x17 and its ordinal mask is 0x0F. Wiring one to the other does not produce
// an error at runtime -- it produces a plausible wrong number, and the stamp
// and flow adapters read fixed window lanes, so the wrong number arrives
// downstream looking like data.
//
// The console already had a COMMENT saying the two were different. That is
// what shipped R101. This file is the difference between a comment and a
// mechanism: the confusion is now a compile error, and this is the evidence
// that it is, fired in both directions rather than argued.
//
// THIS IS EVIDENCE ABOUT THE INSTRUMENT, NOT ABOUT THE DESIGN. It asserts the
// CORRECT behaviour (the types do not interconvert) rather than asserting a
// bug, so it does not expire when anything is repaired.
//
// A NOTE ON WHY THE SELECTOR IS AN `#ifdef` AND NOT A FUNCTION-LIKE MACRO:
// CLAUDE.md records that Verilator's `-D` cannot override a function-like
// `define` and says nothing when it fails to, so a mutant build silently
// measured unmutated production. The C++ preprocessor has no such asymmetry
// here, but the discipline is the same: the driver compiles this file with the
// macro UNDEFINED and confirms the result DIFFERS, which is the only thing
// that proves the selector engaged at all.

#include <cstdint>

#include "zfield/generated/zfield_host_image.hpp"

using zfield::host_image::RequiredMask;
using zfield::host_image::WindowMask;

namespace {

// A stand-in for the host-side slot that holds the capture-window mask. In
// production this is fed from the loader header word at [32 +: OUT_LANES].
struct CaptureWindow {
  WindowMask written;
};

// crater_ring, measured: writes R13,R14,R15,R17 with out_base = 13.
constexpr WindowMask kCraterRingWindow{0x17};   // window positions 0,1,2,4
constexpr RequiredMask kCraterRingOrdinals{0x0F};  // ordinals 0,1,2,3

// The two masks describe the SAME program and are NOT equal. If these ever
// compare equal, the worked example has been broken and this control is no
// longer testing anything.
static_assert(kCraterRingWindow.bits() != kCraterRingOrdinals.bits(),
              "the worked example must keep the two masks distinct in VALUE "
              "as well as in TYPE, or the compile-fail case proves nothing");

static_assert(WindowMask::kBits == 7, "window mask is OUT_LANES wide");
static_assert(RequiredMask::kBits == 8, "required mask is carried in a u8");

void use_window(CaptureWindow& cw) {
#if defined(ZFH_MASK_CONFUSION)
  // ===================== THE MUTATION, ONE LINE =========================
  // An ordinal-indexed mask assigned into a window-indexed slot. This is
  // EXACTLY the defect class the whole campaign exists to prevent, and it
  // MUST NOT COMPILE.
  cw.written = kCraterRingOrdinals;
  // ======================================================================
#else
  // The legal thing: a window mask into a window slot.
  cw.written = kCraterRingWindow;
#endif
}

}  // namespace

int main() {
  CaptureWindow cw;
  use_window(cw);
  // Return zero on the legal path. The driver only reads the COMPILER's exit
  // code, never this one -- a compile-fail control that got as far as running
  // has already told us what we needed to know.
  return cw.written.empty() ? 1 : 0;
}
