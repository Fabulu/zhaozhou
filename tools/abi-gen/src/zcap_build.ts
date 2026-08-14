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
  readonly line: number;
  readonly name: string;
  readonly file: string;
}

/** SOURCE_MAP section body: count + entries + NUL-terminated string blob. */
export function buildSourceMap(entries: readonly SourceMapEntry[]): Uint8Array {
  const enc = new TextEncoder();
  const blob: number[] = [];
  const put = (s: string): number => {
    const off = blob.length;
    for (const b of enc.encode(s)) blob.push(b);
    blob.push(0);
    return off;
  };
  const metas = entries.map((e) => ({ e, nameOff: put(e.name), fileOff: put(e.file) }));
  const body = new Uint8Array(4 + entries.length * 16 + blob.length);
  const dv = new DataView(body.buffer);
  dv.setUint32(0, entries.length, true);
  let off = 4;
  for (const { e, nameOff, fileOff } of metas) {
    dv.setUint32(off + 0, e.sourceId, true);
    dv.setUint16(off + 4, e.moduleId, true);
    dv.setUint8(off + 6, e.kind);
    dv.setUint8(off + 7, e.flags);
    dv.setUint32(off + 8, e.line, true);
    dv.setUint16(off + 12, nameOff, true);
    dv.setUint16(off + 14, fileOff, true);
    off += 16;
  }
  body.set(blob, off);
  return body;
}
