import assert from 'node:assert/strict';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import test from 'node:test';

import { emitCpp } from '../../src/backends/index.js';
import {
  allocateAuthoredResourceIndices, allocateTransientResourceIndices,
} from '../../src/backends/cpp/emitter.js';
import { compileFrontend } from '../../src/frontend/index.js';
import { lowerHir } from '../../src/hir/index.js';
import { lowerZir } from '../../src/zir/index.js';
import { repoRoot } from '../helpers.js';

function compileSources(sources: Record<string, string>) {
  const frontend = compileFrontend(sources);
  assert.equal(frontend.ok, true, frontend.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  return emitCpp(hir, lowerZir(hir));
}

function compileFixture(reverse = false) {
  const dir = path.join(repoRoot(), 'compiler', 'tests', 'form', 'fixture');
  const files = ['a_arena.form', 'b_audit.form'];
  if (reverse) files.reverse();
  return compileSources(Object.fromEntries(files.map((file) => [file, readFileSync(path.join(dir, file), 'utf8')])));
}

function runGeneratedNative(
  output: ReturnType<typeof compileFixture>,
  smokeSource: string,
  extraSources: readonly string[] = [],
  extraFlags: readonly string[] = [],
): { status: number | null; stdout: string; stderr: string } {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  const root = mkdtempSync(path.join(tmpdir(), 'form-cpp-'));
  try {
    const generated = path.join(root, 'generated', 'form');
    mkdirSync(generated, { recursive: true });
    for (const file of output.files) {
      const target = path.join(root, ...file.path.split('/'));
      mkdirSync(path.dirname(target), { recursive: true });
      writeFileSync(target, file.content, { encoding: 'utf8' });
    }
    const smoke = path.join(root, 'smoke.cpp');
    writeFileSync(smoke, smokeSource, 'utf8');
    const executable = path.join(root, 'smoke.exe');
    const repo = repoRoot();
    const generatedSources = output.files
      .filter((file) => file.path.endsWith('.cpp'))
      .map((file) => path.join(root, ...file.path.split('/')));
    const args = [
      '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
      ...extraFlags,
      `-I${generated}`,
      `-I${path.join(repo, 'runtime', 'include')}`,
      `-I${path.join(repo, 'reference', 'include')}`,
      ...generatedSources,
      path.join(repo, 'reference', 'src', 'zref_frame.cpp'),
      ...extraSources,
      smoke, '-o', executable,
    ];
    const build = spawnSync(compiler, args, { encoding: 'utf8', windowsHide: true });
    assert.equal(build.status, 0, `${build.stdout}\n${build.stderr}`);
    const run = spawnSync(executable, [], { encoding: 'utf8', windowsHide: true });
    return { status: run.status, stdout: run.stdout, stderr: run.stderr };
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
}

function byPath(output: ReturnType<typeof compileFixture>, name: string): string {
  const file = output.files.find((item) => item.path.endsWith(name));
  assert.ok(file, `missing emitted ${name}`);
  return file.content;
}

const U64_MASK = (1n << 64n) - 1n;
const U32_MASK = (1n << 32n) - 1n;

function floorDiv(numerator: bigint, denominator: bigint): bigint {
  let quotient = numerator / denominator;
  if (numerator % denominator < 0n) quotient -= 1n;
  return quotient;
}

function roundHalfUp(numerator: bigint, denominator: bigint): bigint {
  if (denominator < 0n) return roundHalfUp(-numerator, -denominator);
  return floorDiv(numerator + denominator / 2n, denominator);
}

function satSigned(value: bigint, bits: bigint): bigint {
  const lo = -(1n << (bits - 1n));
  const hi = (1n << (bits - 1n)) - 1n;
  return value < lo ? lo : value > hi ? hi : value;
}

interface OracleStream { state: bigint; increment: bigint }

function oracleStream(seed: bigint, ...ids: bigint[]): OracleStream {
  let mixed = (0x853c49e6748fea9bn ^ seed) & U64_MASK;
  for (const id of ids) mixed = ((mixed ^ (id & U32_MASK)) * 0xda942042e4dd58b5n) & U64_MASK;
  return { state: mixed, increment: ((mixed << 1n) | 1n) & U64_MASK };
}

function oracleDraw(stream: OracleStream): bigint {
  const old = stream.state;
  stream.state = (old * 6364136223846793005n + stream.increment) & U64_MASK;
  const shifted = (((old >> 18n) ^ old) >> 27n) & U32_MASK;
  const rotate = Number((old >> 59n) & 31n);
  return ((shifted >> BigInt(rotate)) | (shifted << BigInt((-rotate) & 31))) & U32_MASK;
}

function signed32(bits: bigint): bigint {
  return bits <= 0x7fffffffn ? bits : bits - (1n << 32n);
}

test('C++17 emission is byte-stable, module-partitioned, and phase-flat', () => {
  const first = compileFixture(false);
  const second = compileFixture(true);
  assert.deepEqual(first, second);
  assert.deepEqual(first.files.map((file) => file.path), [
    'generated/form/arena.cpp',
    'generated/form/arena.hpp',
    'generated/form/audit.cpp',
    'generated/form/audit.hpp',
    'generated/form/form_game.hpp',
    'generated/form/form_types.hpp',
  ]);
  assert.ok(first.files.every((file) => file.content.startsWith('// Generated by Form C++ backend v1; DO NOT EDIT.\n')));
  assert.ok(first.files.every((file) => file.content.endsWith('\n') && !file.content.includes('\r')));

  const arena = byPath(first, 'arena.hpp');
  assert.match(arena, /namespace form::arena \{/);
  assert.match(arena, /std::array<World3, _form_pool_capacity> _form_pool_column_706f736974696f6e/);
  assert.match(arena, /std::array<u32, _form_pool_capacity> _form_pool_column_616765/);
  const arenaSource = byPath(first, 'arena.cpp');
  const contract = arenaSource.indexOf('ZHAO_OP_SET_PRESENTATION_CONTRACT');
  const view = arenaSource.indexOf('ZHAO_OP_SET_VIEW');
  const draw = arenaSource.indexOf('ZHAO_OP_DRAW_POPULATION');
  assert.ok(contract >= 0 && contract < view && view < draw);
  assert.match(arenaSource, /record\.payload\.geometry_tokens\[0u\] = 80u/);
  assert.match(arenaSource, /record\.payload\.shared_tokens = 20u/);
  assert.match(arenaSource, /view_projection\.m03 = fx16_sub\(0, fx16_from_fx24\(_view_camera_0\.x\)\)/);
  const game = byPath(first, 'form_game.hpp');
  assert.match(game, /struct FormState \{/);
  assert.match(game, /inline void sim_tick\(FormState& state, const PadFrame pads\[4\], u32 tick\)/);
  assert.match(game, /inline void present_frame\(const FormState& state, zref::FrameBuilder& builder\)/);
  assert.match(game, /inline u32 sim_hash\(u32 previous, const FormState& state\)/);
  const seed = game.indexOf('arena::system_seed_wave');
  const observe = game.indexOf('audit::system_observe');
  const advance = game.indexOf('arena::system_advance');
  assert.ok(seed >= 0 && seed < observe && observe < advance);
  assert.match(game, /if \(\(tick % 2u\) == 0u\) arena::system_seed_wave/);
  assert.doesNotMatch(game, /if \(\(tick % 4u\) == 0u\) arena::system_advance/);
  assert.match(game, /arena::system_advance\(state, pads, tick\);/);
  assert.match(byPath(first, 'arena.cpp'), /if \(\(i % 4u\) == \(tick % 4u\)\)/);
});

test('transient resource allocator rejects duplicate sites and bounded-index exhaustion', () => {
  const site = { module: 0, sourceId: 0x90000001, role: 'draw_form_transform' as const };
  assert.throws(
    () => allocateTransientResourceIndices([site, site], new Set(), 4),
    /duplicate transient mapping/,
  );
  assert.throws(
    () => allocateTransientResourceIndices([
      site,
      { module: 1, sourceId: 0x90010001, role: 'surface_stamp_terrain_patch' },
    ], new Set([1]), 2),
    /exhausted the 24-bit handle index space/,
  );
  assert.deepEqual(
    allocateTransientResourceIndices([
      { module: 256, sourceId: 0x91000001, role: 'surface_stamp_terrain_patch' },
      site,
    ], new Set([1]), 4),
    [
      { ...site, handle: 0x01000002 },
      {
        module: 256,
        sourceId: 0x91000001,
        role: 'surface_stamp_terrain_patch',
        handle: 0x01000003,
      },
    ],
  );
  const frontend = compileFrontend({
    'duplicate.form': `module duplicate {
  const PAGE: u32 = 3;
  global camera: world3 = world3 { x = 0w, y = 0w, z = 0w };
  presentation showcase {
    view 0 from camera budget 100%;
    emit draw_form(form: PAGE, transform: camera, view_mask: 1, weight: 100%);
    emit draw_form(form: PAGE, transform: camera, view_mask: 1, weight: 100%);
  }
}\n`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((diagnostic) =>
    `${diagnostic.code}: ${diagnostic.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  const presentation = hir.declarations.find((decl) => decl.kind === 'presentation');
  assert.ok(presentation?.kind === 'presentation');
  presentation.emits[1]!.sourceId = presentation.emits[0]!.sourceId;
  assert.throws(
    () => emitCpp(hir, lowerZir(hir)),
    /duplicate presentation source_id/,
  );
});

test('authored resources preserve full-u32 identity and role without 24-bit aliases', () => {
  assert.deepEqual(
    allocateAuthoredResourceIndices([
      { resourceId: 16777217, role: 'form_page' },
      { resourceId: 1, role: 'form_page' },
      { resourceId: 1, role: 'form_page' },
      { resourceId: 1, role: 'sound' },
    ], 4),
    [
      { resourceId: 1, role: 'form_page', handle: 0x01000001 },
      { resourceId: 16777217, role: 'form_page', handle: 0x01000002 },
      { resourceId: 1, role: 'sound', handle: 0x01000003 },
    ],
  );
  assert.throws(
    () => allocateAuthoredResourceIndices([
      { resourceId: 1, role: 'form_page' },
      { resourceId: 2, role: 'form_page' },
      { resourceId: 3, role: 'form_page' },
    ], 2),
    /exhausted the 24-bit handle index space/,
  );
  assert.throws(
    () => allocateAuthoredResourceIndices([{ resourceId: 0x100000000, role: 'form_page' }]),
    /out-of-range authored resource ID/,
  );
});

test('native authored-resource table publishes page IDs 1 and 16777217 separately', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const output = compileSources({
    'pages.form': `module pages {
  const LOW: u32 = 1;
  const HIGH: u32 = 16777217;
  global camera: world3 = world3 { x = 0w, y = 0w, z = 0w };
  presentation showcase {
    view 0 from camera budget 100%;
    emit draw_form(form: LOW, transform: camera, view_mask: 1, weight: 100%);
    emit draw_form(form: HIGH, transform: camera, view_mask: 1, weight: 100%);
  }
}\n`,
  });
  const types = byPath(output, 'form_types.hpp');
  assert.match(types, /AuthoredResourceBinding\{1u, AuthoredResourceRole::FormPage, 16777217u\}/);
  assert.match(types, /AuthoredResourceBinding\{16777217u, AuthoredResourceRole::FormPage, 16777218u\}/);
  assert.doesNotMatch(types, /page_id\s*&\s*0x00ffffff/);
  const run = runGeneratedNative(output, `
#include "form_game.hpp"
#include <zhao_abi.h>
int main() {
  form::FormState state{};
  form::initialize(state, 7u);
  zhao::ZhaoFrameBuilder builder;
  builder.begin_frame(1u, 1u, 0u, 0u);
  form::present_frame(state, builder, {});
  builder.end_frame(zhao_abi::ZHAO_COMPL_DONE);
  const auto packet = builder.seal(1u, 1u, 1u);
  form::u32 handles[2]{};
  form::u32 found = 0u;
  for (std::size_t off = zhao_abi::ZHAO_FRAME_HEADER_BYTES;
       off + 8u <= packet.size();) {
    const form::u16 opcode = static_cast<form::u16>(packet[off]) |
      static_cast<form::u16>(static_cast<form::u16>(packet[off + 1u]) << 8u);
    const form::u16 bytes = static_cast<form::u16>(packet[off + 2u]) |
      static_cast<form::u16>(static_cast<form::u16>(packet[off + 3u]) << 8u);
    if (bytes == 0u || off + bytes > packet.size()) return 10;
    if (opcode == zhao_abi::ZHAO_OP_DRAW_FORM) {
      zhao_abi::ZhReader reader(packet.data() + off, bytes);
      zhao_abi::ZhRecordDrawForm record{};
      if (!zhao_abi::zhao_unpack_draw_form(reader, record) || found >= 2u) return 11;
      handles[found++] = record.payload.form;
    }
    off += bytes;
  }
  if (found != 2u || handles[0] == handles[1]) return 12;
  if (handles[0] != form::authored_resource_handle(form::AuthoredResourceRole::FormPage, 1u)) return 13;
  if (handles[1] != form::authored_resource_handle(form::AuthoredResourceRole::FormPage, 16777217u)) return 14;
  return 0;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
});

test('emitted deterministic source has no forbidden numeric or host-clock surface', () => {
  const output = compileFixture();
  const forbidden = /\b(?:float|double)\b|<cmath>|\b(?:chrono|clock_gettime|system_clock|steady_clock|gettimeofday)\b/;
  for (const file of output.files) {
    assert.doesNotMatch(file.content, forbidden, file.path);
    assert.doesNotMatch(file.content, /\bwhile\s*\(|\bgoto\b/, file.path);
  }
});

test('C++ backend refuses field applications until W3.4 supplies the physical wrapper', () => {
  const frontend = compileFrontend({
    'field_user.form': `module field_user {
  @earth field lift_ground() -> terrain_delta
    footprint circle(0m, 0m, 4m);
    max_ops 16
  {
    let d = dist(sample.x, sample.z, 0m, 0m);
    return terrain_delta { height = d, velocity = 0m, material = 0, nav_cost = 0m };
  }
  system apply_earth every 1 ticks reads writes terrain {
    apply terrain_field lift_ground(origin: world2 { x = 0w, y = 0w }) duration 45t;
  }
}
`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  assert.throws(
    () => emitCpp(hir, lowerZir(hir)),
    /refused 1 unlinked physical field invocation before emission:.*earth field 'lift_ground'.*W3\.4 must supply validated physical Field IR wrappers/s,
  );
});

test('C++ backend reports every earth and flow site in one pre-emission refusal', () => {
  const frontend = compileFrontend({
    'physical.form': `module physical {
  struct particle { position: world3; velocity: velocity3; age: u32; }
  pool motes: particle[8];
  @earth field lift_ground() -> terrain_delta
    footprint circle(0m, 0m, 4m);
    max_ops 16
  {
    return terrain_delta { height = 1m, velocity = 0m, material = 0, nav_cost = 0m };
  }
  @flow field drift_on() -> flow_update
    footprint none;
    max_ops 48
  {
    return flow_update { x = p.x, y = p.y, z = p.z, vx = p.vx, vy = p.vy, vz = p.vz, attr0 = 0m };
  }
  system apply_earth every 1 ticks reads writes terrain {
    apply terrain_field lift_ground(origin: world2 { x = 0w, y = 0w }) duration 1t;
  }
  system apply_flow every 4 ticks stagger over motes reads motes writes motes {
    drift_on(motes);
  }
}
`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  let message = '';
  try {
    emitCpp(hir, lowerZir(hir));
    assert.fail('physical sites should refuse C++ emission');
  } catch (error) {
    message = String(error);
  }
  assert.match(message, /refused 2 unlinked physical field invocations before emission/);
  assert.match(message, /earth field 'lift_ground' \(apply statement\)/);
  assert.match(message, /flow field 'drift_on' \(direct call\)/);
  assert.match(message, /W3\.4 must supply validated physical Field IR wrappers for every listed site/);
});

test('imported pool spawn uses its owner-qualified capacity', () => {
  const output = compileSources({
    'owner.form': `module owner {
  struct item { value: u32; }
  pool remote: item[3];
}
`,
    'user.form': `module user {
  import owner { item, remote };
  system populate every 1 ticks reads writes remote {
    spawn(remote, item { value = 7 });
  }
}
`,
  });
  const source = byPath(output, 'user.cpp');
  assert.match(source, /state\.owner\.remote\._form_pool_count >= form::owner::remote_pool::_form_pool_capacity/);
  assert.doesNotMatch(source, /state\.owner\.remote\._form_pool_count >= form::user::remote_pool::_form_pool_capacity/);
});

test('WinLibs consumes whole-module qualification across backend declarations and pool operations', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const output = compileSources({
    'a_owner.form': `module owner {
  const BASE: u32 = 5;
  enum mode { ready = 1, done = 2 }
  struct item { value: u32; }
  pool items: item[3];
  global marker: u32 = 7;
  fn echo(value: u32) -> u32 { return value; }
}\n`,
    'b_user.form': `module user {
  import owner;
  global observed: u32 = 0;
  system qualified every 1 ticks reads owner.marker, owner.items writes owner.items, observed {
    spawn(owner.items, owner.item { value = owner.BASE });
    let selected = owner.mode.ready;
    observed = owner.echo(owner.marker) + owner.BASE;
    kill(owner.items, 0);
  }
}\n`,
  });
  const source = byPath(output, 'user.cpp');
  assert.match(source, /state\.owner\.items\._form_pool_count/);
  assert.match(source, /form::owner::item/);
  assert.match(source, /owner::echo\(_form_value_0\)/);
  assert.match(source, /_form_value_0 = state\.owner\.marker/);
  const run = runGeneratedNative(output, `
#include "form_game.hpp"
int main() {
  form::FormState state{};
  form::initialize(state, 1u);
  const form::PadFrame pads[4]{};
  form::sim_tick(state, pads, 0u);
  if (state.user.observed != 12u) return 1;
  if (state.owner.items._form_pool_count != 0u) return 2;
  return 0;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
});

test('WinLibs compiles imported nominal values without direct type imports', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const output = compileSources({
    'a_owner.form': `module owner {
  struct hidden_record { value: u32; }
  enum hidden_mode { ready = 1, done = 2 }
  global payload_state: hidden_record = hidden_record { value = 7 };
  global current: hidden_mode = hidden_mode.ready;
  pool entries: hidden_record[3];
  fn echo(value: hidden_record) -> hidden_record { return value; }
  fn echo_mode(value: hidden_mode) -> hidden_mode { return value; }
}
`,
    'b_consumer.form': `module consumer {
  import owner { payload_state, current, entries, echo, echo_mode };
  system use_imports every 1 ticks reads payload_state, current, entries writes payload_state {
    payload_state = echo(payload_state);
    let selected = echo_mode(current);
    let same = selected == current;
    for entry in entries { let observed = entry.value; }
  }
}
`,
  });
  const run = runGeneratedNative(output, `#include "form_game.hpp"
int main() {
  form::FormState state{};
  form::initialize(state, 1u);
  state.owner.entries._form_pool_count = 1u;
  state.owner.entries._form_pool_column_76616c7565[0] = 99u;
  form::PadFrame pads[4]{};
  form::sim_tick(state, pads, 0u);
  return state.owner.payload_state.value == 7u ? 0 : 1;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
});

test('WinLibs compiles namespace-safe aggregate and exact enum constants', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  // L1 has array types but no array literal or other array-valued constant
  // expression, so there is no source-level array constant to exercise here.
  const output = compileSources({
    'a_owner.form': `module owner {
  enum motion_mode { waiting = 2, moving = 7 }
  struct imported_record { point: world2; shade: colour8; selected: motion_mode; scaled: fx16; }
  const SELECTED: motion_mode = motion_mode.moving;
  const IMPORTED: imported_record = imported_record {
    selected = SELECTED,
    scaled = 1m / (5m - 0.5m),
    shade = #102030,
    point = world2 { y = 2w, x = 1w },
  };
  const SCALE: fx16 = IMPORTED.scaled;
}\n`,
    'b_consumer.form': `module consumer {
  import owner { motion_mode, imported_record, SELECTED, IMPORTED };
  struct local_record {
    imported: imported_record;
    position: world3;
    velocity: velocity3;
    tint: colour8;
    selected: motion_mode;
  }
  const LOCAL: local_record = local_record {
    selected = SELECTED,
    tint = #80402010,
    velocity = velocity3 { z = -3w, x = -1w, y = -2w },
    position = world3 { z = 6w, y = 5w, x = 4w },
    imported = IMPORTED,
  };
  const COPY: local_record = LOCAL;
  global initial: local_record = COPY;
}\n`,
  });
  const ownerHeader = byPath(output, 'owner.hpp');
  const consumerHeader = byPath(output, 'consumer.hpp');
  assert.doesNotMatch(ownerHeader + consumerHeader, /\[&\]/);
  assert.match(ownerHeader,
    /inline constexpr form::owner::motion_mode SELECTED = static_cast<form::owner::motion_mode>\(7u\);/);
  assert.match(ownerHeader,
    /imported_record\{World2\{16777216LL, 33554432LL\}, 4279246896u, static_cast<form::owner::motion_mode>\(7u\), 14564\}/);
  assert.match(consumerHeader,
    /local_record\{form::owner::IMPORTED, World3\{67108864LL, 83886080LL, 100663296LL\}, Velocity3\{-16777216LL, -33554432LL, -50331648LL\}, 2151686160u, static_cast<form::owner::motion_mode>\(7u\)\}/);

  const run = runGeneratedNative(output, `#include "form_game.hpp"
static_assert(form::owner::SELECTED == form::owner::motion_mode::moving);
static_assert(form::owner::IMPORTED.point.x == 0x1000000LL);
static_assert(form::owner::IMPORTED.point.y == 0x2000000LL);
static_assert(form::owner::IMPORTED.scaled == 14564);
static_assert(form::owner::SCALE == form::owner::IMPORTED.scaled);
static_assert(form::consumer::LOCAL.imported.shade == 0xff102030u);
static_assert(form::consumer::LOCAL.position.z == 0x6000000LL);
static_assert(form::consumer::LOCAL.velocity.z == -0x3000000LL);
static_assert(form::consumer::LOCAL.tint == 0x80402010u);
static_assert(form::consumer::COPY.selected == form::owner::motion_mode::moving);
int main() {
  form::FormState state{};
  form::initialize(state, 1u);
  return state.consumer.initial.position.y == 0x5000000LL
      && state.consumer.initial.imported.point.x == 0x1000000LL ? 0 : 1;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
});

test('C++ declaration order follows enum, struct, and constant dependencies', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const output = compileSources({
    'ordered.form': `module ordered {
  const DERIVED: u32 = BASE + 1;
  struct outer { payload: inner; selected: mode; }
  const BASE: u32 = 7;
  struct inner { value: u32; }
  enum mode { ready = 1, done = 2 }
  const RECORD: outer = outer { payload = inner { value = DERIVED }, selected = mode.ready };
  global state_value: outer = RECORD;
}\n`,
  });
  const header = byPath(output, 'ordered.hpp');
  const mode = header.indexOf('enum class mode');
  const inner = header.indexOf('struct inner');
  const outer = header.indexOf('struct outer');
  const base = header.indexOf(' BASE =');
  const derived = header.indexOf(' DERIVED =');
  const record = header.indexOf(' RECORD =');
  assert.ok(mode >= 0 && mode < inner && inner < outer);
  assert.ok(base >= 0 && base < derived && derived < record);
  const run = runGeneratedNative(output, `
#include "form_game.hpp"
static_assert(form::ordered::BASE == 7u);
static_assert(form::ordered::DERIVED == 8u);
static_assert(form::ordered::RECORD.payload.value == 8u);
static_assert(form::ordered::RECORD.selected == form::ordered::mode::ready);
int main() { form::FormState state{}; form::initialize(state, 1u); return state.ordered.state_value.payload.value == 8u ? 0 : 1; }
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
});

test('native composition preserves nested members and indices while pool layout names cannot collide', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const output = compileSources({
    'composition.form': `module composition {
  const TWO: u32 = 2;
  struct inner { value: u32; }
  struct outer { payload: inner; }
  struct row { count: u32; capacity: u32; _form_pool_count: u32; lanes: u32[TWO]; }
  const BASE: outer = outer { payload = inner { value = 7 } };
  const FROM_NESTED: u32 = BASE.payload.value;
  global box: outer = BASE;
  global observed: u32 = 0;
  pool items: row[3];
  fn inspect(item: outer) -> u32 { return item.payload.value; }
  system mutate every 1 ticks
    reads box, items, items.count, items.capacity, items._form_pool_count, items.lanes
    writes observed, items, items.count, items.capacity, items._form_pool_count, items.lanes
  {
    for i in 0..items.count {
      items.count[i] = items.count[i] + 1;
      items.capacity[i] = items.capacity[i] + 2;
      items._form_pool_count[i] = items._form_pool_count[i] + 3;
      items.lanes[i][1] = items.lanes[i][1] + 4;
    }
    observed = inspect(box);
    kill(items, 0);
  }
}\n`,
  });
  const header = byPath(output, 'composition.hpp');
  assert.match(header, /u32 _form_pool_count\{\}/);
  assert.match(header, /_form_pool_column_636f756e74/);
  assert.match(header, /_form_pool_column_6361706163697479/);
  assert.match(header, /_form_pool_column_5f666f726d5f706f6f6c5f636f756e74/);
  assert.doesNotMatch(header, /std::array<u32, _form_pool_capacity> (?:count|capacity|_form_pool_count)\{/);
  const source = byPath(output, 'composition.cpp');
  assert.match(source, /_form_pool_column_6c616e6573.*checked_index/s);
  assert.match(source, /_form_index_values\.size\(\)/);
  const run = runGeneratedNative(output, `
#include "form_game.hpp"
int main() {
  form::FormState state{};
  form::initialize(state, 1u);
  auto& pool = state.composition.items;
  pool._form_pool_count = 2u;
  pool._form_pool_column_636f756e74[0] = 10u;
  pool._form_pool_column_636f756e74[1] = 20u;
  pool._form_pool_column_6361706163697479[0] = 30u;
  pool._form_pool_column_6361706163697479[1] = 40u;
  pool._form_pool_column_5f666f726d5f706f6f6c5f636f756e74[0] = 50u;
  pool._form_pool_column_5f666f726d5f706f6f6c5f636f756e74[1] = 60u;
  pool._form_pool_column_6c616e6573[0][1] = 70u;
  pool._form_pool_column_6c616e6573[1][1] = 80u;
  const form::PadFrame pads[4]{};
  form::sim_tick(state, pads, 0u);
  if (form::composition::FROM_NESTED != 7u || state.composition.observed != 7u) return 1;
  if (pool._form_pool_count != 1u) return 2;
  if (pool._form_pool_column_636f756e74[0] != 21u ||
      pool._form_pool_column_6361706163697479[0] != 42u ||
      pool._form_pool_column_5f666f726d5f706f6f6c5f636f756e74[0] != 63u ||
      pool._form_pool_column_6c616e6573[0][1] != 84u) return 3;
  return 0;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
});

test('WinLibs compiles the full W3.2 positive surface after physical sites are isolated', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const dir = path.join(repoRoot(), 'compiler', 'tests', 'frontend', 'corpus', 'positive');
  const names = ['declarations.form', 'fields.form', 'library.form', 'main.form', 'presentation.form', 'scenario.form', 'systems.form'];
  const sources = Object.fromEntries(names.map((name) => [name, readFileSync(path.join(dir, name), 'utf8')]));
  sources['systems.form'] = sources['systems.form']!
    .replace(
      '    shatter_storm(shards, shatter_params { threshold = 1m, bias = 0.5m });',
      '    for i in 0..shards.count { shards.age[i] = shards.age[i] + 1; }',
    )
    .replace(
      `    apply terrain_field rising_ridge(
      origin: world2 { x = 0w, y = 0w },
      params: ridge_params { amplitude = 2m, width = 8m }
    ) duration 45t;
`,
      '',
    );
  const output = compileSources(sources);
  const run = runGeneratedNative(output, `#include "form_game.hpp"
int main() {
  form::FormState state{};
  form::initialize(state, 0x11223344u);
  if (form::systems::lift(-0x10000) != 0x10000) return 1;
  zhao::ZhaoFrameBuilder builder;
  form::present_frame(state, builder);
  return builder.command_count() == 8u ? 0 : 2;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
});

test('WinLibs decodes every presentation record and renders visible generated resources', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const output = compileSources({
    'gallery.form': `module gallery {
  struct mote { position: world3; }
  pool motes: mote[4];
  const FORM_PAGE: u32 = 12;
  const PATCH_PAGE: u32 = 3;
  const BRUSH_PAGE: u32 = 7;
  global camera: world3 = world3 { x = 0w, y = 0w, z = 0w };
  global authored: world3 = world3 { x = 0.25w, y = -0.25w, z = 1w };
  sound chime {
    sample "tone/chime.ztone";
    gain 75%;
    pitch 1.0;
    pan -123;
  }
  presentation main {
    view 0 from camera budget 90%;
    shared budget 10%;
    emit draw_form(form: FORM_PAGE, transform: authored, view_mask: 1, weight: 100%);
    emit draw_population(pool: motes, view_mask: 1, weight: 80%);
    emit draw_procedural(patch: PATCH_PAGE, transform: authored, screen_error: 0.5m);
    emit surface_stamp(brush: BRUSH_PAGE, at: authored, radius: 2m, ring_width: 0.5m, tag: 5, strength: 80%);
    emit audio(sound: chime, at: authored);
  }
}
`,
  });
  const repo = repoRoot();
  const renderSources = [
    'reference/src/zfield/zfield_decode.cpp',
    'reference/src/zfield/zfield_interpret.cpp',
    'reference/src/zterrain/terrain_core.cpp',
    'reference/src/zrender/render_frame.cpp',
    'reference/src/zrender/rast.cpp',
    'reference/src/zrender/terrain.cpp',
    'reference/src/zrender/sprites.cpp',
    'reference/src/zrender/resolve.cpp',
    'reference/src/zsky/emit_layers.cpp',
  ].map((file) => path.join(repo, file));
  const run = runGeneratedNative(output, `#include "form_game.hpp"
#include <zref/zref_render.hpp>
#include <cstdio>
struct Bound {
  form::u32 form_calls{};
  form::u32 population_calls{};
  form::u32 audio_calls{};
  form::u32 terrain_calls{};
  form::u32 transform_handle{};
  form::u32 population_handle{};
  form::u32 patch_handle{};
  form::u32 patch_source{};
  form::u32 audio_source{};
  form::Fx16 patch_radius{};
  form::World3 form_at{};
  form::World3 patch_at{};
  form::World3 audio_at{};
  zref::render::FormTransform transform{};
  zref::render::Population population{};
  zref::render::TerrainPatch patch{};
  zref::render::RenderResources render{};
};
static void bind_form(void* raw, form::u32 handle, form::World3 at, form::Fx16 size) {
  Bound& b = *static_cast<Bound*>(raw);
  ++b.form_calls;
  b.transform_handle = handle;
  b.form_at = at;
  b.transform = {form::fx16_from_fx24(at.x), form::fx16_from_fx24(at.y), form::fx16_from_fx24(at.z), size};
  b.render.transforms.push_back({handle, b.transform});
}
static void bind_population(void* raw, form::u32 handle, form::u32 module, form::u32 index,
                            form::u32 count, const void* pool) {
  Bound& b = *static_cast<Bound*>(raw);
  ++b.population_calls;
  b.population_handle = handle;
  (void)module; (void)index; (void)count; (void)pool;
  b.population.parts.push_back({1 << 14, -(1 << 14), 1 << 16, 64u, 0u, 255u, 0u});
  b.render.populations.push_back({handle, b.population});
}
static void bind_audio(void* raw, form::u32 source, form::World3 at) {
  Bound& b = *static_cast<Bound*>(raw);
  ++b.audio_calls;
  b.audio_source = source;
  b.audio_at = at;
}
static void bind_terrain(void* raw, form::u32 handle, form::u32 source,
                         form::World3 at, form::Fx16 radius) {
  Bound& b = *static_cast<Bound*>(raw);
  ++b.terrain_calls;
  b.patch_handle = handle;
  b.patch_source = source;
  b.patch_at = at;
  b.patch_radius = radius;
  b.render.terrain_patches.push_back({handle, &b.patch});
}
static form::u16 read16(const form::u8* p) { return static_cast<form::u16>(p[0]) | static_cast<form::u16>(p[1] << 8u); }
int main() {
  form::FormState state{};
  form::initialize(state, 0x1234u);
  state.gallery.motes._form_pool_count = 1u;
  Bound bound{};
  zref::render::FormPattern pattern{};
  for (form::u32 i = 0u; i < 64u; ++i) {
    pattern.mask[i] = 1u;
    pattern.rgb[i * 3u] = 255u;
  }
  bound.render.forms.push_back({0x0100000cu, pattern});
  bound.patch.width = 2u;
  bound.patch.height = 2u;
  bound.patch.env_x0 = -(4 << 16);
  bound.patch.env_z0 = -(4 << 16);
  bound.patch.env_x1 = 4 << 16;
  bound.patch.env_z1 = 4 << 16;
  bound.patch.heights.assign(4u, 0);
  bound.render.terrain_patches.push_back({0x01000003u, &bound.patch});
  bound.render.materials.push_back({0x01000003u, zref::render::Material{96u, 128u, 160u}});
  const form::PresentationResources bindings{&bound, &bind_form, &bind_population, &bind_audio, &bind_terrain};
  const form::u32 truth_before = form::sim_hash(0x55aa55aau, state);
  zhao::ZhaoFrameBuilder builder;
  builder.begin_frame(7u, 1u, 0u, 0u);
  form::present_frame(state, builder, bindings);
  builder.end_frame(zhao_abi::ZHAO_COMPL_DONE);
  const form::u32 truth_after = form::sim_hash(0x55aa55aau, state);
  if (truth_before != truth_after || builder.command_count() != 9u || builder.command_bytes() != 432u) return 30;
  if (bound.form_calls != 1u || bound.population_calls != 1u || bound.audio_calls != 1u ||
      bound.terrain_calls != 1u) return 31;
  constexpr form::Fx24 x = 1LL << 22;
  constexpr form::Fx24 y = -(1LL << 22);
  constexpr form::Fx24 z = 1LL << 24;
  if (bound.form_at.x != x || bound.form_at.y != y || bound.form_at.z != z ||
      bound.patch_at.x != x || bound.patch_at.y != y || bound.patch_at.z != z ||
      bound.audio_at.x != x || bound.audio_at.y != y || bound.audio_at.z != z ||
      bound.transform_handle == 0u || bound.population_handle == 0u || bound.patch_handle == 0u ||
      bound.patch_source == 0u || bound.patch_radius != 0x20000 || bound.audio_source == 0u) return 32;

  const std::vector<form::u8> packet = builder.seal(7u, 1u, 1u);
  const auto valid = zhao::zhao_frame_validate(packet);
  if (valid.error != zhao_abi::ZH_ABI_OK) return 33;
  const auto header = zhao::zhao_frame_parse_header(packet.data());
  form::u32 seen = 0u;
  for (form::u32 off = zhao_abi::ZHAO_FRAME_HEADER_BYTES;
       off < zhao_abi::ZHAO_FRAME_HEADER_BYTES + header.command_bytes;) {
    const form::u16 opcode = read16(packet.data() + off);
    const form::u16 bytes = read16(packet.data() + off + 2u);
    zhao_abi::ZhReader reader(packet.data() + off, bytes);
    if (opcode == zhao_abi::ZHAO_OP_DRAW_FORM) {
      zhao_abi::ZhRecordDrawForm r{};
      if (!zhao_abi::zhao_unpack_draw_form(reader, r) || r.payload.form != 0x0100000cu ||
          r.payload.material_set != r.payload.form || r.payload.transform == 0u ||
          r.payload.viewport_mask != 1u || r.payload.semantic_weight != 255u || r.payload.flags != 1u) return 34;
      seen |= 1u;
    } else if (opcode == zhao_abi::ZHAO_OP_DRAW_POPULATION) {
      zhao_abi::ZhRecordDrawPopulation r{};
      if (!zhao_abi::zhao_unpack_draw_population(reader, r) || r.payload.population != 0x01000001u ||
          r.payload.viewport_mask != 1u || r.payload.semantic_weight != 205u || r.payload.flags != 1u) return 35;
      seen |= 2u;
    } else if (opcode == zhao_abi::ZHAO_OP_DRAW_PROCEDURAL) {
      zhao_abi::ZhRecordDrawProcedural r{};
      if (!zhao_abi::zhao_unpack_draw_procedural(reader, r) || r.payload.program != 0x01000003u ||
          r.payload.material != r.payload.program || r.payload.transform.tx != 0x4000 ||
          r.payload.transform.ty != 0x10000 || r.payload.transform.r00 != 0x10000 ||
          r.payload.transform.r11 != 0x10000 || r.payload.screen_error != 0x8000 ||
          r.payload.kind != zhao_abi::FORGE_HEIGHTFIELD_PATCH) return 36;
      seen |= 4u;
    } else if (opcode == zhao_abi::ZHAO_OP_SURFACE_STAMP) {
      zhao_abi::ZhRecordSurfaceStamp r{};
      if (!zhao_abi::zhao_unpack_surface_stamp(reader, r) || r.payload.brush != 0x01000007u ||
          r.payload.patch != bound.patch_handle || r.payload.operation != 0u || r.payload.tag != 5u ||
          r.payload.strength != 52685u || r.payload.transform.tx != 0x4000 ||
          r.payload.transform.ty != 0x10000 || r.payload.transform.r00 != 0x10000 ||
          r.payload.transform.r11 != 0x10000 || r.payload.radius != 0x20000 ||
          r.payload.ring_width != 0x8000) return 37;
      seen |= 8u;
    } else if (opcode == zhao_abi::ZHAO_OP_EMIT_AUDIO_EVENT) {
      zhao_abi::ZhRecordEmitAudioEvent r{};
      if (!zhao_abi::zhao_unpack_emit_audio_event(reader, r) || r.payload.event_id != 0u ||
          r.payload.pan_fx != -123 || r.payload.gain != 49344u ||
          r.payload.sample_handle != form::authored_resource_handle(form::AuthoredResourceRole::Sound, 1u) || r.payload.timestamp != 0u) return 38;
      seen |= 16u;
    }
    off += bytes;
  }
  if (seen != 31u) return 39;

  zref::render::SoftwareRenderer renderer;
  zref::render::RenderCanvas canvas;
  const auto rendered = renderer.render_frame(packet, 0u, canvas, bound.render);
  if (rendered.status != zhao_abi::ZH_ABI_OK || rendered.resource_misses != 0u) return 40;
  if (renderer.sheets().size() != 1u || renderer.sheets()[0].first != bound.patch_handle) return 41;
  form::u32 stamped = 0u;
  for (form::u32 i = 0u; i < 64u * 64u; ++i) {
    if (renderer.sheets()[0].second.tag[i] == 5u && renderer.sheets()[0].second.strength[i] != 0u) ++stamped;
  }
  if (stamped == 0u) return 42;
  form::u32 visible = 0u;
  for (form::u32 i = 0u; i < zref::render::canvas_bytes(zhao_abi::VIDEO_Z60); ++i)
    if (canvas.slot[0][i] != 0u) ++visible;
  if (visible == 0u) return 43;
  std::printf("%u %08x\\n", visible, rendered.canvas_crc32c);
  return 0;
}
`, renderSources, [
    '-Wno-error=unused-parameter',
    '-Wno-error=unused-variable',
    '-Wno-error=shift-negative-value',
  ]);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
  assert.match(run.stdout.replaceAll('\r', ''), /^[1-9][0-9]* [0-9a-f]{8}\n$/);
});

test('module 0 and 256 presentation sites retain distinct persistent resources', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const sources: Record<string, string> = {};
  for (let module = 0; module <= 256; ++module) {
    const suffix = module.toString().padStart(3, '0');
    const name = `m${suffix}`;
    sources[`${suffix}_${name}.form`] = module === 0 || module === 256
      ? `module ${name} {
  const FORM_PAGE: u32 = 5;
  const BRUSH_PAGE: u32 = 7;
  global camera: world3 = world3 { x = 0w, y = 0w, z = 0w };
  global authored: world3 = world3 { x = ${module === 0 ? '-1w' : '1w'}, y = 0w, z = 1w };
  presentation showcase {
    view 0 from camera budget 45%;
    view 1 from camera budget 45%;
    shared budget 10%;
    emit draw_form(form: FORM_PAGE, transform: authored, view_mask: 3, weight: 100%);
    emit surface_stamp(brush: BRUSH_PAGE, at: authored, radius: 2m, ring_width: 0.5m,
                       tag: ${module === 0 ? 17 : 29}, strength: 80%);
  }
}\n`
      : `module ${name} { }\n`;
  }
  const first = compileSources(sources);
  const second = compileSources(Object.fromEntries(Object.entries(sources).reverse()));
  assert.deepEqual(first, second);
  const types = byPath(first, 'form_types.hpp');
  assert.doesNotMatch(types, /transient_handle/);
  assert.match(types, /std::array<PresentationResourceBinding, 4>/);
  assert.match(types, /PresentationResourceBinding\{0u, [0-9]+u, PresentationResourceRole::DrawFormTransform/);
  assert.match(types, /PresentationResourceBinding\{256u, [0-9]+u, PresentationResourceRole::SurfaceStampTerrainPatch/);

  const nativeOutput: typeof first = {
    ...first,
    files: first.files.filter((file) => !file.path.endsWith('.cpp')
      || file.path.endsWith('/m000.cpp') || file.path.endsWith('/m256.cpp')),
  };
  const repo = repoRoot();
  const renderSources = [
    'reference/src/zfield/zfield_decode.cpp',
    'reference/src/zfield/zfield_interpret.cpp',
    'reference/src/zterrain/terrain_core.cpp',
    'reference/src/zrender/render_frame.cpp',
    'reference/src/zrender/rast.cpp',
    'reference/src/zrender/terrain.cpp',
    'reference/src/zrender/sprites.cpp',
    'reference/src/zrender/resolve.cpp',
    'reference/src/zsky/emit_layers.cpp',
  ].map((file) => path.join(repo, file));
  const run = runGeneratedNative(nativeOutput, `#include "form_game.hpp"
#include <zref/zref_render.hpp>
#include <array>
#include <vector>
struct Bound {
  bool valid{true};
  form::u32 form_calls{};
  form::u32 terrain_calls{};
  std::array<form::u32, 2> transform_handles{};
  std::array<form::u32, 2> patch_handles{};
  std::array<form::u32, 2> patch_sources{};
  std::array<zref::render::FormTransform, 2> transforms{};
  std::array<zref::render::TerrainPatch, 2> patches{};
  zref::render::RenderResources render{};
};
static form::u32 slot_for_position(form::World3 at) { return at.x < 0 ? 0u : 1u; }
static form::u32 slot_for_source(form::u32 source) {
  const form::u32 module = (source >> 16u) & 0xfffu;
  return module == 0u ? 0u : module == 256u ? 1u : 2u;
}
static void bind_form(void* raw, form::u32 handle, form::World3 at, form::Fx16 size) {
  Bound& b = *static_cast<Bound*>(raw);
  const form::u32 slot = slot_for_position(at);
  if (b.transform_handles[slot] != 0u && b.transform_handles[slot] != handle) b.valid = false;
  if (b.transform_handles[slot] == 0u) {
    b.transform_handles[slot] = handle;
    b.transforms[slot] = {form::fx16_from_fx24(at.x), form::fx16_from_fx24(at.y),
                          form::fx16_from_fx24(at.z), size};
    b.render.transforms.push_back({handle, b.transforms[slot]});
  }
  ++b.form_calls;
}
static void bind_terrain(void* raw, form::u32 handle, form::u32 source,
                         form::World3 at, form::Fx16 radius) {
  Bound& b = *static_cast<Bound*>(raw);
  const form::u32 slot = slot_for_source(source);
  if (slot > 1u || slot != slot_for_position(at) || radius != 0x20000) { b.valid = false; return; }
  if ((b.patch_handles[slot] != 0u && b.patch_handles[slot] != handle) ||
      (b.patch_sources[slot] != 0u && b.patch_sources[slot] != source)) b.valid = false;
  if (b.patch_handles[slot] == 0u) {
    b.patch_handles[slot] = handle;
    b.patch_sources[slot] = source;
    b.render.terrain_patches.push_back({handle, &b.patches[slot]});
  }
  ++b.terrain_calls;
}
static form::u16 read16(const form::u8* p) {
  return static_cast<form::u16>(p[0]) | static_cast<form::u16>(p[1] << 8u);
}
struct Decoded {
  std::array<form::u32, 2> stamp_sources{};
  std::array<form::u32, 2> stamp_handles{};
  std::array<form::u32, 2> draw_sources{};
  std::array<form::u32, 2> transform_handles{};
  form::u32 stamps{};
  form::u32 draws{};
  form::u32 view_zero{};
  form::u32 view_one{};
};
static bool decode(const std::vector<form::u8>& packet, Decoded& out) {
  const auto valid = zhao::zhao_frame_validate(packet);
  if (valid.error != zhao_abi::ZH_ABI_OK) return false;
  const auto header = zhao::zhao_frame_parse_header(packet.data());
  for (form::u32 off = zhao_abi::ZHAO_FRAME_HEADER_BYTES;
       off < zhao_abi::ZHAO_FRAME_HEADER_BYTES + header.command_bytes;) {
    const form::u16 opcode = read16(packet.data() + off);
    const form::u16 bytes = read16(packet.data() + off + 2u);
    zhao_abi::ZhReader reader(packet.data() + off, bytes);
    if (opcode == zhao_abi::ZHAO_OP_SET_VIEW) {
      zhao_abi::ZhRecordSetView record{};
      if (!zhao_abi::zhao_unpack_set_view(reader, record)) return false;
      if (record.payload.view_id == 0u) ++out.view_zero;
      if (record.payload.view_id == 1u) ++out.view_one;
    } else if (opcode == zhao_abi::ZHAO_OP_DRAW_FORM) {
      zhao_abi::ZhRecordDrawForm record{};
      if (!zhao_abi::zhao_unpack_draw_form(reader, record)) return false;
      const form::u32 slot = slot_for_source(record.hdr.source_id);
      if (slot > 1u) return false;
      out.draw_sources[slot] = record.hdr.source_id;
      out.transform_handles[slot] = record.payload.transform;
      ++out.draws;
    } else if (opcode == zhao_abi::ZHAO_OP_SURFACE_STAMP) {
      zhao_abi::ZhRecordSurfaceStamp record{};
      if (!zhao_abi::zhao_unpack_surface_stamp(reader, record)) return false;
      const form::u32 slot = slot_for_source(record.hdr.source_id);
      if (slot > 1u) return false;
      out.stamp_sources[slot] = record.hdr.source_id;
      out.stamp_handles[slot] = record.payload.patch;
      ++out.stamps;
    }
    off += bytes;
  }
  return true;
}
static std::vector<form::u8> frame(form::FormState& state, const form::PresentationResources& resources,
                                   form::u32 number) {
  zhao::ZhaoFrameBuilder builder;
  builder.begin_frame(number, 1u, 0u, 0u);
  form::present_frame(state, builder, resources);
  builder.end_frame(zhao_abi::ZHAO_COMPL_DONE);
  return builder.seal(number, 1u, 1u);
}
int main() {
  static_assert(form::kPresentationResourceBindings.size() == 4u);
  form::FormState state{};
  form::initialize(state, 0x1234u);
  Bound bound{};
  for (auto& patch : bound.patches) {
    patch.width = 2u; patch.height = 2u;
    patch.env_x0 = -(4 << 16); patch.env_z0 = -(4 << 16);
    patch.env_x1 = 4 << 16; patch.env_z1 = 4 << 16;
    patch.heights.assign(4u, 0);
  }
  zref::render::FormPattern pattern{};
  for (form::u32 i = 0u; i < 64u; ++i) {
    pattern.mask[i] = 1u;
    pattern.rgb[i * 3u] = 255u;
  }
  bound.render.forms.push_back({0x01000005u, pattern});
  const form::PresentationResources resources{&bound, &bind_form, nullptr, nullptr, &bind_terrain};
  const std::vector<form::u8> first = frame(state, resources, 10u);
  const std::vector<form::u8> second = frame(state, resources, 11u);
  Decoded a{};
  Decoded b{};
  if (!decode(first, a) || !decode(second, b) || !bound.valid) return 1;
  if (a.draws != 2u || a.stamps != 2u || a.view_zero != 2u || a.view_one != 2u) return 2;
  if (a.draw_sources != b.draw_sources || a.stamp_sources != b.stamp_sources ||
      a.transform_handles != b.transform_handles || a.stamp_handles != b.stamp_handles) return 3;
  if (((a.draw_sources[0] >> 28u) != 9u) || ((a.draw_sources[1] >> 28u) != 9u) ||
      (((a.draw_sources[0] >> 16u) & 0xfffu) != 0u) ||
      (((a.draw_sources[1] >> 16u) & 0xfffu) != 256u) ||
      ((a.draw_sources[0] & 0xffffu) != (a.draw_sources[1] & 0xffffu)) ||
      ((a.stamp_sources[0] & 0xffffu) != (a.stamp_sources[1] & 0xffffu))) return 4;
  if (a.transform_handles[0] == a.transform_handles[1] ||
      a.stamp_handles[0] == a.stamp_handles[1] ||
      a.transform_handles[0] == a.stamp_handles[0] ||
      a.transform_handles[1] == a.stamp_handles[1]) return 5;
  if (bound.form_calls != 4u || bound.terrain_calls != 4u ||
      bound.render.transforms.size() != 2u || bound.render.terrain_patches.size() != 2u ||
      bound.patch_handles != a.stamp_handles || bound.transform_handles != a.transform_handles) return 6;
  std::array<bool, 4> mapped{};
  for (const auto& binding : form::kPresentationResourceBindings) {
    const form::u32 slot = binding.module == 0u ? 0u : binding.module == 256u ? 1u : 2u;
    if (slot > 1u) return 7;
    if (binding.role == form::PresentationResourceRole::DrawFormTransform) {
      if (binding.source_id != a.draw_sources[slot] || binding.handle != a.transform_handles[slot]) return 8;
      mapped[slot] = true;
    } else {
      if (binding.source_id != a.stamp_sources[slot] || binding.handle != a.stamp_handles[slot]) return 9;
      mapped[2u + slot] = true;
    }
  }
  if (!mapped[0] || !mapped[1] || !mapped[2] || !mapped[3]) return 10;
  zref::render::SoftwareRenderer renderer;
  zref::render::RenderCanvas canvas{};
  const auto rendered_first = renderer.render_frame(first, 0u, canvas, bound.render);
  if (rendered_first.status != zhao_abi::ZH_ABI_OK || rendered_first.resource_misses != 0u ||
      renderer.sheets().size() != 2u) return 11;
  const auto rendered_second = renderer.render_frame(second, 0u, canvas, bound.render);
  if (rendered_second.status != zhao_abi::ZH_ABI_OK || rendered_second.resource_misses != 0u ||
      renderer.sheets().size() != 2u) return 12;
  std::array<bool, 2> stamped{};
  for (const auto& sheet : renderer.sheets()) {
    const form::u32 slot = sheet.first == bound.patch_handles[0] ? 0u
      : sheet.first == bound.patch_handles[1] ? 1u : 2u;
    if (slot > 1u) return 13;
    const form::u8 tag = slot == 0u ? 17u : 29u;
    for (form::u32 i = 0u; i < 64u * 64u; ++i) {
      if (sheet.second.tag[i] == tag && sheet.second.strength[i] != 0u) stamped[slot] = true;
    }
  }
  return stamped[0] && stamped[1] ? 0 : 14;
}
`, renderSources, [
    '-Wno-error=unused-parameter',
    '-Wno-error=unused-variable',
    '-Wno-error=shift-negative-value',
  ]);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
});

test('WinLibs matches independent fixed-point, intrinsic, eager-order, and RNG oracles', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const output = compileSources({
    'oracle.form': `module oracle {
  struct pair { first: u32; second: u32; }
  struct cell { value: u32; }
  pool cells: cell[2];
  global held: bool = false;
  global terrain_sample: fx16 = 0m;
  global mixed: fx16 = 0m;
  global ru: u32 = 0;
  global ri: i32 = 0;
  global runit: unit8 = 0%;
  global rangle: angle16 = 0turn;
  global rfx: fx16 = 0m;
  global ordered: u32 = 0;
  global eager_and: bool = false;
  global eager_or: bool = false;
  global selected: u32 = 0;
  global called: u32 = 0;
  global record_first: u32 = 0;
  global record_second: u32 = 0;
  global other_stream: u32 = 0;

  fn difference(a: u32, b: u32) -> u32 { return a - b; }
  fn div16(a: fx16, b: fx16) -> fx16 { return a / b; }
  fn div24(a: fx24, b: fx24) -> fx24 { return a / b; }
  fn choose_min(a: fx16, b: fx16) -> fx16 { return min(a, b); }
  fn choose_max(a: fx16, b: fx16) -> fx16 { return max(a, b); }
  fn choose_clamp(a: fx16, lo: fx16, hi: fx16) -> fx16 { return clamp(a, lo, hi); }
  fn magnitude(a: i32) -> i32 { return abs(a); }
  fn sine(a: angle16) -> fx16 { return sin(a); }
  fn cosine(a: angle16) -> fx16 { return cos(a); }
  fn angle_of(y: fx16, x: fx16) -> angle16 { return atan2_approx(y, x); }
  fn root(a: fx16) -> fx16 { return sqrt_approx(a); }
  fn narrow(a: fx24) -> fx16 { return to_fx16(a); }
  fn widen(a: fx16) -> fx24 { return to_fx24(a); }
  fn unit(a: fx16) -> unit8 { return to_unit8(a); }
  fn angle(a: fx16) -> angle16 { return to_angle16(a); }
  fn scalar_angle(a: angle16) -> fx16 { return to_fx16(a); }
  fn scalar_unit(a: unit8) -> fx16 { return to_fx16(a); }
  fn product2(a: world2, b: world2) -> fx24 { return dot2(a, b); }
  fn product3(a: world3, b: world3) -> fx24 { return dot3(a, b); }
  fn vector_length(a: world3) -> fx16 { return length(a); }
  fn vector_normal(a: world3) -> world3 { return normalize(a); }

  system exercise every 1 ticks reads input, terrain, cells writes held, terrain_sample, mixed, ru, ri, runit, rangle, rfx, ordered, eager_and, eager_or, selected, called, record_first, record_second, other_stream, cells.value {
    let s0 = random.stream(17, 4);
    ru = random.u32(s0);
    ri = random.i32(s0);
    runit = random.unit8(s0);
    rangle = random.angle16(s0);
    rfx = random.fx16(s0, -1m, 1m);
    ordered = random.u32(s0) - random.u32(s0);
    eager_and = (random.u32(s0) != 0) && (random.u32(s0) != 0);
    eager_or = (random.u32(s0) != 0) || (random.u32(s0) != 0);
    selected = if true { random.u32(s0) } else { random.u32(s0) };
    called = difference(random.u32(s0), random.u32(s0));
    let record = pair { second = random.u32(s0), first = random.u32(s0) };
    record_first = record.first;
    record_second = record.second;
    cells.value[random.u32(s0) & 1] = random.u32(s0);
    let s1 = random.stream(99, 7);
    other_stream = random.u32(s1);
    held = input.held(input.player(2), BTN_A);
    terrain_sample = terrain.height(world2 { x = 2w, y = 3w });
    mixed = mix(1m, 3m, 50%);
  }

  scenario seeded_run { seed 1234; }
}
`,
  });

  const div16Inputs: [bigint, bigint][] = [
    [1n, 131072n], [-1n, 131072n], [1n, -131072n], [-1n, -131072n],
    [0x7fffffffn, 1n], [-0x80000000n, 1n], [1n, 0n], [-1n, 0n], [0n, 0n],
  ];
  const div24Inputs: [bigint, bigint][] = [
    [1n, 33554432n], [-1n, 33554432n], [1n, -33554432n], [-1n, -33554432n],
    [1n, 0n], [-1n, 0n],
  ];
  const expectedDiv16 = div16Inputs.map(([a, b]) => b === 0n
    ? (a < 0n ? -0x80000000n : 0x7fffffffn)
    : satSigned(roundHalfUp(a * 65536n, b), 32n));
  const expectedDiv24 = div24Inputs.map(([a, b]) => b === 0n
    ? (a < 0n ? -(1n << 63n) : (1n << 63n) - 1n)
    : satSigned(roundHalfUp(a * 16777216n, b), 64n));

  const run = runGeneratedNative(output, `#include "form_game.hpp"
#include <cstdio>
#include <limits>
static form::Fx16 terrain(form::World2 at) { return form::fx16_from_fx24(form::fx24_add(at.x, at.y)); }
int main() {
  using namespace form;
  const Fx16 d16[][2] = {{1, 131072}, {-1, 131072}, {1, -131072}, {-1, -131072},
                         {0x7fffffff, 1}, {std::numeric_limits<i32>::min(), 1},
                         {1, 0}, {-1, 0}, {0, 0}};
  for (const auto& row : d16) std::printf("%d ", oracle::div16(row[0], row[1]));
  std::printf("\\n");
  const Fx24 d24[][2] = {{1, 33554432}, {-1, 33554432}, {1, -33554432}, {-1, -33554432}, {1, 0}, {-1, 0}};
  for (const auto& row : d24) std::printf("%lld ", static_cast<long long>(oracle::div24(row[0], row[1])));
  std::printf("\\n");

  const World2 v2{3LL << 24, 4LL << 24};
  const World3 v3{3LL << 24, 4LL << 24, 0};
  const World3 norm = oracle::vector_normal(v3);
  if (oracle::choose_min(3 << 16, 1 << 16) != (1 << 16) ||
      oracle::choose_max(3 << 16, 1 << 16) != (3 << 16) ||
      oracle::choose_clamp(4 << 16, 1 << 16, 3 << 16) != (3 << 16) ||
      oracle::magnitude(std::numeric_limits<i32>::min()) != std::numeric_limits<i32>::max() ||
      oracle::sine(0x4000u) != (1 << 16) || oracle::cosine(0x4000u) != 0 ||
      oracle::angle_of(1 << 16, 0) != 0x4000u || oracle::root(4 << 16) != (2 << 16) ||
      oracle::narrow(384) != 2 || oracle::narrow(-384) != -1 ||
      oracle::widen(3) != 768 || oracle::unit(0x7f80) != 128u ||
      oracle::angle(-1) != 0xffffu || oracle::scalar_angle(0x1234u) != 0x1234 ||
      oracle::scalar_unit(128u) != 0x8000 ||
      oracle::product2(v2, v2) != (25LL << 24) || oracle::product3(v3, v3) != (25LL << 24) ||
      oracle::vector_length(v3) != (5 << 16) || norm.x != 10066330 || norm.y != 13421773 || norm.z != 0) return 20;

  FormState state{};
  PadFrame pads[4]{};
  pads[2].buttons = 1u << 4u;
  oracle::scenario_seeded_run(state, 0x12345678u);
  state.terrain_height_sampler = &terrain;
  state.oracle.cells._form_pool_count = 2u;
  for (u32 tick = 0u; tick < 3u; ++tick) {
    sim_tick(state, pads, tick);
    std::printf("%u,%d,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%u,%llu,%llu,%llu,%llu\\n",
      state.oracle.ru, state.oracle.ri, static_cast<u32>(state.oracle.runit), static_cast<u32>(state.oracle.rangle), state.oracle.rfx,
      state.oracle.ordered, static_cast<u32>(state.oracle.eager_and), static_cast<u32>(state.oracle.eager_or), state.oracle.selected,
      state.oracle.called, state.oracle.record_first, state.oracle.record_second, state.oracle.cells._form_pool_column_76616c7565[0], state.oracle.cells._form_pool_column_76616c7565[1],
      state.oracle.other_stream, static_cast<u32>(state.oracle.held), static_cast<u32>(state.rng.size()), state.oracle.terrain_sample,
      state.oracle.mixed, static_cast<u32>(state.rng[0].state),
      static_cast<unsigned long long>(state.rng[0].state), static_cast<unsigned long long>(state.rng[0].increment),
      static_cast<unsigned long long>(state.rng[1].state), static_cast<unsigned long long>(state.rng[1].increment));
  }
  if (state.oracle.held != 1u || state.oracle.terrain_sample != (5 << 16) || state.oracle.mixed != (2 << 16)) return 21;
  FormState repeat{};
  oracle::scenario_seeded_run(repeat, 0x12345678u);
  repeat.terrain_height_sampler = &terrain;
  repeat.oracle.cells._form_pool_count = 2u;
  for (u32 tick = 0u; tick < 3u; ++tick) sim_tick(repeat, pads, tick);
  if (sim_hash(0u, state) != sim_hash(0u, repeat)) return 22;
  FormState changed = state;
  changed.rng[0].state ^= 0x9e3779b97f4a7c15ULL;
  sim_tick(state, pads, 3u);
  sim_tick(changed, pads, 3u);
  if (sim_hash(0u, state) == sim_hash(0u, changed)) return 23;
  FormState flat{};
  initialize(flat, 1u);
  flat.oracle.cells._form_pool_count = 2u;
  sim_tick(flat, pads, 0u);
  if (flat.oracle.terrain_sample != 0) return 24;
  return 0;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
  const lines = run.stdout.replaceAll('\r', '').trim().split('\n');
  assert.deepEqual(lines[0]!.trim().split(/\s+/).map(BigInt), expectedDiv16);
  assert.deepEqual(lines[1]!.trim().split(/\s+/).map(BigInt), expectedDiv24);

  const s0 = oracleStream(17n, 4n);
  const s1 = oracleStream(99n, 7n);
  const cells = [0n, 0n];
  const expectedRows: string[] = [];
  for (let tick = 0; tick < 3; tick++) {
    const draw = (): bigint => oracleDraw(s0);
    const ru = draw();
    const ri = signed32(draw());
    const unit = draw() >> 24n;
    const angle = draw() >> 16n;
    const randomFxRaw = draw();
    const rfx = -65536n + floorDiv(131072n * randomFxRaw, 1n << 32n);
    const ordered = (draw() - draw()) & U32_MASK;
    const andLeft = draw();
    const andRight = draw();
    const anded = andLeft !== 0n && andRight !== 0n ? 1n : 0n;
    const oredLeft = draw();
    const oredRight = draw();
    const ored = oredLeft !== 0n || oredRight !== 0n ? 1n : 0n;
    const selected = draw();
    draw();
    const called = (draw() - draw()) & U32_MASK;
    const second = draw();
    const first = draw();
    const index = Number(draw() & 1n);
    cells[index] = draw();
    const other = oracleDraw(s1);
    expectedRows.push([
      ru, ri, unit, angle, rfx, ordered, anded, ored, selected, called, first, second,
      cells[0]!, cells[1]!, other, 1n, 2n, 5n << 16n, 2n << 16n,
      Number(s0.state & U32_MASK), s0.state, s0.increment, s1.state, s1.increment,
    ].join(','));
  }
  assert.deepEqual(lines.slice(2), expectedRows);
});

test('native scenario driver consumes all seven explicit TestZIR operation kinds', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const output = compileSources({
    'scenario_ops.form': `module scenario_ops {
  global origin: world3 = world3 { x = 1w, y = 2w, z = 3w };
  global counter: u32 = 0;
  system step every 1 ticks reads counter writes counter { counter = counter + 1; }
  presentation limits { view 0 from origin budget 100%; }
  scenario all_operations {
    seed 42;
    load scenario_ops;
    spawn player 2 at origin;
    at 5 ticks step();
    assert counter == 1 within 0.25;
    capture frame 9 as "snapshot";
    assert_budget limits;
  }
}\n`,
  });
  const game = byPath(output, 'form_game.hpp');
  for (const kind of ['Seed', 'Load', 'SpawnPlayer', 'At', 'Assert', 'Capture', 'AssertBudget']) {
    assert.match(game, new RegExp(`ScenarioOperationKind::${kind}`));
  }
  assert.match(game, /std::array<ScenarioOperation, 7>/);
  const run = runGeneratedNative(output, `
#include "form_game.hpp"
#include <cstring>
struct Results {
  form::u32 seed{}; form::u32 loads{}; form::u32 spawns{}; form::u32 assertions{};
  form::u32 captures{}; form::u32 budgets{}; form::u32 player{}; form::u32 assertion_index{};
  form::World3 placement{}; form::Fx16 tolerance{}; form::Bool passed{}; form::Bool has_tolerance{};
};
static void on_seed(void* raw, form::u32 value) { static_cast<Results*>(raw)->seed = value; }
static void on_load(void* raw, form::u32 module, const char* name) {
  auto& out = *static_cast<Results*>(raw); if (module == 0u && std::strcmp(name, "scenario_ops") == 0) ++out.loads;
}
static void on_spawn(void* raw, form::u32 player, form::World3 placement) {
  auto& out = *static_cast<Results*>(raw); ++out.spawns; out.player = player; out.placement = placement;
}
static void on_assert(void* raw, form::u32 index, form::Bool passed, form::Fx16 tolerance, form::Bool has_tolerance) {
  auto& out = *static_cast<Results*>(raw); ++out.assertions; out.assertion_index = index;
  out.passed = passed; out.tolerance = tolerance; out.has_tolerance = has_tolerance;
}
static void on_capture(void* raw, form::u32 frame, const char* name) {
  auto& out = *static_cast<Results*>(raw); if (frame == 9u && std::strcmp(name, "snapshot") == 0) ++out.captures;
}
static void on_budget(void* raw, form::u32 module, const char* name) {
  auto& out = *static_cast<Results*>(raw); if (module == 0u && std::strcmp(name, "limits") == 0) ++out.budgets;
}
int main() {
  if (form::kScenarioScripts.size() != 1u || form::kScenarioScripts[0].operation_count != 7u) return 1;
  Results results{};
  const form::ScenarioDriver driver{&results, &on_seed, &on_load, &on_spawn, &on_assert, &on_capture, &on_budget};
  const form::PadFrame pads[4]{};
  form::FormState state{};
  form::run_scenario_script(form::kScenarioScripts[0], state, 99u, driver, pads);
  if (results.seed != 42u || results.loads != 1u || results.spawns != 1u || results.player != 2u) return 2;
  if (results.placement.x != 0x1000000LL || results.placement.y != 0x2000000LL || results.placement.z != 0x3000000LL) return 3;
  if (state.scenario_ops.counter != 1u) return 4;
  if (results.assertions != 1u || results.assertion_index != 4u || !results.passed ||
      results.tolerance != 0x4000 || !results.has_tolerance) return 5;
  if (results.captures != 1u || results.budgets != 1u) return 6;
  return 0;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
});

test('stream-valued eager select advances the selected persistent slot by reference', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const leftInitial = oracleStream(11n, 1n);
  const leftAdvanced = { ...leftInitial };
  const leftValue = oracleDraw(leftAdvanced);
  const rightInitial = oracleStream(22n, 2n);
  const rightAdvanced = { ...rightInitial };
  const rightValue = oracleDraw(rightAdvanced);
  const output = compileSources({
    'streams.form': `module streams {
  global choose_left: bool = true;
  global result: u32 = 0;
  system select_stream every 1 ticks reads choose_left writes result {
    let selected = if choose_left { random.stream(11, 1) } else { random.stream(22, 2) };
    result = random.u32(selected);
  }
}\n`,
  });
  const source = byPath(output, 'streams.cpp');
  assert.match(source, /\(\[&\]\(\) -> decltype\(auto\).*\? _form_ref_value_1 : _form_ref_value_2/s);
  const run = runGeneratedNative(output, `
#include "form_game.hpp"
int main() {
  const form::PadFrame pads[4]{};
  form::FormState left{};
  form::initialize(left, 1u);
  form::sim_tick(left, pads, 0u);
  if (left.streams.result != ${leftValue}u) return 1;
  if (left.rng[0].state != ${leftAdvanced.state}ULL || left.rng[0].increment != ${leftAdvanced.increment}ULL) return 2;
  if (left.rng[1].state != ${rightInitial.state}ULL || left.rng[1].increment != ${rightInitial.increment}ULL) return 3;

  form::FormState right{};
  form::initialize(right, 1u);
  right.streams.choose_left = 0u;
  form::sim_tick(right, pads, 0u);
  if (right.streams.result != ${rightValue}u) return 4;
  if (right.rng[0].state != ${leftInitial.state}ULL || right.rng[0].increment != ${leftInitial.increment}ULL) return 5;
  if (right.rng[1].state != ${rightAdvanced.state}ULL || right.rng[1].increment != ${rightAdvanced.increment}ULL) return 6;
  std::array<form::u8, form::kCanonicalStateMaxBytes> left_bytes{};
  std::array<form::u8, form::kCanonicalStateMaxBytes> right_bytes{};
  const auto left_size = form::serialize_canonical_state(left, left_bytes.data(), left_bytes.size());
  const auto right_size = form::serialize_canonical_state(right, right_bytes.data(), right_bytes.size());
  if (left_size == 0u || left_size != right_size || left_bytes == right_bytes) return 7;
  return 0;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
});

test('WinLibs proves every stagger residue, peak bound, 600 ticks, and ordinary cadence', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const output = compileSources({
    'cadence.form': `module cadence {
  struct particle { hits: u32; }
  pool particles: particle[10];
  global slow_calls: u32 = 0;
  system slow every 4 ticks reads slow_calls writes slow_calls {
    slow_calls = slow_calls + 1;
  }
  system staggered every 4 ticks stagger over particles reads particles writes particles.hits {
    for i in 0..particles.count { particles.hits[i] = particles.hits[i] + 1; }
  }
}
`,
  });
  const run = runGeneratedNative(output, `#include "form_game.hpp"
#include <cstdio>
int main() {
  form::FormState a{}, b{};
  form::PadFrame pads[4]{};
  constexpr form::u32 cartridge = 0x4d3c2b1au;
  form::initialize(a, cartridge);
  a.cadence.particles._form_pool_count = 10u;
  b = a;
  form::u32 chain_a = form::sim_hash_initial(cartridge);
  form::u32 chain_b = chain_a;
  for (form::u32 tick = 0u; tick < 600u; ++tick) {
    form::u32 before = 0u;
    for (form::u32 i = 0u; i < 10u; ++i) before += a.cadence.particles._form_pool_column_68697473[i];
    form::sim_tick(a, pads, tick);
    form::sim_tick(b, pads, tick);
    form::u32 after = 0u;
    for (form::u32 i = 0u; i < 10u; ++i) after += a.cadence.particles._form_pool_column_68697473[i];
    const form::u32 expected_peak[4] = {3u, 3u, 2u, 2u};
    if (after - before != expected_peak[tick % 4u]) return 10;
    if (tick == 3u) for (form::u32 i = 0u; i < 10u; ++i) if (a.cadence.particles._form_pool_column_68697473[i] != 1u) return 11;
    if (tick == 7u) for (form::u32 i = 0u; i < 10u; ++i) if (a.cadence.particles._form_pool_column_68697473[i] != 2u) return 12;
    chain_a = form::sim_hash(chain_a, a);
    chain_b = form::sim_hash(chain_b, b);
  }
  if (a.cadence.slow_calls != 150u) return 13;
  for (form::u32 i = 0u; i < 10u; ++i) if (a.cadence.particles._form_pool_column_68697473[i] != 150u) return 14;
  if (chain_a != chain_b || form::sim_hash(0u, a) != form::sim_hash(0u, b)) return 15;
  std::printf("%08x\\n", chain_a);
  return 0;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
  assert.equal(run.stdout.replaceAll('\r', ''), '7b14278f\n');
});

test('WinLibs compiles and runs generated C++17 state/hash smoke', (t) => {
  const compiler = 'C:/programmieren/dsstuff/mingw64/bin/g++.exe';
  if (!existsSync(compiler)) {
    t.skip(`WinLibs compiler absent: ${compiler}`);
    return;
  }
  const output = compileFixture();
  const run = runGeneratedNative(output, `#include "form_game.hpp"
#include <cstdio>
int main() {
  form::FormState state{};
  form::PadFrame pads[4]{};
  const form::u32 cartridge = 0x13579bdfu;
  form::initialize(state, cartridge);
  form::u32 chain = form::sim_hash_initial(cartridge);
  for (form::u32 tick = 0u; tick < 8u; ++tick) {
    form::sim_tick(state, pads, tick);
    chain = form::sim_hash(chain, state);
  }
  const form::u32 present_before = form::sim_hash(0x10203040u, state);
  zhao::ZhaoFrameBuilder builder;
  form::present_frame(state, builder);
  const form::u32 present_after = form::sim_hash(0x10203040u, state);
  if (present_before != present_after || builder.command_count() != 4u || builder.command_bytes() != 208u) return 4;
  const form::u32 before = form::sim_hash(0x2468ace0u, state);
  state.audit.observed += 1u;
  const form::u32 after = form::sim_hash(0x2468ace0u, state);
  if (state.arena.particles._form_pool_count != 4u || state.arena.particles._form_pool_column_616765[0] != 2u) return 2;
  if (state.arena.counter != 1u || state.audit.observed != 2u || before == after) return 3;
  std::printf("%08x %08x %08x\\n", chain, before, after);
  return 0;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
  assert.equal(run.stdout.replaceAll('\r', ''), 'd981e8e2 65c1abfe 07e322c7\n');
});
