# CLIFFPROD — FORGE.CLIFF is composed, and five premises died on the way

2026-09-25. Packet CLIFFPROD, branch `gz/cliffprod`, base
`claude/ceiling-architecture-20260912` at `2018d474`.
Owner vacation directive 2026-09-23 §6.

**Decision records live where they govern, not here.** This file is the
premise ledger and the evidence index. The decisions themselves are in:

* `fpga/rtl/forge/zhao_forge_cliff_feed.sv` — four records (page coordinates,
  the halo policy, the single position register, page completion under a
  pulled patch);
* `fpga/rtl/forge/zhao_forge_cliff_emit.sv` — four records (one edge = one
  job, the transcribed endpoint map, the consumed placement law, single-flight
  fetch);
* `fpga/rtl/forge/zhao_forge_cliff_srvshare.sv` — one record (one
  parameterised sharer rather than two typed ones) plus the detector argument;
* `fpga/rtl/prod/zhao_console_core.sv` — the `CLIFF_*` parameter block,
  including `CLIFF_VDIST_EN`'s whole measurement, and the cascade rationale;
* `design/contracts/FORGE.CLIFF.md` — the superseding section;
* `design/fit_targets.yml` — the fit gate's two questions, named before it ran.

---

## 1. Did the register move, and did pixels move?

**The register moved: 11 → 10.** `zhao_forge_cliff_ram` is off the
`BUILT BUT NOT CONNECTED` list. `zhao_console_core` instantiates it as
`u_forge_cliff`; the core smoke elaborates and runs it (`SMOKE_RC=0`,
*"PASS — the connected core carries traffic on every wire this bench can
reach"*).

**Pixels: not demonstrated, and the honest word is *not yet*.** What IS
demonstrated is the packet's stated minimum and then some — a cliff evaluated
from real terrain, with the result reaching a consumer, measured end to end:

```
compcache cs service ─┐                                      ┌─ v_* (4 world fx16 vertices)
                      ├─ srvshare ─ feed ─ cliff_ram ─ emit ─┤
compcache lat service ┘                                      └─ t_* (2 index triples)
                                                                 │
                              jobarb2 (client B) ─ zhao_forge_assemble ─ GEOM.CLIP ─ pixels
```

The chain is wired through to `zhao_forge_assemble`, which is the block that
projects and hands triangles to the clipper. What has **not** been done is to
drive a frame through it and look at the walls. The console smoke exercises the
composition's wiring, not its picture. Anyone claiming a visible cliff needs a
rendered frame, and this packet did not produce one.

## 2. Every premise found false, with the measurement

### 2.1 (MINE, and the packet's) "the block is ready and only its producer is missing" — FALSE, by one whole leg

Three producers were named. There was also **no consumer**. `edge_ci_i`,
`edge_side_i`, `edge_span_i`, `rim_edge` — **zero hits as ports across all of
`fpga/rtl`**. `design/contracts/FORGE.CLIFF.md` had recorded it
(*"THE EMISSION STAGE IS NOT WRITTEN"*) and no blocker list had picked it up.
Feeding the block its three inputs would have closed nothing.

### 2.2 The contract's "**the halo is free, not a problem**" — FALSE, and expensively so

§2 of the contract argued that because `cs_ci_i`/`cs_cj_i` are 5 bits, the
34×34 window's halo ring is unaddressable and therefore "exactly this
contract's own *off-lattice loads as 0* case".

A patch seam is not outside the island. Measured on one real 33×33
`ComposedLattice` (`forge_cliff_chain` lanes 1 and 2):

| halo | rim edges |
|---|---|
| VOID (the contract's reading) | **218** — bit-identical to `zref::forge::rim_plan` |
| SOLID (shipped) | **90** |

**218 − 90 = 128 = 4 sides × 32 cells.** Every edge of the difference is
perimeter — a wall around every patch, a visible grid over the island, 128
spurious edges per page against a budget of 512. The ring is now a port with a
named default, not a free constant.

### 2.3 "golden hold +0.263 ns against candidate hold −4.140 ns is a real cost of the swap" — NOT PROVEN, and the evidence points the other way

This was the packet's own headline cost. Three measurements, all from the
committed receipts, taken **before** any fit was run:

1. **No hold report exists on disk for either row.** `blockpaths/` carries
   `.setup.margin.rpt` and `.setup.summary.rpt` for both and **no hold report
   at all**. Neither hold number has a single path endpoint behind it.
2. **Both blocks' worst setup path is the SAME path, and it ends on the vdist
   port**: `edge_key_r_rtl_0|…~PORT_B_WRITE_ENABLE_REG → vd_addr_o[*]`, at
   −15.259 ns (golden) against −15.174 ns (candidate). The candidate is
   **0.085 ns better** on the path they share. It is a property of the design
   in either shape — which is precisely what R142 established for the inferred
   latch and the four RAM warnings, arriving a third time.
3. **The two summaries are not like for like.** Of the candidate's 314
   reported paths, **228 touch `vd_addr_o`/`vd_data_i` (72.6%)**; of the
   golden's 2,000, **32 do (1.6%)**. One summary is dominated by a single port
   and the other is not.

And **in the composed console that port carries nothing**: `CLIFF_VDIST_EN` is
0, so `vd_addr_o` has no load and `vd_data_i` is a constant. A leaf fit cannot
see that, because it turns every port into a virtual pin with a full I/O
budget. This is CLAUDE.md's recorded virtual-pin trap and its "compare like
with like, or do not compare" law in a fitter's clothes.

The fit gate `zhao_forge_cliff_chain_fit_top` was written to settle it, with
the falsifier named in advance: *a hold violation that survives on a path whose
endpoint is an internal register*.

### 2.4 "`zhao_forge_cliff_emit`'s fetch is single-flight" — FALSE in its own first cut, and it was the block's own decision record that said so

DECISION RECORD 4 argued at length that the address and the answer could not
separate. **The code did not implement it.** `lat_req_o` was asserted for the
whole fetch phase, so on the capture cycle the position register still held the
old index and the same address was re-issued. The symptom was surgical: the
edge **plan** stayed perfectly correct at 218 of 218, and every quad's vertex 1
held vertex 0's x with a height of zero.

`forge_cliff_chain` lane 1 caught it on the first run. **A decision record is
not the code**, and the only thing that makes them the same is a bench that
compares against something independent.

### 2.5 (MINE) "a Verilated test that hangs at ~0 CPU is a logic hang" — FALSE

`test_forge_cliff_srvshare_unit` hung three times with no output. `zhao_sim.hpp`
documents the cause in its own header: Verilator 5.051 + winlibs libwinpthread
deadlocks in `VlThreadPool::~VlThreadPool()` at exit-time static destruction,
at ~0 CPU, with the verdict still in an unflushed buffer — so **every Verilated
main must end through `zhao::exit_hard`**. Two startup probes are now kept in
that test, because "hung before `main`" and "hung at exit" look identical from
outside when nothing has been flushed.

## 3. Every counter, and the evidence it fires

| counter | block | fired by |
|---|---|---|
| `grants0_o`, `grants1_o` | srvshare | stimulus — every run |
| `denied1_o` | srvshare | chain lane 4: cs 227, lat 296 |
| **`poison1_o`** | srvshare | **`forge_cliff_srvshare_unit` lane 4 — 0 → 1 on a refusal in flight, with TWO negative controls (the refusal value on an idle bus; a legal value in flight)** |
| `pages_issued_o`, `windows_done_o`, `cs_reads_o`, `solid_cells_o` | feed | stimulus — every run |
| `cs_denied_o` | feed | chain lane 4, incumbent at ~2 cycles in 3 |
| `cells_degraded_o`, `pages_degraded_o` | feed | chain lane 5 — patch pulled mid-window: 579 cells, 1 page |
| `edges_taken_o`, `quads_emitted_o`, `tris_emitted_o`, `lat_reads_o` | emit | stimulus — every run |
| `lat_denied_o` | emit | chain lane 4, and **cross-checked against the sharer's own count from the other end** |
| `endpoint_clamped_o` | emit | reachable at the block's own port by offering an illegal edge |

**No committed mutant was needed**, and that is a result rather than a
shortcut: every guard here is reachable either by legal stimulus or at a
block's own port. The one that looked unreachable — `poison1_o` — is reachable
at the sharer's port because *a client that does not guard is a legal client*.

**And the composed chain reads `poison1_o` as ZERO.** That is evidence about the
**feed** (it gates on `serve_valid_i`, so it never asks while the patch is gone)
and none whatever about the detector. Chain lane 5b therefore asserts the
**reason** — the feed stopped reading; every interior cell was read or counted
degraded — instead of quoting a silence.

One counter removed before it was written: a **stray-response** detector on the
sharer. Its two sides would have been the serve block's answer gate and our own
request, one clock apart — the same net. Structurally blind, and it would have
read zero forever as reassurance.

## 4. What existing machinery was shared rather than re-derived

* **The placement law.** World positions are **read back** from
  `zhao_terrain_compcache_front`'s `lat_wx_o`/`lat_wz_o` — TERRAIN.PLACE's
  placed coordinates. Nothing in the new RTL multiplies a lattice index by a
  pitch, so `zhao_terrain_place_law_pkg` keeps exactly one implementation.
* **The endpoint map** is transcribed from `zhao_forge_cliff_ram.sv:439-467`,
  the evaluator's own vdist addresses, rather than derived from the side
  geometry a second time.
* **The borrowed-cycle discipline** is `zhao_terrain_heighttap`'s own internal
  pattern (`cs_go_c = cs_want_c && !o_cs_req_i`), reused.
* **`zhao_forge_jobarb`** is instantiated a second time rather than a
  three-client arbiter being written: its output face is protocol-identical to
  a client face, so the cascade is a rename plus one instance.
* **`zhao_forge_assemble`** is ridden, not cloned — no second vertex store, no
  second depth-quantiser, no fourth clipdoor client.
* **`zhao_terrain_tapshare`** was read and **rejected in writing**: right idea,
  but typed to the world-(x,z) service and carrying a response *valid*
  qualifier these two services do not have.

Before building, the tree was grepped for each of these. The one genuine
near-duplicate found was `zhao_terrain_tapshare`, and the rejection is recorded
in `zhao_forge_cliff_srvshare.sv`'s header rather than left implicit.

## 5. Gates

| gate | before | after |
|---|---|---|
| `gate_sweep.py` | RC 0, 30 gates match baseline | **RC 0, 30 gates match baseline** |
| `completion_register.py` | 11 gaps | **10 gaps** |
| `check_prod_manifest.py` | OK | OK (3 new modules declared) |
| `check_console_inventory.py` | OK | OK |
| `gen_prod_top.py` | — | regenerates **byte-identical**: no port moved, and the generated file was not stale |
| `run_console_core_smoke.ps1` | — | **RC 0** |
| `forge_cliff_chain` | — | **OK** (7 lanes) |
| `forge_cliff_srvshare_unit` | — | **OK** (5 lanes, positive + 2 negative controls) |

Verilator `--lint-only -Wall`: 0 diagnostics on all four new files. **That is
one tool's opinion and not synthesizability** — the fit gate below is the first
time any of the three new blocks goes through `quartus_map`.

## 6. What was NOT done, and why

* **No frame was rendered.** See §1. The composition is wired and exercised;
  the walls have not been looked at. CLAUDE.md is explicit that gates passing
  is not the thing looking right, so this is named rather than implied.
* **The vdist store was not built.** Measured route, measured obstacles
  (§2.3 and the contract addendum §3): it needs new ports on
  `zhao_proj_subsystem` and `zhao_terrain_group_seq` — both inside the terrain
  arm that packet EDGECLOSE was live in — and the once-per-view fill makes a
  naive store silently wrong. `CLIFF_VDIST_EN` is the one wire that turns it on.
* **The strata U was not emitted.** No port exists for it: `o_untex_o` is
  hardwired under R197. It is the assembler's attribute set, not this chain.
* **The island-directory halo was not built.** Unreachable without a structured
  patch coordinate on the serve face; `halo_substance_i` is already the port it
  would drive.
* **Counters were not lifted to `zhao_console_core`'s port list.** They are
  terminated locally with a stated reason. Exporting them ripples into
  `zhao_console_board` while another packet was live in the same file, and it
  is telemetry rather than function.
* **No console fit was run.** The 2026-09-08 batching law: the questions this
  packet raises are answered by a subsystem fit, and the console's ALM total
  moves for many reasons at once.

---

## 7. The §6 timing recheck, answered

`zhao_forge_cliff_chain_fit_top`, one fit, both questions named in
`design/fit_targets.yml` and in the wrapper's own header **before it ran**.
`sourceCommit f8d1f121`, **`rtlCleanAtHead: true`**, digest `fbb87047b3bf`,
745.6 s, `5CSEBA6U23I7`, Quartus 17.0.2.

### The three rows, side by side

| | golden leaf | candidate leaf | **composed context** |
|---|---|---|---|
| ALM | 6,674 | 976 | **1,350** (whole chain, 5 modules) |
| registers | 4,025 | 939 | **1,749** |
| DSP | 2 | 2 | **0** |
| RAM blocks | 14 | 15 | **8** |
| memory bits | 119,808 | 120,964 | **55,428** |
| Fmax | 39.59 MHz | 39.72 MHz | **94.93 MHz** |
| setup WNS | −15.259 ns | −15.174 ns | **−0.534 ns** |
| hold WNS | +0.263 ns | −4.140 ns | −2.113 ns |
| hold report on disk | **none** | **none** | **yes** |
| virtual pins | 265 | 273 | 448 |

### (a) Timing — the premise is NOT supported

**138 violated hold paths, and all 138 launch from an input pin. Zero
register-to-register.** By launch pin: `lat_rsp_i` 55, `cs_substance_i` 45,
`serve_src_id_i` 38 — and all three are **internal nets in
`zhao_console_core`**: the compose cache's registered lattice and cell-state
outputs, and a telemetry tap the core drives itself.

A virtual pin models almost no input delay, so data arrives too early relative
to the clock and hold fails. Driven by a real registered source with real
clock-to-out, these paths do not exist.

**Setup moved the other way and moved a lot**: −0.534 ns / 94.93 MHz against
−15.174 / 39.72 at the leaf. Both leaf setup numbers were the vdist port's
virtual pins, and that port carries nothing in the console.

So the −4.140 ns hold was a boundary measurement of a port the composed console
does not use, and the sign change against the golden is **not a cost of the
R142 swap**. That is now **three** things read as the price of the candidate and
found not to be: the inferred latch, the four RAM warnings, and the hold slack.

**On my own loose wording, because it matters.** The falsifier was named as *"a
hold violation that survives here on a path whose ENDPOINT is an internal
register"*. Read literally that fired — the endpoints **are** internal
registers. Read as intended (register to register) it did not. Rather than
reinterpret my own sentence to get the answer I wanted, the decisive measurement
is the **launch-point census**, which settles it either way: a boundary artefact
launches at the boundary.

**Caveat, and it bounds the claim.** 448 virtual pins remain and Quartus reports
**Critical Warning (15725)** — *"clock port is fed by virtual pin `clk~input`;
timing analysis treats input to the clock port as a ripple clock"*. **This is
not a console timing number and must not be quoted as one.** It is better
bounded than the leaf rows and taken on the *same* footing as them — which is
what makes it like for like, and like for like was the whole problem with the
pair it replaces.

### (b) Area — and a saving nobody had measured

The whole capability: **1,350 ALM, 1,749 registers, 0 DSP, 8 RAM blocks, 55,428
memory bits**. Five categories, kept apart, never summed.

Against the evaluator alone the ALM figure is **not** a clean delta — different
boundary, different constant folding — so it is quoted as a measurement of the
chain, not as a difference. But two differences **are** exact and explicable:

* **120,964 − 55,428 = 65,536 = 2048 × 32 = `prio_mem_r` exactly.** The
  priority table is pruned because vdist is off. Confirmed independently by the
  map's own inference table, which lists **four** memories rather than five, and
  by `prio_mem` appearing **zero** times in the entire report. About **7 M10K
  returned**.
* **DSP 2 → 0** — the vdist address arithmetic's two multipliers.

And the four `Warning (276020)` RAM pass-throughs are now **three**, because
`prio_mem_r` was one of the four and went with it.

**So `CLIFF_VDIST_EN = 0` is not merely free: it returns 2 DSP and ~7 M10K** on
a device already at 97% of its ALM budget. Turning vdist on later buys those
back, and that price is now known in advance rather than discovered by a fit
after the fact.

### What the first fit of this chain found, and why there were two

The first fit reported `Info (10041): Inferred latch for "poison1_r[0..31]"`
**32 times** — in my own new `zhao_forge_cliff_srvshare`, on the instance where
`POISON_EN = 0` makes the counter's increment condition constant-false, leaving
a register that can only ever hold its reset value.

That is the **same shape R117 already paid to remove** from
`triangles_submitted_o[0]`, and `--lint-only -Wall` had reported **zero**
diagnostics on all four files since they were written. A fit that measures a
circuit you already know is wrong is wasted, so the counter was moved into an
explicit `generate` — with `POISON_EN = 0` it now **does not exist** rather than
existing and reading zero — and the fit was re-run on the repaired, committed
RTL. The clean run reports `Info (10041)` **zero** times.

Two tool facts paid for on the way: an **implicit generate** lints clean and is
a Quartus 17.0 syntax error, and a comment whose first word is the linter's own
name is parsed as a **pragma** and fails the build.
