/**
 * Ledger validator unit tests (plan W2 acceptance): prove the validator
 * REJECTS the five canonical violations. Fixtures are minimal in-memory
 * documents; the `exists` probe is stubbed so no disk access is needed.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import { checkAll } from '../rules';
import type { Block, BlocksDoc, OpsDoc, Op } from '../types';

const exists = (p: string) => p.startsWith('tests/') || p.startsWith('design/');

function rtlBlock(over: Partial<Block> = {}): Block {
  return {
    id: 'CMD.DECODER',
    name: 'decoder',
    kind: 'rtl',
    subsystem: 'command',
    clock_domain: 'gpu',
    purpose: 'validate and dispatch semantic commands safely',
    contract: 'design/contracts/CMD.DECODER.md',
    phase: 1,
    owner_issue: 'ZH-008',
    inputs: ['bytes'],
    outputs: ['records'],
    upstream: ['CMD.DMA'],
    downstream: [],
    backpressure: 'ready_valid',
    latency: 'variable_bounded:4',
    target_throughput: '1 per clock',
    reference_model: 'zref::CmdDecoder',
    tests: { directed: 'tests/command/cmd_decoder_directed.cpp', random: 'tests/command/cmd_decoder_random.cpp' },
    counters: ['commands'],
    source_ids: true,
    budget_group: 'command_debug',
    maturity: 'SPECIFIED',
    maturity_log: [],
    deferred: false,
    superseded_by: null,
    ...over,
  };
}

function dmaBlock(over: Partial<Block> = {}): Block {
  return rtlBlock({
    id: 'CMD.DMA',
    subsystem: 'command',
    purpose: 'fetch sealed frame packets from the HPS-DDR ring',
    upstream: [],
    downstream: ['CMD.DECODER'],
    reference_model: 'zref::CmdDma',
    ...over,
  });
}

function blocksDoc(blocks: Block[]): BlocksDoc {
  return { schema_version: 1, counter_catalog: ['commands', 'frame_cycles'], blocks };
}

function op(over: Partial<Op> = {}): Op {
  return {
    id: 'FIELD.ADD',
    name: 'add',
    class: 'alu',
    profiles: ['E', 'W', 'F', 'M', 'S'],
    semantics: 'a + b in Q16.16, saturating, single rounding law',
    operand_q: ['spec/qformats.md §Q16.16'],
    result_q: 'spec/qformats.md §Q16.16',
    rounding: 'saturating',
    reference_function: 'zref::fieldir::op_add',
    implementation_blocks: ['FIELD.SEQ.EARTH'],
    cost_units: 1,
    field_ir_opcode: 'ADD',
    differential_tests: 'tests/differential/field_add.cpp',
    ...over,
  };
}

function opsDoc(ops: Op[]): OpsDoc {
  return {
    schema_version: 1,
    profiles: {
      E: { name: 'earth', description: 'Earth8 patch fields', sequencer: 'FIELD.SEQ.EARTH' },
      W: { name: 'warp', description: 'Warp8 deformation fields', sequencer: 'FIELD.SEQ.WARP' },
      F: { name: 'flow', description: 'particle force fields', sequencer: 'FIELD.SEQ.FLOW' },
      M: { name: 'formation', description: 'transform generation fields', sequencer: 'FIELD.SEQ.FORMATION' },
      S: { name: 'stamp', description: 'Scar Scribe sheet fields', sequencer: 'FIELD.SEQ.STAMP' },
    },
    ops,
  };
}

/** Baseline fixture: two gpu blocks, one edge, one op on all five profiles. */
function baseline(): { blocks: BlocksDoc; ops: OpsDoc } {
  const seqs: Block[] = (['EARTH', 'WARP', 'FLOW', 'FORMATION', 'STAMP'] as const).map((s) =>
    rtlBlock({
      id: `FIELD.SEQ.${s}`,
      subsystem: 'field',
      purpose: `evaluate the ${s.toLowerCase()} profile of Field IR programs`,
      upstream: [],
      downstream: [],
      reference_model: 'zref::fieldir::interpret',
      leaf: true,
    })
  );
  return {
    blocks: blocksDoc([dmaBlock(), rtlBlock(), ...seqs]),
    ops: opsDoc([
      op({
        implementation_blocks: ['FIELD.SEQ.EARTH', 'FIELD.SEQ.WARP', 'FIELD.SEQ.FLOW', 'FIELD.SEQ.FORMATION', 'FIELD.SEQ.STAMP'],
      }),
    ]),
  };
}

test('baseline fixture passes all rules (no false alarms)', () => {
  const { blocks, ops } = baseline();
  const errors = checkAll(blocks, ops, { exists });
  assert.deepEqual(errors, []);
});

test('V3: rejects an advanced maturity without evidence', () => {
  const { blocks, ops } = baseline();
  blocks.blocks[1] = rtlBlock({ maturity: 'RTL_VERIFIED', maturity_log: [] });
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(errors.some((e) => e.startsWith('V3:') && e.includes('CMD.DECODER')), errors.join('\n'));
});

test('V9: rejects an unknown op profile', () => {
  const { blocks, ops } = baseline();
  ops.ops[0] = op({ profiles: ['E', 'X'] as never });
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(errors.some((e) => e.startsWith('V9:') && e.includes('unknown profile')), errors.join('\n'));
});

test('V11: rejects an op referencing a nonexistent block', () => {
  const { blocks, ops } = baseline();
  ops.ops[0] = op({ implementation_blocks: ['FIELD.SEQ.NOSUCH'] });
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(errors.some((e) => e.startsWith('V11:') && e.includes('unknown block')), errors.join('\n'));
});

test('V7: rejects an edge referencing an unknown block', () => {
  const { blocks, ops } = baseline();
  blocks.blocks[0] = dmaBlock({ downstream: ['CMD.GHOST'] });
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(errors.some((e) => e.startsWith('V7:') && e.includes('CMD.GHOST')), errors.join('\n'));
});

test('V8: rejects a clock-domain-crossing edge without SYS.CDC routing / a documented bridge', () => {
  const { blocks, ops } = baseline();
  // CMD.DMA (gpu) -> SW.CMDBUILD (hps): crossing with async_bridge unset on both ends.
  const sw = rtlBlock({
    id: 'SW.CMDBUILD',
    kind: 'software',
    subsystem: 'sw',
    clock_domain: 'hps',
    purpose: 'build and seal semantic frame packets on the ARM side',
    upstream: ['CMD.DMA'],
    downstream: [],
    reference_model: undefined,
    tests: { unit: 'tests/sw/test_cmdbuild.ts' },
    counters: [],
    source_ids: undefined,
    budget_group: undefined,
  });
  blocks.blocks[0] = dmaBlock({ downstream: ['CMD.DECODER', 'SW.CMDBUILD'] });
  blocks.blocks.push(sw);
  let errors = checkAll(blocks, ops, { exists });
  assert.ok(errors.some((e) => e.startsWith('V8:')), errors.join('\n'));
  // Documented bridge on the DMA heals it.
  blocks.blocks[0] = dmaBlock({ downstream: ['CMD.DECODER', 'SW.CMDBUILD'], async_bridge: true });
  errors = checkAll(blocks, ops, { exists });
  assert.ok(!errors.some((e) => e.startsWith('V8:')), errors.join('\n'));
});

test('V2: rejects a maturity jump of more than one step vs git history', () => {
  const { blocks, ops } = baseline();
  const prev = baseline().blocks;
  prev.blocks[1] = rtlBlock({ maturity: 'SPECIFIED' });
  blocks.blocks[1] = rtlBlock({ maturity: 'UNIT_VERIFIED' });
  const errors = checkAll(blocks, ops, { prevBlocks: prev, exists });
  assert.ok(errors.some((e) => e.startsWith('V2:') && e.includes('steps')), errors.join('\n'));
});

test('V2: blocked_on hardware never advances from SPECIFIED', () => {
  const { blocks, ops } = baseline();
  blocks.blocks[0] = dmaBlock({ blocked_on: 'hardware', maturity: 'SPECIFIED' });
  const prev = baseline().blocks;
  prev.blocks[0] = dmaBlock({ blocked_on: 'hardware' });
  blocks.blocks[0] = dmaBlock({ blocked_on: 'hardware', maturity: 'REFERENCE_COMPLETE' });
  const errors = checkAll(blocks, ops, { prevBlocks: prev, exists });
  assert.ok(errors.some((e) => e.includes('blocked_on') || e.startsWith('V2:')), errors.join('\n'));
});

test('V7: rejects an asymmetric edge (downstream without matching upstream)', () => {
  const { blocks, ops } = baseline();
  blocks.blocks[1] = rtlBlock({ upstream: [] }); // CMD.DECODER no longer lists CMD.DMA
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(errors.some((e) => e.startsWith('V7:') && e.includes('not symmetric')), errors.join('\n'));
});

test('generators are deterministic: two renders are byte-identical', async () => {
  const { renderArchitecture } = await import('../gen/architecture');
  const { renderDashboard } = await import('../gen/dashboard');
  const { blocks, ops } = baseline();
  assert.equal(renderArchitecture(blocks), renderArchitecture(blocks));
  assert.equal(renderDashboard(blocks, ops), renderDashboard(blocks, ops));
});
