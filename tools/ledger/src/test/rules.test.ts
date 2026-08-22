/**
 * Ledger validator unit tests (plan W2 acceptance): prove the validator
 * REJECTS the five canonical violations. Fixtures are minimal in-memory
 * documents; the `exists` probe is stubbed so no disk access is needed.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import { checkAll, checkCitations, checkProseClaims, checkScopeGuards, type RtlFile, type SbyTask } from '../rules';
import type { Block, BlocksDoc, OpsDoc, Op, FormalRunsDoc, FormalRunEntry } from '../types';

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

test('V15: rejects a duplicate counter_catalog entry (counter_id = index needs a dense index space)', () => {
  const { blocks, ops } = baseline();
  blocks.counter_catalog = ['commands', 'frame_cycles', 'commands'];
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(errors.some((e) => e.startsWith('V15:') && e.includes('duplicate-free')), errors.join('\n'));
});

test('V15: rejects a block declaring the same counter twice', () => {
  const { blocks, ops } = baseline();
  blocks.blocks[1] = rtlBlock({ counters: ['commands', 'commands'] });
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(errors.some((e) => e.startsWith('V15:') && e.includes('twice')), errors.join('\n'));
});

test('V12: a block counter outside the catalog is rejected (counter_id totality)', () => {
  const { blocks, ops } = baseline();
  blocks.blocks[1] = rtlBlock({ counters: ['not_a_catalog_counter'] });
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(errors.some((e) => e.startsWith('V12:') && e.includes('not in counter_catalog')), errors.join('\n'));
});

// ---------------------------------------------------------------------------
// V16 — a formal property may back a maturity claim only if the lane recorded
// a GREEN run for it. These tests encode the two wave-2 escapes directly:
// a proof that had never been elaborated, and a proof passing vacuously for
// want of cover statements.
// ---------------------------------------------------------------------------

function formalDoc(over: Partial<FormalRunEntry> = {}): FormalRunsDoc {
  return {
    version: 1,
    runs: [
      {
        property: 'tests/formal/cmd_decoder_safe.sby',
        status: 'green',
        date: '2026-08-16',
        commit: 'abc1234',
        lane: 'formal_cmd_decoder_safe',
        tasks: ['bmc', 'cover'],
        covers: true,
        ...over,
      },
    ],
  };
}

function provenBlocks() {
  const { blocks, ops } = baseline();
  blocks.blocks[1] = rtlBlock({
    maturity: 'RTL_VERIFIED',
    maturity_log: [
      { state: 'RTL_VERIFIED', date: '2026-08-16', commit: 'abc1234', evidence: 'tests/command/cmd_decoder_random.cpp' },
    ],
    tests: {
      directed: 'tests/command/cmd_decoder_directed.cpp',
      random: 'tests/command/cmd_decoder_random.cpp',
      formal: 'tests/formal/cmd_decoder_safe.sby',
    },
  });
  return { blocks, ops };
}

test('V16: a green, covered formal run backs an RTL_VERIFIED claim', () => {
  const { blocks, ops } = provenBlocks();
  const errors = checkAll(blocks, ops, { exists }, formalDoc());
  assert.deepEqual(errors, []);
});

test('V16: a property that has NEVER RUN is a hard failure, not a skip', () => {
  const { blocks, ops } = provenBlocks();
  const errors = checkAll(blocks, ops, { exists }, formalDoc({ status: 'never_ran' }));
  assert.ok(errors.some((e) => /V16.*never_ran/.test(e)), errors.join('\n'));
});

test('V16: a proof with no cover task cannot back a maturity claim (vacuity)', () => {
  const { blocks, ops } = provenBlocks();
  const errors = checkAll(blocks, ops, { exists }, formalDoc({ tasks: ['bmc'], covers: false }));
  assert.ok(errors.some((e) => /V16.*covers: false/.test(e)), errors.join('\n'));
});

test('V16: banked evidence cannot back a maturity claim', () => {
  const { blocks, ops } = provenBlocks();
  const errors = checkAll(blocks, ops, { exists }, formalDoc({ status: 'banked' }));
  assert.ok(errors.some((e) => /V16.*not green/.test(e)), errors.join('\n'));
});

test('V16: covers: true must be backed by an actual cover task name', () => {
  const { blocks, ops } = provenBlocks();
  const errors = checkAll(blocks, ops, { exists }, formalDoc({ tasks: ['bmc'], covers: true }));
  assert.ok(errors.some((e) => /V16.*claims covers: true/.test(e)), errors.join('\n'));
});

test('V16: a .sby on disk with no registry entry is rejected', () => {
  const { blocks, ops } = provenBlocks();
  const errors = checkAll(
    blocks,
    ops,
    { exists, formalTasksOnDisk: ['tests/formal/cmd_decoder_safe.sby', 'tests/formal/orphan.sby'] },
    formalDoc()
  );
  assert.ok(errors.some((e) => /V16.*orphan\.sby.*HARD FAILURE/.test(e)), errors.join('\n'));
});

// ---------------------------------------------------------------------------
// V19 — every bounded (mode bmc) proof carries a self-asserting scope guard
// in its cone, or an explicit SCOPE-TOTAL waiver. Encodes the census this
// rule was built from: six bounded harnesses whose scope lived only in
// comments, where it could drift without a red.
// ---------------------------------------------------------------------------

function sby(over: Partial<SbyTask> = {}): SbyTask {
  return {
    path: 'tests/formal/example.sby',
    text: '[options]\nmode bmc\ndepth 30\n\n[files]\nharness.sv\n',
    sources: ['module h;\n  a_scope_window: assert(f_cyc <= 30);\nendmodule\n'],
    ...over,
  };
}

test('V19: a guarded bounded proof passes', () => {
  assert.deepEqual(checkScopeGuards([sby()]), []);
});

test('V19: a bounded proof with no scope guard anywhere in its cone is rejected', () => {
  const errors = checkScopeGuards([sby({ sources: ['module h;\nendmodule\n'] })]);
  assert.ok(errors.some((e) => e.startsWith('V19:') && e.includes('example.sby')), errors.join('\n'));
});

test('V19: task-prefixed bmc mode lines (bmc: mode bmc) are recognised as bounded', () => {
  const errors = checkScopeGuards([
    sby({
      text: '[tasks]\nbmc\ncover\n\n[options]\nbmc: mode bmc\nbmc: depth 24\ncover: mode cover\n',
      sources: ['module h;\nendmodule\n'],
    }),
  ]);
  assert.ok(errors.some((e) => e.startsWith('V19:')), errors.join('\n'));
});

test('V19: an unbounded (mode prove) proof needs no guard — its scope IS total', () => {
  const errors = checkScopeGuards([
    sby({
      text: '[tasks]\nprove\ncover\n\n[options]\nprove: mode prove\ncover: mode cover\n',
      sources: ['module h;\nendmodule\n'],
    }),
  ]);
  assert.deepEqual(errors, []);
});

test('V19: an explicit SCOPE-TOTAL waiver in the .sby is accepted', () => {
  const errors = checkScopeGuards([
    sby({
      text: '# SCOPE-TOTAL: depth 12 exceeds the 9-state FSM diameter (exhaustive).\n[options]\nmode bmc\ndepth 12\n',
      sources: ['module h;\nendmodule\n'],
    }),
  ]);
  assert.deepEqual(errors, []);
});

test('V19: the arbiter a_horizon_* guard naming is accepted', () => {
  const errors = checkScopeGuards([
    sby({ sources: ['module h;\n  a_horizon_is_refresh_free: assert(cnt < LIMIT);\nendmodule\n'] }),
  ]);
  assert.deepEqual(errors, []);
});

// ---------------------------------------------------------------------------
// V17 — citation coherence. Encodes the W2.6 phantom-pointer incident: cited
// oracle symbols nobody defined, contract-cited test files that never
// existed, and the MEM.HPS.BRIDGE alias (a real file about something else).
// ---------------------------------------------------------------------------

const V17_REFERENCE = 'namespace zref {\nclass CmdDecoder {\n public: void step();\n};\nuint32_t crc32c(const uint8_t*);\n}\n';

function v17Block(over: Partial<Block> = {}): Block {
  return rtlBlock({
    maturity: 'REFERENCE_COMPLETE',
    maturity_log: [
      { state: 'REFERENCE_COMPLETE', date: '2026-08-16', commit: 'abc1234', evidence: 'tests/command/cmd_decoder_directed.cpp' },
    ],
    ...over,
  });
}

function v17opts(over: Partial<Parameters<typeof checkCitations>[1]> = {}) {
  return {
    exists,
    referenceText: V17_REFERENCE,
    readText: (p: string): string | null => {
      if (p === 'design/contracts/CMD.DECODER.md') {
        return '# Contract\n## Scalar reference function\n\n`zref::CmdDecoder` — decode oracle.\n\n## Directed tests\n\n`tests/command/cmd_decoder_directed.cpp`\n';
      }
      if (p.startsWith('tests/command/cmd_decoder_')) return '#include "zref.hpp"\n// differential vs zref::CmdDecoder\n';
      return null;
    },
    ...over,
  };
}

test('V17: a defined symbol, coherent contract and oracle-naming tests pass', () => {
  const doc = blocksDoc([v17Block()]);
  assert.deepEqual(checkCitations(doc, v17opts()), []);
});

test('V17a: a REFERENCE_COMPLETE block citing an undefined oracle symbol is rejected', () => {
  const doc = blocksDoc([v17Block({ reference_model: 'zref::CmdDma' })]);
  const errors = checkCitations(doc, v17opts());
  assert.ok(errors.some((e) => /V17.*phantom citation.*zref::CmdDma/s.test(e) || (e.includes('V17') && e.includes('CmdDma'))), errors.join('\n'));
});

test('V17a: a SPECIFIED block may cite a not-yet-written oracle', () => {
  const doc = blocksDoc([rtlBlock({ reference_model: 'zref::CmdDma' })]);
  // contract not written yet either (readText null) — nothing to drift against
  assert.deepEqual(checkCitations(doc, v17opts({ readText: () => null })), []);
});

test('V17b: contract scalar-reference drift from the ledger is rejected at ANY maturity', () => {
  const doc = blocksDoc([rtlBlock({ reference_model: 'zref::CmdDecoderV2' })]);
  const errors = checkCitations(doc, v17opts());
  assert.ok(errors.some((e) => e.includes('V17') && e.includes('drifted')), errors.join('\n'));
});

test('V17c: a contract-cited tests/ path that does not exist is rejected past SPECIFIED', () => {
  const doc = blocksDoc([v17Block()]);
  const errors = checkCitations(
    doc,
    v17opts({
      readText: (p: string): string | null => {
        if (p === 'design/contracts/CMD.DECODER.md') {
          return '## Scalar reference function\n\n`zref::CmdDecoder`\n\n## Randomized differential tests\n\ntests/command/cmd_decoder_phantom_soak.cpp\n';
        }
        if (p.startsWith('tests/command/cmd_decoder_')) return 'zref::CmdDecoder differential';
        return null;
      },
      exists: (p: string) => exists(p) && !p.includes('phantom'),
    })
  );
  assert.ok(errors.some((e) => e.includes('V17') && e.includes('phantom_soak')), errors.join('\n'));
});

test('V17c: the same phantom contract citation is LEGAL while the block is SPECIFIED', () => {
  const doc = blocksDoc([rtlBlock()]);
  const errors = checkCitations(
    doc,
    v17opts({
      readText: (p: string): string | null =>
        p === 'design/contracts/CMD.DECODER.md'
          ? '## Scalar reference function\n\n`zref::CmdDecoder`\n\ntests/command/cmd_decoder_phantom_soak.cpp\n'
          : null,
      exists: (p: string) => exists(p) && !p.includes('phantom'),
    })
  );
  assert.deepEqual(errors, []);
});

test('V17d: an existing test file that never names its oracle is an alias, rejected', () => {
  const doc = blocksDoc([v17Block()]);
  const errors = checkCitations(
    doc,
    v17opts({
      readText: (p: string): string | null => {
        if (p === 'design/contracts/CMD.DECODER.md') return '## Scalar reference function\n\n`zref::CmdDecoder`\n';
        if (p === 'tests/command/cmd_decoder_random.cpp') return '// random soak of something else entirely\n';
        if (p.startsWith('tests/command/cmd_decoder_')) return 'zref::CmdDecoder';
        return null;
      },
    })
  );
  assert.ok(errors.some((e) => e.includes('V17') && e.includes('alias') && e.includes('cmd_decoder_random.cpp')), errors.join('\n'));
});

// ---------------------------------------------------------------------------
// V20 — prose invariant claims must carry a machine-resolvable ENFORCED-BY.
// Encodes the two claims that turned out FALSE ("validated upstream",
// "toggle-free by construction") as the shapes the lint must catch.
// ---------------------------------------------------------------------------

function rtl(text: string, p = 'fpga/rtl/x/zhao_x.sv'): RtlFile[] {
  return [{ path: p, text }];
}
const v20opts = {
  exists: (p: string) => p.startsWith('tests/') || p.startsWith('fpga/') || p.startsWith('spec/'),
  readText: (p: string) => (p === 'fpga/rtl/video/zhao_video_scanout.sv' ? 'swap_ack <= swap_req;' : ''),
};
const v20formal = (): FormalRunsDoc => formalDoc();

test('V20: an annotated claim with a resolvable path passes', () => {
  const errors = checkProseClaims(
    rtl('// gaps impossible by construction\n// ENFORCED-BY: tests/formal/cmd_decoder_safe.sby\n'),
    v20opts,
    v20formal()
  );
  assert.deepEqual(errors, []);
});

test('V20: a claim with no ENFORCED-BY in the window is rejected', () => {
  const errors = checkProseClaims(rtl('// this is validated upstream, trust us\n'), v20opts, v20formal());
  assert.ok(errors.some((e) => e.startsWith('V20:') && e.includes(':1 ')), errors.join('\n'));
});

test('V20: hyphenated and line-wrapped claim phrasings are caught', () => {
  const hyphen = checkProseClaims(rtl('// stable-by-construction, no gray coding\n'), v20opts, v20formal());
  assert.equal(hyphen.length, 1, hyphen.join('\n'));
  const wrapped = checkProseClaims(
    rtl('//     swap at the tick (the copy is stable for a full frame by\n//     construction — contract law).\n'),
    v20opts,
    v20formal()
  );
  assert.equal(wrapped.length, 1, wrapped.join('\n'));
});

test('V20: an ENFORCED-BY pointing at a nonexistent path is rejected', () => {
  const errors = checkProseClaims(
    rtl('// cannot happen by construction\n// ENFORCED-BY: tests/ghost/nothing.cpp\n'),
    { ...v20opts, exists: () => false },
    v20formal()
  );
  assert.ok(errors.some((e) => /V20.*does not exist/.test(e)), errors.join('\n'));
});

test('V20: an ENFORCED-BY naming an UNREGISTERED .sby is rejected (V16 transitivity)', () => {
  const errors = checkProseClaims(
    rtl('// never occurs, by construction\n// ENFORCED-BY: tests/formal/never_ran_thing.sby\n'),
    v20opts,
    v20formal()
  );
  assert.ok(errors.some((e) => /V20.*not registered in design\/formal_runs\.yml/.test(e)), errors.join('\n'));
});

test('V20: a file:symbol annotation resolves the symbol in the named file', () => {
  const good = checkProseClaims(
    rtl('// acked one cycle later by construction\n// ENFORCED-BY: fpga/rtl/video/zhao_video_scanout.sv:swap_ack\n'),
    v20opts,
    v20formal()
  );
  assert.deepEqual(good, []);
  const bad = checkProseClaims(
    rtl('// acked one cycle later by construction\n// ENFORCED-BY: fpga/rtl/video/zhao_video_scanout.sv:no_such_signal\n'),
    v20opts,
    v20formal()
  );
  assert.ok(bad.some((e) => /V20.*symbol "no_such_signal" not found/.test(e)), bad.join('\n'));
});

test('V20: prose without claim phrases is not flagged (no false alarms on ordinary comments)', () => {
  const errors = checkProseClaims(
    rtl('// strict-priority guaranteed client round-robin\n// the offer is registered and held stable\n'),
    v20opts,
    v20formal()
  );
  assert.deepEqual(errors, []);
});

test('V16: a SPECIFIED block may cite a formal property that does not exist yet', () => {
  const { blocks, ops } = baseline();
  blocks.blocks[1] = rtlBlock({
    tests: {
      directed: 'tests/command/cmd_decoder_directed.cpp',
      random: 'tests/command/cmd_decoder_random.cpp',
      formal: 'planned/not_yet.sby',
    },
  });
  const errors = checkAll(blocks, ops, { exists }, formalDoc());
  assert.deepEqual(errors, []);
});

// V21 — a `profile` block is a CONFIGURATION of another block, not hardware.
//
// It is exempt from V4 (its own reference model, its own directed and random
// tests, its own counters) and from V5 (its own ALM budget), because the engine
// it names carries both. Those exemptions are the whole risk: without
// `implemented_by` a profile would just be an RTL block with its obligations
// switched off, which is the "make the rule quiet by rewriting its input" shape
// this project treats as a defect. These tests hold the exemptions to their
// price.
//
// The fixture CONVERTS one of the five FIELD.SEQ entries rather than adding a
// sixth, because that is exactly the change the owner ruling made in
// design/blocks.yml.

function asProfile(over: Partial<Block> = {}): Block {
  return {
    id: 'FIELD.SEQ.WARP',
    name: 'Warp8 field sequencer',
    kind: 'profile',
    subsystem: 'field',
    clock_domain: 'gpu',
    purpose: 'the W profile of ops.yml, run on the shared sequencer',
    contract: 'design/contracts/FIELD.SEQ.WARP.md',
    phase: 9,
    owner_issue: 'ZH-045',
    inputs: ['cached_program'],
    outputs: ['displaced_vertices'],
    upstream: [],
    downstream: [],
    backpressure: 'ready_valid',
    latency: 'variable',
    target_throughput: '1 Field IR instruction per clock',
    counters: ['commands'],
    source_ids: true,
    maturity: 'SPECIFIED',
    maturity_log: [],
    deferred: false,
    superseded_by: null,
    leaf: true,
    implemented_by: 'CMD.DECODER', // an rtl block in the baseline fixture
    ...over,
  };
}

/** Replace the baseline's FIELD.SEQ.WARP with the given block. */
function swapWarp(blocks: BlocksDoc, b: Block): void {
  const i = blocks.blocks.findIndex((x) => x.id === 'FIELD.SEQ.WARP');
  assert.ok(i >= 0, 'fixture should contain FIELD.SEQ.WARP');
  blocks.blocks[i] = b;
}

test('V21: a profile naming a real rtl block passes, and owes no tests of its own', () => {
  const { blocks, ops } = baseline();
  swapWarp(blocks, asProfile());
  const errors = checkAll(blocks, ops, { exists });
  assert.deepEqual(errors, []);
});

test('V21: a profile with no implemented_by is rejected', () => {
  const { blocks, ops } = baseline();
  swapWarp(blocks, asProfile({ implemented_by: null }));
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(
    errors.some((e) => e.startsWith('V21:') && e.includes('names no implemented_by')),
    errors.join(' | ')
  );
});

test('V21: a profile pointing at a block that does not exist is rejected', () => {
  const { blocks, ops } = baseline();
  swapWarp(blocks, asProfile({ implemented_by: 'NO.SUCH.BLOCK' }));
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(
    errors.some((e) => e.startsWith('V21:') && e.includes('unknown block NO.SUCH.BLOCK')),
    errors.join(' | ')
  );
});

test('V21: a profile implemented by another profile is rejected', () => {
  const { blocks, ops } = baseline();
  swapWarp(blocks, asProfile());
  const i = blocks.blocks.findIndex((x) => x.id === 'FIELD.SEQ.FLOW');
  blocks.blocks[i] = asProfile({ id: 'FIELD.SEQ.FLOW', implemented_by: 'FIELD.SEQ.WARP' });
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(
    errors.some((e) => e.startsWith('V21:') && e.includes('must be implemented by an rtl block')),
    errors.join(' | ')
  );
});

test('V21: a profile may not out-claim the engine it configures', () => {
  const { blocks, ops } = baseline();
  // the engine (CMD.DECODER in this fixture) is SPECIFIED; the profile claims more
  swapWarp(blocks, asProfile({ maturity: 'RTL_VERIFIED' }));
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(
    errors.some((e) => e.startsWith('V21:') && e.includes('out-claims its engine')),
    errors.join(' | ')
  );
});

test('V21: a profile may not book an ALM budget — the area belongs to the engine', () => {
  const { blocks, ops } = baseline();
  swapWarp(blocks, asProfile({ resource_budget: { alm_percent: 3 } }));
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(
    errors.some((e) => e.startsWith('V21:') && e.includes('books an ALM budget')),
    errors.join(' | ')
  );
});

// V22 — a `formal_timing` block declares a property, never a fiction.
//
// V4 lets such a block skip reference_model and the directed/random pair,
// because it has no function from inputs to outputs for a differential to
// compare. That exemption is only honest if the block cannot ALSO keep the
// scalar-reference fields lying around: SYS.PLL declared `zref::SysPll` and two
// `tests/platform/sys_pll_*.cpp` paths, none of which has ever existed, purely
// so V4 would pass. These tests hold the exemption to that price.

function formalTimingBlock(over: Partial<Block> = {}): Block {
  return {
    id: 'SYS.PLL',
    name: 'clock tree',
    kind: 'rtl',
    evidence_kind: 'formal_timing',
    subsystem: 'platform',
    clock_domain: 'gpu',
    purpose: 'instantiate the PLL wrappers producing the clock tree',
    contract: 'design/contracts/SYS.PLL.md',
    phase: 0,
    owner_issue: 'ZH-001',
    inputs: ['refclk'],
    outputs: ['clocks'],
    upstream: [],
    downstream: [],
    backpressure: 'none',
    latency: 'variable',
    target_throughput: 'n/a',
    tests: { formal: 'tests/formal/sys_pll_lock.sby' },
    counters: ['commands'],
    source_ids: true,
    maturity: 'SPECIFIED',
    maturity_log: [],
    deferred: false,
    superseded_by: null,
    leaf: true,
    ...over,
  };
}

test('V22: a formal_timing block with a property and no fiction passes', () => {
  const { blocks, ops } = baseline();
  blocks.blocks.push(formalTimingBlock());
  const errors = checkAll(blocks, ops, { exists });
  assert.deepEqual(errors, []);
});

test('V22: a formal_timing block may not keep a reference_model', () => {
  const { blocks, ops } = baseline();
  blocks.blocks.push(formalTimingBlock({ reference_model: 'zref::SysPll' }));
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(
    errors.some((e) => e.startsWith('V22:') && e.includes('may not name one')),
    errors.join(' | ')
  );
});

test('V22: a formal_timing block may not declare a differential', () => {
  const { blocks, ops } = baseline();
  blocks.blocks.push(
    formalTimingBlock({
      tests: { formal: 'tests/formal/sys_pll_lock.sby', directed: 'tests/platform/sys_pll_directed.cpp' },
    })
  );
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(
    errors.some((e) => e.startsWith('V22:') && e.includes('no function to differentiate')),
    errors.join(' | ')
  );
});

test('V22: a formal_timing block must name a formal property', () => {
  const { blocks, ops } = baseline();
  blocks.blocks.push(formalTimingBlock({ tests: {} }));
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(
    errors.some((e) => e.startsWith('V22:') && e.includes('names no tests.formal')),
    errors.join(' | ')
  );
});

test('V22: past SPECIFIED, the formal property must exist on disk', () => {
  const { blocks, ops } = baseline();
  // `exists` in these fixtures accepts tests/ and design/ paths, so point it elsewhere
  blocks.blocks.push(
    formalTimingBlock({
      maturity: 'REFERENCE_COMPLETE',
      maturity_log: [
        { state: 'REFERENCE_COMPLETE', date: '2026-08-22', commit: 'abc1234', evidence: 'design/contracts/SYS.PLL.md' },
      ],
      tests: { formal: 'nowhere/sys_pll_lock.sby' },
    })
  );
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(
    errors.some((e) => e.startsWith('V22:') && e.includes('does not exist on disk')),
    errors.join(' | ')
  );
});

test('V22: an ordinary rtl block still owes its reference and differential', () => {
  const { blocks, ops } = baseline();
  // the exemption must not leak to blocks that did not claim it
  blocks.blocks[1] = rtlBlock({ reference_model: undefined });
  const errors = checkAll(blocks, ops, { exists });
  assert.ok(
    errors.some((e) => e.startsWith('V4:') && e.includes('no reference_model')),
    errors.join(' | ')
  );
});
