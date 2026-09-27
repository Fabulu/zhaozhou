# FINDINGS -- FIELDACTIVE, 2026-09-27

**Branch `gz/fieldactive`.** Base `62b79f3d`.
Written against the brief's nine deliverables AND the owner ruling
`reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md` (commit `d84c97ea`),
which landed mid-packet and lifted the fence around `TERRAIN.COMPOSED_MATERIAL`.

---

## THE SIX ACCEPTANCE CLAUSES, EACH ANSWERED SEPARATELY

The owner's ruling makes this a checklist, not a summary. Status at this head:

| # | clause | state |
|---|---|---|
| 1 | a real production Field program is **installed and executed** | **INSTALLED, measured.** Execution pending the run in flight. |
| 2 | the field **covers the intended terrain** | **MET, measured** -- `tp_covers=1`, 1,089 lattice vertices offered (33x33). |
| 3 | material write produces a value that **cannot equal the authored baseline by accident** | **DESIGNED, NOT YET LANDED.** The lever is identified and already wired; see below. |
| 4 | composed material **reaches the intended production consumer** | same lever as 3. |
| 5 | the **uncovered/control form restores the authored result** | **BUILT** -- `-FieldUncovered`, asserts `render_pixels_o` returns to 2,816. |
| 6 | the **no-field forms remain unchanged** | **MET** -- every `ifndef` arm is the old assertion, plain smoke PASS at 2,816. |

**Clauses 3 and 4 share one instrument and it already exists.**
`tsfill_tile_max_q` / `tsfill_tile_or_q` (bench `:3872-3873`) accumulate
`tfill_tile_c = tfill_ts_off_c >> 12` -- the tile index derived from the
**address the mosaic actually issues on its fill requests**. Authored baseline
`[6 7]`. That is a VALUE produced by the real consumer, not a cell count, and
the bench's own comment at `:5518` records both prior failures of this exact
clause: a constant `MAT_BASE_RGB_C = 0xFFFFFF` making `tile_max != 0` pass for
a reason unrelated to the mosaic, and `mat_cells` counting cells rather than
values. Pinning `[6 7]` on the no-field and uncovered forms and requiring the
covered form to differ satisfies 3, 4, 5 and 6 off one wired number.

**I have deliberately not asserted a covered-form value I have not yet seen.**
Measure, then pin -- in that order.

---

## WHAT I FOUND FALSE IN WHAT I WAS HANDED

Seven. Four were load-bearing for the refusal this packet overturns.

### 1. Section 13.7 is not in the file my brief names

My brief cites *"`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` -- 13.7"*.
**That file has sections 0-10 and 623 lines; there is no 13.7 in it.** The real
one is `reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt:1670`.
`FINDINGS-I34CLOSE.md:48` repeats the misattribution, as do five comment sites
in `zhao_console_core.sv`. The content was never in dispute -- only which of the
two documents is standing authority, and one is the owner delegation.

### 2. The real 13.7 asks for FOUR things; my brief asked for one

> nonzero actual runs, correct complete output values, actual terrain
> consumption and **successful recovery after a deliberately bad
> program/association**.

The recovery clause appears in no brief and no findings file. It is a separate
control form, not a property of the positive run.

### 3. The plain smoke's zero is partly BY DESIGN

13.7: *"The current no-program smoke ... explicitly asserts zero completed FIELD
runs; KEEP IT as a refusal/control case."* Preserved -- every `ifndef` arm is
the old assertion.

### 4. Four files declared absent "by ls, not inferred" all EXIST

`reports/FIELD-REPAIR-PLAN-20260920.md:388` lists `pack_field_host.cpp`,
`zfield_host_plan.hpp`, `zfield_host_plan.cpp` and `zhao_field_host_v2.sv` as
non-existent. All four exist (188 / 418 / 726 / 1,812 lines); the packer is
registered at `tests/CMakeLists.txt:311`. They were built after 2026-09-20, so
the plan was true when written -- **but entry I34 still quotes the
consequence**, "NO C++ HELPER TURNS A HostPlan INTO DOORBELL LOAD WORDS", and
`FINDINGS-I34CLOSE.md` cited that list today to refuse this packet.

### 5. The REGS=32 blocker is half true and its CONCLUSION is false

`zhao_console_core.sv:8078` says lowering against 32 "is expected to refuse" and
the remedy is another program, a new one, **or widen REGS**. Measured, packer
built from this tree, sweeping `--scalar-base` at `--registers 32`:

| program | result |
|---|---|
| `crater_ring` | REFUSED at every scalar base -- the entry is right |
| `impact_wave` | **LOWERS** at scalar-base 7, 8, 9 |
| `wave_pool`   | **LOWERS** at scalar-base 8, 9, 11, 12, 13 |

`wave_pool` at scalar-base 12: RC 0, `register_high_water 31`, profile 0
(Earth), `output_count 4`, `required_mask 0x0F`.

**The ceiling is a function of `scalar_base`, whose library default is 32** --
so uniforms start exactly AT the composed ceiling and everything refuses. It is
a settable `LowerOptions` field exposed as `--scalar-base`, and **the entry's
remedy list does not mention it.** No REGS widening, no ALM purchase, no
authored fixture program. `wave_pool` is a REAL SHIPPED SPELL
(`spells/membrane.form`) whose required mask is lanes 0..3 -- exactly I34's four
Earth channels.

### 6. The packet headroom is a third of what the entry says

Entry I34: *"PKT_MAX_C is 768 and the current packet is 696"* -- reads as 72
bytes spare. Measured: body 704, packet 744, **real headroom 24 bytes**. A
112-byte record never fitted. Raised to 896.

### 7. "Every counter past terrain's door is unreachable BY CONSTRUCTION" is false

The bench asserted the smoke "fails every terrain page CRC, so terrain's
composed door never opens and no vertex ever reaches the compose lane".
**Measured false**: a plain run prints `pt_samples=1089` and draws 61 terrain
triangles. Entry I34 says to settle exactly this before writing a line of the
new mode, and adds that `terr_pt_samples_o` is "never printed or asserted" --
it IS printed. This made a zero look like a LAW instead of a STIMULUS GAP.

---

## THE ARCHITECTURE, WHICH NO SINGLE FILE STATES

**The console has TWO doors into the field engine and they are not the same
door.** This shaped the packet:

1. **THE CAPSULE** to `zhao_field_loader` as an FH2 INSTALL_CAPSULE, fetched
   over the REAL HPS bridge (arbiter client 5). It validates, SEALs and
   PUBLISHES handle/hash/ready. **This is the only thing
   `zhao_terrain_fieldlist:304` can resolve a TerrainField handle against.**
2. **THE LOAD WORDS** to `zhao_field_host_v2` via the doorbell op-0 LOAD arm.
   The loader owns the BACKING STORE descriptor table (FH15) and **has no `ld_*`
   port**, so the microcode must arrive here too, HEADER LAST.

Both streams come from ONE `HostPlan`, so the sealed capsule and the running
microcode cannot describe different programs.

---

## WHAT WAS BUILT

* `tests/prod/smoke_field_fixture_gen.cpp` + generated `smoke_field_fixture.svh`
  -- a 1,472-byte ZFH2 capsule and 43 doorbell LOAD words, via the same four
  library calls `pack_field_host` makes. ctest `smoke_field_fixture_fresh`.
* **HPS region 6** at `0x1000_0000` -- the address `fld_ldr_stage_base_i` has
  advertised since the loader was composed and which nothing ever backed. The
  plain form still `$fatal`s there, on purpose.
* `-FieldActive` and `-FieldUncovered`, each with its own build tag; both
  elaborate clean.
* A real TerrainField record (0x0200, 112 B).

### Measured, link by link, each by its own counter

```
fldstage install_ok=1 commit_ok=1 ldr_installs_ok=1 ldr_installs_failed=0
         ldr_bad_crc=0 ldr_bad_meta=0 ldr_load_bytes=1536
fldpub   pub_ready=01
fldearth records=1  noprog=1089  skipped_uncovered=0  faults=0
fldpatch tp_covers=1
fldhost  db_load_words=43 db_fh2_posts=1 db_commits=1 db_addr_refused=0
         db_commits_refused=1      <- entry I42's own probe, UNREGRESSED
```

1,536 bytes is the whole capsule fetched as 24 aligned bursts, CRC checked,
sealed, object 0 published.

---

## THE ANTI-VACUITY DESIGN

**`-FieldUncovered` differs in ONE FIELD OF ONE RECORD**: the footprint moves to
16384.0, where no lattice vertex is. Same capsule, same install, same load
words, same commit, same record, same packet length, same HPS traffic.
`zhao_tp_covers` then rejects every vertex, so the machine does ALL the work up
to the coverage test and NONE after it.

It asserts `fld_earth_runs_o == 0`, `skipped_uncovered != 0`, **and that
`render_pixels_o` returns to the reference 2,816.** That last is load-bearing:
if the install/load/commit apparatus perturbed the scene on its own, the
covered form's numbers would prove nothing about fields.

A control that is "no record at all" would NOT have this property -- it would
differ in packet length, CRC, record count and every counter upstream of
coverage, so any difference could be attributed to a dozen things.

---

## WHAT I GOT WRONG AND CAUGHT MYSELF

Four, and every one produced a DOWNSTREAM zero with no named cause -- the
flattering direction.

1. **Diagnostics printed AFTER the assertion they diagnose.** The first run
   fatalled at the terrmat assertion and printed none of the field counters
   that say why. Moved upstream, reason written beside them.
2. **The capsule's RESOURCE_EPOCH was 0.** The loader composes
   `CHECK_EPOCH(1'b1)`, the bench drives `0xF1E1D000`, and
   `zhao_field_loader:764` refuses a mismatched epoch as STALE before reserving
   a slot. Confirmed by counter: `ldr_bad_meta=1`, `ldr_load_bytes=64` -- it
   fetched exactly the header and refused. Now a named constant plus an
   **elaboration $fatal** comparing `SFF_EPOCH` against `FLD_CFG_PLAN_BASE_C`.
3. **The INSTALL post's hash operand is not the program hash.**
   `zhao_field_loader:968` compares it against the CRC folded over the RECEIVED
   bytes with 12..15 masked -- the image's BODY_CRC32C. **Caught by reading the
   RTL while a run was still simulating**, the only reason it did not cost
   another rebuild. The port is called `hash` and the fieldlist matches on a
   hash, so the wrong constant is the one that reads correctly; the fixture now
   emits `SFF_HASH` and `SFF_BODY_CRC` under deliberately different names.
4. **ACCEPTED IS NOT COMPLETED.** The frame gate waited on posts being taken by
   the mailbox. The mailbox is a QUEUE, so all 45 posts were accepted in ~45
   cycles while the loader was still fetching 1,536 bytes over a bridge shared
   with terrain's page loads. The frame started, CMD.EXEC lowered the record,
   and the fieldlist swept the publication port BEFORE the seal -- storing
   `e_res` LOW and answering `noprog` for all 1,089 vertices, with
   `install_ok=1` sitting beside it because the RETURN arrived after the damage.
   The gate now waits on the RETURNS. `terr_fl_unresolved_o` -- already at the
   core edge, read by nothing -- is now printed, because it is the counter that
   separates "the record never arrived" from "the record arrived too early".

Plus one process error: **I read `tail`'s exit code for the register**, which
printed RC=0 on a tool that returns 1 while gaps remain. Re-measured: **RC 1**.
This repo's own documented trap.

---

## THE REGISTER, MEASURED BARE

| | value |
|---|---|
| base `62b79f3d` | **2** -- I34, I55. RC 1 |
| at this head | **2** -- I34, I55. RC 1 |

**I did not touch I34's prose, deliberately.** The register classifies it by
regex-matching `BOUNDARY` (`completion_register.py:62`, `:238`), and that word
is already measured stale -- so a prose edit would move the number without
moving the machine. The owner's ruling makes this doubly binding. The two stale
sentences I DID correct are in the BENCH, not the entry, and neither is
load-bearing for any classification.

---

## THE REMAINDER, DECLARED RATHER THAN TRIMMED

The owner's ruling put `TERRAIN.COMPOSED_MATERIAL` in scope. It is **not** in
this packet, and I am declaring it rather than quietly cutting it. What I
established about it, so the next packet does not re-derive it:

* **The address is FREE and uncontended.** `spec/memory_rules.md:306` has
  `0x058B_0000 .. 0x05FF_FFFF` as "reserved / unmapped", and `TERRAIN.DEVSTORE`
  ends exactly at `0x058B_0000`. The commissioned 2 MiB carves cleanly out of
  reserved space with no neighbour to move.
* **The enactment pattern is established** by `TERRAIN.DEVSTORE`
  (`memory_rules.md:362`): a region row, `zhao_pkg` `_BASE`/`_SPAN`
  elaboration constants that agree to the byte, and `MEM.GUARD` arms --
  *"a window opened WITH its block, never ahead of it."*
* **The tap point is `zhao_terrain_matjoin`'s composed write face** --
  `o_we_o`, `o_ci_o`/`o_cj_o`, `{o_mat_a_o, o_mat_b_o, o_weight_o}`, which is
  port-for-port the compose cache's layer E. Publishing from there does NOT
  require the `patch_v2` rewrite.
* **`zhao_terrain_patch_acc` is BUILT AND COMPOSED NOWHERE.** It owns
  `out_mat_*`, the resolved material u32, and appears in `zhao_console_core.sv`
  only in comments and not at all in `design/fit_targets.yml`. That is a
  "BUILT, INSTALLED NOWHERE" uncashed cheque and it is why the entry couples
  COMPOSED_MATERIAL to a much larger rewrite than the destination needs.
* **The bench needs a new WRITABLE region too.** Its HPS far side accepts writes
  into the particle region only; anything else is `$fatal`.
* **Cost must be MEASURED, not estimated.** The owner was explicit that the
  existing bandwidth figure is evidence about the presently assumed publication
  shape and is not authority to refuse. A refusal grounded in the old number
  will be sent back.

---

## GATES

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2**, RC 1 -- unchanged from base |
| `check_console_inventory.py` | RC 0 |
| `check_prod_manifest.py` | RC 0 |
| `check_quartus17_syntax.py` | RC 0 |
| `gen_prod_top.py --check` | fresh (89 instances) |
| `gen_console_board.py --check` | FRESH (1624 core ports, 127 parameters) |
| `gen_shell_paired_diff.py --check` | fresh |
| `check_case_labels.py` | RC 0 |
| `check_console_closure_lint.py` (gate 31) | RC 0 |
| `mutant_copy_drift.py` (AFTER commit, R121) | RC 0 -- 79 copies, no drift |
| `test_cmd_exec_directed` (R60, BUILT and RAN) | **977 checks**, RC 0 |
| console smoke, plain | **PASS**, pixels=2816, frames_admitted=1 |
| `-FieldActive` / `-FieldUncovered` lint | RC 0, both elaborate |

**I changed no core port**, which is why the three generated-file gates are
still fresh across an edit to the bench.

---

## THE DETECTORS I ADDED, AND THAT THEY FIRE

* **`smoke_field_fixture_fresh`** -- fired deliberately. Clean check RC 0; one
  constant perturbed gives **RC 1, "is STALE"**; restored gives RC 0. The
  restore was verified by CONTENT, never `Copy-Item`, so the stale-timestamp
  trap does not apply.
* **The `field_composed == 0` assertion** -- fired for real, twice, on real
  faults. It is not a detector whose silence I am quoting.
* **The epoch elaboration $fatal** -- written precisely because the fault it
  catches presented as a downstream zero with no named cause.

## ONE OBSERVATION I AM NOT QUOTING AS CLEAN

`fld_earth_lane_desync_o` read **1** on the runs where the program was not
resident. The plain smoke asserts it zero and passes. It may be a consequence of
the 1,089-vertex noprog path rather than a defect, but **I have not proven
that**, and it is recorded here rather than waived.

## PROCESS

Another repository's build (`-DUPHEAVAL_ZHAOZ...`, parent PID 19328) and another
packet's smoke (`..._smoke_walkras_f72d3823`) ran on this machine throughout.
Both were identified by command line and parent PID and **neither was touched**.