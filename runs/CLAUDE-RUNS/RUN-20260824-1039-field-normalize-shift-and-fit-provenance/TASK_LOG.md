# Task Log: RUN-20260824-1039 — normalize dynamic shifts, and fit provenance

**Created:** 2026-08-24 10:39 UTC+02:00
**Status:** In Progress

---

## Objective

Two things, one of which I caused:

1. **Field wave 1** — remove the two dynamic barrel shifts from
   `zhao_field_normalize`'s mantissa path, which the Field ruling names as wave 1
   and which was still unimplemented after the leading-zero work.
2. **Fit provenance** — `run_block_fit.ps1` measures whatever is in the working
   tree at the moment Quartus reads it. I found this by breaking it.

---

## Progress Timeline

### 10:33 — I edited RTL while a fit was running

The rule I have enforced on every agent this week. The `zhao_field_seq` fit
launched before the break was still running when I changed
`zhao_field_normalize.sv`.

**And the harness does not copy sources.** `run_block_fit.ps1:325` emits
`set_global_assignment -name SYSTEMVERILOG_FILE <absolute path into the working
tree>`, so the fit elaborates the live file.

The timestamps decided it:

    edit        10:33:06
    map report  10:34:47     <- 101 seconds LATER

The edit sits inside the window where analysis was reading sources. **Which
version was elaborated cannot be established**, so the fit was killed. It may
have been a perfectly good measurement. It could not be *shown* to be, and an
unprovable number is worth nothing here — that is the whole argument this
project keeps re-learning.

### 10:40 — wave 1, and it is exact rather than approximate

The mantissa path was:

```systemverilog
rsh   = (d_exp > 0) ?  d_exp : 0;
lsh   = (d_exp < 0) ? -d_exp : 0;
m_val = 24'((h_rt >> rsh) << lsh);
```

Two 64-bit barrel shifts in series, built and muxed, to answer a question the
leading-zero search **has already answered**. `lz_t` is `h_rt << lz`, so its
top set bit is at bit 63 by construction and the normalised mantissa is a slice:

```systemverilog
assign m_val = (h_rt == 64'd0) ? 24'd0 : lz_t[63:40];
```

Equivalent in both directions — for `n >= 23` the old form shifts right by
`n-23` and drops the low bits, and `lz_t[63:40]` is those same bits; for
`n < 23` it shifts left by `23-n` and zero-fills, which is where the left shift
by `lz = 63-n` had already put them. Either way the MSB lands at bit 23.

This also retires the `lz_t_unused` hack that existed purely to keep `lz_t`
alive for `-Wall`. It is load-bearing now.

**Tests: 4/4**, including `field_normalize_random_nightly` at **50,000 random
cases**, plus lint clean — which is what confirms `rsh`, `lsh` and
`lz_t_unused` left nothing dangling.

### 10:45 — the guard, and three controls that tested nothing

The flow now hashes every source named in the QSF at QSF-write time, again the
moment `quartus_map` finishes, and once at the end. Any difference sets
`status = contaminated:source-changed-during-fit` **before the summary is
parsed**, so such a row cannot reach `ok` and cannot enter the census.

**Getting a control to fire took four attempts, and the first three failed
silently in three different ways:**

1. edited at a fixed `t=8s` — **before the QSF existed**, so the hash was taken
   of the already-edited file and nothing ever differed;
2. polled for the QSF and matched a **stale workspace**. There are **36 orphaned
   `zhao-block-fit-*` directories** in `%TEMP%`; it fired at `t=0.5s`, again
   before the real run had hashed anything;
3. baselined the workspaces correctly and then edited **terrain files, which are
   not in the 27-file shell cone**. The guard ignored them because it is
   supposed to.

`sourcesHashed: 27` on the row is what made (3) legible, and it matches
`grep -c SYSTEMVERILOG_FILE` on the shell QSF exactly.

> **A control that reports "not detected" is not evidence that the detector is
> broken.** Three times running, it was evidence that the control was. This is
> the same shape as the worktree checkout that silently did not take — and the
> counter-lesson to it, because there the tool was fine and the *observer* was
> wrong in the other direction.

### 10:55 — BOTH ARMS PROVEN

| arm | control | result |
| --- | --- | --- |
| start-vs-end | `zhao_debug_counters.sv`, left edited | **CONTAMINATED**, ALM suppressed |
| during-map | `zhao_input_rumble.sv`, **reverted after the map** so the tree ends byte-identical | **CONTAMINATED** |
| negative | three clean runs | `ok`, `sourcesHashed: 27` |

The during-map arm is the one worth having: at the end of that run the file was
byte-identical to its committed form, so start-vs-end alone would have called it
clean.

**The harness behaved better than I designed.** On contamination it prints
`KEEPING the previous measurement (222 ALM / 0 DSP) rather than erasing it` —
so a poisoned attempt cannot destroy a good row either.

**One self-inflicted casualty:** the first control restored its file with
`Set-Content -NoNewline`, which rewrote all 1,452 lines with different line
endings. Restored with `git checkout`; later controls use
`[IO.File]::WriteAllBytes` for a byte-exact revert.


### 11:20 — the gate found two failures and NEITHER was noise

`ctest -L fast`, 630 s:

* **`format_check` was ALREADY RED at `b3a16a0`.** The violation is in
  `tests/terrain/terrain_normals_directed.cpp` — the input-poisoning lines from
  this morning's `TERRAIN.NORMALS` run — and `git status` shows the file
  unmodified, so it was **committed in violation**. That run reported its gates
  green. Formatted with the pinned `node_modules/clang-format` (the version pin
  is the point: a system clang-format from another LLVM reformats half the tree).

  `git diff --stat` claimed 262 insertions/deletions on a file I had not
  touched; `git status --porcelain` showed it clean. That is the `core.autocrlf`
  artefact this repo already documents — **the diff was not evidence of a
  change.**

* **`ledger_check` V20 fired on MY OWN comment.** `zhao_field_normalize.sv:288`
  states "by construction" and "exactly equivalent, in both directions" with no
  `ENFORCED-BY:` within ten lines. The rule is right and I broke it: an
  equivalence argument written in a comment is exactly the kind of claim that
  rots silently. Now names
  `tests/differential/field_normalize_directed.cpp:main`, matching the
  convention already used at line 255 of the same file.

Errors 2 → 1. The remainder is the recorded V16 `FIELD.SEQ.CORE` baseline.

### 11:35 — committed the harness work, `92e279d`

Separately from the RTL, because it stands on its own and the RTL still owed a
sweep.

### 11:40 — the sweep, and a mutant whose TARGET no longer exists

Removing `rsh`/`lsh` broke two mutants:

* **M36** ("the zero-length guard is dropped") merely moved — re-anchored on the
  slice form. Its equivalence proof survives the move because it is semantic:
  with `h_rt == 0` the leading-zero search leaves `lz_t` zero, so `lz_t[63:40]`
  is zero anyway, exactly as `(0 >> 0) << 0` was.
* **M35** ("the normalise shifts go the wrong way") **cannot be expressed at
  all** now. It swapped two signals that no longer exist.

**Deleting M35 would have shrunk the sweep silently**, which is the failure this
project has already recorded in the other direction (a mutant surviving because
it no longer does anything). It was given a **successor testing the same
property against the structure that exists**: the mantissa is taken from the
right bits, `lz_t[63:40]` → `lz_t[62:39]`. That one should be CAUGHT, unlike
M36.

Preflight: **38 mutants across 11 files, 0 do not build** — non-zero count
parsed, both re-anchored mutants confirmed buildable.

### 11:50 — SWEEP 38/38, and the successor mutant is CAUGHT

    attempted=38  expected=38  accounted=38  caught=33
    SURVIVOR: M01, M07, M20, M36, M38

**No mutant was discarded by the re-elaboration hash guard** (`attempted` equals
`accounted`), so all 38 genuinely rebuilt. The five survivors are the same five
as before and all are documented proven equivalents — **no coverage hole**.

**M35's successor is caught**, which is the result that matters: the slice form
is genuinely covered, not merely un-mutated. Had I deleted M35 when its target
vanished, the score would still have read 33/37 and looked identical.

Committed `440a1a1`, pushed. Harness work committed separately at `92e279d`.

### 11:56 — THE FORMAL LANE HAS BEEN DEAD SINCE THE 23rd, BEHIND A `pending` LABEL

The last ledger error is V16: `FIELD.SEQ.CORE` is `RTL_VERIFIED` while
`tests/formal/field_seq_bound.sby` is recorded `pending`. The entry says the
status flips "only by a run that passes inside the lane's own budget", so I ran
it.

**It did not elaborate at all. It failed in 0.92 seconds.**

    zhao_field_seq.sv:328: error: identifier 'exec_writes' used before its declaration
    ... 7 errors, then 4 more at :368 for rd_a_q/rd_b_q/rd_c_q/rd_h_q

**The register-file rewrite did this.** It put a combinational write decode and
a clocked memory process *above* the declarations they reference. Verilator
accepts a forward reference; **`read_slang`, the formal frontend, does not.**
Eleven errors, two ordering classes, no signal changed meaning.

> **A red already labelled *expected* stops being looked at.** V16 exists so
> banked evidence cannot back a maturity claim, and it was doing that job
> honestly — but the same label removed the only pressure that would have
> revealed the lane could not even start. The entry was truthful and it still
> hid a completely broken lane.

This is the sibling of the `SKIP`-if-absent gate that hid weeks of drift, and of
the map-only census that looked healthy because the scanner walked the wrong
branch. **The failure is never the label; it is that a label ends the looking.**

### 12:25 — elaborating, and the wall time is finally being measured

Depth **129 of 172**, every property UNSATISFIABLE so far, `cover` queued behind
`bmc` on the single solver. The derivation did **not** need re-deriving: it keys
off the package constant `MAX_OP_CYCLES = 80`, not measured latency, and
NORMALIZE3's worst case at 67 + 1 for the new state is still far under it.

Deliberately running **no builds or tests concurrently** — the number this owes
the ledger is the run's *solo* wall time, and contending for CPU would inflate
the very measurement being taken.

Noted in passing: yosys warns `no driver for u_norm.r24_next[31:24]`. Benign and
pre-existing — `r24_next` is clamped to `0x00FF_FFFF` and only `[23:0]` is ever
consumed, so the top bits are optimised away. Not from the mantissa change.

### 12:26 — the proof does not FAIL, it does not FIT

    Total Test time (real) = 1800.20 sec   <- exactly the budget

`ZHAO_FORMAL_WRAPPER_TIMEOUT` is **1800**, and `tests/CMakeLists.txt:65` says
the wrapper budget is the one that BINDS — a lane's CTest `TIMEOUT` is only a
backstop. So this is a **timeout at k~130 of 172**, not a violated property.
Every bound reached was UNSATISFIABLE.

The policy for this is already written and sanctioned: *"a lane that needs more
sets it before its `configure_file` and puts it back afterwards"*, with
`RUN_SERIAL TRUE` because **"wall time must approximate solo time"**. That is
also the justification for not co-scheduling anything while measuring.

`mem_vram_arbiter_liveness` went through exactly this in August and its comment
carries the warning worth repeating: a proof lane that times out gets read as a
failure, **or gets "fixed" by deleting tasks.**

Measuring the solo number rather than picking a round one. At 13:25 it is at
**k=140 after 57 minutes**, and the tail is steepening — k=130 took ~29 min and
the next ten bounds took another 28. This is a multi-hour lane, which is itself
the finding.

### 13:30 — I MISREAD A SUPERSEDED SECTION AS A LIVE BLOCKER

Chasing FRAMEBLIT (priority a), `REMAINING_BLOCKERS.md` appeared to say CMD.DMA
still had two defects — a one-cycle payload CRC re-walk and a `slot_buf` that
could not infer as RAM — described as *"NOT yet done, and it is a real
redesign"*, gating the composed fit and FRAMEBLIT step 8.

**All of it is already done, and the document says so.** The text I read sits
inside a block headed *"(SUPERSEDED, kept for the reasoning)"*, and one
paragraph above it the live text reads **"This unblocks the composed fit and
FRAMEBLIT step 8."**

The evidence agrees, which is why I caught it:

| check | result |
| --- | --- |
| `slot_ram` inference | **32,768 memory bits** on the map — exactly 4,096 bytes |
| the fit that "never succeeded" | `status ok`, **3,607 ALMs**, 1,293 s |
| the commit | `f5e067e`, *"CMD.DMA FITS: 83,977 ALMs -> 3,607, and the staging buffer is real memory"*, **2026-08-22** |
| the CRC re-walk | gone at `fd262de`; the RTL says *"crc_final() is gone"* and folds per beat |

> **Reading a long report is a lane with its own failure mode.** A superseded
> section is not stale data to be corrected — it is correct data about the past,
> and treating it as current would have had me "fix" something already fixed and
> then report a blocker cleared that was never blocking. The label that saved
> this was the document's own, written by whoever chose to keep the reasoning.

So **FRAMEBLIT step 8 is unblocked, and it is "rerun the composed Quartus
synthesis"** — a machine task, queued behind the formal measurement, since two
heavy Quartus/solver jobs must not co-schedule.

### 13:45 — I CORRUPTED THE MEASUREMENT I HAD SPENT AN HOUR PROTECTING

`ctest -R "field_seq"` also matches **`formal_field_seq_bound`**. So the run I
started to verify the declaration reorder launched a **second copy of the same
two-hour proof**, which then competed with the solo measurement for about thirty
minutes before timing out at 1800 s.

An hour of refusing to co-schedule anything, undone by an unanchored regex.

The four lanes I actually wanted all passed — `field_seq_directed`,
`field_seq_random`, `field_seq_random_nightly`, `lint_zhao_field_seq` — so the
reorder is behaviour-neutral, and the sweep preflight still reports **38 mutants,
0 do not build**, meaning no anchor moved. Committed `0d80f64`, pushed.

**The wall-time figure is therefore an upper bound, and is recorded as one.**
For sizing a timeout that errs in the safe direction — the lane's own warning is
about lanes that pass solo and fail contended, i.e. under-sizing — but it is not
the solo number the policy asks for and must not be written down as though it
were.

### 14:30 — THE PROOF PASSES

    engine_0 (btor btormc) returned pass
    engine_0 did not produce any traces
    DONE (PASS, rc=0)
    Elapsed clock time: 2:02:17 (7337 s)

**`bmc` passes at depth 172.** 19 inputs, 152 states, 4 bad-state properties, 6
constraints; every bound UNSATISFIABLE and no trace produced.

**7,337 s is 4.1x the 1,800 s wrapper budget** — with ~30 minutes of that
contended by my duplicate. The lane was never failing. It was never *finishing*.

That also settles the depth question the RF change raised: the derivation keys
off the package constant `MAX_OP_CYCLES = 80`, not measured latency, so
NORMALIZE3's worst case at 67 + 1 for the new state stays far under it and **172
did not need re-deriving.**

### 14:45 — GREEN, and the ledger has zero errors for the first time

    bmc   depth 172   PASS   2:02:17 (7,337 s)   no trace
    cover              PASS   15.9 s              all four traces

**Three separate things were wrong and none of them was the property.**

1. it could not **elaborate** since the 23rd (`read_slang` vs Verilator);
2. the **budget** was 4.1x too small — 1800 s against a 7,337 s run;
3. the **entry** said `pending`, which is what stopped anyone looking.

Wrapper raised to **12,000 s** and CTest to **12,600 s**, via the per-lane
override `tests/CMakeLists.txt` already sanctions. Verified after reconfigure
rather than assumed: the generated `field_seq_bound` wrapper reads
`TIMEOUT 12000` and `cmd_dma_crc_gate` still reads `1800`, so the override is
both applied and **restored**. CTest sits above the wrapper on purpose, so the
wrapper fails first with a readable sby message — the `formal_texture_bilerp`
rule, where CTest 3600 against wrapper 1800 gave a bare FAILED at 1,800.88 s.

    ledger_check   2 errors -> 0
    ctest -L fast  green, 272 tests

Committed `764b2a0`, pushed.

### 14:50 — FRAMEBLIT step 8 started

Step 8 of the integration is *"rerun the composed Quartus synthesis
immediately"*, and it was gated on CMD.DMA, which cleared on the 22nd. The
machine is free for the first time today, so it is running now at
`-Processors 4`.

The old reason this needed a second machine — a 28.4 GB peak — **was a bug, not
a requirement**: `VIRTUAL_PIN ON -to *` matched every internal net rather than
the 101 top-level ports. Fixed at `d1a2b8a`; the real peak is 6.2 GB.

### 15:16 — STEP 8 ANSWERED: the composed shell fits, and misses one clock

`run_composed_fit.ps1 -Processors 4`, run `wumen-764b2a0-20260824T124441Z`,
**1,889 s**, all four stages `success`.

| | composed shell | device |
| --- | ---: | ---: |
| ALMs | **7,442** | 41,910 (**17.8%**) |
| registers | 9,926 | |
| block memory | 114,688 bits, **13** M10K | 553 |
| DSPs | **0** | 112 |
| virtual pins | 2,336 | |

**Timing fails, and on exactly one domain:**

    setup  -1.991 ns  836 endpoints  TNS -253.490   gpu_clk   (10 ns target)
           +1.469 ns    0 endpoints                 vid_clk   (20 ns)
          +29.416 ns    0 endpoints                 audio_clk (40 ns)
    hold   -0.952 ns    1 endpoint   TNS   -0.952   gpu_clk

`gpu_clk` closes at **83.4 MHz** against 100 — 16.6% short. **The hold
violation is the one that cannot be waited out**: a setup miss means run it
slower, a hold miss means the design is wrong at any frequency. One endpoint.

**This also retires a standing caveat.** `REMAINING_BLOCKERS` has said since the
audit that WNS/TNS/hold extraction was *"fixed but UNPROVEN — do not quote a
slack number until a real `.sta.rpt` has been read."* One has now been read, and
the report carries `Info (332111): 10.000 gpu_clk` — the constraint-evidence
line whose ABSENCE is what made the old 199.72 MHz turn out to be 37. The lane
is trustworthy, and its first trustworthy statement is that we are short.

**Scope, stated so it cannot be over-read:** this is the 27-file SHELL cone —
scanout, CMD.DMA, SDRAM, HPS bridge, audio, debug. **Terrain, Field, raster,
texture and geometry are not in it.** A composed number for the renderer is a
separate run that has never been done.

The harness reported exit 1 on the push step; that was PowerShell's
`NativeCommandError` firing on git writing progress to stderr. The commit
(`be65ec0`) and the push both landed — checked rather than assumed.

### 15:20 — the hold violation could not be ACTED on, and that was a harness gap

The composed fit said `hold worst slack -0.952 ns, 1 failing endpoint` and
**nothing on disk said which endpoint.**

`report.tcl` has always written `setup/hold/recovery/removal_paths.rpt` at
`-detail full_path`. All four die with the workspace: the run directory kept the
JSON summaries and discarded the only artifact that makes a failure actionable.

The only path reports left on disk were in workspaces dated **2026-08-22** — a
different commit. Reading those to answer a question about today's fit would
have been precisely the error this repository keeps recording, so they were not
read.

> **A failing number you cannot act on is barely better than no number.** The
> hold violation is the single most important line in that report — a setup miss
> can be run slower, a hold miss is wrong at every frequency — and it was the
> least actionable thing in it.

Fixed at `f2680df`: `characterization/*.rpt`, `*.tsv` and the `.sta.rpt` are
copied into the report root. Same shape as the ticks-suffixed workspace — it
does not change what is measured, it changes whether it can be worked on.

Composed fit re-running at `f2680df` to capture the detail. It answers **both**
open timing items at once: the failing hold endpoint, and the critical-path
family behind the 836 setup endpoints.

### 16:00 — the CDC was BOTH headline numbers, and I had published the wrong one

With the path detail preserved, the composed fit reads:

    -1.991  starve_q[57]  vid_clk -> cdc_err  gpu_clk     <- a CDC, not logic
    -0.875  cmd_dma|hdr_win[28][5] -> crc_pay_r[8]        <- worst REAL path
    -0.765  f_pos[1] -> recq[2][*]  (a large family)

**The worst setup path and the only hold failure are the same crossing.**
`starve_q` is a 64-bit counter in `vid_clk`, sampled into `starve_samp` on
`gpu_clk`, and `cdc_err` is a **runtime tripwire** that raises if the counter
moves during the sample window — the premise being that it advances only during
active lines while the tick lands in vblank. TimeQuest analyses two unrelated
clocks and reports both numbers; neither is a defect.

**So I had to correct STATUS.md.** I published **83.4 MHz** to Fabian's channel
this morning. The real worst *synchronous* path is **-0.875 ns, ~92 MHz** —
short by 8%, not 17%. I also over-stated the hold finding: *"wrong at every
frequency"* is true of a synchronous path and false of an asynchronous one.

> **A summary line cannot tell you what KIND of failure it is.** The verdict
> said `timingPassed: false` and the worst number was an artefact of a decision
> we made on purpose. A permanently red verdict cannot report a regression —
> and it had already hidden one, because -0.875 was invisible behind -1.991.

Docketed for Fabian with three options and a recommendation, **not decided**:
silencing a measurement to turn a flag green is the move
`mem_vram_arbiter_liveness`'s comment warns about.

### 16:20 — priority (b) is NOT missing ops. It is timing

Checked rather than assumed, because "the rest of the Field IR engine" reads
like unbuilt work:

| op | RTL | reference | differential | ledger |
| --- | --- | --- | --- | --- |
| NORMALIZE2/3, CURVE, DCURVE, SPLINE, NOISE2, RING, RIDGE, ROT2, ROT3 | yes | yes | yes | 40 ops |

All ten are implemented, mirrored in `zref`, exercised by
`field_seq_directed.cpp`, and green. **Field's functionality is complete**; what
remains is waves 2/4/5/6, which are timing.

### 16:30 — GLUE 3, oracle first

The framer has **no shipped reference function** — it is shell glue. Its oracle
is behavioural, so it was established BEFORE the edit and run green first:
`shell_golden_replay`, `cmd_decoder_directed`, `cmd_scheduler_directed`.

    in_rec_region = (f_pos >= 36) && (f_pos < (pkt_len - 32'd4));

A 32-bit subtract in series with the compare that gates a **144-bit array
write**. `pkt_len - 4` is now registered, and the equivalence is **exact**:
`f_pos` resets when `f_pos + 1 >= pkt_len`, and the region needs `f_pos >= 36`,
so a one-cycle-lagged copy has been correct for 35 cycles before anything reads
it. Underflow preserved deliberately — the same expression, registered, so
`pkt_len < 4` still wraps rather than being quietly "fixed" under cover of a
timing change.

Green: shell golden replay (86 s), lint, duo markers, cmd decoder, and
`ctest -L fast` at 272 tests with the ledger at 0. Committed `86e27e3`.

**Whether it is worth anything is the fitter's call.** A composed re-fit is
running and the number goes in this log either way.

### 17:00 — ONE LINE TOOK 836 FAILING ENDPOINTS TO 16

Composed refit at `86e27e3`, same machine, same flow, same seed:

| | `f2680df` | `86e27e3` |
| --- | ---: | ---: |
| worst setup | -1.991 ns | **-0.423 ns** |
| failing setup endpoints | **836** | **16** |
| worst hold | -0.952 ns FAIL | **+0.254 ns PASS** |
| failing hold endpoints | 1 | **0** |
| gpu_clk | 83.4 / ~92 real | **95.9 MHz** |

The `f_pos -> recq[*]` family is gone from the path list, which is what the
change targeted. **The CDC dropped off the worst list too — and that is NOT a
fix.** Registering a subtract in the framer cannot repair a crossing between
unrelated clocks; relieving pressure let the fitter place it better, and
placement-dependent slack can come back. Recorded as a caveat, not a win.

The 16 survivors are ordinary synchronous logic:
`hps_arbiter|held_req.client[0] -> cmd_dma|crc_hdr_r[1]`, -0.423 down to -0.08.

### 17:10 — FABIAN SENT A FULL CLOSURE PLAN, and it was one fit out of date

`reports/ShellFixes.md` (`33b0a63`). Its diagnosis is right and matches what the
path detail showed independently: the 83.4 MHz is the monitored CDC, the real
problem is ~8% concentrated in two pieces of control logic, and **"do not lower
the GPU target to 92 MHz"** because the renderer's budgets assume 1,666,667
compute clocks per frame.

Two of its predictions had already resolved by the time it arrived:

* **item 3** (rewrite the framer as streaming hardware) states its expected
  result as *"the large `f_pos -> recq[*]` family disappears entirely."* **It
  already has** — from the one-line registered subtract, not the rewrite. The
  rewrite remains the better structure; it is no longer the timing lever.
* **item 4's prediction** that *"HPS arbiter state into CMD.DMA CRC control"*
  would surface next is **exactly what the fit now names.**

**item 2 applied** (`59de7ca`): `crc_pay_r`'s seed moved out of `M_HDR_CHK`'s
success branch — where the whole validation ladder sat in a register's clock
enable — to an unconditional seed at the end of `M_HCRC`. Checked against the
code rather than accepted: `crc_pay_r` is meaningless on header failure,
`M_SEED` is unreachable without success, and nothing folds into it in between.

Verified by the differential AND `formal_cmd_dma_crc_gate` — the right instrument
for a change to CRC seeding — plus `ctest -L fast` green at 272 and ledger 0.

Refit running. **One measured change per commit**, as section 6 requires.

### 17:20 — THE CDC IS A COIN FLIP, and that is the real result of the day

Four composed fits, **none of which touch the crossing**:

| fit | RTL change | CDC hold |
| --- | --- | ---: |
| `f2680df` | — | **-0.952 FAIL** |
| `86e27e3` | GLUE 3 subtract | **+0.254 pass** |
| `59de7ca` | CRC seed -> M_HCRC | **+0.259 pass** |
| `e617267` | CRC seed -> M_SEED_PREP | **-0.728 FAIL** |

1.2 ns of swing on placement alone. **`timingPassed` is therefore
nondeterministic**, which is worse than permanently red: a flag that flips at
random cannot be trusted in either direction, and a real regression arriving on
a lucky fit is indistinguishable from luck.

That promotes ShellFixes item 1 from *recommended* to **required**, on empirical
rather than architectural grounds. Docketed at `d950641`; **not taken
unilaterally**, because it changes a crossing with a stated safety premise and
needs its own formal properties.

### 17:25 — and the synchronous problem went DIFFUSE

| fit | worst setup | endpoints | worst family |
| --- | ---: | ---: | --- |
| `f2680df` | -1.991 | 836 | `f_pos -> recq[*]` |
| `86e27e3` | -0.423 | 16 | `hps_arbiter -> crc_hdr_r` |
| `59de7ca` | -0.621 | 60 | `m.M_HCRC -> crc_pay_r` |
| `e617267` | -0.570 | 36 | `scanout_fetch -> mem_guard|fwd_req.addr` |

**Three fits, three different worst families, all in -0.42..-0.62.** The
single-family era ended with GLUE 3. No path dominates, so further one-path
surgery has low expected value — and single-seed A/B can no longer resolve
0.2 ns from placement noise, which is exactly why my `M_SEED_PREP` relocation
still reads worse than the state before the CMD.DMA change.

> **The CDC repair is a precondition for judging anything else.** You cannot
> A/B a diffuse timing problem against a verdict that flips on placement.

### 17:30 — the renderer's FIRST composed measurement

`fpga/rtl/synth/zhao_pair_tess_normals.sv` (`1b17ff5`), the registered
characterization wrapper the audit asked for and nobody built.

**Every renderer number in the census is a LEAF fit.** `zhao_terrain_tess`
alone presents nine 32-bit vertex outputs as virtual pins, so the fitter is told
they are free and the seam that matters is never placed. The composed SHELL has
been measured four times; the RENDERER has never been built as one machine at
all.

Registered stimulus in, registered hash sink out, so nothing reaches a pin and
no lane can be optimised away for being unobserved. **The lattice is a
registered memory read, not a function of the request** — driving it
combinationally would flatter the number by deleting a real cycle boundary.

Limits written into the file rather than left implied: no true lattice depth, no
real height distribution, `nrm_ready_i` tied high. It answers "how fast can the
pair run when nothing stalls it", which is the question a clock target asks.

### 18:00 — TWO REVIEWS, ONE OBJECTION, AND THEY WERE BOTH RIGHT

Fabian commissioned a second opinion. Both reviews rejected my
"2 km islands or 110 DSPs" framing, **independently, for the same reason**, and
neither could settle it because the measurement did not exist.

The objection: **all 106 calibration points are SYMMETRIC.** The band table
(8..27 -> 1 DSP, 28..33 -> 3) was correct and was then applied to a case it had
never measured — narrowing ONE operand while the other stays 32 bits.

Twelve asymmetric points, three of them controls:

| a x b | DSP | | a x b | DSP |
| --- | ---: | --- | --- | ---: |
| 33 x 27 | **3** | | 27 x 27 | 1 *(control ok)* |
| 32 x 32 | 3 *(control ok)* | | 27 x 24 | 1 |
| **32 x 27** | **3** | | 27 x 18 | 1 |
| 32 x 24 | 3 | | 24 x 24 | 1 |
| **32 x 18** | **2** | | 24 x 18 | 1 |
| 28 x 28 | 3 *(control ok)* | | **23 x 11** | **1** |

**`32 x 27` costs exactly what `32 x 32` costs.** Narrowing the coordinate alone
recovers NOTHING.

> **The counter-evidence was in the design the whole time.**
> `zhao_project_core` has contained 32x27 products mapping at 3 DSPs each since
> it was written — 11 x 3 = 33, the measured total. I had the number and read it
> as confirmation of the band model instead of as a refutation of my own plan.

Two results nobody predicted: **32x18 = 2**, a real partial lever; and
**23x11 = 1**, which is what makes the BINNER fix worth doing.

### 18:10 — the architecture both reviews converged on

**Segmented coordinates.** A wide island/patch/instance origin plus a bounded
local delta, with the origin's contribution folded into the SAME pre-rounding
accumulator the projector already uses, and ONE final rescale. Bit-exact by
integer distributivity; every Q16.16 fraction bit kept; island extent bounded
only by fx16 itself at +-32 km. **No island cap. No format amendment.**

Where the two differed I sided with the first: camera-relative needs a far-cull
to bound the delta, and **this machine has no z clip by design**
(`zhao_geom_cull.sv:40-43`), so that route needs a fog ruling. Patch-origin
rebase bounds the delta BY CONSTRUCTION — a patch spans <=128 m at the coarsest
pitch.

### 18:20 — GEOM.BINNER, the one win that needs no ruling at all

Four products of a 23-bit slope by an 11-bit tile offset, multiplied out of a
**36-bit accumulator register**: 3 DSPs each, 12 total.

The proof is mechanical, not a bound: `tri_kx0_i` is `signed [22:0]`,
`kx_r[0] <= ext23(tri_kx0_i)` is PURE SIGN EXTENSION (`:435`), and `kx_r` is
assigned nowhere else. Bits [35:23] carry no information, so `k * t` and
`k[22:0] * t` are the same integer and every downstream truncation applies
identically.

`-Wall` objected that 13 bits of the function argument went unread — **correct,
and the point of the change**. Fixed properly by taking the argument at its real
23-bit width so the narrowing is visible at every call site rather than hidden.

Green: directed, random, random_nightly, lint, and the **formal arena-bounds
proof**. Expect 12 -> 4 on the map.

### 18:40 — ALL SEVEN OWNER RULINGS LANDED, and one corrects my language

Fabian ruled every open item. Recorded verbatim at `24c1625`. The one that
changes how I must *speak*, not just what I build:

> **"Do not claim 110 DSP saved until each affected block maps."**

The SetView bound **enables** the audit; it banks nothing. I have been quoting
110 as money in hand and it is not — and today's own BINNER result is the proof:
I predicted 4, measured 6, because I counted operator sites instead of reading
the loop bound. **The only DSP saving actually on the board is 12 -> 6.**

The rulings in one line each: SetView bounds the **nine multiplied** linear words
to signed 27-bit and rejects violations whole; BAKE radius 512 m rejected not
clamped; creature radius 128 m with rigid bone matrices and Loom scale kept
separate (<=4.0); `rescale_s32` is a **bug**, s128 throughout; scars are
**sheet-based** — no pool, no fading, 8 MiB canonical; Earth WRITE ops are **live
composition**, never persistent mutation; two-bone hardware stays behind the
existing measured all-clips error gate.

**And one of my questions was simply the wrong question.** I asked for a
scar-texture *pool size*. There is no pool: a stamp rasterises into the patch's
64x64 sheet and the record then occupies nothing. Ten thousand overlapping
impacts still leave one fixed sheet.

### 19:00 — THE CDC MAILBOX IS WIRED AND PROVED

`zhao_cdc_snapshot` — a one-entry toggle-handshake mailbox — replaces the raw
64-bit sample. **It replaces BOTH crossings, which is the part I nearly missed:**
`prov[6].value` was reading `starvation_o` (a `vid_clk` value) directly on a
`gpu_clk` tick, so the counter itself sampled across the boundary and the
tripwire merely *watched* it happen.

`formal_cdc_snapshot`: **bmc PASS, cover PASS** at depth 32. Six assertions, six
covers.

Two frontend corrections on the way: `read_slang` rejects concurrent SVA
outright — *"encountered unsupported SVA feature"* on all six `assert property`
— so the properties are **immediate assertions inside clocked blocks**, matching
`zhao_debug_frameblit`; and it hit the same declaration-before-use rule as the
Field sequencer, because the toggle pair is by nature referenced from both
domains.

### 19:10 — the golden replay caught it, and the DESIGN was right

    FAIL: golden: starvation constant: expected 0x0, got 0x280

**0x280 = 640, and this check's own tolerance is `<= 1024` ("two lines").** The
value was never wrong. What moved is *when it becomes visible*: the mailbox
publishes once per vid frame and the GPU collects it a few clocks later, so the
first real reading lands one frame after a torn read did. The test captured its
baseline at tick 2 — before any snapshot existed — saw 0, then saw the true 640
and called it non-constant.

Baseline capture moved to tick 3 **with the reason written beside it**. Same
shape as the Field register-file change: a deliberate latency change recorded
before the test moved, not a test bent to fit a result.

### 19:20 — THE LEDGER REFUSED ME TWICE MORE, AND WAS RIGHT BOTH TIMES

* **V19** — a bounded `bmc` proof must carry a scope guard that fires if the
  depth is raised, or an explicit waiver. Mine needs no guard: every assertion is
  **step-local**, relating now to `$past` one clock earlier, with no horizon and
  nothing accumulating toward a bound. `# SCOPE-TOTAL:` waiver written, and it
  states the contrast — in `field_seq_bound` the **depth IS the claim**, which is
  exactly why that one has a real guard.
* **V20** — `// stable for >= 2 dst clocks by construction` was an invariant
  claim with no enforcer. It now names `a_hold_stable_while_busy`, an assertion
  in the same file that **proves** the stability the comment asserts.

> Twice in one day the ledger has refused a comment of mine where the claim was
> TRUE but UNBACKED. That is the rule doing precisely what it exists for, and it
> is a better reviewer than I am on my own prose.

`shell_err_cdc_o` now means something a person can act on — the GPU side failed
to collect a snapshot before the video side had another. The old bit meant "the
counter moved while I happened to be looking", which is a statement about luck.

### 19:45 — THE CDC REPAIR, MEASURED. Best result of the day.

| | before (`e617267`) | after (`768c0ff`) |
| --- | ---: | ---: |
| worst setup | -0.570 ns, 36 | **-0.208 ns, 23** |
| worst hold | **-0.728 FAIL** | **+0.262, 0 — PASSES** |
| cross-domain paths in hold | 1 | **0** |
| gpu_clk | ~94.6 MHz | **~98.0 MHz** |

**2% short of target, from 17% short this morning.**

**The falsifiable prediction held.** I said beforehand the crossing should
DISAPPEAR from both worst lists rather than merely improve, because the only
thing crossing asynchronously now is a single toggle behind a three-flop
synchronizer. Measured: **0** occurrences in `setup_paths.rpt`, **0** vid<->gpu
paths anywhere in the hold list. The 72 remaining `starve_q` mentions are
vid->vid internal paths — the counter and the mailbox's source side.

It paid twice: the hold failure is gone AND setup improved 0.36 ns, because
relieving that path let the fitter place everything else better.

### 20:10 — I BUILT A BAD MEASUREMENT AND CAUGHT IT BY ARITHMETIC

TMU+CACHE pair, first fit:

    zhao_texture_tmu   leaf   1,921 ALM
    zhao_texture_cache leaf   1,087 ALM
    pair                        438 ALM     <- 85% GONE

**Nothing in the flow flagged it.** Status `ok`, provenance guard clean, 34
sources hashed, every field in the row internally consistent. What gave it away
was comparing against the LEAF AREA: TESS+NORMALS lost 27% when its seam went
internal, which virtual-pin removal explains — **85% does not.**

Cause: I tied `req_base_i`, `req_pal_base_i` and 28 of 32 mode bits to
CONSTANTS, so the TMU's format decode was unreachable and the tool folded most
of the block away. **The 37.63 MHz described a TMU that had been deleted.**

> **A characterization wrapper that over-constrains its stimulus does not measure
> the design. It measures what survives the folding.**

The fourteenth instance in this repository of an artifact being real while being
an artifact of something other than what it was read as — and **the first where I
built the artifact myself.** Every decoded field now comes from a 96-bit
registered stimulus. Discarded at `fc899a7`; the row must not be cited.

Worth noting for when the honest number lands: the LEAF TMU measures **36.11
MHz**, consistent with the docket's standing note that its 199.72 MHz was an
artifact of an SDC carrying no I/O constraints.

### 21:00 — FOUR PAIRS RANKED, and the reading I gave twice was wrong twice

| pair | Fmax | character |
| --- | ---: | --- |
| TESS+NORMALS | **31.10** | state machine + sequenced multiply walk |
| TMU+CACHE | 37.25 | format decode + cache handshake |
| FRAGMENT+TILESTORE | 55.52 | read-modify-write pipeline |
| **SETUP+BINNER** | **88.79** | almost pure arithmetic |

**A 2.9x spread.** I called the renderer "systemically slow" from the first two
points, which happened to agree. FRAGMENT+TILESTORE partly killed it; SETUP+BINNER
killed it outright. **I generalised from two samples, twice.**

**The hypothesis test came back clean.** SETUP is three edge functions and a
constant term with essentially no state machine, and it is the FASTEST by a wide
margin. FRAGMENT+TILESTORE is the TIGHTEST loop — a read-modify-write across its
own seam — and it is third. So the renderer is **not** limited by datapath logic
depth, and it is **not** limited by its seams. It is limited in the blocks that
carry **control state and iteration**.

That changes what "pipeline the worst seam" means: adding datapath stages
everywhere would have been right if depth were the limit. It is not. The work is
in the walks and the handshakes.

**The binner DSP fix survives composition:** the pair measures exactly 10 DSPs =
setup 4 + binner 6. The stored binner leaf row still reads 12 because it predates
today, which is one more reminder that a census of stale rows misleads.

### 21:10 — AND THE RANKING POINTS AT MY OWN WORK

Sequencing has been the DSP campaign's principal tool all week:

    field_seq       79 -> 3
    geom_skin       72 -> 9
    texture_tmu     28 -> 6
    surface_stamp   28 -> 0
    terrain_normals 18 -> 3    <- mine, this morning

Every one traded parallel hardware for a **walk with a state counter**, justified
by throughput headroom. **Not one was ever measured for frequency**, because no
renderer block had a valid Fmax to compare against until today.

And the ranking now says the slow pairs are exactly the ones carrying control
state, while the arithmetic-only pair runs at 88.79 MHz.

> **"I traded frequency for DSPs and never measured the frequency"** is the
> obvious hypothesis, it is mine, and it is load-bearing for twelve more blocks
> the same technique is queued against. Better to learn it from one block now
> than from twelve later.

Consistent is not proven. That is why I want the critical PATH, not an argument.

### 21:20 — the harness could not answer it, so the harness was fixed

`run_block_fit.ps1` read `blockfit.sta.rpt` for an Fmax and let it die with the
workspace — **the identical gap `run_shell_fit.ps1` had this morning**, in a
different file, found the same way: by needing the evidence and not having it.

> **A measurement you cannot interrogate is a number, not evidence.**

Fixed at `e1a3906`, then broken by my own hand and fixed again at `bb833ee`: the
Python heredoc that wrote the change ate a backslash and put a literal
**backspace** in the path, so Quartus refused a directory called
`...hesislockpaths`. Forward slashes now — no escaping layer can mangle them.

### 21:50 — THE CRITICAL PATH ARRIVED AND VOIDED MY OWN RANKING

Three fits and two harness fixes to get one path report, and it was worth every
one of them:

    From  altsyncram:lat_mem_rtl_0|...|ram_block1a0~PORT_B_WRITE_ENABLE_REG
    To    zhao_terrain_tess:u_tess|vy[0][3]
    Data Delay              30.000 ns
    Number of Logic Levels  10

**Three nanoseconds per logic level.** Cyclone V logic is 0.3-0.5 ns/level, and
the clock path spent **66% of its delay in interconnect**. That is not a deep
datapath — it is a **1,523-ALM design sprawling in a 41,910-ALM device** with
virtual pins on every port. Nothing pressures the fitter to pack it.

**The number characterises the PLACEMENT, not the logic.**

#### 1. My sequencing change is ACQUITTED

**Zero of the 25 worst paths touch `u_normals`.** I suspected the six-step walk I
built this morning (18 DSPs -> 3) had cost frequency, and said so publicly,
because the DSP campaign leans on that technique for a dozen more blocks. The
hypothesis was mine, it was reasonable, and it is **wrong**.

#### 2. The ranking cannot support a pipelining decision

I stated "the renderer is limited by control state, not depth or seams"
**twice, confidently**, on a measurement that could not carry it. What makes it
untrustworthy rather than merely imprecise: the fastest pair (1,405 ALM) is
nearly the same size as the slowest (1,523 ALM), so I cannot even argue the
inflation cancels.

> The audit asked for pair fits because leaf fits carry ~1,000 fictional virtual
> pins. That was RIGHT, and pairs did fix it — 1,045 pins to 67. **It fixed the
> boundary problem and left the SPRAWL problem.** I did not notice until I read a
> path instead of a summary.

Recorded as `QUARTUS_GOTCHAS` 12. **Unaffected:** the composed shell fit (7,442
ALMs of genuinely connected logic, real top, paths between named blocks), and
every DSP/memory figure, which are inference results rather than placement ones.

### 22:00 — RULING 4 IMPLEMENTED: `rescale_s32` takes `__int128`

The bug was real and the ruling was right to call it a bug rather than a choice.
`mat3x4_mul` builds genuine 128-bit products:

    __int128 p = a*b + a*b + a*b;                  // creature_core.cpp:77
    out.m[i*4+j] = rescale_s32(p, 16, L, ...);     // silently -> int64_t

The narrowing happened **at the call boundary**, so a value past INT64_MAX
wrapped *before* the round, the shift and the clamp could act on it — the
saturating law was bypassed exactly where it was needed, and **nothing recorded a
clamp, because no clamp occurred.** Silence was the bug's signature.

Directed cases either side of the int64 rails, plus the real 3-term row shape
`3*(2^31)^2`, plus the `k == 0` identity path which had its own narrowing through
`sat_s32_from_s64`. The clamp count is asserted **exactly** (5 rescale + 1 mul),
not as a lower bound — a test that tolerated extra clamps would not have caught a
bug whose symptom is *missing* ones.

**29,385,065 checks, 0 failures.** My first attempt asserted `>= 7` and failed;
the code was right and my arithmetic was not.

### 22:00 — GEOM.WCACHE built to the owner's brief, and its proof FAILS

Contract (15 TODOs -> 0), reference `zref::geom::VertexArena`, RTL
`zhao_vertex_arena`, differential — **all green**. The differential passes
sixteen directed lookups including the case that matters: reopening an arena
must not resurrect a payload still physically in the memory.

**The formal proof does not pass, and it is registered as failing rather than
omitted.** `cover` passes; `bmc` is refuted at k=4 on
`a_hit_is_the_watched_value` — the property the contract leads with.

**Four hypotheses, four experiments, four keepers, no diagnosis:**

| hypothesis | verdict | what it produced anyway |
| --- | --- | --- |
| read-during-write ordering | wrong | **read-old specified** — genuinely undefined before |
| no reset assumption | wrong, but needed | `rst_n` was FREE; moved k=2 -> k=4 |
| fills during reset | wrong, but a real gap | clock-only memory stored what no observer recorded |
| out-of-range watched key | wrong, but correct anyway | the claim is about REAL vertices |

**I got the bisection wrong first, twice, and both errors were method errors.**
I "disabled" an assertion with a `str.replace` and **did not assert the match**,
so nothing was replaced and I read the resulting failure as evidence about a
different assertion. Then I "confirmed" it from a VCD through a grep whose
pattern was mangled by a signal identifier containing a quote character — it
matched nothing, and I read the absence of change-lines as proof of no change.

> **A bisection is worth more than a trace read through an unverified filter.**
> Both errors have the same shape as the fourteen already recorded here: an
> artifact that is real while being an artifact of something other than what it
> was read as. Corrected in the ledger at `65f36ae`, in public, because the first
> commit had claimed the load-bearing property held.

`-Wall` also found two real design gaps: out-of-range indices were
**unrepresentable** (so the refusal lived in the contract, the oracle and the
tests and nowhere in silicon), and `org`/`seal`/`open` never range-checked their
arena — an out-of-range origin write would have **aliased onto a real arena's
datum**.

### 23:15 — RULING 5, and both numbers were right

`SURFACE.SHEET` cited **14.38 MiB** as its own pool twice. That figure is
**F+E+D+G+H, five layers** at 14,722 B/patch. The sheet is layer F alone at
8,192 B/patch = **8 MiB**; the other 6.38 MiB belongs to layers this block never
touches.

The ruling said the layer table wins *"unless another pool is identified
explicitly"* — and one is, so the fix was to stop conflating them rather than to
change a number. **Both figures were always correct; only the label on one was
wrong.** A contract claiming 14.38 MiB overstates its own footprint by 80%, the
same shape as the projector's "270 patches per frame" — a capacity filed as a
demand that made the most saturated block look like the most wasteful.

### 23:30 — RULING 2: the bake radius REFUSES, it does not clamp

`MAX_BAKE_RADIUS_RAW = 32'sh0200_0000` (512.0 m), with the ruling's arithmetic
kept beside it: at the largest legal pitch a patch is 128 m across and ~181 m
corner-to-corner, so the farthest swept vertex of a barely-intersecting patch is
under **~694 m** from the centre — inside signed-27-bit Q16.16. **That is what
makes the `dx`/`dz` re-domain legal**, and it is why the number is 512.

**A refusal is not a lawful no-op**, and the block now distinguishes them. B5
already said `radius <= 0` is accepted and sweeps writing nothing — a legal
request for nothing. An oversized radius is a request the machine **refuses**:
consumed so the producer cannot wedge, sweeping nothing, and **counted** on
`bake_radius_rejects_o`. Without the counter a rejection is indistinguishable
from a bake that merely did not happen.

---

## Decisions Made

**Hashing rather than copying.** Copying sources into the workspace would make
provenance immutable by construction and is the better fix. The tree has one
`` `include `` (`sdram_params.svh`), so a copy must preserve directory
structure — real surgery, and not something to land untested while a long fit is
queued. The limitation is **written into the gotcha rather than left implied**:
an edit made and undone entirely inside the elaboration window is still
invisible.

---

## Next Steps

- [ ] `ctest -L fast` + ledger, then commit
- [ ] **relaunch the `zhao_field_seq` fit at the committed commit** — still the
      first I/O-constrained Fmax for the Field engine; the 33.86 MHz predates the
      SDC I/O fix and may itself be optimistic
- [ ] consider clearing the 36 orphaned `%TEMP%` workspaces
