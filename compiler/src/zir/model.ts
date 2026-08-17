// model.ts — domain-partitioned ZIR (W3.3, D3/D6).

import type {
  HirEmit, HirExpr, HirField, HirGlobal, HirPool, HirScenario, HirStmt,
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

export interface TestZir {
  kind: 'TestZIR';
  scenarios: HirScenario[];
}

export interface ZirProgram {
  sim: SimZir;
  present: PresentZir;
  test: TestZir;
}
