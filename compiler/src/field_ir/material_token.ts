// material_token.ts — the 32-bit MATERIAL TOKEN, v1, as a Field IR program can
// build it. `spec/qformats.md` §14.1 is the law; this is its THIRD view.
//
// ---------------------------------------------------------------------------
// WHY A THIRD VIEW EXISTS, AND WHAT PINS IT
// ---------------------------------------------------------------------------
// §14.1 says, in as many words, "Implementations, and they are two views of
// this section, not three laws":
//
//     fpga/rtl/common/zhao_material_token_pkg.sv    -- zmt_encode / zmt_tag_ok
//     reference/include/zref/zref_fieldir.hpp       -- material_token_encode
//
// Neither of them can be called from here. A Field IR program is BUILT in
// TypeScript (`compiler/src/field_ir/*`, the single sanctioned program source
// path -- never hand-serialized), so a program that wants to emit a token needs
// the layout expressed on this side too. CLAUDE.md's sibling-contract rule says
// a second implementation of a ratified arithmetic is a defect, so this file is
// deliberately the SMALLEST possible restatement -- three byte positions and one
// tag constant, no arithmetic -- and it is PINNED rather than trusted:
//
//   * `compiler/tests/material_program.test.ts` asserts the constant this file
//     produces against a literal transcribed from §14.1's own bit diagram, and
//     asserts the tag against 0xE1 separately from the packing.
//   * `tests/prod/smoke_field_fixture_gen.cpp` runs the emitted program through
//     `zfield::interpret` and checks out-lane 2 with
//     `zref::fieldir::material_token_tag_ok` / `material_token_decode` -- the
//     C++ view, not this one. So the TS packing is checked by the reference
//     decoder, in a committed generator, rather than by its own mirror.
//
// That is the difference between a third view and a third LAW: this one is
// never the authority for anything, and something that is the authority reads
// every value it produces.
//
// ---------------------------------------------------------------------------
// WHY THE CORPUS DID NOT ALREADY HAVE THIS
// ---------------------------------------------------------------------------
// `crater_ring.ts:41-42` and `wave_pool.ts:88` write out-lane 2 as a BARE
// material id -- `MAT_SOIL = 1`, `MAT_CHARRED = 2`. Those are pre-token values:
// §14.1 was written on 2026-09-27 (MATERIALPATH) on the fabric and reference
// sides, and the program corpus was never brought along. A bare id has tag byte
// 0x00, so `zmt_tag_ok` REFUSES it -- correctly, and by design. Measured in the
// composed console by packet NOPROG: `terrmat token_refused=1024`, one per
// compose-cache cell, with `field_composed=0`.
//
// So until this file existed, NO field program in this tree could produce a
// material the fabric would accept. That is recorded as a finding rather than
// repaired in place: changing `wave_pool`'s or `crater_ring`'s material lane
// moves their program HASH, which their committed .zvec goldens, the fuzz
// corpus seeds and `generated_conformance.test.ts` all pin.

/**
 * v1 tag, `spec/qformats.md` §14.1. Mnemonic: Earth material, revision 1.
 * The same constant as `zref::fieldir::kMaterialTokenTagV1` and
 * `zhao_material_token_pkg::ZMT_TAG_V1`.
 */
export const MATERIAL_TOKEN_TAG_V1 = 0xe1;

/**
 * The token with its weight byte left at zero, ready for a Field IR ADD to
 * place a computed weight in it.
 *
 * THE POINT OF THE SPLIT. The frozen v1 ISA has NO bitwise operators -- no AND,
 * OR, XOR or shift (`types.ts` OpName). A program therefore cannot assemble a
 * token field by field; it must ADD a computed low byte to a constant upper
 * three. That is safe if and only if the addend is bounded to 0..255, and the
 * caller owes that bound from the SHAPE of its arithmetic rather than by
 * asserting it -- see `scorch_wash.ts`, where the bound is the smoothstep
 * macro's own internal clamp.
 *
 * Returned as a signed 32-bit value, because that is what `LDC` stores: any tag
 * with bit 7 set (0xE1 does) makes the token negative as an int32, and
 * `builder.ldc` applies `| 0` anyway.
 */
export function materialTokenBase(matA: number, matB: number): number {
  if (!Number.isInteger(matA) || matA < 0 || matA > 255) {
    throw new Error(`materialTokenBase: matA ${matA} is not a u8`);
  }
  if (!Number.isInteger(matB) || matB < 0 || matB > 255) {
    throw new Error(`materialTokenBase: matB ${matB} is not a u8`);
  }
  return ((MATERIAL_TOKEN_TAG_V1 << 24) | (matA << 16) | (matB << 8)) | 0;
}

/**
 * The whole token, for tests and for a program whose weight is a constant.
 * Mirrors `zref::fieldir::material_token_encode`.
 */
export function materialToken(matA: number, matB: number, weight: number): number {
  if (!Number.isInteger(weight) || weight < 0 || weight > 255) {
    throw new Error(`materialToken: weight ${weight} is not a u8`);
  }
  return (materialTokenBase(matA, matB) + weight) | 0;
}

/** Tag check, mirroring `zref::fieldir::material_token_tag_ok`. */
export function materialTokenTagOk(tok: number): boolean {
  return ((tok >>> 24) & 0xff) === MATERIAL_TOKEN_TAG_V1;
}

/** Decode, mirroring `zref::fieldir::material_token_decode`'s field order. */
export function materialTokenDecode(
  tok: number,
): { matA: number; matB: number; weight: number } | null {
  if (!materialTokenTagOk(tok)) return null;
  return {
    matA: (tok >>> 16) & 0xff,
    matB: (tok >>> 8) & 0xff,
    weight: tok & 0xff,
  };
}
