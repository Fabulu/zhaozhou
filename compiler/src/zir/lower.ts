// lower.ts — HIR partition + deterministic phase scheduler (W3.3, D3/D6).
// Runtime receives a flat call list; all phase choices happen here.

import {
  declarationsOf, serializeHir, type HirDeclaration, type HirProgram, type HirSystem,
} from '../hir/model.js';
import type {
  PresentZir, SimZir, TestZir, TestZirOperation, ZirProgram, ZirSystem,
} from './model.js';

const COMMAND_BYTES: Readonly<Record<string, number>> = {
  draw_form: 32,
  draw_population: 32,
  draw_procedural: 64,
  surface_stamp: 64,
  audio: 32,
};

export function lowerZir(hir: HirProgram): ZirProgram {
  const sim = lowerSim(hir);
  const present = lowerPresent(hir);
  const test = lowerTest(hir);
  return { sim, present, test };
}

function lowerTest(hir: HirProgram): TestZir {
  return {
    kind: 'TestZIR',
    scenarios: declarationsOf(hir, 'scenario').map((scenario) => ({
      module: scenario.module,
      name: scenario.name,
      sourceId: scenario.sourceId,
      operations: scenario.items.map((item): TestZirOperation => {
        switch (item.ast.kind) {
          case 'seed':
            return { kind: 'seed', value: u32(item.ast.value, 'scenario seed'), span: item.span };
          case 'load': {
            const target = item.ast.target;
            const module = hir.modules.find((candidate) => candidate.name === target);
            if (!module) throw new Error(`internal TestZIR load target '${target}' is unresolved`);
            return { kind: 'load', module: module.index, name: module.name, span: item.span };
          }
          case 'spawn_player': {
            const placement = resolveDeclaration(hir, scenario.module, item.ast.at, 'global');
            return {
              kind: 'spawn_player', player: u32(item.ast.index, 'scenario player'),
              placement: { module: placement.module, global: placement.name }, span: item.span,
            };
          }
          case 'at': {
            const action = item.expressions[0];
            if (!action || action.symbol?.kind !== 'system' || action.symbol.module === null) {
              throw new Error(`internal TestZIR action at ${item.ast.tick} has no resolved system`);
            }
            const system = declarationsOf(hir, 'system').find(
              (candidate) => candidate.module === action.symbol!.module && candidate.name === action.symbol!.name,
            );
            if (!system) throw new Error(`internal TestZIR system '${action.symbol.name}' is missing`);
            return {
              kind: 'at', tick: u32(item.ast.tick, 'scenario action tick'),
              system: { module: system.module, name: system.name, sourceId: system.sourceId }, span: item.span,
            };
          }
          case 'assert': {
            const expression = item.expressions[0];
            if (!expression) throw new Error('internal TestZIR assertion has no expression');
            return { kind: 'assert', expression, tolerance: item.expressions[1] ?? null, span: item.span };
          }
          case 'capture':
            return { kind: 'capture', frame: u32(item.ast.frame, 'scenario capture frame'), name: item.ast.name, span: item.span };
          case 'assert_budget': {
            const presentation = resolveDeclaration(hir, scenario.module, item.ast.budgetSet, 'presentation');
            return { kind: 'assert_budget', presentation: { module: presentation.module, name: presentation.name }, span: item.span };
          }
        }
      }),
      span: scenario.span,
    })),
  };
}

function resolveDeclaration<K extends HirDeclaration['kind']>(
  hir: HirProgram,
  requester: number,
  authored: string,
  kind: K,
): Extract<HirDeclaration, { kind: K }> {
  const parts = authored.split('.');
  let owner = requester;
  let name = authored;
  if (parts.length === 2) {
    const module = hir.modules.find((candidate) => candidate.name === parts[0]);
    const imported = module && (module.index === requester
      || hir.modules[requester]!.imports.some((item) => item.module === module.index && item.names.length === 0));
    if (!module || !imported) throw new Error(`internal TestZIR qualified name '${authored}' is not visible`);
    owner = module.index;
    name = parts[1]!;
  } else if (parts.length === 1) {
    const local = hir.declarations.find((declaration) => declaration.module === requester && declaration.name === name);
    if (!local) {
      const imports = hir.modules[requester]!.imports.filter((item) => item.names.includes(name));
      const found = imports.map((item) => hir.declarations.find(
        (declaration) => declaration.module === item.module && declaration.name === name,
      )).filter((value): value is HirDeclaration => value !== undefined);
      if (found.length !== 1) throw new Error(`internal TestZIR name '${authored}' is unresolved or ambiguous`);
      owner = found[0]!.module;
    }
  } else {
    throw new Error(`internal TestZIR malformed declaration path '${authored}'`);
  }
  const declaration = hir.declarations.find(
    (candidate): candidate is Extract<HirDeclaration, { kind: K }> =>
      candidate.module === owner && candidate.name === name && candidate.kind === kind,
  );
  if (!declaration) throw new Error(`internal TestZIR '${authored}' is not a ${kind}`);
  return declaration;
}

function u32(value: bigint, label: string): number {
  if (value < 0n || value > 0xffffffffn) throw new Error(`internal TestZIR ${label} ${value} is outside u32`);
  return Number(value);
}

/** Pure function of the checked phase assignment plus canonical declaration order. */
export function buildPhaseSchedule(hir: HirProgram): SimZir['phases'] {
  const systems = declarationsOf(hir, 'system');
  const canonicalOrder = new Map<string, number>();
  systems.forEach((system, index) => canonicalOrder.set(key(system.module, system.name), index));
  const byKey = new Map(systems.map((system) => [key(system.module, system.name), system] as const));
  const moduleIndex = new Map(hir.modules.map((module) => [module.name, module.index] as const));
  const seen = new Set<string>();
  const phases: SimZir['phases'] = [];
  for (const frontendPhase of [...hir.schedule.phases].sort((a, b) => a.index - b.index)) {
    const phaseSystems: HirSystem[] = [];
    for (const item of frontendPhase.systems) {
      const module = moduleIndex.get(item.module);
      if (module === undefined) throw new Error(`internal ZIR schedule names unknown module '${item.module}'`);
      const id = key(module, item.name);
      const system = byKey.get(id);
      if (!system) throw new Error(`internal ZIR schedule names unknown system '${item.module}.${item.name}'`);
      if (seen.has(id)) throw new Error(`internal ZIR schedule repeats '${item.module}.${item.name}'`);
      seen.add(id);
      phaseSystems.push(system);
    }
    phaseSystems.sort((a, b) => canonicalOrder.get(key(a.module, a.name))! - canonicalOrder.get(key(b.module, b.name))!);
    phases.push({
      index: frontendPhase.index,
      systems: phaseSystems.map((system) => scheduleSystem(system, frontendPhase.index, canonicalOrder.get(key(system.module, system.name))!)),
    });
  }
  if (seen.size !== systems.length) {
    const missing = systems.filter((system) => !seen.has(key(system.module, system.name))).map((system) => `${system.module}.${system.name}`);
    throw new Error(`internal ZIR schedule omitted systems: ${missing.join(', ')}`);
  }
  return phases;
}

function lowerSim(hir: HirProgram): SimZir {
  const phases = buildPhaseSchedule(hir);
  return {
    kind: 'SimZIR',
    globals: declarationsOf(hir, 'global'),
    pools: declarationsOf(hir, 'pool'),
    fields: declarationsOf(hir, 'field'),
    phases,
    callList: phases.flatMap((phase) => phase.systems),
    rngStateCount: hir.rngSlotCount,
  };
}

function lowerPresent(hir: HirProgram): PresentZir {
  const layouts: PresentZir['layouts'] = [];
  const templates: PresentZir['templates'] = [];
  for (const presentation of declarationsOf(hir, 'presentation')) {
    if (presentation.views.length > 0 || presentation.sharedBudgetPct > 0) {
      layouts.push({
        module: presentation.module,
        presentation: presentation.name,
        views: presentation.views.map((view) => {
          if (!view.camera) throw new Error(`internal ZIR presentation '${presentation.name}' has an unbound view`);
          return { id: view.id, camera: view.camera, budgetPct: view.budgetPct, recordBytes: 96 as const };
        }),
        sharedBudgetPct: presentation.sharedBudgetPct,
        contractRecordBytes: 48,
      });
    }
    let ordinal = 0;
    for (const command of presentation.emits) {
      const recordBytes = COMMAND_BYTES[command.emitKind];
      if (recordBytes === undefined) throw new Error(`internal ZIR emit kind '${command.emitKind}' has no ABI record size`);
      templates.push({ module: presentation.module, presentation: presentation.name, ordinal: ordinal++, command, recordBytes });
    }
  }
  const layoutBytes = layouts.reduce(
    (sum, layout) => sum + layout.contractRecordBytes + layout.views.reduce((viewSum, view) => viewSum + view.recordBytes, 0),
    0,
  );
  return {
    kind: 'PresentZIR',
    layouts,
    templates,
    perFrameEstimateBytes: layoutBytes + templates.reduce((sum, template) => sum + template.recordBytes, 0),
  };
}

function scheduleSystem(system: HirSystem, phase: number, declarationOrder: number): ZirSystem {
  return {
    module: system.module,
    name: system.name,
    phase,
    declarationOrder,
    every: system.every,
    rateGuard: system.staggerPool || system.every === 1
      ? null
      : { divisor: system.every, remainder: 0 },
    stagger: system.staggerPool ? {
      module: system.staggerPool.module,
      pool: system.staggerPool.name,
      divisor: system.staggerRate ?? system.every,
    } : null,
    reads: [...system.reads],
    writes: [...system.writes],
    body: system.body,
    sourceId: system.sourceId,
    span: system.span,
  };
}

function key(module: number, name: string): string { return `${module}\0${name}`; }

export function serializeZir(program: ZirProgram): string { return serializeHir(program); }
