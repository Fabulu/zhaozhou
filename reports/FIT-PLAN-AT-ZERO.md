# The fit plan, written BEFORE the fit — Zhaozhou console, 2026-09-20

**Status: not yet runnable.** The register reads 21. This file exists now
because `CLAUDE.md` requires the fit gates to be named in advance with the
question each one answers, and because a plan written after the results are in
is not a plan, it is a rationalisation.

It lives in `reports/` and not in a run folder deliberately: every pass creates
a new run folder, so anything durable left in the current one is orphaned by the
next.

---

## 0. The standing goal this serves

Drive the mandatory gap count to **zero**, freeze that design, then run the
honest fit against **5CSEBA6U23I7**. Fit at completion only.

## 1. What must be true before ANY fit starts

These are not nice-to-haves. Each one has already caused a measurement to be
taken of a machine nobody meant to measure.

| # | Precondition | Why, and what it cost when skipped |
|---|---|---|
| 1 | **Register reads 0** | The goal. A fit of a design with tie-offs measures a circuit that cannot run. |
| 2 | **`rtlCleanAtHead` true — the tree is CLEAN at the fitted commit** | A row fitted from a dirty tree has a digest that describes nothing. One such row is stamped `ok` in the receipts while a row with a clean tree, a real digest and three honestly declared breaches is stamped `failed:structure`. **Read `rtlCleanAtHead` before `status`, always.** |
| 3 | **Superseded-in-a-production-root reads 0** | R86 made this fatal across all 72 roots so the console cannot be fitted while composing a superseded module. **UNMET AGAIN as of 2026-09-20 late evening: the check reads 2.** It was cleared by ATTRDIV (R104/R122) and then reopened -- correctly -- when H1 landed `zhao_field_host_v2` without composing it, because composition is packet C1's serialised act. `zhao_console_core` references the v1 host ten times and v2 zero times, so the console really does compose a superseded module and R86 exists to stop exactly that being fitted. **This is the gate working, not a regression, and it is NOT to be silenced** -- see R143, including the fact that `check_console_inventory.py` honours a `version_exceptions` entry here while `completion_register.py` does not read that section at all, so the two gates currently disagree. **C1 composing v2 clears it.** The ATTRDIV lane cleared the last pair by adopting `zhao_raster_attrdiv_v2` at both sites (R104/R122). **Six of the seven preconditions in this table are now met; only #1, the register at 21, is outstanding.** |
| 4 | **`dsp_census.py` clean, and its `unpriced_requirements:` rows checked against the filesystem** | Three rows once named modules that exist. A census that prices a module list nobody verified is an inventory, not a measurement. |
| 5 | **`zhao_prod_top` regenerated and `--check` fresh** | It is generated and instantiates every production block by name. A new port nobody connects is a `PINMISSING` that only the next fit discovers. It was found stale for two separate port changes made days apart. |
| 6 | **Worst paths split by origin** | `reports/synthesis/worst_path_index.json` keeps the prior gating path per module. A recorded endpoint the present design no longer contains can only describe the earlier one — that is structural evidence where the provenance record is absent, and it is how a palette baseline row was dated without a `.sources.sha256`. |
| 7 | **Every block has been through `quartus_map` at least once** | **Verilator lint-clean is not Quartus-synthesizable.** Two SystemVerilog forms passed `verilator --lint-only` with 0 diagnostics and failed `quartus_map` with a syntax error: a bare module-scope elaboration check, and an implicit generate. `check_quartus17_syntax.py` now catches both, but a block that has never been mapped has not been shown to synthesize, however clean its lint. |

## 2. The fits, and the question each one answers

`CLAUDE.md`: *a fit nobody could state a question for is a fit that should not
run.* An island fit is 1.5–4 hours; two of them once consumed most of a session
while the actual engineering took minutes. The fit is the scarce resource.

### ~~F-CLIFF1~~ — **ALREADY RUN, 18 September. Superseded by F-CLIFF-GOLDEN below.**

**This entry said "it has never run" and that was FALSE** (ruling R117). The
receipts have been in `reports/synthesis/blockpaths/` for two days —
`zhao_forge_cliff_ram@first-measurement.{map,fit,sta,setup,hold}.rpt` plus a
`.sources.sha256` that **matches the current source**, so the numbers describe
the file in the tree today. And it was not a map: it was a **full fit on the
target part `5CSEBA6U23I7`**.

It answered its own question. Five inferred memories (`win_mem`, `edge_key_r`,
`edge_span_r`, `prio_mem_r`, `run_mem_r`), MLAB bits 0, 120,964 block memory
bits, 15 RAM blocks — the window went to block RAM, which was the pass
condition. **Fitted at 976 ALM, 2% of the device.** At map stage, like for like,
1,326 combinational ALUTs against the golden's 8,149 and 826 registers against
3,875.

Two declared breaches, neither of which gates the decision: **four**
`Warning (276020)` pass-through insertions where the gate demanded
`ramConversionWarnings 0`, and a cosmetic bit-0 inferred latch on
`triangles_submitted_o` (the counter increments by 2, so bit 0 is provably
constant — an artifact, not a missing branch).

**How this entry came to be wrong is the lesson.** I inherited "the gate has
never run" from a lane report, wrote it into a RULING, and then copied the
ruling into this plan. A ruling number made an unverified claim read as more
authoritative rather than less. It was found only because I went to run the gate
and looked for the runner.

### F-CLIFF-GOLDEN — **RUN 2026-09-20 18:20. RESULT: 6,674 ALM fitted.**

**ANSWERED.** `FIT_RC=0`, device `5CSEBA6U23I7`, `.sources.sha256` matching the
current source. Landed in the pre-committed band "roughly 6,000-8,000 ALM", so:
**the swap saves 5,698 ALM fit-minus-fit** (6,674 -> 976) **and 3,086 registers**,
for one extra RAM block and ~1.2k memory bits, DSP unchanged. **13.6% of the
device.** The old map-only row reproduced exactly (8,149 ALUT / 3,875 reg /
119,808 bits). Full reading in ruling R142.

#### THE ADOPTION IS EXECUTED — 2026-09-20 evening, packet `gz/forge4`

**R142 ruled "adopt" and for most of a day nothing in any ledger moved.** That
is CLAUDE.md's uncashed cheque in its textbook shape: the plan was right, the
prerequisite was built, the measurement was taken on the target part, and the
last step was simply never performed. `prod_manifest.yml`'s own row had named
its discharge condition -- *"adopt in place of zhao_forge_cliff only after that
gate"* -- and the gate had by then run **twice**.

`zhao_forge_cliff_ram` is now FORGE.CLIFF's adopted implementation in all four
ledgers that have to move together (`prod_manifest.yml` `top:`,
`console_inventory.yml` `superseded`/`superseded_by`, `blocks.yml`
`implementation:`, `fit_targets.yml`'s `zhao_prod_top` source list), and
`zhao_prod_top.sv` is regenerated with its extra `walk_fault_o[7:0]` connected
and folded -- **read in the generated output**, per R151/R161.

**The `implementation:` line is load-bearing and not decoration.**
`completion_register.py:successor_in()` requires the version tail to `fullmatch`
`v\d+`, deliberately. `_ram` is a RIVAL, not a version, so the choice is
**structurally invisible** to the register and FORGE.CLIFF resolved by naming
convention to the 6,674-ALM module. A decision the primary instrument cannot see
is the R159 shape pointed the other way.

**And the equivalence half was re-run rather than inherited**, because R142 says
in as many words that a fitted number settles AREA and nothing about whether the
two implementations agree: `forge_cliff_ram_differential` built and run at the
adoption commit -- oracle / golden / candidate in one process, **246 lattices,
752 pages, 0 mismatches**, every coverage counter nonzero (merged 128, dropped
122, both 103, merge-only 25, dropped span>1 92, span13 3, partial 182, cw1 2,
ch1 2, load-stalled 37, output-stalled 70, vdist 66), and it reproduces the
2026-09-10 report's cycle figures to the digit (5,540,759 vs 5,383,571, −2.84%;
worst page 85,740 → 84,208; candidate slower on 99 of 246 lattices, by the
recorded +4 clocks per page).

**ADOPTION IS NOT COMPOSITION and the register did not move: 21 → 21.**
FORGE.CLIFF still has no producer for any of its three inputs -- no
lattice-walking page issuer, no 34x34 solid-bit window (`solid\w*_o` as an
output port across all of `fpga/rtl` including `synth/` and the probes: still
zero hits) and no vdist read master (`vdist` in exactly four files, all the
wrong end). The decision settles WHICH module gets composed when those exist.

One accounting note so the move is not misread as a regression: the register's
`superseded check` goes **73 → 72 production roots** with **70 → 71 further
fit-target tops**, total unchanged at 143 and both lists still CLEAN. The golden
left `prod_manifest.yml`'s `top:` and kept its own standalone fit target, so one
module changed category. Nothing lost coverage.

**R117's two blockers are NON-DIFFERENTIAL:** the golden has the same four
`Warning (276020)` and the same one inferred latch. Neither is introduced by the
candidate, so neither is an adoption blocker -- they are costs of the design, not
of the swap.

#### BOTH CONDITIONS ARE DISCHARGED -- 2026-09-23, packet `gz/cliffadopt`

**And no fit was spent doing it.** The lane arrived authorised to run a leaf fit
of the golden, on a brief stating that only one half of the fit-minus-fit
comparison existed today. **It already existed.**
`zhao_forge_cliff@golden-for-F-CLIFF1` landed 2026-09-20 18:20 at **6,674 ALM**,
its `.sources.sha256` (`445a89ba...`) matches `fpga/rtl/forge/zhao_forge_cliff.sv`
in the tree today, and R142 is the reading of it. Re-running it would have
measured a circuit already measured. **The authorised fit was refused on the
ground that it was redundant** -- another false-absence claim attached to this
very gate, which is now the third time this pair of blocks has produced one.

What was spent instead: the committed leaf map row at **32.6 s**, plus **two
hand-run leaf `quartus_map` runs of about 33 s each** for the controls. R109
itself draws the distinction ("It is a `quartus_map`, not a fit"), so this is
strictly less than the authorisation rather than more.

**Condition 2 -- the bit-0 latch -- REMOVED AND MEASURED.**
`triangles_submitted_o` is now `{tri_pairs_r, 1'b0}` over a 31-bit pair counter,
which is R142's own named remedy taken verbatim.
`zhao_forge_cliff_ram@cliffadopt-latchfix` reports `Info (10041)` **zero times**.
The zero is not quoted on its own: the report carries 584 `Info` lines, so the
category is live, and
`tests/mutants/zhao_forge_cliff_ram_bit0_latch_control.sv` -- the pre-fix counter,
renamed, polarity inverted so it passes when the latch fires -- reports the latch
**once**. Both runs carry the same four `Warning (276020)` and identical
1,326 ALUT / 826 register / 120,964 bit / 2 DSP numbers, so the fix moved the
latch and moved nothing else.

**Condition 1 -- the four `Warning (276020)` -- ACCEPTED IN WRITING, WITH THEIR
COST**, in `zhao_forge_cliff_ram.sv`'s own header, because a run folder is
orphaned by the next pass. They are `edge_key_r`, `edge_span_r`, `prio_mem_r` and
`run_mem_r` -- predicted from the source (the four tables written in an
`always_ff` and read by a bare `assign`) *before* the report was read, and the
report names those four and only those four. `win_mem`, the one with a clocked
read, is absent. The cost is **no additional area** (the fitted 976 ALM was
measured on RTL that already contains all four), **no differential cost** (R142
measured exactly four on the golden too), and **a real timing cost** -- the fit's
worst internal path launches from `edge_key_r_rtl_0|...~PORT_B_WRITE_ENABLE_REG`,
one of these four.

**The cheap removal was tried and measured not to exist.**
`tests/mutants/zhao_forge_cliff_ram_norwcheck_probe.sv` puts
`(* ramstyle = "no_rw_check" *)` on all four declarations and nothing else, and
changes **not one number**: same four warnings, same 1,326 ALUT, same 826
registers, same 889-ALM estimate, and `ramstyle` never mentioned in the report.
The reason is the part worth keeping -- **this pass-through is not a
read-during-write POLICY.** A Cyclone V M10K cannot read combinationally, so a
bare `assign mem[idx]` read must be built as a registered-read RAM plus logic
that reproduces what the source says. `no_rw_check` waives a *choice* about
collisions; it cannot waive the *semantics* of the source. Removal therefore
means making the four reads synchronous -- the follow-up this module's header
already defers, a cycle-level change that moves the differential test's recorded
figures, and not a blocker (R142).

**What is still NOT done, and is refused with evidence: COMPOSITION.** Re-measured
at this base rather than inherited. `solid` as an output-port suffix across every
subdirectory of `fpga/rtl`: **zero hits**. `vdist`: four files, and in
`zhao_console_core.sv` **every occurrence is a comment** -- no port, no wire. The
only live references sit in `zhao_prod_top.sv`, the generated **pricing** top
whose own header says "Blocks are not wired to each other", feeding the block
from `assign u09_src = {16{u09_lfsr_q}}`. That is an LFSR, not a producer. All
three inputs -- page command, 34x34 solid window, vdist read master -- still have
none, so wiring FORGE.CLIFF today would tie off three inputs and change no pixel.
`completion_register.py` stays at **13** and `zhao_forge_cliff_ram` stays under
BUILT BUT NOT CONNECTED, honestly.

#### AND ONE THING THE RECEIPTS CARRY THAT NOBODY HAS QUOTED: HOLD

Both labelled rows in `reports/synthesis/zhao_block_fit.json` carry full timing
fields, and every reading of this rivalry so far has used the area fields only.
R142 said in as many words that the fit "measures AREA", which is true of what it
was asked -- but the numbers were sitting there:

| | golden `@golden-for-F-CLIFF1` | candidate `@first-measurement` |
|---|---|---|
| fitted ALM | 6,674 | **976** |
| Fmax (`clk`) | 39.59 MHz | 39.72 MHz |
| setup slack | -15.259 ns | -15.174 ns |
| setup TNS | -10,377 ns | **-2,070 ns** |
| **hold slack** | **+0.263 ns** | **-4.140 ns** |
| hold TNS | 0 | -4.605 ns |
| fit seconds | 1,047.6 | 466.2 |

Like for like -- same part, same tool, same pinned seed, same virtual-pin I/O
mode, both `rtlCleanAtHead: true` -- so the comparison is legitimate in the way
R142 established for the area numbers.

**The candidate is better on setup and WORSE ON HOLD.** Setup TNS improves by 5x
alongside the area, which is the expected direction. Hold goes from met to
**-4.140 ns violated**, and that is a real difference between the two
implementations that no ruling mentions.

**Stated with its limits, because a leaf fit is not the console.** These are
unconstrained-ish leaf placements with virtual pins; both sides sit ~15 ns behind
setup, so neither closes at this stage and the absolute numbers are not board
truth. What survives those caveats is the *sign change*: on identical treatment
the golden met hold and the candidate does not. Hold violations do not fix
themselves in a larger placement.

**This does not reopen the adoption.** 5,698 ALM is 13.6% of the device, the
binding constraint is ALMs, and a hold violation on a leaf fit is a routing and
retiming problem rather than a functional one. It is recorded here so the number
everyone quotes travels with the one nobody had read, and so whoever eventually
composes FORGE.CLIFF knows to watch hold on this block specifically. The likely
first suspect is the same four pass-throughs -- a write enable launching a data
path is exactly where short paths come from.



*(Original entry, kept because its reasoning is why the fit was worth spending:)*

### ~~F-CLIFF-GOLDEN~~ -- the question, as posed before the run

**Question: what does `zhao_forge_cliff` actually FIT at on `5CSEBA6U23I7`?**

`zhao_forge_cliff` is **7,664 ALM and 18.3% of the whole ALM budget**
(`BUDGET_HEATMAP.md:159`) — but that figure is a **map-only ESTIMATE that was
never fitted**, and `FORGE-CLIFF-BITMAP-RAM-20260910.md:165` says so in as many
words.

So the number everyone will want — "the RAM version saves N ALM" — **cannot be
computed today**, because it would set a fitted 976 against an unfitted 7,664.
That is estimate-versus-fit, the same mismatched-comparison error as measuring a
grounded stance against an aerial drawing, and it would land confidently in the
flattering direction.

One cheap leaf fit of the golden closes it. Until then the defensible statement
is the map-stage one: **the candidate uses 16% of the golden's combinational
ALUTs and 21% of its registers**, and separately fits at 976 ALM on the target.

Given ALMs are the binding constraint and the console sits at roughly 113% of
the device, this is plausibly the largest single lever in the tree — which is
exactly why its headline number must not be manufactured.

#### F-CLIFF-GOLDEN: what each outcome MEANS, written BEFORE the number lands

Launched 2026-09-20 evening, `-RowLabel @golden-for-F-CLIFF1`, same device as the
candidate (`5CSEBA6U23I7`), same tool, same stage. **These readings are fixed in
advance so the interpretation cannot be chosen after the fact** -- which is the
whole reason CLAUDE.md requires a fit's question to be named before it runs.

The candidate `zhao_forge_cliff_ram` is **fitted at 976 ALM**. The golden's only
existing figure is **7,664 ALM, map-only, never fitted**
(`FORGE-CLIFF-BITMAP-RAM-20260910.md:165` says so in as many words), and
`BUDGET_HEATMAP.md:159` puts that at **18.3% of the whole ALM budget**.

| golden fits at | what it means | what I do |
|---|---|---|
| **roughly 6,000-8,000 ALM** | the map estimate was sound; the swap is worth ~15-17% of the device | adopt, subject to R117's two named items; quote the delta as fit-minus-fit |
| **materially below ~4,000** | **the map estimate was badly wrong and the case shrinks** | say so plainly, re-price the swap, and do NOT quote the old 7,664 anywhere again |
| **above ~8,000** | the estimate was conservative and the lever is larger | adopt, but re-check that the two designs are functionally equivalent before believing a delta that large |
| **fails to place** | the golden does not fit the target ALONE, which is itself a finding | that makes adoption near-mandatory rather than optional, and it goes to the owner |

**In every branch the delta is quoted fit-minus-fit or not at all.** Setting the
candidate's 976 against an unfitted 7,664 is the mismatched comparison this
repository keeps landing in the flattering direction -- the same error as R104's
cost model, R112's budget-versus-residual, R98's truncating container and
ZIDL's `forge_kind`-versus-`j_family_i`.

**And one thing this fit CANNOT settle**, stated now so it is not claimed later:
it measures AREA. It says nothing about whether the two implementations agree
functionally. That is `tests/forge/forge_cliff_ram_differential.cpp`'s job and it
is Verilator's, not Quartus's.

### F-CONSOLE-TARGET — the VERDICT fit, on `5CSEBA6U23I7`

**Question: does the frozen console place and route inside 41,910 ALM / 112 DSP
/ 553 M10K, and what is `gpu_clk`?**

This is the pass/fail line and the only fit whose answer is the project's
answer. It is expected to **REFUSE TO PLACE**, and that is why the next one
exists.

### F-CONSOLE-SIZE — the MAP fit, on `5CEBA9F31C7`

**Question: if it does not fit, by how much, and WHERE?**

R80 requires both runs because **a refusal is not a map.** A device that cannot
place the design reports that it cannot place the design; it does not report
which subsystem is over, which path is critical, or what to attack. The larger
part is fitted *only to measure size*.

The one composed number that exists today is
`zhao_console_core@console-core-first-light`: **47,582 ALM / 151 DSP / 306
M10K**, `gpu_clk` 18.5 MHz, setup slack −44.06 ns, TNS −39,647 ns, and
`treeCleanAtHead: false`. Against the target that is roughly **113% ALM and 135%
DSP** — and it was measured on a tree carrying a live metadata-swap defect, from
a dirty checkout, before this run's twenty-odd repairs. **It is a starting
estimate and nothing more. Do not quote it as the console's size.**

## 3. What the fits are NOT for

Every one of these is a Verilator question and answers in seconds. Sending them
to Quartus is how days go missing:

correctness · throughput in clocks · handshake behaviour · field routing ·
atomicity under backpressure · parameter sensitivity · whether a counter fires ·
whether a guard is reachable.

The RCP V3 swap sat behind a fit for days; the question that actually killed it
— *does the tile meet its throughput criterion at the island's NCTX?* — was one
verilate flag and under a minute.

## 4. While a fit runs

**A running fit is not a reason to idle**, and this is enforced by a Stop hook
(`tools/hooks/fit-running-stop-guard.ps1`), not by trust — the rule was written
into `CLAUDE.md` and then violated twice in the same session, because advisory
prose loses to the pull of reporting a status.

A per-block fit compiles only its own closure and `design/fit_targets.yml` says
exactly which files, so **everything outside that list is free**. Check the
closure, then pick up the next thing.

The one hard constraint is the **live-tree trap**: the fit reads the working
tree, so never edit a file inside the running fit's closure
(`QUARTUS_GOTCHAS.md` §11).

**And when the fit comes back: write down where you were BEFORE reading it.**
Fit results redirect the work — that is what they are for — and the half-finished
thing being held in someone's head is exactly what gets lost. A line in
`TASK_LOG.md` costs seconds; reconstructing it costs the session.

## 5. Disk, before starting anything long

The machine reached **zero bytes free, 952 GB of 952 GB** on 2026-09-06. Quartus
died mid-placement 55 minutes into a fit and a Verilator build died with "No
space left on device". About 33 GB was ours: ~129,000 `.rgb` raw frame buffers.

`tools/maintenance/purge_render_intermediates.py` is the tool. Its root defaults
to the **zencrifice root, not the repo** — 15 of the 33 GB sat in a sibling
creature directory outside `zhaozhou` entirely. Dry run by default; it spares
anything touched in 48 h and never touches `.webm`, `.png`, `.md` or git packs.

Check free space before a fit, not after one dies.

## 6. Reading the results honestly

1. **`rtlCleanAtHead` before `status`.** See precondition 2.
2. **`ruleViolations: []` on a LABELLED row is silence, not compliance.**
   Labelled rows are never rule-checked — 0 of 26 carry violations, against 12
   of 92 unlabelled.
3. **A row stamped `failed:structure` is not a failed measurement.** The fit
   completed and the *budget rules* rejected it. The numbers are evidence.
4. **A fit that measures a circuit already known to be wrong is wasted.** The
   `@pktC` receipt measured an arrangement carrying a live metadata-swap defect.
   Repair first, then measure.
5. **Component checks passing is not the design closing.** A palette verified
   against the sheets, a light rig verified against a face table and a mesh
   verified by CRC can all pass while the thing is wrong. This is the art law,
   and it transfers: gates catch regressions, only looking catches wrongness.

## 7. After the numbers land

The ceiling is `5CSEBA6U23I7`: **41,910 ALM / 112 DSP / 553 M10K**. The older
30k/85 figure is an aspiration, not the gate.

**ALMs are the binding constraint and memory is the slack** — but the lever is
**lookup-for-computation**, not relocating state. Moving a register file into an
M10K does not buy ALMs; replacing an arithmetic cone with a table does. And
M10K is not free: R59's own price was wrong by 2.4×, and TERRAIN.LOD's store is
**185 M10K of 553**, not the ~77 that was claimed.

Optimisation starts only once the map exists. Guessing which subsystem is over
before the fit says so is how the wrong thing gets optimised.
