// emit_fuzz.ts — tests/fuzz/abi_corpus_gen.ts emitter: a self-contained,
// dependency-free TS module carrying the fuzz corpus as literal data plus a
// regeneration entry point (P5 6.5, capture_format.md 6). Expected error
// codes were computed by the oracle validator at generation time; C++, TS
// and SV validators must reproduce them exactly (tri-language parity suite).

import { CorpusCase } from './fuzz.js';

function bytesLiteral(bytes: Uint8Array, perLine = 16): string {
  const lines: string[] = [];
  for (let i = 0; i < bytes.length; i += perLine) {
    const chunk = Array.from(bytes.slice(i, i + perLine), (b) => `0x${b.toString(16).padStart(2, '0')}`);
    lines.push(`  ${chunk.join(', ')}${i + perLine < bytes.length ? ',' : ''}`);
  }
  return lines.join('\n');
}

export function emitFuzz(cases: readonly CorpusCase[]): string {
  const L: string[] = [];
  L.push('// GENERATED FILE - DO NOT EDIT');
  L.push('// Source: spec/commands.zidl via tools/abi-gen (`npm run abi:gen`).');
  L.push('// Fuzz corpus: valid + malformed frame packets with oracle-expected');
  L.push('// zhao_abi_error codes (spec/capture_format.md 3.2 order). Consumers:');
  L.push('//   C++  tests/fuzz/test_abi_fuzz_parity.cpp (reads abi_corpus.zcorpus)');
  L.push('//   TS   compiler tests (reads abi_corpus.zcorpus)');
  L.push('//   SV   zhao_abi_probe fv-mode (driven by the C++ parity test)');
  L.push('// This module is also the human-readable form of the corpus.');
  L.push('');
  L.push('export interface AbiCorpusCase {');
  L.push('  readonly name: string;');
  L.push('  readonly packet: readonly number[];');
  L.push('  readonly expectedError: number;');
  L.push('}');
  L.push('');
  L.push('export const ABI_CORPUS: readonly AbiCorpusCase[] = [');
  for (const c of cases) {
    L.push('  {');
    L.push(`    name: '${c.name}',`);
    L.push(`    expectedError: ${c.expectedError},`);
    L.push('    packet: [');
    L.push(bytesLiteral(c.packet));
    L.push('    ],');
    L.push('  },');
  }
  L.push('];');
  L.push('');
  L.push('/** Serialize to the .zcorpus binary (must equal tests/abi/golden/abi_corpus.zcorpus). */');
  L.push('export function corpusBinary(): Uint8Array {');
  L.push('  let size = 12;');
  L.push('  for (const c of ABI_CORPUS) size += 2 + c.name.length + 4 + c.packet.length + 4;');
  L.push('  const out = new Uint8Array(size);');
  L.push('  const dv = new DataView(out.buffer);');
  L.push("  dv.setUint32(0, 0x524f435a, true); // 'ZCOR'");
  L.push('  dv.setUint16(4, 1, true);');
  L.push('  dv.setUint16(6, 0, true);');
  L.push('  dv.setUint32(8, ABI_CORPUS.length, true);');
  L.push('  let off = 12;');
  L.push('  const enc = new TextEncoder();');
  L.push('  for (const c of ABI_CORPUS) {');
  L.push('    const name = enc.encode(c.name);');
  L.push('    dv.setUint16(off, name.length, true); off += 2;');
  L.push('    out.set(name, off); off += name.length;');
  L.push('    dv.setUint32(off, c.packet.length, true); off += 4;');
  L.push('    out.set(c.packet, off); off += c.packet.length;');
  L.push('    dv.setUint32(off, c.expectedError, true); off += 4;');
  L.push('  }');
  L.push('  return out;');
  L.push('}');
  L.push('');
  return L.join('\n');
}
