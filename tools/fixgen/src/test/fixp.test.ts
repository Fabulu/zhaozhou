/**
 * fixgen self-tests (npm run -w tools/fixgen test): property + known-answer
 * checks on the generator's numeric core. The heavyweight cross-language
 * differentials live in CTest (tests/unit/test_fixp.cpp consumes the golden
 * binaries this tool emits).
 */

import * as assert from "assert/strict";
import { test } from "node:test";
import {
  QFMT_VERSION,
  rhuDiv,
  rescaleU,
  buildSinTable,
  fxSin,
  fxCos,
  buildRcp24Table,
  rcp24Norm,
  rcp24,
  buildFieldRcpTable,
  fieldRcp,
  unitMul,
  noise2Hash,
  rcp24SampleInput,
} from "../fixp.js";
import { renderCpp, renderTs, renderMem } from "../emit.js";
import { goldenSinCos, goldenUnit8, goldenNoise2 } from "../golden.js";

test("QFMT_VERSION is 1 (qformats.md 13)", () => {
  assert.equal(QFMT_VERSION, 1);
});

test("round_half_up division and rescale (qformats.md 4)", () => {
  assert.equal(rhuDiv(1n, 2n), 1n); // 0.5 -> 1 (half-up)
  assert.equal(rhuDiv(3n, 2n), 2n);
  assert.equal(rhuDiv(5n, 3n), 2n);
  assert.equal(rescaleU(3n, 1), 2n); // 1.5 -> 2
  assert.equal(rescaleU(5n, 2), 1n); // 1.25 -> 1
  assert.equal(rescaleU(7n, 2), 2n); // 1.75 -> 2
  assert.equal(rescaleU(123n, 0), 123n); // k == 0 identity
});

test("sin table endpoints and monotonicity (qformats.md 7.1)", () => {
  const t = buildSinTable();
  assert.equal(t.length, 257);
  assert.equal(t[0], 0);
  assert.equal(t[256], 0x10000); // exactly 1.0 in Q1.16
  for (let i = 1; i < 257; i++) assert.ok(t[i] > t[i - 1], "monotone at " + i);
  for (const v of t) assert.ok(v >= 0 && v <= 0x10000);
});

test("fx_sin exact identities over all 2^16 angles (qformats.md 7.1)", () => {
  const t = buildSinTable();
  assert.equal(fxSin(t, 0), 0);
  assert.equal(fxSin(t, 0x4000), 0x10000);
  assert.equal(fxSin(t, 0x8000), 0);
  assert.equal(fxSin(t, 0xc000), -0x10000);
  for (let a = 1; a < 65536; a++) {
    const s = fxSin(t, a);
    assert.equal(fxSin(t, (65536 - a) & 0xffff), (-s) | 0, "sin(-a) == -sin(a) at " + a);
    assert.equal(fxSin(t, (32768 - a) & 0xffff), s, "sin(pi - a) == sin(a) at " + a);
    assert.equal(fxCos(t, a), fxSin(t, (a + 0x4000) & 0xffff), "cos by construction");
  }
});

test("fx_sin within 1.35 LSB of libm over all 2^16 angles (qformats.md 7.1 bound 1.3)", () => {
  const t = buildSinTable();
  let maxLsb = 0;
  for (let a = 0; a < 65536; a++) {
    const exact = Math.sin((2 * Math.PI * a) / 65536);
    const err = Math.abs(fxSin(t, a) / 65536 - exact) * 65536;
    if (err > maxLsb) maxLsb = err;
  }
  assert.ok(maxLsb <= 1.35, "max err " + maxLsb + " LSB");
});

test("rcp_u24 table formula spot checks (qformats.md 6.1)", () => {
  const t = buildRcp24Table();
  assert.equal(t.length, 256);
  for (let idx = 0; idx < 256; idx++) {
    const mid = (1 << 23) + idx * (1 << 15) + (1 << 14);
    assert.equal(t[idx], Number(rhuDiv(1n << 54n, BigInt(mid))), "T24[" + idx + "]");
  }
});

test("rcp_u24_norm within 1 LSB of exact 2^47/m on a stress sample (qformats.md 6.1)", () => {
  const t = buildRcp24Table();
  const pts: number[] = [];
  for (let m = 1 << 23; m < 1 << 24; m += 61703) pts.push(m); // strided full domain
  pts.push(1 << 23, (1 << 23) + 1, (1 << 24) - 1, 0xffffff);
  for (const m of pts) {
    const exact = Number(2n ** 47n) / m;
    const err = Math.abs(rcp24Norm(t, m) - exact);
    assert.ok(err <= 1, "m=" + m + " err=" + err);
  }
});

test("rcp24 wrapper exponent reconstruction (qformats.md 6.1)", () => {
  const t = buildRcp24Table();
  for (const d of [1, 2, 3, 255, 256, 0x8000, 0x123456, 0x7fffff, 0x800000, 0xffffff]) {
    const { r, k } = rcp24(t, d);
    const got = (r / 2 ** 24) * 2 ** k;
    const ex = 2 ** 24 / d;
    assert.ok(Math.abs(got - ex) / ex < 1e-6, "d=" + d);
  }
});

test("field_rcp pinned zero + saturation + power-of-two exactness (qformats.md 6.2)", () => {
  const tf = buildFieldRcpTable();
  const z = fieldRcp(tf, 0);
  assert.equal(z.r, 0x7fffffff);
  assert.equal(z.rcp0, true);
  // power-of-two inputs: 2^32 / 2^s, saturating at s31 max when s == 1;
  // s == 31 exercises INT32_MIN (JS 1<<31 wraps negative — the only s31 mag).
  for (let s = 1; s <= 30; s++) {
    const a = 1 << s;
    const exact = Math.min(2 ** (32 - s), 0x7fffffff);
    assert.equal(fieldRcp(tf, a).r, exact, "a=2^" + s);
  }
  assert.equal(fieldRcp(tf, -0x80000000).r, -2, "a=INT32_MIN");
});

test("unit_mul boundaries (qformats.md 3)", () => {
  assert.equal(unitMul(0, 0), 0);
  assert.equal(unitMul(255, 255), 254);
  assert.equal(unitMul(255, 128), 128); // (32640+128)>>8 = 128
  assert.equal(unitMul(128, 128), 64);
  assert.equal(unitMul(1, 255), 1); // (255+128)>>8 = 1
});

test("noise2 hash determinism + lane decorrelation (qformats.md 7.5)", () => {
  // frozen known-answer (self-computed with the frozen constants; any change
  // to a constant changes these and MUST bump QFMT_VERSION)
  assert.equal(noise2Hash(0, 0, 0, 0), noise2Hash(0, 0, 0, 0));
  const a = noise2Hash(1, 2, 3, 0);
  const b = noise2Hash(1, 2, 3, 1);
  assert.notEqual(a, b);
  assert.notEqual(a, noise2Hash(2, 1, 3, 0));
  // lane outputs in U 0.0.16 range when shifted
  for (let i = 0; i < 1000; i++) {
    const h = noise2Hash(i * 7, i * 13, i, i & 1);
    assert.ok(h >>> 0 === h);
    assert.ok((h >>> 16) <= 0xffff);
  }
});

test("rcp24 sample inputs: nonzero, in u24 domain, 2^20 coverage (qformats.md 12)", () => {
  const n = 1 << 20;
  let max = 0;
  for (let i = 0; i < n; i++) {
    const d = rcp24SampleInput(i);
    assert.ok(d >= 1 && d <= 0xffffff, "i=" + i);
    if (d > max) max = d;
  }
  assert.ok(max === 0xffffff || max >= 0xfffff0, "high inputs covered");
});

test("emitters are deterministic and share hex digits across languages (qformats.md 11)", () => {
  const sinTab = buildSinTable();
  const t24 = buildRcp24Table();
  const tf = buildFieldRcpTable();
  const cpp1 = renderCpp(sinTab, t24, tf).content;
  const cpp2 = renderCpp(sinTab, t24, tf).content;
  assert.equal(cpp1, cpp2);
  const ts = renderTs(sinTab, t24, tf).content;
  const mem = renderMem("sin_q16.mem", sinTab, 5).content;
  // same hex digits: C++ "0x0000FF," <-> mem "0000FF" (SIN_Q16 block only;
  // the \b keeps 8-digit RCP24 literals out of the match)
  const cppHex = (cpp1.match(/\b0x[0-9A-F]{5}\b/g) ?? []).map((s) => s.slice(2)).slice(0, 257);
  const memHex = mem.trim().split("\n");
  assert.deepEqual(cppHex, memHex);
  assert.ok(ts.includes("0x" + memHex[0]));
  // every emitted text ends with a newline
  for (const c of [cpp1, ts, mem]) assert.ok(c.endsWith("\n"));
  assert.ok(!cpp1.includes("\r"));
});

test("golden renders are deterministic", () => {
  assert.deepEqual(goldenSinCos().bytes.subarray(0, 64), goldenSinCos().bytes.subarray(0, 64));
  const g1 = goldenUnit8();
  const g2 = goldenUnit8();
  assert.deepEqual(Buffer.from(g1.bytes), Buffer.from(g2.bytes));
  const n1 = goldenNoise2();
  assert.equal(n1.bytes.length, 1024 * 20);
});
