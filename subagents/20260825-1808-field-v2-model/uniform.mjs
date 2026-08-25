import { buildCraterRing } from 'file://C:/programmieren/zencrifice/zhaozhou/compiler/dist/src/field_ir/crater_ring.js';
import { buildImpactWave } from 'file://C:/programmieren/zencrifice/zhaozhou/compiler/dist/src/field_ir/impact_wave.js';
import { buildWavePool } from 'file://C:/programmieren/zencrifice/zhaozhou/compiler/dist/src/field_ir/wave_pool.js';
import { OP_INFO } from 'file://C:/programmieren/zencrifice/zhaozhou/compiler/dist/src/field_ir/types.js';

const VERTS=1089, ASSOC=128, BUDGET=1333333;
const II = {LDC:1,ADD:1,SUB:1,MIN:1,MAX:1,ABS:1,CLAMP:1,SELECT:1,CMP:1,MOV:1,
  MUL:1,MAD:1,SIN:1,COS:1,RCP:9,CURVE:23,DCURVE:20,SPLINE:39,LEN2:42,LEN3:42,
  DIST2:42,RING:48,NOISE2:23,ROT2:20,ROT3:21,NORMALIZE2:60,NORMALIZE3:59,RIDGE:16};
const UNIT = {MUL:'MUL',MAD:'MUL',DOT2:'MUL',DOT3:'MUL',SIN:'SIN',COS:'SIN',
  RCP:'RCP',CURVE:'CURVE',DCURVE:'CURVE',SPLINE:'CURVE',LEN2:'LEN',LEN3:'LEN',
  DIST2:'LEN',RING:'RING',NOISE2:'NOISE',NORMALIZE2:'NORM',NORMALIZE3:'NORM'};

const progs = { impact_wave: buildImpactWave().program, wave_pool: buildWavePool().program, crater_ring: buildCraterRing().program };

for (const [name,prog] of Object.entries(progs)) {
  // varying seeds: input regs holding x and z (per-vertex); everything else per-association
  const varying = new Set(prog.inputs.filter(l=>l.name==='x'||l.name==='z').map(l=>l.reg));
  let nVar=0, nUni=0;
  const varyCnt={}, varDem={};
  const rows=[];
  for (const ins of prog.code) {
    if (ins.op==='END') continue;
    const info=OP_INFO[ins.op];
    let isVar=false;
    const srcs=[ins.a,ins.b,ins.c];
    info.srcGroups.forEach((w,gi)=>{ for(let k=0;k<w;k++) if (varying.has(srcs[gi]+k)) isVar=true; });
    for(let k=0;k<info.dstWidth;k++){ if(isVar) varying.add(ins.dst+k); else varying.delete(ins.dst+k); }
    rows.push(`${ins.op}${isVar?'':' [UNIFORM]'}`);
    if (isVar){ nVar++; const u=UNIT[ins.op]||'ALU'; varyCnt[ins.op]=(varyCnt[ins.op]||0)+1;
      if(u!=='ALU') varDem[u]=(varDem[u]||0)+II[ins.op]*VERTS; }
    else nUni++;
  }
  console.log(`\n=== ${name}: ${nVar} varying + ${nUni} uniform (of ${prog.code.length-1}+END)`);
  console.log(rows.join(', '));
  console.log('varying histogram:', JSON.stringify(varyCnt));
  console.log('per-assoc unit demand (varying only, as-is II):', JSON.stringify(varDem));
  const b=Math.floor(BUDGET/ASSOC);
  for (const [u,d] of Object.entries(varDem)) {
    const ops=d/VERTS; // weighted II sum; report needed II
  }
  // needed effective II per unit for closure (varying ops only)
  const opsPerUnit={};
  for (const ins of prog.code){ if(ins.op==='END')continue;
    const u=UNIT[ins.op]||'ALU'; if(u==='ALU')continue;
    // recompute varying using rows: simpler—redo classification
  }
  console.log('per-assoc budget:', b, '-> front-end W=4 issue (varying only):', Math.ceil(VERTS/4)*nVar, `(${(Math.ceil(VERTS/4)*nVar*128/BUDGET*100).toFixed(1)}% of budget)`);
}
