# FINDINGS -- MATPUB, 2026-09-27

**Branch `gz/matpub`.** Base `8d37df8c`. Commits `085757ac`, `a65cf38e`,
`9a4556ea`, and the commit carrying this file.

---

## THE ONE-LINE ANSWER

**`TERRAIN.COMPOSED_MATERIAL` IS BUILT AND LIVE IN THE ASSEMBLED CONSOLE**, and
the one-pixel tag defect **is not the field and is not the composed material** --
it is **phase sensitivity to SDRAM traffic**, proved by making it appear in the
PLAIN run with no field anywhere near it and disappear from `-FieldUncovered`,
using the same 64 writes in both directions.

Register **1 -> 1**, bare, RC 1, **0 unresolvable**. **I34's head line
untouched.**

---

## 1. `TERRAIN.COMPOSED_MATERIAL`, BUILT (deliverable 3)

`fpga/rtl/terrain/zhao_terrain_matpub.sv`. Measured at the console's own edge,
in every form:

```
matpub cells=1024 commits=1 published=1 skipped=0 stranger=0
       bursts=64 denied=0 oob=0 short=0 commit_busy=0 busy=0
```

1,024 cells captured off `zhao_terrain_matjoin`'s composed write face, one
commit, one patch published as **64 sixty-four-byte writes** into
`[0x058B_0000, 0x05AB_0000)` through the terrain build share's new **sixth**
requester and MEM.GUARD's new **write-only** window, guard granting every
request. Tap, payload and granularity are the decision record's, unchanged.
`zhao_terrain_patch_acc` was not re-scouted; it stays ruled out.

Composition: share `N` 5 -> 6, guard window + header table, `zhao_pkg`
constants, `blocks.yml`, `prod_manifest.yml`, `fit_targets.yml`,
`tests/CMakeLists.txt` (directed test + `-Wall` lint), `spec/memory_rules.md`
region row + decision record, **both** wrapper mutants' port blocks, and both
generated files regenerated.

## 2. THE COST, MEASURED ON THE BUILT THING (deliverable 5)

**64 fabric requests per published patch, one published patch on this scene.**
The decision record predicted 64 and the brief told me not to inherit it; it is
now measured and it agrees. Break-even is 129 patches. Guard refusals: 0.

**Silicon, hand-counted and NOT fitted** (the ruling forbids a full-console fit,
authorises targeted local measurement): a 1,024 x 32-bit plane buffer (~4 M10K),
a 256 x 49-bit slot-record RAM (~2 M10K), a 512-bit burst shifter, two FSMs and
ten 32-bit censuses -- **order 900 ALM, 0 DSP, 6 M10K**. Both arrays are
memories deliberately: ALMs are the binding constraint, M10K is the slack.
**It is NOT zero silicon** and must not be reported as though LASTGAP's
zero-silicon evidence covered it; that evidence was about the constant pool.

## 3. A FINDING THAT CHANGED THE DESIGN: THE PUBLISH GATE NEEDS A THIRD ARM

The decision record says *"published only for a patch whose composed plane
actually changed."* That is right and **not sufficient**.

`TERRAIN.DEVSTORE` already paid for the missing half: *"THE KEY IS THE RESIDENCY
PAGE SLOT. A compose slot is reused by a different patch from one frame to the
next, so the HISTORY ... would be inherited by a stranger."*

A change-only gate is blind exactly there. If a slot changes occupant and the new
plane happens to hash equal to the old one's, it **SKIPS** -- and the region then
describes a **stranger** under this patch's address, silently, every counter
balancing. That is the cancelling-errors shape with a slot number on it.

So the gate has **three publish arms** (never published / occupant changed /
signature changed) plus one skip arm; `stranger_pub_o` counts the second; and
`terrain_matpub_directed` **case 4 constructs the exact collision** -- a
bit-identical plane under a different patch id -- and requires a publication.
**It ADDS publications the costed gate would have skipped and removes none**, so
it is not a trim of capacity, semantics, update behaviour or destination.

The slot is described honestly: it is **not** a residency slot (this console
exports none at the compose seam), it is the patch identity's low byte. Two ids
colliding in eight bits share a slot, and the stranger arm is what makes that
cost a WRITE rather than a wrong record.

## 4. THE ONE-PIXEL TAG DEFECT (deliverable 2)

### 4.1 IT IS NOT THE FIELD AND NOT THE MATERIAL

A probe on **the same net the counter reads** (`gth_tag_w`, which drives
`zhao_post_gather_tag`'s `f_tag_i`) captures every beat with a nonzero tag byte:

| form | tagged beats | reserved | samples / 1216 |
|---|---|---|---|
| plain, publisher OFF | **0** | 0 | **1216** (clean) |
| plain, publisher ON | **2** | 0 | 1213 |
| `-FieldUncovered`, publisher OFF | 2 | 0 | 1214 |
| `-FieldUncovered`, publisher ON | **0** | 0 | **1216** (clean) |
| `-FieldActive`, publisher OFF | 2 | **1** | 1213 |
| `-FieldActive`, publisher ON | 2 | 0 | 1214 |

**Read the two rows that settle it.** Adding my publisher's 64 writes to the
PLAIN run -- no field, no capsule, no composed material -- **reproduces both
symptoms**. Adding the same 64 writes to `-FieldUncovered` **removes them** and
that form goes clean. The symptoms move with the traffic pattern **in both
directions**. That is a phase sensitivity, not a material defect.

### 4.2 THE OFFENDING PIXELS ARE BLACK, WHICH NAMES THE MECHANISM

Every captured beat reports **`rgb565=0000`**, and `gather_fragments_o` is not
the covered-fragment count -- RASTER.RESOLVE sweeps a touched TILE WHOLE, so the
gather sees all 256 pixels of every touched tile (11 x 256 = 2,816).

So these are **uncovered pixels whose colour is the clear value and whose tag
byte is not**. The tile-store word is
`[63:40] colour [39:32] tag [31:8] depth [7:0] stencil`
(`zhao_raster_resolve.sv:147`; `px_tag = q_data_r[39:32]` at `:197`). The colour
cleared and the tag byte did not.

**Latent, not cosmetic.** At the observed values (0x13, 0x18, 0x1d, 0x22, 0x40,
0xC0) classification lands on untagged, below-knee or reserved, none of which
contributes bloom. A stale byte landing above the knee in channel 0b01 would
light a pixel nothing drew.

**I did not repair it.** The clear path is inside `zhao_shell_top_v2`, which this
brief puts out of bounds beyond reading; the repair wants its own packet with
the shell in scope and its own controls.

### 4.3 THE GATE THAT FIRES HAS ALWAYS BEEN BLIND TO THIS

`zhao_post_gather_tag` classifies `frag_untagged_o` on **`f_tag_i[7:6]` alone**
(`:224`, `:298-301`). The law the bench states is *"every fragment must be
UNTAGGED ... the default effect tag 0"*; the counter is `tag[7:6] == 0`. **A tag
of 0x3F counts as untagged.** Every row above with `reserved=0` still carries
stray tags -- they were simply small. `-FieldActive` did not make a defect
appear; it enlarged one past the threshold the counter happens to test. **The
gate was passing on a coincidence**, and only a probe on the tag BYTE could say
so.

### 4.4 `err_unpublished_o` -- THE WRONG INSTRUMENT, AND ALREADY FIRED

* **Wrong instrument for the hypothesis it was cited for.** It fires on
  `d_leave_i && !pub_valid_q` (`zhao_material_window.sv:578-579`) -- a triangle
  leaving the door with **nothing published**. LASTGAP's hypothesis was a
  producer publishing **`sample_count = 0`**, a publication that exists and asks
  for nothing, where `pub_valid_q` is HIGH. Different events; its zero was never
  evidence about the shortfall in either direction.
* **It already HAS a committed positive control.**
  `tests/texture/material_window_directed.cpp:635` asserts
  `err_unpublished_o == 1` on a departure with nothing published, with silent
  cases either side. "It has not been shown to fire" was **false about this
  tree**.

### 4.5 THE COMFORTABLE EXPLANATION, KILLED BY A CONTROL

My first hypothesis was that the bench reads the counters while beats are still
in flight -- a bench artefact, no defect, the flattering direction. I built the
control instead of reasoning: capture, idle **20,000 clocks**, capture again.

```
quiesce after 20000 idle clocks: texture frags 1216 -> 1216, samples 1213 -> 1213
                                 gather untagged 2815 -> 2815, reserved 1 -> 1
```

**Nothing moves.** Neither symptom is a quiescence artefact. The control is
committed and stays in the bench.

## 5. THE TWO DEAD ASSERTIONS (deliverable 4)

MATFIELD committed the clause-3 and clause-4 consumer assertions and **they had
never executed under either previous packet** -- `-FieldActive` stopped ~780
lines above them. LASTGAP measured their numbers, printed them, and refused to
claim they passed: *"a number printed above a gate is not an assertion
executed."*

**They now execute, and the repair was to move the assertion rather than to
quote the number.** The whole two-sided law -- clause 3, clause 4, clause 5's
opposite direction, clause 6's no-field arm -- is hoisted into the early-diag
block, above the fragment-tag gate and the sample gate.

**Moving an assertion is only honest if its subject is already final**, so that
is what was checked rather than the line number: `tsfill_tile_max_q`,
`tsfill_tile_or_q` and `render_texture_palette_lookups_o` are accumulated by the
fill socket during the frame and complete before any of these gates run -- which
is exactly how LASTGAP could print all three there. Nothing between the two
sites can change them. Same assertion, same values, executed earlier.

**MEASURED, AT THE PUSHED HEAD -- all three arms EXECUTED and PASSED:**

```
-FieldActive     SMOKE: fieldmat CLAUSE 3/4 EXECUTED tile_max=212 tile_or=222
                        (field names 212/30, authored range tops out at 6)
                        field_composed=1024 token_refused=0
-FieldUncovered  SMOKE: fieldmat CLAUSE 5 EXECUTED tile_max=6
                        (authored range tops out at 6) field_composed=0
plain            SMOKE: fieldmat CLAUSE 6 EXECUTED tile_max=6
                        (authored range tops out at 6)
```

**HOW EACH ARM IS SHOWN ABLE TO FAIL, without planting a defect.** The three
arms are ONE instrument asserted in OPPOSITE directions: the covered form
requires the fetched tile strictly ABOVE the authored range, the uncovered and
no-field forms require it at or below. An instrument stuck at 212 fails the
plain and uncovered arms; one stuck at 6 fails the covered arm. The three
readings above are 212 / 6 / 6, so each arm is the others' fail demonstration
and none of them can be passing because the value never moves. That is the
property the brief told me to preserve, and it is why the pair is evidence
rather than two numbers.

**Not duplicated**: the old site carries a pointer, not a copy. The two-sided
property is preserved -- the same instrument asserted strictly ABOVE the
authored range in the covered form and at-or-below it in the uncovered and
no-field forms.

## 6. CLAIMS FOUND FALSE, AND ONE IS MINE (deliverable 7)

1. **MINE, in my own first commit message.** I wrote *"two gather beats have
   carried a nonzero effect tag all along."* **FALSE** -- the plain run with the
   publisher off carries **ZERO**. I measured two forms and generalised to a
   population I had not measured. The plain control refuted it, and it is the
   control I had not run when I wrote the sentence. Corrected in `a65cf38e`
   rather than quietly fixed.
2. **LASTGAP's hypothesis that the tag and the sample shortfall are ONE cause**
   -- refuted. Plain-with-publisher has **3 missing samples and ZERO reserved
   beats**; the symptoms move independently.
3. **"`err_unpublished_o` has not been shown to fire"** -- false about this tree
   (4.4), and it is the wrong instrument for the question anyway.
4. **The framing that the FIELD produces this defect** -- false. Any added
   traffic produces it, and added traffic can also remove it.
5. **`gather_frag_untagged_o` does not mean "tag == 0"** (4.3).

## 7. THE SIX CLAUSES, EACH ANSWERED SEPARATELY (deliverable 1)

| # | clause | verdict |
|---|---|---|
| 1 | installed **and executed** | **MET.** `runs=1089 noprog=0 faults=0`, unchanged by this branch. |
| 2 | **covers the intended terrain** | **MET.** `tp_covers=1`, `skipped_uncovered=0`. |
| 3 | cannot equal the baseline by accident | **MET.** `field_composed=1024 token_refused=0`, and the assertion now **EXECUTES** instead of being printed above a gate. |
| 4 | reaches the **intended consumer** | **MET.** `tile[max/or]=[212 222]` off the address the island issued; 212/30 are exactly `SFF_MAT_A`/`SFF_MAT_B` against an authored plane topping out at 6. Assertion now **EXECUTES**. |
| 5 | uncovered **restores the authored result** | **MET, and greener than it was.** `tile[max/or]=[6 7]`, and with the publisher armed the form's texture arm is **clean** (`samples=1216`, `tagged_beats=0`) where it previously lost two samples. |
| 6 | **no-field forms unchanged** | **The four numbers the brief names HOLD** -- `pixels=2816`, `frames_admitted=1`, `fragments=1216`, `tile[max/or]=[6 7]`. **The plain run's sample count moved 1216 -> 1213 and the form now exits 1.** Section 8. |

**I55 IS NOT REGRESSED**, in every form: `paramwalk tris=101`, `fetcharm
vread=303` with its asserted invariant, `tilewalk tiles=11 jobs=101`,
`raster pixels=2816`.

## 8. WHAT I AM NOT HIDING -- THE PLAIN RUN'S SAMPLE COUNT

Arming the publisher in the plain form moves `render_texture_samples_o` from
1,216 to 1,213, so the plain smoke exits 1 on the sample gate where it passed.

**I did not disarm the publisher to make it green.** That would be closing a gap
by disconnecting function -- the first thing PACKET-PROTOCOL forbids -- and it
would hide a real console defect the field forms have been hitting since before
this branch existed. The publisher behaves exactly as designed: 64 bursts, zero
denials, 0.15% of frame.

**This is NOT the measured conflict the owner's ruling asks me to escalate on.**
That clause is about publication semantics failing the frame/bandwidth contract,
and they do not -- the traffic is two orders of magnitude inside budget. This is
a **pre-existing phase sensitivity in the render path that any added traffic
perturbs**, and it wants its own packet with the shell in scope.

## 9. GATES, AT THE PUSHED HEAD

| gate | result |
|---|---|
| `completion_register.py` **BARE** | **1** -- `I34`. RC 1. **0 unresolvable.** Head line untouched. |
| `check_console_inventory.py` | RC 0 -- 289 elaborated, 298 fit sources |
| `check_prod_manifest.py` | RC 0 |
| `check_quartus17_syntax.py` | RC 0, self-test 13 fire / 22 no-fire |
| `check_case_labels.py` | RC 0, self-test 3 fire / 1 no-fire |
| `gen_prod_top.py --check` | fresh (89 instances) |
| `gen_console_board.py --check` | FRESH (1638 core ports, 127 parameters) |
| `gen_shell_paired_diff.py --check` | fresh, harness and mutant |
| `check_console_closure_lint.py` (gate 31) | RC 0, self-test fired 5/5, no missing pin |
| `mutant_copy_drift.py` (**AFTER** the commits, R121) | RC 0 -- 80 copies, no drift |
| `lint zhao_terrain_matpub -Wall` | **RC 0** |
| `terrain_matpub_directed` (BUILT AND RAN, R60) | **46 checks, 0 failures** |

**One gate was nearly misread.** Running the sweep through `| tail` printed
`RC=0` for the completion register -- which is **`tail`'s** exit code. The
register's own RC is 1. That is this tree's documented
read-the-pipeline's-status trap, and it appeared in my own first gate sweep.

**ROWS I DID NOT RUN, named rather than omitted.** `-Mutant`, `-BadVertex`,
`-NoEchoArm`, `-BadTraceArm`, `-TerrainFlatLattice`, `test_cmd_exec_directed`,
and the TypeScript compiler suite. I changed core ports, so both wrapper mutants
were updated and gate 31 confirms no missing pin -- **that is an argument about
elaboration, not a measurement of those forms**, and I flag it for the
coordinator rather than quoting it. Same row NOPROG, MATFIELD and LASTGAP each
flagged.

## 10. WHAT I GOT WRONG AND CAUGHT MYSELF

* **Generalised a claim from two forms to three** (6.1); the missing control
  refuted it. I had the control available and wrote the sentence before running
  it.
* **My first lint of the new block failed on a comment** whose first word after
  the slashes was the linter's own name -- the pragma trap PACKET-PROTOCOL
  warns about, hit on the first try.
* **I hand-transcribed the guard change into four mutant copies** before reading
  `devbound`'s own header: *"GENERATED by `tools/rtl/gen_mem_guard_mutants.py`
  from production, so a refresh is a command rather than an act of
  transcription."* Hand edits reverted, generator run.
* **Two width bugs caught before any build.** `SLOTSW` was used in the port list
  where a body localparam cannot reach; and `CELLAW'(CELLS)` truncates 1,024 to
  **zero** at `CELLAW = 10`, which would have made `short_fill_o` compare
  against 0 and never fire. The fill counter is now two bits wider than the
  address so an OVERFILL is representable too.
* **My cell index was `{c_cj_i, c_ci_i}`**, correct only while `CELLS_X` is
  exactly 32 and silently truncating at any other shape. Replaced with the
  arithmetic.
* **A heredoc into `bash -c` died on quote-heavy text**, exactly as
  PACKET-PROTOCOL warns and exactly as it did for both previous packets.

## 11. PROCESS

Other repositories' builds ran throughout; **nothing was killed**. **No Quartus
was run. No subagent was spawned.** I edited no `zhao_geom_*` file and did not
touch the shell. No `until ... sleep` poll loop was armed. Pushed only
`gz/matpub`, never `--force`. **I did not write the head-line declaration.**
