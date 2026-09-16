# G8B timing campaign â€” scope, packages, and what each one buys

Written immediately after G8B's first measurement, in the shape that made the
Timing4 campaign efficient: name the packages, name what each one is worth in
nanoseconds, name the one fit at the end, and write down the risks before
anyone starts rather than after.

**Baseline:** `zhao_terrain_pipe_rpp3_matw18_fit_top@g8b`, commit `968243b5`,
clean tree, seed 1. **43.94 MHz**, setup WNS âˆ’12.758 ns, setup TNS
âˆ’7,360.359 ns, 2,000 of 2,000 exported paths negative. 7,424 ALMs, 8,070
registers, 34 DSP, 44 RAM blocks, 95,610 memory bits. Hold clean at +0.251/0.

**Target:** 100 MHz, the machine's operating requirement. That needs
**+12.758 ns** on the worst path â€” not the +0.587 ns G8A's last mile needed.

## STATUS: `@g8b-t1b` IS MEASURED. 43.94 -> 73.59 MHz, and T1b MET its acceptance.

`@g8b-t1b`, clean commit `4a33a786`, seed 1, 449.2 s. **Fmax 73.59 MHz**, **ALM
7,807** — which is 34 LOWER than `@g8b-t12`'s 7,841 despite T1b adding a
pipeline stage, a fourth vertex slot and a second triangle slot. Registering the
product let the fitter drop logic elsewhere; the register cost was real and the
ALM cost was not.

The campaign so far, all from clean committed trees at seed 1:

| row | Fmax | ALM | worst block | worst slack |
|---|---:|---:|---|---:|
| `@g8b` | 43.94 | 7,424 | `zhao_terrain_tess` | −12.758 |
| `@g8b-t2` | 43.54 | 7,531 | `zhao_terrain_tess` | −12.968 |
| `@g8b-t12` | 57.87 | 7,841 | `zhao_terrain_tess` | −7.280 |
| **`@g8b-t1b`** | **73.59** | **7,807** | `zhao_terrain_tess` | **−3.588** |

**+29.65 MHz on the subsystem, for +383 ALMs.** T1b's acceptance was "better
than −4.109 ns, where `zhao_project_core` sits"; −3.588 meets it.

**AND THE PATH HAS CHANGED CHARACTER, which is the real result:**

```
u_tess|j_s[0] -> u_tess|Mult1~8|ENA_DFF0     -3.588 ns, data 13.705
```

That endpoint is the multiply's **ENABLE**, not its data. `j_s` is the job
stride register. The blend arithmetic this campaign has been cutting since T1 is
no longer the constraint at all — what is left in the tessellator is a CONTROL
path into the DSP, which is a different problem needing a different fix, and
nothing in this brief so far is about it.

**Where the subsystem now stands against 100 MHz** (the constraint is T = 10 ns,
so a block's ceiling is 1000/(10 + |slack|)):

| endpoint block | slack | data | ceiling |
|---|---:|---:|---:|
| `zhao_terrain_tess` | −3.588 | 13.705 | 73.6 MHz |
| `zhao_project_core` | −2.449 | 11.870 | 80.3 MHz |
| worst inferred RAM | −2.070 | 11.957 | 82.9 MHz |
| next nine RAMs | −0.9 … −0.57 | 10.1–10.4 | 91–94 MHz |

**Nothing is far away any more, and nothing is close enough.** Ten of the
twelve worst endpoints are now within 2.1 ns of target and the RAM family sits
in a tight band just over 10 ns of data delay, which is a different kind of
problem from a single 22 ns chain: it will not yield to one cut. T3 must cover
the tessellator's control path, `zhao_project_core`, and the RAM band together,
and it should be scoped from this export rather than from the T2-era plan below,
which was written when the projector and the RAMs were 0.5 ns apart and both
far behind the tessellator.

### What the tessellator's new worst path actually is, cell by cell

Read from the export rather than guessed, because the endpoint name alone
("the multiply's enable") suggests the blend and it is not the blend:

```
j_s[0]
  -> Mult1~8|ay[0] -> Mult1~8|resulta[3]     a MULTIPLY, cell index x stride
  -> Add7~21|sumout                          the lattice coordinate
  -> LessThan18~0|combout                    a bound compare
  -> Equal0~26 -> Equal0~27 -> Equal0~28     the run-cell / mode decode
  -> cell_skip|combout                       "is this run-cell void?"
  -> eb[0]~1|combout                         the ENUMERATOR ADVANCE
  -> Mult1~8|ena[0] -> ENA_DFF0
```

**This is the enumerator, and it closes a loop through the same DSP.** `j_s` is
the job stride; the chain computes the current cell's lattice coordinate,
decides whether the cell is void and must be skipped, advances the enumerator —
and the result gates the geomorph multiply's own clock enable. Data out of the
DSP, round the control logic, back into the DSP's enable, in one cycle.

**It is long for a reason the block states as a virtue**, which is why this is a
design question and not a tidy-up. `zhao_terrain_tess`'s own header says *"THE
ENUMERATOR ADVANCES AT ISSUE, NOT AT CAPTURE. That is what keeps the pipe at
three cycles per triangle — one lattice read per clock... Advancing at capture
would insert a bubble."* The tightness is deliberate and it is what protects the
initiation rate. Any cut here has to keep that rate, which means precomputing
the next cell's coordinates and its skip decision a cycle ahead rather than
simply registering `cell_skip` — registering it is the bubble the header
forbids.

**So T3 has three sub-problems, not one chain**, and the third is not what its
endpoint names suggest.

### T3's three cones, named from the export

**(a) The tessellator's enumerator loop — 73.6 MHz.** Above.

**(b) `zhao_project_core`'s output stage — 80.3 MHz.**

```
s6_prod_x[38] -> out_x_o[7]        -2.449 ns, data 11.870
```

The viewport transform's final product into the output register. 1,496 of the
2,000 summarised endpoints are in this block and the worst several all leave
`s6_prod_x`. This is a DIFFERENT cone from T2's, which registered the row
products at `s1`; T2 is not undone by it and does not help it.

**(c) THE "RAM BAND" IS NOT A MEMORY PROBLEM. It is the divider's delay lines.**

Every one of the twelve worst RAM endpoints is a Quartus-inferred
`shift_taps_*` (ALTSHIFT_TAPS) instance, not a design array — and **all twelve
launch from the same node**, `s2_cw[25]~DUPLICATE`:

| endpoint | owner | worst |
|---|---|---:|
| `altsyncram_ofc1` | `shift_taps_nuv` | −2.070 |
| `altsyncram_2gc1` | `shift_taps_kuv` | −0.901 |
| `altsyncram_kfc1` | `shift_taps_luv` | −0.734 |
| … nine more | `shift_taps_*` | −0.68 … −0.27 |

`s2_cw` is the clip-space **w** in `zhao_project_core` (line 730). It feeds
`pre_d = s2_cw[30:0]` and `pre_d2 = pre_d[30:1]`, the divisor of the long
division. So these are the DIVIDER PIPELINE's delay registers, which Quartus
chose to implement in M10K shift registers, and the constraint is one bit of the
divisor fanning into a dozen of them.

**Reading this as "the inferred RAMs are slow" would send the next pass to look
at memory inference, which is the wrong component entirely.** The candidates are
duplicating `s2_cw`'s drivers per consumer (Quartus already made one
`~DUPLICATE` on its own), restructuring so the delay lines are not all gated
from one bit, or turning shift-register recognition off for this cone so the
taps become flops — which trades registers for routing and needs measuring, not
assuming.

Reaching 100 MHz needs (a), (b) and (c). None of them is the chain this
campaign has been cutting since T1.

## Superseded: `@g8b-t12`, 43.54 -> 57.87 MHz, and T1 MISSED ITS OWN ACCEPTANCE.

`@g8b-t12`, clean commit `834181eb`, seed 1, 488 s. **Fmax 57.87 MHz**, ALM
7,841 (+417 over `@g8b`'s 7,424), registers 7,997 -> 8,452, RAM 44 unchanged.
`failed:structure` is the 100 MHz rule refusing the row, not a failed
measurement: `rtlCleanAtHead` is true and the digest is real.

**+14.3 MHz is real and it is not the ~69.5 MHz this brief predicted.** The
prediction is refused by the measurement, and the reason is specific:

| endpoint block | `@g8b` | `@g8b-t2` | `@g8b-t12` | ceiling now |
|---|---:|---:|---:|---:|
| **`zhao_terrain_tess`** | −12.758 | −12.968 | **−7.280** | **57.9 MHz** |
| `zhao_project_core` | −9.811 | −4.388 | −4.109 | 69.1 MHz |
| inferred RAMs | −5.328 | −4.058 | −3.619 | 71.6 MHz |

**THE TESSELLATOR IS STILL THE WORST BLOCK**, and the path is stage B alone:

```
u_tess|lnd_morph_q[0] -> u_tess|vo_y[7]     -7.280 ns, data 16.383
```

The split worked -- 22.481 ns of data delay became 16.383, a genuine 6.1 ns --
but **this brief estimated stage B at "roughly 13 ns, which clears T1's
acceptance of better than -4.388 ns", and it is 16.4.** That estimate was made
by reading the expression and counting operators, and it was optimistic by about
3.4 ns. T1's stated acceptance is NOT met. The honest statement is that T1 is a
large partial win, not a completed package.

**What the numbers now say to do.** The remaining 16.383 ns is one multiply plus
one rescale plus one saturating add, and the multiply's DSP OUTPUT REGISTER IS
AGAIN UNUSED -- the same finding T2 cashed in `zhao_project_core`, in the block
next door. Registering `m_prod` splits stage B roughly 10 / 6 and should put the
tessellator behind `zhao_project_core`, whose −4.109 then caps the subsystem at
about 69 MHz. That is T1b below, and only after it does T3 (project_core's
residual and the RAM paths, now 0.5 ns apart) become the binding package the
plan assumed it already was.

## T2 IS LANDED AND MEASURED.

`@g8b-t2`, clean commit `3aea9b7d`, seed 1, 497.2 s. Fmax **43.54 MHz** —
essentially unchanged, exactly as the ceiling table below predicted, because
the tessellator still caps it. What moved is the thing T2 claimed:

| endpoint block | before (`@g8b`) | after (`@g8b-t2`) | ceiling |
|---|---:|---:|---:|
| `zhao_terrain_tess` | −12.758 ns | −12.968 ns | 43.5 MHz |
| **`zhao_project_core`** | **−9.811 ns** | **−4.388 ns** | **69.5 MHz** |
| inferred RAMs | −5.328 ns | −4.058 ns | 71.1 MHz |

**+5.42 ns on the projector cone** (data 19.043 → 13.685), for +107 ALMs and no
DSP change. Its ceiling moved 50.5 → 69.5 MHz and it is no longer the second
constraint — it now sits level with the RAM paths.

This fit was worth spending despite the brief's own advice below, and the
reason is worth recording: it could not move Fmax, but it was the only way to
confirm the cut did what it was designed to do BEFORE T1 was built on the
assumption that it had. The prediction was "Fmax unchanged, projector cone
improved"; both halves came true, which is what makes the remaining plan
trustworthy.

**Revised outlook.** T1 alone now takes the subsystem from 43.5 MHz to roughly
**69.5 MHz** — no longer the 6.6 MHz the original table predicted, because T2
has already cleared what was behind it. After T1, `zhao_project_core`'s residual
−4.388 and the RAMs' −4.058 are level, so T3 becomes a single package covering
both rather than two in series.

## The ceilings, and why the order is not the obvious one

| # | endpoint block | negative rows | worst | ceiling |
|---|---|---:|---:|---:|
| 1 | `zhao_terrain_tess` | 160 | âˆ’12.758 ns | 43.9 MHz |
| 2 | `zhao_project_core` | **1,631** | âˆ’9.811 ns | **50.5 MHz** |
| 3 | inferred RAMs (`altsyncram_*`) | ~130 | âˆ’5.328 ns | 65.4 MHz |

Each fix only exposes the next. **No single package reaches 100 MHz, and fixing
the worst path first buys 6.6 MHz.** That is the whole reason this is a
campaign and not a patch.

---

## T1 â€” the tessellator morph blend

**Evidence.** Worst path âˆ’12.758 ns, data 22.481 ns, skew âˆ’0.097 ns: logic
depth, not placement. From the wrapper's registered lattice response,
`zhao_terrain_tess` runs one combinational chain:

```
lat_h_i -> m_dab (34-bit sub) -> rescale1 -> fx_add_sat -> m_hc
        -> m_d (34-bit sub) -> j_morph * m_d (17x34 signed multiply)
        -> rescale16 -> fx_add_sat -> m_y -> vs_y / vo_y
```

**What it owns.** One or two registers inside that chain, and the landing that
has to travel with them.

**The risk, which is the real cost.** `last_y` is consumed in six branches of
the ModeVtx landing logic (`vland && vpop`, `vland` with and without
`vo_valid`, `vpop` with and without `vs_valid`). Registering the blend delays
the LANDING, not a value: `pend_idx`, `pend_stride` and `pend_slot` travel with
it, and the `vtx_room` credit â€” "a landing never finds both slots full without
a pop" â€” is argued against the current timing and must be re-argued.

**What does NOT work, checked:** the trick that fixed RASTER.RESOLVE. There,
half the arithmetic moved to the cycle the memory response arrived. Here `m_hc`
depends on `lat_h_i` itself, so there is nothing upstream of the response to
move work into. A stage is required.

**Acceptance, REVISED after T2 landed.** Worst tess path better than -4.388 ns,
which is where `zhao_project_core` and the RAM paths now sit together. Exact
arithmetic unchanged: `terrain_pipe_differential` stays bit-exact against
`zhao_terrain_project`. Its 478 packets over 2,343 cycles will move; the
**rate** must not.

**T1 is now worth 43.5 -> ~69.5 MHz on its own**, not the 6.6 MHz the original
ceiling table predicted. T2 cleared what was behind it, so this package is no
longer buying six megahertz before hitting the next wall -- it is the single
thing standing between this subsystem and roughly 69 MHz.

**Design sketch, from reading the block rather than guessing.** The chain must
be split and the landing must move with it, because `m_hc` depends on the
kind-2 lattice response itself and there is nothing upstream to move work into:

* **stage A** (the kind-2 response arrives): `m_dab`, `rescale1`, `fx_add_sat`
  -> `m_hc`, then `m_d = m_hc - vh[slot]`. Register `m_d` with `lat_wx_i`,
  `lat_wz_i` and the `pend_*` metadata. For kinds 0 and 1 this stage carries
  the response and metadata through unchanged.
* **stage B**: `j_morph * m_d`, `rescale16`, `fx_add_sat` -> `m_y`, and the
  landing happens here.

Three consumers move with it and all three must be handled together:
`vy[pend_slot] <= m_y` (the kind-2 capture), `last_x/y/z` into the ModeVtx
skid, and `last_x/y/z` into the ModeTri triangle emit (`o_bx`/`o_cx` and the
underside swap). The `vtx_room` credit -- "a landing never finds both slots
full without a pop" -- is argued against the current timing and must count the
new in-flight stage, or the arena reports a fill fault exactly as the G8B
wrapper's first stimulus did.

### T1 IS IMPLEMENTED. Four things the sketch above did not foresee.

All four were found by a test, none by reading, and each is worth carrying into
the sequenced-branch cut that still owes the same treatment.

1. **THE JOB'S OWN PARAMETERS HAVE TO TRAVEL WITH THE VERTEX.** `j_morph`,
   `j_surface` and `j_src` are JOB registers. Read them at stage B and a vertex
   whose job has since been replaced is blended with the NEXT job's morph factor
   and emitted under the next job's winding. `terrain_tess_directed` reported the
   same blended `y` for morph factors that must differ -- every vertex was using
   its successor's. The sketch listed the three *value* consumers and missed the
   three *parameter* ones.

2. **THE SNAPSHOT NEEDS A WRITE-FORWARD, and this one nearly shipped.** `vy[]`
   is the one corner field the blend writes, and since the split that write
   happens at stage B -- the same edge stage A snapshots `vy[0]`/`vy[1]` on. A
   slot whose blend completes on that edge is captured at its stale pre-blend
   value. `terrain_tess_directed` caught it as a wrong `b` corner with a correct
   `a` and `c`. The first hypothesis was wrong (`j_morph` read at stage B) and
   the tell that it was wrong is worth recording: **the output was byte-identical
   after the "fix", and the change was verified to be in the build.** A repair
   that changes nothing did not repair anything.

3. **THE DRAIN CONDITIONS BOTH NEEDED THE NEW STAGE.** `StTri`'s exit and
   `idle_o` each enumerate what is in flight, and leaving either alone strands a
   vertex in stage B: the triangle is never emitted and the job never drains.

4. **COUNTING THE NEW STAGE IN THE CREDIT IS NECESSARY AND NOT SUFFICIENT.**
   This is the one the sketch got half right, and the half it missed cost the
   rate rather than correctness. The credit was extended to reserve for both
   in-flight vertices, which is the exactly-minimal invariant --

   ```
   occupancy_next + in_flight_next <= DEPTH
   ```

   -- and with DEPTH still 2 it is **never satisfiable in steady state**, because
   a read already issued cannot be told to wait: its lattice response arrives on
   the next edge whatever the consumer is doing. With the consumer ALWAYS READY,
   `cnt=1, land=1, land_a=1, pop=1` gives `2 > 1`, so a bubble every other
   vertex. `terrain_tess_modes_directed` measured it exactly: **128 cycles for 81
   unstitched vertices against a budget of 93**, on all three of its ModeVtx rate
   checks and on nothing else.

   **The buffer must be DEEPER than the number of vertices in flight.** The skid
   is now three slots, `vtx_room` is `vnxt <= 2`, and the same steady state is
   `2 <= 2` -- 88 cycles for 81 vertices, one per clock again. The cost is one
   more `{x, y, z, idx, stride}` register set, and it is the honest price of the
   stage: latency may grow, the initiation rate may not.

   The trap here is that the failure is a RATE failure with correct values.
   `terrain_tess_directed` (6,751 checks) and `terrain_pipe_differential` (37,
   bit-exact) were both green while this was live, because neither asserts
   cycles. Only the mode test counts them -- which is what CLAUDE.md's *counters
   see what pictures cannot* says, arriving from the other direction: here the
   picture was right and the counter was the whole finding.

### A FIFTH thing, found only by the bitmap mode: the overlap check is coverage

`terrain_pipe_differential_bitmap` went red on *"next job accepts and reopens
arenas while an older copied output remains stalled"*, reporting 0. That check
asserts the CROSS-JOB PIPELINING property, and a bare `expected 0x1, got 0x0`
cannot distinguish "the machine stopped overlapping" from "this stimulus
stopped reaching the overlap". The first costs an investigation of the wrong
component, so both counters are now PRINTED beside the cycle count.

They were marginal before T1 and nobody knew: each mode reached the state
**exactly once per run**, by the periodic `cycle%13` stall pattern happening to
be low on the cycle `job_ready_o` rose. That is a coincidence, not stimulus.

**The wrong fix is recorded because it is the tempting one.** Holding
`out_ready_i` low until `job_ready_o` rises guarantees an output is pending at
the handoff -- and it BACKS PRESSURE UP THE PIPE: the replay output cannot
drain, so the arena is not released, so the tessellator cannot push its vertices
and never reaches StIdle, and StIdle is exactly what drives `job_ready_o`. The
stimulus meant to reach the state is what prevents it. Measured: still 0 and 0.

Gating the JOB OFFER instead (withhold `job_valid_i` until an output is pending)
worked with T1 and **failed at HEAD** -- the opposite direction to the periodic
pattern. Two stimuli that each work on one tree are two coincidences, not a
gate. The committed driver does both, with a short armed output stall bounded at
14 cycles so it cannot jam, and it reaches the state on both trees:

| tree | bitmap cycles | coverage | dense cycles | coverage |
|---|---:|---:|---:|---:|
| HEAD | 2,442 | 1 / 1 | 2,552 | 4 / 6 |
| T1 | 2,446 | 2 / 3 | 2,563 | 2 / 3 |

**So T1 does not cost the overlap**, which is the question the red was actually
asking. Both budgets are bounded deliberately: a pipe that genuinely could not
overlap now FAILS the check rather than hanging the driver, and a red assertion
is a far better diagnostic than a timeout.

**Result.** `terrain_tess_modes_directed` 33/33, `terrain_tess_directed`
6,751/0, `terrain_pipe_differential` 37/37 and `_bitmap` 33/33, on the numbers
above. Fitted as `@g8b-t12`: **57.87 MHz**, and T1's acceptance NOT met — see
the status section at the top and T1b below.

---

## T1b — the multiply's output register, which was again unused

**Evidence.** `@g8b-t12`'s worst path is stage B on its own:

```
u_tess|lnd_morph_q[0] -> u_tess|vo_y[7]     -7.280 ns, data 16.383
```

One 17×34 signed multiply, one `rescale16`, one `fx_add_sat`, nothing
registered between. **The DSP's output register is unused** — the identical
finding T2 cashed in `zhao_project_core`, in the block next door, and worth
stating plainly: the same unused output register sat in two blocks of one
subsystem, and reading one of them did not make anyone look at the other.

**What it does.** `m_prod` is registered. Stage B computes the multiply and
nothing else; the new stage C does `rescale16` + `fx_add_sat` and owns the
landing. `rescale16` and `fx_add_sat` are untouched and applied in the same
order to the same values, so `terrain_pipe_differential` stays bit-exact.

**What it costs, and the two rules it inherits from T1.**

* **A third in-flight stage, so the queue goes to FOUR slots.** T1 established
  that the buffer must be deeper than the number of vertices in flight, because
  a read already issued cannot be told to wait. `vtx_room` becomes `vnxt <= 3`
  and the steady state is `1+1+1+1-1 = 3 <= 3`.
* **The write-forward moves with the landing, and now needs TWO copies.**
  `vy[]` is written at stage C, and both the A→B and B→C snapshots are taken on
  edges where that write can land. Keying the forward on `lnd_*` after the
  landing moved would forward a value no longer being written there and miss
  the one that is — wrong in both directions at once. The worked case is a
  morphing slot 1 followed by a non-morphing slot 2: slot 1's blended `y`
  reaches `vy[1]` one cycle AFTER slot 2's A→B snapshot, and only the B→C
  forward catches it.

**The three named skid slots became a queue.** At four, the hand-written
land / pop / land-and-pop arms stop being readable — three slots already needed
a nested three-way shift inside each of three arms. `vq_*[0]` is the head, a pop
shifts down, a landing writes the post-shift occupancy, and because the
landing's assignment comes second it wins on the index they share, so
land-and-pop needs no arm of its own. **Ordering semantics are unchanged; only
the spelling is.** The narrowing of the landing index is asserted rather than
assumed: a landing at index `VQ_DEPTH` would wrap to 0 and overwrite the head,
which would present as a corrupted vertex rather than as an overflow.

**Acceptance.** Better than −4.109 ns, which is where `zhao_project_core` now
sits — that is what "the tessellator is no longer the constraint" means, and it
is deliberately stated against the MEASURED neighbour rather than against an
estimate of this block, which is exactly how T1's acceptance came to be missed.

---

## T2 â€” the projector's arbitration-to-datapath path

**This is the big one: 1,631 of the 2,000 negative endpoints.**

**Evidence.** Worst path âˆ’9.811 ns, data 19.043 ns, from `u_tess|vo_valid` to
**`zhao_project_core|s1_ry[53]`** â€” stage 1's row sum. The nine MATWÃ—32 row
multiplies and their sum happen in one cycle, with the service's operand mux in
series ahead of them.

**The package is: pipeline stage 1's row sum.** The matrix is read at stage 1's
input, on the accept edge, so splitting the sum *after* that read leaves the
capture-at-accept law intact â€” which the first attempt at this package did not,
and see below.

The service's arbitration is in the path but is not its length:

```systemverilog
wire both_c  = a_valid_i && b_valid_i;
wire grant_a = a_valid_i && !(both_c &&  prefer_b_q);
wire take_a  = en_i && grant_a && core_ready;
wire signed [31:0] core_vx = take_a ? a_vx_i : b_vx_i;   // and vy, vz, view, payload
```

A client's valid arrives, runs through round-robin arbitration, and the result
selects the operands that feed the projector's row-sum arithmetic â€” all in one
cycle. The datapath launches from a control decision made from a far-away
valid.

**What it owns.** Registering the arbitration decision and the selected
operands so the core launches from flops. `zhao_skid2` already exists and is
proven; a two-entry skid per client is the obvious shape.

**The risk.** `a_ready_o = take_a` is ready-depends-on-valid by construction.
A skid terminates that, which is the point, but it changes the handshake the
clients see, and **`proj_rowmux_directed` pins the declared fixed-latency delta
at +3 en-cycles and the initiation interval at exactly 3.** The architecture
rule permits latency to grow and forbids the rate to regress, so II=3 is the
line; the +3 becomes +4 or +5 and that test's number is re-derived, not relaxed.

**And it is not terrain-only work.** `zhao_project_core` is the shared
projector: `GEOM.PROJECT` and `TERRAIN.PROJECT` both declare
`zref::render::project_vertex` as their reference model. A repair here has two
callers and its contract belongs to both.

**Acceptance.** Worst projector path better than âˆ’5.328 ns, so the RAMs become
binding. `proj_matw_directed` and `proj_rowmux_directed` still pass, including
the positive control that skews one matrix word by a raw LSB.

### T2 WAS ATTEMPTED AND REVERTED. Read this before attempting it again.

The obvious form of this fix â€” register the arbitrated vertex so the core
launches from flops â€” **violates a stated contract of this module**, and the
sentence that forbids it is in `zhao_project_service.sv`'s own header:

> LATENCY IS FIXED FROM THE ACCEPTED CYCLE. â€¦ A client that is not granted this
> cycle is simply not accepted this cycle; **it is never accepted and then
> delayed.** `*_ready_o` is the whole of that contract.

The projector's capture-at-accept law binds a vertex to the matrix in effect at
**its accept edge**. Put a register between the accept and the core and the two
come apart: a configuration write landing in the gap projects a vertex under a
matrix it was not accepted under.

`terrain_wcache_differential_rpp1` caught it immediately and precisely:

```
FAIL: reconfig (b): a torn fill is faithful per corner to the matrix at its
      accept edge
[terrain_wcache_differential] 1/11698 checks FAILED
```

One failure in 11,698 â€” which is what a one-cycle reordering looks like when
only a write that lands in the gap can see it.

**Delaying the configuration by the same cycle does NOT fix it**, and that was
tried. It preserves the order of a write against an *earlier* accept, but a
write one cycle *after* an accept now arrives at the core simultaneously with
the vertex it should have followed. The same test still failed, the same way.

### AND THE REVERT LED TO THE RIGHT DIAGNOSIS, WHICH IS NOT ARBITRATION AT ALL

Nineteen nanoseconds is far more than a grant and a mux, so the endpoint was
worth reading rather than assuming. Of the 1,717 paths ending inside
`zhao_project_core`, the worst six all land in the same place:

```
  vo_valid  ->  s1_ry[53]    -9.811 ns, data 19.043
  vo_valid  ->  s1_ry[50]    -9.754 ns, data 18.982
  vo_valid  ->  s1_ry[48]    -9.737 ns, data 18.953
```

`s1_ry` is **stage 1's row sum** â€” Â§2's `mat4_vec4`. So the path is not
"arbitration is slow". It is the nine MATWÃ—32 row multiplies and their sum,
**all in one cycle**, with the arbitration mux select sitting in series in front
of them. The launch is a control bit because a select fans out across 32 mux
bits and therefore beats the data into the cone, not because the control logic
is deep.

That relocates the package. **T2 is: pipeline stage 1's row sum inside
`zhao_project_core`.** And crucially, it does **not** run into the wall the
first attempt hit: the matrix is read at stage 1's input, at the accept edge,
and splitting the SUM after that read leaves the capture-at-accept law
untouched. No buffering between accept and the matrix, so nothing tears.

The other launch points confirm the shape rather than competing with it:
`ram_block8a4` and `ram_block8a3` launch 734 of these paths, and the divider
lanes (`g_div_stage[..].r_dv[..]`) another few hundred â€” the core is a long
arithmetic pipeline whose first stage is the widest.

### THIS CUT WAS ALREADY SPECIFIED, NINE DAYS AGO, AND DELIBERATELY DEFERRED

`zhao_project_core.sv`'s own comment says so in passing â€”

> This block already misses the product clock on exactly this cone (73.62 MHz
> after the stage-5b cut; `reports/PROJECT-CORE-CLOCK-20260907.md` names
> `mat -> view mux -> Mult0 -> row adder -> s1` as the standing worst path)

â€” and that report ends with the fix and the reason it was not done:

> **`Mult0`, not `Mult9`.** The cut moved the viewport `fx_mad` off the critical
> path and exposed the **row transform** â€¦ a combinational DSP output worth
> 3.938 ns, **output register unused**. â€¦ The next cut is
> `row_x/row_y/row_w` registered before `rescale16_row`.
>
> **Not pursued.** The owner's 2026-09-07 direction puts texture first, and this
> is the geometry lane. **Recorded with its evidence so the pass that owns it
> does not start from a reading.**

**This is that pass**, and the reason for the deferral has expired: the texture
lane closed at 108.37 MHz with zero setup TNS on 2026-09-16.

Two things follow. First, T2 does not start from a blank page â€” the path was
walked hop by hop in that report, the DSP's **unused output register** is named
as the specific waste, and 61.09 â†’ 73.62 MHz is the measured precedent for the
same treatment one stage later. Second, this is the healthy form of the
uncashed-cheque pattern rather than the usual one: the knowledge was written
down properly, with its evidence, by someone who knew they were deferring it,
and it was read back before anyone re-derived it.

The standalone core measured 73.62 MHz on this cone. G8B measures the same cone
at 19.043 ns â€” worse, because the service's operand mux now sits in front of it
and the terrain client's valid launches it from another block.

### THE CUT WAS IMPLEMENTED AND IT ALMOST WORKS. Start from here, not from zero.

Written out in the spatial branch of `zhao_project_core`: the nine products
registered into `p_x0_q â€¦ p_w2_q` plus `t_x_q/t_y_q/t_w_q` on the accept edge,
and the three four-term sums moved to the next cycle, feeding `s1_rx/ry/rw`.
`seq_holds` becomes `p_valid_q` so `busy_o` still covers the new stage â€” the
queue-occupancy law the module's own comment insists on.

**What passed:**

* `proj_matw_directed` â€” **242/242**, both MATW values, both ROWS_PER_PASS,
  including the exhaustive narrowing differential and the refusal law.
* `proj_rowmux_directed` â€” the full stream equality under four stall patterns.
* Capture-at-accept is untouched by construction, and the mid-sequence
  configuration-write case passes. This is the property that killed the first
  attempt at this package; the second shape does not have the problem.

**What had to move, and legitimately:** the spatial branch gains one cycle, so
the declared `L1 = L3 + 3` becomes `+2` in `proj_rowmux_directed` and
`proj_matw_directed`. The initiation interval is **unchanged at exactly 3** â€”
the half of the rule that may not move. The sequenced branch was deliberately
not cut: `ROWS_PER_PASS=1` is an unselected DSP lever and its row sum lives
inside an FSM. It still owes the same treatment, and the `+2` is the reminder.

**WHAT BLOCKED IT, and it is the only thing left:**
`proj_matw_mutant_control` stops producing output. It passes at HEAD in
**0.019 s with 14 checks**; with this change its `run_and_compare` loop runs to
its 100,000-cycle bound without collecting the records it waits for. That was
confirmed by stashing the change and re-running, so it is this change and not a
stale binary.

The stale committed copy `tests/mutants/zhao_project_core_mutant.sv` was
**refreshed onto the new body first** (its one mutation is `cfg_fits = 1'b1`)
and the failure survives that, so mutant drift is not the cause either. The
control drives two core instances through its own `tb_proj_matw` testbench with
`r_`/`m_` prefixes.

**Refined on a second attempt, so a third does not repeat it.** It is NOT a
hang. `run_and_compare`'s loop is bounded at 100,000 cycles and a timeout there
is a `CHECK` failure, not a lock-up. The process was left running **nine
minutes** without exiting and produced no output only because its stdout was
redirected to a file and therefore block-buffered. What actually happens is that
**every call runs its full bound** instead of the ~150 cycles it needs at HEAD,
so the test becomes minutes long rather than 0.019 s.

Ruled out, each by test rather than by argument:

* **stale binary** — stashed the change, rebuilt, re-ran: passes at HEAD in
  0.019 s with 14 checks.
* **mutant drift** — the committed copy was 52 substantive lines stale (this
  change made it so, which is the class `mutant_copy_drift` exists for), was
  refreshed onto the new body keeping its one mutation `cfg_fits = 1'b1`, and
  the symptom survived.
* **the core itself** — `proj_matw_directed` passes 242/242 on the same core at
  both MATW values and both `ROWS_PER_PASS`, and it measures latencies
  successfully, so outputs do arrive in that testbench.
* **`en_i`** — the driver sets `r_en_i = m_en_i = 1` in reset and never clears
  them.

**The next diagnostic is one edit, not an investigation.** Print
`r.size()`/`m.size()` when the loop exits on its bound: the existing
`CHECK(r.size() == m.size(), "stream lengths %zu vs %zu", …)` already formats
exactly that and is simply never reached while the loop is still spinning.
Whether one stream stalls or both do splits the remaining space in half.

**The change is not in the tree.** It was reverted rather than shipped with a
control that no longer reports, because a red instrument is worse than a slow
clock. Redoing it is perhaps twenty minutes with this section in hand.

So the two shapes below are **superseded**. They were the right answers to the
wrong question, and are kept only so nobody re-derives them:

1. **Capture the matrix with the vertex.** Register the selected view's sixteen
   matrix words alongside the operands at the accept edge, so the pair travels
   together and the law holds by construction. ~512 flops, affordable at 7,424
   of 41,910 ALMs â€” but the core reads `mat[view]` internally today, so this
   changes `zhao_project_core`'s input interface, not just the service.
2. **Shorten the path without buffering.** Attack the arbitration-to-operand
   cone itself rather than inserting a stage: the mux is three-way over four
   32-bit operands plus the rider, gated by a grant computed from two far-away
   valids. Registering `prefer_b_q`-derived terms, or narrowing what the grant
   has to decide before the mux, keeps the accept edge intact.

The reverted attempt is not in the tree. What it bought is this section.

---

## T3 -- SCOPED from the `@g8b-t1b` export, and it is three packages

The paragraph this section used to hold said *"this package cannot be scoped
properly yet... scope it from the post-T1/T2 export, not from this one"*, and
called itself "the inferred RAM paths". The first half was right and the second
half was a guess made from endpoint names. Now that the export exists, the name
is wrong: **there are no slow design memories.** See the status section at the
top of this brief for the cell-by-cell evidence; this section is the plan.

### T3a -- the tessellator's enumerator loop (73.6 MHz, the current cap)

**Owns:** `j_s` -> cell-times-stride multiply -> bound compare -> run-cell
decode -> `cell_skip` -> enumerator advance -> the geomorph DSP's clock enable.

**The constraint on any fix** is the block's own law: the enumerator advances at
ISSUE, not at capture, which is what holds three cycles per triangle. Simply
registering `cell_skip` inserts the bubble that header forbids. The shape that
can work is precomputing the NEXT run-cell's coordinate and its void decision
one cycle ahead, so the issue gate reads registers rather than a fresh multiply.
That is a real enumerator redesign.

**Acceptance:** better than -2.449 ns (where `zhao_project_core` sits) AND
`terrain_tess_modes_directed` still at 93 cycles or fewer for 81 unstitched
vertices. The second half is not optional -- T1 met a timing target and lost the
rate, and only the mode test noticed.

### T3b -- `zhao_project_core`'s output stage (80.3 MHz)

**Owns:** `s6_prod_x[38] -> out_x_o[7]`, -2.449 ns, data 11.870. The viewport
transform's final product into the output register. **A different cone from
T2's**, which registered the row products at `s1`; T2 neither helps this nor is
undone by it.

**Acceptance:** better than -0.5 ns, with `proj_matw_directed` and
`proj_rowmux_directed` unchanged and the initiation interval still exactly 3.

### T3c -- the divider's shift-register delay lines (82.9 MHz, then 91-94)

**Owns:** twelve `shift_taps_*` (ALTSHIFT_TAPS) instances, all launching from
`s2_cw[25]~DUPLICATE` -- one bit of the clip-space `w` that feeds `pre_d` and
`pre_d2`, the long division's divisor.

**Three candidates, none of them assumed:** duplicate `s2_cw`'s drivers per
consumer (Quartus already made one `~DUPLICATE` unprompted, which is the tool
pointing at where it hurts); restructure so the taps are not all gated from one
bit; or turn shift-register recognition off on this cone so the taps become
flops, trading M10K and routing for registers. **The third is the one to measure
first**, because it is a single attribute and reversible, and because 94 of the
device's 553 M10Ks are already spent.

**Acceptance:** worst `shift_taps_*` path better than -0.5 ns, with no increase
in DSP and the M10K change declared either way.

### The order, and the one thing not to do

**T3c, then T3b, then T3a.** Cheapest first, and deliberately so: T3a is the
current cap but it is also the only one of the three that is a redesign, and the
other two sit 2.4 ns and 2.1 ns behind it -- close enough that fixing T3a alone
buys almost nothing. **Do not fit after each.** One fit after all three, as
`@g8b-t3`.

**And do not read 73.59 MHz as "nearly there".** The subsystem needs 100, and
the whole machine is at least 10,591 ALM and 88 DSP over its closure criterion
on understated evidence (`reports/RESOURCE-RESCUE-ROADMAP-CURRENT-20260913.md`,
live position 2026-09-16). Timing and area are separate breaches and this
campaign only addresses one of them.

---

## The fit plan

**One fit, after T1 and T2 together.** Not one per package.

The per-block ceiling table already predicts what a single-package re-fit would
report â€” about 50 MHz after T1 alone â€” so spending a fit to confirm it would
buy nothing. G8B costs roughly 8 minutes of Quartus, which is cheap enough to
tempt exactly the wrong habit; the expensive resource is the campaign's
attention, not the runner.

T3 is scoped from that fit's export, then a second fit closes the campaign.

Row labels: `@g8b` is taken and the runner is one-shot by design. Use
`@g8b-t12` and `@g8b-t3`.

## What to hold on to while doing this

* **The subsystem is functionally correct and fully gated.** 203/203 across
  packets Bâ€“I, `terrain_pipe_rpp3_matw18` bit-exact against the oracle, and the
  G8B wrapper's activity witness at 15/15 with zero matrix refusals, all three
  legal view masks, 14,928 projector contentions and 603 output stalls. This is
  a timing campaign on a working machine, not a repair of a broken one.
* **Resources are not the problem and should not be traded for clock without
  saying so.** 7,424 of 41,910 ALMs, 34 of 112 DSPs, 44 of 553 RAM blocks.
  There is room to spend registers here, and registers are what this needs.
* **Latency may grow; initiation rate may not.** That rule is what makes T1 and
  T2 legal at all, and it is the one line neither may cross.
