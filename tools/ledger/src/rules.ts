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
  FORMAL_EVIDENCE_MIN_MATURITY,
  type Block,
  type BlocksDoc,
  type FormalRunsDoc,
  type Maturity,
  type OpsDoc,
  type ProfileId,
} from './types';

export interface RuleOptions {
  /** Previously committed blocks.yml (from git); null = bootstrap (file's first appearance). */
  prevBlocks?: BlocksDoc | null;
  /** Path-existence probe (repo-root-relative). Defaults to () => true. */
  exists?: (p: string) => boolean;
  /** Repo-relative paths of every `.sby` found under tests/formal (rule V16). */
  formalTasksOnDisk?: string[];
  /** Every `.sby` with its text + staged source texts (rule V19). */
  sbyTasks?: SbyTask[];
  /** Every RTL source under fpga/rtl with its text (rule V20). */
  rtlFiles?: RtlFile[];
  /** Text reader for ENFORCED-BY symbol resolution (repo-root-relative). */
  readText?: (p: string) => string | null;
  /** Concatenated text of every source under reference/ (rule V17). */
  referenceText?: string;
}

/** One RTL source file (rule V20). */
export interface RtlFile {
  /** repo-relative path */
  path: string;
  text: string;
}

/** One `.sby` property with the texts of the sources it stages (rule V19). */
export interface SbyTask {
  /** repo-relative path of the .sby */
  path: string;
  /** raw text of the .sby */
  text: string;
  /** raw texts of the committed sources its [files] section stages (resolved; missing/generated staging paths omitted) */
  sources: string[];
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

/**
 * V16: a formal property may back a maturity claim only if the lane recorded
 * a GREEN run for it — and "the .sby exists" is not a run.
 *
 * This rule exists because the repo lost that distinction twice on the same
 * lane. MEM.GUARD's ledger said "Formally proven." while the proof failed and
 * its assertions were vacuous; MEM.VRAM.ARBITER carried RTL_VERIFIED citing a
 * liveness bound that FAILS when run. Both survived a full wave because a
 * `.sby` that had never parsed produced the same green SKIP as a machine with
 * no oss-cad-suite installed. The lane wrappers now hard-fail on anything but
 * a missing tool; this rule closes the ledger side.
 *
 * Checks:
 *   a. every `.sby` on disk is registered (adding a property without running
 *      it is a failure, not a silent SKIP);
 *   b. every registered property exists on disk;
 *   c. entries are dated, commit-pinned, and name at least one task;
 *   d. a block at >= RTL_VERIFIED citing `tests.formal` needs an entry with
 *      status `green` AND covers: true (reachable assertions, not vacuous);
 *   e. `never_ran` is a hard failure the moment anything cites it;
 *   f. `banked` may not back a maturity claim.
 *
 * `formalTasksOnDisk` is injected (like `exists`) so the rule stays pure.
 */
export function checkFormalRuns(
  blocksDoc: BlocksDoc,
  formalDoc: FormalRunsDoc,
  opts: RuleOptions = {}
): string[] {
  const exists = opts.exists ?? (() => true);
  const errors: string[] = [];
  const runs = formalDoc.runs ?? [];
  const byProperty = new Map(runs.map((r) => [r.property, r] as const));

  if (byProperty.size !== runs.length) {
    errors.push('V16: design/formal_runs.yml has duplicate property entries');
  }

  // (b) + (c): registry hygiene
  for (const r of runs) {
    if (!exists(r.property)) {
      errors.push(`V16: formal_runs entry "${r.property}" does not exist on disk`);
    }
    if (!/^\d{4}-\d{2}-\d{2}$/.test(r.date)) {
      errors.push(`V16: formal_runs "${r.property}" has invalid date "${r.date}"`);
    }
    if (!/^[0-9a-f]{7,40}$/.test(r.commit)) {
      errors.push(`V16: formal_runs "${r.property}" has invalid commit "${r.commit}"`);
    }
    if (!Array.isArray(r.tasks) || r.tasks.length === 0) {
      errors.push(`V16: formal_runs "${r.property}" records no tasks — a run with no tasks is not a run`);
    }
    if (r.status === 'green' && r.covers && !(r.tasks ?? []).some((t) => /cover/.test(t))) {
      errors.push(
        `V16: formal_runs "${r.property}" claims covers: true but no task name contains "cover" (${(r.tasks ?? []).join(', ')})`
      );
    }
  }

  // (a): every .sby on disk is registered
  for (const p of opts.formalTasksOnDisk ?? []) {
    if (!byProperty.has(p)) {
      errors.push(
        `V16: ${p} exists but is not in design/formal_runs.yml — a formal property that has never been recorded as run is a HARD FAILURE, not a SKIP`
      );
    }
  }

  // (d)(e)(f): maturity claims
  const minRank = rank(FORMAL_EVIDENCE_MIN_MATURITY);
  for (const b of blocksDoc.blocks) {
    const prop = b.tests?.formal;
    if (!prop) continue;
    const entry = byProperty.get(prop);
    const loadBearing = rank(b.maturity) >= minRank;
    if (!entry) {
      // A SPECIFIED block may cite a property that does not exist yet (V6
      // gates path existence the same way) — but only if it truly is absent.
      if (exists(prop)) {
        errors.push(
          `V16: ${b.id} cites formal "${prop}" which exists on disk but has no design/formal_runs.yml entry`
        );
      } else if (loadBearing) {
        errors.push(`V16: ${b.id} is ${b.maturity} but its formal property "${prop}" does not exist`);
      }
      continue;
    }
    if (entry.status === 'never_ran') {
      errors.push(
        `V16: ${b.id} cites formal "${prop}" whose recorded status is never_ran — it has never successfully elaborated`
      );
      continue;
    }
    if (!loadBearing) continue;
    if (entry.status !== 'green') {
      errors.push(
        `V16: ${b.id} is ${b.maturity} but formal "${prop}" is recorded as "${entry.status}", not green — banked evidence cannot back a maturity claim`
      );
    } else if (!entry.covers) {
      errors.push(
        `V16: ${b.id} is ${b.maturity} citing formal "${prop}" with covers: false — a proof with no cover task has not shown its assertions are reachable (the MEM.GUARD vacuity failure)`
      );
    }
  }

  return errors;
}

/**
 * V19: every BOUNDED formal proof carries a self-asserting scope guard.
 *
 * A bounded (bmc) proof states its laws only up to its depth, and the depth's
 * MEANING rests on structural facts of the harness (a refresh-free horizon, a
 * frame-constant map, a shrunk buffer, an abstract raster period). Twice this
 * repo watched a bounded number quietly outlive the facts that made it true
 * (the frozen B = 40; the "both gates in the cone" harness header). The cure
 * that worked is the arbiter's `a_horizon_is_refresh_free` / the linebuf's
 * `a_scope_four_sessions`: an assertion IN THE PROOF'S OWN CONE that states
 * the scope and FIRES if anyone raises the depth past what was actually
 * proven — silent scope drift becomes a red run.
 *
 * Rule: a `.sby` with a `mode bmc` task must have an `a_scope_*` or
 * `a_horizon_*` assertion somewhere in its staged sources (or the .sby
 * itself), or carry an explicit `# SCOPE-TOTAL: <reason>` header waiver
 * asserting the bound covers the full reachable state space. Unbounded
 * (`mode prove`) tasks are exempt — their scope IS total.
 */
export function checkScopeGuards(sbys: SbyTask[]): string[] {
  const errors: string[] = [];
  for (const s of sbys) {
    const bounded = /^\s*(?:[\w-]+\s*:\s*)?mode\s+bmc\b/m.test(s.text);
    if (!bounded) continue;
    const waived = /SCOPE-TOTAL:/.test(s.text);
    const guarded = [s.text, ...s.sources].some((t) => /\ba_(?:scope|horizon)_\w+\s*:/.test(t));
    if (!guarded && !waived) {
      errors.push(
        `V19: ${s.path} has a bounded (mode bmc) task but no self-asserting scope guard — ` +
        'a bounded proof must carry an a_scope_* / a_horizon_* assertion in its cone that FIRES ' +
        'if the depth is raised past what was proven (the a_horizon_is_refresh_free pattern), ' +
        'or an explicit "# SCOPE-TOTAL: <reason>" waiver in the .sby'
      );
    }
  }
  return errors;
}

/**
 * V17: citation coherence — cited symbols must be DEFINED, contract-cited
 * artifacts must EXIST, and the cited test must actually be ABOUT its oracle.
 *
 * The phantom-pointer incident this closes (W2.6): `zref::CmdDma` and
 * `zref::Crc32c` were cited as reference_model with NO such symbol defined
 * anywhere; `zref::framePixelCrc` plus FOUR test files were cited in
 * design/contracts/*.md — free text V6 never reads — and none existed.
 * V6 already existence-gates the `blocks.yml` test paths, so this rule
 * deliberately does NOT re-check those (charter §29-6: one enforcer per
 * semantic); it checks the surfaces V6 cannot see:
 *
 *   a. a block at >= REFERENCE_COMPLETE citing `reference_model` must have
 *      that symbol's last segment DEFINED under reference/ (class/struct
 *      declaration or function definition hit);
 *   b. the contract's "## Scalar reference function" backticked zref symbol
 *      must EQUAL the ledger's reference_model (drift between contract and
 *      ledger is how zref::framePixelCrc survived; zref::VideoSys was
 *      renamed to Scanout for exactly this — a class by any other name is a
 *      phantom citation);
 *   c. every `tests/...` path cited in the contract of a block past
 *      SPECIFIED must exist on disk (the four W2.6 phantom files);
 *   d. ANTI-ALIAS TIE: the files at tests.directed / tests.random of an rtl
 *      block past SPECIFIED must textually reference the reference_model's
 *      last segment. Existence alone is satisfiable by a file about
 *      something else — MEM.HPS.BRIDGE's tests.random pointed at a real
 *      file that never instantiated the bridge. This is the part with teeth.
 */
export function checkCitations(blocksDoc: BlocksDoc, opts: RuleOptions = {}): string[] {
  const exists = opts.exists ?? (() => true);
  const readText = opts.readText ?? (() => null);
  const refText = opts.referenceText ?? null;
  const errors: string[] = [];
  const refComplete = rank('REFERENCE_COMPLETE');

  for (const b of blocksDoc.blocks) {
    const lastSeg = b.reference_model ? b.reference_model.split('::').pop()! : null;

    // (a) cited oracle symbol is defined under reference/
    if (refText !== null && lastSeg && rank(b.maturity) >= refComplete) {
      const esc = lastSeg.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
      const defined = new RegExp(`(class|struct)\\s+${esc}\\b|\\b${esc}\\s*\\(`).test(refText);
      if (!defined) {
        errors.push(
          `V17: ${b.id} is ${b.maturity} citing reference_model "${b.reference_model}" but no definition of ` +
          `"${lastSeg}" exists under reference/ — a symbol nobody defined is a phantom citation (the zref::CmdDma failure)`
        );
      }
    }

    // (b)+(c): contract coherence
    const contractText = b.contract ? readText(b.contract) : null;
    if (contractText !== null) {
      const section = /##\s*Scalar reference function\s*\n([\s\S]*?)(?=\n##\s|$)/.exec(contractText);
      const cited = section ? /`(zref::[A-Za-z0-9_:]+)`/.exec(section[1]) : null;
      if (cited && b.reference_model && cited[1] !== b.reference_model) {
        errors.push(
          `V17: ${b.id} contract ${b.contract} cites scalar reference "${cited[1]}" but the ledger says ` +
          `"${b.reference_model}" — contract and ledger have drifted (the zref::framePixelCrc failure)`
        );
      }
      if (!cited && b.kind === 'rtl' && rank(b.maturity) >= refComplete) {
        errors.push(
          `V17: ${b.id} is ${b.maturity} but its contract ${b.contract} names no backticked zref:: symbol ` +
          'under "## Scalar reference function"'
        );
      }
      if (b.maturity !== 'SPECIFIED') {
        const seen = new Set<string>();
        // lookbehind: "tests/..." must start the path — a workspace-rooted
        // citation like compiler/tests/foo.test.ts is NOT a repo-root
        // tests/ path (first real run of this rule false-positived on
        // exactly that; the fix is the rule's, not the contract's)
        const pathRe = /(?<![A-Za-z0-9_/.-])tests\/[A-Za-z0-9_\-./]*[A-Za-z0-9_]\.[a-z]{1,4}\b/g;
        for (const m of contractText.matchAll(pathRe)) {
          const p = m[0];
          if (seen.has(p)) continue;
          seen.add(p);
          if (!exists(p)) {
            errors.push(
              `V17: ${b.id} (${b.maturity}) contract ${b.contract} cites "${p}" which does not exist — ` +
              'a contract-cited test that was never written is a phantom citation (the W2.6 four-file failure)'
            );
          }
        }
      }
    }

    // (d) anti-alias tie: the cited test must be ABOUT the cited oracle
    if (b.kind === 'rtl' && lastSeg && b.maturity !== 'SPECIFIED') {
      for (const p of [b.tests?.directed, b.tests?.random]) {
        if (!p || !exists(p)) continue; // V6 owns existence
        let text = readText(p);
        // FOLLOW LOCAL INCLUDES ONE LEVEL.
        //
        // The dominant shape in this tree is a shared driver header beside the
        // test -- velocity_dev.hpp, surface_dev.hpp, raster_dev.hpp,
        // geom_dev.hpp, governor_dev.hpp, cmd_sim.hpp -- which owns the cycle
        // contract and CALLS the oracle, so the test file itself never names
        // it. Reading only the named file therefore reported three blocks as
        // citing an "alias" when the differential was real and passing:
        // TERRAIN.VELOCITY (velocity_dev.hpp:227 calls velocity_vertex),
        // SURFACE.SHEET and RASTER.EDGEWALK.
        //
        // One level only, and only siblings. The rule exists to catch a test
        // that is not about its oracle at all; chasing includes arbitrarily far
        // would eventually find any symbol anywhere and the check would stop
        // meaning anything.
        if (text !== null && !text.includes(lastSeg)) {
          const dir = p.slice(0, Math.max(p.lastIndexOf('/'), 0));
          for (const m of text.matchAll(/#include\s+"([A-Za-z0-9_.\-]+)"/g)) {
            const sib = dir ? `${dir}/${m[1]}` : m[1];
            if (!exists(sib)) continue;
            const t2 = readText(sib);
            if (t2 !== null && t2.includes(lastSeg)) { text = text + t2; break; }
          }
        }
        if (text !== null && !text.includes(lastSeg)) {
          errors.push(
            `V17: ${b.id} (${b.maturity}) test "${p}" never mentions its oracle "${lastSeg}" — ` +
            'an existing file that is not about the cited reference model is an alias, not evidence (the MEM.HPS.BRIDGE failure)'
          );
        }
      }
    }
  }
  return errors;
}

/**
 * V20: a prose invariant claim must name its enforcer, machine-resolvably.
 *
 * Both false claims this project has found in RTL prose — "validated
 * upstream" on a mode byte nothing validated (the ZHAO_TIMING OOB index)
 * and "toggle-free by construction" false for the FULL case (a real CDC
 * hazard) — were found by asking one question: WHO ENFORCES THIS SENTENCE?
 * This rule asks it mechanically, forever. Any RTL comment line matching a
 * claim phrase (`by construction`, `by-construction`, wrapped across a
 * comment line break, `validated upstream`, `guaranteed by`, `cannot
 * happen`, `never occurs`) must be followed within WINDOW lines by
 *
 *     ENFORCED-BY: <repo-relative path>[:<symbol>]
 *
 * and the pointer must RESOLVE: the path exists; a `.sby` path must be a
 * property registered in design/formal_runs.yml (transitivity with V16 —
 * an enforcer that never ran is not an enforcer); a `:symbol` suffix must
 * occur in the named file's text. A claim that is really an ASSUMPTION on
 * another block is not annotated — it is REWRITTEN as an explicit
 * assumption naming who upholds it (and then no longer matches a claim
 * phrase), because pointing prose at prose enforces nothing.
 */
const V20_CLAIM = /(by[ -]construction|validated upstream|guaranteed by|cannot happen|never occurs)/i;
const V20_WINDOW = 10;
const V20_ANNOTATION = /ENFORCED-BY:\s*([^\s`")\]]+)/;

export function checkProseClaims(
  files: RtlFile[],
  opts: RuleOptions = {},
  formalDoc?: FormalRunsDoc
): string[] {
  const exists = opts.exists ?? (() => true);
  const readText = opts.readText ?? (() => null);
  const registered = new Set((formalDoc?.runs ?? []).map((r) => r.property));
  const errors: string[] = [];

  for (const f of files) {
    const lines = f.text.split(/\r?\n/);
    // A line is a claim if it matches alone, or joined with the start of the
    // next comment line (the wrapped "by\n//   construction" case).
    const isClaim = (i: number): boolean => {
      if (V20_CLAIM.test(lines[i])) return true;
      if (i + 1 < lines.length) {
        const next = lines[i + 1].replace(/^\s*(\/\/|\*)?\s*/, '');
        return V20_CLAIM.test(`${lines[i]} ${next.slice(0, 24)}`) && !V20_CLAIM.test(` ${next.slice(0, 24)}`);
      }
      return false;
    };

    for (let i = 0; i < lines.length; i++) {
      // every annotation must resolve, wherever it appears
      const ann = V20_ANNOTATION.exec(lines[i]);
      if (ann) {
        const ref = ann[1];
        const colon = ref.indexOf(':');
        const p = colon >= 0 ? ref.slice(0, colon) : ref;
        const sym = colon >= 0 ? ref.slice(colon + 1) : null;
        if (!exists(p)) {
          errors.push(`V20: ${f.path}:${i + 1} ENFORCED-BY "${ref}" — path "${p}" does not exist`);
        } else if (p.endsWith('.sby') && !registered.has(p)) {
          errors.push(
            `V20: ${f.path}:${i + 1} ENFORCED-BY "${ref}" — "${p}" is not registered in design/formal_runs.yml (an enforcer that never ran is not an enforcer)`
          );
        } else if (sym) {
          // a `.sby:symbol` resolves against the property's whole cone (the
          // .sby text plus its staged sources — assertions live in harness
          // or DUT files, not in the .sby itself)
          let hay: string | null = null;
          if (p.endsWith('.sby')) {
            const task = (opts.sbyTasks ?? []).find((s) => s.path === p);
            if (task) hay = [task.text, ...task.sources].join('\n');
          }
          if (hay === null) hay = readText(p);
          if (hay === null || !hay.includes(sym)) {
            errors.push(`V20: ${f.path}:${i + 1} ENFORCED-BY "${ref}" — symbol "${sym}" not found in ${p} (or its staged cone)`);
          }
        }
      }

      if (!isClaim(i)) continue;
      let annotated = false;
      for (let j = i; j <= Math.min(i + V20_WINDOW, lines.length - 1); j++) {
        if (V20_ANNOTATION.test(lines[j])) {
          annotated = true;
          break;
        }
      }
      if (!annotated) {
        errors.push(
          `V20: ${f.path}:${i + 1} states an invariant claim with no ENFORCED-BY: <path[:symbol]> within ${V20_WINDOW} lines — ` +
          'name the enforcer machine-resolvably, add the missing enforcement, or rewrite the claim as an explicit assumption naming who upholds it'
        );
      }
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
    // field-ir.md 1: op semantics live in exactly TWO places, the C++
    // interpreter and the TS one. Most ops have no per-op function to name, so
    // `zfield::interpret` -- the one interpreter -- is a legal answer here.
    // Demanding a per-op symbol is what made this file name functions that
    // resolve to nothing.
    if (!/^(zref::fieldir::[a-z0-9_]+|zfield::interpret)$/.test(op.reference_function)) {
      errors.push(`V10: op ${op.id} reference_function "${op.reference_function}" is neither a zref::fieldir:: symbol nor zfield::interpret`);
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
  opts: RuleOptions = {},
  formalDoc?: FormalRunsDoc
): string[] {
  return [
    ...checkBlocks(blocksDoc, opts),
    ...checkOps(opsDoc, blocksDoc, opts),
    ...(formalDoc ? checkFormalRuns(blocksDoc, formalDoc, opts) : []),
    ...checkCitations(blocksDoc, opts),
    ...(opts.sbyTasks ? checkScopeGuards(opts.sbyTasks) : []),
    ...(opts.rtlFiles ? checkProseClaims(opts.rtlFiles, opts, formalDoc) : []),
  ];
}
