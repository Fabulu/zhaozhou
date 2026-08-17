// zcap_build.ts — generator-internal .zcap builder (golden PRODUCER,
// capture_format.md 4). The C++ (zref_frame) and consumer-TS (zcap.ts)
// writers are the TESTEES and must produce byte-identical files.

import { crc32c } from './crc32c.js';

export const ZCAP_MAGIC = 0x5041435a; // 'Z','C','A','P' little-endian u32
export const ZCAP_FORMAT_VERSION = 1;
export const ZCAP_HEADER_BYTES = 32;
export const ZCAP_SECTION_ENTRY_BYTES = 32;
export const ZCAP_FLAG_LITTLE_ENDIAN = 0x0001;
export const ZCAP_SECTION_CRC_PRESENT = 0x0001;

export const ZCAP_SECTION = {
  ABI_INFO: 0x0001,
  FRAME_PACKET: 0x0002,
  RESOURCE_PAGES: 0x0003,
  CONTROLLER_SNAPSHOT: 0x0004,
  FRAMEBUFFER_EXPECTED: 0x0005,
  TILE_CRC: 0x0006,
  DEPTH_STENCIL_CRC: 0x0007,
  COUNTERS: 0x0008,
  SOURCE_MAP: 0x0009,
  TRACE: 0x000a,
} as const;

export interface ZcapSectionInput {
  readonly type: number;
  readonly version: number;
  readonly body: Uint8Array;
  readonly crcPresent?: boolean; // default true
}

export function buildZcap(sections: readonly ZcapSectionInput[]): Uint8Array {
  if (sections.length === 0) throw new Error('.zcap needs at least one section');
  const tableBytes = sections.length * ZCAP_SECTION_ENTRY_BYTES;
  let bodyTotal = 0;
  for (const s of sections) bodyTotal += s.body.length;
  const total = ZCAP_HEADER_BYTES + tableBytes + bodyTotal;

  const out = new Uint8Array(total);
  const dv = new DataView(out.buffer);

  // header with placeholders (section_count/total_file_length backpatched below)
  dv.setUint32(0, ZCAP_MAGIC, true);
  dv.setUint16(4, ZCAP_FORMAT_VERSION, true);
  dv.setUint16(6, ZCAP_FLAG_LITTLE_ENDIAN, true);
  dv.setUint32(12, sections.length, true);
  dv.setUint32(16, ZCAP_HEADER_BYTES, true); // section_table_offset
  dv.setUint32(20, ZCAP_SECTION_ENTRY_BYTES, true); // section_entry_size
  dv.setBigUint64(24, BigInt(total), true); // total_file_length

  // section table, then bodies in order
  let tableOff = ZCAP_HEADER_BYTES;
  let bodyOff = ZCAP_HEADER_BYTES + tableBytes;
  for (const s of sections) {
    const crcPresent = s.crcPresent ?? true;
    out.set(s.body, bodyOff);
    const e = tableOff;
    dv.setUint16(e + 0, s.type, true);
    dv.setUint16(e + 2, s.version, true);
    dv.setUint16(e + 4, crcPresent ? ZCAP_SECTION_CRC_PRESENT : 0, true);
    dv.setUint16(e + 6, 0, true); // reserved
    dv.setBigUint64(e + 8, BigInt(bodyOff), true);
    dv.setBigUint64(e + 16, BigInt(s.body.length), true);
    dv.setUint32(e + 24, crcPresent ? crc32c(0, s.body) : 0, true);
    dv.setUint32(e + 28, 0, true); // reserved
    tableOff += ZCAP_SECTION_ENTRY_BYTES;
    bodyOff += s.body.length;
  }

  // header CRC last (over bytes [0,8) — the pre-CRC header prefix)
  dv.setUint32(8, crc32c(0, out, 0, 8), true);
  return out;
}

/** ABI_INFO section body (capture_format.md 4.2). */
export function buildAbiInfo(
  abiVersion: number,
  zcapSchemaVersion: number,
  generatorName: string,
  generatorSha256: Uint8Array,
  zidlSha256: Uint8Array,
): Uint8Array {
  if (generatorSha256.length !== 32 || zidlSha256.length !== 32) {
    throw new Error('sha256 fields must be 32 bytes');
  }
  const name = new Uint8Array(16);
  for (let i = 0; i < Math.min(16, generatorName.length); i++) {
    if (generatorName.charCodeAt(i) >= 0x80) throw new Error('generator name must be ASCII');
    name[i] = generatorName.charCodeAt(i);
  }
  const out = new Uint8Array(4 + 4 + 16 + 32 + 32);
  const dv = new DataView(out.buffer);
  dv.setUint32(0, abiVersion, true);
  dv.setUint32(4, zcapSchemaVersion, true);
  out.set(name, 8);
  out.set(generatorSha256, 24);
  out.set(zidlSha256, 56);
  return out;
}

export interface SourceMapEntry {
  readonly sourceId: number;
  readonly moduleId: number;
  readonly kind: number;
  readonly flags: number;
  readonly spanBegin: number;
  readonly spanEnd: number;
  readonly name: string;
  readonly file: string;
  readonly programHash: number | null;
}

/** Canonical sourceids.zmap file embedded verbatim as the SOURCE_MAP body. */
export function buildSourceMap(entries: readonly SourceMapEntry[]): Uint8Array {
  const rows = [...entries].sort((a, b) => a.sourceId - b.sourceId);
  const fileCount = rows.reduce((count, row) => Math.max(count, row.moduleId + 1), 0);
  const files = Array.from({ length: fileCount }, (_, moduleId) => {
    const names = new Set(rows.filter((row) => row.moduleId === moduleId).map((row) => row.file));
    if (names.size !== 1) throw new Error(`sourceids.zmap: module ${moduleId} needs exactly one file`);
    return [...names][0]!;
  });
  if (files.length > 0x1000) throw new Error('sourceids.zmap: file count exceeds module range');
  const blob: number[] = [];
  const offsets = new Map<string, number>();
  const put = (text: string): number => {
    const prior = offsets.get(text);
    if (prior !== undefined) return prior;
    const offset = blob.length;
    for (const byte of new TextEncoder().encode(text)) blob.push(byte);
    blob.push(0);
    offsets.set(text, offset);
    return offset;
  };
  const fileOffsets = files.map(put);
  const nameOffsets = rows.map((row) => put(row.name));
  const headerBytes = 32;
  const entryBytes = 24;
  const fileBytes = 8;
  const bodyBytes = rows.length * entryBytes + files.length * fileBytes + blob.length;
  if (rows.length > 0xffff_ffff || blob.length > 0xffff_ffff || bodyBytes > 0xffff_ffff) {
    throw new Error('sourceids.zmap exceeds v1 u32 size limits');
  }
  const out = new Uint8Array(headerBytes + bodyBytes);
  const dv = new DataView(out.buffer);
  dv.setUint32(0, 0x504d535a, true);
  dv.setUint16(4, 1, true);
  const headerFlags = rows.some((row) => row.programHash !== null) ? 1 : 0;
  dv.setUint16(6, headerFlags, true);
  dv.setUint32(8, rows.length, true);
  dv.setUint32(12, files.length, true);
  dv.setUint32(16, blob.length, true);
  dv.setBigUint64(24, 0n, true);
  let offset = headerBytes;
  rows.forEach((row, index) => {
    const moduleId = (row.sourceId >>> 16) & 0xfff;
    if (row.moduleId !== moduleId || row.file !== files[row.moduleId]) {
      throw new Error(`sourceids.zmap: denormalized module/file for 0x${row.sourceId.toString(16)}`);
    }
    if ((row.sourceId >>> 28) !== row.kind) throw new Error('sourceids.zmap: denormalized kind');
    const flags = row.programHash === null ? 0 : 1;
    if (row.flags !== flags) throw new Error('sourceids.zmap: denormalized program-hash flags');
    if (row.spanEnd < row.spanBegin) throw new Error('sourceids.zmap: reversed span');
    dv.setUint32(offset + 0, row.sourceId, true);
    dv.setUint16(offset + 4, row.moduleId, true);
    dv.setUint8(offset + 6, row.kind);
    dv.setUint8(offset + 7, flags);
    dv.setUint32(offset + 8, row.spanBegin, true);
    dv.setUint32(offset + 12, row.spanEnd, true);
    dv.setUint32(offset + 16, nameOffsets[index]!, true);
    dv.setUint32(offset + 20, row.programHash ?? 0, true);
    offset += entryBytes;
  });
  for (const pathOff of fileOffsets) {
    dv.setUint32(offset + 0, pathOff, true);
    dv.setUint32(offset + 4, 0, true);
    offset += fileBytes;
  }
  out.set(blob, offset);
  dv.setUint32(20, crc32c(0, out, headerBytes), true);
  return out;
}
