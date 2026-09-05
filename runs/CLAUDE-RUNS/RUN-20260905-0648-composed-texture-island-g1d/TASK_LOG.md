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
