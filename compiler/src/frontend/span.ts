// span.ts — source spans for the Form frontend (W3.2, plan D2).
//
// Law: spec/form/language-semantics.md §2 — every AST node carries a
// SourceSpan of BYTE OFFSETS {file, start, end} measured from file start;
// `\r\n` counts as two bytes and neither byte is part of any token
// (language-semantics §1.1). This is the frontend SourceSpan; lowering into
// the Field IR builder's {sourceId, line, col} span (field-ir/types.ts) is a
// separate step (`toFieldIrSpan`) owned by W3.3/W3.4 source-ID assignment.

/** Byte-offset span into one source file. `start` is inclusive, `end` exclusive. */
export interface SourceSpan {
  readonly file: string;
  readonly start: number;
  readonly end: number;
}

export function span(file: string, start: number, end: number): SourceSpan {
  return { file, start, end };
}

/** Smallest span covering both inputs (same file). */
export function joinSpans(a: SourceSpan, b: SourceSpan): SourceSpan {
  return { file: a.file, start: Math.min(a.start, b.start), end: Math.max(a.end, b.end) };
}

/**
 * Line/col (1-based) for a byte offset — the bridge to the Field IR span
 * shape (field-ir/types.ts SourceSpan). `lines` is the ascending list of
 * byte offsets at which each line begins (line 1 starts at 0).
 */
export function lineColAt(lines: readonly number[], offset: number): { line: number; col: number } {
  // Binary search for the greatest line start <= offset.
  let lo = 0;
  let hi = lines.length - 1;
  while (lo < hi) {
    const mid = (lo + hi + 1) >>> 1;
    if (lines[mid]! <= offset) lo = mid;
    else hi = mid - 1;
  }
  return { line: lo + 1, col: offset - lines[lo]! + 1 };
}

/** Ascending line-start byte offsets for a UTF-8 source buffer. */
export function lineStarts(bytes: Uint8Array): number[] {
  const starts = [0];
  for (let i = 0; i < bytes.length; i++) {
    if (bytes[i] === 0x0a) starts.push(i + 1); // LF (CRLF: line starts after the LF)
  }
  return starts;
}
