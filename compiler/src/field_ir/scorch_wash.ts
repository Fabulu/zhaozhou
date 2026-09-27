// scorch_wash.ts — a MATERIAL field: scorched ground inside a radial footprint,
// with a soft rim, and NO deformation at all.
//
// This is the Earth-profile counterpart to `wave_pool` (height) and
// `crater_ring` (height + a material id). It is the first program in this tree
// whose out-lane 2 is a LEGAL v1 material token, `spec/qformats.md` §14.1, so it
// is the first one whose material write the fabric will accept rather than
// refuse. Built from the same builder/alloc/serialize chain as the other two --
// the single sanctioned program source path, never hand-serialized.
//
// ---------------------------------------------------------------------------
// WHY IT WRITES ZERO HEIGHT, WHICH IS THE WHOLE EXPERIMENTAL DESIGN
// ---------------------------------------------------------------------------
// A god's scorch recolours ground; it does not move it. That is the spell, and
// it is also what makes this program the right instrument for the owner's
// acceptance clause 3 ("a value that cannot equal the authored baseline by
// accident") in the COMPOSED console.
//
// `zhao_terrain_patch.sv:339` composes field height ADDITIVELY --
// `acc_next = cur_covers ? zhao_tp_fx_add_sat(acc, fld_height_i) : acc`. So a
// field writing height ZERO leaves every lattice height BIT-IDENTICAL to the
// authored terrain. The console's geometry, its fragment count and its pixel
// count are therefore unchanged by this field, and the ONLY thing that moves is
// the material -- observable as the mosaic tile the texture island fetches.
//
// That matters because the alternative CONFOUNDS the evidence. A program that
// deforms the ground AND writes material changes the tile index for two
// possible reasons: the material write landed, or the deformed terrain simply
// sampled a different part of the mosaic. CLAUDE.md: "compare like with like,
// or do not compare." Holding the geometry exactly still is what makes a moved
// tile index attributable to the material write alone.
//
// `zhao_terrain_veljoin.sv:282` passes velocity THROUGH (`v_velocity_o =
// a_velocity_i`) rather than accumulating it, and zero is the physically
// correct surface velocity for ground that is not moving. Both zero lanes are
// declared rather than omitted, so the required-output mask stays 0x0F -- the
// same four-lane shape `wave_pool` has, and the shape I34's four Earth channels
// are written against. An ABSENT lane and a lane written zero are different
// statements (`zref_fieldir.hpp`: "an absent output is not a write of zero");
// this program means the second one, deliberately.
//
// ---------------------------------------------------------------------------
// THE TOKEN IS ASSEMBLED BY ARITHMETIC, BECAUSE THE ISA HAS NO BITWISE OPS
// ---------------------------------------------------------------------------
// The frozen v1 ISA (`types.ts` OpName) has no AND, OR, XOR or shift. So the
// token cannot be packed field by field; the upper three bytes are one LDC and
// the weight byte is ADDED to it. That is only correct while the addend is
// bounded to 0..255, and the bound here is STRUCTURAL rather than asserted:
//
//   * `cov` is the §3.20 smoothstep macro's result. The macro clamps its
//     interpolant to [0,1] internally (`builder.ts:192-205`: `clamp(t, 0, 1)`
//     then `t^2 * (3 - 2t)`), so `cov` is in [0, 1.0] = raw [0, 65536] for ANY
//     input, including a degenerate footprint where the two radii coincide.
//   * `WEIGHT_SPAN` is an LDC whose RAW value is 0x2F = 47, i.e. fx 47/65536.
//     Q16.16 MUL is `(a*b) >> 16`, so `cov * WEIGHT_SPAN` is raw `[0, 47]` --
//     an integer in the low byte, which is exactly what is wanted.
//   * `0xD0 + 47 = 0xFF`, so the addition CANNOT carry out of the weight byte
//     into matB. The upper three bytes of the emitted token are invariant over
//     the whole input domain.
//
// ---------------------------------------------------------------------------
// THE VALUE, AND WHY IT CANNOT COINCIDE WITH AN AUTHORED MATERIAL
// ---------------------------------------------------------------------------
// The console smoke's authored layer-E plane
// (`tests/prod/tb_zhao_console_core_smoke.sv:3881-3886`) spans:
//
//     authored matA   in {1, 2}            (SGF_TERR_MAT_A_LO_C=1,  span 2)
//     authored matB   in {5, 6}            (SGF_TERR_MAT_B_LO_C=5,  span 2)
//     authored weight in [0x30, 0xCF]      (lo 0x30, span 160)
//
// so the SET OF BYTE VALUES the authored plane can carry anywhere is
// {1,2,5,6} union [0x30,0xCF]. This program's three bytes are
//
//     matA   = 0xD4 = 212
//     matB   = 0x1E = 30
//     weight in [0xD0, 0xFF] = [208, 255]
//
// and NOT ONE of them is in that set: 212 > 0xCF, 30 < 0x30, and [208,255] is
// disjoint from [0x30,0xCF] (they abut at 0xCF/0xD0). So the field's triple
// cannot equal an authored triple, cannot equal one with matA and matB SWAPPED,
// and cannot equal one under any ROTATION or single-byte match of the three --
// no byte of this token is a byte the authored plane is able to produce. That is
// an argument from the two layouts, not an observation about one run, and
// `material_program.test.ts` asserts the disjointness so that a later edit to
// either side which destroys it goes RED instead of going quiet.
//
// The owner's bar names two previous failures of exactly this clause -- a check
// that passed on a constant 0xFFFFFF, and `mat_cells` counting cells rather than
// values. Both are excluded here by construction rather than by inspection.

import { FieldBuilder } from './builder.js';
import { buildProgram } from './alloc.js';
import { FieldProgram, IoLane, SourceSpan } from './types.js';
import { serializeProgram, programHashOfBytes, decodeZprog } from './serialize.js';
import { materialTokenBase } from './material_token.js';

/** kind 3 (field program), module 1, index 4 (capture_format.md §5). */
export const SCORCH_WASH_SOURCE_ID = 0x30010004;

/**
 * Spans point at THIS FILE, which is the program's actual source.
 *
 * `wave_pool.hpp` and `crater_ring.hpp` both declare a `.form` source --
 * "spells/membrane.form" -- and THERE IS NO `spells/` DIRECTORY IN THIS TREE
 * (found by packet NOPROG; `find . -name '*.form'` returns only the compiler's
 * own test corpus). A generated file citing a source that does not exist sends
 * the next reader looking for it, so this program does not repeat that: until
 * there is a Nanquan front end emitting Field IR, the TypeScript module IS the
 * source and says so.
 */
const L = (line: number, col: number): SourceSpan => ({
  sourceId: SCORCH_WASH_SOURCE_ID, line, col,
});

/** raw fx helper */
const fx = (v: number): number => Math.round(v * 65536);

// ---------------------------------------------------------------------------
// THE KNOBS. Every shape and value is a named editable constant -- CLAUDE.md,
// "never remove the owner's control in the name of fidelity". A value chosen to
// be disjoint from something else is still a value somebody may need to change,
// and the check that the disjointness survives lives in the test, not here.
// ---------------------------------------------------------------------------

/** Candidate tile A: the scorched tile. Outside the authored matA/matB/weight ranges. */
export const MAT_SCORCH_A = 0xd4;
/** Candidate tile B: the ash fringe. Also outside all three authored ranges. */
export const MAT_SCORCH_B = 0x1e;
/** Weight at the clean rim (cov = 0). Chosen one above the authored ceiling 0xCF. */
export const SCORCH_WEIGHT_LO = 0xd0;
/** Weight added at full scorch (cov = 1). `0xD0 + 0x2F = 0xFF`, the exact endpoint. */
export const SCORCH_WEIGHT_SPAN = 0x2f;

/** The lowest and highest tokens this program can emit, for the tests to pin. */
export const SCORCH_TOKEN_LO =
  (materialTokenBase(MAT_SCORCH_A, MAT_SCORCH_B) + SCORCH_WEIGHT_LO) | 0;
export const SCORCH_TOKEN_HI =
  (materialTokenBase(MAT_SCORCH_A, MAT_SCORCH_B) + SCORCH_WEIGHT_LO + SCORCH_WEIGHT_SPAN) | 0;

/**
 * THE UNIFORM SET THE COMPOSED CONSOLE STAGES, and why it is centred on the
 * island datum rather than on a patch.
 *
 * `tests/prod/smoke_field_fixture_gen.cpp` preloads these through
 * `zfield::prepare`. The console places ONE of the geom fixture's terrain
 * records, and WHICH one is not a constant this module should be asserting: a
 * centre pinned to one patch would silently stop varying if the placement
 * moved, which is exactly the quiet vacuity clause 3 exists to exclude.
 *
 * So the scorch is centred on the ISLAND DATUM (0,0) -- the origin the bench
 * envelope law and its particle population both use -- with a 160 m outer
 * radius and no full-scorch core. Any 32 m patch lying within 160 m of the datum
 * then samples a MONOTONE, NON-CONSTANT slice of the falloff, so out-lane 2
 * provably depends on the varying lanes wherever the patch is placed.
 * `material_program.test.ts` sweeps this exact set and asserts it varies.
 */
export const SCORCH_CONSOLE_UNIFORMS = {
  age: 0,
  phase: 0,
  centreX: 0,
  centreZ: 0,
  rIn: 0,
  rOut: 160 * 65536,
  nav: 2 * 65536,
};

function earthInputs(): IoLane[] {
  return [
    // RANGES ARE WIDER THAN wave_pool's +/-40 m ON PURPOSE, and it is not
    // generosity. The console smoke's terrain patches sit at patch coordinates
    // (r + SGF_TERR_IX0, r + SGF_TERR_IZ0) with a 32 m edge, so their lattice
    // vertices carry world coordinates out past 110 m. wave_pool declares +/-40
    // and is nonetheless driven with those coordinates -- the bounds are
    // DESCRIPTIVE (they seed the .zvec input generator) and nothing enforces
    // them at run time, so that mismatch is invisible. Declaring the domain
    // this program is actually evaluated over is the honest version.
    { name: 'x', type: 'fx', reg: 0, min: fx(-256), max: fx(256) },
    { name: 'z', type: 'fx', reg: 1, min: fx(-256), max: fx(256) },
    { name: 'age', type: 'u32', reg: 2, min: 0, max: 65535 },
    { name: 'phase', type: 'fx', reg: 3, min: 0, max: fx(1) },
    { name: 'p0', type: 'fx', reg: 4, min: fx(-256), max: fx(256) },  // centre x
    { name: 'p1', type: 'fx', reg: 5, min: fx(-256), max: fx(256) },  // centre z
    { name: 'p2', type: 'fx', reg: 6, min: 0, max: fx(256) },         // r_in: full scorch
    { name: 'p3', type: 'fx', reg: 7, min: 0, max: fx(256) },         // r_out: clean ground
    { name: 'p4', type: 'fx', reg: 8, min: 0, max: fx(4) },           // nav surcharge
    { name: 'p5', type: 'fx', reg: 9, min: fx(-1), max: fx(1) },    // unused
    { name: 'p6', type: 'fx', reg: 10, min: fx(-1), max: fx(1) },   // unused
    { name: 'p7', type: 'fx', reg: 11, min: fx(-1), max: fx(1) },   // unused
  ];
}

export interface ScorchWashBuild {
  program: FieldProgram;
  bytes: Uint8Array;
  hash: number;
  /** PC of the DIST2 instruction in the final allocated code */
  dist2Pc: number;
  /** PC of the ADD that places the weight byte into the token */
  tokenAddPc: number;
}

export function buildScorchWash(): ScorchWashBuild {
  const b = new FieldBuilder('earth', SCORCH_WASH_SOURCE_ID, earthInputs());

  const x = b.inputVal(0), z = b.inputVal(1);
  const p0 = b.inputVal(4), p1 = b.inputVal(5), p2 = b.inputVal(6);
  const p3 = b.inputVal(7), p4 = b.inputVal(8);

  // d = |p - centre|, then coverage: 1 inside p2, falling to 0 at p3. The
  // reversed edge order (p3, p2) is the same trick crater_ring's walls and
  // wave_pool's envelope use, and it is what makes cov DECREASE with distance.
  const d = b.dist2(x, z, p0, p1, L(177, 13));
  const cov = b.smoothstep(p3, p2, d, L(178, 15));

  // NO DEFORMATION. See the header: this is the experimental design, not a
  // shortcut. Two separate LDCs rather than one shared Val, so the two output
  // lanes never alias a single register in the allocator.
  const height = b.ldc(0, L(183, 18));
  const velocity = b.ldc(0, L(184, 20));

  // THE MATERIAL TOKEN. `wByte` is raw [0, 47] because `cov` is raw [0, 65536]
  // and Q16.16 MUL is (a*b)>>16 -- so this ADD moves the low byte only.
  const wSpan = b.ldc(SCORCH_WEIGHT_SPAN, L(188, 17));
  const wByte = b.mul(cov, wSpan, L(189, 17));
  const tokBase = b.ldc(materialTokenBase(MAT_SCORCH_A, MAT_SCORCH_B) + SCORCH_WEIGHT_LO,
                        L(190, 19));
  const material = b.add(tokBase, wByte, L(192, 20));

  // Scorched ground is dearer to cross, in proportion to how scorched it is.
  // Signed deltas combine by saturating addition (`compose_nav`); a positive
  // one makes the tile cost MORE and never speaks to passability.
  const nav = b.mul(cov, p4, L(197, 15));

  b.output('height', 'fx', height);
  b.output('velocity', 'fx', velocity);
  b.output('material', 'u32', material);
  b.output('nav_cost', 'fx', nav);
  b.end(L(203, 3));

  const program = buildProgram(b);
  const bytes = serializeProgram(program);
  const hash = programHashOfBytes(bytes);

  const dist2Pc = program.code.findIndex((i) => i.op === 'DIST2');
  if (dist2Pc < 0) throw new Error('scorch_wash: DIST2 instruction missing');
  const tokenAddPc = program.code.findIndex((i) => i.op === 'ADD');
  if (tokenAddPc < 0) throw new Error('scorch_wash: the token ADD is missing');

  // builder-side self-check: the serialized image must fully re-validate
  const decoded = decodeZprog(bytes);
  if (!decoded.ok) throw new Error(`scorch_wash: self-validation failed: ${decoded.errors}`);

  return { program, bytes, hash, dist2Pc, tokenAddPc };
}
