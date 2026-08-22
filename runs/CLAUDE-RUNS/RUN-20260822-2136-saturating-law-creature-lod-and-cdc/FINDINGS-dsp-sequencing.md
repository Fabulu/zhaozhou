# DSP sequencing pilot on zhao_geom_lod - Findings

**Agent ID:** claude-dsp-sequencing
**Created:** 2026-08-23
**Parent Task:** RUN-20260822-2136
**Status:** Complete

---

## Summary

`zhao_geom_lod`'s five 32x32 products were sequenced through **one** multiplier
over five clocks, and the block fit measured **18 DSPs -> 6** with **ALMs also
falling, 1,303 -> 1,183**. The block went from 16% of the device's DSPs to 5.4%,
for four extra clocks on a path whose rate has a 16x margin. The lever the
2026-08-23 blocker report named works, and it is larger than that report's
"roughly 8" estimate, because once a sequencer exists there is no reason for the
other two products to stay outside it.

---

## Findings

### 1. The measurement, both sides, same machine

| | ALMs | DSPs | registers | fit seconds | commit | worktree |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| before | 1,303 | **18** | 21 | 456.3 | `d8278bd` | clean |
| after | 1,183 | **6** | 271 | 631.1 | `09bbe05` | clean |
| delta | **-120 (-9.2%)** | **-12 (-67%)** | +250 | +175 | | |

Quartus Prime Lite 17.0.2, 5CSEBA6U23I7 (provisional), `run_block_fit.ps1`,
virtual pins, no composed fit. Device share: **16.1% -> 5.4%** of 112 DSPs.

The BEFORE was re-measured rather than quoted. The committed row for this block
said 1,303 / 18 but carried `sourceCommit: 8d48608` and `rtlCleanAtHead: false`
-- a provenance flag that had never once been true in that file, which is
indistinguishable from not having it. Re-run at HEAD with a clean worktree it
reproduced to the digit, and that row was committed on its own (`3f1d45f`)
before anything changed, so the comparison is not against a number in a comment.

### 2. Six, not eight -- and the reason generalises

The block's own header, and `reports/REMAINING_BLOCKERS.md`, predicted "roughly
8" from sequencing *the three legality products*. That under-counts. The cost of
sequencing is the sequencer -- a state register, an operand mux, and the flops
that hold the intermediate results. Once it is paid for three products, adding
the other two is nearly free, so `thresh*R` and the boundary product joined the
same multiplier and the answer is 6.

**The estimate to use for the remaining blocks is therefore not "share the
obvious group" but "how many products can reach ONE multiplier".**

### 3. The ALMs went DOWN, and that kills the standing objection

+250 registers and -120 ALMs at the same time. Two things caused it:

* A Cyclone V ALM carries registers whether the design uses them or not. At 21
  registers this block was using almost none of the flops it had already paid
  for; 271 still fit inside fewer ALMs.
* The area was never mostly the multipliers. It was **five parallel 64-bit
  product-and-compare datapaths** -- five 64-bit adders, five 64-bit
  comparators, the sign-extension trees behind them. Sequencing collapsed those
  to one of each.

The usual objection to sequencing is that it trades area for DSPs. Measured
here, it does not: it returns both. That objection now has to be re-argued with
evidence rather than assumed for the next block.

### 4. What it cost, stated plainly

* **Latency 1 clock -> 5 clocks.** `valid_o` is a one-cycle pulse five clocks
  after the accepting edge.
* **A real handshake.** `ready_o` was added; `tick_i` is ignored while low, so a
  caller that ticks blindly cannot corrupt an evaluation in flight. This moved
  the RTL TOWARD `design/blocks.yml`, which already declared GEOM.MESHFETCH
  `backpressure: ready_valid` and `latency: variable`.
* **Throughput 1/clock -> 1/5 clocks** = 10 M evaluations/s at 50 MHz, against a
  demand near 600 k/s for ten thousand live creatures at 60 Hz. A 16x margin --
  but on an instance count nobody has ruled, which is why it is now docket
  item 4 under GEOM.MESHFETCH.

### 5. The verification did not weaken; it grew

* **Differential green, including the nightly lane.** 212,530 evaluations,
  **1,267,100 checks**, against the unmodified `zref::creature::lod_raw` /
  `lod_update`. The reference was not touched -- this was a pure hardware
  restructuring and the answers did not move.
* **The constructed section-12 boundary cases still work.** They are driven
  through `dut_step`, which is the single place that learned the new handshake.
* **`ctest -L fast`: 262/262 passed.**
* **Mutation sweep: 26 attempted / 26 accounted / 25 caught / 1 equivalent.**

The sweep went from 23 mutants to 26 because the code moved:

| | what happened |
| --- | --- |
| M01/M02/M03 | the three legality products are no longer three expressions, so these land on the rounding term and the ONE comparison they now share |
| M20/M21/M22 | re-aimed at the operand the STATE feeds the multiplier -- which is exactly where a rung can now silently borrow another rung's error term |
| M06/M13/M15/M17/M18/M19/M23 | follow their operand from a port to the register that latches it at accept |
| **M24** *(new)* | a legality bit latched into the wrong flop -- caught |
| **M25** *(new)* | `valid_o` pulsing before the answer is written -- caught |
| **M26** *(new)* | a rung's product skipped entirely -- caught |

The single survivor is **M18**, equivalent by proof (if `e[rung_i] == 0` that
rung is always legal, `raw` is the coarsest legal rung, so `raw >= rung_i` and
the refining branch M18 mutates is unreachable). That proof now lives in
`tools/sweep_geom_lod.sh`'s header instead of only in `TASK_LOG.md`, so a reader
of the sweep can see why one survivor is expected.

Two mutants had to be RE-SPELLED rather than merely re-aimed: M02 and M03
originally deleted a rounding term from an expression that no longer exists, and
writing them against the shared comparison made `half_r` unused, which fails
`-Wall`. The preflight caught that before any scoring -- exactly what it is for
-- and they were moved onto `half_r`'s own assignment, which is the same
mutation and lints.

### 6. What transfers to the bigger offenders

Surveyed rather than assumed. The decisive question is **not** whether the
products are independent -- they nearly always are -- but whether the block's
declared RATE actually consumes the parallelism.

| block | DSPs | stated rate | rate met today? | verdict |
| --- | ---: | --- | --- | --- |
| `zhao_terrain_lod` | 28 | 1 decision per patch per **frame** | yes, ~560 cycles/patch | **best target.** 30 products live permanently, consumed one cycle in 34, and 32 idle isqrt cycles per subpatch are already sitting there. Its contract already names six of them as shifts, not multiplies |
| `zhao_texture_tmu` | 28 | 1 sample/clock | **no -- 1 per 4 or 6, stated in its own header** | **strong.** The rate line is already missed by design, and 12 of its 32 products are literal duplicates across four `zhao_texture_bilerp` instances driven by the same fractions |
| `zhao_surface_stamp` | 28 | 1 texel/clock | yes, measured 4,102 cycles | **partial.** The two radius squares are per-stamp constants latched in an already-idle acquire state -- free. The four per-texel products are on the rate and are not |
| `zhao_terrain_project` | 33 | 1 vertex/clock | **yes, and consumed** | **do not sequence blindly.** 6,144 clocks/patch already gives ~270 patches against a 256-patch budget. Its own budget note prices row time-multiplexing at 33->12 but says outright that it "trades directly against the ledger's 1 vertex per clock." That is a contract question first |

Three specific transferable lessons:

1. **Count products that can reach ONE multiplier, not products in the obvious
   group.** That is the difference between the predicted 8 and the measured 6.
2. **Expect ALMs to fall, not rise**, wherever the parallel form carried wide
   adders and comparators alongside the multipliers. Do not pre-emptively
   trade area away.
3. **The rate line in `design/blocks.yml` is the gate, and for two of these four
   blocks the RTL already misses it or has enormous slack against it.** Check
   the block's own header before the ledger's: `zhao_texture_tmu` states its
   real rate (1 per 4-6 clocks) in prose while the ledger still says 1/clock.

Applying only what was measured, the running per-block DSP total moves from
**213 to 201** against a device with **112**. The lever works. It has to be
pulled about ten more times.

---

## Recommendations

- **Next block: `zhao_terrain_lod` (28 DSPs).** Its rate is one decision per
  patch per *frame*, it already has a 4-state FSM with 32 idle isqrt cycles per
  subpatch, and all 30 of its products are combinational-always-live but
  consumed one cycle in 34. Its contract already names six of the 24 ladder
  multiplies as a constant shift.
- **Then `zhao_texture_tmu` (28 DSPs)**, hoisting the four bilerp weight
  products up into the TMU: 12 of 32 products are exact duplicates. Its contract
  flags this as sanctioned but structurally invasive (ports, directed tests,
  formal harness).
- **Do not touch `zhao_terrain_project` as a restructuring.** Its 33 DSPs buy a
  rate the design actually spends. Sequencing it is an owner question about the
  patch budget, not a hardware cleanup.
- **Give the ledger a DSP number to enforce.** `resource_budget.dsp_percent` and
  `resource_actual.dsp` exist in the schema and in `tools/ledger/src/types.ts`,
  nothing writes them, and no validator reads them. That is precisely how the
  design reached 1.9x the device's DSP capacity with every gate green. The ALM
  side has V5; DSP has nothing.

---

## Files Created in This Directory

- `FINDINGS-dsp-sequencing.md` - this file

---

## Files Examined

- `fpga/rtl/geometry/zhao_geom_lod.sv` - the block restructured and measured
- `tests/differential/geom_lod_directed.cpp` - the differential; `dut_step` learned the handshake
- `tools/sweep_geom_lod.sh`, `tools/sweep_geom_lod_preflight.py` - the sweep, grown 23 -> 26
- `tools/quartus/run_block_fit.ps1` - the measurement
- `reports/synthesis/zhao_block_fit.json` - both rows
- `design/contracts/GEOM.MESHFETCH.md` - latency section filled from measurement
- `design/blocks.yml` - GEOM.MESHFETCH already declared `backpressure: ready_valid`
- `docs/OWNER_DOCKET.md` - the five-clock choice, item 4
- `reports/REMAINING_BLOCKERS.md` - the 213/112 section, now with a measured addendum
- `fpga/rtl/terrain/zhao_terrain_lod.sv`, `fpga/rtl/texture/zhao_texture_tmu.sv`,
  `fpga/rtl/surface/zhao_surface_stamp.sv`, `fpga/rtl/terrain/zhao_terrain_project.sv`
  - surveyed for the generalisation question
