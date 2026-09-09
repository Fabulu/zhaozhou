# Texture gate 2 fails its ALM redline, and ONE BLOCK is 81% of the overage

2026-09-09. Owner brief section 7.1: *"The current texture gate remains the
immediate task... If a texture gate fails, diagnose the failed specimen and
complete the existing remedy; do not use this brief as an excuse to disappear
into projection."*

`@g2-prod` fits at **10,836 ALM against a 7,500 redline** -- over by **3,336**.
This is that diagnosis, from the fit's own per-entity table rather than from a
sum of leaf fits.

## Where the island's ALM actually is

Own contribution, excluding children, from
`blockpaths/zhao_texture_island_v3_top@g2-prod.fit.rpt` section 14:

| entity | own ALM | share of island | inclusive |
|---|---|---|---|
| **`zhao_texture_v3own:u_own`** | **2,706.7** | **25.0%** | 2,818.2 |
| `zhao_raster_perspuv_svc:u_persp` | 1,710.1 | 15.8% | 1,710.1 |
| `zhao_texture_island_v3_top` (its own glue) | 1,306.0 | 12.1% | 10,836.0 |
| `zhao_texture_cache_pipe:u_cache` | 1,150.8 | 10.6% | 1,150.8 |
| `zhao_raster_rcp24_svc:u_rcp` | 869.8 | 8.0% | 1,002.9 |
| `zhao_texture_tmu_plan:u_plan` | 771.2 | 7.1% | 771.2 |
| `zhao_texture_material_combine_v2:u_combine` | 536.7 | 5.0% | 536.7 |
| `zhao_texture_rsp_dispatch:u_dispatch` | 470.8 | 4.3% | 470.8 |
| `zhao_texture_aux_pipe:u_aux` | 293.2 | 2.7% | 501.8 |
| `zhao_texture_palette_res:u_palette` | 274.1 | 2.5% | 274.1 |
| `zhao_texture_aux_div6:u_div` | 208.6 | 1.9% | 208.6 |
| `zhao_texture_frag_expand:u_expand` | 146.4 | 1.4% | 146.4 |
| `zhao_field_rcp24_rom:u_rom` | 133.2 | 1.2% | 133.2 |
| `zhao_texture_bilerp_lane:u_bilerp` | 81.0 | 0.7% | 81.0 |

## The finding

**`zhao_texture_v3own` is 2,707 ALM -- a quarter of the island, and 81% of the
3,336-ALM overage on its own.** The 64-owner transaction file and completion
pipeline is the ALM problem. Nothing else is close: the next block is 1,000 ALM
smaller, and the four smallest entities together are under 570.

That is not where I would have looked. The reciprocal tile carried the DSP
argument all day, the combiner carries the brief's ROM packets, and the cache
pipe is the new timing hot node -- and none of the three is the area problem.

**And `v3own` is also implicated in timing.** `ISLAND-TIMING-IS-ONE-REGISTER-BIT`
found `u_own|live_cnt_q[6]` sourcing 42 of 43 internal paths on the pre-packet
specimen, and on `@g2-prod` it still sources 28 of 117. So the largest area
consumer and a third of the internal timing paths are the same block.

## What this means for the brief's queued texture packets

Sections 7.2, 7.3 and 7.4 propose replacing products with quarter-square ROM
lanes in `material_combine_v2` (2 DSP -> 0), the bilinear filter (3 DSP -> 0) and
pixel fog. Those are **DSP levers, and they are aimed at 618 ALM of the island**
(combine 536.7 + bilerp 81.0). Even a total elimination of both blocks would
leave the island **2,718 ALM over its redline.**

So, stated plainly and without proposing the work: **the queued ROM packets
cannot close this gate.** They are the right moves for the DSP objective and the
wrong instrument for the ALM redline. Whatever closes 7,500 has to reach
`v3own`.

## What is NOT claimed here

* **No cause for `v3own`'s 2,707 ALM.** This is a placement-attributed area
  figure, not an analysis of what inside the block spends it. The obvious
  suspects -- a 64-entry transaction file, the completion pipeline, seven
  32-bit counters, the fence phase machine -- are suspects, not measurements.
  The next step is that block's own register/array attribution, which is a
  MapOnly question and needs no island fit.
* **No proposal.** Brief section 0.1 authorises continuing texture and forbids
  turning this into a second project. A diagnosis is not a rewrite, and the
  remedy belongs to whoever owns the texture gate's acceptance.
* **The 2,704.5 "recoverable by dense packing" figure for the whole island is
  not subtracted anywhere above.** Own-ALM columns are `[A] used in final
  placement` minus `[B] recoverable` plus `[C]`; quoting the recoverable
  estimate as a saving would be reading the fitter's own slack as a design
  change.

## Provenance

`@g2-prod`, `MIGRATION_SHADOWS=0`, clean tree, digest `6812f753ff3b`, 148
minutes, `status: ok`. The declared shipping profile in
`design/prod_manifest.yml`. Section 14 of the fit report is the source for every
number above, and the first parse of it read the table of CONTENTS instead of the
table -- which is why the entity list is quoted with its totals reconciling to
10,836.

---

# THE CAUSE, measured -- and the remedy already exists inside this island

The section above deliberately stopped at "no cause for `v3own`'s 2,707 ALM"
and named the next step as a MapOnly. That ran: `zhao_texture_v3own@alm-
attribution`, clean tree, **3,750 registers, 20,640 memory bits, 0 DSP**.

## What the RAM summary shows

| array | type | depth x width |
|---|---|---|
| `v3bank:u_ctx`, `u_ares`, `u_fres`, `g_sres[0..2]` | **M10K block** | 64 x 40, 64 x 64 |
| `v3rq:u_rq_aux`, `u_rq_init`, `u_rq_tmu` | **M10K block** | 64 x 40 |
| `cq_ax_q`, `cq_s0_q`, `cq_s1_q`, `cq_s2_q`, `oq_res_q` | AUTO | 4 x 40 |
| `oq_ctx_q` | AUTO | 4 x 64 |

The AUTO ones are **depth 4**. Refusing an M10K for a four-deep array is the
fitter being right, and all six together are 1,056 bits. **They are not the
2,707 ALM**, and an analysis that stopped at the RAM summary would have said
"nothing is wrong here".

## The arrays that never reach a RAM summary at all

A RAM summary lists what INFERRED. State that stayed in flip-flops does not
appear, so the summary is silent about exactly the thing that costs. The
question needs `tools/quartus/check_ram_inference.py` -- repaired this morning
for nested-bracket blindness, which is why it can see these at all -- and it
names **nineteen** arrays in `v3own`, every one with the same finding:

> read COMBINATIONALLY through dynamic index `...` -- forces a per-bit mux the
> width of the array

**Eleven of them are `[OWNERS]`, and `OWNERS = 64`:**

```
cbi_q [64]x1   clm_q [64]x4   cmt_q [64]x4   crs_q [64]x1   fcl_q [64]x1
fdn_q [64]x1   ftc_q [64]x1   iss_q [64]x4   live_q[64]x1   rdy_q [64]x1
req_q [64]x4
```

Eleven separate 64-deep arrays, each read through a dynamic index in
combinational logic. **Every one forces a 64:1 multiplexer the width of the
array**, and none can become memory while that read stays asynchronous. About
1,472 bits of per-owner state, held as flip-flops, behind eleven wide muxes.

That is where a quarter of the island goes.

## The remedy is not a proposal -- it is already running beside it

`zhao_texture_v3bank` is in this same island, instantiated by this same block,
and its arrays **did** become M10K: 64 x 40 and 64 x 64, six of them. It is
described in the manifest as "the section 6 bank primitive, one instance per
declared bank". The owner brief lists the pattern among the existing blueprints:

> *Memory-backed descriptor/identity transport: the texture Decrufter replaces
> asynchronous indexed fabric payload with synchronous, atomic records.*

So the Decrufter treatment that produced the descriptor bank, the metadata bank
and the UV join has simply **not been applied to `v3own`'s per-owner state**.
Same island, same block, same eleven arrays it did not reach.

## What is still not claimed

* **No ALM figure for the remedy.** Eleven 64:1 muxes and 1,472 register bits
  are a mechanism, not a saving. How much returns depends on how many of the
  eleven can take a synchronous read without changing the completion protocol --
  and `v3own` is the block whose D0-class hazards this repository has already
  paid for once. Predict what moves, not how far.
* **Not every array can convert.** A read that must answer in the same cycle it
  is requested cannot become a synchronous memory read without a pipeline stage,
  and some of these eleven are on the admission path where that stage would
  change the credit protocol. Which ones is an RTL question, not a report one.
* **No work started.** Brief 0.1 authorises continuing texture and forbids
  turning this into a second project. This completes the diagnosis 7.1 asks
  for and stops there.

---

# THE CONVERSION TRIAGE -- which of the eleven can actually take the remedy

The section above said which ones convert *"is an RTL question, not a report
one"*. It is answered here, by reading every non-assertion index of each array.
The discriminator is simple and mechanical: **is the read address already
registered?** A synchronous memory read needs its address a cycle early, so an
array indexed by a `_q` signal can convert and one indexed by a `_c` signal
cannot without moving a protocol.

| array | bits | read index | verdict |
|---|---|---|---|
| `cmt_q` | 256 | `c4t_slot_q`, `c4a_slot_q` | **CONVERTIBLE** |
| `fdn_q` | 64 | `fetch_q` | **CONVERTIBLE** |
| `ftc_q` | 64 | `fetch_q` | **CONVERTIBLE** |
| `crs_q` | 64 | `_q` plus `sel_data_c[...]` | mixed |
| `rdy_q` | 64 | `_q` plus `sel_data_c[...]` | mixed |
| `live_q` | 64 | `_q`, a loop scan, and `bnd_tkt_c[...]` | mixed |
| `iss_q` | 256 | `iss_t_slot_c`, `iss_a_slot_c` | **BLOCKED** |
| `req_q` | 256 | `iss_t_slot_c`, `iss_a_slot_c` | **BLOCKED** |
| `cbi_q` | -- | assertions only | **NOT IN SYNTHESIS** |
| `clm_q` | -- | assertions only | **NOT IN SYNTHESIS** |
| `fcl_q` | -- | assertions only | **NOT IN SYNTHESIS** |

## Three of the eleven are not there at all

`cbi_q`, `clm_q` and `fcl_q` have **exactly two references each, both inside
assertions, and ZERO writes** -- and that block is guarded by `` `ifndef
SYNTHESIS ``. Quartus defines `SYNTHESIS`, so they are excluded before
synthesis sees them and cost nothing.

**That is a false-positive class in `check_ram_inference.py`**, which reported
all three as combinationally-read arrays. The tool scans text and does not
evaluate `` `ifndef ``. Worth knowing before its output is used to size
anything: it lists what LOOKS like a fabric array, and three of the eleven it
named here are simulation scaffolding. Not corrected in the tool today -- the
fix needs a preprocessor-aware scan, which is a larger change than the finding
justifies, and the caveat is recorded instead.

## What that leaves

**384 bits across three arrays are convertible today** -- `cmt_q`, `fdn_q`,
`ftc_q` -- because their read addresses are already registered. Each removes a
64:1 multiplexer of its width.

**512 bits across `iss_q` and `req_q` are the expensive half and are blocked.**
Both are read at `iss_t_slot_c` / `iss_a_slot_c` on the ISSUE path, and both are
4 bits wide, so they carry the two largest muxes. Converting them means giving
the issue decision a pipeline stage, which is a change to when a request may be
launched -- exactly the credit/admission protocol this block's D0-class hazards
came from.

**192 bits are mixed**: `crs_q`, `rdy_q` and `live_q` each have registered reads
AND a combinational one. A mixed array can still convert if the combinational
read is the one that moves -- but that is per-read design work, not a
reclassification.

## Still not claimed

No saving. Three convertible arrays are 384 bits and three muxes; the arithmetic
from bits to ALMs runs through packing decisions this analysis does not make.
And the convertible three are the CHEAP ones -- the two that would pay most are
the two that are blocked, which is the ordinary shape of this kind of result and
worth stating before anyone budgets from the easy half.
