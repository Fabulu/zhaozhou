# RUN-20260821-1200 — the Field IR engine, the sequencer, and a gate that lied

Owner asks handled in this run: finish the Field IR engine and its sequencer,
fix the CI the owner reported broken, and file four content dumps into the
repo. One agent policy observed: **no subagents were used.**

Everything in this run is SIMULATION. Nothing has run on a board.

## State inherited

`main` at `0b8e43a`, with every Field IR *operation* built but no sequencer to
run them, and `DEBUG.FRAMEBLIT` step 4 parked on a branch pending an owner
decision (`reports/BLIT_INTEGRATION_PHASE_SHIFT.md`).

**CI was red on every push and had been for some time.** The local suite was
green at the same moment. Both readings were accurate, which is the part worth
recording.

## Work

### `f719dda` — `FIELD.SEQ.CORE`, the sequencer (block 92)

The register file and the instruction walk: zero the file, load the declared
input lanes, walk until `OP_END`, read the output lanes. 64x32 flops, three
combinational read ports walked over three states, six clocks per instruction.

Flops rather than M10K because a 64-entry file with three read ports does not
map onto a block RAM without duplicating it. One writer at a time: the walk
owns the file while busy, the host owns it otherwise, so there is no
arbitration to get wrong and no bypass network to need.

The block does not validate. `zfield::interpret` runs only on decoded programs
and its `default:` is `__builtin_unreachable()`; the decoder is the validator.
The one thing checked is not semantic — `instr_count_i` bounds the walk so a
mis-loaded instruction memory reports `ST_PC_OVERRUN` instead of hanging.

Evidence: 102 directed + ~1,850 random against `zfield::interpret` itself.
Mutation sweep **19 / 17 caught / 2 recorded equivalent / 0 discarded.**

The two equivalents are a redundant PAIR, not dead code: the ALU clears
`writes_o` for exactly `OP_END` and an unsupported op, so each guard is
redundant alone, while `both_write_guards_removed` is caught. That mutation
only became visible once a test named register 0 as an output — `OP_END`'s
`dst` is zero and no earlier program read register 0 back.

#### The defect it found in a block already signed off

`zhao_field_alu`'s `OP_ABS` returned `INT32_MIN` for `abs(INT32_MIN)`.
`spec/qformats.md` §3.7 is explicit: `abs(0x80000000) = 0x7FFFFFFF + SAT`.

The reason it survived weeks of testing is the point. The ALU's own test did
not ask the reference — it **restated** the rule, restated it the same wrong
way, and asserted the wrong value, under a comment warning that "the oracle and
the RTL could agree and both be wrong about what the reference does with this
one input". They did. The actual law was outvoted two to one.

Fixed, given the `sat_rescale_o` lane it was missing, and its test now asks the
shipped interpreter.

**A wrong restatement agrees with a wrong implementation forever.**

### `8264534` — the format gate that skipped locally and ran on CI

`format_check` needs clang-format, which oss-cad-suite does not ship. On a
machine without it the test reported SKIPPED and the suite went green; on the
runner it ran and went red. Thirty-five files had drifted behind that skip,
several from before this week.

**The fix is not to fail when the tool is missing — it is to make the tool
present.** `clang-format` is now a devDependency pinned to the same 1.8.0
(LLVM 15) the CI job installs, and `tests/CMakeLists.txt` searches that
`node_modules` path FIRST. `npm install` puts the authoritative binary on any
machine that builds this repo.

The version pin is why the pinned path is searched first. A system
clang-format from a different LLVM major reformats the same file differently,
so "clean locally" against an unpinned binary is not evidence about CI at all
— which is the real reason the skip existed.

Checked rather than assumed: with all whitespace removed, 33 of the 34 swept
files are byte-identical to their previous versions. The 34th
(`tests/measure/measure_tokens_binner.cpp`) differs only in the ORDER of four
`using` declarations, which is inert at namespace scope. `git diff -w` is NOT
sufficient evidence here — it reported 1,072 changed lines on a change that
altered no code, because reflowing moves tokens between lines.

CI job "format + static analysis (charter 27)": **green.**

### `0cc6c28` — the owner docket and three filed notes

`docs/OWNER_DOCKET.md` is new. It exists because the accumulated feature asks
lived in run QUEUE files and an agent's memory, so the only complete list was
outside the repo. Newest first; every entry is an ask, not a decision.

Today's entry: terrain rotated at arbitrary angles, and rotated in real time
("so a skyscraper suddenly falls over"). Static rotation is one format field —
islands carry translation only, and rotating the ISLAND rather than the patch
is what preserves the one-solid-interval column law. Real-time rotation leans
on `GEOM.LOOM`'s existing terrain-patch transform parenting, so the renderer is
not the new part. Four questions recorded for the owner rather than answered:
world-space column walking after a tilt, what the deep keel does when it stops
pointing down, the static-to-dynamic hand-off frame, and where the pivot comes
from.

Also filed: `reports/RASTER_Polygon_Budget_Proposal.md` (deferred behind
FRAMEBLIT steps 4-8 and the composed fit, per the owner),
`docs/CREATURE_ANIMATION_APPROACH.md`, `docs/PROJECT_ODDS_AND_SCOPE.md`.

### `fcf7e79` — this run log

### `1250b59` — the three notes, refiled from the full text

The owner re-sent the three content dumps and asked whether they had been put
where they needed to go. They had — but the first filing was made from a
TRUNCATED view, and the result was three faithful-sounding paraphrases with the
information removed.

Restored: the six-projects list and the whole odds table (70-90% vertical
slice, 50-70% FPGA demo, 40-60% sellable short game, 10-25% full campaign,
single digits for all of it plus a physical console); every cost figure in the
raster proposal (92,160 pixels at 240p, 4.5 visible pixels per authored
triangle, 5+16+0-16 = 21-37 clocks per triangle x tile job, a 128-triangle /
1,024-reference binner at 2.83 clocks per reference, a TMU at one sample per
4 and 6 clocks), plus its authoring-tier table, its 53-creature battle budget
and its nine-step order; and in the animation note the owner's own two-modes
framing, the six hardware-lowering options, and the "armies share states"
insight.

**A paraphrase that sounds right is not the thing it paraphrases.** Dropping
the numbers dropped the content, and the same lesson as the `abs` defect
applies: a restatement can be wrong in ways that read as correct.

### `290814a` — D5's 338k figure marked superseded

`reports/status/phase2_wave2.md` declares 60 Hz full-canvas cadence infeasible
and builds it on a measured ~338k gpu cycles per Duo blit. The FRAMEBLIT
redesign streams fetch and commit instead of serialising them and measures ~58k
cheaper, so that headline is no longer the machine's number.

The CONCLUSION is unchanged and says so: the commit phase still dominates
against a 318,592-cycle frame, and the Z60 raw-demand bullet is a bandwidth
proof no blit redesign touches. The marker does not pre-empt the open decision
in `reports/BLIT_INTEGRATION_PHASE_SHIFT.md`; it exists so nobody reads 338k as
current while that decision waits.

### `d9a48ea` (zhaozhou-site) — update.ps1 passes --branch

`deploy.ps1` was fixed for this and `update.ps1` was not, leaving the more
dangerous path broken: update.ps1 is the script that regenerates every render,
so a full refresh through it landed as a Cloudflare PREVIEW while the public
URL kept serving the old page. Wrangler exits 0 and prints "Deployment
complete" either way.

The site directory was not a git repository when the bug first bit, which is
why wrangler had nothing to infer. It is one now (2026-08-20) and sits on
`main` — making the inference correct today and silently wrong the first time
anyone works on a branch. deploy.ps1's comment still asserts the old fact; the
new note says so rather than repeating it.

## DEBUG.FRAMEBLIT integration, steps 4-6 (the DSP path)

The owner asked why the DSP overrun was still sitting there. The honest answer
was that the path to it ran through FRAMEBLIT, and I had been gating on a
decision plus a blocker that turned out to be stale.

### The decision, taken

`reports/BLIT_INTEGRATION_PHASE_SHIFT.md`: **option 1, take the speedup.** The
deciding fact was **one frame less input latency** — content that used to
appear at frame N+1 appears at frame N, roughly 16.7 ms at the 60 Hz field
rate — plus ~58k gpu cycles of freed budget. It does NOT reach 60 Hz; the
commit phase still dominates a 318,592-cycle frame.

### Step 4: rebased, not merged

The branch was **13 commits behind main**, so `git diff main branch` showed
main's newer files as deletions on the branch side. Merging it would have
deleted the entire Field IR engine. Rebased forward instead; the branch then
differed from main only in `zhao_shell_top.sv` and two CMake lines.

### Step 5/7: the laws re-derived, and a test that was hiding a defect

41 of 340 checks failed and not one was a wrong pixel. Every `got` was the NEXT
expected value.

**The structural fix matters more than the three closed forms.** The CRC check
compared `h.crcs[i] == expect_crc[i]`, welding WHICH PICTURE to WHICH FRAME IT
LANDED ON. That is what turned a latency win into nine correctness failures,
and the obvious way to make it green was to revert the win — which charter
§25 now forbids. Split into the two claims it conflated: every displayed frame
is either a repeat of the one before it or the next distinct picture IN ORDER
(exact, true at any cadence), while the cadence is pinned separately.

That rewrite exposed a real defect: the old drain loop was bounded by
`crc_checked < expect_crc.size()` and **silently stopped checking partway
through**. The same 40-frame run went from 340 checks to 1,586.

The three forms, each derived and then tested at frame counts they were not
derived from (8, 20, 60; only 40 was used):

- the half-rate cadence **inverted** — repeats move from ticks 1,2,4,6... to
  tick 0 and every odd tick. Two boundary cases were found by MEASURING which
  ticks disagreed rather than guessing, and I would have got both wrong: tick 2
  is FRESH, and the final tick 2F+2 repeats because after the last publish
  there is nothing further to show;
- `deadline_faults` is `(k+1)/2`, was `1 + k/2`. They agree at every odd tick
  and differ at every even one, which is why exactly half fired;
- **every fence now closes CLEAN except fence 0.** The old law said every fence
  is STATUS_DEADLINE because a full-canvas blit cannot finish inside one frame
  period. In steady state that is now false, and that is the headline result.

**The golden did not move.** Predicted algebraically first — the content CRCs
are unchanged and `(2F+2)/2 == 1 + (2F+1)/2 == F+1` — then confirmed: the
600-frame gate passed **23,430 checks** with the golden MD5 byte-identical
(`db078abb...`) before and after. Taking the speedup cost zero golden churn.

### Step 6: the 1.97 Mbit buffer is gone

`zhao_cmd_dma.sv`, **874 -> 633 lines**. Removed: `blit_buf` (1,966,080 bits),
the five `M_BLIT_*` states, the ENTIRE MEM.GUARD client (this block no longer
writes VRAM at all), the `FORMAL_BLIT_LEN` override, the orphaned
`ST_BLIT_REJECT`, and the shell's `_dead` tie-offs.

That buffer is why the composed fit could not complete: it never inferred as an
M10K — async-reset write, combinational read — so Quartus reported **Error
276003**. It does not need a registered read now; it does not exist. It is also
the module whose elaboration ALONE peaked at **16.2 GB** against 0.26 GB for
ordinary blocks, which is a better suspect for the slow composed elaborate than
the virtual-pin wildcard was.

The contract's own deferred question — *"whether a 1.97 Mbit on-chip buffer
should exist at all"* — is answered.

Formal property (b) went with it, recorded rather than dropped: it said no VRAM
write is offered before the blit CRC passed, and it was **VACUOUS** until the
harness gained `FORMAL_BLIT_LEN`, because the smallest lawful canvas is
153,600 B and no tractable BMC depth could open the gate. The property that had
to be rescued from vacuity is the one now deleted.

## Three documents were quoting a blocker that had been fixed

`reports/composed/README.md`, `design/budgets/dsp.md` and
`tools/quartus/run_composed_fit.ps1` all said the composed fit needs a machine
with more than 24 GB, citing `quartus_map` committing 28.4 GB. **I repeated that
to the owner as a current constraint. The owner remembered it had been fixed
and was right.**

It was a wildcard virtual-pin assignment matched against every node name in the
design (`d1a2b8a`); with the 101 ports named, the composed run completes in
42:33 at **6.2 GB** (`f3506b6`), whose own message says the work-PC handoff
"is very likely unnecessary now".

The script's staleness was not cosmetic: it exists to hand the job to a second
machine that was never needed, its RAM warning tripped below 30 GB, and
`$Processors` was defaulted to 1 purely to cut a peak that had already gone.

And 6.2 GB is not settled either. `9c693a9` measured that parsing the whole
source cone is free (0.24 GB) and the cost is in ELABORATION, superlinear for a
top of sixteen ordinary instances with no generate blocks and no large arrays
— *"there is nothing pathological in the design."* A newer Quartus is the
named lever nobody has pulled.

`9c693a9` had ALREADY warned this figure "was believed long enough to shape
decisions". The warning was in the repo and I did not read it before repeating
the mistake.

## The verification harness lied twice, in two new ways

Recorded because the sweep is the evidence every RTL claim in this run rests
on, and it was weaker than it looked.

**1. A failed apply was counted as a result.** Python on Windows printed the
mutation names with CRLF; command substitution strips only the trailing
newline, so 23 of 24 names reached `apply` with a `
` attached and raised
KeyError. The script printed APPLY-FAILED, **continued**, and reported
`caught=0 survived=1 discarded=0` with exit code 0.

An apply that fails is not "survived" and not "caught". It is no evidence, and
counting it as an outcome makes the total a lie. The sweep now aborts on a
failed apply, and cross-checks `attempted == expected == accounted` before it
will report at all.

**2. THE DISCARD GUARD COULD NEVER FIRE.** This is the more serious one. The
guard exists to catch Verilator serving a cached model instead of re-elaborating
the mutated RTL, and it did that by checking the test BINARY's hash changed.

Two builds from byte-identical source produced `440d4a24...` and `8b57af01...`
— **the linked binary is not reproducible.** So "hash differs from pristine"
was unconditionally true and the discard branch was unreachable. Every
"0 discarded" reported in this run before now, including on the sequencer
commit `f719dda`, rested on a check that could not fail.

The generated Verilator model IS reproducible: the same two builds both
produced `5972223c...`. It is a pure function of the RTL, which is the property
the guard needed. The guard now hashes all 13 generated model sources, and was
tested in both directions before being trusted — mutate without rebuilding and
the hash is identical (discard fires); mutate and rebuild and it differs.

**3. A preflight, because the abort came too late.** The hardened sweep aborted
correctly on an ambiguous revert — after 13 mutations and forty minutes of
rebuilds. Every mutation is now proved to apply once, change something, and
revert byte-identically before any build runs. It is a two-second check.

## The shell had two source lists, and only one was updated

Found by accident. Re-measuring `CMD.DMA` after step 6, Quartus failed in 15
seconds with:

    Error (10703): zhao_shell_top.sv(864): can't resolve aggregate expression
    in connection to port 20 on instance "u_slotmgr"

Step 4 wired `zhao_debug_frameblit`, `zhao_hps_arbiter` and
`zhao_video_slotmgr` into the shell and into `tests/CMakeLists.txt` — the list
VERILATOR reads. It did not add them to `zhao_shell_fit.qsf`, the list QUARTUS
reads. **The suite was green for hours while Quartus could not elaborate the
shell at all**, and it would have failed the composed fit outright.

**A CORRECTION TO THE STEP 6 COMMIT.** It called the buffer removal "the
composed-fit unblock". That was INCOMPLETE rather than wrong: the buffer really
did cause Error 276003, but this second blocker sat underneath it.

Two statements of one fact with nothing checking they agree — the `OP_ABS`
shape again, except here the two statements are TOOLCHAINS, which is exactly
why a green Verilator suite proved nothing about the synthesis lane. So the fix
is not only the three missing lines: `tests/lint/source_list_parity` asserts the
two lists name the same modules, needs no external tool so it cannot skip, and
was verified in BOTH directions — it passes now, and deleting
`zhao_video_slotmgr` from the QSF makes it fail naming that exact module.

## The first DSP cut, and a comment of mine that was wrong

`zhao_raster_blend` formed BOTH `(src-dst)*a` and `src*a` unconditionally in two
`always_comb` blocks, though `mode_i` consumes at most one. Two DSP per channel,
three channels, six of `RASTER.FRAGMENT`'s ten. Now one selected signed product.

Method, in order: the oracle first (`zref::FragmentPipeline::blend_channel`,
whose ADD_MOD path uses "the FROZEN unit8 multiply, not a local copy"); then the
RTL; then the differential — `raster_fragment_random` exercised all four modes,
REPLACE/ALPHA/ADD/ADD_MOD = 6862/5457/6676/5563, **24,558 blend operations
bit-identical**; then the sweep.

**Sweep: 11 mutations, 10 caught, 1 SURVIVED, 0 discarded**, attempted =
expected = accounted.

The survivor was `logical_shift` — the mutation I had predicted was the
critical one, because I had written that the signed arithmetic shift was
load-bearing. **It is not, and the comment was mine.** `>>` and `>>>` differ
only above bit 9, and both consumers truncate below it: ALPHA takes
`mixed[9:0]`, ADD_MOD takes `mixed[7:0]`. For a negative sum the logical shift
gives `A + 1024`, and `(A + 1024) mod 1024 == A mod 1024`. Zero observable
differences across all 130,816 ALPHA pairs, 64,380 of them with a negative sum.
**EQUIVALENT MUTANT**, recorded in the RTL rather than left looking like a hole.

What IS load-bearing is the ORIGINAL comment's claim, about a different
transformation: the `+128` must be applied to the SIGNED product so ties round
toward +infinity. Splitting the sign off and rescaling the magnitude unsigned
differs by one LSB on **1,024 of 130,816** pairs — measured, not asserted. The
file now says which fact carries the weight and which does not.

The corrected comment was proved inert: the generated model hashes identical to
the sweep's baseline, so 10/1/0 describes the committed design.

## CMD.DMA: three measurements, two of my conclusions wrong

The block gating FRAMEBLIT step 8. It had NEVER been successfully processed.

| attempt | result |
| --- | --- |
| census (`96c0394`, with `blit_buf`) | `failed:quartus_map`, 16.2 GB elaboration |
| HEAD after step 6 | `timeout`, 4,838 s |
| HEAD + bounded CRC loop | **synthesis 0 errors**, then `failed:quartus_fit` |
| + `slot_buf` as 512x64 words | **worse**: 109,350 nodes vs 95,328 |

**1. The CRC cone.** I wrote into `REMAINING_BLOCKERS.md` that this needed the
payload CRC seed accumulated incrementally across cycles — a state-machine
redesign. **It needed a bound check.** The loop is written `k < 192` and the
reachable maximum is 64: `fetched` is zeroed at accept, `M_HDR_REQ` issues
exactly ONE burst, `burst_len` caps at 64, `M_HDR_WAIT` adds 8 per beat and
leaves on `last`. Iterations 64..191 had their guard false in every reachable
state — 128 steps of unreachable logic that synthesis built a ~1,248-stage
dependent chain for before discarding.

"156 dependent CRC steps" was measured and true. "Therefore it needs a rewrite"
was inferred and false. **A combinational cone that large is worth a bound
check before it is worth a redesign.**

**2. The word re-description.** Reasoning: aligned 8-byte groups mean one word
per write, so the 4,096-way decoder disappears. Predicted a large reduction;
measured **+14,022 nodes in the wrong direction**. The change was sound and
bit-identical — `cmd_random`'s transcript hash unchanged at
`0xb95b5f70a413bdbd` across 1,000 frames — and was reverted anyway, because
**bit-identical is not the same as better**.

Why it grew is NOT recorded, deliberately. Two inferences about this block were
already wrong; a third guess written as fact would be the pattern.

**What the measurements establish:** a re-description does not fix this, only a
real memory does. The remedy has been in the RTL from the start — registered
read, one-cycle beat-stream lead, initialiser dropped so it can infer as RAM,
the cure `zhao_scanout_linebuf` got via `zhao_dc_sdp_ram`. **Three memories,
one defect**, and `slot_buf` is the last.

**3. A second blocker I had not found.** Step 4 wired three modules into the
shell and into the CMake list — the one VERILATOR reads — and not into the
QSF, the one QUARTUS reads. The suite was green for hours while Quartus could
not elaborate the shell at all. So the step 6 commit's claim to be "the
composed-fit unblock" was INCOMPLETE, not merely optimistic.
`tests/lint/source_list_parity` now asserts the two lists agree, needs no
external tool so it cannot skip, and was verified in both directions.

## Field IR dispatch: 3 ops to 13

The sequencer executed three opcodes at the start of this session and executes
thirteen now.

| | ops |
| --- | --- |
| single-cycle | RCP, SIN, COS |
| multi-cycle, 1 lane | LEN2, LEN3, DIST2, RIDGE, RING |
| 2 lanes | NORMALIZE2, NOISE2, ROT2 |
| 3 lanes | NORMALIZE3, ROT3 |

New machinery: `Q_MISS` holds `v_valid` until the unit takes the operands,
`Q_MWAIT` holds `r_ready` until it answers, `Q_WB1`/`Q_WB2` walk the second and
third output lanes because the file has ONE write port. The state encoding went
3 bits to 4 — all eight codes were already used.

**THE SWEEPS DROVE THE DESIGN THREE TIMES.**

*First:* 16 mutations, 7 survived. Not equivalences — with only LEN in the
group, `multi_op` WAS `op_is_len` and `multi_width` was always 1, so a mutation
ignoring the opcode was indistinguishable BY CONSTRUCTION. The machinery could
not be verified until a multi-lane op used it. Adding NORMALIZE killed two.

*Second:* `slow_ledger_not_accumulated` survived because `diff()` compares the
COLLAPSED `Status.sat`, so a cleared `add` lane was masked by `rescale` on the
same op. Added a per-lane check and proved it in both directions.

*Third:* two mutations survived where RIDGE and RING declared a width of two.
I had written the "lane the op does not own must not move" guard for the WIDE
ops at `dst+2` and never for the NARROW ones at `dst+1`. **A systematic blind
spot in a whole class**, found by the sweep and closed for RIDGE, RING and
LEN2.

**Four recorded equivalences, with the condition rather than the conclusion.**
The handshake mutations survive because the seam is never exercised: the
sequencer drains each op before issuing the next, so `v_ready` is always high
when `Q_MISS` asks, and `zhao_field_len` carries a pipeline stage tolerating an
early valid. I predicted one would HANG, tested it, and was wrong. The RTL
records what makes them equivalent and what would end it — a stalling unit, or
a sequencer that pipelines.

**Operand mappings came from the oracle, not the port names.** RIDGE takes its
second lane from `reg[b]`, NOT `reg[a+1]`, unlike NOISE2 which shares its unit.
Three mutations aim at that one.

The refusal test has now moved three times — RCP, then ROT3, then CURVE — each
time because wiring an op made it fail. CURVE will outlast the others: it is
the only family needing a table port. The permanent half is the `0xFE` case,
which never needs maintenance.

**A stale build bit me during a manual check**: a rebuild without clearing the
model directory left a reverted design still reporting the mutant's failure —
exactly the Verilator caching trap the sweeps guard against with forced
regeneration, in the one place those guards were not running. Caught by
comparing the RTL against the pristine copy before believing the result.

## Limitations

- Everything is Verilator simulation. No synthesis, no fit, no board.
- Sequencer dispatch reaches the arithmetic core only at `f719dda`; the five
  `FIELD.SEQ.*` profile blocks stay SPECIFIED until the remaining op blocks are
  wired to the walk.
- `DEBUG.FRAMEBLIT` steps 5-8 remain blocked on the owner's decision in
  `reports/BLIT_INTEGRATION_PHASE_SHIFT.md`.
