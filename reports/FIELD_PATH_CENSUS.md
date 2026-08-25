# FIELD.SEQ timing path census — 200 paths, grouped by cone family

*Owner directive of 2026-08-25 (`reports/WordOfCaution`): group at least the top
200 setup paths by distinct logical cone, report the worst per family, the
counts above 10/12/15/18 ns, the runner-up and third-place families, and which
cuts would share one pipeline boundary.*

Measured at `2727d85` (wave 10, 58.99 MHz), `zhao_field_seq` leaf fit,
`-npaths 200 -nworst 1` so every row is a distinct endpoint rather than 200 bits
of one bus. Delay is `period - slack` with a 10 ns period.

---

## Why this exists

`block_paths.tcl` reported **25** paths. That was enough to see the winner and
nothing else, and wave 8 is what proved it insufficient: registering SIN's
output removed the #1 cone entirely and bought **2%**, because a NORMALIZE cone
of equal length was standing behind it at 19.875 ns — 0.16 ns *worse* than the
path it replaced. The 25-path report could not have shown that, and I had been
reading only path #1 for six consecutive waves.

---

## The answer to the question that was asked

> "We need to know whether Field contains two ~20 ns cones or eight before
> spending the remaining opcode-cycle headroom."

**Neither. It contains a PLATEAU.**

| threshold | paths above |
| --- | ---: |
| > 10 ns | **200 / 200** |
| > 12 ns | **200 / 200** |
| > 15 ns | **83 / 200** |
| > 18 ns | **0 / 200** |

The entire top 200 lies between roughly 12 and 17 ns. There is no tall spike to
remove and no clean second place — the 200th path is only ~5 ns faster than the
first.

**This is why the wave-by-wave gains have been shrinking**: 19.1% (wave 5),
3.5% (6), 7.7% (7), **2.0% (8)**, 9.8% (9), 7.6% (10). Each cut lands on one
family and the plateau underneath is unmoved.

---

## By source family

| family | paths | worst slack | worst delay |
| --- | ---: | ---: | ---: |
| `mul` | **97** | -5.423 | 15.42 ns |
| `normalize` | 41 | -5.909 | 15.91 ns |
| `ring` | 37 | -6.951 | **16.95 ns** |
| (sequencer top) | 25 | -5.738 | 15.74 ns |

## By destination family

| family | paths | worst slack | worst delay |
| --- | ---: | ---: | ---: |
| `mul` | 63 | -6.951 | **16.95 ns** |
| `rcp` | 15 | -6.659 | 16.66 ns |
| (sequencer top) | 10 | -5.738 | 15.74 ns |
| `normalize` | **96** | -5.423 | 15.42 ns |
| `curve` | 16 | -4.546 | 14.55 ns |

---

## Which cuts share one pipeline boundary

**The shared multiplier lane is the hub: 97 paths leave `u_mul` and 63 arrive at
it — 160 of 200 endpoints touch it.**

That is the 2026-08-23 rearchitecture working exactly as designed (ten private
multipliers became one shared lane, 79 DSPs became 3) and it is also why the
timing plateau exists: every arithmetic op now routes through one physical
place, so every op's cone passes through the same silicon.

**A single register pair at `u_mul`'s operand and result therefore cuts
`ring`, `normalize`, `rcp`, `curve` and the sequencer top SIMULTANEOUSLY.**

Contrast with the wave 11 I was about to write, which registered RING's operand
alone: worst path 16.95 → ~15.9 ns, exposing `normalize` immediately behind it.
That is wave 8 again — a ~1 ns gain for a real change — and the census is what
made it visible before the work rather than after the fit.

---

## What this does not settle

* **Routing fraction is rising**: 30% at wave 6, 42% at wave 9, **48% at wave
  10**. `QUARTUS_GOTCHAS` §12 warns that a leaf fit in a mostly-empty device
  measures routing, not the design. At 26–39 logic levels the depth still
  dominates, but the margin is narrowing and no number here is a system
  frequency until a composed fit exists.
* **The clock is not the binding constraint anyway.** See
  `reports/EARTH60_CAPACITY.md`: the engine retires one instruction every 7
  clocks against a cost model that assumed 1, and that 7x gap dwarfs the
  remaining 1.7x of clock work.
