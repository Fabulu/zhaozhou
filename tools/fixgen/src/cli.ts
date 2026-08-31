/**
 * fixgen CLI (qformats.md 11): `gen` writes every output; `check` regenerates
 * in memory and byte-compares against the committed files (drift = exit 1).
 * Repo root is resolved from this module's location so the tool works from
 * any cwd (npm workspace, CTest shim, CI).
 */

import * as fs from "fs";
import * as path from "path";
import { buildSinTable, buildRcp24Table, buildFieldRcpTable } from "./fixp.js";
import { renderCpp, renderTs, renderMem, renderDepthCpp, renderDepthTs, RenderedFile } from "./emit.js";
import { buildDepthProfiles } from "./depth.js";
import { goldenSinCos, goldenUnit8, goldenRcp24, goldenNoise2, renderManifest } from "./golden.js";

function repoRoot(): string {
  // dist/cli.js -> tools/fixgen/dist -> tools/fixgen -> repo root
  return path.resolve(__dirname, "..", "..", "..");
}

interface Artifact {
  path: string;
  bytes: Buffer;
}

function buildAll(): Artifact[] {
  const sinTab = buildSinTable();
  const t24 = buildRcp24Table();
  const tf = buildFieldRcpTable();
  // Derived through the SAME rcp_u24 the tables above define, so a change to
  // the reciprocal moves the profiles too instead of leaving them stale.
  // buildDepthProfiles throws if wmin stops pinning to 0xFFFFFF exactly or the
  // far floor reaches zero -- a generator that emits a table failing its own
  // law is worse than no generator.
  const depth = buildDepthProfiles(t24);

  const texts: RenderedFile[] = [
    renderCpp(sinTab, t24, tf),
    renderTs(sinTab, t24, tf),
    renderDepthCpp(depth),
    renderDepthTs(depth),
    renderMem("sin_q16.mem", sinTab, 5),
    renderMem("rcp24_t0.mem", t24, 8),
    renderMem("field_rcp_t0.mem", tf, 5),
  ];

  const goldens = [goldenSinCos(), goldenUnit8(), goldenRcp24(), goldenNoise2()];
  texts.push(renderManifest(goldens));

  const out: Artifact[] = texts.map((t) => ({ path: t.path, bytes: Buffer.from(t.content, "utf8") }));
  for (const g of goldens) out.push({ path: g.path, bytes: g.bytes });
  out.sort((a, b) => a.path.localeCompare(b.path));
  return out;
}

function cmdGen(): number {
  const root = repoRoot();
  for (const a of buildAll()) {
    const full = path.join(root, a.path);
    fs.mkdirSync(path.dirname(full), { recursive: true });
    fs.writeFileSync(full, a.bytes);
    console.log("wrote " + a.path + " (" + a.bytes.length + " bytes)");
  }
  return 0;
}

function cmdCheck(): number {
  const root = repoRoot();
  const all = buildAll();
  let bad = 0;
  for (const a of all) {
    const full = path.join(root, a.path);
    if (!fs.existsSync(full)) {
      console.error("MISSING: " + a.path);
      bad++;
      continue;
    }
    const disk = fs.readFileSync(full);
    if (!disk.equals(a.bytes)) {
      console.error("STALE:   " + a.path + " (disk " + disk.length + " B, expected " + a.bytes.length + " B)");
      bad++;
    }
  }
  const checked = all.length;
  if (bad > 0) {
    console.error("fixgen check: " + bad + " of " + checked + " generated files out of date — run `npm run tables:gen` and commit");
    return 1;
  }
  console.log("fixgen check: all " + checked + " generated files byte-identical (QFMT_VERSION constant from src/fixp.ts)");
  return 0;
}

const cmd = process.argv[2];
if (cmd === "gen") process.exit(cmdGen());
else if (cmd === "check") process.exit(cmdCheck());
else {
  console.error("usage: fixgen <gen|check>");
  process.exit(2);
}
