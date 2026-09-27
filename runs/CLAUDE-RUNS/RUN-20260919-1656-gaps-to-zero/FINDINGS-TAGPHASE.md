# FINDINGS -- TAGPHASE, 2026-09-27

**Branch `gz/tagphase`.** Base `4c2191d6`. Worktree
`C:\programmieren\zencrifice\gz-tagphase`.
Commits `b32224a6`, `dcf152e4`, `5d7e6994`, `7dfdcb1c`, `9565124c`, and the
commit carrying this file.

Written against the brief's nine numbered Deliverable items.

---

## THE ONE-LINE ANSWER

**CLAUSE 6 IS MET WITH THE PUBLISHER ARMED, BY REPAIR.** `samples` 1213 -> 1216,
stray tag beats 2 -> 0, `SMOKE: PASS`, `SMOKE_RC=0`, and `matpub` still reports
its 64 bursts in the same run.

**And the brief's central sentence was wrong in a load-bearing place, for the
eleventh packet running.** The defect is not the tile-store clear path and it is
not a tag defect. It is a **metadata swap at the composer's own join**: at
`GEOM_WALK_RASTER = 1` the door read 378 bits of material state
COMBINATIONALLY from GEOM.PARAMWALK's live output bus while the other 2,037 bits
were held. **Sixty of 101 job accepts read a bus presenting no record at all.**

Register **1 -> 1**, bare, RC 1, **0 unresolvable**. **I34's head line
untouched.**

---

## 1. DELIVERABLE 1 -- CLAUSE 6 MET WITH THE PUBLISHER ARMED, AND HOW

### 1.1 The before and after, same script, same fixture, same tree

| | base `4c2191d6` | at `5d7e6994` |
|---|---|---|
| plain smoke | **RC 1** on the sample gate | **PASS, RC 0** |
| `texture fragments/samples` | 1216 / **1213** | 1216 / **1216** |
| `plan_accepted` / `dispatch_accepted` | 1213 / 1213 | **1216 / 1216** |
| `tagprobe tagged_beats` | **2** | **0** |
| `matpub` | `cells=1024 bursts=64 denied=0` | **unchanged -- still armed** |

**The gate was not disarmed and neither was the publisher.** `matpub` reports
its 64 bursts and zero denials in the passing run.

### 1.2 The number no previous packet had, and it decided the mechanism

`plan_accepted=1213` against `fragments=1216`. **Three fragments never got a
PLAN at all**, so this was never the TMU generation check dropping published
commits -- their producer asked for nothing. That single number moved the
search from the texture island to the material state.

### 1.3 The defect, measured in four seams and then at the join itself

A committed probe reads the state at each seam; the first dirty one owns the
fault. Every number below is from the plain run with the publisher armed.

```
ROUND 1  WALK records=101 invalid=0 zero_sample=0 nonzero_tag=0
         LIVE captures=75 invalid=0 zero_sample=0 nonzero_tag=0
         FRAG accepted=1216 nonzero_tag=2 | STORE writes=1216 nonzero_tag=2

ROUND 2  preEZ n=1216 tag=2 zsamp=3 | postEZ tag=2 zsamp=3
         admit tag=2 zsamp=3 | retire tag=2
         control[0..7] state=00000000 tag=00        <- ordinary fragments
         retire[0] tag=22 addr=12 state=04c00000    <- the offenders
         retire[1] tag=1d addr=32 state=04c00000

ROUND 3  jobs=101 walk_bus_idle=60 ms_invalid=3 ms_zero_sample=5 ms_nonzero_tag=4
         badjob ordinal=21 valid=0 sampcnt=0 tag=22 fragstate=04c00000 pw_t_valid=0
         badjob ordinal=36 valid=0 sampcnt=0 tag=1d fragstate=04c00000 pw_t_valid=0
```

Round 1 ruled out the arena round trip and the composition. Round 2 ruled out
Early-Z, the 491-bit skid, the texture island and the fragment leaf -- four
seams by measurement rather than by reading -- and its **control** is what makes
the rest evidence: every ordinary fragment reads `state=0 tag=0` while the two
offenders read `state=04c00000` AND a stray tag, so **three matstate fields are
wrong together**. Round 3 read the matstate at the job accept and found the two
`badjob` rows carrying **the exact tag bytes and the exact fragment state that
reached the framebuffer**. The chain is closed by measurement, not inference.

**Round 1 captured the state only for the offenders**, which cannot separate
"this fixture's fragments carry 0x04c00000" from "these two got a strange
state". The control in round 2 is the whole reason the answer is decidable.

### 1.4 What was wrong

`zhao_geom_bin_pipe_v2`'s `JOB_SRC == 1` door takes the job's whole 1,877-bit
metadata from its own input ports. **2,037 of those bits are GEOM.SETUP's and
GEOM.ATTRPACK's output and ARE held correctly** -- on the walk both back ends
retire on `tw_be_out_ready_o`, asserted only on `job_valid_o && job_ready_i`.

**The remaining 378 were held by nothing.** `tri_flat_request_i`,
`tri_continuation_tail_i` and `tri_fragment_state_i` are combinational from
`tri_matstate_c`, hence from `pw_t_matstate_w` -- GEOM.PARAMWALK's LIVE OUTPUT
BUS. And `zhao_geom_tilewalk` RELEASES that record in `T_TAKE`, waits in
`T_WAIT` for the back ends, and only offers the job in `T_JOB`.

So the door read **A's corners, A's planes and B's material state**. CLAUDE.md's
metadata-swap chapter, at this composer's own join.

**One cause, both symptoms** -- which two packets argued as one and as two
without measuring either:

* `zhao_ms_flat_request` returns ALL-ZERO when the STORED `VALID` bit is low --
  the legal "takes no texture sample" profile -- so those jobs asked for
  nothing;
* `zhao_ms_tail` reads `EFFTAG` **ungated by VALID**, so the same instant leaked
  a stray tag into a pixel nothing tagged.

### 1.5 The repair

`fpga/rtl/common/zhao_walk_meta_hold.sv`: one hold register and two interlock
counters, capturing on the edge GEOM.PARAMWALK's record is CONSUMED
(`tw_be_valid_w && st_tri_ready_w && ap_tri_ready_w`, which is the walker's own
`be_valid_o && be_ready_i`) and holding until the job is taken. Exactly the
discipline the other 2,037 bits already had.

**It narrows nothing.** The same 128 bits reach the door, on the clock they
belong to. **Every `zhao_geom_*` file is untouched** -- `git diff
4c2191d6..HEAD --name-only` matches nothing under `fpga/rtl/geometry`,
`fpga/rtl/field` or `fpga/rtl/terrain`.

**One entry and not a queue**, because the walker's handshake is serial: one
record taken, handed over and its job offered before the next can be taken. If
that ever stops being true, `err_overwrite_o` says so.

### 1.6 Anti-vacuity, which is the part to read

The new console assertion does **NOT** assert `walk_bus_idle` to zero. It counts
job accepts at which the paramwalk presented no record, which is a property of
the walker's own `T_TAKE`/`T_JOB` sequencing and is **TRUE of a healthy
console** -- asserting it away would be asserting that the walk stopped walking.

**It is asserted NONZERO instead**, and it reads 57. That is what makes the three
clean lines beside it evidence: the matstate reads correct **at accepts where
the bus has nothing on it**, which can only be true if something is holding it.
With `walk_bus_idle` at zero a held record and a live read would be
indistinguishable and the three checks would pass on a console that never
exercised the hold.

---

## 2. DELIVERABLE 2 -- ALL SIX CLAUSES, EACH SEPARATELY

| # | clause | verdict at `9565124c` |
|---|---|---|
| 1 | installed **and executed** | **MET.** `-FieldActive`: `fldearth records=1 runs=1089 noprog=0 not_begun=0 skipped_uncovered=0 faults=0 tail_rejected=0 short=0`. |
| 2 | **covers the intended terrain** | **MET.** `fldpatch tp_covers=1`, `skipped_uncovered=0`. |
| 3 | cannot equal the baseline by accident | **MET.** `terrmat field_composed=1024 token_refused=0`, and `SMOKE: fieldmat CLAUSE 3/4 EXECUTED` -- the assertion runs, it is not a number printed above a gate. |
| 4 | reaches the **intended consumer** | **MET, AND ON BETTER EVIDENCE THAN BEFORE.** `tile_max=212` = `SFF_MAT_A` against an authored plane topping out at 6; and the request side now shows **26 mosaic requests naming exactly {212, 30}**, so BOTH named tiles demonstrably arrive at the consumer. Section 4. |
| 5 | uncovered **restores the authored result** | **MET.** `-FieldUncovered`: `runs=0 skipped_uncovered=1089`, `field_composed=0`, `SMOKE: fieldmat CLAUSE 5 EXECUTED tile_max=6`, `tile[max/or]=[6 7]` -- the plain run's tiles exactly. |
| 6 | **no-field forms unchanged** | **MET. `SMOKE: PASS`, RC 0**, `pixels=2816`, `frames_admitted=1`, `fragments=1216 samples=1216`, `tile[max/or]=[6 7]`, `tagged_beats=0`, `reserved=0`, with the publisher ARMED. |

**Clauses 4 and 5 still read the same instrument in opposite directions** --
212 in the covered form, 6 in the uncovered and no-field forms -- so each arm is
the others' fail demonstration. That property is preserved.

**BOTH FIELD FORMS NOW STOP ONLY ON `lane_desync_o`**, the inherited red the
brief says not to silence. **It was previously UNREACHABLE in those forms**
because the sample gate fired first; my repair did not cause it, it EXPOSED it
-- LASTGAP's own "each fatal only reveals the next one". Verified structurally
rather than asserted: this branch changes no file under `fpga/rtl/field`,
`fpga/rtl/terrain` or `fpga/rtl/geometry`, and the field engine's own census is
unchanged (`runs=1089 noprog=0 faults=0`).

---

## 3. DELIVERABLE 3 -- THE DECLARED-UNRUN FORMS, RUN, WITH VERDICTS

Every row the brief named. **All BUILT AND RUN at this branch**, not argued.

| form / target | verdict |
|---|---|
| plain smoke | **PASS, RC 0** -- section 1 |
| `-FieldActive` | clauses 1-4 all EXECUTE and pass; stops on the inherited `lane_desync_o` |
| `-FieldUncovered` | clause 5 EXECUTES and passes; stops on the same inherited red |
| `-Mutant` | **PASS, RC 0**, INVERTED -- `terr_pl_slot_overflow_o fired 1 time(s)` |
| `-BadVertex` | **PASS, RC 0** -- `pixels=512`, one refused record dropped its batch (holes=1, groups_poisoned=2, replay_poisoned=8) |
| `-NoEchoArm` | **PASS, RC 0** |
| `-BadTraceArm` | **PASS, RC 0** |
| `-TerrainFlatLattice` | **PASS, RC 0** -- `pixels=2560`, and **it needed no new term**: UNPARK's `50 = 14 + 36` still holds (`50 triangle(s)`, `jobs=36`) |
| `test_cmd_exec_directed` (R60) | **977 checks passed, RC 0** -- the same 977 PHASEFIX measured, so the target has not drifted |
| `walk_meta_hold_directed` (new) | **33 checks, 0 failures, RC 0** |
| TypeScript suite | **RUN. Section 6.5.** |

**`tests/command/run_cmd_exec_directed.ps1` is committed.** Four consecutive
packets quoted R60's own named target as "not run", and the reason was never
reluctance: `cmake --preset windows-native` in a fresh worktree verilates the
WHOLE suite, which is hours. **The rule kept losing to the cost of obeying it**,
which is the shape CLAUDE.md finds in advisory prose. The runner is committed so
the next packet inherits the path rather than the excuse.

---

## 4. THE FINDING THAT IS LARGER THAN CLAUSE 6

**The swap corrupted the terrain material MOSAIC PICK**, and no previous packet
had this. Read rather than assumed, at
`zhao_texture_island_v3_top.sv:1314-1316`:

```systemverilog
.wr_mosaic_material_a_i(frag_base_rgb_i[23:16]),
.wr_mosaic_material_b_i(frag_base_rgb_i[15:8]),
.wr_mosaic_weight_i(frag_weight_i),
```

Those are the flat request's `base_rgb` and `recipe_weight` -- `ms[BASERGB]` and
`ms[WEIGHT]`, **exactly the fields the swap corrupted**. So terrain fragments
were picking their mosaic tile with ANOTHER TRIANGLE'S mat_a, mat_b and weight.
Three lost samples was the symptom that happened to trip a gate; this is the
consequence that did not.

**AND IT CHANGED CLAUSE 4'S EVIDENCE, WHICH I CHASED RATHER THAN ACCEPTED.**
Repairing the join moved `-FieldActive`'s `tile_or` from 222 to 212 -- tile 30
stopped being fetched. The comfortable reading is "the requests became
coherent", and this repository's law is that the explanation which absolves the
design is the one to check hardest.

Measured instead:

```
tagphase4 field pair: requests naming {212,30} = 26, weight[min/max]=[248 250]
          picks of MAT_A=26, picks of MAT_B=0
```

**mat_b DOES reach the mosaic** -- 26 requests carry the field's pair -- at
weights 248..250 of 255. Under the frozen law (`terrain_rules` 6.2,
`pick = p < weight ? mat_a : mat_b`, `p = hash mod 255`) mat_b needs
`p >= weight`, which is 5..7 of 255 hash values, about 2.4%. **Over 26 requests
the expected number of mat_b picks is about 0.6.** Observing zero is ordinary.

**So tile 30's absence is not a regression, and the OLD `tile_or = 222` was
evidence of the DEFECT rather than of the field working.** Clause 4's evidence
is stronger than it was, because the request side now proves both named tiles
arrive.

**A coarse first version of that probe would have misled me**, and it is
committed in corrected form for that reason: `mat_b[or]=ff` and
`mat_b_nonzero_reqs=1216` look like a complete answer and are not. The mosaic
lane runs with `req_mosaic_i` tied HIGH and serves EVERY fragment, mesh
included, so an OR across 1,216 picks saturates and says nothing about terrain's
own pair. The overall `weight[min/max]=[90 250]` says the same from the other
side: 90 is the MESH material's constant `0x5A`.

---

## 5. DELIVERABLE 4 -- `I55` IS NOT REGRESSED

All four numbers the brief names, in **every** form that draws the full frame:

| | plain | `-FieldActive` | `-FieldUncovered` |
|---|---|---|---|
| `paramwalk tris` | **101** | 101 | 101 |
| `fetcharm vread` | **303**, invariant `vread == 3*tris` asserted | 303 | 303 |
| `tilewalk tiles/jobs/door` | **11 / 101 / 101** | 11 / 101 / 101 | 11 / 101 / 101 |
| `raster pixels` | **2816** | 2816 | 2816 |

`frames_admitted=1` in all three. `-TerrainFlatLattice` reads 2,560 and
`-BadVertex` 512, which are their declared not-2,816 numbers.

**What else moved, declared rather than left to be found.** In the plain run
`cache[hit/miss]` went `[1213 49]` -> `[1216 44]`, `palette_lookups` 41 -> 26 and
`mosaic fills` `[29 20 0]` -> `[23 21 0]`. That is the texture requests becoming
COHERENT: jobs that had been asking with another triangle's material state now
ask with their own, so they hit. `walk_bus_idle` itself moved 60 -> 57, which is
the island's changed backpressure re-pacing the job accepts -- downstream
feedback, not a second change. **No I55 number and no pixel count moved.**

---

## 6. DELIVERABLE 6 -- WHAT I FOUND FALSE, AND ONE IS MINE

### 6.1 THE BRIEF'S CENTRAL SENTENCE, and it aims the repair at the wrong file

> *"the stray-tag pixels are ALL `rgb565=0000`, uncovered pixels whose colour
> cleared and whose tag byte did not ... The repair lives in
> `zhao_shell_top_v2`."*

**Both halves are false, and the first is impossible in this fixture.**
`tb_zhao_console_core_smoke.sv:5550` drives `frame_clear_word_i = '0` -- ONE
64-bit constant covering colour AND tag -- and `zhao_raster_tilestore` has **no
byte enables by explicit design** (its header argues the point at length). A
pixel whose colour came from the clear therefore has a tag byte of zero **by
construction**. These pixels were WRITTEN.

And the repair is in `zhao_console_core`, not the shell. The shell passes
`walk_job_*` and the matstate ports straight through; the join is written in the
composer.

**This is the eleventh consecutive packet to find its brief wrong in a
load-bearing place**, and the direction is the usual one: it made the remaining
work look like a clear-path fix in a file the packet already owned.

### 6.2 THE GATE THAT FIRES IS THE SAMPLE GATE, NOT THE TAG GATE

The brief presents clause 6 as a tag problem. In the plain run the tag gate
PASSES (`reserved_beats=0`) and the **sample** gate is what exits 1. The two
symptoms share one cause but only one of them was ever the blocker.

### 6.3 A DETECTOR THAT CANNOT FIRE, AND ITS COMMENT DESCRIBES A DIFFERENT ONE

`zhao_geom_tilewalk.sv:255-258`:

```systemverilog
if ((state_q == S_RUN) && (tstate_q != T_TAKE) && t_valid_i && t_ready_o)
  overlap_o <= overlap_o + 32'd1;
```

and `t_ready_o` is itself `(state_q == S_RUN) && (tstate_q == T_TAKE) &&
be_ready_i`. **The guard demands `tstate_q != T_TAKE` and then ANDs in a term
that is only true when `tstate_q == T_TAKE`.** The two are contradictory, so the
counter is a **structural zero**.

Its own comment says it counts *"on the OFFER and not on a take"* --
`t_valid_i && t_ready_o` **is** a take. So the comment describes the correct
detector and the code implements the impossible one.

Its committed mutant fires it only by **deleting the state term from
`t_ready_o`**, so `overlap_o == 0` is evidence about that one term and **not**
about the handshake the comment describes. The brief's own instruction -- "ask
what a counter sums before citing it" -- applies to this one.

**I did not edit it.** It lives in a file this packet may only read. It is
reported, and `err_overwrite_o` in the new block watches the same property from
the CONSUMER's side, where no producer term can cancel it, fired by ordinary
stimulus rather than by a mutant.

### 6.4 `zhao_geom_bin_pipe_v2`'s OWN CLAIM ABOUT THOSE PORTS

`zhao_geom_bin_pipe_v2.sv:111-117` says the metadata fields *"arrive here on
`tri_*_plane_i`, `tri_ax_i..tri_cy_i`, `tri_src_id_i`, `tri_area2_i`,
`tri_min_x_i`, `tri_fragment_state_i` and `tri_continuation_tail_i` -- the same
ports, on the same silicon, whichever source fed it."*

**True of the ports and false of the TIMING**, which is the half that mattered:
the plane ports are driven by GEOM.ATTRPACK's held output, the three matstate
ports by a live combinational path from the paramwalk's bus.

### 6.5 THE TYPESCRIPT SUITE HAS BEEN RED SINCE 2026-09-16 AND NOBODY RAN IT

Four consecutive packets declared it unrun. It is run now, and there are three
separate things, all dated, none mine (`git diff 4c2191d6..HEAD` touches zero
files under `tools/`, `spec/` or `compiler/`):

1. **`npm test` cannot reach its own tests.** Node v24.19.0 no longer accepts a
   directory for `node --test <dir>`; it resolves it as a module and dies with
   `MODULE_NOT_FOUND`. The suite therefore fails with something that looks like
   a broken package **before running a single test**, which is what has been
   masking the two reds below.
2. **`tools/ledger` 63/64.** `V17d: an existing test file that never names its
   oracle is an alias, rejected` fails. Broken by **`1afe5a30` (2026-09-16)**,
   whose own message is *"ledger_check goes green: V17(d) now asks what its own
   sentence says"* -- it changed `rules.ts` and did not update `rules.test.ts`.
   **A production rule was moved to make a repo-wide check green and its unit
   test was left asserting the old behaviour.**
3. **`tools/abi-gen` 17/20.** Three fail on the ratified table: `4 !== 3`
   commands and `SetView 112 !== 96`. `spec/commands.zidl` last moved in
   **`97a36b81` (2026-09-26, TERRAINMAT)**, "terrain gets a material IDENTITY --
   the ABI field", and the expectations were never updated. **And
   `npm run abi:check` is CLEAN, RC 0, "38 outputs match"** -- the gate people
   run is green while the test encoding `capture_format.md` 1.3 is red.

`tools/fixgen`: **14/14 pass.**

Reproduction, since `npm test` cannot get there:
`cd tools/<ws>; node --test dist/test/*.test.js` (pass the files, not the
directory). `typescript` and `@types/node` were absent in this worktree;
`npm install` added 32 packages and `node_modules` is gitignored, so none of it
is committed.

**This is MEMORY.md's "local gates must match CI" one level up**: the tool IS
pinned as a devDependency, and the thing that drifted is the RUNNER.

### 6.6 MINE, AND THE CONTROL FORM CAUGHT IT

My sharpened mosaic probe referenced `SFF_MAT_A`/`SFF_MAT_B`, which live in
`smoke_field_fixture.svh` -- itself included under `ifdef
ZHAO_SMOKE_FIELD_ACTIVE`. Unguarded, **every non-field form died at verilate**
with *"Can't find definition of variable: 'SFF_MAT_A'"*. `-Mutant` is the run
that caught it. That is the argument for running the control forms rather than
reasoning about them, made at my own expense.

---

## 7. DELIVERABLE 7 -- WHAT I REFUSED, AND WHAT ELSE I GOT WRONG

### Refused

* **Editing `zhao_geom_tilewalk.sv`'s blind `overlap_o`.** Out of bounds for
  this packet; reported with the exact expression and the reason its mutant does
  not rescue it (6.3).
* **Changing the `node --test` invocation in `package.json`.** The portable form
  depends on CI's Node version, which I cannot see from here, and silently
  breaking a shared test entry point the day before the full console fit is a
  loud, expensive failure against an invisible benefit. Reported with the exact
  working reproduction instead (6.5).
* **Repairing `abi-gen`'s three ratified-table expectations.** They encode
  `capture_format.md` 1.3 and the ABI moved under them; deciding what the
  ratified table now IS belongs with whoever changed it, not with a packet
  passing through.
* **Silencing `lane_desync_o`.** The brief says not to, and it is now the only
  thing stopping either field form.
* **Putting the hold register in the composer directly.** `zhao_console_core`
  already owns 22 `always_ff` blocks, so it would have been consistent with
  practice and cheaper in ceremony -- but a block gets a standalone directed
  test in which both counters fire by stimulus, and a composer register would
  have owed a committed mutant instead.

### Got wrong and caught myself

1. **I hit the documented heredoc trap twice**, on quote-heavy `python - <<PY`
   and `cat <<EOF` text, for the sixth packet running. The remedy in
   PACKET-PROTOCOL.md worked both times.
2. **My first mosaic probe was too coarse to answer its own question** (section
   4) and I nearly reported `mat_b_nonzero_reqs=1216` as the discriminator. It
   counts mesh fragments too.
3. **I read `LINTRC=0` off a pipeline** whose exit status was `head`'s -- this
   tree's documented read-the-pipeline's-status trap, hit on my first lint of
   the new block. The real lint was re-run under PowerShell with
   `VERILATOR_ROOT` set.
4. **My first instinct was the tile store and the resolve**, and I read both
   closely before checking what the bench drives as the clear word. One `grep`
   for `frame_clear_word` would have killed the brief's premise in a minute.

---

## 8. DELIVERABLE 8 -- WHAT THE FULL CONSOLE FIT WILL NEED TO KNOW

**1. There is one new production block and it is small.**
`zhao_walk_meta_hold`: 128 matstate bits + an 18-bit arena id + one valid bit +
two 32-bit censuses = **211 registers, 0 DSP, 0 M10K**, hand-counted and NOT
fitted (R236). Order 250 ALM. It is in the console fit source list
(`design/fit_targets.yml`, **299 sources now**) and in `design/prod_manifest.yml`.

**2. Two new core ports, and every generated file is refreshed.**
`geom_wmh_job_unheld_o` and `geom_wmh_overwrite_o`. `zhao_console_board.sv` was
REGENERATED (**1,640 core ports**, 127 parameters, `--check` FRESH);
`gen_prod_top.py --check` is fresh at **89 instances** -- unchanged, because the
block is not a top. Both wrapper mutants and the bench's `dut (.*)` net list
carry the two ports; `.*` binds by NAME, so a missing net is an elaboration
error rather than a silent miss.

**3. Gate 31 is OK with NO MISSING PIN**, self-test fired 5/5, at 299 sources.

**4. `check_quartus17_syntax.py` is RC 0 over 676 files**, and the new block was
written to the Quartus 17 rules deliberately: its elaboration checks sit inside
`initial begin ... end` rather than at module scope, which is the form CLAUDE.md
records Verilator accepting silently and `quartus_map` rejecting. **It has still
never been through `quartus_map`** -- no packet may run Quartus -- so that is an
unmeasured claim and is flagged as one.

**5. The arrangement being fitted is 1.** `GEOM_WALK_RASTER = 1` by default and
this repair is on that arrangement's critical path. At `GEOM_WALK_RASTER = 0`
the hold is still elaborated and still captures, but `tri_matstate_c` takes the
door arm, so it is dead weight in the retained oracle -- **about 211 registers
arrangement 0 does not need.** Named here rather than left to surprise someone.

**6. The added path is a REGISTER, not combinational depth.** The hold replaces
a long combinational run (paramwalk bus -> `tri_matstate_c` -> `zhao_ms_*`
unpacks -> `tri_meta_w` -> the door) with a registered one, so if anything it
should help the cone that feeds `job_metadata_capture_w`. That is an
expectation, not a measurement, and the fit is what settles it.

**7. The console-board lint row is still the INHERITED red** PACKET-PROTOCOL
records (262 DECLFILENAME/PINCONNECTEMPTY warnings under `-Wall`). I did not
re-measure its count against base; the protocol says not to read it as mine and
to diff before claiming otherwise.

**8. Two things the fit cannot answer and should not be asked to.**
`lane_desync_o` in both field forms, and the TypeScript suite's two dated reds
(6.5). Neither is a synthesis question.

---

## 9. GATES, AT THE PUSHED HEAD

| gate | result |
|---|---|
| `completion_register.py` **BARE** | **1** -- `I34`. **RC 1.** 0 unresolvable. **Head line untouched.** |
| `check_console_inventory.py` | **OK** -- 411 declared, 290 elaborated, 299 fit sources |
| `check_prod_manifest.py` | **OK** -- every module counted once or declared absent |
| `gen_prod_top.py --check` | **fresh (89 instances)** |
| `gen_console_board.py --check` | **FRESH (1640 core ports, 127 parameters)** -- regenerated |
| `gen_shell_paired_diff.py --check` | **fresh**, harness AND mutant |
| `check_quartus17_syntax.py` | **RC 0**, 676 files, self-test 13 fire / 22 no-fire |
| `check_case_labels.py` | **OK**, self-test 3 fire / 1 no-fire |
| `check_console_closure_lint.py` (gate 31) | **OK**, self-test fired 5/5, **no missing pin** |
| `mutant_copy_drift.py` (**AFTER** the commits, R121) | **OK** -- 80 copies, no drift |
| `lint zhao_walk_meta_hold -Wall` | **RC 0** |
| `walk_meta_hold_directed` (BUILT AND RAN, R60) | **33 checks, 0 failures** |
| `cmd_exec_directed` (BUILT AND RAN, R60) | **977 checks passed** |
| `npm run abi:check` | **clean, RC 0** (38 outputs match) -- and see 6.5 |

---

## 10. PROCESS

Other repositories' builds ran on this machine throughout; **nothing was
killed**. **No Quartus was run. No subagent was spawned** (PACKET-PROTOCOL's "a
packet does not spawn subagents"). **No `zhao_geom_*` file was edited** --
verified by `git diff --name-only`, not asserted. No `until ... sleep` poll loop
was armed. Pushed only `gz/tagphase`, never `--force`, never
`--force-with-lease`. **I did not write the head-line declaration.**
