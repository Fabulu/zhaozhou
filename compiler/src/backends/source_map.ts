// source_map.ts — canonical binary sourceids.zmap (capture_format.md §7).

import { crc32c } from '../generated/abi.js';
import type { HirProgram, HirSourceRow } from '../hir/model.js';

export const ZMAP_MAGIC = 0x504d535a;
export const ZMAP_VERSION = 1;
export const ZMAP_HEADER_BYTES = 32;
export const ZMAP_ENTRY_BYTES = 24;
export const ZMAP_FILE_BYTES = 8;
/** Portable v1 admission ceiling: no source map may exceed the console's 128 MiB local memory. */
export const ZMAP_MAX_BYTES = 128 * 1024 * 1024;
const U32_MAX = 0xffff_ffffn;

type StructuralCount = number | bigint;

function structuralCount(value: StructuralCount, label: string): bigint {
  if (typeof value === 'number' && (!Number.isSafeInteger(value) || value < 0)) {
    throw new Error(`sourceids.zmap: invalid ${label}`);
  }
  const count = BigInt(value);
  if (count < 0n || count > U32_MAX) throw new Error('sourceids.zmap exceeds v1 u32 size limits');
  return count;
}

/** Non-allocating v1 layout arithmetic over the three u32 count fields. */
export function sourceMapV1ByteLength(
  entryCount: StructuralCount,
  fileCount: StructuralCount,
  stringBlobBytes: StructuralCount,
): bigint {
  const entries = structuralCount(entryCount, 'entry count');
  const files = structuralCount(fileCount, 'file count');
  const blob = structuralCount(stringBlobBytes, 'string blob size');
  const body = entries * BigInt(ZMAP_ENTRY_BYTES) + files * BigInt(ZMAP_FILE_BYTES) + blob;
  if (body > U32_MAX) throw new Error('sourceids.zmap exceeds v1 u32 size limits');
  return BigInt(ZMAP_HEADER_BYTES) + body;
}

/** Enforce the one global v1 source-map byte law before allocation or embedding. */
export function assertSourceMapV1ByteLength(byteLength: StructuralCount): number {
  const bytes = typeof byteLength === 'bigint' ? byteLength : (() => {
    if (!Number.isSafeInteger(byteLength) || byteLength < 0) {
      throw new Error('sourceids.zmap: invalid byte length');
    }
    return BigInt(byteLength);
  })();
  if (bytes < 0n) throw new Error('sourceids.zmap: invalid byte length');
  if (bytes > BigInt(ZMAP_MAX_BYTES)) {
    throw new Error('sourceids.zmap exceeds v1 128 MiB global byte limit');
  }
  return Number(bytes);
}

export interface DecodedSourceMap {
  entries: HirSourceRow[];
  files: string[];
  flags: number;
}

export function emitSourceMap(hir: HirProgram): Uint8Array {
  const rows = [...hir.sourceIds].sort((a, b) => a.sourceId - b.sourceId);
  const files = [...hir.modules].sort((a, b) => a.index - b.index).map((module) => module.file);
  if (files.length > 0x1000) throw new Error('sourceids.zmap file count exceeds 12-bit module range');
  const fileIndices = new Map(files.map((file, index) => [file, index]));
  const chunks: Uint8Array[] = [];
  const offsets = new Map<string, number>();
  let blobBytes = 0;
  const intern = (text: string): number => {
    const prior = offsets.get(text);
    if (prior !== undefined) return prior;
    const bytes = new TextEncoder().encode(text);
    const offset = blobBytes;
    blobBytes += bytes.length + 1;
    offsets.set(text, offset);
    chunks.push(bytes);
    return offset;
  };
  const fileOffsets = files.map(intern);
  const nameOffsets = rows.map((row) => intern(row.name));
  const totalBytes = assertSourceMapV1ByteLength(
    sourceMapV1ByteLength(rows.length, files.length, blobBytes),
  );
  const out = new Uint8Array(totalBytes);
  const view = new DataView(out.buffer);
  view.setUint32(0, ZMAP_MAGIC, true);
  view.setUint16(4, ZMAP_VERSION, true);
  const flags = rows.some((row) => row.programHash !== null) ? 1 : 0;
  view.setUint16(6, flags, true);
  view.setUint32(8, rows.length, true);
  view.setUint32(12, files.length, true);
  view.setUint32(16, blobBytes, true);
  view.setBigUint64(24, 0n, true);
  let offset = ZMAP_HEADER_BYTES;
  rows.forEach((row, index) => {
    const fileIndex = fileIndices.get(row.file);
    if (fileIndex === undefined) throw new Error(`sourceids.zmap row file '${row.file}' is not a module file`);
    if (fileIndex !== row.module) throw new Error(`sourceids.zmap file/module mismatch for 0x${row.sourceId.toString(16)}`);
    const encodedKind = row.sourceId >>> 28;
    if (encodedKind !== row.kind) throw new Error(`sourceids.zmap kind mismatch for 0x${row.sourceId.toString(16)}`);
    if (((row.sourceId >>> 16) & 0xfff) !== row.module) throw new Error(`sourceids.zmap module mismatch for 0x${row.sourceId.toString(16)}`);
    view.setUint32(offset + 0, row.sourceId, true);
    view.setUint16(offset + 4, fileIndex, true);
    view.setUint8(offset + 6, row.kind);
    view.setUint8(offset + 7, row.programHash === null ? 0 : 1);
    view.setUint32(offset + 8, row.span.start, true);
    view.setUint32(offset + 12, row.span.end, true);
    view.setUint32(offset + 16, nameOffsets[index]!, true);
    view.setUint32(offset + 20, row.programHash ?? 0, true);
    offset += ZMAP_ENTRY_BYTES;
  });
  for (const pathOffset of fileOffsets) {
    view.setUint32(offset + 0, pathOffset, true);
    view.setUint32(offset + 4, 0, true);
    offset += ZMAP_FILE_BYTES;
  }
  for (const chunk of chunks) {
    out.set(chunk, offset);
    offset += chunk.length + 1;
  }
  view.setUint32(20, crc32c(0, out, ZMAP_HEADER_BYTES), true);
  return out;
}

/** Strict decoder used by tests/tools; any integrity/layout fault refuses all. */
export function decodeSourceMap(bytes: Uint8Array): DecodedSourceMap {
  assertSourceMapV1ByteLength(bytes.length);
  if (bytes.length < ZMAP_HEADER_BYTES) throw new Error('sourceids.zmap: truncated header');
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (view.getUint32(0, true) !== ZMAP_MAGIC) throw new Error('sourceids.zmap: bad magic');
  if (view.getUint16(4, true) !== ZMAP_VERSION) throw new Error('sourceids.zmap: unsupported version');
  const flags = view.getUint16(6, true);
  if ((flags & ~1) !== 0) throw new Error('sourceids.zmap: reserved flags');
  if (view.getBigUint64(24, true) !== 0n) throw new Error('sourceids.zmap: nonzero reserved header');
  const entryCount = view.getUint32(8, true);
  const fileCount = view.getUint32(12, true);
  const blobBytes = view.getUint32(16, true);
  let expectedBytes: bigint;
  try {
    expectedBytes = sourceMapV1ByteLength(entryCount, fileCount, blobBytes);
  } catch {
    throw new Error('sourceids.zmap: inconsistent lengths');
  }
  if (expectedBytes !== BigInt(bytes.length)) throw new Error('sourceids.zmap: inconsistent lengths');
  const tableBytes = entryCount * ZMAP_ENTRY_BYTES + fileCount * ZMAP_FILE_BYTES;
  const expectedCrc = view.getUint32(20, true);
  if (crc32c(0, bytes, ZMAP_HEADER_BYTES) !== expectedCrc) throw new Error('sourceids.zmap: body CRC mismatch');
  const blobStart = ZMAP_HEADER_BYTES + tableBytes;
  const readString = (offset: number): string => {
    if (offset < 0 || offset >= blobBytes) throw new Error('sourceids.zmap: string offset outside blob');
    let end = blobStart + offset;
    while (end < bytes.length && bytes[end] !== 0) end++;
    if (end === bytes.length) throw new Error('sourceids.zmap: unterminated string');
    return new TextDecoder('utf-8', { fatal: true }).decode(bytes.subarray(blobStart + offset, end));
  };
  const rawEntries: { sourceId: number; fileIndex: number; kind: number; spanStart: number; spanEnd: number; nameOff: number; hash: number | null }[] = [];
  let offset = ZMAP_HEADER_BYTES;
  let previous = -1;
  for (let i = 0; i < entryCount; i++) {
    const sourceId = view.getUint32(offset + 0, true);
    const fileIndex = view.getUint16(offset + 4, true);
    const kind = view.getUint8(offset + 6);
    const entryFlags = view.getUint8(offset + 7);
    const spanStart = view.getUint32(offset + 8, true);
    const spanEnd = view.getUint32(offset + 12, true);
    const nameOff = view.getUint32(offset + 16, true);
    const hash = view.getUint32(offset + 20, true);
    if (sourceId <= previous) throw new Error('sourceids.zmap: entries not strictly ascending');
    if ((sourceId >>> 28) !== kind) throw new Error('sourceids.zmap: denormalized kind mismatch');
    if (fileIndex >= fileCount) throw new Error('sourceids.zmap: file index outside table');
    if (((sourceId >>> 16) & 0xfff) !== fileIndex) throw new Error('sourceids.zmap: module/file index mismatch');
    if ((entryFlags & ~1) !== 0) throw new Error('sourceids.zmap: reserved entry bits');
    if (spanEnd < spanStart) throw new Error('sourceids.zmap: reversed span');
    if ((entryFlags & 1) === 0 && hash !== 0) throw new Error('sourceids.zmap: hash without flag');
    rawEntries.push({ sourceId, fileIndex, kind, spanStart, spanEnd, nameOff, hash: entryFlags & 1 ? hash : null });
    previous = sourceId;
    offset += ZMAP_ENTRY_BYTES;
  }
  const files: string[] = [];
  if (((flags & 1) !== 0) !== rawEntries.some((entry) => entry.hash !== null)) {
    throw new Error('sourceids.zmap: program-hash header flag mismatch');
  }
  for (let i = 0; i < fileCount; i++) {
    const pathOff = view.getUint32(offset + 0, true);
    if (view.getUint32(offset + 4, true) !== 0) throw new Error('sourceids.zmap: nonzero file reserved word');
    files.push(readString(pathOff));
    offset += ZMAP_FILE_BYTES;
  }
  const entries: HirSourceRow[] = rawEntries.map((row) => ({
    sourceId: row.sourceId,
    kind: row.kind,
    module: (row.sourceId >>> 16) & 0xfff,
    file: files[row.fileIndex]!,
    span: { file: files[row.fileIndex]!, start: row.spanStart, end: row.spanEnd },
    name: readString(row.nameOff),
    programHash: row.hash,
  }));
  return { entries, files, flags };
}
