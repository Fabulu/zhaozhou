// model.ts — domain-partitioned ZIR (W3.3, D3/D6).

import type {
  HirEmit, HirExpr, HirField, HirGlobal, HirPool, HirStmt,
} from '../hir/model.js';
import type { SourceSpan } from '../frontend/span.js';

export interface ZirSystem {
  module: number;
  name: string;
  phase: number;
  declarationOrder: number;
  every: number;
  rateGuard: { divisor: number; remainder: 0 } | null;
  stagger: { module: number; pool: string; divisor: number } | null;
  reads: string[];
  writes: string[];
  body: HirStmt[];
  sourceId: number;
  span: SourceSpan;
}

export interface SimZir {
  kind: 'SimZIR';
  globals: HirGlobal[];
  pools: HirPool[];
  fields: HirField[];
  phases: { index: number; systems: ZirSystem[] }[];
  /** Exact emitted order: phase, then canonical declaration order. */
  callList: ZirSystem[];
  rngStateCount: number;
}

export interface ZirCommandTemplate {
  module: number;
  presentation: string;
  ordinal: number;
  command: HirEmit;
  recordBytes: number;
}

export interface ZirViewLayout {
  module: number;
  presentation: string;
  views: {
    id: number;
    camera: HirExpr;
    budgetPct: number;
    recordBytes: 96;
  }[];
  sharedBudgetPct: number;
  contractRecordBytes: 48;
}

export interface PresentZir {
  kind: 'PresentZIR';
  layouts: ZirViewLayout[];
  templates: ZirCommandTemplate[];
  perFrameEstimateBytes: number;
}

export type TestZirOperation =
  | { kind: 'seed'; value: number; span: SourceSpan }
  | { kind: 'load'; module: number; name: string; span: SourceSpan }
  | { kind: 'spawn_player'; player: number; placement: { module: number; global: string }; span: SourceSpan }
  | { kind: 'at'; tick: number; system: { module: number; name: string; sourceId: number }; span: SourceSpan }
  | { kind: 'assert'; expression: HirExpr; tolerance: HirExpr | null; span: SourceSpan }
  | { kind: 'capture'; frame: number; name: string; span: SourceSpan }
  | { kind: 'assert_budget'; presentation: { module: number; name: string }; span: SourceSpan };

export interface TestZirScenario {
  module: number;
  name: string;
  sourceId: number;
  operations: TestZirOperation[];
  span: SourceSpan;
}

export interface TestZir {
  kind: 'TestZIR';
  scenarios: TestZirScenario[];
}

export interface ZirProgram {
  sim: SimZir;
  present: PresentZir;
  test: TestZir;
}
