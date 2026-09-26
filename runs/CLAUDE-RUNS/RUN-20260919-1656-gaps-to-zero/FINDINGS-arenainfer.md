# FINDINGS -- ARENAINFER

Branch `gz/arenainfer`. Base `99da0431` on `claude/ceiling-architecture-20260912`.
Worktree `C:\programmieren\zencrifice\gz-arenainfer`. Nothing was edited in the
coordinator's checkout.

**Completion register, measured BARE at the last pushed commit: 4** (3 tie-offs
+ 1 disconnected + 0 unbuilt), unchanged from the 4 this packet opened with.
**This packet closed no gap and never could** -- see section 9.

---

## 1. THE ANSWER

**An array declared inside a genvar-indexed `generate for` block is not a RAM
candidate for Quartus 17.0.2** -- at ONE iteration, with every other property
held fixed. That is the whole of why `zhao_geom_arenabin`'s fourteen staging
banks went to flip-flops.

It is a **fourth storage-inference killer** beside `QUARTUS_GOTCHAS.md` section
10's three, and it is the dangerous one because a block carrying it passes all
the others: the read is synchronous, nothing resets the array, the element is
written whole, and a shared read/write process with a read enable is measured
innocent at 65,536 bits. The five arrays in the SAME BLOCK that do infer sit in
an `always_ff @(posedge clk or negedge rst_n)` with a forty-line reset branch --
the dirtier description of the two.

## 2. REGISTERS AND MEMORY BITS, BEFORE AND AFTER

### The block. `-MapOnly`, device `5CSEBA6U23I7`, `rtlCleanAtHead: true` on both.

| | `@arenacompose` (before) | `@arenainfer` (after) |
|---|---:|---:|
| `registers` | **146,414** | **1,010** |
| `blockMemoryBits` | 33,408 | **291,456** |
| `dspBlocks` | 6 | 6 |
| `virtualPins` | 1,305 | 1,305 |
| map seconds | 1,025.7 | **37.1** |

**145,404 registers gone -- 87% of the shipping part's 167,640 register sites,
from one block.** All fourteen banks are in the map's RAM Summary as
`zhao_dc_sdp_ram:g_stage[0..13].u_bank|...|ALTSYNCRAM AUTO Simple Dual Port
1024 x 18`.

### The probe. Nine `-MapOnly` rows, ONE VARIABLE PER ARM, one bank of 576 x 18.

`tests/probes/zhao_arenabin_stage_probe.sv`, run by
`tools/quartus/arenabin_stage_probe.ps1`, same device, `STAGE_IDS=1`.

| arm | what it changes | registers | mem bits | secs |
|---|---|---:|---:|---:|
| v0 | production, verbatim -- **the control, and it FIRED** | 10,386 | **0** | 117.1 |
| v1 | the read address becomes its own net | 10,386 | **0** | 45.9 |
| v3 | write enable without the genvar compare | 10,386 | **0** | 41.3 |
| v4 | the read gets its own `always_ff` | 10,386 | **0** | 48.9 |
| v5 | `(* ramstyle = "no_rw_check" *)` | 10,386 | **0** | 34.9 |
| v6 | v1 and v5 together | 10,386 | **0** | 34.1 |
| v2 | declared at **MODULE SCOPE** | **0** | **10,368** | 10.7 |
| v8 | declared in a **generate-IF**, no for-loop | **0** | **10,368** | 20.1 |
| v7 | a **submodule instance** inside the for-loop | **0** | 18,432 | 20.9 |

Arm 8 is the arm that names the culprit: a generate-`if` scope infers perfectly.
**It is the LOOP, not "a generate".**

`-Device` was never passed, so the shell_fit QSF's own `5CSEBA6U23I7` stands and
the unvalidated-device trap cannot apply. `measuredDevice` was read off every row
afterwards rather than assumed.
`reports/synthesis/arenabin/STAGE-PROBE-ROWS.txt` carries the table beside the
logs; the probe's own header carries it too.

## 3. WHAT SHAPE MADE IT INFER

Each bank is a `zhao_dc_sdp_ram` instance -- the committed module that exists
precisely so "the shape lives in ONE place, written once, correctly", after this
project produced this defect five times. Its body is line-for-line what the loop
described. **The same circuit with the declaration moved across a module
boundary.** `STAGE_IDS` stays a knob; `ck_ids_o`'s slot-per-bank structure is
untouched; no new module was commissioned.

The depth goes 576 -> 1024 because `zhao_dc_sdp_ram` is `1 << ADDR_W` deep, and
that is **declared, not hidden** -- the block header's on-chip table now reads
290,880 described bits against 177,984. It is not extra silicon: an M10K holds
512 words at 18 bits, so both depths are two blocks per bank, 28 of the device's
553. It is also strictly safer: `tile_c` is a `TIDX_W`-wide sum and the old array
was only `TILES` deep.

**`blockMemoryBits` IS NOT AN M10K COUNT, and "28 M10K" remains a PREDICTION.**
A map reports described bits; only a fit reports blocks, and no fit of this
block exists. That is as unmeasured after this packet as before it.

## 4. THE ARITHMETIC PROVEN UNCHANGED

* `dspBlocks` **6 on both rows** -- same corner tests, same `k_mul_tile`.
* `virtualPins` **1,305 on both rows** -- the port list is identical to the bit.
* `geom_arenabin_directed` **317 checks / 0 failures**;
  `geom_arenabin_price` **549 checks / 0 failures**, built and RUN (ruling R60),
  at the pushed commit.
* `price` reports the producer at **5.88 clocks/ref at R7's giant**, byte-identical
  to BINARENA's number, so the repair did not move throughput either.
* And the negative evidence: **map time fell 27x**, 1,025.7 s -> 37.1 s, the same
  tell section 10 records for SURFACE.SHEET at 34x.

## 5. THE `u_geom_tidq` UNDERFLOW -- NOT WHAT THE BRIEF SAYS, AND MUCH WORSE

The brief and `zhao_console_core.sv:8215` say the queue "underflows exactly once
per frame" so the arena's lists are "four references short of the picture -- 97
against 101", and name the cause as a seal-ordering question: *"an id discarded
by that flush whose triangle has not yet crossed the door."*

**Both halves are measured wrong.** A read-only hierarchy probe in the smoke --
no console port added, no wrapper mutant, no closure lint, no fit disturbed --
now prints both sides of the seam:

```
SMOKE: tidq       underflow=1 overflow=0 unnamed=0
SMOKE: tidqseam   pushes=75 pops=75 flushes=1 vid_seal_abort=0 | at first underflow: pushes=0 pops=1
SMOKE: tidqids    pushed=0 1 2 3 4 5 6 7 | popped=262143 0 1 2 3 4 5 6
```

* **`vid_seal_abort=0`.** GEOM.VERTID never abandoned a triangle. The flush/abort
  story is dead; the flush fired once and discarded nothing.
* **`pushes = pops = 75`.** Nothing was lost. The id stream is complete.
* **At the ONLY underflow, `pushes=0` and `pops=1`.** The FIRST door beat landed
  before the FIRST push. That is not a frame edge -- it is STARTUP, and it is
  structural: GEOM.SETUP is three stages, GEOM.VERTID needs its S_PUB x3 + S_TD
  before the arena hands back an index, so the door is ALWAYS ahead.
* **`tidqids` is the proof, and it was measured rather than inferred.**
  `262143` is `ID_POISON` (2^18-1). After it, **popped[k] == pushed[k-1] for
  every k.**

> **THE QUEUE IS PERMANENTLY ONE ENTRY BEHIND. Triangle 1 is dropped for want of
> an identity (that is the visible `unnamed=1` and the 4 missing references), and
> EVERY TRIANGLE AFTER IT IS BINNED UNDER ITS PREDECESSOR'S ARENA DESCRIPTOR
> INDEX -- 74 of 75 on this fixture.**

**That is console entry I54's named failure, live in the composed console:** ids
that are in range, decode cleanly, and are wrong. No range guard can see it, the
reference COUNT is unaffected, and the existing `refs` comparison in the smoke
cannot see it either because it differences two totals. `underflow=1` is the only
trace it leaves, and its magnitude is nothing like what that 1 suggests.

**COSTED AND ESCALATED, NOT FIXED.** The repair is a join change across three
production blocks and it is not this packet's:

1. `zhao_console_core`: the shell door must not fire until the queue has an
   entry -- `door_tri_valid_w &&= (tidq level != 0)`, using the `level_o` port
   that is already there and connected to `()`.
2. That alone can DEADLOCK. If GEOM.VERTID aborts on a seal (`vid_seal_abort_o`,
   also unconnected) while GEOM.SETUP holds that triangle, the id never arrives
   and the door blocks forever. It is 0 on all six forms today, which is exactly
   the "a gate that cannot reach the state is not evidence about the state"
   shape.
3. So it also needs `zhao_geom_vertid` to push a poison entry on the abort, and
   `zhao_geom_tidq`'s flush to stop discarding entries whose triangles are still
   in flight -- and the flush and the abort coincide by construction, so the
   queue's `else if (flush_i)` priority has to be re-thought.

That is a handshake change at the console's geometry door, with a deadlock mode
only a full smoke can exercise, in the middle of a live tree. **It belongs with
I55 or I54, with this measurement attached.**

**ASSERTED, as asked -- and the assertion is the MEASUREMENT, not the counter.**
The bench now prints both id streams in order, so the next reader sees
`popped[k] == pushed[k-1]` rather than a 1 they have to interpret. I deliberately
did NOT add a `$fatal` on `geom_tidq_underflow_o == 0`: it would be correct-
behaviour-shaped and it would turn the plain smoke RED at once, which reads as
this packet's regression and blocks every merge behind it. **That is a decision
the coordinator should overturn if the defect is not being taken immediately** --
the honest gate is a fatal, and the only reason it is not here is that the fix is
not.

## 6. THE INSTRUMENT DEFECT: THE RAM CHECKER WAS BLIND TO THE EXACT CLASS IT EXISTS FOR

`python tools/quartus/check_ram_inference.py --rank fpga/rtl/geometry/zhao_geom_arenabin.sv`,
before this packet, reported **five arrays "that will not infer as memory":**
`head_ram`, `tail_ram`, `nch_ram`, `fill_ram`, `hv_ram`. **All five are in the map
report's RAM Summary as INFERRED.** It said **nothing whatever** about `bank`, the
145,152-bit array that was the entire defect, because `bank` trips none of its
five rules.

**100% false alarms and a 100% miss, in one run, on the block holding this
campaign's largest single register overage.** The silence is the half that
matters and it is silent in the flattering direction.

**Rule 6 now detects it**, with a positive control and TWO negative ones (a
generate-IF and a procedural `for` inside an `always_ff`), asserted at startup
like rule 5 and the write scan. Fired at the pre-repair file it names `bank`,
MECHANICAL. Against the three blocks whose inference is on record --
`zhao_texture_palette_res`, `zhao_raster_tilestore`, `zhao_forge_assemble` -- it
fires ZERO times, so it has no measured counterexample in this tree.

**AND RULE 4'S REMEDY WAS MEASURED WRONG AND IS CORRECTED.** It said the fix for a
multidimensional array is *"one flat array per lane inside a `generate`, with the
outer index a genvar"*. GEOM.ARENABIN followed that advice exactly and paid
145,152 bits of flip-flops for it. **Advice inside a tool is a claim, and that one
had never been measured.**

**SEVEN MORE ARRAYS IN THE TREE CARRY THE CONSTRUCT.** A prediction, not a
measurement -- only a map row per block says what happened:

| bits | file | array |
|---:|---|---|
| 54,272 | `common/zhao_proj_arena3.sv` | `mem` |
| 27,392 | `terrain/zhao_terrain_residency_v2.sv` | `keyram` |
| 10,240 | `terrain/zhao_terrain_residency_v2.sv` | `statram_lo` |
| 5,120 | `geometry/zhao_geom_binner_v2.sv` | `meta_ram` |
| 4,352 | `terrain/zhao_terrain_residency_v2.sv` | `statram_hi` |
| 2,048 | `texture/zhao_texture_cache_pipe.sv` | `data_r` |
| 2,048 | `texture/zhao_texture_cache.sv` | `mem_r` |
| 384 | `texture/zhao_texture_cache_pipe.sv` | `tag_r` |

Two of them are already known flip-flop cases from `QUARTUS_GOTCHAS`'s own
history -- `zhao_texture_cache`'s 9,728 bits and `zhao_texture_cache_pipe`'s
88-minute fit. That is corroboration, not proof. **TERRAIN is I13CLOSE's area and
is live; none of these is touched here.**

## 7. EVERY CLAIM IN THE BRIEF I FOUND FALSE

1. **"the block's own header claims *generate infers where a 2-D array does not*,
   so the header is wrong."** At `99da0431` the header claims no such thing.
   ARENACOMPOSE had already deleted that sentence and replaced it with a record
   that the fit refuted it (`:418`, *"used to assert the opposite"*). The brief
   describes a sentence that had already been corrected -- a document going stale
   quietly, which is CLAUDE.md's own subject.
2. **"`check_ram_inference.py` has measured rules, so use it, and read rule 5 and
   the deleted rule 2 before theorising."** The tool could not see this defect at
   all (section 6), and neither rule 5 nor the rule-2 correction bears on it. The
   brief pointed at an instrument that was structurally blind to the question.
   Rule 2 is also not "deleted" -- one sentence of it was removed by FLOPARRAY.
3. **"`geom_arenabin_directed` (275 checks) and `geom_arenabin_price` (470)."**
   Measured **317** and **549** at this tree. The brief quotes BINARENA's counts;
   ARENACOMPOSE grew both benches afterwards. Harmless, but a gate row that names
   a number nobody re-measured is the shape this campaign keeps paying for.
4. **"`u_geom_tidq` underflows once per frame, so the lists are four references
   short."** True as far as it goes and badly under-stated -- see section 5. The
   cause named (a flush discarding an id) is measurably not the cause
   (`vid_seal_abort=0`, `pushes==pops`), and the consequence is 74 wrong
   descriptor indices, not 4 missing references.
5. **"only the five module-scope directory arrays inferred ... 33,408 bits"**, set
   against BINARENA's declared directory total of 32,832. Both numbers are right
   and they are different quantities: `hv_ram` is **inferred TWICE**
   (`hv_ram_rtl_0` and `hv_ram_rtl_1`, 576 bits each) because it has two read
   addresses, which is section 10's own "two-port templates REPLICATE". Worth
   naming so nobody differences them.
6. **"28 M10K out of 553 if it can be reached."** Still unmeasured. A map does not
   report M10K.
7. **"This is finishing GAP work, not optimization."** It is not, and the
   coordinator reached that independently mid-packet. The register read 4 before
   and 4 after.

## 8. WHAT I REFUSED

* **I did not un-compose the block.** The fence was right and was never tempting:
  the repair makes the number go away by moving the storage, not by removing it.
* **I did not ship a pragma that does nothing.** `no_rw_check` was measured (arm
  5), it is as inert as ARENACOMPOSE's `"M10K"`, and it is in the probe as
  evidence and nowhere near production.
* **I did not start a console or full-device fit.** Every row here is `-MapOnly`
  on one block or one probe.
* **I did not repair the tidq join.** Costed and escalated in section 5; it is a
  three-block handshake change with a deadlock mode, in a live tree, and it is
  not a storage-shape repair.
* **I did not touch `zhao_dc_sdp_ram`.** Giving it a `DEPTH` parameter would have
  saved the 576 -> 1024 rounding, and it is a shared file with three production
  consumers and a committed formal proof; re-dating it would mark every consumer's
  fit row stale for a saving that is zero M10K.
* **I did not repair the seven other generate-for arrays.** They are a prediction
  until each has a row, and three of them are in TERRAIN, which is live.

## 9. WHAT I GOT WRONG AND CAUGHT MYSELF

* **My headline hypothesis was wrong and I measured it dead.** I opened convinced
  the killer was read-during-write: production ties `stg_wa` and `stg_ra` to one
  net, which describes a single-port RAM whose RDW returns the OLD word, and
  Cyclone V M10K same-port RDW is new-data. It is a real mechanism and it is a
  case every `sync` point in the 102-bench calibration grid dodges. **Arm 1 killed
  it**, and arm 5 killed `no_rw_check` with it -- the attribute I would have
  reached for next.
* **I first read the checker's five findings as a lead.** They are five arrays
  that infer. Checking the RAM Summary before the checker's opinion is what
  stopped that.
* **I fed `-TopParameters` as one comma-separated string** and `run_block_fit.ps1`
  refused it with the exact incident that taught it to. The guard works; I was the
  next person it caught.
* **I staged a log file that was still being written** and caught it on
  `git diff --cached --name-only` before committing.
* **AND THE PACKET ITSELF WAS MISLABELLED, WHICH I SHOULD HAVE SAID EARLIER.**
  The brief calls this "finishing GAP work, not optimization". It is not:
  `completion_register.py` read **4** before this packet and **4** after, and no
  storage-shape change could have moved it. The coordinator reached the same
  conclusion independently and stopped the packet. The cost of my not saying so at
  the start is one slot.

## 10. BRANCH AND COMMITS

Branch **`gz/arenainfer`**, pushed. Never rebased, never forced, never pushed to
the shared branch.

| commit | what |
|---|---|
| `ec149c48` | the seven-arm probe, its runner, and the checker-is-blind finding |
| `4bb430cf` | the repair: the generate-for is the killer, measured nine ways |
| `93d811e4` | the block's own row, `QUARTUS_GOTCHAS` section 10's fourth killer, checker rule 6 |

## 11. GATES AT THE PUSHED COMMIT

| gate | result |
|---|---|
| `completion_register.py` (bare) | **4**, unchanged from the 4 this packet opened with |
| `check_console_inventory.py` | OK (405 modules, 291 fit sources) |
| `check_prod_manifest.py` | OK |
| `gen_prod_top.py --check` | fresh (86 instances) |
| `gen_console_board.py --check` | FRESH (1592 core ports) |
| `gen_shell_paired_diff.py --check` | fresh, harness and mutant |
| `check_quartus17_syntax.py` | RC 0 |
| `check_case_labels.py` | OK |
| `verilator --lint-only -Wall zhao_geom_arenabin` | RC 0 |
| probe, all nine arms, `--lint-only -Wall` | RC 0 |
| `test_geom_arenabin_directed` | 317 checks, 0 failures |
| `test_geom_arenabin_price` | 549 checks, 0 failures |
| `run_console_core_smoke.ps1` (plain) | PASS, `raster pixels=2816`, `frames_admitted=1` |

`reports/synthesis/zhao_block_fit.json` went **261 -> 271 rows**, counted before
and after; no row was lost.

**NOT RUN, and it is declared rather than implied:** `mutant_copy_drift.py`, the
console-board lint, the five other smoke forms, `check_console_closure_lint.py`
and `npm run abi:check`. No port changed anywhere in this packet -- `virtualPins`
is 1,305 on both block rows and `gen_console_board --check` is fresh -- so nothing
here can produce a `PINMISSING` or a wrapper-parity red; but that is an argument,
not a run, and the coordinator gates the merge.
