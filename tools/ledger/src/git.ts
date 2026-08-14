/**
 * V2 git-history support: read the previously committed blocks.yml.
 * Charter §4 maturity law is temporal — advancement is only legal relative to
 * what the ledger said at HEAD. Bootstrap exemption: when the file first
 * appears in history there is nothing to compare against (plan R5).
 */
import { spawnSync } from 'node:child_process';
import { parse as parseYaml } from 'yaml';
import type { BlocksDoc } from './types';

export interface PrevLedger {
  /** null = bootstrap (file not present in HEAD) or git unavailable (tests, CI archive builds). */
  doc: BlocksDoc | null;
  note: string;
}

export function readPrevBlocks(repoRoot: string, ref = 'HEAD', relPath = 'design/blocks.yml'): PrevLedger {
  const git = (args: string[]): { status: number | null; stdout: string; error?: Error } =>
    spawnSync('git', args, { cwd: repoRoot, encoding: 'utf8', windowsHide: true }) as {
      status: number | null;
      stdout: string;
      error?: Error;
    };

  const cat = git(['show', `${ref}:${relPath}`]);
  if (cat.error || cat.status !== 0 || !cat.stdout) {
    return {
      doc: null,
      note: `bootstrap: ${relPath} not found at ${ref} (first appearance — maturity ordering rule V2 exempt, plan R5)`,
    };
  }
  const doc = parseYaml(cat.stdout) as BlocksDoc;
  if (!doc || !Array.isArray(doc.blocks)) {
    return { doc: null, note: `bootstrap: ${relPath} at ${ref} is not a blocks document` };
  }
  return { doc, note: `comparing against ${ref}` };
}
