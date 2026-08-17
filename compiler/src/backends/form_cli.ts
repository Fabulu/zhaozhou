// form_cli.ts — committed W3.3 artifact generator and byte-staleness gate.

import {
  existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync,
} from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { compileFrontend } from '../frontend/index.js';
import { lowerHir, serializeHir } from '../hir/index.js';
import { lowerZir } from '../zir/index.js';
import { emitCpp } from './cpp/index.js';
import { emitCostReport } from './cost_report.js';
import { emitSourceMap } from './source_map.js';

const compilerRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const fixtureRoot = path.join(compilerRoot, 'tests', 'form', 'fixture');
const goldenRoot = path.join(compilerRoot, 'tests', 'form', 'golden');
const encoder = new TextEncoder();

function expectedArtifacts(): Map<string, Uint8Array> {
  const sourceFiles = ['a_arena.form', 'b_audit.form'];
  const sources = Object.fromEntries(sourceFiles.map((file) => [file, readFileSync(path.join(fixtureRoot, file), 'utf8')]));
  const frontend = compileFrontend(sources);
  if (!frontend.ok) {
    throw new Error(frontend.diagnostics.map((item) => `${item.code} ${item.span.file}:${item.span.start}-${item.span.end}: ${item.message}`).join('\n'));
  }
  const hir = lowerHir(frontend);
  if (!hir) throw new Error('Form frontend admitted fixture but HIR lowering refused it');
  const zir = lowerZir(hir);
  const files = new Map<string, Uint8Array>();
  files.set('hir.json', encoder.encode(serializeHir(hir)));
  files.set('sim.zir.json', encoder.encode(serializeHir(zir.sim)));
  files.set('present.zir.json', encoder.encode(serializeHir(zir.present)));
  files.set('test.zir.json', encoder.encode(serializeHir(zir.test)));
  files.set('sourceids.zmap', emitSourceMap(hir));
  files.set('costs.zcost', emitCostReport(hir, zir, {
    abiVersion: 2,
    commandMemoryCeilingBytes: 1_048_576,
    fieldPrograms: [],
    budgets: [{ line: 'frame_slot_bytes', limit: 1_048_576, owner: 'spec/commands.zidl' }],
  }));

  // Until W3.4 supplies validated physical FieldProgram metadata, declared
  // fields remain explicit in costs.zcost.unlinked_programs without invented
  // instruction, cycle, table, DSP, or register numbers.
  for (const file of emitCpp(hir, zir).files) files.set(file.path, encoder.encode(file.content));
  return files;
}

function diskFiles(root: string, prefix = ''): string[] {
  if (!existsSync(root)) return [];
  const result: string[] = [];
  for (const item of readdirSync(root, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name, 'en'))) {
    const relative = prefix ? `${prefix}/${item.name}` : item.name;
    if (item.isDirectory()) result.push(...diskFiles(path.join(root, item.name), relative));
    else if (item.isFile()) result.push(relative);
  }
  return result;
}

function writeArtifacts(expected: Map<string, Uint8Array>): void {
  mkdirSync(goldenRoot, { recursive: true });
  const expectedNames = new Set(expected.keys());
  for (const stale of diskFiles(goldenRoot)) {
    if (!expectedNames.has(stale)) rmSync(path.join(goldenRoot, ...stale.split('/')));
  }
  for (const [relative, bytes] of expected) {
    const target = path.join(goldenRoot, ...relative.split('/'));
    mkdirSync(path.dirname(target), { recursive: true });
    writeFileSync(target, bytes);
  }
  process.stdout.write(`form:gen wrote ${expected.size} files under ${goldenRoot}\n`);
}

function checkArtifacts(expected: Map<string, Uint8Array>): void {
  const failures: string[] = [];
  const actualNames = diskFiles(goldenRoot);
  const expectedNames = [...expected.keys()].sort();
  for (const missing of expectedNames.filter((name) => !actualNames.includes(name))) failures.push(`missing ${missing}`);
  for (const extra of actualNames.filter((name) => !expected.has(name))) failures.push(`unexpected ${extra}`);
  for (const [relative, bytes] of expected) {
    const target = path.join(goldenRoot, ...relative.split('/'));
    if (!existsSync(target)) continue;
    const actual = readFileSync(target);
    if (!actual.equals(bytes)) failures.push(`stale ${relative}`);
  }
  if (failures.length > 0) throw new Error(`form:check failed:\n${failures.map((item) => `  ${item}`).join('\n')}\nRun npm run form:gen.`);
  process.stdout.write(`form:check verified ${expected.size} byte-stable files\n`);
}

const mode = process.argv[2];
if (mode !== '--write' && mode !== '--check') throw new Error('usage: form_cli --write | --check');
const expected = expectedArtifacts();
if (mode === '--write') writeArtifacts(expected);
else checkArtifacts(expected);
