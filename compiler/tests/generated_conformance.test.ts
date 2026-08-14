// generated_conformance.test.ts — the TS testee of the tri-language
// byte-identity matrix (spec/capture_format.md 6): the generated abi.ts +
// consumer mirrors (frame.ts/zcap.ts) must reproduce every committed golden
// byte-for-byte and must agree with the corpus' oracle-expected error codes.

import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  ZHAO_COMMAND_TABLE,
  FRAME_SLOT_BYTES,
  ZhByteWriter,
  ZH_ABI_OK,
  zhaoSampleBeginFrame,
  zhaoSampleDebugBootstrap,
  zhaoSampleDrawForm,
  zhaoSampleDrawPopulation,
  zhaoSampleDrawProcedural,
  zhaoSampleEmitAudioEvent,
  zhaoSampleEndFrame,
  zhaoSampleNop,
  zhaoSampleSetPresentationContract,
  zhaoSampleSetView,
  zhaoSampleSurfaceStamp,
  zhaoSampleTerrainField,
  zhaoPackBeginFrame,
  zhaoPackDebugBootstrap,
  zhaoPackDrawForm,
  zhaoPackDrawPopulation,
  zhaoPackDrawProcedural,
  zhaoPackEmitAudioEvent,
  zhaoPackEndFrame,
  zhaoPackNop,
  zhaoPackSetPresentationContract,
  zhaoPackSetView,
  zhaoPackSurfaceStamp,
  zhaoPackTerrainField,
  ZHAO_GENERATOR_NAME,
  ZHAO_GENERATOR_SHA256,
  ZHAO_ZIDL_SHA256,
  ZHAO_ZCAP_SCHEMA_VERSION,
  ZHAO_ABI_VERSION,
  crc32c,
} from '../src/generated/abi.js';
import { validateFrame, ZhaoFrameBuilder } from '../src/generated/frame.js';
import {
  buildAbiInfo,
  buildSourceMap,
  buildZcap,
  findSections,
  parseAbiInfo,
  parseSourceMap,
  readZcap,
  sectionBody,
  verifySection,
  ZCAP_SECTION,
} from '../src/generated/zcap.js';
import { repoRoot, goldenDir } from './helpers.js';

// sample/pack pairs per command
interface CmdEntry {
  readonly snake: string;
  readonly sample: () => { hdr: { opcode: number; recordBytes: number } };
  readonly pack: (r: never, w: ZhByteWriter) => void;
}
const SAMPLES: readonly CmdEntry[] = [
  { snake: 'nop', sample: zhaoSampleNop, pack: zhaoPackNop },
  { snake: 'begin_frame', sample: zhaoSampleBeginFrame, pack: zhaoPackBeginFrame },
  { snake: 'end_frame', sample: zhaoSampleEndFrame, pack: zhaoPackEndFrame },
  { snake: 'set_view', sample: zhaoSampleSetView, pack: zhaoPackSetView },
  { snake: 'set_presentation_contract', sample: zhaoSampleSetPresentationContract, pack: zhaoPackSetPresentationContract },
  { snake: 'terrain_field', sample: zhaoSampleTerrainField, pack: zhaoPackTerrainField },
  { snake: 'surface_stamp', sample: zhaoSampleSurfaceStamp, pack: zhaoPackSurfaceStamp },
  { snake: 'draw_form', sample: zhaoSampleDrawForm, pack: zhaoPackDrawForm },
  { snake: 'draw_population', sample: zhaoSampleDrawPopulation, pack: zhaoPackDrawPopulation },
  { snake: 'draw_procedural', sample: zhaoSampleDrawProcedural, pack: zhaoPackDrawProcedural },
  { snake: 'emit_audio_event', sample: zhaoSampleEmitAudioEvent, pack: zhaoPackEmitAudioEvent },
  { snake: 'debug_bootstrap', sample: zhaoSampleDebugBootstrap, pack: zhaoPackDebugBootstrap },
];

test('byte-identity: TS packer reproduces every command golden', () => {
  for (const e of SAMPLES) {
    const w = new ZhByteWriter();
    e.pack(e.sample() as never, w);
    const want = readFileSync(path.join(goldenDir(), `cmd_${e.snake}.bin`));
    assert.deepEqual(Uint8Array.from(w.bytes), new Uint8Array(want), e.snake);
  }
});

test('command table matches generated sample opcodes and sizes', () => {
  for (const e of SAMPLES) {
    const r = e.sample();
    const info = ZHAO_COMMAND_TABLE.find((c) => c.opcode === r.hdr.opcode);
    assert.ok(info, e.snake);
    assert.equal(info.recordBytes, r.hdr.recordBytes, e.snake);
    assert.equal(wireSize(e.snake), r.hdr.recordBytes, e.snake);
  }
});

function wireSize(snake: string): number {
  return readFileSync(path.join(goldenDir(), `cmd_${snake}.bin`)).length;
}

test('byte-identity: TS frame builder reproduces frame_minimal.bin', () => {
  const b = new ZhaoFrameBuilder();
  b.appendRecord(packOf('begin_frame')).appendRecord(packOf('nop')).appendRecord(packOf('end_frame'));
  const pkt = b.seal({ frameId: 1, sequence: 0, resourceEpoch: 1, deadline: 0 });
  const want = readFileSync(path.join(goldenDir(), 'frame_minimal.bin'));
  assert.deepEqual(pkt, new Uint8Array(want));
  const r = validateFrame(pkt, FRAME_SLOT_BYTES);
  assert.equal(r.error, ZH_ABI_OK);
  assert.equal(r.commandsConsumed, 3);
  assert.equal(r.bytesConsumed, pkt.length);
});

function packOf(snake: string): Uint8Array {
  const e = SAMPLES.find((x) => x.snake === snake)!;
  const w = new ZhByteWriter();
  e.pack(e.sample() as never, w);
  return w.toUint8Array();
}

test('byte-identity: TS zcap writer reproduces zcap_minimal.zcap', () => {
  const frame = readFileSync(path.join(goldenDir(), 'frame_minimal.bin'));
  const zcap = buildZcap([
    { type: ZCAP_SECTION.ABI_INFO, version: 1, body: buildAbiInfo(ZHAO_ABI_VERSION, ZHAO_GENERATOR_NAME, Uint8Array.from(ZHAO_GENERATOR_SHA256), Uint8Array.from(ZHAO_ZIDL_SHA256), ZHAO_ZCAP_SCHEMA_VERSION) },
    { type: ZCAP_SECTION.FRAME_PACKET, version: 1, body: new Uint8Array(frame) },
    { type: ZCAP_SECTION.SOURCE_MAP, version: 1, body: buildSourceMap([
      { sourceId: (5 << 28) | (1 << 16) | 1, moduleId: 1, kind: 5, flags: 0, line: 10, name: 'begin_frame', file: 'demo_form.zf' },
      { sourceId: (5 << 28) | (1 << 16) | 0, moduleId: 1, kind: 5, flags: 0, line: 20, name: 'nop', file: 'demo_form.zf' },
      { sourceId: (5 << 28) | (1 << 16) | 2, moduleId: 1, kind: 5, flags: 0, line: 30, name: 'end_frame', file: 'demo_form.zf' },
    ]) },
  ]);
  const want = readFileSync(path.join(goldenDir(), 'zcap_minimal.zcap'));
  assert.deepEqual(zcap, new Uint8Array(want));
});

test('zcap reader: golden parses, verifies, and round-trips source IDs', () => {
  const file = new Uint8Array(readFileSync(path.join(goldenDir(), 'zcap_minimal.zcap')));
  const z = readZcap(file);
  assert.equal(z.error, 'OK');
  assert.equal(z.totalLength, file.length);
  assert.equal(z.sections.length, 3);

  const info = findSections(z, ZCAP_SECTION.ABI_INFO);
  assert.equal(info.length, 1);
  const infoBody = sectionBody(file, info[0]!)!;
  assert.ok(verifySection(file, info[0]!));
  const abi = parseAbiInfo(infoBody);
  assert.equal(abi.abiVersion, 1);
  assert.equal(abi.generatorName, 'zhaozhou-abi-gen');

  const map = findSections(z, ZCAP_SECTION.SOURCE_MAP);
  assert.equal(map.length, 1);
  const entries = parseSourceMap(sectionBody(file, map[0]!)!);
  assert.equal(entries.length, 3);
  const names = entries.map((e) => e.name).sort();
  assert.deepEqual(names, ['begin_frame', 'end_frame', 'nop']);
  for (const e of entries) {
    // §5 scheme round-trip: kind 5, module 1, index = table position
    assert.equal(e.kind, 5);
    assert.equal(e.moduleId, 1);
    assert.equal(e.sourceId >>> 28, 5);
  }
});

test('tri-language parity: TS validator agrees with every corpus expectation', () => {
  const data = readFileSync(path.join(goldenDir(), 'abi_corpus.zcorpus'));
  const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
  const count = dv.getUint32(8, true);
  let off = 12;
  let ran = 0;
  for (let i = 0; i < count; i++) {
    const nameLen = dv.getUint16(off, true);
    off += 2;
    const name = new TextDecoder().decode(data.subarray(off, off + nameLen));
    off += nameLen;
    const pktLen = dv.getUint32(off, true);
    off += 4;
    const pkt = data.subarray(off, off + pktLen);
    off += pktLen;
    const expected = dv.getUint32(off, true);
    off += 4;
    const got = validateFrame(Uint8Array.from(pkt), FRAME_SLOT_BYTES).error;
    assert.equal(got, expected, name);
    ran++;
  }
  assert.ok(ran >= 15);
});

test('crc32c still healthy in the emitted module', () => {
  assert.equal(crc32c(0, new TextEncoder().encode('123456789')), 0xe3069283);
});
