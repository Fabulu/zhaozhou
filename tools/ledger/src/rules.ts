/**
 * Relational validation rules V1–V14 (P2 FINDINGS §5, as gated by plan W2).
 * The JSON Schema files validate SHAPE; these rules validate LAW:
 * maturity ordering vs git history, evidence, cross-references, CDC discipline,
 * op/profile membership, counters, budget ceilings.
 *
 * Rules are pure: disk access is injected via `exists` so unit tests can run
 * without touching the repository (charter §27: deterministic tools).
 */
import {
  MATURITY_LADDER,
  SOFTWARE_MAX_MATURITY,
  BUDGET_CEILINGS,
  TOTAL_ALM_CEILING_PERCENT,
  type Block,
  type BlocksDoc,
  type Maturity,
  type OpsDoc,
  type ProfileId,
} from './types';

export interface RuleOptions {
  /** Previously committed blocks.yml (from git); null = bootstrap (file's first appearance). */
  prevBlocks?: BlocksDoc | null;
  /** Path-existence probe (repo-root-relative). Defaults to () => true. */
  exists?: (p: string) => boolean;
}

const LADDER_INDEX: ReadonlyMap<Maturity, number> = new Map(
  MATURITY_LADDER.map((m, i) => [m, i] as const)
);

function rank(m: Maturity): number {
  return LADDER_INDEX.get(m) ?? -1;
}

export function checkBlocks(blocksDoc: BlocksDoc, opts: RuleOptions = {}): string[] {
  const exists = opts.exists ?? (() => true);
  const errors: string[] = [];
  const blocks = blocksDoc.blocks;
  const byId = new Map<string, Block>();
  const catalog = new Set(blocksDoc.counter_catalog ?? []);

  // --- V15: counter_id totality (plan W2.1/D9, spec/counters.md 2) -------------
  // counter_id = catalog index (u16 everywhere: .zcap COUNTERS, RTL read-mux,
  // debug payloads). That law is only sound if the index space is DENSE and
  // TOTAL: the catalog must be duplicate-free (a duplicate tears a hole in
  // the index space a reader cannot interpret), and every counter a block
  // declares must map to a catalog index (membership itself is V12; this
  // rule additionally rejects duplicate declarations inside one block,
  // which would alias the same counter_id twice in a snapshot).
  const catalogList = blocksDoc.counter_catalog ?? [];
  const seenCatalog = new Set<string>();
  catalogList.forEach((c, idx) => {
    if (seenCatalog.has(c)) {
      errors.push(
        `V15: counter_catalog entry "${c}" at index ${idx} duplicates index ${catalogList.indexOf(c)} — ` +
        'counter_id = catalog index requires a duplicate-free, contiguous index space (spec/counters.md 2)'
      );
    }
    seenCatalog.add(c);
  });
  for (const b of blocks) {
    const seen = new Set<string>();
    for (const c of b.counters ?? []) {
      if (seen.has(c)) {
        errors.push(`V15: ${b.id} declares counter "${c}" twice (one counter_id, one snapshot entry)`);
      }
      seen.add(c);
    }
  }

  // --- V1: id uniqueness (regex/subsystem shape live in the JSON Schema) ---
  for (const b of blocks) {
    if (byId.has(b.id)) errors.push(`V1: duplicate block id ${b.id}`);
    byId.set(b.id, b);
  }

  // --- V2: maturity ordering vs previously committed ledger ---
  // Bootstrap exemption: when blocks.yml first appears in history there is no
  // previous version and every block may start at SPECIFIED.
  const prev = opts.prevBlocks ?? null;
  if (prev) {
    const prevById = new Map(prev.blocks.map((b) => [b.id, b] as const));
    for (const b of blocks) {
      const p = prevById.get(b.id);
      if (!p) continue; // new block: V3 governs evidence, not ordering
      const delta = rank(b.maturity) - rank(p.maturity);
      if (delta > 1) {
        errors.push(
          `V2: ${b.id} advanced ${p.maturity} -> ${b.maturity} (${delta} steps); maturity advances one step at a time (charter §4)`
        );
      }
      if (delta < 0) {
        const explained = (b.maturity_log ?? []).some((e) => typeof e.regression_reason === 'string');
        if (!explained) {
          errors.push(
            `V2: ${b.id} regressed ${p.maturity} -> ${b.maturity} without a maturity_log regression_reason`
          );
        }
      }
    }
  }

  for (const b of blocks) {
    // --- blocked_on: hardware never advances from SPECIFIED (plan §4 rule) ---
    if (b.blocked_on === 'hardware' && b.maturity !== 'SPECIFIED') {
      errors.push(`V2: ${b.id} is blocked_on: hardware but maturity is ${b.maturity} (must stay SPECIFIED)`);
    }

    // --- V3: every maturity > SPECIFIED has dated, commit-pinned, on-disk evidence ---
    if (b.maturity !== 'SPECIFIED') {
      const entries = (b.maturity_log ?? []).filter((e) => e.state === b.maturity);
      if (entries.length === 0) {
        errors.push(`V3: ${b.id} maturity ${b.maturity} has no maturity_log entry for that state`);
      } else {
        for (const e of entries) {
          if (!/^\d{4}-\d{2}-\d{2}$/.test(e.date)) {
            errors.push(`V3: ${b.id} maturity_log entry has invalid date "${e.date}"`);
          }
          if (!/^[0-9a-f]{7,40}$/.test(e.commit)) {
            errors.push(`V3: ${b.id} maturity_log entry has invalid commit "${e.commit}"`);
          }
          if (!exists(e.evidence)) {
            errors.push(`V3: ${b.id} evidence path "${e.evidence}" does not exist on disk`);
          }
        }
      }
    }

    // --- V4: RTL blocks carry contract + reference + tests + counters + source ids ---
    if (b.kind === 'rtl') {
      if (!b.reference_model) errors.push(`V4: rtl block ${b.id} has no reference_model`);
      if (!b.tests?.directed) errors.push(`V4: rtl block ${b.id} has no tests.directed`);
      if (!b.tests?.random) errors.push(`V4: rtl block ${b.id} has no tests.random`);
      if (!b.counters || b.counters.length < 1) errors.push(`V4: rtl block ${b.id} has no counters`);
      if (b.source_ids !== true) errors.push(`V4: rtl block ${b.id} must set source_ids: true`);
    }

    // --- V12: counters extend the §25 allowlist via the declared catalog ---
    for (const c of b.counters ?? []) {
      if (!catalog.has(c)) errors.push(`V12: ${b.id} counter "${c}" is not in counter_catalog`);
    }

    // --- V13: software ladder restriction ---
    if (b.kind === 'software' && !b.runs_on_target_hardware) {
      if (rank(b.maturity) > rank(SOFTWARE_MAX_MATURITY)) {
        errors.push(
          `V13: software block ${b.id} at ${b.maturity} exceeds ${SOFTWARE_MAX_MATURITY} without runs_on_target_hardware`
        );
      }
    }

    // --- V6: referenced paths exist; test paths gated on maturity > SPECIFIED ---
    if (!exists(b.contract)) errors.push(`V6: ${b.id} contract path "${b.contract}" does not exist`);
    if (b.maturity !== 'SPECIFIED') {
      const testPaths = [b.tests?.directed, b.tests?.random, b.tests?.formal, b.tests?.unit].filter(
        (p): p is string => typeof p === 'string'
      );
      for (const p of testPaths) {
        if (!exists(p)) errors.push(`V6: ${b.id} test path "${p}" does not exist (maturity ${b.maturity})`);
      }
      if (b.resource_actual && b.resource_actual.source_report && !exists(b.resource_actual.source_report)) {
        errors.push(`V6: ${b.id} resource_actual report "${b.resource_actual.source_report}" does not exist`);
      }
    }

    // --- V5: once SYNTHESIZED, resource_actual <= budget; group sums <= §25 ---
    if (rank(b.maturity) >= rank('SYNTHESIZED')) {
      if (!b.resource_actual) {
        errors.push(`V5: ${b.id} is ${b.maturity} but has no resource_actual (tools/report output)`);
      } else if (b.resource_budget?.alm_percent !== undefined && b.resource_actual.alm !== undefined) {
        if (b.resource_actual.alm > b.resource_budget.alm_percent) {
          errors.push(`V5: ${b.id} resource_actual alm ${b.resource_actual.alm} exceeds budget ${b.resource_budget.alm_percent}`);
        }
      }
    }
  }

  // --- V5 (group sums + total ALM ceiling) ---
  const groupSums = new Map<string, number>();
  let totalAlm = 0;
  for (const b of blocks) {
    if (b.kind !== 'rtl') continue;
    const alm = b.resource_budget?.alm_percent ?? 0;
    if (alm > 0) {
      const g = b.budget_group ?? '(none)';
      groupSums.set(g, (groupSums.get(g) ?? 0) + alm);
      totalAlm += alm;
    }
  }
  for (const [g, sum] of groupSums) {
    const ceiling = BUDGET_CEILINGS[g];
    if (ceiling !== undefined && sum > ceiling) {
      errors.push(`V5: budget group ${g} sums to ${sum}% > §25 ceiling ${ceiling}%`);
    }
  }
  if (totalAlm > TOTAL_ALM_CEILING_PERCENT) {
    errors.push(`V5: total allocated ALM ${totalAlm}% exceeds the ${TOTAL_ALM_CEILING_PERCENT}% ceiling (§25 reserve)`);
  }

  // --- V7: edges symmetric, reference existing blocks, connectivity ---
  for (const b of blocks) {
    for (const u of b.upstream ?? []) {
      const up = byId.get(u);
      if (!up) {
        errors.push(`V7: ${b.id} upstream references unknown block ${u}`);
        continue;
      }
      if (!(up.downstream ?? []).includes(b.id)) {
        errors.push(`V7: edge ${u} -> ${b.id} is not symmetric (${u}.downstream does not list ${b.id})`);
      }
    }
    for (const d of b.downstream ?? []) {
      const dn = byId.get(d);
      if (!dn) {
        errors.push(`V7: ${b.id} downstream references unknown block ${d}`);
        continue;
      }
      if (!(dn.upstream ?? []).includes(b.id)) {
        errors.push(`V7: edge ${b.id} -> ${d} is not symmetric (${d}.upstream does not list ${b.id})`);
      }
    }
    if ((b.upstream ?? []).length === 0 && (b.downstream ?? []).length === 0 && !b.leaf) {
      errors.push(`V7: ${b.id} is in no edge; mark it leaf: true or connect it (ledger is the schematic)`);
    }
  }

  // --- V8: clock-domain-crossing edges route via SYS.CDC or a documented bridge ---
  for (const b of blocks) {
    for (const d of b.downstream ?? []) {
      const dn = byId.get(d);
      if (!dn) continue; // V7 already flagged it
      if (b.clock_domain !== dn.clock_domain) {
        const bridged = b.async_bridge === true || dn.async_bridge === true;
        if (!bridged) {
          errors.push(
            `V8: edge ${b.id} (${b.clock_domain}) -> ${d} (${dn.clock_domain}) crosses clock domains without SYS.CDC or a documented async bridge (async_bridge: true)`
          );
        }
      }
    }
  }

  // --- superseded_by must resolve ---
  for (const b of blocks) {
    if (b.superseded_by && !byId.has(b.superseded_by)) {
      errors.push(`V1: ${b.id} superseded_by references unknown block ${b.superseded_by}`);
    }
  }

  return errors;
}

export function checkOps(
  opsDoc: OpsDoc,
  blocksDoc: BlocksDoc,
  opts: RuleOptions = {}
): string[] {
  const exists = opts.exists ?? (() => true);
  const errors: string[] = [];
  const byId = new Map(blocksDoc.blocks.map((b) => [b.id, b] as const));
  const opIds = new Set<string>();
  const frozenFive: readonly ProfileId[] = ['E', 'W', 'F', 'M', 'S'];

  // --- V9: profiles subset of the frozen five, >= 1 each; all five in use ---
  const usedProfiles = new Set<ProfileId>();
  for (const op of opsDoc.ops) {
    if (opIds.has(op.id)) errors.push(`V9: duplicate op id ${op.id}`);
    opIds.add(op.id);
    if (op.profiles.length < 1) errors.push(`V9: op ${op.id} has no profiles`);
    for (const p of op.profiles) {
      if (!frozenFive.includes(p)) errors.push(`V9: op ${op.id} has unknown profile "${p}"`);
      usedProfiles.add(p);
    }
  }
  for (const p of frozenFive) {
    if (!usedProfiles.has(p)) errors.push(`V9: frozen profile ${p} is not used by any op`);
  }

  for (const op of opsDoc.ops) {
    // --- V10: reference_function + differential_tests (existence gated on maturity) ---
    if (!/^zref::fieldir::[a-z0-9_]+$/.test(op.reference_function)) {
      errors.push(`V10: op ${op.id} reference_function "${op.reference_function}" is not a zref::fieldir:: symbol`);
    }
    // Path existence is GATED: planned paths are legal while every implementing
    // block is still SPECIFIED (ledger exists before the tests do); the moment
    // any implementation block advances, the evidence must be on disk.
    const implemented = op.implementation_blocks.filter((id) => byId.has(id));
    const active = implemented.some((id) => byId.get(id)!.maturity !== 'SPECIFIED');
    if (active && !exists(op.differential_tests)) {
      errors.push(`V10: op ${op.id} differential_tests "${op.differential_tests}" missing (implementation active past SPECIFIED)`);
    }

    // --- V11: implementation_blocks exist ---
    for (const id of op.implementation_blocks) {
      if (!byId.has(id)) errors.push(`V11: op ${op.id} implementation_blocks references unknown block ${id}`);
    }

    // --- A3e: every op carries field_ir_opcode or a macro-expansion reference ---
    if (!op.field_ir_opcode && !(op.field_ir_macro && op.field_ir_macro.length > 0)) {
      errors.push(`A3e: op ${op.id} carries neither field_ir_opcode nor field_ir_macro`);
    }
    for (const m of op.field_ir_macro ?? []) {
      if (!opIds.has(m)) errors.push(`A3e: op ${op.id} field_ir_macro references unknown op ${m}`);
    }
  }

  // --- V11: every FIELD.SEQ.* implements >= 1 op ---
  for (const b of blocksDoc.blocks) {
    if (b.id.startsWith('FIELD.SEQ.')) {
      const n = opsDoc.ops.filter((op) => op.implementation_blocks.includes(b.id)).length;
      if (n === 0) errors.push(`V11: sequencer ${b.id} implements no op in ops.yml`);
    }
  }

  // --- profile sequencers must exist ---
  for (const [pid, prof] of Object.entries(opsDoc.profiles)) {
    if (!byId.has(prof.sequencer)) {
      errors.push(`V9: profile ${pid} sequencer ${prof.sequencer} does not exist in blocks.yml`);
    }
  }

  return errors;
}

export function checkAll(
  blocksDoc: BlocksDoc,
  opsDoc: OpsDoc,
  opts: RuleOptions = {}
): string[] {
  return [...checkBlocks(blocksDoc, opts), ...checkOps(opsDoc, blocksDoc, opts)];
}
