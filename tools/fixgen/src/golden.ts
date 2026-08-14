/**
 * Golden vector binaries + manifest (qformats.md 12).
 *
 * Exact little-endian byte dumps — no JSON floats anywhere. Records per the
 * frozen manifest table; the C++ tests consume these files as the oracle.
 */

import * as crypto from "crypto";
import {
  QFMT_VERSION,
  buildSinTable,
  fxSin,
  fxCos,
  unitMul,
  rcp24,
  buildRcp24Table,
  noise2Hash,
  rcp24SampleInput,
  noise2KatInputs,
} from "./fixp.js";
import { RenderedFile } from "./emit.js";

export interface GoldenFile extends RenderedFile {
  content: string; // unused for bins; kept for interface symmetry
  bytes: Buffer;
  records: number;
  layout: string;
}

function sha256(b: Buffer): string {
  return crypto.createHash("sha256").update(b).digest("hex");
}

/** sin_cos_u16.bin: per angle a ascending: s32 sin_raw, s32 cos_raw. */
export function goldenSinCos(): GoldenFile {
  const sinTab = buildSinTable();
  const buf = Buffer.alloc(65536 * 8);
  for (let a = 0; a < 65536; a++) {
    buf.writeInt32LE(fxSin(sinTab, a), a * 8);
    buf.writeInt32LE(fxCos(sinTab, a), a * 8 + 4);
  }
  return {
    path: "tests/golden/fixp/sin_cos_u16.bin",
    content: "",
    bytes: buf,
    records: 65536,
    layout: "per angle a (ascending): s32 LE fx_sin(a).raw, s32 LE fx_cos(a).raw",
  };
}

/** unit8_mul_u8.bin: out[a*256 + b] = unit_mul(a, b), u8. */
export function goldenUnit8(): GoldenFile {
  const buf = Buffer.alloc(65536);
  for (let a = 0; a < 256; a++) {
    for (let b = 0; b < 256; b++) {
      buf[a * 256 + b] = unitMul(a, b);
    }
  }
  return {
    path: "tests/golden/fixp/unit8_mul_u8.bin",
    content: "",
    bytes: buf,
    records: 65536,
    layout: "out[a*256 + b] = unit_mul(a, b), u8",
  };
}

/** rcp24_sample.bin: per i ascending: u32 d, u32 r = rcp_u24(d).r. 2^20 records. */
export function goldenRcp24(): GoldenFile {
  const t24 = buildRcp24Table();
  const n = 1 << 20;
  const buf = Buffer.alloc(n * 8);
  for (let i = 0; i < n; i++) {
    const d = rcp24SampleInput(i);
    const r = rcp24(t24, d);
    buf.writeUInt32LE(d, i * 8);
    buf.writeUInt32LE(r.r, i * 8 + 4);
  }
  return {
    path: "tests/golden/fixp/rcp24_sample.bin",
    content: "",
    bytes: buf,
    records: n,
    layout: "per i (ascending): u32 LE d = (i*16 + (i & 15)) | 1, u32 LE r = rcp_u24(d).r",
  };
}

/** noise2_kat.bin: per record: u32 x, y, seed, h0, h1. 1024 records. */
export function goldenNoise2(): GoldenFile {
  const buf = Buffer.alloc(1024 * 20);
  for (let i = 0; i < 1024; i++) {
    const { x, y, seed } = noise2KatInputs(i);
    const h0 = noise2Hash(x, y, seed, 0);
    const h1 = noise2Hash(x, y, seed, 1);
    const o = i * 20;
    buf.writeUInt32LE(x, o);
    buf.writeUInt32LE(y, o + 4);
    buf.writeUInt32LE(seed, o + 8);
    buf.writeUInt32LE(h0, o + 12);
    buf.writeUInt32LE(h1, o + 16);
  }
  return {
    path: "tests/golden/fixp/noise2_kat.bin",
    content: "",
    bytes: buf,
    records: 1024,
    layout: "per record: u32 LE x = i*2654435761 mod 2^32, y = i*40503 mod 2^32, seed = i*0x9E3779B1 mod 2^32 + 1, h0, h1",
  };
}

/** manifest.json — integer/string fields only, no floats (qformats.md 12). */
export function renderManifest(goldens: GoldenFile[]): RenderedFile {
  const files = goldens.map((g) => ({
    file: g.path.split("/").pop() as string,
    records: g.records,
    bytes: g.bytes.length,
    layout: g.layout,
    sha256: sha256(g.bytes),
  }));
  const manifest = {
    qfmt_version: QFMT_VERSION,
    generator: "tools/fixgen",
    spec: "spec/qformats.md 12",
    files,
  };
  return {
    path: "tests/golden/fixp/manifest.json",
    content: JSON.stringify(manifest, null, 2) + "\n",
  };
}
