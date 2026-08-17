import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';

import {
  assertSourceMapV1ByteLength,
  COST_SCHEMA,
  decodeSourceMap,
  emitCostReport,
  emitSourceMap,
  sourceMapV1ByteLength,
  validateCostReport,
  ZMAP_ENTRY_BYTES,
  ZMAP_FILE_BYTES,
  ZMAP_HEADER_BYTES,
  ZMAP_MAGIC,
  ZMAP_MAX_BYTES,
  ZMAP_VERSION,
} from '../../src/backends/index.js';
import { compileFrontend } from '../../src/frontend/index.js';
import { crc32c } from '../../src/generated/abi.js';
import {
  assertSourceMapV1ByteLength as assertGeneratedSourceMapV1ByteLength,
  buildSourceMap,
  buildZcap,
  parseSourceMap,
  readZcap,
  sourceMapV1ByteLength as generatedSourceMapV1ByteLength,
  ZCAP_HEADER_BYTES,
  ZCAP_SECTION,
  ZCAP_SECTION_ENTRY_BYTES,
  ZMAP_MAX_BYTES as GENERATED_ZMAP_MAX_BYTES,
} from '../../src/generated/zcap.js';
import { declarationsOf, lowerHir } from '../../src/hir/index.js';
import { lowerZir } from '../../src/zir/index.js';
import { repoRoot } from '../helpers.js';

function fixtureSources(reverse = false): Record<string, string> {
  const dir = path.join(repoRoot(), 'compiler', 'tests', 'form', 'fixture');
  const files = ['a_arena.form', 'b_audit.form'];
  if (reverse) files.reverse();
  return Object.fromEntries(files.map((file) => [file, readFileSync(path.join(dir, file), 'utf8')]));
}

function compileFixture(reverse = false) {
  const frontend = compileFrontend(fixtureSources(reverse));
  assert.equal(frontend.ok, true, frontend.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  return { hir, zir: lowerZir(hir) };
}

function physicalField(hir: NonNullable<ReturnType<typeof lowerHir>>) {
  const field = declarationsOf(hir, 'field')[0]!;
  return {
    profile: field.profile,
    sourceId: field.sourceId,
    outputs: [{ name: 'height', type: 'fx' as const, reg: 0, min: 0, max: 0 }],
    cost: {
      instrCount: 4,
      byClass: { ALU: 3, MUL: 1, TABLE: 0, NOISE: 0, SPECIAL: 0 },
      cycles: 5,
      dsp: 1,
      tableBytes: 0,
      regHighWater: 3,
    },
  };
}

function costOptions(hir: NonNullable<ReturnType<typeof lowerHir>>) {
  return {
    abiVersion: 2,
    commandMemoryCeilingBytes: 1_048_576,
    fieldPrograms: [physicalField(hir)],
    budgets: [{ line: 'frame_slot_bytes', limit: 1_048_576, owner: 'spec/commands.zidl' }],
  };
}

test('sourceids.zmap has exact v1 layout, ordering, and body CRC', () => {
  const { hir } = compileFixture();
  const first = emitSourceMap(hir);
  const second = emitSourceMap(hir);
  assert.deepEqual(first, second);

  const view = new DataView(first.buffer, first.byteOffset, first.byteLength);
  assert.equal(view.getUint32(0, true), ZMAP_MAGIC);
  assert.equal(view.getUint16(4, true), ZMAP_VERSION);
  assert.equal(view.getUint16(6, true), 0);
  assert.equal(view.getUint32(8, true), hir.sourceIds.length);
  assert.equal(view.getUint32(12, true), hir.modules.length);
  assert.equal(view.getBigUint64(24, true), 0n);
  assert.equal(view.getUint32(20, true), crc32c(0, first, ZMAP_HEADER_BYTES));
  const blobBytes = view.getUint32(16, true);
  assert.equal(first.length, ZMAP_HEADER_BYTES + hir.sourceIds.length * ZMAP_ENTRY_BYTES + hir.modules.length * ZMAP_FILE_BYTES + blobBytes);

  const decoded = decodeSourceMap(first);
  assert.deepEqual(decoded.files, ['a_arena.form', 'b_audit.form']);
  assert.deepEqual(decoded.entries, [...hir.sourceIds].sort((a, b) => a.sourceId - b.sourceId));
  assert.ok(decoded.entries.every((row, index, rows) => index === 0 || rows[index - 1]!.sourceId < row.sourceId));

  const consumed = parseSourceMap(first);
  assert.deepEqual(consumed.map((row) => ({
    sourceId: row.sourceId,
    kind: row.kind,
    module: row.moduleId,
    file: row.file,
    span: { file: row.file, start: row.spanBegin, end: row.spanEnd },
    name: row.name,
    programHash: row.programHash,
  })), decoded.entries);
});

test('sourceids.zmap applies one exact 128 MiB size law without large allocation', () => {
  assert.equal(ZMAP_MAX_BYTES, 128 * 1024 * 1024);
  assert.equal(GENERATED_ZMAP_MAX_BYTES, ZMAP_MAX_BYTES);
  const maximumBlobBytes = BigInt(ZMAP_MAX_BYTES - ZMAP_HEADER_BYTES);
  const maximumLength = sourceMapV1ByteLength(0n, 0n, maximumBlobBytes);
  assert.equal(maximumLength, BigInt(ZMAP_MAX_BYTES));
  assert.equal(assertSourceMapV1ByteLength(maximumLength), ZMAP_MAX_BYTES);
  assert.equal(
    generatedSourceMapV1ByteLength(0n, 0n, maximumBlobBytes),
    maximumLength,
  );
  assert.equal(assertGeneratedSourceMapV1ByteLength(maximumLength), ZMAP_MAX_BYTES);

  const overLimit = sourceMapV1ByteLength(0n, 0n, maximumBlobBytes + 1n);
  for (const assertLength of [
    assertSourceMapV1ByteLength,
    assertGeneratedSourceMapV1ByteLength,
  ] as const) {
    assert.throws(() => assertLength(overLimit), /128 MiB global byte limit/);
    assert.throws(() => assertLength(-1n), /invalid byte length/);
  }
  for (const byteLength of [sourceMapV1ByteLength, generatedSourceMapV1ByteLength] as const) {
    assert.throws(() => byteLength(0x1_0000_0000n, 0n, 0n), /u32 size limits/);
    assert.throws(() => byteLength(0xffff_ffffn, 1n, 0n), /u32 size limits/);
  }
});

test('SOURCE_MAP capture admission rejects oversized and imprecise u64 layouts', () => {
  const oversizedBody = { length: ZMAP_MAX_BYTES + 1 } as unknown as Uint8Array;
  assert.throws(
    () => buildZcap([{ type: ZCAP_SECTION.SOURCE_MAP, version: 1, body: oversizedBody }]),
    /128 MiB global byte limit/,
  );

  const valid = buildZcap([{
    type: ZCAP_SECTION.SOURCE_MAP,
    version: 1,
    body: buildSourceMap([]),
  }]);
  assert.equal(readZcap(valid).error, 'OK');
  const sectionOffset = ZCAP_HEADER_BYTES;

  const oversizedSource = valid.slice();
  new DataView(oversizedSource.buffer).setBigUint64(
    sectionOffset + 16,
    BigInt(ZMAP_MAX_BYTES) + 1n,
    true,
  );
  assert.equal(readZcap(oversizedSource).error, 'BAD_TABLE');

  const unsafeOffset = valid.slice();
  new DataView(unsafeOffset.buffer).setBigUint64(
    sectionOffset + 8,
    (1n << 53n) + 1n,
    true,
  );
  assert.equal(readZcap(unsafeOffset).error, 'BAD_TABLE');

  const unsafeTotal = valid.slice();
  new DataView(unsafeTotal.buffer).setBigUint64(24, (1n << 53n) + 1n, true);
  assert.equal(readZcap(unsafeTotal).error, 'BAD_TABLE');
  assert.equal(valid.length, ZCAP_HEADER_BYTES + ZCAP_SECTION_ENTRY_BYTES + ZMAP_HEADER_BYTES);
});

test('sourceids.zmap carries all 65536 module rows with a multi-megabyte name table', () => {
  const { hir } = compileFixture();
  const file = hir.modules[0]!.file;
  const rows = Array.from({ length: 0x1_0000 }, (_, index) => ({
    sourceId: 0xa000_0000 + index,
    kind: 10,
    module: 0,
    file,
    span: { file, start: index * 2, end: index * 2 + 1 },
    name: `pool_${index.toString(16).padStart(4, '0')}_${'n'.repeat(52)}`,
    programHash: null,
  }));
  const bytes = emitSourceMap({ ...hir, modules: [hir.modules[0]!], sourceIds: rows });
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  assert.equal(view.getUint32(8, true), 0x1_0000);
  assert.ok(view.getUint32(16, true) > 0xffff, 'string blob must exceed the old u16 offset ceiling');
  assert.ok(view.getUint32(ZMAP_HEADER_BYTES + (rows.length - 1) * ZMAP_ENTRY_BYTES + 16, true) > 0xffff);

  const consumed = parseSourceMap(bytes);
  assert.equal(consumed.length, rows.length);
  assert.equal(consumed[0]!.name, rows[0]!.name);
  assert.equal(consumed.at(-1)!.name, rows.at(-1)!.name);
  assert.equal(consumed.at(-1)!.sourceId, 0xa000_ffff);
});

test('sourceids.zmap consumers refuse malformed boundaries before any invalid read', () => {
  const { hir } = compileFixture();
  const valid = emitSourceMap(hir);
  for (const parse of [decodeSourceMap, parseSourceMap] as const) {
    assert.throws(() => parse(valid.subarray(0, ZMAP_HEADER_BYTES - 1)), /truncated header/);

    const impossibleCount = valid.slice();
    new DataView(impossibleCount.buffer).setUint32(8, 0xffff_ffff, true);
    assert.throws(() => parse(impossibleCount), /inconsistent lengths/);

    const outsideName = valid.slice();
    new DataView(outsideName.buffer).setUint32(ZMAP_HEADER_BYTES + 16, 0xffff_ffff, true);
    new DataView(outsideName.buffer).setUint32(20, crc32c(0, outsideName, ZMAP_HEADER_BYTES), true);
    assert.throws(() => parse(outsideName), /string offset outside blob/);

    const unterminated = valid.slice();
    unterminated[unterminated.length - 1] = 0x41;
    new DataView(unterminated.buffer).setUint32(20, crc32c(0, unterminated, ZMAP_HEADER_BYTES), true);
    assert.throws(() => parse(unterminated), /unterminated string/);
  }
});

test('sourceids.zmap rejects CRC and denormalized metadata corruption', () => {
  const { hir } = compileFixture();
  const valid = emitSourceMap(hir);
  const crcFault = valid.slice();
  crcFault[crcFault.length - 1] = crcFault[crcFault.length - 1]! ^ 0x80;
  assert.throws(() => decodeSourceMap(crcFault), /CRC mismatch/);

  const kindFault = valid.slice();
  kindFault[ZMAP_HEADER_BYTES + 6] = kindFault[ZMAP_HEADER_BYTES + 6]! ^ 1;
  new DataView(kindFault.buffer).setUint32(20, crc32c(0, kindFault, ZMAP_HEADER_BYTES), true);
  assert.throws(() => decodeSourceMap(kindFault), /kind mismatch/);

  const offsetFault = valid.slice();
  new DataView(offsetFault.buffer).setUint32(ZMAP_HEADER_BYTES + 16, 0xffff_ffff, true);
  new DataView(offsetFault.buffer).setUint32(20, crc32c(0, offsetFault, ZMAP_HEADER_BYTES), true);
  assert.throws(() => decodeSourceMap(offsetFault), /string offset outside blob/);
});

test('canonical SOURCE_MAP interoperates with native C++ and rejects one shared malformed corpus', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }

  const { hir } = compileFixture();
  const compilerBytes = emitSourceMap(hir);
  const rows = [...hir.sourceIds].sort((a, b) => a.sourceId - b.sourceId);
  const files = [...hir.modules].sort((a, b) => a.index - b.index).map((module) => module.file);
  const q = (text: string) => JSON.stringify(text);
  const expectedFiles = files.map((file, index) =>
    `if (parsed.map.files[${index}u] != ${q(file)}) return 10;`).join('\n');
  const expectedRows = rows.map((row, index) => {
    const hashPresent = row.programHash !== null;
    return `{
      const auto& entry = parsed.map.entries[${index}u];
      if (entry.source_id != ${row.sourceId}u || entry.module_id != ${row.module}u
          || entry.file_index != ${row.module}u || entry.kind != ${row.kind}u
          || entry.flags != ${hashPresent ? 1 : 0}u || entry.span_begin != ${row.span.start}u
          || entry.span_end != ${row.span.end}u || entry.name != ${q(row.name)}
          || entry.file != ${q(row.file)}
          || entry.program_hash.has_value() != ${hashPresent ? 'true' : 'false'}${hashPresent ? `
          || entry.program_hash.value() != ${row.programHash}u` : ''}) return 11;
    }`;
  }).join('\n');

  const bridgeSource = `
#include "zref/zref_frame.hpp"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
std::vector<uint8_t> unhex(const std::string& text) {
  if ((text.size() & 1u) != 0u) return {};
  std::vector<uint8_t> out;
  out.reserve(text.size() / 2u);
  for (size_t i = 0; i < text.size(); i += 2u) {
    const int high = nibble(text[i]);
    const int low = nibble(text[i + 1u]);
    if (high < 0 || low < 0) return {};
    out.push_back(static_cast<uint8_t>((high << 4) | low));
  }
  return out;
}
void print_hex(const std::vector<uint8_t>& bytes) {
  for (uint8_t byte : bytes) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
  }
  std::cout << '\\n';
}
}  // namespace

int main() {
  using namespace zhao;
  const auto maximum = zhao_source_map_v1_byte_length(
      0u, 0u, ZHAO_ZMAP_MAX_BYTES - ZHAO_ZMAP_HEADER_BYTES);
  const auto over = zhao_source_map_v1_byte_length(
      0u, 0u, ZHAO_ZMAP_MAX_BYTES - ZHAO_ZMAP_HEADER_BYTES + 1u);
  const auto overflow = zhao_source_map_v1_byte_length(
      std::numeric_limits<uint32_t>::max(), 1u, 0u);
  uint8_t sentinel = 0;
  const auto too_large = zhao_zcap_parse_source_map(&sentinel, ZHAO_ZMAP_MAX_BYTES + 1u);
  if (!maximum.ok() || maximum.bytes != ZHAO_ZMAP_MAX_BYTES
      || zhao_source_map_v1_admit_byte_length(maximum.bytes) != ZhaoSourceMapError::kOk
      || !over.ok() || over.bytes != ZHAO_ZMAP_MAX_BYTES + 1u
      || zhao_source_map_v1_admit_byte_length(over.bytes) != ZhaoSourceMapError::kTooLarge
      || overflow.error != ZhaoSourceMapError::kSizeOverflow
      || too_large.error != ZhaoSourceMapError::kTooLarge
      || !too_large.map.entries.empty() || !too_large.map.files.empty()) return 2;

  ZhaoSourceMap invalid_text;
  invalid_text.files.emplace_back("\\xC0", 1u);
  if (zhao_zcap_build_source_map(invalid_text).error != ZhaoSourceMapError::kInvalidUtf8) return 3;
  invalid_text.files[0] = std::string("a\\0b", 3u);
  if (zhao_zcap_build_source_map(invalid_text).error != ZhaoSourceMapError::kInvalidUtf8) return 4;

  std::string line;
  if (!std::getline(std::cin, line)) return 5;
  const auto compiler_map = unhex(line);
  const auto parsed = zhao_zcap_parse_source_map(compiler_map);
  if (!parsed.ok() || parsed.map.flags != ${rows.some((row) => row.programHash !== null) ? 1 : 0}u
      || parsed.map.files.size() != ${files.length}u
      || parsed.map.entries.size() != ${rows.length}u) return 6;
  ${expectedFiles}
  ${expectedRows}

  ZhaoSourceMap native_map;
  native_map.files = {"base.form", "terrain.form"};
  ZhaoSourceMapEntry field;
  field.source_id = zhao_source_id(3u, 1u, 9u);
  field.module_id = 1u;
  field.file_index = 1u;
  field.kind = 3u;
  field.flags = 1u;
  field.span_begin = 7u;
  field.span_end = 11u;
  field.name = "ridge";
  field.file = native_map.files[1];
  field.program_hash = 0xDEADBEEFu;
  native_map.entries.push_back(field);
  ZhaoSourceMapEntry command;
  command.source_id = zhao_source_id(5u, 0u, 2u);
  command.module_id = 0u;
  command.file_index = 0u;
  command.kind = 5u;
  command.span_begin = 20u;
  command.span_end = 23u;
  command.name = "nop";
  command.file = native_map.files[0];
  native_map.entries.push_back(command);
  const auto built = zhao_zcap_build_source_map(native_map);
  if (!built.ok()) return 7;
  print_hex(built.bytes);

  while (std::getline(std::cin, line)) {
    const auto bytes = unhex(line);
    const auto result = zhao_zcap_parse_source_map(bytes);
    if (!result.ok() && (!result.map.entries.empty() || !result.map.files.empty())) return 8;
    std::cout << (result.ok() ? '1' : '0') << '\\n';
  }
  return 0;
}
`;

  const base = buildSourceMap([
    {
      sourceId: 0x3000_0001, moduleId: 0, kind: 3, flags: 1,
      spanBegin: 2, spanEnd: 5, name: 'alpha', file: 'alpha.form', programHash: 0x1234_5678,
    },
    {
      sourceId: 0x3001_0001, moduleId: 1, kind: 3, flags: 0,
      spanBegin: 8, spanEnd: 13, name: 'beta', file: 'beta.form', programHash: null,
    },
  ], ['alpha.form', 'beta.form']);
  const empty = buildSourceMap([]);
  const bodyCrc = (bytes: Uint8Array) => {
    new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).setUint32(
      20, crc32c(0, bytes, ZMAP_HEADER_BYTES), true,
    );
    return bytes;
  };
  const changed = (edit: (bytes: Uint8Array, view: DataView) => void, crc = true) => {
    const bytes = base.slice();
    edit(bytes, new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength));
    return crc ? bodyCrc(bytes) : bytes;
  };
  const fileTable = ZMAP_HEADER_BYTES + 2 * ZMAP_ENTRY_BYTES;
  const blobStart = fileTable + 2 * ZMAP_FILE_BYTES;
  const corpus: Array<{ name: string; bytes: Uint8Array }> = [
    { name: 'compiler emitted valid', bytes: compilerBytes },
    { name: 'generated builder valid', bytes: base },
    { name: 'empty valid', bytes: empty },
    { name: 'truncated header', bytes: base.subarray(0, ZMAP_HEADER_BYTES - 1) },
    { name: 'bad magic', bytes: changed((_bytes, view) => view.setUint32(0, 0, true), false) },
    { name: 'unsupported version', bytes: changed((_bytes, view) => view.setUint16(4, 2, true), false) },
    { name: 'reserved header flags', bytes: changed((_bytes, view) => view.setUint16(6, 2, true), false) },
    { name: 'reserved header word', bytes: changed((_bytes, view) => view.setUint32(24, 1, true), false) },
    { name: 'impossible count', bytes: changed((_bytes, view) => view.setUint32(8, 0xffff_ffff, true), false) },
    { name: 'trailing byte', bytes: Uint8Array.from([...base, 0]) },
    { name: 'body crc', bytes: changed((bytes) => { bytes[bytes.length - 1] = bytes[bytes.length - 1]! ^ 1; }, false) },
    { name: 'duplicate id', bytes: changed((_bytes, view) => view.setUint32(ZMAP_HEADER_BYTES + ZMAP_ENTRY_BYTES, 0x3000_0001, true)) },
    { name: 'kind mismatch', bytes: changed((bytes) => { bytes[ZMAP_HEADER_BYTES + 6] = 4; }) },
    { name: 'file outside table', bytes: changed((_bytes, view) => view.setUint16(ZMAP_HEADER_BYTES + 4, 2, true)) },
    { name: 'module file mismatch', bytes: changed((_bytes, view) => view.setUint16(ZMAP_HEADER_BYTES + 4, 1, true)) },
    { name: 'reserved entry bits', bytes: changed((bytes) => { bytes[ZMAP_HEADER_BYTES + 7] = 3; }) },
    { name: 'reversed span', bytes: changed((_bytes, view) => view.setUint32(ZMAP_HEADER_BYTES + 12, 1, true)) },
    { name: 'hash without flag', bytes: changed((bytes) => { bytes[ZMAP_HEADER_BYTES + 7] = 0; }) },
    { name: 'hash header mismatch', bytes: changed((bytes, view) => {
      bytes[ZMAP_HEADER_BYTES + 7] = 0;
      view.setUint32(ZMAP_HEADER_BYTES + 20, 0, true);
    }) },
    { name: 'file reserved word', bytes: changed((_bytes, view) => view.setUint32(fileTable + 4, 1, true)) },
    { name: 'name offset outside blob', bytes: changed((_bytes, view) => view.setUint32(ZMAP_HEADER_BYTES + 16, 0xffff_ffff, true)) },
    { name: 'unterminated string', bytes: changed((bytes) => { bytes[bytes.length - 1] = 0x41; }) },
    { name: 'invalid utf8', bytes: changed((bytes, view) => {
      const nameOffset = view.getUint32(ZMAP_HEADER_BYTES + 16, true);
      bytes[blobStart + nameOffset] = 0xc0;
    }) },
  ];

  const root = mkdtempSync(path.join(tmpdir(), 'zmap-native-'));
  try {
    const source = path.join(root, 'source_map_bridge.cpp');
    const executable = path.join(root, 'source_map_bridge.exe');
    writeFileSync(source, bridgeSource, 'utf8');
    const repo = repoRoot();
    const build = spawnSync(compiler, [
      '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-static',
      `-I${path.join(repo, 'runtime', 'include')}`,
      `-I${path.join(repo, 'reference', 'include')}`,
      path.join(repo, 'reference', 'src', 'zref_frame.cpp'), source, '-o', executable,
    ], { encoding: 'utf8', windowsHide: true });
    assert.equal(build.status, 0, `${build.stdout}\n${build.stderr}`);

    const input = [compilerBytes, ...corpus.map((item) => item.bytes)]
      .map((bytes) => Buffer.from(bytes).toString('hex')).join('\n');
    const run = spawnSync(executable, [], {
      encoding: 'utf8', input: `${input}\n`, maxBuffer: 16 * 1024 * 1024, windowsHide: true,
    });
    assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
    const lines = run.stdout.trim().split(/\r?\n/);
    const nativeEmitted = Uint8Array.from(Buffer.from(lines[0]!, 'hex'));
    assert.deepEqual(parseSourceMap(nativeEmitted), [
      {
        sourceId: 0x3001_0009, moduleId: 1, fileIndex: 1, kind: 3, flags: 1,
        spanBegin: 7, spanEnd: 11, name: 'ridge', file: 'terrain.form', programHash: 0xdead_beef,
      },
      {
        sourceId: 0x5000_0002, moduleId: 0, fileIndex: 0, kind: 5, flags: 0,
        spanBegin: 20, spanEnd: 23, name: 'nop', file: 'base.form', programHash: null,
      },
    ]);

    const nativeAcceptance = lines.slice(1).map((line) => line === '1');
    const tsAcceptance = corpus.map(({ bytes }) => {
      try {
        parseSourceMap(bytes);
        return true;
      } catch {
        return false;
      }
    });
    assert.equal(nativeAcceptance.length, corpus.length);
    assert.deepEqual(nativeAcceptance, tsAcceptance,
      corpus.map((item, index) => `${item.name}: native=${nativeAcceptance[index]} ts=${tsAcceptance[index]}`).join('\n'));
    assert.deepEqual(tsAcceptance.slice(0, 3), [true, true, true]);
    assert.ok(tsAcceptance.slice(3).every((accepted) => !accepted));
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test('costs.zcost is complete canonical integer JSON and deterministic', () => {
  const first = compileFixture(false);
  const second = compileFixture(true);
  const bytes = emitCostReport(first.hir, first.zir, costOptions(first.hir));
  assert.deepEqual(bytes, emitCostReport(second.hir, second.zir, costOptions(second.hir)));
  const text = new TextDecoder().decode(bytes);
  assert.equal(text.at(-1), '\n');
  assert.equal(text.includes('\r'), false);
  assert.equal(text.slice(0, -1).includes('\n'), false);

  const report = validateCostReport(bytes);
  assert.equal(report.$schema, COST_SCHEMA);
  assert.equal(report.abi_version, 2);
  assert.deepEqual(report.modules, [{ index: 0, name: 'arena' }, { index: 1, name: 'audit' }]);
  assert.deepEqual(report.pools, [{ capacity: 4, element_bytes: 29, module: 0, name: 'particles' }]);
  assert.deepEqual(report.rates, [
    { every: 2, invocation_every: 2, module: 0, name: 'seed_wave', phase: 0, selected_peak: 0, stagger: false },
    { every: 4, invocation_every: 1, module: 0, name: 'advance', phase: 1, selected_peak: 1, stagger: true },
    { every: 1, invocation_every: 1, module: 1, name: 'observe', phase: 0, selected_peak: 0, stagger: false },
  ]);
  assert.equal(report.command_memory.per_frame_estimate_bytes, 208);
  assert.deepEqual(report.command_templates.map((row) => ({
    command: row.command, module: row.module, ordinal: row.ordinal,
    presentation: row.presentation, record_bytes: row.record_bytes,
  })), [
    { command: 'draw_population', module: 0, ordinal: 0, presentation: 'main_view', record_bytes: 32 },
    { command: 'audio', module: 0, ordinal: 1, presentation: 'main_view', record_bytes: 32 },
  ]);
  assert.deepEqual(report.particle_bandwidth, { bytes_per_element: 0, peak_elements: 0, bytes_per_tick: 0, pools: [] });
  assert.deepEqual(report.programs[0]!.class_counts, { ALU: 3, MUL: 1, NOISE: 0, SPECIAL: 0, TABLE: 0 });
  assert.equal(report.programs[0]!.instr_count, 4);
  assert.deepEqual(report.programs[0]!.footprint_rect, declarationsOf(first.hir, 'field')[0]!.footprint.rect.map(Number));
  assert.deepEqual(report.unlinked_programs, []);
});

test('costs.zcost validator refuses noncanonical and nonschema bytes', () => {
  const { hir, zir } = compileFixture();
  const bytes = emitCostReport(hir, zir, costOptions(hir));
  const report = JSON.parse(new TextDecoder().decode(bytes));
  const pretty = new TextEncoder().encode(`${JSON.stringify(report, null, 2)}\n`);
  assert.throws(() => validateCostReport(pretty), /not canonical/);

  report.abi_version = 2.5;
  const fractional = new TextEncoder().encode(`${JSON.stringify(report)}\n`);
  assert.throws(() => validateCostReport(fractional), /unsigned integer/);

  const unlinkedText = new TextDecoder().decode(emitCostReport(hir, zir, { ...costOptions(hir), fieldPrograms: [] }));
  const fabricated = new TextEncoder().encode(unlinkedText.replace(
    '"kind":"field"',
    '"instr_count":1,"kind":"field"',
  ));
  assert.throws(() => validateCostReport(fabricated), /forbidden 'instr_count'/);

  const duplicate = JSON.parse(unlinkedText) as { unlinked_programs: unknown[] };
  duplicate.unlinked_programs.push(duplicate.unlinked_programs[0]);
  assert.throws(
    () => validateCostReport(new TextEncoder().encode(`${JSON.stringify(duplicate)}\n`)),
    /duplicate unlinked program source_id/,
  );
});

test('costs.zcost moves fields from truthful unlinked rows to validated linked programs', () => {
  const { hir, zir } = compileFixture();
  const unlinkedBytes = emitCostReport(hir, zir, { ...costOptions(hir), fieldPrograms: [] });
  const unlinked = validateCostReport(unlinkedBytes);
  assert.deepEqual(unlinked.programs, []);
  assert.equal(unlinked.unlinked_programs.length, 1);
  const field = declarationsOf(hir, 'field')[0]!;
  assert.deepEqual(unlinked.unlinked_programs[0], {
    footprint_rect: field.footprint.rect.map(Number),
    kind: 'field',
    max_ops: field.maxOps,
    module: field.module,
    name: field.name,
    profile: field.profile,
    source_id: field.sourceId,
  });
  for (const fake of ['instr_count', 'cycles_est', 'class_counts', 'dsp', 'table_bytes', 'register_hwm']) {
    assert.equal(fake in unlinked.unlinked_programs[0]!, false, fake);
  }

  const linked = validateCostReport(emitCostReport(hir, zir, costOptions(hir)));
  assert.equal(linked.programs.length, 1);
  assert.deepEqual(linked.unlinked_programs, []);
  assert.equal(linked.programs[0]!.source_id, unlinked.unlinked_programs[0]!.source_id);
});
