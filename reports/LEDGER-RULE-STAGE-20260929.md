# The ledger's rule stage ran for the first time — 2026-09-29

`tools/ledger/src/cli.ts:123` runs rules V1–V23 only `if (errors.length === 0)` against the
**schema** stage. The schema stage had never been clean, so **no ledger rule had ever run on
this repository.** Seven remaining schema errors were not seven findings; they were a curtain.

    schema 7 -> 0     then the rule stage ran:  349 errors
    and then           349 -> 230

`tools/design/check_counter_ids.py`'s own header had already recorded the mechanism —
*"(V15 could not have caught it even in principle; it also never ran, because the ledger's
schema stage fails first and the rule stage is skipped.)"* — and nothing had acted on it.

---

## Where it stands

> **CURRENT AS OF THE AMENDMENT AT THE END OF THIS FILE: 224, not 230.** The table below is
> the state at the first write-up; V5 closed, V20 dropped to 167, and the amendment carries
> the V20 classification that changes what the largest number means. A summary table that
> disagrees with its own document is the stale-claim failure this campaign keeps finding, so
> it is flagged here rather than left to the reader.

| rule | n | what it is | kind of work |
|---|---|---|---|
| V20 | 172 → **167** | an RTL prose invariant claim with no machine-resolvable `ENFORCED-BY` | **164 investigations**, 3 phantom enforcers (see amendment) |
| V4 | 29 | rtl block missing `reference_model` / `tests.random` / `source_ids` | oracle + test authoring |
| V17 | 18 | citation coherence: oracle/contract/test disagreement | mostly downstream of V4 |
| V7 | 10 | edges naming six modules that have no ledger row | six rows to author |
| V5 | 1 → **0** | `PART.CLIPFEED` was INTEGRATED with no `resource_actual` | closed: the rung was wrong, not the number |

**Nothing left is a records fix.** Everything closed on 2026-09-29 was bookkeeping that hid
real work; everything remaining *is* the real work.

---

## What was closed, and the two decisions behind it

Both remaining schema problems had been filed in `TASK_LOG` as *"owner/architect
decisions"*. Under the standing directive that filing is the first thing to check, and
neither survived it — one was settled by the charter, the other by the id convention.

**`*.GROUP_SEQ` (3 errors)** — not a legal block id (the pattern forbids underscores, which
is why all 136 ids concatenate compound words: `GEOM.CLIPDOOR`, `DEBUG.FRAMEBLIT`) and no
record declared it. Repointed to `TERRAIN.GROUPSEQ` / `GEOM.GROUPSEQ`. The rows are still
owed and were **not** invented — see *six modules outside the ledger* below.

**`source_ids: false` (4 errors)** — the schema pinned `const: true` for every rtl block.
**Charter line 391 is the law and it is not vacuous:** *"The FPGA command and trace paths
carry stable source IDs. The profiler must be able to attribute triangles, fragments,
texture misses, terrain samples, particle updates and deadline pressure to Form
declarations."* So `false` is a declared breach, and V4 is right to demand `true`.

`TERRAIN.MATJOIN` is keyed by the subpatch cell (`a_ci_i`/`a_cj_i`) and
`TERRAIN.EDGERECON` by patch index (`f_ix_i`/`f_iz_i`); neither has any port that could
hold a source id. Flipping the flag would be a fiction; building the plumbing is the repair
and **both files are inside the running `zhao_console_core` fit's closure**, so editing them
would leave a fit 70+ CPU-minutes in measuring a design that no longer exists. The breach
was made *representable* instead — `source_ids: false` now requires a `source_ids_breach`
explaining it — so the finding moved from the schema stage, where it blocked 23 rules, to
V4, where it is reported every run.

**→ OWED: the source-id plumbing on both blocks, immediately after the console fit lands.**

---

## Findings that are not ledger bookkeeping

### 1. Six composed production modules are outside the ledger entirely

V7's last 10 findings are edges naming six ids, and `zhao_console_core.sv` instantiates
every one of them:

    TERRAIN.LODFEED    zhao_terrain_lodfeed.sv
    TERRAIN.HDRREAD    zhao_terrain_hdrread.sv
    TWOD.SAMPLER       zhao_twod_sampler.sv
    CMD.EXEC           zhao_cmd_exec.sv          227 KB, 36 references in the core
    TERRAIN.GROUPSEQ   zhao_terrain_group_seq.sv
    GEOM.GROUPSEQ      zhao_geom_group_seq.sv

None has a contract, a `zref` reference model, or a random-test arm, so an honest row cannot
be written from what is on disk and a `purpose` would be describing someone else's design.
Six blocks of shipped silicon have no ledger row.

### 2. 118 declared counters have no presentation path at all

V12's 44 unregistered counters were all real — each resolves to an output port through an
explicit `counter_ports:` mapping — so registering them was honest, and they are registered.

`tools/design/check_counters.py` separately reports **118 counters with no `<name>_o` port,
no mapping and no snap channel.** None was registered: `counter_catalog` is an allowlist, so
adding an unimplemented name creates a counter nobody can read and permanently silences V12
about it. This is much larger than V12's 44 and it is RTL work.

Also closed here: `design/counter_ids.lock` was 110 entries behind since 2026-09-20, so 110
live counter ids were renumberable without any gate noticing. The lock now holds all 439.

### 3. The four `mem_guard` mutant controls pass, but their recorded assertions are stale

Re-run 2026-09-29, all four `expect fail` satisfied, `RC=0`, 1.3–3.2 s. But:

    mutant     e85a5896 recorded      re-run today
    pbview     a1_pb_wr_view0    ->   a1_region
    pbunion    a1_pb_wr_view0    ->   a1_region
    resbound   a1_map            ->   a1_region
    devbound   a1_region         ->   a1_resource_bounded

Not copy drift (`mutant_copy_drift.py`: OK, all 80 copies current). `b9c2d3e2` **widened
`a1_region`** with the TERRAIN.DEVSTORE arm; it is timestamped 14:07 against `e85a5896`'s
16:37 and **neither is an ancestor of the other** — parallel branches joined later by
`a9678ad2 Merge DEVSDRAM`. The controls were measured on a tree without the widened
assertion and never re-run after the merge.

**→ OWED: `a1_pb_wr_view0` and `a1_map` are no longer tripped by any mutant, so no committed
positive control now shows those two theorems are non-vacuous.** They were shown once, on a
tree that no longer exists. Closing it needs a fault aimed past `a1_region`.

### 4. V17(d) is over-strict where a contract declares an ENTRY POINT

V17(d) requires the cited test to textually mention the `reference_model`'s last segment.
`GEOM.LIGHT` fails it, and the data is right while the rule is not: its ports are
`n_x_i/n_y_i/n_z_i`, a world normal, its test differentiates against
`zref::render::shade_from_world_normal_unclamped`, and its contract already states the
distinction deliberately — *"`zref::render::shade_flat_tri_dir` is the law.
**`shade_from_world_normal_unclamped` is this block's entry point into it**"*.

Changing the ledger to satisfy the rule would destroy the shared-law record to turn a gate
green. `uncashed_cheques.py` check 3 already has two tiers for this same problem ("the
strings differ even when the law does not"); V17(d) has one.

**By contrast `GEOM.PARAMARENA` / `GEOM.PARAMWALK` are genuine V17(d) hits:**
`geom_paramarena_directed.cpp` contains no `zref::geom::` reference at all, so the
`parambuf_chunk_follow` claim is not backed by the cited evidence — "an alias, not evidence".

### 5. Three more citation defects, each a different kind

* **`POST.ECHO`** declares `reference_model: zref::post::echo`, but `echo` is a **namespace**
  in `zref_post.hpp:135`, not a class or function — V17(a) wants a definition. The block
  needs to name the actual function inside it.
* **`GEOM.LOOM`**'s contract cites `tests/geometry/geom_loom_random.cpp` and
  `tests/formal/geom_loom_order.sby`; neither exists. Its `tests.formal` was the literal
  string `"PLANNED -- NOT WRITTEN"` (now `null`) — the same unwritten work, claimed twice.
* **`TWOD.ASSET`**'s contract cites `tests/compositor/twod_asset_directed.cpp`, which has
  never existed. Its ledger row now points at the chain test **with a comment saying that is
  SYSTEM-level coverage and a block-level directed test is still owed**; the contract needs
  the same correction.

### 6. V3 only inspects the CURRENT maturity's evidence entry

V3 filters `maturity_log` to entries whose `state` equals the block's current maturity, so
every superseded entry's evidence is unchecked. `MATERIAL.RESOLVE`'s REFERENCE_COMPLETE entry
holds `...hpp + tests/...cpp (32 checks)` — not a path — and no rule looks at it. Latent
across every block with history.

### 7. V20's 172 are not bulk-fixable, and must not be

V20 asks mechanically *who enforces this sentence?* of every `by construction` / `validated
upstream` / `cannot happen` claim in RTL prose. Its own comment records that both false
claims this project has found in RTL prose were caught by asking exactly that — one a mode
byte nothing validated, one a real CDC hazard.

**Adding `ENFORCED-BY:` lines to satisfy it would be the worst available action.** The rule's
value is that some of those 172 claims are false, and each needs an answer: name the real
enforcer, add the missing enforcement, or rewrite the claim as an assumption naming who
upholds it. 172 individual investigations, and the yield is measured in real defects.

---

## Two tooling traps that cost time today

**The unsourced shell, for the third time in a third tool.** I concluded "no SymbiYosys
anywhere on this machine" and was one step from recording four formal properties as
`never_ran` — a false statement in the registry. `sby` is at
`.tools/oss-cad-suite/bin/sby.exe` and is on PATH the moment `tools/env/zhao-env.ps1` is
sourced. CLAUDE.md records this for `cmake` and for `ctest`. **Source the environment before
concluding a tool is absent.**

**`cmd | tee f | head -3` truncates `f`.** `head` exiting sends `tee` a SIGPIPE, so a
captured log is cut short and a by-rule histogram reads 31 instead of 349. The
stale-binary family: the shell told the truth about the wrong thing.

---

## The console fit, attempt 3

Alive at 67.3 CPU-min over 74.4 min wall (0.9 cores sustained, normal for `quartus_map`'s
mostly-serial analysis). **Peak memory 4.43 GB** — the fit is not a memory hog, which
answers the standing question directly; the 40 GB-scale pressure seen earlier was other work
on the shared machine (`vmmemWSL` at 36.8 GB, a 13-process `cc1plus` fleet, three Upheaval
`upheaval-studio` suites).

**It measures the current design, and this was checked rather than assumed.** Its banner
reads `HEAD=2a354f96`, now 11 commits behind. Of everything committed since, exactly one
file in its 299-file closure changed — `fpga/rtl/prod/zhao_console_core.sv` — and that diff
is **17 added, 0 removed, every added line a comment** (the `// TIE:` reason on
`u_cliff_lat_share.poison_value_i`). `design/blocks.yml`, the schema, `ops.yml`,
`formal_runs.yml` and the counter lock are not fit inputs.

---

# Amendment, same day — V20 classified, and four more rules closed

After the sections above, work continued. **229 → 224.** V5, V2, V16, V3, V6 and V10 are now
all at zero; the remaining 224 are V20 167, V4 29, V17 18, V7 10.

## V20's 172 are NOT one population, and the split decides the work

    164   the annotation is genuinely ABSENT — no ENFORCED-BY within 10 lines of the claim
      3   an annotation exists and names a test file that DOES NOT EXIST
      5   an annotation existed and was merely UNRESOLVABLE  → fixed, zhao_mem_guard.sv

`zhao_mem_guard.sv` turned out to be the exception rather than the pattern, and it is
instructive: its author had done the whole job — named the right proof, committed a positive
control for each deliberate fault — and the file still read as five unenforced claims. Two
form defects did it, both of which look correct to a reader:

* **A wildcard symbol resolves to nothing.** `a1_resource_*` and `a1_pb_*` are the natural way
  to cite a family; V20 matches the symbol literally. `a1_resource_*` has one member;
  `a1_pb_*` has seventeen.
* **A trailing comma becomes part of the path.** `V20_ANNOTATION` captures up to whitespace,
  so `...mem_guard_no_escape.sby, and the committed mutant ...` yields a path ending in a
  comma — **and the resulting error says "path does not exist", pointing at the filesystem
  instead of the punctuation.**

Fixed comment-only: 0 added and 0 removed lines that are not `//` comments, so the running
console fit still measures what it snapshotted.

**The 164 are the real campaign.** Each needs the enforcer named, the missing enforcement
added, or the claim rewritten as an explicit assumption naming who upholds it. They must not
be cleared in bulk: the rule's entire value is that some of those claims are false — its own
comment records the two it was written after, a mode byte nothing validated and a
"toggle-free by construction" that was false for the FULL case, a real CDC hazard.

### The three phantom enforcers, with candidates found but NOT adopted

| claim site | cites (missing) | candidate that mentions the subject |
|---|---|---|
| `zhao_geom_paramwalk.sv:835` | `tests/geometry/geom_chunkser_directed.cpp` | `tests/geometry/geom_arenabin_directed.cpp` |
| `zhao_terrain_clipfeed.sv:4` | `tests/terrain/terrain_clipfeed_directed.cpp` | `tests/mutants/terrain_clipfeed_tokenskew_mutant.cpp` |
| `zhao_texture_mosaic_hold.sv:4` | `tests/texture/texture_mosaic_hold_directed.cpp` | `tests/texture/desc_identity_stall_control.cpp` |

Deliberately not repointed. Each candidate merely *mentions* the subject; whether it actually
enforces the specific claim is unverified, and writing an `ENFORCED-BY` at a test that does not
enforce the sentence is precisely the failure V20 exists to catch. Three investigations, not
three edits.

## PART.CLIPFEED was the only block above RTL_VERIFIED and had not earned it

V5 wanted a `resource_actual` at rank ≥ SYNTHESIZED and there was none, with no standalone
synthesis of `zhao_part_clipfeed` to take one from. Corrected INTEGRATED → UNIT_VERIFIED.

**A prior pass had recorded a decision for INTEGRATED, and its reasoning refutes itself:**
*"Nothing higher is claimed: SYNTHESIZED would assert a fit this entry does not cite."* The
ladder is `… RTL_VERIFIED, SYNTHESIZED, INTEGRATED, HARDWARE_PROVEN` — SYNTHESIZED is **lower**.
That sentence only makes sense if INTEGRATED sat beneath it, so the rung was chosen on a
misreading of the order, and INTEGRATED asserts the fit it was declining plus one more rung. A
factual correction to a stated premise, not a difference of judgement.

Corroboration: it was the only block of 136 above RTL_VERIFIED (75 UNIT_VERIFIED, 28
SPECIFIED, 20 RTL_VERIFIED, 12 REFERENCE_COMPLETE, 1 INTEGRATED, **zero SYNTHESIZED**), while
TWOD.BAND, TWOD.CMD and POST.GATHER all record the same "composed in zhao_console_core" fact
at RTL_VERIFIED; and it still owes `reference_model` and `tests.random`. The composition fact
is kept — in `notes` and in the preserved, annotated INTEGRATED log entry. What the ladder
lacks is a rung for *composed but not independently synthesized*; borrowing a higher one to
express it is what went wrong.

### And how the rung was claimed is the more useful finding

    rules.ts:117   if (!p) continue; // new block: V3 governs evidence, not ordering

V2's "maturity advances one step at a time" compares against the previously committed ledger,
so a block absent from it is exempt by design and delegated to V3 — and V3 checks only that
the **current** state has a dated, commit-pinned, on-disk evidence file, never that the rungs
below were earned. **Between them a row can be BORN at any height on a single evidence file**,
which is what happened: this row is not in `blocks.yml` at either 2026-09-22 commit its log
cites, those being the dates its *tests* landed. Not an advance that slipped past a rule — a
birth the rule structurally cannot see.

## The four mem_guard formal controls were re-run, and two theorems lost their witness

Registered green (V16 4 → 0) from a run here rather than transcribed. Detail in the section
above; the part that is owed: **`a1_pb_wr_view0` and `a1_map` are no longer tripped by any
mutant**, because `b9c2d3e2` widened `a1_region` on a branch parallel to the one that measured
the controls, and nobody re-ran them after the merge. No committed positive control currently
shows those two theorems are non-vacuous.

## Also corrected: two of my own conclusions

* **"No SymbiYosys on this machine" was wrong** — an unsourced shell. It is at
  `.tools/oss-cad-suite/bin/sby.exe` once `tools/env/zhao-env.ps1` is sourced. I was one step
  from writing `never_ran` into the formal registry about four properties that had in fact
  been run. Third tool to bite this way after `cmake` and `ctest`.
* **"GEOM.LIGHT's `reference_model` is wrong" was wrong.** Its contract already distinguishes
  the law from this block's entry point, deliberately and correctly. It is V17(d) that is
  over-strict for an oracle reached through a documented entry point — while
  GEOM.PARAMARENA/PARAMWALK, whose test contains no `zref::geom::` reference at all, are
  genuine hits.
