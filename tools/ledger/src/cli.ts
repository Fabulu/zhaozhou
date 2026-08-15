/**
 * tools/ledger CLI (W2).
 *   check          — schema + V1–V14 (incl. git-history maturity law V2)
 *   gen [--verify] — regenerate design/diagrams/architecture.mmd + dashboard.md
 *                    (--verify: compare against committed bytes, exit 1 on drift)
 *   gen-contracts  — scaffold missing design/contracts/<ID>.md stubs
 */
import * as fs from 'node:fs';
import * as path from 'node:path';
import { loadLedger, loadYaml } from './load';
import { checkAll } from './rules';
import type { FormalRunsDoc } from './types';
import { readPrevBlocks } from './git';
import { renderArchitecture } from './gen/architecture';
import { renderDashboard } from './gen/dashboard';
import { genContracts } from './gen/contracts';

const REPO_ROOT = path.resolve(__dirname, '..', '..', '..');
const DESIGN_DIR = path.join(REPO_ROOT, 'design');

function repoRelativeExists(p: string): boolean {
  if (path.isAbsolute(p)) return fs.existsSync(p);
  return fs.existsSync(path.join(REPO_ROOT, p));
}

/**
 * Every `.sby` under tests/formal, repo-relative with forward slashes — the
 * V16 "a property on disk must have a recorded run" side of the check.
 */
function formalTasksOnDisk(): string[] {
  const dir = path.join(REPO_ROOT, 'tests', 'formal');
  if (!fs.existsSync(dir)) return [];
  return fs
    .readdirSync(dir)
    .filter((f) => f.endsWith('.sby'))
    .map((f) => `tests/formal/${f}`)
    .sort();
}

function cmdCheck(): number {
  const { blocks, ops } = loadLedger(DESIGN_DIR);
  const errors: string[] = [...blocks.schemaErrors, ...ops.schemaErrors];
  const formalFile = path.join(DESIGN_DIR, 'formal_runs.yml');
  let formalDoc: FormalRunsDoc | undefined;
  if (!fs.existsSync(formalFile)) {
    errors.push('V16: design/formal_runs.yml is missing — the formal-run registry is not optional');
  } else {
    formalDoc = loadYaml<FormalRunsDoc>(formalFile);
  }
  if (errors.length === 0) {
    const prev = readPrevBlocks(REPO_ROOT);
    errors.push(
      ...checkAll(
        blocks.doc,
        ops.doc,
        { prevBlocks: prev.doc, exists: repoRelativeExists, formalTasksOnDisk: formalTasksOnDisk() },
        formalDoc
      )
    );
    console.log(`ledger: V2 git-history — ${prev.note}`);
    console.log(`ledger: V16 formal registry — ${formalDoc?.runs?.length ?? 0} recorded run(s)`);
  }
  const nBlocks = blocks.doc.blocks?.length ?? 0;
  const nOps = ops.doc.ops?.length ?? 0;
  if (errors.length === 0) {
    // V14 staleness: committed generated files must equal regeneration (plan W2/R11).
    // Wired into check so `npm run ledger:check` (and the CTest shim) guards it too;
    // `npm run -w tools/ledger gen -- --verify` exposes the same comparison directly.
    const outputs: Array<[string, string]> = [
      [path.join(DESIGN_DIR, 'diagrams', 'architecture.mmd'), renderArchitecture(blocks.doc)],
      [path.join(DESIGN_DIR, 'diagrams', 'dashboard.md'), renderDashboard(blocks.doc, ops.doc)],
    ];
    for (const [file, content] of outputs) {
      const rel = path.relative(REPO_ROOT, file);
      if (!fs.existsSync(file)) {
        errors.push(`V14: ${rel} missing (run npm run ledger:gen and commit it)`);
      } else if (fs.readFileSync(file, 'utf8') !== content) {
        errors.push(`V14: ${rel} is stale — regeneration differs (run npm run ledger:gen and commit)`);
      }
    }
  }
  if (errors.length > 0) {
    console.error(`ledger: CHECK FAILED — ${errors.length} error(s) against ${nBlocks} blocks / ${nOps} ops`);
    for (const e of errors) console.error(`  - ${e}`);
    return 1;
  }
  const blocked = blocks.doc.blocks.filter((b) => b.blocked_on === 'hardware').length;
  console.log(`ledger: check OK — ${nBlocks} blocks (${blocked} blocked_on: hardware) / ${nOps} ops; schemas + V1–V16 + staleness green`);
  return 0;
}

function cmdGen(verify: boolean): number {
  const { blocks, ops } = loadLedger(DESIGN_DIR);
  const errs = [...blocks.schemaErrors, ...ops.schemaErrors];
  if (errs.length > 0) {
    console.error('ledger: refusing to generate from an invalid ledger:');
    for (const e of errs) console.error(`  - ${e}`);
    return 1;
  }
  const outputs: Array<[string, string]> = [
    [path.join(DESIGN_DIR, 'diagrams', 'architecture.mmd'), renderArchitecture(blocks.doc)],
    [path.join(DESIGN_DIR, 'diagrams', 'dashboard.md'), renderDashboard(blocks.doc, ops.doc)],
  ];
  let drift = 0;
  for (const [file, content] of outputs) {
    if (verify) {
      if (!fs.existsSync(file)) {
        console.error(`ledger: STALE — ${path.relative(REPO_ROOT, file)} does not exist (run npm run ledger:gen and commit)`);
        drift++;
        continue;
      }
      const committed = fs.readFileSync(file, 'utf8');
      if (committed !== content) {
        console.error(`ledger: STALE — ${path.relative(REPO_ROOT, file)} differs from regeneration (run npm run ledger:gen and commit)`);
        drift++;
      } else {
        console.log(`ledger: fresh — ${path.relative(REPO_ROOT, file)}`);
      }
    } else {
      fs.mkdirSync(path.dirname(file), { recursive: true });
      fs.writeFileSync(file, content, { encoding: 'utf8' });
      console.log(`ledger: wrote ${path.relative(REPO_ROOT, file)} (${Buffer.byteLength(content)} bytes)`);
    }
  }
  if (drift > 0) {
    console.error(`ledger: GEN VERIFY FAILED — ${drift} stale file(s); staleness is a CI failure (plan W2)`);
    return 1;
  }
  return 0;
}

function cmdGenContracts(): number {
  const { blocks } = loadLedger(DESIGN_DIR);
  if (blocks.schemaErrors.length > 0) {
    for (const e of blocks.schemaErrors) console.error(`  - ${e}`);
    return 1;
  }
  const res = genContracts(blocks.doc.blocks, DESIGN_DIR);
  console.log(`ledger: contracts — wrote ${res.written.length}, kept ${res.skipped.length} existing`);
  return 0;
}

function main(argv: string[]): number {
  const [cmd, ...rest] = argv;
  switch (cmd) {
    case 'check':
      return cmdCheck();
    case 'gen':
      return cmdGen(rest.includes('--verify'));
    case 'gen-contracts':
      return cmdGenContracts();
    default:
      console.error('usage: cli.js <check | gen [--verify] | gen-contracts>');
      return 2;
  }
}

if (require.main === module) {
  process.exitCode = main(process.argv.slice(2));
}
