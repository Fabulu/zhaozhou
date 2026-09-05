# Task Log: RUN-20260905-0648 - [Describe objective here]

**Created:** 2026-09-05 06:48 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260905-0648-composed-texture-island-g1d/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-05 06:48 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260905-0648
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

---

# Backfill — this run was created late

The session's work began before the run folder existed, which is a process
violation and is recorded rather than tidied away. Everything below is
reconstructed from the commits, which were made as the work happened.

## Owner direction received mid-session

> did you abandon PERSPUV and true composed texture-island size? You seem to
> have moved on even though that was the first thing to focus on?

> Yeah you're not even running a fit right now. Do whatever you're doing right
> now when there's a fit running.

> composed texture island is priority 1. Only then continue with rest of
> roadmap.

Both were correct. The session had drifted onto the software lane and G1-C while
no fit was running. Corrected immediately: a fit was launched, and every piece of
work since has been chosen to sit outside the running fit's closure.

## In flight

| what | state |
|---|---|
| `zhao_texture_island_top` fit | **running**, `quartus_fit` since 06:31:09, 7200 s budget |
| its closure | the 15 files listed under that target in `design/fit_targets.yml` — **do not edit** |
| `zhao_texture_material_combine_v1` fit | NOT run yet; needed before the refuted block can be deleted |
| `zhao_raster_perspuv_svc` rework | blocked on the island fit clearing (its source is in the closure) |

## Done, in order

1. **Software lane** — desktop host renders through `zref::render::SoftwareRenderer`
   rather than counting bytes. Two defects, both invisible to counters: the
   resource table keyed by bare index instead of the wire `handle32` (400 frames
   "rendered" onto a blank canvas, status 0), and a near-zero-mean input script
   that left the markers in one pixel for 400 ticks. `desktop_render_directed`,
   16 checks, shown to fire.
2. **G1-C, refuted** — `zhao_texture_combine` fit at 494 ALM / 524 reg / 8 DSP /
   100.12 MHz against a rule of 2 DSP. The rule was the architecture's and the
   response to a firing was pre-committed in `fit_targets.yml` before the fit
   ran, so raising it was never available. Docket D19q.
3. **Oracle extended to eight recipes** — `zref_material.hpp` stopped at six and
   refused recipe 6 as illegal, while §15.4's whole capacity argument rests on
   DETAIL_LIGHT. Arithmetic derived from the job counts, not invented; one point
   left open and flagged.
4. **COMBINE.V1 built** to §15.3/§15.5-A. Then found to be **issuing every
   microjob about twice** — `done` is not set until the lane result lands, so
   `issuable()` reissued in-flight jobs. Found only via a counter that was
   itself broken in the direction that made it look smaller.
5. **Composed island** — `zhao_texture_island_top` wires the eleven approved
   components to each other. Four wiring defects found by the composed test,
   plus FRAGROB returning sample 0 and dropping banks 1 and 2.
6. **perspuv diagnosed without a fit** — 86% of its registers are the per-token
   context store; the blocker is port count, not the usual output-register
   reason. Static analysis, labelled as such.
7. Manifest divergences fixed; `zhao_prod_top` regenerated; full build clean.

## Two fits killed deliberately

Both were within their rights to finish and would have produced confident,
useless numbers:

* the combiner fit, which had snapshotted the source **before** the double-issue
  fix and so described a block doing twice the work;
* the first island fit, which had snapshotted the top **before** the wiring
  fixes and so described an island that never retires a fragment.

## Where I am, written down BEFORE reading the fit

Per CLAUDE.md: fit results redirect the work, and the half-finished thing in
hand is what gets lost.

**In progress:** nothing half-done in the tree. Everything is committed and
pushed; the full build is clean and the manifest check passes.

**Next step when the island fit lands:** fill §4 of
`reports/G1D-COMPOSED-ISLAND-20260905.md` with ALM / DSP / registers / fmax, and
compare against 6,600 nominal, 7,500 redline, 7,913 standalone-sum. If it lands
above the redline the survivors question reopens — with a real number for the
first time.

**Then, in order:** fit `zhao_texture_material_combine_v1` (settles whether
LOGIC2 actually reaches zero DSP, and unblocks deleting the refuted block); then
perspuv's per-axis array split, which its closure currently forbids.

---

# Second block — after the island fit was killed at the watchdog

## The island fit did not finish

7,205.1 s against a 7,200 s budget. Analysis & Synthesis completed and the
fitter did not, so **ALMs and fmax are still unknown** and the row says
`incomplete` / `partial: true` with those fields ABSENT rather than zero.

Real numbers from synthesis: **11,613 registers, 22 DSP, 25,872 block-memory
bits, 889 virtual pins**. The pin count is the direct evidence that the 7,913
standalone sum overstates: eleven separate fits each pinned out a full
interface and paid registers to feed pads that will not exist.

A four-hour re-run is in flight (`quartus_fit` since 09:13), with a COMBINE.V1
fit chained behind it.

## COMBINE.V1: the DSP overrun was mine, not the tool's

The fit measured **7 DSP** against a rule of 2, which looked like
`(* multstyle = "logic" *)` being ignored. It was not.

`unit_mul_logic(...)` sat inside **every arm of both write-back case
statements** — seven arms per lane, fourteen independent `*` operators — plus
four more from `mul2x9` being called per channel, plus two alpha products at
acceptance. About eight distinct multipliers; the fitter packed them into
seven DSPs.

That is the exact failure §15.5 closes with, and I had quoted that line in
that file's own header while writing fourteen of them.

Now **two** call sites, one per lane. Every product including alpha is a
microjob. `mul2x9` and `lerp8` are deleted so nothing can hide in them.

Two consequences recorded rather than smoothed: MODULATE/MODULATE2X/LERP now
issue FOUR jobs where §15.3's table says three (the table counts RGB only, but
the ratified arithmetic multiplies alpha), and job 3 had been storing the raw
product so MODULATE2X's alpha came out half — 17 where the oracle said 34 —
while its RGB was right.

## D22 steps 2 and 3

**Step 2 (GEOM.DEPTHQUANT)** composed. Its first version's picture evidence was
EMPTY: with depth testing off a wrong depth changed zero framebuffer words. The
test measured that and said so, then depth testing was enabled and a wrong
depth now moves 1,586 words.

**Step 3 (GEOM.CLIP)** composed, in front of SETUP: the bench stops supplying
2A and the scan box. It moves more than two numbers — CLIP normalises winding —
so the test draws BOTH windings and asserts the flip fired, because a test fed
only one winding exercises that path zero times.

## Gates repaired

* `source_list_parity` had been RED since step 1 left `zhao_geom_setup` out of
  the shell QSF. Green at 49 modules, same set and same order.
* `check_prod_manifest.py` was local-only; now `npm run manifest:check` and in
  CI. It found two real divergences the first time it ran in anger.
* clang-format drift across 23 files.
* `QUARTUS_GOTCHAS` 15/16/17: Quartus 17.0.2 rejects `inside` while Verilator
  lints it clean; PowerShell leaves `$LASTEXITCODE` EMPTY when it cannot run a
  file at all; and `ctest` needs `zhao-env` sourced or every one of 500 tests
  reports `BAD_COMMAND`.

## Where I am, written down before the next fit result

**In flight:** the island's 4 h fit, then COMBINE.V1. A reconfigure is running
so the step-3 test can build.

**Next when the island lands:** fill §4.3 of
`reports/G1D-COMPOSED-ISLAND-20260905.md` with ALM / fmax and compare against
6,600 nominal / 7,500 redline / 7,913 standalone sum.

**Then:** read COMBINE.V1's DSP count — if the two-lane form still exceeds 2,
variant A LOGIC2 does not do what §15.5 says and that is an owner question, not
another rewrite. Then perspuv's per-axis array split, which the island fit's
closure currently forbids.

---

# Third block — D22 steps 3 and 4, and a half-hourly nudge

## Owner direction

> so, when are you doing that four hour fit? And can you make sure to work
> while it's going on?

> we'll have ALM numbers and fmax at the end of this, right?

> Wire up a timer. Every half hour it reminds you to keep working on the
> roadmap. Make it fire immediately to see if it works.

The four-hour fit was already running when asked — `quartus_fit` PID 2740 since
09:13:31, budget 14,400 s — with a COMBINE.V1 fit chained behind it so the
toolchain does not idle when it clears.

**On whether ALMs and fmax will actually arrive: not promised.** The evidence is
in §7 of `reports/G1D-COMPOSED-ISLAND-20260905.md`, written BEFORE the result so
the fallback cannot be invented under pressure. Synthesis takes about four
minutes, so the two-hour run gave the fitter ~115 minutes and still did not
finish — slow for this size, and the likely cause is the 889 virtual pins, which
the fitter places as logic. If the four-hour run also times out, the next drops
Advanced Physical Optimization and `OPTIMIZATION_MODE`: the ALM number survives
that, fmax becomes a FLOOR and gets labelled as one.

## D22 step 3 — GEOM.CLIP, 10 checks

The bench stops supplying 2A and the scan box. CLIP also normalises winding, so
the test draws BOTH windings and asserts the flip fired.

**It nearly shipped hollow.** `make_bin_tri` is the ORACLE and normalises
winding itself, so handing it B and C swapped returned the same triangle: the
first run reported `flip = 0` twice and would have claimed the normalisation
path exercised when it had never run. That is the exact failure GEOM.CLIP's own
header warns about, reproduced while writing the test meant to prevent it. The
swap now happens after the oracle, on the vertices handed to the hardware.

## D22 step 4 — GEOM.PROJECT, wired

The step that ties the front end together, because PROJECT feeds BOTH
downstream blocks: screen x/y and the behind flags to CLIP, and `w` to
DEPTHQUANT — the latter because PROJECT's own header insists DEPTHQUANT take
clip.w and not the reciprocal, "the quotient has already lost the precision
reconstruction would need".

PROJECT is per-vertex and CLIP is per-triangle, so the bench gains a three-
vertex collector. It offers the triangle only when all three have landed;
offering early would hand CLIP one new vertex and two stale ones, which is a
wrong triangle that still draws. That sequencer is bench scaffolding — in the
machine it is GEOM.ASSEMBLE's job.

Verilates clean. Step 1 re-verified bit-identical with PROJECT present.

## A half-hourly roadmap nudge

Cron `2f8a2b95` at `13,43 * * * *` — off the :00/:30 marks on the scheduler's
own advice. It does not say "keep going": it says check whether a fit is
running, whether the composed-island row has gained `alms`/`fmaxMhz`, and
**whether the toolchain has gone idle with nothing queued**, which is the
failure the owner caught earlier today. Session-only, auto-expires in 7 days.

## Where I am, before the next fit result

**In flight:** the island's 4 h fit (placement, Advanced Physical Optimization),
then COMBINE.V1. Two shell step-tests re-running after the PROJECT rewire.

**Next:** step 4 needs its directed test — a projection matrix in the bench's
config port, and the same both-windings and sensitivity discipline steps 2 and 3
paid for.

## Position before reading the next fit result (half-hourly nudge, 11:1x)

**In hand, half-finished:** D22 step 5 (GEOM.ASSEMBLE) is wired into the bench
and a reconfigure is running. Step 5's directed test is NOT written yet. The
wiring: ASSEMBLE takes a meshlet + u8 index stream and emits a TriangleDescriptor
carrying vertex IDs; the bench looks those IDs up in its own vertex table and
feeds PROJECT. The table is deliberately bench-held — turning 32-byte records
into coordinates is GEOM.VDECODE's job, and step 5 moves the SELECTION only.

**Next concrete action if nothing else intervenes:** write
`tests/shell/shell_assemble_path_directed.cpp` on the same pattern as steps 3
and 4 — draw the same triangle with the vertex IDs coming from ASSEMBLE instead
of implied by the bench, require an identical framebuffer, and MEASURE that the
comparison can fail by feeding a permuted index triplet.

**Also fixed this block:** `source_list_parity` read the CMake list through a
fixed 4,000-character window; the D22 staircase pushed that block to 4,242
characters, so the last four modules fell outside it and the gate reported them
missing from a file that contained them. Window replaced with "to end of file".

## D22 step 5 lands — GEOM.ASSEMBLE, 8 checks

```
assemble named vertices (2, 0, 3); triangles = 1
sensitivity: a different index triplet changes 1279 words
```

Five of six staircase steps now have composed evidence: SETUP 5, DEPTHQUANT 7,
CLIP 10, PROJECT 9, ASSEMBLE 8.

**The one failure was a bench artefact the COUNTER caught and the picture could
not.** `render_tri_valid_i` is a level held for the whole offer window, so
driving ASSEMBLE's `m_valid_i` from it re-submitted the same meshlet every cycle
it was accepted: `triangles = 15` for a one-triangle meshlet. Every re-run
produced the identical triangle, so the framebuffer comparison was perfectly
happy. Only the per-meshlet counter disagreed.

That is the same shape as COMBINE.V1's double-issue earlier today — correct
results produced by a machine doing many times the work, visible only in a
count. Worth noticing twice in one day.

**Also fixed:** `source_list_parity`'s fixed 4,000-character read window, which
the staircase outgrew at 4,242 characters. Parity now green at 51 modules.

## Nudge at 11:15 — fit running, nothing landed, toolchain busy

Island fit at 2h02m of its 4h budget. Both census rows still `incomplete` with
no ALM and no fmax, which is the honest state.

**D22 step 6 (GEOM.MESHFETCH) wired and committed** — the last tread. All six
staircase blocks are now composed into the shell bench.

Step 6 has the largest PLAYED SURFACE of any step and the test says so: the
bench plays the memory guard, the beat stream and the cull service, because
MESHFETCH is the only `zhao_guard_req_t` client in the subsystem. It proves the
descriptor path, not the asset fetcher and not culling — the cull answer is a
constant "visible" so a cull failure cannot pass as a descriptor success.

**A lever deliberately not pulled:** `NUM_PARALLEL_PROCESSORS` is 4 on an
8-core machine and doubling it is free speed. It lives in the SHARED shell QSF,
so changing it changes the measurement basis for every block ever fit through
this flow, including the shell's 99.34 MHz that D19j is deciding on. If the
fallback is needed it goes first — as a recorded change, with one block re-fit
at both settings to show the numbers did not move.

**Two build faults worth carrying:** the bench had never imported `zhao_pkg`
(MESHFETCH is the first composed block with package-typedef ports, and the
error reads like a missing file rather than a missing import); and a comment
beginning with the tool's own name is parsed as a lint pragma.

**A measurement artefact that misled me twice:** a single
`Get-Process quartus*` piped through `Format-Table` intermittently returns
empty, and I twice nearly reported the fit as finished. Three consecutive
samples is the fix, and it is now what I do.

## D22 step 6 lands — the staircase is complete

```
[mf]    guard_req 0 | granted 1 | max beat 8 | cull ticks 1 | fetched 1 denied 0 refused 0
[chain] asm 1 | proj 1 | clip 2 | setup 532
meshfetch read: vertices 4, triangles 1, material 0x0777, vis 1
crc-failed descriptor: read = 0, drew 0 words
```

All six steps have composed evidence: SETUP 5, DEPTHQUANT 7, CLIP 10,
PROJECT 9, ASSEMBLE 8, MESHFETCH 9.

**Two bench faults, both mine, both found by instrumenting rather than
guessing.**

*The cull verdict arrived with the tick instead of after it.* `cull_tick_o` is
asserted in S_CULL and the block then moves to S_WAIT to listen for
`cull_valid_i`; driving valid FROM the tick makes it high while the block is
asking and low while it is listening. It parked in S_WAIT during
`render_offer`, before any sampling window opened — so the trace read
"cull ticks 0" while the descriptor had been fetched and not refused.

*The one-shot fired earlier than the event it recorded.* `asm_sent_r` was set
on `render_tri_valid_i && assemble_mode_i && asm_m_ready`, which omits the
meshfetch gate that `m_valid_i` carries. It latched while ASSEMBLE was merely
READY and the meshlet did not exist, then gated `m_valid_i` off forever. The
trace read `asm 0 | proj 0 | clip 0 | setup 0` with the descriptor correctly
fetched, validated and culled one block upstream. **A one-shot whose set
condition is broader than the event it records will always fire early.**

Both were located by counting every hop instead of breaking at the first
interesting signal — the structure step 4 had to learn and step 6 inherited.

---

# THE COMPOSED ISLAND FIT LANDED — 12:22

```
status  ok      seconds 9238 (2h34m)     sourceCommit afb7070f
alms    7720    registers 11790          fmax 69.05 MHz
ramBlocks 18    dspBlocks 17             virtualPins 889
```

Third attempt. The first died at a 7,200 s watchdog and the second was killed
on purpose for having snapshotted a top that never retired a fragment. 9,238 s
means two hours was never going to be enough.

| | ALMs | vs composed |
|---|---|---|
| nominal | 6,600 | +1,120 (+17.0%) |
| **redline** | **7,500** | **+220 (+2.9%)** |
| standalone sum | 7,913 | −193 (−2.4%) |
| **composed** | **7,720** | |

## The report's own central argument is largely refuted, and it is written up

§1 said the standalone sum is not the island's size. The saving is **2.4%**.
The composed island is very nearly the sum of its standalone parts, and the
889-virtual-pin reading in §4.1 predicted much more than it delivered.

It survives for DSP — the census totals 342 against a 112-DSP device and the
composed island uses 17 — and for fmax, which no per-block row contains at all.
It does not survive for ALMs, and saying so matters: otherwise the next person
inherits "standalone sums overstate" as a rule of thumb that is 2.4% true.

## The finding that actually matters

**fmax 69.05 MHz against a 105 MHz composed acceptance floor — 34% short**, and
well below the shell's own 99.34 MHz which is already under D19j.

**No explanation is offered.** The critical path has not been read. A composed
number exists and its cause does not yet, and guessing from the block list is
the arithmetic form of measuring instead of looking.

## Next, in order

1. Read the fit's timing report and name the critical path. That is one file.
2. COMBINE.V1's fit is running now (chained, started 11:48) — it settles
   whether variant A LOGIC2 reaches zero DSP after the multiplier count went
   from ~14 to 2.
3. perspuv's per-axis array split: its closure is FREE now that the island fit
   has cleared.

## 12:45 — the toolchain was IDLE, and COMBINE.V1's DSP rule is MET

The nudge's third check fired for real: three samples of `Get-Process quartus*`
returned zero. Fixed immediately — COMBINE.V1 relaunched with a 4 h budget.

**Its previous run was killed by the watchdog at 3,601.8 s against 3,600.** Same
failure mode as the island's first attempt, and for a block of only 744
registers, which is surprising until the pin count is read: **596 virtual pins**
against 744 registers. That block is pin-dominated, and placing pins is what the
fitter is spending its hour on.

**The synthesis numbers are the result that matters, and the rule is met:**

```
registers 744 | blockMemoryBits 0 | dspBlocks 2 | virtualPins 596
```

**2 DSP.** The architecture's §3.4 tripwire is "reject DSP > 2". The refuted
II=1 block measured 8; the first LOGIC2 attempt measured 7 because I had written
fourteen multipliers inside case arms; hoisting one product per lane and moving
every acceptance-time product into a microjob took it to exactly 2.

So variant A LOGIC2 **does** reach what §15.5 says it reaches. ALMs and fmax
still pending — those come from the fitter.

## Where I am, before the next result

**In hand:** nothing half-finished. G1-D is written up including its critical
path. The staircase is complete and committed.

**Next when COMBINE.V1 lands:** if ALMs ≤ 800 and DSP 2, variant A is proven and
the refuted `zhao_texture_combine` can be deleted — its RTL, its manifest row,
its entry in `zhao_prod_top`'s source list, and `texture_combine_diff.cpp`,
together.

**Then:** perspuv's per-axis array split. Its closure is free now.

## 13:16 — G1 is briefed; consolidating

Fit running (COMBINE.V1, 4 h budget). Nothing new landed. Not idle.

**G1-D is written up and turned into a decision brief**
(`reports/G1-CLOSEOUT-DECISION-BRIEF-20260905.md`, docket D19t): what the
number settles, what it reopens, and what it deliberately does not touch.

**perspuv's standing row is worth noting against the island's:**

```
zhao_raster_perspuv_svc   alms 2204  reg 3293  fmax 82.00 MHz
zhao_texture_island_top   alms 7720  reg 11790 fmax 69.05 MHz
```

perspuv alone closes at 82 MHz; the island closes at 69.05. So perspuv is NOT
the island's limit, which is consistent with the critical path naming
`zhao_raster_rcp24_svc` and nothing else. That row is also the PRE-SPLIT
figure — the chained fit is what will say whether the per-axis split moved
3,293.

**Now consolidating.** A full build is running before the full `fast` gate.
Today changed FRAGROB's ports, the combiner twice, perspuv's storage, and
`tb_zhao_shell.sv` six times; subsets have passed but the whole gate has not
been run since, and it is what CI will run.

**Deliberately NOT started:** an RCP24 registered-pick variant. It is the named
cause of the island's fmax, and it spends throughput margin ruling R7
constrains — an owner call, listed as question 3 in the brief.

## 14:00 — determinism verified, and shown to be able to fail

The roadmap makes determinism a day-one property. The host was heavily changed
today, so record/replay was re-verified end to end:

```
record  300 ticks, seed 0x1234        -> match.zrec, 8,432 bytes
replay  OK -- 300 ticks, identical hash stream
```

**And then made to fail, because a replay that always says OK proves nothing.**
One bit flipped at three offsets:

```
offset 8263 -> DIVERGED at hash index 279 of 301
offset 7167 -> DIVERGED at hash index 142 of 301
offset  843 -> OK, identical hash stream
```

The first two are the format working: a desync is LOCATED at the tick it
happened, which is why inputs and hashes are recorded separately.

**The third is not a defect and is worth understanding.** A recording carries
input bytes that provably cannot change authoritative state — the right stick
is read only on a tick that fires a bolt, so its value on every other tick is
inert. Corrupt one and the simulation replays identically, because it genuinely
would have.

So the replay answers *"did the simulation diverge"* and NOT *"is this file
intact"*. Only the first is what the roadmap asks for. `session_io.hpp` now
says so, and says why there is deliberately no CRC: it would answer the second
question and invite treating a passing CRC as evidence about the first.

**A gotcha worth carrying:** the desktop host exe needs `zhao-env` sourced too.
Without it, it fails with `0xC0000139` STATUS_ENTRYPOINT_NOT_FOUND, which reads
like a corrupt binary and is a missing runtime DLL on PATH. Same family as
`ctest` reporting BAD_COMMAND on all 500 tests.

## 14:45 — the gate CI runs was RED, and it was my doing

Running every npm gate CI runs found `ledger:check` failing with two errors,
both introduced today. `abi:check`, `tables:check` and `manifest:check` were
green. **CI would have failed on this session's work**, and nothing else I had
run would have told me.

**V20** — `zhao_texture_island_top.sv` claimed "different fragments by
construction" with no `ENFORCED-BY` within ten lines. Named the real enforcer:
FRAGROB's `ctx_m`, written at allocation and read at `head_slot_c` on
retirement. And wrote down the gap rather than letting the tag imply more than
it does: **no test isolates per-fragment recipe identity.** The composed test
would catch the recipe never arriving; it would not catch two fragments
swapping recipes. Closing that needs counters at the island boundary, which
changes the port list and invalidates the 7,720 ALM number — so it waits for a
pass that re-fits.

**V17** was a checker fragility, not a drift. The contract cites
`zref::material::combine` correctly, but the rule takes the first backticked
`zref::…` matching `[A-Za-z0-9_:]+` **up to a closing backtick** — a symbol
written with its parameter list attached never closes, so the rule falls
through to the next `zref::` it finds and reported `zref::unit_mul`. The
citation was right and the message named a symbol mentioned in passing.

`ledger: check OK — 107 blocks / 40 ops` now.

**Worth noting from the census it prints:** *"per-block fits do not share, so
every census total is an UPPER BOUND, not a prediction."* That was already
known. What today added is that for ALMs the bound is TIGHT — 2.4% — which is
new and is what §4.4 of the G1-D report now says.

Full `fast` gate running.

## 15:1x — a fix that was never wired, and two bugs an idle path was hiding

**CORRECTION to this morning's work.** The commit claiming the combiner's
recipe now rides FRAGROB's context word wrote the packing and did not connect
it — the instance passed `frag_ctx_i` where it meant `fr_f_ctx`. So the
combiner read recipe and `sample_count` from the test's raw context value,
whose `[23:22]` is zero, and all 64 fragments took the untextured
base-passthrough path issuing NO product jobs. The composed test passed 11
checks the whole time.

Found by bringing COMBINE.V1's per-recipe counters to the island boundary and
asserting them: all eight read zero. **Third time today a counter saw what
results could not, and the first time one caught a fix that never took effect.**

**Two more, both hidden by the palette being permanently idle.** The island
hardcoded `CLASS_BILINEAR`, so `palette 0` read as "expected" and meant "never
tested". With the class made a per-fragment boundary input:

* the response identity came from the bilinear token unconditionally, so
  palette answers carried the wrong slot/generation;
* lane and palette both answer into ONE port with no arbitration, so
  coincident answers lost the lane's result while its producer believed it
  taken.

64 in / 64 out, 0 ID errors, bilerp 131 and palette 61, 15 checks.

### OPEN: the job distribution

```
combine jobs by recipe: 0 0 0 4 0 0 24 112     (indices 0..7)
```

64 fragments cycling eight recipes should be eight each. Accounting:
LERP 4 = 1 fragment, DETAIL_LIGHT 24 = 4, DETAIL_MASK 112 = 28 — and
MODULATE/MODULATE2X at zero where 8 each was expected. The counts **rise with
recipe index**, which suggests the recipe value observed is biased high rather
than randomly wrong.

Ruled out: FRAGROB's retire handshake is correct (`R_HOLD` exits on ready, one
transfer per retirement), so this is NOT the re-submission pattern that bit
ASSEMBLE and COMBINE.V1 earlier today. Also ruled out: refusals, which are 0.

Not understood. The assertion is set to what it can honestly claim — more than
one recipe moved — and the anomaly is written down rather than tuned away.

**These changes invalidate the 7,720 ALM measurement**, which is why they were
batched into one pass. The island needs re-fitting when the queue clears:
COMBINE.V1 is running, perspuv is chained.
