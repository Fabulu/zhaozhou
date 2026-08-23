# Task Log: RUN-20260823-1736 — TEXTURE.TMU DSP rearchitecture

**Created:** 2026-08-23 17:36 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260823-1736-texture-tmu-dsp-rearchitecture/

---

## Objective

Take `zhao_texture_tmu` from a measured **28 DSPs** to the derived target of
**6–9**, without moving a single sample by one LSB, and report **Fmax** and
**samples/frame** against the derived demand of **850,000 samples/frame**.

Full argument in `SPEC_v1.md`.

---

## Progress Timeline

### 2026-08-23 17:36 — Task started

- Run ID from the in-repo `runs/CLAUDE-RUNS/init-run.ps1`.
- HEAD `0f3245a`. `git -c core.autocrlf=true status --porcelain -- fpga/rtl` is
  **empty** — the tree is clean where it matters.
  (A first `git status` without `core.autocrlf=true` listed ~40 files as
  modified. They are line-ending phantoms; recorded because a dirty tree is the
  first thing that would invalidate a fit's `rtlCleanAtHead`.)
- Baseline read from `reports/synthesis/zhao_block_fit.json`:
  `zhao_texture_tmu` = **1,839 ALMs / 310 registers / 28 DSPs / 0 RAM**,
  `zhao_texture_bilerp` = **38 ALMs / 0 registers / 7 DSPs**, both at
  `sourceCommit 96c0394`, `rtlCleanAtHead: false`, and **no `fmaxMhz` field on
  either** — those rows predate the constrained-SDC fix (`QUARTUS_GOTCHAS.md` §7).

### 2026-08-23 17:4x — Reading, before any RTL

Read in full: `design/contracts/TEXTURE.TMU.md` (the whole file — several of its
properties are load-bearing and stated nowhere else),
`fpga/rtl/texture/zhao_texture_tmu.sv` (707 lines),
`fpga/rtl/texture/zhao_texture_bilerp.sv` (114),
`tests/formal/texture_bilerp.sby` + `texture_bilerp_fv.sv`,
`tests/texture/texture_tmu_dev.hpp`, `reports/QUARTUS_GOTCHAS.md`,
`design/budgets/dsp.md`, `design/budgets/latency.md`, the `THE THREE DEMAND
NUMBERS` docket entry, `tools/sweep_surface_stamp.sh`,
`tools/quartus/run_block_fit.ps1`, and
`runs/CLAUDE-RUNS/RUN-20260823-1415-surface-stamp-dsp-rearchitecture/TASK_LOG.md`
as the depth model.

### 2026-08-23 17:5x — **Step 1, ledger rule V17: the oracle resolves**

Run *before* any RTL, as the rule requires:

    npm run ledger:check
    => CHECK FAILED — 1 error(s) against 92 blocks / 40 ops
       V16: FIELD.SEQ.CORE is RTL_VERIFIED but formal
            "tests/formal/field_seq_bound.sby" is recorded as "pending"

**Same single error SURFACE.STAMP and GEOM.SKIN baselined against — the Field
agent's gate, not mine.** V17 is green. Ledger baseline for this run = 1 error.

Checked this block's cited symbols by hand rather than trusting the aggregate:

| cited symbol | resolves to |
| --- | --- |
| `zref::Tmu` | `reference/include/zref/zref_texture.hpp:122` |
| `zref::Tmu::plan` | `reference/include/zref/zref_texture.hpp:209` |
| `zref::Tmu::level_offset_texels` | `reference/src/zrender/texture.cpp:154` |
| `zref::sky::rgb565::to_rgb888` | `reference/include/zref/zref_sky.hpp:202` |
| `zref::terrain::mirror_texel` | `reference/include/zref/zref_terrain.hpp:202` |
| `zref::FragmentPipeline::star_disc_masked` | `reference/src/zrender/fragment.cpp:178` |
| `zref::render::TerrainTileset` | **DOES NOT EXIST** |

**A defect found by doing this by hand.** The contract, the RTL header and
`reference/src/zrender/texture.cpp:224` all cite `zref::render::TerrainTileset`
as the authority for the row-major layout choice. The struct is
`zref::render::Tileset` (`reference/include/zref/zref_render.hpp:166`).
Everything the citation *claims* is exact — `uint16_t palette[256]`,
`uint8_t tiles[256][64*64]`, and `rast.cpp:250` reading it as
`tiles[tile][(ty << 6) + tx]` — so the argument stands and only the name is
wrong. It is a documentation defect in three files and it is fixed in this run,
because a citation that does not resolve is exactly what V17 exists to catch and
this one slipped past by living in prose rather than in the ledger.

### 2026-08-23 17:5x — Where the 28 DSPs are. Answer: all of them, in one 114-line file

The run brief's second lesson is "look for multipliers in the boring places",
because SURFACE.STAMP's 28 were all in coverage geometry and two were per-stamp
constants that got silicon anyway. **I looked, and here the answer is the boring
one — which is itself the finding.**

    grep -n " \* " fpga/rtl/texture/zhao_texture_tmu.sv     -> nothing
    grep -n " \* " fpga/rtl/texture/zhao_texture_bilerp.sv  -> 8 lines

The only `*` in the 707-line TMU are `32*k` / `16*k` inside `+:` part-selects.
And the shipped fit agrees to the digit: bilerp = 7 DSP, TMU = 28 DSP,
**28 = 4 × 7 exactly**. The contract already recorded that as a failed
prediction on 2026-08-21 ("THE FIT SAID IT DID NOT HAPPEN") — it expected the
four instances' identical weight products to be shared and they were not.

**Why the addressing costs nothing is worth naming**, because it is the reason
the brief's second lesson does not bite here: `LOG2W`/`LOG2H` make texel
conversion a shift (`u_raw << log2s`), the mip level offset a repunit table plus
**one** variable shift, and the row-major index `(v << log2w) + u` another
shift. Every one would be a multiply if texture dimensions were arbitrary.
**That is the concrete reason the brief forbids non-power-of-two support**, and
it is load-bearing rather than stylistic.

**Found while counting: `QUARTUS_GOTCHAS.md` §5 in the wild, four times over.**
The four texel products are declared `{17'd0, t00_i} * {8'd0, w00}` — a **25×25**
multiply whose honest need is **8×17**. The zero-extensions were written to make
widths line up for the addition. §5 measured that same mistake costing
`zhao_geom_lod` ten DSP blocks.

### 2026-08-23 18:0x — Design fixed (details in SPEC_v1.md)

**Move 1 — factor the exact integer expression.** `A = (t00<<8) + (t10−t00)·fu`,
`B = (t01<<8) + (t11−t01)·fu`, `S = (A<<8) + (B−A)·fv`, `out = (S+32768)>>16`.
Expanding `S` gives the contract's four-weight law term for term. **Three
products per channel instead of eight; 12 across the block instead of 32.**

**This is NOT the form `spec/qformats.md` §3 refuses, and the distinction is the
whole argument.** The single-rounding law rejects "two lerps then a lerp"
because the textbook writes each lerp as a *rounded* unit8 blend — three
`rescale` calls. The form above is the same algebraic factoring with **no
intermediate rescale at all**: `A` and `B` are exact integers, `S` is the exact
25-bit weighted sum, and there is exactly one `(S + 32768) >> 16`.

**The decisive consequence: `zhao_texture_bilerp`'s ports do not change**, so
`tests/formal/texture_bilerp.sby` needs not one line changed and P1–P4 keep
meaning what they meant. P1 computes the law in the harness from free
`fu_free`/`fv_free` and asserts it against the shipping module's output — so it
proves the *factored* form equals the four-product law over all 2^48 inputs,
with no edit. The contract's sanctioned fix (hoisting the weights into the TMU)
would have changed ports, directed tests, the formal harness and the contract,
and would have removed only 12 of the 32 products. **Factoring removes 20 and
touches no interface.**

**Move 2 — `FILT_LANES` ∈ {4, 2, 1}** as the measured frontier: 4 / 2 / 1
instances of the unchanged bilerp module, with the four channels time-multiplexed
through them in 1 / 2 / 4 passes, for 12 / 6 / 3 products. The frontier is also
coverage: a mutation in the pass counter or the channel mux is invisible at
`FILT_LANES = 4` (one pass, the mux degenerates) and visible at 2 and 1 — the
same shape as SURFACE.STAMP's S03/S04.

### 2026-08-23 18:0x — THE OTHER PROBLEM, which is not a DSP problem

The brief asks for samples/frame against the 850,000 demand. **The answer today
is 0.33× and the contract already says so** — under "Target throughput" it
records the ledger's "1 sample per clock" as "**NOT met by this increment**",
at one sample per 4 clocks (direct) and per 6 (CLUT).

| path | II today | samples/frame at 100 MHz | vs. 850,000 |
| --- | ---: | ---: | ---: |
| direct colour | 4 | 416,667 | 0.49× |
| **CLUT — the terrain path** | **6** | **277,778** | **0.33×** |

**And the DSP problem and the throughput problem do not touch.** Every DSP is in
the filter; the filter is bypassed entirely for CLUT, which is the path that
misses demand worst. So the work splits into two commits — **A: the arithmetic,
B: the pipeline** — and A is committed and pushed before B is started, so that
the assigned target lands even if B cannot be closed.

### 2026-08-23 18:1x — MEASURED: the pristine baseline, and **my prediction was wrong**

Fit from the **untouched** tree, before a line of RTL moved, because the shipped
row carries no Fmax at all:

    tools/quartus/run_block_fit.ps1 -Module zhao_texture_tmu `
      -ExtraSources fpga/rtl/texture/zhao_texture_bilerp.sv,fpga/rtl/texture/zhao_texture_tmu.sv `
      -RowLabel "@pre-rearch" -KeepWorkspace

Quartus Prime Lite 17.0.2, 5CSEBA6U23I7, virtual pins, sourceCommit `d284a86`,
`rtlCleanAtHead: true`, 335.3 s. **Constrained** — evidence captured live from
the workspace before the harness deleted it, saved beside this log at
`fit-evidence/pre-rearch_constraint.txt`:

    Info (332111): Found 1 clocks
    Info (332111):   Period   Clock Name
    Info (332111): ======== ============
    Info (332111):   10.000          clk

| | measured |
| --- | ---: |
| ALMs | 1,844 / 41,910 |
| registers | 310 |
| **DSP blocks** | **28 / 112 (25%)** |
| RAM blocks | 0 |
| **Fmax (Slow 1100mV 100C)** | **199.72 MHz** |

**SPEC_v1.md predicted 45–70 MHz and named the suspect. It was wrong, and being
wrong here is the useful result.** The whole reason the brief ordered a
`@pre-rearch` re-fit is that SURFACE.STAMP's "met, measured" throughput claim
turned out to be true about cycles and false about time — that block was holding
`gpu_clk` to 32.33 MHz. `reports/REMAINING_BLOCKERS.md` was amended at `d284a86`
to say every contract's "met" should be suspected until someone measures
seconds-per-item.

**Suspected, measured, and cleared: this block closes at 199.72 MHz, twice its
100 MHz constraint.** The contract's two named worries — the 48-bit shift and
wrap fold in one combinational cone, and "the 32 multiplies are all in the
sample cone" — are real cones and neither is critical. That is worth as much as
a confirmation would have been: it says the 28 DSPs were **not** buying a
degraded clock, so unlike SURFACE.STAMP there is no second problem hiding behind
the first, and it changes what the throughput number means (see below).

**Which changes the samples/frame formula, and I am changing it in the open.**
SURFACE.STAMP published `(Fmax / 60) / (cycles per item)`. That is right only
while `Fmax < 100 MHz`, because the block cannot be clocked above the shared
`gpu_clk` however fast it closes. The general form is

    items/frame = min(Fmax, 100 MHz) / (60 x II)

which reproduces every figure SURFACE.STAMP published (its Fmax was 87.54) and
gives this block **1,666,667 / II** rather than 3,328,667 / II. Using the raw
Fmax here would have overstated the block by exactly 2x — the same category of
error as quoting a nominal rate for a measured one, which is the mistake that
run corrected four published numbers for.

### 2026-08-23 18:2x — RTL landed, and the differential passed FIRST TRY at all three settings

`zhao_texture_bilerp.sv` rewritten to the factored form (ports unchanged);
`zhao_texture_tmu.sv` given `FILT_LANES`, the channel multiplex, `ST_FILT` and a
3-bit state. **Verified before building** that the factoring is the same integer
as the law, by brute force over all 65,536 `(fu, fv)` against eight corner
footprints plus 400,000 random ones, with the width bounds asserted at every
step: **0 mismatches**, the nearest identity holds for all 256 texel values, and
the named tie `t = (0,255), fu = 128, fv = 0` still gives **128, not 127**.

    test_texture_tmu_directed  (FILT_LANES = 4)   76 / 76
    test_texture_tmu_lanes2    (FILT_LANES = 2)   76 / 76
    test_texture_tmu_lanes1    (FILT_LANES = 1)   76 / 76
    test_texture_tmu_random                        8 / 8
      3,749 bilinear samples (476 at a rounding tie, 26 at the identity),
      2,052 mipped, wraps 8,588/2,036/1,535, 1,122 mode errors, 5,499 clean

Not one sampled byte moved at any setting — which is the point: `FILT_LANES` is
a resource axis, not a behaviour axis.

### 2026-08-23 18:2x — the block had NO throughput test at all, and now it has one

`test_backpressure_and_latency` asserted accept-to-retire ≤ 16 and byte
stability across nine timing patterns. **Nothing anywhere measured the sustained
rate.** The ledger's "1 sample per clock" and the contract's "one per 4 / one
per 6" were both prose. That is precisely the shape `REMAINING_BLOCKERS.md` now
warns about, sitting in the suite that was supposed to catch it.

`test_throughput_against_the_derived_demand` measures both initiation intervals
on an always-hit cache and asserts them **exactly**, so neither can drift
unnoticed:

| FILT_LANES | CLUT II | direct II | CLUT samples/frame | vs. 850,000 |
| ---: | ---: | ---: | ---: | ---: |
| **4 (default)** | 6 | **4** | 277,777 | **0.33x** |
| 2 | 6 | 5 | 277,777 | 0.33x |
| 1 | 6 | 7 | 277,777 | 0.33x |

The integer division is exact rather than approximate and the test says why: the
drain is strictly less than the interval on every path (3 < 4, 5 < 6, 6 < 7).

**The CLUT interval does not move with FILT_LANES, and that is the finding.** A
palette is never filtered, so the demand-critical path — terrain is CLUT8 —
does not touch the filter at all. The DSP problem and the throughput problem
live in different halves of the block.

### 2026-08-23 18:3x — the default is FILT_LANES = 4, which is the OPPOSITE of SURFACE.STAMP's choice, for the opposite reason

SURFACE.STAMP defaulted to its **cheapest** setting because its demand was met
26.9x over and provisioning past a met demand is the error its 28 DSPs came
from. Here the demand is **not** met — 0.33x — so throughput is the scarce
resource and DSPs are not. Four lanes cost **zero** cycles and (predicted) still
land inside the 6–9 target; two lanes would trade a cycle this block does not
have for DSPs it does not need to save. Same principle, opposite answer, and the
answer is only knowable because both numbers were measured.

**This is provisional until the three fits report.** If FILT_LANES = 4 lands
above 9 DSPs the default drops to 2 and the cycle is paid.

### 2026-08-23 18:3x — the mutation preflight rejected four of my own mutations

    linted 30 mutants at FILT_LANES (4, 2, 1), 4 do not build
      B07 the two sub-texel fractions are swapped        UNUSEDSIGNAL
      B13 the unit8 complement never reaches 256         UNUSEDSIGNAL
      M13 the lane base does not scale with FILT_LANES   UNUSEDPARAM
      M16 ST_FILT leaves one pass early                  UNUSEDPARAM

Same shape as SURFACE.STAMP's five: each orphaned a signal or a parameter and
tripped `-Wall`. Under guard 5 they would have read as "discarded"; before guard
5 existed they would have read as **caught**. **Fixed the mutations, not the
guard**, and one of the rewrites is better than what it replaced: M16 became
"`LAST_FILT_PASS` is always 0", which is **textually identical in effect at
FILT_LANES 4 and 2** and a real defect only at 1 — the deepest frontier mutant
in the table, and one no default build could ever reach.

    linted 30 mutants at FILT_LANES (4, 2, 1), 0 do not build

### 2026-08-23 18:1x — `git add -A` staged 288 files, and it nearly went in

Committing move 1 with `git add -A` staged **288 files, 91,980 insertions and
90,431 deletions**. None of it was mine. The repository's working tree is full
of **line-ending phantoms**: `git status` without `core.autocrlf=true` reports
about forty files as modified when their content is identical, and `git add -A`
does not merely report them, it **normalises and stages** them.

Caught by reading `git diff --cached --stat` before committing rather than
after. Reset, then staged the ten paths of this run **by name**. The rule for
the rest of this run, and it costs nothing: `git add -- <explicit paths>`, never
`-A`.

This is the same failure class as everything in `reports/QUARTUS_GOTCHAS.md` —
a tool doing something reasonable and unrequested, with no symptom except a
number nobody looked at. Here the number was in the staging summary.

(Separately: the first `git commit -m @'…'@` PowerShell here-string died with
`fatal: /: '/' is outside repository`, git having parsed part of the message as
a pathspec. Switched to `git commit -F -` with a heredoc. Recorded because a
commit that fails *loudly* is the good case and it still cost a cycle.)

### 2026-08-23 18:1x — another session is committing to this repo, concurrently

HEAD moved twice underneath this run without my touching it: `0f3245a` →
`d284a86` ("BLOCKERS: suspect every 'throughput met' claim in every contract")
→ `8e7f974` ("Docket the Field rearchitecture ruling"). Both are by the owner's
account with a Claude co-author trailer, and **neither touches `fpga/rtl`** —
checked, not assumed, because a fit's `rtlCleanAtHead` and `sourceCommit` are
only meaningful if nothing else is moving the RTL.

Recorded because the `@pre-rearch` fit row carries `sourceCommit d284a86`, which
is **not** the commit that was HEAD when this run started, and a reader
reconstructing the timeline later would otherwise find that inexplicable.

### 2026-08-23 18:5x — MEASURED at FILT_LANES = 4: 28 → 12 DSPs, and **THREE of my five predictions were wrong**

    tools/quartus/run_block_fit.ps1 -Module zhao_texture_tmu `
      -ExtraSources fpga/rtl/texture/zhao_texture_bilerp.sv,fpga/rtl/texture/zhao_texture_tmu.sv `
      -KeepWorkspace

sourceCommit `7403deb`, `rtlCleanAtHead: true`, 894.9 s, **constrained**
(`Info (332111): 10.000 clk`, saved as `fit-evidence/lanes4_constraint.txt`).

| | @pre-rearch | FILT_LANES = 4 | delta |
| --- | ---: | ---: | ---: |
| ALMs | 1,844 | **1,951** | **+107** |
| registers | 310 | **294** | −16 |
| **DSP blocks** | **28** | **12** | **−16** |
| **Fmax** | **199.72 MHz** | **192.46 MHz** | **−3.6%** |

**THE FINDING: twelve products became twelve DSP blocks. One each. No packing
at all.** SPEC_v1.md predicted 4–8 and reasoned that a Cyclone V
variable-precision block does three 9×9 *or* two 18×19, so eight 9×9s and four
18×9s should have packed into about five. **Quartus 17.0.2 Lite packed
nothing.** Every inferred `*` got its own block regardless of operand width.

That is the same tool behaviour this contract already recorded once, from the
other side: the four bilerp instances' identical weight products "did not share"
either. The generalisation, and it is the useful output of this fit:

> **On this kit, DSP blocks = the number of `*` operators in the elaborated
> design.** Not the number of distinct products, and not the number of DSP-sized
> multipliers the operands would fit into. Operand width buys nothing back once
> the operator exists (`QUARTUS_GOTCHAS.md` §5 is the converse — width can make
> it *worse*).

**Which falsifies the default choice I recorded an hour ago.** 12 misses the
6–9 target. `FILT_LANES = 2` is 6 products, so by the rule above it should be
**6 DSPs** — the bottom of the target — and the docket's own prediction ("about
5–6 DSPs at half rate") was right where mine was wrong. Fitting to confirm
rather than asserting it.

**The other two wrong predictions, both about direction:**

- **ALMs went UP, +107, where I predicted a fall.** The four 25-bit adder trees
  did go away; what replaced them is two 9-bit subtracts, two 17-bit adds and a
  27-bit add per channel, plus the channel multiplexer. I had counted the
  removals and not the additions.
- **Fmax went DOWN 3.6%, where I predicted it would improve.** The reason is
  structural and I should have seen it: the old filter was four *parallel*
  multiplies feeding one adder tree, depth ≈ mult + 2 adds. The factored form is
  **serial** — mult → add → sub → mult → add — because the V lerp cannot start
  until the U lerps finish. Fewer, narrower multipliers, longer path.
  199.72 → 192.46 MHz is still **1.92× the 100 MHz constraint**, so it costs
  nothing real; but "fewer multipliers must be faster" was an assumption, not an
  argument, and it was wrong.

**Registers fell 16**, which is the two `bl_*` byte groups the old code held
versus... actually no: nothing in the sequential block changed except `pass_r`
and `fres_r` being *added*. −16 with two registers added is a fitter packing
difference, not a design one, and I am recording it as unexplained rather than
inventing a reason for it.

### 2026-08-23 19:1x — SWEEP RUN 1: one survivor, M18, and it is a TRUE equivalent no test can kill

    M18 a CLUT texel reports the filter's alpha instead of the law's 255  *** SURVIVED ***

The mutation replaces `smp_a_o = q_clut_r ? 8'd255 : fin[3]` with
`smp_a_o = fin[3]`. **It is equivalent for every input this block can be
handed, and the reason is a line in the same file:**

`zhao_texture_tmu.sv:632-633`, `decode16`'s `default` branch — the one CLUT8
(format 0) and CLUT4 (format 2) fall into, because the case only names
ARGB1555 and ARGB4444 — sets **`a_ = 8'd255`**. So on a CLUT sample all four
alpha taps entering the filter are 255, and a flat footprint filters to itself
exactly (formal P1 with P2: `Σw = 65,536`, so `Σ 255·w = 255 << 16` and the
single rescale returns 255). `fin[3]` **is** 255, by two independent
mechanisms, and removing either leaves the other.

**I tried to find a test that kills it and there is none.** A test would need a
CLUT sample whose `fin[3] ≠ 255`; `fin[3]` is `bilerp` of four bytes that
`decode16` has already forced to 255 for every CLUT format code. The input does
not exist. So this is not a coverage gap — the brief's preference for killing an
equivalent over arguing it does not apply, because there is nothing to kill it
with.

**What the sweep actually found is a real fact about the design, and it is worth
more than the score:** the "a CLUT texel's alpha is 255" law is enforced
**twice** — once by the mux the contract documents, and once, accidentally, by
`decode16`'s default arm. The mux is defence in depth, not the sole mechanism.
That is fine and it stays; recording it means nobody later "simplifies" the mux
away believing it is load-bearing, or adds a CLUT arm to `decode16` believing it
is free.

**Not rewritten into a killable mutant.** Replacing M18 with the inverted mux
(`q_clut_r ? fin[3] : 8'd255`) would be caught instantly on the direct-colour
path and would score 30/30 — and would delete the finding. A score bought by
deleting the evidence for it is the failure mode this repository's whole log is
about.

### 2026-08-23 19:2x — SWEEP RUN 1 COMPLETE: 30 / 30 / 30, caught 28, and the second survivor is a REAL GAP

    linted 30 mutants at FILT_LANES (4, 2, 1), 0 do not build
    pristine models 82607e77dbbe/82607e77dbbe, 4 lanes green
    ...
    attempted=30 expected=30 accounted=30 caught=28
    SURVIVOR: M18 a CLUT texel reports the filter's alpha instead of the law's 255
    SURVIVOR: M27 a bilinear request fetches one tap instead of four
    SWEEP-EXIT=0

`attempted == accounted == expected == 30`, so the cross-check passes: every
mutant re-elaborated, every one linked, no discards. Kept as
`sweep_run1_28of30.log`.

**M18 is the argued equivalent above. M27 is NOT — it is a hole in the test
harness, and it is the kind that would have shipped.**

The mutation forces `q_en_r <= 4'b0001`, so a **bilinear** request enables one
cache lane instead of four. On real hardware three of the four taps would then
be garbage. `tests/texture/texture_tmu_dev.hpp` did not notice, because its
modelled cache **ignores `cac_en_o` entirely**:

    for (int k = 0; k < 4; ++k)
      cac_hw[k] = mem.halfword(static_cast<uint32_t>(top_.cac_addr_o[k]));

Four lanes of correct data, always, enabled or not. The **real**
`zhao_texture_cache` says the opposite in its own header
(`zhao_texture_cache.sv:75-76`): "A nearest sample enables lane 0 only
(`acc_en_i = 4'b0001`); lanes 1–3 are then **not looked up, not counted, and
not filled**." A disabled lane's halfword is whatever line that lane happens to
hold — unspecified, and almost never the texel.

So the model was **strictly more generous than the block it stands for**, which
is the one direction a model must never be wrong in. Fixing it, not arguing it —
the brief's own preference, and here it is not even a close call.

### 2026-08-23 19:4x — MEASURED at FILT_LANES = 2: 6 DSPs — and an Fmax of 48.45 MHz that turned out to be the real story

    tools/quartus/run_block_fit.ps1 -Module zhao_texture_tmu ... `
      -TopParameters FILT_LANES=2 -RowLabel "@lanes2" -KeepWorkspace

sourceCommit `8754bd2`, `rtlCleanAtHead: true`, 1,748.7 s.

| | @pre-rearch | LANES = 4 | LANES = 2 |
| --- | ---: | ---: | ---: |
| ALMs | 1,844 | 1,951 | 1,917 |
| registers | 310 | 294 | 313 |
| **DSP blocks** | **28** | **12** | **6** |
| **reported Fmax** | 199.72 | 192.46 | **48.45** |

**Six DSPs — the bottom of the 6–9 target, and exactly what "one DSP per `*`"
predicts from six products.** The docket's own guess ("about 5–6 DSPs at half
rate") was right.

**A four-fold Fmax collapse from adding a 4:1 byte mux is not believable, so I
did not believe it.** `-KeepWorkspace` means the fitted database survives, so
the question is answerable for the cost of a two-second `quartus_sta` re-run
rather than another fit:

    report_timing -setup -npaths 3 -detail path_only

    ; -10.641 ; q_fmt_r[2] ; fres_r[6] ; clk ; clk ; 10.000 ; -0.119 ; 20.462 ;

The worst path is `q_fmt_r → decode16 → ch_pack → the channel mux → the filter
→ fres_r`. **20.462 ns, 8 logic levels.** So I ran the same query against the
other two workspaces, expecting to see the same cone one mux shorter.

**It was not there at all.**

    FILT_LANES = 4 : texture_samples_o[3]  -> texture_samples_o[21]  4.634 ns
    @pre-rearch    : texture_samples_o[19] -> texture_samples_o[27]  4.818 ns

### 2026-08-23 19:5x — THE FINDING: the Fmax column has been measuring the sample counter

Both of those "~195 MHz" figures are the **32-bit saturating sample counter's
carry chain**. Not the filter. Not the address generator. Not the wrap folds.
**The 32 multiplies of the shipped design appear in no timed path whatsoever.**

The reason is in the generated SDC, which I read rather than assumed
(`fit-evidence/sdc_has_no_io_delays.txt` has it in full): it contains eight
`create_clock` lines, `derive_clock_uncertainty`, and **nothing else**. No
`set_input_delay`. No `set_output_delay`. TimeQuest excludes every
pin-to-register and register-to-pin path when none is declared — and this block's
arithmetic runs **from `req_*` pins** and **to `smp_*` pins**. There is almost
nothing in it that is register-to-register except the counter.

`FILT_LANES = 2` differs in exactly one accidental way: `fres_r` is a real
register, so one filter output finally terminates somewhere TimeQuest will look.
**The 48.45 MHz is not a regression I introduced. It is the first time this
block's arithmetic has ever been timed.**

**This is `QUARTUS_GOTCHAS.md` §7 one layer down.** That entry found
`create_clock` resolving to an empty collection, fixed it, and concluded the
Fmax column was real at last. It is real only for register-to-register logic —
and the run brief's lesson 1, which sent me to re-fit the old RTL under the
corrected SDC, is only half-satisfied by doing that. I did it, got 199.72 MHz,
and wrote "suspected, measured, and cleared" in this log two hours ago.
**That sentence was wrong**, and it was wrong in precisely the way the brief
warns about: a number reported confidently, from the right tool, answering a
question I had not checked it was asking.

**Then I asked what a real constraint says.** Applying `set_input_delay` /
`set_output_delay 0` to the *existing* `FILT_LANES = 2` database — again no
re-fit, the same placement — moves the worst path again:

    ; -23.704 ; req_mode_i[8] ; q_addr_r[55] ; clk ; clk ; 10.000 ; 3.360 ; 37.004 ;

**37.004 ns: the address generator.** Which is exactly what this block's
contract has said since the day it was written —

> "the address generator is a 48-bit shift, a wrap fold and a `(v << log2w) + u`
> in one combinational cone from `req_valid_i` to the latched address, **which
> is the longest path in the file**"

— and which nothing had ever measured. The contract was right, the number is
27 MHz, and it is **pre-existing**: that cone is unchanged by this run's work.

Fixed in `tools/quartus/run_block_fit.ps1`: the generated SDC now declares
`set_input_delay -clock clk 0` on every non-clock input and
`set_output_delay -clock clk 0` on every output — the *same clock, no external
budget* model, guarded by a `get_ports clk` test because eight of this design's
71 clock ports are not called `clk`. Validated against a real database before
being trusted (`quartus_sta` accepted it and reported the path above). Recorded
as `QUARTUS_GOTCHAS.md` §9.

**Every other row in `reports/synthesis/zhao_block_fit.json` predates this**, in
exactly the way 47 rows predated §7's fix. That is not this run's to repair, and
it is docketed rather than mentioned.

### 2026-08-23 20:0x — MEASURED: the OLD block, properly constrained, is **36.92 MHz**

`@pre-rearch-io` — the *shipped* pre-rearchitecture RTL (checked out from
`8e7f974`), fitted under the corrected SDC. 1,275.9 s, `rtlCleanAtHead: false`
**deliberately and correctly** (the tree was holding older RTL on purpose, and
the harness said so rather than hiding it).

| | @pre-rearch (clock only) | @pre-rearch-io (clock + I/O) |
| --- | ---: | ---: |
| ALMs | 1,844 | 1,844 |
| registers | 310 | 342 |
| DSP blocks | 28 | 28 |
| **Fmax** | **199.72 MHz** | **36.92 MHz** |

**Same RTL. Same tool. Same device. A factor of 5.4 between them, and the only
difference is whether the SDC declared I/O delays.**

So the answer to the run brief's first lesson, on this block, is **yes after
all**. Two hours ago I wrote "suspected, measured, and cleared — this block
closes at 199.72 MHz, twice its constraint" and drew the conclusion that unlike
SURFACE.STAMP there was no second problem hiding behind the first. **There was.
It is the same problem, and the measurement I used to rule it out was measuring
the sample counter.** `zhao_texture_tmu` was holding `gpu_clk` to **37% of its
constraint**, which is within noise of the 32% SURFACE.STAMP was holding it to.

The correction is in this log rather than replacing what it corrects, because
the wrong reading and the reason for it are the useful part.

### 2026-08-23 20:0x — SWEEP RUN 2 (the shipping RTL): 30 / 30 / 30, caught 29

    linted 30 mutants at FILT_LANES (4, 2, 1), 0 do not build
    pristine models 59747279bbfe/59747279bbfe, 4 lanes green
    ...
    attempted=30 expected=30 accounted=30 caught=29
    SURVIVOR: M18 a CLUT texel reports the filter's alpha instead of the law's 255
    SWEEP-EXIT=0

Scored against `1590bb6` — the shipping default `FILT_LANES = 2` — in the
worktree, detached, with the mutant count verified non-zero before anything was
scored. Kept as `sweep_run2_final_29of30.log`.

**M27 is dead**, killed by the harness fix rather than argued away: the modelled
cache now returns the complement of the texel on a lane `cac_en_o` did not
enable, which is the direction the real block behaves in.

**M18 survives and is the one true equivalent**, argued above against
`zhao_texture_tmu.sv:632-633`. No input exists that can distinguish it.

**Both sweep logs are kept**, deliberately: run 1 (28/30) is the *evidence for
why the harness changed*, and a repository that kept only the final number would
have lost the reason.

### 2026-08-23 20:1x — I REVERTED MY OWN REARCHITECTURE IN A COMMIT ABOUT YAML

`git show --stat 5f5ffbf` — a commit whose message is one paragraph about a
`target_throughput` value breaking a YAML parse:

    design/blocks.yml                       |   9 +-
    fpga/rtl/texture/zhao_texture_bilerp.sv | 145 +++++-----------
    fpga/rtl/texture/zhao_texture_tmu.sv    | 293 ++++++--------------------

**All of 7403deb and 1590bb6, undone.**

The mechanism, and it is entirely mine: `@pre-rearch-io` needs the OLD RTL in
the working tree, so it was put there with

    git checkout 8e7f974 -- fpga/rtl/texture/zhao_texture_{bilerp,tmu}.sv

`git checkout <rev> -- <paths>` **stages**; it does not merely update the
working tree. Twenty minutes later `git add -- design/blocks.yml && git commit`
committed the whole index, revert included. Then `git checkout HEAD -- <rtl>`,
intended to restore the rearchitecture, faithfully restored the revert.

**What it did NOT invalidate, checked rather than hoped:**

- the mutation sweep — its worktree was checked out at `1590bb6` and scored
  those bytes;
- the fits — `@pre-rearch-io` was *supposed* to see the old RTL, and its row
  correctly carries `rtlCleanAtHead: false`;
- the differential lanes — all four were run and green before the checkout.

Restored from `1590bb6` and verified with `git diff 1590bb6 -- fpga/rtl`, which
is empty. Committed with `-o <paths>` so the diffstat could not exceed the
message.

**Caught by grepping the shipped file for a symbol that had to be in it**, while
checking something unrelated. Nothing in the build, the tests or the tools would
have noticed: everything still compiled, because the reverted RTL is *also*
correct RTL — it is just the wrong one.

**Second git default in one run to do something reasonable and unrequested**;
`git add -A` staging 288 files of line-ending churn was the first. Both were
caught by looking at what was actually staged rather than at what was intended.
The rules now written into the restore commit:

  · `git checkout <rev> -- <paths>` is a STAGING operation. Follow it with
    `git reset -- <paths>` when the intent was the working tree only.
  · Commit with `-o <paths>`, or read `git diff --cached --stat`, so the
    diffstat has to match the message before anything lands.

### 2026-08-23 20:3x — I edited RTL under a running fit chain. Third instance, and it is the brief's named one.

While a four-fit chain was running I added the measured DSP column to
`zhao_texture_tmu.sv`'s `FILT_LANES` table. **That is precisely the run brief's
named failure mode** — *a fit running while something rewrites `.sv` files
describes neither version* — and the SURFACE.STAMP run recorded doing the same
thing about forty minutes after writing the rule into its own SPEC. I did it
about four hours after reading theirs.

What actually happened, checked rather than assumed:

- the edit is **comment-only**, so no netlist can differ;
- fit 1 of the chain was already past `quartus_map` (its placement preparation
  had finished 1m52s in), so its netlist was read before the edit existed;
- fits 2–4 had **not started**, so none of them read the edited file;
- **but they would have**, and every one of them would then have carried
  `rtlCleanAtHead: false` against an uncommitted working tree — four rows whose
  provenance says "this describes something not in git".

Reverted within about a minute with `git checkout HEAD -- <file>`, the copy kept
in the scratchpad to be re-applied after the chain finishes. `git -c
core.autocrlf=true status --porcelain -- fpga/rtl` is empty again, and the index
was checked too, because the *last* thing that bit this run was a staged file
nobody looked at.

**The tell was not the edit; it was that I had stopped counting what was
running.** The rule for the rest of this run: before touching anything under
`fpga/rtl`, `Get-Process quartus_*` first — the same check that is already
mandatory before *starting* a fit, applied to the other end.

### 2026-08-23 20:4x — samples/frame has TWO honest values now, and quoting one would be the wrong-number failure again

SURFACE.STAMP published `items/frame = (Fmax / 60) / (cycles per item)`. Earlier
in this run I generalised that to `min(Fmax, 100 MHz) / (60 × II)`, because that
block's Fmax was 87.54 — below the shared `gpu_clk` — while this block's looked
like 199.72 and the constraint was clearly the binding term.

**With the corrected SDC that reasoning inverts, and the two formulas now
disagree by 2.7×.** At `@pre-rearch-io`'s measured **36.92 MHz**:

| | at the 100 MHz `gpu_clk` the console is designed around | at this block's own measured Fmax |
| --- | ---: | ---: |
| direct colour, II 4 | 416,667 | 153,833 |
| **CLUT, II 6** | **277,778** | **102,556** |
| vs. the 850,000 demand | 0.33× | **0.12×** |

**Both are true and neither alone is honest.** The left column is the rate the
block would deliver *if it closed timing*, and it does not — 36.92 MHz is 37% of
its constraint, so composing it as-is would not slow the console down to 0.33×,
it would fail timing. The right column is what the console would deliver if
`gpu_clk` were dropped to what this block can actually take, which is not a
decision anyone has made or should.

Reported as a pair, with the timing failure named, rather than collapsed into
one figure. The directed test prints the left column because that is the one an
RTL change can move; the timing half is the fit's to report and is now in the
contract's Synthesis section and in `reports/REMAINING_BLOCKERS.md`.

### 2026-08-23 21:0x — MEASURED, like for like: **28 → 6 DSPs, 36.92 → 36.11 MHz**

The census row: shipping default `FILT_LANES = 2`, corrected SDC, sourceCommit
`1c98bb8`, `rtlCleanAtHead: true`, 675.9 s. Constraint and worst path saved as
`fit-evidence/default_lanes2_io_constraint.txt`.

| | `@pre-rearch-io` | shipping default | delta |
| --- | ---: | ---: | ---: |
| ALMs | 1,844 | **1,921** | +77 (+4.2%) |
| registers | 342 | **350** | +8 |
| **DSP blocks** | **28** | **6** | **−22 (−79%)** |
| **Fmax** | **36.92 MHz** | **36.11 MHz** | −2.2% |

**And the worst path is the same cone before and after**, which is the finding
that makes the −2.2% legible rather than mysterious:

    before : q_fv_r[1] -> smp_a_o[1]   20.913 ns
    after  : q_fmt_r[0] -> smp_a_o[4]  21.432 ns

Both are a registered mode-or-fraction bit, through `decode16` and the filter,
to the sample output. The factored form is **0.5 ns slower on it**, and the
shape says why: the old filter was four *parallel* multiplies into one adder
tree; the new one is serial — mult → add → sub → mult → add — because the V lerp
cannot start until the U lerps finish. Fewer, narrower multipliers; a longer
chain. **It costs nothing that was not already lost**, because the block was
already at 37% of its clock.

**A correction to what I wrote an hour ago.** I recorded the 37.004 ns
`req_mode_i[8] → q_addr_r[55]` result as "the honest number is the address
generator". It is not a critical path — it was measured by applying the I/O
constraints *post hoc* to a database that had been **placed with no I/O
objective at all**, so the fitter had never once optimised those paths. With the
objective actually present during the fit, the address generator comes down and
the filter-to-output cone leads at ~21 ns. **37 ns is an upper bound on an
unoptimised placement.** What it is still worth is *what it named*: this
contract's unmeasured claim that the address generator is "the longest path in
the file" now has evidence in both directions, and it is a near miss rather than
the limiter.

Recorded as a correction rather than by editing the earlier entry, because the
distinction — a post-hoc timing query on an unoptimised placement is not a
fitted critical path — is the reusable part.
