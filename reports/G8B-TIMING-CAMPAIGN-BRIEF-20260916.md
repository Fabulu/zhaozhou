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

## STATUS: T2 IS LANDED AND MEASURED. T1 is the only thing left before ~69 MHz.

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

## T3 â€” the inferred RAM paths

**Evidence.** ~130 rows across the `altsyncram_*` instances, worst âˆ’5.328 ns
with 13â€“15 ns of data delay. These are the lattice/arena/window memories.

**What it owns.** Whichever of those paths survives T1 and T2. This package
cannot be scoped properly yet: T1 and T2 change the placement and the fanout
around these RAMs, and a path list taken before them describes an arrangement
that will not exist. **Scope it from the post-T1/T2 export, not from this one.**

**Acceptance.** Worst RAM path better than âˆ’0.0 ns at 10.000 ns.

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
