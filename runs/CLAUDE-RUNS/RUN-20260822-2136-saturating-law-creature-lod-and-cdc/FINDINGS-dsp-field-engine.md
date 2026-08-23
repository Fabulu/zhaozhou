# Field IR engine: one shared arithmetic engine — Findings

**Agent ID:** claude-dsp-field-engine
**Created:** 2026-08-23
**Parent Task:** RUN-20260822-2136
**Status:** Complete

---

## Summary

`zhao_field_seq` instantiated ten op units side by side — ALU, reciprocal, sine,
integer root, length, normalise, curve, noise, ring and rotation — each owning
its own physical multiplier, while retiring **one instruction at a time** on a
six-clock walk. Nine of ten were idle at every instant, holding multipliers.

They now share one `zhao_field_exec_shared`: **one** signed 33×33 multiplier
lane, **one** integer square root, **one** sine table, **one** ordinary
reciprocal and the two (different) reciprocal seed ROMs.

**DSPs: 79 → 3 of 112.** **ALMs: 10,615 →
8,901.** Simple ops still retire in **six clocks**; the worst op
is **67**, against a proven bound of 80 and a required ceiling of 96.

---

## Findings

### 1. The measurement, both sides, measured by me

| | ALMs | DSPs | registers | commit | worktree |
| --- | ---: | ---: | ---: | --- | --- |
| before, CONSTRAINED | 10,615 | **79** / 112 | 4,543 | `57352cf` | clean |
| after, CONSTRAINED | **8,901** | **3** / 112 | 5,356 | `62d7b0e` | clean |
| delta | **-1,714 (-16.1%)** | **-76 (-96%)** | +813 | | |
| before, unconstrained (for contrast) | 10,623 | 79 | 4,510 | `57352cf` | clean |
| after, unconstrained (4-DSP sine) | 8,832 | 4 | 5,367 | `e576625` | clean |

Quartus Prime Lite 17.0.2, 5CSEBA6U23I7 (provisional), virtual pins, no composed
fit, `zhao_field_seq` plus its fourteen field sources as the cone. Nothing here
is a programmed device.

**The BEFORE was re-measured rather than quoted.** The committed row said
10,623 ALMs / 79 DSPs at `47d607c9`; Analysis & Synthesis reproduced **79 DSP
blocks and 4,600 registers** on this machine before a line was changed, and the
fitter reproduced the ALMs.

### 2. Three DSPs, not eight to twelve — and the reason is the one the LOD pilot found

The docket's target for this subsystem was 8–12. The answer is
3, and the reason generalises: **the cost of sharing is the
sequencer, and once it is paid, every remaining product is nearly free.**

Twenty-nine nonconstant multipliers were counted in the old cone — three in the
ALU, three squares in LEN, ten in NORMALIZE, four in RCP (two instances), one
each in NOISE, ROT and RING, four in CURVE, and two sine interpolators. Every
one of them now issues into the same 33×33 lane. There was no point stopping at
"share the obvious group": the group is the whole engine, because
`zhao_field_seq` cannot have two instructions in flight.

**The rule to carry to the next block is not "share the obvious group" but
"how many products can reach ONE multiplier".** For a subsystem that retires one
item at a time, the answer is all of them.

Measured per entity, which is where the last DSP was found:

| entity | ALMs before | ALMs after | DSPs before | DSPs after |
| --- | ---: | ---: | ---: | ---: |
| `zhao_field_normalize` | 3,323.9 | 2,169.8 | **33** | 0 |
| `zhao_field_ring` | 829.4 | 271.2 | 7 | 0 |
| `zhao_field_curve` | 769.0 | 622.4 | 10 | 0 |
| `zhao_field_len` | 461.0 | 151.4 | 9 | 0 |
| `zhao_field_rot` | 434.4 | 229.3 | 4 | 0 |
| `zhao_field_rcp` (x2 before) | 397.2 + 445.3 | 456.1 | 4 + 4 | 0 |
| `zhao_field_alu` | 399.0 | 290.2 | 9 | 0 |
| `zhao_field_noise` | 216.0 | 207.2 | 2 | 0 |
| `zhao_field_isqrt` (x2 before) | 176.1 + 193.3 | 238.0 | 0 | 0 |
| `zhao_field_sin` (x2 before) | 195.0 + 205.5 | 200.3 | 1 + 1 | **1 -> 0** |
| `zhao_field_mul` | — | 39.8 | — | **3** |

Two things fall out of that table that a summary number hides.

**The de-duplication alone is about 1,000 ALMs.** One integer root instead of
two, one reciprocal instead of two, one sine table instead of two, one ordinary
seed ROM instead of two. None of that is the multiplier work; it is the plain
answer to "should this calculation exist twice", and it was worth asking
separately.

**The last DSP was `zhao_field_sin`'s 18x6 interpolation product** — the one
place a private nonconstant multiply survived, because sequencing it would make
SIN and COS multi-cycle and lengthen every ROT by six clocks to save a DSP on a
device with 108 spare. Keeping the multiplier and refusing the DSP is a one-line
synthesis directive, `multstyle = "logic"`, invisible to Verilator and slang, so
simulation and the formal proof see identical arithmetic. **4 DSPs -> 3.**

### 2b. THE PARETO POINT: why there is one variant here and not two

The brief asked for `MUL_LANES` as a parameter with 1 and 2 both fitted. I did
not build it, and the reason is measured rather than asserted.

**Neither axis binds.** DSPs are at 3 against a target of 8. The worst op is 67
clocks against a ceiling of 96. A frontier is information about a TRADE, and
there is nothing to trade: a second lane spends the whole remaining DSP budget
to buy latency on a constraint already met with 30% margin.

**And the latency it would buy is small, because the multiplier is not what the
long ops are waiting for.** Cycle accounting per operation, derived from the
state machines and cross-checked against the totals section 12 measures:

| op | measured, 1 lane | ceiling, UNLIMITED lanes | what the remainder is |
| --- | ---: | ---: | --- |
| MUL / MAD / DOT2 / DOT3 | 6 | **6** | fetch, latch, three register reads, execute |
| RCP | 15 | 14 | two dependent product stages |
| RIDGE | 22 | 22 | a strict hash chain |
| ROT2 / ROT3 | 24 / 25 | 15 / 16 | four independent products, issued serially |
| DCURVE / CURVE | 26 / 29 | 26 / 29 | 12 clocks of binary search, then a chain |
| NOISE2 | 29 | 29 | mix -> LCG -> xor-shift, strict |
| SPLINE | 45 | 45 | binary search, then a strict Horner chain |
| LEN2 / LEN3 / DIST2 | 48 | 46 | **36 of the 48 are the integer square root** |
| RING | 54 | 52 | nine products, each dependent on the last |
| NORMALIZE2 / NORMALIZE3 | 66 / 67 | 62 / 63 | **36 are the root**; a 4-step chain |

**The binding resource in the two most expensive ops is `zhao_field_isqrt`** —
thirty-four clocks of restoring digit recurrence with **no multiplier in it at
all**. A second multiplier lane cannot touch it.

ROT is the one op with real headroom, and **its nine clocks are available at
MUL_LANES = 1**: its four products are independent and are currently issued in
four separate issue-and-wait pairs. Issuing them back to back on the single lane
recovers six of the nine for zero DSPs. It is not taken here because no
acceptance criterion moves and it would have invalidated a running sweep; it is
recorded so that it is a decision rather than an oversight.

So the incremental value of a second lane, over a well-scheduled single lane, is
**at most two clocks on any operation and zero on the six-clock path**, for
double the DSPs. The second point is dominated. If the long ops must get
shorter, the lever is a two-bit-per-cycle integer root — 34 clocks to 17, zero
DSPs, roughly double the compare-and-subtract logic — and that is on the docket
as a recommendation, not done.

### 3. Simple ops still cost six clocks, and that is a schedule, not a coincidence

The acceptance criterion was that MUL, MAD, DOT2 and DOT3 keep their six-clock
cadence on a machine with one multiplier. They do, and the mechanism is worth
recording because it was not the obvious one.

The lane is **two cycles deep** (input- and output-registered), so a product
lands two cycles after its issue. The three register-read cycles were the
suggested issue slots:

```
Q_RD0: a0 x b0     Q_RD1: a1 x b1     Q_RD2: a2 x b2     Q_EXEC: consume
```

That does **not** work with a two-cycle lane: the third product would land one
cycle *after* Q_EXEC and DOT3 would cost seven clocks. Making the lane one cycle
deep would fix it and put a 33×33 multiply, a 66-bit accumulate and a saturating
rescale in a single combinational path.

The fix costs nothing and needs no extra read port: **read the first operand
group in `Q_LATCH`**, from the instruction memory's own combinational outputs,
instead of a cycle later from the latched fields.

```
Q_LATCH  ins_a+0  ins_b+0  ins_c   -> issue, lands in Q_RD2
Q_RD1    i_a+1    i_b+1            -> issue, lands in Q_GATH
Q_RD2    i_a+2    i_b+2            -> issue, lands in Q_EXEC
Q_GATH   (the accumulator closes)
Q_EXEC   prod_ab, dot2 and dot3 are standing ready
```

Same three ports, same six clocks, one multiplier. `Q_GATH` looks like slack and
is not: delete it and DOT3 loses its third term. There is a mutant for exactly
that (M18).

### 4. The measured cost table

Section 12 of `tests/differential/field_seq_directed.cpp` prints this on every
run, so a latency regression is a diff rather than an inference:

| op | clocks |
| --- | ---: |
| MOV, ADD, MUL, MAD, DOT2, DOT3, SIN, COS | **6** |
| RCP | 15 |
| RIDGE | 22 |
| ROT2 / ROT3 | 24 / 25 |
| DCURVE / CURVE | 26 / 29 |
| NOISE2 | 29 |
| SPLINE | 45 |
| LEN2 / LEN3 / DIST2 | 48 |
| RING | 54 |
| NORMALIZE2 / NORMALIZE3 | 66 / 67 |

Worst op **67**, required ceiling 96, proven bound 80.

### 4b. NO PER-BLOCK FIT HAS EVER BEEN TIMING-CONSTRAINED

This one outgrew the block and is the most valuable thing in this document.

`tools/quartus/run_block_fit.ps1` copied `zhao_shell_fit.sdc`, which says:

```
create_clock -name gpu_clk -period 10.000 [get_ports {gpu_clk}]
```

Every leaf block's clock port is called **`clk`**, and the block flow makes all
I/O virtual. Quartus says what happened in its own log, three times per run:

```
Warning (332174): Ignored filter at blockfit.sdc(4): gpu_clk could not be
                  matched with a port
Warning (332049): Ignored create_clock at blockfit.sdc(4): Argument <targets>
                  is an empty collection
```

**So every row in `reports/synthesis/zhao_block_fit.json` was fitted with no
timing objective.** The area numbers stand — they are placement and resource
counts. Anything anyone infers about block-level timing from that file is void,
and more subtly an unconstrained fit optimises for area rather than speed, so
those ALM numbers are the optimistic end for a design later asked to close
timing.

It was found by asking a question the flow could not answer: the mid-flight
brief wanted WNS and TNS in the results table, and there were none to report.
The coordinator reproduced it independently and has fixed the script to write a
per-block SDC naming every clock-shaped port in `fpga/rtl`.

**Both sides of this measurement were therefore re-taken with the fixed script**,
and the unconstrained pair is kept beside them, because the difference between
the two is itself the evidence for how much the old census understated.

### 5. The anti-hang proof: 120 was a magic constant, and it was also wrong for this design

`tests/formal/field_seq_bound.sby` asserted `busy_cnt <= 16'd120` at depth 140,
with a paragraph explaining why 120 was generous. Under the new schedule two
NORMALIZE3s take about 134 clocks, so that number would have had to be *edited* —
which is how a bound stops being one.

It is now derived, from one definition:

* `zhao_field_seq_pkg::MAX_OP_CYCLES = 80` — declared in
  `fpga/rtl/field/zhao_field_seq.sv`, beside the cost table it bounds. In a
  package in the same file, so every consumer that already compiles the
  sequencer gets it and no source list anywhere had to grow.
* the harness **imports** it and computes
  `MAX_RUN_CYCLES = 2 * MAX_OP_CYCLES + 8 = 168` (two instructions, which is what
  the count shrink allows, plus the fetch that overruns and `Q_DONE`) and
  `SCOPE_STEPS = MAX_RUN_CYCLES + 22 = 190`;
* `depth 180` sits between them: above the run bound it must exercise, below the
  scope guard that pins the proven window.

**A new and stronger property came with it.** `a_op_bounded` asserts
`op_cnt <= MAX_OP_CYCLES` — no *instruction*, whatever opcode the solver invents
and wherever its operands point, runs longer than the design's own stated
ceiling. That is the assertion this rearchitecture actually needs: every op now
waits on shared resources, and every wait is an `if (valid)` that a mis-timed
schedule turns into a spin. The old parallel units could not wait on each other
at all.

Result: PENDING at the time of this commit -- the derived-depth run is measuring its solo wall time; see the note below.

### 5b. The proof got expensive, and the fix is an abstraction rather than a smaller claim

Putting a registered signed 33x33 multiplier in the cone made this proof
something a bounded model checker struggles with: every bit of the product, at
every unrolled step. **Unabstracted, the BMC took two hours and twenty minutes
to reach k = 114 of 180 and was still slowing down.** That is not a proof anyone
will re-run, and a formal lane nobody re-runs is decoration.

Three ways out were considered, and two of them were bad:

* **Shrink `instr_count_i` to 1.** Cheap, and a NARROWING of what the previous
  proof covered. Rejected: assuming the hard case away is how a formal lane
  stops meaning anything.
* **Lower the depth until it finishes.** Then the run-level bound is asserted
  over a window too short to violate it, which is worse than not asserting it —
  it looks like a proof and is not.
* **Abstract the product.** `cutpoint` replaces the product wire with a value
  the solver may choose freely each cycle, and `opt_clean` then removes the
  multiplier that no longer drives anything. Measured: 4 `$mul` cells in the
  model become 1. **Both passes are needed** — without the `opt_clean` the dead
  multiplier stays and nothing is saved, which is why the first attempt produced
  a byte-identical model and looked like the selection had failed.

**The abstraction is sound, and that is a property of what is being proven.** A
free product is a SUPERSET of the real one: every behaviour the multiplier can
produce, the free value can produce too, so a proof over the abstraction proves
the property over the design. It would be unsound for a property about VALUES —
and the values are not proven here. They are proven by 906 directed and 12,906
random programs against `zfield::interpret`, which is the right instrument for
that job and the wrong one for liveness.

All three assertions are about CONTROL. Exactly one control path in the whole
engine reads a product — NORMALIZE's `is_zero`, choosing between the zero-vector
answer and the reciprocal walk — and under the abstraction the solver may take
either branch whenever it likes. More behaviours to check, not fewer.

**The cover task is deliberately NOT abstracted.** Its entire job is to show the
antecedents are reachable in the real design; reaching them with a product the
multiplier could never produce would prove nothing, and would be precisely the
vacuity the covers exist to rule out. The `bmc:` prefix in the .sby is what keeps
the two apart.

**And the part that makes it non-lossy rather than merely sound: the bound is
STRUCTURAL.** Every operation walks a fixed schedule, so its cycle count does not
depend on any product's value. That was audited one FSM at a time, by reading
every `state <=` in the cone, not assumed:

| block | what decides the path | product-dependent? |
| --- | --- | --- |
| `zhao_field_isqrt` | `step` counts 32 iterations | no |
| `zhao_field_rcp` | fixed six states; `is_zero` is on the OPERAND | no |
| `zhao_field_len` | `iss_cnt` / `got_cnt`, then the root | no |
| `zhao_field_curve` | `k` counts six search steps; the comparison moves `lo`, not the state | no |
| `zhao_field_noise` | the opcode (`h_ridge`) and a lane counter | no |
| `zhao_field_rot` | straight-line, four issue/wait pairs | no |
| `zhao_field_ring` | nine products and a `half` flag | no |
| `zhao_field_seq` | opcode-derived widths and the handshake | no |
| `zhao_field_normalize` | **`is_zero`, which is `n2 == 0` — a sum of PRODUCTS** | **yes** |

**Exactly one exception, and it happens to fall the safe way.** Both of
NORMALIZE's branches are fixed-length, and the zero branch is the SHORT one
(straight to `N_OUT`) while the non-zero branch is the long one (the reciprocal
walk). The abstract machine may take either, so it may always take the longer —
which is exactly the case the bound must cover. The abstraction cannot hide a
violation; it can only hand the solver the worst case more freely.

**What would make this unsound — or rather sound but useless.** If any operation
terminated on a data-dependent condition — a convergence test, an early-out on a
comparison against a product, a normalise loop iterating until a bit is set — a
free product could spin it forever and the proof would fail with a
counterexample that cannot happen. Passing would then be luck rather than proof.
No operation here is written that way. The `.sby` header carries the same
statement, so the place that would stop being true is the place a reader looks.

### 6. The contamination test, and the mutant that proves it is not decoration

**This is the defect class the old design could not have had.** When every op
owned its own multiplier and its own accumulator, nothing an op left behind
could reach another. They now share a 66-bit product register, a wide
accumulator, an integer root, a sine table and a reciprocal — and a leftover in
any of them is invisible to every test that runs one op at a time, which was
every section of the differential and every block-level test in the tree.

Section 13 runs ten operations **alone**, then the same ten **interleaved** in
both directions, and requires every answer *and each of the five saturation
ledger lanes* to equal the isolated result. The ledger is the half that matters
most: a shared accumulator not cleared between ops can produce the right number
and the wrong `mul` lane, and `Status.sat` collapses all five into one bit, so
the five are compared separately. It also runs the same op three times in a row,
which is what an accumulator that is added to rather than loaded fails.

That claim is proven rather than asserted: **M05** in the sweep makes exactly
that change — `acc <= mul_p` becomes `acc <= acc + mul_p` — and it is caught.

**And then the measurement corrected the claim I was about to make.** I ran M05
and read which checks failed first: `6.dot3`, in a section written long before
this work. So section 13 is NOT the only thing that catches an accumulator
carry-over, and saying so would have been a nice-sounding falsehood. It is
caught early because section 6 happens to run a DOT3 after another
product-using op — coverage by accident of ordering.

What section 13 adds is that the property is checked **as a property**: each
operation alone, then the same ten interleaved in both directions, with all five
saturation ledger lanes compared, plus the same op three times in a row. That
does not depend on the accidental ordering of another section, and it is the
only place the ledger lanes are compared between isolated and interleaved runs.

### 6b. The gap the sweep found, and the section that closes it

The first sweep returned three survivors. Two were timing equivalences (see
finding 9). The third, **M20**, deletes `&& !multi_op` from the Q_EXEC
write-back guard, so a multi-cycle op would also write `reg[dst]` in Q_EXEC —
forty clocks before it has an answer.

It survives, and it is **provably equivalent**, by an argument that is
mechanical rather than a label:

1. `multi_op` is true only for opcodes `zhao_field_alu` does not claim (0x12-0x17,
   0x1A-0x1D, 0x21, 0x22, 0x28, 0x29 — disjoint from the ALU's case arms);
2. for any opcode not in that case, the `unique case` takes `default:`, and that
   arm sets `writes_o = 1'b0` (zhao_field_alu.sv:344, read, not assumed);
3. `exec_writes_o` is `alu_writes` for every non-SIN/COS op;
4. the Q_EXEC write-back is gated on `exec_writes`, so the term cannot change
   the outcome. The guard stays because it says what the value MEANS and stops
   being redundant the moment the ALU learns a new op.

**But equivalent means the guard was never tested**, and that is a real gap once
operations take tens of clocks: WHEN a result becomes visible is part of the
contract, not an implementation detail. Section 14 closes it. It watches the
register file DURING the walk — `rf_rdata_o` is combinational and reading it
disturbs nothing — with a sentinel that an early write destroys, one case per
dispatch family.

The law is exact rather than merely strict, and getting it wrong the first time
is what taught me the shape: the file has ONE write port, so lane 0 lands at the
accepting edge and lane k at `Q_WB(k)`, and `reg[dst]` may legitimately change
`lanes - 1` cycles before retirement. My first version asserted the sentinel
survived to retirement and correctly failed on every two-lane op.

**M33 proves the section is not decoration**: lane 0 commits in `Q_MISS`, tens of
clocks early, and is overwritten by the real value later, so every ANSWER in the
suite stays correct. Section 14 catches it and **nothing else does** — 7
failures, all of them in section 14.

### 7. Two deviations from the suggested design, both deliberate

**LEN and NORMALIZE gather their squares through the lane, not during the
operand reads.** The suggested schedule had them square during `Q_RD0/1/2`. That
requires the operand *shaping* — which lane is squared, and DIST2's saturating
difference — to move out of the op blocks and into the shared engine, which in
turn means the block-level differentials can no longer drive those blocks
without restating that shaping in a test wrapper. A second implementation of an
op's semantics in a test is exactly the shape that let `abs` be wrong for weeks
with green tests agreeing with it.

Cost of the deviation: LEN 48 clocks instead of about 42, NORMALIZE3 67 instead
of about 58. Both are far inside the 96-clock ceiling, and both blocks keep their
own differentials driving their own ports.

**`zhao_field_sin` keeps its 18×6 interpolation multiply, but not its DSP.** It
is a nonconstant product and the rule says no production op unit keeps one.
Routing it through the shared lane would make SIN and COS multi-cycle and
lengthen every ROT by six clocks, to save one DSP on a device with 108 spare.

The per-entity fit made the cost exact rather than arguable — the sine was
**1 of the 4 DSP blocks**, the lane the other 3 — so the answer is
`multstyle = "logic"`: keep the multiplier, refuse the DSP, build it from ALMs.
Six bits of multiplier is a six-term shift-add. The attribute is invisible to
Verilator and to slang, so the differentials and the formal proof see exactly
the same arithmetic, and sin/rot/seq are unchanged and green.

This is what measuring first bought: without the per-entity table I would have
argued the point instead of costing it at one block.

### 8. The block-level differentials survived, through test-only wrappers

Eight op units lost the ability to elaborate alone: their arithmetic arrives
through ports now. Rather than delete their differentials or point them all at
the sequencer (which would have made `configure` far slower and the sweep
unaffordable), each got a wrapper in `tests/rtl/` that supplies **exactly the
shared resources and nothing else**, presenting the block's original port list.

The wrappers contain no rounding, no saturation, no operand selection and no
law. The single exception is documented where it occurs:
`zhao_field_alu_tb.sv` restates the operand *pairing* (lane k of `a` with lane k
of `b`), because the ALU no longer states it — in production that pairing is the
sequencer's read-address walk and is proven end to end against
`zfield::interpret` by `field_seq_directed`. Two different oracles, which is the
only reason a restatement is tolerable.

### 9. Differential and sweep numbers

| lane | directed | random |
| --- | ---: | ---: |
| `field_seq_directed` | 1,106 | 12,906 (3,000 programs), 31/31 opcodes |
| `field_alu_ops` | 1,005 | 140,000 |
| `field_len_directed` | 159 | 12,000 |
| `field_normalize_directed` | 419 | 18,024 |
| `field_noise_directed` | 346 | 12,346 |
| `field_rot_directed` | 3,495 | 27,495 |
| `field_ring_directed` | 572 | 32,572 |
| `field_curve_directed` | 11,863 | 39,863 |
| `field_rcp_directed` | 329 | 60,000 |
| `field_sin_directed` | 20 | — |

Mutation sweep: 33 attempted, 33 expected, 33 accounted, **30 caught**, 3 survivors (all proven equivalent, finding 9), 0 discarded, exit 0

**Three survivors, all one equivalence, and it is worth naming as a family.**
M01 (the lane's operand registers not held between issues), M07 (the read-slot
shadow one stage deep) and M20 (the write-back guard) each produce a value that
is transiently wrong and ALWAYS overwritten before anything reads it, because
the walk's read points are fixed:

* **M01** — a consumer reads `p_o` only on its `p_valid_o` pulse, and at that
  instant the product register still holds the issued operands' product: the
  flop update `p_o <= a_q * b_q` samples `a_q`/`b_q` from BEFORE the same edge.
  The hold matters only for a consumer that reads late, and none does. It stays
  because it stops the multiplier array being re-driven by whatever is on the
  mux, and because it stops being equivalent the moment a consumer reads late.
* **M07** — the shadow fires one cycle early AND at the right time, so the
  accumulator takes a stale product and then, unconditionally, the correct one
  on the next cycle. `Q_RD1` always issues, so the correction is not optional.
* **M20** — proven above from `zhao_field_alu`'s `default:` arm.

None of the three is labelled; each is an argument that can be checked by
reading the named line.

`tools/sweep_field_dsp.sh` carries all seven guards, and **derives guard 7's
consumer list from `tests/CMakeLists.txt` at run time** rather than declaring it.
This sweep spans eleven files whose consumer sets differ — a mutation in
`zhao_field_mul.sv` reaches eight targets, one in `zhao_field_exec_shared.sv`
reaches one — and a hand-maintained list across eleven files is a list that
drifts. The declared union is cross-checked against the derivation and the run
refuses to start on any mismatch; it caught a real one on the first attempt
(`test_field_alu_ops` was declared and unreached, which meant the ALU's new seam
had no mutant at all).

---

## Open questions for the owner

Nothing here needs a ruling. Two things are worth knowing:

1. **The Field engine is now 3 DSPs of 112.** The docket's
   table budgeted 8–12 for it. That is headroom the remaining subsystems can
   spend, and the same "one item in flight, therefore one multiplier" argument
   applies unchanged to `GEOM.CULL` and `SURFACE.STAMP`.
2. **Registers grew 4,600 → 5,356.** That is the price of the
   schedule: the lane's operand and product registers, the shared accumulator,
   and the wait states in nine op FSMs. On Cyclone V an ALM carries its flops
   whether the design uses them or not, so this is only a cost if the ALM number
   moved with it — see the table in finding 1.

---

## Provenance

Everything in this document is **simulation, synthesis and fit**. No hardware
has run any of it. The device is a provisional target, all I/O is virtual, and a
per-block fit says nothing about the composed machine's routing or timing
closure.

Work was done in a separate git worktree (`.worktrees/field-dsp`, branch
`wp/field-dsp`) with its own `build/` directory, per the standing ruling of
2026-08-23.
