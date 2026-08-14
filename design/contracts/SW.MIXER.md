# Contract — SW.MIXER (Fixed-point audio mixer)

> Ledger: `design/blocks.yml` · owner ZH-075 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

48 kHz fixed-point 32-voice 3-bus mixer; same code in ZRef/ZEmu/hardware path (Q5 ruling); fx24 internal accumulators.

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

Latency: `fixed:1`.

## Target throughput

Target throughput: 1 output sample per 48 kHz tick.

## Overflow and malformed-input behaviour

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Directed tests

Planned: `(tbd)`.

## Randomized differential tests

Planned: `(tbd)`.

## Integration capture cases

TODO — fill before this block advances past SPECIFIED (charter §4: no RTL before contract and reference exist).

## Notes

Contract filled (Phase-1-active scope note). Adopts the ZRef fixed-point library per plan Q5.
