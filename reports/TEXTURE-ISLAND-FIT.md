# Texture island: does the 99.5 MHz renderer survive the texture path?

**Three blocks are the problem, not ten.** All ten are fitted. Once each
block's own logic is separated from its virtual-pin boundary, **six of the ten
already meet or beat the 120–125 MHz island target**, and the island's real
floor is **`perspuv_svc` at 62.67 MHz** — the block ruling R7 had already
ordered rebuilt with two product lanes.

Answering the question asked on 2026-09-02:

> "the important bit is actually fitting all the texture stuff to see if the
> 99.5 MHz renderer and full fitted console actually holds up or if it needs
> more reingeneering."

**It needs reengineering in three named places, and the rest of the island is
already fast enough.** That is a different and much better answer than this
page gave on its first pass, and the difference is a measurement error of mine
rather than any change to the RTL.

---

## THE CORRECTION THIS PAGE EXISTS TO CARRY

The first version of this report said:

> the island's floor is **54.95 MHz** — barely half the console clock … seven
> of nine sit below the shipped shell's own 99.50 MHz

**Most of those numbers were the virtual-pin boundary, not the block.** From
`aux_pipe`'s worst path:

    From Node  req_wx_i[3]        Type iExt — an external input
    To Node    u_div|LessThan0~26
    Data path  18.944 ns, of which THE FIRST INTERCONNECT HOP IS 9.985 ns

Nine hundred picoseconds short of the entire clock period, in one wire, before
any logic at all. That is a virtual pin the fitter placed wherever it liked, in
a design with no neighbours to place it near. `aux_pipe`'s **own logic runs at
120.37 MHz.** I called it "the slowest thing on the island" and rewrote it.

This is the same error the project has made twice before and it wears new
clothes each time: **a measurement across mismatched things, reported as a
measurement of the thing.** The leaf fit compares a block against a placement
it will never have.

`tools/quartus/internal_paths.py` now separates the two from the archived
reports, re-fitting nothing.

## The rows

Provisional device `5CSEBA6U23I7`, Quartus 17.0.2 Lite, all I/O virtual.
**Internal** is the worst register-to-register path — the block's own logic.
**Reported** is the worst path of any kind, which is what the fitter's Fmax
column says. Sorted by internal, slowest first.

| block | internal | reported | ALM | reg | M10K | DSP | worst internal path |
|---|---:|---:|---:|---:|---:|---:|---|
| `perspuv_svc` **rebuilt** | **99.14** | 82.00 | 2204 | 3293 | 1 | 6 | `pk_i → p1_prod_q` |
| `perspuv_svc` *(one lane, before)* | *62.67* | *62.67* | *1792* | *2827* | *2* | *3* | *`p1_prod_q → e_q`* |
| `rcp24_svc` | **73–80** | 68.5 | 1041 | 1101 | 0 | 6 | `m1_i_q → c_m.raddr_a` |
| `cache_pipe` | **81.06** | 81.06 | 5634 | 10812 | 3 | 0 | `rq_rp → valid_r` |
| `texjoin_v2` | **93.12** | 93.12 | 3824 | 7151 | 4 | 0 | `wq_rp → tmu_v_q` |
| `tmu_plan` | 110.57 | 88.54 | 1142 | 1380 | 0 | 0 | `t2_iv1[9] → t3_row1[4]` |
| `aux_pipe` | 120.37 | 63.63 | 1182 | 1598 | 1 | 0 | `u_div rv_q → u_div ru_q` |
| `palette_res` | 121.48 | 104.42 | 152 | 141 | 2 | 0 | `gen_r → gen_r` |
| `mosaic` | 124.07 | 86.63 | 197 | 192 | 0 | 4 | sample-counter carry |
| `bilerp_lane` | 125.88 | 99.69 | 125 | 177 | 0 | 3 | `Add1 → b1_b_q` |
| `rsp_dispatch` | 126.06 | 110.90 | 806 | 1432 | 0 | 0 | `raw_n → cq_d` |

**The four in bold are where the reported number IS the internal number** —
their own logic is the limit, and those are the four to work on. The other six
are limited by a boundary that will not exist once they have neighbours.

### The three real repairs

1. **`perspuv_svc` — DONE, 62.67 → 99.14 MHz internal.** The old worst path
   `p1_prod_q[36] → e_q[5][1][8]` was a product register through variable
   rescale and saturation into entry storage, all one cone. Rebuilt per R7 as
   two parallel product lanes with the rescale pipelined after them, in four
   registered stages. **1.00 → 1.99 products per clock**, measured with the
   input saturated, which is what carries three-sample materials past R7's
   1.642 requirement.

   Its new worst internal path is `pk_i~5 → p1_prod_q[0][36]` — **the 16-entry
   priority scan feeding the multiplier**, which is the same class of structure
   the TEXJOIN rebuild replaced with a work FIFO and which was deliberately
   left alone here. It is the next thing to take, if 99.14 is not enough.

   +412 ALM and 3 more DSP for the second lane, exactly as expected.
2. **`cache_pipe`, 81.06 MHz — REBUILT, refit running.** `rq_rp[1] →
   valid_r[1][2]`, and the resource columns said the same thing louder:
   **10,812 registers against 3 M10K.** A texture cache keeping tags and lines
   in flip-flops did not infer as memory.

   Now C0–C4 with the arrays read through a registered address and an explicit
   capture stage between the memory output and the compare. **The acceptance
   question is inference, not Fmax** — X7 refuses a cache fit as closure "if
   the RAMs become flops/MLABs or an M10K output launches a broad combinational
   path", so the number to read on the refit is the M10K count, not the clock.

   Two real bugs were caught by the existing suite while rebuilding it: the
   memory output register is overwritten every clock and was being read two
   stages later, and `valid` was sampled one clock after the tag — which during
   a fill disagree by construction, so a probe could see the old tag with the
   new valid and call a miss a hit.
3. **`rcp24_svc`, 73–80 MHz.** Worst internal path ends at
   `c_m.raddr_a[...]` — a memory address register. Its ruling says **"no
   architectural rewrite justified"**, and the internal number is much better
   than the 68.46 that was reported, so this is the one to leave alone longest.

`texjoin_v2` at 93.12 is close enough to be a composition question rather than
a rewrite.

**The island's internal floor has moved twice today**: from `perspuv_svc` at
62.67, to `cache_pipe` at 81.06, and — if the cache rebuild lands where its
structure now says it should — to `texjoin_v2` at 93.12. That is the shape of
progress here: each repair promotes the next block to being the problem.

### The area

**15,749 ALM ≈ 37.6 % of the device** for the texture island alone, before
geometry, terrain, particles, post or the existing shell. `cache_pipe` is over
a third of that on its own, and the same seam rebuild addresses it.

## Seeds

`rcp24_svc` was fitted at three seeds: **68.46 / 68.63 / 63.93**, a spread of
4.70 MHz against the measured noise floor of **4.61 MHz**
(`reports/NOISE_FLOOR.md`). Its number is a real number, not a bad draw.

**And getting that took fixing the tool.** `run_shell_fit.ps1` has had `-Seed`
since the noise floor was measured; `run_block_fit.ps1` never did. So when
`rcp24_svc` was re-fitted to test whether its clock skew was a placement
artefact, it returned 68.46 MHz with the same worst path to three decimal
places — which confirmed nothing at all, because **Quartus is deterministic for
a given design and seed.** The second number was the first number. That is the
wrong experiment, run in earnest, and it was the only experiment the tool could
perform.

## What changed in the RTL, and what it bought

| change | before | after |
|---|---|---|
| `texjoin_v2`: 13-level priority scan → work FIFO, registered outputs | 61.66 | **93.12** |
| `aux_pipe`: input boundary registered | 54.95 | 63.63 reported; internal 120.37 |
| `tmu_plan`: 32-bit arithmetic narrowed to `MAXLOG2` | −0.690 ns | **+0.956 ns** on the same cone |

The `tmu_plan` row is the honest form of a claim I first got wrong. Its
*reported* number went **down**, 93.55 → 88.54, and I wrote that up as "no
measurable Fmax change, a real area win". Compared like with like — internal
path to internal path, through the same wrap-and-shift cone — it is
**−0.690 → +0.956 ns**, 1.65 ns on exactly the cone the narrowing targeted,
with ALM down 1419 → 1142.

`aux_pipe`'s row is the honest form of the other one. The input register did
what it was for — it shortened the *boundary* path, and the reported number
moved 8.68 MHz, about twice the noise floor. It did **not** buy 8.68 MHz of
logic, and the block was never the island's slowest.

## What this does and does not say

**It does say:** three blocks are limited by their own arithmetic, and each has
a named path and a ruling that already anticipates it.

**It does not say the island runs at 120 MHz.** An internal number is an upper
bound on one block placed alone with the whole device to spread into.
Composition adds inter-block routing and congestion and makes things **worse**,
not better. The 120–125 MHz island target is measured on the *composed*
island across three seeds, and none of that has been done.

* All I/O is **virtual**; `texjoin_v2` carries 830 virtual pins.
* The block SDC declares `set_input_delay 0` / `set_output_delay 0` — a "same
  clock, no external budget" model, deliberately optimistic about inter-block
  routing. Boundary paths are **timed**, not excluded; they are just timed
  against a placement that has no neighbours.
* `5CSEBA6U23I7` is **provisional**, not board truth.
* **Nothing here is a programmed device.**

**It does not say the blocks are wrong.** Every one passes its directed suite,
several against a shipped hardware oracle.

## Method notes worth keeping

**A `timeout` row is not a timing result.** `tmu_plan` returned
`timeout 3385.8s` against a 3000 s budget with a formal proof competing for
CPU; re-run alone at 9000 s it fitted in 2524 s.

**A `contaminated` row is not one either.** `cache_pipe`'s first attempt
returned `contaminated:source-changed-during-fit` after 4,550 s because I
edited four files in its shared `-ExtraSources` list mid-fit. The tool declined
to attach a number to sources it could not name.

**A missing source used to look identical to a failure.** `failed:quartus_map`
was what `-Module <leaf>` produced when nobody passed `-ExtraSources`.
`design/fit_targets.yml` plus a preflight now separate the two before the
fitter spends an hour discovering it.

**200 archived paths were not enough.** For `aux_pipe`, `rsp_dispatch` and one
seed of `rcp24_svc`, **all 200 worst paths touched a port**, so the report
contained no register-to-register path at all. A leaf with hundreds of virtual
pins fills its worst-path list with its boundary before reaching its
arithmetic. `block_paths.tcl` now asks for 2000.

---

# ADDENDUM 2026-09-03 — MEASURED AGAINST THE SPECIFICATION, THE REBUILD IS A REGRESSION

The fit numbers above were reported without comparing them to
`reports/islandrearchitecture5.md`, the owner brief committed today at 08:04
("Agent please read, full brief!") which supersedes `Islandrearchitect{,2,3}.md`
and `islandrearchitecture4.md`. That document is not advice; it is the
**ALM / DSP / REGISTER RECOVERY SPECIFICATION** this rebuild exists to satisfy,
and it carries explicit numeric tripwires. Comparing:

|                          | ALM | registers | M10K | DSP |
|---|---|---|---|---|
| island **as built today** | **18,497** | **28,143** | **10** | **25** |
| spec target | 6,600 | 6,050 | 37-64 | 11-13 |
| spec **hard redline** | 7,500 | 9,000 | 64 | 14 |
| the prototype it replaces | 15,749 | 25,123 | 11 | 16 |

**2.47x the ALM redline, 3.13x the register redline, 1.79x the DSP redline —
and worse than the prototype on every axis except DSP.** The prototype's
diagnosis was "25,123 registers against only 11 M10Ks: state that belongs in
memories was implemented as wide flip-flop arrays". The rebuild moved that
number to 28,143 registers against 10 M10Ks. It went the wrong way.

## The specific failure: TEXTURE.CACHE.V2

Section 10 of the brief names this block "THE ALM RECOVERY CENTRE" and sets:

    cache v2:  require M10K >= 8    reject registers > 2,000    reject ALMs > 1,500

Measured, and set beside the block it was written to replace:

| | ALM | registers | M10K |
|---|---|---|---|
| `zhao_texture_cache_pipe` (the C0-C4 rebuild) | 5,903 | 11,328 | **2** |
| `zhao_texture_cache` (what it replaces) | 1,087 | 1,737 | **4** |
| tripwire | <= 1,500 | <= 2,000 | >= 8 |

The rebuild is **5.4x the ALMs, 6.5x the registers and HALF the M10Ks** of the
block it supersedes, and it fails all three tripwires — the M10K one by being
below a *minimum*, which is the tell that the storage never became storage.

**This corrects a claim made earlier today.** The C0-C4 result was reported as
a pass on the grounds that "the acceptance question was M10K inference, not
Fmax, and the RAMs stayed RAMs: 2 M10K, 0 DSP". Two M10Ks is not the RAMs
staying RAMs. The requirement is eight, the predecessor already had four, and
11,328 registers is where the array actually went. The brief legislates against
exactly this reading:

> A fit that meets Fmax while violating its memory/DSP structure is not a pass.

98.66 MHz was true and is not the point.

## This was already the owner's instruction

The brief's own KEEP / REWRITE / DEFER table lists under **REWRITE BEFORE
INTEGRATION**:

    current cache_pipe storage and hit path
    current TEXJOIN wide storage organization
    current PERSPUV token table and one-product lane
    current RCP scans and 32x64 product expression

So three of the four blocks reported as fit victories this morning are on the
brief's rewrite list, and the fits confirm why: `texjoin_v2` at 3,824 ALM /
7,151 reg against a 900 / 1,200 budget, `perspuv_svc` at 2,204 / 3,293 against
900 / 700.

The brief also states the constraint that governs the rest of the session:

> REJECT: adding terrain/Field RTL faster than the texture fit can be closed.

## What this does NOT change

The `zhao_prod_top` production-only resource fit still answers the owner's
question in `WeNeedSomeMeasurements.md` and is still worth finishing: it
measures the machine as it stands, and "as it stands" now has a known,
quantified 11,000-ALM recovery target sitting inside it. The 72% fabric figure
in that brief was computed from a ~17,000-ALM island estimate; the measured
island is 18,497, so the honest number is slightly worse than the owner feared
— and the recovery specification for it already exists and is unimplemented.
