/**
 * Deterministic Markdown emitter: design/diagrams/dashboard.md.
 * Maturity matrix, evidence links, budget-group sums vs §25, op×profile matrix,
 * blocked-on-hardware filter, counter coverage. No timestamps — byte-identical.
 */
import {
  MATURITY_LADDER,
  BUDGET_CEILINGS,
  type BlocksDoc,
  type OpsDoc,
  type ProfileId,
} from '../types';

function cell(n: number): string {
  return n === 0 ? '·' : String(n);
}

export function renderDashboard(blocksDoc: BlocksDoc, opsDoc: OpsDoc): string {
  const blocks = blocksDoc.blocks;
  const rtl = blocks.filter((b) => b.kind === 'rtl');
  const sw = blocks.filter((b) => b.kind === 'software');
  const L: string[] = [];

  L.push('# Zhaozhou design ledger — status dashboard');
  L.push('');
  L.push('> GENERATED from `design/blocks.yml` + `design/ops.yml` by `npm run ledger:gen` — do not edit.');
  L.push('> Staleness is a CI failure: regenerated output must be byte-identical to the committed file (plan W2/R11).');
  L.push('');
  L.push(`Blocks: **${blocks.length}** (${rtl.length} FPGA/rtl + ${sw.length} software) · Ops: **${opsDoc.ops.length}** (${opsDoc.ops.filter((o) => o.class === 'alu').length} ALU, ${opsDoc.ops.filter((o) => o.class === 'table').length} table, ${opsDoc.ops.filter((o) => o.class === 'sink').length} sinks, ${opsDoc.ops.filter((o) => o.class === 'stamp_mode').length} stamp modes) · Profiles: **${Object.keys(opsDoc.profiles).length}** (frozen five).`);
  L.push('');

  // ---------------- maturity matrix ----------------
  L.push('## Maturity matrix (charter §4 ladder)');
  L.push('');
  L.push(`| subsystem | ${MATURITY_LADDER.join(' | ')} | blocked | total |`);
  L.push(`|---|${MATURITY_LADDER.map(() => '---:').join('|')}|---:|---:|`);
  const subs = [...new Set(blocks.map((b) => b.subsystem))];
  subs.sort();
  for (const sub of subs) {
    const rows = blocks.filter((b) => b.subsystem === sub);
    const counts = MATURITY_LADDER.map((m) => rows.filter((b) => b.maturity === m).length);
    const blocked = rows.filter((b) => b.blocked_on === 'hardware').length;
    L.push(`| ${sub} | ${counts.map(cell).join(' | ')} | ${cell(blocked)} | ${rows.length} |`);
  }
  const tot = MATURITY_LADDER.map((m) => blocks.filter((b) => b.maturity === m).length);
  L.push(`| **all** | ${tot.map(cell).join(' | ')} | ${cell(blocks.filter((b) => b.blocked_on === 'hardware').length)} | ${blocks.length} |`);
  L.push('');

  // ---------------- evidence ----------------
  L.push('## Evidence ledger (maturity > SPECIFIED)');
  L.push('');
  const evidenced = blocks.filter((b) => (b.maturity_log ?? []).length > 0);
  if (evidenced.length > 0) {
    L.push('| block | state | date | commit | evidence |');
    L.push('|---|---|---|---|---|');
    for (const b of evidenced) {
      for (const e of b.maturity_log) {
        L.push(`| ${b.id} | ${e.state} | ${e.date} | \`${e.commit}\` | ${e.evidence} |`);
      }
    }
  } else {
    L.push('_None yet — every block is SPECIFIED (wave-1 bootstrap). Advancement requires commit-pinned evidence (rule V3, charter §4)._');
  }
  L.push('');

  // ---------------- budgets ----------------
  L.push('## Budget groups vs §25 ceilings');
  L.push('');
  L.push('Per-block percentage budgets are deliberately unfrozen until Phase 0 (charter §25: no absolute counts before board data); group membership is recorded from day one.');
  L.push('');
  L.push('| §25 group | ceiling | rtl blocks | allocated ALM% |');
  L.push('|---|---:|---:|---:|');
  for (const [group, ceiling] of Object.entries(BUDGET_CEILINGS)) {
    if (group === 'reserve') continue;
    const members = rtl.filter((b) => b.budget_group === group);
    const allocated = members.reduce((s, b) => s + (b.resource_budget?.alm_percent ?? 0), 0);
    L.push(`| ${group} | ${ceiling}% | ${members.length} | ${allocated}% |`);
  }
  L.push(`| _reserve (untouchable)_ | ${BUDGET_CEILINGS.reserve}% | — | — |`);
  L.push('');

  // ---------------- op × profile matrix ----------------
  L.push('## Op × profile matrix (frozen five)');
  L.push('');
  const profiles: ProfileId[] = ['E', 'W', 'F', 'M', 'S'];
  L.push(`| op | class | ${profiles.join(' | ')} | cost | Field IR |`);
  L.push(`|---|---|${profiles.map(() => ':---:').join('|')}|---:|---|`);
  for (const op of opsDoc.ops) {
    const marks = profiles.map((p) => (op.profiles.includes(p) ? String(op.cost_units) : '·'));
    const ir = op.field_ir_opcode ?? `macro: ${op.field_ir_macro!.join(' + ')}`;
    L.push(`| \`${op.id}\` | ${op.class} | ${marks.join(' | ')} | ${op.cost_units} | ${ir} |`);
  }
  L.push('');
  L.push('Cell value = provisional cost_units (instruction slots feeding FORM §14 `costs.zcost`). Numeric opcodes exist where the plan froze them (NOISE2 0x1C, DCURVE 0x1D); other mnemonics are pinned to numbers by the ISA v1 table (spec/form/field-ir.md, W5).');
  L.push('');

  // ---------------- blocked / deferred / cut ----------------
  L.push('## Blocked on hardware');
  L.push('');
  L.push('| block | owner | why |');
  L.push('|---|---|---|');
  for (const b of blocks.filter((x) => x.blocked_on === 'hardware')) {
    L.push(`| ${b.id} | ${b.owner_issue} | ${(b.notes ?? '').replace(/\|/g, '\\|')} |`);
  }
  L.push('');
  L.push('Rule: `blocked_on: hardware` blocks never advance from SPECIFIED regardless of evidence, until the orchestrator clears the hardware lane (plan §4).');
  L.push('');

  L.push('## Deferred / cut order (§26)');
  L.push('');
  const cut = blocks.filter((b) => b.cut_order != null).sort((a, b) => (a.cut_order ?? 99) - (b.cut_order ?? 99));
  if (cut.length > 0) {
    L.push('| cut order | block | deferred |');
    L.push('|---:|---|---|');
    for (const b of cut) {
      L.push(`| ${b.cut_order} | ${b.id} | ${b.deferred ? 'yes' : 'no'} |`);
    }
  } else {
    L.push('(none registered)');
  }
  const deferredOnly = blocks.filter((b) => b.deferred === true && b.cut_order == null);
  if (deferredOnly.length > 0) {
    L.push('');
    L.push(`Deferred without a cut slot: ${deferredOnly.map((b) => b.id).join(', ')}.`);
  }
  L.push('');

  // ---------------- counters ----------------
  L.push('## §25 counter coverage');
  L.push('');
  const wired = new Set(rtl.flatMap((b) => b.counters ?? []));
  const minimum = blocksDoc.counter_catalog.filter((c) =>
    [
      'frame_cycles','deadline_faults','commands','meshlets_fetched','vertices_decoded','vertices_transformed',
      'triangles_submitted','triangles_clipped','triangles_culled','lod_representation_counts',
      'field_instructions_by_profile','progcach_hits','progcach_misses','programs_rejected',
      'terrain_samples_evaluated','terrain_triangles_emitted','surface_stamps','surface_texels_touched',
      'tile_references','max_tile_list_depth','covered_fragments','early_z_rejects','texture_samples',
      'cache_hits','cache_misses','blended_fragments','soft_particles','polygon_particles',
      'vram_bytes_by_client','hps_ddr_bytes_by_client','scanout_starvation_cycles','audio_underruns',
    ].includes(c)
  );
  const unwired = minimum.filter((c) => !wired.has(c));
  L.push(`§25 minimum counters wired to at least one rtl block: **${minimum.length - unwired.length}/${minimum.length}**.`);
  if (unwired.length > 0) {
    L.push('');
    L.push(`Not yet wired: ${unwired.map((c) => `\`${c}\``).join(', ')}.`);
  }
  L.push('');
  return L.join('\n');
}
