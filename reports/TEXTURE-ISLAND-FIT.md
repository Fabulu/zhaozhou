# Texture island: does the 99.5 MHz renderer survive the texture path?

**No.** All ten blocks are now fitted. **Not one reaches the 150 MHz leaf
target. Not one reaches the 120–125 MHz island target. Eight of ten sit below
the shipped shell's own 99.50 MHz**, and the island's floor is **54.95 MHz** —
barely half the console clock.

Answering the question that was asked, 2026-09-02:

> "the important bit is actually fitting all the texture stuff to see if the
> 99.5 MHz renderer and full fitted console actually holds up or if it needs
> more reingeneering."

**It needs more reengineering.** The renderer's 99.50 MHz is real and stands;
the texture island cannot be clocked anywhere near it as written.

---

## The rows

Provisional device `5CSEBA6U23I7`, Quartus 17.0.2 Lite, one seed each, all I/O
virtual. Sorted slowest first, because the slowest is the one that sets the
clock.

| block | Fmax | vs shell 99.50 | ALM | reg | M10K | DSP | vpins | secs |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `zhao_texture_aux_pipe` | **54.95** | −44.6 | 1118 | 1244 | 1 | 0 | 374 | 2005 |
| `zhao_raster_texjoin_v2` | **61.66** | −37.8 | 3465 | 6143 | 3 | 0 | 829 | 3321 |
| `zhao_raster_perspuv_svc` | **62.67** | −36.8 | 1792 | 2827 | 2 | 3 | 267 | 2304 |
| `zhao_raster_rcp24_svc` | **68.46** | −31.0 | 1041 | 1101 | 0 | 6 | 177 | 2351 |
| `zhao_texture_cache_pipe` | **81.06** | −18.4 | 5634 | 10812 | 3 | 0 | 413 | 2858 |
| `zhao_texture_mosaic` | 86.63 | −12.9 | 197 | 192 | 0 | 4 | 180 | 922 |
| `zhao_texture_tmu_plan` | 93.55 | −5.9 | 1419 | 1054 | 0 | 0 | 363 | 2524 |
| `zhao_texture_bilerp_lane` | 99.69 | +0.2 | 125 | 177 | 0 | 3 | 132 | 768 |
| `zhao_texture_palette_res` | 104.42 | +4.9 | 152 | 141 | 2 | 0 | 165 | 738 |
| `zhao_texture_rsp_dispatch` | 110.90 | +11.4 | 806 | 1432 | 0 | 0 | 399 | 965 |
| **island total** | | | **15,749** | **25,123** | **11** | **16** | | |

`cache_pipe`'s first attempt came back **`contaminated:source-changed-during-fit`**
after 4,550 s. That is the provenance guard working exactly as designed and it
was my fault: I edited four files in the shared `-ExtraSources` list while its
fit was running, and the tool refused to attach a number to sources it could
not name. Re-run against the one-file closure in `design/fit_targets.yml`, it
fitted in 2,858 s with `rtlCleanAtHead = true`.

### The area is its own finding, and it points at the same defect

**15,749 ALMs is 37.6 % of the device** for the texture island alone — before
geometry, terrain, particles, post or the existing shell.

But the number that actually diagnoses something is the pair at the end:
**25,123 registers against 11 M10K.** A texture cache that stores its tags and
lines in *flip-flops* is a cache that did not infer as memory, and
`cache_pipe` alone is 10,812 registers and 3 M10K. That is exactly ruling X7 —
*"reads tag/data arrays combinationally, no M10K capture stage"* — showing up
in the resource columns rather than in a code review.

## Targets, for reference

| stage | target | worst measured |
|---|---|---|
| leaf datapaths | **150 MHz** | 54.95 |
| perspective/TMU/cache/AUX island, three seeds | **120–125 MHz** | — |
| texture-survivor composition | **115–120 MHz** | — |
| full composition acceptance floor | **105 MHz** | — |

Shipped shell: **99.50 MHz best, 96.87 mean.**

## Is this a seed draw? No.

The measured placement noise floor is **4.61 MHz** across three seeds on
identical RTL (`reports/NOISE_FLOOR.md`), and one seed is one draw from that
distribution. A 5 MHz difference between two rows here is not a difference.

**A 95 MHz shortfall is not noise.** `aux_pipe` is 20× the noise floor below
target. No seed sweep recovers that, and running one to be sure would be
spending three hours to confirm something the first hour already settled.

## What is actually wrong — the paths, not guesses

The archived setup reports name the startpoint and endpoint of every worst
path, so none of the following is inference.

### `aux_pipe`, 54.95 MHz — no input register at all

    req_env_x1_i[20]  ->  zhao_texture_aux_div6:u_div|ru_q[0][11]     slack -8.199

**The path begins at an input port.** The whole clamp-and-normalise cone sits
between the block's boundary and its first flop, and then runs on into the
divider's first stage. This block is "pipelined" in the sense that it has
stages; it has no register at the seam where it meets the world.

### `texjoin_v2`, 61.66 MHz — the ruling named this before the fit did

    free_cnt_q[3]  ->  pick_sidx~10_OTERM1201     slack -5.935, data delay 15.0 ns

`free_cnt_q` feeds the issue pick. That is the **16 × 3 combinational issue
scan** the brief flagged as X3 — *"likely a timing wall"* — and it is one,
measured. 3,465 ALM and 6,143 registers for a 16-entry join is the same finding
from the area side.

### `perspuv_svc`, 62.67 MHz — the rescale cone

    p1_prod_q[36]_OTERM87  ->  e_q[11][1][8]      slack -5.937, data delay 15.0 ns

Product register through variable rescale and saturation into entry storage, in
one combinational cone. R7's production ruling already requires this block be
rebuilt with **two parallel product lanes** and *"pipeline variable
rescale/saturation after both products"* — the second half of that sentence is
what this path is.

### `cache_pipe`, 81.06 MHz — the arrays are registers, not memory

    rq_rp[1]~DUPLICATE  ->  valid_r[1][2]     slack -2.337, data delay 12.159 ns

The return-queue read pointer runs through a combinational cone into the
tag-valid array. Taken with 10,812 registers and 3 M10K, this is ruling X7
measured from two directions at once: the arrays are read combinationally, so
they cannot be inferred as M10K, so they become flip-flops, so the block is
both large and slow. Rebuilding it around the C0–C4 synchronous seam is already
required by Phase 2 and this is the evidence for why.

### `rcp24_svc`, 68.46 MHz — SUSPECT, and it should not be repaired yet

    m1_i_q[1]  ->  r_o[7]     slack -4.527, data delay 8.516, CLOCK SKEW -5.951

**The data delay is 8.5 ns — comfortably inside 10 — and the path fails on
5.95 ns of clock skew** into an output register. Nearly six nanoseconds of skew
on a single-clock leaf is not a logic problem; it is what an unconstrained
virtual-pin placement does at a block boundary.

This row should be re-measured before anyone touches the RTL. Rewriting a block
because of a placement artefact is how a wrong number becomes a wrong design,
and the ruling for this block says plainly: **"no architectural rewrite
justified."**

## What this does and does not say

**It does say:** the texture island as written cannot be clocked at the
renderer's rate, three of the four worst blocks have specific named causes, and
two of those three were already predicted by the buildability ruling before any
of this was measured.

**It does not say** that the console is 54.95 MHz. These are leaf fits.

* All I/O is **virtual** — no package pins, no board I/O delay, no PLL.
  `texjoin_v2` carries **829 virtual pins**, which is a great deal of
  unconstrained fabric I/O for one leaf and depresses its number by an unknown
  amount.
* `5CSEBA6U23I7` is **provisional**, not board truth.
* A per-block fit says nothing about the composed machine's routing — and
  composition normally makes things **worse**, not better.
* **Nothing here is a programmed device.**

**It does not say the blocks are wrong.** Every one of them passes its directed
suite and several pass against a shipped hardware oracle. They compute correct
results. They were written for function and never once fitted, which is the
whole reason this page exists — *"a source file exists and a directed test
passes" is not the same maturity as production-buildable hardware.*

## What follows

1. **Re-fit `rcp24_svc`** before touching it. Skew, not logic. *(running)*
2. **`aux_pipe`**: input boundary registered 2026-09-03; re-fit pending.
   54.95 MHz was the *before*. *(running)*
3. **`tmu_plan`**: narrowed 2026-09-03; re-fit pending. 93.55 MHz was the
   *before*. *(running)*
4. **`texjoin_v2`**: the X3 restructure, which was already required. The scan
   is the wall and the fit now says so with a path name.
5. **`perspuv_svc`**: the two-lane rebuild R7 already ordered, with the rescale
   pipelined after the products rather than inside the cone.
6. **`cache_pipe`**: the C0–C4 synchronous seam, so the arrays become memory.
   This is the one that also buys back most of the 37.6 % area.
7. Then three seeds on the survivors, then compose.

## Method notes worth keeping

**A `timeout` row is not a timing result.** The first `tmu_plan` attempt
returned `timeout 3385.8s` against a 3000 s budget with a formal proof
competing for CPU. Re-run alone at 9000 s it fitted in 2524 s. The tool's own
header warns that such a row *"reads as 'this block does not fit', when all it
meant was 'we did not wait'."*

**A `contaminated` row is not one either.** It means the sources moved under
the fitter and the tool declined to lie about which commit it measured.

**A missing source used to look identical to a failure.** `failed:quartus_map`
was what `-Module <leaf>` produced when nobody passed `-ExtraSources`, and it
reads in this table as "does not fit". `design/fit_targets.yml` plus a preflight
now separate the two before the fitter spends an hour discovering it.
