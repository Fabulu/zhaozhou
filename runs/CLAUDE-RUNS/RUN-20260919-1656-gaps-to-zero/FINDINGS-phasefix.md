# FINDINGS — PHASEFIX, 2026-09-27

Branch `gz/phasefix`, worktree `C:\programmieren\zencrifice\gz-phasefix`,
base `9a8ba109`. Reported against the brief's nine numbered Deliverable items.

**The headline: the console DRAWS at `GEOM_WALK_RASTER = 1`.** `raster
pixels=2816`, `resolved_tiles=11`, `SMOKE: PASS` — and 2,816 is the
reference-derived number the shipped arrangement 0 draws, not a number this
packet chose. Every one of those pixels came from a triangle the walker fetched
out of SDRAM.

**And the brief's own central sentence needed one correction, in the same
direction its predecessor's did.** The brief names `zhao_shell_top_v2.sv:1653`
— `rpx_ready = !post_phase_w && fbw_px_ready` — as "THE LINE". That line is the
**symptom**. Nothing about it is wrong and changing it would have been wrong.

---

## 1. THE REGISTER, MEASURED BARE

| | value |
|---|---|
| at base `9a8ba109` | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |
| at my last pushed commit | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |

**`I55` is not claimed closed by this packet**, and §5 says exactly why not and
what is now left of it. The blocker is gone and the console draws at 1; what
remains is a shipping decision with an unmeasured area and throughput cost.

Nothing was closed by removing, narrowing, stubbing, tying off or
disconnecting. The binner's on-chip drain is **untouched** — `git diff` against
`zhao_geom_binner_v2.sv` and `zhao_geom_bin_pipe_v2.sv` is **empty**. This
change only DELAYS a phase.

---

## 2. THE PHASE FIX — AND THE BRIEF'S LINE IS THE SYMPTOM, NOT THE FAULT

### 2.1 — Where the decision is actually made

`rpx_ready = !post_phase_w && fbw_px_ready` is a **correct** line. It expresses
the design's own rule: ONE framebuffer writer, switched rather than arbitrated.
The fault is that `post_phase_w` was **allowed to rise too early**, and that
decision is made two modules up, in `zhao_post_lease`:

```systemverilog
  assign raster_done_c = raster_quiet_i && !raster_px_i && fbw_drained_i;
```

`raster_quiet_i` is `zhao_geom_bin_pipe_v2.quiet_o`, and **every term of it
describes the BINNER and the tile pipe**:

```systemverilog
  assign quiet_o = binner_initialized_o && !frame_inflight_q &&
                   !frame_begin_i && !drain_busy_o && tile_quiet_w;
```

At `JOB_SRC = 1` the binner issues **no job at all** — `job_valid_w =
walk_job_valid_i`, and `bin_job_ready_w` is assigned `1'b1` so the drain's jobs
are consumed and discarded. So at `frame_end` the tile pipe is **genuinely
idle** and `quiet_o` is **genuinely TRUE**, while GEOM.TILEWALK — the machine
about to draw the entire frame — has not started. It *cannot* have started: the
sweep reads an arena that `frame_end` is what seals.

**THIS IS NOT A BROKEN COUNTER. It is a correct signal asked the wrong
question.** CLAUDE.md: *"a gate that cannot reach the state is not evidence
about the state."* Nothing in `quiet_o` could ever have gone red, because there
is no fault in what it watches. The arrangement changed the producer out from
under a signal whose name still fitted.

### 2.2 — Which of the three shapes, and why the other two are not choices

The brief offered three. **I chose "the post phase opens later", and the other
two are not alternatives that lost on cost — they are unavailable.**

* **Make the raster's claim survive (arbitrate the port).** Refused, and **not
  on area**. The ~300 ALM argument in the shell's own comment is beside the
  point: **POST.COMPOSITE READS THE FRAMEBUFFER AS ITS SOURCE**
  (`u_source` / `zhao_post_fbread`, origin `fb_base_i`). A raster writing into
  that buffer during a post pass would be composited from a half-written frame.
  That is a **correctness fault**, not a cost, and it would have been invisible
  in a pixel count — the run would have drawn 2,816 pixels and been wrong.
* **Run the sweep earlier.** Impossible rather than expensive. The walk reads
  the parameter arena, which does not exist until the frame seals, and the seal
  is caused by `frame_end`.

So "the post phase opens later" is the only shape that is both correct and
free, and it is free because it changes no datapath at all.

### 2.3 — What was built

**`zhao_post_lease` gains `WALK_GATE` and `walk_active_i`**, and a three-state
FSM folded in as a **second term**:

```systemverilog
  assign raster_done_c = raster_quiet_i && !raster_px_i && fbw_drained_i
                         && !walk_hold_c;
```

It is a second term and **not a repair of the first**, deliberately: the two
are enabled by different things — the bin pipe's state machine, and a sequencer
three modules away — which is the property CLAUDE.md's chapter on lockstep
detectors asks for. Repairing `quiet_o` instead would have put both sides of
the comparison back under one producer's control.

**`WK_OWED` is the state that looks droppable and is not.** Gating on
`!walk_active_i` alone is **TRUE at the instant of `frame_end`**, so the post
phase opens in the gap before the sweep begins — which is the defect, not the
repair. Measured: that gap is **1,513 clocks** on this fixture.

**`WALK_GATE` is `JOB_SRC`, passed through from the shell**, not a second knob.
The door and the gate are two halves of one arrangement: a console with the
door open and the gate shut draws nothing (that is I55 for eight packets), and
one with the gate built and the door shut would hold the post phase for a sweep
whose jobs cannot reach the raster. Passing the same parameter makes disagreeing
about it **unrepresentable** rather than merely discouraged.

**At `WALK_GATE = 0` nothing is built.** The generate branch drives
`walk_hold_c` from a literal, both counters are tied, and `walk_active_i` has
no reader — sunk explicitly in `unused_c` rather than waived by a lint file,
because a directory-wide UNUSEDSIGNAL waiver is how this tree lost `id_c`.

---

## 3. A PIXEL WHOSE BYTES WENT THROUGH SDRAM, AT ARRANGEMENT 1 — DELIVERED

Same script, same fixture, measured on this tree at both ends.

```
base 9a8ba109                        this branch
  raster    pixels=0                   raster    pixels=2816 bursts=176
  tilewalk  tiles=3 jobs=13 door=13    tilewalk  tiles=11 jobs=101 door=101
  paramwalk tris=14                    paramwalk tris=101
  rasterdiag resolved_tiles=0          rasterdiag resolved_tiles=11
  clip      setup_submitted=89         clip      setup_submitted=176
  %Fatal: 1 frame(s) ADMITTED and      walkphase phasehold=68203 clk
  the shell still rasterised 0                   phasesweeps=1
  pixels from 89 triangles             SMOKE: PASS
```

**2,816 is the SHIPPED arrangement's own number** — reference-derived by
`smoke_geom_fixture_gen.cpp`, which tessellates the same lattice through the
same projector, clip, setup and binner, and required by the gate list. The walk
arrangement draws **the same picture**, which is the only form of "it works"
worth having.

### The chain closes end to end, in counters kept by six modules on six different register enables

```
  arenabin refs=101   ==  binrefs tile_references=101   (independently binned)
                      ==  tilewalk jobs=101
                      ==  door=101                      (walk_jobs_taken_o)
                      ==  rasterdiag jobs[started]=101
                      ==  paramwalk tris=101
  fetcharm vread=303  ==  3 x 101
  resolved_tiles=11   ==  tilewalk tiles=11 == arenabin tiles=11
  setup_submitted=176 ==  75 live + 101 walk
```

`setup_submitted = 176` is the time multiplex, restated at the new scale:
METASIDE measured `89 = 75 + 14` when the sweep was cut off at 14 triangles.
The sweep now runs to completion, so it is `75 + 101`.

**Why these are SDRAM's bytes.** At `JOB_SRC = 1` the door is the only path to
`zhao_raster_tile_pipe_v2`, and the triangles it carries were decoded by
`zhao_geom_paramwalk` from descriptors and ProjectedVertex records it fetched
through the real guard, arbiter and controller (`paramwalk chunks=15 stale=0
illegal=0`, `fetcharm vread=303 vbad=0 pvsplit=0`). Directive §4: *"a complete
frame whose output depends on the bytes written and read through the real
guard/arbiter/controller path."*

### The cost, declared rather than absorbed

The post phase is held **68,203 gpu clocks**, of which the sweep is **66,690**
and **1,513** is the `frame_end`-to-publication gap. That is arrangement 1's
throughput price on this fixture and it is charged here, per directive §7,
rather than left for a later packet to find.

---

## 4. THE THREE ROUND-TRIP ASSERTIONS

METASIDE named this gap honestly rather than glossing it: `a_ms_flat_roundtrip`,
`a_ms_tail_roundtrip` and `a_ms_frag_roundtrip` were **live and silent through
89 triangles and had not been seen to fire**.

**No legal stimulus can fire them.** Both sides are expressions of the SAME
inputs — a literal concatenation against fourteen indexed writes and reads — so
what they differ on is the **layout**, and layout is not an input. That is
exactly the case CLAUDE.md's committed-mutant chapter exists for.

**THREE seams, because the three assertions read DISJOINT field sets.** Measured
in the unpackers rather than assumed:

| assertion | fields its unpacker reads |
|---|---|
| `a_ms_flat_roundtrip` | VALID, SAMPCNT, BASEBIND, RECIPE, WEIGHT, BASERGB, RESPCLS, PALSLOT, PALGEN |
| `a_ms_tail_roundtrip` | DETAIL, VTXALPHA, EFFTAG, STENREF |
| `a_ms_frag_roundtrip` | FRAGST |

So **one corrupted field fires exactly one of them**, and a single mutant would
have left two untested while looking like it had covered all three.

**The mutation is `~x`, not a dropped write, and that is the load-bearing
choice.** Omitting the write leaves the field **zero**, which **agrees** with
the composition on any triangle whose value happens to be zero — and this
fixture's defaults are exactly where the zeros live. A mutant that passes is a
mutant that proved nothing. `~x != x` for every x.

**Plain `ifdef` selecting between two definitions**, which is the form a
command-line `-D` reaches; CLAUDE.md records two combiner mutants that measured
**unmutated production** because `-D` cannot override a function-like define and
says nothing when it fails to. Each `else` arm is byte-identical to the line it
replaces, so with nothing defined the function is character-for-character what
it was.

**Each control requires ITS OWN assertion's message, not merely a nonzero exit.**
A control that accepts any failure passes when the build breaks, when a
different assertion fires, or when the fixture moves underneath it. The three
`$fatal` strings are distinct, which is what makes that check possible.

**The negative control is the PLAIN run**, which must stay silent.

**ALL THREE HAVE NOW BEEN SEEN TO GO RED.** Each fired on ITS OWN message, and
the plain run -- the negative control -- stayed silent:

| assertion | line | form | message matched | control |
|---|---|---|---|---|
| `a_ms_flat_roundtrip` | `:33785` | `-MsMutFlat` (`~weight`) | *does not reproduce the flat request* | **PASS** |
| `a_ms_tail_roundtrip` | `:33789` | `-MsMutTail` (`~vtx_alpha`) | *does not reproduce the continuation tail* | **PASS** |
| `a_ms_frag_roundtrip` | `:33792` | `-MsMutFrag` (`~frag_state`) | *does not reproduce the fragment state* | **PASS** |

So METASIDE's owed item is discharged: three detectors that had never been shown
to fire now have, individually, each against the fault it exists to catch.

Forms: `run_console_core_smoke.ps1 -MsMutFlat | -MsMutTail | -MsMutFrag`, each
with its own build-directory tag — all three change the same function, so two
sharing an object directory would mean the second measured the first's
mutation and passed for the wrong reason.

---

## 5. WHY `I55` IS NOT CLAIMED CLOSED, AND WHAT IS LEFT OF IT

`GEOM_WALK_RASTER` remains **0**. The brief authorises un-parking *"when the
console DRAWS at 1"*, and it now does — so this is a refusal that needs its
scope stated precisely, because CLAUDE.md is right that a wrong refusal is
caught by nothing at all.

**What is settled:** the scheduling fault is repaired, the walk feeds the live
raster, and §4's own test — a pixel whose bytes went through SDRAM — is met and
reproducible by one committed switch.

**What is NOT settled, and it is not a decision, it is two measurements:**

1. **The committed control forms have not been re-measured in arrangement 1.**
   `-Mutant`, `-BadVertex`, `-NoEchoArm`, `-BadTraceArm` and
   `-TerrainFlatLattice` all assert pixel counts (`2816`, `2560`, `512`) that
   were measured against the binner's drain. Flipping the default makes every
   one of them a walk-arranged run. They ought to agree — `arenabin` bins the
   same 101 references the binner does, proven equal in this very run — but
   "ought to" is the word this campaign keeps finding in refuted claims.
2. **Arrangement 1 has never been through a fit.** DOORCOST priced `JOB_SRC=1`
   at **+983 comb ALUT / +0 DSP** on the shipping part; my gate adds a 2-bit
   FSM and two counters on top of that, unmeasured. The device is already at
   ~350%. Flipping the shipped default on an unmeasured arrangement is exactly
   the "unmeasured claim about the tightest budget in the design" that three
   earlier packets refused for sound reasons.

**So the honest position is: the un-park is now a ONE-LINE change with its
evidence attached**, and it wants the control sweep plus a fit — which this
packet is forbidden. A parked entry reported open is honest; reporting it closed
while five committed controls have never run in the arrangement would make the
register lie in the direction nobody audits.

---

## 6. CLAIMS FOUND FALSE, AND ONE BENCH THAT WOULD HAVE LIED

### 6.1 — The brief's "THE LINE" is the symptom, not the fault

§2.1. `zhao_shell_top_v2.sv:1653` is correct and editing it would have been
wrong. The premise arrives in the flattering direction again — it makes the
remaining work look like a one-line edit in the file the packet already owns,
rather than a question about which producer a quiet signal names.

This is the **third** brief in a row whose central sentence needed correcting,
and the brief itself predicted it: *"That is the kind of error to hunt in this
brief too."*

### 6.2 — THE BENCH'S WAIT ORDER WOULD HAVE REPORTED THE WORKING CONSOLE AS BROKEN

This is the finding I would most want inherited, because it nearly cost the
result.

`tb_zhao_console_core_smoke.sv` waited for `render_drain_done_o` **first**, then
waited for `geom_tw_busy_o` to rise. `render_drained_o` is `fbw_drained_w &&
!post_busy_o` — so once the post phase is held for the sweep, **the drain cannot
complete until the sweep has**. By the time control reached the sweep wait, the
sweep was already over, and the bench would have printed

> `SMOKE: NOTE GEOM.TILEWALK NEVER STARTED`

**about a frame it had just drawn** — the most convincing-sounding wrong
diagnosis available, in the file whose job is to diagnose.

Repaired by waiting for the sweep first, in a block guarded by
`geom_walk_raster_o` so **arrangement 0 does not execute one statement of it**
and the shipped stimulus is unchanged. The old wait is left standing behind a
`tw_sweep_observed` flag rather than deleted, because arrangement 0 still runs
it and a future arrangement that does *not* hold the phase would need it.

**The general shape is worth more than the instance: a repair that changes WHEN
something finishes invalidates every bench that waits in the old order, and
those benches fail LOUDLY and WRONGLY rather than silently.**

### 6.3 — `gen_shell_fit_ports_v2.py` HAS BEEN RED SINCE 2026-09-25, AND IT IS NOT MINE

`python tools/quartus/gen_shell_fit_ports_v2.py` exits **1**:

```
FAIL: new input 'pg_valid_i' has no declared driver.
```

**Verified inherited rather than assumed inherited**, per CLAUDE.md's rule that
"inherited" is the word that makes a red nobody's job: reproduced **identically
at base `9a8ba109` in a throwaway worktree**, which the run left `git status`
CLEAN — so it writes nothing and is failing on its own DRIVERS table, not on
anything a packet has in flight.

**And the two dates decide whose it is.** `pg_valid_i` entered
`zhao_shell_top_v2.sv` in `b4ba5976` (**2026-09-25**, TERRAINAUX). The generator
was last touched in `c1e05192` (**2026-09-27**, NORMALMAP) — two days *later*,
and still red. So a packet has edited that tool since the port landed without
fixing it.

**The consequence I owe, stated rather than left to be discovered:**
`fpga/rtl/generated/zhao_shell_v2_fit_top.sv` **cannot be refreshed for my new
shell input** until `pg_valid_i` gets a declared driver. That file is a
synthesis instrument, not in the console's closure, and this generator is **not**
in the protocol's gate list — but it is now one port staler than it was, and
that is mine to declare even though the blockage is not.

### 6.4 — `gen_prod_top.py`'s dropping bug did NOT interact, and I checked rather than assumed

METASIDE's §6.5 found that the generator **silently drops blocks whose port
width reaches a package constant** — 89 instances became 86 — while `--check`
still reports fresh. My two core ports are literal `[31:0]`, so nothing was
dropped: **89 instances before, 89 after**, and `--check` fresh. The 66
deletions in that file's diff are the pseudo-random stimulus vector reindexing
by one new input bit, not blocks going missing. Worth stating because "89" is
the only tell that bug has.

---

## 7. WHAT I REFUSED, AND WHAT I GOT WRONG AND CAUGHT MYSELF

### Refused

* **Arbitrating RASTER.FBWRITE's pixel port.** §2.2 — a correctness fault, not
  a cost. The ~300 ALM saving is not why it is refused, and quoting it as the
  reason would have hidden the real one.
* **Repairing `quiet_o` instead of adding a second term.** It would have put
  both sides of `raster_done_c` back under one producer's control.
* **Retiring the binner's drain.** Directive §7 requires the complete oracle and
  at `GEOM_WALK_RASTER = 0` it is the thing drawing the picture. Untouched:
  `git diff` on both binner files is empty.
* **Un-parking `GEOM_WALK_RASTER`.** §5 — and the refusal's scope is stated
  there, because the thing it refuses is now two measurements rather than a
  question.
* **Deleting the bench's old sweep wait** (§6.2) rather than guarding it.
* **A single MATSTATE mutant covering all three assertions.** §4 — it cannot,
  and one that looked like it did would be worse than none.

### Got wrong and caught myself

1. **I hit the documented heredoc trap**, on the very first commit message, for
   the **fifth** packet running — SWAPCLOSE hit it three times, METASIDE four.
   The remedy in PACKET-PROTOCOL.md worked first time. The count is the
   finding: advisory prose has now lost to this five packets in a row.
2. **My first instinct was to edit the line the brief named.** I read
   `zhao_post_lease` before touching it, and the read is what showed the line
   was correct. Had I edited first and measured after, I would have shipped an
   arbitrated pixel port and a frame composited from a half-written buffer —
   which draws pixels, passes a pixel count, and is wrong.
3. **I nearly ran `gen_shell_fit_ports_v2.py`'s red as my own.** The throwaway
   worktree at base cost two minutes and moved it from "something I broke" to a
   dated, attributable inherited defect.
4. **I wrote the two wrapper mutants with one line-ending convention and had to
   redo one.** `zhao_console_core_slot_overflow_mutant.sv` is **LF**;
   `zhao_console_core_untex_decl_mutant.sv` is **CRLF**. Two committed copies of
   the same core's port list, in one directory, disagreeing about line endings —
   CLAUDE.md's EOL chapter says what that costs the next three-way merge.

---

## 8. THE LEAF `-MapOnly` ROWS, AND WHAT THEY COST TO GET

`zhao_post_lease` had **never been mapped or fitted** -- zero rows in
`zhao_block_map.json` and zero in `zhao_block_fit.json`. So these are a NEW
BASELINE and a delta measured against it **by the same tool on the same tree in
the same hour**, which is a real A/B rather than this campaign's
"declared-today versus measured-a-week-ago" shape.

```
  module zhao_post_lease, map_only, device 5CSEBA6U23I7, rtlCleanAtHead TRUE

  @phasefix-gate0   1382 reg   1445 combALUT   1505 estALM   1 DSP   9984 bits
  @phasefix-gate1   1448 reg   1514 combALUT   1539 estALM   1 DSP   9984 bits
  ----------------------------------------------------------------------------
  the WALK GATE      +66        +69             +34          +0      +0
```

**+66 REGISTERS IS EXACTLY 2 + 32 + 32** -- the three-state FSM plus the two
counters -- so the fitter confirms the arithmetic rather than merely agreeing
with it in magnitude.

**AND `virtualPins` IS IDENTICAL AT 1004 IN BOTH ROWS**, because `walk_active_i`
is a port in both builds and merely UNREAD at gate 0. That matters: it means the
delta is the gate's LOGIC and not a boundary artefact, which is the objection
CLAUDE.md raises against leaf rows and which usually cannot be answered.

**AGAINST THIS CAMPAIGN'S OWN REFUSAL BAR:** LANESCOST was refused at +11,979
ALUT / +9 DSP; a second setup+attrpack instance at +1,621 ALUT / +40 DSP; the
door was called affordable at +983 ALUT / +0 DSP. **+69 comb ALUT and +0 DSP is
an order of magnitude below the smallest thing this campaign has called
affordable.**

**READ THESE ROWS CORRECTLY, in three parts, because a map row invites three
misreadings:**

1. **They carry NO placed ALMs and NO Fmax.** `-MapOnly` stops after analysis
   and synthesis; `estimatedAlms` is A&S's own estimate. Nothing here is a
   timing claim, and Section 5 still stands: arrangement 1 as a whole has never
   been fitted.
2. **They are LABELLED rows, so `ruleViolations: []` is SILENCE, not
   compliance.** CLAUDE.md: labelled rows are never rule-checked.
3. **The `seconds` differ (123.9 against 82.9) and that is NOT a design
   signal** -- it is wall-clock under different machine load, with three other
   jobs running. Reading it as "the gate made it faster" would be exactly the
   kind of number this file warns about.

### And getting these rows is what found the console's Quartus defect

The first attempt at both returned `failed:analysis`, on errors in files I had
not touched. See Section 6.5 -- it is the most valuable thing in this FINDINGS
and it was found by running a routine receipt and refusing to wave off its red.

### A tool inconsistency found on the way, reported not fixed

The rows are named `zhao_post_leasephasefix-gate0` -- the label CONCATENATED
with no separator. `zhao_block_map.json` already holds BOTH conventions:
`zhao_forge_cliff@edge-split-wip` and `zhao_field_earth_adapterearthlock_after`.
One database, two spellings, so a reader grepping `module@label` silently misses
rows. Not mine to fix inside this packet, and named so the next reader does not
conclude a row is absent when it is merely spelled differently.

---

## 9. GATES AND RESULTS AT THE PUSHED COMMITS

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2** -- `I34`, `I55`; no higher than the 2 I started at |
| `check_console_inventory.py` | **OK** |
| `check_prod_manifest.py` | **OK** (409 modules, 89 tops) |
| `gen_prod_top.py --check` | **fresh (89 instances)** -- 89 before and after; see Section 6.4 |
| `gen_console_board.py --check` | **FRESH (1626 core ports)** -- METASIDE's 1624 plus my two |
| `gen_shell_paired_diff.py --check` | **fresh**, harness AND mutant |
| `check_quartus17_syntax.py` | **RC 0**, 672 files, self-test 13 fire / 22 no-fire -- **and see Section 6.5, it does NOT know the form that broke the console** |
| `check_case_labels.py` | **OK**, self-test 3 fire / 1 no-fire |
| `check_console_closure_lint.py` (gate 31) | **OK**, self-test fired 5/5 |
| `mutant_copy_drift.py` | **OK**, 80 copies -- run **AFTER** each commit (R121) |
| `zhao_post_lease` lint `-Wall`, `WALK_GATE=0` and `-GWALK_GATE=1` | **RC 0** both |
| `gen_shell_fit_ports_v2.py` | **RED, INHERITED since 2026-09-25** -- Section 6.3, reproduced at base |

### The console forms

| form | result |
|---|---|
| PLAIN (the SHIPPED, parked arrangement) | **PASS** -- `raster pixels=2816`, `frames_admitted=1`, `texture fragments=1216`, `tile[max/or]=[6 7]`, `setup_submitted=75`, sweep a STRUCTURAL ZERO, **and `walkphase phasehold=0 phasesweeps=0`** |
| `-WalkRaster` (arrangement 1) | **PASS** -- `raster pixels=2816`, `resolved_tiles=11`, `tilewalk tiles=11 jobs=101 door=101`, `paramwalk tris=101`, `phasehold=68203 phasesweeps=1` |
| `-MsMutFlat` / `-MsMutTail` / `-MsMutFrag` | **PASS**, all three INVERTED -- each assertion fired on its OWN message (Section 4) |

### The directed tests (RULING R60 -- BUILT and RUN at the pushed commit)

| target | checks |
|---|---|
| `post_lease_directed` (WALK_GATE=0, the control) | **39 passed** |
| `post_lease_walkgate` (`-GWALK_GATE=1`) | **41 passed** |
| `cmd_exec_directed` (R60's named target) | **977 passed** |

41 against 39 is the corroborating signal, not an accident: case 8's gated arm
carries four checks and its ungated arm two, so the totals differing says the
`ifdef` engaged on the **C++** side as well as in the RTL -- which a single
number cannot say. Both arms check `walk_gate_built_o` FIRST, so a `-G` that
silently failed to engage fails on the premise rather than passing while
measuring the other build.

### What is NOT covered, said plainly

* **`frame_admit_i` arriving during `WK_OWED`.** The gate returns to `WK_OFF`
  there, mirroring the pass sequencer's own rule that an admit abandons an
  arming. The smoke and case 8 each run ONE frame, so neither reaches it. An
  obviously-complete `case` statement is not a tested path.
* **The five other committed control forms in arrangement 1** -- Section 5.
* **Any fit.** Two leaf map rows are not a console area claim and this FINDINGS
  does not make one.

---

## 10. BRANCH AND COMMITS

**Branch `gz/phasefix`. Pushed. Never `--force`, never `--force-with-lease`.
Not merged to the integration branch.**

| commit | what |
|---|---|
| `6ff36f86` | `feat(PHASEFIX)`: I55's pixels had nowhere to land because `quiet` names the wrong producer |
| `736ebf5a` | `feat(PHASEFIX)`: the console DRAWS at GEOM_WALK_RASTER=1 -- 2,816 pixels, every byte through SDRAM |
| `80d2b979` | `docs(PHASEFIX)`: the FINDINGS -- and the bench's own wait order would have called the working console broken |
| `8e1ab9f6` | `fix(PHASEFIX)`: the production console does NOT pass quartus_map, and it is two import lines |
| `d9bcdb39` | `test(PHASEFIX)`: run the WHOLE post-lease suite against the gate, and my first version of it was wrong |
