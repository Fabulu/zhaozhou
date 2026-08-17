// emitter.ts — byte-stable C++17 lowering for Form Sim/Present/Test ZIR.

import type { Expr, RecordLit, ScenarioItem, Stmt } from '../../frontend/ast.js';
import type { Type } from '../../frontend/checker.js';
import type {
  HirDeclaration, HirEnum, HirExpr, HirField, HirFunction, HirGlobal, HirModule,
  HirPool, HirPresentation, HirProgram, HirScenario, HirSound, HirStmt, HirStruct,
} from '../../hir/model.js';
import { declarationsOf } from '../../hir/model.js';
import type { ZirProgram, ZirSystem, ZirViewLayout } from '../../zir/model.js';
import { FORM_CPP_BACKEND_VERSION, type CppGeneratedFile, type CppOutput } from './model.js';

const CPP_KEYWORDS = new Set([
  'alignas', 'alignof', 'and', 'and_eq', 'asm', 'auto', 'bitand', 'bitor', 'bool',
  'break', 'case', 'catch', 'char', 'class', 'compl', 'concept', 'const', 'consteval',
  'constexpr', 'constinit', 'const_cast', 'continue', 'co_await', 'co_return',
  'co_yield', 'decltype', 'default', 'delete', 'do', 'dynamic_cast', 'else', 'enum',
  'explicit', 'export', 'extern', 'false', 'for', 'friend', 'goto', 'if', 'inline',
  'int', 'long', 'mutable', 'namespace', 'new', 'noexcept', 'not', 'not_eq', 'nullptr',
  'operator', 'or', 'or_eq', 'private', 'protected', 'public', 'register',
  'reinterpret_cast', 'requires', 'return', 'short', 'signed', 'sizeof', 'static',
  'static_assert', 'static_cast', 'struct', 'switch', 'template', 'this', 'thread_local',
  'throw', 'true', 'try', 'typedef', 'typeid', 'typename', 'union', 'unsigned', 'using',
  'virtual', 'void', 'volatile', 'wchar_t', 'while', 'xor', 'xor_eq',
]);

const BUTTONS = new Map<string, number>([
  ['BTN_UP', 0], ['BTN_DOWN', 1], ['BTN_LEFT', 2], ['BTN_RIGHT', 3],
  ['BTN_A', 4], ['BTN_B', 5], ['BTN_X', 6], ['BTN_Y', 7],
  ['BTN_L2', 8], ['BTN_R2', 9], ['BTN_L1', 10], ['BTN_R1', 11],
  ['BTN_L3', 12], ['BTN_R3', 13], ['BTN_SELECT', 14], ['BTN_START', 15],
]);

class Lines {
  private readonly rows: string[] = [];
  line(text = ''): void { this.rows.push(text); }
  block(rows: readonly string[], indent = '  '): void {
    for (const row of rows) this.rows.push(row.length === 0 ? '' : indent + row);
  }
  finish(): string { return this.rows.join('\n').replace(/[ \t]+$/gm, '').replace(/\n*$/, '') + '\n'; }
}

interface ExprContext {
  state: string;
  pads: string;
  tick: string;
}

export function emitCpp(hir: HirProgram, zir: ZirProgram): CppOutput {
  return new CppEmitter(hir, zir).run();
}

class CppEmitter {
  private readonly modules = new Map<number, HirModule>();
  private readonly moduleNames = new Map<number, string>();
  private readonly declByKey = new Map<string, HirDeclaration>();
  private readonly structs = new Map<string, HirStruct>();
  private readonly pools = new Map<string, HirPool>();
  private readonly sounds = new Map<string, HirSound>();
  private loopOrdinal = 0;

  constructor(private readonly hir: HirProgram, private readonly zir: ZirProgram) {
    for (const module of hir.modules) {
      this.modules.set(module.index, module);
      this.moduleNames.set(module.index, ident(module.name));
    }
    for (const decl of hir.declarations) {
      this.declByKey.set(key(decl.module, decl.name), decl);
      if (decl.kind === 'struct') this.structs.set(key(decl.module, decl.name), decl);
      if (decl.kind === 'pool') this.pools.set(key(decl.module, decl.name), decl);
      if (decl.kind === 'sound') this.sounds.set(key(decl.module, decl.name), decl);
    }
  }

  run(): CppOutput {
    this.refuseUnlinkedFieldApplications();
    const files: CppGeneratedFile[] = [{ path: 'generated/form/form_types.hpp', content: this.typesHeader() }];
    for (const module of [...this.hir.modules].sort((a, b) => a.index - b.index)) {
      files.push({ path: `generated/form/${module.name}.hpp`, content: this.moduleHeader(module) });
      files.push({ path: `generated/form/${module.name}.cpp`, content: this.moduleSource(module) });
    }
    files.push({ path: 'generated/form/form_game.hpp', content: this.gameHeader() });
    files.sort((a, b) => utf8Compare(a.path, b.path));
    for (const file of files) {
      if (file.content.includes('\r') || !file.content.endsWith('\n')) throw new Error(`C++ emitter violated LF law for ${file.path}`);
    }
    return { files, manifestCrc32c: this.hir.manifestCrc32c };
  }

  private refuseUnlinkedFieldApplications(): void {
    const visit = (statements: readonly HirStmt[]): void => {
      for (const statement of statements) {
        if (statement.ast.kind === 'apply') {
          const { file, start, end } = statement.span;
          throw new Error(
            `C++ backend cannot emit terrain_field application '${statement.ast.program}' at ${file}:${start}-${end} `
            + 'until W3.4 supplies its validated physical Field IR wrapper',
          );
        }
        visit(statement.body);
        visit(statement.elseBody);
      }
    };
    for (const declaration of this.hir.declarations) {
      if (declaration.kind === 'system' || declaration.kind === 'fn') visit(declaration.body);
    }
  }

  private banner(): string {
    return `// Generated by Form C++ backend v${FORM_CPP_BACKEND_VERSION}; DO NOT EDIT.`;
  }

  private typesHeader(): string {
    const o = new Lines();
    o.line(this.banner());
    o.line('#pragma once');
    o.line();
    o.line('#include <array>');
    o.line('#include <cstddef>');
    o.line('#include <cstdint>');
    o.line('#include <cstdlib>');
    o.line('#include <limits>');
    o.line('#include <type_traits>');
    o.line('#include <zref/zref_frame.hpp>');
    o.line('#include <zref/zref_input.hpp>');
    o.line();
    o.line('namespace zref { using FrameBuilder = zhao::ZhaoFrameBuilder; }');
    o.line();
    o.line('namespace form {');
    o.line('using u8 = std::uint8_t;');
    o.line('using u16 = std::uint16_t;');
    o.line('using u32 = std::uint32_t;');
    o.line('using i16 = std::int16_t;');
    o.line('using i32 = std::int32_t;');
    o.line('using i64 = std::int64_t;');
    o.line('using Fx16 = i32;');
    o.line('using Fx24 = i64;');
    o.line('using Angle16 = u16;');
    o.line('using Unit8 = u8;');
    o.line('using Bool = u8;');
    o.line('using Colour8 = u32;');
    o.line('using PadFrame = zref::PadFrame;');
    o.line('struct World2 { Fx24 x{}; Fx24 y{}; };');
    o.line('struct World3 { Fx24 x{}; Fx24 y{}; Fx24 z{}; };');
    o.line('struct Velocity3 { Fx24 x{}; Fx24 y{}; Fx24 z{}; };');
    o.line('struct Stream { u64 state{}; u64 increment{}; };'.replace(/u64/g, 'std::uint64_t'));
    o.line();
    o.line('inline i32 sat_i32(i64 value) {');
    o.line('  if (value > std::numeric_limits<i32>::max()) return std::numeric_limits<i32>::max();');
    o.line('  if (value < std::numeric_limits<i32>::min()) return std::numeric_limits<i32>::min();');
    o.line('  return static_cast<i32>(value);');
    o.line('}');
    o.line('inline Fx16 fx16_add(Fx16 a, Fx16 b) { return sat_i32(static_cast<i64>(a) + b); }');
    o.line('inline Fx16 fx16_sub(Fx16 a, Fx16 b) { return sat_i32(static_cast<i64>(a) - b); }');
    o.line('inline Fx16 fx16_mul(Fx16 a, Fx16 b) {');
    o.line('  const i64 biased = static_cast<i64>(a) * b + 0x8000;');
    o.line('  const i64 scaled = biased >= 0 ? biased / 0x10000 : -(((-biased) + 0xffff) / 0x10000);');
    o.line('  return sat_i32(scaled);');
    o.line('}');
    o.line('inline Fx24 fx24_add(Fx24 a, Fx24 b) {');
    o.line('  if (b > 0 && a > std::numeric_limits<Fx24>::max() - b) return std::numeric_limits<Fx24>::max();');
    o.line('  if (b < 0 && a < std::numeric_limits<Fx24>::min() - b) return std::numeric_limits<Fx24>::min();');
    o.line('  return a + b;');
    o.line('}');
    o.line('inline Fx24 fx24_sub(Fx24 a, Fx24 b) {');
    o.line('  if (b < 0 && a > std::numeric_limits<Fx24>::max() + b) return std::numeric_limits<Fx24>::max();');
    o.line('  if (b > 0 && a < std::numeric_limits<Fx24>::min() + b) return std::numeric_limits<Fx24>::min();');
    o.line('  return a - b;');
    o.line('}');
    o.line('inline Fx24 fx24_mul(Fx24 a, Fx24 b) {');
    o.line('  const __int128 biased = static_cast<__int128>(a) * b + 0x800000;');
    o.line('  const __int128 scaled = biased >= 0 ? biased / 0x1000000 : -(((-biased) + 0xffffff) / 0x1000000);');
    o.line('  if (scaled > std::numeric_limits<Fx24>::max()) return std::numeric_limits<Fx24>::max();');
    o.line('  if (scaled < std::numeric_limits<Fx24>::min()) return std::numeric_limits<Fx24>::min();');
    o.line('  return static_cast<Fx24>(scaled);');
    o.line('}');
    o.line('inline Fx16 fx16_from_fx24(Fx24 value) {');
    o.line('  i64 rounded = value / 0x100;');
    o.line('  const i64 remainder = value % 0x100;');
    o.line('  if (remainder >= 0x80) ++rounded;');
    o.line('  else if (remainder < -0x80) --rounded;');
    o.line('  return sat_i32(rounded);');
    o.line('}');
    o.line('template <class T> inline T select_value(Bool condition, T yes, T no) { return condition ? yes : no; }');
    o.line('template <class T> inline T min_value(T a, T b) { return a < b ? a : b; }');
    o.line('template <class T> inline T max_value(T a, T b) { return a > b ? a : b; }');
    o.line('template <class T> inline T clamp_value(T value, T lo, T hi) { return min_value(max_value(value, lo), hi); }');
    o.line('inline i32 abs_value(i32 value) { return value == std::numeric_limits<i32>::min() ? std::numeric_limits<i32>::max() : (value < 0 ? -value : value); }');
    o.line('inline i64 abs_value(i64 value) { return value == std::numeric_limits<i64>::min() ? std::numeric_limits<i64>::max() : (value < 0 ? -value : value); }');
    o.line('inline Unit8 unit_add(Unit8 a, Unit8 b) { const u32 sum = static_cast<u32>(a) + b; return static_cast<Unit8>(sum > 255u ? 255u : sum); }');
    o.line('inline Unit8 unit_sub(Unit8 a, Unit8 b) { return static_cast<Unit8>(a < b ? 0u : static_cast<u32>(a) - b); }');
    o.line('inline Unit8 unit_mul(Unit8 a, Unit8 b) { const u32 product = static_cast<u32>(a) * b + 128u; return static_cast<Unit8>(product > 0xff00u ? 255u : product >> 8u); }');
    o.line('inline World2 world2_add(World2 a, World2 b) { return World2{fx24_add(a.x, b.x), fx24_add(a.y, b.y)}; }');
    o.line('inline World2 world2_sub(World2 a, World2 b) { return World2{fx24_sub(a.x, b.x), fx24_sub(a.y, b.y)}; }');
    o.line('inline World3 world3_add(World3 a, World3 b) { return World3{fx24_add(a.x, b.x), fx24_add(a.y, b.y), fx24_add(a.z, b.z)}; }');
    o.line('inline World3 world3_sub(World3 a, World3 b) { return World3{fx24_sub(a.x, b.x), fx24_sub(a.y, b.y), fx24_sub(a.z, b.z)}; }');
    o.line('inline Velocity3 velocity3_add(Velocity3 a, Velocity3 b) { return Velocity3{fx24_add(a.x, b.x), fx24_add(a.y, b.y), fx24_add(a.z, b.z)}; }');
    o.line('inline Velocity3 velocity3_sub(Velocity3 a, Velocity3 b) { return Velocity3{fx24_sub(a.x, b.x), fx24_sub(a.y, b.y), fx24_sub(a.z, b.z)}; }');
    o.line('[[noreturn]] inline void form_abort(u32 code) { (void)code; std::abort(); }');
    o.line('template <class T, std::size_t N> inline T& checked_index(std::array<T, N>& values, u32 index, u32 limit) {');
    o.line('  if (index >= limit || index >= N) form_abort(822u);');
    o.line('  return values[index];');
    o.line('}');
    o.line('template <class T, std::size_t N> inline const T& checked_index(const std::array<T, N>& values, u32 index, u32 limit) {');
    o.line('  if (index >= limit || index >= N) form_abort(822u);');
    o.line('  return values[index];');
    o.line('}');
    o.line('inline Bool input_held(const PadFrame& pad, u32 bit) { return static_cast<Bool>((pad.buttons >> (bit & 31u)) & 1u); }');
    o.line('inline Stream random_stream(u32 seed, u32 id0 = 0u, u32 id1 = 0u, u32 id2 = 0u) {');
    o.line('  std::uint64_t mixed = 0x853c49e6748fea9bULL ^ seed;');
    o.line('  mixed = (mixed ^ id0) * 0xda942042e4dd58b5ULL;');
    o.line('  mixed = (mixed ^ id1) * 0xda942042e4dd58b5ULL;');
    o.line('  mixed = (mixed ^ id2) * 0xda942042e4dd58b5ULL;');
    o.line('  return Stream{mixed, (mixed << 1u) | 1u};');
    o.line('}');
    o.line('inline u32 random_u32(Stream& stream) {');
    o.line('  const std::uint64_t old = stream.state;');
    o.line('  stream.state = old * 6364136223846793005ULL + stream.increment;');
    o.line('  const u32 shift = static_cast<u32>(((old >> 18u) ^ old) >> 27u);');
    o.line('  const u32 rotate = static_cast<u32>(old >> 59u);');
    o.line('  return (shift >> rotate) | (shift << ((0u - rotate) & 31u));');
    o.line('}');
    o.line('}  // namespace form');
    return o.finish();
  }

  private moduleHeader(module: HirModule): string {
    const o = new Lines();
    const ns = this.moduleName(module.index);
    const declarations = this.hir.declarations.filter((decl) => decl.module === module.index);
    o.line(this.banner());
    o.line('#pragma once');
    o.line();
    o.line('#include "form_types.hpp"');
    o.line('#include <array>');
    for (const imported of [...new Set(module.imports.map((item) => item.module))].sort((a, b) => a - b)) {
      o.line(`#include "${this.modules.get(imported)!.name}.hpp"`);
    }
    o.line();
    o.line('namespace form { struct FormState; }');
    o.line(`namespace form::${ns} {`);
    o.line();

    for (const decl of declarations) {
      if (decl.kind === 'enum') this.emitEnum(o, decl);
      if (decl.kind === 'struct') this.emitStruct(o, decl);
    }
    for (const decl of declarations) if (decl.kind === 'const') {
      const value = decl.raw === null ? this.expr(decl.init, { state: 'state', pads: 'pads', tick: 'tick' }) : cppInteger(decl.raw, decl.type);
      o.line(`inline constexpr ${this.cppType(decl.type)} ${ident(decl.name)} = ${value};`);
    }
    if (declarations.some((decl) => decl.kind === 'const')) o.line();
    for (const pool of declarations.filter((decl): decl is HirPool => decl.kind === 'pool')) this.emitPool(o, pool);

    o.line('struct State {');
    const states = declarations.filter((decl): decl is HirPool | HirGlobal => decl.kind === 'pool' || decl.kind === 'global');
    if (states.length === 0) o.line('  u8 empty{};');
    for (const decl of states) {
      const type = decl.kind === 'pool' ? `${ident(decl.name)}_pool` : this.cppType(decl.type);
      o.line(`  ${type} ${ident(decl.name)}{};`);
    }
    o.line('};');
    o.line();

    for (const fn of declarations.filter((decl): decl is HirFunction => decl.kind === 'fn')) {
      o.line(`${this.cppType(fn.returnType)} ${ident(fn.name)}(${fn.params.map((param) => `${this.cppType(param.type)} ${ident(param.name)}`).join(', ')});`);
    }
    for (const system of declarations.filter((decl) => decl.kind === 'system')) {
      o.line(`void system_${ident(system.name)}(FormState& state, const PadFrame pads[4], u32 tick);`);
    }
    for (const presentation of declarations.filter((decl): decl is HirPresentation => decl.kind === 'presentation')) {
      o.line(`void present_${ident(presentation.name)}(const FormState& state, zref::FrameBuilder& builder);`);
    }
    for (const scenario of declarations.filter((decl): decl is HirScenario => decl.kind === 'scenario')) {
      o.line(`void scenario_${ident(scenario.name)}(FormState& state, u32 cartridge_hash);`);
    }
    o.line();
    o.line(`}  // namespace form::${ns}`);
    return o.finish();
  }

  private emitEnum(o: Lines, decl: HirEnum): void {
    o.line(`enum class ${ident(decl.name)} : u32 {`);
    decl.members.forEach((member, index) => o.line(`  ${ident(member.name)} = ${member.value.toString()}u${index + 1 === decl.members.length ? '' : ','}`));
    o.line('};');
    o.line();
  }

  private emitStruct(o: Lines, decl: HirStruct): void {
    o.line(`struct ${ident(decl.name)} {`);
    if (decl.fields.length === 0) o.line('  u8 empty{};');
    for (const field of decl.fields) o.line(`  ${this.cppType(field.type)} ${ident(field.name)}{};`);
    o.line('};');
    o.line();
  }

  private emitPool(o: Lines, pool: HirPool): void {
    const struct = this.structs.get(key(pool.structModule, pool.structName));
    if (!struct) throw new Error(`C++ emitter cannot find pool struct ${pool.structModule}.${pool.structName}`);
    o.line(`struct ${ident(pool.name)}_pool {`);
    o.line(`  static constexpr u32 capacity = ${pool.capacity}u;`);
    o.line('  u32 count{};');
    for (const field of struct.fields) o.line(`  std::array<${this.cppType(field.type)}, capacity> ${ident(field.name)}{};`);
    o.line('};');
    o.line();
  }

  private moduleSource(module: HirModule): string {
    const o = new Lines();
    const ns = this.moduleName(module.index);
    const declarations = this.hir.declarations.filter((decl) => decl.module === module.index);
    o.line(this.banner());
    o.line(`#include "${module.name}.hpp"`);
    o.line('#include "form_game.hpp"');
    o.line('#include <vector>');
    o.line('#include <zhao_abi.h>');
    o.line();
    o.line(`namespace form::${ns} {`);
    o.line();

    for (const fn of declarations.filter((decl): decl is HirFunction => decl.kind === 'fn')) this.emitFunction(o, fn);
    for (const system of declarations.filter((decl) => decl.kind === 'system')) this.emitSystem(o, system);
    for (const presentation of declarations.filter((decl): decl is HirPresentation => decl.kind === 'presentation')) this.emitPresentation(o, presentation);
    for (const scenario of declarations.filter((decl): decl is HirScenario => decl.kind === 'scenario')) this.emitScenario(o, scenario);

    o.line(`}  // namespace form::${ns}`);
    return o.finish();
  }

  private emitFunction(o: Lines, fn: HirFunction): void {
    o.line(`${this.cppType(fn.returnType)} ${ident(fn.name)}(${fn.params.map((param) => `${this.cppType(param.type)} ${ident(param.name)}`).join(', ')}) {`);
    this.emitStatements(o, fn.body, 1, null);
    o.line('}');
    o.line();
  }

  private emitSystem(o: Lines, system: Extract<HirDeclaration, { kind: 'system' }>): void {
    o.line(`void system_${ident(system.name)}(FormState& state, const PadFrame pads[4], u32 tick) {`);
    o.line('  (void)pads;');
    o.line('  (void)tick;');
    const killedPools = this.killedPools(system.body);
    for (const pool of killedPools) {
      o.line(`  std::array<Bool, form::${this.moduleName(pool.module)}::${ident(pool.name)}_pool::capacity> ${this.killFlags(pool)}{};`);
    }
    this.emitStatements(o, system.body, 1, system);
    for (const pool of killedPools) this.emitStableCompaction(o, pool, 1);
    o.line('}');
    o.line();
  }

  private emitStatements(o: Lines, statements: HirStmt[], depth: number, system: Extract<HirDeclaration, { kind: 'system' }> | null): void {
    const pad = '  '.repeat(depth);
    const ctx: ExprContext = { state: 'state', pads: 'pads', tick: 'tick' };
    for (const statement of statements) {
      const ast = statement.ast;
      switch (ast.kind) {
        case 'let': {
          const type = ast.type ? this.cppType(statement.expressions[0]!.type) : 'auto';
          o.line(`${pad}${type} ${ident(ast.name)} = ${this.expr(statement.expressions[0]!, ctx)};`);
          break;
        }
        case 'assign': {
          const target = this.expr(statement.expressions[0]!, ctx);
          const value = this.expr(statement.expressions[1]!, ctx);
          o.line(`${pad}${target} = ${value};`);
          break;
        }
        case 'if':
          o.line(`${pad}if (${this.expr(statement.expressions[0]!, ctx)}) {`);
          this.emitStatements(o, statement.body, depth + 1, system);
          if (statement.elseBody.length > 0) {
            o.line(`${pad}} else {`);
            this.emitStatements(o, statement.elseBody, depth + 1, system);
          }
          o.line(`${pad}}`);
          break;
        case 'for':
          this.emitFor(o, statement, ast, depth, system);
          break;
        case 'call_stmt':
          o.line(`${pad}${this.expr(statement.expressions[0]!, ctx)};`);
          break;
        case 'spawn':
          this.emitSpawn(o, statement, ast, depth);
          break;
        case 'kill':
          this.emitKill(o, statement, ast, depth);
          break;
        case 'return':
          o.line(`${pad}return${statement.expressions.length ? ` ${this.expr(statement.expressions[0]!, ctx)}` : ''};`);
          break;
        case 'apply':
          throw new Error('unlinked terrain_field application reached C++ statement emission');
        case 'bad_stmt':
          throw new Error('C++ emitter received recovered bad statement');
      }
    }
  }

  private emitFor(o: Lines, statement: HirStmt, ast: Extract<Stmt, { kind: 'for' }>, depth: number, system: Extract<HirDeclaration, { kind: 'system' }> | null): void {
    const pad = '  '.repeat(depth);
    const ctx: ExprContext = { state: 'state', pads: 'pads', tick: 'tick' };
    const variable = ident(ast.varName);
    if (ast.range.kind === 'range') {
      const lo = this.expr(statement.expressions[0]!, ctx);
      const hi = this.expr(statement.expressions[1]!, ctx);
      const ordinal = this.loopOrdinal++;
      const begin = `_for_begin_${ordinal}`;
      const end = `_for_end_${ordinal}`;
      o.line(`${pad}const u32 ${begin} = static_cast<u32>(${lo});`);
      o.line(`${pad}const u32 ${end} = static_cast<u32>(${hi});`);
      o.line(`${pad}for (u32 ${variable} = ${begin}; ${variable} < ${end}; ++${variable}) {`);
      if (system?.staggerPool) {
        o.line(`${pad}  if ((${variable} % ${system.staggerRate ?? system.every}u) == (tick % ${system.staggerRate ?? system.every}u)) {`);
        this.emitStatements(o, statement.body, depth + 2, system);
        o.line(`${pad}  }`);
      } else {
        this.emitStatements(o, statement.body, depth + 1, system);
      }
      o.line(`${pad}}`);
      return;
    }
    const pool = this.pools.get(key(statement.expressions[0]!.symbol!.module!, statement.expressions[0]!.symbol!.name));
    if (!pool) throw new Error(`C++ emitter cannot resolve loop pool ${ast.range.pool}`);
    const poolExpr = this.stateMember(pool.module, pool.name, 'state');
    const struct = this.structs.get(key(pool.structModule, pool.structName))!;
    const ordinal = this.loopOrdinal++;
    const index = `_${variable}_index_${ordinal}`;
    const end = `_pool_end_${ordinal}`;
    o.line(`${pad}const u32 ${end} = ${poolExpr}.count;`);
    o.line(`${pad}for (u32 ${index} = 0u; ${index} < ${end}; ++${index}) {`);
    const inner = system?.staggerPool ? depth + 2 : depth + 1;
    if (system?.staggerPool) o.line(`${pad}  if ((${index} % ${system.staggerRate ?? system.every}u) == (tick % ${system.staggerRate ?? system.every}u)) {`);
    o.line(`${'  '.repeat(inner)}${this.cppStructName(pool.structModule, pool.structName)} ${variable}{${struct.fields.map((field) => `${poolExpr}.${ident(field.name)}[${index}]`).join(', ')}};`);
    this.emitStatements(o, statement.body, inner, system);
    for (const field of struct.fields) o.line(`${'  '.repeat(inner)}${poolExpr}.${ident(field.name)}[${index}] = ${variable}.${ident(field.name)};`);
    if (system?.staggerPool) o.line(`${pad}  }`);
    o.line(`${pad}}`);
  }

  private emitSpawn(o: Lines, statement: HirStmt, ast: Extract<Stmt, { kind: 'spawn' }>, depth: number): void {
    const pad = '  '.repeat(depth);
    const record = statement.expressions[0]!;
    const pool = this.resolveDeclaration(this.moduleForSpan(statement.span.file), ast.pool);
    if (!pool || pool.kind !== 'pool') throw new Error(`C++ emitter cannot resolve spawn pool ${ast.pool}`);
    const struct = this.structs.get(key(pool.structModule, pool.structName))!;
    const poolExpr = this.stateMember(pool.module, pool.name, 'state');
    const values = this.recordValues(record, struct);
    o.line(`${pad}if (${poolExpr}.count >= ${ident(pool.name)}_pool::capacity) form_abort(821u);`);
    o.line(`${pad}{`);
    o.line(`${pad}  const u32 _spawn_index = ${poolExpr}.count++;`);
    for (const field of struct.fields) o.line(`${pad}  ${poolExpr}.${ident(field.name)}[_spawn_index] = ${values.get(field.name) ?? '{}'};`);
    o.line(`${pad}}`);
  }

  private emitKill(o: Lines, statement: HirStmt, ast: Extract<Stmt, { kind: 'kill' }>, depth: number): void {
    const pad = '  '.repeat(depth);
    const resolved = this.resolveDeclaration(this.moduleForSpan(statement.span.file), ast.pool);
    if (!resolved || resolved.kind !== 'pool') throw new Error(`C++ emitter cannot resolve kill pool ${ast.pool}`);
    const pool = resolved;
    const poolExpr = this.stateMember(pool.module, pool.name, 'state');
    const indexExpr = this.expr(statement.expressions[0]!, { state: 'state', pads: 'pads', tick: 'tick' });
    o.line(`${pad}{`);
    o.line(`${pad}  const u32 _kill_index = static_cast<u32>(${indexExpr});`);
    o.line(`${pad}  if (_kill_index >= ${poolExpr}.count) form_abort(822u);`);
    o.line(`${pad}  ${this.killFlags(pool)}[_kill_index] = 1u;`);
    o.line(`${pad}}`);
  }

  private killedPools(statements: HirStmt[]): HirPool[] {
    const found = new Map<string, HirPool>();
    const visit = (items: HirStmt[]): void => {
      for (const statement of items) {
        if (statement.ast.kind === 'kill') {
          const decl = this.resolveDeclaration(this.moduleForSpan(statement.span.file), statement.ast.pool);
          if (!decl || decl.kind !== 'pool') throw new Error(`C++ emitter cannot resolve kill pool ${statement.ast.pool}`);
          found.set(key(decl.module, decl.name), decl);
        }
        visit(statement.body);
        visit(statement.elseBody);
      }
    };
    visit(statements);
    return [...found.values()].sort((a, b) => a.module - b.module || a.order - b.order);
  }

  private killFlags(pool: HirPool): string { return `_killed_${pool.module}_${ident(pool.name)}`; }

  private emitStableCompaction(o: Lines, pool: HirPool, depth: number): void {
    const pad = '  '.repeat(depth);
    const poolExpr = this.stateMember(pool.module, pool.name, 'state');
    const struct = this.structs.get(key(pool.structModule, pool.structName))!;
    const suffix = `${pool.module}_${ident(pool.name)}`;
    o.line(`${pad}u32 _write_${suffix} = 0u;`);
    o.line(`${pad}for (u32 _read_${suffix} = 0u; _read_${suffix} < ${poolExpr}.count; ++_read_${suffix}) {`);
    o.line(`${pad}  if (!${this.killFlags(pool)}[_read_${suffix}]) {`);
    for (const field of struct.fields) {
      o.line(`${pad}    ${poolExpr}.${ident(field.name)}[_write_${suffix}] = ${poolExpr}.${ident(field.name)}[_read_${suffix}];`);
    }
    o.line(`${pad}    ++_write_${suffix};`);
    o.line(`${pad}  }`);
    o.line(`${pad}}`);
    o.line(`${pad}${poolExpr}.count = _write_${suffix};`);
  }

  private emitPresentation(o: Lines, presentation: HirPresentation): void {
    o.line(`void present_${ident(presentation.name)}(const FormState& state, zref::FrameBuilder& builder) {`);
    o.line('  (void)state;');
    const layout = this.zir.present.layouts.find(
      (item) => item.module === presentation.module && item.presentation === presentation.name,
    );
    if (layout) this.emitViewLayout(o, layout, 1);
    const templates = this.zir.present.templates.filter((template) => template.module === presentation.module && template.presentation === presentation.name);
    for (const template of templates) this.emitCommand(o, template.command, 1);
    o.line('}');
    o.line();
  }

  private emitViewLayout(o: Lines, layout: ZirViewLayout, depth: number): void {
    const pad = '  '.repeat(depth);
    const mode = layout.views.some((view) => view.id === 1) ? 'VIDEO_DUO' : 'VIDEO_Z60';
    o.line(`${pad}{`);
    o.line(`${pad}  zhao_abi::ZhRecordSetPresentationContract record{};`);
    o.line(`${pad}  record.hdr.opcode = zhao_abi::ZHAO_OP_SET_PRESENTATION_CONTRACT;`);
    o.line(`${pad}  record.hdr.record_bytes = 48u;`);
    o.line(`${pad}  record.hdr.source_id = 0u;`);
    o.line(`${pad}  record.payload.mode = zhao_abi::${mode};`);
    o.line(`${pad}  record.payload.view_count = ${layout.views.length}u;`);
    for (const view of layout.views) {
      o.line(`${pad}  record.payload.geometry_tokens[${view.id}u] = ${view.budgetPct}u;`);
      o.line(`${pad}  record.payload.fragment_tokens[${view.id}u] = ${view.budgetPct}u;`);
    }
    o.line(`${pad}  record.payload.shared_tokens = ${layout.sharedBudgetPct}u;`);
    o.line(`${pad}  std::vector<u8> bytes;`);
    o.line(`${pad}  zhao_abi::zhao_pack_set_presentation_contract(record, bytes);`);
    o.line(`${pad}  builder.append_record(bytes);`);
    o.line(`${pad}}`);
    for (const view of layout.views) {
      const camera = `_view_camera_${view.id}`;
      o.line(`${pad}{`);
      o.line(`${pad}  const World3 ${camera} = ${this.expr(view.camera, { state: 'state', pads: 'pads', tick: 'tick' })};`);
      o.line(`${pad}  zhao_abi::ZhRecordSetView record{};`);
      o.line(`${pad}  record.hdr.opcode = zhao_abi::ZHAO_OP_SET_VIEW;`);
      o.line(`${pad}  record.hdr.record_bytes = 96u;`);
      o.line(`${pad}  record.hdr.source_id = 0u;`);
      o.line(`${pad}  record.payload.view_id = ${view.id}u;`);
      o.line(`${pad}  record.payload.viewport_id = ${view.id}u;`);
      o.line(`${pad}  record.payload.view_projection.m00 = 0x10000;`);
      o.line(`${pad}  record.payload.view_projection.m11 = 0x10000;`);
      o.line(`${pad}  record.payload.view_projection.m22 = 0x10000;`);
      o.line(`${pad}  record.payload.view_projection.m33 = 0x10000;`);
      o.line(`${pad}  record.payload.view_projection.m03 = fx16_sub(0, fx16_from_fx24(${camera}.x));`);
      o.line(`${pad}  record.payload.view_projection.m13 = fx16_sub(0, fx16_from_fx24(${camera}.y));`);
      o.line(`${pad}  record.payload.view_projection.m23 = fx16_sub(0, fx16_from_fx24(${camera}.z));`);
      o.line(`${pad}  record.payload.pixel_error = 0x10000;`);
      o.line(`${pad}  record.payload.geometry_tokens = ${view.budgetPct}u;`);
      o.line(`${pad}  record.payload.fragment_tokens = ${view.budgetPct}u;`);
      o.line(`${pad}  std::vector<u8> bytes;`);
      o.line(`${pad}  zhao_abi::zhao_pack_set_view(record, bytes);`);
      o.line(`${pad}  builder.append_record(bytes);`);
      o.line(`${pad}}`);
    }
  }

  private emitCommand(o: Lines, emit: HirPresentation['emits'][number], depth: number): void {
    const pad = '  '.repeat(depth);
    const ctx: ExprContext = { state: 'state', pads: 'pads', tick: 'tick' };
    const arg = (name: string): HirExpr | null => emit.args.find((item) => item.name === name)?.value ?? null;
    const value = (name: string, fallback = '0u'): string => arg(name) ? this.expr(arg(name)!, ctx) : fallback;
    const begin = (record: string, opcode: string, bytes: number): void => {
      o.line(`${pad}{`);
      o.line(`${pad}  zhao_abi::ZhRecord${record} record{};`);
      o.line(`${pad}  record.hdr.opcode = zhao_abi::ZHAO_OP_${opcode};`);
      o.line(`${pad}  record.hdr.record_bytes = ${bytes}u;`);
      o.line(`${pad}  record.hdr.source_id = ${emit.sourceId}u;`);
    };
    const end = (pack: string): void => {
      o.line(`${pad}  std::vector<u8> bytes;`);
      o.line(`${pad}  zhao_abi::zhao_pack_${pack}(record, bytes);`);
      o.line(`${pad}  builder.append_record(bytes);`);
      o.line(`${pad}}`);
    };
    switch (emit.emitKind) {
      case 'draw_population': {
        begin('DrawPopulation', 'DRAW_POPULATION', 32);
        const poolArg = arg('pool');
        const pool = poolArg?.symbol?.kind === 'pool' && poolArg.symbol.module !== null
          ? this.pools.get(key(poolArg.symbol.module, poolArg.symbol.name)) : null;
        o.line(`${pad}  record.payload.population = ${pool ? `(0x01000000u | ${pool.populationIndex}u)` : '0u'};`);
        o.line(`${pad}  record.payload.viewport_mask = static_cast<u8>(${value('view_mask')});`);
        o.line(`${pad}  record.payload.semantic_weight = static_cast<u8>(${value('weight')});`);
        end('draw_population');
        break;
      }
      case 'draw_form':
        begin('DrawForm', 'DRAW_FORM', 32);
        o.line(`${pad}  record.payload.form = static_cast<u32>(${value('form')});`);
        o.line(`${pad}  record.payload.transform = 0u;`);
        o.line(`${pad}  record.payload.viewport_mask = static_cast<u8>(${value('view_mask')});`);
        o.line(`${pad}  record.payload.semantic_weight = static_cast<u8>(${value('weight')});`);
        end('draw_form');
        break;
      case 'draw_procedural':
        begin('DrawProcedural', 'DRAW_PROCEDURAL', 64);
        o.line(`${pad}  record.payload.program = static_cast<u32>(${value('patch')});`);
        o.line(`${pad}  record.payload.screen_error = static_cast<i32>(${value('screen_error')});`);
        end('draw_procedural');
        break;
      case 'surface_stamp':
        begin('SurfaceStamp', 'SURFACE_STAMP', 64);
        o.line(`${pad}  record.payload.brush = static_cast<u32>(${value('brush')});`);
        o.line(`${pad}  record.payload.radius = static_cast<i32>(${value('radius')});`);
        o.line(`${pad}  record.payload.ring_width = static_cast<i32>(${value('ring_width')});`);
        o.line(`${pad}  record.payload.tag = static_cast<u8>(${value('tag')});`);
        o.line(`${pad}  record.payload.strength = static_cast<u16>(${value('strength')});`);
        end('surface_stamp');
        break;
      case 'audio': {
        begin('EmitAudioEvent', 'EMIT_AUDIO_EVENT', 32);
        const soundArg = arg('sound');
        const sound = soundArg?.symbol?.kind === 'sound' && soundArg.symbol.module !== null
          ? this.sounds.get(key(soundArg.symbol.module, soundArg.symbol.name)) : null;
        o.line(`${pad}  record.payload.event_id = ${sound?.eventIndex ?? 0}u;`);
        o.line(`${pad}  record.payload.sample_handle = 0x${(0x01000000 | (sound?.eventIndex ?? 0)).toString(16)}u;`);
        if (sound) {
          const gain = sound.params.find((item) => item.kind === 'gain');
          const pan = sound.params.find((item) => item.kind === 'pan');
          if (gain) o.line(`${pad}  record.payload.gain = static_cast<u16>(${this.expr(gain.value, ctx)});`);
          if (pan) o.line(`${pad}  record.payload.pan_fx = static_cast<i16>(${this.expr(pan.value, ctx)});`);
        }
        end('emit_audio_event');
        break;
      }
      default:
        throw new Error(`C++ emitter does not know presentation command '${emit.emitKind}'`);
    }
  }

  private emitScenario(o: Lines, scenario: HirScenario): void {
    o.line(`void scenario_${ident(scenario.name)}(FormState& state, u32 cartridge_hash) {`);
    o.line('  initialize(state, cartridge_hash);');
    const seed = scenario.items.find((item) => item.ast.kind === 'seed')?.ast as Extract<ScenarioItem, { kind: 'seed' }> | undefined;
    if (seed) {
      o.line(`  for (u32 i = 0u; i < state.rng.size(); ++i) state.rng[i] = random_stream(${seed.value.toString()}u, i);`);
    }
    o.line('}');
    o.line();
  }

  private gameHeader(): string {
    const o = new Lines();
    const modules = [...this.hir.modules].sort((a, b) => a.index - b.index);
    o.line(this.banner());
    o.line('#pragma once');
    o.line();
    o.line('#include "form_types.hpp"');
    o.line('#include <array>');
    for (const module of modules) o.line(`#include "${module.name}.hpp"`);
    o.line();
    o.line('namespace form {');
    o.line(`inline constexpr u32 kProgramManifestCrc32c = 0x${hex8(this.hir.manifestCrc32c)}u;`);
    o.line(`inline constexpr std::size_t kCanonicalStateMaxBytes = ${this.canonicalMaxBytes()}u;`);
    o.line();
    o.line('struct FormState {');
    for (const module of modules) o.line(`  ${this.moduleName(module.index)}::State ${this.moduleName(module.index)}{};`);
    o.line(`  std::array<Stream, ${this.zir.sim.rngStateCount}> rng{};`);
    o.line('};');
    o.line();
    o.line('inline void initialize(FormState& state, u32 cartridge_hash) {');
    o.line('  state = FormState{};');
    o.line('  (void)cartridge_hash;');
    for (const global of declarationsOf(this.hir, 'global')) {
      o.line(`  ${this.stateMember(global.module, global.name, 'state')} = ${this.expr(global.init, { state: 'state', pads: 'pads', tick: 'tick' })};`);
    }
    o.line('}');
    o.line();
    this.emitWriter(o);
    this.emitSerializer(o);
    this.emitHash(o);
    this.emitSimTick(o);
    this.emitPresentFrame(o);
    this.emitScenarioTable(o);
    o.line('}  // namespace form');
    return o.finish();
  }

  private emitWriter(o: Lines): void {
    o.line('struct CanonicalWriter {');
    o.line('  u8* out;');
    o.line('  std::size_t capacity;');
    o.line('  std::size_t used{};');
    o.line('  void byte(u8 value) { if (used < capacity) out[used] = value; ++used; }');
    o.line('  void u16le(u16 value) { byte(static_cast<u8>(value)); byte(static_cast<u8>(value >> 8u)); }');
    o.line('  void u32le(u32 value) { u16le(static_cast<u16>(value)); u16le(static_cast<u16>(value >> 16u)); }');
    o.line('  void u64le(std::uint64_t value) { u32le(static_cast<u32>(value)); u32le(static_cast<u32>(value >> 32u)); }');
    o.line('};');
    o.line();
  }

  private emitSerializer(o: Lines): void {
    o.line('inline std::size_t serialize_canonical_state(const FormState& state, u8* out, std::size_t capacity) {');
    o.line('  CanonicalWriter writer{out, capacity, 0u};');
    let unique = 0;
    for (const module of [...this.hir.modules].sort((a, b) => a.index - b.index)) {
      const decls = this.hir.declarations.filter((decl) => decl.module === module.index && (decl.kind === 'pool' || decl.kind === 'global')) as (HirPool | HirGlobal)[];
      for (const decl of decls) {
        if (decl.kind === 'global') {
          for (const line of this.serializeType(decl.type, this.stateMember(decl.module, decl.name, 'state'), 'writer', () => `_s${unique++}`)) o.line(`  ${line}`);
        } else {
          const poolExpr = this.stateMember(decl.module, decl.name, 'state');
          const struct = this.structs.get(key(decl.structModule, decl.structName))!;
          const index = `_pool${unique++}`;
          const count = `${index}_count`;
          o.line(`  const u32 ${count} = ${poolExpr}.count;`);
          o.line(`  if (${count} > ${this.moduleName(decl.module)}::${ident(decl.name)}_pool::capacity) form_abort(822u);`);
          o.line(`  writer.u32le(${count});`);
          o.line(`  for (u32 ${index} = 0u; ${index} < ${count}; ++${index}) {`);
          for (const field of struct.fields) {
            for (const line of this.serializeType(field.type, `${poolExpr}.${ident(field.name)}[${index}]`, 'writer', () => `_s${unique++}`)) o.line(`    ${line}`);
          }
          o.line('  }');
        }
      }
    }
    o.line('  for (const Stream& stream : state.rng) { writer.u64le(stream.state); writer.u64le(stream.increment); }');
    o.line('  return writer.used <= capacity ? writer.used : 0u;');
    o.line('}');
    o.line();
  }

  private serializeType(type: Type, expression: string, writer: string, variable: () => string): string[] {
    switch (type.t) {
      case 'unit8': case 'bool': return [`${writer}.byte(static_cast<u8>(${expression}));`];
      case 'angle16': return [`${writer}.u16le(static_cast<u16>(${expression}));`];
      case 'fx16': case 'i32': case 'u32': case 'colour8': case 'enum': return [`${writer}.u32le(static_cast<u32>(${expression}));`];
      case 'fx24': return [`${writer}.u64le(static_cast<std::uint64_t>(${expression}));`];
      case 'world2': return [...this.serializeType({ t: 'fx24' }, `${expression}.x`, writer, variable), ...this.serializeType({ t: 'fx24' }, `${expression}.y`, writer, variable)];
      case 'world3': case 'velocity3': return [
        ...this.serializeType({ t: 'fx24' }, `${expression}.x`, writer, variable),
        ...this.serializeType({ t: 'fx24' }, `${expression}.y`, writer, variable),
        ...this.serializeType({ t: 'fx24' }, `${expression}.z`, writer, variable),
      ];
      case 'array': {
        const rows: string[] = [];
        for (let i = 0; i < type.len; i++) rows.push(...this.serializeType(type.elem, `${expression}[${i}u]`, writer, variable));
        return rows;
      }
      case 'struct': {
        const [module, name] = splitTypeName(type.name);
        const struct = this.structs.get(key(module, name));
        if (!struct) throw new Error(`C++ serializer cannot find struct ${type.name}`);
        return struct.fields.flatMap((field) => this.serializeType(field.type, `${expression}.${ident(field.name)}`, writer, variable));
      }
      default:
        throw new Error(`C++ serializer cannot serialize truth type '${type.t}'`);
    }
  }

  private emitHash(o: Lines): void {
    o.line('inline u32 crc32c_update(u32 crc, const u8* bytes, std::size_t count) {');
    o.line('  u32 value = crc ^ 0xffffffffu;');
    o.line('  for (std::size_t i = 0u; i < count; ++i) {');
    o.line('    value ^= bytes[i];');
    o.line('    for (u32 bit = 0u; bit < 8u; ++bit) value = (value >> 1u) ^ (0x82f63b78u & (0u - (value & 1u)));');
    o.line('  }');
    o.line('  return value ^ 0xffffffffu;');
    o.line('}');
    o.line('inline u32 sim_hash_initial(u32 cartridge_hash) {');
    o.line('  const u8 identity[8] = {');
    o.line('    static_cast<u8>(kProgramManifestCrc32c), static_cast<u8>(kProgramManifestCrc32c >> 8u),');
    o.line('    static_cast<u8>(kProgramManifestCrc32c >> 16u), static_cast<u8>(kProgramManifestCrc32c >> 24u),');
    o.line('    static_cast<u8>(cartridge_hash), static_cast<u8>(cartridge_hash >> 8u),');
    o.line('    static_cast<u8>(cartridge_hash >> 16u), static_cast<u8>(cartridge_hash >> 24u)');
    o.line('  };');
    o.line('  return crc32c_update(0u, identity, 8u);');
    o.line('}');
    o.line('inline u32 sim_hash(u32 previous, const FormState& state) {');
    o.line('  std::array<u8, kCanonicalStateMaxBytes> bytes{};');
    o.line('  const std::size_t count = serialize_canonical_state(state, bytes.data(), bytes.size());');
    o.line('  const u8 prefix[4] = { static_cast<u8>(previous), static_cast<u8>(previous >> 8u), static_cast<u8>(previous >> 16u), static_cast<u8>(previous >> 24u) };');
    o.line('  return crc32c_update(crc32c_update(0u, prefix, 4u), bytes.data(), count);');
    o.line('}');
    o.line();
  }

  private emitSimTick(o: Lines): void {
    o.line('inline void sim_tick(FormState& state, const PadFrame pads[4], u32 tick) {');
    let currentPhase = -1;
    for (const system of this.zir.sim.callList) {
      if (system.phase !== currentPhase) {
        currentPhase = system.phase;
        o.line(`  // schedule phase ${currentPhase}`);
      }
      const call = `${this.moduleName(system.module)}::system_${ident(system.name)}(state, pads, tick);`;
      if (system.rateGuard) o.line(`  if ((tick % ${system.rateGuard.divisor}u) == 0u) ${call}`);
      else o.line(`  ${call}`);
    }
    o.line('}');
    o.line();
  }

  private emitPresentFrame(o: Lines): void {
    o.line('inline void present_frame(const FormState& state, zref::FrameBuilder& builder) {');
    for (const presentation of declarationsOf(this.hir, 'presentation')) {
      o.line(`  ${this.moduleName(presentation.module)}::present_${ident(presentation.name)}(state, builder);`);
    }
    o.line('}');
    o.line();
  }

  private emitScenarioTable(o: Lines): void {
    const scenarios = declarationsOf(this.hir, 'scenario');
    o.line('using ScenarioEntryFn = void (*)(FormState&, u32);');
    o.line('struct ScenarioEntry { const char* name; u32 module; u32 source_id; ScenarioEntryFn enter; };');
    o.line(`inline constexpr std::array<ScenarioEntry, ${scenarios.length}> kScenarioEntries{{`);
    for (const scenario of scenarios) {
      o.line(`  ScenarioEntry{"${escapeCpp(scenario.name)}", ${scenario.module}u, ${scenario.sourceId}u, &${this.moduleName(scenario.module)}::scenario_${ident(scenario.name)}},`);
    }
    o.line('}};');
    o.line();
  }

  private expr(expression: HirExpr, ctx: ExprContext): string {
    const ast = expression.ast;
    switch (ast.kind) {
      case 'literal': return cppInteger(literalRaw(ast, expression.type), expression.type);
      case 'string': return `"${escapeCpp(ast.value)}"`;
      case 'ident': return this.identExpr(expression, ast.name, ctx);
      case 'member': {
        if (expression.symbol?.kind === 'global' && expression.symbol.module !== null) return this.stateMember(expression.symbol.module, expression.symbol.name, ctx.state);
        if (expression.symbol?.kind === 'enum' && expression.symbol.module !== null) {
          const [enumName, member] = expression.symbol.name.split('.');
          return `${this.cppStructName(expression.symbol.module, enumName!)}::${ident(member!)}`;
        }
        if (expression.symbol?.kind === 'pool' && expression.symbol.module !== null) {
          const pool = this.stateMember(expression.symbol.module, expression.symbol.name, ctx.state);
          return ast.field === 'count' ? `${pool}.count` : `${pool}.${ident(ast.field)}`;
        }
        return `(${this.expr(expression.children[0]!, ctx)}).${ident(ast.field)}`;
      }
      case 'index': {
        const values = this.expr(expression.children[0]!, ctx);
        const index = `static_cast<u32>(${this.expr(expression.children[1]!, ctx)})`;
        const symbol = expression.children[0]!.symbol;
        if (symbol?.kind === 'pool' && symbol.module !== null) {
          return `checked_index(${values}, ${index}, ${this.stateMember(symbol.module, symbol.name, ctx.state)}.count)`;
        }
        return `checked_index(${values}, ${index}, static_cast<u32>(${values}.size()))`;
      }
      case 'call': return this.callExpr(expression, ctx);
      case 'unary': {
        const operand = this.expr(expression.children[0]!, ctx);
        if (ast.op === '-' && expression.type.t === 'fx16') return `fx16_sub(0, ${operand})`;
        if (ast.op === '-' && expression.type.t === 'fx24') return `fx24_sub(0, ${operand})`;
        return `(${ast.op}${operand})`;
      }
      case 'binary': return this.binaryExpr(expression, ast.op, ctx);
      case 'if_expr': return `select_value(${this.expr(expression.children[0]!, ctx)}, ${this.expr(expression.children[1]!, ctx)}, ${this.expr(expression.children[2]!, ctx)})`;
      case 'record': return this.recordExpr(expression, ast, ctx);
      case 'range': throw new Error('C++ emitter received range as value expression');
    }
  }

  private identExpr(expression: HirExpr, authored: string, ctx: ExprContext): string {
    const symbol = expression.symbol;
    if (!symbol) return ident(authored);
    if (symbol.module === null) {
      if (symbol.kind === 'const' && BUTTONS.has(symbol.name)) return `${BUTTONS.get(symbol.name)}u`;
      return ident(symbol.name);
    }
    if (symbol.kind === 'global' || symbol.kind === 'pool') return this.stateMember(symbol.module, symbol.name, ctx.state);
    if (symbol.kind === 'const') return `${this.moduleName(symbol.module)}::${ident(symbol.name)}`;
    if (symbol.kind === 'sound') return `${this.sounds.get(key(symbol.module, symbol.name))?.eventIndex ?? 0}u`;
    return ident(authored);
  }

  private binaryExpr(expression: HirExpr, op: string, ctx: ExprContext): string {
    const left = this.expr(expression.children[0]!, ctx);
    const right = this.expr(expression.children[1]!, ctx);
    if (expression.type.t === 'fx16') {
      if (op === '+') return `fx16_add(${left}, ${right})`;
      if (op === '-') return `fx16_sub(${left}, ${right})`;
      if (op === '*') return `fx16_mul(${left}, ${right})`;
    }
    if (expression.type.t === 'fx24') {
      if (op === '+') return `fx24_add(${left}, ${right})`;
      if (op === '-') return `fx24_sub(${left}, ${right})`;
      if (op === '*') return `fx24_mul(${left}, ${right})`;
    }
    if (expression.type.t === 'unit8') {
      if (op === '+') return `unit_add(${left}, ${right})`;
      if (op === '-') return `unit_sub(${left}, ${right})`;
      if (op === '*') return `unit_mul(${left}, ${right})`;
    }
    if (expression.type.t === 'world2' && (op === '+' || op === '-')) return `world2_${op === '+' ? 'add' : 'sub'}(${left}, ${right})`;
    if (expression.type.t === 'world3' && (op === '+' || op === '-')) return `world3_${op === '+' ? 'add' : 'sub'}(${left}, ${right})`;
    if (expression.type.t === 'velocity3' && (op === '+' || op === '-')) return `velocity3_${op === '+' ? 'add' : 'sub'}(${left}, ${right})`;
    return `(${left} ${op} ${right})`;
  }

  private callExpr(expression: HirExpr, ctx: ExprContext): string {
    const ast = expression.ast as Extract<Expr, { kind: 'call' }>;
    const args = expression.children.map((child) => this.expr(child, ctx));
    const symbol = expression.symbol;
    if (symbol?.kind === 'fn' && symbol.module !== null) return `${this.moduleName(symbol.module)}::${ident(symbol.name)}(${args.join(', ')})`;
    if (symbol?.kind === 'system' && symbol.module !== null) return `${this.moduleName(symbol.module)}::system_${ident(symbol.name)}(${ctx.state}, ${ctx.pads}, ${ctx.tick})`;
    const name = callName(ast.callee);
    if (name === 'input.player') return `${ctx.pads}[static_cast<u32>(${args[0] ?? '0u'}) & 3u]`;
    if (name === 'input.held') return `input_held(${args[0]}, ${args[1]})`;
    if (name === 'random.stream') return `random_stream(${args.join(', ')})`;
    if (name === 'random.u32') return `random_u32(${args[0]})`;
    if (name === 'random.i32') return `static_cast<i32>(random_u32(${args[0]}))`;
    if (name === 'min') return `min_value(${args.join(', ')})`;
    if (name === 'max') return `max_value(${args.join(', ')})`;
    if (name === 'clamp') return `clamp_value(${args.join(', ')})`;
    if (name === 'abs') return `abs_value(${args[0]})`;
    return `${ident(name.replaceAll('.', '_'))}(${args.join(', ')})`;
  }

  private recordExpr(expression: HirExpr, ast: RecordLit, ctx: ExprContext): string {
    if (expression.type.t === 'world2' || expression.type.t === 'world3' || expression.type.t === 'velocity3') {
      const names = expression.type.t === 'world2' ? ['x', 'y'] : ['x', 'y', 'z'];
      const type = expression.type.t === 'world2' ? 'World2' : expression.type.t === 'world3' ? 'World3' : 'Velocity3';
      const byName = new Map(ast.fields.map((field, index) => [field.name, this.expr(expression.children[index]!, ctx)]));
      return `${type}{${names.map((name) => byName.get(name) ?? '0').join(', ')}}`;
    }
    if (expression.type.t !== 'struct') throw new Error(`C++ emitter cannot construct record type '${expression.type.t}'`);
    const [module, name] = splitTypeName(expression.type.name);
    const struct = this.structs.get(key(module, name));
    if (!struct) throw new Error(`C++ emitter cannot find record struct ${expression.type.name}`);
    const values = this.recordValues(expression, struct, ctx);
    return `${this.cppStructName(module, name)}{${struct.fields.map((field) => values.get(field.name) ?? '{}').join(', ')}}`;
  }

  private recordValues(expression: HirExpr, struct: HirStruct, ctx: ExprContext = { state: 'state', pads: 'pads', tick: 'tick' }): Map<string, string> {
    const ast = expression.ast as RecordLit;
    return new Map(ast.fields.map((field, index) => [field.name, this.expr(expression.children[index]!, ctx)]));
  }

  private cppType(type: Type): string {
    switch (type.t) {
      case 'fx16': return 'Fx16'; case 'fx24': return 'Fx24'; case 'angle16': return 'Angle16';
      case 'unit8': return 'Unit8'; case 'i32': return 'i32'; case 'u32': return 'u32';
      case 'bool': return 'Bool'; case 'world2': return 'World2'; case 'world3': return 'World3';
      case 'velocity3': return 'Velocity3'; case 'colour8': return 'Colour8';
      case 'padframe': return 'PadFrame'; case 'stream': return 'Stream'; case 'void': return 'void';
      case 'enum': case 'struct': { const [module, name] = splitTypeName(type.name); return this.cppStructName(module, name); }
      case 'array': return `std::array<${this.cppType(type.elem)}, ${type.len}>`;
      default: throw new Error(`C++ emitter cannot lower type '${type.t}'`);
    }
  }

  private cppStructName(module: number, name: string): string { return `form::${this.moduleName(module)}::${ident(name)}`; }
  private moduleName(module: number): string { const name = this.moduleNames.get(module); if (!name) throw new Error(`unknown HIR module ${module}`); return name; }
  private stateMember(module: number, name: string, state: string): string { return `${state}.${this.moduleName(module)}.${ident(name)}`; }
  private moduleForSpan(file: string): number { return this.hir.modules.find((module) => module.file === file)?.index ?? 0; }
  private resolveDeclaration(moduleIndex: number, name: string): HirDeclaration | null {
    const own = this.declByKey.get(key(moduleIndex, name));
    if (own) return own;
    const module = this.modules.get(moduleIndex);
    if (!module) return null;
    for (const imported of module.imports) {
      if (!imported.names.includes(name)) continue;
      const found = this.declByKey.get(key(imported.module, name));
      if (found) return found;
    }
    return null;
  }

  private canonicalMaxBytes(): number {
    let total = this.zir.sim.rngStateCount * 16;
    for (const decl of this.hir.declarations) {
      if (decl.kind === 'global') total += this.typeBytes(decl.type);
      if (decl.kind === 'pool') total += 4 + decl.capacity * decl.elementBytes;
    }
    if (!Number.isSafeInteger(total) || total < 0) throw new Error('canonical FormState maximum size overflow');
    return total;
  }

  private typeBytes(type: Type): number {
    switch (type.t) {
      case 'unit8': case 'bool': return 1;
      case 'angle16': return 2;
      case 'fx16': case 'i32': case 'u32': case 'colour8': case 'enum': return 4;
      case 'fx24': return 8;
      case 'world2': return 16;
      case 'world3': case 'velocity3': return 24;
      case 'array': return type.len * this.typeBytes(type.elem);
      case 'struct': {
        const [module, name] = splitTypeName(type.name);
        return this.structs.get(key(module, name))!.fields.reduce((sum, field) => sum + this.typeBytes(field.type), 0);
      }
      default: throw new Error(`truth type '${type.t}' has no canonical width`);
    }
  }
}

function ident(value: string): string {
  const clean = value.replace(/[^A-Za-z0-9_]/g, '_');
  const prefixed = /^[0-9]/.test(clean) ? `_${clean}` : clean;
  return CPP_KEYWORDS.has(prefixed) ? `form_${prefixed}` : prefixed;
}

function key(module: number, name: string): string { return `${module}\0${name}`; }
function splitTypeName(name: string): [number, string] {
  const split = name.indexOf('::');
  if (split < 0) throw new Error(`unqualified HIR type '${name}'`);
  return [Number(name.slice(0, split)), name.slice(split + 2)];
}
function utf8Compare(a: string, b: string): number { return Buffer.from(a).compare(Buffer.from(b)); }
function hex8(value: number): string { return (value >>> 0).toString(16).padStart(8, '0'); }
function escapeCpp(value: string): string { return value.replaceAll('\\', '\\\\').replaceAll('"', '\\"'); }
function callName(callee: Expr): string {
  if (callee.kind === 'ident') return callee.name;
  if (callee.kind === 'member') return `${callName(callee.obj)}.${callee.field}`;
  return '<call>';
}

function literalRaw(ast: Extract<Expr, { kind: 'literal' }>, type: Type): bigint {
  if (ast.lit === 'bool') return ast.text === 'true' ? 1n : 0n;
  if (ast.lit === 'colour') {
    const digits = ast.text.startsWith('#') ? ast.text.slice(1) : ast.text;
    const rgb = BigInt(`0x${digits}`);
    return digits.length === 6 ? 0xff000000n | rgb : rgb;
  }
  if (ast.lit === 'int' || ast.lit === 'tick') {
    const value = ast.intVal ?? 0n;
    return type.t === 'fx16' ? value << 16n : type.t === 'fx24' ? value << 24n : value;
  }
  const frac = ast.frac!;
  const numerator = BigInt(frac.intDigits + frac.fracDigits);
  const denominator = 10n ** BigInt(frac.fracDigits.length);
  if (type.t === 'fx16') return (numerator << 16n) / denominator;
  if (type.t === 'fx24') return (numerator << 24n) / denominator;
  if (type.t === 'angle16') return ((numerator << 16n) / (denominator * (frac.suffix === 'deg' ? 360n : 1n))) & 0xffffn;
  if (type.t === 'unit8') {
    const raw = (numerator * 256n + denominator * 50n) / (denominator * 100n);
    return raw > 255n ? 255n : raw;
  }
  return numerator / denominator;
}

function cppInteger(value: bigint, type: Type): string {
  if (type.t === 'fx24') return `${value.toString()}LL`;
  if (type.t === 'u32' || type.t === 'colour8') return `${value.toString()}u`;
  if (type.t === 'angle16' || type.t === 'unit8' || type.t === 'bool') return `static_cast<${type.t === 'angle16' ? 'Angle16' : type.t === 'unit8' ? 'Unit8' : 'Bool'}>(${value.toString()}u)`;
  if (type.t === 'enum') return `static_cast<u32>(${value.toString()}u)`;
  return value.toString();
}
