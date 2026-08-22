# Task log

Engineering-side record: what was run, against which commit, and what the
evidence actually was. `STATUS.md` is the owner-facing channel and stays prose;
this file is the audit trail and stays exact.

Newest entry at the top. Every claim here names the command that produced it.
**Everything in this file is simulation, synthesis or fit. No hardware has run
any of it.**

---

## 2026-08-22 (later) -- the GPU/video CDC seam, fixed structurally rather
## than re-timed; and a sweep that caught my own test not compiling

The ruled next move (`docs/OWNER_DOCKET.md`, "RULED 2026-08-22 -- BALANCED stays
authoritative"): *"Fix the video/GPU seam structurally -- especially move
displayed CRC into `vid_clk` rather than crossing per-pixel state."* Done.
**Everything below is SIMULATION. No fit was run for this change** -- the
composed fit is ~40 minutes and belongs to the owner's A/B.

### 1. What the seam actually was

`zhao_shell_top` glue 7 ran `zhao_debug_crc` on `gpu_clk` and re-timed the video
pixel register into it:

```
  always_ff @(posedge gpu_clk ...)
    ...
    else if ((vph_q != vphase) && px_out.valid) begin
      crc_in_byte <= px_out.rgb565[7:0];
      by_hi       <= px_out.rgb565[15:8];
```

`px_out` is a `vid_clk` register. So sixteen data bits, plus `x`, `y` and
`valid`, were sampled by GPU flops **on every displayed pixel** -- 122,880 of
them per Duo frame -- fenced by nothing but a phase toggle. It was correct only
because the simulation freezes `vid_clk = gpu_clk/2` with coincident posedges
(plan R1), which is a property of the testbench and not of a board. That is the
family both HIGH PERFORMANCE hold violations landed on.

**And the documents disagreed about it.** `design/contracts/DEBUG.CRC.md` said
"`vid_clk` domain for the displayed-stream lane"; `design/blocks.yml` said
`clock_domain: gpu`; the RTL followed the ledger and built the crossing. The
owner ruled the contract's reading, so `blocks.yml` is now `clock_domain: video`
and all three agree. The ledger's V8 then correctly demanded the block declare
itself a documented async bridge (`async_bridge: true`), which it now does --
the block genuinely sits on the boundary.

### 2. Step 1 of the method: does the oracle resolve?

Checked before writing any RTL, against `reports/PHANTOM_REFERENCES.md`'s
three-kind taxonomy.

* `zref::Crc32c` -- **resolves**, `reference/include/zref/zref_cmd2.hpp:663`. A
  real device oracle (sof/eof framing, publish-only-on-exact-size, `size_err`),
  delegating the arithmetic to the generated `zhao_abi::zhao_crc32c`. Not a
  phantom of any kind.
* `zref::render::displayed_crc32c` -- **resolves**,
  `reference/src/zrender/resolve.cpp:97`; the whole-shell oracle
  `shell_golden` and `duo_markers` already compare against, including the Duo
  two-cursor border law. Not a phantom either.

**Neither was modified**, which is what makes the test below a differential
rather than a restatement.

### 3. What changed

* `fpga/rtl/debug/zhao_debug_crc.sv` -- a `vid_clk` block with a PIXEL port.
  One RGB565 pixel per clock is two stream bytes, which a byte-serial CRC
  cannot keep up with, so the two bytes fold in ONE `zhao_crc32c_fold` tree
  (about seven XOR levels) instead of two chained eight-level steps. Same
  polynomial machine: the fold derives its columns from the CRC-32C definition
  at elaboration and is held to the shipped `zhao_crc32c_step` by
  `tests/differential/crc32c_fold_directed.cpp`.
* `bytes_captured_o` now means "the length the last event reported", latched at
  the eof pixel. It previously read the running counter, which the finalize
  edge cleared on the same cycle -- so at the publish pulse it was always zero.
  The old value could not have been wrong because it carried no information.
* `fpga/rtl/common/zhao_shell_top.sv` glue 7 -- the serializer is gone. The CRC
  sees `px_out` natively; what crosses to `gpu_clk` is the finalized 32-bit
  value, once per frame, on a toggle with the data held stable beside it. Same
  shape `zhao_video_framectl` uses for `gpu_tick`, which
  `tests/formal/video_framectl_one_fence.sby:a_cdc_data_stable_unless_toggle`
  already proves; three synchronizer flops and an edge detect on the gpu side.

The displayed BYTE STREAM is unchanged byte-for-byte, which is why the golden
captures still match: the old path emitted low byte then high byte on the two
gpu cycles of each vid cycle, and the new path folds the same two bytes in the
same order in one.

### 4. The differential, and why it is cross-granularity

`tests/debug/debug_crc_directed.cpp` drives the device with PIXELS and the
SHIPPED `zref::Crc32c` with the SAME STREAM AS BYTES. A byte-order slip inside
the pixel lane cannot pass a comparison against a model folding the two bytes
in the other order -- which is the one law the port change could have inverted
silently, so it also gets a directed case of its own.

The nine-byte `"123456789"` vector left this file: an odd byte count is not a
displayable stream. It still guards the polynomial in `tests/unit/test_crc.cpp`
and `tests/fuzz/test_abi_fuzz_parity.cpp`.

Three laws that had no test before and do now, because the sweep would
otherwise have shown them as holes: `expect_bytes_i` is LATCHED at sof; an ODD
expectation can never be satisfied by a pixel-granular stream; and a sof
arriving inside an open frame RESTARTS it (unreachable in a lawful raster,
which is exactly why a device ignoring it would have been indistinguishable).

### 5. THE SWEEP CAUGHT MY OWN TEST NOT COMPILING, and that is the result
### worth recording

First run of `tools/sweep_debug_crc.sh`: **22 attempted, 22 accounted, 0
caught, 22 survivors.** Every mutation of a CRC surviving a bit-exact CRC
differential is not a test hole, it is an impossible number, so I went looking
for the lie instead of writing it up.

`tools/sweep_geom_lod.sh`'s header names four ways this build system scores a
run that never happened. All four guards PASSED here: the source moved, the
model re-elaborated, and its hash differed from pristine. **There is a fifth.**

> The executable lives OUTSIDE the target directory. `rm -rf
> build/tests/CMakeFiles/<target>.dir` removes the model and every object, so
> ninja must rebuild them -- but `build/tests/<target>.exe` survives that
> deletion. If the rebuild fails for any reason, the previous executable is
> still sitting there and runs happily against RTL it was never built from.

And what had failed to build was **my own test file**: `0x5EC0ND50u` is not a
hex literal. It had been committed, because `ctest` does not build and a stale
executable had been passing every run since I added the restart case. A test
that cannot compile is not a test, and the directed lane had been reporting 54
checks when it should have reported 57.

The guard added: delete the EXE too, and require it to EXIST after the rebuild;
a missing executable is DISCARDED, never scored. The pristine baseline now also
has to link, not just elaborate.

### 6. The sweep, re-run with the guard

**22 attempted, 22 accounted, 20 caught, 2 survivors.** Both survivors are the
two mutants named EQUIVALENT in the script before it ran, each with a proof in
the file's footer that no input can distinguish them:

* **M21, the reset value of `crc_r`.** `crc_r` is read in exactly one place,
  `fold_c = in_sof_i ? SEED : crc_r`; the `crc_r` arm needs `in_sof_i` low, and
  `fold_o` is consumed only inside `if (in_sof_i || running)`, so reaching it
  needs `running = 1`. `running` is set only by that branch, on a cycle that
  also writes `crc_r <= fold_o`. Every read sees a value the branch wrote.
* **M22, clearing `n_bytes` at eof.** `n_bytes` is read only via `n_next`, and
  `n_next` ignores it whenever `in_sof_i` is high. The eof clear runs on the
  same cycle that clears `running`, so the value it writes can only be read
  after a sof has re-opened the frame -- and a sof ignores `n_bytes`. Dead
  code; keeping it is documentation.

Both are still DRIVEN each run, so the proofs are re-checked rather than
trusted. If a future change makes either reachable they stop surviving.

### 7. Measured (all simulation)

| lane | result |
| --- | --- |
| `debug_crc_directed` | 57 checks |
| `debug_crc_random` (300 frames) | 2,100 checks |
| `debug_crc_random_nightly` (3,000 frames) | green |
| `lint_debug_crc`, `lint_shell_top` | green |
| `shell_golden_replay` | green |
| `shell_duo_markers_fast` | green |
| mutation sweep | 22 / 22 / 20 caught / 2 equivalent |

### 8. Two things left for the owner, and one for the main session

Recorded in `docs/OWNER_DOCKET.md` rather than decided here: whether to cut
`gpu_clk` and `vid_clk` in the SDC now that the per-pixel crossing is gone (the
SDC deliberately keeps them timed, and cutting them would improve the A/B by
telling the tool to stop looking), and the 64-bit `starvation_o` sample, which
is the last `vid_clk -> gpu_clk` family that is not a toggle handoff -- guarded
by a quiescence tripwire, which is a protocol argument rather than a structural
one.

**And a live gate failure that is not mine:** `npm run ledger:check` currently
fails V20 on `fpga/rtl/geometry/zhao_geom_lod.sv:254` -- "mutually exclusive by
construction" with no `ENFORCED-BY:` within ten lines. That claim arrived in
`54ec158`. Verified attributable by restoring the file's pre-`54ec158` text and
re-running the check, which then reported only my own V8 (since fixed). I left
it alone because only its author knows which enforcer it meant to name.

---

## 2026-08-22 (late) -- the fit measurement, four owner rulings, and a block
## whose test caught my own bug

### 1. The composed fit at `6d23c84`, which is the measurement I owed

Run `wumen-6d23c84-20260822T164408Z`, compared against the immediately prior run
`de2794d` rather than against any doc summary:

| | `de2794d` | `6d23c84` |
| --- | ---: | ---: |
| setup worst | -0.729 ns | **-0.475 ns** |
| failing endpoints | 97 | **56** |
| ALMs | 7,667 | **7,415** |
| hold | 0 failing | 0 failing, +0.253 ns |

35% better on worst slack, 42% fewer failing endpoints, 252 ALMs smaller. The
caveat I carried into it -- that the design is placement-bound below ~1.5 ns and
this might not move the headline -- did not hold, because the change removed
five real borrow chains rather than rearranging logic.

**The per-path census is NOT available for this run.** `run_composed_fit.ps1`
deletes its workspace unless `-KeepWorkspace` is passed, so `setup_paths.rpt` is
gone and only the headline JSONs persist. Re-running with the flag costs another
~40 minutes and is deferred, because the owner has ruled the CDC seam comes
first and it will move the paths anyway.

### 2. Four rulings from Fabian, and what each changed

**(a) One engine, five profiles.** The five `FIELD.SEQ.*` entries were
`kind: rtl`, which under V4 demanded five reference models and ten test files,
and under V5 booked **five engines worth of ALM budget for one engine**. Nothing
in the RTL ever distinguished them: `zhao_field_seq` has no profile input and no
profile-specific port, and what would distinguish a profile is carried by the
decoded program.

Implemented as a new ledger kind rather than a comment: `kind: profile` plus
`implemented_by`, JSON schema updated, and **rule V21** added -- a profile must
name an `rtl` block, may not out-claim its engine maturity, and may not book an
ALM budget. Six unit tests, ledger suite 46/46 green.

The V4 and V5 exemptions are the whole risk, so V21 exists to charge for them.
Without it a profile is just an RTL block with its obligations switched off,
which is the "make the rule quiet by rewriting its input" shape this project
already treats as a defect.

**(b) "Camera visibility sectors" is deleted.** The phrase appeared exactly
twice in the repository -- in `GEOM.MESHFETCH`'s purpose line and in the contract
generated from it. Nothing defined it. Ruled: conservative per-camera frustum
rejection of a BOUNDING SPHERE before vertex decode, rejecting only when outside
every active camera, with an optional two-bit per-camera visibility result
carried downstream. Static meshes take an asset-generated bound; animated
creatures take a conservative animation-safe instance bound, with per-pose exact
bounds explicitly not required. `GEOM.CLIP` stays the exact per-triangle stage.

Fabian's reasoning for why CLIP cannot substitute is recorded in the docket: it
receives already-projected triangles, far too late to save the decode, pose,
skin, project, setup and bin work that rejecting at MESHFETCH avoids.

I also corrected my own framing. I had claimed the existing projection code
"defines it completely". It does not -- projection defines the frustum, but the
coarse bound representation was still open, and the bounding-sphere ruling is
what closes it.

**(c) The LOD boundary overflow: fix the law.** See section 5.

**(d) BALANCED stays authoritative; CDC seam first.** Recorded, and I have
stopped tuning setup paths. The explicit instruction: moving the CDC logic
changes placement enough that today's 56 endpoints may not be tomorrow's 56.

### 3. GEOM.MESHFETCH, classified before anything was built

`zref::MeshFetch` is one of the 25 phantom references. Classifying it against
the three kinds gave three different answers for one block:

| job | kind | state |
| --- | --- | --- |
| decide LOD per governor targets | 1 -- shipped under another name | built today |
| fetch meshlet descriptors | 2 -- no implementation | needs a format |
| cull against "visibility sectors" | 2, and the term was undefined | now ruled |

The LOD third was kind 1 and cheap: `zref::creature::lod_raw` / `lod_update`
already exist, are what the reference simulation itself calls, and are pinned to
charter 9 and 10. Recorded as an addendum in `reports/PHANTOM_REFERENCES.md`.

### 4. `zhao_geom_lod.sv` -- and there is no divider in it

Written out directly, the law divides twice per evaluation: a rung error
`rhu(proj*e_r/R)` and a boundary `rhu(thresh*R/e_r)`. That is two 64/32 variable
dividers on a per-instance path.

Neither is needed. Every use of a quotient here is a COMPARISON against an
integer, and for integers N >= 0, e > 0:

    floor(N/e) <= T   <=>   N < (T+1)*e
    floor(N/e) >= T   <=>   N >= T*e

So rung legality becomes `proj*e_r + R/2 < (thresh+1)*R` with no division at
all, and the two hysteresis tests reduce to division by the CONSTANTS 9 and 11 --
a multiply and a shift. The identities need a non-negative numerator, which is
exactly the block's domain (a projected radius, three error magnitudes, a bound
radius from `isqrt_u64`), asserted under FORMAL rather than assumed.

**Deliberately NOT exploited:** `compile_creature` sets `splat_error =
bound_radius/2` and `glint_error = bound_radius`, which would collapse two of
the three products. That is a property of the software that compiles creature
types; nothing in hardware enforces it, so a block assuming it would be wrong
for any type built another way. Recorded in the RTL so the opportunity is not
lost and nobody applies it later without a guard that fires.

### 5. The overflow in the shipped reference, found by the random lane

At `R = 59353, thresh = 40818, e = 1, proj = 339695` the RTL and the reference
disagreed. The REFERENCE was wrong: `boundary_q8` computed `thresh * R` in
`__int128` and then narrowed the quotient to `int32_t`. 2,422,670,754 wraps to
**-1,872,296,542**, and a negative boundary makes the eager-coarsen test false
for every projected radius -- **the ladder refuses to coarsen, and a creature
walking into the distance stays pinned at a fine rung forever.** Reachable with
a small `micro_error` and a large threshold, both ordinary.

Fabian's amendment was the half I had missed: widening `boundary_q8` to
`int64_t` is NOT a fix, because the quotient can approach 2^62 and the next line
multiplies it by 9 or 11. So the boundary is now **never formed at all** -- the
reference cross-multiplies in `__int128` using the same identity the RTL uses,
which also means reference and hardware now evaluate one mathematical predicate
instead of two routes to it.

Clamping to `INT32_MAX` was considered and rejected on Fabian's reasoning: it
moves the 90% hysteresis threshold downward, so there are representable radii
where `proj <= 0.9 * true_boundary` holds but `proj <= 0.9 * INT32_MAX` does
not. It fixes the catastrophic wrap while quietly changing the transition law.

`creature_core`'s own anchors stayed green through the change, so nothing moved
in the well-defined domain.

### 6. The bug in MY OWN RTL, and what caught it

The three rung error terms were an unpacked array indexed by a genvar inside a
generate loop. **All three legality bits came out equal**, so the block could
only ever return the coarsest rung or the finest -- never `kMicro` or `kSplat`.

It agreed with the oracle on **27,618 of 29,459 checks** anyway, because most
cases legitimately land on rung 3 or rung 0. What exposed it was the
deliberately absurd corner sweep (ref=2, dut=0), and a magnitude bisect then
showed it had nothing to do with magnitude at all -- it reproduced at
`R = 65,536` just as well as at `INT32_MAX`.

There are exactly three rungs and there always will be; the ladder is fixed by
charter 9, not parameterised. The loop bought nothing and hid the one class of
bug a loop can hide. It is three named terms now, which cannot silently share a
value, and two sweep mutants (M21, M22) exist to keep it dead.

### 7. Mutation sweep: 23 attempted, 23 accounted, 22 caught, 1 equivalent

The three survivors of the first scored run were each dealt with on their
merits, rather than all being filed as equivalent:

* **M08, `<` widened to `<=` on the refine test -- REAL HOLE, now closed.** The
  identity is `floor(N/e) <= M  <=>  N < (M+1)*e`, so the two spellings differ
  at exactly one point, `N == (M+1)*e`, and random draws cannot land on a point.
  It is constructed now: with `R = 1, e = 2k, thresh = k, proj = 1` we get
  `N = 2k` and `(M+1)*e = 2k` exactly, swept for k = 1..8.
* **M15, hold saturation deleted -- REAL HOLE, now closed.** It differs only at
  `hold == 0xFFFF`, and only on a path that INCREMENTS the hold. Section 6 drove
  the rail, but on inputs that switch rungs -- and a switch clears the hold to
  zero, so the increment never ran at the rail at all. Now driven with
  `rung_i == raw`, and on a refused switch.
* **M18, `e == 0` refining always refused -- EQUIVALENT, with a proof.** If
  `e[rung_i] == 0` then that rung is always legal, because legality is
  `proj*0 + R/2 < (thresh+1)*R` and `R/2 < R <= (thresh+1)*R` for every
  `R > 0, thresh >= 0`. `raw` is the COARSEST legal rung, so `raw >= rung_i`,
  so the refining branch (`raw < rung_i`) is unreachable. Not a hole; recorded
  so it does not read as one.

### 8. FOUR distinct ways the sweep harness scored runs that never happened

This took longer than the RTL did, and each one is a variant of something
already in this project's history:

1. **`verilate()` elaborates at CONFIGURE time**, so `ninja` alone relinks a
   cached model against changed source. First run: all 20 mutants discarded.
   That was the guard working, not a nuisance.
2. **Future mtimes.** An early version stamped the mutated source forward to
   force a rebuild, which made a model elaborated from a MUTANT look newer than
   the pristine source restored after it, so the next elaboration was skipped.
   `os.utime(p, None)` now -- never forward.
3. **Model hash is necessary but not sufficient.** A pristine model can be
   linked against an OBJECT still compiled from a mutant -- observed as 994
   failures against provably clean RTL, after I deleted `.ninja_deps` to clear a
   file lock and cost ninja its dependency information. The whole target
   directory is deleted each iteration now.
4. **Hashing the wrong file.** `Vzhao_geom_lod.cpp` is Verilator's WRAPPER and
   is byte-identical between pristine and mutant; the logic lives in
   `Vzhao_geom_lod___024root__0.cpp`. This one reported DISCARDED for work that
   was actually fine -- the opposite failure from the other three, and the only
   one that hides a real result rather than inventing one.

The harness now also checks that the PRISTINE build passes its own test before
any mutant runs, because otherwise every "caught" below it means nothing. That
check fired once for real, after the `.ninja_deps` incident.

### 9. A process note on the diagnosis

The magnitude bisect that found the generate-loop bug took one 40-second
rebuild. The two hours before it went into a standalone Verilator probe that
never linked (libstdc++ ABI mismatch) and into three theories about signed width
casts, none of which were right.

**The tool said "dut returns only 0 or 3, never 1 or 2" the moment I asked it a
question shaped like data instead of a hypothesis.** Same lesson as
`Total block memory bits: 0` and `rebuilding 'build.ninja'`.

---

## 2026-08-22 -- A subtractor that was a NOT, and the law underneath it that
## no test could reach

### What I was actually looking for

The largest remaining timing family was `INPUT.SNAPSHOT`'s `seq -> gaps`, 38
paths at -0.351 ns. Reading the block rather than the report, the saturation
test was:

```systemverilog
if (((64'hFFFF_FFFF_FFFF_FFFF - gaps) >= {61'd0, gap_sum})) begin
```

**All-ones minus x is exactly `~x`.** For unsigned N-bit x, (2^N - 1) - x == ~x
with no borrow, because every bit of the minuend is 1 and no column can borrow
from its neighbour. Identity, not approximation. So a full 64-bit borrow chain
sat in front of the compare doing the work of a free bitwise inversion.

Grepping for the pattern found it FIVE times across four blocks: `CMD.DMA`'s
`sat_add`, this one, two arms of `HPS.BRIDGE` and one of `VRAM.ARBITER`. Same
law, five hand-written copies, and they did not even agree on the spelling --
three tested overflow (`a > MAX - b`), one tested headroom (`MAX - b >= a`).

### The part that mattered more than the timing

Before claiming the rewrite was safe I checked whether anything would notice if
it were wrong. Mutating `~b` back to plain `b` -- which removes the headroom
test altogether and breaks saturation completely:

```
[cmd_dma_directed] 48 checks passed
cmd_dma random: 5000 packets done[cmd_dma_random] 19015 checks passed
```

**The mutant passes the entire simulation suite.** And no amount of extra
stimulus fixes that: these are u64 counters incremented by small amounts, so the
rail is on the order of 2^64 events away and nothing external can preload them.
The saturating arm is UNREACHABLE IN SIMULATION. Five copies of a law that no
test can check is how a design acquires a defect nobody can find.

That is a test gap the sweep exposed, so it got closed rather than noted.

### What was done

1. **One definition.** `zhao_pkg::zhao_sat_add64` / `zhao_sat_add32`. All five
   sites now call it. `INPUT.SNAPSHOT` also dropped its outer
   `gaps != all-ones` test -- a second full-width compare guarding a case the
   saturating add already handles, which is only sound because the rail is
   absorbing, which is now proven.
2. **A proof instead of a test.** `tests/formal/sat_add.sby`. The oracle is the
   (W+1)-bit sum, which cannot overflow and is therefore the true arithmetic
   answer; the properties say the W-bit result is that answer CLAMPED. That is a
   specification, not a second copy of the code -- the distinction the ABS
   defect in `zhao_field_alu` is this project's standing reminder about.
   Six assertions per width: clamped-sum, never-wraps, identity at b = 0, and
   the rail is absorbing.
3. **Non-vacuity.** Five covers, all reached with witness traces (`trace0`..
   `trace4`): it saturates, it hits the exact boundary `a + b == MAX`, it lands
   one past, it does an ordinary carry-free add, and the 32-bit width saturates.
4. **SCOPE-TOTAL rather than an `a_scope_*` guard.** V19 asks a bounded proof to
   pin its horizon. This harness is purely combinational with every input free,
   so depth 1 already evaluates all 2^128 pairs symbolically; there is no
   horizon to pin, which is the case the rule's explicit waiver is for.

### Mutation sweep: 7 attempted, 7 accounted, 6 caught, 1 equivalent

| mutant | result |
| --- | --- |
| M1 headroom test dropped (`~b` -> `b`) | CAUGHT (and passes the whole sim suite) |
| M2 rail off by one, 64-bit | CAUGHT |
| M3 boundary widened (`>` -> `>=`) | **EQUIVALENT** |
| M4 saturation removed entirely | CAUGHT |
| M5 headroom taken on the wrong operand | CAUGHT |
| M6 `a` compared against itself | CAUGHT |
| M7 rail off by one, 32-bit | CAUGHT |

**M3 is equivalent, not a hole.** `a >= ~b` differs from `a > ~b` only at
`a == ~b`, which is exactly `a + b == MAX`; there the original computes
`a + b == MAX` and the mutant returns `MAX` -- the same value. No input
distinguishes them, so no test can. And the case is not merely untested:
`c_exact_boundary` produces a witness reaching it.

### Two process failures on the way, both mine, both already-documented shapes

**A sweep that reported six survivors without mutating anything.** The first
harness called a script through a path the interpreter could not open, printed
`*** SURVIVED ***` six times, and its "preflight" did not abort. Identical in
shape to the `[String]::Replace($old,$new,1)` incident. Rewritten with a
hash-compare that aborts, a byte-identical revert check, and an
`attempted == expected == accounted` cross-check -- the numbers in the table
above come from that version.

**`ninja: error: rebuilding 'build.ninja': subcommand failed` -- again.** A
`cmake -S . -B build` run from a shell without the mingw toolchain on PATH
poisoned the cache; the next ctest reported **"100% tests passed, 256 tests"**
against a completely stale tree. The tell was the count: 256, when registering
`formal_sat_add` should have made it 257. This is the third time this exact
failure has produced a green result that meant nothing, and the second time the
count is what caught it.

### Ledger

The V20 rule caught my own comment: I had written that the guard "must be right
BY CONSTRUCTION" with nothing named as its enforcer, and the check refused it.
That was correct, and the fix was not to soften the sentence -- it was to build
the enforcer. Every one of the five sites now carries
`ENFORCED-BY: tests/formal/sat_add.sby`.

---

## 2026-08-22 -- FIELD.SEQ.CORE to RTL_VERIFIED, on a formal proof of the
## anti-hang law

### Why a new artifact was needed at all

I checked the convention before claiming the rung, and it settled a question I
had left open earlier in the session: **no block in this ledger advances to
RTL_VERIFIED citing the same file as its UNIT_VERIFIED.** Every one cites a
distinct artifact -- a separate `*_random.cpp`, a formal property, or a
system-level demo. So FIELD.SEQ.CORE could not advance on
`field_seq_directed.cpp`, which is already its UNIT_VERIFIED evidence.

### The law, which the design states twice and the differential cannot reach

From the contract and again beside the port:

> `instr_count_i` is a LIVENESS bound, not a semantic check. The decoder
> guarantees exactly one OP_END and that it is last, so a lawful program never
> reaches this limit. But the instruction MEMORY is the shell's to load, and a
> walk with no bound turns a mis-loaded memory into a machine that hangs
> forever instead of one that reports a status. **A hang is the worse failure,
> and it is the one nobody can debug from a frame capture.**

906 directed and 2,506 random programs cannot prove that, because every one of
them is a program somebody chose, and a hang is what happens on the program
nobody chose. In `tests/formal/field_seq_bound.sby` **every instruction word is
free** -- `ins_op_i` may be an opcode that does not exist and may change every
cycle -- which is exactly what a mis-loaded memory is.

    a_pc_bounded   the walk never steps past its bound
    a_progress     never busy for more than 120 consecutive clocks
    four covers    it RUNS, finishes cleanly, OVERRUNS, and refuses

bmc + cover green at depth 140.

### THREE FALSE COUNTEREXAMPLES, all mine, all recorded beside their fixes

Every one was the model being under-constrained rather than the design being
wrong, and that distinction is the whole point:

1. **`instr_count_i` free every cycle.** The solver lowered the bound out from
   under an advancing pc. Fixed by assuming it stable while busy -- which is
   what the contract already says.
2. **No forced initial reset.** `zhao_field_seq` declares `logic [3:0] state;`
   with no initialiser, which is correct RTL because reset assigns it -- but a
   free `rst_n` lets the solver simply never reset and choose the start state
   itself. It began mid-execute with a pc past the bound.
3. **`a_pc_bounded` ungated.** `pc` is zeroed by `start_i` and KEPT after a run,
   so an idle sequencer holds the last program's pc while the shell loads a new
   count. The ungated assertion compared one run's pc against the next run's
   bound and failed at k = 10 on a machine behaving perfectly.

**"The property failed so I assumed the failure away" is the single easiest way
to turn a formal lane into decoration.** The test I held each fix to: is this an
assumption the CONTRACT already makes? All three were.

### Two frontend limits worth knowing

* `busy |-> ##[1:120] done` is rejected -- "unsupported SVA feature". The same
  law is written as a counter and a bound, which is a safety property and
  exactly as strong here: if the machine can hang, the counter runs away.
* `initial assume (!rst_n)` is rejected -- "reading net state during design
  initialization unsupported". The first cycle is constrained through
  `f_past_valid` instead.

### V19 scope guard

The ledger refused the first attempt: a bounded proof must carry a guard that
FIRES if the depth is raised past what was proven. Added
`a_scope_short_program_window`, pinning depth 140 under the `instr_count_i <= 2`
shrink. Raising either without re-deriving the other now fails loudly instead of
quietly claiming more than was shown.

---

## 2026-08-22 -- MEM.GUARD counter moved: 125 -> 97 failing endpoints, and the
## CDC seam surfaces again

| | before (`ae3ce73`) | after (`de2794d`) |
| --- | ---: | ---: |
| setup worst | -0.639 ns | -0.729 ns |
| **failing endpoints** | 125 | **97** |
| hold | +0.250, 0 failing | -1.596, **4 failing** |
| ALMs | 7,648 | 7,667 |

**22% fewer failing setup endpoints**, a slightly deeper worst path, +19 ALMs.

### The change

`guard_violations` incremented in the cycle of the verdict, putting the whole
range-check chain on the ENABLE of a 32-bit register bank -- one decision
fanning out to 32 flops, which is why the census showed 1,383 paths from one
comparison. It now counts off `guard_violation`, the registered pulse that
already exists for exactly this verdict.

The safety path is untouched and `formal_mem_guard_no_escape` still passes.
**That proof is why I did this at all**: I had twice declined to touch this
block because its whole contract is that no memory escape exists, and what
changed is not nerve -- it is that a change here can be PROVEN safe rather than
argued safe, and this one is to a debug counter rather than the decision.

### The four hold violations are NOT this change

    -1.596  scanout_serializer|starve_q -> starve_samp   vid_clk -> gpu_clk
    -0.575  video_scaler|stage2.y       -> eof_pend      vid_clk -> gpu_clk
    -0.437  video_scaler|stage2.x       -> sof_seen_q    vid_clk -> gpu_clk
    -0.255  video_scaler|stage2.x       -> crc_in_sof    vid_clk -> gpu_clk

Every one is the GPU/video seam. The same families appeared at -1.064 with
three failures earlier today, vanished when the CRC pressure came off, and are
back now. They move with placement and have never been fixed -- which is
precisely the docket item: `DEBUG.CRC.md` says the displayed lane is
video-domain, `design/blocks.yml` says GPU, and the implementation followed
blocks.yml and built a per-pixel crossing.

**Three separate runs have now produced these, uncorrelated with what was
changed.** That is as strong an argument as the timing reports can make that
the seam needs the architectural fix rather than another placement roll.

### Kept, unlike the fold un-sharing

Both were principled and both left the worst path deeper. The difference: the
fold split cost 375 ALMs and made *everything* worse (125 -> 424 endpoints);
this one costs 19 ALMs and makes the endpoint count materially better. Where a
change has a real benefit and the regression is in an unrelated, known-unfixed
seam, keeping it is the honest call.

---

## 2026-08-22 -- fitter effort: 7x fewer failing endpoints, a deeper worst
## path, and two hold violations. FABIAN'S CALL.

The project had been on `OPTIMIZATION_MODE "BALANCED"` / `FITTER_EFFORT
"STANDARD FIT"` throughout. With the design shown to be placement-bound below
~1.5 ns, effort is the obvious lever and it had never been pulled.

| | BALANCED | HIGH PERFORMANCE EFFORT |
| --- | ---: | ---: |
| setup worst | **-0.639 ns** | -1.389 ns |
| failing endpoints | 125 | **17** |
| hold worst | **+0.250 ns, 0 failing** | -1.103 ns, **2 failing** |
| ALMs | **7,648** | 8,147 |

### Neither closes, and they fail differently

High effort left **seven times fewer failing endpoints** -- 17 against 125,
which is much closer to the zero that closure means. It also made the worst
path deeper, cost 499 ALMs, and introduced **two hold violations**.

That last item is why this is not a straightforward win. A setup failure means
the clock is too fast and can be answered by slowing down or pipelining. A HOLD
failure cannot: the data arrives too EARLY, and no clock speed fixes it. Two of
them is a worse position to be in than 125 setup endpoints at under 0.7 ns.

### Reverted, and why

Back to BALANCED. Two reasons, and the second matters more:

1. Every number recorded in this project -- every per-block fit, every composed
   result in this session's history -- was taken on BALANCED. Switching makes
   all of them incomparable with everything after.
2. **It is a project-basis decision, not a fix.** The brief in
   reports/composed/README.md is explicit that a fit at different effort is a
   different measurement, and it says to report back rather than change the
   basis to get a number. It says that about lowering effort; the principle is
   the same raising it.

So it is measured, recorded, and put to Fabian rather than decided quietly at
the end of a long session. **If the 17-endpoint result is the better starting
point for closure, the switch is one line and the two hold violations become
the next problem.**

---

## 2026-08-22 -- THE FIT IS DETERMINISTIC. Every A/B this session was signal.

I had been calling the later results "placement variation" without ever having
measured whether this flow varies at all. That was an assumption doing load-
bearing work, so I tested it: re-ran the composed fit on RTL byte-identical to
`ae3ce73` (confirmed with `git diff ae3ce73 HEAD -- fpga/rtl/`, empty).

| | `ae3ce73` | `e6b5fef`, same RTL |
| --- | ---: | ---: |
| setup worst | -0.639 ns | **-0.639 ns** |
| failing endpoints | 125 | **125** |
| hold worst | +0.250 ns | **+0.250 ns** |
| ALMs | 7,648 | **7,648** |

**Identical to the digit.** Quartus's placement is seeded deterministically
here, so two fits of the same source give the same answer.

### What that changes

Every A/B comparison in this session was real signal:

* the header-ladder split really did make the design worse (-1.096 -> -1.472),
  not noise;
* un-sharing the folds really did make it worse (-0.639 -> -1.414), not noise;
* and the six improvements really were improvements.

I had hedged both regressions as "placement variation". They were not variation
-- they were consequences. The reasoning I gave for each still holds (the
fitter's effort moved elsewhere; the extra area cost more than the removed mux
saved), but "variation" was the wrong word and it let me off too lightly.

### What it does NOT change

The design is still placement-BOUND below ~1.5 ns: small structural changes
move the number unpredictably in sign, even though each individual measurement
is repeatable. Deterministic is not the same as insensitive.

### The standing state

    setup   -0.639 ns   125 failing endpoints   1,995 negative-slack paths
    hold    +0.250 ns     0 failing
    ALMs    7,648 of 41,910

Worst path 10.64 ns against a 10 ns budget: 6.4% over. Not closed.

The largest remaining family is 1,383 paths at -0.195 ns from
`scanout|fetch -> mem_guard|guard_violations` -- the guard's range check. I
have deliberately NOT touched it. It is 2% over, it is the one block whose
entire contract is that no memory escape exists, its no-escape proof depends on
the response timing, and the last two structural changes I made both made the
headline worse. That trade is bad at this margin and it is Fabian's call, not
one to make quietly at the end of a long session.

---

## 2026-08-22 -- un-sharing the folds made it WORSE, and is reverted

The census after the -0.639 ns run named the worst path precisely:

    -0.639 ns  hps_arbiter|held_req.client[0] -> cmd_dma|crc_pay_r[28]
    -0.630 ns  hps_arbiter|held_req.client[0] -> cmd_dma|crc_hdr_r[28]

The arbiter's routing decision reaching the CRC through the state-selected mux
on the fold's DATA input -- the consequence of sharing one fold pair between
the header/seed walk and the streaming path. The review Fabian relayed had
warned against exactly that over-sharing, and I had done it anyway.

So I split them: a walk pair on `hw_word`, a streaming pair taking
`hps_rsp_i.data` directly with nothing in between.

**Measured, and it is worse:**

| | shared (`ae3ce73`) | un-shared (`9ac7881`) |
| --- | ---: | ---: |
| setup worst | **-0.639 ns** | -1.414 ns |
| failing endpoints | **125** | 424 |
| ALMs | **7,648** | 8,023 |

Reverted in `692434f`.

### Why this is not a contradiction of the advice

The reasoning was right: a mux in the data path of the thing being shortened is
a real cost, and removing it did shorten that path. But two extra fold
instances are +375 ALMs, and at this margin the fitter's placement dominates.
The design is 3.4x worse by endpoint count for a structurally cleaner circuit.

**That is the second change this session that was well-motivated, did what it
was designed to do, and made the headline worse.** The first was the header
ladder split. Both are the same lesson: below about 1.5 ns, this design's
timing is placement-bound, not structure-bound, and a change that removes real
work can still lose.

The difference in what I did with them: the ladder split was KEPT, because its
own family improved and the degradation was in families it never touched. This
one is REVERTED, because its own cost -- the area -- is what caused the
regression, and the best measured state is the shared one.

### The standing state is `ae3ce73`'s numbers

    setup   -0.639 ns   125 failing endpoints   1,995 negative-slack paths
    hold    +0.250 ns     0 failing
    ALMs    7,648

Worst path 10.64 ns against a 10 ns budget. Not closed.

---

## 2026-08-22 -- MEASURED: -0.639 ns. Best of the campaign, and the census is
## down from 13,651 paths to 1,995.

Composed fit at clean HEAD `ae3ce73`.

| | start | best before | now |
| --- | ---: | ---: | ---: |
| setup worst | -56.374 ns | -1.096 ns | **-0.639 ns** |
| failing endpoints | 3,746 | 172 | **125** |
| negative-slack paths | -- | 13,651 | **1,995** |
| hold | -1.064, 3 failing | +0.251, 0 | **+0.250, 0** |
| ALMs | 9,181 | 7,713 | **7,648** |

**Worst path 65 ns -> 10.64 ns against a 10 ns budget: a 6.5x overrun is now
6.4%.** And the design is ~1,530 ALMs smaller than when the campaign started.

### The remaining census, in full

    1,383  scanout|fetch  -> guard_scan|guard_violations   -0.195
      144  cmd_dma|fetched -> cmd_dma|burst_end            -0.338
       25  cmd_dma|pkt_len_r -> cmd_dma|stream_w           -0.298
        9  cmd_dma|m.M_SEED  -> cmd_dma|crc_hdr_r          -0.471
        8  cmd_dma|need_total -> cmd_dma|burst_end         -0.032
        6  cmd_dma|m.M_SEED  -> cmd_dma|crc_pay_r          -0.480

The `cb -> seed_steps_q` family that was 18,013 paths at -1.324 is gone. Two
thirds of what is left is one scanout-to-guard family at -0.195, which is
within noise of the target.

### The whole campaign, five fixes

| fix | what it was | effect |
| --- | --- | ---: |
| parallel CRC fold | 8 dependent XOR levels per byte, 64 deep per beat | families eliminated |
| FRAMEBLIT rewire | the same chain in a second block | -28 ns family gone |
| CMD.DMA CRC walks | 224 XOR levels behind a command_bytes-derived bound | -55 -> -3 ns |
| r_len pipelining | a subtract fanning out four ways in the cycle used | -3.067 -> -1.096 |
| record-walk split | RAM output -> opcode lookup -> ladder in one cycle | family gone |
| seed as a 3-way decision | an add, min, subtract and two compares for a value that is 0, 16 or 28 | -1.324 -> -0.639 |

Every one was accidental combinational depth. None was evidence the
architecture is too slow, and the design got SMALLER at every step.

### The lesson that generalises

The last fix is the one worth remembering. I had PROVEN the seed length can
only be 0, 16 or 28, used that proof to justify constant fold widths, written
it in a comment -- and then gone on computing the value the long way. The proof
was in the file and the arithmetic ignored it. 18,013 of 20,000 negative-slack
paths ran through that gap.

**A law you have proven is worth nothing until the logic is written in its
terms.** It is now, and the law itself is asserted:

    a_seed_bytes_lawful    seed_bytes_q in {0, 16, 28} when m == M_SEED
    a_payload_end_aligned  payload_end_q[2:0] == 3'b100

### Not closed

125 endpoints still fail. Timing closure means zero.

---

## 2026-08-22 -- THE LADDER SPLIT WORKED AND THE DESIGN GOT WORSE. Both are true.

| | before (`a0ef3ec`) | after (`14e5a21`) |
| --- | ---: | ---: |
| setup worst | -1.096 ns | **-1.472 ns** |
| failing endpoints | 172 | **355** |
| hold | +0.251, 0 failing | **-1.366, 1 failing** |
| ALMs | 7,713 | 7,671 |

**The headline moved the wrong way.** Recorded as such rather than buried.

### The census says the change did what it was for

Full negative-slack census both sides, via `quartus_sta` against the preserved
fit:

| family | before | after |
| --- | ---: | ---: |
| `hdr_win -> seed_steps_q` | 6,973 @ -1.096 | -- gone -- |
| `cb -> seed_steps_q` | -- | 5,619 @ **-0.854** |
| `slot_ram -> done_status` | 81 @ -0.466 | 623 @ **-1.472** |
| `slot_ram -> walk_off` | @ -0.838 | @ **-1.155** |
| `slot_ram -> walk_cnt` | @ -0.564 | @ **-1.082** |
| `scanout\|fetch -> guard_scan` | 31 @ -0.019 | **3,862** @ -0.468 |

The targeted family improved by 0.24 ns and shrank by 1,354 paths. It is no
longer the worst. Every family that overtook it is one the change did not
touch, and one of them -- the scanout-to-guard path -- went from 31 paths at
essentially zero to 3,862.

### What that actually means, and it is the useful finding

**At this margin, fixing one family no longer monotonically improves the
design.** The fitter had been spending its effort holding the header ladder
together; relieved of that, it spent it elsewhere and three other families
drifted past. Nothing regressed structurally -- the RTL those families run
through is unchanged.

The first three fixes were 6.5x, 4x and 3x overruns: real structural depth,
where any improvement had to show. Everything left is within 1.5 ns of the
target, which is placement territory. The remaining work is broad-front, not
another hotspot hunt.

### The change is kept

It removes real work from a cycle that already does seven checks, and its own
family improved. Reverting because an untouched family drifted would be
choosing the number over the reasoning. But it is NOT an improvement to the
design and is not recorded as one.

### The honest state

    setup   -1.472 ns   355 failing endpoints   13,585 negative-slack paths
    hold    -1.366 ns     1 failing

Timing is not closed and the last three measurements have been -3.067, -1.096,
-1.472. The trend is no longer monotone.

---

## 2026-08-22 -- MEASURED: -1.096 ns. The campaign in three steps.

Composed fit at clean HEAD `a0ef3ec`.

| | start | after CRC folds | after r_len pipelining |
| --- | ---: | ---: | ---: |
| setup worst | -56.374 ns | -3.067 ns | **-1.096 ns** |
| failing endpoints | 3,746 | 584 | **172** |
| hold | -1.064, 3 failing | +0.245, 0 | **+0.251, 0** |
| ALMs | 9,181 | 7,633 | 7,713 |

**Worst path 65 ns -> 10.9 ns against a 10 ns budget.** A 6.5x overrun is now
an 11% one, and the design is ~1,470 ALMs smaller than when it started.

### What is left, and it is no longer one thing

    29  slot_ram write-enable -> cmd_dma|walk_off      the record walk
    29  slot_ram write-enable -> cmd_dma|walk_cnt
     7  cmd_dma|wr_off        -> cmd_dma|hdr_win
     7  cmd_dma|cw            -> cmd_dma|crc_pay_r     the folds themselves
     5  cmd_dma|m.M_SEED      -> cmd_dma|crc_pay_r
     5  cmd_dma|cw            -> cmd_dma|crc_hdr_r
     4  cmd_dma|m.M_SEED      -> cmd_dma|crc_hdr_r
     2  hps_arbiter|state     -> cmd_dma|crc_pay_r

and the worst single path:

    -1.096 ns  cmd_dma|hdr_win[29][1] -> cmd_dma|seed_steps_q[0]
               10.902 ns

`hdr_win[29]` is part of command_bytes again, but this time it is not a CRC:
it is the header ladder itself. That one cycle checks magic, ABI version,
reserved flags, four length laws, the header CRC, the epoch, AND now derives
the seed controls. Splitting the ladder across two states is the obvious next
move, and it is a sequencing change rather than an arithmetic one.

The fold paths appear here for the first time at ~1 ns over, which is what a
seven-level XOR tree plus its state mux should cost. They are no longer the
problem; they are simply now visible above the others.

### The shape of the whole campaign

Three structural defects, each found by reading the tool's report rather than
the source:

1. a bit-serial CRC chained 8 bytes deep per beat and 28 deep for a seed;
2. the same chain in a second block;
3. a combinational subtract fanning out four ways in the cycle it was used.

None of them was evidence that the architecture is too slow. All three were
accidental depth, and the design got SMALLER when they were removed.

### Still not closed

172 endpoints fail. The report holds 100 of them. The audio Gray-decode family
that was -14.9 ns has not appeared since the first fix and has never been
confirmed fixed -- it needs the full census, not an inference from its absence.

---

## 2026-08-22 -- MEASURED: -56.374 ns -> -3.067 ns, and the design got SMALLER

Composed fit at clean HEAD `fd262de`.

| | before (`8ff73c9`) | after (`fd262de`) |
| --- | ---: | ---: |
| setup worst | -56.374 ns | **-3.067 ns** |
| setup failing endpoints | 3,746 | **584** |
| hold worst | -1.064 ns, **3 failing** | **+0.245 ns, 0 failing** |
| ALMs | 9,181 | **7,633** |
| registers | 9,576 | 9,704 |

**An 18x reduction in the violation, and 1,548 FEWER ALMs.** The bit-serial CRC
chains were expensive in logic as well as in depth; replacing 224 dependent XOR
levels with two constant-width fold trees gave back area rather than costing it.

**The three hold violations are gone too.** They were placement-dependent on the
GPU/video seam, and with the CRC pressure removed the fitter no longer has to
contort around it. That is an observation, not a fix -- the seam is still
crossed directly and still deserves the architectural answer.

### Every CRC family has left the report

    before:  33  cmd_dma|hdr_win        -> cmd_dma|crc_pay_r      -55.2
             33  hps_arbiter|state      -> frameblit|crc_acc      -28.8
              7  audio_fifo|cnt_gray_sync -> cnt_snap_o.value     -14.9

    after:   48  frameblit|r_len -> frameblit|off                  -3.067
             30  frameblit|r_len -> mem_guard|guard_violations
             15  frameblit|r_len -> mem_guard|fwd_req.addr
              5  frameblit|r_len -> mem_guard|fwd_req.len
              2  frameblit|r_len -> frameblit|state

Not one CRC path remains. The audio Gray-decode family has also dropped out of
the top 100 -- it was -14.9 ns and the new worst is -3.067, so it is either
below the reporting threshold or was inflated by placement pressure. It should
be re-checked in the full census rather than assumed fixed.

### The new worst path is one register's fan-out

    -3.067 ns  zhao_debug_frameblit|r_len[2] -> zhao_debug_frameblit|off[12]
               12.866 ns against a 10 ns budget

Every remaining failing family starts at `r_len`. That is the blit length
feeding `remaining = r_len - off`, then `this_len`, then the offset update and
the guard request's address and length. A subtract, a compare, a select and an
address add, in one cycle, fanning out to four destinations.

**12.9 ns is a 29% overrun, not a 6.5x one.** This is ordinary pipelining work
rather than a structural defect: `remaining` and `this_len` can be registered a
cycle ahead, since `off` only changes at chunk boundaries.

### Credit where it is due

The shape of this fix came from a review Fabian relayed, and two of its points
were things I had got wrong:

1. **Constant fold widths, not a runtime count.** My first cut shared one
   generic instance with a runtime `n_i`, which keeps all nine matrices and
   adds a nine-way mux after the XOR trees. Two instances at constant 8 and 4
   let Quartus discard the rest.
2. **The streaming path needs no shifter.** I had written one for a case that
   cannot occur: `M_PAY_WAIT`'s `wr_off` starts at `fetched` >= 40 and the
   payload starts at 36, so the lower bound never clips.

I verified the load-bearing claim myself rather than taking it: with
`command_bytes % 16 == 0` and the first burst capped at 64, the seed length can
only be 0, 16 or 28 bytes, and `36 + cb` is always 4 mod 8. Both check out.

---

## 2026-08-22 -- MEASURED: the FRAMEBLIT rewire removed its family and did not
## move the headline

Composed fit at clean HEAD `8ff73c9`, all four stages successful.

### Before and after

| | before (`d67621d`) | after (`8ff73c9`) |
| --- | ---: | ---: |
| ALMs | 9,167 | 9,181 (+14) |
| registers | 9,171 | 9,576 |
| setup worst | -55.199 ns | **-56.374 ns** |
| setup failing endpoints | 3,595 | **3,746** |
| hold worst | +0.247 ns | **-1.064 ns, 3 failing** |

### What the rewire actually did, which the summary numbers hide

The failing families, before:

    33  cmd_dma|hdr_win        -> cmd_dma|crc_pay_r     -55.2 .. -53.3
    33  hps_arbiter|state      -> frameblit|crc_acc     -28.8 .. -24.2
     7  audio_fifo|cnt_gray_sync -> cnt_snap_o.value    -14.9 .. -10.6

and after:

    32  cmd_dma|hdr_win        -> cmd_dma|crc_pay_r     -56.4 .. ...
    11  audio_fifo|cnt_gray_sync -> cnt_snap_o.value

**The FRAMEBLIT CRC family is gone.** Thirty-three failing paths at -28.8 ns
eliminated, for +14 ALMs. The fold does what it was built to do.

### And the headline did not move, for a reason that was known in advance

The worst path is CMD.DMA's payload-CRC seed loop, which **still calls the
bit-serial chain** -- it was deliberately left for a second pass. Removing a
-28 ns family cannot improve a -55 ns worst case. The -55.199 -> -56.374 drift
is 2% on a path that was not touched: placement variation, not a regression.

### Two things NOT fully explained, recorded as such

1. **Failing endpoints rose 3,595 -> 3,746** even though a failing family was
   removed. The top-100 path report cannot account for that; it holds 100 of
   3,746. Unexplained.
2. **Three hold violations appeared**, worst -1.064 ns, all of them
   `vid_clk -> gpu_clk`:

       vphase                        -> vph_q
       zhao_video_scaler|stage2.y[6] -> crc_in_sof
       zhao_video_mode|mode_cur[1]   -> eof_pend

   None involves DEBUG.FRAMEBLIT. They sit on the GPU/video seam this project
   deliberately leaves TIMED rather than false-pathed -- the timing report's own
   `knownCdc` note says so -- so hold on them is placement-dependent and was
   +0.247 ns by luck in the previous run rather than by construction. That is
   an explanation of the mechanism, not a dismissal: three real hold
   violations are recorded here and they need a constraint decision, not a
   shrug.

### The reading

The approach is validated and the remaining work is located. CMD.DMA's seed
loop is next, and it is a bigger change than FRAMEBLIT's drop-in: the seed
folds a byte range that starts at 36 and ends wherever `command_bytes` says, so
it needs a multi-cycle walk rather than one instance with `n_i` tied high.

---

## 2026-08-22 -- the parallel CRC-32C fold, built and verified

`fpga/rtl/common/zhao_crc32c_fold.sv`. Folds up to eight bytes in ONE shallow
XOR tree instead of sixty-four dependent levels.

    fold_N(c, d) = XOR over set bits i of c of fold_N(1<<i, 0)
               XOR XOR over set bits j of d of fold_N(0, 1<<j)

CRC-32C is linear over GF(2) with no constant term, so each column is a
compile-time constant and the runtime logic is 96 masked 32-bit XORs -- a tree
about seven levels deep. **The columns are derived at elaboration by calling
the bit-serial definition itself**, so the module cannot drift from what it
replaces: change the polynomial and the columns change with it.

`n_i` selects how many leading bytes participate, because no caller always has
eight -- the seed walks a range ending wherever `command_bytes` says, and a
final bridge beat can be partial. Nine matrices, muxed.

### The oracle, and why it is not a restatement

`zhao_abi::zhao_crc32c` wraps the raw running state in an inversion at each
end, so

    zhao_crc32c(c, buf, n) == ~raw_chain(~c, buf, n)

and the raw transform is recovered exactly by undoing both. The test therefore
compares against **the shipped function every other CRC user calls**, not a
second copy of the algorithm written beside it. That distinction is not
academic here: `zhao_field_alu`'s ABS returned the wrong answer for INT32_MIN
and its test restated the law the same wrong way, so the two agreed with each
other through 828 directed and 120,000 random checks.

### Verification

    crc32c_fold_directed        1,178 checks
    crc32c_fold_random          200,000 folds
    crc32c_fold_random_nightly  4,000,000

The directed lane includes **every state basis vector and every data basis
vector at every byte count** -- 32 + 64 columns x 9 counts -- because the module
IS its columns, and a wrong column is exactly a wrong answer for one basis
input that random traffic need never isolate. Plus: n=0 is the identity, bytes
above the count do not participate, and a 256-byte message folded eight at a
time matches the byte-at-a-time chain at every length.

**Mutation sweep: 18 attempted, 18 caught, 0 survivors.** Wrong polynomial,
wrong shift direction, wrong bit tested, seven rounds instead of eight, data
not mixed in, data mixed at the top, high-byte-first ordering, every byte
folded regardless of the count, count off by one, both basis derivations
reversed, either column set dropped, data columns truncated, accumulator seeded
wrong, columns OR-ed instead of XOR-ed, and both count-mux errors.

### What remains

**This is the primitive, verified in isolation. It is not yet wired in.**
CMD.DMA's seed loop and streaming CRC, and DEBUG.FRAMEBLIT's `crc_acc`, still
call the bit-serial chain. Wiring it in is the next step, and the composed fit
is what will say whether it moves the -55.199 ns.

Deliberately stopped here rather than rewiring two verified blocks at the tail
of a long session: the primitive is worth having proven on its own, and the
rewire wants its own differential run against unchanged packet behaviour.

---

## 2026-08-22 -- TIMING DIAGNOSED. It is the CRC, and it is bit-serial.

Re-ran the composed fit with `-KeepWorkspace` to keep `setup_paths.rpt`, on the
principle that cost four wrong theories earlier today: measure, do not infer.

### The failing paths

Worst path, and it names itself:

    -55.199 ns   zhao_cmd_dma:u_dma|hdr_win[28][4]
              -> zhao_cmd_dma:u_dma|crc_pay_r[3]      64.584 ns data delay

`hdr_win[28]` is `command_bytes`. So the chain is: read `cb` out of the header
window, compute `seed_end = 36 + cb`, run the 28 guarded CRC-32C steps of the
payload-CRC seed, land in `crc_pay_r`. **The payload-CRC SEED LOOP**, not the
`crc_final()` header sweep I had guessed at.

Three families among the 100 worst, all on `gpu_clk`:

| paths | from -> to | slack | delay |
| ---: | --- | ---: | ---: |
| 33 | `cmd_dma\|hdr_win` -> `cmd_dma\|crc_pay_r` | -55.2 .. -53.3 | ~63-65 ns |
| 33 | `hps_arbiter\|state.A_IDLE` -> `frameblit\|crc_acc` | -28.8 .. -24.2 | ~34-39 ns |
| 7 | `audio_fifo\|cnt_gray_sync` -> `cnt_snap_o.value` | -14.9 .. -10.6 | ~20-25 ns |

### The cause, and it is one line of shared code

`zhao_abi_pkg::zhao_crc32c_step` is **BIT-SERIAL**:

    crc = c ^ {24'b0, d};
    for (int i = 0; i < 8; i++)
      crc = (crc >> 1) ^ (crc[0] ? 32'h82F63B78 : 32'b0);

Eight DEPENDENT XOR levels per byte. So:

* FRAMEBLIT and `M_PAY_WAIT` fold 8 bytes per beat = **64 chained levels**
  -> ~38 ns measured;
* the seed loop folds 28 bytes in one cycle = **224 chained levels**
  -> ~64 ns measured.

Both against a 10 ns budget. This is not a placement problem or a fitter
problem; it is a combinational depth problem in code every CRC user shares.

**Spreading the seed loop over more cycles does NOT fix it.** FRAMEBLIT already
does only 8 bytes per cycle and still costs 38 ns. At ~0.6 ns per level the
budget is roughly 16 levels, which is TWO bytes per cycle -- and the streaming
payload CRC has to keep up with 8 bytes per bridge beat, so it cannot be slowed
down at all.

### The fix, which is standard

CRC-32C is a LINEAR function of its input, so folding N bytes is a fixed
GF(2) matrix -- an XOR tree of depth ~log, not 8N dependent levels. Replacing
the bit-serial loop with a parallel form makes 8 bytes per cycle cost a handful
of levels instead of 64.

It is verifiable bit-exactly against what exists: `zhao_crc32c_step` is itself
the oracle, and equivalence over random `(c, data)` is a pure combinational
check that can be driven very hard.

### A caveat that matters, from the same run

    Unconstrained Input Ports       609
    Unconstrained Input Port Paths  13,920

Paths that START at an input port are not analysed. The STREAMING CRC chains
begin at `hps_rsp_i.data`, which is an input port of these blocks, so they fall
in that class. **The -55.199 ns is a lower bound on the problem, not a
measurement of all of it.** The harness's own limitations list already says
unconstrained paths remain reportable; here it materially changes what the
number means.

---

## 2026-08-22 -- the sweep harness could mis-attribute a verdict, and did

### What happened

The Field IR arithmetic-core sweep came back 31/34 with four mutations tagged
"caught by the RANDOM lane only", meaning the directed cases supposedly could
not tell them apart. One of the four was `sub_is_add` -- replacing `a - b` with
`a + b` in the block's own subtract.

That is not a plausible directed-lane miss, and it is not one. Applied by hand:

    FAIL: FIELD.SUB: zero minus INT32_MIN saturates: value:
          expected 0x7FFFFFFF, got 0x80000000
    [field_alu_ops] 9/984 checks FAILED

**The attribution was simply wrong.** All four were CONSECUTIVE in the
mutation order, which is the tell: four independent coverage gaps do not
arrive in a row.

### The flaw in my own guard

Every sweep in this project discards a result when the generated-model hash
equals the BASELINE hash, on the reasoning that an unchanged hash means
Verilator never re-ran. That catches the case it was written for and misses a
worse one:

**a build that silently serves the PREVIOUS mutant's model.** That hash differs
from the baseline, so the guard passes it, and the verdict is recorded against
the wrong mutation. The sweep was sharing the machine with a `quartus_fit` run
at the time, which is exactly when such a race surfaces.

So the guard proved "something was rebuilt", not "THIS mutation was built" --
a weaker statement than the one it was making, which is the same defect shape
as the parity gate earlier today.

### Fixed

The hash must now differ from the baseline **and** from the previous
iteration's hash. A stale model is identical to the previous mutant's, so it is
caught and the sweep aborts rather than reporting.

    if h == base_hash:  DISCARD -- Verilator did not re-run
    if h == prev_hash:  DISCARD -- this build served a stale model and the
                        verdict would be attributed to the wrong mutation

### The actual cause, found after two more wrong theories

I blamed concurrency with the Quartus fitter. Then I blamed a stale-model
race and strengthened the hash guard. Then the strengthened guard fired on a
QUIET machine, which killed that theory too. Then I "proved" one mutation
equivalent by diffing the generated C++ and getting zero lines.

All four were wrong, and they were wrong about the same thing:

    ninja: error: rebuilding 'build.ninja': subcommand failed

**When ninja cannot regenerate build.ninja it builds NOTHING AT ALL**, and it
says so in one line that scrolls past while the rest of its output looks
ordinary. Every mutation was testing whatever binary happened to be lying
around from an earlier build.

Why it could not regenerate: `tests/CMakeLists.txt` reads
`set(VERILATOR_ROOT "$ENV{VERILATOR_ROOT}")` -- from the ENVIRONMENT, at
configure time. Editing `tests/lint/source_list_parity.cmake.in` earlier in the
session marked `build.ninja` stale, and every subsequent build from a shell
without `VERILATOR_ROOT` set tried to reconfigure, failed, and did nothing.

**The zero-line diff that "proved" the equivalence proved only that the model
had not been regenerated.** The equivalence is still true, but it is true by
algebra -- clamping a value already on the rail returns that same value -- and
that argument never needed the diff. The diff was evidence of nothing and it
was quoted as if it were the stronger form.

### Fixed

`build()` sets `VERILATOR_ROOT` explicitly and treats
`rebuilding 'build.ninja'` in ninja's output as FATAL. A sweep that cannot
build must stop, not report.

### Which earlier results this affects

**None of the committed ones.** `build.ninja` only became stale when the
parity-gate template was edited, in commit `f0e101b`. The CMD.DMA RAM sweep
(19/19), the DEBUG.FRAMEBLIT atomicity sweep (20/20) and the sequencer
ALU-dispatch sweep (10/11 + 1 equivalent) were all run and committed BEFORE
that, on a build tree that regenerated normally. They stand.

The arithmetic-core sweep is the only one affected, and its first three
attempts are discarded. The fourth, on a sound build, is the one recorded:
**34 attempted, 31 caught -- every one by the DIRECTED lane -- 3 equivalent by
algebra, 0 real gaps.**

### The lesson, which is the same one as the morning's

Four theories about a failing measurement, each reasoned from the code, none
from the tool's own output. The line that settled it had been printed on every
single run. This is the second time today that reading what the tool actually
said would have replaced an afternoon of inference -- the first was
`Total block memory bits: 0` sitting in a report since the first CMD.DMA fit.

The three survivors are a separate matter and stand on proof rather than on
the sweep: they are the `sat32` boundary mutations, equivalent because clamping
a value already sitting exactly on the rail returns that same value. Written up
in the RTL with the argument, and the ledger bound they do NOT affect lives in
`sat32_fired`, whose own mutations were caught both ways.

---

## 2026-08-22 -- THE COMPOSED SHELL SYNTHESISES. First composed result this
## project has ever had.

    tools/quartus/run_composed_fit.ps1 -NoPush
    run: wumen-f0e101b-20260822T051855Z
    Quartus 17.0.2 Lite, 5CSEBA6U23I7, NUM_PARALLEL_PROCESSORS staged to 1

    PASS source parity: 26 ordered shell sources match tests/CMakeLists.txt.
    RUN analysisAndElaboration  47s    peak virtual memory 4,998 MB
    RUN synthesis             3m26s    peak virtual memory 4,968 MB
    RUN fitter                        peak virtual memory 5,359 MB
    RUN timequest
    exit=0 after 1679.3s   CLEAN_HEAD f0e101b

### IT FITS. IT DOES NOT CLOSE TIMING.

The script prints "PASS analysis/elaboration, synthesis, fitter, and
TimeQuest", and that means the four STAGES ran, not that timing was met. The
report is explicit: `"timingPassed": false`. Both statements are in the same
run and only one of them is the answer to the device question.

**Fitted resources** -- comfortable:

| | composed shell | device |
| --- | ---: | ---: |
| Logic utilization (ALMs) | **9,167** | 41,910 (**21.9%**) |
| registers | 9,571 | |
| block memory bits | 114,688 | |
| RAM blocks | 13 | 553 |
| DSP blocks | 0 | 112 |
| virtual pins | 2,336 | |

**Timing** -- not close:

| analysis | worst slack | failing endpoints |
| --- | ---: | ---: |
| setup | **-55.199 ns** | **3,595** |
| hold | +0.247 ns | 0 |
| recovery / removal | -- | 0 |

And the failure is in ONE domain:

    Slack       TNS         Clock
    -55.199     -12870.651  gpu_clk
      1.468          0.000  vid_clk
     33.665          0.000  audio_clk

`gpu_clk` is targeted at 10 ns. The worst path takes about 65 ns, so it misses
by roughly 6.5x. `vid_clk` and `audio_clk` both pass.

### The caveat, stated but not used as an excuse

Three of the five critical warnings are the same one: the clock ports are fed
by VIRTUAL PINS, so "timing analysis treats input to the clock port as a ripple
clock" -- the clock arrives through general routing rather than a global clock
network. That makes this pessimistic by an amount nobody has measured. It does
not plausibly account for 55 ns.

### What is NOT yet known, and will be measured rather than guessed

Which paths. The obvious suspect is CMD.DMA's `M_HDR_CHK`, which evaluates
`crc_final()` over 32 bytes -- 32 chained CRC-32C steps, on the order of 256
dependent XOR/shift stages -- plus the payload-CRC seed loop over 28 more, all
combinationally in ONE cycle. That is the same shape as the 192-iteration loop
that made this block unsynthesisable, bounded but not shortened.

**That is a hypothesis and it is written down as one.** Four hypotheses about
this exact block were wrong earlier this week, every one an inference about
what the fitter was building, and the thing that settled it was reading the
report. The next step is `report_timing` on the failing endpoints with
`-KeepWorkspace`, not another theory.

### The numbers

| | composed `zhao_shell_top` | device |
| --- | ---: | ---: |
| Estimate of Logic utilization (ALMs needed) | **9,440** | 41,910 (**22.5%**) |
| Combinational ALUT usage for logic | 11,902 | |
| Dedicated logic registers | 9,171 | |
| Total block memory bits | **114,688** | |
| Total MLAB memory bits | 0 | |
| Total DSP Blocks | 0 | 112 |
| Total virtual pins | 2,336 | |

### What this settles

`reports/composed/README.md` named the blocker precisely: **Quartus Error
276003**, registers that cannot convert to RAM megafunctions, from two memories
with asynchronous read logic -- `zhao_scanout_linebuf|mem` (fixed earlier) and
`zhao_cmd_dma|blit_buf` (not fixed).

`blit_buf` left with the blit engine, and `slot_buf` became `slot_ram` with a
registered read earlier today. **No 276003 in this run.** 114,688 block memory
bits, 0 MLAB bits -- the memories are real M10K.

It also demolishes the memory story that shaped months of planning. The record
said composed analysis + synthesis cost **42:33 at a 6.2 GB peak** and was the
reason a second machine was briefed. Measured now: **4m13s total at a 5.0 GB
peak**, on a 23.8 GB machine. The 28.4 GB figure was already known to be a
wildcard-VIRTUAL_PIN bug; the residual 6.2 GB was recorded as "unexplained
rather than settled", and a large part of it was CMD.DMA's 32,768-register
staging array, which no longer exists.

**The handoff brief in reports/composed/README.md is now doubly superseded:
the machine was never needed and the RTL blocker it names is gone.**

### Scope, stated plainly

This is `zhao_shell_top` -- 26 modules: the command spine, video, memory,
debug. It is NOT all 92 ledger blocks, and it is not a programmed board or
fabricated silicon. A composed fit against a provisional device with virtual
I/O says the design maps and closes timing IN THE TOOL. Nothing has run.

---

## 2026-08-22 -- the composed fit's runner could never start, and the tooling
## environment is not reproducible

### The composed fit has never run, and the reason is a two-line script bug

`run_composed_fit.ps1` failed **after 0 seconds**:

    EXCEPTION: Argument transformation for parameter "Processors":
    cannot convert value "-ReportRoot" to type "System.Int32"

Windows PowerShell 5.1 **array splatting supplies POSITIONAL arguments**. The
strings that look like parameter names are handed over as values, so
`@('-ReportRoot', $runDir, ...)` put the literal `-ReportRoot` into
`run_shell_fit.ps1`'s first positional parameter, which is `[int]$Processors`.

Reproduced directly, then fixed by switching to **hashtable splatting**, which
binds by name. This script could not reach `quartus_map` on any invocation, so
the composed fit's own runner had never started a fit.

### Then it found a real defect: the two source lists disagree on ORDER

    Shell source parity failed at index 23:
      CMake='fpga/rtl/debug/zhao_debug_frameblit.sv',
      QSF='fpga/rtl/video/zhao_video_slotmgr.sv'

`zhao_debug_frameblit` and `zhao_video_slotmgr` are swapped between
`tests/CMakeLists.txt` and the QSF.

**And my own gate could not see it.** `source_list_parity` was written earlier
this month to catch exactly this class, and its comment says: *"This gate does
not merge the lists (Quartus needs an ORDER, CMake does not). It asserts the
SETS match."* True of the tools, false of the project --
`run_shell_fit.ps1` compares the lists INDEX BY INDEX and refuses to run.

So a gate written to catch two-statements-of-one-fact was itself a weaker
statement of the fact it guarded. Order checking added; verified with teeth by
swapping the pair back (fails in 0.07 s naming index 22) and restoring.

    PASS source parity: 26 ordered shell sources match tests/CMakeLists.txt.

### FOURTH instance of the same environment defect

Four different tools, one cause: **PowerShell's PATH resolves to a different
toolchain than the one the build tree was created with.**

| tool | resolved to | consequence |
| --- | --- | --- |
| `ctest` | msys2 | all 337 tests reported BAD_COMMAND |
| `git` | msys2 (no autocrlf) | 29 RTL files called modified; `rtlCleanAtHead` never true in 42 rows |
| `cmake` | msys2 | "CXX compiler is unknown", configure refused |
| `verilator` | -- | a reconfigure in the wrong env emptied `VERILATOR_ROOT`, and every lint test fell back to a compiled-in `/yosyshq/...` path that does not exist here |

The last one was self-inflicted: reconfiguring from PowerShell dropped
`VERILATOR_ROOT`, because `tests/CMakeLists.txt` reads it from
`$ENV{VERILATOR_ROOT}` at configure time. 68 lint tests failed until it was
restored and the tree reconfigured with it set.

**The working combination, recorded so it is not rediscovered:**

    ctest    C:/Programmieren/dsstuff/mingw64/bin/ctest.exe
    cmake    C:/Programmieren/dsstuff/mingw64/bin/cmake.exe
    git      Bash's /mingw64/bin/git, or force -c core.autocrlf=true
    configure with VERILATOR_ROOT set, and with the build dir spelled
      exactly as CMakeCache.txt has it (the cache is case-sensitive about it)

    ctest -L fast: 252/252 after the repair.

---

## 2026-08-22 (later) -- DEBUG.FRAMEBLIT to RTL_VERIFIED; two sweeps; the clean flag proven

### DEBUG.FRAMEBLIT mutation sweep -- the atomicity law

    scratchpad/mut_frameblit.py    20 mutations
    attempted=20 caught=20 survived=0

Aimed at the LAW, not the copy: publish before writes retire, publish before
writes issue, publish with no byte check, publish with no CRC, unfinalised CRC,
CRC never accumulated, publish on a lost lease, publish while aborting, release
before writes retire, release a lease never owned, ownership taken before the
checks, aborted beats folded into the CRC, accept any length, blit with no
lease, ignore slot mismatch, ignore high slot bits, ignore a generation move,
either half of the guard verdict ignored, a bridge error forgotten.

Each leaves a working blit and an unsound machine, which is why a
happy-path-only differential would have been green through all twenty.

Advanced UNIT_VERIFIED -> RTL_VERIFIED. Justification against how this project
has used the rung: unit differential against the shipped oracle
`zref::debug::run_blit` **and** the composed path -- instantiated in
`zhao_shell_top` at `u_frameblit`, driven end to end by
`tests/shell/shell_golden.cpp`. Same shape as CMD.SCHEDULER's RTL_VERIFIED,
which cites a system-level demo over its own directed test.

Three lanes green including formal (27 assertions to depth 44, bmc + cover).

    tools/quartus/run_block_fit.ps1 -Module zhao_debug_frameblit
    zhao_debug_frameblit   ok   ALM 962 / 41,910   (2.3%)
                                registers      909
                                blockMemoryBits  0
                                DSPs             0

The block extracted from CMD.DMA precisely so a debug path could not stop the
shell fitting, and it is 2.3% of the device.

### rtlCleanAtHead now carries information

**`rtlCleanAtHead: true` for the first time in the file's history, on row 43.**

All 42 prior rows said false. A field meant to say whether a fit result can be
trusted against the commit it names was answering identically on clean and
dirty trees -- indistinguishable from not having it.

Cause, same class as this session's ctest finding: PowerShell resolves `git` to
whichever binary is first on PATH -- the msys2 one under devkitPro -- which
carries no `core.autocrlf`. A status through it calls every CRLF worktree file
modified: 29 RTL files, **279 insertions against 279 deletions on a 279-line
file**. Every line. Pure line-ending churn. Bash's mingw64 git, with
`autocrlf=true`, calls the same tree clean.

`run_composed_fit.ps1` already documented this exact failure and already forced
`-c core.autocrlf=true`. `run_block_fit.ps1` never inherited it. Fixed.

### Sequencer ALU-dispatch sweep -- does the new coverage discriminate?

    scratchpad/mut_seq_alu.py    11 mutations
    attempted=11 caught=10 survived=1

The opcode-coverage gate had just found SUB/MIN/MAX/ABS issued only by the
random pool and CMP by neither lane. A test written to close a coverage hole is
worth exactly what it discriminates, so this sweep asked whether the new
directed cases can tell a broken dispatch from a working one.

**Every caught mutation was caught by the DIRECTED lane, not only the random
one** -- including both immediate mutations, which is the path CMP's comparison
mode rides on. The sweep reports which lane did the work precisely so "caught
by the 400-program random lane" cannot be mistaken for "the directed case
covers it".

**ONE EQUIVALENT MUTANT, and it is genuinely equivalent.**
`exec_writes = unit_handled ? 1'b1 : alu_writes` -> `1'b1` survives.
`zhao_field_alu` clears `writes_o` in exactly two places: `OP_END`, which also
raises `is_end_o`, and the `default:` refusal, which also raises
`op_unsupported_o`. The write-back guard already carries
`!alu_is_end && !exec_unsupported`, and for an ALU op `exec_unsupported` IS
`alu_unsupported` -- so every case where `alu_writes` is 0 is excluded by a
different term of the same condition. Recorded in the RTL with an ENFORCED-BY
pointing at the source of the guarantee, so it does not read as a hole. The
expression stays because it says what the value MEANS and would stop working
the moment the ALU learns a third non-writing op.

---

## 2026-08-22 -- CMD.DMA staging buffer to block memory; the block fits

### State recovery

    git log --oneline -5          -> 995595f at session start
    ctest -L fast                 -> BAD_COMMAND on all 337 tests

**The suite was not broken.** `PATH` resolved `ctest` to
`c:\devkitPro\msys2\usr\bin\ctest.exe`, while the build tree was configured by
`C:/Programmieren/dsstuff/mingw64/bin/cmake.exe`. The msys2 ctest misreads the
Windows absolute paths in `CTestTestfile.cmake` and concatenates them onto the
working directory:

    Command: "/c/.../build/tests/C:/Programmieren/.../test_cmd_dma_directed.exe"

**The gate is `C:/Programmieren/dsstuff/mingw64/bin/ctest.exe`.** Using it,
baseline was 252/252. Recorded because "the whole suite fails" was one wrong
inference away from a day spent debugging nothing.

### Finding 1 — the formal lane was RED at HEAD (fixed, commit `b9d4101`)

    ctest -R formal_cmd_dma_crc_gate   -> bad state property 3 REACHABLE at k=12

Property 3 is `assert(fetched <= 32'd64)` under `m == M_HDR_CHK` — an assertion
I had added myself to justify bounding the CRC seed loop at 64, with the
comment "this is the whole reason the loop is safe".

It was not safe. The bound holds only if the bridge delivers exactly the beats
requested, and nothing in the RTL enforces that: `zhao_hps_bridge.sv` forwards
the external HPS's `last` straight through (`rsp.last <= hps_rd_last`) without
counting against `busy_len`. The formal harness leaves every response beat
free, so a bridge withholding `last` for nine beats drives `fetched` to 72.

Consequence was bounded — missed bytes fall outside the seed, the payload CRC
mismatches, the packet is rejected — so this was a **false assertion**, not an
exploitable hole. Still false.

Fixed by construction: both burst waits record `burst_end` when they issue and
leave on `last` **or** on having taken the whole burst. `M_PAY_WAIT` got the
same guard though no assertion covers it.

**Which lane enforces it — measured by mutating the guard back:**

| lane | verdict |
| --- | --- |
| `cmd_dma_directed` | passed |
| `cmd_dma_random` (400) | passed |
| `cmd_dma_random_nightly` (5,000) | passed |
| `formal_cmd_dma_crc_gate` | **caught it** |

The C++ bridge model is lawful by construction, so the differential is
structurally blind to this. ENFORCED-BY `tests/formal`, not the sim lanes.

### Finding 2 — the measurement that ended four wrong theories

`blockfit.map.rpt` from the run that synthesised cleanly:

    Estimate of Logic utilization (ALMs needed) : 83,977     device 41,910
    Combinational ALUT usage for logic          : 94,698
    Dedicated logic registers                   : 33,680
    Total block memory bits                     : 0

`Total block memory bits: 0` — no RAM inferred at all. 32,768 of the 33,680
registers were `slot_buf`. The block was a 4,096-entry **register file** with a
variable read address and a variable write address, which is why all three
mux-sharing attempts moved the number by ~0.02%.

Four theories, three 45-minute compiles, all spent reasoning about the source
while this report sat in `output_files` from the first run.

### The change (commit `f5e067e`)

The design note in `reports/REMAINING_BLOCKERS.md` claimed each CRC walk would
become a four-step read loop. **Wrong** — both walks and every header field
live inside bytes 0..63, and the rest of the payload CRC already streams from
the bus in `M_PAY_WAIT`. So:

* `hdr_win` — 64 bytes in registers. The **entire header ladder unchanged**: no
  extra states, no re-timed checks. 512 registers.
* `slot_ram` — 512 x 64b. No initialiser, written by a process with **no
  reset**, **one** registered read port, address muxed across the four states
  that read it (`M_PCRC_RD`, `M_WALK_RD`, `M_STREAM_RD`, and the stream's
  one-word lookahead).

Every multi-byte read fits in one word, so none costs a second access:
`command_bytes` and record lengths are multiples of 16, so `36+cb` and
`36+walk_off` are both 4 mod 8 — and the walk's **two** 16-bit fields land in
the **same** word, one read serving the pair. The `M_PCRC` split from the prior
session was the precondition, not a detour: it is what put the readers in
different states.

Cost: the walk takes two cycles per record instead of one; the stream fetches a
word every eight bytes with seven cycles of slack.

### Evidence

    tools/quartus/run_block_fit.ps1 -Module zhao_cmd_dma
    Quartus 17.0.2 Lite, 5CSEBA6U23I7

|  | before | after | device |
| --- | --- | --- | --- |
| ALMs (fitted) | 83,977 | **3,607** | 41,910 |
| registers | 33,680 | **1,571** | |
| block memory bits | 0 | **32,768** | |
| M10K blocks | 0 | **4** | 553 |
| DSPs | 0 | 0 | 112 |
| MLAB bits | — | 0 | |

"Fitter placement was successful" — a line this block had never produced.
Router estimated average interconnect usage **2%**, peak 28% in one region.
`quartus_map`'s log was 9.8–11 MB on every prior run and is **571 KB** now.

**Reproduced exactly across two independent runs** (1038.5 s and 1181.4 s,
both ALM 3607).

Recorded in `reports/synthesis/zhao_block_fit.json`.

### Mutation sweep — the new RAM seam

    scratchpad/mut_dma_ram.py    19 mutations
    attempted=19 caught=19 survived=0

Forced regeneration (`os.utime` +5 s) with a **generated-model** hash check
(not the linked binary, which is not reproducible); no result accepted without
the hash moving. Preflight caught one two-site mutation before any build.

Covered: wrong word address, dropped header offset, off-by-one word, inverted
half-word select, swapped opcode/length fields, every skipped read cycle, the
stream's lookahead and word crossing, the write-address shift, error beats
taken as data, a too-small header window.

**The restore guard fired** at the end — an mtime race left the model one build
stale. Source was pristine; a forced rebuild returned the model to baseline
hash `4d6eb35acf01daa3`. The guard exists for exactly this.

### Test gaps closed first (the stream rework is the delicate part)

1. `cmd_dma_random` (5,000 packets) **never checked the content** of what it
   streamed — only the verdict, byte count, and that broken packets produced
   nothing. Now compares bit-exact.
2. `pkt_ready_i` was **hardwired to 1** for the whole suite. Now stalls on a
   third of packets, so the word-boundary prefetch meets a consumer that pauses.

Verified with teeth: breaking the word crossing fails **60 of 1,542**.

### Process findings

* `[String]::Replace($old,$new,1)` — .NET has **no count overload**. The call
  threw, execution continued, "MUTANT APPLIED" printed, and ninja's "no work to
  do" was the only tell. An unapplied mutation reads exactly like a surviving
  one.
* Reverting a file to byte-identical content within the same timestamp
  granularity does **not** trigger a ninja rebuild. Forcing mtime is required,
  twice observed this session.

---

## 2026-08-22 — Field IR sequencer: opcode coverage gate

`zhao_field_seq` dispatches all **31** Field IR opcodes (15 in the ALU
including DOT2/DOT3, 16 in the units). Nothing enforced that the differential
had actually issued each one.

Added `opsIssued()` behind `Prog::op()` — the single funnel for every
instruction the test builds — plus a check that every required opcode was seen.

**It failed on its first run**, and the gaps were real:

| opcode | where it was |
| --- | --- |
| SUB, MIN, MAX, ABS | random pool **only** — the directed lane never touched them |
| CMP | **neither lane** — dispatched by the ALU, decoded by the reference, never once executed |

Closed with a directed section covering all five, including **all six CMP
comparison modes** at equal/less/greater and across sign boundaries, and CMP
added to the random pool with its `imm` mode selected in range.

    test_field_seq_directed            906 checks passed
    test_field_seq_directed --random 400   2506 checks passed
    coverage: all 31 required opcodes issued

`opsIssued()` legitimately exceeds the required set — some cases issue a
deliberately invalid opcode to prove the unsupported path. Noted in the code so
it does not read as a discrepancy.
