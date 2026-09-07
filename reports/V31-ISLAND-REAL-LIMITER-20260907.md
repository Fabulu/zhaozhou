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
* **No fix is attempted here.** Registering perspuv's outputs changes the
  perspuv→fragrob handoff, which is an island-top protocol change rather than a
  self-contained wrapper — and the fit lane that would measure it is busy. The
  header's own note that *"the round trip goes from five clocks to six against
  NTOK = 16 slots"* shows latency there has been costed before, so this belongs
  to a pass that can measure it.
