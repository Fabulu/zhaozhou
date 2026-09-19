# Contract — SYS.RESET (Reset sequencer)

> Ledger: `design/blocks.yml` · owner ZH-000 · phase 0 · maturity SPECIFIED · **blocked_on: none**

> **AUTHORED 2026-09-19**, replacing fifteen sections of "Deliberately
> unwritten … they wait for the board". The board was probed on 2026-09-13:
> `zhaozhou-board-bringup-20260913/reports/board_truth.json`, commit
> `eee32c4e`. Owner ruling 2026-08-31 §8's condition is met and
> `design/blocks.yml` records the strike.
>
> Numbers are tagged **MEASURED** (`board_truth.json` states it and names its
> evidence), **DERIVED** (computed here from a MEASURED number plus a ratified
> spec, computation shown) or **UNSETTLED** (the tree does not contain it and
> this contract will not invent it). **Every one is a named, editable parameter
> of `zhao_sys_reset.sv`.**

## Purpose and exclusions

Sequence per-domain resets after `pll_locked`; formal property
reset-reaches-idle (every FSM returns to its idle state on reset assertion).

**Owns:** the one asynchronous-assert / synchronous-release reset network for
the console — `rst_n_gpu`, `rst_n_sdram`, `rst_n_video`, `rst_n_audio` — the
staggered release order between them, and the `reset_assertions` census.

**Excludes:** frequency synthesis and `pll_locked` itself (SYS.PLL); data
crossings between domains (SYS.CDC); any per-block internal reset. A block that
synthesises its own reset from a counter is the defect this block exists to make
unnecessary.

**This block does not reset anything's *contents*.** `spec/…` per-block reset
states — SDRAM's precharge-all/refresh-armed init, VIDEO.MODE's `x=0/y=0` and
`VIDEO_Z60`, AUDIO.FIFO's empty-and-silent — are each block's own obligation on
seeing its `rst_n` low. This block only decides *when* each `rst_n` is low.
CLAUDE.md's wording is the standing warning: *a synchronized reset release is
not a memory-drain certificate* (completion plan §12.2 says the same).

## Clock and reset semantics

### The reset source — MEASURED

`board_truth.json` `reset`:

| Field | Value | Tag |
|---|---|---|
| `coreContract` | **"MiSTer HPS/framework RESET, synchronized release after core PLL lock"** | MEASURED |
| `volatileLoadResetSequenceStatus` | `proven` | MEASURED |
| `physicalButtonMapping` | `null` | **UNMEASURED** — `openCapabilities` still lists `physical_button_reset_mapping` |

**That first row is the contract.** The board's own proven reset convention is
*assert on framework reset, release synchronously after the core's PLL locks* —
which is precisely the structure below. This block is not inventing a scheme; it
is implementing the one the board has already been shown to accept.

**What is not claimed:** which physical button, if any, reaches
`hard_reset_n_i`. That is a board-framework/pin question, it is open in the
capture, and it is `zhao_console_board`'s to answer when a pin map exists.

### The structure

```
  arst_n  =  hard_reset_n_i  &  pll_locked_i          (asynchronous, active low)
```

* **Assertion is ASYNCHRONOUS and immediate.** Either input falling drives every
  `rst_n_*` low with no clock required. This is the safety direction: a domain
  whose clock has stopped because the PLL unlocked cannot be reset
  synchronously, and that is exactly when it most needs to be.
* **Release is SYNCHRONOUS, per domain, and STAGGERED.** Each domain carries its
  own `SYNC_STAGES`-deep (default 2) shift register clocked by *that domain's*
  clock, so the release edge is retimed into the domain that consumes it and
  metastability on the release is resolved locally.
* **The stagger lives in the `ref_clk_i` domain**, in a single bounded counter,
  so the order between domains is decided once by a clock that is running
  whether or not the PLL is locked. `ref_clk_i` is `FPGA_CLK1_50`, 50 MHz, pin
  V11 — MEASURED, `board_truth.json` `clocks.fpgaInputs`.

`pll_locked_i` is **asynchronous** (SYS.PLL says so explicitly). It is used
directly as an async *assert*, which is safe by construction, and is separately
two-flop synchronised into `ref_clk_i` for the census, which is the only place
its *value* is read.

## Input and output packet layouts

No packets.

| Port | Dir | Meaning |
|---|---|---|
| `ref_clk_i` | in | `FPGA_CLK1_50`; runs the stagger counter and the census |
| `pll_locked_i` | in | from SYS.PLL, asynchronous |
| `hard_reset_n_i` | in | board hard reset, asynchronous, active low |
| `gpu_clk_i`, `sdram_clk_i`, `video_clk_i`, `audio_clk_i` | in | the four domain clocks, from SYS.PLL |
| `rst_n_gpu_o`, `rst_n_sdram_o`, `rst_n_video_o`, `rst_n_audio_o` | out | active low, async assert / sync release, one per domain |
| `seq_done_o` | out | `ref_clk_i`; the stagger counter reached `RELEASE_SPAN` |
| `reset_assertions_o` | out | `ref_clk_i`; the census |

**Deviation from the ledger, declared.** `design/blocks.yml` lists inputs
`[pll_locked, hard_reset_n]` and outputs `[rst_n_gpu, rst_n_sdram, rst_n_video,
rst_n_audio]`. The four outputs and both listed inputs are present exactly as
named. The ledger does not list the **five clocks**, and it could not: a
synchronous release requires the clock it is synchronous to. `seq_done_o` and
`reset_assertions_o` are additions — the census is the ledger's own
`counters: [reset_assertions]`, and `seq_done_o` exists so the bounded-latency
law below is *observable* rather than asserted.

## Backpressure rules

None. Nothing can refuse a reset.

## Memory ownership

None. Total state: one `RELEASE_SPAN`-bounded counter, four short shift
registers, a two-flop lock synchroniser, one edge register and the census.

## Q formats and rounding

None. All quantities are cycle counts of `ref_clk_i`, integers, exact.

## Latency (fixed or variable)

The ledger says **`variable_bounded:16`**, and this contract makes that exact.

**The bound is 16 `ref_clk_i` cycles for the SEQUENCER, plus `SYNC_STAGES`
cycles of each domain's own clock for that domain's release.** Both halves are
stated because only the first is 16, and quoting the first alone would be a
number that is true about the wrong thing.

| Parameter | Default | Tag | Meaning |
|---|---|---|---|
| `RELEASE_SPAN` | 16 | **DERIVED** — it is the ledger's `variable_bounded:16`, read as the sequencer's bound | the counter saturates here; `seq_done_o` rises |
| `RELEASE_STEP_SDRAM` | 0 | **DERIVED (ordering)** | released first, at ref cycle 1 |
| `RELEASE_STEP_GPU` | 3 | **DERIVED (ordering)** | |
| `RELEASE_STEP_VIDEO` | 6 | **DERIVED (ordering)** | |
| `RELEASE_STEP_AUDIO` | 9 | **DERIVED (ordering)** | released last |
| `SYNC_STAGES` | 2 | DERIVED | the standard two-flop release synchroniser |

**The comparison is STRICT.** Domain X releases on the first `ref_clk_i` cycle
where the stagger counter **exceeds** its step — at cycle `STEP + 1`. It is `>`
and not `>=` because `step_q >= 0` is constant-true, Verilator's `UNSIGNED`
warning said so, and suppressing that warning would have hidden a release level
that can never fall. The elaboration guard therefore refuses a step at or beyond
`RELEASE_SPAN - 1`, which is where the counter saturates.

**"Variable" means what, exactly.** The *sequencer's* schedule is fixed: given
`arst_n` rising at `ref_clk_i` cycle 0, step `k` releases at cycle `k + 1`. The
latency is variable because **`arst_n`'s rise is an event, not a delay** — it
waits on `pll_locked_i`, whose own lock time is UNSETTLED (SYS.PLL's *Latency*
section). The 16 is the bound on everything after that event.

**Why this order.** It is a choice, it is DERIVED from what each domain does at
reset, and it is four editable constants rather than a hard-coded sequence:

1. **SDRAM first.** `MEM.SDRAM.md`: on reset the controller runs
   PRECHARGE-ALL + 2×AUTO_REFRESH + MODE REGISTER SET *before any client
   traffic*. It has the longest thing to do and the rest of the machine reads
   through it.
2. **GPU next**, so the render/compute clients come up into a memory system that
   has already started its init.
3. **VIDEO next.** `spec/video_rules.md` §4: with no complete frame the raster
   displays black and repeats — a correct, defined state that does not depend on
   anyone else being up.
4. **AUDIO last.** `AUDIO.FIFO.md`: reset leaves the FIFO empty and the output
   *silent* — "zero pairs, NOT repeats — there is no 'last pair' yet". Releasing
   it last minimises the window in which it is running and starved.

**The order is not a correctness requirement of any block.** Every one of the
four is specified to be safe from its own reset regardless of the others. The
stagger is chosen to keep startup transients apart and to keep the gap editable;
setting all four steps to 0 is a legal configuration and the bench proves the
block still behaves.

An elaboration check (inside `initial begin ... end`, the form Quartus 17.0
requires) refuses any configuration with a step at or beyond `RELEASE_SPAN`,
because such a domain would never be released at all.

## Target throughput

`n/a (sequencer)`.

## Overflow and malformed-input behaviour

* The stagger counter **saturates** at `RELEASE_SPAN - 1`; it never wraps, so
  the release levels are monotonic and a released domain cannot be re-asserted
  by the counter rolling over. This is the one bug this shape most easily has.
* `reset_assertions_o` **saturates** at `2**ASSERT_W - 1` (default width 16). A
  wrapping census can read zero, which is the flattering direction.
* `pll_locked_i` chattering produces one census increment per fall and a full
  re-run of the stagger each time. Correct, and deliberately not filtered: a
  chattering PLL should be *visible* in the census, not smoothed away.
* Both inputs low simultaneously is the ordinary power-on case, not an error.
* A domain clock that is not running holds that domain's `rst_n` at whatever the
  async assert left it — **low**, because the assert needs no clock. Safe
  direction, stated because its safety is structural rather than lucky.

## Counters and traces

`reset_assertions_o` (ledger `counters: [reset_assertions]`), `ref_clk_i`
domain, saturating, width `ASSERT_W` (default 16).

**Definition:** it increments on each transition of the reset network from
*released* to *asserted*, as observed in `ref_clk_i` through the two-flop lock
synchroniser. It is reset by `hard_reset_n_i` **only** — never by
`pll_locked_i` — so a lock loss is counted rather than erased by the thing it is
counting. That separation is the point: CLAUDE.md's *detector wired to two
operands that move together cannot fire* is exactly a census cleared by its own
trigger.

The power-on assertion is not counted; there is no released state before it to
transition from. The first count is the first *loss*.

**How it is fired** (a detector that has not been shown to FIRE has not been
tested): drop `pll_locked_i` after the sequence completes. The directed bench
does it three times and asserts the delta each time — never the absolute value
alone.

**And what does NOT fire it, stated so nobody has to discover it:** pulsing
`hard_reset_n_i` **clears** this census rather than incrementing it, because
`hard_reset_n_i` is its reset. That is not the counter failing to see an
assertion — it is the census being scoped to *losses since power-on* — and
directed test 7 asserts the asymmetry, so the two resets are demonstrably
different resets rather than the same one spelled twice.

**What the two operands are clocked by**, stated because CLAUDE.md requires the
question to be asked of every checker: `rel_q` and the live synchroniser output
are adjacent stages of one `ref_clk_i` shift register, one cycle apart by
construction. It is an edge detector, so it is blind to nothing that the level
itself can express — and the *level* is what a reset is.

No per-transaction trace.

## Scalar reference function

**None.** A reset network's observable is a set of levels against a clock; there
is no value for a `zref::` oracle to return. The bench is the specification,
expressed as timing assertions, and the formal lane below is the general case.

Building a C++ model of "rst_n goes low when either input goes low" would be a
second implementation of one line of RTL, which `GEOM.LIGHT.md` line 118 names
as the failure to avoid.

## Directed tests

`tests/platform/sys_reset_directed.cpp`, verilated as `sys_reset_directed`.

1. **Power-on:** all four `rst_n_*` low, with no clock edge required — checked
   before any clock is toggled.
2. **No release without lock:** with `hard_reset_n_i` high and `pll_locked_i`
   low, run hundreds of cycles of all five clocks; every `rst_n_*` stays low.
   (The `blocked_on` note says this block exists to wait for lock; a bench that
   never tested the waiting would be testing the easy half.)
3. **Staggered release, in order:** after lock, `rst_n_sdram_o` rises before
   `rst_n_gpu_o` before `rst_n_video_o` before `rst_n_audio_o`, and each at its
   declared `ref_clk_i` step plus its own `SYNC_STAGES`.
4. **The bounded-latency law:** `seq_done_o` rises within `RELEASE_SPAN`
   `ref_clk_i` cycles of `arst_n` rising, and all four outputs are high by then.
   This is the ledger's `variable_bounded:16` asserted as a number.
5. **Asynchronous assertion:** with every clock held *static*, drop
   `pll_locked_i` and evaluate — all four outputs fall with no clock edge. A
   synchronous-assert implementation passes tests 1–4 and fails this one.
6. **Census as a delta:** three induced losses, `reset_assertions_o` +1 each,
   stable between them; and the census is *not* cleared by the loss.
7. **The two resets are different resets:** `hard_reset_n_i` clears the census,
   `pll_locked_i` does not.
8. **Re-release after a loss:** the stagger re-runs from step 0, in order, and
   `seq_done_o` re-rises — the sequencer is re-armable, not one-shot.
9. **Parameter sensitivity:** a second elaboration with all four steps at 0
   releases every domain together and still satisfies 1, 2, 4 and 5 — proving
   the order is a knob and not a hidden law.

## Randomized differential tests

**None.** Same boundary as SYS.PLL: the second implementation is the silicon,
and the board is under an explicit review hold
(`board_truth.json` `reviewHold.futurePhysicalLoadsAuthorized: false`). What
randomisation would buy here — arbitrary relative clock phases between five
domains — is the formal lane's job below, and it gets it exhaustively rather
than by sampling.

## Formal properties

`tests/formal/sys_reset_sequence.sby` (ledger `tests.formal`). **NOT WRITTEN BY
THIS PASS**, named here as the obligation:

* `reset_reaches_idle` — the ledger's own stated property: on assertion every
  output is low within 0 cycles and stays low while `arst_n` is low. This is the
  clause `design/blocks.yml` names in SYS.RESET's `purpose`.
* `release_is_ordered` — for any interleaving of the five clocks,
  `rst_n_sdram_o` is never released later than `rst_n_audio_o`, and the declared
  order holds pairwise.
* `release_is_bounded` — `seq_done_o` rises within `RELEASE_SPAN` `ref_clk_i`
  cycles of `arst_n`, under any clock ratio.
* `no_glitch_on_release` — no output is asserted again without `arst_n` having
  fallen. The stagger counter's saturation is what makes this true, so it is the
  property that would catch a wrapping counter.
* `census_saturates` — `reset_assertions_o` never wraps.

Relative clock phase is free in the formal harness. That is the whole reason
this block gets a formal lane rather than a longer directed one.

## Synthesis / resource ceiling

Pure flops and one small counter. **Expected: 0 DSP, 0 M10K, on the order of
40–60 ALMs**, dominated by the census width and the counter.

**No fit row exists. That number is an expectation, not a measurement** — the
campaign's rule is to fit at completion, and CLAUDE.md's rule is that a block
which has never been through `quartus_map` has not been shown to be
synthesizable however clean its lint. Both apply.

**The part of this block that a fit does not measure and an SDC must:**
`ref_clk_i` to each domain is a real asynchronous crossing at the release
synchronisers. Those need `set_false_path` on the *level*, and only there —
completion plan §12.2: *"cross-domain false paths are permitted only at actual
CDC structures, not to erase a real slow synchronous path."* Recovery and
removal checks on the reset flops of every downstream block are the other half,
and they are the board framework's constraint file, not this RTL's. Neither
exists yet.

## Integration capture cases

None in Phase 2 — a `.zcap` records frames and packets.

The capture that would settle this block is a board capture, and **it is blocked
by an explicit review hold**: `board_truth.json`
`reviewHold.futurePhysicalLoadsAuthorized: false` with seven prerequisites. **Do
not load, flash or SSH to the board to close this section.** The existing
`volatileLoadResetSequenceStatus: proven` row is evidence about a *MiSTer
framework core's* reset, not about this block.

## Notes

Fanout to every domain's reset; only the SYS.CDC edge is drawn in the schematic.

**What a reader should distrust first, in order:**

1. The **release order**, because it is a judgement dressed as four constants.
   It is defensible (each step cites the consuming contract's own reset clause)
   and it is not measured, and test 9 exists so that changing it is cheap.
2. **`RELEASE_SPAN = 16`**, because it is read off the ledger's
   `variable_bounded:16` rather than derived from any physical requirement. The
   ledger is the only source for it in the tree.
3. The claim that assertion is asynchronous, because it is the property most
   easily lost by a later edit and the only test that catches it is test 5.
4. Anything this contract says about **SDC constraints**, because none are
   written.
