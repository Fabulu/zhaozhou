# FINDINGS -- LASTGAP, 2026-09-27

**Branch `gz/lastgap`.** Base `394cac06`. Commits `bc39a3d8`, `fad886be`,
`9fc43fee`, and the commit carrying this file.
Written against the brief's nine numbered deliverables, the owner ruling
`reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md` and the coordinator's
`reports/DECISION-20260927-I34-CONSTANT-POOL.md`.

---

## THE ONE-LINE ANSWER

**The constant pool is delivered, and clauses 3 and 4 are MET IN THE ASSEMBLED
CONSOLE.** `terrmat field_composed=1024 token_refused=0` against
`field_composed=0 token_refused=1024` at my base, and the texture island fetched
**mosaic tiles 212 and 30 -- `SFF_MAT_A` and `SFF_MAT_B` -- against an authored
layer-E plane that tops out at tile 6.** It cost zero silicon, and the register
high-water did not move: measured twice, on two different programs.

**`I34` does not close**, and both reasons are measured rather than argued:

1. `TERRAIN.COMPOSED_MATERIAL` is **decided and specified but NOT BUILT**
   (section 7) -- with its cost measured, and a finding the owner will want:
   **at this console's real dirty fraction it FITS**, so COMPOSEPUB's bandwidth
   refusal does not carry across to this channel.
2. The positive form still stops, ~780 lines above MATFIELD's two clause
   assertions, on **one framebuffer pixel of 2,816 carrying an effect tag** and a
   **three-fragment texture-sample shortfall** that appear once the field is
   staged (section 6). I did not silence either assertion.

Register **1 -> 1**, bare, RC 1. **I34's head line untouched.**

---

## 1. DELIVERABLE 2 -- THE CONSTANT POOL, DELIVERED IN THE LIBRARY

`LowerOptions::materialize_scalars` (ON by default) makes `lower()` emit every
scalar slot a VECTOR uop reads as an `LDC dst=scalar_base+s,
imm=prep->scalar[s]` at the head of the physical stream.

**NOTHING UNDER `fpga/` CHANGED.** `zhao_field_alu.sv:307` already carries
`OP_LDC: result_o = $signed(imm_i)`, and the doorbell's `LdUop` arm already
carries the 32-bit immediate at `ld_data_i[63:32]`. `LDC = 0x02` was in the
generated op table (`dst_width 1`, `n_groups 0`, `IMM_RAW`) and in the silicon
the whole time; what was missing was anything that EMITTED it.

Measured on `scorch_wash` at the console's own composition (scalar_base 8,
out_base 3, REGS 32, INSTR_N 32):

| | before | after |
|---|---|---|
| physical uops | 11 | **22** (11 LDC), against `INSTR_N = 32` |
| `register_high_water` | 28 | **28** |
| doorbell load words | 38 | 49 |
| capsule bytes | 1,344 | 1,408 |
| token base `0xE1D41ED0` | preload row at **register 27, never written** | **LDC into register 27**, the register uop 20's `ADD` reads |

**THE LDCs ARE PROVEN TO EXECUTE IN THE COMPOSED SILICON, by outcome rather than
by inspection.** If they did not, register 27 would still read zero, out-lane 2
would carry tag `0x00`, and all 1,024 words would be refused -- which is exactly
what the base measures. Instead `field_composed=1024 token_refused=0`.

**A NAMED REFUSAL RATHER THAN A SILENT ZERO.** Materialising without `prep` would
emit `LDC #0` and reproduce the exact defect under the repair's name, so
`kRefusalNoPreparedValues` refuses it. Fired deliberately: lowering `scorch_wash`
with `prep == nullptr` returns false with that string.

## 2. DELIVERABLE 7 -- WHAT I FOUND FALSE, AND TWO ARE LOAD-BEARING

### 2.1 THE BRIEF'S REGISTER-PRESSURE PREDICTION IS FALSE

The brief and the decision record both state that *"folding constants into LDC
uops **adds register pressure by construction**"*, and require the high-water to
be re-measured because *"28 of 32 is the before number"*.

**Measured: 28 before, 28 after** for `scorch_wash`, and independently **61
before, 61 after** for `crater_ring` in `field_host_plan_directed` -- the second
taken by building the BASE COMMIT's `zfield_host_plan.cpp` in a throwaway
directory and running the same test against it, so it is a control and not a
recollection.

It does not move, and the reason is structural rather than lucky: `lower()`'s
association-region loop already called `touch()` on every register
`scalar_base .. scalar_base + n_scalar - 1`, so the LDC destinations are
registers the high-water had ALREADY counted. Materialisation moves a register
from *association-owned* to *point-written*; it does not add one.

The instruction to re-measure was right and I carried it out. The prediction
attached to it was wrong, in the direction that made the chosen fix look more
expensive than it is.

### 2.2 `lower()` DOES NOT FOLD "LITERALS" -- IT FOLDS THE WHOLE UNIFORM BLOCK, AND THIS ONE NEARLY MANUFACTURED A FALSE PASS

The decision record says the fix is that *"`lower()` emits folded constants as
LDC uops"*, and the generator's own comment said *"`lower()` folds every
uniform-only value -- which includes EVERY LITERAL"*. Read as "the literals",
that is **wrong about which values are missing**, and it is the dangerous kind of
wrong.

Measured over `scorch_wash`'s plan: **20 scalar slots, 11 read by vector uops,
and only 6 of those 11 are literal-only.** The other five are PARAMS-DERIVED --
`s2`,`s3` (the scorch centre), `s5` (`rOut`), `s6` (the nav surcharge), and
**`s11 = RCP(s4 - s5)`, the falloff reciprocal, computed by the uniform
instruction block from the association's parameters.**

**Had I materialised only the literals, the console would have passed clause 3's
stated test while the field computed on nothing.** With `s11 = 0`: `MUL -> 0`,
`CLAMP(0, 0.0, 1.0) -> 0`, the smoothstep collapses, the weight addend is 0, and
out-lane 2 becomes **exactly `0xE1D41ED0`** -- a *legal v1 token*, tag `0xE1`,
matA `0xD4`, matB `0x1E`, weight `0xD0`. That triple lies outside the authored
plane's 164 reachable bytes, so **`field_composed` would have moved,
`token_refused` would have gone to zero, and MATFIELD's entire non-coincidence
argument would have been satisfied** -- by a constant, from a field that never
read `x` or `z`. This console has shipped a check that passed on a constant twice
before.

The fix materialises **every slot a vector uop reads**, which is why the run
carries 43 distinct weights rather than one, and why the mosaic fetched two
distinct tiles rather than one.

### 2.3 THE FIXTURE CLAIMED EACH PRELOAD ROW WAS "POSTED TWICE"

`smoke_field_fixture_gen.cpp` emitted `preload_rows=M (each posted TWICE: kind 3
UNIFORM to the scalar bank AND kind 7 PREPARED to the prepared file)` into the
generated fixture. **Exactly one word is posted per row, kind 7.** MATFIELD
removed the `LdUniform` post *in the same commit that wrote that sentence*,
correctly, and the sentence describing the old behaviour survived. Corrected;
the line now also reports the LDC count.

### 2.4 THE BENCH'S `field_composed=0` FATAL NAMED A DEFECT THAT IS NOW FIXED

Its message told the next reader the cause is *"`lower()` folds literals into
preload rows ... so the token BASE reads zero"*. True when written, false now.
Rewritten to say the constant-pool defect is repaired and that a recurrence means
something NEW -- the precise failure CLAUDE.md records as *"a wrong diagnosis
attached to a right alarm sends the next person to reshape something that is
already correct."*

### 2.5 A STALE CLAIM IN THE BENCH'S TERRAIN NOTE (found, NOT corrected)

`tb_zhao_console_core_smoke.sv:9975-9978` states *"`zhao_texture_mosaic_v2`'s
answer ... is still read by nobody inside `zhao_texture_island_v3_top`, so the
layer-E triple still has no consumer"*. Entry I34's own text records that
**MATCARRY landed that reader at `zhao_console_core.sv:22818`**, and this packet
MEASURES the pick reaching the island -- `tile[max/or]=[212 222]`. Left alone: it
is outside my hunks and is a claim about another lane's block.

## 3. DELIVERABLE 3 -- THE TWO PREVIOUSLY-UNREACHED ASSERTIONS

MATFIELD's clause-3 and clause-4 consumer assertions are committed and had
**never executed**, because `-FieldActive` fatalled ~2,600 lines above them.

**They are still not executed, and I am NOT claiming they passed.** What changed
is that the run now gets past the fatal that blocked them and **the numbers they
assert on are measured and printed**, by the diagnostic this packet hoisted above
every gate:

```
early-diag mosaic fills[tileset/mesh/stray]=[27 20 0] tile[max/or]=[212 222]
           first_addr=001d4730
```

* clause 3 asserts `tsfill_tile_max_q > TERR_AUTHORED_TILE_MAX_C`. Measured
  **212 > 6**. It would pass.
* clause 4 asserts the tile is EXACTLY `SFF_MAT_A` or `SFF_MAT_B`. Measured
  **212 == 0xD4 == SFF_MAT_A**, and `tile_or = 222 = 0xD4 | 0x1E`, so the only
  two tiles fetched are the two the program can name. It would pass.
* `stray=0` says no fill landed outside the bound rows, and `first_addr` decodes
  to `TERR_TS_BASE + 212*4096` exactly.

**A number printed above a gate is not an assertion executed, and I am keeping
that distinction.** What blocks execution is section 6.

**AND THIS IS THE CAMPAIGN'S "UNEXERCISED TAIL" ONE LAYER DEEPER.** NOPROG found
five stale census assertions in the tail behind the `field_composed` fatal and
repaired them. Behind those sits a FURTHER gate that no field form had ever
reached either. Each fatal only reveals the next one, and the tail is still not
fully exercised in any field form.

## 4. DELIVERABLE 5 -- THE COST, MEASURED

**Zero added silicon, measured rather than asserted:**

| proof | reading |
|---|---|
| `git diff --stat 394cac06..HEAD` | **no file under `fpga/`** |
| `gen_prod_top.py --check` | **fresh** (89 instances) -- regenerates from block ports, so a port change makes it stale |
| `gen_console_board.py --check` | **FRESH** (1626 core ports, 127 parameters) -- red on any core port change, by design |

**What it DOES cost is instruction slots and engine time, which is the trade the
decision took.** 22 uops per point instead of 11, over 1,089 points. The capsule
grew 1,344 -> 1,408 bytes and the doorbell stream 38 -> 49 words; `crater_ring`'s
capsule in the directed test grew 1,536 -> 1,664, which is exactly 16 LDCs x 8
bytes.

`fld_earth_stall_cycles` reads **234,134** with 1,089 real evaluations, against
3,266 in FIELDACTIVE's run -- **but that is NOT a like-for-like regression and
must not be quoted as one**: in that run the engine never executed a single
program (`runs=0`). It is the cost of the engine actually working, and there is
no earlier measurement of a *working* engine to compare it against.

**That extra staging traffic has a consequence, in section 6.**

## 5. THE SIX CLAUSES, EACH ANSWERED SEPARATELY

| # | clause | verdict |
|---|---|---|
| 1 | a real production Field program **installed and executed** | **MET, MEASURED.** `scorch_wash` fetched and sealed over the real HPS bridge, resolved by the field list, and **executed 1,089 times with `noprog=0 faults=0 short=0`** -- one evaluation per lattice vertex of the 33x33 patch. |
| 2 | the field **covers the intended terrain** | **MET, MEASURED.** `tp_covers=1`, `skipped_uncovered=0` in the covered form. |
| 3 | a value that **cannot equal the authored baseline by accident** | **MET IN THE ASSEMBLED CONSOLE.** `field_composed=1024 token_refused=0`: every one of the 1,024 compose-cache cells took the field's material and NOT ONE was refused by `zmt_tag_ok`. The value is non-coincident **by layout** (the authored plane's 164 reachable bytes against the program's 50, intersection empty), enforced by a detector that parses the bench and was fired by MATFIELD, and it is **not a constant** -- 43 distinct weights, two distinct mosaic tiles. |
| 4 | that composed material **reaches the intended production consumer** | **MET, MEASURED AT THE CONSUMER.** `tile[max/or]=[212 222]` off the ADDRESS the texture island issued on its fill requests. 212 and 30 are exactly `SFF_MAT_A`/`SFF_MAT_B`; the authored plane tops out at 6; `stray=0`. |
| 5 | the **uncovered control restores the authored result** | **SUBSTANCE MET, AND ON THE SAME INSTRUMENT AS CLAUSES 3/4.** `runs=0 skipped_uncovered=1089`, `field_composed=0`, `token_refused=0`, `raster pixels=2816`, and **`tile[max/or]=[6 7]` -- the plain run's tiles EXACTLY**, against the covered form's `[212 222]`. The two-sided law does what it was written to do: the covered form moves the mosaic pick and the uncovered form restores it, differing in ONE field of ONE record. **But the form is NOT green**: it loses two texture samples (section 6), and below that sits the inherited `lane_desync_o` red NOPROG measured and deliberately left. |
| 6 | the **no-field forms remain unchanged** | **MET, MEASURED.** Plain `SMOKE: PASS`, RC 0, `pixels=2816`, `frames_admitted=1`, `fragments=1216`, `tile[max/or]=[6 7]`, `untagged=2816 reserved=0`, `token_refused=0`. None of the four numbers the brief names as the no-regression bar moved. |

**I55 IS NOT REGRESSED**: `paramwalk tris=101`, `fetcharm vread=303`, `tilewalk
tiles=11 jobs=101` unmoved in every form I ran.

## 6. A MARGINAL RACE MY CHANGE TIPPED OVER -- DECLARED, NOT SILENCED

Two numbers move in the field forms, and **neither is the material's value**:

| form | texture fragments / samples | gather classification |
|---|---|---|
| plain (PASS) | 1216 / **1216** | `untagged=2816 reserved=0` |
| `-FieldUncovered` | 1216 / **1214** | `untagged=2816 reserved=0` |
| `-FieldActive` | 1216 / **1213** | `untagged=2815 **reserved=1**` |

**`-FieldUncovered` runs the field ZERO times** (`runs=0
skipped_uncovered=1089`) and still loses two samples, so this is **not** the
composed material -- it is the STAGING. The only delta from MATFIELD's head,
where this same assertion PASSED, is 11 more doorbell words and 64 more capsule
bytes over the HPS bridge that terrain's page loads share.

**What I ruled OUT by measurement, so the next packet does not re-derive it:**

* **Not an unpublished material span.** `matwin resolves=2 switches=2 occ_max=2
  no_record=0 err[unpub/underflow]=[0 0]`, identical to plain -- and
  `err_unpublished_o` is precisely the counter for a triangle leaving the door
  with nothing published.
* **Not terrain's material mode.** `zhao_terrain_clipfeed:839` derives
  `MATMODE_BACKED` from `mset_q != 0`, and a terrain span reaches a non-zero
  `sample_count` only on that arm -- but `terrmat backed=128 orphan=0` in ALL
  THREE forms, so every terrain triangle had its material.
* **Not the packet order.** The `TerrainField` record is laid down AFTER
  `DrawForm` and before `EndFrame`, so `SetEnvironment`'s material already
  precedes it.
* **Not the frame gate.** The field form's gate is strictly LATER than plain's --
  install, commit, stage-done and `fld_db_load_words_o >= SFF_N_LOAD + 1`.
* **Not a dropped or mismatched texture retire context.** `frag_tag_o` is
  `returned_retire_ctx_w.raster_continuation.post_earlyz.effect_tag`
  (`zhao_raster_texture_stage_v3.sv:383`), so a context returning out of order
  would carry another fragment's tag -- and that stage has a sequence guard with
  a drop policy. The console reports `walkquiet abort=0 seqabort=0 seqmis=0`, so
  it never fired. (By this file's own standard that zero is a claim, not a
  proof: I did not fire the guard.)
* **Not a base_rgb width spill into the tag field.** I suspected it, because
  `base_rgb` is 24 bits, is *"the published texel RGB when `sample_count == 0`"*,
  and the field's material makes its top byte large for the first time.
  `zhao_texture_material_combine_v3.sv:511` is `s_rgba0 = (s_count == 2'd0) ?
  s_base : s_s0[31:0]` -- a 32-bit RGBA either way, no truncation, no spill.
  **Ruled out by reading, and recorded because it is the attractive wrong
  answer.**

* **Not terrain starvation on the shared bridge**, which was my first guess.
  Measured across the three forms: `arb c1_bursts=1670` and `res hits=2
  misses=10 claims=5 resident=3` are IDENTICAL in all three, so terrain's page
  loading is untouched. What differs is only the capsule's own traffic --
  `hps_beats_in` 13,865 (plain) against 14,063 (both field forms), about +198
  beats -- and the socket's `contention` figure, 43 / 39 / 18. So the field
  staging changes the PHASING at the socket without starving anything.

**THE HYPOTHESIS I AM HANDING OVER, MARKED AS A HYPOTHESIS.** The two symptoms
are probably ONE: fragments whose producer published `sample_count = 0` write a
tile-store word whose tag byte is not driven from a published material, and one
of the three in `-FieldActive` happened to land with channel bits `0b10`/`0b11`,
which `zhao_post_gather_tag` counts as `reserved_channel_o`. It fits the counts
-- uncovered has two such fragments and `reserved=0`, active has three and
`reserved=1` -- and `fb_tag_o` is `fifo_q[rptr][23:16]`, a stored per-pixel field
rather than a recomputed one. **I did not prove it and I am not asserting it.**

**I did not weaken either assertion.** The fragment-tag law ("with
`fragment_decl` bit 0 clear every fragment must be untagged") and the sample law
("every material this fixture uploads declares one sample, so samples ==
fragments") are both sound, both are falsifiable, and the sample law is recorded
as having been FALSE by 26 at an earlier commit. Silencing either to make a form
green is the one thing this campaign's rules forbid most plainly.

**AND `err_unpublished_o` READING ZERO HERE IS A CLAIM I AM FLAGGING RATHER THAN
QUOTING.** Three fragments demonstrably came from a producer that published
`sample_count = 0`, while the counter whose job is to notice exactly that reads
zero. Either those are two different events, or the counter cannot see this one.
**I did not fire it**, so by this repository's own law -- *"a detector reading
zero is a claim, and it is the claim to check hardest"* -- its silence is not
evidence, and the next packet should fire it before believing it.

## 7. `TERRAIN.COMPOSED_MATERIAL` -- DECIDED AND SPECIFIED, NOT BUILT

`reports/DECISION-20260927-I34-COMPOSED-MATERIAL-PUBLISHER.md` records the tap,
payload, granularity and cost in the directive's format.

**The brief's instruction to check `zhao_terrain_patch_acc` first is discharged
by checking it and ruling it OUT.** It is the four-bank **field-major**
accumulator of the `patch_v2` topology this console does not run, with 16 M10K of
its own scratch and its own INIT/ACCUM/DRAIN phases. Publishing from it would
couple the destination to a rewrite the destination does not need. Its only
tree-wide instantiation is `tests/differential/tb_terrain_fieldmajor.sv`, in no
fit closure -- so it stays a BUILT-INSTALLED-NOWHERE cheque, and I did not cash
it here.

**THE TAP IS `zhao_terrain_matjoin`'s composed write face** -- `o_we_o`,
`o_ci_o`/`o_cj_o`, `{o_mat_a_o, o_mat_b_o, o_weight_o}`. It is the only place the
composed triple exists as a unit: upstream it is an authored plane plus a field
lane, downstream it is already a mosaic pick. FIELDACTIVE and MATFIELD named the
same tap independently, which is corroboration rather than one packet's taste.

**THE COST, DERIVED FROM COMPOSEPUB'S OWN LEDGER AND LABELLED DERIVED.** 4 B per
cell x 1,024 cells = 4,096 B per patch = **64 fabric requests per published
patch**. COMPOSEPUB prices 18,432 requests as taking the frame 80.17% -> 124.41%,
which is **exactly 40.00 grant-clocks per request** -- an integer, which is the
check that its two numbers describe one model.

| patches published per frame | requests | added % of frame |
|---|---|---|
| 256 (COMPOSEPUB's worst case) | 16,384 | 39.32% -- **does not fit** |
| **129 (break-even vs the 19.83% headroom)** | 8,256 | **19.82%** |
| 1 (this console's measured scene) | 64 | **0.15%** |

**So at the dirty fraction this console actually produces, the publisher fits
with room to spare**, and COMPOSEPUB's bandwidth refusal does not carry across.
That is the number COMPOSEPUB's own record commissions: *"the packet that builds
the publisher owes that fraction measured on a real scene."* **It is DERIVED, not
measured on a board, and MATFIELD's standard applies: it is not an escalation.**

**I did not build it, and I am declaring that rather than trimming it.** Reasons,
so the next packet does not re-derive them:

* It is **not zero silicon** -- an M10K cell buffer, a new fabric write client and
  a `MEM.GUARD` arm -- so it stales `gen_prod_top` and `gen_console_board`, which
  are this packet's zero-silicon EVIDENCE for the constant-pool fix. The two must
  land in separate commits or that evidence is muddied.
* `check_console_inventory.py` **G4** fails any module under `fpga/rtl` with no
  disposition, so a block committed uncomposed needs an inventory row saying it is
  out -- which is the **BUILT, INSTALLED NOWHERE** shape, and
  `zhao_terrain_patch_acc` is already one instance of it. **Adding a second while
  claiming progress is worse than declaring the gap.**
* The guard window must open **WITH** its block on `TERRAIN.DEVSTORE`'s
  precedent, never ahead of it -- COMPOSEPUB's own stated reason for leaving the
  sibling windows shut.

## 8. DELIVERABLE 8 -- WHAT I GOT WRONG AND CAUGHT MYSELF

* **I computed the COMPOSED_MATERIAL break-even as 82 patches.** It is 129. I
  scaled the request count without dividing the headroom by the per-request cost.
  The error ran in the **pessimistic** direction, so it would have argued toward a
  refusal the owner has already declined once. Corrected in the decision record
  *and left visible there*, because an error that argues for less work is the one
  that needs a witness.
* **I hoisted a diagnostic to the wrong side of the gate that was firing.** I put
  the material-window counters above the SAMPLE gate to explain a shortfall, while
  the form I was debugging was fataling on the TAG gate thirty lines earlier -- so
  the run bought nothing and cost a full console build. This is FIELDACTIVE's own
  lesson reproduced by the person quoting it. The general form is worth more than
  the fix: **a diagnostic's position must be checked against WHICH gate is
  actually firing, not the one you were thinking about.**
* **I nearly shipped a literals-only materialisation** (2.2), which would have
  produced a green clause 3 on a constant token. Caught by measuring the
  provenance of every scalar slot BEFORE writing the code rather than after.
* **I briefly concluded the tag and the sample shortfall were two different
  causes**, on the grounds that `reserved` tracks the material and the shortfall
  does not. The counts (2 with `reserved=0`, 3 with `reserved=1`) are consistent
  with ONE cause and I corrected myself from the data.
* **My first heredoc into `bash -c` died on quote-heavy text**, exactly as
  PACKET-PROTOCOL warns; and a `sed`-then-`python` patch of my own helper script
  mangled it into a syntax error. Both fixed by writing files with an editor tool.
* **I ran a freshly built directed test without the toolchain environment** and
  got `0xC0000135` (DLL not found), which reads like a broken binary and is a
  wrong shell -- this tree's documented trap, in its `.exe` form.

## 9. GATES, AT THE PUSHED HEAD

| gate | result |
|---|---|
| `completion_register.py` **BARE**, python's own RC | **1** -- `I34`. **RC 1.** Unchanged from base `394cac06`. **I34's head line untouched.** |
| `check_console_inventory.py` | RC 0 |
| `check_prod_manifest.py` | RC 0 |
| `check_quartus17_syntax.py` | RC 0 |
| `check_case_labels.py` | RC 0 |
| `gen_prod_top.py --check` | **fresh** (89 instances) |
| `gen_console_board.py --check` | **FRESH** (1626 core ports, 127 parameters) |
| `gen_shell_paired_diff.py --check` | fresh |
| `check_console_closure_lint.py` (gate 31) | RC 0, **self-test fired 5/5** |
| `mutant_copy_drift.py` (**AFTER** the commits, ruling R121) | RC 0 -- 80 copies, no drift |
| `smoke_field_fixture_gen --check` | RC 0, and its new constant-pool check reports **11 LDC uops, 11 scalar reads each proven to follow its own LDC, material token base present** |
| `test_field_host_plan_directed` (R60, BUILT and RAN) | **235 checks, 0 failures, PASS** -- and the BASE COMMIT's lowerer, built in a throwaway directory, gives **235 / 0** too |
| console smoke, **PLAIN** | **PASS**, RC 0, at the pushed head: `pixels=2816`, `frames_admitted=1`, `fragments=1216 samples=1216`, `tile[max/or]=[6 7]`, `stray=0`, `reserved=0` |
| console smoke, `-FieldActive` | **RC 1**, on the fragment-tag gate (section 6). Everything above it passes, including `field_composed=1024 token_refused=0` and `tile[max/or]=[212 222]` |
| console smoke, `-FieldUncovered` | **RC 1**, on the texture-sample gate (section 6). Everything above it passes, including `pixels=2816`, `field_composed=0`, `token_refused=0` and `tile[max/or]=[6 7]`. Below it sits the inherited `lane_desync_o` red NOPROG measured |

**THE ROWS I DID NOT RUN, named rather than omitted.** `-Mutant`, `-BadVertex`,
`-NoEchoArm`, `-BadTraceArm`, `-TerrainFlatLattice`, `test_cmd_exec_directed`,
and the TypeScript compiler suite.

For the compiler suite the reason is specific and checkable rather than an
argument from effort: **`compiler/node_modules` is absent in this worktree**, so
the suite needs a network install; I changed **no TypeScript and no generated
program artifact**; and `material_program.test.ts`'s non-coincidence detector
parses five localparams out of the bench --
`SGF_TERR_MAT_A_LO_C`, `SGF_TERR_MAT_B_LO_C`, `TERR_MAT_SPAN_C`,
`SGF_TERR_WEIGHT_LO_C`, `TERR_WEIGHT_SPAN_C` -- and **`git diff 394cac06..HEAD`
over that file touches none of the five.** Verified by diff, not by memory.

For the console control forms: I changed no RTL and no port, and my bench edits
are `$display` additions plus one rewritten `$fatal` MESSAGE. **That is an
argument, not a measurement**, and I flag it as the row I am asking the
coordinator to close rather than quoting -- the same row NOPROG and MATFIELD both
flagged.

## 10. PROCESS

Other repositories' builds ran on this machine throughout; **nothing was
killed**. **No Quartus was run. No subagent was spawned** (PACKET-PROTOCOL's "a
packet does not spawn subagents"). I edited no `zhao_geom_*` file and did not
touch the shell. No `until ... sleep` poll loop was armed. Pushed only
`gz/lastgap`, never `--force`.
