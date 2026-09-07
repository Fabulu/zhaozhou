# The island's real clock limiter is a combinational queue head — and it is not the block that looks worst

*2026-09-07. Analysis only, from evidence already on disk. Written while the
`zhao_texture_v3own` refit holds the fit lane; nothing here is in its closure.*

---

## The block that looks worst is not the problem

`zhao_texture_aux_pipe` reports **63.63 MHz** — the lowest of any texture-island
block, and 36 MHz under the product clock. It is also the largest
reported-versus-internal gap in the ledger: its core→core is **120.37**.

Chasing it would have been wasted work. Its worst paths all start at
`req_wx_i[3]`, an **input port**, running combinationally into
`zhao_texture_aux_div6` — the block computes its numerator from the pins before
its first flop (`nu_c = (req_wx_i − req_env_x0_i) <<< 6` at line 233; the first
`always_ff` is at line 435).

**And in the composed island fit, `aux_pipe` does not appear in the worst paths
at all** — not once. The 63.63 is a leaf-fit boundary artefact. This is CLAUDE.md's
own lesson running the other way: the *alarming* number was the artefact, and the
check that separated them was cheap.

## What the composed fit actually names

`zhao_texture_island_top`, reported **67.57** / core→core **77.30**:

    -4.800  pal_ld_gen_i[1]                      -> zhao_texture_palette_res:u_palette|res_r[3]
    -4.800  pal_ld_gen_i[1]                      -> ...|res_r[0]
    -4.795  pal_ld_gen_i[1]                      -> ...|res_r[2]
    -4.792  pal_ld_gen_i[1]                      -> ...|res_r[1]
    -4.168  pal_ld_gen_i[1]                      -> ...|loading_r
    -2.936  zhao_raster_perspuv_svc:u_persp|head_q[1]
                                                 -> zhao_texture_fragrob:u_fragrob|altsyncram:axg_m_rtl_0
    -2.834  live_r[6]                            -> zhao_raster_rcp24_svc:u_rcp|c_m.raddr_a[1]

The five worst start at `pal_ld_gen_i`, an **island-top input port** — the
palette load path, terminated at the boundary, which is what sets the reported
67.57. **The honest limiter is the sixth line at −2.936**, and it agrees with the
endpoint split's core→core figure of 77.30.

## And it is the same defect §16.2 named

`zhao_raster_perspuv_svc` drives its outputs by dynamically indexing flop arrays:

```systemverilog
assign head_done    = e_val[head_q] && (e_have[head_q] == 2'b11);
assign u_o          = e_dz[head_q] ? 32'sd0 : e_q_u[head_q];
assign v_o          = e_dz[head_q] ? 32'sd0 : e_q_v[head_q];
assign tag_o        = e_tag[head_q];
assign sat_o        = e_dz[head_q] ? 1'b0 : e_sat[head_q];
assign depth_zero_o = e_dz[head_q];
```

**Seven arrays indexed by `head_q`** — `e_val`, `e_have`, `e_dz`, `e_q_u`,
`e_q_v`, `e_sat`, `e_tag` — at `NTOK = 16` deep, two of them 32 bits wide. The
measured path launches from `head_q[1]` and lands in `fragrob`'s M10K. So a
16-way select across seven arrays sits between a queue pointer and the next
block's memory.

**Its own header describes the opposite design:**

> `P0  pop a queue, register the operands   (array reads, NO arithmetic)`

The internal pipeline does register its array reads. The **external** outputs
bypass that and read the arrays combinationally. That is the identical shape S01
§16.2 records for `zhao_raster_ticketq` — *"the commentary says a FIFO head is a
register"* while `dout_o = mem_q[head_q]` — except that one is in the
not-yet-adopted `rcp24_v3`, and **this one is in the block the island actually
composes.**

## Why this is the priority for the island's clock

* It is the **worst core→core path in the composed island** — the only figure
  with no boundary to blame.
* The **fix pattern already exists and was verified today.**
  `zhao_raster_ticketq_rh` gives a queue a registered head plus a spare so one
  pop per clock survives, and its before/after showed a throughput cost of
  **0.16%** with all 52 checks green. The same structure applies here.
* Both endpoints, `zhao_raster_perspuv_svc` and `zhao_texture_fragrob`, are
  already on the eight-block refit list in
  `V31-ISLAND-BUDGET-BLOCKED-20260907.md`, so the measurement is needed anyway.

## Caveats stated rather than buried

* **The composed row is four commits stale** (`compare_rows.py` refuses sums
  containing it). The *path* it names is still the best composed evidence
  available, but the number attached to it is not current — and this report is
  not a claim that the island is at 77.30 today.
* The palette input paths at −4.800 are **not** dismissed as pure artefact. In
  the assembled machine `pal_ld_gen_i` is driven by real logic, so that seam is
  real too; what is artefactual is treating a pin-terminated path as the block's
  internal ceiling. A registered characterisation wrapper is how that gets
  measured honestly, and four already exist in `fpga/rtl/synth/` as the pattern.
* **A fix WAS attempted, and landed** — see below. The paragraph that follows
  was written before it and is kept as the reasoning that got overturned.

* **No fix is attempted here.** Registering perspuv's outputs changes the
  perspuv→fragrob handoff, which is an island-top protocol change rather than a
  self-contained wrapper — and the fit lane that would measure it is busy. The
  header's own note that *"the round trip goes from five clocks to six against
  NTOK = 16 slots"* shows latency there has been costed before, so this belongs
  to a pass that can measure it.


---

# REPAIRED THE SAME DAY (`4ff7d48c`)

The deferral above was overturned for the same reason as §16's: only the
**timing** confirmation needs Quartus, and the output is a `r_valid_o`/`r_ready_i`
handshake, which makes a registered stage **self-contained**. The port list does
not change and the island top is untouched — only latency moves, and this file's
own header already costs that trade (*"the round trip goes from five clocks to
six against NTOK = 16 slots"*).

**A skid, not a plain register.** `r_ready_c = !r_valid_q || r_ready_i` is high
whenever the output register will be free, so one transfer per clock survives.
Both internal retirement sites — the free-count delta and the head advance —
moved to the internal ready **together**, because this file records what happened
when they were separated once: *"a count which grew without bound, `v_ready_o`
stuck high, and `tail_q` wrapping over live entries: the lane answered 193 of 335
fragments and then hung."*

## Verified at both levels, with real pre-change runs

| | before | after |
|---|---:|---:|
| `raster_perspuv_svc_directed` | 666 products / 335 clocks — **1.99/clk** | **identical**, 1.99/clk |
| | 8 checks pass | 8 checks pass |
| `island_composed_directed` | **119 checks pass** | **119 checks pass** |

The throughput gate is R7's *"≥ 1.64 products/clock"* and it is met at 1.99 in
both — the skid costs **nothing measurable**, not merely "little". The island's
check count is identical, so nothing was quietly skipped, and its credit phase
still reports submitted 200 / retired 200 with live peak 64 of 64 while the sink
is deliberately shut.

## And the island test was shown to GUARD it

119 passing is only evidence if 119 could have failed. The skid was mutated —
`r_ready_c = 1'b1`, overwriting the output register while it still holds
un-transferred data, which is the exact error this structure exists to prevent —
and the island test **failed 31 of 119** with concrete data loss:

    FAIL: phase 1 retired every fragment it submitted   expected 32, got 19
    FAIL: and every CLUT sample performed its palette lookup  expected 96, got 75
    FAIL: phase 2 retired every fragment it submitted   expected 32, got 0

So the composed bench genuinely exercises back-pressure through this handshake,
and the identical 119 before and after is a real result rather than a test that
stopped looking. The mutant was built from a copy outside the repository, so the
tree never held broken RTL.

## The cost side, stated before the refit reports it

`zhao_raster_perspuv_svc` is already `failed:structure`, and not marginally:

    registers 3157 > allowed 700   (4.5x)
    ALM       1910 > allowed 900

**The output boundary added here makes that slightly worse** — roughly 83 more
flip-flops (32 + 32 + TAGW + 3), about **+2.6%** on a block already 4.5× over.
Saying only "it removes the island's worst path" would be the flattering half.

Two things are worth separating, because §12.4 insists on it:

* **The rule may be mis-set for this block.** It holds a 16-entry table of
  32-bit U and V plus tag and flags — `16 × (32+32+TAGW+5)` is already well over
  a thousand bits before any pipeline. A 700-register budget for a structure
  whose payload alone exceeds it is a budget question, not only an
  implementation one.
* **That is not licence to relax it.** §12.4: *"A useful intermediate can fail an
  allocation and still be worth retaining. Its status must say precisely that.
  … Do not silently edit max_alms to 5,700 because 5,678 happened to be
  measured."* So the block stays **retained and failing**, the gate is untouched,
  and the +83 is declared rather than absorbed.

Note also that the sibling rule message on `zhao_texture_fragrob` — *"state that
belongs in memories is in flip-flops"*, fired at 2,631 against 2,500 — is the
exact case CLAUDE.md records as a **right alarm with a wrong diagnosis**: that
block already holds 13 M10Ks. The message should not be read as an instruction
about where fragrob's payload lives.

## Still to measure

The timing benefit. That was the entire point, and only a refit shows it.
Compare against the island's **67.57 reported / 77.30 core→core** on matched
scope — remembering that row is four commits stale, so the refit is needed for
both halves of the comparison.


---

## The palette load path is the same defect a THIRD time — and is deliberately left alone

The five paths worse than the one repaired above all start at `pal_ld_gen_i`.
That is the palette **load/configuration** interface — `ld_valid`, `ld_op`
(BEGIN/WRITE/END), `ld_slot`, `ld_gen`, `ld_idx`, `ld_rgb565`, `ld_crc_ok` — not
a per-fragment path. Inside `zhao_texture_palette_res` it reaches:

```systemverilog
if (ld_gen_i == gen_r[ld_slot_i]) begin      // line 202
```

An input port indexing an array with **another** input port, compared, and
landing in a register. Structurally the same shape as `ticketq`'s `mem_q[head_q]`
and perspuv's `e_q_u[head_q]`: **a dynamically indexed array read with no
register in front of it.** Three instances found today, in three different
blocks.

**It is not being fixed in this pass, for two reasons that are worth separating.**

1. **Magnitude is unknown and the honest prior says "small".** These paths are
   port-terminated in a leaf fit of the island, so they carry virtual-pin delay.
   CLAUDE.md records the last time this exact shape looked decisive: *"Deleting
   the boundary entirely would buy about 4 MHz of 36 needed. The artefact was
   real and almost irrelevant."* Registering the load interface is
   architecturally right regardless — but claiming it buys the gap between
   reported 67.57 and core→core 77.30 would be exactly the comfortable reading.

2. **There are already four unmeasured RTL changes stacked** — T4, the fence
   rewrite, perspuv's output boundary, and `ticketq_rh`. §12.5 says *"attribute
   each delta before treating it as a mechanism."* A fifth would make the next
   fit's numbers harder to attribute, not easier. The right move is to measure
   what is in the tree before adding to it.

**Recorded as the next candidate**, with the note that a palette load is a
configuration event, so a registered load interface costs a cycle per word on a
path that runs rarely — which is the cheapest kind of pipeline register there is.
