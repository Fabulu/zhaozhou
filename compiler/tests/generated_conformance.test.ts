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
  zhaoSampleDrawSky,
  zhaoSampleDebugFrameBlit,
  zhaoSampleDebugRumble,
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
  zhaoPackDrawSky,
  zhaoPackDebugFrameBlit,
  zhaoPackDebugRumble,
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
  ZH_ABI_BAD_ABI_VERSION,
  ZH_ABI_BAD_HEADER_CRC,
  ZH_ABI_BAD_MAGIC,
  ZH_ABI_BAD_PAYLOAD_CRC,
  ZH_ABI_UNKNOWN_OPCODE,
  ZHAO_FRAME_HEADER_BYTES,
  ZHAO_OFF_HEADER_CRC,
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
  { snake: 'draw_sky', sample: zhaoSampleDrawSky, pack: zhaoPackDrawSky },
  { snake: 'debug_frame_blit', sample: zhaoSampleDebugFrameBlit, pack: zhaoPackDebugFrameBlit },
  { snake: 'debug_rumble', sample: zhaoSampleDebugRumble, pack: zhaoPackDebugRumble },
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
  assert.equal(abi.abiVersion, ZHAO_ABI_VERSION); // ABI v2 since wave 2
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

// m1 (review RUN-20260814-1912): the TS leg's bytesConsumed discipline was
// unpinned — the corpus stores error codes only, and frame.ts defaults to 36
// on the header-level abort paths. Pin the exact consumption per
// capture_format.md 3.2: 36 B on a header-level abort, the whole packet
// (40 + command_bytes) once the payload walk is reached.
test('bytesConsumed: 36 on header-level abort, full packet on record-level errors', () => {
  const good = new Uint8Array(readFileSync(path.join(goldenDir(), 'frame_minimal.bin')));

  const badMagic = Uint8Array.from(good);
  badMagic[0] = badMagic[0]! ^ 0xff;
  const r1 = validateFrame(badMagic, FRAME_SLOT_BYTES);
  assert.equal(r1.error, ZH_ABI_BAD_MAGIC);
  assert.equal(r1.bytesConsumed!, 36, 'magic abort consumes exactly the 36-B header');

  const badVer = Uint8Array.from(good);
  badVer[4] = badVer[4]! ^ 0x01; // abi_version low byte (ZHAO_ABI_VERSION 2 -> 3)
  const r2 = validateFrame(badVer, FRAME_SLOT_BYTES);
  assert.equal(r2.error, ZH_ABI_BAD_ABI_VERSION);
  assert.equal(r2.bytesConsumed!, 36, 'version abort consumes exactly the 36-B header');

  const badHcrc = Uint8Array.from(good);
  badHcrc[ZHAO_OFF_HEADER_CRC] = badHcrc[ZHAO_OFF_HEADER_CRC]! ^ 0x01;
  const r3 = validateFrame(badHcrc, FRAME_SLOT_BYTES);
  assert.equal(r3.error, ZH_ABI_BAD_HEADER_CRC);
  assert.equal(r3.bytesConsumed!, 36, 'header-CRC abort consumes exactly the 36-B header');

  const badPcrc = Uint8Array.from(good);
  badPcrc[ZHAO_FRAME_HEADER_BYTES + 4] = badPcrc[ZHAO_FRAME_HEADER_BYTES + 4]! ^ 0x80; // inside the command stream
  const r4 = validateFrame(badPcrc, FRAME_SLOT_BYTES);
  assert.equal(r4.error, ZH_ABI_BAD_PAYLOAD_CRC);
  assert.equal(r4.bytesConsumed!, good.length,
    'payload verdict consumes 40 + command_bytes (the whole 120-B packet)');

  const badOp = Uint8Array.from(good);
  badOp[ZHAO_FRAME_HEADER_BYTES + 48] = 0x05; // unknown opcode in the EndFrame record
  badOp[ZHAO_FRAME_HEADER_BYTES + 49] = 0x20;
  // re-seal payload CRC so the walk is reached
  const dv = new DataView(badOp.buffer);
  dv.setUint32(good.length - 4, crc32c(0, badOp.subarray(ZHAO_FRAME_HEADER_BYTES, good.length - 4)), true);
  const r5 = validateFrame(badOp, FRAME_SLOT_BYTES);
  assert.equal(r5.error, ZH_ABI_UNKNOWN_OPCODE);
  assert.equal(r5.bytesConsumed, good.length, 'record-walk abort consumes the whole packet');
});
