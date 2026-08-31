# Contract — POST.ECHO (Frame echo (optional))

> Ledger: `design/blocks.yml` · owner ZH-071 · phase 11 · maturity SPECIFIED · deferred

## Purpose and exclusions

Optional echo of the composited frame back to a capture buffer; first on the §26 cut list.

## Clock and reset semantics
**DEFERRED — owner ruling 2026-08-31 §4.** Not part of the v1 silicon
requirement; first on the cut list.

The ruling's instruction is specific and it is what this contract records:
**keep no expensive storage or datapath for it now.** A later PC implementation
or an evidence-backed hardware revival is allowed; **the base console does not
wait for it.**

The sections below therefore state what it WOULD be, at the level needed to keep
the seam cheap, and no further. Writing a full contract for a deferred block
would be the same error as building one: it makes the design look decided when
it is not.

## Input and output packet layouts
Would echo the composited frame to a capture buffer, after step 8 and before
HUD — so a capture shows the world as the player saw it, without the interface.

**The seam that must be kept:** `POST.COMPOSITE` should not make the
post-ink/pre-HUD image unavailable. That costs nothing today — it is a tap point,
not a buffer — and it is the whole of what "keep the seam where cheap" means
here.

## Backpressure rules

**Deliberately unwritten.** This block is DEFERRED by owner ruling 2026-08-31 §4 and is first on the cut list. Specifying clocks, packets, throughput and test plans for a block nobody is building would make the design look decided when it is not — the same error as building it. Fill this in only if an evidence-backed revival happens.

## Memory ownership
Would own a capture buffer. **None is allocated now**, per the ruling.

## Q formats and rounding

**Deliberately unwritten.** This block is DEFERRED by owner ruling 2026-08-31 §4 and is first on the cut list. Specifying clocks, packets, throughput and test plans for a block nobody is building would make the design look decided when it is not — the same error as building it. Fill this in only if an evidence-backed revival happens.

## Latency (fixed or variable)

**Deliberately unwritten.** This block is DEFERRED by owner ruling 2026-08-31 §4 and is first on the cut list. Specifying clocks, packets, throughput and test plans for a block nobody is building would make the design look decided when it is not — the same error as building it. Fill this in only if an evidence-backed revival happens.

## Target throughput

**Deliberately unwritten.** This block is DEFERRED by owner ruling 2026-08-31 §4 and is first on the cut list. Specifying clocks, packets, throughput and test plans for a block nobody is building would make the design look decided when it is not — the same error as building it. Fill this in only if an evidence-backed revival happens.

## Overflow and malformed-input behaviour
Would drop the echo rather than stall or fault the frame. An echo is
observational; it must never be able to affect what is displayed.

## Counters and traces

**Deliberately unwritten.** This block is DEFERRED by owner ruling 2026-08-31 §4 and is first on the cut list. Specifying clocks, packets, throughput and test plans for a block nobody is building would make the design look decided when it is not — the same error as building it. Fill this in only if an evidence-backed revival happens.

## Scalar reference function
`zref::PostEcho` (ledger). **Not implemented.** The ledger entry stays so the
block is countable as deferred rather than forgotten.

## Directed tests

**Deliberately unwritten.** This block is DEFERRED by owner ruling 2026-08-31 §4 and is first on the cut list. Specifying clocks, packets, throughput and test plans for a block nobody is building would make the design look decided when it is not — the same error as building it. Fill this in only if an evidence-backed revival happens.

## Randomized differential tests

**Deliberately unwritten.** This block is DEFERRED by owner ruling 2026-08-31 §4 and is first on the cut list. Specifying clocks, packets, throughput and test plans for a block nobody is building would make the design look decided when it is not — the same error as building it. Fill this in only if an evidence-backed revival happens.

## Formal properties

**Deliberately unwritten.** This block is DEFERRED by owner ruling 2026-08-31 §4 and is first on the cut list. Specifying clocks, packets, throughput and test plans for a block nobody is building would make the design look decided when it is not — the same error as building it. Fill this in only if an evidence-backed revival happens.

## Synthesis / resource ceiling
**Zero.** Nothing is built. If this block acquires area before an
evidence-backed revival, the deferral has been ignored.

## Integration capture cases

**Deliberately unwritten.** This block is DEFERRED by owner ruling 2026-08-31 §4 and is first on the cut list. Specifying clocks, packets, throughput and test plans for a block nobody is building would make the design look decided when it is not — the same error as building it. Fill this in only if an evidence-backed revival happens.

## Notes

cut_order 1 — the first thing to go if synthesis fails (§26).
