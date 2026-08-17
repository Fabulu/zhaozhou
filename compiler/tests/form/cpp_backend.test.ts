import assert from 'node:assert/strict';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import test from 'node:test';

import { emitCpp } from '../../src/backends/index.js';
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
  assert.match(arena, /std::array<World3, capacity> position/);
  assert.match(arena, /std::array<u32, capacity> age/);
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
  assert.match(source, /state\.owner\.remote\.count >= form::owner::remote_pool::capacity/);
  assert.doesNotMatch(source, /state\.owner\.remote\.count >= form::user::remote_pool::capacity/);
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
  state.owner.entries.count = 1u;
  state.owner.entries.value[0] = 99u;
  form::PadFrame pads[4]{};
  form::sim_tick(state, pads, 0u);
  return state.owner.payload_state.value == 7u ? 0 : 1;
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
  state.gallery.motes.count = 1u;
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
          r.payload.sample_handle != 0x01000001u || r.payload.timestamp != 0u) return 38;
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
  state.oracle.cells.count = 2u;
  for (u32 tick = 0u; tick < 3u; ++tick) {
    sim_tick(state, pads, tick);
    std::printf("%u,%d,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%u,%llu,%llu,%llu,%llu\\n",
      state.oracle.ru, state.oracle.ri, static_cast<u32>(state.oracle.runit), static_cast<u32>(state.oracle.rangle), state.oracle.rfx,
      state.oracle.ordered, static_cast<u32>(state.oracle.eager_and), static_cast<u32>(state.oracle.eager_or), state.oracle.selected,
      state.oracle.called, state.oracle.record_first, state.oracle.record_second, state.oracle.cells.value[0], state.oracle.cells.value[1],
      state.oracle.other_stream, static_cast<u32>(state.oracle.held), static_cast<u32>(state.rng.size()), state.oracle.terrain_sample,
      state.oracle.mixed, static_cast<u32>(state.rng[0].state),
      static_cast<unsigned long long>(state.rng[0].state), static_cast<unsigned long long>(state.rng[0].increment),
      static_cast<unsigned long long>(state.rng[1].state), static_cast<unsigned long long>(state.rng[1].increment));
  }
  if (state.oracle.held != 1u || state.oracle.terrain_sample != (5 << 16) || state.oracle.mixed != (2 << 16)) return 21;
  FormState repeat{};
  oracle::scenario_seeded_run(repeat, 0x12345678u);
  repeat.terrain_height_sampler = &terrain;
  repeat.oracle.cells.count = 2u;
  for (u32 tick = 0u; tick < 3u; ++tick) sim_tick(repeat, pads, tick);
  if (sim_hash(0u, state) != sim_hash(0u, repeat)) return 22;
  FormState changed = state;
  changed.rng[0].state ^= 0x9e3779b97f4a7c15ULL;
  sim_tick(state, pads, 3u);
  sim_tick(changed, pads, 3u);
  if (sim_hash(0u, state) == sim_hash(0u, changed)) return 23;
  FormState flat{};
  initialize(flat, 1u);
  flat.oracle.cells.count = 2u;
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
  a.cadence.particles.count = 10u;
  b = a;
  form::u32 chain_a = form::sim_hash_initial(cartridge);
  form::u32 chain_b = chain_a;
  for (form::u32 tick = 0u; tick < 600u; ++tick) {
    form::u32 before = 0u;
    for (form::u32 i = 0u; i < 10u; ++i) before += a.cadence.particles.hits[i];
    form::sim_tick(a, pads, tick);
    form::sim_tick(b, pads, tick);
    form::u32 after = 0u;
    for (form::u32 i = 0u; i < 10u; ++i) after += a.cadence.particles.hits[i];
    const form::u32 expected_peak[4] = {3u, 3u, 2u, 2u};
    if (after - before != expected_peak[tick % 4u]) return 10;
    if (tick == 3u) for (form::u32 i = 0u; i < 10u; ++i) if (a.cadence.particles.hits[i] != 1u) return 11;
    if (tick == 7u) for (form::u32 i = 0u; i < 10u; ++i) if (a.cadence.particles.hits[i] != 2u) return 12;
    chain_a = form::sim_hash(chain_a, a);
    chain_b = form::sim_hash(chain_b, b);
  }
  if (a.cadence.slow_calls != 150u) return 13;
  for (form::u32 i = 0u; i < 10u; ++i) if (a.cadence.particles.hits[i] != 150u) return 14;
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
  if (state.arena.particles.count != 4u || state.arena.particles.age[0] != 2u) return 2;
  if (state.arena.counter != 1u || state.audit.observed != 2u || before == after) return 3;
  std::printf("%08x %08x %08x\\n", chain, before, after);
  return 0;
}
`);
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
  assert.equal(run.stdout.replaceAll('\r', ''), 'd981e8e2 65c1abfe 07e322c7\n');
});
