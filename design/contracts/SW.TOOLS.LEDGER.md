# Contract — SW.TOOLS.LEDGER (Design ledger tooling)

> Ledger: `design/blocks.yml` · owner ZH-001 · phase 1 · maturity UNIT_VERIFIED (W6; evidence pinned in the ledger)

## Purpose and exclusions

This tool: schema + V1–V14 validation of blocks.yml/ops.yml, git-history maturity law, and deterministic architecture/dashboard/contract generators.

Exclusions: no report parsing (SW.TOOLS.REPORT), no board data (SW.TOOLS.BOARDPROBE); the ledger never encodes resource_actual — that arrives only from Quartus report JSON (V5 activates at SYNTHESIZED).

## Input and output packet layouts

In: `design/blocks.yml`, `design/ops.yml` (JSON-Schema-validated against design/schema/*.json), the git history of blocks.yml (rule V2 reads HEAD). Out: `design/diagrams/architecture.mmd`, `design/diagrams/dashboard.md`, contract scaffolds. All outputs are deterministic and timestamp-free: `ledger:gen` must reproduce the committed files byte-identically (staleness = CI failure, plan R11).

## Backpressure rules

Backpressure: `none`.

## Memory ownership

Pure functions over parsed documents (rule disk access is injected); no persistent state.

## Q formats and rounding

N/A (no numeric formats beyond budget percentages, which are pass-through data it never invents).

## Latency (fixed or variable)

Latency: `batch`.

## Target throughput

Target throughput: n/a (tool).

## Overflow and malformed-input behaviour

Every deviation is a named rule violation (V1–V14, A3e) with the offending block/op id — never a crash, never a silent pass. The git-history comparison bootstraps cleanly when blocks.yml first appears (plan R5). blocked_on:hardware blocks cannot advance past SPECIFIED regardless of evidence (plan §4 rule).

## Directed tests

`tools/ledger/src/test/rules.test.ts` (10/10 via `npm run -w tools/ledger test`): schema + relational law incl. "validator rejects a deliberately advanced maturity without evidence" (the W2 acceptance negative test).

## Randomized differential tests

N/A — the tool is deterministic; its "differential" is the byte-identity staleness gate (`npm run ledger:check`, also a CTest `fast` shim `ledger_check`).

## Integration capture cases

This repository's own ledger: `npm run ledger:check` green over the live git history is the integration case (the W6 maturity advancements themselves exercised V2 one-step-at-a-time and V3 evidence pinning).

## Notes

Contract filled (Phase-1-active). `npm run ledger:check` / `ledger:gen` — staleness is CI failure (plan R11).
