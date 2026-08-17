// cost_report.ts — canonical costs.zcost emission (cost-model.md §2.1).

import type { FieldProgram } from '../field_ir/types.js';
import { declarationsOf, type HirProgram } from '../hir/model.js';
import type { ZirProgram } from '../zir/model.js';

export const COST_SCHEMA = 'zhaozhou/spec/form/cost-model.md#2.1';

export interface BudgetLine {
  line: string;
  limit: number;
  owner: string;
}

export interface PoolRef {
  module: number;
  name: string;
}

export interface CostReportOptions {
  abiVersion: number;
  commandMemoryCeilingBytes: number;
  /** Physical Field IR supplied by W3.4; one program per HIR field. */
  fieldPrograms: readonly Pick<FieldProgram, 'profile' | 'sourceId' | 'cost' | 'outputs'>[];
  /** Pools to which a flow application is attached, after field-dialect admission. */
  flowPools?: readonly PoolRef[];
  budgets?: readonly BudgetLine[];
}

export interface CostReport {
  $schema: typeof COST_SCHEMA;
  abi_version: number;
  budgets: BudgetLine[];
  command_memory: { ceiling_bytes: number; per_frame_estimate_bytes: number };
  modules: { index: number; name: string }[];
  particle_bandwidth: {
    bytes_per_element: number;
    peak_elements: number;
    bytes_per_tick: number;
    pools: string[];
  };
  pools: { capacity: number; element_bytes: number; module: number; name: string }[];
  programs: {
    attr0_writes: number;
    class_counts: { ALU: number; MUL: number; NOISE: number; SPECIAL: number; TABLE: number };
    cycles_est: number;
    dsp: number;
    footprint_rect: [number, number, number, number];
    instr_count: number;
    kind: 'field';
    max_ops: number;
    module: number;
    name: string;
    profile: 'earth' | 'flow';
    register_hwm: number;
    source_id: number;
    table_bytes: number;
  }[];
  rates: { every: number; module: number; name: string; phase: number; stagger: boolean }[];
  scenario_asserts: { lines: string[]; module: number; name: string }[];
  source_attribution: 'sourceids.zmap';
}

type Json = null | boolean | number | string | Json[] | { [key: string]: Json };

function canonical(value: Json): Json {
  if (Array.isArray(value)) return value.map(canonical);
  if (value !== null && typeof value === 'object') {
    const out: { [key: string]: Json } = {};
    for (const key of Object.keys(value).sort()) out[key] = canonical(value[key]!);
    return out;
  }
  return value;
}

function uint(value: number, label: string, max = Number.MAX_SAFE_INTEGER): number {
  if (!Number.isSafeInteger(value) || value < 0 || value > max) {
    throw new Error(`costs.zcost ${label} must be an unsigned safe integer`);
  }
  return value;
}

function fixed(value: bigint, label: string): number {
  const number = Number(value);
  if (!Number.isSafeInteger(number)) throw new Error(`costs.zcost ${label} exceeds exact JSON integer range`);
  return number;
}

export function buildCostReport(hir: HirProgram, zir: ZirProgram, options: CostReportOptions): CostReport {
  const abiVersion = uint(options.abiVersion, 'abi_version', 0xffff_ffff);
  const ceiling = uint(options.commandMemoryCeilingBytes, 'command_memory.ceiling_bytes');
  const physicalById = new Map(options.fieldPrograms.map((program) => [program.sourceId, program]));
  if (physicalById.size !== options.fieldPrograms.length) throw new Error('costs.zcost duplicate field program source_id');

  const fields = declarationsOf(hir, 'field');
  const programs: CostReport['programs'] = fields.map((field) => {
    const physical = physicalById.get(field.sourceId);
    if (!physical) throw new Error(`costs.zcost missing physical Field IR for ${field.module}.${field.name}`);
    if (physical.profile !== field.profile) throw new Error(`costs.zcost profile mismatch for ${field.module}.${field.name}`);
    if (physical.cost.instrCount > field.maxOps) throw new Error(`costs.zcost instr_count exceeds max_ops for ${field.module}.${field.name}`);
    physicalById.delete(field.sourceId);
    const count = physical.cost.byClass;
    return {
      attr0_writes: physical.outputs.some((lane) => lane.name === 'attr0') ? 1 : 0,
      class_counts: {
        ALU: uint(count.ALU, 'programs[].class_counts.ALU'),
        MUL: uint(count.MUL, 'programs[].class_counts.MUL'),
        NOISE: uint(count.NOISE, 'programs[].class_counts.NOISE'),
        SPECIAL: uint(count.SPECIAL, 'programs[].class_counts.SPECIAL'),
        TABLE: uint(count.TABLE, 'programs[].class_counts.TABLE'),
      },
      cycles_est: uint(physical.cost.cycles, 'programs[].cycles_est'),
      dsp: uint(physical.cost.dsp, 'programs[].dsp'),
      footprint_rect: field.footprint.rect.map((value) => fixed(value, 'programs[].footprint_rect')) as [number, number, number, number],
      instr_count: uint(physical.cost.instrCount, 'programs[].instr_count'),
      kind: 'field',
      max_ops: uint(field.maxOps, 'programs[].max_ops'),
      module: uint(field.module, 'programs[].module', 0xfff),
      name: field.name,
      profile: field.profile,
      register_hwm: uint(physical.cost.regHighWater, 'programs[].register_hwm'),
      source_id: uint(field.sourceId, 'programs[].source_id', 0xffff_ffff),
      table_bytes: uint(physical.cost.tableBytes, 'programs[].table_bytes'),
    };
  });
  if (physicalById.size !== 0) throw new Error('costs.zcost contains Field IR with no HIR declaration');

  const pools = declarationsOf(hir, 'pool');
  const flowKeys = new Set((options.flowPools ?? []).map((pool) => `${pool.module}\0${pool.name}`));
  const attached = pools.filter((pool) => flowKeys.delete(`${pool.module}\0${pool.name}`));
  if (flowKeys.size !== 0) throw new Error(`costs.zcost unknown flow pool '${[...flowKeys][0]!.replace('\0', '.')}'`);
  const bytesPerElement = attached.reduce((sum, pool) => sum + pool.elementBytes, 0);
  const peakElements = attached.reduce((sum, pool) => sum + pool.capacity, 0);
  const bytesPerTick = attached.reduce((sum, pool) => sum + pool.elementBytes * pool.capacity, 0);

  const phaseBySystem = new Map<string, number>();
  for (const phase of zir.sim.phases) {
    for (const system of phase.systems) phaseBySystem.set(`${system.module}\0${system.name}`, phase.index);
  }
  const rates = declarationsOf(hir, 'system').map((system) => {
    const phase = phaseBySystem.get(`${system.module}\0${system.name}`);
    if (phase === undefined) throw new Error(`costs.zcost system missing from schedule: ${system.module}.${system.name}`);
    return {
      every: uint(system.every, 'rates[].every'),
      module: uint(system.module, 'rates[].module', 0xfff),
      name: system.name,
      phase: uint(phase, 'rates[].phase'),
      stagger: system.staggerPool !== null,
    };
  });

  const scenarioAsserts: CostReport['scenario_asserts'] = [];
  for (const scenario of declarationsOf(hir, 'scenario')) {
    const lines = scenario.items
      .filter((item) => item.ast.kind === 'assert_budget')
      .map((item) => (item.ast as Extract<typeof item.ast, { kind: 'assert_budget' }>).budgetSet);
    if (lines.length > 0) scenarioAsserts.push({ lines, module: scenario.module, name: scenario.name });
  }

  return {
    $schema: COST_SCHEMA,
    abi_version: abiVersion,
    budgets: [...(options.budgets ?? [])].map((budget) => ({
      line: budget.line,
      limit: uint(budget.limit, `budgets[${budget.line}].limit`),
      owner: budget.owner,
    })),
    command_memory: {
      ceiling_bytes: ceiling,
      per_frame_estimate_bytes: uint(zir.present.perFrameEstimateBytes, 'command_memory.per_frame_estimate_bytes'),
    },
    modules: [...hir.modules].sort((a, b) => a.index - b.index).map((module) => ({ index: module.index, name: module.name })),
    particle_bandwidth: {
      bytes_per_element: uint(bytesPerElement, 'particle_bandwidth.bytes_per_element'),
      peak_elements: uint(peakElements, 'particle_bandwidth.peak_elements'),
      bytes_per_tick: uint(bytesPerTick, 'particle_bandwidth.bytes_per_tick'),
      pools: attached.map((pool) => pool.name),
    },
    pools: pools.map((pool) => ({
      capacity: uint(pool.capacity, 'pools[].capacity'),
      element_bytes: uint(pool.elementBytes, 'pools[].element_bytes'),
      module: uint(pool.module, 'pools[].module', 0xfff),
      name: pool.name,
    })),
    programs,
    rates,
    scenario_asserts: scenarioAsserts,
    source_attribution: 'sourceids.zmap',
  };
}

export function emitCostReport(hir: HirProgram, zir: ZirProgram, options: CostReportOptions): Uint8Array {
  const report = buildCostReport(hir, zir, options);
  return new TextEncoder().encode(JSON.stringify(canonical(report as unknown as Json)) + '\n');
}

/** Strict canonicality and structural validator for tools and staleness checks. */
export function validateCostReport(bytes: Uint8Array): CostReport {
  let text: string;
  try {
    text = new TextDecoder('utf-8', { fatal: true }).decode(bytes);
  } catch {
    throw new Error('costs.zcost is not valid UTF-8');
  }
  if (!text.endsWith('\n') || text.endsWith('\n\n') || text.includes('\r')) throw new Error('costs.zcost must have exactly one trailing LF');
  let parsed: unknown;
  try {
    parsed = JSON.parse(text.slice(0, -1));
  } catch {
    throw new Error('costs.zcost is not valid JSON');
  }
  if (parsed === null || Array.isArray(parsed) || typeof parsed !== 'object') throw new Error('costs.zcost root must be an object');
  const canonicalText = JSON.stringify(canonical(parsed as Json)) + '\n';
  if (canonicalText !== text) throw new Error('costs.zcost is not canonical sorted-key JSON');
  assertReportShape(parsed as Record<string, unknown>);
  return parsed as unknown as CostReport;
}

function assertReportShape(report: Record<string, unknown>): void {
  const required = ['$schema', 'abi_version', 'budgets', 'command_memory', 'modules', 'particle_bandwidth', 'pools', 'programs', 'rates', 'scenario_asserts', 'source_attribution'];
  for (const key of required) if (!(key in report)) throw new Error(`costs.zcost missing '${key}'`);
  if (report.$schema !== COST_SCHEMA || report.source_attribution !== 'sourceids.zmap') throw new Error('costs.zcost sentinel mismatch');

  const object = (value: unknown, path: string): Record<string, unknown> => {
    if (value === null || Array.isArray(value) || typeof value !== 'object') throw new Error(`costs.zcost ${path} must be an object`);
    return value as Record<string, unknown>;
  };
  const array = (value: unknown, path: string): unknown[] => {
    if (!Array.isArray(value)) throw new Error(`costs.zcost ${path} must be an array`);
    return value;
  };
  const string = (value: unknown, path: string): string => {
    if (typeof value !== 'string') throw new Error(`costs.zcost ${path} must be a string`);
    return value;
  };
  const integer = (value: unknown, path: string, unsigned = true): number => {
    if (typeof value !== 'number' || !Number.isSafeInteger(value) || (unsigned && value < 0)) throw new Error(`costs.zcost ${path} must be ${unsigned ? 'an unsigned ' : 'an '}integer`);
    return value;
  };
  const bool = (value: unknown, path: string): boolean => {
    if (typeof value !== 'boolean') throw new Error(`costs.zcost ${path} must be boolean`);
    return value;
  };
  const requireKeys = (row: Record<string, unknown>, keys: readonly string[], path: string): void => {
    for (const key of keys) if (!(key in row)) throw new Error(`costs.zcost ${path} missing '${key}'`);
  };

  integer(report.abi_version, '$.abi_version');
  for (const [index, value] of array(report.budgets, '$.budgets').entries()) {
    const row = object(value, `$.budgets[${index}]`);
    requireKeys(row, ['line', 'limit', 'owner'], `$.budgets[${index}]`);
    string(row.line, `$.budgets[${index}].line`); integer(row.limit, `$.budgets[${index}].limit`); string(row.owner, `$.budgets[${index}].owner`);
  }
  const command = object(report.command_memory, '$.command_memory');
  requireKeys(command, ['ceiling_bytes', 'per_frame_estimate_bytes'], '$.command_memory');
  integer(command.ceiling_bytes, '$.command_memory.ceiling_bytes'); integer(command.per_frame_estimate_bytes, '$.command_memory.per_frame_estimate_bytes');

  for (const [index, value] of array(report.modules, '$.modules').entries()) {
    const row = object(value, `$.modules[${index}]`); requireKeys(row, ['index', 'name'], `$.modules[${index}]`);
    integer(row.index, `$.modules[${index}].index`); string(row.name, `$.modules[${index}].name`);
  }
  const bandwidth = object(report.particle_bandwidth, '$.particle_bandwidth');
  requireKeys(bandwidth, ['bytes_per_element', 'peak_elements', 'bytes_per_tick', 'pools'], '$.particle_bandwidth');
  integer(bandwidth.bytes_per_element, '$.particle_bandwidth.bytes_per_element'); integer(bandwidth.peak_elements, '$.particle_bandwidth.peak_elements'); integer(bandwidth.bytes_per_tick, '$.particle_bandwidth.bytes_per_tick');
  array(bandwidth.pools, '$.particle_bandwidth.pools').forEach((value, index) => string(value, `$.particle_bandwidth.pools[${index}]`));

  for (const [index, value] of array(report.pools, '$.pools').entries()) {
    const row = object(value, `$.pools[${index}]`); requireKeys(row, ['capacity', 'element_bytes', 'module', 'name'], `$.pools[${index}]`);
    integer(row.capacity, `$.pools[${index}].capacity`); integer(row.element_bytes, `$.pools[${index}].element_bytes`); integer(row.module, `$.pools[${index}].module`); string(row.name, `$.pools[${index}].name`);
  }
  for (const [index, value] of array(report.programs, '$.programs').entries()) {
    const path = `$.programs[${index}]`; const row = object(value, path);
    requireKeys(row, ['attr0_writes', 'class_counts', 'cycles_est', 'dsp', 'footprint_rect', 'instr_count', 'kind', 'max_ops', 'module', 'name', 'profile', 'register_hwm', 'source_id', 'table_bytes'], path);
    for (const key of ['attr0_writes', 'cycles_est', 'dsp', 'instr_count', 'max_ops', 'module', 'register_hwm', 'source_id', 'table_bytes']) integer(row[key], `${path}.${key}`);
    if (row.kind !== 'field' || (row.profile !== 'earth' && row.profile !== 'flow')) throw new Error(`costs.zcost ${path} field sentinel mismatch`);
    string(row.name, `${path}.name`);
    const classes = object(row.class_counts, `${path}.class_counts`); requireKeys(classes, ['ALU', 'MUL', 'NOISE', 'SPECIAL', 'TABLE'], `${path}.class_counts`);
    for (const key of ['ALU', 'MUL', 'NOISE', 'SPECIAL', 'TABLE']) integer(classes[key], `${path}.class_counts.${key}`);
    const footprint = array(row.footprint_rect, `${path}.footprint_rect`);
    if (footprint.length !== 4) throw new Error(`costs.zcost ${path}.footprint_rect must contain four integers`);
    footprint.forEach((item, itemIndex) => integer(item, `${path}.footprint_rect[${itemIndex}]`, false));
  }
  for (const [index, value] of array(report.rates, '$.rates').entries()) {
    const path = `$.rates[${index}]`; const row = object(value, path); requireKeys(row, ['every', 'module', 'name', 'phase', 'stagger'], path);
    integer(row.every, `${path}.every`); integer(row.module, `${path}.module`); string(row.name, `${path}.name`); integer(row.phase, `${path}.phase`); bool(row.stagger, `${path}.stagger`);
  }
  for (const [index, value] of array(report.scenario_asserts, '$.scenario_asserts').entries()) {
    const path = `$.scenario_asserts[${index}]`; const row = object(value, path); requireKeys(row, ['lines', 'module', 'name'], path);
    array(row.lines, `${path}.lines`).forEach((item, itemIndex) => string(item, `${path}.lines[${itemIndex}]`)); integer(row.module, `${path}.module`); string(row.name, `${path}.name`);
  }
}
