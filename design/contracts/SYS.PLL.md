# Contract — SYS.PLL (Vendor PLL wrappers)

> Ledger: `design/blocks.yml` · owner ZH-000 · phase 0 · maturity SPECIFIED · **blocked_on: none**

> **AUTHORED 2026-09-19, replacing fifteen sections of "Deliberately unwritten …
> they wait for the board".** The placeholder cited owner ruling 2026-08-31 §8
> and it was right to: every number here is a property of a physical device.
> **That device has since been probed.**
> `zhaozhou-board-bringup-20260913/reports/board_truth.json`, captured
> 2026-09-13 at commit `eee32c4e`, is the capture, and `design/blocks.yml`'s
> `blocked_on` field for this block was struck to `none` citing it.
>
> **Two rules govern every number below.**
>
> 1. Each is tagged **MEASURED**, **DERIVED** or **UNSETTLED**. MEASURED means
>    `board_truth.json` states it and names its evidence. DERIVED means this
>    document computed it from a MEASURED number plus a ratified spec, and the
>    computation is shown. UNSETTLED means the tree does not contain the answer
>    and this contract refuses to invent one.
> 2. **Every value is a named, editable parameter of `zhao_sys_pll.sv`.**
>    CLAUDE.md's art law rule 6 — "never remove the owner's control in the name
>    of fidelity" — applies to clock frequencies exactly as it applies to a
>    creature's radius. A frequency derived from board truth is still a knob.

## Purpose and exclusions

Instantiate the Cyclone V PLL/IP wrappers producing the gpu/sdram/video/audio
clock tree; nothing else may touch clock configuration.

**Owns:** the single reference-to-domain frequency synthesis for the whole
console, the `pll_locked` output every other block's reset release waits on, and
the `pll_lock_lost` census.

**Excludes:** reset sequencing (SYS.RESET — this block emits `pll_locked` and
nothing more), clock-domain crossing structures (SYS.CDC), pin assignment and
I/O standards (the board framework `zhao_console_board` / the QSF), and SDRAM
timing parameters (MEM.SDRAM, `zhao_sdram_params_pkg`, ZH-004 seam).

**No other block may instantiate a PLL, a clock divider producing a clock, or a
gated clock.** A second frequency source in the tree is the defect this block
exists to make impossible.

## Clock and reset semantics

### The reference — MEASURED

`board_truth.json` `clocks.fpgaInputs`, evidence
`fpgaContractStatus: physically_proven_for_pinned_mister_framework`:

| Net | Frequency | Pin | I/O standard | Tag |
|---|---|---|---|---|
| `FPGA_CLK1_50` | 50,000,000 Hz | V11 | 3.3-V LVTTL | MEASURED |
| `FPGA_CLK2_50` | 50,000,000 Hz | Y13 | 3.3-V LVTTL | MEASURED |
| `FPGA_CLK3_50` | 50,000,000 Hz | E11 | 3.3-V LVTTL | MEASURED |
| `hpsOsc1` | 25,000,000 Hz | (HPS internal) | — | MEASURED, evidence `live device-tree clock-frequency 0x017d7840` |

`REF_CLK_HZ = 50_000_000` is the parameter default and **this block takes one
reference**, `FPGA_CLK1_50`. The other two are left to the board framework; the
console does not need three references and a second PLL fed from a second
oscillator would be a second frequency authority.

**What is NOT measured and is not claimed here:** the oscillator part markings
(`board_truth.json` `clocks.physicalOscillatorPartMarkings: null`), so
**no ppm stability, jitter or temperature-drift figure appears in this
contract.** `openCapabilities` lists `physical_oscillator_markings` as still
open. Any future SDC `derive_clock_uncertainty` refinement needs that capture,
not this document.

### The device — MEASURED

`board_truth.json` `fpga`: `buildTarget 5CSEBA6U23I7`, `packageContract
UFBGA-672`, `speedGradeContract 7`, family Cyclone V SoC, and
`compatibilityStatus: physically_proven_for_mister_rbf`. The PLL primitive is
therefore the Cyclone V fractional PLL, reached through `altera_pll` (the
Qsys/IP-Catalog wrapper) — see *Synthesis* below.

`physicalMarkingStatus: unread`: the *package top* was never read with an eye.
The part is proven by a loaded MiSTer RBF, not by a photograph. That is enough
to select the PLL primitive family and not enough to close
`openCapabilities.fpga_top_marking`.

### The reset

`pll_arst_i` — **active-high, asynchronous**, the Cyclone V PLL's own reset
convention. It is a real port and not a test hook: the vendor primitive has one,
and it is also the only legal stimulus that can make `pll_lock_lost_o` move (see
*Counters*). A counter that cannot be fired is not an instrument (CLAUDE.md,
*a detector that has not been shown to FIRE has not been tested*).

`arst_n_i` — **active-low, asynchronous**, the board's raw hard reset. It resets
this block's housekeeping (the lock synchroniser and the lock-loss census) and
**nothing else**. It is deliberately separate from `pll_arst_i`, because a
single reset would clear the lock-loss counter in the same instant the loss it
is counting occurs — the cancelling-errors-inside-a-checker shape CLAUDE.md
records, where the detector reads a reassuring zero because both of its operands
moved together.

`pll_locked_o` is **asynchronous to every domain**, including `ref_clk_i`. Every
consumer synchronises it. SYS.RESET does; so does this block's own census.

## Input and output packet layouts

No packets. This is a configuration wrapper; its "layout" is its port list.

| Port | Dir | Width | Meaning |
|---|---|---|---|
| `ref_clk_i` | in | 1 | `FPGA_CLK1_50`, pin V11 |
| `arst_n_i` | in | 1 | board hard reset, async, active low |
| `pll_arst_i` | in | 1 | vendor PLL reset, async, active high |
| `gpu_clk_o` | out | 1 | the render/compute domain |
| `sdram_clk_o` | out | 1 | the SDR SDRAM command domain |
| `video_clk_o` | out | 1 | `vid_clk`, the raster/serializer domain |
| `audio_clk_o` | out | 1 | the AUDIO.FIFO output domain |
| `pll_locked_o` | out | 1 | asynchronous; all four outputs are valid while high |
| `pll_lock_lost_o` | out | `LOST_W` | census, `ref_clk_i` domain |

**Deviation from the ledger, declared rather than silently taken.**
`design/blocks.yml` lists SYS.PLL's inputs as `[ref_clk, pll_config]`.
`pll_config` is **not a runtime port here**: a Cyclone V PLL's counters are set
at configuration time, and a runtime-reconfigurable PLL means instantiating
`altera_pll_reconfig` with an Avalon-MM slave, a reconfiguration state machine
and a second `locked` transient per change. Nothing in the tree asks for one —
the video mode latch (`spec/video_rules.md` §1.1) changes a *timing table*, not
a frequency, and the audio rate is fixed at 48 kHz. So **`pll_config` is the
parameter block below**, resolved at elaboration. If a mode ever needs a
per-mode pixel frequency (see *the unsettled ratio*), that decision reopens this
line and not any other.

## Backpressure rules

None. There is no handshake on a clock.

## Memory ownership

None. No RAM, no ROM, no register file. The block's entire state is the lock
synchroniser, the simulation model's divider counters and the lock-loss census.

## Q formats and rounding

None — no arithmetic on data. The one place rounding would matter is frequency
synthesis, and that is the vendor primitive's fractional counter, whose exact
achieved frequency is a **fitter output** (`.qsf`/IP report), not a value this
RTL computes. **Do not read the parameters below as achieved frequencies; they
are requests.** The achieved values arrive with the first fit and belong in a
receipt, not here.

## Latency (fixed or variable)

`fixed:1` in the ledger, and that is the right shape: every output is a clock,
so the only latency is **lock time**, which is a property of the vendor PLL and
of the reference, not of this RTL.

* **Real hardware: UNSETTLED.** Cyclone V PLL lock time is a datasheet figure
  gated by the achieved counter settings, and no fit or board measurement of it
  exists. `board_truth.json` `reset.volatileLoadResetSequenceStatus: proven`
  records that *a* MiSTer core's reset-after-lock sequence worked on this board;
  it records no number.
* **Simulation model: `SIM_LOCK_CYCLES` reference cycles, default 8.** DERIVED
  from nothing physical — it is a deliberately short, editable stand-in so a
  bench can see the lock edge. It is tagged in the RTL as a sim-only knob.

**This is why SYS.RESET is a sequencer and not a delay line.** It waits on
`pll_locked`, an event, rather than on a cycle count nobody can supply.

## Target throughput

`n/a (configuration wrapper)`, as the ledger says. The *frequencies* are the
budget-facing numbers:

| Output | Value | Tag | Where it comes from |
|---|---|---|---|
| `GPU_CLK_HZ` | 100,000,000 | **DERIVED (target)** | `spec/terrain_rules.md` lines 525 and 579 and `spec/sky_and_beams.md` line 162 all cost the frame against a **"1.67 M-cycle frame (100 MHz placeholder — Phase 0 freezes the clock)"**. 100 MHz × 1/60 s = 1,666,667 cycles, which is that figure. SYS.PLL *is* the Phase-0 block that freezes it, so authoring this row is that act. The composed acceptance floor is **105 MHz** (`design/V1-RELEASE-DEFINITION.md`: the shell's 99.34 MHz is called out as *below* it), i.e. 100 MHz is the contract and 105 MHz is the margin the fit must show. |
| `SDRAM_CLK_HZ` | 100,000,000 | **DERIVED, corroborated** | Two independent routes agree. (a) `MEM.SDRAM.md` puts the controller in `sdram_clk` with no CDC to the gpu-side arbiter described anywhere, so the simplest correct tree runs them at one frequency. (b) `zhao_sdram_params_pkg.sv` sets `REFRESH_INTERVAL = 780` cycles and `spec/memory_rules.md` line 26 glosses it as *"8192 rows / 64 ms at the conservative clock"*; 64 ms / 8192 = 7.8125 µs, and 780 / 7.8125 µs = **99.84 MHz**. The agreement is corroboration, **not a measurement** — the params package states no frequency and its own header calls its numbers "provisional sim constants, NOT board truth". |
| `VIDEO_CLK_HZ` | **0 = UNSETTLED** | **UNSETTLED** | See the section below. The parameter default of `0` is a deliberate "not frozen" sentinel, not a frequency. |
| `AUDIO_CLK_HZ` | 12,288,000 | **DERIVED, and it flags a contract conflict** | `spec/audio_rules.md` §1 ratifies **48,000 Hz, exactly 800 stereo pairs per displayed frame (48000/60)** — MEASURED-equivalent, a ratified spec number. `AUDIO.FIFO.md` then describes the output side as running in "`audio_clk` (48 kHz)". **A Cyclone V PLL cannot emit 48 kHz** (its output counters bottom out in the MHz). So the physical audio domain clock is 256 × 48 kHz = 12.288 MHz, the standard I²S master rate, and the 48 kHz *sample cadence* is a clock enable inside that domain. **This reading is recorded here as an OPEN QUESTION for the AUDIO.FIFO owner**, because it is the one place this contract's arithmetic touches another contract's surface. It changes no audio number: 800 pairs per frame stands either way. |

### The unsettled ratio: `gpu_clk` to `video_clk`

This is the one number this contract cannot supply, and the reason is worth
stating precisely rather than hiding behind a default.

**What the ratified documents say.**

* `spec/video_rules.md` preamble: *"`vid_clk = gpu_clk / 2` (one vid cycle = 2
  gpu cycles) **in simulation and on the frozen sim profile**; PLL ratios are
  re-derived from board truth post-ZH-016 **WITHOUT changing any contract
  surface here**."*
* `fpga/rtl/common/zhao_pkg.sv`: `ZHAO_VID_CYCLES_PER_GPU = 2`, commented
  *"frozen sim profile; PLL ratios are board data and never change these
  contract surfaces"*. `zhao_timing_t.frame_gpu_cycles` is commented
  `v_total * h_total * 2` — it is that product and nothing more.
* `design/blocks.yml`, SYS.PLL's own note: *"Absolute frequencies frozen
  post-Phase-0 (charter §25); **Verilator lane uses a single conceptual
  clock**."*
* `VIDEO.MODE.md`: *"board-derived PLL ratios change numbers post-ZH-016, never
  these contract surfaces."*

So **2 is the SIMULATION ratio, scoped as such in four places, and every one of
them defers the hardware ratio to ZH-016.**

**Why 2 cannot also be the hardware ratio.** The raster runs at 60 Hz for every
mode — `spec/audio_rules.md` §1 pins it, "exactly 800 pairs per frame (48000 /
60 = 800): one displayed frame consumes exactly 800 pairs", mode-independent. At
60 Hz the pixel rate is `h_total × v_total × 60`:

| Mode | `h_total` | `v_total` | pixel rate at 60 Hz | `gpu_clk` at ratio 2 |
|---|---|---|---|---|
| Z60 | 480 | 262 | 7,545,600 Hz | 15.09 MHz |
| Storm | 416 | 262 | 6,539,520 Hz | 13.08 MHz |
| Duo | 608 | 262 | 9,557,760 Hz | 19.12 MHz |

Ratio 2 therefore implies a `gpu_clk` of 13–19 MHz, against a cost model written
at 100 MHz — a factor of 5 to 7. And it implies **three different `gpu_clk`
frequencies**, one per video mode, because `v_total` is constant while `h_total`
is not. `100 MHz ÷ each pixel rate` gives 13.25, 15.29 and 10.46 — **not an
integer in any mode**, so no fixed integer divider serves the tree either.

**Why the answer is not in the tree.** The reading is not "somebody forgot to
write the ratio down"; it is that **ZH-016's precondition is not met.**
`board_truth.json` `openCapabilities` lists **`analog_video`** and
**`external_io_timing`** among the capabilities still open, and
`physicalCapabilities.boardVideoProbe` is `passed_owner_visible_color_bars` —
the board was proven able to *emit* video, with no timing characterised.
The board probe answered the clock **inputs** (three 50 MHz oscillators, pinned
and measured) and did not answer the video **output** timing. SYS.PLL's and
SYS.RESET's preconditions are met; VIDEO's are not.

**What this contract therefore does.** `VIDEO_CLK_HZ` defaults to `0`, meaning
*not frozen*, and the simulation clock tree is parameterised by an explicit
divider `SIM_VIDEO_DIV`, defaulting to **2 so that every existing bench and
`ZHAO_VID_CYCLES_PER_GPU` continue to describe the same machine**. The value is
a knob; nothing downstream reads a hard-coded 2 from this block.

**The AUDIO side of the same sim profile IS settled, and it was not invented
here.** `fpga/rtl/common/zhao_shell_top.sv` — the protected shell, pinned by
SHA-256 in `.gitattributes` — its v2 successor and `zhao_console_core.sv` all
carry the identical port comment: *"frozen ratios: vid = gpu/2, **audio =
gpu/4**, fixed phase — plan R1"*. `SIM_AUDIO_DIV` therefore defaults to **4**.
The first draft of this contract said 8, invented, and the answer was three
files away. Recorded because it is this repository's own law about checking the
confident one-line summary, caught on the author's own number.

**What has to happen to close it.** One of three, and it is VIDEO's call and not
this block's:

1. **A pixel clock enable, not a pixel clock.** Run `video_clk` at a single
   frozen frequency and emit one pixel per N cycles with a per-mode `ce_pixel`.
   This is the MiSTer framework's own idiom (`CLK_VIDEO` + `CE_PIXEL` into
   `video_mixer`/`ascal`) and it costs one PLL output for all three modes. The
   repository contains **no** `CLK_VIDEO` or `CE_PIXEL` identifier today, so
   adopting it is a decision, not a discovery.
2. **Three PLL outputs, one per mode**, muxed at frame start with the mode
   latch. Cheap in PLL outputs, expensive in clock muxing and constraints, and
   it makes `video_clk` a glitch hazard at every mode switch.
3. **Runtime PLL reconfiguration** — `altera_pll_reconfig`. This is the option
   that would make `pll_config` a real runtime port, and it is the most
   expensive.

Until one is chosen, this block emits a parameterised `video_clk` and says so.

## Overflow and malformed-input behaviour

* `pll_lock_lost_o` is a **saturating** census of width `LOST_W` (default 16).
  It does not wrap. A wrapping census can read zero after 65,536 losses, which
  is the flattering direction and the one this repository refuses.
* A reference that stops (oscillator dead, pin unbonded) is **not detectable
  here** and no port claims otherwise: with `ref_clk_i` stopped, every register
  in this block stops with it. Reference loss detection, if it is ever wanted,
  belongs in a block clocked by a *different* reference. Stated because its
  absence must not be mistaken for coverage.
* `pll_arst_i` asserted forever ⇒ `pll_locked_o` low forever ⇒ SYS.RESET holds
  every domain in reset forever. That is the correct behaviour and it is the
  safe direction.

## Counters and traces

`pll_lock_lost_o` (ledger `counters: [pll_lock_lost]`), `ref_clk_i` domain,
`arst_n_i` reset, saturating at `2**LOST_W - 1`.

**Definition:** it increments on each **falling edge of `pll_locked` as observed
through the block's own two-flop synchroniser**. It counts *losses*, so the
power-on interval before the first lock is not a loss and is not counted.

**How it is fired** (the positive control, because a detector reading zero is a
claim): assert `pll_arst_i` after lock. `pll_locked_o` falls, the synchroniser
carries the fall into `ref_clk_i`, the counter increments once. Release
`pll_arst_i` and it re-locks. The directed bench does exactly this three times
and asserts the delta each time.

**Why the two operands cannot move together** (CLAUDE.md, *a detector wired to
two operands that move together cannot fire*): the comparison is between two
adjacent stages of one shift register, one `ref_clk_i` cycle apart by
construction. It is an edge detector, not a value comparison, and the census
register it writes is reset by `arst_n_i` — a *different* reset from the one
that causes the event.

No trace port. Nothing here is per-transaction.

## Scalar reference function

**None, and this is deliberate.** There is no `zref::` oracle for a clock: the
observable is a waveform and its reference is the vendor datasheet plus the
fitter's report, neither of which is a C++ function this repository can call.

Writing one would be the failure `GEOM.LIGHT.md` line 118 names — a second
implementation of an arithmetic that already has an owner — with the extra
problem that the owner is Intel.

What *is* checkable in simulation, and is checked: the **ratios** of the
simulation clock tree, the lock protocol, and the census. Those are properties
of this RTL. The frequencies are properties of the fit.

## Directed tests

`tests/platform/sys_pll_directed.cpp`, verilated as `sys_pll_directed`.

1. **No output toggles while `pll_arst_i` is asserted**, and `pll_locked_o` is
   low. (A PLL that emits clocks before lock is a real and common defect.)
2. **Lock rises `SIM_LOCK_CYCLES` reference cycles after release**, exactly —
   not "eventually".
3. **The divider tree holds its declared ratios** over a long run: `gpu_clk` and
   `sdram_clk` edge-for-edge with the reference at `SIM_*_DIV = 1`,
   `video_clk` at exactly half the `gpu_clk` edge count (the
   `ZHAO_VID_CYCLES_PER_GPU = 2` surface), `audio_clk` at exactly a quarter of `gpu_clk` (the `audio = gpu/4` half of the
   same frozen sim profile).
   Counted as edges over hundreds of cycles, so a one-cycle phase error shows.
4. **`video_clk`'s duty cycle is 50 %** — a divider that emits a one-cycle-wide
   pulse per period passes an edge count and is not a clock.
5. **`pll_lock_lost_o` moves as a delta**, once per induced loss, three times,
   and is stable across the intervals between.
6. **The census survives the event that causes it**: after three losses the
   counter reads 3, having never been cleared by the reset that produced them.
7. **`arst_n_i` clears the census and `pll_arst_i` does not** — the two resets
   are shown to be different resets, not the same one spelled twice.
8. **Parameter sensitivity**: a second elaboration at `SIM_VIDEO_DIV = 4`
   changes the ratio, proving the knob is live and that 2 is not welded in.

## Randomized differential tests

**None, and the reason is a boundary rather than an omission.** A differential
test needs two implementations of one function; the second implementation of a
PLL is the silicon, which this lane cannot run (`board_truth.json`
`reviewHold.futurePhysicalLoadsAuthorized: false`, seven prerequisites listed).
Randomising the divisor parameters would exercise the *model*, not the block's
risk, and the block's risk is entirely in the vendor instantiation.

Re-open this section when the review hold lifts and a board measurement of the
achieved frequencies exists. That measurement is the differential.

## Formal properties

`tests/formal/sys_pll_lock.sby` (ledger `tests.formal`). **NOT WRITTEN BY THIS
PASS**, and named here as the obligation it is:

* `lock_monotone_within_arst` — `pll_locked_o` does not glitch: between one
  assertion of `pll_arst_i` and the next it rises at most once.
* `no_clock_before_lock` — no output clock edge while `pll_arst_i` is high.
* `census_saturates` — `pll_lock_lost_o` never wraps.

The first two are properties of the **simulation model**, not of the vendor
primitive, and a green run must be read that way.

## Synthesis / resource ceiling

**The vendor PLL is IP, and Verilator cannot elaborate it.** How that is
handled, stated plainly because it is the largest unverified surface in this
block:

```
`ifdef ZHAO_SYS_PLL_VENDOR
    altera_pll #(...) u_pll (...);     // Quartus only. NEVER linted, NEVER simulated.
`else
    <behavioural divider + lock model>  // Verilator and every bench in this repo.
`endif
```

* The selector is a **plain `` `ifdef ``** choosing between two definitions, not
  a function-like `` `define `` overridden from the command line. CLAUDE.md
  records that Verilator's `-D` cannot override a function-like macro **and says
  nothing when it fails to**, which produced two mutants that measured
  unmutated production. This shape is the one `-D` reaches.
* **`ZHAO_SYS_PLL_VENDOR` is never defined in any Verilator lane**, so every
  number this repository can produce about SYS.PLL describes the *model*.
* **The real IP is UNVERIFIED here.** Not lint-checked, not simulated, not
  synthesised, not fitted, not loaded. It has never been through `quartus_map`,
  and CLAUDE.md's rule applies without softening: *a block that has never been
  through `quartus_map` has not been shown to be synthesizable, however clean
  its lint.* The `altera_pll` parameter names and the achieved frequencies are
  the first thing the first fit will contradict.
* The behavioural branch is **synthesizable** (counters and flops only), so a
  `quartus_map` of the default configuration is a real gate for everything
  except the IP itself.

**Resource ceiling.** A Cyclone V PLL is a hard block: the expected cost is
**1 PLL, 0 DSP, 0 M10K**, plus a few dozen ALMs for the lock synchroniser, the
census and the model's counters (the model's counters vanish under
`ZHAO_SYS_PLL_VENDOR`). The device has 6 PLLs in the FPGA portion; one output
group is well inside that. **No fit row exists. This paragraph is an
expectation, not a measurement**, and the campaign's fit-at-completion rule is
why.

## Integration capture cases

None in Phase 2 — a `.zcap` records frames and packets, and a clock is neither.

The capture that *would* settle this block is a board capture, and it is
**blocked by an explicit review hold**: `board_truth.json`
`reviewHold.futurePhysicalLoadsAuthorized: false`, with seven named
prerequisites including a repaired build-copy `sys_top`, zero Critical Warnings,
a pinned SSH host fingerprint and a complete hash manifest. **Do not load,
flash, or SSH to the board to close this section.**

## Notes

Absolute frequencies frozen post-Phase-0 (charter §25); the Verilator lane uses
a single conceptual clock, which is exactly what `SIM_GPU_DIV = SIM_SDRAM_DIV =
1` and `SIM_VIDEO_DIV = 2` mean in the model.

**What a reader should distrust first, in order:**

1. `VIDEO_CLK_HZ`, because it is `0` and a zero is a claim.
2. The `altera_pll` instantiation, because nothing in this repository has
   compiled it.
3. `AUDIO_CLK_HZ`, because it resolves a conflict between two contracts by
   reading one of them as a sample rate rather than a clock rate, and the AUDIO
   owner has not agreed.
4. `SDRAM_CLK_HZ`, because the 99.84 MHz corroboration comes from a parameter
   package whose own header says its numbers are provisional.
