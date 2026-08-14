# Contract — SW.CPUCOLL (CPU-side collision and canonical scars)

> Ledger: `design/blocks.yml` · owner ZH-036 · phase 7 · maturity SPECIFIED

## Purpose and exclusions

Terrain collision, canonical scar state and navigation on the ARM side, derived from the exact same field semantics the FPGA evaluates.

Phase-1 scope: this software block is wave-1-active. Its contract is authoritative NOW; the headings below that name C++/RTL artifacts describe the shape of the evidence to come, and no maturity advance happens without that evidence being committed (rules V2/V3).

## Input and output packet layouts

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Backpressure rules

Backpressure: `none`.

## Memory ownership

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Q formats and rounding

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Latency (fixed or variable)

Latency: `variable`.

## Target throughput

Target throughput: gameplay-rate queries.

## Overflow and malformed-input behaviour

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Directed tests

Planned: `(tbd)`.

## Randomized differential tests

Planned: `(tbd)`.

## Integration capture cases

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Notes

One semantics, two consumers (FPGA terrain + CPU collision) — never re-derived by hand (§29-6).
