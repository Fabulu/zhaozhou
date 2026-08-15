// lexer.ts — hand-written Form lexer (W3.2, D2; abi-gen zero-dep pattern).
//
// Law: spec/form/language-semantics.md §1 — UTF-8 source, byte-offset spans,
// CRLF-tolerant (both bytes counted, neither in any token); `//` line
// comments and NESTING `/* */` block comments; idents 1..64 bytes;
// decimal/hex integers (0X and leading-zero octal refused); Q-format
// fractional literals with unit suffixes (m, px, w, turn, deg) and `%`;
// tick literals `120t`; colour literals `#RRGGBB` / `#AARRGGBB`; strings
// with only `\\` and `\"` escapes, 1..256 bytes; the §1.9 punctuation set.
// Domain keywords are lexed as keywords so OUT-list declarations fail as
// refusals, not unknown syntax (§1.1).

import { DiagnosticSink } from './diagnostics.js';
import { SourceSpan, span } from './span.js';

export const KEYWORDS: ReadonlySet<string> = new Set([
  'module', 'import', 'const', 'enum', 'struct', 'pool', 'global', 'system',
  'every', 'tick', 'ticks', 'stagger', 'over', 'reads', 'writes', 'fn', 'let',
  'return', 'for', 'in', 'if', 'else', 'true', 'false', 'random', 'stream',
  'field', 'footprint', 'max_ops', 'none', 'rect', 'circle', 'capsule',
  'presentation', 'view', 'from', 'budget', 'shared', 'emit', 'draw_form',
  'draw_population', 'draw_procedural', 'surface_stamp', 'sound', 'audio',
  'scenario', 'seed', 'load', 'spawn', 'player', 'at', 'assert', 'capture',
  'as', 'input', 'params', 'sample', 'material', 'nav_cost', 'height',
  'velocity', 'age', 'phase', 'dt', 'attr',
  // domain keywords — refused in L1 (§8) but lexed as keywords (§1.1)
  'build', 'warp', 'formation', 'stamp',
  // reserved forever
  'macro', 'class', 'interface', 'extern', 'f32', 'f64', 'float', 'double',
  'string', 'while', 'break', 'continue',
]);

/** Keywords usable as record/struct field names and argument labels (the
 *  field-dialect lane vocabulary, §6.2: the frozen records name their lanes
 *  with these words — language-semantics §2 grammar + §6.2 records). */
export const SOFT_KEYWORDS: ReadonlySet<string> = new Set([
  'sample', 'material', 'nav_cost', 'height', 'velocity', 'age', 'phase',
  'dt', 'attr', 'params', 'input', 'player', 'pool', 'at', 'budget', 'spawn',
  'load', 'seed', 'capture', 'assert', 'stream', 'field', 'sound',
]);

export type TokenKind =
  | 'ident'      // also keywords (kind 'kw' is separate)
  | 'kw'
  | 'int'        // 123, 0x5A17
  | 'frac'       // 1.5, 1.5m, 0.65px, 2.25w, 0.25turn, 90deg, 45%
  | 'tick'       // 120t
  | 'colour'     // #RRGGBB / #AARRGGBB
  | 'string'
  | 'punct'
  | 'bad'        // error token (diagnostic already recorded)
  | 'eof';

export interface FracInfo {
  /** integer digits (no sign — sign is a separate unary op) */
  intDigits: string;
  /** fractional digits ('' for `45%`) */
  fracDigits: string;
  /** '', 'm', 'px', 'w', 'turn', 'deg', '%' */
  suffix: string;
}

export interface Token {
  kind: TokenKind;
  text: string;            // source text (string tokens: the decoded value)
  span: SourceSpan;
  intVal?: bigint;         // int / tick / colour
  frac?: FracInfo;         // frac / percent
  isKeyword?: boolean;
}

const PUNCT2 = ['::', '->', '..', '==', '!=', '<=', '>=', '&&', '||', '<<', '>>'];
const PUNCT1 = new Set([
  '{', '}', '(', ')', '[', ']', ',', ';', ':', '.', '=',
  '<', '>', '+', '-', '*', '/', '!', '~', '&', '|', '^', '%', '@',
]);

const ID_START = new Uint8Array(128);
const ID_CONT = new Uint8Array(128);
for (let c = 0; c < 128; c++) {
  const ch = String.fromCharCode(c);
  if (/[A-Za-z_]/.test(ch)) ID_START[c] = 1;
  if (/[A-Za-z0-9_]/.test(ch)) ID_CONT[c] = 1;
}

function isDigit(b: number): boolean { return b >= 0x30 && b <= 0x39; }
function isHexDigit(b: number): boolean {
  return isDigit(b) || (b >= 0x61 && b <= 0x66) || (b >= 0x41 && b <= 0x46);
}
function isLowerAlpha(b: number): boolean { return b >= 0x61 && b <= 0x7a; }

const utf8Decoder = new TextDecoder('utf-8', { fatal: true });

/** Validate UTF-8; returns the offset of the first invalid byte, or -1. */
function firstInvalidUtf8(bytes: Uint8Array): number {
  try {
    utf8Decoder.decode(bytes);
    return -1;
  } catch {
    // Binary search the longest valid prefix.
    let lo = 0;
    let hi = bytes.length;
    while (lo < hi) {
      const mid = (lo + hi + 1) >>> 1;
      try {
        utf8Decoder.decode(bytes.subarray(0, mid));
        lo = mid;
      } catch {
        hi = mid - 1;
      }
    }
    return Math.min(lo, bytes.length - 1);
  }
}

export function tokenize(bytes: Uint8Array, file: string, sink: DiagnosticSink): Token[] {
  const tokens: Token[] = [];
  const n = bytes.length;
  let i = 0;

  const bad = (code: string, s: SourceSpan, message: string): Token => {
    sink.error(code, s, message);
    const text = utf8Decoder.decode(bytes.subarray(s.start, Math.min(s.end, s.start + 16)));
    return { kind: 'bad', text, span: s };
  };

  const invalid = firstInvalidUtf8(bytes);
  if (invalid >= 0) {
    sink.error('FORM-E-001', span(file, invalid, invalid + 1),
      `source file is not valid UTF-8 (byte 0x${bytes[invalid]!.toString(16)} at offset ${invalid})`);
    return [{ kind: 'eof', text: '<eof>', span: span(file, n, n) }];
  }

  while (i < n) {
    const b = bytes[i]!;

    // whitespace (CRLF-tolerant: both bytes skipped, counted in offsets)
    if (b === 0x20 || b === 0x09 || b === 0x0d || b === 0x0a) { i++; continue; }

    // comments
    if (b === 0x2f /* / */) {
      const nxt = bytes[i + 1];
      if (nxt === 0x2f) { // //
        while (i < n && bytes[i] !== 0x0a) i++;
        continue;
      }
      if (nxt === 0x2a) { // /* — nests
        const open = span(file, i, i + 2);
        let depth = 1;
        i += 2;
        while (i < n && depth > 0) {
          if (bytes[i] === 0x2f && bytes[i + 1] === 0x2a) { depth++; i += 2; }
          else if (bytes[i] === 0x2a && bytes[i + 1] === 0x2f) { depth--; i += 2; }
          else i++;
        }
        if (depth > 0) {
          sink.error('FORM-E-002', open, 'unterminated block comment');
          tokens.push({ kind: 'eof', text: '<eof>', span: span(file, n, n) });
          return tokens;
        }
        continue;
      }
    }

    // identifiers / keywords
    if (b < 128 && ID_START[b]) {
      const start = i;
      while (i < n && bytes[i]! < 128 && ID_CONT[bytes[i]!]) i++;
      const text = utf8Decoder.decode(bytes.subarray(start, i));
      if (text.length > 64) {
        sink.error('FORM-E-009', span(file, start, i),
          `identifier exceeds the 64-byte limit (${text.length} bytes)`);
      }
      if (KEYWORDS.has(text)) {
        tokens.push({ kind: 'kw', text, span: span(file, start, i), isKeyword: true });
      } else {
        tokens.push({ kind: 'ident', text, span: span(file, start, i) });
      }
      continue;
    }

    // numbers
    if (isDigit(b)) {
      const start = i;
      let hex = false;
      if (b === 0x30 /* 0 */ && bytes[i + 1] === 0x78 /* x */) {
        hex = true;
        i += 2;
        const dstart = i;
        while (i < n && isHexDigit(bytes[i]!)) i++;
        if (i === dstart) {
          tokens.push(bad('FORM-E-006', span(file, start, i), 'malformed integer literal: 0x without hex digits'));
          continue;
        }
      } else if (b === 0x30 && bytes[i + 1] === 0x58 /* 0X */) {
        tokens.push(bad('FORM-E-006', span(file, start, i + 2),
          "malformed integer literal: '0X' is refused (use lowercase 0x)"));
        i += 2;
        continue;
      } else {
        while (i < n && isDigit(bytes[i]!)) i++;
        // leading-zero octal refused: "0" alone is fine, "0123" is not
        const decText = utf8Decoder.decode(bytes.subarray(start, i));
        if (decText.length > 1 && decText.startsWith('0')) {
          tokens.push(bad('FORM-E-006', span(file, start, i),
            `malformed integer literal: leading-zero octal '${decText}' is not allowed`));
          continue;
        }
      }

      // fractional part? ('..' is a range — stop before it)
      let fracDigits = '';
      const sawDot = !hex && bytes[i] === 0x2e /* . */ && bytes[i + 1] !== 0x2e;
      if (sawDot) {
        const dotAt = i;
        i++;
        const fstart = i;
        while (i < n && isDigit(bytes[i]!)) i++;
        fracDigits = utf8Decoder.decode(bytes.subarray(fstart, i));
        if (fracDigits === '') {
          // `1.` — floating-style literal, refused under the numeric policy
          tokens.push(bad('FORM-E-711', span(file, start, i),
            'floating-point literal refused (no digits after the decimal point; the numeric policy is qformats-only)'));
          continue;
        }
        void dotAt;
      }

      // scientific notation check (before unit suffixes — §1.2/§8)
      if ((bytes[i] === 0x65 /* e */ || bytes[i] === 0x45 /* E */) && !hex) {
        let j = i + 1;
        if (bytes[j] === 0x2b || bytes[j] === 0x2d) j++;
        if (isDigit(bytes[j]!)) {
          while (j < n && isDigit(bytes[j]!)) j++;
          i = j;
          tokens.push(bad('FORM-E-711', span(file, start, i),
            'scientific notation is a floating-point literal and is refused (numeric policy, FORM §5)'));
          continue;
        }
      }

      let suffix = '';
      const atIdentChar = (off: number): boolean => bytes[off] !== undefined && bytes[off]! < 128 && ID_CONT[bytes[off]!] === 1;
      if (!hex && !sawDot) {
        // tick literal: 120t (suffix must not run into an identifier)
        if (bytes[i] === 0x74 /* t */ && !atIdentChar(i + 1)) {
          i++;
          const text = utf8Decoder.decode(bytes.subarray(start, i));
          tokens.push({ kind: 'tick', text, span: span(file, start, i), intVal: BigInt(text.slice(0, -1)) });
          continue;
        }
        // percent: 45%
        if (bytes[i] === 0x25 /* % */) {
          i++;
          const text = utf8Decoder.decode(bytes.subarray(start, i));
          tokens.push({ kind: 'frac', text, span: span(file, start, i), frac: { intDigits: text.slice(0, -1), fracDigits: '', suffix: '%' } });
          continue;
        }
      }
      if (!hex) {
        // unit suffixes: m, px, w, turn, deg (boundary must not run into an ident)
        const restText = utf8Decoder.decode(bytes.subarray(i, Math.min(i + 5, n)));
        for (const sfx of ['turn', 'deg', 'px']) {
          if (restText.startsWith(sfx) && !atIdentChar(i + sfx.length)) { suffix = sfx; i += sfx.length; break; }
        }
        if (!suffix && (bytes[i] === 0x6d /* m */ || bytes[i] === 0x77 /* w */) && !atIdentChar(i + 1)) {
          suffix = String.fromCharCode(bytes[i]!);
          i++;
        }
      }

      const text = utf8Decoder.decode(bytes.subarray(start, i));
      if (sawDot || suffix !== '') {
        // intDigits = digits before the '.' / '%' / suffix
        const intDigits = text.split(/[.%]/)[0]!.replace(/[a-z]+$/, '');
        tokens.push({
          kind: 'frac', text, span: span(file, start, i),
          frac: { intDigits, fracDigits, suffix },
        });
      } else {
        tokens.push({
          kind: 'int', text, span: span(file, start, i),
          intVal: BigInt(hex ? '0x' + text.slice(2) : text),
        });
      }
      continue;
    }

    // colour literal
    if (b === 0x23 /* # */) {
      const start = i;
      i++;
      const dstart = i;
      while (i < n && isHexDigit(bytes[i]!)) i++;
      const digits = utf8Decoder.decode(bytes.subarray(dstart, i));
      if (digits.length === 6 || digits.length === 8) {
        tokens.push({ kind: 'colour', text: `#${digits}`, span: span(file, start, i), intVal: BigInt('0x' + digits) });
      } else {
        tokens.push(bad('FORM-E-006', span(file, start, i),
          `malformed colour literal: #${digits} (expected 6 or 8 hex digits)`));
      }
      continue;
    }

    // string literal
    if (b === 0x22 /* " */) {
      const start = i;
      i++;
      let value = '';
      let terminated = false;
      while (i < n) {
        const c = bytes[i]!;
        if (c === 0x22) { i++; terminated = true; break; }
        if (c === 0x0a) break; // strings do not span lines (§1 — one line)
        if (c === 0x5c /* \ */) {
          const e = bytes[i + 1];
          if (e === 0x5c || e === 0x22) { value += String.fromCharCode(e!); i += 2; continue; }
          if (e === undefined) { break; }
          sink.error('FORM-E-004', span(file, i, i + 2),
            `illegal escape in string: '\\${String.fromCharCode(e)}' (only \\\\ and \\" are allowed)`);
          i += 2; // recover: skip the escape
          continue;
        }
        // copy one UTF-8 scalar
        const seqLen = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
        value += utf8Decoder.decode(bytes.subarray(i, i + seqLen));
        i += seqLen;
      }
      if (!terminated) {
        sink.error('FORM-E-003', span(file, start, i), 'unterminated string literal');
      } else {
        const byteLen = i - start - 2;
        if (byteLen > 256) {
          sink.error('FORM-E-009', span(file, start, i),
            `string literal exceeds the 256-byte limit (${byteLen} bytes)`);
        }
      }
      tokens.push({ kind: 'string', text: value, span: span(file, start, i) });
      continue;
    }

    // punctuation (longest match first)
    if (b < 128) {
      const two = i + 1 < n ? String.fromCharCode(b, bytes[i + 1]!) : '';
      if (PUNCT2.includes(two)) {
        tokens.push({ kind: 'punct', text: two, span: span(file, i, i + 2) });
        i += 2;
        continue;
      }
      const one = String.fromCharCode(b);
      if (PUNCT1.has(one)) {
        tokens.push({ kind: 'punct', text: one, span: span(file, i, i + 1) });
        i++;
        continue;
      }
    }

    // nothing matches
    const ch = utf8Decoder.decode(bytes.subarray(i, i + (b < 0x80 ? 1 : b < 0xe0 ? 2 : b < 0xf0 ? 3 : 4)));
    tokens.push(bad('FORM-E-005', span(file, i, i + ch.length),
      `character '${ch}' (0x${b.toString(16)}) does not begin any token`));
    i += ch.length;
  }

  tokens.push({ kind: 'eof', text: '<eof>', span: span(file, n, n) });
  return tokens;
}
