// field_v3_cost_model.mjs — the REGENERATED Field cost model (Fieldv3.md Phase 1).
//
// Committed, not archived: reports/FIELD_V3_COST_MODEL.md cites this script and
// every number in that report must reproduce from
//   node tools/report/field_v3_cost_model.mjs
// (run from the repo root; requires compiler/dist to be built).
//
// Inputs and their provenance:
//   MEASURED  restricted Fmax 59.22 MHz — reports/synthesis/zhao_block_fit.json
//             row zhao_field_v2_front (sourceCommit a03d336); setup -6.886 ns /
//             hold -1.938 ns from blockpaths/zhao_field_v2_front.sta.rpt.
//   MEASURED  transport: 12 input lanes x 1 clk + 4 output lanes x 3 read
//             phases + 1 handshake, per point — read from zhao_field_v2_front.sv
//             (reports/Fieldv3.md section 3).
//   MEASURED  the three committed Earth programs, built live from the committed
//             builders (the single sanctioned chain), hash-checked below.
//   DERIVED   v2 as-is unit IIs (ready-only-when-idle: II = latency) from
//             reports/FIELD_RESOURCE_MODEL.md, +-2 cycle error bar.
//   TARGET    v3 service IIs are the Phase 3 probe GATES (CURVE<=14,
//             DIST2<=20, vector MUL II 1, prepared RING 9 slots/group) — they
//             are acceptance targets, not measurements, until the probes fit.
import { buildCraterRing } from '../../compiler/dist/src/field_ir/crater_ring.js';
import { buildImpactWave } from '../../compiler/dist/src/field_ir/impact_wave.js';
import { buildWavePool } from '../../compiler/dist/src/field_ir/wave_pool.js';
import { OP_INFO } from '../../compiler/dist/src/field_ir/types.js';

const VERTS = 1089;                    // 33x33 lattice: the ACTUAL active vertex count of a full patch
const W = 4;                           // vector width
const GROUPS = Math.ceil(VERTS / W);   // 273 vector groups (1089 = 272*4 + 1; last group 1 live lane)
const ASSOC = 128;                     // stress-frame associations
const FMAX_V2 = 59.22e6;               // MEASURED
const CLOCKS = [100e6, 90e6, 80e6, FMAX_V2];
const RESERVE = 0.8;                   // 20% reserve
const FRAME_HZ = 60;

// Hash pins: the committed golden hashes (compiler/tests/generated/*.hpp).
const GOLDEN = { impact_wave: 0x82f5f4e4, wave_pool: 0x8bdceb63, crater_ring: 0x484add8d };

// v2 as-is IIs (DERIVED; ready-only-when-idle II = latency)
const V2_II = { RCP: 9, CURVE: 23, DCURVE: 20, SPLINE: 39, LEN2: 42, LEN3: 42, DIST2: 42,
                RING: 48, NOISE2: 23, ROT2: 20, ROT3: 21, NORMALIZE2: 60, NORMALIZE3: 59,
                RIDGE: 16, SIN: 1, COS: 1, MUL: 1, MAD: 1, DOT2: 1, DOT3: 1 };
const V2_UNIT = { RCP: 'RCP', CURVE: 'CURVE', DCURVE: 'CURVE', SPLINE: 'CURVE',
                  LEN2: 'LEN', LEN3: 'LEN', DIST2: 'LEN', RING: 'RING', NOISE2: 'NOISE',
                  ROT2: 'ROT', ROT3: 'ROT', NORMALIZE2: 'NORM', NORMALIZE3: 'NORM',
                  RIDGE: 'RIDGE', SIN: 'SIN', COS: 'SIN', MUL: 'MUL', MAD: 'MUL',
                  DOT2: 'MUL', DOT3: 'MUL' };

// v3 service targets (TARGET — Phase 3 probe gates)
const V3 = { CURVE_II: 14, DIST_II: 20, RING_MUL_SLOTS: 9 };

// ---------------------------------------------------------------------------
// uniform/varying taint over the physical code (forward taint, kill on overwrite)
// ---------------------------------------------------------------------------
function split(prog) {
  const varying = new Set();
  // Earth record: lanes 0,1 (x,z) vary per lattice point; everything else is
  // per-association uniform (field-ir.md 7.1; FIELD_V2_MODEL.md measured split).
  varying.add(prog.inputs[0].reg);
  varying.add(prog.inputs[1].reg);
  const cls = [];
  for (const ins of prog.code) {
    if (ins.op === 'END') { cls.push('end'); continue; }
    const info = OP_INFO[ins.op];
    let isVar = false;
    const srcs = [ins.a, ins.b, ins.c];
    info.srcGroups.forEach((w, gi) => {
      for (let k = 0; k < w; k++) if (varying.has(srcs[gi] + k)) isVar = true;
    });
    for (let k = 0; k < info.dstWidth; k++) {
      if (isVar) varying.add(ins.dst + k); else varying.delete(ins.dst + k);
    }
    cls.push(isVar ? 'varying' : 'uniform');
  }
  return cls;
}

// ---------------------------------------------------------------------------
// v3 demand vector (spec/form/cost-model.md section 5) per association
// ---------------------------------------------------------------------------
function demandVector(prog, cls) {
  const d = { vec_groups: GROUPS, vec_issue: 0, vmul_slots: 0, curve_req: 0,
              dist_req: 0, cold_ops: 0, uniform_ops: 0, table_bytes: 0,
              vreg_hwm: 0, sreg_hwm: 0 };
  const vregs = new Set(), sregs = new Set();
  prog.code.forEach((ins, i) => {
    if (cls[i] === 'end') return;
    const info = OP_INFO[ins.op];
    if (cls[i] === 'uniform') {
      d.uniform_ops += 1;
      for (let k = 0; k < info.dstWidth; k++) sregs.add(ins.dst + k);
      return;
    }
    d.vec_issue += 1;
    for (let k = 0; k < info.dstWidth; k++) vregs.add(ins.dst + k);
    switch (ins.op) {
      case 'MUL': case 'MAD': case 'DOT2': case 'DOT3': d.vmul_slots += 1; break;
      case 'CURVE': case 'DCURVE': d.curve_req += 1; d.vmul_slots += 1; break;
      case 'SPLINE': d.cold_ops += 1; break;
      case 'DIST2': case 'LEN2': case 'LEN3': d.dist_req += 1; d.vmul_slots += 1; break;
      case 'RING': d.vmul_slots += V3.RING_MUL_SLOTS; break; // prepared (radii uniform)
      case 'NORMALIZE2': case 'NORMALIZE3': case 'NOISE2': case 'RIDGE':
      case 'ROT2': case 'ROT3': d.cold_ops += 1; break;
      default: break; // plain ALU (incl. RCP if ever varying -> cold, guarded below)
    }
    if (ins.op === 'RCP') d.cold_ops += 1; // varying RCP: cold-lane exact scalar
  });
  for (const t of prog.tables) d.table_bytes += t.points.length * 12; // x,y,dy words
  d.vreg_hwm = vregs.size; d.sreg_hwm = sregs.size;
  return d;
}

// per-association clocks under the v3 targets
function v3Cost(d) {
  const issue = d.vec_issue * GROUPS;                    // 1 group-instr/clock
  const vmul  = d.vmul_slots * GROUPS;                   // II 1 bank
  const curve = d.curve_req * GROUPS * V3.CURVE_II;      // barrel service occupancy
  const dist  = d.dist_req * GROUPS * V3.DIST_II;        // two-bank root service
  const cold  = d.cold_ops * VERTS * 40;                 // pessimistic scalar cold lane
  const bind  = Math.max(issue, vmul, curve, dist, cold);
  const which = bind === dist ? 'DIST service' : bind === curve ? 'CURVE service'
              : bind === vmul ? 'vector MUL' : bind === cold ? 'cold lane' : 'issue';
  return { issue, vmul, curve, dist, bind, which };
}

// v2 as-built honest cost per association (transport + serialized long units)
function v2Cost(prog) {
  const transport = 12 * VERTS + (4 * 3 + 1) * VERTS;    // 27,225 (measured from the front RTL)
  const dem = {};
  for (const ins of prog.code) {
    if (ins.op === 'END' || ins.op === 'LDC') continue;
    const u = V2_UNIT[ins.op];
    if (!u || u === 'MUL' || u === 'SIN') continue;      // pipelined II=1 units never bind here
    dem[u] = (dem[u] || 0) + V2_II[ins.op] * VERTS;      // 4 lanes serialize through 1 scalar unit
  }
  const entries = Object.entries(dem).sort((a, b) => b[1] - a[1]);
  const worst = entries.length ? entries[0] : ['-', 0];
  return { transport, unit: worst[0], unitDemand: worst[1], total: transport + worst[1] };
}

const built = [
  ['impact_wave', buildImpactWave()],
  ['wave_pool', buildWavePool()],
  ['crater_ring', buildCraterRing()],
];
for (const [name, b] of built) {
  if (b.hash >>> 0 !== GOLDEN[name]) {
    throw new Error(name + ': built hash 0x' + (b.hash >>> 0).toString(16) +
                    ' != golden 0x' + GOLDEN[name].toString(16) + ' — model would describe a stale build');
  }
}

console.log('=== Field v3 regenerated cost model ===');
console.log('vector groups/assoc (W=4, ' + VERTS + ' verts): ' + GROUPS);
for (const [name, b] of built) {
  const prog = b.program;
  const cls = split(prog);
  const nVar = cls.filter((c) => c === 'varying').length;
  const nUni = cls.filter((c) => c === 'uniform').length;
  const d = demandVector(prog, cls);
  const c3 = v3Cost(d);
  const c2 = v2Cost(prog);
  console.log('');
  console.log('--- ' + name + ' (' + prog.code.length + ' instrs: ' + nVar + ' varying / ' + nUni + ' uniform + END) ---');
  console.log('demand vector: ' + JSON.stringify(d));
  console.log('v3/assoc: issue ' + c3.issue + '  vmul ' + c3.vmul + '  curve ' + c3.curve +
              '  dist ' + c3.dist + '  -> binds on ' + c3.which + ' at ' + c3.bind +
              ' clocks (hard target <= 6000)');
  console.log('v2 honest/assoc: transport ' + c2.transport + ' + worst unit ' + c2.unit + ' ' +
              c2.unitDemand + ' = ' + c2.total + ' clocks');
}

// frame roll-up under v3 (uses the hard 6,000 target as the per-assoc bound)
const initFinal = ASSOC * 2 * GROUPS;
const frame = ASSOC * 6000 + initFinal;
console.log('');
console.log('=== frame roll-up (128 associations) ===');
console.log('v3: 128 x 6,000 + init/final ' + initFinal + ' = ' + frame +
            ' (acceptance ceiling 850,000 incl. plan/table/setup margin)');
for (const f of CLOCKS) {
  const usable = Math.round((f / FRAME_HZ) * RESERVE);
  console.log('  ' + (f / 1e6).toFixed(2) + ' MHz: usable ' + usable + '/frame, 850k = ' +
              ((850000 / usable) * 100).toFixed(0) + '%');
}
const usable59 = Math.round((FMAX_V2 / FRAME_HZ) * RESERVE);
console.log('');
console.log('v2 as-built at its measured 59.22 MHz (usable ' + usable59 + '/frame):');
for (const [name, b] of built) {
  const t = v2Cost(b.program).total * ASSOC;
  console.log('  ' + name + ': ' + t + ' clocks/frame = ' + ((t / usable59) * 100).toFixed(0) + '%');
}
console.log('');
console.log('patch reduction (TERRAIN.PATCH): vertex-major 1+n costs ' + (2 * VERTS) +
            ' clocks/assoc at n=1; field-major four-bank RMW costs ' + GROUPS +
            ' vector clocks per pass (init/final counted above).');
