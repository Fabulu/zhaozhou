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

export interface ExactConstantBindings<C> {
  typeOf(context: C, expression: Expr): ExactConstantType | null;
  constant(context: C, expression: Expr): ExactConstantReference<C> | null;
  enumMember(context: C, expression: Expr): ExactEnumMember | null;
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
  const reference = bindings.constant(context, expression);
  if (reference !== null) {
    if (active.has(reference.key)) return null;
    active.add(reference.key);
    const value = evaluateExactConstant(
      reference.context,
      reference.expression,
      reference.type,
      bindings,
      active,
    );
    active.delete(reference.key);
    return value === null ? null : normalizeExactConstant(value, reference.type);
  }

  const enumMember = bindings.enumMember(context, expression);
  if (enumMember !== null) return normalizeExactConstant(enumMember.value, enumMember.type);

  const expressionType = bindings.typeOf(context, expression) ?? expected;
  switch (expression.kind) {
    case 'literal':
      return exactLiteral(expression, expressionType);
    case 'unary': {
      const operandType = bindings.typeOf(context, expression.operand)
        ?? (expression.op === '!' ? { t: 'bool' } : expressionType);
      const operand = evaluateExactConstant(context, expression.operand, operandType, bindings, active);
      if (operand === null) return null;
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
      const left = evaluateExactConstant(context, expression.l, operandType, bindings, active);
      const right = evaluateExactConstant(context, expression.r, operandType, bindings, active);
      if (left === null || right === null) return null;
      return exactBinary(expression.op, left, right, operandType, expressionType);
    }
    default:
      return null;
  }
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
    if (type?.t === 'fx16') return normalizeExactConstant(value << 16n, type);
    if (type?.t === 'fx24') return normalizeExactConstant(value << 24n, type);
    return type === null ? value : normalizeExactConstant(value, type);
  }
  const fraction = literal.frac;
  if (!fraction) return null;
  const numerator = BigInt(fraction.intDigits + fraction.fracDigits);
  const denominator = 10n ** BigInt(fraction.fracDigits.length);
  if (type?.t === 'fx16') return normalizeExactConstant((numerator << 16n) / denominator, type);
  if (type?.t === 'fx24') return normalizeExactConstant((numerator << 24n) / denominator, type);
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
