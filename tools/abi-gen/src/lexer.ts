// lexer.ts — hand-written lexer for .zidl (zero deps; capture_format.md 1).
// Lexical rules: C-style comments (// and /* */), identifiers, decimal/0x hex
// integers, punctuation { } [ ] : ; = ,. Line/column tracked for diagnostics.

import { SourcePos, ZidlError } from './types.js';

export type TokenKind =
  | 'ident'
  | 'int'
  | 'lbrace' // {
  | 'rbrace' // }
  | 'lbracket' // [
  | 'rbracket' // ]
  | 'colon' // :
  | 'semicolon' // ;
  | 'equals' // =
  | 'eof';

export interface Token {
  readonly kind: TokenKind;
  readonly text: string;
  readonly value: number; // for int
  readonly pos: SourcePos;
}

const IDENT_START = /[A-Za-z_]/;
const IDENT_CONT = /[A-Za-z0-9_]/;

export function tokenize(source: string): Token[] {
  const tokens: Token[] = [];
  let line = 1;
  let col = 1;
  let i = 0;

  const push = (kind: TokenKind, text: string, value = 0) => {
    tokens.push({ kind, text, value, pos: { line, col } });
  };

  while (i < source.length) {
    const ch = source[i]!;

    // whitespace
    if (ch === '\n') {
      i++; line++; col = 1; continue;
    }
    if (ch === ' ' || ch === '\t' || ch === '\r') {
      i++; col++; continue;
    }

    // comments
    if (ch === '/' && source[i + 1] === '/') {
      while (i < source.length && source[i] !== '\n') { i++; col++; }
      continue;
    }
    if (ch === '/' && source[i + 1] === '*') {
      const startLine = line, startCol = col;
      i += 2; col += 2;
      while (i < source.length && !(source[i] === '*' && source[i + 1] === '/')) {
        if (source[i] === '\n') { line++; col = 1; } else { col++; }
        i++;
      }
      if (i >= source.length) {
        throw new ZidlError('unterminated block comment', { line: startLine, col: startCol });
      }
      i += 2; col += 2;
      continue;
    }

    // integers: decimal or 0x hex
    if (/[0-9]/.test(ch)) {
      const start = i;
      let value: number;
      if (ch === '0' && (source[i + 1] === 'x' || source[i + 1] === 'X')) {
        i += 2;
        while (i < source.length && /[0-9A-Fa-f]/.test(source[i]!)) i++;
        const text = source.slice(start, i);
        if (text.length <= 2) {
          throw new ZidlError('malformed hex literal', { line, col });
        }
        value = Number.parseInt(text, 16);
      } else {
        while (i < source.length && /[0-9]/.test(source[i]!)) i++;
        value = Number.parseInt(source.slice(start, i), 10);
      }
      if (!Number.isSafeInteger(value) || value < 0) {
        throw new ZidlError('integer literal out of range', { line, col });
      }
      push('int', source.slice(start, i), value);
      col += i - start;
      continue;
    }

    // identifiers / keywords
    if (IDENT_START.test(ch)) {
      const start = i;
      while (i < source.length && IDENT_CONT.test(source[i]!)) i++;
      push('ident', source.slice(start, i));
      col += i - start;
      continue;
    }

    // punctuation
    const punct: Record<string, TokenKind> = {
      '{': 'lbrace', '}': 'rbrace', '[': 'lbracket', ']': 'rbracket',
      ':': 'colon', ';': 'semicolon', '=': 'equals',
    };
    const kind = punct[ch];
    if (kind) {
      push(kind, ch);
      i++; col++;
      continue;
    }

    throw new ZidlError(`unexpected character '${ch}'`, { line, col });
  }

  tokens.push({ kind: 'eof', text: '<eof>', value: 0, pos: { line, col } });
  return tokens;
}
