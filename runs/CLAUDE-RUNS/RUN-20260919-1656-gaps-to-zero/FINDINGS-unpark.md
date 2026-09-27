# FINDINGS — UNPARK, 2026-09-27

Branch `gz/unpark`, worktree `C:\programmieren\zencrifice\gz-unpark`,
base `c93fe343`. Reported against the brief's nine numbered Deliverable items.

**The headline: `GEOM_WALK_RASTER` is UN-PARKED. The console ships at
arrangement 1** — the SDRAM walk feeds the live raster — **and the binner's
on-chip drain is retained as the complete oracle: buildable, asserted, and for
the first time gated by a ctest of its own.**

**Five control forms, five verdicts. Four passed unchanged. One failed, and not
on the number anyone expected** — not on pixels, which were exactly right, but on
*triangles*. **And two of the five were asserting nothing whatever about the
arrangement**, which is the finding I would most want inherited.

---

## 1. THE FIVE CONTROL FORMS IN ARRANGEMENT 1 (Deliverable 1)

Measured through `-WalkRaster` on the pre-flip tree, so each form's own
assertions were judged against the arrangement without the default moving
underneath them.

| form | verdict | evidence |
|---|---|---|
| `-Mutant` | **CORRECT — and its number is NOT a pixel count** | `terr_pl_slot_overflow_o=1`, fired. **Arrangement-independent by construction:** it asserts a TERRAIN PAGE-LOADER counter, upstream of the whole raster, and ends the run with `disable run` ~850 lines *before* the pixel gate. It drew 2,560 through the walk (10 tiles, 36 jobs) and **nothing asserted that.** |
| `-BadVertex` | **CORRECT for arrangement 1** | `pixels=512` — its own reference-derived `SGF_EXP_TERR_TILES * 256`, hit exactly, **drawn by the walk out of SDRAM**: 2 tiles, 65 jobs, 65 door, arenabin agreeing at 65, `resolved_tiles=2`. TERRAIN ALONE, with the whole mesh batch refused. |
| `-NoEchoArm` | **CORRECT for arrangement 1** | `pixels=2816` via the main gate, identical to the plain arrangement-1 run. The phase hold does **not** disturb POST.ECHO's disarmed negative control — the interaction I went looking for and did not find. |
| `-BadTraceArm` | **CORRECT for arrangement 1** | `pixels=2816` via the main gate, identical again. |
| `-TerrainFlatLattice` | **ARRANGEMENT-DEPENDENT — needed a second expected value** | **FAILED.** Not on pixels: `pixels=2560` is exactly the mesh's ten tiles, and correct. On triangles: *"GEOM.SETUP took 50 and the MESH reference wants 14"* — and **50 = 14 + 36, where 36 is exactly `geom_pw_tris_o`.** |

### None of the five was STALE, and that distinction is the useful part

The brief expected numbers that "may be right or stale". **They cannot be stale
in the drift sense, because every one of them is REFERENCE-DERIVED.**
`SGF_EXP_PIXELS` (2816), `SGF_EXP_MESH_TILES` (10 -> 2560) and
`SGF_EXP_TERR_TILES` (2 -> 512) are generated into
`tests/prod/smoke_geom_fixture.svh` by `smoke_geom_fixture_gen.cpp` and
freshness-gated by ctest `smoke_geom_fixture_fresh`. A generated, gated constant
does not drift. It can only be **wrong for the arrangement** — which is a
different failure, and is exactly the one that occurred, once.

### The repair is a second TERM, not a second NUMBER

`triangles_submitted_o` is a **lifetime** counter, and at `GEOM_WALK_RASTER = 1`
GEOM.SETUP processes the live frame **and** the walk's triangles through the same
silicon — the time multiplex, this architecture's central bet. The clean verdict
path one `else` away had already been taught this by SWAPCLOSE
(`SGF_EXP_ACCEPTED + SGF_EXP_TERR_ACCEPTED + geom_pw_tris_o`); the
`-TerrainFlatLattice` arm had not.

So the check became `SGF_EXP_ACCEPTED + geom_pw_tris_o`, and **that is a
strengthening in both arrangements**:

* at arrangement 0, `geom_pw_tris_o` is **zero, and that zero is separately
  asserted** as part of the structural-zero verdict — so the check reduces
  character-for-character to the one that was there. **Proven, not argued:**
  `-TerrainFlatLattice -BinnerDrain` passes with `setup_submitted=14`.
* at arrangement 1 the walk's contribution is a **named term counted in a
  different module on a different register enable**, so a walk that silently
  dropped or duplicated a triangle now fails *here* as well as at the raster
  door.

What the control exists for is untouched: TERRAIN must contribute **none**, and
`SGF_EXP_ACCEPTED` still carries no terrain term.

### AND TWO OF THE FIVE ASSERTED NOTHING ABOUT THE ARRANGEMENT AT ALL

The arrangement verdict — structural zero at 0, `phasesweeps == 1` and
`phasehold != 0` at 1, `jobs == door`, `vread == 3 x tris` — sat **inline** at the
end of the CLEAN verdict. That put it inside the `else` arm of
`ZHAO_SMOKE_BAD_VERTEX` and ~2,300 lines below `-Mutant`'s `disable run`:

* **`-BadVertex` compiled it out.** In arrangement 1 it sweeps 2 tiles and issues
  65 jobs — and checked none of it. It is the form where this mattered most:
  with the mesh batch refused, what the walk draws is **terrain alone**, so it is
  the only run in which the sweep's output is separated from the mesh's by the
  control itself.
* **`-Mutant` left before it**, via `disable run`. In arrangement 1 it sweeps 10
  tiles and issues 36 jobs — and checked none of that either.

**Two of the six gated forms ran a real sweep that no assertion ever looked at.
Nothing was failing and nothing could have** — CLAUDE.md's *"a gate that cannot
reach the state is not evidence about the state"*, in its quietest form.

It is now a task, `check_walk_arrangement`, called from **all three** verdict
paths and **before** each prints its PASS line. A task rather than three pasted
copies, because copies drift and the one that drifts is the copy in the form
nobody runs by hand. It contains no `disable` and no `$finish`, so each caller
keeps its own ending and `-Mutant`'s verdict is still the last thing that form
says. Each new assertion was **measured before being asserted**: `-Mutant`
jobs=36 door=36 tris=36 sweeps=1; `-BadVertex` jobs=65 door=65 tris=65 sweeps=1.

---

## 2. UN-PARKED (Deliverable 2)

`fpga/rtl/prod/zhao_console_core.sv`: the `ifdef` polarity is inverted. The
shipped default is **1**; `ZHAO_CONSOLE_BINNER_DRAIN` selects **0**. It remains a
named, editable constant — directive section 0 forbids removing the owner's
control in the name of fidelity.

**Why it is not a judgement call.** Directive section 4: *"a parallel legacy
on-chip frame arena that still supplies the actual pixels is not closure."* At 0
the drain supplies every pixel, so **0 is an arrangement in which this entry can
never close**, whatever else gets built.

**Where section 4's own test is met**, since the brief asked me to say where: at
arrangement 1 the door is the only path into `zhao_raster_tile_pipe_v2`, and the
triangles it carries were decoded by `zhao_geom_paramwalk` from descriptors and
ProjectedVertex records fetched through the real guard, arbiter and controller —
`paramwalk chunks=15 stale=0 illegal=0`, `fetcharm vread=303 vbad=0 pvsplit=0`,
`vread = 3 x 101` exactly. Six counters in six modules on six independent
register enables agree at **101**.

### The objection that stopped three packets cannot discriminate

Three refusals called the flip "an unmeasured claim about the tightest budget in
the design". The budget is real and it **cannot decide between the
arrangements**. The console needs **293,352 ALUTs against 83,820 (350%)** in
*either* arrangement — it must remove **209,532** — and the measured leaf delta
between the two, on the shipping part `5CSEBA6U23I7`, both rows
`rtlCleanAtHead: true`:

| `zhao_geom_bin_pipe_v2` | `@doorcost-src0` | `@doorcost-src1walk` | delta |
|---|---|---|---|
| comb ALUT | 36,666 | 37,655 | **+989** |
| est. ALM | 28,909 | 29,566 | +657 |
| registers | 38,368 | 36,489 | **-1,879** |
| memory bits | 892,204 | 285,612 | **-606,592** |
| DSP | 89 | 89 | **+0** |

plus `zhao_post_lease` `@phasefix-gate0` -> `@phasefix-gate1`: **+69 comb ALUT,
+66 registers, +0 DSP**.

**So arrangement 1 costs +1,058 comb ALUT and buys back 1,813 registers and
606,592 memory bits, for zero DSP. +1,058 against 209,532 to remove is 0.5%** —
and the DSP wall, which this campaign has moved by 6 in its entire lifetime, is
untouched. **0.5% does not move 350%.** A fit cannot answer *which arrangement*;
it can only answer *how far out*, and that answer is the same either way.

**Read those rows correctly, in three parts.** They are `map_only`, so they carry
**no placed ALM and no Fmax** — `estimatedAlms` is A&S's own estimate. They are
**labelled**, so `ruleViolations: []` on them is **silence, not compliance**. And
the database shows its own repeatability floor: `@doorcost-base` and
`@doorcost-src0` are **the same design** and differ by **27 est. ALM** — so `+34`
est. ALM on `zhao_post_lease` is *too small to distinguish from noise*, and its
**+66 registers = 2 + 32 + 32** (the FSM plus two counters) is the number there
that means something. **I ran no Quartus and make no area or timing claim.**

### The throughput cost, charged rather than absorbed

The post phase is held **68,203 gpu clocks** — **66,690** of sweep plus a
**1,513**-clock `frame_end`-to-publication gap — on a frame of roughly 730,000
clocks. About **+9%**. Directive section 7 asks for that to be reported rather
than discovered; it is reported here and in the constant's own comment.

---

## 3. THE REGISTER, MEASURED BARE (Deliverable 3)

| | value |
|---|---|
| at base `c93fe343` | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |
| at my last pushed commit | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |

`python tools/budget/completion_register.py`, bare, reading python's own exit
code (RC 1, normal while gaps remain).

**`I55` DID NOT CLOSE IN THE REGISTER, AND THAT IS DELIBERATE.** The one string
that removes an entry from the count is `NOT a tie-off` in its head line, and my
brief forbade me that line in terms. The machine is now behind it — but the
instruction was not *"write the line if you believe the machine is there"*, it
was *"do not touch it"*. So the head line is **untouched**, verified after every
edit, and the entry carries a new section stating exactly what shipped and what
it rests on. **Writing that line is the coordinator's call; the evidence for it
is assembled.**

Nothing was closed by removing, narrowing, stubbing, tying off or disconnecting.
The binner's drain is untouched as a datapath: `git diff` against
`zhao_geom_binner_v2.sv` and `zhao_geom_bin_pipe_v2.sv` is **empty**.

---

## 4. BOTH ARRANGEMENTS ASSERTED, GATE PROVEN ABSENT (Deliverable 4)

PHASEFIX's property is kept **and is now stronger**, because the flip would
otherwise have cost coverage — the one thing an un-park must not do.

At **arrangement 0** (`-BinnerDrain`): `tilewalk tiles=0 jobs=0 door=0`,
`paramwalk tris=0 vread=0`, and **`walkphase phasehold=0 phasesweeps=0`** — the
walk gate **proven absent rather than assumed absent**: at `WALK_GATE = 0` the
generate branch is not elaborated and both counters are tied literals.

At **arrangement 1**: `phasesweeps == 1` **and** `phasehold != 0`, both asserted,
because either alone is satisfiable by a broken gate — one that never engaged
reads 0 sweeps, one that released on the clock it armed reads 0 held clocks while
drawing a correct picture only by luck.

**AND ARRANGEMENT 1 NOW HAS A CTEST, WHICH IT NEVER HAD.** Before the flip,
arrangement 1 was reachable only through `-WalkRaster`, which **no ctest ran** —
so of the two arrangements exactly one was gated in CI, and it was whichever the
constant happened to name. Flipping the constant alone would merely have moved
the *ungated* arrangement from 1 to 0, and the ungated one would then have been
**the complete oracle section 7 requires be retained**. *An oracle nobody runs is
not retained, it is merely present.* So `console_core_smoke_binner_drain` is
registered beside the plain `console_core_smoke`. **Two arrangements, two
ctests — one more than was true while the constant read 0.**

---

## 5. THE SHIPPED CONSOLE UNREGRESSED (Deliverable 5)

See section 9. The shipped arrangement draws the reference-derived **2,816
pixels** across 11 tiles with `frames_admitted=1`; the retained oracle draws the
same 2,816 from the other producer. **The same picture from two producers, which
is the only form of "it works" worth having.**

### AND THE UN-PARK CHANGED NO DOCUMENTED GATE VALUE, which is worth stating

`PACKET-PROTOCOL.md`'s gate table requires the plain run at
**`raster pixels=2816`, `frames_admitted=1`**, `-TerrainFlatLattice` at the
mesh's **2,560** and `-BadVertex` at **512**. **Every one of those still holds,
unchanged, at the newly shipped arrangement** — measured above. The table needs
no amendment for this flip.

That is the cleanest statement of what un-parking did and did not do: it changed
**which silicon produces the picture** and **nothing about the picture**. The
only expected value anywhere that had to move was the *triangle* count inside
`-TerrainFlatLattice`, and it moved by gaining the walk's own term rather than a
new literal (section 1).

---

## 6. CLAIMS FOUND FALSE (Deliverable 6)

### 6.1 — THE ENTRY ITSELF WAS FALSE, AND HAD BEEN FOR A DAY

`I55`'s own last section read:

> *"SO THE CONSOLE SHIPS PARKED ... At 1 this console renders ZERO PIXELS, and a
> machine that draws nothing is not mergeable."*

PHASEFIX repaired that on 2026-09-27 and **never came back to the prose**, so the
entry went on asserting zero pixels while the tree it sits in drew 2,816 of them.
My brief told me to read the entry "as currently written" because its account had
been rewritten twice and both earlier versions were wrong. **The third version
was wrong too — and in the most consequential way available, because it was the
paragraph that justified the park.** Corrected in this packet's own section.

CLAUDE.md's *"a document cannot go stale loudly"*, one level in from where that
chapter puts it: not a rules file describing a repaired defect, but an entry
describing its own subject.

### 6.2 — "Five committed control forms assert pixel counts" — FALSE for `-Mutant`

`-Mutant` asserts **no pixel count at all**. It reaches its verdict at the
terrain page loader and leaves via `$finish; disable run;` roughly 850 lines
before the pixel gate. What it asserts is `terr_pl_slot_overflow_o != 0`, a
counter upstream of the entire raster — arrangement-independent by construction.
It *prints* a pixel count, which nothing reads.

### 6.3 — "pixel counts taken against the BINNER'S DRAIN" — FALSE

They are **reference-derived**, not read off the drain. This matters because it
changes the *kind* of doubt: this campaign's instinct is to treat "never
re-measured" as "probably stale", and a generated, gated number cannot drift. It
can only be wrong for the arrangement — **and the one that was is a triangle
count, not any pixel count.**

### 6.4 — "Arrangement 1 has never been fitted" — TRUE, implication FALSE

I did not fit it. But the implication carried by three refusals — that a fit
stands between the arrangement and shipping — is false, for the arithmetic in
section 2: the console misses by **350%** in both arrangements and they differ by
**0.5%**. **A fit that cannot distinguish two options is not a gate on choosing
between them.** This is CLAUDE.md's refusal chapter exactly: each refusal was
correct about what it refused and wrong about what that implied.

### 6.5 — The brief's scope: five forms, but the flip moves SIXTEEN

`run_console_core_smoke.ps1` carries **15 behaviour-selecting switches** plus the
plain run. Un-parking changes the arrangement for **all sixteen**, not the five
the brief names. The five are exactly the gated ones minus plain, so the scope is
*defensible* rather than wrong — but the exposure is named rather than absorbed:
of the ten non-gate forms, `-FieldActive` and `-FieldUncovered` belong to the
concurrent MATFIELD packet and were deliberately not touched.

### 6.6 — `console_core_tieoff_audit` is RED, and it is INHERITED

`python tools/design/packet_h_tieoff_audit.py fpga/rtl/prod/zhao_console_core.sv`
exits **1** on two SILENT literal connections
(`u_cliff_lat_share.poison_value_i`, `u_terrain_veljoin.sweeps_aborted_o`).
**Verified inherited rather than assumed:** the same tool on the same file at base
`c93fe343`, in the same session, reports the **same two names** and the
**identical** header — `9 declared, 2 reasoned, 19 by group comment, 2 SILENT`,
22 literal-connection rows on both sides. Neither line is one I touched.

### 6.7 — THE BRIEF WAS RIGHT AND THE ENTRY WAS WRONG, for once in that direction

The brief's do-not-regress list cites `geom_bin_pipe_v2_door` at **11,381**
checks. Entry `I55` says **11,378**. The measurement's own source —
`FINDINGS-doorcost.md`, from the packet that ran it — says **11,381** in four
separate places, and so does `TASK_LOG.md` twice. **So the entry carries a
transcribed number that is three low, and the brief carries the measured one.**

I did not re-run the test (it needs a build tree this packet deliberately did not
stand up, §9), so I am **not editing either figure**: replacing a number I have
not measured with another number I have not measured is how this drift started.
It is named so the next reader does not average them. Provenance favours 11,381
decisively — six independent citations against one.

Worth recording because this campaign's habit is to assume the brief is the
unreliable document. Here the brief is the accurate one, and the entry — the
thing packets quote as authoritative — is the copy that drifted.

---

## 7. WHAT I REFUSED, AND WHAT I GOT WRONG (Deliverable 7)

### Refused

* **Touching `I55`'s head line.** Section 3.
* **Retiring the binner's drain.** Section 7 requires the complete oracle.
  Un-parking makes retirement *discussable*; it does not perform it. Both binner
  files are byte-identical to base.
* **Any Quartus run.** The brief forbade a console fit, and `-MapOnly` would have
  bought a number the database already holds (8.1).
* **Keeping `-WalkRaster` as a no-op alias.** It would define a macro nothing
  reads, and CLAUDE.md is explicit that an inert directive "reads as a
  guarantee". Removed, so it now fails loudly against the script's own param
  check and whoever types it reads the paragraph that replaced it.
* **Running the two field forms.** MATFIELD owns that stimulus and is live.

### Got wrong and caught myself

1. **I edited the bench while five of my own runs were verilating it** —
   CLAUDE.md's *"a suite reads the LIVE TREE"*, the exact rule on the exact file.
   I **discarded all five** and re-ran after the tree settled; nothing from them
   is quoted. The tell was noticing the edit timestamps straddled the launch, not
   any failure — **they would have produced numbers, and the numbers would have
   looked fine.**
2. **My log capture was silently throwing away almost everything.** I wrote
   `2>&1 | Out-File`; the smoke script says nearly everything through
   `Write-Host`, which writes to the **Information** stream and never enters the
   pipeline. A `-LintOnly` run, which exits before the simulator, wrote a
   **zero-byte log while returning RC=0**. Ruling R82 arriving through the
   redirection operator: *the run that most needs its log is the one that never
   reaches the simulator.* Fixed to `*>&1`.
3. **I named a variable `$args`** — a PowerShell **automatic** variable. Five runs
   wrote zero-byte logs *while recording RC=0*. Caught by reading the log
   **sizes**, which is why the runner now prints `bytes=` beside every RC.
4. **I hit the documented heredoc trap** on quote-heavy Python into `bash -c`,
   for the **sixth** packet running. The remedy — write the file, then run it —
   worked first time. The count is the finding.
5. **My first instinct on the `-TerrainFlatLattice` red was that the pixel count
   was wrong.** It was not; 2,560 is exactly right. Reading the assertion text
   rather than the headline showed the failing quantity was *triangles*. Had I
   "fixed" a pixel number I would have weakened a correct check to accommodate a
   correct machine.

---

## 8. WHAT THE FULL CONSOLE FIT NEEDS TO KNOW (Deliverable 8)

### 8.1 — Neither arrangement-1-only branch is new silicon to the mapper

Both have been through `quartus_map` **standalone, on the shipping part, with
`rtlCleanAtHead: true`**: `zhao_geom_bin_pipe_v2` at `JOB_SRC = 1`
(`@doorcost-src1walk`) and `zhao_post_lease` at `WALK_GATE = 1`
(`@phasefix-gate1`). **So an analysis-and-synthesis failure at arrangement 1
would be a COMPOSITION fault, not a leaf one** — that narrows the search before
it starts.

### 8.2 — The block that does not fit is common to BOTH arrangements

`zhao_geom_arenabin`: **146,414 registers**, because a `14 x 576 x 18` staging
array **declared inside a `generate` did not infer as memory** and went to
flip-flops. At four registers per ALM that is ~**87% of the device's register
sites for one block**, against `zhao_geom_binner_v2`'s 2,109. `(* ramstyle *)`
was tried twice and changed nothing, with no Quartus warning either time.
**This is the first thing the fit should look at, and it is not the
arrangement's** — un-parking neither created nor worsened it. Hoisting those
banks to module scope is the named next experiment; fourteen banks at 576 x 18
would be **2 M10K each, 28 of the device's 553**.

### 8.3 — The arrangement's own cost, and where it is not

**+1,058 comb ALUT, -1,813 registers, -606,592 memory bits, +0 DSP** (section 2).
An order of magnitude below the smallest thing this campaign has called
affordable, and **not where the 209,532 ALUTs are**.

### 8.4 — Read the leaf database's noise floor before quoting a small delta

`@doorcost-base` and `@doorcost-src0` are the **same design** and differ by **27
est. ALM**. Any leaf delta near that is noise.

### 8.5 — Throughput, which a fit does not measure and will be asked about

**+68,203 gpu clocks** of post-phase hold per frame, ~+9%. The walk's consumer
side runs **7.18x** the clocks per tile reference the on-chip drain does (29.58
vs 4.12, `geom_arenabin_price`). That is the price of section 4's mandate, not a
defect, and this packet did not move it.

### 8.6 — One residual hazard in the gate that now ships

`zhao_post_lease`'s walk gate leaves `WK_OWED` only when `walk_active_i` rises.
**A frame that armed the gate and never swept would hold the post phase forever**
and the console would stop publishing. It is not reachable on any stimulus I ran:
GEOM.TILEWALK's sweep is started by the **publication edge** and walks the whole
576-entry directory regardless of content (measured: `-BadVertex` sweeps 574
empty tiles of 576), so a zero-geometry frame still sweeps and still completes.
But **the gate's safety rests on "publication always follows `frame_end`"**,
which is a property of another module, and nothing asserts the conjunction.
`frame_admit_i` arriving during `WK_OWED` is also still untested — the smoke runs
one frame. Both were true before this packet; **un-parking makes them shipped
rather than parked**, which is why they are named here.

---

## 9. GATES AND RESULTS (Deliverable 9)

### The console forms — SHIPPED arrangement 1 (no switch)

| form | result | pixels | tilewalk | walkphase |
|---|---|---|---|---|
| plain | **PASS** | **2816** | tiles=11 jobs=101 door=101 | hold=68203 sweeps=1 |
| `-Mutant` | **PASS** | 2560 (ungated) | tiles=10 jobs=36 door=36 | hold=49504 sweeps=1 |
| `-BadVertex` | **PASS** | **512** | tiles=2 jobs=65 door=65 | hold=22118 sweeps=1 |
| `-NoEchoArm` | **PASS** | **2816** | tiles=11 jobs=101 | hold=68203 sweeps=1 |
| `-BadTraceArm` | **PASS** | **2816** | tiles=11 jobs=101 | hold=68203 sweeps=1 |
| `-TerrainFlatLattice` | **PASS** | **2560** | tiles=10 jobs=36 | hold=49414 sweeps=1 |

`setup_submitted` = 176 (75 live + 101 walk) plain; 50 (14 + 36) flat-lattice.

### The console forms — RETAINED ORACLE, arrangement 0 (`-BinnerDrain`)

| form | result | pixels | tilewalk | walkphase |
|---|---|---|---|---|
| plain | **PASS** | **2816**, resolved_tiles=11 | **0 / 0 / 0** | **hold=0 sweeps=0** |
| `-TerrainFlatLattice` | **PASS** | **2560**, setup=14 | **0 / 0 / 0** | **hold=0 sweeps=0** |

### The static gates

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2** — `I34`, `I55`; no higher than the 2 I started at |
| `check_console_inventory.py` | **OK** |
| `check_prod_manifest.py` | **OK** |
| `gen_prod_top.py --check` | **fresh (89 instances)** |
| `gen_console_board.py --check` | **FRESH (1626 core ports, 127 parameters)** |
| `gen_shell_paired_diff.py --check` | **fresh**, harness and mutant |
| `check_quartus17_syntax.py` | **RC 0**, 673 files, self-test 13 fire / 22 no-fire |
| `check_case_labels.py` | **OK** |
| `check_entry_claims.py` | **OK** |
| `check_localparam_comments.py` | **OK** (at `tools/design/`) |
| `check_findings_citations.py` | **OK**, no NEW dangling citations |
| `mutant_copy_drift.py` | **OK**, 80 copies — run **AFTER** each commit (R121) |
| `packet_h_tieoff_audit.py` | **RED, INHERITED** — reproduced identically at base, 6.6 |

### What is NOT covered, said plainly

* **The directed tests named in the brief's do-not-regress list were not built
  and run**, and that is a declared gap rather than an estimate. The reason they
  are not at risk is structural and was checked rather than assumed: my only RTL
  change is one `localparam` default in `zhao_console_core.sv`, and **no
  directed-test target compiles that file** — the sole non-smoke consumer is
  `console_core_tieoff_audit`, a Python audit of its header, which I ran (6.6).
  The bench I changed, `tb_zhao_console_core_smoke.sv`, is built by
  `run_console_core_smoke.ps1` **and nothing else**, so the eight console forms
  above *are* its directed test. A full configure-and-build was judged a poor use
  of a machine carrying eight live console runs and a concurrent packet.
* **Ten of the sixteen committed smoke forms** in the shipped arrangement. The
  six in the gate table are all measured; `-FieldActive` and `-FieldUncovered`
  are MATFIELD's.
* **Any fit.** No Quartus was run and no area or timing claim is made.

---

## 10. BRANCH AND COMMITS

**Branch `gz/unpark`. Pushed. Never `--force`, never `--force-with-lease`. Not
merged to the integration branch.**

| commit | what |
|---|---|
| `c40589ca` | `test(UNPARK)`: the five control forms could not be run in arrangement 1 -- the tag chain could only express five of ten |
| `3088cbbc` | `test(UNPARK)`: one of the five controls was ARRANGEMENT-DEPENDENT and two were asserting nothing at all |
| `9b75a9a0` | `feat(UNPARK)`: THE CONSOLE SHIPS AT ARRANGEMENT 1 -- and the oracle is gated for the first time |

### Reproducing every number above

The probe is the committed smoke script; there is no throwaway tooling behind
any figure here. From PowerShell with `tools\env\zhao-env.ps1` sourced:

```
tests\prod\run_console_core_smoke.ps1                                  # SHIPPED, arrangement 1
tests\prod\run_console_core_smoke.ps1 -BinnerDrain                     # the retained oracle, arrangement 0
tests\prod\run_console_core_smoke.ps1 -Mutant                          # ... and each control form,
tests\prod\run_console_core_smoke.ps1 -BadVertex                       #     in either arrangement,
tests\prod\run_console_core_smoke.ps1 -NoEchoArm                       #     by adding -BinnerDrain
tests\prod\run_console_core_smoke.ps1 -BadTraceArm
tests\prod\run_console_core_smoke.ps1 -TerrainFlatLattice
```

Every form now prints `build dir:` as its second line, so a reader can difference
two logs and **see** they were built apart instead of reading the tag chain and
trusting it.

---

## 11. ONE RECOMMENDED AMENDMENT TO `PACKET-PROTOCOL.md`, NOT MADE HERE

That file is the coordinator's, so this is a recommendation rather than an edit.

**Ruling R82 says `... 2>&1 | Out-File -Encoding utf8 <log>` and read the log.
That recipe silently discards almost everything these scripts print.**
`run_console_core_smoke.ps1` says nearly all of it through `Write-Host`, which
writes to the **Information** stream and **never enters the pipeline** — so
`2>&1` captures only the simulator's native stdout. A `-LintOnly` run, which
exits before the simulator, produces a **zero-byte log under RC=0**.

The fix is one character: **`*>&1`**, not `2>&1`.

This is R82's own subject arriving through the recipe written to enforce it —
the same shape as the `[IO.File]` entry in CLAUDE.md, where advice that is
correct in substance aims packets at the wrong place when followed literally.
I lost two forms' logs to it before noticing, and the tell was a **log size**,
not a failure.
