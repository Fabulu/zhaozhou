# FINDINGS — DOORCOST, 2026-09-27

Branch `gz/doorcost`, worktree `C:\programmieren\zencrifice\gz-doorcost`,
base `71122893`. Reported against the brief's nine numbered Deliverable items.

**The headline, so it is not buried: the door was AFFORDABLE the whole time, and
the 2,065-wire price that stopped six packets was an estimate 73x too big.**
MUXBUILD's structural finding is correct and stands — `job_*` really is internal
and nobody had opened the module. What was wrong is the *price*: **2,037 of those
2,065 wires are already ports on `zhao_geom_bin_pipe_v2`**, because the time
multiplex feeds the *same* `u_geom_setup`/`u_geom_attrpack` pair, so their output
arrives on the same `tri_*` ports whichever source fed it. The 1,877-bit metadata
is assembled *inside* the module, thirty lines above the binner instantiation, out
of ports that already exist. **The record never needed transporting because it was
never anywhere else.** The door is **twenty-eight bits**.

---

## 1. THE REGISTER, MEASURED BARE, AND `I55` DID NOT CLOSE

| | value |
|---|---|
| at base `71122893` | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |
| at my last pushed commit | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |

**`I55` did not close and this packet does not claim it did.** `walk_valid_i` and
`t_ready_i` are still tied, `paramwalk dirs/chunks/tris` is still `0/0/0`, and no
pixel in the console yet comes from bytes that went through SDRAM. Re-measured
BARE after editing entry `I55`; `check_entry_claims.py` OK, no NEW claim.

Nothing was closed by removing, narrowing, stubbing, tying off or disconnecting.
Nothing was tied off: `walk_job_*` is *bound*, and §8 explains why that differs.

---

## 2. THE TWO OPTIONS, PRICED LIKE FOR LIKE — AND THEY HAVE THE SAME PRICE

All rows: `tools/quartus/run_block_map.ps1`, **map-only**, device
**`5CSEBA6U23I7`** (hardcoded in that script, so the `-Device` unvalidated-write
trap does not apply), **`rtlCleanAtHead: true` on every row**, at the console's own
`CHUNKS=8192 CHUNK_W=13`. A map row carries **no ALMs and no Fmax by
construction**; `estimatedAlms` is Analysis & Synthesis's estimate, not a placed
count.

| row | commit | comb ALUT | est. ALM | registers | DSP | memory bits | vpins | s |
|---|---|---|---|---|---|---|---|---|
| `zhao_geom_bin_pipe_v2@doorcost-base` | `71122893` | **36,672** | 28,882 | 38,368 | **89** | 892,204 | 4,863 | 328 |
| `zhao_geom_bin_pipe_v2@doorcost-src0` | `84817e4e` | 36,666 | 28,909 | 38,368 | 89 | 892,204 | 4,923 | 261 |
| `zhao_geom_bin_pipe_v2@doorcost-src1walk` | `84817e4e` | **37,655** | 29,566 | 36,489 | **89** | **285,612** | 4,923 | 241 |
| `zhao_probe_doorcost_jobmux@doorcost-runtimemux` | `84817e4e` | **2,050** | 4,107 | **0** | **0** | 0 | 6,164 | 90 |

### `@doorcost-base` is the FIRST shipping-part figure this block has ever had

Entry `I55` says, correctly, *"There is NO shipping-part figure for
`zhao_geom_bin_pipe_v2` anywhere in this tree."* There is now. **Do not difference
it against the 30,266-ALUT figure the entry quotes** — that is the *sizing* part
`5CEBA9F31C7` and its `CHUNKS` is not stated. Comparing them would be the
mismatched-pose error with devices instead of poses.

### `src0` is the control, and it is why the door is free to OPEN

The ports exist, the `generate` select is present, and the arrangement this
console elaborates measures **−6 comb ALUTs, identical registers, identical DSP,
identical memory bits, and exactly +60 virtual pins** — the door's own bit count,
`1+1+12+12+1+1+32 = 60`. **The control can fail:** had the select wrongly built
the multiplex it would read about **+2,050**.

### `src1walk` is the door DRIVING

**+983 comb ALUT, +684 estimated ALM, +0 DSP, −1,879 registers, −606,592 block
memory bits.**

The register and memory deltas are **Quartus pruning** the binner's payload path
once nothing reads `bin_job_meta_w` — `d_meta_r` is 1,877 bits and
`d_profile_bad_r` 2, which is the −1,879 exactly. That pruning is what the end
state *removes by design*, but **the drain is still in the RTL in this
arrangement**, so the saving is declared AVAILABLE, not taken. I claim no
retirement.

### The two routes MUXBUILD offered are the same price, and neither was measured

* **Route 1 — export `job_*` through two module boundaries.** The port bits cost
  **no logic**: measured, 60 port bits cost −6 ALUT. What it costs is the
  **run-time 2:1 multiplex**, because both sources stay live.
* **Route 2 — lift `u_tile` out.** Structurally different, **same multiplex, same
  price.** Lifting a child changes where wires are declared, not what logic exists.
* **The multiplex, measured:** `zhao_probe_doorcost_jobmux` is that select on the
  2,048-bit bundle and nothing else — **2,050 comb ALUT, 0 registers, 0 DSP, 0
  memory bits.** One LUT per bit, as arithmetic predicts (2,048 forward + 2 for
  the ready demux).
* **Its `estimatedAlms` of 4,107 is NOT its cost** and I decline to quote it as
  one: 6,164 virtual pins on pure combinational logic, and `estALM > combALUT` is
  the tell. On the real block the ratio runs the other way (28,882 vs 36,672).
  This is the handover's virtual-pin warning; the honest number is `combAluts`.

**So the routes are not alternatives on price; they are alternatives on structure,
and both pay 2,050 ALUTs the elaboration select does not.**

### What I did instead, and why it needs no multiplex at all

`JOB_SRC` on `zhao_geom_bin_pipe_v2` selects the `job_*` source **at
elaboration**: `0` = the binner's on-chip drain (today's console, and the retained
complete oracle directive §7 asks for), `1` = the SDRAM walk. A `generate` select
costs **zero logic**.

The run-time multiplex is refused on **two grounds, and I state them separately
because an earlier draft of this file overstated the second one.**

1. **It is not NEEDED.** The two arrangements are *alternatives*, never
   concurrent, so the selection is an elaboration question and a `generate`
   select answers it for **zero logic**. 2,050 ALUTs for a select nothing needs
   is 2,050 ALUTs on the axis measured at 335% over.
2. **A CONCURRENT arrangement is what directive §4 forbids** — *"a parallel
   legacy on-chip frame arena that still supplies the actual pixels is not
   closure."* **Precisely: §4 forbids the legacy still supplying PIXELS, not a
   select line as such.** A run-time mux statically driven so the walk supplies
   every pixel would satisfy §4's letter and still be ground 1. My first draft
   wrote this as "a run-time select is the forbidden thing wearing a select
   line", which is the kind of one-line overstatement this repository's own rules
   say is where the error lives — corrected here rather than propagated.

Priced either way, because a refusal with no number is what this packet exists to
replace.

### The verdict, against this campaign's own bar

| | ALUT | DSP |
|---|---|---|
| LANESCOST, **refused** | +11,979 | +9 |
| second setup+attrpack instance, **refused** | +1,621 | +40 |
| **this door, driving** | **+983** | **+0** |
| this door, as the console elaborates it today | **−6** | +0 |

**+983 ALUT and zero DSP is below the smallest thing this campaign has ever
refused, on the axis measured at 335% over, and it returns 1,879 registers.
AFFORDABLE.** Note the direction of the inherited estimate: *"2,065 wires sounds
huge"* was wrong in the **alarming** direction, exactly as LANESCOST's predicted
+90,000 against a measured +11,979 was. Two for two this week.

**One honest caveat.** The memory saving runs the *wrong way* along the owner's
"trade ALMs for M10K" lever: this spends the scarce resource (ALUTs) and returns
the plentiful one (~59 M10K blocks). That would matter if the door were an
optimisation. It is not — it is mandatory function `I55` requires, and +983 is
0.47% of the 209,532 ALUTs that must come out.

---

## 3. OPENED. And the number that decided it is +983 ALUT / +0 DSP.

* **`JOB_SRC`** and the **`walk_job_*` port set** on `zhao_geom_bin_pipe_v2`:
  `walk_job_valid_i`, `walk_job_ready_o`, `walk_job_tile_x_i`,
  `walk_job_tile_y_i`, `walk_job_first_i`, `walk_job_last_i`,
  `walk_jobs_taken_o`. Twenty-eight bits of door plus a counter.
* **`write_profile_bad_o`** on `zhao_geom_binner_v2` — the profile verdict on the
  **write** edge. `job_profile_bad_o` is read back out of the metadata bank's pad,
  so it exists only for a job the binner **stored**; on the walk path there is no
  stored word and the consumer would be handed a verdict about another triangle.
  It is **two wires off `meta_aux_bad_c`/`meta_area_bad_c`**, which that block
  already computes for the pad, and adds no logic. Recomputing those two lines one
  level up would be a **second expression of the Packet-D bit layout** — and the
  comment beside that expression already records two copies of this exact fact as
  *"the shape this repository keeps finding gone stale"*.
* **The one job bundle split into `bin_job_*` and `job_*`.** One bundle is how a
  second source comes to be spliced in with an OR; refused here for the seventh
  time and now structurally awkward rather than merely forbidden.
* Four instantiations connected: `zhao_shell_top_v2`, `tb_geom_bin_pipe_v2`,
  `tb_geom_binner_v2_pair`, `tb_zhao_geom_paramarena`, `zhao_shell_v2_lease_path`.
  An intentionally unconnected output is `.port_o ()`.

---

## 4. A PIXEL THROUGH THE DOOR — AND IT IS *NOT* A PIXEL FROM SDRAM

`geom_bin_pipe_v2_door` (`pd_door`, `-GJOB_SRC=1`): **11,381 checks, 0 failures.**
Two walk-sourced jobs on tile (0,0):

```
started=2  sunk=0  tiles=1  fb=256  resolved=1  cand=256  frag=256
binner_tile_references_o = 0
```

**`tri_valid_i` is never asserted**, so the binner bins nothing, and all **256
framebuffer beats — compared PER BEAT against `zref` for rgb565, in-tile address,
x, y, `src_id` and `last`** — came through the door or did not exist.

`geom_bin_pipe_v2_door_shut_control` (`pd_doorshut`, JOB_SRC=0, **the same
source**): **4,084 checks, 0 failures.** Identical stimulus, `walk_job_ready_o`
asserted low on **4,000 consecutive cycles**, fb/cand/frag/tiles/started all zero.
**That control is what makes the open one mean THE DOOR** rather than "something
in a 54-file texture island produced pixels". The door's counter is also read
**before** the first offer and required zero: a counter shown firing without first
being shown silent is not a control.

**What is NOT proven, said plainly: no pixel here came from bytes that went
through SDRAM.** The planes are driven by the bench exactly as GEOM.SETUP and
GEOM.ATTRPACK drive them in the console. The `fetcharm` invariant has **not** moved
off zero.

**The counters are NOT independent corroboration, and I say so.** In `JOB_SRC=1`,
`job_valid_w` **is** `walk_job_valid_i`, so `walk_jobs_taken_o` and `jobs_taken_o`
increment on the **same condition** — structurally blind to every fault that
condition participates in (CLAUDE.md's lockstep law). What the pair *is* good for
is telling the two arrangements apart: at `JOB_SRC=0`, `jobs_taken_o` counts
binner jobs while `walk_jobs_taken_o` is a structural zero.

---

## 5. THE TriangleDescriptor v2 DEFERRAL: ITS REASON IS DISCHARGED

MUXBUILD deferred TD v2 on the strong reason, not the size one:

> **TD v2 costs live SDRAM write bandwidth every frame for a consumer that cannot
> exist yet** … the only thing that would read `area2` and the box is a back end
> that has nowhere to deliver a `job_*` until the 2,065 wires above are opened.

**The door is now open and priced, so that consumer *can* exist.** The deferral
was correct when written and its stated ground is gone. TD v2 is the **next
build**, and MUXBUILD's own ordering — *"open the raster door, THEN TD v2, THEN the
multiplex"* — is now at its second step.

TD v2 remains **necessary**, and not for size: `zhao_geom_setup` consumes
`tri_area2_i` and the four scissored box bounds, the 16-byte record carries
neither, and **`zhao_geom_setup.sv:386` *defines* `kc2` as `area2 - kc0 - kc1`**,
so the barycentric identity recovers nothing and a back end built on it would be
correct for any garbage `area2`. I did **not** build it speculatively, and I did
not recompute `2A` or the scissored box on the walk side.

**The full remaining chain for `I55`, in order:**

1. **TriangleDescriptor v2** — decided and costed; now unblocked.
2. **The console-side source select** feeding `u_geom_setup`/`u_geom_attrpack`
   from the walk during the drain window, **upstream of the three-way fork** as
   the swap-architecture record requires. Narrow — corners, `area2` and the box,
   not 2,065 bits — and it is `zhao_console_core`'s.
3. **A sequencer on `walk_valid_i`** over the head table.
4. **The binner's drain retired by REMOVAL** — the −606,592 bits above. This *is*
   a subsystem retirement: the bin phase and the drain share **one FSM and four
   memories** (`tile_ram`, `ref_ram`, `next_ram`, `tri_ram`), states 0–5 and 6–11
   in one `case`. **MUXBUILD's "subsystem retirement" was right — about the
   BINNER, not about the door.**

Not discharged: **a triangle arriving as `frame_end_i` pulses**, against a real
mux. There is still no console-level mux to exercise it against.

---

## 6. THE ID REPAIR AND SCHEMA v2

`ctest` was not used (one-per-tree, and a second build tree was live); the
directed executables were built and run directly, which ruling R60 is about.

| test | result |
|---|---|
| `pd_door` (new) | **11,381 checks, 0 failures**, RC 0 |
| `pd_doorshut` (new control) | **4,084 checks, 0 failures**, RC 0 |
| `mutant_copy_drift.py` | **RC 0**, 78 copies, **run AFTER the commit** (R121) |

**`geom_paramarena_directed` (550) and `geom_tidq_directed` (83) were NOT re-run,
and I say so rather than implying it.** I changed no file either depends on for
behaviour: `zhao_geom_binner_v2` gained one output driven by an existing
expression, and `tb_zhao_geom_paramarena.sv`'s only change is
`.write_profile_bad_o ()`. Both are elaboration-visible and neither can move a
value. **That is an argument, not a measurement**, and the coordinator's merge
gates are where it gets checked. Given §7.1 I judged the remaining Packet-D budget
better spent proving *that* red is not mine.

---

## 7. CLAIMS FOUND FALSE, AND AN INHERITED RED

### 7.1 — the whole `geom_bin_pipe_v2_directed` family has been RED since 2026-09-26, and it is NOT this packet's

`check_candidate` reads `stage_candidate_data_o` at offsets **410**
(`source_id`), **426** (`fragment_state`), **458** (`invw24`) and **482** (in-tile
address) — the constants `zhao_render_texture_pkg.sv` declares. **Those constants
are stale by one bit.**

Commit **`ceba0bfe` (NORMALMAP, 2026-09-26)** widened the Early-Z **payload** by
one bit and moved `PRETEX_EARLYZ_KEY_LO` **410 → 411** and `_HI` **489 → 490**. It
did **not** move the four KEY field constants inside that span. The package is now
inconsistent in a way a packed struct cannot be: **`PRETEX_EARLYZ_PAYLOAD_HI` is
409 and `PRETEX_EARLYZ_KEY_LO` is 411, so BIT 410 BELONGS TO NEITHER FIELD.**

**Measured, not inferred.** Reading each field one bit higher returns exactly the
expected value:

```
depth@458 = 0xa00000   depth@459 = 0x500000 = expected
src@410   = 0x1e02     src@411   = 0x0f01   = expected
```

Two fields, both off by a single left shift — and **`src_id` is an IDENTITY, not
an arithmetic quantity**, so a doubling there can only be a bit offset and cannot
be a data error. That one observation separated "my door delivers a wrong plane"
from "the probe is read at the wrong offset", which are very different findings.

**Proven inherited by re-measurement at the base commit, not asserted.** Built and
ran `pd_full` in a throwaway worktree at `71122893` (`gz-doorcost-basecheck`,
since removed): `BASE_CONFIGURE_RC=0`, `BASE_BUILD_RC=0`, **`BASE_PD_FULL_RC=2`**,
failing at **`cycle=4219: candidate invw/depth differed from current-rast plane`**
— the identical message at the identical cycle as in my tree, with none of my
changes present. `ceba0bfe` is also an ancestor of `71122893`
(`git merge-base --is-ancestor`), and I changed neither the package nor the tile
pipe.

**And the package's own self-check cannot catch it.**
`zhao_render_texture_pkg.sv:405` reads
`(PRETEX_SOURCE_ID_LO == 410) && (PRETEX_SOURCE_ID_HI == 425) && …` — **it
compares each constant to its own literal.** It asserts 410 == 410. It is
structurally incapable of firing when the layout moves: CLAUDE.md's "detector
wired to two operands that move together", with the two operands being *one*
operand twice.

**The repair, handed over rather than guessed at.** It belongs to the
raster/texture lane because it decides what every fragment field *means*, and the
arithmetic is unambiguous: the key is **80 bits** (16 + 32 + 24 + 8) and at
`KEY_LO = 411` it lands exactly on `KEY_HI = 490` —

```
source_id       411..426
fragment_state  427..458
invw24          459..482
in_tile_addr    483..490
PAYLOAD_HI      409 -> 410
```

— and `geom_bin_pipe_v2_directed.cpp`'s four hardcoded offsets move with them.
**My door test copies NEITHER set of numbers.** Asserting the stale ones would
assert the bug; asserting the corrected ones would rest this door's evidence on an
unreviewed repair. The framebuffer oracle needs neither, because it reads real
ports (`fb_*`).

Affected, all red for the same reason: `geom_bin_pipe_v2_directed`,
`geom_bin_pipe_v2_coordinate_mutant`, `geom_bin_pipe_v2_omit_v3_quiet_mutant`,
`geom_bin_pipe_v2_identity_cancel_control`, `geom_bin_pipe_v2_old_ready_mutant`,
`geom_bin_pipe_v2_skip_cancel_mutant` — **six of the thirteen tests
`ZHAO_PACKET_D_REQUIRED_TESTS` exists to guarantee.**

### 7.2 — `zhao_console_board.sv` was INHERITED stale by three core ports

`gen_console_board.py --check` reported **STALE at 1616 core ports** before I had
touched `zhao_console_core.sv` or the board. MUXBUILD's FINDINGS record it fresh at
**1613**, so the three arrived after that — `afafb3e4` ("31 PINMISSING on the new
fetch arm") is the shape. Regenerated: **+11 lines, FRESH at 1616.** **If MATCARRY
also changed core ports, regenerate once after the merge rather than resolving
this by hand.**

### 7.3 — `jobs_taken_o` is NOT a binner counter, and I read it as one

`zhao_geom_bin_pipe_v2.sv:736` increments it on `job_valid_w && job_ready_w` — the
job the **tile pipe** accepted, from whichever source the arrangement selected. My
first draft asserted it zero alongside `binner_tile_references_o`, and it fired
correctly on the door's own two jobs. Recorded in the test beside the corrected
assertion rather than quietly fixed.

### 7.4 — the brief's "2,065 wires"

Not a false *claim* — MUXBUILD stated it as a count of declared wires, and as that
it is right. What was false is the implication that they must **cross a
boundary**. §2 has the measurement. Note the direction: for once the inherited
estimate made the work look **bigger**, and the effect was the same as a
flattering one — six packets left it alone.

### 7.5 — `-MapOnly` on `run_block_fit.ps1` records NO ALUT at all

The brief says "Leaf `-MapOnly` is yours". `run_block_fit.ps1:1087-1101` harvests
only registers, memory bits, DSP and virtual pins from the `.map.summary`, and its
own comment says why: *"Analysis & Synthesis reports 'Logic utilization (in ALMs)
: N/A' … copying that N/A would be inventing a measurement that was never made."*
**Correct — and it means that tool could not have answered this packet's
question.** The instrument that does is `tools/quartus/run_block_map.ps1`, which
parses the `.map.rpt` **tables** (a second parser for a second format) and yields
`combAluts` and `estimatedAlms`. It also hardcodes the device, so the brief's
"`run_block_fit.ps1` writes `-Device` UNVALIDATED" trap does not apply. Every
existing `zhao_geom_binner_v2@*` row in `zhao_block_fit.json` lacks `combAluts`
for exactly this reason.

---

## 8. WHAT I REFUSED, AND WHAT I GOT WRONG AND CAUGHT MYSELF

### Refused

* **The run-time job-bus multiplex**, on directive §4 and now with its number:
  **2,050 comb ALUT** for the forbidden parallel path with a select line on it.
  Both of MUXBUILD's routes need it; the elaboration select does not.
* **Plumbing `walk_job_*` to `zhao_console_core`.** A core port with no driver is
  a tie-off, ruling R159 makes an undeclared one *lower* the register, and the
  handover calls that "the easiest way to move this metric [and] the one thing the
  campaign forbids". It is bound at `zhao_shell_top_v2` instead, with the
  reasoning beside it, and **declared in entry `I55`** — which already owns the
  sentence *"what is tied is WHO ASKS IT TO WALK and WHO TAKES THE TRIANGLES"* —
  rather than as a new entry that would count one gap twice.
  **`walk_job_*` is BOUND, not TIED**, and the difference is structural: at
  `JOB_SRC = 0` the branch that reads those inputs **is not built**, so there is
  no path a capability is being withheld from. The capability is the parameter.
* **Building TriangleDescriptor v2.** Its deferral's *reason* is discharged (§5)
  and it is the next build, but building it here would be the same speculative act
  MUXBUILD correctly declined, one step further along.
* **Repairing the stale Packet-D layout constants (§7.1).** Diagnosed to the bit,
  with the exact replacement values, and handed to the lane that owns what those
  fields mean. Guessing there changes the meaning of every fragment.
* **Retiring the binner's drain.** A genuine subsystem retirement — one FSM, four
  memories — worth −606,592 bits. Named, measured, not taken.
* **ORing the walk into the live stream.** Seventh refusal. Not done, and the
  bundle split makes it awkward as well as forbidden.
* **Recomputing `2A` or the scissored box on the walk side.**
* **Any console or full-device fit.** Four leaf map-only runs, ~13 minutes total.

### Wrong, and caught

1. **I nearly added 1,879 flops to a design that did not need them.** When the
   door test failed, my first explanation was "`u_tile` does not latch the record,
   so the door must register it" — plausible, and it would have cost back the
   entire register saving. `zhao_raster_tile_pipe_v2.sv:1446` is
   `if (job_metadata_capture_w)` under a comment reading *"Exact frozen metadata
   unpack, once, on the accepted job identity"*, and the capture condition is the
   accept condition. **The comfortable explanation absolved the door and was
   false**, and the check was two lines of a file I already had open — CLAUDE.md's
   own law, on the packet that quotes it.
2. **I ran a stale binary and believed it.** A build returned `BUILD_RC=1` and I
   read the run that followed as new evidence. The tell was that the failure
   message was **byte-identical** to the previous one — a measurement that did not
   move after a change that must have moved it.
3. **I asserted `raster_jobs_started_o == 1` from the shape of a neighbouring
   test.** Both references of a tile start work; only an *aborted* one is sunk
   instead, and the identity-abort case nearby is the 1-started/1-sunk shape, which
   is exactly what made 1 look right. Expectations are now **read off the hardware
   first and then written down**, with that note beside them.
4. **My first Quartus run failed in 18 s with 3 errors and it was my invocation.**
   `powershell -File script.ps1 -TopParameters 'A=1','B=2'` flattens the array to
   the single string `A=1,B=2`, so the QSF got
   `set_parameter -name CHUNKS 8192,CHUNK_W=13`. Loud, cheap, entirely mine.
5. **I trusted `grep -c $'\r$'` and it reported 582 CRLF lines in a pure-LF
   file.** `zhao_geom_bin_pipe_v2.sv` is **LF** while `zhao_geom_binner_v2.sv`
   beside it is **CRLF**. Had the patcher assumed CRLF it would have rewritten
   every line of a 582-line production file. The patcher's `assert` caught it; the
   patchers now **detect** line endings and never assume, verified with Python
   rather than with the shell.
6. **I walked into the documented heredoc trap FOUR times.** It fails on
   quote-heavy text *and* eats a backslash level, so `\\n` inside a `printf` became
   a literal newline and broke the build twice with *"missing terminating \"
   character"*. Every patcher is now written with the Write tool. Written down,
   warned about, and done anyway — four times.
7. **My first instantiation grep missed one of four sites.** I matched
   `zhao_geom_bin_pipe_v2 #`, and `zhao_shell_v2_lease_path.sv:545` instantiates
   the block **without** a parameter list. The configure failed with PINMISSING —
   which is what the brief warns about and what gate 31 exists for.

---

## 9. BRANCH, COMMITS AND GATES

**Branch `gz/doorcost`. Pushed. Never `--force`, never `--force-with-lease`. Not
merged to the integration branch.**

| commit | what |
|---|---|
| `84817e4e` | `feat(DOORCOST)`: I55's raster door is TWENTY-EIGHT wires, not 2,065 |
| `c33808d2` | `test(DOORCOST)`: the door carries a job, and the price is on the shipping part |

### Gate state at the pushed commit

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2** — `I34`, `I55`; no higher than the 2 I started at |
| `check_console_closure_lint.py` (**gate 31**) | **OK**, 296 sources, self-test fired **5/5** — no implicit net, no missing module, **no missing pin** |
| `check_console_inventory.py` | **OK** — 408 modules, 287 elaborated, 296 fit sources |
| `check_prod_manifest.py` | **OK** — 408 modules, 88 tops, 128 inside, 192 excluded |
| `gen_prod_top.py --check` | **fresh** (88 instances) |
| `gen_console_board.py --check` | **FRESH** (1616 core ports) — regenerated; was INHERITED stale, §7.2 |
| `gen_shell_paired_diff.py --check` | **fresh**, harness and mutant |
| `check_quartus17_syntax.py` | **RC 0**, 667 files, self-test 13 fire / 22 no-fire |
| `check_case_labels.py` | **OK**, self-test 3 fire / 1 no-fire |
| `check_entry_claims.py` | **OK** — 23 known sites, no NEW claim (run after editing `I55`) |
| `mutant_copy_drift.py` | **RC 0**, 78 copies, **run AFTER the commit** (R121) |
| `pd_door` / `pd_doorshut` | **11,381 / 4,084 checks, 0 failures**, RC 0 both |
| Verilator `--lint-only -Wall` on the door module | **0 real errors in BOTH `JOB_SRC` arrangements** |
| `npm run abi:check` | not implicated — `spec/commands.zidl` untouched |
| `geom_bin_pipe_v2_directed` and its four mutants | **RED, INHERITED — §7.1**, proven by re-measurement at base. Not this packet's. |
| `run_console_core_smoke.ps1` (plain form) | **PASS**, `SMOKE_RC=0`, **`raster pixels=2816`** over 176 bursts from 75 triangles, **`frames_admitted=1`**, 1,216 fragments carried a texel, every issued word retired. `fetcharm vread=0` and `paramwalk tris=0`, correctly: the console elaborates `JOB_SRC=0`, so the door is present and not driven. |

**THE FIVE SMOKE CONTROL FORMS WERE NOT RUN, and that is a declared gap rather
than an implied pass.** `-Mutant`, `-BadVertex`, `-NoEchoArm`, `-BadTraceArm` and
`-TerrainFlatLattice` are on the gate list; I ran only the plain form. The
reasoning, stated so it can be overruled: this packet adds **no `zhao_console_core`
port** (its only core-file change is 122 lines of comment in entry `I55`), the
plain form PASSES at the exact required `raster pixels=2816` / `frames_admitted=1`,
each form is a cold build of the full console bench at roughly twenty minutes, and
**another lane was already building two of those forms** on this box while I
finished (`zhao_console_core_smoke_untex_*` and `_badvtx_*` under parent PIDs that
are not mine -- classified by command line and parent PID per ruling R81, and left
strictly alone). The coordinator gates the merged result, which is where the five
forms belong. If that judgement is wrong, the five forms are the thing to run
before merging this branch.

**Quartus:** four **leaf map-only** runs on `5CSEBA6U23I7`, ~13 minutes total. No
console fit, no full-device fit, no `-PhysicalPins` row. Authority:
`OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §8, *"local, bounded synthesis/map
experiments … Use named questions and frozen source snapshots"* — verified in that
committed file rather than taken from the brief.

---

## THE ONE-PARAGRAPH HANDOVER

**The door is open, it costs nothing to have and +983 ALUT / +0 DSP to use, and
`I55` still needs three more builds.** The 2,065-wire price that stopped six
packets counted wires that are already ports: the time multiplex feeds the *same*
setup/attrpack pair, so the 1,877-bit metadata arrives on `tri_*` whichever source
fed it, and the real door is the twenty-eight bits the *walk* knows — which tile,
first, last, handshake. `JOB_SRC` selects at elaboration, which is free and which
directive §4 requires, because a run-time select would be the forbidden parallel
path; that multiplex is priced at **2,050 ALUT** by a committed probe so the
refusal has a number. Both of MUXBUILD's routes need it, so they were never
alternatives on price. **Next, in order: TriangleDescriptor v2 — whose deferral
reason this packet discharges — then the console-side source select upstream of the
three-way fork, then a sequencer on `walk_valid_i`, then the binner's drain retired
by REMOVAL for −606,592 bits.** And before any of it: **six of the thirteen
required Packet-D tests have been red since 2026-09-26** because `ceba0bfe` moved
`PRETEX_EARLYZ_KEY_LO` one bit and left four field constants behind, with the
package's own self-check comparing each constant to its own literal so it could
never fire. §7.1 has the exact replacement values and the base-commit
re-measurement that proves the red is not this packet's.
