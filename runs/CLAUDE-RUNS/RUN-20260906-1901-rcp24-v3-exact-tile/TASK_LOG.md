# Task Log: RUN-20260906-1901 - [Describe objective here]

**Created:** 2026-09-06 19:01 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-1901-rcp24-v3-exact-tile/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 19:01 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-1901
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## Objective

TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt section 0 point I: rebuild
`zhao_raster_rcp24_svc` as a STANDALONE tile (S26.1 — do not merge into the
composition) with

  1. its free/ready/done SCANS replaced by queues and bounded contexts (S10.2);
  2. its 32-by-64 multiply replaced by an exact 32x32 product plus a signed-wrap
     high-word correction (S10.5), negative case included.

## What is measured before anything is built

`zhao_raster_rcp24_svc@v3-before`, MapOnly, source digest e11495c17546:
1065 registers, 0 memory bits, 6 DSP, 177 virtual pins.
Committed full fits of the same block: 1041 ALM / 1101 reg / 6 DSP / 68.46 MHz
(seed sweep: 1037/1104/63.93 and 1038/1105/68.63).

## THE FINDING THAT SHAPED THE TEST

The negative high-word correction is UNREACHABLE from the reciprocal's own
inputs. Walking all 16,777,215 nonzero denominators through both Newton steps
with the committed T24 table:

    max w over the whole domain = 0x401F_EF88 = 1,075,834,760
    2^31                        = 0x8000_0000 = 2,147,483,648
    phases with w > 2^31        = 0

So a denominator-driven paired test PASSES WITH THE CORRECTION DELETED. The
arithmetic core is therefore its own module, `zhao_raster_rcp24_mul`, exposed on
its own port in the testbench, and the differential drives it over the same
(x, w) families `tools/rtl/architecture_numeric_checks.py` drives. Fire test F1
deletes the correction and must fail; if it did not, the whole reduction would
be untested.

## Multiplier sites, counted before blaming any tool

    4  fpga/rtl/raster/zhao_raster_rcp24_mul.sv   (four 16x16 unsigned)
    0  fpga/rtl/raster/zhao_raster_rcp24_v3.sv
    0  fpga/rtl/raster/zhao_raster_ticketq.sv
    0  fpga/rtl/field/zhao_field_rcp24_rom.sv
    ---
    1  fpga/rtl/raster/zhao_raster_rcp24_svc.sv   (one 32x64) -> measured 6 DSP

CLAUDE.md's combiner incident is the reason this count exists as a number rather
than as an assumption. `multstyle` is not invoked anywhere here; the four sites
are meant to occupy DSPs, and S10.7 names two blocks as the mapping target.

## Model check before the RTL differential

The bit slices actually written into `zhao_raster_rcp24_mul.sv` were re-expressed
in Python and compared against `mx_original` from the owner's script over the
same boundary cross-product plus 200,000 random pairs:

    MX cases 200256, negative-correction 99483, mismatch 0
    MW cases 200000, mismatch 0

This is not the RTL simulation -- it is a cheap check that the SLICES are right
before spending a build, so that a failing differential points at wiring rather
than at the identity.

## Build lane

`cmake --preset windows-native` into `build/` FAILED: another session owns that
tree and cmake hit "Error copying file ... Permission denied" on two verilate
copy steps. Isolated into `build-rcp24v3/` rather than racing it, per the
serial-agent rule.

## Physical measurements, MapOnly, same tool path both sides

| row | digest | registers | mem bits | DSP | vpins |
| --- | --- | ---: | ---: | ---: | ---: |
| `zhao_raster_rcp24_svc@v3-before` | e11495c17546 | 1065 | 0 | **6** | 177 |
| `zhao_raster_rcp24_v3@v3-after` (NCTX=16) | fa1354165244 | 1940 | 2582 | **3** | 276 |
| `zhao_raster_rcp24_v3@v3-nctx8` (NCTX=8) | fa1354165244 | 1414 | 1366 | **3** | 276 |

The NCTX=8 point exists to attribute the register growth and to prove the
parameter took at all: QUARTUS_GOTCHAS 3 says this tool accepts directives and
silently ignores them, and identical rows would have meant the override was
dropped. Registers and memory bits both halve exactly, so it took.

  +349 registers  from the deeper pipeline, at the SAME eight contexts;
  +526 registers  from doubling the contexts to sixteen.

## THE DSP ANSWER, from the tool's own multiplier census

    BEFORE, one `*` in source (32-by-64):
      Two Independent 18x18            2
      Sum of two 18x18                 1
      Independent 27x27                3
      Total number of DSP blocks       6
      Fixed Point Unsigned Multiplier  7
      Fixed Point Dedicated Output Adder Chain 1

    AFTER, four `*` in source (16x16 each):
      Two Independent 18x18            2
      Sum of two 18x18                 1
      Total number of DSP blocks       3
      Fixed Point Unsigned Multiplier  4

The old block wrote ONE multiply and Quartus built SEVEN multipliers out of it.
The new one writes four and gets four. `multstyle` is not used anywhere here and
was never the question; the site count was, in both directions.

S10.7's stated mapping target was TWO blocks via the two-18-bit mode. Measured
THREE. That is a miss against the target and is reported as one; it is still
inside the block target's `max_dsp: 4` and half the old cost.

## Two things the exact-tool run found that no test would have

1. `zhao_raster_rcp24_mul:u_mul|altshift_taps:o_corr_q_rtl_0` -- Quartus turned
   the correction operand's five-stage delay into a SHIFT-REGISTER MEGAFUNCTION
   backed by memory. Carrying a 32-bit value through O/M/X/L/H with no
   intermediate use is a delay line, and the tool spends RAM on it.
2. `altsyncram:p_x0_q_rtl_0` AND `p_x0_q_rtl_1` -- the seed plane inferred
   TWICE, 512 bits each, because it is read at two places: at R for phase MW0
   and again at W to initialise the scratch row. That second read is a design
   choice made to avoid carrying the seed down the pipeline, and it cost a
   duplicated memory instead.

Neither is a correctness problem and neither is fixed in this pass -- moving the
correction subtract out of stage C would contradict S10.8, which names C as the
stage that does it. Both are recorded as measured levers with their cost.

## Provenance of the artefact I am matching

    tools/rtl/architecture_numeric_checks.py
      sha256 1b7c0b69ef512ee2e4c69a8efd4abf3d1322265ffcce18f4ab291334593b6cbb
      Appendix C says the same. The script in the tree IS the one the document
      describes.

    tools/rtl/numeric_check_results.json
      sha256 as-is        a8f7a9a142c2a8c0bfd541ef1ca7cf34622e78d31f7e7cc4f83661de43689e9d
      sha256 LF-normalised bfa864a10636ade67b8c52c17abdf5124ddc550d2f8796bd8853deda098591aa
      Appendix C gives bfa864a1... The local file reproduces the documented
      result EXACTLY; the difference is CRLF, because `Path.write_text` uses the
      platform newline and this ran on Windows. Worth knowing before somebody
      reports the result hash as a mismatch.

## The differential, first clean run

    B.1 boundary MX: 256 checked, 48 negative-correction
    B.2 random MX: 250000 checked, 124848 negative-correction (49.94%)
    B.3 random MW: 250000 checked
    A.0 stimulus covers 24 distinct exponents k
    A.1 ready 1-in-1: 4104 answered, 16582 clocks (4.04 per reciprocal)
    A.2 ready 1-in-3: 4104 answered, 16829 clocks (4.10 per reciprocal)
    A.3 ready 1-in-7: 4104 answered, 29550 clocks (7.20 per reciprocal)
    T   saturated: 256 reciprocals in 1040 clocks (4.06 each)
    T   serial reference: 28556 clocks for 4104 (6.96 each)
    [raster_rcp24_v3_directed] 52 checks passed

The owner's script counts 124,679 negative-correction cases over 250,256 pairs.
This drives 124,896 of them through actual RTL over 250,256 pairs, from an
independent 64-bit LCG read only at its top 32 bits. The two counts are not
meant to be equal; they are meant to be the same fraction of the same families,
and they are (49.82% against 49.94%).

## THE FIRE TESTS FIRED THE WRONG WAY FIRST

The first fire-test pass returned six BUILD-FAILED rows. The recorded text was

    verilated.cpp:78:10: warning: 'STDOUT_FILENO' redefined

-- a WARNING that every build in this tree emits. `rebuild.ps1` had
`2>&1 | Out-Null` under `$ErrorActionPreference = 'Stop'`, and Windows
PowerShell 5.1 wraps a native executable's redirected stderr in a
NativeCommandError, which `Stop` then treated as fatal. Six mutants were never
compiled and never run.

They were NOT scored as caught. The harness rule "a mutant that does not compile
is NOT a caught mutant" is the only reason this did not become six lines of
false evidence, and it is exactly the case CLAUDE.md's broken-instrument law
describes: the defect made the result look better than the truth, and nobody
audits good news.

Fixed by dropping the redirect and using `Continue`. Re-run below.

## FIRE TESTS: 6 mutations, 6 FIRED, with the exact text

RTL restored byte-exact afterwards -- all four files hash identically to
`reports/synthesis/blockpaths/zhao_raster_rcp24_v3@v3-full.sources.sha256`, the
manifest the running full fit snapshotted. Tests and fit refer to the same bytes.

**F1 drop the negative correction** -- `c_hi_q <= h_high_q`. 4/52 failed:

    first mismatch a=0x00000001 b=0xFFFFFFFF got=0x00000000FFFFFFFF want=0xFFFFFFFFFFFFFFFF
    FAIL: boundary MX: corrected P64 matches the uint64 reference: expected 0x0, got 0x2D
    FAIL: boundary MX: the 32-bit iterate matches the uint64 reference: expected 0x0, got 0x2A
    first mismatch a=0xFB0D05C7 b=0xAA2165CC got=0xA6D76658164F1D94 want=0xABCA6091164F1D94
    FAIL: random MX: corrected P64 matches the uint64 reference: expected 0x0, got 0x1E7B0
    FAIL: random MX: the 32-bit iterate matches the uint64 reference: expected 0x0, got 0x1E7B0

THE POINT OF THE WHOLE PASS IS IN THIS ROW. 0x1E7B0 = 124,848, the exact
negative-correction count -- and EVERY PHASE A CHECK STILL PASSED. The paired
denominator differential, the one the V2 block already had, does not notice that
S10.5's correction has been deleted. Only the direct (x, w) drive does.

**F2 drop the rounding carry** -- 5/52 failed, in BOTH phases:

    FAIL: boundary MX: the 32-bit iterate matches the uint64 reference: expected 0x0, got 0x73
    FAIL: random MX: the 32-bit iterate matches the uint64 reference: expected 0x0, got 0x1E8FD
    FAIL: every V3 answer is BIT-IDENTICAL to the serial block's: expected 0x0, got 0x13

**F3 MW extraction shifted one place** -- 7/52 failed:

    FAIL: random MW: w = P[55:24] matches (m*x) >> 24: expected 0x0, got 0x3D090
    FAIL: every V3 answer is BIT-IDENTICAL to the serial block's: expected 0x0, got 0xFDD
    FAIL: no denominator reaches the negative correction (max w = 0x401FEF88): expected 0x0, got 0xBE8

**F4 drop the low-to-high carry** -- 9/52 failed:

    first mismatch a=0x0000FFFF b=0x7FFFFFFF got=0x00007FFE7FFF0001 want=0x00007FFF7FFF0001
    FAIL: random MW: the tiled 32x32 product is EXACT: expected 0x0, got 0xF443

**F5 MW0 reads stale scratch instead of the seed** -- 6/52 failed:

    FAIL: every V3 answer is BIT-IDENTICAL to the serial block's: expected 0x0, got 0xFDD
    FAIL: no denominator reaches the negative correction (max w = 0x401FEF88): expected 0x0, got 0x96A

**F6 skip the second Newton MW** -- 7/52 failed:

    FAIL: exactly four product launches per NONZERO reciprocal: expected 0x3F74, got 0x2F97
    FAIL: and not under four, which would mean a launch was skipped: expected 0x28, got 0x1E

## The zero that is not a broken instrument

`negcorr_jobs_o == 0` is an assertion that a counter reads EXACTLY ZERO, which
CLAUDE.md says to distrust on sight. F3 and F5 both drove it nonzero (0xBE8 and
0x96A) by corrupting w, so it is a live detector that happens to read zero and
not a detector that cannot fire. The measured claim "no denominator reaches
w > 2^31" is now carried by an assertion that has been SEEN to fail.

Likewise F6 proves the four-launches-per-reciprocal counter contract is live:
16,244 expected against 12,183 measured, exactly three phases per reciprocal.

## What was built

    fpga/rtl/raster/zhao_raster_rcp24_mul.sv    the S10.5 identity, six stages,
                                                FOUR 16x16 multiplier sites
    fpga/rtl/raster/zhao_raster_ticketq.sv      the flop FIFO that replaces the
                                                three scans, PRELOAD for the free list
    fpga/rtl/raster/zhao_raster_rcp24_v3.sv     the tile: 16 contexts, free/NEW/
                                                CONT/DONE queues, 10-clock loop
    tests/raster/tb_rcp24_v3_pair.sv            serial oracle + tile + the mul
                                                unit on its own port
    tests/raster/raster_rcp24_v3_directed.cpp   the differential
    tests/CMakeLists.txt                        target, exhaustive nightly, and
                                                three lint gates

V3-DIAGNOSIS-VERIFICATION-20260906.md S2.10 counted the thing being replaced:
three scans at `rcp24_svc.sv:141` (free), `:159` (ready), `:212` (done), and
exactly one multiplier site at `:268`, a 32x64. All four are gone.

## Deliberate deviations from S10, each with its reason

* **The payload/scratch planes are SystemVerilog arrays, not explicit RAM
  primitives.** S21.2 budgets four M10Ks for them. Quartus inferred five
  altsyncrams from the arrays on its own; forcing the shape belongs to the
  composition pass, where the record contract is fixed.
* **Normalisation is still the serial block's 24-bit priority scan.** S10.3
  prefers a balanced leading-zero network but requires the same zero/nonzero
  convention exactly. Transcribing the existing loop makes the convention
  identical by construction; replacing it is a separate, measured change.
* **The final result sits in a local 16-entry array plus a DONE queue** rather
  than writing the owner's RCP_RESULT and publishing a sample-owner ticket.
  S10.2 asks for the latter, and S26.1 forbids adopting the shared record and
  credit contracts in this tile. S10.2's own rule applies when it composes: "If
  an implementation keeps a local output buffer for modularity, its credits and
  RAM blocks must be counted." They are counted here: 16 x 24 bits.
* **The negative-correction flag is not carried in the multiplier tag.** It is
  counted at operand selection instead, because a bit that only a counter reads
  is dead logic in a block whose whole point is area.

## EXHAUSTIVE: all 2^24 denominators, which S10.9 says had never been run

    E   exhaustive: 16777215/16777215 answered in 67108876 clocks (4.00 each),
        peak occupancy 16
    [raster_rcp24_v3_directed] 56 checks passed          224 s

Every one of the 16,777,215 nonzero denominators is bit-exact against
`zref::rcp_u24` -- r, k and the zero flag. 67,108,876 / 16,777,215 = 4.0000006,
so the multiplier launched its four jobs per reciprocal and idled for six clocks
in sixteen million. S10.9: "This review did not run that sweep against the
compiled repository reference or the new RTL." It has now been run against the
new RTL. Registered as `raster_rcp24_v3_exhaustive`, nightly.

## SIXTEEN CONTEXTS IS MEASURED, NOT ASSUMED

S10.2 asks for the context count to "satisfy the actual feedback latency, not a
comment saying that three contexts ought to be enough. Measure product launches
per cycle under saturated independent work." Same RTL, `-GNCTX=8`:

    NCTX=16   saturated 256 reciprocals in 1040 clocks (4.06 each)
    NCTX=8    saturated 256 reciprocals in 1447 clocks (5.65 each)

    FAIL: a saturated V3 tile costs under 4.6 clocks per reciprocal:
          expected 0x2E, got 0x38

Every correctness check still passed at NCTX=8; only throughput moved. Eight
contexts offer 8 tickets per ten-clock loop = 0.8 per clock, below the one
launch per clock the multiplier can take, so the predicted 4 x 10/8 = 5 clocks
plus fill and drain is the 5.65 measured. Sixteen saturates; eight does not.

The price of sixteen: +526 registers and +1216 memory bits over eight.
The price of eight: 39% less throughput.

This doubles as a SEVENTH fire test, and a better one than a mutation, because
the configuration is legitimate rather than broken: the throughput gate is a
live detector that a real design choice can trip.

## The BEFORE block's worst path is the cone S22.1 names

`reports/synthesis/blockpaths/zhao_raster_rcp24_svc.setup.rpt` Path #1:

    From Node          m1_i_q[1]
    To Node            r_o[22]
    Data Arrival Time  14.547
    Slack              -4.607 (VIOLATED)

`m1_i_q` is the in-flight context index and `r_o` is a continuous assign that
walks the DONE SCAN, indexes `c_x[done_i]`, adds 64, shifts by 7 and clamps --
S22.1's "context-array output -> variable rounding add -> variable shift ->
saturation", all in one clock. 14.547 ns of it.

In the V3 tile `r_o` is `res_q[done_dout]`: a registered 24-bit mantissa
selected by a queue head. The rounding add, the shift and the clamp happen
between the product unit's E register and `t1_r_q`. Whether that buys the
expected slack is the full fit's answer, not this paragraph's.

The composed-fit corroboration from reports/V3-DIAGNOSIS-VERIFICATION-20260906.md
is that RCP24 owns 18 of the island's 189 internal paths and that
`live_r[6] -> c_m.raddr_a[*]` recurs among the worst -- the scan structure
again. NO CLAIM IS MADE HERE ABOUT WHAT THIS DOES TO THE ISLAND'S FMAX: the
composed baseline is optimistic for unrelated reasons (AUX's hardcoded 0..65536
envelope constant-folds away that block's own -8.199 ns / 54.95 MHz path), so
subtracting RCP's contribution from it would be arithmetic on a number that is
not measuring what it appears to.

## THE design/fit_targets.yml ENTRY I WANT (not edited by this lane)

Place it beside the existing `zhao_raster_rcp24_svc` entry. Note that entry's
`rules:` block currently sits AFTER a `# --- terrain ---` section comment, which
reads as though the rules belong to the terrain group; they do not, and it is
worth straightening while somebody is in the file.

    - top: zhao_raster_rcp24_v3
      sources:
        - fpga/rtl/field/zhao_field_rcp24_rom.sv
        - fpga/rtl/raster/zhao_raster_ticketq.sv
        - fpga/rtl/raster/zhao_raster_rcp24_mul.sv
        - fpga/rtl/raster/zhao_raster_rcp24_v3.sv
      rules:
        # MEASURED 3, at NCTX=16 and again at NCTX=8. The gate is set AT the
        # measurement and not at the svc target's 4: the reduction's whole
        # purpose is the DSP count, and a ceiling with a spare block does not
        # notice a fifth multiplier site appearing. The tool's own census says
        # "Fixed Point Unsigned Multiplier: 4", which is the source count.
        max_dsp: 3
        # S21.2 budgets "RCP local payload77 + scratch64, each 16 deep" as RAM.
        # min_memory_bits, not min_m10k -- fit_rules.ps1's own comment says an
        # M10K COUNT is not a sound floor and that it cost two false failures on
        # 2026-09-04. MEASURED 2582 bits; the floor is set below that so the two
        # named area levers (the altshift_taps delay line, the duplicated seed
        # plane, ~672 bits between them) can be removed without a false failure,
        # but a collapse back into flops cannot pass.
        min_memory_bits: 1800
        max_registers: <full fit>
        max_alms: <full fit>
        max_m10k: <full fit>

Do NOT give this target the svc row's `max_alms: 650` / `max_registers: 600`.
Those came from islandrearchitecture5.md's V2 budget, the svc block already
violates both at 1041/1101, and V3 deliberately spends registers to buy the DSP
halving and the scan removal. Inheriting them would report a change of
architecture as a regression.

## The context sweep closes the question S10.8 left open

S10.8: "16 contexts are a defensible starting point, not a measured optimum."
Same RTL, three elaborations, saturated batch of 256:

    NCTX=8     1447 clocks   5.65 per reciprocal   (throughput gate FAILS)
    NCTX=16    1040 clocks   4.06 per reciprocal
    NCTX=32    1042 clocks   4.07 per reciprocal

    exhaustive, NCTX=16, 16,777,215 reciprocals: 4.0000006 per reciprocal

Sixteen is the KNEE, not merely a sufficient number. Thirty-two buys nothing
because the multiplier is already taking one launch every clock and four
launches is what a reciprocal costs; the extra sixteen contexts would be pure
area. Eight is below the ten-clock feedback loop's requirement and leaves the
multiplier idle 29% of the time.

So S10.8's "defensible starting point" is now a measured optimum, and the answer
happens to be the number the document guessed.

## What the clocks mean in frames

At 100 MHz and 60 fps, and taking the 276,480-fragment terrain-primary estimate
the svc header quotes:

    serial zhao_raster_rcp24, measured      6.96 clk   239,464 /frame   BELOW estimate
    V3 tile NCTX=8                          5.65 clk   294,985 /frame
    V3 tile NCTX=16, saturated batch        4.06 clk   410,509 /frame
    V3 tile NCTX=16, exhaustive 2^24        4.00 clk   416,667 /frame

The svc header's own arithmetic -- "4 multiplier jobs per reciprocal, one launch
per clock => one reciprocal every 4 clocks => 416,666/frame at 100 MHz" -- is now
a MEASURED number over the entire denominator domain rather than a design
intention. THAT IS A THROUGHPUT CLAIM AND NOT A TIMING ONE: it assumes the tile
closes at 100 MHz, which is the full fit's question, and it says nothing about
the composed island.

## From the running fitter's log, before its numbers land

    Info (176235): Finished register packing
        Extra Info (176218): Packed 207 registers into blocks of type DSP block
        Extra Info (176220): Created 64 register duplicates
    Info (128003): register retiming complete: estimated slack improvement of 870 ps

207 of the tile's registers were absorbed INTO the DSP blocks -- the O-stage
operand registers and the M-stage product registers sitting in the DSP's own
input and output pipeline. That is the packing S10.7 asked to be checked
("Register, clock-enable, signedness, and mode compatibility must be checked in
the exact Quartus 17 microbenchmark"), and it is happening. It also means the
synthesis register count and the fitter register count are not the same
quantity, which is why the before/after MapOnly pair is compared to itself and
the full fits are compared to each other.

## The BEFORE baseline in full, per S22.3 and S22.5

`zhao_raster_rcp24_svc`, Slow 1100mV 100C, clk constrained at 10.000 ns:

    Fmax           68.46 MHz  (restricted 68.46)
    Setup slack    -4.607     End Point TNS  -1282.133
    Hold slack     -0.817     End Point TNS     -2.235
    Worst path     m1_i_q[1] -> r_o[22], data arrival 14.547 ns

S22.3: "Require setup, hold, recovery/removal where applicable... A reported
Fmax is not complete timing signoff." The HOLD VIOLATION is worth saying out
loud: `zhao_block_fit.json` carries `fmaxMhz` and nothing else about timing, so
a block that fails hold by 0.817 ns has been sitting in the ledger looking like
a 68.46 MHz row. That is not something this pass introduced and not something it
fixes; it is something the before/after comparison has to include or the "after"
number will be compared against half a baseline.

## V20: two prose claims, closed the true way

`npm run ledger:check` flagged `zhao_raster_rcp24_mul.sv:92` and `:155`. They are
different KINDS of claim and got different resolutions, which is the point of
V20 offering three.

**:92 was an ASSUMPTION ON THE CALLER wearing a claim's clothes.** "The pipeline
is nonstallable by construction and the caller reserves its downstream position
before issuing" is not a property this module can hold -- it has no ready port
and cannot refuse anything. V20's third resolution applies: rewritten as an
explicit assumption naming who upholds it. `zhao_raster_rcp24_v3` reserves a
context from its free queue before admission, its queues are sized to the
context count, and `zhao_raster_ticketq.err_o` latches any overflow, surfaced as
`qerr_o` and asserted low on every pass including the 2^24 sweep. No
`ENFORCED-BY:` is added, because pointing prose at prose enforces nothing.

**:155 was a real invariant, and is now a real refusal.** "The high half of an
exact 32x32 product is < 2^32 by construction, so high_sum_c[32] cannot be set."
That is checkable every clock, so it is checked every clock:

    a_high_carry_never_set: assert (high_sum_c[32] == 1'b0)

following `zhao_geom_assetfetch.sv:696`'s existing shape -- a labelled immediate
assertion under `ifndef SYNTHESIS`. `ENFORCED-BY:` names it as
`fpga/rtl/raster/zhao_raster_rcp24_mul.sv:a_high_carry_never_set`, a symbol that
resolves in the file.

First version read `if (rst_n && l_v_q)` and Verilator refused it:

    %Warning-SYNCASYNCNET: Signal flopped as both synchronous and async: 'rst_n'

Correctly -- `rst_n` is this module's ASYNCHRONOUS reset and reading it
synchronously is a reset-domain smell even inside a checker. `l_v_q` is itself
cleared by that async reset, so gating on it alone is the same guard without the
smell.

### F7: the assertion was shown to fire, ALONE

    ('F7-force-high-carry-bit',
      + 33'h1_0000_0000 on high_sum_c)

sets bit 32 while leaving bits [31:0] untouched, so the product and both
extractions stay exactly correct and nothing else can notice:

    [0] %Error: zhao_raster_rcp24_mul.sv:267: Assertion failed in
        TOP.tb_rcp24_v3_pair.u_mul.a_high_carry_never_set: rcp24_mul: the high
        half carried out of 32 bits (p11=00000000 crosshi=00000 carry=0)

ONE failure, and it is the assertion. All 56 value checks passed under this
mutant. A decorative assertion would have let it through silently, which is
exactly the invisible-corruption case the claim was about.

### The fix costs nothing in silicon, measured rather than assumed

    zhao_raster_rcp24_v3@v3-after   digest fa1354165244   1940 reg  2582 bits  3 DSP
    zhao_raster_rcp24_v3@v3-after2  digest 6553c5cef196   1940 reg  2582 bits  3 DSP

Different bytes, identical synthesis. The `ifndef SYNTHESIS` block is free, and
now that is a measurement and not a convention.

    ledger: check OK -- 111 blocks / 40 ops; schemas + V1-V17 + V19-V23 green

## Fit lane note

The first full fit was KILLED mid-placement by the task harness after ~55
minutes and wrote no row; `quartus_fit` was verified gone and no row exists, so
no ALM/Fmax number was invented from it. The relaunch was then stopped
DELIBERATELY, because the V20 edit changed `zhao_raster_rcp24_mul.sv` and a fit
whose source digest does not match the shipped bytes is evidence about a file
nobody has. Relaunched a third time against digest 6553c5cef196.

## HARD BLOCKER: THE MACHINE'S DISK IS FULL

    Get-PSDrive C  ->  UsedGB 951.8   FreeGB 0

Quartus failed with

    Fehler beim Ausfuehren des Programms "quartus_map.exe": Es steht nicht genug
    Speicherplatz auf dem Datentraeger zur Verfuegung.

and the concurrent fire-test re-run died with `tail: write error: No space left
on device`. This almost certainly also explains the FIRST full fit being killed
at ~55 minutes with no row: it was in placement, which is where the fitter's `db`
directory grows.

**It is not this lane's footprint.** Every `zhao-block-fit-*` workspace in TEMP
was 24 MB or less (13 of them cleared, ~100 MB, which did not move the counter
off zero). The repository's entire set of build trees is 3.8 GB:

    build 1.68 | build-verify 0.71 | build-casecheck 0.67 | build-lane 0.35
    build-curve 0.08 | build-fieldv3* 0.18 | others 0.15 | build-rcp24v3-quick ~0

951 GB is consumed somewhere outside the repo. Other lanes' stale build trees
were NOT deleted -- that is destructive to somebody else's work in progress, it
would recover under 2 GB, and a disk at absolute zero for unrelated reasons will
simply refill.

**Consequence: no ALM and no Fmax for the V3 tile.** MapOnly rows carry neither
by construction, and every attempt at a full fit has now been killed by the
harness once and by the disk twice. That number is NOT estimated, NOT inferred
from the MapOnly registers, and NOT carried over from the NCTX=8 point. It is
simply absent, and the trade it would settle -- 875 more registers for 3 fewer
DSPs -- is therefore still open.

## A NEAR MISS WORTH RECORDING

The disk filled DURING a fire-test mutation. `fire_tests.py` restores in a
`finally`, but `shutil.copyfile(path, saved)` had already produced a ZERO-BYTE
backup, and had the failure landed a moment later -- after the mutation write,
before the restore -- the repository would have been left holding a deliberately
broken RTL file with a truncated backup beside it.

It did not, and the reason that is a FACT rather than a hope is the per-file
manifest: the four live files hash identically to
`zhao_raster_rcp24_v3@v3-after2.sources.sha256`, the bytes the tests and the
MapOnly rows actually measured. The stray zero-byte `.firebak` was verified
empty before deletion rather than assumed stale.

A restore-from-copy harness on a full disk is a way to lose source. If this
harness is used again it should verify the backup is non-empty before writing
the mutation.

## DISK FREED, FULL FIT RELAUNCHED -- WHERE I AM BEFORE THE RESULTS COME BACK

The disk-full blocker cleared on its own (another lane released space): C: went
0 -> 19.6 GB -> 165 GB free. The full fit is running again, `quartus_fit` alive,
snapshot digest **6553c5cef196** -- byte-identical to the `@v3-after2` MapOnly row
and to the bytes every test measured. So the ALM/Fmax row, when it lands, will be
about exactly the code that produced the register/DSP numbers.

**Two false starts, both caught before they cost fitter hours:**

1. `-Module` does NOT supply its own source file. The first relaunch declared only
   the three ExtraSources and preflight refused it -- correctly, and with the right
   reason: "Running anyway would spend the fitter's time to report
   'failed:quartus_map', which reads in the report as 'this block does not fit'."
2. `run_block_fit.ps1:612` builds the row name as `"$mod$RowLabel"` -- **no
   separator**. My earlier rows only have their `@` because I passed it inside the
   label. `-RowLabel v3-full` therefore produced `zhao_raster_rcp24_v3v3-full`,
   which would have orphaned the row from its own before/after/nctx8 family. Killed
   the fit three minutes in, deleted the malformed digest file, relaunched with
   `-RowLabel '@v3-full'`. Cheap now, invisible later.

**WHAT I WAS DOING WHEN THE FIT WENT OUT, so the results do not erase it:**

* Open: `design/fit_targets.yml` entry for the tile. Written up but NOT added,
  because `max_alms` / `max_registers` / `max_m10k` need this fit. Do not inherit
  svc's `max_alms: 650` / `max_registers: 600` -- svc itself is at 1041/1101.
* Open: re-running fire tests F1-F6 against the SHIPPED bytes. F1-F6 were proven
  on digest `fa1354165244`, F7 on `6553c5cef196`; the difference is comments plus
  the simulation-only assertion, corroborated by identical synthesis numbers.
  **This is blocked by the fit, not by the disk** -- the mutations edit files
  inside the running fit's closure, which is the live-tree trap. It runs after.
* Corrected: the default run reports **52 checks**, not 56. 56 was the
  `--exhaustive` run. The 52 number is what CI's `raster_rcp24_v3_directed` prints.

**Committed and pushed while the fit runs** (the lane had been entirely
uncommitted for hours -- three untracked RTL files, the tests, and every report
row):

* `6d8459e9` the tile, its tests, and the four MapOnly rows. Staged as a single
  hunk of `tests/CMakeLists.txt` via `git apply --cached`, because that file also
  carries the texture and terrain lanes' uncommitted work and committing it whole
  would have taken theirs.
* `5de89e38` the fire-test probe out of the run folder into `tools/rtl/`, with the
  restore rewritten and the rebuild script's paths derived from `$PSScriptRoot`.

**Noted, not acted on:** my `prod_manifest.yml` declarations were already in HEAD
-- they got swept into another lane's commit (`4c2f7891`) while my RTL stayed
untracked. Three lanes are writing this working tree at once; that is how a file
gets committed by somebody who was not editing it.

**Checked and settled cheaply:** the V20 detector is a labelled IMMEDIATE
assertion, and neither `rebuild_rcp24_v3.ps1` nor the CMake target passes
`--assert` (only 3 targets in the whole suite do). F7 fired it under exactly those
flags, so the detector is live in CI as registered. No change needed.

### Check counts settled, and both now measured on the SHIPPED bytes

    default      52 checks passed
    --exhaustive 56 checks passed

Both numbers are real and neither replaces the other: the exhaustive run adds the
2^24 sweep and its companions on top of the default 52. Earlier reporting quoted
56 without saying which run produced it, which is how a number starts drifting.

Re-run just now against the binary built from the CURRENT tree -- digest
`6553c5cef196`, the bytes that are committed -- so the headline result is no
longer inherited from the pre-V20 build:

    E  exhaustive: 16777215/16777215 answered in 67108876 clocks (4.00 each),
       peak occupancy 16

identical to the original sweep, and the throughput and negative-correction
counts (124,848 in B.2, 48 in B.1) reproduce exactly. What is still outstanding
is the FIRE tests F1-F6 on these bytes; those mutate files inside the running
fit's closure and wait for it to finish.
