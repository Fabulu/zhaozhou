/**
 * Contract scaffolder: design/contracts/<ID>.md — one stub per block with the
 * §4 required headings. Filled (authoritative) for Phase-1-active blocks only:
 * CMD.*, DEBUG.*, SW.* (plan W2). The generator NEVER overwrites an existing
 * file: contracts are scaffolding evidence, not a staleness-checked artifact.
 */
import * as fs from 'node:fs';
import * as path from 'node:path';
import type { Block } from '../types';

/** Extra authored content for Phase-1-active blocks (filled contracts). */
const FILLED_NOTES: Record<string, string> = {
  'CMD.DMA':
    'Exclusions: no semantic decoding (that is CMD.DECODER), no reordering of packets within an epoch. ' +
    'Clock/reset: gpu domain, reset returns the fetch FSM to idle with the ring pointer unadvanced (a partially fetched packet is never handed on). ' +
    'Malformed input: header CRC failure or epoch mismatch drops the packet, increments deadline_faults, and yields the safe error code — never partial delivery.',
  'CMD.DECODER':
    'Exclusions: no frame-slot ownership (CMD.SCHEDULER), no memory access. ' +
    'Packet layouts come EXCLUSIVELY from the generated ABI package (spec/commands.zidl → zhao_abi_pkg.sv); hand-written layouts are a review blocker. ' +
    'Malformed input: per-record length/opcode checks; a malformed record stops the packet with a safe error and no register/memory side effects (Dalvik model — reject before any write).',
  'CMD.SCHEDULER':
    'Exclusions: no decoding, no data-path work. ' +
    'Slot FSM: FREE → CLAIMED → EXECUTING → FENCED → DONE with deadline enforcement and a fault counter; the completion fence is the only signal that releases a slot. ' +
    'Overflow/malformed: a slot that misses its deadline faults, repeats or drops per policy, and NEVER crosses the frame boundary into the next slot.',
  'DEBUG.COUNTERS':
    'Exclusions: no trace (DEBUG.TRACE), no CRCs (DEBUG.CRC). ' +
    'The counter set is exactly the counter_catalog of design/blocks.yml (§25 minimum + declared extensions); adding a counter is a ledger edit, not an RTL whim. ' +
    'Counters are readable via debug commands (0xF00n umbrella, header flags bit0 set).',
  'DEBUG.CRC':
    'Exclusions: not a general checksum engine — CRC-32C only, identical polynomial/table to the frame packet and .zcap CRC (plan A3d). ' +
    'Evidence role: tile/frame CRCs are the HARDWARE_PROVEN maturity evidence primitive (§29-17) and the differential compare key.',
  'DEBUG.TRACE':
    'Exclusions: no counters, no CRC. ' +
    'Source-ID scheme per spec/capture_format.md §5: every trace event carries the propagated source ID; the ring drains into the HPS trace arena via MEM.HPS.BRIDGE. ' +
    'Trace selection is a debug command; the ring is bounded and drops-oldest with a drop count (never silently stops).',
};

const SW_SCOPE_NOTE =
  'Phase-1 scope: this software block is wave-1-active. Its contract is authoritative NOW; the headings below that ' +
  'name C++/RTL artifacts describe the shape of the evidence to come, and no maturity advance happens without that ' +
  'evidence being committed (rules V2/V3).';

const HEADINGS: ReadonlyArray<{ title: string; rtlOnly?: boolean; swOnly?: boolean }> = [
  { title: 'Purpose and exclusions' },
  { title: 'Clock and reset semantics', rtlOnly: true },
  { title: 'Input and output packet layouts' },
  { title: 'Backpressure rules' },
  { title: 'Memory ownership' },
  { title: 'Q formats and rounding' },
  { title: 'Latency (fixed or variable)' },
  { title: 'Target throughput' },
  { title: 'Overflow and malformed-input behaviour' },
  { title: 'Counters and traces', rtlOnly: true },
  { title: 'Scalar reference function', rtlOnly: true },
  { title: 'Directed tests' },
  { title: 'Randomized differential tests' },
  { title: 'Formal properties', rtlOnly: true },
  { title: 'Synthesis / resource ceiling', rtlOnly: true },
  { title: 'Integration capture cases' },
];

export function renderContract(block: Block): string {
  const filled = FILLED_NOTES[block.id] !== undefined || block.kind === 'software';
  const L: string[] = [];
  L.push(`# Contract — ${block.id} (${block.name})`);
  L.push('');
  L.push(`> Ledger: \`design/blocks.yml\` · owner ${block.owner_issue} · phase ${block.phase} · maturity ${block.maturity}${block.blocked_on ? ` · **blocked_on: ${block.blocked_on}**` : ''}${block.deferred ? ' · deferred' : ''}`);
  L.push('');
  L.push('## Purpose and exclusions');
  L.push('');
  L.push(block.purpose);
  if (FILLED_NOTES[block.id]) {
    L.push('');
    L.push(FILLED_NOTES[block.id]);
  }
  if (block.kind === 'software') {
    L.push('');
    L.push(SW_SCOPE_NOTE);
  }
  L.push('');
  for (const h of HEADINGS.slice(1)) {
    if (h.rtlOnly && block.kind !== 'rtl') continue;
    L.push(`## ${h.title}`);
    L.push('');
    if (filled) {
      switch (h.title) {
        case 'Backpressure rules':
          L.push(`Backpressure: \`${block.backpressure}\`.`);
          break;
        case 'Latency (fixed or variable)':
          L.push(`Latency: \`${block.latency}\`.`);
          break;
        case 'Target throughput':
          L.push(`Target throughput: ${block.target_throughput}.`);
          break;
        case 'Scalar reference function':
          L.push(`Reference: \`${block.reference_model}\` (SW.ZREF).`);
          break;
        case 'Directed tests':
          L.push(`Planned: \`${block.tests?.directed ?? block.tests?.unit ?? '(tbd)'}\`.`);
          break;
        case 'Randomized differential tests':
          L.push(`Planned: \`${block.tests?.random ?? block.tests?.differential ?? '(tbd)'}\`.`);
          break;
        case 'Counters and traces':
          L.push(`Counters: ${(block.counters ?? []).map((c) => `\`${c}\``).join(', ')}. Source IDs: ${block.source_ids ? 'propagated' : 'n/a'}.`);
          break;
        case 'Formal properties':
          L.push(block.tests?.formal ? `Planned: \`${block.tests.formal}\`.` : 'None planned for this block.');
          break;
        case 'Synthesis / resource ceiling':
          L.push(`Budget group: \`${block.budget_group}\` (§25). Per-block percentages unfrozen until Phase 0 (V5 gate).`);
          break;
        default:
          L.push('TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).');
      }
    } else {
      L.push('TODO — stub generated by `npm run -w tools/ledger gen-contracts`; fill before this block advances past SPECIFIED (charter §4).');
    }
    L.push('');
  }
  if (block.notes) {
    L.push('## Notes');
    L.push('');
    L.push(String(block.notes));
    L.push('');
  }
  return L.join('\n');
}

export interface ContractGenResult {
  written: string[];
  skipped: string[];
}

/** Writes ONLY missing contract files (never overwrites — hand edits win). */
export function genContracts(blocks: Block[], designDir: string): ContractGenResult {
  const dir = path.join(designDir, 'contracts');
  fs.mkdirSync(dir, { recursive: true });
  const written: string[] = [];
  const skipped: string[] = [];
  for (const b of blocks) {
    const file = path.join(dir, `${b.id}.md`);
    if (fs.existsSync(file)) {
      skipped.push(b.id);
      continue;
    }
    fs.writeFileSync(file, renderContract(b), { encoding: 'utf8' });
    written.push(b.id);
  }
  return { written, skipped };
}
