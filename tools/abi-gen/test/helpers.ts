// helpers.ts — shared test utilities: locate the repo root from compiled
// test output (dist/test/) regardless of workspace nesting.

import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

export function repoRoot(): string {
  let dir = path.dirname(fileURLToPath(import.meta.url));
  for (let i = 0; i < 8; i++) {
    if (existsSync(path.join(dir, 'spec', 'commands.zidl'))) return dir;
    dir = path.dirname(dir);
  }
  throw new Error('repo root not found (spec/commands.zidl missing?)');
}

export function goldenDir(): string {
  return path.join(repoRoot(), 'tests', 'abi', 'golden');
}
