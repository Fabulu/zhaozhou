# The Field IR engine — build report

> Everything here is **simulation**. No part of this has run on a physical
> board. Every number below is reproducible from a committed test in this repo.

The Field IR is the small instruction set that terrain, deformation, particle
flow, formation and stamp programs are written in. One C++ interpreter is the
law (`reference/src/zfield/zfield_interpret.cpp`); `spec/form/field-ir.md` §1
pins op semantics to exactly two implementations, that interpreter and the
TypeScript one, and forbids re-deriving them from prose anywhere else — the RTL
included.

This report tracks the hardware engine that runs those programs. It is built one
op family at a time: RTL, then a differential test against the interpreter, then
a mutation sweep that tries to break the test.

## Why every result carries a mutation score

A differential test that passes proves nothing on its own — it may simply not
look where the bug is. So each piece is finished by deliberately breaking the
RTL in ways a real implementation plausibly would, and checking the test
notices. A mutation the test does *not* catch is either a hole to close or an
equivalent mutant, and equivalent mutants are recorded explicitly so they do not
read as holes later.

**The sweep verifies its own builds.** Verilator in this tree elaborates at
configure time and will serve a cached model after the source changes — observed
here running an old model against a new source, including once against a
*pristine* source. Any sweep that trusts an incremental build can therefore
report a perfect score for a test that never ran. Every iteration rebuilds the
target from scratch and hashes the executable before and after; a result whose
hash did not move is **discarded, never scored**.

## What is built

| Piece | Ops | RTL | Directed | Random (fast lane) | Mutations |
| --- | --- | --- | ---: | ---: | ---: |
| Arithmetic core | 15 opcodes, `0x00`–`0x0C`, `0x10`–`0x11` | `zhao_field_alu.sv` | 1,005 | 140,000 | **31 / 34**, 3 equivalent |
| Reciprocal | `RCP` | `zhao_field_rcp.sv` + generated ROM | 329 | 60,000 | **23 / 23** |
| Sine / cosine | `SIN`, `COS` | `zhao_field_sin.sv` + generated ROM | 20 | exhaustive | **20 / 20** |
| Length / distance | `LEN2`, `LEN3`, `DIST2` | `zhao_field_len.sv`, `zhao_field_isqrt.sv` | 159 | 9,000 | **21 / 21** |
| Normalise | `NORMALIZE2`, `NORMALIZE3` | `zhao_field_normalize.sv` + generated ROM | 419 | 13,522 | **7 / 7** |
| Table ops | `CURVE`, `DCURVE`, `SPLINE` | `zhao_field_curve.sv` | 11,863 | 21,000 | **18 / 18** |
| Lattice noise | `NOISE2`, `RIDGE` | `zhao_field_noise.sv` | 346 | 12,000 | **15 / 17**, 2 equivalent |
| Rotation | `ROT2`, `ROT3` | `zhao_field_rot.sv` | 3,495 | 15,000 | **17 / 17** |
| Band | `RING` | `zhao_field_ring.sv` | 572 | 24,000 | **17 / 18**, 1 equivalent |
| Shared engine | all 31 opcodes | `zhao_field_exec_shared.sv`, `zhao_field_mul.sv` | 1,127 | 12,906 | see below |
| Program cache | resident directory | `zhao_field_progcache.sv` | 124 | random lane | **3 / 4**, 1 gap CLOSED |
| Earth sinks | `WRITE.MATERIAL`, `WRITE.NAV`, `WRITE.HAZARD` | `zhao_field_sinks.sv` | 21,345 | 4,000 streams | **11 / 11** |

## 2026-08-23: ten calculators became one engine

Every row above was built as an independent block with its own multiplier, and
that was the right way to build them -- each one was verified against the
interpreter before it was wired to anything. It was the wrong way to SHIP them.

`zhao_field_seq` retires **one instruction at a time**, so nine of the ten units
were idle at every instant while holding silicon. The first synthesis ever run
on this subsystem measured **79 DSP blocks of a 112-block device** -- 71% of the
chip for one subsystem.

They now share `zhao_field_exec_shared`: one signed 33x33 multiplier lane, one
integer square root, one sine table, one reciprocal, and the two DIFFERENT
reciprocal seed ROMs (`FIELD_RCP_T0` seeds a 32-bit reciprocal with one
correction step, `RCP24_T0` a 24-bit one with two; feeding either function the
other's table would be invisible until some normalised vector came out short).

**79 DSP blocks -> 3.** The production Field cone contains exactly one
nonconstant `*`.

**And simple ops still cost six clocks.** The three register-read cycles were
idle; they are now the lane's issue slots, and the first operand group is read
in `Q_LATCH` from the instruction memory's own outputs rather than a cycle later
from the latched fields -- which, with a two-cycle lane, puts DOT3's third
product in `Q_EXEC`, the state that consumes it. MUL, MAD, DOT2 and DOT3 retire
in six clocks on a machine with one multiplier; the ops that lengthened are the
per-sample ones, worst case NORMALIZE3 at 67 clocks.

Each block's differential still drives that block, through a harness wrapper in
`tests/rtl/` that supplies the shared resources and no semantics. What is new is
what a shared engine newly admits and a parallel one could not: an operation
leaving state behind for the next one. `field_seq_directed` section 13 runs every
operation ALONE and then interleaved in both directions and requires every answer
and each of the five saturation ledger lanes to match; section 14 pins WHEN a
long operation commits, because with sequenced units that is part of the contract
too. Both are proven non-vacuous by mutants written to break exactly them.

Tests are `tests/differential/field_<piece>_directed.cpp`. The "directed" column
is the check count with no arguments; "random" is the count added by the fast
lane's `--random` argument, and each nightly lane runs the same test with a
larger draw. The sine lane has no `--random` argument because it sweeps **all
65,536 angles** and reports the sweep as one check.

**EVERY PIECE IS NOW SWEPT.** The gap this section used to describe -- "built
before the sweep harness was written and have not been swept" -- is closed.

| piece | mutations | caught | survivors |
| --- | ---: | ---: | ---: |
| Arithmetic core | 34 | 31 | 3 equivalent |
| Reciprocal | 23 | **23** | 0 |
| Sine / cosine | 20 | **20** | 0 |
| Length / distance | 21 | **21** | 0 |
| Normalise | 7 | 7 | 0 |
| Table ops | 18 | 18 | 0 |
| Lattice noise | 17 | 15 | 2 equivalent |
| Rotation | 17 | 17 | 0 |
| Band | 18 | 17 | 1 equivalent |

### Sine / cosine, 2026-08-22 -- 20 of 20

COS is SIN a quarter turn on, the quadrant split, the mirror on odd quadrants,
the sign on the upper half, the table index and its fraction, the endpoint
clamp, and every part of the interpolation: slope direction, the round-half-up
constant, the shift, and whether the interpolated term is added at all. The
directed lane sweeps ALL 65,536 angles, so there is no random lane to hide in.

### Length / distance, 2026-08-22 -- 21 of 21

DIST2's subtract-before-square and its own `add` ledger lane, LEN2 vs LEN3's
third component, the saturating difference at both rails, the absolute value
before squaring, the sum of squares being unsigned and exact, the root's
saturation bound, and both output ledger lanes including the flag that has to
RIDE the 34-cycle root rather than be sampled at the end.

### A CAVEAT ON THESE SWEEPS' LANE ATTRIBUTION, withdrawn rather than repeated

The harness prints whether a mutation was caught by the directed lane or only
by the random one. **That attribution is not reliable and is withdrawn.**

On the length sweep it reported `len3_third_dropped` as caught only by the
random lane. Applied by hand, the DIRECTED lane fails two checks on it:

    FAIL: |(2,3,6)| is exactly 7.0: value: expected 0x70000, got 0x39B05
    FAIL: |(3,4,12)| is exactly 13.0: value: expected 0xD0000, got 0x50000

That is the second time the per-lane tag has been wrong while the
caught/survived verdict held -- the first was on the arithmetic core, where the
cause turned out to be a build that was not rebuilding. That cause is fixed and
this one is not explained.

**The caught/survived counts above are what these sweeps establish.** Which lane
did the catching is not, and it is better to say so than to repeat a number I
have twice found to be wrong.

### The reciprocal, swept 2026-08-22

23 mutations, **23 caught, 0 survivors**, every one by the DIRECTED lane rather
than only by the 20,000-case random draw. They attacked the parts a large
random draw over a nearly-right implementation will happily agree with: the
zero case and its own `rcp0` ledger lane, sign handling on both the magnitude
and the result, the normalisation exponent, the seed-ROM index, the Newton
correction's `2^48` constant and its sign, the `rescale_u(.,47)` round-half-up
constant and shift, the Q16 shift, the exponent rescale's rounding, and the
saturation bound with its lane.

**The equivalent mutant the block already documents was confirmed, not
rediscovered.** `zhao_field_rcp.sv` records that removing the `e == 0` guard
survives the whole suite, and explains why: `e == 0` happens only for
|a| == 1, and 1/(1/65536) is 2^32, which saturates whatever the shift did. That
note was written before this sweep existed and it held up — which is the point
of recording equivalents rather than leaving them to look like holes.

### The three Earth sinks, built 2026-08-25

`FIELD.WRITE.MATERIAL`, `FIELD.WRITE.NAV` and `FIELD.WRITE.HAZARD` are the
owner ruling of 2026-08-24 in fabric: `zhao_field_sinks.sv`, differentially
tested against `zref::fieldir::compose_material`, `compose_nav` and
`compose_hazard`.

**What was already there was NOT hardware evidence.**
`tests/differential/field_write_earth_sinks.cpp` pins all three laws and passes
-- but it drives the REFERENCE only. There is no DUT in it, because there was no
RTL. It sat in `tests/differential/` looking exactly like the hardware
differentials beside it. Advancing anything on its strength would have been the
precise confusion the maturity ladder exists to prevent.

**The three ops named references that do not exist.** `ops.yml` cited
`zref::fieldir::sink_write_material` and friends -- three of the forty phantom
`zref::fieldir::*` names. The behaviour is real under another name, exactly as
`interpret` was, so the fix is to point the ledger at the real law rather than
write a second one. Repointed to `compose_*` and to the new differential.

**The interface adds nothing the laws did not supply.** MATERIAL needs an
explicit enable because "last ENABLED writer wins" requires one, and the
reference has it. NAV and HAZARD do not get enable bits: the additive identity
is 0 and the reference states outright that zero is neutral for hazard, so a
beat with no contribution writes 0. Inventing two enable signals would have been
interface where a neutral element already existed.

**No DSP.** The u8 conversion is `s * 255`, which is a shift and a subtract.

**11 mutants, 11 caught**, and they are deliberately not variations on one
theme -- each is a different rule a reasonable implementer might have written:
material ignoring the enable; nav wrapping, flooring at one, or flooring PER
STEP; hazard summing instead of maxing, letting a weak field lower authored
danger, skipping the negative floor, skipping the 1.0 clamp, truncating instead
of rounding, scaling by 256; and the authored layer not surviving an empty
program.

**M40 is the one that matters.** The reference saturates to the int32 range
after every delta but floors at zero only ONCE, on the way out. So from cost
1.0, deltas `{-10.0, +20.0}` give **11.0, not 20.0** -- the intermediate -9.0 is
carried, not floored. A per-step floor passes every other nav case in the file.
Section 3e exists for exactly that, and the sweep is where "3e would catch it"
stopped being a claim.

### FIELD.PROGCACHE: a block the sweep never covered, 2026-08-25

The audit of 2026-08-25 found this block had a 16.8 KB directed test, 223 lines
of RTL, and **neither sweep coverage nor an `ENFORCED-BY` marker**. I first
described it as "pointedly excluded" by `sweep_field_dsp.sh`; that was wrong and
worth correcting, because it implied a decision somebody made. **It was simply
never added.** Overlooked is a different and more fixable problem than excluded.

Four mutants, attacking the four laws the block's own header states:

| | defect | result |
| --- | --- | --- |
| M76 | the victim is the MOST recently used entry | caught |
| M77 | eviction preferred over a free slot | caught |
| M78 | a free slot claimed even when valid | caught |
| M79 | a lookup and a commit fire on the same clock | **SURVIVED** |

**M76 mattered most going in** -- the header warns that a realistic acquire rate
would "silently invert the eviction order", a defect that costs hit rate without
ever producing a wrong answer. It is caught, so that law is genuinely tested.

### M79 was a real hole, and it is now closed

`cm_ready_o`'s `&& !lu_fire` term stops a lookup and a commit touching the entry
table on the same clock. **Every existing case drove a lookup, waited for its
reply, and only then drove a commit**, so the term was never exercised and
deleting it survived the entire suite.

It is also a law the reference CANNOT answer: `zref::field::ProgCache` is
sequential by construction and has no notion of two requests in flight. So this
is a structural property of the block, asserted directly rather than
differentially.

**Closed and verified in both directions**, which is the only way a new test
means anything:

    pristine RTL     124 checks pass
    M79 applied      FAILS, got 0x20 -- 32 simultaneous fires

Two silent traps were hit while proving that second row, both worth recording:
`Copy-Item -ErrorAction SilentlyContinue` reported success having found no
source, so the restore had to go through git; and the rebuild afterwards printed
`[1/1] Linking` with no verilate step, meaning the "failure" was still the
mutant's model. Deleting the model directory forced real regeneration.

### Wave 10 measured: 54.80 -> 58.99 MHz, and the table became memory

| | wave 9 (`9fcd7b9`) | wave 10 (`4bfdbde`) |
| --- | ---: | ---: |
| Fmax | 54.80 MHz | **58.99 MHz** (+7.6%) |
| ALMs | 4,692 | **4,494** (-198) |
| M10K blocks | 4 | **5** |
| memory bits | 8,192 | **12,561** (+4,369) |
| DSPs | 3 | 3, unchanged |

Provenance clean, 1856.8 s. Cumulative **8.59 -> 58.99 MHz, 6.9x.**

**+4,369 bits is exactly 257 x 17.** The table INFERRED, rather than staying
logic -- which is what the storage law predicts for synchronous reads with no
reset on the array and no byte enables, and what the 198 recovered ALMs are.
This is the first wave to pay for itself in area as well as time.

### The path left SIN entirely, and the shape of the problem changed

    u_ring|e1[4] -> u_mul|b_q[0]     16.95 ns

RING computing an operand and handing it to the shared multiplier, through
`lz_t`, `ShiftLeft0`, `Ram0` and two adders. **26 logic levels**, against SIN's
39 -- the cone is genuinely shorter now, not merely different.

**But the routing fraction is rising and that is worth watching.** The split is
now 8.606 ns of cell against 8.094 ns of interconnect -- **48% routing**, up
from 42% at wave 9 and 30% at wave 6. `QUARTUS_GOTCHAS` §12 says a leaf fit in a
mostly-empty device measures ROUTING rather than the design, and that is what
voided the renderer pair ranking. At 39 levels the depth dominated and the
number meant what it looked like; at 26 levels and half routing, it is starting
not to.

**This does not void the measurement** -- 0.33 ns per logic level is still
inside the ideal band, and the ALM and M10K deltas are placement-independent
facts. But the next wave should be judged with the ratio in mind, and a
composed fit will be needed before any of these numbers is called a system
frequency.

### Wave 11: RING's operand into the multiplier

The cone ends at `u_mul|b_q`, the shared lane's operand register, so RING is
computing a product operand combinationally across its whole normalisation.
Registering that operand splits it.

RING costs ~54 clocks against `MAX_OP_CYCLES = 80`, and NORMALIZE3 at 68 is the
worst op, so one or two clocks on RING do not move the bound at all. Two of the
twelve are spent, both on ROT.

### Wave 9 measured: 49.90 -> 54.80 MHz, for nothing

| | wave 8 (`ed3c274`) | wave 9 (`9fcd7b9`) |
| --- | ---: | ---: |
| Fmax | 49.90 MHz | **54.80 MHz** (+9.8%) |
| ALMs | 4,725 | **4,692** (-33) |
| DSP / M10K | 3 / 4 | 3 / 4, unchanged |

Provenance clean, 1224.4 s. Cumulative **8.59 -> 54.80 MHz, 6.4x**, and this one
cost **no clocks and no area** -- `NORMALIZE3` is still 68 of 80.

### Reading the RUNNER-UP, which is what wave 8 taught

All twelve worst paths are now ONE cone:

    i_op[2] -> u_exec|u_sin|result_o[N]   18.25 ns

so there is no second cone hiding behind this one -- unlike wave 8, where SIN
and NORMALIZE were the same length and cutting one bought 2%.

**And a reading error of mine is corrected here.** I reported "90 of 162 cells
are OUTSIDE SIN" for wave 8, and said the same again at 80 cells for wave 9.
Both were double-counts: the hierarchical name is
`zhao_field_exec_shared:u_exec|zhao_field_sin:u_sin|...`, so a grep for `u_exec|`
matches the prefix of every SIN cell. Of the 80 "u_exec" cells, **72 are the
`u_sin` prefix**. The cone is essentially all SIN plus about four cells of
opcode compare. No amount of work outside SIN was ever going to help.

**The depth is real, not sprawl.** The report gives 39 logic levels, 10.41 ns of
cell delay against 7.66 ns of routing -- about **0.46 ns per level**, inside the
0.3-0.5 ideal band. This is not the routing-dominated artefact that voided the
renderer ranking (`QUARTUS_GOTCHAS` §12).

### Wave 10: the shelved ROM draft, now justified by measurement

The cone in order is

    Add1 -> i -> u_base -> Add3 -> Add8 -> Add4 -> Add6 -> s_quarter -> Add10

so `Add1` (22 cells) is the ANGLE DECODE ahead of the table, and `Add10` (20)
is the final negate. The midpoint sits at the ROM output: about 30 cells before,
46 after.

That is exactly where the synchronous dual-port ROM drafted after wave 6 and
shelved after wave 7 puts a register. It was correctly shelved twice -- at wave
6 SIN was off the path entirely, and at wave 8 the ROM was four cells of a cone
whose midpoint lay elsewhere. **The measurement now puts the midpoint on it.**

Latency becomes 2. OP_SIN and OP_COS still pay nothing (`a0` stands three states
before `Q_EXEC`); ROT pays one more wait state, its second of the twelve-clock
budget. The retiming hazard returns with it -- `q`, `t` and `i` must travel with
the delayed read -- which is what the drafted `q_q`/`t_q`/`i_q` and the
back-to-back test sections exist for.

### Wave 8 measured: 48.92 -> 49.90 MHz. IT WORKED AND IT BARELY HELPED.

| | wave 7 (`dc341b5`) | wave 8 (`ed3c274`) |
| --- | ---: | ---: |
| Fmax | 48.92 MHz | **49.90 MHz** (+2.0%) |
| ALMs | 4,713 | 4,725 |
| DSP / M10K | 3 / 4 | 3 / 4, unchanged |

Provenance clean, 1429.9 s. Cumulative **8.59 -> 49.90 MHz, 5.8x.**

**Two percent, against a cut that should have halved the path.** The register
did exactly what it was meant to: SIN is GONE from the worst path, entirely.
The problem is what was behind it.

    before   i_op[0] -> walk_wdata_q[30]        19.713 ns   (72 cells in u_sin)
    after    u_norm|h_rt[46] -> u_norm|o0_o[1]  19.875 ns   (117 cells in u_norm)

**The two cones were the same length.** Removing one exposed the other at
19.875 ns -- 0.16 ns WORSE than the path it replaced. This is the whack-a-mole
phase, and it is worth naming because the lesson generalises: cutting the worst
path only pays when the second-worst is meaningfully shorter, and nothing in the
report tells you that until you look at the runner-up.

**A prediction is withdrawn.** Wave 8 was expected to split ~72 cells from ~90
and land near the 10 ns budget. It did split them; the gain was 2% because the
budget was never the binding constraint on that path alone.

### Wave 9: the incrementer that wave 7 left behind

    Add12  66 cells of the 117 in u_norm

`Add12` is `sh + 65'(rnd)`, the increment wave 7 substituted for the 65-bit
ripple-carry ADD. Wave 7 halved it -- 122 cells to 66 -- and it is still the
single largest element on the path.

There is no further identity to spend: the rounding is already one bit. So
wave 9 registers across it, splitting

    h_rt -> leading zero -> e_val -> k -> (v >>> k)        [stage A]
    sh + rnd -> saturate -> o0_o                           [stage B]

at a cost of one clock per output lane. NORMALIZE3 is 68 clocks against
`MAX_OP_CYCLES = 80`, so the headroom is there -- but it is TWELVE clocks total
and wave 8 already spent one on ROT, so the accounting has to be checked and not
assumed.

### Wave 7 measured: 45.42 -> 48.92 MHz, and SIN CAME BACK

| | wave 6 (`9371875`) | wave 7 (`dc341b5`) |
| --- | ---: | ---: |
| Fmax | 45.42 MHz | **48.92 MHz** (+7.7%) |
| ALMs | 4,665 | 4,713 (+48) |
| DSP / M10K | 3 / 4 | 3 / 4, unchanged |

Provenance clean, 1294.6 s. Cumulative **8.59 -> 48.92 MHz, 5.7x.**

**The path returned to SIN.** NORMALIZE is gone from it entirely:

    i_op[0] -> walk_wdata_q[30]   19.713 ns
    72 of 162 cells in u_sin, ZERO in u_norm

So the pipelined-SIN work drafted and then SHELVED after wave 6 is the right
change after all. Shelving it was still correct: at wave 6 it would have bought
nothing, and the measurement said so.

**What is left in SIN is no longer algebra.** Six serial adders -- `Add1` (22
cells), `Add8` (14), `Add3` (10), `Add6` (8), `Add10` (6), `Add4` (4) -- and
only FOUR cells of ROM. The tree is already depth 3 and `base` is already folded
in; there is no further identity to exploit.

**And the other 90 cells are not in SIN at all** -- they are the opcode mux and
the result selection between SIN and `walk_wdata_q`. Even a perfect SIN would
leave them.

### Wave 8: stop hunting identities, spend a clock

The owner ruling of 2026-08-25 makes latency currency: at 100 MHz even eight
clocks per simple op is 2.2x the real throughput of six clocks at 33.86 MHz.

Registering SIN's OUTPUT cuts this path at its midpoint:

    i_op -> decode -> ROM -> tree -> round -> sign     (~72 cells)
    REGISTER
    sin result -> exec mux -> walk_wdata_q             (~90 cells)

**It costs OP_SIN and OP_COS nothing.** `a0` latches at the `Q_RD1 -> Q_RD2`
edge and `Q_GATH` is the ONLY entry to `Q_EXEC`, three states later, so up to
two cycles of table latency are absorbed by depth that already exists.

ROT pays: its walk captures `trig_out` on consecutive edges and needs one wait
state per read. That is ~2 clocks on a 25-clock op, against `MAX_OP_CYCLES = 80`
with NORMALIZE3 worst at 68 -- twelve clocks of declared headroom.

### Wave 6 measured: 43.89 -> 45.42 MHz, and SIN LEFT THE PATH ENTIRELY

| | wave 5 (`c37462f`) | wave 6 (`9371875`) |
| --- | ---: | ---: |
| Fmax | 43.89 MHz | **45.42 MHz** (+3.5%) |
| ALMs | 4,682 | **4,665** (-17) |
| DSP / M10K | 3 / 4 | 3 / 4, unchanged |

Provenance clean, `rtlCleanAtHead` true, 891.3 s. Cumulative **8.59 -> 45.42
MHz, 5.3x.**

Modest, and proportionate: removing one adder from a five-adder chain.

**THE PATH THEN MOVED OFF SIN COMPLETELY.** The new worst path is

    u_exec|u_norm|h_rt[16] -> u_exec|u_norm|o0_o[0]   21.835 ns

with **175 cells in `zhao_field_normalize` and ZERO in `u_sin`.** SIN went from
64 cells to none.

**This kills wave 7 as drafted.** A pipelined SIN table was scoped, drafted and
ready to apply -- synchronous dual-port ROM, retimed decode, one extra ROT state
-- and it would now buy **no frequency at all**. That was the stated risk before
wave 6 ran: *flattening SIN may simply promote whatever was second.* It did.

The draft is not wasted -- the table is still two combinational 257-entry LUT
muxes, roughly 8,700 bits, and converting it to one M10K is an AREA win worth
taking later. But it is no longer a SPEED wave, and shipping it as one would
have been measuring the wrong thing.

**Wave 7 is now `resc_s` in NORMALIZE, and 122 of the path's 175 cells are one
adder.** `Add12` is this:

    r = (k == 0) ? 65'(v) : ((65'(v) + (65'sd1 <<< (k - 1))) >>> k);

a full 65-bit ripple-carry add of a dynamically shifted rounding constant.

**It can be removed exactly, not approximated.** Writing `v = q*2^k + r` with
`0 <= r < 2^k` under floor semantics, `(v + 2^(k-1)) >> k` = `q + [r >= 2^(k-1)]`
= `(v >>> k) + v[k-1]`. The rounding constant only ever contributes the single
bit at position `k-1`. So the 65-bit ADD becomes a shift plus an INCREMENT --
and because the result saturates to 32 bits, the increment needs about 34 bits
with the high slice deciding overflow, not 65.

### Wave 5 measured: 36.84 -> 43.89 MHz, 2026-08-25

| | wave 4 (`7396df3`) | wave 5 (`c37462f`) |
| --- | ---: | ---: |
| Fmax | 36.84 MHz | **43.89 MHz** (+19.1%) |
| ALMs | 4,673 | 4,682 (+9) |
| registers | 3,498 | 3,505 |
| DSP / M10K | 3 / 4 | 3 / 4, unchanged |

Provenance clean: `sourceCommit` equals HEAD, `rtlCleanAtHead` true, 46 sources
hashed, 1048.7 s.

**Nineteen percent for nine ALMs**, the largest single-wave gain since the
register file became block memory. Cumulative on this block: **8.59 -> 43.89
MHz, 5.1x.**

That is a bigger gain than wave 4's 8.4%, and the reason is worth keeping: wave
4 registered an ENDPOINT, which can only ever buy the routing and setup at the
end of a path. Wave 5 changed the SHAPE of the cone. Depth is worth more than
endpoints.

**SIN is still the largest contributor**, so this is not finished. The new worst
path is

    i_op[4] -> zhao_field_exec_shared:u_exec|zhao_field_rot:u_rot|s_val[16]
    22.627 ns  (was 26.946)

and 64 of its 154 cells are still inside `u_sin`, now feeding ROT's table read.
Broken down by sub-block, what remains is **five serial adders** -- `Add11` (18
cells), `Add3` (12), `Add9` (10), `Add4` (6), `Add5` (4) -- plus four cells of
ROM. The tree flattened the ACCUMULATION; the adds around it did not move:

    ROM -> d = next_v - base -> tree (depth 3) -> s_quarter = base + interp -> negate

**Wave 6 is therefore `s_quarter`, and it is free.** `base + (X >>> 6)` equals
`((base <<< 6) + X) >>> 6` EXACTLY, because `base <<< 6` is a multiple of 64 and
the shift is a floor -- so `base` can join the tree as another leaf instead of
being added after it. The tree has 7 leaves today (6 terms plus the rounding
constant) and 8 leaves is still depth 3, so folding `base` in removes a
full-width serial add and costs **no extra level**.

### Wave 4 measured, and the path re-ranked the waves AGAIN, 2026-08-25

| | wave 3 (`01598b3`) | wave 4 (`7396df3`) |
| --- | ---: | ---: |
| Fmax | 33.98 MHz | **36.84 MHz** (+8.4%) |
| ALMs | 4,821 | **4,673** (-3%) |
| registers | 3,459 | 3,498 (+39, the added stage) |
| DSP / M10K | 3 / 4 | 3 / 4, unchanged |

Provenance clean: `sourceCommit` equals HEAD, `rtlCleanAtHead` true, 45 sources
hashed, 977.3 s.

**A real gain and a modest one.** The prediction recorded before the run was
that removing the opcode from the write-port address path would help but not
reach 100 MHz, because the long arithmetic units were untouched. That held.

**But the endpoint moved and the CAUSE did not.** The new worst path is

    i_op[7]~DUPLICATE -> walk_wdata_q[27]   26.946 ns  (was 29.250 ns)

which is the same opcode-driven result selection, now landing in wave 4's own
pipeline register instead of the memory write port. Wave 4 bought the 2.3 ns of
routing and write-port setup it was aimed at, and nothing more, which is exactly
what registering an endpoint can buy.

**Attributed rather than guessed: 80 of the path's cells are inside `u_sin`,
and ZERO are in any other unit** -- not `u_isqrt`, `u_rcp`, `u_curve`,
`u_noise`, `u_ring`, `u_rot`, `u_alu`, `u_mul`, `u_norm` or `u_len`. It is a
ripple-carry chain, `cin`/`cout` repeating the length of the cone.

So **wave 5 (SIN) is next, not wave 2 (isqrt)**. The ruling's wave order puts
isqrt first, and isqrt contributes nothing measurable to the current worst path.
This is the second time the measured path has re-ranked the plan -- the first
was wave 4 itself being promoted ahead of wave 2 at 33.98 MHz. Reading the order
instead of the report would have spent a day on the wrong unit, twice.

### The sequencer's registered write-back, wave 4, 2026-08-25

The walk's register-file write is delayed by one edge to get the opcode out of
the write-port address path. The valid bitmap moved with it, driven from the
same registered enable. A comment claimed that setting the bit at the OLD edge
"would open a window where valid is 1 while the memory still holds the previous
value" — presented as a hazard the new placement avoided.

**That claim was wrong, and the ledger's V20 rule is what forced it to be
checked.** The rule refused an invariant with no named enforcer; rather than
name a plausible one, the variant was built and run.

| variant | binary hash changed | result |
| --- | --- | --- |
| valid set at the pre-delay edge | yes, `4ffcaa51` -> distinct | **passes all 1,127 checks** |
| valid assignment deleted | yes, `a3dd0343` | **fails from check 1.one add** |

The first is a **proven-equivalent mutant**: the decoder leaves two edges
between a write-back and the next operand read, so nothing is ever inside the
one-cycle window where bit and datum would disagree. The second establishes the
bit is load-bearing, so the equivalence is not vacuous — without that second
run, "the mutant passed" would have been indistinguishable from "the tests do
not exercise this at all", which is the exact confusion equivalence records
exist to prevent.

The ordering is still worth keeping: it is correct by construction instead of
correct by a slack budget that a later wave could spend. But it is recorded as
a preference with a reason, not as a bug that was fixed.

### The arithmetic core, swept 2026-08-22

34 mutations, **31 caught, 3 equivalent, 0 real gaps**, and every one of the 31
was caught by the DIRECTED lane rather than only by the 20,000-program random
draw. The mutations attacked the laws rather than the arithmetic: where
saturation triggers, which direction rounding goes, which ledger lane records
it, CLAMP's operand order, each CMP predicate's boundary, MAD's `c <<< 16`,
DOT2/DOT3's per-lane terms.

**Two directed-lane holes were found and closed.** Every DOT2 case used a `b`
vector whose first two elements were equal — `{1.0,1.0,1.0}`, `{MAX,MAX,MAX}`,
`{0.5,0.5,0.5}` — so a block computing `a0*b0 + a1*b0` instead of
`a0*b0 + a1*b1` answered every one of them correctly. That is visible by
inspection and needed no sweep to justify. Asymmetric cases added for both
DOT2's second term and DOT3's third.

**The three survivors are equivalent by algebra**, and the argument is in
`zhao_field_alu.sv` beside the bound: clamping a value that already sits
exactly on the rail returns that same value, so `v > MAX`, `v > MAX-1` and
`v >= MAX` cannot be distinguished by any input. What makes that safe rather
than merely untested is that the LEDGER bound lives in a separate function,
`sat32_fired`, whose own mutations were caught both ways.

**A warning about this sweep's method, because it nearly produced a confident
wrong answer.** Three earlier runs of it reported different scores — 31/34 with
four "caught only by the random lane", then a spurious equivalence, then a
false staleness abort. All three were artefacts of ONE defect: `ninja` could
not regenerate `build.ninja` (cmake reads `VERILATOR_ROOT` from the
environment and it was unset), and when that happens **ninja builds nothing at
all** while still printing plausible output. Every mutation then tested
whatever binary was lying around. The harness now treats
`rebuilding 'build.ninja'` as fatal. Only the fourth run, on a sound build, is
recorded here.

## What is not built

**Every op is built, and so is the sequencer.** `FIELD.SEQ.CORE` is
`RTL_VERIFIED`: all 31 opcodes dispatch under a coverage gate, and the anti-hang
law is formally proven with every instruction word free
(`tests/formal/field_seq_bound.sby`).

**The five `FIELD.SEQ.*` profiles are no longer waiting on anything to be**
**built, because they are not blocks.** Owner ruling, 2026-08-22: *one engine,*
*five profiles*. They are now `kind: profile` in the ledger with
`implemented_by: FIELD.SEQ.CORE`, and rule V21 holds that exemption to its
price — a profile must name its engine, may not out-claim its maturity, and may
not book an ALM budget.

This section used to say the five would "stay SPECIFIED until the sequencer
exists". That framing was wrong in a way worth recording: nothing in the RTL
ever distinguished them. `zhao_field_seq` has no profile input and no
profile-specific port, and what would distinguish a profile — which registers
the input and output lanes bind to — is carried by the DECODED PROGRAM, not by
the block. Five ledger entries at `kind: rtl` were demanding five reference
models and ten test files under V4, and booking **five engines worth of ALM
budget for one engine** under V5.

**What genuinely remains for the profiles is not hardware.** Each needs its lane
binding written down — which registers the inputs arrive in and which the
outputs are read from, per program. That is a software and shell question and it
belongs with the blocks that consume the output. For FLOW it is particle
behaviour, which is reserved to the owner in any case.
## Notes worth keeping

### There are two reciprocal tables and they are not interchangeable

`FIELD_RCP_T0` seeds a 32-bit reciprocal with **one** correction step;
`RCP24_T0` seeds a 24-bit one with **two**. Feeding either function the other's
table would be invisible until some normalised vector came out slightly short.
Both are generated from `zref_tables.hpp` rather than typed, and every entry is
checked against the source table.

`RCP24_T0` **descends** — a larger mantissa has a smaller reciprocal — so the
obvious endpoint sanity check (assert the first entry is the smallest) is
backwards for it.

### The zero case is asymmetric between the two normalise ops

`NORMALIZE2` records the `rcp0` ledger lane on a zero vector; `NORMALIZE3`
returns zeros and records nothing. Making them consistent is the obvious
tidy-up and would disagree with every capture the software has produced. Both
halves are pinned, and the mutation that "fixes" it fails.

### The table ops read per-program data, not hardware constants

`CURVE`, `DCURVE` and `SPLINE` are the first ops that read a **table**, and
tables are carried in the `.zprog` image. So `zhao_field_curve.sv` takes a table
port instead of owning a ROM, and the port is a registered read per the M10K
rules.

Three laws in that block are worth naming because each has a plausible wrong
version:

- **The segment search is six steps for every table size**, not
  `ceil(log2(n))`. The decoder caps a table at 64 entries, which is exactly what
  six steps reach.
- **The search runs on the clamped value**, never the raw one, and both bounds
  come from the table's own ends.
- **`SPLINE`'s closing term is `rescale_s32(v, 1)`** — the one-half of
  Catmull-Rom — not `v << 16`. The shift form amplified the term by 2^16 and is
  a fixed defect (review C1, RUN-20260814-1912 wave-1). It is named in the RTL,
  in the test and here so it does not come back.

### A sweep that reverts silently is a sweep that lies

The `RING` sweep first reported 17 of 18 caught. It was wrong.

`rcp0_not_sticky` replaced its line with `rcp0_o <= 1'b0;` — text that also
appears in the reset and accept branches — so the uniqueness check refused to
**revert** it. The mutation stayed applied, the next mutation was measured
against a still-mutated design and scored as CAUGHT, and the pristine re-check
came back red at the end.

The binary-hash assertion cannot see this: the binary *does* change every time.
So the sweep now makes **two** assertions per mutation — the hash moved, and the
revert both succeeded **and** left the file byte-identical to a pristine copy
taken at the start. A failed revert aborts the run rather than continuing to
report numbers nobody should trust.

Re-run with the check in place, one of the "caught" results turned out to be a
false positive.

### `RING`'s midpoint lane is dead, and the line stays

Moving the midpoint's saturation from the `rescale` lane to `add` survives,
because **the midpoint cannot saturate**. The exact sum of two `s32` values lies
in `[-2^32, 2^32 - 2]`, and halving with round-half-up lands in
`[INT32_MIN, INT32_MAX]` for every input — verified over 300,000 cases, with
both rails hit exactly and nothing outside.

`sat_rescale_o` is therefore always low for `RING`. The line stays because the
reference records the lane there; the test asserts the lane is low rather than
leaving it unexamined.

The *other* survivor was not equivalent at all: pooling the `rcp` lane into
`mul` survived only because every ring in the test had a span of whole units.
`field_rcp` saturates when the reciprocal exceeds `INT32_MAX`, which needs a
span of a few **raw** units. With those cases added it is caught by 28 checks.

### The rotation ops round TWICE, and that is the law

`fx_sub(fx_mul(c,p), fx_mul(s,q))` rounds each product separately and then
saturates the difference. Everywhere else in this design — `mat4_vec4`,
`fx_mad`, GEOM.SKIN — a row of products is summed exactly and rescaled **once**,
because double rounding is normally the bug. Here the reference does the
opposite.

**The rule is not "single rounding is always right"; it is "match the
reference".** An implementation improved into the house style is wrong, and
about a quarter of random inputs can tell the two apart. The test counts the
inputs where a fused form *would* differ and asserts that count is large — a
sweep on which the two happen to agree proves nothing about which is
implemented.

### Two mutations that survived because they were wrong, not because the test was

Worth recording, because a surviving mutation is only evidence if the mutation
is real:

- **`fused_single_rounding` (first attempt)** was algebraically a no-op.
  `rescale(t·2^16 + p, 16)` equals `t + rescale(p, 16)` exactly, so it
  "survived" while changing nothing. Rewritten to hold the exact 64-bit
  products and rescale once — a coordinated multi-edit, since it needs a wider
  register and every use of it updated — it is caught by 249 directed checks.
- **`sat_lanes_pooled`** survived because the test only compared the
  **collapsed** `Status.sat` bit, which cannot tell an `add` saturation from a
  `mul` one. The test now restates the per-lane attribution and drives the two
  lanes apart; the mutation is caught.

### The PCG's last step is dead code here, and it stays

`noise2_hash` ends with `(w >> 22) ^ w`. Both `NOISE2` and `RIDGE` then keep
only bits `[31:16]` — and `w >> 22` has nothing above bit 9. **The xor perturbs
exactly the half the ops discard**, so that line cannot change either op's
answer.

Two mutations of it survive the sweep and both are recorded as equivalent
mutants rather than left looking like holes: dropping the xor-shift, and
changing its shift amount from 22 to 21.

It stays in the RTL. The reference is the law and this block is its
differential, not its optimiser — an implementation that agrees on every
observable output while quietly computing something else is the thing this
method exists to prevent, and the day the op is widened to keep more bits, a
"simplified" version would be silently wrong. The test pins the *reason*
directly (section 9) rather than leaving it in a comment nobody re-derives.

### Three gaps the table-op sweep found, and what closed them

The first sweep of `zhao_field_curve.sv` left one survivor and two mutations
that only the random lane caught. All three were real holes in the directed set:

1. **The `[0,1]` clamp on the spline segment parameter survived removal.** On a
   well-formed spline table it is unreachable — but the decoder validates a
   spline table's kind, count, x-order and spacing, and *never* its `dy`. A
   fully decodable program can carry a `dy` that is not `1/step`, which drives
   the parameter out of range. The clamp is what makes the hardware agree with
   the interpreter on such a program, and every well-formed table hides that.
2. **The high end of the clamp was invisible.** The test's table builder gives
   the last knot a slope of zero, because there is no segment after it — so the
   extrapolation term vanishes whether or not the value was clamped, and a block
   clamping only the low end passed every case. Closed with a table whose final
   slope is set by hand.
3. **The single-rounding law was untested at the rails.**
   `rescale(d·dy + (y << 16), 16)` and `rescale(d·dy, 16) + y` are *exactly*
   equal for every value that fits — they part company only where the
   intermediate saturates and the sum would have pulled it back. A directed set
   that never reaches the rail cannot tell the two apart at all.

After those three sections were added, the directed test alone catches all
eighteen mutations.
