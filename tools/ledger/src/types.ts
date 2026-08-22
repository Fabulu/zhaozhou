/**
 * Zhaozhou design ledger types (charter §4; tools/ledger, W2).
 * Spec source: RUN-20260814-1852-wave1-design-ledger FINDINGS §3 + plan W2.
 */

export type Maturity =
  | 'SPECIFIED'
  | 'REFERENCE_COMPLETE'
  | 'UNIT_VERIFIED'
  | 'RTL_VERIFIED'
  | 'SYNTHESIZED'
  | 'INTEGRATED'
  | 'HARDWARE_PROVEN';

/** Charter §4: the single frozen ladder. Order = advancement order. */
export const MATURITY_LADDER: readonly Maturity[] = [
  'SPECIFIED',
  'REFERENCE_COMPLETE',
  'UNIT_VERIFIED',
  'RTL_VERIFIED',
  'SYNTHESIZED',
  'INTEGRATED',
  'HARDWARE_PROVEN',
] as const;

/** Software blocks stop at INTEGRATED unless runs_on_target_hardware (rule V13). */
export const SOFTWARE_MAX_MATURITY: Maturity = 'INTEGRATED';

export interface MaturityLogEntry {
  state: Maturity;
  date: string;
  commit: string;
  evidence: string;
  regression_reason?: string;
}

export interface BlockTests {
  directed?: string;
  random?: string;
  formal?: string;
  unit?: string;
  differential?: string;
}

export interface Block {
  id: string;
  name: string;
  /**
   * `profile` is NOT a third kind of hardware. It is a block-shaped record for
   * a CONFIGURATION of another block, and it exists because five entries in
   * this ledger described one engine wearing five hats.
   *
   * The five FIELD.SEQ.* profiles (EARTH/WARP/FLOW/FORMATION/STAMP) each
   * carried `kind: rtl`, which under V4 demanded five reference models and ten
   * test files, and under V5 booked FIVE ENGINES' WORTH OF ALM BUDGET for one
   * engine. Nothing in the RTL ever distinguished them: `zhao_field_seq` has no
   * profile input and no profile-specific port, and the thing that would
   * distinguish them -- which registers the input and output lanes bind to --
   * is carried by the DECODED PROGRAM, not by the block.
   *
   * Owner ruling 2026-08-22: one engine, five profiles. A `profile` block must
   * name `implemented_by`, carries no evidence obligations of its own, and
   * books no area -- the engine it names carries both.
   */
  kind: 'rtl' | 'software' | 'profile';
  subsystem: string;
  clock_domain: 'hps' | 'gpu' | 'sdram' | 'video' | 'audio' | 'async';
  purpose: string;
  contract: string;
  phase: number;
  owner_issue: string;
  inputs: string[];
  outputs: string[];
  upstream: string[];
  downstream: string[];
  backpressure: 'ready_valid' | 'credit' | 'none';
  latency: string;
  target_throughput: string;
  /**
   * WHAT KIND OF EVIDENCE THIS BLOCK CAN HAVE.
   *
   * `scalar_reference` (the default, and what every block assumed): the block
   * computes a function from inputs to outputs, so a C++ reference is the
   * oracle and a differential is the evidence.
   *
   * `formal_timing`: it does not. A PLL has no scalar model; a reset
   * sequencer's correctness is a timing and sequencing property. Naming a C++
   * function for them was a category error at specification time, recorded in
   * reports/PHANTOM_REFERENCES.md as kind 3, which also predicted this would
   * need fixing in the ledger BEFORE those blocks came up. They are phase 0,
   * so it is overdue: SYS.PLL, SYS.RESET and SYS.CDC each carried THREE
   * fictions -- a phantom `reference_model` plus two `tests/platform/*.cpp`
   * paths that have never existed -- purely to satisfy V4.
   *
   * A `formal_timing` block declares a formal property instead, and rule V22
   * FORBIDS it from declaring the scalar-reference fields at all, so the
   * exemption cannot be used to make an ordinary block's obligations quiet.
   */
  evidence_kind?: 'scalar_reference' | 'formal_timing';
  reference_model?: string;
  implementation?: string;
  tests?: BlockTests;
  counters?: string[];
  memory?: string[];
  source_ids?: boolean;
  async_bridge?: boolean;
  budget_group?: string;
  resource_budget?: { alm_percent?: number; dsp_percent?: number; m10k_percent?: number };
  resource_actual?: { source_report?: string; alm?: number; dsp?: number; m10k?: number; fmax_mhz?: number };
  maturity: Maturity;
  maturity_log: MaturityLogEntry[];
  blocked_on?: 'hardware';
  deferred?: boolean;
  cut_order?: number | null;
  superseded_by?: string | null;
  /** `profile` blocks only: the block that actually implements this. */
  implemented_by?: string | null;
  leaf?: boolean;
  runs_on_target_hardware?: boolean;
  notes?: string | null;
}

export interface BlocksDoc {
  schema_version: number;
  counter_catalog: string[];
  blocks: Block[];
}

/**
 * design/formal_runs.yml — the formal-lane run registry (rule V16).
 *
 * A `.sby` file existing on disk is NOT evidence: twice in wave 2 a block's
 * maturity rested on a property that had never been elaborated, because the
 * lane could not distinguish "skipped, tool absent" from "never ran". This
 * registry makes the distinction explicit and machine-checkable.
 */
export type FormalRunStatus = 'green' | 'banked' | 'never_ran';

export interface FormalRunEntry {
  /** repo-relative path to the .sby */
  property: string;
  status: FormalRunStatus;
  date: string;
  commit: string;
  /** CTest test name that runs it, or null when deliberately not in the lane. */
  lane?: string | null;
  tasks: string[];
  /** at least one of `tasks` is a cover task (assertions proven reachable) */
  covers: boolean;
  notes?: string | null;
}

export interface FormalRunsDoc {
  version: number;
  runs: FormalRunEntry[];
}

/**
 * Maturity at or above which a cited formal property must be `green` AND
 * carry cover statements. Below this a block may cite a property it has not
 * yet finished proving; at or above it, the claim is load-bearing.
 */
export const FORMAL_EVIDENCE_MIN_MATURITY: Maturity = 'RTL_VERIFIED';

export type ProfileId = 'E' | 'W' | 'F' | 'M' | 'S';

export interface OpProfile {
  name: string;
  description: string;
  sequencer: string;
}

export interface Op {
  id: string;
  name: string;
  class: 'alu' | 'table' | 'sink' | 'stamp_mode';
  profiles: ProfileId[];
  semantics: string;
  operand_q: string[];
  result_q: string;
  rounding: 'saturating' | 'wrapping';
  reference_function: string;
  implementation_blocks: string[];
  cost_units: number;
  field_ir_opcode?: string;
  field_ir_macro?: string[];
  differential_tests: string;
  notes?: string | null;
}

export interface OpsDoc {
  schema_version: number;
  profiles: Record<ProfileId, OpProfile>;
  ops: Op[];
}

/** §25 budget-table ceilings (percent of fabric). */
export const BUDGET_CEILINGS: Readonly<Record<string, number>> = {
  platform: 14,
  command_debug: 5,
  field: 6,
  geometry_mantle: 20,
  tile: 30,
  myriad_forge: 9,
  twod_post: 6,
  reserve: 10,
} as const;

export const TOTAL_ALM_CEILING_PERCENT = 90; // §25/V5: 10% untouchable reserve
