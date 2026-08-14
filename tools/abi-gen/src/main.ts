// main.ts — abi-gen entry point (`npm run abi:gen` / `npm run abi:check`).
//
// gen:   parse spec/commands.zidl -> semantic pass -> LayoutIR -> emit all
//        five targets + goldens. Deterministic, timestamp-free: identical
//        .zidl => byte-identical outputs (capture_format.md 6).
// check: regenerate everything in memory and compare against the working
//        tree; any drift (stale generated file) exits non-zero (CI gate).

import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { tokenize } from './lexer.js';
import { Parser } from './parser.js';
import { semantic } from './layout.js';
import { sha256Hex } from './sha256.js';
import { emitCpp } from './emit_cpp.js';
import { emitTs } from './emit_ts.js';
import { emitSv } from './emit_sv.js';
import { emitDoc } from './emit_doc.js';
import { emitFuzz } from './emit_fuzz.js';
import { buildCorpus, corpusBinary } from './fuzz.js';
import { buildZcap, buildAbiInfo, buildSourceMap } from './zcap_build.js';
import { buildFrame } from './oracle.js';
import { sampleCommand, sampleRecordBytes } from './sample.js';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');

const ZIDL = path.join(ROOT, 'spec/commands.zidl');
const OUT = {
  cpp: path.join(ROOT, 'runtime/include/zhao_abi.h'),
  ts: path.join(ROOT, 'compiler/src/generated/abi.ts'),
  sv: path.join(ROOT, 'fpga/rtl/generated/zhao_abi_pkg.sv'),
  doc: path.join(ROOT, 'spec/generated/abi.md'),
  fuzz: path.join(ROOT, 'tests/fuzz/abi_corpus_gen.ts'),
  frameTmpl: path.join(ROOT, 'compiler/src/generated/frame.ts'),
  zcapTmpl: path.join(ROOT, 'compiler/src/generated/zcap.ts'),
} as const;

const GOLDEN_DIR = path.join(ROOT, 'tests/abi/golden');

export interface GeneratedFile {
  readonly path: string;
  readonly content: string | Uint8Array;
}

export function generateAll(): readonly GeneratedFile[] {
  const zidlSource = readFileSync(ZIDL, 'utf8');
  const ast = new Parser(tokenize(zidlSource)).parse();
  const ir = semantic(ast);

  const identitySha256 = sha256Hex(new TextEncoder().encode(ir.identityText));
  const zidlSha256 = sha256Hex(new TextEncoder().encode(zidlSource));

  const slotBytes = ir.consts.find((c) => c.name === 'FRAME_SLOT_BYTES')?.value ?? 1048576;
  const corpus = buildCorpus(ir, slotBytes);

  const files: GeneratedFile[] = [
    { path: OUT.cpp, content: emitCpp(ir, identitySha256, zidlSha256) },
    { path: OUT.ts, content: emitTs(ir, identitySha256, zidlSha256) },
    { path: OUT.sv, content: emitSv(ir) },
    { path: OUT.doc, content: emitDoc(ir, identitySha256, zidlSha256) },
    { path: OUT.fuzz, content: emitFuzz(corpus) },
    { path: OUT.frameTmpl, content: templateOutput('frame.ts', ir, identitySha256, zidlSha256) },
    { path: OUT.zcapTmpl, content: templateOutput('zcap.ts', ir, identitySha256, zidlSha256) },
  ];

  // ---- goldens ---------------------------------------------------------------
  for (const c of ir.commands) {
    files.push({
      path: path.join(GOLDEN_DIR, `cmd_${c.snake}.bin`),
      content: sampleRecordBytes(c, sampleCommand(ir, c)),
    });
  }

  // minimal frame: BeginFrame / Nop / EndFrame sample records, sealed
  const byName = new Map(ir.commands.map((c) => [c.name, c] as const));
  const frameCmds = ['BeginFrame', 'Nop', 'EndFrame'].map((n) => {
    const c = byName.get(n)!;
    return sampleRecordBytes(c, sampleCommand(ir, c));
  });
  const frameStream = new Uint8Array(frameCmds.reduce((n, r) => n + r.length, 0));
  let fo = 0;
  for (const r of frameCmds) {
    frameStream.set(r, fo);
    fo += r.length;
  }
  const frameMinimal = buildFrame(ir, {
    abiVersion: ir.abi.version,
    flags: 0,
    frameId: 1,
    sequence: 0,
    resourceEpoch: 1,
    deadline: 0,
    commandCount: frameCmds.length,
    commandBytes: frameStream.length,
  }, frameStream);
  files.push({ path: path.join(GOLDEN_DIR, 'frame_minimal.bin'), content: frameMinimal });

  // minimal .zcap: ABI_INFO + FRAME_PACKET + SOURCE_MAP (self-describing)
  const enc = new TextEncoder();
  const generatorSha = enc.encode(identitySha256).length === 64
    ? Uint8Array.from({ length: 32 }, (_, i) => Number.parseInt(identitySha256.slice(i * 2, i * 2 + 2), 16))
    : new Uint8Array(32);
  const zidlSha = Uint8Array.from({ length: 32 }, (_, i) =>
    Number.parseInt(zidlSha256.slice(i * 2, i * 2 + 2), 16));
  const abiInfo = buildAbiInfo(ir.abi.version, 1, 'zhaozhou-abi-gen', generatorSha, zidlSha);
  const sourceMap = buildSourceMap([
    { sourceId: (5 << 28) | (1 << 16) | 1, moduleId: 1, kind: 5, flags: 0, line: 10, name: 'begin_frame', file: 'demo_form.zf' },
    { sourceId: (5 << 28) | (1 << 16) | 0, moduleId: 1, kind: 5, flags: 0, line: 20, name: 'nop', file: 'demo_form.zf' },
    { sourceId: (5 << 28) | (1 << 16) | 2, moduleId: 1, kind: 5, flags: 0, line: 30, name: 'end_frame', file: 'demo_form.zf' },
  ]);
  const zcapMinimal = buildZcap([
    { type: 0x0001, version: 1, body: abiInfo },
    { type: 0x0002, version: 1, body: frameMinimal },
    { type: 0x0009, version: 1, body: sourceMap },
  ]);
  files.push({ path: path.join(GOLDEN_DIR, 'zcap_minimal.zcap'), content: zcapMinimal });

  files.push({ path: path.join(GOLDEN_DIR, 'abi_corpus.zcorpus'), content: corpusBinary(corpus) });

  return files;
}

/** templates are copied verbatim under a GENERATED banner (single source). */
function templateOutput(
  name: 'frame.ts' | 'zcap.ts',
  ir: { abi: { version: number } },
  identitySha256: string,
  zidlSha256: string,
): string {
  const tmpl = readFileSync(path.join(ROOT, 'tools/abi-gen/templates', name), 'utf8');
  const banner = [
    '// GENERATED FILE - DO NOT EDIT (template: tools/abi-gen/templates/' + name + ')',
    `// Generated by tools/abi-gen from spec/commands.zidl (ABI v${ir.abi.version}).`,
    `// abi_identity_sha256 = ${identitySha256} / zidl_sha256 = ${zidlSha256}`,
    '// This is the consumer-side TS mirror of reference/src/zref_frame.cpp;',
    '// law: spec/capture_format.md. Locked to goldens by compiler tests.',
    '',
  ].join('\n');
  return banner + tmpl;
}

function cmdGen(): void {
  for (const f of generateAll()) {
    mkdirSync(path.dirname(f.path), { recursive: true });
    writeFileSync(f.path, f.content as never);
    const size = typeof f.content === 'string'
      ? Buffer.byteLength(f.content, 'utf8')
      : f.content.byteLength;
    console.log(`wrote ${path.relative(ROOT, f.path)} (${size} B)`);
  }
  console.log('abi:gen done');
}

function cmdCheck(): void {
  const files = generateAll();
  const stale: string[] = [];
  for (const f of files) {
    let actual: Buffer;
    try {
      actual = readFileSync(f.path);
    } catch {
      stale.push(`${path.relative(ROOT, f.path)} (missing)`);
      continue;
    }
    const want = typeof f.content === 'string' ? Buffer.from(f.content, 'utf8') : Buffer.from(f.content);
    if (!actual.equals(want)) stale.push(path.relative(ROOT, f.path));
  }
  if (stale.length > 0) {
    console.error('abi:check FAILED — stale generated files (run `npm run abi:gen`):');
    for (const s of stale) console.error(`  ${s}`);
    process.exitCode = 1;
    return;
  }
  console.log(`abi:check clean (${files.length} outputs match)`);
}

function cmdWriteFuzzCorpus(): void {
  // helper used by the nightly lane to refresh the corpus in place
  const f = generateAll().find((x) => x.path.endsWith('abi_corpus.zcorpus'))!;
  writeFileSync(f.path, f.content as never);
  console.log(`wrote ${path.relative(ROOT, f.path)}`);
}

const mode = process.argv[2] ?? 'gen';
if (mode === 'gen') cmdGen();
else if (mode === 'check') cmdCheck();
else if (mode === 'corpus') cmdWriteFuzzCorpus();
else {
  console.error(`unknown mode '${mode}' (gen | check | corpus)`);
  process.exitCode = 2;
}
