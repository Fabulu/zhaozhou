/**
 * Deterministic Mermaid emitter: design/diagrams/architecture.mmd.
 * One subgraph per subsystem, one node per block, edges from upstream/downstream,
 * classDef per maturity (the schematic IS the status dashboard — charter §4).
 * No timestamps, no locale-dependent sorting: byte-identical on every run.
 */
import type { BlocksDoc } from '../types';

function nodeId(id: string): string {
  return 'n_' + id.replace(/[^A-Za-z0-9]/g, '_');
}

const SUBSYSTEM_ORDER = [
  'platform',
  'command',
  'memory',
  'field',
  'input',
  'audio',
  'video',
  'measure',
  'terrain',
  'surface',
  'geometry',
  'raster',
  'texture',
  'particles',
  'forge',
  'compositor',
  'debug',
  'sw',
] as const;

const SUBSYSTEM_TITLES: Record<string, string> = {
  platform: 'SYS — platform glue',
  command: 'CMD — command path',
  memory: 'MEM — memory system',
  field: 'FIELD — Field IR sequencers',
  input: 'INPUT',
  audio: 'AUDIO',
  video: 'VIDEO',
  measure: 'MEASURE — The Measure',
  terrain: 'TERRAIN — Mantle',
  surface: 'SURFACE — Scar Scribe',
  geometry: 'GEOM — Ten Thousand Forms',
  raster: 'RASTER — tile renderer',
  texture: 'TEXTURE',
  particles: 'PART — Myriad',
  forge: 'FORGE — Primitive Forge',
  compositor: 'TWOD/POST — Twin Horizons · Mirror Gate',
  debug: 'DEBUG',
  sw: 'SW — software (HPS)',
};

export function renderArchitecture(doc: BlocksDoc): string {
  const lines: string[] = [];
  lines.push('---');
  lines.push('title: Zhaozhou console architecture — generated from design/blocks.yml (do not edit)');
  lines.push('---');
  lines.push('flowchart LR');
  lines.push('');
  lines.push('    %% classDef per maturity: the schematic is the status dashboard (charter §4)');
  lines.push('    classDef specified fill:#f2f2f2,stroke:#999,color:#333');
  lines.push('    classDef advanced fill:#cfe8cf,stroke:#3c7a3c,color:#111');
  lines.push('    classDef blocked fill:#f7d4d4,stroke:#b33,stroke-dasharray:5 4,color:#111');
  lines.push('    classDef deferred fill:#eee6f7,stroke:#86c,stroke-dasharray:2 3,color:#333');
  lines.push('');

  for (const sub of SUBSYSTEM_ORDER) {
    const blocks = doc.blocks.filter((b) => b.subsystem === sub);
    if (blocks.length === 0) continue;
    lines.push(`    subgraph ${sub.replace(/[^a-z]/g, '')}["${SUBSYSTEM_TITLES[sub] ?? sub}"]`);
    for (const b of blocks) {
      const label = `${b.id}\\n${b.name}`;
      lines.push(`        ${nodeId(b.id)}["${label}"]`);
    }
    lines.push('    end');
    lines.push('');
  }

  lines.push('    %% dataflow edges — exactly the upstream/downstream relation of blocks.yml (V7-checked)');
  const seen = new Set<string>();
  for (const b of doc.blocks) {
    for (const u of b.upstream) {
      const key = `${u}->${b.id}`;
      if (seen.has(key)) continue;
      seen.add(key);
      const crosses = doc.blocks.find((x) => x.id === u)?.clock_domain !== b.clock_domain;
      const suffix = crosses ? ' -. CDC .->' : ' -->';
      // Mermaid: A --> B ; dashed with label marks a clock-domain crossing (V8 bridged)
      lines.push(`    ${nodeId(u)}${suffix}${nodeId(b.id)}`);
    }
  }
  lines.push('');

  lines.push('    %% maturity classes');
  const specified = doc.blocks.filter(
    (b) => b.maturity === 'SPECIFIED' && !b.blocked_on && !b.deferred
  );
  if (specified.length) {
    lines.push(`    class ${specified.map((b) => nodeId(b.id)).join(',')} specified`);
  }
  const advanced = doc.blocks.filter((b) => b.maturity !== 'SPECIFIED');
  if (advanced.length) {
    lines.push(`    class ${advanced.map((b) => nodeId(b.id)).join(',')} advanced`);
  }
  const blocked = doc.blocks.filter((b) => b.blocked_on === 'hardware');
  if (blocked.length) {
    lines.push(`    class ${blocked.map((b) => nodeId(b.id)).join(',')} blocked`);
  }
  const deferred = doc.blocks.filter((b) => b.deferred === true && !b.blocked_on);
  if (deferred.length) {
    lines.push(`    class ${deferred.map((b) => nodeId(b.id)).join(',')} deferred`);
  }
  lines.push('');
  return lines.join('\n');
}
