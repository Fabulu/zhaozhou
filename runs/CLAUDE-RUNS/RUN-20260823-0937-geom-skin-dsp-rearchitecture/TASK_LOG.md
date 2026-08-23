# TASK LOG — RUN-20260823-0937-geom-skin-dsp-rearchitecture

**Started:** 2026-08-23 09:37 UTC+02:00
**Branch:** main
**Base commit:** 7cd3d8c
**Spec:** SPEC_v1.md

> Everything in this log is **simulation, synthesis or fit**. No hardware has
> run any of it.

---

## 09:37 — run initialized

    C:\programmieren\zencrifice\runs\CLAUDE-RUNS\init-run.ps1 geom-skin-dsp-rearchitecture
    => Run ID: RUN-20260823-0937

`init-run.ps1` writes to `C:\programmieren\zencrifice\runs\CLAUDE-RUNS\`, which
is **outside the zhaozhou repository** (zencrifice is not a git repo). The
tracked copy lives at `zhaozhou/runs/CLAUDE-RUNS/`, which is where every
previous run's FINDINGS were committed, so the folder was mirrored there and
this log is the tracked one. `ARCHIVE.md` exists only in the zencrifice copy
and is updated at the end.

Machine state at start: `quartus_map.exe` (PID 24612) and `quartus_sta.exe`
(PID 2892) already running — the Field agent's constrained `zhao_field_seq`
fit. **No fit started from this run until the machine was free.**

## 09:50 — STEP 1, before any RTL: does the oracle resolve? (ledger rule V17)

    npm run ledger:check
    => ledger: V2 git-history — comparing against HEAD
       ledger: V16 formal registry — 23 recorded run(s)
       ledger: CHECK FAILED — 1 error(s) against 92 blocks / 40 ops
         - V16: FIELD.SEQ.CORE is RTL_VERIFIED but formal
           "tests/formal/field_seq_bound.sby" is recorded as "pending", not green

**V17 is green. The oracle resolves.** `GEOM.SKIN` cites
`zref::creature::skin_vertex`, defined at
`reference/src/zcreature/creature_core.cpp:222`, declared in
`reference/include/zref/zref_creature.hpp`, named in both the contract's
*Scalar reference function* section and the test file — which is the four-part
V17 check (definition exists, contract cite matches `reference_model`, cited
test paths exist, test text mentions the oracle).

The single V16 error is the Field agent's deliberately-pending formal status,
confirmed by the coordinator as not mine. **It is this run's ledger baseline: a
second error would be mine.**

## 09:55–10:10 — the measurement that starts the argument

`reports/synthesis/zhao_block_fit.json`, row `zhao_geom_skin`, sourceCommit
`16df9ee`, `rtlCleanAtHead: true`:

| | measured |
| --- | ---: |
| ALMs | 1,801 |
| **DSP blocks** | **72** |
| registers | 145 |
| virtual pins | 1,038 |
| seconds | 400.2 |

`reports/REMAINING_BLOCKERS.md:1223` had explicitly **not** queued this block
for sequencing, and gave a good reason: vertices are the highest-rate object in
the geometry pipeline, so unlike `geom_lod` (once per instance per frame) or
`terrain_lod` (once per patch per frame) the parallelism might be earned. It
said the answer needed a vertex budget nobody had stated.

**The budget is now stated: ~120,000 skinned vertex instances per 60 Hz frame.**

    gpu_clk                 100 MHz (10.000 ns; zhao_shell_fit.sdc:4, and the
                            per-block SDC run_block_fit.ps1 generates)
    clocks per frame        1,666,666
    demand                  120,000 vertices
    clocks per vertex        13.88
    products per vertex      18 two-weight (2 matrices x 3 rows x 3 terms), 9 rigid
    honest multipliers       18 / 13.88 = 1.30

**The one-clock form was over-provisioned by 13.9x.** The recurring cause named
in the brief — parallel multipliers where the block's rate does not require
them — is true here, by an order of magnitude. Checked rather than assumed, as
instructed, and `REMAINING_BLOCKERS.md` guessed the other way in good faith
without the number.

### The finding that changes what the blend identity is worth

**72 = 18 x 4.** A signed 32x32 in the old combinational cone cost four DSP
blocks; the six 7-bit weight multiplies cost approximately **none** — Quartus
had already put them in logic.

So the `(pb << 6) + w0*(pa - pb)` identity in the brief is exact, is used, and
saves **ALMs, not DSPs**. Recorded here because a report crediting the DSP
reduction to the identity would be crediting the wrong change.

## 10:10 — architecture: lanes by TERM, not by row

The brief proposed three ROW lanes. Evaluated and **rejected in favour of TERM
lanes**:

| | row lanes | term lanes |
| --- | --- | --- |
| coordinate operand | 3:1 mux per lane per cycle | **fixed wire** (lane 0 is always x) |
| matrix operand | 6:1 mux | 6:1 mux |
| a row-product completes | only after the last issue cycle | one cycle per row-product |
| blend can overlap issue | no | **yes**, saves 2 clocks |

With three term lanes a whole row-product is a one-cycle three-lane dot
product, so `pa[r]` and `pb[r]` become final three cycles apart — exactly the
cadence a one-row-per-cycle blend walk consumes them at. That is why the blend
is ONE shared shift-add unit and not three, at no cycle cost.

### The parameter and its legal settings

`MUL_LANES` = TL term lanes x RL row-product lanes:

| `MUL_LANES` | TL | RL | issue slots blend/rigid | latency blend/rigid | vertices/frame |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1 | 1 | 18 / 9 | 22 / 13 | 75,757 — **FAILS demand (63%)** |
| 3 | 3 | 1 | 6 / 3 | 10 / 7 | 166,666 — 1.39x |
| 6 | 3 | 2 | 3 / 2 | 8 / 7 | 208,333 — 1.74x |

`MUL_LANES = 2` is refused at elaboration: 2 divides neither the three terms
nor the three rows, so a cycle would straddle two row-products and every
accumulator would need a masked multi-source adder. The guard is an unresolved
module reference inside a generate-if — the portable static assertion.
`$error` was deliberately not used: Quartus 17.0.2's support for elaboration
system tasks is not something to discover twenty minutes into a fit.

**`MUL_LANES = 1` is kept because it fails.** A frontier with no failing end
does not show where the wall is.

## 10:15 — widths proven before synthesis (QUARTUS_GOTCHAS §5)

§5 cost `zhao_geom_lod` ten DSP blocks. The old skinner carried 67- and 75-bit
lanes on "slack is free" reasoning. Proven from the s32 input widths alone:

| quantity | bound | signed bits | was |
| --- | --- | ---: | ---: |
| `m*x` | <= 2^62 | 64 | 64 |
| `pa` | <= 3*2^62 + 2^47 = 1.3835e19 < 2^64 | **65** | 67 |
| `pa - pb` | <= 2.767e19 < 2^65 | **66** | — |
| `w0*(pa-pb)`, w0<=63 | <= 1.743e21 < 2^71 | **72** | — |
| `(pb<<6) + w0*(pa-pb)` | <= 2.629e21 < 2^72 | **73** | 75 |

These are adders, so the saving is ALMs. The multiplier operands stay a full
signed 32x32 — the audit's "a bone matrix is a bounded rotation" is true of the
content and false of the contract.

## 10:20 — RTL rewritten

`fpga/rtl/geometry/zhao_geom_skin.sv`. Lint, all three settings plus the
illegal one:

    verilator --lint-only -Wall --top-module zhao_geom_skin -GMUL_LANES=1 ... => clean
    verilator --lint-only -Wall --top-module zhao_geom_skin -GMUL_LANES=3 ... => clean
    verilator --lint-only -Wall --top-module zhao_geom_skin -GMUL_LANES=6 ... => clean
    verilator --lint-only -Wall --top-module zhao_geom_skin -GMUL_LANES=2 ...
      => %Error-MODMISSING: Cannot find file containing module:
         'ZHAO_GEOM_SKIN_MUL_LANES_MUST_BE_1_3_OR_6'

One lint fix on the way: `UNUSEDSIGNAL: 'idx'[31:5]` — an `int` index into a
24-entry array. Narrowed to `logic [4:0]`, which is also the honest width.

## 10:25 — the two things the contract said must be closed BEFORE the rewrite

`design/contracts/GEOM.SKIN.md` had recorded the weight identity as **not
done**, with two blockers. Both were real and both are closed.

**1. The identity is FALSE for `w0 > 64`** (223,020 mismatches in the
contract's own sampled sweep) and needed an `ENFORCED-BY:` naming who upholds
`w0 <= 64`. Closed with `require_legal_w0()` in the differential, which runs on
every vertex the test drives and fails the suite if any driver presents
`w0 > 64`. The RTL header names it. **The upstream HARDWARE obligation is
recorded as an owner-docket item on GEOM.VDECODE** (SPECIFIED, no RTL) rather
than papered over — and GEOM.SKIN deliberately does not add a defensive clamp,
because that would invent behaviour in the out-of-contract region and hide the
missing upstream check behind a plausible answer.

**2. The differential did not reach the operand extremes**, and the rewrite
made that worse rather than better: narrowing 67 -> 65 and 75 -> 73 turned an
untested argument into a load-bearing one. Closed with:

- **section 7, the row-product rails** — 9 rail values for matrix elements x 9
  for the vertex, with `B = -A` so `pa - pb` reaches twice `|pa|`, across
  w0 = 0, 1, 2, 32, 62, 63 plus rigid;
- **the near-cancellation family** — `B = -A` at `w0 == 32` makes the blend
  `32*(pa + pb)`, so the ANSWER is small while every intermediate sits at its
  bound. A lane one bit too narrow wraps and gives a large wrong answer here,
  where anywhere else it would give the same saturated rail as the correct one.
  This is the case that distinguishes a correct width from a lucky one;
- **section 7b** — 600 full-range random iterations, a SEPARATE lane rather
  than a widening of `--random`. The pose-range lane is deliberately aimed away
  from the rails and a recorded mutation property depends on that (the
  no-saturation mutation fails 6 directed checks and passes 900 random ones);
  widening it would have destroyed a known property to gain one the directed
  sections can carry.

## 10:30 — the rate is now a test, not a comment (section 8)

The DSP count is justified by a frame budget, so the frame budget is asserted:
accept-to-valid latency, the sustained issue interval of a never-stalling
stream, that the two agree, and that the resulting vertices/frame lands on the
**correct side** of 120,000 — above for `MUL_LANES` 3 and 6, and **below for
1**, which is checked as a failure so the frontier's failing end cannot quietly
start passing.

## 10:35 — the frontier is BUILT, not just parameterised

`tests/CMakeLists.txt` now elaborates `zhao_geom_skin.sv` three times:
`test_geom_skin_directed` (MUL_LANES=3), `test_geom_skin_lanes1`,
`test_geom_skin_lanes6`, each with `-GMUL_LANES=<n>` and a matching
`ZHAO_SKIN_LANES` compile definition carrying the frontier table's latencies.

**This is not ceremony.** `MUL_LANES = 3` collapses the term walk (TSTEPS == 1),
so the default build **never executes the multi-cycle accumulate path at all**.
Only the `MUL_LANES = 1` build does. The two extra targets are written out
rather than generated in a `foreach`, because the mutation sweep's guard 7
derives its consumer set by grepping `verilate(<target> ... <file>)` out of
`tests/CMakeLists.txt` and a `${lanes}` in the target name would leave it
deriving a literal that matches no target.

## 10:40 — coordinator constraints received and honoured

1. `build/` was wiped and is being reconfigured clean; no `cmake`/`ninja`
   against it until cleared. Held.
2. Build via presets, env dot-sourced in the SAME invocation:
   `. .\tools\env\zhao-env.ps1; cmake --preset windows-native; cmake --build build`.
3. Ledger baseline is the one V16 error. Re-checked after the `blocks.yml` and
   contract edits: still exactly one error, still V16. **No error introduced.**

## 10:45 — documents updated

- `design/contracts/GEOM.SKIN.md`: latency (was `fixed:1`), target throughput
  (was "one vertex per clock", now the derived frame budget), backpressure (the
  `!busy` term and the palette latch), clock/reset (the engine state reset
  adds), Q formats (the proven width table), the weight-identity section
  rewritten from *not done* to done with both blockers checked off, the tests
  sections, and the synthesis section with the before numbers.
- `design/blocks.yml`: `latency: variable`, `target_throughput` restated from
  the frame demand.

---

## Open at this point in the run

- **The build gate has not run.** Waiting on the coordinator's clean rebuild.
- **No fit has been run.** `quartus_fit.exe` PID 30196 was still live at 10:45.
- The sweep (`tools/sweep_geom_skin.sh`, 28 mutants, preflight lints every
  mutant at all three MUL_LANES settings) is written but not run: its preflight
  temporarily writes mutated text into the RTL, which must not happen while
  another agent's `cmake` configure is elaborating that file.

---

## 11:05 — first run of the new differential, and it FAILED. Well, class.

Built in an **isolated build directory** (`build-skin/`, gitignored via
`build-*/`) rather than in `build/`, which the coordinator had wiped and was
reconfiguring. Same preset toolchain, pinned explicitly:

    . .\tools\env\zhao-env.ps1
    cmake -S . -B build-skin -G Ninja -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_CXX_COMPILER=C:/programmieren/dsstuff/mingw64/bin/g++.exe `
      -DCMAKE_MAKE_PROGRAM=C:/programmieren/dsstuff/mingw64/bin/ninja.exe
    ninja -C build-skin test_geom_skin_directed test_geom_skin_lanes1 test_geom_skin_lanes6

Result: **974 of 5,774 checks failed**, identically at all three lane counts,
and **every failure was in the new sections 7 and 7b**. Sections 1-6 -- the
entire pre-existing differential -- passed. The signature was a sign flip at
the saturation rail:

    FAIL: fullrange[599] w0=0: x: expected 0x80000000, got 0x7FFFFFFF

Identical failures at MUL_LANES 1, 3 and 6 means arithmetic, not scheduling.

### The cause is in the ORACLE, and it predates this run

    reference/include/zref/zref_fixp.hpp:106
      constexpr int32_t rescale_s32(int64_t x, int k, SatLedger* L, ...)

    reference/src/zcreature/creature_core.cpp:255
      *o[i] = rescale_s32(v.w0 * pa + w1 * pb, 22, L, &SatLedger::mul);

`pa` and `pb` are `__int128`. **The argument is silently narrowed to int64.**
Reproduced exactly for the first failing case:

    pb   = 1660515586393437354          (w0 = 0, so blend = 64*pb)
    blend = 1.0627e20                   -- 67 bits, does NOT fit int64
    exact rescale(blend, 22) -> saturates +  0x7FFFFFFF   <- what the RTL gives
    rescale(int64(blend), 22) -> wraps  -  0x80000000     <- what the oracle gives

The narrowing is **not intended**. `rescale_s32`'s own comment says *"The
rounding add runs in s128: x near INT64_MAX must not wrap before the shift"* --
it was written for s64 inputs -- and a `rescale_s64(__int128 x, ...)` exists
twenty lines below it.

**This is not a regression I introduced.** The old RTL carried 67- and 75-bit
lanes and did not truncate either, so it diverged from the oracle in exactly
the same places. Nothing had ever caught it because the differential had never
driven an operand that large -- **which is the precise gap
`design/contracts/GEOM.SKIN.md` recorded as blocking this rewrite.** The
contract said the extremes had to be reached before the rewrite, not after. It
was right, and this is what was behind the door.

### What was done about it, and what was deliberately NOT

- **The reference was not changed.** `skin_vertex` is the function every shipped
  picture was skinned with; changing its arithmetic changes those pictures in
  the extreme region. That is an owner decision. **OWNER DOCKET ITEM.**
- **The RTL was not taught to imitate the narrowing.** Baking a C++ implicit
  conversion into silicon needs a ruling, not a commit.
- The differential now **checks the shipped oracle wherever the oracle is well
  defined** and, above the narrowing boundary, checks the exact arithmetic the
  RTL claims -- saying so in the check name
  (`[beyond the oracle's int64 narrowing]`).
- `skin_exact()` is `skin_vertex` with the implicit narrowing removed. It is
  **not** a second oracle written beside the RTL: every in-domain coordinate
  asserts that it equals the shipped oracle bit for bit, so it earns the
  out-of-domain cases by tracking the real one first.
- The split is **counted and printed**, because a silent domain split is a way
  to lose a differential without noticing.

### How much of the differential is still against the shipped oracle

    [geom_skin] oracle-checked coordinates: 3976, beyond the oracle's int64 narrowing: 1778
    [geom_skin_directed] 9750 checks passed          (MUL_LANES 3)
    [geom_skin_directed] 9756 checks passed          (MUL_LANES 1)
    [geom_skin_directed] 9750 checks passed          (MUL_LANES 6)

    --random 500  : oracle-checked 1500, beyond 0   -- 3,000 checks, all three builds
    --random 8000 : oracle-checked 24000, beyond 0  -- 48,000 checks

**The pose-range random lane never leaves the oracle's domain: 0 of 24,000.**
That is the reassuring half of the finding -- the divergence is unreachable
with anything resembling a real bone matrix, which is why it survived this long
and why it is a docket item rather than an emergency.

The 6-check difference between MUL_LANES 1 and 3/6 is section 6's busy-cycle
law: it asserts `v_ready_o == 0` on every cycle between accept and result, and
the rigid latency is 13 clocks at 1 against 7 at 3 and 6. The count moving with
the schedule is the check working.

### And the most sensitive width case is still oracle-backed

The near-cancellation family (`B = -A`, `w0 == 32`) makes the blend
`32*(pa + pb)`, so the **final value is small and IN the oracle's domain while
every intermediate sits at its bound**. The 65-bit accumulator and 66-bit
difference lane are therefore exercised at their limits against the SHIPPED
oracle, not against the local model. Only the region where the finished blend
itself exceeds 2^63 relies on `skin_exact`.

### Two build-state traps hit and avoided, recorded per the standing rule

1. A `printf` format string was split across two source lines without a closing
   quote. The build FAILED -- and the same PowerShell command then ran the three
   **previous** binaries and printed their old failures, which read exactly like
   a fix that had not worked. Caught only because the compiler errors were in
   the same output. This is the project's recurring failure mode verbatim: the
   executable outlives the build that failed to replace it.
2. `build-skin/` was used precisely so that a second build could not collide
   with the coordinator's clean reconfigure of `build/`. The official gate still
   has to run in `build/`.

---

## 11:40 — the owner's worktree ruling, and the three things it uncovered

`docs/OWNER_DOCKET.md` (RULED 2026-08-23) carries a standing process ruling I
had not complied with:

> **Mutation sweeps must run in separate git worktrees with separate build
> directories.** The terrain sweep contaminated other targets with
> mutant-generated Verilator sources and made clean RTL look broken — sharing
> one build tree between agents "is no longer defensible".

I had a separate build directory but not a separate worktree, so the sweep was
mutating `fpga/rtl/geometry/zhao_geom_skin.sv` **in the tree every other agent
reads**. I stopped the sweep, restored, committed the tooling, and made a
worktree. Three defects fell out of doing it properly, and none of them would
ever have surfaced in the tree the sweeps were authored in.

### (a) Every mutation sweep in this repository is broken in a fresh checkout

`.gitattributes` did not pin `*.sh`, so a worktree checkout gave
`tools/sweep_*.sh` **CRLF** endings. The failure was silent, which is the part
that matters:

    linted 0 mutants at MUL_LANES (1, 3, 6), 0 do not build     <- exit 0

The preflight parses the mutant table with `^"(.*?)"$` in MULTILINE mode. Under
CRLF the character before each newline is `\r`, not the closing quote, so it
matched **nothing** and reported a clean pass over an empty set. Bash reading
`#!/usr/bin/env bash\r` and CRLF array entries would then have carried `\r` into
every anchor.

**This affected `sweep_geom_cull.sh`, `sweep_geom_lod.sh`, `sweep_field_dsp.sh`
and `sweep_terrain_lod.sh` too** — verified: `sweep_geom_cull.sh` checks out
CRLF as well. They had only ever been run in the tree they were authored in,
where the working copy was already LF, so no checkout of them had ever been
exercised. Fixed repo-wide with `*.sh text eol=lf`.

And the guard that should have caught it: **a preflight that lints nothing must
FAIL.** It now aborts on an empty mutant table.

### (b) A `TaskStop`ped sweep KEPT RUNNING and rewrote the RTL under two fits

This is the project's recurring failure mode, and I walked into it.

Two Quartus fits failed at `quartus_map` with a syntax error at line 223. I
fixed the cause (see (c)), verified the fix with Verilator at all three lane
settings, launched a third fit — and it failed **at the same line 223**. The
file no longer contained my fix.

    git diff fpga/rtl/geometry/zhao_geom_skin.sv
    -          vertices_transformed_o <= vertices_transformed_o + 32'd1;
    +          vertices_transformed_o <= vertices_transformed_o + 32'd2;

**Mutant M22 was applied to the RTL.** Two `sweep_geom_skin.sh` processes I had
stopped were still alive:

    Get-CimInstance Win32_Process | ? { $_.CommandLine -match "sweep_geom_skin" }
      21744  bash.exe  ... tools/sweep_geom_skin.sh
      28312  bash.exe  ... tools/sweep_geom_skin.sh

Stopping the task killed the shell that launched them, not the sweep itself. Each
zombie kept marching through its mutant table, and `restore()` copies a GOLD
snapshot taken at **sweep start** — which is what silently reverted my edit and
fed a mutant to a Quartus run.

Two fits and one code change were destroyed by a process I believed was dead.
**Killed by PID and verified gone**, then `git checkout --` and a hash check
before restarting anything.

The lesson generalises past this incident: *stopping a background task is not
the same as the work stopping*, and the symptom was indistinguishable from "my
fix did not work".

### (c) QUARTUS_GOTCHAS §8 — a module-scope `if` generate needs `generate`

    if (!(MUL_LANES == 1 || MUL_LANES == 3 || MUL_LANES == 6)) begin : g_illegal
      ZHAO_GEOM_SKIN_MUL_LANES_MUST_BE_1_3_OR_6 u_static_assert ();
    end

    Error (10170): Verilog HDL syntax error at zhao_geom_skin.sv(223) near
                   text: "if";  expecting "endmodule"
    Error (10112): Ignored design unit "zhao_geom_skin" due to previous errors

Verilator, slang and the LRM all accept it. Quartus 17.0.2 does not. The block
linted clean under `-Wall` at MUL_LANES 1, 3 and 6 and then died in analysis and
synthesis at 44 s.

This is §1 with a different keyword — both are generate-region syntax that three
frontends accept — and the irony is sharp: `$error` was avoided *because* this
tool's support for elaboration system tasks was unknown, and the `if` was the
part it could not parse. Recorded as §8, and the file's closing tally corrected
from "six of the seven" to "seven of the eight".
