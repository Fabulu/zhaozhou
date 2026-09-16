# G8B timing campaign — scope, packages, and what each one buys

Written immediately after G8B's first measurement, in the shape that made the
Timing4 campaign efficient: name the packages, name what each one is worth in
nanoseconds, name the one fit at the end, and write down the risks before
anyone starts rather than after.

**Baseline:** `zhao_terrain_pipe_rpp3_matw18_fit_top@g8b`, commit `968243b5`,
clean tree, seed 1. **43.94 MHz**, setup WNS −12.758 ns, setup TNS
−7,360.359 ns, 2,000 of 2,000 exported paths negative. 7,424 ALMs, 8,070
registers, 34 DSP, 44 RAM blocks, 95,610 memory bits. Hold clean at +0.251/0.

**Target:** 100 MHz, the machine's operating requirement. That needs
**+12.758 ns** on the worst path — not the +0.587 ns G8A's last mile needed.

## The ceilings, and why the order is not the obvious one

| # | endpoint block | negative rows | worst | ceiling |
|---|---|---:|---:|---:|
| 1 | `zhao_terrain_tess` | 160 | −12.758 ns | 43.9 MHz |
| 2 | `zhao_project_core` | **1,631** | −9.811 ns | **50.5 MHz** |
| 3 | inferred RAMs (`altsyncram_*`) | ~130 | −5.328 ns | 65.4 MHz |

Each fix only exposes the next. **No single package reaches 100 MHz, and fixing
the worst path first buys 6.6 MHz.** That is the whole reason this is a
campaign and not a patch.

---

## T1 — the tessellator morph blend

**Evidence.** Worst path −12.758 ns, data 22.481 ns, skew −0.097 ns: logic
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
it, and the `vtx_room` credit — "a landing never finds both slots full without
a pop" — is argued against the current timing and must be re-argued.

**What does NOT work, checked:** the trick that fixed RASTER.RESOLVE. There,
half the arithmetic moved to the cycle the memory response arrived. Here `m_hc`
depends on `lat_h_i` itself, so there is nothing upstream of the response to
move work into. A stage is required.

**Acceptance.** Worst tess path better than −9.811 ns, so package 2 becomes the
binding constraint. Exact arithmetic unchanged —
`terrain_pipe_differential` stays bit-exact against `zhao_terrain_project`.
Its 478 packets over 2,343 cycles will move; the **rate** must not.

---

## T2 — the projector's arbitration-to-datapath path

**This is the big one: 1,631 of the 2,000 negative endpoints.**

**Evidence.** Worst path −9.811 ns, data 19.043 ns, launching from
`u_tess|vo_valid` — a CONTROL bit — into `zhao_project_core`. Reading
`zhao_project_service` shows why:

```systemverilog
wire both_c  = a_valid_i && b_valid_i;
wire grant_a = a_valid_i && !(both_c &&  prefer_b_q);
wire take_a  = en_i && grant_a && core_ready;
wire signed [31:0] core_vx = take_a ? a_vx_i : b_vx_i;   // and vy, vz, view, payload
```

A client's valid arrives, runs through round-robin arbitration, and the result
selects the operands that feed the projector's row-sum arithmetic — all in one
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

**Acceptance.** Worst projector path better than −5.328 ns, so the RAMs become
binding. `proj_matw_directed` and `proj_rowmux_directed` still pass, including
the positive control that skews one matrix word by a raw LSB.

### T2 WAS ATTEMPTED AND REVERTED. Read this before attempting it again.

The obvious form of this fix — register the arbitrated vertex so the core
launches from flops — **violates a stated contract of this module**, and the
sentence that forbids it is in `zhao_project_service.sv`'s own header:

> LATENCY IS FIXED FROM THE ACCEPTED CYCLE. … A client that is not granted this
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

One failure in 11,698 — which is what a one-cycle reordering looks like when
only a write that lands in the gap can see it.

**Delaying the configuration by the same cycle does NOT fix it**, and that was
tried. It preserves the order of a write against an *earlier* accept, but a
write one cycle *after* an accept now arrives at the core simultaneously with
the vertex it should have followed. The same test still failed, the same way.

So T2 has exactly two honest shapes, and both are bigger than "add a register":

1. **Capture the matrix with the vertex.** Register the selected view's sixteen
   matrix words alongside the operands at the accept edge, so the pair travels
   together and the law holds by construction. ~512 flops, affordable at 7,424
   of 41,910 ALMs — but the core reads `mat[view]` internally today, so this
   changes `zhao_project_core`'s input interface, not just the service.
2. **Shorten the path without buffering.** Attack the arbitration-to-operand
   cone itself rather than inserting a stage: the mux is three-way over four
   32-bit operands plus the rider, gated by a grant computed from two far-away
   valids. Registering `prefer_b_q`-derived terms, or narrowing what the grant
   has to decide before the mux, keeps the accept edge intact.

The reverted attempt is not in the tree. What it bought is this section.

---

## T3 — the inferred RAM paths

**Evidence.** ~130 rows across the `altsyncram_*` instances, worst −5.328 ns
with 13–15 ns of data delay. These are the lattice/arena/window memories.

**What it owns.** Whichever of those paths survives T1 and T2. This package
cannot be scoped properly yet: T1 and T2 change the placement and the fanout
around these RAMs, and a path list taken before them describes an arrangement
that will not exist. **Scope it from the post-T1/T2 export, not from this one.**

**Acceptance.** Worst RAM path better than −0.0 ns at 10.000 ns.

---

## The fit plan

**One fit, after T1 and T2 together.** Not one per package.

The per-block ceiling table already predicts what a single-package re-fit would
report — about 50 MHz after T1 alone — so spending a fit to confirm it would
buy nothing. G8B costs roughly 8 minutes of Quartus, which is cheap enough to
tempt exactly the wrong habit; the expensive resource is the campaign's
attention, not the runner.

T3 is scoped from that fit's export, then a second fit closes the campaign.

Row labels: `@g8b` is taken and the runner is one-shot by design. Use
`@g8b-t12` and `@g8b-t3`.

## What to hold on to while doing this

* **The subsystem is functionally correct and fully gated.** 203/203 across
  packets B–I, `terrain_pipe_rpp3_matw18` bit-exact against the oracle, and the
  G8B wrapper's activity witness at 15/15 with zero matrix refusals, all three
  legal view masks, 14,928 projector contentions and 603 output stalls. This is
  a timing campaign on a working machine, not a repair of a broken one.
* **Resources are not the problem and should not be traded for clock without
  saying so.** 7,424 of 41,910 ALMs, 34 of 112 DSPs, 44 of 553 RAM blocks.
  There is room to spend registers here, and registers are what this needs.
* **Latency may grow; initiation rate may not.** That rule is what makes T1 and
  T2 legal at all, and it is the one line neither may cross.
