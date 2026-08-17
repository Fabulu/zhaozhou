// checker.test.ts — the D6 schedule (one writer per component per phase,
// both spans cited), the E-832 registry-overflow gate, scheduler determinism
// (schedule is a pure function of declaration order + read/write sets), and
// literal exactness arithmetic (Q16.16/Q1.39.24/U 0.0.16/U 0.0.8 laws).

import { test } from 'node:test';
import assert from 'node:assert/strict';

import { compile } from './helpers.js';

const MOD = (body: string): string => `module m {\n${body}\n}\n`;

test('schedule: one writer per component per phase; readers follow writers', () => {
  const r = compile(MOD(`
    global a: fx16 = 0m;
    global b: fx16 = 0m;
    system w1 every 1 ticks reads writes a { a = 1m; }
    system r1 every 1 ticks reads a writes b { b = a; }
  `));
  assert.deepEqual(r.codes, []);
  const phases = r.check!.schedule!.phases;
  assert.equal(phases.length, 2);
  assert.deepEqual(phases[0]!.systems.map((s) => s.name), ['w1']);
  assert.deepEqual(phases[1]!.systems.map((s) => s.name), ['r1']);
});

test('schedule: declaration order within a phase (deterministic-scheduling §3.5)', () => {
  const r = compile(MOD(`
    global a: fx16 = 0m;
    global b: fx16 = 0m;
    system x every 1 ticks reads writes a { a = 1m; }
    system y every 1 ticks reads writes b { b = 1m; }
    system z every 2 ticks reads writes { }
  `));
  // z has no interactions — shares phase 0 with x and y, in declaration order
  const phases = r.check!.schedule!.phases;
  assert.deepEqual(phases[0]!.systems.map((s) => s.name), ['x', 'y', 'z']);
});

test('schedule: E-500 cites BOTH spans', () => {
  const r = compile(MOD(`
    global a: fx16 = 0m;
    system first every 1 ticks reads writes a { a = 1m; }
    system second every 1 ticks reads writes a { a = 2m; }
  `));
  assert.deepEqual(r.codes, ['FORM-E-500']);
  const msg = r.diagnostics.find((d) => d.code === 'FORM-E-500')!.message;
  assert.ok(msg.includes('first') && msg.includes('second'), 'both systems named');
  assert.ok(/:\d+/.test(msg), 'both spans cited as offsets');
});

test('schedule: multi-rate writer and fast reader interact through phase order only', () => {
  const r = compile(MOD(`
    global a: fx16 = 0m;
    system slow_writer every 4 ticks reads writes a { a = 1m; }
    system fast_reader every 1 ticks reads a writes a { a = a + 1m; }
  `));
  // reader is also a writer of a — conservative ordering puts the fast system
  // after the slow writer; no same-phase double write
  assert.deepEqual(r.codes, []);
  const phases = r.check!.schedule!.phases;
  assert.equal(phases.length, 2);
});

test('schedule: stagger pins one isolated per-entity iteration to every-rate', () => {
  const r = compile(MOD(`
    struct s { x: fx16; }
    pool p: s[16];
    system staggered every 4 ticks stagger over p reads p writes p.x {
      for i in 0..p.count { p.x[i] = p.x[i] + 1m; }
    }
  `));
  assert.deepEqual(r.codes, []);
  assert.equal(r.check!.schedule!.phases[0]!.systems[0]!.every, 4n);
});

test('stagger refuses missing iteration, extra loops, and off-loop/global writes', () => {
  const missing = compile(MOD(`
    struct s { x: fx16; }
    pool p: s[16];
    system staggered every 4 ticks stagger over p reads p writes p { }
  `));
  assert.deepEqual(missing.codes, ['FORM-E-504']);

  const leaking = compile(MOD(`
    struct s { x: fx16; }
    pool p: s[16];
    global g: fx16 = 0m;
    system staggered every 4 ticks stagger over p reads p writes p.x, g {
      g = 1m;
      for i in 0..p.count {
        p.x[i] = p.x[i] + 1m;
        for j in 0..p.count { p.x[j] = p.x[j] + 1m; }
      }
    }
  `));
  assert.ok(leaking.codes.includes('FORM-E-504'));
});

test('E-832: > 65536 declarations in one module trips the source-ID registry gate', () => {
  const decls = Array.from({ length: 65537 }, (_, i) => `  const K${i}: u32 = ${i % 100};`).join('\n');
  const r = compile(MOD(decls));
  assert.deepEqual(r.codes, ['FORM-E-832']);
  assert.ok(!r.ok);
});

test('exactness: fx16 steps are 2^-16 — 0.5m exact, 0.5000001m is FORM-E-008', () => {
  assert.deepEqual(compile(MOD('  const A: fx16 = 0.5m;')).codes, []);
  assert.deepEqual(compile(MOD('  const B: fx16 = 0.5000001m;')).codes, ['FORM-E-008']);
});

test('exactness: fx24 steps are 2^-24; angle16 wraps mod 1 turn; unit8 rounds half-up', () => {
  assert.deepEqual(compile(MOD('  const A: fx24 = 2.25w;')).codes, []);
  // 2^-24 = 0.000000059604644775390625 — representable; 1e-7 is not
  assert.deepEqual(compile(MOD('  const A2: fx24 = 0.000000059604644775390625w;')).codes, []);
  assert.deepEqual(compile(MOD('  const B: fx24 = 0.0000001w;')).codes, ['FORM-E-008']);
  // 2.5 turns wraps to 0.5 turns — no range error for angle16 (U 0.0.16 wraps)
  assert.deepEqual(compile(MOD('  const C: angle16 = 2.5turn;')).codes, []);
  // 1deg is not an exact 1/65536 multiple -> FORM-E-008
  assert.deepEqual(compile(MOD('  const D: angle16 = 1deg;')).codes, ['FORM-E-008']);
  // 90deg = 0.25 turn exact
  assert.deepEqual(compile(MOD('  const E: angle16 = 90deg;')).codes, []);
  // unit8 percent rounds by law (raw = (pct*256+50)/100) — never FORM-E-008
  assert.deepEqual(compile(MOD('  const F: unit8 = 33%;')).codes, []);
});

test('exactness: fx16 range is [-32768, 32768) — FORM-E-007 outside', () => {
  assert.deepEqual(compile(MOD('  const A: fx16 = 32767.5m;')).codes, []);
  assert.deepEqual(compile(MOD('  const B: fx16 = 32768m;')).codes, ['FORM-E-007']);
});

test('space-typing: velocity3 never mixes into world3 arithmetic (FORM-E-330)', () => {
  const r = compile(MOD(`
    fn f(p: world3, v: velocity3) -> world3 { return p + v; }
  `));
  assert.deepEqual(r.codes, ['FORM-E-330']);
});

test('present purity: presentation blocks write nothing, ever (domains §2.2)', () => {
  const r = compile(MOD(`
    global a: fx16 = 0m;
    presentation p { a = 1m; }
  `));
  assert.deepEqual(r.codes, ['FORM-E-405']);
});

test('field dialect: fx24 never appears inside a field program (Q2)', () => {
  const r = compile(MOD(`
    @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 32 {
      let wide = 2.25w;
      return terrain_delta { height = 0m, velocity = 0m, material = 0, nav_cost = 0m };
    }
  `));
  assert.ok(r.codes.includes('FORM-E-332'), r.codes.join(','));
});

test('one-writer analysis feeds W3.3: the schedule shape is committed data', () => {
  // mirrors the positive corpus ordering: input_latch -> spawn_waves -> integrate
  const r = compile(MOD(`
    struct s { x: fx16; }
    pool p: s[4];
    global pos: fx16 = 0m;
    global out: fx16 = 0m;
    system latch every 1 ticks reads input writes pos { pos = 0m; }
    system spawner every 60 ticks reads pos writes p { spawn(p, s { x = pos }); }
    system mover every 1 ticks reads p writes out { for c in p { out = c.x; } }
  `));
  assert.deepEqual(r.codes, [], r.diagnostics.map((d) => d.code + ' ' + d.message).join('\n'));
  const names = r.check!.schedule!.phases.map((ph) => ph.systems.map((s) => s.name));
  assert.deepEqual(names, [['latch'], ['spawner'], ['mover']]);
});
