// exact_constant.ts — shared typed, exact constant reduction for checker and HIR.

import type { Expr } from './ast.js';

export interface ExactConstantType {
  readonly t: string;
}

export interface ExactConstantReference<C> {
  readonly context: C;
  readonly expression: Expr;
  readonly type: ExactConstantType;
  readonly key: string;
}

export interface ExactEnumMember {
  readonly type: ExactConstantType;
  readonly value: bigint;
}

export interface ExactRecordValue {
  readonly kind: 'record';
  readonly fields: ReadonlyMap<string, ExactConstantValue>;
}

export interface ExactArrayValue {
  readonly kind: 'array';
  readonly elements: readonly ExactConstantValue[];
}

/** Internal exact value domain. Public declaration bounds still require a scalar. */
export type ExactConstantValue = bigint | ExactRecordValue | ExactArrayValue;

export interface ExactConstantBindings<C> {
  typeOf(context: C, expression: Expr): ExactConstantType | null;
  constant(context: C, expression: Expr): ExactConstantReference<C> | null;
  enumMember(context: C, expression: Expr): ExactEnumMember | null;
  /** Exact declared field type, resolved in the aggregate declaration's owner. */
  memberType?(context: C, aggregate: ExactConstantType, field: string): ExactConstantType | null;
  /** Exact element type for a fixed array. */
  elementType?(context: C, aggregate: ExactConstantType): ExactConstantType | null;
  /** Optional compiler-owned aggregate source (there is no authored array literal in L1). */
  aggregateValue?(context: C, expression: Expr): ExactRecordValue | ExactArrayValue | null;
}

/**
 * Reduces a scalar constant in its authored type. Arithmetic is normalized at
 * every operation exactly as the runtime is: u32/i32 wrap, Q formats saturate,
 * shift counts use their low five bits, and boolean results are canonical 0/1.
 * Null means the expression cannot be reduced exactly; callers that require a
 * declaration bound must diagnose it rather than inventing a fallback value.
 */
export function evaluateExactConstant<C>(
  context: C,
  expression: Expr,
  expected: ExactConstantType | null,
  bindings: ExactConstantBindings<C>,
  active: Set<string> = new Set(),
): bigint | null {
  const value = evaluateExactConstantValue(context, expression, expected, bindings, active);
  return typeof value === 'bigint' ? value : null;
}

/**
 * Shared exact reducer for scalar and aggregate constant values. Record fields
 * retain authored names, and member/index projection never substitutes a value
 * when the aggregate, field, index, or dependency cannot be reduced exactly.
 */
export function evaluateExactConstantValue<C>(
  context: C,
  expression: Expr,
  expected: ExactConstantType | null,
  bindings: ExactConstantBindings<C>,
  active: Set<string> = new Set(),
): ExactConstantValue | null {
  const reference = bindings.constant(context, expression);
  if (reference !== null) {
    if (active.has(reference.key)) return null;
    active.add(reference.key);
    const value = evaluateExactConstantValue(
      reference.context,
      reference.expression,
      reference.type,
      bindings,
      active,
    );
    active.delete(reference.key);
    return value === null ? null : normalizeExactValue(reference.context, value, reference.type, bindings);
  }

  const enumMember = bindings.enumMember(context, expression);
  if (enumMember !== null) return normalizeExactConstant(enumMember.value, enumMember.type);

  const expressionType = bindings.typeOf(context, expression) ?? expected;
  const aggregate = bindings.aggregateValue?.(context, expression) ?? null;
  if (aggregate !== null) {
    return expressionType === null
      ? aggregate : normalizeExactValue(context, aggregate, expressionType, bindings);
  }

  switch (expression.kind) {
    case 'literal':
      return exactLiteral(expression, expressionType);
    case 'unary': {
      const operandType = bindings.typeOf(context, expression.operand)
        ?? (expression.op === '!' ? { t: 'bool' } : expressionType);
      const operand = evaluateExactConstantValue(context, expression.operand, operandType, bindings, active);
      if (typeof operand !== 'bigint') return null;
      if (expression.op === '!') return operand === 0n ? 1n : 0n;
      if (expression.op === '-') {
        return expressionType === null ? -operand : normalizeExactConstant(-operand, expressionType);
      }
      if (expression.op === '~') {
        return expressionType === null ? ~operand : normalizeExactConstant(~operand, expressionType);
      }
      return null;
    }
    case 'binary': {
      const logical = expression.op === '&&' || expression.op === '||';
      const comparison = ['<', '<=', '>', '>=', '==', '!='].includes(expression.op);
      const knownLeft = bindings.typeOf(context, expression.l);
      const knownRight = bindings.typeOf(context, expression.r);
      const operandType = knownLeft ?? knownRight
        ?? (logical ? { t: 'bool' } : comparison ? { t: 'i32' } : expressionType);
      const left = evaluateExactConstantValue(context, expression.l, operandType, bindings, active);
      const right = evaluateExactConstantValue(context, expression.r, operandType, bindings, active);
      if (typeof left !== 'bigint' || typeof right !== 'bigint') return null;
      return exactBinary(expression.op, left, right, operandType, expressionType);
    }
    case 'record': {
      const fields = new Map<string, ExactConstantValue>();
      for (const field of expression.fields) {
        const fieldType = expressionType === null
          ? bindings.typeOf(context, field.value)
          : bindings.memberType?.(context, expressionType, field.name)
            ?? bindings.typeOf(context, field.value);
        const value = evaluateExactConstantValue(context, field.value, fieldType, bindings, active);
        if (value === null) return null;
        fields.set(field.name, value);
      }
      return { kind: 'record', fields };
    }
    case 'member': {
      const objectType = bindings.typeOf(context, expression.obj);
      const value = evaluateExactConstantValue(context, expression.obj, objectType, bindings, active);
      if (value === null || typeof value === 'bigint' || value.kind !== 'record') return null;
      return value.fields.get(expression.field) ?? null;
    }
    case 'index': {
      const objectType = bindings.typeOf(context, expression.obj);
      const value = evaluateExactConstantValue(context, expression.obj, objectType, bindings, active);
      const index = evaluateExactConstantValue(context, expression.index, { t: 'u32' }, bindings, active);
      if (value === null || typeof value === 'bigint' || value.kind !== 'array'
          || typeof index !== 'bigint' || index < 0n || index >= BigInt(value.elements.length)) return null;
      return value.elements[Number(index)] ?? null;
    }
    default:
      return null;
  }
}

function normalizeExactValue<C>(
  context: C,
  value: ExactConstantValue,
  type: ExactConstantType,
  bindings: ExactConstantBindings<C>,
): ExactConstantValue {
  if (typeof value === 'bigint') return normalizeExactConstant(value, type);
  if (value.kind === 'record') {
    const fields = new Map<string, ExactConstantValue>();
    for (const [name, field] of value.fields) {
      const fieldType = bindings.memberType?.(context, type, name) ?? null;
      fields.set(name, fieldType === null ? field : normalizeExactValue(context, field, fieldType, bindings));
    }
    return { kind: 'record', fields };
  }
  const elementType = bindings.elementType?.(context, type) ?? null;
  return {
    kind: 'array',
    elements: elementType === null
      ? value.elements
      : value.elements.map((element) => normalizeExactValue(context, element, elementType, bindings)),
  };
}

function exactLiteral(
  literal: Extract<Expr, { kind: 'literal' }>,
  type: ExactConstantType | null,
): bigint | null {
  if (literal.lit === 'bool') return literal.text === 'true' ? 1n : 0n;
  if (literal.lit === 'colour') {
    const digits = literal.text.startsWith('#') ? literal.text.slice(1) : literal.text;
    const rgb = BigInt(`0x${digits}`);
    return digits.length === 6 ? 0xff000000n | rgb : rgb;
  }
  if (literal.lit === 'int' || literal.lit === 'tick') {
    const value = literal.intVal ?? 0n;
    if (type?.t === 'fx16') return value << 16n;
    if (type?.t === 'fx24') return value << 24n;
    return type === null ? value : normalizeExactConstant(value, type);
  }
  const fraction = literal.frac;
  if (!fraction) return null;
  const numerator = BigInt(fraction.intDigits + fraction.fracDigits);
  const denominator = 10n ** BigInt(fraction.fracDigits.length);
  if (type?.t === 'fx16') return (numerator << 16n) / denominator;
  if (type?.t === 'fx24') return (numerator << 24n) / denominator;
  if (type?.t === 'angle16') {
    const turns = denominator * (fraction.suffix === 'deg' ? 360n : 1n);
    return normalizeExactConstant((numerator << 16n) / turns, type);
  }
  if (type?.t === 'unit8') {
    const raw = (numerator * 256n + denominator * 50n) / (denominator * 100n);
    return normalizeExactConstant(raw, type);
  }
  return numerator === 0n ? 0n : null;
}

function exactBinary(
  operator: string,
  left: bigint,
  right: bigint,
  operandType: ExactConstantType | null,
  resultType: ExactConstantType | null,
): bigint | null {
  const normalizeOperand = (value: bigint): bigint => operandType === null
    ? value : normalizeExactConstant(value, operandType);
  const normalizeResult = (value: bigint): bigint => resultType === null
    ? normalizeOperand(value) : normalizeExactConstant(value, resultType);
  const kind = operandType?.t;
  switch (operator) {
    case '+': return normalizeResult(left + right);
    case '-': return normalizeResult(left - right);
    case '*': {
      if (kind === 'fx16') return normalizeResult(floorDiv(left * right + 0x8000n, 0x10000n));
      if (kind === 'fx24') return normalizeResult(roundHalfUp(left * right, 0x1000000n));
      if (kind === 'unit8') {
        const product = left * right + 128n;
        return product > 0xff00n ? 255n : product >> 8n;
      }
      return normalizeResult(left * right);
    }
    case '/': {
      if (right === 0n) return null;
      if (kind === 'fx16') return normalizeResult(roundHalfUp(left * 0x10000n, right));
      if (kind === 'fx24') return normalizeResult(roundHalfUp(left * 0x1000000n, right));
      if (kind === 'i32' && left === -0x80000000n && right === -1n) return left;
      return normalizeResult(left / right);
    }
    case '%': {
      if (right === 0n) return null;
      if ((kind === 'i32' || kind === 'fx16') && left === -0x80000000n && right === -1n) return 0n;
      if (kind === 'fx24' && left === -0x8000000000000000n && right === -1n) return 0n;
      return normalizeResult(left % right);
    }
    case '<<': {
      const shift = Number(BigInt.asUintN(32, right) & 31n);
      return kind === 'i32'
        ? BigInt.asIntN(32, BigInt.asUintN(32, left) << BigInt(shift))
        : normalizeResult(BigInt.asUintN(32, left) << BigInt(shift));
    }
    case '>>': {
      const shift = Number(BigInt.asUintN(32, right) & 31n);
      return kind === 'i32'
        ? BigInt.asIntN(32, left) >> BigInt(shift)
        : normalizeResult(BigInt.asUintN(32, left) >> BigInt(shift));
    }
    case '&': return normalizeResult(left & right);
    case '|': return normalizeResult(left | right);
    case '^': return normalizeResult(left ^ right);
    case '<': return left < right ? 1n : 0n;
    case '<=': return left <= right ? 1n : 0n;
    case '>': return left > right ? 1n : 0n;
    case '>=': return left >= right ? 1n : 0n;
    case '==': return left === right ? 1n : 0n;
    case '!=': return left !== right ? 1n : 0n;
    case '&&': return left !== 0n && right !== 0n ? 1n : 0n;
    case '||': return left !== 0n || right !== 0n ? 1n : 0n;
    default: return null;
  }
}

export function normalizeExactConstant(value: bigint, type: ExactConstantType): bigint {
  switch (type.t) {
    case 'fx16': return saturateSigned(value, 32);
    case 'fx24': return saturateSigned(value, 64);
    case 'i32': return BigInt.asIntN(32, value);
    case 'u32': case 'colour8': case 'enum': return BigInt.asUintN(32, value);
    case 'angle16': return BigInt.asUintN(16, value);
    case 'unit8': return value < 0n ? 0n : value > 255n ? 255n : value;
    case 'bool': return value === 0n ? 0n : 1n;
    default: return value;
  }
}

function floorDiv(numerator: bigint, denominator: bigint): bigint {
  let quotient = numerator / denominator;
  if (numerator % denominator < 0n) --quotient;
  return quotient;
}

function roundHalfUp(numerator: bigint, denominator: bigint): bigint {
  if (denominator < 0n) return roundHalfUp(-numerator, -denominator);
  return floorDiv(numerator + denominator / 2n, denominator);
}

function saturateSigned(value: bigint, bits: number): bigint {
  const magnitude = BigInt(bits - 1);
  const minimum = -(1n << magnitude);
  const maximum = (1n << magnitude) - 1n;
  return value < minimum ? minimum : value > maximum ? maximum : value;
}
