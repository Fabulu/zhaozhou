// material_program.test.ts — `scorch_wash`, the first Field IR program in this
// tree whose out-lane 2 is a LEGAL v1 material token (spec/qformats.md §14.1).
//
// build → validate → serialize → emit the committed C++ wrapper, exactly as
// `wave_programs.test.ts` does for wave_pool/impact_wave (field-ir.md §11.3
// byte-stability: this test regenerates and fails on drift).
//
// ---------------------------------------------------------------------------
// AND IT IS THE HOME OF THE NON-COINCIDENCE CHECK
// ---------------------------------------------------------------------------
// The owner's I34 acceptance clause 3 is that the field's material "cannot equal
// the authored baseline by accident". `scorch_wash`'s three bytes are chosen to
// be outside every byte range the console smoke's authored layer-E plane can
// produce -- but a constant chosen for a property and never checked again is how
// this campaign's flattering-direction failures happen. So the check below does
// NOT restate the authored ranges: it PARSES THEM OUT OF THE BENCH, and fails if
// the disjointness is ever destroyed from either side.
//
// That is the difference between an argument and an instrument. If somebody
// widens `TERR_MAT_SPAN_C`, or moves `SGF_TERR_WEIGHT_LO_C`, or edits
// `MAT_SCORCH_A`, this test goes RED rather than the console quietly acquiring a
// field material that a cell might have authored anyway.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';

import {
  buildScorchWash,
  MAT_SCORCH_A,
  MAT_SCORCH_B,
  SCORCH_WEIGHT_LO,
  SCORCH_WEIGHT_SPAN,
  SCORCH_TOKEN_LO,
  SCORCH_TOKEN_HI,
  SCORCH_CONSOLE_UNIFORMS,
} from '../src/field_ir/scorch_wash.js';
import {
  MATERIAL_TOKEN_TAG_V1,
  materialToken,
  materialTokenTagOk,
  materialTokenDecode,
} from '../src/field_ir/material_token.js';
import { emitCppWrapper } from '../src/field_ir/emit_cpp.js';
import { interpret } from '../src/field_ir/interpret.js';
import { decodeZprog } from '../src/field_ir/serialize.js';
import { repoRoot } from './helpers.js';

const GENERATED_DIR = path.join(repoRoot(), 'compiler', 'tests', 'generated');

function stableFile(rel: string, bytes: Buffer | string, note: string): void {
  const abs = path.join(GENERATED_DIR, rel);
  if (!existsSync(abs)) {
    writeFileSync(abs, bytes);
    console.log(`[material_program] WROTE ${rel} (${note}) — commit it`);
    return;
  }
  const committed = readFileSync(abs);
  const want = Buffer.isBuffer(bytes) ? bytes : Buffer.from(bytes, 'utf8');
  assert.ok(committed.equals(want),
    `${rel} is stale — regenerate and commit (field-ir.md §11.3 determinism)`);
}

// ---------------------------------------------------------------------------
// §14.1's bit diagram, transcribed ONCE, here, from the spec and not from the
// implementation it checks. The point of writing the literal out is that a bug
// in `materialTokenBase`'s shifts cannot hide behind the same bug in the
// expectation.
// ---------------------------------------------------------------------------
test('material token: the packing is §14.1 and the tag is 0xE1', () => {
  assert.equal(MATERIAL_TOKEN_TAG_V1, 0xe1);

  // the value tests/differential/material_token_directed.cpp and
  // composepub_acceptance case 12 both use: deliberately asymmetric, so a
  // swapped, rotated or truncated layout cannot pass.
  assert.equal(materialToken(0x2a, 0x7c, 0xb3) >>> 0, 0xe12a7cb3);
  assert.ok(materialTokenTagOk(0xe12a7cb3 | 0));
  assert.ok(!materialTokenTagOk(0x002a7cb3 | 0));
  assert.deepEqual(materialTokenDecode(0xe12a7cb3 | 0),
    { matA: 0x2a, matB: 0x7c, weight: 0xb3 });
  assert.equal(materialTokenDecode(0x002a7cb3 | 0), null);

  // a bare material id -- what wave_pool and crater_ring actually emit -- is
  // NOT a token, and that is the defect this program exists to not have.
  assert.ok(!materialTokenTagOk(1));
  assert.ok(!materialTokenTagOk(2));
});

test('scorch_wash: emit + commit the C++ wrapper and .zprog', () => {
  mkdirSync(GENERATED_DIR, { recursive: true });
  const { program, bytes, hash, dist2Pc, tokenAddPc } = buildScorchWash();

  // shape: the earth profile's twelve lanes and four canonical outputs, so the
  // required-output mask is 0x0F -- the same shape wave_pool has.
  assert.ok(program.code.length <= 32, `instrs ${program.code.length} > 32`);
  assert.equal(program.inputs.length, 12);
  assert.equal(program.outputs.length, 4);
  assert.deepEqual(program.outputs.map((o) => o.name),
    ['height', 'velocity', 'material', 'nav_cost']);
  assert.equal(program.outputs[2]!.type, 'u32');

  // NO BITWISE OPS WERE USED, because there are none to use. If a future ISA
  // gains them this assertion is the thing that says "the arithmetic assembly
  // in scorch_wash.ts can be simplified now".
  assert.ok(program.code.some((i) => i.op === 'DIST2'));
  assert.ok(program.code.some((i) => i.op === 'ADD'));

  const wrapper = emitCppWrapper(program, bytes, hash, {
    namespace: 'scorch_wash',
    // NOT a .form file. wave_pool.hpp/crater_ring.hpp both cite
    // "spells/membrane.form", which does not exist in this tree; the TS module
    // is this program's actual source and is named as such.
    sourceName: 'compiler/src/field_ir/scorch_wash.ts (scorch_wash earth program)',
    markers: [
      { name: 'dist2', pc: dist2Pc, sourceId: 0x30010004, line: 177, col: 13 },
      { name: 'tokenAdd', pc: tokenAddPc, sourceId: 0x30010004, line: 192, col: 20 },
    ],
  });
  stableFile('scorch_wash.hpp', wrapper, 'generated typed wrapper');
  stableFile('scorch_wash.zprog', Buffer.from(bytes), 'serialized program');

  assert.ok(wrapper.includes(`kProgramHash = 0x${(hash >>> 0).toString(16)}`));
  console.log(`[scorch_wash] program hash = 0x${(hash >>> 0).toString(16)}` +
    ` (${program.code.length} instrs, DIST2 at pc ${dist2Pc}, token ADD at pc ${tokenAddPc})`);
});

// ---------------------------------------------------------------------------
// The program's out-lane 2 is a token FOR EVERY INPUT, not for one sample.
// Swept over the footprint and beyond it, plus the degenerate case the
// smoothstep macro's clamp exists to survive.
// ---------------------------------------------------------------------------
test('scorch_wash: out-lane 2 is a v1 token over the whole domain', () => {
  const { bytes } = buildScorchWash();
  const decRes = decodeZprog(bytes);
  if (!decRes.ok) throw new Error(decRes.errors.join('; '));
  const prog = decRes.prog;
  const fx = (v: number): number => Math.round(v * 65536);

  // p0,p1 centre; p2 r_in; p3 r_out; p4 nav surcharge
  const params = [fx(0), fx(0), fx(6), fx(14), fx(2), 0, 0, 0];

  let sawLo = false, sawHi = false, sawMiddle = false;
  const seen = new Set<number>();

  for (let ix = -40; ix <= 40; ix += 1) {
    for (let iz = -40; iz <= 40; iz += 5) {
      const inputs = [fx(ix), fx(iz), 0, fx(0.5), ...params];
      const out = interpret(prog, inputs).outputs;

      const tok = out[2]! | 0;
      assert.ok(materialTokenTagOk(tok),
        `out-lane 2 at (${ix},${iz}) is not a v1 token: 0x${(tok >>> 0).toString(16)}`);
      const d = materialTokenDecode(tok)!;
      assert.equal(d.matA, MAT_SCORCH_A, `matA moved at (${ix},${iz})`);
      assert.equal(d.matB, MAT_SCORCH_B, `matB moved at (${ix},${iz})`);
      assert.ok(d.weight >= SCORCH_WEIGHT_LO,
        `weight ${d.weight} below the floor at (${ix},${iz})`);
      assert.ok(d.weight <= SCORCH_WEIGHT_LO + SCORCH_WEIGHT_SPAN,
        `weight ${d.weight} above the ceiling at (${ix},${iz})`);
      seen.add(d.weight);
      if (d.weight === SCORCH_WEIGHT_LO) sawLo = true;
      if (d.weight === SCORCH_WEIGHT_LO + SCORCH_WEIGHT_SPAN) sawHi = true;
      if (d.weight > SCORCH_WEIGHT_LO && d.weight < SCORCH_WEIGHT_LO + SCORCH_WEIGHT_SPAN) {
        sawMiddle = true;
      }

      // height and velocity are EXACTLY zero -- the whole experimental design.
      assert.equal(out[0], 0, `height is not zero at (${ix},${iz})`);
      assert.equal(out[1], 0, `velocity is not zero at (${ix},${iz})`);
    }
  }

  // ANTI-VACUITY FOR THIS TEST ITSELF. A sweep that only ever saw one weight
  // would pass every assertion above while proving the lane is a constant.
  assert.ok(sawLo, 'the sweep never reached the clean rim (weight floor)');
  assert.ok(sawHi, 'the sweep never reached full scorch (weight ceiling)');
  assert.ok(sawMiddle, 'the sweep never landed on the soft rim -- the field does not vary');
  assert.ok(seen.size > 8, `only ${seen.size} distinct weights -- the rim is not resolved`);

  // the degenerate footprint the macro's internal clamp exists for: r_in == r_out
  // makes the interpolant a division by zero. The token must STILL be legal.
  for (const deg of [[fx(6), fx(6)], [fx(14), fx(6)], [fx(0), fx(0)]]) {
    const inputs = [fx(3), fx(3), 0, fx(0.5), fx(0), fx(0), deg[0]!, deg[1]!, fx(2), 0, 0, 0];
    const out = interpret(prog, inputs).outputs;
    const tok = out[2]! | 0;
    assert.ok(materialTokenTagOk(tok),
      `degenerate footprint produced a non-token 0x${(tok >>> 0).toString(16)}`);
    const d = materialTokenDecode(tok)!;
    assert.equal(d.matA, MAT_SCORCH_A, 'degenerate footprint moved matA');
    assert.equal(d.matB, MAT_SCORCH_B, 'degenerate footprint moved matB');
  }

  console.log(`[scorch_wash] token range 0x${(SCORCH_TOKEN_LO >>> 0).toString(16)}` +
    `..0x${(SCORCH_TOKEN_HI >>> 0).toString(16)}, ${seen.size} distinct weights over the sweep`);
});

// ---------------------------------------------------------------------------
// CLAUSE 3, AS AN INSTRUMENT RATHER THAN AN ARGUMENT.
// ---------------------------------------------------------------------------
test('scorch_wash: no byte of the token is a byte the authored plane can author', () => {
  const bench = readFileSync(
    path.join(repoRoot(), 'tests', 'prod', 'tb_zhao_console_core_smoke.sv'), 'utf8');

  // Parse the authored layer-E ranges out of the bench. If a localparam is
  // renamed or removed, this THROWS rather than silently checking nothing --
  // which is the failure mode a regex-based check normally has.
  const num = (name: string): number => {
    const re = new RegExp(
      `localparam\\s+(?:logic\\s*\\[[^\\]]*\\]|int\\s+unsigned|int)\\s+${name}\\s*=\\s*` +
      `(?:\\d+'[hdb])?([0-9a-fA-F]+)\\s*;`);
    const m = re.exec(bench);
    assert.ok(m, `the bench no longer declares ${name} -- ` +
      'this check cannot verify the authored range and must not pass silently');
    const raw = m![1]!;
    const hex = /'h/.test(m![0]!);
    return parseInt(raw, hex ? 16 : 10);
  };

  const aLo = num('SGF_TERR_MAT_A_LO_C');
  const bLo = num('SGF_TERR_MAT_B_LO_C');
  const matSpan = num('TERR_MAT_SPAN_C');
  const wLo = num('SGF_TERR_WEIGHT_LO_C');
  const wSpan = num('TERR_WEIGHT_SPAN_C');

  // The bench's writer is `lo + ((k) % span)`, so the reachable set is
  // [lo, lo + span - 1] for each of the three bytes.
  const authored = new Set<number>();
  for (let i = 0; i < matSpan; ++i) { authored.add(aLo + i); authored.add(bLo + i); }
  for (let i = 0; i < wSpan; ++i) authored.add(wLo + i);

  console.log(`[scorch_wash] authored layer E: matA in [${aLo},${aLo + matSpan - 1}], ` +
    `matB in [${bLo},${bLo + matSpan - 1}], weight in [0x${wLo.toString(16)},` +
    `0x${(wLo + wSpan - 1).toString(16)}] -- ${authored.size} distinct byte values`);

  // EVERY byte this program can emit, against EVERY byte the plane can author.
  const fieldBytes = [MAT_SCORCH_A, MAT_SCORCH_B];
  for (let w = SCORCH_WEIGHT_LO; w <= SCORCH_WEIGHT_LO + SCORCH_WEIGHT_SPAN; ++w) {
    fieldBytes.push(w);
  }
  for (const fb of fieldBytes) {
    assert.ok(!authored.has(fb),
      `scorch_wash can emit byte 0x${fb.toString(16)}, which the authored layer-E ` +
      'plane can also produce -- clause 3 non-coincidence is DESTROYED. Move ' +
      'MAT_SCORCH_A/B or SCORCH_WEIGHT_LO, or narrow the bench range.');
  }

  // and the whole-triple statement, which is what the consumer compares
  assert.ok(!(authored.has(MAT_SCORCH_A) && authored.has(MAT_SCORCH_B)),
    'both candidate ids are authorable');
  console.log(`[scorch_wash] ${fieldBytes.length} field byte values, ` +
    'none of them authorable: clause 3 holds by layout');
});

// ---------------------------------------------------------------------------
// THE CONSOLE'S OWN UNIFORM SET, over every patch the fixture could place.
//
// `smoke_field_fixture_gen.cpp` preloads `SCORCH_CONSOLE_UNIFORMS`. The console
// places ONE of the geom fixture's terrain records and this test does not assume
// which, so it checks EVERY candidate: a centre-on-the-datum falloff has to make
// out-lane 2 vary over whichever 32 m patch is chosen. A constant material in
// the console would satisfy clause 3 and still be weak evidence, because a
// constant cannot demonstrate that the engine consumed the VARYING lanes.
//
// Patch geometry, from tb_zhao_console_core_smoke.sv: patch (ix,iz) spans
// x in [ix*32, (ix+1)*32] m and z in [iz*32, (iz+1)*32] m at pitch_log2 0, with
// SGF_TERR_IX0 = 0 and SGF_TERR_IZ0 = 1 over SGF_TERR_RECORDS = 3 records.
// ---------------------------------------------------------------------------
test('scorch_wash: the console uniform set varies over every candidate patch', () => {
  const { bytes } = buildScorchWash();
  const decRes = decodeZprog(bytes);
  if (!decRes.ok) throw new Error(decRes.errors.join('; '));
  const prog = decRes.prog;
  const u = SCORCH_CONSOLE_UNIFORMS;
  const params = [u.centreX, u.centreZ, u.rIn, u.rOut, u.nav, 0, 0, 0];

  const IX0 = 0, IZ0 = 1, RECORDS = 3, EDGE = 32;
  for (let r = 0; r < RECORDS; ++r) {
    const ix = r + IX0, iz = r + IZ0;
    const weights = new Set<number>();
    for (let vi = 0; vi <= EDGE; ++vi) {
      for (let vj = 0; vj <= EDGE; ++vj) {
        const wx = (ix * EDGE + vi) * 65536;
        const wz = (iz * EDGE + vj) * 65536;
        const out = interpret(prog, [wx, wz, u.age, u.phase, ...params]).outputs;
        const tok = out[2]! | 0;
        assert.ok(materialTokenTagOk(tok),
          `patch (${ix},${iz}) vertex (${vi},${vj}) produced a non-token`);
        const d = materialTokenDecode(tok)!;
        assert.equal(d.matA, MAT_SCORCH_A, `patch (${ix},${iz}): matA moved`);
        assert.equal(d.matB, MAT_SCORCH_B, `patch (${ix},${iz}): matB moved`);
        assert.ok(d.weight >= SCORCH_WEIGHT_LO &&
                  d.weight <= SCORCH_WEIGHT_LO + SCORCH_WEIGHT_SPAN,
          `patch (${ix},${iz}): weight ${d.weight} outside the declared band`);
        weights.add(d.weight);
        assert.equal(out[0], 0, `patch (${ix},${iz}): height is not zero`);
        assert.equal(out[1], 0, `patch (${ix},${iz}): velocity is not zero`);
      }
    }
    // THE ANTI-VACUITY ASSERTION FOR THE CONSOLE'S ACTUAL STIMULUS.
    assert.ok(weights.size > 1,
      `patch (${ix},${iz}): the console uniform set makes out-lane 2 CONSTANT ` +
      `(${[...weights]}) -- it cannot then show the engine read the varying lanes. ` +
      'Move SCORCH_CONSOLE_UNIFORMS.rOut so the falloff crosses this patch.');
    const lo = Math.min(...weights), hi = Math.max(...weights);
    console.log(`[scorch_wash] patch (${ix},${iz}): ${weights.size} distinct weights, ` +
      `0x${lo.toString(16)}..0x${hi.toString(16)} over its 33x33 lattice`);
  }
});
