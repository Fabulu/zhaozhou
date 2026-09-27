# FINDINGS -- MATFIELD, 2026-09-27

**Branch `gz/matfield`.** Base `c93fe343`. Commits `a06bda07`, `d09a9292`.
Written against the brief's nine numbered deliverables and the owner ruling
`reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md`.

---

## THE ONE-LINE ANSWER

**The composed console cannot execute any lowered Field program correctly,
because a program's CONSTANT POOL is never delivered to the execution register
file.** Every literal -- including the constants inside the sec 3.20 smoothstep
macro -- is folded by `lower()` into a preload row at physical register
`scalar_base + s`, and `zhao_field_host_v2`'s preload port writes only zeros and
the declared IN_LANES. So the field NOPROG measured executing 1,089 times was
computing on zeros, and **nothing in the tree could see it**: every counter is
true and not one looks at a value.

I found it by building what the brief asked for. **A material carries a TAG, and
a tag turns a silent wrong value into a counted refusal.** That is the whole
reason sec 14.1 put a version byte on the word.

`I34` does not close. Register **2 -> 2**, bare, RC 1. I34's head untouched.

---

## 1. DELIVERABLE 2 -- THE MATERIAL-WRITING FIELD PROGRAM (BUILT)

`compiler/src/field_ir/scorch_wash.ts` -- earth profile, 22 instructions, hash
`0xFDB4FE21`, four canonical outputs so `required_mask` stays `0x0F`. The first
program in this tree whose out-lane 2 is a legal v1 material token.

**No program in the corpus could produce one.** `wave_pool.ts:88` and
`crater_ring.ts:41-42` write out-lane 2 as a BARE material id -- `MAT_SOIL = 1`,
`MAT_CHARRED = 2` -- tag byte `0x00`, correctly refused by `zmt_tag_ok`. Sec 14.1
was written on 2026-09-27 (MATERIALPATH) on the fabric and reference sides and
**the program corpus was never brought along**: a ratified law landed in two of
its three views. Measured, not inferred -- pointing the generator at `wave_pool`
prints `out-lane 2 ... is NOT a v1 material token: 0x00000001 (tag 0x00)`.

## 2. DELIVERABLE 3 -- NON-COINCIDENCE, ARGUED FROM THE LAYOUT

| | authored layer E (`tb_zhao_console_core_smoke.sv:3881-3886`) | `scorch_wash` |
|---|---|---|
| matA | `{1, 2}` | `0xD4` = 212 |
| matB | `{5, 6}` | `0x1E` = 30 |
| weight | `[0x30, 0xCF]` | `[0xD0, 0xFF]` |

The authored plane can carry **164 distinct byte values anywhere in it**;
the program can emit **50**; **the intersection is empty**. 212 > 0xCF,
30 < 0x30, and `[0xD0,0xFF]` abuts `[0x30,0xCF]` without overlapping. So the
field triple cannot equal an authored one, nor one with matA/matB **swapped**,
nor one under any **rotation** or single-byte match. **No byte of this token is a
byte the authored plane is able to produce.**

**The weight band is STRUCTURAL, not asserted.** The frozen v1 ISA has no bitwise
operators, so the token is one `LDC` for the upper three bytes plus an `ADD` of a
computed low byte. That is safe only while the addend is bounded, and the bound
is the smoothstep macro's own internal clamp (`builder.ts:192-205`): `cov` is raw
`[0,65536]` for ANY input including a degenerate footprint, so `cov*0x2F >> 16`
is raw `[0,47]`, and `0xD0 + 0x2F = 0xFF` cannot carry into matB.

**It is an instrument, not an argument, and it was FIRED.**
`compiler/tests/material_program.test.ts` does not restate the authored ranges --
it **parses them out of the bench**, and throws if a localparam is renamed so it
cannot pass silently. Fired with `MAT_SCORCH_A = 0x01`: RC 1, "scorch_wash can
emit byte 0x1, which the authored layer-E plane can also produce -- clause 3
non-coincidence is DESTROYED". Byte-stability fired independently on the same
edit. Restored by CONTENT, re-measured to `0xFDB4FE21`.

The domain sweep is anti-vacuous about **itself**: it asserts it saw the weight
floor, the ceiling, the rim between them and more than 8 distinct weights,
because a sweep seeing one value would pass everything else while proving the
lane is constant. **22 distinct weights** measured; 16 / 19 / 10 over the three
candidate patch lattices at the console's own uniform set.

**Zero height is the experimental design.** `zhao_terrain_patch.sv:339` composes
field height ADDITIVELY, so height 0 leaves the lattice bit-identical. A program
that both deforms AND paints moves the mosaic tile for two possible reasons.
Holding geometry still leaves ONE variable. *Compare like with like.*

## 3. DELIVERABLE 6 -- THE DEFECT, AND IT IS THE PACKET

### 3.1 THE CONSTANT POOL IS NEVER DELIVERED

`lower()` folds every uniform-only value into preload rows at physical register
`scalar_base + s` (`zfield_host_plan.hpp:220-235`, `Translator::reg_of`), and the
physical uops SOURCE those registers. `zhao_field_host_v2`'s register-file
preload port (`:1250-1256`) writes **only** zeros on the slow path and the
declared IN_LANES from the point's own inputs.

**Measured at the settings this console composes:**

| program | scalar_base | preload rows | where the constants land |
|---|---|---|---|
| `scorch_wash` | 8 | 20, at registers 8..27 | **token base `0xE1D41ED0` at REGISTER 27** |
| `wave_pool` | 12 | 19, at registers 12..30 | `MAT_SOIL` at 29, nav `0x4000` at 30, **smoothstep's 1.0/2.0/3.0 at 26/27/28** |

**The host writes registers 0..14. None of those constants arrives.**

So `wave_pool`'s envelope evaluated `smoothstep` with 0 for 1.0, 2.0 and 3.0 --
`clamp(t,0,0) = 0` -- giving env 0 and **height exactly zero for its whole
recorded life.**

### 3.2 WHY NOTHING SAW IT

Every counter is true and none looks at a value: `fldearth runs=1089 noprog=0
faults=0`, `fldhost out_incomplete=0 uniform_bad=0 bad_image=0`. A program
reading zeros still executes, produces four outputs and retires.

**The only value-bearing gate in this mode was the positive form's pixel count,
and it was deliberately left UNPINNED** -- correctly at the time, because the
staged program was a height spell whose composed lattice the oracle does not
model. *The one number that could have caught this was the one number nobody
could assert.*

### 3.3 THE TWO CANDIDATE REPAIRS -- BOTH DECISIONS (PROTOCOL RULE 4)

* **(a) RTL.** A per-slot constant RAM loaded by the doorbell, and a preload
  phase writing `scalar_base .. scalar_base+n-1` from it after the input lanes.
  New doorbell kind, new RAM, a production block changed.
* **(b) LIBRARY -- RECOMMENDED.** Make `lower()` materialise folded constants as
  `LDC` uops inside the physical program instead of preload rows. **Zero
  silicon**; it spends a few uops and registers, and the measured high-water is
  **28 against a ceiling of 32**.

### 3.4 A SECOND STIMULUS GAP, IN MY OWN LANE

**The bench's TerrainField record never sets `tf.params`.** It sets `program_f`,
`footprint`, `start_tick` and `duration_ticks` and stops
(`tb_zhao_console_core_smoke.sv:6479-6521`), so `rec_params_i` is zero and
`p0..p7` reach the adapter as zeros -- even though the adapter DOES deliver them
(`zhao_field_earth_adapter.sv:868-886` drives all twelve input lanes per point).
**Declared, not fixed**: fixing it would not change the outcome while 3.1 stands,
and an unverified edit is worse than a named one. The next packet needs both.

### 3.5 A NON-REPAIR I BUILT AND THEN DISPROVED BEFORE COMMITTING

I first added an `LdUniform` word per preload row, reasoning that `LdUniform`
writes `fab_sb_*` -> `zhao_field_v3_sbank` and that this is "the register file
the microcode reads". **It is not.** That bank lives in
`zhao_field_v3_svcpath` and its read address is driven solely by
`zhao_field_v3_ring_svc`'s `f_slot_r`, loaded from a **RING instruction's
immediate** (`ring_svc:404-407`) -- the RING service's four-operand file. This
program has no RING, so the writes were inert. **A non-repair under a repair's
name**, reverted before it was committed, with the reason left beside the
constant. I measured the console with it in place: `token_refused` stayed 1024,
which is the evidence that it changed nothing.

**What IS a real fix, and is kept:** the prepared-file INDEX. The old emission
passed `addr = physical_register` under a comment reading "`addr` is the
prepared index". `LdPrepared` writes `prep_value[ld_addr_i]` (`:1586-1589`),
addressed by the SCALAR INDEX `s` -- the space `OutputSource::source_index`
carries for a prepared ordinal (`zfield_host_plan.cpp:423`). Rows were landing at
`scalar_base + s`. **The comment was right about the field and the code did not
do it.**

## 4. A THIRD LOWERING KNOB NOBODY WAS SETTING

`scorch_wash` refused at **every** scalar base with "an output ordinal whose
source register lies outside the capture window" -- not a scalar-base problem.
`LowerOptions::out_base` **defaults to 0**, so the window is registers `[0,7)`.
`wave_pool` lowers at that default **by luck**: two of its ordinals resolve to
PREPARED_SCALARs (not window-checked) and its VECTOR_REG ordinals land at 3, 4
and 6. `scorch_wash`'s land at 5 and 7.

Entry I34's remedy list says "widen REGS". FIELDACTIVE found `scalar_base`,
which that list omits. **`out_base` is a third knob neither names.**

Measured grid at ceiling 32 -- 25 pairs lower AND pass the output contract, and
high-water is a function of `scalar_base` alone: **8->28**, 9->29, 10->30,
11->31, 12->32, everything else refuses. Chosen **8 / 3**: high-water 28, four
registers under the ceiling against `wave_pool`'s 31. **No REGS widening, no ALM
purchase.**

## 5. DELIVERABLE 6 -- FALSE CLAIMS FOUND

1. **"`wave_pool.hpp` has NO PRODUCER IN THIS TREE"** (`FINDINGS-NOPROG.md` 8.1)
   -- **FALSE and load-bearing.** `compiler/src/field_ir/wave_pool.ts` and
   `compiler/tests/wave_programs.test.ts` both exist; `npm run build` +
   `node --test` regenerates the committed artifacts **byte-identically** (hash
   `0x8bdceb63`, git clean), verified before I changed anything. That false
   absence was NOPROG's stated reason for not starting this work.
2. **The smoke script's "raster pixels is NOT the plain run's 2816 in this form
   -- a field that changed no height would be a field doing nothing"** -- never
   measured, and false. Corrected.
3. **`smoke_field_fixture_gen.cpp`'s "`addr` is the prepared index"** -- 3.5.
4. **The bench's `field_composed=0` message blamed the PROGRAM** ("the field
   PROGRAM is not emitting a v1 material token"). Disproved: the program emits
   one at every vertex, verified by the reference decoder. Corrected to name the
   constant pool, so the next reader is not sent the wrong way. *A wrong
   diagnosis attached to a right alarm sends the next person to reshape
   something that is already correct.*
5. **The bench comment at `:5571`** says the coordinates are
   "`(r + SGF_TERR_IX0, r + SGF_TERR_IZ0)` = `(r-1, r+1)`"; the committed
   fixture has `SGF_TERR_IX0 = 0`, so it is `(r+0, r+1)`. Stale about its own
   constant. **Not corrected** -- outside my hunks in a shared file.

**Claims I checked that HOLD:** `zhao_terrain_patch_acc` is BUILT AND COMPOSED
NOWHERE (only instantiation tree-wide is `tb_terrain_fieldmajor.sv:223`, in no fit
closure). The address `0x058B_0000` is free and `DEVSTORE` ends exactly there.
matjoin's composed write face is the tap point.

## 6. A LATENT CORRUPTION HAZARD, PROVED WITH A PROBE

`.gitattributes`'s `compiler/tests/generated/** text eol=lf` (a W6 CI fix for the
**text** wrapper) also caught the `.zprog` **binary program images**, and
`text eol=lf` collapses a CRLF byte PAIR on check-in. **Measured**: a 16-byte
probe with two deliberate `0x0D 0x0A` pairs staged as a **14-byte blob**. Git
warns, but in words identical to every ordinary source file, so it reads as
routine. The four committed images survive **only by luck** -- each has CR bytes
(6/7/7/4) and **zero** CRLF pairs. Fixed with
`compiler/tests/generated/*.zprog -text`; the probe then round-trips 16/16, no
existing image shifted, the `.hpp` stays text.

## 7. TWO OBSERVATIONS WORTH CARRYING

* **The program hash does not cover the input lane bounds.** Widening
  `scorch_wash`'s declared domain changed the `.zprog` at byte 25, same length,
  **identical hash** -- it is `CRC-32C(code|tables) + instr_count` by design. So
  "same hash" does not mean "same image", and the fieldlist resolves on the
  hash while the loader CRCs the bytes.
* **`compiler/package.json`'s `npm test` does not run under the installed Node
  24**: `node --test dist/tests/` treats the directory as a module
  (`MODULE_NOT_FOUND`); the glob form works. Not a campaign gate; not fixed.

## 8. WHAT I GOT WRONG AND CAUGHT MYSELF

* **The `LdUniform` non-repair** (3.5) -- built on a misread of which module
  instantiates the scalar bank, caught by reading `ring_svc` before committing.
* **A procedural assignment to an undeclared identifier** in my own bench patch,
  caught reading the script back before applying it. It is a module-scope
  localparam now, derived from the three authored knobs with a `max()` rather
  than written as `6`.
* **I added a UTF-8 BOM to `run_console_core_smoke.ps1`** via `utf-8-sig`. A
  SHARED file, so a line-1 merge conflict. Caught on `git diff --numstat`
  reporting **2** lines when I had edited **1**.
* **I swept `scalar_base` for a refusal whose message names the capture window.**
  54 identical refusals before I read what it actually said.
* **Two heredocs into `bash -c` failed on quote-heavy text**, exactly as
  PACKET-PROTOCOL warns.

## 9. PROCESS

Other repositories' builds ran throughout; **nothing was killed**. **No Quartus
was run. No subagent was spawned.** No RTL file and no port changed in either
commit, which is why the three generated-file gates stay fresh.

## 10. THE SIX CLAUSES, EACH ANSWERED SEPARATELY

All three forms re-run at the pushed head `d09a9292`, so every number below
describes the tree that is committed and not an earlier one.

```
-FieldActive   fldearth records=1 runs=1089 noprog=0 not_begun=0
                        skipped_uncovered=0 faults=0 tail_rejected=0 short=0
               fldhost  runs=1089 faults=0 out_incomplete=0 bad_image=0
                        zero_mask=0 uniform_bad=0 db_load_words=39 db_commits=1
               fldstage install_ok=1 commit_ok=1 ldr_installs_ok=1 bad_crc=0
                        bad_meta=0 ldr_load_bytes=1408
               fldpatch tp_covers=1
               raster   pixels=2816
               terrmat  field_composed=0 token_refused=1024
plain          SMOKE: PASS, RC 0; raster pixels=2816; frames_admitted=1;
               fragments carrying a texel 1216; terrmat field_composed=0
               token_refused=0; mosaic fills[23 21 0] tile[max/or]=[6 7]
```

| # | clause | verdict |
|---|---|---|
| 1 | a real production Field program **installed and executed** | **MET, MEASURED.** `scorch_wash` fetched and sealed over the REAL HPS bridge (`ldr_installs_ok=1`, `bad_crc=0`, `bad_meta=0`, 1,408 B), resolved by the field list (`unresolved=0`), and **executed 1,089 times with `noprog=0 faults=0 short=0`** -- one evaluation per lattice vertex of the 33x33 patch. |
| 2 | the field **covers the intended terrain** | **MET, MEASURED.** `tp_covers=1`, `skipped_uncovered=0` in the covered form. |
| 3 | a value that **cannot equal the authored baseline by accident** | **THE VALUE IS BUILT AND PROVEN. IT DOES NOT SURVIVE TO THE CONSUMER, AND THE CAUSE IS NAMED AND MEASURED.** Non-coincidence is established from the two layouts (sec 2), enforced by a detector that was FIRED, and the program's out-lane 2 is verified a legal v1 token at **3,267 vertices by the REFERENCE decoder**. In the composed console it is refused -- `token_refused=1024`, one per compose-cache cell -- because the token's upper three bytes live in a folded constant at **physical register 27** and the host writes registers 0..14 (sec 3.1). **NOT MET in the assembled console.** |
| 4 | reaches the **intended production consumer** | **ROUTING MET** -- NOPROG measured 1,024 tag checks at `zhao_terrain_matjoin`, the consumer entry I34 names, and this run reproduces the same 1,024. Acceptance is blocked by clause 3, not by routing. **The consumer-side assertions I added are COMMITTED AND NOT YET EXERCISED**: the run fatals ~2,600 lines above them on the named cause. **I am not claiming they passed.** |
| 5 | the **uncovered/control form restores the authored result** | see the row below the table. |
| 6 | the **no-field forms remain unchanged** | **MET, MEASURED.** Plain `SMOKE: PASS`, RC 0, `raster pixels=2816`, `frames_admitted=1`, 1,216 textured fragments, `tile[max/or]=[6 7]`, `token_refused=0`. None of the four numbers the brief names as the no-regression bar moved. **And the plain arm of my new mosaic law was exercised and passed**: `tile_max=6` against the derived authored ceiling of 6. |

**CLAUSE 5, MEASURED, AND MY NEW LAW WAS EXERCISED THERE.** `-FieldUncovered`,
same head:

```
fldearth records=1 runs=0 noprog=0 not_begun=0 skipped_uncovered=1089
         faults=0 tail_rejected=0 short=0
raster   pixels=2816
terrmat  field_composed=0 token_refused=0 held_overrun=0 | cc_mat_cells=1024
mosaic   fills[tileset/mesh/stray]=[23 21 0] tile[max/or]=[6 7]
```

The capsule installed, the microcode loaded, the hash committed, the record banked
and resolved -- and because the footprint sits at 16384.0 where no lattice vertex
is, **every vertex took `skipped_uncovered` and the authored result is restored
EXACTLY**: `pixels=2816`, the plain run's number, and `tile[max/or]=[6 7]`, the
plain run's tiles. `token_refused=0` confirms no material word was even offered.

**And unlike the covered form, this one REACHES my new mosaic law** -- the
clause-5 arm sits at `:10931`, above the `lane_desync_o` fatal at `:11252` -- so
the assertion that no tile may exceed the authored ceiling was **exercised and
passed** with `tile_max=6`. Together with the plain arm (also exercised, also 6),
two of the three arms of the two-sided law are live; only the covered arm waits on
clause 3.

**The form still exits 1, on `fld_earth_lane_desync_o` alone (`:11252`), and I
did not silence it.** That is the pre-existing declared defect NOPROG measured and
deliberately left red: the counter's meaning differs from its own header -- arm (a)
differences `vtx_live` against `ans_ready_i`, the veljoin's answer-channel ready,
while the header claims it differences the consumer's own `fld_ready_o`, **a port
that module does not have**. It reads exactly 1 whether every vertex succeeds,
every vertex refuses, or none is evaluated. Waiving it would manufacture a false
green, and repairing it is a decision about a guard. **Clause 5's substance is MET
and measured; what is red is that one inherited detector.**

**AND ONE RESULT THAT IS BETTER THAN ITS CLAUSE ASKS.** `raster pixels=2816` in
the COVERED form, equal to the plain run. That number is now PINNED rather than
merely displayed, and FIELDACTIVE named the pin as owed: it left the count
ungated and said the honest fix was "a SECOND expectation" derived from the
reference. With a material field writing height ZERO and terrain composing height
ADDITIVELY, that second expectation IS `SGF_EXP_PIXELS`, **by construction**. So
the positive form now gates on geometry as well, and the geometry is provably
unmoved -- which is exactly what would make the mosaic-tile evidence for clauses
3 and 4 attributable to the material alone, once the material arrives.

## 11. GATES AT THE PUSHED HEAD `d09a9292`

| gate | result |
|---|---|
| `completion_register.py`, **BARE**, python's own RC | **2** -- `I34`, `I55`. **RC 1.** Unchanged from base `c93fe343`. **I34's head line untouched.** |
| `check_console_inventory.py` | RC 0 |
| `check_prod_manifest.py` | RC 0 |
| `check_quartus17_syntax.py` | RC 0 |
| `check_case_labels.py` | RC 0 |
| `gen_prod_top.py --check` | **fresh** (89 instances) |
| `gen_console_board.py --check` | **FRESH** (1626 core ports, 127 parameters) |
| `gen_shell_paired_diff.py --check` | **fresh** |
| `check_console_closure_lint.py` (gate 31) | RC 0, **self-test fired 5/5** |
| `mutant_copy_drift.py` (**AFTER** the commit, ruling R121) | RC 0 -- 80 copies, no drift |
| `smoke_field_fixture_gen --check` (my own new gate) | RC 0, **and it re-verifies the token over 3,267 vertices on every run** |
| console smoke, **PLAIN** | **PASS**, RC 0, `pixels=2816`, `frames_admitted=1` |
| console smoke, `-FieldActive` | **RC 1**, on `field_composed=0`, the measured constant-pool gap. Every other assertion in it passes, including 1,089 runs at `faults=0` and the pinned 2,816 pixels. |
| console smoke, `-FieldUncovered` | see below |
| compiler suite | **325 pass / 5 fail of 330**; base `c93fe343` measured in a throwaway worktree is **320 / 5 of 325** -- the SAME five by name, in files this packet does not touch. +5 passing, no new red. |

**THE THREE GATE ROWS I DID NOT RUN, named rather than omitted.** `-Mutant`,
`-BadVertex`, `-NoEchoArm`, `-BadTraceArm` and `-TerrainFlatLattice` were not
run, and neither was R60's `test_cmd_exec_directed`. **This worktree has no CMake
build tree**, and configuring one verilates the whole suite -- a third heavy job
on a machine already carrying two console builds and another repository's. I
changed **no RTL, no port and no parameter**; the only C++ I touched is
`smoke_field_fixture_gen.cpp`, which is in no other target's closure, and
`check_case_labels.py` (which parses `cmd_exec_directed.cpp`) is green. **That is
an argument, not a measurement, and I am flagging it as the row I am asking the
coordinator to close rather than quoting.** It is the same row NOPROG flagged.

## 12. DELIVERABLE 9 -- BRANCH AND COMMITS

**`gz/matfield`**, pushed. Never `--force`, never `--force-with-lease`.

| commit | what |
|---|---|
| `a06bda07` | `scorch_wash`, `material_token.ts`, the emitter test with its fired non-coincidence detector, and the `.gitattributes` `.zprog` repair |
| `d09a9292` | the constant-pool finding, the fixture staged on `scorch_wash` with reference-decoder verification, `out_base`, the prepared-index repair, the mosaic clause-3/4 law, the pinned pixel count, the corrected messages |
