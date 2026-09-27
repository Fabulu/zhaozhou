# FINDINGS -- NOPROG, 2026-09-27

**Branch `gz/noprog`.** Base `a201825d`.
Written against the brief's nine deliverables and the owner ruling
`reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md`.

---

## 1. THE NOPROG MECHANISM, MEASURED -- AND IT IS NOT WHAT THE BRIEF SAID

The brief, and `FINDINGS-FIELDACTIVE.md` before it, attributed the composed
console's `noprog=1089` to `zhao_field_earth_adapter`'s documented
intake-versus-replay race, on the fingerprint (1,089 = 33x33) that the adapter's
own header records **verbatim**. That attribution is **wrong**, and the counter
it rests on **could not have said otherwise**.

### 1.1 The counter cannot attribute the refusal

`fld_earth_noprog_o` does **not** count the adapter's own binding flag. It
increments in the adapter's `E_WAIT`, on the **engine's** reply:

```systemverilog
// zhao_field_earth_adapter.sv:1425
if (resp_status_i == StNoProgram) begin
  if (noprog_o != 32'hFFFF_FFFF) noprog_o <= noprog_o + 32'd1;
```

and the engine's `0xF0` has **two** independent causes, ORed into one wire:

```systemverilog
// zhao_field_host_v2.sv:1306
wire gnoprog_c = req_noprog_i[pick_id] || !hdr_loaded[gslot_c];
```

The adapter's own header says so at `:846-849` -- *"a lane whose handle resolved
to no ready object and a lane whose object holds no header BOTH come back 0xF0
and are counted in one place."* So **neither `fldearth noprog` nor `fldhost
noprog` can say which cause fired**, and "the adapter's residency flag is the
only thing that disagrees" was an assumption the instrument structurally cannot
support. That is CLAUDE.md's own law, in the very block the brief sent me to.

### 1.2 What settled it cost no port at all

`terr_fl_replays_o`, `terr_fl_entries_replayed_o` and `terr_fl_open_at_patch_o`
have been **core ports and bench wires since the fieldlist was composed, and
nothing had ever read them**. The brief said *"the adapter exports no evidence
for the replay side"* -- true of the **adapter**, false of the **list beside
it**. Printing them:

```
SMOKE: fldlist records=1 sealed=1 unresolved=0 tail_rejected=0
               replays=1 entries_replayed=1 open_at_patch=0 idle=1
```

* **`open_at_patch=0`** -- **no patch job was ever taken while the list was
  open.** The brief's central premise, *"this console has a job pending FIRST"*,
  is **MEASURED FALSE**. The ordering the intake-versus-replay race requires
  never occurred in this console.
* **`entries_replayed=1`** -- the replay **did** reach the adapter, so its
  `b_res` was written from a resolved entry (`unresolved=0`).
* **`replays=1`** -- no second job replayed over the first one's binding.

**The adapter's binding was correct throughout.** It is not the defect, and the
"ONE NARROWED RESIDUAL" its header declares is not what this console hit.

---

## 2. THE REAL DEFECT: TWO PRODUCTION GUARDS THAT PULL OPPOSITE WAYS

Both are deliberate, both carry their reasons beside them, and **no single
ordering of one staging pass satisfies both.**

| guard | says |
|---|---|
| `zhao_field_doorbell.sv:429`  `head_refuse = head_is_commit && !head_hdr_ok` | a COMMIT for a slot whose HEADER was never written is REFUSED without touching the directory. **Header BEFORE commit.** |
| `zhao_field_host_v2.sv:1448`  `if (pc_cm_valid_i && pc_cm_ready_o && pc_cm_ok_i) hdr_loaded[pc_cm_slot_c] <= 1'b0;` | a successful insert invalidates the slot's program and init proof, because *"the directory has promised the slot to a new hash"*. **Header AFTER commit.** |

The underlying law is `zhao_field_progcache`'s own header: *"Phase B, COMMIT:
only after a miss ... insert into the first free slot, else evict the
least-recently-used entry"*, answering INSERTED **with a slot**. **The commit is
the allocator**, which is why the host invalidates on it.

### 2.1 Both directions MEASURED on the composed console

| staging order | result |
|---|---|
| INSTALL -> LOADs (header last) -> COMMIT *(FIELDACTIVE's)* | commit OK, `hdr_loaded` cleared, every request 0xF0: **`fldearth runs=0 noprog=1089`**, `fldhost runs=0 noprog=1131 grants=1131` |
| INSTALL -> COMMIT -> LOADs (header last) *(my first attempt)* | doorbell law 2 **refuses the commit**, `fld_commit_ok_q` stays 0, the frame gate never opens: **`dma_done=0`**, *"GEOM.REPLAY released no meshlet"* |

The second row is mine and I record it as **a wrong call I made and caught** -- I
read the host's law, did not read the doorbell's, and reordered on half the
evidence. It cost one run, and the contradiction is now measured in both
directions rather than argued in one.

### 2.2 The resolution that removes no guard

Header LAST among the load words (satisfies the doorbell) -> COMMIT allocates
the slot -> **the header load word is RE-POSTED** (satisfies the host). The
doorbell clears its own `hdr_written` shadow on the same insert (`:684`), so the
two flags stay in step and the re-post lifts both. The re-posted word is the
generator's own word 42, not anything the bench composes.

**Nothing is narrowed and no law is relaxed.** That the recovery works is pinned
at the leaf rather than assumed -- see section 3.

### 2.3 THE CONTRADICTION IS NOT FIXED, AND I AM DECLARING IT

Reconciling it belongs in production RTL: either the doorbell's law 2 admits a
commit that **allocates** for a not-yet-loaded slot, or the host stops
invalidating an insert whose hash is the one already in that slot. **Both change
a guard, so both are decisions rather than repairs**, and a bench is the wrong
place to take either.

**Recommendation: the doorbell's law 2 is the one to move.** Its stated purpose
is to stop a commit referencing a slot with no program; an allocation for a slot
**about to be** loaded is exactly the legitimate case it currently forbids, while
the host's invalidation is the guard that protects a genuine eviction.

**Until then, every HPS staging a field program must write the header twice**,
and nothing in the tree said so before this packet. It is now written in the
bench and pinned by `FT029`.

---

## 3. THE INSTRUMENTS ADDED, AND THAT THEY DISCRIMINATE

* **`FT029` in `tests/field/field_host_v2_directed.cpp`** -- the leaf test no
  bench held. **The whole of `tests/field/` loads programs and never commits**,
  which is precisely why no component test could reproduce this -- and it is a
  different reason from the one the brief gave. It asserts the **correct**
  behaviour in both directions: (3) a successful insert un-loads the slot;
  (4) re-writing the HEADER ALONE restores it, with the microcode, output map
  and association shown to have survived. A test asserting only (3) would pass
  while the ordering remained unusable. It asserts the allocated slot is 0
  rather than assuming it.
* **The three unread fieldlist counters**, now printed (section 1.2).
* **`fld_commit_slot_q`** -- the slot the directory actually allocated, latched
  from the return and asserted against `SFF_SLOT`. **Its reset value is 7, not
  0**, so "never latched" cannot read as a pass.
* **`fld_commit_inserted_q`** -- asserts the commit INSERTED, not merely that it
  was answered `ok`. A law-2 refusal is also answered, so the old assertion
  could not tell an insert from a refusal.
* **The frame gate now waits on `fld_db_load_words_o >= SFF_N_LOAD + 1`** -- the
  HOST's count of load words CONSUMED, not posts the mailbox ACCEPTED. This is
  the same ACCEPTED-IS-NOT-COMPLETED defect FIELDACTIVE recorded one door along.

---

## 4. FALSE CLAIMS FOUND IN WHAT I WAS HANDED

1. **"`I34` classifies on a regex for the word BOUNDARY, so editing it moves the
   number."** -- **FALSE**, and it is in both my brief and the OWNER DECISION
   document. `completion_register.py` sets
   `t["mandatory_gap"] = t["kind"] != "resolved-in-composer"`. The `BOUNDARY`
   regex only selects the **printed label**; removing the word changes
   `boundary` to `unclassified` and **the entry still counts as a gap**. The
   phrase that actually settles an entry is `NOT a tie-off`, **in the head**,
   and the tool already hard-fails if it appears only in a body. The forbidden
   shortcut is real; it is a different string, and a packet told to avoid the
   wrong one could take the right one by accident.
2. **The `_BOUNDARY` regex is currently matching a STRUCK QUOTATION** of the
   entry's own superseded head. The classifier is reading prose *about* a word.
3. **"This console has a job pending FIRST."** -- **MEASURED FALSE**,
   `open_at_patch=0`. This was the brief's stated reason no leaf bench caught
   the defect; the real reason is in section 3.
4. **"The adapter exports no evidence for the replay side."** -- true of the
   adapter, **false of `zhao_terrain_fieldlist` beside it**, whose three replay
   counters were already core ports. The named next diagnostic (a new adapter
   port) was not needed.
5. **"`noprog` incremented exactly once per lattice vertex, which means the
   patch's field lane fired per vertex."** -- the conclusion happens to be true
   and the inference is invalid: `noprog` counts the ENGINE's reply, so its
   value says nothing about `add_fire_i`. `entries_replayed=1` is what says it.
6. **FIELDACTIVE's "1089 + 42 = 1131"** -- correct arithmetic that
   discriminates nothing: the sum holds identically under both causes of
   `gnoprog_c`, so it was quoted as evidence for a hypothesis it cannot
   distinguish.

---

## 5. WHAT I GOT WRONG AND CAUGHT MYSELF

* **I reordered on half the evidence** (section 2.1). I read the host's law and
  the allocator's, concluded "commit first", and did not read the doorbell
  before changing the order.
* **The frame gate had to move with the reorder**, and I nearly shipped it
  without: once the commit runs first, `fld_commit_ok_q` no longer implies the
  program is loaded.

---

## 6. THE REGISTER, MEASURED BARE

| | value |
|---|---|
| base `a201825d` | **2** -- I34, I55. python RC 1 |

**I did not touch `I34`'s prose.**

---

## 7. THE SIX ACCEPTANCE CLAUSES, EACH ANSWERED SEPARATELY

The owner's ruling makes this a checklist, not a summary. Measured at
`-FieldActive` on this head:

```
fldearth records=1 runs=1089 noprog=0 not_begun=0 skipped_uncovered=0
         faults=0 tail_rejected=0 short=0
fldhost  runs=1089 noprog=42 grants=1131 out_incomplete=0 bad_image=0
         zero_mask=0 no_result=0 uniform_bad=0
         db_posts=48 db_load_words=44 db_commits=1 db_commits_refused=1
fldlist  records=1 sealed=1 unresolved=0 replays=1 entries_replayed=1
         open_at_patch=0
fldstageB commit_slot=0 commit_inserted=1
terrmat  field_composed=0 token_refused=1024
```

| # | clause | verdict |
|---|---|---|
| 1 | a real production Field program is **installed and executed** | **MET, MEASURED.** `wave_pool`, a real lowered Earth program, fetched and sealed over the real HPS bridge (`ldr_installs_ok=1`, `ldr_load_bytes=1536`, `bad_crc=0`, `bad_meta=0`), resolved by the field list (`unresolved=0`), and **EXECUTED 1,089 times with zero faults**. It was `runs=0` before this packet. |
| 2 | the field **covers the intended terrain** | **MET, MEASURED.** `tp_covers=1`, one lane offered per lattice vertex of the 33x33 patch, `skipped_uncovered=0` in the covered form and `1089` in the control. |
| 3 | the material write produces a value that **cannot equal the authored baseline by accident** | **THE MECHANISM IS FOUND, CORRECT, AND MEASURED FIRING -- IN THE REFUSAL DIRECTION. The positive half is NOT MET.** See 7.1. |
| 4 | that composed material **reaches the intended production consumer** | **ROUTING MET AND PROVEN LIVE.** The word reaches `zhao_terrain_matjoin`'s tag check **1,024 times** -- once per compose-cache cell -- which is the consumer entry I34 names (`f_material_i` -> layer E -> the mosaic). Acceptance is blocked by clause 3, not by routing. |
| 5 | the **uncovered/control form restores the authored result** | **MET, MEASURED.** `-FieldUncovered` differs in ONE field of ONE record (the footprint moves to 16384.0): `runs=0`, `skipped_uncovered=1089`, `noprog=0`, `token_refused=0`, and **`raster pixels=2816` -- exactly the plain run's authored baseline.** |
| 6 | the **no-field forms remain unchanged** | **MET, MEASURED.** Plain smoke `SMOKE: PASS`, `raster pixels=2816`, `frames_admitted=1`, `texture fragments=1216`, RC 0. |

### 7.1 CLAUSE 3, HONESTLY

`token_refused=1024` is **`zhao_material_token_pkg`'s tag check doing its job**,
not a routing fault. `zmt_tag_ok` requires `[31:24] == 8'hE1`, and that tag
exists precisely so an undecodable word is refused rather than silently
composed: every 24-bit pattern is a LEGAL material (`spec/terrain_rules.md`
sec 6.2 gives weight 0 and 255 meanings), so without a tag a stuck bus or an
unwritten lane would render. **The tag IS clause 3's anti-vacuity device**, and
this is the first time in this console's life that it has been exercised at all.

**What is missing is a field PROGRAM that emits a v1 material token.** The
ratified law is that the program's out-lane 2 **is** the token, tag included
(`zref::fieldir::material_token_encode`, `reference/include/zref/zref_fieldir.hpp:144`).
`wave_pool` is a HEIGHT spell: `compiler/tests/generated/wave_pool.hpp` sets
`out.material = outputs[2]`, a plain register value, so its ordinal 2 carries no
tag and is **correctly** refused.

So clause 3's positive half is **a BUILD, not a decision** -- and the build is a
field program, not RTL. It is declared in section 8.

---

## 8. THE REMAINDER, DECLARED RATHER THAN TRIMMED

### 8.1 A MATERIAL-WRITING FIELD PROGRAM (clause 3's positive half)

Needed: an Earth program whose ordinal 2 is `{8'hE1, matA, matB, weight}`. Then
`field_composed` moves and clauses 3 and 4 close together, with no RTL change at
all -- the whole path from the adapter's `material_o` to the join's tag check is
already composed and now proven live 1,024 times.

**`wave_pool.hpp` has NO PRODUCER IN THIS TREE.** `compiler/tests/generated/`
holds `wave_pool`, `crater_ring` and `impact_wave` as committed `.hpp` + `.zprog`
artifacts, and no CMake target, script or `.form` source that regenerates them
appears anywhere. So this is a Nanquan-side act, and the standing direction is
*hardware first, Nanquan is provisional* -- which is why I did not start it.

**AND THE GENERATOR CITES A FILE THAT DOES NOT EXIST.**
`tests/prod/smoke_field_fixture_gen.cpp:42` says *"WAVE_POOL IS A REAL SHIPPED
SPELL (`spells/membrane.form`)"*. **There is no `spells/` directory anywhere in
the tree**, and `find . -name '*.form'` returns only the compiler's own test
corpus. The claim is a FALSE PRESENCE -- handover section 6's "number fifteen"
shape -- and it matters because the next packet will go looking for that file.

### 8.2 `TERRAIN.COMPOSED_MATERIAL` -- NOT BUILT

The owner commissioned it and I did not build it. The noprog chase and its two
production findings took the packet. Declaring it, with what I established so the
next packet does not re-derive it:

* **The consumer question is ALREADY ANSWERED, and differently from HEIGHT and
  VELOCITY.** `zhao_terrain_matjoin` **already consumes the field's material** --
  `f_material_i`, overriding the authored layer-E triple and counting
  `field_composed_o` -- and that is composed in the console today. COMPOSEPUB
  refused the sibling regions on *measured consumer-absence*; **that argument
  does not carry here**, because this channel's fabric consumer exists and is now
  proven live (1,024 tag checks). What `COMPOSED_MATERIAL` adds is a PUBLICATION
  DESTINATION, which is a different question from whether anything reads the
  channel.
* **The bandwidth record itself commissions the measurement the owner wants.**
  `spec/memory_rules.md` (COMPOSEPUB's decision record) says: *"The real driver
  is the dirty set ... at dirty fraction d the cost is d x the row, and the
  break-even against the ledger's own 19.83% headroom is d ~ 0.45 for the write
  alone and d ~ 0.20 once the re-stage read is counted. **The packet that builds
  the publisher owes that fraction measured on a real scene.**"* That is the
  number to measure, and `terr_pt_subpatch_dirty_o` plus `field_composed_o` are
  the instruments -- **`field_composed_o` is exactly the composed-material dirty
  count**, so the fraction is measurable the moment 8.1 lands.
* The address is free and uncontended: `spec/memory_rules.md:306` has
  `0x058B_0000 .. 0x05FF_FFFF` reserved, and `TERRAIN.DEVSTORE` ends exactly at
  `0x058B_0000`. DEVSTORE (`:362`) is the enactment pattern: a region row,
  `zhao_pkg` `_BASE`/`_SPAN` agreeing to the byte, and MEM.GUARD arms -- *"a
  window opened WITH its block, never ahead of it."*
* `zhao_terrain_patch_acc` is still BUILT AND COMPOSED NOWHERE.

**I am NOT refusing it, and I am not repeating the refusal the owner declined.**
The owner's own sequencing applies: the destination cannot be measured
meaningfully until something composes material at all, and as of this head
nothing does. 8.1 is the prerequisite and it is one program.

### 8.3 THE DOORBELL / HOST ORDERING CONTRADICTION (section 2.3)

Unfixed, and it is a DECISION about a guard.

### 8.4 THE PROGRAM-CACHE WRONG-SLOT INVALIDATION

**A production defect I found, measured, and did not fix.**
`zhao_field_progcache` assigns `cm_slot_o <= victim` NONBLOCKING on the commit's
fire cycle; `zhao_field_host_v2:1448` reads `pc_cm_slot_c` ON that same cycle. So
a successful insert clears **the previous commit's slot**, not the one it just
allocated.

Measured with a two-commit form of FT029: the second insert returned slot 1,
`hdr_loaded[1]` stayed SET, the request came back `0x00` where the law says
`0xF0`, and `noprog_o` was one short (expected 3, got 2).

It is **latent** today: the reset value is 0 and the console's only commit
targets slot 0, so the stale read happens to name the right slot. It bites the
first time two different slots are committed in sequence -- i.e. the first time
the console stages a second field program.

CLAUDE.md's metadata-swap shape exactly: *a flag and the record it describes
loaded by different enables*, invisible while the two operands agree.

**Why I did not fix it:** repairing it edits production RTL and stales three
committed `zhao_field_host_v2_*` mutant copies, which needs three-way merges this
packet could not carry honestly on top of everything above. **Recommended fix:**
clear on the settled slot -- gate the invalidation on the commit RESPONSE
handshake (`pc_cm_resp_valid_o && pc_cm_resp_ready_i && pc_cm_inserted_o`), by
which time `pc_cm_slot_c` is the slot that was actually allocated.

---

## 9. THE COST, MEASURED

**ZERO added silicon. No RTL file and no port changed in this packet.**

That is not an estimate and it is not an omission -- it is the measurement, and
three gates are its proof rather than my word:

| proof | reading |
|---|---|
| `git diff --stat a201825d..HEAD` | touches `tests/` and `runs/` only; no file under `fpga/rtl/` |
| `gen_prod_top.py --check` | **fresh** -- it regenerates from the block ports, so a port change makes it stale |
| `gen_console_board.py --check` | **fresh** -- it goes red whenever a core port changes, by design |

The repair was a STAGING ORDER, so the console runs its first field program at no
ALM, DSP or M10K cost at all. **No fit was launched**, per the owner's
instruction, and none is owed by this change: there is nothing new to place.

The costs that ARE owed are owed by the two declared items -- 8.1 (a program, so
no fabric cost) and 8.2 (a region and a writer, whose dirty-fraction cost is the
number COMPOSEPUB's own record commissions, per 8.2).

---

## 10. GATES, AT THE COMMIT I PUSH

| gate | result |
|---|---|
| `completion_register.py` (BARE, python's own RC) | **2** -- I34, I55. **RC 1.** Unchanged from base `a201825d` |
| `check_console_inventory.py` | RC 0 |
| `check_prod_manifest.py` | RC 0 |
| `check_quartus17_syntax.py` | RC 0 |
| `check_case_labels.py` | RC 0 |
| `gen_prod_top.py --check` | fresh |
| `gen_console_board.py --check` | fresh |
| `gen_shell_paired_diff.py --check` | fresh |
| `check_console_closure_lint.py` (gate 31) | RC 0 |
| `mutant_copy_drift.py` (**AFTER** the commit, ruling R121) | RC 0 -- 80 copies, no drift |
| `test_cmd_exec_directed` (R60, BUILT and RAN) | **977 checks**, RC 0 |
| `test_field_host_v2_directed` (R60, BUILT and RAN, includes FT029) | **170 checks**, RC 0 |
| console smoke, PLAIN | **SMOKE: PASS**, `raster pixels=2816`, `frames_admitted=1`, RC 0 |
| `-FieldUncovered` | **RC 1**, on `lane_desync_o` alone -- see section 13. Clause 5's substance is MET and measured; I did not silence the detector to make the form green |
| `-FieldActive` | **RC 1 BY DESIGN AT THIS HEAD** -- it fatals on `field_composed=0`, which is clause 3's positive half and is declared in 8.1. Every other assertion in it passes, including the 1,089 runs. |
| `-FieldActive` / `-FieldUncovered` / plain lint | RC 0 each |

I did not run `-Mutant`, `-BadVertex`, `-NoEchoArm`, `-BadTraceArm` or
`-TerrainFlatLattice`. I changed no RTL and no port, and the only bench code I
touched is inside `ifdef ZHAO_SMOKE_FIELD_ACTIVE` except for three assertion
counts that are now `ifdef`-split with the plain arm **byte-identical** to
before. **That is an argument, not a measurement**, and I am flagging it as the
one gate row I am asking the coordinator to close rather than quoting.

---

## 11. THREE ASSERTIONS IN ONE BLOCK HAD NEVER BEEN REACHED

`-FieldActive` and `-FieldUncovered` were built by FIELDACTIVE and **the positive
gate fatals around line 8090**, so roughly 2,200 lines of assertions below it had
never executed in either form. Three of them were absolute censuses written for
the no-program form, and all three were wrong in the new modes:

| assertion | plain | the new forms |
|---|---|---|
| `fld_db_posts_o != 2` | 2 probes | **48** = 2 probes + install + 43 load words + commit + re-posted header |
| `fld_ret_seen_q != 2` | 2 | **4** -- and NOT 48. A LOAD WORD IS NOT ANSWERED: `zhao_field_doorbell`'s `D_LOAD` advances the queue and writes no return record. Posts CONSUMED and posts ANSWERED are different populations, and I got this wrong first by reusing the count above |
| `fld_db_commits_o != 0` | 0 -- the only commit is the probe's law-2 refusal, which never reaches the directory | **1** -- the staged program's real insert |

**The general shape, and it is this campaign's own:** a new mode that fatals
early leaves every assertion after the fatal unexercised, and those assertions
can be wrong *for that mode* without anything going red. The mode looks built and
its tail has never run. Each one cost a full console run to find, one at a time,
because the first fatal hides the next.

---

## 12. WHAT ELSE I GOT WRONG AND CAUGHT MYSELF

Beyond section 5:

* **I edited the bench while a `-FieldActive` run was verilating** -- the
  live-tree trap CLAUDE.md documents in as many words. I stopped that run rather
  than read it, and verified no orphan children by command line before assuming
  the lane was closed. Its result is discarded, not quoted.
* **I splatted a switch through `powershell -File`** and got
  *"Der Wert System.String kann nicht in den Typ SwitchParameter konvertiert
  werden"* on both field forms -- two wasted invocations. My own brief warns
  about exactly this ("a switch passed as a quoted string becomes POSITIONAL").
* **I normalised `tb_zhao_console_core_smoke.sv` to CRLF** when it is natively
  all-LF, which would have turned a 33-line diff into an 11,000-line one. Caught
  it on `git diff --stat` in the same minute and reverted. `field_host_v2_directed.cpp`
  IS natively CRLF, which is what made the mixed-ending problem real in the first
  place.
* **My first FT029 placed the case last and used slot 0**, which the BADIMAGE
  case poisons on purpose. I read its `0xF0` as the case's own result for one
  build.

---

## 13. `lane_desync_o` READS 1 IN EVERY FIELD FORM, AND ITS DOCUMENTED OPERAND
## DOES NOT EXIST

FIELDACTIVE recorded this and honourably declined to call it clean:

> *"`fld_earth_lane_desync_o` read 1 on the runs where the program was not
> resident ... It MAY be a consequence of the 1,089-vertex noprog path rather
> than a defect, but I have not proven that."*

**THAT HYPOTHESIS IS NOW MEASURED FALSE.** `desync` reads exactly **1** in three
runs whose field behaviour has nothing in common:

| run | field behaviour | desync |
|---|---|---|
| before the repair | `runs=0 noprog=1089` | **1** |
| after the repair | `runs=1089 noprog=0 faults=0` | **1** |
| `-FieldUncovered` | `runs=0 skipped_uncovered=1089` | **1** |

It is 1 when every vertex refuses, 1 when every vertex succeeds, and 1 when no
vertex is evaluated at all. It is therefore **not** a consequence of the noprog
path; it is a single edge event, invariant across the whole population.

### AND ARM (a) IS NOT WIRED TO WHAT ITS HEADER SAYS

`zhao_field_earth_adapter.sv:112` and `:1156` both say arm (a) differences this
module's belief that lanes remain against

> *"the CONSUMER's own `fld_ready_o`, which is its internal `busy`, loaded by its
> own state machine from its own vertex accept."*

**THE MODULE HAS NO SUCH PORT.** `fld_ready` occurs nine times in that file and
**every one is a comment**; there is no `fld_ready_i` in the port list. Arm (a)
is `vtx_live != ans_ready_i`, and the console wires
`.ans_ready_i(tvj_a_ready)` (`zhao_console_core.sv:29881`) -- the **veljoin's**
ready on the ANSWER channel, which is a different block with a different
cadence from `zhao_terrain_patch`'s `busy`.

So the detector's own justification -- *"Nothing loads both ... so this is not
the blind detector that chapter is about"* -- describes a comparison this module
cannot make. Arm (a) is UNGATED and fires on any single cycle where the answer
channel's ready disagrees with `vtx_live`, which is a fact about **another
block's handshake** -- precisely what the same header says two hundred lines
earlier that this file *"must not quietly depend on"*.

**This is the fourth false claim found in an RTL header this packet, and the
first one where the header's argument for a detector's soundness is the part that
is wrong.** It is not the flattering direction for once -- it produces an alarming
1 rather than a reassuring 0 -- but the cost is the same: the number cannot be
interpreted, so nobody can tell a real lane desync from a one-cycle cadence skew.

### WHAT I DID ABOUT IT: NOTHING, DELIBERATELY, AND IT IS WHY `-FieldUncovered`
### EXITS 1

Two repairs are possible and **both change a guard, so both are decisions**:
give the adapter the patch's `fld_ready_o` as a real port and difference against
it (a port change, four instantiation sites plus two console mutant copies), or
re-scope arm (a) to the answer channel it is actually wired to and say so.

**I did not weaken or delete the assertion.** Waiving a guard to make a form
green is the one thing this campaign's rules forbid most plainly, and a detector
whose meaning is unsettled is not evidence in either direction -- so silencing it
would convert an open question into a false green.

**Clause 5's SUBSTANCE is met and measured** (section 7): `runs=0`,
`skipped_uncovered=1089`, `noprog=0`, `token_refused=0`, and `raster pixels=2816`
-- the authored baseline restored exactly, measured across three separate runs.
What is red is this one pre-existing detector, whose semantics this packet has
now measured and whose repair is a decision I am handing over rather than taking.

---

## 14. FIVE STALE ASSERTIONS, NOT THREE

Section 11 listed three. Running the forms to the end found **five**, and the
last of them is mine:

| # | assertion | plain | field forms |
|---|---|---|---|
| 1 | `fld_db_posts_o` | 2 | **48** |
| 2 | `fld_ret_seen_q` | 2 | **4** (loads are unanswered) |
| 3 | `fld_db_commits_o` | 0 | **1** |
| 4 | `cmd_commands_o` | 14 | **15** -- the packet carries a TerrainField record |
| 5 | `fld_db_load_words_o` | n/a | **`SFF_N_LOAD + 1`** -- MY re-posted header. Caused by this packet, not inherited |

**Each one cost a full console run to find, because the first fatal hides the
next.** That is the shape worth carrying: a mode built behind an early-fatal gate
has an UNEXERCISED TAIL, and every literal census in that tail is a claim nobody
has tested. `-FieldActive` fatals ~2,400 lines above the last of these, so the
whole tail had never run in either field form.

**And one of my own comments went stale inside the packet.** The
`fld_commit_ok_q` message I wrote said the commit *"is posted BEFORE the load
words"*, which was true of my first (refused) ordering and false after I reverted
it. Caught and corrected in the same file as section 2's write-up of exactly that
failure mode -- which is the honest version of how easy it is.
