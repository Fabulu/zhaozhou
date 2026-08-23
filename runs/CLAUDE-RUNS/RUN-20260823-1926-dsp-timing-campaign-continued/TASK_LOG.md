# Task Log: RUN-20260823-1926 - The DSP and timing campaign, continued

**Created:** 2026-08-23 19:26 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260823-1926-dsp-timing-campaign-continued/

---

## Objective

Census **160 -> 85-90**, and close timing on the blocks that miss it. Continues
RUN-20260823-0934, which is archived; its SPEC (census reconciliation and
GEOM.SKIN) was complete and the work had outgrown it.

---

## Progress Timeline

### 2026-08-23 19:26 UTC+02:00 - Run opened

Entry state:

| | |
| --- | ---: |
| census | **160** DSPs against 112 |
| ceiling | 85-90 |
| blocks with a real Fmax | **4 of 47** |
| `ctest -L fast` | 269/270, sole failure the V16 `FIELD.SEQ.CORE` gate |

Measured Fmax so far — **three of four are comfortably fast, only Field is
genuinely slow**:

| block | Fmax |
| --- | ---: |
| `zhao_texture_tmu` (pre-rearchitecture) | **199.72 MHz** |
| `zhao_geom_skin` | 89.65 MHz |
| `zhao_surface_stamp` | 87.54 MHz |
| `zhao_field_seq` | **33.86 MHz** |

In flight: `zhao_texture_tmu`, 28 -> 6-9 DSPs against a demand of 850,000
samples/frame derived from Sacrifice. Its `@pre-rearch` baseline is already
recorded, which is the SURFACE.STAMP lesson applied on the first move.

### 21:30 - TEXTURE.TMU complete: 28 -> 6 DSPs, census 134

Both fits under the corrected SDC, so like-for-like:

| | before | after |
| --- | ---: | ---: |
| DSPs | **28** | **6** (-79%) |
| ALMs | 1,844 | 1,921 (+4.2%) |
| Fmax | 36.92 MHz | 36.11 MHz |
| worst path | `q_fv_r[1]->smp_a_o[1]` 20.913 ns | `q_fmt_r[0]->smp_a_o[4]` 21.432 ns |

Frontier **28 / 12 / 6 / 3** at pre-rearch and `FILT_LANES` 4/2/1, with **Fmax
flat at 35.62-36.92 MHz** — none of these settings touches the limiting cone, so
**the DSP axis is nearly free in time on this block.** `zhao_texture_bilerp`
alone 7 -> 3.

Method: factor the contract's four-weight law —
`A=(t00<<8)+(t10-t00)*fu`, `B` likewise, `S=(A<<8)+(B-A)*fv`, one rescale. Same
integer, three products per channel instead of eight, two channels multiplexed.
**Not** the staged-rounding form `qformats §3` refuses; nothing intermediate is
rounded.

Sweep 30/30/30, caught 29. Run 1 (28/30) kept as the evidence for the harness
change: **M27 was a real hole** — the modelled cache ignored `cac_en_o` and was
strictly more generous than the real block. M18 is a true equivalent argued
against a named line. `ctest -L fast` **271/272**, the one failure the
pre-existing V16 baseline.

### Three findings from this block that outlive it

**1. THE SDC DEFECT HAD A SECOND HALF, and I published the bad number.**
The harness declared no `set_input_delay`/`set_output_delay`, and TimeQuest
**silently excludes every pin-to-register and register-to-pin path when none is
declared.** Same RTL, same tool, same device: **199.72 MHz clock-only vs 36.92
MHz with I/O** — a factor of **5.4**. This block's arithmetic runs pin-to-pin, so
199.72 was **the sample counter's speed** and the arithmetic had never been timed
at all. Harness fixed (`QUARTUS_GOTCHAS` §9). **Every Fmax measured before the
fix is suspect**; DSP counts are unaffected, since synthesis never reads the SDC.

**2. A post-hoc timing query on an unoptimised placement is not a critical
path.** I then published *37.004 ns, the address generator* as the honest worst
path. It was measured by applying I/O constraints to a database **placed with no
I/O objective at all**, so the fitter had never once optimised those paths. It is
an upper bound. Fitted properly the filter/output cone leads at 21.4 ns and the
address generator comes second. **Left uncorrected, the next implementer would
have pipelined the second-worst cone.**

**3. A GREEN HARNESS IS NOT A GREEN THEOREM.** The formal proof **does not close
on the factored form** — cover passes in ~1 s, bmc ran 3,300 s with no answer
against 741 s for the arithmetic it replaced. The opposite had been committed in
three places, all derived from "the harness needed no edit", which is true and is
a different claim. The run log had even noticed the proof was *slow* and read it
as more work rather than as the solver failing.

The cause is the same fact that makes the theorem worth having: the checker
states the law as `t00*w00 + ... + t11*w11`, and until now the DUT computed
**that same expression**, so the assertion was nearly a syntactic identity. It is
now a real distributive-law identity across three multiplies of three widths, one
feeding another.

**What replaced it is stronger and total, not sampled:** (i) no lane truncates —
every intermediate is monotone in each texel, so extremes over the byte domain
fall at texel **corners**, and 16 corners x 65,536 `(fu,fv)` bounds the whole
domain exactly, 0 violations; (ii) given no truncation the pre-rounding sum is
**exactly linear** in the four texels, so four basis vectors per `(fu,fv)`
determine the entire map, and the recovered coefficients equal `w00/w10/w01/w11`
exactly — settling it for **every integer texel quadruple**, not merely every
byte one. Script kept and runnable.

### Deliberately left open, named rather than closed quietly

1. the formal lane **left failing** so the regression stays visible — banked in
   `formal_runs.yml`; fixing the proof versus removing the lane is the owner's
   call;
2. the block **does not close 100 MHz** (36.11) — pre-existing, unrelated to the
   DSPs, fix specified;
3. it delivers **0.33x its derived demand** — the II=2 design is written into the
   contract, **not built**.

The agent also disclosed five process failures, including **three of five
predictions wrong** (Fmax direction, ALM direction, DSP packing) — all, by its
own diagnosis, from reasoning about what was *removed* rather than what
*replaced* it. And it re-ran its own tamper check after HEAD moved under it
twice, which is how it had earlier caught a silent revert of its own
rearchitecture (`git checkout <rev> -- <paths>` **stages**).

### 21:45 - RULING: the next run is a repo-wide audit, not another rescue

Docketed (`1390b7d`). Premise verified before acceptance:

* census covers **41 of 94** RTL files;
* **`zhao_geom_project` is not in the report at all**, despite duplicating
  `terrain_project`'s 33-DSP projection law;
* **`FORGE.CLIFF` confirmed** — three `assign x = mem_r[idx]` async reads over
  ~120 kbit written from an async-reset process, which is why its fit timed out.
  Diagnosable from source without spending another fit.

**And the fourth check refuted my own method:** counting nonconstant multiplies
in `geom_project` with grep gave 0, then gave line-counts. Both useless. That is
the argument for an **elaborated-AST scanner** rather than pattern matching, and
it is recorded as such rather than quietly dropped.

Honest total is **~180-185 DSP, not 134** — hidden projector plus pose
arithmetic. **Treat this as a 180-DSP design that must reach 85-90.** Worse than
it looked, and still credible: the two projectors alone are ~50 DSPs of pure
duplication.

`design/budgets/dsp.md` corrected too: "DSP blocks = the number of `*` operators,
**whatever the operand widths**" is contradicted by our own §5 evidence — the
same `zhao_geom_lod` source cost **28 DSPs at 72-bit operands and 18 at 64-bit**.
Operator count is a **lower bound**, exact only while operands stay inside one
block's native width.

### 22:26 - Budget audit wave 1 launched; the method changes here

With the machine idle and no implementation agent running, started the audit
rather than the next rescue. Its map-only pass is long unattended Quartus time,
which is exactly what should occupy an idle machine overnight.

**Scope is deliberately narrow: this run does NOT optimise anything.** The
deliverable is evidence and ranked work — an elaborated-AST scanner, Quartus
calibration microbenches, and a map-only pass to close the 41-of-94 gap.

**The acceptance test is falsifiable on purpose:** run the heatmap against
`zhao_field_seq` (zero M10Ks while spending 8,901 ALMs on a register file and
three ROMs built from logic) and `zhao_texture_tmu` (II=6 against a demand
needing II=1) and **confirm both light up red without anyone telling it the
answer.** If the tool cannot rediscover what we already know, it will not find
what we do not.

### 22:40 - STATUS written for the day  (`770618b`)

327 -> 134 measured; the honest ~180 stated rather than the flattering 134; the
seven recurring mistakes; and both corrections I owe — the 199.72 MHz figure
that was timing a counter, and the proof reported as passing that had not run.

### Closing position

| | |
| --- | ---: |
| census, measured | **134** DSPs / 112 |
| census, honest source-level estimate | **~180-185** |
| ceiling | 85-90 |
| blocks rebuilt today | 4 — Field 79->3, skin 72->9, stamp 28->0, TMU 28->6 |
| blocks meeting their frame budget | 1 — `zhao_geom_skin`, 124,514 vs 120,000 |
| modules with any fit | **41 of 94** |
| `ctest -L fast` | 271/272, sole failure the V16 baseline |

**Two blocks have specified, unbuilt fixes**: the Field engine must stop
building memories out of logic (6 M10Ks from 502 idle), and the TMU must accept
more than one request at a time (the cache's four lanes make one CLUT sample per
clock available, 1.67 M/frame against a 0.33x delivery today).

**The lesson the day kept teaching, now at twelve instances:** an artifact can be
real and still be an artifact of something other than what it was read as — an
SDC that constrained no clock, then one that excluded every I/O path; a sweep
that re-ran the previous binary; a preflight that scored an empty set; a proof
whose sources predated the edit; a stopped sweep still rewriting RTL; a contract
whose "met" meant cycles; a timing query on an unoptimised placement; and a
green harness read as a green theorem.

**A green result from a tool nobody has watched run is not evidence — and
neither is a number from a tool that was asked the wrong question.**

---

## Subagent Spawns

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| 17:36 | `acc49f0` | TEXTURE.TMU 28 -> 6-9 DSPs | **COMPLETE** — 6 DSPs, census 134, archived | `runs/CLAUDE-RUNS/RUN-20260823-1736-texture-tmu-dsp-rearchitecture/` |
| 22:26 | `af363d9` | Budget audit wave 1 — scanner, calibration, map-only pass | Running | own run dir |

---

## Files Created

- this run directory

---

## Decisions Made

**Closed the previous run rather than letting it accrete a whole day.** Its
SPEC was census reconciliation and GEOM.SKIN; both were complete while the work
had moved on to three other blocks and an architectural ruling. A run whose log
no longer matches its SPEC stops being a record and becomes a diary.

---

## Next Steps

- [ ] `zhao_texture_tmu` result
- [ ] `zhao_terrain_normals` 18 -> 1-2 (derived demand 2,000 normals/frame)
- [ ] `zhao_terrain_project` 33, cache-then-sequence
- [ ] the Field rearchitecture, wave 0 first: preserve the 33.86 MHz netlist and
      group the top 200 setup paths by family
- [ ] derive per-frame demand for the five Field profiles
- [ ] re-measure blocks whose rows predate the SDC fix
- [ ] archive this run when the campaign closes
