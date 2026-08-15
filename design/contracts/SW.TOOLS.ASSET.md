# Contract — SW.TOOLS.ASSET (Asset pipeline)

> Ledger: `design/blocks.yml` · owner ZH-079 · phase 3 · maturity SPECIFIED

## Purpose and exclusions

Meshlet/LOD/microform/texture packers producing streamable assets.

Wave-3 scope (plan W3.6, the packer subset — plan D8): `tools/pack` (TS,
zero deps) — module set → SW.COMPILER.FORM build → deterministic `.zpak`
cartridge per spec/cartridge.md, with the `pack:check` staleness gate.
Excluded (L4 lane, later waves): meshlet/LOD/microform packing, procedural
texture baking, scar-response tables, glTF ingest, compression formats
beyond the page shapes spec/cartridge.md §4 defines.

## Input and output packet layouts

Inputs: the Form source set, the compiler's output tree (generated C++,
`.zprog` programs, `sourceids.zmap`, `costs.zcost`), and authored resource
pages (terrain patches, sky set, tone bank — shapes cartridge §4). A pack
manifest (canonical JSON, cost-model §2 canonicalization law) names the
page table and page-id constants.

Output: one `.zpak` (cartridge §1-§3): magic ZPAK, ABI_INFO first
(`zhaozhou-pack` generator identity, .zidl + manifest SHA-256 pins),
fixed section order, RESOURCE_PAGES last, exact `total_file_length`, per-
section CRC-32C + page SHA-256. Two packs of one build are byte-identical.

## Backpressure rules

Backpressure: `none`. Batch tool.

## Memory ownership

Tool process owns everything; the output file is written
create→bodies→table→backpatch (capture_format §4.4 writer discipline) and
is never partially visible (staging path + atomic rename in `pack:check`
verification mode).

## Q formats and rounding

The packer re-serializes, never computes: numeric fields are copied from
compiler/authored inputs verbatim. The one rounding site it owns is the
terrain-patch bake-back (height16, qformats §9) which happens at authoring
time, not pack time. CRC-32C/SHA-256 are exact by construction.

## Latency (fixed or variable)

Latency: `batch` (wall time irrelevant; determinism is the contract).

## Target throughput

Target throughput: n/a (tool).

## Overflow and malformed-input behaviour

A pack whose inputs violate cartridge §3 (unknown page kind, duplicate
page_id, unresolved Form page-id constant — FORM-E-830/831 — odd/empty
terrain patch, tone event id missing from a referenced bank) fails with a
deterministic diagnostic naming the offending page/section; no partial
`.zpak` is written. `pack:check` regenerates in memory and diffs against
the committed cartridge — drift fails CI (same law as abi:check).

## Directed tests

`tools/pack/test/` + `tests/e2e/` (W3.6, labels fast): pack determinism
(run twice, byte-compare); page-table cross-checks (every Form page-id
const resolves, kinds match); corrupt-input refusals; pack→ZEmu load
round-trip byte-stable; `pack:check` staleness gate green.

## Randomized differential tests

Randomized manifest shuffles: same logical pages in randomized input order
must still pack to the identical `.zpak` (the fixed section order is the
only legal order) — fuzz over page permutations, assert byte-identity.

## Integration capture cases

W3.7: `wound_lab.zpak` is the committed golden cartridge; the e2e test
loads it and runs the 600-tick golden chain (SW.ZEMU contract integration
cases).

## Notes

Microform crossfade sophistication is §26 cut item 8 (unchanged). Maturity
target REFERENCE_COMPLETE at wave-3 gate for the packer subset (plan D12);
the L4 asset lanes stay SPECIFIED until their waves.
