import { buildCraterRing } from 'file://C:/programmieren/zencrifice/zhaozhou/compiler/dist/src/field_ir/crater_ring.js';
import { buildImpactWave } from 'file://C:/programmieren/zencrifice/zhaozhou/compiler/dist/src/field_ir/impact_wave.js';
import { buildWavePool } from 'file://C:/programmieren/zencrifice/zhaozhou/compiler/dist/src/field_ir/wave_pool.js';

const progs = {
  impact_wave: buildImpactWave(),
  wave_pool: buildWavePool(),
  crater_ring: buildCraterRing(),
};

const agg = {};
const out = {};
for (const [name, b] of Object.entries(progs)) {
  const h = {};
  for (const ins of b.program.code) {
    h[ins.op] = (h[ins.op] || 0) + 1;
    agg[ins.op] = (agg[ins.op] || 0) + 1;
  }
  out[name] = {
    hash: '0x' + (b.hash >>> 0).toString(16),
    instrCount: b.program.code.length,
    histogram: h,
    codeList: b.program.code.map((i, pc) => `${pc}: ${i.op}`),
  };
}
out.__aggregate = agg;
console.log(JSON.stringify(out, null, 2));
