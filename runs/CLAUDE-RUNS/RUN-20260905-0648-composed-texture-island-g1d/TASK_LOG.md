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

## The fast gate was not slow, it was expensive -- and I misread it three times

Three fast-gate runs were started and abandoned in this session. I attributed
each to machine contention from the running fit, and each time relaunched it.
That was wrong, and the tell was there from the first run: the output showed the
SAME three tests started and none finished, run after run.

Measured instead of assumed: `test_shell_meshfetch_path_directed` alone takes
**446 seconds** and passes its 9 checks. There are five such
`shell_*_path_directed` tests, all labelled `fast`.

Instrumenting the drain loop showed the fixed `200000` step budget after
`render_frame_end()` never terminates early -- **no blit ever completes**, so
that loop is waiting for nothing and simply burns its full count, three times
per run. But cutting it to 5,000 broke the test (1,728 drawn words against
1,792) and saved only 57 s of 392 s, which says the drain is NOT where the cost
is: the run is roughly four million shell evaluations and most of them are in
the draw loops.

So this is not a hang and not a bug to fix by trimming a constant. These are
genuinely heavy full-shell tests wearing a `fast` label, and the pre-existing
`shell_draw_directed` uses the same 200,000 idiom, so it is not something the
D22 staircase tests introduced.

NOT CHANGED UNILATERALLY. Moving five tests out of `fast` cuts what runs on
every commit, and `local gates must match CI` is a standing rule -- that is a
coverage-versus-cost call for the owner, not a quiet relabel by the agent who
found the gate inconvenient. Recorded here with the measurement so the decision
can be made on numbers.

The immediate consequence is that a green `fast` gate costs roughly half an
hour of wall clock in the shell tests alone, which is why every attempt this
session was cut short by something else needing the machine.

## Mipmapping: the owner ruled, and two defects stand in front of it

Owner ruling: full trilinear is NOT a requirement; the cheap two-level blend is
-- one texel from level L, one from L+1, decode both, blend the COLOURS, never
the indices. An architecture for it was commissioned and landed
(`reports/TERRAIN-MIP-TWO-LEVEL-BLEND-ARCHITECTURE-20260905.md`).

Two blockers, both read out of current source and **verified here rather than
taken on report**:

* **The mip level cannot be non-zero at all.** `req_lod_i` is Q4.4 -- integer in
  [7:4], fraction in [3:0] -- and the island zero-extends a 4-bit LOD into the
  FRACTION nibble while the planner reads [7:4]. Enabling MIP_EN changes
  nothing, and the clamping, level UV scaling and packed chain offsets the
  planner already implements are all unreachable. The defect and the feature
  share one fix: those misplaced fractional bits ARE the blend weight.
* **Every CLUT lookup reads the same byte.** `lu_idx_i` takes
  `disp_clut_data[7:0]` of a 64-bit word; a CLUT8 halfword holds two texels and
  the selector reaches the bilerp lane and nothing else. Zero hits anywhere in
  the file for another byte selection. Odd texels decode the wrong byte.

**That qualifies today's own CLUT repair.** Making the path return colour
instead of black was real, and it did not make the path correct: the palette is
resident and answering and about half the indices it is asked for are still the
wrong texel. LIVE, not RIGHT -- and the directed test asserts residency and
non-blackness, which is exactly the gap a colour check cannot see.

**And a subagent's claim had to be corrected, not relayed.** The lerp8 conflict
was reported as islandrearchitecture5.md §15.1's `(x+128)>>8` against the
oracle. §15.1 actually says `sat_u8(a + rescale_s((b-a)*w, 8))` and defers to a
signed rescale; `(a*b+128)>>8` is `unit_mul8`, a different operator. The real
disagreement is with `spec/qformats.md`, which ratifies round-half-up as
`floor((n + floor(d/2))/d)` -- 0 where the oracle's magnitude rounding gives
-1, and the oracle's own comment says it departs deliberately. A two-level
blend generates exactly those negative half-LSB products whenever the higher
level is darker, so this is an owner ruling that must precede the RTL.

## D22 tread 7 lands: GEOM.VDECODE

The bench stops supplying decoded coordinates and supplies the four 32-byte
format-0 records. Measured first run: 4 decoded, 0 refused, 0 format_bad,
BYTE-IDENTICAL framebuffer, sensitivity 1279 words. The bench's vertex table is
driven to POISON throughout the decode pass, so a shell still reading it draws
a different picture rather than accidentally agreeing.

The transform is identity by construction and the batch engine is untouched;
both are stated in the test rather than left for a reader to assume.

One check of twelve failed and it was instrumentation: the gate flag must clear
when the triangle goes, and the test read it after the drain. Now a separate
sticky latch.

## The fast gate finally ran: 395 of 400

Three failures, two of them mine:

* `source_list_parity` caught VDECODE added to the Verilator list and not the
  Quartus QSF -- the exact "green under Verilator, cannot be elaborated by
  Quartus" failure the gate exists for. Fixed; passes.
* `desktop_smoke` was a regression from earlier today: it passes a bare
  positional path and the host now takes flags, so it printed usage and
  returned 2 with the binary working perfectly.
* `format_check` still undiagnosed.

## The combiner refit failed TWICE, and my explanation was wrong

First attempt: `incomplete:failed:quartus_fit.exe` at 3,002.7 s, no ALM, no
fmax. I blamed myself — five heavy Verilator tests, a full build and a configure
running on eight cores while Quartus placed — and wrote that into a commit
message and into this log as "probably me".

Second attempt, relaunched alone with the machine quiet and the workspace kept:
**3,003.3 s**, identical failure.

Two independent runs stopping within a second of each other is not resource
starvation. `run_block_fit.ps1` carries `[int]$TimeoutSeconds = 3000`, and both
runs died at the deadline. The block simply takes longer than fifty minutes;
its placement PREPARATION alone measured 32 minutes, and perspuv — a smaller
block by ALMs — took 10,505 s end to end.

**The script's own header already said this.** It records that
`zhao_measure_tokens` once reported `timeout` at the 900 s default and then
fitted cleanly in 749 s of quartus_fit on the very next run with a larger
budget. The lesson was written down, in the file I was invoking, and I read past
it to reach for a more interesting explanation that happened to be about my own
behaviour.

That is the session's recurring failure in yet another costume: a confident
cause published before the two-minute check that would have settled it. The
check here was one grep for the timeout constant.

Relaunched with `-TimeoutSeconds 21600`. The starvation concern is not
baseless — running five heavy tests beside a fit is still a bad idea — but it
was not the cause, and the commit that claimed it needs the correction more than
the caution needs repeating.

## Closed since

**D22 tread 7 verified: 12 of 12.** The twelfth check was instrumentation, not
the decode — the gate flag must clear when the triangle goes, and the test read
it after the drain. Separated into a sticky observation latch and it passes.

**The LOD nibble is fixed.** LODW 4 to 8 and the pad removed, so the Q4.4 LOD
reaches the planner in the nibble it reads. The oracle already declared this
ABI (`zref_texture.hpp:151`, `U 4.4`) and the planner already read [7:4], so
the island was the outlier and this is not a judgement call between two
defensible widths. Inert at the commit and verified so rather than assumed —
`bind_mode_i = 0` means MIP_EN is low and `lvl_req` is 0 either way, and all 25
island checks plus the nine-test texture neighbourhood are unchanged.

## D22 tread 8 lands: the shell fetches its own records

GEOM.ASSETFETCH composed. The bench stops synthesising vertex records and
supplies raw pool bytes; the fetcher reads the meshlet footprint out of them and
streams 32-byte records to VDECODE, closing MESHFETCH → ASSETFETCH → VDECODE.
Measured: `meshlets 1, beats 24, denied 0, refused 0`, four records decoded,
byte-identical framebuffer, sensitivity 1279 words, 16 checks.

**24 beats is exactly three 64-byte lines** — index run of one, vertex run of
two. That is the phase structure the block implements, and it is why my first
beat player could never have worked: it granted once and streamed the whole
pool, so the index phase would have swallowed everything and the vertex phase
never started. Reading the fetcher's state machine before running was what
caught it.

Two of my own process failures on the way, both already written down in
CLAUDE.md and both committed anyway:

* I read `BUILD_RC` off a PowerShell PIPELINE ending in `Select-String` rather
  than off cmake, so a build that produced no executable reported success and
  the "missing" exe was blamed on the wrong thing. CLAUDE.md: *"Read the
  build's exit code, not the pipeline's."*
* The verilate rule could not see the new source until `cmake --preset`
  regenerated the graph — the stale-`build.ninja` trap, hit for the second time
  today.

## The fitter mode is now a knob, and the pessimism is priced

Owner call: get a cheap, pessimistic combiner fit rather than an exact one,
because a block at 29.74 MHz against a 125 MHz target is being rearchitected
regardless of the third significant figure. The only question this run answers
is whether the counter narrowing moved the number AT ALL.

`run_block_fit.ps1` gained `-OptimizationMode`. The default is unchanged and
the row records the mode when it is overridden, because a BALANCED row and a
HIGH PERFORMANCE EFFORT row are not comparable and a table that mixes them
silently is worse than one that omits the fast rows.

**The pessimism is quantified from a measurement already in the tree**: the
shell QSF records OPTIMIZATION_MODE at +2.08 MHz, alongside
OPTIMIZATION_TECHNIQUE (−3.01, reverted) and register duplication (nil). So a
BALANCED row reads about 2 MHz low — roughly 7% here, and nowhere near enough
to change a rearchitect-or-not verdict.

And nothing was wrong with the fitter, which was worth checking rather than
assuming: `"HIGH PERFORMANCE EFFORT"` is deliberate and documented, the block
carries **596 virtual pins against 744 registers** so the placement problem is
far larger than its 1,994 ALMs suggest, and perspuv at 2,204 ALMs spent 78
minutes in the same stage. The scaling is consistent. The only bug was the
3,000 s timeout.

## The fitter-mode detour: two conclusions, both unsound, both published

This is the worst sequence of the session and it is recorded in full because
the shape of it matters more than the outcome.

1. I proposed BALANCED as a cheap characterisation fit and implied it would be
   a large saving.
2. I then measured its placement preparation at ~25 minutes against 32 and
   said the saving was modest.
3. I then measured it at 54 minutes and said the mode had made things WORSE.

**None of those comparisons was valid.** The BALANCED run had a full shell
verilate build running beside it; the earlier run did not; and other work was
happening on this machine that I had no visibility of whatever. I timed runs
under different and partly unknown load and attributed every difference to the
setting.

CLAUDE.md states the law directly: *"A measurement across MISMATCHED POSES
measures the pose. Compare like with like, or do not compare."* I broke it
twice within an hour, in opposite directions, and published both readings as
findings.

**And the question was already settled before I touched it.** The shell QSF
carries a closed apparatus audit: every fitter and synthesis knob measured,
OPTIMIZATION_MODE at +2.08 MHz and KEPT, OPTIMIZATION_TECHNIQUE at −3.01 and
reverted, register duplication nil, PHYSICAL_SYNTHESIS_EFFORT normal because it
"showed no headroom to scale". Adding a knob to un-pull a lever that had been
measured and deliberately pulled was wrong regardless of any timing.

The knob is reverted. The running fit is not killed and not restarted — it
keeps its budget and reports with the mode recorded, which is accurate about
how it was produced.

**What was actually true and survives all of it:** the block carries 596
virtual pins against 744 registers, so its placement problem is far larger than
1,994 ALMs suggests; the 3,000 s timeout was the real bug; and relaxing the SDC
would floor the reported fmax at whatever we asked for, which is why it stays
at 100 MHz.

### And the apparatus audit was already closed

Chasing the fit cost further was re-litigating a settled question. The shell
QSF records that every fitter and synthesis knob has ALREADY been measured:
OPTIMIZATION_MODE (+2.08 MHz, kept), OPTIMIZATION_TECHNIQUE (-3.01, reverted),
register duplication (nil), PHYSICAL_SYNTHESIS_EFFORT normal because it
"showed no headroom to scale". Advanced Physical Optimization runs because
FITTER_EFFORT is STANDARD FIT, which is the full-effort setting.

So there is no cheap lever that preserves the measurement, and the one that
would be cheap -- relaxing the SDC -- destroys it. The fit costs hours because
the block is hard and wide-ported. BALANCED is the only real saving available
and it is small.

The lesson is the one this session keeps repeating from a new angle: the answer
was written down in the file I was already invoking, and I went looking for it
in the log output instead.

## D22 tread 9: the index stream, in flight

The cheapest tread in the staircase, because the work was already being done.
ASSETFETCH reads the meshlet footprint as three 64-byte lines, and EIGHT of its
24 beats were the index run — fetched, then discarded, because `ix_req_i` was
tied off and ASSEMBLE still read the bench's flat stream. Tread 9 connects the
port that was already being fed.

Checked before building, not after: ASSETFETCH answers with
`assign ix_valid_o = ix_pend_q` — registered, one cycle late — and ASSEMBLE
holds `ix_req_o` through S_FETCH until `ix_valid_i` arrives. Its "no ready"
means the RESPONDER cannot be stalled, not that ASSEMBLE cannot wait, so a
registered answer is legal. The index buffer is sized `MAX_TRIANGLES * 3` =
378 bytes, so the one-triangle case is far inside it.

The bench's `asm_index_stream_i` is driven with a DIFFERENT legal triplet
during the fetch pass, so a shell still reading it draws a different triangle
rather than agreeing by accident.

**Tread 10 is the last one** and it is scoped: replace the played guard and beat
stream with the real `zhao_mem_guard` over `zhao_sdram_model`, which the bench
already instantiates.

## D22 tread 9 lands: nine treads

The u8 index stream now comes out of the fetched footprint. 17 checks, beats
still 24 -- the fetch is unchanged, only the consumer moved, which is what a
tread that moves ONE thing should look like.

The decisive check is not the framebuffer: ASSEMBLE named (2, 0, 3), the
triplet the FETCHED run carried, while the bench stream was poisoned with
(1, 0, 3). A shell still reading the bench draws the decoy triangle.

Both hazards were checked by reading the block BEFORE building, not debugged
after: ASSETFETCH answers only from S_SERVE so it cannot return a half-filled
buffer, and ASSEMBLE holds its request with no deadline because its "no ready"
constrains the RESPONDER, not the asker. That asymmetry is also the reason the
fetcher buffers the whole footprint instead of caching it.

Still owed on this tread: prefetch_stall_o is tied off. The block header names
it as the counter that decides whether double buffering earns its ~2.4 KB, and
a dangling evidence port is the pattern this bench criticises elsewhere in its
own comments. The edit is prepared and deliberately not built while the fit and
other work hold the machine.

## The fit outlives its watchdog

Owner asked for a longer budget without stopping the run. The budget could not
be changed in place — `run_block_fit.ps1` computes its deadline in memory at
launch, so editing the script cannot reach a run already going.

What could be done: the watchdog is a SEPARATE PROCESS from Quartus.
`quartus_fit.exe` (3188) was a child of the script's PowerShell (4368), and
Windows does not take children down with the parent. Killing 4368 left 3188
running with no deadline at all, verified immediately after.

**The cost, taken deliberately:** the script would have run `quartus_sta` twice
after the fitter — once for timing, once with `report.tcl` — and parsed the
results into the JSON row. Nothing does that now, so those stages and the row
are mine to run when the fitter finishes. The workspace survives because
`-KeepWorkspace` was passed and no cleanup will run.

The DEFAULT is separately raised 3,000 → 28,800 s, with the evidence beside the
constant: the same mistake has now been made at two different values (900 in
August, 3,000 today), and the fits that actually succeed here take 9,238 s and
10,505 s. A too-small timeout produces a row saying a block does not fit when
nobody waited, and that failure is silent and looks like data.

## The watcher, and the false verdict it nearly delivered

Built a 30-minute poller that reports stage transitions and gives up at 04:00.
Its first version used **log growth** as the liveness signal.

Measured before arming it properly, at 22:31:

    log last written   20:44:47   -- silent for 107 minutes
    CPU consumed       4,402 s, climbing ~50 s per minute

Quartus does not log during placement preparation. The fit was burning close to
a full core and saying nothing. That heuristic would have declared a healthy
fit **wedged at about 00:30** and woken the owner to say so.

**Liveness is CPU, not chatter.** Rebuilt on that, exiting on: the fitter gone,
04:00, or CPU failing to advance across two consecutive polls — an hour of
genuinely doing nothing rather than an hour of quiet work. It never kills
anything; a timer has already made that decision twice today and was wrong both
times.

Worth recording plainly: I wrote the "an instrument that returns a confident
wrong answer is worse than none" lesson into this log repeatedly today, and then
built one, an hour later, into the tool meant to watch the thing the lesson was
about.

## The uncontended stall baseline: 27

`prefetch_stall_o` is connected — it was tied off when ASSETFETCH was
composed, which is the dangling-evidence-port pattern this bench criticises in
its own comments, on the one counter the block's header nominates for deciding
whether double buffering earns its ~2.4 KB.

    meshlets 1, beats 24, denied 0, refused 0, STALLS 27

Twenty-seven cycles of a consumer waiting on a buffer still filling, against 24
beats of fetch, with a bench memory that grants immediately and answers in one
cycle. The consumers begin asking almost as soon as the fetch starts, so single
buffering already costs about a full fetch per meshlet **in the best case this
machine can ever have** — before contention exists at all.

Measured NOW rather than with tread 10 on purpose: tread 10 puts both fetchers
behind a real arbiter, and everything in treads 6 through 9 assumed a memory
that never says no. Taking the baseline afterwards leaves two variables and one
number.

## The same lens, turned on the rest of the tree

The island's defect was a CLASS — ten signals, one shape — so the question is
whether it is anywhere else. Checked rather than assumed, in both directions:

    zhao_texture_island_top    HAD IT, ten places, repaired and guarded
    zhao_shell_top (21 blocks) clean -- one consumer, at the admission handshake
    zhao_field_v3_svcpath      clean -- one consumer, at the admission handshake

**The class was real and the spread was not.** All three are now guarded and
all three contracts are mutation-verified: a deliberately inserted late read of
a genuine per-transaction input is caught in each.

**And the shell contract had to be narrowed before it was correct.** Its first
version treated every `render_*` input as per-transaction and flagged
`zhao_raster_fbwrite` reading `fb_base_i`, `fb_stride_i` and
`frame_end_i` — which would have reported two fabricated defects in the
shell. Those are FRAME-SCOPED: constant across every triangle, so reading them
late returns the same value. The distinction the gate must make is
transaction-scoped versus frame-scoped, not early versus late. Shipping the
first version would have trained its reader to skip the output.

**A LATENT instance, recorded not exempted.** `bind_base_i` and
`bind_mode_i` are consumed at the TMU planner, downstream of admission by the
whole RCP24/PERSPUV latency — the same place the ten repaired taps read from.
Harmless today because the island carries ONE global binding, so the value is
constant. It becomes the island's bug the moment bindings vary per fragment, and
the owner's two-level mip blend is exactly that change. No gate exemption was
added, because an exemption would claim more than the gate checks.

## The audit arrived, and four hard contradictions are repaired

`reports/AUDIT.md` landed from the owner: substantial reference work but NOT one
coherent full-console reference. Its first four items are done, each verified in
source before being touched and each mutation-tested.

**R1 + R2 were one problem.** The resolver refused `recipe >= 6` while the
combiner implemented eight and drove all eight in its differential -- so a legal
terrain record was refused at the door of the block that exists to execute it.
The proper fix takes the ceiling from the enum, which needs that header, which
was impossible while both defined a different `zref::material::Ledger`. The
literal is exactly what let them drift. Fixed, plus the crossing test the audit
asked for by name -- and that file COMPILING is the R2 proof.

**R8: the residency arena accepted uploads longer than a page**, because it
validated against a guard covering the whole arena. The audit's counterexample
reproduced before the fix: `outcome 0, length 512 into a 256-byte page`,
overlapping a PINNED neighbour with every pin counter at zero. That is why
counting pin refusals was never evidence that pins are honoured.

**R15: `product_jobs(kMask)` claimed a product MASK does not perform.** It is a
binary gate. The implementation, the RTL and the composed test all said zero and
only the cost model disagreed -- and S15.4's capacity argument reads that
number. The island test now CALLS `product_jobs()` instead of keeping a copy, so
the drift is closed rather than corrected.

## R5 closed: the texture island is exact on both halves

Three-channel bilinear sequencing built -- the lane always carried `chan_i` and
`out_chan_o` and neither was used, so every direct-colour fragment came out
GREY and there was nothing correct to compare against. bilerp jobs 96 -> 288.

    CLUT     21 matched, 0 mismatched
    BILINEAR 21 matched, 0 mismatched, 0 grey

both against `zref::material::combine` rather than a second copy of the
arithmetic. Four island defects fixed along the way: the CLUT byte select (every
odd texel decoded its neighbour's index), the 565->888 expansion (zero-fill
where the ABI replicates), the bilinear FRACTIONS read live from the planner --
the audit's flagged-but-untested risk -- and single-channel filtering.

**And the exact check passed while being unable to fail.** Mutating red back to
zero-fill produced ZERO mismatches, because the palette entry in use had
`r5 = 1` and 1 has no low bits to replicate. A pattern that distinguishes the
laws in all three channels fixed it. Then the NEXT mutation "passed" too -- and
that was a STALE BINARY: a leftover test process held the executable, the link
failed, and I read the result off a pipeline instead of off cmake. Twice in one
session, both times the trap CLAUDE.md names.

## 8 km floating island: built, and streamed

The sparse island directory was explicitly unbuilt ("Phase-6 loader work") and
is the thing that makes the island possible at all: 15,625 patches against a
1,024 residency, so absence must mean OPEN SKY rather than a miss. Solid ground
came to **793 patches** -- the exact figure terrain_rules 1.4 costs out, derived
here independently from its 3.25 km².

Then streamed, over 80 frames crossing the island and coming home:

    published 1485, evicted 1404, RETURNED 702, refused 0, peak 81

**Return is the test, not streaming.** A first frame under no pressure proves
nothing; the failures live where a patch leaves, loses its page, and comes back.
`Arena::release` had to be added -- a page could previously only be freed by
REPUBLISHING the same resource, so anything that merely stopped being wanted
held its page forever.

## D22 tread 10: real memory, and the protocol nobody was speaking

The bench's last played input was MEMORY itself. GEOM.ASSETFETCH now asks
`zhao_shell_top`'s own MEM.GUARD, which forwards to MEM.VRAM.ARBITER on client
slot 3, which offers to MEM.SDRAM.CTRL, and the beats come back out of the DRAM
model past refresh and CAS latency with scanout reading the framebuffer all
frame. The played pool holds DECOY records throughout.

    played: beats 24, stalls 30, painted 1792
    REAL:   beats 24, stalls 360, painted 1792, 24 beats out of the DRAM

**30 to 360.** Twelve times the uncontended baseline, which is the number the
register file could never have produced and the reason that baseline was
recorded before the tread.

**And the find: both geometry fetchers had the guard protocol wrong.** They
wanted `ready && ok` in ONE cycle. `zhao_mem_guard` cannot do that -- ready
is the level `!fwd_active`, ok is a pulse the cycle AFTER the accept, and the
accept is what raises fwd_active. A passing request looked like a denial with no
violation flag, silently. `zhao_raster_fbwrite` and `zhao_debug_frameblit`
already waited a cycle and fbwrite's header even quotes the guard line: **four
clients, two protocols, and the two that were wrong are exactly the two whose
memory was played.** Three unit benches went red on the repair, each because it
was the model the block had been written to match:

    assetfetch_rtl_directed      16 of 28 FAILED  ->  56 checks passed
    assetfetch_random             4 of  9 FAILED  ->   9 checks passed
    geom_meshfetch_rtl_directed   6 of  7 FAILED  ->   7 checks passed

That first row twice over: it reported 28 checks while failing 16 and reports 56
now. A failing test that also stops counting reads low, like every other broken
instrument.

**One geometry client, not two.** `zhao_vram_arbiter` tags the controller by
CASTING THE SLOT INDEX, so slot 3 is ENGINE1 and slot 4 is DEBUG, and the guard
grants the asset pool to ENGINE1 alone. MESHFETCH has no identity the memory law
admits; its half of the protocol repair is REASONED, not measured, and says so.

## R6: the island orders at its boundary now, exactly

The composed test asserted max displacement <= 8 and called it a regression
guard on a known defect. The old comment concluded the repair was the recovery
architecture's rearchitecture -- move the allocation ahead of the reciprocal
work. Too pessimistic by one idea: the interior is SUPPOSED to reorder, so order
at the EDGE. A submission sequence stamped at admission, carried through the
combiner beside the caller's tag, restored in a reorder buffer that cannot
overflow because FRAGROB admits at most FCTXN fragments.

    0 out of order, max displacement 0, 11 fragments held at the boundary

That last number is why the first two mean anything.

## And the island was binding a ONE-BY-ONE TEXTURE

The remaining gap was a printed note: no fragment addressed an odd texel, so the
CLUT byte select was unexercised. The note blamed the harness's reach --
"forcing odd texels needs control over the coordinate after the perspective
divide, which this test does not have".

Wrong, and comfortably so. `bind_mode_i` was 0, and bits [11:8]/[15:12] are
log2 width and height: a 1x1 sheet. Every coordinate resolved to texel (0, 0).
Sweeping the u coordinate across FIVE orders of magnitude moved nothing -- same
one cache miss against 192 hits, same colours. The coordinate path was not being
composed at all.

    64x64 sheet:  miss 1 -> 6,  CLUT low 21/high 0 -> low 12/high 9, 0 mismatched

Mutating the select to the low byte reproduces the old reading exactly and fails
only the new check -- because the exact-colour check accepts either byte, so a
byte-select regression scores zero mismatches and looks perfect. What catches it
is requiring that both bytes were seen.

## The toolchain was idle, so the composed island is refitting

Started 2026-09-06 against HEAD. The committed row -- 7,720 ALM, 69.05 MHz, 17
DSP at `afb7070f` -- predates the ingress-capture repair AND today's R6 ordering
boundary, which added a reorder buffer with a 64-to-1 mux of 33 bits on the emit
path and widened the combiner tag from 16 to 22 bits. That last one touches the
record file in every combiner slot, so it moves area INSIDE the closure as well
as at the island level. No prediction offered; this island's last obvious
explanation was worth 4 MHz of 36.

## D19v, found while the fit runs: 2 DSP, ZERO claimed, gate at exactly 2

No new fit was needed -- it was already in the committed row. COMBINE.V1
measures `dspBlocks 2`, and its rule violations list ALM, registers and fmax but
NOT DSP, because the target gates at `max_dsp: 2`, inherited unchanged from the
refuted II=1 block. The block's own header says `multstyle = "logic"` makes it
use ZERO DSP, which is variant A's stated advantage over B. Two call sites, two
DSP blocks, one each.

The target's comment predicted the wrong failure mode: it watches for a BREACH,
and this landed exactly on the ceiling. **The gap between what the block claims
and what the gate allows is exactly two, and the measurement sat in it.** The
mechanism is that the attribute sits on a function-local automatic variable, and
the shell QSF already records that this same attribute "was believed for weeks".

## D22 step 4 compared ONE vertex of three

Found while asking what step 4's evidence was actually missing. The bench
exposed only `pj_x_r[0]`, so vertex A was compared against
`zref::render::project_vertex` and B and C reached the assertions only through
the drawn picture -- which is the same evidence step 4 had BEFORE GEOM.PROJECT
was composed at all. `dbg_proj_behind_o` was already three bits wide, so two of
the three vertices had a flag exposed and no coordinates to go with it.

All three are now exposed and compared on x, y and w, with a DISTINCTNESS check
beside them, so a collector that wrote one vertex into all three slots could not
pass by having them all agree.

## The guard-verdict mistake was in THREE clients, and the rule is now a gate

Fixing two instances leaves the rule unenforced, so
``tools/rtl/check_guard_verdict.py`` enforces it, wired into
``npm run design:report``. It found a third client immediately, and the polarity
is inverted: MEM.SCANOUT's fetcher tested ``ready && violation``, so where the
fetchers read every PASS as a denial and stalled, scanout reads every DENIAL as
a pass and drops into F_BEATS to wait for beats a refused request never sends.

The retry path and ``violation_now`` were both structurally dead, and the arm's
comment -- *"denied (impossible in Phase 2)"* -- is true of the region rules and
was NOT why the arm never ran. F_VERD added; ``shell_draw_directed`` (20 checks,
the whole display path) passes with the extra cycle.

**Two things were done to the gate before trusting it.** It reported three
offences on its first run that were all PROSE, including two inside the comments
documenting this repair and one in fbwrite's header where it QUOTES the guard
line to explain why that block gets it right -- a gate that flags the
documentation of a fix as the absence of the fix trains the reader to ignore it.
And it self-tests at import on a known-bad and a known-good arm.

**Then its coverage audit found a bug in itself.** The client list is checked
against the tree so it cannot rot; the first audit reported
``zhao_debug_frameblit`` as "no longer a client", because it declares
``input zhao_pkg::zhao_guard_rsp_t`` and my discovery regex had no provision
for a package qualifier. A detector reading low on its first run, inside the tool
written to enforce a different instance of that law. Seven clients, all clean.

## R7: the particle expansion is blocked on the wrong thing

Two places -- the header and the PART.EXPAND contract -- said turning a world
radius into a screen half-side "needs a decision first". It does not.
``zref::render::draw_form_marker`` implements it, and
``tests/render/render_directed.cpp`` exercises that exact branch with
``flags = 0``:

    half_sub = rescale_s32(fx_mul(size_fx16, c.s.d), 8)     // size * (1/w)

with the inversion trap recorded beside it. A particle expansion using it would
be CALLING a ratified law. The warning against inventing a projection is a good
warning and, standing unqualified, it was doing the opposite of its job.

**What is actually missing is ``base_radius_fx16``** -- the per-species base
radius ``particle_radius()`` multiplies. ``species`` is a u7 and no species
table exists in ``reference/``, ``spec/`` or ``design/``. That is content
and therefore the owner's, and guessing it would be the real instance of the
failure the warning describes. The block stays unfixed, now against the right
blocker.

## The owner's COMBINE / ASSETFETCH recovery brief arrived

Filed at ``reports/COMBINE-ASSETFETCH-RECOVERY-20260906.txt`` and indexed as
docket D19x, per its own instruction: the COMBINE and asset-fetch
SPECIALIZATION of texture recovery v2, not a replacement for it. Two headline
decisions -- COMBINE needs a different execution ORGANIZATION rather than
different material math; ASSETFETCH needs OVERLAP AND OWNERSHIP rather than two
fetch engines.

### Its three prerequisites

**1. The island's reorder buffer has no admission credit. OPEN.** It refutes a
proof I wrote this morning, and the counterexample is exact: hold the sink
not-ready, admit and complete 0..63, and while entry 0 still holds sequence 0
the INTERNAL CONTEXTS HAVE BEEN RELEASED and admit more work -- sequence 64
overwrites entry 0. FRAGROB releases upstream of the buffer, so what is parked
at the output was never bounded by FRAGROB's capacity, and a 64-fragment test
cannot reach the wrap. A false "cannot overflow" is worse than the bug: it tells
the next reader not to check.

The fix is written and waiting: reserve at admission, return ONLY at final
external acceptance, gate ``frag_ready_o`` AND RCP's valid (or RCP takes a job
the caller was told did not handshake), no same-cycle bypass. It cannot land
while the island fit runs -- ``run_block_fit.ps1`` re-hashes sources at the end
and marks a changed run contaminated, so editing now would destroy the
measurement rather than merely risk it.

**2. Both geometry fetchers read ingress pins live after acceptance. CLOSED.**
Verified in source: ASSETFETCH's client, MESHFETCH's client AND descriptor
address, all sampled when the request is FORMED, several states after the job
was accepted. Captured, and both files put under the ingress gate.

Mutating it back found that **the gate could not see any port declared with an
unqualified typedef** -- its regex enumerated type spellings and landed the
capture group on ``zhao_client_e`` instead of ``m_client_i``. True of every
such port under contract since the gate was written. A detector reading low,
inside the tool written to enforce a rule about exactly that.

**3. The committed COMBINE timing reports were the wrong experiment. CLOSED,
and it corrects me.** They were the 29.74 MHz run; I had committed one saying it
was the census export, using its MTIME as evidence of freshness. The 36.28 MHz
database survived under ``%TEMP`%` and ``quartus_sta`` re-exported matching
reports in **32 seconds** instead of a second 13,627-second fit:

    setup  -17.561 ns   rec[0].recipe[0] -> jobs_by_recipe_r[5][30]
    hold    -5.284 ns   f_s2_rgb_i[6]    -> rec[1].s2_b[6]
    fitter  1,475 ALM, 893 registers, 2 DSP, 0 M10K

**The worst cone is recipe state driving a counter** -- the same family
``zhao_mem_guard`` was repaired against, not the ``Decoder5~9 -> Add46~41``
the superseded report pointed at. And D19v's DSP finding now rests on FITTER
evidence.

### ASSETFETCH, first two targeted fixes from the migration map

Client captured (above), and **exact response counts validated**: a 64-byte line
is exactly eight packed words, and the block believed ``beat_last_i`` about
where a line ended. A line stopping at word five produced a meshlet whose last
three words were whatever the RAM held from the PREVIOUS meshlet -- a vertex
record that decodes cleanly, passes its format check, and is partly somebody
else's. Three named counters (truncated / overrun / unowned), both faults
abandon the job, 56 -> 62 checks, and disabling the check reproduces the silent
wrong picture.

The overrun scenario **segfaulted with no output at all** first time, because
the played responder's pool reader was unchecked and an overrun beat is by
definition a word outside the authorised line. Buffered output lost in a crash,
exactly as CLAUDE.md describes it.

### The fit

Owner direction: enough time, no watchdog, periodic check-in. Killed the
watchdog Start-Job alone (pid 22624); the fitter (836) and the run_block_fit
host (15816) are both alive, so the host still runs STA and writes the row.
``tools/quartus/watch_fit.ps1`` reports and never kills, and measures liveness
by CPU delta -- log growth would have called a healthy fit hung earlier today.

## GEOM.MEM.ADAPTER: the answer to "MESHFETCH has no memory identity"

Owner brief section 12, and it closes the blocker D22 tread 10 recorded. The
arbiter casts the SLOT INDEX to the client enum -- slot 3 IS ENGINE1, slot 4 IS
DEBUG -- and the guard grants the asset pool to ENGINE1 alone. **A spare slot is
not a spare privilege.** So the two fetchers SHARE the one permitted client
upstream of the guard, with explicit logical ownership.

What it holds to, all from section 12: the three acceptance boundaries are
different events (a guard OK is not returned data; an arbiter grant is not a
completed line, so the owner is not released on the first credit return); TWO
burst scales, the expected count taken from the accepted request's own length
rather than a constant; ONE logical request in flight, which is what makes the
return-routing proof trivial; round-robin at logical-request boundaries rather
than a descriptor preference that could starve payload filling; and **ENGINE1
SUBSTITUTED, not forwarded** -- both requesters in the test present SCANOUT and
the guard sees ENGINE1.

``zhao_shell_top``'s return generator is generalised to match. It counted a
constant eight, and against a 32-byte descriptor that never fires ``last`` at
all and carries the count into the NEXT request.

**30 checks.** Both lengths, ownership of beats and verdicts, fairness, a
three-cycle gap standing in for somebody else's physical burst, a denial
reaching only the requester that asked, and an over-serving memory whose surplus
is reported rather than passed on.

### Three faults, all mine, none the adapter's

Its memory model first served a constant eight for BOTH lengths, as a device to
prove the adapter derives its own count. It was still delivering A's fifth
through eighth words after the adapter had correctly finished A's four-word
line, so every later scenario ran against a model mid-burst. Seventeen checks
failed and ``err_unowned`` reading 4 was the adapter correctly reporting my
own bench's strays.

Its cycle model observed at the TOP of the loop, before driving that cycle's
inputs, so everything came back exactly one beat short -- which reads like an
off-by-one in the adapter and was an off-by-one in when the bench looked.

And the over-serve was first counted as SHORT. The counter fired correctly and
named the OPPOSITE fault: a memory that has not finished at the expected count
sent too much. A wrong label on a right alarm is how a diagnosis sends the next
person to reshape something already correct.

### And the gates caught the registration, in sequence

Each one found the next omission the moment the previous was fixed:
``check_guard_verdict``'s coverage audit -- written earlier today -- caught the
adapter as an unlisted guard client on the first module written after it
existed; then the manifest said UNACCOUNTED; then it said the module is
instantiated by the generated production top but missing from the fit's source
list, *"the fit would die at elaboration"*. That last is the exact failure the
manifest gate's own CI comment records finding the day it was first run in
anger. Ledger, manifest and source list really are three acts.

## G1-D LANDED, AND IT IS 16,193 ALM

14,004 s at ``ea4870d3``. **2.16x the redline**, three rules fired.

    alms      16,193   (nominal 6,600 / redline 7,500 / standalone sum 7,913)
    registers 27,097   (rule 9,000)
    dsp           17   (rule 14)
    fmax       63.54 MHz
    virtual pins 1,259

More than double the previous 7,720 / 11,790 / 69.05, which predates the
ingress-capture repair, the R6 boundary and the widened combiner tag. **The
worst path is now INSIDE MATERIAL.COMBINE.V1** -- ``u_combine|Mux136~0`` at
−5.737 ns, worse than any path the pre-repair census found anywhere, which
corroborates the owner's brief from the island level.

## Attribution WITHOUT a second four-hour fit

Owner direction: attribution before touching the capture and order stores, and
spend the big fit very sparingly. **Analysis & Synthesis answers it alone**, in
minutes -- ``run_block_fit.ps1 -MapOnly``, added for this.

**Nineteen arrays inferred as RAM, and the list is the finding because of what
is missing from it.** Of the island's fourteen per-fragment attribute arrays,
**exactly one** (``fsc_m``) inferred; of the reorder buffer's three stores,
**exactly one** (``rob_tag_m``). Same shape, same depth as their neighbours.
What separates them is the number of READ ADDRESSES.

So *"the table is in flops, move it to M10K"* has a true premise and the **wrong
prescription**. An M10K has two ports; an array read at three points cannot be
one however it is declared. The fix restructures the READ POINTS.

**Same mechanism as PERSPUV**, and now with evidence on both: ``e_tag``
(single read) inferred, ``e_num_u`` and ``e_q_u`` did not — the mechanism
that block's own diagnosis talked its reader out of.

The report also deleted itself: ``run_block_fit`` discarded the workspace, and
the map report is the ONE the register rule tells you to read. Second time in
this project. It is harvested now, whatever the run's outcome.

## COMBINE v2, drafted

Brief sections 4-6, beside V1 rather than replacing it. Two registered product
lanes (exactly two ``*`` sites), the fixed paired schedule, eight bounded
contexts, separate NEW/CONT/DONE/FREE queues, one writer and one reader per
store, a HELD output, and Q/R/O/M/F/W stages that forbid the brief's banned
one-cycle cones structurally. Every equation is the oracle's.

**Lints clean under -Wall and that is ALL that is established** — no
differential, no fit. Three faults found while linting, all mine: the completion
word dropped a bit; the refusal flag was built from the saturation flag and an
always-false term, so a refused fragment would have retired as a successful
black one; and four F-stage locals inferred latches.

## And GEOM.MEM.ADAPTER is fitting

## The pre-fit addendum arrived, and it is right about all of it

Owner direction: check everything in detail before the next fit and deliver a
rearchitect brief. Five agents verified the addendum's findings against the
source in parallel; the brief is
``reports/ZHAOZHOU-PREFIT-VERIFICATION-AND-REARCHITECT-20260906.txt``, 613
lines, with a copy at the zencrifice root for handover.

**Every claim checked holds. Three are WORSE than stated, two are new, and one
of mine was wrong.**

### The V2 draft's five blockers -- all real, all fixed

The sharpest was mine and recent: ``w_v <= m_v`` registered validity one edge
later than the ``m_ctx`` / ``m_ph`` / result it gated, so the write enable
belonged to the PREVIOUS transaction. Gone; F is combinational from the stable M
registers and the write plus its enqueue happen on ``m_v`` at one edge.

Also: the "synchronous" reads were asynchronous wires with the writes inside an
asynchronous-reset block -- the shape that stops arrays inferring, which this
island has now demonstrated twice. Four clock-only reset-free memory processes
and a real alignment stage. The product-job counter counted admitted FRAGMENTS
(one for a bypass costing zero, one for a DETAIL_LIGHT costing six). Phase zero
inherited the previous occupant's saturation flag. And a sixth the addendum did
not list: the context was released when DONE was popped rather than when the
output was accepted.

### W7 closed: the differential passes, and the phase count is exact

    [material_combine_v2_diff] 27 checks passed
    phases issued 560, the schedule owes 560, over 320 fragments

**The phase count is the new evidence.** V1 issued every microjob about twice
while every colour stayed exact, because recomputing a product is idempotent --
a result check cannot see a schedule. Mutating DETAIL_LIGHT from three phases to
two fails on four independent axes: product jobs 800 against 1,200, the phase
count 520 against 560, DETAIL_LIGHT no longer the most expensive recipe, and
twelve corrupted colours.

### The three that are worse than the addendum says

**AUX context is not a window.** ``aux_ready_i`` is tied high through the
island, so the mismatched tuple is the ONLY cycle every request gets: every AUX
sheet lookup uses the previous fragment's world X/Z under the current fragment's
identity. The differential records the slot and generation and never compares
the context -- the one observable that would fail.

**The cache's lost result is LIVE.** Its header says "Nothing instantiates this
yet"; it is in the island and the production top with real backpressure. The
owner's sequence reproduced from source: accepted 0-7, returned 0,1,2,3,6,7. It
does not deadlock -- it returns correct-looking data under a mismatched tag.

**The bilinear test's other three taps are never fetched.** With
``acc_en = 0001`` lanes 1-3 are never needed, filled or written, yet the
response copies all four. ``disp_bil_data[63:16]`` is uninitialised RAM that
only ``fu=fv=0`` discards, and the test's own comment claiming the four texels
are identical is false.

### And the severity multiplier

FRAGROB retires in ALLOCATION ORDER with no timeout, so any ONE lost sample
stalls the entire island rather than one fragment. That turns every hole in the
sample paths from a wrong pixel into a dead island, and is why typed sample
completions became a fit blocker.

### What I got wrong

My RAM diagnosis over-generalised. ``rob_m[seq_head_r]``'s three output slices
read the SAME word, so "read from several places" was wrong about several
arrays. What survives is the inventory fact, not the mechanism I attached to it
-- and PERSPUV had already taught me that exact lesson once, in this same run.

## In flight

COMBINE.V1 refit, relaunched alone after the first attempt was starved. Owner
priority 5 and the sanctioned next fit -- deliberately not a full island refit.

Build tree regenerating through `cmake --preset` after configure_file copy
errors, which is the documented stale-build.ninja trap: the graph could not
rebuild itself through another `cmake --build`.

## Next, in the owner's order

Priority 5: refit the narrow-increment combiner as a bounded attribution
experiment, NOT the whole island -- and the island's 7,720 ALM / 69.05 MHz is
stale regardless, since the capture table adds combinational LUT-RAM reads on
PERSPUV's input path. No prediction offered: the last obvious explanation for
this island was worth 4 MHz of 36.

## 2026-09-05 — terrain mip two-level blend architecture (sub-agent)
Wrote `reports/TERRAIN-MIP-TWO-LEVEL-BLEND-ARCHITECTURE-20260905.md` — the
owner-ruled "nearest within two mips, blended" design. Architecture only; no
RTL/test/tool changed; combine_v1 fit closure untouched and its file unread.
Key rulings proposed: Q4.4 LOD ABI end to end (fixes the nibble defect), the
pair issued as ONE planner request on two cache lanes, a credit-governed
palette blend station (reserve-before-issue), lerp8 magnitude-rounding law
governs. Three adjacent defects recorded in its §9: CLUT byte-select dropped
at the composed island, palette 565→888 zero-fill vs oracle replication,
dispatch silent unknown-class drop.

## ASSETFETCH: a held index request is now one episode

The composed-island fit was still live, so its exact closure remained untouched.
The next D19x prerequisite outside that closure was in the geometry reader:
``GEOM.ASSEMBLE`` holds ``ix_req`` for its whole S_FETCH state, while ASSETFETCH
turned that level into a synchronous RAM read and ``ix_valid`` pulse every cycle.
With one unqueued reader those duplicate answers merely overwrite each other;
with the owner's two-bank reader they become duplicate triangle transactions.

Closed in ``458418a1``. ASSETFETCH captures the triplet index on the first cycle
of a request episode, performs one read from that captured index, returns one
valid pulse, and does not rearm until the request deasserts. Release also clears
all pending index-reader state.

The new directed case keeps ``ix_req`` high for 12 cycles, changes the live index
immediately after acceptance, and then deasserts/rearms for a second triplet:

    fixed RTL       [assetfetch_rtl_directed] 66 checks passed
    pre-fix RTL     2 of 66 checks FAILED
                      repeated ix_valid; live-index poison changed the answer
    random RTL      217 admitted, 23 refused, 4 empty, 11,160 beats
                    [assetfetch_random] 9 checks passed

The pre-fix result came from a separately built copy under ``build/``; the test
therefore demonstrated that both new checks fire rather than merely passing the
repair. A full CMake preset refresh was not claimed: it hit the already-documented
``configure_file`` permission failures while regenerating unrelated Verilator
targets. Both named tests were instead verilated, statically linked and run
directly with the pinned Windows toolchain, one job at a time beside Quartus.

## GEOM.MEM-ADAPTER: overlong returns now retain ownership through physical LAST

The texture-island fit and its wrapper remained live, so the exact fit closure was
again left untouched. The next safe D19x ownership defect was in the already
landed shared ENGINE1 adapter. On the eighth useful payload word without
``m_beat_last``, it counted LONG but immediately returned to IDLE. The ninth
through twelfth words therefore arrived after the logical owner had been
released, were counted UNOWNED, and a queued descriptor could cross the adapter
boundary before the physical response ended.

Closed in ``c1d066b8``. ``zhao_geom_mem_adapter`` now enters an explicit drain
state at that mismatch. It emits no surplus beat to either requester, retains the
recorded owner, holds both requesters, counts one LONG response, and returns to
IDLE only on downstream ``LAST``.

The focused test queues a four-word A descriptor immediately after an overlong
eight-word B payload has crossed the adapter boundary:

    fixed RTL       [geom_mem_adapter_directed] 35 checks passed
                    jobs A 3, jobs B 5, denied 1, contention 1
                    short 0, long 1, unowned 0
    pre-fix RTL     2 of 35 checks FAILED
                    queued A accepted before physical LAST
                    four B surplus beats misclassified UNOWNED
                    MUTATION_EXIT=1

Both variants were verilated and statically linked directly, one job at a time,
under ``build/focused-geom-mem-overlong*``. The pre-fix source came from the
committed parent and used the same strengthened test, so the two new ownership
checks were demonstrated nonvacuous. The first direct fixed build exposed the
known MinGW/Verilator libstdc++ ABI mismatch at link; rebuilding all translation
units consistently with ``_GLIBCXX_USE_CXX11_ABI=0`` produced the passing gate.
No broad CMake regeneration was used or claimed.
