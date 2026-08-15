// negative_corpus.ts — the W3.2 negative corpus: one refusal per FORM-E code
// (plan W3.2 acceptance: every error code exercised, each test asserting the
// exact code). Sources are whole modules (or module sets for the import
// cases); the corpus test asserts the FIRST error diagnostic carries the
// expected code.
//
// Documented exemptions (cannot be raised by the frontend — see the W3.2
// report spec-issue notes): FORM-E-668 (no L1 table declaration grammar),
// FORM-E-821/822 (deterministic runtime aborts), FORM-E-830/831 (pack-time
// page-id resolution). FORM-E-832 is generated programmatically (65537
// declarations).

export type CaseSource = string | Record<string, string> | Uint8Array;
export interface NegCase {
  code: string;
  name: string;
  src: CaseSource | (() => CaseSource);
}

const P = (body: string): string => `module m {\n${body}\n}\n`;

/** Shared prelude: a pool + struct + globals for effect tests. */
const PRE = `  struct particle { position: world3; velocity: velocity3; age: u32; }
  struct plain { x: fx16; }
  struct tuning { gain: fx16; }
  pool motes: particle[8];
  pool pebbles: plain[2];
  global energy: fx16 = 0m;
  global charge: fx16 = 0m;
  global step: u32 = 1;
  global origin: world3 = world3 { x = 0w, y = 0w, z = 0w };
  enum hue { red, green = 4, blue }
`;
const S = (body: string): string => P(PRE + body);

const EARTH_OK = `  @earth field lift_ground() -> terrain_delta
    footprint circle(0m, 0m, 4m);
    max_ops 16
  {
    let d = dist(sample.x, sample.z, 0m, 0m);
    return terrain_delta { height = d, velocity = 0m, material = 0, nav_cost = 0m };
  }`;

const FLOW_OK = `  @flow field drift_on() -> flow_update
    footprint none;
    max_ops 48
  {
    return flow_update { x = p.x, y = p.y, z = p.z, vx = p.vx, vy = p.vy, vz = p.vz, attr0 = 0m };
  }`;

export const NEGATIVE_CORPUS: NegCase[] = [
  // -- FORM-E-001..009 lexical ------------------------------------------------
  { code: 'FORM-E-001', name: 'invalid-utf8', src: () => { const b = new TextEncoder().encode('module m { // é\n}\n'); b[b.length - 3] = 0xff; return b; } },
  { code: 'FORM-E-002', name: 'unterminated-block-comment', src: 'module m { /* never closed\n' },
  { code: 'FORM-E-003', name: 'unterminated-string', src: 'module m {\n  sound s { sample "abc; }\n}\n' },
  { code: 'FORM-E-004', name: 'illegal-escape', src: 'module m {\n  sound s { sample "a\\nb"; }\n}\n' },
  { code: 'FORM-E-005', name: 'no-token-starts', src: 'module m { const A: u32 = 1 $ 2; }\n' },
  { code: 'FORM-E-006', name: 'malformed-hex', src: 'module m { const A: u32 = 0x; }\n' },
  { code: 'FORM-E-006', name: 'uppercase-0X', src: 'module m { const A: u32 = 0X10; }\n' },
  { code: 'FORM-E-006', name: 'leading-zero-octal', src: 'module m { const A: u32 = 0123; }\n' },
  { code: 'FORM-E-007', name: 'int-exceeds-u32', src: 'module m { const A: u32 = 4294967296; }\n' },
  { code: 'FORM-E-008', name: 'frac-not-exact-fx16', src: 'module m { const A: fx16 = 0.5000001m; }\n' },
  { code: 'FORM-E-009', name: 'ident-too-long', src: () => `module m { const ${'A'.repeat(65)}: u32 = 1; }\n` },
  { code: 'FORM-E-009', name: 'string-too-long', src: () => `module m {\n  sound s { sample "${'x'.repeat(257)}"; }\n}\n` },

  // -- FORM-E-100..110 parse --------------------------------------------------
  { code: 'FORM-E-100', name: 'expected-token', src: 'module m { const A u32 = 1; }\n' },
  { code: 'FORM-E-101', name: 'unexpected-top-level', src: 'module m { 42; }\n' },
  { code: 'FORM-E-102', name: 'duplicate-param', src: P('  fn f(a: u32, a: u32) -> u32 { return a; }') },
  { code: 'FORM-E-102', name: 'duplicate-struct-field', src: P('  struct s { a: u32; a: u32; }') },
  { code: 'FORM-E-103', name: 'module-not-first', src: 'const A: u32 = 1;\nmodule m { }\n' },
  { code: 'FORM-E-103', name: 'trailing-content', src: 'module m { }\nconst A: u32 = 1;\n' },
  { code: 'FORM-E-104', name: 'record-unknown-field', src: S('  system s every 1 ticks reads motes, origin writes motes { spawn(motes, particle { position = origin, velocity = velocity3 { x = 0w, y = 0w, z = 0w }, age = 0, sparkle = 1 }); }') },
  { code: 'FORM-E-105', name: 'record-omits-field', src: S('  system s every 1 ticks reads motes, origin writes motes { spawn(motes, particle { position = origin }); }') },
  { code: 'FORM-E-106', name: 'record-duplicate-field', src: S('  system s every 1 ticks reads motes, origin writes motes { spawn(motes, particle { position = origin, position = origin }); }') },
  { code: 'FORM-E-107', name: 'range-outside-for', src: P('  fn f() -> u32 { let r = 0..8; return 0; }') },
  { code: 'FORM-E-108', name: 'stmt-in-presentation', src: S('  presentation p { let x = 1m; }') },
  { code: 'FORM-E-109', name: 'keyword-as-ident', src: P('  fn f() -> u32 { let tick = 3; return 0; }') },
  { code: 'FORM-E-110', name: 'expr-violation', src: P('  fn f() -> u32 { let x = 5(); return 0; }') },
  { code: 'FORM-E-110', name: 'chained-comparison', src: P('  fn f(a: u32, b: u32, c: u32) -> bool { return a < b < c; }') },

  // -- FORM-E-200..229 modules, names, imports ---------------------------------
  { code: 'FORM-E-201', name: 'duplicate-top-level', src: P('  const A: u32 = 1;\n  const A: u32 = 2;') },
  { code: 'FORM-E-202', name: 'unknown-module-import', src: P('  import nowhere;\n  const A: u32 = 1;') },
  { code: 'FORM-E-203', name: 'unknown-name', src: P('  fn f() -> u32 { return ghost; }') },
  {
    code: 'FORM-E-204', name: 'import-cycle',
    src: { 'a.form': 'module a { import b; const A: u32 = 1; }\n', 'b.form': 'module b { import a; const B: u32 = 2; }\n' },
  },
  {
    code: 'FORM-E-205', name: 'ambiguous-import',
    src: {
      'a.form': 'module a { fn helper() -> u32 { return 1; } }\n',
      'b.form': 'module b { fn helper() -> u32 { return 2; } }\n',
      'm.form': 'module m { import a { helper }; import b { helper }; }\n',
    },
  },
  {
    code: 'FORM-E-206', name: 'private-name-selected',
    src: {
      'a.form': 'module a { const _SECRET: u32 = 1; }\n',
      'm.form': 'module m { import a { _SECRET }; }\n',
    },
  },

  // -- FORM-E-300..334 types ----------------------------------------------------
  { code: 'FORM-E-300', name: 'type-mismatch', src: P('  fn f(b: fx16) -> fx16 { let a: u32 = b; return b; }') },
  { code: 'FORM-E-301', name: 'unknown-type', src: P('  fn f() -> quat { return 0; }') },
  { code: 'FORM-E-302', name: 'let-reassignment', src: P('  fn f() -> u32 { let a = 1; a = 2; return a; }') },
  { code: 'FORM-E-303', name: 'use-before-let', src: P('  fn f() -> u32 { let b = a + 1; let a = 2; return b; }') },
  { code: 'FORM-E-304', name: 'wrong-arg-count', src: P('  fn g(a: u32) -> u32 { return a; }\n  fn f() -> u32 { return g(1, 2); }') },
  { code: 'FORM-E-305', name: 'index-not-integer', src: S('  system s every 1 ticks reads motes.age writes energy { let i: fx16 = 0.5m; let a = motes.age[i]; energy = 0m; }') },
  { code: 'FORM-E-306', name: 'field-on-non-struct', src: S('  system s every 1 ticks reads energy writes energy { let a = energy.sparkle; energy = 0m; }') },
  { code: 'FORM-E-307', name: 'enum-member-missing', src: P('  enum hue { red, green }\n  fn f() -> u32 { let c = hue.ultramarine; return 1; }') },
  { code: 'FORM-E-308', name: 'non-const-duration', src: S('  ' + '@earth field lift_ground() -> terrain_delta footprint circle(0m, 0m, 4m); max_ops 8 { return terrain_delta { height = 0m, velocity = 0m, material = 0, nav_cost = 0m }; }\n  system s every 1 ticks reads writes terrain { apply terrain_field lift_ground(origin: world2 { x = 0w, y = 0w }) duration step; }') },
  { code: 'FORM-E-309', name: 'fn-return-mismatch', src: P('  fn f(a: fx16) -> u32 { return a; }') },
  { code: 'FORM-E-310', name: 'missing-return', src: P('  fn f() -> u32 { let a = 1; }') },
  { code: 'FORM-E-311', name: 'select-branch-mismatch', src: P('  fn f(c: bool) -> u32 { let x = if c { 1m } else { 2.25w }; return 0; }') },
  { code: 'FORM-E-312', name: 'cond-not-bool', src: P('  fn f() -> u32 { return if 1 { 2 } else { 3 }; }') },
  { code: 'FORM-E-313', name: 'wrong-literal-suffix', src: P('  const A: fx24 = 1.5;') },
  { code: 'FORM-E-320', name: 'negative-to-u32', src: P('  const A: u32 = -1;') },
  { code: 'FORM-E-330', name: 'world-velocity-mix', src: P('  fn f() -> world3 { let o = world3 { x = 0w, y = 0w, z = 0w }; let v = o + velocity3 { x = 0w, y = 0w, z = 0w }; return o; }') },
  { code: 'FORM-E-331', name: 'mixed-precision', src: P('  fn f() -> fx16 { return 1m + 2.25w; }') },
  { code: 'FORM-E-332', name: 'fx24-in-field', src: P('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { let v = world2 { x = 0w, y = 0w }; return terrain_delta { height = 0m, velocity = 0m, material = 0, nav_cost = 0m }; }') },
  { code: 'FORM-E-333', name: 'assign-to-const', src: P('  const A: u32 = 1;\n  fn f() -> u32 { A = 2; return 0; }') },
  { code: 'FORM-E-334', name: 'conversion-arg-wrong', src: P('  fn f() -> unit8 { return to_unit8(2.25w); }') },

  // -- FORM-E-400..409 domains and effects ---------------------------------------
  { code: 'FORM-E-400', name: 'write-outside-system', src: S('  fn f() -> u32 { energy = 1m; return 0; }') },
  { code: 'FORM-E-401', name: 'write-not-declared', src: S('  system s every 1 ticks reads writes energy { charge = 1m; }') },
  { code: 'FORM-E-402', name: 'read-not-declared', src: S('  system s every 1 ticks reads writes energy { let a = charge; energy = 1m; }') },
  { code: 'FORM-E-402', name: 'read-in-fn', src: S('  fn f() -> fx16 { return energy; }') },
  { code: 'FORM-E-403', name: 'input-outside-sim', src: S('  presentation p { emit draw_form(form: 1, transform: input.player(0), view_mask: 0, weight: 50%); }') },
  { code: 'FORM-E-404', name: 'random-stream-in-field', src: P('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { let s = random.stream(1); return terrain_delta { height = 0m, velocity = 0m, material = 0, nav_cost = 0m }; }') },
  { code: 'FORM-E-405', name: 'presentation-assign', src: S('  presentation p { energy = 1m; }') },
  { code: 'FORM-E-406', name: 'spawn-in-fn', src: S('  fn f() -> u32 { spawn(motes, particle { position = origin, velocity = velocity3 { x = 0w, y = 0w, z = 0w }, age = 0 }); return 0; }') },
  { code: 'FORM-E-407', name: 'read-write-separation-cycle', src: S('  system a every 1 ticks reads energy writes energy { energy = energy; }\n  system b every 1 ticks reads energy writes energy { energy = energy; }') },
  { code: 'FORM-E-408', name: 'sound-forward-reference', src: S('  presentation p { emit audio(sound: boom, at: origin); }\n  sound boom { sample "tone/boom.ztone"; gain 80%; pitch 1.0; pan 0; }') },
  { code: 'FORM-E-409', name: 'scenario-stmt-outside', src: S('  system s every 1 ticks reads writes energy { seed 1; energy = 0m; }') },

  // -- FORM-E-460..464 terrain/present object model --------------------------------
  { code: 'FORM-E-460', name: 'direct-terrain-mutation', src: S('  system s every 1 ticks reads writes terrain { terrain.height = 1m; }') },
  { code: 'FORM-E-461', name: 'apply-outside-sim', src: P('  fn f() -> u32 { apply terrain_field lift_ground(origin: world2 { x = 0w, y = 0w }) duration 45t; return 0; }\n' + EARTH_OK) },
  { code: 'FORM-E-462', name: 'apply-non-earth', src: S('  system s every 1 ticks reads writes terrain { apply terrain_field drift_on(origin: world2 { x = 0w, y = 0w }) duration 45t; }\n' + FLOW_OK) },
  { code: 'FORM-E-463', name: 'duration-zero', src: S('  system s every 1 ticks reads writes terrain { apply terrain_field lift_ground(origin: world2 { x = 0w, y = 0w }) duration 0t; }\n' + EARTH_OK) },
  { code: 'FORM-E-464', name: 'camera-not-world3', src: S('  presentation p { view 0 from 5 budget 10%; }') },

  // -- FORM-E-500..507 scheduling ----------------------------------------------------
  { code: 'FORM-E-500', name: 'two-writers-one-phase', src: S('  system a every 1 ticks reads writes energy { energy = 1m; }\n  system b every 1 ticks reads writes energy { energy = 2m; }') },
  { code: 'FORM-E-501', name: 'descending-range', src: S('  system s every 1 ticks reads writes energy { for i in 5..3 { energy = 0m; } }') },
  { code: 'FORM-E-502', name: 'unbounded-trip-count', src: S('  system s every 1 ticks reads step writes energy { for i in 0..step { energy = 0m; } }') },
  { code: 'FORM-E-503', name: 'spawn-in-pool-sugar', src: S('  system s every 1 ticks reads motes writes motes, energy { for c in motes { spawn(motes, particle { position = origin, velocity = velocity3 { x = 0w, y = 0w, z = 0w }, age = 0 }); } }') },
  { code: 'FORM-E-504', name: 'stagger-non-pool', src: S('  system s every 4 ticks stagger over energy reads writes energy { energy = 0m; }') },
  { code: 'FORM-E-505', name: 'read-write-cycle', src: S('  global a: fx16 = 0m;\n  global b: fx16 = 0m;\n  system s1 every 1 ticks reads a writes b { b = a; }\n  system s2 every 1 ticks reads b writes a { a = b; }') },
  { code: 'FORM-E-506', name: 'every-zero', src: S('  system s every 0 ticks reads writes energy { energy = 0m; }') },
  { code: 'FORM-E-507', name: 'stagger-rate-mismatch', src: S('  system s every 4 ticks stagger 2 over motes reads motes writes motes { energy = 0m; }') },

  // -- FORM-E-600..610 presentation emit -----------------------------------------------
  { code: 'FORM-E-600', name: 'emit-outside-presentation', src: S('  system s every 1 ticks reads writes energy { emit audio(sound: boom, at: origin); }\n  sound boom { sample "tone/boom.ztone"; gain 80%; pitch 1.0; pan 0; }') },
  { code: 'FORM-E-601', name: 'emit-arg-missing', src: S('  presentation p { emit draw_form(form: 1, transform: origin, view_mask: 0); }') },
  { code: 'FORM-E-602', name: 'emit-arg-unknown', src: S('  presentation p { emit draw_form(form: 1, transform: origin, view_mask: 0, weight: 50%, mode: 1); }') },
  { code: 'FORM-E-603', name: 'emit-kind-unknown', src: S('  presentation p { emit draw_quad(form: 1); }') },
  { code: 'FORM-E-604', name: 'three-views', src: S('  presentation p { view 0 from origin budget 30%; view 1 from origin budget 30%; view 1 from origin budget 30%; }') },
  { code: 'FORM-E-605', name: 'budget-sum-exceeds', src: S('  presentation p { view 0 from origin budget 60%; view 1 from origin budget 60%; }') },
  { code: 'FORM-E-606', name: 'view-id-not-01', src: S('  presentation p { view 2 from origin budget 30%; }') },
  { code: 'FORM-E-607', name: 'view-no-camera', src: S('  presentation p { view 0 budget 30%; }') },
  { code: 'FORM-E-608', name: 'draw-population-non-pool', src: S('  presentation p { emit draw_population(pool: energy, view_mask: 3, weight: 80%); }') },
  { code: 'FORM-E-609', name: 'audio-non-sound', src: S('  presentation p { emit audio(sound: energy, at: origin); }') },
  { code: 'FORM-E-610', name: 'page-id-not-const', src: S('  presentation p { emit draw_form(form: energy, transform: origin, view_mask: 0, weight: 50%); }') },

  // -- FORM-E-650..667 field dialect -----------------------------------------------------
  { code: 'FORM-E-650', name: 'unknown-profile', src: P('  @magic field f() -> terrain_delta footprint none; max_ops 8 { return terrain_delta { height = 0m, velocity = 0m, material = 0, nav_cost = 0m }; }') },
  { code: 'FORM-E-651', name: 'if-stmt-in-field', src: P('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { if sample.x > 0m { } return terrain_delta { height = 0m - 0.0m, velocity = 0m, material = 0, nav_cost = 0m }; }').replace('0x - 0.0', '0m') },
  { code: 'FORM-E-651', name: 'loop-in-field', src: P('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { for i in 0..2 { } return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }') },
  { code: 'FORM-E-652', name: 'call-in-field', src: P('  fn g(a: fx16) -> fx16 { return a; }\n  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { let x = g(1m); return terrain_delta { height = 0m, velocity = 0x0, material = 0, nav_cost = 0m }; }') },
  { code: 'FORM-E-653', name: 'stmt-not-let-return', src: S('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { energy = 1m; return terrain_delta { height = 0m, velocity = 0x0, material = 0, nav_cost = 0m }; }') },
  { code: 'FORM-E-654', name: 'max-ops-above-ceiling', src: P('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 33 { return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }') },
  { code: 'FORM-E-655', name: 'count-above-max-ops', src: P('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 1 { let a = 1m; let b = 2m; return terrain_delta { height = a, velocity = 0x0, material = 0, nav_cost = b }; }') },
  { code: 'FORM-E-656', name: 'state-access-in-field', src: S('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { let a = energy; return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }') },
  { code: 'FORM-E-657', name: 'input-in-field', src: P('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { let p = input.player(0); return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }') },
  { code: 'FORM-E-658', name: 'non-fx16-local', src: P('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { let c = #FF0000; return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }') },
  { code: 'FORM-E-659', name: 'register-budget', src: () => {
      const lets = Array.from({ length: 65 }, (_, i) => `    let v${i} = ${i}m;`).join('\n');
      const sum = Array.from({ length: 65 }, (_, i) => `v${i}`).join(' + ');
      return `module m {\n  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 32 {\n${lets}\n    return terrain_delta { height = ${sum}, velocity = 0x0, material = 0, nav_cost = 0x0 };\n  }\n}\n`;
    } },
  { code: 'FORM-E-660', name: 'params-field-not-fx16', src: P('  struct ps { ramp: fx16; seed: u32; }\n  @earth field f(params: ps) -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }') },
  { code: 'FORM-E-661', name: 'params-too-many', src: () => {
      const fields = Array.from({ length: 9 }, (_, i) => `p${i}: fx16;`).join(' ');
      return `module m {\n  struct ps { ${fields} }\n  @earth field f(params: ps) -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }\n}\n`;
    } },
  { code: 'FORM-E-662', name: 'params-not-struct', src: S('  @earth field f(params: energy) -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }') },
  { code: 'FORM-E-663', name: 'return-record-mismatch', src: P('  @earth field f() -> flow_update footprint rect(0m, 0m, 1m, 1m); max_ops 8 { return flow_update { x = 0x0, y = 0x0, z = 0x0, vx = 0x0, vy = 0x0, vz = 0x0, attr0 = 0x0 }; }') },
  { code: 'FORM-E-664', name: 'pool-lane-mapping', src: S('  @flow field drift_on() -> flow_update footprint none; max_ops 48 { return flow_update { x = p.x, y = p.y, z = p.z, vx = p.vx, vy = p.vy, vz = p.vz, attr0 = 0x0 }; }\n  system s every 1 ticks reads pebbles writes pebbles { drift_on(pebbles); }') },
  { code: 'FORM-E-665', name: 'division-in-field', src: P('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { let a = sample.x / 2m; return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }') },
  { code: 'FORM-E-665', name: 'bitwise-in-field', src: P('  @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 8 { let a = sample.x & 1m; return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }') },
  { code: 'FORM-E-666', name: 'footprint-mismatch-flow', src: P('  @flow field f() -> flow_update footprint rect(0m, 0m, 1m, 1m); max_ops 8 { return flow_update { x = p.x, y = p.y, z = p.z, vx = p.vx, vy = p.vy, vz = p.vz, attr0 = 0x0 }; }') },
  { code: 'FORM-E-666', name: 'footprint-mismatch-earth', src: P('  @earth field f() -> terrain_delta footprint none; max_ops 8 { return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }') },
  { code: 'FORM-E-667', name: 'flow-wrong-pool', src: S('  @flow field drift_on() -> flow_update footprint none; max_ops 48 { return flow_update { x = p.x, y = p.y, z = p.z, vx = p.vx, vy = p.vy, vz = p.vz, attr0 = 0x0 }; }\n  system s every 1 ticks reads writes energy { drift_on(energy); }') },

  // -- FORM-E-700..720 the D1 OUT list (refused, never silently parsed) --------------------
  { code: 'FORM-E-700', name: 'form-decl', src: P('  form ladder_x { rung: u32; }') },
  { code: 'FORM-E-701', name: 'macro-decl', src: P('  macro twice(x: u32) { return x * 2; }') },
  { code: 'FORM-E-702', name: 'generics', src: P('  struct box<T> { item: T; }') },
  { code: 'FORM-E-703', name: 'oop-class', src: P('  class wizard { hp: u32; }') },
  { code: 'FORM-E-704', name: 'closure-arrow', src: P('  fn f() -> u32 { let g = (x) -> x + 1; return 0; }') },
  { code: 'FORM-E-705', name: 'string-type', src: P('  string greeting = "hi";') },
  { code: 'FORM-E-705', name: 'string-value', src: P('  fn f() -> u32 { let s = "hi"; return 0; }') },
  { code: 'FORM-E-706', name: 'first-class-fn', src: P('  fn g(a: u32) -> u32 { return a; }\n  fn f() -> u32 { let h = g; return 0; }') },
  { code: 'FORM-E-707', name: 'pointer', src: P('  fn f(a: u32) -> u32 { let b = *a; return 0; }') },
  { code: 'FORM-E-708', name: 'while', src: P('  fn f() -> u32 { while true { } return 0; }') },
  { code: 'FORM-E-709', name: 'self-recursion', src: P('  fn f(a: u32) -> u32 { return f(a); }') },
  { code: 'FORM-E-709', name: 'mutual-recursion', src: P('  fn a(x: u32) -> u32 { return b(x); }\n  fn b(x: u32) -> u32 { return a(x); }') },
  { code: 'FORM-E-710', name: 'host-ffi', src: P('  extern fn host_sqrt(x: u32) -> u32;') },
  { code: 'FORM-E-711', name: 'float-type', src: P('  fn f() -> f32 { return 1.0; }') },
  { code: 'FORM-E-711', name: 'scientific-notation', src: P('  const A: u32 = 1e3;') },
  { code: 'FORM-E-712', name: 'break', src: P('  fn f() -> u32 { for i in 0..4 { break; } return 0; }') },
  { code: 'FORM-E-712', name: 'continue', src: P('  fn f() -> u32 { for i in 0..4 { continue; } return 0; }') },
  { code: 'FORM-E-713', name: 'terrain-material', src: P('  terrain_material rock { grit: u32; }') },
  { code: 'FORM-E-714', name: 'population-decl', src: P('  population orcs { size: u32; }') },
  { code: 'FORM-E-715', name: 'warp-profile', src: P('  @warp field wf() -> terrain_delta footprint none; max_ops 8 { return terrain_delta { height = 0x0, velocity = 0x0, material = 0, nav_cost = 0x0 }; }') },
  { code: 'FORM-E-716', name: 'spell-decl', src: P('  spell fireball { power: u32; }') },
  { code: 'FORM-E-717', name: 'layer-construct', src: S('  presentation p { emit layer(x: 1); }') },
  { code: 'FORM-E-718', name: 'audio-graph', src: P('  sound s { sample "tone/s.ztone"; envelope 2; }') },
  { code: 'FORM-E-719', name: 'dynamic-pool', src: P('  struct p { x: fx16; }\n  pool ps: p[];') },
  { code: 'FORM-E-720', name: 'assert-budget-unknown', src: S('  scenario sc { seed 1; assert_budget nowhere; }') },

  // -- FORM-E-800..820 capacities and bounds ----------------------------------------------
  { code: 'FORM-E-800', name: 'capacity-zero', src: P('  struct p { x: fx16; }\n  pool ps: p[0];') },
  { code: 'FORM-E-800', name: 'capacity-non-const', src: S('  pool ps: plain[energy];') },
  { code: 'FORM-E-801', name: 'pool-element-not-struct', src: P('  pool xs: u32[4];') },
  { code: 'FORM-E-801', name: 'pool-element-recursive', src: P('  struct node { next: node; }\n  pool ns: node[4];') },
  { code: 'FORM-E-820', name: 'provable-oob', src: S('  system s every 1 ticks reads motes.age writes energy { let a = motes.age[99]; energy = 0m; }') },
  { code: 'FORM-E-820', name: 'array-oob', src: P('  fn f(arr: u32[2]) -> u32 { return arr[5]; }') },

  // -- FORM-E-900..907 scenarios -------------------------------------------------------------
  { code: 'FORM-E-900', name: 'seed-missing', src: S('  scenario sc { spawn player 0 at origin; }') },
  { code: 'FORM-E-900', name: 'seed-repeated', src: S('  scenario sc { seed 1; seed 2; }') },
  { code: 'FORM-E-901', name: 'load-unknown', src: S('  scenario sc { seed 1; load nowhere; }') },
  { code: 'FORM-E-902', name: 'player-index', src: S('  scenario sc { seed 1; spawn player 4 at origin; }') },
  { code: 'FORM-E-903', name: 'at-not-ascending', src: S('  scenario sc { seed 1; at 100 ticks tick_one(); at 50 ticks tick_one(); }\n  system tick_one every 1 ticks reads writes energy { energy = 0m; }') },
  { code: 'FORM-E-904', name: 'action-unknown', src: S('  scenario sc { seed 1; at 10 ticks no_such_system(); }') },
  { code: 'FORM-E-905', name: 'capture-before-at', src: S('  scenario sc { seed 1; at 200 ticks tick_one(); capture frame 150 as "early"; }\n  system tick_one every 1 ticks reads writes energy { energy = 0m; }') },
  { code: 'FORM-E-906', name: 'assert-undeclared', src: S('  scenario sc { seed 1; assert ghost_state == 1; }') },
  { code: 'FORM-E-907', name: 'tolerance-not-exact', src: S('  scenario sc { seed 1; assert energy == 0m within 0.00001; }') },
];

/** Codes the frontend cannot raise (documented spec-issue exemptions). */
export const EXEMPT_CODES = ['FORM-E-668', 'FORM-E-821', 'FORM-E-822', 'FORM-E-830', 'FORM-E-831'];
