# FINDINGS -- texture island / MATERIAL.RESOLVE, second pass (gz/texmat2)

Register at the branch point `1fbfacc9`: **29**. (My brief said 27; the worktree
was newer than the brief.) At my last pushed commit: **28**.

---

## The headline, and it is a MEASUREMENT rather than a reading

The brief asked me to establish *by measurement, not by reading*, whether a
texel reaches a fragment in the composed console, and what stops it. That could
not be asked at all before this packet, because **the composed console exported
no texture counter of any kind**:

* seven of them -- `texture_fragments_o`, cache hits/misses, palette lookups,
  plan/dispatch accepted, combine refused -- were left **DANGLING** at
  `zhao_shell_top_v2`'s instantiation of `zhao_geom_bin_pipe_v2`;
* the eighth -- R9's `cnt_texture_samples_o`, the count of TMU samples actually
  PUBLISHED into a fragment -- was **sunk inside `zhao_raster_tile_pipe_v2`** as
  `unused_cnt_texture_samples` and XOR-folded into that module's unused sink. It
  had no output port at any level above the island.

So "the island samples nothing" was RTL reading, repeated in three places, and
never once a number. It is a number now, and the first reading said something
the reading-of-the-source did not:

```
SMOKE: texture  fragments=1190 samples=0 cache[hit/miss]=[0 0]
                palette_lookups=0 plan_accepted=0 dispatch_accepted=0
                combine_refused=0
```

**1,190 fragments DO reach the island.** Nothing is refused, nothing misses the
cache, nothing is even PLANNED. The island is not rejecting the work -- it is
never asked for any. A zero on its own would have been ambiguous between that
and "no fragment ever got there"; the other seven are what make it a diagnosis.

---

## Gaps closed

### I49 -- MATERIAL.RESOLVE's REQUEST and RESPONSE. Register 29 -> 28.

`fpga/rtl/texture/zhao_material_window.sv`, composed in `zhao_console_core`
between GEOM.REPLAY and GEOM.CLIP. Five ports left the core's list rather than
being driven from constants: `mat_req_valid_i`, `mat_req_material_set_i`,
`mat_req_material_id_i`, `mat_req_quality_tier_i`, `mat_rsp_ready_i`.

**The request's halves are joined BY CONSTRUCTION.** Entry I39 refused the
obvious pairing by name -- "TWO LIVE WIRES ARE NOT A PRODUCER" -- and it was
right. The window does not pair two wires: it reads three fields of ONE
triangle record. `material_set`, `material_id` and the draw's `semantic_weight`
all ride with the meshlet from GEOM.DRAWJOB (ruling R29) through
GEOM.MESHFETCH, GEOM.ASSETFETCH, GEOM.ASSEMBLE and GEOM.REPLAY, latched by the
same enable at every stage. GEOM.ASSEMBLE and GEOM.REPLAY gained a
`material_set` and a `quality_tier` field beside the `raster_state` they already
carried -- the same per-meshlet register, the same latch, the same argument.

**And the JOIN, which is the half that is easy to fake.** The published answer
is read combinationally at the shell's triangle door, several pipeline stages
downstream -- the exact shape of the metadata-swap defect CLAUDE.md has a
chapter about. What makes it sound is an **interlock**, not a latency:

> the window never changes what it publishes while ANY triangle is between
> GEOM.CLIP's input and the door.

The three disposal events of that span are exhaustive -- GEOM.CLIP accepts one,
GEOM.CLIP retires one with a non-ACCEPT verdict (`ret_valid_o`/`ret_verdict_o`,
which fire once per submitted triangle), or the door takes one -- so the
occupancy always returns to zero and the drain always completes. The argument
does not depend on GEOM.CLIP's latency, on GEOM.SETUP's, or on whether and in
what order GEOM.CLIP drops a triangle.

**The cost is measured, not argued.** `mat_win_drain_stall_o` and
`mat_win_answer_stall_o` are counted separately because they have different
cures, and a repeat of the same material -- which the oracle's own header says is
the common case -- costs no drain, no request and no stall at all.
`mat_win_resolves_o` against `mat_win_switches_o` is the "counters see what
pictures cannot" reading: a window that re-resolved a material it already held
would produce a byte-identical frame and spend the meshlet loop's clocks twice.
The smoke asserts they are EQUAL.

**R20's law carried one seam further.** A resolve that finds no record publishes
the DEFINED FAULT MATERIAL (`sample_count = 0`, the legal "takes no texture
sample" profile) and is counted on `mat_win_no_record_o`. It never stalls the
triangle stream and never inherits the previous answer's fields.

**Evidence:** `tests/texture/material_window_directed.cpp` -- **45 checks, 0
failures**, seven cases. It plays the resolver's role and tests only the join,
asserting the CORRECT behaviour ("the record held at every disposal") rather
than that a detector fires. Both structural guards were **FIRED** with legal
stimulus at the block's own ports (a departure that never had an arrival), with
a negative control that an arrival and a departure on one clock cancel. That is
the `t_ack_i` shape, not the `wq_overflow_o` shape, so neither owes a committed
mutant -- the distinction is worth keeping. `verilator --lint-only -Wall` RC 0.

### And then the island SAMPLED. The same counter, one commit later.

```
SMOKE: texture  fragments=1190 samples=1190 cache[hit/miss]=[1190 15]
                palette_lookups=0 plan_accepted=1190 dispatch_accepted=1190
                combine_refused=0
SMOKE: matwin   resolves=1 switches=1 stall[drain/answer]=[0 100] occ_max=2
                no_record=0 sel_ovf=0 clut_unowned=0 err[unpub/underflow]=[0 0]
SMOKE: binding  page_gen=1 acks=3 last_status=0 fill[lines/beats]=[15 120]
SMOKE: material responses=1 status=1 hits=0 misses=1 not_resident=0 fetch_denied=0
```

Every link in that chain is real and none of it is played by the bench except
the two BOUNDARY channels a bench is supposed to drive (the V3 binding page and
the texture fill socket, both core input ports):

`PublishResource` in the command packet -> CMD.EXEC -> MEM.UPLOAD on the
TERRAIN.BUILD socket -> the 5f.1 directory row -> the DRAW's `material_set` in
GEOM.DRAWJOB's job sideband -> the meshlet through MESHFETCH/ASSETFETCH/
ASSEMBLE/REPLAY -> `zhao_material_window` reads all three request fields off ONE
triangle record -> MATERIAL.RESOLVE fetches the record as ENGINE1 through the
real MEM.GUARD -> the window publishes it as the flat request's material half ->
the sealed binding page accepts the witnesses -> the cache misses 15 lines ->
the fill socket serves each as exactly eight 16-bit beats (15 x 8 = 120) ->
**1,190 TMU samples published into 1,190 fragments.**

Three details worth keeping:

* **The record it fetched is record 1, not record 0, and that is the point.**
  `zhao_geom_meshfetch` reads the material id from descriptor bytes 4-5 and this
  fixture's descriptors carry `16'h0001`. So the fixture now uploads TWO legal
  records that differ in every field the flat request carries, and the smoke
  asserts the answer is record 1. A window that defaulted to 0, or that paired
  the draw's set with another meshlet's id, cannot produce that answer. It is a
  positive discriminator, not a convenience.
* **`resolves == switches == 1`** is asserted, not displayed. One material, one
  resolve. The re-resolve fault produces a byte-identical picture.
* **`stall[drain/answer] = [0 100]`.** The drain cost NOTHING on this frame
  because there is one material in it; the 100 clocks are the memory fetch. The
  interlock's price is a real number and it is zero where the common case lives.

The smoke now FAILS if no sample lands, if no fragment reaches the island, if
the combiner refuses one, or if the fill socket serves a line that is not
exactly eight beats.

### I20's `tri_flat_request_i` -- CLOSED. (The entry stays open on two ports.)

The 298-bit flat request is now built inside the core from the window's
published answer. Every field has a named owner at the assignment:

| field | owner |
|---|---|
| `sample_count`, `base_binding_selector`, `material_recipe`, `recipe_weight` | MATERIAL.RESOLVE's record |
| `response_class` | the material record's `tmu_mode`, through one editable parameter -- **owner decision D1** |
| `palette_slot`, `palette_generation` | **ZERO IS THE BINDING ROW'S OWN LAW** for a direct format (`binding_row_legal` refuses a direct row whose pair is non-zero); unproduced for a CLUT row, and COUNTED on `mat_win_clut_unowned_o` |
| `lod_q4_4` | the SAMPLER's, excluded by MATERIAL.RESOLVE's contract in terms |
| `aux_required`, `aux_surface_ctx` | zero is the LEGAL non-terrain profile in the resolver's own words, and the tile pipe refuses a non-zero one that has no producer |
| `base_alpha` | owner ruling R48 -- opaque, in a named editable constant |
| `base_rgb` | the VERTEX's. **I20's remaining half**, named rather than quietly filled |

Before anything is published the request is all-zero, which is the same legal
profile the port carried when it was a boundary -- so the retirement changes
behaviour only once a material has actually been resolved.

`tri_continuation_tail_i` (48b) and `tri_fragment_state_i` (32b) remain OPEN and
I20 remains a BOUNDARY.

---

## A pre-existing RED gate, repaired

**The waived console-board lint was RC 1 at the branch point and had been since
`91335fe2`** (ruling R64's mip-plane retirement, landed earlier the same day).
That commit deliberately left eight pins NAMED AND EMPTY at `u_terr_mipgen` so
the retirement would be visible where the block is used -- the right intent --
and the eight `%Warning-PINCONNECTEMPTY` it produced turned a gate whose pass
condition is **SILENT RC 0** into a failure for every packet that ran it
afterwards. hostdbg's `f121c179` row records the gate silent; nobody ran it
between that commit and this one.

Repaired with a `lint_off PINCONNECTEMPTY` pragma around the eight, which keeps
R64's intent and restores the gate. Deleting the pins instead would have hidden
the retirement, which is the opposite of what the ruling asked for.

---

## The previous texture lane's refusals, re-checked

My brief said not to repeat the first texture packet's refusals without
re-checking whether their stated cause is still true. **Every one of them is
spent**, and none of the six needed to be carried forward:

| FINDINGS-texmat refusal | state on 2026-09-20 |
|---|---|
| "The island still samples nothing... GEOM.REPLAY (I11) is the missing carrier of meshlet material" | **CLOSED by this packet.** I11 landed, the carrier exists, and the island now publishes 1,190 samples. |
| "MATERIAL.RESOLVE would hang on a denied fetch" | closed 2026-09-19 under R20 (`mem_rsp_denied_i`, `fetch_denied_o`); this packet carries the same law one seam further. |
| "MEM.UPLOAD: no ratified command carries an upload request" | closed by R17's `PublishResource`, composed and exercised in the smoke. |
| "Counter catalog: TEXTURE.MOSAIC and TWOD.SPRITE still list `texture_samples`" | closed. `spec/counters.md` section 2 now carries R19's one-emitter-per-id law with `mosaic_picks` and `sprite_texels` named, enforced by `counter_ids_append_only`. |
| "I18 / I19, I41" | I19 and I41 closed by the host/debug and geometry lanes; I18 is an owner decision (hostdbg's, still open). |
| "MEASURE.TOKENS / GOVERNOR: two sources and two units" | the budget side landed under R18/R33; the request side is still I20's shell tie-off. |
## OWNER DECISIONS

### D1. `MaterialSample.modes[3:0]` -- the tmu_mode encoding is UNRATIFIED, and the texture island cannot sample without it

**The evidence.** `spec/commands.zidl:245` ratifies the FIELD --
`u8 modes; // bits 0-3 tmu_mode (nearest/bilinear/CLUT/direct)` -- and **never
assigns the numbers**. Searched `spec/`, `reference/`, `design/contracts/`,
`tests/` and `tools/` for `tmu_mode`: four hits, none of them an encoding
(`zref_material_resolve.hpp:163` is an accessor;
`design/contracts/MATERIAL.RESOLVE.md:52` is the same prose; the directed tests
use 1, 2 and 3 as opaque bytes and check only that the field unpacks).

**Why it blocks a sample rather than being cosmetic.**
`zhao_texture_binding_resolver_v2` takes `{response_class, palette_slot,
palette_generation}` from the flat request as **witnesses** and refuses the
sample when they disagree with the row (`read_witness_bad_c`, `:565-571`). The
class is `binding_class(mode)` -- `0 CLUT`, `1 NEAR`, `2 BIL`. A hardware-built
request therefore has to state a class, and the only material-side field that
could say one is `tmu_mode`. A zero witness triple matches only a CLUT row at
palette slot 0 generation 0, and a palette cannot be loaded at generation 0
(`zhao_texture_palette_res_v2` rejects a BEGIN whose generation equals the
current one, which is 0 after reset), so **all-zero witnesses can never produce
a real texel.**

**My provisional choice, and it is one line to reverse.** `tmu_mode[1:0]` IS the
binding resolver's class numbering, carried in the single parameter
`zhao_material_window.TMU_MODE_CLASS` (default `8'b11_10_01_00`, the identity,
with mode 3 mapping to the reserved class 3 that the resolver refuses). The
reason for that choice over any other is not aesthetics: it keeps
`witness_mismatch_o` ABLE TO FIRE. The witness then differences the MATERIAL
RECORD in VRAM against the BINDING PAGE from the config stream -- two
independent sources. A mapping derived from the binding page instead would be
the "detector wired to two operands that move together" defect, and the checker
would go quiet for the right-looking reason.

**Recommendation:** ratify `modes[3:0]` as `0 CLUT, 1 NEAREST, 2 BILINEAR, 3
reserved` in `spec/commands.zidl`, with `zref::material::response_class_of()`
beside `tmu_mode_of()` and the captures moved in the same pass. It is an ABI
addition of the smallest kind -- a value assignment to an existing field.

### D2. A CLUT material's palette identity has NO producer anywhere in this console

`palette_slot` and `palette_generation` are the binding page's. For a **direct**
format they are zero by the page's own legality law, so the witness is exact and
nothing is invented. For a **CLUT** format they are real values and the material
record does not carry them: `MaterialSample` has `binding_slot`,
`binding_generation` and `modes`, and `MaterialRecord` has `palette_base` --
none of which is the palette's *slot* or the palette's *generation*.

This is not hidden: `mat_win_clut_unowned_o` counts every material published
with a CLUT class, so the absence is loud at the exact moment it would matter.

**Recommendation:** add `palette_slot:2` and `palette_generation:8` to a spare
field of `MaterialSample` or `MaterialRecord`, so the CLUT path can witness like
the direct path does. Until then the console can sample only direct-format
pages, and the counter says so.

### D3. `fb_writer_i` -> VIDEO.SLOTMGR's `lease_writer_o` -- I DECLINED IT, on new evidence

The host/debug lane handed me this and recommended I do it, on the grounds that
its blast radius is only worth paying by a packet already changing shell ports --
which I am. I looked, and the substitution is **not a rename**:

* `fb_writer_i` feeds **two `zhao_mem_guard` instances' `fb_writer`**
  (`zhao_shell_top_v2.sv:1016, 1448`) -- the one-writer-per-frame enforcement --
  and the route tripwire's `expected_writer` (`:1941`) and its ENGINE0-read
  exception (`:1970`);
* `v2_lease_writer` is only meaningful while `v2_lease_valid`. The shell's
  existing consumer says so itself: `rmap_valid_q = v2_lease_valid &&
  v2_lease_writer` (`:2653`). A naive `fb_writer_i := v2_lease_writer` would
  admit **the blit writer whenever no lease is live**, which widens a
  framebuffer safety window rather than narrowing it;
* and the comment at `:1927-1939` records that this exact tripwire has already
  been wrong twice, each time because the guard learned a new legal client and
  the tripwire did not.

It also moves **no register entry** -- I20 stays a boundary on
`tri_continuation_tail_i` and `tri_fragment_state_i` regardless. So the cost is
a safety re-proof for zero gap movement, and the naive form is unsafe.

**Recommendation:** it belongs to the packet that gives the framebuffer lease's
validity an owner (CMD.SCHEDULER's frame arbitration), and the substitution is
`fb_writer := v2_lease_valid && v2_lease_writer` with a committed mutant that
shows the guard still refuses the wrong writer when no lease is live. Recorded
here so the next person does not do the rename.

---

## False claims found

**#17 -- the smoke fixture's DrawForm named a material set that could never
resolve.** `df.material_set` was `{UPL_INDEX_C, 8'h2A}`, taking its generation
byte from `pr.resource`'s (which MEM.UPLOAD does not use), while the publication
created generation `UPL_GEN_C[7:0] = 8'h02`. A resolve issued from the DRAW
could never have hit the directory. Nothing noticed, because the bench issued
the request itself with the correct handle. The record beside it in the same
file states the law it violates -- "its handle generation is the low byte of the
generation it is published with, because that is what GEOM.DRAWJOB compares".
Corrected, and the first run of the closed seam is what found it.

**#18 -- `zhao_material_resolve.sv`'s own header lists four blocking seams and
three of them are spent.** Seams 1 and 2 closed on 2026-09-19 and seam 3
("the REQUEST has no honest producer") by this packet; the file still argues all
four as live. Not edited here -- it is another lane's file and the core's entries
carry the current state -- but it will mislead the next reader.

## Instrument notes

* **`[IO.File]::WriteAllText` with a RELATIVE path resolves against the PROCESS
  working directory, not PowerShell's.** `cd`-ing into a worktree and then
  reading and writing `fpga\rtl\...` read and wrote **the coordinator's
  checkout**. The reads succeeded, the writes succeeded, `git status` in my
  worktree was clean, and every local signal said the edit had landed. Caught by
  `git diff --stat` printing nothing after an edit that must have changed
  something. Reverted by reverse string edit (not `git checkout --`), leaving the
  coordinator's own uncommitted R70 work untouched; it committed it minutes
  later as `50710c1e`, intact. **Always give `[IO.File]` an absolute path, or
  set `[Environment]::CurrentDirectory` first.**
* **The PowerShell backtick trap bit again and left a lone CR**, exactly ruling
  R61's class: a comment written with backticks around `render_texture_samples_o`
  inside a double-quoted string became `<CR>ender_texture_samples_o`. Repaired,
  and two **pre-existing** lone CRs in `tb_zhao_console_core_smoke.sv` (lines
  1850 and 2470) were stripped in the same pass. R61's CR scan lives on the
  shared branch and is not in this worktree's `check_quartus17_syntax.py`; it
  would have caught all three.
* **Two bench bugs in my own directed test, both of which read as block faults**
  and are written down beside the code: a downstream-span model that retires
  only at depth two DEADLOCKS the drain with one triangle stranded, and a
  resolver model that waits to observe `valid && ready` on a later clock never
  sees the acceptance (the window leaves ST_WAIT on the same edge), holds its
  answer high forever, and answers the NEXT resolve instantly with the PREVIOUS
  record's fields -- every handshake healthy, the data one resolve stale.
* **`$new = $old + @('a','b') -join "`n"` COLLAPSES THE ARRAY WITH SPACES.**
  PowerShell binds `+` tighter than `-join`, so `$old + @(...)` stringifies the
  array with the output field separator first and the `-join` then has nothing
  to do. Twenty lines of SystemVerilog went into the smoke bench as ONE line,
  which swallowed the `if` above them and re-pointed an unrelated `$fatal` at my
  own condition -- so the bench failed with "GEOM.ATTRPACK never saw a triangle"
  on a run whose ATTRPACK counters were fine. This is CLAUDE.md's "the comma
  binds tighter than `+`" trap in a new costume. Build the array first, join it,
  THEN concatenate -- and read the inserted region back before running.
* **Two stale `run_console_core_smoke.ps1` processes were found alive**, one of
  them the plain run whose wrapper had already reported its exit code. Killing
  the compiler children is not enough: the script respawns them, so the
  PowerShell parent is what has to go. A suite reading the live tree while it is
  edited is worth nothing in either direction, so both were killed and both
  forms re-run against the repaired bench.
* **`cmake --build` cannot regenerate `build.ninja` when a PINMISSING appears in
  a rule that is part of that regeneration**, and it reports only
  `ninja: error: rebuilding 'build.ninja': subcommand failed` with the real
  diagnostic above the point a `| Select-Object -Last 6` can see. Capture the
  whole log.

---

## Gates at `HEAD` (gz/texmat2)

| gate | result |
|---|---|
| `completion_register.py` | **28** (was 29 at the branch point) |
| `check_console_inventory.py` | OK -- 203 modules elaborated by the core, 207 fit sources |
| `check_prod_manifest.py` | OK -- 337 modules, 66 tops |
| `gen_prod_top.py --check` | fresh (66 instances) |
| `gen_console_board.py --check` | FRESH (1,195 core ports) |
| `gen_shell_paired_diff.py --check` | fresh (59 shared inputs, 91 compared outputs) |
| `mutant_copy_drift.py` | OK -- 48 copies |
| `check_quartus17_syntax.py` | RC 0 -- 327 files |
| console-board lint | **SILENT RC 0** (was RC 1 at the branch point -- repaired, see above) |
| smoke, plain | PASS, `raster pixels=2560`, `frames_admitted=1`, **`samples=1190`** |
| smoke `-Mutant` | PASS -- `terr_pl_slot_overflow_o` fired once |

**R60 (the directed tests must BUILD and RUN, with their check counts):**

| test | result |
|---|---|
| `material_window_directed` (mine) | **45 checks, 0 failures** |
| `cmd_exec_directed` | **677 checks passed** |
| `geom_replay_directed` (leaf I changed) | **113 checks, 0 failed** |
| `geom_assemble_directed` (leaf I changed) | **11 checks passed** |

Three benches needed the new ports connected and would otherwise have failed
the next build with `PINMISSING`, which is what "a port on a leaf costs its
whole instantiation chain plus every bench" means in practice:
`tests/geometry/tb_geom_bin_pipe_v2.sv`, `tests/shell/zhao_shell_v2_lease_path.sv`
and `tests/shell/tb_zhao_shell.sv`. The raster-texture v3 fit top needed it too
and was REGENERATED through its template rather than hand-edited, with the new
counter added to its signature mux so it is read rather than declared.