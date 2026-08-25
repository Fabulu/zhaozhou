// FIELD v2 throughput model — Phase 2.
// Inputs: measured/derived IIs from reports/FIELD_RESOURCE_MODEL.md;
// programs built live from the committed builders (hash-verified vs goldens).
import { buildCraterRing } from 'file://C:/programmieren/zencrifice/zhaozhou/compiler/dist/src/field_ir/crater_ring.js';
import { buildImpactWave } from 'file://C:/programmieren/zencrifice/zhaozhou/compiler/dist/src/field_ir/impact_wave.js';
import { buildWavePool } from 'file://C:/programmieren/zencrifice/zhaozhou/compiler/dist/src/field_ir/wave_pool.js';
import { OP_INFO } from 'file://C:/programmieren/zencrifice/zhaozhou/compiler/dist/src/field_ir/types.js';

const VERTS = 1089, ASSOC = 128, BUDGET = 1333333;

// unit map + per-op II (derived from FIELD_RESOURCE_MODEL.md; NORMALIZE3 measured 59)
const OPMAP = {
  END:{u:'FE',ii:0,lat:0}, MOV:{u:'ALU',ii:1,lat:1}, LDC:{u:'ALU',ii:1,lat:1},
  ADD:{u:'ALU',ii:1,lat:1}, SUB:{u:'ALU',ii:1,lat:1}, MIN:{u:'ALU',ii:1,lat:1},
  MAX:{u:'ALU',ii:1,lat:1}, ABS:{u:'ALU',ii:1,lat:1}, CLAMP:{u:'ALU',ii:1,lat:1},
  SELECT:{u:'ALU',ii:1,lat:1}, CMP:{u:'ALU',ii:1,lat:1},
  MUL:{u:'MUL',ii:1,lat:2}, MAD:{u:'MUL',ii:1,lat:2}, DOT2:{u:'MUL',ii:1,lat:2}, DOT3:{u:'MUL',ii:1,lat:2},
  SIN:{u:'SIN',ii:1,lat:2}, COS:{u:'SIN',ii:1,lat:2},
  RCP:{u:'RCP',ii:9,lat:9},
  CURVE:{u:'CURVE',ii:23,lat:23}, DCURVE:{u:'CURVE',ii:20,lat:20}, SPLINE:{u:'CURVE',ii:39,lat:39},
  LEN2:{u:'LEN',ii:42,lat:42}, LEN3:{u:'LEN',ii:42,lat:42}, DIST2:{u:'LEN',ii:42,lat:42},
  RING:{u:'RING',ii:48,lat:48},
  NOISE2:{u:'NOISE',ii:23,lat:23},
  ROT2:{u:'ROT',ii:20,lat:20}, ROT3:{u:'ROT',ii:21,lat:21},
  NORMALIZE2:{u:'NORM',ii:60,lat:60}, NORMALIZE3:{u:'NORM',ii:59,lat:59},
  RIDGE:{u:'RIDGE',ii:16,lat:16},
};

// single-wavefront in-order issue simulation: 1 instr/clock when operands ready.
// Operand-ready = producer issue time + latency. Returns completion time T_wf.
function wavefrontTime(code, opmap) {
  const ready = new Map(); // reg -> ready time
  let t = 0;
  for (const ins of code) {
    const info = OP_INFO[ins.op];
    const m = opmap[ins.op];
    let earliest = t;
    const srcs = [ins.a, ins.b, ins.c];
    info.srcGroups.forEach((w, gi) => {
      for (let k = 0; k < w; k++) {
        const r = srcs[gi] + k;
        if (ready.has(r)) earliest = Math.max(earliest, ready.get(r));
      }
    });
    t = earliest + 1; // issue takes one slot
    for (let k = 0; k < info.dstWidth; k++) ready.set(ins.dst + k, earliest + m.lat);
  }
  return t;
}

function analyze(name, prog, opmap, {W, N, L}) {
  const I = prog.code.length;
  const groups = Math.ceil(VERTS / W);
  const F = groups * I; // ideal front-end issue cycles / association
  // per-unit demand per association (scalar per-vertex requests)
  const dem = {};
  const cnt = {};
  for (const ins of prog.code) {
    const m = opmap[ins.op];
    if (m.u === 'FE') continue;
    cnt[m.u] = (cnt[m.u] || 0) + 1;
    dem[m.u] = (dem[m.u] || 0) + m.ii * VERTS;
  }
  if (dem.MUL) dem.MUL = Math.ceil(dem.MUL / L);
  if (dem.ALU !== undefined) dem.ALU = 0; // per-lane ALUs: W copies, retire with issue (assumption A3)
  const Twf = wavefrontTime(prog.code, opmap);
  const Feff = Math.max(F, Math.ceil(groups * Twf / N)); // barrel latency-hiding bound
  let bind = 'front-end', C = Feff;
  for (const [u, d] of Object.entries(dem)) if (d > C) { C = d; bind = u; }
  const util = {};
  for (const [u, d] of Object.entries(dem)) util[u] = d / C;
  util['front-end'] = F / C;
  return {name, W, N, L, I, F, Twf, Feff, dem, cnt, bind, C,
          total128: C * ASSOC, pct: (C * ASSOC / BUDGET * 100)};
}

const progs = {
  impact_wave: buildImpactWave().program,
  wave_pool: buildWavePool().program,
  crater_ring: buildCraterRing().program,
};

function sweep(opmap, label) {
  console.log(`\n===== scenario: ${label} =====`);
  for (const [name, prog] of Object.entries(progs)) {
    console.log(`\n--- ${name} (${prog.code.length} instr) Twf=${wavefrontTime(prog.code, opmap)} ---`);
    console.log('W N L |  F_fe  F_eff |  bind      C/assoc  128assoc   %budget | unit demands');
    for (const W of [2, 4]) for (const N of [4, 8]) for (const L of [1, 2, 3, 4]) {
      const r = analyze(name, prog, opmap, {W, N, L});
      const ds = Object.entries(r.dem).map(([u,d]) => `${u}:${d}`).join(' ');
      console.log(`${W} ${N} ${L} | ${String(r.F).padStart(6)} ${String(r.Feff).padStart(6)} | ${r.bind.padEnd(9)} ${String(r.C).padStart(7)} ${String(r.total128).padStart(9)} ${r.pct.toFixed(1).padStart(8)}% | ${ds}`);
    }
  }
}

// Scenario A: v2 barrel/SIMD front-end, units AS THEY ARE (ready-when-idle, II=latency)
sweep(OPMAP, 'A: units as-is (II = latency for long units)');

// Scenario B: CURVE/DCURVE, RING, LEN/DIST2 pipelined to II=1 (latency unchanged); RCP unchanged
const OPMAP_B = JSON.parse(JSON.stringify(OPMAP));
for (const op of ['CURVE','DCURVE','SPLINE','RING','LEN2','LEN3','DIST2']) OPMAP_B[op].ii = 1;
sweep(OPMAP_B, 'B: CURVE/DCURVE + RING + LEN/DIST2 pipelined II=1, RCP II=9 kept');

// closure thresholds: per-assoc budget and required effective II per unit, per program
console.log('\n===== closure arithmetic =====');
console.log('per-association budget =', Math.floor(BUDGET / ASSOC), 'cycles');
for (const [name, prog] of Object.entries(progs)) {
  const perUnitOps = {};
  for (const ins of prog.code) {
    const m = OPMAP[ins.op];
    if (m.u === 'FE' || m.u === 'ALU') continue;
    perUnitOps[m.u] = (perUnitOps[m.u] || 0) + 1;
  }
  const budget = BUDGET / ASSOC;
  const need = Object.entries(perUnitOps).map(([u, n]) =>
    `${u}: ${n} op(s) -> II<=${Math.floor(budget / (n * VERTS))}`).join('; ');
  console.log(`${name}: ${need}`);
}
