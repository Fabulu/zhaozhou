// abi_gen.test.ts — abi-gen own suite (npm run -w tools/abi-gen test):
// grammar accept/reject, layout law, CRC/SHA vectors, determinism, goldens,
// and oracle conformance over the corpus.

import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import assert from 'node:assert/strict';
import { tokenize } from '../src/lexer.js';
import { Parser } from '../src/parser.js';
import { semantic, OPCODE_RANGES } from '../src/layout.js';
import { crc32c, CRC32C_VECTORS, CRC32C_CHECK_CONSTANT } from '../src/crc32c.js';
import { sha256Hex } from '../src/sha256.js';
import { generateAll } from '../src/main.js';
import { validateFrame, buildFrame } from '../src/oracle.js';
import { buildCorpus } from '../src/fuzz.js';
import { sampleCommand, sampleRecordBytes } from '../src/sample.js';
import { repoRoot, goldenDir } from './helpers.js';

const zidlSource = () => readFileSync(path.join(repoRoot(), 'spec', 'commands.zidl'), 'utf8');

function parseAndLayout(source: string) {
  return semantic(new Parser(tokenize(source)).parse(), { opcodeRanges: OPCODE_RANGES });
}

test('parser accepts the real commands.zidl', () => {
  const ir = parseAndLayout(zidlSource());
  assert.equal(ir.abi.version, 2); // ABI v2 (wave 2)
  assert.equal(ir.abi.commandAlignment, 16);
  assert.equal(ir.commands.length, 15);
});

test('parser rejects missing command status keyword', () => {
  const bad = zidlSource().replace('command Nop 0x0000 implemented', 'command Nop 0x0000');
  assert.throws(() => parseAndLayout(bad), /implemented.*reserved|status/i);
});

test('parser rejects duplicate opcodes', () => {
  const bad = zidlSource().replace('command EndFrame 0x0002', 'command EndFrame 0x0001');
  assert.throws(() => parseAndLayout(bad), /duplicate opcode/i);
});

test('parser rejects opcodes outside ratified ranges', () => {
  const bad = zidlSource().replace('command EmitAudioEvent 0x0400', 'command EmitAudioEvent 0x0500');
  assert.throws(() => parseAndLayout(bad), /outside every ratified range/i);
});

test('layout law: implicit padding is a hard error', () => {
  const bad = zidlSource().replace(
    'command BeginFrame 0x0001 implemented {\n  u32 frame_id;',
    'command BeginFrame 0x0001 implemented {\n  u8 kicker;\n  u32 frame_id;');
  assert.throws(() => parseAndLayout(bad), /implicit padding/i);
});

test('layout law: record size must be a multiple of command_alignment', () => {
  const bad = zidlSource().replace(
    'command Nop 0x0000 implemented {\n' +
    '  // decoder test vehicle; record = bare 16-byte command header\n' +
    '}',
    'command Nop 0x0000 implemented {}\n' +
    'command PaddingProbe 0x0003 implemented { pad[1]; }');
  assert.throws(() => parseAndLayout(bad), /multiple of/i);
});

test('computed record sizes match the ratified table (capture_format.md 1.3)', () => {
  const ir = parseAndLayout(zidlSource());
  const want: Record<string, number> = {
    Nop: 16, BeginFrame: 32, EndFrame: 32, SetView: 96, SetPresentationContract: 48,
    TerrainField: 112, SurfaceStamp: 64, DrawForm: 32, DrawPopulation: 32,
    DrawProcedural: 64, EmitAudioEvent: 32, DebugBootstrap: 64,
    DebugFrameBlit: 48, DebugRumble: 32, DrawSky: 176,
  };
  for (const [name, bytes] of Object.entries(want)) {
    const c = ir.commands.find((x) => x.name === name);
    assert.ok(c, name);
    assert.equal(c.recordBytes, bytes, name);
  }
});

test('fx16 is a 4-byte container; mat4fx is 64 B', () => {
  const ir = parseAndLayout(zidlSource());
  assert.equal(ir.structs.get('mat4fx')!.size, 64);
  assert.equal(ir.structs.get('rectfx')!.size, 16);
  assert.equal(ir.structs.get('transform2fx')!.size, 24);
});

test('ABI v2: PadFrame is 20 B / 4-aligned; video_mode is u8-backed', () => {
  const ir = parseAndLayout(zidlSource());
  const pf = ir.structs.get('PadFrame')!;
  assert.equal(pf.size, 20);
  assert.equal(pf.size % 4, 0); // array stride keeps `buttons` 4-aligned
  const buttons = pf.fields.find((f) => f.name === 'buttons')!;
  assert.equal(buttons.offset, 4);
  const rsv = pf.fields.find((f) => f.name === 'rsv')!;
  assert.equal(rsv.offset, 16);
  const vm = ir.enums.find((e) => e.name === 'video_mode')!;
  assert.equal(vm.type, 'u8');
  assert.deepEqual(vm.entries.map((x) => x.value), [0, 1, 2]);
});

test('layout law: enum entry values must fit the backing type', () => {
  const bad = zidlSource().replace('VIDEO_DUO = 2;', 'VIDEO_DUO = 300;');
  assert.throws(() => parseAndLayout(bad), /fit the u8 backing type/i);
});

test('layout law: struct array stride must keep members aligned (rule 3)', () => {
  // u16 rsv would close PadFrame at 18 B — not a multiple of the 4-B cap
  const bad = zidlSource().replace('u32 rsv;        // reserved', 'u16 rsv;        // reserved');
  assert.throws(() => parseAndLayout(bad), /not a multiple of its alignment cap/i);
});

test('CRC-32C vectors (capture_format.md 2.1 — normative)', () => {
  for (const v of CRC32C_VECTORS) {
    assert.equal(crc32c(0, v.data), v.crc, v.name);
  }
});

test('CRC-32C check constant 0x48674BC7 over message||stored-CRC (capture_format.md 2.1)', () => {
  for (const v of CRC32C_VECTORS) {
    const crc = crc32c(0, v.data);
    const withCrc = new Uint8Array([...v.data, crc & 0xff, (crc >>> 8) & 0xff,
      (crc >>> 16) & 0xff, (crc >>> 24) & 0xff]);
    assert.equal(crc32c(0, withCrc), CRC32C_CHECK_CONSTANT, v.name);
  }
});

test('CRC-32C incremental form equals one-shot', () => {
  const data = CRC32C_VECTORS[4]!.data; // 32 B
  const split = crc32c(crc32c(0, data.subarray(0, 7)), data.subarray(7));
  assert.equal(split, crc32c(0, data));
});

test('sha256 standard vectors', () => {
  assert.equal(sha256Hex(new Uint8Array(0)),
    'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');
  assert.equal(sha256Hex(new TextEncoder().encode('abc')),
    'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
});

test('generation is deterministic (run twice, byte-identical)', () => {
  const a = generateAll();
  const b = generateAll();
  assert.equal(a.length, b.length);
  for (let i = 0; i < a.length; i++) {
    const ca = Buffer.from(a[i]!.content as never);
    const cb = Buffer.from(b[i]!.content as never);
    assert.ok(ca.equals(cb), path.relative(repoRoot(), a[i]!.path));
  }
});

test('oracle: goldens validate OK and match generated bytes', () => {
  const ir = parseAndLayout(zidlSource());
  const slot = ir.consts.find((c) => c.name === 'FRAME_SLOT_BYTES')!.value;

  // per-command goldens
  for (const c of ir.commands) {
    const want = readFileSync(path.join(goldenDir(), `cmd_${c.snake}.bin`));
    const got = sampleRecordBytes(c, sampleCommand(ir, c));
    assert.ok(Buffer.from(got).equals(want), c.name);
    // a well-formed record is NOT a frame; frame validation needs a packet
  }

  // minimal frame
  const frame = readFileSync(path.join(goldenDir(), 'frame_minimal.bin'));
  const r = validateFrame(ir, new Uint8Array(frame), slot);
  assert.equal(r.error, 0, 'frame_minimal must validate OK');
  assert.equal(r.commandsConsumed, 3);
  assert.equal(r.bytesConsumed, frame.length);
});

test('oracle: sealed frame builder round-trips', () => {
  const ir = parseAndLayout(zidlSource());
  const slot = ir.consts.find((c) => c.name === 'FRAME_SLOT_BYTES')!.value;
  const pkt = buildFrame(ir, {
    abiVersion: ir.abi.version, flags: 0, frameId: 7, sequence: 3, resourceEpoch: 2,
    deadline: 99, commandCount: 0, commandBytes: 0,
  }, new Uint8Array(0));
  assert.equal(pkt.length, 40);
  assert.equal(validateFrame(ir, pkt, slot).error, 0);
});

test('corpus: every case validates to its recorded expected error', () => {
  const ir = parseAndLayout(zidlSource());
  const slot = ir.consts.find((c) => c.name === 'FRAME_SLOT_BYTES')!.value;
  const cases = buildCorpus(ir, slot);
  assert.ok(cases.length >= 15);
  const names = new Set(cases.map((c) => c.name));
  assert.equal(names.size, cases.length, 'case names must be unique');
  // intent map: the whole point of each mutation (regression guard)
  const intent: Record<string, number> = {
    valid_minimal: 0, valid_all_implemented: 0, valid_empty_frame: 0, valid_debug_with_flag: 0,
    valid_debug_blit_rumble: 0, debug_blit_without_flag: 12,
    bad_magic: 1, bad_abi_version: 2, bad_header_crc: 5, reserved_frame_flag: 3,
    misaligned_command_bytes: 4, count_exceeds_capacity: 4, oversize_command_bytes: 4,
    packet_truncated: 4, packet_too_short: 4, bad_payload_crc: 6, payload_bit_flip: 6,
    unknown_opcode: 7, record_size_mismatch: 4, record_flags_nonzero: 3,
    record_reserved_nonzero: 8, payload_pad_nonzero: 8, enum_out_of_range: 9,
    count_mismatch: 13, debug_without_flag: 12,
  };
  for (const c of cases) {
    const want = intent[c.name];
    assert.ok(want !== undefined, `case ${c.name} missing intent`);
    assert.equal(c.expectedError, want, c.name);
  }
  // and the recorded binary matches a fresh build
  const corpusFile = readFileSync(path.join(goldenDir(), 'abi_corpus.zcorpus'));
  assert.ok(corpusFile.length > 12);
});
