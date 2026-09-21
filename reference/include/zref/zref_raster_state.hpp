// zref_raster_state.hpp -- the per-triangle RASTER STATE word, ruling R28
// (provisional, coordinator, 2026-09-19).
//
// THE FIRST LINE SAID "owner ruling R28" AND THE SECOND SAID "(provisional,
// coordinator)" -- two consecutive lines contradicting each other, with the
// wrong half first. Corrected 2026-09-21 (gz/cfgarm): the decision table's
// R28 row reads "**(provisional, coordinator)** Ratify a `raster_state u32`
// layout in the ABI", and only R1..R7 in that whole table are "(owner,
// explicit)". It matters here and not only pedantically: R28's "no bit of
// [31:2] has a ratified consumer in v1" is a CO-ORDINATOR's provisional call,
// so whoever next needs those bits -- the per-draw fragment constants of
// entries I14/I20 are the live case -- is re-asking a question, not
// overturning the owner. See the citation record in
// `zhao_console_core.sv`'s tie-off block: 223 sites tree-wide say "owner
// ruling" for a coordinator-provisional one.
//
// NOT A DIFFERENT WORD FROM THIS ONE, said here because the names collide and
// two lanes have now reasoned across them: `zref::FragmentPipeline::State` in
// `zref_fragment.hpp` is a SEPARATE 32-bit word ([0] z_test_en ... [31:24]
// sten_mask, matching `zhao_raster_fragment.sv`) carried on
// `tri_fragment_state_i`. THIS word is the TriangleDescriptor's draw-state
// sideband, carried on `zhao_geom_assemble.m_raster_state_i`. Same width,
// same English name, different layouts, different ports.
//
// R28: "Ratify a `raster_state u32` layout in the ABI. Cull mode comes FIRST and
// from the DRAW (DrawForm flags); the remaining bits come from the material
// set. The packet writes the layout into spec/commands.zidl / the PARAMBUF
// contract with a zref model."
//
// THE LAYOUT (spec/commands.zidl, DrawForm and MaterialRecord; design/contracts/
// GEOM.PARAMBUF.md, TriangleDescriptor):
//
//   raster_state[1:0]   cull_mode   FROM THE DRAW: DrawForm.flags[3:2]
//                                   0 NONE (double-sided), 1 NEG, 2 POS,
//                                   3 RESERVED -- refused, never aliased
//   raster_state[31:2]  material    FROM THE MATERIAL: MaterialRecord.
//                                   raster_state[31:2], carried unchanged.
//                                   No bit of it has a ratified consumer in
//                                   v1, so a v1 material writes 0 there.
//   MaterialRecord.raster_state[1:0] RESERVED 0 -- the cull mode is the
//                                   draw's, so the material's copy of those
//                                   bits is not read.
//
// WHERE THE ENCODING COMES FROM, not chosen here: `zhao_geom_clip`'s
// `cull_mode_i` -- "0 NONE (default), 1 neg, 2 pos" -- is the consumer, and
// its NONE is the double-sided law the software raster already implements.
// The field sits at raster_state[1:0] because the consumer takes two bits and
// R28 puts the cull mode FIRST.
//
// DrawForm.flags[3:2]: bits 0-1 are the marker law's (billboard, screen-space
// size); bits 2-15 were reserved-0, so a v1 draw that never set them reads
// NONE -- the double-sided behaviour every existing capture was recorded under.
//
// Reserved 3 is REFUSED rather than aliased, as SetView's depth profile 3 is:
// silently treating 11 as a real mode is how a fourth one gets smuggled in.
#pragma once

#include <cstdint>

namespace zref {
namespace raster_state {

enum CullMode : uint8_t {
  kCullNone = 0,  // double-sided (zhao_geom_clip default, rast.cpp's law)
  kCullNeg = 1,   // cull triangles whose signed area is negative
  kCullPos = 2,   // cull triangles whose signed area is positive
  kCullReserved = 3,
};

constexpr uint16_t kDrawFlagsCullShift = 2;
constexpr uint16_t kDrawFlagsCullMask = 0x3u << kDrawFlagsCullShift;
constexpr uint32_t kCullMask = 0x3u;              // raster_state[1:0]
constexpr uint32_t kMaterialMask = ~kCullMask;    // raster_state[31:2]

/** The draw's cull mode, DrawForm.flags[3:2]. */
constexpr uint8_t cull_of_draw_flags(uint16_t draw_flags) {
  return static_cast<uint8_t>((draw_flags & kDrawFlagsCullMask) >> kDrawFlagsCullShift);
}

/** The word a triangle descriptor carries. `ok` is false -- and the word is
 *  0 -- when the draw names the reserved cull mode 3. */
constexpr uint32_t compose(uint16_t draw_flags, uint32_t material_raster_state, bool* ok) {
  const uint8_t cull = cull_of_draw_flags(draw_flags);
  if (cull == kCullReserved) {
    if (ok) *ok = false;
    return 0u;
  }
  if (ok) *ok = true;
  return (material_raster_state & kMaterialMask) | cull;
}

/** What GEOM.CLIP's `cull_mode_i` receives from a descriptor word. */
constexpr uint8_t cull_mode(uint32_t raster_state) {
  return static_cast<uint8_t>(raster_state & kCullMask);
}

}  // namespace raster_state
}  // namespace zref
