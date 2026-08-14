/**
 * Text emitters: render the generated table sources for C++ / SV / TS.
 * Determinism rules (qformats.md 11): no timestamps, no host paths, LF line
 * endings, fixed hex widths, trailing newline, and IDENTICAL hex digit
 * strings across all three languages (asserted by tests/unit/test_tables_tri.cpp
 * and `npm run tables:check`).
 */

import { QFMT_VERSION } from "./fixp.js";

export interface RenderedFile {
  path: string; // repo-relative, forward slashes
  content: string; // exact bytes (LF, trailing newline)
}

const HEX = (v: number, w: number) => v.toString(16).toUpperCase().padStart(w, "0");

function rows(values: number[], hexWidth: number, perLine: number, indent: string): string {
  const lines: string[] = [];
  for (let i = 0; i < values.length; i += perLine) {
    lines.push(indent + values.slice(i, i + perLine).map((v) => "0x" + HEX(v, hexWidth)).join(", "));
  }
  return lines.join(",\n") + ",\n";
}

/** reference/include/zref/generated/zref_tables.hpp (constexpr, qformats.md 11). */
export function renderCpp(sinTab: number[], t24: number[], tf: number[]): RenderedFile {
  let s = "";
  s += "// GENERATED FILE - tools/fixgen (spec/qformats.md 11) - DO NOT EDIT.\n";
  s += "// QFMT_VERSION " + QFMT_VERSION + "; regenerate with `npm run tables:gen` and commit.\n";
  s += "#pragma once\n";
  s += "#include <cstdint>\n";
  s += "\n";
  s += "namespace zref {\n";
  s += "namespace gen {\n";
  s += "\n";
  s += "inline constexpr uint32_t QFMT_VERSION = " + QFMT_VERSION + ";\n";
  s += "\n";
  s += "// qformats.md 7.1: sin/cos quarter-wave, 257 x Q1.16 (s18 values stored u32),\n";
  s += "// T[i] = round_half_up(sin(pi/2 * i / 256) * 2^16).\n";
  s += "inline constexpr uint32_t SIN_Q16[257] = {\n";
  s += rows(sinTab, 5, 8, "    ");
  s += "};\n";
  s += "\n";
  s += "// qformats.md 6.1: rcp_u24 initial guesses, 256 x Q0.31,\n";
  s += "// T24[idx] = round_half_up(2^54 / (2^23 + idx*2^15 + 2^14)).\n";
  s += "inline constexpr uint32_t RCP24_T0[256] = {\n";
  s += rows(t24, 8, 6, "    ");
  s += "};\n";
  s += "\n";
  s += "// qformats.md 6.2: field_rcp initial guesses, 256 x Q0.17,\n";
  s += "// TF[idx] = round_half_up(2^47 / (2^31 + idx*2^23 + 2^22)).\n";
  s += "inline constexpr uint32_t FIELD_RCP_T0[256] = {\n";
  s += rows(tf, 5, 8, "    ");
  s += "};\n";
  s += "\n";
  s += "}  // namespace gen\n";
  s += "}  // namespace zref\n";
  return { path: "reference/include/zref/generated/zref_tables.hpp", content: s };
}

/** SV $readmemh files (qformats.md 11): one hex word per line, no 0x, no comments. */
export function renderMem(name: string, values: number[], hexWidth: number): RenderedFile {
  return {
    path: "fpga/rtl/generated/tables/" + name,
    content: values.map((v) => HEX(v, hexWidth)).join("\n") + "\n",
  };
}

/** compiler/src/generated/tables.ts (qformats.md 11). */
export function renderTs(sinTab: number[], t24: number[], tf: number[]): RenderedFile {
  let s = "";
  s += "// GENERATED FILE - tools/fixgen (spec/qformats.md 11) - DO NOT EDIT.\n";
  s += "// QFMT_VERSION " + QFMT_VERSION + "; regenerate with `npm run tables:gen` and commit.\n";
  s += "\n";
  s += "export const QFMT_VERSION = " + QFMT_VERSION + ";\n";
  s += "\n";
  s += "/** qformats.md 7.1: sin/cos quarter-wave, 257 x Q1.16 (s18 values),\n";
  s += " * T[i] = round_half_up(sin(pi/2 * i / 256) * 2^16). */\n";
  s += "export const SIN_Q16: readonly number[] = [\n";
  s += rows(sinTab, 5, 8, "    ");
  s += "];\n";
  s += "\n";
  s += "/** qformats.md 6.1: rcp_u24 initial guesses, 256 x Q0.31,\n";
  s += " * T24[idx] = round_half_up(2^54 / (2^23 + idx*2^15 + 2^14)). */\n";
  s += "export const RCP24_T0: readonly number[] = [\n";
  s += rows(t24, 8, 6, "    ");
  s += "];\n";
  s += "\n";
  s += "/** qformats.md 6.2: field_rcp initial guesses, 256 x Q0.17,\n";
  s += " * TF[idx] = round_half_up(2^47 / (2^31 + idx*2^23 + 2^22)). */\n";
  s += "export const FIELD_RCP_T0: readonly number[] = [\n";
  s += rows(tf, 5, 8, "    ");
  s += "];\n";
  s += "\n";
  return { path: "compiler/src/generated/tables.ts", content: s };
}
